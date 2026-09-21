//--------------------------------------------------------------------------------------
// MultiDeviceRender.cpp
//
// This sample demonstrates the usage of multiple command buffer devices to render many
// views of the same scene, such as in a split-screen multiplayer game.  Each view is 
// associated with a single command buffer device, as well as a pair of command buffer 
// objects (double buffered).  The benefit of using multiple devices is that the CPU 
// only has to make one pass over the scene.  As each model in the scene is encountered, 
// it is tested against all of the views, and possibly rendered to each of the views.
// After all command buffer recording is complete, each view's command buffer object is 
// played back on the main device.
//
// The command buffer device setup is in Initialize(), the command buffer recording code 
// is primarily within RenderToViews(), and the playback code is within 
// SubmitViewCommandsToMainDevice().
//
// Xbox Game Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include <xtl.h>
#include <xgraphics.h>
#include <xboxmath.h>
#include <AtgFont.h>
#include <AtgInput.h>
#include <AtgUtil.h>
#include <AtgHelp.h>
#include <AtgApp.h>
#include <AtgSimpleShaders.h>
#include <AtgDebugDraw.h>
#include <AtgSceneAll.h>

#include "ParameterPool.h"

#pragma warning(disable:6385)   // (Code Analysis) Can't figure out yet that 'x & 1'
#pragma warning(disable:6386)   // must be either 0 or 1.

//--------------------------------------------------------------------------------------
// Callouts for labelling the gamepad on the help screen
//--------------------------------------------------------------------------------------
ATG::HELP_CALLOUT g_HelpCallouts[] =
{
    { ATG::HELP_A_BUTTON,     ATG::HELP_PLACEMENT_1, L"Cycle active camera" },
    { ATG::HELP_Y_BUTTON,     ATG::HELP_PLACEMENT_2, L"Level camera" },
    { ATG::HELP_LEFT_STICK,   ATG::HELP_PLACEMENT_2, L"Move camera" },
    { ATG::HELP_RIGHT_STICK,  ATG::HELP_PLACEMENT_2, L"Orient camera" },
    { ATG::HELP_LEFT_SHOULDER,ATG::HELP_PLACEMENT_2, L"Slow down camera" },
    { ATG::HELP_RIGHT_SHOULDER,ATG::HELP_PLACEMENT_2, L"Speed up camera" },
    { ATG::HELP_BACK_BUTTON,  ATG::HELP_PLACEMENT_2, L"Display\nhelp" },
};
#define NUM_HELP_CALLOUTS (sizeof(g_HelpCallouts) / sizeof(g_HelpCallouts[0]))

static const DWORD  g_dwViewCount = 4;

// ViewQuadrantRects holds the screen coordinates of the four views.
D3DRECT g_ViewQuadrantRects[] =
{
    { 0, 0, 640, 360 },
    { 640, 0, 1280, 360 },
    { 0, 360, 640, 720 },
    { 640, 360, 1280, 720 }
};

//--------------------------------------------------------------------------------------
// Globals for our own memory allocator used by CreateGrowableCommandBuffer.
//--------------------------------------------------------------------------------------
static const DWORD  g_dwCommandBufferSize = 1024 * 1024;
char*               g_pAllocateChunk[2];
char*               g_pAllocateCurrent[2];
DWORD               g_dwAllocateIndex;

//--------------------------------------------------------------------------------------
// Name: ViewSettings
// Desc: Contains all of the data relevant to a single view on the scene.
//--------------------------------------------------------------------------------------
struct ViewSettings
{
    ViewSettings() : matCameraWVP( XMMatrixIdentity() ),
                     pCamera( NULL ),
                     pRenderTarget( NULL ),
                     pDepthStencil( NULL ),
                     pResolveTexture( NULL ),
                     pd3dDevice( NULL ),
                     ClearColor( 0 )
    {
    }

    // camera members
    ATG::Camera* pCamera;
    XMMATRIX matCameraWVP;
    ATG::Bound CameraFrustumBounds;

    // rendering members
    D3DSurface* pRenderTarget;
    D3DSurface* pDepthStencil;
    D3DTexture* pResolveTexture;
    D3DCOLOR ClearColor;

    // command buffer members
    D3DDevice* pd3dDevice;
    D3DCommandBuffer* pCommandBuffer[2];
};


//--------------------------------------------------------------------------------------
// Name: class Sample
// Desc: The multiple device render sample class.
//--------------------------------------------------------------------------------------
class Sample : public ATG::Application
{
public:
    HRESULT         Initialize();
    HRESULT         Update();
    HRESULT         Render();

    VOID            RenderView( const ViewSettings* pViewSettings );
    DWORD           GetFrame()
    {
        return m_dwFrame;
    }

private:
    VOID            MoveFrame( ATG::GAMEPAD* pGamepad, FLOAT fDeltaTime, ATG::Frame* pFrame );
    ATG::Camera* FindCamera( const WCHAR* strName );

    VOID            RenderUI();

    VOID            PrepareViewsForRendering();
    VOID            RenderToViews();
    VOID            SetupLights( ATG::Model* pModel );
    VOID            DrawModel( ATG::Model* pModel, ViewSettings* pView );
    VOID            SubmitViewCommandsToMainDevice();

private:
    // sample framework objects
    ATG::Font m_Font;
    ATG::Timer m_Timer;
    ATG::Help m_Help;
    BOOL m_bDrawHelp;

    // views
    ViewSettings    m_Views[ g_dwViewCount ];
    DWORD m_dwControlViewIndex;
    DWORD m_dwCommandBufferIndex;

    // back buffers
    D3DSurface* m_pColorTarget1X;
    D3DSurface* m_pDepthStencil1X;
    D3DSurface* m_pColorTarget4X;
    D3DSurface* m_pDepthStencil4X;

    // front buffer
    D3DTexture* m_pFrontBuffer;

    // scene members
    ATG::Scene* m_pScene;
    std::vector <ATG::PointLight*> m_ScenePointLights;
    UbershaderParameterPool m_ParameterPool;
    DWORD m_dwFrame;
};


//--------------------------------------------------------------------------------------
// Name: main()
// Desc: Entry point to the sample.
//--------------------------------------------------------------------------------------
INT __cdecl main()
{
    Sample MDRSample;

    // Set up the structure used to create the D3DDevice.
    D3DPRESENT_PARAMETERS& d3dpp = MDRSample.m_d3dpp;
    ZeroMemory( &d3dpp, sizeof( D3DPRESENT_PARAMETERS ) );
    d3dpp.BackBufferWidth = 1280;
    d3dpp.BackBufferHeight = 720;
    d3dpp.BackBufferFormat = D3DFMT_A8R8G8B8;
    d3dpp.FrontBufferFormat = D3DFMT_LE_X8R8G8B8;
    d3dpp.MultiSampleType = D3DMULTISAMPLE_NONE;
    d3dpp.MultiSampleQuality = 0;
    d3dpp.BackBufferCount = 0;
    d3dpp.EnableAutoDepthStencil = FALSE;
    d3dpp.DisableAutoBackBuffer = TRUE;
    d3dpp.DisableAutoFrontBuffer = TRUE;
    d3dpp.AutoDepthStencilFormat = D3DFMT_D24S8;
    d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;

    MDRSample.Run();
}


//--------------------------------------------------------------------------------------
// Name: FindCamera()
// Desc: Searches the m_pScene scene hierarchy for a camera matching strName.
//--------------------------------------------------------------------------------------
ATG::Camera* Sample::FindCamera( const WCHAR* strName )
{
    ATG::NameIndexedCollection::iterator i;
    for( i = m_pScene->GetInstanceList()->begin(); i != m_pScene->GetInstanceList()->end(); i++ )
    {
        if( ( *i )->IsDerivedFrom( ATG::Camera::TypeID ) )
        {
            ATG::Camera* pCamera = ( ATG::Camera* )( *i );
            if( _wcsicmp( pCamera->GetName().GetSafeString(), strName ) == 0 )
                return pCamera;
        }
    }
    return NULL;
}


//--------------------------------------------------------------------------------------
// Name: AllocateCallback()
// Desc: This routine is called by D3D to allocate more physical memory for a growable 
//       command buffer.
//--------------------------------------------------------------------------------------
void* AllocateCallback( DWORD dwContext, DWORD dwFlags, DWORD* pdwSize, DWORD dwAlignment )
{
    // NOTE: It's very easy to screw up the implementation of growable
    //       buffer allocation routines.  If you accidentally give bad
    //       results, you can see all kinds of random, impossible-to-debug
    //       GPU hangs.  Be very careful!  As a result I'd recommend keeping
    //       #ifdef'd code that can use non-growable command buffers if
    //       you start seeing random GPU hangs - this can rule out problems
    //       with your growable buffer allocator.
    // 
    // The allocation convention we choose to employ here is that each growable 
    // command buffer used in a frame is allocated contiguously from one
    // chunk of write-combined memory.  We sequentially parcel out memory from 
    // that underlying chunk as the allocation requests come in from D3D.  The 
    // benefit  from growable buffers using one underlying chunk is that it's 
    // load balancing - if one command buffer ends up using less memory, 
    // another command buffer can use more.  By default D3D will ask us to
    // allocate in requests of 32K (this value can be changed via a parameter
    // to CreateGrowableCommandBuffer).  
    //
    // We also need to employ a double-buffering scheme so that the GPU can be
    // rendering one frame's command buffers while the CPU is building the
    // next frame's command buffers.  To accomplish this we employ two separate
    // buffers and 'dwContext' is 0 or 1 on alternating frames.  We could
    // have just as easily set it up so that 'dwContext' was a pointer to some
    // private heap information.
    //
    // By default D3D guarantees that it won't let the GPU get more than 1 
    // frame behind the CPU, so we only need to double-buffer the command
    // buffer chunks (and not triple or quad buffer).
    Sample* pSample = ( Sample* )dwContext;
    DWORD dwIndex = pSample->GetFrame() & 1;

    if( dwIndex != g_dwAllocateIndex )
    {
        g_pAllocateCurrent[dwIndex] = NULL;
        g_dwAllocateIndex = dwIndex;
    }

    if( g_pAllocateChunk[dwIndex] == NULL )
    {
        g_pAllocateChunk[dwIndex] = ( char* )XPhysicalAlloc( g_dwCommandBufferSize,
                                                             MAXULONG_PTR,
                                                             0,
                                                             PAGE_READWRITE | PAGE_WRITECOMBINE );
        if( g_pAllocateChunk[dwIndex] == NULL )
        {
            ATG::FatalError( "Could not create command buffer memory." );
        }
    }

    // If this is the first allocation for a frame, reset our current pointer.
    if( g_pAllocateCurrent[dwIndex] == NULL )
        g_pAllocateCurrent[dwIndex] = g_pAllocateChunk[dwIndex];

    // We have to honor the requested alignment.
    char* pAllocation = ( char* )XGNextMultiple( ( DWORD )g_pAllocateCurrent[dwIndex], dwAlignment );

    // Figure out where the next allocation will go, taking into account
    // the size of the current request.
    g_pAllocateCurrent[dwIndex] = pAllocation + *pdwSize;

    // If we've run out of memory, return NULL.  D3D won't crash but will return
    // failure at EndCommandBuffer time.
    if( g_pAllocateCurrent[dwIndex] > g_pAllocateChunk[dwIndex] + g_dwCommandBufferSize )
    {
        assert( 0 );
        return NULL;
    }

    return pAllocation;
}


//--------------------------------------------------------------------------------------
// Name: FreeCallback()
// Desc: This routine is called by D3D to free physical memory for a growable command
//       buffer.
//--------------------------------------------------------------------------------------
void FreeCallback( DWORD dwContext )
{
    // Our AllocateCallback implementation above ping-pongs between memory 
    // buffers on  every frame and thereby takes care of memory re-use.
    // Consequently we don't have to do anything in this free routine.
    //
    // However, if we employed a more sophisticated allocator we could free
    // all memory associated with the command buffer here.  I'd recommend
    // the nice and simple approach above, however.
}


//--------------------------------------------------------------------------------------
// Name: QueryCallback()
// Desc: This routine is called by D3D to determine how much memory is available for
//       a growable command buffer.  It's only called when game code calls the
//       QueryBufferSpace() D3D API.
//--------------------------------------------------------------------------------------
void QueryCallback( DWORD dwContext, DWORD* pdwUsed, DWORD* pdwRemaining )
{
    *pdwUsed = g_pAllocateCurrent[dwContext] - g_pAllocateChunk[dwContext];
    *pdwRemaining = g_pAllocateChunk[dwContext] + g_dwCommandBufferSize - g_pAllocateCurrent[dwContext];
}

//--------------------------------------------------------------------------------------
// Name: Initialize()
// Desc: Initializes the objects for the sample, and loads various resources.
//--------------------------------------------------------------------------------------
HRESULT Sample::Initialize()
{
    m_bDrawHelp = FALSE;
    m_dwControlViewIndex = 0;

    // Create font.
    if( FAILED( m_Font.Create( "game:\\Media\\Fonts\\Arial_16.xpr" ) ) )
        ATG::FatalError( "Could not load font." );

    // Confine text drawing to the title safe area
    m_Font.SetWindow( ATG::GetTitleSafeArea() );

    // Create help screen.
    if( FAILED( m_Help.Create( "game:\\Media\\Help\\Help.xpr" ) ) )
        ATG::FatalError( "Could not load help resources." );

    // Initialize simple shaders.
    ATG::SimpleShaders::Initialize( NULL, NULL );

    // Set up surface parameters for creating the rendertargets.
    D3DSURFACE_PARAMETERS ColorSurfParams;
    ColorSurfParams.Base = 0;
    ColorSurfParams.ColorExpBias = 0;
    ColorSurfParams.HierarchicalZBase = 0;
    ColorSurfParams.HiZFunc = D3DHIZFUNC_DEFAULT;

    D3DSURFACE_PARAMETERS DepthSurfParams;
    DepthSurfParams.Base = 1024;
    DepthSurfParams.ColorExpBias = 0;
    DepthSurfParams.HierarchicalZBase = 0;
    DepthSurfParams.HiZFunc = D3DHIZFUNC_DEFAULT;

    // Create 1280x720 1xMSAA surfaces.
    HRESULT hr = m_pd3dDevice->CreateRenderTarget( 1280, 720,
                                                   D3DFMT_A8R8G8B8,
                                                   D3DMULTISAMPLE_NONE,
                                                   0, FALSE,
                                                   &m_pColorTarget1X,
                                                   &ColorSurfParams );
    if( FAILED( hr ) )
        ATG::FatalError( "Couldn't create color rendertarget." );

    hr = m_pd3dDevice->CreateRenderTarget( 1280, 720,
                                           D3DFMT_D24S8,
                                           D3DMULTISAMPLE_NONE,
                                           0, FALSE,
                                           &m_pDepthStencil1X,
                                           &DepthSurfParams );
    if( FAILED( hr ) )
        ATG::FatalError( "Couldn't create depth/stencil rendertarget." );

    // Create 640x360 4xMSAA surfaces.
    hr = m_pd3dDevice->CreateRenderTarget( 640, 360,
                                           D3DFMT_A8R8G8B8,
                                           D3DMULTISAMPLE_4_SAMPLES,
                                           0, FALSE,
                                           &m_pColorTarget4X,
                                           &ColorSurfParams );
    if( FAILED( hr ) )
        ATG::FatalError( "Couldn't create color rendertarget." );

    hr = m_pd3dDevice->CreateRenderTarget( 640, 360,
                                           D3DFMT_D24S8,
                                           D3DMULTISAMPLE_4_SAMPLES,
                                           0, FALSE,
                                           &m_pDepthStencil4X,
                                           &DepthSurfParams );
    if( FAILED( hr ) )
        ATG::FatalError( "Couldn't create depth/stencil rendertarget." );

    // Create 1280x720 front buffer texture.
    hr = m_pd3dDevice->CreateTexture( 1280, 720, 1, 0, D3DFMT_LE_X8R8G8B8, D3DPOOL_DEFAULT, &m_pFrontBuffer, NULL );

    // Initialize view settings.
    for( DWORD i = 0; i < g_dwViewCount; ++i )
    {
        ViewSettings& vs = m_Views[i];
        vs.pRenderTarget = m_pColorTarget4X;
        vs.pDepthStencil = m_pDepthStencil4X;

        hr = m_pd3dDevice->CreateTexture( 640, 360, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &vs.pResolveTexture,
                                          NULL );
        if( FAILED( hr ) )
            ATG::FatalError( "Could not create view resolve texture." );

#if USE_NONGROWABLE_BUFFERS
        HRESULT hr0 = m_pd3dDevice->CreateCommandBuffer( dwCommandBufferSize, 0, &vs.pCommandBuffer[0] );
        HRESULT hr1 = m_pd3dDevice->CreateCommandBuffer( dwCommandBufferSize, 0, &vs.pCommandBuffer[1] );
    #else
        HRESULT hr0 = m_pd3dDevice->CreateGrowableCommandBuffer( 0, AllocateCallback, FreeCallback,
                                                                     QueryCallback, ( DWORD )this,
                                                                     0, &vs.pCommandBuffer[0] );
        HRESULT hr1 = m_pd3dDevice->CreateGrowableCommandBuffer( 0, AllocateCallback, FreeCallback,
                                                                 QueryCallback, ( DWORD )this,
                                                                 0, &vs.pCommandBuffer[1] );
#endif

        if( FAILED( hr0 ) || FAILED( hr1 ) )
            ATG::FatalError( "Could not create command buffer object." );

        hr = Direct3D_CreateDevice( 0, D3DDEVTYPE_COMMAND_BUFFER, NULL, 0, NULL, &vs.pd3dDevice );
        if( FAILED( hr ) )
            ATG::FatalError( "Could not create command buffer device." );
    }

    // Create scene object.
    m_pScene = new ATG::Scene();

    // Load the ubershader into the scene resource database.
    ATG::FXLiteMaterialImplementation::SetParameterPool( m_pScene->GetEffectParameterPool() );
    ATG::BaseMaterial* pUbershaderBaseMaterial = ATG::BaseMaterial::CreateFXLiteMaterial( L"Default",
                                                                                          L"game:\\media\\effects\\ubershader_final.fxobj", L"Ubershader_Nested" );
    pUbershaderBaseMaterial->InitializeImplementation();
    pUbershaderBaseMaterial->ChangeDevice( m_pd3dDevice );
    ATG::ResourceDatabase* pRDB = m_pScene->GetResourceDatabase();
    pRDB->AddResource( pUbershaderBaseMaterial );

    // Bind the parameter pool interface to the FXLite parameter pool.
    // This allows us to easily set the shader constants for the ubershader.
    m_ParameterPool.Initialize( m_pScene->GetEffectParameterPool() );

    // Create black texture to use when samplers are NULL.
    pRDB->CreateBlackTexture();

    // Load the scene.
    hr = ATG::SceneFileParser::LoadXATGFile( "game:\\media\\scenes\\MultiDeviceRender.xatg", m_pScene, NULL, 0, NULL );
    if( FAILED( hr ) )
        ATG::FatalError( "Could not load scene file." );

    const WCHAR* strCameraNames[] = { L"camera1", L"camera2", L"camera3", L"camera4" };
    const D3DCOLOR ClearColors[] = { 0xFF400000, 0xFF004000, 0xFF000040, 0xFF404000 };

    // Assign cameras to views, and set up their projections and clear colors.
    for( DWORD i = 0; i < g_dwViewCount; ++i )
    {
        m_Views[i].pCamera = FindCamera( strCameraNames[ i % ARRAYSIZE( strCameraNames ) ] );
        if( m_Views[i].pCamera == NULL )
        {
            ATG::FatalError( "Could not find named camera in scene." );
        }
        else
        {
            ATG::Projection proj;
            proj.SetFovYAspect( XM_PIDIV4, ( 16.0f / 9.0f ), 0.01f, 1000.0f );
            m_Views[i].pCamera->SetProjection( proj );
        }

        m_Views[i].ClearColor = ClearColors[ i % ARRAYSIZE( ClearColors ) ];
    }

    // Find all of the point lights in the scene and store pointers to them in a vector.
    ATG::NameIndexedCollection::iterator i;
    for( i = m_pScene->GetInstanceList()->begin(); i != m_pScene->GetInstanceList()->end(); i++ )
    {
        if( ( *i )->IsDerivedFrom( ATG::PointLight::TypeID ) )
        {
            ATG::PointLight* pLight = ( ATG::PointLight* )( *i );
            m_ScenePointLights.push_back( pLight );
        }
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Update()
// Desc: Gets controller input and updates the camera.
//--------------------------------------------------------------------------------------
HRESULT Sample::Update()
{
    // Update FPS in the timer class.
    m_Timer.MarkFrame();

    // Get frame delta time.
    FLOAT fDeltaTime = ( FLOAT )m_Timer.GetElapsedTime();

    // Get input from controllers (check for exit button sequence).
    ATG::GAMEPAD* pGamepad = ATG::Input::GetMergedInput();

    // The Back button toggles the help screen.
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_BACK )
        m_bDrawHelp = !m_bDrawHelp;

    // The A button cycles which view you are controlling.
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_A )
        m_dwControlViewIndex = ( m_dwControlViewIndex + 1 ) % g_dwViewCount;

    // Update camera.
    ATG::Camera* pActiveCamera = m_Views[ m_dwControlViewIndex ].pCamera;
    MoveFrame( pGamepad, fDeltaTime, pActiveCamera );

    // Update current command buffer index.  This is used to double-buffer the command
    // buffer objects.
    m_dwCommandBufferIndex = m_Timer.m_dwNumFrames % 2;

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: MoveFrame()
// Desc: Converts gamepad input into frame movement.  The input model is a free-cam
//       model, where the left stick controls movement, and the right stick controls
//       orientation.  The shoulder buttons modify the movement speed.
//--------------------------------------------------------------------------------------
VOID Sample::MoveFrame( ATG::GAMEPAD* pGamepad, FLOAT fDeltaTime, ATG::Frame* pFrame )
{
    assert( pFrame != NULL && pGamepad != NULL );

    static const XMVECTOR vX = XMVectorSet( 1, 0, 0, 0 );
    static const XMVECTOR vY = XMVectorSet( 0, 1, 0, 0 );
    static const XMVECTOR vZ = XMVectorSet( 0, 0, 1, 0 );

    const XMVECTOR vWorldUpVector = vY;

    // Obtain the current vectors for the frame.
    const XMVECTOR vForward = pFrame->GetWorldDirection();
    const XMVECTOR vUp = pFrame->GetWorldUp();
    const XMVECTOR vRight = pFrame->GetWorldRight();

    // Compute movement speed.
    FLOAT fSpeed = 1.0f;
    if( pGamepad->wLastButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER )
        fSpeed *= 10.0f;
    else if( pGamepad->wLastButtons & XINPUT_GAMEPAD_LEFT_SHOULDER )
        fSpeed *= 0.1f;

    // Get the thumbstick settings.  Square them so small adjustments are easier.
    FLOAT fX1 = pGamepad->fX1 * fabs( pGamepad->fX1 );
    FLOAT fY1 = pGamepad->fY1 * fabs( pGamepad->fY1 );
    FLOAT fX2 = pGamepad->fX2 * fabs( pGamepad->fX2 );
    FLOAT fY2 = pGamepad->fY2 * fabs( pGamepad->fY2 );

    // Invert Y axis for orientation.
    fY2 = -fY2;

    BOOL bUpdateWorld = FALSE;

    // Adjust frame position based on the left thumbstick.
    XMVECTOR vPos = pFrame->GetWorldPosition();
    if( fY1 != 0 || fX1 != 0 )
    {
        bUpdateWorld = TRUE;
        FLOAT fAmount = fDeltaTime * fSpeed;
        if( pGamepad->wLastButtons & XINPUT_GAMEPAD_LEFT_THUMB )
        {
            vPos += XMVectorScale( vUp, fAmount * fY1 );
        }
        else
        {
            vPos += XMVectorScale( vForward, fAmount * fY1 );
            vPos += XMVectorScale( vRight, fAmount * fX1 );
        }
    }

    // Adjust frame orientation based on right thumbstick.
    XMVECTOR RotationQ = XMQuaternionIdentity();
    FLOAT fRotationSpeed = XM_PIDIV2 * fDeltaTime;
    if( fX2 != 0 || fY2 != 0 )
    {
        bUpdateWorld = TRUE;
        if( pGamepad->wLastButtons & XINPUT_GAMEPAD_RIGHT_THUMB )
        {
            XMVECTOR RotationZ = XMQuaternionRotationAxis( vZ, fRotationSpeed * fX2 );
            RotationQ = XMQuaternionMultiply( RotationQ, RotationZ );
        }
        else
        {
            XMVECTOR RotationX = XMQuaternionRotationAxis( vX, fRotationSpeed * fY2 );
            RotationQ = XMQuaternionMultiply( RotationQ, RotationX );
            XMVECTOR RotationY = XMQuaternionRotationAxis( vY, fRotationSpeed * fX2 );
            RotationQ = XMQuaternionMultiply( RotationQ, RotationY );
        }
    }

    // Iteratively level the frame if the Y button is being held down.
    if( pGamepad->wLastButtons & XINPUT_GAMEPAD_Y )
    {
        bUpdateWorld = TRUE;
        XMVECTOR vDot = XMVector3Dot( vRight, vWorldUpVector );
        FLOAT fAmount = -vDot.x;
        fAmount *= ( fDeltaTime * 4.0f );
        XMVECTOR RotationRestoreUp = XMQuaternionRotationAxis( vZ, fAmount );
        RotationQ = XMQuaternionMultiply( RotationQ, RotationRestoreUp );
    }

    // Compose a new world matrix based on the rotation quaternion and position.
    if( bUpdateWorld )
    {
        XMMATRIX matWorld = pFrame->GetWorldTransform();
        matWorld = XMMatrixMultiply( XMMatrixRotationQuaternion( RotationQ ), matWorld );
        matWorld.r[3] = XMVectorSelect( vPos, XMQuaternionIdentity(), XMVectorSelectControl( 0, 0, 0, 1 ) );
        pFrame->SetWorldTransform( matWorld );
    }
}


//--------------------------------------------------------------------------------------
// Name: PrepareViewsForRendering()
// Desc: Updates the camera data members for each view.
//--------------------------------------------------------------------------------------
VOID Sample::PrepareViewsForRendering()
{
    for( DWORD i = 0; i < g_dwViewCount; ++i )
    {
        ATG::Camera* pCamera = m_Views[i].pCamera;
        assert( pCamera != NULL );
        if( pCamera != NULL )
        {
            m_Views[i].CameraFrustumBounds = pCamera->GetWorldBound();
            m_Views[i].matCameraWVP = pCamera->GetWorldView() * pCamera->GetProjection().GetMatrix();
        }
    }
}


//--------------------------------------------------------------------------------------
// Name: SetupLights()
// Desc: Collides all point light volumes with the model volume, and places their 
//       settings into shader constants.
//--------------------------------------------------------------------------------------
VOID Sample::SetupLights( ATG::Model* pModel )
{
    // Compute inverse world transform for the model.
    XMVECTOR vDummy;
    XMMATRIX matInvWorld = XMMatrixInverse( &vDummy, pModel->GetWorldTransform() );

    const ATG::Bound& ModelBound = pModel->GetWorldBound();

    DWORD dwLightIndex = 0;

    // Loop over the point light vector.
    DWORD dwLightCount = ( DWORD )m_ScenePointLights.size();
    for( DWORD i = 0; i < dwLightCount; ++i )
    {
        // Collide point light with model.
        ATG::PointLight* pLight = m_ScenePointLights[i];
        if( !ModelBound.Collide( pLight->GetWorldBound() ) )
            continue;

        // Transform light position into object space.
        XMVECTOR vObjLightPos;
        vObjLightPos = XMVector3TransformCoord( pLight->GetWorldPosition(), matInvWorld );
        vObjLightPos.w = 1.0f / ( pLight->GetWorldRange() );

        // Set point light position and color into shader constants.
        m_ParameterPool.SetPointLightPosition( dwLightIndex, vObjLightPos );
        m_ParameterPool.SetPointLightColor( dwLightIndex, pLight->GetColor() );

        ++dwLightIndex;
    }

    // Set the total number of point lights into the shader.
    m_ParameterPool.SetPointLightCount( dwLightIndex );

    // No other light types are supported in this sample.
    m_ParameterPool.SetSpotLightCount( 0 );
    m_ParameterPool.SetShadowedSpotLightCount( 0 );
    m_ParameterPool.SetDirLightCount( 0 );

    // Set ambient light.
    m_ParameterPool.SetAmbient( XMVectorReplicate( 0.1f ) );
}


//--------------------------------------------------------------------------------------
// Name: DrawModel()
// Desc: Draws a model to the given view.  The models are bound to materials specified
//       in the content file.
//--------------------------------------------------------------------------------------
VOID Sample::DrawModel( ATG::Model* pModel, ViewSettings* pView )
{
    // Compute world * view * projection matrix for this model and set into constants.
    XMMATRIX matWVP = pModel->GetWorldTransform() * pView->matCameraWVP;
    m_ParameterPool.SetWorldViewProjMatrix( matWVP );

    // Compute object space position and direction for camera and set into constants.
    XMVECTOR vDummy;
    XMMATRIX matInvWorld = XMMatrixInverse( &vDummy, pModel->GetWorldTransform() );
    XMVECTOR vObjViewDir, vObjViewPos;
    vObjViewDir = XMVector3Normalize( XMVector3Transform( pView->pCamera->GetWorldDirection(), matInvWorld ) );
    m_ParameterPool.SetObjectViewDirection( vObjViewDir );
    vObjViewPos = XMVector3TransformCoord( pView->pCamera->GetWorldPosition(), matInvWorld );
    m_ParameterPool.SetObjectViewPosition( vObjViewPos );

    // Loop over mesh mappings.
    DWORD dwMeshMappingCount = pModel->GetNumMeshMappings();
    for( DWORD dwMapIndex = 0; dwMapIndex < dwMeshMappingCount; ++dwMapIndex )
    {
        ATG::MeshMapping& mm = pModel->GetMeshMapping( dwMapIndex );
        ATG::BaseMesh* pMesh = mm.pMesh;

        // Loop over mesh subsets.
        DWORD dwSubsetCount = pMesh->GetNumSubsets();
        for( DWORD dwSubsetIndex = 0; dwSubsetIndex < dwSubsetCount; ++dwSubsetIndex )
        {
            ATG::MaterialInstance* pMaterial = mm.Materials[ dwSubsetIndex ];

            // Set the FXLite material using the view's command buffer device.
            pMaterial->BeginMaterialSinglePass( pView->pd3dDevice );
            // Render the mesh subset.
            pMesh->RenderSubset( dwSubsetIndex, pView->pd3dDevice );
            // End the FXLite material.
            pMaterial->EndMaterialSinglePass();
        }
    }
}


//--------------------------------------------------------------------------------------
// Name: RenderToViews()
// Desc: Walks over the scene once, and renders models to the different views.
//--------------------------------------------------------------------------------------
VOID Sample::RenderToViews()
{
    // Start rendering on each view.
    for( DWORD i = 0; i < g_dwViewCount; ++i )
    {
        D3DDevice* pDevice = m_Views[i].pd3dDevice;
        // Begin command buffer recording.
        pDevice->BeginCommandBuffer( m_Views[i].pCommandBuffer[ m_dwCommandBufferIndex ], 0, NULL, NULL, NULL, 0 );

        // Set rendertargets and clear.
        pDevice->SetRenderTarget( 0, m_Views[i].pRenderTarget );
        pDevice->SetDepthStencilSurface( m_Views[i].pDepthStencil );
        pDevice->Clear( 0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL, m_Views[i].ClearColor, 1.0f,
                        0 );
    }


    // Loop over all objects in the scene.
    ATG::NameIndexedCollection::iterator iter;
    for( iter = m_pScene->GetInstanceList()->begin(); iter != m_pScene->GetInstanceList()->end(); iter++ )
    {
        // Select models from the object list.
        if( ( *iter )->IsDerivedFrom( ATG::Model::TypeID ) )
        {
            ATG::Model* pModel = ( ATG::Model* )( *iter );
            // Obtain the bounding volume for the model.
            const ATG::Bound& ModelWorldBound = pModel->GetWorldBound();

            // Set up lights into shader constants.
            SetupLights( pModel );

            // Loop over the views.
            for( DWORD dwViewIndex = 0; dwViewIndex < g_dwViewCount; ++dwViewIndex )
            {
                // Test the model bounds against the view's camera frustum.
                const ATG::Bound& CameraFrustumBound = m_Views[dwViewIndex].CameraFrustumBounds;
                if( !CameraFrustumBound.Collide( ModelWorldBound ) )
                    continue;

                // Draw the model to this view.
                DrawModel( pModel, &m_Views[dwViewIndex] );
            }
        }
    }


    // Loop over each view and finish up rendering.
    for( DWORD i = 0; i < g_dwViewCount; ++i )
    {
        D3DDevice* pDevice = m_Views[i].pd3dDevice;

        // Resolve rendertarget to resolve texture.
        pDevice->Resolve( D3DRESOLVE_RENDERTARGET0, NULL, m_Views[i].pResolveTexture, NULL, 0, 0, NULL, 0.0f, 0,
                          NULL );

        // End command buffer recording.
        pDevice->EndCommandBuffer();
    }
}


//--------------------------------------------------------------------------------------
// Name: SubmitViewCommandsToMainDevice()
// Desc: Plays back the views' command buffer objects to the main D3D device using
//       RunCommandBuffer.
//--------------------------------------------------------------------------------------
VOID Sample::SubmitViewCommandsToMainDevice()
{
    // Play back each view's command buffer to the main device.
    for( DWORD i = 0; i < g_dwViewCount; ++i )
    {
        m_pd3dDevice->RunCommandBuffer( m_Views[i].pCommandBuffer[ m_dwCommandBufferIndex ], 0 );
    }
}


//--------------------------------------------------------------------------------------
// Name: RenderUI()
// Desc: Draws some statistics and other UI elements
//--------------------------------------------------------------------------------------
VOID Sample::RenderUI()
{
    if( m_bDrawHelp )
    {
        m_Help.Render( &m_Font, g_HelpCallouts, NUM_HELP_CALLOUTS );
    }
    else
    {
        // Draw a title and FPS indicator.
        m_Font.Begin();
        m_Font.SetScaleFactors( 1.2f, 1.2f );
        m_Font.DrawText( 0, 0, 0xffffffff, L"MultiDeviceRender" );
        m_Font.SetScaleFactors( 1.0f, 1.0f );
        m_Font.DrawText( 0, 0, 0xffffff00, m_Timer.GetFrameRate(), ATGFONT_RIGHT );

        m_Font.End();
    }
}


//--------------------------------------------------------------------------------------
// Name: Render()
// Desc: Renders the scene and UI.
//--------------------------------------------------------------------------------------
HRESULT Sample::Render()
{
    m_pd3dDevice->BeginScene();

    // Render the four views.
    PrepareViewsForRendering();
    RenderToViews();

    // Play back the views' command buffer objects.
    SubmitViewCommandsToMainDevice();

    // Switch to a full-screen rendertarget.
    m_pd3dDevice->SetRenderTarget( 0, m_pColorTarget1X );
    m_pd3dDevice->SetDepthStencilSurface( NULL );

    // Draw view textures to back buffer.
    for( DWORD i = 0; i < g_dwViewCount; ++i )
    {
        ATG::DebugDraw::DrawScreenSpaceTexturedRect( g_ViewQuadrantRects[i], m_Views[i].pResolveTexture );
    }

    // Highlight the active view (the one which you can move the camera).
    ATG::DebugDraw::DrawScreenSpaceRect( g_ViewQuadrantRects[ m_dwControlViewIndex ], 3.0f, 0xFFFFFFFF );

    // Render UI.
    RenderUI();

    m_pd3dDevice->EndScene();

    // Sync to present interval, resolve, and swap.
    m_pd3dDevice->SynchronizeToPresentationInterval();
    m_pd3dDevice->Resolve( D3DRESOLVE_RENDERTARGET0, NULL, m_pFrontBuffer, NULL, 0, 0, NULL, 1.0, 0, NULL );
    m_pd3dDevice->Swap( m_pFrontBuffer, NULL );

    // Increment our frame count which is used by the command buffer allocator.
    m_dwFrame++;

    return S_OK;
}

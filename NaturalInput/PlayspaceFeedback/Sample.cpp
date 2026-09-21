//--------------------------------------------------------------------------------------
// AvatarRetargeting.cpp
//
// This sample demonstrates the use of the NUI API to animate
// Avatars.  The skeleton returned from the camera is mapped to an avatar skeleton.
// Joint positions returned from the camera are mapped to joint rotations for use in
// skinning the avatar.
//
// XNA Developer Connection
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include <xtl.h>
#include <xavatar.h>
#include <xgraphics.h>
#include <AtgApp.h>
#include <AtgFont.h>
#include <AtgHelp.h>
#include <AtgInput.h>
#include <AtgSimpleShaders.h>
#include <AtgUtil.h>
#include <AtgNuiVisualization.h>
#include <AtgDebugDraw.h>
#include <AtgNuiCommon.h>

#include "AvatarRenderer.h"
#include "SimpleAnim.h"
#include "PlayspaceBounds.h"
#include "Sample.h"


//----------------------------------------------------------------------------------------------------------------------
// Constants
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
// Camera constants
//----------------------------------------------------------------------------------------------------------------------

// Location in worldspace that the camera looks towards.
static const XMVECTORF32 s_vCameraLookAt = { 0.0f, 0.91f, 0.0f, 0.0f};

// Initial camera position.
static const XMVECTORF32 s_vDefaultCameraPos = {0.0f, 0.91f, -5.0f, 0.0f};

// Default avatar color multiplier is solid white.
static const XMVECTORF32 s_vAvatarDefaultColor = { 1.0f, 1.0f, 1.0f, SWEET_SPOT_OPACITY_MAX };

//--------------------------------------------------------------------------------------
// Callouts for labeling the gamepad on the help screen
//--------------------------------------------------------------------------------------
ATG::HELP_CALLOUT g_HelpCallouts[] =
{
    { ATG::HELP_BACK_BUTTON,    ATG::HELP_PLACEMENT_1, L"Display help" },
    { ATG::HELP_START_BUTTON,   ATG::HELP_PLACEMENT_1, L"Show NUI Troubleshooter"   },
    { ATG::HELP_X_BUTTON,       ATG::HELP_PLACEMENT_1, L"Show Camera Frustum"       },
    { ATG::HELP_LEFT_SHOULDER,  ATG::HELP_PLACEMENT_1, L"Previous Tilt Mode" },
    { ATG::HELP_RIGHT_SHOULDER, ATG::HELP_PLACEMENT_1, L"Next Tilt Mode" },
};

static const DWORD NUM_HELP_CALLOUTS = ARRAYSIZE( g_HelpCallouts );

//----------------------------------------------------------------------------------------------------------------------
// Framebuffer setup constants
//----------------------------------------------------------------------------------------------------------------------

static const DWORD g_dwTileWidth   = 1280;
static const DWORD g_dwTileHeight  = 256;
static const DWORD g_dwFrameWidth  = 1280;
static const DWORD g_dwFrameHeight = 720;

static const D3DRECT g_tiles[3] = 
{
    {             0,              0,  g_dwTileWidth,  g_dwTileHeight },
    {             0, g_dwTileHeight,  g_dwTileWidth, g_dwTileHeight * 2 },
    {             0, g_dwTileHeight * 2,  g_dwTileWidth, g_dwFrameHeight },
};

//----------------------------------------------------------------------------------------------------------------------
// Human-readable Tilt Adjust settings
//----------------------------------------------------------------------------------------------------------------------
static const LPWSTR g_pwstrTiltModes[] = 
{
    L"Full Skeleton",           // TILT_MODE_FULL_SKELETON
    L"Upper Body",              // TILT_MODE_UPPER_BODY
    L"Hands Over Head",         // TILT_MODE_HANDS_OVER_HEAD
    L"Force Far Space",         // TILT_MODE_FORCE_FAR_SPACE
    L"Force Near Space"         // TILT_MODE_FORCE_NEAR_SPACE
};

static const LPSTR g_pstrTiltModes[] = 
{
    "Full Skeleton",           // TILT_MODE_FULL_SKELETON
    "Upper Body",              // TILT_MODE_UPPER_BODY
    "Hands Over Head",         // TILT_MODE_HANDS_OVER_HEAD
    "Force Far Space",         // TILT_MODE_FORCE_FAR_SPACE
    "Force Near Space"         // TILT_MODE_FORCE_NEAR_SPACE
};

//----------------------------------------------------------------------------------------------------------------------
// Name: MapTiltModeToHeadSpace
// Desc: Maps a TILTMODE enum value to a NuiCameraAdjustTilt flag value. (In your application, you will most likely
//       use the flag value directly).
//----------------------------------------------------------------------------------------------------------------------
inline FLOAT MapTiltModeToHeadSpace( TILTMODE mode )
{
    switch ( mode )
    {
    case TILT_MODE_FULL_SKELETON:
    default:
        return NUI_TILT_HEAD_SPACE_FULL_SKELETON;
    case TILT_MODE_UPPER_BODY:
        return NUI_TILT_HEAD_SPACE_UPPER_BODY;
    case TILT_MODE_HANDS_OVER_HEAD:
        return NUI_TILT_HEAD_SPACE_HANDS_OVER_HEAD;
    }
}

//----------------------------------------------------------------------------------------------------------------------
// Name: D3DCOLORIsVisible
// Desc: Returns TRUE if the color provided was at all opaque; FALSE if it was fully transparent.
//----------------------------------------------------------------------------------------------------------------------
inline BOOL D3DCOLORIsVisible( D3DCOLOR color )
{
    return ( color & D3DCOLOR_ALPHA_MASK ) != 0;
}


//-------------------------------------------------------------------------------------
// Name: main
// Desc: The application's entry point
//-------------------------------------------------------------------------------------
INT __cdecl main()
{
    Sample MyAtgApp;
    ZeroMemory( &MyAtgApp.m_d3dpp, sizeof( MyAtgApp.m_d3dpp ) );

    MyAtgApp.m_d3dpp.BackBufferWidth        = 1280;
    MyAtgApp.m_d3dpp.BackBufferHeight       = 720;
    MyAtgApp.m_d3dpp.BackBufferCount        = 1;
    MyAtgApp.m_d3dpp.MultiSampleType        = D3DMULTISAMPLE_4_SAMPLES;
    MyAtgApp.m_d3dpp.EnableAutoDepthStencil = FALSE;
    MyAtgApp.m_d3dpp.DisableAutoBackBuffer  = TRUE;
    MyAtgApp.m_d3dpp.DisableAutoFrontBuffer = TRUE;
    MyAtgApp.m_d3dpp.SwapEffect             = D3DSWAPEFFECT_DISCARD;
    MyAtgApp.m_d3dpp.PresentationInterval   = D3DPRESENT_INTERVAL_ONE;

    MyAtgApp.m_dwDeviceCreationFlags       |= D3DCREATE_CREATE_THREAD_ON_1;

    MyAtgApp.Run();
}


//--------------------------------------------------------------------------------------
// Name: Sample::Initialize
// Desc: Creates all graphics resources and initializes rendering and animation systems.
//--------------------------------------------------------------------------------------
HRESULT Sample::Initialize()
{
    m_vCameraPos = s_vDefaultCameraPos;
    ZeroMemory( &m_tiltObjects, sizeof( NUI_TILT_OBJECTS) );
    ZeroMemory( &m_Skeleton, sizeof( NUI_SKELETON_FRAME ) );
    m_iCurrentSkeletonIndex = 0;

    // Initialize the avatar library
    static const DWORD dwAssetLoadHardwareThread = 5;
    if( FAILED( XAvatarInitialize(      XAVATAR_COORDINATE_SYSTEM_RIGHT_HANDED, 
                                        0, 
                                        dwAssetLoadHardwareThread,
                                        0,    
                                        0) ) ) 
    {
        ATG_PrintError( "Unable to load Avatar asset pack\n");
        return E_FAIL;
    }

    // Create event which will be signaled when frame processing ends
    m_hFrameEndEvent = CreateEvent( NULL,
        FALSE,  // auto-reset
        FALSE,  // create unsignaled
        "NuiFrameEndEvent" );

    if ( !m_hFrameEndEvent )
    {
        ATG_PrintError( "Failed to create NuiFrameEndEvent\n" );
        return E_FAIL;
    }

    HRESULT hr = NuiInitialize( NUI_INITIALIZE_FLAG_USES_SKELETON |
        NUI_INITIALIZE_FLAG_USES_COLOR |
        NUI_INITIALIZE_FLAG_USES_DEPTH_AND_PLAYER_INDEX,
        NUI_INITIALIZE_DEFAULT_HARDWARE_THREAD );

    if ( FAILED( hr ))
    {
        ATG::NuiPrintError( hr, "NuiInitialize" );
        return E_FAIL;
    }

    // Register frame end event with NUI
    hr = NuiSetFrameEndEvent( m_hFrameEndEvent, 0 );
    if( FAILED(hr) )
    {
        ATG::NuiPrintError( hr, "NuiSetFrameEndEvent" );
        return E_FAIL;
    }

    // Open the color stream
    hr = NuiImageStreamOpen( NUI_IMAGE_TYPE_COLOR, NUI_IMAGE_RESOLUTION_640x480, 0, 1, NULL, &m_hImage );
    if ( FAILED (hr) )
    {
        ATG::NuiPrintError( hr, "NuiImageStreamOpen" );
        return E_FAIL;
    }

    // Open the depth stream
    hr = NuiImageStreamOpen( NUI_IMAGE_TYPE_DEPTH_AND_PLAYER_INDEX, NUI_IMAGE_RESOLUTION_320x240, 0, 1, NULL, &m_hDepth );
    if ( FAILED (hr) )
    {
        ATG::NuiPrintError( hr, "NuiImageStreamOpen" );
        return E_FAIL;
    }

    hr = NuiSkeletonTrackingEnable( NULL, 0 );
    if ( FAILED( hr ))
    {
        ATG::NuiPrintError( hr, "NuiSkeletonTrackingEnable" );
    }

    // Obtain the avatar metadata for the user.  If the call fails, (the user doesn't
    // have an avatar associated with their profile yet or no one is 
    // signed in) then just load a random avatar.
    XAVATAR_METADATA metadata;
    if( ERROR_SUCCESS != XAvatarGetMetadataLocalUser( 0, &metadata, NULL ) )
    {
        if ( ERROR_SUCCESS != XAvatarGetMetadataRandom( XAVATAR_BODY_TYPE_ALL, 1, &metadata, NULL ) )
        {
            return E_FAIL;
        }
    }

    m_pAvatarRenderer = new AvatarRenderer( m_pd3dDevice, metadata );

    // Create background vertex shader
    static const D3DVERTEXELEMENT9 backgroundDecl[] =
    {
        { 0,  0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT,  D3DDECLUSAGE_POSITION, 0 },
        { 0, 12, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT,  D3DDECLUSAGE_TEXCOORD, 0 },
        D3DDECL_END()
    };

    if( FAILED( hr = m_pd3dDevice->CreateVertexDeclaration( backgroundDecl, &m_pBackgroundVertexDeclaration ) ) )
        ATG::FatalError( "Error %#X creating Vertex Declaration\n", hr );

    VOID* pCode = NULL;
    if( FAILED( hr = ATG::LoadFile( "game:\\Media\\Shaders\\background_vs.xvu", &pCode ) ) )
        ATG::FatalError( "Error %#X creating Vertex Shader\n", hr );

    if( FAILED( hr = m_pd3dDevice->CreateVertexShader( ( DWORD* )pCode, &m_pBackgroundVertexShader ) ) )
        ATG::FatalError( "Error %#X creating Vertex Shader\n", hr );
    ATG::UnloadFile( pCode );

    // Create background pixel shader
    if( FAILED( hr = ATG::LoadFile( "game:\\Media\\Shaders\\background_ps.xpu", &pCode ) ) )
        ATG::FatalError( "Error %#X creating Pixel Shader\n", hr );

    if( FAILED( hr = m_pd3dDevice->CreatePixelShader( ( DWORD* )pCode, &m_pBackgroundPixelShader ) ) )
        ATG::FatalError( "Error %#X creating Pixel Shader\n", hr );
    ATG::UnloadFile( pCode );


    // Initialize view parameters
    XVIDEO_MODE VideoMode;
    XGetVideoMode( &VideoMode );
    FLOAT fAspectRatio = VideoMode.fIsWideScreen == TRUE ? (16.0f / 9.0f) : (4.0f / 3.0f);
    m_matProj    = XMMatrixPerspectiveFovRH( XM_PI/4, fAspectRatio, 0.01f, 20.0f ); 

    CreateRenderTargets();

    // Create the font
    if( FAILED( m_Font.Create( "d:\\Media\\Fonts\\Arial_16.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Confine text drawing to the title safe area
    m_Font.SetWindow( ATG::GetTitleSafeArea() );

    // Create the help
    if( FAILED( m_Help.Create( "d:\\Media\\Help\\Help.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    ATG::SimpleShaders::Initialize( NULL, NULL );

    // Initialize the Picture in Picture visualization
    if( FAILED( m_pip.Initialize( m_pd3dDevice, NUI_INITIALIZE_FLAG_USES_COLOR |
                                                NUI_INITIALIZE_FLAG_USES_DEPTH_AND_PLAYER_INDEX |
                                                NUI_INITIALIZE_FLAG_USES_SKELETON, 
                                                NUI_IMAGE_RESOLUTION_640x480 ) ) )
    {
        ATG_PrintError ( "Picture in Picture initialization failed" );
    }

    if( FAILED( hr = m_Resource.Create( "d:\\Media\\Resource.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;
    m_pTextureGrass = m_Resource.GetTexture( "Ground" );
    m_pTextureSky = m_Resource.GetTexture( "Sky" );

    m_pSweetSpotTarget = m_Resource.GetTexture( "SweetSpotTarget" );

    m_playspace.Init( this, &m_debugPlayspace );

    InitHUDFeedback();
    ResetHUDFeedback();

    // ShowHUDPlayerLost(GAME_PLAYER_ONE);
    
    //Create the Avatar NUI Mapper
    SAFE_RELEASE( m_pAvatarNuiMapper );

    if( FAILED( hr = XAvatarCreateNuiMapper( m_pAvatarRenderer->GetSkeleton(), m_AvatarBodyType, &m_pAvatarNuiMapper ) ) )
        ATG::FatalError( "Error %#X creating Nui Mapper\n", hr );

    XAVATAR_NUI_MAPPER_OPTIONS options = {0};
    m_pAvatarNuiMapper->GetOptions( &options );

    options.GeneralOptions.MirrorSkeleton = FALSE;
    options.GeneralOptions.PreventSelfIntersection = TRUE;
    options.GeneralOptions.RescaleToUserProportions = TRUE;
    options.GeneralOptions.SkeletonDataIsFiltered = FALSE;
    options.GeneralOptions.DisableJumping = FALSE;

    m_pAvatarNuiMapper->SetOptions( &options );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Sample::CreateRenderTargets
// Desc: Create RenderTargets, Back/Front Buffers, and Depth Stencil
//--------------------------------------------------------------------------------------
VOID Sample::CreateRenderTargets()
{
    D3DSURFACE_PARAMETERS params = {0};

    m_pd3dDevice->CreateRenderTarget(
        g_dwTileWidth, g_dwTileHeight, (D3DFORMAT) MAKESRGBFMT( D3DFMT_X8R8G8B8 ), D3DMULTISAMPLE_4_SAMPLES, 0, 0, &m_pBackBuffer, &params );

    params.Base = m_pBackBuffer->Size / GPU_EDRAM_TILE_SIZE;
    params.HierarchicalZBase = 0;
    m_pd3dDevice->CreateDepthStencilSurface(
        g_dwTileWidth, g_dwTileHeight, D3DFMT_D24S8, D3DMULTISAMPLE_4_SAMPLES, 0, 0, &m_pDepthBuffer, &params );

    m_pd3dDevice->CreateTexture(
        g_dwFrameWidth, g_dwFrameHeight, 1, 0, (D3DFORMAT) MAKESRGBFMT( D3DFMT_LE_X8R8G8B8 ), 0, &m_pFrontBuffer[0], NULL );
    m_pd3dDevice->CreateTexture(
        g_dwFrameWidth, g_dwFrameHeight, 1, 0, (D3DFORMAT) MAKESRGBFMT( D3DFMT_LE_X8R8G8B8 ), 0, &m_pFrontBuffer[1], NULL );
    
}


//--------------------------------------------------------------------------------------
// Name: Sample::UpdateSkeletonTracking
// Desc: Read the data from the camera stream synchronously each frame and pass the
//       depthmap on to the skeleton tracking. 
//
// The Nui API will wait up to NUI_CAMERA_TIMEOUT_DEFAULT ms for a new skeleton.
//--------------------------------------------------------------------------------------
HRESULT Sample::UpdateSkeletonTracking()
{
    // Wait for frame processing to end
    if( WAIT_OBJECT_0 != WaitForSingleObject( m_hFrameEndEvent, NUI_FRAME_END_TIMEOUT_DEFAULT ) )
    {
        return E_FAIL;
    }

    // Get data from the next image frame
    HRESULT hrImage = NuiImageStreamGetNextFrame( m_hImage, 0, &m_pImageFrame );
    // Get data from the next camera depth frame
    HRESULT hrDepth = NuiImageStreamGetNextFrame( m_hDepth, 0, &m_pDepthFrame );
    // Get data from the next skeleton frame
    m_hrNuiResult = NuiSkeletonGetNextFrame( 0, &m_Skeleton );

    if ( SUCCEEDED( hrImage ) )
    {
        m_pip.SetColorTexture( m_pImageFrame->pFrameTexture );
        NuiImageStreamReleaseFrame( m_hImage, m_pImageFrame );
    }

    if ( SUCCEEDED( hrDepth ) )
    {
        m_pip.SetDepthTexture( m_pDepthFrame->pFrameTexture );
        NuiImageStreamReleaseFrame( m_hDepth, m_pDepthFrame );
    }

    if ( m_hrNuiResult == E_PENDING || FAILED ( m_hrNuiResult ) )
    {
        return m_hrNuiResult;
    }
    else
    {
        // If we don't have a lock on the skeleton currently being tracked, then switch to 
        // the first tracked skeleton we can find. If none is tracked, then leave the current 
        // index as is and try again next frame.
        if( m_Skeleton.SkeletonData[ m_iCurrentSkeletonIndex ].eTrackingState != NUI_SKELETON_TRACKED )
        {
            for( UINT i = 0; i < NUI_SKELETON_COUNT; ++ i )
            {
                if( m_Skeleton.SkeletonData[ i ].eTrackingState == NUI_SKELETON_TRACKED )
                {
                    m_iCurrentSkeletonIndex = i;
                    break;
                }
            }
        }
        m_pip.SetSkeletons( &m_Skeleton );
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Sample::Update
// Desc: Called once per frame.  Entry point for animating the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Update()
{
    PIXBeginNamedEvent( 0, __FUNCTION__ );

    // Get elapsed time
    FLOAT fDeltaTime = ( FLOAT )m_Timer.GetElapsedTime();

    UpdateTiltState();

    UpdateInput();

     // Get the skeleton
    UpdateSkeletonTracking();

    BOOL bIsTracked = m_Skeleton.SkeletonData[ m_iCurrentSkeletonIndex ].eTrackingState == NUI_SKELETON_TRACKED;
    
    // Convert the skeleton to avatar skinning data.  NUI joint positions are converted
    // to avatar joint rotations.
    if( m_pAvatarNuiMapper  )
    {
        // if we successfully retrieved skeleton data, set the data. At most this
        // will be set at 30Hz
        if ( SUCCEEDED( m_hrNuiResult ) )
        {
            m_pAvatarNuiMapper->SetNuiSkeletonData( &m_Skeleton, m_Skeleton.SkeletonData[ m_iCurrentSkeletonIndex].dwTrackingID );
        }

        // Convert the skeleton to avatar skinning data.  NUI joint positions are converted
        // to avatar joint rotations. For this sample this update should run at 60Hz since
        // it uses PRESENT_INTERVAL_ONE and the m_pAvatarNuiMapper extrapolates data
        m_pAvatarNuiMapper->Update( fDeltaTime );
        
        UpdateAvatarBasis( );     // Update the rotation of the avatar to match with the skeleton
    }
    
    // Update the animation and avatar with the newly converted skinning data
    UpdateAnimation( fDeltaTime );

    m_playspace.Update( m_iCurrentSkeletonIndex, m_Skeleton );
    
    UpdateCameraPos( bIsTracked );

   // Update UI Animations
    AnimHUDFeedback( fDeltaTime );
    
    PIXEndNamedEvent();
    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Sample::UpdateAnimation
// Desc: Updates the animation stream and blends between the previous and next frames.
//       Called once per frame.
//--------------------------------------------------------------------------------------
HRESULT Sample::UpdateAnimation( FLOAT fDeltaTime )
{
    m_fCurTime += fDeltaTime;
    
    if( SUCCEEDED( m_hrNuiResult ) && m_pAvatarNuiMapper )
    {
        for ( DWORD j = 0; j < m_pAvatarRenderer->GetSkeleton()->Count; ++j )
        {
            m_pAvatarNuiMapper->GetJointPose( j, &m_AvatarJointPose[j], NULL );
        }

        m_pAvatarRenderer->SetJoints( m_AvatarJointPose );
        m_pAvatarRenderer->Update();

        // Update the color feedback animation on the avatar.
        UpdateAvatarAnim();

        return S_OK;
    }
    else
    {
        return E_FAIL;
    }
}


//--------------------------------------------------------------------------------------
// Name: Sample::UpdateAvatarBasis
// Desc: Rotate Avatar with user.
//--------------------------------------------------------------------------------------
HRESULT Sample::UpdateAvatarBasis( )
{
    XAVATAR_NUI_MAPPER_TRANSFORM_INFO info;

    assert( m_pAvatarNuiMapper );
    if ( SUCCEEDED( m_pAvatarNuiMapper->GetAvatarTransformInfo( &info ) ) )
    {
        // Note: this should not be necessary, but there is a mismatch between the avatar mapping coordinate system
        // and the skeleton tracking coordinate system, so we retrofit the skeletal tracking coordinate to match.
        const XMVECTOR s_AvatarPositionAdjust = { -1.0f, 1.0f, -1.0f, 0.0f };

        XMVECTOR vAvatarPos = m_playspace.GetPlayerStatus().m_vLocation * s_AvatarPositionAdjust;
        m_vAvatarPos = vAvatarPos;

        XMMATRIX rotMtx = XMMatrixRotationY( info.YawInRadians );
        XMMATRIX transMtx = XMMatrixTranslation( vAvatarPos.x, vAvatarPos.y, vAvatarPos.z );
        
        m_matWorld = rotMtx * transMtx;
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Sample::RenderBackground
// Desc: Renders the background.
//--------------------------------------------------------------------------------------
HRESULT Sample::RenderBackground()
{
    struct FloorVert
    {
        float    x, y, z;
        float    u, v;
    };

    PIXBeginNamedEvent( 0xFFFFFFFF, "Background Render" );

    m_pd3dDevice->SetRenderState_Inline( D3DRS_ZENABLE, TRUE );
    m_pd3dDevice->SetRenderState_Inline( D3DRS_ZWRITEENABLE, TRUE );
    m_pd3dDevice->SetRenderState_Inline( D3DRS_DEPTHBIAS, 0 );
    m_pd3dDevice->SetRenderState_Inline( D3DRS_ALPHABLENDENABLE, FALSE );
    m_pd3dDevice->SetRenderState_Inline( D3DRS_ALPHATESTENABLE, FALSE );
    m_pd3dDevice->SetRenderState_Inline( D3DRS_VIEWPORTENABLE, TRUE );

    m_pd3dDevice->SetVertexDeclaration( m_pBackgroundVertexDeclaration );
    m_pd3dDevice->SetVertexShader( m_pBackgroundVertexShader );
    m_pd3dDevice->SetPixelShader( m_pBackgroundPixelShader );

    FLOAT floorWidth = 31.0f;
    FloorVert floorVerts[4] = 
    {
        {  floorWidth, 0.0f, -floorWidth, 8.0f, 0.0f },
        { -floorWidth, 0.0f, -floorWidth, 0.0f, 0.0f },
        {  floorWidth, 0.0f,  floorWidth, 8.0f, 8.0f },
        { -floorWidth, 0.0f,  floorWidth, 0.0f, 8.0f }
    };

    INT skySections = 40;
    FloorVert skyVerts[82];

    FloorVert* skyVert = skyVerts;

    const FLOAT uOffset = .12f;
    for ( INT i = 0; i < skySections + 1; i ++ )
    {
        FLOAT width = 6.0f;

        skyVert->x = sin( (i * 2 * D3DX_PI) / skySections ) * width;
        skyVert->y = 0.0f;
        skyVert->z = cos( (i * 2 * D3DX_PI) / skySections ) * width;
        skyVert->u = (2*i) / (FLOAT)skySections + uOffset;
        skyVert->v = 1.0f;
        skyVert++;

        skyVert->x = sin( (i * 2 * D3DX_PI) / skySections ) * width;
        skyVert->y = 17.0f;
        skyVert->z = cos( (i * 2 * D3DX_PI) / skySections ) * width;
        skyVert->u = (2*i) / (FLOAT)skySections + uOffset;
        skyVert->v = -4.5f;
        skyVert++;
    }

    XMMATRIX id = XMMatrixIdentity();
    m_pd3dDevice->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP);
    m_pd3dDevice->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP);

    m_pd3dDevice->SetVertexShaderConstantF( 0, (FLOAT*)&m_matView, 4 );
    m_pd3dDevice->SetVertexShaderConstantF( 4, (FLOAT*)&m_matProj, 4 );
    m_pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_NONE );

    m_pd3dDevice->SetTexture( 0, m_pTextureGrass );
    m_pd3dDevice->DrawPrimitiveUP( D3DPT_TRIANGLESTRIP, 2, &floorVerts, sizeof(FloorVert) );

    m_pd3dDevice->SetTexture( 0, m_pTextureSky );
    m_pd3dDevice->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
    m_pd3dDevice->DrawPrimitiveUP( D3DPT_TRIANGLESTRIP, skySections*2, &skyVerts, sizeof(FloorVert) );

    PIXEndNamedEvent();

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Sample::Render
// Desc: Renders the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Render()
{
    PIXBeginNamedEvent( 0, __FUNCTION__ );
    {
        // Render scene
        static const D3DVECTOR4 clearColor = { 0.75f, 0.78f, 0.86f, 1.0f };
        m_pd3dDevice->SetRenderTarget( 0, m_pBackBuffer );
        m_pd3dDevice->SetDepthStencilSurface( m_pDepthBuffer );
        m_pd3dDevice->BeginTiling( 0, ARRAYSIZE(g_tiles), g_tiles, &clearColor, 1, 0 );

        RenderBackground();

        RenderSweetSpotTarget();
        
        if ( m_bShowPlayspace )
        {
            RenderPlayspaceFrustum();
        }

        PIXBeginNamedEvent( 0xFFFFFFFF, "Avatar Render" );
        {
            if( m_Skeleton.SkeletonData[ m_iCurrentSkeletonIndex ].eTrackingState == NUI_SKELETON_TRACKED )
            {
                m_pAvatarRenderer->Render( m_matWorld, m_matView, m_matProj, m_vAvatarColorFilter );
            }
        }

        PIXEndNamedEvent();

        RenderOverlays();

        // Draw the raw depth and image map with skeleton overlaid as visualization.
        const FLOAT drawWidth = 200.0f;
        const FLOAT drawHeight = drawWidth * 3.0f / 4.0f;
        const FLOAT drawX = 50.0f;
        const FLOAT drawY = 720.0f - 50.0f - drawHeight;
        m_pip.BeginRender();
        m_pip.RenderColorStream( drawX, drawY, drawWidth, drawHeight );
        m_pip.RenderSkeletons( drawX, drawY, drawWidth, drawHeight, FALSE, TRUE );
        m_pip.RenderDepthStream( drawX + drawWidth + 10, drawY, drawWidth, drawHeight );
        m_pip.RenderSkeletons( drawX + drawWidth + 10, drawY, drawWidth, drawHeight, FALSE, FALSE );
        m_pip.EndRender();

        m_pd3dDevice->EndTiling( 0, NULL, m_pFrontBuffer[m_dwCurFrontBuffer], NULL, 1, 0, NULL );
        
        // Present the backbuffer contents to the display
        m_pd3dDevice->SynchronizeToPresentationInterval();
        m_pd3dDevice->Swap( m_pFrontBuffer[ m_dwCurFrontBuffer ], NULL );
        // Flip buffers
        m_dwCurFrontBuffer = ( m_dwCurFrontBuffer + 1 ) & 1;

        m_Timer.MarkFrame();
    }
    PIXEndNamedEvent();
    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Sample::RenderOverlays
// Desc: Show title, timers, feedback and help.
//--------------------------------------------------------------------------------------
VOID Sample::RenderOverlays()
{
    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_SRCBLEND, D3DBLEND_SRCALPHA );
    m_pd3dDevice->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA );

    PIXBeginNamedEvent( 0, __FUNCTION__ );
    {
        if( m_bDrawHelp )
        {
            m_Help.Render( &m_Font, g_HelpCallouts, NUM_HELP_CALLOUTS );
        }
        else
        {
            RenderHUDFeedback();

            m_Font.Begin();

            m_Font.SetScaleFactors( 1.2f, 1.2f );
            m_Font.DrawText( 0, 0, 0xffffffff, L"Playspace Feedback" );

            m_Font.SetScaleFactors( 1.0f, 1.0f );
            m_Font.DrawText( 0, 0, 0xffffff00, m_Timer.GetFrameRate(), ATGFONT_RIGHT );
            
            const PlayerStatus& ps = m_playspace.GetPlayerStatus();

            // Write current player state (in colors from Red through to Green depending on how close to optimum the 
            // state is).
            D3DCOLOR textColor;
            switch( ps.m_state )
            {
                case PLAYERSTATE_OUTSIDE_PLAYSPACE:
                    { textColor = 0xFFFF4000; break; }  // Reddish-orange.
                case PLAYERSTATE_IN_PLAYSPACE_BORDER:
                    { textColor = 0xFFFF8000; break; }  // Amber
                case PLAYERSTATE_IN_USABLE_PLAYSPACE:
                    { textColor = 0xFFFFFF00; break; }  // Yellow
                case PLAYERSTATE_IN_SWEET_SPOT:
                    { textColor = 0xFF00FF00; break; }  // Green
                case PLAYERSTATE_LOST_TRACKING:
                case PLAYERSTATE_NONE:
                default:
                    { textColor = 0xFFFF0000; break; }  // Red
            }

            m_Font.DrawText( 0, 20, textColor, g_pwstrPlayerState[ ps.m_state ], ATGFONT_RIGHT );

            
            WCHAR temp[200];
            wsprintfW( temp, L"SS Dist: %.2f", ps.m_fDistanceFromSweetSpot );

            m_Font.DrawText( 0, 40, 0xffffffff, ps.m_fDistanceFromSweetSpot >= 0.0f ? temp : L"SS Dist: ?", ATGFONT_RIGHT );

            LPCWSTR pText = L"Inside Usable PS";
            if ( ps.m_dwUsablePlayspaceFlags != FRUSTUMPOSITION_INSIDE )
            {
                wsprintfW( temp, L"Outside Usable PS:%s%s%s%s",
                    (ps.m_dwUsablePlayspaceFlags & FRUSTUMPOSITION_OUT_LEFT) != 0 ? L" Left" : L"",
                    (ps.m_dwUsablePlayspaceFlags & FRUSTUMPOSITION_OUT_RIGHT) != 0 ? L" Right" : L"",
                    (ps.m_dwUsablePlayspaceFlags & FRUSTUMPOSITION_OUT_FRONT) != 0 ? L" Front" : L"",
                    (ps.m_dwUsablePlayspaceFlags & FRUSTUMPOSITION_OUT_BACK) != 0 ? L" Back" : L""
                    );
                pText = temp;
            }
            m_Font.DrawText( 0, 60, 0xffffffff, pText, ATGFONT_RIGHT );

            pText = L"Inside Normal PS";
            if ( ps.m_dwNormalPlayspaceFlags != FRUSTUMPOSITION_INSIDE )
            {
                wsprintfW( temp, L"Outside Normal PS:%s%s%s%s",
                    (ps.m_dwNormalPlayspaceFlags & FRUSTUMPOSITION_OUT_LEFT) != 0 ? L" Left" : L"",
                    (ps.m_dwNormalPlayspaceFlags & FRUSTUMPOSITION_OUT_RIGHT) != 0 ? L" Right" : L"",
                    (ps.m_dwNormalPlayspaceFlags & FRUSTUMPOSITION_OUT_FRONT) != 0 ? L" Front" : L"",
                    (ps.m_dwNormalPlayspaceFlags & FRUSTUMPOSITION_OUT_BACK) != 0 ? L" Back" : L""
                    );
                pText = temp;
            }
            m_Font.DrawText( 0, 80, 0xffffffff, pText, ATGFONT_RIGHT );

            wsprintfW( temp, L"Player [%.2f,%.2f,%.2f]", m_vAvatarPos.x, m_vAvatarPos.y, m_vAvatarPos.z );
            m_Font.DrawText( 0, 100, 0xffffffff, temp, ATGFONT_RIGHT );

            const XMVECTOR& vCameraPos = m_vCameraPos;
            wsprintfW( temp, L"Camera [%.2f,%.2f,%.2f]", vCameraPos.x, vCameraPos.y, vCameraPos.z );
            m_Font.DrawText( 0, 120, 0xffffffff, temp, ATGFONT_RIGHT );
            
            wsprintfW( temp, L"Tilt Mode: %s", g_pwstrTiltModes[ m_currentTiltMode ] );
            m_Font.DrawText( 0, 140, 0xFFFFFFFF, temp, ATGFONT_RIGHT );

            if ( m_tiltState == TILT_STATE_BUSY )
            {
                m_Font.DrawText( 0, 160, 0xFF00FFFF, L"Camera Tilting - Please Wait", ATGFONT_RIGHT );
            }
            else if ( m_tiltState == TILT_STATE_NOINFO )
            {
                m_Font.DrawText( 0, 160, 0xFFFF4000, L"Camera Busy - Please Wait", ATGFONT_RIGHT );
            }

            if ( IsTiltObjectDataValid() )
            {
                wsprintfW( temp, L"Camera Height: %.1fm", m_tiltObjects.CameraHeightMeters );
                m_Font.DrawText( 0, 180, 0xFFFFFFFF, temp, ATGFONT_RIGHT );
            }

            // The angle of the camera from horizontal.
            wsprintfW( temp, L"Camera Elevation Angle: %.1f deg.", CalcCameraElevationDegrees() );
            m_Font.DrawText( 0, 200, 0xFFFFFFFF, temp, ATGFONT_RIGHT );
 
            m_Font.DrawText( 0, -1, 0xFFFFFFFF, L"Press " GLYPH_START_BUTTON L"To Open Troubleshooter", ATGFONT_RIGHT );
            m_Font.End();
        }
    }

    PIXEndNamedEvent();
}


//----------------------------------------------------------------------------------------------------------------------
// Name: Sample::InitHUDFeedback
// Desc: Initializes the player lost/found glyphs and movement hints.
//----------------------------------------------------------------------------------------------------------------------
void Sample::InitHUDFeedback()
{
    const D3DCOLOR WHITE_TRANSPARENT = D3DCOLOR_RGBA( 255,255,255,0 );

    // Load textures from the resource bundle.

    m_HUDResources.m_pTextureDavinciOutline = m_Resource.GetTexture( "DavinciOutline" );
    m_HUDResources.m_pTextureDavinciGood = m_Resource.GetTexture( "DavinciGood" );
    m_HUDResources.m_pTextureDavinciMan = m_Resource.GetTexture( "DavinciMan" );
    m_HUDResources.m_pTextureDavinciLost = m_Resource.GetTexture( "DavinciLost" );

    m_HUDResources.m_pTextureHintMoveLeft = m_Resource.GetTexture( "PlayerMoveLeftHint" );
    m_HUDResources.m_pTextureHintMoveRight = m_Resource.GetTexture( "PlayerMoveRightHint" );
    m_HUDResources.m_pTextureHintMoveFwd = m_Resource.GetTexture( "PlayerMoveFwdHint" );
    m_HUDResources.m_pTextureHintMoveBack = m_Resource.GetTexture( "PlayerMoveBackHint" );


    // Position the glyphs at the top left and right inside the title-safe area.
    UINT uiWidth;
    ATG::GetVideoSettings( &uiWidth );
    
    LONG width = uiWidth / 10;

    D3DRECT rectTitleSafe = ATG::GetTitleSafeArea();
    rectTitleSafe.y1 += 40;  // Move the glyph down below the sample title.

    D3DRECT r;
    r.x1 = rectTitleSafe.x1;
    r.x2 = rectTitleSafe.x1 + width;
    r.y1 = rectTitleSafe.y1;
    r.y2 = rectTitleSafe.y1 + width;
    m_HUDPlayerGlyphs[0].m_rectGlyph = r;

#if (GAME_PLAYER_COUNT == 2)
// If we have 2 players, 2nd set of tracking glyphs goes on right hand side.

    r.x1 = rectTitleSafe.x2 - width;
    r.x2 = rectTitleSafe.x2;
    r.y1 = rectTitleSafe.y1;
    r.y2 = rectTitleSafe.y1 + width;
    m_HUDPlayerGlyphs[1].m_rectGlyph = r;

#endif

    const LONG iHalfWidth = width / 2;

    // Bind fader parameters to the colors they represent, and initialize the colors.

    LONG iStride = (LONG)(uiWidth / GAME_PLAYER_COUNT);
    LONG iStartX = iStride / 2;

    for (int i = 0; i < GAME_PLAYER_COUNT; ++i )
    {
        // Setup player positioning hint glyph screen locations.

        // They're clustered in something similar to a compass rose in the middle of each player's
        // half of the screen.

        D3DRECT rMoveFwd, rMoveBack, rMoveLeft, rMoveRight;

        rMoveFwd.x1 = iStartX - iHalfWidth;
        rMoveFwd.x2 = rMoveFwd.x1 + width;
        rMoveFwd.y1 = rectTitleSafe.y2 - width;
        rMoveFwd.y2 = rectTitleSafe.y2;

        m_HUDPlayerGlyphs[ i ].m_rectHintBottom = rMoveFwd;

        rMoveBack.x1 = rMoveFwd.x1;
        rMoveBack.x2 = rMoveFwd.x1 + width;
        rMoveBack.y1 = rMoveFwd.y1 - width;
        rMoveBack.y2 = rMoveFwd.y1;

        m_HUDPlayerGlyphs[ i ].m_rectHintTop = rMoveBack;

        rMoveRight.x2 = rMoveFwd.x1;
        rMoveRight.x1 = rMoveFwd.x1 - width;
        rMoveRight.y1 = rMoveFwd.y1 - iHalfWidth;
        rMoveRight.y2 = rMoveRight.y1 + width;

        m_HUDPlayerGlyphs[ i ].m_rectHintLeft = rMoveRight;

        rMoveLeft.x2 = rMoveFwd.x2 + width;
        rMoveLeft.x1 = rMoveFwd.x2;
        rMoveLeft.y1 = rMoveFwd.y1 - iHalfWidth;
        rMoveLeft.y2 = rMoveLeft.y1 + width;

        m_HUDPlayerGlyphs[ i ].m_rectHintRight = rMoveLeft;

        iStartX += iStride;

        // Bind the animations to their color parameters.

        m_HUDPlayerGlyphs[ i ].m_animDavinciOutline.Bind( &m_HUDPlayerGlyphs[ i ].m_colDavinciOutline );
        m_HUDPlayerGlyphs[ i ].m_animDavinciMan.Bind( &m_HUDPlayerGlyphs[ i ].m_colDavinciMan );
        m_HUDPlayerGlyphs[ i ].m_animDavinciLost.Bind( &m_HUDPlayerGlyphs[ i ].m_colDavinciLost );
        m_HUDPlayerGlyphs[ i ].m_animDavinciGood.Bind( &m_HUDPlayerGlyphs[ i ].m_colDavinciGood );

        m_HUDPlayerGlyphs[ i ].m_animHints.Bind( &m_HUDPlayerGlyphs[ i ].m_colHint );

        // Initialize everything else.

        m_HUDPlayerGlyphs[ i ].m_colDavinciOutline = WHITE_TRANSPARENT;
        m_HUDPlayerGlyphs[ i ].m_colDavinciMan = WHITE_TRANSPARENT;
        m_HUDPlayerGlyphs[ i ].m_colDavinciLost = WHITE_TRANSPARENT;
        m_HUDPlayerGlyphs[ i ].m_colDavinciGood = WHITE_TRANSPARENT;

        m_HUDPlayerGlyphs[ i ].m_colHint = WHITE_TRANSPARENT;

        m_HUDPlayerGlyphs[ i ].m_effect = PSH_NONE;
        m_HUDPlayerGlyphs[ i ].m_dwFrustumFlags = FRUSTUMPOSITION_INSIDE;
    }
}



//----------------------------------------------------------------------------------------------------------------------
// Name: Sample::ResetHUDFeedback
// Desc: Resets the Player Lost/Found glyphs and HUD hints back to a known starting point (hidden).
//----------------------------------------------------------------------------------------------------------------------
void Sample::ResetHUDFeedback()
{
    // Note: Immediate/Set operations operate immediately on SetAnimation.

    for (int i = 0; i < GAME_PLAYER_COUNT; ++i )
    {
        m_HUDPlayerGlyphs[ i ].m_animDavinciGood.SetAnimation( FADE_OUT_IMMEDIATE );
        m_HUDPlayerGlyphs[ i ].m_animDavinciLost.SetAnimation( FADE_OUT_IMMEDIATE );
        m_HUDPlayerGlyphs[ i ].m_animDavinciMan.SetAnimation( FADE_OUT_IMMEDIATE );
        m_HUDPlayerGlyphs[ i ].m_animDavinciOutline.SetAnimation( FADE_OUT_IMMEDIATE );
        m_HUDPlayerGlyphs[ i ].m_animHints.SetAnimation( FADE_OUT_IMMEDIATE );
        m_HUDPlayerGlyphs[ i ].m_effect = PSH_NONE;
        m_HUDPlayerGlyphs[ i ].m_dwFrustumFlags = FRUSTUMPOSITION_INSIDE;
    }
}


//----------------------------------------------------------------------------------------------------------------------
// Name: Sample::ShowHUDPlayerLost
// Desc: Starts the "player lost" animation for the given player ID.
//----------------------------------------------------------------------------------------------------------------------
void Sample::ShowHUDPlayerLost( GAMEPLAYERID id )
{
    m_HUDPlayerGlyphs[ id ].m_animDavinciGood.SetAnimation( FADE_OUT_IMMEDIATE );
    m_HUDPlayerGlyphs[ id ].m_animDavinciOutline.SetAnimation( FADE_IN_1SECMAX );
    m_HUDPlayerGlyphs[ id ].m_animDavinciLost.SetAnimation( FADE_PULSE_5SEC_INFINITE );
    m_HUDPlayerGlyphs[ id ].m_animDavinciMan.SetAnimation( FADE_PULSE_5SEC_INFINITE_ALTPHASE );
}


//----------------------------------------------------------------------------------------------------------------------
// Name: Sample::ShowHUDPlayerFound
// Desc: Starts the player found animation for the given player ID
//----------------------------------------------------------------------------------------------------------------------
void Sample::ShowHUDPlayerFound( GAMEPLAYERID id )
{
    m_HUDPlayerGlyphs[ id ].m_animDavinciLost.SetAnimation( FADE_OUT_HALFSECMAX );
    m_HUDPlayerGlyphs[ id ].m_animDavinciMan.SetAnimation( FADE_PULSE_5SEC_ONCE );
    m_HUDPlayerGlyphs[ id ].m_animDavinciGood.SetAnimation( FADE_PULSE_5SEC_ONCE );
    m_HUDPlayerGlyphs[ id ].m_animDavinciOutline.SetAnimation( FADE_PULSE_5SEC_ONCE );
}


//----------------------------------------------------------------------------------------------------------------------
// Name: Sample::HideHUDPlayerIcon
// Desc: Hides the player lost/found glyphs at the top of the screen
//----------------------------------------------------------------------------------------------------------------------
void Sample::HideHUDPlayerIcon( GAMEPLAYERID id, BOOL fImmediate )
{
    const SimpleAnim* pAnim;
    if ( fImmediate )
    {
        pAnim = FADE_OUT_IMMEDIATE;
    }
    else
    {
        pAnim = FADE_OUT_HALFSECMAX;
    }

    PlayspaceHUDGlyph& dg = m_HUDPlayerGlyphs[ id ];
    dg.m_animDavinciGood.SetAnimation( pAnim );
    dg.m_animDavinciLost.SetAnimation( pAnim );
    dg.m_animDavinciMan.SetAnimation( pAnim );
    dg.m_animDavinciOutline.SetAnimation( pAnim );
}


//----------------------------------------------------------------------------------------------------------------------
// Name: Sample::RenderHUDFeedback
// Desc: Renders the player lost/found glyphs at the top of the screen, and the playspace 'hint' compass glyphs
//----------------------------------------------------------------------------------------------------------------------
void Sample::RenderHUDFeedback()
{
    for ( int i = 0; i < GAME_PLAYER_COUNT; ++i )
    {
        PlayspaceHUDGlyph& dg = m_HUDPlayerGlyphs[ i ];

        // Render the playspace helpers
        if ( D3DCOLORIsVisible( dg.m_colDavinciOutline ) )
        {
            ATG::DebugDraw::DrawScreenSpaceTexturedRectColored( dg.m_rectGlyph, m_HUDResources.m_pTextureDavinciOutline,
                dg.m_colDavinciOutline );
        }

        if ( D3DCOLORIsVisible( dg.m_colDavinciMan ) )
        {
            ATG::DebugDraw::DrawScreenSpaceTexturedRectColored( dg.m_rectGlyph, m_HUDResources.m_pTextureDavinciMan,
                dg.m_colDavinciMan );
        }

        if ( D3DCOLORIsVisible( dg.m_colDavinciLost ) )
        {
            ATG::DebugDraw::DrawScreenSpaceTexturedRectColored( dg.m_rectGlyph, m_HUDResources.m_pTextureDavinciLost,
                dg.m_colDavinciLost );
        }

        if ( D3DCOLORIsVisible( dg.m_colDavinciGood ) )
        {
            ATG::DebugDraw::DrawScreenSpaceTexturedRectColored( dg.m_rectGlyph, m_HUDResources.m_pTextureDavinciGood,
                dg.m_colDavinciGood );
        }

        if ( D3DCOLORIsVisible( dg.m_colHint ) && dg.m_dwFrustumFlags & FRUSTUMPOSITION_OUT_RIGHT )
        {
            ATG::DebugDraw::DrawScreenSpaceTexturedRectColored( dg.m_rectHintRight, m_HUDResources.m_pTextureHintMoveLeft,
                dg.m_colHint );
        }

        if ( D3DCOLORIsVisible( dg.m_colHint ) && dg.m_dwFrustumFlags & FRUSTUMPOSITION_OUT_LEFT )
        {
            ATG::DebugDraw::DrawScreenSpaceTexturedRectColored( dg.m_rectHintLeft, m_HUDResources.m_pTextureHintMoveRight,
                dg.m_colHint );
        }

        if ( D3DCOLORIsVisible( dg.m_colHint ) && dg.m_dwFrustumFlags & FRUSTUMPOSITION_OUT_FRONT )
        {
            ATG::DebugDraw::DrawScreenSpaceTexturedRectColored( dg.m_rectHintTop, m_HUDResources.m_pTextureHintMoveBack,
                dg.m_colHint );
        }

        if ( D3DCOLORIsVisible( dg.m_colHint ) && dg.m_dwFrustumFlags & FRUSTUMPOSITION_OUT_BACK )
        {
            ATG::DebugDraw::DrawScreenSpaceTexturedRectColored( dg.m_rectHintBottom, m_HUDResources.m_pTextureHintMoveFwd,
                dg.m_colHint );
        }
    }
}


//----------------------------------------------------------------------------------------------------------------------
// Name: Sample::AnimHUDFeedback
// Desc: Animates the player lost/found glyphs at the top of the screen and any playspace hints.
//----------------------------------------------------------------------------------------------------------------------
void Sample::AnimHUDFeedback( FLOAT fDeltaTime )
{
    for ( int i = 0; i < GAME_PLAYER_COUNT; ++i )
    {
        PlayspaceHUDGlyph& dg = m_HUDPlayerGlyphs[ i ];
        dg.m_animDavinciOutline.Update( fDeltaTime );
        dg.m_animDavinciLost.Update( fDeltaTime );
        dg.m_animDavinciGood.Update( fDeltaTime );
        dg.m_animDavinciMan.Update( fDeltaTime );
        dg.m_animHints.Update( fDeltaTime );
    }
}

//----------------------------------------------------------------------------------------------------------------------
// Name: Sample::UpdateCameraPos
// Desc: Updates the camera position. If the player is tracked, the camera position is offset in X and Y by the
//       scaled offset of the player's center from the camera axis. This reinforces the avateering relationship.
//----------------------------------------------------------------------------------------------------------------------
void Sample::UpdateCameraPos( BOOL bTrackingPlayer )
{
    const FLOAT CAMERA_OFFSET_SCALE = 0.75f;
    static const XMVECTORF32 s_vUp = {0.0f,1.0f,0.0f,0.0f};
    static const XMVECTORF32 s_vCameraScale = { CAMERA_OFFSET_SCALE, 0.0f, 0.0f, 0.0f };

    // Find distance from sweetspot to adjust camera by.
    XMVECTOR vAvatarSSRelative = m_vAvatarPos - g_vSweetSpot;
    XMVECTOR vAvatarPosScaled = vAvatarSSRelative * s_vCameraScale;

    if ( !bTrackingPlayer )
    {
        vAvatarPosScaled = XMVectorZero();
    }

    XMVECTOR vLookAt = s_vCameraLookAt + vAvatarPosScaled;
    XMVECTOR vCameraPos = s_vDefaultCameraPos + vAvatarPosScaled;

    // Smooth out the camera motion a bit.
    const FLOAT DELTA_TO_APPLY_PER_FRAME = 0.05f;
    vCameraPos = ( vCameraPos * DELTA_TO_APPLY_PER_FRAME )
               + ( m_vCameraPos * ( 1.0f - DELTA_TO_APPLY_PER_FRAME ) );
    m_vCameraPos = vCameraPos;

    m_matView  = XMMatrixLookAtRH( vCameraPos, vLookAt, s_vUp );

    // Update ATG Debug Draw projection matrix
    ATG::DebugDraw::SetViewProjection( m_matView * m_matProj );
}


//----------------------------------------------------------------------------------------------------------------------
// Name: Sample::StartSweetSpotGlow
// Desc: Initiates the Avatar's "I'm in the sweet spot" glow cycle.
//----------------------------------------------------------------------------------------------------------------------
void Sample::StartSweetSpotGlow()
{
    SetAvatarAnim( AVATARANIM_SWEETSPOTGLOW );
}


//----------------------------------------------------------------------------------------------------------------------
// Name: Sample::StopSweetSpotGlow
// Desc: Stops the Avatar's "I'm in the sweet spot" glow cycle.
//----------------------------------------------------------------------------------------------------------------------
void Sample::StopSweetSpotGlow()
{
    SetAvatarAnim( AVATARANIM_NONE );
}


//----------------------------------------------------------------------------------------------------------------------
// Name: Sample::ShowHUDPlayspaceHint
// Desc: Shows a hint in the player's playfield, telling them to move in a given direction to be back in the playspace.
//----------------------------------------------------------------------------------------------------------------------
void Sample::ShowHUDPlayspaceHint( GAMEPLAYERID id, PLAYSPACEHINTEFFECT effect, DWORD dwFrustumPositionFlags )
{
    if ( effect != PSH_NONE )
    {
        m_HUDPlayerGlyphs[ id ].m_dwFrustumFlags = dwFrustumPositionFlags;
    }
    
    if ( m_HUDPlayerGlyphs[ id ].m_effect != effect )
    {
        m_HUDPlayerGlyphs[ id ].m_effect = effect;

        if ( effect == PSH_NONE )
        {
            m_HUDPlayerGlyphs[ id ].m_animHints.SetAnimation( FADE_OUT_1SECMAX );
        }
        else if ( effect == PSH_PULSE )
        {
            m_HUDPlayerGlyphs[ id ].m_animHints.SetAnimation( FADE_PULSE_5SEC_INFINITE );
        }
        else if ( effect == PSH_SOLID )
        {
            m_HUDPlayerGlyphs[ id ].m_animHints.SetAnimation( FADE_IN_1SECMAX );
        }

    }
}


//----------------------------------------------------------------------------------------------------------------------
// Name: Sample::SetAvatarOpacity
// Desc: Sets the opacity of the Avatar. Used when the player's not in the sweet spot, but is still in the playspace to
//       reinforce the idea that they're moving away from it.
//----------------------------------------------------------------------------------------------------------------------
void Sample::SetAvatarOpacity( FLOAT fOpacity /*= 1.0f */ )
{
    m_vAvatarColorFilter.w = fOpacity;
}

//----------------------------------------------------------------------------------------------------------------------
// Name: Sample::~Sample
// Desc: Destroys the sample. Typically never called.
//----------------------------------------------------------------------------------------------------------------------
Sample::~Sample()
{

    // Note that this code will never actually be executed, as quitting a sample
    // simply reboots the dev kit; shutting down the system is not necessary in this
    // case. If it is necessary to shut down the system, this is the correct way to
    // do so.
    if( m_pAvatarRenderer )
        delete m_pAvatarRenderer;

    // Shutdown XAvatar and release memory
    XAvatarShutdown();
    NuiSkeletonTrackingDisable();
    NuiShutdown( );
}


//----------------------------------------------------------------------------------------------------------------------
// Name: Sample::SetAvatarAnim
// Desc: Sets the desired color animation on the player's Avatar.
//----------------------------------------------------------------------------------------------------------------------
void Sample::SetAvatarAnim( AVATARANIM animation )
{
    m_runningAnim = animation;
    m_fAvatarAnimStart = m_fCurTime;

    if ( animation == AVATARANIM_NONE )
    {
        m_vAvatarColorFilter = s_vAvatarDefaultColor;
    }
    UpdateAvatarAnim();
}


//----------------------------------------------------------------------------------------------------------------------
// Name: Sample::UpdateAvatarAnim
// Desc: Updates the color/opacity animation on the Avatar.
//----------------------------------------------------------------------------------------------------------------------
void Sample::UpdateAvatarAnim()
{
    if ( m_runningAnim == AVATARANIM_NONE ) 
        return;

    FLOAT fElapsedTime = m_fCurTime - m_fAvatarAnimStart;

    switch ( m_runningAnim )
    {
        case AVATARANIM_SWEETSPOTGLOW:
        {
            static const XMVECTORF32 s_Green = { 0.0f, 1.0f, 0.0f, SWEET_SPOT_OPACITY_MAX };

            // Ramp up to 100% R, 100% G, 50% B for 0.5s
            if ( fElapsedTime < 0.5f )
            {
                m_vAvatarColorFilter = XMVectorLerp( s_vAvatarDefaultColor, s_Green, fElapsedTime * 2.0f );
            }
            // Then back to 100% R, 100% G, 100% B for 0.5s
            else if ( fElapsedTime < 1.0f )
            {
                fElapsedTime -= 0.5f;
                m_vAvatarColorFilter = XMVectorLerp( s_Green, s_vAvatarDefaultColor, fElapsedTime * 2.0f );
            }
            // Hold at 100% R, 100% G, 100% B for 0.5s
            else if ( fElapsedTime < 1.5f )
            {
                m_vAvatarColorFilter = s_vAvatarDefaultColor;
            }
            // Loop
            else 
            {
                m_fAvatarAnimStart = m_fCurTime;
                m_vAvatarColorFilter = s_vAvatarDefaultColor;
            }
    
        }
    case AVATARANIM_OUTOFBOUNDSCYCLE:
    case AVATARANIM_EDGEOFBOUNDARYCYCLE:
    default:
        {
            __noop;
        }

    }
}


//----------------------------------------------------------------------------------------------------------------------
// Name: Sample::RenderSweetSpotTarget
// Desc: Renders a target on the floor where the sweet-spot lives.
//----------------------------------------------------------------------------------------------------------------------
void Sample::RenderSweetSpotTarget()
{
    static const XMFLOAT2 v2_UVRepeat(1.0f, 1.0f);
 
    m_pd3dDevice->SetRenderState_Inline( D3DRS_ZENABLE, FALSE );
    m_pd3dDevice->SetRenderState_Inline( D3DRS_ZWRITEENABLE, FALSE );
    m_pd3dDevice->SetRenderState_Inline( D3DRS_DEPTHBIAS, 0 );
    m_pd3dDevice->SetRenderState_Inline( D3DRS_ALPHABLENDENABLE, TRUE );
    m_pd3dDevice->SetRenderState_Inline( D3DRS_ALPHATESTENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_SRCBLEND, D3DBLEND_SRCALPHA );
    m_pd3dDevice->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA );

    XMFLOAT3 vTopLeft = XMFLOAT3( - SWEET_SPOT_RADIUS, 0, -SWEET_SPOT_Z_DISTANCE + SWEET_SPOT_RADIUS );
    XMFLOAT3 vTopRight = XMFLOAT3( SWEET_SPOT_RADIUS, 0, -SWEET_SPOT_Z_DISTANCE + SWEET_SPOT_RADIUS );
    XMFLOAT3 vBottomLeft = XMFLOAT3( - SWEET_SPOT_RADIUS, 0, -SWEET_SPOT_Z_DISTANCE  - SWEET_SPOT_RADIUS );

    ATG::DebugDraw::DrawTexturedQuad( vTopLeft, vTopRight, vBottomLeft, v2_UVRepeat, m_pSweetSpotTarget );
}


//----------------------------------------------------------------------------------------------------------------------
// Name: Sample::RenderPlayspaceFrustum
// Desc: Renders the playspace bounds.
//----------------------------------------------------------------------------------------------------------------------
void Sample::RenderPlayspaceFrustum()
{
    static const XMFLOAT2 v2_UVRepeat(1.0f, 1.0f);

    m_pd3dDevice->SetRenderState_Inline( D3DRS_ZENABLE, TRUE );
    m_pd3dDevice->SetRenderState_Inline( D3DRS_ZWRITEENABLE, FALSE );
    m_pd3dDevice->SetRenderState_Inline( D3DRS_DEPTHBIAS, 0 );
    m_pd3dDevice->SetRenderState_Inline( D3DRS_ALPHABLENDENABLE, TRUE );
    m_pd3dDevice->SetRenderState_Inline( D3DRS_ALPHATESTENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_SRCBLEND, D3DBLEND_SRCALPHA );
    m_pd3dDevice->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA );

    // Note: Not optimized; use a vertex buffer in production code.

    // We need to move the verts into avatar space, and untilt them.

    XMMATRIX matLevel = NuiTransformMatrixLevel( m_Skeleton.vNormalToGravity );

    // Adjust for sensor height (if we have it - otherwise assume 5.5ft off floor).
    XMVECTOR vTranslation = { 0.0f, 1.68f, 0.0f, 0.0f }; // Default Y translation is 1.68m

    if ( m_Skeleton.vFloorClipPlane.x != 0.0f && m_Skeleton.vFloorClipPlane.y != 0.0f
         && m_Skeleton.vFloorClipPlane.z != 0.0f && m_Skeleton.vFloorClipPlane.w != 0.0f )
    {
        XMVECTOR vFloorClipPlane = XMPlaneNormalize( m_Skeleton.vFloorClipPlane );
        XMVECTOR vSplatD = XMVectorSplatW( vFloorClipPlane );
        XMVECTOR vNegPlaneVecFromOrigin = vSplatD * vFloorClipPlane;
        vTranslation = XMVectorSetW( vNegPlaneVecFromOrigin, 0.0f );
    }

    XMMATRIX matTranslation = XMMatrixTranslationFromVector( vTranslation );

    XMMATRIX matFixup = matLevel * matTranslation;

    const XMVECTOR vZFlip = { 1.0f, 1.0f, -1.0f, 1.0f };

    for ( INT i = 0; i < DebugPlayspaceVerts::DBGPSFACE_COUNT; ++i )
    {
        XMVECTOR v0 = XMLoadFloat3( &m_debugPlayspace.avBoundingVertices[ m_debugPlayspace.aiFaceOrdering[ i ][ 0 ] ] );
        XMVECTOR v1 = XMLoadFloat3( &m_debugPlayspace.avBoundingVertices[ m_debugPlayspace.aiFaceOrdering[ i ][ 1 ] ] );
        XMVECTOR v2 = XMLoadFloat3( &m_debugPlayspace.avBoundingVertices[ m_debugPlayspace.aiFaceOrdering[ i ][ 2 ] ] );
        XMVECTOR v3 = XMLoadFloat3( &m_debugPlayspace.avBoundingVertices[ m_debugPlayspace.aiFaceOrdering[ i ][ 3 ] ] );

        XMFLOAT3 aTemp[4];

        XMStoreFloat3( &aTemp[0], XMVector3Transform( v0, matFixup ) * vZFlip );
        XMStoreFloat3( &aTemp[1], XMVector3Transform( v1, matFixup ) * vZFlip );
        XMStoreFloat3( &aTemp[2], XMVector3Transform( v2, matFixup ) * vZFlip );
        XMStoreFloat3( &aTemp[3], XMVector3Transform( v3, matFixup ) * vZFlip );
             
        ATG::DebugDraw::DrawQuad( aTemp[0], aTemp[1], aTemp[2], aTemp[3], m_colPlayspaceFrustum[ i ] );
    }
}


//----------------------------------------------------------------------------------------------------------------------
// Name: Sample::UpdateInput
// Desc: Updates the main game loop state based on the gamepad input.
//----------------------------------------------------------------------------------------------------------------------
void Sample::UpdateInput()
{
    // Get current gamepad state
    ATG::GAMEPAD* pGamepad = ATG::Input::GetMergedInput();


    // The Back button toggles the help screen.
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_BACK )
    {
        m_bDrawHelp = !m_bDrawHelp;
    }

    // Start shows the NUI Troubleshooter
    if ( pGamepad->wPressedButtons & XINPUT_GAMEPAD_START )
    {
        // Note: Ordinarily you'd want to check the return code here, but we don't care.
        DWORD result = XShowNuiTroubleshooterUI();

        if ( result == ERROR_FUNCTION_FAILED ) 
        {
            ATG::DebugSpew( "XShowNuiTroubleshooterUI Failed" );
        }

        // Note: ERROR_ACCESS_DENIED is unlikely in this scenario, as we bring up the troubleshooter from a controller
        //       button press, and the guide would have intercepted this.
    }

    if ( pGamepad->wPressedButtons & XINPUT_GAMEPAD_X )
    {
        m_bShowPlayspace = !m_bShowPlayspace;
    }

    BOOL fStillTilting = IsTilting();

    if ( !fStillTilting && pGamepad->wPressedButtons & XINPUT_GAMEPAD_LEFT_SHOULDER )
    {
        int i = (int)m_currentTiltMode - 1;
        if ( i < 0 )
        {
            i = TILT_MODE_COUNT - 1;
        }

        m_currentTiltMode = (TILTMODE) i;

        ChangeTiltMode();
    }
    else if ( !fStillTilting && pGamepad->wPressedButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER )
    {
        int i = (int)m_currentTiltMode + 1;

        if ( i >= TILT_MODE_COUNT )
        {
            i = 0;
        }

        m_currentTiltMode = (TILTMODE) i;

        ChangeTiltMode();
    }

}

//----------------------------------------------------------------------------------------------------------------------
// Name: Sample::ChangeTiltMode
// Desc: Tells the system to adjust the camera for the new playspace configuration.
//----------------------------------------------------------------------------------------------------------------------
void Sample::ChangeTiltMode()
{
    if ( IsTilting() )       // Should never be the case...
        return;

    FLOAT fHeadSpace = MapTiltModeToHeadSpace(m_currentTiltMode);
    DWORD dwResult = NuiCameraAdjustTilt( NUI_TILT_FLAGS_NONE, fHeadSpace, NUI_TILT_DEFAULT_FAR_SPACE_DISTANCE, 0.0f, &m_tiltObjects,
                                          &m_ovTiltComplete );
    if ( dwResult == ERROR_IO_PENDING )
    {
        ATG::DebugSpew( "Camera tilting to %s\n", g_pstrTiltModes[m_currentTiltMode] );
        m_tiltState = TILT_STATE_BUSY;
        return;
    }
    else if ( dwResult == ERROR_RETRY )
    {
        ATG::DebugSpew( "NuiCameraAdjustTilt called too soon after previous call\n" );
    }
    else if ( dwResult == ERROR_BUSY )
    {
        ATG::DebugSpew( "NuiCameraAdjustTilt failed because camera was busy\n" );
    }
    else if ( dwResult == ERROR_TOO_MANY_CMDS )
    {
        ATG::DebugSpew( "NuiCameraAdjustTilt failed to find player candidate; waiting\n" );
    }
    else if ( dwResult == ERROR_SUCCESS )
    {
        ATG::DebugSpew( "NuiCameraAdjustTilt completed immediately - camera tilt already optimal?\n" );
        m_tiltState = TILT_STATE_IDLE;
    }
    else
    {
        ATG::FatalError( "Unexpected result from NuiCameraAdjustTilt - %x\n", dwResult );
    }

    
}

//----------------------------------------------------------------------------------------------------------------------
// Name: Sample::UpdateTiltState
// Desc: Polls to find out if the camera has stopped tilting yet.
//----------------------------------------------------------------------------------------------------------------------
void Sample::UpdateTiltState()
{
    if ( m_tiltState == TILT_STATE_IDLE )
        return;

    if ( m_tiltState == TILT_STATE_NOINFO )
    {
        // First time through, just grab the tilt object info. 
        DWORD dwResult = NuiCameraAdjustTilt( NUI_TILT_FLAGS_NO_TILT, 0.0f, NUI_TILT_DEFAULT_FAR_SPACE_DISTANCE,
            NUI_TILT_DEFAULT_FAR_SPACE_DISTANCE, &m_tiltObjects, &m_ovTiltComplete );

        if ( dwResult == ERROR_IO_PENDING )
        {
            m_tiltState = TILT_STATE_BUSY;
        }
        else if ( dwResult == ERROR_SUCCESS )
        {
            m_tiltState = TILT_STATE_IDLE;
        }

        // Leave in TILT_STATE_NOINFO for all these errors:

        else if ( dwResult == ERROR_RETRY )
        {
            ATG::DebugSpew( "NuiCameraAdjustTilt called too soon after previous call\n" );
        }
        else if ( dwResult == ERROR_BUSY )
        {
            // On startup, the camera may be busy for a few seconds, so if we get ERROR_BUSY, we just eat the message
            // and try next frame.
        }
        else if ( dwResult == ERROR_TOO_MANY_CMDS )
        {
            ATG::DebugSpew( "NuiCameraAdjustTilt failed to find player candidate; waiting\n" );
        } 
        else
        {
            ATG::DebugSpew( "NuiCameraAdjustTilt failed with NO_TILT passed in; this shouldn't happen\n" );
        }

        return;
    }

    if ( XHasOverlappedIoCompleted(&m_ovTiltComplete) )
    {
        ATG::DebugSpew( "NuiCameraAdjustTilt - completed tilt adjustment\n" );

        m_tiltState = TILT_STATE_IDLE;
    }
}


//----------------------------------------------------------------------------------------------------------------------
// Name: Sample::CalcCameraElevationDegrees
// Desc: Calculates the tilt of the camera axis from the horizontal plane, in degrees.
//----------------------------------------------------------------------------------------------------------------------
FLOAT Sample::CalcCameraElevationDegrees()
{
    XMVECTOR& vNormToGrav = m_Skeleton.vNormalToGravity; // Note: Already normalized.

    FLOAT fDegrees = XMConvertToDegrees( atan2( vNormToGrav.z, vNormToGrav.y) );

    return fDegrees;
}

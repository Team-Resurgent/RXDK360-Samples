//--------------------------------------------------------------------------------------
// AvatarMonkeySeeMonkeyDo.cpp
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
#include <AtgNuiJointFilter.h>
#include <AtgNuiJointConverter.h>
#include <AtgAvatarRenderer.h>
#include <AtgNuiCommon.h>

//--------------------------------------------------------------------------------------
// Callouts for labeling the gamepad on the help screen
//--------------------------------------------------------------------------------------
ATG::HELP_CALLOUT g_HelpCallouts[] =
{
    { ATG::HELP_BACK_BUTTON,    ATG::HELP_PLACEMENT_1, L"Display help" },
    { ATG::HELP_LEFT_STICK,     ATG::HELP_PLACEMENT_1, L"Move Camera" },
    { ATG::HELP_RIGHT_STICK,    ATG::HELP_PLACEMENT_1, L"Rotate Camera" },
    { ATG::HELP_A_BUTTON,       ATG::HELP_PLACEMENT_1, L"Next Filter" },
    { ATG::HELP_B_BUTTON,       ATG::HELP_PLACEMENT_1, L"Previous Filter" },
    { ATG::HELP_X_BUTTON,       ATG::HELP_PLACEMENT_1, L"Toggle Tilt Correction" },
};

static const DWORD NUM_HELP_CALLOUTS = ARRAYSIZE( g_HelpCallouts );

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

enum JointFilterMask
{
    JointFilterMask_None                        = 0x00000000,	// Raw data
    JointFilterMask_Blend                       = 0x00000001,	// Simple multi-frame blend
    JointFilterMask_VelDamp                     = 0x00000002,	// Velocity dampening filter
    JointFilterMask_Combi                       = 0x00000004,	// multi-component filter
    JointFilterMask_Taylor                      = 0x00000008,	// Taylor series filter
    JointFilterMask_DoubleExponential           = 0x00000010,	// Double exponential smoothing filter
    JointFilterMask_AdaptiveDoubleExponential   = 0x00000020,	// Adaptive double exponential smoothing filter
};

//--------------------------------------------------------------------------------------
// Name: class Sample
// Desc: Main class to run this application.
//--------------------------------------------------------------------------------------
class Sample : public ATG::Application
{
public:
    Sample() : m_bDrawHelp( FALSE ),
               m_dwCurFrontBuffer(0),
               m_bTiltCorrection( FALSE ),
               m_pNuiJointConverter( NULL ),
               m_pAvatarRenderer( NULL )
    {
    }

    virtual ~Sample()
    {
        // Note that this code will never actually be executed, as quitting a sample
        // simply reboots the dev kit; shutting down the system is not necessary in this
        // case. If it is necessary to shut down the system, this is the correct way to
        // do so.
        if( m_pAvatarRenderer )
            delete m_pAvatarRenderer;
        if( m_pNuiJointConverter )
            delete m_pNuiJointConverter;

        // Shutdown XAvatar and release memory
        XAvatarShutdown();
        NuiSkeletonTrackingDisable();
        NuiShutdown( );
    }

private:
    virtual HRESULT Initialize();
    virtual HRESULT Update();
    HRESULT         UpdateSkeletonTracking();
    VOID            CreateRenderTargets();
    virtual HRESULT Render();
    VOID            RenderOverlays();

    HRESULT UpdateAnimation( FLOAT fDeltaTime );
    
    VOID ResetFilters();

private:
    ATG::Timer                  m_Timer;
    ATG::Font                   m_Font;
    ATG::Help                   m_Help;
    BOOL                        m_bDrawHelp;

    // View parameters
    FLOAT                       m_fLookPitch;
    FLOAT                       m_fLookYaw;
    XMVECTOR                    m_vEyePt;
    XMVECTOR                    m_vUp;
    XMMATRIX                    m_matView;
    XMMATRIX                    m_matProj; 
    XMMATRIX                    m_matWorld;

    // Animation data - updated every frame from the camera skeleton
    XAVATAR_SKELETON_POSE_JOINT m_AvatarJointPose[ XAVATAR_MAX_SKELETON_JOINTS ]; 

    // Rendering surfaces and textures
    D3DSurface*                 m_pBackBuffer;
    D3DSurface*                 m_pDepthBuffer;
    D3DTexture*                 m_pFrontBuffer[2];
    DWORD                       m_dwCurFrontBuffer;

    NUI_SKELETON_FRAME          m_Skeleton;
    UINT                        m_iCurrentSkeletonIndex;
	ATG::NuiJointConverter*     m_pNuiJointConverter;
    HRESULT                     m_hrNuiResult;


    ATG::AvatarRenderer*        m_pAvatarRenderer;

    JointFilterMask             m_JointFilterMask;

    BOOL                        m_bTiltCorrection;

    ATG::FilterDoubleExponential m_filterDoubleExponential;
    ATG::FilterAdaptiveDoubleExponential m_filterAdaptiveDoubleExponential;
    ATG::FilterVelDamp          m_filterVelDamp;
    ATG::FilterBlendJoint       m_filterBlendJoint;
    ATG::FilterTaylorSeries     m_filterTaylorSeries;
    ATG::FilterCombination      m_filterCombination;

    // Visualize the depth and color buffers
    CONST NUI_IMAGE_FRAME*      m_pImageFrame;
	CONST NUI_IMAGE_FRAME*      m_pDepthFrame;

	ATG::NuiVisualization       m_pip;
	HANDLE                      m_hImage;
	HANDLE                      m_hDepth;
	HANDLE                      m_hFrameEndEvent;
};


//-------------------------------------------------------------------------------------
// Name: main()
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
    MyAtgApp.m_d3dpp.PresentationInterval   = D3DPRESENT_INTERVAL_TWO;

    MyAtgApp.m_dwDeviceCreationFlags       |= D3DCREATE_BUFFER_2_FRAMES | D3DCREATE_CREATE_THREAD_ON_1;

    MyAtgApp.Run();
}


//--------------------------------------------------------------------------------------
// Name: Initialize()
// Desc: Creates all graphics resources and initializes rendering and animation systems.
//--------------------------------------------------------------------------------------
HRESULT Sample::Initialize()
{
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

	m_pNuiJointConverter = new ATG::NuiJointConverter();
    if( m_pNuiJointConverter == NULL )
    {
        return E_FAIL;
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

    m_pAvatarRenderer = new ATG::AvatarRenderer( m_pd3dDevice, metadata );

    // Initialize view parameters
    XVIDEO_MODE VideoMode;
    XGetVideoMode( &VideoMode );
    FLOAT fAspectRatio = VideoMode.fIsWideScreen == TRUE ? (16.0f / 9.0f) : (4.0f / 3.0f);
    m_matProj    = XMMatrixPerspectiveFovRH( XM_PI/4, fAspectRatio, 0.01f, 20.0f ); 
	static const XMVECTORF32 g_svEyePtr = {0.0f,0.2f,2.5f,0.0f};
	static const XMVECTORF32 g_svUp = {0.0f,1.0f,0.0f,0.0f};
    m_vEyePt     = g_svEyePtr;
    m_vUp        = g_svUp;
    m_fLookPitch = 0.0f;
    m_fLookYaw   = 0.0f;

    CreateRenderTargets();

    // Create the font
    if( FAILED( m_Font.Create( "d:\\Media\\Fonts\\Arial_16.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Confine text drawing to the title safe area
    m_Font.SetWindow( ATG::GetTitleSafeArea() );

    // Create the help
    if( FAILED( m_Help.Create( "d:\\Media\\Help\\Help.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    m_JointFilterMask = JointFilterMask_AdaptiveDoubleExponential;
    ATG::SimpleShaders::Initialize( NULL, NULL );

    // Initialize the Picture in Picture visualization
	if( FAILED( m_pip.Initialize( m_pd3dDevice, NUI_INITIALIZE_FLAG_USES_COLOR |
                                                NUI_INITIALIZE_FLAG_USES_DEPTH_AND_PLAYER_INDEX |
                                                NUI_INITIALIZE_FLAG_USES_SKELETON, 
		                          NUI_IMAGE_RESOLUTION_640x480 ) ) )
    {
        ATG_PrintError ( "Picture in Picture initialization failed" );
	}

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: CreateRenderTargets()
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
// Name: UpdateSkeletonTracking()
// Desc: Read the data from the camera stream synchronously each frame and pass the
//       depthmap on to the skeleton tracking. 
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
            for( UINT i = 0; i < NUI_SKELETON_COUNT; ++ i )
                if( m_Skeleton.SkeletonData[ i ].eTrackingState == NUI_SKELETON_TRACKED )
                {
                    m_iCurrentSkeletonIndex = i;
                    break;
                }
     
        m_pip.SetSkeletons( &m_Skeleton );
        if( m_bTiltCorrection )
        {
            // Apply tilt correction. First we get our Up vector.
            XMVECTOR vNormToGrav = m_Skeleton.vNormalToGravity;

            // In this release only, until final hardware with built in accelerometer ships,
            // we need to check for an invalid up vector (we will synthesize it from
            // the floor plane if that data is present). If we can't get an up
            // vector, we default to 0.0, 1.0, 0.0 instead.

            if ( fabs(vNormToGrav.x) < FLT_EPSILON &&
                 fabs(vNormToGrav.y) < FLT_EPSILON &&
                 fabs(vNormToGrav.z) < FLT_EPSILON )
            {
                static const XMVECTORF32 c_vUp = { 0.0f, 1.0f, 0.0f, 0.0f };
                vNormToGrav = c_vUp;
            }

            // Generate the leveling matrix and apply it to all points on any skeletons
            // which are currently being tracked. 

            XMMATRIX matLevel = NuiTransformMatrixLevel( vNormToGrav );
     
            for(int i = 0; i < NUI_SKELETON_POSITION_COUNT; ++i)
            {
                m_Skeleton.SkeletonData[m_iCurrentSkeletonIndex].SkeletonPositions[i] = 
                    XMVector3Transform(m_Skeleton.SkeletonData[m_iCurrentSkeletonIndex].SkeletonPositions[i], matLevel);
            }
        }
    }
    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Update()
// Desc: Called once per frame.  Entry point for animating the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Update()
{
    PIXBeginNamedEvent( 0, __FUNCTION__ );

    // Get elapsed time
    FLOAT fDeltaTime = ( FLOAT )m_Timer.GetElapsedTime();

    // Get current gamepad state
    ATG::GAMEPAD* pGamepad = ATG::Input::GetMergedInput();

    // The Back button toggles the help screen.
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_BACK )
        m_bDrawHelp = !m_bDrawHelp;

    // The A button goes to the next filter
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_A )
    {
        if( m_JointFilterMask == JointFilterMask_None )
            m_JointFilterMask = JointFilterMask_Blend;
        else if( m_JointFilterMask == JointFilterMask_Blend )
            m_JointFilterMask = JointFilterMask_VelDamp;
        else if( m_JointFilterMask == JointFilterMask_VelDamp )
            m_JointFilterMask = JointFilterMask_Combi;
        else if( m_JointFilterMask == JointFilterMask_Combi )
            m_JointFilterMask = JointFilterMask_Taylor;
        else if( m_JointFilterMask == JointFilterMask_Taylor )
            m_JointFilterMask = JointFilterMask_DoubleExponential;
        else if( m_JointFilterMask == JointFilterMask_DoubleExponential )
            m_JointFilterMask = JointFilterMask_AdaptiveDoubleExponential;
        else if( m_JointFilterMask == JointFilterMask_AdaptiveDoubleExponential )
            m_JointFilterMask = JointFilterMask_None;

        // Reset filter states when we switch between filters
        ResetFilters();
    }

    // The B button goes to the previous filter
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_B )
    {
        if( m_JointFilterMask == JointFilterMask_None )
            m_JointFilterMask = JointFilterMask_AdaptiveDoubleExponential;
        else if( m_JointFilterMask == JointFilterMask_AdaptiveDoubleExponential )
            m_JointFilterMask = JointFilterMask_DoubleExponential;
        else if( m_JointFilterMask == JointFilterMask_DoubleExponential )
            m_JointFilterMask = JointFilterMask_Taylor;
        else if( m_JointFilterMask == JointFilterMask_Taylor )
            m_JointFilterMask = JointFilterMask_Combi;
        else if( m_JointFilterMask == JointFilterMask_Combi )
            m_JointFilterMask = JointFilterMask_VelDamp;
        else if( m_JointFilterMask == JointFilterMask_VelDamp )
            m_JointFilterMask = JointFilterMask_Blend;
        else if( m_JointFilterMask == JointFilterMask_Blend )
            m_JointFilterMask = JointFilterMask_None;

        // Reset filter states when we switch between filters
        ResetFilters();
    }

    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_X )
        m_bTiltCorrection = !m_bTiltCorrection;

    // Rotate view
    m_fLookYaw     -= pGamepad->fX2 * fDeltaTime;
    m_fLookPitch   -= pGamepad->fY2 * fDeltaTime;
    m_fLookYaw     = fmodf( m_fLookYaw, XM_2PI );
    m_fLookPitch   = fmodf( m_fLookPitch, XM_2PI );

    XMMATRIX lookAtMatrix   = XMMatrixRotationRollPitchYaw( m_fLookPitch, m_fLookYaw, 0.0f );
	static const XMVECTORF32 g_vTransform = {0.0f,0.0f,-1.0f,1.0f};
    XMVECTOR m_vLookToZ     = XMVector3Transform(g_vTransform, lookAtMatrix);

    // Move viewing position
    m_vEyePt = XMVectorAdd(m_vEyePt, XMVectorScale(lookAtMatrix.r[0], fDeltaTime * pGamepad->fX1));
    m_vEyePt = XMVectorAdd(m_vEyePt, XMVectorScale(lookAtMatrix.r[2], fDeltaTime * -pGamepad->fY1));

    m_matView               = XMMatrixLookToRH( m_vEyePt, m_vLookToZ, m_vUp );

    // Get the skeleton
    HRESULT hrSkeleton = UpdateSkeletonTracking();
    
    if ( SUCCEEDED( hrSkeleton ) )
    {
        // Update the animation and avatar with the newly converted skinning data
        UpdateAnimation( fDeltaTime );
    }

    PIXEndNamedEvent();

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: UpdateAnimation()
// Desc: Updates the animation stream and blends between the previous and next frames.
//       Called once per frame.
//--------------------------------------------------------------------------------------
HRESULT Sample::UpdateAnimation( FLOAT fDeltaTime )
{
    if( m_pNuiJointConverter && SUCCEEDED( m_hrNuiResult ) && m_Skeleton.SkeletonData[m_iCurrentSkeletonIndex].eTrackingState == NUI_SKELETON_TRACKED )
    {
        XMVECTOR joints[NUI_SKELETON_POSITION_COUNT];
        XMVECTOR* filteredJoints;
        for(int i = 0; i < NUI_SKELETON_POSITION_COUNT; i++)
        {
            joints[i] = m_Skeleton.SkeletonData[m_iCurrentSkeletonIndex].SkeletonPositions[i];
        }

        switch ( m_JointFilterMask )
        {
        case JointFilterMask_Blend:
            m_filterBlendJoint.Update(joints);
            filteredJoints = m_filterBlendJoint.GetFilteredJoints();
            break;

        case JointFilterMask_DoubleExponential:
            m_filterDoubleExponential.Update(&m_Skeleton.SkeletonData[m_iCurrentSkeletonIndex]);
            filteredJoints = m_filterDoubleExponential.GetFilteredJoints();
            break;

        case JointFilterMask_AdaptiveDoubleExponential:
            m_filterAdaptiveDoubleExponential.Update(&m_Skeleton.SkeletonData[m_iCurrentSkeletonIndex], fDeltaTime);
            filteredJoints = m_filterAdaptiveDoubleExponential.GetFilteredJoints();
            break;

        case JointFilterMask_VelDamp:
            m_filterVelDamp.Update(joints);
            filteredJoints = m_filterVelDamp.GetFilteredJoints();
            break;

        case JointFilterMask_Taylor:
            m_filterTaylorSeries.Update(joints);
            filteredJoints = m_filterTaylorSeries.GetFilteredJoints();
            break;

        case JointFilterMask_Combi:
            m_filterCombination.Update(joints);
            filteredJoints = m_filterCombination.GetFilteredJoints();
            break;

        case JointFilterMask_None:
        default:
            filteredJoints = joints;
            break;
        }

        for(int i = 0; i < NUI_SKELETON_POSITION_COUNT; i++)
        {
            m_Skeleton.SkeletonData[m_iCurrentSkeletonIndex].SkeletonPositions[i] = filteredJoints[i];
        }

        m_pNuiJointConverter->ConvertNuiToAvatarSkeleton( &m_Skeleton.SkeletonData[m_iCurrentSkeletonIndex], m_AvatarJointPose );
        m_pAvatarRenderer->SetJoints( m_AvatarJointPose );
        m_pAvatarRenderer->Update();
        return S_OK;
    }
    else
    {
        // If not being tracked, make sure we reset the filter states
        if (m_Skeleton.SkeletonData[m_iCurrentSkeletonIndex].eTrackingState != NUI_SKELETON_TRACKED)
        {
            ResetFilters();
        }
        return E_FAIL;
    }
}


//--------------------------------------------------------------------------------------
// Name: Render()
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

        PIXBeginNamedEvent( 0xFFFFFFFF, "Avatar Render" );
        {
            
            m_matWorld = XMMatrixTranslation( 0.0f, 0.0f, 0.0f);
            m_pAvatarRenderer->Render( m_matWorld, m_matView, m_matProj );
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
// Name: RenderOverlays()
// Desc: Show title, timers, and help.
//--------------------------------------------------------------------------------------
VOID Sample::RenderOverlays()
{
    PIXBeginNamedEvent( 0, __FUNCTION__ );
    {
        if( m_bDrawHelp )
        {
            m_Help.Render( &m_Font, g_HelpCallouts, NUM_HELP_CALLOUTS );
        }
        else
        {
            m_Font.Begin();

            m_Font.SetScaleFactors( 1.2f, 1.2f );
            m_Font.DrawText( 0, 0, 0xffffffff, L"Avatar MonkeySeeMonkeyDo" );

            m_Font.SetScaleFactors( 1.0f, 1.0f );
            m_Font.DrawText( 0, 0, 0xffffff00, m_Timer.GetFrameRate(), ATGFONT_RIGHT );

            if( m_bTiltCorrection == FALSE )
                m_Font.DrawText( 0, 40, 0xffffffff, L"Tilt Correction off", ATGFONT_RIGHT );
            else
                m_Font.DrawText( 0, 40, 0xffffffff, L"Tilt Correction on", ATGFONT_RIGHT );

            switch ( m_JointFilterMask )
            {
            case JointFilterMask_None:
                m_Font.DrawText( 0, 60, 0xffffffff, L"Joint Filter Mask = None", ATGFONT_RIGHT );
                break;
            case JointFilterMask_Blend:
                m_Font.DrawText( 0, 60, 0xffffffff, L"Joint Filter Mask = Blend", ATGFONT_RIGHT );
                break;
            case JointFilterMask_VelDamp:
                m_Font.DrawText( 0, 60, 0xffffffff, L"Joint Filter Mask = Velocity Dampening", ATGFONT_RIGHT );
                break;
            case JointFilterMask_Combi:
                m_Font.DrawText( 0, 60, 0xffffffff, L"Joint Filter Mask = Combination", ATGFONT_RIGHT );
                break;
            case JointFilterMask_Taylor:
                m_Font.DrawText( 0, 60, 0xffffffff, L"Joint Filter Mask = Taylor Series", ATGFONT_RIGHT );
                break;
            case JointFilterMask_DoubleExponential:
                m_Font.DrawText( 0, 60, 0xffffffff, L"Joint Filter Mask = Double Exponential", ATGFONT_RIGHT );
                break;
            case JointFilterMask_AdaptiveDoubleExponential:
                m_Font.DrawText( 0, 60, 0xffffffff, L"Joint Filter Mask = Adaptive Double Exponential", ATGFONT_RIGHT );
                break;
            }


            m_Font.End();
        }
    }
    PIXEndNamedEvent();
}

//--------------------------------------------------------------------------------------
// Name: ResetFilters
// Desc: Reset any filters used in this sample
//--------------------------------------------------------------------------------------
VOID Sample::ResetFilters()
{
    m_filterBlendJoint.Reset();
    m_filterDoubleExponential.Reset();
    m_filterAdaptiveDoubleExponential.Reset();
    m_filterVelDamp.Reset();
    m_filterTaylorSeries.Reset();
    m_filterCombination.Reset();
}
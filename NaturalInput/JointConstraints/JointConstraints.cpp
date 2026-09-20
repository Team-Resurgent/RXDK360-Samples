//--------------------------------------------------------------------------------------
// JointConstraints.cpp
//
// This sample uses a simple model to constrain joint rotations to lie within a cone
// of directions.  Joints are checked to see if they violate the cone constraint, and then
// modified to lie within the cone.  
//
// Advanced Technology Group
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
#include <AtgNuiCommon.h>
#include <AtgNuiVisualization.h>
#include <AtgDebugDraw.h>

#include <AtgNuiJointConverter.h>
#include <AtgAvatarRenderer.h>

//--------------------------------------------------------------------------------------
// Callouts for labeling the gamepad on the help screen
//--------------------------------------------------------------------------------------
ATG::HELP_CALLOUT g_HelpCallouts[] =
{
    { ATG::HELP_BACK_BUTTON,    ATG::HELP_PLACEMENT_1, L"Display help" },
    { ATG::HELP_LEFT_STICK,     ATG::HELP_PLACEMENT_1, L"Move Camera" },
    { ATG::HELP_RIGHT_STICK,    ATG::HELP_PLACEMENT_1, L"Rotate Camera" },
};

static const DWORD NUM_HELP_CALLOUTS = ARRAYSIZE( g_HelpCallouts );

static const DWORD g_dwTileWidth   = 1280;
static const DWORD g_dwTileHeight  = 256;
static const DWORD g_dwFrameWidth  = 1280;
static const DWORD g_dwFrameHeight = 720;
static const D3DRECT g_tiles[3] = 
{
    {     0,                  0,  g_dwTileWidth, g_dwTileHeight     },
    {     0,     g_dwTileHeight,  g_dwTileWidth, g_dwTileHeight * 2 },
    {     0, g_dwTileHeight * 2,  g_dwTileWidth, g_dwFrameHeight    },
};


//--------------------------------------------------------------------------------------
// Name: class Sample
// Desc: Main class to run this application.
//--------------------------------------------------------------------------------------
class Sample : public ATG::Application
{
public:
    Sample() : m_bDrawHelp( FALSE ),
               m_pNuiJointConverter( NULL ),
               m_pNuiJointConverterConstrained( NULL ),
               m_pAvatarRenderer( NULL ),
               m_pAvatarRendererConstrained( NULL ),
               m_pAvatarRendererNuiMapper( NULL ),
               m_pAvatarRendererNuiMapperConstrained( NULL ),
               m_pAvatarNuiMapper( NULL ),
               m_pAvatarNuiMapperConstrained( NULL )
    {
    }

    virtual ~Sample()
    {
        SAFE_DELETE( m_pAvatarRenderer );
        SAFE_DELETE( m_pAvatarRendererConstrained );
        SAFE_DELETE( m_pAvatarRendererNuiMapper );
        SAFE_DELETE( m_pAvatarRendererNuiMapperConstrained );
        SAFE_DELETE( m_pNuiJointConverter );
        SAFE_DELETE( m_pNuiJointConverterConstrained );

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
    HRESULT         UpdateBasis( );

    HRESULT UpdateAnimation( FLOAT fDeltaTime );

private:
    ATG::Timer                  m_Timer;
    ATG::Font                   m_Font16;
    ATG::Font                   m_Font20;
    D3DTexture*                 m_pNuiMapperTitleTexture;
    D3DTexture*                 m_pConstrainedNuiMapperTitleTexture;
    D3DTexture*                 m_pSimpleRetargetTitleTexture;
    D3DTexture*                 m_pConstrainedSimpleRetargetTitleTexture;
    ATG::Help                   m_Help;
    BOOL                        m_bDrawHelp;

    // View parameters
    FLOAT                       m_fLookPitch;
    FLOAT                       m_fLookYaw;
    XMVECTOR                    m_vEyePt;
    XMVECTOR                    m_vUp;
    XMMATRIX                    m_matView;
    XMMATRIX                    m_matProj; 
    XMMATRIX                    m_matWorldNuiMapper;
    XMMATRIX                    m_matWorldNuiMapperConstrained;
    XMMATRIX                    m_matWorldNuiMapperTitle;
    XMMATRIX                    m_matWorldNuiMapperConstrainedTitle;

    // Animation data - updated every frame from the camera skeleton
    XAVATAR_SKELETON_POSE_JOINT m_AvatarJointPose[ XAVATAR_MAX_SKELETON_JOINTS ]; 
    XAVATAR_SKELETON_POSE_JOINT m_AvatarJointPoseConstrained[ XAVATAR_MAX_SKELETON_JOINTS ]; 
    XAVATAR_SKELETON_POSE_JOINT m_AvatarJointPoseNuiMapper[ XAVATAR_MAX_SKELETON_JOINTS ]; 
    XAVATAR_SKELETON_POSE_JOINT m_AvatarJointPoseNuiMapperConstrained[ XAVATAR_MAX_SKELETON_JOINTS ]; 

    // Rendering surfaces and textures
    D3DSurface*                 m_pBackBuffer;
    D3DSurface*                 m_pDepthBuffer;
    D3DTexture*                 m_pFrontBuffer;

    NUI_SKELETON_FRAME          m_Skeleton;
    NUI_SKELETON_FRAME          m_ConstrainedSkeleton;
    UINT                        m_iCurrentTrackingID;
    UINT                        m_iCurrentSkeletonIndex;
	ATG::NuiJointConverter*     m_pNuiJointConverter;                  // Joint converter for transforming NUI joints positions to rotations
    ATG::NuiJointConverter*     m_pNuiJointConverterConstrained;       // Joint converter that uses joint constraints
    HRESULT                     m_hrNuiResult;

    // Nui Mappers
    IXAvatarNuiMapper*          m_pAvatarNuiMapper;
    IXAvatarNuiMapper*          m_pAvatarNuiMapperConstrained;

    // Rendering for the Avatars that demonstrate joint constraints
    ATG::AvatarRenderer*        m_pAvatarRenderer;                      // Avatar for simple skeleton retargeting         
    ATG::AvatarRenderer*        m_pAvatarRendererConstrained;           // Constrained simple skeleton retargeting
    ATG::AvatarRenderer*        m_pAvatarRendererNuiMapper;             // Normal retargeting using IXAvatarNuiMapper
    ATG::AvatarRenderer*        m_pAvatarRendererNuiMapperConstrained;  // Constrained retargeting using IXAvatarNuiMapper

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
VOID __cdecl main()
{
    Sample MyAtgApp;
    ZeroMemory( &MyAtgApp.m_d3dpp, sizeof( MyAtgApp.m_d3dpp ) );

    MyAtgApp.m_d3dpp.BackBufferWidth        = g_dwFrameWidth;
    MyAtgApp.m_d3dpp.BackBufferHeight       = g_dwFrameHeight;
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
// Name: Initialize()
// Desc: Creates all graphics resources and initializes rendering and animation systems.
//--------------------------------------------------------------------------------------
HRESULT Sample::Initialize()
{
    m_iCurrentSkeletonIndex = 0;
    m_iCurrentTrackingID = 0;

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

    // create event which will be signaled when frame processing ends
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

    if( FAILED( hr ))
    {
        ATG::NuiPrintError( hr, "NuiInitialize" );
        return E_FAIL;
    }

    // register frame end event with NUI
    hr = NuiSetFrameEndEvent( m_hFrameEndEvent, 0 );
    if( FAILED(hr) )
    {
        ATG::NuiPrintError( hr, "NuiSetFrameEndEvent" );
        return E_FAIL;
    }

    // Open the color stream
	hr = NuiImageStreamOpen( NUI_IMAGE_TYPE_COLOR, NUI_IMAGE_RESOLUTION_640x480, 0, 1, NULL, &m_hImage );
    if( FAILED (hr) )
    {
        ATG::NuiPrintError( hr, "NuiImageStreamOpen" );
        return E_FAIL;
    }

    // Open the depth stream
	hr = NuiImageStreamOpen( NUI_IMAGE_TYPE_DEPTH_AND_PLAYER_INDEX, NUI_IMAGE_RESOLUTION_320x240, 0, 1, NULL, &m_hDepth );
    if( FAILED (hr) )
    {
        ATG::NuiPrintError( hr, "NuiImageStreamOpen" );
        return E_FAIL;
    }

    hr = NuiSkeletonTrackingEnable( NULL, 0 );
    if( FAILED( hr ))
    {
        ATG::NuiPrintError( hr, "NuiSkeletonTrackingEnable" );
    }

	m_pNuiJointConverter = new ATG::NuiJointConverter();
    if( m_pNuiJointConverter == NULL )
    {
        return E_FAIL;
    }

	m_pNuiJointConverterConstrained = new ATG::NuiJointConverter();
    if( m_pNuiJointConverterConstrained == NULL )
    {
        return E_FAIL;
    }

    m_pNuiJointConverterConstrained->AddDefaultConstraints();

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
    m_pAvatarRendererConstrained = new ATG::AvatarRenderer( m_pd3dDevice, metadata );
    m_pAvatarRendererNuiMapper = new ATG::AvatarRenderer( m_pd3dDevice, metadata );
    m_pAvatarRendererNuiMapperConstrained = new ATG::AvatarRenderer( m_pd3dDevice, metadata );

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
    if( FAILED( m_Font16.Create( "game:\\Media\\Fonts\\Arial_16.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    if( FAILED( m_Font20.Create( "game:\\Media\\Fonts\\Arial_20.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Confine text drawing to the title safe area
    m_Font16.SetWindow( ATG::GetTitleSafeArea() );
    m_Font20.SetWindow( ATG::GetTitleSafeArea() );

    // Load textures
    D3DXCreateTextureFromFile( m_pd3dDevice, "game:\\media\\textures\\NuiMapperTitle.dds", &m_pNuiMapperTitleTexture );
    D3DXCreateTextureFromFile( m_pd3dDevice, "game:\\media\\textures\\ConstrainedNuiMapperTitle.dds", &m_pConstrainedNuiMapperTitleTexture );
    D3DXCreateTextureFromFile( m_pd3dDevice, "game:\\media\\textures\\SimpleRetargetingTitle.dds", &m_pSimpleRetargetTitleTexture );
    D3DXCreateTextureFromFile( m_pd3dDevice, "game:\\media\\textures\\ConstrainedSimpleRetargetingTitle.dds", &m_pConstrainedSimpleRetargetTitleTexture );

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

    //Create the Avatar NUI Mapper
    SAFE_RELEASE( m_pAvatarNuiMapper );

    if( FAILED( hr = XAvatarCreateNuiMapper( m_pAvatarRendererNuiMapper->GetSkeleton(),
                        XAvatarMetadataGetBodyType( &metadata ), &m_pAvatarNuiMapper ) ) )
        ATG::FatalError( "Error %#X creating Nui Mapper\n", hr );

    XAVATAR_NUI_MAPPER_OPTIONS options = {0};
    m_pAvatarNuiMapper->GetOptions( &options );

    options.GeneralOptions.MirrorSkeleton = TRUE;
    options.GeneralOptions.PreventSelfIntersection = TRUE;
    options.GeneralOptions.RescaleToUserProportions = FALSE;
    options.GeneralOptions.SkeletonDataIsFiltered = FALSE;
    options.GeneralOptions.DisableJumping = FALSE;

    m_pAvatarNuiMapper->SetOptions( &options );

    SAFE_RELEASE( m_pAvatarNuiMapperConstrained );

    if( FAILED( hr = XAvatarCreateNuiMapper( m_pAvatarRendererNuiMapperConstrained->GetSkeleton(),
                        XAvatarMetadataGetBodyType( &metadata ), &m_pAvatarNuiMapperConstrained ) ) )
        ATG::FatalError( "Error %#X creating Nui Mapper\n", hr );

    m_pAvatarNuiMapperConstrained->SetOptions( &options );


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
        g_dwFrameWidth, g_dwFrameHeight, 1, 0, (D3DFORMAT) MAKESRGBFMT( D3DFMT_LE_X8R8G8B8 ), 0, &m_pFrontBuffer, NULL );
    
}


//--------------------------------------------------------------------------------------
// Name: UpdateSkeletonTracking()
// Desc: Read the data from the camera stream synchronously each frame and pass the
//       depthmap on to the skeleton tracking. 
//
//       The Nui API will wait up to NUI_CAMERA_TIMEOUT_DEFAULT ms for a new skeleton.
//--------------------------------------------------------------------------------------
HRESULT Sample::UpdateSkeletonTracking()
{
    // wait for frame processing to end
    if( WAIT_OBJECT_0 != WaitForSingleObject( m_hFrameEndEvent, NUI_FRAME_END_TIMEOUT_DEFAULT ) )
    {
        m_hrNuiResult = E_PENDING;
        return E_FAIL;
    }
    
    // Get data from the next image frame
    HRESULT hrImage = NuiImageStreamGetNextFrame( m_hImage, 0, &m_pImageFrame );
    // Get data from the next camera depth frame
    HRESULT hrDepth = NuiImageStreamGetNextFrame( m_hDepth, 0, &m_pDepthFrame );
    // Get the next skeleton frame
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
        // If the last tracking ID no longer exists in the skeleton data, then switch to
        // the first tracked skeleton we can find. If none is tracked, then leave the current 
        // tracking ID as is and try again next frame.
        for ( int i = NUI_SKELETON_COUNT - 1; i >= 0 ; --i )
        {
            if ( m_Skeleton.SkeletonData[ i ].eTrackingState == NUI_SKELETON_TRACKED )
            {
                m_iCurrentSkeletonIndex = i;

                if ( m_iCurrentTrackingID == m_Skeleton.SkeletonData[i].dwTrackingID )
                {
                    break;
                }
            }
        }

        m_iCurrentTrackingID = m_Skeleton.SkeletonData[ m_iCurrentSkeletonIndex ].dwTrackingID;

        m_pip.SetSkeletons( &m_Skeleton );
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

    // Rotate view
    m_fLookYaw     -= pGamepad->fX2 * fDeltaTime;
    m_fLookPitch   += pGamepad->fY2 * fDeltaTime;
    m_fLookYaw     = fmodf( m_fLookYaw, XM_2PI );
    m_fLookPitch   = fmodf( m_fLookPitch, XM_2PI );

    XMMATRIX lookAtMatrix   = XMMatrixRotationRollPitchYaw( m_fLookPitch, m_fLookYaw, 0.0f );
	static const XMVECTORF32 g_vTransform = {0.0f,0.0f,-1.0f,1.0f};
    XMVECTOR m_vLookToZ     = XMVector3Transform(g_vTransform, lookAtMatrix);

    // Move viewing position
    m_vEyePt = XMVectorAdd(m_vEyePt, XMVectorScale(lookAtMatrix.r[0], fDeltaTime * 3 * pGamepad->fX1));
    m_vEyePt = XMVectorAdd(m_vEyePt, XMVectorScale(lookAtMatrix.r[2], fDeltaTime * 3 * -pGamepad->fY1));

    m_matView               = XMMatrixLookToRH( m_vEyePt, m_vLookToZ, m_vUp );

    // Get the skeleton
    UpdateSkeletonTracking();
    
    // Update the animation and avatar with the newly converted skinning data
    UpdateAnimation( fDeltaTime );

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
    if( m_pNuiJointConverterConstrained && m_pNuiJointConverter && SUCCEEDED( m_hrNuiResult ) && m_Skeleton.SkeletonData[m_iCurrentSkeletonIndex].eTrackingState == NUI_SKELETON_TRACKED )
    {
        // Convert the skeleton to avatar rotations without using constraints
        m_pNuiJointConverter->ConvertNuiToAvatarSkeleton( &m_Skeleton.SkeletonData[m_iCurrentSkeletonIndex], m_AvatarJointPose );
        m_pAvatarRenderer->SetJoints( m_AvatarJointPose );
        m_pAvatarRenderer->Update();

        // Convert the skeleton to avatar rotations with constraints
        PIXBeginNamedEvent( 0, "Joint Constraints" );
        m_pNuiJointConverterConstrained->ConvertNuiToAvatarSkeleton( &m_Skeleton.SkeletonData[m_iCurrentSkeletonIndex], m_AvatarJointPoseConstrained );
        XMemCpy( &m_ConstrainedSkeleton, &m_Skeleton, sizeof( m_Skeleton));
        m_pNuiJointConverterConstrained->GetNuiSkeletonData( &m_ConstrainedSkeleton.SkeletonData[m_iCurrentSkeletonIndex] );
        PIXEndNamedEvent();

        m_pAvatarRendererConstrained->SetJoints( m_AvatarJointPoseConstrained);
        m_pAvatarRendererConstrained->Update();

    }

    // Convert the skeleton to avatar skinning data using the NuiMapper.  NUI joint positions are converted
    // to avatar joint rotations.
    if( m_pAvatarNuiMapper && m_pAvatarNuiMapperConstrained )
    {
        if( SUCCEEDED( m_hrNuiResult ) )
        {
            m_pAvatarNuiMapper->SetNuiSkeletonData( &m_Skeleton, m_iCurrentTrackingID );
            m_pAvatarNuiMapperConstrained->SetNuiSkeletonData( &m_ConstrainedSkeleton, m_iCurrentTrackingID );
        }

        m_pAvatarNuiMapper->Update( fDeltaTime );          
        m_pAvatarNuiMapperConstrained->Update( fDeltaTime );

        UpdateBasis( );     // Update the rotation of the avatar to match with the skeleton
    }

    // Set the joints on the avatar renderer
    assert( m_pAvatarRendererNuiMapper != NULL );
    assert( m_pAvatarNuiMapper != NULL );
    for ( DWORD j = 0; j < m_pAvatarRendererNuiMapper->GetSkeleton()->Count; ++j )
    {
        m_pAvatarNuiMapper->GetJointPose( j, &m_AvatarJointPoseNuiMapper[j], NULL );
    }
    m_pAvatarRendererNuiMapper->SetJoints( m_AvatarJointPoseNuiMapper );
    m_pAvatarRendererNuiMapper->Update();

    assert( m_pAvatarNuiMapperConstrained != NULL );
    for ( DWORD j = 0; j < m_pAvatarRendererNuiMapperConstrained->GetSkeleton()->Count; ++j )
    {
        m_pAvatarNuiMapperConstrained->GetJointPose( j, &m_AvatarJointPoseNuiMapperConstrained[j], NULL );
    }
    m_pAvatarRendererNuiMapperConstrained->SetJoints( m_AvatarJointPoseNuiMapperConstrained );
    m_pAvatarRendererNuiMapperConstrained->Update();

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: UpdateBasis()
// Desc: Rotate Avatar with user.
//--------------------------------------------------------------------------------------
HRESULT Sample::UpdateBasis( )
{
    XAVATAR_NUI_MAPPER_TRANSFORM_INFO info;

    assert( m_pAvatarNuiMapper );
    if ( SUCCEEDED( m_pAvatarNuiMapper->GetAvatarTransformInfo( &info ) ) )
    {
        XMMATRIX rotMtx = XMMatrixRotationY( info.YawInRadians );

        // Translate to the avatar's position, and offset to the side and down for sample visualization
        XMMATRIX transMtx = XMMatrixTranslation( info.Position.x, info.Position.y, info.Position.z ) * XMMatrixTranslation( 1.0f, -.8f, 0.0f);

        m_matWorldNuiMapper = rotMtx * transMtx;
        
        // Place the title over the avatar's head
        m_matWorldNuiMapperTitle = transMtx * XMMatrixTranslation( 0.0f, 2.0f, 0.0f);
    }

    assert( m_pAvatarNuiMapperConstrained );
    if ( SUCCEEDED( m_pAvatarNuiMapperConstrained->GetAvatarTransformInfo( &info ) ) )
    {
        XMMATRIX rotMtx = XMMatrixRotationY( info.YawInRadians );

        // Translate to the avatar's position, and offset to the side and down some for sample visualization
        XMMATRIX transMtx = XMMatrixTranslation( info.Position.x, info.Position.y, info.Position.z ) * XMMatrixTranslation( 2.0f, -0.8f, 0.0f);

        m_matWorldNuiMapperConstrained = rotMtx * transMtx ;

        // Place the title over the avatar's head
        m_matWorldNuiMapperConstrainedTitle = transMtx * XMMatrixTranslation( 0.0f, 2.0f, 0.0f);
    }

    return S_OK;
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
        static const D3DVECTOR4 clearColor = { 90/255.0f, 118/255.0f, 165/255.0f, 1.0f };
    
        m_pd3dDevice->SetRenderTarget( 0, m_pBackBuffer );
        m_pd3dDevice->SetDepthStencilSurface( m_pDepthBuffer );
        m_pd3dDevice->BeginTiling( 0, ARRAYSIZE(g_tiles), g_tiles, &clearColor, 1, 0 );

        if( m_Skeleton.SkeletonData[ m_iCurrentSkeletonIndex ].eTrackingState == NUI_SKELETON_TRACKED )
        {
            PIXBeginNamedEvent( 0, "Avatar Render" );
            {
                
                XMMATRIX matWorld = XMMatrixTranslation( -1.0f, 0.0f, 0.0f);
                m_pAvatarRenderer->Render( matWorld, m_matView, m_matProj );
            }
            PIXEndNamedEvent();

            PIXBeginNamedEvent( 0, "Avatar Render Constrained" );
            {
                
                XMMATRIX matWorld = XMMatrixTranslation( -2.0f, 0.0f, 0.0f);
                m_pAvatarRendererConstrained->Render( matWorld, m_matView, m_matProj );
            }
            PIXEndNamedEvent();

            PIXBeginNamedEvent( 0, "Avatar Render Nui Mapper" );
            {
                m_pAvatarRendererNuiMapper->Render( m_matWorldNuiMapper, m_matView, m_matProj );
            }
            PIXEndNamedEvent();

            PIXBeginNamedEvent( 0, "Avatar Render Nui Mapper Constrained" );
            {
                m_pAvatarRendererNuiMapperConstrained->Render( m_matWorldNuiMapperConstrained, m_matView, m_matProj );
            }
            PIXEndNamedEvent();

            // Draw skeleton
            PIXBeginNamedEvent( 0, "Skeleton Render" );
            {
				    XMMATRIX matWVP = m_matView * m_matProj;
				    m_pNuiJointConverterConstrained->DrawSkeleton( m_pd3dDevice, matWVP );
            }
            PIXEndNamedEvent();

            // Draw Avatar titles above their heads
            struct TexturedVert
            {
                XMFLOAT3 pos;
                XMFLOAT2 uv;
            };

            XGTEXTURE_DESC texDesc;

            // Draw the title for the NuiMapper avatar
            XGGetTextureDesc( m_pNuiMapperTitleTexture, 0, &texDesc ); 

            TexturedVert verts[4];
            verts[0].pos = XMFLOAT3(-.00070f * texDesc.Width, -.00070f * texDesc.Height, 0.0f);
            verts[1].pos = XMFLOAT3(.00070f * texDesc.Width, -.00070f * texDesc.Height, 0.0f);
            verts[2].pos = XMFLOAT3(.00070f * texDesc.Width, .00070f * texDesc.Height, 0.0f);
            verts[3].pos = XMFLOAT3(-.00070f * texDesc.Width, .00070f * texDesc.Height, 0.0f);
            verts[0].uv = XMFLOAT2(0.0f, 1.0f);
            verts[1].uv = XMFLOAT2(1.0f, 1.0f);
            verts[2].uv = XMFLOAT2(1.0f, 0.0f);
            verts[3].uv = XMFLOAT2(0.0f, 0.0f);

            // Rotate the title so that it faces the camera
            XMMATRIX matRotate = XMMatrixRotationRollPitchYaw( 0.0f, m_fLookYaw, 0.0f );
            XMMATRIX matWVP =  matRotate * m_matWorldNuiMapperTitle * m_matView * m_matProj;
            ATG::SimpleShaders::BeginShader_Transformed_Textured( matWVP, m_pNuiMapperTitleTexture );
            ATG::SimpleShaders::SetDeclPosTex();
            
            m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, TRUE );
            m_pd3dDevice->SetRenderState( D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
            m_pd3dDevice->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA );
            static float fMipMapLodBias = -1.0f;
            m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MIPMAPLODBIAS, *(DWORD*)(&fMipMapLodBias)); 
            m_pd3dDevice->DrawPrimitiveUP( D3DPT_QUADLIST, 1, verts, sizeof(TexturedVert) );
            ATG::SimpleShaders::EndShader();


            // Draw the title for the constrained NuiMapper avatar
            XGGetTextureDesc( m_pConstrainedNuiMapperTitleTexture, 0, &texDesc ); 

            verts[0].pos = XMFLOAT3(-.00070f * texDesc.Width, -.00070f * texDesc.Height, 0.0f);
            verts[1].pos = XMFLOAT3(.00070f * texDesc.Width, -.00070f * texDesc.Height, 0.0f);
            verts[2].pos = XMFLOAT3(.00070f * texDesc.Width, .00070f * texDesc.Height, 0.0f);
            verts[3].pos = XMFLOAT3(-.00070f * texDesc.Width, .00070f * texDesc.Height, 0.0f);
     
            matWVP =  matRotate * m_matWorldNuiMapperConstrainedTitle * m_matView * m_matProj;

            ATG::SimpleShaders::BeginShader_Transformed_Textured( matWVP, m_pConstrainedNuiMapperTitleTexture );
            ATG::SimpleShaders::SetDeclPosTex();

            m_pd3dDevice->DrawPrimitiveUP( D3DPT_QUADLIST, 1, verts, sizeof(TexturedVert) );
            ATG::SimpleShaders::EndShader();


            // Draw the title for the simple retargeting avatar
            XGGetTextureDesc( m_pSimpleRetargetTitleTexture, 0, &texDesc ); 

            verts[0].pos = XMFLOAT3(-.00070f * texDesc.Width, -.00070f * texDesc.Height, 0.0f);
            verts[1].pos = XMFLOAT3(.00070f * texDesc.Width, -.00070f * texDesc.Height, 0.0f);
            verts[2].pos = XMFLOAT3(.00070f * texDesc.Width, .00070f * texDesc.Height, 0.0f);
            verts[3].pos = XMFLOAT3(-.00070f * texDesc.Width, .00070f * texDesc.Height, 0.0f);
     
            XMVECTOR vHipCenterPos = m_Skeleton.SkeletonData[m_iCurrentSkeletonIndex].SkeletonPositions[NUI_SKELETON_POSITION_HIP_CENTER];
            
            // position the avatar to the left and up from center
            XMMATRIX matWorld = XMMatrixTranslation( -1.0f + vHipCenterPos.x, 1.0f + vHipCenterPos.y, - vHipCenterPos.z);
            matWVP =  matRotate * matWorld * m_matView * m_matProj;

            ATG::SimpleShaders::BeginShader_Transformed_Textured( matWVP, m_pSimpleRetargetTitleTexture );
            ATG::SimpleShaders::SetDeclPosTex();
            
            m_pd3dDevice->DrawPrimitiveUP( D3DPT_QUADLIST, 1, verts, sizeof(TexturedVert) );
            ATG::SimpleShaders::EndShader();

            // Draw the title for the constrained simple retargeting avatar
            XGGetTextureDesc( m_pConstrainedSimpleRetargetTitleTexture, 0, &texDesc ); 

            verts[0].pos = XMFLOAT3(-.00070f * texDesc.Width, -.00070f * texDesc.Height, 0.0f);
            verts[1].pos = XMFLOAT3(.00070f * texDesc.Width, -.00070f * texDesc.Height, 0.0f);
            verts[2].pos = XMFLOAT3(.00070f * texDesc.Width, .00070f * texDesc.Height, 0.0f);
            verts[3].pos = XMFLOAT3(-.00070f * texDesc.Width, .00070f * texDesc.Height, 0.0f);

            // position the avatar to the left and up from center
            matWorld = XMMatrixTranslation( -2.0f + vHipCenterPos.x, 1.0f + vHipCenterPos.y, - vHipCenterPos.z);
            matWVP =  matRotate * matWorld * m_matView * m_matProj;

            ATG::SimpleShaders::BeginShader_Transformed_Textured( matWVP, m_pConstrainedSimpleRetargetTitleTexture );
            ATG::SimpleShaders::SetDeclPosTex();

            m_pd3dDevice->DrawPrimitiveUP( D3DPT_QUADLIST, 1, verts, sizeof(TexturedVert) );
            ATG::SimpleShaders::EndShader();
        }

        // UI
        RenderOverlays();

        // Draw the raw depth and image map with skeleton overlaid as visualization.
        const FLOAT drawWidth = 150.0f;
        const FLOAT drawHeight = drawWidth * 3.0f / 4.0f;
        const FLOAT drawX = 50.0f;
        const FLOAT drawY = 720.0f - 30.0f - drawHeight;
        m_pip.BeginRender();
        m_pip.RenderColorStream( drawX, drawY, drawWidth, drawHeight );
        m_pip.RenderSkeletons( drawX, drawY, drawWidth, drawHeight, FALSE, TRUE );
        m_pip.RenderDepthStream( drawX + drawWidth + 10, drawY, drawWidth, drawHeight );
        m_pip.RenderSkeletons( drawX + drawWidth + 10, drawY, drawWidth, drawHeight, FALSE, FALSE );
        m_pip.EndRender();

        m_pd3dDevice->EndTiling( 0, NULL, m_pFrontBuffer, NULL, 1, 0, NULL );
        
        // Present the backbuffer contents to the display
        m_pd3dDevice->SynchronizeToPresentationInterval();
        m_pd3dDevice->Swap( m_pFrontBuffer, NULL );

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
            m_Help.Render( &m_Font16, g_HelpCallouts, NUM_HELP_CALLOUTS );
        }
        else
        {
            m_Font16.Begin();

            m_Font16.SetScaleFactors( 1.0f, 1.0f );
            m_Font16.DrawText( 0, 0, 0xffffff00, m_Timer.GetFrameRate(), ATGFONT_RIGHT );

            m_Font16.End();

            m_Font20.Begin();

            m_Font20.SetScaleFactors( 1.0f, 1.0f );
            m_Font20.DrawText( 0, 0, 0xffffffff, L"Joint Constraints" );

            m_Font20.End();
        }
    }

    PIXEndNamedEvent();
}

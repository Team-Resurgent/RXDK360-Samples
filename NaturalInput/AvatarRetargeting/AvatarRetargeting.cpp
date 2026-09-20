//--------------------------------------------------------------------------------------
// AvatarRetargeting.cpp
//
// This sample demonstrates the use of the NUI API to animate
// Avatars. The skeleton returned from the camera is mapped to an avatar skeleton.
// Joint positions returned from the camera are mapped to joint rotations for use in
// skinning the avatar. Head orientation is applied to the avatar.
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include <xtl.h>

#include <AtgApp.h>
#include <AtgAvatarRenderer.h>
#include <AtgDebugDraw.h>
#include <AtgFont.h>
#include <AtgHelp.h>
#include <AtgInput.h>
#include <AtgNuiCommon.h>
#include <AtgNuiVisualization.h>
#include <AtgSimpleShaders.h>
#include <AtgUtil.h>
#include <vector>
#include <xavatar.h>
#include <xgraphics.h>


//--------------------------------------------------------------------------------------
// Callouts for labeling the gamepad on the help screen
//--------------------------------------------------------------------------------------
ATG::HELP_CALLOUT g_HelpCallouts[] =
{
    { ATG::HELP_BACK_BUTTON, ATG::HELP_PLACEMENT_1, L"Display help" },
    { ATG::HELP_LEFT_STICK,  ATG::HELP_PLACEMENT_1, L"Move Camera" },
    { ATG::HELP_RIGHT_STICK, ATG::HELP_PLACEMENT_1, L"Rotate Camera" },
    { ATG::HELP_A_BUTTON,    ATG::HELP_PLACEMENT_1, L"Toggle head orientation" },
    { ATG::HELP_X_BUTTON,    ATG::HELP_PLACEMENT_1, L"Toggle intersection prevention" },
    { ATG::HELP_Y_BUTTON,    ATG::HELP_PLACEMENT_1, L"Toggle mirror mode" },
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


//--------------------------------------------------------------------------------------
// Name: QuaternionToPitchYawRoll
// Desc: Extracts pitch (rotation angle around the X axis), yaw (rotation angle around 
//       the Y axis) and roll (rotation around the Z axis) values from a quaternion. 
//       Values are returned in radians.
//--------------------------------------------------------------------------------------
inline void QuaternionToPitchYawRoll( XMVECTOR Q, FLOAT* pPitch, FLOAT* pYaw, FLOAT* pRoll )
{
    FLOAT x = XMVectorGetX( Q );
    FLOAT y = XMVectorGetY( Q );
    FLOAT z = XMVectorGetZ( Q );
    FLOAT w = XMVectorGetW( Q );

    *pPitch = atan2( 2* ( y*z + w*x ), w*w - x*x - y*y + z*z );
    *pYaw   = asin( 2* ( w*y - x*z ) );
    *pRoll  = atan2( 2* ( x*y + w*z ), w*w + x*x - y*y - z*z );
}


//--------------------------------------------------------------------------------------
// Name: QuaternionRotationPitchYawRoll
// Desc: Reconstruct a quaternion in pitch, yaw and roll angles.
//
// Note: XMQuaternionRotationRollPitchYaw uses the sequence roll, pitch then yaw to 
//       construct a quaternion while this function uses pitch, yaw, roll and can 
//       correctly reconstruct a quaternion from values extracted by 
//       QuaternionToPitchYawRoll.
//--------------------------------------------------------------------------------------
inline XMVECTOR QuaternionRotationPitchYawRoll( FLOAT fPitch, FLOAT fYaw, FLOAT fRoll )
{
    const XMVECTORF32 xAxis = { 1, 0, 0, 0 };
    const XMVECTORF32 yAxis = { 0, 1, 0, 0 };
    const XMVECTORF32 zAxis = { 0, 0, 1, 0 };

    XMVECTOR Q = XMQuaternionMultiply( XMQuaternionRotationNormal( xAxis, fPitch ),
                                       XMQuaternionRotationNormal( yAxis, fYaw )   );
    Q = XMQuaternionMultiply( Q, XMQuaternionRotationNormal( zAxis, fRoll ) );

    return Q;
}


//--------------------------------------------------------------------------------------
// Name: ConvertRotationFromNuiToAvatarSpace
// Desc: Converts the head orientation quaternion from NUI space coordinates to avatar
//       space coordinates.
//--------------------------------------------------------------------------------------
XMVECTOR ConvertRotationFromNuiToAvatarSpace( XMVECTOR vHeadOrientation, IXAvatarNuiMapper* pAvatarNuiMapper )
{
    assert( pAvatarNuiMapper != NULL );

    FLOAT fPitch;
    FLOAT fYaw;
    FLOAT fRoll;
    QuaternionToPitchYawRoll( vHeadOrientation, &fPitch, &fYaw, &fRoll );

    XAVATAR_NUI_MAPPER_OPTIONS AvatarOptions;
    pAvatarNuiMapper->GetOptions( &AvatarOptions );
    if( AvatarOptions.GeneralOptions.MirrorSkeleton )
    {
        return QuaternionRotationPitchYawRoll( fPitch * -1.0f, fYaw * -1.0f, fRoll );
    }
    else
    {
        return QuaternionRotationPitchYawRoll( fPitch * -1.0f, fYaw, fRoll * -1.0f );
    }
}


//--------------------------------------------------------------------------------------
// Name: ComputeShortestRotationPath
// Desc: Returns the shortest path between two quaternions.
//--------------------------------------------------------------------------------------
XMVECTOR ComputeShortestRotationPath( XMVECTOR Q1, XMVECTOR Q2 )
{
    XMVECTOR Q = XMQuaternionMultiply( XMQuaternionInverse( Q1 ), Q2 );
    if( XMVectorGetW( Q ) < 0.0f )
    {
        return XMVectorScale( Q, -1.0f );
    }
    else
        return Q;
}


//--------------------------------------------------------------------------------------
// Name: AvatarCreateListOfParentJoints
// Desc: Returns the list of all the parents (all the way down to the base joint) for 
//       any given avatar joint.
//--------------------------------------------------------------------------------------
std::vector< DWORD > AvatarCreateListOfParentJoints( const XAVATAR_SKELETON* pAvatarSkeleton, DWORD dwJointIndex )
{
    assert( dwJointIndex < XAVATAR_MAX_SKELETON_JOINTS );

    std::vector< DWORD > ParentJoints;

    DWORD dwCurrentJointIndex = dwJointIndex;
    while( dwCurrentJointIndex != BASE__Skeleton )
    {
        ParentJoints.push_back( dwCurrentJointIndex );

        dwCurrentJointIndex = pAvatarSkeleton->pJoints[ dwCurrentJointIndex ].Hierarchy.Parent;
    }

    ParentJoints.push_back( dwCurrentJointIndex );
    ParentJoints.shrink_to_fit();

    return ParentJoints;
}


//--------------------------------------------------------------------------------------
// Name: AvatarComputeJointRotation
// Desc: Walks a series of parent / child joints from the base of the avatar, computing 
//        the absolute rotation value for the last child joint in the list. The list 
//        should be ordered from the last child (target joint) in the series to base 
//        joint.
//--------------------------------------------------------------------------------------
XMVECTOR AvatarComputeJointRotation( IXAvatarNuiMapper *pAvatarNuiMapper, std::vector< DWORD > const& ParentJoints )
{
    XAVATAR_NUI_MAPPER_TRANSFORM_INFO AvatarTransformInfo;
    pAvatarNuiMapper->GetAvatarTransformInfo( &AvatarTransformInfo );

    XAVATAR_NUI_MAPPER_OPTIONS AvatarOptions;
    pAvatarNuiMapper->GetOptions( &AvatarOptions );

    XMVECTOR AbsoluteRotation =  XMQuaternionRotationRollPitchYaw( 0.0f, AvatarTransformInfo.YawInRadians, 0.0f );
    for( std::vector< DWORD >::const_reverse_iterator JointIndex = ParentJoints.rbegin(); JointIndex != ParentJoints.rend(); ++ JointIndex )
    {
        XAVATAR_SKELETON_POSE_JOINT JointPose;
        pAvatarNuiMapper->GetJointPose( *JointIndex, &JointPose, NULL );

        AbsoluteRotation = XMQuaternionMultiply( AbsoluteRotation, JointPose.Rotation );
    }

    return AbsoluteRotation;
}


//--------------------------------------------------------------------------------------
// Name: class Sample
// Desc: Main class to run this application.
//--------------------------------------------------------------------------------------
class Sample : public ATG::Application
{
public:
    Sample()
    :m_bDrawHelp( FALSE ),
     m_dwCurFrontBuffer(0),
     m_fCurTime(0.0f),
     m_HeadOrientationQualityFlags( 0 ),
     m_pAvatarRenderer( NULL ),
     m_pAvatarNuiMapper( NULL ),
     m_bUseHeadOrientation( TRUE ),
     m_bPreventIntersections( FALSE ),
     m_bMirrorAvatar( TRUE ),
     m_iCurrentTrackingID( 0 ),
     m_bTracked( FALSE )
    {}

    virtual ~Sample()
    {
        // Note that this code will never actually be executed, as quitting a sample
        // simply reboots the dev kit; shutting down the system is not necessary in this
        // case. If it is necessary to shut down the system, this is the correct way to
        // do so.
        SAFE_DELETE( m_pAvatarRenderer );

        // Shutdown XAvatar and release memory
        XAvatarShutdown();

        NuiHeadOrientationDisable();
        NuiSkeletonTrackingDisable();

        NuiShutdown( );
    }

private:
    virtual HRESULT Initialize();
    VOID            CreateRenderTargets();
    void            SetAvatarMapperOptions();

    virtual HRESULT Update();
    HRESULT         UpdateSkeletonTracking();
    XMVECTOR        CompensateHeadOrientaionForCameraTilt();
    XMVECTOR        CompensateHeadOrientationForPlayerLocation( BOOL bTiltCorrect );
    HRESULT         UpdateBasis( );
    HRESULT         UpdateAnimation( FLOAT fDeltaTime );
    HRESULT         ApplyHeadRotationToAvatar();

    virtual HRESULT Render();
    HRESULT         RenderBackground();
    VOID            RenderOverlays();
    HRESULT         RenderHeadOrientationHUD( XMVECTOR vRotation );
    HRESULT         ApplyRollToHeadOrientationHUD( XMFLOAT2* Points, DWORD dwCount, XMFLOAT2 Center, FLOAT fRoll );
    VOID            RenderHeadOrientationQualityMessage();

private:
    ATG::Timer                  m_Timer;
    ATG::Font                   m_Font;
    ATG::Help                   m_Help;
    BOOL                        m_bDrawHelp;
    FLOAT                       m_fCurTime;

    // View parameters
    FLOAT                       m_fLookPitch;
    FLOAT                       m_fLookYaw;
    XMVECTOR                    m_vEyePt;
    XMVECTOR                    m_vUp;
    XMMATRIX                    m_matView;
    XMMATRIX                    m_matProj; 
    XMMATRIX                    m_matWorld;
    XMMATRIX                    m_matWorldAligned;

    // Rendering surfaces and textures
    D3DSurface*                 m_pBackBuffer;
    D3DSurface*                 m_pDepthBuffer;
    D3DTexture*                 m_pFrontBuffer[2];
    DWORD                       m_dwCurFrontBuffer;


    LPDIRECT3DTEXTURE9              m_pTextureGrass;
    LPDIRECT3DTEXTURE9              m_pTextureSky;
    LPDIRECT3DVERTEXDECLARATION9    m_pBackgroundVertexDeclaration;
    LPDIRECT3DVERTEXSHADER9         m_pBackgroundVertexShader;
    LPDIRECT3DPIXELSHADER9          m_pBackgroundPixelShader;
    ATG::PackedResource             m_Resource;

    // Animation data - updated every frame from the NUI skeleton
    XAVATAR_SKELETON_POSE_JOINT m_AvatarJointPose[ XAVATAR_MAX_SKELETON_JOINTS ]; 

    HANDLE                      m_hImage;
    HANDLE                      m_hDepth;

    CONST NUI_IMAGE_FRAME*      m_pImageFrame;
    CONST NUI_IMAGE_FRAME*      m_pDepthFrame;

    NUI_SKELETON_FRAME          m_Skeleton;
    DWORD                       m_iCurrentTrackingID;
    BOOL                        m_bTracked;
    HANDLE                      m_hFrameEndEvent;

    HRESULT                     m_hrNuiResult;
    DWORD                       m_HeadOrientationQualityFlags;

    IXAvatarNuiMapper*          m_pAvatarNuiMapper;
    ATG::AvatarRenderer*        m_pAvatarRenderer;
    BOOL                        m_bUseHeadOrientation;
    BOOL                        m_bPreventIntersections;
    BOOL                        m_bMirrorAvatar;

    std::vector< DWORD >        m_ParentJointsFromHead;
    XMVECTOR                    m_HeadOrientationRaw;
    XMVECTOR                    m_HeadOrientation;

    ATG::NuiVisualization       m_pip;
};


//-------------------------------------------------------------------------------------
// Name: main()
// Desc: The application's entry point
//-------------------------------------------------------------------------------------
VOID __cdecl main()
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
    HRESULT hr;

    // Create the font
    if( FAILED( m_Font.Create( "d:\\Media\\Fonts\\Arial_16.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Create the help
    if( FAILED( m_Help.Create( "d:\\Media\\Help\\Help.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

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

    hr = NuiInitialize( NUI_INITIALIZE_FLAG_USES_SKELETON |
                        NUI_INITIALIZE_FLAG_USES_COLOR |
                        NUI_INITIALIZE_FLAG_USES_HEAD_ORIENTATION |      // Request head orientation support
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
        return E_FAIL;
    }

    hr = NuiHeadOrientationEnable( NULL, 0 );
    if ( FAILED( hr ))
    {
        ATG::NuiPrintError( hr, "NuiHeadOrientationEnable" );
        return E_FAIL;
    }

    // Obtain the avatar metadata for the user.  If the call fails, (the user doesn't
    // have an avatar associated with their profile yet or no one is 
    // signed in) then just load a random avatar.
    XAVATAR_METADATA metadata;
    if( ERROR_SUCCESS != XAvatarGetMetadataLocalUser( 0, &metadata, NULL ) )
    {
        if( ERROR_SUCCESS != XAvatarGetMetadataRandom( XAVATAR_BODY_TYPE_ALL, 1, &metadata, NULL ) )
        {
            return E_FAIL;
        }
    }

    m_pAvatarRenderer = new ATG::AvatarRenderer( m_pd3dDevice, metadata );

    m_ParentJointsFromHead = AvatarCreateListOfParentJoints( m_pAvatarRenderer->GetSkeleton(), HEAD__Skeleton );

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
    m_matProj          = XMMatrixPerspectiveFovRH( XM_PI/4, fAspectRatio, 0.01f, 20.0f ); 
    static const XMVECTORF32 g_svEyePt = { 0.0f, 1.0f, 2.0f, 0.0f };
    static const XMVECTORF32 g_svUp    = { 0.0f, 1.0f, 0.0f, 0.0f };
    m_vEyePt     = g_svEyePt;
    m_vUp        = g_svUp;
    m_fLookPitch = 0.0f;
    m_fLookYaw   = 0.0f;

    CreateRenderTargets();

    // Confine text drawing to the title safe area
    m_Font.SetWindow( ATG::GetTitleSafeArea() );

    ATG::SimpleShaders::Initialize( NULL, NULL );

    // Initialize the Picture in Picture visualization
    if( FAILED( m_pip.Initialize( m_pd3dDevice, 
                                  NUI_INITIALIZE_FLAG_USES_COLOR |
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

    //Create the Avatar NUI Mapper
    SAFE_RELEASE( m_pAvatarNuiMapper );

    if( FAILED( hr = XAvatarCreateNuiMapper( m_pAvatarRenderer->GetSkeleton(),
                        XAvatarMetadataGetBodyType( &metadata ), &m_pAvatarNuiMapper ) ) )
        ATG::FatalError( "Error %#X creating Nui Mapper\n", hr );

    SetAvatarMapperOptions();

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
// Name: SetAvatarMapperOptions()
// Desc: Sets options used by the avatar mapper to reflect the sample's settings.
//--------------------------------------------------------------------------------------
void Sample::SetAvatarMapperOptions()
{
    XAVATAR_NUI_MAPPER_OPTIONS options = { 0 };
    m_pAvatarNuiMapper->GetOptions( &options );

    options.GeneralOptions.MirrorSkeleton           = m_bMirrorAvatar;
    options.GeneralOptions.PreventSelfIntersection  = m_bPreventIntersections;
    options.GeneralOptions.RescaleToUserProportions = TRUE;
    options.GeneralOptions.SkeletonDataIsFiltered   = FALSE;
    options.GeneralOptions.DisableJumping           = FALSE;
    m_pAvatarNuiMapper->SetOptions( &options );
}


//--------------------------------------------------------------------------------------
// Name: Update()
// Desc: Called once per frame.  Entry point for animating the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Update()
{

    PIXBeginNamedEvent( 0, __FUNCTION__ );

    // Get elapsed time - capped at 33ms so animation doeswn't break during during debug session.
    FLOAT fDeltaTime = min( ( FLOAT )m_Timer.GetElapsedTime(), 0.033f );

    // Get current gamepad state
    ATG::GAMEPAD* pGamepad = ATG::Input::GetMergedInput();

    // The Back button toggles the help screen.
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_BACK )
    {
        m_bDrawHelp = !m_bDrawHelp;
    }

    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_A )
    {
        m_bUseHeadOrientation = !m_bUseHeadOrientation;
    }
    
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_X )
    {
        m_bPreventIntersections = !m_bPreventIntersections;
        SetAvatarMapperOptions();
    }

    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_Y )
    {
        m_bMirrorAvatar = !m_bMirrorAvatar;
        SetAvatarMapperOptions();
    }

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

    m_matView = XMMatrixLookToRH( m_vEyePt, m_vLookToZ, m_vUp );

    // Get the skeleton data
    HRESULT hrSkeleton = UpdateSkeletonTracking();

    if ( m_pAvatarNuiMapper )
    {
        // if we successfully retrieved skeleton data, set the data. At most this
        // will be set at 30Hz
        if ( SUCCEEDED( hrSkeleton ) )
        {
            m_pAvatarNuiMapper->SetNuiSkeletonData( &m_Skeleton, m_iCurrentTrackingID );
        }

        // Convert the skeleton to avatar skinning data.  NUI joint positions are converted
        // to avatar joint rotations. For this sample this update should run at 60Hz since
        // it uses PRESENT_INTERVAL_ONE and the m_pAvatarNuiMapper extrapolates data
        m_pAvatarNuiMapper->Update( fDeltaTime );
        UpdateBasis();     // Update the rotation of the avatar to match with the skeleton
    }

    // Update the animation and avatar with the newly converted skinning data
    UpdateAnimation( fDeltaTime );

    PIXEndNamedEvent();

    return S_OK;
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
        m_pip.SetColorTexture( m_pImageFrame->pFrameTexture, &m_pImageFrame->ViewArea );
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
        DWORD trackingId = 0;

        for ( int i = NUI_SKELETON_COUNT - 1; i >= 0 ; --i )
        {
            if ( m_Skeleton.SkeletonData[ i ].eTrackingState == NUI_SKELETON_TRACKED )
            {
                if ( m_iCurrentTrackingID == m_Skeleton.SkeletonData[i].dwTrackingID )
                {
                    trackingId = m_iCurrentTrackingID;
                    break;
                }
                else 
                {
                    trackingId = m_Skeleton.SkeletonData[ i ].dwTrackingID;
                }
            }
        }

        m_iCurrentTrackingID = trackingId;
        m_bTracked = ( trackingId != 0 );

        m_pip.SetSkeletons( &m_Skeleton );

        // Process head orientaion data
        if( m_bTracked && m_bUseHeadOrientation )
        {
            NUI_HEAD_ORIENTATION_FRAME HeadOrientationFrame;
            HRESULT hr = NuiHeadOrientationGetNextFrame( 0, &HeadOrientationFrame );
            if( SUCCEEDED( hr ) )
            {
                // Smooth the head orientation data using the default settings.
                NuiHeadOrientationSmooth( &HeadOrientationFrame, NULL );

                // Process only the head orientation data corresponding to the skeleton currently tracked
                for( DWORD i = 0; i < NUI_HEAD_ORIENTATION_COUNT; ++ i )
                {
                    if( HeadOrientationFrame.HeadOrientationData[ i ].TrackingID == m_iCurrentTrackingID )
                    {
                        m_HeadOrientationQualityFlags = HeadOrientationFrame.HeadOrientationData[ i ].QualityFlags;
                        if( !HeadOrientationFrame.HeadOrientationData[ i ].QualityFlags )
                        {
                            // Get the data. We keep the raw data so it can be displayed in the HUD later.
                            m_HeadOrientationRaw = HeadOrientationFrame.HeadOrientationData[ i ].Orientation;

                            // Correct for sensor titlt and the player's position.
                            XMVECTOR vHeadOrientationNuiSpace = XMQuaternionMultiply( m_HeadOrientationRaw, CompensateHeadOrientaionForCameraTilt() );
                            vHeadOrientationNuiSpace = XMQuaternionMultiply( vHeadOrientationNuiSpace, CompensateHeadOrientationForPlayerLocation( TRUE ) );

                            // Bring the head rotation into the avatar space coordinate.
                            m_HeadOrientation = ConvertRotationFromNuiToAvatarSpace( vHeadOrientationNuiSpace, m_pAvatarNuiMapper );
                        }

                        break;
                    }
                }
            }
        }

    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: CompensateHeadOrientaionForCameraTilt()
// Desc: Correct the raw head orientation data with the camera titlt angle
//--------------------------------------------------------------------------------------
XMVECTOR Sample::CompensateHeadOrientaionForCameraTilt()
{
    LONG  lAngleInDegrees;
    DWORD dwMovingFlags;

    NuiCameraElevationGetAngle( &lAngleInDegrees, &dwMovingFlags );

    return XMQuaternionRotationAxis( XMVectorSet( 1.0f, 0.0f, 0.0f, 0.0f ), -1.0f * lAngleInDegrees * XM_PI / 180 );
}


//--------------------------------------------------------------------------------------
// Name: CompensateHeadOrientationForPlayerLocation()
// Desc: Correct the raw head orientation data to account for player's position relative 
//       to the center of the sensor lens.
//--------------------------------------------------------------------------------------
XMVECTOR Sample::CompensateHeadOrientationForPlayerLocation( BOOL bTiltCorrect )
{
    assert( m_bTracked );

    DWORD dwTrackedSkeletonIndex = 0;
    for( ; dwTrackedSkeletonIndex < NUI_SKELETON_COUNT; ++ dwTrackedSkeletonIndex )
    {
        if( m_Skeleton.SkeletonData[ dwTrackedSkeletonIndex ].dwTrackingID == m_iCurrentTrackingID )
        {
            break;
        }
    }
    assert( dwTrackedSkeletonIndex < NUI_SKELETON_COUNT );

    XMVECTOR vHeadLocation = m_Skeleton.SkeletonData[ dwTrackedSkeletonIndex ].SkeletonPositions[ NUI_SKELETON_POSITION_HEAD ];

    XMVECTOR vCenter = XMVectorSet( 0.0f, 0.0f, vHeadLocation.z, 1.0f );
    XMVECTOR vPlayer = XMVectorSet( vHeadLocation.x, vHeadLocation.y, vHeadLocation.z, 1.0f );

    if( bTiltCorrect )
    {
         XMMATRIX m = NuiTransformMatrixLevel( m_Skeleton.vNormalToGravity );

        vCenter = XMVector3Transform( vCenter, m );
        vPlayer = XMVector3Transform( vPlayer, m );
    }

    FLOAT fAngle = XMVectorGetX( XMVector3AngleBetweenVectors( vCenter, vPlayer ) );
    XMVECTOR vNormal = XMVector3Cross( vCenter, vPlayer );

    return XMQuaternionRotationAxis( vNormal, fAngle );
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
        FLOAT fYaw =  info.YawInRadians;
        XMMATRIX rotMtx = XMMatrixRotationY( fYaw );
        XMMATRIX transMtx = XMMatrixTranslation( info.Position.x, info.Position.y, info.Position.z );

        m_matWorld = rotMtx * transMtx;
        m_matWorldAligned = transMtx;
    }

    return S_OK;
}

//--------------------------------------------------------------------------------------
// Name: UpdateAnimation()
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
            m_pAvatarNuiMapper->GetJointPose( j, &m_AvatarJointPose[ j ], NULL );
        }

        if( m_bUseHeadOrientation )
        {
            ApplyHeadRotationToAvatar();
        }

        m_pAvatarRenderer->SetJoints( m_AvatarJointPose );
        m_pAvatarRenderer->Update();
        return S_OK;
    }
    else
    {
        return E_FAIL;
    }
}

//--------------------------------------------------------------------------------------
// Name: ApplyHeadRotationToAvatar()
// Desc: Applies the head rotation to the avatar. The patch is made to the renderer, 
//       overriding the head orientation infered by the avatar mapper.
//--------------------------------------------------------------------------------------
HRESULT Sample::ApplyHeadRotationToAvatar()
{
    XMVECTOR Q1 = AvatarComputeJointRotation( m_pAvatarNuiMapper, m_ParentJointsFromHead );

    XMVECTOR Path = ComputeShortestRotationPath( Q1, m_HeadOrientation );

    m_AvatarJointPose[ HEAD__Skeleton ].Rotation = XMQuaternionMultiply( m_AvatarJointPose[ HEAD__Skeleton ].Rotation, Path );

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
        static const D3DVECTOR4 clearColor = { 0.75f, 0.78f, 0.86f, 1.0f };
        m_pd3dDevice->SetRenderTarget( 0, m_pBackBuffer );
        m_pd3dDevice->SetDepthStencilSurface( m_pDepthBuffer );
        m_pd3dDevice->BeginTiling( 0, ARRAYSIZE(g_tiles), g_tiles, &clearColor, 1, 0 );

        RenderBackground();

        PIXBeginNamedEvent( 0xFFFFFFFF, "Avatar Render" );
        {
            if( m_bTracked )
            {
                m_pAvatarRenderer->Render( m_matWorld, m_matView, m_matProj );
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
        m_pip.RenderSkeletons( drawX, drawY, drawWidth, drawHeight, TRUE, TRUE );
        m_pip.RenderDepthStream( drawX + drawWidth + 10, drawY, drawWidth, drawHeight );
        m_pip.RenderSkeletons( drawX + drawWidth + 10, drawY, drawWidth, drawHeight, TRUE, FALSE );
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
// Name: RenderBackground()
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
            m_Font.DrawText( 0, 0, 0xffffffff, L"Avatar Retargeting" );

            m_Font.SetScaleFactors( 1.0f, 1.0f );
            m_Font.DrawText( 0, 0, 0xffffff00, m_Timer.GetFrameRate(), ATGFONT_RIGHT );

            m_Font.DrawText( 0, 40, 0xffffffff, L"Intersection Prevention:", ATGFONT_RIGHT );
            m_Font.DrawText( 0, 60, 0xffffffff, m_bPreventIntersections ? L"ON" : L"OFF", ATGFONT_RIGHT );

            m_Font.End();

            if( m_bUseHeadOrientation )
            {
                RenderHeadOrientationHUD( m_HeadOrientationRaw );

                RenderHeadOrientationQualityMessage();
            }
        }
    }
    PIXEndNamedEvent();
}


//--------------------------------------------------------------------------------------
// Name: RenderHeadOrientationHUD()
// Desc: Displays a 2D crosshar representing the player's head orientation.
//--------------------------------------------------------------------------------------
HRESULT Sample::RenderHeadOrientationHUD( XMVECTOR vRotation )
{
    const FLOAT fTop    = 150.0f;
    const FLOAT fLeft   = 200.0f;
    const FLOAT fHeight = 100.0f;
    const FLOAT fWidth  = 100.0f;

    const FLOAT fBottom = fTop + fHeight;
    const FLOAT fRight  = fLeft + fWidth;

    FLOAT fPitch;
    FLOAT fYaw;
    FLOAT fRoll;
    QuaternionToPitchYawRoll( vRotation, &fPitch, &fYaw, &fRoll );

    FLOAT Yaw   = fLeft + fWidth * 0.5f + fYaw * fWidth  * 0.5f * -1.0f;
    FLOAT Pitch = fTop + fHeight * 0.5f + fPitch * fHeight * 0.5f * -1.0f;

    XMFLOAT2 Points[ 8 ];
    Points[ 0 ] = XMFLOAT2( Yaw,    fTop    );
    Points[ 1 ] = XMFLOAT2( Yaw,    fBottom );
    Points[ 2 ] = XMFLOAT2( fLeft,  Pitch   );
    Points[ 3 ] = XMFLOAT2( fRight, Pitch   );
    Points[ 4 ] = XMFLOAT2( fLeft,  fTop    );
    Points[ 5 ] = XMFLOAT2( fRight, fTop    );
    Points[ 6 ] = XMFLOAT2( fRight, fBottom );
    Points[ 7 ] = XMFLOAT2( fLeft,  fBottom );

    ApplyRollToHeadOrientationHUD( Points, 8, XMFLOAT2( fWidth * 0.5f + fLeft, fHeight * 0.5f + fTop ), fRoll );

    ATG::DebugDraw::DrawScreenSpaceLine( Points[ 0 ], Points[ 1 ], D3DCOLOR_ARGB( 0xff, 0xff, 0xff, 0xff ), 3 );
    ATG::DebugDraw::DrawScreenSpaceLine( Points[ 2 ], Points[ 3 ], D3DCOLOR_ARGB( 0xff, 0xff, 0xff, 0xff ), 3 );

    ATG::DebugDraw::DrawScreenSpaceLine( Points[ 4 ], Points[ 5 ], D3DCOLOR_ARGB( 0xff, 0xff, 0xff, 0xff ), 3 );
    ATG::DebugDraw::DrawScreenSpaceLine( Points[ 5 ], Points[ 6 ], D3DCOLOR_ARGB( 0xff, 0xff, 0xff, 0xff ), 3 );
    ATG::DebugDraw::DrawScreenSpaceLine( Points[ 6 ], Points[ 7 ], D3DCOLOR_ARGB( 0xff, 0xff, 0xff, 0xff ), 3 );
    ATG::DebugDraw::DrawScreenSpaceLine( Points[ 7 ], Points[ 4 ], D3DCOLOR_ARGB( 0xff, 0xff, 0xff, 0xff ), 3 );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: ApplyRollToHeadOrientationHUD()
// Desc: Rotates the 2D crosshair.
//--------------------------------------------------------------------------------------
HRESULT Sample::ApplyRollToHeadOrientationHUD( XMFLOAT2* Points, DWORD dwCount, XMFLOAT2 Center, FLOAT fRoll )
{
    XMMATRIX m = XMMatrixTransformation2D( XMVectorZero(), 1.0f, XMVectorSplatOne(), XMVectorSet( Center.x, Center.y, 0.0f, 0.0f ), fRoll * -1.0f, XMVectorZero() );

    for( DWORD i = 0 ; i < dwCount; ++ i )
    {
        XMVECTOR vResult = XMVector2Transform( XMVectorSet( Points[ i ].x, Points[ i ].y, 0.0f, 0.0f ), m );

        Points[ i ].x = vResult.x;
        Points[ i ].y = vResult.y;
    }

    return S_OK;
}

//--------------------------------------------------------------------------------------
// Name: RenderHeadOrientationQualityMessage()
// Desc: Displays an explanation of issues faced by NuiHeadOrientation. 
//--------------------------------------------------------------------------------------
VOID Sample::RenderHeadOrientationQualityMessage()
{
    static DOUBLE fTimeStamp = 0;
    static DWORD dwLastQualityFlags = 0;

    if( m_HeadOrientationQualityFlags != 0 )
    {
        fTimeStamp = m_Timer.GetAbsoluteTime();
        dwLastQualityFlags = m_HeadOrientationQualityFlags;
    }

    if( dwLastQualityFlags && m_Timer.GetAbsoluteTime() - fTimeStamp < 2 )
    {
        m_Font.Begin();
        m_Font.SetScaleFactors( 1.0f, 1.0f );
        m_Font.DrawText( 0.0f, 300.0f, 0xffffffff, ATG::GetHeadOrientationQualityFlagPrompt( dwLastQualityFlags ) );
        m_Font.End();
    }
}

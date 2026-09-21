//--------------------------------------------------------------------------------------
// AdvancedSkeletalRetargeting.cpp
//
// This sample demonstrates a method of retargeting the NUI skeleton to an arbitrary humanoid 
// character skeleton.  NUI joint positions are converted into joint rotations and mapped
// to a character skeleton that is used to skin a mesh.
//
// The sample uses assets converted with the XDK sample content exporter, which is included 
// with the XDK.  The content exporter provides a content pipeline for converting assets to a runtime
// format that can be animated and rendered with the ATG framework.  Using this sample
// along with the XDK content exporter, you can export a skinned mesh and control it
// with the NUI sensor.
//
// Microsoft Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include <xtl.h>
#include <xgraphics.h>
#include <XNAmath.h>
#include <AtgFont.h>
#include <AtgInput.h>
#include <AtgUtil.h>
#include <AtgHelp.h>
#include <AtgApp.h>
#include <AtgSimpleShaders.h>
#include <AtgDebugDraw.h>
#include <AtgSceneAll.h>
#include <AtgNuiVisualization.h>
#include <AtgNuiJointConverter.h>
#include <AtgNuiCommon.h>
#include <AtgNuiJointFilter.h>

//--------------------------------------------------------------------------------------
// Callouts for labelling the gamepad on the help screen
//--------------------------------------------------------------------------------------
ATG::HELP_CALLOUT g_HelpCallouts[] =
{
    { ATG::HELP_B_BUTTON,     ATG::HELP_PLACEMENT_2, L"Draw mesh" },
    { ATG::HELP_Y_BUTTON,     ATG::HELP_PLACEMENT_2, L"Draw skeleton" },
    { ATG::HELP_RIGHT_STICK,  ATG::HELP_PLACEMENT_2, L"Rotate camera" },
    { ATG::HELP_BACK_BUTTON,  ATG::HELP_PLACEMENT_2, L"Display\nhelp" },
};


//--------------------------------------------------------------------------------------
// Name: struct ModelInfo
// Desc: Contains all relevant information about a single model.  Each ModelInfo
//       contains either a valid pSkinnedMesh or pStaticMesh pointer.  If the pStaticMesh
//       pointer is valid, the skinning data will not be filled in.
//--------------------------------------------------------------------------------------
struct ModelInfo
{
    // The model from the scene hierarchy
    ATG::Model* pModel;

    // Cached information about the model
    D3DVertexBuffer* pMeshVB;
    D3DIndexBuffer* pMeshIB;
    D3DVertexDeclaration* pMeshDecl;
    DWORD dwMeshVertexCount;
    DWORD dwMeshVertexStride;
    DWORD dwMeshSubsetCount;

    // The skinned mesh, and a binding that maps the skeleton instance to this mesh
    ATG::SkinnedMesh* pSkinnedMesh;
    DWORD dwSkeletonInstanceToSkinnedMeshBinding;

    // The static mesh, and a binding that maps a skeleton bone to this mesh
    ATG::StaticMesh* pStaticMesh;
    DWORD dwSkeletonBoneToStaticMeshBinding;

    // The following members are only used when pStaticMesh is NULL and pSkinnedMesh is
    // not NULL.

    // Memory allocations for bone matrix constants
    VOID* pBoneMatrixBufferVirtual;
};

const DWORD MAX_MAPPED_JOINTS = 100;

struct JointMap
{
    INT iModelJointIndex;
    ATG::INTERMEDIATE_JOINT_INDEX jointConverterIndex;
    INT iModelParentJointIndex;
    INT iModelChildJointIndex;

    JointMap( ) {}
    JointMap( INT iModelJointIdx, ATG::INTERMEDIATE_JOINT_INDEX jointConverterIdx, INT iModelParentJointIdx, INT iModelChildJointIdx ) :
                iModelJointIndex( iModelJointIdx),
                jointConverterIndex( jointConverterIdx ),
                iModelParentJointIndex( iModelParentJointIdx ),
                iModelChildJointIndex( iModelChildJointIdx )
    {}
};

const DWORD SCREEN_WIDTH = 1280;
const DWORD SCREEN_HEIGHT = 720;

//--------------------------------------------------------------------------------------
// Name: class Sample
// Desc: The Advanced Skeletal Retargeting sample class.
//--------------------------------------------------------------------------------------
class Sample : public ATG::Application
{
public:
    HRESULT Initialize();
    HRESULT Update();
    HRESULT Render();

private:
    VOID    InitializeAnimation();
    VOID    InitializeJointMapping();
    HRESULT InitializeSkeletonTracking();
    HRESULT InitializeRendering();
    VOID    LoadScene();
    HRESULT UpdateSkeletonTracking();

    VOID    SetupMaterial( ModelInfo* pModelInfo, DWORD dwSubsetIndex );
    VOID    RenderSkinnedModel( ModelInfo* pModelInfo );
    VOID    RenderStaticModel( ModelInfo* pModelInfo );
    VOID    RenderSkeleton();
    VOID    RenderUI();
    
    VOID    SetInDefaultPose();

private:
    // Sample framework objects
    ATG::Font m_Font;
    ATG::Timer m_Timer;
    ATG::Help m_Help;
    BOOL m_bDrawHelp;
    FLOAT m_fDeltaTime;

    // View parameters
    FLOAT                       m_fLookPitch;
    FLOAT                       m_fLookYaw;
    XMVECTOR                    m_vEyePt;
    XMVECTOR                    m_vUp;
    XMMATRIX                    m_matView;
    XMMATRIX                    m_matProj; 
    XMMATRIX                    m_matWorld;

    // NUI depth and color buffers
    CONST NUI_IMAGE_FRAME*      m_pImageFrame;
    CONST NUI_IMAGE_FRAME*      m_pDepthFrame;

    ATG::NuiVisualization       m_pip;
    HANDLE                      m_hImage;
    HANDLE                      m_hDepth;
    HANDLE                      m_hFrameEndEvent;

    NUI_SKELETON_FRAME          m_NuiSkeleton;
    UINT                        m_iCurrentSkeletonIndex;
    ATG::NuiJointConverter*     m_pNuiJointConverter;           // Joint converter for transforming NUI joint positions to rotations
    ATG::FilterDoubleExponential m_filterDoubleExponential;
    HRESULT                     m_hrNuiResult;

    // NUI-to-Model joint mapping
    JointMap m_MappedJoints[ MAX_MAPPED_JOINTS ];
    DWORD m_dwNumMappedJoints;

    // Render targets
    D3DSurface* m_pColorTarget1X;
    D3DSurface* m_pDepthStencil1X;
    D3DSurface* m_pColorTarget4X;
    D3DSurface* m_pDepthStencil4X;

    // Resolve texture
    D3DTexture* m_pResolveBuffer;

    // Front buffer
    D3DTexture* m_pFrontBuffer;

    // Scene members
    ATG::Scene* m_pScene;
    XMMATRIX m_matWVP;

    // Model info structs
    ModelInfo* m_pModelInfos;
    DWORD m_dwModelCount;

    // Animation members
    ATG::Skeleton m_Skeleton;
    ATG::SkeletonInstance m_SkeletonInstance;

    // Rendering members
    D3DVertexShader* m_pVertexShaderSkinningConstants;
    D3DVertexShader* m_pVertexShaderTransform;
    D3DPixelShader* m_pPixelShaderCharacter;
    XMFLOAT4 m_DirectionalLightDirection;
    XMFLOAT4 m_DirectionalLightColor;
    XMFLOAT4 m_AmbientColor;
    BOOL m_bDrawScene;
    BOOL m_bDrawSkeleton;
    FLOAT m_fBoneRadius;
};


//--------------------------------------------------------------------------------------
// Name: main()
// Desc: Entry point to the sample.
//--------------------------------------------------------------------------------------
INT __cdecl main()
{
    Sample ASRSample;

    // Set up the structure used to create the D3DDevice.
    D3DPRESENT_PARAMETERS& d3dpp = ASRSample.m_d3dpp;
    ZeroMemory( &d3dpp, sizeof( D3DPRESENT_PARAMETERS ) );
    d3dpp.BackBufferWidth = SCREEN_WIDTH;
    d3dpp.BackBufferHeight = SCREEN_HEIGHT;
    d3dpp.BackBufferFormat =  D3DFMT_A8R8G8B8;
    d3dpp.FrontBufferFormat = D3DFMT_LE_X8R8G8B8;
    d3dpp.MultiSampleType = D3DMULTISAMPLE_NONE;
    d3dpp.MultiSampleQuality = 0;
    d3dpp.BackBufferCount = 0;
    d3dpp.EnableAutoDepthStencil = FALSE;
    d3dpp.DisableAutoBackBuffer = TRUE;
    d3dpp.DisableAutoFrontBuffer = TRUE;
    d3dpp.AutoDepthStencilFormat = D3DFMT_D24S8;
    d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_TWO;

    ASRSample.m_dwDeviceCreationFlags |= D3DCREATE_BUFFER_2_FRAMES;

    ASRSample.Run();
}


//--------------------------------------------------------------------------------------
// Name: InitializeSkeletonTracking()
// Desc: Initialize NUI skeleton tracking and the PIP viewer
//--------------------------------------------------------------------------------------
HRESULT Sample::InitializeSkeletonTracking()
{
    HRESULT hr;

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

    // Initialize the Picture in Picture visualization
    if( FAILED( m_pip.Initialize( m_pd3dDevice, NUI_INITIALIZE_FLAG_USES_COLOR |
                                                NUI_INITIALIZE_FLAG_USES_DEPTH_AND_PLAYER_INDEX |
                                                NUI_INITIALIZE_FLAG_USES_SKELETON, 
		                                        NUI_IMAGE_RESOLUTION_640x480 ) ) )
    {
        ATG_PrintError ( "Picture in Picture initialization failed" );
    }

    return hr;
}


//--------------------------------------------------------------------------------------
// Name: InitializeRendering()
// Desc: Initialize rendering resources
//--------------------------------------------------------------------------------------
HRESULT Sample::InitializeRendering()
{
    HRESULT hr;
    
    // Create font
    if( FAILED( m_Font.Create( "game:\\Media\\Fonts\\Arial_16.xpr" ) ) )
        ATG::FatalError( "Could not load font." );

    // Confine text drawing to the title safe area
    m_Font.SetWindow( ATG::GetTitleSafeArea() );

    // Create help screen
    if( FAILED( m_Help.Create( "game:\\Media\\Help\\Help.xpr" ) ) )
        ATG::FatalError( "Could not load help resources." );

    // Initialize simple shaders
    ATG::SimpleShaders::Initialize( NULL, NULL );

    // Set up surface parameters for creating the rendertargets
    D3DSURFACE_PARAMETERS ColorSurfParams;
    ColorSurfParams.Base = 0;
    ColorSurfParams.ColorExpBias = 0;
    ColorSurfParams.HierarchicalZBase = 0;
    ColorSurfParams.HiZFunc = D3DHIZFUNC_DEFAULT;

    D3DSURFACE_PARAMETERS DepthSurfParams;
    DepthSurfParams.Base = XGSurfaceSize( SCREEN_WIDTH, 256, D3DFMT_A8R8G8B8, D3DMULTISAMPLE_4_SAMPLES );
    DepthSurfParams.ColorExpBias = 0;
    DepthSurfParams.HierarchicalZBase = 0;
    DepthSurfParams.HiZFunc = D3DHIZFUNC_DEFAULT;

    // Create 1280x720 1xMSAA surfaces
    hr = m_pd3dDevice->CreateRenderTarget( SCREEN_WIDTH, SCREEN_HEIGHT,
                                           D3DFMT_A8R8G8B8,
                                           D3DMULTISAMPLE_NONE,
                                           0, FALSE,
                                           &m_pColorTarget1X,
                                           &ColorSurfParams );
    if( FAILED( hr ) )
        ATG::FatalError( "Couldn't create color rendertarget." );

    hr = m_pd3dDevice->CreateRenderTarget( SCREEN_WIDTH, SCREEN_HEIGHT,
                                           D3DFMT_D24S8,
                                           D3DMULTISAMPLE_NONE,
                                           0, FALSE,
                                           &m_pDepthStencil1X,
                                           &DepthSurfParams );
    if( FAILED( hr ) )
        ATG::FatalError( "Couldn't create depth/stencil rendertarget." );

    // Create 1280x256 4xMSAA surfaces.
    hr = m_pd3dDevice->CreateRenderTarget( SCREEN_WIDTH, 256,
                                           D3DFMT_A8R8G8B8,
                                           D3DMULTISAMPLE_4_SAMPLES,
                                           0, FALSE,
                                           &m_pColorTarget4X,
                                           &ColorSurfParams );
    if( FAILED( hr ) )
        ATG::FatalError( "Couldn't create color rendertarget." );

    hr = m_pd3dDevice->CreateRenderTarget( SCREEN_WIDTH, 256,
                                           D3DFMT_D24S8,
                                           D3DMULTISAMPLE_4_SAMPLES,
                                           0, FALSE,
                                           &m_pDepthStencil4X,
                                           &DepthSurfParams );
    if( FAILED( hr ) )
        ATG::FatalError( "Couldn't create depth/stencil rendertarget." );

    // Create 1280x720 front buffer texture and resolve buffer texture
    hr = m_pd3dDevice->CreateTexture( SCREEN_WIDTH, SCREEN_HEIGHT, 1, 0, D3DFMT_LE_X8R8G8B8, D3DPOOL_DEFAULT, &m_pFrontBuffer, NULL );
    if( FAILED( hr ) )
        ATG::FatalError( "Couldn't create front buffer." );
    hr = m_pd3dDevice->CreateTexture( SCREEN_WIDTH, SCREEN_HEIGHT, 1, 0, D3DFMT_X8R8G8B8, D3DPOOL_DEFAULT, &m_pResolveBuffer, NULL );
    if( FAILED( hr ) )
        ATG::FatalError( "Couldn't create resolve texture." );

    // Load shaders.
    hr = ATG::LoadVertexShader( "game:\\media\\shaders\\SkinVSConstants.xvu", &m_pVertexShaderSkinningConstants );
    if( FAILED( hr ) )
        ATG::FatalError( "Couldn't load vertex shader." );
    hr = ATG::LoadVertexShader( "game:\\media\\shaders\\TransformVS.xvu", &m_pVertexShaderTransform );
    if( FAILED( hr ) )
        ATG::FatalError( "Couldn't load vertex shader." );
    hr = ATG::LoadPixelShader( "game:\\media\\shaders\\CharacterPS.xpu", &m_pPixelShaderCharacter );
    if( FAILED( hr ) )
        ATG::FatalError( "Couldn't load pixel shader." );

    // Initialize view parameters
    XVIDEO_MODE VideoMode;
    XGetVideoMode( &VideoMode );
    FLOAT fAspectRatio = VideoMode.fIsWideScreen == TRUE ? (16.0f / 9.0f) : (4.0f / 3.0f);
    m_matProj    = XMMatrixPerspectiveFovLH( XM_PI/4, fAspectRatio, 0.01f, 20.0f ); 
    static const XMVECTORF32 g_svEyePt = {0.0f,1.0f,-2.0f,0.0f};
    static const XMVECTORF32 g_svUp = {0.0f,1.0f,0.0f,0.0f};
    m_vEyePt     = g_svEyePt;
    m_vUp        = g_svUp;
    m_fLookPitch = 0.0f;
    m_fLookYaw   = 0.0f;

    return hr;
}

const WCHAR* strModelNames[] = { L"UpperArms", L"LowerArms", L"Head", L"Beard", L"Moustache", L"Armor", L"UpperLegs", L"LowerLegs", L"Boots", L"Gloves", L"Shirt", L"Pants", L"Eyes" };

//--------------------------------------------------------------------------------------
// Name: LoadScene()
// Desc: Load the scene file and initialize the model
//--------------------------------------------------------------------------------------
VOID Sample::LoadScene()
{

    // Create scene object.
    m_pScene = new ATG::Scene();
    ATG::ResourceDatabase* pRDB = m_pScene->GetResourceDatabase();

    // Create default textures in the resource database.
    pRDB->CreateDefaultResources();

    // Load character and animation data from a scene file.
    ATG::SceneFileParser::LoadXATGFile( "game:\\media\\scenes\\Conquistador.xatg", m_pScene, NULL,
                                        ATG::XATGLOADER_DONOTINITIALIZEMATERIALS, NULL );

    // Search for specific models in the scene file.
    m_dwModelCount = ARRAYSIZE( strModelNames );
    m_pModelInfos = new ModelInfo[ m_dwModelCount ];
    ZeroMemory( m_pModelInfos, m_dwModelCount * sizeof( ModelInfo ) );

    for( DWORD i = 0; i < m_dwModelCount; ++i )
    {
        // Search for the model.
        ATG::Model* pModel = ( ATG::Model* )m_pScene->FindObjectOfType( strModelNames[i], ATG::Model::TypeID );
        assert( pModel != NULL );
        m_pModelInfos[i].pModel = pModel;

        // Extract data from the model and cache it.
        ATG::MeshMapping& mm = pModel->GetMeshMapping( 0 );
        ATG::BaseMesh* pMesh = mm.pMesh;
        m_pModelInfos[i].dwMeshVertexCount = pMesh->GetNumVertices();
        m_pModelInfos[i].dwMeshSubsetCount = pMesh->GetNumSubsets();
        m_pModelInfos[i].pMeshVB = pMesh->GetVertexData( 0 )->GetVertexStream( 0 )->pVertexBuffer;
        m_pModelInfos[i].pMeshDecl = pMesh->GetVertexData( 0 )->GetVertexDecl();
        m_pModelInfos[i].dwMeshVertexStride = pMesh->GetVertexData( 0 )->GetVertexStream( 0 )->Stride;
        m_pModelInfos[i].pMeshIB = pMesh->GetIndexData( 0 )->GetIndexBuffer();

        // Check if the mesh is a skinned mesh.
        if( pMesh->Type() == ATG::SkinnedMesh::TypeID )
        {
            m_pModelInfos[i].pSkinnedMesh = ( ATG::SkinnedMesh* )pMesh;
            m_pModelInfos[i].pBoneMatrixBufferVirtual = new XMFLOAT4A[ ATG::g_dwMaxBoneCount * 3 ];
        }
        else if( pMesh->Type() == ATG::StaticMesh::TypeID )
        {
            // Mesh is a static mesh (no skinning data).
            m_pModelInfos[i].pStaticMesh = ( ATG::StaticMesh* )pMesh;
        }
    }

    // Make sure detail map is correctly set up as being sRGB.
    for( DWORD i = 0; i < m_dwModelCount; ++i )
    {
        for( DWORD j = 0; j < m_pModelInfos[i].dwMeshSubsetCount; ++j )
        {
            const ATG::MeshMapping& mm = m_pModelInfos[i].pModel->GetMeshMapping( 0 );
            ATG::MaterialInstance* pMaterial = mm.Materials[ j ];

            if( pMaterial->GetRawParameterCount() >= 1)
            {
                ATG::Texture* pTextureResource = NULL;
                pTextureResource = ( ATG::Texture* )pMaterial->GetRawParameter( 0 ).pValue;
                if( pTextureResource != NULL )
                {
                    // Make sure we set the texture up to be a high-precision (AS_16) sRGB format.
                    // The source image data was encoded as sRGB, so we need to read it as such.
                    // Reading from an AS_16_16_16_16 format texture gives us full precision when converting
                    // from sRGB to linear in the shader (not using AS_16* means the conversion happens with only
                    // 8 bits, losing precision in the process).
                    ATG::ConvertTextureToAs16SRGBFormat( ( D3DTexture* )pTextureResource->GetD3DTexture() );
                }
            }
        }
    }
}


//--------------------------------------------------------------------------------------
// Name: Initialize()
// Desc: Initializes the objects for the sample, and loads various resources.
//--------------------------------------------------------------------------------------
HRESULT Sample::Initialize()
{
    // Initialize member variables.
    m_bDrawHelp = FALSE;
    m_bDrawSkeleton = FALSE;
    m_bDrawScene = TRUE;
    m_fBoneRadius = 0.01f;
    m_DirectionalLightColor = XMFLOAT4( 1, 1, 1, 1 );
    m_DirectionalLightDirection = XMFLOAT4( -0.5774f, -0.5774f, .5774f, 0 );
    m_AmbientColor = XMFLOAT4( 0.05f, 0.05f, 0.05f, 0.1f );
    m_iCurrentSkeletonIndex = 0;
    m_matWorld = XMMatrixIdentity();

    InitializeSkeletonTracking();
    InitializeRendering();
    LoadScene();
    InitializeAnimation();
    InitializeJointMapping();

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: SetInDefaultPose()
// Desc: Sets the model in the bind pose at the initial position.
//--------------------------------------------------------------------------------------
VOID Sample::SetInDefaultPose()
{
    // Set the skeleton in bind pose
    for( DWORD i = 0; i < m_SkeletonInstance.m_pSkeleton->GetBoneCount(); ++i )
    {
        m_SkeletonInstance.SetJointRotationOffset( i, XMQuaternionIdentity() );
    }

    // Set the model in an initial pose
    XMVECTOR vOffset = XMVectorSet(0.0f, 1.0f, 2.0f, 0.0f);
    m_SkeletonInstance.SetJointPositionOffset(m_SkeletonInstance.m_pSkeleton->FindBone( ATG::StringID(L"Pelvis") ),
                                               vOffset );

    m_SkeletonInstance.UpdateAnimation( 0.0f );     // Updating the animation sets up the transformations correctly
    m_SkeletonInstance.BuildWorldPose();            // Build transformations
}


//--------------------------------------------------------------------------------------
// Name: InitializeAnimation()
// Desc: Initializes the animation system and binds each model to parts of the animation
//       system.
//--------------------------------------------------------------------------------------
VOID Sample::InitializeAnimation()
{
    // Find a frame named "Pelvis" - this is the root of the animated object.
    ATG::Frame* pSkeletonFrame = ( ATG::Frame* )m_pScene->FindObjectOfType( L"Pelvis", ATG::Frame::TypeID );
    assert( pSkeletonFrame != NULL );

    // Initialize skeleton.
    m_Skeleton.Initialize( pSkeletonFrame );

    // Initialize the skeleton instance, and allocate enough bindings for the model count.
    m_SkeletonInstance.Initialize( &m_Skeleton, m_dwModelCount );

    // For each model, bind the mesh (either skinned or static) to the skeleton instance.
    for( DWORD i = 0; i < m_dwModelCount; ++i )
    {
        if( m_pModelInfos[i].pSkinnedMesh != NULL )
        {
            // Bind the skinned mesh to the skeleton instance.
            // This creates a structure that maps the skin influences used in the mesh
            // to bones in the skeleton instance.
            m_SkeletonInstance.BindSkinnedMesh( i, m_pModelInfos[i].pSkinnedMesh );
            m_pModelInfos[i].dwSkeletonInstanceToSkinnedMeshBinding = i;
        }
        else
        {
            // Bind the static mesh to a single bone within the skeleton.
            // This bone's transform will be used to render the static mesh in the
            // animated position each frame.
            ATG::Model* pModel = m_pModelInfos[i].pModel;
            ATG::StringID ModelBoneName = pModel->GetName();
            m_pModelInfos[i].dwSkeletonBoneToStaticMeshBinding = m_Skeleton.FindBone( ModelBoneName );
        }
    }

    SetInDefaultPose();
}


//--------------------------------------------------------------------------------------
// Name: ExtractLocalBindDirFromWorldTransforms()
// Desc: Extracts the direction of the bind pose for a joint from its bind pose transformation
//       and the bind pose transformation of its parent.
//--------------------------------------------------------------------------------------
XMVECTOR ExtractLocalBindDirFromWorldTransforms( XMMATRIX matParent, XMMATRIX matChild )
{
    // Subtract the child's bind pose position from the parent to get the world bind pose direction
    XMVECTOR vParentWorldPos = matParent.r[3];
    XMVECTOR vChildWorldPos = matChild.r[3];
    XMVECTOR vWorldBindDir = XMVector3Normalize( vChildWorldPos - vParentWorldPos );

    // Transform the world direction by the inverse of the parent's rotation
    XMVECTOR qParentWorldRotation = XMQuaternionRotationMatrix( matParent ); // Convert from matrix to quaternion to remove positional effects
    XMMATRIX matInvParentWorldRotation = XMMatrixRotationQuaternion( XMQuaternionInverse( qParentWorldRotation ) );
    XMVECTOR vLocalBindDir = XMVector3Transform( vWorldBindDir, matInvParentWorldRotation);

    return vLocalBindDir;
}


//--------------------------------------------------------------------------------------
// Name: InitializeJointMapping()
// Desc: Set up the joint mapping for the NUI-to-Model conversion.  This is where we define 
//       how the joints in the NUI skeleton affect the model skeleton.  Skeleton hierarchy and
//       bind pose information is defined here.
// 
//--------------------------------------------------------------------------------------
VOID Sample::InitializeJointMapping()
{   
    // Find the index of each of the joints
    INT iShoulder_L     = m_SkeletonInstance.m_pSkeleton->FindBone( ATG::StringID( L"Shoulder_L" ) );
    INT iShoulder_R     = m_SkeletonInstance.m_pSkeleton->FindBone( ATG::StringID( L"Shoulder_R" ) );
    INT iElbow_L        = m_SkeletonInstance.m_pSkeleton->FindBone( ATG::StringID( L"Elbow_L" ) );
    INT iElbow_R        = m_SkeletonInstance.m_pSkeleton->FindBone( ATG::StringID( L"Elbow_R" ) );
    INT iWrist_L        = m_SkeletonInstance.m_pSkeleton->FindBone( ATG::StringID( L"Wrist_L" ) );
    INT iWrist_R        = m_SkeletonInstance.m_pSkeleton->FindBone( ATG::StringID( L"Wrist_R" ) );
    INT iHip_L          = m_SkeletonInstance.m_pSkeleton->FindBone( ATG::StringID( L"Hip_L" ) );
    INT iHip_R          = m_SkeletonInstance.m_pSkeleton->FindBone( ATG::StringID( L"Hip_R" ) );
    INT iKnee_L         = m_SkeletonInstance.m_pSkeleton->FindBone( ATG::StringID( L"Knee_L" ) );
    INT iKnee_R         = m_SkeletonInstance.m_pSkeleton->FindBone( ATG::StringID( L"Knee_R" ) );
    INT iAnkle_L        = m_SkeletonInstance.m_pSkeleton->FindBone( ATG::StringID( L"Ankle_L" ) );
    INT iAnkle_R        = m_SkeletonInstance.m_pSkeleton->FindBone( ATG::StringID( L"Ankle_R" ) );
    INT iNeck           = m_SkeletonInstance.m_pSkeleton->FindBone( ATG::StringID( L"Neck" ) );
    INT iHead           = m_SkeletonInstance.m_pSkeleton->FindBone( ATG::StringID( L"Head" ) );
    INT iShoulderCenter = m_SkeletonInstance.m_pSkeleton->FindBone( ATG::StringID( L"Back" ) );
    INT iCollar_L       = m_SkeletonInstance.m_pSkeleton->FindBone( ATG::StringID( L"Collar_L" ) );
    INT iCollar_R       = m_SkeletonInstance.m_pSkeleton->FindBone( ATG::StringID( L"Collar_R" ) );
    INT iPelvis         = m_SkeletonInstance.m_pSkeleton->FindBone( ATG::StringID( L"Pelvis" ) );
    INT iSpine02        = m_SkeletonInstance.m_pSkeleton->FindBone( ATG::StringID( L"Spine02" ) );
    INT iSpine01        = m_SkeletonInstance.m_pSkeleton->FindBone( ATG::StringID( L"Spine01" ) );

    // Set up the joint mapping
    m_dwNumMappedJoints = 0;
    m_MappedJoints[m_dwNumMappedJoints++] = JointMap( iShoulder_R,  ATG::INT_SHOULDER_LEFT,     iCollar_R,       iElbow_R );
    m_MappedJoints[m_dwNumMappedJoints++] = JointMap( iElbow_R,     ATG::INT_ELBOW_LEFT,        iShoulder_R,     iWrist_R );
    m_MappedJoints[m_dwNumMappedJoints++] = JointMap( iShoulder_L,  ATG::INT_SHOULDER_RIGHT,    iCollar_L,       iElbow_L );
    m_MappedJoints[m_dwNumMappedJoints++] = JointMap( iElbow_L,     ATG::INT_ELBOW_RIGHT,       iElbow_L,        iWrist_L );
    m_MappedJoints[m_dwNumMappedJoints++] = JointMap( iHip_R,       ATG::INT_HIP_LEFT,          iPelvis,         iKnee_R );
    m_MappedJoints[m_dwNumMappedJoints++] = JointMap( iKnee_R,      ATG::INT_KNEE_LEFT,         iHip_R,          iAnkle_R );
    m_MappedJoints[m_dwNumMappedJoints++] = JointMap( iHip_L,       ATG::INT_HIP_RIGHT,         iPelvis,         iKnee_L );
    m_MappedJoints[m_dwNumMappedJoints++] = JointMap( iKnee_L,      ATG::INT_KNEE_RIGHT,        iHip_L,          iAnkle_L );
    m_MappedJoints[m_dwNumMappedJoints++] = JointMap( iNeck,        ATG::INT_NECK,              iShoulderCenter, iHead );
    m_MappedJoints[m_dwNumMappedJoints++] = JointMap( iSpine01,     ATG::INT_HIP_CENTER,        iPelvis,         iSpine02 );
    m_MappedJoints[m_dwNumMappedJoints++] = JointMap( iSpine02,     ATG::INT_SPINE,             iSpine01,        iShoulderCenter );
    m_MappedJoints[m_dwNumMappedJoints++] = JointMap( iCollar_L,    ATG::INT_COLLAR_RIGHT,      iShoulderCenter, iShoulder_L );
    m_MappedJoints[m_dwNumMappedJoints++] = JointMap( iCollar_R,    ATG::INT_COLLAR_LEFT,       iShoulderCenter, iShoulder_R );
    m_MappedJoints[m_dwNumMappedJoints++] = JointMap( iPelvis,      ATG::INT_BASE,              -1,              -1);

    // Set the skeleton in bind pose
    for( DWORD i = 0; i < m_SkeletonInstance.m_pSkeleton->GetBoneCount(); ++i )
    {
        m_SkeletonInstance.SetJointRotationOffset( i, XMQuaternionIdentity() );
    }
    m_SkeletonInstance.UpdateAnimation( 0.0f );     // Update the animation to set up the transformations
    m_SkeletonInstance.BuildWorldPose();            // Build transformations

    // Extract bind pose transformation and bind direction for each of the maped joints
    for( DWORD i = 0; i < m_dwNumMappedJoints; ++i )
    {
        if( m_MappedJoints[i].iModelParentJointIndex != -1 )
        {
            XMVECTOR qWorldRot = XMQuaternionRotationMatrix(m_SkeletonInstance.m_WorldPose.LoadTransform( m_MappedJoints[i].iModelJointIndex ) );
            XMVECTOR qParentWorldRot = XMQuaternionRotationMatrix( m_SkeletonInstance.m_WorldPose.LoadTransform( m_MappedJoints[i].iModelParentJointIndex ) );
            XMVECTOR qLocalBindRot = XMQuaternionMultiply( XMQuaternionInverse( qParentWorldRot ), qWorldRot );
            m_pNuiJointConverter->SetBindPose( m_MappedJoints[i].jointConverterIndex , qLocalBindRot );

            XMMATRIX matBindWorld = m_SkeletonInstance.m_WorldPose.LoadTransform( m_MappedJoints[i].iModelJointIndex );
            XMMATRIX matChildBindWorld = m_SkeletonInstance.m_WorldPose.LoadTransform( m_MappedJoints[i].iModelChildJointIndex );
            XMVECTOR vBindDir = ExtractLocalBindDirFromWorldTransforms( matBindWorld, matChildBindWorld );
            m_pNuiJointConverter->SetBindDir( m_MappedJoints[i].jointConverterIndex, vBindDir );
        }
    }

    // Set the joint scale factor to identity.  Normally we would want to scale Z by -1 in order to get a mirrored skeleton (so that viewing the screen looks 
    // like you are looking in a mirror).  In this case, the coordinate system of the model is left-handed, while the coordinate system of NUI is right handed,
    // so to get from right-handed to left-handed, we need to reverse the Z direction.  The two transformations cancel each other and we're left with an identity transform.
    m_pNuiJointConverter->SetJointScaleFactor( XMVectorSet(1, 1, 1, 1) );

}


//--------------------------------------------------------------------------------------
// Name: UpdateSkeletonTracking()
// Desc: Updates the skeleton tracking with the latest data. Note that if there is
//       no new data, then we will skip updates of the skeleton
//--------------------------------------------------------------------------------------
HRESULT Sample::UpdateSkeletonTracking()
{
    // wait for frame processing to end
    if( WAIT_OBJECT_0 != WaitForSingleObject( m_hFrameEndEvent, NUI_FRAME_END_TIMEOUT_DEFAULT ) )
    {
        return E_FAIL;
    }
    
    // Get the color and depth frames
    HRESULT hrImage = NuiImageStreamGetNextFrame( m_hImage, 0, &m_pImageFrame );
    HRESULT hrDepth = NuiImageStreamGetNextFrame( m_hDepth, 0, &m_pDepthFrame );
    // Get the next skeleton frame
    m_hrNuiResult = NuiSkeletonGetNextFrame( 0, &m_NuiSkeleton );

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
        if( m_NuiSkeleton.SkeletonData[ m_iCurrentSkeletonIndex ].eTrackingState != NUI_SKELETON_TRACKED )
        {
            for( UINT i = 0; i < NUI_SKELETON_COUNT; ++ i )
            {
                if( m_NuiSkeleton.SkeletonData[ i ].eTrackingState == NUI_SKELETON_TRACKED )
                {
                    m_iCurrentSkeletonIndex = i;
                }
            }
        }
        m_pip.SetSkeletons( &m_NuiSkeleton );
        
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
    m_fDeltaTime = ( FLOAT )m_Timer.GetElapsedTime();

    // Get input from controllers (check for exit button sequence).
    ATG::GAMEPAD* pGamepad = ATG::Input::GetMergedInput();

    // The Back button toggles the help screen.
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_BACK )
        m_bDrawHelp = !m_bDrawHelp;

    // The Y button toggles skeleton drawing.
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_Y )
        m_bDrawSkeleton = !m_bDrawSkeleton;

    // The B button toggles scene drawing.
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_B )
        m_bDrawScene = !m_bDrawScene;

    // Rotate view
    m_fLookYaw     += pGamepad->fX2 * m_fDeltaTime;
    m_fLookPitch   -= pGamepad->fY2 * m_fDeltaTime;
    m_fLookYaw     = fmodf( m_fLookYaw, XM_2PI );
    m_fLookPitch   = fmodf( m_fLookPitch, XM_2PI );

    XMMATRIX lookAtMatrix   = XMMatrixRotationRollPitchYaw( m_fLookPitch, m_fLookYaw, 0.0f );
    static const XMVECTORF32 g_vTransform = { 0.0f, 0.0f, 1.0f, 1.0f };
    XMVECTOR m_vLookToZ     = XMVector3Transform(g_vTransform, lookAtMatrix);

    // Move viewing position
    m_vEyePt = XMVectorAdd(m_vEyePt, XMVectorScale(lookAtMatrix.r[0], m_fDeltaTime * pGamepad->fX1));
    m_vEyePt = XMVectorAdd(m_vEyePt, XMVectorScale(lookAtMatrix.r[2], m_fDeltaTime * pGamepad->fY1));

    m_matView = XMMatrixLookToLH( m_vEyePt, m_vLookToZ, m_vUp );

    // Update NUI skeleton
    UpdateSkeletonTracking();

    // Update model skeleton from NUI data.
    if( m_NuiSkeleton.SkeletonData[ m_iCurrentSkeletonIndex ].eTrackingState == NUI_SKELETON_TRACKED )
    {
        if( m_pNuiJointConverter )
        {   
            // Filter the joints
            XMVECTOR* filteredJoints;
            m_filterDoubleExponential.Update(&m_NuiSkeleton.SkeletonData[m_iCurrentSkeletonIndex]);
            filteredJoints = m_filterDoubleExponential.GetFilteredJoints();
            for(int i = 0; i < NUI_SKELETON_POSITION_COUNT; i++)
            {
                m_NuiSkeleton.SkeletonData[m_iCurrentSkeletonIndex].SkeletonPositions[i] = filteredJoints[i];
            }

            // Convert the NUI skeleton positions to rotations
            m_pNuiJointConverter->ConvertNuiJoints( &m_NuiSkeleton.SkeletonData[m_iCurrentSkeletonIndex],FALSE);

            // Update the rotations on the model skeleton, using the rotations extracted from the NUI skeleton
            for( DWORD i = 0; i < m_dwNumMappedJoints; ++i )
            {
                XMVECTOR qRot = XMQuaternionIdentity();
                if( m_MappedJoints[i].iModelJointIndex == m_SkeletonInstance.m_pSkeleton->FindBone( ATG::StringID( L"Shoulder_R" ) ) )
                {
                    // We don't want a rotation on the collar, but we have to account for it's rotation somewhere, so we'll add it onto the shoulder
                    // rotation
                    qRot = XMQuaternionMultiply(m_pNuiJointConverter->GetLocalRot( m_MappedJoints[i].jointConverterIndex ), m_pNuiJointConverter->GetLocalRot( ATG::INT_COLLAR_LEFT ) );
                }
                else if( m_MappedJoints[i].iModelJointIndex == m_SkeletonInstance.m_pSkeleton->FindBone( ATG::StringID( L"Collar_R" ) ) )
                {
                    // Do nothing, we already used the collar rotation for the shoulder
                }
                else if( m_MappedJoints[i].iModelJointIndex == m_SkeletonInstance.m_pSkeleton->FindBone( ATG::StringID( L"Shoulder_L" ) ) )
                {
                     // We don't want a rotation on the collar, but we have to account for it's rotation somewhere, so we'll add it onto the shoulder
                    // rotation
                    qRot = XMQuaternionMultiply(m_pNuiJointConverter->GetLocalRot( m_MappedJoints[i].jointConverterIndex ), m_pNuiJointConverter->GetLocalRot( ATG::INT_COLLAR_RIGHT ) );
                }
                else if( m_MappedJoints[i].iModelJointIndex == m_SkeletonInstance.m_pSkeleton->FindBone( ATG::StringID( L"Collar_L" ) ) )
                {
                   // Do nothing, we already used the collar rotation for the shoulder
                }
                else if( m_MappedJoints[i].iModelJointIndex == m_SkeletonInstance.m_pSkeleton->FindBone( ATG::StringID( L"Neck" ) ) )
                {
                    // The model skeleton has two joints for the neck, but the NUI skeleton only has one.  We'll add a rotation to compensate for the missing joint
                    FLOAT angle = 20.0f;
                    qRot = XMQuaternionMultiply(m_pNuiJointConverter->GetLocalRot( m_MappedJoints[i].jointConverterIndex ), XMQuaternionRotationAxis(XMVectorSet(1,0,0,0), XMConvertToRadians(angle)) );
                }
                else
                {
                    // Otherwise, just set the NUI rotation directly onto the joint
                    qRot =  m_pNuiJointConverter->GetLocalRot( m_MappedJoints[i].jointConverterIndex );
                }
                m_SkeletonInstance.SetJointRotationOffset( m_MappedJoints[i].iModelJointIndex, qRot );
            }

            // Set the base position of the model skeleton, based on the NUI hip center
            XMVECTOR vOffset = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
            m_SkeletonInstance.SetJointPositionOffset(m_SkeletonInstance.m_pSkeleton->FindBone(ATG::StringID(L"Pelvis")),
                                                m_NuiSkeleton.SkeletonData[m_iCurrentSkeletonIndex].SkeletonPositions[NUI_SKELETON_POSITION_HIP_CENTER] + vOffset );
        }

        // Update joint transforms.
        m_SkeletonInstance.UpdateAnimation( 0.0f ); // fDeltaTime is not used in this case as we are updating the joints manually
        m_SkeletonInstance.BuildWorldPose();

    }
    else
    {
         // The skeleton isn't being tracked, so go to the default pose
         SetInDefaultPose();

         // Reset filter
         m_filterDoubleExponential.Reset();
    }

    for( DWORD i = 0; i < m_dwModelCount; ++i )
    {
        if( m_pModelInfos[i].pSkinnedMesh == NULL )
            continue;

        // Create the bone palettes for the model.
        m_SkeletonInstance.CreateBonePalette( i, m_pModelInfos[i].pBoneMatrixBufferVirtual, FALSE );
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: SetupMaterial()
// Desc: Loads the textures specified by a material attached to a mesh subset.
//--------------------------------------------------------------------------------------
VOID Sample::SetupMaterial( ModelInfo* pModelInfo, DWORD dwSubsetIndex )
{
    // Select the material for the subset index.
    const ATG::MeshMapping& mm = pModelInfo->pModel->GetMeshMapping( 0 );
    ATG::MaterialInstance* pMaterial = mm.Materials[ dwSubsetIndex ];

    // Load textures into D3D.
    ATG::Texture* pTextureResource = NULL;
	
	for( UINT i = 0; i < pMaterial->GetRawParameterCount(); i++ )
	{
		ATG::MaterialParameter param = pMaterial->GetRawParameter(i);
        pTextureResource = ( ATG::Texture* )param.pValue;
		if( pTextureResource != NULL)
		{
			if( !_wcsicmp( param.strName, L"DiffuseTexture" ) )
			{
				m_pd3dDevice->SetTexture( 0, pTextureResource->GetD3DTexture() );
			}
			else if( !_wcsicmp( param.strName, L"NormalMapTexture" ) )
			{
				m_pd3dDevice->SetTexture( 1, pTextureResource->GetD3DTexture() );
			}
			else if( !_wcsicmp( param.strName, L"AOTexture" ) )
			{
				m_pd3dDevice->SetTexture( 2, pTextureResource->GetD3DTexture() );
			}
		}
	}
}


//--------------------------------------------------------------------------------------
// Name: RenderStaticModel()
// Desc: Renders a static (non-skinned) model.
//--------------------------------------------------------------------------------------
VOID Sample::RenderStaticModel( ModelInfo* pModelInfo )
{
    assert( pModelInfo->pStaticMesh != NULL );

    // Get the world transform matrix that this model is attached to.
    // The animated world transform matrix is held by the skeleton instance.
    DWORD dwBoneIndex = pModelInfo->dwSkeletonBoneToStaticMeshBinding;
    XMMATRIX matWorld;
    if( dwBoneIndex >= 0 )
        matWorld = m_SkeletonInstance.m_WorldPose.LoadTransform( dwBoneIndex );
    else
        matWorld = XMMatrixIdentity();

    // Set the pixel shader.
    XMVECTOR vDummy;
    XMMATRIX matWorldInv = XMMatrixInverse( &vDummy, matWorld );
    XMVECTOR vLightDir = XMVector3TransformNormal( XMLoadFloat4( &m_DirectionalLightDirection ), matWorldInv );
    m_pd3dDevice->SetPixelShader( m_pPixelShaderCharacter );
    m_pd3dDevice->SetPixelShaderConstantF( 0, ( FLOAT* )&vLightDir, 1 );
    m_pd3dDevice->SetPixelShaderConstantF( 1, ( FLOAT* )&m_DirectionalLightColor, 1 );
    m_pd3dDevice->SetPixelShaderConstantF( 2, ( FLOAT* )&m_AmbientColor, 1 );
    XMVECTOR vCameraPos = XMVector3TransformNormal( m_matView.r[3], matWorldInv );
    m_pd3dDevice->SetVertexShaderConstantF( 4, ( FLOAT* )&vCameraPos, 1 );

    // Set the transform only vertex shader.
    m_pd3dDevice->SetVertexShader( m_pVertexShaderTransform );

    // Compute a world * view * projection matrix for this model.
    XMMATRIX matWVP = matWorld * m_matWVP;
    XMMATRIX matWVP_T = XMMatrixTranspose( matWVP );
    m_pd3dDevice->SetVertexShaderConstantF( 0, ( FLOAT* )&matWVP_T, 4 );

    // Render the mesh subsets.
    for( DWORD i = 0; i < pModelInfo->dwMeshSubsetCount; ++i )
    {
        SetupMaterial( pModelInfo, i );
        pModelInfo->pStaticMesh->RenderSubset( i, m_pd3dDevice );
    }
}


//--------------------------------------------------------------------------------------
// Name: RenderSkinnedModel()
// Desc: Renders a skinned model.  Several different code paths are in this method
//       to demonstrate different ways of skinning on the GPU.
//--------------------------------------------------------------------------------------
VOID Sample::RenderSkinnedModel( ModelInfo* pModelInfo )
{
    assert( pModelInfo->pSkinnedMesh != NULL );

    // Set a pixel shader.
    m_pd3dDevice->SetPixelShader( m_pPixelShaderCharacter );
    m_pd3dDevice->SetPixelShaderConstantF( 0, ( FLOAT* )&m_DirectionalLightDirection, 1 );
    m_pd3dDevice->SetPixelShaderConstantF( 1, ( FLOAT* )&m_DirectionalLightColor, 1 );
    m_pd3dDevice->SetPixelShaderConstantF( 2, ( FLOAT* )&m_AmbientColor, 1 );
    XMVECTOR vCameraPos = m_vEyePt;
    m_pd3dDevice->SetVertexShaderConstantF( 4, ( FLOAT* )&vCameraPos, 1 );

    // Set the camera matrix into vertex shader constants c0-c3.
    XMMATRIX matWVP_T = XMMatrixTranspose( m_matWVP );
    m_pd3dDevice->SetVertexShaderConstantF( 0, ( FLOAT* )&matWVP_T, 4 );

    // Set the shader constant skinning vertex shader.
    m_pd3dDevice->SetVertexShader( m_pVertexShaderSkinningConstants );

    FLOAT pZeros[960];
    ZeroMemory( pZeros, 960 * sizeof( FLOAT ) );
    m_pd3dDevice->SetVertexShaderConstantF( 12, pZeros, 240 );

    // Load the bone matrix palette into shader constants c12-c251.
    DWORD dwBindIndex = pModelInfo->dwSkeletonInstanceToSkinnedMeshBinding;
    DWORD dwPaletteSize = m_SkeletonInstance.m_pSkinnedMeshBindings[ dwBindIndex ].GetPaletteSize();
    dwPaletteSize = min( dwPaletteSize, ATG::g_dwMaxBoneCount );
    m_pd3dDevice->SetVertexShaderConstantF( 12, ( FLOAT* )pModelInfo->pBoneMatrixBufferVirtual,
                                            dwPaletteSize * 3 );

    // Render each subset of the mesh.
    for( DWORD i = 0; i < pModelInfo->dwMeshSubsetCount; ++i )
    {
        SetupMaterial( pModelInfo, i );
        pModelInfo->pSkinnedMesh->RenderSubset( i, m_pd3dDevice );
    }
}


//--------------------------------------------------------------------------------------
// Name: RenderSkeleton()
// Desc: Uses the ATG debug draw library to draw the animated skeleton.
//--------------------------------------------------------------------------------------
VOID Sample::RenderSkeleton()
{
    DWORD dwBoneCount = m_Skeleton.GetBoneCount();
    static const XMVECTOR vAxisZ = { 0, 0, 1, 0 };

    for( DWORD i = 0; i < dwBoneCount; ++i )
    {
        XMMATRIX matWorld = m_SkeletonInstance.m_WorldPose.LoadTransform( i );
        XMFLOAT3 Pos;
        XMVECTOR vBonePos = matWorld.r[3];
        XMStoreFloat3( &Pos, vBonePos );
        ATG::DebugDraw::DrawSphere( Pos, m_fBoneRadius, 0xFF808080 );
        INT iParentIndex = m_Skeleton.m_ParentIndex[i];
        if( iParentIndex != -1 )
        {
            XMMATRIX matParentWorld = m_SkeletonInstance.m_WorldPose.LoadTransform( iParentIndex );
            XMVECTOR vParentPos = matParentWorld.r[3];
            XMVECTOR vConeAxis = vParentPos - vBonePos;
            XMFLOAT3 ConeAxis;
            XMStoreFloat3( &ConeAxis, vConeAxis );
            ATG::DebugDraw::DrawConeWireframe( Pos, ConeAxis, m_fBoneRadius, 0.0f, 0xFF808080 );
        }
    }
}

VOID SetupArmorMaterial(D3DDevice* pd3dDevice)
{
    XMFLOAT4 vConstants[27];
    XMFLOAT4* diffuseColorBounceLight = &vConstants[0];
    XMFLOAT4* diffuseColorLow = &vConstants[1];
    XMFLOAT4* diffuseColorMid = &vConstants[2];
    XMFLOAT4* diffuseColorHigh = &vConstants[3];
    XMFLOAT4* diffuseColorRampMidpoint = &vConstants[4];
    XMFLOAT4* diffuseColorBounceLightPoint = &vConstants[5];
    XMFLOAT4* diffuseScaler = &vConstants[6];
    XMFLOAT4* specularColor = &vConstants[7];
    XMFLOAT4* specularPower = &vConstants[8];
    XMFLOAT4* specularScaler = &vConstants[9];
    XMFLOAT4* rimExponent = &vConstants[10];
    XMFLOAT4* rimColorSky = &vConstants[11];
    XMFLOAT4* rimColor = &vConstants[12];
    XMFLOAT4* tonemapAmount = &vConstants[13];
    XMFLOAT4* brightnessControl = &vConstants[14];
    XMFLOAT4* exposure = &vConstants[15];
    XMFLOAT4* normalIntensity = &vConstants[16];
    XMFLOAT4* aoAmount = &vConstants[17];
    XMFLOAT4* lightDir = &vConstants[18];
    XMFLOAT4* lightColor = &vConstants[19];

    BOOL bConstants[6];
    BOOL* useNormalMap = &bConstants[0];
    BOOL* useDiffuseMap = &bConstants[1];
    BOOL* useColorRampDiffuse = &bConstants[2];
    BOOL* useAO = &bConstants[3];
    BOOL* useRimLightSky = &bConstants[4];
    BOOL* useRimLight = &bConstants[5];
    
    *tonemapAmount = XMFLOAT4(.083,.083,.083,.083);
    *brightnessControl = XMFLOAT4(-.00,-.00,-.00,-.00);

    *exposure = XMFLOAT4(.059,.059,.059,.059);
    *diffuseScaler = XMFLOAT4(1,1,1,1);
    *specularColor = XMFLOAT4(1.0,.997,.882,1.0);
    *specularPower = XMFLOAT4(130.763,130.763,130.763,1.0);
    *specularScaler = XMFLOAT4(1,1,1,1);
    *diffuseColorBounceLight = XMFLOAT4(0,0,0,0);
    *diffuseColorLow = XMFLOAT4(0,0,0,0);
    *diffuseColorMid = XMFLOAT4(.176,.007,.085,1.0);
    *diffuseColorHigh = XMFLOAT4(.647,.329,.216,1.0);
    *diffuseColorRampMidpoint = XMFLOAT4(.852,.852,.852,.852);
    *diffuseColorBounceLightPoint = XMFLOAT4(-.077,-.077,-.077,-.077);
    *normalIntensity =XMFLOAT4(.870,.870,.870,.870);
    *rimExponent = XMFLOAT4(1.331,1.331,1.331,1.331);
    *rimColorSky = XMFLOAT4(.75,.9,1.0,1);
    *rimColor = XMFLOAT4(.404,.404,.404,1.0);
    *aoAmount = XMFLOAT4(0,0,0,0);
    *lightDir = XMFLOAT4(0,-1,1,0);
    *lightColor = XMFLOAT4(1,1,1,1);

    *useDiffuseMap = false;
    *useAO = true;
    *useColorRampDiffuse = true;
    *useNormalMap = true;
    *useRimLightSky = true;
    *useRimLight = true;

    pd3dDevice->SetPixelShaderConstantF(12, (float*)vConstants, 27);
    pd3dDevice->SetPixelShaderConstantB(1, bConstants, 6);
}

VOID SetupArmsMaterial( D3DDevice* pd3dDevice )
{
	XMFLOAT4 vConstants[27];
    XMFLOAT4* diffuseColorBounceLight = &vConstants[0];
    XMFLOAT4* diffuseColorLow = &vConstants[1];
    XMFLOAT4* diffuseColorMid = &vConstants[2];
    XMFLOAT4* diffuseColorHigh = &vConstants[3];
    XMFLOAT4* diffuseColorRampMidpoint = &vConstants[4];
    XMFLOAT4* diffuseColorBounceLightPoint = &vConstants[5];
    XMFLOAT4* diffuseScaler = &vConstants[6];
    XMFLOAT4* specularColor = &vConstants[7];
    XMFLOAT4* specularPower = &vConstants[8];
    XMFLOAT4* specularScaler = &vConstants[9];
    XMFLOAT4* rimExponent = &vConstants[10];
    XMFLOAT4* rimColorSky = &vConstants[11];
    XMFLOAT4* rimColor = &vConstants[12];
    XMFLOAT4* tonemapAmount = &vConstants[13];
    XMFLOAT4* brightnessControl = &vConstants[14];
    XMFLOAT4* exposure = &vConstants[15];
    XMFLOAT4* normalIntensity = &vConstants[16];
    XMFLOAT4* aoAmount = &vConstants[17];
    XMFLOAT4* lightDir = &vConstants[18];
    XMFLOAT4* lightColor = &vConstants[19];

    BOOL bConstants[6];
    BOOL* useNormalMap = &bConstants[0];
    BOOL* useDiffuseMap = &bConstants[1];
    BOOL* useColorRampDiffuse = &bConstants[2];
    BOOL* useAO = &bConstants[3];
    BOOL* useRimLightSky = &bConstants[4];
    BOOL* useRimLight = &bConstants[5];

	*tonemapAmount = XMFLOAT4(0,0,0,0);
	*brightnessControl = XMFLOAT4(-.1,-.1,-.1,-.1);
	*exposure = XMFLOAT4(.059,.059,.059,.059);

	*diffuseScaler = XMFLOAT4(1.06,1.06,1.06,1.06);
	*specularColor = XMFLOAT4(.2,.2,.2,1.0);
	*specularPower = XMFLOAT4(23.633,23.633,23.633,23.633);
	*specularScaler = XMFLOAT4(0,0,0,0);
	*diffuseColorBounceLight = XMFLOAT4(.156,.122,.107,1.0);
	*diffuseColorLow = XMFLOAT4(.234,.189,.138,1.0);
	*diffuseColorMid = XMFLOAT4(.706,.546,.291,1.0);
	*diffuseColorHigh = XMFLOAT4(1,1,1,1.0);
	*diffuseColorRampMidpoint = XMFLOAT4(.716,.716,.716,.716);
	*diffuseColorBounceLightPoint = XMFLOAT4(.361,.361,.361,.361);
	*normalIntensity =XMFLOAT4(1,1,1,1);
	*rimExponent = XMFLOAT4(3.521,3.521,3.521,3.521);
	*rimColorSky = XMFLOAT4(.633,.760,.844,1.0);
	*rimColor = XMFLOAT4(.457,.549,.610,1.0);
	*aoAmount = XMFLOAT4(1.066,1.066,1.066,1.066);
	*lightDir = XMFLOAT4(0,-1,1,0);
	*lightColor = XMFLOAT4(1,1,1,1);

	*useDiffuseMap = false;
	*useAO = true;
	*useColorRampDiffuse = true;
	*useNormalMap = true;
	*useRimLightSky = true;
	*useRimLight = true;
    
    pd3dDevice->SetPixelShaderConstantF(12, (float*)vConstants, 27);
    pd3dDevice->SetPixelShaderConstantB(1, bConstants, 6);
}

VOID SetupHeadMaterial( D3DDevice* pd3dDevice )
{
	XMFLOAT4 vConstants[27];
    XMFLOAT4* diffuseColorBounceLight = &vConstants[0];
    XMFLOAT4* diffuseColorLow = &vConstants[1];
    XMFLOAT4* diffuseColorMid = &vConstants[2];
    XMFLOAT4* diffuseColorHigh = &vConstants[3];
    XMFLOAT4* diffuseColorRampMidpoint = &vConstants[4];
    XMFLOAT4* diffuseColorBounceLightPoint = &vConstants[5];
    XMFLOAT4* diffuseScaler = &vConstants[6];
    XMFLOAT4* specularColor = &vConstants[7];
    XMFLOAT4* specularPower = &vConstants[8];
    XMFLOAT4* specularScaler = &vConstants[9];
    XMFLOAT4* rimExponent = &vConstants[10];
    XMFLOAT4* rimColorSky = &vConstants[11];
    XMFLOAT4* rimColor = &vConstants[12];
    XMFLOAT4* tonemapAmount = &vConstants[13];
    XMFLOAT4* brightnessControl = &vConstants[14];
    XMFLOAT4* exposure = &vConstants[15];
    XMFLOAT4* normalIntensity = &vConstants[16];
    XMFLOAT4* aoAmount = &vConstants[17];
    XMFLOAT4* lightDir = &vConstants[18];
    XMFLOAT4* lightColor = &vConstants[19];

    BOOL bConstants[6];
    BOOL* useNormalMap = &bConstants[0];
    BOOL* useDiffuseMap = &bConstants[1];
    BOOL* useColorRampDiffuse = &bConstants[2];
    BOOL* useAO = &bConstants[3];
    BOOL* useRimLightSky = &bConstants[4];
    BOOL* useRimLight = &bConstants[5];
    *tonemapAmount = XMFLOAT4(0,0,0,0);
    *brightnessControl = XMFLOAT4(.3,.3,.3,.3);

    *exposure = XMFLOAT4(.888,.888,.888,.888);

    *diffuseScaler = XMFLOAT4(1.06,1.06,1.06,1.06);
    *specularColor = XMFLOAT4(0,0,0,0);
    *specularPower = XMFLOAT4(30,30,30,30);
    *specularScaler = XMFLOAT4(.177,.177,.177,.177);
    *diffuseColorBounceLight = XMFLOAT4(.159,.148,.125,0);
    *diffuseColorLow = XMFLOAT4(.162,.163,.153,0);
    *diffuseColorMid = XMFLOAT4(.255,.180,.060,0);
    *diffuseColorHigh = XMFLOAT4(.801,.791,.754,0);
    *diffuseColorRampMidpoint = XMFLOAT4(.419,.419,.419,.419);
    *diffuseColorBounceLightPoint = XMFLOAT4(.037,.037,.037,.037);
    *normalIntensity =XMFLOAT4(1,1,1,1);
    *rimExponent = XMFLOAT4(2.219,2.219,2.219,2.219);
    *rimColorSky = XMFLOAT4(.574,.689,.766,1.0);
    *rimColor = XMFLOAT4(.670,.804,.894,1.0);
    *aoAmount = XMFLOAT4(1.065,1.065,1.065,1.065);
    *lightDir = XMFLOAT4(0,-1,1,0);
    *lightColor = XMFLOAT4(1,1,1,1);

    *useDiffuseMap = true;
    *useAO = true;
    *useColorRampDiffuse = true;
    *useNormalMap = true;
    *useRimLightSky = true;
    *useRimLight = true;
    pd3dDevice->SetPixelShaderConstantF(12, (float*)vConstants, 27);
    pd3dDevice->SetPixelShaderConstantB(1, bConstants, 6);
}

VOID SetupBeardMaterial( D3DDevice* pd3dDevice )
{
	XMFLOAT4 vConstants[27];
    XMFLOAT4* diffuseColorBounceLight = &vConstants[0];
    XMFLOAT4* diffuseColorLow = &vConstants[1];
    XMFLOAT4* diffuseColorMid = &vConstants[2];
    XMFLOAT4* diffuseColorHigh = &vConstants[3];
    XMFLOAT4* diffuseColorRampMidpoint = &vConstants[4];
    XMFLOAT4* diffuseColorBounceLightPoint = &vConstants[5];
    XMFLOAT4* diffuseScaler = &vConstants[6];
    XMFLOAT4* specularColor = &vConstants[7];
    XMFLOAT4* specularPower = &vConstants[8];
    XMFLOAT4* specularScaler = &vConstants[9];
    XMFLOAT4* rimExponent = &vConstants[10];
    XMFLOAT4* rimColorSky = &vConstants[11];
    XMFLOAT4* rimColor = &vConstants[12];
    XMFLOAT4* tonemapAmount = &vConstants[13];
    XMFLOAT4* brightnessControl = &vConstants[14];
    XMFLOAT4* exposure = &vConstants[15];
    XMFLOAT4* normalIntensity = &vConstants[16];
    XMFLOAT4* aoAmount = &vConstants[17];
    XMFLOAT4* lightDir = &vConstants[18];
    XMFLOAT4* lightColor = &vConstants[19];

    BOOL bConstants[6];
    BOOL* useNormalMap = &bConstants[0];
    BOOL* useDiffuseMap = &bConstants[1];
    BOOL* useColorRampDiffuse = &bConstants[2];
    BOOL* useAO = &bConstants[3];
    BOOL* useRimLightSky = &bConstants[4];
    BOOL* useRimLight = &bConstants[5];

	*tonemapAmount = XMFLOAT4(0,0,0,0);
	*brightnessControl = XMFLOAT4(-.1,-.1,-.1,-.1);
	*exposure = XMFLOAT4(0,0,0,0);

	*diffuseScaler = XMFLOAT4(1,1,1,1);
	*specularColor = XMFLOAT4(.657, .766, .822, 1.0);
	*specularPower = XMFLOAT4(30,30,30,30);
	*specularScaler = XMFLOAT4(.237,.237, .237, .237);
	*diffuseColorBounceLight = XMFLOAT4(.02,.01,.01,1.0);
	*diffuseColorLow = XMFLOAT4(0,0,0,1.0);
	*diffuseColorMid = XMFLOAT4(.1,.1,.1,1.0);
	*diffuseColorHigh = XMFLOAT4(.305,.305,.305,1.0);
	*diffuseColorRampMidpoint = XMFLOAT4(.834,.834,.834,.834);
	*diffuseColorBounceLightPoint = XMFLOAT4(.136,.136,.136,.136);
	*normalIntensity =XMFLOAT4(1,1,1,1);
	*rimExponent = XMFLOAT4(3.05,3.05,3.05,3.05);
	*rimColorSky = XMFLOAT4(.665,.798,.887,1.0);
	*rimColor = XMFLOAT4(.574,.689,.766,1.0);
	*aoAmount = XMFLOAT4(1.124,1.124,1.124,1.124);
	*lightDir = XMFLOAT4(0,-1,1,0);
	*lightColor = XMFLOAT4(1,1,1,1);

	*useDiffuseMap = false;
	*useAO = true;
	*useColorRampDiffuse = true;
	*useNormalMap = true;
	*useRimLightSky = true;
	*useRimLight = true;
    
    pd3dDevice->SetPixelShaderConstantF(12, (float*)vConstants, 27);
    pd3dDevice->SetPixelShaderConstantB(1, bConstants, 6);
}

VOID SetupLegsMaterial( D3DDevice* pd3dDevice )
{
	XMFLOAT4 vConstants[27];
    XMFLOAT4* diffuseColorBounceLight = &vConstants[0];
    XMFLOAT4* diffuseColorLow = &vConstants[1];
    XMFLOAT4* diffuseColorMid = &vConstants[2];
    XMFLOAT4* diffuseColorHigh = &vConstants[3];
    XMFLOAT4* diffuseColorRampMidpoint = &vConstants[4];
    XMFLOAT4* diffuseColorBounceLightPoint = &vConstants[5];
    XMFLOAT4* diffuseScaler = &vConstants[6];
    XMFLOAT4* specularColor = &vConstants[7];
    XMFLOAT4* specularPower = &vConstants[8];
    XMFLOAT4* specularScaler = &vConstants[9];
    XMFLOAT4* rimExponent = &vConstants[10];
    XMFLOAT4* rimColorSky = &vConstants[11];
    XMFLOAT4* rimColor = &vConstants[12];
    XMFLOAT4* tonemapAmount = &vConstants[13];
    XMFLOAT4* brightnessControl = &vConstants[14];
    XMFLOAT4* exposure = &vConstants[15];
    XMFLOAT4* normalIntensity = &vConstants[16];
    XMFLOAT4* aoAmount = &vConstants[17];
    XMFLOAT4* lightDir = &vConstants[18];
    XMFLOAT4* lightColor = &vConstants[19];

    BOOL bConstants[6];
    BOOL* useNormalMap = &bConstants[0];
    BOOL* useDiffuseMap = &bConstants[1];
    BOOL* useColorRampDiffuse = &bConstants[2];
    BOOL* useAO = &bConstants[3];
    BOOL* useRimLightSky = &bConstants[4];
    BOOL* useRimLight = &bConstants[5];

	*tonemapAmount = XMFLOAT4(0,0,0,0);
	*brightnessControl = XMFLOAT4(-.01,-.01,-.01,-.01);
	*exposure = XMFLOAT4(.177,.177,.177,.177);

	*diffuseScaler = XMFLOAT4(1,1,1,1);
	*specularColor = XMFLOAT4(.5,.5,.5,1.0);
	*specularPower = XMFLOAT4(30,30,30,30);
	*specularScaler = XMFLOAT4(1,1,1,1);
	*diffuseColorBounceLight = XMFLOAT4(.121,.004,0,1.0);
	*diffuseColorLow = XMFLOAT4(.204,.165,.113,1.0);
	*diffuseColorMid = XMFLOAT4(.627,.486,.267,1.0);
	*diffuseColorHigh = XMFLOAT4(.914,.843,.729,1.0);
	*diffuseColorRampMidpoint = XMFLOAT4(.5,.5,.5,.5);
	*diffuseColorBounceLightPoint = XMFLOAT4(.1,.1,.1,.1);
	*normalIntensity =XMFLOAT4(1,1,1,1);
	*rimExponent = XMFLOAT4(2.2,2.2,2.2,2.2);
	*rimColorSky = XMFLOAT4(.75,.9,1.0,1.0);
	*rimColor = XMFLOAT4(.75,.9,1.1,1.0);
	*aoAmount = XMFLOAT4(1.066,1.066,1.066,1.066);
	*lightDir = XMFLOAT4(0,-1,1,0);
	*lightColor = XMFLOAT4(1,1,1,1);

	*useDiffuseMap = false;
	*useAO = true;
	*useColorRampDiffuse = true;
	*useNormalMap = true;
	*useRimLightSky = false;
	*useRimLight = false;
    
    pd3dDevice->SetPixelShaderConstantF(12, (float*)vConstants, 27);
    pd3dDevice->SetPixelShaderConstantB(1, bConstants, 6);
}


VOID SetupBootsMaterial( D3DDevice* pd3dDevice )
{
	XMFLOAT4 vConstants[27];
    XMFLOAT4* diffuseColorBounceLight = &vConstants[0];
    XMFLOAT4* diffuseColorLow = &vConstants[1];
    XMFLOAT4* diffuseColorMid = &vConstants[2];
    XMFLOAT4* diffuseColorHigh = &vConstants[3];
    XMFLOAT4* diffuseColorRampMidpoint = &vConstants[4];
    XMFLOAT4* diffuseColorBounceLightPoint = &vConstants[5];
    XMFLOAT4* diffuseScaler = &vConstants[6];
    XMFLOAT4* specularColor = &vConstants[7];
    XMFLOAT4* specularPower = &vConstants[8];
    XMFLOAT4* specularScaler = &vConstants[9];
    XMFLOAT4* rimExponent = &vConstants[10];
    XMFLOAT4* rimColorSky = &vConstants[11];
    XMFLOAT4* rimColor = &vConstants[12];
    XMFLOAT4* tonemapAmount = &vConstants[13];
    XMFLOAT4* brightnessControl = &vConstants[14];
    XMFLOAT4* exposure = &vConstants[15];
    XMFLOAT4* normalIntensity = &vConstants[16];
    XMFLOAT4* aoAmount = &vConstants[17];
    XMFLOAT4* lightDir = &vConstants[18];
    XMFLOAT4* lightColor = &vConstants[19];

    BOOL bConstants[6];
    BOOL* useNormalMap = &bConstants[0];
    BOOL* useDiffuseMap = &bConstants[1];
    BOOL* useColorRampDiffuse = &bConstants[2];
    BOOL* useAO = &bConstants[3];
    BOOL* useRimLightSky = &bConstants[4];
    BOOL* useRimLight = &bConstants[5];
	*tonemapAmount = XMFLOAT4(0,0,0,0);
	*brightnessControl = XMFLOAT4(-.053,-.053,-.053,-.053);
	*exposure = XMFLOAT4(1.006,1.006,1.006,1.006);

	*diffuseScaler = XMFLOAT4(1,1,1,1);
	*specularColor = XMFLOAT4(.695,.695,.695,1.0);
	*specularPower = XMFLOAT4(25.142,25.142,25.142,25.142);
	*specularScaler = XMFLOAT4(.414,.414,.414,.414);
	*diffuseColorBounceLight = XMFLOAT4(.015,.015,.015,1.0);
	*diffuseColorLow = XMFLOAT4(0,0,0,1.0);
	*diffuseColorMid = XMFLOAT4(.213,.093,.063,1.0);
	*diffuseColorHigh = XMFLOAT4(.411,.294,.201,1.0);
	*diffuseColorRampMidpoint = XMFLOAT4(.882,.882,.882,.882);
	*diffuseColorBounceLightPoint = XMFLOAT4(.26,.26,.26,.26);
	*normalIntensity =XMFLOAT4(1.012,1.012,1.012,1.012);
	*rimExponent = XMFLOAT4(2.959,2.959,2.959,2.959);
	*rimColorSky = XMFLOAT4(.410,.419,.546,1);
	*rimColor = XMFLOAT4(.505,.606,.674,1.0);
	*aoAmount = XMFLOAT4(1.066,1.066,1.066,1.066);
	*lightDir = XMFLOAT4(0,-1,1,0);
	*lightColor = XMFLOAT4(1,1,1,1);

	*useDiffuseMap = false;
	*useAO = true;
	*useColorRampDiffuse = true;
	*useNormalMap = true;
	*useRimLightSky = true;
	*useRimLight = true;
    pd3dDevice->SetPixelShaderConstantF(12, (float*)vConstants, 27);
    pd3dDevice->SetPixelShaderConstantB(1, bConstants, 6);
}

VOID SetupGlovesMaterial( D3DDevice* pd3dDevice )
{
	XMFLOAT4 vConstants[27];
    XMFLOAT4* diffuseColorBounceLight = &vConstants[0];
    XMFLOAT4* diffuseColorLow = &vConstants[1];
    XMFLOAT4* diffuseColorMid = &vConstants[2];
    XMFLOAT4* diffuseColorHigh = &vConstants[3];
    XMFLOAT4* diffuseColorRampMidpoint = &vConstants[4];
    XMFLOAT4* diffuseColorBounceLightPoint = &vConstants[5];
    XMFLOAT4* diffuseScaler = &vConstants[6];
    XMFLOAT4* specularColor = &vConstants[7];
    XMFLOAT4* specularPower = &vConstants[8];
    XMFLOAT4* specularScaler = &vConstants[9];
    XMFLOAT4* rimExponent = &vConstants[10];
    XMFLOAT4* rimColorSky = &vConstants[11];
    XMFLOAT4* rimColor = &vConstants[12];
    XMFLOAT4* tonemapAmount = &vConstants[13];
    XMFLOAT4* brightnessControl = &vConstants[14];
    XMFLOAT4* exposure = &vConstants[15];
    XMFLOAT4* normalIntensity = &vConstants[16];
    XMFLOAT4* aoAmount = &vConstants[17];
    XMFLOAT4* lightDir = &vConstants[18];
    XMFLOAT4* lightColor = &vConstants[19];

    BOOL bConstants[6];
    BOOL* useNormalMap = &bConstants[0];
    BOOL* useDiffuseMap = &bConstants[1];
    BOOL* useColorRampDiffuse = &bConstants[2];
    BOOL* useAO = &bConstants[3];
    BOOL* useRimLightSky = &bConstants[4];
    BOOL* useRimLight = &bConstants[5];

	*tonemapAmount = XMFLOAT4(0,0,0,0);
	*brightnessControl = XMFLOAT4(-.01,-.01,-.01,-.01);
	*exposure = XMFLOAT4(.888,.888,.888,.888);

	*diffuseScaler = XMFLOAT4(1,1,1,1);
	*specularColor = XMFLOAT4(.638,.638,.638,.638);
	*specularPower = XMFLOAT4(16.089,16.089,16.089,16.089);
	*specularScaler = XMFLOAT4(.592,.592,.592,.592);
	*diffuseColorBounceLight = XMFLOAT4(.015,.015,.015,1.0);
	*diffuseColorLow = XMFLOAT4(.006,0,0,1.0);
	*diffuseColorMid = XMFLOAT4(.275,.058,.011,1);
	*diffuseColorHigh = XMFLOAT4(.275,.208,.167,1);
	*diffuseColorRampMidpoint = XMFLOAT4(.763,.763,.763,.763);
	*diffuseColorBounceLightPoint = XMFLOAT4(.432,.432,.432,.432);
	*normalIntensity =XMFLOAT4(1,1,1,1);
	*rimExponent = XMFLOAT4(3.402,3.402,3.402,3.402);
	*rimColorSky = XMFLOAT4(.436,.523,.582,1.0);
	*rimColor = XMFLOAT4(.468,.562,.624,1.0);
	*aoAmount = XMFLOAT4(1.686,1.686,1.686,1.686);
	*lightDir = XMFLOAT4(0,-1,1,0);
	*lightColor = XMFLOAT4(1,1,1,1);

	*useDiffuseMap = false;
	*useAO = true;
	*useColorRampDiffuse = true;
	*useNormalMap = true;
	*useRimLightSky = true;
	*useRimLight = true;
    pd3dDevice->SetPixelShaderConstantF(12, (float*)vConstants, 27);
    pd3dDevice->SetPixelShaderConstantB(1, bConstants, 6);
}

VOID SetupShirtMaterial( D3DDevice* pd3dDevice )
{
	XMFLOAT4 vConstants[27];
    XMFLOAT4* diffuseColorBounceLight = &vConstants[0];
    XMFLOAT4* diffuseColorLow = &vConstants[1];
    XMFLOAT4* diffuseColorMid = &vConstants[2];
    XMFLOAT4* diffuseColorHigh = &vConstants[3];
    XMFLOAT4* diffuseColorRampMidpoint = &vConstants[4];
    XMFLOAT4* diffuseColorBounceLightPoint = &vConstants[5];
    XMFLOAT4* diffuseScaler = &vConstants[6];
    XMFLOAT4* specularColor = &vConstants[7];
    XMFLOAT4* specularPower = &vConstants[8];
    XMFLOAT4* specularScaler = &vConstants[9];
    XMFLOAT4* rimExponent = &vConstants[10];
    XMFLOAT4* rimColorSky = &vConstants[11];
    XMFLOAT4* rimColor = &vConstants[12];
    XMFLOAT4* tonemapAmount = &vConstants[13];
    XMFLOAT4* brightnessControl = &vConstants[14];
    XMFLOAT4* exposure = &vConstants[15];
    XMFLOAT4* normalIntensity = &vConstants[16];
    XMFLOAT4* aoAmount = &vConstants[17];
    XMFLOAT4* lightDir = &vConstants[18];
    XMFLOAT4* lightColor = &vConstants[19];

    BOOL bConstants[6];
    BOOL* useNormalMap = &bConstants[0];
    BOOL* useDiffuseMap = &bConstants[1];
    BOOL* useColorRampDiffuse = &bConstants[2];
    BOOL* useAO = &bConstants[3];
    BOOL* useRimLightSky = &bConstants[4];
    BOOL* useRimLight = &bConstants[5];

	*tonemapAmount = XMFLOAT4(0,0,0,0);
	*brightnessControl = XMFLOAT4(0,0,0,0);
	*exposure = XMFLOAT4(.532,.532,.532,.532);

	*diffuseScaler = XMFLOAT4(1,1,1,1);
	*specularColor = XMFLOAT4(.092,.092,.092,1.0);
	*specularPower = XMFLOAT4(1,1,1,1);
	*specularScaler = XMFLOAT4(1.1,1.1,1.1,1.1);
	*diffuseColorBounceLight = XMFLOAT4(0,0,0,1.0);
	*diffuseColorLow = XMFLOAT4(.085, 0, .007,1.0);
	*diffuseColorMid = XMFLOAT4(.440,.440,.440,1.0);
	*diffuseColorHigh = XMFLOAT4(.593,.576, .794,1.0);
	*diffuseColorRampMidpoint = XMFLOAT4(.686,.686,.686,.686);
	*diffuseColorBounceLightPoint = XMFLOAT4(.2,.2,.2,.2);
	*normalIntensity =XMFLOAT4(1,1,1,1);
	*rimExponent = XMFLOAT4(1.302,1.302,1.302,1.302);
	*rimColorSky = XMFLOAT4(.622,.747,.830,1.0);
	*rimColor = XMFLOAT4(.559,.670,.745,1.0);
	*aoAmount = XMFLOAT4(1.864,1.864,1.864,1.864);
	*lightDir = XMFLOAT4(0,-1,1,0);
	*lightColor = XMFLOAT4(1,1,1,1);

	*useDiffuseMap = true;
	*useAO = true;
	*useColorRampDiffuse = true;
	*useNormalMap = true;
	*useRimLightSky = true;
	*useRimLight = true;
    pd3dDevice->SetPixelShaderConstantF(12, (float*)vConstants, 27);
    pd3dDevice->SetPixelShaderConstantB(1, bConstants, 6);
}

VOID SetupPantsMaterial( D3DDevice* pd3dDevice )
{
	XMFLOAT4 vConstants[27];
    XMFLOAT4* diffuseColorBounceLight = &vConstants[0];
    XMFLOAT4* diffuseColorLow = &vConstants[1];
    XMFLOAT4* diffuseColorMid = &vConstants[2];
    XMFLOAT4* diffuseColorHigh = &vConstants[3];
    XMFLOAT4* diffuseColorRampMidpoint = &vConstants[4];
    XMFLOAT4* diffuseColorBounceLightPoint = &vConstants[5];
    XMFLOAT4* diffuseScaler = &vConstants[6];
    XMFLOAT4* specularColor = &vConstants[7];
    XMFLOAT4* specularPower = &vConstants[8];
    XMFLOAT4* specularScaler = &vConstants[9];
    XMFLOAT4* rimExponent = &vConstants[10];
    XMFLOAT4* rimColorSky = &vConstants[11];
    XMFLOAT4* rimColor = &vConstants[12];
    XMFLOAT4* tonemapAmount = &vConstants[13];
    XMFLOAT4* brightnessControl = &vConstants[14];
    XMFLOAT4* exposure = &vConstants[15];
    XMFLOAT4* normalIntensity = &vConstants[16];
    XMFLOAT4* aoAmount = &vConstants[17];
    XMFLOAT4* lightDir = &vConstants[18];
    XMFLOAT4* lightColor = &vConstants[19];

    BOOL bConstants[6];
    BOOL* useNormalMap = &bConstants[0];
    BOOL* useDiffuseMap = &bConstants[1];
    BOOL* useColorRampDiffuse = &bConstants[2];
    BOOL* useAO = &bConstants[3];
    BOOL* useRimLightSky = &bConstants[4];
    BOOL* useRimLight = &bConstants[5];

	*tonemapAmount = XMFLOAT4(0,0,0,0);
	*brightnessControl = XMFLOAT4(0,0,0,0);
	*exposure = XMFLOAT4(.532,.532,.532,.532);

	*diffuseScaler = XMFLOAT4(1,1,1,1);
	*specularColor = XMFLOAT4(.092,.092,.092,1.0);
	*specularPower = XMFLOAT4(1,1,1,1);
	*specularScaler = XMFLOAT4(1.1,1.1,1.1,1.1);
	*diffuseColorBounceLight = XMFLOAT4(0,0,0,1.0);
	*diffuseColorLow = XMFLOAT4(.085, 0, .007,1.0);
    *diffuseColorMid = XMFLOAT4(.048,.049,.490,1);
	*diffuseColorHigh = XMFLOAT4(.593,.576, .794,1.0);
	*diffuseColorRampMidpoint = XMFLOAT4(.686,.686,.686,.686);
	*diffuseColorBounceLightPoint = XMFLOAT4(.2,.2,.2,.2);
	*normalIntensity =XMFLOAT4(1,1,1,1);
	*rimExponent = XMFLOAT4(1.302,1.302,1.302,1.302);
	*rimColorSky = XMFLOAT4(.622,.747,.830,1.0);
	*rimColor = XMFLOAT4(.559,.670,.745,1.0);
	*aoAmount = XMFLOAT4(1.864,1.864,1.864,1.864);
	*lightDir = XMFLOAT4(0,-1,1,0);
	*lightColor = XMFLOAT4(1,1,1,1);

	*useDiffuseMap = false;
	*useAO = true;
	*useColorRampDiffuse = true;
	*useNormalMap = true;
	*useRimLightSky = true;
	*useRimLight = true;


    pd3dDevice->SetPixelShaderConstantF(12, (float*)vConstants, 27);
    pd3dDevice->SetPixelShaderConstantB(1, bConstants, 6);
}

VOID SetupEyesMaterial( D3DDevice* pd3dDevice )
{
	XMFLOAT4 vConstants[27];
    XMFLOAT4* diffuseColorBounceLight = &vConstants[0];
    XMFLOAT4* diffuseColorLow = &vConstants[1];
    XMFLOAT4* diffuseColorMid = &vConstants[2];
    XMFLOAT4* diffuseColorHigh = &vConstants[3];
    XMFLOAT4* diffuseColorRampMidpoint = &vConstants[4];
    XMFLOAT4* diffuseColorBounceLightPoint = &vConstants[5];
    XMFLOAT4* diffuseScaler = &vConstants[6];
    XMFLOAT4* specularColor = &vConstants[7];
    XMFLOAT4* specularPower = &vConstants[8];
    XMFLOAT4* specularScaler = &vConstants[9];
    XMFLOAT4* rimExponent = &vConstants[10];
    XMFLOAT4* rimColorSky = &vConstants[11];
    XMFLOAT4* rimColor = &vConstants[12];
    XMFLOAT4* tonemapAmount = &vConstants[13];
    XMFLOAT4* brightnessControl = &vConstants[14];
    XMFLOAT4* exposure = &vConstants[15];
    XMFLOAT4* normalIntensity = &vConstants[16];
    XMFLOAT4* aoAmount = &vConstants[17];
    XMFLOAT4* lightDir = &vConstants[18];
    XMFLOAT4* lightColor = &vConstants[19];

    BOOL bConstants[6];
    BOOL* useNormalMap = &bConstants[0];
    BOOL* useDiffuseMap = &bConstants[1];
    BOOL* useColorRampDiffuse = &bConstants[2];
    BOOL* useAO = &bConstants[3];
    BOOL* useRimLightSky = &bConstants[4];
    BOOL* useRimLight = &bConstants[5];
				
    *tonemapAmount = XMFLOAT4(0,0,0,0);
	*brightnessControl = XMFLOAT4(-.1,-.1,-.1,-.1);
	*exposure = XMFLOAT4(0,0,0,0);

	*diffuseScaler = XMFLOAT4(.888,.888,.888,.888);
	*specularColor = XMFLOAT4(1,1,1,1.0);
	*specularPower = XMFLOAT4(190,190,190,190);
	*specularScaler = XMFLOAT4(1.716,1.716,1.716,1.716);
	*diffuseColorBounceLight = XMFLOAT4(0,0,0,1.0);
	*diffuseColorLow = XMFLOAT4(0.021,0.021,0.021,1.0);
	*diffuseColorMid = XMFLOAT4(.298,.298,.298,.298);
	*diffuseColorHigh = XMFLOAT4(1,1,1,1);
	*diffuseColorRampMidpoint = XMFLOAT4(.935,.935,.935,.935);
	*diffuseColorBounceLightPoint = XMFLOAT4(-.302,-.302,-.302,-.302);
	*normalIntensity =XMFLOAT4(0,0,0,0);
	*rimExponent = XMFLOAT4(2.959,2.959,2.959,2.959);
	*rimColorSky = XMFLOAT4(.410,.419,.546,1.0);
	*rimColor = XMFLOAT4(.505,.606,.674,1.0);
	*aoAmount = XMFLOAT4(0,0,0,0);
	*lightDir = XMFLOAT4(0,-1,1,0);
	*lightColor = XMFLOAT4(1,1,1,1);

	*useDiffuseMap = true;
	*useAO = true;
	*useColorRampDiffuse = true;
	*useNormalMap = true;
	*useRimLightSky = false;
	*useRimLight = false;

    pd3dDevice->SetPixelShaderConstantF(12, (float*)vConstants, 27);
    pd3dDevice->SetPixelShaderConstantB(1, bConstants, 6);
}

VOID SetupDefaultMaterial( D3DDevice* pd3dDevice )
{
	XMFLOAT4 vConstants[27];
    XMFLOAT4* diffuseColorBounceLight = &vConstants[0];
    XMFLOAT4* diffuseColorLow = &vConstants[1];
    XMFLOAT4* diffuseColorMid = &vConstants[2];
    XMFLOAT4* diffuseColorHigh = &vConstants[3];
    XMFLOAT4* diffuseColorRampMidpoint = &vConstants[4];
    XMFLOAT4* diffuseColorBounceLightPoint = &vConstants[5];
    XMFLOAT4* diffuseScaler = &vConstants[6];
    XMFLOAT4* specularColor = &vConstants[7];
    XMFLOAT4* specularPower = &vConstants[8];
    XMFLOAT4* specularScaler = &vConstants[9];
    XMFLOAT4* rimExponent = &vConstants[10];
    XMFLOAT4* rimColorSky = &vConstants[11];
    XMFLOAT4* rimColor = &vConstants[12];
    XMFLOAT4* tonemapAmount = &vConstants[13];
    XMFLOAT4* brightnessControl = &vConstants[14];
    XMFLOAT4* exposure = &vConstants[15];
    XMFLOAT4* normalIntensity = &vConstants[16];
    XMFLOAT4* aoAmount = &vConstants[17];
    XMFLOAT4* lightDir = &vConstants[18];
    XMFLOAT4* lightColor = &vConstants[19];

    BOOL bConstants[6];
    BOOL* useNormalMap = &bConstants[0];
    BOOL* useDiffuseMap = &bConstants[1];
    BOOL* useColorRampDiffuse = &bConstants[2];
    BOOL* useAO = &bConstants[3];
    BOOL* useRimLightSky = &bConstants[4];
    BOOL* useRimLight = &bConstants[5];

    
    *tonemapAmount = XMFLOAT4(0,0,0,0);
    *brightnessControl = XMFLOAT4(0,0,0,0);
    *exposure = XMFLOAT4(0,0,0,0);
    *diffuseScaler = XMFLOAT4(1,1,1,1);
    *specularColor = XMFLOAT4(.5,.5,.5,.5);
    *specularPower = XMFLOAT4(30,30,30,30);
    *specularScaler = XMFLOAT4(1,1,1,1);
    *diffuseColorBounceLight = XMFLOAT4(0,0,0,0);
    *diffuseColorLow = XMFLOAT4(0,0,0,0);
    *diffuseColorMid = XMFLOAT4(.5,.5,.5,.5);
    *diffuseColorHigh = XMFLOAT4(1,1,1,1);
    *diffuseColorRampMidpoint = XMFLOAT4(.5,.5,.5,.5);
    *diffuseColorBounceLightPoint = XMFLOAT4(.1,.1,.1,.1);
    *normalIntensity =XMFLOAT4(1,1,1,1);
    *rimExponent = XMFLOAT4(2.2,2.2,2.2,2.2);
    *rimColorSky = XMFLOAT4(.75,.9,1.0,1);
    *rimColor = XMFLOAT4(0,0,0,0);
    *aoAmount = XMFLOAT4(.75,.9,1.1,1);
    *lightDir = XMFLOAT4(0,-1,1,0);
    *lightColor = XMFLOAT4(1,1,1,1);

    *useDiffuseMap = false;
    *useAO = false;
    *useColorRampDiffuse = false;
    *useNormalMap = false;
    *useRimLightSky = false;
    *useRimLight = false;

    pd3dDevice->SetPixelShaderConstantF(12, (float*)vConstants, 27);
    pd3dDevice->SetPixelShaderConstantB(1, bConstants, 6);
}
//--------------------------------------------------------------------------------------
// Name: Render()
// Desc: Renders the scene and UI.
//--------------------------------------------------------------------------------------
HRESULT Sample::Render()
{
    m_matWVP = m_matWorld * m_matView * m_matProj;
    ATG::DebugDraw::SetViewProjection( m_matWVP );


    m_pd3dDevice->BeginScene();

    m_pd3dDevice->SetRenderState_Inline( D3DRS_VIEWPORTENABLE, TRUE );

    // Set up 4x MSAA render targets for tiling.
    m_pd3dDevice->SetRenderTarget( 0, m_pColorTarget4X );
    m_pd3dDevice->SetDepthStencilSurface( m_pDepthStencil4X );

    const D3DRECT pTilingRects[] =
    {
        { 0,   0, SCREEN_WIDTH, 256 },
        { 0, 256, SCREEN_WIDTH, 512 },
        { 0, 512, SCREEN_WIDTH, SCREEN_HEIGHT }
    };

    // Begin tiling.
    D3DVECTOR4 ClearColor = { 0, 0, 0, 0 };
    m_pd3dDevice->BeginTiling( 0, 3, pTilingRects, &ClearColor, 1.0f, 0 );

    ATG::RenderBackground( D3DCOLOR_ARGB(255, 50, 52, 52), D3DCOLOR_ARGB( 255, 50, 52, 52) );

    // Set default renderstate.
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_ZFUNC, D3DCMP_LESSEQUAL );
    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );

    // Render scene.
    if( m_bDrawScene )
    {
        for( DWORD i = 0; i < 3; ++i )
        {
            m_pd3dDevice->SetSamplerState( i, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
            m_pd3dDevice->SetSamplerState( i, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
            m_pd3dDevice->SetSamplerState( i, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR );
            m_pd3dDevice->SetSamplerState( i, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP );
            m_pd3dDevice->SetSamplerState( i, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP );
            m_pd3dDevice->SetTexture( i, NULL );
        }
        m_pd3dDevice->SetTexture( 2, m_pScene->GetResourceDatabase()->GetBlackTexture()->GetD3DTexture() );

        // Render each model.
        for( DWORD i = 0; i < m_dwModelCount; ++i )
        {
			if( _wcsicmp(strModelNames[i], L"Armor") == 0)
			{
                SetupArmorMaterial( m_pd3dDevice );
			}
			else if( _wcsicmp(strModelNames[i], L"UpperArms") == 0 ||
				     _wcsicmp(strModelNames[i], L"LowerArms") == 0 )
			{
                SetupArmsMaterial( m_pd3dDevice );

			}
			else if( _wcsicmp(strModelNames[i], L"Head") == 0)
			{
                SetupHeadMaterial( m_pd3dDevice );
            }
			else if( _wcsicmp(strModelNames[i], L"Beard") == 0 ||
				     _wcsicmp(strModelNames[i], L"Moustache") == 0 )
			{
                SetupBeardMaterial( m_pd3dDevice );

			}
			else if( _wcsicmp(strModelNames[i], L"UpperLegs") == 0 ||
					 _wcsicmp(strModelNames[i], L"LowerLegs") == 0 )
			{
                SetupLegsMaterial( m_pd3dDevice );

			}
			else if( _wcsicmp(strModelNames[i], L"Boots") == 0)
			{
                SetupBootsMaterial( m_pd3dDevice );

			}
			else if( _wcsicmp(strModelNames[i], L"Gloves") == 0)
			{
                SetupGlovesMaterial( m_pd3dDevice );

			}
			else if( _wcsicmp(strModelNames[i], L"Shirt") == 0)
			{
                SetupShirtMaterial( m_pd3dDevice );

			}
			else if( _wcsicmp(strModelNames[i], L"Pants") == 0)
			{
                SetupPantsMaterial( m_pd3dDevice );
			}
			else if( _wcsicmp(strModelNames[i], L"Eyes") == 0)
			{
                SetupEyesMaterial( m_pd3dDevice );
			}
            else
            {
                SetupDefaultMaterial( m_pd3dDevice );
            }

            if( m_pModelInfos[i].pSkinnedMesh != NULL )
            {
                RenderSkinnedModel( &m_pModelInfos[i] );
            }
            else
            {
                RenderStaticModel( &m_pModelInfos[i] );
            }
        }

    }
    if( m_bDrawSkeleton )
    {
        RenderSkeleton();
    }

    // Render a ground plane.
    XMMATRIX matGround = XMMatrixRotationRollPitchYaw( 0, XMConvertToRadians(45.0f), 0) * m_matWVP;
    ATG::DebugDraw::SetViewProjection( matGround );

    ATG::DebugDraw::DrawGrid( XMFLOAT3( 20, 0, 0 ), XMFLOAT3( 0, 0, 20 ), XMFLOAT3( 0, 0, 0 ), 160, 160,
                                  0xFF5e96e9 );
    ATG::DebugDraw::SetViewProjection( m_matWVP );

    // Draw the raw depth and image map with skeleton overlaid as visualization.
    const FLOAT drawWidth = 150.0f;
    const FLOAT drawHeight = drawWidth * 3.0f / 4.0f;
    const FLOAT drawX = 50.0f;
    const FLOAT drawY = SCREEN_HEIGHT - 30.0f - drawHeight;
    m_pip.BeginRender();
    m_pip.RenderColorStream( drawX, drawY, drawWidth, drawHeight );
    m_pip.RenderSkeletons( drawX, drawY, drawWidth, drawHeight, FALSE, TRUE );
    m_pip.RenderDepthStream( drawX + drawWidth + 10, drawY, drawWidth, drawHeight );
    m_pip.RenderSkeletons( drawX + drawWidth + 10, drawY, drawWidth, drawHeight, FALSE, FALSE );
    m_pip.EndRender();

    if( m_NuiSkeleton.SkeletonData[ m_iCurrentSkeletonIndex ].eTrackingState == NUI_SKELETON_TRACKED )
    {
        static float tx = 1.5f;
        static float ty = 2.0f;
        static float tz = -2.0f;
        m_pNuiJointConverter->DrawSkeleton( m_pd3dDevice,  XMMatrixTranslation( tx, ty, tz) * XMMatrixScaling( .43f, .43f, -.43f) * m_matWVP );
    }

    // End tiling.
    m_pd3dDevice->EndTiling( D3DRESOLVE_RENDERTARGET0 | D3DRESOLVE_CLEARRENDERTARGET, NULL,
                             m_pResolveBuffer, &ClearColor, 1.0f, 0, NULL );

    // Switch to a full-screen rendertarget.
    m_pd3dDevice->SetRenderTarget( 0, m_pColorTarget1X );
    m_pd3dDevice->SetDepthStencilSurface( NULL );

    // Draw resolve buffer back to the screen.
    D3DRECT ScreenSpaceRect = { 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT };
    ATG::DebugDraw::DrawScreenSpaceTexturedRect( ScreenSpaceRect, m_pResolveBuffer );

    // Render UI.
    RenderUI();

    m_pd3dDevice->EndScene();

    // Sync to present interval, resolve, and swap.
    m_pd3dDevice->SynchronizeToPresentationInterval();
    m_pd3dDevice->Resolve( D3DRESOLVE_RENDERTARGET0, NULL, m_pFrontBuffer, NULL, 0, 0, NULL, 1.0, 0, NULL );
    m_pd3dDevice->Swap( m_pFrontBuffer, NULL );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: RenderUI()
// Desc: Draws some statistics and other UI elements
//--------------------------------------------------------------------------------------
VOID Sample::RenderUI()
{
    if( m_bDrawHelp )
    {
        m_Help.Render( &m_Font, g_HelpCallouts, ARRAYSIZE( g_HelpCallouts ) );
    }
    else
    {
        // Draw a title and FPS indicator.
        m_Font.Begin();
        m_Font.SetScaleFactors( 1.2f, 1.2f );
        m_Font.DrawText( 0, 0, 0xffffffff, L"Advanced Skeletal Retargeting" );
        m_Font.SetScaleFactors( 1.0f, 1.0f );
        m_Font.DrawText( 0, 0, 0xffffff00, m_Timer.GetFrameRate(), ATGFONT_RIGHT );

        m_Font.End();
    }
}
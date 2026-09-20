//--------------------------------------------------------------------------------------
// SkinnedCharacter.cpp
//
// This sample demonstrates skeletal mesh deformation, also known as skinning.
// Seven different methods of skinning are implemented in this sample, each with
// different performance and memory tradeoffs.
//
// The different rendering methods are contained in the RenderSkinnedModel() method and
// the HLSL shaders (SkinnedCharacter.hlsl).
// The skeletal animation setup varies slightly between the methods; that code is at the
// end of the Update() method.
// Various types of graphics resources are created to support the different methods;
// these can be found in the Initialize() method.
//
// The skeletal animation system is not the focus of this sample, but it has been 
// documented nonetheless.  It can be found in Animation.h and Animation.cpp.
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

extern "C"
    void _WriteBarrier();
#pragma intrinsic(_WriteBarrier)

// Define a symbol that is used to compile out the use of the GPU performance counter APIs 
// when using a release build of Direct3D.  The GPU performance counter APIs only work with 
// d3d9i.lib and d3d9d.lib.
#if defined( NDEBUG ) && ( !defined( PROFILE ) || defined( FASTCAP ) )
#define _RELEASED3D
#endif

//--------------------------------------------------------------------------------------
// Callouts for labelling the gamepad on the help screen
//--------------------------------------------------------------------------------------
ATG::HELP_CALLOUT g_HelpCallouts[] =
{
    { ATG::HELP_A_BUTTON,     ATG::HELP_PLACEMENT_1, L"Cycle skinning implementation" },
    { ATG::HELP_DPAD,         ATG::HELP_PLACEMENT_1, L"Cycle skinning implementation" },
    { ATG::HELP_B_BUTTON,     ATG::HELP_PLACEMENT_2, L"Draw mesh" },
    { ATG::HELP_START_BUTTON, ATG::HELP_PLACEMENT_2, L"Toggle animation update" },
    { ATG::HELP_Y_BUTTON,     ATG::HELP_PLACEMENT_2, L"Draw skeleton" },
    { ATG::HELP_LEFT_STICK,   ATG::HELP_PLACEMENT_2, L"Rotate directional light" },
    { ATG::HELP_RIGHT_STICK,  ATG::HELP_PLACEMENT_2, L"Rotate camera" },
    { ATG::HELP_LEFT_SHOULDER,ATG::HELP_PLACEMENT_2, L"Slow down animation" },
    { ATG::HELP_RIGHT_SHOULDER,ATG::HELP_PLACEMENT_2,L"Speed up animation" },
    { ATG::HELP_BACK_BUTTON,  ATG::HELP_PLACEMENT_2, L"Display\nhelp" },
    { ATG::HELP_X_BUTTON,     ATG::HELP_PLACEMENT_2, L"Toggle pixel shading" },
    { ATG::HELP_MISC_CALLOUT, ATG::HELP_PLACEMENT_2, L"Triggers move camera in/out" },
};
#define NUM_HELP_CALLOUTS (sizeof(g_HelpCallouts) / sizeof(g_HelpCallouts[0]))

// Vertex declaration that describes our rigid vertex buffer structure
const D3DVERTEXELEMENT9 g_RigidVBElements[] =
{
    { 0,  0, D3DDECLTYPE_FLOAT3,    D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
    { 0, 12, D3DDECLTYPE_FLOAT16_2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
    { 0, 16, D3DDECLTYPE_DEC3N,     D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_NORMAL,   0 },
    { 0, 20, D3DDECLTYPE_DEC3N,     D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TANGENT,  0 },
    D3DDECL_END()
};
const DWORD g_dwRigidVertexSize = 24;

// We will be double-buffering all resources.
const DWORD g_dwBufferCount = 2;

// Display strings for the different skinning methods
struct SkinningMethodDesc
{
    const WCHAR* strImplementationType;
    const WCHAR* strBonePaletteSource;
};
SkinningMethodDesc g_strSkinningMethodNames[] =
{
    { L"Vertex Shader",                 L"Shadowed Shader Constants" },
    { L"Vertex Shader",                 L"Constant Buffer" },
    { L"Vertex Shader",                 L"Vertex Stream via Vertex Cache" },
    { L"Vertex Shader",                 L"Vertex Stream via Texture Cache" },
    { L"Vertex Shader",                 L"Texture" },
    { L"Memory Export Vertex Shader",   L"Texture" },
    { L"VMX128",                        L"Cached System Memory" },
};
const DWORD g_dwSkinningMethodCount = ARRAYSIZE( g_strSkinningMethodNames );

// Enumeration for the skinning methods, to make the rendering code more readable
enum SkinningMethods
{
    SM_VertexShaderShadowedConstants = 0,
    SM_VertexShaderConstantBuffer,
    SM_VertexShaderVFetchVCache,
    SM_VertexShaderVFetchTCache,
    SM_VertexShaderTFetch,
    SM_VertexShaderMemExport,
    SM_VMX128,
    SM_SIZEOF
};
// Compiler assert to make sure the methods enum matches the display strings array
C_ASSERT( SM_SIZEOF == ARRAYSIZE( g_strSkinningMethodNames ) );


//--------------------------------------------------------------------------------------
// Name: struct ModelInfo
// Desc: Contains all relevant information about a single model.  Each ModelInfo
//       contains either a valid pSkinnedMesh or pStaticMesh pointer.  If the pStaticMesh
//       pointer is valid, much of the structure will not be filled in.
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

    // Rigid mesh vertex buffers for VMX128 skinning and memory export
    D3DVertexBuffer* pRigidMeshVB[ g_dwBufferCount ];
    D3DVertexDeclaration* pRigidMeshDecl;

    // Memory export vertex buffer (this will point to pRigidMeshVB[0])
    D3DVertexBuffer* pMemoryExportVB;

    // Memory export constants
    GPU_MEMEXPORT_STREAM_CONSTANT ExportConstantPosition;
    GPU_MEMEXPORT_STREAM_CONSTANT ExportConstantNormalBinormalTangent;
    GPU_MEMEXPORT_STREAM_CONSTANT ExportConstantTexCoord0;

    // Memory allocations for bone matrix constants
    VOID* pBoneMatrixBufferPhysical[ g_dwBufferCount ];
    VOID* pBoneMatrixBufferVirtual;

    // Vertex buffers and a vertex decl for bone matrix constants
    D3DVertexBuffer* pBoneMatrixVB[ g_dwBufferCount ];
    D3DVertexDeclaration* pBoneMatrixVBDecl;

    // Textures for bone matrix constants
    D3DTexture* pBoneMatrixTexture[ g_dwBufferCount ];

    // Constant buffers for bone matrix constants
    D3DConstantBuffer* pBoneMatrixConstantBuffer[ g_dwBufferCount ];
};


//--------------------------------------------------------------------------------------
// Name: class Sample
// Desc: The skinned character sample class.
//--------------------------------------------------------------------------------------
class Sample : public ATG::Application
{
public:
    HRESULT Initialize();
    HRESULT Update();
    HRESULT Render();

private:
    VOID    InitializeAnimation();
    VOID    InitializeMemExportConstants( ModelInfo* pModelInfo );

    VOID    SetupMaterial( ModelInfo* pModelInfo, DWORD dwSubsetIndex );
    VOID    RenderSkinnedModel( ModelInfo* pModelInfo );
    VOID    RenderStaticModel( ModelInfo* pModelInfo );
    VOID    RenderSkeleton();

    VOID    RenderUI();
    VOID    DeformMesh( ATG::SkeletonInstance* pSkeletonInstance, ModelInfo* pModelInfo, D3DVertexBuffer* pDestBuffer );

private:
    // Sample framework objects
    ATG::Font m_Font;
    ATG::Timer m_Timer;
    ATG::Help m_Help;
    BOOL m_bDrawHelp;
    DOUBLE m_fUpdateTime;
    DOUBLE m_fRenderTime;
    FLOAT m_fDeltaTime;

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
    ATG::Camera* m_pCamera;
    ATG::Animation* m_pAnimation;
    XMMATRIX m_matWVP;

    // Model info structs
    ModelInfo* m_pModelInfos;
    DWORD m_dwModelCount;

    // Animation members
    ATG::Skeleton m_Skeleton;
    ATG::SkeletonInstance m_SkeletonInstance;
    BOOL m_bUpdateAnimation;
    DWORD m_dwForceUpdateCount;

    // Rendering members
    SkinningMethods m_SkinningMethod;
    D3DVertexShader* m_pVertexShaderSkinningConstants;
    D3DVertexShader* m_pVertexShaderSkinningVertexFetch;
    D3DVertexShader* m_pVertexShaderSkinningVertexFetchTextureCache;
    D3DVertexShader* m_pVertexShaderSkinningTextureFetch;
    D3DVertexShader* m_pVertexShaderSkinningMemExport;
    D3DVertexShader* m_pVertexShaderTransform;
    D3DPixelShader* m_pPixelShaderSolidColor;
    D3DPixelShader* m_pPixelShaderNormalMapping;
    DWORD m_dwFrameCount;
    XMFLOAT4 m_DirectionalLightDirection;
    XMFLOAT4 m_DirectionalLightColor;
    XMFLOAT4 m_AmbientColor;
    DWORD m_dwInstanceCount;
    BOOL m_bNoPixelShading;
    DWORD m_dwDefaultPredicationMask;
    BOOL m_bDrawScene;
    BOOL m_bDrawSkeleton;
    FLOAT m_fBoneRadius;
    D3DVIEWPORT9 m_NullViewport;

    // GPU performance counters (debug/profile builds only)
    D3DPerfCounters* m_pPerfCounterStart[3];
    D3DPerfCounters* m_pPerfCounterEnd[3];
};


//--------------------------------------------------------------------------------------
// Name: main()
// Desc: Entry point to the sample.
//--------------------------------------------------------------------------------------
VOID __cdecl main()
{
    Sample SCSample;

    // Set up the structure used to create the D3DDevice.
    D3DPRESENT_PARAMETERS& d3dpp = SCSample.m_d3dpp;
    ZeroMemory( &d3dpp, sizeof( D3DPRESENT_PARAMETERS ) );
    d3dpp.BackBufferWidth = 1280;
    d3dpp.BackBufferHeight = 720;
    d3dpp.BackBufferFormat =  ( D3DFORMAT )MAKESRGBFMT( D3DFMT_A8R8G8B8 );
    d3dpp.FrontBufferFormat = ( D3DFORMAT )MAKESRGBFMT( D3DFMT_LE_X8R8G8B8 );
    d3dpp.MultiSampleType = D3DMULTISAMPLE_NONE;
    d3dpp.MultiSampleQuality = 0;
    d3dpp.BackBufferCount = 0;
    d3dpp.EnableAutoDepthStencil = FALSE;
    d3dpp.DisableAutoBackBuffer = TRUE;
    d3dpp.DisableAutoFrontBuffer = TRUE;
    d3dpp.AutoDepthStencilFormat = D3DFMT_D24S8;
    d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;

    SCSample.m_dwDeviceCreationFlags |= D3DCREATE_BUFFER_2_FRAMES;

    SCSample.Run();
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
    m_bUpdateAnimation = TRUE;
    m_dwForceUpdateCount = 0;
    m_SkinningMethod = SM_VertexShaderShadowedConstants;
    m_fBoneRadius = 0.01f;
    m_dwFrameCount = 0;
    m_dwInstanceCount = 1;
    m_bNoPixelShading = FALSE;
    m_dwDefaultPredicationMask = 0;
    m_DirectionalLightColor = XMFLOAT4( 1, 1, 1, 1 );
    m_DirectionalLightDirection = XMFLOAT4( -0.7071f, -0.7071f, 0, 0 );
    m_AmbientColor = XMFLOAT4( 0.05f, 0.05f, 0.05f, 0.1f );

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

    // Create a "null" viewport that will be used to eliminate pixel shading.
    m_NullViewport.X = 0;
    m_NullViewport.Y = 0;
    m_NullViewport.Width = 1;
    m_NullViewport.Height = 1;
    m_NullViewport.MinZ = 0.0f;
    m_NullViewport.MaxZ = 1.0f;

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
                                                   ( D3DFORMAT )MAKESRGBFMT( D3DFMT_A8R8G8B8 ),
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

    // Create 1280x256 4xMSAA surfaces.
    hr = m_pd3dDevice->CreateRenderTarget( 1280, 256,
                                           ( D3DFORMAT )MAKESRGBFMT( D3DFMT_A8R8G8B8 ),
                                           D3DMULTISAMPLE_4_SAMPLES,
                                           0, FALSE,
                                           &m_pColorTarget4X,
                                           &ColorSurfParams );
    if( FAILED( hr ) )
        ATG::FatalError( "Couldn't create color rendertarget." );

    hr = m_pd3dDevice->CreateRenderTarget( 1280, 256,
                                           D3DFMT_D24S8,
                                           D3DMULTISAMPLE_4_SAMPLES,
                                           0, FALSE,
                                           &m_pDepthStencil4X,
                                           &DepthSurfParams );
    if( FAILED( hr ) )
        ATG::FatalError( "Couldn't create depth/stencil rendertarget." );

    // Create 1280x720 front buffer texture and resolve buffer texture.
    hr = m_pd3dDevice->CreateTexture( 1280, 720, 1, 0, ( D3DFORMAT )MAKESRGBFMT( D3DFMT_LE_X8R8G8B8 ), D3DPOOL_DEFAULT, &m_pFrontBuffer, NULL );
    hr = m_pd3dDevice->CreateTexture( 1280, 720, 1, 0, ( D3DFORMAT )MAKESRGBFMT( D3DFMT_X8R8G8B8 ), D3DPOOL_DEFAULT, &m_pResolveBuffer, NULL );

    // Load shaders.
    hr = ATG::LoadVertexShader( "game:\\media\\shaders\\SkinVSConstants.xvu", &m_pVertexShaderSkinningConstants );
    if( FAILED( hr ) )
        ATG::FatalError( "Couldn't load vertex shader." );
    hr = ATG::LoadVertexShader( "game:\\media\\shaders\\SkinVSVertexFetch.xvu", &m_pVertexShaderSkinningVertexFetch );
    if( FAILED( hr ) )
        ATG::FatalError( "Couldn't load vertex shader." );
    hr = ATG::LoadVertexShader( "game:\\media\\shaders\\SkinVSVertexFetchTextureCache.xvu",
                                &m_pVertexShaderSkinningVertexFetchTextureCache );
    if( FAILED( hr ) )
        ATG::FatalError( "Couldn't load vertex shader." );
    hr = ATG::LoadVertexShader( "game:\\media\\shaders\\SkinVSTextureFetch.xvu",
                                &m_pVertexShaderSkinningTextureFetch );
    if( FAILED( hr ) )
        ATG::FatalError( "Couldn't load vertex shader." );
    hr = ATG::LoadVertexShader( "game:\\media\\shaders\\SkinVSMemExport.xvu", &m_pVertexShaderSkinningMemExport );
    if( FAILED( hr ) )
        ATG::FatalError( "Couldn't load vertex shader." );
    hr = ATG::LoadVertexShader( "game:\\media\\shaders\\TransformVS.xvu", &m_pVertexShaderTransform );
    if( FAILED( hr ) )
        ATG::FatalError( "Couldn't load vertex shader." );
    hr = ATG::LoadPixelShader( "game:\\media\\shaders\\SolidColor.xpu", &m_pPixelShaderSolidColor );
    if( FAILED( hr ) )
        ATG::FatalError( "Couldn't load pixel shader." );
    hr = ATG::LoadPixelShader( "game:\\media\\shaders\\NormalMapPS.xpu", &m_pPixelShaderNormalMapping );
    if( FAILED( hr ) )
        ATG::FatalError( "Couldn't load pixel shader." );

    // Create scene object.
    m_pScene = new ATG::Scene();
    ATG::ResourceDatabase* pRDB = m_pScene->GetResourceDatabase();

    // Create default textures in the resource database.
    pRDB->CreateDefaultResources();

    // Load character and animation data from a scene file.
    ATG::SceneFileParser::LoadXATGFile( "game:\\media\\scenes\\SkinnedCharacter.xatg", m_pScene, NULL,
                                        ATG::XATGLOADER_DONOTINITIALIZEMATERIALS, NULL );

    // Create a default camera and add it to the scene.
    m_pCamera = new ATG::Camera();
    m_pCamera->SetLocalTransform( XMMatrixIdentity() );
    m_pCamera->SetLocalPosition( XMVectorSet( 0, 4, -10, 1 ) );
    ATG::Projection proj;
    proj.SetFovXAspect( XM_PIDIV2, 16.0f / 9.0f, 0.01f, 100.0f );
    m_pCamera->SetProjection( proj );
    m_pScene->AddChild( m_pCamera );

    // Search for 4 specific models in the scene file.
    const WCHAR* strModelNames[] = { L"body", L"Head", L"L_eyeBall", L"R_eyeBall" };
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

            DWORD dwBonePaletteSize = ATG::g_dwMaxBoneCount * 3 * sizeof( XMFLOAT4 );
            m_pModelInfos[i].pBoneMatrixBufferVirtual = new XMFLOAT4A[ ATG::g_dwMaxBoneCount * 3 ];

            for( DWORD dwBufferIndex = 0; dwBufferIndex < g_dwBufferCount; ++dwBufferIndex )
            {
                // Create memory allocations for bone matrix palette constants.
                // All of the GPU skinning methods will read bone matrices from these buffers, since
                // the buffers will be "cast" as constant buffers, vertex buffers, and textures.
                m_pModelInfos[i].pBoneMatrixBufferPhysical[dwBufferIndex] = XPhysicalAlloc( dwBonePaletteSize,
                                                                                            MAXULONG_PTR, 0,
                                                                                            PAGE_READWRITE |
                                                                                            PAGE_WRITECOMBINE );

                // Create vertex buffers for bone matrix constants.
                m_pModelInfos[i].pBoneMatrixVB[dwBufferIndex] = new D3DVertexBuffer;
                XGSetVertexBufferHeader( dwBonePaletteSize, 0, D3DPOOL_DEFAULT, 0,
                                         m_pModelInfos[i].pBoneMatrixVB[dwBufferIndex] );
                XGOffsetResourceAddress( m_pModelInfos[i].pBoneMatrixVB[dwBufferIndex],
                                         m_pModelInfos[i].pBoneMatrixBufferPhysical[dwBufferIndex] );

                // Create 1D textures for bone matrix constants.
                // The textures are 240x1 and 64bpp, with each bone matrix occupying 3 texels.
                D3DFORMAT CustomFormat = ( D3DFORMAT )MAKELINFMT( D3DFMT_A16B16G16R16F );
                m_pModelInfos[i].pBoneMatrixTexture[dwBufferIndex] = new D3DTexture;
                XGSetTextureHeader( ATG::g_dwMaxBoneCount * 3, 1, 1, 0, CustomFormat, D3DPOOL_DEFAULT, 0, 0, 0,
                                    m_pModelInfos[i].pBoneMatrixTexture[dwBufferIndex], NULL, NULL );
                XGOffsetBaseTextureAddress( m_pModelInfos[i].pBoneMatrixTexture[dwBufferIndex],
                                            m_pModelInfos[i].pBoneMatrixBufferPhysical[dwBufferIndex], NULL );

                // Create constant buffers for bone matrix constants.
                m_pModelInfos[i].pBoneMatrixConstantBuffer[dwBufferIndex] = new D3DConstantBuffer;
                XGSetConstantBufferHeader( ATG::g_dwMaxBoneCount * 3, 0, 0,
                                           m_pModelInfos[i].pBoneMatrixConstantBuffer[dwBufferIndex] );
                XGOffsetResourceAddress( m_pModelInfos[i].pBoneMatrixConstantBuffer[dwBufferIndex],
                                         m_pModelInfos[i].pBoneMatrixBufferPhysical[dwBufferIndex] );

                // Create rigid mesh vertex buffers.
                const DWORD dwVBSize = m_pModelInfos[i].dwMeshVertexCount * g_dwRigidVertexSize;
                hr = m_pd3dDevice->CreateVertexBuffer( dwVBSize, 0, 0,
                                                       D3DPOOL_DEFAULT, &m_pModelInfos[i].pRigidMeshVB[dwBufferIndex],
                                                       NULL );
            }

            // Create constant VB vertex decl.
            // The bone matrix vertex stream will consist of 3 FLOAT16_4 vectors per stride.
            D3DVERTEXELEMENT9 VertexElements[ 32 ];
            DWORD dwDeclElementCount = ARRAYSIZE( VertexElements );
            m_pModelInfos[i].pMeshDecl->GetDeclaration( VertexElements, ( UINT* )&dwDeclElementCount );
            static const D3DVERTEXELEMENT9 ConstantVBStream[] =
            {
                { 1,  0, D3DDECLTYPE_FLOAT16_4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 1 },
                { 1,  8, D3DDECLTYPE_FLOAT16_4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 2 },
                { 1, 16, D3DDECLTYPE_FLOAT16_4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 3 },
                D3DDECL_END()
            };
            VertexElements[ dwDeclElementCount - 1 ] = ConstantVBStream[0];
            VertexElements[ dwDeclElementCount ] = ConstantVBStream[1];
            VertexElements[ dwDeclElementCount + 1 ] = ConstantVBStream[2];
            VertexElements[ dwDeclElementCount + 2 ] = ConstantVBStream[3];
            m_pd3dDevice->CreateVertexDeclaration( VertexElements, &m_pModelInfos[i].pBoneMatrixVBDecl );

            // Create rigid mesh vertex decl.
            m_pd3dDevice->CreateVertexDeclaration( g_RigidVBElements, &m_pModelInfos[i].pRigidMeshDecl );

            InitializeMemExportConstants( &m_pModelInfos[i] );
        }
        else if( pMesh->Type() == ATG::StaticMesh::TypeID )
        {
            // Mesh is a static mesh (no skinning data).
            m_pModelInfos[i].pStaticMesh = ( ATG::StaticMesh* )pMesh;
        }
    }

    // Initialize animation system.
    InitializeAnimation();


#ifndef _RELEASED3D
    // Set up GPU performance counter structures.
    for( DWORD i = 0; i < 3; ++i )
    {
        m_pd3dDevice->CreatePerfCounters( &m_pPerfCounterStart[i], 1 );
        m_pd3dDevice->CreatePerfCounters( &m_pPerfCounterEnd[i], 1 );
    }
    m_pd3dDevice->EnablePerfCounters( TRUE );

    // Enable the performance counters we care about.
    D3DPERFCOUNTER_EVENTS PerfEvents;
    ZeroMemory( &PerfEvents, sizeof( D3DPERFCOUNTER_EVENTS ) );
    // CP clock cycles.
    PerfEvents.CP[0] = GPUPE_CP_COUNT;
    // NRT busy cycles.
    PerfEvents.RBBM[0] = GPUPE_RBBM_NRT_BUSY;
    // Texture and vertex cache reads from system memory.
    PerfEvents.MH[0] = GPUPE_TC0_READ;
    PerfEvents.MH[1] = GPUPE_TC1_READ;
    PerfEvents.MH[2] = GPUPE_VC0_READ_MEMORY;
    // Vertex cache performance.
    PerfEvents.VC[0] = GPUPE_CC_HITS;
    PerfEvents.VC[1] = GPUPE_CC_MISSES;
    // Texture cache performance.
    PerfEvents.TCF[0] = GPUPE_TAG_HITS;
    PerfEvents.TCF[1] = GPUPE_TAG_MISSES;
    // SQ stuff.
    PerfEvents.SQ[0] = GPUPE_SQ_CONSTANTS_SENT_SP_SIMD0;
    PerfEvents.SQ[1] = GPUPE_SQ_CONSTANTS_USED_SIMD0;
    m_pd3dDevice->SetPerfCounterEvents( &PerfEvents, 0 );
#endif

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

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: InitializeAnimation()
// Desc: Initializes the animation system and binds each model to parts of the animation
//       system.
//--------------------------------------------------------------------------------------
VOID Sample::InitializeAnimation()
{
    // Find a frame named "Root" - this is the root of the animated object.
    ATG::Frame* pSkeletonFrame = ( ATG::Frame* )m_pScene->FindObjectOfType( L"Root", ATG::Frame::TypeID );
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

    // Find the animation track set.
    m_pAnimation = ( ATG::Animation* )m_pScene->FindObjectOfType( L"Untitled", ATG::Animation::TypeID );
    assert( m_pAnimation != NULL );

    // Bind animation to skeleton instance.
    m_SkeletonInstance.CreateAnimationBinding( m_pAnimation );
}


//--------------------------------------------------------------------------------------
// Name: InitializeMemExportConstants()
// Desc: Initializes the memory export constants and structures for a single model.
//--------------------------------------------------------------------------------------
VOID Sample::InitializeMemExportConstants( ModelInfo* pModelInfo )
{
    // The memory export VB is simply the first rigid mesh VB.
    pModelInfo->pMemoryExportVB = pModelInfo->pRigidMeshVB[0];

    // Get the resource data pointer from the memory export VB.
    // This data pointer will be part of the memory export stream constants.
    VOID* pVertexData = NULL;
    pModelInfo->pMemoryExportVB->Lock( 0, 0, &pVertexData, 0 );
    pModelInfo->pMemoryExportVB->Unlock();

    // Stream constants tell the GPU about a memexport destination in system memory.
    // The constant includes a data pointer, which is the starting address of the buffer.
    // The constant includes a datatype, which tells the GPU how to format the data
    // when writing to the buffer.
    // The constant includes a stream length, which is the number of datatypes in the
    // entire buffer, not the number of bytes or DWORDs in the buffer.
    // Think of each stream constant as a single homogenous view on a heterogeneous 
    // vertex stream.  Multiple stream constants are used to write a single vertex, one
    // stream constant for each datatype in the vertex declaration.

    // Create stream constant for the position.
    // It's a FLOAT3 datatype, but we'll export as 3 FLOATs.
    // The total vertex size is 6 times the size of this datatype.
    GPU_SET_MEMEXPORT_STREAM_CONSTANT(
        &pModelInfo->ExportConstantPosition,
        pVertexData,
        ( 6 * pModelInfo->dwMeshVertexCount ),
        SURFACESWAP_LOW_RED,
        GPUSURFACENUMBER_FLOAT,
        GPUCOLORFORMAT_32_FLOAT,
        GPUENDIAN128_8IN32 );

    // Create stream constant for the normal and tangent.
    // It's a DEC3N datatype, which the hardware knows as a 2_10_10_10 signed integer.
    // The total vertex size is 6 times the size of a DEC3N.
    GPU_SET_MEMEXPORT_STREAM_CONSTANT(
        &pModelInfo->ExportConstantNormalBinormalTangent,
        pVertexData,
        ( 6 * pModelInfo->dwMeshVertexCount ),
        SURFACESWAP_LOW_RED,
        GPUSURFACENUMBER_SINTEGER,
        GPUCOLORFORMAT_2_10_10_10,
        GPUENDIAN128_8IN32 );

    // Create stream constant for the texture coordinates.
    // It's a FLOAT16_2 datatype.
    // The total vertex size is 6 times the size of a FLOAT16_2.
    // Note the special endian swapping used to preserve the proper ordering of the
    // components.
    GPU_SET_MEMEXPORT_STREAM_CONSTANT(
        &pModelInfo->ExportConstantTexCoord0,
        pVertexData,
        ( 6 * pModelInfo->dwMeshVertexCount ),
        SURFACESWAP_LOW_RED,
        GPUSURFACENUMBER_FLOAT,
        GPUCOLORFORMAT_16_16_FLOAT,
        GPUENDIAN128_8IN16 );
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

    // Update frame count.
    ++m_dwFrameCount;

    // Get input from controllers (check for exit button sequence).
    ATG::GAMEPAD* pGamepad = ATG::Input::GetMergedInput();

    // The Back button toggles the help screen.
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_BACK )
        m_bDrawHelp = !m_bDrawHelp;

    // The Start button toggles animation updates.
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_START )
    {
        m_bUpdateAnimation = !m_bUpdateAnimation;
        m_dwForceUpdateCount = g_dwBufferCount;
    }

    // The Y button toggles skeleton drawing.
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_Y )
        m_bDrawSkeleton = !m_bDrawSkeleton;

    // The B button toggles scene drawing.
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_B )
        m_bDrawScene = !m_bDrawScene;

    // The A button and right dpad switches to the next skinning method.
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_A ||
        pGamepad->wPressedButtons & XINPUT_GAMEPAD_DPAD_RIGHT )
    {
        m_SkinningMethod = ( SkinningMethods )( ( ( DWORD )m_SkinningMethod + 1 ) % SM_SIZEOF );
        // Skinning method changed; make sure all of the buffers are updated.
        m_dwForceUpdateCount = g_dwBufferCount;
    }

    // The left dpad switches to the previous skinning method.
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_DPAD_LEFT )
    {
        m_SkinningMethod = ( SkinningMethods )( ( ( DWORD )m_SkinningMethod + ( DWORD )SM_SIZEOF - 1 ) % SM_SIZEOF );
        // Skinning method changed; make sure all of the buffers are updated.
        m_dwForceUpdateCount = g_dwBufferCount;
    }

    // The X button toggles pixel shading.
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_X )
        m_bNoPixelShading = !m_bNoPixelShading;

    // The bumpers change the animation speed.
    FLOAT fPlaybackSpeed = m_SkeletonInstance.m_pActiveAnimation->m_fPlaybackSpeed;
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_LEFT_SHOULDER )
        fPlaybackSpeed -= 0.25f;
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER )
        fPlaybackSpeed += 0.25f;

    fPlaybackSpeed = min( max( fPlaybackSpeed, 0.0f ), 2.0f );
    m_SkeletonInstance.m_pActiveAnimation->m_fPlaybackSpeed = fPlaybackSpeed;

    // Update camera position.
    static FLOAT s_fRotateY = -3.5f;
    static FLOAT s_fRotateX = 0.25f;
    static FLOAT s_fCameraDistance = 2.0f;
    FLOAT fTargetHeight = 0.35f;
    const FLOAT fRotateSpeed = XM_PIDIV2;
    const FLOAT fZoomSpeed = 2.0f;
    FLOAT fRotateYAmount = pGamepad->fX2 * fRotateSpeed * m_fDeltaTime;
    FLOAT fRotateXAmount = pGamepad->fY2 * fRotateSpeed * m_fDeltaTime;
    FLOAT fDistance = ( ( FLOAT )pGamepad->bLeftTrigger / 255.0f ) * fZoomSpeed * m_fDeltaTime -
        ( ( FLOAT )pGamepad->bRightTrigger / 255.0f ) * fZoomSpeed * m_fDeltaTime;
    s_fRotateX += fRotateXAmount;
    s_fRotateY += fRotateYAmount;
    s_fCameraDistance += fDistance;
    s_fRotateX = max( s_fRotateX, -XM_PIDIV2 );
    s_fRotateX = min( s_fRotateX, XM_PIDIV2 );
    s_fCameraDistance = max( s_fCameraDistance, 0.2f );
    s_fCameraDistance = min( s_fCameraDistance, 10.0f );
    // When the camera moves in close, move the target point up to the head.
    if( s_fCameraDistance < 1.0f )
    {
        FLOAT fInvDistance = ( 1.0f - s_fCameraDistance );
        fTargetHeight += ( fInvDistance * fInvDistance * 0.46875f );
    }
    static const XMVECTOR vAxisX = { 1, 0, 0, 0 };
    static const XMVECTOR vAxisY = { 0, 1, 0, 0 };
    XMVECTOR qRotateX = XMQuaternionRotationAxis( vAxisX, s_fRotateX );
    XMVECTOR qRotateY = XMQuaternionRotationAxis( vAxisY, s_fRotateY );
    XMVECTOR qRotation = XMQuaternionMultiply( qRotateX, qRotateY );
    XMMATRIX matRotation = XMMatrixRotationQuaternion( qRotation );
    XMMATRIX matWorld = XMMatrixTranslation( 0, 0, -s_fCameraDistance );
    matWorld *= matRotation;
    matWorld *= XMMatrixTranslation( 0, fTargetHeight, 0 );
    m_pCamera->SetLocalTransform( matWorld );

    // Update light direction.
    static FLOAT s_fLightAngle = -1.4f;
    const FLOAT fLightRotateSpeed = XM_PIDIV2;
    fRotateYAmount = pGamepad->fX1 * fLightRotateSpeed * m_fDeltaTime;
    s_fLightAngle += fRotateYAmount;
    m_DirectionalLightDirection.y = -0.5f;
    m_DirectionalLightDirection.x = 0.866f * cosf( s_fLightAngle );
    m_DirectionalLightDirection.z = 0.866f * sinf( s_fLightAngle );

    ATG::Timer CPUTimer;

    PIXBeginNamedEvent( 0, "Update Skinning" );

    // Update animation.
    if( m_bUpdateAnimation || m_dwForceUpdateCount > 0 )
    {
        // The force update count ensures that all of the bone palettes are filled in when animation is paused.
        if( m_dwForceUpdateCount > 0 )
        {
            --m_dwForceUpdateCount;
        }

        // The animation speed is 0 when pixel shading is disabled.
        FLOAT fUpdateTime = ( m_bNoPixelShading || !m_bUpdateAnimation ) ? 0.0f : m_fDeltaTime;
        // Update local and world transforms based on sampled animation track data.
        m_SkeletonInstance.UpdateAnimation( fUpdateTime );
        m_SkeletonInstance.BuildWorldPose();

        for( DWORD i = 0; i < m_dwModelCount; ++i )
        {
            if( m_pModelInfos[i].pSkinnedMesh == NULL )
                continue;

            switch( m_SkinningMethod )
            {
            case SM_VertexShaderShadowedConstants:
                // Bone palette for simple GPU skinning goes into cached memory.
                // We must use FLOAT4 vectors since we will submit this buffer to SetVertexShaderConstantF().
                m_SkeletonInstance.CreateBonePalette( i, m_pModelInfos[i].pBoneMatrixBufferVirtual, FALSE );
                break;
            case SM_VertexShaderConstantBuffer:
                {
                    // Bone palette for GPU skinning goes into the write combined memory buffer.
                    VOID* pCurrentBonePalette = m_pModelInfos[i].pBoneMatrixBufferPhysical[ m_dwFrameCount %
                        g_dwBufferCount ];
                    // We must use FLOAT4 vectors for a constant buffer.
                    m_SkeletonInstance.CreateBonePalette( i, pCurrentBonePalette, FALSE );
                    // Invalidate the GPU caches for this memory.
                    m_pd3dDevice->InvalidateGpuCache( pCurrentBonePalette, ATG::g_dwMaxBoneCount * 3 * sizeof( XMFLOAT4 ),
                        0 );
                    break;
                }
            case SM_VMX128:
                {
                    // Bone palette for CPU skinning goes into cached memory.
                    // Despite the fact that the CPU skinning code could use a HALF4 bone matrix palette, 
                    // aligned FLOAT4 vectors in memory load slightly faster.  
                    m_SkeletonInstance.CreateBonePalette( i, m_pModelInfos[i].pBoneMatrixBufferVirtual, FALSE );
                    // Perform CPU skinning now.
                    D3DVertexBuffer* pCurrentRigidVB = m_pModelInfos[i].pRigidMeshVB[ m_dwFrameCount %
                        g_dwBufferCount ];
                    DeformMesh( &m_SkeletonInstance, &m_pModelInfos[i], pCurrentRigidVB );
                    break;
                }
            default:
                {
                    // Bone palette for GPU skinning goes into the write combined memory buffer.
                    VOID* pCurrentBonePalette = m_pModelInfos[i].pBoneMatrixBufferPhysical[ m_dwFrameCount %
                        g_dwBufferCount ];
                    // We can use HALF4 vectors here for the bone matrix palette.
                    m_SkeletonInstance.CreateBonePalette( i, pCurrentBonePalette, TRUE );
                    // Invalidate the GPU caches for this memory.
                    m_pd3dDevice->InvalidateGpuCache( pCurrentBonePalette, ATG::g_dwMaxBoneCount * 3 * sizeof( XMHALF4 ),
                        0 );
                    break;
                }
            }
        }
    }

    m_fUpdateTime = CPUTimer.GetElapsedTime();
    PIXEndNamedEvent();
    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: XMVector3TransformCoordTransposed()
// Desc: Multiplies a position by a transposed XMMATRIX.
//--------------------------------------------------------------------------------------
__forceinline XMVECTOR XMVector3TransformCoordTransposed( const XMVECTOR V0, const XMMATRIX matTransform )
{
    static const XMVECTOR vSelectYZW = XMVectorSelectControl( 0, 1, 1, 1 );
    static const XMVECTOR vSelectZW = XMVectorSelectControl( 0, 0, 1, 1 );

    XMVECTOR R0 = XMVector4Dot( V0, matTransform.r[0] );
    XMVECTOR R1 = XMVector4Dot( V0, matTransform.r[1] );
    XMVECTOR R2 = XMVector4Dot( V0, matTransform.r[2] );

    XMVECTOR Rxy = XMVectorSelect( R0, R1, vSelectYZW );
    XMVECTOR Result = XMVectorSelect( Rxy, R2, vSelectZW );

    return Result;
}


//--------------------------------------------------------------------------------------
// Name: XMVector3TransformTransposed()
// Desc: Multiplies a vector by a transposed XMMATRIX.
//--------------------------------------------------------------------------------------
__forceinline XMVECTOR XMVector3TransformTransposed( const XMVECTOR V0, const XMMATRIX matTransform )
{
    static const XMVECTOR vSelectYZW = XMVectorSelectControl( 0, 1, 1, 1 );
    static const XMVECTOR vSelectZW = XMVectorSelectControl( 0, 0, 1, 1 );

    XMVECTOR R0 = XMVector3Dot( V0, matTransform.r[0] );
    XMVECTOR R1 = XMVector3Dot( V0, matTransform.r[1] );
    XMVECTOR R2 = XMVector3Dot( V0, matTransform.r[2] );

    XMVECTOR Rxy = XMVectorSelect( R0, R1, vSelectYZW );
    XMVECTOR Result = XMVectorSelect( Rxy, R2, vSelectZW );

    return Result;
}


//--------------------------------------------------------------------------------------
// Name: DeformMesh()
// Desc: Performs optimized VMX128 mesh deformation.
//       The rigid mesh is output to pDestBuffer.
//--------------------------------------------------------------------------------------
VOID Sample::DeformMesh( ATG::SkeletonInstance* pSkeletonInstance, ModelInfo* pModelInfo, D3DVertexBuffer* pDestBuffer )
{
    DWORD dwVertexCount = pModelInfo->dwMeshVertexCount;

    // Obtain the source vertex buffer and lock it read-only.
    D3DVertexBuffer* pSourceVB = pModelInfo->pMeshVB;
    DWORD dwSourceStride = pModelInfo->dwMeshVertexStride;
    BYTE* __restrict pSourceData = NULL;
    pSourceVB->Lock( 0, 0, ( VOID** )&pSourceData, D3DLOCK_READONLY );

    // Lock the destination VB for writing.
    BYTE* __restrict pDestData = NULL;
    pDestBuffer->Lock( 0, 0, ( VOID** )&pDestData, 0 );

    // Set up some useful constants.
    static const XMVECTOR vSelectW = XMVectorSelectControl( 0, 0, 0, 1 );
    static const XMVECTOR vOne = XMVectorReplicate( 1.0f );

    // Obtain the bone palette from the skeleton instance.
    const DWORD dwBindIndex = pModelInfo->dwSkeletonInstanceToSkinnedMeshBinding;
    const ATG::Pose3x4& BoneMatrixPalette = pSkeletonInstance->m_pSkinnedMeshBindings[ dwBindIndex ].m_BoneMatrixPalette;

    // Loop over the vertices.
    for( DWORD i = 0; i < dwVertexCount; ++i )
    {
        // Prefetch the next vertex.
        __dcbt( dwSourceStride, pSourceData );

        // Load data from the source vertex.
        XMVECTOR vPos = XMLoadFloat3( ( XMFLOAT3* )pSourceData );
        const XMVECTOR vNormal = XMLoadDecN4( ( XMDECN4* )( pSourceData + 20 ) );
        const XMVECTOR vTexCoord0 = XMLoadHalf2( ( XMHALF2* )( pSourceData + 24 ) );
        const XMVECTOR vTangent = XMLoadDecN4( ( XMDECN4* )( pSourceData + 28 ) );

        // Make sure the W component of the position is 1.
        vPos = XMVectorSelect( vPos, vOne, vSelectW );

        // Load the bone weights and bone indices.
        const XMVECTOR vBoneWeights = XMLoadUByteN4( ( XMUBYTEN4* )( pSourceData + 12 ) );
        const XMVECTOR vBoneWeightsX = XMVectorSplatX( vBoneWeights );
        const XMVECTOR vBoneWeightsY = XMVectorSplatY( vBoneWeights );
        const XMVECTOR vBoneWeightsZ = XMVectorSplatZ( vBoneWeights );
        const XMVECTOR vBoneWeightsW = XMVectorSplatW( vBoneWeights );
        const BYTE* pBoneIndices = pSourceData + 16;

        // Load the bone matrices from the bone matrix palette.
        XMMATRIX matBone0 = BoneMatrixPalette.LoadTransform( ( INT )pBoneIndices[0] );
        XMMATRIX matBone1 = BoneMatrixPalette.LoadTransform( ( INT )pBoneIndices[1] );
        XMMATRIX matBone2 = BoneMatrixPalette.LoadTransform( ( INT )pBoneIndices[2] );
        XMMATRIX matBone3 = BoneMatrixPalette.LoadTransform( ( INT )pBoneIndices[3] );

        // Transform position and blend.
        XMVECTOR vTransformedPos;
        XMVECTOR vPos0 = XMVector3TransformCoordTransposed( vPos, matBone0 );
        XMVECTOR vPos1 = XMVector3TransformCoordTransposed( vPos, matBone1 );
        XMVECTOR vPos2 = XMVector3TransformCoordTransposed( vPos, matBone2 );
        XMVECTOR vPos3 = XMVector3TransformCoordTransposed( vPos, matBone3 );
        vTransformedPos = XMVectorMultiply( vPos0, vBoneWeightsX );
        vTransformedPos = XMVectorMultiplyAdd( vPos1, vBoneWeightsY, vTransformedPos );
        vTransformedPos = XMVectorMultiplyAdd( vPos2, vBoneWeightsZ, vTransformedPos );
        vTransformedPos = XMVectorMultiplyAdd( vPos3, vBoneWeightsW, vTransformedPos );

        // Transform the normal vector and blend.
        XMVECTOR vTransformedNormal;
        XMVECTOR vNormal0 = XMVector3TransformTransposed( vNormal, matBone0 );
        XMVECTOR vNormal1 = XMVector3TransformTransposed( vNormal, matBone1 );
        XMVECTOR vNormal2 = XMVector3TransformTransposed( vNormal, matBone2 );
        XMVECTOR vNormal3 = XMVector3TransformTransposed( vNormal, matBone3 );
        vTransformedNormal = XMVectorMultiply( vNormal0, vBoneWeightsX );
        vTransformedNormal = XMVectorMultiplyAdd( vNormal1, vBoneWeightsY, vTransformedNormal );
        vTransformedNormal = XMVectorMultiplyAdd( vNormal2, vBoneWeightsZ, vTransformedNormal );
        vTransformedNormal = XMVectorMultiplyAdd( vNormal3, vBoneWeightsW, vTransformedNormal );

        // Transform the tangent vector and blend.
        XMVECTOR vTransformedTangent;
        XMVECTOR vTangent0 = XMVector3TransformTransposed( vTangent, matBone0 );
        XMVECTOR vTangent1 = XMVector3TransformTransposed( vTangent, matBone1 );
        XMVECTOR vTangent2 = XMVector3TransformTransposed( vTangent, matBone2 );
        XMVECTOR vTangent3 = XMVector3TransformTransposed( vTangent, matBone3 );
        vTransformedTangent = XMVectorMultiply( vTangent0, vBoneWeightsX );
        vTransformedTangent = XMVectorMultiplyAdd( vTangent1, vBoneWeightsY, vTransformedTangent );
        vTransformedTangent = XMVectorMultiplyAdd( vTangent2, vBoneWeightsZ, vTransformedTangent );
        vTransformedTangent = XMVectorMultiplyAdd( vTangent3, vBoneWeightsW, vTransformedTangent );

        // Store the results in the destination buffer.
        // Note the usage of write barriers here; they instruct the compiler not to
        // reorder the writes, since we're writing to write-combined memory.
        XMStoreFloat3( ( XMFLOAT3* )pDestData, vTransformedPos );
        _WriteBarrier();
        XMStoreHalf2( ( XMHALF2* )( pDestData + 12 ), vTexCoord0 );
        _WriteBarrier();
        XMStoreDecN4( ( XMDECN4* )( pDestData + 16 ), vTransformedNormal );
        _WriteBarrier();
        XMStoreDecN4( ( XMDECN4* )( pDestData + 20 ), vTransformedTangent );

        // Increment source and dest pointers.
        pSourceData += dwSourceStride;
        pDestData += g_dwRigidVertexSize;
    }

    // Unlock vertex buffers.
    pDestBuffer->Unlock();
    pSourceVB->Unlock();
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
        m_Font.DrawText( 0, 0, 0xffffffff, L"SkinnedCharacter" );
        m_Font.SetScaleFactors( 1.0f, 1.0f );
        m_Font.DrawText( 0, 0, 0xffffff00, m_Timer.GetFrameRate(), ATGFONT_RIGHT );

        WCHAR strText[256];
        swprintf_s( strText, L"%0.3f ms", m_fDeltaTime * 1000.0f );
        m_Font.SetScaleFactors( 0.9f, 0.9f );
        m_Font.DrawText( 0, 25, 0xffffff00, strText, ATGFONT_RIGHT );
        swprintf_s( strText, L"%0.4lf ms Update CPU Time", m_fUpdateTime * 1000 );
        m_Font.DrawText( 0, 50, 0xFF8080FF, strText, ATGFONT_RIGHT );
        swprintf_s( strText, L"%0.4lf ms Render CPU Time", m_fRenderTime * 1000 );
        m_Font.DrawText( 0, 70, 0xFF8080FF, strText, ATGFONT_RIGHT );

        swprintf_s( strText, L"Skinning Method %d", ( DWORD )m_SkinningMethod + 1 );
        m_Font.SetScaleFactors( 1.0f, 1.0f );
        m_Font.DrawText( 0, 30, 0xFF00FFFF, strText );
        swprintf_s( strText, L"Implementation: %s",
                    g_strSkinningMethodNames[ m_SkinningMethod ].strImplementationType );
        m_Font.DrawText( 0, 55, 0xFF00C0C0, strText );
        swprintf_s( strText, L"Bone Palette: %s", g_strSkinningMethodNames[ m_SkinningMethod ].strBonePaletteSource );
        m_Font.DrawText( 0, 80, 0xFF00C0C0, strText );


#ifndef _RELEASED3D
        D3DPERFCOUNTER_VALUES StartValues;
        m_pPerfCounterStart[ ( m_dwFrameCount + 1 ) % 3 ]->GetValues( &StartValues, 0, NULL );
        D3DPERFCOUNTER_VALUES EndValues;
        m_pPerfCounterEnd[ ( m_dwFrameCount + 1 ) % 3 ]->GetValues( &EndValues, 0, NULL );

        // Subtract start values from end values.
        UINT64* pStartValues = ( UINT64* )&StartValues;
        UINT64* pEndValues = ( UINT64* )&EndValues;
        const DWORD dwCount = sizeof( D3DPERFCOUNTER_VALUES ) / sizeof( UINT64 );
        for( DWORD i = 0; i < dwCount; ++i )
        {
            pEndValues[i] -= pStartValues[i];
        }

        FLOAT fYPos = 456.0f;

        // Display GPU busy cycle count for all 3 tiling passes.
        // This includes a small amount of overhead (ground plane, etc) but that's OK.
        m_Font.SetScaleFactors( 0.9f, 0.9f );
        swprintf_s( strText, L"GPU Cycles: %I64d", EndValues.RBBM[0].QuadPart );
        m_Font.DrawText( 0, fYPos, 0xFF8080FF, strText );
        fYPos += 20;

        // Compute constant waterfalling ratio.
        // It's defined as the number of ALU constants sent divided by the number of ALU
        // constants used.  When waterfalling is happening, this ratio will be greater
        // than 1.0.  A ratio of 2.0 means that each shader constant used was sent 
        // (fetched) twice.  Pixel shaders use constants too, so disable the pixel
        // shader to see only the vertex shader contribution.
        FLOAT fWaterfallAmount = 1.0f;
        if( EndValues.SQ[1].QuadPart > 0 )
        {
            fWaterfallAmount = ( FLOAT )EndValues.SQ[0].QuadPart / ( FLOAT )EndValues.SQ[1].QuadPart;
        }
        if( fWaterfallAmount != 1.0f )
        {
            swprintf_s( strText, L"Constant Waterfalling Ratio: %0.2f", fWaterfallAmount );
            m_Font.DrawText( 0, fYPos, 0xFF8080FF, strText );
            fYPos += 20;
        }

        // Display texture cache reads.
        swprintf_s( strText, L"Texture Cache Reads: %I64d", EndValues.MH[0].QuadPart + EndValues.MH[1].QuadPart );
        m_Font.DrawText( 0, fYPos, 0xFF8080FF, strText );
        fYPos += 20;

        // Display texture cache hits/misses.
        swprintf_s( strText, L"Texture Cache Hits/Misses: %I64d / %I64d", EndValues.TCF[0].QuadPart,
                    EndValues.TCF[1].QuadPart );
        m_Font.DrawText( 0, fYPos, 0xFF8080FF, strText );
        fYPos += 20;

        // Display vertex cache reads.
        swprintf_s( strText, L"Vertex Cache Reads: %I64d", EndValues.MH[2].QuadPart );
        m_Font.DrawText( 0, fYPos, 0xFF8080FF, strText );
        fYPos += 20;

        // Display vertex cache hits/misses.
        swprintf_s( strText, L"Vertex Cache Hits/Misses: %I64d / %I64d", EndValues.VC[0].QuadPart,
                    EndValues.VC[1].QuadPart );
        m_Font.DrawText( 0, fYPos, 0xFF8080FF, strText );
        fYPos += 20;

#endif

        m_Font.End();
    }
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
    switch( pMaterial->GetRawParameterCount() )
    {
        case 3:
            pTextureResource = ( ATG::Texture* )pMaterial->GetRawParameter( 2 ).pValue;
            if( pTextureResource != NULL )
                m_pd3dDevice->SetTexture( 2, pTextureResource->GetD3DTexture() );
        case 2:
            pTextureResource = ( ATG::Texture* )pMaterial->GetRawParameter( 1 ).pValue;
            if( pTextureResource != NULL )
                m_pd3dDevice->SetTexture( 1, pTextureResource->GetD3DTexture() );
        case 1:
            pTextureResource = ( ATG::Texture* )pMaterial->GetRawParameter( 0 ).pValue;
            if( pTextureResource != NULL )
                m_pd3dDevice->SetTexture( 0, pTextureResource->GetD3DTexture() );
        default:
            break;
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
    if( m_bNoPixelShading )
    {
        m_pd3dDevice->SetPixelShader( NULL );
    }
    else
    {
        XMVECTOR vDummy;
        XMMATRIX matWorldInv = XMMatrixInverse( &vDummy, matWorld );
        XMVECTOR vLightDir = XMVector3TransformNormal( XMLoadFloat4( &m_DirectionalLightDirection ), matWorldInv );
        m_pd3dDevice->SetPixelShader( m_pPixelShaderNormalMapping );
        m_pd3dDevice->SetPixelShaderConstantF( 0, ( FLOAT* )&vLightDir, 1 );
        m_pd3dDevice->SetPixelShaderConstantF( 1, ( FLOAT* )&m_DirectionalLightColor, 1 );
        m_pd3dDevice->SetPixelShaderConstantF( 2, ( FLOAT* )&m_AmbientColor, 1 );
        XMVECTOR vCameraPos = XMVector3TransformNormal( m_pCamera->GetWorldPosition(), matWorldInv );
        m_pd3dDevice->SetVertexShaderConstantF( 4, ( FLOAT* )&vCameraPos, 1 );
    }

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
    if( m_bNoPixelShading )
    {
        m_pd3dDevice->SetPixelShader( NULL );
    }
    else
    {
        m_pd3dDevice->SetPixelShader( m_pPixelShaderNormalMapping );
        m_pd3dDevice->SetPixelShaderConstantF( 0, ( FLOAT* )&m_DirectionalLightDirection, 1 );
        m_pd3dDevice->SetPixelShaderConstantF( 1, ( FLOAT* )&m_DirectionalLightColor, 1 );
        m_pd3dDevice->SetPixelShaderConstantF( 2, ( FLOAT* )&m_AmbientColor, 1 );
        XMVECTOR vCameraPos = m_pCamera->GetWorldPosition();
        m_pd3dDevice->SetVertexShaderConstantF( 4, ( FLOAT* )&vCameraPos, 1 );
    }

    // Set the camera matrix into vertex shader constants c0-c3.
    XMMATRIX matWVP_T = XMMatrixTranspose( m_matWVP );
    m_pd3dDevice->SetVertexShaderConstantF( 0, ( FLOAT* )&matWVP_T, 4 );

    switch( m_SkinningMethod )
    {
        case SM_VertexShaderShadowedConstants:
        {
            // Vertex shader skinning using shader constants.  

            // This is the simplest, most straightforward implementation of skinning, 
            // but it is not the most efficient since it requires copying the bone
            // matrix palette into Direct3D's shadow copy of the shader constants.

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
            break;
        }
        case SM_VertexShaderConstantBuffer:
        {
            // Vertex shader skinning using a shader constant buffer.  

            // This is similar to method 1, but instead of relying on Direct3D's shadowed
            // copy of the shader constants, the GPU loads the constants directly from
            // our triple-buffered constant buffers.  This reduces the CPU overhead of
            // the drawing operation.  In the case of this sample, the CPU overhead is
            // actually a little higher than method 1, due to calls to
            // GpuOwnVertexShaderConstantF and GpuDisownAll per object.  In a real game
            // title, these APIs would be called once per frame instead of once per
            // object, lowering the overall CPU cycle count.

            // Set the shader constant skinning vertex shader.
            m_pd3dDevice->SetVertexShader( m_pVertexShaderSkinningConstants );

            // Tell Direct3D that we are going to manage constants 12-251 ourselves.
            m_pd3dDevice->GpuOwnVertexShaderConstantF( 12, ATG::g_dwMaxBoneCount * 3 );

            // Select the current frame's constant buffer and instruct the GPU to load
            // constants from it.
            D3DConstantBuffer* pCurrentConstantBuffer = pModelInfo->pBoneMatrixConstantBuffer[ m_dwFrameCount %
                g_dwBufferCount ];
            m_pd3dDevice->GpuLoadVertexShaderConstantF4( 12, ATG::g_dwMaxBoneCount * 3, pCurrentConstantBuffer, 0 );

            // Render mesh subsets.
            for( DWORD i = 0; i < pModelInfo->dwMeshSubsetCount; ++i )
            {
                SetupMaterial( pModelInfo, i );
                pModelInfo->pSkinnedMesh->RenderSubset( i, m_pd3dDevice );
            }

            // Relinquish control of constants 12-251.
            m_pd3dDevice->GpuDisownAll();
            break;
        }
        case SM_VertexShaderVFetchVCache:
        case SM_VertexShaderVFetchTCache:
            {
                // Vertex shader skinning using vertex fetched bone matrices.

                // This method uses a vertex shader to perform skinning, but the bone matrix
                // palette is stored in vertex stream 1.  The vertex shader fetches the 
                // matrices from the stream and proceeds normally.

                // Set the vfetch skinning vertex shader.
                if( m_SkinningMethod == SM_VertexShaderVFetchVCache )
                    m_pd3dDevice->SetVertexShader( m_pVertexShaderSkinningVertexFetch );
                else
                    m_pd3dDevice->SetVertexShader( m_pVertexShaderSkinningVertexFetchTextureCache );

                // Load the bone matrix palette vertex buffer into stream 1.
                D3DVertexBuffer* pCurrentConstantVB = pModelInfo->pBoneMatrixVB[ m_dwFrameCount % g_dwBufferCount ];
                m_pd3dDevice->SetStreamSource( 1, pCurrentConstantVB, 0, 3 * sizeof( XMHALF4 ) );

                // Set the mesh data into stream 0, the mesh index buffer, and the modified
                // vertex declaration that includes the stream 1 vertex elements.
                m_pd3dDevice->SetStreamSource( 0, pModelInfo->pMeshVB, 0, pModelInfo->dwMeshVertexStride );
                m_pd3dDevice->SetIndices( pModelInfo->pMeshIB );
                m_pd3dDevice->SetVertexDeclaration( pModelInfo->pBoneMatrixVBDecl );

                // Render subsets.
                for( DWORD i = 0; i < pModelInfo->dwMeshSubsetCount; ++i )
                {
                    SetupMaterial( pModelInfo, i );
                    ATG::SubsetDesc* pSD = pModelInfo->pSkinnedMesh->GetSubsetDesc( i );
                    m_pd3dDevice->DrawIndexedPrimitive( pSD->GetPrimitiveType(), 0, 0, pModelInfo->dwMeshVertexCount,
                                                        pSD->GetStartIndex(), pSD->GetNumPrimitives() );
                }

                // Clear out vertex streams.
                m_pd3dDevice->SetStreamSource( 0, NULL, 0, 0 );
                m_pd3dDevice->SetStreamSource( 1, NULL, 0, 0 );
                break;
            }
        case SM_VertexShaderTFetch:
        {
            // Vertex shader skinning using texture fetched bone matrices.

            // This method uses a vertex shader to perform skinning, but the bone matrix
            // palette is stored in a 1D texture.  The vertex shader fetches the matrices
            // from the texture and proceeds normally.

            // Set the tfetch skinning vertex shader.
            m_pd3dDevice->SetVertexShader( m_pVertexShaderSkinningTextureFetch );

            // Load the bone matrix palette texture into vertex shader sampler 0.
            D3DTexture* pCurrentConstantTexture = pModelInfo->pBoneMatrixTexture[ m_dwFrameCount % g_dwBufferCount ];
            m_pd3dDevice->SetTexture( D3DVERTEXTEXTURESAMPLER0, pCurrentConstantTexture );

            // Render the mesh subsets.
            for( DWORD i = 0; i < pModelInfo->dwMeshSubsetCount; ++i )
            {
                SetupMaterial( pModelInfo, i );
                pModelInfo->pSkinnedMesh->RenderSubset( i, m_pd3dDevice );
            }

            // Clear out the texture.
            m_pd3dDevice->SetTexture( D3DVERTEXTEXTURESAMPLER0, NULL );
            break;
        }
        case SM_VertexShaderMemExport:
        {
            // Memory export vertex shader skinning.

            // The memory export draw will skin the vertex data and export into 
            // m_pMemoryExportVB.  The draw call will not generate any pixels.

            // This only needs to be done once per frame, on tile 0.
            m_pd3dDevice->SetPredication( D3DPRED_TILE_RENDER( 0 ) );

            // Set the memory export vertex shader.
            m_pd3dDevice->SetVertexShader( m_pVertexShaderSkinningMemExport );

            // Set a pixel shader.  This memory export vertex shader does not output any
            // interpolants, but a pixel shader must be set anyways.
            m_pd3dDevice->SetPixelShader( m_pPixelShaderSolidColor );

            // Load the bone matrix palette texture into vertex shader sampler 0.
            D3DTexture* pCurrentConstantTexture = pModelInfo->pBoneMatrixTexture[ m_dwFrameCount % g_dwBufferCount ];
            m_pd3dDevice->SetTexture( D3DVERTEXTEXTURESAMPLER0, pCurrentConstantTexture );

            // Load the memory export constants into c8, c9, and c10.
            m_pd3dDevice->SetVertexShaderConstantF( 8, pModelInfo->ExportConstantPosition.c, 1 );
            m_pd3dDevice->SetVertexShaderConstantF( 9, pModelInfo->ExportConstantNormalBinormalTangent.c, 1 );
            m_pd3dDevice->SetVertexShaderConstantF( 10, pModelInfo->ExportConstantTexCoord0.c, 1 );

            // Begin memory export.  The export destination is m_pMemoryExportVB.
            // We don't need to triple-buffer here because this VB will be used exclusively
            // by the GPU on this frame.  If we needed to use the contents on the CPU,
            // triple buffering would be recommended.
            m_pd3dDevice->BeginExport( 0, pModelInfo->pMemoryExportVB, D3DBEGINEXPORT_VERTEXSHADER );

            // Set the mesh vertex stream and decl into the Direct3D device.
            m_pd3dDevice->SetVertexDeclaration( pModelInfo->pMeshDecl );
            m_pd3dDevice->SetStreamSource( 0, pModelInfo->pMeshVB, 0, pModelInfo->dwMeshVertexStride );

            // Indices are autogenerated by the GPU.
            m_pd3dDevice->SetIndices( NULL );

            // We want to process each vertex only once.  A point list is ideal for this.
            m_pd3dDevice->DrawVertices( D3DPT_POINTLIST, 0, pModelInfo->dwMeshVertexCount );

            // End memory export.
            m_pd3dDevice->EndExport( 0, pModelInfo->pMemoryExportVB, 0 );

            // Restore predication to automatic.
            m_pd3dDevice->SetPredication( m_dwDefaultPredicationMask );

            // The second draw call actually renders the object on screen.  This is
            // identical to the CPU skinning path, except in this case, the rigid vertex
            // data was just generated by the memory export operation instead of the CPU.

            // Set the transform only vertex shader.
            m_pd3dDevice->SetVertexShader( m_pVertexShaderTransform );

            // Set the pixel shader.
            if( m_bNoPixelShading )
                m_pd3dDevice->SetPixelShader( NULL );
            else
                m_pd3dDevice->SetPixelShader( m_pPixelShaderNormalMapping );

            // Load the memory export VB into stream 0, use the rigid mesh vertex decl,
            // and use the index buffer from the mesh.
            m_pd3dDevice->SetStreamSource( 0, pModelInfo->pMemoryExportVB, 0, g_dwRigidVertexSize );
            m_pd3dDevice->SetVertexDeclaration( pModelInfo->pRigidMeshDecl );
            m_pd3dDevice->SetIndices( pModelInfo->pMeshIB );

            // Draw each subset.
            for( DWORD i = 0; i < pModelInfo->dwMeshSubsetCount; ++i )
            {
                SetupMaterial( pModelInfo, i );
                ATG::SubsetDesc* pSD = pModelInfo->pSkinnedMesh->GetSubsetDesc( i );
                m_pd3dDevice->DrawIndexedPrimitive( pSD->GetPrimitiveType(), 0, 0, pModelInfo->dwMeshVertexCount,
                                                    pSD->GetStartIndex(), pSD->GetNumPrimitives() );
            }

            // Clear out stream 0.
            m_pd3dDevice->SetStreamSource( 0, NULL, 0, 0 );
            break;
        }
        case SM_VMX128:
        {
            // VMX128 software skinning.

            // The CPU has already skinned the mesh into one of the two rigid mesh vertex
            // buffers.  Now, we will use that VB and simply draw the mesh with a very
            // simple transform only vertex shader.

            // Set the transform only vertex shader.
            m_pd3dDevice->SetVertexShader( m_pVertexShaderTransform );

            // Select the rigid mesh VB used on this frame (it's triple buffered).
            D3DVertexBuffer* pCurrentRigidVB = pModelInfo->pRigidMeshVB[ m_dwFrameCount % g_dwBufferCount ];

            // Set the rigid mesh VB, the rigid mesh vertex decl, and the index buffer
            // from the mesh.
            m_pd3dDevice->SetStreamSource( 0, pCurrentRigidVB, 0, g_dwRigidVertexSize );
            m_pd3dDevice->SetIndices( pModelInfo->pMeshIB );
            m_pd3dDevice->SetVertexDeclaration( pModelInfo->pRigidMeshDecl );

            // Render subsets.
            for( DWORD i = 0; i < pModelInfo->dwMeshSubsetCount; ++i )
            {
                SetupMaterial( pModelInfo, i );
                ATG::SubsetDesc* pSD = pModelInfo->pSkinnedMesh->GetSubsetDesc( i );
                m_pd3dDevice->DrawIndexedPrimitive( pSD->GetPrimitiveType(), 0, 0, pModelInfo->dwMeshVertexCount,
                                                    pSD->GetStartIndex(), pSD->GetNumPrimitives() );
            }

            // Clear out stream 0.
            m_pd3dDevice->SetStreamSource( 0, NULL, 0, 0 );
            break;
        }
        default:
            break;
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


//--------------------------------------------------------------------------------------
// Name: Render()
// Desc: Renders the scene and UI.
//--------------------------------------------------------------------------------------
HRESULT Sample::Render()
{
    XMMATRIX matWV = m_pCamera->GetWorldView();
    XMMATRIX matProj = m_pCamera->GetProjection().GetMatrix();
    m_matWVP = matWV * matProj;
    ATG::DebugDraw::SetViewProjection( m_matWVP );

    m_pd3dDevice->BeginScene();

    // Set up 4x MSAA render targets for tiling.
    m_pd3dDevice->SetRenderTarget( 0, m_pColorTarget4X );
    m_pd3dDevice->SetDepthStencilSurface( m_pDepthStencil4X );

    const D3DRECT pTilingRects[] =
    {
        { 0,   0, 1280, 256 },
        { 0, 256, 1280, 512 },
        { 0, 512, 1280, 720 }
    };

#ifndef _RELEASED3D
    m_pd3dDevice->QueryPerfCounters( m_pPerfCounterStart[ m_dwFrameCount % 3 ], 0 );
#endif

    // Begin tiling.
    D3DVECTOR4 ClearColor = { 0, 0, 0, 0 };
    m_pd3dDevice->BeginTiling( 0, 3, pTilingRects, &ClearColor, 1.0f, 0 );

    // Set the viewport to 1x1 if not pixel shading.
    D3DVIEWPORT9 DefaultViewport;
    if( m_bNoPixelShading )
    {
        m_pd3dDevice->GetViewport( &DefaultViewport );
        m_pd3dDevice->SetViewport( &m_NullViewport );
        // The 1x1 viewport will cause predicated tiling to skip all rendering on tiles 1 and 2,
        // so here we force it to render on all tiles.
        m_dwDefaultPredicationMask = D3DPRED_ALL_RENDER;
    }
    else
    {
        m_dwDefaultPredicationMask = 0;
    }
    m_pd3dDevice->SetPredication( m_dwDefaultPredicationMask );

    // Set default renderstate.
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_ZFUNC, D3DCMP_LESSEQUAL );
    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );

    // Render scene.
    if( m_bDrawScene )
    {
        ATG::Timer CPUTimer;

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
        // Static models are not rendered when we're not pixel shading.
        for( DWORD i = 0; i < m_dwModelCount; ++i )
        {
            if( m_pModelInfos[i].pSkinnedMesh != NULL )
            {
                RenderSkinnedModel( &m_pModelInfos[i] );
            }
            else if( !m_bNoPixelShading )
            {
                RenderStaticModel( &m_pModelInfos[i] );
            }
        }

        m_fRenderTime = CPUTimer.GetElapsedTime();
    }
    if( m_bDrawSkeleton )
    {
        RenderSkeleton();
    }

    // Render a ground plane, unless we are not pixel shading.
    if( !m_bNoPixelShading )
        ATG::DebugDraw::DrawGrid( XMFLOAT3( 20, 0, 0 ), XMFLOAT3( 0, 0, 20 ), XMFLOAT3( 0, 0, 0 ), 40, 40,
                                  0xFF404080 );

    // Restore viewport.
    if( m_bNoPixelShading )
        m_pd3dDevice->SetViewport( &DefaultViewport );

    // End tiling.
    m_pd3dDevice->EndTiling( D3DRESOLVE_RENDERTARGET0 | D3DRESOLVE_CLEARRENDERTARGET, NULL,
                             m_pResolveBuffer, &ClearColor, 1.0f, 0, NULL );

#ifndef _RELEASED3D
    m_pd3dDevice->QueryPerfCounters( m_pPerfCounterEnd[ m_dwFrameCount % 3 ], 0 );
#endif

    // Switch to a full-screen rendertarget.
    m_pd3dDevice->SetRenderTarget( 0, m_pColorTarget1X );
    m_pd3dDevice->SetDepthStencilSurface( NULL );

    // Draw resolve buffer back to the screen.
    D3DRECT ScreenSpaceRect = { 0, 0, 1280, 720 };
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

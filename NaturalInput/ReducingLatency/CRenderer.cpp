//--------------------------------------------------------------------------------------
// CRenderer.cpp
//
// Most of the renderer functionality comes from other samples, like
//      Avateering
//      PredicatedTilingFullDepth
//      TwoStageShadowMap
//      XuiAquatica
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "CRenderer.h"
#include <xtl.h>
#include <xgraphics.h>
#include <AtgUtil.h>
#include <AtgMesh.h>
#include <AtgResource.h>
#include <AtgPostProcess.h>


//--------------------------------------------------------------------------------------
// Defines and constants
//--------------------------------------------------------------------------------------

#define NEAR_CLIP       1.0f        // near clip plane for camera
#define FAR_CLIP        5000.0f     // far clip plane for camera
#define SHADOW_SIZE     1024        // shadow map resolution

const DWORD NUM_SHARKS = 2;
const DWORD NUM_TURTLES = (DWORD)(( NUM_FISH - NUM_SHARKS ) * 0.75f);
const DWORD NUM_UDS = NUM_FISH - NUM_SHARKS - NUM_TURTLES;
const DWORD FISH_FRAME_COUNT = 3;
const FLOAT FISH_RADIUS = 120.0f;

//--------------------------------------------------------------------------------------
// Statics for the class
//--------------------------------------------------------------------------------------

const XMVECTOR CRenderer::m_vLightDirection = XMVector3Normalize( XMVectorSet( -0.5f, 0.8f, 0.6f, 0.0f ) );
const XMVECTOR CRenderer::m_vEyePt = XMVectorSet( 800.0f, 0.0f, -1500.0f, 0.0f );
const XMVECTOR CRenderer::m_vLookatDir = XMVectorSet( 0.0f, 0.0f, 1.0f, 0.0f );
const XMVECTOR CRenderer::m_vUp = XMVectorSet( 0.0f, 1.0f, 0.0f, 0.0f );
const DWORD CRenderer::m_dwFogColor = D3DCOLOR_ARGB( 0xff, 65, 102, 165 );
const D3DVECTOR4 CRenderer::m_vFogColor = { 65.0f/255.0f, 102.0f/255.0f, 165.0f/255.0f, 1.0f };

// Fish types
enum FISH_TYPE
{
    FISH_TYPE_SHARK = 0,
    FISH_TYPE_TURTLE,
    FISH_TYPE_UD,
    FISH_STYLE_MAX
};


//--------------------------------------------------------------------------------------
// Class for rendering fish
//--------------------------------------------------------------------------------------

class CFish
{
public:
    CFish()
    {
        m_Type      = FISH_TYPE_SHARK;
        m_pTexture  = NULL;
        m_fRadius   = FISH_RADIUS;
    }

    HRESULT Initialize( const FISH_TYPE type, const FLOAT fRadius, ATG::PackedResource* pResource, D3DDevice* pd3dDevice );
    VOID Update( const FLOAT fTime, XMMATRIX* pModelMtx, XMVECTOR* pBlendWeights );
    VOID Render( XMMATRIX* pModelMtx, XMVECTOR* pBlendWeights, XMVECTOR vLightDir, XMMATRIX matView, XMMATRIX matProj, const ERenderPass RenderPass );

    XMVECTOR m_vPosition;
    ATG::Mesh2 m_Mesh[ FISH_FRAME_COUNT ];
    LPDIRECT3DTEXTURE9 m_pTexture;

    FLOAT m_fRadius;
    FLOAT m_fPhaseOffset;
    FLOAT m_fYOffset;
    FLOAT m_fKickOffset;
    FISH_TYPE m_Type;
    
    static LPDIRECT3DVERTEXDECLARATION9 m_pVertexDeclaration;
    static LPDIRECT3DVERTEXSHADER9 m_pVertexShader;
    static LPDIRECT3DVERTEXSHADER9 m_pVertexShaderDepth;
    static D3DDevice* m_pd3dDevice;
    static DWORD m_dwTextureIdx;

};


//--------------------------------------------------------------------------------------
// Name: VBlankCallback
// Desc: Callback for vblank
//--------------------------------------------------------------------------------------

VOID VBlankCallback( D3DVBLANKDATA* pData )
{
    PIXSetMarker(0, "VBlank Callback");
}


//--------------------------------------------------------------------------------------
// Name: SwapCallback
// Desc: Callback for swap
//--------------------------------------------------------------------------------------

VOID SwapCallback( D3DSWAPDATA *pData )
{
    PIXSetMarker( 0, "Swap Callback" );
}


//--------------------------------------------------------------------------------------
// Name: CRenderer
// Desc: Constructor
//--------------------------------------------------------------------------------------

CRenderer::CRenderer()
{
    m_pd3dDevice            = NULL;
    m_pCmdBufferDevice      = NULL;
    m_pScene                = NULL;

    ZeroMemory( m_pFish, sizeof( m_pFish ) );

    m_pAvatarRenderer       = NULL;
    m_pNuiJointConverter    = NULL;
    m_pSceneVertexDecl      = NULL;
    m_pSceneVS              = NULL;
    m_pScenePS              = NULL;

    m_pReplicateGreenVS     = NULL;
    m_pReplicateGreenPS     = NULL;

    m_pWriteDepthVS         = NULL;
    m_pWriteDepthPS         = NULL;

    m_pWriteSkinnedDepthVS  = NULL;
    m_pWriteSkinnedDepthPS  = NULL;

    m_pSimpleVertexDecl     = NULL;

    m_pScreenSpaceAAPS      = NULL;
    m_pWaterDistortionPS    = NULL;

    m_pRestoreBuffersVS     = NULL;
    m_pVertexDeclRestore    = NULL;
    m_pFastDepthRestorePS   = NULL;
    
    ZeroMemory( m_pCausticTextures, sizeof( m_pCausticTextures ) );

    m_pFrontBufferTexture       = NULL;
    m_pBackBufferTexture        = NULL;
    m_pBackBufferSurface        = NULL;
    m_pDepthStencilSurface      = NULL;

    m_pShadowMapTexture         = NULL;
    m_pHybridShadowMapTexture   = NULL;

    m_pDepthStencilSurfaceAs8888    = NULL;
    m_pDepthStencilSurfaceWith4xMSAA    = NULL;

    m_pShadowMapSurface         = NULL;
    m_pHybridShadowMapSurface   = NULL;

    m_pStateBlock               = NULL;

    ZeroMemory( m_pCmdBuffers, sizeof( m_pCmdBuffers ) );

    m_RenderPass                = RENDER_PASS_NONE;
    m_bUseHybridShadowMap       = TRUE;
    m_bUsePredicatedTiling      = FALSE;
}


//--------------------------------------------------------------------------------------
// Name: Initialize
// Desc: Initializs the renderer
//--------------------------------------------------------------------------------------

HRESULT CRenderer::Initialize( D3DPRESENT_PARAMETERS* pParams, ATG::Font* pFont )
{
    RETURN_ON_FAIL( CreateTextures( pParams ) );
    RETURN_ON_FAIL( CreateRenderTargets( pParams ) );

    // The resources will take a while to load, so show a message
    ShowAppLoadingMessage( pFont );

    RETURN_ON_FAIL( m_PostProcess.Initialize() );

    RETURN_ON_NULL( m_pScene = new ATG::Scene() );

    RETURN_ON_FAIL( m_Resource.Create( "game:\\Media\\Resource.xpr" ) );

    m_pScene->GetResourceDatabase()->AddBundledResources( &m_Resource );
    RETURN_ON_FAIL( ATG::SceneFileParser::LoadXATGFile( "game:\\Media\\scenes\\Terrain.xatg", m_pScene, NULL,
                                                        ATG::XATGLOADER_DONOTINITIALIZEMATERIALS, NULL ) );

    // Load shaders
    RETURN_ON_FAIL( ATG::LoadVertexShader( "game:\\Media\\Shaders\\ShadeSceneVertex.xvu", &m_pSceneVS ) );
    RETURN_ON_FAIL( ATG::LoadPixelShader( "game:\\Media\\Shaders\\ShadeCausticsPixel.xpu", &m_pScenePS ) );
    RETURN_ON_FAIL( ATG::LoadPixelShader( "game:\\media\\shaders\\WaterDistortionPS.xpu", &m_pWaterDistortionPS ) );
    RETURN_ON_FAIL( ATG::LoadPixelShader( "game:\\media\\shaders\\ScreenSpaceAAPS.xpu", &m_pScreenSpaceAAPS ) );
    RETURN_ON_FAIL( ATG::LoadVertexShader( "game:\\media\\shaders\\ReplicateGreenVS.xvu", &m_pReplicateGreenVS ) );
    RETURN_ON_FAIL( ATG::LoadPixelShader( "game:\\media\\shaders\\ReplicateGreenPS.xpu", &m_pReplicateGreenPS ) );
    RETURN_ON_FAIL( ATG::LoadVertexShader( "game:\\media\\shaders\\WriteDepthVS.xvu", &m_pWriteDepthVS ) );
    RETURN_ON_FAIL( ATG::LoadPixelShader( "game:\\media\\shaders\\WriteDepthPS.xpu", &m_pWriteDepthPS ) );
    RETURN_ON_FAIL( ATG::LoadVertexShader( "game:\\media\\shaders\\WriteSkinnedDepthVS.xvu", &m_pWriteSkinnedDepthVS ) );
    RETURN_ON_FAIL( ATG::LoadPixelShader( "game:\\media\\shaders\\WriteSkinnedDepthPS.xpu", &m_pWriteSkinnedDepthPS ) );
    RETURN_ON_FAIL( ATG::LoadPixelShader( "game:\\Media\\Shaders\\FastDepthRestorePS.xpu", &m_pFastDepthRestorePS ) );
    RETURN_ON_FAIL( ATG::LoadVertexShader( "game:\\Media\\Shaders\\RestoreBuffersVS.xvu", &m_pRestoreBuffersVS ) );

    // Create vertex delcarations
    static const D3DVERTEXELEMENT9 VertexElements[] =
    {
        { 0,  0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
        { 0, 12, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
        D3DDECL_END()
    };
    RETURN_ON_FAIL( m_pd3dDevice->CreateVertexDeclaration( VertexElements, &m_pSimpleVertexDecl ) );

    // Create a vertex declaration for buffer restore
    static const D3DVERTEXELEMENT9 VertexElementsPost[] =
    {
        { 0,  0, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
        D3DDECL_END()
    };
    RETURN_ON_FAIL( m_pd3dDevice->CreateVertexDeclaration( VertexElementsPost, &m_pVertexDeclRestore ) );

    // Used to retarget skeleton data to avatar
	RETURN_ON_NULL( m_pNuiJointConverter = new ATG::NuiJointConverter() );

    // Initialize the avatar library
    if( FAILED( XAvatarInitialize(     XAVATAR_COORDINATE_SYSTEM_RIGHT_HANDED, 0, AVATAR_LOAD_HW_THREAD, 0, 0) ) ) 
    {
        ATG_PrintError( "Unable to load Avatar asset pack\n");
        return E_FAIL;
    }

    // Obtain the avatar metadata for the user.  If the call fails, (the user doesn't
    // have an avatar associated with their profile yet or no one is 
    // signed in) then just load a random avatar.
    XAVATAR_METADATA metadata;
    if ( XAvatarGetMetadataLocalUser( 0, &metadata, NULL ) != ERROR_SUCCESS )
    {
        if ( XAvatarGetMetadataRandom( XAVATAR_BODY_TYPE_ALL, 1, &metadata, NULL ) != ERROR_SUCCESS )
        {
            return E_FAIL;
        }
    }

    RETURN_ON_NULL( m_pAvatarRenderer = new AvatarRenderer( m_pd3dDevice, metadata ) );

    // Set textures
    for ( DWORD i = 0; i < NUM_CAUSTIC_TEXTURES; i++ )
    {
        CHAR strTextureName[ 80 ];
        sprintf_s( strTextureName, "WaterCaustic%02ld", i );
        m_pCausticTextures[ i ] = m_Resource.GetTexture( strTextureName );
    }
   
    // Initialize the fish
    int iFish = 0;
    for ( int i = 0; i < NUM_SHARKS; i++ )
    {
        RETURN_ON_NULL( m_pFish[ iFish ] = new CFish );
        RETURN_ON_FAIL( m_pFish[ iFish++ ]->Initialize( FISH_TYPE_SHARK, FISH_RADIUS, &m_Resource, m_pd3dDevice ) );
    }

    for ( int i = 0; i < NUM_TURTLES; i++ )
    {
        RETURN_ON_NULL( m_pFish[ iFish ] = new CFish );
        RETURN_ON_FAIL( m_pFish[ iFish++ ]->Initialize( FISH_TYPE_TURTLE, FISH_RADIUS, &m_Resource, m_pd3dDevice ) );
    }

    for ( int i = 0; i < NUM_UDS; i++ )
    {
        RETURN_ON_NULL( m_pFish[ iFish ] = new CFish );
        RETURN_ON_FAIL( m_pFish[ iFish++ ]->Initialize( FISH_TYPE_UD, FISH_RADIUS, &m_Resource, m_pd3dDevice ) );
    }
    
    m_pd3dDevice->CreateStateBlock( D3DSBT_ALL, &m_pStateBlock );

    // Determine the aspect ratio
    FLOAT fAspectRatio = ( FLOAT )pParams->BackBufferWidth / ( FLOAT )pParams->BackBufferHeight;
    m_matProj = XMMatrixPerspectiveFovLH( XM_PI / 3.0f, fAspectRatio, NEAR_CLIP, FAR_CLIP );

    // Bind the vertex shaders so that it's is more efficient for use with command buffers
    BindSceneVertexDeclaration();
    
    // Record command buffers for static scene
    RETURN_ON_NULL( m_pCmdBufferDevice );
    for ( UINT i = 0; i < NUM_CMD_BUFFER_TYPES; i++ )
    {
        RETURN_ON_FAIL( RecordCommandBuffers( (ECmdBufferType)i ) );
    }

    // Setup shadowmap matrices
    const FLOAT fRadius = 700.0f;
    const FLOAT fNear = 0.1f;
    const FLOAT fFar = fNear + fRadius * 2.0f;
    const XMVECTOR vUp = XMVectorSet( 0.0f, 1.0f, 0.0001f, 0.0f );
    const XMVECTOR vFrom = XMVectorSet( 400.0f, 400.0f, -100.0f, 0.0f );
    const XMVECTOR vDir = -m_vLightDirection;;
    m_matLightView = XMMatrixLookAtLH( vFrom, vFrom + vDir, vUp );

    // Include the entire world in the orthographic projection.
    const FLOAT fWidth = fRadius * 2.0f;
    const FLOAT fHeight = fRadius * 2.0f;

    m_matLightProj = XMMatrixOrthographicLH( fWidth, fHeight, fNear, fFar );

    // Setup the texture matrix.
    XMMATRIX matTexture( 0.5f,  0.0f,  0.0f,  0.0f,
                         0.0f, -0.5f,  0.0f,  0.0f,
                         0.0f,  0.0f,  1.0f,  0.0f,
                         0.5f,  0.5f,  0.0f,  1.0f );

    m_matShadowViewProj = m_matLightView * m_matLightProj * matTexture;
    XMMATRIX matShadowTex = m_matShadowViewProj;
    m_matShadowViewProj = XMMatrixTranspose( matShadowTex );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: CreateDevice
// Desc: Create the d3d device
//--------------------------------------------------------------------------------------

HRESULT CRenderer::CreateDevice( D3DPRESENT_PARAMETERS* pParams )
{
    if ( !m_pd3dDevice )
    {
        Direct3D* pD3D = Direct3DCreate9( D3D_SDK_VERSION );

        // Create the D3D device
        RETURN_ON_FAIL( pD3D->CreateDevice( 0, D3DDEVTYPE_HAL, NULL,
                                             D3DCREATE_BUFFER_2_FRAMES | D3DCREATE_CREATE_THREAD_ON_2,
                                             pParams, ( ::D3DDevice** )&m_pd3dDevice ) );
       
        // Allow global access to the device
        ATG::g_pd3dDevice = (ATG::D3DDevice*)m_pd3dDevice;

        // Create the command buffer device
        RETURN_ON_FAIL( pD3D->CreateDevice( 0, D3DDEVTYPE_COMMAND_BUFFER, NULL, 0, NULL, ( ::D3DDevice** )&m_pCmdBufferDevice ) );

        SAFE_RELEASE( pD3D );
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: CreateTextures
// Desc: Create textures used in the sample
//--------------------------------------------------------------------------------------

HRESULT CRenderer::CreateTextures( D3DPRESENT_PARAMETERS* pParams )
{
    UINT uWidth  = pParams->BackBufferWidth;
    UINT uHeight = pParams->BackBufferHeight;

    // Create front buffer texture.
    RETURN_ON_FAIL( m_pd3dDevice->CreateTexture( uWidth, uHeight, 1, 0, pParams->FrontBufferFormat, D3DPOOL_DEFAULT, &m_pFrontBufferTexture, NULL ) );
    m_PostProcess.ClearTexture( m_pFrontBufferTexture, m_dwFogColor );

    // Create backbuffer textures.
    RETURN_ON_FAIL( m_pd3dDevice->CreateTexture( uWidth, uHeight, 1, 0, pParams->BackBufferFormat, D3DPOOL_DEFAULT, &m_pBackBufferTexture, NULL ) );
    m_PostProcess.ClearTexture( m_pBackBufferTexture, m_dwFogColor );

    // Set up an alias of the resolve texture as an AS_16 sRGB format, since the GPU can't resolve to an AS_16_16_16_16 format. 
    // Alias this texture so there's no loss of precision when sampling the texture in the shader. If using
    // a standard SRGB format, the sRGB->Linear conversion happens with 8 bit precision, resulting 
    // in a loss of data. Using an AS_16 sRGB format causes the conversion to happen at 16-bit precision,
    // meaning no precision is lost.
    m_BackBufferTextureAs16SRGB = *m_pBackBufferTexture;
    ATG::ConvertTextureToAs16SRGBFormat( &m_BackBufferTextureAs16SRGB );

    // Create depthbuffer texture.
    RETURN_ON_FAIL( m_pd3dDevice->CreateTexture( uWidth, uHeight, 1, 0, pParams->AutoDepthStencilFormat, D3DPOOL_DEFAULT, &m_pDepthBufferTexture, NULL ) );

    // Create an alias of the depth texture as 8888 for the fast depth restore
    XGSetTextureHeader( uWidth, uHeight, 1, 0, D3DFMT_A8R8G8B8, 0, 0, 0, 0, &m_DepthBufferTextureAs8888, 0, 0 );
    XGOffsetResourceAddress( &m_DepthBufferTextureAs8888, (VOID*)(m_pDepthBufferTexture->Format.BaseAddress << GPU_TEXTURE_ADDRESS_SHIFT) );

    // Create the shadowmap texture.
    m_pShadowMapTexture = new IDirect3DTexture9;
    RETURN_ON_NULL( m_pShadowMapTexture );

    DWORD dwTextureSize = XGSetTextureHeaderEx( SHADOW_SIZE, SHADOW_SIZE, 1, 0, D3DFMT_D24S8, 0, XGHEADEREX_NONPACKED, 0, 0, 0, m_pShadowMapTexture, NULL, NULL );
    void* pBuffer = NULL;
    RETURN_ON_NULL( pBuffer = XPhysicalAlloc( dwTextureSize, MAXULONG_PTR, 0, PAGE_READWRITE | PAGE_WRITECOMBINE ) );

    XGOffsetResourceAddress( m_pShadowMapTexture, pBuffer );

    // Create the hybrid shadow map
    m_pHybridShadowMapTexture = new IDirect3DTexture9;
    RETURN_ON_NULL( m_pHybridShadowMapTexture );

    XGSetTextureHeaderEx( SHADOW_SIZE, SHADOW_SIZE, 1, 0, ATG::D3DFMT_G16R16_SIGNED_INTEGER, 0, XGHEADEREX_NONPACKED, 0, 0, 0, m_pHybridShadowMapTexture, NULL, NULL );
    
    // convert int to [-1,1]
    m_pHybridShadowMapTexture->Format.ExpAdjust = -15;

    // Alias the same memory as the literal shadow map
    XGOffsetResourceAddress( m_pHybridShadowMapTexture, pBuffer );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: CreateRenderTargets
// Desc: Create render targets for sample
//--------------------------------------------------------------------------------------

HRESULT CRenderer::CreateRenderTargets( D3DPRESENT_PARAMETERS* pParams )
{
    UINT uWidth = pParams->BackBufferWidth;
    UINT uHeight = pParams->BackBufferHeight;
    UINT uRenderTargetSize = XGSurfaceSize( uWidth, uHeight, pParams->BackBufferFormat, D3DMULTISAMPLE_NONE );

    D3DSURFACE_PARAMETERS renderTargetParams;
    D3DSURFACE_PARAMETERS depthStencilParams;

    memset( &renderTargetParams, 0, sizeof( D3DSURFACE_PARAMETERS ) );
    memset( &depthStencilParams, 0, sizeof( D3DSURFACE_PARAMETERS ) );

    depthStencilParams.Base = uRenderTargetSize;
    depthStencilParams.HierarchicalZBase = 0;

    // Create non-tiled backbuffer render target
    RETURN_ON_FAIL( m_pd3dDevice->CreateRenderTarget( uWidth, uHeight, pParams->BackBufferFormat, D3DMULTISAMPLE_NONE,
                                                        0, FALSE, &m_pBackBufferSurface, &renderTargetParams ) );

    // Create non-tiled depth buffer
    RETURN_ON_FAIL( m_pd3dDevice->CreateDepthStencilSurface( uWidth, uHeight, pParams->AutoDepthStencilFormat, D3DMULTISAMPLE_NONE,
                                                                0, FALSE, &m_pDepthStencilSurface, &depthStencilParams ) );


    // Clear these targets
    m_pd3dDevice->SetRenderTarget( 0, m_pBackBufferSurface );
    m_pd3dDevice->SetDepthStencilSurface( m_pDepthStencilSurface );
    m_pd3dDevice->Clear( 0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, m_dwFogColor, 1.0f, 0 );


    // The 8888 version of the depth buffer is placed at the same address in EDRAM for the fast depth restore
    RETURN_ON_FAIL( m_pd3dDevice->CreateRenderTarget( uWidth, uHeight, D3DFMT_A8R8G8B8, D3DMULTISAMPLE_NONE,
                                                        0, FALSE, &m_pDepthStencilSurfaceAs8888, &depthStencilParams ) );

    // Create a 4xMSAA depth target of half size to serve as a full screen quad when restoring the Hi-Z
    RETURN_ON_FAIL( m_pd3dDevice->CreateDepthStencilSurface( uWidth >> 1, uHeight >> 1, pParams->AutoDepthStencilFormat, D3DMULTISAMPLE_4_SAMPLES,
                                                                0, FALSE, &m_pDepthStencilSurfaceWith4xMSAA, &depthStencilParams ) );

    // Create a depth buffer to write out shadow into.  Note that his overlaps in EDRAM with the main render target and z-buffer.
    D3DSURFACE_PARAMETERS surfaceParameters;
    memset( &surfaceParameters, 0, sizeof( D3DSURFACE_PARAMETERS ) );
    surfaceParameters.Base = 0;
    surfaceParameters.HierarchicalZBase = 0;

    RETURN_ON_FAIL( m_pd3dDevice->CreateDepthStencilSurface( SHADOW_SIZE, SHADOW_SIZE, D3DFMT_D24S8, D3DMULTISAMPLE_NONE, 0, FALSE,
                                                                &m_pShadowMapSurface, &surfaceParameters ) );

    // Create the hybrid shadow surface, aliasing the same memory as the literal shadow surface
    surfaceParameters.ColorExpBias = +5;    // D3DFMT_G16R16_EDRAM has range [0,32]
    RETURN_ON_FAIL( m_pd3dDevice->CreateRenderTarget( SHADOW_SIZE, SHADOW_SIZE, D3DFMT_G16R16_EDRAM, D3DMULTISAMPLE_NONE, 0, FALSE,
                                                        &m_pHybridShadowMapSurface, &surfaceParameters ) );

    // Set up tiling rectangles and copy them to the tiling rect array.
    m_pTilingRects[ 0 ].x1 = 0;
    m_pTilingRects[ 0 ].x2 = 1280;
    m_pTilingRects[ 0 ].y1 = 0;
    m_pTilingRects[ 0 ].y2 = 384;

    m_pTilingRects[ 1 ].x1 = 0;
    m_pTilingRects[ 1 ].x2 = 1280;
    m_pTilingRects[ 1 ].y1 = 384;
    m_pTilingRects[ 1 ].y2 = 720;

    // Compute tile width and height.  The tiling render targets will be created using these dimensions.
    DWORD dwTileWidth = m_pTilingRects[ 0 ].x2;
    DWORD dwTileHeight = m_pTilingRects[ 0 ].y2;

    // Expand tile surface dimensions to texture tile size
    dwTileWidth = XGNextMultiple( dwTileWidth, GPU_TEXTURE_TILE_DIMENSION );
    dwTileHeight = XGNextMultiple( dwTileHeight, GPU_TEXTURE_TILE_DIMENSION );

    // Use custom EDRAM allocation to create the render targets. The first render target is placed at address 0 in EDRAM.
    D3DSURFACE_PARAMETERS surfaceParams;
    memset( &surfaceParams, 0, sizeof( D3DSURFACE_PARAMETERS ) );
    surfaceParams.HierarchicalZBase = 0;

    // Create a tiled render target
    RETURN_ON_FAIL( m_pd3dDevice->CreateRenderTarget( dwTileWidth, dwTileHeight, pParams->BackBufferFormat, D3DMULTISAMPLE_2_SAMPLES,
                                                        0, FALSE, &m_pTiledBackBufferSurface, &surfaceParams ) );

    // Record the size of the created render target, and then set up allocation
    // for the next render target right after the end of the first render target.
    surfaceParams.Base = XGSurfaceSize( dwTileWidth, dwTileHeight, pParams->BackBufferFormat, D3DMULTISAMPLE_2_SAMPLES );

    // Put the hierarchical Z buffer at the start of hierarchical Z memory.
    surfaceParams.HierarchicalZBase = 0;

    // Create a tiled depth render target
    RETURN_ON_FAIL( m_pd3dDevice->CreateDepthStencilSurface( dwTileWidth, dwTileHeight, pParams->AutoDepthStencilFormat, D3DMULTISAMPLE_2_SAMPLES,
                                                                0, FALSE, &m_pTiledDepthStencilSurface, &surfaceParams ) );


    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Render
// Desc: Render geometry based on the input flags
//--------------------------------------------------------------------------------------

VOID CRenderer::Render( const ERender Render, CFrameBufferData* pFrameBufferData )
{
    switch ( Render )
    {
    case RENDER_SCENE:
        SetRenderStatesAndConstants( pFrameBufferData );
        RenderScene( pFrameBufferData );
        break;

    case RENDER_SCENE_FLOOR:
        SetRenderStatesAndConstants( pFrameBufferData );
        RenderScene( pFrameBufferData, TRUE );
        break;

    case RENDER_FISH:
        RenderFish( pFrameBufferData );
        break;

    case RENDER_AVATAR:
        RenderAvatar( pFrameBufferData );
        break;
    }
}


//--------------------------------------------------------------------------------------
// Name: RenderPostEffects
// Desc: Render post effects
//--------------------------------------------------------------------------------------

VOID CRenderer::RenderPostEffects( const DWORD dwFlags, CFrameBufferData* pFrameBufferData )
{
    // Bias towards pixel shader
    m_pd3dDevice->SetShaderGPRAllocation( 0, 16, GPU_GPRS - 16 );

    D3DTexture* pCurrentCausticTexture = pFrameBufferData->m_pCurrentCausticTexture;

    m_pd3dDevice->SetRenderTarget( 0, m_pBackBufferSurface );
    m_pd3dDevice->SetDepthStencilSurface( m_pDepthStencilSurface );
   
    if ( dwFlags & RENDER_POST_SCREEN_SPACE_AA )
    {
        PIXBeginNamedEvent( 0, "ScreenSpaceAA" );

        // save the back buffer and depth buffer
        m_pd3dDevice->Resolve( D3DRESOLVE_RENDERTARGET0, NULL, m_pBackBufferTexture, NULL, 0, 0, NULL, 1.0f, 0, NULL );
        m_pd3dDevice->Resolve( D3DRESOLVE_DEPTHSTENCIL, NULL, m_pDepthBufferTexture, NULL, 0, 0, NULL, 1.0f, 0, NULL );

        m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, FALSE );
        m_pd3dDevice->SetRenderState( D3DRS_ZWRITEENABLE, FALSE );
        m_pd3dDevice->SetRenderState( D3DRS_HIZENABLE, FALSE );
        m_pd3dDevice->SetRenderState( D3DRS_HIZWRITEENABLE, FALSE );

        m_pd3dDevice->SetDepthStencilSurface( NULL );
        
        ATG::g_pd3dDevice->SetTexture( 0, m_pDepthBufferTexture );
        ATG::g_pd3dDevice->SetTexture( 1, m_pBackBufferTexture );
        ATG::g_pd3dDevice->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_POINT );
        ATG::g_pd3dDevice->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_POINT );
        ATG::g_pd3dDevice->SetSamplerState( 0, D3DSAMP_MIPFILTER, D3DTEXF_POINT );
        ATG::g_pd3dDevice->SetSamplerState( 1, D3DSAMP_MINFILTER, D3DTEXF_POINT );
        ATG::g_pd3dDevice->SetSamplerState( 1, D3DSAMP_MAGFILTER, D3DTEXF_POINT );
        ATG::g_pd3dDevice->SetSamplerState( 1, D3DSAMP_MIPFILTER, D3DTEXF_POINT );

        static XMVECTOR fScreenSpaceAAParams = XMVectorSet( 0.00002f, 0.0f, 0.0f, 0.0f );
        ATG::g_pd3dDevice->SetPixelShaderConstantF( 0, ( FLOAT* )&fScreenSpaceAAParams, 1 );

        m_pd3dDevice->SetPixelShader( m_pScreenSpaceAAPS );
        m_PostProcess.DrawFullScreenQuad();

        PIXEndNamedEvent();
    }

    if ( dwFlags & RENDER_POST_WATER_DISTORTION )
    {
        PIXBeginNamedEvent( 0, "WaterDistortion" );

        if ( dwFlags & RENDER_POST_SCREEN_SPACE_AA )
        {
            m_pd3dDevice->Resolve( D3DRESOLVE_RENDERTARGET0, NULL, m_pBackBufferTexture, NULL, 0, 0, NULL, 1.0f, 0, NULL );
        }

        m_pd3dDevice->SetTexture( 0, pCurrentCausticTexture );
        m_pd3dDevice->SetTexture( 1, &m_BackBufferTextureAs16SRGB );
        m_pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSU, D3DTADDRESS_MIRROR );
        m_pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSV, D3DTADDRESS_MIRROR );
        m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_ANISOTROPIC );
        m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_ANISOTROPIC );
        m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MIPFILTER, D3DTEXF_POINT );
        m_pd3dDevice->SetSamplerState( 1, D3DSAMP_MINFILTER, D3DTEXF_POINT );
        m_pd3dDevice->SetSamplerState( 1, D3DSAMP_MAGFILTER, D3DTEXF_POINT );
        m_pd3dDevice->SetSamplerState( 1, D3DSAMP_MIPFILTER, D3DTEXF_POINT );

        static XMVECTOR fDistortionParams = XMVectorSet( 3.0f, 2.0f, 0.0075f, 0.0075f );
        ATG::g_pd3dDevice->SetPixelShaderConstantF( 0, ( FLOAT* )&fDistortionParams, 1 );

        m_pd3dDevice->SetPixelShader( m_pWaterDistortionPS );
        m_PostProcess.DrawFullScreenQuad();

        PIXEndNamedEvent();
    }
}


//--------------------------------------------------------------------------------------
// Name: Present
// Desc: Resolve and present the front buffer
//--------------------------------------------------------------------------------------

VOID CRenderer::Present()
{
    PIXBeginNamedEvent( 0, "Present" );

    // Make sure the device is clean for the next frame
    m_pd3dDevice->UnsetAll();

    // Reset GPRs at end of frame
    m_pd3dDevice->SetShaderGPRAllocation( 0, 64, 64 );

    // Want to clear the color and depth when resolving
    m_pd3dDevice->SetRenderTarget( 0, m_pBackBufferSurface );
    m_pd3dDevice->SetDepthStencilSurface( m_pDepthStencilSurface );

    // Wait for the vertical blank before we resolve to the front buffer to avoid tearing.
    m_pd3dDevice->SynchronizeToPresentationInterval();

    // Resolve the final image to the front buffer.
    m_pd3dDevice->Resolve( D3DRESOLVE_RENDERTARGET0 | D3DRESOLVE_CLEARRENDERTARGET | D3DRESOLVE_CLEARDEPTHSTENCIL,
                            NULL, m_pFrontBufferTexture, NULL, 0, 0, &m_vFogColor, 1.0f, 0, NULL );

    // Present the scene.
    m_pd3dDevice->Swap( m_pFrontBufferTexture, NULL );

    PIXEndNamedEvent();
}


//--------------------------------------------------------------------------------------
// Name: Update
// Desc: Update some render data
//--------------------------------------------------------------------------------------

VOID CRenderer::Update( CFrameBufferData* pFrameBufferData, const FLOAT fTime )
{
    // Update view matrix with some movement
    XMVECTOR vCameraMovement = XMVectorSet( 0, cosf( fTime ) / 50.0f, 0, 0 );
    pFrameBufferData->m_matView = XMMatrixLookAtLH( m_vEyePt + vCameraMovement, m_vEyePt + m_vLookatDir, m_vUp );

    // Animate the caustic textures
    DWORD tex = ( ( DWORD )( fTime * NUM_CAUSTIC_TEXTURES ) ) % NUM_CAUSTIC_TEXTURES;
    pFrameBufferData->m_pCurrentCausticTexture = m_pCausticTextures[ tex ];

    // Update fish
    for ( int i = 0; i < NUM_FISH; i++ )
    {
        m_pFish[ i ]->Update( fTime, &pFrameBufferData->m_matModel[ i ], &pFrameBufferData->m_vBlendWeights[ i ] );
    }
}


//--------------------------------------------------------------------------------------
// Name: UpdateAvatar
// Desc: Update the avatar rendering data
//--------------------------------------------------------------------------------------

VOID CRenderer::UpdateAvatar( CFrameBufferData* pFrameBufferData, const FLOAT fTime )
{
    if ( m_pNuiJointConverter )
    {
        if ( pFrameBufferData->m_iSkeletonIdx >= 0 &&
             pFrameBufferData->m_SkeletonFrame.SkeletonData[ pFrameBufferData->m_iSkeletonIdx ].eTrackingState == NUI_SKELETON_TRACKED )
        {
            const XMVECTOR vPos = XMVectorSet( 800.0f, -40.0f, -1300.0f, 1.0f );
            const XMVECTOR vScale = XMVectorSet( -200.0f, 200.0f, 200.0f, 1.0f );
            XMMATRIX rotMtx = XMMatrixRotationY( XM_PI );
            XMMATRIX transMtx = XMMatrixTranslation( vPos.x, vPos.y, vPos.z );
            XMMATRIX scaleMtx = XMMatrixScaling( vScale.x, vScale.y, vScale.z );
            pFrameBufferData->m_matWorldAvatar = scaleMtx * rotMtx * transMtx;

            NUI_SKELETON_DATA* pSkeletoData = &pFrameBufferData->m_SkeletonFrame.SkeletonData[ pFrameBufferData->m_iSkeletonIdx ];
            m_pNuiJointConverter->ConvertNuiToAvatarSkeleton( pSkeletoData, pFrameBufferData->m_AvatarJointPose );
       }

        m_pAvatarRenderer->SetJoints( pFrameBufferData->m_AvatarJointPose );
        m_pAvatarRenderer->Update();
    }
}


//--------------------------------------------------------------------------------------
// Name: Reset
// Desc: Reset the renderer
//--------------------------------------------------------------------------------------

VOID CRenderer::Reset()
{
    // Make sure the GPU is done rendering before switching modes
    DWORD dwFence = m_pd3dDevice->InsertFence();
    m_pd3dDevice->BlockOnFence( dwFence );
    m_pd3dDevice->UnsetAll();
}


//--------------------------------------------------------------------------------------
// Name: ShowAppLoadingMessage
// Desc: Show a "loading" message while loadig all the resources
//--------------------------------------------------------------------------------------

VOID CRenderer::ShowAppLoadingMessage( ATG::Font* pFont )
{
    pFont->SetWindow( ATG::GetTitleSafeArea() );

    m_pd3dDevice->SetRenderTarget( 0, m_pBackBufferSurface );
    m_pd3dDevice->SetDepthStencilSurface( m_pDepthStencilSurface );
    m_pd3dDevice->Clear( 0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, m_dwFogColor, 1.0f, 0 );

    // Show loading... message
    D3DSURFACE_DESC Desc;
    m_pBackBufferSurface->GetDesc( &Desc );

    pFont->Begin();
    pFont->SetScaleFactors( 2.0f, 2.0f );
    pFont->DrawText( 0, Desc.Height / 2.0f, 0xffffffff, L"Loading...       ", ATGFONT_RIGHT );
    pFont->End();

    Present();
}


//--------------------------------------------------------------------------------------
// Name: RecordCommandBuffers
// Desc: Record command buffers for static scene objects
//--------------------------------------------------------------------------------------

HRESULT CRenderer::RecordCommandBuffers( const ECmdBufferType CmdBufferType )
{
    assert( CmdBufferType < ARRAY_SIZE( m_pCmdBuffers ) );

    DWORD dwFlags;
    D3DRECT* pTilingRect;
    DWORD dwTileCount;

    switch ( CmdBufferType )
    {
    case CMD_BUFFER_TYPE_NON_TILED:
        dwTileCount = 0;
        pTilingRect = NULL;
        dwFlags = D3DBEGINCB_OVERWRITE_INHERITED_STATE;
        m_pCmdBufferDevice->SetRenderTarget( 0, m_pBackBufferSurface );
        m_pCmdBufferDevice->SetDepthStencilSurface( m_pDepthStencilSurface );      
        break;

    case CMD_BUFFER_TYPE_TILED:
        dwTileCount = 2;
        pTilingRect = m_pTilingRects;
        dwFlags = D3DBEGINCB_OVERWRITE_INHERITED_STATE | D3DBEGINCB_TILING_PREDICATE_COMPONENTS | D3DBEGINCB_ZPASS;
        m_pCmdBufferDevice->SetRenderTarget( 0, m_pTiledBackBufferSurface );
        m_pCmdBufferDevice->SetDepthStencilSurface( m_pTiledDepthStencilSurface );
        break;

    default:
        return E_FAIL;
    }

    // Specify the GPU tags to inherit.
    D3DTAGCOLLECTION InheritTags = { 0 };
    D3DTagCollection_SetAll( &InheritTags );

    // Create the command buffer for the GPU commands.
    RETURN_ON_FAIL( m_pd3dDevice->CreateCommandBuffer( 320 * 1024, 0, &m_pCmdBuffers[ CmdBufferType ][ CMD_BUFFER_SEA_OPAQUE ] ) );
    RETURN_ON_FAIL( m_pd3dDevice->CreateCommandBuffer( 64 * 1024, 0, &m_pCmdBuffers[ CmdBufferType ][ CMD_BUFFER_SEA_TRANSPARENT ] ) );
    RETURN_ON_FAIL( m_pd3dDevice->CreateCommandBuffer( 64 * 1024, 0, &m_pCmdBuffers[ CmdBufferType ][ CMD_BUFFER_SEA_FLOOR ] ) );

    m_pCmdBufferDevice->SetPixelShader( m_pScenePS );
    m_pCmdBufferDevice->SetVertexShader( m_pSceneVS );
    m_pCmdBufferDevice->SetVertexDeclaration( NULL );

    // CMD_BUFFER_SEA_OPAQUE
    {
        RETURN_ON_FAIL( m_pCmdBufferDevice->BeginCommandBuffer( m_pCmdBuffers[ CmdBufferType ][ CMD_BUFFER_SEA_OPAQUE ],
                                                                dwFlags, &InheritTags, NULL, pTilingRect, dwTileCount ) );

        m_pCmdBufferDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );

        ATG::NameIndexedCollection::iterator i;
        for ( i = m_pScene->GetInstanceList()->begin(); i != m_pScene->GetInstanceList()->end(); i++ )
        {
            // Select models from the object list.
            if ( ( *i )->IsDerivedFrom( ATG::Model::TypeID ) )
            {
                ATG::Model* pModel = ( ATG::Model* )( *i );

                // Loop over mesh mappings.
                DWORD dwMeshMappingCount = pModel->GetNumMeshMappings();
                for( DWORD dwMapIndex = 0; dwMapIndex < dwMeshMappingCount; ++dwMapIndex )
                {
                    ATG::MeshMapping& mm = pModel->GetMeshMapping( dwMapIndex );
                    ATG::BaseMesh* pMesh = mm.pMesh;

                    // Loop over mesh subsets.
                    DWORD dwSubsetCount = pMesh->GetNumSubsets();
                    for ( DWORD dwSubsetIndex = 0; dwSubsetIndex < dwSubsetCount; ++dwSubsetIndex )
                    {
                        ATG::MaterialInstance* pMaterial = mm.Materials[ dwSubsetIndex ];
                        
                        if ( wcscmp( pMaterial->GetName().GetSafeString(), L"sand" ) )
                        {
                            if ( !pMaterial->IsTransparent() )
                            {
                                m_pCmdBufferDevice->SetVertexShaderConstantF( 25, ( FLOAT* )&pModel->GetWorldTransform(), 4 );

                                // Retrieve diffuse texture name
                                ATG::MaterialParameter& param = pMaterial->GetRawParameter( 0 );

                                if ( param.pValue != NULL )
                                {
                                    ATG::Texture2D* pTex2D = ( ATG::Texture2D* )param.pValue;
                                    m_pCmdBufferDevice->SetTexture( 0, pTex2D->GetD3DTexture() );
                                }

                                // Retreive normal map
                                if ( pMaterial->GetRawParameterCount() >= 2 )
                                {
                                    ATG::MaterialParameter& prm = pMaterial->GetRawParameter( 1 );

                                    if ( prm.pValue != NULL )
                                    {
                                        ATG::Texture2D* pTex2D = ( ATG::Texture2D* )prm.pValue;
                                        m_pCmdBufferDevice->SetTexture( 2, pTex2D->GetD3DTexture() );
                                    }
                                }

                                // Render the mesh subset.
                                pMesh->RenderSubset( dwSubsetIndex, m_pCmdBufferDevice, ATG::BaseMesh::NoVertexDecl );//, ATG::BaseMesh::BindDecl );
                            }
                        }
                    }
                }
            }
        }

        RETURN_ON_FAIL( m_pCmdBufferDevice->EndCommandBuffer() );
    }


    // CMD_BUFFER_SEA_TRANSPARENT   
    {
#pragma warning( push )
#pragma warning( disable : 6385 ) // Suppress spurious prefast warning
        RETURN_ON_FAIL( m_pCmdBufferDevice->BeginCommandBuffer( m_pCmdBuffers[ CmdBufferType ][ CMD_BUFFER_SEA_TRANSPARENT ],
                                                                dwFlags, &InheritTags, NULL, pTilingRect, dwTileCount ) );
#pragma warning( pop )

        m_pCmdBufferDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, TRUE );
        m_pCmdBufferDevice->SetRenderState( D3DRS_SRCBLEND, D3DBLEND_SRCALPHA );
        m_pCmdBufferDevice->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA );

        ATG::NameIndexedCollection::iterator i;
        for ( i = m_pScene->GetInstanceList()->begin(); i != m_pScene->GetInstanceList()->end(); i++ )
        {
            // Select models from the object list.
            if ( ( *i )->IsDerivedFrom( ATG::Model::TypeID ) )
            {
                ATG::Model* pModel = ( ATG::Model* )( *i );

                // Loop over mesh mappings.
                DWORD dwMeshMappingCount = pModel->GetNumMeshMappings();
                for ( DWORD dwMapIndex = 0; dwMapIndex < dwMeshMappingCount; ++dwMapIndex )
                {
                    ATG::MeshMapping& mm = pModel->GetMeshMapping( dwMapIndex );
                    ATG::BaseMesh* pMesh = mm.pMesh;

                    // Loop over mesh subsets.
                    DWORD dwSubsetCount = pMesh->GetNumSubsets();
                    for ( DWORD dwSubsetIndex = 0; dwSubsetIndex < dwSubsetCount; ++dwSubsetIndex )
                    {
                        ATG::MaterialInstance* pMaterial = mm.Materials[ dwSubsetIndex ];
                        if ( pMaterial->IsTransparent() )
                        {
                            m_pCmdBufferDevice->SetVertexShaderConstantF( 25, ( FLOAT* )&pModel->GetWorldTransform(), 4 );

                            // Retrieve diffuse texture name
                            ATG::MaterialParameter& param = pMaterial->GetRawParameter( 0 );

                            if ( param.pValue != NULL )
                            {
                                ATG::Texture2D* pTex2D = ( ATG::Texture2D* )param.pValue;
                                m_pCmdBufferDevice->SetTexture( 0, pTex2D->GetD3DTexture() );
                            }

                            // Retreive normal map
                            if ( pMaterial->GetRawParameterCount() >= 2 )
                            {
                                ATG::MaterialParameter& prm = pMaterial->GetRawParameter( 1 );

                                if ( prm.pValue != NULL )
                                {
                                    ATG::Texture2D* pTex2D = ( ATG::Texture2D* )prm.pValue;
                                    m_pCmdBufferDevice->SetTexture( 2, pTex2D->GetD3DTexture() );
                                }
                            }

                            // Render the mesh subset.
                            pMesh->RenderSubset( dwSubsetIndex, m_pCmdBufferDevice, ATG::BaseMesh::NoVertexDecl );
                        }
                    }
                }
            }
        }

        RETURN_ON_FAIL( m_pCmdBufferDevice->EndCommandBuffer() );

    }
    
    // CMD_BUFFER_SEA_FLOOR
    {
#pragma warning( push )
#pragma warning( disable : 6385 ) // Suppress spurious prefast warning
        RETURN_ON_FAIL( m_pCmdBufferDevice->BeginCommandBuffer( m_pCmdBuffers[ CmdBufferType ][ CMD_BUFFER_SEA_FLOOR ],
                                                                dwFlags, &InheritTags, NULL, pTilingRect, dwTileCount ) );
#pragma warning( pop )

        m_pCmdBufferDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );

        ATG::NameIndexedCollection::iterator i;
        for ( i = m_pScene->GetInstanceList()->begin(); i != m_pScene->GetInstanceList()->end(); i++ )
        {
            // Select models from the object list.
            if ( ( *i )->IsDerivedFrom( ATG::Model::TypeID ) )
            {
                ATG::Model* pModel = ( ATG::Model* )( *i );

                // Loop over mesh mappings.
                DWORD dwMeshMappingCount = pModel->GetNumMeshMappings();
                for ( DWORD dwMapIndex = 0; dwMapIndex < dwMeshMappingCount; ++dwMapIndex )
                {
                    ATG::MeshMapping& mm = pModel->GetMeshMapping( dwMapIndex );
                    ATG::BaseMesh* pMesh = mm.pMesh;

                    // Loop over mesh subsets.
                    DWORD dwSubsetCount = pMesh->GetNumSubsets();
                    for ( DWORD dwSubsetIndex = 0; dwSubsetIndex < dwSubsetCount; ++dwSubsetIndex )
                    {
                        ATG::MaterialInstance* pMaterial = mm.Materials[ dwSubsetIndex ];

                        if ( !wcscmp( pMaterial->GetName().GetSafeString(), L"sand" ) )
                        {
                            if ( !pMaterial->IsTransparent() )
                            {
                                m_pCmdBufferDevice->SetVertexShaderConstantF( 25, ( FLOAT* )&pModel->GetWorldTransform(), 4 );

                                // Retrieve diffuse texture name
                                ATG::MaterialParameter& param = pMaterial->GetRawParameter( 0 );

                                if ( param.pValue != NULL )
                                {
                                    ATG::Texture2D* pTex2D = ( ATG::Texture2D* )param.pValue;
                                    m_pCmdBufferDevice->SetTexture( 0, pTex2D->GetD3DTexture() );
                                }

                                // Retreive normal map
                                if ( pMaterial->GetRawParameterCount() >= 2 )
                                {
                                    ATG::MaterialParameter& prm = pMaterial->GetRawParameter( 1 );

                                    if ( prm.pValue != NULL )
                                    {
                                        ATG::Texture2D* pTex2D = ( ATG::Texture2D* )prm.pValue;
                                        m_pCmdBufferDevice->SetTexture( 2, pTex2D->GetD3DTexture() );
                                    }
                                }

                                // Render the mesh subset.
                                pMesh->RenderSubset( dwSubsetIndex, m_pCmdBufferDevice, ATG::BaseMesh::NoVertexDecl );
                            }
                        }
                    }
                }
            }
        }

        RETURN_ON_FAIL( m_pCmdBufferDevice->EndCommandBuffer() );
    }
   
    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: RestoreBuffers
// Desc: Restores the color, depth and hi-z buffers
//--------------------------------------------------------------------------------------

VOID CRenderer::RestoreBuffers()
{
    // Bias towards pixel shader
    m_pd3dDevice->SetShaderGPRAllocation( 0, 16, GPU_GPRS - 16 );

    // Render to the 8888 alias of the depth buffer.
    m_pd3dDevice->SetRenderTarget( 0, m_pBackBufferSurface );
    m_pd3dDevice->SetRenderTarget( 1, m_pDepthStencilSurfaceAs8888 );
    m_pd3dDevice->SetTexture( 0, &m_BackBufferTextureAs16SRGB );
    m_pd3dDevice->SetTexture( 1, &m_DepthBufferTextureAs8888 );
    m_pd3dDevice->SetPixelShader( m_pFastDepthRestorePS );
    m_pd3dDevice->SetVertexShader( m_pRestoreBuffersVS );
    m_pd3dDevice->SetVertexDeclaration( m_pVertexDeclRestore );

    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_POINT );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_POINT );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MIPFILTER, D3DTEXF_NONE );

    m_pd3dDevice->SetSamplerState( 1, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP );
    m_pd3dDevice->SetSamplerState( 1, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP );
    m_pd3dDevice->SetSamplerState( 1, D3DSAMP_MAGFILTER, D3DTEXF_POINT );
    m_pd3dDevice->SetSamplerState( 1, D3DSAMP_MINFILTER, D3DTEXF_POINT );
    m_pd3dDevice->SetSamplerState( 1, D3DSAMP_MIPFILTER, D3DTEXF_NONE );


    // Turn off Z before restoring the depth to prevent any accidental depth culling.
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, FALSE );

    // 3 2D corners are needed to draw a rect primitive.  The vertex shader will
    // generate proper 4D positions and 2D texture coordinates from this data.
    FLOAT fRectCorners[] = { -1, 1, 1, 1, -1, -1 };

    // Draw a full screen quad to copy the depth data
    m_pd3dDevice->DrawPrimitiveUP( D3DPT_RECTLIST, 1, ( const VOID* )fRectCorners, 2 * sizeof( FLOAT ) );

    // Update the Hi-Z data by touching all tiles but not overwriting them using D3DRS_ZFUNC = D3DCMP_NEVER
    m_pd3dDevice->SetRenderTarget( 0, NULL );
    m_pd3dDevice->SetRenderTarget( 1, NULL );
    m_pd3dDevice->SetDepthStencilSurface( m_pDepthStencilSurfaceWith4xMSAA );
    m_pd3dDevice->SetPixelShader( NULL );

    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_HIZENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_HIZWRITEENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_ZFUNC, D3DCMP_NEVER );

    m_pd3dDevice->DrawPrimitiveUP( D3DPT_RECTLIST, 1, ( const VOID* )fRectCorners, 2 * sizeof( FLOAT ) );

    // Restore the depth states
    m_pd3dDevice->SetDepthStencilSurface( m_pDepthStencilSurface );
    m_pd3dDevice->SetRenderState( D3DRS_ZFUNC, D3DCMP_LESSEQUAL );
    m_pd3dDevice->SetRenderState( D3DRS_HIZENABLE, TRUE );

    // Flush the Hi-Z data
    // In this case, the Hi-Z contains the one-pass Z data which is conservative for
    // this scene.  If Hi-Z did not contain conservative data, use D3DFHZS_SYNCHRONOUS
    m_pd3dDevice->FlushHiZStencil( D3DFHZS_ASYNCHRONOUS );

}


//--------------------------------------------------------------------------------------
// Name: BeginShadowPass
// Desc: Setup states for shadowmap render pass
//--------------------------------------------------------------------------------------

VOID CRenderer::BeginShadowPass( CFrameBufferData* pFrameBufferData, const ERenderPass RenderPass )
{
    m_pStateBlock->Capture();

    switch ( RenderPass )
    {
        case RENDER_PASS_SHADOW:
        {
            // Bias towards vertex shader for shadow pass with NULL pixel shader
            m_pd3dDevice->SetShaderGPRAllocation( 0, GPU_GPRS - 16, 16 );

            // Initialize some render states.
            m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, TRUE );
            m_pd3dDevice->SetRenderState( D3DRS_ZWRITEENABLE, TRUE );
            m_pd3dDevice->SetRenderState( D3DRS_HIZENABLE, TRUE );
            m_pd3dDevice->SetRenderState( D3DRS_HIZWRITEENABLE, TRUE );
            m_pd3dDevice->SetRenderState( D3DRS_ZFUNC, D3DCMP_LESSEQUAL );
            m_pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_NONE );

            // Set depth biases as both D3D states and shader constants
            FLOAT vBiases[4] = { 0.001f, 2.0f, 0.0f, 0.0f };   // z-bias, slopescale z-bias
            m_pd3dDevice->SetRenderState( D3DRS_DEPTHBIAS, ATG::FtoDW( vBiases[0] ) );
            m_pd3dDevice->SetRenderState( D3DRS_SLOPESCALEDEPTHBIAS, ATG::FtoDW( vBiases[1] ) );
            m_pd3dDevice->SetPixelShaderConstantF( 0, vBiases, 1 );
            m_pd3dDevice->SetRenderState( D3DRS_COLORWRITEENABLE, 0 );

            m_pd3dDevice->SetRenderTarget( 0, NULL );
            m_pd3dDevice->SetDepthStencilSurface( m_pShadowMapSurface );
            m_pd3dDevice->ClearF( D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL, NULL, NULL, 1.0f, 0L );

            m_pd3dDevice->SetPixelShader( NULL );
        }
        break;

        case RENDER_PASS_HYBRID_SHADOW:
        {
            // Copy the depth texture into EDRAM as color.  If we are regenerating static, the source
            // texture is the depth texture generated above.  Otherwise, it's the stored copy
            // from a previous frame.
            PIXBeginNamedEvent( 0, "Move Shadow Map into EDRAM" );

            // Bias towards pixel shader
            m_pd3dDevice->SetShaderGPRAllocation( 0, 16, GPU_GPRS - 16 );

            m_pd3dDevice->SetRenderState( D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_ALL );
            m_pd3dDevice->SetVertexDeclaration( m_pSimpleVertexDecl );
            m_pd3dDevice->SetVertexShader( m_pReplicateGreenVS );
            m_pd3dDevice->SetPixelShader( m_pReplicateGreenPS );
            m_pd3dDevice->SetTexture( 0, m_pShadowMapTexture );
            m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_POINT );
            m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_POINT );
            m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MIPFILTER, D3DTEXF_NONE );
            m_pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP );
            m_pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP );
            m_pd3dDevice->SetRenderTarget( 0, m_pHybridShadowMapSurface );
            m_pd3dDevice->SetDepthStencilSurface( NULL );
            m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, FALSE );
            m_pd3dDevice->SetRenderState( D3DRS_ZWRITEENABLE, FALSE );
            m_pd3dDevice->SetRenderState( D3DRS_HIZENABLE, FALSE );
            m_pd3dDevice->SetRenderState( D3DRS_HIZWRITEENABLE, FALSE );

            struct SIMPLEVERTEX
            {
                FLOAT   Position[3];
                FLOAT   TexCoord[2];
            };

            static SIMPLEVERTEX Vertices[] =
            {
                { -1.0f,  1.0f, 0.5f, 0.0f, 0.0f }, // x, y, z, s, t
                {  1.0f,  1.0f, 0.5f, 1.0f, 0.0f },
                {  1.0f, -1.0f, 0.5f, 1.0f, 1.0f },
                { -1.0f, -1.0f, 0.5f, 0.0f, 1.0f },
            };

            m_pd3dDevice->DrawPrimitiveUP( D3DPT_QUADLIST, 1, Vertices, sizeof( SIMPLEVERTEX ) );

            PIXEndNamedEvent();

            // Render objects dependent on skeleton tracking
            PIXBeginNamedEvent( 0, "Hybrid Shadowmap" );

            // Bias towards vertex shader
            m_pd3dDevice->SetShaderGPRAllocation( 0, GPU_GPRS - 48, 48 );

            m_pd3dDevice->SetTexture( 0, m_pShadowMapTexture );
            m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_POINT );
            m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_POINT );
            m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MIPFILTER, D3DTEXF_NONE );
            m_pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP );
            m_pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP );
            m_pd3dDevice->SetRenderTarget( 0, m_pHybridShadowMapSurface );
            m_pd3dDevice->SetDepthStencilSurface( NULL );

            // Initialize some render states.
            m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, TRUE );
            m_pd3dDevice->SetRenderState( D3DRS_ZWRITEENABLE, TRUE );
            m_pd3dDevice->SetRenderState( D3DRS_HIZENABLE, TRUE );
            m_pd3dDevice->SetRenderState( D3DRS_HIZWRITEENABLE, TRUE );
            m_pd3dDevice->SetRenderState( D3DRS_ZFUNC, D3DCMP_LESSEQUAL );
            m_pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_NONE );

            // Set depth biases as both D3D states and shader constants
            FLOAT vBiases[4] = { 0.001f, 2.0f, };   // z-bias, slopescale z-bias
            m_pd3dDevice->SetRenderState( D3DRS_DEPTHBIAS, ATG::FtoDW( vBiases[0] ) );
            m_pd3dDevice->SetRenderState( D3DRS_SLOPESCALEDEPTHBIAS, ATG::FtoDW( vBiases[1] ) );
            m_pd3dDevice->SetPixelShaderConstantF( 0, vBiases, 1 );


            // Write pixel depth to output color, using 'min' blend mode
            // Overwrite only the RED channel (dynamic) not the GREEN channel (static)
            // This is slower than rendering depth-only, but we come out ahead if the 
            // dynamic objects cover relatively few pixels.  That's because we can just
            // render straight into the existing render target, rather than combining
            // static and dynamic maps in a post-process step.
            m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, TRUE );
            m_pd3dDevice->SetRenderState( D3DRS_BLENDOP, D3DBLENDOP_MIN );
            m_pd3dDevice->SetRenderState( D3DRS_SRCBLEND, D3DBLEND_ONE );
            m_pd3dDevice->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_ONE );
            m_pd3dDevice->SetRenderState( D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_RED );

        }
        break;
    }
}


//--------------------------------------------------------------------------------------
// Name: EndShadowPass
// Desc: End the shadowmap pass
//--------------------------------------------------------------------------------------

VOID CRenderer::EndShadowPass( const ERenderPass RenderPass )
{
    switch ( RenderPass )
    {
        case RENDER_PASS_SHADOW:
        {
            // Resolve depth to our texture.
            m_pd3dDevice->Resolve( D3DRESOLVE_DEPTHSTENCIL, NULL, m_pShadowMapTexture, NULL, 0, 0, NULL, 1.0f, 0, NULL );
        }
        break;

        case RENDER_PASS_HYBRID_SHADOW:
        {
            m_pd3dDevice->Resolve( D3DRESOLVE_RENDERTARGET0 | (DWORD) D3DRESOLVE_EXPONENTBIAS( +10 ), NULL, m_pHybridShadowMapTexture, NULL, 0, 0, NULL, 1.0f, 0, NULL );
            m_pd3dDevice->SetRenderState( D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_ALL );
            FLOAT vNoBiases[4] = { 0.0f, 0.0f, 0.0f, 0.0f };   // z-bias, slopescale z-bias
            m_pd3dDevice->SetRenderState( D3DRS_DEPTHBIAS, ATG::FtoDW( vNoBiases[0] ) );
            m_pd3dDevice->SetRenderState( D3DRS_SLOPESCALEDEPTHBIAS, ATG::FtoDW( vNoBiases[1] ) );
            m_pd3dDevice->SetPixelShaderConstantF( 0, vNoBiases, 1 );
            PIXEndNamedEvent();
        }
        break;
    }

    m_pStateBlock->Apply();
}


//--------------------------------------------------------------------------------------
// Name: BeginZPass
// Desc: Begin the manual pre-Z pass
//--------------------------------------------------------------------------------------

VOID CRenderer::BeginZPass( CFrameBufferData* pFrameBufferData )
{
    // Bias towards vertex shader
    m_pd3dDevice->SetShaderGPRAllocation( 0, GPU_GPRS - 16, 16 );

    m_pd3dDevice->SetRenderState( D3DRS_ZFUNC, D3DCMP_LESSEQUAL );
    m_pd3dDevice->SetRenderState( D3DRS_FILLMODE, D3DFILL_SOLID );
    m_pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_CCW );
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_ZWRITEENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_HIZENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_HIZWRITEENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_VIEWPORTENABLE, TRUE );
}


//--------------------------------------------------------------------------------------
// Name: EndZPass
// Desc: End the manual pre-Z pass
//--------------------------------------------------------------------------------------

VOID CRenderer::EndZPass()
{
}


//--------------------------------------------------------------------------------------
// Name: RenderScene
// Desc: Render the scene
//--------------------------------------------------------------------------------------

VOID CRenderer::RenderScene( CFrameBufferData* pFrameBufferData, const BOOL bFloorPass )
{
    char szBuffer[ 128 ];

    XMMATRIX matView = pFrameBufferData->m_matView;

    if ( m_RenderPass == RENDER_PASS_Z )
    {
        m_pd3dDevice->SetPixelShader( NULL );   // will this automatically force the auto z-pass vshader?
        m_pd3dDevice->SetVertexShader( m_pSceneVS );
        m_pd3dDevice->SetVertexDeclaration( NULL );
    }
    else
    {
        m_pd3dDevice->SetPixelShader( m_pScenePS );
        m_pd3dDevice->SetVertexShader( m_pSceneVS );
        m_pd3dDevice->SetVertexDeclaration( NULL );
    }

    m_pd3dDevice->SetVertexShaderConstantF( 12, ( FLOAT* )&matView, 4 );

    ECmdBufferType CmdBufferType = m_bUsePredicatedTiling ? CMD_BUFFER_TYPE_TILED : CMD_BUFFER_TYPE_NON_TILED;

    switch ( m_RenderPass )
    {
    case RENDER_PASS_Z:
    case RENDER_PASS_OPAQUE:
    case RENDER_PASS_OPAQUE_AND_Z:
        sprintf_s( szBuffer, bFloorPass ? "RenderScene Floor %d" : "RenderScene Opaque %d", pFrameBufferData->m_uFrameBufferIdx );
        PIXBeginNamedEvent( 0, szBuffer );
        m_pd3dDevice->RunCommandBuffer( m_pCmdBuffers[ CmdBufferType ][ bFloorPass ? CMD_BUFFER_SEA_FLOOR : CMD_BUFFER_SEA_OPAQUE ], 0 );
        PIXEndNamedEvent();
        break;

    case RENDER_PASS_TRANSPARENT:
        sprintf_s( szBuffer, "RenderScene Transparent %d" , pFrameBufferData->m_uFrameBufferIdx );
        PIXBeginNamedEvent( 0, szBuffer );
        m_pd3dDevice->RunCommandBuffer( m_pCmdBuffers[ CmdBufferType ][ CMD_BUFFER_SEA_TRANSPARENT ], 0 );
        PIXEndNamedEvent();
        break;
    }
}


//--------------------------------------------------------------------------------------
// Name: RenderFish
// Desc: Render the fish
//--------------------------------------------------------------------------------------

VOID CRenderer::RenderFish( CFrameBufferData* pFrameBufferData )
{
    char szBuffer[ 128 ];
    sprintf_s( szBuffer, "RenderFish %d", pFrameBufferData->m_uFrameBufferIdx );
    PIXBeginNamedEvent( 0, szBuffer );

    XMMATRIX matView = pFrameBufferData->m_matView;

    switch ( m_RenderPass )
    {
    case RENDER_PASS_SHADOW:
    case RENDER_PASS_HYBRID_SHADOW:
        for ( int i = 0; i < NUM_FISH; i++ )
        {            
            XMMATRIX* pModelMtx = &pFrameBufferData->m_matModel[ i ];
            XMVECTOR* pBlendWeights = &pFrameBufferData->m_vBlendWeights[ i ];
            m_pFish[ i ]->Render( pModelMtx, pBlendWeights, XMVectorZero(), m_matLightView, m_matLightProj, m_RenderPass );
        }
        break;

    default:
        for ( int i = 0; i < NUM_FISH; i++ )
        {            
            XMMATRIX* pModelMtx = &pFrameBufferData->m_matModel[ i ];
            XMVECTOR* pBlendWeights = &pFrameBufferData->m_vBlendWeights[ i ];
            m_pFish[ i ]->Render( pModelMtx, pBlendWeights, m_vLightDirection, matView, m_matProj, m_RenderPass );
        }
    }

    PIXEndNamedEvent();
}


//--------------------------------------------------------------------------------------
// Name: RenderAvatar
// Desc: Render the avatar
//--------------------------------------------------------------------------------------

VOID CRenderer::RenderAvatar( CFrameBufferData* pFrameBufferData )
{
    char szBuffer[ 128 ];
    sprintf_s( szBuffer, "RenderAvatar %d ", pFrameBufferData->m_uFrameBufferIdx );
    PIXBeginNamedEvent( 0, szBuffer );

    XMMATRIX matView = pFrameBufferData->m_matView;
    XMMATRIX matWorld = pFrameBufferData->m_matWorldAvatar;

    if ( pFrameBufferData->m_iSkeletonIdx >= 0 &&
         pFrameBufferData->m_SkeletonFrame.SkeletonData[ pFrameBufferData->m_iSkeletonIdx ].eTrackingState == NUI_SKELETON_TRACKED )
    {
        switch ( m_RenderPass )
        {
        case RENDER_PASS_SHADOW:
            m_pd3dDevice->SetVertexShader( m_pWriteSkinnedDepthVS );
            m_pd3dDevice->SetPixelShader( NULL );
            m_pAvatarRenderer->Render( matWorld, m_matLightView, m_matLightProj, TRUE );
            break;

        case RENDER_PASS_HYBRID_SHADOW:
            m_pd3dDevice->SetVertexShader( m_pWriteSkinnedDepthVS );
            m_pd3dDevice->SetPixelShader( m_pWriteSkinnedDepthPS );
            m_pAvatarRenderer->Render( matWorld, m_matLightView, m_matLightProj, TRUE );
            break;

        default:
            m_pAvatarRenderer->Render( matWorld, matView, m_matProj );
        }
    }

    PIXEndNamedEvent();
}


//--------------------------------------------------------------------------------------
// Name: SetRenderStatesAndConstants
// Desc: Set renderstates and constants for scene rendering
//--------------------------------------------------------------------------------------

VOID CRenderer::SetRenderStatesAndConstants( CFrameBufferData* pFrameBufferData )
{
    D3DTexture* pCurrentCausticTexture = pFrameBufferData->m_pCurrentCausticTexture;
    XMMATRIX matView = pFrameBufferData->m_matView;

    // Set the vertex shader constants. 
    const XMFLOAT4 vZero( 0.0f, 0.0f, 0.0f, 0.0f );
    const XMFLOAT4 vConstants( 1.0f, 0.5f, 1.0f, 0.05f );

    // Lighting vectors (in world space and in model space)and other constants
    const FLOAT fFogDepth = 50.0f;
    XMVECTOR vLight = m_vLightDirection;
    XMVECTOR vLightFishSpace = m_vLightDirection;
    XMVECTOR vDiffuse = XMVectorSet( 1.00f, 1.00f, 1.00f, 1.00f );
    XMVECTOR vAmbient = XMVectorSet( 0.25f, 0.25f, 0.25f, 0.25f );
    XMVECTOR vFog = XMVectorSet( 0.50f, ( ( FLOAT )fFogDepth * 40.0f ), 1.0f / ( ( ( FLOAT )fFogDepth * 40.0f ) - 40.0f ), 0.00f );

    XMVECTOR vDeterminant;
    XMMATRIX matFishInv = XMMatrixInverse( &vDeterminant, pFrameBufferData->m_matModel[ 0 ] );
    vLightFishSpace = XMVector4Normalize( XMVector4Transform( vLight, matFishInv ) );

    // Vertex shader operations use transposed matrices
    XMMATRIX mat, matCamera, matTranspose, matCameraTranspose;
    XMMATRIX matViewTranspose, matProjTranspose;
    matCamera = XMMatrixMultiply( pFrameBufferData->m_matModel[ 0 ], matView );
    mat = XMMatrixMultiply( matCamera, m_matProj );
    matTranspose = XMMatrixTranspose( mat );
    matCameraTranspose = XMMatrixTranspose( matCamera );
    matViewTranspose = XMMatrixTranspose( matView );
    matProjTranspose = XMMatrixTranspose( m_matProj );

    // Set the vertex shader constants
    m_pd3dDevice->SetVertexShaderConstantF( 0, ( FLOAT* )&vZero, 1 );
    m_pd3dDevice->SetVertexShaderConstantF( 1, ( FLOAT* )&vConstants, 1 );
    m_pd3dDevice->SetVertexShaderConstantF( 12, ( FLOAT* )&matViewTranspose, 4 );
    m_pd3dDevice->SetVertexShaderConstantF( 16, ( FLOAT* )&matProjTranspose, 4 );
    m_pd3dDevice->SetVertexShaderConstantF( 20, ( FLOAT* )&vLight, 1 );
    m_pd3dDevice->SetVertexShaderConstantF( 22, ( FLOAT* )&vDiffuse, 1 );
    m_pd3dDevice->SetVertexShaderConstantF( 23, ( FLOAT* )&vAmbient, 1 );
    m_pd3dDevice->SetVertexShaderConstantF( 34, ( FLOAT* )&vFog, 1 );

    m_pd3dDevice->SetRenderState( D3DRS_ZFUNC, D3DCMP_LESSEQUAL );
    m_pd3dDevice->SetRenderState( D3DRS_FILLMODE, D3DFILL_SOLID );
    m_pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_CCW );
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_ZWRITEENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_VIEWPORTENABLE, TRUE );

    // Set the shadowmap texture
    m_pd3dDevice->SetVertexShaderConstantF( 36, ( float* )&m_matShadowViewProj, 4 );

    // Use a border color that is equivalent to 1.0 depth
    m_pd3dDevice->SetSamplerState( 3, D3DSAMP_BORDERCOLOR, 0xffffffff );

    // Point sample the shadowmap texture.
    m_pd3dDevice->SetSamplerState( 3, D3DSAMP_MINFILTER, D3DTEXF_POINT );
    m_pd3dDevice->SetSamplerState( 3, D3DSAMP_MAGFILTER, D3DTEXF_POINT );
    m_pd3dDevice->SetSamplerState( 3, D3DSAMP_MIPFILTER, D3DTEXF_POINT );
    m_pd3dDevice->SetSamplerState( 3, D3DSAMP_ADDRESSU, D3DTADDRESS_BORDER );
    m_pd3dDevice->SetSamplerState( 3, D3DSAMP_ADDRESSV, D3DTADDRESS_BORDER );

    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MIPFILTER, D3DTEXF_POINT );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP );

    m_pd3dDevice->SetSamplerState( 1, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetSamplerState( 1, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetSamplerState( 1, D3DSAMP_MIPFILTER, D3DTEXF_POINT );
    m_pd3dDevice->SetSamplerState( 1, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP );
    m_pd3dDevice->SetSamplerState( 1, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP );

    m_pd3dDevice->SetSamplerState( 6, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetSamplerState( 6, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetSamplerState( 6, D3DSAMP_MIPFILTER, D3DTEXF_POINT );
    m_pd3dDevice->SetSamplerState( 6, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP );
    m_pd3dDevice->SetSamplerState( 6, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP );

    m_pd3dDevice->SetSamplerState( 2, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetSamplerState( 2, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetSamplerState( 2, D3DSAMP_MIPFILTER, D3DTEXF_POINT );
    m_pd3dDevice->SetSamplerState( 2, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP );
    m_pd3dDevice->SetSamplerState( 2, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP );

    FLOAT fAmbient[4] = { 0, 0, 0, 0 };
    FLOAT fWaterColor[4];
    fWaterColor[0] = ( FLOAT )( ( m_dwFogColor >> 16 ) & 0xff ) / 255.0f;
    fWaterColor[1] = ( FLOAT )( ( m_dwFogColor >> 8 ) & 0xff ) / 255.0f;
    fWaterColor[2] = ( FLOAT )( m_dwFogColor & 0xff ) / 255.0f;
    fWaterColor[3] = 1.0f;
    m_pd3dDevice->SetPixelShaderConstantF( 0, fWaterColor, 1 );
    m_pd3dDevice->SetPixelShaderConstantF( 1, fAmbient, 1 );
    m_pd3dDevice->SetPixelShaderConstantF( 2, ( FLOAT* )&m_vLightDirection, 1 );

    // Enable normal mapping
    FLOAT fEnableNormalMap = 1.0f;
    m_pd3dDevice->SetPixelShaderConstantF( 3, &fEnableNormalMap, 1 );

    m_pd3dDevice->SetTexture( 1, pCurrentCausticTexture );
    m_pd3dDevice->SetTexture( 3, m_bUseHybridShadowMap ? m_pHybridShadowMapTexture : m_pShadowMapTexture );
    m_pd3dDevice->SetTexture( 6, pCurrentCausticTexture );
}


//--------------------------------------------------------------------------------------
// Name: BindSceneVertexDeclaration
// Desc: Find and bind the scene vertex declaration
//--------------------------------------------------------------------------------------

VOID CRenderer::BindSceneVertexDeclaration()
{
    ATG::NameIndexedCollection::iterator i;
    for ( i = m_pScene->GetInstanceList()->begin(); i != m_pScene->GetInstanceList()->end(); i++ )
    {
        if ( ( *i )->IsDerivedFrom( ATG::Model::TypeID ) )
        {
            ATG::Model* pModel = ( ATG::Model* )( *i );

            DWORD dwMeshMappingCount = pModel->GetNumMeshMappings();
            for( DWORD dwMapIndex = 0; dwMapIndex < dwMeshMappingCount; ++dwMapIndex )
            {
                ATG::MeshMapping& mm = pModel->GetMeshMapping( dwMapIndex );
                ATG::BaseMesh* pMesh = mm.pMesh;

                // Loop over mesh subsets.
                DWORD dwSubsetCount = pMesh->GetNumSubsets();
                for ( DWORD dwSubsetIndex = 0; dwSubsetIndex < dwSubsetCount; ++dwSubsetIndex )
                {
                    ATG::MaterialInstance* pMaterial = mm.Materials[ dwSubsetIndex ];

                    if ( !wcscmp( pMaterial->GetName().GetSafeString(), L"sand" ) )
                    {
                        m_pSceneVertexDecl = pMesh->GetVertexData( 0 )->GetVertexDecl();
                        break;
                    }
                }
            }
        }
    }

    const DWORD stride = 28;    // pos, normal, tangent, binormal, texcoord
    m_pSceneVS->Bind( 0, m_pSceneVertexDecl, &stride, NULL );
    m_pWriteDepthVS->Bind( 0, m_pSceneVertexDecl, &stride, NULL );

}


//--------------------------------------------------------------------------------------
// Name: SetAvatarRenderedFence()
// Desc: Set the GPU fence of when we're done submitting rendering calls for the avatar
//--------------------------------------------------------------------------------------
VOID CRenderer::SetAvatarRenderedFence( const DWORD dwFence )
{
    m_pAvatarRenderer->SetAvatarRenderedFence( dwFence );
}


//--------------------------------------------------------------------------------------
// Name: CFish class statics
//--------------------------------------------------------------------------------------

LPDIRECT3DVERTEXDECLARATION9 CFish::m_pVertexDeclaration = NULL;
LPDIRECT3DVERTEXSHADER9 CFish::m_pVertexShader = NULL;
LPDIRECT3DVERTEXSHADER9 CFish::m_pVertexShaderDepth = NULL;
D3DDevice* CFish::m_pd3dDevice = NULL;
DWORD CFish::m_dwTextureIdx = 0;

static CHAR* g_strFishTextures[] =
{
    "Shark",
    "Turtle",
    "Ud",
};


//--------------------------------------------------------------------------------------
// Name: Initialize
// Desc: Initialize fish
//--------------------------------------------------------------------------------------

HRESULT CFish::Initialize( FISH_TYPE type, FLOAT fRadius, ATG::PackedResource* pResource, D3DDevice* pd3dDevice )
{
    m_vPosition = XMVectorZero();
    m_Type = type;
    FLOAT fMaxRadius = fRadius;
    FLOAT fMinRadius = fRadius * 0.5f;
    FLOAT fRandRadius = (FLOAT)rand() / ( RAND_MAX + 1.0f ) * ( fMaxRadius - fMinRadius) + fMinRadius;
    m_fRadius = fRandRadius;

    FLOAT fMax = 100.0f;
    FLOAT fMin = 0.0f;
    m_fPhaseOffset = (FLOAT)rand() / ( RAND_MAX + 1.0f ) * ( fMax - fMin ) + fMin;

    switch( m_Type )
    {
        case FISH_TYPE_SHARK:
            fMax = 200.0f;
            fMin = -400.0f;
            break;

        case FISH_TYPE_TURTLE:
            fMax = 175.0f;
            fMin = -200.0f;
            break;

        case FISH_TYPE_UD:
            fMax = 0.0f;
            fMin = -250.0f;
            break;
    }
    m_fYOffset = (FLOAT)rand() / ( RAND_MAX + 1.0f ) * ( fMax - fMin ) + fMin;

    fMax = 0.5f;
    fMin = -0.5f;
    m_fKickOffset = (FLOAT)rand() / ( RAND_MAX + 1.0f ) * ( fMax - fMin ) + fMin;

    char szFileName[ 256 ];
    assert( type < ARRAY_SIZE( g_strFishTextures ) );
    sprintf_s( szFileName, "%s%d", g_strFishTextures[ type ], ( m_dwTextureIdx++ % 2 ) + 1 );
    m_pTexture = pResource->GetTexture( szFileName );

    for ( int i = 0; i < FISH_FRAME_COUNT; i++ )
    {
        sprintf_s( szFileName, "game:\\Media\\Meshes\\%s%d.xbg", g_strFishTextures[type], i + 1 );
        RETURN_ON_FAIL( m_Mesh[i].Create( szFileName ) );
    }

    if ( !m_pd3dDevice )
    {
        m_pd3dDevice = pd3dDevice;
    }        

    if ( !m_pVertexDeclaration )
    {
        // Build the vertex declaration for the fish
        D3DVERTEXELEMENT9 vtxDecl[MAXD3DDECLLENGTH] = { 0 };
        for ( int i = 0; i < FISH_FRAME_COUNT; i++ )
        {
            ATG::AppendVertexElements( vtxDecl, i, m_Mesh[i].GetMesh()->m_VertexElements, i );
        }

        RETURN_ON_FAIL( m_pd3dDevice->CreateVertexDeclaration( vtxDecl, &m_pVertexDeclaration ) );

    }

    if ( !m_pVertexShader )
    {
        RETURN_ON_FAIL( ATG::LoadVertexShader( "game:\\Media\\Shaders\\ShadeFishVertex.xvu", &m_pVertexShader ) );
    }

    if ( !m_pVertexShaderDepth )
    {
        RETURN_ON_FAIL( ATG::LoadVertexShader( "game:\\Media\\Shaders\\WriteDepthFishVS.xvu", &m_pVertexShaderDepth ) );
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Update
// Desc: Update fish
//--------------------------------------------------------------------------------------

VOID CFish::Update( FLOAT fTime, XMMATRIX* pModelMtx, XMVECTOR* pBlendWeights )
{
    // Move the fish in a circle
    XMMATRIX matFish, matTrans, matRotate1, matRotate2;
    XMVECTOR vBlendWeights = XMVectorZero();

    switch( m_Type )
    {
        case FISH_TYPE_SHARK:
        {
            // Animation attributes for the fish
            FLOAT fKickFreq = ( 4.0f + m_fKickOffset ) * fTime;
            FLOAT fPhase = m_fPhaseOffset + fTime / 6.0f;
            FLOAT fBlendWeight1 = sinf( fKickFreq );
            FLOAT fBlendWeight2 = sinf( fKickFreq + D3DX_PI );

            matFish = XMMatrixScaling( 0.5f * m_fRadius / FISH_RADIUS, 0.5f * m_fRadius / FISH_RADIUS, 0.5f * m_fRadius / FISH_RADIUS );
            matRotate1 = XMMatrixRotationZ( -cosf( fKickFreq ) / 15.0f );
            matFish = XMMatrixMultiply( matFish, matRotate1 );
            matRotate2 = XMMatrixRotationY( fPhase - ( D3DX_PI / 2.0f ) );
            matFish = XMMatrixMultiply( matFish, matRotate2 );
            m_vPosition = XMVectorSet( 1300.0f - 0.8f * 1000.0f * sinf( fPhase ), 250.0f + m_fYOffset,
                                           300.0f - 1.0f * 1000.0f * cosf( fPhase ), 0 );
            matTrans = XMMatrixTranslation( m_vPosition.x, m_vPosition.y, m_vPosition.z );
            matFish = XMMatrixMultiply( matFish, matTrans );
            vBlendWeights.x = ( fBlendWeight1 + 1.0f ) / 2.0f;
            vBlendWeights.y = 0.0f;
            vBlendWeights.z = ( fBlendWeight2 + 1.0f ) / 2.0f;
        }
        break;

        case FISH_TYPE_TURTLE:
        {
            // Animation attributes for the fish
            FLOAT fKickFreq = ( 2.5f + m_fKickOffset ) * fTime;
            FLOAT fPhase = -(m_fPhaseOffset + fTime / 8.0f);
            FLOAT fBlendWeight1 = sinf( fKickFreq );

            matFish = XMMatrixScaling( 1.5f * m_fRadius / FISH_RADIUS, 1.5f * m_fRadius / FISH_RADIUS, 1.5f * m_fRadius / FISH_RADIUS );
            matRotate1 = XMMatrixRotationZ( D3DX_PI );
            matFish = XMMatrixMultiply( matFish, matRotate1 );
            matRotate2 = XMMatrixRotationY( fPhase );
            matFish = XMMatrixMultiply( matFish, matRotate2 );
            m_vPosition = XMVectorSet( -600.0f - 1000.0f * sinf( fPhase ), m_fYOffset + 200.0f + 20.0f * cosf( fKickFreq ), -300.0f - 1000.0f * cosf( fPhase ), 0 );
            matTrans = XMMatrixTranslation( m_vPosition.x, m_vPosition.y, m_vPosition.z );
            matFish = XMMatrixMultiply( matFish, matTrans );
            if( fBlendWeight1 > 0.0f )
            {
                vBlendWeights.x = fabsf( fBlendWeight1 );
                vBlendWeights.y = 1.0f - fabsf( fBlendWeight1 );
                vBlendWeights.z = 0.0f;
            }
            else
            {
                vBlendWeights.x = 0.0f;
                vBlendWeights.y = 1.0f - fabsf( fBlendWeight1 );
                vBlendWeights.z = fabsf( fBlendWeight1 );
            }
        }
        break;

        case FISH_TYPE_UD:
        {
            // Animation attributes for the fish
            FLOAT fKickFreq = ( 5.0f + m_fKickOffset ) * fTime;
            FLOAT fPhase = -(m_fPhaseOffset + fTime / 12.0f);
            FLOAT fBlendWeight1 = sinf( fKickFreq );
            FLOAT fBlendWeight2 = sinf( fKickFreq + D3DX_PI );

            matFish = XMMatrixScaling( 1.5f * m_fRadius / FISH_RADIUS, 1.5f * m_fRadius / FISH_RADIUS, 1.5f * m_fRadius / FISH_RADIUS );
            matRotate1 = XMMatrixRotationZ( -cosf( fKickFreq ) / 6.0f );
            matFish = XMMatrixMultiply( matFish, matRotate1 );
            matRotate2 = XMMatrixRotationY( fPhase + ( D3DX_PI / 2.0f ) );
            matFish = XMMatrixMultiply( matFish, matRotate2 );
            m_vPosition = XMVectorSet( 1500.0f - 1000.0f * sinf( fPhase ), m_fYOffset + 50.0f, -2000.0f - 1000.0f * cosf( fPhase ), 0 );
            matTrans = XMMatrixTranslation( m_vPosition.x, m_vPosition.y, m_vPosition.z );
            matFish = XMMatrixMultiply( matFish, matTrans );
            vBlendWeights.x = ( fBlendWeight1 + 1.0f ) / 2.0f;
            vBlendWeights.y = 0.0f;
            vBlendWeights.z = ( fBlendWeight2 + 1.0f ) / 2.0f;
        }
        break;
    }

    *pModelMtx = matFish;
    *pBlendWeights = vBlendWeights;
    
}


//--------------------------------------------------------------------------------------
// Name: Render
// Desc: Render the fish
//--------------------------------------------------------------------------------------

VOID CFish::Render( XMMATRIX* pModelMtx, XMVECTOR* pBlendWeights, XMVECTOR vLightDir, XMMATRIX matView, XMMATRIX matProj, const ERenderPass RenderPass )
{
    m_pd3dDevice->SetVertexShaderConstantF( 2, ( FLOAT* )pBlendWeights, 1 );

    XMMATRIX matModel = *pModelMtx;

    switch ( RenderPass )
    {
        case RENDER_PASS_SHADOW:
        case RENDER_PASS_HYBRID_SHADOW:
        {
            XMMATRIX matVP = matView * matProj;
            XMMATRIX matMVP = matModel * matVP;
            XMMATRIX matTransposeMVP = XMMatrixTranspose( matMVP );
            m_pd3dDevice->SetVertexShaderConstantF( 4, ( FLOAT* )&matTransposeMVP, 4 );

            m_pd3dDevice->SetPixelShader( NULL );
            m_pd3dDevice->SetVertexDeclaration( m_pVertexDeclaration );
            m_pd3dDevice->SetVertexShader( m_pVertexShaderDepth );
        }

        default:
        {
            XMVECTOR vDeterminant;
            XMMATRIX matFishInv = XMMatrixInverse( &vDeterminant, matModel );
            XMVECTOR vLightFishSpace = XMVector4Normalize( XMVector4Transform( vLightDir, matFishInv ) );

            // Vertex shader operations use transposed matrices
            XMMATRIX mat, matCamera, matTranspose, matCameraTranspose;
            XMMATRIX matViewTranspose, matProjTranspose;
            matCamera = XMMatrixMultiply( matModel, matView );
            mat = XMMatrixMultiply( matCamera, matProj );
            matTranspose = XMMatrixTranspose( mat );
            matCameraTranspose = XMMatrixTranspose( matCamera );
            matViewTranspose = XMMatrixTranspose( matView );
            matProjTranspose = XMMatrixTranspose( matProj );
            XMMATRIX matModelTranspose = XMMatrixTranspose( matModel );

            // Render the fish (disable normal mapping)
            FLOAT fEnableNormalMap = 0.0f;
            m_pd3dDevice->SetPixelShaderConstantF( 3, &fEnableNormalMap, 1 );
            m_pd3dDevice->SetTexture( 0, m_pTexture );
            m_pd3dDevice->SetVertexDeclaration( m_pVertexDeclaration );
            m_pd3dDevice->SetVertexShader( m_pVertexShader );
        
            m_pd3dDevice->SetVertexShaderConstantF( 4, ( FLOAT* )&matTranspose, 4 );
            m_pd3dDevice->SetVertexShaderConstantF( 8, ( FLOAT* )&matCameraTranspose, 4 );
            m_pd3dDevice->SetVertexShaderConstantF( 21, ( FLOAT* )&vLightFishSpace, 1 );
            m_pd3dDevice->SetVertexShaderConstantF( 25, ( FLOAT* )&matModel, 4 );
        }
    }

    for ( DWORD dwFrame = 0; dwFrame < FISH_FRAME_COUNT; ++dwFrame )
    {
        m_pd3dDevice->SetStreamSource( dwFrame, &m_Mesh[dwFrame].GetMesh()->m_VB, 0,
                                       m_Mesh[dwFrame].GetMesh()->m_dwVertexSize );
    }

    m_pd3dDevice->SetIndices( &m_Mesh[0].GetMesh()->m_IB );
    m_pd3dDevice->DrawIndexedPrimitive( m_Mesh[0].GetMesh()->m_dwPrimType, 0,
                                        0, m_Mesh[0].GetMesh()->m_pSubsets[0].dwVertexCount,
                                        0, m_Mesh[0].GetMesh()->m_pSubsets[0].dwPrimitiveCount );
}


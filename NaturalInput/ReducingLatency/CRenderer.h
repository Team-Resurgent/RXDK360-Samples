//--------------------------------------------------------------------------------------
// CRenderer.h
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

#include "Common.h"
#include <AtgFont.h>
#include <AtgSceneAll.h>
#include <AtgMesh.h>
#include <AtgPostProcess.h>
#include "AvatarRenderer.h"
#include <AtgNuiJointConverter.h>

enum ERender
{
    RENDER_SCENE,                   // Render the scene
    RENDER_SCENE_FLOOR,             // Render the scene floor
    RENDER_FISH,                    // Render the fish
    RENDER_AVATAR                   // Render the avatar
};

enum ECmdBufferType
{
    CMD_BUFFER_TYPE_NON_TILED,      // Non tiled render target command buffers
    CMD_BUFFER_TYPE_TILED,          // Tiled render target command buffers
    NUM_CMD_BUFFER_TYPES
};

enum ECmdBuffers
{
    CMD_BUFFER_SEA_OPAQUE,          // Opaque geometry
    CMD_BUFFER_SEA_TRANSPARENT,     // Transparent geometry
    CMD_BUFFER_SEA_FLOOR,           // Floor
    NUM_CMD_BUFFERS
};

// Post processing effects
#define RENDER_POST_SCREEN_SPACE_AA     1       // Screen space anti-aliasing
#define RENDER_POST_WATER_DISTORTION    2       // Water distortion

class CFish;

class CRenderer
{
public:
    CRenderer();

    HRESULT CreateDevice( D3DPRESENT_PARAMETERS* pParams );

    HRESULT Initialize( D3DPRESENT_PARAMETERS* pParams, ATG::Font* pFont );
    VOID Reset();

    HRESULT CreateTextures( D3DPRESENT_PARAMETERS* pParams );
    HRESULT CreateRenderTargets( D3DPRESENT_PARAMETERS* pParams );

    VOID Render( const ERender Render, CFrameBufferData* pFrameBufferData );
    VOID RenderPostEffects( const dwFlags, CFrameBufferData* pFrameBufferData );

    VOID Update( CFrameBufferData* pFrameBufferData, const FLOAT fTime );
    VOID UpdateAvatar( CFrameBufferData* pFrameBufferData, const FLOAT fTime );

    VOID BeginShadowPass( CFrameBufferData* pFrameBufferData, const ERenderPass RenderPass );
    VOID EndShadowPass( const ERenderPass RenderPass );

    VOID BeginZPass( CFrameBufferData* pFrameBufferData );
    VOID EndZPass();

    VOID RestoreBuffers();
    
    VOID Present();

    VOID SetAvatarRenderedFence( const DWORD dwFence );

protected:    
    VOID SetRenderStatesAndConstants( CFrameBufferData* pFrameBufferData );

    VOID RenderScene( CFrameBufferData* pFrameBufferData, const BOOL bFloorPass = FALSE );
    VOID RenderFish( CFrameBufferData* pFrameBufferData );
    VOID RenderAvatar( CFrameBufferData* pFrameBufferData );

    VOID BindSceneVertexDeclaration();
    HRESULT RecordCommandBuffers( const ECmdBufferType CmdBufferType );

    VOID ShowAppLoadingMessage( ATG::Font* pFont );

    D3DDevice*              m_pd3dDevice;
    D3DDevice*              m_pCmdBufferDevice;

    ATG::Scene*             m_pScene;
    CFish*                  m_pFish[ NUM_FISH ];

    AvatarRenderer*         m_pAvatarRenderer;
	ATG::NuiJointConverter* m_pNuiJointConverter;

    D3DCommandBuffer*       m_pCmdBuffers[ NUM_CMD_BUFFER_TYPES ][ NUM_CMD_BUFFERS ];

    D3DRECT                 m_pTilingRects[ 2 ];

    // Shaders and decl
    D3DVertexDeclaration*   m_pSceneVertexDecl;
    D3DVertexShader*        m_pSceneVS;
    D3DPixelShader*         m_pScenePS;

    IDirect3DVertexShader9* m_pReplicateGreenVS;
    IDirect3DPixelShader9*  m_pReplicateGreenPS;

    IDirect3DVertexShader9* m_pWriteDepthVS;
    IDirect3DPixelShader9*  m_pWriteDepthPS;

    IDirect3DVertexShader9* m_pWriteSkinnedDepthVS;
    IDirect3DPixelShader9*  m_pWriteSkinnedDepthPS;

    IDirect3DVertexDeclaration9* m_pSimpleVertexDecl;

    D3DPixelShader*         m_pScreenSpaceAAPS;
    D3DPixelShader*         m_pWaterDistortionPS;

    IDirect3DVertexShader9* m_pRestoreBuffersVS;
    D3DVertexDeclaration*   m_pVertexDeclRestore;
    D3DPixelShader*         m_pFastDepthRestorePS;

    // Textures
    D3DTexture*             m_pFrontBufferTexture;
    D3DTexture*             m_pBackBufferTexture;
    D3DTexture              m_BackBufferTextureAs16SRGB;
    D3DTexture*             m_pDepthBufferTexture;
    D3DTexture              m_DepthBufferTextureAs8888;
    D3DTexture*             m_pShadowMapTexture;
    D3DTexture*             m_pHybridShadowMapTexture;
    D3DTexture*             m_pCausticTextures[ NUM_CAUSTIC_TEXTURES ];

    // Render targets
    D3DSurface*             m_pBackBufferSurface;
    D3DSurface*             m_pDepthStencilSurface;
    D3DSurface*             m_pTiledBackBufferSurface;
    D3DSurface*             m_pTiledDepthStencilSurface;
    D3DSurface*             m_pDepthStencilSurfaceAs8888;
    D3DSurface*             m_pDepthStencilSurfaceWith4xMSAA;
    D3DSurface*             m_pShadowMapSurface;
    D3DSurface*             m_pHybridShadowMapSurface;

    IDirect3DStateBlock9*   m_pStateBlock;

    // Matrices
    XMMATRIX                m_matProj;
    XMMATRIX                m_matLightView;
    XMMATRIX                m_matLightProj;
    XMMATRIX                m_matShadowViewProj;   

    ERenderPass             m_RenderPass;
    BOOL                    m_bUseHybridShadowMap;
    BOOL                    m_bUsePredicatedTiling;

    ATG::PackedResource     m_Resource;
    ATG::PostProcess        m_PostProcess;

    static const XMVECTOR   m_vLightDirection;
    static const XMVECTOR   m_vEyePt;
    static const XMVECTOR   m_vLookatDir;
    static const XMVECTOR   m_vUp;
    static const DWORD      m_dwFogColor;
    static const D3DVECTOR4 m_vFogColor;

    friend class CLatencySample;
    friend class CLowLatencySample;
    friend class CHighLatencySample;
};

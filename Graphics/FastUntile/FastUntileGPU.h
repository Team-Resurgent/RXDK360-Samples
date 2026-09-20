//---------------------------------------------------------------------------------------------------------
// FastUntileGPU.h
//
// XNA Developer Connection
// Copyright ( C ) Microsoft Corporation. All rights reserved.
//---------------------------------------------------------------------------------------------------------


#pragma once


class GPUUntiler
{
public:
    // Initialization
    VOID InitializeRemapping( IDirect3DDevice9* pd3dDevice, UINT iTexelPitch );
    VOID Initialize( IDirect3DDevice9* pd3dDevice );

    // Untiling
    VOID UntileMemexport( IDirect3DDevice9* pd3dDevice, 
        IDirect3DTexture9* pSourceTexture, 
        IDirect3DTexture9* pDestTexture,
        UINT iUntileMethod );
    VOID UntileResolve( IDirect3DDevice9* pd3dDevice, 
        IDirect3DTexture9* pSourceTexture, 
        IDirect3DTexture9* pDestTexture, 
        UINT iSourceSize, 
        UINT iDestSize );

private:
    UINT m_iRepeatBlockWidth;
    UINT m_iRepeatBlockHeight;

    // Resources for test geometry
    IDirect3DVertexDeclaration9*    m_pVertexDecl;

    // Resources for untiling 
    IDirect3DLineTexture9* m_pLinearToTiled2DAddressTexture;

    // Shaders for untiling
    IDirect3DVertexShader9*         m_pCopyTextureVS;
    IDirect3DPixelShader9*          m_pCopyTexturePS;
    IDirect3DPixelShader9*          m_pUntileMemexportTexelPS;
    IDirect3DPixelShader9*          m_pUntileMemexportTransaction128bppPS;
    IDirect3DPixelShader9*          m_pUntileMemexportTransaction64bppPS;
    IDirect3DPixelShader9*          m_pUntileMemexportTransaction32bppPS;
    IDirect3DPixelShader9*          m_pUntileMemexportTransaction16bppPS;
    IDirect3DPixelShader9*          m_pUntileMemexportTransaction8bppPS;
    IDirect3DPixelShader9*          m_pUntileMemexportPacked32bppPS;
    IDirect3DPixelShader9*          m_pUntileMemexportPacked16bppPS;
    IDirect3DPixelShader9*          m_pUntileMemexportPacked8bppPS;
    IDirect3DPixelShader9*          m_pUntileResolve128bppPS;
    IDirect3DPixelShader9*          m_pUntileResolve64bppPS;
    IDirect3DPixelShader9*          m_pUntileResolve32bppPS;
    IDirect3DPixelShader9*          m_pUntileResolve16bppPS;
    IDirect3DPixelShader9*          m_pUntileResolve8bppPS;

    // Untile helper functions
    inline VOID SetDummyNonAs16NonsRGBTexture( IDirect3DTexture9* pSourceTexture );
    inline VOID SetDummyPackedSourceTexture( IDirect3DTexture9* pSourceTexture );
    inline VOID SetDummyPackedDestTexture( IDirect3DTexture9* pSourceTexture );
    inline VOID SetDummyTextureForUntileResolve( IDirect3DTexture9* pSourceTexture, 
        BOOL bIsResolveTarget, 
        UINT iSourceSize );
    inline VOID SetMemExportStreamConstantFromTexture( IDirect3DTexture9* pDestTexture, 
        GPU_MEMEXPORT_STREAM_CONSTANT* pMemExportStreamConstant );
    inline VOID CreateDummyMatchingRenderTarget( IDirect3DDevice9* pd3dDevice, 
        IDirect3DTexture9* pSourceTexture, 
        IDirect3DSurface9** ppDummyRenderTarget );
    inline VOID CreateDummyMemexportTransactionRenderTarget( IDirect3DDevice9* pd3dDevice, 
        IDirect3DTexture9* pSourceTexture, 
        IDirect3DSurface9** ppDummyRenderTarget );
    inline VOID CreateDummyResolveRenderTarget( IDirect3DDevice9* pd3dDevice, 
        IDirect3DTexture9* pSourceTexture, 
        IDirect3DSurface9** ppDummyRenderTarget );
};



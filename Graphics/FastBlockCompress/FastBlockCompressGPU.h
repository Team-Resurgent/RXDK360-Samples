//---------------------------------------------------------------------------------------------------------
// FastBlockCompressGPU.h
//
// XNA Developer Connection
// Copyright ( C ) Microsoft Corporation. All rights reserved.
//---------------------------------------------------------------------------------------------------------


struct TextureDescAndBaseAddress;


class GPUCompressor
{
private:
    // Resources for test geometry
    IDirect3DVertexDeclaration9*    m_pVertexDecl;
    IDirect3DVertexBuffer9*         m_pVB;
    IDirect3DIndexBuffer9*          m_pIB;
    UINT                            m_iIndexCount;

    // Compression, tiling, utility shaders
    IDirect3DVertexShader9*         m_pVertexShader;
    IDirect3DPixelShader9*          m_pCopyTextureShader;
    IDirect3DPixelShader9*          m_pMergeBlocksShader;
    IDirect3DPixelShader9*          m_pSplitTextureShader;
    IDirect3DPixelShader9*          m_pTileMemexport64Shader;
    IDirect3DPixelShader9*          m_pTileMemexport128Shader;
    IDirect3DPixelShader9*          m_pEncodeDXT1ResolveShader;
    IDirect3DPixelShader9*          m_pEncodeDXT5ResolveShader;
    IDirect3DPixelShader9*          m_pEncodeCTX1ResolveShader;
    IDirect3DPixelShader9*          m_pEncodeDXNResolveShader;
    IDirect3DPixelShader9*          m_pEncodeDXT1MemexportShader;
    IDirect3DPixelShader9*          m_pEncodeDXT5MemexportShader;
    IDirect3DPixelShader9*          m_pEncodeCTX1MemexportShader;
    IDirect3DPixelShader9*          m_pEncodeDXNMemexportShader;

    // Lookup textures containing tiling patterns
    IDirect3DTexture9*              m_pLinearToTiled2DAddress64Bit;
    IDirect3DTexture9*              m_pLinearToTiled2DAddress128Bit;

    // Helper routines
    VOID GenerateGeometryQuad( IDirect3DDevice9* pd3dDevice, 
        D3DVertexBuffer** pVB,
        D3DIndexBuffer** pIB, 
        UINT* numIndices );
    BOOL Is128BitType( UINT iCompressedType );
    BOOL IsBlockCompressedFormat( D3DFORMAT d3dFmt );
    VOID CreateDummyWriteRenderTargets( IDirect3DDevice9* pd3dDevice,  
        const TextureDescAndBaseAddress* pSrcDescAndBaseAddress, 
        IDirect3DSurface9** ppDummyWriteRenderTarget0, 
        IDirect3DSurface9** ppDummyWriteRenderTarget1 );
    VOID CreateDummyResolveRenderTargets( IDirect3DDevice9* pd3dDevice,  
        const TextureDescAndBaseAddress* pSrcDescAndBaseAddress, 
        IDirect3DSurface9** ppDummyResolveRenderTarget0, 
        IDirect3DSurface9** ppDummyResolveRenderTarget1 );
    VOID CreateDummySrcTexture( const TextureDescAndBaseAddress* pSrcDescAndBaseAddress, 
        UINT iCompressedType, 
        UINT iTilingMethod, 
        BOOL bTileOnly, 
        BOOL b128Bit, 
        IDirect3DTexture9* pDummySrcTexture );
    VOID CreateDummyDstTextures( const TextureDescAndBaseAddress* pDstDescAndBaseAddress, 
        UINT iCompressedType, 
        IDirect3DTexture9* pDummyDstTexture0, 
        IDirect3DTexture9* pDummyDstTexture1, 
        DWORD* pdwResolveExpBias );
    IDirect3DPixelShader9* SelectCompressOrTilePixelShader( UINT iTilingMethod, 
        UINT iCompressedType, 
        BOOL bTileOnly );
    VOID CreateDummyMemexportTexture( const TextureDescAndBaseAddress* pDstDescAndBaseAddress, 
        IDirect3DTexture9* pDummyMemExportTexture );
    VOID BeginMemexportForTiling( IDirect3DDevice9* pd3dDevice,  
        const TextureDescAndBaseAddress* pDstDescAndBaseAddress, 
        IDirect3DTexture9* pDummyMemExportTexture, 
        BOOL b128Bit );
    VOID EndMemexportForTiling( IDirect3DDevice9* pd3dDevice,  
        IDirect3DTexture9* pDummyMemExportTexture );
    VOID MergeBlocksGPU( IDirect3DDevice9* pd3dDevice,  
        const TextureDescAndBaseAddress* pSrcDescAndBaseAddress0,
        const TextureDescAndBaseAddress* pSrcDescAndBaseAddress1, 
        const TextureDescAndBaseAddress* pDstDescAndBaseAddress );

public:
    VOID Initialize( IDirect3DDevice9* pd3dDevice );
    VOID CompressAndTileTexture( IDirect3DDevice9* pd3dDevice, 
        const TextureDescAndBaseAddress* pSrcDescAndBaseAddress, 
        const TextureDescAndBaseAddress* pDstDescAndBaseAddress, 
        UINT iCompressedType, 
        UINT iTilingMethod, 
        UINT iGPURepeatCount );
};


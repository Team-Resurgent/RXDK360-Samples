//---------------------------------------------------------------------------------------------------------
// FastBlockCompressGPU.cpp
//
// This file is designed to be a nearly standalone module, which could be cut-and-pasted into
// title code without significant dependencies.
//
// XNA Developer Connection
// Copyright ( C ) Microsoft Corporation. All rights reserved.
//---------------------------------------------------------------------------------------------------------
#include <xtl.h>    // must come before the others
#include <assert.h>
#include <xgraphics.h>

#include <AtgUtil.h>

#include "FastBlockCompress.h"
#include "FastBlockCompressGPU.h"


//---------------------------------------------------------------------------------------------------------
// Custom D3D Formats used by the compressor
//---------------------------------------------------------------------------------------------------------
// Custom formats which can be used in bitwise-accurate copies.
static const D3DFORMAT D3DFMT_A16B16G16R16_SIGNED_INTEGER = ( D3DFORMAT ) MAKED3DFMT(
    GPUTEXTUREFORMAT_16_16_16_16, 
    GPUENDIAN_8IN16, 
    TRUE, 
    GPUSIGN_ALL_SIGNED,
    GPUNUMFORMAT_INTEGER,
    GPUSWIZZLE_ABGR );
static const D3DFORMAT D3DFMT_LIN_A16B16G16R16_SIGNED_INTEGER = ( D3DFORMAT ) MAKED3DFMT(
    GPUTEXTUREFORMAT_16_16_16_16, 
    GPUENDIAN_8IN16, 
    FALSE, 
    GPUSIGN_ALL_SIGNED,
    GPUNUMFORMAT_INTEGER,
    GPUSWIZZLE_ABGR );
static const D3DFORMAT D3DFMT_LIN_A16B16G16R16_UNSIGNED_INTEGER = ( D3DFORMAT ) MAKED3DFMT(
    GPUTEXTUREFORMAT_16_16_16_16, 
    GPUENDIAN_8IN16, 
    FALSE, 
    GPUSIGN_ALL_UNSIGNED,
    GPUNUMFORMAT_INTEGER,
    GPUSWIZZLE_ABGR );

// Format which aliases D3DFMT_LIN_G8R8, but fetches two texels at once
static const D3DFORMAT D3DFMT_LIN_G8R8G8R8 = ( D3DFORMAT ) MAKED3DFMT(
    GPUTEXTUREFORMAT_8_8_8_8, 
    GPUENDIAN_8IN16, 
    FALSE, 
    GPUSIGN_ALL_UNSIGNED, 
    GPUNUMFORMAT_FRACTION, 
    GPUSWIZZLE_ABGR);

// Custom integer format for recording tiling offsets
static const D3DFORMAT D3DFMT_L16_INTEGER = ( D3DFORMAT ) MAKED3DFMT( 
    GPUTEXTUREFORMAT_16, 
    GPUENDIAN_8IN16, 
    TRUE, 
    GPUSIGN_ALL_UNSIGNED, 
    GPUNUMFORMAT_INTEGER, 
    GPUSWIZZLE_ORRR );


//---------------------------------------------------------------------------------------------------------
// Name: GPUCompressor::GenerateGeometryQuad( )
// Desc: Creates vertex and index buffer for a single fullscreen quad 
//---------------------------------------------------------------------------------------------------------
VOID GPUCompressor::GenerateGeometryQuad( IDirect3DDevice9* pd3dDevice,  
                                         D3DVertexBuffer** pVB,
                                         D3DIndexBuffer** pIB, 
                                         UINT* numIndices )
{
    // Create a vertex buffer and copy the mesh vertex data into it
    pd3dDevice->CreateVertexBuffer( sizeof( TestGeometryVertex ) * 4, 0, 0, D3DPOOL_DEFAULT, 
        pVB, NULL );
    TestGeometryVertex* pVBData = NULL;
    ( *pVB )->Lock( 0, 0, ( VOID** )&pVBData, 0 );

    pVBData[0].Position.x = -1.0f;
    pVBData[0].Position.y = -1.0f;
    pVBData[0].Position.z =  0.0f;
    pVBData[0].TexCoord.x =  0.0f;
    pVBData[0].TexCoord.y =  1.0f;

    pVBData[1].Position.x =  1.0f;
    pVBData[1].Position.y = -1.0f;
    pVBData[1].Position.z =  0.0f;
    pVBData[1].TexCoord.x =  1.0f;
    pVBData[1].TexCoord.y =  1.0f;

    pVBData[2].Position.x = -1.0f;
    pVBData[2].Position.y =  1.0f;
    pVBData[2].Position.z =  0.0f;
    pVBData[2].TexCoord.x =  0.0f;
    pVBData[2].TexCoord.y =  0.0f;

    pVBData[3].Position.x =  1.0f;
    pVBData[3].Position.y =  1.0f;
    pVBData[3].Position.z =  0.0f;
    pVBData[3].TexCoord.x =  1.0f;
    pVBData[3].TexCoord.y =  0.0f;

    ( *pVB )->Unlock( );

    *numIndices = 4;

    // Create an index buffer and copy in the mesh index data.
    pd3dDevice->CreateIndexBuffer( *numIndices * sizeof( WORD ),
                                          0, D3DFMT_INDEX16, D3DPOOL_DEFAULT,
                                          pIB, NULL );
    WORD* pIBData = NULL;
    ( *pIB )->Lock( 0, 0, ( VOID** )&pIBData, 0 );
    *pIBData++ = 0;
    *pIBData++ = 2;
    *pIBData++ = 3;
    *pIBData++ = 1;
    ( *pIB )->Unlock( );
}


//---------------------------------------------------------------------------------------------------------
// Name: GPUCompressor::Is128BitType( )
// Desc: Returns TRUE if the block compressed type is 128 bits-per-block, FALSE if it is 64-bits per block.
//---------------------------------------------------------------------------------------------------------
BOOL GPUCompressor::Is128BitType( UINT iCompressedType )
{
    switch( iCompressedType )
    {
    case COMPRESSED_TYPE_DXT1:
    case COMPRESSED_TYPE_CTX1:
    default:
        return FALSE;
        break;
    case COMPRESSED_TYPE_DXT5:
    case COMPRESSED_TYPE_DXN:
        return TRUE;
        break;
    }
}


//---------------------------------------------------------------------------------------------------------
// Name: GPUCompressor::IsBlockCompressedFormat( )
// Desc: Returns TRUE if the format is block compressed, FALSE otherwise.
//---------------------------------------------------------------------------------------------------------
BOOL GPUCompressor::IsBlockCompressedFormat( D3DFORMAT d3dFmt )
{
    DWORD dwGpuFormat = XGGetGpuFormat( d3dFmt );
    switch( dwGpuFormat )
    {
    case GPUTEXTUREFORMAT_DXT1:
    case GPUTEXTUREFORMAT_DXT2_3:
    case GPUTEXTUREFORMAT_DXT4_5:
    case GPUTEXTUREFORMAT_DXT3A:
    case GPUTEXTUREFORMAT_DXT5A:
    case GPUTEXTUREFORMAT_DXN:
    case GPUTEXTUREFORMAT_CTX1:
    case GPUTEXTUREFORMAT_DXT1_AS_16_16_16_16:
    case GPUTEXTUREFORMAT_DXT2_3_AS_16_16_16_16:
    case GPUTEXTUREFORMAT_DXT4_5_AS_16_16_16_16:
        return TRUE;
        break;

    default:
        return FALSE;
        break;
    }
}


//---------------------------------------------------------------------------------------------------------
// Name: GPUCompressor::CreateDummyWriteRenderTargets( )
// Desc: Creates render targets which hold the output of the compression shaders.  Each target is 
// 4 channel, 16-bit fixed-point.  The write will be done so as to get bitwise accurate copying from
// the shader, through EDRAM, and back to main memory.
// 
// One render target can hold a 64-bit DXT1/CTX1 block.  Two can hold a 128-bit DXT5/DXN block.
//---------------------------------------------------------------------------------------------------------
VOID GPUCompressor::CreateDummyWriteRenderTargets( IDirect3DDevice9* pd3dDevice,  
                                                  const TextureDescAndBaseAddress* pSrcDescAndBaseAddress, 
                                                  IDirect3DSurface9** ppDummyWriteRenderTarget0, 
                                                  IDirect3DSurface9** ppDummyWriteRenderTarget1 )
{
    // Set up the dummy render target(s) to write to
    D3DSURFACE_PARAMETERS SurfParams0 = 
    {
        0,                      // DWORD Base;
        0,                      // DWORD HierarchicalZBase;
        -10,                    // INT ColorExpBias;
        D3DHIZFUNC_DEFAULT ,    // D3DHIZFUNC HiZFunc;
    };
    pd3dDevice->CreateRenderTarget( pSrcDescAndBaseAddress->Desc.Width / 4, 
        pSrcDescAndBaseAddress->Desc.Height / 4, 
        D3DFMT_A16B16G16R16_EDRAM, 
        D3DMULTISAMPLE_NONE, 
        0, 
        FALSE, 
        ppDummyWriteRenderTarget0, 
        &SurfParams0 );

    DWORD dwDummyRenderTargetSize = XGSurfaceSize( pSrcDescAndBaseAddress->Desc.Width / 4,
        pSrcDescAndBaseAddress->Desc.Height / 4,
        D3DFMT_A16B16G16R16_EDRAM, 
        D3DMULTISAMPLE_NONE );
    D3DSURFACE_PARAMETERS SurfParams1 = 
    {
        dwDummyRenderTargetSize,// DWORD Base;
        0,                      // DWORD HierarchicalZBase;
        -10,                    // INT ColorExpBias;
        D3DHIZFUNC_DEFAULT ,    // D3DHIZFUNC HiZFunc;
    };
    pd3dDevice->CreateRenderTarget( pSrcDescAndBaseAddress->Desc.Width / 4, 
        pSrcDescAndBaseAddress->Desc.Height / 4, 
        D3DFMT_A16B16G16R16_EDRAM, 
        D3DMULTISAMPLE_NONE, 
        0, 
        FALSE, 
        ppDummyWriteRenderTarget1, 
        &SurfParams1 );
}


//---------------------------------------------------------------------------------------------------------
// Name: GPUCompressor::CreateDummyResolveRenderTargets( )
// Desc: Creates render targets which alias the output of the compression shaders, for purposes of bitwise
// accurate resolve.  These are currently identical to the write render targets, but are separated
// for explicitness.
//---------------------------------------------------------------------------------------------------------
VOID GPUCompressor::CreateDummyResolveRenderTargets( IDirect3DDevice9* pd3dDevice,  
                                                    const TextureDescAndBaseAddress* pSrcDescAndBaseAddress, 
                                                    IDirect3DSurface9** ppDummyResolveRenderTarget0, 
                                                    IDirect3DSurface9** ppDummyResolveRenderTarget1 )
{
    // Set up the dummy render target(s) to resolve from
    D3DSURFACE_PARAMETERS SurfParams0 = 
    {
        0,                      // DWORD Base;
        0,                      // DWORD HierarchicalZBase;
        -10,                    // INT ColorExpBias;
        D3DHIZFUNC_DEFAULT ,    // D3DHIZFUNC HiZFunc;
    };
    pd3dDevice->CreateRenderTarget( pSrcDescAndBaseAddress->Desc.Width / 4, 
        pSrcDescAndBaseAddress->Desc.Height / 4, 
        D3DFMT_A16B16G16R16_EDRAM, 
        D3DMULTISAMPLE_NONE, 
        0, 
        FALSE, 
        ppDummyResolveRenderTarget0, 
        &SurfParams0 );

    DWORD dwDummyRenderTargetSize = XGSurfaceSize( pSrcDescAndBaseAddress->Desc.Width / 4,
        pSrcDescAndBaseAddress->Desc.Height / 4,
        D3DFMT_A16B16G16R16_EDRAM, 
        D3DMULTISAMPLE_NONE );
    D3DSURFACE_PARAMETERS SurfParams1 = 
    {
        dwDummyRenderTargetSize,// DWORD Base;
        0,                      // DWORD HierarchicalZBase;
        -10,                    // INT ColorExpBias;
        D3DHIZFUNC_DEFAULT ,    // D3DHIZFUNC HiZFunc;
    };
    pd3dDevice->CreateRenderTarget( pSrcDescAndBaseAddress->Desc.Width / 4, 
        pSrcDescAndBaseAddress->Desc.Height / 4, 
        D3DFMT_A16B16G16R16_EDRAM, 
        D3DMULTISAMPLE_NONE, 
        0, 
        FALSE, 
        ppDummyResolveRenderTarget1, 
        &SurfParams1 );
}


//---------------------------------------------------------------------------------------------------------
// Name: GPUCompressor::CreateDummySrcTexture( )
// Desc: Creates a source texture which aliases the true source, but makes some changes in format and 
// dimensions.
// 
// For compression+tiling operations, the only changes are to remove gamma-correction and '_AS_16'.
// Removal of gamma-correction is important for quality.  We wish to minimize perceptual error in 
// compression, and perceptual error is more uniform in gamma-space than in linear space.
// Removal of '_AS_16' is only important for performance, not for correctness.
// 
// For pure tiling operations, the source texture is aliased as 4 channel, 16-bit integer, for bitwise
// accurate fetching.  For resolve operations, it is more convenient to use a signed representation, 
// since EDRAM only supports signed render targets of this bit depth.  For memexport operations, an
// unsigned representation allows us to use the same shader code and memexport stream constant as
// compression.
//---------------------------------------------------------------------------------------------------------
VOID GPUCompressor::CreateDummySrcTexture( const TextureDescAndBaseAddress* pSrcDescAndBaseAddress, 
                                          UINT iCompressedType, 
                                          UINT iTilingMethod, 
                                          BOOL bTileOnly, 
                                          BOOL b128Bit, 
                                          IDirect3DTexture9* pDummySrcTexture )
{
    if( bTileOnly )
    {
        // Pick a size and format suitable for bitwise copies
        D3DFORMAT dwDummySrcTextureFormat;
        switch( iTilingMethod )
        {
        case TILING_METHOD_GPU_RESOLVE:
        default:
            dwDummySrcTextureFormat = D3DFMT_LIN_A16B16G16R16_SIGNED_INTEGER;
            break;

        case TILING_METHOD_GPU_MEMEXPORT:
            dwDummySrcTextureFormat = D3DFMT_LIN_A16B16G16R16_UNSIGNED_INTEGER;
            break;
        }
        UINT iDummySrcTextureWidth = b128Bit 
            ? ( pSrcDescAndBaseAddress->Desc.Width / 2 )
            : ( pSrcDescAndBaseAddress->Desc.Width / 4 );
        UINT iDummySrcTextureHeight = pSrcDescAndBaseAddress->Desc.Height / 4;
        XGSetTextureHeader( iDummySrcTextureWidth, 
            iDummySrcTextureHeight, 
            1, 
            0, 
            dwDummySrcTextureFormat, 
            0, 
            pSrcDescAndBaseAddress->BaseAddress, 
            0, 
            0, 
            pDummySrcTexture, 
            NULL, 
            NULL );
    }
    else
    {
        switch( iCompressedType )
        {
        case COMPRESSED_TYPE_DXT1:
        case COMPRESSED_TYPE_DXT5:
        default:
            XGSetTextureHeader( pSrcDescAndBaseAddress->Desc.Width, 
                pSrcDescAndBaseAddress->Desc.Height, 
                1, 
                0, 
                GetNonAs16NonsRGBFormat( pSrcDescAndBaseAddress->Desc.Format ), 
                0, 
                pSrcDescAndBaseAddress->BaseAddress, 
                0, 
                0, 
                pDummySrcTexture, 
                NULL, 
                NULL );
            break;

        case COMPRESSED_TYPE_DXN:
        case COMPRESSED_TYPE_CTX1:
            // Alias 16-bit format as 32-bit to halve number of fetches
            assert( pSrcDescAndBaseAddress->Desc.Format == D3DFMT_LIN_G8R8 );
            XGSetTextureHeader( pSrcDescAndBaseAddress->Desc.Width / 2, 
                pSrcDescAndBaseAddress->Desc.Height, 
                1, 
                0, 
                D3DFMT_LIN_G8R8G8R8, 
                0, 
                pSrcDescAndBaseAddress->BaseAddress, 
                0, 
                0, 
                pDummySrcTexture, 
                NULL, 
                NULL );
            break;
        }
    }
}


//---------------------------------------------------------------------------------------------------------
// Name: GPUCompressor::CreateDummyDstTextures( )
// Desc: Creates destination textures for resolve operations.  
//
// pDummyDstTexture0 aliases the true destination texture, but changes the size and dimensions to allow
// for bitwise accurate copies.
//
// pDummyDstTexture1 is a temporary buffer, used only for 128-bit block formats.  Since there is no
// 128-bit EDRAM format, two separate resolves are needed for these formats.
// 
// pdwResolveExpBias provides the Resolve exponent bias necessary to allow bitwise accurate copies.
//---------------------------------------------------------------------------------------------------------
VOID GPUCompressor::CreateDummyDstTextures( const TextureDescAndBaseAddress* pDstDescAndBaseAddress, 
                                           UINT iCompressedType, 
                                           IDirect3DTexture9* pDummyDstTexture0, 
                                           IDirect3DTexture9* pDummyDstTexture1, 
                                           DWORD* pdwResolveExpBias )
{
    // Set up the dummy destination texture
    D3DFORMAT dwDummyDstTextureFormat;
    switch( iCompressedType )
    {
        // For 64-bit types, need 64-bit tiling
        // Need bias of +10 to move from EDRAM range of ( -32,32 ) to 
        // texture range of ( -2^15, +2^15 )
    case COMPRESSED_TYPE_DXT1:
    case COMPRESSED_TYPE_CTX1:
    default:
        dwDummyDstTextureFormat = D3DFMT_A16B16G16R16_SIGNED_INTEGER;
        *pdwResolveExpBias = ( DWORD ) D3DRESOLVE_EXPONENTBIAS( +10 );
        break;

        // For 128-bit types, need 128-bit tiling
        // Because the only supported 128-bit resolve targets are float type, 
        // we need bias of -5 to move from EDRAM range of ( -32,32 ) to float 
        // range of ( -1,+1 ), and a bias of +15 to move from there back to integers
    case COMPRESSED_TYPE_DXT5:
    case COMPRESSED_TYPE_DXN:
        dwDummyDstTextureFormat = D3DFMT_A32B32G32R32F;
        *pdwResolveExpBias = ( DWORD ) D3DRESOLVE_EXPONENTBIAS( -5 );
        break;
    }

    // The first dummy destination is an alias of the real dest texture
    XGSetTextureHeader( pDstDescAndBaseAddress->Desc.Width / 4, 
        pDstDescAndBaseAddress->Desc.Height / 4, 
        1, 
        0, 
        dwDummyDstTextureFormat, 
        0, 
        pDstDescAndBaseAddress->BaseAddress, 
        0, 
        0, 
        pDummyDstTexture0, 
        NULL, 
        NULL );

    // The second dummy destination is a temp buffer, which needs to be freed later
    UINT iDummyTextureSize = XGSetTextureHeader( pDstDescAndBaseAddress->Desc.Width / 4, 
        pDstDescAndBaseAddress->Desc.Height / 4, 
        1, 
        0, 
        dwDummyDstTextureFormat, 
        0, 
        0, 
        0, 
        0, 
        pDummyDstTexture1, 
        NULL, 
        NULL );

    VOID* pTempBuffer = XPhysicalAlloc( iDummyTextureSize, MAXULONG_PTR, 0,
        PAGE_READONLY | PAGE_NOCACHE );

    XGOffsetResourceAddress( pDummyDstTexture1, pTempBuffer );
}


//---------------------------------------------------------------------------------------------------------
// Name: GPUCompressor::SelectCompressOrTilePixelShader( )
// Desc: Selects the appropriate pixel shader for the given operation.
//---------------------------------------------------------------------------------------------------------
IDirect3DPixelShader9* GPUCompressor::SelectCompressOrTilePixelShader( UINT iTilingMethod, 
                                                                      UINT iCompressedType, 
                                                                      BOOL bTileOnly )
{
    if( bTileOnly )
    {
        switch( iTilingMethod )
        {
        case TILING_METHOD_GPU_RESOLVE:
        default:
            switch( iCompressedType )
            {
            case COMPRESSED_TYPE_DXT1:
            case COMPRESSED_TYPE_CTX1:
            default:
                return m_pCopyTextureShader;
                break;
            case COMPRESSED_TYPE_DXT5:
            case COMPRESSED_TYPE_DXN:
                return m_pSplitTextureShader;
                break;
            }
            break;
        case TILING_METHOD_GPU_MEMEXPORT:
            switch( iCompressedType )
            {
            case COMPRESSED_TYPE_DXT1:
            case COMPRESSED_TYPE_CTX1:
            default:
                return m_pTileMemexport64Shader;
                break;
            case COMPRESSED_TYPE_DXT5:
            case COMPRESSED_TYPE_DXN:
                return m_pTileMemexport128Shader;
                break;
            }
            break;
        }
    }
    else
    {
        switch( iTilingMethod )
        {
        case TILING_METHOD_GPU_RESOLVE:
        default:
            switch( iCompressedType )
            {
            case COMPRESSED_TYPE_DXT1:
            default:
                return m_pEncodeDXT1ResolveShader;
                break;
            case COMPRESSED_TYPE_DXT5:
                return m_pEncodeDXT5ResolveShader;
                break;
            case COMPRESSED_TYPE_CTX1:
                return m_pEncodeCTX1ResolveShader;
                break;
            case COMPRESSED_TYPE_DXN:
                return m_pEncodeDXNResolveShader;
                break;
            }
            break;

        case TILING_METHOD_GPU_MEMEXPORT:
            switch( iCompressedType )
            {
            case COMPRESSED_TYPE_DXT1:
            default:
                return m_pEncodeDXT1MemexportShader;
                break;
            case COMPRESSED_TYPE_DXT5:
                return m_pEncodeDXT5MemexportShader;
                break;
            case COMPRESSED_TYPE_CTX1:
                return m_pEncodeCTX1MemexportShader;
                break;
            case COMPRESSED_TYPE_DXN:
                return m_pEncodeDXNMemexportShader;
                break;
            }
            break;
        }
    }
}


//---------------------------------------------------------------------------------------------------------
// Name: GPUCompressor::CreateDummyMemexportTexture( )
// Desc: Creates a texture which aliases the true destination texture, and can be used as a memexport
// target for bitwise accurate copy operations.
//---------------------------------------------------------------------------------------------------------
VOID GPUCompressor::CreateDummyMemexportTexture( const TextureDescAndBaseAddress* pDstDescAndBaseAddress, 
                                                IDirect3DTexture9* pDummyMemExportTexture )
{
    XGSetTextureHeader( pDstDescAndBaseAddress->Desc.Width / 2, 
        pDstDescAndBaseAddress->Desc.Height / 4, 
        1, 
        0, 
        D3DFMT_A16B16G16R16_SIGNED_INTEGER, 
        0, 
        pDstDescAndBaseAddress->BaseAddress, 
        0, 
        0, 
        pDummyMemExportTexture, 
        NULL, 
        NULL );
}


//---------------------------------------------------------------------------------------------------------
// Name: GPUCompressor::BeginMemexportForTiling( )
// Desc: Wrapper for BeginExport, which also sets up the lookup texture with the appropriate tiling
// pattern in sampler slot 1.
//---------------------------------------------------------------------------------------------------------
VOID GPUCompressor::BeginMemexportForTiling( IDirect3DDevice9* pd3dDevice,  
                                            const TextureDescAndBaseAddress* pDstDescAndBaseAddress, 
                                            IDirect3DTexture9* pDummyMemExportTexture, 
                                            BOOL b128Bit )
{
    pd3dDevice->BeginExport( 0, pDummyMemExportTexture, D3DBEGINEXPORT_PIXELSHADER );

    // Set up the tiling pattern texture
    const UINT iTileDim = GPU_TEXTURE_TILE_DIMENSION;
    XMVECTOR vTextureDimsInTiles = 
    {
        ( FLOAT ) ( XGNextMultiple( pDstDescAndBaseAddress->Desc.WidthInBlocks, iTileDim ) / iTileDim ), 
        ( FLOAT ) ( XGNextMultiple( pDstDescAndBaseAddress->Desc.HeightInBlocks, iTileDim ) / iTileDim ), 
    };
    pd3dDevice->SetVertexShaderConstantF( 4, ( FLOAT* ) &vTextureDimsInTiles, 1 );

    XMVECTOR vTilePitchInTexels = 
    {
        ( FLOAT ) ( iTileDim * iTileDim ), 
        ( FLOAT ) ( iTileDim * XGNextMultiple( pDstDescAndBaseAddress->Desc.WidthInBlocks, iTileDim ) ), 
    };
    pd3dDevice->SetPixelShaderConstantF( 0, ( FLOAT* ) &vTilePitchInTexels, 1 );

    // Set up the matching MemExport stream constant
    GPU_MEMEXPORT_STREAM_CONSTANT MemExportStreamConstant;

    // How many elements are in the dest texture, considered as an array of 64-bit data
    UINT iMaxIndex = ( pDstDescAndBaseAddress->Desc.RowPitch  / pDstDescAndBaseAddress->Desc.BytesPerBlock )
        * pDstDescAndBaseAddress->Desc.HeightInBlocks;
    if( b128Bit )
    {
        iMaxIndex *= 2;
    }
    GPU_SET_MEMEXPORT_STREAM_CONSTANT( &MemExportStreamConstant, 
        ( VOID* ) pDstDescAndBaseAddress->BaseAddress, 
        iMaxIndex, 
        SURFACESWAP_LOW_RED, 
        GPUSURFACENUMBER_UINTEGER, 
        GPUCOLORFORMAT_16_16_16_16, 
        GPUENDIAN128_8IN16 );

    pd3dDevice->SetPixelShaderConstantF( 1, ( FLOAT* ) &MemExportStreamConstant, 1 );

    IDirect3DTexture9* pTilingPatternTexture = b128Bit 
        ? m_pLinearToTiled2DAddress128Bit 
        : m_pLinearToTiled2DAddress64Bit;
    pd3dDevice->SetTexture( 1, pTilingPatternTexture );
    pd3dDevice->SetSamplerState( 1, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP );
    pd3dDevice->SetSamplerState( 1, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP );
    pd3dDevice->SetSamplerState( 1, D3DSAMP_ADDRESSW, D3DTADDRESS_WRAP );
}


//---------------------------------------------------------------------------------------------------------
// Name: GPUCompressor::EndMemexportForTiling( )
// Desc: Wrapper for EndExport.
//---------------------------------------------------------------------------------------------------------
VOID GPUCompressor::EndMemexportForTiling( IDirect3DDevice9* pd3dDevice,  
                                          IDirect3DTexture9* pDummyMemExportTexture )
{
    pd3dDevice->EndExport( 0, pDummyMemExportTexture, 0 );
}


//---------------------------------------------------------------------------------------------------------
// Name: GPUCompressor::MergeBlocksGPU( )
// Desc: Helper function for generating 128-bit tiled formats on the GPU.
// Assume we have rendered the first 64 bits as 16:16:16:16 to render target 0.
// Assume we have rendered the second 64 bits as 16:16:16:16 to render target 1.
// Assume we have resolved both render targets as 32:32:32:32F with values in ( -1,+1 ).
// Now we must combine the two values without destroying the tiling.
//---------------------------------------------------------------------------------------------------------
VOID GPUCompressor::MergeBlocksGPU( IDirect3DDevice9* pd3dDevice,  
                                   const TextureDescAndBaseAddress* pSrcDescAndBaseAddress0,
                                   const TextureDescAndBaseAddress* pSrcDescAndBaseAddress1, 
                                   const TextureDescAndBaseAddress* pDstDescAndBaseAddress )
{
    assert( pSrcDescAndBaseAddress0->Desc.Width == pSrcDescAndBaseAddress1->Desc.Width 
        && pSrcDescAndBaseAddress0->Desc.Width == pDstDescAndBaseAddress->Desc.Width );
    assert( pSrcDescAndBaseAddress0->Desc.Height == pSrcDescAndBaseAddress1->Desc.Height 
        && pSrcDescAndBaseAddress0->Desc.Height == pDstDescAndBaseAddress->Desc.Height );

    // Set up the quad to draw
    pd3dDevice->SetIndices( m_pIB );
    pd3dDevice->SetStreamSource( 0, m_pVB, 0, sizeof( TestGeometryVertex ) );
    pd3dDevice->SetVertexDeclaration( m_pVertexDecl );
    pd3dDevice->SetVertexShader( m_pVertexShader );

    // Alias the source textures as 32:32F to produce a 64-bit tiling pattern.
    // This pattern will be undone by a 64-bit resolve.
    // Use an expbias of +15 to retrieve integer values from the ( -1,1 ) float source.
    IDirect3DTexture9 DummySrcTexture0;
    XGSetTextureHeaderEx( pSrcDescAndBaseAddress0->Desc.Width * 2, 
        pSrcDescAndBaseAddress0->Desc.Height, 
        1, 
        0, 
        D3DFMT_G32R32F, 
        +15, 
        0, 
        pSrcDescAndBaseAddress0->BaseAddress, 
        0, 
        0, 
        &DummySrcTexture0, 
        NULL, 
        NULL );
    IDirect3DTexture9 DummySrcTexture1;
    XGSetTextureHeaderEx( pSrcDescAndBaseAddress1->Desc.Width * 2, 
        pSrcDescAndBaseAddress1->Desc.Height, 
        1, 
        0, 
        D3DFMT_G32R32F, 
        +15, 
        0, 
        pSrcDescAndBaseAddress1->BaseAddress, 
        0, 
        0, 
        &DummySrcTexture1, 
        NULL, 
        NULL );

    pd3dDevice->SetTexture( 0, &DummySrcTexture0 );
    pd3dDevice->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_POINT );
    pd3dDevice->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_POINT );
    pd3dDevice->SetSamplerState( 0, D3DSAMP_MIPFILTER, D3DTEXF_POINT );
    pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP );
    pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP );

    pd3dDevice->SetTexture( 1, &DummySrcTexture1 );
    pd3dDevice->SetSamplerState( 1, D3DSAMP_MINFILTER, D3DTEXF_POINT );
    pd3dDevice->SetSamplerState( 1, D3DSAMP_MAGFILTER, D3DTEXF_POINT );
    pd3dDevice->SetSamplerState( 1, D3DSAMP_MIPFILTER, D3DTEXF_POINT );
    pd3dDevice->SetSamplerState( 1, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP );
    pd3dDevice->SetSamplerState( 1, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP );

    // Set a pixel shader which copies either a texel from texture 0 or
    // a texel from texture 1, depending on the texcoord
    pd3dDevice->SetPixelShader( m_pMergeBlocksShader );

    // Set up the dummy render target( s ) to write to
    IDirect3DSurface9* pOldRenderTarget;
    pd3dDevice->GetRenderTarget( 0, &pOldRenderTarget );
    D3DSURFACE_PARAMETERS SurfParams = 
    {
        0,                      // DWORD Base;
        0,                      // DWORD HierarchicalZBase;
        -10,                    // INT ColorExpBias;
        D3DHIZFUNC_DEFAULT ,    // D3DHIZFUNC HiZFunc;
    };
    IDirect3DSurface9* pDummyRenderTarget = NULL;
    pd3dDevice->CreateRenderTarget( pDstDescAndBaseAddress->Desc.Width * 2, 
        pDstDescAndBaseAddress->Desc.Height, 
        D3DFMT_A16B16G16R16_EDRAM, 
        D3DMULTISAMPLE_NONE, 
        0, 
        FALSE, 
        &pDummyRenderTarget, 
        &SurfParams );
    pd3dDevice->SetRenderTarget( 0, pDummyRenderTarget );

    // Draw the geometry
    pd3dDevice->DrawIndexedPrimitive( D3DPT_QUADLIST, 0, 0, 0, 0, m_iIndexCount / 4 );

    // Create the dummy destination texture to resolve into
    IDirect3DTexture9 DummyDstTexture;
    XGSetTextureHeader( pDstDescAndBaseAddress->Desc.Width * 2, 
        pDstDescAndBaseAddress->Desc.Height, 
        1, 
        0, 
        D3DFMT_A16B16G16R16_SIGNED_INTEGER, 
        0, 
        pDstDescAndBaseAddress->BaseAddress, 
        0, 
        0, 
        &DummyDstTexture, 
        NULL, 
        NULL );

    // Perform the resolve
    DWORD dwResolveExpBias = ( DWORD ) D3DRESOLVE_EXPONENTBIAS( +10 );
    pd3dDevice->Resolve( D3DRESOLVE_RENDERTARGET0 | dwResolveExpBias, NULL, &DummyDstTexture,
        NULL, 0, 0, NULL, 1.0f, 0L, NULL );

    // Clean up
    pd3dDevice->SetTexture( 0, NULL );
    pd3dDevice->SetTexture( 1, NULL );
    pd3dDevice->SetRenderTarget( 0, pOldRenderTarget );   // necessary for Release
    pDummyRenderTarget->Release( );
}


//---------------------------------------------------------------------------------------------------------
// Name: GPUCompressor::Initialize( )
// Desc: Initialize resources used by the GPU compressor.
//---------------------------------------------------------------------------------------------------------
VOID GPUCompressor::Initialize( IDirect3DDevice9* pd3dDevice )
{
    // Create common vertex declaration used by all the geometry
    // We duplicate these from the main sample, so that this class can be a stand-alone
    static const D3DVERTEXELEMENT9 decl[] =
    {
        { 0, 0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
        { 0, 12, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
        { 0, 20, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TANGENT, 0 },
        { 0, 32, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_BINORMAL, 0 },
        { 0, 44, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_NORMAL, 0 },
        D3DDECL_END( )
    };

    pd3dDevice->CreateVertexDeclaration( decl, &m_pVertexDecl );

    GenerateGeometryQuad( pd3dDevice, &m_pVB, &m_pIB, &m_iIndexCount );

    // Create the tiling lookup textures.  The tiling pattern is always 32x32.
    // These textures are essentially a lookup map for the function XGAddress2DTiledOffset.
    // They are used for manually tiling a texture with memexport.
    pd3dDevice->CreateTexture( 32, 
        32, 
        1, 
        0, 
        D3DFMT_L16_INTEGER, 
        0, 
        &m_pLinearToTiled2DAddress64Bit, 
        NULL );
    pd3dDevice->CreateTexture( 32, 
        32, 
        1, 
        0, 
        D3DFMT_L16_INTEGER, 
        0, 
        &m_pLinearToTiled2DAddress128Bit, 
        NULL );

    XGTEXTURE_DESC Desc64Bit;
    XGGetTextureDesc( m_pLinearToTiled2DAddress64Bit, 0, &Desc64Bit );
    XGTEXTURE_DESC Desc128Bit;
    XGGetTextureDesc( m_pLinearToTiled2DAddress128Bit, 0, &Desc128Bit );

    D3DLOCKED_RECT LockedRect64Bit;
    m_pLinearToTiled2DAddress64Bit->LockRect( 0, &LockedRect64Bit, NULL, 0 );
    D3DLOCKED_RECT LockedRect128Bit;
    m_pLinearToTiled2DAddress128Bit->LockRect( 0, &LockedRect128Bit, NULL, 0 );

    WORD* pOffset64Bit = ( WORD* ) LockedRect64Bit.pBits;
    WORD* pOffset128Bit = ( WORD* ) LockedRect128Bit.pBits;
    for( UINT y = 0; y < 32; ++y )
    {
        for( UINT x = 0; x < 32; ++x )
        {
            WORD iAlignedOffset = ( WORD ) ( Desc64Bit.RowPitch * y + x );
            WORD iTiledOffset64Bit = ( WORD ) XGAddress2DTiledOffset( x, y, 32, 8 );
            WORD iTiledOffset128Bit = ( WORD ) XGAddress2DTiledOffset( x, y, 32, 16 );

            pOffset64Bit[iAlignedOffset] = iTiledOffset64Bit;
            pOffset128Bit[iAlignedOffset] = iTiledOffset128Bit;
        }
    }

    XGTileSurface( pOffset64Bit, 32, 32, NULL, pOffset64Bit, 128, NULL, 2 );
    XGTileSurface( pOffset128Bit, 32, 32, NULL, pOffset128Bit, 128, NULL, 2 );

    m_pLinearToTiled2DAddress128Bit->UnlockRect( 0 );
    m_pLinearToTiled2DAddress64Bit->UnlockRect( 0 );

    // Create shaders
    if( FAILED( ATG::LoadVertexShader( "game:\\Media\\Shaders\\ScreenSpaceShader.xvu",
                                       &m_pVertexShader ) ) )
    {
        ATG::FatalError( "Couldn't create VertexShader\n" );
    }
    if( FAILED( ATG::LoadPixelShader( "game:\\Media\\Shaders\\CopyTexture.xpu",
                                       &m_pCopyTextureShader ) ) )
    {
        ATG::FatalError( "Couldn't create PixelShader\n" );
    }
    if( FAILED( ATG::LoadPixelShader( "game:\\Media\\Shaders\\SplitTexture.xpu",
                                       &m_pSplitTextureShader ) ) )
    {
        ATG::FatalError( "Couldn't create PixelShader\n" );
    }
    if( FAILED( ATG::LoadPixelShader( "game:\\Media\\Shaders\\TileMemexport64.xpu",
                                       &m_pTileMemexport64Shader ) ) )
    {
        ATG::FatalError( "Couldn't create PixelShader\n" );
    }
    if( FAILED( ATG::LoadPixelShader( "game:\\Media\\Shaders\\TileMemexport128.xpu",
                                       &m_pTileMemexport128Shader ) ) )
    {
        ATG::FatalError( "Couldn't create PixelShader\n" );
    }
    if( FAILED( ATG::LoadPixelShader( "game:\\Media\\Shaders\\MergeBlocks.xpu",
                                       &m_pMergeBlocksShader ) ) )
    {
        ATG::FatalError( "Couldn't create PixelShader\n" );
    }
    if( FAILED( ATG::LoadPixelShader( "game:\\Media\\Shaders\\EncodeDXT1_Resolve.xpu",
                                       &m_pEncodeDXT1ResolveShader ) ) )
    {
        ATG::FatalError( "Couldn't create PixelShader\n" );
    }
    if( FAILED( ATG::LoadPixelShader( "game:\\Media\\Shaders\\EncodeDXT5_Resolve.xpu",
                                       &m_pEncodeDXT5ResolveShader ) ) )
    {
        ATG::FatalError( "Couldn't create PixelShader\n" );
    }
    if( FAILED( ATG::LoadPixelShader( "game:\\Media\\Shaders\\EncodeCTX1_Resolve.xpu",
                                       &m_pEncodeCTX1ResolveShader ) ) )
    {
        ATG::FatalError( "Couldn't create PixelShader\n" );
    }
    if( FAILED( ATG::LoadPixelShader( "game:\\Media\\Shaders\\EncodeDXN_Resolve.xpu",
                                       &m_pEncodeDXNResolveShader ) ) )
    {
        ATG::FatalError( "Couldn't create PixelShader\n" );
    }
    if( FAILED( ATG::LoadPixelShader( "game:\\Media\\Shaders\\EncodeDXT1_Memexport.xpu",
                                       &m_pEncodeDXT1MemexportShader ) ) )
    {
        ATG::FatalError( "Couldn't create PixelShader\n" );
    }
    if( FAILED( ATG::LoadPixelShader( "game:\\Media\\Shaders\\EncodeDXT5_Memexport.xpu",
                                       &m_pEncodeDXT5MemexportShader ) ) )
    {
        ATG::FatalError( "Couldn't create PixelShader\n" );
    }
    if( FAILED( ATG::LoadPixelShader( "game:\\Media\\Shaders\\EncodeCTX1_Memexport.xpu",
                                       &m_pEncodeCTX1MemexportShader ) ) )
    {
        ATG::FatalError( "Couldn't create PixelShader\n" );
    }
    if( FAILED( ATG::LoadPixelShader( "game:\\Media\\Shaders\\EncodeDXN_Memexport.xpu",
                                       &m_pEncodeDXNMemexportShader ) ) )
    {
        ATG::FatalError( "Couldn't create PixelShader\n" );
    }
}


//---------------------------------------------------------------------------------------------------------
// Name: GPUCompressor::CompressAndTileTextureGPU( )
// Desc: Root compression/tiling routine for all GPU-based methods.
//---------------------------------------------------------------------------------------------------------
VOID GPUCompressor::CompressAndTileTexture( IDirect3DDevice9* pd3dDevice, 
                                           const TextureDescAndBaseAddress* pSrcDescAndBaseAddress, 
                                           const TextureDescAndBaseAddress* pDstDescAndBaseAddress, 
                                           UINT iCompressedType, 
                                           UINT iTilingMethod, 
                                           UINT iGPURepeatCount )
{
    assert( pDstDescAndBaseAddress->Desc.Width == pSrcDescAndBaseAddress->Desc.Width 
        && pDstDescAndBaseAddress->Desc.Height == pSrcDescAndBaseAddress->Desc.Height );

    // Record old settings
    IDirect3DSurface9* pOldRenderTarget0;
    pd3dDevice->GetRenderTarget( 0, &pOldRenderTarget0 );
    IDirect3DSurface9* pOldRenderTarget1;
    pd3dDevice->GetRenderTarget( 1, &pOldRenderTarget1 );

    // Check whether the output is 64 bits per block or 128 bits per block
    BOOL b128Bit = Is128BitType( iCompressedType );

    // If the source is block compressed, that means we just want to tile
    BOOL bTileOnly = IsBlockCompressedFormat( pSrcDescAndBaseAddress->Desc.Format );
    assert( !bTileOnly || !XGIsTiledFormat( pSrcDescAndBaseAddress->Desc.Format ) );
    assert( !bTileOnly || iTilingMethod == TILING_METHOD_GPU_RESOLVE 
        || iTilingMethod == TILING_METHOD_GPU_MEMEXPORT );

    // Change GPR allocation to favor pixel shader
    //-----------------------------------------------------------------------------------------------------
    // WARNING:  This is one source of significant GPU overhead
    //-----------------------------------------------------------------------------------------------------
    DWORD dwOldGPRAllocationFlags, dwOldGPRAllocationVSCount, dwOldGPRAllocationPSCount;
    pd3dDevice->GetShaderGPRAllocation( &dwOldGPRAllocationFlags, &dwOldGPRAllocationVSCount, 
        &dwOldGPRAllocationPSCount );
    pd3dDevice->SetShaderGPRAllocation( 0, 16, 128 - 16 );

    // Set up the quad to draw
    pd3dDevice->SetIndices( m_pIB );
    pd3dDevice->SetStreamSource( 0, m_pVB, 0, sizeof( TestGeometryVertex ) );
    pd3dDevice->SetVertexDeclaration( m_pVertexDecl );
    pd3dDevice->SetVertexShader( m_pVertexShader );

    // Set up the matrix to map the quad to all of the viewport
    XMMATRIX matProj = XMMatrixOrthographicLH( 2.0f, 2.0f, 0.0f, 4.0f );
    XMMATRIX matWVP = matProj;
    XMMATRIX matWVPT = XMMatrixTranspose( matWVP );
    pd3dDevice->SetVertexShaderConstantF( 0, ( FLOAT* )&matWVPT, 4 );

    // Set up the dummy render target(s) to write to
    IDirect3DSurface9* pDummyWriteRenderTarget0, * pDummyWriteRenderTarget1;
    CreateDummyWriteRenderTargets( pd3dDevice, 
        pSrcDescAndBaseAddress, 
        &pDummyWriteRenderTarget0, 
        &pDummyWriteRenderTarget1 );
    pd3dDevice->SetRenderTarget( 0, pDummyWriteRenderTarget0 );
    if( b128Bit )
    {
        pd3dDevice->SetRenderTarget( 1, pDummyWriteRenderTarget1 );
    }

    // Set up the dummy render target(s) to resolve from
    IDirect3DSurface9* pDummyResolveRenderTarget0, * pDummyResolveRenderTarget1;
    CreateDummyResolveRenderTargets( pd3dDevice, 
        pSrcDescAndBaseAddress, 
        &pDummyResolveRenderTarget0, 
        &pDummyResolveRenderTarget1 );

    // Set up the dummy source texture (for compression we don't want gamma-correction, 
    // even if the texture is gamma-corrected)
    assert( pSrcDescAndBaseAddress->Desc.Width % 4 == 0 
        && pSrcDescAndBaseAddress->Desc.Height % 4 == 0 );
    IDirect3DTexture9 DummySrcTexture;
    CreateDummySrcTexture( pSrcDescAndBaseAddress, 
        iCompressedType, 
        iTilingMethod, 
        bTileOnly, 
        b128Bit, 
        &DummySrcTexture );
    pd3dDevice->SetTexture( 0, &DummySrcTexture );
    pd3dDevice->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_POINT );
    pd3dDevice->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_POINT );
    pd3dDevice->SetSamplerState( 0, D3DSAMP_MIPFILTER, D3DTEXF_POINT );

    // Set up the dummy dest texture(s) to resolve to
    IDirect3DTexture9 DummyDstTexture0, DummyDstTexture1;
    DWORD dwResolveExpBias;
    CreateDummyDstTextures( pDstDescAndBaseAddress, 
        iCompressedType, 
        &DummyDstTexture0, 
        &DummyDstTexture1, 
        &dwResolveExpBias );

    // Turn off alpha-blend ( we don't restore this state )
    pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );

    // Set the pixel shader
    IDirect3DPixelShader9* pPixelShader = SelectCompressOrTilePixelShader( iTilingMethod, 
        iCompressedType, 
        bTileOnly );
    pd3dDevice->SetPixelShader( pPixelShader );

    // Decide how RGB anchors will be chosen
    BOOL bAnchorChoice = FALSE;
    pd3dDevice->SetPixelShaderConstantB( 0, &bAnchorChoice, 1 );

    IDirect3DTexture9 DummyMemExportTexture;
    CreateDummyMemexportTexture( pDstDescAndBaseAddress, &DummyMemExportTexture );

    switch( iTilingMethod )
    {
    case TILING_METHOD_GPU_MEMEXPORT:
        //-------------------------------------------------------------------------------------------------
        // WARNING:  This is one source of significant GPU overhead
        //-------------------------------------------------------------------------------------------------
        BeginMemexportForTiling( pd3dDevice, 
            pDstDescAndBaseAddress, 
            &DummyMemExportTexture, 
            b128Bit );
        break;

    case TILING_METHOD_GPU_RESOLVE:
    default:
        break;
    }

    for( UINT i = 0; i < iGPURepeatCount; ++i )
    {
        // Draw the geometry
        pd3dDevice->DrawIndexedPrimitive( D3DPT_RECTLIST, 0, 0, 0, 0, m_iIndexCount / 4 );

        // If we are tiling by Resolve, perform the proper resolves
        switch( iTilingMethod )
        {
        case TILING_METHOD_GPU_MEMEXPORT:
            break;

        case TILING_METHOD_GPU_RESOLVE:
        default:
            {
                // Set up the dummy render target to resolve from
                pd3dDevice->SetRenderTarget( 0, pDummyResolveRenderTarget0 );

                // Resolve to the dummy destination texture
                pd3dDevice->Resolve( D3DRESOLVE_RENDERTARGET0 | dwResolveExpBias, 
                    NULL, 
                    &DummyDstTexture0,
                    NULL, 
                    0, 
                    0, 
                    NULL, 
                    1.0f, 
                    0L, 
                    NULL );

                // If the compression type is 64-bit, then we're done.  
                // If the compression type is 128-bit, then we need to resolve the second 64-bits to 
                // another location and combine them.  
                if( b128Bit )
                {
                    // Resolve the second 64-bit block of data from render target 1
                    // Set up the dummy render target to resolve from
                    pd3dDevice->SetRenderTarget( 1, pDummyResolveRenderTarget1 );

                    // Resolve to the dummy destination texture
                    pd3dDevice->Resolve( D3DRESOLVE_RENDERTARGET1 | dwResolveExpBias, 
                        NULL, 
                        &DummyDstTexture1,
                        NULL, 
                        0, 
                        0, 
                        NULL, 
                        1.0f, 
                        0L, 
                        NULL );

                    // At this point, we have two correctly tiled textures, each implicitly containing
                    // 64-bits of the 128-bit data per block
                    TextureDescAndBaseAddress SrcDescAndBaseAddress0;
                    XGGetTextureDesc( &DummyDstTexture0, 0, &SrcDescAndBaseAddress0.Desc );
                    SrcDescAndBaseAddress0.BaseAddress = 
                        DummyDstTexture0.Format.BaseAddress << GPU_TEXTURE_ADDRESS_SHIFT;

                    TextureDescAndBaseAddress SrcDescAndBaseAddress1;
                    XGGetTextureDesc( &DummyDstTexture1, 0, &SrcDescAndBaseAddress1.Desc );
                    SrcDescAndBaseAddress1.BaseAddress = 
                        DummyDstTexture1.Format.BaseAddress << GPU_TEXTURE_ADDRESS_SHIFT;

                    MergeBlocksGPU( pd3dDevice, 
                        &SrcDescAndBaseAddress0, 
                        &SrcDescAndBaseAddress1, 
                        &SrcDescAndBaseAddress0 );

                    // Restore the overwritten state in case we are performing more passes
                    pd3dDevice->SetTexture( 0, &DummySrcTexture );
                    pd3dDevice->SetRenderTarget( 1, pDummyWriteRenderTarget1 );
                    pd3dDevice->SetPixelShader( pPixelShader );
                }

                // Restore the overwritten state in case we are performing more passes
                pd3dDevice->SetRenderTarget( 0, pDummyWriteRenderTarget0 );
            }
            break;
        }
    }

    switch( iTilingMethod )
    {
    case TILING_METHOD_GPU_RESOLVE:
    default:
        break;

    case TILING_METHOD_GPU_MEMEXPORT:
        EndMemexportForTiling( pd3dDevice, 
            &DummyMemExportTexture );
        break;
    }

    // Overall clean up
    pd3dDevice->SetTexture( 0, NULL );
    pd3dDevice->SetTexture( 1, NULL );
    pd3dDevice->SetRenderTarget( 0, pOldRenderTarget0 );
    pd3dDevice->SetRenderTarget( 1, pOldRenderTarget1 );
    pDummyWriteRenderTarget0->Release( );
    pDummyWriteRenderTarget1->Release( );
    pDummyResolveRenderTarget0->Release( );
    pDummyResolveRenderTarget1->Release( );
    XPhysicalFree( ( VOID* ) ( DummyDstTexture1.Format.BaseAddress << GPU_TEXTURE_ADDRESS_SHIFT ) );

    pd3dDevice->SetShaderGPRAllocation( dwOldGPRAllocationFlags, dwOldGPRAllocationVSCount, 
        dwOldGPRAllocationPSCount );
}



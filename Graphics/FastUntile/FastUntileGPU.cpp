//---------------------------------------------------------------------------------------------------------
// FastUntileGPU.cpp
//
// XNA Developer Connection
// Copyright ( C ) Microsoft Corporation. All rights reserved.
//---------------------------------------------------------------------------------------------------------

#include <assert.h>

#include <xtl.h>
#include <xgraphics.h>

#include "FastUntile.h"
#include "FastUntileGPU.h"


//---------------------------------------------------------------------------------------------------------
// For pure untiling operations, the default GPR allocation is good enough to minimize texture stalls.
// So might as well avoid the (significant) overhead of changing/restoring allocations.
//
// For untiling in combination with other per-pixel work it may be worthwhile to shift max GPRs to the 
// pixel shader.
//---------------------------------------------------------------------------------------------------------
//#define FAST_UNTILE_ADJUST_GPR_ALLOCATION


//---------------------------------------------------------------------------------------------------------
// Global compile-time constants
//---------------------------------------------------------------------------------------------------------
static const UINT g_iTexelsPerAllocExport = 4;          // Number of memexport registers 
                                                        // --- actually 5, but easier to restrict to 4
static const UINT g_iBytesPerMemoryTransaction = 32;    // granularity of GPU memexports
static const UINT g_iFieldsPerMemoryTransaction = 4 * g_iTexelsPerAllocExport;  // One float4 per register

//---------------------------------------------------------------------------------------------------------
// Helper functions.  
//---------------------------------------------------------------------------------------------------------
D3DFORMAT GetTiledFormat( D3DFORMAT fmtBase )
{
    return (D3DFORMAT) ( fmtBase | D3DFORMAT_TILED_MASK );
}


//---------------------------------------------------------------------------------------------------------
// Name: ReplaceGpuFormat( )
// Desc: Change the GPUTEXTUREFORMAT part of a D3DFORMAT, without changing anything else.  
//---------------------------------------------------------------------------------------------------------
__forceinline VOID ReplaceGpuFormat( D3DFORMAT& d3dFormat, GPUTEXTUREFORMAT NewGpuFormat )
{
    d3dFormat = (D3DFORMAT) ( ( d3dFormat & ~D3DFORMAT_TEXTUREFORMAT_MASK ) | ( NewGpuFormat << D3DFORMAT_TEXTUREFORMAT_SHIFT ) );
}


//---------------------------------------------------------------------------------------------------------
// Name: GPUUntiler::InitializeRemapping( )
// Desc: Initialize the linear-to-tiled lookup texture.
//---------------------------------------------------------------------------------------------------------
VOID GPUUntiler::InitializeRemapping( IDirect3DDevice9* pd3dDevice, UINT iTexelPitch )
{
    // We cannot have more than a 64-bit render target
    if( iTexelPitch > 8 )
    {
        iTexelPitch = 8;
    }

    GetRepeatBlockDimensions( iTexelPitch, &m_iRepeatBlockWidth, &m_iRepeatBlockHeight );

    // Build a texture which serves as a lookup map from linear coordinates to tiled coordinates.
    // The tiling pattern depends only on the bit-depth of the texture, so we only need one of 
    // these per supported texel size.
    if( m_pLinearToTiled2DAddressTexture != NULL )
    {
        m_pLinearToTiled2DAddressTexture->Release();
    }
    pd3dDevice->CreateLineTexture( m_iRepeatBlockWidth * m_iRepeatBlockHeight, 
        1, 
        0,
        (D3DFORMAT) MAKELINFMT( D3DFMT_D16 ), // Worst case needs to encode texel offsets [0,..,4096]
        D3DPOOL_DEFAULT, 
        &m_pLinearToTiled2DAddressTexture, 
        NULL );

    XGTEXTURE_DESC LinearToTiledDesc;
    XGGetTextureDesc( m_pLinearToTiled2DAddressTexture, 0, &LinearToTiledDesc );

    // Make sure we allocated enough bits per texel for the lookup
    assert( m_iRepeatBlockWidth * m_iRepeatBlockHeight < (UINT) ( 1 << LinearToTiledDesc.BitsPerPixel ) );

    D3DLOCKED_RECT LockedRectLinearToTiled;
    m_pLinearToTiled2DAddressTexture->LockRect( 0, &LockedRectLinearToTiled, NULL, 0 );

    CalculateLinearToTiledRemapping( iTexelPitch, (WORD*) LockedRectLinearToTiled.pBits );

    m_pLinearToTiled2DAddressTexture->UnlockRect( 0 );
}


//---------------------------------------------------------------------------------------------------------
// Name: GPUUntiler::Initialize( )
// Desc: Initialize the GPU resources required for untiling
//---------------------------------------------------------------------------------------------------------
VOID GPUUntiler::Initialize( IDirect3DDevice9* pd3dDevice )
{
    // Create common vertex declaration used by all the geometry
    static const D3DVERTEXELEMENT9 decl[] =
    {
        { 0, 0, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
        { 0, 8, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
        D3DDECL_END( )
    };

    pd3dDevice->CreateVertexDeclaration( decl, &m_pVertexDecl );

    // Create shaders
    if( FAILED( ATG::LoadVertexShader( "game:\\Media\\Shaders\\CopyTexture.xvu",
                                       &m_pCopyTextureVS ) ) )
    {
        ATG::FatalError( "Couldn't create VertexShader\n" );
    }
    if( FAILED( ATG::LoadPixelShader( "game:\\Media\\Shaders\\CopyTexture.xpu",
                                       &m_pCopyTexturePS ) ) )
    {
        ATG::FatalError( "Couldn't create PixelShader\n" );
    }
    if( FAILED( ATG::LoadPixelShader( "game:\\Media\\Shaders\\UntileMemexportTexel.xpu",
                                       &m_pUntileMemexportTexelPS ) ) )
    {
        ATG::FatalError( "Couldn't create PixelShader\n" );
    }
    if( FAILED( ATG::LoadPixelShader( "game:\\Media\\Shaders\\UntileMemexportTransaction128bpp.xpu",
                                       &m_pUntileMemexportTransaction128bppPS ) ) )
    {
        ATG::FatalError( "Couldn't create PixelShader\n" );
    }
    if( FAILED( ATG::LoadPixelShader( "game:\\Media\\Shaders\\UntileMemexportTransaction64bpp.xpu",
                                       &m_pUntileMemexportTransaction64bppPS ) ) )
    {
        ATG::FatalError( "Couldn't create PixelShader\n" );
    }
    if( FAILED( ATG::LoadPixelShader( "game:\\Media\\Shaders\\UntileMemexportTransaction32bpp.xpu",
                                       &m_pUntileMemexportTransaction32bppPS ) ) )
    {
        ATG::FatalError( "Couldn't create PixelShader\n" );
    }
    if( FAILED( ATG::LoadPixelShader( "game:\\Media\\Shaders\\UntileMemexportTransaction16bpp.xpu",
                                       &m_pUntileMemexportTransaction16bppPS ) ) )
    {
        ATG::FatalError( "Couldn't create PixelShader\n" );
    }
    if( FAILED( ATG::LoadPixelShader( "game:\\Media\\Shaders\\UntileMemexportTransaction8bpp.xpu",
                                       &m_pUntileMemexportTransaction8bppPS ) ) )
    {
        ATG::FatalError( "Couldn't create PixelShader\n" );
    }
    if( FAILED( ATG::LoadPixelShader( "game:\\Media\\Shaders\\UntileMemexportPacked32bpp.xpu",
                                       &m_pUntileMemexportPacked32bppPS ) ) )
    {
        ATG::FatalError( "Couldn't create PixelShader\n" );
    }
    if( FAILED( ATG::LoadPixelShader( "game:\\Media\\Shaders\\UntileMemexportPacked16bpp.xpu",
                                       &m_pUntileMemexportPacked16bppPS ) ) )
    {
        ATG::FatalError( "Couldn't create PixelShader\n" );
    }
    if( FAILED( ATG::LoadPixelShader( "game:\\Media\\Shaders\\UntileMemexportPacked8bpp.xpu",
                                       &m_pUntileMemexportPacked8bppPS ) ) )
    {
        ATG::FatalError( "Couldn't create PixelShader\n" );
    }
    if( FAILED( ATG::LoadPixelShader( "game:\\Media\\Shaders\\UntileResolve128bpp.xpu",
                                       &m_pUntileResolve128bppPS ) ) )
    {
        ATG::FatalError( "Couldn't create PixelShader\n" );
    }
    if( FAILED( ATG::LoadPixelShader( "game:\\Media\\Shaders\\UntileResolve64bpp.xpu",
                                       &m_pUntileResolve64bppPS ) ) )
    {
        ATG::FatalError( "Couldn't create PixelShader\n" );
    }
    if( FAILED( ATG::LoadPixelShader( "game:\\Media\\Shaders\\UntileResolve32bpp.xpu",
                                       &m_pUntileResolve32bppPS ) ) )
    {
        ATG::FatalError( "Couldn't create PixelShader\n" );
    }
    if( FAILED( ATG::LoadPixelShader( "game:\\Media\\Shaders\\UntileResolve16bpp.xpu",
                                       &m_pUntileResolve16bppPS ) ) )
    {
        ATG::FatalError( "Couldn't create PixelShader\n" );
    }
    if( FAILED( ATG::LoadPixelShader( "game:\\Media\\Shaders\\UntileResolve8bpp.xpu",
                                       &m_pUntileResolve8bppPS ) ) )
    {
        ATG::FatalError( "Couldn't create PixelShader\n" );
    }

    m_pLinearToTiled2DAddressTexture = NULL;
}


//---------------------------------------------------------------------------------------------------------
// Name: GPUUntiler::CreateDummyNonAs16NonsRGBTexture( )
// Desc: Remove gamma correction and '_AS_16' from the texture.  These are irrelevant to untiling, 
// and cost perf.
//---------------------------------------------------------------------------------------------------------
inline VOID GPUUntiler::SetDummyNonAs16NonsRGBTexture( IDirect3DTexture9* pSourceTexture )
{
    XGTEXTURE_DESC SourceDesc;
    XGGetTextureDesc( pSourceTexture, 0, &SourceDesc );

    IDirect3DTexture9 DummyTextureHeader;

    XGSetTextureHeader( SourceDesc.Width, 
        SourceDesc.Height, 
        1,
        ( pSourceTexture->Common & D3DCOMMON_CPU_CACHED_MEMORY ) ? D3DUSAGE_CPU_CACHED_MEMORY : 0, 
        GetNonAs16NonsRGBFormat( SourceDesc.Format ), 
        0, 
        pSourceTexture->Format.BaseAddress << GPU_TEXTURE_ADDRESS_SHIFT, 
        NULL,  
        0, 
        &DummyTextureHeader, 
        NULL, 
        NULL );

    pSourceTexture->Format = DummyTextureHeader.Format;
}


//---------------------------------------------------------------------------------------------------------
// Name: GPUUntiler::CreateDummyPackedSourceTexture( )
// Desc: Create a texture header which matches the source texture extents and texel size, but formats the 
// data as 16-bit integer.  This format is the simplest way to achieve the goal of a single 'alloc export' 
// bracket per memory transaction in the shader.
//---------------------------------------------------------------------------------------------------------
inline VOID GPUUntiler::SetDummyPackedSourceTexture( IDirect3DTexture9* pSourceTexture )
{
    XGTEXTURE_DESC SourceDesc;
    XGGetTextureDesc( pSourceTexture, 0, &SourceDesc );

    D3DFORMAT D3DFMT_R8_INTEGER = (D3DFORMAT) MAKED3DFMT(
        GPUTEXTUREFORMAT_8, 
        GPUENDIAN_NONE, 
        TRUE, 
        GPUSIGN_ALL_UNSIGNED, 
        GPUNUMFORMAT_INTEGER, 
        GPUSWIZZLE_OOOR);
    D3DFORMAT D3DFMT_G16R16_INTEGER = (D3DFORMAT) MAKED3DFMT(
        GPUTEXTUREFORMAT_16_16, 
        GPUENDIAN_8IN16, 
        TRUE, 
        GPUSIGN_ALL_UNSIGNED, 
        GPUNUMFORMAT_INTEGER, 
        GPUSWIZZLE_ABGR);

    D3DFORMAT AliasedFormat;
    switch( SourceDesc.BytesPerBlock )
    {
    case 1:
        AliasedFormat = D3DFMT_R8_INTEGER;
        break;

    case 2:
        AliasedFormat = D3DFMT_D16;
        break;

    case 4:
        AliasedFormat = D3DFMT_G16R16_INTEGER;
        break;

    default:
        AliasedFormat = SourceDesc.Format;  // for larger sizes, aliasing is unnecessary
        break;
    }

    if( !XGIsTiledFormat( SourceDesc.Format ) )
    {
        AliasedFormat = (D3DFORMAT) MAKELINFMT( AliasedFormat );
    }

    IDirect3DTexture9 DummyTextureHeader;

    XGSetTextureHeader( SourceDesc.Width, 
        SourceDesc.Height, 
        1,
        ( pSourceTexture->Common & D3DCOMMON_CPU_CACHED_MEMORY ) ? D3DUSAGE_CPU_CACHED_MEMORY : 0, 
        AliasedFormat, 
        0, 
        pSourceTexture->Format.BaseAddress << GPU_TEXTURE_ADDRESS_SHIFT, 
        NULL,  
        0, 
        &DummyTextureHeader, 
        NULL, 
        NULL );

    pSourceTexture->Format = DummyTextureHeader.Format;

    // Make sure texels are the same number of bytes as before
    XGTEXTURE_DESC DummyDesc;
    XGGetTextureDesc( &DummyTextureHeader, 0, &DummyDesc );
    assert( DummyDesc.BytesPerBlock == SourceDesc.BytesPerBlock );
}


//---------------------------------------------------------------------------------------------------------
// Name: GPUUntiler::CreateDummyPackedDestTexture( )
// Desc: Create a texture header which matches the source texture extents, but formats the data as 
// 16:16:16:16 integer.  This format is the simplest way to achieve the goal of a single 'alloc export' 
// bracket per memory transaction in the shader.
//---------------------------------------------------------------------------------------------------------
inline VOID GPUUntiler::SetDummyPackedDestTexture( IDirect3DTexture9* pSourceTexture )
{
    XGTEXTURE_DESC SourceDesc;
    XGGetTextureDesc( pSourceTexture, 0, &SourceDesc );

    UINT iTexelsPerShaderInvocation = g_iBytesPerMemoryTransaction / SourceDesc.BytesPerBlock;
    iTexelsPerShaderInvocation = Min( iTexelsPerShaderInvocation, g_iFieldsPerMemoryTransaction );
    assert( g_iBytesPerMemoryTransaction % SourceDesc.BytesPerBlock == 0 );

    UINT iAllocExportsPerShaderInvocation = XGNextMultiple( iTexelsPerShaderInvocation, g_iTexelsPerAllocExport )
        / g_iTexelsPerAllocExport; 

    D3DFORMAT D3DFMT_LIN_A8B8G8R8_INTEGER = (D3DFORMAT) MAKED3DFMT(
        GPUTEXTUREFORMAT_8_8_8_8, 
        GPUENDIAN_NONE, 
        FALSE, 
        GPUSIGN_ALL_UNSIGNED, 
        GPUNUMFORMAT_INTEGER, 
        GPUSWIZZLE_ABGR);
    D3DFORMAT D3DFMT_LIN_A16B16G16R16_INTEGER = (D3DFORMAT) MAKED3DFMT(
        GPUTEXTUREFORMAT_16_16_16_16, 
        GPUENDIAN_8IN16, 
        FALSE, 
        GPUSIGN_ALL_UNSIGNED, 
        GPUNUMFORMAT_INTEGER, 
        GPUSWIZZLE_ABGR);

    D3DFORMAT AliasedFormat;
    switch( SourceDesc.BytesPerBlock )
    {
    case 1:
        AliasedFormat = D3DFMT_LIN_A8B8G8R8_INTEGER;
        break;

    case 2:
    case 4:
        AliasedFormat = D3DFMT_LIN_A16B16G16R16_INTEGER;
        break;

    default:
        AliasedFormat = SourceDesc.Format;  // For larger sizes, aliasing is unnecessary
        break;
    }

    IDirect3DTexture9 DummyTextureHeader;

    XGSetTextureHeader( XGNextMultiple( SourceDesc.Width, iAllocExportsPerShaderInvocation ) / iAllocExportsPerShaderInvocation, 
        SourceDesc.Height, 
        1,
        ( pSourceTexture->Common & D3DCOMMON_CPU_CACHED_MEMORY ) ? D3DUSAGE_CPU_CACHED_MEMORY : 0, 
        AliasedFormat, 
        0, 
        pSourceTexture->Format.BaseAddress << GPU_TEXTURE_ADDRESS_SHIFT, 
        NULL,  
        0, 
        &DummyTextureHeader, 
        NULL, 
        NULL );

    pSourceTexture->Format = DummyTextureHeader.Format;

    // Make sure that the new format supports filling a whole number of 32-byte transactions using only 4 memexports
    XGTEXTURE_DESC DummyDesc;
    XGGetTextureDesc( &DummyTextureHeader, 0, &DummyDesc );
    assert( SourceDesc.BytesPerBlock == 1 
        || ( DummyDesc.BytesPerBlock * g_iTexelsPerAllocExport ) % g_iBytesPerMemoryTransaction == 0 );
}


//---------------------------------------------------------------------------------------------------------
// Name: GPUUntiler::CreateDummyTextureForUntileResolve( )
// Desc: For the purpose of using Resolve to untile, we need to pad the texture dimensions out to 
// the full actual texture size, including padding.  That's because the areas of memory which are  
// 'padding' for linear textures do not match areas of memory which are 'padding' for tiled textures.
//
// One point of this aliasing is particularly to support standard, but non-tile-aligned, texture sizes such 
// as 1280x720, or 320x240.  
//
// Another point is to support texture sizes for which linear pitch differs from tiled pitch (which also 
// happens for 320x240).
//---------------------------------------------------------------------------------------------------------
inline VOID GPUUntiler::SetDummyTextureForUntileResolve( IDirect3DTexture9* pSourceTexture, 
                                                       BOOL bIsResolveTarget, 
                                                       UINT iSourceSize )
{
    XGTEXTURE_DESC SourceDesc;
    XGGetTextureDesc( pSourceTexture, 0, &SourceDesc );

    UINT iSourceBaseAddress = pSourceTexture->Format.BaseAddress << GPU_TEXTURE_ADDRESS_SHIFT;

    // If this isn't true, the aliased texture can't match up properly, at least by our methods
    assert( ( SourceDesc.RowPitch / SourceDesc.BytesPerBlock ) % m_iRepeatBlockWidth == 0 );

    UINT iWidthMultiplier = 1;

    // Remove gamma correction and '_AS_16'
    D3DFORMAT NewFormat = GetNonAs16NonsRGBFormat( SourceDesc.Format );

    // We need a texture format to resolve into which has a corresponding render target format.
    // 
    // To handle 16-bit channels bit-correctly ***without aliasing as another format*** you'd need to 
    // match the conventions of D3DFMT_G16R16_EDRAM, which is a signed format with range [-32,32].  
    // Therefore, you would need to do a couple of messy things:
    // 
    //  1) Convert from unsigned to signed at the end of the shader (see 
    //      ReinterpretCastUnsignedToSigned_16_16_16_16 in the FastBlockCompress sample)
    //  2) Use exponent biases during write-to-render-target and resolve (see VarianceShadowMaps
    //      sample)
    //
    // We take the simpler route here and just alias as another format which already has bit-exact
    // round trips.  That won't work if you need to merge untiling into some existing shader which
    // uses the actual depth values. In that case, you need the complicated method described above.
    switch( (GPUTEXTUREFORMAT) XGGetGpuFormat( NewFormat ) )
    {
    case GPUTEXTUREFORMAT_8:                    // no modification needed
    case GPUTEXTUREFORMAT_8_8:                  // no modification needed
    case GPUTEXTUREFORMAT_8_8_8_8:              // no modification needed
    case GPUTEXTUREFORMAT_32_32_FLOAT:          // no modification needed
        break;

    case GPUTEXTUREFORMAT_16:       
        NewFormat = XGIsTiledFormat( NewFormat ) ? D3DFMT_G8R8 : D3DFMT_LIN_G8R8;
        break;

    case GPUTEXTUREFORMAT_32_32_32_32_FLOAT:  
        if( bIsResolveTarget )
        {
            iWidthMultiplier = 2;
            NewFormat = XGIsTiledFormat( NewFormat ) ? D3DFMT_G32R32F : D3DFMT_LIN_G32R32F;
        }
        break;

    default:
        assert( FALSE );    // Add other cases as needed, but let's not assume they work
        break;
    }

    // Destination for resolve must always be marked as tiled, even if it's actually linear.
    if( bIsResolveTarget )
    {
        NewFormat = GetTiledFormat( NewFormat );
    }

    IDirect3DTexture9 DummyTextureHeader;
    UINT iDummyBaseSize;

    XGSetTextureHeader( SourceDesc.RowPitch / SourceDesc.BytesPerBlock * iWidthMultiplier, 
        XGNextMultiple( SourceDesc.Height, GPU_TEXTURE_TILE_DIMENSION ), 
        1,
        ( pSourceTexture->Common & D3DCOMMON_CPU_CACHED_MEMORY ) ? D3DUSAGE_CPU_CACHED_MEMORY : 0, 
        NewFormat, 
        0, 
        iSourceBaseAddress, 
        NULL,  
        SourceDesc.RowPitch, 
        &DummyTextureHeader, 
        &iDummyBaseSize, 
        NULL );

    pSourceTexture->Format = DummyTextureHeader.Format;

    // If this fails, then the default linear texture allocation is too small to accommodate a tiled texture.
    // We need to ensure that the actual allocation was larger than necessary.  Note that the contents of the
    // padding are irrelevant --- it just has to be valid memory.  And for the dest texture, we have to be
    // able to overwrite it.
    assert( iDummyBaseSize <= iSourceSize );
}

//---------------------------------------------------------------------------------------------------------
// Name: GPUUntiler::SetMemExportStreamConstantFromTexture( )
// Desc: Attempt to build a matching GPU_MEMEXPORT_STREAM_CONSTANT from a given texture.  Not all
// permutations can be matched.
//---------------------------------------------------------------------------------------------------------
inline VOID GPUUntiler::SetMemExportStreamConstantFromTexture( IDirect3DTexture9* pDestTexture, 
                                                          GPU_MEMEXPORT_STREAM_CONSTANT* pMemExportStreamConstant )
{
    XGTEXTURE_DESC DestDesc;
    XGGetTextureDesc( pDestTexture, 0, &DestDesc );

    // Address and max index
    DWORD dwBaseAddress = pDestTexture->Format.BaseAddress << GPU_TEXTURE_ADDRESS_SHIFT;
    UINT iMaxIndex = ( DestDesc.RowPitch / DestDesc.BytesPerBlock ) * DestDesc.Height;

    // SurfaceSwap --- optionally swap blue and red
    // These possibilities are enough for us here, but not exhaustive.
    // If we needed to support other swizzle patterns, we could probably jigger SurfaceSwap and
    // GpuEndian128 to do so.
    GPUSURFACESWAP SurfaceSwap = SURFACESWAP_LOW_RED;
    switch( pDestTexture->Format.SwizzleX )
    {
    case GPUSWIZZLE_X:
        SurfaceSwap = SURFACESWAP_LOW_RED;
        assert( pDestTexture->Format.SwizzleX == GPUSWIZZLE_X );
        assert( pDestTexture->Format.SwizzleY == GPUSWIZZLE_Y 
            || pDestTexture->Format.SwizzleZ == GPUSWIZZLE_0 
            || pDestTexture->Format.SwizzleZ == GPUSWIZZLE_1 );
        assert( pDestTexture->Format.SwizzleZ == GPUSWIZZLE_Z 
            || pDestTexture->Format.SwizzleZ == GPUSWIZZLE_0 
            || pDestTexture->Format.SwizzleZ == GPUSWIZZLE_1 );
        assert( pDestTexture->Format.SwizzleW == GPUSWIZZLE_W 
            || pDestTexture->Format.SwizzleW == GPUSWIZZLE_0 
            || pDestTexture->Format.SwizzleW == GPUSWIZZLE_1 );
        break;

    case GPUSWIZZLE_Z:
        SurfaceSwap = SURFACESWAP_LOW_BLUE;
        assert( pDestTexture->Format.SwizzleX == GPUSWIZZLE_Z );
        assert( pDestTexture->Format.SwizzleY == GPUSWIZZLE_Y 
            || pDestTexture->Format.SwizzleZ == GPUSWIZZLE_0 
            || pDestTexture->Format.SwizzleZ == GPUSWIZZLE_1 );
        assert( pDestTexture->Format.SwizzleZ == GPUSWIZZLE_X 
            || pDestTexture->Format.SwizzleZ == GPUSWIZZLE_0 
            || pDestTexture->Format.SwizzleZ == GPUSWIZZLE_1 );
        assert( pDestTexture->Format.SwizzleW == GPUSWIZZLE_W 
            || pDestTexture->Format.SwizzleW == GPUSWIZZLE_0 
            || pDestTexture->Format.SwizzleW == GPUSWIZZLE_1 );
        break;

    default:
        assert(false);
    }

    // SurfaceNumber --- for fixed-point data, use fraction or integer?  
    // Memexport cannot handle gamma conversion or bias
    GPUSURFACENUMBER SurfaceNumber = GPUSURFACENUMBER_UREPEAT;
    switch( (GPUTEXTUREFORMAT) XGGetGpuFormat( DestDesc.Format ) )
    {
    // Integer/fraction format
    case GPUTEXTUREFORMAT_8:
    case GPUTEXTUREFORMAT_8_8:                  
    case GPUTEXTUREFORMAT_8_8_8_8:              
    case GPUTEXTUREFORMAT_16:       
    case GPUTEXTUREFORMAT_16_16_16_16:       
        switch( pDestTexture->Format.NumFormat )
        {
        case GPUNUMFORMAT_FRACTION:
            switch( pDestTexture->Format.SignX )
            {
            case GPUSIGN_UNSIGNED:
                SurfaceNumber = GPUSURFACENUMBER_UREPEAT;
                break;

            case GPUSIGN_SIGNED:
                SurfaceNumber = GPUSURFACENUMBER_SREPEAT;
                break;

            default:
                assert(false);
            }
            break;

        case GPUNUMFORMAT_INTEGER:
            switch( pDestTexture->Format.SignX )
            {
            case GPUSIGN_UNSIGNED:
                SurfaceNumber = GPUSURFACENUMBER_UINTEGER;
                break;

            case GPUSIGN_SIGNED:
                SurfaceNumber = GPUSURFACENUMBER_SINTEGER;
                break;

            default:
                assert(false);
            }
            break;

        default:
            assert(false);
        }
        break;

    case GPUTEXTUREFORMAT_32_32_FLOAT:
    case GPUTEXTUREFORMAT_32_32_32_32_FLOAT:
        SurfaceNumber = GPUSURFACENUMBER_FLOAT;
        break;

    default:    // Add other cases as encountered
        assert(false);
    };

    // ColorFormat --- these enums match those for GPUTEXTUREFORMAT
    // We could assert here that the format is one of the supported types
    GPUCOLORFORMAT ColorFormat = (GPUCOLORFORMAT) pDestTexture->Format.DataFormat;

    // GpuEndian128 --- these enums match those for GPUENDIAN
    GPUENDIAN128 GpuEndian128 = (GPUENDIAN128) pDestTexture->Format.Endian;

    GPU_SET_MEMEXPORT_STREAM_CONSTANT( pMemExportStreamConstant, 
        ( VOID* ) dwBaseAddress, 
        iMaxIndex, 
        SurfaceSwap, 
        SurfaceNumber, 
        ColorFormat, 
        GpuEndian128 );
}


//---------------------------------------------------------------------------------------------------------
// Name: GPUUntiler::CreateDummyMatchingRenderTarget( )
// Desc: Create a render target which is compatible with a given texture, for purposes of this sample.  
// The main issues are removing gamma and '_AS_16', and expanding formats which are less than 32 bpp.
//---------------------------------------------------------------------------------------------------------
inline VOID GPUUntiler::CreateDummyMatchingRenderTarget( IDirect3DDevice9* pd3dDevice, 
                                                        IDirect3DTexture9* pSourceTexture, 
                                                        IDirect3DSurface9** ppDummyRenderTarget )
{
    XGTEXTURE_DESC SourceDesc;
    XGGetTextureDesc( pSourceTexture, 0, &SourceDesc );

    // Find a matching render target format --- but render targets must be at least 32 bpp and at most 64 bpp
    D3DFORMAT RenderTargetFormat = GetNonAs16NonsRGBFormat( SourceDesc.Format );
    switch( (GPUTEXTUREFORMAT) XGGetGpuFormat( RenderTargetFormat ) )
    {
    case GPUTEXTUREFORMAT_32_32_FLOAT:          // no modification needed
    case GPUTEXTUREFORMAT_8_8_8_8:              // no modification needed
        break;

    case GPUTEXTUREFORMAT_8_8:
    case GPUTEXTUREFORMAT_8:
        ReplaceGpuFormat( RenderTargetFormat, GPUTEXTUREFORMAT_8_8_8_8 );
        break;

    case GPUTEXTUREFORMAT_16:
        ReplaceGpuFormat( RenderTargetFormat, GPUTEXTUREFORMAT_16_16_EDRAM );
        break;

    case GPUTEXTUREFORMAT_32_32_32_32_FLOAT:
        ReplaceGpuFormat( RenderTargetFormat, GPUTEXTUREFORMAT_32_32_FLOAT );
        break;

    default:
        assert( FALSE );    // Add other cases as needed
        break;
    }

    pd3dDevice->CreateRenderTarget( SourceDesc.Width, 
        SourceDesc.Height, 
        GetTiledFormat( RenderTargetFormat ), 
        D3DMULTISAMPLE_NONE,
        0, 
        FALSE, 
        ppDummyRenderTarget, 
        NULL );
}


//---------------------------------------------------------------------------------------------------------
// Name: GPUUntiler::CreateDummyMemexportTransactionRenderTarget( )
// Desc: Same as above, but make the render target narrower by a factor equal to the number of texels in 
// a GPU memory transaction.  That's also the number of texels we memexport in one invocation of the 
// shader.
//---------------------------------------------------------------------------------------------------------
inline VOID GPUUntiler::CreateDummyMemexportTransactionRenderTarget( IDirect3DDevice9* pd3dDevice, 
                                                                    IDirect3DTexture9* pSourceTexture, 
                                                                    IDirect3DSurface9** ppDummyRenderTarget)
{
    XGTEXTURE_DESC SourceDesc;
    XGGetTextureDesc( pSourceTexture, 0, &SourceDesc );

    UINT iTexelsPerShaderInvocation = g_iBytesPerMemoryTransaction / SourceDesc.BytesPerBlock;
    assert( g_iBytesPerMemoryTransaction % SourceDesc.BytesPerBlock == 0 );

    D3DFORMAT RenderTargetFormat = GetNonAs16NonsRGBFormat( SourceDesc.Format );
    switch( (GPUTEXTUREFORMAT) XGGetGpuFormat( RenderTargetFormat ) )
    {
    case GPUTEXTUREFORMAT_32_32_FLOAT:          // no modification needed
    case GPUTEXTUREFORMAT_8_8_8_8:              // no modification needed
        break;

    case GPUTEXTUREFORMAT_8:
    case GPUTEXTUREFORMAT_8_8:
        ReplaceGpuFormat( RenderTargetFormat, GPUTEXTUREFORMAT_8_8_8_8 );
        break;

    case GPUTEXTUREFORMAT_16:
        ReplaceGpuFormat( RenderTargetFormat, GPUTEXTUREFORMAT_16_16_EDRAM );
        break;

    case GPUTEXTUREFORMAT_32_32_32_32_FLOAT:
        ReplaceGpuFormat( RenderTargetFormat, GPUTEXTUREFORMAT_32_32_FLOAT );
        break;

    default:
        assert( FALSE );    // Add other cases as needed
        break;
    }

    pd3dDevice->CreateRenderTarget( XGNextMultiple( SourceDesc.Width, iTexelsPerShaderInvocation ) / iTexelsPerShaderInvocation, 
        SourceDesc.Height, 
        GetNonAs16NonsRGBFormat( RenderTargetFormat ), 
        D3DMULTISAMPLE_NONE,
        0, 
        FALSE, 
        ppDummyRenderTarget, 
        NULL );
}


//---------------------------------------------------------------------------------------------------------
// Name: GPUUntiler::CreateDummyResolveRenderTarget( )
// Desc: Create a render target which is compatible with a given texture, for purposes of this sample.  
// The main issues are removing gamma and '_AS_16', and expanding formats which are less than 32 bpp.
//---------------------------------------------------------------------------------------------------------
inline VOID GPUUntiler::CreateDummyResolveRenderTarget( IDirect3DDevice9* pd3dDevice, 
                                                        IDirect3DTexture9* pSourceTexture, 
                                                        IDirect3DSurface9** ppDummyRenderTarget )
{
    XGTEXTURE_DESC SourceDesc;
    XGGetTextureDesc( pSourceTexture, 0, &SourceDesc );

    UINT iWidthMultiplier = 1;

    // Find a matching render target format --- but render targets must be at least 32 bpp and at most 64 bpp
    D3DFORMAT RenderTargetFormat = GetNonAs16NonsRGBFormat( SourceDesc.Format );
    switch( (GPUTEXTUREFORMAT) XGGetGpuFormat( RenderTargetFormat ) )
    {
    case GPUTEXTUREFORMAT_32_32_FLOAT:          // no modification needed
    case GPUTEXTUREFORMAT_8_8_8_8:              // no modification needed
        break;

    case GPUTEXTUREFORMAT_8_8:
    case GPUTEXTUREFORMAT_8:
        ReplaceGpuFormat( RenderTargetFormat, GPUTEXTUREFORMAT_8_8_8_8 );
        break;

    case GPUTEXTUREFORMAT_16:
        ReplaceGpuFormat( RenderTargetFormat, GPUTEXTUREFORMAT_16_16_EDRAM );
        break;

    case GPUTEXTUREFORMAT_32_32_32_32_FLOAT:
        ReplaceGpuFormat( RenderTargetFormat, GPUTEXTUREFORMAT_32_32_FLOAT );
        iWidthMultiplier = 2;
        break;

    default:
        assert( FALSE );    // Add other cases as needed
        break;
    }

    pd3dDevice->CreateRenderTarget( SourceDesc.Width * iWidthMultiplier, 
        SourceDesc.Height, 
        GetTiledFormat( RenderTargetFormat ), 
        D3DMULTISAMPLE_NONE,
        0, 
        FALSE, 
        ppDummyRenderTarget, 
        NULL );
}


//---------------------------------------------------------------------------------------------------------
// Name: GPUUntiler::UntileMemexport( )
// Desc: Untile using memexport on the GPU, in one of 3 flavors.
//---------------------------------------------------------------------------------------------------------
VOID GPUUntiler::UntileMemexport( IDirect3DDevice9* pd3dDevice, 
                                 IDirect3DTexture9* pSourceTexture, 
                                 IDirect3DTexture9* pDestTexture, 
                                 UINT iUntileMethod )
{
#ifdef FAST_UNTILE_ADJUST_GPR_ALLOCATION
    // Bias max GPRs to pixel shader
    //-------------------------------------------------------------------------------------------------
    // WARNING:  This is one source of significant GPU overhead
    //-------------------------------------------------------------------------------------------------
    DWORD iOldGPRAllocationVS, iOldGPRAllocationPS, dwFlags;
    pd3dDevice->GetShaderGPRAllocation( &dwFlags, &iOldGPRAllocationVS, &iOldGPRAllocationPS );
    pd3dDevice->SetShaderGPRAllocation( 0, 16, GPU_GPRS - 16 );
#endif

    // Replace source by its non-AS_16, non-sRGB alias.  The aliasing is necessary because:
    //  1) Memexport can't convert back to gamma space, so we don't want to convert to linear space.
    //  2) In light of the above reason, no point in wasting performance by using _AS_16
    //  3) LockRect won't work unless called on the exact same texture header (not an alias)
    SetDummyNonAs16NonsRGBTexture( pSourceTexture );

    // Replace dest by its tiled, non-AS_16, non-sRGB alias.  The aliasing is necessary because
    // CreateDummyMatchingRenderTarget will assert on gamma-enabled, or '_AS_16' surfaces
    SetDummyNonAs16NonsRGBTexture( pDestTexture );

    XGTEXTURE_DESC SourceDesc;
    XGGetTextureDesc( pSourceTexture, 0, &SourceDesc );
    XGTEXTURE_DESC DestDesc;
    XGGetTextureDesc( pDestTexture, 0, &DestDesc );

    // This handles certain cases where linear and tiled pitch don't match
    UINT iEffectivePitch = XGNextMultiple( DestDesc.RowPitch / DestDesc.BytesPerBlock, GPU_TEXTURE_TILE_DIMENSION );

    // Create a render target matching the source format
    IDirect3DSurface9* pRenderTarget;
    CreateDummyMatchingRenderTarget( pd3dDevice, pSourceTexture, &pRenderTarget );

    IDirect3DSurface9* pMemexportTransactionRenderTarget;
    CreateDummyMemexportTransactionRenderTarget( pd3dDevice, pSourceTexture, &pMemexportTransactionRenderTarget );

    switch( iUntileMethod )
    {
    case UNTILE_METHOD_MEMEXPORT_TEXEL:
        {
            pd3dDevice->SetPixelShader( m_pUntileMemexportTexelPS );
            pd3dDevice->SetRenderTarget( 0, pRenderTarget );

            // Avoid having to add 0.5f in shader
            pd3dDevice->SetRenderState( D3DRS_HALFPIXELOFFSET, TRUE );
        }
        break;

    case UNTILE_METHOD_MEMEXPORT_TRANSACTION:
        {
            switch( SourceDesc.BitsPerPixel )
            {
            case 128:
                pd3dDevice->SetPixelShader( m_pUntileMemexportTransaction128bppPS );
                break;
            case 64:
                pd3dDevice->SetPixelShader( m_pUntileMemexportTransaction64bppPS );
                break;
            case 32:
                pd3dDevice->SetPixelShader( m_pUntileMemexportTransaction32bppPS );
                break;
            case 16:
                pd3dDevice->SetPixelShader( m_pUntileMemexportTransaction16bppPS );
                break;
            case 8:
                pd3dDevice->SetPixelShader( m_pUntileMemexportTransaction8bppPS );
                break;
            default:
                assert( FALSE );    // not yet implemented
                break;
            }
            pd3dDevice->SetRenderTarget( 0, pMemexportTransactionRenderTarget );
            pd3dDevice->SetRenderState( D3DRS_HALFPIXELOFFSET, FALSE );
        }
        break;

    case UNTILE_METHOD_MEMEXPORT_PACKED:
        {
            switch( SourceDesc.BitsPerPixel )
            {
            case 128:   // Does not require a separate shader for Packed
                pd3dDevice->SetPixelShader( m_pUntileMemexportTransaction128bppPS );
                break;
            case 64:   // Does not require a separate shader for Packed
                pd3dDevice->SetPixelShader( m_pUntileMemexportTransaction64bppPS );
                break;
            case 32:
                pd3dDevice->SetPixelShader( m_pUntileMemexportPacked32bppPS );
                break;
            case 16:
                pd3dDevice->SetPixelShader( m_pUntileMemexportPacked16bppPS );
                break;
            case 8:
                pd3dDevice->SetPixelShader( m_pUntileMemexportPacked8bppPS );
                break;
            default:
                assert( FALSE );    // not yet implemented
                break;
            }
            pd3dDevice->SetRenderTarget( 0, pMemexportTransactionRenderTarget );
            pd3dDevice->SetRenderState( D3DRS_HALFPIXELOFFSET, FALSE );

            // Replace source and dest textures with a fixed format, covering
            // the same raw data, but exportable in a single memory transaction
            // per 'alloc export' bracket
            SetDummyPackedSourceTexture( pSourceTexture );
            XGGetTextureDesc( pSourceTexture, 0, &SourceDesc );

            SetDummyPackedDestTexture( pDestTexture );
            XGGetTextureDesc( pDestTexture, 0, &DestDesc );
        }
        break;

    default:
        assert( FALSE );
        break;
    }

    //-------------------------------------------------------------------------------------------------
    // WARNING:  This is one source of significant GPU overhead
    //-------------------------------------------------------------------------------------------------
    // Must do this after all texture aliasing above, so that we use matching texture for
    // BeginExport & EndExport & actual memexport
    pd3dDevice->BeginExport( 0, pDestTexture, D3DBEGINEXPORT_PIXELSHADER );

    XMVECTOR vTexDims = {
        (FLOAT) SourceDesc.Width, 
        (FLOAT) SourceDesc.Height, 
        (FLOAT) iEffectivePitch,  
    };
    pd3dDevice->SetPixelShaderConstantF( 0, (FLOAT*) &vTexDims, 1 );

    GPU_MEMEXPORT_STREAM_CONSTANT MemExportStreamConstant;
    SetMemExportStreamConstantFromTexture( pDestTexture, &MemExportStreamConstant );
    pd3dDevice->SetPixelShaderConstantF( 1, ( FLOAT* ) &MemExportStreamConstant, 1 );

    // Avoid overwriting contents of EDRAM.  Can't just set Render Target to NULL,
    // because then the GPU doesn't know the dimensions to use in rasterization.
    DWORD dwOldColorWriteEnable;
    pd3dDevice->GetRenderState( D3DRS_COLORWRITEENABLE, &dwOldColorWriteEnable );
    pd3dDevice->SetRenderState( D3DRS_COLORWRITEENABLE, 0 );

    pd3dDevice->SetVertexDeclaration( m_pVertexDecl );
    pd3dDevice->SetVertexShader( m_pCopyTextureVS );

    pd3dDevice->SetTexture( 0, pSourceTexture );
    pd3dDevice->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_POINT );
    pd3dDevice->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_POINT );

    pd3dDevice->DrawPrimitiveUP( D3DPT_RECTLIST, 1, ScreenspaceRectangleVerts, sizeof( ScreenspaceVertex ) );

    pd3dDevice->EndExport( 0, pDestTexture, 0 );

    // Unset texture, before the header potentially disappears off the stack
    pd3dDevice->SetTexture( 0, NULL );

    // Restore settings
#ifdef FAST_UNTILE_ADJUST_GPR_ALLOCATION
    pd3dDevice->SetShaderGPRAllocation( dwFlags, iOldGPRAllocationVS, iOldGPRAllocationPS );
#endif
    pd3dDevice->SetRenderState( D3DRS_COLORWRITEENABLE, dwOldColorWriteEnable );
    pd3dDevice->SetRenderTarget( 0, NULL );
    pRenderTarget->Release();
    pMemexportTransactionRenderTarget->Release();
}


//---------------------------------------------------------------------------------------------------------
// Name: GPUUntiler::UntileResolve( )
// Desc: Untile using Resolve on the GPU.  Resolve actually performs tiling, but by remapping texcoords 
// we can force it to effectively perform untiling instead.
//---------------------------------------------------------------------------------------------------------
VOID GPUUntiler::UntileResolve( IDirect3DDevice9* pd3dDevice, 
                               IDirect3DTexture9* pSourceTexture, 
                               IDirect3DTexture9* pDestTexture, 
                               UINT iSourceSize, 
                               UINT iDestSize )
{
#ifdef FAST_UNTILE_ADJUST_GPR_ALLOCATION
    // Bias max GPRs to pixel shader
    //-------------------------------------------------------------------------------------------------
    // WARNING:  This is one source of significant GPU overhead
    //-------------------------------------------------------------------------------------------------
    DWORD iOldGPRAllocationVS, iOldGPRAllocationPS, dwFlags;
    pd3dDevice->GetShaderGPRAllocation( &dwFlags, &iOldGPRAllocationVS, &iOldGPRAllocationPS );
    pd3dDevice->SetShaderGPRAllocation( 0, 16, GPU_GPRS - 16 );
#endif

    // Both source and destination textures, and the render target must nominally have their 
    // dimensions padded out to the tile size.  Linear texels near the right and bottom edge may 
    // actually correspond to pixels past those edges in the render target.  To fetch these texels 
    // properly, the GPU must believe the texture dimensions correspond to the render target dimensions.
    //
    // For our calculation, the textures need to be further padded to the repeat block size.
    //
    // Also, in the case where the linear pitch is larger than the tiled pitch, we need to pad to the
    // the linear pitch.
    SetDummyTextureForUntileResolve( pSourceTexture, FALSE, iSourceSize );
    SetDummyTextureForUntileResolve( pDestTexture, TRUE, iDestSize );

    XGTEXTURE_DESC SourceDesc;
    XGGetTextureDesc( pSourceTexture, 0, &SourceDesc );
    XGTEXTURE_DESC DestDesc;
    XGGetTextureDesc( pDestTexture, 0, &DestDesc );

    // Create a render target matching the source format
    IDirect3DSurface9* pRenderTarget;
    CreateDummyResolveRenderTarget( pd3dDevice, pDestTexture, &pRenderTarget );

    XMVECTOR vTexDims = {
        (FLOAT) DestDesc.Width, 
        (FLOAT) DestDesc.Height, 
        (FLOAT) XGNextMultiple( DestDesc.RowPitch / DestDesc.BytesPerBlock, GPU_TEXTURE_TILE_DIMENSION ),  
    };
    pd3dDevice->SetPixelShaderConstantF( 0, (FLOAT*) &vTexDims, 1 );

    XMVECTOR vTexDimsInRepeatBlocks = {
        (FLOAT) XGNextMultiple( DestDesc.Width, m_iRepeatBlockWidth ) / m_iRepeatBlockWidth, 
        (FLOAT) XGNextMultiple( DestDesc.Height, m_iRepeatBlockHeight ) / m_iRepeatBlockHeight, 
        (FLOAT) XGNextMultiple( DestDesc.RowPitch / DestDesc.BytesPerBlock, m_iRepeatBlockWidth ) / m_iRepeatBlockWidth,  
        (FLOAT) m_iRepeatBlockWidth, 
    };
    pd3dDevice->SetPixelShaderConstantF( 1, (FLOAT*) &vTexDimsInRepeatBlocks, 1 );

    pd3dDevice->SetVertexDeclaration( m_pVertexDecl );
    pd3dDevice->SetVertexShader( m_pCopyTextureVS );

    switch( SourceDesc.BitsPerPixel )
    {
    case 128:
        pd3dDevice->SetPixelShader( m_pUntileResolve128bppPS );
        break;
    case 64:
        pd3dDevice->SetPixelShader( m_pUntileResolve64bppPS );
        break;
    case 32:
        pd3dDevice->SetPixelShader( m_pUntileResolve32bppPS );
        break;
    case 16:
        pd3dDevice->SetPixelShader( m_pUntileResolve16bppPS );
        break;
    case 8:
        pd3dDevice->SetPixelShader( m_pUntileResolve8bppPS );
        break;
    default:
        assert( FALSE );    // not yet implemented
        break;
    }

    pd3dDevice->SetTexture( 0, pSourceTexture );
    pd3dDevice->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_POINT );
    pd3dDevice->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_POINT );
    pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP );
    pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP );

    pd3dDevice->SetTexture( 1, m_pLinearToTiled2DAddressTexture );
    pd3dDevice->SetSamplerState( 1, D3DSAMP_MINFILTER, D3DTEXF_POINT );
    pd3dDevice->SetSamplerState( 1, D3DSAMP_MAGFILTER, D3DTEXF_POINT );
    pd3dDevice->SetSamplerState( 1, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP );
    pd3dDevice->SetSamplerState( 1, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP );

    pd3dDevice->SetRenderTarget( 0, pRenderTarget );

    pd3dDevice->SetRenderState( D3DRS_HALFPIXELOFFSET, TRUE );

    pd3dDevice->DrawPrimitiveUP( D3DPT_RECTLIST, 1, ScreenspaceRectangleVerts, sizeof( ScreenspaceVertex ) );

    pd3dDevice->Resolve( D3DRESOLVE_RENDERTARGET0, NULL, pDestTexture, NULL, 0, 0, NULL, 1.0f, 0L, NULL );

    // Unset texture, before the header potentially disappears off the stack
    pd3dDevice->SetTexture( 0, NULL );

    // Restore settings
#ifdef FAST_UNTILE_ADJUST_GPR_ALLOCATION
    pd3dDevice->SetShaderGPRAllocation( dwFlags, iOldGPRAllocationVS, iOldGPRAllocationPS );
#endif
    pd3dDevice->SetRenderTarget( 0, NULL );
    pRenderTarget->Release();
}



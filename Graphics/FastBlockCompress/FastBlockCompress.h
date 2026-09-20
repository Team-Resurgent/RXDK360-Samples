//---------------------------------------------------------------------------------------------------------
// FastBlockCompress.h
//
// XNA Developer Connection
// Copyright ( C ) Microsoft Corporation. All rights reserved.
//---------------------------------------------------------------------------------------------------------
#include <xgraphics.h>

//---------------------------------------------------------------------------------------------------------
// High-precision to standard-precision texture format mapping table. Maps GPU high-precision sampling format
// (i.e. _AS_16_16_16_16 etc.) to equivalent standard formats. Used to create linear formats for textures from 
// sRGB formats.  Any entry that has no mapping just maps to the same format value.
//---------------------------------------------------------------------------------------------------------


//---------------------------------------------------------------------------------------------------------
// Name: GetNonAs16NonsRGBFormat( )
// Desc: Convert sRGB format to linear format, stripping out '_AS_16'.  This function allows us to 
// override gamma-conversion settings during compression (and during RMS error calculation).  
// Compression error should be measured in gamma space, rather than in linear light space, because
// color error is a subjective, perceptual notion, not a physical notion.
//---------------------------------------------------------------------------------------------------------
__forceinline
D3DFORMAT GetNonAs16NonsRGBFormat( D3DFORMAT fmtBase )
{
    const static DWORD g_MapSrgbToLinearGpuFormat[] = 
    {
        GPUTEXTUREFORMAT_1_REVERSE,
        GPUTEXTUREFORMAT_1,
        GPUTEXTUREFORMAT_8,
        GPUTEXTUREFORMAT_1_5_5_5,
        GPUTEXTUREFORMAT_5_6_5,
        GPUTEXTUREFORMAT_6_5_5,
        GPUTEXTUREFORMAT_8_8_8_8,
        GPUTEXTUREFORMAT_2_10_10_10,
        GPUTEXTUREFORMAT_8_A,
        GPUTEXTUREFORMAT_8_B,
        GPUTEXTUREFORMAT_8_8,
        GPUTEXTUREFORMAT_Cr_Y1_Cb_Y0_REP,     
        GPUTEXTUREFORMAT_Y1_Cr_Y0_Cb_REP,      
        GPUTEXTUREFORMAT_16_16_EDRAM,          
        GPUTEXTUREFORMAT_8_8_8_8_A,
        GPUTEXTUREFORMAT_4_4_4_4,
        GPUTEXTUREFORMAT_10_11_11,
        GPUTEXTUREFORMAT_11_11_10,
        GPUTEXTUREFORMAT_DXT1,
        GPUTEXTUREFORMAT_DXT2_3,  
        GPUTEXTUREFORMAT_DXT4_5,
        GPUTEXTUREFORMAT_16_16_16_16_EDRAM,
        GPUTEXTUREFORMAT_24_8,
        GPUTEXTUREFORMAT_24_8_FLOAT,
        GPUTEXTUREFORMAT_16,
        GPUTEXTUREFORMAT_16_16,
        GPUTEXTUREFORMAT_16_16_16_16,
        GPUTEXTUREFORMAT_16_EXPAND,
        GPUTEXTUREFORMAT_16_16_EXPAND,
        GPUTEXTUREFORMAT_16_16_16_16_EXPAND,
        GPUTEXTUREFORMAT_16_FLOAT,
        GPUTEXTUREFORMAT_16_16_FLOAT,
        GPUTEXTUREFORMAT_16_16_16_16_FLOAT,
        GPUTEXTUREFORMAT_32,
        GPUTEXTUREFORMAT_32_32,
        GPUTEXTUREFORMAT_32_32_32_32,
        GPUTEXTUREFORMAT_32_FLOAT,
        GPUTEXTUREFORMAT_32_32_FLOAT,
        GPUTEXTUREFORMAT_32_32_32_32_FLOAT,
        GPUTEXTUREFORMAT_32_AS_8,
        GPUTEXTUREFORMAT_32_AS_8_8,
        GPUTEXTUREFORMAT_16_MPEG,
        GPUTEXTUREFORMAT_16_16_MPEG,
        GPUTEXTUREFORMAT_8_INTERLACED,
        GPUTEXTUREFORMAT_32_AS_8_INTERLACED,
        GPUTEXTUREFORMAT_32_AS_8_8_INTERLACED,
        GPUTEXTUREFORMAT_16_INTERLACED,
        GPUTEXTUREFORMAT_16_MPEG_INTERLACED,
        GPUTEXTUREFORMAT_16_16_MPEG_INTERLACED,
        GPUTEXTUREFORMAT_DXN,
        GPUTEXTUREFORMAT_8_8_8_8, //_AS_16_16_16_16,
        GPUTEXTUREFORMAT_DXT1, //_AS_16_16_16_16,
        GPUTEXTUREFORMAT_DXT2_3, //_AS_16_16_16_16,
        GPUTEXTUREFORMAT_DXT4_5, //_AS_16_16_16_16,
        GPUTEXTUREFORMAT_2_10_10_10, //_AS_16_16_16_16,
        GPUTEXTUREFORMAT_10_11_11, //_AS_16_16_16_16,
        GPUTEXTUREFORMAT_11_11_10, //_AS_16_16_16_16,
        GPUTEXTUREFORMAT_32_32_32_FLOAT,
        GPUTEXTUREFORMAT_DXT3A,
        GPUTEXTUREFORMAT_DXT5A,
        GPUTEXTUREFORMAT_CTX1,
        GPUTEXTUREFORMAT_DXT3A_AS_1_1_1_1,
        GPUTEXTUREFORMAT_8_8_8_8_GAMMA_EDRAM,
        GPUTEXTUREFORMAT_2_10_10_10_FLOAT_EDRAM,
    };

    DWORD fmtWithoutGpuFormatOrSign = 
        fmtBase & ~( D3DFORMAT_TEXTUREFORMAT_MASK | D3DFORMAT_SIGNX_MASK | D3DFORMAT_SIGNY_MASK | D3DFORMAT_SIGNZ_MASK | D3DFORMAT_SIGNW_MASK );

    GPUTEXTUREFORMAT gpuTextureFormat = ( GPUTEXTUREFORMAT ) 
        ( ( fmtBase & D3DFORMAT_TEXTUREFORMAT_MASK ) >> D3DFORMAT_TEXTUREFORMAT_SHIFT );

    GPUSIGN GpuSignX = ( GPUSIGN ) ( ( fmtBase & D3DFORMAT_SIGNX_MASK ) >> D3DFORMAT_SIGNX_SHIFT );
    GPUSIGN GpuSignY = ( GPUSIGN ) ( ( fmtBase & D3DFORMAT_SIGNY_MASK ) >> D3DFORMAT_SIGNY_SHIFT );
    GPUSIGN GpuSignZ = ( GPUSIGN ) ( ( fmtBase & D3DFORMAT_SIGNZ_MASK ) >> D3DFORMAT_SIGNZ_SHIFT );
    GPUSIGN GpuSignW = ( GPUSIGN ) ( ( fmtBase & D3DFORMAT_SIGNW_MASK ) >> D3DFORMAT_SIGNW_SHIFT );

    if( GpuSignX == GPUSIGN_GAMMA ) GpuSignX = GPUSIGN_UNSIGNED;
    if( GpuSignY == GPUSIGN_GAMMA ) GpuSignY = GPUSIGN_UNSIGNED;
    if( GpuSignZ == GPUSIGN_GAMMA ) GpuSignZ = GPUSIGN_UNSIGNED;
    if( GpuSignW == GPUSIGN_GAMMA ) GpuSignW = GPUSIGN_UNSIGNED;

    return ( D3DFORMAT )(
        fmtWithoutGpuFormatOrSign |
        ( g_MapSrgbToLinearGpuFormat[gpuTextureFormat] << D3DFORMAT_TEXTUREFORMAT_SHIFT ) |
        ( GpuSignX << D3DFORMAT_SIGNX_SHIFT ) |
        ( GpuSignY << D3DFORMAT_SIGNY_SHIFT ) |
        ( GpuSignZ << D3DFORMAT_SIGNZ_SHIFT ) |
        ( GpuSignW << D3DFORMAT_SIGNW_SHIFT )
        );
}


//---------------------------------------------------------------------------------------------------------
// Convenient texture descriptor
//---------------------------------------------------------------------------------------------------------
struct TextureDescAndBaseAddress
{
    XGTEXTURE_DESC Desc;
    UINT BaseAddress;
};


//---------------------------------------------------------------------------------------------------------
// Struct for vertex of test geometry
//---------------------------------------------------------------------------------------------------------
struct TestGeometryVertex
{
    XMFLOAT3 Position;
    XMFLOAT2 TexCoord;
};


//---------------------------------------------------------------------------------------------------------
// Supported compression types  
//---------------------------------------------------------------------------------------------------------
enum COMPRESSED_TYPES
{
    COMPRESSED_TYPE_DXT1, 
    COMPRESSED_TYPE_DXT5, 
    COMPRESSED_TYPE_CTX1, 
    COMPRESSED_TYPE_DXN, 

    COMPRESSED_TYPE_COUNT
};

__declspec(selectany) 
const WCHAR* g_strCompressedTypeNames[] = 
{
    L"DXT1", 
    L"DXT5", 
    L"CTX1", 
    L"DXN", 
};
C_ASSERT( _countof( g_strCompressedTypeNames ) == COMPRESSED_TYPE_COUNT );


//---------------------------------------------------------------------------------------------------------
// Two approaches to VMX SIMD calculation
//---------------------------------------------------------------------------------------------------------
enum SIMD_FORMAT
{
    SIMD_FORMAT_4_FLOAT, 
    SIMD_FORMAT_16_BYTE, 

    SIMD_FORMAT_COUNT
};


//---------------------------------------------------------------------------------------------------------
// Definitions for the various UI selections.
//
// Each block consists of an enum and a corresponding list of UI strings.
//---------------------------------------------------------------------------------------------------------

//---------------------------------------------------------------------------------------------------------
// enum COMPRESSION_METHODS:
// 
// Different compression algorithms.
//---------------------------------------------------------------------------------------------------------
enum COMPRESSION_METHODS
{
    COMPRESSION_METHOD_XGCOMPRESS, 
    COMPRESSION_METHOD_VMX_FLOAT, 
    COMPRESSION_METHOD_VMX_BYTE, 
    COMPRESSION_METHOD_GPU, 

    COMPRESSION_METHOD_COUNT
};

__declspec(selectany) 
const WCHAR* g_strCompressionMethodNames[] = 
{
    L"XGCompressSurface ( xgraphics )", 
    L"VMX ( float math )", 
    L"VMX ( integer byte math )", 
    L"GPU", 
};
C_ASSERT( _countof( g_strCompressionMethodNames ) == COMPRESSION_METHOD_COUNT );


//---------------------------------------------------------------------------------------------------------
// enum TEST_TEXTURES:
// 
// Different source textures.
//---------------------------------------------------------------------------------------------------------
enum TEST_TEXTURES
{
    TEST_TEXTURE_PTC_TESTIMAGE, 
    TEST_TEXTURE_GRASSBLADES, 
    TEST_TEXTURE_GEARBUMP, 

    TEST_TEXTURE_COUNT
};

__declspec(selectany) 
const WCHAR* g_strTestTextureNames[] = 
{
    L"PTC_TestImage", 
    L"GrassBlades", 
    L"GearBump", 
};
C_ASSERT( _countof( g_strTestTextureNames ) == TEST_TEXTURE_COUNT );


//---------------------------------------------------------------------------------------------------------
// enum FILTER_TYPES:
// 
// UI control for filtering option.  Currently just point or bilinear.
//---------------------------------------------------------------------------------------------------------
enum FILTER_TYPES
{
    FILTER_TYPE_POINT, 
    FILTER_TYPE_BILINEAR, 

    FILTER_TYPE_COUNT
};

__declspec(selectany) 
const WCHAR* g_strFilterTypeNames[] = 
{
    L"Point", 
    L"Bilinear", 
};
C_ASSERT( _countof( g_strFilterTypeNames ) == FILTER_TYPE_COUNT );


//---------------------------------------------------------------------------------------------------------
// enum NORMAL_FORMAT_TYPES:
// 
// UI control for normal map format, since we cannot infer from source format
// whether to use DXN or CTX1.
//---------------------------------------------------------------------------------------------------------
enum NORMAL_FORMATS
{
    NORMAL_FORMAT_DXN, 
    NORMAL_FORMAT_CTX1, 

    NORMAL_FORMAT_COUNT
};

__declspec(selectany) 
const WCHAR* g_strNormalFormatNames[] = 
{
    L"DXN", 
    L"CTX1", 
};
C_ASSERT( _countof( g_strNormalFormatNames ) == NORMAL_FORMAT_COUNT );


//---------------------------------------------------------------------------------------------------------
// enum GPU_REPEAT_COUNTS:
// 
// UI control for repeating GPU work several times, to simulate amortizing the 
// overhead cost across several textures.
//---------------------------------------------------------------------------------------------------------
enum GPU_REPEATS
{
    GPU_REPEAT_1X, 
    GPU_REPEAT_10X, 

    GPU_REPEAT_COUNT
};

__declspec(selectany) 
const WCHAR* g_strGpuRepeatNames[] = 
{
    L"1x", 
    L"10x", 
};
C_ASSERT( _countof( g_strGpuRepeatNames ) == GPU_REPEAT_COUNT );


//---------------------------------------------------------------------------------------------------------
// enum TILING_METHODS:
// 
// UI control for tiling method
//---------------------------------------------------------------------------------------------------------
enum TILING_METHODS
{
    TILING_METHOD_XGTILE, 
    TILING_METHOD_GPU_RESOLVE, 
    TILING_METHOD_GPU_MEMEXPORT, 

    TILING_METHOD_COUNT
};

__declspec(selectany) 
const WCHAR* g_strTilingMethodNames[] = 
{
    L"XGTileSurface (xgraphics)", 
    L"GPU Resolve(s)", 
    L"GPU Memexport", 
};
C_ASSERT( _countof( g_strTilingMethodNames ) == TILING_METHOD_COUNT );



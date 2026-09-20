//---------------------------------------------------------------------------------------------------------
// FastUntile.h
//
// XNA Developer Connection
// Copyright ( C ) Microsoft Corporation. All rights reserved.
//---------------------------------------------------------------------------------------------------------


#pragma once


#include <assert.h>

#include <xtl.h>
#include <xgraphics.h>

#include <AtgUtil.h>

//---------------------------------------------------------------------------------------------------------
// Struct for vertex of test geometry
//---------------------------------------------------------------------------------------------------------
struct ScreenspaceVertex
{
    XMFLOAT2 Position;
    XMFLOAT2 TexCoord;
};

__declspec(selectany) 
extern const ScreenspaceVertex ScreenspaceRectangleVerts[] = 
{
    { XMFLOAT2( -1.0f, -1.0f ), XMFLOAT2( 0.0f, 1.0f ), }, 
    { XMFLOAT2(  1.0f, -1.0f ), XMFLOAT2( 1.0f, 1.0f ), }, 
    { XMFLOAT2( -1.0f,  1.0f ), XMFLOAT2( 0.0f, 0.0f ), }, 
    //{ XMFLOAT2(  1.0f,  1.0f ), XMFLOAT2( 1.0f, 0.0f ), },    // Needed for QUADLIST, but not RECTLIST
};


//---------------------------------------------------------------------------------------------------------
// enum UNTILE_METHODS:
// 
// Different methods of untiling demonstrated by the sample.
//---------------------------------------------------------------------------------------------------------
enum UNTILE_METHODS
{
    UNTILE_METHOD_XGUNTILESURFACE, 
    UNTILE_METHOD_CPU_OPTIMIZED, 
    UNTILE_METHOD_MEMEXPORT_TEXEL, 
    UNTILE_METHOD_MEMEXPORT_TRANSACTION, 
    UNTILE_METHOD_MEMEXPORT_PACKED, 
    UNTILE_METHOD_RESOLVE, 

    UNTILE_METHOD_COUNT
};

__declspec(selectany) 
const CHAR* g_strUntileMethodPixNames[] = 
{
    "XGUntileSurface", 
    "CPU optimized", 
    "Memexport by texel", 
    "Memexport by transaction", 
    "Memexport by packed transaction",
    "Remapped Resolve", 
};
C_ASSERT( _countof( g_strUntileMethodPixNames ) == UNTILE_METHOD_COUNT );

__declspec(selectany) 
const WCHAR* g_strUntileMethodNames[] = 
{
    L"XGUntileSurface", 
    L"CPU optimized", 
    L"Memexport by texel", 
    L"Memexport by transaction", 
    L"Memexport by packed transaction",
    L"Remapped Resolve", 
};
C_ASSERT( _countof( g_strUntileMethodNames ) == UNTILE_METHOD_COUNT );


template<typename t_type>
t_type Min( t_type a, t_type b ) { return a < b ? a : b; }
template<typename t_type>
t_type Max( t_type a, t_type b ) { return a > b ? a : b; }


//---------------------------------------------------------------------------------------------------------
// Name: GetNonAs16NonsRGBFormat( )
// Desc: Convert sRGB format to linear format, stripping out '_AS_16'.  This function allows us to 
// override gamma-conversion, and the accompanying '_AS_16' performance cost, during untiling.  
//---------------------------------------------------------------------------------------------------------
inline D3DFORMAT GetNonAs16NonsRGBFormat( D3DFORMAT fmtBase )
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


inline VOID GetRepeatBlockDimensions( UINT iTexelPitch, UINT* piRepeatBlockWidth, UINT* piRepeatBlockHeight )
{
    // These block sizes were determined empirically by examination of the tiling pattern...
    switch( iTexelPitch )
    {
    case 16:
    case 8:
    case 4:
        *piRepeatBlockWidth = 32;
        *piRepeatBlockHeight = 32;
        break;

    case 2:
        *piRepeatBlockWidth = 64;
        *piRepeatBlockHeight = 32;
        break;

    case 1:
        *piRepeatBlockWidth = 128;
        *piRepeatBlockHeight = 32;
        break;

    default:
        assert( FALSE );    // not yet supported
        break;
    }
}


inline VOID GetMoveAlignment( UINT iTexelPitch, UINT* piMoveSize )
{
    // What's the maximum block of consecutive pixels which the tiling operation always moves as a unit.
    // These sizes were determined empirically by examination of the tiling pattern...
    switch( iTexelPitch )
    {
    case 16:
    case 8:
    case 4:
    case 2:
        *piMoveSize = 16;
        break;

    case 1:
        *piMoveSize = 8;
        break;

    default:
        assert( FALSE );    // not yet supported
        break;
    }
}


inline VOID CalculateLinearToTiledRemapping( UINT iTexelPitch, WORD* pLinearToTiled2DAddress )
{
    UINT iRepeatBlockWidth, iRepeatBlockHeight;
    GetRepeatBlockDimensions( iTexelPitch, &iRepeatBlockWidth, &iRepeatBlockHeight );

    UINT iMoveAlignment;
    GetMoveAlignment( iTexelPitch, &iMoveAlignment );
    ATG_Unused( iMoveAlignment );   // avert compile warning about unused local variable

    memset( pLinearToTiled2DAddress, 0x00, iRepeatBlockWidth * iRepeatBlockHeight );    // can't use XMemSet on write-combined memory
    UINT iMaxTiledOffset = 0;
    BOOL bClosedSet = FALSE;
    for( UINT y = 0; y < iRepeatBlockHeight; ++y )
    {
        for( UINT x = 0; x < iRepeatBlockWidth; ++x )
        {
            // Find the lookup value
            UINT iLinearOffset = x + y * iRepeatBlockWidth;
            UINT iTiledOffset = XGAddress2DTiledOffset( x, y, iRepeatBlockWidth, iTexelPitch );
            pLinearToTiled2DAddress[iLinearOffset] = (WORD) iTiledOffset;

            // ERROR CHECKING CODE:  Check whether we have a one-to-one mapping so far
            iMaxTiledOffset = Max( iMaxTiledOffset, iTiledOffset );
            bClosedSet = ( iMaxTiledOffset == iLinearOffset );

            // ERROR CHECKING CODE:  Check that the tiled offset is independent of total texture dims.
            assert( iTiledOffset == XGAddress2DTiledOffset( x, y, 2 * iRepeatBlockWidth, iTexelPitch ) );

            // ERROR CHECKING CODE:  Check that enough texels move as a unit
            BOOL bConsecutive = ( iLinearOffset > 0 && 
                ( iTiledOffset == (UINT) pLinearToTiled2DAddress[iLinearOffset - 1] + 1 ) );
            ATG_Unused( bConsecutive );   // avert compile warning about unused local variable
            assert( bConsecutive || ( ( iLinearOffset * iTexelPitch ) % iMoveAlignment == 0 ) );
        }
    }

    // ERROR CHECKING CODE:  We must have a closed set under tiling.  That means the lookup table maps 
    // this block of texels to itself one-to-one.  This is a necessary but not sufficient condition 
    // for the block to repeat.
    assert( bClosedSet );

    // ERROR CHECKING CODE:  This means if we move over by one repeat block, the pattern starts again 
    // with the next location in memory.  Also a necessary but not sufficient condition.
    assert( XGAddress2DTiledOffset( iRepeatBlockWidth, 0, 2 * iRepeatBlockWidth, iTexelPitch ) 
        == iRepeatBlockWidth * iRepeatBlockHeight );
}
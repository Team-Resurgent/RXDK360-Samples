//---------------------------------------------------------------------------------------------------------
// FastBlockCompressVMX.cpp
//
// This file is designed to be a nearly standalone module, which could be cut-and-pasted into
// title code without significant dependencies.
//
// XNA Developer Connection
// Copyright ( C ) Microsoft Corporation. All rights reserved.
//---------------------------------------------------------------------------------------------------------
#include <xtl.h>    // must come before the others
#include <assert.h>
#include <VectorIntrinsics.h>

#include "FastBlockCompress.h"
#include "FastBlockCompressVMX.h"

//---------------------------------------------------------------------------------------------------------
// Specifier for reading a single channel out of a 2- or 4-channel texel
//---------------------------------------------------------------------------------------------------------
enum CHANNEL
{
    CHANNEL_ALPHA, 
    CHANNEL_RED, 
    CHANNEL_GREEN, 
    CHANNEL_BLUE, 

    CHANNEL_U, 
    CHANNEL_V, 

    CHANNEL_COUNT
};


//---------------------------------------------------------------------------------------------------------
// VMX Compression functions for full D3D texture formats
//---------------------------------------------------------------------------------------------------------
typedef VOID ( *BlockFn )( BYTE *pDst, 
                          __vector4 vSrc0123, __vector4 vSrc4567, __vector4 vSrc89ab, __vector4 vSrccdef );



//---------------------------------------------------------------------------------------------------------
// Helper functions.  
//---------------------------------------------------------------------------------------------------------
template <typename t_type> static inline t_type RoundDownToPowerOf2( const t_type& t, DWORD dwPowerOf2 )
{
    return ( t_type )( ( ( DWORD )t ) & ~( dwPowerOf2 - 1 ) );
}

template <typename t_type> static inline t_type RoundUpToPowerOf2( const t_type& t, DWORD dwPowerOf2 )
{
    return ( t_type )( ( ( ( DWORD )t ) + ( dwPowerOf2 - 1 ) ) & ~( dwPowerOf2 - 1 ) );
}


//---------------------------------------------------------------------------------------------------------
// Kick data into the cache.  
//---------------------------------------------------------------------------------------------------------
static void Prefetch( const BYTE* pData, DWORD dwSize )
{
    static const DWORD g_dwCacheLineSize = 128;

    // Expand input range to be cache-line aligned:
    const BYTE* pEnd = pData + dwSize;
    pData = RoundDownToPowerOf2( pData, g_dwCacheLineSize );
    dwSize = RoundUpToPowerOf2( pEnd, g_dwCacheLineSize ) - pData;

    for( size_t i = 0; i < dwSize; i += g_dwCacheLineSize )
    {
        __dcbt( i, pData );
    }
}


#pragma warning( push )
#pragma warning( disable:4700 ) // use of uninitialized variable in __vpkd3d, __vrlimi

//---------------------------------------------------------------------------------------------------------
// Helper functions for VMX compressors.  These are all force-inlined, so that
// the algorithms are globally optimized.  They are separated into functions
// for clarity and re-usability.
//---------------------------------------------------------------------------------------------------------

//---------------------------------------------------------------------------------------------------------
// Name: ConditionalExchange( )
// Desc: If vCondition is true, then interchange vA and vB.  Implemented branchlessly as two __vsel
// instructions.  This function is used to switch the anchor order in DXT1 blocks, because the wrong
// anchor order implies a 1-bit alpha block.
//---------------------------------------------------------------------------------------------------------
__forceinline void ConditionalExchange( __vector4& vA, __vector4& vB, __vector4 vCondition )
{
    __vector4 vTemp = vA;
    vA = __vsel( vA, vB, vCondition );
    vB = __vsel( vB, vTemp, vCondition );
}


//---------------------------------------------------------------------------------------------------------
// Name: AbsDiffByte( )
// Desc: Input is two vectors, interpreted as 16-wide BYTEs.  Output is the absolute value of the 
// difference, for each BYTE.
//---------------------------------------------------------------------------------------------------------
__forceinline __vector4 AbsDiffByte( __vector4 a, __vector4 b )
{
    return __vsububs( __vmaxub( a, b ), __vminub( a, b ) );
}


//---------------------------------------------------------------------------------------------------------
// Name: HorizSumBytesInHalf( )
// Desc: Input and output are vectors, interpreted as 16-wide unsigned BYTEs.  
// Each element of the output is the sum of two BYTEs of the input:
// 
//  Out[ 0] = Out[ 1] = In[ 0] + In[ 1]
//  ...
//  Out[14] = Out[15] = In[14] + In[15]
// 
// On overflow, the additions saturate to 255.
//---------------------------------------------------------------------------------------------------------
__forceinline __vector4 HorizSumBytesInHalf( __vector4 a )
{
    // If the calling function contains a '__vspltisb( 8 )', compiler will optimize
    // this one away.  We use BYTE rather than HALF because the calling function
    // often already loads this constant.
    __vector4 vByteEight        = __vspltisb( 8 );
    a                           = __vaddubs( a, __vrlh( a, vByteEight ) );

    return a;
}


//---------------------------------------------------------------------------------------------------------
// Name: HorizSumRGBBytesInWord( )
// Desc: Input and output are vectors, interpreted as 16-wide BYTEs.  
// Each element of the output is the sum of three BYTEs of the input (ignoring a fourth BYTE):
// 
//  Out[ 0] = Out[ 1] = Out[ 2] = Out[ 3] = In[ 1] + In[ 2] + In[ 3]
//  ...
//  Out[12] = Out[13] = Out[14] = Out[15] = In[13] + In[14] + In[15]
// 
// On overflow, the additions saturate to 255.
//---------------------------------------------------------------------------------------------------------
__forceinline __vector4 HorizSumRGBBytesInWord( __vector4 a )
{
    // If the calling function contains a '__vspltisb( 8 )', compiler will optimize
    // this one away.  We use BYTE rather than HALF because the calling function
    // often already loads this constant.
    __vector4 vByteEight        = __vspltisb( 8 );
    __vector4 vWordNegSixteen   = __vspltisw( -16 );    // Positive 16 overflows __vspltisw
    a                           = __vslw( a, vByteEight ); // eliminate A from ARGB
    a                           = __vaddubs( a, __vrlh( a, vByteEight ) );
    a                           = __vaddubs( a, __vrlw( a, vWordNegSixteen ) );

    return a;
}


//---------------------------------------------------------------------------------------------------------
// Name: FindMinMaxARGB( )
// Desc: The inputs are 4 VMX registers, each consisting of 16 bytes, interpreted as 4 ARGB vectors.
// The outputs are vMinARGB and vMaxARGB, which contain respectively the min and max overall values 
// of ARGB, replicated 4 times into xyzw.  
//
// Both the FLOAT and BYTE algorithms call this routine, because it's more efficient to compute the
// min/max as bytes, and there's no advantage to using FLOAT.
//---------------------------------------------------------------------------------------------------------
__forceinline void FindMinMaxARGB( __vector4& vMinARGB, __vector4& vMaxARGB, 
                                  __vector4 vSrc0123, __vector4 vSrc4567, __vector4 vSrc89ab, __vector4 vSrccdef )
{
    __vector4 vARGBx4[4] = { vSrc0123, vSrc4567, vSrc89ab, vSrccdef };
    // vARGBx4[0]       [ A0 | R0 | G0 | B0 ][ A1 | R1 | G1 | B1 ][ A2 | R2 | G2 | B2 ][ A3 | R3 | G3 | B3 ]
    // vARGBx4[1]       [ A4 | R4 | G4 | B4 ][ A5 | R5 | G5 | B5 ][ A6 | R6 | G6 | B6 ][ A7 | R7 | G7 | B7 ]
    // vARGBx4[2]       [ A8 | R8 | G8 | B8 ][ A9 | R9 | G9 | B9 ][ Aa | Ra | Ga | Ba ][ Ab | Rb | Gb | Bb ]
    // vARGBx4[3]       [ Ac | Rc | Gc | Bc ][ Ad | Rd | Gd | Bd ][ Ae | Re | Ge | Be ][ Af | Rf | Gf | Bf ]

    __vector4 vMinARGBx4        = __vminub( __vminub( vARGBx4[0], vARGBx4[1] ), 
        __vminub( vARGBx4[2], vARGBx4[3] ) );
    __vector4 vMaxARGBx4        = __vmaxub( __vmaxub( vARGBx4[0], vARGBx4[1] ), 
        __vmaxub( vARGBx4[2], vARGBx4[3] ) );
    vMinARGB                    = vMinARGBx4;
    vMinARGB                    = __vminub( vMinARGB, __vsldoi( vMinARGB, vMinARGB, 4 ) );
    vMinARGB                    = __vminub( vMinARGB, __vsldoi( vMinARGB, vMinARGB, 8 ) );
    vMaxARGB                    = vMaxARGBx4;
    vMaxARGB                    = __vmaxub( vMaxARGB, __vsldoi( vMaxARGB, vMaxARGB, 4 ) );
    vMaxARGB                    = __vmaxub( vMaxARGB, __vsldoi( vMaxARGB, vMaxARGB, 8 ) );
}


//---------------------------------------------------------------------------------------------------------
// Name: FindMinMaxUVUV( )
// Desc: The inputs are 2 VMX registers, each consisting of 16 bytes, interpreted as 8 UV vectors.
// The outputs are vMinUVUV and vMaxUVUV, which contain respectively the min and max overall values 
// of UV, replicated 8 times.  
//
// Both the FLOAT and BYTE algorithms call this routine, because it's more efficient to compute the
// min/max as bytes, and there's no advantage to using FLOAT.
//---------------------------------------------------------------------------------------------------------
__forceinline void FindMinMaxUVUV( __vector4& vMinUVUV, __vector4& vMaxUVUV, 
                                  __vector4 vSrc01234567, __vector4 vSrc89abcdef )
{
    __vector4 vUVUVx8[2] = { vSrc01234567, vSrc89abcdef };
    // vUVUVx8[0]       [ u0 | v0 | u1 | v1 ][ u2 | v2 | u3 | v3 ][ u4 | v4 | u5 | v5 ][ u6 | v6 | u7 | v7 ]
    // vUVUVx8[1]       [ u8 | v8 | u9 | v9 ][ ua | va | ub | vb ][ uc | vc | ud | vd ][ ue | ve | uf | vf ]

    // Find the min/max UV
    // Replicate min/max for U to xz and for V to yw
    vMinUVUV                    = __vminub( vUVUVx8[0], vUVUVx8[1] );
    vMinUVUV                    = __vminub( vMinUVUV, __vsldoi( vMinUVUV, vMinUVUV, 8 ) );
    vMinUVUV                    = __vminub( vMinUVUV, __vsldoi( vMinUVUV, vMinUVUV, 4 ) );
    vMinUVUV                    = __vminub( vMinUVUV, __vsldoi( vMinUVUV, vMinUVUV, 2 ) );
    vMaxUVUV                    = __vmaxub( vUVUVx8[0], vUVUVx8[1] );
    vMaxUVUV                    = __vmaxub( vMaxUVUV, __vsldoi( vMaxUVUV, vMaxUVUV, 8 ) );
    vMaxUVUV                    = __vmaxub( vMaxUVUV, __vsldoi( vMaxUVUV, vMaxUVUV, 4 ) );
    vMaxUVUV                    = __vmaxub( vMaxUVUV, __vsldoi( vMaxUVUV, vMaxUVUV, 2 ) );
}


//---------------------------------------------------------------------------------------------------------
// Name: ExpandBYTEToFLOAT( )
// Desc: Input is a vector interpreted as 16-wide BYTEs.  Output is a vector interpreted as 4-wide FLOATS.
// The output components are equal to the first four values of the input:
//
// vARGBBYTE            [ A  | R  | G  | B  ][    |    |    |    ][    |    |    |    ][    |    |    |    ]
//
// IF t_bUnswap == TRUE:
// vRGBAFLOAT           [         A         ][         R         ][         G         ][         B         ]
//
// IF t_bUnswap == FALSE:
// vRGBAFLOAT           [         R         ][         G         ][         B         ][         A         ]
//
// The remaining 12 values of the input are not used.
//
// The vupkd3d VMX instruction has an automatic swap from ARGB to RGBA when using VPACK_D3DCOLOR.  
// The t_bUnswap parameter undoes this swap.  We do this primarily for clarity's sake --- for 
// for instance when the inputs actually represent UVUV, or AAAA.  
//
// For performance, it may be a better idea to retain the swapped order throughout the calculation.
//---------------------------------------------------------------------------------------------------------
template< BOOL t_bUnswap > 
__forceinline void ExpandBYTEToFLOAT( __vector4& vRGBAFLOAT, __vector4 vARGBBYTE )
{
    static CONST __vector4   UnpackOffset = XM_UNPACK_UNSIGNEDN_OFFSET( 8, 8, 8, 8 );
    static CONST __vector4   UnpackScale = XM_UNPACK_UNSIGNEDN_SCALE( 8, 8, 8, 8 );

    vRGBAFLOAT                  = __vupkd3d( vARGBBYTE, VPACK_D3DCOLOR );
    vRGBAFLOAT                  = __vmaddcfp( UnpackScale, vRGBAFLOAT, UnpackOffset );
#pragma warning( push )
#pragma warning( disable:4127 )   // conditional expression is constant
    if ( t_bUnswap )
#pragma warning( pop )
    {
        vRGBAFLOAT              = __vpermwi( vRGBAFLOAT, VPERMWI_CONST( 3, 0, 1, 2 ) );
    }
}


//---------------------------------------------------------------------------------------------------------
// Name: ExpandBYTEToFLOATRGBA( )
// Desc: Input is a vector interpreted as 16-wide BYTEs.  Output is 4 vectors, each interpreted as 4-wide 
// FLOATS.   The output components are equal to the values of the input.
//
// vARGBBYTE            [ A0 | R0 | G0 | B0 ][ A1 | R1 | G1 | B1 ][ A2 | R2 | G2 | B2 ][ A3 | R3 | G3 | B3 ]
//
// vRGBAFLOAT[0]        [         R0        ][         G0        ][         B0        ][         A0        ]
// vRGBAFLOAT[1]        [         R1        ][         G1        ][         B1        ][         A1        ]
// vRGBAFLOAT[2]        [         R2        ][         G2        ][         B2        ][         A2        ]
// vRGBAFLOAT[3]        [         R3        ][         G3        ][         B3        ][         A3        ]
//---------------------------------------------------------------------------------------------------------
__forceinline void ExpandBYTEToFLOATRGBA( __vector4 vRGBAFLOAT[4], __vector4 vARGBBYTE )
{
    for ( UINT i = 0; i < 4; ++i )
    {
        vARGBBYTE               = __vsldoi( vARGBBYTE, vARGBBYTE, 1 << 2 );
        ExpandBYTEToFLOAT<FALSE>( vRGBAFLOAT[i], vARGBBYTE );
    }
}


//---------------------------------------------------------------------------------------------------------
// Name: ExpandBYTEToFLOATRGBA( )
// Desc: Input is 4 vectors interpreted as 16-wide BYTEs.  Output is 16 vectors, each interpreted as 4-wide 
// FLOATS.   The output components are equal to input values expanded to FLOAT.
//
// vMin and vMax are expanded in place.  Upon input, they contain the same 4 values replicated 4 times,
// so no data is lost by the expansion.
//
// vARGBBYTE[0]         [ A0 | R0 | G0 | B0 ][ A1 | R1 | G1 | B1 ][ A2 | R2 | G2 | B2 ][ A3 | R3 | G3 | B3 ]
// vARGBBYTE[1]         [ A4 | R4 | G4 | B4 ][ A5 | R5 | G5 | B5 ][ A6 | R6 | G6 | B6 ][ A7 | R7 | G7 | B7 ]
// vARGBBYTE[2]         [ A8 | R8 | G8 | B8 ][ A9 | R9 | G9 | B9 ][ Aa | Ra | Ga | Ba ][ Ab | Rb | Gb | Bb ]
// vARGBBYTE[3]         [ Ac | Rc | Gc | Bc ][ Ad | Rd | Gd | Bd ][ Ae | Re | Ge | Be ][ Af | Rf | Gf | Bf ]
//
// vRGBAFLOAT[ 0]       [         R0        ][         G0        ][         B0        ][         A0        ]
// vRGBAFLOAT[ 1]       [         R1        ][         G1        ][         B1        ][         A1        ]
// vRGBAFLOAT[ 2]       [         R2        ][         G2        ][         B2        ][         A2        ]
// vRGBAFLOAT[ 3]       [         R3        ][         G3        ][         B3        ][         A3        ]
// vRGBAFLOAT[ 4]       [         R4        ][         G4        ][         B4        ][         A4        ]
// vRGBAFLOAT[ 5]       [         R5        ][         G5        ][         B5        ][         A5        ]
// vRGBAFLOAT[ 6]       [         R6        ][         G6        ][         B6        ][         A6        ]
// vRGBAFLOAT[ 7]       [         R7        ][         G7        ][         B7        ][         A7        ]
// vRGBAFLOAT[ 8]       [         R8        ][         G8        ][         B8        ][         A8        ]
// vRGBAFLOAT[ 9]       [         R9        ][         G9        ][         B9        ][         A9        ]
// vRGBAFLOAT[10]       [         Ra        ][         Ga        ][         Ba        ][         Aa        ]
// vRGBAFLOAT[11]       [         Rb        ][         Gb        ][         Bb        ][         Ab        ]
// vRGBAFLOAT[12]       [         Rc        ][         Gc        ][         Bc        ][         Ac        ]
// vRGBAFLOAT[13]       [         Rd        ][         Gd        ][         Bd        ][         Ad        ]
// vRGBAFLOAT[14]       [         Re        ][         Ge        ][         Be        ][         Ae        ]
// vRGBAFLOAT[15]       [         Rf        ][         Gf        ][         Bf        ][         Af        ]
//---------------------------------------------------------------------------------------------------------
__forceinline void ExpandBYTEToFLOATRGBA( __vector4 vRGBAFLOAT[16], __vector4 vARGBBYTE[4] )
{
    ExpandBYTEToFLOATRGBA( &vRGBAFLOAT[0],  vARGBBYTE[0] );
    ExpandBYTEToFLOATRGBA( &vRGBAFLOAT[4],  vARGBBYTE[1] );
    ExpandBYTEToFLOATRGBA( &vRGBAFLOAT[8],  vARGBBYTE[2] );
    ExpandBYTEToFLOATRGBA( &vRGBAFLOAT[12], vARGBBYTE[3] );
}


//---------------------------------------------------------------------------------------------------------
// Name: ExpandBYTEToFLOATUVUV( )
// Desc: Input is a vector interpreted as 16-wide BYTEs.  Output is 2 vectors, each interpreted as 4-wide 
// FLOATS.   The output components are equal to the values of the input.
//
// vUVUVBYTE            [ u0 | v0 | u1 | v1 ][ u2 | v2 | u3 | v3 ][    |    |    |    ][    |    |    |    ]
//
// vUVUVFLOAT[0]        [         u0        ][         v0        ][         u1        ][         v1        ]
// vUVUVFLOAT[1]        [         u2        ][         v2        ][         u3        ][         v3        ]
//
// The remaining 8 input values are not used (they belong to the following DXT block).
//---------------------------------------------------------------------------------------------------------
__forceinline void ExpandBYTEToFLOATUVUV( __vector4 vUVUVFLOAT[2], __vector4 vUVUVBYTE )
{
    for ( UINT i = 0; i < 2; ++i )
    {
        vUVUVBYTE               = __vsldoi( vUVUVBYTE, vUVUVBYTE, 1 << 2 );
        ExpandBYTEToFLOAT<TRUE>( vUVUVFLOAT[i], vUVUVBYTE );
    }
}


//---------------------------------------------------------------------------------------------------------
// Name: ExpandBYTEToFLOATUVUV( )
// Desc: Input is 4 vectors interpreted as 16-wide BYTEs.  Output is 8 vectors, each interpreted as 4-wide 
// FLOATS.   The output components are equal to input values expanded to FLOAT.  Only the first 8 values 
// of each input vector are used (the others belong to the following DXT block).
//
// vMin and vMax are expanded in place.  Upon input, they contain the same 2 values replicated 8 times,
// so no data is lost by the expansion.  Upon output, they contain the expanded values replicated 2 times.
//
// vUVUVBYTE[0]          [ u0 | v0 | u1 | v1 ][ u2 | v2 | u3 | v3 ][    |    |    |    ][    |    |    |    ]
// vUVUVBYTE[1]          [ u4 | v4 | u5 | v5 ][ u6 | v6 | u7 | v7 ][    |    |    |    ][    |    |    |    ]
// vUVUVBYTE[2]          [ u8 | v8 | u9 | v9 ][ ua | va | ub | vb ][    |    |    |    ][    |    |    |    ]
// vUVUVBYTE[3]          [ uc | vc | ud | vd ][ ue | ve | uf | vf ][    |    |    |    ][    |    |    |    ]
//
// vUVUVFLOAT[0]         [         u0        ][         v0        ][         u1        ][         v1        ]
// vUVUVFLOAT[1]         [         u2        ][         v2        ][         u3        ][         v3        ]
// vUVUVFLOAT[2]         [         u4        ][         v4        ][         u5        ][         v5        ]
// vUVUVFLOAT[3]         [         u6        ][         v6        ][         u7        ][         v7        ]
// vUVUVFLOAT[4]         [         u8        ][         v8        ][         u9        ][         v9        ]
// vUVUVFLOAT[5]         [         ua        ][         va        ][         ub        ][         vb        ]
// vUVUVFLOAT[6]         [         uc        ][         vc        ][         ud        ][         vd        ]
// vUVUVFLOAT[7]         [         ue        ][         ve        ][         uf        ][         vf        ]
//---------------------------------------------------------------------------------------------------------
__forceinline void ExpandBYTEToFLOATUVUV( __vector4 vUVUVFLOAT[8], __vector4 vUVUVBYTE[4] )
{
    ExpandBYTEToFLOATUVUV( &vUVUVFLOAT[0], vUVUVBYTE[0] );
    ExpandBYTEToFLOATUVUV( &vUVUVFLOAT[2], vUVUVBYTE[1] );
    ExpandBYTEToFLOATUVUV( &vUVUVFLOAT[4], vUVUVBYTE[2] );
    ExpandBYTEToFLOATUVUV( &vUVUVFLOAT[6], vUVUVBYTE[3] );
}


//---------------------------------------------------------------------------------------------------------
// Name: ExpandBYTEToFLOATAAAA( )
// Desc: Input is a vectors interpreted as 16-wide BYTEs.  Output is 4 vectors, each interpreted as 4-wide 
// FLOATS.   The output components are equal to the values of the input.
//
// vSrc                 [ A0 | A1 | A2 | A3 ][ A4 | A5 | A6 | A7 ][ A8 | A9 | Aa | Ab ][ Ac | Ad | Ae | Af ]
//
// vAlpha[0]            [         A0        ][         A1        ][         A2        ][         A3        ]
// vAlpha[1]            [         A4        ][         A5        ][         A6        ][         A7        ]
// vAlpha[2]            [         A8        ][         A9        ][         Aa        ][         Ab        ]
// vAlpha[3]            [         Ac        ][         Ad        ][         Ae        ][         Af        ]
//---------------------------------------------------------------------------------------------------------
__forceinline void ExpandBYTEToFLOATAAAA( __vector4 vAlpha[4], __vector4 vSrc )
{
    for ( UINT i = 0; i < 4; ++i )
    {
        vSrc                    = __vsldoi( vSrc, vSrc, 1 << 2 );
        ExpandBYTEToFLOAT<TRUE>( vAlpha[i], vSrc );
    }
}


//---------------------------------------------------------------------------------------------------------
// Name: SelectOneChannelFromFour( )
// Desc: This is a helper function to enable the same code to be used for alpha, U, or V compression.
// Input is 4 vectors, interpreted as 16-wide BYTEs.  Output is a vector, interpreted as 16-wide BYTEs.
//
// The output contains some 16 of the 64 input values, as controlled by the hard-coded permutation masks.
// When the transpose option is selected, then the output is transposed as if it were a 4x4 matrix.
// This option sometimes helps simplify the subsequent calculation.
//
// Some examples:
//
// IF t_bTranspose == FALSE, t_iChannel == CHANNEL_ALPHA:
// In[0]                [ A0 | R0 | G0 | B0 ][ A1 | R1 | G1 | B1 ][ A2 | R2 | G2 | B2 ][ A3 | R3 | G3 | B3 ]
// In[1]                [ A4 | R4 | G4 | B4 ][ A5 | R5 | G5 | B5 ][ A6 | R6 | G6 | B6 ][ A7 | R7 | G7 | B7 ]
// In[2]                [ A8 | R8 | G8 | B8 ][ A9 | R9 | G9 | B9 ][ Aa | Ra | Ga | Ba ][ Ab | Rb | Gb | Bb ]
// In[3]                [ Ac | Rc | Gc | Bc ][ Ad | Rd | Gd | Bd ][ Ae | Re | Ge | Be ][ Af | Rf | Gf | Bf ]
//---------------------------------------------------------------------------------------------------------
// Out                  [ A0 | A1 | A2 | A3 ][ A4 | A5 | A6 | A7 ][ A8 | A9 | Aa | Ab ][ Ac | Ad | Ae | Af ]
//
// IF t_bTranspose == FALSE, t_iChannel == CHANNEL_U:
// In[0]                [ u0 | v0 | u1 | v1 ][ u2 | v2 | u3 | v3 ][    |    |    |    ][    |    |    |    ]
// In[1]                [ u4 | v4 | u5 | v5 ][ u6 | v6 | u7 | v7 ][    |    |    |    ][    |    |    |    ]
// In[2]                [ u8 | v8 | u9 | v9 ][ ua | va | ub | vb ][    |    |    |    ][    |    |    |    ]
// In[3]                [ uc | vc | ud | vd ][ ue | ve | uf | vf ][    |    |    |    ][    |    |    |    ]
//---------------------------------------------------------------------------------------------------------
// Out                  [ u0 | u1 | u2 | u3 ][ u4 | u5 | u6 | u7 ][ u8 | u9 | ua | ub ][ uc | ud | ue | uf ]
//
// IF t_bTranspose == TRUE, t_iChannel == CHANNEL_ALPHA:
// In[0]                [ A0 | R0 | G0 | B0 ][ A1 | R1 | G1 | B1 ][ A2 | R2 | G2 | B2 ][ A3 | R3 | G3 | B3 ]
// In[1]                [ A4 | R4 | G4 | B4 ][ A5 | R5 | G5 | B5 ][ A6 | R6 | G6 | B6 ][ A7 | R7 | G7 | B7 ]
// In[2]                [ A8 | R8 | G8 | B8 ][ A9 | R9 | G9 | B9 ][ Aa | Ra | Ga | Ba ][ Ab | Rb | Gb | Bb ]
// In[3]                [ Ac | Rc | Gc | Bc ][ Ad | Rd | Gd | Bd ][ Ae | Re | Ge | Be ][ Af | Rf | Gf | Bf ]
//---------------------------------------------------------------------------------------------------------
// Out                  [ A0 | A4 | A8 | Ac ][ A1 | A5 | A9 | Ad ][ A2 | A6 | Aa | Ae ][ A3 | A7 | Ab | Af ]
//
//---------------------------------------------------------------------------------------------------------
template< BOOL t_bTranspose, UINT t_iChannel >
__forceinline void SelectOneChannelFromFour( __vector4& vOneChannelx16, __vector4 vFourChannelx4[4] )
{
    // Merge all 16 Alpha values into 1 VMX register as BYTEs
#pragma warning( push )
#pragma warning( disable:4127 )   // conditional expression is constant
    if( t_bTranspose )
#pragma warning( pop )
    {
        // 'Transpose' so that:
        //  Elements 0/4/8/c are in the .x component
        //  Elements 1/5/9/d are in the .y component
        //  Elements 2/6/a/e are in the .z component
        //  Elements 3/7/c/f are in the .w component
        static CONST __vector4i viPermSelectChannel[CHANNEL_COUNT] = 
        {
            // transpose
            { 0x00100414, 0x08180c1c, 0x00100414, 0x08180c1c, },    // alpha
            { 0x01110515, 0x09190d1d, 0x01110515, 0x09190d1d, },    // B
            { 0x02120616, 0x0a1a0e1e, 0x02120616, 0x0a1a0e1e, },    // G
            { 0x03130717, 0x0b1b0f1f, 0x03130717, 0x0b1b0f1f, },    // R

            { 0x00100212, 0x04140616, 0x00100212, 0x04140616, },    // U
            { 0x01110313, 0x05150717, 0x01110313, 0x05150717, },    // V
        };

        vOneChannelx16          = __vmrglb( 
            __vperm( vFourChannelx4[0], vFourChannelx4[2], 
            *( __vector4* )&viPermSelectChannel[t_iChannel] ), 
            __vperm( vFourChannelx4[1], vFourChannelx4[3], 
            *( __vector4* )&viPermSelectChannel[t_iChannel] ) );
    }
    else
    {
        // Leave in native order so that:
        //  Elements 0-3   are in the x component
        //  Elements 4-7   are in the y component
        //  Elements 8-11  are in the z component
        //  Elements 12-15 are in the w component
        static CONST __vector4i viPermSelectChannel[CHANNEL_COUNT] = 
        {
            // non-transpose
            { 0x0004080c, 0x1014181c, 0x0004080c, 0x1014181c, },    // alpha
            { 0x0105090d, 0x1115191d, 0x0105090d, 0x1115191d, },    // B
            { 0x02060a0e, 0x12161a1e, 0x02060a0e, 0x12161a1e, },    // G
            { 0x03070b0f, 0x13171b1f, 0x03070b0f, 0x13171b1f, },    // R

            { 0x00020406, 0x10121416, 0x00020406, 0x10121416, },    // U
            { 0x01030507, 0x11131517, 0x01030507, 0x11131517, },    // V
        };

        vOneChannelx16          = __vmrglw( 
            __vperm( vFourChannelx4[0], vFourChannelx4[2], 
            *( __vector4* )&viPermSelectChannel[t_iChannel] ), 
            __vperm( vFourChannelx4[1], vFourChannelx4[3], 
            *( __vector4* )&viPermSelectChannel[t_iChannel] ) );
    }
}


//---------------------------------------------------------------------------------------------------------
// Name: SelectOneChannelFromFour( )
// Desc: This is a helper function to enable the same code to be used for alpha, U, V compression.
// Input and output are vectors, interpreted as 16-wide BYTEs.
//
// The input values are assumed to contain the same data, replicated either 2 or 4 times, and the output
// values are also replicated.
//
// Some examples:
//
// IF t_iChannel == CHANNEL_ALPHA:
// In                   [ A  | R  | G  | B  ][ A  | R  | G  | B  ][ A  | R  | G  | B  ][ A  | R  | G  | B  ]
//---------------------------------------------------------------------------------------------------------
// Out                  [ A  | A  | A  | A  ][ A  | A  | A  | A  ][ A  | A  | A  | A  ][ A  | A  | A  | A  ]
//
// IF t_iChannel == CHANNEL_U:
// In                   [ u  | v  | u  | v  ][ u  | v  | u  | v  ][ u  | v  | u  | v  ][ u  | v  | u  | v  ]
//---------------------------------------------------------------------------------------------------------
// Out                  [ u  | u  | u  | u  ][ u  | u  | u  | u  ][ u  | u  | u  | u  ][ u  | u  | u  | u  ]
//
//---------------------------------------------------------------------------------------------------------
template< UINT t_iChannel >
__forceinline void SelectOneChannelFromFour( __vector4& vOneChannelx16, __vector4 vFourChannelx4 )
{
    // Replicate one BYTE from each of xyzw to all four BYTEs
    C_ASSERT( CHANNEL_ALPHA == 0 && CHANNEL_RED == 1 && CHANNEL_GREEN == 2 && CHANNEL_BLUE == 3
        && CHANNEL_U == 4 && CHANNEL_V == 5 );
    vOneChannelx16              = __vspltb( vFourChannelx4, t_iChannel % 4 );
}


//---------------------------------------------------------------------------------------------------------
// Name: FindCovarianceRGB( )
// Desc: Finds covariances between R and G, and B and G, across the 16 input vectors.
// Where the actual covariance formula uses the mean, we substitute the center, for computational 
// efficiency.
//
// The results are used to estimate which of the 4 bounding box diagonals is a closest fit to the
// principal axis.
//---------------------------------------------------------------------------------------------------------
__forceinline __vector4 FindCovarianceRGB( __vector4 vRGBA[16], __vector4 vCenter )
{
    // Move everything relative to the center.  
    __vector4 vDiffCenter[16];
    for( UINT i = 0; i < 16; ++i )
    {
        vDiffCenter[i]          = __vsubfp( vRGBA[i], vCenter ); 
    }
    __vector4 vCovPartial[8];
    // Sum up (R*G, G*G, B*G, A*A) over all 16 points
    for( UINT i = 0; i < 8; ++i )
    {
        //   R_i   * G_i,   G_i   * G_i,   B_i   * B_i
        // + R_i+i * G_i+1, G_i+1 * G_i+1, B_i+1 * B_i+1 
        vCovPartial[i]          = __vmaddfp( vDiffCenter[2*i+0], 
            __vpermwi( vDiffCenter[2*i+0], VPERMWI_CONST( 1, 1, 1, 3 ) ), 
            __vmulfp( vDiffCenter[2*i+1], 
            __vpermwi( vDiffCenter[2*i+1], VPERMWI_CONST( 1, 1, 1, 3 ) ) ) );
    }
    __vector4 vCovariance       = __vaddfp( 
        __vaddfp( 
        __vaddfp( vCovPartial[0], vCovPartial[1] ), 
        __vaddfp( vCovPartial[2], vCovPartial[3] ) ), 
        __vaddfp( 
        __vaddfp( vCovPartial[4], vCovPartial[5] ), 
        __vaddfp( vCovPartial[6], vCovPartial[7] ) ) );

    return vCovariance;
}


//---------------------------------------------------------------------------------------------------------
// Name: FindCovarianceUV( )
// Desc: Finds covariance between U and V, across the 16 input vectors.
// Where the actual covariance formula uses the mean, we substitute the center, for computational 
// efficiency.
//
// The results are used to estimate which of the 2 bounding box diagonals is a closer fit to the
// principal axis.
//---------------------------------------------------------------------------------------------------------
__forceinline __vector4 FindCovarianceUV( __vector4 vUVUV[8], __vector4 vCenter )
{
    // Move everything relative to the center.  
    __vector4 vDiffCenter[8];
    for( UINT i = 0; i < 8; ++i )
    {
        vDiffCenter[i]          = __vsubfp( vUVUV[i], vCenter );   
    }
    // Sum up U*V over all 16 points
    __vector4 vCovPartial[4];
    for( UINT i = 0; i < 4; ++i )
    {
        vCovPartial[i]          = __vmaddfp( vDiffCenter[2*i+0], 
            __vpermwi( vDiffCenter[2*i+0], VPERMWI_CONST( 1, 0, 3 ,2 ) ), 
            __vmulfp( vDiffCenter[2*i+1], 
            __vpermwi( vDiffCenter[2*i+1], VPERMWI_CONST( 1, 0, 3, 2 ) ) ) );
    }
    __vector4 vCovariance       = __vaddfp( __vaddfp( vCovPartial[0], vCovPartial[1] ), 
        __vaddfp( vCovPartial[2], vCovPartial[3] ) );
    vCovariance                 = __vaddfp( vCovariance, __vpermwi( vCovariance, VPERMWI_CONST( 2, 3, 0, 1 ) ) );

    return vCovariance;
}


//---------------------------------------------------------------------------------------------------------
// Name: Find2BitLerpedColorsRGBBYTE( )
// Desc: Input is a vector containing both anchor colors as BYTEs.  The first anchor is repeated in 
// x and z, and the second anchors is repeated in y and w.  
//
// Output is a vector containing two interpolated values, 1/3 and 2/3 of the way between the anchors.
// The value repeated in x and z is:
//
// 1/3 anchor0 + 2/3 anchor1
//
// The value repeated in y and w is:
//
// 2/3 anchor0 + 1/3 anchor1
//
//---------------------------------------------------------------------------------------------------------
__forceinline __vector4 Find2BitLerpedColorsRGBBYTE( __vector4 vMergedAnchors )
{
    __vector4 vByteTwo          = __vspltisb( 2 );
    __vector4 vByteFour         = __vspltisb( 4 );
    __vector4 vByteFive         = __vspltisb( 5 );
    __vector4 vByteEight        = __vspltisb( 8 );
    __vector4 vByteSixteen      = __vaddubs( vByteEight, vByteEight );

    // Find palette colors explicitly, without overflowing bytes, and without multiplication or division.
    // 1/3 ~ 1/4 + 1/16 + 1/32, 2/3 = 1 - 1/3 
    // The decoding hardware makes these exact same approximations!
    // However, the rounding here is inexact, for speed. We could get more exact results by 
    // temporarily expanding to HALF representation.
    __vector4 vAnchorsOne4th    = __vsrb( __vaddubs( vMergedAnchors, vByteTwo ), vByteTwo );
    __vector4 vAnchorsOne16th   = __vsrb( __vaddubs( vMergedAnchors, vByteEight ), vByteFour );
    __vector4 vAnchorsOne32th   = __vsrb( __vaddubs( vMergedAnchors, vByteSixteen ), vByteFive );
    __vector4 vAnchorsOneThird  = __vaddubs( vAnchorsOne4th, 
        __vaddubs( vAnchorsOne16th, vAnchorsOne32th ) );
    __vector4 vAnchorsTwoThirds = __vsububs( vMergedAnchors, vAnchorsOneThird );
    __vector4 vAnchorsLerped    = __vaddubs( vAnchorsOneThird, 
        __vsldoi( vAnchorsTwoThirds, vAnchorsTwoThirds, 4 ) );

    return vAnchorsLerped;
}


//---------------------------------------------------------------------------------------------------------
// Name: Find2BitLerpedColorsUVBYTE( )
// Desc: Input is a vector containing both anchor colors as BYTEs.  The first anchor is repeated in 
// the 0-1 BYTEs of xyzw, and the second anchors is repeated in the 2-3 BYTEs of xyzw.  
//
// Output is a vector containing two interpolated values, 1/3 and 2/3 of the way between the anchors.
// The value repeated in 0-1 BYTEs of xyzw is:
//
// 1/3 anchor0 + 2/3 anchor1
//
// The value repeated in 2-3 BYTEs of xyzw is:
//
// 2/3 anchor0 + 1/3 anchor1
//
//---------------------------------------------------------------------------------------------------------
__forceinline __vector4 Find2BitLerpedColorsUVBYTE( __vector4 vMergedAnchors )
{
    __vector4 vByteTwo          = __vspltisb( 2 );
    __vector4 vByteFour         = __vspltisb( 4 );
    __vector4 vByteFive         = __vspltisb( 5 );
    __vector4 vByteEight        = __vspltisb( 8 );
    __vector4 vByteSixteen      = __vaddubs( vByteEight, vByteEight );

    // Find palette colors explicitly, without overflowing bytes, and without multiplication or division.
    // 1/3 ~ 1/4 + 1/16 + 1/32, 2/3 = 1 - 1/3 
    // The decoding hardware makes these exact same approximations!
    // However, the rounding here is inexact, for speed. We could get more exact results by 
    // temporarily expanding to HALF representation.
    __vector4 vAnchorsOne4th    = __vsrb( __vaddubs( vMergedAnchors, vByteTwo ), vByteTwo );
    __vector4 vAnchorsOne16th   = __vsrb( __vaddubs( vMergedAnchors, vByteEight ), vByteFour );
    __vector4 vAnchorsOne32th   = __vsrb( __vaddubs( vMergedAnchors, vByteSixteen ), vByteFive );
    __vector4 vAnchorsOneThird  = __vaddubs( vAnchorsOne4th, 
        __vaddubs( vAnchorsOne16th, vAnchorsOne32th ) );
    __vector4 vAnchorsTwoThirds = __vsububs( vMergedAnchors, vAnchorsOneThird );
    __vector4 vAnchorsLerped    = __vaddubs( vAnchorsOneThird, 
        __vsldoi( vAnchorsTwoThirds, vAnchorsTwoThirds, 2 ) );

    return vAnchorsLerped;
}


//---------------------------------------------------------------------------------------------------------
// Name: Find3BitLerpedColorsABYTE( )
// Desc: Input is a vector containing both anchor colors as BYTEs.  The first anchor is repeated in 
// the 0 & 2 BYTEs of xyzw, and the second anchors is repeated in the 1 & 3 BYTEs of xyzw.  
//
// Output is three vectors containing six interpolated values, 1/7, 2/7, 3/7, 4/7, 5/7 and 6/7 of the way 
// between the anchors.  The values in the the 0 & 2 BYTEs of xyzw are as follows.  (The values in 
// the 1 & 3 BYTEs of xyzw are the reverse of these.)
//
// vLerpedOne7th    = 1/7 anchor0 + 6/7 anchor1
// vLerpedTwo7th    = 2/7 anchor0 + 5/7 anchor1
// vLerpedThree7th  = 3/7 anchor0 + 4/7 anchor1
// vLerpedThree7th  = 4/7 anchor0 + 3/7 anchor1
// vLerpedTwo7th    = 5/7 anchor0 + 2/7 anchor1
// vLerpedOne7th    = 6/7 anchor0 + 1/7 anchor1
//
//---------------------------------------------------------------------------------------------------------
__forceinline void Find3BitLerpedColorsABYTE( __vector4 vMergedAnchors, __vector4& vLerpedOne7th, 
                                             __vector4& vLerpedTwo7th, __vector4& vLerpedThree7th )
{
    __vector4 vByteTwo          = __vspltisb( 2 );
    __vector4 vByteThree        = __vspltisb( 3 );
    __vector4 vByteFour         = __vspltisb( 4 );
    __vector4 vByteFive         = __vspltisb( 5 );
    __vector4 vByteSixteen      = __vslb( vByteTwo, vByteThree );

    // Find palette colors explicitly
    // 1/7 ~ 1/8 + 1/32, 6/7 = 1 - 1/7 
    // 2/7 ~ 1/4 + 1/32, 5/7 = 1 - 2/7 
    // 3/7 ~ 1/7 + 2/7, 4/7 = 1 - 3/7 
    // The decoding hardware makes these exact same approximations!
    // However, the rounding here is inexact, for speed. We could get more exact results by 
    // temporarily expanding to HALF representation.
    __vector4 vAnchorsOne4th    = __vsrb( __vaddubs( vMergedAnchors, vByteTwo ), vByteTwo );
    __vector4 vAnchorsOne8th    = __vsrb( __vaddubs( vMergedAnchors, vByteFour ), vByteThree );
    __vector4 vAnchorsOne32th   = __vsrb( __vaddubs( vMergedAnchors, vByteSixteen ), vByteFive );
    __vector4 vAnchorsOne7th    = __vaddubs( vAnchorsOne8th, vAnchorsOne32th );
    __vector4 vAnchorsTwo7th    = __vaddubs( vAnchorsOne4th, vAnchorsOne32th );
    __vector4 vAnchorsThree7th  = __vaddubs( vAnchorsOne7th, vAnchorsTwo7th );
    __vector4 vAnchorsFour7th   = __vsububs( vMergedAnchors, vAnchorsThree7th );
    __vector4 vAnchorsFive7th   = __vsububs( vMergedAnchors, vAnchorsTwo7th );
    __vector4 vAnchorsSix7th    = __vsububs( vMergedAnchors, vAnchorsOne7th );
    vLerpedOne7th               = __vaddubs( vAnchorsOne7th, 
        __vsldoi( vAnchorsSix7th, vAnchorsSix7th, 4 ) );
    vLerpedTwo7th               = __vaddubs( vAnchorsTwo7th, 
        __vsldoi( vAnchorsFive7th, vAnchorsFive7th, 4 ) );
    vLerpedThree7th             = __vaddubs( vAnchorsThree7th, 
        __vsldoi( vAnchorsFour7th, vAnchorsFour7th, 4 ) );
}


//---------------------------------------------------------------------------------------------------------
// Name: Find2BitPaletteIndicesFLOAT( )
// Desc: Input is 4 vectors containing 16 FLOAT dot products, and an increment, to normalize these 
// values to the range 0-3.
//
// Output is the normalized indices, packed as bits into:
//
// vPackedIndices:      [            | 31-24][            | 23-16][            | 15-8 ][            |  7-0 ]
//---------------------------------------------------------------------------------------------------------
__forceinline __vector4 Find2BitPaletteIndicesFLOAT( __vector4 vDotx4[4], __vector4 vStepInc )
{
    __vector4 vZero             = __vzero( );
    __vector4 vWordOne          = __vspltisw( 1 );
    __vector4 vWordTwo          = __vspltisw( 2 );
    __vector4 vWordThree        = __vspltisw( 3 );
    __vector4 vFloatOneHalf     = __vcfsx( vWordOne, 1 );

    __vector4 vPackedIndices    = vZero;
    for ( UINT i = 0; i < 4; ++i )
    {
        // This calculation assumes no pixels are outside the range of the anchor colors.
        // Otherwise need a max/min.
        __vector4 vPalFloat     = __vmaddfp( vDotx4[i], vStepInc, vFloatOneHalf );
        __vector4 vPalIndex     = __vand( vWordThree, __vctuxs( vPalFloat, 0 ));

        // Incoming index is quantized distance from vAnchor[0] to vAnchor[1]
        // The indexing in the DXT1 standard differs from this indexing as follows:
        //
        // Above:  0 means vAnchor[0]
        //         1 means lerp( vAnchor[0], vAnchor[1], 1/3 )
        //         2 means lerp( vAnchor[0], vAnchor[1], 2/3 )
        //         3 means vAnchor[1]
        //
        // DXT1:   0 means vAnchor[0]
        //         1 means vAnchor[1]
        //         2 means lerp( vAnchor[0], vAnchor[1], 1/3 )
        //         3 means lerp( vAnchor[0], vAnchor[1], 2/3 )
        // Implied remapping: 0/1/2/3 --> 0/2/3/1 
        //
        // Remapping in one instruction:
        static CONST __vector4i viIndexRemapping = {0x00020301, 0x00020301, 0x00020301, 0x00020301}; 
        __vector4 vRemappedIndex= __vperm( *(__vector4*)&viIndexRemapping, *(__vector4*)&viIndexRemapping, 
            vPalIndex );
        vPackedIndices          = __vor( __vslw( vPackedIndices, vWordTwo ), vRemappedIndex );
    }

    return vPackedIndices;
}


//---------------------------------------------------------------------------------------------------------
// Name: Find3BitPaletteIndicesFLOAT( )
// Desc: Input is 4 vectors containing 16 FLOAT differences, and an increment, to normalize these 
// values to the range 0-7.
//
// Output is the normalized indices, packed as bits into:
//
// vPackedIndices:      [            | 11-0 ][            | 23-12][            | 35-24][            | 47-36]
//---------------------------------------------------------------------------------------------------------
__forceinline __vector4 Find3BitPaletteIndicesFLOAT( __vector4 vDotx4[4], __vector4 vStepInc )
{
    __vector4 vZero             = __vzero( );
    __vector4 vWordOne          = __vspltisw( 1 );
    __vector4 vWordThree        = __vspltisw( 3 );
    __vector4 vWordSeven        = __vspltisw( 7 );
    __vector4 vFloatOneHalf     = __vcfsx( vWordOne, 1 );

    __vector4 vPackedIndices    = vZero;
    for ( UINT i = 0; i < 4; ++i )
    {
        // This calculation assumes no pixels are outside the range of the anchor colors.
        // Otherwise need a max/min.
        __vector4 vPalFloat     = __vmaddfp( vDotx4[i], vStepInc, vFloatOneHalf );
        __vector4 vPalIndex     = __vand( vWordSeven, __vctuxs( vPalFloat, 0 ));

        // Incoming index is quantized distance from vAnchor[0] to vAnchor[1]
        // The indexing in the DXT1 standard differs from this indexing as follows:
        //
        // Above:  0 means vAnchor[0]
        //         1 means lerp( vAnchor[0], vAnchor[1], 1/7 )
        //         2 means lerp( vAnchor[0], vAnchor[1], 2/7 )
        //         3 means lerp( vAnchor[0], vAnchor[1], 3/7 )
        //         4 means lerp( vAnchor[0], vAnchor[1], 4/7 )
        //         5 means lerp( vAnchor[0], vAnchor[1], 5/7 )
        //         6 means lerp( vAnchor[0], vAnchor[1], 6/7 )
        //         7 means vAnchor[1]
        //
        // DXT1:   0 means vAnchor[0]
        //         1 means vAnchor[1]
        //         2 means lerp( vAnchor[0], vAnchor[1], 1/7 )
        //         3 means lerp( vAnchor[0], vAnchor[1], 2/7 )
        //         4 means lerp( vAnchor[0], vAnchor[1], 3/7 )
        //         5 means lerp( vAnchor[0], vAnchor[1], 4/7 )
        //         6 means lerp( vAnchor[0], vAnchor[1], 5/7 )
        //         7 means lerp( vAnchor[0], vAnchor[1], 6/7 )
        // Implied remapping: 0/1/2/3/4/5/6/7 --> 0/2/3/4/5/6/7/1 
        //
        // Remapping in one instruction:
        static CONST __vector4i viIndexRemapping = {0x00020304, 0x05060701, 0x00020304, 0x05060701}; 
        __vector4 vRemappedIndex= __vperm( *(__vector4*)&viIndexRemapping, *(__vector4*)&viIndexRemapping, 
            vPalIndex );
        vPackedIndices          = __vor( __vslw( vPackedIndices, vWordThree ), vRemappedIndex );
    }

    return vPackedIndices;
}


//---------------------------------------------------------------------------------------------------------
// Name: CompressRGBBYTEFind4PaletteIndices( )
// Desc: This function was introduced as an expedient to force the compiler to unroll some loops in 
// the CompressRGBBlockVMXBYTE.  It contains the code to find 4 of the palette indices in an RGB block.
//
// vARGBx4T contains 4 block pixels, whose palette indices are to be found.
//
// vPaletteColors contains the four palette colors, in DXT1 order:
//
// vPaletteColors[0] = Anchor0
// vPaletteColors[1] = Anchor1
// vPaletteColors[2] = 2/3 * Anchor0 + 1/3 * Anchor1
// vPaletteColors[3] = 1/3 * Anchor0 + 2/3 * Anchor1
//
// vPackedIndices contains palette indices for the block, and this function shifts xyzw each 2 bits to
// the left, and inserts a new index in the lower bits.
//---------------------------------------------------------------------------------------------------------
__forceinline void CompressRGBBYTEFind4PaletteIndices( __vector4 vARGBx4T, __vector4 vPaletteColors[4], 
                                                    __vector4& vPackedIndices )
{
    __vector4 vZero             = __vzero();
    __vector4 vWordOne          = __vspltisw( 1 );
    __vector4 vWordTwo          = __vspltisw( 2 );
    __vector4 vWordThree        = __vspltisw( 3 );
    __vector4 vWordIndex[4]     = { vZero, 
        vWordOne, 
        vWordTwo, 
        vWordThree };

    // Find the 'sum over R,G,B of absolute diffs' for each pixel vs. each palette color
    // vARGBx4T         [ A0 | R0 | G0 | B0 ][ A1 | R1 | G1 | B1 ][ A2 | R2 | G2 | B2 ][ A3 | R3 | G3 | B3 ]
    __vector4 vSumAbsDiffx4[4];
    for( UINT j = 0; j < 4; ++j )
    {
        // vSumAbsDiffx4[j]:     
        //              [ x0 | x0 | x0 | x0 ][ x1 | x1 | x1 | x1 ][ x2 | x2 | x2 | x2 ][ x3 | x3 | x3 | x3 ]
        // where
        // xi           = abs( R0 - vPaletteColors[j].r )
        //              + abs( G0 - vPaletteColors[j].g )
        //              + abs( B0 - vPaletteColors[j].b )
        __vector4 vAbsDiff      = AbsDiffByte( vARGBx4T, vPaletteColors[j] );
        vSumAbsDiffx4[j]        = HorizSumRGBBytesInWord( vAbsDiff );
    }

    // Choose the best palette color for each pixel.  Note the "Real-Time DXT Compression" paper has
    // a much cleverer way of doing this in MMX/SSE using boolean operations.  In practice, however, the 
    // brute force approach using the __vsel instruction seems to be faster in VMX.
    __vector4 vMinSubAbsDiffx4 = vSumAbsDiffx4[0];
    __vector4 vPalIndexx4       = vZero;
    for( UINT j = 1; j < 4; ++j )
    {
        // vPalIndexx4  [ I0 | I0 | I0 | I0 ][ I1 | I1 | I1 | I1 ][ I2 | I2 | I2 | I2 ][ I3 | I3 | I3 | I3 ]
        // where vSumAbsDiffx4[Ii] is minimum for vSumAbsDiffx4 in given xyzw component.
        vMinSubAbsDiffx4        = __vminuw( vMinSubAbsDiffx4, vSumAbsDiffx4[j] );
        __vector4 vIsMin        = __vcmpequw( vMinSubAbsDiffx4, vSumAbsDiffx4[j] );
        vPalIndexx4             = __vsel( vPalIndexx4, vWordIndex[j], vIsMin );
    }

    // vPackedIndices   [             << |I0][             << |I1][             << |I2][             << |I3]
    vPackedIndices              = __vor( vPalIndexx4, __vslw( vPackedIndices, vWordTwo ) );
}


//---------------------------------------------------------------------------------------------------------
// Name: CompressUVBYTEFindPaletteIndices( )
// Desc: vUVUVx8[2] contains 16 block pixels as BYTEs, whose palette indices are to be found.
//
// vPaletteColors contains the four palette colors, in DXT1 order:
//
// vPaletteColors[0] = Anchor0
// vPaletteColors[1] = Anchor1
// vPaletteColors[2] = 2/3 * Anchor0 + 1/3 * Anchor1
// vPaletteColors[3] = 1/3 * Anchor0 + 2/3 * Anchor1
//
// vPackedIndices contains palette indices for the block, packed as:
//
// vPackedIndices:      [            | 31-24][            | 23-16][            | 15-8 ][            |  7-0 ]
//---------------------------------------------------------------------------------------------------------
__forceinline __vector4 CompressUVBYTEFindPaletteIndices( __vector4 vUVUVx8[2], __vector4 vPaletteUVs[4] )
{
    __vector4 vZero             = __vzero( );
    __vector4 vByteOne          = __vspltisb( 1 );
    __vector4 vByteTwo          = __vspltisb( 2 );
    __vector4 vByteThree        = __vspltisb( 3 );

    // Choose palette value which minimizes 'sum over U, V of absolute diffs'.
    // Manually unrolling this loop eliminated some register spilling to the stack.
    __vector4 vSumAbsDiffx8[2][4];
    vSumAbsDiffx8[0][0] = HorizSumBytesInHalf( AbsDiffByte( vUVUVx8[0], vPaletteUVs[0] ) );
    vSumAbsDiffx8[0][1] = HorizSumBytesInHalf( AbsDiffByte( vUVUVx8[0], vPaletteUVs[1] ) );
    vSumAbsDiffx8[0][2] = HorizSumBytesInHalf( AbsDiffByte( vUVUVx8[0], vPaletteUVs[2] ) );
    vSumAbsDiffx8[0][3] = HorizSumBytesInHalf( AbsDiffByte( vUVUVx8[0], vPaletteUVs[3] ) );
    vSumAbsDiffx8[1][0] = HorizSumBytesInHalf( AbsDiffByte( vUVUVx8[1], vPaletteUVs[0] ) );
    vSumAbsDiffx8[1][1] = HorizSumBytesInHalf( AbsDiffByte( vUVUVx8[1], vPaletteUVs[1] ) );
    vSumAbsDiffx8[1][2] = HorizSumBytesInHalf( AbsDiffByte( vUVUVx8[1], vPaletteUVs[2] ) );
    vSumAbsDiffx8[1][3] = HorizSumBytesInHalf( AbsDiffByte( vUVUVx8[1], vPaletteUVs[3] ) );
    __vector4 vSumAbsDiffx16[4];
    for( UINT j = 0; j < 4; ++j )
    {
        vSumAbsDiffx16[j]       = __vpkuhum( vSumAbsDiffx8[0][j], vSumAbsDiffx8[1][j] );
    }

    __vector4 vByteIndex[4]     = { vZero, 
        vByteOne, 
        vByteTwo, 
        vByteThree };
    __vector4 vMinSubAbsDiffx16 = vSumAbsDiffx16[0];
    __vector4 vPackedIndices  = vZero;
    for( UINT j = 1; j < 4; ++j )
    {
        vMinSubAbsDiffx16       = __vminub( vMinSubAbsDiffx16, vSumAbsDiffx16[j] );
        __vector4 vIsMin        = __vcmpequb( vMinSubAbsDiffx16, vSumAbsDiffx16[j] );
        vPackedIndices          = __vsel( vPackedIndices, vByteIndex[j], vIsMin );
    }

    return vPackedIndices;
}


//---------------------------------------------------------------------------------------------------------
// Name: CompressABYTEFindPaletteIndices( )
// Desc: vAlphax16 contains 16 block pixels as BYTEs, whose palette indices are to be found.
//
// vPaletteColors contains the eight palette colors, in DXT1 order:
//
// vPaletteColors[0] = Anchor0
// vPaletteColors[1] = Anchor1
// vPaletteColors[2] = 6/7 * Anchor0 + 1/7 * Anchor1
// vPaletteColors[3] = 5/7 * Anchor0 + 2/7 * Anchor1
// vPaletteColors[4] = 4/7 * Anchor0 + 3/7 * Anchor1
// vPaletteColors[5] = 3/7 * Anchor0 + 4/7 * Anchor1
// vPaletteColors[6] = 2/7 * Anchor0 + 5/7 * Anchor1
// vPaletteColors[7] = 1/7 * Anchor0 + 6/7 * Anchor1
//
// vPackedIndices contains palette indices for the block, packed as:
//
// vPackedIndices:      [            | 11-0 ][            | 23-12][            | 35-24][            | 47-36]
//---------------------------------------------------------------------------------------------------------
__forceinline __vector4 CompressABYTEFindPaletteIndices( __vector4 vAlphax16, __vector4 vPaletteColors[8] )
{
    __vector4 vZero             = __vzero( );
    __vector4 vByteOne          = __vspltisb( 1 );
    __vector4 vByteTwo          = __vspltisb( 2 );
    __vector4 vByteThree        = __vspltisb( 3 );
    __vector4 vByteFour         = __vspltisb( 4 );
    __vector4 vByteFive         = __vspltisb( 5 );
    __vector4 vByteSix          = __vspltisb( 6 );
    __vector4 vByteSeven        = __vspltisb( 7 );

    // Find index which minimizes diff with source value
    __vector4 vByteIndex[8]     = { vZero, 
        vByteOne, 
        vByteTwo, 
        vByteThree, 
        vByteFour, 
        vByteFive, 
        vByteSix, 
        vByteSeven, };
    __vector4 vAbsDiffx16[8];
    for( UINT j = 0; j < 8; ++j )
    {
        vAbsDiffx16[j]          = AbsDiffByte( vAlphax16, vPaletteColors[j] );
    }

    __vector4 vMinAbsDiffx16    = vAbsDiffx16[0];
    __vector4 vPackedIndices    = vZero;
    for( UINT j = 0; j < 8; ++j )
    {
        __vector4 vIsMin        = __vcmpgtub( vMinAbsDiffx16, vAbsDiffx16[j] );
        vPackedIndices          = __vsel( vPackedIndices, vByteIndex[j], vIsMin );
        vMinAbsDiffx16          = __vminub( vMinAbsDiffx16, vAbsDiffx16[j] );
    }

    return vPackedIndices;
}


//---------------------------------------------------------------------------------------------------------
// Name: Pack8888To32( )
// Desc: On input, vPackedIndices contains palette indices for the block, packed as:
//
// vPackedIndices:      [             | 15-8][             |  7-0][             |31-24][             |23-16]
//
// On output, vPackedIndices contains palette indices for the block, packed as:
//
// vPackedIndices:      [  15-0   |  31-16  ][  15-0   |  31-16  ][  15-0   |  31-16  ][  15-0   |  31-16  ]
//---------------------------------------------------------------------------------------------------------
__forceinline __vector4 Pack8888To32( __vector4 vPackedIndices )
{
    vPackedIndices              = __vpkuhum( vPackedIndices, vPackedIndices );
    vPackedIndices              = __vpkuhum( vPackedIndices, vPackedIndices );

    return vPackedIndices;
}


//---------------------------------------------------------------------------------------------------------
// Name: PackAnchorsAnd12121212To64( )
// Desc: On input, vPackedIndices contains palette indices for the block, packed as:
//
// vPackedIndices:      [            | 11-0 ][            | 23-12][            | 35-24][            | 47-36]
// 
// And vPackedAnchors contains block anchors, packed as:
//
// vPackedAnchors:      [                   ][         | a1 | a0 ][                   ][         | a1 | a0 ]
//
// On output, vPackedBlock contains anchors and indices for the block, packed as:
//
// vPackedBlock:        [ a1 | a0 |  15-0   ][  31-16  |  47-32  ][ a1 | a0 |  15-0   ][  31-16  |  47-32  ]
//---------------------------------------------------------------------------------------------------------
__forceinline __vector4 PackAnchorsAnd12121212To64( __vector4 vPackedAnchors, __vector4 vPackedIndices )
{
    __vector4 vWordOne          = __vspltisw( 1 );
    __vector4 vWordEight        = __vspltisw( 8 );
    __vector4 vWordSixteen      = __vadduws( vWordEight, vWordEight );
    __vector4 vWord255          = __vsubuws( __vslw( vWordOne, vWordEight ), vWordOne );
    __vector4 vWord8to15Bits    = __vslw( vWord255, vWordSixteen );

    // Currently we have this:
    // vPackedAnchors:  [                   ][         | a1 | a0 ][                   ][         | a1 | a0 ]
    // vPackedIndices:  [            | 11-0 ][            | 23-12][            | 35-24][            | 47-36]
    //
    // To determine endianness, let's consider the 48 bits of indices broken into 8-bit chunks.
    //
    //          Bits:   48 -------------------------------------------------------------- 0
    //                   [ 47 - 40 ][ 39 - 32 ][ 31 - 24 ][ 23 - 16 ][ 15 -  8 ][  7 -  0 ]
    //
    // Because the Xbox 360 CPU is big-endian, the order becomes:
    //
    //          Bits:   48 -------------------------------------------------------------- 0
    //                   [  7 -  0 ][ 15 -  8 ][ 23 - 16 ][ 31 - 24 ][ 39 - 32 ][ 47 - 40 ]
    //
    // D3DFMT_DXT5A uses an endianness of GPUENDIAN_8IN16 for legacy reasons.  Therefore, the order becomes: 
    //
    //          Bits:   48 -------------------------------------------------------------- 0
    //                   [ 15 -  8 ][  7 -  0 ][ 31 - 24 ][ 23 - 16 ][ 47 - 40 ][ 39 - 32 ]
    //
    // Therefore the end result we hope for is:
    //
    // Block:           [ a1 | a0 |  15-0   ][  31-16  |  47-32  ][ a1 | a0 |  15-0   ][  31-16  |  47-32  ]
    //
    static CONST __vector4i viRotateCounts =  {0x00000000, 0x00000000c, 0x00000018, 0x00000004};
    __vector4 vRotatedIndices   = __vrlw( vPackedIndices, *( __vector4* )viRotateCounts );
    // Now we have this:
    // vRotatedIndices: [           | 11-0  ][   | 23-12 |       ][-24 |          | 35][       | 47-36 |   ]

    __vector4 vIndicesBy24s     = __vor( vRotatedIndices, __vrlimi( vIndicesBy24s, vRotatedIndices, 0xf, 3 ));
    // Now we have this:
    // vIndicesBy24s:   [    garbage        ][   |      23-0     ][      garbage      ][-24 |  |    47-    ]

    __vector4 vPackedWord0      = __vmrghh( vPackedAnchors, vIndicesBy24s );
    // Now we have this:
    // vPackedWord0:    [      garbage      ][      garbage      ][      garbage      ][ a1 | a0 |  15-0   ]

    __vector4 vPackedWord1      = __vor( vIndicesBy24s, 
        __vrlimi( vPackedWord1, __vand( vIndicesBy24s, vWord8to15Bits ), 0xf, 2 ));
    // Now we have this:
    // vPackedWord1:    [      garbage      ][      garbage      ][      garbage      ][  31-16  |  47-32  ]

    __vector4 vPackedBlock      = __vmrglw( 
        __vmrglw( vPackedWord0, vPackedWord0 ), 
        __vmrglw( vPackedWord1, vPackedWord1 ));
    // vPackedBlock:    [ a1 | a0 |  15-0   ][  31-16  |  47-32  ][ a1 | a0 |  15-0   ][  31-16  |  47-32  ]

    return vPackedBlock;
}


//---------------------------------------------------------------------------------------------------------
// Name: CompressRGBBlockVMXFLOAT( )
// Desc: Encode a single 64-bit RGB block, as part of either a DXT1 or a DXT4/5 texture.  
//
// Input is 4 vectors, each interpreted as 16-wide BYTEs.  Input ordering is the following in 
// texture coordinates:
//
// -----------------
// | 0 | 1 | 2 | 3 |
// -----------------
// | 4 | 5 | 6 | 7 |
// -----------------
// | 8 | 9 | a | b |
// -----------------
// | c | d | e | f |
// -----------------
//
// The input data is expanded to FLOAT, and much of the calculation uses FLOAT VMX intrinsics.
//
// We do not handle DXT1 blocks with 1-bit alpha.  This may be useful to implement in some cases, 
// but it would severely impact performance for non-alpha textures.
//
// This routine currently compiles to code with no branches and no writes to memory, other than 
// the 64-bit output.  Both of these are important for performance.
//---------------------------------------------------------------------------------------------------------
__forceinline VOID CompressRGBBlockVMXFLOAT( BYTE *pDst, 
                                          __vector4 vMin, 
                                          __vector4 vMax, 
                                          __vector4 vSrc0123, 
                                          __vector4 vSrc4567, 
                                          __vector4 vSrc89ab, 
                                          __vector4 vSrccdef )
{
    // Load needed constants.  Note most of these operations get scheduled during 
    // pipeline stalls, and are therefore effectively free.
    __vector4 vZero             = __vzero( );
    __vector4 vWordOne          = __vspltisw( 1 );
    __vector4 vWordFour         = __vspltisw( 4 );
    __vector4 vWordSixteen      = __vslw( vWordOne, vWordFour );
    __vector4 vFloatOneHalf     = __vcfsx( vWordOne, 1 );
    __vector4 vFloatOne         = __vaddfp( vFloatOneHalf, vFloatOneHalf );
    __vector4 vFloatTwo         = __vaddfp( vFloatOne, vFloatOne );
    __vector4 vFloatThree       = __vaddfp( vFloatTwo, vFloatOne );

    // Unpack to full float.  Note that VPACK_D3DCOLOR has an implicit endian swap
    // from ARGB to RGBA.
    __vector4 vARGBx4[4] = { vSrc0123, vSrc4567, vSrc89ab, vSrccdef };
    __vector4 vRGBA[16];
    ExpandBYTEToFLOATRGBA( vRGBA, vARGBx4 );
    // vRGBA[ 0]        [         R0        ][         G0        ][         B0        ][         A0        ]
    // vRGBA[ 1]        [         R1        ][         G1        ][         B1        ][         A1        ]
    // vRGBA[ 2]        [         R2        ][         G2        ][         B2        ][         A2        ]
    // vRGBA[ 3]        [         R3        ][         G3        ][         B3        ][         A3        ]
    // vRGBA[ 4]        [         R4        ][         G4        ][         B4        ][         A4        ]
    // vRGBA[ 5]        [         R5        ][         G5        ][         B5        ][         A5        ]
    // vRGBA[ 6]        [         R6        ][         G6        ][         B6        ][         A6        ]
    // vRGBA[ 7]        [         R7        ][         G7        ][         B7        ][         A7        ]
    // vRGBA[ 8]        [         R8        ][         G8        ][         B8        ][         A8        ]
    // vRGBA[ 9]        [         R9        ][         G9        ][         B9        ][         A9        ]
    // vRGBA[10]        [         Ra        ][         Ga        ][         Ba        ][         Aa        ]
    // vRGBA[11]        [         Rb        ][         Gb        ][         Bb        ][         Ab        ]
    // vRGBA[12]        [         Rc        ][         Gc        ][         Bc        ][         Ac        ]
    // vRGBA[13]        [         Rd        ][         Gd        ][         Bd        ][         Ad        ]
    // vRGBA[14]        [         Re        ][         Ge        ][         Be        ][         Ae        ]
    // vRGBA[15]        [         Rf        ][         Gf        ][         Bf        ][         Af        ]

    __vector4 vMinFLOAT, vMaxFLOAT;
    ExpandBYTEToFLOAT<FALSE>( vMinFLOAT, vMin );
    ExpandBYTEToFLOAT<FALSE>( vMaxFLOAT, vMax );
    __vector4 vCenter           = __vmaddfp( vFloatOneHalf, vMinFLOAT, __vmulfp( vFloatOneHalf, vMaxFLOAT ));

    // Find the covariances between R and G, B and G.  Check whether each is positive or negative
    // vAnchorMask has these meanings:
    //  .x == 0xff <--> R and G are negatively correlated
    //  .y == 0xff <--> G and G are negatively correlated (never)
    //  .z == 0xff <--> B and G are negatively correlated
    // We only need 2 bits of information to choose from among the 4 diagonals.  
    // The mostly visually important are those involving G.  
    __vector4 vCovariance       = FindCovarianceRGB( vRGBA, vCenter );
    __vector4 vAnchorMask       = __vcmpgefp( vZero, vCovariance );

    // At this point, vAnchorMask is 0x00 for components of vAnchor[0] which should equal 
    // vMin, and 0xff for components of vAnchor[0] which should equal vMax.
    __vector4 vAnchorFLOAT[2];
    vAnchorFLOAT[0]             = __vsel( vMinFLOAT, vMaxFLOAT, vAnchorMask );
    vAnchorFLOAT[1]             = __vsel( vMaxFLOAT, vMinFLOAT, vAnchorMask );

    // These are floating point 31.0, 63.0, 31.0
    static CONST __vector4i vi565Scale =  {0x41f80000, 0x427c0000, 0x41f80000, 0x00000000};

    // These are bit shifts for the anchor colors.
    // These swap red and blue as well.
    static CONST __vector4i vi565Shift =  {0x0000000b, 0x00000005, 0x00000000, 0x00000000};

    // Find the packed anchors.  Note experiments indicate that it is slightly faster to generate 
    // these from the FLOAT anchors than from the BYTE anchors.
    __vector4 vAnchor565[2];
    for ( UINT k = 0; k < 2; ++k )
    {
        // vAnchorFLOAT[k]
        //              [         R         ][         G         ][         B         ][         A         ]
        vAnchor565[k]           = __vmaddfp( vAnchorFLOAT[k], *( __vector4* )&vi565Scale, vFloatOneHalf );
        vAnchor565[k]           = __vctuxs( vAnchor565[k], 0 );

        assert( vAnchor565[k].u[0] < 32 && vAnchor565[k].u[1] < 64 && vAnchor565[k].u[2] < 32 );

        vAnchor565[k]           = __vslw( vAnchor565[k], *( __vector4* )&vi565Shift );
        // vAnchor565[k]
        //              [          |R5|     ][             |G6|  ][                |B5][                   ]

        vAnchor565[k]           = __vor( __vspltw( vAnchor565[k], 0 ), 
            __vor( __vspltw( vAnchor565[k], 1 ), 
            __vspltw( vAnchor565[k], 2 )) );
        // vAnchor565[k]
        //              [          |R5|G6|B5][          |R5|G6|B5][          |R5|G6|B5][          |R5|G6|B5]
    }

    // Switch the anchors if they would indicate an alpha block.
    // If the anchors are equal, write all zeros to the palette later.
    __vector4 vAnchorsEqual     = __vcmpequw( vAnchor565[1], vAnchor565[0] );
    __vector4 vAnchorsSwapped   = __vcmpgtuw( vAnchor565[1], vAnchor565[0] );
    ConditionalExchange( vAnchor565[0], vAnchor565[1], vAnchorsSwapped );
    ConditionalExchange( vAnchorFLOAT[0], vAnchorFLOAT[1], vAnchorsSwapped );

    // Put the packed anchors in the x component of a vector
    __vector4 vPackedAnchors    = __vor( __vslw( vAnchor565[0], vWordSixteen ), vAnchor565[1] );
    // vPackedAnchors   [ R5|G6|B5| r5|g6|b5][ R5|G6|B5| r5|g6|b5][ R5|G6|B5| r5|g6|b5][ R5|G6|B5| r5|g6|b5]

    // $TODO: palettize to the expanded 5:6:5 anchors, not the raw anchors?
    // For each input color, find the closest representable step along the line joining the anchors.
    __vector4 vDiag             = __vsubfp( vAnchorFLOAT[1], vAnchorFLOAT[0] );
    __vector4 vDot[16];
    for ( UINT i = 0; i < 16; ++i )
    {
        __vector4 vDiff         = __vsubfp( vRGBA[i], vAnchorFLOAT[0] );
        vDot[i]                 = __vmsum3fp( vDiff, vDiag );
    }

    // Condense 16 dot products from 16 vectors into 4 and 'transpose' so that elements are in 
    // the desired output order, reading down each column and then over to the right.  A 'column-major'
    // order is necessary, because the values in each column will end up consecutive in the output.
    //
    // Here is the correct arrangement:
    //
    //  Elements 7-4   are in the .x components
    //  Elements 3-0   are in the .y components
    //  Elements 15-12 are in the .z components
    //  Elements 11-8  are in the .w components
    //
    //  vDotx4[0]       [        dot7       ][        dot3       ][        dotf       ][        dotb       ]
    //  vDotx4[1]       [        dot6       ][        dot2       ][        dote       ][        dota       ]
    //  vDotx4[2]       [        dot5       ][        dot1       ][        dotd       ][        dot9       ]
    //  vDotx4[3]       [        dot4       ][        dot0       ][        dotc       ][        dot8       ]
    //
    // This swizzle order matches the DXT standard, Xbox CPU conventions, and D3DFMT_DXT1 endianness, 
    // as follows:
    //
    // DXT standard is that the 0th input goes in the lowest order bits, and so forth.  This implies
    // an order of:
    // 
    //          Bits:   31 ------------------------------------------ 0
    //                  15|14|13|12|11|10| 9| 8| 7| 6| 5| 4| 3| 2| 1| 0
    //
    // Because the Xbox 360 CPU is big-endian, the order becomes:
    //
    //          Bits:   31 ------------------------------------------ 0
    //                   3| 2| 1| 0| 7| 6| 5| 4|11|10| 9| 8|15|14|13|12
    //
    // D3DFMT_DXT1 uses an endianness of GPUENDIAN_8IN16 for legacy reasons.  Therefore, the order becomes: 
    //
    //          Bits:   31 ------------------------------------------ 0
    //                   7| 6| 5| 4| 3| 2| 1| 0|15|14|13|12|11|10| 9| 8
    //
    __vector4 vDotx4[4];
    vDotx4[0]                   = __vmrglw(
        __vmrglw( vDot[ 7], vDot[15] ), 
        __vmrglw( vDot[ 3], vDot[11] ));
    vDotx4[1]                   = __vmrglw(             
        __vmrglw( vDot[ 6], vDot[14] ), 
        __vmrglw( vDot[ 2], vDot[10] ));
    vDotx4[2]                   = __vmrglw(             
        __vmrglw( vDot[ 5], vDot[13] ), 
        __vmrglw( vDot[ 1], vDot[ 9] ));
    vDotx4[3]                   = __vmrglw(             
        __vmrglw( vDot[ 4], vDot[12] ), 
        __vmrglw( vDot[ 0], vDot[ 8] ));

    // Convert each dot product into an integral number of steps of 1/3 the distance between anchors.
    __vector4 vStepInc          = __vmulfp( vFloatThree, __vrefp( __vmsum3fp( vDiag, vDiag )) );
    __vector4 vPackedIndices = Find2BitPaletteIndicesFLOAT( vDotx4, vStepInc );

    // Currently we have this:
    // vPackedIndices:  [             | 15-8][             |  7-0][             |31-24][             |23-16]
    // We want this:
    // vPackedIndices:  [  15-0   |  31-16  ][  15-0   |  31-16  ][  15-0   |  31-16  ][  15-0   |  31-16  ]
    vPackedIndices = Pack8888To32( vPackedIndices );

    // Special case to avoid alpha pixels when anchors are the same
    vPackedIndices              = __vsel( vPackedIndices, vZero, vAnchorsEqual );

    // Store 64 bits to possibly unaligned, possibly non-cacheable address
    __stvewx( vPackedAnchors, pDst, 0 );
    __stvewx( vPackedIndices, pDst, 4 );

    // Non-alpha block
    assert( (( WORD* )pDst )[0] > ( (WORD* )pDst )[1]
    || ( (( WORD* )pDst )[0] == ( (WORD* )pDst )[1] && ( (DWORD* )pDst )[1] == 0 ));
}


//---------------------------------------------------------------------------------------------------------
// Name: CompressUVBlockVMXFLOAT( )
// Desc: Encode a single 64-bit UV block, as part of a CTX1 texture.  
//
// Input is 4 vectors, each interpreted as 16-wide BYTEs, of which only the first 8 bytes belong
// to this block.  Input ordering is the following in texture coordinates:
//
// -----------------
// | 0 | 1 | 2 | 3 |
// -----------------
// | 4 | 5 | 6 | 7 |
// -----------------
// | 8 | 9 | a | b |
// -----------------
// | c | d | e | f |
// -----------------

// The input data is expanded to FLOAT, and much of the calculation uses FLOAT VMX intrinsics.
//
// This routine currently compiles to code with no branches and no writes to memory, other than 
// the 64-bit output.  Both of these are important for performance.
//---------------------------------------------------------------------------------------------------------
__forceinline VOID CompressUVBlockVMXFLOAT( BYTE *pDst, 
                                         __vector4 vMin, 
                                         __vector4 vMax, 
                                         __vector4 vSrc0123, 
                                         __vector4 vSrc4567, 
                                         __vector4 vSrc89ab, 
                                         __vector4 vSrccdef )
{
    // Load needed constants.  Note most of these operations get scheduled during 
    // pipeline stalls, and are therefore effectively free.
    __vector4 vZero             = __vzero( );
    __vector4 vWordOne          = __vspltisw( 1 );
    __vector4 vWordThree        = __vspltisw( 3 );
    __vector4 vWordEight        = __vspltisw( 8 );
    __vector4 vWord255          = __vsubuws( __vslw( vWordOne, vWordEight ), vWordOne );
    __vector4 vFloatOneHalf     = __vcfsx( vWordOne, 1 );
    __vector4 vFloatThree       = __vcfsx( vWordThree, 0 );
    __vector4 vFloatSix         = __vaddfp( vFloatThree, vFloatThree );
    __vector4 vFloat255         = __vcfsx( vWord255, 0 );

    // Unpack to full float.  Note that VPACK_D3DCOLOR has an implicit endian swap
    // from ARGB to RGBA, and therefore we must either spend instructions to undo
    // this swap, or work in the order: ( V0, U1, V1, U0 ).
    // For clarity, we choose to spend the instructions to regain the normal order.
    __vector4 vUVUVx4[4] = { vSrc0123, vSrc4567, vSrc89ab, vSrccdef };
    __vector4 vUVUV[8];
    ExpandBYTEToFLOATUVUV( vUVUV, vUVUVx4 );

    __vector4 vMinFLOAT, vMaxFLOAT;
    ExpandBYTEToFLOAT<TRUE>( vMinFLOAT, vMin );
    ExpandBYTEToFLOAT<TRUE>( vMaxFLOAT, vMax );

    // Find the block center
    __vector4 vCenter           = __vmaddfp( vFloatOneHalf, vMinFLOAT, __vmulfp( vFloatOneHalf, vMaxFLOAT ));

    // Anchors are endpoints of one of the two diagonals of the bounding box.  Which 
    // diagonal is determined by the sign of the covariance.
    // The actual covariance uses the mean, but we substitute the center.  
    __vector4 vCovariance = FindCovarianceUV( vUVUV, vCenter );
    __vector4 vAnchorTest       = __vcmpgefp( vZero, vCovariance );
    __vector4 vAnchorMask       = __vmrglw( vAnchorTest, vZero );

    // At this point, vAnchorMask is 0x00 for components of vAnchor[0] which should equal 
    // vMin, and 0xff for components of vAnchor[0] which should equal vMax.
    __vector4 vAnchor[2];
    vAnchor[0]                  = __vsel( vMinFLOAT, vMaxFLOAT, vAnchorMask );
    vAnchor[1]                  = __vsel( vMaxFLOAT, vMinFLOAT, vAnchorMask );

    // Find the packed anchors.  Note experiments indicate that it is slightly faster to generate 
    // these from the FLOAT anchors than from the BYTE anchors.
    __vector4 vWordAnchor[2];
    for ( UINT k = 0; k < 2; ++k )
    {
        vWordAnchor[k]          = __vctuxs( __vmaddfp( vAnchor[k], 
            *( __vector4* )&vFloat255, vFloatOneHalf ), 0 );
    }

    // Due to D3D endianness pattern for D3DFMT_CTX1, v comes before u.
    // vWordAnchor[0]:  [              | u0 ][              | v0 ][              | u0 ][              | v0 ]
    // vWordAnchor[1]:  [              | u1 ][              | v1 ][              | u1 ][              | v1 ]
    // vPackedAnchors:  [ v0 | u0 | v1 | u1 ][ v0 | u0 | v1 | u1 ][ v0 | u0 | v1 | u1 ][ v0 | u0 | v1 | u1 ]
    static CONST __vector4i viAnchorPerm =  {0x07031713, 0x07031713, 0x07031713, 0x07031713};
    __vector4 vPackedAnchors    = __vperm( vWordAnchor[0], vWordAnchor[1], *( __vector4* )viAnchorPerm ); 

    // $TODO: palettize to the expanded 8:8 anchors, not the raw anchors?
    // For each input vector, find the closest representable step along the line joining the anchors.
    __vector4 vDiag             = __vsubfp( vAnchor[1], vAnchor[0] );
    __vector4 vDotx2[8];
    for ( UINT i = 0; i < 8; ++i )
    {
        __vector4 vDiffUVUV     = __vsubfp( vUVUV[i], vAnchor[0] );
        __vector4 vDiffDiagUVUV = __vmulfp( vDiffUVUV, vDiag );
        __vector4 vDiffDiagVUVU = __vpermwi( vDiffDiagUVUV, VPERMWI_CONST( 1, 0, 3, 2 ));
        vDotx2[i]              = __vaddfp( vDiffDiagUVUV, vDiffDiagVUVU );
    }

    // We currently have this arrangement:
    //
    // vDotx2[0]       [        dot0       ][        dot0       ][        dot1       ][        dot1       ]
    // vDotx2[1]       [        dot2       ][        dot2       ][        dot3       ][        dot3       ]
    // vDotx2[2]       [        dot4       ][        dot4       ][        dot5       ][        dot5       ]
    // vDotx2[3]       [        dot6       ][        dot6       ][        dot7       ][        dot7       ]
    // vDotx2[4]       [        dot8       ][        dot8       ][        dot9       ][        dot9       ]
    // vDotx2[5]       [        dota       ][        dota       ][        dotb       ][        dotb       ]
    // vDotx2[6]       [        dotc       ][        dotc       ][        dotd       ][        dotd       ]
    // vDotx2[7]       [        dote       ][        dote       ][        dotf       ][        dotf       ]
    //
    // We need to condense 16 dot products from 8 vectors into 4 and 'transpose' so that elements are in 
    // the desired output order, reading down each column and then over to the right.  A 'column-major'
    // order is necessary, because the values in each column will end up consecutive in the output.
    //
    // Here is the correct arrangement:
    //
    //  Elements 7-4   are in the .x components
    //  Elements 3-0   are in the .y components
    //  Elements 15-12 are in the .z components
    //  Elements 11-8  are in the .w components
    //
    //  vDotx4[0]       [        dot7       ][        dot3       ][        dotf       ][        dotb       ]
    //  vDotx4[1]       [        dot6       ][        dot2       ][        dote       ][        dota       ]
    //  vDotx4[2]       [        dot5       ][        dot1       ][        dotd       ][        dot9       ]
    //  vDotx4[3]       [        dot4       ][        dot0       ][        dotc       ][        dot8       ]
    //
    // This swizzle order matches the DXT standard, Xbox CPU conventions, and D3DFMT_DXT1 endianness, 
    // as follows:
    //
    // DXT standard is that the 0th input goes in the lowest order bits, and so forth.  This implies
    // an order of:
    // 
    //          Bits:   31 ------------------------------------------ 0
    //                  15|14|13|12|11|10| 9| 8| 7| 6| 5| 4| 3| 2| 1| 0
    //
    // Because the Xbox 360 CPU is big-endian, the order becomes:
    //
    //          Bits:   31 ------------------------------------------ 0
    //                   3| 2| 1| 0| 7| 6| 5| 4|11|10| 9| 8|15|14|13|12
    //
    // D3DFMT_DXT1 uses an endianness of GPUENDIAN_8IN16 for legacy reasons.  Therefore, the order becomes: 
    //
    //          Bits:   31 ------------------------------------------ 0
    //                   7| 6| 5| 4| 3| 2| 1| 0|15|14|13|12|11|10| 9| 8
    //
    __vector4 vTempTranspose[4];
    vTempTranspose[0]           = __vrlimi( vDotx2[0], vDotx2[4], 0x5, 3 );   //  0| 8| 1| 9
    vTempTranspose[1]           = __vrlimi( vDotx2[1], vDotx2[5], 0x5, 3 );   //  2|10| 3|11
    vTempTranspose[2]           = __vrlimi( vDotx2[2], vDotx2[6], 0x5, 3 );   //  4|12| 5|13
    vTempTranspose[3]           = __vrlimi( vDotx2[3], vDotx2[7], 0x5, 3 );   //  6|14| 7|15
    __vector4 vDotx4[4];
    vDotx4[0]                  = __vmrglw( vTempTranspose[3], vTempTranspose[1] );
    vDotx4[1]                  = __vmrghw( vTempTranspose[3], vTempTranspose[1] );
    vDotx4[2]                  = __vmrglw( vTempTranspose[2], vTempTranspose[0] );
    vDotx4[3]                  = __vmrghw( vTempTranspose[2], vTempTranspose[0] );

    // The 4-element dot product is twice the 2-element dot product, since vDiag.xy == vDiag.zw .
    // Therefore, we use vFloatSix rather than vFloatThree.
    __vector4 vStepInc          = __vmulfp( vFloatSix, __vrefp( __vmsum4fp( vDiag, vDiag )) );
    __vector4 vPackedIndices = Find2BitPaletteIndicesFLOAT( vDotx4, vStepInc );

    // Currently we have this:
    // vPackedIndices:  [             | 15-8][             |  7-0][             |31-24][             |23-16]
    // We want this:
    // vPackedIndices:  [  15-0   |  31-16  ][  15-0   |  31-16  ][  15-0   |  31-16  ][  15-0   |  31-16  ]
    vPackedIndices = Pack8888To32( vPackedIndices );

    // Store 64 bits to possibly unaligned, possibly non-cacheable address
    __stvewx( vPackedAnchors, pDst, 0 );
    __stvewx( vPackedIndices, pDst, 4 );
}


//---------------------------------------------------------------------------------------------------------
// Name: CompressABlockVMXFLOAT( )
// Desc: Encode a single 64-bit Alpha block, as part of either a DXT4/5 or a DXN texture.  This routine
// could also be used to generate 1-channel DXT5A textures (not implemented).  
//
// Input is 4 vectors, each interpreted as 16-wide BYTEs.  Input ordering is the following in texture 
// coordinates:
//
// -----------------
// | 0 | 1 | 2 | 3 |
// -----------------
// | 4 | 5 | 6 | 7 |
// -----------------
// | 8 | 9 | a | b |
// -----------------
// | c | d | e | f |
// -----------------
//
// The input data is expanded to FLOAT, and much of the calculation uses FLOAT VMX intrinsics.
//
// We do not handle Alpha blocks with explicit 0 and 1 (transparent/opaque) encoding.  This may be useful 
// to implement in some cases, but it would severely impact performance.
//
// This routine currently compiles to code with no branches and no writes to memory, other than 
// the 64-bit output.  Both of these are important for performance.
//---------------------------------------------------------------------------------------------------------
template< UINT t_iChannel >
__forceinline VOID CompressABlockVMXFLOAT( BYTE *pDst, 
                                        __vector4 vMin, 
                                        __vector4 vMax, 
                                        __vector4 vSrc0123, 
                                        __vector4 vSrc4567, 
                                        __vector4 vSrc89ab, 
                                        __vector4 vSrccdef )
{
    // Load needed constants.  Note most of these operations get scheduled during 
    // pipeline stalls, and are therefore effectively free.
    __vector4 vWordOne          = __vspltisw( 1 );
    __vector4 vWordSeven        = __vspltisw( 7 );
    __vector4 vWordEight        = __vspltisw( 8 );
    __vector4 vWord255          = __vsubuws( __vslw( vWordOne, vWordEight ), vWordOne );
    __vector4 vFloatOneHalf     = __vcfsx( vWordOne, 1 );
    __vector4 vFloatSeven       = __vcfsx( vWordSeven, 0 );
    __vector4 vFloat255         = __vcfsx( vWord255, 0 );

    __vector4 vARGBx4[4] = { vSrc0123, vSrc4567, vSrc89ab, vSrccdef };

    // Merge all 16 Alpha values into 1 VMX register as BYTEs
    __vector4 vAlphax16;
    SelectOneChannelFromFour<TRUE, t_iChannel>( vAlphax16, vARGBx4 );

    SelectOneChannelFromFour<t_iChannel>( vMin, vMin );
    SelectOneChannelFromFour<t_iChannel>( vMax, vMax );

    // Unpack to full float.  Note that VPACK_D3DCOLOR has an implicit endian swap
    // from ARGB to RGBA, and therefore we must either spend instructions to undo
    // this swap, or work in the order: ( 1, 2, 3, 0 ).
    // For clarity, we choose to spend the instructions to regain the normal order.
    __vector4 vAlphax4[4];
    ExpandBYTEToFLOATAAAA( vAlphax4, vAlphax16 );

    ExpandBYTEToFLOAT<FALSE>( vMin, vMin );
    ExpandBYTEToFLOAT<FALSE>( vMax, vMax );

    __vector4 vAnchor[2];
    vAnchor[0] = vMax;
    vAnchor[1] = vMin;

    // We presently have this ordering:
    //
    // vAlpha[0]x4      [         A0        ][         A4        ][         A8        ][         Ac        ]
    // vAlpha[1]x4      [         A1        ][         A5        ][         A9        ][         Ad        ]
    // vAlpha[2]x4      [         A2        ][         A6        ][         Aa        ][         Ae        ]
    // vAlpha[3]x4      [         A3        ][         A7        ][         Ab        ][         Af        ]
    //
    // We will now subtract Anchor[0] from each alpha, and also reverse this order from top-to-bottom:
    //
    // vDiffx4[0]       [       diff3       ][       diff7       ][       diffb       ][       difff       ]
    // vDiffx4[1]       [       diff2       ][       diff6       ][       diffa       ][       diffe       ]
    // vDiffx4[2]       [       diff1       ][       diff5       ][       diff9       ][       diffd       ]
    // vDiffx4[3]       [       diff0       ][       diff4       ][       diff8       ][       diffc       ]
    //
    // This swizzle order matches the DXT standard, Xbox CPU conventions, and D3DFMT_DXT1 endianness, 
    // as follows:
    //
    // DXT standard is that the 0th input goes in the lowest order bits, and so forth.  This implies
    // an order of:
    // 
    //          Bits:   48 ---------------------------------------------------------- 0
    //                   15| 14| 13| 12| 11| 10|  9|  8|  7|  6|  5|  4|  3|  2|  1|  0
    //
    // Since endian-swaps will break into the middle of indices, we treat this case differently 
    // from previous cases.  We can see that we will want indices to occur in descending order, so 
    // for now, we simply use the standard ordering 15/14/13/12/11/10/9/8/7/6/5/4/3/2/1/0.
    //
    __vector4 vDiffx4[4];
    vDiffx4[0]                  = __vsubfp( vAlphax4[3], vAnchor[0] );
    vDiffx4[1]                  = __vsubfp( vAlphax4[2], vAnchor[0] );
    vDiffx4[2]                  = __vsubfp( vAlphax4[1], vAnchor[0] );
    vDiffx4[3]                  = __vsubfp( vAlphax4[0], vAnchor[0] );

    // Find the 3-bit palette entries
    __vector4 vStepInc = __vmulfp( vFloatSeven, __vrefp( __vsubfp( vAnchor[1], vAnchor[0] )) );
    __vector4 vPackedIndices = Find3BitPaletteIndicesFLOAT( vDiffx4, vStepInc );

    // Find the packed anchors.  Note experiments indicate that it is slightly faster to regenerate 
    // these from the FLOAT anchors than to simply use the BYTE anchors (!?).
    __vector4 vWordAnchor[2];
    vWordAnchor[0]              = __vctuxs( __vmaddfp( vAnchor[0], 
        *( __vector4* )&vFloat255, vFloatOneHalf ), 0 );
    vWordAnchor[1]              = __vctuxs( __vmaddfp( vAnchor[1], 
        *( __vector4* )&vFloat255, vFloatOneHalf ), 0 );

    __vector4 vPackedAnchors    = __vmrglb( vWordAnchor[1], vWordAnchor[0] );
    // Now we have this:
    // vPackedAnchors:  [                   ][         | a1 | a0 ][                   ][         | a1 | a0 ]

    __vector4 vPackedBlock = PackAnchorsAnd12121212To64( vPackedAnchors, vPackedIndices );
    // vPackedBlock:    [ a1 | a0 |  15-0   ][  31-16  |  47-32  ][ a1 | a0 |  15-0   ][  31-16  |  47-32  ]

    // Store 64 bits to possibly unaligned, possibly non-cacheable address
    __stvewx( vPackedBlock, pDst, 0 );
    __stvewx( vPackedBlock, pDst, 4 );
}


//---------------------------------------------------------------------------------------------------------
// Name: CompressRGBBlockVMXBYTE( )
// Desc: Encode a single 64-bit RGB block, as part of either a DXT1 or a DXT4/5 texture.  
//
// Input is 4 vectors, each interpreted as 16-wide BYTEs.  Input ordering is the following in texture coordinates:
//
// -----------------
// | 0 | 1 | 2 | 3 |
// -----------------
// | 4 | 5 | 6 | 7 |
// -----------------
// | 8 | 9 | a | b |
// -----------------
// | c | d | e | f |
// -----------------
//
// The input data is left as BYTE, and all of the calculation uses FLOAT BYTE intrinsics.  In large
// part, the algorithm here follows the techniques outlined in "Real-Time DXT Compression" by 
// J.M.P. van Waveren (http://www.intel.com/cd/ids/developer/asmo-na/eng/324337.htm).
//
// We do not handle DXT1 blocks with 1-bit alpha.  This may be useful to implement in some cases, 
// but it would severely impact performance for non-alpha textures.
//
// This routine currently compiles to code with no branches and no writes to memory, other than 
// the 64-bit output.  Both of these are important for performance.
//---------------------------------------------------------------------------------------------------------
__forceinline VOID CompressRGBBlockVMXBYTE( BYTE *pDst, 
                                         __vector4 vMin, 
                                         __vector4 vMax, 
                                         __vector4 vSrc0123, 
                                         __vector4 vSrc4567, 
                                         __vector4 vSrc89ab, 
                                         __vector4 vSrccdef )
{
    // Load needed constants.  Note most of these operations get scheduled during 
    // pipeline stalls, and are therefore effectively free.
    __vector4 vZero             = __vzero( );
    __vector4 vByteTwo          = __vspltisb( 2 );
    __vector4 vByteThree        = __vspltisb( 3 );
    __vector4 vByteFive         = __vspltisb( 5 );
    __vector4 vByteSix          = __vspltisb( 6 );
    __vector4 vWordOne          = __vspltisw( 1 );
    __vector4 vWordThree        = __vspltisw( 3 );
    __vector4 vWordFive         = __vspltisw( 5 );
    __vector4 vWordEleven       = __vspltisw( 11 );
    __vector4 vWordThirtyOne    = __vsubuws( __vslw( vWordOne, vWordFive ), vWordOne );

    // Load 16 ARGB values into 4 VMX registers as BYTEs
    // Note ARGB byte order is normal here, but reversed for the float method due to __vupkd3d
    __vector4 vARGBx4[4] = { vSrc0123, vSrc4567, vSrc89ab, vSrccdef };
    __vector4 vMinARGB = vMin, vMaxARGB = vMax;

    // Here van Waveren insets the endpoints by 1/16 to improve RMS error.
    // Not sure yet whether we want this, as it will give less accurate representation of
    // the extreme values, which are sometimes the only values.

    // In the BYTE routine, we use the min/max ARGB value as the anchors unconditionally.
    // This choice is usually best, but not when the block consists of areas of different 
    // color, same luminance.
    __vector4 vAnchors[2]       = { vMaxARGB, vMinARGB };

    // Find the packed 5:6:5 anchors, and also the corresponding unpacked 8:8:8 representation
    __vector4 vByteTwoThreeTwo  = __vmrghb( vByteTwo, vByteThree );
    __vector4 vByteFiveSixFive  = __vmrghb( vByteFive, vByteSix );
    __vector4 vMergedAnchors    = __vmrghw( vAnchors[0], vAnchors[1] ); 

    __vector4 vPackedAnchors    = __vsrb( vMergedAnchors, vByteTwoThreeTwo );
    vMergedAnchors              = __vor( __vslb( vPackedAnchors, vByteTwoThreeTwo ), 
        __vsrb( vMergedAnchors, vByteFiveSixFive ) );
    // vPackedAnchors:  [    |  R5|  G6|  B5][    |  r5|  g6|  b5][    |  R5|  G6|  B5][    |  r5|  g6|  b5]
    // vMergedAnchors:  [    |R5+3|G6+2|B5+3][    |r5+3|g6+2|b5+3][    |R5+3|G6+2|B5+3][    |r5+3|g6+2|b5+3]
    // (The bit extension is by 'repeating fraction'.)

    __vector4 vBlues            = vPackedAnchors; 
    __vector4 vGreens           = __vsrh( vPackedAnchors, vWordThree ); 
    __vector4 vReds             = __vsrw( vPackedAnchors, vWordFive ); 
    __vector4 vBlueMask         = vWordThirtyOne;
    __vector4 vRedMask          = __vslh( vWordThirtyOne, vWordEleven );
    vPackedAnchors              = __vsel( __vsel( vGreens, vReds, vRedMask ), vBlues, vBlueMask );
    // vPackedAnchors:  [ garbage | R5|G6|B5][ garbage | r5|g6|b5][ garbage | R5|G6|B5][ garbage | r5|g6|b5]

    vPackedAnchors              = __vpkuwum( vPackedAnchors, vPackedAnchors ); 
    // vPackedAnchors:  [ R5|G6|B5| r5|g6|b5][ R5|G6|B5| r5|g6|b5][ R5|G6|B5| r5|g6|b5][ R5|G6|B5| r5|g6|b5]

    __vector4 vAnchorsLerped = Find2BitLerpedColorsRGBBYTE( vMergedAnchors );
    __vector4 vPaletteColors[4] = { __vspltw( vMergedAnchors, 0 ), 
        __vspltw( vMergedAnchors, 1 ), 
        __vspltw( vAnchorsLerped, 1 ), 
        __vspltw( vAnchorsLerped, 0 ), };

    // 'Transpose' the input data, so that elements are in the desired output order, reading down 
    // each column and then over to the right.  A 'column-major' order is necessary, because the 
    // values in each column will end up consecutive in the output.
    //
    // Here is the correct arrangement:
    //
    //  Elements 7-4   are in the .x components
    //  Elements 3-0   are in the .y components
    //  Elements 15-12 are in the .z components
    //  Elements 11-8  are in the .w components
    //
    //  vARGBx4T[0]     [        dot7       ][        dot3       ][        dotf       ][        dotb       ]
    //  vARGBx4T[1]     [        dot6       ][        dot2       ][        dote       ][        dota       ]
    //  vARGBx4T[2]     [        dot5       ][        dot1       ][        dotd       ][        dot9       ]
    //  vARGBx4T[3]     [        dot4       ][        dot0       ][        dotc       ][        dot8       ]
    //
    // This swizzle order matches the DXT standard, Xbox CPU conventions, and D3DFMT_DXT1 endianness, 
    // as follows:
    //
    // DXT standard is that the 0th input goes in the lowest order bits, and so forth.  This implies
    // an order of:
    // 
    //          Bits:   31 ------------------------------------------ 0
    //                  15|14|13|12|11|10| 9| 8| 7| 6| 5| 4| 3| 2| 1| 0
    //
    // Because the Xbox 360 CPU is big-endian, the order becomes:
    //
    //          Bits:   31 ------------------------------------------ 0
    //                   3| 2| 1| 0| 7| 6| 5| 4|11|10| 9| 8|15|14|13|12
    //
    // D3DFMT_DXT1 uses an endianness of GPUENDIAN_8IN16 for legacy reasons.  Therefore, the order becomes: 
    //
    //          Bits:   31 ------------------------------------------ 0
    //                   7| 6| 5| 4| 3| 2| 1| 0|15|14|13|12|11|10| 9| 8
    //
    __vector4 vARGBx4T[4];
    __vector4 vTempTranspose[4];
    vTempTranspose[0]           = __vmrghw( vARGBx4[1], vARGBx4[3] );
    vTempTranspose[1]           = __vmrglw( vARGBx4[1], vARGBx4[3] );
    vTempTranspose[2]           = __vmrghw( vARGBx4[0], vARGBx4[2] );
    vTempTranspose[3]           = __vmrglw( vARGBx4[0], vARGBx4[2] );
    vARGBx4T[0]                 = __vmrghw( vTempTranspose[0], vTempTranspose[2] );
    vARGBx4T[1]                 = __vmrglw( vTempTranspose[0], vTempTranspose[2] );
    vARGBx4T[2]                 = __vmrghw( vTempTranspose[1], vTempTranspose[3] );
    vARGBx4T[3]                 = __vmrglw( vTempTranspose[1], vTempTranspose[3] );

    // Choose palette color which minimizes 'sum over R,G,B of absolute diffs'.
    // The use of a helper function forces the compiler to unroll the implicit loop over pixels.
    __vector4 vPackedIndices    = vZero;
    CompressRGBBYTEFind4PaletteIndices( vARGBx4T[3], vPaletteColors, vPackedIndices );
    CompressRGBBYTEFind4PaletteIndices( vARGBx4T[2], vPaletteColors, vPackedIndices );
    CompressRGBBYTEFind4PaletteIndices( vARGBx4T[1], vPaletteColors, vPackedIndices );
    CompressRGBBYTEFind4PaletteIndices( vARGBx4T[0], vPaletteColors, vPackedIndices );

    // Currently we have this:
    // vPackedIndices:  [             | 15-8][             |  7-0][             |31-24][             |23-16]
    // We want this:
    // vPackedIndices:  [  15-0   |  31-16  ][  15-0   |  31-16  ][  15-0   |  31-16  ][  15-0   |  31-16  ]
    vPackedIndices = Pack8888To32( vPackedIndices );

    // Special case to avoid alpha pixels when anchors are the same
    __vector4 vAnchorsEqual     = __vcmpequw( vPackedAnchors, __vsldoi( vPackedAnchors, vPackedAnchors, 2 ) );
    vPackedIndices              = __vsel( vPackedIndices, vZero, vAnchorsEqual );

    // Store 64 bits to possibly unaligned, possibly non-cacheable address
    __stvewx( vPackedAnchors, pDst, 0 );
    __stvewx( vPackedIndices, pDst, 4 );

    // Non-alpha block
    assert( (( WORD* )pDst )[0] > ( (WORD* )pDst )[1]
    || ( (( WORD* )pDst )[0] == ( (WORD* )pDst )[1] && ( (DWORD* )pDst )[1] == 0 ));
}


//---------------------------------------------------------------------------------------------------------
// Name: CompressUVBlockVMXBYTE( )
// Desc: Compress a single 64-bit UV block, as part of a CTX1 texture.  
//
// Input is 4 vectors, each interpreted as 16-wide BYTEs, of which only the first 8 bytes belong
// to this block.  Input ordering is the following in texture coordinates:
//
// -----------------
// | 0 | 1 | 2 | 3 |
// -----------------
// | 4 | 5 | 6 | 7 |
// -----------------
// | 8 | 9 | a | b |
// -----------------
// | c | d | e | f |
// -----------------
//
// The input data is left as BYTE, and all of the calculation uses FLOAT BYTE intrinsics.  In large
// part, the algorithm here follows the techniques outlined in "Real-Time YCoCg-DXT Compression" by 
// J.M.P. van Waveren and Ignacio Castano 
// (http://developer.nvidia.com/object/real-time-ycocg-dxt-compression.html), although this article
// actually pertains to color encoding, rather than normal map encoding.
//
// This routine currently compiles to code with no branches and no writes to memory, other than 
// the 64-bit output.  Both of these are important for performance.
//---------------------------------------------------------------------------------------------------------
__forceinline VOID CompressUVBlockVMXBYTE( BYTE *pDst, 
                                        __vector4 vMin, 
                                        __vector4 vMax, 
                                        __vector4 vSrc0123, 
                                        __vector4 vSrc4567, 
                                        __vector4 vSrc89ab, 
                                        __vector4 vSrccdef )
{
    // Load needed constants.  Note most of these operations get scheduled during 
    // pipeline stalls, and are therefore effectively free.
    __vector4 vZero             = __vzero( );
    __vector4 vByteFour         = __vspltisb( 4 );
    __vector4 vByteSix          = __vspltisb( 6 );
    __vector4 vByteEight        = __vspltisb( 8 );
    __vector4 vByteFourteen     = __vspltisb( 14 );

    // Load 16 UV values into 2 VMX registers as BYTEs.  Note these 2 instructions are semi-redundant with 
    // the same ones in the calling function, but we retain them for uniformity.
    //
    // Incoming order:
    // vUVUVx4[0]       [ u0 | v0 | u1 | v1 ][ u2 | v2 | u3 | v3 ][    |    |    |    ][    |    |    |    ]
    // vUVUVx4[1]       [ u4 | v4 | u5 | v5 ][ u6 | v6 | u7 | v7 ][    |    |    |    ][    |    |    |    ]
    // vUVUVx4[2]       [ u8 | v8 | u9 | v9 ][ ua | va | ub | vb ][    |    |    |    ][    |    |    |    ]
    // vUVUVx4[3]       [ uc | vc | ud | vd ][ ue | ve | uf | vf ][    |    |    |    ][    |    |    |    ]
    __vector4 vUVUVx4[4] = { vSrc0123, vSrc4567, vSrc89ab, vSrccdef };
    __vector4 vUVUVx8[2];
    vUVUVx8[0]                  = __vrlimi( vUVUVx4[1], vUVUVx4[0], 0x3, 2 );
    vUVUVx8[1]                  = __vrlimi( vUVUVx4[3], vUVUVx4[2], 0x3, 2 );
    // vUVUVx8[0]       [ u8 | v8 | u9 | v9 ][ ua | va | ub | vb ][ uc | vc | ud | vd ][ ue | ve | uf | vf ]
    // vUVUVx8[1]       [ u0 | v0 | u1 | v1 ][ u2 | v2 | u3 | v3 ][ u4 | v4 | u5 | v5 ][ u6 | v6 | u7 | v7 ]

    // Idea from van Waveren paper on YCoCg compression.  
    // Choose anchors on one of the two diagonals, based on which 
    // quadrants contain the most samples:
    //  MaxV +---+---+
    //       | 1 | 3 |
    //       +---+---+
    //       | 0 | 2 |
    //  MinV +---+---+
    //     MinU     MaxU
    __vector4 vCenter           = __vavgub( vMin, vMax );
    __vector4 vQuad1or2[2];
    for( int i = 0; i < 2; ++i )
    {
        __vector4 vUVHi         = __vcmpgtub( vUVUVx8[i], vCenter );
        __vector4 vVUHi         = __vrlh( vUVHi, vByteEight );
        vQuad1or2[i]            = __vxor( vUVHi, vVUHi );
    }

    // We now have 16 values ( each repeated twice ) which are either 0x00 or 0xff
    // 0x00 means sample is in the upper right quadrant ( 3 ) or lower left quadrant ( 0 )
    // 0xff means sample is in the lower right quadrant ( 2 ) or upper left quadrant ( 1 )
    // If the 0x00's outnumber the 0xff's we choose anchors pointing this way: / 
    // If the 0xff's outnumber the 0x00's we choose anchors pointing this way: \ 
    __vector4 vAnchorTest       = __vavgub( vQuad1or2[0], vQuad1or2[1] );
    vAnchorTest                 = __vavgub( vAnchorTest, __vsldoi( vAnchorTest, vAnchorTest, 8 ) );
    vAnchorTest                 = __vavgub( vAnchorTest, __vsldoi( vAnchorTest, vAnchorTest, 4 ) );
    vAnchorTest                 = __vavgub( vAnchorTest, __vsldoi( vAnchorTest, vAnchorTest, 2 ) );
    vAnchorTest                 = __vcmpgtsb( vZero, vAnchorTest );
    __vector4 vAnchorMask       = __vmrglb( vAnchorTest, vZero );

    // At this point, vAnchorMask is 0x00 for components of vAnchor[0] which should equal 
    // vMin, and 0xff for components of vAnchor[0] which should equal vMax.
    __vector4 vAnchors[2];
    vAnchors[0]                 = __vsel( vMin, vMax, vAnchorMask );
    vAnchors[1]                 = __vsel( vMax, vMin, vAnchorMask );

    // Due to default swizzle pattern for D3DFMT_CTX1, v comes before u.
    // vAnchor0:        [  u0| v0| u0| v0][  u0| v0| u0| v0][  u0| v0| u0| v0][  u0| v0| u0| v0]
    // vAnchor1:        [  u1| v1| u1| v1][  u1| v1| u1| v1][  u1| v1| u1| v1][  u1| v1| u1| v1]
    // vPackedAnchors:  [  v0| u0| v1| u1][  v0| u0| v1| u1][  v0| u0| v1| u1][  v0| u0| v1| u1]
    __vector4 vMergedAnchors    = __vmrglh( vAnchors[0], vAnchors[1] ); 
    __vector4 vPackedAnchors    = __vrlh( vMergedAnchors, vByteEight ); 

    __vector4 vAnchorsLerped = Find2BitLerpedColorsUVBYTE( vMergedAnchors );
    __vector4 vPaletteUVs[4] = { vAnchors[0], 
        vAnchors[1], 
        __vsplth( vAnchorsLerped, 1 ), 
        __vsplth( vAnchorsLerped, 0 ), };

    // Find the 16 palette indices
    __vector4 vPackedIndices = CompressUVBYTEFindPaletteIndices( vUVUVx8, vPaletteUVs );

    // We now have a single 2-bit palette index in each of the 8 bytes of vPalIndexx16:
    //
    //          Bits:   [  98|  ba|  dc|  ef][  10|  32|  54|  76][1918|1b1a|1d1c|1f1e][1110|1312|1514|1716]
    //        Pixels:   [   4|   5|   6|   7][   0|   1|   2|   3][  12|  13|  14|  15][   8|   9|  10|  11]
    //
    // DXT standard is that the 0th input goes in the lowest order bits, and so forth.  This implies
    // an order of:
    // 
    //          Bits:   31 ------------------------------------------ 0
    //                  15|14|13|12|11|10| 9| 8| 7| 6| 5| 4| 3| 2| 1| 0
    //
    // Because the Xbox 360 CPU is big-endian, the order becomes:
    //
    //          Bits:   31 ------------------------------------------ 0
    //                   3| 2| 1| 0| 7| 6| 5| 4|11|10| 9| 8|15|14|13|12
    //
    // D3DFMT_DXT1 uses an endianness of GPUENDIAN_8IN16 for legacy reasons.  Therefore, the order becomes: 
    //
    //          Bits:   31 ------------------------------------------ 0
    //                   7| 6| 5| 4| 3| 2| 1| 0|15|14|13|12|11|10| 9| 8
    //
    // Therefore want this:
    // vPackedIndices:  [  15-0   |  31-16  ][  15-0   |  31-16  ][  15-0   |  31-16  ][  15-0   |  31-16  ]
    //
    vPackedIndices              = __vor( 
        __vor( __vrlw( vPackedIndices, vByteEight ), 
        __vsrw( vPackedIndices, vByteFourteen ) ), 
        __vor( __vsrw( vPackedIndices, vByteFour ), 
        __vslw( vPackedIndices, vByteSix ) ) );

    // Currently we have this:
    // vPackedIndices:  [             | 15-8][             |  7-0][             |31-24][             |23-16]
    // We want this:
    // vPackedIndices:  [  15-0   |  31-16  ][  15-0   |  31-16  ][  15-0   |  31-16  ][  15-0   |  31-16  ]
    vPackedIndices = Pack8888To32( vPackedIndices );

    // Store 64 bits to possibly unaligned, possibly non-cacheable address
    __stvewx( vPackedAnchors, pDst, 0 );
    __stvewx( vPackedIndices, pDst, 4 );
}


//---------------------------------------------------------------------------------------------------------
// Name: CompressABlockVMXFLOAT( )
// Desc: Compress a single 64-bit Alpha block, as part of either a DXT4/5 or a DXN texture.  This routine
// could also be used to generate 1-channel DXT5A textures (not implemented).  
//
// Input is 4 vectors, each interpreted as 16-wide BYTEs.  Input ordering is the following in texture coordinates:
//
// -----------------
// | 0 | 1 | 2 | 3 |
// -----------------
// | 4 | 5 | 6 | 7 |
// -----------------
// | 8 | 9 | a | b |
// -----------------
// | c | d | e | f |
// -----------------
//
// The input data is left as BYTE, and all of the calculation uses FLOAT BYTE intrinsics.  In large
// part, the algorithm here follows the techniques outlined in "Real-Time DXT Compression" by 
// J.M.P. van Waveren (http://www.intel.com/cd/ids/developer/asmo-na/eng/324337.htm).
//
// We do not handle Alpha blocks with explicit 0 and 1 (transparent/opaque) encoding.  This may be useful 
// to implement in some cases, but it would severely impact performance.
//
// This routine currently compiles to code with no branches and no writes to memory, other than 
// the 64-bit output.  Both of these are important for performance.
//---------------------------------------------------------------------------------------------------------
template< UINT t_iChannel >
__forceinline VOID CompressABlockVMXBYTE( BYTE *pDst, 
                                       __vector4 vMin, 
                                       __vector4 vMax, 
                                       __vector4 vSrc0123, 
                                       __vector4 vSrc4567, 
                                       __vector4 vSrc89ab, 
                                       __vector4 vSrccdef )
{
    // Load needed constants.  Note most of these operations get scheduled during 
    // pipeline stalls, and are therefore effectively free.
    __vector4 vWordThree        = __vspltisw( 3 );
    __vector4 vWordSeven        = __vspltisw( 7 );
    __vector4 vWordEight        = __vspltisw( 8 );

    // Load 16 ARGB values into 4 VMX registers as BYTEs
    // Note ARGB byte order is normal here, but reversed for the float method due to __vupkd3d
    __vector4 vARGBx4[4] = { vSrc0123, vSrc4567, vSrc89ab, vSrccdef };

    __vector4 vAlphax16;
    SelectOneChannelFromFour<FALSE, t_iChannel>( vAlphax16, vARGBx4 );

    SelectOneChannelFromFour<t_iChannel>( vMin, vMin );
    SelectOneChannelFromFour<t_iChannel>( vMax, vMax );

    // Convention is Max comes before Min for 7-step alpha case
    __vector4 vAnchor[2];
    vAnchor[0] = vMax;
    vAnchor[1] = vMin;
    __vector4 vMergedAnchors    = __vmrglw( vAnchor[0], vAnchor[1] );
    __vector4 vPackedAnchors    = __vmrglb( vAnchor[1], vAnchor[0] );
    // Now we have this:
    // vPackedAnchors:  [  a1|  a0|  a1|  a0][  a1|  a0|  a1|  a0][  a1|  a0|  a1|  a0][  a1|  a0|  a1|  a0]

    __vector4 vLerpedOne7th;
    __vector4 vLerpedTwo7th;
    __vector4 vLerpedThree7th;
    Find3BitLerpedColorsABYTE( vMergedAnchors, vLerpedOne7th, vLerpedTwo7th, vLerpedThree7th );
    __vector4 vPaletteColors[8] = { vAnchor[0], 
        vAnchor[1], 
        __vspltw( vLerpedOne7th, 1 ),  
        __vspltw( vLerpedTwo7th, 1 ),  
        __vspltw( vLerpedThree7th, 1 ),  
        __vspltw( vLerpedThree7th, 0 ),
        __vspltw( vLerpedTwo7th, 0 ),  
        __vspltw( vLerpedOne7th, 0 ), };  

    // Find index which minimizes diff with source value
    __vector4 vPackedIndices = CompressABYTEFindPaletteIndices( vAlphax16, vPaletteColors );

    // Currently we have 3 bits of index in each of 16 bytes.
    //
    // vPackedIndices.x:    [ 2- 0| 5- 3| 8- 6|11- 9]
    // vPackedIndices.y:    [14-12|17-15|20-18|23-21]
    // vPackedIndices.z:    [26-24|29-27|32-30|35-33]
    // vPackedIndices.w:    [38-36|41-39|44-42|47-45]
    //
    // First work on words in parallel to combine the 4 indices within each one.
    __vector4 vIndex37bf        = vPackedIndices;
    __vector4 vIndex26ae        = __vsrw( vIndex37bf, vWordEight );
    __vector4 vIndex159d        = __vsrw( vIndex26ae, vWordEight );
    __vector4 vIndex048c        = __vsrw( vIndex159d, vWordEight );
    vIndex37bf                  = __vand( vIndex37bf, vWordSeven );
    vIndex26ae                  = __vand( vIndex26ae, vWordSeven );
    vIndex159d                  = __vand( vIndex159d, vWordSeven );
    vIndex048c                  = __vand( vIndex048c, vWordSeven );
    vPackedIndices              = vIndex37bf;
    vPackedIndices              = __vor( vIndex26ae, __vslw( vPackedIndices, vWordThree ) );
    vPackedIndices              = __vor( vIndex159d, __vslw( vPackedIndices, vWordThree ) );
    vPackedIndices              = __vor( vIndex048c, __vslw( vPackedIndices, vWordThree ) );
    // Now we have this:
    // Palette:         [          |11-0 ][          |23-12][          |35-24][          |47-36]

    __vector4 vPackedBlock = PackAnchorsAnd12121212To64( vPackedAnchors, vPackedIndices );
    // vPackedBlock:    [ a1| a0|  15-0  ][ 31-16 | 47-32  ][ a1| a0|  15-0  ][ 31-16 | 47-32  ]

    // Store 64 bits to possibly unaligned, possibly non-cacheable address
    __stvewx( vPackedBlock, pDst, 0 );
    __stvewx( vPackedBlock, pDst, 4 );
}


//---------------------------------------------------------------------------------------------------------
// Name: CompressDXT1BlockVMXFLOAT( )
// Desc: Compress a single 64-bit DXT1 block, using primarily SIMD FLOAT operations.  
// Input is 4 vectors, each interpreted as 16-wide BYTEs.  
//---------------------------------------------------------------------------------------------------------
VOID CompressDXT1BlockVMXFLOAT( BYTE *pDst, __vector4 vSrc0123, __vector4 vSrc4567, __vector4 vSrc89ab, __vector4 vSrccdef )
{
    // Take min/max
    __vector4 vMin, vMax;
    FindMinMaxARGB( vMin, vMax, vSrc0123, vSrc4567, vSrc89ab, vSrccdef );

    CompressRGBBlockVMXFLOAT( pDst, vMin, vMax, vSrc0123, vSrc4567, vSrc89ab, vSrccdef );
}


//---------------------------------------------------------------------------------------------------------
// Name: CompressDXT5BlockVMXFLOAT( )
// Desc: Compress a single 128-bit DXT5 block, using primarily SIMD FLOAT operations.  
// Input is 4 vectors, each interpreted as 16-wide BYTEs.  
//---------------------------------------------------------------------------------------------------------
VOID CompressDXT5BlockVMXFLOAT( BYTE *pDst, __vector4 vSrc0123, __vector4 vSrc4567, __vector4 vSrc89ab, __vector4 vSrccdef )
{
    // Take min/max
    __vector4 vMin, vMax;
    FindMinMaxARGB( vMin, vMax, vSrc0123, vSrc4567, vSrc89ab, vSrccdef );

    // The ordering of these calls causes a significant difference in performance
    // possibly due to write-combine order
    CompressABlockVMXFLOAT<CHANNEL_ALPHA>(  pDst, vMin, vMax, vSrc0123, vSrc4567, vSrc89ab, vSrccdef  );
    CompressRGBBlockVMXFLOAT( pDst + 8, vMin, vMax, vSrc0123, vSrc4567, vSrc89ab, vSrccdef );
}


//---------------------------------------------------------------------------------------------------------
// Name: CompressCTX1BlockVMXFLOAT( )
// Desc: Encode a single 64-bit CTX1 block, using primarily SIMD FLOAT operations.  
// Input is 4 vectors, each interpreted as 16-wide BYTEs, of which only the first 8 belong to this block.  
//---------------------------------------------------------------------------------------------------------
VOID CompressCTX1BlockVMXFLOAT( BYTE *pDst, __vector4 vSrc0123, __vector4 vSrc4567, __vector4 vSrc89ab, __vector4 vSrccdef )
{
    __vector4 vUVUVx4[4] = { vSrc0123, vSrc4567, vSrc89ab, vSrccdef };
    __vector4 vUVUVx8[2];
    vUVUVx8[0]                  = __vrlimi( vUVUVx4[0], vUVUVx4[1], 0x3, 2 );
    vUVUVx8[1]                  = __vrlimi( vUVUVx4[2], vUVUVx4[3], 0x3, 2 );
    // vUVUVx8[0]       [ u0 | v0 | u1 | v1 ][ u2 | v2 | u3 | v3 ][ u4 | v4 | u5 | v5 ][ u6 | v6 | u7 | v7 ]
    // vUVUVx8[1]       [ u8 | v8 | u9 | v9 ][ ua | va | ub | vb ][ uc | vc | ud | vd ][ ue | ve | uf | vf ]

    // Take min/max
    __vector4 vMin, vMax;
    FindMinMaxUVUV( vMin, vMax, vUVUVx8[0], vUVUVx8[1] );

    CompressUVBlockVMXFLOAT( pDst, vMin, vMax, vSrc0123, vSrc4567, vSrc89ab, vSrccdef );
}


//---------------------------------------------------------------------------------------------------------
// Name: CompressDXNBlockVMXFLOAT( )
// Desc: Encode a single 128-bit DXN block, using primarily SIMD FLOAT operations.  
// Input is 4 vectors, each interpreted as 16-wide BYTEs, of which only the first 8 belong to this block.  
//---------------------------------------------------------------------------------------------------------
VOID CompressDXNBlockVMXFLOAT( BYTE *pDst, __vector4 vSrc0123, __vector4 vSrc4567, __vector4 vSrc89ab, __vector4 vSrccdef )
{
    __vector4 vUVUVx4[4] = { vSrc0123, vSrc4567, vSrc89ab, vSrccdef };
    __vector4 vUVUVx8[2];
    vUVUVx8[0]                  = __vrlimi( vUVUVx4[0], vUVUVx4[1], 0x3, 2 );
    vUVUVx8[1]                  = __vrlimi( vUVUVx4[2], vUVUVx4[3], 0x3, 2 );
    // vUVUVx8[0]       [ u0 | v0 | u1 | v1 ][ u2 | v2 | u3 | v3 ][ u4 | v4 | u5 | v5 ][ u6 | v6 | u7 | v7 ]
    // vUVUVx8[1]       [ u8 | v8 | u9 | v9 ][ ua | va | ub | vb ][ uc | vc | ud | vd ][ ue | ve | uf | vf ]

    // Take min/max
    __vector4 vMin, vMax;
    FindMinMaxUVUV( vMin, vMax, vUVUVx8[0], vUVUVx8[1] );

    CompressABlockVMXFLOAT<CHANNEL_V>( pDst, vMin, vMax, vSrc0123, vSrc4567, vSrc89ab, vSrccdef );
    CompressABlockVMXFLOAT<CHANNEL_U>( pDst + 8, vMin, vMax, vSrc0123, vSrc4567, vSrc89ab, vSrccdef );
}


//---------------------------------------------------------------------------------------------------------
// Name: CompressDXT1BlockVMXFLOAT( )
// Desc: Encode a single 64-bit DXT1 block, using primarily SIMD BYTE operations.  
// Input is 4 vectors, each interpreted as 16-wide BYTEs.  
//---------------------------------------------------------------------------------------------------------
VOID CompressDXT1BlockVMXBYTE( BYTE *pDst, __vector4 vSrc0123, __vector4 vSrc4567, __vector4 vSrc89ab, __vector4 vSrccdef )
{
    // Take min/max
    __vector4 vMinARGB, vMaxARGB;
    FindMinMaxARGB( vMinARGB, vMaxARGB, vSrc0123, vSrc4567, vSrc89ab, vSrccdef );

    CompressRGBBlockVMXBYTE( pDst, vMinARGB, vMaxARGB, vSrc0123, vSrc4567, vSrc89ab, vSrccdef );
}


//---------------------------------------------------------------------------------------------------------
// Name: CompressDXT5BlockVMXFLOAT( )
// Desc: Encode a single 128-bit DXT5 block, using primarily SIMD BYTE operations.  
// Input is 4 vectors, each interpreted as 16-wide BYTEs.  
//---------------------------------------------------------------------------------------------------------
VOID CompressDXT5BlockVMXBYTE( BYTE *pDst, __vector4 vSrc0123, __vector4 vSrc4567, __vector4 vSrc89ab, __vector4 vSrccdef )
{
    // Take min/max
    __vector4 vMinARGB, vMaxARGB;
    FindMinMaxARGB( vMinARGB, vMaxARGB, vSrc0123, vSrc4567, vSrc89ab, vSrccdef );

    // The ordering of these calls causes a significant difference in performance
    // possibly due to write-combine order
    CompressABlockVMXBYTE<CHANNEL_ALPHA>( pDst, vMinARGB, vMaxARGB, vSrc0123, vSrc4567, vSrc89ab, vSrccdef );
    CompressRGBBlockVMXBYTE( pDst + 8, vMinARGB, vMaxARGB, vSrc0123, vSrc4567, vSrc89ab, vSrccdef );
}


//---------------------------------------------------------------------------------------------------------
// Name: CompressCTX1BlockVMXFLOAT( )
// Desc: Encode a single 64-bit CTX1 block, using primarily SIMD BYTE operations.  
// Input is 4 vectors, each interpreted as 16-wide BYTEs, of which only the first 8 belong to this block.  
//---------------------------------------------------------------------------------------------------------
VOID CompressCTX1BlockVMXBYTE( BYTE *pDst, __vector4 vSrc0123, __vector4 vSrc4567, __vector4 vSrc89ab, __vector4 vSrccdef )
{
    __vector4 vUVUVx4[4] = { vSrc0123, vSrc4567, vSrc89ab, vSrccdef };
    __vector4 vUVUVx8[2];
    vUVUVx8[0]                  = __vrlimi( vUVUVx4[0], vUVUVx4[1], 0x3, 2 );
    vUVUVx8[1]                  = __vrlimi( vUVUVx4[2], vUVUVx4[3], 0x3, 2 );
    // vUVUVx8[0]       [ u0 | v0 | u1 | v1 ][ u2 | v2 | u3 | v3 ][ u4 | v4 | u5 | v5 ][ u6 | v6 | u7 | v7 ]
    // vUVUVx8[1]       [ u8 | v8 | u9 | v9 ][ ua | va | ub | vb ][ uc | vc | ud | vd ][ ue | ve | uf | vf ]

    // Take min/max
    __vector4 vMinUVUV, vMaxUVUV;
    FindMinMaxUVUV( vMinUVUV, vMaxUVUV, vUVUVx8[0], vUVUVx8[1] );

    CompressUVBlockVMXBYTE( pDst, vMinUVUV, vMaxUVUV, vSrc0123, vSrc4567, vSrc89ab, vSrccdef );
}


//---------------------------------------------------------------------------------------------------------
// Name: CompressDXNBlockVMXFLOAT( )
// Desc: Encode a single 128-bit DXN block, using primarily SIMD BYTE operations.  
// Input is 4 vectors, each interpreted as 16-wide BYTEs, of which only the first 8 belong to this block.  
//---------------------------------------------------------------------------------------------------------
VOID CompressDXNBlockVMXBYTE( BYTE *pDst, __vector4 vSrc0123, __vector4 vSrc4567, __vector4 vSrc89ab, __vector4 vSrccdef )
{
    __vector4 vUVUVx4[4] = { vSrc0123, vSrc4567, vSrc89ab, vSrccdef };
    __vector4 vUVUVx8[2];
    vUVUVx8[0]                  = __vrlimi( vUVUVx4[0], vUVUVx4[1], 0x3, 2 );
    vUVUVx8[1]                  = __vrlimi( vUVUVx4[2], vUVUVx4[3], 0x3, 2 );
    // vUVUVx8[0]       [ u0 | v0 | u1 | v1 ][ u2 | v2 | u3 | v3 ][ u4 | v4 | u5 | v5 ][ u6 | v6 | u7 | v7 ]
    // vUVUVx8[1]       [ u8 | v8 | u9 | v9 ][ ua | va | ub | vb ][ uc | vc | ud | vd ][ ue | ve | uf | vf ]

    // Take min/max
    __vector4 vMinUVUV, vMaxUVUV;
    FindMinMaxUVUV( vMinUVUV, vMaxUVUV, vUVUVx8[0], vUVUVx8[1] );

    CompressABlockVMXBYTE<CHANNEL_V>( pDst, vMinUVUV, vMaxUVUV, vSrc0123, vSrc4567, vSrc89ab, vSrccdef );
    CompressABlockVMXBYTE<CHANNEL_U>( pDst + 8, vMinUVUV, vMaxUVUV, vSrc0123, vSrc4567, vSrc89ab, vSrccdef );
}

#pragma warning( pop )


//---------------------------------------------------------------------------------------------------------
// Name: CompressTextureVMX( )
// Desc: Encode a full texture, using the CompressBlock routine on each 4x4 block.  
//---------------------------------------------------------------------------------------------------------
template <BlockFn CompressBlock>
VOID CompressTextureVMX( const TextureDescAndBaseAddress* pSrcDescAndBaseAddress, 
                        const TextureDescAndBaseAddress* pDstDescAndBaseAddress )
{
    assert( pSrcDescAndBaseAddress->Desc.Width % 4 == 0 
        && pSrcDescAndBaseAddress->Desc.Height % 4 == 0 );
    assert( pSrcDescAndBaseAddress->Desc.Width == pDstDescAndBaseAddress->Desc.Width 
        && pSrcDescAndBaseAddress->Desc.Height == pDstDescAndBaseAddress->Desc.Height );

    assert( !XGIsTiledFormat( pSrcDescAndBaseAddress->Desc.Format ) );

    const BYTE* pSrcRow = ( BYTE* ) pSrcDescAndBaseAddress->BaseAddress;
    UINT iSrcBytesPerTexel = pSrcDescAndBaseAddress->Desc.BitsPerPixel / 8;
    UINT iSrcRowPitchInBytes = pSrcDescAndBaseAddress->Desc.RowPitch;
    UINT iSrcBlockPitchInBytes = 4* iSrcRowPitchInBytes;

    BYTE* pDstRow = ( BYTE* ) pDstDescAndBaseAddress->BaseAddress;

    // Let's bring in two rows of blocks into the L2 cache at a time 
    // ( which is 8x512x4 = 4 KB for a 512x512 8:8:8:8 source ). 
    // It should be okay to prefetch past the end of the buffer ---
    // prefetches are discarded for addresses which aren't in the TLB.
    // Even for the max 2D texture dimensions of 4096x4096, this is
    // only 32 KB, so not a huge displacement of cache contents.
    Prefetch( pSrcRow, iSrcBlockPitchInBytes );

    for( UINT jBlock = 0; jBlock < pSrcDescAndBaseAddress->Desc.Height / 4; ++jBlock )
    {
        Prefetch( pSrcRow + iSrcBlockPitchInBytes, iSrcBlockPitchInBytes );

        const BYTE* pSrcBlock = pSrcRow;
        BYTE* pDstBlock = pDstRow;

        for( UINT iBlock = 0; iBlock < pSrcDescAndBaseAddress->Desc.Width / 4; ++iBlock )
        {
            __vector4 vSrc0123           = __lvlx( pSrcBlock, 0 * iSrcRowPitchInBytes );
            __vector4 vSrc4567           = __lvlx( pSrcBlock, 1 * iSrcRowPitchInBytes );
            __vector4 vSrc89ab           = __lvlx( pSrcBlock, 2 * iSrcRowPitchInBytes );
            __vector4 vSrccdef           = __lvlx( pSrcBlock, 3 * iSrcRowPitchInBytes );

            CompressBlock( pDstBlock, vSrc0123, vSrc4567, vSrc89ab, vSrccdef );

            pSrcBlock += 4 * iSrcBytesPerTexel;
            pDstBlock += pDstDescAndBaseAddress->Desc.BytesPerBlock;
        }

        pSrcRow += iSrcBlockPitchInBytes;  // Src pitch per Dst row of block
        pDstRow += pDstDescAndBaseAddress->Desc.RowPitch;
    }
}


//---------------------------------------------------------------------------------------------------------
// Name: VMXCompressor::CompressTextureVMX( )
// Desc: Encode a full texture, choosing the appropriate routine to use on each 4x4 block.  
//---------------------------------------------------------------------------------------------------------
VOID VMXCompressor::CompressTexture( const TextureDescAndBaseAddress* pSrcDescAndBaseAddress, 
                                    const TextureDescAndBaseAddress* pDstDescAndBaseAddress, 
                                    UINT iCompressedType, 
                                    UINT iSIMDFormat )
{
    switch( iSIMDFormat )
    {
    case SIMD_FORMAT_4_FLOAT:
        switch( iCompressedType )
        {
        case COMPRESSED_TYPE_DXT1:
            CompressTextureVMX<&CompressDXT1BlockVMXFLOAT>( pSrcDescAndBaseAddress, 
                pDstDescAndBaseAddress );
            break;
        case COMPRESSED_TYPE_DXT5:
            CompressTextureVMX<&CompressDXT5BlockVMXFLOAT>( pSrcDescAndBaseAddress, 
                pDstDescAndBaseAddress );
            break;
        case COMPRESSED_TYPE_CTX1:
            CompressTextureVMX<&CompressCTX1BlockVMXFLOAT>( pSrcDescAndBaseAddress, 
                pDstDescAndBaseAddress );
            break;
        case COMPRESSED_TYPE_DXN:
            CompressTextureVMX<&CompressDXNBlockVMXFLOAT>( pSrcDescAndBaseAddress, 
                pDstDescAndBaseAddress );
            break;
        }
        break;

    case SIMD_FORMAT_16_BYTE:
        switch( iCompressedType )
        {
        case COMPRESSED_TYPE_DXT1:
            CompressTextureVMX<&CompressDXT1BlockVMXBYTE>( pSrcDescAndBaseAddress, 
                pDstDescAndBaseAddress );
            break;
        case COMPRESSED_TYPE_DXT5:
            CompressTextureVMX<&CompressDXT5BlockVMXBYTE>( pSrcDescAndBaseAddress, 
                pDstDescAndBaseAddress );
            break;
        case COMPRESSED_TYPE_CTX1:
            CompressTextureVMX<&CompressCTX1BlockVMXBYTE>( pSrcDescAndBaseAddress, 
                pDstDescAndBaseAddress );
            break;
        case COMPRESSED_TYPE_DXN:
            CompressTextureVMX<&CompressDXNBlockVMXBYTE>( pSrcDescAndBaseAddress, 
                pDstDescAndBaseAddress );
            break;
        }
        break;
    }
}



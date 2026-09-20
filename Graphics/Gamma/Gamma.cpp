//-----------------------------------------------------------------------------
// Gamma.cpp
//
// Sample demonstrating arbitrary combinations of gamma options.  "Gamma 
// conversion" of color values means a transformation applied to linear 
// intensity into some other representation which more closely matches 
// human perception.  There are several reasons why a console application 
// needs to be gamma-aware.
//
//  1) Art packages export textures in gamma space.
//  2) TVs and monitors expect video signals in gamma space.
//  3) Most color calculations, including filtering, lighting, and alpha 
// blending are only correct in linear space.
//  4) Gamma space offers more perceptual precision for a fixed number of
// bits than linear space.
//
// There are six places where a gamma conversion may occur between initial 
// source and final display.  Of these, four are controllable within the Xbox 
// 360 hardware.
//
//  1) Between source and run-time format.  This conversion might take place in 
// software (exporter from an art package) or hardware (video camera).
//  2) During texture sampling.  This conversion might take place in hardware
// (automatic piecewise-linear conversion for sRGB texture) or in shader code.
//  3) During shader output.  This conversion might take place in hardware
// (automatic piecewise-linear conversion for sRGB render target) or in shader code.
//  4) During resolve from EDRAM to main memory.  This occurs automatically 
// in hardware if you resolve from an sRGB render target to a linear texture
// format (but not the other way around).
//  5) During conversion of the front buffer to a video output signal.  This
// conversion is controlled by either the 8-bit lookup table in SetGammaRamp
// (for 8-bit front buffer formats) or the 128-entry PWL function in SetPWLGamma
// (for 10-bit front buffer formats).
//  6) Upon display by the television set.
//
// Additional intermediate conversions of types (2), (3) and (4) can take place
// during multipass rendering, and also during alpha-blending.  The Alpha/Z units
// on the Xbox 360 GPU can perform PWL gamma/degamma so that alpha-blending 
// always takes place in linear space.
//
// XNA Developer Connection
// Copyright (C) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#include <xtl.h>
#include <xgraphics.h>
#include <d3dx9.h>

#include <AtgApp.h>
#include <AtgFont.h>
#include <AtgHelp.h>
#include <AtgInput.h>
#include <AtgUtil.h>


//--------------------------------------------------------------------------------------
// Callouts for labelling the gamepad on the help screen
//--------------------------------------------------------------------------------------
ATG::HELP_CALLOUT g_HelpCallouts[] =
{
    { ATG::HELP_BACK_BUTTON,    ATG::HELP_PLACEMENT_2, L"Display\nhelp"  },
    { ATG::HELP_DPAD,           ATG::HELP_PLACEMENT_2, L"Change item focus (u/d)\nDecrease / increase active item (l/r)" },
    { ATG::HELP_LEFT_STICK,     ATG::HELP_PLACEMENT_2, L"Change item focus (u/d)\nDecrease / increase active item (l/r)" },
    { ATG::HELP_LEFT_TRIGGER,   ATG::HELP_PLACEMENT_2, L"Decrease\nactive item" },
    { ATG::HELP_RIGHT_TRIGGER,  ATG::HELP_PLACEMENT_2, L"Increase\nactive item" },
    { ATG::HELP_X_BUTTON,       ATG::HELP_PLACEMENT_2, L"Toggle\nUI" },
};
#define NUM_HELP_CALLOUTS _countof(g_HelpCallouts)

//--------------------------------------------------------------------------------------
// Struct for vertex of test geometry
//--------------------------------------------------------------------------------------
struct TestGeometryVertex
{
    XMFLOAT3 Position;
    XMFLOAT2 TexCoord;
    XMFLOAT3 Normal;
};

//--------------------------------------------------------------------------------------
// Structs representing texels of various color formats.  These are used only for 
// offline calculation.  For real-time calculation on the console, XMMATH types should 
// be used instead for better performance.
//--------------------------------------------------------------------------------------
struct COLOR_32_32_32_32F   // XMVECTOR
{
    FLOAT a, b, g, r;
};

struct COLOR_8_8_8_8        // XMCOLOR
{
    BYTE a, b, g, r;
};
struct COLOR_2_10_10_10     // XMUDECN4
{
    UINT a : 2;
    UINT b : 10;
    UINT g : 10;
    UINT r : 10;
};
struct COLOR_16_16_16_16    // XMUSHORTN4
{
    WORD r, g, b, a;    // order reversed here because D3DFMT_A16B16G16R16 endian-swaps differently
};


//--------------------------------------------------------------------------------------
// Helper functions.  These perform all the necessary transforms between 32-bit linear
// values and various gamma spaces of various bit depths.
//--------------------------------------------------------------------------------------
template<typename t_type> 
t_type Squared( t_type a ) { return a * a; }
template<typename t_type>
t_type Min( t_type a, t_type b ) { return a < b ? a : b; }
template<typename t_type>
t_type Max( t_type a, t_type b ) { return a > b ? a : b; }
template<typename t_type>
t_type Saturate( t_type a ) { return Min( 1.0f, Max( 0.0f, a ) ); }

template<typename t_type, UINT t_N>
t_type ConvertFloatToNBits( FLOAT f ) { return (t_type) ( ( ( 1 << t_N ) - 1 ) * Saturate( f ) + 0.5f ); }
template<typename t_type, UINT t_N>
FLOAT ConvertNBitsToFloat( t_type i ) { return ( (FLOAT) i ) / ( ( 1 << t_N ) - 1 ); }

typedef FLOAT DegammaFunc( FLOAT );
typedef FLOAT GammaFunc( FLOAT );

// Conversions from gamma spaces to linear space
FLOAT DegammaFuncNull( FLOAT f ) { return f; }
FLOAT DegammaFunc_2_0( FLOAT f ) { return Squared( f ); }
FLOAT DegammaFunc_2_2( FLOAT f ) { return powf( f, 2.2f ); }
FLOAT DegammaFuncPWL( FLOAT f ) 
{ 
    if( f > 3.0f / 4.0f )
    {
        return ( 1.0f / 2.0f + ( f - 3.0f / 4.0f ) * 2.0f );
    }
    else if( f > 3.0f / 8.0f )
    {
        return ( 1.0f / 8.0f + ( f - 3.0f / 8.0f ) * 1.0f );
    }
    else if( f > 1.0f / 4.0f )
    {
        return ( 1.0f / 16.0f + ( f - 1.0f / 4.0f ) / 2.0f );
    }
    else
    {
        return ( 0.0f + ( f - 0.0f ) / 4.0f );
    }
}
FLOAT DegammaFuncsRGB( FLOAT f )
{
    if( f <= 0.04045f )
    {
        return f / 12.92f;
    }
    else
    {
        return powf( ( f + 0.055f ) / 1.055f, 2.4f );
    }
}
FLOAT DegammaFuncTV( FLOAT f )
{
    if( f <= 0.0812f )
    {
        return f / 4.5f;
    }
    else
    {
        return powf( ( f + 0.099f ) / 1.099f, 1.0f / 0.45f );
    }
}

// Conversions from linear space to gamma spaces
FLOAT GammaFuncNull( FLOAT f ) { return f; }
FLOAT GammaFunc_2_0( FLOAT f ) { return sqrtf( f ); }
FLOAT GammaFunc_2_2( FLOAT f ) { return powf( f, 1.0f / 2.2f ); }
FLOAT GammaFuncPWL( FLOAT f ) 
{ 
    if( f > 1.0f / 2.0f )
    {
        return ( 3.0f / 4.0f + ( f - 1.0f / 2.0f ) / 2.0f );
    }
    else if( f > 1.0f / 8.0f )
    {
        return ( 3.0f / 8.0f + ( f - 1.0f / 8.0f ) * 1.0f );
    }
    else if( f > 1.0f / 16.0f )
    {
        return ( 1.0f / 4.0f + ( f - 1.0f / 16.0f ) * 2.0f );
    }
    else
    {
        return ( 0.0f + ( f - 0.0f ) * 4.0f );
    }
}
FLOAT GammaFuncsRGB( FLOAT f )
{
    if( f <= 0.0031308f )
    {
        return f * 12.92f;
    }
    else
    {
        return powf( f, 1.0f / 2.4f ) * 1.055f - 0.055f;
    }
}
FLOAT GammaFuncTV( FLOAT f )
{
    if( f <= 0.018f )
    {
        return f * 4.5f;
    }
    else
    {
        return powf( f, 0.45f ) * 1.099f - 0.099f;
    }
}

template<typename t_type, UINT t_N, DegammaFunc Degamma> 
FLOAT ConvertNBitGammaToFloat( t_type i ) { return Degamma( ConvertNBitsToFloat<t_type, t_N>( i ) ); }
template<typename t_type, UINT t_N, GammaFunc Gamma> 
t_type ConvertFloatToNBitGamma( FLOAT f ) { return ConvertFloatToNBits<t_type, t_N>( Gamma( f ) ); }

// Gamma conversions from FLOAT to integer
BYTE ConvertFloatTo8BitLinear( FLOAT f )            { return ConvertFloatToNBitGamma<BYTE,  8, GammaFuncNull>( f ); }
BYTE ConvertFloatTo2BitLinear( FLOAT f )            { return ConvertFloatToNBitGamma<BYTE,  2, GammaFuncNull>( f ); }
WORD ConvertFloatTo10BitLinear( FLOAT f )           { return ConvertFloatToNBitGamma<WORD, 10, GammaFuncNull>( f ); }
WORD ConvertFloatTo16BitLinear( FLOAT f )           { return ConvertFloatToNBitGamma<WORD, 16, GammaFuncNull>( f ); }

BYTE ConvertFloatTo8BitGammaPWL( FLOAT f )          { return ConvertFloatToNBitGamma<BYTE,  8, GammaFuncPWL>( f ); }
WORD ConvertFloatTo10BitGammaPWL( FLOAT f )         { return ConvertFloatToNBitGamma<WORD, 10, GammaFuncPWL>( f ); }
WORD ConvertFloatTo16BitGammaPWL( FLOAT f )         { return ConvertFloatToNBitGamma<WORD, 16, GammaFuncPWL>( f ); }

BYTE ConvertFloatTo8BitGamma_2_0( FLOAT f )         { return ConvertFloatToNBitGamma<BYTE,  8, GammaFunc_2_0>( f ); }
WORD ConvertFloatTo10BitGamma_2_0( FLOAT f )        { return ConvertFloatToNBitGamma<WORD, 10, GammaFunc_2_0>( f ); }

BYTE ConvertFloatTo8BitGamma_2_2( FLOAT f )         { return ConvertFloatToNBitGamma<BYTE,  8, GammaFunc_2_2>( f ); }
WORD ConvertFloatTo10BitGamma_2_2( FLOAT f )        { return ConvertFloatToNBitGamma<WORD, 10, GammaFunc_2_2>( f ); }

WORD ConvertFloatTo10BitGammasRGB( FLOAT f )        { return ConvertFloatToNBitGamma<WORD, 10, GammaFuncsRGB>( f ); }
WORD ConvertFloatTo16BitGammasRGB( FLOAT f )        { return ConvertFloatToNBitGamma<WORD, 16, GammaFuncsRGB>( f ); }

WORD ConvertFloatTo10BitGammasRGBOverride( FLOAT f ){ return ConvertFloatTo10BitGammasRGB( DegammaFuncTV ( GammaFuncsRGB( f ) ) ); }
WORD ConvertFloatTo16BitGammasRGBOverride( FLOAT f ){ return ConvertFloatTo16BitGammasRGB( DegammaFuncTV ( GammaFuncsRGB( f ) ) ); }

// Degamma conversions from integer to FLOAT
FLOAT Convert8BitGammaPWLToFloat( BYTE i )          { return ConvertNBitGammaToFloat<BYTE,  8, DegammaFuncPWL>( i ); }
FLOAT Convert8BitLinearToFloat( BYTE i )            { return ConvertNBitGammaToFloat<BYTE,  8, DegammaFuncNull>( i ); }
FLOAT Convert8BitGamma_2_0ToFloat( BYTE i )         { return ConvertNBitGammaToFloat<BYTE,  8, DegammaFunc_2_0>( i ); }
FLOAT Convert8BitGamma_2_2ToFloat( BYTE i )         { return ConvertNBitGammaToFloat<BYTE,  8, DegammaFunc_2_2>( i ); }
FLOAT Convert8BitGammasRGBToFloat( BYTE i )         { return ConvertNBitGammaToFloat<BYTE,  8, DegammaFuncsRGB>( i ); }
FLOAT Convert8BitGammaTVToFloat( BYTE i )           { return ConvertNBitGammaToFloat<BYTE,  8, DegammaFuncTV>( i ); }

FLOAT Convert10BitGammaPWLToFloat( WORD i )         { return ConvertNBitGammaToFloat<WORD, 10, DegammaFuncPWL>( i ); }
FLOAT Convert10BitLinearToFloat( WORD i )           { return ConvertNBitGammaToFloat<WORD, 10, DegammaFuncNull>( i ); }
FLOAT Convert10BitGamma_2_0ToFloat( WORD i )        { return ConvertNBitGammaToFloat<WORD, 10, DegammaFunc_2_0>( i ); }
FLOAT Convert10BitGamma_2_2ToFloat( WORD i )        { return ConvertNBitGammaToFloat<WORD, 10, DegammaFunc_2_2>( i ); }
FLOAT Convert10BitGammasRGBToFloat( WORD i )        { return ConvertNBitGammaToFloat<WORD, 10, DegammaFuncsRGB>( i ); }
FLOAT Convert10BitGammaTVToFloat( WORD i )          { return ConvertNBitGammaToFloat<WORD, 10, DegammaFuncTV>( i ); }

// Conversions from one gamma space to another
WORD Convert8BitGammaPWLTo10BitGammasRGB( BYTE i )  { return ConvertFloatTo10BitGammasRGB( Convert8BitGammaPWLToFloat( i ) ); }
WORD Convert8BitLinearTo10BitGammasRGB( BYTE i )    { return ConvertFloatTo10BitGammasRGB( Convert8BitLinearToFloat( i ) ); }
WORD Convert8BitGamma_2_0To10BitGammasRGB( BYTE i ) { return ConvertFloatTo10BitGammasRGB( Convert8BitGamma_2_0ToFloat( i ) ); }
WORD Convert8BitGamma_2_2To10BitGammasRGB( BYTE i ) { return ConvertFloatTo10BitGammasRGB( Convert8BitGamma_2_2ToFloat( i ) ); }
WORD Convert8BitGammaTVTo10BitGammasRGB( BYTE i )   { return ConvertFloatTo10BitGammasRGB( Convert8BitGammaTVToFloat( i ) ); }

WORD Convert10BitGammaPWLTo16BitGammasRGB( WORD i ) { return ConvertFloatTo16BitGammasRGB( Convert10BitGammaPWLToFloat( i ) ); }
WORD Convert10BitLinearTo16BitGammasRGB( WORD i )   { return ConvertFloatTo16BitGammasRGB( Convert10BitLinearToFloat( i ) ); }
WORD Convert10BitGamma_2_0To16BitGammasRGB( WORD i ){ return ConvertFloatTo16BitGammasRGB( Convert10BitGamma_2_0ToFloat( i ) ); }
WORD Convert10BitGamma_2_2To16BitGammasRGB( WORD i ){ return ConvertFloatTo16BitGammasRGB( Convert10BitGamma_2_2ToFloat( i ) ); }
WORD Convert10BitGammaTVTo16BitGammasRGB( WORD i )  { return ConvertFloatTo16BitGammasRGB( Convert10BitGammaTVToFloat( i ) ); }

// Conversions from one gamma space to another, with a second conversion from TV-gamma
// to sRGB appended.  The purpose of the second conversion is to undo the implicit 
// system conversion from sRGB to TV-gamma, and ensure that the final output signal is 
// in sRGB space.  This might be done to match the output of other platforms, in case those
// platforms cannot be changed.
WORD Convert8BitGammaPWLTo10BitGammasRGBOverride( BYTE i )  { return ConvertFloatTo10BitGammasRGBOverride( Convert8BitGammaPWLToFloat( i ) ); }
WORD Convert8BitLinearTo10BitGammasRGBOverride( BYTE i )    { return ConvertFloatTo10BitGammasRGBOverride( Convert8BitLinearToFloat( i ) ); }
WORD Convert8BitGamma_2_0To10BitGammasRGBOverride( BYTE i ) { return ConvertFloatTo10BitGammasRGBOverride( Convert8BitGamma_2_0ToFloat( i ) ); }
WORD Convert8BitGamma_2_2To10BitGammasRGBOverride( BYTE i ) { return ConvertFloatTo10BitGammasRGBOverride( Convert8BitGamma_2_2ToFloat( i ) ); }
WORD Convert8BitGammasRGBTo10BitGammasRGBOverride( BYTE i ) { return ConvertFloatTo10BitGammasRGBOverride( Convert8BitGammasRGBToFloat( i ) ); }

WORD Convert10BitGammaPWLTo16BitGammasRGBOverride( WORD i ) { return ConvertFloatTo16BitGammasRGBOverride( Convert10BitGammaPWLToFloat( i ) ); }
WORD Convert10BitLinearTo16BitGammasRGBOverride( WORD i )   { return ConvertFloatTo16BitGammasRGBOverride( Convert10BitLinearToFloat( i ) ); }
WORD Convert10BitGamma_2_0To16BitGammasRGBOverride( WORD i ){ return ConvertFloatTo16BitGammasRGBOverride( Convert10BitGamma_2_0ToFloat( i ) ); }
WORD Convert10BitGamma_2_2To16BitGammasRGBOverride( WORD i ){ return ConvertFloatTo16BitGammasRGBOverride( Convert10BitGamma_2_2ToFloat( i ) ); }
WORD Convert10BitGammasRGBTo16BitGammasRGBOverride( WORD i ){ return ConvertFloatTo16BitGammasRGBOverride( Convert10BitGammasRGBToFloat( i ) ); }

//--------------------------------------------------------------------------------------
// Definitions for the various UI selections
//--------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------
// Supported texture conversion types.  These determine the linear-to-gamma operation
// performed when generating compressed textures from uncompressed source.  They do 
// not determine the D3D format of the texture.
//--------------------------------------------------------------------------------------
enum TEXTURE_CONVERSIONS 
{
    TEXTURE_CONVERSION_8_BIT_LINEAR, 
    TEXTURE_CONVERSION_8_BIT_GAMMA_PWL, 
    TEXTURE_CONVERSION_8_BIT_GAMMA_2_0, 
    TEXTURE_CONVERSION_8_BIT_GAMMA_2_2, 
    TEXTURE_CONVERSION_10_BIT_LINEAR, 
    TEXTURE_CONVERSION_10_BIT_GAMMA_PWL, 
    TEXTURE_CONVERSION_10_BIT_GAMMA_2_0, 
    TEXTURE_CONVERSION_10_BIT_GAMMA_2_2, 
    TEXTURE_CONVERSION_16_BIT_LINEAR, 

    TEXTURE_CONVERSION_COUNT
};

const WCHAR* g_TextureConversionNames[TEXTURE_CONVERSION_COUNT] = 
{
    L"8-bit linear", 
    L"8-bit sRGB PWL", 
    L"8-bit sRGB 2.0", 
    L"8-bit sRGB 2.2", 
    L"10-bit linear", 
    L"10-bit sRGB PWL", 
    L"10-bit sRGB 2.0", 
    L"10-bit sRGB 2.2", 
    L"16-bit linear", 
};

UINT g_iTexelSizesInBytesForConversion[TEXTURE_CONVERSION_COUNT] = 
{
    4,  // TEXTURE_CONVERSION_8_BIT_LINEAR, 
    4,  // TEXTURE_CONVERSION_8_BIT_GAMMA_PWL, 
    4,  // TEXTURE_CONVERSION_8_BIT_GAMMA_2_0, 
    4,  // TEXTURE_CONVERSION_8_BIT_GAMMA_2_2, 
    4,  // TEXTURE_CONVERSION_10_BIT_LINEAR, 
    4,  // TEXTURE_CONVERSION_10_BIT_GAMMA_PWL,
    4,  // TEXTURE_CONVERSION_10_BIT_GAMMA_2_0,
    4,  // TEXTURE_CONVERSION_10_BIT_GAMMA_2_2,
    8,  // TEXTURE_CONVERSION_16_BIT_LINEAR, 
};

//--------------------------------------------------------------------------------------
// Supported D3D texture format types.  These determine the D3D format of the texture.
// They are not explicitly linked to the gamma-to-linear transform which was used to
// generate the texture data.
//--------------------------------------------------------------------------------------
enum SOURCE_TEXTURE_TYPES 
{
    SOURCE_TYPE_8_BIT_LINEAR, 
    SOURCE_TYPE_8_BIT_GAMMA, 
    SOURCE_TYPE_8_BIT_GAMMA_AS_16, 
    SOURCE_TYPE_10_BIT_LINEAR_AS_16, 
    SOURCE_TYPE_10_BIT_GAMMA_AS_16, 
    SOURCE_TYPE_16_BIT_LINEAR, 

    SOURCE_TYPE_COUNT
};

const WCHAR* g_SourceTypeNames[SOURCE_TYPE_COUNT] = 
{
    L"8-bit linear", 
    L"8-bit sRGB", 
    L"8-bit sRGB as 16-bit", 
    L"10-bit linear as 16-bit", 
    L"10-bit sRGB as 16-bit", 
    L"16-bit linear", 
};

UINT g_iTexelSizesInBytesForSource[TEXTURE_CONVERSION_COUNT] = 
{
    4,  // SOURCE_TYPE_8_BIT_LINEAR, 
    4,  // SOURCE_TYPE_8_BIT_GAMMA, 
    4,  // SOURCE_TYPE_8_BIT_GAMMA_AS_16, 
    4,  // SOURCE_TYPE_10_BIT_LINEAR_AS_16, 
    4,  // SOURCE_TYPE_10_BIT_GAMMA_AS_16, 
    8,  // SOURCE_TYPE_16_BIT_LINEAR, 
};

D3DFORMAT g_d3dfmtTexture[SOURCE_TYPE_COUNT] = 
{
    // SOURCE_TYPE_8_BIT_LINEAR
    D3DFMT_A8B8G8R8, 

    // SOURCE_TYPE_8_BIT_GAMMA --- This format is useless
    (D3DFORMAT) MAKESRGBFMT( D3DFMT_A8B8G8R8 ), 

    // SOURCE_TYPE_8_BIT_GAMMA_AS_16
    (D3DFORMAT) MAKED3DFMT2(
        GPUTEXTUREFORMAT_8_8_8_8_AS_16_16_16_16,// GPUTEXTUREFORMAT 
        GPUENDIAN_8IN32,                        // GPUENDIAN
        TRUE,                                   // BOOL (Tiled)
        GPUSIGN_GAMMA,                          // GPUSIGN (b)
        GPUSIGN_GAMMA,                          // GPUSIGN (g)
        GPUSIGN_GAMMA,                          // GPUSIGN (r)
        GPUSIGN_UNSIGNED,                       // GPUSIGN (a)
        GPUNUMFORMAT_FRACTION,                  // GPUNUMFORMAT
        GPUSWIZZLE_X,                           // GPUSWIZZLE (b)
        GPUSWIZZLE_Y,                           // GPUSWIZZLE (g)
        GPUSWIZZLE_Z,                           // GPUSWIZZLE (r)
        GPUSWIZZLE_W                            // GPUSWIZZLE (a)
    ), 

    // SOURCE_TYPE_10_BIT_LINEAR_AS_16
    D3DFMT_A2B10G10R10, 

    // SOURCE_TYPE_10_BIT_GAMMA_AS_16 --- This format is useless
    (D3DFORMAT) MAKESRGBFMT( D3DFMT_A2B10G10R10 ), 

    // SOURCE_TYPE_16_BIT_LINEAR
    D3DFMT_A16B16G16R16, 
};

//--------------------------------------------------------------------------------------
// Supported gamma-to-linear tranforms performed in the shader following texture
// sampling.  
//--------------------------------------------------------------------------------------
enum DEGAMMA_CALCS 
{
    DEGAMMA_CALC_NONE, 
    DEGAMMA_CALC_PWL, 
    DEGAMMA_CALC_2_0, 
    DEGAMMA_CALC_2_2, 
    DEGAMMA_CALC_SRGB, 
    DEGAMMA_CALC_TV, 

    DEGAMMA_CALC_COUNT
};

const WCHAR* g_ShaderDegammaNames[DEGAMMA_CALC_COUNT] = 
{
    L"none", 
    L"piece-wise linear", 
    L"2.0", 
    L"2.2", 
    L"sRGB", 
    L"TV", 
};

//--------------------------------------------------------------------------------------
// Supported linear-to-gamma tranforms performed in the shader prior to output to EDRAM.
//--------------------------------------------------------------------------------------
enum GAMMA_CALCS 
{
    GAMMA_CALC_NONE, 
    GAMMA_CALC_PWL, 
    GAMMA_CALC_2_0, 
    GAMMA_CALC_2_2, 
    GAMMA_CALC_SRGB, 
    GAMMA_CALC_TV, 

    GAMMA_CALC_COUNT
};

const WCHAR* g_ShaderGammaNames[GAMMA_CALC_COUNT] = 
{
    L"none", 
    L"piece-wise linear", 
    L"2.0", 
    L"2.2", 
    L"sRGB", 
    L"TV", 
};

//--------------------------------------------------------------------------------------
// Supported D3D render target formats.  These determine the format in which pixel 
// shader output gets stored in EDRAM.  
//--------------------------------------------------------------------------------------
enum DEST_RENDER_TARGET_TYPES 
{
    DEST_TYPE_8_BIT_LINEAR, 
    DEST_TYPE_8_BIT_GAMMA, 
    DEST_TYPE_10_BIT_LINEAR, 
    DEST_TYPE_10_BIT_FLOAT_LINEAR, 
    DEST_TYPE_16_BIT_LINEAR, 

    DEST_TYPE_COUNT
};

const WCHAR* g_DestTypeNames[DEST_TYPE_COUNT] = 
{
    L"8-bit linear", 
    L"8-bit sRGB", 
    L"10-bit linear", 
    L"10-bit float linear", 
    L"16-bit linear", 
};

D3DFORMAT g_d3dfmtRenderTarget[DEST_TYPE_COUNT] = 
{
    // DEST_TYPE_8_BIT_LINEAR
    D3DFMT_A8B8G8R8, 

    // DEST_TYPE_8_BIT_GAMMA
    (D3DFORMAT) MAKESRGBFMT( D3DFMT_A8B8G8R8 ), 

    // DEST_TYPE_10_BIT_LINEAR
    D3DFMT_A2B10G10R10, 

    // DEST_TYPE_10_BIT_FLOAT_LINEAR
    D3DFMT_A2B10G10R10F_EDRAM, 

    // DEST_TYPE_16_BIT_LINEAR
    D3DFMT_A16B16G16R16_EDRAM, 
};

//--------------------------------------------------------------------------------------
// Supported D3D front buffer formats.  A resolve from an sRGB format to a linear 
// format triggers an automatic gamma-to-linear conversion in hardware.
//--------------------------------------------------------------------------------------
enum FRONT_BUFFER_TYPES
{
    FRONT_BUFFER_8_BIT_GAMMA, 
    FRONT_BUFFER_8_BIT_LINEAR, 
    FRONT_BUFFER_10_BIT_GAMMA, 
    FRONT_BUFFER_10_BIT_LINEAR,

    FRONT_BUFFER_COUNT
};

const WCHAR* g_FrontBufferNames[FRONT_BUFFER_COUNT] = 
{
    L"8-bit sRGB", 
    L"8-bit linear", 
    L"10-bit sRGB", 
    L"10-bit linear", 
};

D3DFORMAT g_d3dfmtFrontBuffer[FRONT_BUFFER_COUNT] = 
{
    //FRONT_BUFFER_8_BIT_GAMMA, 
    (D3DFORMAT) MAKESRGBFMT( D3DFMT_LE_X8R8G8B8 ), 

    //FRONT_BUFFER_8_BIT_LINEAR, 
    D3DFMT_LE_X8R8G8B8, 

    //FRONT_BUFFER_10_BIT_GAMMA, 
    (D3DFORMAT) MAKESRGBFMT( D3DFMT_LE_X2R10G10B10 ), 

    //FRONT_BUFFER_10_BIT_LINEAR,
    D3DFMT_LE_X2R10G10B10, 
};

//--------------------------------------------------------------------------------------
// Supported types of gamma ramp passed to the hardware via SetGammaRamp or
// SetPWLGamma.  These affect the automatic conversion of front buffer values when 
// generating the final output signal.
//--------------------------------------------------------------------------------------
enum DISPLAY_GAMMA_TYPES 
{
    // The 'override' options manually undo the system conversion from sRGB to TV, 
    // so that the final output signal is sRGB, like on other platforms.

    DISPLAY_GAMMA_PWL, 
    DISPLAY_GAMMA_PWL_OVERRIDE, 
    DISPLAY_GAMMA_2_0, 
    DISPLAY_GAMMA_2_0_OVERRIDE, 
    DISPLAY_GAMMA_2_2, 
    DISPLAY_GAMMA_2_2_OVERRIDE, 
    DISPLAY_GAMMA_LINEAR, 
    DISPLAY_GAMMA_LINEAR_OVERRIDE, 
    DISPLAY_GAMMA_SRGB, 
    DISPLAY_GAMMA_SRGB_OVERRIDE,

    DISPLAY_GAMMA_COUNT
};

const WCHAR* g_DisplayGammaNames[DISPLAY_GAMMA_COUNT] = 
{
    L"gamma PWL to sRGB", 
    L"gamma PWL to sRGB (override)", 
    L"gamma 2.0 to sRGB", 
    L"gamma 2.0 to sRGB (override)", 
    L"gamma 2.2 to sRGB", 
    L"gamma 2.2 to sRGB (override)", 
    L"linear to sRGB", 
    L"linear to sRGB (override)", 
    L"sRGB to sRGB",
    L"sRGB to sRGB (override)", 
};

typedef struct { D3DGAMMARAMP m_8BitGamma; D3DPWLGAMMA m_PWLGamma; } GenericGammaRamp;

GenericGammaRamp g_GammaRamp[DISPLAY_GAMMA_COUNT];

enum TEST_TEXTURE_TYPE
{
    TEST_TEXTURE_COLOR_GRADIENT, 
    TEST_TEXTURE_GRAYSCALE_GRADIENT, 
    TEST_TEXTURE_SOLID, 

    TEST_TEXTURE_COUNT
};

const WCHAR* g_TestTextureNames[TEST_TEXTURE_COUNT] = 
{
    L"Color gradient", 
    L"Grayscale blocks", 
    L"Solid", 
};


enum TEST_GEOMETRY_TYPE
{
    TEST_GEOMETRY_FULLSCREEN_QUAD, 
    TEST_GEOMETRY_SPHERE, 

    TEST_GEOMETRY_COUNT
};

const WCHAR* g_TestGeometryNames[TEST_GEOMETRY_COUNT] = 
{
    L"Fullscreen quad", 
    L"Sphere", 
};


enum TEST_SHADING_TYPE
{
    TEST_SHADING_TEXTURE_COPY, 
    TEST_SHADING_DIFFUSE_LIT, 

    TEST_SHADING_COUNT
};

const WCHAR* g_TestShadingNames[TEST_SHADING_COUNT] = 
{
    L"Ambient", 
    L"Directional", 
};


//--------------------------------------------------------------------------------------
// Dither test settings.  There are two entirely different tests you can run.  
// Each only works properly under certain ideal conditions.
//
// If you have a setup where your front buffer is displayed at native resolution --- 
// pixel-accurate, with no processing or loss either in the Xbox system software, the
// cable, or the TV set --- then you can run the LINEAR_SPACE test.  This setup may
// be very difficult to guarantee.
// 
// If you have a setup which is highly non-native resolution --- front buffer size
// different from dash settings different from TV native size, or you use an interlaced
// signal, or your TV has heavy internal filtering --- then the GAMMA_SPACE test may
// be appropriate, at least for dither patterns which are sufficiently mixed for blurring
// to occur.
//
// I get good results from the GAMMA_SPACE test by using:
//      - 1280x720 front buffer
//      - 480p widescreen in the dash
//      - component connection
//      - Xbox video connector switch set to "TV"
//      - Sharp Aquos LCD TV at factory defaults
//--------------------------------------------------------------------------------------

enum DITHER_COVERAGE
{
    DITHER_COVERAGE_NONE, 
    DITHER_COVERAGE_FULLSCREEN, 
    DITHER_COVERAGE_SIDEBYSIDE, 

    DITHER_COVERAGE_COUNT
};

const WCHAR* g_DitherCoverageNames[DITHER_COVERAGE_COUNT] = 
{
    L"None", 
    L"Fullscreen", 
    L"Vertical strips", 
};

enum DITHER_TEST
{
    DITHER_TEST_LINEAR_SPACE, 
    DITHER_TEST_GAMMA_SPACE, 

    DITHER_TEST_COUNT
};

const WCHAR* g_DitherTestNames[DITHER_TEST_COUNT] = 
{
    L"Linear space", 
    L"Gamma space", 
};

//--------------------------------------------------------------------------------------
// Presets:
//
// Each of the following determines a particular combination of all the preceding
// gamma parameters.  Unlike arbitrary combinations, each of these is "correct" in the
// sense that given raw linear source values, each produces a final output which 
// will display linear values on a standard TV set.  The presets do differ in how
// much precision they maintain through the pipeline.
//--------------------------------------------------------------------------------------
enum GammaPresetTypes   // Each of these is a common combination of param options
{
    PRESET_8_BIT_LINEAR, 
    PRESET_8_BIT_SRGB_PWL_AS_8, 
    PRESET_8_BIT_SRGB_PWL, 
    PRESET_8_BIT_SRGB_2_0, 
    PRESET_8_BIT_SRGB_2_2, 
    PRESET_10_BIT_HYBRID, 
    PRESET_10_BIT_LINEAR, 
    PRESET_10_BIT_SRGB_PWL, 
    PRESET_10_BIT_SRGB_2_0, 
    PRESET_10_BIT_SRGB_2_2, 
    PRESET_16_BIT_HYBRID, 

    PRESET_COUNT
};

const WCHAR* g_GammaPresetNames[PRESET_COUNT] = 
{
    L"8-bit linear always", 
    L"8-bit sRGB PWL (DO NOT USE!)", 
    L"8-bit sRGB PWL always", 
    L"8-bit sRGB 2.0 always", 
    L"8-bit sRGB 2.2 always", 
    L"8-bit sRGB PWL in, 10-bit linear out", 
    L"10-bit linear always", 
    L"10-bit sRGB PWL always", 
    L"10-bit sRGB 2.0 always", 
    L"10-bit sRGB 2.2 always", 
    L"10-bit sRGB PWL in, 16-bit linear out", 
};

enum GammaPresetParams 
{
    GAMMA_PRESET_TEXTURE_CONVERSION, 
    GAMMA_PRESET_SOURCE_TYPE, 
    GAMMA_PRESET_SHADER_DEGAMMA, 
    GAMMA_PRESET_SHADER_GAMMA,
    GAMMA_PRESET_DEST_TYPE, 
    GAMMA_PRESET_FRONT_BUFFER, 
    GAMMA_PRESET_DISPLAY_GAMMA, 

    GAMMA_PRESET_COUNT
};

typedef UINT GammaPreset[GAMMA_PRESET_COUNT];

GammaPreset g_GammaPresets[] = 
{
    // PRESET_8_BIT_LINEAR, 
    {
        TEXTURE_CONVERSION_8_BIT_LINEAR,    // GAMMA_PRESET_TEXTURE_CONVERSION, 
        SOURCE_TYPE_8_BIT_LINEAR,           // GAMMA_PRESET_SOURCE_TYPE, 
        DEGAMMA_CALC_NONE,                  // GAMMA_PRESET_SHADER_DEGAMMA, 
        GAMMA_CALC_NONE,                    // GAMMA_PRESET_SHADER_GAMMA,
        DEST_TYPE_8_BIT_LINEAR,             // GAMMA_PRESET_DEST_TYPE, 
        FRONT_BUFFER_8_BIT_LINEAR,          // GAMMA_PRESET_FRONT_BUFFER, 
        DISPLAY_GAMMA_LINEAR,               // GAMMA_PRESET_DISPLAY_GAMMA, 
    }, 

    // PRESET_8_BIT_SRGB_PWL_AS_8, 
    {
        TEXTURE_CONVERSION_8_BIT_GAMMA_PWL, // GAMMA_PRESET_TEXTURE_CONVERSION, 
        SOURCE_TYPE_8_BIT_GAMMA,            // GAMMA_PRESET_SOURCE_TYPE, 
        DEGAMMA_CALC_NONE,                  // GAMMA_PRESET_SHADER_DEGAMMA, 
        GAMMA_CALC_NONE,                    // GAMMA_PRESET_SHADER_GAMMA,
        DEST_TYPE_8_BIT_LINEAR,             // GAMMA_PRESET_DEST_TYPE, 
        FRONT_BUFFER_8_BIT_LINEAR,          // GAMMA_PRESET_FRONT_BUFFER, 
        DISPLAY_GAMMA_LINEAR,               // GAMMA_PRESET_DISPLAY_GAMMA, 
    }, 

    // PRESET_8_BIT_SRGB_PWL, 
    {
        TEXTURE_CONVERSION_8_BIT_GAMMA_PWL, // GAMMA_PRESET_TEXTURE_CONVERSION, 
        SOURCE_TYPE_8_BIT_GAMMA_AS_16,      // GAMMA_PRESET_SOURCE_TYPE, 
        DEGAMMA_CALC_NONE,                  // GAMMA_PRESET_SHADER_DEGAMMA, 
        GAMMA_CALC_NONE,                    // GAMMA_PRESET_SHADER_GAMMA,
        DEST_TYPE_8_BIT_GAMMA,              // GAMMA_PRESET_DEST_TYPE, 
        FRONT_BUFFER_8_BIT_GAMMA,           // GAMMA_PRESET_FRONT_BUFFER, 
        DISPLAY_GAMMA_PWL,                  // GAMMA_PRESET_DISPLAY_GAMMA, 
    }, 

    // PRESET_8_BIT_SRGB_2_0, 
    {
        TEXTURE_CONVERSION_8_BIT_GAMMA_2_0, // GAMMA_PRESET_TEXTURE_CONVERSION, 
        SOURCE_TYPE_8_BIT_LINEAR,           // GAMMA_PRESET_SOURCE_TYPE, 
        DEGAMMA_CALC_2_0,                   // GAMMA_PRESET_SHADER_DEGAMMA, 
        GAMMA_CALC_2_0,                     // GAMMA_PRESET_SHADER_GAMMA,
        DEST_TYPE_8_BIT_LINEAR,             // GAMMA_PRESET_DEST_TYPE, 
        FRONT_BUFFER_8_BIT_LINEAR,          // GAMMA_PRESET_FRONT_BUFFER, 
        DISPLAY_GAMMA_2_0,                  // GAMMA_PRESET_DISPLAY_GAMMA, 
    }, 

    // PRESET_8_BIT_SRGB_2_2, 
    {
        TEXTURE_CONVERSION_8_BIT_GAMMA_2_2, // GAMMA_PRESET_TEXTURE_CONVERSION, 
        SOURCE_TYPE_8_BIT_LINEAR,           // GAMMA_PRESET_SOURCE_TYPE, 
        DEGAMMA_CALC_2_2,                   // GAMMA_PRESET_SHADER_DEGAMMA, 
        GAMMA_CALC_2_2,                     // GAMMA_PRESET_SHADER_GAMMA,
        DEST_TYPE_8_BIT_LINEAR,             // GAMMA_PRESET_DEST_TYPE, 
        FRONT_BUFFER_8_BIT_LINEAR,          // GAMMA_PRESET_FRONT_BUFFER, 
        DISPLAY_GAMMA_2_2,                  // GAMMA_PRESET_DISPLAY_GAMMA, 
    }, 

    // PRESET_10_BIT_HYBRID, 
    {
        TEXTURE_CONVERSION_8_BIT_GAMMA_PWL, // GAMMA_PRESET_TEXTURE_CONVERSION, 
        SOURCE_TYPE_8_BIT_GAMMA_AS_16,      // GAMMA_PRESET_SOURCE_TYPE, 
        DEGAMMA_CALC_NONE,                  // GAMMA_PRESET_SHADER_DEGAMMA, 
        GAMMA_CALC_NONE,                    // GAMMA_PRESET_SHADER_GAMMA,
        DEST_TYPE_10_BIT_LINEAR,            // GAMMA_PRESET_DEST_TYPE, 
        FRONT_BUFFER_10_BIT_LINEAR,         // GAMMA_PRESET_FRONT_BUFFER, 
        DISPLAY_GAMMA_LINEAR,               // GAMMA_PRESET_DISPLAY_GAMMA, 
    }, 

    // PRESET_10_BIT_LINEAR, 
    {
        TEXTURE_CONVERSION_10_BIT_LINEAR,   // GAMMA_PRESET_TEXTURE_CONVERSION, 
        SOURCE_TYPE_10_BIT_LINEAR_AS_16,    // GAMMA_PRESET_SOURCE_TYPE, 
        DEGAMMA_CALC_NONE,                  // GAMMA_PRESET_SHADER_DEGAMMA, 
        GAMMA_CALC_NONE,                    // GAMMA_PRESET_SHADER_GAMMA,
        DEST_TYPE_10_BIT_LINEAR,            // GAMMA_PRESET_DEST_TYPE, 
        FRONT_BUFFER_10_BIT_LINEAR,         // GAMMA_PRESET_FRONT_BUFFER, 
        DISPLAY_GAMMA_LINEAR,               // GAMMA_PRESET_DISPLAY_GAMMA, 
    }, 

    // PRESET_10_BIT_SRGB_PWL, 
    {
        TEXTURE_CONVERSION_10_BIT_GAMMA_PWL,// GAMMA_PRESET_TEXTURE_CONVERSION, 
        SOURCE_TYPE_10_BIT_GAMMA_AS_16,     // GAMMA_PRESET_SOURCE_TYPE, 
        DEGAMMA_CALC_NONE,                  // GAMMA_PRESET_SHADER_DEGAMMA, 
        GAMMA_CALC_PWL,                     // GAMMA_PRESET_SHADER_GAMMA,
        DEST_TYPE_10_BIT_LINEAR,            // GAMMA_PRESET_DEST_TYPE, 
        FRONT_BUFFER_10_BIT_GAMMA,          // GAMMA_PRESET_FRONT_BUFFER, 
        DISPLAY_GAMMA_PWL,                  // GAMMA_PRESET_DISPLAY_GAMMA, 
    }, 

    // PRESET_10_BIT_SRGB_2_0, 
    {
        TEXTURE_CONVERSION_10_BIT_GAMMA_2_0,// GAMMA_PRESET_TEXTURE_CONVERSION, 
        SOURCE_TYPE_10_BIT_LINEAR_AS_16,    // GAMMA_PRESET_SOURCE_TYPE, 
        DEGAMMA_CALC_2_0,                   // GAMMA_PRESET_SHADER_DEGAMMA, 
        GAMMA_CALC_2_0,                     // GAMMA_PRESET_SHADER_GAMMA,
        DEST_TYPE_10_BIT_LINEAR,            // GAMMA_PRESET_DEST_TYPE, 
        FRONT_BUFFER_10_BIT_LINEAR,         // GAMMA_PRESET_FRONT_BUFFER, 
        DISPLAY_GAMMA_2_0,                  // GAMMA_PRESET_DISPLAY_GAMMA, 
    }, 

    // PRESET_10_BIT_SRGB_2_2, 
    {
        TEXTURE_CONVERSION_10_BIT_GAMMA_2_2,// GAMMA_PRESET_TEXTURE_CONVERSION, 
        SOURCE_TYPE_10_BIT_LINEAR_AS_16,    // GAMMA_PRESET_SOURCE_TYPE, 
        DEGAMMA_CALC_2_2,                   // GAMMA_PRESET_SHADER_DEGAMMA, 
        GAMMA_CALC_2_2,                     // GAMMA_PRESET_SHADER_GAMMA,
        DEST_TYPE_10_BIT_LINEAR,            // GAMMA_PRESET_DEST_TYPE, 
        FRONT_BUFFER_10_BIT_LINEAR,         // GAMMA_PRESET_FRONT_BUFFER, 
        DISPLAY_GAMMA_2_2,                  // GAMMA_PRESET_DISPLAY_GAMMA, 
    }, 

    // PRESET_16_BIT_HYBRID, 
    {
        TEXTURE_CONVERSION_10_BIT_GAMMA_PWL,// GAMMA_PRESET_TEXTURE_CONVERSION, 
        SOURCE_TYPE_10_BIT_GAMMA_AS_16,     // GAMMA_PRESET_SOURCE_TYPE, 
        DEGAMMA_CALC_NONE,                  // GAMMA_PRESET_SHADER_DEGAMMA, 
        GAMMA_CALC_NONE,                    // GAMMA_PRESET_SHADER_GAMMA,
        DEST_TYPE_16_BIT_LINEAR,            // GAMMA_PRESET_DEST_TYPE, 
        FRONT_BUFFER_10_BIT_LINEAR,         // GAMMA_PRESET_FRONT_BUFFER, 
        DISPLAY_GAMMA_LINEAR,               // GAMMA_PRESET_DISPLAY_GAMMA, 
    }, 
};


//--------------------------------------------------------------------------------------
// Structs aliasing color data as various formats
//--------------------------------------------------------------------------------------
struct GenericRawColorBuffer 
{
    union
    {
        void*                       m_pVoid;
        COLOR_32_32_32_32F*         m_pCOLOR_32_32_32_32F;
    };
};

struct GenericCompressedColorBuffer 
{
    union
    {
        void*                       m_pVoid;
        COLOR_8_8_8_8*              m_pCOLOR_8_8_8_8;
        COLOR_2_10_10_10*           m_pCOLOR_2_10_10_10;
        COLOR_16_16_16_16*          m_pCOLOR_16_16_16_16;
    };
    D3DXVECTOR3                     m_vAmbient;
};

struct AllFormatColorBuffer
{
    UINT                            m_iTextureWidth;
    UINT                            m_iTextureHeight;
    GenericRawColorBuffer           m_pRawData;
    GenericCompressedColorBuffer    m_pCompressedData[TEXTURE_CONVERSION_COUNT];
    IDirect3DTexture9*              m_pTexture[SOURCE_TYPE_COUNT];
};

//--------------------------------------------------------------------------------------
// Supported tweakable UI parameters.
//--------------------------------------------------------------------------------------
enum GammaParamTypes 
{
    GAMMA_PARAM_PRESET, 

    GAMMA_PARAM_TEXTURE_CONVERSION, 
    GAMMA_PARAM_SOURCE_TYPE, 
    GAMMA_PARAM_SHADER_DEGAMMA, 
    GAMMA_PARAM_SHADER_GAMMA,
    GAMMA_PARAM_DEST_TYPE, 
    GAMMA_PARAM_FRONT_BUFFER, 
    GAMMA_PARAM_DISPLAY_GAMMA, 

    GAMMA_PARAM_TEXTURE_AMBIENT, 
    GAMMA_PARAM_RUNTIME_AMBIENT, 
    GAMMA_PARAM_LIGHTING_COLOR, 

    GAMMA_PARAM_TEST_TEXTURE, 
    GAMMA_PARAM_TEST_GEOMETRY, 
    GAMMA_PARAM_TEST_SHADING, 
    GAMMA_PARAM_DITHER, 

    GAMMA_PARAM_COUNT
};

class GammaParam
{
public:
    static const DWORD  m_dwActiveParamColor = 0xffffff00;   
    static const DWORD  m_dwInactiveParamColor = 0xffffffff;   
    static const DWORD  m_dwActiveOptionColor = 0xff00ff00;   
    static const DWORD  m_dwInactiveOptionColor = 0xffff00ff;   
    static const FLOAT  m_fActiveParamScale;
    static const FLOAT  m_fInactiveParamScale;
    static const FLOAT  m_fParamX;
    static const FLOAT  m_fOptionX;

    GammaParam( const WCHAR* ParamName ) : m_ParamName( ParamName )
    {}

    virtual VOID RenderOptionUI( ATG::Font* pFont, FLOAT fParamY, BOOL bActive ) = 0;
    VOID RenderUI( ATG::Font* pFont, FLOAT fParamY, BOOL bActive )
    {
        if( bActive ) 
        {
            pFont->SetScaleFactors( m_fActiveParamScale, m_fActiveParamScale );
            pFont->DrawText( m_fParamX, fParamY, m_dwActiveParamColor, m_ParamName );
        }
        else
        {
            pFont->SetScaleFactors( m_fInactiveParamScale, m_fInactiveParamScale );
            pFont->DrawText( m_fParamX, fParamY, m_dwInactiveParamColor, m_ParamName );
        }
        RenderOptionUI( pFont, fParamY, bActive );
    }

    virtual VOID        DecreaseValue( FLOAT fScale = 1.0f ) = 0;
    virtual VOID        IncreaseValue( FLOAT fScale = 1.0f ) = 0;

protected:
    const WCHAR*        m_ParamName;
};

const FLOAT  GammaParam::m_fActiveParamScale = 1.1f;
const FLOAT  GammaParam::m_fInactiveParamScale = 1.1f;
const FLOAT  GammaParam::m_fParamX = 0.0f;
const FLOAT  GammaParam::m_fOptionX = 240.0f;

class GammaParamEnum : public GammaParam
{
public:
    GammaParamEnum( const WCHAR* ParamName, const WCHAR** OptionNames, UINT iCount )
        : GammaParam( ParamName )
        , m_OptionNames( OptionNames )
        , m_iCount( iCount )
        , m_iValue( 0 )
    {}

    VOID RenderOptionUI( ATG::Font* pFont, FLOAT fParamY, BOOL bActive )
    {
        UINT iValue = GetValue();
        const WCHAR *OptionName = ( iValue >= 0 && iValue < m_iCount ) ? m_OptionNames[iValue] : L"n/a";
        if( bActive ) 
        {
            WCHAR SelectText[256];
            swprintf_s( SelectText, L"< %s >", OptionName );
            pFont->DrawText( m_fOptionX, fParamY, m_dwActiveOptionColor, SelectText );
        }
        else
        {
            pFont->DrawText( m_fOptionX, fParamY, m_dwInactiveOptionColor, OptionName );
        }
    }

    UINT                GetValue() { return m_iValue; };
    VOID                SetValue( UINT iValue ) { m_iValue = iValue; };
    VOID                DecreaseValue( FLOAT fScale = 1.0f ) { m_iValue += m_iCount - 1; m_iValue %= m_iCount; }
    VOID                IncreaseValue( FLOAT fScale = 1.0f ) { m_iValue += 1;            m_iValue %= m_iCount; }

protected:
    const WCHAR**       m_OptionNames;
    UINT                m_iCount;
    UINT                m_iValue;
};

class GammaParamGrayscale : public GammaParam
{
public:
    GammaParamGrayscale( const WCHAR* ParamName, const D3DXVECTOR3& vColor, const D3DXVECTOR3& vIncr )
        : GammaParam( ParamName )
        , m_vColor( vColor )
        , m_vIncr( vIncr )
    {}

    VOID RenderOptionUI( ATG::Font* pFont, FLOAT fParamY, BOOL bActive )
    {
        WCHAR OptionText[256];
        swprintf_s( OptionText, L"%2.2f, %2.2f, %2.2f", m_vColor.x, m_vColor.y, m_vColor.z );
        DWORD dwColor = bActive ? m_dwActiveOptionColor : m_dwInactiveOptionColor;
        pFont->DrawText( m_fOptionX, fParamY, dwColor, OptionText );
    }

    const D3DXVECTOR3&  GetValue() { return m_vColor; }
    VOID                DecreaseValue( FLOAT fScale = 1.0f ) 
    { 
        D3DXVECTOR3 vIncr;
        D3DXVec3Scale( &vIncr, &m_vIncr, fScale );
        D3DXVec3Subtract( &m_vColor, &m_vColor, &vIncr );
    }
    VOID                IncreaseValue( FLOAT fScale = 1.0f ) 
    { 
        D3DXVECTOR3 vIncr;
        D3DXVec3Scale( &vIncr, &m_vIncr, fScale );
        D3DXVec3Add( &m_vColor, &m_vColor, &vIncr );
    }

protected:
    D3DXVECTOR3         m_vColor;
    D3DXVECTOR3         m_vIncr;
};

typedef GammaParam* GammaParamArray[GAMMA_PARAM_COUNT];

//-----------------------------------------------------------------------------
// Name: class Sample
// Desc: Main class to run this application. Most functionality is inherited
//       from the ATG::Application base class.
//-----------------------------------------------------------------------------
class Sample : public ATG::Application
{
    ATG::Font                       m_Font;                 // Font for drawing text
    ATG::Timer                      m_Timer;                // Timer
    ATG::Help                       m_Help;                 // Display help
    BOOL                            m_bDrawHelp;
    BOOL                            m_bDrawUI;

    UINT                            m_iActiveUIParameter;   // Which UI item is affected by l/r input

    // The various tweakable UI parameters
    GammaParamEnum                  m_PresetParam;          // Which preset is active (or 0xffffffff for custom settings)

    GammaParamEnum                  m_TextureConversionParam;
    GammaParamEnum                  m_SourceTypeParam;
    GammaParamEnum                  m_ShaderDegammaParam;
    GammaParamEnum                  m_ShaderGammaParam;
    GammaParamEnum                  m_DestTypeParam;
    GammaParamEnum                  m_FrontBufferParam;
    GammaParamEnum                  m_DisplayGammaParam;

    GammaParamGrayscale             m_TextureAmbientParam;
    GammaParamGrayscale             m_RuntimeAmbientParam;
    GammaParamGrayscale             m_LightingColorParam;

    // $TODO:  Make these UI params
    D3DXVECTOR3                     m_vLightDirection;

    GammaParamEnum                  m_TestTextureParam;
    GammaParamEnum                  m_TestGeometryParam;
    GammaParamEnum                  m_DitherCoverageParam;
    GammaParamEnum                  m_DitherTestParam;

    GammaParam*                     m_ParamArray[GAMMA_PARAM_COUNT];
    GammaParamEnum*                 m_PresetParamArray[GAMMA_PRESET_COUNT];

    AllFormatColorBuffer            m_ColorBuffer[TEST_TEXTURE_COUNT];

    IDirect3DSurface9*              m_pRenderTarget[DEST_TYPE_COUNT];
    void*                           m_pFrontBufferData;
    IDirect3DTexture9*              m_pFrontBuffer[FRONT_BUFFER_COUNT];

    // Shaders for the gamma tests, including one pixel shader per degamma/gamma combination
    IDirect3DVertexShader9*         m_pVertexShader;
    IDirect3DPixelShader9*          m_pDitherPS;
    IDirect3DPixelShader9*          m_pGammaGenericPS;

    // Resources for test geometry
    IDirect3DVertexDeclaration9*    m_pVtxDecl;
    IDirect3DVertexBuffer9*         m_pVB[TEST_GEOMETRY_COUNT];
    IDirect3DIndexBuffer9*          m_pIB[TEST_GEOMETRY_COUNT];
    UINT                            m_iIndexCount[TEST_GEOMETRY_COUNT];
    XMMATRIX                        m_matWorldToView[TEST_GEOMETRY_COUNT];
    XMMATRIX                        m_matWorldToScreen[TEST_GEOMETRY_COUNT];

    // Create test geometry
    VOID GenerateFullscreenQuad( D3DVertexBuffer** pVB,
                                 D3DIndexBuffer** pIB, UINT* numIndices );
    VOID GenerateTexturedSphereGeometry( UINT dwNumSlices, UINT dwNumStacks,
        TestGeometryVertex* pData, FLOAT fUVScaler );
    VOID GenerateSphereIndices( UINT dwNumSlices, UINT dwNumStacks, WORD* pIndices );
    VOID GenerateTexturedSphere( UINT numSlices, UINT numStacks, D3DVertexBuffer** pVB,
                                 D3DIndexBuffer** pIB, UINT* numIndices, FLOAT fUVScaler );

    // Simulated "offline" computation of texture data
    VOID                            ComputeRawColorGradientData();
    VOID                            ComputeRawGrayscaleGradientData();
    VOID                            ComputeRawSolidData();

    // Compress texture data
    VOID                            ComputeCompressedData( UINT iTestTexture, UINT iTextureConversionType );

    // Generate Gamma ramps for video output
    VOID                            CreateGammaRamp( UINT iGammaType );

    // Draw/Resolve helpers
    VOID DrawGeometryToRenderTarget(IDirect3DTexture9* pSrcTexture,
        IDirect3DSurface9* pDstRenderTarget,
        IDirect3DPixelShader9* pPixelShader, 
        IDirect3DVertexBuffer9* pVB, 
        IDirect3DIndexBuffer9* pIB, 
        UINT iIBSize);
    VOID CopyRenderTargetToTexture(IDirect3DSurface9* pSrcRenderTarget,
        IDirect3DTexture9* pDstTexture );

public:
    Sample();

    HRESULT Initialize();
    HRESULT Update();
    HRESULT Render();
};


//-----------------------------------------------------------------------------
// Name: Sample::Sample
// Desc: Initializes the UI elements.
//-----------------------------------------------------------------------------
Sample::Sample()
: m_PresetParam( L"Gamma Preset", g_GammaPresetNames, ARRAYSIZE(g_GammaPresetNames) )
, m_TextureConversionParam( L"  Texture convert", g_TextureConversionNames, ARRAYSIZE(g_TextureConversionNames) )
, m_SourceTypeParam( L"  Source", g_SourceTypeNames, ARRAYSIZE(g_SourceTypeNames) )
, m_ShaderDegammaParam( L"  Shader Degamma", g_ShaderDegammaNames, ARRAYSIZE(g_ShaderDegammaNames) )
, m_ShaderGammaParam( L"  Shader Gamma", g_ShaderGammaNames, ARRAYSIZE(g_ShaderGammaNames) )
, m_DestTypeParam( L"  Dest", g_DestTypeNames, ARRAYSIZE(g_DestTypeNames) )
, m_FrontBufferParam( L"  Front Buffer", g_FrontBufferNames, ARRAYSIZE(g_FrontBufferNames) )
, m_DisplayGammaParam( L"  Output ramp", g_DisplayGammaNames, ARRAYSIZE(g_DisplayGammaNames) )
, m_TextureAmbientParam( L"Texture ambient", D3DXVECTOR3( 0.5f, 0.5f, 0.5f ), D3DXVECTOR3( 0.1f, 0.1f, 0.1f ) )
, m_RuntimeAmbientParam( L"Runtime ambient", D3DXVECTOR3( 0.5f, 0.5f, 0.5f ), D3DXVECTOR3( 0.01f, 0.01f, 0.01f ) )
, m_LightingColorParam( L"Lighting Color", D3DXVECTOR3( 0.0f, 0.0f, 0.0f ), D3DXVECTOR3( 0.01f, 0.01f, 0.01f ) )
, m_TestTextureParam( L"Texture", g_TestTextureNames, ARRAYSIZE(g_TestTextureNames) )
, m_TestGeometryParam( L"Geometry", g_TestGeometryNames, ARRAYSIZE(g_TestGeometryNames) )
, m_DitherCoverageParam( L"Dither area", g_DitherCoverageNames, ARRAYSIZE(g_DitherCoverageNames) )
, m_DitherTestParam( L"Dither test", g_DitherTestNames, ARRAYSIZE(g_DitherTestNames) )
{
    {
        UINT i = 0;
        m_ParamArray[i++] =  &m_PresetParam; 
        m_ParamArray[i++] =  &m_TextureConversionParam;
        m_ParamArray[i++] =  &m_SourceTypeParam;
        m_ParamArray[i++] =  &m_ShaderDegammaParam;
        m_ParamArray[i++] =  &m_ShaderGammaParam;
        m_ParamArray[i++] =  &m_DestTypeParam;
        m_ParamArray[i++] =  &m_FrontBufferParam;
        m_ParamArray[i++] =  &m_DisplayGammaParam;
        m_ParamArray[i++] =  &m_TextureAmbientParam;
        m_ParamArray[i++] =  &m_RuntimeAmbientParam;
        m_ParamArray[i++] =  &m_LightingColorParam;
        m_ParamArray[i++] =  &m_TestTextureParam;
        m_ParamArray[i++] =  &m_TestGeometryParam;
        m_ParamArray[i++] =  &m_DitherCoverageParam;
        m_ParamArray[i++] =  &m_DitherTestParam;
        assert( i == GAMMA_PARAM_COUNT );
    }

    {
        UINT i = 0;
        m_PresetParamArray[i++] =  &m_TextureConversionParam; 
        m_PresetParamArray[i++] =  &m_SourceTypeParam; 
        m_PresetParamArray[i++] =  &m_ShaderDegammaParam; 
        m_PresetParamArray[i++] =  &m_ShaderGammaParam; 
        m_PresetParamArray[i++] =  &m_DestTypeParam; 
        m_PresetParamArray[i++] =  &m_FrontBufferParam; 
        m_PresetParamArray[i++] =  &m_DisplayGammaParam; 
        assert( i == GAMMA_PRESET_COUNT );
    }
}


//--------------------------------------------------------------------------------------
// Name: GenerateFullscreenQuad()
// Desc: Creates vertex and index buffer for a single fullscreen quad 
//       (overkill, but allows the code to look more parallel)
//--------------------------------------------------------------------------------------
VOID Sample::GenerateFullscreenQuad( D3DVertexBuffer** pVB,
                                 D3DIndexBuffer** pIB, UINT* numIndices )
{
    // Create a vertex buffer and copy the mesh vertex data into it
    m_pd3dDevice->BlockUntilIdle();
    m_pd3dDevice->CreateVertexBuffer(
        sizeof( TestGeometryVertex ) * 4,
        0, 0, D3DPOOL_DEFAULT, pVB, NULL );
    TestGeometryVertex* pVBData = NULL;
    ( *pVB )->Lock( 0, 0, ( VOID** )&pVBData, 0 );

    pVBData[0].Position.x = -1.0f;
    pVBData[0].Position.y = -1.0f;
    pVBData[0].Position.z =  0.0f;
    pVBData[0].TexCoord.x =  0.0f;
    pVBData[0].TexCoord.y =  0.0f;
    pVBData[0].Normal.x   =  0.0f;
    pVBData[0].Normal.y   =  0.0f;
    pVBData[0].Normal.z   = -1.0f;

    pVBData[1].Position.x =  1.0f;
    pVBData[1].Position.y = -1.0f;
    pVBData[1].Position.z =  0.0f;
    pVBData[1].TexCoord.x =  1.0f;
    pVBData[1].TexCoord.y =  0.0f;
    pVBData[1].Normal.x   =  0.0f;
    pVBData[1].Normal.y   =  0.0f;
    pVBData[1].Normal.z   = -1.0f;

    pVBData[2].Position.x = -1.0f;
    pVBData[2].Position.y =  1.0f;
    pVBData[2].Position.z =  0.0f;
    pVBData[2].TexCoord.x =  0.0f;
    pVBData[2].TexCoord.y =  1.0f;
    pVBData[2].Normal.x   =  0.0f;
    pVBData[2].Normal.y   =  0.0f;
    pVBData[2].Normal.z   = -1.0f;

    pVBData[3].Position.x =  1.0f;
    pVBData[3].Position.y =  1.0f;
    pVBData[3].Position.z =  0.0f;
    pVBData[3].TexCoord.x =  1.0f;
    pVBData[3].TexCoord.y =  1.0f;
    pVBData[3].Normal.x   =  0.0f;
    pVBData[3].Normal.y   =  0.0f;
    pVBData[3].Normal.z   = -1.0f;

    ( *pVB )->Unlock();

    *numIndices = 4;

    // Create an index buffer and copy in the mesh index data.
    m_pd3dDevice->CreateIndexBuffer( *numIndices * sizeof( WORD ),
                                          0, D3DFMT_INDEX16, D3DPOOL_DEFAULT,
                                          pIB, NULL );
    WORD* pIBData = NULL;
    ( *pIB )->Lock( 0, 0, ( VOID** )&pIBData, 0 );
    *pIBData++ = 0;
    *pIBData++ = 2;
    *pIBData++ = 3;
    *pIBData++ = 1;
    ( *pIB )->Unlock();
}


//--------------------------------------------------------------------------------------
// Name: GenerateTexturedSphereGeometry()
// Desc: Creates geometry for a sphere
//--------------------------------------------------------------------------------------
VOID Sample::GenerateTexturedSphereGeometry( UINT numSlices, UINT numStacks,
                                     TestGeometryVertex* pData, FLOAT fUVScaler )
{
    for( DWORD i = 0; i < numSlices + 1; i++ )
    {
        for( DWORD j = 0; j < numStacks + 1; j++ )
        {
            FLOAT fTheta = FLOAT( i ) / numSlices * 2 * XM_PI;
            FLOAT fPhi = ( FLOAT( j ) / numStacks * 2 - 1.0f ) * XM_PIDIV2;
            pData->TexCoord.x = FLOAT( i ) / numSlices * fUVScaler;
            pData->TexCoord.y = FLOAT( j ) / numStacks * fUVScaler;
            pData->Position.x = cosf( fTheta ) * cosf( fPhi );
            pData->Position.z = sinf( fTheta ) * cosf( fPhi );
            pData->Position.y = sinf( fPhi );
            pData->Normal = pData->Position;

            pData++;
        }
    }
}


//--------------------------------------------------------------------------------------
// Name: GenerateSphereIndices()
// Desc: Creates indices for a sphere
//--------------------------------------------------------------------------------------
VOID Sample::GenerateSphereIndices( UINT dwNumSlices, UINT dwNumStacks, WORD* pIndices )
{
    WORD i0 = ( WORD )0;
    WORD i1 = ( WORD )1;
    WORD i2 = ( WORD )dwNumStacks + 2;
    WORD i3 = ( WORD )dwNumStacks + 1;

    for( DWORD i = 0; i < dwNumSlices; i++ )
    {
        for( DWORD j = 0; j < dwNumStacks; j++ )
        {
            pIndices[0] = i0;
            pIndices[1] = i1;
            pIndices[2] = i2;
            pIndices[3] = i3;

            pIndices += 4;
            i0++;
            i1++;
            i2++;
            i3++;
        }
        i0++;
        i1++;
        i2++;
        i3++;
    }
}


//--------------------------------------------------------------------------------------
// Name: GenerateTexturedSphere()
// Desc: Creates a sphere with texture coordinates
//--------------------------------------------------------------------------------------
VOID Sample::GenerateTexturedSphere( UINT numSlices, UINT numStacks, D3DVertexBuffer** pVB,
                             D3DIndexBuffer** pIB, UINT* numIndices, FLOAT fUVScaler )
{
    // Create a vertex buffer and copy the mesh vertex data into it
    m_pd3dDevice->BlockUntilIdle();
    m_pd3dDevice->CreateVertexBuffer(
        sizeof( TestGeometryVertex ) * ( numSlices + 1 ) * ( numStacks + 1 ),
        0, 0, D3DPOOL_DEFAULT, pVB, NULL );
    TestGeometryVertex* pVBData = NULL;
    ( *pVB )->Lock( 0, 0, ( VOID** )&pVBData, 0 );
    GenerateTexturedSphereGeometry( numSlices, numStacks, pVBData, fUVScaler );
    ( *pVB )->Unlock();

    *numIndices = 4 * numSlices * numStacks;

    // Create an index buffer and copy in the mesh index data.
    m_pd3dDevice->CreateIndexBuffer( *numIndices * sizeof( WORD ),
                                          0, D3DFMT_INDEX16, D3DPOOL_DEFAULT,
                                          pIB, NULL );
    WORD* pIBData = NULL;
    ( *pIB )->Lock( 0, 0, ( VOID** )&pIBData, 0 );
    GenerateSphereIndices( numSlices, numStacks, pIBData );
    ( *pIB )->Unlock();
}


//--------------------------------------------------------------------------------------
// Name: ComputeRawColorGradientData()
// Desc: Generate a sample gradient-like texture which tends to exhibit banding
//--------------------------------------------------------------------------------------
VOID Sample::ComputeRawColorGradientData()
{
    AllFormatColorBuffer& ColorBuffer = m_ColorBuffer[TEST_TEXTURE_COLOR_GRADIENT];
    const UINT& iWidth = ColorBuffer.m_iTextureWidth = 256;
    const UINT& iHeight = ColorBuffer.m_iTextureHeight = 256;

     // Fill in sample texture data
    ColorBuffer.m_pRawData.m_pVoid = new COLOR_32_32_32_32F[iHeight * iWidth];

   // Generate a pattern which will be prone to banding and precision loss
    for( UINT y = 0; y < iHeight; ++y )
    {
        for( UINT x = 0; x < iWidth; ++x )
        {
            COLOR_32_32_32_32F& RawColor = ColorBuffer.m_pRawData.m_pCOLOR_32_32_32_32F[ y * iWidth + x ];

            RawColor.a = 1.0f;
            RawColor.b = sqrtf( Squared(  1.0f * ( x / 255.0f ) ) + Squared( 0.25f * ( y / 255.0f ) ) );
            RawColor.g = sqrtf( Squared( 0.25f * ( x / 255.0f ) ) + Squared(  1.0f * ( y / 255.0f ) ) );
            RawColor.r = sqrtf( Squared(  0.5f * ( x / 255.0f ) ) + Squared(  0.5f * ( y / 255.0f ) ) );
        }
    }
}


//--------------------------------------------------------------------------------------
// Name: ComputeRawGrayscaleGradientData()
// Desc: Generate a sample gradient for which we will compare the results of 
//       gamma correction and ditherin
//--------------------------------------------------------------------------------------
VOID Sample::ComputeRawGrayscaleGradientData()
{
    AllFormatColorBuffer& ColorBuffer = m_ColorBuffer[TEST_TEXTURE_GRAYSCALE_GRADIENT];
    const UINT& iWidth = ColorBuffer.m_iTextureWidth = 256;
    const UINT& iHeight = ColorBuffer.m_iTextureHeight = 256;

     // Fill in sample texture data
    ColorBuffer.m_pRawData.m_pVoid = new COLOR_32_32_32_32F[iHeight * iWidth];

   // Generate a pattern which covers every grayscale color over a block of at least 256 texels
    for( UINT y = 0; y < iHeight; ++y )
    {
        for( UINT x = 0; x < iWidth; ++x )
        {
            COLOR_32_32_32_32F& RawColor = ColorBuffer.m_pRawData.m_pCOLOR_32_32_32_32F[ y * iWidth + x ];

            RawColor.a = 
            RawColor.b = 
            RawColor.g = 
            RawColor.r = ( x / 8 + ( y / 32 ) * 32 ) / 255.0f;
        }
    }
}


//--------------------------------------------------------------------------------------
// Name: ComputeRawSolidData()
// Desc: Fills a buffer with solid white
//--------------------------------------------------------------------------------------
VOID Sample::ComputeRawSolidData()
{
    AllFormatColorBuffer& ColorBuffer = m_ColorBuffer[TEST_TEXTURE_SOLID];
    const UINT& iWidth = ColorBuffer.m_iTextureWidth = 256;
    const UINT& iHeight = ColorBuffer.m_iTextureHeight = 256;

     // Fill in sample texture data
    ColorBuffer.m_pRawData.m_pVoid = new COLOR_32_32_32_32F[iHeight * iWidth];

   // Generate a pattern which covers every grayscale color over a block of at least 256 texels
    for( UINT y = 0; y < iHeight; ++y )
    {
        for( UINT x = 0; x < iWidth; ++x )
        {
            COLOR_32_32_32_32F& RawColor = ColorBuffer.m_pRawData.m_pCOLOR_32_32_32_32F[ y * iWidth + x ];

            RawColor.a = 
            RawColor.b = 
            RawColor.g = 
            RawColor.r = 1.0f;
        }
    }
}


//--------------------------------------------------------------------------------------
// Name: ComputeCompressedData()
// Desc: Generate a compressed texture from the raw data.  During this calculation,
//       the ambient texture color is multiplied in, and we perform a linear-to-gamma
//       transformation.
//--------------------------------------------------------------------------------------
VOID Sample::ComputeCompressedData( UINT iTestTexture, UINT iTextureConversionType )
{
    AllFormatColorBuffer& ColorBuffer = m_ColorBuffer[iTestTexture];
    const UINT& iWidth = ColorBuffer.m_iTextureWidth;
    const UINT& iHeight = ColorBuffer.m_iTextureHeight;

    // Copy sample texture data with gamma-conversion to texture
    assert( iTextureConversionType >= 0 && iTextureConversionType < TEXTURE_CONVERSION_COUNT );
    GenericRawColorBuffer& RawTextureData = ColorBuffer.m_pRawData;
    GenericCompressedColorBuffer& CompressedTextureData = ColorBuffer.m_pCompressedData[iTextureConversionType];
    const D3DXVECTOR3& vTextureAmbient = CompressedTextureData.m_vAmbient = m_TextureAmbientParam.GetValue();
    switch ( iTextureConversionType )
    {
    case TEXTURE_CONVERSION_8_BIT_LINEAR:
        for( UINT j = 0; j < iHeight * iWidth; ++j )
        {
            COLOR_32_32_32_32F ColorUncompressed = RawTextureData.m_pCOLOR_32_32_32_32F[j];
            COLOR_8_8_8_8& ColorCompressed = CompressedTextureData.m_pCOLOR_8_8_8_8[j];

            ColorCompressed.a = ConvertFloatTo8BitLinear( ColorUncompressed.a ); 
            ColorCompressed.b = ConvertFloatTo8BitLinear( ColorUncompressed.b * vTextureAmbient.x ); 
            ColorCompressed.g = ConvertFloatTo8BitLinear( ColorUncompressed.g * vTextureAmbient.y ); 
            ColorCompressed.r = ConvertFloatTo8BitLinear( ColorUncompressed.r * vTextureAmbient.z ); 
        }
        break;

    case TEXTURE_CONVERSION_8_BIT_GAMMA_PWL:
        for( UINT j = 0; j < iHeight * iWidth; ++j )
        {
            COLOR_32_32_32_32F ColorUncompressed = RawTextureData.m_pCOLOR_32_32_32_32F[j];
            COLOR_8_8_8_8& ColorCompressed = CompressedTextureData.m_pCOLOR_8_8_8_8[j];

            ColorCompressed.a = ConvertFloatTo8BitLinear( ColorUncompressed.a ); 
            ColorCompressed.b = ConvertFloatTo8BitGammaPWL( ColorUncompressed.b * vTextureAmbient.x ); 
            ColorCompressed.g = ConvertFloatTo8BitGammaPWL( ColorUncompressed.g * vTextureAmbient.y ); 
            ColorCompressed.r = ConvertFloatTo8BitGammaPWL( ColorUncompressed.r * vTextureAmbient.z ); 
        }
        break;

    case TEXTURE_CONVERSION_8_BIT_GAMMA_2_0:
        for( UINT j = 0; j < iHeight * iWidth; ++j )
        {
            COLOR_32_32_32_32F ColorUncompressed = RawTextureData.m_pCOLOR_32_32_32_32F[j];
            COLOR_8_8_8_8& ColorCompressed = CompressedTextureData.m_pCOLOR_8_8_8_8[j];

            ColorCompressed.a = ConvertFloatTo8BitLinear( ColorUncompressed.a ); 
            ColorCompressed.b = ConvertFloatTo8BitGamma_2_0( ColorUncompressed.b * vTextureAmbient.x ); 
            ColorCompressed.g = ConvertFloatTo8BitGamma_2_0( ColorUncompressed.g * vTextureAmbient.y ); 
            ColorCompressed.r = ConvertFloatTo8BitGamma_2_0( ColorUncompressed.r * vTextureAmbient.z ); 
        }
        break;

    case TEXTURE_CONVERSION_8_BIT_GAMMA_2_2:
        for( UINT j = 0; j < iHeight * iWidth; ++j )
        {
            COLOR_32_32_32_32F ColorUncompressed = RawTextureData.m_pCOLOR_32_32_32_32F[j];
            COLOR_8_8_8_8& ColorCompressed = CompressedTextureData.m_pCOLOR_8_8_8_8[j];

            ColorCompressed.a = ConvertFloatTo8BitLinear( ColorUncompressed.a ); 
            ColorCompressed.b = ConvertFloatTo8BitGamma_2_2( ColorUncompressed.b * vTextureAmbient.x ); 
            ColorCompressed.g = ConvertFloatTo8BitGamma_2_2( ColorUncompressed.g * vTextureAmbient.y ); 
            ColorCompressed.r = ConvertFloatTo8BitGamma_2_2( ColorUncompressed.r * vTextureAmbient.z ); 
        }
        break;

    case TEXTURE_CONVERSION_10_BIT_LINEAR:
        for( UINT j = 0; j < iHeight * iWidth; ++j )
        {
            COLOR_32_32_32_32F ColorUncompressed = RawTextureData.m_pCOLOR_32_32_32_32F[j];
            COLOR_2_10_10_10& ColorCompressed = CompressedTextureData.m_pCOLOR_2_10_10_10[j];

            ColorCompressed.a = ConvertFloatTo2BitLinear( ColorUncompressed.a ); 
            ColorCompressed.b = ConvertFloatTo10BitLinear( ColorUncompressed.b * vTextureAmbient.x ); 
            ColorCompressed.g = ConvertFloatTo10BitLinear( ColorUncompressed.g * vTextureAmbient.y ); 
            ColorCompressed.r = ConvertFloatTo10BitLinear( ColorUncompressed.r * vTextureAmbient.z ); 
        }
        break;

    case TEXTURE_CONVERSION_10_BIT_GAMMA_PWL:
        for( UINT j = 0; j < iHeight * iWidth; ++j )
        {
            COLOR_32_32_32_32F ColorUncompressed = RawTextureData.m_pCOLOR_32_32_32_32F[j];
            COLOR_2_10_10_10& ColorCompressed = CompressedTextureData.m_pCOLOR_2_10_10_10[j];

            ColorCompressed.a = ConvertFloatTo2BitLinear( ColorUncompressed.a ); 
            ColorCompressed.b = ConvertFloatTo10BitGammaPWL( ColorUncompressed.b * vTextureAmbient.x ); 
            ColorCompressed.g = ConvertFloatTo10BitGammaPWL( ColorUncompressed.g * vTextureAmbient.y ); 
            ColorCompressed.r = ConvertFloatTo10BitGammaPWL( ColorUncompressed.r * vTextureAmbient.z ); 
        }
        break;

    case TEXTURE_CONVERSION_10_BIT_GAMMA_2_0:
        for( UINT j = 0; j < iHeight * iWidth; ++j )
        {
            COLOR_32_32_32_32F ColorUncompressed = RawTextureData.m_pCOLOR_32_32_32_32F[j];
            COLOR_2_10_10_10& ColorCompressed = CompressedTextureData.m_pCOLOR_2_10_10_10[j];

            ColorCompressed.a = ConvertFloatTo2BitLinear( ColorUncompressed.a ); 
            ColorCompressed.b = ConvertFloatTo10BitGamma_2_0( ColorUncompressed.b * vTextureAmbient.x ); 
            ColorCompressed.g = ConvertFloatTo10BitGamma_2_0( ColorUncompressed.g * vTextureAmbient.y ); 
            ColorCompressed.r = ConvertFloatTo10BitGamma_2_0( ColorUncompressed.r * vTextureAmbient.z ); 
        }
        break;

    case TEXTURE_CONVERSION_10_BIT_GAMMA_2_2:
        for( UINT j = 0; j < iHeight * iWidth; ++j )
        {
            COLOR_32_32_32_32F ColorUncompressed = RawTextureData.m_pCOLOR_32_32_32_32F[j];
            COLOR_2_10_10_10& ColorCompressed = CompressedTextureData.m_pCOLOR_2_10_10_10[j];

            ColorCompressed.a = ConvertFloatTo2BitLinear( ColorUncompressed.a ); 
            ColorCompressed.b = ConvertFloatTo10BitGamma_2_2( ColorUncompressed.b * vTextureAmbient.x ); 
            ColorCompressed.g = ConvertFloatTo10BitGamma_2_2( ColorUncompressed.g * vTextureAmbient.y ); 
            ColorCompressed.r = ConvertFloatTo10BitGamma_2_2( ColorUncompressed.r * vTextureAmbient.z ); 
        }
        break;

    case TEXTURE_CONVERSION_16_BIT_LINEAR:
        for( UINT j = 0; j < iHeight * iWidth; ++j )
        {
            COLOR_32_32_32_32F ColorUncompressed = RawTextureData.m_pCOLOR_32_32_32_32F[j];
            COLOR_16_16_16_16& ColorCompressed = CompressedTextureData.m_pCOLOR_16_16_16_16[j];

            ColorCompressed.a = ConvertFloatTo16BitLinear( ColorUncompressed.a ); 
            ColorCompressed.b = ConvertFloatTo16BitLinear( ColorUncompressed.b * vTextureAmbient.x ); 
            ColorCompressed.g = ConvertFloatTo16BitLinear( ColorUncompressed.g * vTextureAmbient.y ); 
            ColorCompressed.r = ConvertFloatTo16BitLinear( ColorUncompressed.r * vTextureAmbient.z ); 
        }
        break;
    }

    XGTileSurface( CompressedTextureData.m_pVoid, 
        iWidth, 
        iHeight, 
        NULL, 
        CompressedTextureData.m_pVoid, 
        iWidth * g_iTexelSizesInBytesForConversion[iTextureConversionType], 
        NULL, 
        g_iTexelSizesInBytesForConversion[iTextureConversionType] );
}


VOID Sample::CreateGammaRamp( UINT iGammaType )
{
    switch ( iGammaType )
    {
    case DISPLAY_GAMMA_PWL:
        {
            D3DGAMMARAMP& GammaRamp = g_GammaRamp[iGammaType].m_8BitGamma;
            for( UINT j = 0; j < 256; ++j )
            {
                GammaRamp.red[j]    = 
                    GammaRamp.green[j]  = 
                    GammaRamp.blue[j]   = Convert8BitGammaPWLTo10BitGammasRGB( (BYTE) j ) << 6;
            }

            D3DPWLGAMMA& PWLGamma = g_GammaRamp[iGammaType].m_PWLGamma;
            for( UINT j = 0; j < 128; ++j )
            {
                PWLGamma.red[j].Base    = 
                    PWLGamma.green[j].Base  = 
                    PWLGamma.blue[j].Base   = Convert10BitGammaPWLTo16BitGammasRGB( (WORD) ( j << 3 ) );
            }
            for( UINT j = 0; j < 127; ++j )
            {
                PWLGamma.red[j].Delta    = PWLGamma.red[j+1].Base - PWLGamma.red[j].Base;
                PWLGamma.green[j].Delta  = PWLGamma.green[j+1].Base - PWLGamma.green[j].Base;
                PWLGamma.blue[j].Delta   = PWLGamma.blue[j+1].Base - PWLGamma.blue[j].Base;
            }
            PWLGamma.red[127].Delta    = 0xffff - PWLGamma.red[127].Base;
            PWLGamma.green[127].Delta  = 0xffff - PWLGamma.green[127].Base;
            PWLGamma.blue[127].Delta   = 0xffff - PWLGamma.blue[127].Base;
        }
        break;

    case DISPLAY_GAMMA_2_0:
        {
            D3DGAMMARAMP& GammaRamp = g_GammaRamp[iGammaType].m_8BitGamma;
            for( UINT j = 0; j < 256; ++j )
            {
                GammaRamp.red[j]    = 
                    GammaRamp.green[j]  = 
                    GammaRamp.blue[j]   = Convert8BitGamma_2_0To10BitGammasRGB( (BYTE) j ) << 6;
            }

            D3DPWLGAMMA& PWLGamma = g_GammaRamp[iGammaType].m_PWLGamma;
            for( UINT j = 0; j < 128; ++j )
            {
                PWLGamma.red[j].Base    = 
                    PWLGamma.green[j].Base  = 
                    PWLGamma.blue[j].Base   = Convert10BitGamma_2_0To16BitGammasRGB( (WORD) ( j << 3 ) );
            }
            for( UINT j = 0; j < 127; ++j )
            {
                PWLGamma.red[j].Delta    = PWLGamma.red[j+1].Base - PWLGamma.red[j].Base;
                PWLGamma.green[j].Delta  = PWLGamma.green[j+1].Base - PWLGamma.green[j].Base;
                PWLGamma.blue[j].Delta   = PWLGamma.blue[j+1].Base - PWLGamma.blue[j].Base;
            }
            PWLGamma.red[127].Delta    = 0xffff - PWLGamma.red[127].Base;
            PWLGamma.green[127].Delta  = 0xffff - PWLGamma.green[127].Base;
            PWLGamma.blue[127].Delta   = 0xffff - PWLGamma.blue[127].Base;
        }
        break;

    case DISPLAY_GAMMA_2_2:
        {
            D3DGAMMARAMP& GammaRamp = g_GammaRamp[iGammaType].m_8BitGamma;
            for( UINT j = 0; j < 256; ++j )
            {
                GammaRamp.red[j]    = 
                    GammaRamp.green[j]  = 
                    GammaRamp.blue[j]   = Convert8BitGamma_2_2To10BitGammasRGB( (BYTE) j ) << 6;
            }

            D3DPWLGAMMA& PWLGamma = g_GammaRamp[iGammaType].m_PWLGamma;
            for( UINT j = 0; j < 128; ++j )
            {
                PWLGamma.red[j].Base    = 
                    PWLGamma.green[j].Base  = 
                    PWLGamma.blue[j].Base   = Convert10BitGamma_2_2To16BitGammasRGB( (WORD) ( j << 3 ) );
            }
            for( UINT j = 0; j < 127; ++j )
            {
                PWLGamma.red[j].Delta    = PWLGamma.red[j+1].Base - PWLGamma.red[j].Base;
                PWLGamma.green[j].Delta  = PWLGamma.green[j+1].Base - PWLGamma.green[j].Base;
                PWLGamma.blue[j].Delta   = PWLGamma.blue[j+1].Base - PWLGamma.blue[j].Base;
            }
            PWLGamma.red[127].Delta    = 0xffff - PWLGamma.red[127].Base;
            PWLGamma.green[127].Delta  = 0xffff - PWLGamma.green[127].Base;
            PWLGamma.blue[127].Delta   = 0xffff - PWLGamma.blue[127].Base;
        }
        break;

    case DISPLAY_GAMMA_LINEAR:
        {
            D3DGAMMARAMP& GammaRamp = g_GammaRamp[iGammaType].m_8BitGamma;
            for( UINT j = 0; j < 256; ++j )
            {
                GammaRamp.red[j]    = 
                    GammaRamp.green[j]  = 
                    GammaRamp.blue[j]   = Convert8BitLinearTo10BitGammasRGB( (BYTE) j ) << 6;
            }

            D3DPWLGAMMA& PWLGamma = g_GammaRamp[iGammaType].m_PWLGamma;
            for( UINT j = 0; j < 128; ++j )
            {
                PWLGamma.red[j].Base    = 
                    PWLGamma.green[j].Base  = 
                    PWLGamma.blue[j].Base   = Convert10BitLinearTo16BitGammasRGB( (WORD) ( j << 3 ) );
            }
            for( UINT j = 0; j < 127; ++j )
            {
                PWLGamma.red[j].Delta    = PWLGamma.red[j+1].Base - PWLGamma.red[j].Base;
                PWLGamma.green[j].Delta  = PWLGamma.green[j+1].Base - PWLGamma.green[j].Base;
                PWLGamma.blue[j].Delta   = PWLGamma.blue[j+1].Base - PWLGamma.blue[j].Base;
            }
            PWLGamma.red[127].Delta    = 0xffff - PWLGamma.red[127].Base;
            PWLGamma.green[127].Delta  = 0xffff - PWLGamma.green[127].Base;
            PWLGamma.blue[127].Delta   = 0xffff - PWLGamma.blue[127].Base;
        }
        break;

    case DISPLAY_GAMMA_SRGB:
        {
            D3DGAMMARAMP& GammaRamp = g_GammaRamp[iGammaType].m_8BitGamma;
            for( UINT j = 0; j < 256; ++j )
            {
                GammaRamp.red[j]    = 
                    GammaRamp.green[j]  = 
                    GammaRamp.blue[j]   = (WORD) j << 8;
            }

            D3DPWLGAMMA& PWLGamma = g_GammaRamp[iGammaType].m_PWLGamma;
            for( UINT j = 0; j < 128; ++j )
            {
                PWLGamma.red[j].Base    = 
                    PWLGamma.green[j].Base  = 
                    PWLGamma.blue[j].Base   = (WORD) ( j << 9 );
            }
            for( UINT j = 0; j < 127; ++j )
            {
                PWLGamma.red[j].Delta    = PWLGamma.red[j+1].Base - PWLGamma.red[j].Base;
                PWLGamma.green[j].Delta  = PWLGamma.green[j+1].Base - PWLGamma.green[j].Base;
                PWLGamma.blue[j].Delta   = PWLGamma.blue[j+1].Base - PWLGamma.blue[j].Base;
            }
            PWLGamma.red[127].Delta    = 0xffff - PWLGamma.red[127].Base;
            PWLGamma.green[127].Delta  = 0xffff - PWLGamma.green[127].Base;
            PWLGamma.blue[127].Delta   = 0xffff - PWLGamma.blue[127].Base;
        }
        break;

    case DISPLAY_GAMMA_PWL_OVERRIDE:
        {
            D3DGAMMARAMP& GammaRamp = g_GammaRamp[iGammaType].m_8BitGamma;
            for( UINT j = 0; j < 256; ++j )
            {
                GammaRamp.red[j]    = 
                    GammaRamp.green[j]  = 
                    GammaRamp.blue[j]   = Convert8BitGammaPWLTo10BitGammasRGBOverride( (BYTE) j ) << 6;
            }

            D3DPWLGAMMA& PWLGamma = g_GammaRamp[iGammaType].m_PWLGamma;
            for( UINT j = 0; j < 128; ++j )
            {
                PWLGamma.red[j].Base    = 
                    PWLGamma.green[j].Base  = 
                    PWLGamma.blue[j].Base   = Convert10BitGammaPWLTo16BitGammasRGBOverride( (WORD) ( j << 3 ) );
            }
            for( UINT j = 0; j < 127; ++j )
            {
                PWLGamma.red[j].Delta    = PWLGamma.red[j+1].Base - PWLGamma.red[j].Base;
                PWLGamma.green[j].Delta  = PWLGamma.green[j+1].Base - PWLGamma.green[j].Base;
                PWLGamma.blue[j].Delta   = PWLGamma.blue[j+1].Base - PWLGamma.blue[j].Base;
            }
            PWLGamma.red[127].Delta    = 0xffff - PWLGamma.red[127].Base;
            PWLGamma.green[127].Delta  = 0xffff - PWLGamma.green[127].Base;
            PWLGamma.blue[127].Delta   = 0xffff - PWLGamma.blue[127].Base;
        }
        break;

    case DISPLAY_GAMMA_2_0_OVERRIDE:
        {
            D3DGAMMARAMP& GammaRamp = g_GammaRamp[iGammaType].m_8BitGamma;
            for( UINT j = 0; j < 256; ++j )
            {
                GammaRamp.red[j]    = 
                    GammaRamp.green[j]  = 
                    GammaRamp.blue[j]   = Convert8BitGamma_2_0To10BitGammasRGBOverride( (BYTE) j ) << 6;
            }

            D3DPWLGAMMA& PWLGamma = g_GammaRamp[iGammaType].m_PWLGamma;
            for( UINT j = 0; j < 128; ++j )
            {
                PWLGamma.red[j].Base    = 
                    PWLGamma.green[j].Base  = 
                    PWLGamma.blue[j].Base   = Convert10BitGamma_2_0To16BitGammasRGBOverride( (WORD) ( j << 3 ) );
            }
            for( UINT j = 0; j < 127; ++j )
            {
                PWLGamma.red[j].Delta    = PWLGamma.red[j+1].Base - PWLGamma.red[j].Base;
                PWLGamma.green[j].Delta  = PWLGamma.green[j+1].Base - PWLGamma.green[j].Base;
                PWLGamma.blue[j].Delta   = PWLGamma.blue[j+1].Base - PWLGamma.blue[j].Base;
            }
            PWLGamma.red[127].Delta    = 0xffff - PWLGamma.red[127].Base;
            PWLGamma.green[127].Delta  = 0xffff - PWLGamma.green[127].Base;
            PWLGamma.blue[127].Delta   = 0xffff - PWLGamma.blue[127].Base;
        }
        break;

    case DISPLAY_GAMMA_2_2_OVERRIDE:
        {
            D3DGAMMARAMP& GammaRamp = g_GammaRamp[iGammaType].m_8BitGamma;
            for( UINT j = 0; j < 256; ++j )
            {
                GammaRamp.red[j]    = 
                    GammaRamp.green[j]  = 
                    GammaRamp.blue[j]   = Convert8BitGamma_2_2To10BitGammasRGBOverride( (BYTE) j ) << 6;
            }

            D3DPWLGAMMA& PWLGamma = g_GammaRamp[iGammaType].m_PWLGamma;
            for( UINT j = 0; j < 128; ++j )
            {
                PWLGamma.red[j].Base    = 
                    PWLGamma.green[j].Base  = 
                    PWLGamma.blue[j].Base   = Convert10BitGamma_2_2To16BitGammasRGBOverride( (WORD) ( j << 3 ) );
            }
            for( UINT j = 0; j < 127; ++j )
            {
                PWLGamma.red[j].Delta    = PWLGamma.red[j+1].Base - PWLGamma.red[j].Base;
                PWLGamma.green[j].Delta  = PWLGamma.green[j+1].Base - PWLGamma.green[j].Base;
                PWLGamma.blue[j].Delta   = PWLGamma.blue[j+1].Base - PWLGamma.blue[j].Base;
            }
            PWLGamma.red[127].Delta    = 0xffff - PWLGamma.red[127].Base;
            PWLGamma.green[127].Delta  = 0xffff - PWLGamma.green[127].Base;
            PWLGamma.blue[127].Delta   = 0xffff - PWLGamma.blue[127].Base;
        }
        break;

    case DISPLAY_GAMMA_LINEAR_OVERRIDE:
        {
            D3DGAMMARAMP& GammaRamp = g_GammaRamp[iGammaType].m_8BitGamma;
            for( UINT j = 0; j < 256; ++j )
            {
                GammaRamp.red[j]    = 
                    GammaRamp.green[j]  = 
                    GammaRamp.blue[j]   = Convert8BitLinearTo10BitGammasRGBOverride( (BYTE) j ) << 6;
            }

            D3DPWLGAMMA& PWLGamma = g_GammaRamp[iGammaType].m_PWLGamma;
            for( UINT j = 0; j < 128; ++j )
            {
                PWLGamma.red[j].Base    = 
                    PWLGamma.green[j].Base  = 
                    PWLGamma.blue[j].Base   = Convert10BitLinearTo16BitGammasRGBOverride( (WORD) ( j << 3 ) );
            }
            for( UINT j = 0; j < 127; ++j )
            {
                PWLGamma.red[j].Delta    = PWLGamma.red[j+1].Base - PWLGamma.red[j].Base;
                PWLGamma.green[j].Delta  = PWLGamma.green[j+1].Base - PWLGamma.green[j].Base;
                PWLGamma.blue[j].Delta   = PWLGamma.blue[j+1].Base - PWLGamma.blue[j].Base;
            }
            PWLGamma.red[127].Delta    = 0xffff - PWLGamma.red[127].Base;
            PWLGamma.green[127].Delta  = 0xffff - PWLGamma.green[127].Base;
            PWLGamma.blue[127].Delta   = 0xffff - PWLGamma.blue[127].Base;
        }
        break;

    case DISPLAY_GAMMA_SRGB_OVERRIDE:
        {
            D3DGAMMARAMP& GammaRamp = g_GammaRamp[iGammaType].m_8BitGamma;
            for( UINT j = 0; j < 256; ++j )
            {
                GammaRamp.red[j]    = 
                    GammaRamp.green[j]  = 
                    GammaRamp.blue[j]   = Convert8BitGammasRGBTo10BitGammasRGBOverride( (BYTE) j ) << 6;
            }

            D3DPWLGAMMA& PWLGamma = g_GammaRamp[iGammaType].m_PWLGamma;
            for( UINT j = 0; j < 128; ++j )
            {
                PWLGamma.red[j].Base    = 
                    PWLGamma.green[j].Base  = 
                    PWLGamma.blue[j].Base   = Convert10BitGammasRGBTo16BitGammasRGBOverride( (WORD) ( j << 3 ) );
            }
            for( UINT j = 0; j < 127; ++j )
            {
                PWLGamma.red[j].Delta    = PWLGamma.red[j+1].Base - PWLGamma.red[j].Base;
                PWLGamma.green[j].Delta  = PWLGamma.green[j+1].Base - PWLGamma.green[j].Base;
                PWLGamma.blue[j].Delta   = PWLGamma.blue[j+1].Base - PWLGamma.blue[j].Base;
            }
            PWLGamma.red[127].Delta    = 0xffff - PWLGamma.red[127].Base;
            PWLGamma.green[127].Delta  = 0xffff - PWLGamma.green[127].Base;
            PWLGamma.blue[127].Delta   = 0xffff - PWLGamma.blue[127].Base;
        }
        break;
    }
}


//-----------------------------------------------------------------------------
// Name: Initialize()
// Desc: Initialize app-dependent objects.
//-----------------------------------------------------------------------------
HRESULT Sample::Initialize()
{
    // Create the font
    if( FAILED( m_Font.Create( "game:\\Media\\Fonts\\Arial_16.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Confine text drawing to the title safe area
    m_Font.SetWindow( ATG::GetTitleSafeArea() );

    // Create the help
    if( FAILED( m_Help.Create( "game:\\Media\\Help\\Help.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    m_bDrawHelp = FALSE;
    m_bDrawUI = TRUE;

    m_iActiveUIParameter = 0;
    
    // Create common vertex declaration used by all the screen-space effects
    static const D3DVERTEXELEMENT9 decl[] =
    {
        { 0, 0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
        { 0, 12, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
        { 0, 20, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_NORMAL, 0 },
        D3DDECL_END()
    };

    m_pd3dDevice->CreateVertexDeclaration( decl, &m_pVtxDecl );

    GenerateFullscreenQuad( &m_pVB[TEST_GEOMETRY_FULLSCREEN_QUAD],
        &m_pIB[TEST_GEOMETRY_FULLSCREEN_QUAD], &m_iIndexCount[TEST_GEOMETRY_FULLSCREEN_QUAD] );
    GenerateTexturedSphere( 50, 50, &m_pVB[TEST_GEOMETRY_SPHERE],
        &m_pIB[TEST_GEOMETRY_SPHERE], &m_iIndexCount[TEST_GEOMETRY_SPHERE], 1.0f );

    m_matWorldToView[TEST_GEOMETRY_FULLSCREEN_QUAD] = XMMatrixIdentity();
    m_matWorldToScreen[TEST_GEOMETRY_FULLSCREEN_QUAD] = XMMatrixIdentity();

    XMVECTOR vEyePt =       {  0.0f,  0.0f, -3.0f };
    XMVECTOR vLookatDir =   {  0.0f,  0.0f,  1.0f };
    XMVECTOR vUpVec =       {  0.0f,  1.0f,  0.0f };
    m_matWorldToView[TEST_GEOMETRY_SPHERE] = XMMatrixLookAtLH( vEyePt, vEyePt + vLookatDir, vUpVec );

    FLOAT fAspectRatio = (FLOAT)m_d3dpp.BackBufferWidth / (FLOAT)m_d3dpp.BackBufferHeight;
    m_matWorldToScreen[TEST_GEOMETRY_SPHERE] = m_matWorldToView[TEST_GEOMETRY_SPHERE] 
        * XMMatrixPerspectiveFovLH( D3DX_PI/4, fAspectRatio, 0.02f, 5.0f );

    // Create shaders
    if( FAILED( ATG::LoadVertexShader( "game:\\Media\\Shaders\\DefaultVS.xvu",
                                       &m_pVertexShader ) ) )
        return ATGAPPERR_MEDIANOTFOUND;
    if( FAILED( ATG::LoadPixelShader( "game:\\Media\\Shaders\\GammaGenericPS.xpu",
                                       &m_pGammaGenericPS ) ) )
        return ATGAPPERR_MEDIANOTFOUND;
    if( FAILED( ATG::LoadPixelShader( "game:\\Media\\Shaders\\DitherGenericPS.xpu",
                                       &m_pDitherPS ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Create raw texture data
    ComputeRawColorGradientData();
    ComputeRawGrayscaleGradientData();
    ComputeRawSolidData();

    for( UINT j = 0; j < TEST_TEXTURE_COUNT; ++j )
    {
        // Allocate compressed texture memory
        AllFormatColorBuffer& ColorBuffer = m_ColorBuffer[j];
        const UINT& iWidth = ColorBuffer.m_iTextureWidth;
        const UINT& iHeight = ColorBuffer.m_iTextureHeight;
        for( UINT i = 0; i < TEXTURE_CONVERSION_COUNT; ++i )
        {
            GenericCompressedColorBuffer& CompressedData = ColorBuffer.m_pCompressedData[i];

            UINT iSize = iWidth * iHeight * g_iTexelSizesInBytesForConversion[i];
            CompressedData.m_pVoid = XPhysicalAlloc( iSize, MAXULONG_PTR, 0, 
                PAGE_READWRITE | PAGE_WRITECOMBINE | MEM_LARGE_PAGES );
        }

        // Create sample textures
        for( UINT i = 0; i < SOURCE_TYPE_COUNT; ++i )
        {
            ColorBuffer.m_pTexture[i] = new D3DTexture;
            XGSetTextureHeader( iWidth, iHeight, 1, 0, g_d3dfmtTexture[i], D3DPOOL_DEFAULT, 
                0, 0, iWidth * g_iTexelSizesInBytesForSource[i], ColorBuffer.m_pTexture[i], 
                NULL, NULL );
        }
    }

    // Create sample render targets
    const DWORD dwRenderTargetWidth = m_d3dpp.BackBufferWidth, 
        dwRenderTargetHeight = m_d3dpp.BackBufferHeight;
    D3DSURFACE_PARAMETERS SurfaceParameters;
    ZeroMemory( &SurfaceParameters, sizeof( D3DSURFACE_PARAMETERS ) );
    for( UINT i = 0; i < DEST_TYPE_COUNT; ++i )
    {
        switch ( i )
        {
        case DEST_TYPE_8_BIT_LINEAR:
        case DEST_TYPE_8_BIT_GAMMA:
        case DEST_TYPE_10_BIT_LINEAR:
            SurfaceParameters.ColorExpBias = 0;
            break;

        case DEST_TYPE_10_BIT_FLOAT_LINEAR:
        case DEST_TYPE_16_BIT_LINEAR:
            SurfaceParameters.ColorExpBias = +5;
            break;
        }
        if( FAILED( m_pd3dDevice->CreateRenderTarget( dwRenderTargetWidth, dwRenderTargetHeight,
                                                 g_d3dfmtRenderTarget[i], D3DMULTISAMPLE_NONE, 
                                                 0, FALSE, &m_pRenderTarget[i], &SurfaceParameters ) ) )
        {
            return E_FAIL;
        }
    }

    // Create sample front buffers
    const DWORD dwFrontBufferWidth = dwRenderTargetWidth, dwFrontBufferHeight = dwRenderTargetHeight;
    UINT iFrontBufferSize = XGNextMultiple( dwFrontBufferWidth, GPU_TEXTURE_TILE_DIMENSION ) 
        * XGNextMultiple( dwFrontBufferHeight, GPU_TEXTURE_TILE_DIMENSION ) 
        * 4;
    void* m_pFrontBufferData = XPhysicalAlloc( iFrontBufferSize, MAXULONG_PTR, 0, 
        PAGE_READWRITE | PAGE_WRITECOMBINE | MEM_LARGE_PAGES );
    for( UINT i = 0; i < FRONT_BUFFER_COUNT; ++i )
    {
        m_pFrontBuffer[i] = new D3DTexture;
        XGSetTextureHeader( dwFrontBufferWidth, dwFrontBufferHeight, 1, 0, g_d3dfmtFrontBuffer[i], 
            D3DPOOL_DEFAULT, 0, 0, dwFrontBufferWidth * 4, m_pFrontBuffer[i], NULL, NULL );
        XGOffsetBaseTextureAddress( m_pFrontBuffer[i], m_pFrontBufferData, NULL );
    }

    // Create Gamma Ramps
    for( UINT i = 0; i < DISPLAY_GAMMA_COUNT; ++i )
    {
        CreateGammaRamp( i );
    }

    m_vLightDirection.x = 3.0f;
    m_vLightDirection.y = 2.0f;
    m_vLightDirection.z = 1.0f;
    D3DXVec3Normalize( &m_vLightDirection, &m_vLightDirection );

    // Set default render states
    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_ZWRITEENABLE, FALSE );

    return S_OK;
}


//-----------------------------------------------------------------------------
// Name: Update()
// Desc: Called once per frame, the call is the entry point for animating
//       the scene.
//-----------------------------------------------------------------------------
HRESULT Sample::Update()
{
    // Get the current gamepad status
    ATG::GAMEPAD* pGamepad = ATG::Input::GetMergedInput();

    // Toggle help
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_BACK )
        m_bDrawHelp = !m_bDrawHelp;

    // Toggle UI
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_X )
        m_bDrawUI = !m_bDrawUI;

    if( !m_bDrawHelp && m_bDrawUI )
    {
        // Record initial state in order to detect changes
        static BOOL bFirstUpdate = TRUE;
        BOOL bPresetTurnedOn = FALSE, bPresetTurnedOff = FALSE;
        UINT iOldFrontBufferType = m_FrontBufferParam.GetValue();
        UINT iOldDisplayGamma = m_DisplayGammaParam.GetValue();
        UINT iOldSourceType = m_SourceTypeParam.GetValue();

        static FLOAT fLastX1 = 0.0f, fLastY1 = 0.0f;
        FLOAT fDecrease = 0.0f;
        FLOAT fIncrease = 0.0f;

        // Process gamepad input
        if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_DPAD_UP || ( pGamepad->fY1 > 0.0f && fLastY1 <= 0.0f ) )
        {
            m_iActiveUIParameter += GAMMA_PARAM_COUNT - 1;
            m_iActiveUIParameter %= GAMMA_PARAM_COUNT;
        }
        if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_DPAD_DOWN || ( pGamepad->fY1 < 0.0f && fLastY1 >= 0.0f ) )
        {
            m_iActiveUIParameter += 1;
            m_iActiveUIParameter %= GAMMA_PARAM_COUNT;
        }

        if( pGamepad->bLeftTrigger > 0 )
            fDecrease = pGamepad->bLeftTrigger / 255.0f;
        if( pGamepad->bRightTrigger > 0 )
            fIncrease = pGamepad->bRightTrigger / 255.0f;
        if( pGamepad->fX1 < 0.0f && fLastX1 >= 0.0f )
            fDecrease = 1.0f;
        if( pGamepad->fX1 > 0.0f && fLastX1 <= 0.0f )
            fIncrease = 1.0f;
        if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_DPAD_LEFT )
            fDecrease = 1.0f;
        if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_DPAD_RIGHT )
            fIncrease = 1.0f;

        if( fDecrease > 0.0f )
        {
            m_ParamArray[m_iActiveUIParameter]->DecreaseValue( fDecrease );
        }
        if( fIncrease > 0.0f )
        {
            m_ParamArray[m_iActiveUIParameter]->IncreaseValue( fIncrease );
        }

        // Respond to changes
        if( fDecrease > 0.0f || fIncrease > 0.0f )
        {
            switch ( m_iActiveUIParameter )
            {
            case GAMMA_PARAM_PRESET:
                bPresetTurnedOn = TRUE;
                break;

            case GAMMA_PARAM_TEXTURE_CONVERSION: 
            case GAMMA_PARAM_SOURCE_TYPE: 
            case GAMMA_PARAM_SHADER_DEGAMMA: 
            case GAMMA_PARAM_SHADER_GAMMA:
            case GAMMA_PARAM_DEST_TYPE: 
            case GAMMA_PARAM_FRONT_BUFFER: 
            case GAMMA_PARAM_DISPLAY_GAMMA: 
                bPresetTurnedOff = TRUE;
                break;
            }
        }

        fLastX1 = pGamepad->fX1;
        fLastY1 = pGamepad->fY1;

        if( bFirstUpdate || bPresetTurnedOn )
        {
            const GammaPreset& Preset = g_GammaPresets[ m_PresetParam.GetValue() ];
            for( UINT i = 0; i < GAMMA_PRESET_COUNT; ++i )
            {
                m_PresetParamArray[i]->SetValue( Preset[i] );
            }
        }
        else if( bPresetTurnedOff )
        {
            m_PresetParam.SetValue( 0xffffffff );    // forces display of "n/a"
        }

        UINT iNewTestTexture = m_TestTextureParam.GetValue();
        UINT iNewTextureConversion = m_TextureConversionParam.GetValue();
        UINT iNewSourceType = m_SourceTypeParam.GetValue();
        UINT iNewFrontBufferType = m_FrontBufferParam.GetValue();
        UINT iNewDisplayGamma = m_DisplayGammaParam.GetValue();
        const D3DXVECTOR3& vNewTextureAmbient = m_TextureAmbientParam.GetValue();
        GenericCompressedColorBuffer NewCompressedData = m_ColorBuffer[ iNewTestTexture ].m_pCompressedData[ iNewTextureConversion ];
        D3DTexture* pNewTexture = m_ColorBuffer[ iNewTestTexture ].m_pTexture[ iNewSourceType ];

        pNewTexture->Format.BaseAddress = NULL;
        XGOffsetBaseTextureAddress( pNewTexture, NewCompressedData.m_pVoid, NULL );

        if( bFirstUpdate || vNewTextureAmbient != NewCompressedData.m_vAmbient )
        {
            // All the D3D textures for a particular buffer alias the same memory, 
            // so we need to lock the old texture
            AllFormatColorBuffer& ColorBuffer = m_ColorBuffer[iNewTestTexture];
            IDirect3DTexture9* pTexture = ColorBuffer.m_pTexture[ iOldSourceType ];
            D3DLOCKED_RECT LockedRect;

            if ( !bFirstUpdate )
            {
                pTexture->LockRect( 0, &LockedRect, NULL, 0 );
            }

            ComputeCompressedData( iNewTestTexture, iNewTextureConversion );

            if ( !bFirstUpdate )
            {
                pTexture->UnlockRect( 0 );
            }
        }

        if( bFirstUpdate || iNewFrontBufferType != iOldFrontBufferType )
        {
            switch ( iNewFrontBufferType )
            {
            case FRONT_BUFFER_8_BIT_GAMMA:
            case FRONT_BUFFER_8_BIT_LINEAR:
                m_d3dpp.FrontBufferFormat = D3DFMT_LE_X8R8G8B8;
                break;

            case FRONT_BUFFER_10_BIT_GAMMA:
            case FRONT_BUFFER_10_BIT_LINEAR:
                m_d3dpp.FrontBufferFormat = D3DFMT_LE_X2R10G10B10;
                break;
            }
            m_pd3dDevice->Reset( &m_d3dpp );
        }

        if( bFirstUpdate || iNewFrontBufferType != iOldFrontBufferType || iNewDisplayGamma != iOldDisplayGamma )
        {
            switch ( iNewFrontBufferType )
            {
            case FRONT_BUFFER_8_BIT_GAMMA:
            case FRONT_BUFFER_8_BIT_LINEAR:
                m_pd3dDevice->SetGammaRamp( 0, 0, &g_GammaRamp[iNewDisplayGamma].m_8BitGamma );
                break; 

            case FRONT_BUFFER_10_BIT_GAMMA:
            case FRONT_BUFFER_10_BIT_LINEAR:
                m_pd3dDevice->SetPWLGamma( 0, &g_GammaRamp[iNewDisplayGamma].m_PWLGamma );
                break; 

            default:
                assert( false );
            }
        }

        bFirstUpdate = FALSE;
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: DrawGeometryToRenderTarget()
// Desc: Draw a full texture into a render target, using either a custom shader or a 
// simple copy.
//--------------------------------------------------------------------------------------
VOID Sample::DrawGeometryToRenderTarget(IDirect3DTexture9* pSrcTexture,
                                       IDirect3DSurface9* pDstRenderTarget,
                                       IDirect3DPixelShader9* pPixelShader, 
                                       IDirect3DVertexBuffer9* pVB, 
                                       IDirect3DIndexBuffer9* pIB, 
                                       UINT iIBSize)
{
    // Make sure that the required resources exist
    assert( pSrcTexture );
    assert( pDstRenderTarget );

    // Make sure that the required shaders and objects exist
    assert( pPixelShader );

    // Query stats for the src and dst
    XGTEXTURE_DESC SrcDesc;
    XGGetTextureDesc( pSrcTexture, 0, &SrcDesc );
    XGTEXTURE_DESC DstDesc;
    XGGetSurfaceDesc( pDstRenderTarget, &DstDesc );

    // Set all the right D3D state and resources
    m_pd3dDevice->SetIndices( pIB );
    m_pd3dDevice->SetStreamSource( 0, pVB, 0, sizeof( TestGeometryVertex ) );
    m_pd3dDevice->SetVertexDeclaration( m_pVtxDecl );
    m_pd3dDevice->SetVertexShader( m_pVertexShader );
    m_pd3dDevice->SetTexture( 0, pSrcTexture );
    m_pd3dDevice->SetPixelShader( pPixelShader );
    m_pd3dDevice->SetRenderTarget( 0L, pDstRenderTarget );

    // Draw the rect
    m_pd3dDevice->DrawIndexedPrimitive( D3DPT_QUADLIST, 0, 0, 0, 0, iIBSize / 4 );
}


//--------------------------------------------------------------------------------------
// Name: CopyRenderTargetToTexture()
// Desc: Resolve a full render target into a texture.
//--------------------------------------------------------------------------------------
VOID Sample::CopyRenderTargetToTexture(IDirect3DSurface9* pSrcRenderTarget,
                                       IDirect3DTexture9* pDstTexture )
{
    // Make sure that the required resources exist
    assert( pSrcRenderTarget );
    assert( pDstTexture );

    // Query stats for the src and dst
    XGTEXTURE_DESC SrcDesc;
    XGGetSurfaceDesc( pSrcRenderTarget, &SrcDesc );
    XGTEXTURE_DESC DstDesc;
    XGGetTextureDesc( pDstTexture, 0, &DstDesc );

    // Set all the right D3D state and resources
    m_pd3dDevice->SetRenderTarget( 0L, pSrcRenderTarget );

    // Some format combinations require an exp bias.  
    DWORD ColorExpBias = 0;
    if( SrcDesc.Format == D3DFMT_A2B10G10R10F_EDRAM
        || SrcDesc.Format == D3DFMT_A16B16G16R16_EDRAM )
        ColorExpBias = ( DWORD )D3DRESOLVE_EXPONENTBIAS( -5 );

    // Resolve the render target
    m_pd3dDevice->Resolve( D3DRESOLVE_RENDERTARGET0 | ColorExpBias, NULL, pDstTexture,
                           NULL, 0, 0, NULL, 1.0f, 0L, NULL );
}

//-----------------------------------------------------------------------------
// Name: Render()
// Desc: Called once per frame, to render the scene.
//-----------------------------------------------------------------------------
HRESULT Sample::Render()
{
    // Recover all the relevant settings from the UI
    UINT iShaderDegamma = m_ShaderDegammaParam.GetValue();
    UINT iShaderGamma = m_ShaderGammaParam.GetValue();
    UINT iTestGeometry = m_TestGeometryParam.GetValue();
    D3DTexture* pTexture = m_ColorBuffer[ m_TestTextureParam.GetValue() ].m_pTexture[ m_SourceTypeParam.GetValue() ];
    D3DSurface* pRenderTarget = m_pRenderTarget[ m_DestTypeParam.GetValue() ];
    D3DTexture* pFrontBuffer = m_pFrontBuffer[ m_FrontBufferParam.GetValue() ];
    UINT iDitherCoverageType = m_DitherCoverageParam.GetValue();
    UINT iDitherTestType = m_DitherTestParam.GetValue();

    m_pd3dDevice->SetRenderTarget( 0, pRenderTarget );

    PIXBeginNamedEvent( 0, "Clear to pattern" );

    // Clear the viewport to a pattern (this is visible behind the non-full-screen test geometry)
    D3DCOLOR D3D_BLACK = D3DCOLOR_ARGB( 0x00, 0x00, 0x00, 0x00 );
    D3DCOLOR D3D_TEAL  = D3DCOLOR_ARGB( 0x00, 0x00, 0xff, 0xff );
    UINT iStripCount = 32;
    UINT iStripWidth = m_d3dpp.BackBufferWidth / iStripCount;
    UINT iStripHeight = m_d3dpp.BackBufferHeight;
    for( UINT iStrip = 0; iStrip < iStripCount; ++iStrip )
    {
        D3DRECT d3dRectBlack = { iStripWidth * iStrip, 0, iStripWidth * ( iStrip + 1 ), iStripHeight };
        m_pd3dDevice->Clear( 1, &d3dRectBlack, D3DCLEAR_TARGET, D3D_BLACK, 1.0f, 0L );
        ++iStrip;
        D3DRECT d3dRectTeal = { iStripWidth * iStrip, 0, iStripWidth * ( iStrip + 1 ), iStripHeight };
        m_pd3dDevice->Clear( 1, &d3dRectTeal, D3DCLEAR_TARGET, D3D_TEAL, 1.0f, 0L );
    }

    PIXEndNamedEvent();

    // Record the timing of the copy-with-gamma operation
    PIXBeginNamedEvent( 0, "Apply lighting" );

    XMMATRIX matWorldToViewTranspose = XMMatrixTranspose( m_matWorldToView[ iTestGeometry ] );
    XMMATRIX matWorldToScreenTranspose = XMMatrixTranspose( m_matWorldToScreen[ iTestGeometry ] );
    m_pd3dDevice->SetVertexShaderConstantF( 4, (FLOAT*) &matWorldToViewTranspose, 4 );
    m_pd3dDevice->SetVertexShaderConstantF( 0, (FLOAT*) &matWorldToScreenTranspose, 4 );

    m_pd3dDevice->SetPixelShaderConstantF( 0, (FLOAT*) &m_RuntimeAmbientParam.GetValue(), 1 );

    FLOAT vShaderGammaCalc[4] = 
    { 
        (FLOAT) iShaderDegamma, 
        (FLOAT) iShaderGamma, 
        0.0f, 
        0.0f 
    };
    m_pd3dDevice->SetPixelShaderConstantF( 1, vShaderGammaCalc, 1 );

    m_pd3dDevice->SetPixelShaderConstantF( 2, (FLOAT*) &m_vLightDirection, 1 );
    m_pd3dDevice->SetPixelShaderConstantF( 3, (FLOAT*) (FLOAT*) &m_LightingColorParam.GetValue(), 1 );

    DrawGeometryToRenderTarget( pTexture, pRenderTarget, m_pGammaGenericPS, 
        m_pVB[ iTestGeometry ], m_pIB[ iTestGeometry ], m_iIndexCount[ iTestGeometry ] );

    PIXEndNamedEvent();

    PIXBeginNamedEvent( 0, "Dither" );

    FLOAT fDitherType[4] = 
    { 
        (FLOAT) iDitherCoverageType, 
        (FLOAT) iDitherTestType, 
        0.0f, 
        0.0f 
    };
    m_pd3dDevice->SetPixelShaderConstantF( 4, (FLOAT*) &fDitherType, 1 );

    DrawGeometryToRenderTarget( pTexture, pRenderTarget, m_pDitherPS, 
        m_pVB[ iTestGeometry ], m_pIB[ iTestGeometry ], m_iIndexCount[ iTestGeometry ]);

    PIXEndNamedEvent();

    // Output statistics
    m_Timer.MarkFrame();

    PIXBeginNamedEvent( 0, "Render UI" );

    if( m_bDrawHelp )
    {
        m_Help.Render( &m_Font, g_HelpCallouts, NUM_HELP_CALLOUTS );
    }
    else if( m_bDrawUI )
    {
        m_Font.Begin();

        // Overriding ATG::Font behavior because alpha blend needs special handling on 
        // render targets which use exponent bias.
        m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );

        m_Font.SetScaleFactors( 1.2f, 1.2f );
        m_Font.DrawText( 0, 0, 0xffff00ff, L"Gamma" );

        FLOAT fParamY = 40.0f;

        for( UINT i = 0; i < GAMMA_PARAM_COUNT; ++i )
        {
            GammaParam* Param = m_ParamArray[i];

            Param->RenderUI( &m_Font, fParamY, ( i == m_iActiveUIParameter ) );
            fParamY += 30.0f;
        }

        m_Font.End();
    }

    PIXEndNamedEvent();

    // Present the scene
    m_pd3dDevice->SynchronizeToPresentationInterval();

    CopyRenderTargetToTexture( pRenderTarget, pFrontBuffer );

    m_pd3dDevice->Swap( pFrontBuffer, NULL );

    m_pd3dDevice->UnsetAll();

    return S_OK;
}

#include <pix.h>
//-----------------------------------------------------------------------------
// Name: main()
// Desc: Entry point to the program.
//-----------------------------------------------------------------------------
VOID __cdecl main()
{
    Sample atgApp;
    ATG::GetVideoSettings( &atgApp.m_d3dpp.BackBufferWidth, &atgApp.m_d3dpp.BackBufferHeight );

    // Use fixed back buffer resolution regardless of output dimensions
    atgApp.m_d3dpp.BackBufferWidth = 1280;
    atgApp.m_d3dpp.BackBufferHeight = 720;

    // We use custom everything
    atgApp.m_d3dpp.DisableAutoFrontBuffer = TRUE;
    atgApp.m_d3dpp.DisableAutoBackBuffer = TRUE;

    atgApp.m_d3dpp.FrontBufferFormat = D3DFMT_LE_X8R8G8B8;

    atgApp.Run();
}



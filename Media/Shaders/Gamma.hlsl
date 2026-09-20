//--------------------------------------------------------------------------------------
// Gamma.hlsl
//
// Custom shaders for the Gamma sample.
//
// XNA Developer Connection
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------
// Keep these in sync with the enums in Gamma.cpp
//--------------------------------------------------------------------------------------
#define DEGAMMA_CALC_NONE   0 
#define DEGAMMA_CALC_PWL    1 
#define DEGAMMA_CALC_2_0    2 
#define DEGAMMA_CALC_2_2    3 
#define DEGAMMA_CALC_SRGB   4 
#define DEGAMMA_CALC_TV     5 

#define GAMMA_CALC_NONE     0 
#define GAMMA_CALC_PWL      1 
#define GAMMA_CALC_2_0      2 
#define GAMMA_CALC_2_2      3 
#define GAMMA_CALC_SRGB     4 
#define GAMMA_CALC_TV       5 

#define DITHER_COVERAGE_NONE         0
#define DITHER_COVERAGE_FULLSCREEN   1
#define DITHER_COVERAGE_SIDEBYSIDE   2

#define DITHER_TEST_LINEAR_SPACE    0
#define DITHER_TEST_GAMMA_SPACE     1

//--------------------------------------------------------------------------------------
// Vertex shader constants
//--------------------------------------------------------------------------------------
uniform float4x4 g_matWorldToScreen : register(c0);
uniform float4x3 g_matWorldToView   : register(c4);

//--------------------------------------------------------------------------------------
// Pixel shader constants
//--------------------------------------------------------------------------------------
uniform float3  g_vAmbient          : register(c0);
uniform int2    g_iGammaCalc        : register(c1);
uniform float3  g_vLightDirection   : register(c2);
uniform float3  g_vLightColor       : register(c3);
uniform int2    g_iDitherType       : register(c4);

//--------------------------------------------------------------------------------------
// Texture samplers
//--------------------------------------------------------------------------------------
sampler Sampler0  : register(s0);

//--------------------------------------------------------------------------------------
// Name: CopyTextureVS()
// Desc: Pass-thru shader
//--------------------------------------------------------------------------------------
struct VERTEX_IN
{
    float3 Position     : POSITION0;
    float2 TexCoord     : TEXCOORD0;
    float3 Normal       : NORMAL0;
};

struct INTERPOLATORS
{
    float4 Position     : POSITION0;
    float2 TexCoord     : TEXCOORD0;
    float3 Normal       : NORMAL0;
};

INTERPOLATORS DefaultVS( VERTEX_IN In  )
{
    INTERPOLATORS Out;
    Out.Position    = mul( float4( In.Position, 1.0f ), g_matWorldToScreen );
    Out.TexCoord    = In.TexCoord;
    Out.Normal      = mul( In.Normal, g_matWorldToView );
    return Out;
}


//--------------------------------------------------------------------------------------
// The various supported gamma-to-linear and linear-to-gamma conversions
//--------------------------------------------------------------------------------------
float3 DegammaCalcNone( float3 vColor )
{
    return vColor;
}


float3 DegammaCalcPWL( float3 vColor )
{
    return ( vColor > 3.0f / 4.0f )
        ? ( 1.0f / 2.0f  + ( vColor - 3.0f / 4.0f ) * 2.0f )
        : ( vColor > 3.0f / 8.0f )
        ? ( 1.0f / 8.0f + ( vColor.xyz - 3.0f / 8.0f ) * 1.0f )
        : ( vColor > 1.0f / 4.0f )
        ? ( 1.0 / 16.0f + ( vColor - 1.0f / 4.0f ) / 2.0f )
        : ( 0.0 + ( vColor - 0.0f ) / 4.0f );
}


float3 DegammaCalc_2_0( float3 vColor )
{
    return vColor * vColor;
}


float3 DegammaCalc_2_2( float3 vColor )
{
    return pow( vColor, 2.2f );
}


float3 DegammaCalc_sRGB( float3 vColor )
{
    return ( vColor <= 0.04045f )
        ? vColor / 12.92f
        : pow( ( vColor + 0.055f ) / 1.055f, 2.4f );
}


float3 DegammaCalc_TV( float3 vColor )
{
    return ( vColor <= 0.0812f )
        ? vColor / 4.5f
        : pow( ( vColor + 0.099f ) / 1.099f, 1.0f / 0.45f );
}


float3 GammaCalcNone( float3 vColor )
{
    return vColor;
}


float3 GammaCalcPWL( float3 vColor )
{
    return ( vColor > 1.0f / 2.0f )
            ? ( 3.0f / 4.0f + ( vColor - 1.0f / 2.0f ) / 2.0f )
            : ( vColor > 1.0f / 8.0f )
            ? ( 3.0f / 8.0f + ( vColor - 1.0f / 8.0f ) * 1.0f )
            : ( vColor > 1.0f / 16.0f )
            ? ( 1.0f / 4.0f + ( vColor - 1.0f / 16.0f ) * 2.0f )
            : ( 0.0f + ( vColor - 0.0f ) * 4.0f );
}


float3 GammaCalc_2_0( float3 vColor )
{
    return sqrt( vColor );
}


float3 GammaCalc_2_2( float3 vColor )
{
    return pow( vColor, 1.0f / 2.2f );
}


float3 GammaCalc_sRGB( float3 vColor )
{
    return ( vColor <= 0.0031308f )
        ? vColor * 12.92f
        : pow( vColor, 1.0f / 2.4f ) * 1.055f - 0.055f;
}


float3 GammaCalc_TV( float3 vColor )
{
    return ( vColor <= 0.018f )
        ? vColor * 4.5f
        : pow( vColor, 0.45f ) * 1.099f - 0.099f;
}


float3 DegammaGeneric( float3 vColor, int iDegammaCalc )
{
    switch ( iDegammaCalc )
    {
        case DEGAMMA_CALC_NONE:
        vColor = DegammaCalcNone( vColor );
        break;
        
        case DEGAMMA_CALC_PWL:
        vColor = DegammaCalcPWL( vColor );
        break;
        
        case DEGAMMA_CALC_2_0:
        vColor = DegammaCalc_2_0( vColor );
        break;
        
        case DEGAMMA_CALC_2_2:
        vColor = DegammaCalc_2_2( vColor );
        break;
        
        case DEGAMMA_CALC_SRGB:
        vColor = DegammaCalc_sRGB( vColor );
        break;
        
        case DEGAMMA_CALC_TV:
        vColor = DegammaCalc_TV( vColor );
        break;
    }
    
    return vColor;
}


float3 GammaGeneric( float3 vColor, int iGammaCalc )
{
    switch ( iGammaCalc )
    {
        case GAMMA_CALC_NONE:
        vColor = GammaCalcNone( vColor );
        break;
        
        case GAMMA_CALC_PWL:
        vColor = GammaCalcPWL( vColor );
        break;
        
        case GAMMA_CALC_2_0:
        vColor = GammaCalc_2_0( vColor );
        break;
        
        case GAMMA_CALC_2_2:
        vColor = GammaCalc_2_2( vColor );
        break;
        
        case GAMMA_CALC_SRGB:
        vColor = GammaCalc_sRGB( vColor );
        break;
        
        case GAMMA_CALC_TV:
        vColor = GammaCalc_TV( vColor );
        break;
    }
    
    return vColor;
}

float3 PerformShading ( INTERPOLATORS In, in float3 vColor )
{
    vColor *= g_vAmbient;
    vColor += saturate( dot ( In.Normal, -g_vLightDirection ) ) * g_vLightColor;
    
    return vColor;
} 

//--------------------------------------------------------------------------------------
// Name: DiffuseLitGamma
// Desc: Performs diffuse lighting by a single directional light, with optional 
//       manual gamma-to-linear and linear-to-gamma calculation.
//--------------------------------------------------------------------------------------
float4 GammaGenericPS( INTERPOLATORS In ) : COLOR0
{
    float4 vColor = tex2D( Sampler0, In.TexCoord );
    
    vColor.xyz = DegammaGeneric( vColor.xyz, g_iGammaCalc.x );
    
    vColor.xyz = PerformShading( In, vColor.xyz );
    
    vColor.xyz = GammaGeneric( vColor.xyz, g_iGammaCalc.y );
    
    return vColor;
}

//--------------------------------------------------------------------------------------
// Dithering functions
//--------------------------------------------------------------------------------------
static int iDitherPattern[2][2] = 
{
    {0x00, 0x03, },
    {0x02, 0x01, },
};


int3 DitherPatternFunc( int2 vScreenPos )
{
    int3 iReturn = 0;

    [unroll]
    for ( int iLevel = 0; iLevel < 4; ++iLevel )
    {
        iReturn *= 4;
    
        int2 vDitherPos = vScreenPos % 2;

        iReturn.x += iDitherPattern[ vDitherPos.x ]    [ vDitherPos.y ];
        iReturn.y += iDitherPattern[ 1 - vDitherPos.x ][ vDitherPos.y ];
        iReturn.z += iDitherPattern[ vDitherPos.x ]    [ 1 - vDitherPos.y ];
        
        vScreenPos /= 2;
    }

    return iReturn;
}


float3 Dither( float3 vColor, int2 vScreenPos )
{
    int3 iDither = DitherPatternFunc( vScreenPos );
    return iDither < ( 0xff * vColor + 0.5f ) ? 1.0f : 0.0f;
}

void PixelKillTest( float2 vScreenPosition )
{
    bool bDrawPixel = true;
    switch ( g_iDitherType.x )
    {
    case DITHER_COVERAGE_NONE:
        bDrawPixel = false;
        break;
        
    case DITHER_COVERAGE_FULLSCREEN:
        bDrawPixel = true;
        break;
        
    case DITHER_COVERAGE_SIDEBYSIDE:
        int iColumn = ( vScreenPosition.x / 16 );
        bDrawPixel = ( frac( iColumn / 2.0f ) == 0 );
        break;
    }
    
    clip( bDrawPixel - 0.5f );  // clip kills pixel if arg < 0
}

float4 DitherGenericPS( INTERPOLATORS In, in float2 vScreenPosition : VPOS ) : COLOR0
{
    PixelKillTest( vScreenPosition );

    float4 vColor = tex2D( Sampler0, In.TexCoord );
    
    vColor.xyz = DegammaGeneric( vColor.xyz, g_iGammaCalc.x );

    vColor.xyz = PerformShading( In, vColor.xyz );
    
    if ( g_iDitherType.y == DITHER_TEST_GAMMA_SPACE )
    {
        vColor.xyz = GammaCalc_TV( vColor.xyz );
    }
    
    vColor.xyz = Dither( vColor.xyz, vScreenPosition );
    
    return vColor;
}


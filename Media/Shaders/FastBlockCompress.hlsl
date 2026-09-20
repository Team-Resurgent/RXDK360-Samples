//---------------------------------------------------------------------------------------------------------
// FastBlockCompress.hlsl
//
// File containing utility shaders used by the FastBlockCompress sample
//
// XNA Developer Connection
// Copyright ( C ) Microsoft Corporation. All rights reserved.
//---------------------------------------------------------------------------------------------------------

struct VERTEX
{
    float4 vPosition    : POSITION0;
    float2 vTexCoord    : TEXCOORD0;
};

struct INTERPOLATORS
{
    float4 vPosition     : POSITION0;
    float4 vTexCoord     : TEXCOORD0;
};

//---------------------------------------------------------------------------------------------------------
// Name: ScreenSpaceShaderVS()
// Desc: Simple pass through. 
//---------------------------------------------------------------------------------------------------------
float4x4 g_matWVP           : register(c0);
float2 g_vTextureDims       : register(c4);

INTERPOLATORS ScreenSpaceShaderVS( VERTEX In )
{
    INTERPOLATORS Out;
    Out.vPosition = mul( In.vPosition, g_matWVP );
    Out.vTexCoord.xy = In.vTexCoord;
    
    // zw are helper data for GetTiledOffset2D.  
    // The integer part is tile number, 
    // and the fractional part is coords within a tile.
    // These are exact precision for power-of-two dimensions
    Out.vTexCoord.zw = In.vTexCoord * g_vTextureDims;
    
    return Out;
}


//---------------------------------------------------------------------------------------------------------
// Name: CopyTexturePS()
// Desc: Copies a texture. 
//---------------------------------------------------------------------------------------------------------
sampler samplerInput        : register(s0);

float4 CopyTexturePS( INTERPOLATORS In ) : COLOR
{
    float4 vColor = tex2D( samplerInput, In.vTexCoord.xy );
    return vColor;
}

//---------------------------------------------------------------------------------------------------------
// Name: SquaredDiffPS()
// Desc: Part of RMS Error calculation 
//---------------------------------------------------------------------------------------------------------
sampler samplerRaw          : register(s0);
sampler samplerCompressed   : register(s1);

#define DIFF_MODE_COLOR_ALPHA   0
#define DIFF_MODE_UV            1

int g_iDiffMode             : register(c0);          

float4 SquaredDiffPS( INTERPOLATORS In ) : COLOR
{
    float2 vTexCoord = In.vTexCoord;
    float4 vRawTexel;
    float4 vCompressedTexel;
    
    asm
    {
        tfetch2D vRawTexel, vTexCoord, samplerRaw, \
            MinFilter = point, MagFilter = point, MipFilter = point
        tfetch2D vCompressedTexel, vTexCoord, samplerCompressed, \
            MinFilter = point, MagFilter = point, MipFilter = point
    };
    
    float4 vDiff = vCompressedTexel - vRawTexel;
    
    // This doesn't really need to be a branch, as .ba will always be equal for normal maps
    switch( g_iDiffMode )
    {
    case DIFF_MODE_COLOR_ALPHA:
    default:
        float fSquaredErrorColor = dot( vDiff.rgb, vDiff.rgb );
        float fSquaredErrorAlpha = vDiff.a * vDiff.a;
        
        return float4( fSquaredErrorColor, fSquaredErrorAlpha, 0.0f, 0.0f );
        
    case DIFF_MODE_UV:
        float fSquaredErrorUV = dot( vDiff.rg, vDiff.rg );
        
        return float4( fSquaredErrorUV, 0.0f, 0.0f, 0.0f );
    }
}


//---------------------------------------------------------------------------------------------------------
// Name: DownScale2x2PS()
// Desc: Downscale by 2x in each direction, for mip generation.  
// This is the 'reduction' part of RMS Error calculation.
//---------------------------------------------------------------------------------------------------------
sampler samplerDiff          : register(s0);

float4 DownScale2x2PS( INTERPOLATORS In ) : COLOR
{
    float2 vTexCoord = In.vTexCoord;
    float4 vTexel00, vTexel01, vTexel10, vTexel11;
    
    asm
    {
        tfetch2D vTexel00, vTexCoord, samplerDiff, OffsetX = -0.5, OffsetY = -0.5, \
            MinFilter = point, MagFilter = point, MipFilter = point
        tfetch2D vTexel01, vTexCoord, samplerDiff, OffsetX = -0.5, OffsetY = +0.5, \
            MinFilter = point, MagFilter = point, MipFilter = point
        tfetch2D vTexel10, vTexCoord, samplerDiff, OffsetX = +0.5, OffsetY = -0.5, \
            MinFilter = point, MagFilter = point, MipFilter = point
        tfetch2D vTexel11, vTexCoord, samplerDiff, OffsetX = +0.5, OffsetY = +0.5, \
            MinFilter = point, MagFilter = point, MipFilter = point
    };
    
    return 0.25f * ( vTexel00 + vTexel01 + vTexel10 + vTexel11 );
}



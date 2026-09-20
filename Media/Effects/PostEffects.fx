//--------------------------------------------------------------------------------------
// PostEffects.fx
//
// This effect contains the techniques and shaders for SceneViewer2 post effects.
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

// Focal settings are:
// .x = focal depth (center of sharp focused area in Z space)
// .y = focal aperture (size of focused region in Z space)
// .z = focal slope (amount of fully focused area, 1.0 = none, infinity = entire aperture)
// .w = maximum circle of confusion in pixels (max blurriness)
float4      g_FocalSettings = float4( 0.0f, 1.0f, 2.0f, 5.0f );
float2      g_InverseScreenSize = float2( 1 / 1280.0f, 1 / 720.0f );

#define BUFFER_SAMPLER sampler_state { MipFilter = NONE; MinFilter = LINEAR; MagFilter = LINEAR; AddressU = CLAMP; AddressV = CLAMP; }                         
#define BUFFER_SAMPLER_POINT sampler_state { MipFilter = NONE; MinFilter = POINT; MagFilter = POINT; AddressU = CLAMP; AddressV = CLAMP; }                         
sampler2D color_sampler = BUFFER_SAMPLER;
sampler2D depth_sampler = BUFFER_SAMPLER_POINT; 

static const float4 g_Grayscale = float4( 0.3, 0.59, 0.11, 0 );

#define TAP_COUNT 12
static const float2 g_SamplePositions[TAP_COUNT] = 
{
float2( 0.200887527842703, -0.805816066868008 ),
float2( 0.169759583602972, 0.787268282932537 ),
float2( -0.639597778815228, -0.370236979450183 ),
float2( 0.629148098269191, 0.367398185340504 ),
float2( -0.456211403483755, 0.542374725109481 ),
float2( -0.411828977295713, -0.484566120009967 ),
float2( 0.106871055317844, 0.59918690478536 ),
float2( -0.117388437710382, -0.518626519610864 ),
float2( 0.436862373204161, -0.182937652849816 ),
float2( 0.206632546759667, 0.343668469981935 ),
float2( -0.330409446933952, 0.175773621457362 ),
float2( -0.1, -0.16089955706328 ),
};


struct VS_INPUT
{
    float4  vPos    : POSITION0;
    float2  vTex    : TEXCOORD0;
};

struct VS_OUTPUT
{
    float4  Pos     : POSITION;
    float2  Tex     : TEXCOORD0;
};

struct PS_INPUT
{
    float2  Tex     : TEXCOORD0;
};

VS_OUTPUT vs_passthru( VS_INPUT In )
{
    VS_OUTPUT Out;
    Out.Pos = In.vPos;
    Out.Tex = In.vTex;
    return Out;
}

float4 ps_texture( PS_INPUT In ) : COLOR
{ 
    float4 Color = tex2D( color_sampler, In.Tex );
    return Color;
}

float4 ps_grayscale( PS_INPUT In ) : COLOR
{
    float4 Color = tex2D( color_sampler, In.Tex );
    return dot( Color, g_Grayscale );
}

float4 tex2DOffset(sampler2D ss, float2 uv, float2 offset)
{
  float4 result;
  float offsetX = offset.x;
  float offsetY = offset.y;
  asm {
    tfetch2D result, uv, ss, OffsetX=offsetX, OffsetY=offsetY
  };
  return result;
}

float ComputeBlurAmount( float DepthValue )
{
    float DepthRamp = abs( ( DepthValue - g_FocalSettings.x ) / g_FocalSettings.y );
    float v = ( DepthRamp * g_FocalSettings.z ) - ( g_FocalSettings.z - 1 );
    return min( v, 1 );
}

float4 ps_PoissonDOF( PS_INPUT In, uniform int TapCount ) : COLOR
{
    float4 DepthSample = tex2D( depth_sampler, In.Tex );
    float4 Color = tex2D( color_sampler, In.Tex );
    
    float BlurAmount = ComputeBlurAmount( DepthSample.r );
    
    if( BlurAmount > 0 )
    {
        float2 BlurSize = g_InverseScreenSize * BlurAmount * g_FocalSettings.w;
        for( int i = 1; i < TapCount; i++ )
        {
            float2 offset = BlurSize * g_SamplePositions[i];
            float4 TapSample = tex2D( color_sampler, In.Tex + offset );
            Color += TapSample;
        }
        Color /= TapCount;
    }
    return Color;
}

float4 ps_EdgeDetect( PS_INPUT In ) : COLOR
{
    float2 texCoord = In.Tex;
    float4 DepthValuesA = 0;
    float4 DepthValuesB = 0;
    asm {
        tfetch2D DepthValuesA.x___, texCoord, depth_sampler, OffsetX = -1.0, OffsetY =  0.0, MinFilter=point, MagFilter=point, MipFilter=point
        tfetch2D DepthValuesA._x__, texCoord, depth_sampler, OffsetX =  1.0, OffsetY =  0.0, MinFilter=point, MagFilter=point, MipFilter=point
        tfetch2D DepthValuesA.__x_, texCoord, depth_sampler, OffsetX =  0.0, OffsetY = -1.0, MinFilter=point, MagFilter=point, MipFilter=point
        tfetch2D DepthValuesA.___x, texCoord, depth_sampler, OffsetX =  0.0, OffsetY =  1.0, MinFilter=point, MagFilter=point, MipFilter=point
        tfetch2D DepthValuesB.x___, texCoord, depth_sampler, OffsetX =  0.0, OffsetY =  0.0, MinFilter=point, MagFilter=point, MipFilter=point
    };
    
    float Value = dot( DepthValuesA, -1 );
    Value += 4 * DepthValuesB.x;
    return Value * 1000;
}

float4 ps_EdgeDetectWithColor( PS_INPUT In ) : COLOR
{
    return ps_texture( In ) + ps_EdgeDetect( In );
}

technique NoEffect
{
    pass
    {
        alphablendenable = false;
        vertexshader = compile vs_3_0 vs_passthru();
        pixelshader = compile ps_3_0 ps_texture();
    }
}

technique PoissonDOF12
{
    pass
    {
        alphablendenable = false;
        vertexshader = compile vs_3_0 vs_passthru();
        pixelshader = compile ps_3_0 ps_PoissonDOF( 12 );
    }
}

technique PoissonDOF8
{
    pass
    {
        alphablendenable = false;
        vertexshader = compile vs_3_0 vs_passthru();
        pixelshader = compile ps_3_0 ps_PoissonDOF( 8 );
    }
}

technique PoissonDOF5
{
    pass
    {
        alphablendenable = false;
        vertexshader = compile vs_3_0 vs_passthru();
        pixelshader = compile ps_3_0 ps_PoissonDOF( 5 );
    }
}

technique Grayscale
{
    pass
    {
        alphablendenable = false;
        vertexshader = compile vs_3_0 vs_passthru();
        pixelshader = compile ps_3_0 ps_grayscale();
    }
}

technique EdgeDetect
{
    pass
    {
        alphablendenable = false;
        vertexshader = compile vs_3_0 vs_passthru();
        pixelshader = compile ps_3_0 ps_EdgeDetect();
    }
}

technique EdgeDetectWithColor
{
    pass
    {
        alphablendenable = false;
        vertexshader = compile vs_3_0 vs_passthru();
        pixelshader = compile ps_3_0 ps_EdgeDetectWithColor();
    }
}


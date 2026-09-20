//--------------------------------------------------------------------------------------
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

float4x3 WorldView;
float4x4 Projection;
float4 DirFromLight;

float   nSamples = 512.0;
float   thickness = .1;
float4  color = { 0, 1, 0, 1 };

float rangeMax = 1.0;
float rangeMin = -1.0f;

struct VS_OUTPUT
{
    float4  Pos     : POSITION;
    float2  uv      : TEXCOORD0;
};


sampler amplitude_sampler  = sampler_state
{
    MipFilter = POINT;
    MinFilter = POINT;
    MagFilter = POINT;
    
    AddressU = CLAMP;
    AddressV = CLAMP;
};                                

VS_OUTPUT vsWave( 
  float2    uv      : BARYCENTRIC,
  int       quadID  : QUADID,
  float     index   : INDEX )
{
    VS_OUTPUT Out;

	uv *= 2.0;
	uv -= float2( .5, .5 );
	float u = (index + uv.x) / nSamples;
    float amplitude = tex2Dlod( amplitude_sampler, float4( u,.5,0,0) ).x;


    amplitude /= ( rangeMax - rangeMin ) /2;
    amplitude += -1 - rangeMin;

    Out.Pos.x = u * 2 - 1;
    Out.Pos.y = uv.y * thickness + amplitude;
    Out.Pos.y *= 1 - thickness/2;
    Out.Pos.z = 0;
    Out.Pos.w = 1;        

    Out.uv = uv;    
 
    return Out;
};


float4 psWave( 
    float2 uv : TEXCOORD0
 ) : COLOR
{
    float4 result = color;

    float falloff = 1.0 - abs( uv.y * 2.0 );
    falloff = pow( falloff, 3 );
    result.a *= falloff;

    return result;
};

technique DrawWave
{
    pass 
    {
        VertexShader = compile vs_3_0 vsWave();
        PixelShader = compile ps_3_0 psWave();
    
        //FillMode = WIREFRAME;
        TessellationMode = CONTINUOUS;
        MinTessellationLevel = 3.0;
        MaxTessellationLevel = 3.0;
        
        AlphaBlendEnable = true;
        DestBlend = INVSRCALPHA;
        SrcBlend = SRCALPHA;
    }
}


//--------------------------------------------------------------------------------------
// Planet.hlsl
//
// This effect contains shaders for simple planet rendering.  
//
// XNA Developer Connection
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#define TEXTURE_SAMPLER sampler_state { MipFilter = LINEAR; MinFilter = LINEAR;\
                                        MagFilter = LINEAR; AddressU = WRAP;\
                                        AddressV = WRAP; }
shared float4x4 world_view_proj_matrix : register(c0);
shared float4x4 world_matrix : register(c4);
sampler simpleshader_sampler : register(s0) = TEXTURE_SAMPLER;

struct VS_OUTPUT
{
    float4  Pos     : POSITION;
    float4  Color   : COLOR;
    float2  Tex     : TEXCOORD0_centroid;
};

struct VS_INPUT
{
    float4  vPos    : POSITION0;
    float4  vColor  : COLOR0;
    float2  vTex    : TEXCOORD0;
};

VS_OUTPUT PlanetVS( VS_INPUT In )
{
    VS_OUTPUT Out;

    Out.Pos = mul( world_view_proj_matrix, float4( In.vPos.xyz, 1 ) );
    float3 normal = mul( world_matrix, float4( In.vPos.xyz, 0.0f) );
    Out.Color = clamp(dot(normal, float3( -.707f, 0.0f, .707f)) * 2, 0, 1.0) + .5f;
    Out.Tex = In.vTex;
    return Out; 
};

float4 PlanetPS( VS_OUTPUT In ) : COLOR
{
    return tex2D( simpleshader_sampler, In.Tex ) * In.Color;
}

//--------------------------------------------------------------------------------------
// LightShafts_Spotlight.hlsl
//
// Shaders for spotlight rendering in the LightShafts sample
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
// ATI 3D Application Research Group. 
// Copyright (C) ATI Research, Inc. All rights reserved. 
//--------------------------------------------------------------------------------------


//--------------------------------------------------------------------------------------
// Vertex shader constants
//--------------------------------------------------------------------------------------
uniform float4x4 g_matWorldViewProj : register(c0);


//--------------------------------------------------------------------------------------
// Pixel shader constants
//--------------------------------------------------------------------------------------
uniform float4   g_vEyeL            : register(c0);


//--------------------------------------------------------------------------------------
// Samplers
//--------------------------------------------------------------------------------------
sampler BaseTextureSampler : register(s0);


//--------------------------------------------------------------------------------------
// Vertex structures
//--------------------------------------------------------------------------------------
struct VS_FRUSTUM_OUTPUT
{
    float4 vPos      : POSITION;
    float4 vColor    : COLOR0;
    float2 vTexCoord : TEXCOORD0;
};


//--------------------------------------------------------------------------------------
// Name: SpotlightFrustumVS()
// Desc: Vertex Shader for shaded "light source" at tip of frustum
//--------------------------------------------------------------------------------------
VS_FRUSTUM_OUTPUT SpotlightFrustumVS( float3 vPosition : POSITION )
{
    VS_FRUSTUM_OUTPUT Out = (VS_FRUSTUM_OUTPUT)0; 
    Out.vPos      = mul( float4(vPosition,1), g_matWorldViewProj );
    Out.vColor    = float4(1,1,1,1);
    return Out;
}


//--------------------------------------------------------------------------------------
// Name: SpotlightFrustumFrontVS()
// Desc: Vertex Shader for textured front of frustum
//--------------------------------------------------------------------------------------
VS_FRUSTUM_OUTPUT SpotlightFrustumFrontVS( float3 vPosition : POSITION, 
                                           float2 vTexCoord : TEXCOORD0 )
{
    VS_FRUSTUM_OUTPUT Out = (VS_FRUSTUM_OUTPUT)0; 
    Out.vPos      = mul( float4(vPosition,1), g_matWorldViewProj );
    Out.vTexCoord = vTexCoord;
    return Out;
}


//--------------------------------------------------------------------------------------
// Name: SpotlightWireFrustumVS()
// Desc: Vertex Shader for wireframe frustum
//--------------------------------------------------------------------------------------
VS_FRUSTUM_OUTPUT SpotlightWireFrustumVS( float3 vPosition : POSITION, 
                                          float4 vColor    : COLOR0 )
{
    VS_FRUSTUM_OUTPUT Out = (VS_FRUSTUM_OUTPUT)0; 
    Out.vPos      = mul( float4(vPosition,1), g_matWorldViewProj );
    Out.vColor    = vColor;
    return Out;
}


//--------------------------------------------------------------------------------------
// Name: SpotlightFrustumPS()
// Desc: 
//--------------------------------------------------------------------------------------
float4 SpotlightFrustumPS( float4 vColor : COLOR0 ) : COLOR
{
    return vColor;
}


//--------------------------------------------------------------------------------------
// Name: SpotlightFrustumFrontPS()
// Desc: 
//--------------------------------------------------------------------------------------
float4 SpotlightFrustumFrontPS( float2 vTexCoord  : TEXCOORD0 ) : COLOR
{
    return tex2D( BaseTextureSampler, vTexCoord );
}


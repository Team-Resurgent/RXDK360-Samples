//-----------------------------------------------------------------------------
// File: NuiPCConduit.hlsl
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
// Global variables
//-----------------------------------------------------------------------------
float4x4 mWorldViewProj : register(c0);		   // World * View * Projection transformation
texture g_Texture : register(t0);              // Color texture for mesh


//--------------------------------------------------------------------------------------
// Texture samplers
//--------------------------------------------------------------------------------------
sampler MeshTextureSampler = 
sampler_state
{
    Texture = <g_Texture>;
    MipFilter = LINEAR;
    MinFilter = LINEAR;
    MagFilter = LINEAR;
};


//-----------------------------------------------------------------------------
// Vertex shader output structure
//-----------------------------------------------------------------------------
struct VS_OUTPUT
{
    float4 Position   : POSITION;   // vertex position 
    float2 uv         : TEXCOORD0;
};


//-----------------------------------------------------------------------------
// Position and texture coordinate shader
//-----------------------------------------------------------------------------
VS_OUTPUT VS( in float2 vPosition : POSITION, in float2 uv : TEXCOORD0 )
{
	VS_OUTPUT Output;
	
  	Output.Position = mul( mWorldViewProj, float4( vPosition.x, vPosition.y, 0.0f, 1.0f ) );
  	Output.uv = uv;
    
    return Output;
}


//-----------------------------------------------------------------------------
// Vertex shader output structure
//-----------------------------------------------------------------------------
struct SIMPLE_VS_OUTPUT
{
    float4 Position   : POSITION;   // vertex position 
    float4 Color	  : COLOR;
};


//-----------------------------------------------------------------------------
// Simple position only vertex shader
//-----------------------------------------------------------------------------
SIMPLE_VS_OUTPUT SimpleVS( in float2 vPosition : POSITION, in float4 color : COLOR )
{
	SIMPLE_VS_OUTPUT Output;
	
	// and transform the vertex into projection space. 
    Output.Position = mul( mWorldViewProj, float4( vPosition.x, vPosition.y, 0.0f, 1.0f ) );
	Output.Color = color;
	
    return Output;
}


//--------------------------------------------------------------------------------------
// Pixel shader output structure
//--------------------------------------------------------------------------------------
struct PS_OUTPUT
{
    float4 color : COLOR0;      
};


PS_OUTPUT PS( VS_OUTPUT input )
{
	PS_OUTPUT result;
	result.color = tex2D(MeshTextureSampler, input.uv);

	return result;
}


PS_OUTPUT SimplePS( SIMPLE_VS_OUTPUT input )
{
	PS_OUTPUT result;
	result.color = input.Color;

	return result;
}
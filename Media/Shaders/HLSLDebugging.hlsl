//--------------------------------------------------------------------------------------
// HLSLDebugging.hlsl
//
// Basic diffuse lighting shader to demonstrate the use of HLSL assert and dbgprint
// 
// XNA Developer Connection
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

// This is defined in the build rules to enable/disable the HLSL debugging features
// for demonstation purposes
//#define HLSL_ENABLE_DEBUGGING

//--------------------------------------------------------------------------------------
// Vertex shader
//--------------------------------------------------------------------------------------

uniform float4x4 WorldViewProj : register(c0);  // World-view-projection matrix
uniform float4x4 World         : register(c4);  // World

struct VSOUT
{
    float4 Position  : POSITION;
    float2 TexCoord0 : TEXCOORD0;
    float  NdotL     : TEXCOORD1;
};

VSOUT VS_EntryPoint( const float3 Position   : POSITION,
                     const float3 Normal     : NORMAL, 
                     const float2 TexCoord0  : TEXCOORD0 )
{
    VSOUT  Output;

    // Transform the vertex
    Output.Position = mul( WorldViewProj, float4( Position, 1.0f ) );

    // Transform the normal
    float3 normal = mul( World, float4( Normal, 0.0f ) );

    // Pass thru base texcoords
    Output.TexCoord0 = TexCoord0;

#ifdef HLSL_ENABLE_DEBUGGING
    // Print the Length of the Normal
    // Output is viewable in PIX
    dbgprint("Normal Len: {0}", length( normal ));
#endif

#ifdef HLSL_ENABLE_DEBUGGING
    // Assert that the Normal is unit length
    // Use /Xassert or D3DXSHADEREX_ENABLE_ASSERTIONS to enable runtime asserts
    assert(length( normal ) > 0.995f && length( normal ) < 1.005f);
#endif

    // Calculate the per-vertex dot(N, L) lighting coefficient
    Output.NdotL = dot( normal, float3( -.707f, 0.0f, .707f ) );
    if ( Output.NdotL < 0 ) { Output.NdotL = 0; }

    return Output;
}


//--------------------------------------------------------------------------------------
// Pixel shader
//--------------------------------------------------------------------------------------

uniform extern sampler DiffuseMap : register(s0); // Diffuse texture

float4 PS_EntryPoint( VSOUT Input ) : COLOR
{
    float4 DiffuseTexture = tex2D( DiffuseMap, Input.TexCoord0 );

#ifdef HLSL_ENABLE_DEBUGGING
    // Print the dot(N, L) lighting coefficient
    // Output is viewable in PIX
    dbgprint("NdotL: {0}", Input.NdotL);
#endif

#ifdef HLSL_ENABLE_DEBUGGING
    // Assert that dot(N, L) lighting coefficient is a valid value
    // Use /Xassert or D3DXSHADEREX_ENABLE_ASSERTIONS to enable runtime asserts
    assert(Input.NdotL <= 1.0f);
#endif

    return ((Input.NdotL + 0.3f) * DiffuseTexture);
}

//--------------------------------------------------------------------------------------
// ComputeNormals.fx
//
// Calculates a normal map from a given heightmap
// Stores alpha = height... rgb = normal
//
// Microsoft Game Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

// Heightmap sampler
sampler heightmap_sampler  : register (s1) = sampler_state
{
    MipFilter = POINT;
    MinFilter = POINT;
    MagFilter = POINT;
    
    AddressU = WRAP;
    AddressV = WRAP;
};                                

uniform float tap_width = 1.f/256.f;
uniform float3 scalings = float3( 50.f, 0.25f, 50.f );
                                                                      
//--------------------------------------------------------------------------------------
// Vertex Shader Output
//--------------------------------------------------------------------------------------
struct VS_OUTPUT
{
    float4 Pos     : POSITION;
    float2 UV      : TEXCOORD0;
};


//--------------------------------------------------------------------------------------
// Vertex Shader
//--------------------------------------------------------------------------------------
VS_OUTPUT VS(
    float2 Pos   : POSITION,
    float2 UV    : TEXCOORD0 )
{
    VS_OUTPUT Out = (VS_OUTPUT)0;
    
    // Pass through position (pre-transformed into clip space)
    Out.Pos = float4( Pos.x, Pos.y, 0.f, 1.f );

    // Pass through texture coordinates
    Out.UV = UV;
    
    return Out;
}


//--------------------------------------------------------------------------------------
// Pixel Shader
//--------------------------------------------------------------------------------------
float4 PS( float2 UV : TEXCOORD0 ) : COLOR
{
    // Sample the Height map
    float dx = tap_width * scalings.x;
    float dz = tap_width * scalings.z;
    
    float height1 = tex2D( heightmap_sampler, UV ).r;
    UV += float2( tap_width, 0 );
    float height2 = tex2D( heightmap_sampler, UV ).r;
    UV += float2( -tap_width, tap_width );
    float height3 = tex2D( heightmap_sampler, UV ).r;

    // calculate height differential    
    float dyx = scalings.y * (height2-height1);
    float dyz = scalings.y * (height3-height1);
    
    // compute normal
    float3 utNormal = float3( -dyx/dx, 1.f, -dyz/dz );
    float3 Normal = normalize( utNormal );
    
    return float4( Normal, height1 );
}


//--------------------------------------------------------------------------------------
// Default Technique
// Establishes Vertex and Pixel Shader
//--------------------------------------------------------------------------------------
technique T0
{
    pass P0
    {
        // shaders
        VertexShader = compile vs_2_0 VS();
        PixelShader  = compile ps_2_0 PS();

        HalfPixelOffset  = TRUE;
        ZEnable          = FALSE;
        ZWriteEnable     = FALSE;
    }  
}

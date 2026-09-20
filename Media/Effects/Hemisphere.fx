//-----------------------------------------------------------------------------
// File: Hemisphere.fx
//
// Desc: Effects for Hemisphere Lighting
//       These effects show hemi sphere lighting model.
// 
// Note: This effect file works with EffectEdit.
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
// Constants for EffectEdit
//-----------------------------------------------------------------------------
string XFile = "SkullOcc.x";                // model
int    BCLR  = 0xff202080;                  // background


//-----------------------------------------------------------------------------
// Global constants
//-----------------------------------------------------------------------------

// light directions ( view space )
float3 DirFromLight < string UIDirectional = "Light Direction"; > = {0.577, -0.577, 0.577};

// direction of light from sky ( view space )
float3 DirFromSky < string UIDirectional = "Direction from Sky"; > = { 0.0f, -1.0f, 0.0f };            

// light intensity
float4 I_a = { 0.5f, 0.5f, 0.5f, 1.0f };    // ambient
float4 I_b = { 0.1f, 0.0f, 0.0f, 1.0f };    // ground
float4 I_c = { 0.9f, 0.9f, 1.0f, 1.0f };    // sky
float4 I_d = { 1.0f, 0.9f, 0.8f, 1.0f };    // diffuse
float4 I_s = { 1.0f, 1.0f, 1.0f, 1.0f };    // specular

// material reflectivity
float4 k_a = { 0.8f, 0.8f, 0.8f, 1.0f };    // ambient
float4 k_d = { 0.4f, 0.4f, 0.4f, 1.0f };    // diffuse
float4 k_s = { 0.1f, 0.1f, 0.1f, 1.0f };    // specular
float  n   = 32.0f;                         // power

// transformations
float4x3 WorldView  : WORLDVIEW;
float4x4 Projection : PROJECTION;


//-----------------------------------------------------------------------------
// Vertex shaders
//-----------------------------------------------------------------------------
struct VS_OUTPUT
{
    float4 Pos  : POSITION;
    float4 Diff : COLOR0;
    float4 Spec : COLOR1;
};


//-----------------------------------------------------------------------------
// Name: VS
// Desc: Vertex shader for hemi sphere lighting
//-----------------------------------------------------------------------------
VS_OUTPUT VS( 
    float3 Pos  : POSITION, 
    float3 Norm : NORMAL, 
    float  Occ  : TEXCOORD0,
    uniform bool bHemi, 
    uniform bool bDiff,
    uniform bool bSpec )
{
    VS_OUTPUT Out = ( VS_OUTPUT )0;

    float3 L = -DirFromLight;                                   // diffuse direction
    float3 Y = -DirFromSky;                                     // hemisphere up axis
    float3 P = mul( float4( Pos, 1 ), ( float4x3 )WorldView );  // position ( view space )
    float3 N = normalize( mul( Norm, ( float3x3 )WorldView ) ); // normal ( view space )
    float3 R = normalize( 2 * dot( N, L ) * N - L );            // reflection vector ( view space )
    float3 V = -normalize( P );                                 // view direction ( view space )

    float4 Amb  = k_a * I_a;
    float4 Hemi = k_a * lerp( I_b, I_c, ( dot( N, Y ) + 1 ) / 2 ) * ( 1 - Occ );
    float  temp = 1 - max( 0, dot( N, L ) );
    float4 Diff = k_d * I_d *  ( 1 - temp * temp );
    float4 Spec = k_s * I_s * pow( max( 0, dot( R, V ) ), n/4 );
    float4 Zero = 0;

    Out.Pos  = mul( float4( P, 1 ), Projection );               // position ( projected )
    Out.Diff = ( bDiff ? Diff : 0 )
             + ( bHemi ? Hemi : Amb );                          // diffuse + ambient/hemisphere
    Out.Spec = ( bSpec ? Spec : 0 );                            // specular

    return Out;
}


//-----------------------------------------------------------------------------
// Pixel shaders
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Name: PS
// Desc: Vertex shader for hemi sphere lighting
//-----------------------------------------------------------------------------
float4 PS(
    VS_OUTPUT Input ) : COLOR
{
    return (float4)Input.Diff + Input.Spec;
}


//-----------------------------------------------------------------------------
// Techniques
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
// Name: THemisphereDiffuseSpecular
// Desc: Technique with Specular, Diffuse and Hemisphere
//-----------------------------------------------------------------------------
technique THemisphereDiffuseSpecular
{
    pass P0
    {
        VertexShader = compile vs_3_0 VS( true, true, true );
        PixelShader = compile ps_3_0 PS();
    }
}


//-----------------------------------------------------------------------------
// Name: THemisphereDiffuse
// Desc: Technique with Diffuse and Hemisphere
//-----------------------------------------------------------------------------
technique THemisphereDiffuse
{
    pass P0
    {
        VertexShader = compile vs_3_0 VS( true, true, false );
        PixelShader = compile ps_3_0 PS();
    }
}


//-----------------------------------------------------------------------------
// Name: THemisphere
// Desc: Technique with Hemisphere
//-----------------------------------------------------------------------------
technique THemisphere
{
    pass P0
    {
        VertexShader = compile vs_3_0 VS( true, false, false );
        PixelShader = compile ps_3_0 PS();
    }
}


//-----------------------------------------------------------------------------
// Name: TAmbientDiffuseSpecular
// Desc: Technique with Specular, Diffuse and Ambient
//-----------------------------------------------------------------------------
technique TAmbientDiffuseSpecular
{
    pass P0
    {
        VertexShader = compile vs_3_0 VS( false, true, true );
        PixelShader = compile ps_3_0 PS();
    }
}


//-----------------------------------------------------------------------------
// Name: TAmbientDiffuse
// Desc: Technique with Ambient and Diffuse
//-----------------------------------------------------------------------------
technique TAmbientDiffuse
{
    pass P0
    {
        VertexShader = compile vs_3_0 VS( false, true, false );
        PixelShader = compile ps_3_0 PS();
    }
}


//-----------------------------------------------------------------------------
// Name: TAmbient
// Desc: Technique with Ambient
//-----------------------------------------------------------------------------
technique TAmbient
{
    pass P0
    {
        VertexShader = compile vs_3_0 VS( false, false, false );
        PixelShader = compile ps_3_0 PS();
    }
}

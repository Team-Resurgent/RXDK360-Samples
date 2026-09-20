//-----------------------------------------------------------------------------
// File: HemisphereOrennayerLighting.fx
//
// Desc: Effects for Hemisphere Lighting & Oren-Nayer diffuse model
// 
// The algorithms described in this sample are based very closely on the 
// lighting model of O-N diffuse model described in the whitepaper
// "Generalization of Lambert's Reflectance Model".
//
// "Generalization of Lambert's Reflectance Model"
// Michael Oren and Shree K. Nayer
// http://www1.cs.columbia.edu/CAVE/publinks/oren_ACMS_1994.pdf
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
float3 DirFromLight < string UIDirectional = "Light Direction"; >
                        = {0.577, -0.577, 0.577};
float3 CameraPos < string UIDirectional = "Camera Pos"; >
                        = {0.f, 0.f, -16.f};

// direction of light from sky ( view space )
float3 DirFromSky < string UIDirectional = "Direction from Sky"; >
                        = { 0.0f, -1.0f, 0.0f };            

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

// Paremeters for O-N lighting
#define fRoughness (0.6f * 0.6F)
float A = 1.0f - 0.5f*(fRoughness / (fRoughness + 0.33f));
float B = 0.45f * (fRoughness / (fRoughness + 0.09f));


//-----------------------------------------------------------------------------
// Texture samplers
//-----------------------------------------------------------------------------
texture texSinTan;
sampler SinTanTex = sampler_state
{ 
    Texture = <texSinTan>;
    MipFilter = NONE; 
    MinFilter = LINEAR;
    MagFilter = LINEAR;

    AddressU = Clamp;
    AddressV = Clamp;
 
};


//-----------------------------------------------------------------------------
// Vertex shaders
//-----------------------------------------------------------------------------
struct VS_OUTPUT
{
    float4 Pos  : POSITION;
    float4 Amb : COLOR0;
    float3 Normal   : TEXCOORD0;
    float3 Eye  : TEXCOORD1;
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

    float3 Y = -DirFromSky;                                     // hemisphere up axis
    float3 P = mul( float4( Pos, 1 ), ( float4x3 )WorldView );  // position ( view space )
    float3 N = normalize( mul( Norm, ( float3x3 )WorldView ) ); // normal ( view space )
    float3 E  = CameraPos - Pos;

    // Calc Hemisphere factor
    float4 Hemi = k_a * lerp( I_b, I_c, ( dot( N, Y ) + 1 ) / 2 ) * ( 1 - Occ );

    Out.Pos  = mul( float4( P, 1 ), Projection );               // position ( projected )
    Out.Amb = ( bHemi ? Hemi : 0 );                             // diffuse + ambient/hemisphere
    Out.Normal = N;
    Out.Eye    = E;
    
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
    VS_OUTPUT Input,
    uniform bool bSpec,
    uniform bool bDiff,
    uniform bool bHemi
    ) : COLOR
{
    float3 L = -DirFromLight;                       // Light direction
    float3 N = normalize( Input.Normal );           // Normal
    float3 E = normalize( Input.Eye );              // Eye vector
    float  LN = dot( L, N );
    float  EN = dot( E, N );
    float3 R = normalize( 2 * LN * N - L );            // reflection vector ( view space )

    // Retrieve Sin( A ) * Tan ( B ) from texture
    // where A = max ( angle of incidence, angle of reflection )
    //       A = min ( angle of incidence, angle of reflection )
    float2 tcoord = { LN, max( 0, EN ) };
    float  SinTan = tex2D( SinTanTex, 0.5f * tcoord + 0.5f ).x;

    float3 al = normalize( L - LN * N );
    float3 ae = normalize( E - EN * N );
    float  C  = max( 0, dot( al, ae ) );

    float4 Spec = ( bSpec ? k_s * I_s * pow( max( 0, dot( E, R ) ), n/4 ) : 0 );
    float4 Diff = ( bDiff ? k_d * I_d * LN * ( A + B * C * SinTan ) : 0 );
    float4 Amb =  ( bHemi ? Input.Amb : k_a * I_a );

    return (float4)Amb + Diff + Spec;
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
        PixelShader = compile ps_3_0 PS( true, true, true );
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
        PixelShader = compile ps_3_0 PS( false, true, true);
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
        PixelShader = compile ps_3_0 PS( false, false, true );
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
        PixelShader = compile ps_3_0 PS( true, true, false );
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
        PixelShader = compile ps_3_0 PS( false, true, false );
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
        PixelShader = compile ps_3_0 PS( false, false, false );
    }
}


#if 0
// Runtime does not call initialization func for texture.
//-----------------------------------------------------------------------------
// Name: GenerateSinTanTable
// Desc: Function used to fill the Sin Tan table texture
//-----------------------------------------------------------------------------
float4 GenerateSinTanTable(float2 Pos : POSITION) : COLOR
{
    float min = 2.0f*( (Pos.x < Pos.y) ? Pos.x : Pos.y)-1.0f;
    float max = 2.0f*( (Pos.x < Pos.y) ? Pos.y : Pos.x )-1.0f;
    float4 Out = sin( acos( min ) ) * tan( acos( max ) );

    return Out;
}

//-----------------------------------------------------------------------------
// Name: SinTanTable
// Desc: procedural texture that contains a Sin Tan table
//-----------------------------------------------------------------------------
texture SinTanTable 
< 
    string function = "GenerateSinTanTable"; 
    int width = 256;
    int height = 256;
>;
#endif

//--------------------------------------------------------------------------------------
// CommonShaders.hlsl
//
// Provides a framework for accessing common, simple shaders across the samples
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

struct VSIN
{
    float4 Position         : POSITION;
    float4 Diffuse          : COLOR0;
    float2 TexCoord0        : TEXCOORD0;
};

struct VSOUT
{
    float4 Position         : POSITION;
    float4 Diffuse          : COLOR0;
    float2 TexCoord0        : TEXCOORD0;
};

struct VSLITOUT
{
    float4 Position         : POSITION;
    float2 UV               : TEXCOORD0;
    float4 Normal           : TEXCOORD1;
};

// Common Vertex Shader Constants
uniform extern matrix g_matWorldViewProj : register( c0 );
uniform extern matrix g_matWorldRotation : register( c4 );

// Common Pixel Shader Constants
uniform extern float4  g_vConstantColor  : register( c0 );
uniform extern float4  g_vLightColor     : register( c1 );
uniform extern float4  g_vLightDirection : register( c2 );

// Common Sampler Constants
uniform extern sampler g_TextureSampler : register( s0 );


//--------------------------------------------------------------------------------------
// Name: ProcessVertex()
// Desc: Common vertex shader
//--------------------------------------------------------------------------------------
VSOUT ProcessVertex( VSIN Input, bool bTransform, bool bCopyDiffuse, 
                     bool bCopyTexcoord )
{
    VSOUT Output = (VSOUT)0;
    
    if( bTransform )
        Output.Position = mul( Input.Position, g_matWorldViewProj );
    else
        Output.Position = Input.Position;
    
    if( bCopyDiffuse )
        Output.Diffuse = Input.Diffuse;
    
    if( bCopyTexcoord )
        Output.TexCoord0 = Input.TexCoord0;
    
    return Output;
}

//--------------------------------------------------------------------------------------
// Name: vs_TransformLitVertices()
// Desc: Common vertex shader to transform simple textured vertices for per-pixel 
//       lighting.
//--------------------------------------------------------------------------------------
VSLITOUT vs_TransformLitVertices( float4 Position : POSITION, float4 Normal : NORMAL, float2 UV : TEXCOORD0 )
{
    VSLITOUT Output;

    Output.Position = mul( Position, g_matWorldViewProj );
    Output.Normal   = mul( Normal,   g_matWorldRotation );
    Output.UV       = UV;

    return Output;
}

//--------------------------------------------------------------------------------------
// Name: ps_SimplePerPixelLighting()
// Desc: Common pixel shader to perform simple per-pixel lighting on a textured mesh.
//--------------------------------------------------------------------------------------
float4 ps_SimplePerPixelLighting( VSLITOUT In ) : COLOR
{
    float4 fAlbedo = tex2D( g_TextureSampler, In.UV );
    return dot( -g_vLightDirection, normalize( In.Normal ) ) * g_vLightColor * fAlbedo;
};

static const bool YesTransform = true;
static const bool NoTransform  = false;
static const bool YesDiffuse   = true;
static const bool NoDiffuse    = false;
static const bool YesTexcoord  = true;
static const bool NoTexcoord   = false;


//--------------------------------------------------------------------------------------
// Shader entry points for various configurations of shaders
//--------------------------------------------------------------------------------------
VSOUT PositionVS( VSIN Input )
{
    return ProcessVertex( Input, YesTransform, NoDiffuse, NoTexcoord );
}


VSOUT PositionTexcoordVS( VSIN Input )
{
    return ProcessVertex( Input, YesTransform, NoDiffuse, YesTexcoord );
}


VSOUT PositionDiffuseVS( VSIN Input )
{
    return ProcessVertex( Input, YesTransform, YesDiffuse, NoTexcoord );
}


VSOUT PositionDiffuseTexcoordVS( VSIN Input )
{
    return ProcessVertex( Input, YesTransform, YesDiffuse, YesTexcoord );
}


VSOUT ScreenspaceVS( VSIN Input )
{
    return ProcessVertex( Input, NoTransform, NoDiffuse, NoTexcoord );
}


VSOUT ScreenspaceTexcoordVS( VSIN Input )
{
    return ProcessVertex( Input, NoTransform, NoDiffuse, YesTexcoord );
}


VSOUT ScreenspaceDiffuseTexcoordVS( VSIN Input )
{
    return ProcessVertex( Input, NoTransform, YesDiffuse, YesTexcoord );
}


VSOUT ScreenspaceDiffuseVS( VSIN Input )
{
    return ProcessVertex( Input, NoTransform, YesDiffuse, NoTexcoord );
}


//--------------------------------------------------------------------------------------
// Name: TextureModDiffusePS()
// Desc: Returns the texture modulated with the diffuse
//--------------------------------------------------------------------------------------
float4 TextureModDiffusePS( float4 Diffuse   : COLOR0,
                            float2 TexCoord0 : TEXCOORD0 ) : COLOR
{
    float4 TexelColor = tex2D( g_TextureSampler, TexCoord0 );
    
    return TexelColor * Diffuse;
}

//--------------------------------------------------------------------------------------
// Name: TextureModConstantPS()
// Desc: Returns the texture modulated with the constant color
//--------------------------------------------------------------------------------------
float4 TextureModConstantPS( [unused] float4 Diffuse   : COLOR0,
                             float2 TexCoord0 : TEXCOORD0 ) : COLOR
{
    float4 TexelColor = tex2D( g_TextureSampler, TexCoord0 );
    
    return TexelColor * g_vConstantColor;
}


//--------------------------------------------------------------------------------------
// Name: ConstantColorPS()
// Desc: Just passes through the color specified via shader constant
//--------------------------------------------------------------------------------------
float4 ConstantColorPS() : COLOR
{
    return g_vConstantColor;
}


//--------------------------------------------------------------------------------------
// Name: TexturePS()
// Desc: Returns the result of texture lookup
//--------------------------------------------------------------------------------------
float4 TexturePS( float2 TexCoord0 : TEXCOORD0 ) : COLOR
{
    return tex2D( g_TextureSampler, TexCoord0 );
}


//--------------------------------------------------------------------------------------
// Name: DiffusePS()
// Desc: Returns the diffuse
//--------------------------------------------------------------------------------------
float4 DiffusePS( float4 Diffuse : COLOR0 ) : COLOR
{
    return Diffuse;
}


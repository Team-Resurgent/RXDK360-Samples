//-----------------------------------------------------------------------------
// Vertex shader for the Gamepad
//-----------------------------------------------------------------------------

struct VSIN
{
    float3 Position         : POSITION;
    float3 Normal           : NORMAL;
    float2 TexCoord         : TEXCOORD;
};

struct VSOUT
{
    float4 Position         : POSITION;
    float4 Diffuse          : COLOR0;
    float4 Specular         : COLOR1;
    float2 BaseTexCoord     : TEXCOORD0;
    float2 EnvTexCoord      : TEXCOORD1;
};


//--------------------------------------------------------------------------------------
// Vertex shader constants
//--------------------------------------------------------------------------------------
uniform float4x4 g_matWorldViewProj  : register(c0);
uniform float4x4 g_matWorldView      : register(c4);
uniform float4   g_vMtrlDiffuse      : register(c8);
uniform float4   g_vMtrlSpecular     : register(c9);


//--------------------------------------------------------------------------------------
// Pixel shader constants
//--------------------------------------------------------------------------------------
uniform extern sampler g_GamepadTexture : register( s0 );
uniform extern sampler g_EnvMapTexture  : register( s1 );


//--------------------------------------------------------------------------------------
// Name: GamepadVS()
// Desc: Vertex shader for the gamepad mesh
//--------------------------------------------------------------------------------------
VSOUT GamepadVS( VSIN Input )
{
    VSOUT Output;

    // Transform position to the clipping space
    Output.Position = mul( float4( Input.Position, 1 ), g_matWorldViewProj );
    
    float4 g_vGlobalAmbient  = float4( 0.2f, 0.2f, 0.2f, 0.0f );
    float3 g_vLightAmbient   = float4( 0.3f, 0.3f, 0.3f, 1.0f );
    float3 g_vLightDiffuse   = float4( 1.0f, 1.0f, 1.0f, 1.0f );
    float3 g_vLightDirection = float4( 0.3f,-1.0f, 1.0f, 0.0f );
    
    float3 vNormal  = mul( float4( Input.Normal, 0 ), g_matWorldView );
    float3 vLight   = g_vLightAmbient + g_vLightDiffuse * max( 0, dot( vNormal, -g_vLightDirection ) );
    Output.Diffuse  = float4( g_vGlobalAmbient.rgb + vLight * g_vMtrlDiffuse.rgb, g_vMtrlDiffuse.a );

    Output.Specular = g_vMtrlSpecular;
    
    Output.BaseTexCoord = Input.TexCoord;
    
    // Transform camera-space normal to texture coordinates
//  vNormal = mul( float4( Input.Normal, 0 ), g_matWorldView );
    Output.EnvTexCoord = vNormal.xy * float2( 0.5f, -0.5f ) + float2( 0.5f, 0.5f );

    return Output;
}


//--------------------------------------------------------------------------------------
// Name: GamepadPS()
// Desc: Pixel shader for the gamepad mesh
//--------------------------------------------------------------------------------------
float4 GamepadPS( float4 vDiffuse      : COLOR0,
                  float4 vSpecular     : COLOR1,
//                float2 vBaseTexCoord : TEXCOORD0,
                  float2 vEnvTexCoord  : TEXCOORD1 ) : COLOR
{
//  vDiffuse  *= tex2D( g_GamepadTexture, vBaseTexCoord );
    vSpecular *= float4( tex2D( g_EnvMapTexture, vEnvTexCoord ).rgb, 0 );
    return vDiffuse + vSpecular;
}


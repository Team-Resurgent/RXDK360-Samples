//------------------------------------------------------------------------------
// Vertex shader to pass thru vertex components
//------------------------------------------------------------------------------


struct VSOUT
{
    float4 Position         : POSITION;
    float2 TexCoord0        : TEXCOORD0;    
    float3 TexCoord1        : TEXCOORD1;    
};


uniform float3   LightDirection  : register(c1);  // Light direction
uniform float4x4 WorldViewProj   : register(c4);  // World-view-projection matrix


VSOUT TriStripVS( const float3 Position  : POSITION,
                  const float3 Normal    : NORMAL,
                  const float2 TexCoord0 : TEXCOORD0 )
{
    VSOUT  Output;

    // Transform the vertex
    Output.Position = mul( float4(Position, 1.0f), WorldViewProj );

    Output.TexCoord0 = TexCoord0;
    Output.TexCoord1 = Normal;

    return Output;
}


sampler2D ColorMap : register(s0);
samplerCUBE EnvironmentMap : register(s1);


float4 TriStripPS( const float2 TexCoord0 : TEXCOORD0,
                  const float3 TexCoord1 : TEXCOORD1 ) : COLOR
{
    float4 Color = tex2D( ColorMap, TexCoord0 );
    float4 Environ = texCUBE( EnvironmentMap, TexCoord1 );

    // Modulate alpha, add color.
    Color.a = Color.a * Environ.a;
    Color.rgb = Color.rgb + Environ.rgb;

    return Color;
}

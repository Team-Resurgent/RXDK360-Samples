//-----------------------------------------------------------------------------
// Shader for the Pointsprite sample
//-----------------------------------------------------------------------------

struct VSOUT
{
    float4 Position         : POSITION;
    float4 Diffuse          : COLOR0;
    float  PSize            : PSIZE;
};


//-----------------------------------------------------------------------------
// Vertex shader constants
//-----------------------------------------------------------------------------
uniform float4x4 WorldViewProj  : register(c0);  // World-view-projection matrix
uniform float    Opacity        : register(c4);  // Opacity

sampler g_TextureSampler : register(s0);


//-----------------------------------------------------------------------------
// Name: PointSpriteVS()
// Desc: Vertex shader for the particles and lights of the particle system
//-----------------------------------------------------------------------------
VSOUT PointSpriteVS( const float3 Position  : POSITION,
                     const float4 Diffuse   : COLOR0 )
{
    VSOUT  Output;
    Output.Position    = mul( float4(Position, 1.0f), WorldViewProj );
    Output.Diffuse     = Diffuse * Opacity;
    Output.PSize       = 48.0f / Output.Position.z;
    return Output;
}


//-----------------------------------------------------------------------------
// Name: PointSpritePS()
// Desc: Pixel shader for the particles and lights of the particle system
//-----------------------------------------------------------------------------
float4 PointSpritePS( const float2 TexCoord0 : SPRITETEXCOORD,
                      const float4 Diffuse   : COLOR0 ) : COLOR
{
    return Diffuse * tex2D( g_TextureSampler, TexCoord0 );
}

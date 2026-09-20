//------------------------------------------------------------------------------
// Vertex shader to pass thru vertex components
//------------------------------------------------------------------------------


struct VSOUT
{
    float4 Position         : POSITION;
    float4 Diffuse          : COLOR0;
    float2 TexCoord0        : TEXCOORD0;    
};


uniform float3   LightDirection  : register(c1);  // Light direction
uniform float4x4 WorldViewProj   : register(c4);  // World-view-projection matrix


VSOUT main( const float3 Position  : POSITION,
            const float4 Diffuse   : COLOR0,
            const float2 TexCoord0 : TEXCOORD0 )
{
    VSOUT  Output;

    // Transform the vertex
    Output.Position = mul( float4(Position, 1.0f), WorldViewProj );

    Output.Diffuse   = Diffuse;
    Output.TexCoord0 = TexCoord0;

    return Output;
}










//------------------------------------------------------------------------------
// Vertex shader to pass thru vertex components
//------------------------------------------------------------------------------


struct VSOUT
{
    float4 Position         : POSITION;
    float4 Diffuse          : COLOR0;
    float2 TexCoord0        : TEXCOORD0;    
};


VSOUT main( const float4 Position  : POSITION,
            const float4 Diffuse   : COLOR0,
            const float2 TexCoord0 : TEXCOORD0 )
{
    VSOUT  Output;

    Output.Position  = Position;
    Output.Diffuse   = Diffuse;
    Output.TexCoord0 = TexCoord0;

    return Output;
}










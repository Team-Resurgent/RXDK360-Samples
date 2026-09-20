const float4x4 c_modelview    : register( c0 );
const float4x4 c_projection   : register( c4 );

struct VS_IN
{
    float4      position        : POSITION;         // stream 0
    float2      uv0             : TEXCOORD0;        // stream 0
};

struct VS_OUT
{
    float4 projPos      : POSITION;
    float2 uv0          : TEXCOORD0;
};

VS_OUT main( VS_IN input )
{
    VS_OUT output;
    
    float4 outpos = mul(c_modelview, input.position);
    output.projPos = mul(c_projection, outpos);
    output.uv0   = input.uv0;

    return output;
}

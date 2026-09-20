
#include "skinning.hlsl"

struct VS_IN
{
    Vertex_c vertex;                    // stream 0
    float2 uv1 : TEXCOORD1;
    int1 hlIdx : TESSFACTOR;
};

struct VS_OUT
{
    float4 projPos      : POSITION;
    float4 color        : COLOR;
    float2 uv0          : TEXCOORD0;
    float2 uv1          : TEXCOORD1;
    float3 viewNormal	: TEXCOORD2;
    float3 viewPosition	: TEXCOORD3;    
    float  hlAmount     : COLOR1;
};

VS_OUT main( VS_IN input )
{
    VS_OUT output;
    
    ApplySkinning( input.vertex, output.viewPosition, output.projPos, output.viewNormal );
    
    // pass on colour and UV
    output.color = input.vertex.color;
    output.uv0   = input.vertex.uv0;
    output.uv1   = input.uv1;
    output.hlAmount = GetHighlightAmount( input.hlIdx.x );
    
    return output;
}


#include "skinning.hlsl"

struct VS_IN
{
    Vertex_c vertex;                    // stream 0
    float2   uv1        : TEXCOORD1;	// stream 0
    float2   uv2        : TEXCOORD2;	// stream 0
    float2   uv3        : TEXCOORD3;	// stream 0
    float2   uv4        : TEXCOORD4;	// stream 0
    float2   uv5        : TEXCOORD5;	// stream 0
    int1     hlIdx      : TESSFACTOR;
};

struct VS_OUT
{
    float4 projPos		: POSITION;
    float4 color		: COLOR;
    float2 uv0			: TEXCOORD0;
    float2 uv1			: TEXCOORD1;
    float2 uv2			: TEXCOORD2;
    float2 uv3			: TEXCOORD3;
    float2 uv4			: TEXCOORD4;
    float2 uv5			: TEXCOORD5;
    float3 viewNormal	: TEXCOORD6;
    float3 viewPosition	: TEXCOORD7;        
    float  hlAmount     : COLOR1;  
};

VS_OUT main( VS_IN input )
{
    VS_OUT output;
    
    ApplySkinning( input.vertex, output.viewPosition, output.projPos, output.viewNormal );

    output.color    = input.vertex.color;
    output.uv0      = input.vertex.uv0;
    output.uv1      = input.uv1;
    output.uv2      = input.uv2;
    output.uv3      = input.uv3;
    output.uv4      = input.uv4;
    output.uv5      = input.uv5;
    output.hlAmount = GetHighlightAmount( input.hlIdx.x );
    
    return output;
}

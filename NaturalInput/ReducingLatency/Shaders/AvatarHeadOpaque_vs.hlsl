// We're using the shader attribute [optimizeAutoZ(true)], but by default this is switched
// off and therefore generates a shader compiler warning
#pragma warning( disable : 3608 )


#include "AvatarSkinning.hlsl"
#include "AvatarCommon.hlsl"

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
    float4 uv0			: TEXCOORD0;
    float2 uv1			: TEXCOORD1;
    float2 uv2			: TEXCOORD2;
    float2 uv3			: TEXCOORD3;
    float2 uv4			: TEXCOORD4;
    float2 uv5			: TEXCOORD5;
    float3 viewNormal	: TEXCOORD6;
    float2 hlAmountFog  : COLOR1;  
};

[optimizeAutoZ(true)]
VS_OUT main( VS_IN input )
{
    VS_OUT output;
    
    float3 vSkinnedPos;
    float3 vViewPosition;
    
    ApplySkinning( input.vertex, vSkinnedPos, vViewPosition, output.projPos, output.viewNormal );

    // Generate water caustic tex coords from vertex xz position
    float2 vCausticTexCoords = 0.003 * vSkinnedPos.xz;

    // Fog calculation
    float fFogValue = clamp( (g_vFogRange.y - vViewPosition.z) * g_vFogRange.z, 0.0f, 1.0f );

    output.color    = input.vertex.color;
    output.uv0.xy	= input.vertex.uv0;
    output.uv0.zw	= vCausticTexCoords;
    output.uv1      = input.uv1;
    output.uv2      = input.uv2;
    output.uv3      = input.uv3;
    output.uv4      = input.uv4;
    output.uv5      = input.uv5;
    output.hlAmountFog.x = GetHighlightAmount( input.hlIdx.x );
    output.hlAmountFog.y = fFogValue;
    
    return output;
}

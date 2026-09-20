// We're using the shader attribute [optimizeAutoZ(true)], but by default this is switched
// off and therefore generates a shader compiler warning
#pragma warning( disable : 3608 )


#include "AvatarSkinning.hlsl"
#include "AvatarCommon.hlsl"

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
    float4 uv0          : TEXCOORD0;
    float2 uv1          : TEXCOORD1;
    float3 viewNormal	: TEXCOORD2;
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
    
    // pass on colour and UV
    output.color = input.vertex.color;
    output.uv0.xy	= input.vertex.uv0;
    output.uv0.zw	= vCausticTexCoords;
    output.uv1   = input.uv1;
    output.hlAmountFog.x = GetHighlightAmount( input.hlIdx.x );
    output.hlAmountFog.y = fFogValue;
    
    return output;
}

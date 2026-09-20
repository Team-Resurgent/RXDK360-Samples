#include "AvatarLighting.hlsl"
#include "AvatarCommon.hlsl"

struct PS_IN
{
    float4 color                : COLOR;
    float4 uv0                  : TEXCOORD0;
    float2 uv1                  : TEXCOORD1;
    float2 uv2                  : TEXCOORD2;
    float3 viewNormal	        : TEXCOORD3;
    float2 highlightAmount      : COLOR1;
    float2 screenCoord          : VPOS;  
};

sampler2D     s_colourTex       : register(s0);
sampler2D     s_intensityTex    : register(s1);
sampler2D     s_decalTex        : register(s2);

float4 main( PS_IN input ) : COLOR
{
	float2 coord = input.uv0.xy;
    float2 vCausticTexCoords = input.uv0.zw;
    
	float4 result = tex2D( s_colourTex, coord );
	float4 causticColor = tex2D( s_causticTex, vCausticTexCoords );
	
	float fFogValue = input.highlightAmount.y;
	result.rgb = IntensityMap(result.rgb, tex2D( s_intensityTex, input.uv1 ));
	
	float4 decal = tex2D( s_decalTex, input.uv2 );
	result.rgb = lerp(result.rgb, decal.rgb, decal.a);
	
    result *= input.color;
    
    return ApplyLighting( result, causticColor, normalize( input.viewNormal ), fFogValue );
}

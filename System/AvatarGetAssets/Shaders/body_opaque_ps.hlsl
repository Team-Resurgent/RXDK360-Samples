#include "lighting.hlsl"
#include "common.hlsl"

struct PS_IN
{
    float4 color                : COLOR;
    float2 uv0                  : TEXCOORD0;
    float2 uv1                  : TEXCOORD1;
    float2 uv2                  : TEXCOORD2;
    float3 viewNormal	        : TEXCOORD3;
    float3 viewPosition	        : TEXCOORD4;
    float2 screenCoord          : VPOS;  
};

sampler2D     s_colourTex       : register(s0);
sampler2D     s_intensityTex    : register(s1);
sampler2D     s_decalTex        : register(s2);

float4 main( PS_IN input ) : COLOR
{
	float4 result = tex2D( s_colourTex, input.uv0 );
	
	result.rgb = IntensityMap(result.rgb, tex2D( s_intensityTex, input.uv1 ));
	
	float4 decal = tex2D( s_decalTex, input.uv2 );
	result.rgb = lerp(result.rgb, decal.rgb, decal.a);
	
    result *= input.color;
    result.a = round(result.a);
    return ApplyLighting( 
                result, normalize( input.viewNormal ), 
                input.viewPosition, 0, 0, input.screenCoord );
}


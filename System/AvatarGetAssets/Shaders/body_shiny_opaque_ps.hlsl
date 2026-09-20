#include "lighting.hlsl"
#include "common.hlsl"

struct PS_IN
{
    float4 color		        : COLOR;
    float2 uv0                  : TEXCOORD0;
    float2 uv1                  : TEXCOORD1;
    float3 viewNormal	        : TEXCOORD2;
    float3 viewPosition	        : TEXCOORD3;
    float2 screenCoord          : VPOS;  
};

sampler2D     s_colourTex    : register(s0);
sampler2D     s_intensityTex : register(s1);
sampler2D     s_envmap       : register(s2);

float4 main( PS_IN input ) : COLOR
{
	float2 coord = input.uv0;
	float4 result = tex2D( s_colourTex, coord );    
    
    result.rgb = IntensityMap(result.rgb, tex2D( s_intensityTex, input.uv1 ));
	
	result *= input.color;
	
    float3 normal = normalize( input.viewNormal );
    float3 env = EnvironmentMap( s_envmap, normal );
    result.a = round(result.a);
    return ApplyLighting( 
				result, normal, input.viewPosition,
				env, Reflectivity.x, input.screenCoord );
}


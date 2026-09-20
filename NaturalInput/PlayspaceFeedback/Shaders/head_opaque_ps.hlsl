#include "lighting.hlsl"
#include "common.hlsl"

struct PS_IN
{
    float4 color		        : COLOR;
    float2 uv0			        : TEXCOORD0;
    float2 uv1			        : TEXCOORD1;
    float2 uv2			        : TEXCOORD2;
    float2 uv3			        : TEXCOORD3;
    float2 uv4			        : TEXCOORD4;
    float2 uv5			        : TEXCOORD5;
    float3 viewNormal	        : TEXCOORD6;
    float3 viewPosition	        : TEXCOORD7;  
    float2 screenCoord          : VPOS;  
};

sampler2D     texSkinFeatures   : register(s0);
sampler2D     texFacialHair     : register(s1);
sampler2D     texEyeBrow        : register(s2);
sampler2D     texEye            : register(s3);
sampler2D     texMouth          : register(s4);
sampler2D     texEyeShadow      : register(s5);

const float4  s_colorFilter  : register(c6);

float4 main( PS_IN input ) : COLOR
{	
	float4 faceColour = FaceShader(
	                        tex2D( texSkinFeatures, input.uv0 ),
	                        tex2D( texFacialHair, input.uv1 ),
	                        tex2D( texEyeBrow, input.uv2 ),
	                        tex2D( texEye, input.uv3 ),
	                        tex2D( texMouth, input.uv4 ),
	                        tex2D( texEyeShadow, input.uv5 ) );
    faceColour *= input.color;     
	faceColour.a = 1;

	return s_colorFilter * ApplyLighting( faceColour, normalize( input.viewNormal ), input.viewPosition, 0, 0,
								  input.screenCoord );
}


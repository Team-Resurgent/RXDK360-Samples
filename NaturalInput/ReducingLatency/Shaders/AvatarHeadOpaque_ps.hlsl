#include "AvatarLighting.hlsl"
#include "AvatarCommon.hlsl"

struct PS_IN
{
    float4 color		        : COLOR;
    float4 uv0			        : TEXCOORD0;
    float2 uv1			        : TEXCOORD1;
    float2 uv2			        : TEXCOORD2;
    float2 uv3			        : TEXCOORD3;
    float2 uv4			        : TEXCOORD4;
    float2 uv5			        : TEXCOORD5;
    float3 viewNormal	        : TEXCOORD6;
    float2 highlightAmount      : COLOR1;
    float2 screenCoord          : VPOS;  
};

sampler2D     texSkinFeatures   : register(s0);
sampler2D     texFacialHair     : register(s1);
sampler2D     texEyeBrow        : register(s2);
sampler2D     texEye            : register(s3);
sampler2D     texMouth          : register(s4);
sampler2D     texEyeShadow      : register(s5);

float4 main( PS_IN input ) : COLOR
{	
	float2 coord = input.uv0.xy;
    float2 vCausticTexCoords = input.uv0.zw;

    float4 causticColor = tex2D( s_causticTex, vCausticTexCoords );

	float4 faceColour = FaceShader(
	                        tex2D( texSkinFeatures, coord ),
	                        tex2D( texFacialHair, input.uv1 ),
	                        tex2D( texEyeBrow, input.uv2 ),
	                        tex2D( texEye, input.uv3 ),
	                        tex2D( texMouth, input.uv4 ),
	                        tex2D( texEyeShadow, input.uv5 ) );
    faceColour *= input.color;     
	faceColour.a = 1;

    float fFogValue = input.highlightAmount.y;

    return ApplyLighting( faceColour, causticColor, normalize( input.viewNormal ), fFogValue );
}


//--------------------------------------------------------------------------------------
// Shader for the Conquistador model
//--------------------------------------------------------------------------------------

/////////////////////////////////////////////////////////////////////////
// Material Properties
////////////////////////////////////////////////////////////////////////
float3 diffuseColorBounceLight      : register (c12);
float3 diffuseColorLow              : register (c13);
float3 diffuseColorMid              : register (c14);
float3 diffuseColorHigh             : register (c15);
float  diffuseColorRampMidpoint     : register (c16);
float  diffuseColorBounceLightPoint : register (c17);
float  diffuseScaler                : register (c18);

float3 specularColor                : register (c19);
float  specularPower                : register (c20);
float  specularScaler               : register (c21);

float  rimExponent                  : register (c22);
float3 rimColorSky                  : register (c23);
float3 rimColor                     : register (c24);

float  tonemapAmount                : register (c25);
float  brightnessControl            : register (c26);
float  exposure                     : register (c27);

float  normalIntensity              : register (c28);
float  AOAmount                     : register (c29);

float4 lightDir                     : register (c30);
float3 lightColor                   : register (c31);

bool   useNormalMap                 : register (b1);
bool   useDiffuseMap                : register (b2);
bool   useColorRampDiffuse          : register (b3);
bool   useAO                        : register (b4);
bool   useRimLightSky               : register (b5);
bool   useRimLight                  : register (b6);

////////////////////////////////////////////////////////////////////////////////////


// Pixel shader texture samplers for surface materials.
#define TRILINEAR_SAMPLER sampler_state { MipFilter = LINEAR; MinFilter = LINEAR; MagFilter = LINEAR; AddressU = WRAP; AddressV = WRAP; }
sampler2D   diffuse_texture     : register(s0) = TRILINEAR_SAMPLER;
sampler2D   normal_map_texture  : register(s1) = TRILINEAR_SAMPLER;
sampler2D   ao_texture          : register(s2) = TRILINEAR_SAMPLER;

struct PS_IN
{
    float2 UV               : TEXCOORD0;
    float3 Normal           : TEXCOORD1;
    float3 Tangent          : TEXCOORD2;
    float3 Binormal         : TEXCOORD3;
    float3 ViewDir          : TEXCOORD4;
};

float3 RecoverXYZFromNormalMapSample( float2 NormalMapSample )
{
    // Expand normal map sample to -1..1 range.
    NormalMapSample = NormalMapSample * 2.0 - 1.0;
    float3 result = float3( NormalMapSample.x, NormalMapSample.y, saturate( 1 - dot( NormalMapSample, NormalMapSample ) ) );
    return result;
}

float3 BumpNormalToObjectSpace( float3 Normal, float3 Binormal, float3 Tangent, float3 SampledNormal )
{
    float3 obj_binormal = normalize( Binormal );
    float3 obj_tangent = normalize( Tangent );
    float3 obj_normal = normalize( Normal );
    float3 obj_bumpnormal = obj_tangent * SampledNormal.x +
                            obj_binormal * SampledNormal.y +
                            obj_normal * SampledNormal.z;
                            
    return normalize( obj_bumpnormal );
}

float3 Diffuse( float3 N, float3 L )
{
    float diffuse = dot(N,L);
    diffuse = max(-1, diffuse );
    return float3( diffuse, diffuse, diffuse );
}

float3 Specular ( float3 N, float3 L, float3 V, float specularPower)
{
  float3 H = normalize(L + V);
  float NdotH = max( 0, dot(N,H));
  return pow( NdotH, specularPower);
}

float Fresnel (float3 N, float3 V, float fresnelExp) 
{
    return max(0, pow( abs(1.0 - dot(N,V) ), fresnelExp) );
}

float3 Ungamma (float3 input)
{
  return pow(input,2.2);
}

float3 RimLight (float3 N,
                 float3 V )
{
  rimColorSky = Ungamma(rimColorSky);
  rimColor = Ungamma(rimColor);

  float3 Fr = Fresnel(N,V,rimExponent);
  float3 rimLighting = 0;
  if ( useRimLight ) 
      rimLighting += Fr*rimColor;
  if ( useRimLightSky ) 
      rimLighting += Fr*rimColorSky * max( 0, dot(N, float3(0,1,0) ) );

  return rimLighting;
}

float3 Tonemap(float3 input) 
{
  float A = 0.22;
  float B = 0.3;
  float C = 0.1;
  float D = 0.2;
  float E = 0.01;
  float F = 0.3;
  float linearWhite = 11.2;
  float3 Fcolor = ((input*(A*input+C*B)+D*E)/(input*(A*input+B)+D*F)) - E/F;
  float  Fwhite = ((linearWhite*(A*linearWhite+C*B)+D*E)/(linearWhite*(A*linearWhite+B)+D*F)) - E/F;
  return Fcolor/Fwhite;
}

float4 CharacterPS (PS_IN In) : COLOR
{
	float3 V   = normalize( In.ViewDir );
	
	float2 UV   = In.UV;

    // Sample and decode normal map texture.
    float2 NormalMapXY = tex2D(normal_map_texture, UV).xy;
    float3 NormalMapSample = RecoverXYZFromNormalMapSample( NormalMapXY );
    
    // Scale the intensity of the normal map sample.
    NormalMapSample.xy *= normalIntensity;
    
    // Convert normal map sample to object space.
    float3 N = BumpNormalToObjectSpace( In.Normal, In.Binormal, In.Tangent, NormalMapSample );

	float AO = tex2D(ao_texture, UV).x;
	float3 vDiffuse = float3(0,0,0);

	float3 L = normalize( -lightDir.xyz );
	float3 Lc = Ungamma(lightColor); 

	// Diffuse Lighting
	vDiffuse  = Diffuse( N, L ) * Lc;
	vDiffuse *= diffuseScaler;
	vDiffuse *= saturate(lerp(1.0, AO, AOAmount));

	if( useDiffuseMap )
	{
		float4 DiffuseTex   = tex2D(diffuse_texture, UV);
		DiffuseTex.rgb = Ungamma(DiffuseTex.rgb);
 	    diffuseColorMid.rgb = lerp(diffuseColorMid.rgb, DiffuseTex.rgb, DiffuseTex.a);
	}
	if( useColorRampDiffuse )
	{
	   if( vDiffuse.x < diffuseColorBounceLightPoint )
	   {
		 vDiffuse = lerp(diffuseColorBounceLight, diffuseColorLow, vDiffuse.x/diffuseColorBounceLightPoint);
	   }
	   else if( vDiffuse.x < diffuseColorRampMidpoint )
	   {
		 vDiffuse = lerp(diffuseColorLow, diffuseColorMid, (vDiffuse.x - diffuseColorBounceLightPoint) /(diffuseColorRampMidpoint - diffuseColorBounceLightPoint));
	   }
	   else
	   {
		 vDiffuse = lerp(diffuseColorMid, diffuseColorHigh, (vDiffuse.x - diffuseColorRampMidpoint)/(1.0f - diffuseColorRampMidpoint));
	   }
	}

	// Specular Lighting
	specularColor = Ungamma(specularColor);
	float3 vSpecular = Specular( N, L, V, specularPower);
	vSpecular *= Lc * specularColor;
	vSpecular *= specularScaler;		
	vSpecular *= saturate(lerp(1.0,AO,AOAmount));

	// Rim Lighting
	float3 vRimLighting = RimLight(N,V);
	vRimLighting *= saturate(lerp(1.0,AO,AOAmount));

	float3 Color = 0;  
	Color = (vDiffuse+vSpecular+vRimLighting);
	Color = lerp( Color * pow(2,exposure), Tonemap( Color * pow( 2, exposure) ), tonemapAmount);

	Color -= brightnessControl;
	
    // De-saturate
    float luminance = dot(Color.rgb, float3(0.3,0.59,0.11));
    Color = lerp(luminance, Color.rgb, .7);

	return float4(Color.rgb, 1.0f);
}























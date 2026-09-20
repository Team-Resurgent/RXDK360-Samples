
const float4 CustomColor1       : register(c210);
const float4 CustomColor2       : register(c211);
const float4 CustomColor3       : register(c212);

const float4 Transparency       : register(c213);
const float4 Reflectivity       : register(c214);

const float4 SkinTone           : register(c215);
const float4 Hair               : register(c216);
const float4 MouthTone          : register(c217);
const float4 IrisTone           : register(c218); 
const float4 EyeBrowTone        : register(c219);
const float4 EyeShadowTone      : register(c220);
const float4 FacialHair         : register(c221);
const float4 SkinFeature1Tone   : register(c222);
const float4 SkinFeature2Tone   : register(c223);


float3 
InterpolateIntensity(
    float value,
    float3 colour )
{
    // 0->Black
    // 1->colour
    return value * colour;
}

float4
FaceShader(
    float4 skinFeatures,
    float4 facialHair,
    float4 eyeBrow,
    float4 eye,
    float4 mouth,
    float4 eyeShadow)
{
    const float4 White = { 1, 1, 1, 1 };
    
    // Intensity mapping
    skinFeatures.rgb = 
	    InterpolateIntensity(skinFeatures.r, SkinFeature1Tone) + 
	    InterpolateIntensity(skinFeatures.g, SkinFeature2Tone) + 
	    InterpolateIntensity(skinFeatures.b, SkinTone);
	eyeBrow.rgb = 
	    InterpolateIntensity(eyeBrow.r, EyeBrowTone) +
	    InterpolateIntensity(eyeBrow.g, White) +
	    InterpolateIntensity(eyeBrow.b, SkinTone);
	eye.rgb = 
        InterpolateIntensity(eye.r, IrisTone) + 
	    InterpolateIntensity(eye.g, White) + 
	    InterpolateIntensity(eye.b, SkinTone);
	mouth.rgb = 
	    InterpolateIntensity(mouth.r, MouthTone) +
	    InterpolateIntensity(mouth.g, White) +
	    InterpolateIntensity(mouth.b, SkinTone);
	facialHair.rgb = 
	    InterpolateIntensity(facialHair.r, FacialHair) +
	    InterpolateIntensity(facialHair.g, White) +
	    InterpolateIntensity(facialHair.b, SkinTone);
	eyeShadow.rgb = 
	    InterpolateIntensity(eyeShadow.r, EyeShadowTone);
	
	float4 currentColour;
	
	currentColour.rgb = SkinTone.rgb;
	currentColour.rgb = lerp(currentColour.rgb, skinFeatures.rgb, skinFeatures.a);
	currentColour.rgb = lerp(currentColour.rgb, eyeShadow.rgb, eyeShadow.a);
	currentColour.rgb = lerp(currentColour.rgb, mouth.rgb, mouth.a);
	currentColour.rgb = lerp(currentColour.rgb, eye.rgb, eye.a);
	currentColour.rgb = lerp(currentColour.rgb, facialHair.rgb, facialHair.a);
	currentColour.rgb = lerp(currentColour.rgb, eyeBrow.rgb, eyeBrow.a);

	currentColour.a = 1;
	
	return currentColour;
}

float3
IntensityMap(
    float3 baseColour,
    float4 intensityMap)
{
    intensityMap.rgb = 
        InterpolateIntensity(intensityMap.r, CustomColor1.rgb) +
        InterpolateIntensity(intensityMap.g, CustomColor2.rgb) + 
        InterpolateIntensity(intensityMap.b, CustomColor3.rgb);
	return lerp(baseColour.rgb, intensityMap.rgb, intensityMap.a);
}
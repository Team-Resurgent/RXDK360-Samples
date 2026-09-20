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

// vshader
uniform float4   g_vFogRange    : register(c34); // ( x, fog_end, (1/(fog_end-fog_start)), x)

sampler2D s_causticTex          : register(s6);

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

// Depth biases matching the current D3D states
float2 vBiases : register(c0);

// Receives depth in interpolator, performs perspective divide and depth-bias, 
// and broadcasts output to COLOR.  If the perspective matrix is known to 
// be orthogonal, we can skip the divide and reduce ALU load.  However, the 
// shader is likely to be fill-bound, or nearly so, as it stands.
float4 WriteSkinnedDepthPS( in float2 vPosZW : TEXCOORD0 ) : COLOR
{
    // Perspective divide
    float fDepth = vPosZW.x / vPosZW.y;
    
    // This calculation matches what the GPU does, but at different precisions.
    // Depth bias occurs after perspective divide (because the scan converter is 
    // later in the pipeline than the primitive assembler).
    // These bias values should match those set as D3D states.
    float zBias = vBiases.x + vBiases.y * max( abs( ddx( fDepth ) ), abs( ddy( fDepth ) ) ); 
    
    // Standard viewport arrangement for fixed-point depth has far plane at 1.0
    // and near plane at 0.0.  So bias to the rear is a '+'.
    return fDepth + zBias;
}

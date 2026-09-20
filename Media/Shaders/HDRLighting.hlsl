//--------------------------------------------------------------------------------------
// HDRLighting.hlsl
//
// Shaders for High Dynamic Range Lighting sample. 
//
// These shaders are used to quickly calculate the average luminance of the rendered
// scene, simulate the viewer's light adaptation level, map the high-dynamic range of
// colors to a range displayable on a PC monitor, and perform post-process lighting
// effects. 
//
// The algorithms described in this sample are based very closely on the lighting
// effects implemented in Masaki Kawase's Rthdribl sample and the tone mapping process
// described in the whitepaper "Tone Reproduction for Digital Images"
//
// References: 
//     Real-Time High Dynamic Range Image-Based Lighting (Rthdribl)
//     Masaki Kawase
//     http://www.daionet.gr.jp/~masa/rthdribl/ 
//
//     "Photographic Tone Reproduction for Digital Images"
//     Erik Reinhard, Mike Stark, Peter Shirley and Jim Ferwerda
//     http://www.cs.utah.edu/~reinhard/cdrom/ 
// 
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------


//--------------------------------------------------------------------------------------
// Global constants
//--------------------------------------------------------------------------------------
static const int    MAX_SAMPLES            = 16;    // Maximum texture grabs
static const int    NUM_LIGHTS             = 2;     // Scene lights 
static const float  BRIGHT_PASS_THRESHOLD  = 5.0f;  // Threshold for BrightPass filter
static const float  BRIGHT_PASS_OFFSET     = 10.0f; // Offset for BrightPass filter

// The per-color weighting to be used for luminance calculations in RGB order.
static const float3 LUMINANCE_VECTOR  = float3(0.2125f, 0.7154f, 0.0721f);

// The per-color weighting to be used for blue shift under low light.
static const float3 BLUE_SHIFT_VECTOR = float3(1.05f, 0.97f, 1.27f); 


//--------------------------------------------------------------------------------------
// Vertex shader constants
//--------------------------------------------------------------------------------------

// Transformation matrices
uniform float4x4 g_mObjectToView  : register(c0);   // Object space to view space
uniform float4x4 g_mProjection    : register(c4);   // View space to clip space


//--------------------------------------------------------------------------------------
// Pixel shader constants
//--------------------------------------------------------------------------------------

uniform float4   g_fLightingCoefficient : register(c12);

#define g_fDiffuseCoefficient g_fLightingCoefficient.x   // Coefficient for diffuse equation
#define g_fPhongCoefficient   g_fLightingCoefficient.y   // Coefficient for the phong equation
#define g_fPhongExponent      g_fLightingCoefficient.z   // Exponent for the phong equation

uniform float    g_fEmissive        : register(c4);  // Emissive intensity of the current light

// Tone mapping variables
uniform float    g_fMiddleGray      : register(c5);  // The middle gray key value
uniform float    g_fElapsedTime     : register(c7);  // Time in seconds since the last calculation

uniform bool     g_bEnableBlueShift : register(c8);  // Flag indicates if blue shift is performed
uniform bool     g_bEnableToneMap   : register(c9);  // Flag indicates if tone mapping is performed

uniform float    g_fBloomScale      : register(c10);  // Bloom process multiplier
uniform float    g_fStarScale       : register(c11);  // Star process multiplier

// Light variables
uniform float4   g_avLightPositionView[NUM_LIGHTS] : register(c20);   // Light positions in view space
uniform float    g_afLightIntensity[NUM_LIGHTS]    : register(c22);      // Floating point light intensities

// Contains sampling offsets used by the techniques

#ifdef EntryPoint_SampleLumInitialPS
#define USE_SAMPLEOFFSETS
#endif

#ifdef EntryPoint_SampleLumIterativePS
#define USE_SAMPLEOFFSETS
#endif

#ifdef EntryPoint_SampleLumFinalPS
#define USE_SAMPLEOFFSETS
#endif

#ifdef EntryPoint_DownScale4x4PS
#define USE_SAMPLEOFFSETS
#endif

#ifdef EntryPoint_DownScale2x2PS
#define USE_SAMPLEOFFSETS
#endif

#ifdef EntryPoint_GaussBlur5x5PS
#define USE_SAMPLEOFFSETS
#define USE_SAMPLEWEIGHTS
#endif

#ifdef EntryPoint_BloomPS
#define USE_SAMPLEOFFSETS
#define USE_SAMPLEWEIGHTS
#endif

#ifdef EntryPoint_StarPS
#define USE_SAMPLEOFFSETS
#define USE_SAMPLEWEIGHTS
#endif

#ifdef EntryPoint_MergeTextures_1PS
#define USE_SAMPLEWEIGHTS
#endif
#ifdef EntryPoint_MergeTextures_2PS
#define USE_SAMPLEWEIGHTS
#endif
#ifdef EntryPoint_MergeTextures_3PS
#define USE_SAMPLEWEIGHTS
#endif
#ifdef EntryPoint_MergeTextures_4PS
#define USE_SAMPLEWEIGHTS
#endif
#ifdef EntryPoint_MergeTextures_5PS
#define USE_SAMPLEWEIGHTS
#endif
#ifdef EntryPoint_MergeTextures_6PS
#define USE_SAMPLEWEIGHTS
#endif
#ifdef EntryPoint_MergeTextures_7PS
#define USE_SAMPLEWEIGHTS
#endif
#ifdef EntryPoint_MergeTextures_8PS
#define USE_SAMPLEWEIGHTS
#endif

#ifdef USE_SAMPLEOFFSETS
uniform float4   g_avSampleOffsets[MAX_SAMPLES] : register(c0);
#endif // USE_SAMPLEOFFSETS

#ifdef USE_SAMPLEWEIGHTS
uniform float4   g_avSampleWeights[MAX_SAMPLES] : register(c16);
#endif // USE_SAMPLEWEIGHTS

//--------------------------------------------------------------------------------------
// Texture samplers
//--------------------------------------------------------------------------------------
sampler s0 : register(s0);
sampler s1 : register(s1);
sampler s2 : register(s2);
sampler s3 : register(s3);
sampler s4 : register(s4);
sampler s5 : register(s5);
sampler s6 : register(s6);
sampler s7 : register(s7);


//--------------------------------------------------------------------------------------
// Vertex shaders
//--------------------------------------------------------------------------------------


//--------------------------------------------------------------------------------------
// Name: TransformSceneVS()
// Desc: Transforms the incoming vertex from object to clip space, and passes the vertex
//       position and normal in view space on to the pixel shader
//--------------------------------------------------------------------------------------
struct TRANSFORMED_VERTEX
{
    float4 Position   : POSITION;
    float2 TexCoords  : TEXCOORD0;
    float3 ViewPos    : TEXCOORD1;
    float3 ViewNormal : TEXCOORD2;
};

TRANSFORMED_VERTEX TransformSceneVS( float3 vPosition  : POSITION, 
                                     float3 vNormal    : NORMAL,
                                     float2 vTexCoords : TEXCOORD0 )
{
    TRANSFORMED_VERTEX Output;
  
    // tranform the position/normal into view space
    float4 vViewPosition = mul( float4( vPosition, 1.0f ), g_mObjectToView );
    float3 vViewNormal   = normalize( mul( vNormal, (float3x3)g_mObjectToView ) );

    // project view space to screen space
    Output.Position = mul( vViewPosition, g_mProjection );
    
    // Pass the texture coordinate without modification
    Output.TexCoords = vTexCoords;

    // Pass view position into a texture iterator
    Output.ViewPos = vViewPosition.xyz;
    
    // Pass view surface normal into a texture iterator
    Output.ViewNormal = vViewNormal;
    
    return Output;
}


//--------------------------------------------------------------------------------------
// Pixel shaders
//--------------------------------------------------------------------------------------


//--------------------------------------------------------------------------------------
// Name: PointLightPS()
// Desc: Per-pixel diffuse, specular, and emissive lighting
//--------------------------------------------------------------------------------------
float4 PointLightPS( in float2 vTexCoords    : TEXCOORD0,
                     in float3 vViewPosition : TEXCOORD1,
                     in float3 vViewNormal   : TEXCOORD2 ) : COLOR
{
    float3 vPointToCamera = normalize(-vViewPosition);

    // Start with ambient term
    float fIntensity = 0.02f;

    // Add emissive term to the total intensity
    fIntensity += g_fEmissive; 

    for( int i=0; i < NUM_LIGHTS; i++ )
    {
        // Calculate illumination variables
        float3 vLightToPoint = normalize(vViewPosition - g_avLightPositionView[i]);
        float3 vReflection   = reflect(vLightToPoint, vViewNormal);
        float  fPhongValue   = saturate(dot(vReflection, vPointToCamera));

        // Calculate diffuse term
        float  fDiffuse      = g_fDiffuseCoefficient * saturate(dot(vViewNormal, -vLightToPoint));

        // Calculate specular term
        float  fSpecular     = g_fPhongCoefficient * pow(fPhongValue, g_fPhongExponent);
        
        // Scale according to distance from the light
        float fDistance = distance(g_avLightPositionView[i], vViewPosition);
        fIntensity += (fDiffuse + fSpecular) * g_afLightIntensity[i]/(fDistance*fDistance);
    }
    
    // Multiply by texture color
    float3 vColor = fIntensity * tex2D(s0, vTexCoords);

    return float4(vColor, 1.0f);
}


//--------------------------------------------------------------------------------------
// Name: LightSpherePS()
// Desc: Renders emissive light spheres
//--------------------------------------------------------------------------------------
float4 LightSpherePS() : COLOR
{
    return float4( g_fEmissive, g_fEmissive, g_fEmissive, 1.0f );
}


//--------------------------------------------------------------------------------------
// Name: FinalScenePassPS()
// Desc: Perform blue shift, tone map the scene, and add post-processed light effects
//--------------------------------------------------------------------------------------
float4 FinalScenePassPS( in float2 vScreenPosition : TEXCOORD0 ) : COLOR
{
    float4 vSample     = tex2D( s0, vScreenPosition );
    float4 vBloom      = tex2D( s1, vScreenPosition );
    float4 vStar       = tex2D( s2, vScreenPosition );
    float  fAdaptedLum = tex2D( s3, float2(0.5f, 0.5f) ).r;

    // For very low light conditions, the rods will dominate the perception of
    // light, and therefore color will be desaturated and shifted towards blue.
    if( g_bEnableBlueShift )
    {
        // Define a linear blending from -1.5 to 2.6 (log scale) which
        // determines the lerp amount for blue shift
        float fBlueShiftCoefficient = 1.0f - (fAdaptedLum + 1.5)/4.1;
        fBlueShiftCoefficient = saturate(fBlueShiftCoefficient);

        // Lerp between current color and blue, desaturated copy
        float3 vRodColor = dot( (float3)vSample, LUMINANCE_VECTOR ) * BLUE_SHIFT_VECTOR;
        vSample.rgb = lerp( (float3)vSample, vRodColor, fBlueShiftCoefficient );
    }
    
    // Map the high range of color values into a range appropriate for
    // display, taking into account the user's adaptation level, and selected
    // values for for middle gray and white cutoff.
    if( g_bEnableToneMap )
    {
        vSample.rgb *= g_fMiddleGray/(fAdaptedLum + 0.001f);
        vSample.rgb /= (1.0f+vSample);
    }  
    
    // Add the star and bloom post processing effects
    vSample += g_fStarScale * vStar;
    vSample += g_fBloomScale * vBloom;
    
    return vSample;
}


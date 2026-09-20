//--------------------------------------------------------------------------------------
// VarianceShadowMaps.hlsl
//
// Shadow mapping sample comparing variance shadow maps to traditional shadow maps.
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------


//--------------------------------------------------------------------------------------
// Vertex shader globals
//--------------------------------------------------------------------------------------

float4x4 g_matCameraViewProj : register(c0); // Standard world * view * projection transform.
float4x4 g_matShadowViewProj : register(c4); // Transform into light space
float3   g_vLightDirection   : register(c8); // Local space light direction
float3   g_vViewPosition     : register(c9); // Local space view position


//--------------------------------------------------------------------------------------
// Pixel  shader globals
//--------------------------------------------------------------------------------------
float2   g_vTexMod                    : register(c3);  // Constant used to optionally invert the base texture color
bool     g_bUseVarianceShadowMap      : register(b0);  // Whether to use VSM or traditional shadow mapping
bool     g_bDebugShowShadow           : register(b2);  // Debugging option to isolate the shadow
float    g_fEpsilonVSM                : register(c10); // Epsilon for VSM algorithm

static float  g_fAmbientI       = 0.1f;                      // Ambient intensity
static float4 g_vSpecularColor = { 1.0f, 0.5f, 0.5f, 0.0f }; // Specular color

sampler2D Sampler0          : register(s0); // Shadow map texture for downsample pixel shader.
sampler2D Sampler1          : register(s1); // Shadow map texture for downsample pixel shader.
sampler2D DiffuseTexture    : register(s0); // Base diffuse texture

sampler2D ShadowMap         : register(s1); // Shadow map texture
sampler2D VarianceShadowMap : register(s1); // Variance shadow map


//--------------------------------------------------------------------------------------
// Name: DepthOnlyVS()
// Desc: Vertex shader that outputs depth for rendering into shadow maps
//--------------------------------------------------------------------------------------
float4 DepthOnlyVS( in float4 vPosition   : POSITION ) : POSITION
{
    return mul( vPosition, g_matCameraViewProj );
}


//--------------------------------------------------------------------------------------
// Name: CopyDepthToVariancePS()
// Desc: Pixel shader for copying a D24S8 shadow map into a variance shadow map. 
//       Variance shadow maps contain both depth and depth-squared.
//--------------------------------------------------------------------------------------
float4 CopyDepthToVariancePS( in float2 vTexCoord : TEXCOORD0 ) : COLOR
{
    float fDepth0 = tex2D( Sampler0, vTexCoord ).r;
    return float4( fDepth0, fDepth0*fDepth0, 0.0f, 0.0f );
}


//--------------------------------------------------------------------------------------
// Name: HorizontalBlurDepthToVariancePS()
// Desc: Copy depth to variance and perform the horizontal portion of a two-pass
//       separable 5x5 Gaussian blur
//--------------------------------------------------------------------------------------
float4 HorizontalBlurDepthToVariancePS( in float2 vTexCoord : TEXCOORD0 ) : COLOR0
{
    // Fetch a row of 5 pixels from the D24S8 depth map
    float4 DepthSamples0123;
    float4 DepthSamples4___;
    asm
    {
        tfetch2D DepthSamples0123.x___, vTexCoord, Sampler0, OffsetX = -2.0, MinFilter=point, MagFilter=point
        tfetch2D DepthSamples0123._x__, vTexCoord, Sampler0, OffsetX = -1.0, MinFilter=point, MagFilter=point
        tfetch2D DepthSamples0123.__x_, vTexCoord, Sampler0, OffsetX = -0.0, MinFilter=point, MagFilter=point
        tfetch2D DepthSamples0123.___x, vTexCoord, Sampler0, OffsetX = +1.0, MinFilter=point, MagFilter=point
        tfetch2D DepthSamples4___.x___, vTexCoord, Sampler0, OffsetX = +2.0, MinFilter=point, MagFilter=point
    };
    
    // Do the Guassian blur (using a 5-tap filter kernel of [ 1 4 6 4 1 ] )
    float z  = dot( DepthSamples0123.xyzw,  float4( 1.0/16, 4.0/16, 6.0/16, 4.0/16 ) ) + DepthSamples4___.x * ( 1.0 / 16 );

    DepthSamples0123.xyzw = DepthSamples0123.xyzw * DepthSamples0123.xyzw;
    DepthSamples4___.x    = DepthSamples4___.x    * DepthSamples4___.x;
    float z2 = dot( DepthSamples0123.xyzw,  float4( 1.0/16, 4.0/16, 6.0/16, 4.0/16 ) ) + DepthSamples4___.x * ( 1.0 / 16 );
    
    return float4( z, z2, 0, 0 );
}


//--------------------------------------------------------------------------------------
// Name: VerticalBlurDepthToVariancePS()
// Desc: Vertical portion of a two-pass separable 5x5 Gaussian blur for variance
//       shadow maps
//--------------------------------------------------------------------------------------
float4 VerticalBlurDepthToVariancePS( in float2 vTexCoord : TEXCOORD0 ) : COLOR0
{
    // Note that this second pass of the separable filter can use filtered fetches
    // Fetch 4 samples which filter across a column of 5 pixels from the VSM
    float4 t0, t1;
    asm
    {
        tfetch2D t0.xy__, vTexCoord, Sampler0, OffsetY = +1.5, MinFilter=linear, MagFilter=linear
        tfetch2D t0.__xy, vTexCoord, Sampler0, OffsetY = +0.5, MinFilter=linear, MagFilter=linear
        tfetch2D t1.xy__, vTexCoord, Sampler0, OffsetY = -0.5, MinFilter=linear, MagFilter=linear
        tfetch2D t1.__xy, vTexCoord, Sampler0, OffsetY = -1.5, MinFilter=linear, MagFilter=linear
    };
    
    // Sum results with Gaussian weights
    float z  = dot( float4( t0.x, t0.z, t1.x, t1.z ), float4( 2.0/16, 6.0/16, 6.0/16, 2.0/16 ) );
    float z2 = dot( float4( t0.y, t0.w, t1.y, t1.w ), float4( 2.0/16, 6.0/16, 6.0/16, 2.0/16 ) );
    return float4( z, z2, 0, 0 );
}


//--------------------------------------------------------------------------------------
// Name: LightWithShadowsVS()
// Desc: Lighting with shadowing vertex shader
//--------------------------------------------------------------------------------------
struct VSOUT_LIGHTWITHSHADOWS
{
     float4 vPosition  : POSITION;
     float2 vTexCoord  : TEXCOORD0;
     float4 vShadowPos : TEXCOORD1;
     float3 vNormal    : TEXCOORD2;
     float3 vLightDir  : TEXCOORD3;
     float3 vHalfAngle : TEXCOORD4;
};

VSOUT_LIGHTWITHSHADOWS LightWithShadowsVS( in float4 vPosition  : POSITION,
                                           in float3 vNormal    : NORMAL,
                                           in float2 vTexCoord  : TEXCOORD0 )
{
    VSOUT_LIGHTWITHSHADOWS Output;
    
    // Transform the position
    Output.vPosition = mul( vPosition, g_matCameraViewProj );
    
    // Compute the position in light space for the shadow map
    Output.vShadowPos = mul( vPosition, g_matShadowViewProj );
    
    // Transform the shadow position from [-1..+1] clip sapce to [0..1] texture space
    Output.vShadowPos.x = ( +Output.vShadowPos.x + Output.vShadowPos.w ) / 2;
    Output.vShadowPos.y = ( -Output.vShadowPos.y + Output.vShadowPos.w ) / 2;
        
    // Copy the normal
    Output.vNormal    = vNormal;
    
    // Copy the diffuse texture coordinate
    Output.vTexCoord  = vTexCoord;
    
    // Light direction is constant for a directional light
    Output.vLightDir  = normalize( g_vLightDirection );
    
    // Compute the half angle vector (H = L + V)
    float3 V          = normalize( g_vViewPosition - vPosition );
    Output.vHalfAngle = normalize( g_vLightDirection + V );
    
    return Output;
}


//--------------------------------------------------------------------------------------
// Name: PerPixelLighting()
// Desc: Helper function to compute simple per-pixel lighintg
//--------------------------------------------------------------------------------------
float4 PerPixelLighting( float3 vNormal, float3 vHalfAngle, float3 vLightDir,
                         float2 vTexCoord, float fShadowAttenuation )
{
    // Normalize the normal and half angle vectors (if the geometry is sufficienty 
    // tessellated this could potentially be skipped).
    vNormal    = normalize( vNormal );
    vHalfAngle = normalize( vHalfAngle );
    vLightDir  = normalize( vLightDir );
    
    // Fetch the diffuse color
    float4 vDiffuseColor = tex2D( DiffuseTexture, vTexCoord.xy );

    // Optionally invert the texture (to distinguish white and black chess pieces)
    vDiffuseColor = g_vTexMod.x + g_vTexMod.y * vDiffuseColor; 
    
    // Compute the diffuse and specluar contributions with shadowing
    float fDiffuseI  =      max( 0, dot( vNormal, vLightDir  ) );
    float fSpecularI = pow( max( 0, dot( vNormal, vHalfAngle ) ), 20 );

    // Combine them with shadows and ambient
    return                        vDiffuseColor    * g_fAmbientI + 
           fShadowAttenuation * ( vDiffuseColor    * fDiffuseI + 
                                  g_vSpecularColor * fSpecularI );
}


//--------------------------------------------------------------------------------------
// Name: ComputeShadowAttenuationPoint()
// Desc: Compute the attenuation due to shadowing using point sampling
//--------------------------------------------------------------------------------------
float ComputeShadowAttenuationPoint( float3 vShadowCoord )
{
    // Compute shadowing term
    float z = tex2D( ShadowMap, vShadowCoord.xy ).r;
    return ( vShadowCoord.z <= z ) ? 1.0f : 0.0f;
}


//--------------------------------------------------------------------------------------
// Name: ComputeShadowAttenuationBilinear()
// Desc: Compute the attenuation due to shadowing using point sampling
//--------------------------------------------------------------------------------------
float ComputeShadowAttenuationBilinear( float3 vShadowCoord )
{
    float  LOD;
    float4 SampledDepth;
    float4 Weights;
    asm 
    {
        // The LOD for the fetches from the depth texture is computed using aniso
        // filtering so that it is based on the minimum of the x and y gradients.
        getCompTexLOD2D LOD.x, vShadowCoord.xy, ShadowMap, AnisoFilter=max16to1
        setTexLOD LOD.x

        // Fetch the bilinear filter fractions and four samples from the depth texture.
        tfetch2D SampledDepth.x___, vShadowCoord.xy, ShadowMap, OffsetX = -0.5, OffsetY = -0.5, UseComputedLOD=false, UseRegisterLOD=true
        tfetch2D SampledDepth._x__, vShadowCoord.xy, ShadowMap, OffsetX =  0.5, OffsetY = -0.5, UseComputedLOD=false, UseRegisterLOD=true
        tfetch2D SampledDepth.__x_, vShadowCoord.xy, ShadowMap, OffsetX = -0.5, OffsetY =  0.5, UseComputedLOD=false, UseRegisterLOD=true
        tfetch2D SampledDepth.___x, vShadowCoord.xy, ShadowMap, OffsetX =  0.5, OffsetY =  0.5, UseComputedLOD=false, UseRegisterLOD=true

        getWeights2D Weights, vShadowCoord.xy, ShadowMap, MagFilter=linear, MinFilter=linear, UseComputedLOD=false, UseRegisterLOD=true
    };

    // Return the bilinear filtered result
    Weights = float4( (1-Weights.x)*(1-Weights.y), Weights.x*(1-Weights.y), (1-Weights.x)*Weights.y, Weights.x*Weights.y );
    float4 Attenuation = step( vShadowCoord.z, SampledDepth );
    return dot( Attenuation, Weights );
}


//--------------------------------------------------------------------------------------
// Name: ComputeShadowAttenuationVSM()
// Desc: Compute the attenuation due to shadowing using the variance shadow map
//--------------------------------------------------------------------------------------
float ComputeShadowAttenuationVSM( float3 vShadowCoord )
{
    float4 vVSM   = tex2D( VarianceShadowMap, vShadowCoord.xy );
    float  fAvgZ  = vVSM.r; // Filtered z
    float  fAvgZ2 = vVSM.g; // Filtered z-squared

    // Standard shadow map comparison
    if( vShadowCoord.z <= fAvgZ )
        return 1.0f;

    // Use variance shadow mapping to compute the maximum probability that the
    // pixel is in shadow
    float variance = ( fAvgZ2 ) - ( fAvgZ * fAvgZ );
    variance       = min( 1.0f, max( 0.0f, variance + g_fEpsilonVSM ) );
    
    float mean     = fAvgZ;
    float d        = vShadowCoord.z - mean;
    float p_max    = variance / ( variance + d*d );

    // To combat light-bleeding, experiment with raising p_max to some power
    // (Try values from 0.1 to 100.0, if you like.)
    return pow( p_max, 4.0f );
}


//--------------------------------------------------------------------------------------
// Name: LightWithShadowsPS()
// Desc: Pixel shader for lighting with either variance shadow maps or traditional,
//       point-filtered shadow maps
//--------------------------------------------------------------------------------------
float4 LightWithShadowsPS( float2 vTexCoord      : TEXCOORD0, 
                           float4 vLightSpacePos : TEXCOORD1, 
                           float3 vNormal        : TEXCOORD2, 
                           float3 vLightDir      : TEXCOORD3,
                           float3 vHalfAngle     : TEXCOORD4 ) : COLOR
{
    // Compute projected xyz
    vLightSpacePos.xyz = vLightSpacePos.xyz / vLightSpacePos.w;

    // Compute the attenuation due to shadowing
    float fShadow;
    {
        if( g_bUseVarianceShadowMap )
            fShadow = ComputeShadowAttenuationVSM( vLightSpacePos.xyz );
        else
            fShadow = ComputeShadowAttenuationBilinear( vLightSpacePos.xyz );
        
        if( g_bDebugShowShadow )
            return fShadow;
    }

    // Use the shadowing term and the input parameters to do per-pixel lighting
    return PerPixelLighting( vNormal, vHalfAngle, vLightDir, vTexCoord, fShadow );
}


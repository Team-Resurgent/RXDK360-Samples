//--------------------------------------------------------------------------------------
// ShadowMap.hlsl
//
// Shadow mapping sample
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------


// Standard world * view * projection transform.
float4x4 matWorldViewProj : register(c0);


// Simple position only vertex shader
void WriteDepthVS( in float4 vPosition : POSITION,
                   in float3 vNormal : NORMAL,
                   in float2 vTexCoord : TEXCOORD0,
                   out float4 oPosition : POSITION )
{
    oPosition = mul( vPosition, matWorldViewProj );
}


//--------------------------------------------------------------------------------------
// Shaders for downsampling a shadow map.
//--------------------------------------------------------------------------------------


// Shadow map texture for downdample pixel shader.
sampler2D DownsampleTex : register(s0);


// Depth downsample vertex shader
void DownsampleDepthVS( in float4 vPosition : POSITION,
                        in float2 vTexCoord : TEXCOORD0,
                        out float4 oPosition : POSITION,
                        out float2 oTexCoord : TEXCOORD0 )
{
    oPosition = mul( vPosition, matWorldViewProj );
    oTexCoord = vTexCoord;
}


// Depth downsample pixel shader
void DownsampleDepthPS( in float2 vTexCoord : TEXCOORD0,
                        out float4 oColor : COLOR,
                        out float oDepth : DEPTH )
{
    // Fetch the four samples
    float4 SampledDepth;
    asm {
        tfetch2D SampledDepth.x___, vTexCoord, DownsampleTex, OffsetX = -0.5, OffsetY = -0.5
        tfetch2D SampledDepth._x__, vTexCoord, DownsampleTex, OffsetX =  0.5, OffsetY = -0.5
        tfetch2D SampledDepth.__x_, vTexCoord, DownsampleTex, OffsetX = -0.5, OffsetY =  0.5
        tfetch2D SampledDepth.___x, vTexCoord, DownsampleTex, OffsetX =  0.5, OffsetY =  0.5
    };
    
    // Find the maximum.
    SampledDepth.xy = max( SampledDepth.xy, SampledDepth.zw );
    SampledDepth.x = max( SampledDepth.x, SampledDepth.y );

    oColor = SampledDepth.x;
    oDepth = SampledDepth.x;
}


//--------------------------------------------------------------------------------------
// Shaders for shadow mapping.
//--------------------------------------------------------------------------------------


// Transform into light space
float4x4 matShadowTex : register(c4);

// Local space light direction
float3 vLocalLightDir : register(c8);

// Local space view position
float3 vLocalViewPos : register(c9);

// Base diffuse texture
sampler2D DiffuseTex : register(s0);

// Shadow map texture
sampler2D DepthTex : register(s1);

// Rotation map texture
sampler2D RotationTex : register(s2);

// Offset map texture
sampler3D OffsetTex : register(s3);

// Ambient intensity
static float AmbientI = { 0.1f };

// Specular color
static float4 vSpecularColor = { 1.0f, 0.5f, 0.5f, 0.0f };

// Hack for inverting the texture color
float2 TexMod : register(c3);  

// Scale for the rotated poisson filter
float FilterScale : register(c2);


// Lighting with shadowing vertex shader
void LightWithShadowsVS( in float4 vPosition : POSITION,
                         in float3 vNormal : NORMAL,
                         in float2 vTexCoord : TEXCOORD0,
                         out float4 oPosition : POSITION,
                         out float4 oTexCoord : TEXCOORD0,
                         out float4 oShadowPos : TEXCOORD1,
                         out float3 oNormal : TEXCOORD2,
                         out float3 oLightDir : TEXCOORD3,
                         out float3 oHalfAngle : TEXCOORD4 )
{
    // Transform the position.
    oPosition = mul( vPosition, matWorldViewProj );
    
    // Copy the diffuse texture coordinate
    oTexCoord.xy = vTexCoord;
    
    // Also pack the screen x,y in the texcoord.
    oTexCoord.zw = oPosition.xy * float2( 640.0f, 360.0f );
    
    // Compute the position in light space for the shadow map
    oShadowPos = mul( vPosition, matShadowTex );
    
    // Copy the normal
    oNormal = vNormal;
    
    // Light direction is constant for a directional light
    oLightDir = vLocalLightDir;
    
    // Compute the vector to the eye (V)
    float3 V = normalize( vLocalViewPos - vPosition );
    
    // Compute the half angle vector (H = L + V)
    oHalfAngle = normalize( vLocalLightDir + V );
}


// Compute the attenuation due to shadowing using bilinear PCF sampling
float ComputeShadowAttenuationBilinear( float3 vShadowCoord )
{
    // Fetch the bilinear filter fractions and four samples from the depth texture. The LOD for the 
    // fetches from the depth texture is computed using aniso filtering so that it is based on the 
    // minimum of the x and y gradients (instead of the maximum).  
    float4 Weights;
    float LOD;
    float4 SampledDepth;
    asm {
        getCompTexLOD2D LOD.x, vShadowCoord.xy, DepthTex, AnisoFilter=max16to1
        setTexLOD LOD.x

        tfetch2D SampledDepth.x___, vShadowCoord.xy, DepthTex, OffsetX = -0.5, OffsetY = -0.5, UseComputedLOD=false, UseRegisterLOD=true
        tfetch2D SampledDepth._x__, vShadowCoord.xy, DepthTex, OffsetX =  0.5, OffsetY = -0.5, UseComputedLOD=false, UseRegisterLOD=true
        tfetch2D SampledDepth.__x_, vShadowCoord.xy, DepthTex, OffsetX = -0.5, OffsetY =  0.5, UseComputedLOD=false, UseRegisterLOD=true
        tfetch2D SampledDepth.___x, vShadowCoord.xy, DepthTex, OffsetX =  0.5, OffsetY =  0.5, UseComputedLOD=false, UseRegisterLOD=true

        getWeights2D Weights, vShadowCoord.xy, DepthTex, MagFilter=linear, MinFilter=linear, UseComputedLOD=false, UseRegisterLOD=true
    };

    Weights = float4( (1-Weights.x)*(1-Weights.y), Weights.x*(1-Weights.y), (1-Weights.x)*Weights.y, Weights.x*Weights.y );
        
    float4 Attenuation = step( vShadowCoord.z, SampledDepth );
    
    return dot( Attenuation, Weights );
}


// Lighting with shadows utilizing a bilinear PCF sampled shadow map
float4 LightWithShadowsBilinearPS( float4 vTexCoord : TEXCOORD0, 
                                   float4 vLightSpacePos : TEXCOORD1, 
                                   float3 vNormal : TEXCOORD2, 
                                   float3 vLightDir : TEXCOORD3,
                                   float3 vHalfAngle : TEXCOORD4 ) : COLOR
{
    // Compute projected xyz.
    vLightSpacePos.xyz = vLightSpacePos.xyz / vLightSpacePos.w;

    // Compute the attenuation due to shadowing.
    float ShadowAttenuation = ComputeShadowAttenuationBilinear( vLightSpacePos.xyz );
    
    // Normalize the normal and half angle vectors (if the geometry is sufficienty 
    // tessellated this could potentially be skipped).
    vNormal = normalize( vNormal );
    vHalfAngle = normalize( vHalfAngle );
    
    // Fetch the diffuse color
    float4 vDiffuseColor = tex2D( DiffuseTex, vTexCoord.xy );

    // Optionally invert the texture (to distinguish white and black chess pieces)
    vDiffuseColor = TexMod.x + TexMod.y * vDiffuseColor; 
    
    // Compute the diffuse and specluar contributions with shadowing
    float DiffuseI = max( 0, dot( vNormal, vLightDir ) ) * ShadowAttenuation + AmbientI;
    float SpecularI = pow( max( 0, dot( vNormal, vHalfAngle ) ), 20 ) * ShadowAttenuation;

    // Combine them with shadows and ambient
    return DiffuseI * vDiffuseColor + SpecularI * vSpecularColor;
}


// Compute the attenuation due to shadowing using point sampling
float ComputeShadowAttenuationPoint( float3 vShadowCoord )
{
    // Fetch a single sample from the depth texture. The LOD for the fetch from the depth 
    // texture is computed using aniso filtering so that it is based on the minimum of the 
    // x and y gradients (instead of the maximum).  
    float LOD;
    float SampledDepth;
    asm {
        getCompTexLOD2D LOD.x, vShadowCoord.xy, DepthTex, AnisoFilter=max16to1
        setTexLOD LOD.x

        tfetch2D SampledDepth.x, vShadowCoord.xy, DepthTex, UseComputedLOD=false, UseRegisterLOD=true
    };

    return SampledDepth >= vShadowCoord.z;
}


// Lighting with shadows utilizing a point sampled shadow map
float4 LightWithShadowsPointPS( float4 vTexCoord : TEXCOORD0, 
                                float4 vLightSpacePos : TEXCOORD1, 
                                float3 vNormal : TEXCOORD2, 
                                float3 vLightDir : TEXCOORD3,
                                float3 vHalfAngle : TEXCOORD4 ) : COLOR
{
    // Compute projected xyz.
    vLightSpacePos.xyz = vLightSpacePos.xyz / vLightSpacePos.w;

    // Compute the attenuation due to shadowing.
    float ShadowAttenuation = ComputeShadowAttenuationPoint( vLightSpacePos.xyz );
    
    // Normalize the normal and half angle vectors (if the geometry is sufficienty 
    // tessellated this could potentially be skipped).
    vNormal = normalize( vNormal );
    vHalfAngle = normalize( vHalfAngle );
    
    // Fetch the diffuse color
    float4 vDiffuseColor = tex2D( DiffuseTex, vTexCoord.xy );

    // Optionally invert the texture (to distinguish white and black chess pieces)
    vDiffuseColor = TexMod.x + TexMod.y * vDiffuseColor; 
    
    // Compute the diffuse and specluar contributions with shadowing
    float DiffuseI = max( 0, dot( vNormal, vLightDir ) ) * ShadowAttenuation + AmbientI;
    float SpecularI = pow( max( 0, dot( vNormal, vHalfAngle ) ), 20 ) * ShadowAttenuation;

    // Combine them with shadows and ambient
    return DiffuseI * vDiffuseColor + SpecularI * vSpecularColor;
}


static const int NUM_POISSON_TAPS = 8;
static const float2 g_Poisson[NUM_POISSON_TAPS] = 
{
    float2( 0.000000f, 0.000000f ),
    float2( 0.527837f,-0.085868f ),
    float2(-0.040088f, 0.536087f ),
    float2(-0.670445f,-0.179949f ),
    float2(-0.419418f,-0.616039f ),
    float2( 0.440453f,-0.639399f ),
    float2(-0.757088f, 0.349334f ),
    float2( 0.574619f, 0.685879f ),
};


// Compute the attenuation due to shadowing using 8-tap rotated poisson disc PCF sampling
float ComputeShadowAttenuationRotatedPoisson( float3 vShadowCoord, float2 vScreenPos )
{
    // Essentialy random angle for each screen pixel
    float fAngle = (tex2D( RotationTex, vScreenPos ).x * 2.0 - 1.0) * 3.14159;

    // Rotation and scale
    float4 vRotScale;
    vRotScale.x =  cos(fAngle) * FilterScale;
    vRotScale.y =  sin(fAngle) * FilterScale;
    vRotScale.z = -sin(fAngle) * FilterScale;
    vRotScale.w =  cos(fAngle) * FilterScale;

    float2 vShadowCoord1 = vShadowCoord.xy + (g_Poisson[0].x * vRotScale.xy) + (g_Poisson[0].y * vRotScale.zw);
    float2 vShadowCoord2 = vShadowCoord.xy + (g_Poisson[1].x * vRotScale.xy) + (g_Poisson[1].y * vRotScale.zw);
    float2 vShadowCoord3 = vShadowCoord.xy + (g_Poisson[2].x * vRotScale.xy) + (g_Poisson[2].y * vRotScale.zw);
    float2 vShadowCoord4 = vShadowCoord.xy + (g_Poisson[3].x * vRotScale.xy) + (g_Poisson[3].y * vRotScale.zw);
    float2 vShadowCoord5 = vShadowCoord.xy + (g_Poisson[4].x * vRotScale.xy) + (g_Poisson[4].y * vRotScale.zw);
    float2 vShadowCoord6 = vShadowCoord.xy + (g_Poisson[5].x * vRotScale.xy) + (g_Poisson[5].y * vRotScale.zw);
    float2 vShadowCoord7 = vShadowCoord.xy + (g_Poisson[6].x * vRotScale.xy) + (g_Poisson[6].y * vRotScale.zw);
    float2 vShadowCoord8 = vShadowCoord.xy + (g_Poisson[7].x * vRotScale.xy) + (g_Poisson[7].y * vRotScale.zw);

    // Fetch the bilinear filter fractions and four samples from the depth texture. The LOD for the 
    // fetches from the depth texture is computed using aniso filtering so that it is based on the 
    // minimum of the x and y gradients (instead of the maximum).  
    float LOD;
    float4 Depths1, Depths2, Depths3;
    asm {
        getCompTexLOD2D LOD.x, vShadowCoord.xy, DepthTex, AnisoFilter=max16to1
        setTexLOD LOD.x

        tfetch2D Depths1.x___, vShadowCoord1.xy, DepthTex, UseComputedLOD=false, UseRegisterLOD=true
        tfetch2D Depths1._x__, vShadowCoord2.xy, DepthTex, UseComputedLOD=false, UseRegisterLOD=true
        tfetch2D Depths1.__x_, vShadowCoord3.xy, DepthTex, UseComputedLOD=false, UseRegisterLOD=true
        tfetch2D Depths1.___x, vShadowCoord4.xy, DepthTex, UseComputedLOD=false, UseRegisterLOD=true

        tfetch2D Depths2.x___, vShadowCoord5.xy, DepthTex, UseComputedLOD=false, UseRegisterLOD=true
        tfetch2D Depths2._x__, vShadowCoord6.xy, DepthTex, UseComputedLOD=false, UseRegisterLOD=true
        tfetch2D Depths2.__x_, vShadowCoord7.xy, DepthTex, UseComputedLOD=false, UseRegisterLOD=true
        tfetch2D Depths2.___x, vShadowCoord8.xy, DepthTex, UseComputedLOD=false, UseRegisterLOD=true
    };

    float4 Attenuation1 = step( vShadowCoord.z, Depths1 );
    float4 Attenuation2 = step( vShadowCoord.z, Depths2 );
    
    float4 vWeights = { 1.0 / NUM_POISSON_TAPS, 1.0 / NUM_POISSON_TAPS, 
                        1.0 / NUM_POISSON_TAPS, 1.0 / NUM_POISSON_TAPS };
    
    return dot( Attenuation1, vWeights ) + dot( Attenuation2, vWeights );
}


// Lighting with shadows utilizing a 8-tap rotated poisson disc sampled shadow map
float4 LightWithShadowsRotatedPoissonPS( float4 vTexCoord : TEXCOORD0, 
                                         float4 vLightSpacePos : TEXCOORD1, 
                                         float3 vNormal : TEXCOORD2, 
                                         float3 vLightDir : TEXCOORD3,
                                         float3 vHalfAngle : TEXCOORD4 ) : COLOR
{
    // Compute projected xyz.
    vLightSpacePos.xyz = vLightSpacePos.xyz / vLightSpacePos.w;

    // Compute the attenuation due to shadowing.
    float ShadowAttenuation = ComputeShadowAttenuationRotatedPoisson( vLightSpacePos.xyz, vTexCoord.zw );
    
    // Normalize the normal and half angle vectors (if the geometry is sufficienty 
    // tessellated this could potentially be skipped).
    vNormal = normalize( vNormal );
    vHalfAngle = normalize( vHalfAngle );
    
    // Fetch the diffuse color
    float4 vDiffuseColor = tex2D( DiffuseTex, vTexCoord.xy );

    // Optionally invert the texture (to distinguish white and black chess pieces)
    vDiffuseColor = TexMod.x + TexMod.y * vDiffuseColor; 
    
    // Compute the diffuse and specluar contributions with shadowing
    float DiffuseI = max( 0, dot( vNormal, vLightDir ) ) * ShadowAttenuation + AmbientI;
    float SpecularI = pow( max( 0, dot( vNormal, vHalfAngle ) ), 20 ) * ShadowAttenuation;

    // Combine them with shadows and ambient
    return DiffuseI * vDiffuseColor + SpecularI * vSpecularColor;
}


// Compute the attenuation due to shadowing using 64-tap stratified PCF sampling
float ComputeShadowAttenuationStratified( float3 vShadowCoord, float2 vScreenPos )
{
    // Adjust the LOD for the fetches from the depth texture using aniso filtering 
    // so that it is based on the minimum of the x and y gradients 
    // (instead of the maximum).
    float LOD;
    asm {
        getCompTexLOD2D LOD.x, vShadowCoord.xy, DepthTex, AnisoFilter=max16to1
    };

    const float4 vWeights4 = { 1.0 / 4, 1.0 / 4, 1.0 / 4, 1.0 / 4 };
    const float4 vWeights52 = { 1.0 / 52, 1.0 / 52, 1.0 / 52, 1.0 / 52 };
    
    float Slice = 0.0;
    float TotalAttenuation;

    // Take 4 representative samples, if they are all the same we can skip the rest
    // of the sampling.
    {
        // Fetch our stratified offsets.
        float4 Offset0 = tex3D( OffsetTex, float3( vScreenPos, Slice ) ) * FilterScale;
        Slice += (1.0 / 32.0);
        
        float4 Offset1 = tex3D( OffsetTex, float3( vScreenPos, Slice ) ) * FilterScale;
        Slice += (1.0 / 32.0);

        // Compute offset texture coordintes.
        float2 vShadowCoord1 = vShadowCoord.xy + Offset0.xy;
        float2 vShadowCoord2 = vShadowCoord.xy + Offset0.zw;
        float2 vShadowCoord3 = vShadowCoord.xy + Offset1.xy;
        float2 vShadowCoord4 = vShadowCoord.xy + Offset1.zw;

        float4 Depths;
        
        // Fetch depths
        Depths.x = tex2Dlod( DepthTex, float4( vShadowCoord1.xy, 0.0, LOD ) ).x;
        Depths.y = tex2Dlod( DepthTex, float4( vShadowCoord2.xy, 0.0, LOD ) ).x;
        Depths.z = tex2Dlod( DepthTex, float4( vShadowCoord3.xy, 0.0, LOD ) ).x;
        Depths.w = tex2Dlod( DepthTex, float4( vShadowCoord4.xy, 0.0, LOD ) ).x;

        float4 Attenuation = step( vShadowCoord.z, Depths );
        
        TotalAttenuation = dot( Attenuation, vWeights4 );
    }

    // If all 4 samples are the same we will have exactly 0.0 or 1.0
    if( TotalAttenuation != 0.0 && TotalAttenuation != 1.0 )
    {
        // Readjust total attenuation based on the new number of samples
        TotalAttenuation *= 1.0 / 13.0;
        
        // Fetch the rest of the samples
        for( int i = 1; i < 13; i++ )
        {
            // Fetch our stratified offsets.
            float4 Offset0 = tex3D( OffsetTex, float3( vScreenPos, Slice ) ) * FilterScale;
            Slice += (1.0 / 32.0);

            float4 Offset1 = tex3D( OffsetTex, float3( vScreenPos, Slice ) ) * FilterScale;
            Slice += (1.0 / 32.0);

            // Compute offset texture coordintes.
            float2 vShadowCoord1 = vShadowCoord.xy + Offset0.xy;
            float2 vShadowCoord2 = vShadowCoord.xy + Offset0.zw;
            float2 vShadowCoord3 = vShadowCoord.xy + Offset1.xy;
            float2 vShadowCoord4 = vShadowCoord.xy + Offset1.zw;

            float4 Depths;
            
            // Fetch depths
            Depths.x = tex2Dlod( DepthTex, float4( vShadowCoord1.xy, 0.0, LOD ) ).x;
            Depths.y = tex2Dlod( DepthTex, float4( vShadowCoord2.xy, 0.0, LOD ) ).x;
            Depths.z = tex2Dlod( DepthTex, float4( vShadowCoord3.xy, 0.0, LOD ) ).x;
            Depths.w = tex2Dlod( DepthTex, float4( vShadowCoord4.xy, 0.0, LOD ) ).x;

            float4 Attenuation = step( vShadowCoord.z, Depths );
            
            TotalAttenuation += dot( Attenuation, vWeights52 );
        }
    }
        
    return TotalAttenuation;
}


// Lighting with shadows utilizing a 9-tap stratified sampled shadow map
float4 LightWithShadowsStratifiedPS( float4 vTexCoord : TEXCOORD0, 
                                         float4 vLightSpacePos : TEXCOORD1, 
                                         float3 vNormal : TEXCOORD2, 
                                         float3 vLightDir : TEXCOORD3,
                                         float3 vHalfAngle : TEXCOORD4 ) : COLOR
{
    // Compute projected xyz.
    vLightSpacePos.xyz = vLightSpacePos.xyz / vLightSpacePos.w;

    // Compute the attenuation due to shadowing.
    float ShadowAttenuation = ComputeShadowAttenuationStratified( vLightSpacePos.xyz, vTexCoord.zw );
    
    // Normalize the normal and half angle vectors (if the geometry is sufficienty 
    // tessellated this could potentially be skipped).
    vNormal = normalize( vNormal );
    vHalfAngle = normalize( vHalfAngle );
    
    // Fetch the diffuse color
    float4 vDiffuseColor = tex2D( DiffuseTex, vTexCoord.xy );

    // Optionally invert the texture (to distinguish white and black chess pieces)
    vDiffuseColor = TexMod.x + TexMod.y * vDiffuseColor; 
    
    // Compute the diffuse and specluar contributions with shadowing
    float DiffuseI = max( 0, dot( vNormal, vLightDir ) ) * ShadowAttenuation + AmbientI;
    float SpecularI = pow( max( 0, dot( vNormal, vHalfAngle ) ), 20 ) * ShadowAttenuation;

    // Combine them with shadows and ambient
    return DiffuseI * vDiffuseColor + SpecularI * vSpecularColor;
}

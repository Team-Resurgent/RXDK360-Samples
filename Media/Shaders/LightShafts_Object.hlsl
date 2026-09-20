//--------------------------------------------------------------------------------------
// LightShafts_Object.hlsl
//
// Shaders for object rendering in the LightShafts sample
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
// ATI 3D Application Research Group. 
// Copyright (C) ATI Research, Inc. All rights reserved. 
//--------------------------------------------------------------------------------------


//--------------------------------------------------------------------------------------
// Vertex shader constants
//--------------------------------------------------------------------------------------
uniform float4x4 g_matWorldViewProj         : register(c0);
uniform float4x4 g_matWorldLightProj        : register(c4);
//      float4x4 g_matWorldLight            : register(c8);
uniform float4x4 g_matWorldLightProjBias    : register(c20);
uniform float4x4 g_matWorldLightProjScroll1 : register(c24);
uniform float4x4 g_matWorldLightProjScroll2 : register(c28);


//--------------------------------------------------------------------------------------
// Pixel shader constants
//--------------------------------------------------------------------------------------
uniform float3   g_vModelL                    : register(c0);
uniform float3   g_vModelV                    : register(c1);

const   bool     g_bScrollingNoise = true; // : register(c2);
const   bool     g_bShadowMapping  = true; // : register(c3);


//--------------------------------------------------------------------------------------
// Samplers
//--------------------------------------------------------------------------------------
sampler CookieSampler           : register(s1);
sampler ScrollingNoiseSampler   : register(s2);
sampler ShadowMapSampler        : register(s3);
sampler AmbientOcclusionSampler : register(s4);
sampler AmbientCubeSampler      : register(s5);
sampler VolumeNoiseSampler      : register(s6);
sampler MarbleSplineSampler     : register(s7);


//--------------------------------------------------------------------------------------
// Vertex structures
//--------------------------------------------------------------------------------------
struct VS_OBJECT_OUTPUT
{
    float4 vPos                 : POSITION;
    float2 vBaseTexCoord        : TEXCOORD0;
    float3 vModelPos            : TEXCOORD1;
    float4 vCookieProjTexCoord  : TEXCOORD2;
    float4 vScroll1ProjTexCoord : TEXCOORD3;
    float4 vScroll2ProjTexCoord : TEXCOORD4;
    float3 vScaledModelPos      : TEXCOORD5;
    float2 vProjLightSpaceDepth : TEXCOORD6;
};

struct VS_OBJECT_DEPTH_OUTPUT
{
    float4 vPos   : POSITION;
};


//--------------------------------------------------------------------------------------
// Name: snoise()
// Desc: Signed noise routine. Given a 3D position in space of noise, this function
//       returns a signed scalar noise value in -1..1 range
//--------------------------------------------------------------------------------------
float snoise( float3 vPos )
{
    return 2.0f * tex3D( VolumeNoiseSampler, vPos ).x - 1.0f;
}


//--------------------------------------------------------------------------------------
// Name: marble()
// Desc: Simple marble routine. Given a 3D shader-space position (typically model
//       coordinates) and a scalar frequency, this function returns an albedo for
//       marble.
//--------------------------------------------------------------------------------------
float4 marble( float3 vPos, float fFrequency )
{
   float m = -2.0f * snoise( vPos * fFrequency ) + 0.75f;

   // Cubic interpolation of f along color spline (gloss in alpha)
   return tex1D( MarbleSplineSampler, m );
}


//--------------------------------------------------------------------------------------
// Name: ObjectDepthVS()
// Desc: Vertex Shader which puts interpolated depth into scalar texture coordinate
//--------------------------------------------------------------------------------------
VS_OBJECT_DEPTH_OUTPUT ObjectDepthVS( float3 vPosition : POSITION )
{
    VS_OBJECT_DEPTH_OUTPUT Out = (VS_OBJECT_DEPTH_OUTPUT)0; 

    // Output clip-space position
    Out.vPos = mul( float4(vPosition,1), g_matWorldViewProj );

    return Out;
}


//--------------------------------------------------------------------------------------
// Name: ObjectMainVS()
// Desc: Vertex Shader which sets up for Phong lighting and a projective texture
//--------------------------------------------------------------------------------------
VS_OBJECT_OUTPUT ObjectMainVS( float3 vPosition : POSITION, 
                               float2 vTexCoord : TEXCOORD0 )
{
    VS_OBJECT_OUTPUT Out = (VS_OBJECT_OUTPUT)0;

    float4 vObjectSpacePos = float4( vPosition, 1 );

    // Output clip-space position
    Out.vPos = mul( vObjectSpacePos, g_matWorldViewProj );

    Out.vModelPos = vObjectSpacePos.xyz;

    // Pass through the base map texture coordinates
    Out.vBaseTexCoord = float2( vTexCoord.x, vTexCoord.y );

    // Output projective coordinates for cookie
    Out.vCookieProjTexCoord = mul( vObjectSpacePos, g_matWorldLightProjBias );

    // Output scrolling projective coordinates for noise map
    Out.vScroll1ProjTexCoord = mul( vObjectSpacePos, g_matWorldLightProjScroll1 );
    Out.vScroll2ProjTexCoord = mul( vObjectSpacePos, g_matWorldLightProjScroll2 );

    // Position in projective light space
    float4 vLightSpacePos = mul( vObjectSpacePos, g_matWorldLightProj );

    // Output scaled model coordinates (for the marble effect)
    Out.vScaledModelPos = vPosition.xyz * float3( 0.025, 0.025, 0.1025 );

    // Output projective light space depth (for shadow mapping)
    Out.vProjLightSpaceDepth.x = vLightSpacePos.z;
    Out.vProjLightSpaceDepth.y = vLightSpacePos.w;

    return Out;
}


//--------------------------------------------------------------------------------------
// Name: ObjectNoiseShadowPS()
// Desc: Pixel shader for rendering the object with marble, noise, shadow, and a
//       spotlight with projected cookie texture
//--------------------------------------------------------------------------------------
float4 ObjectNoiseShadowPS( VS_OBJECT_OUTPUT In  ) : COLOR
{
    // Sample normal and ambient occlusion term from map
    float4 vTexel = tex2D( AmbientOcclusionSampler, In.vBaseTexCoord );
    float3 vNormal = normalize( 2*vTexel.rgb-1 );    // Model space normal
    float  fAmbientOcclusion = vTexel.a;             // Ambient occlusion term

    // Lighting
    float3 vLightDir  = normalize(g_vModelL - In.vModelPos); // Compute L in model space
    float3 vViewDir   = normalize(g_vModelV - In.vModelPos); // Compute V in model space
    float3 vHalfAngle = normalize(vLightDir + vViewDir);     // Compute half-angle

    float fNdotL   = dot(vNormal, vLightDir);             // N.L
    float fDiffuse = saturate(fNdotL - 0.5f);             // Horizon scooted to hide shadow map artifacts

    float fNdotH    = clamp( dot( vNormal, vHalfAngle ), 0.0f, 1.0f ); // Compute N.H
    float fNdotH_2  = fNdotH   * fNdotH;
    float fNdotH_4  = fNdotH_2 * fNdotH_2;
    float fNdotH_8  = fNdotH_4 * fNdotH_4;
    float fNdotH_16 = fNdotH_8 * fNdotH_8;

    // Sample the cookie
    float4 vCookie = tex2Dproj( CookieSampler, In.vCookieProjTexCoord );

    // Include the scrolling noise
    float fCompositeNoise = 1.0f;
    // if( g_bScrollingNoise )
    {
        // One noise map, but different projections and channels used
        float4 vNoise1 = tex2Dproj( ScrollingNoiseSampler, In.vScroll1ProjTexCoord );
        float4 vNoise2 = tex2Dproj( ScrollingNoiseSampler, In.vScroll2ProjTexCoord );
        fCompositeNoise = vNoise1.r + vNoise2.g;
    }

    // Perform shadow mapping. When fShadow == 0.0f it is in shadow
    float fShadow = 1.0f;
    // if( g_bShadowMapping )
    {
        float fProjLightSpaceDepth = In.vProjLightSpaceDepth.x / In.vProjLightSpaceDepth.y;
        float fShadowMapDepth      = tex2Dproj( ShadowMapSampler, In.vCookieProjTexCoord ).x;
        fShadowMapDepth += 0.001f;
        fShadow = ( fProjLightSpaceDepth < fShadowMapDepth ) ? 1.0f : 0.0f;
    }

    // Ambient cube map
    float4 vAmbientColor = fAmbientOcclusion * texCUBE( AmbientCubeSampler, vNormal );

    // Compute the albedo using a standard marble function
    float4 vAlbedo = marble( In.vScaledModelPos, 1.0f ); // gloss in alpha channel

    // Final composite of albedo and lighting
    float3 vOutput = fShadow * vCookie.xyz * ( fDiffuse * fCompositeNoise * vAlbedo.xyz + fNdotH_16 * vAlbedo.w ) + 
                     0.4f * vAmbientColor * vAlbedo.xyz;
    return float4(vOutput,1);
}


//--------------------------------------------------------------------------------------
// AsyncPrecompiledCommandBuffers.fx
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

// Transform matrices
shared float4x4 world_view_proj_matrix : register( c0 );
shared float4x4 shadow_matrix : register( c4 );
shared float uniform_scale : register( c8 ) = 1.0;

// Lighting constants (all in object space)
shared float3 g_DirLightDirection : register( c12 );
shared float4 g_DirLightColor : register( c13 );

#define POINT_LIGHT_COUNT 2
shared float4 g_PointLightPosRange[POINT_LIGHT_COUNT] : register( c14 );
shared float4 g_PointLightColor[POINT_LIGHT_COUNT] : register( c16 );

// Samplers
shared sampler diffuse_texture : register( s0 );

// Shadow map texture
sampler2D DepthTex : register(s1);

struct VS_INPUT
{
    float4  Pos     : POSITION0;
    float2  Tex     : TEXCOORD0;
    float3  Normal  : NORMAL;
};

struct VS_OUTPUT
{
    float4  Pos     : POSITION;
    float2  Tex     : TEXCOORD0;
    float3  ObjPos  : TEXCOORD1;
    float4  LightPos: TEXCOORD2;
    float3  Normal  : NORMAL;
};

struct PS_INPUT
{
    float2  Tex     : TEXCOORD0;
    float3  ObjPos  : TEXCOORD1;
    float4  LightPos: TEXCOORD2;
    float3  Normal  : NORMAL;
};

VS_OUTPUT vs_Transform( VS_INPUT In )
{
    VS_OUTPUT Out;
    float4 ScaledPos = float4( In.Pos.xyz * uniform_scale, 1 );
    Out.Pos = mul( ScaledPos, world_view_proj_matrix );
    Out.ObjPos = ScaledPos;
    Out.Tex = In.Tex;
    Out.LightPos = mul( ScaledPos, shadow_matrix );
    Out.Normal = In.Normal;
    return Out;
};

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
        tfetch2D SampledDepth.x___, vShadowCoord.xy, DepthTex, OffsetX = -0.5, OffsetY = -0.5, UseComputedLOD=false
        tfetch2D SampledDepth._x__, vShadowCoord.xy, DepthTex, OffsetX =  0.5, OffsetY = -0.5, UseComputedLOD=false
        tfetch2D SampledDepth.__x_, vShadowCoord.xy, DepthTex, OffsetX = -0.5, OffsetY =  0.5, UseComputedLOD=false
        tfetch2D SampledDepth.___x, vShadowCoord.xy, DepthTex, OffsetX =  0.5, OffsetY =  0.5, UseComputedLOD=false

        getWeights2D Weights, vShadowCoord.xy, DepthTex, MagFilter=linear, MinFilter=linear, UseComputedLOD=false
    };

    Weights = float4( (1-Weights.x)*(1-Weights.y), Weights.x*(1-Weights.y), (1-Weights.x)*Weights.y, Weights.x*Weights.y );
        
    float4 Attenuation = step( vShadowCoord.z, SampledDepth );
    
    return dot( Attenuation, Weights );
}

float4 ps_TexturePoint2Directional1( PS_INPUT In ) : COLOR
{
    float4 vTexColor = tex2D( diffuse_texture, In.Tex );
    float Attenuation = ComputeShadowAttenuationBilinear( In.LightPos.xyz / In.LightPos.w );
    float4 vLightColor = lerp( 0.3f, 1, Attenuation ) * saturate( dot( In.Normal, -g_DirLightDirection ) ) * g_DirLightColor;
    
    for( int i = 0; i < POINT_LIGHT_COUNT; ++i )
    {
        float3 vObjToLight = g_PointLightPosRange[i].xyz - In.ObjPos;
        float fRange = length( vObjToLight );
        vObjToLight /= fRange;
        
        float fDiffuse = saturate( dot( vObjToLight, In.Normal ) );
        
        float fAttenuation = saturate( 1.0f - fRange * g_PointLightPosRange[i].w );
        
        vLightColor += ( fDiffuse * fAttenuation ) * g_PointLightColor[i];
    }
    
    return vTexColor * vLightColor;
}

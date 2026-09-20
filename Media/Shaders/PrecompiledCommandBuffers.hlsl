//--------------------------------------------------------------------------------------
// PrecompiledCommandBuffers.fx
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

// Transform matrices
shared float4x4 view_proj_matrix : register( c0 );
shared float4x4 world_matrix : register( c4 );
shared float4x4 world_view_proj_matrix : register( c0 );
shared float uniform_scale : register( c8 ) = 1.0;

// Lighting constants (all in object space)
shared float3 g_DirLightDirection : register( c12 );
shared float4 g_DirLightColor : register( c13 );

#define POINT_LIGHT_COUNT 2
shared float4 g_PointLightPosRange[POINT_LIGHT_COUNT] : register( c14 );
shared float4 g_PointLightColor[POINT_LIGHT_COUNT] : register( c16 );

// Samplers
#define TEXTURE_SAMPLER sampler_state { MipFilter = LINEAR; MinFilter = LINEAR; MagFilter = LINEAR; AddressU = WRAP; AddressV = WRAP; };
shared sampler diffuse_texture : register( s0 ) = TEXTURE_SAMPLER;

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
    float3  Normal  : NORMAL;
};

struct PS_INPUT
{
    float2  Tex     : TEXCOORD0;
    float3  ObjPos  : TEXCOORD1;
    float3  Normal  : NORMAL;
};

VS_OUTPUT vs_Transform_SeparateWorld( VS_INPUT In )
{
    VS_OUTPUT Out;
    float4 ScaledPos = float4( In.Pos.xyz * uniform_scale, 1 );
    float4 vPos = mul( ScaledPos, world_matrix );
    vPos = mul( vPos, view_proj_matrix );
    Out.Pos = vPos;
    Out.ObjPos = ScaledPos;
    Out.Tex = In.Tex;
    Out.Normal = In.Normal;
    return Out;
};

VS_OUTPUT vs_Transform( VS_INPUT In )
{
    VS_OUTPUT Out;
    float4 ScaledPos = float4( In.Pos.xyz * uniform_scale, 1 );
    Out.Pos = mul( ScaledPos, world_view_proj_matrix );
    Out.ObjPos = ScaledPos;
    Out.Tex = In.Tex;
    Out.Normal = In.Normal;
    return Out;
};

float4 ps_Texture( PS_INPUT In ) : COLOR
{
    return tex2D( diffuse_texture, In.Tex );
};

float4 ps_TexturePoint2Directional1( PS_INPUT In ) : COLOR
{
    float4 vTexColor = tex2D( diffuse_texture, In.Tex );
    float4 vLightColor = saturate( dot( In.Normal, -g_DirLightDirection ) ) * g_DirLightColor;
    
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

float4 ps_TextureDirectional1( [unused] PS_INPUT In ) : COLOR
{
    float4 vTexColor = tex2D( diffuse_texture, In.Tex );
    float4 vLightColor = saturate( dot( In.Normal, -g_DirLightDirection ) ) * g_DirLightColor;
    
    return vTexColor * vLightColor;
}


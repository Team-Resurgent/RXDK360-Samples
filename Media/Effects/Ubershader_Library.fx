//--------------------------------------------------------------------------------------
// UberShader_Library.fx
//
// This effect contains the techniques and shaders for multiple light per pass lighting.
// The techniques statically compile light counts, so there is one technique for each
// configuration of lights.
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include "shadowmaps.inc"
#include "lighting.inc"

#define MAX_POINT_LIGHT_COUNT 6
#define MAX_SPOT_LIGHT_COUNT 6
#define MAX_DIR_LIGHT_COUNT 1

#define SHADOW_SAMPLER sampler_state { MipFilter = NONE; MinFilter = POINT; MagFilter = POINT; AddressU = CLAMP; AddressV = CLAMP; }                         
#define TEXTURE_SAMPLER sampler_state { MipFilter = LINEAR; MinFilter = LINEAR; MagFilter = LINEAR; AddressU = WRAP; AddressV = WRAP; }

shared float4x4 world_view_proj_matrix;
shared float4  ambient;                                 
shared float4  obj_view_direction;                      
shared float4  obj_view_position;                       
shared float4  dir_light_obj_dirs[MAX_DIR_LIGHT_COUNT];                   
shared float4   dir_light_colors[MAX_DIR_LIGHT_COUNT];                                     
shared float4x4  dir_light_shadow_proj_matrix[MAX_DIR_LIGHT_COUNT];
shared sampler2D dir_light_shadow_textures[MAX_DIR_LIGHT_COUNT] : register(s8) = { SHADOW_SAMPLER };
shared float4x4  dir_light_shadow_proj_matrix_scene[MAX_DIR_LIGHT_COUNT];
shared sampler2D dir_light_shadow_textures_scene[MAX_DIR_LIGHT_COUNT] : register(s9) = { SHADOW_SAMPLER };
shared float4  point_light_obj_pos_ranges[MAX_POINT_LIGHT_COUNT];                           
shared float4   point_light_colors[MAX_POINT_LIGHT_COUNT];                                   
shared sampler2D point_light_shadow_textures[MAX_POINT_LIGHT_COUNT];
shared float4  spot_light_obj_pos_ranges[MAX_SPOT_LIGHT_COUNT];                            
shared float4  spot_light_obj_dirs[MAX_SPOT_LIGHT_COUNT];                                  
shared float4   spot_light_colors[MAX_SPOT_LIGHT_COUNT];                                    
shared float4  spot_light_inner_outer_angles[MAX_SPOT_LIGHT_COUNT];
shared sampler2D spot_light_shadow_textures[MAX_SPOT_LIGHT_COUNT] : register(s10) = { SHADOW_SAMPLER, SHADOW_SAMPLER, SHADOW_SAMPLER, SHADOW_SAMPLER, SHADOW_SAMPLER, SHADOW_SAMPLER };
shared float4x4  spot_light_shadow_proj_matrix[MAX_SPOT_LIGHT_COUNT];

float4 cDiffuseMaterial = 1;
float4 cSpecularMaterial = 0;
float4 cEmissiveMaterial = 0;
float cSpecularExponent = 16.0f;      
sampler2D DiffuseTexture : register(s0) = TEXTURE_SAMPLER;
sampler2D SpecularMapTexture : register(s4) = TEXTURE_SAMPLER;
sampler2D NormalMapTexture : register(s5) = TEXTURE_SAMPLER;

bool bEmissive = false;

struct VS_OUTPUT
{
    float4  Pos         : POSITION;
    float2  Tex0        : TEXCOORD0;
    float3  ObjPos      : TEXCOORD1;
    float3  Tangent     : TEXCOORD2;
    float3  Binormal    : TEXCOORD3;
    float3  Normal      : TEXCOORD4;
};

struct PS_INPUT
{
    float2  Tex0        : TEXCOORD0;
    float3  ObjPos      : TEXCOORD1;
    float3  Tangent     : TEXCOORD2;
    float3  Binormal    : TEXCOORD3;
    float3  Normal      : TEXCOORD4;
};

struct VS_INPUT
{
    float4 vPosition    : POSITION0;
    float3 vNormal      : NORMAL0;
    float2 vTexCoord    : TEXCOORD0;
    float3 vTangent     : TANGENT;
    float3 vBinormal    : BINORMAL;
};



VS_OUTPUT vs_main( VS_INPUT In )
{
    VS_OUTPUT Out = (VS_OUTPUT) 0;
    
    // Transform Position
    Out.Pos = mul( In.vPosition,  world_view_proj_matrix );
    Out.ObjPos = In.vPosition.xyz;
    
    // Just pass thru the texture coords
    Out.Tex0 = In.vTexCoord;

    Out.Tangent = In.vTangent;
    Out.Binormal = In.vBinormal;
    
    Out.Normal = In.vNormal;
          
    return Out;
}


float4 ps_main( [unused] PS_INPUT In,
                uniform const bool bDirLightCount[MAX_DIR_LIGHT_COUNT],
                uniform const bool bPointLightCount[MAX_POINT_LIGHT_COUNT],
                uniform const bool bSpotLightCount[MAX_SPOT_LIGHT_COUNT]
                 ) : COLOR
{
    float4 texDiffuse = tex2D( DiffuseTexture, In.Tex0 );
    if( bEmissive )
        return texDiffuse;

    // Sample normal map
    float3 NormalMapSample = RecoverXYZFromNormalMapSample( tex2D( NormalMapTexture, In.Tex0 ) );
    
    float3 pixel_normal = BumpNormalToObjectSpace( In.Normal, In.Binormal, In.Tangent, NormalMapSample );
    
    float3 obj_view_reflected = reflect( obj_view_direction, pixel_normal );

    float4 lightDiffuse = ambient;
    float4 lightSpecular = 0;
        
    for( int i = 0; i < MAX_POINT_LIGHT_COUNT; i++ )
    {
        if( bPointLightCount[i] )
        {
            float2 LightTerms = ComputePointLight(
                                    In.ObjPos,
                                    pixel_normal,
                                    obj_view_reflected,
                                    point_light_obj_pos_ranges[i],
                                    cSpecularExponent );
            lightDiffuse += LightTerms.xxxx * point_light_colors[i];
            lightSpecular += LightTerms.yyyy * point_light_colors[i];
        }
    }
    
    // spot lights
    for( int i = 0; i < MAX_SPOT_LIGHT_COUNT; i++ )
    {
        if( bSpotLightCount[i] )
        {
            float2 LightTerms = ComputeSpotLight( 
                                    In.ObjPos,
                                    pixel_normal,
                                    obj_view_reflected,
                                    spot_light_obj_pos_ranges[i],
                                    spot_light_obj_dirs[i],
                                    spot_light_inner_outer_angles[i],
                                    cSpecularExponent,
                                    spot_light_shadow_proj_matrix[i],
                                    spot_light_shadow_textures[i] );
            lightDiffuse += LightTerms.xxxx * spot_light_colors[i];
            lightSpecular += LightTerms.yyyy * spot_light_colors[i];                                   
        }
    }
    
    // directional lights
    for( int i = 0; i < MAX_DIR_LIGHT_COUNT; i++ )
    {
        if( bDirLightCount[i] )
        {
            float2 LightTerms = ComputeDirLight( 
                                    In.ObjPos,
                                    pixel_normal,
                                    obj_view_reflected,
                                    dir_light_obj_dirs[i],
                                    cSpecularExponent,
                                    dir_light_shadow_proj_matrix[i],
                                    dir_light_shadow_textures[i],
                                    dir_light_shadow_proj_matrix_scene[i],
                                    dir_light_shadow_textures_scene[i] );
            lightDiffuse += LightTerms.xxxx * dir_light_colors[i];
            lightSpecular += LightTerms.yyyy * dir_light_colors[i];                                
        }
    }
    
    lightDiffuse.a = 1;
    lightSpecular.a = 1;
    
    lightDiffuse *= texDiffuse;
    lightSpecular *= tex2D( SpecularMapTexture, In.Tex0 );
    
    float4 ColorOut = lightDiffuse + lightSpecular;
    ColorOut.a = 1;
        
    return ColorOut;
}


static const bool b_Point0[6] = { false, false, false, false, false, false };
static const bool b_Point1[6] = { true, false, false, false, false, false };
static const bool b_Point2[6] = { true, true, false, false, false, false };
static const bool b_Point3[6] = { true, true, true, false, false, false };
static const bool b_Point4[6] = { true, true, true, true, false, false };
static const bool b_Point5[6] = { true, true, true, true, true, false };
static const bool b_Point6[6] = { true, true, true, true, true, true };

static const bool b_Spot0[6] = { false, false, false, false, false, false };
static const bool b_Spot1[6] = { true, false, false, false, false, false };
static const bool b_Spot2[6] = { true, true, false, false, false, false };
static const bool b_Spot3[6] = { true, true, true, false, false, false };
static const bool b_Spot4[6] = { true, true, true, true, false, false };
static const bool b_Spot5[6] = { true, true, true, true, true, false };
static const bool b_Spot6[6] = { true, true, true, true, true, true };

static const bool b_Dir0[1] = { false };
static const bool b_Dir1[1] = { true };
static const bool b_Dir2[1] = { true };

technique Ubershader_Dir0_Point0_Spot0
{
    pass
    {
        VertexShader = compile vs_3_0 vs_main();
        PixelShader = compile ps_3_0 ps_main( b_Dir0, b_Point0, b_Spot0 );
    }
}

technique Ubershader_Dir0_Point0_Spot1
{
    pass
    {
        VertexShader = compile vs_3_0 vs_main();
        PixelShader = compile ps_3_0 ps_main( b_Dir0, b_Point0, b_Spot1 );
    }
}

technique Ubershader_Dir0_Point0_Spot2
{
    pass
    {
        VertexShader = compile vs_3_0 vs_main();
        PixelShader = compile ps_3_0 ps_main( b_Dir0, b_Point0, b_Spot2 );
    }
}

technique Ubershader_Dir0_Point0_Spot3
{
    pass
    {
        VertexShader = compile vs_3_0 vs_main();
        PixelShader = compile ps_3_0 ps_main( b_Dir0, b_Point0, b_Spot3 );
    }
}

technique Ubershader_Dir0_Point0_Spot4
{
    pass
    {
        VertexShader = compile vs_3_0 vs_main();
        PixelShader = compile ps_3_0 ps_main( b_Dir0, b_Point0, b_Spot4 );
    }
}

technique Ubershader_Dir0_Point0_Spot5
{
    pass
    {
        VertexShader = compile vs_3_0 vs_main();
        PixelShader = compile ps_3_0 ps_main( b_Dir0, b_Point0, b_Spot5 );
    }
}

technique Ubershader_Dir0_Point0_Spot6
{
    pass
    {
        VertexShader = compile vs_3_0 vs_main();
        PixelShader = compile ps_3_0 ps_main( b_Dir0, b_Point0, b_Spot6 );
    }
}

technique Ubershader_Dir0_Point1_Spot0
{
    pass
    {
        VertexShader = compile vs_3_0 vs_main();
        PixelShader = compile ps_3_0 ps_main( b_Dir0, b_Point1, b_Spot0 );
    }
}

technique Ubershader_Dir0_Point1_Spot1
{
    pass
    {
        VertexShader = compile vs_3_0 vs_main();
        PixelShader = compile ps_3_0 ps_main( b_Dir0, b_Point1, b_Spot1 );
    }
}

technique Ubershader_Dir0_Point1_Spot2
{
    pass
    {
        VertexShader = compile vs_3_0 vs_main();
        PixelShader = compile ps_3_0 ps_main( b_Dir0, b_Point1, b_Spot2 );
    }
}

technique Ubershader_Dir0_Point1_Spot3
{
    pass
    {
        VertexShader = compile vs_3_0 vs_main();
        PixelShader = compile ps_3_0 ps_main( b_Dir0, b_Point1, b_Spot3 );
    }
}

technique Ubershader_Dir0_Point1_Spot4
{
    pass
    {
        VertexShader = compile vs_3_0 vs_main();
        PixelShader = compile ps_3_0 ps_main( b_Dir0, b_Point1, b_Spot4 );
    }
}

technique Ubershader_Dir0_Point1_Spot5
{
    pass
    {
        VertexShader = compile vs_3_0 vs_main();
        PixelShader = compile ps_3_0 ps_main( b_Dir0, b_Point1, b_Spot5 );
    }
}

technique Ubershader_Dir0_Point1_Spot6
{
    pass
    {
        VertexShader = compile vs_3_0 vs_main();
        PixelShader = compile ps_3_0 ps_main( b_Dir0, b_Point1, b_Spot6 );
    }
}

technique Ubershader_Dir0_Point2_Spot0
{
    pass
    {
        VertexShader = compile vs_3_0 vs_main();
        PixelShader = compile ps_3_0 ps_main( b_Dir0, b_Point2, b_Spot0 );
    }
}

technique Ubershader_Dir0_Point2_Spot1
{
    pass
    {
        VertexShader = compile vs_3_0 vs_main();
        PixelShader = compile ps_3_0 ps_main( b_Dir0, b_Point2, b_Spot1 );
    }
}

technique Ubershader_Dir0_Point2_Spot2
{
    pass
    {
        VertexShader = compile vs_3_0 vs_main();
        PixelShader = compile ps_3_0 ps_main( b_Dir0, b_Point2, b_Spot2 );
    }
}

technique Ubershader_Dir0_Point2_Spot3
{
    pass
    {
        VertexShader = compile vs_3_0 vs_main();
        PixelShader = compile ps_3_0 ps_main( b_Dir0, b_Point2, b_Spot3 );
    }
}

technique Ubershader_Dir0_Point2_Spot4
{
    pass
    {
        VertexShader = compile vs_3_0 vs_main();
        PixelShader = compile ps_3_0 ps_main( b_Dir0, b_Point2, b_Spot4 );
    }
}

technique Ubershader_Dir0_Point2_Spot5
{
    pass
    {
        VertexShader = compile vs_3_0 vs_main();
        PixelShader = compile ps_3_0 ps_main( b_Dir0, b_Point2, b_Spot5 );
    }
}

technique Ubershader_Dir0_Point2_Spot6
{
    pass
    {
        VertexShader = compile vs_3_0 vs_main();
        PixelShader = compile ps_3_0 ps_main( b_Dir0, b_Point2, b_Spot6 );
    }
}

technique Ubershader_Dir0_Point3_Spot0
{
    pass
    {
        VertexShader = compile vs_3_0 vs_main();
        PixelShader = compile ps_3_0 ps_main( b_Dir0, b_Point3, b_Spot0 );
    }
}

technique Ubershader_Dir0_Point3_Spot1
{
    pass
    {
        VertexShader = compile vs_3_0 vs_main();
        PixelShader = compile ps_3_0 ps_main( b_Dir0, b_Point3, b_Spot1 );
    }
}

technique Ubershader_Dir0_Point3_Spot2
{
    pass
    {
        VertexShader = compile vs_3_0 vs_main();
        PixelShader = compile ps_3_0 ps_main( b_Dir0, b_Point3, b_Spot2 );
    }
}

technique Ubershader_Dir0_Point3_Spot3
{
    pass
    {
        VertexShader = compile vs_3_0 vs_main();
        PixelShader = compile ps_3_0 ps_main( b_Dir0, b_Point3, b_Spot3 );
    }
}

technique Ubershader_Dir0_Point3_Spot4
{
    pass
    {
        VertexShader = compile vs_3_0 vs_main();
        PixelShader = compile ps_3_0 ps_main( b_Dir0, b_Point3, b_Spot4 );
    }
}

technique Ubershader_Dir0_Point3_Spot5
{
    pass
    {
        VertexShader = compile vs_3_0 vs_main();
        PixelShader = compile ps_3_0 ps_main( b_Dir0, b_Point3, b_Spot5 );
    }
}

technique Ubershader_Dir0_Point3_Spot6
{
    pass
    {
        VertexShader = compile vs_3_0 vs_main();
        PixelShader = compile ps_3_0 ps_main( b_Dir0, b_Point3, b_Spot6 );
    }
}

technique Ubershader_Dir0_Point4_Spot0
{
    pass
    {
        VertexShader = compile vs_3_0 vs_main();
        PixelShader = compile ps_3_0 ps_main( b_Dir0, b_Point4, b_Spot0 );
    }
}

technique Ubershader_Dir0_Point4_Spot1
{
    pass
    {
        VertexShader = compile vs_3_0 vs_main();
        PixelShader = compile ps_3_0 ps_main( b_Dir0, b_Point4, b_Spot1 );
    }
}

technique Ubershader_Dir0_Point4_Spot2
{
    pass
    {
        VertexShader = compile vs_3_0 vs_main();
        PixelShader = compile ps_3_0 ps_main( b_Dir0, b_Point4, b_Spot2 );
    }
}

technique Ubershader_Dir0_Point4_Spot3
{
    pass
    {
        VertexShader = compile vs_3_0 vs_main();
        PixelShader = compile ps_3_0 ps_main( b_Dir0, b_Point4, b_Spot3 );
    }
}

technique Ubershader_Dir0_Point4_Spot4
{
    pass
    {
        VertexShader = compile vs_3_0 vs_main();
        PixelShader = compile ps_3_0 ps_main( b_Dir0, b_Point4, b_Spot4 );
    }
}

technique Ubershader_Dir0_Point4_Spot5
{
    pass
    {
        VertexShader = compile vs_3_0 vs_main();
        PixelShader = compile ps_3_0 ps_main( b_Dir0, b_Point4, b_Spot5 );
    }
}

technique Ubershader_Dir0_Point4_Spot6
{
    pass
    {
        VertexShader = compile vs_3_0 vs_main();
        PixelShader = compile ps_3_0 ps_main( b_Dir0, b_Point4, b_Spot6 );
    }
}

technique Ubershader_Dir0_Point5_Spot0
{
    pass
    {
        VertexShader = compile vs_3_0 vs_main();
        PixelShader = compile ps_3_0 ps_main( b_Dir0, b_Point5, b_Spot0 );
    }
}

technique Ubershader_Dir0_Point5_Spot1
{
    pass
    {
        VertexShader = compile vs_3_0 vs_main();
        PixelShader = compile ps_3_0 ps_main( b_Dir0, b_Point5, b_Spot1 );
    }
}

technique Ubershader_Dir0_Point5_Spot2
{
    pass
    {
        VertexShader = compile vs_3_0 vs_main();
        PixelShader = compile ps_3_0 ps_main( b_Dir0, b_Point5, b_Spot2 );
    }
}

technique Ubershader_Dir0_Point5_Spot3
{
    pass
    {
        VertexShader = compile vs_3_0 vs_main();
        PixelShader = compile ps_3_0 ps_main( b_Dir0, b_Point5, b_Spot3 );
    }
}

technique Ubershader_Dir0_Point5_Spot4
{
    pass
    {
        VertexShader = compile vs_3_0 vs_main();
        PixelShader = compile ps_3_0 ps_main( b_Dir0, b_Point5, b_Spot4 );
    }
}

technique Ubershader_Dir0_Point5_Spot5
{
    pass
    {
        VertexShader = compile vs_3_0 vs_main();
        PixelShader = compile ps_3_0 ps_main( b_Dir0, b_Point5, b_Spot5 );
    }
}

technique Ubershader_Dir0_Point5_Spot6
{
    pass
    {
        VertexShader = compile vs_3_0 vs_main();
        PixelShader = compile ps_3_0 ps_main( b_Dir0, b_Point5, b_Spot6 );
    }
}

technique Ubershader_Dir0_Point6_Spot0
{
    pass
    {
        VertexShader = compile vs_3_0 vs_main();
        PixelShader = compile ps_3_0 ps_main( b_Dir0, b_Point6, b_Spot0 );
    }
}

technique Ubershader_Dir0_Point6_Spot1
{
    pass
    {
        VertexShader = compile vs_3_0 vs_main();
        PixelShader = compile ps_3_0 ps_main( b_Dir0, b_Point6, b_Spot1 );
    }
}

technique Ubershader_Dir0_Point6_Spot2
{
    pass
    {
        VertexShader = compile vs_3_0 vs_main();
        PixelShader = compile ps_3_0 ps_main( b_Dir0, b_Point6, b_Spot2 );
    }
}

technique Ubershader_Dir0_Point6_Spot3
{
    pass
    {
        VertexShader = compile vs_3_0 vs_main();
        PixelShader = compile ps_3_0 ps_main( b_Dir0, b_Point6, b_Spot3 );
    }
}

technique Ubershader_Dir0_Point6_Spot4
{
    pass
    {
        VertexShader = compile vs_3_0 vs_main();
        PixelShader = compile ps_3_0 ps_main( b_Dir0, b_Point6, b_Spot4 );
    }
}

technique Ubershader_Dir0_Point6_Spot5
{
    pass
    {
        VertexShader = compile vs_3_0 vs_main();
        PixelShader = compile ps_3_0 ps_main( b_Dir0, b_Point6, b_Spot5 );
    }
}

technique Ubershader_Dir0_Point6_Spot6
{
    pass
    {
        VertexShader = compile vs_3_0 vs_main();
        PixelShader = compile ps_3_0 ps_main( b_Dir0, b_Point6, b_Spot6 );
    }
}

technique Ubershader_Dir1_Point0_Spot0
{
    pass
    {
        VertexShader = compile vs_3_0 vs_main();
        PixelShader = compile ps_3_0 ps_main( b_Dir1, b_Point0, b_Spot0 );
    }
}

technique Ubershader_Dir1_Point0_Spot1
{
    pass
    {
        VertexShader = compile vs_3_0 vs_main();
        PixelShader = compile ps_3_0 ps_main( b_Dir1, b_Point0, b_Spot1 );
    }
}

technique Ubershader_Dir1_Point0_Spot2
{
    pass
    {
        VertexShader = compile vs_3_0 vs_main();
        PixelShader = compile ps_3_0 ps_main( b_Dir1, b_Point0, b_Spot2 );
    }
}

technique Ubershader_Dir1_Point0_Spot3
{
    pass
    {
        VertexShader = compile vs_3_0 vs_main();
        PixelShader = compile ps_3_0 ps_main( b_Dir1, b_Point0, b_Spot3 );
    }
}

technique Ubershader_Dir1_Point0_Spot4
{
    pass
    {
        VertexShader = compile vs_3_0 vs_main();
        PixelShader = compile ps_3_0 ps_main( b_Dir1, b_Point0, b_Spot4 );
    }
}

technique Ubershader_Dir1_Point0_Spot5
{
    pass
    {
        VertexShader = compile vs_3_0 vs_main();
        PixelShader = compile ps_3_0 ps_main( b_Dir1, b_Point0, b_Spot5 );
    }
}

technique Ubershader_Dir1_Point0_Spot6
{
    pass
    {
        VertexShader = compile vs_3_0 vs_main();
        PixelShader = compile ps_3_0 ps_main( b_Dir1, b_Point0, b_Spot6 );
    }
}

technique Ubershader_Dir1_Point1_Spot0
{
    pass
    {
        VertexShader = compile vs_3_0 vs_main();
        PixelShader = compile ps_3_0 ps_main( b_Dir1, b_Point1, b_Spot0 );
    }
}

technique Ubershader_Dir1_Point1_Spot1
{
    pass
    {
        VertexShader = compile vs_3_0 vs_main();
        PixelShader = compile ps_3_0 ps_main( b_Dir1, b_Point1, b_Spot1 );
    }
}

technique Ubershader_Dir1_Point1_Spot2
{
    pass
    {
        VertexShader = compile vs_3_0 vs_main();
        PixelShader = compile ps_3_0 ps_main( b_Dir1, b_Point1, b_Spot2 );
    }
}

technique Ubershader_Dir1_Point1_Spot3
{
    pass
    {
        VertexShader = compile vs_3_0 vs_main();
        PixelShader = compile ps_3_0 ps_main( b_Dir1, b_Point1, b_Spot3 );
    }
}

technique Ubershader_Dir1_Point1_Spot4
{
    pass
    {
        VertexShader = compile vs_3_0 vs_main();
        PixelShader = compile ps_3_0 ps_main( b_Dir1, b_Point1, b_Spot4 );
    }
}

technique Ubershader_Dir1_Point1_Spot5
{
    pass
    {
        VertexShader = compile vs_3_0 vs_main();
        PixelShader = compile ps_3_0 ps_main( b_Dir1, b_Point1, b_Spot5 );
    }
}

technique Ubershader_Dir1_Point1_Spot6
{
    pass
    {
        VertexShader = compile vs_3_0 vs_main();
        PixelShader = compile ps_3_0 ps_main( b_Dir1, b_Point1, b_Spot6 );
    }
}

technique Ubershader_Dir1_Point2_Spot0
{
    pass
    {
        VertexShader = compile vs_3_0 vs_main();
        PixelShader = compile ps_3_0 ps_main( b_Dir1, b_Point2, b_Spot0 );
    }
}

technique Ubershader_Dir1_Point2_Spot1
{
    pass
    {
        VertexShader = compile vs_3_0 vs_main();
        PixelShader = compile ps_3_0 ps_main( b_Dir1, b_Point2, b_Spot1 );
    }
}

technique Ubershader_Dir1_Point2_Spot2
{
    pass
    {
        VertexShader = compile vs_3_0 vs_main();
        PixelShader = compile ps_3_0 ps_main( b_Dir1, b_Point2, b_Spot2 );
    }
}

technique Ubershader_Dir1_Point2_Spot3
{
    pass
    {
        VertexShader = compile vs_3_0 vs_main();
        PixelShader = compile ps_3_0 ps_main( b_Dir1, b_Point2, b_Spot3 );
    }
}

technique Ubershader_Dir1_Point2_Spot4
{
    pass
    {
        VertexShader = compile vs_3_0 vs_main();
        PixelShader = compile ps_3_0 ps_main( b_Dir1, b_Point2, b_Spot4 );
    }
}

technique Ubershader_Dir1_Point2_Spot5
{
    pass
    {
        VertexShader = compile vs_3_0 vs_main();
        PixelShader = compile ps_3_0 ps_main( b_Dir1, b_Point2, b_Spot5 );
    }
}

technique Ubershader_Dir1_Point2_Spot6
{
    pass
    {
        VertexShader = compile vs_3_0 vs_main();
        PixelShader = compile ps_3_0 ps_main( b_Dir1, b_Point2, b_Spot6 );
    }
}

technique Ubershader_Dir1_Point3_Spot0
{
    pass
    {
        VertexShader = compile vs_3_0 vs_main();
        PixelShader = compile ps_3_0 ps_main( b_Dir1, b_Point3, b_Spot0 );
    }
}

technique Ubershader_Dir1_Point3_Spot1
{
    pass
    {
        VertexShader = compile vs_3_0 vs_main();
        PixelShader = compile ps_3_0 ps_main( b_Dir1, b_Point3, b_Spot1 );
    }
}

technique Ubershader_Dir1_Point3_Spot2
{
    pass
    {
        VertexShader = compile vs_3_0 vs_main();
        PixelShader = compile ps_3_0 ps_main( b_Dir1, b_Point3, b_Spot2 );
    }
}

technique Ubershader_Dir1_Point3_Spot3
{
    pass
    {
        VertexShader = compile vs_3_0 vs_main();
        PixelShader = compile ps_3_0 ps_main( b_Dir1, b_Point3, b_Spot3 );
    }
}

technique Ubershader_Dir1_Point3_Spot4
{
    pass
    {
        VertexShader = compile vs_3_0 vs_main();
        PixelShader = compile ps_3_0 ps_main( b_Dir1, b_Point3, b_Spot4 );
    }
}

technique Ubershader_Dir1_Point3_Spot5
{
    pass
    {
        VertexShader = compile vs_3_0 vs_main();
        PixelShader = compile ps_3_0 ps_main( b_Dir1, b_Point3, b_Spot5 );
    }
}

technique Ubershader_Dir1_Point3_Spot6
{
    pass
    {
        VertexShader = compile vs_3_0 vs_main();
        PixelShader = compile ps_3_0 ps_main( b_Dir1, b_Point3, b_Spot6 );
    }
}

technique Ubershader_Dir1_Point4_Spot0
{
    pass
    {
        VertexShader = compile vs_3_0 vs_main();
        PixelShader = compile ps_3_0 ps_main( b_Dir1, b_Point4, b_Spot0 );
    }
}

technique Ubershader_Dir1_Point4_Spot1
{
    pass
    {
        VertexShader = compile vs_3_0 vs_main();
        PixelShader = compile ps_3_0 ps_main( b_Dir1, b_Point4, b_Spot1 );
    }
}

technique Ubershader_Dir1_Point4_Spot2
{
    pass
    {
        VertexShader = compile vs_3_0 vs_main();
        PixelShader = compile ps_3_0 ps_main( b_Dir1, b_Point4, b_Spot2 );
    }
}

technique Ubershader_Dir1_Point4_Spot3
{
    pass
    {
        VertexShader = compile vs_3_0 vs_main();
        PixelShader = compile ps_3_0 ps_main( b_Dir1, b_Point4, b_Spot3 );
    }
}

technique Ubershader_Dir1_Point4_Spot4
{
    pass
    {
        VertexShader = compile vs_3_0 vs_main();
        PixelShader = compile ps_3_0 ps_main( b_Dir1, b_Point4, b_Spot4 );
    }
}

technique Ubershader_Dir1_Point4_Spot5
{
    pass
    {
        VertexShader = compile vs_3_0 vs_main();
        PixelShader = compile ps_3_0 ps_main( b_Dir1, b_Point4, b_Spot5 );
    }
}

technique Ubershader_Dir1_Point4_Spot6
{
    pass
    {
        VertexShader = compile vs_3_0 vs_main();
        PixelShader = compile ps_3_0 ps_main( b_Dir1, b_Point4, b_Spot6 );
    }
}

technique Ubershader_Dir1_Point5_Spot0
{
    pass
    {
        VertexShader = compile vs_3_0 vs_main();
        PixelShader = compile ps_3_0 ps_main( b_Dir1, b_Point5, b_Spot0 );
    }
}

technique Ubershader_Dir1_Point5_Spot1
{
    pass
    {
        VertexShader = compile vs_3_0 vs_main();
        PixelShader = compile ps_3_0 ps_main( b_Dir1, b_Point5, b_Spot1 );
    }
}

technique Ubershader_Dir1_Point5_Spot2
{
    pass
    {
        VertexShader = compile vs_3_0 vs_main();
        PixelShader = compile ps_3_0 ps_main( b_Dir1, b_Point5, b_Spot2 );
    }
}

technique Ubershader_Dir1_Point5_Spot3
{
    pass
    {
        VertexShader = compile vs_3_0 vs_main();
        PixelShader = compile ps_3_0 ps_main( b_Dir1, b_Point5, b_Spot3 );
    }
}

technique Ubershader_Dir1_Point5_Spot4
{
    pass
    {
        VertexShader = compile vs_3_0 vs_main();
        PixelShader = compile ps_3_0 ps_main( b_Dir1, b_Point5, b_Spot4 );
    }
}

technique Ubershader_Dir1_Point5_Spot5
{
    pass
    {
        VertexShader = compile vs_3_0 vs_main();
        PixelShader = compile ps_3_0 ps_main( b_Dir1, b_Point5, b_Spot5 );
    }
}

technique Ubershader_Dir1_Point5_Spot6
{
    pass
    {
        VertexShader = compile vs_3_0 vs_main();
        PixelShader = compile ps_3_0 ps_main( b_Dir1, b_Point5, b_Spot6 );
    }
}

technique Ubershader_Dir1_Point6_Spot0
{
    pass
    {
        VertexShader = compile vs_3_0 vs_main();
        PixelShader = compile ps_3_0 ps_main( b_Dir1, b_Point6, b_Spot0 );
    }
}

technique Ubershader_Dir1_Point6_Spot1
{
    pass
    {
        VertexShader = compile vs_3_0 vs_main();
        PixelShader = compile ps_3_0 ps_main( b_Dir1, b_Point6, b_Spot1 );
    }
}

technique Ubershader_Dir1_Point6_Spot2
{
    pass
    {
        VertexShader = compile vs_3_0 vs_main();
        PixelShader = compile ps_3_0 ps_main( b_Dir1, b_Point6, b_Spot2 );
    }
}

technique Ubershader_Dir1_Point6_Spot3
{
    pass
    {
        VertexShader = compile vs_3_0 vs_main();
        PixelShader = compile ps_3_0 ps_main( b_Dir1, b_Point6, b_Spot3 );
    }
}

technique Ubershader_Dir1_Point6_Spot4
{
    pass
    {
        VertexShader = compile vs_3_0 vs_main();
        PixelShader = compile ps_3_0 ps_main( b_Dir1, b_Point6, b_Spot4 );
    }
}

technique Ubershader_Dir1_Point6_Spot5
{
    pass
    {
        VertexShader = compile vs_3_0 vs_main();
        PixelShader = compile ps_3_0 ps_main( b_Dir1, b_Point6, b_Spot5 );
    }
}

technique Ubershader_Dir1_Point6_Spot6
{
    pass
    {
        VertexShader = compile vs_3_0 vs_main();
        PixelShader = compile ps_3_0 ps_main( b_Dir1, b_Point6, b_Spot6 );
    }
}

technique Ubershader_Dir2_Point0_Spot0
{
    pass
    {
        VertexShader = compile vs_3_0 vs_main();
        PixelShader = compile ps_3_0 ps_main( b_Dir2, b_Point0, b_Spot0 );
    }
}

technique Ubershader_Dir2_Point0_Spot1
{
    pass
    {
        VertexShader = compile vs_3_0 vs_main();
        PixelShader = compile ps_3_0 ps_main( b_Dir2, b_Point0, b_Spot1 );
    }
}

technique Ubershader_Dir2_Point0_Spot2
{
    pass
    {
        VertexShader = compile vs_3_0 vs_main();
        PixelShader = compile ps_3_0 ps_main( b_Dir2, b_Point0, b_Spot2 );
    }
}

technique Ubershader_Dir2_Point0_Spot3
{
    pass
    {
        VertexShader = compile vs_3_0 vs_main();
        PixelShader = compile ps_3_0 ps_main( b_Dir2, b_Point0, b_Spot3 );
    }
}

technique Ubershader_Dir2_Point0_Spot4
{
    pass
    {
        VertexShader = compile vs_3_0 vs_main();
        PixelShader = compile ps_3_0 ps_main( b_Dir2, b_Point0, b_Spot4 );
    }
}

technique Ubershader_Dir2_Point0_Spot5
{
    pass
    {
        VertexShader = compile vs_3_0 vs_main();
        PixelShader = compile ps_3_0 ps_main( b_Dir2, b_Point0, b_Spot5 );
    }
}

technique Ubershader_Dir2_Point0_Spot6
{
    pass
    {
        VertexShader = compile vs_3_0 vs_main();
        PixelShader = compile ps_3_0 ps_main( b_Dir2, b_Point0, b_Spot6 );
    }
}

technique Ubershader_Dir2_Point1_Spot0
{
    pass
    {
        VertexShader = compile vs_3_0 vs_main();
        PixelShader = compile ps_3_0 ps_main( b_Dir2, b_Point1, b_Spot0 );
    }
}

technique Ubershader_Dir2_Point1_Spot1
{
    pass
    {
        VertexShader = compile vs_3_0 vs_main();
        PixelShader = compile ps_3_0 ps_main( b_Dir2, b_Point1, b_Spot1 );
    }
}

technique Ubershader_Dir2_Point1_Spot2
{
    pass
    {
        VertexShader = compile vs_3_0 vs_main();
        PixelShader = compile ps_3_0 ps_main( b_Dir2, b_Point1, b_Spot2 );
    }
}

technique Ubershader_Dir2_Point1_Spot3
{
    pass
    {
        VertexShader = compile vs_3_0 vs_main();
        PixelShader = compile ps_3_0 ps_main( b_Dir2, b_Point1, b_Spot3 );
    }
}

technique Ubershader_Dir2_Point1_Spot4
{
    pass
    {
        VertexShader = compile vs_3_0 vs_main();
        PixelShader = compile ps_3_0 ps_main( b_Dir2, b_Point1, b_Spot4 );
    }
}

technique Ubershader_Dir2_Point1_Spot5
{
    pass
    {
        VertexShader = compile vs_3_0 vs_main();
        PixelShader = compile ps_3_0 ps_main( b_Dir2, b_Point1, b_Spot5 );
    }
}

technique Ubershader_Dir2_Point1_Spot6
{
    pass
    {
        VertexShader = compile vs_3_0 vs_main();
        PixelShader = compile ps_3_0 ps_main( b_Dir2, b_Point1, b_Spot6 );
    }
}

technique Ubershader_Dir2_Point2_Spot0
{
    pass
    {
        VertexShader = compile vs_3_0 vs_main();
        PixelShader = compile ps_3_0 ps_main( b_Dir2, b_Point2, b_Spot0 );
    }
}

technique Ubershader_Dir2_Point2_Spot1
{
    pass
    {
        VertexShader = compile vs_3_0 vs_main();
        PixelShader = compile ps_3_0 ps_main( b_Dir2, b_Point2, b_Spot1 );
    }
}

technique Ubershader_Dir2_Point2_Spot2
{
    pass
    {
        VertexShader = compile vs_3_0 vs_main();
        PixelShader = compile ps_3_0 ps_main( b_Dir2, b_Point2, b_Spot2 );
    }
}

technique Ubershader_Dir2_Point2_Spot3
{
    pass
    {
        VertexShader = compile vs_3_0 vs_main();
        PixelShader = compile ps_3_0 ps_main( b_Dir2, b_Point2, b_Spot3 );
    }
}

technique Ubershader_Dir2_Point2_Spot4
{
    pass
    {
        VertexShader = compile vs_3_0 vs_main();
        PixelShader = compile ps_3_0 ps_main( b_Dir2, b_Point2, b_Spot4 );
    }
}

technique Ubershader_Dir2_Point2_Spot5
{
    pass
    {
        VertexShader = compile vs_3_0 vs_main();
        PixelShader = compile ps_3_0 ps_main( b_Dir2, b_Point2, b_Spot5 );
    }
}

technique Ubershader_Dir2_Point2_Spot6
{
    pass
    {
        VertexShader = compile vs_3_0 vs_main();
        PixelShader = compile ps_3_0 ps_main( b_Dir2, b_Point2, b_Spot6 );
    }
}

technique Ubershader_Dir2_Point3_Spot0
{
    pass
    {
        VertexShader = compile vs_3_0 vs_main();
        PixelShader = compile ps_3_0 ps_main( b_Dir2, b_Point3, b_Spot0 );
    }
}

technique Ubershader_Dir2_Point3_Spot1
{
    pass
    {
        VertexShader = compile vs_3_0 vs_main();
        PixelShader = compile ps_3_0 ps_main( b_Dir2, b_Point3, b_Spot1 );
    }
}

technique Ubershader_Dir2_Point3_Spot2
{
    pass
    {
        VertexShader = compile vs_3_0 vs_main();
        PixelShader = compile ps_3_0 ps_main( b_Dir2, b_Point3, b_Spot2 );
    }
}

technique Ubershader_Dir2_Point3_Spot3
{
    pass
    {
        VertexShader = compile vs_3_0 vs_main();
        PixelShader = compile ps_3_0 ps_main( b_Dir2, b_Point3, b_Spot3 );
    }
}

technique Ubershader_Dir2_Point3_Spot4
{
    pass
    {
        VertexShader = compile vs_3_0 vs_main();
        PixelShader = compile ps_3_0 ps_main( b_Dir2, b_Point3, b_Spot4 );
    }
}

technique Ubershader_Dir2_Point3_Spot5
{
    pass
    {
        VertexShader = compile vs_3_0 vs_main();
        PixelShader = compile ps_3_0 ps_main( b_Dir2, b_Point3, b_Spot5 );
    }
}

technique Ubershader_Dir2_Point3_Spot6
{
    pass
    {
        VertexShader = compile vs_3_0 vs_main();
        PixelShader = compile ps_3_0 ps_main( b_Dir2, b_Point3, b_Spot6 );
    }
}

technique Ubershader_Dir2_Point4_Spot0
{
    pass
    {
        VertexShader = compile vs_3_0 vs_main();
        PixelShader = compile ps_3_0 ps_main( b_Dir2, b_Point4, b_Spot0 );
    }
}

technique Ubershader_Dir2_Point4_Spot1
{
    pass
    {
        VertexShader = compile vs_3_0 vs_main();
        PixelShader = compile ps_3_0 ps_main( b_Dir2, b_Point4, b_Spot1 );
    }
}

technique Ubershader_Dir2_Point4_Spot2
{
    pass
    {
        VertexShader = compile vs_3_0 vs_main();
        PixelShader = compile ps_3_0 ps_main( b_Dir2, b_Point4, b_Spot2 );
    }
}

technique Ubershader_Dir2_Point4_Spot3
{
    pass
    {
        VertexShader = compile vs_3_0 vs_main();
        PixelShader = compile ps_3_0 ps_main( b_Dir2, b_Point4, b_Spot3 );
    }
}

technique Ubershader_Dir2_Point4_Spot4
{
    pass
    {
        VertexShader = compile vs_3_0 vs_main();
        PixelShader = compile ps_3_0 ps_main( b_Dir2, b_Point4, b_Spot4 );
    }
}

technique Ubershader_Dir2_Point4_Spot5
{
    pass
    {
        VertexShader = compile vs_3_0 vs_main();
        PixelShader = compile ps_3_0 ps_main( b_Dir2, b_Point4, b_Spot5 );
    }
}

technique Ubershader_Dir2_Point4_Spot6
{
    pass
    {
        VertexShader = compile vs_3_0 vs_main();
        PixelShader = compile ps_3_0 ps_main( b_Dir2, b_Point4, b_Spot6 );
    }
}

technique Ubershader_Dir2_Point5_Spot0
{
    pass
    {
        VertexShader = compile vs_3_0 vs_main();
        PixelShader = compile ps_3_0 ps_main( b_Dir2, b_Point5, b_Spot0 );
    }
}

technique Ubershader_Dir2_Point5_Spot1
{
    pass
    {
        VertexShader = compile vs_3_0 vs_main();
        PixelShader = compile ps_3_0 ps_main( b_Dir2, b_Point5, b_Spot1 );
    }
}

technique Ubershader_Dir2_Point5_Spot2
{
    pass
    {
        VertexShader = compile vs_3_0 vs_main();
        PixelShader = compile ps_3_0 ps_main( b_Dir2, b_Point5, b_Spot2 );
    }
}

technique Ubershader_Dir2_Point5_Spot3
{
    pass
    {
        VertexShader = compile vs_3_0 vs_main();
        PixelShader = compile ps_3_0 ps_main( b_Dir2, b_Point5, b_Spot3 );
    }
}

technique Ubershader_Dir2_Point5_Spot4
{
    pass
    {
        VertexShader = compile vs_3_0 vs_main();
        PixelShader = compile ps_3_0 ps_main( b_Dir2, b_Point5, b_Spot4 );
    }
}

technique Ubershader_Dir2_Point5_Spot5
{
    pass
    {
        VertexShader = compile vs_3_0 vs_main();
        PixelShader = compile ps_3_0 ps_main( b_Dir2, b_Point5, b_Spot5 );
    }
}

technique Ubershader_Dir2_Point5_Spot6
{
    pass
    {
        VertexShader = compile vs_3_0 vs_main();
        PixelShader = compile ps_3_0 ps_main( b_Dir2, b_Point5, b_Spot6 );
    }
}

technique Ubershader_Dir2_Point6_Spot0
{
    pass
    {
        VertexShader = compile vs_3_0 vs_main();
        PixelShader = compile ps_3_0 ps_main( b_Dir2, b_Point6, b_Spot0 );
    }
}

technique Ubershader_Dir2_Point6_Spot1
{
    pass
    {
        VertexShader = compile vs_3_0 vs_main();
        PixelShader = compile ps_3_0 ps_main( b_Dir2, b_Point6, b_Spot1 );
    }
}

technique Ubershader_Dir2_Point6_Spot2
{
    pass
    {
        VertexShader = compile vs_3_0 vs_main();
        PixelShader = compile ps_3_0 ps_main( b_Dir2, b_Point6, b_Spot2 );
    }
}

technique Ubershader_Dir2_Point6_Spot3
{
    pass
    {
        VertexShader = compile vs_3_0 vs_main();
        PixelShader = compile ps_3_0 ps_main( b_Dir2, b_Point6, b_Spot3 );
    }
}

technique Ubershader_Dir2_Point6_Spot4
{
    pass
    {
        VertexShader = compile vs_3_0 vs_main();
        PixelShader = compile ps_3_0 ps_main( b_Dir2, b_Point6, b_Spot4 );
    }
}

technique Ubershader_Dir2_Point6_Spot5
{
    pass
    {
        VertexShader = compile vs_3_0 vs_main();
        PixelShader = compile ps_3_0 ps_main( b_Dir2, b_Point6, b_Spot5 );
    }
}

technique Ubershader_Dir2_Point6_Spot6
{
    pass
    {
        VertexShader = compile vs_3_0 vs_main();
        PixelShader = compile ps_3_0 ps_main( b_Dir2, b_Point6, b_Spot6 );
    }
}


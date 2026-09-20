//--------------------------------------------------------------------------------------
// UberShader_Final.fx
//
// This effect contains the techniques and shaders for multiple light per pass lighting.
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

float cSpecularExponent = 16.0f;      

// Diffuse texture stages.  Note that only the multitex shader uses the last 3 stages.
sampler2D DiffuseTexture : register(s0) = TEXTURE_SAMPLER;
sampler2D DiffuseTexture1 : register(s1) = TEXTURE_SAMPLER;
sampler2D DiffuseTexture2 : register(s2) = TEXTURE_SAMPLER;
sampler2D DiffuseTexture3 : register(s3) = TEXTURE_SAMPLER;
float DiffuseTextureUVIndex = 0;
float DiffuseTexture1UVIndex = 0;
float DiffuseTexture2UVIndex = 0;
float DiffuseTexture3UVIndex = 0;
bool DiffuseLayerEnabled = true;
bool DiffuseLayer1Enabled = false;
bool DiffuseLayer2Enabled = false;
bool DiffuseLayer3Enabled = false;

// Specular map texture stage.
sampler2D SpecularMapTexture : register(s4) = TEXTURE_SAMPLER;

// Normal map texture stages.  Only the multitex shader uses the last 2 stages.
sampler2D NormalMapTexture : register(s5) = TEXTURE_SAMPLER;
sampler2D NormalMapTexture1 : register(s6) = TEXTURE_SAMPLER;
sampler2D NormalMapTexture2 : register(s7) = TEXTURE_SAMPLER;
float NormalMapTextureUVIndex = 0;
float NormalMapTexture1UVIndex = 0;
float NormalMapTexture2UVIndex = 0;
bool NormalMapLayerEnabled = true;
bool NormalMapLayer1Enabled = false;
bool NormalMapLayer2Enabled = false;

shared bool bPointLightCount[MAX_POINT_LIGHT_COUNT];
shared bool bSpotLightCount[MAX_SPOT_LIGHT_COUNT];
shared bool bDirLightCount[MAX_DIR_LIGHT_COUNT];
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
};


struct VS_OUTPUT_MULTITEX
{
    float4  Pos         : POSITION;
    float4  Tex01       : TEXCOORD0;
    float3  ObjPos      : TEXCOORD1;
    float3  Tangent     : TEXCOORD2;
    float3  Binormal    : TEXCOORD3;
    float3  Normal      : TEXCOORD4;
};

struct PS_INPUT_MULTITEX
{
    float4  Tex01       : TEXCOORD0;
    float3  ObjPos      : TEXCOORD1;
    float3  Tangent     : TEXCOORD2;
    float3  Binormal    : TEXCOORD3;
    float3  Normal      : TEXCOORD4;
};

struct VS_INPUT_MULTITEX
{
    float4 vPosition    : POSITION0;
    float3 vNormal      : NORMAL0;
    float2 vTexCoord0   : TEXCOORD0;
    float2 vTexCoord1   : TEXCOORD1;
    float3 vTangent     : TANGENT;
};


VS_OUTPUT vs_main( VS_INPUT In )
{
    VS_OUTPUT Out = (VS_OUTPUT) 0;
    
    // Transform Position
    [isolate]
    {
        Out.Pos = mul( In.vPosition,  world_view_proj_matrix );
    }
    Out.ObjPos = In.vPosition.xyz;
        
    // Just pass thru the texture coords
    Out.Tex0 = In.vTexCoord;

    Out.Tangent = In.vTangent;
    Out.Binormal = cross( In.vNormal, In.vTangent );
    
    Out.Normal = In.vNormal;
          
    return Out;
}


VS_OUTPUT_MULTITEX vs_main_multitexture( VS_INPUT_MULTITEX In )
{
    VS_OUTPUT_MULTITEX Out;
    
    // Transform Position
    [isolate]
    {
        Out.Pos = mul( In.vPosition,  world_view_proj_matrix );
    }
    Out.ObjPos = In.vPosition.xyz;
        
    // Just pass thru the texture coords
    Out.Tex01.xy = In.vTexCoord0;
    Out.Tex01.zw = In.vTexCoord1;

    Out.Tangent = In.vTangent;
    Out.Binormal = cross( In.vNormal, In.vTangent );
    
    Out.Normal = In.vNormal;
          
    return Out;
}


float4 ps_main_nested( PS_INPUT In,
                       uniform const bool bPointLightBool[MAX_POINT_LIGHT_COUNT],
                       uniform const bool bSpotLightBool[MAX_SPOT_LIGHT_COUNT],
                       uniform const bool bDirLightBool[MAX_DIR_LIGHT_COUNT]
                       ) : COLOR
{
    float4 texDiffuse = tex2D( DiffuseTexture, In.Tex0 );
    float  texSpecular = tex2D( SpecularMapTexture, In.Tex0 ).r;
    
    if( bEmissive )
        return texDiffuse;
    
    float4 test = 0;
    
    // Sample normal map
    float3 NormalMapSample = RecoverXYZFromNormalMapSample( tex2D( NormalMapTexture, In.Tex0 ) );
    
    float3 pixel_normal = BumpNormalToObjectSpace( In.Normal, In.Binormal, In.Tangent, NormalMapSample );
    
    float3 obj_view_reflected = reflect( obj_view_direction, pixel_normal );

    float4 lightDiffuse = ambient;
    float4 lightSpecular = 0;
    if( bPointLightBool[0] )
    {
        float2 LightTerms = ComputePointLight(
                                In.ObjPos,
                                pixel_normal,
                                obj_view_reflected,
                                point_light_obj_pos_ranges[0],
                                cSpecularExponent );
        lightDiffuse += LightTerms.xxxx * point_light_colors[0];
        lightSpecular += LightTerms.yyyy * point_light_colors[0];
        if( bPointLightBool[1] )
        {
            float2 LightTerms = ComputePointLight(
                                    In.ObjPos,
                                    pixel_normal,
                                    obj_view_reflected,
                                    point_light_obj_pos_ranges[1],
                                    cSpecularExponent );
            lightDiffuse += LightTerms.xxxx * point_light_colors[1];
            lightSpecular += LightTerms.yyyy * point_light_colors[1];
            if( bPointLightBool[2] )
            {
                float2 LightTerms = ComputePointLight(
                                        In.ObjPos,
                                        pixel_normal,
                                        obj_view_reflected,
                                        point_light_obj_pos_ranges[2],
                                        cSpecularExponent );
                lightDiffuse += LightTerms.xxxx * point_light_colors[2];
                lightSpecular += LightTerms.yyyy * point_light_colors[2];
                if( bPointLightBool[3] )
                {
                    float2 LightTerms = ComputePointLight(
                                            In.ObjPos,
                                            pixel_normal,
                                            obj_view_reflected,
                                            point_light_obj_pos_ranges[3],
                                            cSpecularExponent );
                    lightDiffuse += LightTerms.xxxx * point_light_colors[3];
                    lightSpecular += LightTerms.yyyy * point_light_colors[3];
                    if( bPointLightBool[4] )
                    {
                        float2 LightTerms = ComputePointLight(
                                                In.ObjPos,
                                                pixel_normal,
                                                obj_view_reflected,
                                                point_light_obj_pos_ranges[4],
                                                cSpecularExponent );
                        lightDiffuse += LightTerms.xxxx * point_light_colors[4];
                        lightSpecular += LightTerms.yyyy * point_light_colors[4];
                        if( bPointLightBool[5] )
                        {
                            float2 LightTerms = ComputePointLight(
                                                    In.ObjPos,
                                                    pixel_normal,
                                                    obj_view_reflected,
                                                    point_light_obj_pos_ranges[5],
                                                    cSpecularExponent );
                            lightDiffuse += LightTerms.xxxx * point_light_colors[5];
                            lightSpecular += LightTerms.yyyy * point_light_colors[5];
                        }
                    }
                }
            }
        }
    }
    
    // directional lights
    if( bDirLightBool[0] )
    {
        float2 LightTerms = ComputeDirLight(
                                In.ObjPos,
                                pixel_normal,
                                obj_view_reflected,
                                dir_light_obj_dirs[0],
                                cSpecularExponent,
                                dir_light_shadow_proj_matrix[0],
                                dir_light_shadow_textures[0],
                                dir_light_shadow_proj_matrix_scene[0],
                                dir_light_shadow_textures_scene[0] );
        lightDiffuse += LightTerms.xxxx * dir_light_colors[0];
        lightSpecular += LightTerms.yyyy * dir_light_colors[0];
    }
    
    // spot lights
    if( bSpotLightBool[0] )
    {
        float2 LightTerms = ComputeSpotLight(
                                In.ObjPos,
                                pixel_normal,
                                obj_view_reflected,
                                spot_light_obj_pos_ranges[0],
                                spot_light_obj_dirs[0],
                                spot_light_inner_outer_angles[0],
                                cSpecularExponent,
                                spot_light_shadow_proj_matrix[0],
                                spot_light_shadow_textures[0] );
        lightDiffuse += LightTerms.xxxx * spot_light_colors[0];
        lightSpecular += LightTerms.yyyy * spot_light_colors[0];                                   
        if( bSpotLightBool[1] )
        {
            float2 LightTerms = ComputeSpotLight(
                                    In.ObjPos,
                                    pixel_normal,
                                    obj_view_reflected,
                                    spot_light_obj_pos_ranges[1],
                                    spot_light_obj_dirs[1],
                                    spot_light_inner_outer_angles[1],
                                    cSpecularExponent,
                                    spot_light_shadow_proj_matrix[1],
                                    spot_light_shadow_textures[1] );
            lightDiffuse += LightTerms.xxxx * spot_light_colors[1];
            lightSpecular += LightTerms.yyyy * spot_light_colors[1];                                   
            if( bSpotLightBool[2] )
            {
                float2 LightTerms = ComputeSpotLight(
                                        In.ObjPos,
                                        pixel_normal,
                                        obj_view_reflected,
                                        spot_light_obj_pos_ranges[2],
                                        spot_light_obj_dirs[2],
                                        spot_light_inner_outer_angles[2],
                                        cSpecularExponent,
                                        spot_light_shadow_proj_matrix[2],
                                        spot_light_shadow_textures[2] );
                lightDiffuse += LightTerms.xxxx * spot_light_colors[2];
                lightSpecular += LightTerms.yyyy * spot_light_colors[2];                                   
                if( bSpotLightBool[3] )
                {
                    float2 LightTerms = ComputeSpotLight(
                                            In.ObjPos,
                                            pixel_normal,
                                            obj_view_reflected,
                                            spot_light_obj_pos_ranges[3],
                                            spot_light_obj_dirs[3],
                                            spot_light_inner_outer_angles[3],
                                            cSpecularExponent,
                                            spot_light_shadow_proj_matrix[3],
                                            spot_light_shadow_textures[3] );
                    lightDiffuse += LightTerms.xxxx * spot_light_colors[3];
                    lightSpecular += LightTerms.yyyy * spot_light_colors[3];                                   
                    if( bSpotLightBool[4] )
                    {
                        float2 LightTerms = ComputeSpotLight(
                                                In.ObjPos,
                                                pixel_normal,
                                                obj_view_reflected,
                                                spot_light_obj_pos_ranges[4],
                                                spot_light_obj_dirs[4],
                                                spot_light_inner_outer_angles[4],
                                                cSpecularExponent,
                                                spot_light_shadow_proj_matrix[4],
                                                spot_light_shadow_textures[4] );
                        lightDiffuse += LightTerms.xxxx * spot_light_colors[4];
                        lightSpecular += LightTerms.yyyy * spot_light_colors[4];                                   
                        if( bSpotLightBool[5] )
                        {
                            float2 LightTerms = ComputeSpotLight(
                                                    In.ObjPos,
                                                    pixel_normal,
                                                    obj_view_reflected,
                                                    spot_light_obj_pos_ranges[5],
                                                    spot_light_obj_dirs[5],
                                                    spot_light_inner_outer_angles[5],
                                                    cSpecularExponent,
                                                    spot_light_shadow_proj_matrix[5],
                                                    spot_light_shadow_textures[5] );
                            lightDiffuse += LightTerms.xxxx * spot_light_colors[5];
                            lightSpecular += LightTerms.yyyy * spot_light_colors[5];                                   
                        }
                    }
                }
            }
        }
    }
    
    lightDiffuse.a = 1;
    lightSpecular.a = 1;
    
    float4 ColorOut = ( lightDiffuse * texDiffuse ) + ( lightSpecular * texSpecular );
        
    return ColorOut;
}


float4 GetDiffuseTexture( float2 TexCoord0, float2 TexCoord1 )
{
    float2 TexCoord = ( DiffuseTextureUVIndex == 0 ) ? TexCoord0 : TexCoord1;
    float4 Result = tex2D( DiffuseTexture, TexCoord );
    if( DiffuseLayer1Enabled )
    {
        TexCoord = ( DiffuseTexture1UVIndex == 0 ) ? TexCoord0 : TexCoord1;
        float4 Src = tex2D( DiffuseTexture1, TexCoord );
        Result *= ( 1 - Src.a );
        Src.rgb *= Src.a;
        Result += Src;
        if( DiffuseLayer2Enabled )
        {
            TexCoord = ( DiffuseTexture2UVIndex == 0 ) ? TexCoord0 : TexCoord1;
            float4 Src = tex2D( DiffuseTexture2, TexCoord );
            Result *= ( 1 - Src.a );
            Src.rgb *= Src.a;
            Result += Src;
            if( DiffuseLayer3Enabled )
            {
                TexCoord = ( DiffuseTexture3UVIndex == 0 ) ? TexCoord0 : TexCoord1;
                float4 Src = tex2D( DiffuseTexture3, TexCoord );
                Result *= ( 1 - Src.a );
                Src.rgb *= Src.a;
                Result += Src;
            }
        }
    }
    return Result;
}


float4 NormalCombine( float4 Dest, float4 Src )
{
    Src.xy = 2 * Src.xy - 1;
    Dest = Dest * ( ( 2 * Src * Src.a ) + ( 1 - Src.a ) );
    return Dest;
}


float2 GetNormalMapTexture( float2 TexCoord0, float2 TexCoord1 )
{
    float2 TexCoord = ( NormalMapTextureUVIndex == 0 ) ? TexCoord0 : TexCoord1;
    float4 Result = tex2D( NormalMapTexture, TexCoord );
    Result.xy = 2 * Result.xy - 1;
    if( NormalMapLayer1Enabled )
    {
        TexCoord = ( NormalMapTexture1UVIndex == 0 ) ? TexCoord0 : TexCoord1;
        Result = NormalCombine( Result, tex2D( NormalMapTexture1, TexCoord ) );
        if( NormalMapLayer2Enabled )
        {
            TexCoord = ( NormalMapTexture2UVIndex == 0 ) ? TexCoord0 : TexCoord1;
            Result = NormalCombine( Result, tex2D( NormalMapTexture2, TexCoord ) );
        }
    }
    Result.xy = Result.xy * 0.5 + 0.5;
    return Result.xy;
}


float4 ps_main_multitexture( PS_INPUT_MULTITEX In,
                             uniform const bool bPointLightBool[MAX_POINT_LIGHT_COUNT],
                             uniform const bool bSpotLightBool[MAX_SPOT_LIGHT_COUNT],
                             uniform const bool bDirLightBool[MAX_DIR_LIGHT_COUNT]
                             ) : COLOR
{
    float4 test = 0;

    float4 texDiffuse = GetDiffuseTexture( In.Tex01.xy, In.Tex01.zw );
    if( bEmissive )
        return texDiffuse;  
    
    float2 NormalMap = GetNormalMapTexture( In.Tex01.xy, In.Tex01.zw );
    
    // Sample normal map
    float3 NormalMapSample = RecoverXYZFromNormalMapSample( NormalMap );
    
    float3 pixel_normal = BumpNormalToObjectSpace( In.Normal, In.Binormal, In.Tangent, NormalMapSample );
    
    float3 obj_view_reflected = reflect( obj_view_direction, pixel_normal );

    float4 lightDiffuse = ambient;
    float4 lightSpecular = 0;
    
    if( bPointLightBool[0] )
    {
        float2 LightTerms = ComputePointLight(
                                In.ObjPos,
                                pixel_normal,
                                obj_view_reflected,
                                point_light_obj_pos_ranges[0],
                                cSpecularExponent );
        lightDiffuse += LightTerms.xxxx * point_light_colors[0];
        lightSpecular += LightTerms.yyyy * point_light_colors[0];
        if( bPointLightBool[1] )
        {
            float2 LightTerms = ComputePointLight(
                                    In.ObjPos,
                                    pixel_normal,
                                    obj_view_reflected,
                                    point_light_obj_pos_ranges[1],
                                    cSpecularExponent );
            lightDiffuse += LightTerms.xxxx * point_light_colors[1];
            lightSpecular += LightTerms.yyyy * point_light_colors[1];
            if( bPointLightBool[2] )
            {
                float2 LightTerms = ComputePointLight(
                                        In.ObjPos,
                                        pixel_normal,
                                        obj_view_reflected,
                                        point_light_obj_pos_ranges[2],
                                        cSpecularExponent );
                lightDiffuse += LightTerms.xxxx * point_light_colors[2];
                lightSpecular += LightTerms.yyyy * point_light_colors[2];
                if( bPointLightBool[3] )
                {
                    float2 LightTerms = ComputePointLight(
                                            In.ObjPos,
                                            pixel_normal,
                                            obj_view_reflected,
                                            point_light_obj_pos_ranges[3],
                                            cSpecularExponent );
                    lightDiffuse += LightTerms.xxxx * point_light_colors[3];
                    lightSpecular += LightTerms.yyyy * point_light_colors[3];
                    if( bPointLightBool[4] )
                    {
                        float2 LightTerms = ComputePointLight(
                                                In.ObjPos,
                                                pixel_normal,
                                                obj_view_reflected,
                                                point_light_obj_pos_ranges[4],
                                                cSpecularExponent );
                        lightDiffuse += LightTerms.xxxx * point_light_colors[4];
                        lightSpecular += LightTerms.yyyy * point_light_colors[4];
                        if( bPointLightBool[5] )
                        {
                            float2 LightTerms = ComputePointLight(
                                                    In.ObjPos,
                                                    pixel_normal,
                                                    obj_view_reflected,
                                                    point_light_obj_pos_ranges[5],
                                                    cSpecularExponent );
                            lightDiffuse += LightTerms.xxxx * point_light_colors[5];
                            lightSpecular += LightTerms.yyyy * point_light_colors[5];
                        }
                    }
                }
            }
        }
    }
    
    // directional lights
    if( bDirLightBool[0] )
    {
        float2 LightTerms = ComputeDirLight(
                                In.ObjPos,
                                pixel_normal,
                                obj_view_reflected,
                                dir_light_obj_dirs[0],
                                cSpecularExponent,
                                dir_light_shadow_proj_matrix[0],
                                dir_light_shadow_textures[0],
                                dir_light_shadow_proj_matrix_scene[0],
                                dir_light_shadow_textures_scene[0] );
        lightDiffuse += LightTerms.xxxx * dir_light_colors[0];
        lightSpecular += LightTerms.yyyy * dir_light_colors[0];
    }
    
    // spot lights
    if( bSpotLightBool[0] )
    {
        float2 LightTerms = ComputeSpotLight(
                                In.ObjPos,
                                pixel_normal,
                                obj_view_reflected,
                                spot_light_obj_pos_ranges[0],
                                spot_light_obj_dirs[0],
                                spot_light_inner_outer_angles[0],
                                cSpecularExponent,
                                spot_light_shadow_proj_matrix[0],
                                spot_light_shadow_textures[0] );
        lightDiffuse += LightTerms.xxxx * spot_light_colors[0];
        lightSpecular += LightTerms.yyyy * spot_light_colors[0];                                   
        if( bSpotLightBool[1] )
        {
            float2 LightTerms = ComputeSpotLight(
                                    In.ObjPos,
                                    pixel_normal,
                                    obj_view_reflected,
                                    spot_light_obj_pos_ranges[1],
                                    spot_light_obj_dirs[1],
                                    spot_light_inner_outer_angles[1],
                                    cSpecularExponent,
                                    spot_light_shadow_proj_matrix[1],
                                    spot_light_shadow_textures[1] );
            lightDiffuse += LightTerms.xxxx * spot_light_colors[1];
            lightSpecular += LightTerms.yyyy * spot_light_colors[1];                                   
            if( bSpotLightBool[2] )
            {
                float2 LightTerms = ComputeSpotLight(
                                        In.ObjPos,
                                        pixel_normal,
                                        obj_view_reflected,
                                        spot_light_obj_pos_ranges[2],
                                        spot_light_obj_dirs[2],
                                        spot_light_inner_outer_angles[2],
                                        cSpecularExponent,
                                        spot_light_shadow_proj_matrix[2],
                                        spot_light_shadow_textures[2] );
                lightDiffuse += LightTerms.xxxx * spot_light_colors[2];
                lightSpecular += LightTerms.yyyy * spot_light_colors[2];                                   
                if( bSpotLightBool[3] )
                {
                    float2 LightTerms = ComputeSpotLight(
                                            In.ObjPos,
                                            pixel_normal,
                                            obj_view_reflected,
                                            spot_light_obj_pos_ranges[3],
                                            spot_light_obj_dirs[3],
                                            spot_light_inner_outer_angles[3],
                                            cSpecularExponent,
                                            spot_light_shadow_proj_matrix[3],
                                            spot_light_shadow_textures[3] );
                    lightDiffuse += LightTerms.xxxx * spot_light_colors[3];
                    lightSpecular += LightTerms.yyyy * spot_light_colors[3];                                   
                    if( bSpotLightBool[4] )
                    {
                        float2 LightTerms = ComputeSpotLight(
                                                In.ObjPos,
                                                pixel_normal,
                                                obj_view_reflected,
                                                spot_light_obj_pos_ranges[4],
                                                spot_light_obj_dirs[4],
                                                spot_light_inner_outer_angles[4],
                                                cSpecularExponent,
                                                spot_light_shadow_proj_matrix[4],
                                                spot_light_shadow_textures[4] );
                        lightDiffuse += LightTerms.xxxx * spot_light_colors[4];
                        lightSpecular += LightTerms.yyyy * spot_light_colors[4];                                   
                        if( bSpotLightBool[5] )
                        {
                            float2 LightTerms = ComputeSpotLight(
                                                    In.ObjPos,
                                                    pixel_normal,
                                                    obj_view_reflected,
                                                    spot_light_obj_pos_ranges[5],
                                                    spot_light_obj_dirs[5],
                                                    spot_light_inner_outer_angles[5],
                                                    cSpecularExponent,
                                                    spot_light_shadow_proj_matrix[5],
                                                    spot_light_shadow_textures[5] );
                            lightDiffuse += LightTerms.xxxx * spot_light_colors[5];
                            lightSpecular += LightTerms.yyyy * spot_light_colors[5];                                   
                        }
                    }
                }
            }
        }
    }
    
    lightDiffuse.a = 1;
    lightSpecular.a = 1;
    
    lightDiffuse *= texDiffuse;
    lightSpecular *= tex2D( SpecularMapTexture, In.Tex01.xy );
    
    float4 ColorOut = lightDiffuse + lightSpecular;
        
    return ColorOut;
}


float4 ps_main_loopunroll( PS_INPUT In,
                           uniform const bool bPointLightBool[MAX_POINT_LIGHT_COUNT],
                           uniform const bool bSpotLightBool[MAX_SPOT_LIGHT_COUNT],
                           uniform const bool bDirLightBool[MAX_DIR_LIGHT_COUNT]
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
    
    // Point lights 
    for( int i = 0; i < MAX_POINT_LIGHT_COUNT; i++ )
    {
        if( bPointLightBool[i] )
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
    
    // Spot lights
    for( int i = 0; i < MAX_SPOT_LIGHT_COUNT; i++ )
    {
        if( bSpotLightBool[i] )
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
        if( bDirLightBool[i] )
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
        
    return ColorOut;
}


technique Ubershader_Nested
{
    pass
    {
        VertexShader = compile vs_3_0 vs_main();                   
        PixelShader = compile ps_3_0 ps_main_nested( bPointLightCount, bSpotLightCount, bDirLightCount );
        
        fillmode = solid;
    }
}

technique Ubershader_LoopUnroll
{
    pass
    {
        VertexShader = compile vs_3_0 vs_main();                   
        PixelShader = compile ps_3_0 ps_main_loopunroll( bPointLightCount, bSpotLightCount, bDirLightCount );
        
        fillmode = solid;
    }
}


technique Ubershader_MultiTexture
{
    pass
    {
        VertexShader = compile vs_3_0 vs_main_multitexture();                  
        PixelShader = compile ps_3_0 ps_main_multitexture( bPointLightCount, bSpotLightCount, bDirLightCount );
        
        fillmode = solid;
    }
}


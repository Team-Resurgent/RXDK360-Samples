//--------------------------------------------------------------------------------------
// PassPerLight.fx
//
// This effect contains the techniques and shaders for pass-per-light lighting.
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include "shadowmaps.inc"
#include "lighting.inc"

#define SHADOW_SAMPLER sampler_state { MipFilter = NONE; MinFilter = POINT; MagFilter = POINT; AddressU = CLAMP; AddressV = CLAMP; }                         
#define TEXTURE_SAMPLER sampler_state { MipFilter = LINEAR; MinFilter = LINEAR; MagFilter = LINEAR; AddressU = WRAP; AddressV = WRAP; };

shared float4x4 world_view_proj_matrix;


float4 cDiffuseMaterial = 1;
float4 cSpecularMaterial = 0;
float4 cEmissiveMaterial = 0;
float cSpecularExponent = 16.0f;      
sampler2D DiffuseTexture : register(s0) = TEXTURE_SAMPLER;
sampler2D SpecularMapTexture : register(s4) = TEXTURE_SAMPLER;
sampler2D NormalMapTexture : register(s5) = TEXTURE_SAMPLER;

bool bEmissive = false;

shared float4 obj_view_direction;                      

shared float4 light_color;
shared float4 light_obj_pos_range;
shared float4 light_obj_direction;
shared float4 spot_light_angles;

shared bool bShadowedLight = false;
shared float4x4 light_world_view_proj_matrix;
shared sampler2D shadow_buffer_texture : register(s10) = SHADOW_SAMPLER; 
shared float4x4 light_world_view_proj_matrix_scene;
shared sampler2D shadow_buffer_texture_scene : register(s11) = SHADOW_SAMPLER; 

struct VS_OUTPUT
{
    float4  Pos         : POSITION;
    float4  Tex0        : TEXCOORD0;
    float3  ObjPos      : TEXCOORD1;
    float3  Tangent     : TEXCOORD2;
    float3  Binormal    : TEXCOORD3;
    float3  Normal      : TEXCOORD4;
};

struct VS_OUTPUT_AMBIENT
{
    float4  Pos         : POSITION;
    float4  Tex0        : TEXCOORD0;
};

struct VS_INPUT
{
    float4 vPosition    : POSITION0;
    float3 vNormal      : NORMAL0;
    float2 vTexCoord    : TEXCOORD0;
    float3 vTangent     : TANGENT;
};



VS_OUTPUT vs_main( VS_INPUT In )
{
    VS_OUTPUT Out = (VS_OUTPUT) 0;
    
    // Transform Position
    Out.Pos = mul( In.vPosition,  world_view_proj_matrix );
    Out.ObjPos = In.vPosition.xyz;
    
    // Just pass thru the texture coords
    Out.Tex0 = float4( In.vTexCoord, 0, 0 );

    Out.Normal = In.vNormal;
    Out.Tangent = In.vTangent;
    Out.Binormal = cross( In.vNormal, In.vTangent );
          
    return Out;
}


VS_OUTPUT_AMBIENT vs_main_ambient( VS_INPUT In )
{
    VS_OUTPUT_AMBIENT Out;
    
    // Transform Position
    Out.Pos = mul( In.vPosition,  world_view_proj_matrix );

    // Just pass thru the texture coords
    Out.Tex0 = float4( In.vTexCoord, 0, 0 );
          
    return Out;
}


float4 ps_main_ambient( VS_OUTPUT_AMBIENT In ) : COLOR
{
    float4 diffuseTex = tex2D( DiffuseTexture, In.Tex0 );
    if( bEmissive )
        return diffuseTex;
    float4 Ambient = diffuseTex * light_color;
    return Ambient;
}

float4 ps_main_point( VS_OUTPUT In ) : COLOR
{
    if( bEmissive )
        return 0;
        
    // Sample normal map
    float3 NormalMapSample = RecoverXYZFromNormalMapSample( tex2D( NormalMapTexture, In.Tex0 ) );
    
    float3 pixel_normal = BumpNormalToObjectSpace( In.Normal, In.Binormal, In.Tangent, NormalMapSample );
    
    float3 obj_view_reflected = reflect( obj_view_direction, pixel_normal );
    
    float2 Result = ComputePointLight( In.ObjPos,
                                       pixel_normal,
                                       obj_view_reflected,
                                       light_obj_pos_range,
                                       cSpecularExponent );
                                       
    float4 lightDiffuse = light_color * Result.xxxx;
    float4 lightSpecular = light_color * Result.yyyy;
    
    lightDiffuse.a = 1;
    lightSpecular.a = 1;
    
    lightDiffuse *= tex2D( DiffuseTexture, In.Tex0 );
    lightSpecular *= tex2D( SpecularMapTexture, In.Tex0 );
    
    float4 ColorOut = lightDiffuse + lightSpecular;
    
    return ColorOut;
}

float4 ps_main_spot( VS_OUTPUT In ) : COLOR
{
    if( bEmissive )
        return 0;
        
    // Sample normal map
    float3 NormalMapSample = RecoverXYZFromNormalMapSample( tex2D( NormalMapTexture, In.Tex0 ) );
    
    float3 pixel_normal = BumpNormalToObjectSpace( In.Normal, In.Binormal, In.Tangent, NormalMapSample );
    
    float3 obj_view_reflected = reflect( obj_view_direction, pixel_normal );
    
    float2 Result = ComputeSpotLight( In.ObjPos,
                                      pixel_normal,
                                      obj_view_reflected,
                                      light_obj_pos_range,
                                      light_obj_direction,
                                      spot_light_angles,
                                      cSpecularExponent,
                                      light_world_view_proj_matrix,
                                      shadow_buffer_texture );
                                      
    
    float4 lightDiffuse = light_color * Result.xxxx;
    float4 lightSpecular = light_color * Result.yyyy;
    
    lightDiffuse.a = 1;
    lightSpecular.a = 1;
    
    lightDiffuse *= tex2D( DiffuseTexture, In.Tex0 );
    lightSpecular *= tex2D( SpecularMapTexture, In.Tex0 );
    
    float4 ColorOut = lightDiffuse + lightSpecular;
        
    return ColorOut;
}


float4 ps_main_dir( [unused] VS_OUTPUT In ) : COLOR
{
    if( bEmissive )
        return 0;
        
    // Sample normal map
    float3 NormalMapSample = RecoverXYZFromNormalMapSample( tex2D( NormalMapTexture, In.Tex0 ) );
    
    float3 pixel_normal = BumpNormalToObjectSpace( In.Normal, In.Binormal, In.Tangent, NormalMapSample );
    
    float3 obj_view_reflected = reflect( obj_view_direction, pixel_normal );
    
    float2 Result = ComputeDirLight( In.ObjPos,
                                     pixel_normal,
                                     obj_view_reflected,
                                     light_obj_direction,
                                     cSpecularExponent,
                                     light_world_view_proj_matrix,
                                     shadow_buffer_texture,
                                     light_world_view_proj_matrix_scene,
                                     shadow_buffer_texture_scene );
                                      
    float4 lightDiffuse = light_color * Result.xxxx;
    float4 lightSpecular = light_color * Result.yyyy;
    
    lightDiffuse.a = 1;
    lightSpecular.a = 1;
    
    lightDiffuse *= tex2D( DiffuseTexture, In.Tex0 );
    lightSpecular *= tex2D( SpecularMapTexture, In.Tex0 );
    
    float4 ColorOut = lightDiffuse + lightSpecular;
        
    return ColorOut;
}

technique AmbientLight
{
    pass
    {
        VertexShader = compile vs_3_0 vs_main_ambient();                   
        PixelShader = compile ps_3_0 ps_main_ambient();
        
        fillmode = solid;
        alphablendenable = false;
        srcblend = one;
        destblend = one;
        zenable = true;
        zwriteenable = true;
        zfunc = lessequal;
        stencilenable = false;        
    }
}

technique PointLight
{
    pass
    {
        VertexShader = compile vs_3_0 vs_main();                   
        PixelShader = compile ps_3_0 ps_main_point();
        
        fillmode = solid;
        alphablendenable = true;
        srcblend = one;
        destblend = one;
        zenable = true;
        zwriteenable = true;
        zfunc = lessequal;
    }
}

technique SpotLight
{
    pass
    {
        VertexShader = compile vs_3_0 vs_main();                   
        PixelShader = compile ps_3_0 ps_main_spot();
        
        fillmode = solid;
        alphablendenable = true;
        srcblend = one;
        destblend = one;
        zenable = true;
        zwriteenable = true;
        zfunc = lessequal;
        stencilenable = false;        
    }
}

technique DirLight
{
    pass
    {
        VertexShader = compile vs_3_0 vs_main();                   
        PixelShader = compile ps_3_0 ps_main_dir();
        
        fillmode = solid;
        alphablendenable = true;
        srcblend = one;
        destblend = one;
        zenable = true;
        zwriteenable = true;
        zfunc = lessequal;
        stencilenable = false;        
    }
}


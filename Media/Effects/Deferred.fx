//--------------------------------------------------------------------------------------
// Deferred.fx
//
// This effect contains the techniques and shaders for deferred lighting.
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include "shadowmaps.inc"
#include "lighting.inc"

#define SHADOW_SAMPLER sampler_state { MipFilter = NONE; MinFilter = POINT; MagFilter = POINT; AddressU = CLAMP; AddressV = CLAMP; }                         
#define TEXTURE_SAMPLER sampler_state { MipFilter = LINEAR; MinFilter = LINEAR; MagFilter = LINEAR; AddressU = WRAP; AddressV = WRAP; };

shared float4x4 world_view_proj_matrix;
shared float4x4 world_matrix;

bool bEmissive = false;
float cSpecularExponent = 16.0f;      
sampler2D DiffuseTexture : register(s0) = TEXTURE_SAMPLER;
sampler2D SpecularMapTexture : register(s4) = TEXTURE_SAMPLER;
sampler2D NormalMapTexture : register(s5) = TEXTURE_SAMPLER;

//--------------------------------------------------------------------------------------

struct VS_INPUT
{
    float4 Position         : POSITION0;
    float3 Normal           : NORMAL0;
    float2 TexCoord0        : TEXCOORD0; 
    float3 Tangent          : TANGENT;
};

struct VS_OUTPUT
{
    float4 Position         : POSITION;
    float2 TexCoord0        : TEXCOORD0;
    float3 Normal           : TEXCOORD1;
    float3 Tangent          : TEXCOORD2;
    float3 Binormal         : TEXCOORD3;
};

struct PS_INPUT
{
    float2 TexCoord0        : TEXCOORD0;
    float3 Normal           : TEXCOORD1;
    float3 Tangent          : TEXCOORD2;
    float3 Binormal         : TEXCOORD3;
};

VS_OUTPUT vs_main( VS_INPUT In )
{
    VS_OUTPUT Out;
    Out.TexCoord0 = In.TexCoord0;
    
    Out.Position = mul( float4( In.Position.xyz, 1 ), world_view_proj_matrix );
    Out.Normal = normalize( mul( In.Normal, world_matrix ) );
    Out.Tangent = normalize( mul( In.Tangent, world_matrix ) );
    Out.Binormal = cross( Out.Normal, Out.Tangent );
    
    return Out;
}



struct PS_OUT_DEFERRED
{
    float4 Color[2] : COLOR0;
};                                          
                                               
PS_OUT_DEFERRED ps_main( PS_INPUT In )
{
    PS_OUT_DEFERRED Out;
    // Sample diffuse color from diffuse texture and store it in rendertarget 0.
    Out.Color[0] = tex2D( DiffuseTexture, In.TexCoord0 );
    
    if( bEmissive )
    {
        // Set alpha channel of rendertarget 0 to 1.
        Out.Color[0].a = 1;
        Out.Color[1] = float4( 0, 0, 0, 0 );
        return Out;
    }
    else
    {
        // Set alpha channel of rendertarget 0 to 0.                       
        Out.Color[0].a = 0;
        
        float3 WorldNormal;
        // Sample normal map.
        float2 NormalMapSample = tex2D( NormalMapTexture, In.TexCoord0 );
        // Recover the XYZ vector from the XY normal map sample.
        float3 NormalMap = RecoverXYZFromNormalMapSample( NormalMapSample );
        
        // Accumulate world-space normal vector from the normal map.
        WorldNormal = In.Tangent * NormalMap.x;
        WorldNormal += In.Binormal * NormalMap.y;
        WorldNormal += In.Normal * NormalMap.z;
        WorldNormal = normalize( WorldNormal );
        // Pack the normal back into 0..1 range per component.
        WorldNormal = WorldNormal * 0.5 + 0.5;
        // Store normal into rendertarget 1.
        Out.Color[1].rgb = WorldNormal;
        
        // Sample specular map and store it in the rendertarget 1 alpha channel.
        float4 SpecularMapSample = tex2D( SpecularMapTexture, In.TexCoord0 );
        Out.Color[1].a = SpecularMapSample.r;
        return Out;
    }
}

//--------------------------------------------------------------------------------------

#define DEFERRED_SAMPLER sampler_state { MipFilter = POINT; MinFilter = POINT; MagFilter = POINT; AddressU = CLAMP; AddressV = CLAMP; };
shared sampler2D depth_buffer_texture : register(s0) = DEFERRED_SAMPLER; 
shared sampler2D color_buffer_texture : register(s1) = DEFERRED_SAMPLER;
shared sampler2D normal_buffer_texture : register(s2) = DEFERRED_SAMPLER;
shared float4x4 inv_view_proj_matrix;
shared float4 light_color;
shared float4 light_world_pos_range;
shared float4 light_world_direction;
shared float4 spot_light_angles;
shared float4 world_view_direction;
shared float2 screen_size = float2( 1280.0, 720.0 );
shared bool bShadowedLight = false;
shared float4x4 light_world_view_proj_matrix;
shared sampler2D shadow_buffer_texture : register(s10) = SHADOW_SAMPLER;
shared float4x4 light_world_view_proj_matrix_scene;
shared sampler2D shadow_buffer_texture_scene : register(s11) = SHADOW_SAMPLER;


struct VS_IN_LIGHTING
{
    float4 Position : POSITION;
    float2 TexCoord : TEXCOORD0;
};

struct VS_OUT_LIGHTING
{
    float4 vPosition : POSITION;
    float2 vTexCoord : TEXCOORD0;
};

struct VS_OUT_LIGHTING_VPOS
{
    float4 vPosition : POSITION;
};

struct PS_IN_LIGHTING
{
    float2 vTexCoord : TEXCOORD0;
};

struct PS_IN_LIGHTING_VPOS
{
    float2 vScreenPos: VPOS0;
};

VS_OUT_LIGHTING vs_lightingpass( VS_IN_LIGHTING In )
{
    VS_OUT_LIGHTING Out;
    Out.vTexCoord = In.TexCoord;
    Out.vPosition = In.Position;
    return Out;
}

VS_OUT_LIGHTING_VPOS vs_lightingpass_transformed( VS_IN_LIGHTING In )
{
    VS_OUT_LIGHTING_VPOS Out;
    float4 vPos = mul( In.Position, world_view_proj_matrix );
    Out.vPosition = vPos;
    return Out;
}

float3 DeProjectWorldPos( float2 TexturePos )
{
    float2 ScreenPos = TexturePos * float2( 2, -2 ) + float2( -1, 1 );
    float4 DepthSample = tex2D( depth_buffer_texture, TexturePos );
    float4 HomogenousPos = float4( ScreenPos, DepthSample.r, 1 );
    float4 WorldPos = mul( HomogenousPos, inv_view_proj_matrix );
    WorldPos /= WorldPos.w;
    return WorldPos;
}

float4 ps_ambientlight( PS_IN_LIGHTING In ) : COLOR
{
    float4 ColorSample = tex2D( color_buffer_texture, In.vTexCoord );
    if( ColorSample.a == 1 )
        return ColorSample;
    return light_color * ColorSample;
}

float4 ps_pointlight( PS_IN_LIGHTING In ) : COLOR
{
    float3 WorldPos = DeProjectWorldPos( In.vTexCoord );
    float3 Direction = light_world_pos_range.xyz - WorldPos;
    float Distance = length( Direction );
    float4 result = 0;
    if( Distance < light_world_pos_range.w )
    {
        float4 ColorSample = tex2D( color_buffer_texture, In.vTexCoord );
        if( ColorSample.a == 0 )
        {
            float4 Normal = tex2D( normal_buffer_texture, In.vTexCoord );
            Normal = ( Normal * 2.0f ) - 1.0f;
            Direction = normalize( Direction );
            
            // diffuse lighting
            float Attenuation = 1.0f - saturate( Distance / light_world_pos_range.w );
            
            // specular lighting
            float3 world_view_reflected = reflect( world_view_direction, Normal );
            float Specular = pow( saturate( dot( world_view_reflected, Direction ) ), cSpecularExponent );
            Attenuation += ( Specular * ColorSample.a );
            
            result = light_color * Attenuation * ColorSample * saturate( dot( Normal, Direction ) );
        }
    }
    return result;
}

float4 ps_spotlight( PS_IN_LIGHTING In ) : COLOR
{
    float3 WorldPos = DeProjectWorldPos( In.vTexCoord );
    float3 Direction = light_world_pos_range.xyz - WorldPos;
    float Distance = length( Direction );
    float4 result = 0;
    if( Distance < light_world_pos_range.w )
    {
        Direction /= Distance;
        float AngleToAxis = dot( Direction, -light_world_direction );
        if( AngleToAxis >= spot_light_angles.x )
        {
            float4 ColorSample = tex2D( color_buffer_texture, In.vTexCoord );
            if( ColorSample.a == 1 )
                return 0;
            float4 Normal = tex2D( normal_buffer_texture, In.vTexCoord );
            Normal = ( Normal * 2.0f ) - 1.0f;
        
            // diffuse lighting    
            float Spot = 1.0f - saturate( ( spot_light_angles.y - AngleToAxis ) * spot_light_angles.z );
            float SpotAmount = Spot * ( 1.0f - saturate( Distance / light_world_pos_range.w ) );
            
            // specular lighting
            float3 world_view_reflected = reflect( world_view_direction, Normal );
            float Specular = pow( saturate( dot( world_view_reflected, Direction ) ), cSpecularExponent );
            SpotAmount += ( Specular * ColorSample.a );
        
            if( bShadowedLight )
            {
                float4 LightPos = mul( float4( WorldPos, 1 ), light_world_view_proj_matrix );
                LightPos /= LightPos.w;
                SpotAmount *= ShadowMapSample( LightPos.z, LightPos.xy, shadow_buffer_texture );
            }
            result = light_color * SpotAmount * ColorSample * saturate( dot( Normal, Direction ) );
        }
    }
    return result;
}

float4 ps_spotlight_vpos( PS_IN_LIGHTING_VPOS In ) : COLOR
{
    float2 vTexCoord = In.vScreenPos / screen_size;
    float3 WorldPos = DeProjectWorldPos( vTexCoord );
    float3 Direction = light_world_pos_range.xyz - WorldPos;
    float Distance = length( Direction );
    float4 result = 0;
    if( Distance < light_world_pos_range.w )
    {
        Direction /= Distance;
        float AngleToAxis = dot( Direction, -light_world_direction );
        if( AngleToAxis >= spot_light_angles.x )
        {
            float4 ColorSample = tex2D( color_buffer_texture, vTexCoord );
            if( ColorSample.a == 0 )
            {
                float4 Normal = tex2D( normal_buffer_texture, vTexCoord );
                Normal = ( Normal * 2.0f ) - 1.0f;
                
                // diffuse lighting
                float Spot = 1.0f - saturate( ( spot_light_angles.y - AngleToAxis ) * spot_light_angles.z );
                float SpotAmount = Spot * ( 1.0f - saturate( Distance / light_world_pos_range.w ) );
                            
                // specular lighting
                float3 world_view_reflected = reflect( world_view_direction, Normal );
                float Specular = pow( saturate( dot( world_view_reflected, Direction ) ), cSpecularExponent );
                SpotAmount += ( Specular * ColorSample.a );
            
                if( bShadowedLight )
                {
                    float4 LightPos = mul( float4( WorldPos, 1 ), light_world_view_proj_matrix );
                    LightPos /= LightPos.w;
                    SpotAmount *= ShadowMapSample( LightPos.z, LightPos.xy, shadow_buffer_texture );
                }
                result = light_color * SpotAmount * ColorSample * saturate( dot( Normal, Direction ) );
            }
        }
    }
    return result;
}


float4 ps_dirlight_vpos( PS_IN_LIGHTING_VPOS In ) : COLOR
{
    float2 vTexCoord = In.vScreenPos / screen_size;
    float3 WorldPos = DeProjectWorldPos( vTexCoord );
    float3 Direction = light_world_direction;
    float4 result = 0;
    
    float4 ColorSample = tex2D( color_buffer_texture, vTexCoord );
    if( ColorSample.a == 1 )
        return 0;
    float4 Normal = tex2D( normal_buffer_texture, vTexCoord );
    Normal = ( Normal * 2.0f ) - 1.0f;
    
    // diffuse lighting
    float DirLight = saturate( dot( Normal, -Direction ) );
                
    // specular lighting
    float3 world_view_reflected = reflect( world_view_direction, Normal );
    DirLight += pow( saturate( dot( world_view_reflected, Direction ) ), cSpecularExponent );

    if( bShadowedLight )
    {
        float4 LightPos = mul( float4( WorldPos, 1 ), light_world_view_proj_matrix );
        LightPos /= LightPos.w;
        float2 LightPosMinMax = abs( LightPos.xy * 2 - 1 );
        asm
        {
            max4 LightPosMinMax.x, LightPosMinMax.xy
        };
        float ShadowValue = 0;
        if( LightPosMinMax.x <= 1.0 )
        {
            ShadowValue = ShadowMapSample( LightPos.z, LightPos.xy, shadow_buffer_texture );
        }
        else
        {
            LightPos = mul( float4( WorldPos, 1 ), light_world_view_proj_matrix_scene );
            LightPos.xyz /= LightPos.w;
            ShadowValue = ShadowMapSample( LightPos.z, LightPos.xy, shadow_buffer_texture_scene );
        }
        DirLight *= ShadowValue;
    }
    result = light_color * DirLight * ColorSample;
    
    return result;
}

technique BuildBuffers
{
    pass
    {
        VertexShader = compile vs_3_0 vs_main();                   
        PixelShader = compile ps_3_0 ps_main();
        
        fillmode = solid;
        zenable = true;
        zwriteenable = true;
        zfunc = lessequal;
        stencilenable = false;        
        alphablendenable = false;
    }
}

technique AmbientLight
{
    pass
    {
        VertexShader = compile vs_3_0 vs_lightingpass();                   
        PixelShader = compile ps_3_0 ps_ambientlight();
        
        fillmode = solid;
        zenable = false;
        zwriteenable = false;
        stencilenable = false;        
        alphablendenable = false;
        srcblend = one;
        destblend = one;
    }
}

technique PointLight
{
    pass
    {
        VertexShader = compile vs_3_0 vs_lightingpass();                   
        PixelShader = compile ps_3_0 ps_pointlight();
        
        fillmode = solid;
        zenable = false;
        zwriteenable = false;
        stencilenable = false;        
        alphablendenable = true;
        alphatestenable = false;
        srcblend = one;
        destblend = one;
    }
}

technique SpotLight
{
    pass
    {
        VertexShader = compile vs_3_0 vs_lightingpass_transformed();                   
        PixelShader = compile ps_3_0 ps_spotlight_vpos();
        
        fillmode = solid;
        zenable = false;
        zwriteenable = false;
        stencilenable = false;        
        alphablendenable = true;
        alphatestenable = false;
        srcblend = one;
        destblend = one;
    }
}

technique DirLight
{
    pass
    {
        VertexShader = compile vs_3_0 vs_lightingpass();                   
        PixelShader = compile ps_3_0 ps_dirlight_vpos();
        
        fillmode = solid;
        zenable = false;
        zwriteenable = false;
        stencilenable = false;        
        alphablendenable = true;
        alphatestenable = false;
        srcblend = one;
        destblend = one;
    }
}

//--------------------------------------------------------------------------------------
// Shaders for the AdvancedCommandBuffers sample
//--------------------------------------------------------------------------------------

// Vertex shader constants for transform.
float4x4    view_proj_matrix                        : register(c0);
float4x4    world_matrix                            : register(c4);
float4      world_camera_position                   : register(c8);

// Pixel shader constants for lighting.
float4      directional_light_direction             : register(c0);
float4      directional_light_color                 : register(c1);
float4      ambient_light_color                     : register(c2);
const float specular_exponent = 16.0;

// Pixel shader texture samplers for surface materials.
#define TRILINEAR_SAMPLER sampler_state { MipFilter = LINEAR; MinFilter = LINEAR; MagFilter = LINEAR; AddressU = WRAP; AddressV = WRAP; }
sampler2D   diffuse_texture     : register(s0) = TRILINEAR_SAMPLER;
sampler2D   normal_map_texture  : register(s1) = TRILINEAR_SAMPLER;
sampler2D   specular_texture    : register(s2) = TRILINEAR_SAMPLER;

struct VS_IN
{
    float3  Position        : POSITION0;
    float2  Tex0            : TEXCOORD0;
    float3  Normal          : NORMAL;
    float3  Tangent         : TANGENT;
};

struct VS_OUT
{
    float4 Position         : POSITION;
    float2 Tex0             : TEXCOORD0;
    float3 WorldNormal      : TEXCOORD1;
    float3 WorldTangent     : TEXCOORD2;
    float3 WorldBinormal    : TEXCOORD3;
};

struct PS_IN
{
    float2 Tex0             : TEXCOORD0;
    float3 WorldNormal      : TEXCOORD1;
    float3 WorldTangent     : TEXCOORD2;
    float3 WorldBinormal    : TEXCOORD3;
};


float4 VSQuery( float3 WorldPosition: POSITION0 ) : POSITION
{
    return mul( float4( WorldPosition, 1 ), view_proj_matrix );
}


VS_OUT VSBackground( VS_IN In )
{
    float4 WorldPosition = mul( float4( In.Position, 1 ), world_matrix );
    VS_OUT Out;
    Out.Position = mul( WorldPosition, view_proj_matrix );
    float3 WorldNormal = mul( In.Normal, world_matrix );
    float3 WorldTangent = mul( In.Tangent, world_matrix );
    Out.WorldNormal = WorldNormal;
    Out.WorldTangent = WorldTangent;
    Out.WorldBinormal = cross( WorldNormal, WorldTangent );
    Out.Tex0 = In.Tex0;
    return Out;
}

float3 RecoverXYZFromNormalMapSample( float2 NormalMapSample )
{
    // Expand normal map sample to -1..1 range.
    NormalMapSample = NormalMapSample * 2.0 - 1.0;
    float3 result = float3( NormalMapSample.x, NormalMapSample.y, saturate( 1 - dot( NormalMapSample, NormalMapSample ) ) );
    return result;
}

float3 BumpNormalToWorldSpace( float3 Normal, float3 Binormal, float3 Tangent, float3 SampledNormal )
{
    float3 obj_binormal = normalize( Binormal );
    float3 obj_tangent = normalize( Tangent );
    float3 obj_normal = normalize( Normal );
    float3 obj_bumpnormal = obj_tangent * SampledNormal.x +
                            obj_binormal * SampledNormal.y +
                            obj_normal * SampledNormal.z;
                            
    return normalize( obj_bumpnormal );
}

float4 PSBackground( PS_IN In ) : COLOR
{
    // Sample and decode normal map texture.
    float2 NormalMapXY = tex2D( normal_map_texture, In.Tex0 ).xy;
    float3 NormalMapSample = RecoverXYZFromNormalMapSample( NormalMapXY );
    // Reduce the intensity of the normal map sample.
    NormalMapSample.xy *= 0.6;
    
    // Convert normal map sample to object space.
    float3 WorldNormalMapSample = BumpNormalToWorldSpace( In.WorldNormal, In.WorldBinormal, In.WorldTangent, NormalMapSample );
    
    // Compute diffuse lighting term from directional light.
    float DiffuseLightAmount = saturate( dot( WorldNormalMapSample, -directional_light_direction ) );
    float4 DiffuseColor = tex2D( diffuse_texture, In.Tex0 );
    DiffuseColor *= ( directional_light_color * saturate( DiffuseLightAmount ) + ambient_light_color );
        
    return DiffuseColor;
}

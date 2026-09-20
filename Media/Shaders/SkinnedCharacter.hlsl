//--------------------------------------------------------------------------------------
// Shader for the SkinnedCharacter sample
//--------------------------------------------------------------------------------------

#define MAX_BONE_COUNT  80

// Vertex shader constants for transform and skinning.
float4x4    world_view_proj_matrix                  : register(c0);
float4      obj_camera_position                     : register(c4);
float4x3    bone_palette[MAX_BONE_COUNT]            : register(c12);

// Pixel shader constants for lighting.
float4      directional_light_direction             : register(c0);
float4      directional_light_color                 : register(c1);
float4      ambient_light_color                     : register(c2);
const float specular_exponent = 16.0;

// Vertex shader constants for memexport write destinations.
float4      export_address_position                 : register(c8);
float4      export_address_normal_tangent           : register(c9);
float4      export_address_texcoord0                : register(c10);
static float4 const01 = float4( 0, 1, 0, 0 );

// Vertex shader texture sampler for the bone matrix palette.
#define POINT_SAMPLER sampler_state { MipFilter = NONE; MinFilter = POINT; MagFilter = POINT; AddressU = CLAMP; AddressV = CLAMP; }
sampler1D   bone_palette_texture : register(s0) = POINT_SAMPLER;

// Pixel shader texture samplers for surface materials.
#define TRILINEAR_SAMPLER sampler_state { MipFilter = LINEAR; MinFilter = LINEAR; MagFilter = LINEAR; AddressU = WRAP; AddressV = WRAP; }
sampler2D   diffuse_texture     : register(s0) = TRILINEAR_SAMPLER;
sampler2D   normal_map_texture  : register(s1) = TRILINEAR_SAMPLER;
sampler2D   specular_texture    : register(s2) = TRILINEAR_SAMPLER;

struct VS_IN
{
    float3  Position        : POSITION0;
    float4  BoneIndices     : BLENDINDICES;
    float4  BoneWeights     : BLENDWEIGHT;
    float3  Normal          : NORMAL;
    float2  Tex0            : TEXCOORD0;
    float3  Tangent         : TANGENT;
};

struct VS_IN_NOWEIGHTS
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
    float3 Normal           : TEXCOORD1;
    float3 Tangent          : TEXCOORD2;
    float3 Binormal         : TEXCOORD3;
    float3 ObjViewDir       : TEXCOORD4;
};

struct PS_IN
{
    float2 Tex0             : TEXCOORD0;
    float3 Normal           : TEXCOORD1;
    float3 Tangent          : TEXCOORD2;
    float3 Binormal         : TEXCOORD3;
    float3 ObjViewDir       : TEXCOORD4;
};


float4x3 ConstantFetchBoneMatrix( int BoneIndex )
{
    return bone_palette[ BoneIndex ];
}


VS_OUT SkinVSConstants( VS_IN In )
{
    VS_OUT Out;
    float4 InPos = float4( In.Position, 1 );
    
    float BoneWeights[4] = (float[4])In.BoneWeights;
    int BoneIndices[4] = (int[4])In.BoneIndices;
    
    float4x3 LocalToWorldMatrix = 0.0f;
    for( int i = 3; i >= 0; --i )
    {
        LocalToWorldMatrix += BoneWeights[i] * ConstantFetchBoneMatrix( BoneIndices[i] );
    }
    
    float3 OutPos = 0;
    float3 OutNormal = 0;
    float3 OutTangent = 0;
    
    [isolate]
    {
        OutPos = mul( InPos, LocalToWorldMatrix );
        OutNormal = mul( In.Normal, LocalToWorldMatrix );
        OutTangent = mul( In.Tangent, LocalToWorldMatrix );
    }

    Out.ObjViewDir = normalize( obj_camera_position - OutPos );
    Out.Position = mul( float4( OutPos, 1 ), world_view_proj_matrix );
    Out.Normal = OutNormal;
    Out.Tangent = OutTangent;
    Out.Binormal = cross( OutNormal, OutTangent );
    Out.Tex0 = In.Tex0;
    return Out;
}


float4x3 VFetchBoneMatrix( int BoneIndex )
{
    float4 Result1;
    float4 Result2;
    float4 Result3;
    asm
    {
        vfetch Result1, BoneIndex, position1;
        vfetch Result2, BoneIndex, position2;
        vfetch Result3, BoneIndex, position3;
    };
    
    float4x3 Result;
    Result._11_21_31_41 = Result1;
    Result._12_22_32_42 = Result2;
    Result._13_23_33_43 = Result3;
    return Result;
}


VS_OUT SkinVSVertexFetch( VS_IN In )
{
    VS_OUT Out;
    float4 InPos = float4( In.Position, 1 );
    
    float BoneWeights[4] = (float[4])In.BoneWeights;
    int BoneIndices[4] = (int[4])In.BoneIndices;
    
    float4x3 LocalToWorldMatrix = 0;
    for( int i = 3; i >= 0; --i )
    {
        LocalToWorldMatrix += BoneWeights[i] * VFetchBoneMatrix( BoneIndices[i] );
    }
    
    float3 OutPos = 0;
    float3 OutNormal = 0;
    float3 OutTangent = 0;
    
    [isolate]
    {
        OutPos = mul( InPos, LocalToWorldMatrix );
        OutNormal = mul( In.Normal, LocalToWorldMatrix );
        OutTangent = mul( In.Tangent, LocalToWorldMatrix );
    }

    Out.ObjViewDir = normalize( obj_camera_position - OutPos );
    Out.Position = mul( float4( OutPos, 1 ), world_view_proj_matrix );
    Out.Normal = OutNormal;
    Out.Tangent = OutTangent;
    Out.Binormal = cross( OutNormal, OutTangent );
    Out.Tex0 = In.Tex0;
    return Out;
}


float4x3 VFetchBoneMatrixTextureCache( int BoneIndex )
{
    float4 Result1;
    float4 Result2;
    float4 Result3;
    asm
    {
        vfetch Result1, BoneIndex, position1, UseTextureCache=true
        vfetch Result2, BoneIndex, position2, UseTextureCache=true
        vfetch Result3, BoneIndex, position3, UseTextureCache=true
    };
    
    float4x3 Result;
    Result._11_21_31_41 = Result1;
    Result._12_22_32_42 = Result2;
    Result._13_23_33_43 = Result3;
    return Result;
}


VS_OUT SkinVSVertexFetchTextureCache( VS_IN In )
{
    VS_OUT Out;
    float4 InPos = float4( In.Position, 1 );
    
    float BoneWeights[4] = (float[4])In.BoneWeights;
    int BoneIndices[4] = (int[4])In.BoneIndices;
    
    float4x3 LocalToWorldMatrix = 0;
    for( int i = 3; i >= 0; --i )
    {
        LocalToWorldMatrix += BoneWeights[i] * VFetchBoneMatrixTextureCache( BoneIndices[i] );
    }
    
    float3 OutPos = 0;
    float3 OutNormal = 0;
    float3 OutTangent = 0;
    
    [isolate]
    {
        OutPos = mul( InPos, LocalToWorldMatrix );
        OutNormal = mul( In.Normal, LocalToWorldMatrix );
        OutTangent = mul( In.Tangent, LocalToWorldMatrix );
    }

    Out.ObjViewDir = normalize( obj_camera_position - OutPos );
    Out.Position = mul( float4( OutPos, 1 ), world_view_proj_matrix );
    Out.Normal = OutNormal;
    Out.Tangent = OutTangent;
    Out.Binormal = cross( OutNormal, OutTangent );
    Out.Tex0 = In.Tex0;
    return Out;
}


float4x3 TFetchBoneMatrix( float BoneIndex )
{
    float TexCoord = BoneIndex / MAX_BONE_COUNT;
    float4 Result1;
    float4 Result2;
    float4 Result3;
    asm
    {
        tfetch1D Result1, TexCoord, bone_palette_texture, UseComputedLOD = false, OffsetX = 0.5
        tfetch1D Result2, TexCoord, bone_palette_texture, UseComputedLOD = false, OffsetX = 1.5
        tfetch1D Result3, TexCoord, bone_palette_texture, UseComputedLOD = false, OffsetX = 2.5
    };
    
    float4x3 Result;
    Result._11_21_31_41 = Result1.xyzw;
    Result._12_22_32_42 = Result2.xyzw;
    Result._13_23_33_43 = Result3.xyzw;
    return Result;
}


VS_OUT SkinVSTextureFetch( VS_IN In )
{
    VS_OUT Out;
    float4 InPos = float4( In.Position, 1 );
    
    float BoneWeights[4] = (float[4])In.BoneWeights;
    int BoneIndices[4] = (int[4])In.BoneIndices;
    
    float4x3 LocalToWorldMatrix = 0;
    for( int i = 3; i >= 0; --i )
    {
        LocalToWorldMatrix += BoneWeights[i] * TFetchBoneMatrix( BoneIndices[i] );
    }
    
    float3 OutPos = 0;
    float3 OutNormal = 0;
    float3 OutTangent = 0;
    
    [isolate]
    {
        OutPos = mul( InPos, LocalToWorldMatrix );
        OutNormal = mul( In.Normal, LocalToWorldMatrix );
        OutTangent = mul( In.Tangent, LocalToWorldMatrix );
    }

    Out.ObjViewDir = normalize( obj_camera_position - OutPos );
    Out.Position = mul( float4( OutPos, 1 ), world_view_proj_matrix );
    Out.Normal = OutNormal;
    Out.Tangent = OutTangent;
    Out.Binormal = cross( OutNormal, OutTangent );
    Out.Tex0 = In.Tex0;
    return Out;
}


void SkinVSMemExport( VS_IN In, int Index: INDEX )
{
    float4 InPos = float4( In.Position, 1 );
    
    float BoneWeights[4] = (float[4])In.BoneWeights;
    int BoneIndices[4] = (int[4])In.BoneIndices;
    
    float4x3 LocalToWorldMatrix = 0;
    for( int i = 3; i >= 0; --i )
    {
        LocalToWorldMatrix += BoneWeights[i] * TFetchBoneMatrix( BoneIndices[i] );
    }
    
    float3 OutPos = 0;
    float3 OutNormal = 0;
    float3 OutTangent = 0;
    
    [isolate]
    {
        OutPos = mul( InPos, LocalToWorldMatrix );
        OutNormal = mul( In.Normal, LocalToWorldMatrix );
        OutTangent = mul( In.Tangent, LocalToWorldMatrix );
    }

    // The memexport hardware doesn't know about DEC3N types so we have to encode the values ourselves.
    OutNormal *= 511.0;
    OutTangent *= 511.0;
    
    float OutX = OutPos.x;
    float OutY = OutPos.y;
    float OutZ = OutPos.z;
    
    // The vertex struct size is equal to 6 FLOATs, and the position is the first three.
    int PositionIndex = ( Index * 6 ) + 0;
    asm
    {
        // we need alloc export=2 because we're writing to more than just eM0
        alloc export=2
        mad eA, PositionIndex, const01, export_address_position
        mov eM0, OutX
        mov eM1, OutY
        mov eM2, OutZ
    };
    
    // The vertex struct size is equal to 6 FLOAT16_2s, and the texcoord0 is element 3.
    int TexCoordIndex = ( Index * 6 ) + 3;
    float2 InTex0 = In.Tex0;
    asm
    {
        alloc export=1
        mad eA, TexCoordIndex, const01, export_address_texcoord0
        mov eM0, InTex0
    };
    
    // The vertex struct size is equal to 6 DEC3Ns, and the normal and tangent
    // are elements 4 and 5.
    int NormalTangentIndex = ( Index * 6 ) + 4;
    asm
    {
        alloc export=2 // we need alloc export=2 because we're writing to more than just eM0
        mad eA, NormalTangentIndex, const01, export_address_normal_tangent
        mov eM0, OutNormal
        mov eM1, OutTangent
    };
}


VS_OUT TransformVS( VS_IN_NOWEIGHTS In )
{
    VS_OUT Out;
    Out.ObjViewDir = normalize( obj_camera_position - In.Position );
    Out.Position = mul( float4( In.Position, 1 ), world_view_proj_matrix );
    Out.Normal = In.Normal;
    Out.Tangent = In.Tangent;
    Out.Binormal = cross( In.Normal, In.Tangent );
    Out.Tex0 = In.Tex0;
    return Out;
}

float4 SolidColorPS() : COLOR
{
    return 1;
}

float3 RecoverXYZFromNormalMapSample( float2 NormalMapSample )
{
    // Expand normal map sample to -1..1 range.
    NormalMapSample = NormalMapSample * 2.0 - 1.0;
    float3 result = float3( NormalMapSample.x, NormalMapSample.y, saturate( 1 - dot( NormalMapSample, NormalMapSample ) ) );
    return result;
}

float3 BumpNormalToObjectSpace( float3 Normal, float3 Binormal, float3 Tangent, float3 SampledNormal )
{
    float3 obj_binormal = normalize( Binormal );
    float3 obj_tangent = normalize( Tangent );
    float3 obj_normal = normalize( Normal );
    float3 obj_bumpnormal = obj_tangent * SampledNormal.x +
                            obj_binormal * SampledNormal.y +
                            obj_normal * SampledNormal.z;
                            
    return normalize( obj_bumpnormal );
}

float4 NormalMapPS( PS_IN In ) : COLOR
{
    // Sample and decode normal map texture.
    float2 NormalMapXY = tex2D( normal_map_texture, In.Tex0 ).xy;
    float3 NormalMapSample = RecoverXYZFromNormalMapSample( NormalMapXY );
    // Reduce the intensity of the normal map sample.
    NormalMapSample.xy *= 0.6;
    
    // Convert normal map sample to object space.
    float3 ObjNormalMapSample = BumpNormalToObjectSpace( In.Normal, In.Binormal, In.Tangent, NormalMapSample );
    
    // Compute diffuse lighting term from directional light.
    float DiffuseLightAmount = saturate( dot( ObjNormalMapSample, -directional_light_direction ) );
    float4 DiffuseColor = tex2D( diffuse_texture, In.Tex0 );
    DiffuseColor *= ( directional_light_color * saturate( DiffuseLightAmount ) + ambient_light_color );
    
    // Compute specular lighting term from directional light.
    float3 ObjViewDir = normalize( In.ObjViewDir );
    float3 ReflectedView = reflect( ObjViewDir, ObjNormalMapSample );
    float SpecularLightAmount = pow( saturate( dot( -ReflectedView, ObjNormalMapSample ) ), 16.0 );
    float SpecularMap = tex2D( specular_texture, In.Tex0 ).x;
    SpecularLightAmount *= SpecularMap;
    SpecularLightAmount *= DiffuseLightAmount;
    float4 SpecularColor = directional_light_color * SpecularLightAmount;
    
    return DiffuseColor + SpecularColor;
}


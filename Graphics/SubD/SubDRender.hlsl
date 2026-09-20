//--------------------------------------------------------------------------------------
// SubDRender.hlsl
//
// Modified bicubic Bezier patch tessellation vertex shader.
// Patch data is provided in a 20-point ACC2 patch, which is evaluated for
// position and tangent data per tessellated vertex.
//
// XNA Developer Connection Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

// vertex shader constants
float4x4 g_matWVP           : register(c0); 
float4x4 g_matWorld         : register(c4);
float4   g_vCameraPosWorld  : register(c8);

// pixel shader constants
float3   g_vLightDir  : register(c0);

// pixel shader samplers
#define TEXTURE_SAMPLER sampler_state { MipFilter = LINEAR; MinFilter = LINEAR; MagFilter = LINEAR; AddressU = WRAP; AddressV = WRAP; }
sampler diffuse_sampler : register(s0) = TEXTURE_SAMPLER;
sampler normal_sampler : register(s1) = TEXTURE_SAMPLER;
sampler specular_sampler : register(s2) = TEXTURE_SAMPLER;

//--------------------------------------------------------------------------------------
// Name: QuadPatchVS
// Desc: Tessellator vertex shader that evaluates the ACC2 patch.
//--------------------------------------------------------------------------------------
[maxtempreg(36)]
void QuadPatchVS( in  int    vIndex     : INDEX,
                  in  float2 vUV        : BARYCENTRIC,
                  in  int    vQuadID    : QUADID,
                  out float4 oPosition  : POSITION,
                  out float3 oNormal    : NORMAL,
                  out float2 oTexCoord  : TEXCOORD0,
                  out float3 oTangent   : TANGENT,
                  out float3 oBinormal  : BINORMAL,
                  out float3 oPixelToCamera : TEXCOORD1 ) 
{
    // Decode the vQuadID input from the tessellator
    if ( vQuadID == 1 )
    {
        vUV.x = ( 1 - vUV.x );
    } 
    else if ( vQuadID == 2 )
    {
        vUV.y = ( 1 - vUV.y );
        vUV.x = ( 1 - vUV.x );
    } 
    else if ( vQuadID == 3 )
    {
        vUV.y = ( 1 - vUV.y );
    }
    
    // Compute the weights from the parametric coordinates
    float4 uWeights = { ( 1.0 - vUV.x) * ( 1.0 - vUV.x ) * ( 1.0 - vUV.x ),
                        3 * ( 1.0 - vUV.x ) * ( 1.0 - vUV.x ) * vUV.x,
                        3 * ( 1.0 - vUV.x ) * vUV.x * vUV.x,
                        vUV.x * vUV.x * vUV.x };

    float4 vWeights = { ( 1.0 - vUV.y) * ( 1.0 - vUV.y ) * ( 1.0 - vUV.y ),
                        3 * ( 1.0 - vUV.y ) * ( 1.0 - vUV.y ) * vUV.y,
                        3 * ( 1.0 - vUV.y ) * vUV.y * vUV.y,
                        vUV.y * vUV.y * vUV.y };
                        
                        
    float4 u3Weights = {-( 1 - vUV.x ) * ( 1 - vUV.x ), 
                        ( 1 - vUV.x ) * ( 1 - 3 * vUV.x ), 
                        vUV.x * ( 2 - 3 * vUV.x ), 
                        vUV.x * vUV.x };
    
    
    float4 v3Weights = {-( 1 - vUV.y ) * ( 1 - vUV.y ), 
                        ( 1 - vUV.y ) * ( 1 - 3 * vUV.y ), 
                        vUV.y * ( 2 - 3 * vUV.y ), 
                        vUV.y * vUV.y };

    float4 uD = 0;
    float4 vD = 0;
    
    // Compute the index in the topology data
    // There are 32 ints per patch, broken into 8 vector4s.
    // We want the first vector4 of the patch.
    int TopologyIndex = vIndex.x * 8;
    int4 VertexIndices;
         
    // Fetch the 20 control points                   
    float4 pos01, pos02, pos10, pos110, pos111, pos120, pos121, pos13, pos20, pos210, pos211, pos220, pos221, pos23, pos31, pos32;  
    float4 pos00, pos03, pos30, pos33;
    
    asm {
        vfetch pos00, vIndex.x, position0
        vfetch pos01, vIndex.x, position1
        vfetch pos02, vIndex.x, position2
        vfetch pos03, vIndex.x, position3
        vfetch pos10, vIndex.x, position4
        vfetch pos111, vIndex.x, position5
        vfetch pos121, vIndex.x, position6
        vfetch pos13, vIndex.x, position7
        vfetch pos20, vIndex.x, position8, UseTextureCache=true
        vfetch pos211, vIndex.x, position9, UseTextureCache=true
        vfetch pos221, vIndex.x, position10, UseTextureCache=true
        vfetch pos23, vIndex.x, position11, UseTextureCache=true
        vfetch pos30, vIndex.x, position12, UseTextureCache=true
        vfetch pos31, vIndex.x, position13, UseTextureCache=true
        vfetch pos32, vIndex.x, position14, UseTextureCache=true
        vfetch pos33, vIndex.x, position15, UseTextureCache=true
        vfetch pos110, vIndex.x, texcoord12
        vfetch pos210, vIndex.x, texcoord13
        vfetch pos220, vIndex.x, texcoord14
        vfetch pos120, vIndex.x, texcoord15
    };
    
    // Fetch the four texcoords and tangent vectors for the mesh vertices on the corners of this patch
    float4 tex01, tex23;
    float3 tangent0, tangent1, tangent2, tangent3;
    asm {
        vfetch tex01, vIndex.x, texcoord0
        vfetch tex23, vIndex.x, texcoord1
        vfetch tangent0.xyz, vIndex.x, tangent0
        vfetch tangent1.xyz, vIndex.x, tangent1
        vfetch tangent2.xyz, vIndex.x, tangent2
        vfetch tangent3.xyz, vIndex.x, tangent3
    };

    float4 u0, u1, u2, u3;
    float4 v0, v1, v2, v3;

    // Evaluate the patch
    u0 = pos00 * uWeights.x + pos10 * uWeights.y + pos20 * uWeights.z + pos30 * uWeights.w;
    u1 = pos01 * uWeights.x + pos111 * uWeights.y + pos211 * uWeights.z + pos31 * uWeights.w;
    u2 = pos02 * uWeights.x + pos121 * uWeights.y + pos221 * uWeights.z + pos32 * uWeights.w;
    u3 = pos03 * uWeights.x + pos13 * uWeights.y + pos23 * uWeights.z + pos33 * uWeights.w;

    v0 = pos00 * vWeights.x + pos01 * vWeights.y + pos02 * vWeights.z + pos03 * vWeights.w;
    v1 = pos10 * vWeights.x + pos110 * vWeights.y + pos120 * vWeights.z + pos13 * vWeights.w;
    v2 = pos20 * vWeights.x + pos210 * vWeights.y + pos220 * vWeights.z + pos23 * vWeights.w;
    v3 = pos30 * vWeights.x + pos31 * vWeights.y + pos32 * vWeights.z + pos33 * vWeights.w;

    oPosition = 0.5*(
        u0 * vWeights.x + u1 * vWeights.y + u2 * vWeights.z + u3 * vWeights.w + 
        v0 * uWeights.x + v1 * uWeights.y + v2 * uWeights.z + v3 * uWeights.w); 

    // Compute the patch U and V tangents at this location
    vD = u0 * v3Weights.x + u1 * v3Weights.y + u2 * v3Weights.z + u3 * v3Weights.w;
    uD = v0 * u3Weights.x + v1 * u3Weights.y + v2 * u3Weights.z + v3 * u3Weights.w;

    float3 normal = normalize( cross( uD.xyz, vD.xyz ) );
            
    // transform the position and normal
    oPosition.w = 1;
    float3 vPositionWorld = mul( g_matWorld, oPosition );
    oPosition = mul( g_matWVP, oPosition );         // position (view space)

    oNormal = mul( (float3x3)g_matWorld, normal );   // normal (view space)    
    
    // compute the final texture coordinate
    float2 texlerp01 = lerp( tex01.xy, tex01.zw, vUV.x );
    float2 texlerp32 = lerp( tex23.zw, tex23.xy, vUV.x );
    oTexCoord = lerp( texlerp01, texlerp32, vUV.y );
    
    // compute the texture tangent vector
    float3 tangent01 = lerp( tangent0, tangent1, vUV.x );
    float3 tangent23 = lerp( tangent3, tangent2, vUV.x );
    float3 tangent = lerp( tangent01, tangent23, vUV.y );
    
    oTangent = tangent;
    oBinormal = cross( normal, tangent );
    
    oPixelToCamera = g_vCameraPosWorld - vPositionWorld;
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

//--------------------------------------------------------------------------------------
// Name: QuadPatchPS
// Desc: Simple diffuse + ambient light shader with no texturing.
//--------------------------------------------------------------------------------------
float4 QuadPatchPS( float3 normal : NORMAL, float2 texcoord : TEXCOORD0, float3 tangent : TANGENT, float3 binormal : BINORMAL, float3 pixeltocamera : TEXCOORD1 ) : COLOR0
{
    // Sample and decode normal map texture.
    float2 NormalMapXY = tex2D( normal_sampler, texcoord ).xy;
    float3 NormalMapSample = RecoverXYZFromNormalMapSample( NormalMapXY );
    // Reduce the intensity of the normal map sample.
    NormalMapSample.xy *= 0.6;
    
    // Convert normal map sample to object space.
    float3 ObjNormalMapSample = BumpNormalToObjectSpace( normal, binormal, tangent, NormalMapSample );
    
    const float4 vDirLightColor = float4( 1, 1, 1, 1 );
    const float4 vAmbientLightColor = float4( 0.05, 0.05, 0.05, 1 );
    const float SpecularIntensity = 0.6f;
    
    // Compute diffuse lighting term from directional light.
    float DiffuseLightAmount = saturate( dot( ObjNormalMapSample, g_vLightDir ) );
    float4 DiffuseColor = tex2D( diffuse_sampler, texcoord );
    DiffuseColor *= ( vDirLightColor * saturate( DiffuseLightAmount ) + vAmbientLightColor );
    
    // Compute specular lighting term from directional light.
    float3 HalfVector = normalize( normalize( pixeltocamera ) + g_vLightDir );
    float SpecularLightAmount = pow( saturate( dot( HalfVector, ObjNormalMapSample ) ), 16.0 );
    float SpecularMap = tex2D( specular_sampler, texcoord ).x;
    SpecularLightAmount *= SpecularMap * SpecularIntensity;
    float4 SpecularColor = vDirLightColor * SpecularLightAmount;
    
    return DiffuseColor + SpecularColor;
}

float4 QuadPatchSimplePS( float3 normal : NORMAL ) : COLOR0
{
    float4 oColor = float4( 0.05, 0.05, 0.05, 1 );

    normal = normalize( normal );
    oColor += 0.9 * saturate( dot( normal, g_vLightDir ) );
    
    return oColor;    
}

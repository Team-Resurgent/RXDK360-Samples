//------------------------------------------------------------------------------
// Vertex shader used with per-pixel-lighting
//------------------------------------------------------------------------------


struct VSOUT
{
    float4 Position         : POSITION;
    float2 TexCoord0        : TEXCOORD0;
    float3 PointLight       : TEXCOORD1;
    float3 DirLight         : TEXCOORD2;
};


uniform float4x4 WorldViewProj                : register(c4);  // World-view-projection matrix
uniform float4   ObjectSpaceDirLightDirection : register(c10); // ObjectSpaceDirLightDirection
uniform float4   ObjectSpacePtLightPosition   : register(c11); // ObjectSpacePtLightPosition


VSOUT ShadePerPixelLightingVertex( const float3 Position   : POSITION,
                                   const float3 Normal     : NORMAL, 
                                   const float2 TexCoord0  : TEXCOORD0, 
                                   const float3 TSTangent  : TANGENT, 
                                   const float3 TSBiNormal : BINORMAL, 
                                   const float3 TSNormal   : NORMAL1 )
{
    VSOUT  Output;
    float3 vPtLight;

    // Transform the vertex
    Output.Position = mul( float4(Position, 1.0f), WorldViewProj );

    // Directional light vector output in the diffuse component
    // DirLight = vObjectSpaceDirLightDirection dot [ T B N ]
    Output.DirLight.x = dot( ObjectSpaceDirLightDirection, TSTangent );
    Output.DirLight.y = dot( ObjectSpaceDirLightDirection, TSNormal );
    Output.DirLight.z = dot( ObjectSpaceDirLightDirection, TSBiNormal );

    // vPtLight = vPtLightWorldPos - pVertex->p;
    vPtLight = ObjectSpacePtLightPosition - Position;

    // Pass thru base texcoords
    Output.TexCoord0 = TexCoord0;

    // Point light vector output in the texture coord set 1
    // oT1 = vPtLight dot [ T B N ]
    Output.PointLight.x = dot( vPtLight, TSTangent );
    Output.PointLight.y = dot( vPtLight, TSNormal );
    Output.PointLight.z = dot( vPtLight, TSBiNormal );

    return Output;
}


uniform extern sampler DiffuseMap               : register(s0); // Diffuse texture
uniform extern sampler BumpMap                  : register(s1); // Normal map
uniform extern sampler NormalizationMap         : register(s2); // Normalization cube map

uniform extern float4 g_vAmbientLightColor      : register(c0); // Color of ambient light
uniform extern float4 g_vDirectionalLightColor  : register(c1); // Color of directional light
uniform extern float4 g_vPointLightColor        : register(c2); // Color of point light
uniform extern float4 g_vTextureScale           : register(c3); // Controls texture application

float4 ShadePerPixelLightingPixel( VSOUT Input ) : COLOR
{
    float4 DiffuseTexture = tex2D( DiffuseMap, Input.TexCoord0 );
    DiffuseTexture = lerp( 1, DiffuseTexture, g_vTextureScale );
    
    float3 BumpedNormal = tex2D( BumpMap, Input.TexCoord0 );
    
    // Individual lighting contributions.  Note that you could use either 
    // the HLSL normalize() function or a normalization cube map.
    float4 vDirectionalLight = saturate( dot( BumpedNormal, Input.DirLight ) );

    float3 vNormalizedPointLight = normalize( Input.PointLight );
    // float3 vNormalizedPointLight = texCUBE( NormalizationMap, Input.PointLight );
    float4 vPointLight = saturate( dot( BumpedNormal, vNormalizedPointLight ) );
    
    // Total lighting
    float4 vTotalLighting = saturate( g_vAmbientLightColor + 
                                      vDirectionalLight * g_vDirectionalLightColor + 
                                      vPointLight * g_vPointLightColor );
    
    return vTotalLighting * DiffuseTexture;
}
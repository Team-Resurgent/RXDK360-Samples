//------------------------------------------------------------------------------
// Vertex shader used with per-pixel-lighting & parallax mapping
//------------------------------------------------------------------------------

struct VSOUT
{
    float4 Position         : POSITION;
    float2 TexCoord0        : TEXCOORD0;
    float3 PointLight       : TEXCOORD1;
    float3 EyeVector        : TEXCOORD2;
};

uniform float4x4 WorldViewProj                : register(c4);  // World-view-projection matrix
uniform float4   ObjectSpacePtLightPosition   : register(c21); // ObjectSpacePtLightPosition
uniform float4   ObjectSpaceEyeVector         : register(c22); // ObjectSpacePtLightPosition

VSOUT ParallaxMappingVertex( const float3 Position   : POSITION,
                                   const float2 TexCoord0  : TEXCOORD0,
                                   const float3 TSTangent  : TANGENT, 
                                   const float3 TSNormal   : NORMAL0,
                                   const float3 TSBiNormal : BINORMAL 
)
{
    VSOUT  Output;
    float3 vPtLight;
    float3 vEye;

    // Transform the vertex
    Output.Position = mul( float4(Position, 1.0f), WorldViewProj );

    // vPtLight = vPtLightWorldPos - pVertex->p;
    vPtLight = - ObjectSpacePtLightPosition + Position;
    vEye = - ObjectSpaceEyeVector + Position;

    // Pass thru base texcoords
    Output.TexCoord0 = TexCoord0;

    // Point light vector output in the texture coord set 1
    // oT1 = vPtLight dot [ T B N ]
    Output.PointLight.x = dot( vPtLight, TSTangent );
    Output.PointLight.y = dot( vPtLight, TSBiNormal );
    Output.PointLight.z = dot( vPtLight, TSNormal );

    Output.EyeVector.x = dot( vEye, TSTangent );
    Output.EyeVector.y = dot( vEye, TSBiNormal );
    Output.EyeVector.z = dot( vEye, TSNormal );

    return Output;
}


//------------------------------------------------------------------------------
// Pixel shader used with per-pixel-lighting & parallax mapping
//------------------------------------------------------------------------------

uniform extern sampler DiffuseMap               : register(s0); // Diffuse texture
uniform extern sampler BumpMap                  : register(s1); // Normal map
uniform extern sampler NormalizationMap         : register(s2); // Normalization cube map 

uniform extern float4 g_vAmbientLightColor      : register(c0); // Color of ambient light
uniform extern float4 g_vPointLightColor        : register(c2); // Color of point light
uniform extern float4 g_vTextureScale           : register(c3); // Controls texture application
uniform extern float4 g_vParallaxFactor         : register(c4); // Controls parallax application


float4 ParallaxMappingPixel( VSOUT Input ) : COLOR
{
    float4 BumpedNormal = tex2D( BumpMap, Input.TexCoord0 );
    float3 vNormalizedEyeVector = normalize( Input.EyeVector );
    float2 NewTexCoord = Input.TexCoord0
                -vNormalizedEyeVector * BumpedNormal.w * g_vParallaxFactor.x;

    // Fetch texture again    
    BumpedNormal = tex2D( BumpMap, NewTexCoord );

    float4 DiffuseTexture = tex2D( DiffuseMap, NewTexCoord );
    DiffuseTexture = lerp( 1, DiffuseTexture, g_vTextureScale.xxxx );
    
    // Individual lighting contributions.
    float3 vNormalizedNormal = normalize( 2.f * BumpedNormal.xyz -1.f ) ;
    float3 vNormalizedPointLight = normalize( Input.PointLight );
    float3 vPointLight = dot( vNormalizedNormal, vNormalizedPointLight );
    
    float3 vHalf = normalize( vNormalizedPointLight + vNormalizedEyeVector );
    float3 vSpecularColor = pow ( max( 0, dot( vNormalizedNormal, vHalf ) ), 50 ) ;
    
    return float4( vPointLight, 1.f ) * DiffuseTexture + float4( vSpecularColor, 1.f );
}


float4 BumpMappingPixel( VSOUT Input ) : COLOR
{
    float4 BumpedNormal = tex2D( BumpMap, Input.TexCoord0 );
    float3 vNormalizedEyeVector = normalize( Input.EyeVector );
    float4 DiffuseTexture = tex2D( DiffuseMap, Input.TexCoord0 );
    DiffuseTexture = lerp( 1, DiffuseTexture, g_vTextureScale.xxxx );
    
    // Individual lighting contributions.
    float3 vNormalizedNormal = normalize( 2.f * BumpedNormal.xyz -1.f ) ;
    float3 vNormalizedPointLight = normalize( Input.PointLight );
    float3 vPointLight = dot( vNormalizedNormal, vNormalizedPointLight );
    
    float3 vHalf = normalize( vNormalizedPointLight + vNormalizedEyeVector );
    float3 vSpecularColor = pow ( max( 0, dot( vNormalizedNormal, vHalf ) ), 50 ) ;
    
    return float4( vPointLight, 1.f ) * DiffuseTexture + float4( vSpecularColor, 1.f );
}


float4 NormalMapPixel( VSOUT Input ) : COLOR
{
    float4 BumpedNormal = tex2D( BumpMap, Input.TexCoord0 );
    return BumpedNormal;
}


float4 HeightMap( VSOUT Input ) : COLOR
{
    float4 BumpedNormal = tex2D( BumpMap, Input.TexCoord0 );
    return float4( BumpedNormal.w, BumpedNormal.w, BumpedNormal.w, 1.f);
}

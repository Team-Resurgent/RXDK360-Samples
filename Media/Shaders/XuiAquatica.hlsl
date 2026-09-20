//--------------------------------------------------------------------------------------
// Shader for the XuiAquatica sample
//--------------------------------------------------------------------------------------

struct VSOUT
{
    float4 vPosition    : POSITION;
    float4 vLightAndFog : COLOR0_center;     // COLOR0.x = light, COLOR0.y = fog
    float4 vTexCoords   : TEXCOORD0;  // TEXCOORD0.xy = basetex, TEXCOORD0.zw = caustictex
    float3 vTangent     : TEXCOORD1;
    float3 vBinormal    : TEXCOORD2;
    float3 vNormal      : TEXCOORD3;
};


//--------------------------------------------------------------------------------------
// Vertex shader constants
//--------------------------------------------------------------------------------------
uniform float4   g_vZero             : register(c0);  // ( 0, 0, 0, 0 )
uniform float4   g_vConstants        : register(c1);  // ( 1, 0.5, -, - )
uniform float3   g_vBlendWeights     : register(c2);  // ( fWeight1, fWeight2, fWeight3, 0 )
uniform float4x4 g_matWorldViewProj  : register(c4);  // world-view-projection matrix
uniform float4x4 g_matWorldView      : register(c8);  // world-view matrix
uniform float4x4 g_matView           : register(c12); // view matrix
uniform float4x4 g_matProjection     : register(c16); // projection matrix
uniform float3   g_vSeafloorLightDir : register(c20); // seafloor light direction
uniform float3   g_vFishLightDir     : register(c21); // fish light direction
uniform float4   g_vFogRange         : register(c24); // ( x, fog_end, (1/(fog_end-fog_start)), x)
uniform float4x4 g_matModel          : register(c25); // model transformation matrix


//--------------------------------------------------------------------------------------
// Pixel shader constants
//--------------------------------------------------------------------------------------
sampler TextureSampler0 : register(s0);
sampler TextureSampler1 : register(s1);
sampler NormalMapTexture : register(s2);

uniform float4 g_vFogColor          : register(c0); // Fog color
uniform float3 g_vAmbient           : register(c1); // Material ambient color
uniform float3 g_vLightDir          : register(c2); // Light direction
uniform bool   g_bEnableNormalMap   : register(c3); // Enable normal mapping


//--------------------------------------------------------------------------------------
// Name: ShadeSceneVertex
// Desc: Vertex shader for the scene
//--------------------------------------------------------------------------------------
VSOUT ShadeSceneVertex( const float3 vPosition      : POSITION,
                        const float3 vNormal        : NORMAL,
                        const float3 vTangent       : TANGENT,
                        const float3 vBinormal      : BINORMAL,
                        const float2 vBaseTexCoords : TEXCOORD0 )
{
    // Transform to view space (world matrix is identity)
    float4 vViewPosition = mul( float4(vPosition, 1.0f), g_matView );

    // Transform to projection space
    float4 vOutputPosition = mul( vViewPosition, g_matProjection );

    // Lighting calculation
    float fLightValue = max( dot( vNormal, g_vSeafloorLightDir ), g_vZero.x );

    // Generate water caustic tex coords from vertex xz position
    float2 vCausticTexCoords = 0.003 * vPosition.xz;
    
    // Fog calculation:
    float fFogValue = clamp( (g_vFogRange.y - vViewPosition.z) * g_vFogRange.z, g_vZero.x, g_vConstants.x );

    // Compress output values
    VSOUT  Output;
    Output.vPosition      = vOutputPosition;
    Output.vLightAndFog.x = fLightValue;
    Output.vLightAndFog.y = fFogValue;
    Output.vTexCoords.xy  = vBaseTexCoords;
    Output.vTexCoords.zw  = vCausticTexCoords;
    
    Output.vLightAndFog.z = 0.0f;
    Output.vLightAndFog.w = 0.0f;
    
    Output.vNormal = vNormal;
    Output.vTangent = vTangent;
    Output.vBinormal = vBinormal;
    
    return Output;
}


//--------------------------------------------------------------------------------------
// Name: ShadeFishVertex
// Desc: Vertex shader for the fish
//--------------------------------------------------------------------------------------
VSOUT ShadeFishVertex( const float3 vPosition0     : POSITION0,
                       const float3 vPosition1     : POSITION1,
                       const float3 vPosition2     : POSITION2,
                       const float3 vNormal0       : NORMAL0,
                       const float3 vNormal1       : NORMAL1,
                       const float3 vNormal2       : NORMAL2,
                       const float2 vBaseTexCoords : TEXCOORD0 )
{
    // Tween the 3 positions (v0,v1,v2) into one position
    float4 vModelPosition = float4( vPosition0 * g_vBlendWeights.x + vPosition1 * g_vBlendWeights.y + vPosition2 * g_vBlendWeights.z, 1.0f );

    // Transform position to the clipping space
    float4 vOutputPosition = mul( vModelPosition, g_matWorldViewProj );
    
    // Transform position to the camera space
    float4 vViewPosition = mul( vModelPosition, g_matWorldView );
    
    // Transform model
    float4 vModelTransformed = mul( vModelPosition, g_matModel );

    // Tween the 3 normals (v3,v4,v5) into one normal
    float3 vModelNormal = vNormal0 * g_vBlendWeights.x + vNormal1 * g_vBlendWeights.y + vNormal2 * g_vBlendWeights.z;

    // Do the lighting calculation
    float fLightValue = max( dot( vModelNormal, g_vFishLightDir ), g_vZero.x );

    // Generate water caustic tex coords from vertex xz position
    float2 vCausticTexCoords = 0.003 * vModelTransformed.xz;

    // Fog calculation:
    float fFogValue = clamp( (g_vFogRange.y - vViewPosition.z) * g_vFogRange.z, g_vZero.x, g_vConstants.x );

    // Compress output values
    VSOUT  Output;
    Output.vPosition      = vOutputPosition;
    Output.vLightAndFog.x = fLightValue;
    Output.vLightAndFog.y = fFogValue;
    Output.vTexCoords.xy  = vBaseTexCoords;
    Output.vTexCoords.zw  = vCausticTexCoords;

    Output.vLightAndFog.z = 0.0f;
    Output.vLightAndFog.w = 1.0f;
    
    Output.vNormal = float3( 0, 0, 0 );
    Output.vTangent = float3( 0, 0, 0 );
    Output.vBinormal = float3( 0, 0, 0 );

    return Output;
}


//--------------------------------------------------------------------------------------
// Name: ShadeCausticsPixel()
// Desc: Pixel shader to add underwater caustics to a lit base texture.
//--------------------------------------------------------------------------------------
float4 ShadeCausticsPixel( VSOUT Input ) : COLOR
{
    // Decompress input values
    float3 vLightColor       = Input.vLightAndFog.xxx;
    float  fFogValue         = Input.vLightAndFog.y;
    float2 vBaseTexCoords    = Input.vTexCoords.xy;
    float2 vCausticTexCoords = Input.vTexCoords.zw;
    
    if( g_bEnableNormalMap )
    {
        // Compute normal mapping parameters
        float2 NormalMapSample = tex2D( NormalMapTexture, vBaseTexCoords );
        NormalMapSample = NormalMapSample * 2.0 - 1.0;
        // Recover the XYZ vector from the XY normal map sample.
        float3 normalMap = normalize( float3( NormalMapSample, 1 ) );
        
        float3 diffuseBump;
        diffuseBump.x = dot( Input.vTangent, g_vLightDir );
        diffuseBump.y = dot( Input.vBinormal, g_vLightDir );
        diffuseBump.z = dot( normalize( Input.vNormal ), g_vLightDir );
        
        vLightColor.r = saturate( dot( diffuseBump, normalMap ) );
        vLightColor.g = vLightColor.r;
        vLightColor.b = vLightColor.r;
    }

    // Combine lighting, base texture and water caustics texture
    float4 PixelColor0 = tex2D( TextureSampler0, vBaseTexCoords )    * float4( vLightColor + g_vAmbient, 1 );
    float4 PixelColor1 = tex2D( TextureSampler1, vCausticTexCoords ) * float4( vLightColor, 1 );
    
    // Return color blended with fog
    return float4( lerp( g_vFogColor.rgb, PixelColor0.rgb + PixelColor1.rgb, fFogValue ), PixelColor0.a );
}


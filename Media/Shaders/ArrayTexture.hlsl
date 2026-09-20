//--------------------------------------------------------------------------------------
// Shaders for the ArrayTexture sample
//--------------------------------------------------------------------------------------

struct VSOUT
{
    float4 vPosition         : POSITION;
    // Forcing the vLightAndFog pixel shader input to use center interpolation
    // in order to avoid the interpolation penalty when mixing center and centroid.
    float2 vLightAndFog      : COLOR0_center;     // COLOR0.x = light, COLOR0.y = fog
    // Pack the base and caustic tex-coords into one float4 in order
    // to reduce the number of interpolators and avoid being
    // interpolator bound. x,y are the base tex-coords and z,w are for the caustics
    float4 vTexCoords        : TEXCOORD1;
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
uniform float3   g_vDolphinLightDir  : register(c21); // dolphin light direction
uniform float4   g_vFogRange         : register(c24); // ( x, fog_end, (1/(fog_end-fog_start)), x)
uniform float4   g_vTexGen           : register(c25);


//--------------------------------------------------------------------------------------
// Pixel shader constants
//--------------------------------------------------------------------------------------
sampler   DiffuseSampler  : register(s0);
sampler   CausticsSampler : register(s1);

uniform float4 g_vFogColor        : register(c0); // Fog color
uniform float3 g_vAmbient         : register(c1); // Material ambient color
// Used to animated the water caustics when using an array texture for the caustics.
// Left as zero otherwise.
uniform float  g_fCausticTexCoord : register(c2);


//--------------------------------------------------------------------------------------
// Name: ShadeSeaFloorVertex()
// Desc: Vertex shader for the seafloor
//--------------------------------------------------------------------------------------
VSOUT ShadeSeaFloorVertex( const float3 vPosition      : POSITION,
                           const float3 vNormal        : NORMAL,
                           const float2 vBaseTexCoords : TEXCOORD0 )
{
    // Transform to view space (world matrix is identity)
    float4 vViewPosition = mul( float4(vPosition, 1.0f), g_matView );

    // Transform to projection space
    float4 vOutputPosition = mul( vViewPosition, g_matProjection );

    // Lighting calculation
    float fLightValue = max( dot( vNormal, g_vSeafloorLightDir ), g_vZero.x );

    // Generate water caustic tex coords from vertex xz position
    float2 vCausticTexCoords = g_vTexGen.xx * vViewPosition.xz + g_vTexGen.zw;
    
    // Fog calculation:
    float fFogValue = clamp( (g_vFogRange.y - vViewPosition.z) * g_vFogRange.z, g_vZero.x, g_vConstants.x );

    // Compress output values
    VSOUT  Output;
    Output.vPosition         = vOutputPosition;
    Output.vLightAndFog.x    = fLightValue;
    Output.vLightAndFog.y    = fFogValue;
    Output.vTexCoords.xy     = 10*vBaseTexCoords;
    Output.vTexCoords.zw     = vCausticTexCoords;
    return Output;
}


//--------------------------------------------------------------------------------------
// Name: ShadeDolphinVertex()
// Desc: Vertex shader for the dolphin
//--------------------------------------------------------------------------------------
VSOUT ShadeDolphinVertex( const float3 vPosition0     : POSITION0,
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

    // Tween the 3 normals (v3,v4,v5) into one normal
    float3 vModelNormal = vNormal0 * g_vBlendWeights.x + vNormal1 * g_vBlendWeights.y + vNormal2 * g_vBlendWeights.z;

    // Do the lighting calculation
    float fLightValue = max( dot( vModelNormal, g_vDolphinLightDir ), g_vZero.x );

    // Generate water caustic tex coords from vertex xz position
    float2 vCausticTexCoords = g_vConstants.yy * vViewPosition.xz;

    // Fog calculation:
    float fFogValue = clamp( (g_vFogRange.y - vViewPosition.z) * g_vFogRange.z, g_vZero.x, g_vConstants.x );

    // Compress output values
    VSOUT  Output;
    Output.vPosition         = vOutputPosition;
    Output.vLightAndFog.x    = fLightValue;
    Output.vLightAndFog.y    = fFogValue;
    Output.vTexCoords.xy     = vBaseTexCoords;
    Output.vTexCoords.zw     = vCausticTexCoords;
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

    // Fetch from the textures
    float4 vDiffuse  = tex2D( DiffuseSampler,  vBaseTexCoords );
    float4 vCaustics = tex3D( CausticsSampler, float3( vCausticTexCoords, g_fCausticTexCoord ) );

    // Combine lighting, base texture and water caustics texture
    float4 PixelColor0 = vDiffuse  * float4( vLightColor + g_vAmbient, 1 );
    float4 PixelColor1 = vCaustics * float4( vLightColor, 1 );
    
    // Return color blended with fog
    return lerp( g_vFogColor, PixelColor0 + PixelColor1, fFogValue );
}


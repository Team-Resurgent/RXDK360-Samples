// We're using the shader attribute [optimizeAutoZ(true)], but by default this is switched
// off and therefore generates a shader compiler warning
#pragma warning( disable : 3608 )


//--------------------------------------------------------------------------------------
// Shader for the XuiAquatica sample
//--------------------------------------------------------------------------------------

struct VSOUT
{
    float4 vPosition    : POSITION;
    float4 vLightAndFog : COLOR0_center;    // COLOR0.x = light, COLOR0.y = fog
    float4 vTexCoords   : TEXCOORD0;        // TEXCOORD0.xy = basetex, TEXCOORD0.zw = caustictex
    float3 vTangent     : TEXCOORD1;
    float3 vBinormal    : TEXCOORD2;
    float3 vNormal      : TEXCOORD3;
    float4 vLightSpacePos : TEXCOORD4;
};

struct VSOUT_Fish
{
    float4 vPosition    : POSITION;
    float4 vLightAndFog : COLOR0_center;     // COLOR0.x = light, COLOR0.y = fog
    float4 vTexCoords   : TEXCOORD0;        // TEXCOORD0.xy = basetex, TEXCOORD0.zw = caustictex
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
uniform float4   g_vFogRange         : register(c34); // ( x, fog_end, (1/(fog_end-fog_start)), x)
uniform float4x4 g_matModel          : register(c25); // model transformation matrix
float4x4 matShadowTex                : register(c36); // Transform into light space

//--------------------------------------------------------------------------------------
// Pixel shader constants
//--------------------------------------------------------------------------------------
sampler TextureSampler0             : register(s0);
sampler TextureSampler1             : register(s1);
sampler NormalMapTexture            : register(s2);
sampler2D ShadowMapTexture          : register(s3);

uniform float4 g_vFogColor          : register(c0); // Fog color
uniform float3 g_vAmbient           : register(c1); // Material ambient color
uniform float3 g_vLightDir          : register(c2); // Light direction
uniform bool   g_bEnableNormalMap   : register(c3); // Enable normal mapping

float4 g_vPerturbTexCoordScale      : register(c0);
float4 g_vScreenSpaceAA             : register(c0);

sampler DistortionTextureSampler    : register(s0);
sampler BackBufferTextureSampler    : register(s1);
sampler DepthBufferTextureSampler   : register(s0);

//--------------------------------------------------------------------------------------
// Name: ShadeSceneVertex
// Desc: Vertex shader for the scene
//--------------------------------------------------------------------------------------
[optimizeAutoZ(true)]
VSOUT ShadeSceneVertex( const float3 vPosition      : POSITION,
                        const float3 vNormal        : NORMAL,
                        const float3 vTangent       : TANGENT,
                        const float3 vBinormal      : BINORMAL,
                        const float2 vBaseTexCoords : TEXCOORD0 )
{
    float4 vPos = float4(vPosition, 1.0f);
    
    // Transform to view space (world matrix is identity)
    float4x4 matWorldView = mul( g_matModel, g_matView );
    float4 vViewPosition = mul( matWorldView, vPos );

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
    
    // Compute the position in light space for the shadow map
    float4 vModelTransformed = mul( g_matModel, vPos );
    Output.vLightSpacePos = mul( vModelTransformed, matShadowTex );   
    
    return Output;
}


//--------------------------------------------------------------------------------------
// Name: ShadeFishVertex
// Desc: Vertex shader for the fish
//--------------------------------------------------------------------------------------
[optimizeAutoZ(true)]
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
    float4 vModelTransformed = mul( g_matModel, vModelPosition );

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
    
    Output.vNormal = vModelNormal.xyz;
    Output.vTangent = float3( 0, 0, 0 );
    Output.vBinormal = float3( 0, 0, 0 );

    Output.vLightSpacePos = 0;
    return Output;
}

// Simple position only vertex shader
void WriteDepthFishVS( in const float3 vPosition0     : POSITION0,
                       in const float3 vPosition1     : POSITION1,
                       in const float3 vPosition2     : POSITION2,
                       out float4 oPosition           : POSITION, 
                       out float2 oPosZW              : TEXCOORD0 )
{
    // Tween the 3 positions (v0,v1,v2) into one position
    float4 vModelPosition = float4( vPosition0 * g_vBlendWeights.x + vPosition1 * g_vBlendWeights.y + vPosition2 * g_vBlendWeights.z, 1.0f );

    // Transform position to the clipping space
    oPosition = mul( vModelPosition, g_matWorldViewProj );
    oPosZW = oPosition.zw;
}


// Compute the attenuation due to shadowing using bilinear PCF sampling
float ComputeShadowAttenuationBilinear( float3 vShadowCoord )
{
    // Fetch the bilinear filter fractions and four samples from the depth texture. The LOD for the 
    // fetches from the depth texture is computed using aniso filtering so that it is based on the 
    // minimum of the x and y gradients (instead of the maximum).  
    float4 Weights;
    float LOD;
    float4 SampledDepth;
    asm {
        getCompTexLOD2D LOD.x, vShadowCoord.xy, ShadowMapTexture, AnisoFilter=max16to1
        setTexLOD LOD.x

        tfetch2D SampledDepth.x___, vShadowCoord.xy, ShadowMapTexture, OffsetX = -0.5, OffsetY = -0.5, UseComputedLOD=false, UseRegisterLOD=true
        tfetch2D SampledDepth._x__, vShadowCoord.xy, ShadowMapTexture, OffsetX =  0.5, OffsetY = -0.5, UseComputedLOD=false, UseRegisterLOD=true
        tfetch2D SampledDepth.__x_, vShadowCoord.xy, ShadowMapTexture, OffsetX = -0.5, OffsetY =  0.5, UseComputedLOD=false, UseRegisterLOD=true
        tfetch2D SampledDepth.___x, vShadowCoord.xy, ShadowMapTexture, OffsetX =  0.5, OffsetY =  0.5, UseComputedLOD=false, UseRegisterLOD=true

        getWeights2D Weights, vShadowCoord.xy, ShadowMapTexture, MagFilter=linear, MinFilter=linear, UseComputedLOD=false, UseRegisterLOD=true
    };

    Weights = float4( (1-Weights.x)*(1-Weights.y), Weights.x*(1-Weights.y), (1-Weights.x)*Weights.y, Weights.x*Weights.y );
        
    float4 Attenuation = step( vShadowCoord.z, SampledDepth );
    
    return dot( Attenuation, Weights );
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
    
    // Compute projected xyz.
    Input.vLightSpacePos.xyz = Input.vLightSpacePos.xyz / Input.vLightSpacePos.w;

    // Compute the attenuation due to shadowing.
    float ShadowAttenuation = ComputeShadowAttenuationBilinear( Input.vLightSpacePos.xyz );

    // Make shadows a bit lighter
    ShadowAttenuation += 0.6f;
    ShadowAttenuation = min( ShadowAttenuation, 1.0f );
    
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

    float fDirectLight = vLightColor;
    float fIndirectLight = max( dot( Input.vNormal, float3(g_vLightDir.x,g_vLightDir.z,g_vLightDir.y) ), 0.25f );
    float fLight = fDirectLight + fIndirectLight * 0.5f;
    float3 vLight = float3( fLight, fLight, fLight );

    // Combine lighting, base texture and water caustics texture
    float4 PixelColor0 = tex2D( TextureSampler0, vBaseTexCoords )    * float4( vLight * ShadowAttenuation + g_vAmbient, 1 );
    float4 PixelColor1 = tex2D( TextureSampler1, vCausticTexCoords ) * float4( vLight, 1 );
    
    // Return color blended with fog
    return float4( lerp( g_vFogColor.rgb, PixelColor0.rgb + PixelColor1.rgb, fFogValue ), PixelColor0.a );
}

//--------------------------------------------------------------------------------------
// Name: ScreenSpaceAAPS()
//--------------------------------------------------------------------------------------
float4 ScreenSpaceAAPS( float2 vTexCoord : TEXCOORD0 ) : COLOR
{
    float4 vDepth;
    float4 vBackBuffer, vBackBufferBlurred;    

    // fetch all the textures needed
    asm
    {
        tfetch2D vDepth.x_x_, vTexCoord, DepthBufferTextureSampler, OffsetX = 0, OffsetY = 0, MinFilter = point, MagFilter = point
        tfetch2D vDepth._x__, vTexCoord, DepthBufferTextureSampler, OffsetX = 0, OffsetY = -1, MinFilter = point, MagFilter = point
        tfetch2D vDepth.___x, vTexCoord, DepthBufferTextureSampler, OffsetX = -1, OffsetY = 0, MinFilter = point, MagFilter = point
        tfetch2D vBackBuffer, BackBufferTextureSampler, vTexCoord, MinFilter = point, MagFilter = point;
        tfetch2D vBackBufferBlurred, vTexCoord, BackBufferTextureSampler, OffsetX = -0.5, OffsetY = -0.5, MinFilter = linear, MagFilter = linear, AnisoFilter = max16to1
    };

    // Calc the difference between neighbour depths to see if we should use the point sampled or linear sampled backbuffer
    float2 vDepthDiff;
    vDepthDiff.x = vDepth.x - vDepth.y;
    vDepthDiff.y = vDepth.z - vDepth.w;
    vDepthDiff = abs( vDepthDiff );
       
    float fLerp = 0.0f;
    [flatten]
    if ( vDepthDiff.x + vDepthDiff.y > g_vScreenSpaceAA.x )
    {
        fLerp = 1.0f;   
    }

    return lerp( vBackBuffer, vBackBufferBlurred, fLerp );
}

//--------------------------------------------------------------------------------------
// Name: WaterDistortionPS()
// Desc: Water distortion and tonemapping
//--------------------------------------------------------------------------------------
float4 WaterDistortionPS( float2 vTexCoord : TEXCOORD0 ) : COLOR
{
    float2 vPerturbedTexCoord = tex2D( DistortionTextureSampler, vTexCoord.xy * g_vPerturbTexCoordScale.xy );
    vPerturbedTexCoord *= g_vPerturbTexCoordScale.zw;
    float4 vBackBuffer = tex2D( BackBufferTextureSampler, vTexCoord + vPerturbedTexCoord );
          
    return vBackBuffer;
}

//--------------------------------------------------------------------------------------
// Name: FastDepthRestorePS()
// Desc: Pixel shader to restore the depth buffer from a texture by treating
//       it as an A8R8G8B8 texture.  For more information, see the Xbox 360 GPU
//       Performance Update Presentation from Gamefest 2007.
//--------------------------------------------------------------------------------------
static const float EDRAM_TILE_WIDTH = 80.0f;    // See GPU_EDRAM_TILE_WIDTH_1X in documentation

sampler BackBufferTexture           : register(s0);
sampler DepthBufferTextureAsAAAA    : register(s1);

struct PS_OUT_RESTORE
{
    float4 BackBuffer  : COLOR0;
    float4 DepthBuffer : COLOR1;
};

PS_OUT_RESTORE FastDepthRestorePS( float2 ScreenPos : VPOS )
{
    PS_OUT_RESTORE Output;
    
    float ColumnIndex = ScreenPos.x / EDRAM_TILE_WIDTH;
    float HalfColumn = frac( ColumnIndex );
    float2 TexCoord = ScreenPos;
    if( HalfColumn >= 0.5 )
        TexCoord.x -= ( EDRAM_TILE_WIDTH / 2 );
    else
        TexCoord.x += ( EDRAM_TILE_WIDTH / 2 );

    float4 DepthData, BackBufferData;
    asm
    {
        tfetch2D DepthData, TexCoord, DepthBufferTextureAsAAAA, UnnormalizedTextureCoords = true, MinFilter = point, MagFilter = point
        tfetch2D BackBufferData, ScreenPos, BackBufferTexture, UnnormalizedTextureCoords = true, MinFilter = point, MagFilter = point
    };

    Output.BackBuffer = BackBufferData;
    Output.DepthBuffer = DepthData.zyxw;
    
    return Output;
}

//--------------------------------------------------------------------------------------
// Name: PostProcessVS()
// Desc: Vertex shader for a full screen quad for post processing
//--------------------------------------------------------------------------------------
float4 RestoreBuffersVS( const float2 ObjPos: POSITION ) : POSITION
{
    return float4( ObjPos, 0, 1 );
}

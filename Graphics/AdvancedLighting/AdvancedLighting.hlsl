//--------------------------------------------------------------------------------------
// AdvancedLighting.hlsl
//
// This sample demonstrates the use of reflective shadowmaps to calculate one bounce of
// indirect lighting for dynamic scenes.
//
// XNA Developer Connection.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "AdvancedLighting.h"

// We're using the shader attribute [optimizeAutoZ(true)], but by default this is switched
// off and therefore generates a shader compiler warning
#pragma warning( disable : 3608 )

//--------------------------------------------------------------------------------------
// Shader globals shared between pixel and vertex shaders
//--------------------------------------------------------------------------------------
float4x4 g_matLightWVP                  : register( BIND_matLightWVP );
float4x4 g_matSampleLightWVP            : register( BIND_matSampleLightWVP );
float4   g_vWorldSpaceLightDirection    : register( BIND_vWorldSpaceLightDirection );
float3   g_vWorldSpaceCameraPosition    : register( BIND_vWorldSpaceCameraPosition );


//--------------------------------------------------------------------------------------
// Vertex shader globals
//--------------------------------------------------------------------------------------
float4x4 g_matCameraWVP                 : register( BIND_matCameraWVP );
float4x4 g_matWorld                     : register( BIND_matWorld );


//--------------------------------------------------------------------------------------
// Pixel shader globals
//--------------------------------------------------------------------------------------
float4   g_vIndirectLightingRadius      : register( BIND_vIndirectLightingRadius );
float3   g_vDiffuseLightColor           : register( BIND_vDiffuseLightColor );
float4   g_vSpecularLightColor          : register( BIND_vSpecularLightColor );
float3   g_vAmbientLightColor           : register( BIND_vAmbientLightColor );
float3   g_vWorldSpaceLightPos          : register( BIND_vWorldSpaceLightPos );
float    g_fEpsilonVSM                  : register( BIND_fEpsilonVSM );
float4   g_vWorldScale                  : register( BIND_vWorldScale );
float4   g_fSampling[ RSM_NUMSAMPLES ]  : register( BIND_fSampling );
float4   g_fHammersleySampling[ RSM_NUMSAMPLES ] : register( BIND_fHammersleySampling );


//--------------------------------------------------------------------------------------
// Pixel shader debug globals
//--------------------------------------------------------------------------------------
bool     g_bDebugShowNoLighting         : register( BIND_bDebugShowNoLighting );
bool     g_bDebugShowDirectLighting     : register( BIND_bDebugShowDirectLighting );
bool     g_bDebugShowIndirectLighting   : register( BIND_bDebugShowIndirectLighting );
bool     g_bDebugReduceFlicker          : register( BIND_bDebugReduceFlicker );


//--------------------------------------------------------------------------------------
// Sampler definitions
//--------------------------------------------------------------------------------------
sampler2D   DiffuseTexture              : register( BIND_DiffuseTexture );
sampler2D   NormalmapTexture            : register( BIND_NormalmapTexture );
sampler2D   ShadowmapTexture            : register( BIND_ShadowmapTexture );

sampler2D   RSMPositionTexture          : register( BIND_RSMPositionTexture );
sampler2D   RSMLightDirTexture          : register( BIND_RSMLightDirTexture );
sampler2D   RSMFluxTexture              : register( BIND_RSMFluxTexture );
sampler2D   HammersleyTexture           : register( BIND_HammersleyTexture );

sampler2D   RSMPositionTextureVS        : register( BIND_RSMPositionTextureVS );
sampler2D   RSMLightDirTextureVS        : register( BIND_RSMLightDirTextureVS );
sampler2D   RSMFluxTextureVS            : register( BIND_RSMFluxTextureVS );

sampler2D   RSMPositionSmallTexture     : register( BIND_RSMPositionSmallTexture );
sampler2D   RSMLightDirSmallTexture     : register( BIND_RSMLightDirSmallTexture );
sampler2D   RSMFluxSmallTexture         : register( BIND_RSMFluxSmallTexture );
sampler2D   RSMDepthTexture             : register( BIND_RSMDepthTexture );
sampler2D   RSMTexCoordTexture          : register( BIND_RSMTexCoordTexture );


//--------------------------------------------------------------------------------------
// Shader output structures
//--------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------
// Declaration for lighting calculations in pixelshader fragment
//--------------------------------------------------------------------------------------
struct LightingData
{
    float3 vDirectLighting;
    float3 vIndirectLighting;
};

//--------------------------------------------------------------------------------------
// Vertex shader declaration for rendering the reflective shadowmap (RSM)
//--------------------------------------------------------------------------------------
struct VSOUT_RSM
{
    float4 vProjectedPosition   : POSITION;
    float3 vWorldSpacePosition  : TEXCOORD0;
    float3 vWorldSpaceNormal    : TEXCOORD1;
    float2 vTexCoord            : TEXCOORD2;
};

//--------------------------------------------------------------------------------------
// Pixel shader declaration for rendering the reflective shadowmap (RSM)
//--------------------------------------------------------------------------------------
struct PSOUT_RSM
{
    float4 vWorldSpacePosition  : COLOR0;
    float4 vLightDirection      : COLOR1;
    float4 vFlux                : COLOR2;
};

//--------------------------------------------------------------------------------------
// Pixel shader declaration for rendering the small 1D textures from PSOUT_RSM
//--------------------------------------------------------------------------------------
struct PSOUT_SmallRSM
{
    PSOUT_RSM   RSM;
    float4      vTexCoord       : COLOR3;
};

//--------------------------------------------------------------------------------------
// Vertex shader declaration per vertex lighting scene rendering
//--------------------------------------------------------------------------------------
struct VSOUT_ScenePerVertex
{
     float4 vProjectedPosition  : POSITION;     // projected position
     float2 vTexCoord           : TEXCOORD0;    // texture coord for diffuse texture
     float  fDiffuseLightingI   : TEXCOORD1;    // diffuse lighting intensity
     float  fSpecularLightingI  : TEXCOORD2;    // specular lighting intensity
};

//--------------------------------------------------------------------------------------
// Vertex shader declaration for per pixel lighting scene rendering
//--------------------------------------------------------------------------------------
struct VSOUT_ScenePerPixel
{
    float4 vProjectedPosition   : POSITION;     // projected position
    float3 vWorldSpacePosition  : TEXCOORD0;    // world space position
    float3 vWorldSpaceNormal    : TEXCOORD1;    // world space normal
    float3 vTangent             : TEXCOORD2;    // tangent
    float3 vBinormal            : TEXCOORD3;    // binormal
    float2 vTexCoord            : TEXCOORD4;    // texture coordinate for diffuse texture
};

//--------------------------------------------------------------------------------------
// Vertex shader declaration rendering the scene with a variance shadowmap
//--------------------------------------------------------------------------------------
struct VSOUT_SceneShadowed
{
    VSOUT_ScenePerPixel PerPixel;               // per pixel lighting definitions
    float3 vShadowCoord         : TEXCOORD5;    // texture coord for shadowmap
};

//--------------------------------------------------------------------------------------
// Vertex shader declaration rendering the scene with per vertex indirection lighting
//--------------------------------------------------------------------------------------
struct VSOUT_SceneIndirectPerVertex
{
    VSOUT_SceneShadowed Shadowed;
    float3 vIndirectLighting    : TEXCOORD6;    // per vertex indirect lighting
};

//--------------------------------------------------------------------------------------
// Vertex shader declaration for a simple skydome
//--------------------------------------------------------------------------------------
struct VSOUT_SKY
{
     float4 vProjectedPosition  : POSITION;     // projected position
     float2 vTexCoord           : TEXCOORD0;    // texture coord for diffuse texture
};



//--------------------------------------------------------------------------------------
// Helper functions
//--------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------
// Name: Compress()
// Desc: [Scale] -> [0..1]
//--------------------------------------------------------------------------------------
float3 Compress( float3 vData, float fScale )
{
    return ( vData * fScale ) + 0.5f;
}

//--------------------------------------------------------------------------------------
// Name: Uncompress()
// Desc: [0..1] -> [Scale]
//--------------------------------------------------------------------------------------
float3 Uncompress( float3 vData, float fScale, float fOffset )
{
    return ( vData * fScale ) - fOffset;
}

//--------------------------------------------------------------------------------------
// Name: CalcWorldSpaceNormal()
// Desc: Calculates the world space normal from a normal, binormal and tangent
//--------------------------------------------------------------------------------------
float3 CalcWorldSpaceNormal( float3 vNormal,
                             float3 vTangent,
                             float3 vBinormal,
                             float2 vTexCoord )
{
    // Sample the normalmap texture and convert [0..1]->[-1..1] range
    float2 vNormalMapXY     = tex2D( NormalmapTexture, vTexCoord ).xy;
    vNormalMapXY            = vNormalMapXY * 2.0f - 1.0f;
    
    // Calculate the z component
    float3 vSampledNormal   = float3( vNormalMapXY.x, vNormalMapXY.y, saturate( 1 - dot( vNormalMapXY, vNormalMapXY ) ) );
       
    float3 output           = vTangent * vSampledNormal.x +
                              vBinormal * vSampledNormal.y +
                              vNormal * vSampledNormal.z;
                            
    return normalize( output );
}

//--------------------------------------------------------------------------------------
// Name: ComputeShadowAttenuationVSM()
// Desc: Compute the attenuation due to shadowing using the variance shadow map
//--------------------------------------------------------------------------------------
float ComputeShadowAttenuationVSM( float3 vShadowCoord )
{
    float  fOutput  = 1.0f;
    float4 vVSM     = tex2D( ShadowmapTexture, vShadowCoord.xy );
    float  fAvgZ    = vVSM.r; // Filtered z
    float  fAvgZ2   = vVSM.g; // Filtered z-squared

    [flatten]
    if ( vShadowCoord.z > fAvgZ )
    {
        // Use variance shadow mapping to compute the maximum probability that the
        // pixel is in shadow
        float fVariance = ( fAvgZ2 ) - ( fAvgZ * fAvgZ );
        fVariance       = saturate( max( 0.0f, fVariance + g_fEpsilonVSM ) );
        
        float fMean     = fAvgZ;
        float d         = vShadowCoord.z - fMean;
        float p_max     = fVariance / ( fVariance + d * d );

        // To combat light-bleeding, experiment with raising p_max to some power
        // (Try values from 0.1 to 100.0, if you like.)
        fOutput       = pow( p_max, 4.0f );
    }
    
    return fOutput;
}



//--------------------------------------------------------------------------------------
// Lighting calculation functions
//--------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------
// Name: CalcLighting()
// Desc: Calculate lighting per vertex
//--------------------------------------------------------------------------------------
LightingData CalcLighting( VSOUT_ScenePerVertex Input )
{
    LightingData Output;
    
    Output.vDirectLighting      = g_vDiffuseLightColor.xyz * Input.fDiffuseLightingI;
    Output.vDirectLighting     += g_vSpecularLightColor.xyz * Input.fSpecularLightingI;
    Output.vDirectLighting      = saturate( Output.vDirectLighting );    
    
    Output.vIndirectLighting    = g_vAmbientLightColor;
  
    return Output;
}

//--------------------------------------------------------------------------------------
// Name: CalcLighting()
// Desc: Calculate lighting per pixel
//--------------------------------------------------------------------------------------
LightingData CalcLighting( VSOUT_ScenePerPixel Input )
{
    LightingData Output;

    // Calculate the world space normal from the normalmap
    float3 vWorldSpaceNormal    = CalcWorldSpaceNormal( Input.vWorldSpaceNormal, Input.vTangent, Input.vBinormal, Input.vTexCoord );
    
    // Calculate the diffuse lighting intensity in world space
    float fDiffuseLightingI     = saturate( dot( vWorldSpaceNormal, g_vWorldSpaceLightDirection ) );
    
    // Calculate the view vector for this world space position
    float3 vViewVector          = normalize( g_vWorldSpaceCameraPosition - Input.vWorldSpacePosition );
    
    // Calculate the half vector
    float3 vHalfVector          = normalize( g_vWorldSpaceLightDirection + vViewVector );
    
    // Calculate the specular intensity
    float fSpecularLightingI    = pow( saturate( dot( vWorldSpaceNormal, vHalfVector ) ), g_vSpecularLightColor.w );

    // This gets rid of the odd artifact of specular bleeding. With very large view angles the half angle can
    // make the specular bleed onto surface which should be dark, so we check if the diffuse lighting intesity
    // is zero or not.
    [flatten]
    if ( fDiffuseLightingI == 0 )
    {
        fSpecularLightingI      = 0;
    }

    Output.vDirectLighting      = fDiffuseLightingI * g_vDiffuseLightColor;
    Output.vDirectLighting     += fSpecularLightingI * g_vSpecularLightColor;
    Output.vDirectLighting      = saturate( Output.vDirectLighting );
    
    Output.vIndirectLighting    = g_vAmbientLightColor;
 
    return Output;
}

//--------------------------------------------------------------------------------------
// Name: CalcLighting()
// Desc: Calculate lighting per pixel with shadows
//--------------------------------------------------------------------------------------
LightingData CalcLighting( VSOUT_SceneShadowed Input )
{
    LightingData Output     = CalcLighting( Input.PerPixel );
    
    float fShadow           = ComputeShadowAttenuationVSM( Input.vShadowCoord.xyz );
    
    Output.vDirectLighting *= fShadow;
  
    return Output;
}

//--------------------------------------------------------------------------------------
// Name: CalcLighting()
// Desc: Calculate lighting per pixel with shadows and per vertex indirect lighting
//--------------------------------------------------------------------------------------
LightingData CalcLighting( VSOUT_SceneIndirectPerVertex Input )
{
    LightingData Output         = CalcLighting( Input.Shadowed );
    
    Output.vIndirectLighting    = Input.vIndirectLighting;
  
    return Output;
}



//--------------------------------------------------------------------------------------
// Shadowmap and Reflective Shadowmap shaders
//--------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------
// Name: DepthOnlyVS()
// Desc: Vertex shader that outputs depth for rendering into shadowmap
//--------------------------------------------------------------------------------------
float4 DepthOnlyVS( in float4 vPosition : POSITION ) : POSITION
{
    return mul( vPosition, g_matLightWVP );
}

//--------------------------------------------------------------------------------------
// Name: ReflectiveShadowMapVS()
// Desc: Vertex shader for rendering the reflective shadowmap (RSM)
//--------------------------------------------------------------------------------------
VSOUT_RSM ReflectiveShadowMapVS( const float3 vPosition    : POSITION,
                                 const float3 vNormal      : NORMAL,
                                 const float2 vTexCoord    : TEXCOORD0 )
{
    VSOUT_RSM Output;

    // Transform vertex position
    Output.vProjectedPosition   = mul( float4( vPosition, 1.0f ), g_matLightWVP );

    // Pass vertex position through
    Output.vWorldSpacePosition  = mul( float4( vPosition, 1.0f ), g_matWorld );

    // Pass vertex normal through
    Output.vWorldSpaceNormal    = mul( float4( vNormal, 0.0f ), g_matWorld );
    
    // Pass texture coordinates through
    Output.vTexCoord            = vTexCoord;
    
    return Output;
}

//--------------------------------------------------------------------------------------
// Name: ReflectiveShadowMapPS()
// Desc: Pixel shader for rendering the reflective shadowmap (RSM)
//--------------------------------------------------------------------------------------
PSOUT_RSM ReflectiveShadowMapPS( VSOUT_RSM input )
{
    PSOUT_RSM output;

    // We just use the lowest LOD of the diffuse texture for flux color
    float LOD = 10;
    float4 vTexCoord            = float4( input.vTexCoord.xy, 0, LOD );
    float4 vFlux                = tex2Dlod( DiffuseTexture, vTexCoord );

    float3 vWorldSpacePos       = input.vWorldSpacePosition;
    float3 vWorldSpaceNormal	= normalize( input.vWorldSpaceNormal );
   
    // Scale the flux by the incident light direction
    vFlux *= saturate( dot( vWorldSpaceNormal, g_vWorldSpaceLightDirection ) );

    // Since this sample's model uses some specular, we bend the indirect light direction somewhat
    // towards the reflected outgoing light direction. This is totally dependent on the 
    // type of material. If it were a totally diffuse material, we could just use the
    // world space normal as the light direction.( vLightDirection = vWorldSpaceNormal )
    float3 vLightDirection      = reflect( -g_vWorldSpaceLightDirection, vWorldSpaceNormal );
    vLightDirection             = normalize( vWorldSpaceNormal + vLightDirection );
    
    output.vWorldSpacePosition  = float4( Compress( vWorldSpacePos, g_vWorldScale.x ), 1 );
    output.vLightDirection      = float4( Compress( vLightDirection, 0.5f ), 1 );
    output.vFlux                = vFlux;

    return output;
}

//--------------------------------------------------------------------------------------
// Name: GenerateSmallRSMTexturesPS()
// Desc: Generate small 1D textures from the RSM textures. We reduce the flickering
//       of moving lights by keeping the previous frame's RSM data if the RSM
//       position is still visible from the light's point of view, and by using a
//       lower LOD for the flux to rather get an average flux than one specific
//       point in the scene's flux. This shader looks expensive, but it will only
//       executed on a very small 1D texture, in our case 16x1 size.
//--------------------------------------------------------------------------------------
PSOUT_SmallRSM GenerateSmallRSMTexturesPS( float2 vScreenPos : VPOS ) : COLOR
{
    PSOUT_SmallRSM output;
   
    // RSM information from the current frame's importance sampling. To reduce
    // flickering, we use a higher mipmap
    float4 vNewTexCoord             = float4( g_fSampling[ vScreenPos.x ].xy, 0, 7 );
    float4 vNewWorldSpacePosition   = tex2Dlod( RSMPositionTexture, vNewTexCoord );
    float4 vNewLightDirection       = tex2Dlod( RSMLightDirTexture, vNewTexCoord );
    float4 vNewFlux                 = tex2Dlod( RSMFluxTexture, vNewTexCoord );

    // This static branch is only here for debugging purposes and a little more
    // speed can be gained by getting rid of this branch.
    if ( !g_bDebugReduceFlicker )
    {
        // Use current frame's RSM data
        output.RSM.vWorldSpacePosition  = vNewWorldSpacePosition;
        output.RSM.vLightDirection      = vNewLightDirection;
        output.RSM.vFlux                = vNewFlux;    
        output.vTexCoord                = float4( vNewTexCoord.xy, 0, 0 );
        
        return output;
    }
    
    // RSM information from the previous frame
    float2 vOldTexCoord             = float2( ( vScreenPos.x + 0.5f ) / RSM_NUMSAMPLES, 0.5f );
    float4 vOldWorldSpacePosition   = tex2D( RSMPositionSmallTexture, vOldTexCoord );
    float4 vOldLightDirection       = tex2D( RSMLightDirSmallTexture, vOldTexCoord );
    float4 vOldFlux                 = tex2D( RSMFluxSmallTexture, vOldTexCoord );
    
    // Transform the previous world position using the current light transformation matrix   
    float4 vWorldSpacePosition;
    vWorldSpacePosition.xyz         = Uncompress( vOldWorldSpacePosition.xyz, g_vWorldScale.y, g_vWorldScale.z );
    vWorldSpacePosition.w           = 1.0f;
       
    vWorldSpacePosition             = mul( vWorldSpacePosition, g_matSampleLightWVP );
    float2 vDepthTexCoord           = float2( vWorldSpacePosition.x, vWorldSpacePosition.y );
    float  fCalculatedDepth         = vWorldSpacePosition.z;
        
    // Sample the current depth and flux values
    float  fSampledDepth            = tex2Dlod( RSMDepthTexture, float4( vDepthTexCoord.xy, 0, 0 ) );
    float3 vFluxTest                = tex2Dlod( RSMFluxTexture, float4( vDepthTexCoord.xy, 0 ,0 ) ).xyz;
    
    // If the calculated old depth is smaller than the sampled new depth, then we know the
    // previous position is still visible. We also try to not use flux values that are very
    // close to black.
    
    const float fDepthBias  = 0.01f;
    const float fFluxBias   = 0.05f;
   
    [flatten]
    if ( ( fCalculatedDepth - fDepthBias <= fSampledDepth ) &&
         ( dot( vFluxTest.xyz, 1 ) > fFluxBias ) )
    {
        // Use previous frame's RSM data
        output.RSM.vWorldSpacePosition  = vOldWorldSpacePosition;
        output.RSM.vLightDirection      = vOldLightDirection;
        output.RSM.vFlux                = vOldFlux;
        output.vTexCoord                = float4( vDepthTexCoord.xy, 0, 0 );
    }
    else
    {
        // Use current frame's RSM data
        output.RSM.vWorldSpacePosition  = vNewWorldSpacePosition;
        output.RSM.vLightDirection      = vNewLightDirection;
        output.RSM.vFlux                = vNewFlux;    
        output.vTexCoord                = float4( vNewTexCoord.xy, 0, 0 );
    }
     
    return output;
}

//--------------------------------------------------------------------------------------
// Name: WarpTexCoord()
// Desc: Warp a texture coordinate using the intensity of the flux to perform
//       importance sampling on the GPU. This function is called mroe than 100 times
//       from ImportanceSamplingPS() so it's more optimal to use the [call]
//       intrinsic than to inline the instructions. Again this might sound very
//       expensive, but the shader is only applied on a 16x1 texture.
//--------------------------------------------------------------------------------------
[call]
float2 WarpTexCoord( float2 vTexCoord, float2 vOffset, float2 vSize, float fBias )
{
    // Since this function is called 341 times per pixel, a small optimization can go a far
    // way. The following code was optimized to use the sge asm instruction to calculate the
    // four greater than's in parallel rather than as four scalars, and a simple dot() is
    // used in the if statement to know if all four conditions passed or not.
    // if ( vTexCoord.x >= vOffset.x && vTexCoord.x <= vOffset.x + vSize.x &&
    //      vTexCoord.y >= vOffset.y && vTexCoord.y <= vOffset.y + vSize.y )
    
    float4 vTestResult;
    float4 vTestSource0 = float4( vTexCoord.xy, vOffset.xy + vSize.xy );
    float4 vTestSource1 = float4( vOffset.xy, vTexCoord.xy );
    
    asm
    {
        sge vTestResult, vTestSource0, vTestSource1
    };
    
    [branch]
    if ( dot( vTestResult, vTestResult ) == 4 )
    {
        vTexCoord.xy = ( vTexCoord.xy - vOffset ) / vSize.y;

        float3 A, B, C, D;
        float  fFlux1, fFlux2, fFlux3, fFlux4, fRatio, fA, fB, fC, fD;

        // get the 4 average values of the quadrants    
        float2 vTexCoordA = float2( vOffset + vSize * float2( 1, 1 ) * 0.25f );
        float2 vTexCoordB = float2( vOffset + vSize * float2( 3, 1 ) * 0.25f );
        float2 vTexCoordC = float2( vOffset + vSize * float2( 1, 3 ) * 0.25f );
        float2 vTexCoordD = float2( vOffset + vSize * float2( 3, 3 ) * 0.25f );
	    
        float fLOD = fBias;
        asm
        {
            setTexLOD fLOD.x
            tfetch2D A.xyz_, vTexCoordA, RSMFluxTexture, UseComputedLOD=false, UseRegisterLOD=true
            tfetch2D B.xyz_, vTexCoordB, RSMFluxTexture, UseComputedLOD=false, UseRegisterLOD=true
            tfetch2D C.xyz_, vTexCoordC, RSMFluxTexture, UseComputedLOD=false, UseRegisterLOD=true
            tfetch2D D.xyz_, vTexCoordD, RSMFluxTexture, UseComputedLOD=false, UseRegisterLOD=true
        };
    	
        // and take average brightness
        const float3 vLuminance = float3( 0.2125f, 0.7154f, 0.0721f );
        fA = dot( A, vLuminance );
        fB = dot( B, vLuminance );
        fC = dot( C, vLuminance );
        fD = dot( D, vLuminance );

        // adjust sample positions
        fRatio = ( fA + fC ) / ( fA + fB + fC + fD );
    	
        [flatten]
        if ( vTexCoord.x < fRatio )
        {
            vTexCoord.x = vTexCoord.x / fRatio * 0.5f;

            fRatio = fA / ( fA + fC );

            [flatten]
            if ( vTexCoord.y < fRatio )
            {
                vTexCoord.y = vTexCoord.y / fRatio * 0.5f;
            }
            else
            {
                vTexCoord.y = 0.5f + ( vTexCoord.y - fRatio ) / ( 1.0f - fRatio ) * 0.5f;
            }   		
        }
        else
        {
            vTexCoord.x = 0.5f + ( vTexCoord.x - fRatio ) / ( 1.0f - fRatio ) * 0.5f;
        	
            fRatio = fB / ( fB + fD );
        	
            [flatten]
            if ( vTexCoord.y < fRatio )
            {
                vTexCoord.y = vTexCoord.y / fRatio * 0.5f;
            }
            else
            {
                vTexCoord.y = 0.5f + ( vTexCoord.y - fRatio ) / ( 1.0f - fRatio ) * 0.5f;
            }
        }

        vTexCoord.xy = vTexCoord.xy * vSize.x + vOffset.xy;
    }
    
    return vTexCoord;
}

//--------------------------------------------------------------------------------------
// Name: ImportanceSamplingPS()
// Desc: Use a texture with Hammersley distribution points and do importance sampling
//       on it using the intensity of the flux texture to warp the sample points.
//       See "Splatting of Diffuse and Glossy Indirect Illumination" in ShaderX5
//--------------------------------------------------------------------------------------
float4 ImportanceSamplingPS( float2 vTexCoord : TEXCOORD0 ) : COLOR
{
    // Get the Hammersley distrubution point that needs to be warped
    vTexCoord = tex2D( HammersleyTexture, vTexCoord ).xy;
       
    // 512 resolution of the RSM => 4 averaged pixels correspond to a lod-bias of 8
    vTexCoord = WarpTexCoord( vTexCoord, float2( 0.0, 0.0 ), float2( 1.0, 1.0 ), 8 );

    vTexCoord = WarpTexCoord( vTexCoord, float2( 0.0, 0.0 ), float2( 0.5, 0.5 ), 7 );
    vTexCoord = WarpTexCoord( vTexCoord, float2( 0.5, 0.0 ), float2( 0.5, 0.5 ), 7 );
    vTexCoord = WarpTexCoord( vTexCoord, float2( 0.0, 0.5 ), float2( 0.5, 0.5 ), 7 );
    vTexCoord = WarpTexCoord( vTexCoord, float2( 0.5, 0.5 ), float2( 0.5, 0.5 ), 7 );

    for ( int j = 0; j < 4; j++ )
    {
        for ( int i = 0; i < 4; i++ )
        {
            vTexCoord = WarpTexCoord( vTexCoord, float2( 0.25 * i, 0.25 * j ), float2( 0.25, 0.25 ), 6 );
        }
    }

    for ( int j = 0; j < 8; j++ )
    {
        for ( int i = 0; i < 8; i++ )
        {
            vTexCoord = WarpTexCoord( vTexCoord, float2( 0.125 * i, 0.125 * j ), float2( 0.125, 0.125 ), 5 );
        }
    }

    // This last iteration loop is quite expensive, so if you don't need it, don't do it. For this sample
    // we need it though
    for ( int j = 0; j < 16; j++ )
    {
        for ( int i = 0; i < 16; i++ )
        {
            vTexCoord = WarpTexCoord( vTexCoord, float2( 0.0625 * i, 0.0625 * j ), float2( 0.0625, 0.0625 ), 4 );
        }
    }

    return float4( vTexCoord, 0, 0 );
}



//--------------------------------------------------------------------------------------
// Indirect lighting shader
//--------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------
// Name: CalcIndirectLighting()
// Desc: Calculates one bounce of indirect lighting using reflective shadowmap inputs
//--------------------------------------------------------------------------------------
float3 CalcIndirectLighting( float3     vWorldSpacePosition,
                             float3     vWorldSpaceNormal,
                             sampler2D  RSMPositionTexture,
                             sampler2D  RSMLightDirTexture,
                             sampler2D  RSMFluxTexture )
{      
    float3 vTotalIrradiance         = 0;
    float  fTotalSelfOcclusion      = 0;      
    float  fDistanceFalloffScale    = 1;
    
    // For each virtual point light we calculate the radiant intensity, irradiance and self-occlusion
    for ( int i = 0; i < RSM_NUMSAMPLES; ++i )
    {          
        // Sample the small 1D textures to get the RSM information
        float4 vTexCoord              = float4( g_fSampling[ i ].xy, 0, 0 );       
        float3 vVirtualLightPosition  = tex2Dlod( RSMPositionTexture, vTexCoord ).xyz;
        float3 vVirtualLightDirection = tex2Dlod( RSMLightDirTexture, vTexCoord ).xyz;
        float3 vVirtualLightFlux      = tex2Dlod( RSMFluxTexture, vTexCoord ).xyz;
        
        vVirtualLightPosition       = Uncompress( vVirtualLightPosition,  g_vWorldScale.y, g_vWorldScale.z );
        vVirtualLightDirection      = Uncompress( vVirtualLightDirection, 2.0f, 1.0f );
        
        // Indirect lighting is low frequency anyway, so rather take a 0.3ms speed gain from not
        // having to renormalize vVirtualLightDirection in this loop
        // vVirtualLightDirection   = normalize( vVirtualLightDirection );
      
        // Vector from world space position to virtual light position
        float3 R = normalize( vVirtualLightPosition - vWorldSpacePosition.xyz );

        // g_vIndirectLightingRadius.x - scale the result of the distance falloff
        // g_vIndirectLightingRadius.y - clamp the result of distance square
        // g_vIndirectLightingRadius.z - scale the result of the irradiance
        // g_vIndirectLightingRadius.w - scale the result of the self-occlusion

        // We could do an optimization here with dot() to directly calculate the distance square,
        // but the compiler does a really good job of optimizing the result of the distance()
        // function into the above normalize(), so that it's just easier to read the code this way.
        
        // Calculate some distance attenuation
        float fDistanceSq           = distance( vVirtualLightPosition, vWorldSpacePosition.xyz );
        fDistanceSq                 = fDistanceSq * fDistanceSq;
        fDistanceSq                 = min( fDistanceSq, g_vIndirectLightingRadius.y );
        fDistanceSq                 = fDistanceSq / g_vIndirectLightingRadius.y;
        fDistanceFalloffScale       = g_vIndirectLightingRadius.x * ( 1.0f - fDistanceSq );
        
        // Calculate radiant intensity and irradiance
        float3 vRadiantIntensity    = vVirtualLightFlux * saturate( dot( vVirtualLightDirection, -R ) );
        float3 vIrradiance          = vRadiantIntensity * saturate( dot( vWorldSpaceNormal, R ) ) * fDistanceFalloffScale;

        // Calculate self occlusion from indirect light
        float fSelfOcclusion        = saturate( -dot( vWorldSpaceNormal, R ) );
        
        // Add this result to the totals        
        vTotalIrradiance           += vIrradiance;
        fTotalSelfOcclusion        += fSelfOcclusion;
    }
    
    // Average the totals
    vTotalIrradiance               /= RSM_NUMSAMPLES;
    fTotalSelfOcclusion            /= RSM_NUMSAMPLES;
    
    // Scale results by user constants
    vTotalIrradiance               *= g_vIndirectLightingRadius.z;  
    fTotalSelfOcclusion            *= g_vIndirectLightingRadius.w;

    // Output the final indirect lighting
    float3 vIndirectLighting        = g_vAmbientLightColor + vTotalIrradiance - fTotalSelfOcclusion;
       
    return vIndirectLighting;      
}


//--------------------------------------------------------------------------------------
// Name: CalcIndirectLightingSelfOcclude()
// Desc: Calculates self-occlusion from indirect lighting using reflective shadowmaps
//--------------------------------------------------------------------------------------
float3 CalcIndirectLightingSelfOcclude( float3     vWorldSpacePosition,
                                        float3     vWorldSpaceNormal,
                                        sampler2D  RSMPositionTexture,
                                        sampler2D  RSMLightDirTexture  )
{
    float fTotalSelfOcclusion = 0;      
    
    // For each virtual point light we calculate the self-occlusion
    for ( int i = 0; i < RSM_NUMSAMPLES; ++i )
    {
        // Sample the small 1D textures to get the RSM information
        float4 vTexCoord              = float4( g_fSampling[ i ].xy, 0, 0 );
        float3 vVirtualLightPosition  = tex2Dlod( RSMPositionTexture, vTexCoord ).xyz;
        float3 vVirtualLightDirection = tex2Dlod( RSMLightDirTexture, vTexCoord ).xyz;
        
        vVirtualLightPosition   = Uncompress( vVirtualLightPosition,  g_vWorldScale.y, g_vWorldScale.z );
        vVirtualLightDirection  = Uncompress( vVirtualLightDirection, 2.0f, 1.0f );
             
        // Vector from world space position to virtual light position
        float3 R = normalize( vVirtualLightPosition - vWorldSpacePosition.xyz );

        // Calculate self occlusion from indirect light
        float fSelfOcclusion    = saturate( -dot( vWorldSpaceNormal, R ) );

        // Add this result to the total
        fTotalSelfOcclusion    += fSelfOcclusion;
    }
    
    // Averages the total and scale by user constant
    fTotalSelfOcclusion        /= RSM_NUMSAMPLES;
    fTotalSelfOcclusion        *= g_vIndirectLightingRadius.w;

    // Output the final indirect lighting
    float3 vIndirectLighting    = g_vAmbientLightColor - fTotalSelfOcclusion;
       
    return vIndirectLighting;
   
}


//--------------------------------------------------------------------------------------
// Scene shaders
//--------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------
// Name: ShadeScenePerVertexVS()
// Desc: Render scene using per vertex lighting
//--------------------------------------------------------------------------------------
[optimizeAutoZ(true)]
VSOUT_ScenePerVertex ShadeScenePerVertexVS( const float3 vPosition   : POSITION,
                                            const float3 vNormal     : NORMAL,
                                            const float2 vTexCoord   : TEXCOORD0 )
{
    VSOUT_ScenePerVertex Output;
   
    // Transform the position    
    Output.vProjectedPosition   = mul( float4( vPosition, 1 ), g_matCameraWVP );

    // Copy the diffuse texture coordinate
    Output.vTexCoord            = vTexCoord;
         
    // Transform the normal to world space
    float3 vWorldSpaceNormal    = mul( float4( vNormal, 0 ), g_matWorld );
    vWorldSpaceNormal           = normalize( vWorldSpaceNormal );
       
    // Calculate the diffuse lighting intensity in world space
    Output.fDiffuseLightingI    = saturate( dot( vWorldSpaceNormal, g_vWorldSpaceLightDirection ) );

    // Transform the position to world space
    float3 vWorldSpacePosition  = mul( float4( vPosition, 1 ), g_matWorld );
    
    // Calculate the view vector for this vertex
    float3 vViewVector          = normalize( g_vWorldSpaceCameraPosition - vWorldSpacePosition );
    
    // Calculate the half vector
    float3 vHalfVector          = normalize( g_vWorldSpaceLightDirection + vViewVector );
    
    // Calculate the specular intensity
    Output.fSpecularLightingI   = pow( saturate( dot( vWorldSpaceNormal, vHalfVector ) ), 32.0f );

    // This gets rid of the odd artifact of specular bleeding. With very large view angles the half angle can
    // make the specular bleed onto surface which should be dark, so we check if the diffuse lighting intesity
    // is zero or not.
    [flatten]
    if ( Output.fDiffuseLightingI == 0 )
    {
        Output.fSpecularLightingI = 0;
    }
        
    return Output;
}

//--------------------------------------------------------------------------------------
// Name: ShadeScenePerVertexPS()
// Desc: Render scene using per vertex lighting
//--------------------------------------------------------------------------------------
float4 ShadeScenePerVertexPS( VSOUT_ScenePerVertex Input ) : COLOR
{
    // Fetch the diffuse color
    float4 vOutput          = tex2D( DiffuseTexture, Input.vTexCoord.xy );

    // Add lighting
    LightingData Lighting   = CalcLighting( Input );
    vOutput.xyz            *= Lighting.vDirectLighting + Lighting.vIndirectLighting;

    return vOutput;
}


//--------------------------------------------------------------------------------------
// Name: ShadeScenePerPixelVS()
// Desc: Render scene using per pixel lighting
//--------------------------------------------------------------------------------------
[optimizeAutoZ(true)]
VSOUT_ScenePerPixel ShadeScenePerPixelVS( const float3 vPosition   : POSITION,
                                          const float3 vNormal     : NORMAL,
                                          const float3 vBinormal   : BINORMAL,
                                          const float3 vTangent    : TANGENT,
                                          const float2 vTexCoord   : TEXCOORD0 )
{
    VSOUT_ScenePerPixel Output;
   
    // Transform the position    
    Output.vProjectedPosition   = mul( float4( vPosition, 1 ), g_matCameraWVP );
         
    // Transform the position to world space
    Output.vWorldSpacePosition  = mul( float4( vPosition, 1 ), g_matWorld );
         
    // Transform the normal to world space
    Output.vWorldSpaceNormal    = mul( float4( vNormal, 0 ), g_matWorld );

    // Copy binormal and tangent
    Output.vTangent             = vTangent;
    Output.vBinormal            = vBinormal;

    // Copy the diffuse texture coordinate
    Output.vTexCoord            = vTexCoord;   
    
    return Output;
}

//--------------------------------------------------------------------------------------
// Name: ShadeScenePerPixelPS()
// Desc: Render scene using per pixel lighting
//--------------------------------------------------------------------------------------
float4 ShadeScenePerPixelPS( VSOUT_ScenePerPixel Input ) : COLOR
{
    // Fetch the diffuse color
    float4 vOutput          = tex2D( DiffuseTexture, Input.vTexCoord );

    // Add lighting
    LightingData Lighting   = CalcLighting( Input );
    vOutput.xyz            *= Lighting.vDirectLighting + Lighting.vIndirectLighting;

    return vOutput;
}


//--------------------------------------------------------------------------------------
// Name: ShadeSceneShadowedVS()
// Desc: Render scene using per pixel lighting with a variance shadowmap
//--------------------------------------------------------------------------------------
[optimizeAutoZ(true)]
VSOUT_SceneShadowed ShadeSceneShadowedVS( const float3 vPosition   : POSITION,
                                          const float3 vNormal     : NORMAL,
                                          const float3 vBinormal   : BINORMAL,
                                          const float3 vTangent    : TANGENT,
                                          const float2 vTexCoord   : TEXCOORD0 )
{                   
    VSOUT_SceneShadowed Output;
 
    Output.PerPixel = ShadeScenePerPixelVS( vPosition, vNormal, vBinormal, vTangent, vTexCoord );
    
    Output.vShadowCoord = mul( float4( vPosition, 1 ), g_matSampleLightWVP );
          
    return Output;
}


//--------------------------------------------------------------------------------------
// Name: ShadeSceneShadowedPS()
// Desc: Render scene using per pixel lighting with a variance shadowmap
//--------------------------------------------------------------------------------------
float4 ShadeSceneShadowedPS( VSOUT_SceneShadowed Input ) : COLOR
{
    // Fetch the diffuse color
    float4 vOutput          = tex2D( DiffuseTexture, Input.PerPixel.vTexCoord );

    // Add lighting
    LightingData Lighting   = CalcLighting( Input );
    vOutput.xyz            *= Lighting.vDirectLighting + Lighting.vIndirectLighting;

    return vOutput;
}


//--------------------------------------------------------------------------------------
// Name: ShadeSceneIndirectPerVtxSelfOccludeVS()
// Desc: Render scene using per pixel lighting with a variance shadowmap and
//       per vertex indirect lighting with only indirect self-occlusion
//--------------------------------------------------------------------------------------
[optimizeAutoZ(true)]
VSOUT_SceneIndirectPerVertex ShadeSceneIndirectPerVtxSelfOccludeVS(
                                             const float3 vPosition   : POSITION,
                                             const float3 vNormal     : NORMAL,
                                             const float3 vBinormal   : BINORMAL,
                                             const float3 vTangent    : TANGENT,
                                             const float2 vTexCoord   : TEXCOORD0 )
{
    VSOUT_SceneIndirectPerVertex Output;
   
    Output.Shadowed          = ShadeSceneShadowedVS( vPosition, vNormal, vBinormal, vTangent, vTexCoord );
                                                       
    Output.vIndirectLighting = CalcIndirectLightingSelfOcclude(
                                                     Output.Shadowed.PerPixel.vWorldSpacePosition,
                                                     Output.Shadowed.PerPixel.vWorldSpaceNormal,
                                                     RSMPositionTextureVS, RSMLightDirTextureVS );  
    return Output;
}


//--------------------------------------------------------------------------------------
// Name: ShadeSceneIndirectPerVertexVS()
// Desc: Render scene using per pixel lighting with a variance shadowmap and
//       per vertex indirect lighting
//--------------------------------------------------------------------------------------
[optimizeAutoZ(true)]
VSOUT_SceneIndirectPerVertex ShadeSceneIndirectPerVertexVS( const float3 vPosition   : POSITION,
                                                            const float3 vNormal     : NORMAL,
                                                            const float3 vBinormal   : BINORMAL,
                                                            const float3 vTangent    : TANGENT,
                                                            const float2 vTexCoord   : TEXCOORD0 )
{
    VSOUT_SceneIndirectPerVertex Output;
   
    Output.Shadowed          = ShadeSceneShadowedVS( vPosition, vNormal, vBinormal, vTangent, vTexCoord );
                                                        
    Output.vIndirectLighting = CalcIndirectLighting( Output.Shadowed.PerPixel.vWorldSpacePosition,
                                                     Output.Shadowed.PerPixel.vWorldSpaceNormal,
                                                     RSMPositionTextureVS,
                                                     RSMLightDirTextureVS,
                                                     RSMFluxTextureVS );
    return Output;
}

//--------------------------------------------------------------------------------------
// Name: ShadeSceneIndirectPerVertexPS()
// Desc: Render scene using per pixel lighting with a variance shadowmap and
//       per vertex indirect lighting
//--------------------------------------------------------------------------------------
float4 ShadeSceneIndirectPerVertexPS( VSOUT_SceneIndirectPerVertex Input ) : COLOR
{
    // Fetch the diffuse color
    float4 vOutput          = tex2D( DiffuseTexture, Input.Shadowed.PerPixel.vTexCoord );

    // Add lighting
    LightingData Lighting   = CalcLighting( Input );     
    vOutput.xyz            *= Lighting.vDirectLighting + Lighting.vIndirectLighting;

    return vOutput;
}


//--------------------------------------------------------------------------------------
// Name: ShadeSceneIndirectPerPixelPS()
// Desc: Render scene using per pixel lighting with a variance shadowmap and
//       per pixel indirect lighting
//--------------------------------------------------------------------------------------
float4 ShadeSceneIndirectPerPixelPS( VSOUT_SceneShadowed Input ) : COLOR
{
    // Fetch the diffuse color
    float4 vOutput              = tex2D( DiffuseTexture, Input.PerPixel.vTexCoord );

    // Add lighting
    LightingData Lighting       = CalcLighting( Input );
    
    float3 vWorldSpaceNormal    = CalcWorldSpaceNormal( Input.PerPixel.vWorldSpaceNormal,
                                                        Input.PerPixel.vTangent,
                                                        Input.PerPixel.vBinormal,
                                                        Input.PerPixel.vTexCoord );
    
    Lighting.vIndirectLighting  = CalcIndirectLighting( Input.PerPixel.vWorldSpacePosition,
                                                        vWorldSpaceNormal, RSMPositionTexture,
                                                        RSMLightDirTexture, RSMFluxTexture );
    
    vOutput.xyz                *= Lighting.vDirectLighting + Lighting.vIndirectLighting;

    return vOutput;
}



//--------------------------------------------------------------------------------------
// Debug shaders
//--------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------
// Name: DebugPS()
// Desc: Debug pixel shader to show different lighting calculations
//--------------------------------------------------------------------------------------
float4 DebugPS( LightingData Lighting, float2 vTexCoord )
{  
    float4 vOutput          = float4( 0, 0, 0, 1 );

    if ( g_bDebugShowNoLighting )
    {
        // Fetch the diffuse color
        vOutput             = tex2D( DiffuseTexture, vTexCoord );
    }
    else
    {
        if ( g_bDebugShowDirectLighting )
        {
            vOutput.xyz    += Lighting.vDirectLighting;           
        }    
        
        if ( g_bDebugShowIndirectLighting )
        {
            vOutput.xyz    += Lighting.vIndirectLighting;
        }
    }
    
    return vOutput;
}

//--------------------------------------------------------------------------------------
// Name: ShadeScenePerVertexDebugPS()
// Desc: Render scene using per vertex lighting
//--------------------------------------------------------------------------------------
float4 ShadeScenePerVertexDebugPS( VSOUT_ScenePerVertex Input ) : COLOR
{
    LightingData Lighting = CalcLighting( Input );

    return DebugPS( Lighting, Input.vTexCoord );
}

//--------------------------------------------------------------------------------------
// Name: ShadeScenePerPixelDebugPS()
// Desc: Render scene using per pixel lighting
//--------------------------------------------------------------------------------------
float4 ShadeScenePerPixelDebugPS( VSOUT_ScenePerPixel Input ) : COLOR
{
    LightingData Lighting = CalcLighting( Input );
   
    return DebugPS( Lighting, Input.vTexCoord );
}

//--------------------------------------------------------------------------------------
// Name: ShadeSceneShadowedDebugPS()
// Desc: Render scene using per pixel lighting with a variance shadowmap
//--------------------------------------------------------------------------------------
float4 ShadeSceneShadowedDebugPS( VSOUT_SceneShadowed Input ) : COLOR
{
    LightingData Lighting = CalcLighting( Input );

    return DebugPS( Lighting, Input.PerPixel.vTexCoord );   
}

//--------------------------------------------------------------------------------------
// Name: ShadeSceneIndirectPerVertexDebugPS()
// Desc: Render scene using per pixel lighting with a variance shadowmap and
//       per vertex indirect lighting
//--------------------------------------------------------------------------------------
float4 ShadeSceneIndirectPerVertexDebugPS( VSOUT_SceneIndirectPerVertex Input ) : COLOR
{
    LightingData Lighting = CalcLighting( Input );

    return DebugPS( Lighting, Input.Shadowed.PerPixel.vTexCoord );  
}

//--------------------------------------------------------------------------------------
// Name: ShadeSceneIndirectPerPixelDebugPS()
// Desc: Render scene using per pixel lighting with a variance shadowmap and
//       per pixel indirect lighting
//--------------------------------------------------------------------------------------
float4 ShadeSceneIndirectPerPixelDebugPS( VSOUT_SceneShadowed Input ) : COLOR
{
    LightingData Lighting       = CalcLighting( Input );

    float3 vWorldSpaceNormal    = CalcWorldSpaceNormal( Input.PerPixel.vWorldSpaceNormal,
                                                        Input.PerPixel.vTangent,
                                                        Input.PerPixel.vBinormal,
                                                        Input.PerPixel.vTexCoord );
                                                        
    Lighting.vIndirectLighting  = CalcIndirectLighting( Input.PerPixel.vWorldSpacePosition,
                                                        vWorldSpaceNormal, RSMPositionTexture,
                                                        RSMLightDirTexture, RSMFluxTexture );
   
    return DebugPS( Lighting, Input.PerPixel.vTexCoord );
}

//--------------------------------------------------------------------------------------
// Name: ReflectiveShadowmapDebugPS()
// Desc: Renders the sampling points of the RSM.
//--------------------------------------------------------------------------------------
float4 ReflectiveShadowMapDebugPS( float2 vScreenPos : VPOS ) : COLOR
{
    float4 output = 0;
    
    // Render the uniformaly distributed Hammersley points in blue
    for ( int i = 0; i < RSM_NUMSAMPLES; ++i )
    {   
        float2 vSamplingCoord   = g_fHammersleySampling[ i ].xy * float2( 1280, 720 );
        float  fDistance        = distance( vSamplingCoord.xy, vScreenPos.xy );
        
        if ( fDistance < 3.0f )
        {
            output += float4( 0, 0, 1, 0 );
        }       
    }
       
    // Render the warped importance sampled points in red
    for ( int i = 0; i < RSM_NUMSAMPLES; ++i )
    {   
        float2 vSamplingCoord   = g_fSampling[ i ].xy * float2( 1280, 720 );
        float  fDistance        = distance( vSamplingCoord.xy, vScreenPos.xy );
        
        if ( fDistance < 3.0f )
        {
            output += float4( 1, 0, 0, 0 );
        }       
    }
    
    // Render the currently used sample points in green
    for ( int i = 0; i < RSM_NUMSAMPLES; ++i )
    { 
        float2  vTexCoord       = float2( float( i + 0.5 ) / RSM_NUMSAMPLES, 0.5f );
        float2  vSamplingCoord  = tex2D( RSMTexCoordTexture, vTexCoord ).xy * float2( 1280, 720 );
        float   fDistance       = distance( vSamplingCoord.xy, vScreenPos.xy );
        
        if ( fDistance < 3.0f )
        {
            output += float4( 0, 1, 0, 0 );
        }       
    }
   
    return output;
}


//--------------------------------------------------------------------------------------
// Skydome shaders
//--------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------
// Name: ShadeSkyVS()
// Desc: Vertex shader to perform scene rendering : per vertex lighting
//--------------------------------------------------------------------------------------
VSOUT_SKY ShadeSkyVS( const float3 vPosition   : POSITION,
                      const float2 vTexCoord   : TEXCOORD0 )
{
    VSOUT_SKY Output;
   
    // Transform the position    
    Output.vProjectedPosition   = mul( float4( vPosition, 1 ), g_matCameraWVP );

    // Copy the diffuse texture coordinate
    Output.vTexCoord            = vTexCoord;
       
    return Output;
}

//--------------------------------------------------------------------------------------
// Name: ShadeSkyPS()
// Desc: Pixel shader for simple skydome
//--------------------------------------------------------------------------------------
float4 ShadeSkyPS( VSOUT_SKY Input ) : COLOR
{
    return float4( tex2D( DiffuseTexture, Input.vTexCoord.xy ).xyz, 1 );
}
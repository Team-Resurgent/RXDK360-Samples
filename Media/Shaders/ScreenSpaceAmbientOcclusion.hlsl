//--------------------------------------------------------------------------------------
// ScreenSpaceAmbientOcclusion.hlsl
//
// Demonstrates how the depth buffer can be used to calculate ambient occlusion as a
// post effect with no prepasses for normals or positions.
//
// XNA Developer Connection.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#define NUM_OCCLUDERS   8

//--------------------------------------------------------------------------------------
// Vertex shader globals
//--------------------------------------------------------------------------------------
float4x4    g_matWorldViewProj      : register( c0 );   // World * View * Projection
float4x4    g_matWorld              : register( c4 );   // World transform

//--------------------------------------------------------------------------------------
// Pixel  shader globals
//--------------------------------------------------------------------------------------
float4x4    g_matProj               : register( c0 );   // Projection matrix
float4x4    g_matInvProj            : register( c4 );   // Inverse projection matrix
float4      g_vTuneSSAO             : register( c8 );   // Tuning constants for SSAO
float4      g_vSpherePositions[NUM_OCCLUDERS] : register( c10 );  // Positions on a unit sphere

sampler2D   Sampler0                : register( s0 );
sampler2D   Sampler1                : register( s1 );
sampler2D   Sampler2                : register( s2 );


//--------------------------------------------------------------------------------------
// Name: GetClipSpaceFromScreenSpace()
// Desc: Given the screen space position, return the clip space position
//--------------------------------------------------------------------------------------
float4 GetClipSpaceFromScreenSpace( in float2 vScreenPosition : TEXCOORD0 ) : COLOR0
{
    // Get clip space position
    float4 vClipSpacePosition;
    vClipSpacePosition.x = vScreenPosition.x * 2 - 1;
    vClipSpacePosition.y = ( 1 - vScreenPosition.y ) * 2 - 1;
    vClipSpacePosition.z = tex2D( Sampler1, vScreenPosition ).r;  
    vClipSpacePosition.w = 1.0f;

    return vClipSpacePosition;
}


//--------------------------------------------------------------------------------------
// Name: BackProjectDepthValueToCameraSpacePosition()
// Desc: Back project a screen space depth value to a camera space position using the
//       inverse projection matrix
//--------------------------------------------------------------------------------------
float4 BackProjectDepthValueToCameraSpacePosition( in float2 vScreenPosition : TEXCOORD0 ) : COLOR0
{
    // Get clip space position
    float4 vClipSpacePosition = GetClipSpaceFromScreenSpace( vScreenPosition );

    // Back project the clip space position to camera space
    float4 vCameraSpacePosition = mul( vClipSpacePosition, g_matInvProj );
    vCameraSpacePosition /= vCameraSpacePosition.w;

    return float4( vCameraSpacePosition.x, vCameraSpacePosition.y, vCameraSpacePosition.z, 1.0f );
}


//--------------------------------------------------------------------------------------
// Name: BackProjectDepthToCameraSpacePS()
// Desc: Back project a screen space depth value to a camera space depth value using the
//       inverse projection matrix
//--------------------------------------------------------------------------------------
float4 BackProjectDepthToCameraSpacePS( in float2 vScreenPosition : TEXCOORD0 ) : COLOR0
{
    float4 vCameraSpacePosition = BackProjectDepthValueToCameraSpacePosition( vScreenPosition );
     
    return float4( vCameraSpacePosition.z, vCameraSpacePosition.z, vCameraSpacePosition.z, 1.0f );
}


//--------------------------------------------------------------------------------------
// Name: ScreenSpaceAmbientOcclusionPS()
// Desc: Calculates ambient occlusion as a post efffect using depth difference
//--------------------------------------------------------------------------------------
float4 ScreenSpaceAmbientOcclusionPS( in float2 vScreenPosition : TEXCOORD0 ) : COLOR0
{  
    // Get clip space position
    float4 vClipSpacePosition = GetClipSpaceFromScreenSpace( vScreenPosition );

    float fOcclusion          = 1.0f;
    
    // Jump over the expensive calculations if z is out of range
    [branch]
    if ( vClipSpacePosition.z < 0.99f )
    {
        // Back project the clip space position to camera space
        float4 vCameraSpacePosition = BackProjectDepthValueToCameraSpacePosition( vScreenPosition );

        fOcclusion              = 0.0f;
        float fOccluderRadius   = g_vTuneSSAO.x;
        float fFalloffRadius    = g_vTuneSSAO.y;

        // Using unroll is faster in this case than using a loop
        [unroll]
        for ( int i = 0; i < NUM_OCCLUDERS; i++ )
        {
            // Calculate occluder points on a sphere around the camera space position
            float4 vOccluderCameraSpacePosition = float4( vCameraSpacePosition.xyz, 1 ) + float4( g_vSpherePositions[i].xyz * fOccluderRadius, 0 );

            // Project the occluder position to clip space
            float4 vOccluderClipSpacePosition = mul( vOccluderCameraSpacePosition, g_matProj );
            vOccluderClipSpacePosition /= vOccluderClipSpacePosition.w;

            // Convert to texture space		
            float2 vOccluderTextureSpacePosition = vOccluderClipSpacePosition.xy * 0.5f + 0.5f;
            vOccluderTextureSpacePosition.y = 1 - vOccluderTextureSpacePosition.y;

            // Sample the linear depth at this texture space position
            float fSampledCameraSpaceDepth = tex2D( Sampler2, vOccluderTextureSpacePosition.xy ).r;
            
            // Now we have both depths in linear camera space, so we can calculate the depth difference
            float fDepthDifference = vOccluderCameraSpacePosition.z - fSampledCameraSpaceDepth;
        	
            // Calculate some kind of scale using the depthDifference and a tunable falloff radius
            float fScale = fDepthDifference / fFalloffRadius;
        	
            // Any depth differance resulting in a scale larger than 1, should be ignored, otherwise nasty outlines
            // will be visible, almost like a halo effect

            // NOTE: The following nested "if" can be optimized in two ways:
            // 1) Use [flatten]
            //      Using [flatten] 0.36ms was gained. This is due to the fact that [flatten] will change
            //      the branch to an arithmetic cnd, and arithmetic is almost always faster than a branch
            // 2) Use step()
            //      step() is another arithmetic solution which seems to be a little bit faster than using
            //      [flatten]. About 0.1ms was gained using step()
                                  
            // Disabling a warning that says it cannot flatten the first "if", but without it, the shader is slower,
            // we keep the flatten keyword but disable the warning
            /*#pragma warning( disable : 3588 )
            [flatten]
            if ( fScale < 1 )
            {	
                [flatten]
   	            if ( fScale < 0 )
                {
                    fScale *= 1.5f;
                }
                
	            fOcclusion += fScale;
            }*/
            
            // These step() instructions result in the same output as the above "if"s. See the optimization note above.
            fScale *= step( fScale, 0.0 ) * 0.5 + 1.0;
            fScale *= step( fScale, 1.0 );
            fOcclusion += fScale;
        }

        // Calculate the average of the occlusion samples
        fOcclusion /= NUM_OCCLUDERS;
        fOcclusion = saturate( fOcclusion );

        // Invert the result, so that occlusion darkens
        fOcclusion = 1 - fOcclusion;
    }

    return float4( fOcclusion, fOcclusion, fOcclusion, 1 );
}


//--------------------------------------------------------------------------------------
// Name: CombineLocalAndGlobalOcclusionPS()
// Desc: Combine local and global occlusion results as the final output of the
//       Screen Space Ambient Occlusion effect
//--------------------------------------------------------------------------------------
float4 CombineLocalAndGlobalOcclusionPS( in float2 vScreenPosition : TEXCOORD0 ) : COLOR
{
    float fLocalOcclusion   = tex2D( Sampler0, vScreenPosition ).r;
    float fGlobalOcclusion  = tex2D( Sampler1, vScreenPosition ).r;
    
    // Sqaure the result to darken the occlusion
    fLocalOcclusion *= fLocalOcclusion;
    fGlobalOcclusion *= fGlobalOcclusion;

    float fOcclusion = ( fLocalOcclusion + fGlobalOcclusion ) / 2.0f;
   
    return float4( fOcclusion, fOcclusion, fOcclusion, 1.0f );
}


//--------------------------------------------------------------------------------------
// Name: DontCombineLocalAndGlobalOcclusionPS()
// Desc: Show only local or global occlusion as the final output of the
//       Screen Space Ambient Occlusion effect
//--------------------------------------------------------------------------------------
float4 DontCombineLocalAndGlobalOcclusionPS( in float2 vScreenPosition : TEXCOORD0 ) : COLOR
{
    float fOcclusion = tex2D( Sampler0, vScreenPosition ).r;
    
    // Sqaure the result to darken the occlusion
    fOcclusion *= fOcclusion;
    
    return float4( fOcclusion, fOcclusion, fOcclusion, 1.0f );
}


//--------------------------------------------------------------------------------------
// Vertex shader declarations for scene
//--------------------------------------------------------------------------------------
struct VSOUT_SCENE
{
    float4 vPosition        : POSITION;
    float3 vNormal          : TEXCOORD0;
    float2 vTexCoord        : TEXCOORD1;
};

//--------------------------------------------------------------------------------------
// Pixel shader constants for scene
//--------------------------------------------------------------------------------------
float4   g_vLightDir        : register( c0 );  // Light direction in world space
float4   g_vAmbient         : register( c1 );  // Ambient color

//--------------------------------------------------------------------------------------
// Name: ShadeSceneVS()
// Desc: Vertex shader to perform scene rendering
//--------------------------------------------------------------------------------------
VSOUT_SCENE ShadeSceneVS( const float3 vPosition    : POSITION,
                          const float3 vNormal      : NORMAL,
                          const float2 vTexCoord    : TEXCOORD0 )
{
    VSOUT_SCENE Output;

    // Transform vertex position
    Output.vPosition    = mul( float4( vPosition, 1.0f ), g_matWorldViewProj );

    // Pass vertex normal through
    Output.vNormal      = mul( vNormal, g_matWorld );
    
    // Pass texture coordinates through
    Output.vTexCoord    = vTexCoord;

    return Output;
}


//--------------------------------------------------------------------------------------
// Name: ShadeScenePS()
// Desc: Pixel shader to perform scene rendering
//--------------------------------------------------------------------------------------
float4 ShadeScenePS( VSOUT_SCENE Input  ) : COLOR
{
    // Normalize the normal and light direction
    float3 vNormal      = normalize( Input.vNormal );
    float3 vLightDir    = normalize( g_vLightDir );
    
    // Fetch the diffuse color
    float4 vDiffuseColor = tex2D( Sampler0, Input.vTexCoord.xy );
  
    // Compute diffuse lighting intensity
    float fDiffuseI     = saturate( dot( vNormal, vLightDir  ) );

    // Combine lighting with diffuse color
    float4 vPixelColor  = vDiffuseColor * fDiffuseI + g_vAmbient;
    vPixelColor.a = 1;
    
    return vPixelColor;
}

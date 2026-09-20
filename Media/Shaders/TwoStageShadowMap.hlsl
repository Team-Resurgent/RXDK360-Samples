//--------------------------------------------------------------------------------------
// TwoStageShadowMap.hlsl
//
// Extra shaders for two-stage shadow mapping sample.  The existing scene shaders from 
// ShadowMap.hlsl are still used unchanged.  This file contains utility shaders to 
// handle the special two-channel shadow map format.
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------


//--------------------------------------------------------------------------------------
// Shaders for direct copy to EDRAM
//--------------------------------------------------------------------------------------

// Shadow map texture for replication.
sampler2D ShadowTex : register(s0);


void ReplicateGreenVS( in float2 vPosition : POSITION, 
                    in float2 vTexCoord : TEXCOORD0,                   
                    out float4 oPosition : POSITION, 
                    out float2 oTexCoord : TEXCOORD0 )                   
{
    oPosition = float4( vPosition.x, vPosition.y, 1.0f, 1.0f );
    oTexCoord = vTexCoord;
}

float4 ReplicateGreenPS( in float2 vTexCoord : TEXCOORD0 ) : COLOR
{
    return tex2D( ShadowTex, vTexCoord ).yyyy;
}

//--------------------------------------------------------------------------------------
// Shaders for outputting depth
//--------------------------------------------------------------------------------------

// Standard world * view * projection transform.
float4x4 matWorldViewProj : register(c0);

// Simple position only vertex shader
void WriteDepthVS( in float4 vPosition : POSITION,
                   in float3 vNormal : NORMAL,
                   in float2 vTexCoord : TEXCOORD0,
                   out float4 oPosition : POSITION, 
                   out float2 oPosZW : TEXCOORD0 )
{
    oPosition = mul( vPosition, matWorldViewProj );
    oPosZW = oPosition.zw;
}


// Depth biases matching the current D3D states
float2 vBiases : register(c0);

// Receives depth in interpolator, performs perspective divide and depth-bias, 
// and broadcasts output to COLOR.  If the perspective matrix is known to 
// be orthogonal, we can skip the divide and reduce ALU load.  However, the 
// shader is likely to be fill-bound, or nearly so, as it stands.
float4 WriteDepthPS( in float2 vPosZW : TEXCOORD0 ) : COLOR
{
    // Perspective divide
    float fDepth = vPosZW.x / vPosZW.y;
    
    // This calculation matches what the GPU does, but at different precisions.
    // Depth bias occurs after perspective divide (because the scan converter is 
    // later in the pipeline than the primitive assembler).
    // These bias values should match those set as D3D states.
    float zBias = vBiases.x + vBiases.y * max( abs( ddx( fDepth ) ), abs( ddy( fDepth ) ) ); 
    
    // Standard viewport arrangement for fixed-point depth has far plane at 1.0
    // and near plane at 0.0.  So bias to the rear is a '+'.
    return fDepth + zBias;
}

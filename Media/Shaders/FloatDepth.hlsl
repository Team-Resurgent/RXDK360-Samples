//--------------------------------------------------------------------------------------
// FloatDepth.hlsl
//
// A sample showing how to use an inverted-direction floating-point depth buffer.
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------


// Standard world * view * projection transform.
float4x4 matWorldViewProj : register(c0);

// Local space light direction
float3 vLocalLightDir : register(c5);

// Local space view position
float3 vLocalViewPos : register(c6);


// Simple position only vertex shader
void FloatDepthVS( in float4 vPosition : POSITION,
                   in float3 vNormal : NORMAL,
                   out float4 oPosition : POSITION,
                   out float3 oNormal : NORMAL,
                   out float3 oLightDir : TEXCOORD0,
                   out float3 oHalfAngle : TEXCOORD1 )
{
    oPosition = mul( vPosition, matWorldViewProj );

    // Copy the normal
    oNormal = vNormal;
    
    // Light direction is constant for a directional light
    oLightDir = vLocalLightDir;
    
    // Compute the vector to the eye (V)
    float3 V = normalize( vLocalViewPos - vPosition );
    
    // Compute the half angle vector (H = L + V)
    oHalfAngle = normalize( vLocalLightDir + V );
}


// Ambient intensity
static float AmbientI = { 0.4f };

// Diffuse color
float4 vDiffuseColor : register(c0);

// Specular color
static float4 vSpecularColor = { 1.0f, 1.0f, 1.0f, 0.0f };


void FloatDepthPS( in float3 vNormal : NORMAL,
                   in float3 vLightDir : TEXCOORD0,
                   in float3 vHalfAngle : TEXCOORD1,
                   out float4 oColor : COLOR )
{
    // Normalize the normal and half angle vectors (if the geometry is sufficienty 
    // tessellated this could potentially be skipped).
    vNormal = normalize( vNormal );
    vHalfAngle = normalize( vHalfAngle );
    
    // Compute the diffuse and specluar contributions with shadowing
    float DiffuseI = max( 0, dot( vNormal, vLightDir ) ) + AmbientI;
    float SpecularI = pow( max( 0, dot( vNormal, vHalfAngle ) ), 20 );

    // Combine them with shadows and ambient
    oColor = DiffuseI * vDiffuseColor + SpecularI * vSpecularColor;
}

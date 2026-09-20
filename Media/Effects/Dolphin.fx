//--------------------------------------------------------------------------------------
// Dolphin.fx
//
// Dolphin Material FXLite Effect
// Inside water-scene objects are three passes:
//      Refraction
//      Reflection
//      Normal
//
// The refraction pass renders the object into a refraction texture.  This texture is
// later applied to the water surface, with perturbed coordinates to simulate
// refraction.  The deeper the object is in the water, the more watery color it is
// rendered to simulate underwater absorption.  Geometry above the water plane is
// clipped (by the shader).
//
// The reflection pass renders the object into a reflection texture.  This texture
// captures local reflections, such as the dolphin's fin above the water surface.
// Geometry below the water surface is clipped, and that above it is mirrored
// about the water plane.
// 
// The normal pass renders the portion of the object that is above the waterline
// and immediately visible to the viewer.
//
// Microsoft Game Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

// Parameter values shared across effects
shared float4x4 matProj        : register(c0);                                     
shared float4x4 matView        : register(c4);
shared float4x4 matViewProj    : register(c8);
shared float4   lightDirection : register(c12);
shared float4   I_a            : register(c13);                                      
shared float4   I_d            : register(c14);
shared float4   I_s            : register(c15);

uniform float4x4 matWorld;

uniform const float4 Diffuse = float4( 0.9, 0.9, 0.9, 1.f );
uniform const float4 Ambient = float4( 0.9, 0.9, 0.9, 1.f );
uniform const float4 Specular = float4( 0.5f, 0.5f, 0.5f, 0.f );
uniform const float4 Emissive = float4( 0.f, 0.f, 0.f, 0.f );
uniform const float  Power = 64.f;

uniform const float4 Fog = float4( 0.17f, 0.27f, 0.26f, 1.f );
uniform float3 vBlendWeights : register (c20) = float3( 0.f, 1.f, 0.f );


// Base Texture sampler
sampler base_sampler  : register (s1) = sampler_state
{
    MipFilter = LINEAR;
    MinFilter = LINEAR;
    MagFilter = LINEAR;
    
    AddressU = MIRROR;
    AddressV = MIRROR;
};                                

                                                                      
//--------------------------------------------------------------------------------------
// Vertex Shader Output
//--------------------------------------------------------------------------------------
struct VS_OUTPUT
{
    float4 Pos      : POSITION0;
    float3 Diffuse  : COLOR0;
    float3 Spec     : COLOR1;
    float2 TexCoord : TEXCOORD0;
    float  Depth    : TEXCOORD1;
};


//--------------------------------------------------------------------------------------
// Vertex Shader ("Normal" pass)
//--------------------------------------------------------------------------------------
VS_OUTPUT VS_Normal( 
    const float3 vPosition0 : POSITION0,
    const float3 vPosition1 : POSITION1,
    const float3 vPosition2 : POSITION2,
    const float3 vNormal0   : NORMAL0,
    const float3 vNormal1   : NORMAL1,
    const float3 vNormal2   : NORMAL2,
    const float2 TexCoord   : TEXCOORD0 )
{
    VS_OUTPUT Out = (VS_OUTPUT)0;
    
    // Tween the model vertex positions and normals
    float4 vModelPosition = float4( vPosition0 * vBlendWeights.x 
                                    + vPosition1 * vBlendWeights.y 
                                    + vPosition2 * vBlendWeights.z,
                                    1.0f );
    float3 vModelNormal = vNormal0 * vBlendWeights.x 
                            + vNormal1 * vBlendWeights.y 
                            + vNormal2 * vBlendWeights.z;

    matrix matWorldView = mul( matWorld, matView );

    // Transform the light into view space
    float3 L = -normalize( mul( lightDirection, (float3x3)matView ) );

    // position (view space)
    float3 P = mul( vModelPosition, (float4x3)matWorldView );
    
    // normal (view space)
    float3 N = normalize( mul( vModelNormal, (float3x3)matWorldView ) );     

    // Phong light reflection vector (view space)
    float3 R = reflect( -L, N );
    
    // view direction (view space)
    float3 V = -normalize( P );
  
    // Compute all outputs in view space
    Out.Pos      = mul( float4(P, 1), matProj);                         // position
    Out.Diffuse  = I_a * Ambient + I_d * Diffuse * max(0, dot(N, L));   // diffuse + amb
    Out.Spec     = I_s * Specular * pow( max( 0, dot(R, V) ), Power/4); // specular
    Out.TexCoord = TexCoord;
    // Unused in "normal pass" pixel shader
    // Out.Depth    = 0.f;
    return Out;
}


//--------------------------------------------------------------------------------------
// Vertex Shader
//--------------------------------------------------------------------------------------
VS_OUTPUT VS_Reflection(
    const float3 vPosition0 : POSITION0,
    const float3 vPosition1 : POSITION1,
    const float3 vPosition2 : POSITION2,
    const float3 vNormal0   : NORMAL0,
    const float3 vNormal1   : NORMAL1,
    const float3 vNormal2   : NORMAL2,
    const float2 TexCoord   : TEXCOORD0 )
{
    // Reflects geometry about the (assumed) y=0 water plane
    const matrix matReflect =
    {
        1.f, 0.f, 0.f, 0.f,
        0.f,-1.f, 0.f, 0.f,
        0.f, 0.f, 1.f, 0.f,
        0.f, 0.f, 0.f, 1.f
    };

    VS_OUTPUT Out = (VS_OUTPUT)0;

    // Tween the model vertex positions and normals
    float4 vModelPosition = float4( vPosition0 * vBlendWeights.x 
                                    + vPosition1 * vBlendWeights.y 
                                    + vPosition2 * vBlendWeights.z,
                                    1.0f );
    float3 vModelNormal = vNormal0 * vBlendWeights.x 
                            + vNormal1 * vBlendWeights.y 
                            + vNormal2 * vBlendWeights.z;

    matrix matWorldView = mul( matWorld, matView );

    // Transform the light into view space
    float3 L = -normalize( mul( lightDirection, (float3x3)matView ) );

    // position (view space)
    float3 P = mul( vModelPosition, (float4x3)matWorldView );
    
    // normal (view space)
    float3 N = normalize( mul( vModelNormal, (float3x3)matWorldView ) );

    // Phong light reflection vector (view space)
    float3 R = reflect( -L, N );
    
    // view direction (view space)
    float3 V = -normalize(P);                           
  
    // position (projected)
    // (geometry is reflected about the water plane)
    Out.Pos      = mul( vModelPosition, mul( mul( matWorld,matReflect ), matViewProj ));
    Out.Diffuse  = I_a * Ambient + I_d * Diffuse * max( 0, dot(N, L) ); // diffuse + amb
    Out.Spec     = I_s * Specular * pow(max(0, dot(R, V)), Power/4);    // specular
    Out.TexCoord = TexCoord;
    Out.Depth    = -mul( vModelPosition, matWorld ).y;
    return Out;
}


//--------------------------------------------------------------------------------------
// Vertex Shader
//--------------------------------------------------------------------------------------
VS_OUTPUT VS_Refraction(
    const float3 vPosition0 : POSITION0,
    const float3 vPosition1 : POSITION1,
    const float3 vPosition2 : POSITION2,
    const float3 vNormal0   : NORMAL0,
    const float3 vNormal1   : NORMAL1,
    const float3 vNormal2   : NORMAL2,
    const float2 TexCoord   : TEXCOORD0 )
{
    // "Scrunches" the scene, collapsing it towards y=0 to simulate refraction
    const matrix matScale =
    {
        1.f, 0.f,   0.f, 0.f,
        0.f, 0.75f, 0.f, 0.f,
        0.f, 0.f,   1.f, 0.f,
        0.f, 0.f,   0.f, 1.f
    };

    VS_OUTPUT Out = (VS_OUTPUT)0;

    // Tween the model vertex positions and normals
    float4 vModelPosition = float4( vPosition0 * vBlendWeights.x 
                                    + vPosition1 * vBlendWeights.y 
                                    + vPosition2 * vBlendWeights.z,
                                    1.0f );
    float3 vModelNormal = vNormal0 * vBlendWeights.x 
                        + vNormal1 * vBlendWeights.y 
                        + vNormal2 * vBlendWeights.z;

    matrix matWorldView = mul( matWorld, matView );

    // Transform the light into view space
    float3 L = -normalize( mul( lightDirection,(float3x3)matView ) );

    // position (view space)
    float3 P = mul( vModelPosition, (float4x3)matWorldView);
    
    // normal (view space)
    float3 N = normalize(mul(vModelNormal, (float3x3)matWorldView));

    // Phong light reflection vector (view space)
    float3 R = reflect( -L, N );
    
    // view direction (view space)
    float3 V = -normalize( P );
  
    // position (projected)
    // Include the scaling matrix in the position calculation, to "scrunch" the
    // scene, collapsing it towards y=0
    Out.Pos      = mul(vModelPosition, mul( mul( matWorld, matScale ), matViewProj ));
    Out.Diffuse  = I_a * Ambient + I_d * Diffuse * max(0, dot(N, L));   // diffuse + amb
    Out.Spec     = I_s * Specular * pow(max(0, dot(R, V)), Power/4 );   // specular
    Out.TexCoord = TexCoord;
    // Depth is used for clipping, as well as fading to the water background color
    Out.Depth    = mul( vModelPosition, matWorld ).y;
    return Out;
}


//--------------------------------------------------------------------------------------
// Pixel Shader (Refraction)
//--------------------------------------------------------------------------------------
float4 PS_Refraction(
    float3 Diffuse   : COLOR0,
    float3 Spec      : COLOR1,
    float2 TexCoords : TEXCOORD0,
    float  Depth     : TEXCOORD1 ) : COLOR
{
    // clip what's above the water plane
    // the small "fudge" factor prevents a discontinuity where the "normal" pass
    // geometry meets its refraction image in the scene.
    clip( -Depth.xxxx + 0.1f );
    
    // Compute the dolphin color
    float3 baseTexture = tex2D( base_sampler, TexCoords );
    float3 Color = (Diffuse * baseTexture) + Spec;

    // Fade the dolphin image, the deeper it is
    float fBlend = max(0,0.5f + Depth.x * .15f);
    return float4( lerp( Fog.rgb, Color, fBlend ), 1.f);
}


//--------------------------------------------------------------------------------------
// Pixel Shader (Reflection)
//--------------------------------------------------------------------------------------
float4 PS_Reflection(
    float3 Diffuse   : COLOR0,
    float3 Spec      : COLOR1,
    float2 TexCoords : TEXCOORD0,
    float  Depth     : TEXCOORD1 ) : COLOR
{
    // Clip what is beneath the water line
    clip( -Depth.xxxx );
    
    // Compute the dolphin color
    float3 baseTexture = tex2D( base_sampler, TexCoords );
    float3 Color = (Diffuse * baseTexture) + Spec;
    // Outputting 1 to the alpha channel here causes the water surface (when rendered)
    // to use the local reflection from the reflection map.  Areas where the alpha
    // value remained zero will reflect the environment map the skybox uses.
    return float4( Color, 1.f );
}


//--------------------------------------------------------------------------------------
// Pixel Shader (Normal)
//--------------------------------------------------------------------------------------
float4 PS_Normal(
    float3 Diffuse   : COLOR0,
    float3 Spec      : COLOR1,
    float2 TexCoords : TEXCOORD0 ) : COLOR
{
    // Compute dolphin color, and output it
    float3 baseTexture = tex2D( base_sampler, TexCoords );
    float3 Color = (Diffuse * baseTexture) + Spec;
    return float4( Color, 1.f);
}


//--------------------------------------------------------------------------------------
// Default Technique
// Establishes Vertex and Pixel Shader
//--------------------------------------------------------------------------------------
technique T0
{
    pass Refraction
    {
        // shaders
        VertexShader = compile vs_2_0 VS_Refraction();
        PixelShader  = compile ps_2_0 PS_Refraction();
    }  
    pass Reflection
    {
        // shaders
        VertexShader = compile vs_2_0 VS_Reflection();
        PixelShader  = compile ps_2_0 PS_Reflection();
        CullMode = CW;
    }  
    pass Normal
    {
        // shaders
        VertexShader = compile vs_2_0 VS_Normal();
        PixelShader  = compile ps_2_0 PS_Normal();
    }  
}



//--------------------------------------------------------------------------------------
// ComputeHeightmap.fx
//
// Generates water hight map based on simple sin waves
// Stores red = height
//
// Microsoft Game Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

// x,z = wave period scaling, y = wave amplitude scaling
uniform float3 scalings = float3( 50.f, 0.25f, 50.f );
uniform float fTime = 0.f;
                                                                      
//--------------------------------------------------------------------------------------
// Vertex Shader Output
//--------------------------------------------------------------------------------------
struct VS_OUTPUT
{
    float4 Pos     : POSITION;
    float2 UV      : TEXCOORD0;
};


//--------------------------------------------------------------------------------------
// Vertex Shader
//--------------------------------------------------------------------------------------
VS_OUTPUT VS(
    float2 Pos   : POSITION,
    float2 UV    : TEXCOORD0 )
{
    VS_OUTPUT Out = (VS_OUTPUT)0;
    
    // Pass through position (pre-transformed into clip space)
    Out.Pos = float4( Pos.x, Pos.y, 0.f, 1.f );
    
    // Pass through texture coordinates
    Out.UV = UV;
    
    return Out;
}


//--------------------------------------------------------------------------------------
// Pixel Shader
//--------------------------------------------------------------------------------------
float4 PS( float2 UV : TEXCOORD0 ) : COLOR
{
    // one wave runs the x axis
    float f = sin( UV.x * 3.14159 * 32 + 4.f * fTime );
    
    // another wave runs the y axis
    f += sin( UV.y * 3.14159 * 32 + 4.f * fTime );
    
    // another wave runs diagonally with a different period
    f += 0.25f * sin( (UV.x + UV.y)*3.14159f*64 + 8.f * fTime );

    // normalize the wave, and pack into 0-1
    f /= 2.25f;
    f += 1.f;
    f /= 2.f;
    float4 Output = float4( 0.f, 0.f, 0.f, 0.f);

    // scale by amplitude factor and output to red channel
    Output.r = scalings.y * f;
    
    return Output;
}


//--------------------------------------------------------------------------------------
// Default Technique
// Establishes Vertex and Pixel Shader
//--------------------------------------------------------------------------------------
technique T0
{
    pass P0
    {
        // shaders
        VertexShader = compile vs_2_0 VS();
        PixelShader  = compile ps_2_0 PS();

        HalfPixelOffset  = TRUE;
        ZEnable          = FALSE;
        ZWriteEnable     = FALSE;
    }  
}

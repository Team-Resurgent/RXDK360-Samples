//--------------------------------------------------------------------------------------
// PostProcessTest.hlsl
//
// Custom shaders for the Postprocess sample which runs experiments with different
// methods of applying fullscreen effects.
//
// XNA Developer Connection
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------
// Shader constants
//--------------------------------------------------------------------------------------
uniform float4 g_vBlurWeightsAndOffsets[2] : register(c12);   // interpretation varies

//--------------------------------------------------------------------------------------
// Texture samplers
//--------------------------------------------------------------------------------------
sampler Sampler0  : register(s0);

//--------------------------------------------------------------------------------------
// Pixel shaders
//--------------------------------------------------------------------------------------


//--------------------------------------------------------------------------------------
// Name: Blur4x4NaiveHorizontalPS()
// Desc: Naive method for performing a 4x4 separable blur.  (Can do better if the 
//       texture is filterable.)  
//
// uniform float4 g_vBlurWeightsAndOffsets[2] : register(c12) // Weights are [0].xyzw
//                                                            // Offsets are hard-coded
//--------------------------------------------------------------------------------------
float4 Blur4x4NaiveHorizontalPS( in float2 vTexCoord : TEXCOORD0 ) : COLOR
{
    float4 t0, t1, t2, t3;
    asm
    {
        tfetch2D t0, vTexCoord, Sampler0, OffsetX = -2 
        tfetch2D t1, vTexCoord, Sampler0, OffsetX = -1 
        tfetch2D t2, vTexCoord, Sampler0, OffsetX =  0 
        tfetch2D t3, vTexCoord, Sampler0, OffsetX = +1 
    };
    
    return  g_vBlurWeightsAndOffsets[0].x * t0 
        +   g_vBlurWeightsAndOffsets[0].y * t1
        +   g_vBlurWeightsAndOffsets[0].z * t2
        +   g_vBlurWeightsAndOffsets[0].w * t3;
}

//--------------------------------------------------------------------------------------
// Name: Blur4x4NaiveVerticalPS()
// Desc: Naive method for performing a 4x4 separable blur.  (Can do better if the 
//       texture is filterable.)  
//
// uniform float4 g_vBlurWeightsAndOffsets[2] : register(c12) // Weights are [0].xyzw
//                                                            // Offsets are hard-coded
//--------------------------------------------------------------------------------------
float4 Blur4x4NaiveVerticalPS( in float2 vTexCoord : TEXCOORD0 ) : COLOR
{
    float4 t0, t1, t2, t3;
    asm
    {
        tfetch2D t0, vTexCoord, Sampler0, OffsetY = -2 
        tfetch2D t1, vTexCoord, Sampler0, OffsetY = -1 
        tfetch2D t2, vTexCoord, Sampler0, OffsetY =  0 
        tfetch2D t3, vTexCoord, Sampler0, OffsetY = +1 
    };
    
    return  g_vBlurWeightsAndOffsets[0].x * t0 
        +   g_vBlurWeightsAndOffsets[0].y * t1
        +   g_vBlurWeightsAndOffsets[0].z * t2
        +   g_vBlurWeightsAndOffsets[0].w * t3;
}

//--------------------------------------------------------------------------------------
// Name: Blur4x4TunedPS()
// Desc: Tuned method for performing a 4x4 separable blur.  
//
// uniform float4 g_vBlurWeightsAndOffsets[2] : register(c12) // Weights are [1].xy
//                                                            // Offsets are [0].xy/zw
//--------------------------------------------------------------------------------------
float4 Blur4x4TunedPS( in float2 vTexCoord : TEXCOORD0 ) : COLOR
{
    float4 t0, t1;
    t0 = tex2D( Sampler0, vTexCoord + g_vBlurWeightsAndOffsets[0].xy );
    t1 = tex2D( Sampler0, vTexCoord + g_vBlurWeightsAndOffsets[0].zw );
    
    return  g_vBlurWeightsAndOffsets[1].x * t0 
        +   g_vBlurWeightsAndOffsets[1].y * t1;
}

//--------------------------------------------------------------------------------------
// Name: Blur4x4IdealPS()
// Desc: Ideal method for performing a 4x4 separable blur. 
// 
// uniform float4 g_vBlurWeightsAndOffsets[2] : register(c12) // Weights are [1].xyzw
//                                                            // Offsets are [0].xw/yz
//                                                            //             [0].xz/yw
//--------------------------------------------------------------------------------------
float4 Blur4x4IdealPS( in float2 vTexCoord : TEXCOORD0 ) : COLOR
{
    float4 t0, t1, t2, t3;
    t0 = tex2D( Sampler0, vTexCoord + g_vBlurWeightsAndOffsets[0].xz );
    t1 = tex2D( Sampler0, vTexCoord + g_vBlurWeightsAndOffsets[0].xw );
    t2 = tex2D( Sampler0, vTexCoord + g_vBlurWeightsAndOffsets[0].yz );
    t3 = tex2D( Sampler0, vTexCoord + g_vBlurWeightsAndOffsets[0].yw );
    
    return  g_vBlurWeightsAndOffsets[1].x * t0 
        +   g_vBlurWeightsAndOffsets[1].y * t1 
        +   g_vBlurWeightsAndOffsets[1].z * t2 
        +   g_vBlurWeightsAndOffsets[1].w * t3;
}

//--------------------------------------------------------------------------------------
// Name: Blur5x5NaiveHorizontalPS()
// Desc: Naive method for performing a 5x5 separable blur.  (Can do better if the 
//       texture is filterable.)  
// 
// uniform float4 g_vBlurWeightsAndOffsets[2] : register(c12) // Weights are [0].xyzw/[1].x
//                                                            // Offsets are hard-coded
//--------------------------------------------------------------------------------------
float4 Blur5x5NaiveHorizontalPS( in float2 vTexCoord : TEXCOORD0 ) : COLOR
{
    float4 t0, t1, t2, t3, t4;
    asm
    {
        tfetch2D t0, vTexCoord, Sampler0, OffsetX = -2 
        tfetch2D t1, vTexCoord, Sampler0, OffsetX = -1 
        tfetch2D t2, vTexCoord, Sampler0, OffsetX =  0 
        tfetch2D t3, vTexCoord, Sampler0, OffsetX = +1 
        tfetch2D t4, vTexCoord, Sampler0, OffsetX = +2 
    };
    
    return  g_vBlurWeightsAndOffsets[0].x * t0 
        +   g_vBlurWeightsAndOffsets[0].y * t1
        +   g_vBlurWeightsAndOffsets[0].z * t2
        +   g_vBlurWeightsAndOffsets[0].w * t3
        +   g_vBlurWeightsAndOffsets[1].x * t4;
}

//--------------------------------------------------------------------------------------
// Name: Blur5x5NaiveVerticalPS()
// Desc: Naive method for performing a 5x5 separable blur.  (Can do better if the 
//       texture is filterable.)  
// 
// uniform float4 g_vBlurWeightsAndOffsets[2] : register(c12) // Weights are [0].xyzw/[1].x
//                                                            // Offsets are hard-coded
//--------------------------------------------------------------------------------------
float4 Blur5x5NaiveVerticalPS( in float2 vTexCoord : TEXCOORD0 ) : COLOR
{
    float4 t0, t1, t2, t3, t4;
    asm
    {
        tfetch2D t0, vTexCoord, Sampler0, OffsetY = -2 
        tfetch2D t1, vTexCoord, Sampler0, OffsetY = -1 
        tfetch2D t2, vTexCoord, Sampler0, OffsetY =  0 
        tfetch2D t3, vTexCoord, Sampler0, OffsetY = +1 
        tfetch2D t4, vTexCoord, Sampler0, OffsetY = +2 
    };
    
    return  g_vBlurWeightsAndOffsets[0].x * t0 
        +   g_vBlurWeightsAndOffsets[0].y * t1
        +   g_vBlurWeightsAndOffsets[0].z * t2
        +   g_vBlurWeightsAndOffsets[0].w * t3
        +   g_vBlurWeightsAndOffsets[1].x * t4;
}

//--------------------------------------------------------------------------------------
// Name: Blur5x5TunedPS()
// Desc: Tuned method for performing a 5x5 separable blur.  
// 
// uniform float4 g_vBlurWeightsAndOffsets[2] : register(c12) // Weights are [1].xyz
//                                                            // Offsets are [0].xy/zw
//--------------------------------------------------------------------------------------
float4 Blur5x5TunedPS( in float2 vTexCoord : TEXCOORD0 ) : COLOR
{
    float4 t0, t1, t2;
    t0 = tex2D( Sampler0, vTexCoord + g_vBlurWeightsAndOffsets[0].xy );
    t1 = tex2D( Sampler0, vTexCoord );
    t2 = tex2D( Sampler0, vTexCoord + g_vBlurWeightsAndOffsets[0].zw );
    
    return  g_vBlurWeightsAndOffsets[1].x * t0 
        +   g_vBlurWeightsAndOffsets[1].y * t1
        +   g_vBlurWeightsAndOffsets[1].z * t2;
}

//--------------------------------------------------------------------------------------
// Name: Blur5x5IdealPS()
// Desc: Ideal method for performing a 5x5 separable blur.  
// 
// uniform float4 g_vBlurWeightsAndOffsets[2] : register(c12) // Weights are [1].xz
//                                                            // Offsets are [0].xy/zw
//--------------------------------------------------------------------------------------
float4 Blur5x5IdealPS( in float2 vTexCoord : TEXCOORD0 ) : COLOR
{
    float4 t0, t2;
    t0 = tex2D( Sampler0, vTexCoord + g_vBlurWeightsAndOffsets[0].xy );
    t2 = tex2D( Sampler0, vTexCoord + g_vBlurWeightsAndOffsets[0].zw );

    return  g_vBlurWeightsAndOffsets[1].x * t0 
        +   g_vBlurWeightsAndOffsets[1].z * t2;
}

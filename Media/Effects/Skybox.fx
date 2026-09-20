//--------------------------------------------------------------------------------------
// Skybox.fx
//
// Skybox FXLite Effect
// Skybox is drawn by removing translation information from the view matrix and
// computing cubmap coordinates from a cube.
// Note:  skybox must be drawn first (z test is not enabled, it will be overdraw)
//
// Microsoft Game Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

// Parameter values shared across effects
shared float4x4 matProj        : register(c0);                                      
shared float4x4 matView        : register(c4);
shared float4x4 matViewProj    : register(c8);   // (not used)
shared float4   lightDirection : register(c12);  // (not used)
shared float4   I_a            : register(c13);  // (not used)                                     
shared float4   I_d            : register(c14);  // (not used)
shared float4   I_s            : register(c15);  // (not used)

// Define an (unused) structure
// The FXLDumpInfo sample walks this effect & its parameters,
// this demonstrates how to walk a structure or array
struct thisIsAStruct
{
    float4 StructMember1;
    float4 StructMember2;
} myStruct;

thisIsAStruct myStruct2;

// Define an (unused) array to demonstrate walking arrays
float4 myArray[2]
    <string Comment1="this is a my first annotation comment!";
     string Comment2="this is a my second annotation comment!";> 
    =
{
    float4( 0, 1, 2, 3 ),
    float4( 4, 5, 6, 7 )
};
                                                                      

//--------------------------------------------------------------------------------------
// Vertex Shader
// Assumes view matrix is well behaved.
//--------------------------------------------------------------------------------------
void VS ( in  float3 v0   : POSITION,                                 
          out float4 oPos : POSITION,                                 
          out float3 oT0  : TEXCOORD0 )                               
{                                                                     
    // Remove translation orientation from view transform
    float4x4 matViewNoTrans = matView;                                
    matViewNoTrans._41 = 0.f;                                         
    matViewNoTrans._42 = 0.f;                                         
    matViewNoTrans._43 = 0.f;                                         
                                                                      
    // Now, transform the skybox based only on the rotations
    // (2.f scaling factor is simply because the cube runs x/y/z = +/- 1.f, but the
    // projection matrix set the near plane of the frustum to 1.f.
    oPos = mul( float4(2.f*v0,1.f), mul( matViewNoTrans, matProj ) );
    oPos.z = 1.f;
                                                                      
    oT0 = v0;                                                         
}                                                                     


// Cubmap sampler
shared sampler envmap_sampler  : register (s0) = sampler_state
{
    MipFilter = LINEAR;
    MinFilter = LINEAR;
    MagFilter = LINEAR;
    
    // Cubemaps must be set to clamp to minimize artifacts
    // (Hardware cannot appropriately filter cubemap seams)
    AddressU = CLAMP;
    AddressV = CLAMP;
};                                

                                                                      
//--------------------------------------------------------------------------------------
// Pixel Shader
//--------------------------------------------------------------------------------------
void PS( in  float3 t0 : TEXCOORD0,                                                                                                   
         out float4 r0 : COLOR0 )                                                                                                      
{                                                                     
    // The skybox texture is pre-lit, so simply output the texture color                                                               
    r0 = texCUBE( envmap_sampler, t0 );                              
}                                                                    


                                                                     
//--------------------------------------------------------------------------------------
// Technique
//--------------------------------------------------------------------------------------
technique T0
{
    pass P0
    {
        VertexShader = compile vs_2_0 VS();
        PixelShader = compile ps_2_0 PS();
        
        ZWriteEnable = FALSE;
        ZEnable      = FALSE;
    }
}
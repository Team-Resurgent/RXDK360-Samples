//--------------------------------------------------------------------------------------
// Shaders for PredicatedTilingVPos sample
//
// XNA Developer Connection.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

// Vertex shader constants
float4x4 matWVP : register(c0);                                            

// Pixel shader constants
float3 LightDirection : register(c0);    

// Pixel shader constant for the screenspace offset.
// The rendering code will make this constant have a different value on each tiling pass.
float4 ScreenSpaceOffset : register(c4);                                  
                                                                           
struct VS_OUT                                                              
{                                                                          
    float4 ProjPos   : POSITION;                                            
    float3 ObjNormal : TEXCOORD0;                                               
};                                                                         
                                                                           
VS_OUT vs_main( const float3 ObjPos : POSITION,                               
                const float3 Normal : NORMAL,                                 
                const float2 TexCoord : TEXCOORD0 )                           
{                                                                          
    VS_OUT Out;                                                            
    Out.ProjPos = mul( matWVP, float4( ObjPos, 1 ) ); 
    Out.ObjNormal = Normal;                   
    return Out;                                                            
}                                                                          

struct PS_IN                                   
{                                              
    float3 ObjNormal : TEXCOORD0;                      
};                                             
                                               
float4 ps_main( PS_IN In, float2 ScreenPos : VPOS ) : COLOR                
{
    // Compute lighting.
    float3 Normal = normalize( In.ObjNormal );
    float4 Result = saturate( dot( Normal, -LightDirection ) );                
    Result += 0.3 * saturate( dot( Normal, LightDirection ) );
    
    // Accumulate the current screenspace offset into the screen pos, which came from the VPOS interpolator.
    // VPOS only reports the pixel offset from the upper left corner of the rendertarget.
    // During tiling, the VPOS values will "reset" for each tile, so the offset shader constant is required to obtain the true screenspace position.
    ScreenPos += ScreenSpaceOffset.xy;
    
    // Convert the true screen pos into a gradient running across the entire 1280x720 screen.
    float2 ScreenFraction = ScreenPos / float2( 1280, 720 );
    Result *= float4( 1 - ScreenFraction, 0.5f, 1 );
    
    return Result;                           
}                                              

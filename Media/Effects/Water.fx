//--------------------------------------------------------------------------------------
// Water.fx
//
// Projected Grid Water FXLite Effect
//
// Microsoft Game Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#define TESSELLATION

// Parameter values shared across effects
shared float4x4 matProj        : register(c0);                                     
shared float4x4 matView        : register(c4);
shared float4x4 matViewProj    : register(c8);
shared float4   lightDirection : register(c12);
shared float4   I_a            : register(c13);                                      
shared float4   I_d            : register(c14);
shared float4   I_s            : register(c15);

// Used for (proceedurally) blooming the sun's reflection against the water
uniform const float sun_shininess = 250.f;
uniform const float sun_strength  = 6.f;

// Water colors
uniform const float4 Diffuse = float4( 0.17f, 0.27f, 0.26f, 1.f );
uniform const float4 Ambient = float4( 0.17f, 0.27f, 0.26f, 1.f );
uniform const float4 Specular = float4( 0.2f, 0.2f, 0.2f, 0.f );
uniform const float4 Emissive = float4( 0.f, 0.f, 0.f, 0.f );
uniform const float  Power = 32.f;
uniform const float  k_r = 0.30f;

// Transforms from clip space into our temporary world coordinates
// where water vertices are evaluated.
uniform float4x4  matRange : register( c24 );                                                                      

uniform float3 scalings = float3( 50.f, 0.25f, 50.f );
uniform float3 eye_W    = float3( 0.f, 0.f, 0.f );
uniform const float  reflrefr_offset=0.1f;

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


sampler heightmap_sampler  : register (s1) = sampler_state
{
    MipFilter = POINT;
    MinFilter = POINT;
    MagFilter = POINT;
    
    AddressU = WRAP;
    AddressV = WRAP;
};                                

sampler normal_sampler  : register (s2) = sampler_state
{
    MipFilter = LINEAR;
    MinFilter = LINEAR;
    MagFilter = LINEAR;
    
    AddressU = WRAP;
    AddressV = WRAP;
};                                

sampler fresnel_sampler  : register (s3) = sampler_state
{
    MipFilter = LINEAR;
    MinFilter = LINEAR;
    MagFilter = LINEAR;
    
    AddressU = CLAMP;
    AddressV = CLAMP;
};                                

sampler refl_sampler  : register (s4) = sampler_state
{
    MipFilter = LINEAR;
    MinFilter = LINEAR;
    MagFilter = LINEAR;
    
    AddressU = CLAMP;
    AddressV = CLAMP;
};                                
                                                                      
sampler refr_sampler  : register (s5) = sampler_state
{
    MipFilter = LINEAR;
    MinFilter = LINEAR;
    MagFilter = LINEAR;
    
    AddressU = CLAMP;
    AddressV = CLAMP;
};                                

//--------------------------------------------------------------------------------------
// Vertex Shader Output
//--------------------------------------------------------------------------------------
struct VS_OUTPUT
{
    float4 Pos       : POSITION;
    float4 NMCoords  : TEXCOORD0;
    float3 View_W    : TEXCOORD1;
    float3 screenPos : TEXCOORD2;   
};

//--------------------------------------------------------------------------------------
// Vertex Shader
//--------------------------------------------------------------------------------------
VS_OUTPUT VS(
#ifdef TESSELLATION
    in  int    vIndex      : INDEX,                              
    in  float3 vUVW        : BARYCENTRIC,                  
    in  int    vQuadID     : QUADID
#else
    in  float4 pos         : POSITION0
#endif
     )
{
    VS_OUTPUT Out = (VS_OUTPUT)0;
    
#ifdef TESSELLATION
    // Fetch the corners of the base triangle                           
    float4 pos0, pos1, pos2;                              
    asm {                                                               
        vfetch pos0, vIndex.x, position0                                
        vfetch pos1, vIndex.x, position1                                
        vfetch pos2, vIndex.x, position2                                
    };                                                                  
                                                                        
    // Re-order the weights based on the QuadID                         
    float3 uvw = vUVW * (vQuadID == 0);                                 
    uvw += vUVW.zxy * (vQuadID == 1);                                   
    uvw += vUVW.yzx * (vQuadID == 2);                                   
    uvw += vUVW.xzy * (vQuadID == 4);                                   
    uvw += vUVW.yxz * (vQuadID == 5);                                   
    uvw += vUVW.zyx * (vQuadID == 6);                                   
                                                                        
    // Weight them by the barycentric coordinates                       
    float4 PS = pos0 * uvw.z + pos1 * uvw.y + pos2 * uvw.x;            
#else
    float4 PS = pos;
#endif
    
    // Project the clip space coordinate into world space on the y=0 plane
    PS.z = - ( matRange._12 * PS.x 
            + matRange._22 * PS.y 
            + matRange._42  /*- Y_water_plane*/  ) 
                                                    / matRange._32;
    float4 Pos = mul( PS, matRange );
    
    // Pos is now in world-space
    Pos = Pos / Pos.w;

    // Compute Texture coord for height map sample.
    float4 tc = float4( Pos.x / scalings.x, Pos.z / scalings.z, 0.f, 0.f );

    // Sample the heightmap
    float4 t = tex2Dlod( heightmap_sampler, tc );
    
    // Multiple wave amplitude by it's scaling factor
    Pos.y += scalings.y * t.a;
    
    float3 View_W = normalize( (float3)Pos - eye_W );

    Out.Pos = mul( Pos, matViewProj );
    Out.NMCoords = tc;
    Out.View_W = View_W;

    // Compute the perturbations used to sample the reflection and refraction textures
    float4 tpos = mul( float4( Pos.x, 0, Pos.z, 1 ), matViewProj );
    Out.screenPos = tpos.xyz/tpos.w;
    Out.screenPos.xy = 0.5 + 0.5 * Out.screenPos.xy * float2(1, -1 );
    Out.screenPos.z = reflrefr_offset / Out.screenPos.z;    
    
    return Out;
}


//--------------------------------------------------------------------------------------
// Pixel Shader
//--------------------------------------------------------------------------------------
float4 PS( VS_OUTPUT i ) : COLOR
{
    // Sample the Normal map
    float3 Normal_W = tex2Dlod( normal_sampler, i.NMCoords ).rgb;
    float3 R_W = normalize( reflect( i.View_W, Normal_W ) );
    
    // sample the reflection & refraction maps
    float4 refl = tex2D( refl_sampler, i.screenPos.xy - i.screenPos.z * Normal_W.xz );
    float3 refr = tex2D( refr_sampler, i.screenPos.xy - i.screenPos.z * Normal_W.xz );

    // proceedurally bloom the sun
    float3 sunlight = sun_strength
                    * pow( saturate( dot( R_W, -lightDirection ) ), sun_shininess )
                    * float3( 1.2, 0.9, 0.7 );

    // Sample the Environment map
    // the reflection alpha channel masks the global reflection (env map) by the
    // local reflection (from the textures), when local reflections are present
    // Note: lerp the value to avoid hard edges along the reflection silhouette
    float3 ReflectionC = lerp(  texCUBE( envmap_sampler, R_W ), refl.rgb, refl.a );
    ReflectionC += sunlight;
    
    // Fetch the fresnel term
    float f = tex1D( fresnel_sampler, dot( R_W, Normal_W ).x ).a;
    
    // Return the sum of Vertex-Specular, the Reflection, 
    return float4( lerp( refr, ReflectionC, f ), 1.f );
}


//--------------------------------------------------------------------------------------
// Pixel Shader when wireframe drawing is enabled
//--------------------------------------------------------------------------------------
float4 PS_BLACK( [unused] VS_OUTPUT i ) : COLOR
{
    return float4( 0.f, 0.f, 0.f, 1.f );
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
        
        TessellationMode = CONTINUOUS;
        MinTessellationLevel = 8.f;
        MaxTessellationLevel = 8.f;
    }  
    
    pass P_WIREFRAME
    {
        // shaders
        VertexShader = compile vs_2_0 VS();
        PixelShader  = compile ps_2_0 PS_BLACK();
        
        TessellationMode = CONTINUOUS;
        MinTessellationLevel = 1.f;
        MaxTessellationLevel = 1.f;

        FillMode = WIREFRAME;
        ZFunc    = ALWAYS;
    } 
}

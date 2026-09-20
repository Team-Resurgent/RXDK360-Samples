//--------------------------------------------------------------------------------------
// Shaders for the GPUParticle sample
//--------------------------------------------------------------------------------------

struct VSUPDATEOUT
{
    float4 Position  : POSITION;
    float2 TexCoord0 : TEXCOORD;
};

struct PSUPDATEOUT
{
    float4 Color0    : COLOR0;
    float4 Color1    : COLOR1;
    float4 Color2    : COLOR2;
    float4 Color3    : COLOR3;
};

struct VSRENDEROUT
{
    float4 Position  : POSITION;
    float  PSize     : PSIZE;
};

uniform float4x4 WorldView      : register(c8);  // matWorldView
uniform float4x4 WorldViewProj  : register(c4);  // matWorldViewProjection
uniform float4   Constants      : register(c1);  // ( 1, 0.5, 2, 4 )
uniform float4   Zero           : register(c0);  // ( 0, 0, 0, 0 )


//--------------------------------------------------------------------------------------
// Name: ParticleUpdateVS()
// Desc: Vertex shader for updating particles
//--------------------------------------------------------------------------------------
VSUPDATEOUT ParticleUpdateVS( float2 Position : POSITION,
                              float2 TexCoord : TEXCOORD )
{
    VSUPDATEOUT Output;

    // Passthough position & UV
    Output.Position  = float4( Position, 0.0f, 1.0f );
    Output.TexCoord0 = TexCoord;
    return Output;
}

sampler ParticlePosXY : register(s0);
sampler ParticlePosZW : register(s1);
sampler ParticleVelXY : register(s2);
sampler ParticleVelZW : register(s3);
sampler PlaneMap      : register(s4);
sampler NoiseTex      : register(s5);

uniform float4  Acceleration_Delta       : register(c0);
uniform float4  EmitterParamPos          : register(c1);    // Position
uniform float4  EmitterParamPosRange     : register(c2);    // Position range
uniform float4  EmitterParamVelRange     : register(c3);    // Velocity range in XZ plane, offset in YZ, Velocity range in XY plane, offset in XY, 
uniform float4  EmitterParamLifeAndSpeed : register(c4);    // Speed range, speed offset, Life range, life offset


//--------------------------------------------------------------------------------------
// Name: ParticleUpdatePS()
// Desc: Pixel shader for updating particles
//--------------------------------------------------------------------------------------
PSUPDATEOUT ParticleUpdatePS( VSUPDATEOUT Input )
{
    PSUPDATEOUT Output;

    // Fetch and merge 2 float32-float32 textures
    float4 ParticlePos = float4( tex2D( ParticlePosXY, Input.TexCoord0 ).xy, tex2D( ParticlePosZW, Input.TexCoord0 ).xy );
    float4 ParticleVel = float4( tex2D( ParticleVelXY, Input.TexCoord0 ).xy, tex2D( ParticleVelZW, Input.TexCoord0 ).xy );

    // Update particle life
    ParticlePos.w -= Acceleration_Delta.w;

    if( ParticlePos.w <= 0.0f )
    {
        // Emit new particle
        // Use Vertpos / modf for random value.
        float fTmp;
        float4 fRandom;
        fRandom = tex2D( NoiseTex, ParticlePos.xz );
        
        // Set velocity
        sincos( fRandom.x * EmitterParamVelRange.x + EmitterParamVelRange.y , ParticleVel.x, ParticleVel.z );
        sincos( fRandom.y * EmitterParamVelRange.z + EmitterParamVelRange.w , ParticleVel.y, fTmp );
        ParticleVel.xy *= float2( fTmp, ParticleVel.x );
      
        // Set speed
        ParticleVel.xyz *= EmitterParamLifeAndSpeed.x * fRandom.z + EmitterParamLifeAndSpeed.y;

        // Set pos
        ParticlePos.xyz = EmitterParamPos.xyz + EmitterParamPosRange.xyz * fRandom.wzy;

        // Set life
        ParticlePos.w = EmitterParamLifeAndSpeed.z * fRandom.w + EmitterParamLifeAndSpeed.w;
    }

    // Update particle pos
    ParticlePos.xyz += ParticleVel.xyz  * Acceleration_Delta.w;
    
    // Fetch plane data from plane map
    float3 ParticlePos2 = ParticlePos.xyz * 1.0f / 64.0f + 0.5f;
    float4 Plane = tex2D( PlaneMap, ParticlePos2.xz );
    
    // Calc collision parameters
    float fNV = dot( ParticleVel.xyz,  Plane.xyz);
    float fNQ = dot( float4( ParticlePos.xyz, 1.0f ), Plane.xyzw );

    if( fNV <= 0 && fNQ <= 0 )
    {
        // Adjust position
        ParticlePos.xyz = ParticlePos.xyz - Plane.xyz * fNQ; 
        // Adjust vel
        ParticleVel.xyz = reflect (ParticleVel.xyz, Plane.xyz) * 0.90;
        
        // Test code for normal
        //ParticleVel.xyz = ParticleVel.xyz;
    }

    // Update velocity
    ParticleVel.xyz += Acceleration_Delta.xyz * Acceleration_Delta.w;
    ParticleVel.xyz *= 0.9995f;                        // Termination speed factor

    // Store result to four 32f:32f textures.
    // Note that Xenon does not support 32f:32f:32f:32f rendertargets
    Output.Color0 = float4( ParticlePos.xy, 0.0f, 1.0f );
    Output.Color1 = float4( ParticlePos.zw, 0.0f, 1.0f );
    Output.Color2 = float4( ParticleVel.xy, 0.0f, 1.0f );
    Output.Color3 = float4( ParticleVel.zw, 0.0f, 1.0f );
    return Output;
}


//--------------------------------------------------------------------------------------
// Name: ParticleRender()
// Desc: Vertex shader for rendering particles
//--------------------------------------------------------------------------------------
VSRENDEROUT ParticleRenderVS( const float2 PositionXY : POSITION0,
                              const float2 PositionZW : POSITION1 )
{
    VSRENDEROUT Output;

    // Transform position to the clipping space 
    float4 pos = float4( PositionXY.xy, PositionZW.x, 0.0f );
    Output.Position = mul( float4( pos.xyz, 1.0f ), WorldViewProj );
    Output.PSize    = 12.0f - Output.Position.z;
    return Output;
}


//--------------------------------------------------------------------------------------
// Name: ParticleRenderPS()
// Desc: Pixel shader for rendering particles
//--------------------------------------------------------------------------------------
sampler ParticleTexture : register(s0);

float4 ParticleRenderPS( float2 TexCoord0 : SPRITETEXCOORD ) : COLOR
{
    return tex2D( ParticleTexture, TexCoord0 );
}
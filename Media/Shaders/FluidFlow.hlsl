//--------------------------------------------------------------------------------------
// Flow.hlsl
//
// Shaders for fluid flow simulation. The implementation here is based on Stam's
// "Stable Fluids" SIGGRAPH 99 paper and Harris' flow implementation in OGL.
//
// Authored by Pedro Sander, ATI Research
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
// ATI 3D Application Research Group. 
// Copyright (C) ATI Research, Inc. All rights reserved. 
//--------------------------------------------------------------------------------------

uniform float2   g_vFlowDims                : register(c4);
uniform float2   g_vInvFlowDims             : register(c5);

uniform float    g_fTimestep                : register(c6);
uniform float    g_fDissipation             : register(c7);

uniform float    g_fScale                   : register(c8);

uniform float    g_fRootVisParticles        : register(c17);

uniform float    g_fCenterFactor            : register(c18);
uniform float    g_fStencilFactor           : register(c19);

#define NUM_INJECTORS 2
uniform float2   g_vPosition[NUM_INJECTORS] : register(c20);
uniform float    g_fRadius[NUM_INJECTORS]   : register(c22);
uniform float3   g_vColor[NUM_INJECTORS]    : register(c24);


sampler TextureSampler0 : register(s0);
sampler TextureSampler1 : register(s1);


//--------------------------------------------------------------------------------------
// Name: tex2Dl()
// Desc: Bilinear interpolation helper function, used since Alpha hardware cannot
//       bilinear filter floating point textures.
//--------------------------------------------------------------------------------------
float4 tex2Dl( sampler samp, float2 tc )
{
    // Adjust for half-pixel offset
    tc -= 0.5f * g_vInvFlowDims;

    // Fetch 4 nearest texels
    float2 uv1   = floor( tc * g_vFlowDims ) * g_vInvFlowDims;
    float2 uv2   = uv1 + g_vInvFlowDims;

    float4 tex12 = tex2D( samp, float2( uv1.x, uv2.y ) ); 
    float4 tex22 = tex2D( samp, float2( uv2.x, uv2.y ) ); 
    float4 tex11 = tex2D( samp, float2( uv1.x, uv1.y ) ); 
    float4 tex21 = tex2D( samp, float2( uv2.x, uv1.y ) ); 

    // Calc interpolating factors
    float2 s = ( tc - uv1 ) * g_vFlowDims;

    // Lerp the 4 texels together
    float4 tex1 = lerp( tex11, tex21, s.x );
    float4 tex2 = lerp( tex12, tex22, s.x );
    return lerp( tex1, tex2, s.y );
}

//--------------------------------------------------------------------------------------
// Name: gaussian()
// Desc: Gaussian distribution helper function
//--------------------------------------------------------------------------------------
float gaussian( float d2, float fRadius )
{
    return exp( -d2 / fRadius );
}


//--------------------------------------------------------------------------------------
// Name: FlowAddImpulsePS()
// Desc: Add values to density and velocity buffer
//--------------------------------------------------------------------------------------
float4 FlowAddImpulsePS( float2 TexCoord0 : TEXCOORD0 ) : COLOR
{
    float4 vOutput = tex2D( TextureSampler0, TexCoord0 );
    
    for( int i=0; i<NUM_INJECTORS; i++ )
    {
        float2 vPos   = g_vPosition[i] - TexCoord0;
        float  fDist2 = dot( vPos, vPos );
        vOutput += g_vColor[i].xyzz * gaussian( fDist2, g_fRadius[i] );
    }
    return vOutput;
} 


//--------------------------------------------------------------------------------------
// Name: FlowAdvectPS()
// Desc: Advect buffers based on velocity
//       (TextureSampler0 = velocity; TextureSampler1 = buffer to be advected)
//--------------------------------------------------------------------------------------
float4 FlowAdvectPS( float2 TexCoord0 : TEXCOORD0 ) : COLOR
{
    float2 vVelocity = tex2D( TextureSampler0, TexCoord0 ).xy; 
    float2 vPosition = TexCoord0 - g_fTimestep * vVelocity * g_vInvFlowDims;

    return g_fDissipation * tex2Dl( TextureSampler1, vPosition );
} 


//--------------------------------------------------------------------------------------
// Name: FlowBCStripPS()
// Desc: Set boundary value to be equal (or negative) of neighbor
//--------------------------------------------------------------------------------------
float4 FlowBCStripPS( float2 TexCoord0 : TEXCOORD0 ) : COLOR
{
    float2 tc = TexCoord0 - 0.5f * g_vInvFlowDims;
    if(      tc.x < 0.0f+0.5f*g_vInvFlowDims.x )   tc.x += g_vInvFlowDims.x;
    else if( tc.y < 0.0f+0.5f*g_vInvFlowDims.y )   tc.y += g_vInvFlowDims.y;
    else if( tc.x > 1.0f-1.5f*g_vInvFlowDims.x )   tc.x -= g_vInvFlowDims.x;
    else if( tc.y > 1.0f-1.5f*g_vInvFlowDims.y )   tc.y -= g_vInvFlowDims.y;
    else
        g_fScale = 1.0f;
    return g_fScale * tex2D( TextureSampler0, tc );
} 


//--------------------------------------------------------------------------------------
// Name: FlowDivergencePS()
// Desc: Compute divergence of velocity field
//--------------------------------------------------------------------------------------
float4 FlowDivergencePS( float2 TexCoord0 : TEXCOORD0 ) : COLOR
{
    float2 tc = TexCoord0;

    float4 vL = tex2D( TextureSampler0, float2( tc.x - g_vInvFlowDims.x, tc.y ) );
    float4 vR = tex2D( TextureSampler0, float2( tc.x + g_vInvFlowDims.x, tc.y ) );
    float4 vB = tex2D( TextureSampler0, float2( tc.x,                    tc.y - g_vInvFlowDims.y ) );
    float4 vT = tex2D( TextureSampler0, float2( tc.x,                    tc.y + g_vInvFlowDims.y ) );
       
    return 0.5f * ( (vR.x - vL.x) + (vT.y - vB.y) );
} 


//--------------------------------------------------------------------------------------
// Name: FlowJacobiPS()
// Desc: Perform Jacobi integration
//--------------------------------------------------------------------------------------
float4 FlowJacobiPS( float2 TexCoord0 : TEXCOORD0 ) : COLOR
{
    float2 tc = TexCoord0;
    float4 vBCenter = tex2D( TextureSampler1, tc );

    float4 xL = tex2D( TextureSampler0, float2( tc.x - g_vInvFlowDims.x, tc.y ) );
    float4 xR = tex2D( TextureSampler0, float2( tc.x + g_vInvFlowDims.x, tc.y ) );
    float4 xB = tex2D( TextureSampler0, float2( tc.x,                    tc.y - g_vInvFlowDims.y ) );
    float4 xT = tex2D( TextureSampler0, float2( tc.x,                    tc.y + g_vInvFlowDims.y ) );
      
    return ( xL + xR + xB + xT + g_fCenterFactor * vBCenter ) * g_fStencilFactor;
} 


//--------------------------------------------------------------------------------------
// Name: FlowSubtractGradientPS()
// Desc: Subtract gradient of pressure from velocity
//--------------------------------------------------------------------------------------
float4 FlowSubtractGradientPS( float2 TexCoord0 : TEXCOORD0 ) : COLOR
{
    float2 tc = TexCoord0;

    float pL = tex2D( TextureSampler0, float2( tc.x - g_vInvFlowDims.x, tc.y ) ).x;
    float pR = tex2D( TextureSampler0, float2( tc.x + g_vInvFlowDims.x, tc.y ) ).x;
    float pB = tex2D( TextureSampler0, float2( tc.x,                    tc.y - g_vInvFlowDims.y ) ).x;
    float pT = tex2D( TextureSampler0, float2( tc.x,                    tc.y + g_vInvFlowDims.y ) ).x;

    float2 grad = float2( pR - pL, pT - pB ) * 0.5;

    float4 vOutput = tex2D( TextureSampler1, tc );
    vOutput.x -= ( pR - pL ) * 0.5;
    vOutput.y -= ( pT - pB ) * 0.5;
    return vOutput;
} 


//--------------------------------------------------------------------------------------
// Name: FlowDisplayPS()
// Desc: 
//--------------------------------------------------------------------------------------
float4 FlowDisplayPS( float2 TexCoord0 : TEXCOORD0 ) : COLOR
{
//  return tex2Dl( TextureSampler0, TexCoord0 );
    return tex2D( TextureSampler0, TexCoord0 );
} 


//--------------------------------------------------------------------------------------
// Name: ParticleInitPS()
// Desc: Initialize particle positions 
//--------------------------------------------------------------------------------------
float4 ParticleInitPS( float2 TexCoord0 : TEXCOORD0 ) : COLOR
{
    // Scale texcoords based on how many particles we want to be visible in the
    // simulation (they stay in [-0.5f..+0.5f] range)
    TexCoord0 = (TexCoord0-0.5f)*256.0f / g_fRootVisParticles + 0.5f / g_fRootVisParticles;
    float2 tc = abs(TexCoord0);
    if( max( tc.x, tc.y ) > 0.5f )
    {
        // Set particles outside visible range to -8
        return -8.0f;
    }
    else
    {
        return float4( TexCoord0+0.5f, 0, 0 );
    }
} 


//--------------------------------------------------------------------------------------
// Name: ParticleAdvectPS()
// Desc: Advect particles based on velocity
//       (TextureSampler0 = velocity; TextureSampler1 = particle)
//--------------------------------------------------------------------------------------
float4 ParticleAdvectPS( float2 TexCoord0 : TEXCOORD0 ) : COLOR
{
    float2 vPos = tex2D( TextureSampler1, TexCoord0 ).xy;
    if( vPos.x == -8.0f )
    {
        // Ignore particles outside visible range
        return -8.0f;
    }
    else
    {
        vPos = saturate( vPos );
        float2 vPosB = float2( vPos.x, 1.0f-vPos.y );
        float2 vVel  = tex2D( TextureSampler0, vPosB ).xy;
        vVel.y = -vVel.y;
        vPos += g_vInvFlowDims * g_fTimestep * vVel;
        return float4( vPos, 0, 0 );
    }
} 


//--------------------------------------------------------------------------------------
// Name: repel()
// Desc: Returns amount by which pos is repeled by p
//--------------------------------------------------------------------------------------
float2 repel( float2 pos, float2 p )
{
    float2 v = pos - (p);
    float dist = length(v);
    if( dist < 0.001f )
    {
        // They are the same particle, don't repel itself
        return 0.0f;
    }
    else
    {
        // Repel particle by a small fixed amount
        return v / dist * 0.001f;
    }
}


//--------------------------------------------------------------------------------------
// Name: ParticleRepelPS()
// Desc: Repel a nearby particle
//       (TextureSampler0=particlerepel; TextureSampler1=particle)
//--------------------------------------------------------------------------------------
float4 ParticleRepelPS( float2 TexCoord0 : TEXCOORD0 ) : COLOR
{
    float2 pos = tex2D( TextureSampler1, TexCoord0 ).xy;
    if( pos.x == -8.0f )
    {
        // Ignore particles outside visible range
        return -8.0f;
    }
    else
    {
        float2 posb = float2( pos.x, 1.-pos.y );
        pos += repel( pos, tex2D( TextureSampler0, float2( posb.x, posb.y ) ).xy );
        return float4( pos, 0, 0 );
    }
} 

struct VSRENDEROUT
{
    float4 Position  : POSITION;
    float  PSize     : PSIZE;
    float4 Color     : COLOR0;
};


//--------------------------------------------------------------------------------------
// Name: ParticleRepelRenderVS()
// Desc: Render particle positions to repel buffer
//--------------------------------------------------------------------------------------
VSRENDEROUT ParticleRepelRenderVS( const float2 PositionXY : POSITION0 )
{
    VSRENDEROUT Output;

    // Transform position to the clipping space 
    float2 tc = ((PositionXY.xy-.5)*2.);
    Output.Position = float4( tc, 0.0f, 1.0f );
    Output.PSize    = 3.0f;
    Output.Color    = float4( PositionXY.xy, 0, 1 );
    return Output;
}


//--------------------------------------------------------------------------------------
// Name: ParticleRepelRenderPS()
// Desc: 
//--------------------------------------------------------------------------------------
float4 ParticleRepelRenderPS( float4 Color     : COLOR0 ) : COLOR
{
    return Color;
}


//--------------------------------------------------------------------------------------
// Name: ParticleRenderVS()
// Desc: Render particles to screen
//--------------------------------------------------------------------------------------
VSRENDEROUT ParticleRenderVS( float2 PositionXY : POSITION0 )
{
    VSRENDEROUT Output;

    // Transform position to the clipping space 
    float2 tc = ((PositionXY.xy-.5)*2.);
    Output.Position = float4( tc, 0.0f, 1.0f );
    Output.PSize    = 8.0f;
    Output.Color    = 1.0f;
    return Output;
}


//--------------------------------------------------------------------------------------
// Name: ParticleRenderPS()
// Desc: 
//--------------------------------------------------------------------------------------
float4 ParticleRenderPS( float2 TexCoord : SPRITETEXCOORD,
                         float4 Color    : COLOR0 ) : COLOR
{
   return ( 1.0f - length( TexCoord - 0.5f ) * 2.0f ) * Color;
}


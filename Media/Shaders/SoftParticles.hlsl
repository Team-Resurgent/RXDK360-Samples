//-----------------------------------------------------------------------------
// Shader for the SoftParticles sample
//-----------------------------------------------------------------------------

struct VSOUT
{
    float4 vPosition         : POSITION;
    float3 vDiffuse          : COLOR0;
    float2 vTexCoord0        : TEXCOORD0;
    float4 vDepth            : TEXCOORD1;
};


//-----------------------------------------------------------------------------
// Vertex shader constants
//-----------------------------------------------------------------------------
uniform float4x4 matWorldViewProj  : register(c0);  // World-view-projection matrix
uniform float3   vLightDirection   : register(c4);  // Light direction


//-----------------------------------------------------------------------------
// Pixel shader constants
//-----------------------------------------------------------------------------
sampler2D Sampler0 : register(s0);
sampler2D Sampler1 : register(s1);

uniform float4   vAmbient           : register(c0);   // Ambient light color
uniform float4   vParticleSoftness  : register(c1);   // .x contains particle softness
uniform float4x4 matInvProj         : register(c2);   // Inverse projection matrix


//-----------------------------------------------------------------------------
// Name: SceneVS()
// Desc: Vertex shader to shade all meshes
//-----------------------------------------------------------------------------
VSOUT SceneVS( const float3 vPosition  : POSITION,
               const float3 vNormal    : NORMAL,
               const float2 vTexCoords : TEXCOORD0 )
{
    VSOUT  Output;

    // Transform to world view projection space
    Output.vPosition = mul( float4( vPosition, 1.0f ), matWorldViewProj );

    // Lighting calculation
    Output.vDiffuse = max( dot( vNormal, vLightDirection ), 0.0f );

    // Copy tex coords
    Output.vTexCoord0 = vTexCoords;
    
    // Calculate depth
    Output.vDepth.xy = Output.vPosition.xy / Output.vPosition.w;
    Output.vDepth.zw = Output.vPosition.zw;

    return Output;
}



//-----------------------------------------------------------------------------
// Name: ScenePS()
// Desc: Pixel shader that is used for all the objects in the scene.
//-----------------------------------------------------------------------------
float4 ScenePS( float3 vDiffuse : COLOR0,
                float2 vTexCoord0 : TEXCOORD0,
                [unused] float4 vDepth : TEXCOORD1 ) : COLOR
{   
    return tex2D( Sampler0, vTexCoord0 ) * float4( vDiffuse + vAmbient, 1.0f );
}


//-----------------------------------------------------------------------------
// Name: SkyPS()
// Desc: Pixel shader that is used to render the sky dome.
//-----------------------------------------------------------------------------
float4 SkyPS( [unused] float3 vDiffuse : COLOR0,
              float2 vTexCoord0 : TEXCOORD0,
              [unused] float4 vDepth : TEXCOORD1 ) : COLOR
{
    return tex2D( Sampler0, vTexCoord0 );
}


//-----------------------------------------------------------------------------
// Name: ParticlePS()
// Desc: Pixel shader that draws the smoothed particle.
//-----------------------------------------------------------------------------
float4 ParticlePS( [unused] float3 vDiffuse : COLOR0,
                   float2 vTexCoord0 : TEXCOORD0,
                   float4 vDepth : TEXCOORD1 ) : COLOR
{   
    float4 vPixelColor = tex2D( Sampler0, vTexCoord0 ); 
    
    // Generate alpha from the green chanel
    vPixelColor.a = vPixelColor.g * 4.0f;
    
    // Fetch from the Z buffer at a half pixel offset in order to avoid edge artifacts
    float4 vZBuffer;
    float2 vDepthTexCoord = 0.5f * vDepth.xy + 0.5f;
    vDepthTexCoord.y = 1.0f - vDepthTexCoord.y;
    asm {
        tfetch2D vZBuffer.x___, vDepthTexCoord, Sampler1, OffsetX = 0.5, OffsetY = 0.5
    };
    
    // Use the inverted projection matrix to retrieve the screen space Z, this is required
    // get a linear depth difference value.
    float4 vZBufferDepth = mul( float4( vDepth.xy, vZBuffer.x, 1.0f ), matInvProj );
    float4 vParticleDepth = mul( float4( vDepth.xy, vDepth.z/vDepth.w, 1.0f ), matInvProj );

    float fZDifference = ( vZBufferDepth.z/vZBufferDepth.w - vParticleDepth.z/vParticleDepth.w ) * vParticleSoftness.x;
    
    vPixelColor.a *= clamp( fZDifference, 0.0f, 1.0f );

    return vPixelColor;
}



//--------------------------------------------------------------------------------------
// Vertex shader to perform cartoon-style shading
//--------------------------------------------------------------------------------------


//--------------------------------------------------------------------------------------
// Vertex shader constants
//--------------------------------------------------------------------------------------
uniform float3   LightDirection  : register(c0);  // Light direction
uniform float    ExtrusionFactor : register(c1);  // Sihlouette scale
uniform float4x4 World           : register(c4);  // World-view matrix
uniform float4x4 WorldViewProj   : register(c8);  // World-view-projection matrix


//--------------------------------------------------------------------------------------
// Pixel shader constants
//--------------------------------------------------------------------------------------
uniform float4 Diffuse : register(c0);  // Diffuse color

sampler TextureSampler : register(s0);


//--------------------------------------------------------------------------------------
// Vertex structures
//--------------------------------------------------------------------------------------
struct VSOUT_CARTOON
{
    float4 Position         : POSITION;
    float2 TexCoord0        : TEXCOORD0;
};
struct VSOUT_OUTLINE
{
    float4 Position         : POSITION;
};


//--------------------------------------------------------------------------------------
// Name: ShadeCartoonVertex()
// Desc: Vertex shader to perform cartoon-style shading
//--------------------------------------------------------------------------------------
VSOUT_CARTOON ShadeCartoonVertex( const float3 Position  : POSITION,
                                  const float3 Normal    : NORMAL )
{
    VSOUT_CARTOON  Output;
    float3         ViewSpaceNormal;
    float3         ExtrudedPosition;

    ExtrudedPosition = Position + ExtrusionFactor * Normal;

    // Transform the vertex
    Output.Position = mul( float4(ExtrudedPosition, 1.0f), WorldViewProj );

    // Transform the normal into view space
    ViewSpaceNormal = mul( Normal, World );

    // tu = vNormal dot vLight
    Output.TexCoord0.x = dot( ViewSpaceNormal, LightDirection );
    Output.TexCoord0.y = 0.0f;
    
    return Output;
}


//--------------------------------------------------------------------------------------
// Name: ShadeOutlineVertex()
// Desc: Vertex shader to perform the outline for cartoon-style shading
//--------------------------------------------------------------------------------------
VSOUT_OUTLINE ShadeOutlineVertex( const float3 Position : POSITION )
{
    VSOUT_OUTLINE Output;

    // Transform the vertex
    Output.Position = mul( float4(Position, 1.0f), WorldViewProj );

    return Output;
}


//--------------------------------------------------------------------------------------
// Name: ShadeCartoonPixel()
// Desc: Pixel shader to perform cartoon-style shading
//--------------------------------------------------------------------------------------
float4 ShadeCartoonPixel( float2 TexCoord0 : TEXCOORD0 ) : COLOR
{
    // Fetch a texel from the cartoon texture
    float4 TexelColor = tex2D( TextureSampler, TexCoord0 );
    
    // Modulate the result with a solid color specified by the app
    return TexelColor * Diffuse;
}


//--------------------------------------------------------------------------------------
// Name: ShadeOutlinePixel()
// Desc: Pixel shader to perform the outline for cartoon-style shading
//--------------------------------------------------------------------------------------
float4 ShadeOutlinePixel() : COLOR
{
    return float4( 0.0f, 0.0f, 0.0f, 1.0f );
}


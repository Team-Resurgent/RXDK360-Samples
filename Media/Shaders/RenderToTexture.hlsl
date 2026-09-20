//------------------------------------------------------------------------------
// Shaders for the RenderToTexture sample
//------------------------------------------------------------------------------


//------------------------------------------------------------------------------
// Vertex shader output
//------------------------------------------------------------------------------
struct VSOUT
{
    float4 Position         : POSITION;
    float2 TexCoord0        : TEXCOORD0;    
};


//------------------------------------------------------------------------------
// Vertex shader constants
//------------------------------------------------------------------------------
uniform float3   g_LightDirection  : register(c0);  // Light direction
uniform float    g_ExtrusionFactor : register(c1);  // Sihlouette scale
uniform float4x4 g_World           : register(c4);  // World-view matrix
uniform float4x4 g_WorldViewProj   : register(c8);  // World-view-projection matrix


//------------------------------------------------------------------------------
// Pixel shader constants
//------------------------------------------------------------------------------
uniform float4 g_Diffuse : register(c0);  // Diffuse color

sampler g_TextureSampler : register(s0);




//------------------------------------------------------------------------------
// Name: ShadeMeshVertex()
// Desc: Simple vertex shader
//------------------------------------------------------------------------------
VSOUT ShadeMeshVertex( float3 Position  : POSITION,
                       float3 Normal    : NORMAL )
{
    VSOUT  Output;
    float3 ViewSpaceNormal;
    float3 ExtrudedPosition;

    ExtrudedPosition = Position + g_ExtrusionFactor * Normal;

    // Transform the vertex
    Output.Position = mul( float4(ExtrudedPosition, 1.0f), g_WorldViewProj );

    // Transform the normal into view space
    ViewSpaceNormal = mul( Normal, g_World );

    // tu = vNormal dot vLight
    Output.TexCoord0.x = dot( ViewSpaceNormal, g_LightDirection );
    Output.TexCoord0.y = 0;

    return Output;
}




//------------------------------------------------------------------------------
// Name: ShadeMeshPixel()
// Desc: Simple pixel shader
//------------------------------------------------------------------------------
float4 ShadeMeshPixel( VSOUT In ) : COLOR
{
    // Fetch a texel from the lighting texture
    return g_Diffuse * tex2D( g_TextureSampler, In.TexCoord0 );
}




//------------------------------------------------------------------------------
// Name: ShadeMirrorVertex()
// Desc: Simple vertex shader
//------------------------------------------------------------------------------
VSOUT ShadeMirrorVertex( float4 Position  : POSITION,
                         float2 TexCoord0 : TEXCOORD0 )
{
    VSOUT Output;
    Output.Position  = mul( Position, g_WorldViewProj );
    Output.TexCoord0 = TexCoord0;
    return Output;
}




//-----------------------------------------------------------------------------
// Name: ShadeMirrorPixel()
// Desc: Returns the result of texture lookup
//-----------------------------------------------------------------------------
float4 ShadeMirrorPixel( VSOUT In ) : COLOR
{
    return tex2D( g_TextureSampler, In.TexCoord0 );
}


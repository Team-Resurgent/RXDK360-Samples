//--------------------------------------------------------------------------------------
// Shader for the ZSprite sample
//--------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------
// Vertex shader constants
//--------------------------------------------------------------------------------------
uniform float4   Zero           : register(c0);  // ( 0, 0, 0, 0 )
uniform float4   Constants      : register(c1);  // ( 1, 0.5, -, - )
uniform float4x4 WorldViewProj  : register(c4);  // world-view-projection matrix
uniform float4x4 WorldView      : register(c8);  // world-view matrix
uniform float4x4 View           : register(c12); // view matrix
uniform float4x4 Projection     : register(c16); // projection matrix
uniform float3   LightDirection : register(c20); // Light direction
uniform float4   Diffuse        : register(c22); // material diffuse color * light diffuse color


//--------------------------------------------------------------------------------------
// Vertex structures
//--------------------------------------------------------------------------------------
struct VSOUT_TEAPOT
{
    float4 Position   : POSITION;
    float4 Diffuse    : COLOR0;
};

struct VSOUT_ZSPRITE
{
    float4 Position   : POSITION;
    float2 TexCoord0  : TEXCOORD0;
};


//--------------------------------------------------------------------------------------
// Name: TeapotVS()
// Desc: Vertex shader for rendering a teapot
//--------------------------------------------------------------------------------------
VSOUT_TEAPOT TeapotVS( float3 Position : POSITION,
                       float3 Normal   : NORMAL )
{
    VSOUT_TEAPOT Output;
    
    // Transform 
    Output.Position = mul( float4(Position, 1.0f), WorldViewProj );

    // Lighting calculation
    Output.Diffuse.xyz = max( dot( Normal, -LightDirection ), Zero.x ) * Diffuse;
    Output.Diffuse.w   = Diffuse.w;
    
    return Output;
}


//--------------------------------------------------------------------------------------
// Name: TeapotPS()
// Desc: Pixel shader for rendering a teapot
//--------------------------------------------------------------------------------------
float4 TeapotPS( float4 Diffuse : COLOR0 ) : COLOR0
{
    return Diffuse;
}


//--------------------------------------------------------------------------------------
// Name: ZSpriteVS()
// Desc: Pass-thru vertex shader for rendering a ZSprite
//--------------------------------------------------------------------------------------
VSOUT_ZSPRITE ZSpriteVS( float4 Position  : POSITION,
                         float2 TexCoords : TEXCOORD0 )
{
    VSOUT_ZSPRITE Output;
    Output.Position  = Position;
    Output.TexCoord0 = TexCoords;
    return Output;
}


//--------------------------------------------------------------------------------------
// Name: ZSpritePS()
// Desc: Pixel shader for rendering a z-sprite. This shader outputs both a color and a
//       depth value from the two source textures that define the z-sprite.
//--------------------------------------------------------------------------------------
sampler ZSpriteColorTexture : register(s0);
sampler ZSpriteDepthTexture : register(s1);

struct ZSPRITE_PIXEL
{
    float4 Color : COLOR0;
    float  Depth : DEPTH0;
};

ZSPRITE_PIXEL ZSpritePS( float2 TexCoord0 : TEXCOORD0 )
{
    ZSPRITE_PIXEL Out;
    Out.Color = tex2D( ZSpriteColorTexture, TexCoord0 ).rgba; // A8R8G8B8 color texture
    Out.Depth = tex2D( ZSpriteDepthTexture, TexCoord0 ).r;    // D24S8 depth texture
    return Out;
}


//--------------------------------------------------------------------------------------
// Name: QuadVS()
// Desc: Vertex shader for a simple quad
//--------------------------------------------------------------------------------------
float4 QuadVS( float3 Position : POSITION ) : POSITION
{
    // Transform 
    return mul( float4(Position, 1.0f), WorldViewProj );
}


//--------------------------------------------------------------------------------------
// Name: QuadPS()
// Desc: Pixel shader for a simple quad
//--------------------------------------------------------------------------------------
float4 QuadPS() : COLOR0
{
    // Transform 
    return float4( 0.0f, 0.8f, 0.0f, 1.0f );
}


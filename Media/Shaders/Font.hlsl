//-----------------------------------------------------------------------------
// Vertex shader for the CAtgFont class
//-----------------------------------------------------------------------------

struct VS_IN
{
    float2 Pos : POSITION;
    float2 Tex : TEXCOORD0;
};

struct VS_OUT
{
    float4 Position         : POSITION;
    float4 Diffuse          : COLOR0;
    float2 TexCoord0        : TEXCOORD0;
};


uniform float4   Color         : register(c0);  // ( r, g, b, a )


VS_OUT main( VS_IN In )
{
    VS_OUT Out;

    Out.Position.x =  ( In.Pos.x * 1.0/320.0 - 1.0 );
    Out.Position.y = -( In.Pos.y * 1.0/240.0 - 1.0 );
    Out.Position.z =  ( 0.0 );
    Out.Position.w =  ( 1.0 );

    // Lighting calculation
    Out.Diffuse = Color;

    // Texture coordinates
    Out.TexCoord0.x = In.Tex.x;
    Out.TexCoord0.y = In.Tex.y;

    return Out;
}


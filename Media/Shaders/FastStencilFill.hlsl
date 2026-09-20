//--------------------------------------------------------------------------------------
// Shader for the FastStencilFill sample
//--------------------------------------------------------------------------------------

sampler StencilPatternTexture : register( s0 );
float4 SolidColor : register( c0 );

struct VS_IN
{
    float2 Position: POSITION0;
    float2 TexCoord: TEXCOORD0;
};

struct VS_OUT
{
    float4 Position: POSITION;
    float2 TexCoord: TEXCOORD0;
};

struct PS_IN
{
    float2 TexCoord: TEXCOORD0;
};

VS_OUT PassthruVSMain( VS_IN In )
{
    VS_OUT Out;
    Out.Position = float4( In.Position, 0, 1 );
    Out.TexCoord = In.TexCoord;
    return Out;
}

float4 SolidPSMain( [unused] PS_IN In ) : COLOR
{
    return SolidColor;
}

float4 TexturedPSMain( PS_IN In ) : COLOR
{
    return tex2D( StencilPatternTexture, In.TexCoord ).r;
}

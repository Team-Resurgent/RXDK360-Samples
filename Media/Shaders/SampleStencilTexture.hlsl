//--------------------------------------------------------------------------------------
// Shader for the SampleStencilTexture sample
//--------------------------------------------------------------------------------------

sampler DepthStencilTexture : register( s0 );

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

float4 SampleStencilPSMain( PS_IN In ) : COLOR
{
    // Sample from the depth/stencil texture, which is an A8R8G8B8 texture.
    float4 DepthStencilSample = tex2D( DepthStencilTexture, In.TexCoord );
    
    // Stencil value is held in the blue channel.
    float StencilValue = DepthStencilSample.b;
    
    // Stencil value will be 1, 2, 3, or 4 divided by 256.
    // Multiply by 64 to get a large difference in brightness.
    float DisplayColor = StencilValue * 64.0f;
    return float4( DisplayColor.xxx, 1 );
}
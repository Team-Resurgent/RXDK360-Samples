struct PS_IN
{
    float2 uv0                  : TEXCOORD0;
};

sampler2D     s_colorTex       : register(s0);

float4 main( PS_IN input ) : COLOR
{
    float4 result = tex2D( s_colorTex, input.uv0 );
    result.w = result.x * 0.15f;
    
    return result;
}


//--------------------------------------------------------------------------------------
// Pixel shader constants
//--------------------------------------------------------------------------------------
uniform float   fTime   : register(c0);
uniform float   fHeight : register(c1);

//--------------------------------------------------------------------------------------
// Vertex shader output
//--------------------------------------------------------------------------------------
struct SinWave_VSOUT
{
    float4 vPosition    : POSITION;
    float4 vPos         : COLOR0;
};

//-----------------------------------------------------------------------------
// Name: SinWave_VS()
// Desc: Vertex shader for rendering quad with sin wave
//-----------------------------------------------------------------------------
SinWave_VSOUT SinWaveVS( const float3 vPosition : POSITION )
{
    SinWave_VSOUT  Output;

    Output.vPosition = float4( vPosition.xy, 0.0f , 1.0f );
    Output.vPos = Output.vPosition;
    
    return Output;
}

//-----------------------------------------------------------------------------
// Name: SinWave_PS()
// Desc: Pixel shader for rendering a quad with sin wave
//-----------------------------------------------------------------------------
float4 SinWavePS( float4 vPos : COLOR0 ) : COLOR
{   
    float x = vPos.x + fTime;
    float y = ( ( vPos.y - fHeight ) / 0.25f ) * 2.0f - 1.0f;
    float4 vColor0 = float4( 1.0f, 1.0f, 1.0f, 1.0f );
    float4 vColor1 = float4( 0.25f, 0.25f, 0.25f, 0.5f );
    float w = 0.02f;
    float f1 = sin( x * 5.0f ) * 0.9f + 0.1f;
    float f2 = sin( x * 5.0f + 0.2f ) * 0.9f - 0.1f;
    float fValue = smoothstep( y - w, y + w, f1 ) * smoothstep( f2 - w, f2 + w, y );
    float4 vColor = lerp( vColor1, vColor0, fValue );
    return vColor;
}

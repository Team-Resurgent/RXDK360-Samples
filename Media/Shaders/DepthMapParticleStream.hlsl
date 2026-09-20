//-----------------------------------------------------------------------------
// Shader for the DepthMapParticleStream sample
//-----------------------------------------------------------------------------

sampler s0: register(s0);
sampler s1: register(s1);
sampler s2: register(s2);
sampler s3: register(s3);
sampler s4: register(s4);
sampler s5: register(s5);
sampler s6: register(s6);
sampler s7: register(s7);
sampler s8: register(s8);
sampler s9: register(s9);

float4 GetSegmentationFromDepthTexturePS( in float2 vTexCoord : TEXCOORD0 ) : COLOR
{
    // Extract player ID from the depth image
    float value = tex2D( s0, vTexCoord ).x;
    value /= 8;
    value = frac( value );
    value *= 8;

    // Test whether this pixel belongs to any player
    if ( (int)value != 0 )
        return float4( 0.5, 0, 0, 1 );    // value == 1 means this pixel belongs to player1
                                          // similarly, value == 2 means this pixel belongs to player2
                                          // value == 0 means this pixel is non-player pixel

    return float4( 0, 0, 0, 1 );
}

static const float Gx[3][3] =
{
    { -1, 0, +1, },
    { -2, 0, +2, },
    { -1, 0, +1, },
};

static const float Gy[3][3] =
{
    { +1, +2, +1, },
    {  0,  0,  0, },
    { -1, -2, -1, },
};

float4 EdgeDetectPS( in float2 vTexCoord : TEXCOORD0 ) : COLOR
{
    float4 t0, t1, t2;
    float4 v = 0;
    float4 h = 0;

    for ( int i = 0; i < 3; ++i )
    {
        int o = i - 1;
        asm
        {
            tfetch2D t0, vTexCoord, s0, OffsetX = -1, OffsetY = o
            tfetch2D t1, vTexCoord, s0, OffsetX =  0, OffsetY = o
            tfetch2D t2, vTexCoord, s0, OffsetX =  1, OffsetY = o
        };

        v += Gx[i][0] * t0 + Gx[i][1] * t1 + Gx[i][2] * t2; 
        h += Gy[i][0] * t0 + Gy[i][1] * t1 + Gy[i][2] * t2;   
    }    

    float4 r = abs( v ) + abs( h );    // Edge intensity 
    float3 n = {v.x, h.x, 0};
    n = normalize( n );
    n.xy = (n.xy + 1.0f) / 2.0f;       // Normal of the edge 

    return float4( r.x, n.x, n.y, 1 );
}

// Blend the textures in history nicely to get a trails rendering effect
float4 RenderTrailPS( in float2 vTexCoord: TEXCOORD0 ) : COLOR
{
    float4 value0 = tex2D( s0, vTexCoord );
    float4 value1 = tex2D( s1, vTexCoord );
    float4 value2 = tex2D( s2, vTexCoord );
    float4 value3 = tex2D( s3, vTexCoord );
    float4 value4 = tex2D( s4, vTexCoord );
    float4 value5 = tex2D( s5, vTexCoord );
    float4 value6 = tex2D( s6, vTexCoord );
    float4 value7 = tex2D( s7, vTexCoord );
    float4 value8 = tex2D( s8, vTexCoord );
    float4 value9 = tex2D( s9, vTexCoord );

    float ratio = min( value9.x, 1 );

    float4 color = 0;
    float factor = 1 / 8.0f;
    float4 trailcolor = float4(1.0f, 0.8f, 0.5f, 1.0f);

    color += trailcolor * value0.xxxw * factor * (0.1 + 0.2*8);
    color += trailcolor * value1.xxxw * factor * (0.1 + 0.2*7);
    color += trailcolor * value2.xxxw * factor * (0.1 + 0.2*6);
    color += trailcolor * value3.xxxw * factor * (0.1 + 0.2*5);
    color += trailcolor * value4.xxxw * factor * (0.1 + 0.2*4);
    color += trailcolor * value5.xxxw * factor * (0.1 + 0.2*3);
    color += trailcolor * value6.xxxw * factor * (0.1 + 0.2*2);
    color += trailcolor * value7.xxxw * factor * (0.1 + 0.2*1);
    color += trailcolor * value8.xxxw * factor * (0.1 + 0.2*0);

    color = float4(1, 0, 0, 1) * ratio  + color * (1 - ratio);
    return float4( color.xyz, 1) ;
}

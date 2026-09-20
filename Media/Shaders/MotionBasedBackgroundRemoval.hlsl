//-----------------------------------------------------------------------------
// Shaders for the MotionBasedBackgroundRemoval sample
//-----------------------------------------------------------------------------

sampler s0: register(s0);
sampler s1: register(s1);
sampler s2: register(s2);

static const int DepthDelta = 200;

float4 DepthPlayerRemovalPS( in float2 vTexCoord : TEXCOORD0 ) : COLOR
{
    float fCurrentDepth = tex2D( s0, vTexCoord ).x / 8;
    float fBackgroundDepth = tex2D( s1, vTexCoord ).x / 8;

    if ( (int)fCurrentDepth > 0 )
    {
        // Only keeps pixels whose depth become larger than a threshold
        if( ((int)fBackgroundDepth - (int)fCurrentDepth) < DepthDelta )
        {
            if( (int)fBackgroundDepth > 0 )
            {                                
                return (fCurrentDepth * 0.78 + fBackgroundDepth * (1 - 0.78)) * 8;
            }
            else
            {
                return fCurrentDepth * 8;
            }
        }
    }
        
    return fBackgroundDepth * 8;
}

float4 UpdateDepthPlayerPS( in float2 vTexCoord : TEXCOORD0 ) : COLOR
{
    float fCurrentDepth = tex2D( s0, vTexCoord ).x / 8;
    float fBackgroundDepth = tex2D( s1, vTexCoord ).x / 8;

    // If the depth difference between current pixel and its background is larger than a threshold,
    // we identify this pixel as a player pixel
    if ( (int)fCurrentDepth > 0 && (int)fBackgroundDepth > 0 && 
         ((int)fBackgroundDepth - (int)fCurrentDepth) > DepthDelta )
    {
        return fCurrentDepth * 8;
    }

    return 0;    
}

float4 UpdateColorBackgroundPS( in float2 vTexCoord : TEXCOORD0 ) : COLOR
{
    float s = 0;

    // Sample around current pixel location on the player map, 
    for ( int i = -7; i <= 7; i += 2 )
        for ( int j = -7; j <= 7; j += 2 )  
        {
            float4 t;
            asm
            {
                tfetch2D t, vTexCoord, s0, OffsetX = i, OffsetY = j                
            };
            s += t.x;
        }
    
    // if no player pixel is adjacent to this pixel, this is background area,
    // so it is safe to grab the pixel from current color image
    if ( s == 0 )
        return tex2D( s1, vTexCoord );

    // Otherwise, just leave this pixel unchanged 
    // (which should also a background pixel as we only grab background pixels from the color image)
    return tex2D( s2, vTexCoord );
}

float4 UpdateColorPlayerPS( in float2 vTexCoord : TEXCOORD0 ) : COLOR
{
    // Apply the player mask onto the color image
    return tex2D( s1, vTexCoord ) * (1 - step( tex2D( s0, vTexCoord ).x, 0 ));
}

float4 BlendColorPlayerBackgroundPS( in float2 vTexCoord : TEXCOORD0 ) : COLOR
{
    // s0 player mask
    // s1 perlin noise
    // s2 background

    float player = tex2D( s0, vTexCoord ).x/65536.0f;
    float4 back = tex2D( s2, vTexCoord );
    float noise = tex2D( s1, vTexCoord ).x;
    back.g *= (player*10 + 1);
    if ( player > 0 )            
        back.gb *= 0.5 * tex2D( s1, vTexCoord ).x;

    return back;
}

float4 PlayerMaskPS( in float2 vTexCoord : TEXCOORD0 ) : COLOR
{
    // To get a great heat distortion effect, we need a binary player mask    
    float player = tex2D( s0, vTexCoord ).x;
    return 1 - step( player, 0 );
}

uniform float time_offset : register(c0);

float4 HeatDistortionPS( in float2 vTexCoord : TEXCOORD0 ) : COLOR
{
    // s0 player mask
    // s1 perlin noise
    // s2 background
    
    float mask = tex2D( s0, vTexCoord );
    float noise = tex2D( s1, (vTexCoord*2 + float2(time_offset, time_offset)) ) * mask * 60;
    
    // Offset the sampling position a little by the time-shifted perlin noise value
    return tex2D( s2, vTexCoord - float2(noise/640.0f, noise/480.0f) ) * (mask/20 + 1);
}

float4 DisplayF16PS( in float2 vTexCoord : TEXCOORD0 ) : COLOR
{
    return tex2D( s0, vTexCoord ).r / 65536.0f;
}


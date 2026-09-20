
//------------------------------------------------------------------------------
// Vertex shader to pass thru vertex components
//------------------------------------------------------------------------------


struct VSOUT
{
    float4 Position         : POSITION;
    float2 TexCoord0        : TEXCOORD0;    
};


float4  scaleOfs : register( c0 );

VSOUT DefaultVS(    const float4 Position  : POSITION,
                    const float2 TexCoord0 : TEXCOORD0 )
{
    VSOUT  Output;

    // Transform the vertex
    Output.Position.xy = Position.xy * scaleOfs.zw + scaleOfs.xy;
    Output.Position.zw = Position.zw;
    Output.TexCoord0 = TexCoord0;

    return Output;
}


sampler2D s0 : register( s0 );
sampler2D s1 : register( s1 );
float4  numMips : register( c0 );

float4 GenericPS( const float2 TexCoord0 : TEXCOORD0 ) : COLOR
{
    float4 Color;
    
    [branch]
    if( numMips.y < 1 )
    {
        float4  tc = float4( TexCoord0.xy, 0, TexCoord0.y * numMips.x );
        Color = tex2Dlod( s0, tc );
    } else
    {
        if( TexCoord0.x >= 0.5f )
        {
            float4  tc = float4( 2 * (TexCoord0.x - 0.5f), frac( TexCoord0.y * numMips.y ), 0, TexCoord0.y * numMips.y );
            Color = tex2Dlod( s1, tc );
        } else
        {
            float4  tc = float4( 2 * TexCoord0.x, frac( TexCoord0.y * numMips.x ), 0, TexCoord0.y * numMips.x );
            Color = tex2Dlod( s0, tc );
        }
    }

    return Color;
}


float4  control : register( c0 );
float4  freeAreas[ 32 ] : register( c32 );
int    counter : register( i0 );

float4 MemViewPS( const float2 TexCoord0 : TEXCOORD0 ) : COLOR
{
    float4 Color;
    
    Color = tex2D( s0, TexCoord0.xy );
    
    int     texelX = round( TexCoord0.x * (control.x - 1) );
    int     texelY = round( TexCoord0.y * (control.y - 1) );
    
    int     texelIndex = texelX + texelY * (int)control.x;
    
    for( int i = 0; i < counter; ++i )
    {
        if( texelIndex >= (int)freeAreas[ i ].x   &&
            texelIndex < (int)freeAreas[ i ].y )
        {
            Color = lerp( Color, float4( 0, 1, 0, 1 ), control.z );
            break;
        }
    }

    return Color;
}

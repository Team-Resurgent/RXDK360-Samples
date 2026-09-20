
// this is needed for including the C++ header <texSize.h> into an HLSL file
#ifndef INT
#define INT int
#endif

#include "texSize.h"

float4x4 matWVP : register(c0);

float4  texSizes0 : register( PASTE( c, REG_TEX_SIZES_0 ) );
// x - RT_SX = TEX_SX / 4
// y - TEX_SY
// z - TEX_SY + ((VERT_TILE_REPEAT - (TEX_SY % VERT_TILE_REPEAT)) % VERT_TILE_REPEAT);       // align to VERT_TILE_REPEAT
// w - (TEX_SX + ((HORZ_32_TO_8_TILE_REPEAT - (TEX_SX % HORZ_32_TO_8_TILE_REPEAT)) % HORZ_32_TO_8_TILE_REPEAT)) / 4;       // align to HORZ_32_TO_8_TILE_REPEAT

float4  texSizes1 : register( PASTE( c, REG_TEX_SIZES_1 ) );
// x - TEX_SX
// y - TEX_SY
// z - TEX_RESCALE

// the below is for simplicity
#define RT_SX texSizes0.x
#define RT_SY texSizes0.y
#define RT_SY_A texSizes0.z
#define RT_SX_A texSizes0.w
#define TEX_SX texSizes1.x
#define TEX_SY texSizes1.y
#define TEX_RESCALE texSizes1.z

static const float2 REMAP_SIZE_8_TO_32 = float2( HORZ_8_TO_32_TILE_REPEAT, VERT_TILE_REPEAT );
static const float2 REMAP_SIZE_32_TO_8 = float2( HORZ_32_TO_8_TILE_REPEAT, VERT_TILE_REPEAT );
sampler s0 : register( s0 );
sampler s1 : register( s1 );
  
struct VS_IN                                 
{
    float4 ObjPos   : POSITION;
    float4 Color    : COLOR;
};

struct VS_OUT
{
    float4 ProjPos  : POSITION;
    float4 Color    : TEXCOORD0;
};


VS_OUT vs( VS_IN In )
{
    VS_OUT Out;
    Out.ProjPos = mul( matWVP, In.ObjPos );
    Out.Color = In.Color;
    return Out;
}                                            

// this renders a simple grid of pixels
float4 ps( float4 c : TEXCOORD0, float2 v : VPOS ) : COLOR  
{               
    FLOAT m = ( 0 == (v.y % 10)) ? 0 : 1;
    FLOAT n = ( 0 == (v.x % 10)) ? 0 : 1;
    return c * m * n;
}

// use your weight values here, this is a pyramid filter at the moment
static const FLOAT w9[ 9 ]  = {  1.f/25.f, 2.f/25.f, 3.f/25.f, 4.f/25.f,
                                 5.f/25.f,
                                 4.f/25.f, 3.f/25.f, 2.f/25.f, 1.f/25.f };

// when using bilinear taps to optimise filtering we can make 5 tfetches instead of 9
// but we need to calculate new weights (wt) and subpixel offsets (wo). the 'ww' array below
// is an intermediate array of weights between each pixel in the filter that is used to calculate
// the offsets (wo)
static const FLOAT ww9[ 5 ] = {  w9[ 1 ] / (w9[ 0 ] + w9[ 1 ]), w9[ 3 ] / (w9[ 2 ] + w9[ 3 ]),
                                 w9[ 4 ],
                                 w9[ 6 ] / (w9[ 5 ] + w9[ 6 ]), w9[ 8 ] / (w9[ 7 ] + w9[ 8 ]) };
                                
static const FLOAT wo9[ 5 ] = {  lerp( -3.5, -2.5, ww9[ 0 ] ), lerp( -1.5, -0.5, ww9[ 1 ] ),
                                 0.5f,
                                 lerp( 1.5, 2.5, ww9[ 3 ] ), lerp( 3.5, 4.5, ww9[ 4 ] ) };
                                    
static const FLOAT wt9[ 5 ] = {  (w9[ 0 ] + w9[ 1 ]), (w9[ 2 ] + w9[ 3 ]),
                                 w9[ 4 ],
                                 (w9[ 5 ] + w9[ 6 ]), (w9[ 7 ] + w9[ 8 ]) };

// this is a similar array for a 5 tap filter, which can be done with 3 tfetches
static const FLOAT w5[ 5 ]  = {  1.f/9.f, 2.f/9.f, 3.f/9.f, 2.f/9.f, 1.f/9.f };
                                                                
static const FLOAT ww5[ 3 ] = {  w5[ 1 ] / (w5[ 0 ] + w5[ 1 ]),
                                 w5[ 2 ],
                                 w5[ 4 ] / (w5[ 3 ] + w5[ 4 ]) };
                                
static const FLOAT wo5[ 3 ] = {  lerp( -1.5, -0.5, ww5[ 0 ] ),
                                 0.5f,
                                 lerp( 1.5, 2.5, ww5[ 2 ] ) };
                                    
static const FLOAT wt5[ 3 ] = {  (w5[ 0 ] + w5[ 1 ]),
                                  w5[ 2 ],
                                 (w5[ 3 ] + w5[ 4 ]) };
                                 



// this just does a remapping from a normal UV coordinate into an 8 bit texture aliased as a 32bit texture
// See RetileOutput for the inverse transform
float4 SampleInput( float2 tc )
{
    float4 pixels;
    float2 tcc = frac( tc / REMAP_SIZE_8_TO_32 ) * REMAP_SIZE_8_TO_32;  // less alu than a %
    float2 addr;
    asm { tfetch2D addr.wx, s1, tcc,  UnnormalizedTextureCoords = true, OffsetX = 0.5f, OffsetY = 0.5 };
    addr += floor( tc / REMAP_SIZE_8_TO_32 ) * REMAP_SIZE_8_TO_32;
    asm { tfetch2D pixels.wxyz,  s0, addr, UnnormalizedTextureCoords = true, OffsetX = 0.5f, OffsetY = 0.5 };    // swizzle for ABGR
    return pixels;                           
}

// this places the four 8 bit values (stored in one float4, of course) into the right place in the
// output 32bpp texture aliased as an 8 bit texture. This is simply an inverse transform
// to SampleInput. Note that it's possible to use this on its own.
void RetileOutput( out FLOAT x, out FLOAT y, float2 vpos )
{
#if 1
    // so now this should go through our lookup table to re-tile for L8
    float2 posInTile = vpos / REMAP_SIZE_32_TO_8;
    float2 tcc = frac( posInTile ) * REMAP_SIZE_32_TO_8;
    float2 addr;
    asm { tfetch2D addr.wx, s1, tcc,  UnnormalizedTextureCoords = true, OffsetX = 0.5f, OffsetY = 0.5 };
    addr += floor( posInTile ) * REMAP_SIZE_32_TO_8;
    
    x = addr.x + 0.5f;
    y = addr.y;
#else
    // useful for debugging in PIX -- removes output swizzle
    x = vpos.x + 0.5f;
    y = vpos.y;
#endif    
}

// VPOS helps here because the shader is not interpolator bound and we save ALU if we don't use "round" on the texture coordinate
float4 psHorz( float2 vp : VPOS ) : COLOR
{  
    float4  tc;
    
    tc.x = vp.y;
    tc.yzw = float3( vp.x - 1, vp.x, vp.x + 1 );

    // manual clamp because sampling outside the texture's boundaries means
    // when we remap it into tiled mode the tfetch may end up anywhere
    // possibly sampling garbage
    float4    sc = SampleInput( tc.zx );
    float4    sr = ( tc.w < RT_SX ) ? SampleInput( tc.wx ) : sc;  
    float4    sl = ( tc.y >= 0 )    ? SampleInput( tc.yx ) : sc;

    FLOAT v0 = 0;
    FLOAT v1 = 0;
    FLOAT v2 = 0;
    FLOAT v3 = 0;

    v0 = dot( sl.xyzw, float4( w9[ 0 ], w9[ 1 ], w9[ 2 ], w9[ 3 ] ) ) + dot( sc, float4( w9[ 4 ], w9[ 5 ], w9[ 6 ], w9[ 7 ] ) ) + sr.x * w9[ 8 ];
    v1 = dot( sl.yzw,  float3( w9[ 0 ], w9[ 1 ], w9[ 2 ] ) )          + dot( sc, float4( w9[ 3 ], w9[ 4 ], w9[ 5 ], w9[ 6 ] ) ) + dot( sr.xy,   float2( w9[ 7 ], w9[ 8 ] ) );
    v2 = dot( sl.zw,   float2( w9[ 0 ], w9[ 1 ] ) )                   + dot( sc, float4( w9[ 2 ], w9[ 3 ], w9[ 4 ], w9[ 5 ] ) ) + dot( sr.xyz,  float3( w9[ 6 ], w9[ 7 ], w9[ 8 ] ) );
    v3 = sl.w * w9[ 0 ]                                               + dot( sc, float4( w9[ 1 ], w9[ 2 ], w9[ 3 ], w9[ 4 ] ) ) + dot( sr.xyzw, float4( w9[ 5 ], w9[ 6 ], w9[ 7 ], w9[ 8 ] ) );
    
    return float4( v0, v1, v2, v3 );
}                                   

// VPOS is too expensive for this shader, it's on the verge of being ALU bound
// note that if you can do this step as a final step in the pass that outputs
// a texture to blur, you'll save time
float4 psL8toABGR( float4 vp : TEXCOORD0 ) : COLOR
{
    return SampleInput( round( float2( TEX_RESCALE * vp.z, vp.y ) ) );
}


struct VSCOUT_BlurVert
{
    float4 ProjPos  : POSITION;
    float4 tc0 : TEXCOORD0;
};
  
VSCOUT_BlurVert vsVert( in float4 pos : POSITION )
{                                            
    VSCOUT_BlurVert Out;

    Out.ProjPos = pos;

    FLOAT x = (pos.x * 0.5f + 0.5f);  
    FLOAT y = (0.5f - pos.y * 0.5f);

    Out.tc0 = float4( TEX_SX * x, TEX_SY * y, RT_SX_A * x, RT_SY_A * y );

    return Out;                              
}


float4 psVert( float4 tc : TEXCOORD0 ) : COLOR
{
    FLOAT x, y;
    RetileOutput( x, y, round( tc.zw ) );
    
    float2  uv0 = float2( x, y + wo9[ 0 ] );
    float2  uv1 = float2( x, y + wo9[ 1 ] );
    float2  uv2 = float2( x, y + wo9[ 2 ] );
    float2  uv3 = float2( x, y + wo9[ 3 ] );
    float2  uv4 = float2( x, y + wo9[ 4 ] );

    // now do the offsets -- note the texture is in filterable layout
    float4  t0 = tex2D( s0, uv0 / float2( RT_SX, RT_SY ) );
    float4  t1 = tex2D( s0, uv1 / float2( RT_SX, RT_SY ) );
    float4  t2 = tex2D( s0, uv2 / float2( RT_SX, RT_SY ) );
    float4  t3 = tex2D( s0, uv3 / float2( RT_SX, RT_SY ) );
    float4  t4 = tex2D( s0, uv4 / float2( RT_SX, RT_SY ) );

    float4  c = t0 * wt9[ 0 ] + t1 * wt9[ 1 ] + t2 * wt9[ 2 ] + t3 * wt9[ 3 ] + t4 * wt9[ 4 ];
    
    return c.yzwx;  // swizzle for ABGR output
}

float4 psBlur5x5( float4 tc : TEXCOORD0 ) : COLOR
{
    FLOAT x, y;
    RetileOutput( x, y, round( tc.zw ) );
    
    // note the texture is in filterable layout
    // although we remap the output, the input is still a regular, unswizzled texture
    float2  uv0 = float2( x, y + wo5[ 0 ] );
    float2  uv1 = float2( x, y + wo5[ 1 ] );
    float2  uv2 = float2( x, y + wo5[ 2 ] );
    float4  t0 = tex2D( s0, uv0 / float2( RT_SX, RT_SY ) );
    float4  t1 = tex2D( s0, uv1 / float2( RT_SX, RT_SY ) );
    float4  t2 = tex2D( s0, uv2 / float2( RT_SX, RT_SY ) );
    float4  c0 = (t0 * wt5[ 0 ] + t1 * wt5[ 1 ] + t2 * wt5[ 2 ]);
    
    uv0 = float2( x + 1, y + wo5[ 0 ] );
    uv1 = float2( x + 1, y + wo5[ 1 ] );
    uv2 = float2( x + 1, y + wo5[ 2 ] );
    t0 = tex2D( s0, uv0 / float2( RT_SX, RT_SY ) );
    t1 = tex2D( s0, uv1 / float2( RT_SX, RT_SY ) );
    t2 = tex2D( s0, uv2 / float2( RT_SX, RT_SY ) );
    float4 c1 = (t0 * wt5[ 0 ] + t1 * wt5[ 1 ] + t2 * wt5[ 2 ]);
    
    uv0 = float2( x - 1, y + wo5[ 0 ] );
    uv1 = float2( x - 1, y + wo5[ 1 ] );
    uv2 = float2( x - 1, y + wo5[ 2 ] );
    t0 = tex2D( s0, uv0 / float2( RT_SX, RT_SY ) );
    t1 = tex2D( s0, uv1 / float2( RT_SX, RT_SY ) );
    t2 = tex2D( s0, uv2 / float2( RT_SX, RT_SY ) );
    float4 c2 = (t0 * wt5[ 0 ] + t1 * wt5[ 1 ] + t2 * wt5[ 2 ]);

    FLOAT   o0 = dot( c2.zw, float2( w5[ 0 ], w5[ 1 ] ) ) + dot( c0.xyz,  float3( w5[ 2 ], w5[ 3 ], w5[ 4 ] ) );
    FLOAT   o1 = c2.w *                       w5[ 0 ]     + dot( c0.xyzw, float4( w5[ 1 ], w5[ 2 ], w5[ 3 ], w5[ 4 ] ) );
    FLOAT   o2 =                                            dot( c0.xyzw, float4( w5[ 0 ], w5[ 1 ], w5[ 2 ], w5[ 3 ] ) )     + c1.x *              w5[ 4 ];
    FLOAT   o3 =                                            dot( c0.yzw,  float3(          w5[ 0 ], w5[ 1 ], w5[ 2 ] ) )     + dot( c1.xy, float2( w5[ 3 ], w5[ 4 ] ) );

    return float4( o0, o1, o2, o3 ).yzwx;  // swizzle for ABGR
}


VS_OUT vsConsume( float4 pos : POSITION )    
{                                            
    VS_OUT Out;                              
    Out.ProjPos = pos;                       
    Out.Color.xzw = (pos.x * 0.5f + 0.5f);
    Out.Color.y = (0.5f - pos.y * 0.5f);
    return Out;                              
}

float4 psConsume( float2 tc : TEXCOORD0 ) : COLOR
{
    return tex2D( s0, tc );                  
}                                            


float4 psBlur9( float2 v : VPOS ) : COLOR
{
#ifdef VERTICAL
    FLOAT x = v.x + 0.5f;
    FLOAT y = v.y;

    float2  uv0 = float2( x, y + wo9[ 0 ] );
    float2  uv1 = float2( x, y + wo9[ 1 ] );
    float2  uv2 = float2( x, y + wo9[ 2 ] );
    float2  uv3 = float2( x, y + wo9[ 3 ] );
    float2  uv4 = float2( x, y + wo9[ 4 ] );
#else
    FLOAT x = v.x;
    FLOAT y = v.y + 0.5f;

    float2  uv0 = float2( x + wo9[ 0 ], y );
    float2  uv1 = float2( x + wo9[ 1 ], y );
    float2  uv2 = float2( x + wo9[ 2 ], y );
    float2  uv3 = float2( x + wo9[ 3 ], y );
    float2  uv4 = float2( x + wo9[ 4 ], y );
#endif

    // now do the offsets -- note the texture is in filterable layout
    float4  t0 = tex2D( s0, uv0 / float2( TEX_SX, TEX_SY ) );
    float4  t1 = tex2D( s0, uv1 / float2( TEX_SX, TEX_SY ) );
    float4  t2 = tex2D( s0, uv2 / float2( TEX_SX, TEX_SY ) );
    float4  t3 = tex2D( s0, uv3 / float2( TEX_SX, TEX_SY ) );
    float4  t4 = tex2D( s0, uv4 / float2( TEX_SX, TEX_SY ) );

    float4  c = t0 * wt9[ 0 ] + t1 * wt9[ 1 ] + t2 * wt9[ 2 ] + t3 * wt9[ 3 ] + t4 * wt9[ 4 ];
    
    return c.x; // grayscale
}


// VPOS is too expensive for this shader
float4 psBlur5( float4 v : TEXCOORD0 ) : COLOR
{
    v.xy = round( v.xy );
    
#ifdef VERTICAL
    FLOAT x = v.x + 0.5f;
    FLOAT y = v.y;

    float2  uv0 = float2( x, y + wo5[ 0 ] );
    float2  uv1 = float2( x, y + wo5[ 1 ] );
    float2  uv2 = float2( x, y + wo5[ 2 ] );
#else
    FLOAT x = v.x;
    FLOAT y = v.y + 0.5f;

    float2  uv0 = float2( x + wo5[ 0 ], y );
    float2  uv1 = float2( x + wo5[ 1 ], y );
    float2  uv2 = float2( x + wo5[ 2 ], y );
#endif

    // now do the offsets -- note the texture is in filterable layout
    float4  t0 = tex2D( s0, uv0 / float2( TEX_SX, TEX_SY ) );
    float4  t1 = tex2D( s0, uv1 / float2( TEX_SX, TEX_SY ) );
    float4  t2 = tex2D( s0, uv2 / float2( TEX_SX, TEX_SY ) );

    float4  c = t0 * wt5[ 0 ] + t1 * wt5[ 1 ] + t2 * wt5[ 2 ];
    
    return c.x; // grayscale
}

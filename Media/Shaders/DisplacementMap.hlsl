//-----------------------------------------------------------------------------
// Shader for the DisplacementMap sample
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Vertex shader constants
//-----------------------------------------------------------------------------

// Composite world, view, projection transform matrix
float4x4 CompositeTransform : register(c0);

float4 ExportAddress : register(c4);

// GridWidth, 1.0/GridWidth, 0.5/GridWidth
float4 GridWidth : register(c5);

float2 TexCoordScale : register(c6);

float4 PositionScale : register(c7);

float2 TessEdgeLenScale : register(c8);

float2 TextureDimensions : register(c9);

sampler DisplacementMapSampler : register(s0);

static float4 ExportConst = { 0.0, 1.0, 0.0, 0.0 };

// The basis matrix for an interpolating quadratic curve
static float3 InterpQuadraticBasis1 = {  1.0, -1.5, 0.5 };
static float3 InterpQuadraticBasis2 = { -2.0,  2.0, 0.5 };
static float3 InterpQuadraticBasis3 = {  1.0, -0.5, 0.0 };

// The basis matrix for an approximating quadratic B-spline
static float3 ApproxQuadraticBasis1 = {  0.5, -1.0, 0.5 };
static float3 ApproxQuadraticBasis2 = { -1.0,  1.0, 0.5 };
static float3 ApproxQuadraticBasis3 = {  0.5,  0.0, 0.0 };


//-----------------------------------------------------------------------------
// Pixel shader constants
//-----------------------------------------------------------------------------

sampler DiffuseMapSampler : register(s0);


//-----------------------------------------------------------------------------
// Name: ComputeTessFactorsVS()
// Desc: Compute the per-edge tessellation factors used by the tessellation 
//       unit based on the edge length in screen space.
//-----------------------------------------------------------------------------
void ComputeTessFactorsVS( in int vIndex : INDEX )
{
    // First compute the integer x, y coordinates of the top left corner of the 
    // quad from the index.
    float y = floor( vIndex * GridWidth.y + GridWidth.z );
    float x = vIndex - ( y * GridWidth.x );

    // Compute the postions of the corners
    float4 pos0 = float4( x,       y,       0.0, 1.0 ) * PositionScale;
    float4 pos1 = float4( x + 1.0, y,       0.0, 1.0 ) * PositionScale;
    float4 pos2 = float4( x,       y + 1.0, 0.0, 1.0 ) * PositionScale;
    float4 pos3 = float4( x + 1.0, y + 1.0, 0.0, 1.0 ) * PositionScale;

    // Transform the positions into clip space
    pos0 = mul( CompositeTransform, pos0 );
    pos1 = mul( CompositeTransform, pos1 );
    pos2 = mul( CompositeTransform, pos2 );
    pos3 = mul( CompositeTransform, pos3 );
    
    // Project and scale the positions into screen coordinates
    pos0.xy = ( pos0.xy / pos0.w ) * TessEdgeLenScale;
    pos1.xy = ( pos1.xy / pos1.w ) * TessEdgeLenScale;
    pos2.xy = ( pos2.xy / pos2.w ) * TessEdgeLenScale;
    pos3.xy = ( pos3.xy / pos3.w ) * TessEdgeLenScale;
    
    float4 tess_factors;
    
    // Compute the tessellation factors based on the screen space edge length
    tess_factors.x = length( pos1.xy - pos0.xy );
    tess_factors.y = length( pos3.xy - pos1.xy );
    tess_factors.z = length( pos3.xy - pos2.xy );
    tess_factors.w = length( pos2.xy - pos0.xy );
    
    // Export the computed tessellation factors
    asm {
        alloc export=1
        mad eA, vIndex, ExportConst, ExportAddress
        max eM0, tess_factors, tess_factors
    };
}


//-----------------------------------------------------------------------------
// Name: SampleDisplacementBiquadratic()
// Desc: Sample the displacement map using bi-quadratic filtering.
//-----------------------------------------------------------------------------
float SampleDisplacementBiquadratic( float2 texcoord )
{
    // Sample 9 samples from the displacement map.
    float3 P_123, P_456, P_789;
    asm {
        tfetch2D P_123.x___, texcoord, DisplacementMapSampler, OffsetX = -1, OffsetY = -1, 
                             MinFilter=point, MagFilter=point, 
                             UseComputedLOD=false, UseRegisterLOD=false
        tfetch2D P_123._x__, texcoord, DisplacementMapSampler, OffsetX =  0, OffsetY = -1, 
                             MinFilter=point, MagFilter=point, 
                             UseComputedLOD=false, UseRegisterLOD=false
        tfetch2D P_123.__x_, texcoord, DisplacementMapSampler, OffsetX =  1, OffsetY = -1, 
                             MinFilter=point, MagFilter=point, 
                             UseComputedLOD=false, UseRegisterLOD=false

        tfetch2D P_456.x___, texcoord, DisplacementMapSampler, OffsetX = -1, OffsetY =  0, 
                             MinFilter=point, MagFilter=point, 
                             UseComputedLOD=false, UseRegisterLOD=false
        tfetch2D P_456._x__, texcoord, DisplacementMapSampler, OffsetX =  0, OffsetY =  0, 
                             MinFilter=point, MagFilter=point, 
                             UseComputedLOD=false, UseRegisterLOD=false
        tfetch2D P_456.__x_, texcoord, DisplacementMapSampler, OffsetX =  1, OffsetY =  0, 
                             MinFilter=point, MagFilter=point, 
                             UseComputedLOD=false, UseRegisterLOD=false

        tfetch2D P_789.x___, texcoord, DisplacementMapSampler, OffsetX = -1, OffsetY =  1, 
                             MinFilter=point, MagFilter=point, 
                             UseComputedLOD=false, UseRegisterLOD=false
        tfetch2D P_789._x__, texcoord, DisplacementMapSampler, OffsetX =  0, OffsetY =  1, 
                             MinFilter=point, MagFilter=point, 
                             UseComputedLOD=false, UseRegisterLOD=false
        tfetch2D P_789.__x_, texcoord, DisplacementMapSampler, OffsetX =  1, OffsetY =  1, 
                             MinFilter=point, MagFilter=point, 
                             UseComputedLOD=false, UseRegisterLOD=false
    };
    
    // Compute the fractional part of the texture coordinate
    float2 uv = frac( texcoord * TextureDimensions );
    
    float3 curve;
    float3 weights;
    
    // u^2, u, 1
    float3 u2 = { uv.x * uv.x, uv.x, 1.0 };
    weights.x = dot( u2, ApproxQuadraticBasis1 );
    weights.y = dot( u2, ApproxQuadraticBasis2 );
    weights.z = dot( u2, ApproxQuadraticBasis3 );
    
    // Compute the curve in v by evaluating at u
    curve.x = dot( weights, P_123 );
    curve.y = dot( weights, P_456 );
    curve.z = dot( weights, P_789 );

    // v^2, v, 1
    float3 v2 = { uv.y * uv.y, uv.y, 1.0 };
    weights.x = dot( v2, ApproxQuadraticBasis1 );
    weights.y = dot( v2, ApproxQuadraticBasis2 );
    weights.z = dot( v2, ApproxQuadraticBasis3 );
    
    // Compute the final value by evaluating the curve at v
    return dot( weights, curve );
}


//-----------------------------------------------------------------------------
// Name: SampleDisplacementBilinear()
// Desc: Sample the displacement map using bilinear filtering.
//-----------------------------------------------------------------------------
float SampleDisplacementBilinear( float2 texcoord )
{
    return tex2Dlod( DisplacementMapSampler, float4( texcoord, 0.0, 0.0 ) ).x;
}


//-----------------------------------------------------------------------------
// Name: DisplacementMapVS()
// Desc: Vertex shader for displacement mapping
//-----------------------------------------------------------------------------
void DisplacementMapVS( in  int    vIndex     : INDEX,
                        in  float2 vUV        : BARYCENTRIC,
                        in  int    vQuadID    : QUADID,
                        out float4 oPosition  : POSITION,
                        out float2 oTexCoord  : TEXCOORD0 )
{
    // First compute the integer x, y coordinates of the top left corner of the 
    // quad from the index.
    float y = floor( vIndex * GridWidth.y + GridWidth.z );
    float x = vIndex - ( y * GridWidth.x );
    
    // Re-order the parametric coordinates based on the quad id.
    float2 uv = vUV * ( vQuadID == 0 );
    uv += float2( 1.0 - vUV.x,       vUV.y ) * ( vQuadID == 1 ); 
    uv += float2( 1.0 - vUV.x, 1.0 - vUV.y ) * ( vQuadID == 2 );
    uv += float2(       vUV.x, 1.0 - vUV.y ) * ( vQuadID == 3 );
    
    // Compute the texture coordinate.
    float2 texcoord = float2( x + uv.x, y + uv.y ) * TexCoordScale;
    
    // Sample the displacement map.
    float z = SampleDisplacementBiquadratic( texcoord );

    // Compute the postion.
    float4 pos = float4( x + uv.x, y + uv.y, z, 1.0 ) * PositionScale;

    oPosition = mul( CompositeTransform, pos );
    
    oTexCoord = texcoord;
}


//--------------------------------------------------------------------------------------
// Name: TestPS
// Desc: Simple pixel shader that just passes through the color
//--------------------------------------------------------------------------------------
float4 DisplacementMapPS( float2 vTexCoord : TEXCOORD0 ) : COLOR
{
    return tex2D( DiffuseMapSampler, vTexCoord );
}

//--------------------------------------------------------------------------------------
// Shaders for the GPUParticle sample
//--------------------------------------------------------------------------------------

struct VSOUT
{
    float4 Position         : POSITION;
    float4 Diffuse          : COLOR0;
    float2 Tex              : TEXCOORD;
};

struct VSOUTHEIGHT
{
    float4 Position         : POSITION;
    float4 Texcoord0        : TEXCOORD0;
    float4 Texcoord1        : TEXCOORD1;
};

uniform float4x4 WorldView      : register(c8);  // matWorldView
uniform float4x4 WorldViewProj  : register(c4);  // matWorldViewProjection
uniform float3   LightDirection : register(c19); // light direction (in model space)
uniform float4   Diffuse        : register(c21); // material diffuse color * light diffuse color

uniform float4   Ambient        : register(c22); // material ambient color
uniform float4   FogRange       : register(c23); // ( x, fog_end, (1/(fog_end-fog_start)), x)
uniform float4   EyeDirection   : register(c24); // eye vector (in model space)
uniform float4   Constants      : register(c1);  // ( 1, 0.5, 2, 4 )
uniform float4   Zero           : register(c0);  // ( 0, 0, 0, 0 )


//--------------------------------------------------------------------------------------
// Name: TerrainVS()
// Desc: 
//--------------------------------------------------------------------------------------
VSOUT TerrainVS( const float3 Position : POSITION,
                 const float3 Normal   : NORMAL,
                 const float2 Tex      : TEXCOORD )
{
    VSOUT   Output;
    float4  DiffuseColor;

    // Transform position to the clipping space
    Output.Position = mul(float4(Position,1.0f), WorldViewProj);
    
    // Output Tex
    Output.Tex = Tex ;

    // Do the lighting calculation
    DiffuseColor = dot(Normal, LightDirection);
    DiffuseColor = max(DiffuseColor, Zero.x);
    DiffuseColor = DiffuseColor * Diffuse + Ambient;
    
    Output.Diffuse = min(DiffuseColor, Constants.xxxx);

    return Output;
}


//--------------------------------------------------------------------------------------
// Name: TerrainPS()
// Desc: 
//--------------------------------------------------------------------------------------
sampler TerrainTex : register(s0);

float4 TerrainPS( VSOUT Input )  : COLOR
{
    float4 Output = tex2D (TerrainTex, Input.Tex) * Input.Diffuse ;
    return Output;
}


//--------------------------------------------------------------------------------------
// Name: TerrainHeightVS()
// Desc: 
//--------------------------------------------------------------------------------------
VSOUTHEIGHT TerrainHeightVS( const float3 Position : POSITION,
                             const float3 Normal   : NORMAL,
                             const float2 Tex      : TEXCOORD )
{
    VSOUTHEIGHT   Output;

    // Transform position to the clipping space
    Output.Position = mul( float4( Position, 1.0f ), WorldViewProj );

    Output.Texcoord0 = float4( Normal.xyz , 0.f) ;
    Output.Texcoord1 = float4( Output.Position.xyz, 1.f ) ;
    
    return Output;
}


//--------------------------------------------------------------------------------------
// Name: TerrainHeightPS()
// Desc: 
//--------------------------------------------------------------------------------------
float4 TerrainHeightPS( float4 Texcoord0 : TEXCOORD0,
                        float4 Texcoord1 : TEXCOORD1 ) : COLOR
{

    //Float16 texture does not keep negative value
    float4 Output = normalize( float4( Texcoord0.xyz, 0.f ));
    Texcoord1.z += 0.001f; //Threshold
    Texcoord1.y = -Texcoord1.y ;
    
    //Calculate D factor
    float3 fP0 = Texcoord1.xzy * 32.f ;
    fP0.y = fP0.y * 2.f - 32.f;
    float fD = dot( - Output.xyz, fP0.xyz) ;
    Output.w = fD ;    
    
    return Output;
}

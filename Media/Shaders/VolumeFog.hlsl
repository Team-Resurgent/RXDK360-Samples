//--------------------------------------------------------------------------------------
// Vertex and Pixel shaders for Volume Fog
//--------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------
// Vertex shader constants 
//--------------------------------------------------------------------------------------
float4x4 matWorldViewProj : register(c0);
float3   vLocalLightPos   : register(c5);
float4   vMaterialColor   : register(c6);
float4   vAmbientColor    : register(c7);

float3   vLocalEyePos     : register(c5);
float2   ScaleAndOffset   : register(c6);


//--------------------------------------------------------------------------------------
// Pixel shader constants 
//--------------------------------------------------------------------------------------
float4 FogColorAndDensity : register(c0);


//--------------------------------------------------------------------------------------
// Name: SkyBoxVS()
// Desc: Unlit with a single textures.
//--------------------------------------------------------------------------------------
void SkyBoxVS( in  float4 vLocalPos  : POSITION,
               in  float3 vNormal    : NORMAL,
               in  float2 vTexCoord  : TEXCOORD0,
               out float4 oScreenPos : POSITION,
               out float2 oTexCoord  : TEXCOORD0,
               out float4 oColor     : COLOR )
{
    oScreenPos = mul( vLocalPos, matWorldViewProj );
    oTexCoord  = vTexCoord;
    oColor     = 1.0f;
}


//--------------------------------------------------------------------------------------
// Name: LitDiffuse1TexVS()
// Desc: Diffuse lighting with a single texture.
//       Isolate the computation of the position since it has to match the position from
//       the z-prepass.
//--------------------------------------------------------------------------------------
void LitDiffuse1TexVS( in  float4 vLocalPos  : POSITION,
                       in  float3 vNormal    : NORMAL,
                       in  float2 vTexCoord  : TEXCOORD0,
                       out float4 oScreenPos : POSITION,
                       out float2 oTexCoord  : TEXCOORD0,
                       out float4 oColor     : COLOR )
{
    [isolate] oScreenPos = mul( vLocalPos, matWorldViewProj );
    oTexCoord  = vTexCoord;
    oColor     = max( 0.0f, dot( vNormal, vLocalLightPos ) ) * vMaterialColor + vAmbientColor;
}


//--------------------------------------------------------------------------------------
// Name: DepthOutputVS()
// Desc: Output depth [0..1] in a texture coordinate.
//       Isolate the computation of the position since it has to match the position used
//       in the color pass.
//--------------------------------------------------------------------------------------
void DepthOutputVS( in  float4 vLocalPos  : POSITION,
                    in  float3 vNormal    : NORMAL,
                    in  float2 vTexCoord  : TEXCOORD0,
                    out float4 oScreenPos : POSITION,
                    out float2 oTexDepth  : TEXCOORD0 )
{
    [isolate] oScreenPos  = mul( vLocalPos, matWorldViewProj );
    oTexDepth.x = oScreenPos.w * ScaleAndOffset.x + ScaleAndOffset.y;
    oTexDepth.y = 0.0f;
}


//--------------------------------------------------------------------------------------
// Name: PassThruPosTexVS()
// Desc: Pass through position and textures.
//--------------------------------------------------------------------------------------
void PassThruPosTexVS( in  float4 vLocalPos  : POSITION,
                       in  float2 vTexCoord  : TEXCOORD0,
                       out float4 oScreenPos : POSITION,
                       out float2 oTexCoord  : TEXCOORD0 )
{
    oScreenPos = vLocalPos;
    oTexCoord  = vTexCoord;
}


//--------------------------------------------------------------------------------------
// Name: ComputeFogValuePS()
// Desc: Subtract the sum of the front faces from the sum of the back faces and
//       compute the final fog value.
//--------------------------------------------------------------------------------------
sampler2D SumTex : register(s0);

float4 ComputeFogValuePS( float2 vTexCoord : TEXCOORD0 ) : COLOR
{
    float2 Sums = tex2D( SumTex, vTexCoord );

    float4 Color;
    Color.rgb = FogColorAndDensity.rgb;
    Color.a   = (Sums.g - Sums.r) * FogColorAndDensity.a;
    
    return Color;
}


//--------------------------------------------------------------------------------------
// Name: DepthOutputPS()
// Desc: Output the depth value.
//--------------------------------------------------------------------------------------
sampler1D DepthEncodeTex : register(s0);

float4 DepthOutputPS( float2 vTexDepth : TEXCOORD0 ) : COLOR
{
    // Return the depth value.
    return vTexDepth.xxxx;
}


//--------------------------------------------------------------------------------------
// Name: DiffuseTexModulatePS()
// Desc: Modulate diffuse color with texture.
//--------------------------------------------------------------------------------------
sampler2D DiffuseTexture : register(s0);

float4 DiffuseTexModulatePS( float4 vColor    : COLOR0,
                             float2 vTexCoord : TEXCOORD0 ) : COLOR
{
    return vColor * tex2D( DiffuseTexture, vTexCoord );
}

//--------------------------------------------------------------------------------------
// Shader to perform glass rendering
//--------------------------------------------------------------------------------------


//--------------------------------------------------------------------------------------
// Vertex shader constants
//--------------------------------------------------------------------------------------
struct VSOUT_SCENE
{
    float4 Position         : POSITION;
    float2 TexCoord0        : TEXCOORD0;   
    float  Light            : TEXCOORD1;
};

struct VSOUT_ZNGLASS
{
    float4 Position         : POSITION;
    float  Depth            : TEXCOORD0;
    float3 Normal           : TEXCOORD1;
};

struct VSOUT_GLASS
{
    float4 Position         : POSITION;
    float4 TexCoord         : TEXCOORD0;
    float3 CubeCoord        : TEXCOORD1;
    float3 LightDir         : TEXCOORD2;
    float3 Normal           : TEXCOORD3;
    float4 TransCosPhi      : TEXCOORD4;
};

//--------------------------------------------------------------------------------------
// Vertex shader constants
//--------------------------------------------------------------------------------------
uniform float4x4 g_matView              : register(c0);  // World-view matrix
uniform float4x4 g_matProj              : register(c4);  // World-view-projection matrix
uniform float4   g_vLightPos            : register(c8);  // Light position
uniform float4   g_vGlassProperties1    : register(c9);  // refraction index, refraction index^2

//--------------------------------------------------------------------------------------
// Pixel shader constants
//--------------------------------------------------------------------------------------
uniform float4   g_vGlassProperties2    : register(c0);  // transparency, reflectancy, specular power, specular amplitude
uniform float4   g_vGlassProperties3    : register(c1);  // Fresnel term at normal, 0.5/sin(refraction fov/2), 1/refraction index, 1/refraction index^2
uniform float4   g_vAmbient             : register(c2);  // Ambient color


sampler Sampler0 : register(s0);
sampler Sampler1 : register(s1);
sampler Sampler2 : register(s2);
sampler Sampler3 : register(s3);


//--------------------------------------------------------------------------------------
// Name: ShadeSceneVertex()
// Desc: Vertex shader to perform scene rendering
//--------------------------------------------------------------------------------------
VSOUT_SCENE ShadeSceneVertex( const float3 vPosition  : POSITION,
                              const float3 vNormal    : NORMAL,
                              const float2 vTexCoord0 : TEXCOORD0 )
{
    VSOUT_SCENE Output;

    // Get view position
    float4 vViewPosition = mul( float4( vPosition, 1.0f ), g_matView );

    // Get vertex position
    Output.Position = mul( vViewPosition, g_matProj );

    // Calculate light intensity
    float3 vN = mul( float4( vNormal, 1.0f ), g_matView );
    vN = normalize( vN );
    float3 vLight = normalize( float3( g_vLightPos.x, g_vLightPos.y, g_vLightPos.z ) - vViewPosition );
    Output.Light = max( dot( vN, vLight ) * g_vLightPos.w, 0.0 );

    // Pass texture coordinates through
    Output.TexCoord0 = vTexCoord0;

    return Output;
}


//--------------------------------------------------------------------------------------
// Name: ShadeZNGlassVertex()
// Desc: Vertex shader to perform depth and normal shading
//--------------------------------------------------------------------------------------
VSOUT_ZNGLASS ShadeZNGlassVertex( const float3 vPosition  : POSITION,
                                  const float3 vNormal    : NORMAL )
{
    VSOUT_ZNGLASS Output;
    
    // Get view position
    float4 vViewPosition = mul( float4( vPosition, 1.0f ), g_matView );

    // Get vertex position
    Output.Position = mul( vViewPosition, g_matProj );
    
    Output.Depth = vViewPosition.z;
    Output.Normal = 0.5 + ( normalize( mul( float4( vNormal, 1.0f ), g_matView ) ) * 0.5 );
    
    return Output;
}

//--------------------------------------------------------------------------------------
// Name: ShadeGlassVertex()
// Desc: Vertex shader to perform refraction calculations
//--------------------------------------------------------------------------------------
VSOUT_GLASS ShadeGlassVertex( const float3 vPosition  : POSITION,
                              const float3 vNormal    : NORMAL,
                              const float2 vTexCoord0 : TEXCOORD0 )
{
    VSOUT_GLASS Output;

    // Get view position
    float4 vViewPosition = mul( float4( vPosition, 1.0f ), g_matView );

    // Get vertex position
    float4 vOutPosition = mul( vViewPosition, g_matProj );
    Output.Position = vOutPosition;

    // Calculate light intensity
    float3 vN = mul( float4( vNormal, 1.0f ), g_matView );
    vN = normalize( vN );
    Output.LightDir = normalize( float3( g_vLightPos.x, g_vLightPos.y, g_vLightPos.z ) - vViewPosition );
    Output.Normal = vN;

    // Pass texture coordinates through
    Output.TexCoord.xy = vTexCoord0;
    
    // Calculate coordinates for backface texture
    Output.TexCoord.zw = vOutPosition.xy / vOutPosition.w;
    
    //
    // Calculate reflection vector
    //
    float3 vIncident = normalize( float3( vViewPosition.x, vViewPosition.y, vViewPosition.z ) );
    // Cos of incident angle
    float fCosPhi = dot( vIncident, vN );
    Output.TransCosPhi.w = fCosPhi;
    Output.CubeCoord = 2 * -fCosPhi * vN + vIncident;

    //
    // Calculate front face refraction
    //
    // Cos of refraction angle through the material
    float fCosTheta = sqrt( 1.0 - g_vGlassProperties1.y * ( 1.0 - fCosPhi * fCosPhi ) ) / g_vGlassProperties1.y;
    
    // Refracted vector
    Output.TransCosPhi.xyz = ( g_vGlassProperties1.x * vIncident ) - ( ( fCosTheta + g_vGlassProperties1.x * fCosPhi ) * vN );

    return Output;
}


//--------------------------------------------------------------------------------------
// Name: ShadeScenePixel()
// Desc: Pixel shader to perform scene rendering
//--------------------------------------------------------------------------------------
float4 ShadeScenePixel( VSOUT_SCENE Input  ) : COLOR
{
    float4 PixelColor = tex2D( Sampler0, Input.TexCoord0 ) * ( ( Input.Light.xxxx ) + g_vAmbient );
    PixelColor.a = 0;
    return PixelColor;
}

//--------------------------------------------------------------------------------------
// Name: ShadeZNGlassPixel()
// Desc: Pixel shader to perform depth and normal shading
//--------------------------------------------------------------------------------------
float4 ShadeZNGlassPixel( VSOUT_ZNGLASS Input ) : COLOR
{   
    return float4( Input.Normal.x, Input.Normal.y, Input.Normal.z, Input.Depth );
}

//--------------------------------------------------------------------------------------
// Name: ShadeGlassPixel()
// Desc: Pixel shader to perform backface refraction and Fresnel simulation.
//--------------------------------------------------------------------------------------
float4 ShadeGlassPixel( VSOUT_GLASS Input  ) : COLOR
{
    //
    // Calculate back face refraction
    //
    // Lookup backface normal
    float2 vBackfaceCoord = 0.5 * ( Input.TexCoord.zw + 1.0 );
    vBackfaceCoord.y = 1.0 - vBackfaceCoord.y;
    float4 vBackfaceZN = 2.0 * ( tex2D( Sampler2, vBackfaceCoord ) - 0.5 );
    
    float3 vN = -float3( vBackfaceZN.x, vBackfaceZN.y, vBackfaceZN.z );
    float fCosPhi = dot( Input.TransCosPhi.xyz, vN );
    
    // Cos of refraction angle exiting the material
    float fCosTheta = sqrt( 1.0 - g_vGlassProperties3.w * ( 1.0 - fCosPhi * fCosPhi ) ) / g_vGlassProperties3.w;
    
    // Refracted vector
    float3 vTrans = ( g_vGlassProperties3.z * Input.TransCosPhi.xyz ) - ( ( fCosTheta + g_vGlassProperties3.z * fCosPhi ) * vN );
    
    // Refraction texture coordinates
    float2 RefTexCoord;
    RefTexCoord.x = 0.5 + ( vTrans.x * g_vGlassProperties3.y );
    RefTexCoord.y = 0.5 + ( -vTrans.y * g_vGlassProperties3.y );
    
    float4 TexColor = lerp( tex2D( Sampler0, Input.TexCoord.xy ), tex2D( Sampler1, RefTexCoord ), g_vGlassProperties2.x );
    TexColor = lerp( TexColor, texCUBE( Sampler3, Input.CubeCoord ), g_vGlassProperties2.y );
    
    // Calculate specular
    float Specular = pow( g_vGlassProperties2.w * max( dot( Input.Normal, Input.LightDir ), 0.0 ), g_vGlassProperties2.z );
    
    // Calculate Fresnel term
    Specular *= g_vGlassProperties3.x + pow( 1.0 + Input.TransCosPhi.w, 5.0 ) * ( 1.0 - g_vGlassProperties3.x );
    float4 PixelColor = TexColor + Specular;

    // Write the specular into the alpha channel for blooming
    PixelColor.a = min( Specular, 1.0 );
    
    return PixelColor;
}


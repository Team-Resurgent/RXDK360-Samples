//--------------------------------------------------------------------------------------
// Shader to perform depth-of-field rendering
//--------------------------------------------------------------------------------------


//--------------------------------------------------------------------------------------
// Vertex shader constants
//--------------------------------------------------------------------------------------
struct VSOUT
{
    float4 Position         : POSITION;
    float2 TexCoord0        : TEXCOORD0;   
    float  CameraDepth      : TEXCOORD1; 
    float3 Normal           : TEXCOORD2;
    float3 View             : TEXCOORD3;
};

uniform float3   g_vCameraPt            : register(c1);  // Camera position in object space
uniform float4x4 g_matWorldView         : register(c8);  // World-view matrix
uniform float4x4 g_matWorldViewProj     : register(c12); // World-view-projection matrix


//--------------------------------------------------------------------------------------
// Pixel shader constants
//--------------------------------------------------------------------------------------
uniform float3 g_vLightDirection        : register(c0);  // Light direction
uniform float  g_fReflectivityCoeff     : register(c2);  // For reflective objects
uniform float2 g_TexMod                 : register(c3);  // Hack for inverting textures
uniform float3 g_vConstantColor         : register(c4);  // Constant color

uniform float  g_fFocalPlaneDistance    : register(c20); // Focal plane distance
uniform float  g_fNearBlurPlaneDistance : register(c21); // Near blur plance distance
uniform float  g_fFarBlurPlaneDistance  : register(c22); // Far blur plane distance
uniform float  g_fFarBlurLimit          : register(c23); // Far blur limit [0,1]

static const int NUM_POISSON_TAPS = 8;
static const float2 g_Poisson[8] = 
{
    float2( 0.000000f, 0.000000f ),
    float2( 0.527837f,-0.085868f ),
    float2(-0.040088f, 0.536087f ),
    float2(-0.670445f,-0.179949f ),
    float2(-0.419418f,-0.616039f ),
    float2( 0.440453f,-0.639399f ),
    float2(-0.757088f, 0.349334f ),
    float2( 0.574619f, 0.685879f ),
};

static const float2 g_vMaxCoC = float2( 5.0f, 10.0f );
static const float  g_fRadiusScale = 0.4f;

uniform float2   g_vPixelSizeLow  : register(c0);
uniform float2   g_vPixelSizeHigh : register(c1);

sampler SourceTextureSampler : register(s0);
sampler BlurTextureSampler   : register(s1);


//--------------------------------------------------------------------------------------
// Name: ShadeDOFVertex()
// Desc: Vertex shader to perform depth-of-field rendering
//--------------------------------------------------------------------------------------
VSOUT ShadeDOFVertex( float3 vPosition  : POSITION,
                      float3 vNormal    : NORMAL,
                      float2 vTexCoord0 : TEXCOORD0 )
{
    VSOUT Output;

    // Transform the vertex
    Output.Position = mul( float4(vPosition, 1.0f), g_matWorldViewProj );

    // Ouput the view vector in object space
    Output.View = normalize( g_vCameraPt - vPosition );

    // Ouput the normal in object space
    Output.Normal = vNormal;

    // tu = vNormal dot vLight
    Output.TexCoord0 = vTexCoord0;

    // Compute the camera depth of the vertex
    // Note: optimize this to use a dp4
    float4 vCameraPos = mul( float4(vPosition,1.0f), g_matWorldView );
    Output.CameraDepth = vCameraPos.z;

    return Output;
}


//--------------------------------------------------------------------------------------
// Name: ComputeReflection()
// Desc: Schlick approximation for Fresnel reflection
//--------------------------------------------------------------------------------------
float ComputeReflection( float ks, float fCosGamma )
{
    return ks + (1-ks) * pow( 1.0f - max( 0, fCosGamma ), 5.0f );
}


//--------------------------------------------------------------------------------------
// Name: ReflectiveSurfacePS()
// Desc: Pixel shader to render a reflective surface, where the reflectivity is output
//       in the alpha channel
//--------------------------------------------------------------------------------------
float4 ReflectiveSurfacePS( float2 vTexCoord0 : TEXCOORD0,   
                            float3 vNormal    : TEXCOORD2,
                            float3 vView      : TEXCOORD3 ) : COLOR
{
    // Diffuse lighting
    float4 TexelColor   = tex2D( SourceTextureSampler, vTexCoord0 );
    float3 DiffuseColor = dot( vNormal, g_vLightDirection )*0.5f + 0.5f;

    // Specular highlight
    float3 vHalf     = normalize( vView + g_vLightDirection );
    float  fSpecular = pow( max( 0, dot( vHalf, vNormal ) ), 20 );
    float3 vSpecular = fSpecular * float3(1,0,0);

    float fReflection = ComputeReflection( g_fReflectivityCoeff, dot( vNormal, vView ) );
    
    return float4( TexelColor*DiffuseColor + vSpecular, fReflection );
}


//--------------------------------------------------------------------------------------
// Name: ComputeDepthBlur()
// Desc: Compute a per-pixel depth blur value
//--------------------------------------------------------------------------------------
float ComputeDepthBlur( FLOAT fDepth )
{
    // Compute depth blur
    float fDepthBlur;
    
    if( fDepth < g_fFocalPlaneDistance )
    {
        // Scale depth value between near blur distance and focal distance to [-1,0] range
        fDepthBlur = ( fDepth - g_fFocalPlaneDistance ) / ( g_fFocalPlaneDistance - g_fNearBlurPlaneDistance );
    }
    else
    {
        // Scale depth value between focal distance and far blur distance to [0,1] range
        fDepthBlur = ( fDepth - g_fFocalPlaneDistance ) / ( g_fFarBlurPlaneDistance - g_fFocalPlaneDistance );

        // Clamp the far blur to a maximum bluriness
        fDepthBlur = clamp( fDepthBlur, 0, g_fFarBlurLimit );
    }

    // Scale and bias the depth blur into the [0,1] range
    return fDepthBlur * 0.5f + 0.5f;
}
    

//--------------------------------------------------------------------------------------
// Name: ShadeDOFPixel()
// Desc: Pixel shader to perform depth-of-field rendering
//--------------------------------------------------------------------------------------
float4 ShadeDOFPixel( float2 vTexCoord0   : TEXCOORD0,   
                      float  vCameraDepth : TEXCOORD1, 
                      float3 vNormal      : TEXCOORD2,
                      float3 vView        : TEXCOORD3 ) : COLOR
{
    // Fetch a texel from the base texture
    float4 vTexelColor  = tex2D( SourceTextureSampler, vTexCoord0 );

    // Optionally invert the texture (to distinguish white and black chess pieces)
    vTexelColor = g_TexMod.x + g_TexMod.y * vTexelColor; 

    // Light the pixel
    float3 vDiffuseColor = dot( vNormal, g_vLightDirection )*0.5f + 0.5f;
    float3 vHalf     = normalize( vView + g_vLightDirection );
    float  fSpecular = pow( max( 0, dot( vHalf, vNormal ) ), 20 );
    float3 vSpecular = fSpecular * float3(1,0.5,0.5);

    // Compute the per-pixel depth blur, to be output in the alpha channel
    float fDepthBlur = ComputeDepthBlur( vCameraDepth );

    // Modulate the result with a solid color specified by the app
    return float4( vTexelColor*vDiffuseColor + vSpecular, fDepthBlur );
}


//-----------------------------------------------------------------------------
// Name: ScreenSpaceQuadShaderVS  
// Desc: Pass-thru shader for drawing screen-space quads
//-----------------------------------------------------------------------------
struct PASSTHRU_VERTEX
{
    float4 Position   : POSITION;
    float2 TexCoords  : TEXCOORD0;
};

PASSTHRU_VERTEX ScreenSpaceQuadVS( float2 vPosition  : POSITION, 
                                   float2 vTexCoords : TEXCOORD0 )
{
    PASSTHRU_VERTEX Output;
    Output.Position  = float4( vPosition.x-0.5f, vPosition.y-0.5f, 1.0f, 1.0f );
    Output.TexCoords = vTexCoords;
    return Output;
}


//--------------------------------------------------------------------------------------
// Name: ConstantColorVS()
// Desc: Simple shader for drawing solid-colored primitives
//--------------------------------------------------------------------------------------
float4 ConstantColorVS( float3 vPosition  : POSITION ) : POSITION
{
    // Transform the vertex position
    return mul( float4(vPosition, 1.0f), g_matWorldViewProj );
}


//-----------------------------------------------------------------------------
// Name: ConstantColorPS  
// Desc: Simple shader for drawing solid-colored primitives
//-----------------------------------------------------------------------------
float4 ConstantColorPS( float4 vPosition : POSITION ) : COLOR
{
    return float4( g_vConstantColor, 0.5f );
}


//-----------------------------------------------------------------------------
// Name: DebugShowDepthBlurValuePS()
// Desc: Helpful pshader to view gray scale depth-bluriness values 
//-----------------------------------------------------------------------------
float4 DebugShowDepthBlurValuePS( float2 vTexCoord : TEXCOORD0 ) : COLOR0
{
    // Fecth center tap
    float4 vOutColor = tex2D( SourceTextureSampler, vTexCoord );

    float fDepthBlur = 1 - abs( vOutColor.a*2 - 1 );
    return float4( fDepthBlur, fDepthBlur, fDepthBlur, fDepthBlur );
}


//-----------------------------------------------------------------------------
// Name: DOFPostProcessPS()
// Desc: Post-processing DOF pixel shader
//-----------------------------------------------------------------------------
float4 PoissonDOFFilterPS( float2 vTexCoord : TEXCOORD0 ) : COLOR0
{
    // Fecth center tap
    float4 vOutColor = tex2D( SourceTextureSampler, vTexCoord );

    // Save depth
    float fCenterDepth = vOutColor.a;

    // Convert depth into blur radius in pixels
    float fDiscRadius = abs( fCenterDepth * g_vMaxCoC.y - g_vMaxCoC.x );
    
    // Compute disc radius on low-res image
    float fDiscRadiusLow = fDiscRadius * g_fRadiusScale;
    
    // Accumulate output color across all taps
    vOutColor = 0;
    
    for( int t=0; t<NUM_POISSON_TAPS; t++ )
    {
        // Fetch lo-res tap
        float2 vCoordLow = vTexCoord + (g_vPixelSizeLow * g_Poisson[t] * fDiscRadiusLow );
        float4 vTapLow   = tex2D( BlurTextureSampler, vCoordLow );
        
        // Fetch hi-res tap
        float2 vCoordHigh = vTexCoord + (g_vPixelSizeHigh * g_Poisson[t] * fDiscRadius );
        float4 vTapHigh   = tex2D( SourceTextureSampler, vCoordHigh );
        
        // Put tap bluriness into [0,1] range
        float fTapBlur = abs( vTapHigh.a * 2.0f - 1.0f );
        
        // Mix lo-res and hi-res taps based on bluriness
        float4 vTap = lerp( vTapHigh, vTapLow, fTapBlur );
        
        // Apply leaking reduction: lower weight for taps that are closer than the
        // center tap and in focus
        vTap.a = ( vTap.a >= fCenterDepth ) ? 1.0f : abs( vTap.a * 2.0f - 1.0f );
        
        // Accumumate
        vOutColor.rgb += vTap.rgb * vTap.a;
        vOutColor.a   += vTap.a;
    }
    // Normalize and return result
    return ( vOutColor / vOutColor.a );
}


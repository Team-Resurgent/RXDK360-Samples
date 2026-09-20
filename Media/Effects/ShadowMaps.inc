
float ShadowMapSamplePoint( float pixelDepth, float2 texCoord, sampler2D samplerDepthTexture )
{
    float4 DepthSample = tex2D( samplerDepthTexture, texCoord );
    return ( pixelDepth >= DepthSample.r ) ? 0 : 1;
}

shared float g_ShadowMapSize = 512;
shared float2 g_TexelOffsets[3] = { float2( 1, 0 ), float2( 0, 1 ), float2( 1, 1 ) };

float ShadowMapSamplePCFEdgeTapSmoothing( float pixelDepth, float2 texCoord, sampler2D samplerDepthTexture )
{
    float4 Weights = 0;
    float4 DepthValues = 0;
    float2 TexLOD = 0;
    asm {
        getCompTexLOD2D TexLOD.xy, texCoord, samplerDepthTexture, AnisoFilter=max8to1
        setTexLOD TexLOD.x
        getWeights2D Weights, texCoord, samplerDepthTexture, MinFilter=linear, MagFilter=linear, MipFilter=point, UseComputedLOD=false, UseRegisterLOD=true
        tfetch2D DepthValues.x___, texCoord, samplerDepthTexture, OffsetX = -0.5, OffsetY = -0.5, MinFilter=point, MagFilter=point, UseComputedLOD=false, UseRegisterLOD=true
        tfetch2D DepthValues._x__, texCoord, samplerDepthTexture, OffsetX =  0.5, OffsetY = -0.5, MinFilter=point, MagFilter=point, UseComputedLOD=false, UseRegisterLOD=true
        tfetch2D DepthValues.__x_, texCoord, samplerDepthTexture, OffsetX = -0.5, OffsetY =  0.5, MinFilter=point, MagFilter=point, UseComputedLOD=false, UseRegisterLOD=true
        tfetch2D DepthValues.___x, texCoord, samplerDepthTexture, OffsetX =  0.5, OffsetY =  0.5, MinFilter=point, MagFilter=point, UseComputedLOD=false, UseRegisterLOD=true
    };
        
    Weights.zw = float2( 1, 1 ) - Weights.xy;
    Weights = Weights.zxzx * Weights.wwyy;
    
    float4 inLight = pixelDepth.xxxx < DepthValues;
    return dot( inLight, Weights );
}

/*
float ShadowMapSamplePCF9EdgeTapSmoothing( float pixelDepth, float2 texCoord, sampler2D samplerDepthTexture )
{
    float4 Weights = 0;
    float4 DepthValuesA = 0;
    float4 DepthValuesB = 0;
    float4 DepthValuesC = 0;
    asm {
        getWeights2D Weights.xy__, texCoord, samplerDepthTexture, MinFilter=linear, MagFilter=linear, MipFilter=point
        tfetch2D DepthValuesA.x___, texCoord, samplerDepthTexture, OffsetX = -0.5, OffsetY = -0.5, MinFilter=point, MagFilter=point, MipFilter=point
        tfetch2D DepthValuesA._x__, texCoord, samplerDepthTexture, OffsetX =  0.5, OffsetY = -0.5, MinFilter=point, MagFilter=point, MipFilter=point
        tfetch2D DepthValuesA.__x_, texCoord, samplerDepthTexture, OffsetX = -0.5, OffsetY =  0.5, MinFilter=point, MagFilter=point, MipFilter=point
        tfetch2D DepthValuesA.___x, texCoord, samplerDepthTexture, OffsetX =  0.5, OffsetY =  0.5, MinFilter=point, MagFilter=point, MipFilter=point
        tfetch2D DepthValuesB.x___, texCoord, samplerDepthTexture, OffsetX = -0.5, OffsetY =  1.5, MinFilter=point, MagFilter=point, MipFilter=point
        tfetch2D DepthValuesB._x__, texCoord, samplerDepthTexture, OffsetX =  0.5, OffsetY =  1.5, MinFilter=point, MagFilter=point, MipFilter=point
        tfetch2D DepthValuesB.__x_, texCoord, samplerDepthTexture, OffsetX =  1.5, OffsetY = -0.5, MinFilter=point, MagFilter=point, MipFilter=point
        tfetch2D DepthValuesB.___x, texCoord, samplerDepthTexture, OffsetX =  1.5, OffsetY =  0.5, MinFilter=point, MagFilter=point, MipFilter=point
        tfetch2D DepthValuesC.x___, texCoord, samplerDepthTexture, OffsetX =  1.5, OffsetY =  1.5, MinFilter=point, MagFilter=point, MipFilter=point
    };
    
    Weights.zw = float2( 1, 1 ) - Weights.xy;
    Weights = Weights.zxzx * Weights.wwyy;
    
    float4 inLight = pixelDepth.xxxx < DepthValues;
    return dot( inLight, Weights );
}
*/

#define ShadowMapSample ShadowMapSamplePCFEdgeTapSmoothing

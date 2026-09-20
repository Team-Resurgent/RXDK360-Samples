//---------------------------------------------------------------------------------------------------------
// FastBlockCompressGPU.hlsl
//
// File containing all shaders used by GPUCompressor class
//
// This file is designed to be a nearly standalone module, which could be cut-and-pasted into
// title code without significant dependencies.
//
// XNA Developer Connection
// Copyright ( C ) Microsoft Corporation. All rights reserved.
//---------------------------------------------------------------------------------------------------------

// Test speed of bandwidth-only
//#define STUB_COMPRESSION

// Shader parameters which can either be compile-time choices or static run-time branches.
#ifndef g_bAnchorChoice
bool g_bAnchorChoice : register(b0);    // if true, DXT1 anchors chosen using covariance
#endif                                  // if false, DXT1 anchors are always min/max luminance
#ifndef g_bMemExport
bool g_bMemExport    : register(b1);    // if true, memexport results, else write to EDRAM
#endif

struct VERTEX
{
    float4 vPosition    : POSITION0;
    float2 vTexCoord    : TEXCOORD0;
};

struct INTERPOLATORS
{
    float4 vPosition     : POSITION0;
    float4 vTexCoord     : TEXCOORD0;
};

//---------------------------------------------------------------------------------------------------------
// Name: ScreenSpaceShaderVS()
// Desc: Simple pass through. 
// 
// WARNING: This shader is only included here for reference, and to make this a stand-alone module.  
// The sample uses a separate copy of this shader in FastBlockCompress.hlsl
//---------------------------------------------------------------------------------------------------------
float4x4 g_matWVP           : register(c0);
float2 g_vTextureDims       : register(c4);

INTERPOLATORS ScreenSpaceShaderVS( VERTEX In )
{
    INTERPOLATORS Out;
    Out.vPosition = mul( In.vPosition, g_matWVP );
    Out.vTexCoord.xy = In.vTexCoord;
    
    // zw are helper data for GetTiledOffset2D.  
    // The integer part is tile number, 
    // and the fractional part is coords within a tile.
    // These are exact precision for power-of-two dimensions
    Out.vTexCoord.zw = In.vTexCoord * g_vTextureDims;
    
    return Out;
}


//---------------------------------------------------------------------------------------------------------
// Name: CopyTexturePS()
// Desc: Copies a texture. 
// 
// WARNING: This shader is only included here for reference, and to make this a stand-alone module.  
// The sample uses a separate copy of this shader in FastBlockCompress.hlsl
//---------------------------------------------------------------------------------------------------------
sampler samplerInput        : register(s0);

float4 CopyTexturePS( INTERPOLATORS In ) : COLOR
{
    float4 vColor = tex2D( samplerInput, In.vTexCoord.xy );
    return vColor;
}


//---------------------------------------------------------------------------------------------------------
// Name: MergeBlocksPS()
// Desc: Helper function to aid in 128-bit tiling operations.  The two source textures
// each contain 64 bits out of each 128-bit block.  This shader effectively interleaves the two 
// halves without otherwise changing the data order.
//---------------------------------------------------------------------------------------------------------
sampler samplerBlock0       : register(s0);
sampler samplerBlock1       : register(s1);

// SamplerBlock0 fetches to the partial contents of Block0 (e.g. RGB within DXT5), as 16:16.
// SamplerBlock1 fetches to the partial contents of Block1 (e.g. Alpha within DXT5), as 16:16.
// We must return a 16:16:16:16 value consisting of two samples from one of these samplers.
float4 MergeBlocksPS( [unused] INTERPOLATORS In, float2 vUnnormalizedTexCoord : VPOS ) : COLOR
{
    float4 vTexel0, vTexel1;
    
    if( frac( 0.5f * vUnnormalizedTexCoord.x ) == 0.0f )
    {
        asm
        {
            tfetch2D vTexel0, vUnnormalizedTexCoord, samplerBlock0, OffsetX =  0.0, OffsetY =  0.0, \
                UnnormalizedTextureCoords = true, MinFilter = point, MagFilter = point, MipFilter = point
            tfetch2D vTexel1, vUnnormalizedTexCoord, samplerBlock0, OffsetX = +1.0, OffsetY =  0.0, \
                UnnormalizedTextureCoords = true, MinFilter = point, MagFilter = point, MipFilter = point
        };
    }
    else
    {
        asm
        {
            tfetch2D vTexel0, vUnnormalizedTexCoord, samplerBlock1, OffsetX = -1.0, OffsetY =  0.0, \
                UnnormalizedTextureCoords = true, MinFilter = point, MagFilter = point, MipFilter = point
            tfetch2D vTexel1, vUnnormalizedTexCoord, samplerBlock1, OffsetX =  0.0, OffsetY =  0.0, \
                UnnormalizedTextureCoords = true, MinFilter = point, MagFilter = point, MipFilter = point
        };
    }
    
    return float4( vTexel0.rg, vTexel1.rg );
}


//---------------------------------------------------------------------------------------------------------
// Name: LoadTexelsRGBA()
// Desc: Fetch the 16 source texels for a DXT block.
//---------------------------------------------------------------------------------------------------------
sampler samplerRaw          : register(s0);

void LoadTexelsRGBA( in float2 vTexCoord, out float4 vRGBA[16] )
{
    float4 vRGBARaw00, vRGBARaw01, vRGBARaw02, vRGBARaw03;
    float4 vRGBARaw10, vRGBARaw11, vRGBARaw12, vRGBARaw13;
    float4 vRGBARaw20, vRGBARaw21, vRGBARaw22, vRGBARaw23;
    float4 vRGBARaw30, vRGBARaw31, vRGBARaw32, vRGBARaw33;
    
    // point-sample a 4x4 block of values from the input texture
    // With [isolate], the assignments to registers are more predictable, 
    // and the fetch pattern may be friendlier to the memory controller.
    // At some times, [isolate] has made DXT1 & DXN performance slightly better, 
    // but currently (at ship time 01/2010), it exposes a compiler error for DXT5.
    //[isolate]
    asm
    {
    // If [isolate] is used, then the compiler will respect the fetch order.
    // At some times, performance has differed depending which order is used.
#define FETCH_BY_ROWS
#ifdef FETCH_BY_ROWS    
        tfetch2D vRGBARaw00, vTexCoord, samplerRaw, OffsetX = -1.5f, OffsetY = -1.5f, \
            MinFilter = point, MagFilter = point, MipFilter = point
        tfetch2D vRGBARaw10, vTexCoord, samplerRaw, OffsetX = -0.5f, OffsetY = -1.5f, \
            MinFilter = point, MagFilter = point, MipFilter = point
        tfetch2D vRGBARaw20, vTexCoord, samplerRaw, OffsetX = +0.5f, OffsetY = -1.5f, \
            MinFilter = point, MagFilter = point, MipFilter = point
        tfetch2D vRGBARaw30, vTexCoord, samplerRaw, OffsetX = +1.5f, OffsetY = -1.5f, \
            MinFilter = point, MagFilter = point, MipFilter = point
        tfetch2D vRGBARaw01, vTexCoord, samplerRaw, OffsetX = -1.5f, OffsetY = -0.5f, \
            MinFilter = point, MagFilter = point, MipFilter = point
        tfetch2D vRGBARaw11, vTexCoord, samplerRaw, OffsetX = -0.5f, OffsetY = -0.5f, \
            MinFilter = point, MagFilter = point, MipFilter = point
        tfetch2D vRGBARaw21, vTexCoord, samplerRaw, OffsetX = +0.5f, OffsetY = -0.5f, \
            MinFilter = point, MagFilter = point, MipFilter = point
        tfetch2D vRGBARaw31, vTexCoord, samplerRaw, OffsetX = +1.5f, OffsetY = -0.5f, \
            MinFilter = point, MagFilter = point, MipFilter = point
        tfetch2D vRGBARaw02, vTexCoord, samplerRaw, OffsetX = -1.5f, OffsetY = +0.5f, \
            MinFilter = point, MagFilter = point, MipFilter = point
        tfetch2D vRGBARaw12, vTexCoord, samplerRaw, OffsetX = -0.5f, OffsetY = +0.5f, \
            MinFilter = point, MagFilter = point, MipFilter = point
        tfetch2D vRGBARaw22, vTexCoord, samplerRaw, OffsetX = +0.5f, OffsetY = +0.5f, \
            MinFilter = point, MagFilter = point, MipFilter = point
        tfetch2D vRGBARaw32, vTexCoord, samplerRaw, OffsetX = +1.5f, OffsetY = +0.5f, \
            MinFilter = point, MagFilter = point, MipFilter = point
        tfetch2D vRGBARaw03, vTexCoord, samplerRaw, OffsetX = -1.5f, OffsetY = +1.5f, \
            MinFilter = point, MagFilter = point, MipFilter = point
        tfetch2D vRGBARaw13, vTexCoord, samplerRaw, OffsetX = -0.5f, OffsetY = +1.5f, \
            MinFilter = point, MagFilter = point, MipFilter = point
        tfetch2D vRGBARaw23, vTexCoord, samplerRaw, OffsetX = +0.5f, OffsetY = +1.5f, \
            MinFilter = point, MagFilter = point, MipFilter = point
        tfetch2D vRGBARaw33, vTexCoord, samplerRaw, OffsetX = +1.5f, OffsetY = +1.5f, \
            MinFilter = point, MagFilter = point, MipFilter = point
#else
        tfetch2D vRGBARaw00, vTexCoord, samplerRaw, OffsetX = -1.5f, OffsetY = -1.5f, \
            MinFilter = point, MagFilter = point, MipFilter = point
        tfetch2D vRGBARaw01, vTexCoord, samplerRaw, OffsetX = -1.5f, OffsetY = -0.5f, \
            MinFilter = point, MagFilter = point, MipFilter = point
        tfetch2D vRGBARaw02, vTexCoord, samplerRaw, OffsetX = -1.5f, OffsetY = +0.5f, \
            MinFilter = point, MagFilter = point, MipFilter = point
        tfetch2D vRGBARaw03, vTexCoord, samplerRaw, OffsetX = -1.5f, OffsetY = +1.5f, \
            MinFilter = point, MagFilter = point, MipFilter = point
        tfetch2D vRGBARaw10, vTexCoord, samplerRaw, OffsetX = -0.5f, OffsetY = -1.5f, \
            MinFilter = point, MagFilter = point, MipFilter = point
        tfetch2D vRGBARaw11, vTexCoord, samplerRaw, OffsetX = -0.5f, OffsetY = -0.5f, \
            MinFilter = point, MagFilter = point, MipFilter = point
        tfetch2D vRGBARaw12, vTexCoord, samplerRaw, OffsetX = -0.5f, OffsetY = +0.5f, \
            MinFilter = point, MagFilter = point, MipFilter = point
        tfetch2D vRGBARaw13, vTexCoord, samplerRaw, OffsetX = -0.5f, OffsetY = +1.5f, \
            MinFilter = point, MagFilter = point, MipFilter = point
        tfetch2D vRGBARaw20, vTexCoord, samplerRaw, OffsetX = +0.5f, OffsetY = -1.5f, \
            MinFilter = point, MagFilter = point, MipFilter = point
        tfetch2D vRGBARaw21, vTexCoord, samplerRaw, OffsetX = +0.5f, OffsetY = -0.5f, \
            MinFilter = point, MagFilter = point, MipFilter = point
        tfetch2D vRGBARaw22, vTexCoord, samplerRaw, OffsetX = +0.5f, OffsetY = +0.5f, \
            MinFilter = point, MagFilter = point, MipFilter = point
        tfetch2D vRGBARaw23, vTexCoord, samplerRaw, OffsetX = +0.5f, OffsetY = +1.5f, \
            MinFilter = point, MagFilter = point, MipFilter = point
        tfetch2D vRGBARaw30, vTexCoord, samplerRaw, OffsetX = +1.5f, OffsetY = -1.5f, \
            MinFilter = point, MagFilter = point, MipFilter = point
        tfetch2D vRGBARaw31, vTexCoord, samplerRaw, OffsetX = +1.5f, OffsetY = -0.5f, \
            MinFilter = point, MagFilter = point, MipFilter = point
        tfetch2D vRGBARaw32, vTexCoord, samplerRaw, OffsetX = +1.5f, OffsetY = +0.5f, \
            MinFilter = point, MagFilter = point, MipFilter = point
        tfetch2D vRGBARaw33, vTexCoord, samplerRaw, OffsetX = +1.5f, OffsetY = +1.5f, \
            MinFilter = point, MagFilter = point, MipFilter = point
#endif
    };
    vRGBA[ 0] = vRGBARaw00;
    vRGBA[ 1] = vRGBARaw10;
    vRGBA[ 2] = vRGBARaw20;
    vRGBA[ 3] = vRGBARaw30;
    vRGBA[ 4] = vRGBARaw01;
    vRGBA[ 5] = vRGBARaw11;
    vRGBA[ 6] = vRGBARaw21;
    vRGBA[ 7] = vRGBARaw31;
    vRGBA[ 8] = vRGBARaw02;
    vRGBA[ 9] = vRGBARaw12;
    vRGBA[10] = vRGBARaw22;
    vRGBA[11] = vRGBARaw32;
    vRGBA[12] = vRGBARaw03;
    vRGBA[13] = vRGBARaw13;
    vRGBA[14] = vRGBARaw23;
    vRGBA[15] = vRGBARaw33;
}


//---------------------------------------------------------------------------------------------------------
// Name: LoadTexelsUV()
// Desc: Fetch the 16 source texels for a DXT block.
//---------------------------------------------------------------------------------------------------------
void LoadTexelsUV( in float2 vTexCoord, out float2 vUV[16] )
{
    float4 vUVUVRaw0010, vUVUVRaw2030;
    float4 vUVUVRaw0111, vUVUVRaw2131;
    float4 vUVUVRaw0212, vUVUVRaw2232;
    float4 vUVUVRaw0313, vUVUVRaw2333;
    
    // point-sample a 4x4 block of values from the input texture
    // With [isolate], the assignments to registers are more predictable, 
    // and the fetch pattern may be friendlier to the memory controller.
    // Currently, [isolate] makes DXT1 & DXN performance slightly better, 
    // but causes a bogus compilation error for DXT5.
    //[isolate]
    asm
    {
        tfetch2D vUVUVRaw0010, vTexCoord, samplerRaw, OffsetX = -0.5f, OffsetY = -1.5f, \
            MinFilter = point, MagFilter = point, MipFilter = point
        tfetch2D vUVUVRaw2030, vTexCoord, samplerRaw, OffsetX = +0.5f, OffsetY = -1.5f, \
            MinFilter = point, MagFilter = point, MipFilter = point
        tfetch2D vUVUVRaw0111, vTexCoord, samplerRaw, OffsetX = -0.5f, OffsetY = -0.5f, \
            MinFilter = point, MagFilter = point, MipFilter = point
        tfetch2D vUVUVRaw2131, vTexCoord, samplerRaw, OffsetX = +0.5f, OffsetY = -0.5f, \
            MinFilter = point, MagFilter = point, MipFilter = point
        tfetch2D vUVUVRaw0212, vTexCoord, samplerRaw, OffsetX = -0.5f, OffsetY = +0.5f, \
            MinFilter = point, MagFilter = point, MipFilter = point
        tfetch2D vUVUVRaw2232, vTexCoord, samplerRaw, OffsetX = +0.5f, OffsetY = +0.5f, \
            MinFilter = point, MagFilter = point, MipFilter = point
        tfetch2D vUVUVRaw0313, vTexCoord, samplerRaw, OffsetX = -0.5f, OffsetY = +1.5f, \
            MinFilter = point, MagFilter = point, MipFilter = point
        tfetch2D vUVUVRaw2333, vTexCoord, samplerRaw, OffsetX = +0.5f, OffsetY = +1.5f, \
            MinFilter = point, MagFilter = point, MipFilter = point
    };
    vUV[ 0] = vUVUVRaw0010.xy;
    vUV[ 1] = vUVUVRaw0010.zw;
    vUV[ 2] = vUVUVRaw2030.xy;
    vUV[ 3] = vUVUVRaw2030.zw;
    vUV[ 4] = vUVUVRaw0111.xy;
    vUV[ 5] = vUVUVRaw0111.zw;
    vUV[ 6] = vUVUVRaw2131.xy;
    vUV[ 7] = vUVUVRaw2131.zw;
    vUV[ 8] = vUVUVRaw0212.xy;
    vUV[ 9] = vUVUVRaw0212.zw;
    vUV[10] = vUVUVRaw2232.xy;
    vUV[11] = vUVUVRaw2232.zw;
    vUV[12] = vUVUVRaw0313.xy;
    vUV[13] = vUVUVRaw0313.zw;
    vUV[14] = vUVUVRaw2333.xy;
    vUV[15] = vUVUVRaw2333.zw;
}


//---------------------------------------------------------------------------------------------------------
// Name: FindMinMaxRGBA()
// Desc: Find the component-wise min and max of 16 RGBA vectors.
//---------------------------------------------------------------------------------------------------------
void FindMinMaxRGBA( float4 vRGBA[16], out float4 vMinRGBA, out float4 vMaxRGBA )
{
    // Find the axis-aligned bounding box in color space.
    vMinRGBA = vRGBA[0];
    vMaxRGBA = vRGBA[0];
    for( int i = 0; i < 16; ++i )   // The compiler removes the extra instructions for i == 0
    {
        vMinRGBA = min( vMinRGBA, vRGBA[i] );
        vMaxRGBA = max( vMaxRGBA, vRGBA[i] );
    }
}


//---------------------------------------------------------------------------------------------------------
// Name: FindMinMaxRGBA()
// Desc: Find the component-wise min and max of 16 UV vectors.
//---------------------------------------------------------------------------------------------------------
void FindMinMaxUV( float2 vUV[16], out float2 vMinUV, out float2 vMaxUV )
{
    // Find the axis-aligned bounding box in UV space.
    // This formulation gives best vector min/max op usage.
    float2 vMinUV0 = min( min( vUV[ 0], vUV[ 4] ), min( vUV[ 8], vUV[12] ) );
    float2 vMinUV1 = min( min( vUV[ 1], vUV[ 5] ), min( vUV[ 9], vUV[13] ) );
    float2 vMinUV2 = min( min( vUV[ 2], vUV[ 6] ), min( vUV[10], vUV[14] ) );
    float2 vMinUV3 = min( min( vUV[ 3], vUV[ 7] ), min( vUV[11], vUV[15] ) );
    vMinUV = min( min( vMinUV0, vMinUV1 ), min( vMinUV2, vMinUV3 ) );
    float2 vMaxUV0 = max( max( vUV[ 0], vUV[ 4] ), max( vUV[ 8], vUV[12] ) );
    float2 vMaxUV1 = max( max( vUV[ 1], vUV[ 5] ), max( vUV[ 9], vUV[13] ) );
    float2 vMaxUV2 = max( max( vUV[ 2], vUV[ 6] ), max( vUV[10], vUV[14] ) );
    float2 vMaxUV3 = max( max( vUV[ 3], vUV[ 7] ), max( vUV[11], vUV[15] ) );
    vMaxUV = max( max( vMaxUV0, vMaxUV1 ), max( vMaxUV2, vMaxUV3 ) );
}

    
//---------------------------------------------------------------------------------------------------------
// Name: EncodeDXT1Internal()
// Desc: Given 16 RGBA vectors, and their component-wise min and max, returns a 16:16:16:16 unsigned
// integer vector which is bitwise equal to a 64-bit DXT1 block.
//---------------------------------------------------------------------------------------------------------
float4 EncodeDXT1Internal( float4 vRGBA[16], float3 vMinRGBA, float3 vMaxRGBA )
{
#ifdef STUB_COMPRESSION
    return vMinRGBA.x;    // Force retention of texture fetches
#endif

    float3 vCenter = 0.5f * ( vMinRGBA + vMaxRGBA );
    
    // Anchors are endpoints of one of the two diagonals of the bounding box.  Which 
    // diagonal is determined by the sign of the covariances.
    // The actual covariance uses the mean, but we substitute the center.  
    float fCovarianceRG = 0;
    float fCovarianceBG = 0;
    if( g_bAnchorChoice )
    {
        for( int i = 0; i < 16; ++i )
        {
            float3 vDiffCenter = vRGBA[i] - vCenter;

            fCovarianceRG += vDiffCenter.x * vDiffCenter.y;
            fCovarianceBG += vDiffCenter.z * vDiffCenter.y;
        }
    }

    float3 vAnchor[2];
    vAnchor[0].x = ( fCovarianceRG >= 0.0f ) ? vMaxRGBA.x : vMinRGBA.x;
    vAnchor[0].y = vMaxRGBA.y;
    vAnchor[0].z = ( fCovarianceBG >= 0.0f ) ? vMaxRGBA.z : vMinRGBA.z;
    vAnchor[1] = ( vMinRGBA + vMaxRGBA ) - vAnchor[0];
    
    float3 vRGBIncr = float3( 31.0f, 63.0f, 31.0f );
    float3 vRGBShift = float3( 32.0f, 64.0f, 32.0f );
    float3 vAnchor565[2];
    float fPackedAnchor565[2];
    vAnchor565[0] = vAnchor[0];
    vAnchor565[0] *= vRGBIncr;
    vAnchor565[0] = round( vAnchor565[0] );
    fPackedAnchor565[0] = vAnchor565[0].b + vRGBShift.b * ( vAnchor565[0].g + vRGBShift.g * vAnchor565[0].r );
    vAnchor565[1] = vAnchor[1];
    vAnchor565[1] *= vRGBIncr;
    vAnchor565[1] = round( vAnchor565[1] );
    fPackedAnchor565[1] = vAnchor565[1].b + vRGBShift.b * ( vAnchor565[1].g + vRGBShift.g * vAnchor565[1].r );
    
    // Handle case where anchors are mis-ordered.  This can only happen when we 
    // allow multiple anchor choices
    if( g_bAnchorChoice )
    {
        [flatten]
        if( fPackedAnchor565[0] < fPackedAnchor565[1] )
        {
            float fTemp = fPackedAnchor565[0];
            fPackedAnchor565[0] = fPackedAnchor565[1];
            fPackedAnchor565[1] = fTemp;
            
            float3 vTemp = vAnchor[0];
            vAnchor[0] = vAnchor[1];
            vAnchor[1] = vTemp;
        }
    }
    
    // For each input color, find the closest representable step along the line joining the anchors.
    float3 vDiag = vAnchor[1] - vAnchor[0];
    float fStepInc = 3.0f / dot( vDiag, vDiag );
    float fPaletteIndices[16];
    fPaletteIndices[ 0] = round( dot( vRGBA[ 0] - vAnchor[0], vDiag ) * fStepInc );
    fPaletteIndices[ 1] = round( dot( vRGBA[ 1] - vAnchor[0], vDiag ) * fStepInc );
    fPaletteIndices[ 2] = round( dot( vRGBA[ 2] - vAnchor[0], vDiag ) * fStepInc );
    fPaletteIndices[ 3] = round( dot( vRGBA[ 3] - vAnchor[0], vDiag ) * fStepInc );
    fPaletteIndices[ 4] = round( dot( vRGBA[ 4] - vAnchor[0], vDiag ) * fStepInc );
    fPaletteIndices[ 5] = round( dot( vRGBA[ 5] - vAnchor[0], vDiag ) * fStepInc );
    fPaletteIndices[ 6] = round( dot( vRGBA[ 6] - vAnchor[0], vDiag ) * fStepInc );
    fPaletteIndices[ 7] = round( dot( vRGBA[ 7] - vAnchor[0], vDiag ) * fStepInc );
    fPaletteIndices[ 8] = round( dot( vRGBA[ 8] - vAnchor[0], vDiag ) * fStepInc );
    fPaletteIndices[ 9] = round( dot( vRGBA[ 9] - vAnchor[0], vDiag ) * fStepInc );
    fPaletteIndices[10] = round( dot( vRGBA[10] - vAnchor[0], vDiag ) * fStepInc );
    fPaletteIndices[11] = round( dot( vRGBA[11] - vAnchor[0], vDiag ) * fStepInc );
    fPaletteIndices[12] = round( dot( vRGBA[12] - vAnchor[0], vDiag ) * fStepInc );
    fPaletteIndices[13] = round( dot( vRGBA[13] - vAnchor[0], vDiag ) * fStepInc );
    fPaletteIndices[14] = round( dot( vRGBA[14] - vAnchor[0], vDiag ) * fStepInc );
    fPaletteIndices[15] = round( dot( vRGBA[15] - vAnchor[0], vDiag ) * fStepInc );
    
    // Remap palette indices according to DXT1 convention
    fPaletteIndices[ 0] = ( fPaletteIndices[ 0] == 0.0f ) ? 0.0f : ( fPaletteIndices[ 0] == 3.0f ) ? 1.0f : ( fPaletteIndices[ 0] + 1 );
    fPaletteIndices[ 1] = ( fPaletteIndices[ 1] == 0.0f ) ? 0.0f : ( fPaletteIndices[ 1] == 3.0f ) ? 1.0f : ( fPaletteIndices[ 1] + 1 );
    fPaletteIndices[ 2] = ( fPaletteIndices[ 2] == 0.0f ) ? 0.0f : ( fPaletteIndices[ 2] == 3.0f ) ? 1.0f : ( fPaletteIndices[ 2] + 1 );
    fPaletteIndices[ 3] = ( fPaletteIndices[ 3] == 0.0f ) ? 0.0f : ( fPaletteIndices[ 3] == 3.0f ) ? 1.0f : ( fPaletteIndices[ 3] + 1 );
    fPaletteIndices[ 4] = ( fPaletteIndices[ 4] == 0.0f ) ? 0.0f : ( fPaletteIndices[ 4] == 3.0f ) ? 1.0f : ( fPaletteIndices[ 4] + 1 );
    fPaletteIndices[ 5] = ( fPaletteIndices[ 5] == 0.0f ) ? 0.0f : ( fPaletteIndices[ 5] == 3.0f ) ? 1.0f : ( fPaletteIndices[ 5] + 1 );
    fPaletteIndices[ 6] = ( fPaletteIndices[ 6] == 0.0f ) ? 0.0f : ( fPaletteIndices[ 6] == 3.0f ) ? 1.0f : ( fPaletteIndices[ 6] + 1 );
    fPaletteIndices[ 7] = ( fPaletteIndices[ 7] == 0.0f ) ? 0.0f : ( fPaletteIndices[ 7] == 3.0f ) ? 1.0f : ( fPaletteIndices[ 7] + 1 );
    fPaletteIndices[ 8] = ( fPaletteIndices[ 8] == 0.0f ) ? 0.0f : ( fPaletteIndices[ 8] == 3.0f ) ? 1.0f : ( fPaletteIndices[ 8] + 1 );
    fPaletteIndices[ 9] = ( fPaletteIndices[ 9] == 0.0f ) ? 0.0f : ( fPaletteIndices[ 9] == 3.0f ) ? 1.0f : ( fPaletteIndices[ 9] + 1 );
    fPaletteIndices[10] = ( fPaletteIndices[10] == 0.0f ) ? 0.0f : ( fPaletteIndices[10] == 3.0f ) ? 1.0f : ( fPaletteIndices[10] + 1 );
    fPaletteIndices[11] = ( fPaletteIndices[11] == 0.0f ) ? 0.0f : ( fPaletteIndices[11] == 3.0f ) ? 1.0f : ( fPaletteIndices[11] + 1 );
    fPaletteIndices[12] = ( fPaletteIndices[12] == 0.0f ) ? 0.0f : ( fPaletteIndices[12] == 3.0f ) ? 1.0f : ( fPaletteIndices[12] + 1 );
    fPaletteIndices[13] = ( fPaletteIndices[13] == 0.0f ) ? 0.0f : ( fPaletteIndices[13] == 3.0f ) ? 1.0f : ( fPaletteIndices[13] + 1 );
    fPaletteIndices[14] = ( fPaletteIndices[14] == 0.0f ) ? 0.0f : ( fPaletteIndices[14] == 3.0f ) ? 1.0f : ( fPaletteIndices[14] + 1 );
    fPaletteIndices[15] = ( fPaletteIndices[15] == 0.0f ) ? 0.0f : ( fPaletteIndices[15] == 3.0f ) ? 1.0f : ( fPaletteIndices[15] + 1 );
    
    // Pack 32 2-bit palette indices into 2 16-bit unsigned integers
    float fPackedIndices[2];
    [flatten]
    if( fPackedAnchor565[0] == fPackedAnchor565[1] )
    {
        // Handle case where anchors are equal
        fPackedIndices[0] = fPackedIndices[1] = 0.0f; 
    }
    else
    {
        fPackedIndices[0] =  fPaletteIndices[ 0] + 
                    4.0f * ( fPaletteIndices[ 1] + 
                    4.0f * ( fPaletteIndices[ 2] + 
                    4.0f * ( fPaletteIndices[ 3] + 
                    4.0f * ( fPaletteIndices[ 4] + 
                    4.0f * ( fPaletteIndices[ 5] + 
                    4.0f * ( fPaletteIndices[ 6] + 
                    4.0f * ( fPaletteIndices[ 7] ))))))); 
        fPackedIndices[1] =  fPaletteIndices[ 8] + 
                    4.0f * ( fPaletteIndices[ 9] + 
                    4.0f * ( fPaletteIndices[10] + 
                    4.0f * ( fPaletteIndices[11] + 
                    4.0f * ( fPaletteIndices[12] + 
                    4.0f * ( fPaletteIndices[13] + 
                    4.0f * ( fPaletteIndices[14] + 
                    4.0f * ( fPaletteIndices[15] ))))))); 
    }
        
    return float4( fPackedAnchor565[0], fPackedAnchor565[1], fPackedIndices[0], fPackedIndices[1] );
}


//---------------------------------------------------------------------------------------------------------
// Name: EncodeCTX1Internal()
// Desc: Given 16 UV vectors, and their component-wise min and max, returns a 16:16:16:16 unsigned
// integer vector which is bitwise equal to a 64-bit CTX1 block.
//---------------------------------------------------------------------------------------------------------
float4 EncodeCTX1Internal( float2 vUV[16], float2 vMinUV, float2 vMaxUV )
{
#ifdef STUB_COMPRESSION
    return vMinUV.x;    // Force retention of texture fetches
#endif

    float2 vCenter = 0.5f * ( vMinUV + vMaxUV );
    
    // Anchors are endpoints of one of the two diagonals of the bounding box.  Which 
    // diagonal is determined by the sign of the covariance.
    // The actual covariance uses the mean, but we substitute the center.  Difference 
    // of 12 ALU slots, for little apparent change in quality.  
    float fCovariance = 0;
    for( int i = 0; i < 6; ++i )
    {
        float2 vDiffCenter = vUV[i] - vCenter;

        fCovariance += vDiffCenter.x * vDiffCenter.y;
    }
    bool bSwap = ( fCovariance < 0.0f );

    float2 vAnchor[2];
    vAnchor[0] = bSwap ? float2( vMinUV.x, vMaxUV.y ) : vMinUV;
    vAnchor[1] = ( vMinUV + vMaxUV ) - vAnchor[0];

    float fPackedAnchor88[2];
    for( int k = 0; k < 2; ++k )
    {
        float2 vAnchor88 = round( 255.0f * vAnchor[k] );
        fPackedAnchor88[k] = vAnchor88.y + 256.0f * vAnchor88.x;
    }
    
    // For each input color, find the closest representable step along the line joining the anchors.
    float2 vDiag = vAnchor[1] - vAnchor[0];
    float fDots[16];
    [unroll]
    for( int i = 0; i < 16; ++i )
    {
        float2 vDiff = vUV[i] - vAnchor[0];
        fDots[i] = dot( vDiff, vDiag );
    }
    
    // For each input color, find the closest representable step along the line joining the anchors.
    float fStepInc = 3.0f / dot( vDiag, vDiag );
    float fPaletteIndices[16];
    fPaletteIndices[ 0] = round( fDots[ 0] * fStepInc );
    fPaletteIndices[ 1] = round( fDots[ 1] * fStepInc );
    fPaletteIndices[ 2] = round( fDots[ 2] * fStepInc );
    fPaletteIndices[ 3] = round( fDots[ 3] * fStepInc );
    fPaletteIndices[ 4] = round( fDots[ 4] * fStepInc );
    fPaletteIndices[ 5] = round( fDots[ 5] * fStepInc );
    fPaletteIndices[ 6] = round( fDots[ 6] * fStepInc );
    fPaletteIndices[ 7] = round( fDots[ 7] * fStepInc );
    fPaletteIndices[ 8] = round( fDots[ 8] * fStepInc );
    fPaletteIndices[ 9] = round( fDots[ 9] * fStepInc );
    fPaletteIndices[10] = round( fDots[10] * fStepInc );
    fPaletteIndices[11] = round( fDots[11] * fStepInc );
    fPaletteIndices[12] = round( fDots[12] * fStepInc );
    fPaletteIndices[13] = round( fDots[13] * fStepInc );
    fPaletteIndices[14] = round( fDots[14] * fStepInc );
    fPaletteIndices[15] = round( fDots[15] * fStepInc );
    
    // Remap palette indices according to CTX1 convention
    [unroll]
    for( int i = 0; i < 16; ++i )
    {
        float fIndex = fPaletteIndices[i];
        fPaletteIndices[i] = ( fIndex == 0.0f ) 
            ? 0.0f
            : ( fIndex == 3.0f )
                ? 1.0f
                : ( fIndex + 1 );
    }
    
    // Pack 32 2-bit palette indices into 2 16-bit unsigned integers
    float fPackedIndices[2];
    fPackedIndices[0] =  fPaletteIndices[ 0] + 
                4.0f * ( fPaletteIndices[ 1] + 
                4.0f * ( fPaletteIndices[ 2] + 
                4.0f * ( fPaletteIndices[ 3] + 
                4.0f * ( fPaletteIndices[ 4] + 
                4.0f * ( fPaletteIndices[ 5] + 
                4.0f * ( fPaletteIndices[ 6] + 
                4.0f * ( fPaletteIndices[ 7] ))))))); 
    fPackedIndices[1] =  fPaletteIndices[ 8] + 
                4.0f * ( fPaletteIndices[ 9] + 
                4.0f * ( fPaletteIndices[10] + 
                4.0f * ( fPaletteIndices[11] + 
                4.0f * ( fPaletteIndices[12] + 
                4.0f * ( fPaletteIndices[13] + 
                4.0f * ( fPaletteIndices[14] + 
                4.0f * ( fPaletteIndices[15] ))))))); 
        
    return float4( fPackedAnchor88[0], fPackedAnchor88[1], fPackedIndices[0], fPackedIndices[1] );
}


//---------------------------------------------------------------------------------------------------------
// Name: ExtractBits()
// Desc: Code to extract 16 bits from an 18-bit integer.
//---------------------------------------------------------------------------------------------------------
// Float has 24 bits of accuracy.
#define power_2_0	1
#define power_2_1	(2 * power_2_0)
#define power_2_2	(2 * power_2_1)	
#define power_2_3	(2 * power_2_2)	
#define power_2_4	(2 * power_2_3)	
#define power_2_5	(2 * power_2_4)	
#define power_2_6	(2 * power_2_5)	
#define power_2_7	(2 * power_2_6)	
#define power_2_8	(2 * power_2_7)	
#define power_2_9	(2 * power_2_8)	
#define power_2_10	(2 * power_2_9)	
#define power_2_11	(2 * power_2_10)	
#define power_2_12	(2 * power_2_11)	
#define power_2_13	(2 * power_2_12)	
#define power_2_14	(2 * power_2_13)	
#define power_2_15	(2 * power_2_14)	
#define power_2_16	(2 * power_2_15)	
#define power_2_17	(2 * power_2_16)	
#define power_2_18	(2 * power_2_17)	

#define EXTRACT_BITS(bitfield, lo_bit, hi_bit) ExtractBits(bitfield, power_2_##lo_bit, power_2_##hi_bit)
float ExtractBits(float bitfield, int lo_power /*const power of 2*/, int hi_power /*const power of 2*/)
{
	float result = bitfield;	// calling this an 'int' adds an unnecessary 'truncs'
	if (lo_power != power_2_0 /*2^0 compile time test*/)
	{
		// Should be 2 instructions: mad, floors
		result /= lo_power;
		result = floor(result);
	}
	if (hi_power != power_2_18 /*2^18 compile time test*/)
	{
		// Should be 3 instructions: mulsc, frcs, mulsc
		result /= (hi_power/lo_power);
		result = frac(result);
		result *= (hi_power/lo_power);
	}
	return result;
}


//---------------------------------------------------------------------------------------------------------
// Name: EncodeDXT5AInternal()
// Desc: Given 16 RGBA vectors, and their component-wise min and max, returns a 16:16:16:16 unsigned
// integer vector which is bitwise equal to a 64-bit DXT5A block.  
//
// The iChannel input determines which of the 4 channels is interpreted as A.
//---------------------------------------------------------------------------------------------------------
float4 EncodeDXT5AInternal( float fAlpha[16], float fMin, float fMax )
{
#ifdef STUB_COMPRESSION
    return fMin;    // Force retention of texture fetches
#endif

    float fIntMin = round( 255.0f * fMin );
    float fIntMax = round( 255.0f * fMax );
    float fPackedAnchors = fIntMax + 256.0f * fIntMin;
    
    // For each input color, find the closest representable step along the line joining the anchors.
    float fStepInc = 7.0f / ( fMax - fMin );
    float fPaletteIndices[16];
    [unroll]
    for( int i = 0; i < 16; ++i )
    {
        fPaletteIndices[i] = round( ( fMax - fAlpha[i] ) * fStepInc );
    }

    // Remap palette indices according to DXT5A convention
    // [8 vector ops]
    fPaletteIndices[ 0] = ( fPaletteIndices[ 0] == 0.0f ) ? 0.0f : ( fPaletteIndices[ 0] == 7.0f ) ? 1.0f : ( fPaletteIndices[ 0] + 1 );
    fPaletteIndices[ 1] = ( fPaletteIndices[ 1] == 0.0f ) ? 0.0f : ( fPaletteIndices[ 1] == 7.0f ) ? 1.0f : ( fPaletteIndices[ 1] + 1 );
    fPaletteIndices[ 2] = ( fPaletteIndices[ 2] == 0.0f ) ? 0.0f : ( fPaletteIndices[ 2] == 7.0f ) ? 1.0f : ( fPaletteIndices[ 2] + 1 );
    fPaletteIndices[ 3] = ( fPaletteIndices[ 3] == 0.0f ) ? 0.0f : ( fPaletteIndices[ 3] == 7.0f ) ? 1.0f : ( fPaletteIndices[ 3] + 1 );
    fPaletteIndices[ 4] = ( fPaletteIndices[ 4] == 0.0f ) ? 0.0f : ( fPaletteIndices[ 4] == 7.0f ) ? 1.0f : ( fPaletteIndices[ 4] + 1 );
    fPaletteIndices[ 5] = ( fPaletteIndices[ 5] == 0.0f ) ? 0.0f : ( fPaletteIndices[ 5] == 7.0f ) ? 1.0f : ( fPaletteIndices[ 5] + 1 );
    fPaletteIndices[ 6] = ( fPaletteIndices[ 6] == 0.0f ) ? 0.0f : ( fPaletteIndices[ 6] == 7.0f ) ? 1.0f : ( fPaletteIndices[ 6] + 1 );
    fPaletteIndices[ 7] = ( fPaletteIndices[ 7] == 0.0f ) ? 0.0f : ( fPaletteIndices[ 7] == 7.0f ) ? 1.0f : ( fPaletteIndices[ 7] + 1 );
    fPaletteIndices[ 8] = ( fPaletteIndices[ 8] == 0.0f ) ? 0.0f : ( fPaletteIndices[ 8] == 7.0f ) ? 1.0f : ( fPaletteIndices[ 8] + 1 );
    fPaletteIndices[ 9] = ( fPaletteIndices[ 9] == 0.0f ) ? 0.0f : ( fPaletteIndices[ 9] == 7.0f ) ? 1.0f : ( fPaletteIndices[ 9] + 1 );
    fPaletteIndices[10] = ( fPaletteIndices[10] == 0.0f ) ? 0.0f : ( fPaletteIndices[10] == 7.0f ) ? 1.0f : ( fPaletteIndices[10] + 1 );
    fPaletteIndices[11] = ( fPaletteIndices[11] == 0.0f ) ? 0.0f : ( fPaletteIndices[11] == 7.0f ) ? 1.0f : ( fPaletteIndices[11] + 1 );
    fPaletteIndices[12] = ( fPaletteIndices[12] == 0.0f ) ? 0.0f : ( fPaletteIndices[12] == 7.0f ) ? 1.0f : ( fPaletteIndices[12] + 1 );
    fPaletteIndices[13] = ( fPaletteIndices[13] == 0.0f ) ? 0.0f : ( fPaletteIndices[13] == 7.0f ) ? 1.0f : ( fPaletteIndices[13] + 1 );
    fPaletteIndices[14] = ( fPaletteIndices[14] == 0.0f ) ? 0.0f : ( fPaletteIndices[14] == 7.0f ) ? 1.0f : ( fPaletteIndices[14] + 1 );
    fPaletteIndices[15] = ( fPaletteIndices[15] == 0.0f ) ? 0.0f : ( fPaletteIndices[15] == 7.0f ) ? 1.0f : ( fPaletteIndices[15] + 1 );
    
    // Pack 32 2-bit palette indices into 2 16-bit unsigned integers
    // First put 6 3-bit indices into each field (overshooting)
    float fPackedIndices[3];
    fPackedIndices[0] =  fPaletteIndices[ 0] + 
                8.0f * ( fPaletteIndices[ 1] + 
                8.0f * ( fPaletteIndices[ 2] + 
                8.0f * ( fPaletteIndices[ 3] + 
                8.0f * ( fPaletteIndices[ 4] + 
                8.0f * ( fPaletteIndices[ 5] ))))); 
    fPackedIndices[1] =  fPaletteIndices[ 5] + 
                8.0f * ( fPaletteIndices[ 6] + 
                8.0f * ( fPaletteIndices[ 7] + 
                8.0f * ( fPaletteIndices[ 8] + 
                8.0f * ( fPaletteIndices[ 9] + 
                8.0f * ( fPaletteIndices[10] ))))); 
    fPackedIndices[2] =  fPaletteIndices[10] + 
                8.0f * ( fPaletteIndices[11] + 
                8.0f * ( fPaletteIndices[12] + 
                8.0f * ( fPaletteIndices[13] + 
                8.0f * ( fPaletteIndices[14] + 
                8.0f * ( fPaletteIndices[15] ))))); 
            
    // Now select the appropriate 16 out of 18 bits from each field    
    fPackedIndices[0] = EXTRACT_BITS(fPackedIndices[0], 0, 16);
    fPackedIndices[1] = EXTRACT_BITS(fPackedIndices[1], 1, 17);
    fPackedIndices[2] = EXTRACT_BITS(fPackedIndices[2], 2, 18);

    return float4( fPackedAnchors, fPackedIndices[0], fPackedIndices[1], fPackedIndices[2] );
}


//---------------------------------------------------------------------------------------------------------
// Name: ReinterpretCastUnsignedToSigned_16_16_16_16()
// Desc: Given a 16:16:16:16 unsigned integer vector, find a 16:16:16:16 signed integer vector, which is
// bitwise equivalent.
//---------------------------------------------------------------------------------------------------------
float4 ReinterpretCastUnsignedToSigned_16_16_16_16( float4 vUnsigned )
{
    // Return 4 signed 16-bit integers which are bitwise equivalent to the unsigned integer data.
    // This conversion is required for EDRAM writes, since the 64-bit render targets are all signed.
    // For the memexport pathway, it's slightly faster to simply use a natively unsigned type.
    // [2 vector ops]
    return vUnsigned - ( ( vUnsigned >= 32768.0f ) ? 65536.0f : 0.0f ); 
}
    

//---------------------------------------------------------------------------------------------------------
// Name: GetTiledOffset2D()
// Desc: Incoming vTexCoordInTiles are texcoords in units of tiles.  This means: 
//
//     vTexCoordInTiles = 1/32 * vTexCoordUnnormalized
//
// where vTexCoordUnnormalized is in integer texel units.
// This function find the offset in texels of these texcoords in a tiled texture.
//---------------------------------------------------------------------------------------------------------
sampler samplerTilingPattern    : register(s1);
float2 g_vTilePitchUV           : register(c0);

float GetTiledOffset2D( float2 vTexCoordInTiles )
{
    float4 fTileOffset;
    asm
    {
        tfetch2D fTileOffset, vTexCoordInTiles, samplerTilingPattern, \
            MinFilter = point, MagFilter = point, MipFilter = point
    };
    return dot( g_vTilePitchUV, floor( vTexCoordInTiles ) ) + fTileOffset.x;
}


//---------------------------------------------------------------------------------------------------------
// Name: MemExport64
// Desc: Write a float4 out to memory as a 16:16:16:16 unsigned integer 
//---------------------------------------------------------------------------------------------------------
static float4 const01               = float4( 0, 1, 0, 0 );
float4 g_fMemExportStreamConstant   : register(c1);

void MemExport64( float2 vTexCoordInTiles, float4 vData )
{
    float fTiledOffset = GetTiledOffset2D( vTexCoordInTiles );
    asm
    {
        alloc export=1
        mad eA, fTiledOffset, const01, g_fMemExportStreamConstant
        mov eM0, vData
    };
}


//---------------------------------------------------------------------------------------------------------
// Name: MemExport128
// Desc: Write two float4's out to memory as 16:16:16:16 unsigned integers 
//---------------------------------------------------------------------------------------------------------
void MemExport128( float2 vTexCoordInTiles, float4 vData0, float4 vData1 )
{
    float fTiledOffset = GetTiledOffset2D( vTexCoordInTiles );
    fTiledOffset *= 2.0f;
    asm
    {
        alloc export=2
        mad eA, fTiledOffset, const01, g_fMemExportStreamConstant
        mov eM0, vData0
        mov eM1, vData1
    };
}


//---------------------------------------------------------------------------------------------------------
// Name: TileTextureMemexport64()
// Desc: Tiles a 64-bit texture. 
//---------------------------------------------------------------------------------------------------------
float4 TileMemexport64( INTERPOLATORS In ) : COLOR0
{
    float2 vTexCoord = In.vTexCoord.xy;
    float4 vColor0;
    asm
    {
        tfetch2D vColor0, vTexCoord, samplerRaw, \
            MinFilter = point, MagFilter = point, MipFilter = point
    };
    MemExport64( In.vTexCoord.zw, vColor0 );
    
    return 0.0f;
}


//---------------------------------------------------------------------------------------------------------
// Name: TileTextureMemexport128()
// Desc: Tiles a 128-bit texture. 
//---------------------------------------------------------------------------------------------------------
float4 TileMemexport128( INTERPOLATORS In ) : COLOR0
{
    float2 vTexCoord = In.vTexCoord.xy;
    float4 vColor0, vColor1;
    asm
    {
        tfetch2D vColor0, vTexCoord, samplerRaw, OffsetX =  -0.5f, OffsetY =  0.0f, \
            MinFilter = point, MagFilter = point, MipFilter = point
        tfetch2D vColor1, vTexCoord, samplerRaw, OffsetX =  +0.5f, OffsetY =  0.0f, \
            MinFilter = point, MagFilter = point, MipFilter = point
    };
    MemExport128( In.vTexCoord.zw, vColor0, vColor1 );
    
    return 0.0f;
}


//---------------------------------------------------------------------------------------------------------
// Pixel shader output declaration for a 64-bit DXT block
//---------------------------------------------------------------------------------------------------------
struct OUT64
{
    float4 vColor0 : COLOR0;
};

//---------------------------------------------------------------------------------------------------------
// Pixel shader output declaration for a 128-bit DXT block
//---------------------------------------------------------------------------------------------------------
struct OUT128
{
    float4 vColor0 : COLOR0;
    float4 vColor1 : COLOR1;
};


//---------------------------------------------------------------------------------------------------------
// Name: SplitTexture()
// Desc: Takes a 128-bit-per-block input and writes it out as 2 separate 64-bit chunks.  This 
// is a step in tiling, since the GPU has no 128-bit render target types. 
//---------------------------------------------------------------------------------------------------------
OUT128 SplitTexture( INTERPOLATORS In )
{
    OUT128 Out;
    float2 vTexCoord = In.vTexCoord.xy;
    float4 vColor0, vColor1;
    asm
    {
        tfetch2D vColor0, vTexCoord, samplerRaw, OffsetX =  -0.5f, OffsetY =  0.0f, \
            MinFilter = point, MagFilter = point, MipFilter = point
        tfetch2D vColor1, vTexCoord, samplerRaw, OffsetX =  +0.5f, OffsetY =  0.0f, \
            MinFilter = point, MagFilter = point, MipFilter = point
    };
    Out.vColor0 = vColor0;
    Out.vColor1 = vColor1;
    
    return Out;
}


//---------------------------------------------------------------------------------------------------------
// Name: EncodeDXT1()
// Desc: Entrypoint for encoding a DXT1 texture, either via EDRAM or memexport. 
//---------------------------------------------------------------------------------------------------------
OUT64 EncodeDXT1( INTERPOLATORS In )
{
    OUT64 Out;
    float4 vRGBA[16];
    LoadTexelsRGBA( In.vTexCoord.xy, vRGBA );
    
    float4 vMinRGBA, vMaxRGBA;
    FindMinMaxRGBA( vRGBA, vMinRGBA, vMaxRGBA );
    
    // [isolate] gives better compilation results currently
    [isolate]
    {
        Out.vColor0 = EncodeDXT1Internal( vRGBA, vMinRGBA.xyz, vMaxRGBA.xyz );
    }
    
    if( g_bMemExport )
    {
        MemExport64( In.vTexCoord.zw, Out.vColor0 );
    }
    else
    {
        Out.vColor0 = ReinterpretCastUnsignedToSigned_16_16_16_16( Out.vColor0 );
    }
    
    return Out;
}


//---------------------------------------------------------------------------------------------------------
// Name: EncodeDXT5()
// Desc: Entrypoint for encoding a DXT5 texture, either via EDRAM or memexport. 
//---------------------------------------------------------------------------------------------------------
OUT128 EncodeDXT5( INTERPOLATORS In ) : COLOR
{
    OUT128 Out;
    float4 vRGBA[16];
    LoadTexelsRGBA( In.vTexCoord.xy, vRGBA );
    
    float4 vMinRGBA, vMaxRGBA;
    FindMinMaxRGBA( vRGBA, vMinRGBA, vMaxRGBA );
    
    // Slightly better results from repeating the texture fetches,
    // due to relief on register pressure
    [isolate]
    {
        float fAlpha[16];
        for( int i = 0; i < 16; ++i )
        {
            fAlpha[i] = vRGBA[i].a;
        }
        Out.vColor0 = EncodeDXT5AInternal( fAlpha, vMinRGBA.w, vMaxRGBA.w );
    }
    [isolate]
    {
        LoadTexelsRGBA( In.vTexCoord.xy, vRGBA );
        Out.vColor1 = EncodeDXT1Internal( vRGBA, vMinRGBA.xyz, vMaxRGBA.xyz );
    }
    
    if( g_bMemExport )
    {
        MemExport128( In.vTexCoord.zw, Out.vColor0, Out.vColor1 );
    }
    else
    {
        Out.vColor0 = ReinterpretCastUnsignedToSigned_16_16_16_16( Out.vColor0 );
        Out.vColor1 = ReinterpretCastUnsignedToSigned_16_16_16_16( Out.vColor1 );
    }
    
    return Out;
}


//---------------------------------------------------------------------------------------------------------
// Name: EncodeDXN()
// Desc: Entrypoint for encoding a DXN texture, either via EDRAM or memexport. 
//---------------------------------------------------------------------------------------------------------
OUT128 EncodeDXN( INTERPOLATORS In ) : COLOR
{
    OUT128 Out;
    float2 vUV[16];
    LoadTexelsUV( In.vTexCoord.xy, vUV );
    
    float2 vMinUV, vMaxUV;
    FindMinMaxUV( vUV, vMinUV, vMaxUV );
    
    float fU[16], fV[16];
    for( int i = 0; i < 16; ++i )
    {
        fU[i] = vUV[i].x;
        fV[i] = vUV[i].y;
    }
    Out.vColor0 = EncodeDXT5AInternal( fU, vMinUV.x, vMaxUV.x );
    Out.vColor1 = EncodeDXT5AInternal( fV, vMinUV.y, vMaxUV.y );
    
    if( g_bMemExport )
    {
        MemExport128( In.vTexCoord.zw, Out.vColor0, Out.vColor1 );
    }
    else
    {
        Out.vColor0 = ReinterpretCastUnsignedToSigned_16_16_16_16( Out.vColor0 );
        Out.vColor1 = ReinterpretCastUnsignedToSigned_16_16_16_16( Out.vColor1 );
    }

    return Out;
}


//---------------------------------------------------------------------------------------------------------
// Name: EncodeCTX1()
// Desc: Entrypoint for encoding a CTX1 texture, either via EDRAM or memexport. 
//---------------------------------------------------------------------------------------------------------
OUT64 EncodeCTX1( INTERPOLATORS In ) : COLOR
{
    OUT64 Out;
    float2 vUV[16];
    LoadTexelsUV( In.vTexCoord.xy, vUV );
    
    float2 vMinUV, vMaxUV;
    FindMinMaxUV( vUV, vMinUV, vMaxUV );
    
    Out.vColor0 = EncodeCTX1Internal( vUV, vMinUV, vMaxUV );
    
    if( g_bMemExport )
    {
        MemExport64( In.vTexCoord.zw, Out.vColor0 );
    }
    else
    {
        Out.vColor0 = ReinterpretCastUnsignedToSigned_16_16_16_16( Out.vColor0 );
    }
    
    return Out;
}
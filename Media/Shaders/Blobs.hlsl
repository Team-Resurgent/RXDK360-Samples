//-----------------------------------------------------------------------------
// File: Blobs.hlsl
//
// Desc: HLSL file for the Blobs sample. 
// 
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
// Global variables
//-----------------------------------------------------------------------------
static const float GAUSSIANTEXSIZE = 64;
static const float THRESHOLD = 0.08f;

// Transform data
uniform float4x4 mWORLD_VIEW    : register(c10);
uniform float4x4 mPROJECTION    : register(c14);

// Per-blob data for the vertex shader
uniform float4   vBLOB_POS_SIZE : register(c0);
uniform float4   vBLOB_COLOR    : register(c1);

// Textures
sampler SourceBlobSampler   : register(s0);
sampler NormalBufferSampler : register(s1);
sampler ColorBufferSampler  : register(s2);
sampler EnvMapSampler       : register(s3);




//-----------------------------------------------------------------------------
// Vertex/pixel shader output structures
//-----------------------------------------------------------------------------
struct VS_OUTPUT_BLENDER
{
    float4 vPosition : POSITION;
    float2 tCurr     : TEXCOORD0;
    float2 tBack     : TEXCOORD1;
};

struct VS_OUTPUT_LIGHT
{
    float4 vPosition : POSITION;
    float2 t         : TEXCOORD0;
};


struct PS_OUTPUT
{
    float4 vNormal : COLOR0;
    float4 vColor  : COLOR1;
};




//-----------------------------------------------------------------------------
// Name: BlobBlenderVS
// Desc: Vertex shader for rendering blobs
//-----------------------------------------------------------------------------
VS_OUTPUT_BLENDER BlobBlenderVS( float2 vTexCoords : TEXCOORD0 )
{
    // Extract blob attributes
    float4 vBlobPosition = float4( vBLOB_POS_SIZE.xyz, 1 );
    float  fBlobSize     = vBLOB_POS_SIZE.w;
    
    // Calc camera and screenspace positions
    float4 vBlobCameraPos = mul( vBlobPosition, mWORLD_VIEW );
    float4 vBlobScreenPos = mul( vBlobCameraPos, mPROJECTION );

    // For calculating billboarding
    float4 vBlobCameraOffset = float4( fBlobSize, fBlobSize, vBlobCameraPos.z, 1 );
    float4 vBlobScreenOffset = mul( vBlobCameraOffset, mPROJECTION );

    float2 vPosOffset;
    vPosOffset.x = vBlobScreenPos.x + ( 2*vTexCoords.x - 1.0f ) * vBlobScreenOffset.x;
    vPosOffset.y = vBlobScreenPos.y + ( 2*vTexCoords.y - 1.0f ) * vBlobScreenOffset.y;
    vPosOffset  /= vBlobScreenPos.w;

    VS_OUTPUT_BLENDER Output;
    Output.vPosition.x = +( 2 * vPosOffset.x - 0.5f / 640.0f );
    Output.vPosition.y = -( 2 * vPosOffset.y - 0.5f / 480.0f );
    Output.vPosition.z = 0.0f;
    Output.vPosition.w = 1.0f;
    Output.tCurr.x     = vTexCoords.x;
    Output.tCurr.y     = vTexCoords.y;
    Output.tBack.x     = 0.5f + vPosOffset.x;
    Output.tBack.y     = 0.5f + vPosOffset.y;

    return Output;
}




//-----------------------------------------------------------------------------
// Name: DoLerp
// Desc: Peform a linear interpolation
//-----------------------------------------------------------------------------
float DoLerp( in float2 tCurr )
{
    // Scale out into pixel space
    float2 pixelpos = GAUSSIANTEXSIZE * tCurr;

    // Determine the lerp amounts
    float2 lerps = frac( pixelpos );

    // Get the upper left position
    float2 lerppos = float2( (pixelpos-(lerps/GAUSSIANTEXSIZE))/GAUSSIANTEXSIZE );

    float sourcevals[4];
    sourcevals[0] = tex2D( SourceBlobSampler, lerppos + float2(        0.0,                  0.0        ) ).r;  
    sourcevals[1] = tex2D( SourceBlobSampler, lerppos + float2(1.0/GAUSSIANTEXSIZE,          0.0        ) ).r;
    sourcevals[2] = tex2D( SourceBlobSampler, lerppos + float2(        0.0,         1.0/GAUSSIANTEXSIZE ) ).r;
    sourcevals[3] = tex2D( SourceBlobSampler, lerppos + float2(1.0/GAUSSIANTEXSIZE, 1.0/GAUSSIANTEXSIZE ) ).r;

    // Bilinear filtering
    float Output = lerp( lerp( sourcevals[0], sourcevals[1], lerps.x ),
                         lerp( sourcevals[2], sourcevals[3], lerps.x ),
                         lerps.y );
    return Output;
}


//-----------------------------------------------------------------------------
// Name: BlobBlenderPS
// Desc: 
//-----------------------------------------------------------------------------
PS_OUTPUT BlobBlenderPS( VS_OUTPUT_BLENDER Input )
{ 
    // Get the new blob weight
    float weight = DoLerp( Input.tCurr );

    // Get the old data
    float4 oldNormalData = tex2D( NormalBufferSampler, Input.tBack );
    float4 oldColorData  = tex2D( ColorBufferSampler,  Input.tBack );
    
    // Generate new surface data
    float4 newNormalData = float4( (Input.tCurr.x-0.5f) * vBLOB_POS_SIZE.w,
                                   (Input.tCurr.y-0.5f) * vBLOB_POS_SIZE.w,
                                   0.0f, 1.0f );
    newNormalData *= weight;
    
    //generate new material properties
    float4 newColorData = vBLOB_COLOR * weight;

    // Additive blending
    PS_OUTPUT output;
    output.vNormal = newNormalData + oldNormalData; 
    output.vColor  = newColorData  + oldColorData;
    return output;
}




//-----------------------------------------------------------------------------
// Name: BlobLightVS
// Desc: Vertex shader for passing-thru vertex data
//-----------------------------------------------------------------------------
VS_OUTPUT_LIGHT BlobLightVS( float4 vPosition  : POSITION,
                             float2 vTexCoords : TEXCOORD0 )
{
    VS_OUTPUT_LIGHT Output;
    Output.vPosition = vPosition;
    Output.t         = vTexCoords;

    return Output;
}




//-----------------------------------------------------------------------------
// Name: BlobLightPS
// Desc: 
//-----------------------------------------------------------------------------
float4 BlobLightPS( float2 TexCoord0 : TEXCOORD0 ) : COLOR
{
    static const float aaval = THRESHOLD * 0.07f;

    float4 blobdata = tex2D( NormalBufferSampler, TexCoord0 );
    float4 color    = tex2D( ColorBufferSampler,  TexCoord0 );
    
    color /= blobdata.w;

    float3 EnvMapDir;
    EnvMapDir.x = -blobdata.x / blobdata.w;
    EnvMapDir.y = -blobdata.y / blobdata.w;
    EnvMapDir.z =  blobdata.w - THRESHOLD;

    float4 Output;  
    Output.rgb  = color.rgb + texCUBE( EnvMapSampler, EnvMapDir );
    Output.rgb *= saturate( EnvMapDir.z / aaval );
    Output.a    = 1.0f;

    return Output;
}


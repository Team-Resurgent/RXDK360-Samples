//--------------------------------------------------------------------------------------
// DepthVisualizer.hlsl
//
// Advanced Technology Group
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

// Keep track of certain register assignments - duplicate these definitions in .cpp file
// sampler registers
#define reg_depth                       0
#define reg_filter                      0
#define reg_normal                      1
#define reg_mask                        1
#define reg_irshadow                    2
#define reg_depthlod                    2
#define reg_masklod                     2

// float4 registers
#define reg_SweetSpotOriginAndRadius    0
#define reg_SweetSpotDirection          1
#define reg_NearFarDist                 2
#define reg_FloorPlane                  3
#define reg_SweeperPlane                4
#define reg_SweetSpotColor              5
#define reg_FloorPlaneColor             6
#define reg_SweeperPlaneColor           7
#define reg_OutlineColor                8
#define reg_TwiceTanHalfFOV             9
#define reg_LaserToSensorOffsetWorld    10
#define reg_LaserToSensorOffsetTexcoord 11
#define reg_TopLevel                    12
#define reg_TexDims                     13
#define reg_NormalFadeParams            14
#define reg_GenericColor                15
#define reg_BilateralPeakThickness      16
#define reg_RealisticFarDepth           17
#define reg_Matrix                      20  // uses 4 slots
#define reg_Weights                     24  // uses 1 slot per weight
#define reg_ColorCodeColors             32  // uses COLOR_CODE_COUNT slots

// bool registers
#define reg_DrawFloorPlane              0
#define reg_SweeperOn                   1
#define reg_FadeNormal                  2
#define reg_Renormalize                 3
#define reg_ColorCode                   4
#define reg_MaskValid                   5

#define SAMPLER_REGISTER(reg)           register(s##reg##)
#define FLOAT_REGISTER(reg)             register(c##reg##)
#define BOOL_REGISTER(reg)              register(b##reg##)


// Channels of the 4-bit mask texture --- definitions must match .cpp file.  
// These apply only to the top mip layer.  The lower mips layers are only used for 
// temporary scratch space.
#define CHANNEL_SHOULD_BE_VALID     x
#define CHANNEL_IS_IR_SHADOW        y
#define CHANNEL_IS_SMALL_HOLE       z
#define CHANNEL_IS_SMALL_ISLAND     w

bool GetShouldBeValid( bool4 vMask )                  { return vMask.CHANNEL_SHOULD_BE_VALID; }
void SetShouldBeValid( inout bool4 vMask, bool bTF )  { vMask.CHANNEL_SHOULD_BE_VALID = bTF; }
bool GetIsIRShadow( bool4 vMask )                     { return vMask.CHANNEL_IS_IR_SHADOW; }
void SetIsIRShadow( inout bool4 vMask, bool bTF )     { vMask.CHANNEL_IS_IR_SHADOW = bTF; }
bool GetIsSmallHole( bool4 vMask )                    { return vMask.CHANNEL_IS_SMALL_HOLE; }
void SetIsSmallHole( inout bool4 vMask, bool bTF )    { vMask.CHANNEL_IS_SMALL_HOLE = bTF; }
bool GetIsSmallIsland( bool4 vMask )                  { return vMask.CHANNEL_IS_SMALL_ISLAND; }
void SetIsSmallIsland( inout bool4 vMask, bool bTF )  { vMask.CHANNEL_IS_SMALL_ISLAND = bTF; }


// Match enum in .cpp file.  These are the color codes in the processed depth map
#define COLOR_CODE_NO_DEPTH                 0
#define COLOR_CODE_UNRESOLVED_IR_SHADOW     1 
#define COLOR_CODE_UNRESOLVED_SMALL_HOLE    2 
#define COLOR_CODE_RESOLVED_IR_SHADOW       3 
#define COLOR_CODE_RESOLVED_SMALL_HOLE      4 
#define COLOR_CODE_SMALL_ISLAND             5 
#define COLOR_CODE_COUNT                    6

static float g_fFarDepth = 8191.0f;    // Max representable depth value in 13 bits

struct VERTEX
{
    float4 vPosition    : POSITION0;
    float2 vTexCoord    : TEXCOORD0;
};

struct INTERPOLATORS
{
    float4 vPosition     : POSITION0;
    float2 vTexCoord     : TEXCOORD0;
};

//---------------------------------------------------------------------------------------------------------
// Name: ScreenSpaceShaderVS()
// Desc: Simple pass through. 
//---------------------------------------------------------------------------------------------------------
float4x4 g_matWVP           : FLOAT_REGISTER(reg_Matrix);

INTERPOLATORS ScreenSpaceShaderVS( VERTEX In )
{
    INTERPOLATORS Out;
    Out.vPosition = mul( In.vPosition, g_matWVP );
    Out.vTexCoord.xy = In.vTexCoord;
    
    return Out;
}


//---------------------------------------------------------------------------------------------------------
// Name: SolidColorPS()
// Desc: Simple pass through. 
//---------------------------------------------------------------------------------------------------------
float4 g_vGenericColor            : FLOAT_REGISTER(reg_GenericColor);

float4 SolidColorPS( [unused] float2 vTexCoord : TEXCOORD0 ) : COLOR0
{
    return g_vGenericColor;
}


//---------------------------------------------------------------------------------------------------------
// Name: EncodeSignedAsUnsigned()
// Desc: Given a signed float4, convert it into an unsigned value that can be written to a
// 2:10:10:10 render target.  When this value is resolved and then treated as signed, it will 
// fetch back to the original value.  (Could also use EncodeSignedAsBias below --- one is
// sometimes more convenient than the other.)
//---------------------------------------------------------------------------------------------------------
float4 EncodeSignedAsUnsigned( float4 vSigned )
{
    // sends [0.0f,1.0f] to [0x000,0x1ff], [-1.0f,-0.0f] to [0x201,0x3ff] 
    float4 vUnsigned = ( vSigned >= 0.0f ) 
        ? ( 511.0f / 1023.0f * vSigned ) 
        : ( 511.0f / 1023.0f * vSigned + 1024.0f / 1023.0f );
    
    return vUnsigned;
} 
    
    
//---------------------------------------------------------------------------------------------------------
// Name: EncodeSignedAsBias()
// Desc: Given a signed float4, convert it into an unsigned value that can be written to a
// 2:10:10:10 render target.  When this value is resolved and then treated as biased, it will 
// fetch back to the original value.
//---------------------------------------------------------------------------------------------------------
float4 EncodeSignedAsBias( float4 vSigned )
{
    // sends [-1.0f,1.0f] to [0x001,03ff]
    return 511.0f / 1023.0f * vSigned + 512.0f / 1023.0f;
}
    
    
//---------------------------------------------------------------------------------------------------------
// Name: CopySignedToBiasPS()
// Desc: Allow a signed normal map to be displayed as a color texture, without losing the negatives.  
// Used for the thumbnail display of the normal map. 
//---------------------------------------------------------------------------------------------------------
sampler2D texNormal                 : SAMPLER_REGISTER(reg_normal);
bool g_bRenormalize          : BOOL_REGISTER(reg_Renormalize);

float4 CopySignedToBiasPS( float2 vTexCoord : TEXCOORD0 ) : COLOR0
{
    float3 vNormal = tex2D( texNormal, vTexCoord );
    if( g_bRenormalize )
    {
        vNormal = normalize( vNormal );
    }
    
    return EncodeSignedAsBias( float4( vNormal, 1.0f ) );
}


//---------------------------------------------------------------------------------------------------------
// Name: NuiToWorld()/WorldToNui()
// Desc: Given xy in screenspace texcoords, and z in Nui depth units (mm), produce xyz in world space (mm).
//
// Here we interpret the depth pixels as covering equal area, rather than equal angle.  This matches how 
// the camera acquires the data.  It implies that to recover world space position, we invert an orthogonal
// transform, rather than a projective tranform.  It also implies that to repopulate the depth buffer, 
// we render using an orthogonal transform, rather than a projective transform.
//
//  TexCoord  0 - - - -0.5- - - - 1
//             ===================    <-- each '=' represents one pixel
//              \       |       /  |  
//               \      |      / depth
//                \     |     /    |  
//                 \    |    /     V 
//                  \   |   /
//                   \  |  /
//                    \ | <-- half FOV
//                     \|/
//             ---------+---------
//
//---------------------------------------------------------------------------------------------------------
float2 g_vTwiceTanHalfFOV : FLOAT_REGISTER(reg_TwiceTanHalfFOV);

float3 NuiToWorld( float3 vNuiPosition )
{
    float3 vWorldPosition;
    
    vWorldPosition.xy = vNuiPosition.z * g_vTwiceTanHalfFOV * ( vNuiPosition.xy - 0.5f );
    vWorldPosition.z = vNuiPosition.z;
    
    return vWorldPosition;
}


float3 WorldToNui( float3 vWorldPosition )
{
    float3 vNuiPosition;
    
    vNuiPosition.xy = vWorldPosition.xy / ( vWorldPosition.z * g_vTwiceTanHalfFOV ) + 0.5f;
    vNuiPosition.z = vWorldPosition.z;
    
    return vNuiPosition;
}


//---------------------------------------------------------------------------------------------------------
// Name: SensorToLaser()
// Desc: Given xyz in NUI space relative to the sensor, produce xyx in NUI space relative to the
// laser.  This is a parallax calculation.  In other words, given texture coordinates for the depth map, 
// and the depth at that pixel, produce texture coordinates for the same object as if it were seen by 
// the laser instead of the sensor.
//
//  TexCoord   0- - - 0.5 - - -1
//             ==================   <-- each '=' represents one pixel
//             |      |      /  |  
//             |\     |    // depth
//             | \    |  / /    |  
//             |  \ X |/  /     V   <-- In = X relative to sensor
//             |   \ /|  /          <-- Out = X relative to laser
//             |   /\ | /
//             | /   \|/<-- half FOV
//            -+-------+-------
//           laser    sensor
//             --------->   
//               offset 
//
//---------------------------------------------------------------------------------------------------------
float3 g_vLaserToSensorOffsetWorld    : FLOAT_REGISTER(reg_LaserToSensorOffsetWorld);
float3 g_vLaserToSensorOffsetTexCoord : FLOAT_REGISTER(reg_LaserToSensorOffsetTexcoord);
//float2 g_vTwiceTanHalfFOV : FLOAT_REGISTER(reg_TwiceTanHalfFOV);
float4 g_vTexDims    : FLOAT_REGISTER(reg_TexDims); // .x = Width, .y = Height, .z = 1/Width, .w = 1/Height

float3 SensorToLaser( float3 vPositionRelativeToSensor )
{
    // Convert to world space
    float3 vWorldPosition = NuiToWorld( vPositionRelativeToSensor );

    // Shift in world space by the offset between the sensor and the laser.  
    vWorldPosition += g_vLaserToSensorOffsetWorld;

    // Convert back to NUI space
    float3 vPositionRelativeToLaser = WorldToNui( vWorldPosition );

    // Undo the shift between the sensor and the laser.  This causes IR shadow width to go to zero
    // at the largest distance picked up by the sensor.
    vPositionRelativeToLaser -= g_vLaserToSensorOffsetTexCoord * g_vTexDims.z;

    // We use the knowledge that the offset is only in the .x direction, to reduce the 
    // amount of calculation, and eliminate precision error in the .y direction.
    vPositionRelativeToLaser.y = vPositionRelativeToSensor.y;

    return vPositionRelativeToLaser;
}


//---------------------------------------------------------------------------------------------------------
// Name: IRShadowGenerateVS()
// Desc: From the surface seen by the sensor, return the position from the pov of the laser.  The effect
// is to re-render the depth map from the laser's point of view.  Nearby objects are shifted more than 
// distant objects.
//
// This is totally analogous to rendering a shadow map from the light's point of view.  
//
// Be careful to remember this is a vertex shader, while everything around it is pixel shaders.
//---------------------------------------------------------------------------------------------------------
sampler2D texDepth  : SAMPLER_REGISTER(reg_depth);
sampler2D texMask   : SAMPLER_REGISTER(reg_mask);

//float4x4 g_matWVP           : FLOAT_REGISTER(reg_Matrix);
//float4 g_vTexDims    : FLOAT_REGISTER(reg_TexDims); // .x = Width, .y = Height, .z = 1/Width, .w = 1/Height

float4 IRShadowGenerateVS( int index: INDEX ) : POSITION
{
    // Find normalized texcoords.  This pass is not computation bound, so no need to ration
    // instructions.
    float2 vTexCoord;
    vTexCoord.y = floor( ( index + 0.5f ) * g_vTexDims.z );
    vTexCoord.x = index - vTexCoord.y * g_vTexDims.x;
    vTexCoord *= g_vTexDims.zw;

    float fDepth = tex2Dlod( texDepth, float4( vTexCoord, 0.0f, 0.0f ) );
    float4 vMask = tex2Dlod( texMask, float4( vTexCoord, 0.0f, 0.0f ) );

    // If we are going to erase this pixel as noise, don't let it cast a shadow.
    fDepth = GetIsSmallIsland( vMask ) ? 0.0f : fDepth;

    float3 vPositionRelativeToLaser = SensorToLaser( float3( vTexCoord, fDepth ) );

    // Convert to view space (this can reduce to a single 'mad')
    vPositionRelativeToLaser.xy *= 2.0f;
    vPositionRelativeToLaser.xy -= 1.0f;
    vPositionRelativeToLaser.y *= -1.0f;
    vPositionRelativeToLaser.z /= 65535.0f;
    
    // Orthogonal shadow projection (w == 1.0f)
    return float4( vPositionRelativeToLaser, 1.0f );
}


//---------------------------------------------------------------------------------------------------------
// Name: IRShadowApplyPS()
// Desc: Given a texel in the depth map, sample the infrared shadow map to determine whether
// the texel is inside the IR shadow.  
//
// This calculation is analogous to the application of a regular shadow map.
//
// The mask texture may be initialized to default settings, or the non-IR-shadow bits may have been
// set by preceding passes.  These values must be respected, and transmitted intact.
//---------------------------------------------------------------------------------------------------------
//sampler2D texDepth : SAMPLER_REGISTER(reg_depth);
sampler2D texIRShadow : SAMPLER_REGISTER(reg_irshadow);
//sampler2D texMask : SAMPLER_REGISTER(reg_mask);
float g_fRealisticFarDepth   : FLOAT_REGISTER(reg_RealisticFarDepth); 

float4 IRShadowApplyPS( float2 vTexCoord : TEXCOORD0 ) : COLOR0
{
    float fNuiDepth = tex2D( texDepth, vTexCoord ).x;
    bool4 vReturnMask = tex2D( texMask, vTexCoord );

    [branch] // optimize, since hopefully most of the scene will skip this branch
    if( fNuiDepth == 0.0f )
    {
        // We have no actual depth recorded at this pixel.  However, the shadowing algorithm
        // requires that we know both the depth of the occluder and the depth of the occluded
        // object.  
        // 
        // The best chance of being in IRShadow is an object at the farthest allowed depth.
        // Sometimes this means we will get an incorrect answer, particularly when one nearby object
        // shadows another nearby object.
        //  
        // The "FillSmallHoles" pass can help correct this.  Another approach (not tried here) 
        // would be change the order of passes in the sample, and do this instead:
        //  
        //      1. First fill this pixel, assuming it is in IRShadow
        //      2. Then, using the filled depth, calculate whether the assumption was valid
        //
        // For now, we stick to the simple approach, which works reasonably well in practice.
        //
        float fOccludedDepth = g_fRealisticFarDepth;

        float3 vPositionRelativeToLaser = SensorToLaser( float3( vTexCoord, g_fRealisticFarDepth ) );

        // Use linear filtering to catch edges
        float fIROccluderDepth;
        asm
        {
            tfetch2D fIROccluderDepth.x___, texIRShadow, vPositionRelativeToLaser, MinFilter=linear, MagFilter=linear
        };

        // The shadow map was effectively cleared to g_fFarDepth + 1.  Any other value encountered here means 
        // that some object in the scene could have cast a shadow over this pixel. 
        bool bInIRShadow  = ( fIROccluderDepth <= fOccludedDepth );

        SetIsIRShadow( vReturnMask, bInIRShadow );
        SetShouldBeValid( vReturnMask, GetShouldBeValid( vReturnMask ) || bInIRShadow );
    }
    else
    {
        SetIsIRShadow( vReturnMask, false );
    }

    return vReturnMask;
}


//---------------------------------------------------------------------------------------------------------
// Name: DownfillSmallHolesPS()
// Description: This function is the reduction step for filling small holes in the depth map.
// (Also used in reverse for erasing small islands of valid depth, surrounded by invalid depths.)
// We use a 4-bit mask to denote whether a pixel's neighborhood contains mostly valid depth in the
// 4 compass directions.  The mask is propagated down several mip layers.
//---------------------------------------------------------------------------------------------------------
//sampler2D texMask   : SAMPLER_REGISTER(reg_mask);
bool g_bTopLevel   : FLOAT_REGISTER(reg_TopLevel); // Are we reading/writing the top mip layer?

#ifndef g_bHolesOrIslands   // passed as a #define from the command line, for efficiency
bool g_bHolesOrIslands : register(b0);   // Are we filling holes, or erasing islands?
#endif

float4 DownfillSmallHolesPS( float2 vTexCoord : TEXCOORD0 ) : COLOR0
{
    // Sampling pattern:
    //
    //  *       - destination pixel center of lower mip
    //  ><      - incoming texcoords (HALFPIXELOFFSET turned off)
    //  <>      - texcoords to sample from higher mip (blend 4 texels)
    //  = or || - pixel borders of lower mip
    //  - or |  - pixel borders of higher mip
    //
    //  |---------||---------|---------||---------|
    //  |         ||         |         ||         |
    //  |         ||         |         ||         |
    //  |         ||         |         ||         |
    //  |=========><=========|=========<>=========|
    //  |         ||         |         ||         |
    //  |         ||         |         ||         |
    //  |         ||         |         ||         |
    //  |---------||---------*---------||---------|
    //  |         ||         |         ||         |
    //  |         ||         |         ||         |
    //  |         ||         |         ||         |
    //  |=========<>=========|=========<>=========|
    //  |         ||         |         ||         |
    //  |         ||         |         ||         |
    //  |         ||         |         ||         |
    //  |---------||---------|---------||---------|
    //

    float4 vMasks00, vMasks10, vMasks01, vMasks11;
    asm
    {
        tfetch2D vMasks00, vTexCoord, texMask, OffsetX = 0.0, OffsetY = 0.0, MinFilter=linear, MagFilter=point, MipFilter=point
        tfetch2D vMasks10, vTexCoord, texMask, OffsetX = 2.0, OffsetY = 0.0, MinFilter=linear, MagFilter=point, MipFilter=point
        tfetch2D vMasks01, vTexCoord, texMask, OffsetX = 0.0, OffsetY = 2.0, MinFilter=linear, MagFilter=point, MipFilter=point
        tfetch2D vMasks11, vTexCoord, texMask, OffsetX = 2.0, OffsetY = 2.0, MinFilter=linear, MagFilter=point, MipFilter=point
    };

    // At the top level, only one channel of the incoming mask is used
    if( g_bTopLevel )
    {
        vMasks00 = GetShouldBeValid( vMasks00 );
        vMasks10 = GetShouldBeValid( vMasks10 );
        vMasks01 = GetShouldBeValid( vMasks01 );
        vMasks11 = GetShouldBeValid( vMasks11 );
    }

    float fEpsilon = 0.125f;  // The check below should test how many of the 4 filter inputs were true vs. false
    bool4 vMask;   // .x = left, .y = right, .z = top, .w = bottom
    if( g_bHolesOrIslands )
    {
        vMask.x = ( vMasks00.x >= 0.5f - fEpsilon ) && ( vMasks01.x >= 0.5f - fEpsilon );
        vMask.y = ( vMasks10.y >= 0.5f - fEpsilon ) && ( vMasks11.y >= 0.5f - fEpsilon );
        vMask.z = ( vMasks00.z >= 0.5f - fEpsilon ) && ( vMasks10.z >= 0.5f - fEpsilon );
        vMask.w = ( vMasks01.w >= 0.5f - fEpsilon ) && ( vMasks11.w >= 0.5f - fEpsilon );
    }
    else
    {
        vMask.x = ( vMasks00.x > 0.5f + fEpsilon ) || ( vMasks01.x > 0.5f + fEpsilon );
        vMask.y = ( vMasks10.y > 0.5f + fEpsilon ) || ( vMasks11.y > 0.5f + fEpsilon );
        vMask.z = ( vMasks00.z > 0.5f + fEpsilon ) || ( vMasks10.z > 0.5f + fEpsilon );
        vMask.w = ( vMasks01.w > 0.5f + fEpsilon ) || ( vMasks11.w > 0.5f + fEpsilon );
    }

    return vMask;
}


//---------------------------------------------------------------------------------------------------------
// Name: UpfillSmallHolesPS()
//---------------------------------------------------------------------------------------------------------
//sampler2D texMask       : SAMPLER_REGISTER(reg_mask);
sampler2D texMaskLod    : SAMPLER_REGISTER(reg_masklod);
//bool g_bTopLevel   : FLOAT_REGISTER(reg_TopLevel); // intentionally putting bool in float register
//float4 g_vTexDims    : FLOAT_REGISTER(reg_TexDims); // .x = Width, .y = Height, .z = 1/Width, .w = 1/Height

float4 UpfillSmallHolesPS( float2 vTexCoord : TEXCOORD0 ) : COLOR0
{
    // Sampling pattern:
    //
    //  *   - destination pixel center of higher mip
    //  ><  - incoming texcoords (HALFPIXELOFFSET turned on)
    //      - also texcoords to sample from higher mip and lower mip (lower mip sample always a bilinear blend)
    //
    //  |---------||---------|---------||---------|
    //  |         ||         |         ||         |
    //  |         ||         |         ||         |
    //  |         ||         |         ||         |
    //  |=========||=========|=========||=========|
    //  |         ||         |         ||         |
    //  |         ||         |         ||         |
    //  |         ||         |         ||         |
    //  |---------||---------|---------||---------|
    //  |         ||         |         ||         |
    //  |         ||         |   >*<   ||         |
    //  |         ||         |         ||         |
    //  |=========||=========|=========||=========|
    //  |         ||         |         ||         |
    //  |         ||         |         ||         |
    //  |         ||         |         ||         |
    //  |---------||---------|---------||---------|
    //

    float4 vMask;
    float4 vMaskLOD;
    asm
    {
        tfetch2D vMask, vTexCoord, texMask, OffsetX = 0.0, OffsetY = 0.0, MinFilter=point, MagFilter=point, MipFilter=point
        tfetch2D vMaskLOD, vTexCoord, texMaskLod, OffsetX = 0.0, OffsetY = 0.0, MinFilter=point, MagFilter=linear, MipFilter=point, LODBias=1.0f
    };

    bool4 vReturnMask = vMask;
    if( g_bTopLevel )
    {
        vMask = GetShouldBeValid( vMask );
    }

    bool bFill;
    if( g_bHolesOrIslands )
    {
        // Small holes:  Fill in mask at current mip if all of the 4 flags --- left, right, top, bottom --- 
        // are set at this mip or at lower mip
        bFill = ( vMask.x && vMask.y && vMask.z && vMask.w ) 
            || ( vMaskLOD.x && vMaskLOD.y && vMaskLOD.z && vMaskLOD.w );
    }
    else
    {
        // Small islands:  Fill in mask at current mip if any of the 4 flags --- left, right, top, bottom --- 
        // are set at this mip and at lower mip
        bFill = ( vMask.x || vMask.y || vMask.z || vMask.w ) 
            && ( vMaskLOD.x || vMaskLOD.y || vMaskLOD.z || vMaskLOD.w );
    }

    if( g_bTopLevel )
    {
        // Top level, each channel has a different meaning in the mask, per the CHANNEL_* #defines
        if( g_bHolesOrIslands )
        {
            SetIsSmallHole( vReturnMask, bFill && !vMask );
            SetShouldBeValid( vReturnMask, bFill || vMask );
        }
        else
        {
            SetIsSmallIsland( vReturnMask, !bFill && vMask );
            SetShouldBeValid( vReturnMask, bFill && vMask );
        }
    }
    else
    {
        // Lower level, true means "should fill", false means "should omit"
        vReturnMask = bFill;
    }

    return vReturnMask;
}


//---------------------------------------------------------------------------------------------------------
// Name: DownsampleDepthPS()
// Desc: Downsample with 'max' filter, propagating only into regions marked "valid" in the mask.  
//---------------------------------------------------------------------------------------------------------
//sampler2D texDepth  : SAMPLER_REGISTER(reg_depth);
//sampler2D texMask   : SAMPLER_REGISTER(reg_mask);
//bool g_bTopLevel   : FLOAT_REGISTER(reg_TopLevel); // Are we at the top mip layer?

void DownsampleDepthPS( float2 vTexCoord : TEXCOORD0, out float4 Out0 : COLOR0, out float4 Out1 : COLOR1 )
{
    // Sampling pattern:
    //
    //  *   - destination pixel center of lower mip
    //  ><  - incoming texcoords (HALFPIXELOFFSET turned off )
    //  +   - texcoords to sample from higher mip
    //
    //  ><=========|=========||
    //  ||         |         ||
    //  ||    +    |    +    ||
    //  ||         |         ||
    //  ||---------*---------||
    //  ||         |         ||
    //  ||    +    |    +    ||
    //  ||         |         ||
    //  ||=========|=========||
    //

    float4 depths;
    asm
    {
        tfetch2D depths.x___, vTexCoord, texDepth, OffsetX = 0.5, OffsetY = 0.5, MinFilter=point, MagFilter=point, MipFilter=point
        tfetch2D depths._x__, vTexCoord, texDepth, OffsetX = 0.5, OffsetY = 1.5, MinFilter=point, MagFilter=point, MipFilter=point
        tfetch2D depths.__x_, vTexCoord, texDepth, OffsetX = 1.5, OffsetY = 0.5, MinFilter=point, MagFilter=point, MipFilter=point
        tfetch2D depths.___x, vTexCoord, texDepth, OffsetX = 1.5, OffsetY = 1.5, MinFilter=point, MagFilter=point, MipFilter=point
    };

    // The compiler will optimize away all but the SHOULD_BE_VALID channel (so these use only one GPR)
    float4 vMasks00, vMasks10, vMasks01, vMasks11;
    asm
    {
        tfetch2D vMasks00, vTexCoord, texMask, OffsetX = 0.5, OffsetY = 0.5, MinFilter=point, MagFilter=point, MipFilter=point
        tfetch2D vMasks10, vTexCoord, texMask, OffsetX = 0.5, OffsetY = 1.5, MinFilter=point, MagFilter=point, MipFilter=point
        tfetch2D vMasks01, vTexCoord, texMask, OffsetX = 1.5, OffsetY = 0.5, MinFilter=point, MagFilter=point, MipFilter=point
        tfetch2D vMasks11, vTexCoord, texMask, OffsetX = 1.5, OffsetY = 1.5, MinFilter=point, MagFilter=point, MipFilter=point
    };
    bool4 masks = bool4( 
        GetShouldBeValid( vMasks00 ), 
        GetShouldBeValid( vMasks10 ), 
        GetShouldBeValid( vMasks01 ), 
        GetShouldBeValid( vMasks11 )
    );

    float fMaxDepth;
    float4 zero = 0.0f;
    bool4 invalidDepths;
    bool4 validDepths;
    bool bAnyMask;
    asm         // the compiler tends not to do as well as these hand-coded instructions
    {
        max4 fMaxDepth.x___, depths
        seq invalidDepths, depths, zero
        sne validDepths, depths, zero
        max4 bAnyMask.x___, masks
    };
    
    // The purpose of this downsample operation is to restore 'missing' depth values which were occluded
    // from the point-of-view of the depth sensor.  Because of the relative arrangement of the laser and 
    // the depth sensor, occluded pixels are to the *right* of the occluding foreground object in screenspace.
    //
    // So areas which have invalid depth due to occlusion will have background pixels to their right and 
    // foreground pixels to their left.  These areas are probably part of the background.  Therefore, we 
    // want to propagate depths from the right to the left.
    bool bPropagateDepthFromRight = ( validDepths.z && validDepths.w ) 
        || ( invalidDepths.x && validDepths.z ) 
        || ( invalidDepths.y && validDepths.w );
        
    Out0 = bPropagateDepthFromRight ? fMaxDepth : 0.0f;
    Out1 = bAnyMask && Out0 == 0;
}


//---------------------------------------------------------------------------------------------------------
// Name: ShiftDepthPS()
// Desc: Shift depths to the left to cover empty texels.  
//---------------------------------------------------------------------------------------------------------
void ShiftDepthPS( float2 vTexCoord : TEXCOORD0, out float4 Out0 : COLOR0, out float4 Out1 : COLOR1 )
{
    bool bInIRShadow = true;//tex2D( texIRMask, vTexCoord );

    // Sampling pattern:
    //
    //  *   - destination pixel center
    //  ><  - incoming texcoords (HALFPIXELOFFSET turned off)
    //  +   - texcoords to sample
    //
    //  ><=========|=========||
    //  ||         |         ||
    //  ||   +*    |    +    ||
    //  ||         |         ||
    //  ||=========|=========||
    //

    float2 depths;
    asm
    {
        tfetch2D depths.x___, vTexCoord, texDepth, OffsetX = 0.5, OffsetY = 0.5, MinFilter=point, MagFilter=point, MipFilter=point
        tfetch2D depths._x__, vTexCoord, texDepth, OffsetX = 1.5, OffsetY = 0.5, MinFilter=point, MagFilter=point, MipFilter=point
    };

    // The compiler will optimize away all but the SHOULD_BE_VALID channel (so these use only one GPR)
    float4 vMasks00, vMasks10;
    asm
    {
        tfetch2D vMasks00, vTexCoord, texMask, OffsetX = 0.5, OffsetY = 0.5, MinFilter=point, MagFilter=point, MipFilter=point
        tfetch2D vMasks10, vTexCoord, texMask, OffsetX = 1.5, OffsetY = 0.5, MinFilter=point, MagFilter=point, MipFilter=point
    };
    bool2 masks = float2( GetShouldBeValid( vMasks00 ), GetShouldBeValid( vMasks10 ) );
    
    // If this pixel is empty and in the mask, fill from the right.  If there's nothing to the right, mark this pixel
    // for fill at the next stage.
    Out0 = ( depths.x == 0.0f && masks.x ) ? depths.y : depths.x;
    Out1 = ( depths.x == 0.0f && depths.y == 0.0f && masks.x && masks.y );
}


//---------------------------------------------------------------------------------------------------------
// Name: UpfillDepthPS()
// Desc: Fill in missing depth pixels with the value at the coarser mip. 
//---------------------------------------------------------------------------------------------------------
//sampler2D texDepth : SAMPLER_REGISTER(reg_depth);    // declared earlier
sampler2D texDepthLod : SAMPLER_REGISTER(reg_depthlod);
//bool g_bTopLevel   : FLOAT_REGISTER(reg_TopLevel); // Are we at the top mip layer?

float4 UpfillDepthPS( float2 vTexCoord : TEXCOORD0 ) : COLOR0
{
    // Sampling pattern:
    //
    //  *   - texcoords to sample from lower mip
    //  ><  - incoming texcoords (HALFPIXELOFFSET turned off)
    //  +   - destination pixel center of higher mip
    //
    //  ><=========|=========||=========|
    //  ||         |         ||         |
    //  ||    +    |         ||         |
    //  ||         |         ||         |
    //  ||---------*---------||---------|
    //  ||         |         ||         |
    //  ||         |         ||         |
    //  ||         |         ||         |
    //  ||=========|=========||=========|
    //

    float4 depths;
    asm
    {
        tfetch2D depths.x___, vTexCoord, texDepth,    OffsetX = 0.5, OffsetY = 0.5, MinFilter=point, MagFilter=point, MipFilter=point
        tfetch2D depths._x__, vTexCoord, texDepthLod, OffsetX = 0.5, OffsetY = 0.5, MinFilter=point, MagFilter=linear, MipFilter=point, LODBias=1.0f
    };

    // The compiler will optimize away all but the SHOULD_BE_VALID channel (so this writes to only one register field)
    float4 vMask;
    asm
    {
        tfetch2D vMask, vTexCoord, texMask, OffsetX = 0.5, OffsetY = 0.5, MinFilter=point, MagFilter=point, MipFilter=point
    };
    bool mask = GetShouldBeValid( vMask );
    
    float fFillDepth;
    if( g_bTopLevel )
    {
        // Top level:  If pixel is empty, fill from lower mip.  
        // If not in the mask (small island), erase.  
        // If pixel is still empty, use far depth.

        fFillDepth = ( depths.x == 0.0f ) ? depths.y : depths.x;
        fFillDepth = mask ? fFillDepth : 0.0f;
        fFillDepth = ( fFillDepth == 0.0f ) ? g_fFarDepth : fFillDepth;
    }
    else
    {
        // Lower level:  If pixel is empty, and in the mask, fill from lower mip.
        fFillDepth = ( mask && depths.x == 0.0f ) ? depths.y : depths.x;
    }

    return fFillDepth;
}


//---------------------------------------------------------------------------------------------------------
// Filtering infrastructure:
//
// The compile line should define:
//
//  - (g_fOffsetX, g_fOffsetY): Direction of a 1D filter step, e.g. (1,1) is diagonal, (1,0) is horizontal
//  - g_iHalfWidth:             Filter kernel = [-g_iHalfWidth, g_iHalfWidth]
//  - g_iNumChannels:           How many channels exist in the texture, e.g. '1' for depth
//  - g_bSigned                 Are we writing signed data (to an unsigned render target)?
//---------------------------------------------------------------------------------------------------------
#ifndef g_fOffsetX
static float g_fOffsetX = 1.0f;
#endif
#ifndef g_fOffsetY
static float g_fOffsetY = 1.0f;
#endif
#ifndef g_iHalfWidth
#define g_iHalfWidth 2
#endif
#ifndef g_iNumChannels
#define g_iNumChannels 4
#endif
#ifndef g_bSigned
#define g_bSigned false
#endif

#if g_iNumChannels == 1
    #define TEXEL_TYPE float
    #define DEST_CHANNEL_MASK x
#elif g_iNumChannels == 2
    #define TEXEL_TYPE float2
    #define DEST_CHANNEL_MASK xy
#elif g_iNumChannels == 3
    #define TEXEL_TYPE float3
    #define DEST_CHANNEL_MASK xyz
#elif g_iNumChannels == 4
    #define TEXEL_TYPE float4
    #define DEST_CHANNEL_MASK xyzw
#else
    #error TEXEL_TYPE must be one of float, float2, float3, float4
#endif

sampler2D texFilter  : SAMPLER_REGISTER(reg_filter);   // defined above

float g_fKernelWeights[2*g_iHalfWidth+1] : FLOAT_REGISTER(reg_Weights);

//---------------------------------------------------------------------------------------------------------
// Name: SampleAtOffset()
// Desc: One filter tap.  
//---------------------------------------------------------------------------------------------------------
TEXEL_TYPE SampleAtOffset( sampler2D tex, float2 vTexCoord, float fOffsetX, float fOffsetY )
{
    float4 vSample;
    asm
    {
        tfetch2D vSample, vTexCoord, tex, OffsetX = fOffsetX, OffsetY = fOffsetY, MinFilter=point, MagFilter=point
    };
    return vSample.DEST_CHANNEL_MASK;
}

    
//---------------------------------------------------------------------------------------------------------
// Name: DirectionalBlurPS()
// Desc: Blur in a direction specified by preprocessor defines. This function could be sped up for
// textures which support bilinear filtering, by using subtexel coords (see the Postprocess sample).  
// That isn't implemented here, to keep things simple.
//---------------------------------------------------------------------------------------------------------
float4 DirectionalBlurPS( float2 vTexCoord : TEXCOORD0 ) : COLOR0
{
    float4 vOutput = 0.0f;
    for( int x = -g_iHalfWidth; x <= g_iHalfWidth; ++x )
    {
        float fOffsetX = x * g_fOffsetX;
        float fOffsetY = x * g_fOffsetY;
        TEXEL_TYPE vSample = SampleAtOffset( texFilter, vTexCoord, fOffsetX, fOffsetY );
        vOutput.DEST_CHANNEL_MASK += vSample * g_fKernelWeights[x+g_iHalfWidth]; 
    }
    
    if( g_bSigned )
    {
        vOutput = EncodeSignedAsUnsigned( vOutput );
    }

    return vOutput;
}
 
 
//---------------------------------------------------------------------------------------------------------
// Name: BilateralFalloff()
// Desc: Defines how much to discount bilateral samples, based on how far off they are in value from
// the center sample.
//---------------------------------------------------------------------------------------------------------
float2 g_vBilateralPeakThickness : FLOAT_REGISTER(reg_BilateralPeakThickness); // .x thickness in depth

float BilateralFalloff( TEXEL_TYPE vSample, TEXEL_TYPE vCenterSample, float fPeakThickness )
{
    // For TEXEL_TYPE of 'float' this collapses to: one add, two mul's, and 4 exp's for the
    // 4 off-center samples collectively.
    
    TEXEL_TYPE vDiff = vSample - vCenterSample;
    return exp2( -fPeakThickness * length( vDiff ) * length( vDiff ) );
}


//---------------------------------------------------------------------------------------------------------
// Name: BilateralFilter1DPS()/BilateralFilter2DPS()
// Desc: We want to smooth depth to reduce noise.  But we don't want to smooth across unrelated features
// --- for instance between the player and the back wall.  
//
// Bilateral filtering addresses these requirements by dynamically scaling down sample weights for 
// samples with very different value from the the reference value.  Unfortunately, a consequence is
// that the filter becomes non-separable and therefore much more expensive in terms of execution time.
//
// Bilateral filters must point-sample each texel, so technically, they cannot be optimized using 
// hardware bilinear filtering.  However, you can sometimes treat them as separable and get okay results.
//---------------------------------------------------------------------------------------------------------

//---------------------------------------------------------------------------------------------------------
// Name: BilateralFilter1DPS()
// Desc: Fake a separable bilateral filter (as described above).
//---------------------------------------------------------------------------------------------------------
float4 BilateralFilter1DPS( float2 vTexCoord : TEXCOORD0 ) : COLOR0
{
    float4 vOutput = 0.0f;
    TEXEL_TYPE vSamples[2*g_iHalfWidth+1];
    for( int x = -g_iHalfWidth; x <= g_iHalfWidth; ++x )
    {
        float fOffsetX = x * g_fOffsetX;
        float fOffsetY = x * g_fOffsetY;
        vSamples[x+g_iHalfWidth] = SampleAtOffset( texFilter, vTexCoord, fOffsetX, fOffsetY );
    }
    
    float fTotalWeight = 0.0f;
    TEXEL_TYPE vWeightedSum = 0.0f;
    TEXEL_TYPE vCenterSample = vSamples[g_iHalfWidth];
    for( int x = -g_iHalfWidth; x <= g_iHalfWidth; ++x )
    {
        TEXEL_TYPE vSample = vSamples[x+g_iHalfWidth];
        float fWeight = g_fKernelWeights[x+g_iHalfWidth];
        
        // Bilateral stuff
        float fBilateralFalloff = BilateralFalloff( vSample, vCenterSample, g_vBilateralPeakThickness.x );
        fWeight *= fBilateralFalloff;
        
        fTotalWeight += fWeight;
        vWeightedSum += fWeight * vSample;
    }

    vOutput.DEST_CHANNEL_MASK = vWeightedSum / fTotalWeight;
    
    if( g_bSigned )
    {
        vOutput = EncodeSignedAsUnsigned( vOutput );
    }

    return vOutput;
}


//---------------------------------------------------------------------------------------------------------
// Name: BilateralFilter2DPS()
// Desc: Bilateral filters are not separable, so technically should be 2D.  The compiler will blow up if
// you pass it this shader with too many fetches, or with fewer fetches but too many channels.
//---------------------------------------------------------------------------------------------------------
float4 BilateralFilter2DPS( float2 vTexCoord : TEXCOORD0 ) : COLOR0
{
    float4 vOutput = 0.0f;
    TEXEL_TYPE vSamples[2*g_iHalfWidth+1][2*g_iHalfWidth+1];
    for( int x = -g_iHalfWidth; x <= g_iHalfWidth; ++x )
    {
        for( int y = -g_iHalfWidth; y <= g_iHalfWidth; ++y )
        {
            vSamples[x+g_iHalfWidth][y+g_iHalfWidth] = SampleAtOffset( texFilter, vTexCoord, x, y );
        }
    }
    
    float fTotalWeight = 0.0f;
    TEXEL_TYPE vWeightedSum = 0.0f;
    TEXEL_TYPE vCenterSample = vSamples[g_iHalfWidth][g_iHalfWidth];
    for( int x = -g_iHalfWidth; x <= g_iHalfWidth; ++x )
    {
        for( int y = -g_iHalfWidth; y <= g_iHalfWidth; ++y )
        {
            TEXEL_TYPE vSample = vSamples[x+g_iHalfWidth][y+g_iHalfWidth];
            float fWeight = g_fKernelWeights[x+g_iHalfWidth] * g_fKernelWeights[y+g_iHalfWidth];
            
            // Bilateral stuff
            float fBilateralFalloff = BilateralFalloff( vSample, vCenterSample, g_vBilateralPeakThickness.x );
            fWeight *= fBilateralFalloff;
            
            fTotalWeight += fWeight;
            vWeightedSum += fWeight * vSample;
        }
    }

    vOutput.DEST_CHANNEL_MASK = vWeightedSum / fTotalWeight;
    
    if( g_bSigned )
    {
        vOutput = EncodeSignedAsUnsigned( vOutput );
    }

    return vOutput;
}


//---------------------------------------------------------------------------------------------------------
// Name: NuiDepthToProjectiveDepth()
// Desc: Given z in Nui depth units (mm), produce z appropriate for use in a depth buffer.
//---------------------------------------------------------------------------------------------------------
float2 g_vNearFarDist        : FLOAT_REGISTER(reg_NearFarDist);  // .x = near distance
                                                                        // .y = far distance

float NuiDepthToProjectiveDepth( float fNuiDepth )
{
    float fProjZ = ( fNuiDepth - g_vNearFarDist.x ) * g_vNearFarDist.y 
        / ( g_vNearFarDist.y - g_vNearFarDist.x );
    float fProjW = fNuiDepth;
    float fProjDepth = fProjZ / fProjW;
    
    return fProjDepth;
}


//---------------------------------------------------------------------------------------------------------
// Name: DepthToNormalPS()
// Desc: Given the depth map, generate an approximate viewspace normal map.
//---------------------------------------------------------------------------------------------------------
//sampler2D texDepth : SAMPLER_REGISTER(reg_depth);    // declared earlier

float4 DepthToNormalPS( float2 vTexCoord : TEXCOORD0 ) : COLOR0
{
    // Gather depth samples at offsets [0,0], [1,0], [0,1]
    // Use linear filtering to support combinations where one of depth/normal is half-res
    float3 depths;
    asm
    {
        tfetch2D depths.x___, vTexCoord, texDepth, OffsetX = 0.0, OffsetY = 0.0, MinFilter=linear, MagFilter=linear
        tfetch2D depths._x__, vTexCoord, texDepth, OffsetX = 1.0, OffsetY = 0.0, MinFilter=linear, MagFilter=linear
        tfetch2D depths.__x_, vTexCoord, texDepth, OffsetX = 0.0, OffsetY = 1.0, MinFilter=linear, MagFilter=linear
    };
    
    // Remove the segmentation bits, leaving units of mm
    // Probably no point in this, since we don't care about behavior across edges
    // Also, better to keep fractional precision in later passes
    //depths -= frac( depths );
    
    // Recover the 3 worldspace sample coordinates
    float3 vWorld00 = NuiToWorld( float3( vTexCoord, depths.x ) );
    float3 vWorld10 = NuiToWorld( float3( vTexCoord + float2( g_vTexDims.z, 0 ), depths.y ) );
    float3 vWorld01 = NuiToWorld( float3( vTexCoord + float2( 0, g_vTexDims.w ), depths.z ) );
    
    // From the change in depth in the x and the y direction, compute the viewspace normal vector
    float3 vTangent = vWorld10 - vWorld00;
    float3 vBinormal = vWorld01 - vWorld00;
    float3 vNormal = normalize( cross( vTangent, vBinormal ) );
    
    // Encode signed into unsigned, to avoid clamping to 0.0f when writing to unsigned render target
    return EncodeSignedAsUnsigned( float4( vNormal, 0.0f ) );
}


//---------------------------------------------------------------------------------------------------------
// Name: DownsampleNormalPS()
// Desc: Downsample normal map to produce mips.  
//---------------------------------------------------------------------------------------------------------
//sampler2D texNormal  : SAMPLER_REGISTER(reg_normal);

float4 DownsampleNormalPS( float2 vTexCoord : TEXCOORD0 ) : COLOR0
{
    float3 vNormal;
    asm
    {
        tfetch2D vNormal.xyz_, vTexCoord, texNormal, MinFilter=linear, MagFilter=point, MipFilter=point
    };

    return EncodeSignedAsUnsigned( float4( normalize( vNormal ), 0.0f ) );
}


//---------------------------------------------------------------------------------------------------------
// DepthVisualizer shaders.  Used to produce intuitive views of the depth data.
//---------------------------------------------------------------------------------------------------------
//sampler2D texDepth : SAMPLER_REGISTER(reg_depth); // declared earlier
//sampler2D texNormal : SAMPLER_REGISTER(reg_normal); // declared earlier

//float2 g_vTwiceTanHalfFOV          : FLOAT_REGISTER(reg_TwiceTanHalfFOV); // declared earlier
float4 g_vSweetSpotOriginAndRadius   : FLOAT_REGISTER(reg_SweetSpotOriginAndRadius); 
float4 g_vSweetSpotDirection         : FLOAT_REGISTER(reg_SweetSpotDirection); 
//float2 g_vNearFarDist                : FLOAT_REGISTER(reg_NearFarDist);    // .x = near distance
                                                                                    // .y = far distance
float4 g_vFloorPlane                 : FLOAT_REGISTER(reg_FloorPlane);       // .xyz = plane normal
                                                                                    // .w = dist from origin
float4 g_vSweeperPlane               : FLOAT_REGISTER(reg_SweeperPlane);     // .xyz = plane normal
                                                                                    // .w = dist from origin
float3 g_vSweetSpotColor             : FLOAT_REGISTER(reg_SweetSpotColor);
float3 g_vFloorPlaneColor            : FLOAT_REGISTER(reg_FloorPlaneColor);
float3 g_vSweeperPlaneColor          : FLOAT_REGISTER(reg_SweeperPlaneColor);
float3 g_vOutlineColor               : FLOAT_REGISTER(reg_OutlineColor);
float3 g_vNormalFadeParams           : FLOAT_REGISTER(reg_NormalFadeParams);
bool g_bDrawFloorPlane               : BOOL_REGISTER(reg_DrawFloorPlane);
bool g_bSweeperOn                    : BOOL_REGISTER(reg_SweeperOn);
bool g_bFadeNormal                   : BOOL_REGISTER(reg_FadeNormal);
//bool g_bRenormalize                  : BOOL_REGISTER(reg_Renormalize);


//---------------------------------------------------------------------------------------------------------
// Name: DistanceFromLine()
// Desc: How far is the point from the line?  
//---------------------------------------------------------------------------------------------------------
float DistanceFromLine( float3 vPoint, float3 vLineOrigin, float3 vLineDirection )
{
    return length( cross( vPoint - vLineOrigin, vLineDirection ) );
}


//---------------------------------------------------------------------------------------------------------
// Name: DistanceFromPlane()
// Desc: How far is the point from the plane? 
//---------------------------------------------------------------------------------------------------------
float DistanceFromPlane( float3 vPoint, float4 vPlane )
{
    // shader compiler can implement this as a single dot4
    return dot( vPoint, vPlane.xyz ) + vPlane.w;
}


//---------------------------------------------------------------------------------------------------------
// Name: DepthMapVisualizeInteriorPixel()
// Desc: Helper computation for pixels not lying on one of the highlighted contours.  This function 
// produces the output for most areas of the image.
//---------------------------------------------------------------------------------------------------------
float3 DepthMapVisualizeInteriorPixel( float2 vTexCoord, 
    float fDepth, 
    float fNormalizedSweetSpotDist, 
    bool bAboveFloorPlane, 
    bool bInsideSweetSpot )
{
    // Read the normal, optionally biasing to smoother values at greater distance from the sweetspot.
    float fLodBias = 0.0f;
    if( g_bFadeNormal )
    {
        fLodBias = g_vNormalFadeParams.z * saturate( g_vNormalFadeParams.x * fNormalizedSweetSpotDist + g_vNormalFadeParams.y );
    }
    float3 vNormal;
    asm
    {
        setTexLOD fLodBias
        tfetch2D vNormal.xyz_, vTexCoord, texNormal, MinFilter=linear, MagFilter=linear, MipFilter=linear, UseRegisterLOD=true, UseComputedLOD=false
    };
    if( g_bRenormalize )
    {
        vNormal = normalize( vNormal );
    }
    
    // Convert signed to biased (to avoid clamping when we write this to a color surface)
    float3 vColor = EncodeSignedAsBias( float4( vNormal, 0.0f ) ).xyz;
    
    float fIntensity;
    float fOutlineIntensity;

    [flatten]
    if( bAboveFloorPlane && bInsideSweetSpot )
    {
        // Fully bright area
        fIntensity = 1.0f;
        fOutlineIntensity = 1.0f;
    }    
    else
    {
        // Dimmer as we get farther from the sweet spot --- this factor is 1.0f inside the sweet spot 
        // and fades to 0.0f at a distance of one radius outside the cylinder
        float fSweetSpotLerpFactor = saturate( 1.0f - fNormalizedSweetSpotDist );
        fIntensity = lerp( 0.10f, 0.25f, fSweetSpotLerpFactor );
        fOutlineIntensity = lerp( 0.15f, 0.50f, fSweetSpotLerpFactor ); 
    }

    // Outline only on depth discontinuities.  The ddx/ddy are coalesced into one getGradients instruction.
    float fDepthJump = sqrt( ddx( fDepth ) * ddx( fDepth ) + ddy( fDepth ) * ddy( fDepth ) );
    fDepthJump = saturate( fDepthJump - 50.0f );    // 5 cm threshold
    fOutlineIntensity *= fDepthJump;

    // Distinguish regions of different depth, using different intensity (pretty subtle distinctions though)
    float fDepthScale = saturate( 3.0f * ( fDepth / g_fFarDepth ) );
    float3 fDepthColor = lerp( 0.3f, 0.05f, fDepthScale );

    // Inside sweetspot - bias to saturated colors; outside sweetspot - bias toward grayscale
    vColor = lerp( fDepthColor, vColor, fIntensity );

    // Add the outlines near feature edges
    vColor = lerp( vColor, g_vOutlineColor, fOutlineIntensity );

    return vColor;
}

//---------------------------------------------------------------------------------------------------------
// Name: DepthMapVisualizePS()
// Desc: Draw an interesting representation of the depth map and playspace.  We choose to simply
// render the normals as color.  You could equally well employ the normals with any artificial lighting
// environment.  The important thing, for our purposes, is to have some way to convey surface
// detail.
//---------------------------------------------------------------------------------------------------------
float4 DepthMapVisualizePS( float2 vTexCoord : TEXCOORD0, out float fProjDepth : DEPTH ) : COLOR0
{
    // Use linear mag-filtering to support combinations where the visualization is higher-res than the sources
    float fDepth;
    asm
    {
        tfetch2D fDepth.x___, vTexCoord, texDepth, MinFilter=point, MagFilter=linear, MipFilter=point
    };

    // Depth output --- this will allow subsequent geometry to depth test correctly, 
    // as if it were part of the scene
    fProjDepth = NuiDepthToProjectiveDepth( fDepth );
    
    // Convert to world space
    float3 vPosWorld = NuiToWorld( float3( vTexCoord, fDepth ) );
    
    // Do a bunch of tests against the synthetic geometry
    float fNormalizedSweetSpotDist = DistanceFromLine( vPosWorld, g_vSweetSpotOriginAndRadius.xyz, g_vSweetSpotDirection )
        / g_vSweetSpotOriginAndRadius.w - 1.0f; // 0.0f means on boundary, -1.0f means on the axis
    float fFloorPlaneDist = DistanceFromPlane( vPosWorld, g_vFloorPlane );
    float fSweeperPlaneDist = DistanceFromPlane( vPosWorld, g_vSweeperPlane );
    
    // Classify what situation we're in with respect to the synthetic geometry (e.g. near, inside, outside, above, below, etc.) 
    bool bInsideSweetSpot = fNormalizedSweetSpotDist <= 0.0f;
    bool bNearSweetSpotBoundary = abs( fNormalizedSweetSpotDist ) < 0.01f;      // within 1% of radius
    bool bNearFloorPlane = g_bDrawFloorPlane && abs( fFloorPlaneDist ) < 10.0f; // within 1 cm
    bool bAboveFloorPlane = !g_bDrawFloorPlane || fFloorPlaneDist > 0.0f;
    bool bNearSweeperPlane = g_bSweeperOn && abs( fSweeperPlaneDist ) < 5.0f;   // within 0.5 cm
    
    if( bNearFloorPlane )
    {
        // Intersection contour of object and floor
        return float4( g_vFloorPlaneColor, 1.0f );
    }
    else if( bAboveFloorPlane && bNearSweetSpotBoundary )
    {
        // Intersection contour of object and sweet spot boundary
        return float4( g_vSweetSpotColor, 1.0f );
    }
    else if( bAboveFloorPlane && bNearSweeperPlane )
    {
        // Intersection contour of object and sweeper plane
        return float4( bInsideSweetSpot ? g_vSweetSpotColor : g_vSweeperPlaneColor, 1.0f );
    }
    else
    {   
        // Pixel does not lie on any intersection contour
        float3 vColor = DepthMapVisualizeInteriorPixel( vTexCoord, fDepth, fNormalizedSweetSpotDist, 
            bAboveFloorPlane, bInsideSweetSpot );
        return float4( vColor, 1.0f );
    }
}


//---------------------------------------------------------------------------------------------------------
// Name: SyntheticPlanePS()
// Desc: Visualize one of the sweeper planes, or the floor plane, as if it were part of the depth scene. 
//---------------------------------------------------------------------------------------------------------
//float3 g_vSweetSpotColor             : FLOAT_REGISTER(reg_SweetSpotColor);
//float4 g_vGenericColor               : FLOAT_REGISTER(reg_GenericColor);
//float4 g_vSweetSpotOriginAndRadius   : FLOAT_REGISTER(reg_SweetSpotOriginAndRadius); 
//float4 g_vSweetSpotDirection         : FLOAT_REGISTER(reg_SweetSpotDirection); 
float4x3 g_matPlaneToWorld     : FLOAT_REGISTER(reg_Matrix); 
                                                    
float4 SyntheticPlanePS( float2 vTexCoord : TEXCOORD0 ) : COLOR0
{
    float3 vPosWorld = mul( float4( ( vTexCoord - 0.5f ) * 2.0f, 0.0f, 1.0f ), g_matPlaneToWorld ).xyz;
    
    // Do a tests against the synthetic geometry
    float fNormalizedSweetSpotDist = DistanceFromLine( vPosWorld, g_vSweetSpotOriginAndRadius.xyz, g_vSweetSpotDirection )
        / g_vSweetSpotOriginAndRadius.w - 1.0f; // 0.0f means on boundary, -1.0f means on the axis
    
    bool bInsideSweetSpot = fNormalizedSweetSpotDist <= 0.0f;
    bool bNearSweetSpotBoundary = abs( fNormalizedSweetSpotDist ) < 0.02f;
    
    if( bNearSweetSpotBoundary )
    {
        return float4( g_vSweetSpotColor, 1.0f );
    }
    else if ( bInsideSweetSpot )
    {
        return 0.0f;
    }
    else 
    {
        return g_vGenericColor;
    }
}


//---------------------------------------------------------------------------------------------------------
// Name: DepthThumbnailPS()
// Desc: View the processed depth map, including color coding of regions of interest.
//---------------------------------------------------------------------------------------------------------
//sampler2D texDepth                                      : SAMPLER_REGISTER(reg_depth);
//sampler2D texMask                                       : SAMPLER_REGISTER(reg_mask);
bool g_bColorCode                                : BOOL_REGISTER(reg_ColorCode);
bool g_bMaskValid                                : BOOL_REGISTER(reg_MaskValid);
float4 g_vColorCodeColors[COLOR_CODE_COUNT]      : FLOAT_REGISTER(reg_ColorCodeColors);

float4 DepthThumbnailPS( float2 vTexCoord : TEXCOORD0 ) : COLOR0
{
    float fDepth = tex2D( texDepth, vTexCoord );
    bool4 vMask = tex2D( texMask, vTexCoord );

    // Filtering may give ambiguous results for whether a particular depth is valid
    bool bAdjustedDepthValid = ( fDepth > 0.0f && fDepth < g_fFarDepth * 0.9f );  
    bool bIRShadow = g_bMaskValid && GetIsIRShadow( vMask );
    bool bHole = g_bMaskValid && GetIsSmallHole( vMask );
    bool bIsland = g_bMaskValid && GetIsSmallIsland( vMask );
    bool bOrigDepthValid = bAdjustedDepthValid && !bIRShadow && !bHole && !bIsland;
    bool bResolvedIRShadow = bIRShadow && bAdjustedDepthValid;
    bool bUnresolvedIRShadow = bIRShadow && !bAdjustedDepthValid;
    bool bResolvedHole = bHole && bAdjustedDepthValid;
    bool bUnresolvedHole = bHole && !bAdjustedDepthValid;

    if( !g_bColorCode || bOrigDepthValid )
    {
        return fDepth / g_fFarDepth;
    }
    else if( bIsland )
    {
        return g_vColorCodeColors[COLOR_CODE_SMALL_ISLAND];
    }
    else if( bResolvedIRShadow )
    {
        return g_vColorCodeColors[COLOR_CODE_RESOLVED_IR_SHADOW];
    }
    else if( bUnresolvedIRShadow )
    {
        return g_vColorCodeColors[COLOR_CODE_UNRESOLVED_IR_SHADOW];
    }
    else if( bResolvedHole )
    {
        return g_vColorCodeColors[COLOR_CODE_RESOLVED_SMALL_HOLE];
    }
    else if( bUnresolvedHole )
    {
        return g_vColorCodeColors[COLOR_CODE_UNRESOLVED_SMALL_HOLE];
    }
    else    // no sensor depth and no adjusted depth
    {
        return g_vColorCodeColors[COLOR_CODE_NO_DEPTH];
    }
}
    
    

//---------------------------------------------------------------------------------------------------------
// FastUntile.hlsl
//
// File containing utility shaders used by the FastUntile sample
//
// XNA Developer Connection
// Copyright ( C ) Microsoft Corporation. All rights reserved.
//---------------------------------------------------------------------------------------------------------

struct VERTEX
{
    float2 vPosition    : POSITION0;
    float2 vTexCoord    : TEXCOORD0;
};

struct INTERPOLATORS
{
    float4 vPosition     : POSITION0;
    float2 vTexCoord     : TEXCOORD0;
};

//---------------------------------------------------------------------------------------------------------
// Name: CopyTextureVS()
// Desc: Simple pass through. 
//---------------------------------------------------------------------------------------------------------
INTERPOLATORS CopyTextureVS( VERTEX In )
{
    INTERPOLATORS Out;
    Out.vPosition = float4( In.vPosition, 0.0f, 1.0f );
    Out.vTexCoord = In.vTexCoord;
    
    return Out;
}


//---------------------------------------------------------------------------------------------------------
// Name: CopyTexturePS()
// Desc: Copies a texture. 
//---------------------------------------------------------------------------------------------------------
sampler tex        : register(s0);

float4 CopyTexturePS( INTERPOLATORS In ) : COLOR
{
    float4 vColor = tex2D( tex, In.vTexCoord );
    return vColor;
}


//---------------------------------------------------------------------------------------------------------
// Name: BitwiseCompareTexturesPS()
// Desc: Debug shader:  Outputs green if two texels are identical, red if they differ at all. 
//---------------------------------------------------------------------------------------------------------
sampler tex0        : register(s0);
sampler tex1        : register(s1);

static const float4 g_vGreen =  { 0.0f, 1.0f, 0.0f, 1.0f };
static const float4 g_vRed =    { 1.0f, 0.0f, 0.0f, 1.0f };

float4 BitwiseCompareTexturePS( INTERPOLATORS In ) : COLOR
{
    float4 vColor0 = tex2D( tex0, In.vTexCoord );
    float4 vColor1 = tex2D( tex1, In.vTexCoord );
    if ( vColor0.r == vColor1.r && vColor0.g == vColor1.g && vColor0.b == vColor1.b && vColor0.a == vColor1.a ) 
    {
        return g_vGreen;
    }
    else
    {
        return g_vRed;
    }
}


//---------------------------------------------------------------------------------------------------------
// Name: UntileMemexportTexelPS()
// Desc: Fetch from a texture (tiled or linear) and memexport to linear arrangement, 
// using one fetch and one export.  
//---------------------------------------------------------------------------------------------------------
//sampler2D tex                       : register(s0);   // declared earlier

float3 g_vTexDims                   : register(c0); // .x = width in texels
                                                    // .y = height in texels
                                                    // .z = row pitch in texels
static float4 const01               = float4( 0, 1, 0, 0 );
float4 g_fMemExportStreamConstant   : register(c1);

float4 UntileMemexportTexelPS( float2 vTexCoord : TEXCOORD0 ) : COLOR0
{
    float4 vTexel = tex2D( tex, vTexCoord );
    float fLinearOffset = dot( floor( vTexCoord * g_vTexDims ), float2( 1.0f, g_vTexDims.z ) );
    asm
    {
        alloc export=1
        mad eA, fLinearOffset, const01, g_fMemExportStreamConstant
        mov eM0, vTexel
    };
    return 0.0f;
}


//---------------------------------------------------------------------------------------------------------
// Name: UntileMemexportTransactionPS()
// Desc: Fetch from a texture (tiled or linear) and memexport to linear arrangement, 
// using enough fetches and exports to fill a 32-byte memory transaction.  The 
// number of fetches and exports is communicated by preprocessor terms:
//
//  - g_iFetchBytes (number of bytes per fetch)
//  - g_iMemExportBytes (number of bytes per memexport)
//
// The sample compiles this entrypoint multiple times, with multiple choices of these
// two constants.
//
// Note: The incoming TexCoords are based off a render target which is narrower than the
// texture, because we are processing several texels per "pixel". 
//
// Note: We have a special case for 8-bit formats.  These cannot naturally fill a 32 bit
// transaction.  There are only 5 memexport registers, so we can only export 20 values
// in a memexport bracket.  We choose to fill half a transaction, or 16 texels, in this case.
//---------------------------------------------------------------------------------------------------------

// Don't let the compiler see the entrypoint unless the prerequisite quantities
// are declared in the compile line
#if defined( g_iFetchBytes ) && defined( g_iMemExportBytes )

// Granularity of hardware memory transactions
#define g_iMemTransactionBytes  32   

// Number of fetches needed to fill a transaction
#define g_iFetchCount   ( g_iMemTransactionBytes / g_iFetchBytes )

// Number of memexports needed to fill a transaction
#define g_iMemExportCount   ( g_iMemTransactionBytes / g_iMemExportBytes )

// If possible, we always export 4 texels per 'alloc export' bracket.
#if ( ( g_iMemExportCount / 4 ) * 4 == g_iMemExportCount )
#define g_iMemExportRegisters 4
#elif ( ( g_iMemExportCount / 2 ) * 2 == g_iMemExportCount )
#define g_iMemExportRegisters 2
#else
#define g_iMemExportRegisters 1
#endif

// Number of fetches needed to fill a memexport
#define g_iFetchesPerMemExport   ( g_iFetchCount / g_iMemExportCount )

#if ( g_iFetchesPerMemExport * g_iMemExportCount != g_iFetchCount )
#error Must have an integer number of fetches per memexport
#endif
#if ( g_iFetchesPerMemExport != 1 && g_iFetchesPerMemExport != 2 && g_iFetchesPerMemExport != 4 && g_iFetchesPerMemExport != 8 )
#error Shader only supports 1, 2, 4 or 8 fetches per memexport
#endif

// These shader constants are all declared above, but included here for completeness
//sampler2D tex                       : register(s0);   // declared earlier

//float3 g_vTexDims                   : register(c0); // .x = width in texels
                                                    // .y = height in texels
                                                    // .z = row pitch in texels
//static float4 const01               = float4( 0, 1, 0, 0 );
//float4 g_fMemExportStreamConstant   : register(c1);

float4 UntileMemexportTransactionPS( float2 vTexCoord : TEXCOORD0 ) : COLOR0
{
    float fLinearOffset = dot( round ( vTexCoord * g_vTexDims ), float2( 1.0f, g_vTexDims.z ) )
        / g_iFetchesPerMemExport;
    
    vTexCoord += 0.5f / g_vTexDims; // Can't use D3DRS_HALFPIXELOFFSET, because render target is reduced-width
    float2 vTexCoordTexel = vTexCoord;
    [unroll]    // one memexport block per iteration
    for( int j = 0; j < g_iMemExportCount; j += g_iMemExportRegisters )
    {
        float4 vTexel[g_iMemExportRegisters * g_iFetchesPerMemExport];

        [unroll]    // one fetch per iteration
        for( int i = 0; i < g_iMemExportRegisters * g_iFetchesPerMemExport; ++i )
        {
            vTexel[i] = tex2D( tex, vTexCoordTexel );
            vTexCoordTexel.x += 1.0f / g_vTexDims;
        }
    
        float4 vData0, vData1, vData2, vData3;
        switch( g_iFetchesPerMemExport )    // pack 4 values into each memexport register
        {
        case 1:
        default:
            vData0 = vTexel[0], vData1 = vTexel[1], vData2 = vTexel[2], vData3 = vTexel[3];
            break;
        case 2:
            vData0 = float4( vTexel[0].xy, vTexel[1].xy );
            vData1 = float4( vTexel[2].xy, vTexel[3].xy );
            vData2 = float4( vTexel[4].xy, vTexel[5].xy );
            vData3 = float4( vTexel[6].xy, vTexel[7].xy );
            break;
        case 4:
            vData0 = float4( vTexel[ 0].x, vTexel[ 1].x, vTexel[ 2].x, vTexel[ 3].x );
            vData1 = float4( vTexel[ 4].x, vTexel[ 5].x, vTexel[ 6].x, vTexel[ 7].x );
            vData2 = float4( vTexel[ 8].x, vTexel[ 9].x, vTexel[10].x, vTexel[11].x );
            vData3 = float4( vTexel[12].x, vTexel[13].x, vTexel[14].x, vTexel[15].x );
            break;
        }
        
        switch( g_iMemExportRegisters ) // memxport as many registers per block as we can
        {
        case 1:
            asm
            {
                alloc export=1
                mad eA, fLinearOffset, const01, g_fMemExportStreamConstant
                mov eM0, vData0
            };
            break;
        case 2:
            asm
            {
                alloc export=2
                mad eA, fLinearOffset, const01, g_fMemExportStreamConstant
                mov eM0, vData0
                mov eM1, vData1
            };
            break;
        case 4:
        default:
            asm
            {
                alloc export=2
                mad eA, fLinearOffset, const01, g_fMemExportStreamConstant
                mov eM0, vData0
                mov eM1, vData1
                mov eM2, vData2
                mov eM3, vData3
            };
            break;
        }
        fLinearOffset += g_iMemExportRegisters;
    }
    return 0.0f;
}

#endif  // #if defined( g_iFetchBytes ) && defined( g_iMemExportBytes )


//---------------------------------------------------------------------------------------------------------
// Name: UntileResolvePS()
// Desc: Write the texture data to EDRAM, but pre-adjust texture coordinates, so that 
// the natural tiling operation during Resolve will produce a linear result.  
//
// The shader must know the dimensions of a "repeat block" --- which is a rectangle
// such that the tiling pattern over that region is repeated over the entire texture.  
// The repeat block dimensions are communicated by preprocessor terms:
//
//  - g_fRepeatBlockWidth 
//  - g_fRepeatBlockHeight 
//
// The sample compiles this entrypoint multiple times, with multiple choices of these
// two constants.
//---------------------------------------------------------------------------------------------------------

// Don't let the compiler see the entrypoint unless the prerequisite quantities
// are declared in the compile line
#if defined( g_fRepeatBlockWidth ) && defined( g_fRepeatBlockHeight )

// For 128-bit textures, there is no matching render target.  So we use a double-wide
// 64-bit render target and alternately output AB and GR.
#ifndef g_iPixelsPerTexel
#define g_iPixelsPerTexel 1
#endif

//sampler2D tex                       : register(s0);   // defined earlier
sampler1D texInverseTilingPattern   : register(s1);

const static float2 g_vRepeatBlockDims = float2( g_fRepeatBlockWidth, g_fRepeatBlockHeight );
// float3 g_vTexDims                   : register(c0); // .x = width in texels
                                                    // .y = height in texels
                                                    // .z = row pitch in texels
float4 g_vTexDimsInRepeatBlocks     : register(c1); // .x = width in repeat blocks
                                                    // .y = height in repeat blocks
                                                    // .z = row pitch in repeat blocks
                                                    // .w = repeat block width in texels --- a convenience

float4 UntileResolvePS( float2 vTexCoord : TEXCOORD0 ) : COLOR0
{
    //-----------------------------------------------------------------------------------------------------
    // Convert regular TexCoords into macro and micro TexCoords.
    // Macro TexCoords are in units of repeat blocks, and will not change for untiling.
    // Micro TexCoords are internal to a repeat block, and will be remapped through the 
    // lookup texture.
    //-----------------------------------------------------------------------------------------------------
    float2 vMacroTexCoord = floor( vTexCoord * g_vTexDimsInRepeatBlocks );
    float2 vMicroTexCoords = floor( vTexCoord * g_vTexDims - vMacroTexCoord * g_vRepeatBlockDims );
    
    float fMacroLinearOffset = vMacroTexCoord.x + vMacroTexCoord.y * g_vTexDimsInRepeatBlocks.z;
    float fMicroLinearOffset = vMicroTexCoords.x + vMicroTexCoords.y * g_vTexDimsInRepeatBlocks.w;
     
    //-----------------------------------------------------------------------------------------------------
    // Perform the micro-TexCoords remapping using the lookup texture.
    //-----------------------------------------------------------------------------------------------------
    float fMacroInverseTiledOffset = fMacroLinearOffset;
    float fMicroInverseTiledOffset;
    asm
    {
        tfetch1D fMicroInverseTiledOffset.x___, texInverseTilingPattern, fMicroLinearOffset, \
            UnnormalizedTextureCoords = true
    } ;
        
    //-----------------------------------------------------------------------------------------------------
    // Regenerate normal TexCoords from macro and micro TexCoords.
    //-----------------------------------------------------------------------------------------------------
    float fInverseTiledOffset = fMacroInverseTiledOffset * g_vRepeatBlockDims.x * g_vRepeatBlockDims.y
        + fMicroInverseTiledOffset;

    float2 vInverseTiledTexCoord;
    vInverseTiledTexCoord.y = floor( ( fInverseTiledOffset + 0.5f ) / g_vTexDims.z );
    vInverseTiledTexCoord.x = fInverseTiledOffset - vInverseTiledTexCoord.y * g_vTexDims.z;

    //-----------------------------------------------------------------------------------------------------
    // For 128-bit formats the render target width is twice the texture width
    //-----------------------------------------------------------------------------------------------------
    vInverseTiledTexCoord.x /= g_iPixelsPerTexel;

    //-----------------------------------------------------------------------------------------------------
    // The actual fetch from the source texture
    //-----------------------------------------------------------------------------------------------------
    float4 vTexel;
    asm
    {
        tfetch2D vTexel, tex, vInverseTiledTexCoord, UnnormalizedTextureCoords = true
    } ;
    
    //-----------------------------------------------------------------------------------------------------
    // For 128-bit formats we can only output two channels per pixel
    //-----------------------------------------------------------------------------------------------------
    return ( frac( vMicroTexCoords.x / g_iPixelsPerTexel ) == 0.0f ) ? vTexel.xyzw : vTexel.zwxy;
}

#endif  // #if defined( g_fRepeatBlockWidth ) && defined( g_fRepeatBlockHeight )


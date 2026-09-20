//---------------------------------------------------------------------------------------------------------
// FastBlockCompress.cpp
//
// XNA Developer Connection
// Copyright ( C ) Microsoft Corporation. All rights reserved.
//---------------------------------------------------------------------------------------------------------
#include <xtl.h>    // must come before the others
#include <d3dx9.h>
#include <tracerecording.h>
#include <xgraphics.h>

#include <AtgApp.h>
#include <AtgFont.h>
#include <AtgHelp.h>
#include <AtgInput.h>
#include <AtgUtil.h>

#include "FastBlockCompress.h"
#include "FastBlockCompressVMX.h"
#include "FastBlockCompressGPU.h"

// Define a symbol that is used to compile out the use of the GPU performance counter APIs 
// when using a release build of Direct3D.  The GPU performance counter APIs only work with 
// d3d9i.lib and d3d9d.lib.
#if defined( NDEBUG ) && ( !defined( PROFILE ) || defined( FASTCAP ) )
#define _RELEASED3D
#endif


//---------------------------------------------------------------------------------------------------------
// Callouts for labeling the gamepad on the help screen
//---------------------------------------------------------------------------------------------------------
ATG::HELP_CALLOUT g_HelpCallouts[] =
{
    { ATG::HELP_BACK_BUTTON,    ATG::HELP_PLACEMENT_2, L"Display\nhelp" },
    { ATG::HELP_B_BUTTON,       ATG::HELP_PLACEMENT_2, L"Toggle\nBig/small menu" },
    { ATG::HELP_X_BUTTON,       ATG::HELP_PLACEMENT_2, L"Flashing\ndiffs" },
    { ATG::HELP_Y_BUTTON,       ATG::HELP_PLACEMENT_2, L"PIX Trace capture\ncompression & tiling" },
    { ATG::HELP_DPAD,           ATG::HELP_PLACEMENT_2, 
        L"Change item focus (u/d)\nDecrease / increase active item (l/r)" },
    { ATG::HELP_LEFT_STICK,     ATG::HELP_PLACEMENT_2, 
        L"Change item focus (u/d)\nDecrease / increase active item (l/r)" },
    { ATG::HELP_RIGHT_STICK,    ATG::HELP_PLACEMENT_2, 
        L"Translate image u/d/r/l\nClick to reset camera" },
    { ATG::HELP_LEFT_TRIGGER,   ATG::HELP_PLACEMENT_1, L"Zoom out" },
    { ATG::HELP_RIGHT_TRIGGER,  ATG::HELP_PLACEMENT_1, L"Zoom in" },
};
#define NUM_HELP_CALLOUTS _countof( g_HelpCallouts )


//---------------------------------------------------------------------------------------------------------
// Custom CTX1/DXN formats which fetch 1.0 in AB rather than repeating GR.
// This matches what D3DFMT_G8R8 does.
//---------------------------------------------------------------------------------------------------------
static const D3DFORMAT D3DFMT_CTX1_GR = ( D3DFORMAT ) MAKED3DFMT(
    GPUTEXTUREFORMAT_CTX1, 
    GPUENDIAN_8IN16, 
    TRUE, 
    GPUSIGN_ALL_UNSIGNED, 
    GPUNUMFORMAT_FRACTION, 
    GPUSWIZZLE_OOGR );
static const D3DFORMAT D3DFMT_DXN_GR = ( D3DFORMAT ) MAKED3DFMT(
    GPUTEXTUREFORMAT_DXN, 
    GPUENDIAN_8IN16, 
    TRUE, 
    GPUSIGN_ALL_UNSIGNED, 
    GPUNUMFORMAT_FRACTION, 
    GPUSWIZZLE_OOGR );


//---------------------------------------------------------------------------------------------------------
// Helper functions.  
//---------------------------------------------------------------------------------------------------------
template<typename t_type> 
t_type Squared( t_type a ) { return a * a; }
template<typename t_type>
t_type Min( t_type a, t_type b ) { return a < b ? a : b; }
template<typename t_type>
t_type Max( t_type a, t_type b ) { return a > b ? a : b; }


//---------------------------------------------------------------------------------------------------------
// Modes for compression error calculation 
//---------------------------------------------------------------------------------------------------------
enum DIFF_MODES
{
    DIFF_MODE_COLOR_ALPHA = 0, 
    DIFF_MODE_UV, 

    DIFF_MODE_COUNT
};


//---------------------------------------------------------------------------------------------------------
// Supported tweakable UI parameters.     
//---------------------------------------------------------------------------------------------------------
enum UIParamTypes 
{
    UI_PARAM_COMPRESSION_METHOD, 
    UI_PARAM_TILING_METHOD, 
    UI_PARAM_TEST_TEXTURE, 
    UI_PARAM_NORMAL_FORMAT, 
    UI_PARAM_GPU_REPEAT, 
    UI_PARAM_FILTER_TYPE, 

    UI_PARAM_COUNT
};


//---------------------------------------------------------------------------------------------------------
// UIParam
//
// Base class for various types of menu selectors
//---------------------------------------------------------------------------------------------------------
class UIParam
{
public:
    static const DWORD  m_dwActiveParamColor = 0xffffff00;   
    static const DWORD  m_dwInactiveParamColor = 0xffffffff;   
    static const DWORD  m_dwActiveOptionColor = 0xff00ff00;   
    static const DWORD  m_dwInactiveOptionColor = 0xff808080;   
    static const DWORD  m_dwInvalidOptionColor = 0x80404040;   
    static const FLOAT  m_fActiveParamScale;
    static const FLOAT  m_fInactiveParamScale;
    static const FLOAT  m_fParamX;
    static const FLOAT  m_fOptionX;

    UIParam( const WCHAR* ParamName = NULL ) : m_ParamName( ParamName ), m_bValid( TRUE )
    {}

    virtual VOID RenderOptionUI( ATG::Font* pFont, FLOAT fParamX, FLOAT fParamY, BOOL bActive, 
        BOOL bValid ) = 0;

    VOID RenderUI( ATG::Font* pFont, FLOAT fParamX, FLOAT fParamY, BOOL bActive )
    {
        // Probably an uninitialized class instance if this triggers
        assert( m_ParamName );

        if( bActive ) 
        {
            pFont->SetScaleFactors( m_fActiveParamScale, m_fActiveParamScale );
            pFont->DrawText( fParamX, fParamY, m_dwActiveParamColor, m_ParamName );
        }
        else
        {
            pFont->SetScaleFactors( m_fInactiveParamScale, m_fInactiveParamScale );
            pFont->DrawText( fParamX, fParamY, m_dwInactiveParamColor, m_ParamName );
        }
        RenderOptionUI( pFont, fParamX, fParamY, bActive, m_bValid );
    }

    virtual VOID        DecreaseValue( FLOAT fScale = 1.0f ) = 0;
    virtual VOID        IncreaseValue( FLOAT fScale = 1.0f ) = 0;

    VOID                SetValid( BOOL bValid ) { m_bValid = bValid; }

protected:
    const WCHAR*        m_ParamName;
    BOOL                m_bValid;
};

const FLOAT  UIParam::m_fActiveParamScale = 1.1f;
const FLOAT  UIParam::m_fInactiveParamScale = 1.1f;
const FLOAT  UIParam::m_fOptionX = 300.0f;

class UIParamEnum : public UIParam
{
public:
    UIParamEnum( const WCHAR* ParamName, const WCHAR** OptionNames, UINT iCount, UINT iValue = 0 )
        : UIParam( ParamName )
        , m_OptionNames( OptionNames )
        , m_iCount( iCount )
        , m_iValue( iValue )
    {}

    VOID RenderOptionUI( ATG::Font* pFont, FLOAT fParamX, FLOAT fParamY, BOOL bActive, BOOL bValid )
    {
        UINT iValue = GetValue( );
        const WCHAR *OptionText = ( iValue >= 0 && iValue < m_iCount ) ? m_OptionNames[iValue] : L"n/a";
        DWORD dwOptionTextColor = bValid 
            ? ( bActive ? m_dwActiveOptionColor : m_dwInactiveOptionColor )
            : m_dwInvalidOptionColor;
        WCHAR SelectText[256];
        if( bActive ) 
        {
            swprintf_s( SelectText, L"< %s >", OptionText );
            OptionText = SelectText;
        }
        pFont->DrawText( fParamX + m_fOptionX, fParamY, dwOptionTextColor, OptionText );
    }

    UINT                GetValue( ) { return m_iValue; };
    VOID                SetValue( UINT iValue ) { m_iValue = iValue; };
    VOID                DecreaseValue( FLOAT fScale = 1.0f ) { m_iValue += m_iCount - 1; m_iValue %= m_iCount; }
    VOID                IncreaseValue( FLOAT fScale = 1.0f ) { m_iValue += 1;            m_iValue %= m_iCount; }

protected:
    const WCHAR**       m_OptionNames;
    UINT                m_iCount;
    UINT                m_iValue;
};

static const WCHAR* g_BoolOptionNames[2] = { L"FALSE", L"TRUE" };
class UIParamBool : public UIParamEnum
{
public:
    UIParamBool( const WCHAR* ParamName, BOOL bValue = FALSE )
        : UIParamEnum( ParamName, g_BoolOptionNames, 2, ( UINT ) bValue )
    {}

    BOOL                GetValue( ) { return UIParamEnum::GetValue( ) == 0 ? FALSE : TRUE; };
};

//-----------------------------------------------------------------------------
// Name: class Sample
// Desc: Main class to run this application. Most functionality is inherited
//       from the ATG::Application base class.
//-----------------------------------------------------------------------------
class Sample : public ATG::Application
{
    // ATG helper items
    ATG::PackedResource             m_Resource;
    ATG::Font                       m_Font;                 // Font for drawing text
    ATG::Timer                      m_FrameTimer;           // Timer for frame
    ATG::Timer                      m_CompressionTimer;     // Timer for compression
    ATG::Timer                      m_TilingTimer;          // Timer for tiling
    ATG::Help                       m_Help;                 // Display help

    // Sample options
    BOOL                            m_bDrawHelp;
    BOOL                            m_bBigMenu;
    BOOL                            m_bFlashingDiffs;
    FLOAT                           m_fFlashingTimer;

    // Camera options
    FLOAT                           m_fZoom;
    FLOAT                           m_fOffsetX;
    FLOAT                           m_fOffsetY;

    BOOL                            m_bTraceCompression;
    UINT                            m_iGPURepeatCount;
    FLOAT                           m_fCompressionTimeInMS;
    FLOAT                           m_fTilingTimeInMS;
    FLOAT                           m_fSmoothedCompressionTimeInMS;
    FLOAT                           m_fSmoothedTilingTimeInMS;

    UINT                            m_iCompressedType;
    UINT                            m_iDiffMode;

    // UI elements and parameters
    UINT                            m_iActiveUIParameter;   // Which UI item is affected by l/r input
    UINT                            m_iVisibleUIStart;
    const static UINT               m_iVisibleUICount = 4;

    UIParamEnum                     m_CompressionMethodParam;
    UIParamEnum                     m_TilingMethodParam;
    UIParamEnum                     m_TestTextureParam;
    UIParamEnum                     m_NormalFormatParam;
    UIParamEnum                     m_GpuRepeatParam;
    UIParamEnum                     m_FilterTypeParam;

    // The Params as the Menu sees them
    UIParam*                        m_UIParamArray[UI_PARAM_COUNT];

    // Resources for test geometry
    IDirect3DVertexDeclaration9*    m_pVertexDecl;
    IDirect3DVertexBuffer9*         m_pVB;
    IDirect3DIndexBuffer9*          m_pIB;
    UINT                            m_iIndexCount;

    // Additional texture resources
    IDirect3DTexture9*              m_pRawTexture;
    IDirect3DTexture9*              m_pCompressedTexture;
    IDirect3DTexture9*              m_pSquaredDiffTexture;
    IDirect3DSurface9*              m_pSquaredDiffRenderTarget;

    // Shaders 
    IDirect3DVertexShader9*         m_pVertexShader;
    IDirect3DPixelShader9*          m_pCopyTextureShader;
    IDirect3DPixelShader9*          m_pSquaredDiffShader;
    IDirect3DPixelShader9*          m_pDownScale2x2Shader;

    // Performance data
    D3DPerfCounters*                m_pPerfCounterStart[3];
    D3DPerfCounters*                m_pPerfCounterEnd[3];
    DWORD                           m_dwFrameCount;
    const static DWORD              m_dwGpuCyclesPerMs = GPU_CLOCK_SPEED / 1000;   // 500 MHz
    XGIDEALSHADERCOST               m_ShaderCost;

    // The custom compressor classes
    VMXCompressor                   m_VMXCompressor;
    GPUCompressor                   m_GPUCompressor;

    // Small helper functions
    VOID GenerateGeometryQuad( D3DVertexBuffer** pVB, D3DIndexBuffer** pIB, UINT* numIndices );
    VOID LoadTestTexture( UINT iTestTexture );
    BOOL NeedTempBufferForTiling( UINT iCompressionMethod, UINT iTilingMethod, BOOL bRepeatGPU );

    // Performance analysis
    VOID PerfCounterInit( );
    VOID CompressionTimingDebugRender( );

    // Standard compression function
    VOID CompressTextureXGraphics( const TextureDescAndBaseAddress* pSrcDescAndBaseAddress, 
        const TextureDescAndBaseAddress* pDstDescAndBaseAddress );

    // The root compression function
    VOID CompressTexture( const TextureDescAndBaseAddress* pSrcDescAndBaseAddress, 
        const TextureDescAndBaseAddress* pDstDescAndBaseAddress );

    // Error analysis
    VOID CalculateRMSError( TextureDescAndBaseAddress* pSrcDescAndBaseAddress, 
        TextureDescAndBaseAddress* pDstDescAndBaseAddress );

    // Draw/Resolve helpers
    VOID DrawGeometryToRenderTarget( IDirect3DTexture9* pSrcTexture,
        UINT iFilterType, 
        IDirect3DPixelShader9* pPixelShader, 
        IDirect3DVertexBuffer9* pVB, 
        IDirect3DIndexBuffer9* pIB, 
        UINT iIBSize );
    VOID RenderUI( );

public:
    Sample( );

    HRESULT Initialize( );
    HRESULT Update( );
    HRESULT Render( );
};


//-----------------------------------------------------------------------------
// Name: Sample::Sample( )
// Desc: Init alizes the UI elements.
//-----------------------------------------------------------------------------
Sample::Sample( )
: m_CompressionMethodParam( L"Compression method", g_strCompressionMethodNames, _countof( g_strCompressionMethodNames ) )
, m_TilingMethodParam( L"Tiling method", g_strTilingMethodNames, _countof( g_strTilingMethodNames ) )
, m_TestTextureParam( L"Texture", g_strTestTextureNames, _countof( g_strTestTextureNames ) )
, m_NormalFormatParam( L"Normal map format", g_strNormalFormatNames, _countof( g_strNormalFormatNames ) )
, m_GpuRepeatParam( L"GPU repeat", g_strGpuRepeatNames, _countof( g_strGpuRepeatNames ) )
, m_FilterTypeParam( L"Filtering", g_strFilterTypeNames, _countof( g_strFilterTypeNames ) )
{
    m_UIParamArray[UI_PARAM_COMPRESSION_METHOD]             = &m_CompressionMethodParam;
    m_UIParamArray[UI_PARAM_TILING_METHOD]                  = &m_TilingMethodParam;
    m_UIParamArray[UI_PARAM_TEST_TEXTURE]                   = &m_TestTextureParam;
    m_UIParamArray[UI_PARAM_NORMAL_FORMAT]                  = &m_NormalFormatParam;
    m_UIParamArray[UI_PARAM_GPU_REPEAT]                     = &m_GpuRepeatParam;
    m_UIParamArray[UI_PARAM_FILTER_TYPE]                    = &m_FilterTypeParam;
}


//---------------------------------------------------------------------------------------------------------
// Name: Sample::GenerateGeometryQuad( )
// Desc: Creates vertex and index buffer for a single fullscreen quad 
//---------------------------------------------------------------------------------------------------------
VOID Sample::GenerateGeometryQuad( D3DVertexBuffer** pVB,
                                 D3DIndexBuffer** pIB, UINT* numIndices )
{
    // Create a vertex buffer and copy the mesh vertex data into it
    m_pd3dDevice->CreateVertexBuffer( sizeof( TestGeometryVertex ) * 4, 0, 0, D3DPOOL_DEFAULT, 
        pVB, NULL );
    TestGeometryVertex* pVBData = NULL;
    ( *pVB )->Lock( 0, 0, ( VOID** )&pVBData, 0 );

    pVBData[0].Position.x = -1.0f;
    pVBData[0].Position.y = -1.0f;
    pVBData[0].Position.z =  0.0f;
    pVBData[0].TexCoord.x =  0.0f;
    pVBData[0].TexCoord.y =  1.0f;

    pVBData[1].Position.x =  1.0f;
    pVBData[1].Position.y = -1.0f;
    pVBData[1].Position.z =  0.0f;
    pVBData[1].TexCoord.x =  1.0f;
    pVBData[1].TexCoord.y =  1.0f;

    pVBData[2].Position.x = -1.0f;
    pVBData[2].Position.y =  1.0f;
    pVBData[2].Position.z =  0.0f;
    pVBData[2].TexCoord.x =  0.0f;
    pVBData[2].TexCoord.y =  0.0f;

    pVBData[3].Position.x =  1.0f;
    pVBData[3].Position.y =  1.0f;
    pVBData[3].Position.z =  0.0f;
    pVBData[3].TexCoord.x =  1.0f;
    pVBData[3].TexCoord.y =  0.0f;

    ( *pVB )->Unlock( );

    *numIndices = 4;

    // Create an index buffer and copy in the mesh index data.
    m_pd3dDevice->CreateIndexBuffer( *numIndices * sizeof( WORD ),
                                          0, D3DFMT_INDEX16, D3DPOOL_DEFAULT,
                                          pIB, NULL );
    WORD* pIBData = NULL;
    ( *pIB )->Lock( 0, 0, ( VOID** )&pIBData, 0 );
    *pIBData++ = 0;
    *pIBData++ = 2;
    *pIBData++ = 3;
    *pIBData++ = 1;
    ( *pIB )->Unlock( );
}


//---------------------------------------------------------------------------------------------------------
// Name: Sample:PerfCounterInit( )
// Desc: Set up the perf counters we plan to capture
//---------------------------------------------------------------------------------------------------------
VOID Sample::PerfCounterInit( )
{
#ifndef _RELEASED3D
    // Set up GPU performance counter structures.
    for( DWORD i = 0; i < 3; ++i )
    {
        m_pd3dDevice->CreatePerfCounters( &m_pPerfCounterStart[i], 1 );
        m_pd3dDevice->CreatePerfCounters( &m_pPerfCounterEnd[i], 1 );
    }
    m_pd3dDevice->EnablePerfCounters( TRUE );

    // Enable the performance counters we care about.
    D3DPERFCOUNTER_EVENTS PerfEvents;
    ZeroMemory( &PerfEvents, sizeof( D3DPERFCOUNTER_EVENTS ) );
    // CP clock cycles.
    PerfEvents.CP[0] = GPUPE_CP_COUNT;
    // NRT busy cycles.
    PerfEvents.RBBM[0] = GPUPE_RBBM_NRT_BUSY;
    m_pd3dDevice->SetPerfCounterEvents( &PerfEvents, 0 );

    m_dwFrameCount = 0;
#endif
}


//---------------------------------------------------------------------------------------------------------
// Name: Sample::LoadTestTexture( )
// Desc: Load a new uncompressed texture, determine the correct compressed type, and create the
// placeholder destination texture.
//
// Also create some accompanying objects, such as the memexport stream constant for writing to the 
// destination, and the helper texture & render target for RMS error calculation.
//---------------------------------------------------------------------------------------------------------
VOID Sample::LoadTestTexture( UINT iTestTexture )
{
    UINT iNormalFormat = m_NormalFormatParam.GetValue( );

    const WCHAR* strTextureNameW = g_strTestTextureNames[iTestTexture];
    CHAR strTextureName[MAX_PATH];
    WideCharToMultiByte( CP_ACP, 0, strTextureNameW, wcslen( strTextureNameW ) + 1, 
        strTextureName, MAX_PATH, NULL, NULL );

    m_pRawTexture = m_Resource.GetTexture( strTextureName );

    DWORD dwRawBaseAddress = m_pRawTexture->Format.BaseAddress << GPU_TEXTURE_ADDRESS_SHIFT;
    DWORD dwRawMipAddress = m_pRawTexture->Format.MipAddress << GPU_TEXTURE_ADDRESS_SHIFT;

    XGTEXTURE_DESC RawDesc;
    XGGetTextureDesc( m_pRawTexture, 0, &RawDesc );
    assert( RawDesc.Width % 4 == 0 && RawDesc.Height % 4 == 0 );

    D3DFORMAT d3dRawNonTiledNonsRGBFormat = GetNonAs16NonsRGBFormat( RawDesc.Format );
    switch( d3dRawNonTiledNonsRGBFormat )
    {
    case D3DFMT_LIN_X8B8G8R8:
    case D3DFMT_LIN_X8R8G8B8:
    default:
        m_iCompressedType = COMPRESSED_TYPE_DXT1;
        break;
    case D3DFMT_LIN_A8B8G8R8:
    case D3DFMT_LIN_A8R8G8B8:
        m_iCompressedType = COMPRESSED_TYPE_DXT5;
        break;
    case D3DFMT_LIN_G8R8:
        switch( iNormalFormat )
        {
        case NORMAL_FORMAT_DXN:
        default:
            m_iCompressedType = COMPRESSED_TYPE_DXN;
            break;
        case NORMAL_FORMAT_CTX1:
            m_iCompressedType = COMPRESSED_TYPE_CTX1;
            break;
        }
        break;
    }
    switch( m_iCompressedType )
    {
    case COMPRESSED_TYPE_DXT1:
    case COMPRESSED_TYPE_DXT5:
    default:
        m_iDiffMode = DIFF_MODE_COLOR_ALPHA;
        break;

    case COMPRESSED_TYPE_CTX1:
    case COMPRESSED_TYPE_DXN:
        m_iDiffMode = DIFF_MODE_UV;
        break;
    }

    // Change source texture to cacheable memory
    XPhysicalProtect( ( VOID* ) dwRawBaseAddress, 
        RawDesc.WidthInBlocks * RawDesc.HeightInBlocks * RawDesc.BytesPerBlock, 
        PAGE_READONLY );
    XGSetTextureHeader( RawDesc.Width, 
        RawDesc.Height, 
        1, 
        D3DUSAGE_CPU_CACHED_MEMORY, 
        RawDesc.Format, 
        0, 
        dwRawBaseAddress, 
        dwRawMipAddress, 
        RawDesc.RowPitch, 
        m_pRawTexture, 
        NULL, 
        NULL );

    // Create matching dest texture header
    if( m_pCompressedTexture )
    {
        m_pCompressedTexture->Release( );
    }

    // Decide what compressed format best matches the raw format
    D3DFORMAT d3dCompressedFormat;
    BOOL b128Bit;
    switch( m_iCompressedType )
    {
    case COMPRESSED_TYPE_DXT1:
    default:
        d3dCompressedFormat = ATG::GetAs16SRGBFormat( D3DFMT_DXT1 );
        b128Bit = FALSE;
        break;
    case COMPRESSED_TYPE_DXT5:
        d3dCompressedFormat = ATG::GetAs16SRGBFormat( D3DFMT_DXT5 );
        b128Bit = TRUE;
        break;
    case COMPRESSED_TYPE_CTX1:
        d3dCompressedFormat = D3DFMT_CTX1_GR;
        b128Bit = FALSE;
        break;
    case COMPRESSED_TYPE_DXN:
        d3dCompressedFormat = D3DFMT_DXN_GR;
        b128Bit = TRUE;
        break;
    }

    m_pd3dDevice->CreateTexture( RawDesc.Width, 
        RawDesc.Height, 
        1, 
        0, 
        d3dCompressedFormat, 
        0, 
        &m_pCompressedTexture, 
        NULL );

    // Create helper texture and render target for RMS error calculation
    // The full float D3DFMT_G32R32F format will only fit up to 1024x1024 in EDRAM
    // Allocate the helper texture header to not use packed mips ( because we need to resolve
    // into each level individually )
    if( m_pSquaredDiffTexture )
    {
        DWORD dwSquaredDiffBaseAddress = 
            m_pSquaredDiffTexture->Format.BaseAddress << GPU_TEXTURE_ADDRESS_SHIFT;
        XPhysicalFree( ( VOID* ) dwSquaredDiffBaseAddress );
        delete m_pSquaredDiffTexture;
    }

    m_pSquaredDiffTexture = new IDirect3DTexture9;

    DWORD dwSquaredDiffTextureSize = XGSetTextureHeaderEx( RawDesc.Width,
        RawDesc.Height,
        0,
        0,
        D3DFMT_G32R32F,
        0,
        XGHEADEREX_NONPACKED,
        0,
        XGHEADER_CONTIGUOUS_MIP_OFFSET,
        0,
        m_pSquaredDiffTexture,
        NULL,
        NULL );

    VOID* pSquaredDiffBuffer = XPhysicalAlloc( dwSquaredDiffTextureSize, MAXULONG_PTR, 0,
                                    PAGE_READONLY | PAGE_NOCACHE );

    if( pSquaredDiffBuffer == NULL )
    {
        ATG::FatalError( "Couldn't create SquaredDiff texture\n" );
    }

    XGOffsetResourceAddress( m_pSquaredDiffTexture, pSquaredDiffBuffer );

    if( m_pSquaredDiffRenderTarget )
    {
        m_pSquaredDiffRenderTarget->Release( );
    }

    m_pd3dDevice->CreateRenderTarget( RawDesc.Width, 
        RawDesc.Height, 
        D3DFMT_G32R32F, 
        D3DMULTISAMPLE_NONE,
        0, 
        FALSE, 
        &m_pSquaredDiffRenderTarget, 
        NULL );
}


//-----------------------------------------------------------------------------
// Name: Sample::Initialize( )
// Desc: Initialize app-dependent objects.
//-----------------------------------------------------------------------------
HRESULT Sample::Initialize( )
{
    // Create the font
    if( FAILED( m_Font.Create( "game:\\Media\\Fonts\\Arial_16.xpr" ) ) )
    {
        ATG::FatalError( "Couldn't create Font Arial_16.xpr\n" );
    }

    // Expanding Font area to get additional screen real estate...
    m_Font.SetWindow( 64, 8, 1280 - 64, 720 - 8 );

    // Create the help
    if( FAILED( m_Help.Create( "game:\\Media\\Help\\Help.xpr" ) ) )
    {
        ATG::FatalError( "Couldn't create Help.xpr\n" );
    }

    m_bDrawHelp = FALSE;
    m_bBigMenu = FALSE;
    m_bFlashingDiffs = FALSE;
    m_fFlashingTimer = 0.0f;

    m_fZoom = 1.0f;
    m_fOffsetX = 0.0f;
    m_fOffsetY = 0.0f;

    m_iGPURepeatCount = 1;
    m_bTraceCompression = FALSE;
    m_fSmoothedCompressionTimeInMS = m_fCompressionTimeInMS = 0.0f;
    m_fSmoothedTilingTimeInMS = m_fTilingTimeInMS = 0.0f;

    m_iActiveUIParameter = 0;
    m_iVisibleUIStart = 0;

    // Create common vertex declaration used by all the geometry
    static const D3DVERTEXELEMENT9 decl[] =
    {
        { 0, 0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
        { 0, 12, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
        { 0, 20, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TANGENT, 0 },
        { 0, 32, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_BINORMAL, 0 },
        { 0, 44, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_NORMAL, 0 },
        D3DDECL_END( )
    };

    m_pd3dDevice->CreateVertexDeclaration( decl, &m_pVertexDecl );

    GenerateGeometryQuad( &m_pVB, &m_pIB, &m_iIndexCount );

    // Create the textures resource
    if( FAILED( m_Resource.Create( "game:\\Media\\Resource.xpr" ) ) )
    {
        ATG::FatalError( "Couldn't create Resource.xpr\n" );
    }
    m_pCompressedTexture = NULL;
    m_pSquaredDiffTexture = NULL;
    m_pSquaredDiffRenderTarget = NULL;

    // Create shaders
    if( FAILED( ATG::LoadVertexShader( "game:\\Media\\Shaders\\ScreenSpaceShader.xvu",
                                       &m_pVertexShader ) ) )
    {
        ATG::FatalError( "Couldn't create VertexShader\n" );
    }
    if( FAILED( ATG::LoadPixelShader( "game:\\Media\\Shaders\\CopyTexture.xpu",
                                       &m_pCopyTextureShader ) ) )
    {
        ATG::FatalError( "Couldn't create PixelShader\n" );
    }
    if( FAILED( ATG::LoadPixelShader( "game:\\Media\\Shaders\\SquaredDiff.xpu",
                                       &m_pSquaredDiffShader ) ) )
    {
        ATG::FatalError( "Couldn't create PixelShader\n" );
    }
    if( FAILED( ATG::LoadPixelShader( "game:\\Media\\Shaders\\DownScale2x2.xpu",
                                       &m_pDownScale2x2Shader ) ) )
    {
        ATG::FatalError( "Couldn't create PixelShader\n" );
    }

    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_ZWRITEENABLE, FALSE );

    PerfCounterInit( );

    m_GPUCompressor.Initialize( m_pd3dDevice );

    return S_OK;
}


//-----------------------------------------------------------------------------
// Name: Sample::Update( )
// Desc: Called once per frame, the call is the entry point for animating
//       the scene.
//-----------------------------------------------------------------------------
HRESULT Sample::Update( )
{
    // Get the current gamepad status
    ATG::GAMEPAD* pGamepad = ATG::Input::GetMergedInput( );

    // Toggle help
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_BACK )
        m_bDrawHelp = !m_bDrawHelp;

    if( !m_bDrawHelp )
    {
        FLOAT fElapsedTime = ( FLOAT ) m_FrameTimer.GetElapsedTime( );
        m_fFlashingTimer += fElapsedTime;
        FLOAT fIntPart;
        m_fFlashingTimer = modf( m_fFlashingTimer, &fIntPart );

        if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_B )
        {
            m_bBigMenu = !m_bBigMenu;
        }
        if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_X )
        {
            m_bFlashingDiffs = !m_bFlashingDiffs;
        }
        if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_Y )
        {
            m_bTraceCompression = TRUE;
        }

        // UI controls:
        {
            // Record state prior to applying controller input
            static BOOL g_bFirstUpdate = TRUE;

            UINT iOldCompressionMethod          = m_CompressionMethodParam.GetValue( );
            UINT iOldTilingMethod               = m_TilingMethodParam.GetValue( );
            UINT iOldTestTexture                = m_TestTextureParam.GetValue( );
            UINT iOldNormalFormat               = m_NormalFormatParam.GetValue( );
            UINT iOldGpuRepeat                  = m_GpuRepeatParam.GetValue( );

            static FLOAT fLastX1 = 0.0f, fLastY1 = 0.0f;
            FLOAT fDecrease = 0.0f;
            FLOAT fIncrease = 0.0f;

            // Process UI Input
            if( ( pGamepad->wPressedButtons & XINPUT_GAMEPAD_DPAD_UP ) 
                || ( pGamepad->fY1 > 0.1f && fLastY1 <= 0.1f ) )
            {
                m_iActiveUIParameter += UI_PARAM_COUNT - 1;
                m_iActiveUIParameter %= UI_PARAM_COUNT;
            }
            if( ( pGamepad->wPressedButtons & XINPUT_GAMEPAD_DPAD_DOWN ) 
                || ( pGamepad->fY1 < -0.1f && fLastY1 >= -0.1f ) )
            {
                m_iActiveUIParameter += 1;
                m_iActiveUIParameter %= UI_PARAM_COUNT;
            }

#pragma warning( push )
#pragma warning( disable:4127 6326 )   // conditional expression is constant
            // Keep the selected parameter in view, in the small menu
            if( m_iVisibleUICount < UI_PARAM_COUNT )
#pragma warning( pop )
            {
                if( ( m_iActiveUIParameter + 1 ) % UI_PARAM_COUNT == m_iVisibleUIStart )
                {
                    m_iVisibleUIStart += UI_PARAM_COUNT - 1;
                    m_iVisibleUIStart %= UI_PARAM_COUNT;
                }
                else if( ( m_iVisibleUIStart + m_iVisibleUICount ) % UI_PARAM_COUNT == m_iActiveUIParameter )
                {
                    m_iVisibleUIStart += 1;
                    m_iVisibleUIStart %= UI_PARAM_COUNT;
                }
            }

            // Make responsiveness roughly independent of frame rate
            FLOAT fCameraSpeed = 0.1f * m_fCompressionTimeInMS;
            fCameraSpeed = Max( fCameraSpeed, 0.2f );

            // Change zoom
            m_fZoom *= ( 1.0f + 0.01f * fCameraSpeed * ( pGamepad->bLeftTrigger / 255.0f ) );
            m_fZoom *= ( 1.0f - 0.01f * fCameraSpeed * ( pGamepad->bRightTrigger / 255.0f ) );
            m_fZoom = Min( m_fZoom, 1.0f );
            m_fZoom = Max( m_fZoom, 1.0f / 128.0f );   // single 4x4 block of 512x512 texture

            // Change offset
            m_fOffsetX += 0.1f * fCameraSpeed * m_fZoom * pGamepad->fX2;
            m_fOffsetX = Min( m_fOffsetX,  1.0f * ( 1.1f - m_fZoom ) );
            m_fOffsetX = Max( m_fOffsetX, -1.0f * ( 1.1f - m_fZoom ) );
            m_fOffsetY += 0.1f * fCameraSpeed * m_fZoom * pGamepad->fY2;
            m_fOffsetY = Min( m_fOffsetY,  1.0f * ( 1.1f - m_fZoom ) );
            m_fOffsetY = Max( m_fOffsetY, -1.0f * ( 1.1f - m_fZoom ) );

            // Reset camera
            if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_RIGHT_THUMB )
            {
                m_fZoom = 1.0f;
                m_fOffsetX = 0.0f;
                m_fOffsetY = 0.0f;
            }

            if( pGamepad->fX1 < -0.1f && fLastX1 >= -0.1f )
                fDecrease = 1.0f;
            if( pGamepad->fX1 > 0.1f && fLastX1 <= 0.1f )
                fIncrease = 1.0f;
            if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_DPAD_LEFT )
                fDecrease = 1.0f;
            if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_DPAD_RIGHT )
                fIncrease = 1.0f;

            // Correct for bias in thumbsticks, and limit to one move at a time
            fLastX1 = pGamepad->fX1;
            fLastY1 = pGamepad->fY1;

            if( fDecrease > 0.0f )
            {
                m_UIParamArray[m_iActiveUIParameter]->DecreaseValue( fDecrease );
            }
            if( fIncrease > 0.0f )
            {
                m_UIParamArray[m_iActiveUIParameter]->IncreaseValue( fIncrease );
            }

            UINT iNewCompressionMethod          = m_CompressionMethodParam.GetValue( );
            UINT iNewTilingMethod               = m_TilingMethodParam.GetValue( );
            UINT iNewTestTexture                = m_TestTextureParam.GetValue( );
            UINT iNewNormalFormat               = m_NormalFormatParam.GetValue( );
            UINT iNewGpuRepeat                  = m_GpuRepeatParam.GetValue( );

            // Has a change occurred which requires re-initialization of 
            // some assets?
            if( g_bFirstUpdate 
                || iNewCompressionMethod        != iOldCompressionMethod
                || iNewTilingMethod             != iOldTilingMethod
                || iNewTestTexture              != iOldTestTexture
                || iNewNormalFormat             != iOldNormalFormat
                || iNewGpuRepeat                != iOldGpuRepeat
                )
            {
                // Check for unsupported tiling method
                m_TilingMethodParam.SetValid( TRUE );
                if( iNewCompressionMethod == COMPRESSION_METHOD_GPU )
                {
                    if( iNewTilingMethod == TILING_METHOD_XGTILE )
                    {
                        m_TilingMethodParam.IncreaseValue();
                    }
                }

                // Allow release of shaders, modification of texture data
                m_pd3dDevice->BlockUntilIdle( );

                LoadTestTexture( iNewTestTexture );

                m_iGPURepeatCount = ( iNewGpuRepeat == GPU_REPEAT_10X )
                    ? 10
                    : 1;

                // Check for unsupported normal format
                m_NormalFormatParam.SetValid( TRUE );
                if( m_iCompressedType != COMPRESSED_TYPE_CTX1
                    && m_iCompressedType != COMPRESSED_TYPE_DXN )
                {
                    m_NormalFormatParam.SetValid( FALSE );
                }
            }

            g_bFirstUpdate = FALSE;
        }
    }

    return S_OK;
}


//---------------------------------------------------------------------------------------------------------
// Name: Sample::CompressionTimingDebugRender( )
// Desc: Print compression time to screen
//---------------------------------------------------------------------------------------------------------
VOID Sample::CompressionTimingDebugRender( )
{
    UINT iCompressionMethod         = m_CompressionMethodParam.GetValue( );
    UINT iTilingMethod              = m_TilingMethodParam.GetValue( );

    WCHAR strText[256];
    FLOAT fYPos = 40.0f;
    m_Font.SetScaleFactors( 0.9f, 0.9f );

    // Print RMS Error
    {
        UINT i1x1Level = m_pSquaredDiffTexture->GetLevelCount( ) - 1;
        D3DLOCKED_RECT LockedRect;
        m_pSquaredDiffTexture->LockRect( i1x1Level, &LockedRect, NULL, 0 );

        FLOAT fRMSError8BitColor = 255.0f * sqrtf( ( ( FLOAT* ) LockedRect.pBits )[0] );
        FLOAT fRMSError8BitAlpha = 255.0f * sqrtf( ( ( FLOAT* ) LockedRect.pBits )[1] );
        FLOAT fRMSError8BitNormal = 255.0f * sqrtf( ( ( FLOAT* ) LockedRect.pBits )[0] );

        m_pSquaredDiffTexture->UnlockRect( i1x1Level );

        switch( m_iDiffMode )
        {
        case DIFF_MODE_COLOR_ALPHA:
        default:
            m_Font.DrawText( 0.0f, fYPos, 0xFF8080FF, L"RMS Error:" );
            swprintf_s( strText, L"%3.3f (8-bit color)", fRMSError8BitColor );
            m_Font.DrawText( 100.0f, fYPos, 0xFF8080FF, strText );
            fYPos += 20;

            swprintf_s( strText, L"%3.3f (8-bit alpha)", fRMSError8BitAlpha );
            m_Font.DrawText( 100.0f, fYPos, 0xFF8080FF, strText );
            fYPos += 20;
            break;

        case DIFF_MODE_UV:
            m_Font.DrawText( 0.0f, fYPos, 0xFF8080FF, L"RMS Error:" );
            swprintf_s( strText, L"%3.3f (8-bit UV)", fRMSError8BitNormal );
            m_Font.DrawText( 100.0f, fYPos, 0xFF8080FF, strText );
            fYPos += 20;
        }
    }

    switch( iCompressionMethod )
    {
    case COMPRESSION_METHOD_GPU:
        {
#ifndef _RELEASED3D
            m_Font.DrawText( 0.0f, fYPos, 0xFF8080FF, L"GPU MS:" );
            swprintf_s( strText, L"%3.3f (x%d)", m_fSmoothedCompressionTimeInMS, m_iGPURepeatCount );
            m_Font.DrawText( 100.0f, fYPos, 0xFF8080FF, strText );
            fYPos += 20;
#else
            m_Font.DrawText( 0.0f, fYPos, 0xFF8080FF, L"GPU MS:" );
            m_Font.DrawText( 100.0f, fYPos, 0xFF8080FF, L"(unavailable in release)" );
            fYPos += 20;
#endif
        }
        break;

        // We allow the VMX code to be compiled for DEBUG or PROFILE and these affect the
        // execution speed a lot.
    case COMPRESSION_METHOD_VMX_FLOAT:
    case COMPRESSION_METHOD_VMX_BYTE:
        {
            m_Font.DrawText( 0.0f, fYPos, 0xFF8080FF, L"CPU MS:" );
#if defined _DEBUG
            swprintf_s( strText, L"%3.3f (compression - debug)", m_fSmoothedCompressionTimeInMS );
#elif defined PROFILE
            swprintf_s( strText, L"%3.3f (compression - instrumented)", m_fSmoothedCompressionTimeInMS );
#else
            swprintf_s( strText, L"%3.3f (compression - optimized)", m_fSmoothedCompressionTimeInMS );
#endif
            m_Font.DrawText( 100.0f, fYPos, 0xFF8080FF, strText );
            fYPos += 20;
        }
        break;

    case COMPRESSION_METHOD_XGCOMPRESS:
    default:
        {
            m_Font.DrawText( 0.0f, fYPos, 0xFF8080FF, L"CPU MS:" );
            swprintf_s( strText, L"%3.3f (compression)", m_fSmoothedCompressionTimeInMS );
            m_Font.DrawText( 100.0f, fYPos, 0xFF8080FF, strText );
            fYPos += 20;
        }
        break;
    }

    switch( iTilingMethod )
    {
    case TILING_METHOD_XGTILE:
    default:
        if( iCompressionMethod != COMPRESSION_METHOD_GPU )  // tiling done during compression
        {
            m_Font.DrawText( 0.0f, fYPos, 0xFF8080FF, L"CPU MS:" );
            swprintf_s( strText, L"%3.3f (tiling)", m_fSmoothedTilingTimeInMS );
            m_Font.DrawText( 100.0f, fYPos, 0xFF8080FF, strText );
            fYPos += 20;
        }
        break;

    case TILING_METHOD_GPU_RESOLVE:
    case TILING_METHOD_GPU_MEMEXPORT:
        if( iCompressionMethod != COMPRESSION_METHOD_GPU )  // tiling done during compression
        {
#ifndef _RELEASED3D
            m_Font.DrawText( 0.0f, fYPos, 0xFF8080FF, L"GPU MS:" );
            swprintf_s( strText, L"%3.3f (tiling - x%d)", m_fSmoothedTilingTimeInMS, m_iGPURepeatCount );
            m_Font.DrawText( 100.0f, fYPos, 0xFF8080FF, strText );
            fYPos += 20;
#else
            m_Font.DrawText( 0.0f, fYPos, 0xFF8080FF, L"GPU MS:" );
            m_Font.DrawText( 100.0f, fYPos, 0xFF8080FF, L"(unavailable in release)" );
            fYPos += 20;
#endif
        }
        break;
    }
}


//---------------------------------------------------------------------------------------------------------
// Name: Sample::RenderUI( )
// Desc: Render the screen display for our custom menu.
//---------------------------------------------------------------------------------------------------------
VOID Sample::RenderUI( )
{

    PIXBeginNamedEvent( 0, "Render UI" );

    if( m_bDrawHelp )
    {
        m_Help.Render( &m_Font, g_HelpCallouts, NUM_HELP_CALLOUTS );
    }
    else 
    {
        m_Font.Begin( );

        m_Font.SetScaleFactors( 1.2f, 1.2f );
        m_Font.DrawText( 0.0f, 0.0f, 0xffff00ff, L"Fast Block Compress" );

        m_Font.SetScaleFactors( 1.0f, 1.0f );
        m_Font.DrawText( 0.0f, 150.0f, 0xff00ffff, L"Uncompressed", ATGFONT_LEFT );

        WCHAR CompressedTypeText[256];
        swprintf_s( CompressedTypeText, L"%s Compressed", g_strCompressedTypeNames[m_iCompressedType] );
        m_Font.DrawText( 1152.0f, 150.0f, 0xff00ffff, CompressedTypeText, ATGFONT_RIGHT );

        CompressionTimingDebugRender( );

        FLOAT fParamX = 500.0f;
        FLOAT fParamY = 0.0f;
        FLOAT fParamYInc = 30.0f;

#pragma warning( push )
#pragma warning( disable:6326 )   // Potential comparison of a constant with another constant
        BOOL bBigMenu = m_bBigMenu || UI_PARAM_COUNT <= m_iVisibleUICount;
#pragma warning( pop )

        UINT iVisibleUIStart = bBigMenu ? 0 : m_iVisibleUIStart;
        UINT iVisibleUICount = bBigMenu ? UI_PARAM_COUNT : m_iVisibleUICount;

        if( !bBigMenu )
        {
            m_Font.SetScaleFactors( 1.2f, 1.2f );
            m_Font.DrawText( fParamX + 40.0f, fParamY, 0xffff00ff, GLYPH_UP_ARROW ); 
            m_Font.SetScaleFactors( 1.0f, 1.0f );
            m_Font.DrawText( fParamX + 100.0f, fParamY, 0xffffffff, L"( " GLYPH_B_BUTTON L" to expand )" ); 
            fParamY += fParamYInc;
        }

        m_Font.SetScaleFactors( 1.0f, 1.0f );
        for( UINT i = 0; i < iVisibleUICount; ++i )
        {
            UINT iParamIndex = ( iVisibleUIStart + i ) % UI_PARAM_COUNT;

            UIParam* Param = m_UIParamArray[iParamIndex];

            Param->RenderUI( &m_Font, fParamX, fParamY, ( iParamIndex == m_iActiveUIParameter ) );
            fParamY += fParamYInc;
        }

        if( !bBigMenu )
        {
            m_Font.SetScaleFactors( 1.2f, 1.2f );
            m_Font.DrawText( fParamX + 40.0f, fParamY, 0xffff00ff, GLYPH_DOWN_ARROW );
            fParamY += fParamYInc;
        }

        m_Font.End( );
    }
}


//---------------------------------------------------------------------------------------------------------
// Name: Sample::DrawGeometryToRenderTarget( )
// Desc: Draw texture to back buffer, using current camera settings relative to the viewport.
//---------------------------------------------------------------------------------------------------------
VOID Sample::DrawGeometryToRenderTarget( IDirect3DTexture9* pSrcTexture,
                                        UINT iFilterType, 
                                        IDirect3DPixelShader9* pPixelShader, 
                                        IDirect3DVertexBuffer9* pVB, 
                                        IDirect3DIndexBuffer9* pIB, 
                                        UINT iIBSize )
{
    // Make sure that the required resources exist
    assert( pSrcTexture );

    // Make sure that the required shaders and objects exist
    assert( pPixelShader );

    // Set all the right D3D state and resources
    m_pd3dDevice->SetIndices( pIB );
    m_pd3dDevice->SetStreamSource( 0, pVB, 0, sizeof( TestGeometryVertex ) );
    m_pd3dDevice->SetVertexDeclaration( m_pVertexDecl );
    m_pd3dDevice->SetVertexShader( m_pVertexShader );

    m_pd3dDevice->SetTexture( 0, pSrcTexture );
    switch( iFilterType )
    {
    case FILTER_TYPE_POINT:
    default:
        m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_POINT );
        m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_POINT );
        m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MIPFILTER, D3DTEXF_POINT );
        break;

    case FILTER_TYPE_BILINEAR:
        m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
        m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
        m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MIPFILTER, D3DTEXF_POINT );
        break;
    }
    m_pd3dDevice->SetPixelShader( pPixelShader );

    // Set up the float shader constants
    XMMATRIX matProj = XMMatrixOrthographicOffCenterLH( 
        -1.0f * m_fZoom + m_fOffsetX, 
         1.0f * m_fZoom + m_fOffsetX, 
        -1.0f * m_fZoom + m_fOffsetY, 
         1.0f * m_fZoom + m_fOffsetY, 
         0.0f, 
         1.0f );

    // No world transform yet
    XMMATRIX matWVP = matProj;
    
    XMMATRIX matWVPT = XMMatrixTranspose( matWVP );
    m_pd3dDevice->SetVertexShaderConstantF( 0, ( FLOAT* )&matWVPT, 4 );

    // Turn on alpha blending
    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_SRCBLEND, D3DBLEND_SRCALPHA );
    m_pd3dDevice->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA );

    // Draw the geometry
    m_pd3dDevice->DrawIndexedPrimitive( D3DPT_QUADLIST, 0, 0, 0, 0, iIBSize / 4 );
}


//---------------------------------------------------------------------------------------------------------
// Name: Sample::CalculateRMSError( )
// Desc: Calculate sqrt( sum_over_all_texels( ( src - dst )^2 ) ).
// This is a standard error metric, although not necessarily that representative of perceptual 
// error.
//---------------------------------------------------------------------------------------------------------
VOID Sample::CalculateRMSError( TextureDescAndBaseAddress* pSrcDescAndBaseAddress, 
                               TextureDescAndBaseAddress* pDstDescAndBaseAddress )
{
    // Most convenient and quick way to calculate --- find squared error for each texel 
    // in a shader, then generate mipmaps down to 1x1.  No need to Lock or synchronize
    // if we are okay with a 1 frame delay in UI update.

    PIXBeginNamedEvent( 0, "Calculate RMS Error" );

    // Set up the quad to draw
    m_pd3dDevice->SetIndices( m_pIB );
    m_pd3dDevice->SetStreamSource( 0, m_pVB, 0, sizeof( TestGeometryVertex ) );
    m_pd3dDevice->SetVertexDeclaration( m_pVertexDecl );
    m_pd3dDevice->SetVertexShader( m_pVertexShader );

    // Set up the matrix to map the quad to all of the viewport
    XMMATRIX matProj = XMMatrixOrthographicLH( 2.0f, 2.0f, 0.0f, 4.0f );
    XMMATRIX matWVP = matProj;
    XMMATRIX matWVPT = XMMatrixTranspose( matWVP );
    m_pd3dDevice->SetVertexShaderConstantF( 0, ( FLOAT* )&matWVPT, 4 );

    // Set the two textures to be compared, each point-sampled, each treated
    // without gamma-correction
    IDirect3DTexture9 DummySrcTexture;
    XGSetTextureHeader( pSrcDescAndBaseAddress->Desc.Width, 
        pSrcDescAndBaseAddress->Desc.Height, 
        1, 
        0, 
        GetNonAs16NonsRGBFormat( pSrcDescAndBaseAddress->Desc.Format ), 
        0, 
        pSrcDescAndBaseAddress->BaseAddress, 
        0, 
        0, 
        &DummySrcTexture, 
        NULL, 
        NULL );
    m_pd3dDevice->SetTexture( 0, &DummySrcTexture );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_POINT );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_POINT );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MIPFILTER, D3DTEXF_POINT );

    IDirect3DTexture9 DummyDstTexture;
    XGSetTextureHeader( pDstDescAndBaseAddress->Desc.Width, 
        pDstDescAndBaseAddress->Desc.Height, 
        1, 
        0, 
        GetNonAs16NonsRGBFormat( pDstDescAndBaseAddress->Desc.Format ), 
        0, 
        pDstDescAndBaseAddress->BaseAddress, 
        0, 
        0, 
        &DummyDstTexture, 
        NULL, 
        NULL );
    m_pd3dDevice->SetTexture( 1, &DummyDstTexture );
    m_pd3dDevice->SetSamplerState( 1, D3DSAMP_MINFILTER, D3DTEXF_POINT );
    m_pd3dDevice->SetSamplerState( 1, D3DSAMP_MAGFILTER, D3DTEXF_POINT );
    m_pd3dDevice->SetSamplerState( 1, D3DSAMP_MIPFILTER, D3DTEXF_POINT );

    // Turn off alpha-blend ( we don't restore this state )
    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );

    // Set the squared diff shader
    m_pd3dDevice->SetPixelShader( m_pSquaredDiffShader );

    // Set the diff mode
    m_pd3dDevice->SetPixelShaderConstantF( 0, ( FLOAT* ) &m_iDiffMode, 1 );

    // Set up the scratch render target
    D3DVIEWPORT9 OldViewport;
    m_pd3dDevice->GetViewport( &OldViewport );
    IDirect3DSurface9* pOldRenderTarget;
    m_pd3dDevice->GetRenderTarget( 0, &pOldRenderTarget );
    m_pd3dDevice->SetRenderTarget( 0, m_pSquaredDiffRenderTarget );

    // Draw the geometry
    m_pd3dDevice->DrawIndexedPrimitive( D3DPT_QUADLIST, 0, 0, 0, 0, m_iIndexCount / 4 );

    // Resolve to the squared diff texture
    m_pd3dDevice->Resolve( D3DRESOLVE_RENDERTARGET0, NULL, m_pSquaredDiffTexture,
                           NULL, 0, 0, NULL, 1.0f, 0L, NULL );

    // Mip down to 1x1 --- adapted from ATG::PostProcess::BuildMipMaps
    m_pd3dDevice->SetPixelShader( m_pDownScale2x2Shader );

    m_pd3dDevice->SetTexture( 0, m_pSquaredDiffTexture );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_POINT );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_POINT );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MIPFILTER, D3DTEXF_POINT );

    D3DVIEWPORT9 MipViewport;
    m_pd3dDevice->GetViewport( &MipViewport );
    UINT iNumMipLevels = m_pSquaredDiffTexture->GetLevelCount( );

    for( UINT iMipLevel = 1; iMipLevel < iNumMipLevels; iMipLevel++ )
    {
        MipViewport.Width >>= 1;
        MipViewport.Height >>= 1;
        m_pd3dDevice->SetViewport( &MipViewport );
        m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MINMIPLEVEL, iMipLevel - 1 );

        // Draw the geometry
        m_pd3dDevice->DrawIndexedPrimitive( D3DPT_QUADLIST, 0, 0, 0, 0, m_iIndexCount / 4 );

        m_pd3dDevice->Resolve( D3DRESOLVE_RENDERTARGET0, NULL, m_pSquaredDiffTexture, NULL,
            iMipLevel, 0, NULL, 1.0f, 0L, NULL );
    }

    // If we want to see the value in the Visual Studio debugger turn this to TRUE
    static BOOL bViewResults = FALSE;
    if( bViewResults )
    {
        D3DLOCKED_RECT LockedRect;
        m_pSquaredDiffTexture->LockRect( iNumMipLevels - 1, &LockedRect, NULL, 0 );

        switch( m_iDiffMode )
        {
        case COMPRESSED_TYPE_DXT1:
        case COMPRESSED_TYPE_DXT5:
        default:
            {
                static FLOAT fRMSError8BitColor = sqrtf( ( ( FLOAT* ) LockedRect.pBits )[0] );
                static FLOAT fRMSError8BitAlpha = sqrtf( ( ( FLOAT* ) LockedRect.pBits )[1] );
            }
            break;

        case COMPRESSED_TYPE_CTX1:
        case COMPRESSED_TYPE_DXN:
            {
                static FLOAT fRMSError8BitNormal = sqrtf( ( ( FLOAT* ) LockedRect.pBits )[0] );
            }
            break;
        }

        m_pSquaredDiffTexture->UnlockRect( iNumMipLevels - 1 );
    }

    // Restore state
    m_pd3dDevice->SetTexture( 0, NULL );    // Must clear these, since the texture 
    m_pd3dDevice->SetTexture( 1, NULL );    // headers are temporary var ables
    m_pd3dDevice->SetRenderTarget( 0, pOldRenderTarget );
    m_pd3dDevice->SetViewport( &OldViewport );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MINMIPLEVEL, 13 );

    PIXEndNamedEvent( );
}


//---------------------------------------------------------------------------------------------------------
// Name: Sample::CompressTextureXGraphics( )
// Desc: The standard block compressor provided in the XDK libraries.
//---------------------------------------------------------------------------------------------------------
VOID Sample::CompressTextureXGraphics( const TextureDescAndBaseAddress* pSrcDescAndBaseAddress, 
                                      const TextureDescAndBaseAddress* pDstDescAndBaseAddress )
{
    D3DFORMAT fmtSrcLinear = GetNonAs16NonsRGBFormat( 
        ( D3DFORMAT ) MAKELINFMT( pSrcDescAndBaseAddress->Desc.Format ) );
    D3DFORMAT fmtDstLinear = GetNonAs16NonsRGBFormat( 
        ( D3DFORMAT ) MAKELINFMT( pDstDescAndBaseAddress->Desc.Format ) );

    assert( !XGIsTiledFormat( pSrcDescAndBaseAddress->Desc.Format ) );

    XGCompressSurface( ( VOID* ) pDstDescAndBaseAddress->BaseAddress, 
        pDstDescAndBaseAddress->Desc.RowPitch, 
        pDstDescAndBaseAddress->Desc.Width, 
        pDstDescAndBaseAddress->Desc.Height, 
        fmtDstLinear, 
        NULL, 
        ( VOID* ) pSrcDescAndBaseAddress->BaseAddress, 
        pSrcDescAndBaseAddress->Desc.RowPitch, 
        fmtSrcLinear, 
        NULL, 
        XGCOMPRESS_NO_DITHERING,    // dithering gives better visuals, but higher RMS error, slower perf
        0.0f );
}


//---------------------------------------------------------------------------------------------------------
// Name: Sample::NeedTempBufferForTiling( )
// Desc: Returns TRUE if an intermediate buffer is necessary for the selected combination of compression
// and tiling options.  Returns FALSE if tiling can be done in place, or at the same time as compression.
//---------------------------------------------------------------------------------------------------------
BOOL Sample::NeedTempBufferForTiling( UINT iCompressionMethod, UINT iTilingMethod, BOOL bRepeatGPU )
{
    switch( iCompressionMethod )
    {
    case COMPRESSION_METHOD_XGCOMPRESS:
    case COMPRESSION_METHOD_VMX_FLOAT:
    case COMPRESSION_METHOD_VMX_BYTE:
        switch( iTilingMethod )
        {
        case TILING_METHOD_XGTILE:
        case TILING_METHOD_GPU_MEMEXPORT:
            return TRUE;

        case TILING_METHOD_GPU_RESOLVE:
            return bRepeatGPU;

        default:
            assert( FALSE );    // unreachable
            return FALSE;
        }
        break;

    case COMPRESSION_METHOD_GPU:
        switch( iTilingMethod )
        {
        case TILING_METHOD_XGTILE:
            return TRUE;

        case TILING_METHOD_GPU_RESOLVE:
        case TILING_METHOD_GPU_MEMEXPORT:
            return FALSE;

        default:
            assert( FALSE );    // unreachable
            return FALSE;
        }
        break;
        break;

    default:
        assert( FALSE );    // unreachable
        return FALSE;
    }
}


//---------------------------------------------------------------------------------------------------------
// Name: Sample::CompressTexture( )
// Desc: Root compression/tiling routine for all methods in the sample.
//---------------------------------------------------------------------------------------------------------
VOID Sample::CompressTexture( const TextureDescAndBaseAddress* pSrcDescAndBaseAddress, 
                             const TextureDescAndBaseAddress* pDstDescAndBaseAddress )
{
    UINT iCompressionMethod         = m_CompressionMethodParam.GetValue( );
    UINT iTilingMethod              = m_TilingMethodParam.GetValue( );

    // We will need to temporarily modify some fields of these const inputs
    TextureDescAndBaseAddress SrcDescAndBaseAddressCopy = *pSrcDescAndBaseAddress;
    TextureDescAndBaseAddress DstDescAndBaseAddressCopy = *pDstDescAndBaseAddress;
    TextureDescAndBaseAddress* pSrcDescAndBaseAddressCopy = &SrcDescAndBaseAddressCopy;
    TextureDescAndBaseAddress* pDstDescAndBaseAddressCopy = &DstDescAndBaseAddressCopy;

    // Cannot tile in place efficiently.  In this case, compress to a temp write-combined buffer.
    VOID* pTempBuffer = NULL;
    assert( XGIsTiledFormat( pDstDescAndBaseAddressCopy->Desc.Format ) );
    BOOL bNeedTempBuffer = NeedTempBufferForTiling( iCompressionMethod, iTilingMethod, 
        ( m_iGPURepeatCount > 1 ) );
    if( bNeedTempBuffer )
    {
        DWORD dwDstSizeInBytes = pDstDescAndBaseAddressCopy->Desc.RowPitch 
            * pDstDescAndBaseAddressCopy->Desc.HeightInBlocks;
        pTempBuffer = XPhysicalAlloc( dwDstSizeInBytes, MAXULONG_PTR, 0, 
            PAGE_READWRITE | PAGE_WRITECOMBINE );
        pDstDescAndBaseAddressCopy->BaseAddress = ( UINT ) pTempBuffer;
    }

    // Perform the compression
    switch( iCompressionMethod )
    {
    case COMPRESSION_METHOD_XGCOMPRESS:
    default:
        {
            if ( m_bTraceCompression )
            {
                XTraceStartRecording( "d:\\Trace_CompressTextureXGraphics.pix2" );
            }

            m_CompressionTimer.GetElapsedTime( );

            CompressTextureXGraphics( pSrcDescAndBaseAddressCopy, pDstDescAndBaseAddressCopy );

            __eieio( ); // memory barrier for write-combined writes to complete

            m_fCompressionTimeInMS = ( FLOAT ) m_CompressionTimer.GetElapsedTime( ) * 1000.0f;

            if ( m_bTraceCompression )
            {
                XTraceStopRecording( );
            }
        }
        break;

    case COMPRESSION_METHOD_VMX_FLOAT:
        {
            if ( m_bTraceCompression )
            {
                XTraceStartRecording( "d:\\Trace_CompressTextureVMX_FLOAT.pix2" );
            }

            m_CompressionTimer.GetElapsedTime( );

            m_VMXCompressor.CompressTexture( pSrcDescAndBaseAddressCopy, 
                pDstDescAndBaseAddressCopy, 
                m_iCompressedType, 
                SIMD_FORMAT_4_FLOAT );

            __eieio( ); // memory barrier for write-combined writes to complete

            m_fCompressionTimeInMS = ( FLOAT ) m_CompressionTimer.GetElapsedTime( ) * 1000.0f;

            if ( m_bTraceCompression )
            {
                XTraceStopRecording( );
            }
        }
        break;

    case COMPRESSION_METHOD_VMX_BYTE:
        {
            if ( m_bTraceCompression )
            {
                XTraceStartRecording( "d:\\Trace_CompressTextureVMX_BYTE.pix2" );
            }

            m_CompressionTimer.GetElapsedTime( );

            m_VMXCompressor.CompressTexture( pSrcDescAndBaseAddressCopy, 
                pDstDescAndBaseAddressCopy, 
                m_iCompressedType, 
                SIMD_FORMAT_16_BYTE );

            __eieio( ); // memory barrier for write-combined writes to complete

            m_fCompressionTimeInMS = ( FLOAT ) m_CompressionTimer.GetElapsedTime( ) * 1000.0f;

            if ( m_bTraceCompression )
            {
                XTraceStopRecording( );
            }
        }
        break;

    case COMPRESSION_METHOD_GPU:
        {
            // Note the presence of QueryPerfCounters calls affect the timings in PIX GPU captures
            // Comment these out for more accurate captures
#ifndef _RELEASED3D
            m_pd3dDevice->QueryPerfCounters( m_pPerfCounterStart[ m_dwFrameCount % 3 ], 
                D3DPERFQUERY_WAITGPUIDLE );
#endif

            PIXBeginNamedEvent( 0, "Compress texture GPU" );

            m_GPUCompressor.CompressAndTileTexture( m_pd3dDevice, 
                pSrcDescAndBaseAddressCopy, 
                pDstDescAndBaseAddressCopy, 
                m_iCompressedType, 
                iTilingMethod, 
                m_iGPURepeatCount );

            PIXEndNamedEvent( );

#ifndef _RELEASED3D
            m_pd3dDevice->QueryPerfCounters( m_pPerfCounterEnd[ m_dwFrameCount % 3 ], 
                D3DPERFQUERY_WAITGPUIDLE );

            D3DPERFCOUNTER_VALUES StartValues;
            m_pPerfCounterStart[ m_dwFrameCount % 3 ]->GetValues( &StartValues, 0, NULL );
            D3DPERFCOUNTER_VALUES EndValues;
            m_pPerfCounterEnd[ m_dwFrameCount % 3 ]->GetValues( &EndValues, 0, NULL );

            // Subtract start values from end values.
            UINT64* pStartValues = ( UINT64* )&StartValues;
            UINT64* pEndValues = ( UINT64* )&EndValues;
            const DWORD dwCount = sizeof( D3DPERFCOUNTER_VALUES ) / sizeof( UINT64 );
            for( DWORD i = 0; i < dwCount; ++i )
            {
                pEndValues[i] -= pStartValues[i];
            }

            m_fCompressionTimeInMS = ( FLOAT )EndValues.RBBM[0].QuadPart 
                / ( m_iGPURepeatCount * ( FLOAT )m_dwGpuCyclesPerMs );

            ++m_dwFrameCount;
#else
            m_fCompressionTimeInMS = 0.2f;  // this value is used to drive camera speed
#endif
        }
        break;
    }

    // Now the source is either the intermediate buffer, or the dest
    *pSrcDescAndBaseAddressCopy = *pDstDescAndBaseAddressCopy;
    pSrcDescAndBaseAddressCopy->Desc.Format = 
        ( D3DFORMAT ) MAKELINFMT( pSrcDescAndBaseAddressCopy->Desc.Format );

    // If we used an intermediate buffer, restore the destination address
    pDstDescAndBaseAddressCopy->BaseAddress = pDstDescAndBaseAddress->BaseAddress;

    // Perform the tiling, if it was not already a side-effect of the compression
    switch( iTilingMethod )
    {
    case TILING_METHOD_XGTILE:
        if( iCompressionMethod != COMPRESSION_METHOD_GPU )  // tiling done during compression
        {
            if ( m_bTraceCompression )
            {
                XTraceStartRecording( "d:\\Trace_TileTextureXGraphics.pix2" );
            }

            m_TilingTimer.GetElapsedTime( );

            // Pass in cacheable src pointer to temp untiled buffer, write-combined dst pointer 
            // to actual texture buffer
            const VOID* pUntiledSrc = GPU_CONVERT_CPU_TO_CPU_CACHED_READONLY_ADDRESS( 
                ( const VOID* ) pSrcDescAndBaseAddressCopy->BaseAddress );

            XGTileSurface( ( VOID* ) pDstDescAndBaseAddressCopy->BaseAddress, 
                pDstDescAndBaseAddressCopy->Desc.WidthInBlocks, 
                pDstDescAndBaseAddressCopy->Desc.HeightInBlocks, 
                NULL, 
                pUntiledSrc, 
                pDstDescAndBaseAddressCopy->Desc.RowPitch, 
                NULL, 
                pDstDescAndBaseAddressCopy->Desc.BytesPerBlock );

            __eieio( ); // memory barrier for write-combined writes to complete

            m_fTilingTimeInMS = ( FLOAT ) m_TilingTimer.GetElapsedTime( ) * 1000.0f;

            if ( m_bTraceCompression )
            {
                XTraceStopRecording( );
            }
        }
        break;

    case TILING_METHOD_GPU_RESOLVE:
    case TILING_METHOD_GPU_MEMEXPORT:
        if( iCompressionMethod != COMPRESSION_METHOD_GPU )  // tiling done during compression
        {
#ifndef _RELEASED3D
            m_pd3dDevice->QueryPerfCounters( m_pPerfCounterStart[ m_dwFrameCount % 3 ], 
                D3DPERFQUERY_WAITGPUIDLE );
#endif

            PIXBeginNamedEvent( 0, "Tile texture GPU" );

            m_GPUCompressor.CompressAndTileTexture( m_pd3dDevice, 
                pSrcDescAndBaseAddressCopy, 
                pDstDescAndBaseAddressCopy, 
                m_iCompressedType, 
                iTilingMethod, 
                m_iGPURepeatCount );

            PIXEndNamedEvent( );

#ifndef _RELEASED3D
            m_pd3dDevice->QueryPerfCounters( m_pPerfCounterEnd[ m_dwFrameCount % 3 ], 
                D3DPERFQUERY_WAITGPUIDLE );

            D3DPERFCOUNTER_VALUES StartValues;
            m_pPerfCounterStart[ m_dwFrameCount % 3 ]->GetValues( &StartValues, 0, NULL );
            D3DPERFCOUNTER_VALUES EndValues;
            m_pPerfCounterEnd[ m_dwFrameCount % 3 ]->GetValues( &EndValues, 0, NULL );

            // Subtract start values from end values.
            UINT64* pStartValues = ( UINT64* )&StartValues;
            UINT64* pEndValues = ( UINT64* )&EndValues;
            const DWORD dwCount = sizeof( D3DPERFCOUNTER_VALUES ) / sizeof( UINT64 );
            for( DWORD i = 0; i < dwCount; ++i )
            {
                pEndValues[i] -= pStartValues[i];
            }

            m_fTilingTimeInMS = ( FLOAT )EndValues.RBBM[0].QuadPart 
                / ( m_iGPURepeatCount * ( FLOAT )m_dwGpuCyclesPerMs );

            ++m_dwFrameCount;
#endif
        }
        break;
    }

    // We want smoothed times to converge quickly, and become stable soon afterwards
    // Higher factor means faster convergence but more oscillation
    FLOAT fLerpFactor = fabsf( m_fCompressionTimeInMS - m_fSmoothedCompressionTimeInMS ) / 10.0f;
    fLerpFactor = Max( fLerpFactor, 0.01f );
    fLerpFactor = Min( fLerpFactor, 0.9f );

    m_fSmoothedCompressionTimeInMS = ( 1.0f - fLerpFactor ) * m_fSmoothedCompressionTimeInMS 
        + fLerpFactor * m_fCompressionTimeInMS;
    m_fSmoothedTilingTimeInMS = ( 1.0f - fLerpFactor ) * m_fSmoothedTilingTimeInMS 
        + fLerpFactor * m_fTilingTimeInMS;

    if( pTempBuffer )
    {
        XPhysicalFree( pTempBuffer );
    }

    m_bTraceCompression = FALSE;
}


//-----------------------------------------------------------------------------
// Name: Render( )
// Desc: Called once per frame, to render the scene.
//-----------------------------------------------------------------------------
HRESULT Sample::Render( )
{
    // Recover all the relevant settings from the UI
    UINT iFilterType                = m_FilterTypeParam.GetValue( );

    IDirect3DTexture9* pRawTexture = m_pRawTexture;
    IDirect3DTexture9* pCompressedTexture = m_pCompressedTexture;
    IDirect3DPixelShader9 *pPixelShader = m_pCopyTextureShader;

    // Place an 'if' here to place conditions on whether to re-encode on this frame
    {
        // WARNING:  The block and invalidate below should not be used in shipping code!
        // They exist here because the sample recompresses textures which are in use
        // by the GPU.
        //
        // In the main usage scenario, the compressed texture is new and has never been
        // seen by the GPU.
        //
        // The safe way to ensure synchronization is to LockRect/UnlockRect the compressed 
        // texture.  But that has a non-triv al performance cost ( on the scale we're working 
        // at here ).  The cost is due to the cache flush ( which is performed even if the 
        // texture uses uncacheable memory ).
        //
        // All CPU writes to the compressed buffer go through write-combined addresses,
        // so no CPU cache flush is needed following the writes --- only a memory barrier 
        // ( __eieio ).
        m_pd3dDevice->BlockUntilIdle( );
        m_pd3dDevice->InvalidateResourceGpuCache( pCompressedTexture, 0 );

        // Make sure that the required resources exist
        assert( pRawTexture );
        assert( pCompressedTexture );

        TextureDescAndBaseAddress SrcDescAndBaseAddress;
        XGGetTextureDesc( pRawTexture, 0, &SrcDescAndBaseAddress.Desc );
        SrcDescAndBaseAddress.BaseAddress = pRawTexture->Format.BaseAddress << GPU_TEXTURE_ADDRESS_SHIFT;

        TextureDescAndBaseAddress DstDescAndBaseAddress;
        XGGetTextureDesc( pCompressedTexture, 0, &DstDescAndBaseAddress.Desc );
        DstDescAndBaseAddress.BaseAddress = pCompressedTexture->Format.BaseAddress << GPU_TEXTURE_ADDRESS_SHIFT;

        CompressTexture( &SrcDescAndBaseAddress, &DstDescAndBaseAddress );

        CalculateRMSError( &SrcDescAndBaseAddress, &DstDescAndBaseAddress );
    }

    PIXBeginNamedEvent( 0, "Clear to pattern" );

    // Clear the viewport to a pattern ( this is visible behind the non-full-screen test geometry )
    D3DCOLOR D3D_BLACK      = D3DCOLOR_ARGB( 0xff, 0x00, 0x00, 0x00 );
    D3DCOLOR D3D_TEAL       = D3DCOLOR_ARGB( 0xff, 0x00, 0xff, 0xff );
    UINT iStripCount = 32;
    UINT iStripWidth = m_d3dpp.BackBufferWidth / iStripCount;
    UINT iStripHeight = m_d3dpp.BackBufferHeight;
    for( UINT iStrip = 0; iStrip < iStripCount; ++iStrip )
    {
        D3DRECT d3dRectBlack = { iStripWidth * iStrip, 0, iStripWidth * ( iStrip + 1 ), iStripHeight };
        m_pd3dDevice->Clear( 1, &d3dRectBlack, D3DCLEAR_TARGET, D3D_BLACK, 1.0f, 0L );
        ++iStrip;
        D3DRECT d3dRectGreen = { iStripWidth * iStrip, 0, iStripWidth * ( iStrip + 1 ), iStripHeight };
        m_pd3dDevice->Clear( 1, &d3dRectGreen, D3DCLEAR_TARGET, D3D_TEAL, 1.0f, 0L );
    }

    PIXEndNamedEvent( );

    // With 512x512 source and 512x512 viewport, this causes exact texel-to-pixel
    // sampling, even with bilinear filtering enabled.
    m_pd3dDevice->SetRenderState( D3DRS_HALFPIXELOFFSET, TRUE );

    D3DVIEWPORT9 OldViewport;
    m_pd3dDevice->GetViewport( &OldViewport );

    {
        // Render uncompressed into left-hand viewport
        D3DVIEWPORT9 Viewport;
        Viewport.X = 64;
        Viewport.Y = 192;
        Viewport.Width = 512;
        Viewport.Height = 512;
        Viewport.MinZ = 0.0f;
        Viewport.MaxZ = 1.0f;
        m_pd3dDevice->SetViewport( &Viewport );

        PIXBeginNamedEvent( 0, "Left Viewport (Uncompressed)" );

        // Set up the bool shader constants for the raw texture
        // The raw texture isn't a filterable format, so must use point-sampling
        DrawGeometryToRenderTarget( pRawTexture, iFilterType, pPixelShader, 
            m_pVB, m_pIB, m_iIndexCount );

        PIXEndNamedEvent( );
    }

    {
        // Render compressed into right-hand viewport
        D3DVIEWPORT9 Viewport;
        Viewport.X = 704;
        Viewport.Y = 192;
        Viewport.Width = 512;
        Viewport.Height = 512;
        Viewport.MinZ = 0.0f;
        Viewport.MaxZ = 1.0f;
        m_pd3dDevice->SetViewport( &Viewport );

        // Flashing diffs cause righthand viewport to alternate between compressed and
        // raw each second
        if( m_bFlashingDiffs && ( m_fFlashingTimer > 0.5f ) )
        {
            PIXBeginNamedEvent( 0, "Right Viewport (Uncompressed)" );

            DrawGeometryToRenderTarget( pRawTexture, iFilterType, pPixelShader, 
                m_pVB, m_pIB, m_iIndexCount );

            PIXEndNamedEvent( );
        }
        else 
        {
            PIXBeginNamedEvent( 0, "Right Viewport (Compressed)" );

            DrawGeometryToRenderTarget( pCompressedTexture, iFilterType, pPixelShader, 
                m_pVB, m_pIB, m_iIndexCount );

            PIXEndNamedEvent( );
        }
    }

    m_pd3dDevice->SetViewport( &OldViewport );

    RenderUI( );

    PIXEndNamedEvent( );

    // Present the backbuffer contents to the display
    ATG::g_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    m_pd3dDevice->UnsetAll( );

    return S_OK;
}


//-----------------------------------------------------------------------------
// Name: main( )
// Desc: Entry point to the program.
//-----------------------------------------------------------------------------

VOID __cdecl main( )
{
    Sample atgApp;
    ATG::GetVideoSettings( &atgApp.m_d3dpp.BackBufferWidth, &atgApp.m_d3dpp.BackBufferHeight );

    // Use fixed back buffer resolution regardless of output dimensions
    atgApp.m_d3dpp.BackBufferWidth = 1280;
    atgApp.m_d3dpp.BackBufferHeight = 720;

    // The sample doesn't use depth
    atgApp.m_d3dpp.EnableAutoDepthStencil = FALSE;

    // Make sure display is gamma correct.
    atgApp.m_d3dpp.BackBufferFormat =  ( D3DFORMAT )MAKESRGBFMT( D3DFMT_A8R8G8B8 );
    atgApp.m_d3dpp.FrontBufferFormat = ( D3DFORMAT )MAKESRGBFMT( D3DFMT_LE_X8R8G8B8 );

    atgApp.Run( );
}



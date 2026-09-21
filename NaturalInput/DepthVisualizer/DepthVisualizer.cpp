//---------------------------------------------------------------------------------------------------------
// DepthVisualizer.cpp
//
// Advanced Technology Group
// Copyright (C) Microsoft Corporation. All rights reserved.
//---------------------------------------------------------------------------------------------------------
#include <xtl.h>
#include <xgraphics.h>
#include <xboxmath.h>
#include <AtgApp.h>
#include <AtgFont.h>
#include <AtgHelp.h>
#include <AtgInput.h>
#include <AtgMesh.h>
#include <AtgNuiVisualization.h>
#include <AtgPostProcess.h>
#include <AtgResource.h>
#include <AtgSimpleShaders.h>
#include <AtgUtil.h>
#include <AtgNuiCommon.h>

#include <NuiApi.h>

#include <xbdm.h>

//---------------------------------------------------------------------------------------------------------
// Custom formats for use with normals
//---------------------------------------------------------------------------------------------------------
D3DFORMAT D3DFMT_B6G5R5 = (D3DFORMAT) MAKED3DFMT(
    GPUTEXTUREFORMAT_6_5_5, 
    GPUENDIAN_8IN16, 
    TRUE, 
    GPUSIGN_ALL_UNSIGNED, 
    GPUNUMFORMAT_FRACTION, 
    GPUSWIZZLE_OBGR);
D3DFORMAT D3DFMT_W6V5U5 = (D3DFORMAT) MAKED3DFMT(
    GPUTEXTUREFORMAT_6_5_5, 
    GPUENDIAN_8IN16, 
    TRUE, 
    GPUSIGN_ALL_SIGNED, 
    GPUNUMFORMAT_FRACTION, 
    GPUSWIZZLE_OBGR);

//---------------------------------------------------------------------------------------------------------
// Callouts for labelling the gamepad on the help screen
//---------------------------------------------------------------------------------------------------------
ATG::HELP_CALLOUT g_HelpCallouts[] =
{
    { ATG::HELP_START_BUTTON,   ATG::HELP_PLACEMENT_1, L"Pause" },
    { ATG::HELP_BACK_BUTTON,    ATG::HELP_PLACEMENT_2, L"Display\nhelp" },
    { ATG::HELP_B_BUTTON,       ATG::HELP_PLACEMENT_2, L"Toggle\nBig/small menu" },
    { ATG::HELP_DPAD,           ATG::HELP_PLACEMENT_2, 
        L"Change item focus (u/d)\nDecrease / increase active item (l/r)" },
    { ATG::HELP_LEFT_STICK,     ATG::HELP_PLACEMENT_2, 
        L"Change item focus (u/d)\nDecrease / increase active item (l/r)" },
    { ATG::HELP_RIGHT_STICK,    ATG::HELP_PLACEMENT_2, 
        L"Move sweetspot (thumb button up)\nand floorplane (thumb button down)" },
    { ATG::HELP_RIGHT_TRIGGER,  ATG::HELP_PLACEMENT_1, L"Widen sweetspot" },
    { ATG::HELP_LEFT_TRIGGER,   ATG::HELP_PLACEMENT_1, L"Narrow sweetspot" },
};
#define NUM_HELP_CALLOUTS (sizeof(g_HelpCallouts) / sizeof(g_HelpCallouts[0]))


// Keep track of certain register assignments - duplicate these definitions in .hlsl file
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


// Channels of the 4-bit mask texture --- definitions must match .hlsl file.  
// These apply only to the top mip layer.  The lower mips layers are only used for 
// temporary scratch space.
#define CHANNEL_SHOULD_BE_VALID     X
#define CHANNEL_IS_IR_SHADOW        Y
#define CHANNEL_IS_SMALL_HOLE       Z
#define CHANNEL_IS_SMALL_ISLAND     W

// Preprocessor trickery ... argument substitution does not work with ##
#define CHANNEL_SHIFT_HELPER( type ) D3DFORMAT_SWIZZLE##type##_SHIFT
#define CHANNEL_SHIFT( type ) CHANNEL_SHIFT_HELPER( type )
#define CHANNEL_MASK_HELPER( type ) D3DFORMAT_SWIZZLE##type##_MASK
#define CHANNEL_MASK( type ) CHANNEL_MASK_HELPER( type )

// Swizzle which will cause the data of a 1-channel texture to fetch to the CHANNEL_SHOULD_BE_VALID field of the output,
// and which initializes all other channels to 0.
#define GPUSWIZZLE_ZZZZ ( GPUSWIZZLE_0 | GPUSWIZZLE_0<<3 | GPUSWIZZLE_0<<6 | GPUSWIZZLE_0<<9 ) << D3DFORMAT_SWIZZLEX_SHIFT
#define GPUSWIZZLE_SHOULD_BE_VALID_ZZZ  (                                                                                       \
    ( ( GPUSWIZZLE_X << CHANNEL_SHIFT( CHANNEL_SHOULD_BE_VALID ) ) >> D3DFORMAT_SWIZZLEX_SHIFT )                                \
    | ( ( GPUSWIZZLE_ZZZZ & D3DFORMAT_SWIZZLE_MASK & ~CHANNEL_MASK( CHANNEL_SHOULD_BE_VALID ) ) >> D3DFORMAT_SWIZZLEX_SHIFT )   \
    )


//-------------------------------------------------------------------------------------------------------------------------------------
// Coloring scheme
//-------------------------------------------------------------------------------------------------------------------------------------
XMVECTOR g_vSweetSpotColor = { 0.0f, 0.5f, 1.0f, 1.0f };
XMVECTOR g_vFloorPlaneColor = { 1.0f, 0.0f, 0.7f, 1.0f };
XMVECTOR g_vSweeperPlaneColor = { 1.0f, 0.0f, 0.0f, 0.2f };
XMVECTOR g_vArrowColor = { 0.0f, 1.0f, 0.5f, 1.0f };
XMVECTOR g_vOutlineColor = { 0.0f, 1.0f, 0.0f, 1.0f };


//-------------------------------------------------------------------------------------------------------------------------------------
// Helper functions.  
//-------------------------------------------------------------------------------------------------------------------------------------
template<typename t_type>
t_type Min( t_type a, t_type b ) { return a < b ? a : b; }
template<typename t_type>
t_type Max( t_type a, t_type b ) { return a > b ? a : b; }


//-------------------------------------------------------------------------------------------------------------------------------------
// Struct for vertex of test geometry
//-------------------------------------------------------------------------------------------------------------------------------------
struct TestGeometryVertex
{
    XMFLOAT3 Position;
    XMFLOAT2 TexCoord;
};


//-------------------------------------------------------------------------------------------------------------------------------------
// Color codes for depth map processing.  Must match #defines in .hlsl file  
//-------------------------------------------------------------------------------------------------------------------------------------
enum COLOR_CODE
{
    COLOR_CODE_NO_DEPTH, 
    COLOR_CODE_UNRESOLVED_IR_SHADOW, 
    COLOR_CODE_UNRESOLVED_SMALL_HOLE, 
    COLOR_CODE_RESOLVED_IR_SHADOW, 
    COLOR_CODE_RESOLVED_SMALL_HOLE, 
    COLOR_CODE_SMALL_ISLAND, 

    COLOR_CODE_COUNT
};
const WCHAR* g_strColorCodeNames[] = 
{
    L"No Depth (not IR shadow)", 
    L"No Depth (IR Shadow, unfilled)", 
    L"No Depth (small hole, unfilled)", 
    L"IR Shadow (filled)", 
    L"Small Hole (filled)", 
    L"Small Island (erased)", 
};
C_ASSERT( _countof( g_strColorCodeNames ) == COLOR_CODE_COUNT );
D3DCOLORVALUE g_vColorCodeColors[] = 
{
    { 0.2f, 0.0f, 0.2f, 1.0f }, // dull purple
    { 0.5f, 0.5f, 0.0f, 1.0f }, // dull yellow
    { 0.1f, 0.3f, 0.0f, 1.0f }, // dull green
    { 1.0f, 0.0f, 0.0f, 1.0f }, // red
    { 0.0f, 1.0f, 0.0f, 1.0f }, // green
    { 0.0f, 0.5f, 1.0f, 1.0f }, // cyan
};
C_ASSERT( _countof( g_vColorCodeColors ) == COLOR_CODE_COUNT ); 


//-------------------------------------------------------------------------------------------------------------------------------------
// Supported filter types  
//-------------------------------------------------------------------------------------------------------------------------------------
enum FILTER_TYPES
{
    FILTER_TYPE_GAUSSIAN, 
    FILTER_TYPE_GAUSSIAN_DIAGONAL, 
    FILTER_TYPE_BILATERAL_2D, 
    FILTER_TYPE_BILATERAL_1D, 
    FILTER_TYPE_BILATERAL_1D_DIAGONAL, 

    FILTER_TYPE_COUNT
};

const WCHAR* g_strFilterTypeDepthNames[] = 
{
    L"Gaussian 5x5", 
    L"Gaussian 5x5 (Diagonal)", 
    L"Bilateral 5x5", 
    L"Bilateral 5x5 Separable (Axial)", 
    L"Bilateral 5x5 Separable (Diagonal)", 
};
C_ASSERT( _countof( g_strFilterTypeDepthNames ) == FILTER_TYPE_COUNT );

const WCHAR* g_strFilterTypeNormalNames[] = 
{
    L"Gaussian 5x5", 
    L"Gaussian 5x5 (Diagonal)", 
    L"Bilateral 3x3",   // This one is different for normals, because 5x5 chokes the HLSL compiler
    L"Bilateral 5x5 Separable (Axial)", 
    L"Bilateral 5x5 Separable (Diagonal)", 
};
C_ASSERT( _countof( g_strFilterTypeNormalNames ) == FILTER_TYPE_COUNT );


//-------------------------------------------------------------------------------------------------------------------------------------
// Supported tweakable UI parameters.     
//-------------------------------------------------------------------------------------------------------------------------------------
enum UIParamTypes 
{
    UI_PARAM_LOW_RES_DEPTH, 
    UI_PARAM_LOW_RES_NORMAL,
    UI_PARAM_LOW_PRECISION_NORMAL, 
    UI_PARAM_SMALL_EFFECT_VIEWPORT, 
    UI_PARAM_HIDE_BORDER, 
    UI_PARAM_DRAW_THUMBNAILS, 
    UI_PARAM_COLOR_CODE_THUMBNAILS, 
    UI_PARAM_FILL_IR_SHADOW, 
    UI_PARAM_FILL_SMALL_HOLES, 
    UI_PARAM_SMALL_HOLES_SCALE, 
    UI_PARAM_ERASE_SMALL_ISLANDS, 
    UI_PARAM_SMALL_ISLANDS_SCALE, 
    UI_PARAM_FILTER_DEPTH_COUNT, 
    UI_PARAM_FILTER_DEPTH_TYPE, 
    UI_PARAM_BILATERAL_EDGE_BLUR_DEPTH,
    UI_PARAM_FILTER_NORMAL_COUNT, 
    UI_PARAM_FILTER_NORMAL_TYPE, 
    UI_PARAM_BILATERAL_EDGE_BLUR_NORMAL,
    UI_PARAM_FADE_NORMAL, 
    UI_PARAM_FADE_NORMAL_SCALE, 
    UI_PARAM_RENORMALIZE_NORMAL,
    UI_PARAM_DRAW_FLOOR_PLANE, 

    UI_PARAM_COUNT
};


//-------------------------------------------------------------------------------------------------------------------------------------
// UIParam
//
// Base class for various types of menu selectors
//-------------------------------------------------------------------------------------------------------------------------------------
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
const FLOAT  UIParam::m_fOptionX = 400.0f;

class UIParamEnum : public UIParam
{
public:
    UIParamEnum( const WCHAR* ParamName, const WCHAR** OptionNames, UINT iCount, UINT iValue = 0, UINT iStep = 1 )
        : UIParam( ParamName )
        , m_OptionNames( OptionNames )
        , m_iCount( iCount )
        , m_iValue( iValue )
        , m_iStep( iStep )
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

    UINT                GetValue() { return m_iValue; };
    VOID                SetValue( UINT iValue ) { m_iValue = iValue; };
    VOID                DecreaseValue( FLOAT fScale = 1.0f ) { m_iValue += m_iCount - m_iStep; m_iValue %= m_iCount; }
    VOID                IncreaseValue( FLOAT fScale = 1.0f ) { m_iValue += m_iStep;            m_iValue %= m_iCount; }

protected:
    const WCHAR**       m_OptionNames;
    UINT                m_iCount;
    UINT                m_iValue;
    UINT                m_iStep;
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

static const WCHAR* g_UIntOptionNames[] = { 
    L"0", L"1", L"2", L"3", L"4", L"5", L"6", L"7", L"8", L"9", 
    L"10", L"11", L"12", L"13", L"14", L"15", L"16", L"17", L"18", L"19", 
};
class UIParamUInt : public UIParamEnum
{
public:
    UIParamUInt( const WCHAR* ParamName, UINT iMin, UINT iMax, UINT iValue = 0, UINT iStep = 1 )
        : UIParamEnum( ParamName, g_UIntOptionNames + iMin, iMax - iMin + 1, iValue - iMin, iStep )
        , m_iMin( iMin )
    {
        assert( iMax / iStep * iStep <= _countof( g_UIntOptionNames ) );
    }

    BOOL                GetValue( ) { return UIParamEnum::GetValue( ) + m_iMin; };
    VOID                SetValue( UINT iValue ) { UIParamEnum::SetValue( iValue - m_iMin ); };

private:
    UINT m_iMin;
};

class UIParamFloat : public UIParam
{
public:
    UIParamFloat( const WCHAR* ParamName, FLOAT fValue, FLOAT fIncr )
        : UIParam( ParamName )
        , m_fValue( fValue )
        , m_fIncr( fIncr )
    {}

    VOID RenderOptionUI( ATG::Font* pFont, FLOAT fParamX, FLOAT fParamY, BOOL bActive, BOOL bValid )
    {
        FLOAT fValue = GetValue();
        WCHAR _OptionText[256], *OptionText = _OptionText;
        swprintf_s( _OptionText, L"%2.2f", fValue );
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

    FLOAT               GetValue() { return m_fValue; }
    VOID                DecreaseValue( FLOAT fScale = 1.0f ) 
    { 
        m_fValue -= fScale * m_fIncr;
    }
    VOID                IncreaseValue( FLOAT fScale = 1.0f ) 
    { 
        m_fValue += fScale * m_fIncr;
    }

protected:
    FLOAT               m_fValue;
    FLOAT               m_fIncr;
};


//------------------------------------------------------------------------------------------------------------------
// Name: class Sample
// Desc: Main class to run this application. Most functionality is inherited
// from the ATG::Application base class.
//------------------------------------------------------------------------------------------------------------------
class Sample : public ATG::Application
{
    ATG::Timer                  m_Timer;
    ATG::Font                   m_Font;
    ATG::Help                   m_Help;
    BOOL                        m_bPause;
    BOOL                        m_bDrawHelp;

    // The NUI depth frame
    HANDLE                      m_hDepth;

    // On-screen UI parameters
    UIParamBool                 m_LowResDepthParam;
    UIParamBool                 m_LowResNormalParam;
    UIParamBool                 m_LowPrecisionNormalParam;
    UIParamBool                 m_SmallEffectViewportParam;
    UIParamBool                 m_HideBorderParam;
    UIParamBool                 m_DrawThumbnailsParam;
    UIParamBool                 m_ColorCodeThumbnailsParam;
    UIParamBool                 m_FillIRShadowParam;
    UIParamBool                 m_FillSmallHolesParam;
    UIParamUInt                 m_SmallHolesScaleParam;
    UIParamBool                 m_EraseSmallIslandsParam;
    UIParamUInt                 m_SmallIslandsScaleParam;
    UIParamUInt                 m_FilterDepthCountParam;
    UIParamEnum                 m_FilterDepthTypeParam;
    UIParamFloat                m_BilateralEdgeBlurDepthParam;
    UIParamUInt                 m_FilterNormalCountParam;
    UIParamEnum                 m_FilterNormalTypeParam;
    UIParamFloat                m_BilateralEdgeBlurNormalParam;
    UIParamBool                 m_FadeNormalParam;
    UIParamFloat                m_FadeNormalScaleParam;
    UIParamBool                 m_RenormalizeNormalParam;
    UIParamBool                 m_DrawFloorPlaneParam;

    // The Params as the Menu sees them
    UIParam*                    m_UIParamArray[UI_PARAM_COUNT];
    BOOL                        m_bBigMenu;
    UINT                        m_iActiveUIParameter;   // Which UI item is affected by l/r input
    UINT                        m_iVisibleUIStart;
    UINT                        m_iVisibleUICount;

    // NUI constants and status 
    static const UINT           m_iDepthMapWidth = 320;
    static const UINT           m_iDepthMapHeight = 240;
    FLOAT                       m_fNuiHorizontalFOV;
    FLOAT                       m_fNuiVerticalFOV;
    FLOAT                       m_fCameraElevationRadians;
    FLOAT                       m_fLaserToSensorOffsetWorld;
    FLOAT                       m_fNearPlaneDistance;
    FLOAT                       m_fFarPlaneDistance;
    FLOAT                       m_fRealisticFarDepth; // largest depth value which the sensor tends to ever return

    // Simulated sweet spot (standard values shown to the right, but actual values tuned for a small office)
    // Modify using right thumbstick 
    FLOAT                       m_fSweetSpotHeight;   // =  4000.0f;
    FLOAT                       m_fSweetSpotCenterX;  // =     0.0f;
    FLOAT                       m_fSweetSpotCenterZ;  // =  2260.0f;
    FLOAT                       m_fSweetSpotRadius;   // =   460.0f;
    FLOAT                       m_fPulsedSweetSpotRadius;

    // Simulated floor plane (value tuned for camera on a desk)
    // Modify using right thumbstick (with stick pressed in)
    FLOAT                       m_fFloorPlaneHeight;

    // Texture resources
    IDirect3DTexture9*          m_pDepthTiledTexture;
    IDirect3DTexture9*          m_pNormalTexture;
    IDirect3DTexture9*          m_pIRShadowTexture;
    IDirect3DTexture9*          m_pMaskTexture; // Different channels for different purposes

    // Render target resources
    IDirect3DSurface9*          m_pDepthRenderTarget;
    IDirect3DSurface9*          m_pNormalRenderTarget;
    IDirect3DSurface9*          m_pIRShadowDepthStencilSurface;
    IDirect3DSurface9*          m_pMaskRenderTarget;

    // Shaders
    IDirect3DVertexShader9*     m_pVertexShader;
    IDirect3DPixelShader9*      m_pSolidColorPixelShader;
    IDirect3DPixelShader9*      m_pCopySignedToBiasPixelShader;
    IDirect3DVertexShader9*     m_pIRShadowGenerateVertexShader;
    IDirect3DPixelShader9*      m_pIRShadowApplyPixelShader;
    IDirect3DPixelShader9*      m_pDownfillSmallHolesPixelShader;
    IDirect3DPixelShader9*      m_pUpfillSmallHolesPixelShader;
    IDirect3DPixelShader9*      m_pDownfillSmallIslandsPixelShader;
    IDirect3DPixelShader9*      m_pUpfillSmallIslandsPixelShader;
    IDirect3DPixelShader9*      m_pDownsampleDepthPixelShader;
    IDirect3DPixelShader9*      m_pShiftDepthPixelShader;
    IDirect3DPixelShader9*      m_pUpfillDepthPixelShader;
    IDirect3DPixelShader9*      m_pDirectionalBlurDepthHorizontalPixelShader;
    IDirect3DPixelShader9*      m_pDirectionalBlurDepthVerticalPixelShader;
    IDirect3DPixelShader9*      m_pDirectionalBlurDepthDiagonalNWPixelShader;
    IDirect3DPixelShader9*      m_pDirectionalBlurDepthDiagonalSWPixelShader;
    IDirect3DPixelShader9*      m_pBilateralFilterDepth1DHorizontalPixelShader;
    IDirect3DPixelShader9*      m_pBilateralFilterDepth1DVerticalPixelShader;
    IDirect3DPixelShader9*      m_pBilateralFilterDepth1DDiagonalNWPixelShader;
    IDirect3DPixelShader9*      m_pBilateralFilterDepth1DDiagonalSWPixelShader;
    IDirect3DPixelShader9*      m_pBilateralFilterDepth2DPixelShader;
    IDirect3DPixelShader9*      m_pDepthToNormalPixelShader;
    IDirect3DPixelShader9*      m_pDirectionalBlurNormalHorizontalPixelShader;
    IDirect3DPixelShader9*      m_pDirectionalBlurNormalVerticalPixelShader;
    IDirect3DPixelShader9*      m_pDirectionalBlurNormalDiagonalNWPixelShader;
    IDirect3DPixelShader9*      m_pDirectionalBlurNormalDiagonalSWPixelShader;
    IDirect3DPixelShader9*      m_pBilateralFilterNormal1DHorizontalPixelShader;
    IDirect3DPixelShader9*      m_pBilateralFilterNormal1DVerticalPixelShader;
    IDirect3DPixelShader9*      m_pBilateralFilterNormal1DDiagonalNWPixelShader;
    IDirect3DPixelShader9*      m_pBilateralFilterNormal1DDiagonalSWPixelShader;
    IDirect3DPixelShader9*      m_pBilateralFilterNormal2DPixelShader;
    IDirect3DPixelShader9*      m_pDownsampleNormalPixelShader;
    IDirect3DPixelShader9*      m_pDepthMapVisualizePixelShader;
    IDirect3DPixelShader9*      m_pSyntheticPlanePixelShader;
    IDirect3DPixelShader9*      m_pDepthThumbnailPixelShader;

    // Geometry resources 
    IDirect3DVertexDeclaration9* m_pVertexDecl;
    IDirect3DVertexBuffer9*     m_pQuadVB;
    IDirect3DIndexBuffer9*      m_pQuadIB;
    UINT                        m_iQuadIndexCount;
    IDirect3DVertexBuffer9*     m_pSweetSpotVB;
    IDirect3DIndexBuffer9*      m_pSweetSpotIB;
    IDirect3DIndexBuffer9*      m_pSweetSpotLineIB;
    UINT                        m_iSweetSpotIndexCount;
    UINT                        m_iSweetSpotLineIndexCount;
    IDirect3DVertexBuffer9*     m_pGridVB;
    IDirect3DIndexBuffer9*      m_pGridIB;
    IDirect3DIndexBuffer9*      m_pGridLineIB;
    UINT                        m_iGridIndexCount;
    UINT                        m_iGridLineIndexCount;
    IDirect3DVertexBuffer9*     m_pArrowVB;
    IDirect3DIndexBuffer9*      m_pArrowIB;
    IDirect3DIndexBuffer9*      m_pArrowLineIB;
    UINT                        m_iArrowIndexCount;
    UINT                        m_iArrowLineIndexCount;

    // Generate & render synthetic geometry types
    VOID GenerateGeometryQuad( );
    VOID GenerateGeometrySweetSpot( UINT iWedges, UINT iSlices );
    VOID GenerateGeometryGrid( UINT iDivisionsX, UINT iDivisionsY );
    VOID GenerateGeometryArrow( UINT iWedges, UINT iConeSlices, UINT iStalkSlices, FLOAT fConeRadius, 
        FLOAT fConeLength, FLOAT fStalkRadius, FLOAT fStalkLength );
    VOID DrawQuad( IDirect3DPixelShader9* pPixelShader );
    VOID DrawSweetSpot( IDirect3DPixelShader9* pPixelShader, const XMMATRIX& matWorld );
    VOID DrawGrid( IDirect3DPixelShader9* pPixelShader, const XMMATRIX& matWorld );
    VOID DrawArrow( IDirect3DPixelShader9* pPixelShader, const XMMATRIX& matWorld );

    // Create surfaces
    HRESULT CreateTextureNonPackedMips( UINT Width,
        UINT Height,
        UINT Levels,
        DWORD Usage,
        D3DFORMAT Format,
        D3DPOOL UnusedPool,
        IDirect3DTexture9 **ppTexture,
        HANDLE *pUnusedSharedHandle );
    HRESULT ReleaseTextureNonPackedMips( IDirect3DTexture9 *pTexture );
    HRESULT CreateTexturesAndRenderTargets();

    // Render phases
    VOID DownsampleDepth( IDirect3DTexture9*& pDepthTexture );
    IDirect3DTexture9* AliasDepthAsMask( IDirect3DTexture9* pDepthTexture, IDirect3DTexture9* pAliasTexture );
    VOID MarkIRShadow( IDirect3DTexture9* pDepthTexture, IDirect3DTexture9*& pMaskTexture );
    VOID SetViewportFromDesc( CONST XGTEXTURE_DESC* Desc );
    VOID MarkSmallHoles( IDirect3DTexture9* pDepthTexture, 
        IDirect3DTexture9*& pMaskTexture, 
        BOOL bHolesOrIslands, 
        BOOL bLowResDepth );
    VOID FillInDepth( IDirect3DTexture9*& pDepthTexture, IDirect3DTexture9* pMaskTexture );
    VOID FilterDepth( IDirect3DTexture9*& pDepthTexture, 
        UINT iFilterDepthCount, 
        UINT iFilterDepthType, 
        FLOAT fBilateralEdgeBlurDepth );
    VOID ClearAs16IfPointSampling( IDirect3DTexture9* pNormalTexture );
    VOID ResolveNormalMap( BOOL b16bppNormal, UINT iMip = 0 );
    VOID InferNormalMap( IDirect3DTexture9* pDepthTexture, 
        BOOL b16bppNormal );
    VOID FilterNormal( UINT iFilterNormalCount, 
        UINT iFilterNormalType, 
        FLOAT fBilateralEdgeBlurNormal, 
        BOOL b16bppNormal );
    VOID DownsampleNormal( BOOL b16bppNormal );
    VOID DrawThumbnails( IDirect3DTexture9* pDepthTexture, 
        IDirect3DTexture9* pMaskTexture, 
        BOOL bRenormalizeNormal ); 
    VOID VisualizeDepthRenderDepthMap( IDirect3DTexture9* pDepthTexture, 
        XMVECTOR vSweetSpotOriginAndRadius, 
        XMVECTOR vSweetSpotDirection, 
        XMVECTOR vFloorPlane, 
        XMVECTOR vSweeperPlane, 
        XMVECTOR vSweetSpotColor, 
        XMVECTOR vFloorPlaneColor, 
        XMVECTOR vSweeperPlaneColor, 
        BOOL bDrawFloorPlane, 
        BOOL bSweeperOn, 
        BOOL bFadeNormal, 
        FLOAT fFadeNormalScale, 
        BOOL bRenormalizeNormal );
    VOID VisualizeDepthRenderFloorPlane( XMVECTOR vSweetSpotOriginAndRadius, 
        XMVECTOR vSweetSpotDirection, 
        XMVECTOR vSweetSpotColor, 
        XMVECTOR vFloorPlaneColor );
    VOID VisualizeDepthRenderSweeperPlane( XMMATRIX& matWorldSweep, 
        XMVECTOR vSweetSpotOriginAndRadius, 
        XMVECTOR vSweetSpotDirection, 
        XMVECTOR vSweetSpotColor, 
        XMVECTOR vSweeperPlaneColor );
    VOID VisualizeDepthRenderSweetSpot( XMVECTOR vSweetSpotColor );
    VOID VisualizeDepthRenderArrows( XMVECTOR vArrowColor );
    VOID VisualizeDepth( IDirect3DTexture9* pDepthTexture, 
        BOOL bSmallEffectViewport, 
        BOOL bHideBorder, 
        BOOL bFadeNormal, 
        FLOAT fFadeNormalScale, 
        BOOL bRenormalizeNormal, 
        BOOL bDrawFloorPlane ) ;
    VOID RenderUI( );

    HRESULT StartupCamera();

public:
    Sample( );

    HRESULT Initialize();
    HRESULT Update();
    HRESULT Render();
};


//------------------------------------------------------------------------------------------------------------------
// Name: main()
// Desc: Entry point to the program
//------------------------------------------------------------------------------------------------------------------
INT __cdecl main()
{
    Sample atgApp;
    ATG::GetVideoSettings( &atgApp.m_d3dpp.BackBufferWidth, &atgApp.m_d3dpp.BackBufferHeight );

    // Use fixed back buffer resolution regardless of output dimensions
    atgApp.m_d3dpp.BackBufferWidth = 1280;
    atgApp.m_d3dpp.BackBufferHeight = 720;

    // Make sure display is gamma correct.
    atgApp.m_d3dpp.BackBufferFormat =  ( D3DFORMAT )MAKESRGBFMT( D3DFMT_A8R8G8B8 );
    atgApp.m_d3dpp.FrontBufferFormat = ( D3DFORMAT )MAKESRGBFMT( D3DFMT_LE_X8R8G8B8 );

    atgApp.m_d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_TWO;  // Recommended for Kinect

    atgApp.Run();
}


//---------------------------------------------------------------------------------------------------------
// Name: Sample::Sample( )
// Desc: Initializes the UI elements.
//---------------------------------------------------------------------------------------------------------
Sample::Sample( )
: m_LowResDepthParam( L"Low Res Depth", FALSE )
, m_LowResNormalParam( L"Low Res Normal", FALSE )
, m_LowPrecisionNormalParam( L"Low Precision Normal", FALSE )
, m_SmallEffectViewportParam( L"Small Effect Viewport", FALSE )
, m_HideBorderParam( L"Hide Image Margin", FALSE )
, m_DrawThumbnailsParam( L"Thumbnail Depth and Normal", TRUE )
, m_ColorCodeThumbnailsParam( L"Color Code Depth", TRUE )
, m_FillIRShadowParam( L"Fill IR Shadow", TRUE )
, m_FillSmallHolesParam( L"Fill Small Holes", TRUE )
, m_SmallHolesScaleParam( L"Small Holes Scale (log)", 1, 7, 4 )
, m_EraseSmallIslandsParam( L"Erase Small Islands", TRUE )
, m_SmallIslandsScaleParam( L"Small Islands Scale (log)", 1, 7, 5 )
, m_FilterDepthCountParam( L"Depth Filter Iterations", 0, 10, 2 )
, m_FilterDepthTypeParam( L"Depth Filter Type", g_strFilterTypeDepthNames, _countof(g_strFilterTypeDepthNames), FILTER_TYPE_BILATERAL_1D_DIAGONAL )
, m_BilateralEdgeBlurDepthParam( L"Depth Bilateral Edge Blur (log)", 2.5f, 0.5f )
, m_FilterNormalCountParam( L"Normal Filter Iterations", 0, 10, 1 )
, m_FilterNormalTypeParam( L"Normal Filter Type", g_strFilterTypeNormalNames, _countof(g_strFilterTypeNormalNames), FILTER_TYPE_BILATERAL_2D )
, m_BilateralEdgeBlurNormalParam( L"Normal Bilateral Edge Blur (log)", 0.4f, 0.2f )
, m_FadeNormalParam( L"Fade Normal", TRUE )
, m_FadeNormalScaleParam( L"Fade Normal Scale (log)", 2.0f, 1.0f )
, m_RenormalizeNormalParam( L"Renormalize Normal", FALSE )
, m_DrawFloorPlaneParam( L"Draw Floor Plane", FALSE )
{
    m_UIParamArray[UI_PARAM_LOW_RES_DEPTH]                  = &m_LowResDepthParam;
    m_UIParamArray[UI_PARAM_LOW_RES_NORMAL]                 = &m_LowResNormalParam;
    m_UIParamArray[UI_PARAM_LOW_PRECISION_NORMAL]           = &m_LowPrecisionNormalParam;
    m_UIParamArray[UI_PARAM_SMALL_EFFECT_VIEWPORT]          = &m_SmallEffectViewportParam;
    m_UIParamArray[UI_PARAM_HIDE_BORDER]                    = &m_HideBorderParam;
    m_UIParamArray[UI_PARAM_DRAW_THUMBNAILS]                = &m_DrawThumbnailsParam;
    m_UIParamArray[UI_PARAM_COLOR_CODE_THUMBNAILS]          = &m_ColorCodeThumbnailsParam;
    m_UIParamArray[UI_PARAM_FILL_IR_SHADOW]                 = &m_FillIRShadowParam; 
    m_UIParamArray[UI_PARAM_FILL_SMALL_HOLES]               = &m_FillSmallHolesParam; 
    m_UIParamArray[UI_PARAM_SMALL_HOLES_SCALE]              = &m_SmallHolesScaleParam; 
    m_UIParamArray[UI_PARAM_ERASE_SMALL_ISLANDS]            = &m_EraseSmallIslandsParam; 
    m_UIParamArray[UI_PARAM_SMALL_ISLANDS_SCALE]            = &m_SmallIslandsScaleParam; 
    m_UIParamArray[UI_PARAM_FILTER_DEPTH_COUNT]             = &m_FilterDepthCountParam;
    m_UIParamArray[UI_PARAM_FILTER_DEPTH_TYPE]              = &m_FilterDepthTypeParam;
    m_UIParamArray[UI_PARAM_BILATERAL_EDGE_BLUR_DEPTH]      = &m_BilateralEdgeBlurDepthParam;
    m_UIParamArray[UI_PARAM_FILTER_NORMAL_COUNT]            = &m_FilterNormalCountParam;
    m_UIParamArray[UI_PARAM_FILTER_NORMAL_TYPE]             = &m_FilterNormalTypeParam;
    m_UIParamArray[UI_PARAM_BILATERAL_EDGE_BLUR_NORMAL]     = &m_BilateralEdgeBlurNormalParam;
    m_UIParamArray[UI_PARAM_FADE_NORMAL]                    = &m_FadeNormalParam;
    m_UIParamArray[UI_PARAM_FADE_NORMAL_SCALE]              = &m_FadeNormalScaleParam;
    m_UIParamArray[UI_PARAM_RENORMALIZE_NORMAL]             = &m_RenormalizeNormalParam;
    m_UIParamArray[UI_PARAM_DRAW_FLOOR_PLANE]              = &m_DrawFloorPlaneParam;
}

    
//------------------------------------------------------------------------------------------------------------------
// Name: Initialize()
// Desc: Initialize the app
//------------------------------------------------------------------------------------------------------------------
HRESULT Sample::Initialize()
{
    HRESULT hr = S_OK;

    // Initialize camera
    hr = StartupCamera();
    if ( FAILED( hr ) )
        return hr;

    // Create the font
    if( FAILED( m_Font.Create( "game:\\Media\\Fonts\\Arial_16.xpr" ) ) )
    {
        ATG::FatalError( "Couldn't create Font Arial_16.xpr\n" );
    }

    // Create the help
    if( FAILED( hr = m_Help.Create( "game:\\Media\\Help\\Help.xpr" ) ) )
    {
        ATG_PrintError( "Couldn't create help\n" );
        return hr;
    }

    // Expanding Font area to get additional screen real estate...
    m_Font.SetWindow( 64, 8, m_d3dpp.BackBufferWidth - 64, m_d3dpp.BackBufferHeight - 8 );

    // Create common vertex declaration used by all the geometry
    static const D3DVERTEXELEMENT9 decl[] =
    {
        { 0, 0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
        { 0, 12, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
        D3DDECL_END( )
    };
    m_pd3dDevice->CreateVertexDeclaration( decl, &m_pVertexDecl );

    // NUI constants
    m_fNuiHorizontalFOV  = NUI_CAMERA_DEPTH_NOMINAL_HORIZONTAL_FOV;
    m_fNuiVerticalFOV    = NUI_CAMERA_DEPTH_NOMINAL_VERTICAL_FOV;

    // Double-check FOV & aspect ratio consistent with each other (should be roughly correct for default values)
    FLOAT FovAngleY      = XMConvertToRadians( m_fNuiVerticalFOV );
    FLOAT AspectRatio    = m_iDepthMapWidth / (FLOAT) m_iDepthMapHeight;
    FLOAT FovAngleX      = 2.0f * atanf( AspectRatio * tan( FovAngleY / 2.0f ) ); 
    m_fNuiHorizontalFOV  = XMConvertToDegrees( FovAngleX ); // actually around 57.8

    // Sensor is 8.5 cm to the right of laser
    m_fLaserToSensorOffsetWorld = 85.0f; 

    // These cover the full Nui depth range, and more.
    m_fNearPlaneDistance =    100.0;
    m_fFarPlaneDistance  =  10000.0;
    m_fRealisticFarDepth = 4000.0f;

    // Here, and elsewhere, physical units are in mm.
    // The sweetspot here conforms roughly to recommendations in the docs, except moved closer, to fit
    // better in a typical office environment.
    m_fSweetSpotHeight   =  4000.0f;
    m_fSweetSpotCenterX  =     0.0f;
    m_fSweetSpotCenterZ  =  1800.0f;    //2260.0f;
    m_fSweetSpotRadius   =   460.0f;

    // We can only make an educated guess for floor plane, since the Nui APIs require skeletal 
    // tracking to retrieve the actual floor plane.  A title might do a least-squares analysis to
    // determine a best-fit plane.
    m_fFloorPlaneHeight  =   -1400.0;    // mid-range recommended placement ~ 1.4 m ???

    // Build utility meshes
    GenerateGeometryQuad( );
    GenerateGeometrySweetSpot( 30, 40 );
    GenerateGeometryGrid( 60, 50 );
    GenerateGeometryArrow( 12, 1, 2, 0.5f, 0.25f, 0.25f, 0.75f );

    // Create shaders
    if( FAILED( ATG::LoadVertexShader( "game:\\Media\\Shaders\\ScreenSpaceShader.xvu",
                                       &m_pVertexShader ) ) )
    {
        ATG::FatalError( "Couldn't create VertexShader\n" );
    }
    if( FAILED( ATG::LoadPixelShader( "game:\\Media\\Shaders\\SolidColor.xpu",
                                       &m_pSolidColorPixelShader ) ) )
    {
        ATG::FatalError( "Couldn't create PixelShader\n" );
    }
    if( FAILED( ATG::LoadPixelShader( "game:\\Media\\Shaders\\DepthThumbnail.xpu",
                                       &m_pDepthThumbnailPixelShader ) ) )
    {
        ATG::FatalError( "Couldn't create PixelShader\n" );
    }
    if( FAILED( ATG::LoadPixelShader( "game:\\Media\\Shaders\\CopySignedToBias.xpu",
                                       &m_pCopySignedToBiasPixelShader ) ) )
    {
        ATG::FatalError( "Couldn't create PixelShader\n" );
    }
    if( FAILED( ATG::LoadPixelShader( "game:\\Media\\Shaders\\DirectionalBlurDepthHorizontal.xpu",
                                       &m_pDirectionalBlurDepthHorizontalPixelShader ) ) )
    {
        ATG::FatalError( "Couldn't create PixelShader\n" );
    }
    if( FAILED( ATG::LoadPixelShader( "game:\\Media\\Shaders\\DirectionalBlurDepthVertical.xpu",
                                       &m_pDirectionalBlurDepthVerticalPixelShader ) ) )
    {
        ATG::FatalError( "Couldn't create PixelShader\n" );
    }
    if( FAILED( ATG::LoadPixelShader( "game:\\Media\\Shaders\\DirectionalBlurDepthDiagonalNW.xpu",
                                       &m_pDirectionalBlurDepthDiagonalNWPixelShader ) ) )
    {
        ATG::FatalError( "Couldn't create PixelShader\n" );
    }
    if( FAILED( ATG::LoadPixelShader( "game:\\Media\\Shaders\\DirectionalBlurDepthDiagonalSW.xpu",
                                       &m_pDirectionalBlurDepthDiagonalSWPixelShader ) ) )
    {
        ATG::FatalError( "Couldn't create PixelShader\n" );
    }
    if( FAILED( ATG::LoadPixelShader( "game:\\Media\\Shaders\\BilateralFilterDepth1DHorizontal.xpu",
                                       &m_pBilateralFilterDepth1DHorizontalPixelShader ) ) )
    {
        ATG::FatalError( "Couldn't create PixelShader\n" );
    }
    if( FAILED( ATG::LoadPixelShader( "game:\\Media\\Shaders\\BilateralFilterDepth1DVertical.xpu",
                                       &m_pBilateralFilterDepth1DVerticalPixelShader ) ) )
    {
        ATG::FatalError( "Couldn't create PixelShader\n" );
    }
    if( FAILED( ATG::LoadPixelShader( "game:\\Media\\Shaders\\BilateralFilterDepth1DDiagonalNW.xpu",
                                       &m_pBilateralFilterDepth1DDiagonalNWPixelShader ) ) )
    {
        ATG::FatalError( "Couldn't create PixelShader\n" );
    }
    if( FAILED( ATG::LoadPixelShader( "game:\\Media\\Shaders\\BilateralFilterDepth1DDiagonalSW.xpu",
                                       &m_pBilateralFilterDepth1DDiagonalSWPixelShader ) ) )
    {
        ATG::FatalError( "Couldn't create PixelShader\n" );
    }
    if( FAILED( ATG::LoadPixelShader( "game:\\Media\\Shaders\\BilateralFilterDepth2D.xpu",
                                       &m_pBilateralFilterDepth2DPixelShader ) ) )
    {
        ATG::FatalError( "Couldn't create PixelShader\n" );
    }
    if( FAILED( ATG::LoadPixelShader( "game:\\Media\\Shaders\\DirectionalBlurNormalHorizontal.xpu",
                                       &m_pDirectionalBlurNormalHorizontalPixelShader ) ) )
    {
        ATG::FatalError( "Couldn't create PixelShader\n" );
    }
    if( FAILED( ATG::LoadPixelShader( "game:\\Media\\Shaders\\DirectionalBlurNormalVertical.xpu",
                                       &m_pDirectionalBlurNormalVerticalPixelShader ) ) )
    {
        ATG::FatalError( "Couldn't create PixelShader\n" );
    }
    if( FAILED( ATG::LoadPixelShader( "game:\\Media\\Shaders\\DirectionalBlurNormalDiagonalNW.xpu",
                                       &m_pDirectionalBlurNormalDiagonalNWPixelShader ) ) )
    {
        ATG::FatalError( "Couldn't create PixelShader\n" );
    }
    if( FAILED( ATG::LoadPixelShader( "game:\\Media\\Shaders\\DirectionalBlurNormalDiagonalSW.xpu",
                                       &m_pDirectionalBlurNormalDiagonalSWPixelShader ) ) )
    {
        ATG::FatalError( "Couldn't create PixelShader\n" );
    }
    if( FAILED( ATG::LoadPixelShader( "game:\\Media\\Shaders\\BilateralFilterNormal1DHorizontal.xpu",
                                       &m_pBilateralFilterNormal1DHorizontalPixelShader ) ) )
    {
        ATG::FatalError( "Couldn't create PixelShader\n" );
    }
    if( FAILED( ATG::LoadPixelShader( "game:\\Media\\Shaders\\BilateralFilterNormal1DVertical.xpu",
                                       &m_pBilateralFilterNormal1DVerticalPixelShader ) ) )
    {
        ATG::FatalError( "Couldn't create PixelShader\n" );
    }
    if( FAILED( ATG::LoadPixelShader( "game:\\Media\\Shaders\\BilateralFilterNormal1DDiagonalNW.xpu",
                                       &m_pBilateralFilterNormal1DDiagonalNWPixelShader ) ) )
    {
        ATG::FatalError( "Couldn't create PixelShader\n" );
    }
    if( FAILED( ATG::LoadPixelShader( "game:\\Media\\Shaders\\BilateralFilterNormal1DDiagonalSW.xpu",
                                       &m_pBilateralFilterNormal1DDiagonalSWPixelShader ) ) )
    {
        ATG::FatalError( "Couldn't create PixelShader\n" );
    }
    if( FAILED( ATG::LoadPixelShader( "game:\\Media\\Shaders\\BilateralFilterNormal2D.xpu",
                                       &m_pBilateralFilterNormal2DPixelShader ) ) )
    {
        ATG::FatalError( "Couldn't create PixelShader\n" );
    }
    if( FAILED( ATG::LoadVertexShader( "game:\\Media\\Shaders\\IRShadowGenerate.xvu",
                                       &m_pIRShadowGenerateVertexShader ) ) )
    {
        ATG::FatalError( "Couldn't create VertexShader\n" );
    }
    if( FAILED( ATG::LoadPixelShader( "game:\\Media\\Shaders\\IRShadowApply.xpu",
                                       &m_pIRShadowApplyPixelShader ) ) )
    {
        ATG::FatalError( "Couldn't create PixelShader\n" );
    }
    if( FAILED( ATG::LoadPixelShader( "game:\\Media\\Shaders\\DownfillSmallHoles.xpu",
                                       &m_pDownfillSmallHolesPixelShader ) ) )
    {
        ATG::FatalError( "Couldn't create PixelShader\n" );
    }
    if( FAILED( ATG::LoadPixelShader( "game:\\Media\\Shaders\\UpfillSmallHoles.xpu",
                                       &m_pUpfillSmallHolesPixelShader ) ) )
    {
        ATG::FatalError( "Couldn't create PixelShader\n" );
    }
    if( FAILED( ATG::LoadPixelShader( "game:\\Media\\Shaders\\DownfillSmallIslands.xpu",
                                       &m_pDownfillSmallIslandsPixelShader ) ) )
    {
        ATG::FatalError( "Couldn't create PixelShader\n" );
    }
    if( FAILED( ATG::LoadPixelShader( "game:\\Media\\Shaders\\UpfillSmallIslands.xpu",
                                       &m_pUpfillSmallIslandsPixelShader ) ) )
    {
        ATG::FatalError( "Couldn't create PixelShader\n" );
    }
    if( FAILED( ATG::LoadPixelShader( "game:\\Media\\Shaders\\DownsampleDepth.xpu",
                                       &m_pDownsampleDepthPixelShader ) ) )
    {
        ATG::FatalError( "Couldn't create PixelShader\n" );
    }
    if( FAILED( ATG::LoadPixelShader( "game:\\Media\\Shaders\\ShiftDepth.xpu",
                                       &m_pShiftDepthPixelShader ) ) )
    {
        ATG::FatalError( "Couldn't create PixelShader\n" );
    }
    if( FAILED( ATG::LoadPixelShader( "game:\\Media\\Shaders\\UpfillDepth.xpu",
                                       &m_pUpfillDepthPixelShader ) ) )
    {
        ATG::FatalError( "Couldn't create PixelShader\n" );
    }
    if( FAILED( ATG::LoadPixelShader( "game:\\Media\\Shaders\\DepthToNormal.xpu",
                                       &m_pDepthToNormalPixelShader ) ) )
    {
        ATG::FatalError( "Couldn't create PixelShader\n" );
    }
    if( FAILED( ATG::LoadPixelShader( "game:\\Media\\Shaders\\DownsampleNormal.xpu",
                                       &m_pDownsampleNormalPixelShader ) ) )
    {
        ATG::FatalError( "Couldn't create PixelShader\n" );
    }
    if( FAILED( ATG::LoadPixelShader( "game:\\Media\\Shaders\\DepthMapVisualize.xpu",
                                       &m_pDepthMapVisualizePixelShader ) ) )
    {
        ATG::FatalError( "Couldn't create PixelShader\n" );
    }
    if( FAILED( ATG::LoadPixelShader( "game:\\Media\\Shaders\\SyntheticPlane.xpu",
                                       &m_pSyntheticPlanePixelShader ) ) )
    {
        ATG::FatalError( "Couldn't create PixelShader\n" );
    }

    // Prevent Release of uninitialized resources by CreateTexturesAndRenderTargets
    m_pDepthTiledTexture = NULL;
    m_pNormalTexture = NULL;
    m_pIRShadowTexture = NULL;
    m_pMaskTexture = NULL;
    m_pDepthRenderTarget = NULL;
    m_pNormalRenderTarget = NULL;
    m_pIRShadowDepthStencilSurface = NULL;
    m_pMaskRenderTarget = NULL;

    // Init other member variables
    m_bBigMenu = FALSE;
    m_iActiveUIParameter = 0;
    m_iVisibleUIStart = 0;
    m_iVisibleUICount = 3;

    m_bPause = FALSE;
    m_bDrawHelp = FALSE;

    return hr;
}


//------------------------------------------------------------------------------------------------------------------
// Name: CreateTexturesAndRenderTargets()
// Desc: Create textures and render targets according to sizes chosen in UI
//------------------------------------------------------------------------------------------------------------------
HRESULT Sample::CreateTexturesAndRenderTargets()
{
    BOOL bLowResDepth = m_LowResDepthParam.GetValue();
    BOOL bLowResNormal = m_LowResNormalParam.GetValue();
    BOOL b16bppNormal = m_LowPrecisionNormalParam.GetValue();

    UINT iDepthMapWidth = bLowResDepth ? ( m_iDepthMapWidth / 2 ) : m_iDepthMapWidth;
    UINT iDepthMapHeight = bLowResDepth ? ( m_iDepthMapHeight / 2 ) : m_iDepthMapHeight;
    UINT iNormalMapWidth = bLowResNormal ? ( m_iDepthMapWidth / 2 ) : m_iDepthMapWidth;
    UINT iNormalMapHeight = bLowResNormal ? ( m_iDepthMapHeight / 2 ) : m_iDepthMapHeight;
    D3DFORMAT d3dFmtNormalMap = b16bppNormal ? D3DFMT_W6V5U5 : D3DFMT_A2W10V10U10;

    // Build auxiliary tiled depth texture.
    // Tiled and linear could alias the same memory, but it's a bit messy,
    // as the alignment requirements don't match.
    if( m_pDepthTiledTexture )
    {
        ReleaseTextureNonPackedMips( m_pDepthTiledTexture );
    }
    if( FAILED( CreateTextureNonPackedMips( iDepthMapWidth,
        iDepthMapHeight,
        0,  // full mips for downsampling
        0,
        D3DFMT_D16,
        0,
        &m_pDepthTiledTexture,
        NULL ) ) )
    {
        return E_FAIL;
    }
    m_pDepthTiledTexture->Format.ExpAdjust = -3; // Get rid of the 3 segmentation bits

    if( m_pDepthRenderTarget )
    {
        m_pDepthRenderTarget->Release();
    }

    D3DSURFACE_PARAMETERS SurfParams = {};

    // Avoid overwriting the in-progress backbuffer
    SurfParams.Base = XGSurfaceSize( m_d3dpp.BackBufferWidth, 
        m_d3dpp.BackBufferWidth, 
        m_d3dpp.BackBufferFormat, 
        m_d3dpp.MultiSampleType );

    // Bias back to original depth units
    SurfParams.ColorExpBias = +3;
    if( FAILED( m_pd3dDevice->CreateRenderTarget( iDepthMapWidth, 
        iDepthMapHeight,
        D3DFMT_R32F, 
        D3DMULTISAMPLE_NONE, 
        0, 
        FALSE, 
        &m_pDepthRenderTarget, 
        &SurfParams ) ) )
    {
        return E_FAIL;
    }
    SurfParams.ColorExpBias = 0;

    if( m_pNormalTexture )
    {
        ReleaseTextureNonPackedMips( m_pNormalTexture );
    }
    if( FAILED( CreateTextureNonPackedMips( iNormalMapWidth, 
        iNormalMapHeight, 
        0, 
        0, 
        d3dFmtNormalMap, 
        0, 
        &m_pNormalTexture, 
        NULL ) ) )
    {
        return E_FAIL;
    }

    if( m_pNormalRenderTarget )
    {
        m_pNormalRenderTarget->Release();
    }
    if( FAILED( m_pd3dDevice->CreateRenderTarget( iNormalMapWidth, 
        iNormalMapHeight,
        D3DFMT_A2B10G10R10, 
        D3DMULTISAMPLE_NONE, 
        0, 
        FALSE, 
        &m_pNormalRenderTarget, 
        &SurfParams ) ) )
    {
        return E_FAIL;
    }

    // We use a shadow map 1/2 the dimensions of the working depth map
    if( m_pIRShadowTexture )
    {
        m_pIRShadowTexture->Release();
    }
    if( FAILED( m_pd3dDevice->CreateTexture( iDepthMapWidth / 2, 
        iDepthMapHeight / 2, 
        1, 
        0, 
        D3DFMT_D24S8, 
        0, 
        &m_pIRShadowTexture, 
        NULL ) ) )
    {
        return E_FAIL;
    }
    m_pIRShadowTexture->Format.ExpAdjust = +13; // Fetch from [0.1] into NUI units of mm

    if( m_pIRShadowDepthStencilSurface )
    {
        m_pIRShadowDepthStencilSurface->Release();
    }
    if( FAILED( m_pd3dDevice->CreateDepthStencilSurface( iDepthMapWidth / 2, 
        iDepthMapHeight / 2,
        D3DFMT_D24S8, 
        D3DMULTISAMPLE_NONE, 
        0, 
        FALSE, 
        &m_pIRShadowDepthStencilSurface, 
        &SurfParams ) ) )
    {
        return E_FAIL;
    }

    // The Mask texture and render target is basically a 4-bit bitfield.  
    // The top mip will reserve each channel for a different purpose.  
    // The lower mips are working memory which will be overwritten in several places.
    if( m_pMaskTexture )
    {
        ReleaseTextureNonPackedMips( m_pMaskTexture );
    }
    if( FAILED( CreateTextureNonPackedMips( iDepthMapWidth, 
        iDepthMapHeight, 
        0, 
        0, 
        D3DFMT_DXT3A_1111, 
        0, 
        &m_pMaskTexture, 
        NULL ) ) )
    {
        return E_FAIL;
    }

    if( m_pMaskRenderTarget )
    {
        m_pMaskRenderTarget->Release();
    }
    SurfParams.Base += XGSurfaceSize( iDepthMapWidth, 
        iDepthMapHeight, 
        D3DFMT_R32F, 
        D3DMULTISAMPLE_NONE );
    if( FAILED( m_pd3dDevice->CreateRenderTarget( iDepthMapWidth, 
        iDepthMapHeight,
        D3DFMT_A8B8G8R8, 
        D3DMULTISAMPLE_NONE, 
        0, 
        FALSE, 
        &m_pMaskRenderTarget, 
        &SurfParams ) ) )
    {
        return E_FAIL;
    }

    return S_OK;
}


//-------------------------------------------------------------------------------------------------------------------------------------
// Name: Sample::GenerateGeometryQuad( )
// Desc: Creates vertex and index buffer for a single fullscreen quad 
//-------------------------------------------------------------------------------------------------------------------------------------
VOID Sample::GenerateGeometryQuad( )
{
    IDirect3DVertexBuffer9*& pVB = m_pQuadVB;
    IDirect3DIndexBuffer9*& pIB = m_pQuadIB;
    UINT& iIndexCount = m_iQuadIndexCount;

    // Create a vertex buffer and copy the mesh vertex data into it
    if( FAILED( m_pd3dDevice->CreateVertexBuffer( sizeof( TestGeometryVertex ) * 4, 
        0, 
        0, 
        D3DPOOL_DEFAULT, 
        &pVB, 
        NULL ) ) )
    {
        ATG::FatalError( "Couldn't create vertex buffer\n" );
    }
    TestGeometryVertex* pVBData = NULL;
    pVB->Lock( 0, 0, ( VOID** )&pVBData, 0 );

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

    pVB->Unlock( );

    iIndexCount = 4;

    // Create an index buffer and copy in the mesh index data.
    if( FAILED( m_pd3dDevice->CreateIndexBuffer( iIndexCount * sizeof( WORD ),
        0, 
        D3DFMT_INDEX16, 
        D3DPOOL_DEFAULT,
        &pIB, 
        NULL ) ) )
    {
        ATG::FatalError( "Couldn't create index buffer\n" );
    }

    WORD* pIBData = NULL;
    pIB->Lock( 0, 0, ( VOID** )&pIBData, 0 );
    *pIBData++ = 0;
    *pIBData++ = 2;
    *pIBData++ = 3;
    *pIBData++ = 1;
    pIB->Unlock( );
}


//-------------------------------------------------------------------------------------------------------------------------------------
// Name: Sample::GenerateGeometrySweetSpot( )
// Desc: Creates vertex and index buffer for the simulated Kinect sweetspot
//-------------------------------------------------------------------------------------------------------------------------------------
VOID Sample::GenerateGeometrySweetSpot( UINT iWedges, UINT iSlices )
{
    IDirect3DVertexBuffer9*& pVB = m_pSweetSpotVB;
    IDirect3DIndexBuffer9*& pIB = m_pSweetSpotIB;
    UINT& iIndexCount = m_iSweetSpotIndexCount;
    IDirect3DIndexBuffer9*& pLineIB = m_pSweetSpotLineIB;
    UINT& iLineIndexCount = m_iSweetSpotLineIndexCount;

    // Create a vertex buffer and copy the mesh vertex data into it
    if( FAILED( m_pd3dDevice->CreateVertexBuffer( 
        ( iWedges + 1 ) * ( iSlices + 1 ) * sizeof( TestGeometryVertex ), 
        0, 
        0, 
        D3DPOOL_DEFAULT, 
        &pVB, 
        NULL ) ) )
    {
        ATG::FatalError( "Couldn't create vertex buffer\n" );
    }

    TestGeometryVertex* pVBData = NULL;
    pVB->Lock( 0, 0, ( VOID** )&pVBData, 0 );

    UINT k = 0;
    for( UINT j = 0; j <= iWedges; ++j )
    {
        FLOAT fX = cosf( ( j / (FLOAT) iWedges ) * XM_2PI );
        FLOAT fZ = sinf( ( j / (FLOAT) iWedges ) * XM_2PI );
        FLOAT fU = j / (FLOAT) iWedges;

        for( UINT i = 0; i <= iSlices; ++i )
        {
            FLOAT fY = i / (FLOAT) iSlices;
            FLOAT fV = 0.0f + ( 1.0f - 0.0f ) * ( i / (FLOAT) iSlices );

            pVBData[k].Position.x = fX;
            pVBData[k].Position.y = fY;
            pVBData[k].Position.z = fZ;
            pVBData[k].TexCoord.x = fU;
            pVBData[k].TexCoord.y = fV;
            ++k;
        }
    }

    pVB->Unlock( );

    // Create an index buffer and copy in the mesh index data.
    iIndexCount = iWedges * ( 4 * iSlices );

    if( FAILED( m_pd3dDevice->CreateIndexBuffer( iIndexCount * sizeof( WORD ),
        0, 
        D3DFMT_INDEX16, 
        D3DPOOL_DEFAULT,
        &pIB, 
        NULL ) ) )
    {
        ATG::FatalError( "Couldn't create index buffer\n" );
    }

    WORD* pIBData = NULL;
    pIB->Lock( 0, 0, ( VOID** )&pIBData, 0 );

    k = 0;
    WORD index1 = (WORD) iSlices + 1;
    WORD index2 = 0;
    WORD index3 = index2 + 1;
    WORD index4 = index1 + 1;
    for( UINT j = 0; j < iWedges; ++j )
    {
        for( UINT i = 0; i < iSlices; ++i )
        {
            pIBData[k++] = index1++;
            pIBData[k++] = index2++;
            pIBData[k++] = index3++;
            pIBData[k++] = index4++;
        }

        index1++;
        index2++;
        index3++;
        index4++;
    }

    pIB->Unlock( );

    // Create a second index buffer for wireframe draws
    iLineIndexCount = 2 * ( iWedges + 1 ) * iSlices
        + 2 * iWedges * ( iSlices + 1 );

    if( FAILED( m_pd3dDevice->CreateIndexBuffer( iLineIndexCount * sizeof( WORD ),
        0, 
        D3DFMT_INDEX16, 
        D3DPOOL_DEFAULT,
        &pLineIB, 
        NULL ) ) )
    {
        ATG::FatalError( "Couldn't create index buffer\n" );
    }

    WORD* pLineIBData = NULL;
    pLineIB->Lock( 0, 0, ( VOID** )&pLineIBData, 0 );

    k = 0;
    UINT l = 2 * ( iWedges + 1 ) * iSlices;
    index1 = 0;
    index2 = index1 + 1;
    index3 = 0;
    index4 = index3 + (WORD) iSlices + 1;
    for( UINT j = 0; j <= iWedges; ++j )
    {
        for( UINT i = 0; i <= iSlices; ++i )
        {
            if( i < iSlices )
            {
                pLineIBData[k++] = index1;
                pLineIBData[k++] = index2;
            }
            if( j < iWedges )
            {
                pLineIBData[l++] = index3;
                pLineIBData[l++] = index4;
            }

            index1++;
            index2++;
            index3++;
            index4++;
        }
    }

    pLineIB->Unlock( );
}


//-------------------------------------------------------------------------------------------------------------------------------------
// Name: Sample::GenerateGeometryGrid( )
// Desc: Creates vertex and index buffer for a grid
//-------------------------------------------------------------------------------------------------------------------------------------
VOID Sample::GenerateGeometryGrid( UINT iDivisionsX, UINT iDivisionsY )
{
    IDirect3DVertexBuffer9*& pVB = m_pGridVB;
    IDirect3DIndexBuffer9*& pIB = m_pGridIB;
    UINT& iIndexCount = m_iGridIndexCount;
    IDirect3DIndexBuffer9*& pLineIB = m_pGridLineIB;
    UINT& iLineIndexCount = m_iGridLineIndexCount;

    // Create a vertex buffer and copy the mesh vertex data into it
    if( FAILED( m_pd3dDevice->CreateVertexBuffer( 
        ( iDivisionsX + 1 ) * ( iDivisionsY + 1 ) * sizeof( TestGeometryVertex ), 
        0, 
        0, 
        D3DPOOL_DEFAULT, 
        &pVB, 
        NULL ) ) )
    {
        ATG::FatalError( "Couldn't create vertex buffer\n" );
    }

    TestGeometryVertex* pVBData = NULL;
    pVB->Lock( 0, 0, ( VOID** )&pVBData, 0 );

    UINT k = 0;
    for( UINT j = 0; j <= iDivisionsX; ++j )
    {
        FLOAT fX = 2.0f * j / (FLOAT) iDivisionsX - 1.0f;
        FLOAT fZ = 0.0f;
        FLOAT fU = j / (FLOAT) iDivisionsX;

        for( UINT i = 0; i <= iDivisionsY; ++i )
        {
            FLOAT fY = 2.0f * i / (FLOAT) iDivisionsY - 1.0f;
            FLOAT fV = i / (FLOAT) iDivisionsY;

            pVBData[k].Position.x = fX;
            pVBData[k].Position.y = fY;
            pVBData[k].Position.z = fZ;
            pVBData[k].TexCoord.x = fU;
            pVBData[k].TexCoord.y = fV;
            ++k;
        }
    }

    pVB->Unlock( );

    // Create an index buffer and copy in the mesh index data.
    iIndexCount = iDivisionsX * ( 4 * iDivisionsY );

    if( FAILED( m_pd3dDevice->CreateIndexBuffer( iIndexCount * sizeof( WORD ),
        0, 
        D3DFMT_INDEX16, 
        D3DPOOL_DEFAULT,
        &pIB, 
        NULL ) ) )
    {
        ATG::FatalError( "Couldn't create index buffer\n" );
    }

    WORD* pIBData = NULL;
    pIB->Lock( 0, 0, ( VOID** )&pIBData, 0 );

    k = 0;
    WORD index1 = (WORD) iDivisionsY + 1;
    WORD index2 = 0;
    WORD index3 = index2 + 1;
    WORD index4 = index1 + 1;
    for( UINT j = 0; j < iDivisionsX; ++j )
    {
        for( UINT i = 0; i < iDivisionsY; ++i )
        {
            pIBData[k++] = index1++;
            pIBData[k++] = index2++;
            pIBData[k++] = index3++;
            pIBData[k++] = index4++;
        }

        index1++;
        index2++;
        index3++;
        index4++;
    }

    pIB->Unlock( );

    // Create a second index buffer for wireframe draws
    iLineIndexCount = 2 * ( iDivisionsX + 1 ) * iDivisionsY
        + 2 * iDivisionsX * ( iDivisionsY + 1 );

    if( FAILED( m_pd3dDevice->CreateIndexBuffer( iLineIndexCount * sizeof( WORD ),
        0, 
        D3DFMT_INDEX16, 
        D3DPOOL_DEFAULT,
        &pLineIB, 
        NULL ) ) )
    {
        ATG::FatalError( "Couldn't create index buffer\n" );
    }

    WORD* pLineIBData = NULL;
    pLineIB->Lock( 0, 0, ( VOID** )&pLineIBData, 0 );

    k = 0;
    UINT l = 2 * ( iDivisionsX + 1 ) * iDivisionsY;
    index1 = 0;
    index2 = index1 + 1;
    index3 = 0;
    index4 = index3 + (WORD) iDivisionsY + 1;
    for( UINT j = 0; j <= iDivisionsX; ++j )
    {
        for( UINT i = 0; i <= iDivisionsY; ++i )
        {
            if( i < iDivisionsY )
            {
                pLineIBData[k++] = index1;
                pLineIBData[k++] = index2;
            }
            if( j < iDivisionsX )
            {
                pLineIBData[l++] = index3;
                pLineIBData[l++] = index4;
            }

            index1++;
            index2++;
            index3++;
            index4++;
        }
    }

    pLineIB->Unlock( );
}


//-------------------------------------------------------------------------------------------------------------------------------------
// Name: Sample::GenerateGeometryArrow( )
// Desc: Creates vertex and index buffer for a 3D arrow shape
//-------------------------------------------------------------------------------------------------------------------------------------
VOID Sample::GenerateGeometryArrow( UINT iWedges, UINT iConeSlices, UINT iStalkSlices, FLOAT fConeRadius, 
        FLOAT fConeLength, FLOAT fStalkRadius, FLOAT fStalkLength )
{
    IDirect3DVertexBuffer9*& pVB = m_pArrowVB;
    IDirect3DIndexBuffer9*& pIB = m_pArrowIB;
    UINT& iIndexCount = m_iArrowIndexCount;
    IDirect3DIndexBuffer9*& pLineIB = m_pArrowLineIB;
    UINT& iLineIndexCount = m_iArrowLineIndexCount;

    UINT iSlices = iStalkSlices + iConeSlices + 1; // Implicit slice joins the stalk and cone

    // Create a vertex buffer and copy the mesh vertex data into it
    if( FAILED( m_pd3dDevice->CreateVertexBuffer( 
        ( iWedges + 1 ) * ( iSlices + 1 ) * sizeof( TestGeometryVertex ), 
        0, 
        0, 
        D3DPOOL_DEFAULT, 
        &pVB, 
        NULL ) ) )
    {
        ATG::FatalError( "Couldn't create vertex buffer\n" );
    }

    TestGeometryVertex* pVBData = NULL;
    pVB->Lock( 0, 0, ( VOID** )&pVBData, 0 );

    UINT k = 0;
    for( UINT j = 0; j <= iWedges; ++j )
    {
        for( UINT i = 0; i <= iConeSlices; ++i )
        {
            FLOAT fRadius = fConeRadius * ( i / (FLOAT) iConeSlices );

            FLOAT fX = cosf( ( j / (FLOAT) iWedges ) * XM_2PI ) * fRadius;
            FLOAT fZ = sinf( ( j / (FLOAT) iWedges ) * XM_2PI ) * fRadius;
            FLOAT fU = j / (FLOAT) iWedges;

            FLOAT fY = fConeLength * ( i / (FLOAT) iConeSlices );
            FLOAT fV = 0.0f + ( 1.0f - 0.0f ) * ( i / (FLOAT) iConeSlices );

            pVBData[k].Position.x = fX;
            pVBData[k].Position.y = fY;
            pVBData[k].Position.z = fZ;
            pVBData[k].TexCoord.x = fU;
            pVBData[k].TexCoord.y = fV;
            ++k;
        }

        FLOAT fX = cosf( ( j / (FLOAT) iWedges ) * XM_2PI ) * fStalkRadius;
        FLOAT fZ = sinf( ( j / (FLOAT) iWedges ) * XM_2PI ) * fStalkRadius;
        FLOAT fU = j / (FLOAT) iWedges;

        for( UINT i = 0; i <= iStalkSlices; ++i )
        {
            FLOAT fY = fConeLength + fStalkLength * ( i / (FLOAT) iStalkSlices );
            FLOAT fV = 0.0f + ( 1.0f - 0.0f ) * ( i / (FLOAT) iStalkSlices );

            pVBData[k].Position.x = fX;
            pVBData[k].Position.y = fY;
            pVBData[k].Position.z = fZ;
            pVBData[k].TexCoord.x = fU;
            pVBData[k].TexCoord.y = fV;
            ++k;
        }
    }

    pVB->Unlock( );

    // Create an index buffer and copy in the mesh index data.
    iIndexCount = 4 * iWedges * iSlices;

    if( FAILED( m_pd3dDevice->CreateIndexBuffer( iIndexCount * sizeof( WORD ),
        0, 
        D3DFMT_INDEX16, 
        D3DPOOL_DEFAULT,
        &pIB, 
        NULL ) ) )
    {
        ATG::FatalError( "Couldn't create index buffer\n" );
    }

    WORD* pIBData = NULL;
    pIB->Lock( 0, 0, ( VOID** )&pIBData, 0 );

    k = 0;
    WORD index1 = (WORD) iSlices + 1;
    WORD index2 = 0;
    WORD index3 = index2 + 1;
    WORD index4 = index1 + 1;
    for( UINT j = 0; j < iWedges; ++j )
    {
        for( UINT i = 0; i < iSlices; ++i )
        {
            pIBData[k++] = index1++;
            pIBData[k++] = index2++;
            pIBData[k++] = index3++;
            pIBData[k++] = index4++;
        }

        index1++;
        index2++;
        index3++;
        index4++;
    }

    pIB->Unlock( );

    // Create a second index buffer for wireframe draws
    // The arrows look better if we don't draw the back-side edges in wireframe
    UINT iFrontFacingWedges = iWedges / 2;
    UINT iStartingIndex = iFrontFacingWedges * ( iSlices + 1 );

    iLineIndexCount = 2 * ( iFrontFacingWedges + 1 ) * iSlices
        + 2 * iFrontFacingWedges * ( iSlices + 1 );

    if( FAILED( m_pd3dDevice->CreateIndexBuffer( iLineIndexCount * sizeof( WORD ),
        0, 
        D3DFMT_INDEX16, 
        D3DPOOL_DEFAULT,
        &pLineIB, 
        NULL ) ) )
    {
        ATG::FatalError( "Couldn't create index buffer\n" );
    }

    WORD* pLineIBData = NULL;
    pLineIB->Lock( 0, 0, ( VOID** )&pLineIBData, 0 );

    k = 0;
    UINT l = 2 * ( iFrontFacingWedges + 1 ) * iSlices;
    index1 = (WORD) iStartingIndex;
    index2 = index1 + 1;
    index3 = (WORD) iStartingIndex;
    index4 = index3 + (WORD) iSlices + 1;
    for( UINT j = 0; j <= iFrontFacingWedges; ++j )
    {
        for( UINT i = 0; i <= iSlices; ++i )
        {
            if( i < iSlices )
            {
                pLineIBData[k++] = index1;
                pLineIBData[k++] = index2;
            }
            if( j < iFrontFacingWedges )
            {
                pLineIBData[l++] = index3;
                pLineIBData[l++] = index4;
            }

            index1++;
            index2++;
            index3++;
            index4++;
        }
    }

    pLineIB->Unlock( );
}


//-------------------------------------------------------------------------------------------------------------------------------------
// Name: Sample::StartupCamera( )
// Desc: Typical camera init (depth only --- no color, segmentation or skeletal tracking)
//-------------------------------------------------------------------------------------------------------------------------------------
HRESULT Sample::StartupCamera()
{
    // Initializes the Natural Input system on the default thread.  Disables the nui guide, because this sample 
    // does not use skeletal tracking, and the nui guide forces skeletal tracking on.
    HRESULT hr = NuiInitialize( NUI_INITIALIZE_FLAG_USES_DEPTH | NUI_INITIALIZE_FLAG_NUI_GUIDE_DISABLED,
        NUI_INITIALIZE_DEFAULT_HARDWARE_THREAD );
    if ( FAILED(hr) )
    {
        ATG_PrintError( "NuiInitialize failed\n" );
        return E_FAIL;
    }

    // Open the depth stream, double-buffered
    hr = NuiImageStreamOpen( 
        NUI_IMAGE_TYPE_DEPTH, 
        NUI_IMAGE_RESOLUTION_320x240, 
        0, 
        2, 
        NULL, 
        &m_hDepth );
    if ( FAILED (hr) )
    {
        ATG_PrintError ( "NuiImageStreamOpen failed on depth" );
        return E_FAIL;
    }

    return hr;
}


//------------------------------------------------------------------------------------------------------------------
// Name: CreateTextureNonPackedMips()
// Desc: Attempt at a drop-in replacement for CreateTexture which doesn't use packed mips.
// This is required if you intend to Resolve into a mip level.
//------------------------------------------------------------------------------------------------------------------
HRESULT Sample::CreateTextureNonPackedMips( UINT Width,
                                            UINT Height,
                                            UINT Levels,
                                            DWORD Usage,
                                            D3DFORMAT Format,
                                            D3DPOOL UnusedPool,
                                            IDirect3DTexture9 **ppTexture,
                                            HANDLE *pUnusedSharedHandle )
{
    *ppTexture = new IDirect3DTexture9;

    DWORD dwTextureSize = XGSetTextureHeaderEx( Width,
        Height,
        0,  // full mips for downsampling
        0,
        Format,
        0,
        XGHEADEREX_NONPACKED,   // non-packed
        0,
        XGHEADER_CONTIGUOUS_MIP_OFFSET, // single allocation for base and mip
        0,
        *ppTexture,
        NULL,
        NULL );

    VOID* pBuffer = XPhysicalAlloc( dwTextureSize, MAXULONG_PTR, 0,
        PAGE_READONLY | PAGE_NOCACHE );

    XGOffsetResourceAddress( *ppTexture, pBuffer );

    return S_OK;
}


//------------------------------------------------------------------------------------------------------------------
// Name: ReleaseTextureNonPackedMips()
// Desc: The corresponding Release call.
//------------------------------------------------------------------------------------------------------------------
HRESULT Sample::ReleaseTextureNonPackedMips( IDirect3DTexture9 *pTexture )
{
    assert( pTexture );

    XPhysicalFree( (VOID*) ( pTexture->Format.BaseAddress << GPU_TEXTURE_ADDRESS_SHIFT ) );

    delete pTexture;

    return S_OK;
}


//---------------------------------------------------------------------------------------------------------
// Name: Update()
// Desc: Animate the scene.
//---------------------------------------------------------------------------------------------------------
HRESULT Sample::Update()
{
    // Get the current gamepad status
    ATG::GAMEPAD* pGamepad = ATG::Input::GetMergedInput();

    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_BACK )
    {
        m_bDrawHelp = !m_bDrawHelp;
    }

    if( !m_bDrawHelp )
    {
        // Record state prior to applying controller input
        static BOOL g_bFirstUpdate = TRUE;

        BOOL bOldLowResDepth                = m_LowResDepthParam.GetValue();
        BOOL bOldLowResNormal               = m_LowResNormalParam.GetValue();
        BOOL bOldLowPrecisionNormal         = m_LowPrecisionNormalParam.GetValue();

        // Handle options
        if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_START )
        {
            m_bPause = !m_bPause;
        }

        if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_B )
        {
            m_bBigMenu = !m_bBigMenu;
        }

        // Manipulate sweetspot & floorplane
        if( pGamepad->wLastButtons & XINPUT_GAMEPAD_RIGHT_THUMB )
        {
            FLOAT fStickSensitivity = 10.0f;
            if( pGamepad->fY2 < -0.1f )
            {
                m_fFloorPlaneHeight -= fStickSensitivity;
            }
            else if( pGamepad->fY2 > 0.1f ) 
            {
                m_fFloorPlaneHeight += fStickSensitivity;
            }
        }
        else
        {
            FLOAT fStickSensitivity = 10.0f;
            if( pGamepad->fX2 < -0.1f )
            {
                m_fSweetSpotCenterX -= fStickSensitivity;
            }
            else if( pGamepad->fX2 > 0.1f ) 
            {
                m_fSweetSpotCenterX += fStickSensitivity;
            }
            if( pGamepad->fY2 < -0.1f )
            {
                m_fSweetSpotCenterZ -= fStickSensitivity;
            }
            else if( pGamepad->fY2 > 0.1f ) 
            {
                m_fSweetSpotCenterZ += fStickSensitivity;
            }
        }

        FLOAT fRadiusSensitivity = 10.0f;
        if( pGamepad->bLastLeftTrigger ) 
        {
            m_fSweetSpotRadius -= fRadiusSensitivity;
        }
        if( pGamepad->bLastRightTrigger ) 
        {
            m_fSweetSpotRadius += fRadiusSensitivity;
        }

        static FLOAT g_fLastX1 = 0.0f, g_fLastY1 = 0.0f;
        FLOAT fDecrease = 0.0f;
        FLOAT fIncrease = 0.0f;

        // Process UI Input
        if( ( pGamepad->wPressedButtons & XINPUT_GAMEPAD_DPAD_UP ) 
            || ( pGamepad->fY1 > 0.1f && g_fLastY1 <= 0.1f ) )
        {
            m_iActiveUIParameter += UI_PARAM_COUNT - 1;
            m_iActiveUIParameter %= UI_PARAM_COUNT;
        }
        if( ( pGamepad->wPressedButtons & XINPUT_GAMEPAD_DPAD_DOWN ) 
            || ( pGamepad->fY1 < -0.1f && g_fLastY1 >= -0.1f ) )
        {
            m_iActiveUIParameter += 1;
            m_iActiveUIParameter %= UI_PARAM_COUNT;
        }

        // Keep the selected parameter in view, in the small menu
        if( m_iVisibleUICount < UI_PARAM_COUNT )
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

        if( pGamepad->fX1 < -0.1f && g_fLastX1 >= -0.1f )
            fDecrease = 1.0f;
        if( pGamepad->fX1 > 0.1f && g_fLastX1 <= 0.1f )
            fIncrease = 1.0f;
        if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_DPAD_LEFT )
            fDecrease = 1.0f;
        if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_DPAD_RIGHT )
            fIncrease = 1.0f;

        // Correct for bias in thumbsticks, and limit to one move at a time
        g_fLastX1 = pGamepad->fX1;
        g_fLastY1 = pGamepad->fY1;

        if( fDecrease > 0.0f )
        {
            m_UIParamArray[m_iActiveUIParameter]->DecreaseValue( fDecrease );
        }
        if( fIncrease > 0.0f )
        {
            m_UIParamArray[m_iActiveUIParameter]->IncreaseValue( fIncrease );
        }

        // Read camera angle, to obtain world-to-camera transform.
        XMVECTOR xmvNormal;
        NuiCameraGetNormalToGravity( &xmvNormal );
        m_fCameraElevationRadians = atan2f(xmvNormal.z, xmvNormal.y);

        BOOL bNewLowResDepth                = m_LowResDepthParam.GetValue();
        BOOL bNewLowResNormal               = m_LowResNormalParam.GetValue();
        BOOL bNewLowPrecisionNormal         = m_LowPrecisionNormalParam.GetValue();

        // These options change the size of the working resources, requiring reallocations
        if( g_bFirstUpdate 
            || bOldLowResDepth          != bNewLowResDepth 
            || bOldLowResNormal         != bNewLowResNormal 
            || bOldLowPrecisionNormal   != bNewLowPrecisionNormal 
            )
        {
            CreateTexturesAndRenderTargets();
        }

        g_bFirstUpdate = FALSE;
    }

    return S_OK;
}


//---------------------------------------------------------------------------------------------------------
// Name: Render()
// Desc: Render the scene.
//---------------------------------------------------------------------------------------------------------
HRESULT Sample::Render()
{
    // Retrieve UI settings
    CONST BOOL bLowResDepth                 = m_LowResDepthParam.GetValue();
    CONST BOOL b16bppNormal                 = m_LowPrecisionNormalParam.GetValue();
    CONST BOOL bSmallEffectViewport         = m_SmallEffectViewportParam.GetValue();
    CONST BOOL bHideBorder                  = m_HideBorderParam.GetValue();
    CONST BOOL bDrawThumbnails              = m_DrawThumbnailsParam.GetValue();
    CONST BOOL bFillIRShadow                = m_FillIRShadowParam.GetValue();
    CONST BOOL bFillSmallHoles              = m_FillSmallHolesParam.GetValue();
    CONST BOOL bEraseSmallIslands           = m_EraseSmallIslandsParam.GetValue();
    CONST UINT iFilterDepthCount            = m_FilterDepthCountParam.GetValue();
    CONST UINT iFilterDepthType             = m_FilterDepthTypeParam.GetValue();
    CONST FLOAT fBilateralEdgeBlurDepth     = m_BilateralEdgeBlurDepthParam.GetValue();
    CONST UINT iFilterNormalCount           = m_FilterNormalCountParam.GetValue();
    CONST UINT iFilterNormalType            = m_FilterNormalTypeParam.GetValue();
    CONST FLOAT fBilateralEdgeBlurNormal    = m_BilateralEdgeBlurNormalParam.GetValue();
    CONST BOOL bFadeNormal                  = m_FadeNormalParam.GetValue();
    CONST FLOAT fFadeNormalScale            = m_FadeNormalScaleParam.GetValue();
    CONST BOOL bRenormalizeNormal           = m_RenormalizeNormalParam.GetValue();
    CONST BOOL bDrawFloorPlane              = m_DrawFloorPlaneParam.GetValue();

    // Track which is the current working depth image (the original, or a processed copy)
    IDirect3DTexture9* pDepthTexture = NULL;

    // Default states 
    m_pd3dDevice->SetRenderState( D3DRS_HALFPIXELOFFSET, TRUE );
    m_pd3dDevice->SetSamplerAddressStates( 0, D3DTADDRESS_CLAMP, D3DTADDRESS_CLAMP, D3DTADDRESS_CLAMP );
    m_pd3dDevice->SetSamplerAddressStates( 1, D3DTADDRESS_CLAMP, D3DTADDRESS_CLAMP, D3DTADDRESS_CLAMP );
    m_pd3dDevice->SetSamplerAddressStates( 2, D3DTADDRESS_CLAMP, D3DTADDRESS_CLAMP, D3DTADDRESS_CLAMP );
    m_pd3dDevice->SetSamplerAddressStates( D3DVERTEXTEXTURESAMPLER0 + 0, D3DTADDRESS_CLAMP, D3DTADDRESS_CLAMP, D3DTADDRESS_CLAMP );
    m_pd3dDevice->SetSamplerAddressStates( D3DVERTEXTEXTURESAMPLER0 + 1, D3DTADDRESS_CLAMP, D3DTADDRESS_CLAMP, D3DTADDRESS_CLAMP );

    // Bias to pixel shading, since most of our work is pixel-shader-heavy
    m_pd3dDevice->SetShaderGPRAllocation( 0, 16, GPU_GPRS - 16 );

    // Clear the viewport
    m_pd3dDevice->Clear( 0L, NULL, D3DCLEAR_TARGET0 | D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL, 0x00000000, 1.0f, 0L );

    // Get data from the next camera depth frame
    static CONST NUI_IMAGE_FRAME* pDepthFrame = NULL;
    CONST NUI_IMAGE_FRAME* pPrevDepthFrame = pDepthFrame;
    HRESULT hrDepth = S_OK;
    if( !m_bPause )
    {
        // We could use the event method to acquire Nui data.  However, since we only need the depth
        // map, and we're only running one thread, there would be no advantage to this method.
        //
        // After this call, we briefly have two frames out at once.
        hrDepth = NuiImageStreamGetNextFrame( m_hDepth, NUI_FRAME_END_TIMEOUT_DEFAULT, &pDepthFrame );
    }

    // If we didn't get a new frame somehow, use the old one. 
    assert( !( FAILED( hrDepth ) ^ !pDepthFrame ) );  // these should be consistent
    if( FAILED( hrDepth ) || !pDepthFrame )
    {
        pDepthFrame = pPrevDepthFrame;
    }

    // If we still don't have a frame, give up.  
    if( pDepthFrame )
    {
        pDepthFrame->pFrameTexture->Format.ExpAdjust = -3;   // Get rid of the 3 segmentation bits 
        pDepthTexture = pDepthFrame->pFrameTexture;
    }   

    // If we have depth data --- even stale data --- continue
    if( pDepthTexture )
    {
        // Optionally downsample depth (for perf)
        if( bLowResDepth )
        {
             DownsampleDepth( pDepthTexture );
        }

        // Adjustments to depth map.  The mask texture contains bits for:
        //   CHANNEL_SHOULD_BE_VALID      = pixel should be filled with a valid depth, if possible
        //   CHANNEL_IS_IR_SHADOW         = pixel is in IRShadow
        //   CHANNEL_IS_SMALL_HOLE        = pixel is in a small hole of invalid depth mostly surrounded by valid depths
        //   CHANNEL_IS_SMALL_ISLAND      = pixel is in a small island of valid depth mostly surrounded by invalid depths
        // We start out using the depth texture itself as the mask, until someone fills in data.
        IDirect3DTexture9 AliasDepthAsMaskTexture;
        IDirect3DTexture9* pMaskTexture = AliasDepthAsMask( pDepthTexture, &AliasDepthAsMaskTexture );
        if( bEraseSmallIslands )
        {
            MarkSmallHoles( pDepthTexture, pMaskTexture, FALSE, bLowResDepth ); // FALSE means "erase small islands"
        }
        if( bFillIRShadow )
        {
            MarkIRShadow( pDepthTexture, pMaskTexture );
        }
        if( bFillSmallHoles )
        {
            MarkSmallHoles( pDepthTexture, pMaskTexture, TRUE, bLowResDepth );  // TRUE means "fill small holes"
        }

        // If one of the previous tests was true, this pass will erase valid pixels and/or fill in invalid pixels.
        // Otherwise, all it does is set invalid pixels to the far depth (we run it in this case just for consistency).
        FillInDepth( pDepthTexture, pMaskTexture ); 

        // Optionally filter depth values (for noise reduction)
        if( iFilterDepthCount > 0 )
        {
            FilterDepth( pDepthTexture, iFilterDepthCount, iFilterDepthType, fBilateralEdgeBlurDepth );
        }

        // Sample depths to estimate normals
        InferNormalMap( pDepthTexture, b16bppNormal );

        // Optionally filter normal values (for noise reduction)
        if( iFilterNormalCount > 0 )
        {
            FilterNormal( iFilterNormalCount, iFilterNormalType, fBilateralEdgeBlurNormal, b16bppNormal );
        }

        // Optionally create mips for normal map
        if( bFadeNormal )
        {
            DownsampleNormal( b16bppNormal );
        }

        // Draw the visualization
        VisualizeDepth( pDepthTexture, bSmallEffectViewport, bHideBorder, bFadeNormal, fFadeNormalScale, bRenormalizeNormal, bDrawFloorPlane );

        // Optionally draw the pip thumbnails
        if( bDrawThumbnails )
        {
            DrawThumbnails( pDepthTexture, pMaskTexture, bRenormalizeNormal );
        }
    }

    // Show title, frame rate, and help
    m_Timer.MarkFrame();
    RenderUI( );

    m_pd3dDevice->UnsetAll();

    // Restore the GPR allocations to the defaults. This must be done before
    // calling Present or Swap.
    m_pd3dDevice->SetShaderGPRAllocation( 0, 0, 0 );

    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    // At this point, the previous depth frame is guaranteed to no longer be in use by the GPU, 
    // even with D3DCREATE_BUFFER_2_FRAMES.
    if( !m_bPause && SUCCEEDED( hrDepth ) && pPrevDepthFrame )
    {
        NuiImageStreamReleaseFrame( m_hDepth, pPrevDepthFrame );
    }

    return S_OK;
}


//-------------------------------------------------------------------------------------------------------------------------------------
// Name: Sample::DrawQuad( )
// Desc: 
//-------------------------------------------------------------------------------------------------------------------------------------
VOID Sample::DrawQuad( IDirect3DPixelShader9* pPixelShader )
{
    // Make sure that the required shaders and objects exist
    assert( pPixelShader );

    // Set all the right D3D state and resources
    m_pd3dDevice->SetIndices( m_pQuadIB );
    m_pd3dDevice->SetStreamSource( 0, m_pQuadVB, 0, sizeof( TestGeometryVertex ) );
    m_pd3dDevice->SetVertexDeclaration( m_pVertexDecl );
    m_pd3dDevice->SetVertexShader( m_pVertexShader );

    XMMATRIX matProj = XMMatrixOrthographicLH( 2.0f, 2.0f, 0.0f, 1.0f );
    XMMATRIX matWVP = matProj;
    XMMATRIX matWVPT = XMMatrixTranspose( matWVP );
    m_pd3dDevice->SetVertexShaderConstantF( reg_Matrix, ( FLOAT* )&matWVPT, 4 );

    m_pd3dDevice->SetPixelShader( pPixelShader );

    // Draw the geometry
    m_pd3dDevice->DrawIndexedPrimitive( D3DPT_QUADLIST, 0, 0, 0, 0, m_iQuadIndexCount / 4 );
}


//-------------------------------------------------------------------------------------------------------------------------------------
// Name: Sample::DrawSweetSpot( )
// Desc: 
//-------------------------------------------------------------------------------------------------------------------------------------
VOID Sample::DrawSweetSpot( IDirect3DPixelShader9* pPixelShader, const XMMATRIX& matWorld )
{
    // Make sure that the required shaders and objects exist
    assert( pPixelShader );

    // Set all the right D3D state and resources
    m_pd3dDevice->SetIndices( m_pSweetSpotIB );
    m_pd3dDevice->SetStreamSource( 0, m_pSweetSpotVB, 0, sizeof( TestGeometryVertex ) );
    m_pd3dDevice->SetVertexDeclaration( m_pVertexDecl );
    m_pd3dDevice->SetVertexShader( m_pVertexShader );

    FLOAT FovAngleY = XMConvertToRadians( m_fNuiVerticalFOV );
    FLOAT AspectRatio = m_iDepthMapWidth / (FLOAT) m_iDepthMapHeight;
    FLOAT NearZ = m_fNearPlaneDistance;
    FLOAT FarZ = m_fFarPlaneDistance;
    XMMATRIX matProj = XMMatrixPerspectiveFovLH( FovAngleY, AspectRatio, NearZ, FarZ );
    XMMATRIX matWVP = matWorld * matProj;
    XMMATRIX matWVPT = XMMatrixTranspose( matWVP );
    m_pd3dDevice->SetVertexShaderConstantF( reg_Matrix, ( FLOAT* )&matWVPT, 4 );

    m_pd3dDevice->SetPixelShader( pPixelShader );

    // Draw the geometry
    m_pd3dDevice->DrawIndexedPrimitive( D3DPT_QUADLIST, 0, 0, 0, 0, m_iSweetSpotIndexCount / 4 );

    // Draw the wireframe
    m_pd3dDevice->SetIndices( m_pSweetSpotLineIB );
    m_pd3dDevice->SetRenderState( D3DRS_LINEWIDTH, ATG::FtoDW( 2.0f ) );
    m_pd3dDevice->DrawIndexedPrimitive( D3DPT_LINELIST, 0, 0, 0, 0, m_iSweetSpotLineIndexCount / 2 );
}


//-------------------------------------------------------------------------------------------------------------------------------------
// Name: Sample::DrawGrid( )
// Desc: 
//-------------------------------------------------------------------------------------------------------------------------------------
VOID Sample::DrawGrid( IDirect3DPixelShader9* pPixelShader, const XMMATRIX& matWorld )
{
    // Make sure that the required shaders and objects exist
    assert( pPixelShader );

    // Set all the right D3D state and resources
    m_pd3dDevice->SetIndices( m_pGridIB );
    m_pd3dDevice->SetStreamSource( 0, m_pGridVB, 0, sizeof( TestGeometryVertex ) );
    m_pd3dDevice->SetVertexDeclaration( m_pVertexDecl );
    m_pd3dDevice->SetVertexShader( m_pVertexShader );

    FLOAT FovAngleY = XMConvertToRadians( m_fNuiVerticalFOV );
    FLOAT AspectRatio = m_iDepthMapWidth / (FLOAT) m_iDepthMapHeight;
    FLOAT NearZ = m_fNearPlaneDistance;
    FLOAT FarZ = m_fFarPlaneDistance;
    XMMATRIX matProj = XMMatrixPerspectiveFovLH( FovAngleY, AspectRatio, NearZ, FarZ );
    XMMATRIX matWVP = matWorld * matProj;
    XMMATRIX matWVPT = XMMatrixTranspose( matWVP );
    m_pd3dDevice->SetVertexShaderConstantF( reg_Matrix, ( FLOAT* )&matWVPT, 4 );

    m_pd3dDevice->SetPixelShader( pPixelShader );

    // Draw the geometry
    m_pd3dDevice->DrawIndexedPrimitive( D3DPT_QUADLIST, 0, 0, 0, 0, m_iGridIndexCount / 4 );

    // Draw the wireframe
    m_pd3dDevice->SetIndices( m_pGridLineIB );
    m_pd3dDevice->SetRenderState( D3DRS_LINEWIDTH, ATG::FtoDW( 2.0f ) );
    m_pd3dDevice->DrawIndexedPrimitive( D3DPT_LINELIST, 0, 0, 0, 0, m_iGridLineIndexCount / 2 );
}


//-------------------------------------------------------------------------------------------------------------------------------------
// Name: Sample::DrawArrow( )
// Desc: 
//-------------------------------------------------------------------------------------------------------------------------------------
VOID Sample::DrawArrow( IDirect3DPixelShader9* pPixelShader, const XMMATRIX& matWorld )
{
    // Make sure that the required shaders and objects exist
    assert( pPixelShader );

    // Set all the right D3D state and resources
    m_pd3dDevice->SetIndices( m_pArrowIB );
    m_pd3dDevice->SetStreamSource( 0, m_pArrowVB, 0, sizeof( TestGeometryVertex ) );
    m_pd3dDevice->SetVertexDeclaration( m_pVertexDecl );
    m_pd3dDevice->SetVertexShader( m_pVertexShader );

    FLOAT FovAngleY = XMConvertToRadians( m_fNuiVerticalFOV );
    FLOAT AspectRatio = m_iDepthMapWidth / (FLOAT) m_iDepthMapHeight;
    FLOAT NearZ = m_fNearPlaneDistance;
    FLOAT FarZ = m_fFarPlaneDistance;
    XMMATRIX matProj = XMMatrixPerspectiveFovLH( FovAngleY, AspectRatio, NearZ, FarZ );
    XMMATRIX matWVP = matWorld * matProj;
    XMMATRIX matWVPT = XMMatrixTranspose( matWVP );
    m_pd3dDevice->SetVertexShaderConstantF( reg_Matrix, ( FLOAT* )&matWVPT, 4 );

    m_pd3dDevice->SetPixelShader( pPixelShader );

    // Draw the geometry
    m_pd3dDevice->DrawIndexedPrimitive( D3DPT_QUADLIST, 0, 0, 0, 0, m_iArrowIndexCount / 4 );

    // Draw the wireframe
    m_pd3dDevice->SetIndices( m_pArrowLineIB );
    m_pd3dDevice->SetRenderState( D3DRS_LINEWIDTH, ATG::FtoDW( 2.0f ) );
    m_pd3dDevice->DrawIndexedPrimitive( D3DPT_LINELIST, 0, 0, 0, 0, m_iArrowLineIndexCount / 2 );
}


//-------------------------------------------------------------------------------------------------------------------------------------
// Name: Sample::RenderUI( )
// Desc: Render the screen display for our custom menu.
//-------------------------------------------------------------------------------------------------------------------------------------
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
        m_Font.DrawText( 0.0f, 0.0f, 0xffff00ff, L"Depth Visualizer" );

        FLOAT fParamX = 0.0f;
        FLOAT fParamY = 40.0f;
        FLOAT fParamYInc = 30.0f;

        BOOL bBigMenu = m_bBigMenu || UI_PARAM_COUNT <= m_iVisibleUICount;

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

        // Pip captions
        BOOL bDrawThumbnails = m_DrawThumbnailsParam.GetValue();
        if( bDrawThumbnails )
        {
            const FLOAT drawWidth = m_iDepthMapWidth;
            const FLOAT drawHeight = drawWidth * 3.0f / 4.0f;
            const FLOAT textHeight = 20.0f;
            const FLOAT separatorHeight = 10.0f;
            const FLOAT fontMarginX = 64.0f;
            const FLOAT fontMarginY = 8.0f;
            FLOAT drawX = m_d3dpp.BackBufferWidth - 50.0f - drawWidth - fontMarginX;
            FLOAT drawY = m_d3dpp.BackBufferHeight - 40.0f - 2.0f * drawHeight - 2.0f * separatorHeight - 2.0f * textHeight - fontMarginY;
            drawY += 0.3f * separatorHeight;

            m_Font.SetScaleFactors( 1.0f, 1.0f );

            BOOL bColorCode = m_ColorCodeThumbnailsParam.GetValue();
            if( bColorCode )
            {
                const FLOAT colorCodeHeight = 15.0f;
                const FLOAT colorCodeSeparator = 5.0f;
                const FLOAT colorCodeIndent = 50.0f;
                drawY -= COLOR_CODE_COUNT * ( colorCodeHeight + colorCodeSeparator );
                m_Font.DrawText( drawX + 40.0f, drawY, 0xffffffff, L"Processed depth map" );
                drawY += textHeight;

                m_Font.SetScaleFactors( 0.8f, 0.8f );
                for( UINT i = 0; i < COLOR_CODE_COUNT; ++i )
                {
                    drawY += colorCodeSeparator;
                    CONST D3DCOLORVALUE& CV = g_vColorCodeColors[i];
                    D3DCOLOR Color = D3DCOLOR_COLORVALUE( CV.r, CV.g, CV.b, CV.a );
                    m_Font.DrawText( drawX + 40.0f, drawY, Color, GLYPH_BULLET GLYPH_RIGHT_ARROW );
                    m_Font.DrawText( drawX + colorCodeIndent + 40.0f, drawY, Color, g_strColorCodeNames[i] );
                    drawY += colorCodeHeight;
                }
                drawY += separatorHeight;
            }
            else
            {
                m_Font.DrawText( drawX + 40.0f, drawY, 0xffffffff, L"Processed depth map" );
                drawY += textHeight;
                drawY += separatorHeight;
            }

            drawY += drawHeight;

            m_Font.SetScaleFactors( 1.0f, 1.0f );
            m_Font.DrawText( drawX + 40.0f, drawY, 0xffffffff, L"Inferred normal map" );
            drawY += textHeight;
            drawY += separatorHeight;
        }

        m_Font.End( );
    }

    PIXEndNamedEvent();
}


//---------------------------------------------------------------------------------------------------------
// Name: DownsampleDepth()
// Desc: Downsample the depth to half size.
//---------------------------------------------------------------------------------------------------------
VOID Sample::DownsampleDepth( IDirect3DTexture9*& pDepthTexture )
{
    PIXBeginNamedEvent( 0, "Downsample depth" );

    IDirect3DSurface9* pOldRenderTarget;
    m_pd3dDevice->GetRenderTarget( 0, &pOldRenderTarget );

    m_pd3dDevice->SetRenderTarget( 0, m_pDepthRenderTarget );

    m_pd3dDevice->SetTexture( reg_depth, pDepthTexture );

    DrawQuad( m_pDownsampleDepthPixelShader );

    // Unset texture, because we are about to resolve to it
    m_pd3dDevice->SetTexture( reg_depth, NULL );

    // Now our copy becomes the source for future operations
    pDepthTexture = m_pDepthTiledTexture;

    m_pd3dDevice->Resolve( D3DRESOLVE_RENDERTARGET0, NULL, pDepthTexture, NULL, 0, 0, NULL, 0.0f, 0, NULL );

    m_pd3dDevice->SetRenderTarget( 0, pOldRenderTarget );

    PIXEndNamedEvent();
}


//---------------------------------------------------------------------------------------------------------
// Name: AliasDepthAsMask()
// Desc: Create a texture header, pointing to the bits of the depth map, but usable as if it were the mask
// texture.  
//---------------------------------------------------------------------------------------------------------
IDirect3DTexture9* Sample::AliasDepthAsMask( IDirect3DTexture9* pDepthTexture, IDirect3DTexture9* pAliasTexture )
{
    XGTEXTURE_DESC DepthDesc;
    XGGetTextureDesc( pDepthTexture, 0, &DepthDesc );
    D3DFORMAT DepthFormat = DepthDesc.Format;

    // This will set the SHOULD_BE_VALID channel when the depth is non-zero.  It will set all 
    // other channels to 0 unconditionally.
    D3DFORMAT AliasDepthAsMaskFormat = (D3DFORMAT) ( ( DepthFormat & ~D3DFORMAT_SWIZZLE_MASK ) 
        | ( GPUSWIZZLE_SHOULD_BE_VALID_ZZZ << D3DFORMAT_SWIZZLEX_SHIFT ) );

    // If we don't use the same MipAddress and Levels as the original texture, the GPU can hang
    XGSetTextureHeader( DepthDesc.Width, 
        DepthDesc.Height, 
        0, 
        0, 
        AliasDepthAsMaskFormat,  // Fetches data to .x, fetches 0 to .yzw 
        0, 
        pDepthTexture->Format.BaseAddress << GPU_TEXTURE_ADDRESS_SHIFT, 
        pDepthTexture->Format.MipAddress << GPU_TEXTURE_ADDRESS_SHIFT, 
        0, 
        pAliasTexture, 
        NULL, 
        NULL );

    return pAliasTexture;
}


//---------------------------------------------------------------------------------------------------------
// Name: MarkIRShadow()
// Desc: Marks all pixels with missing depth attributable to IR shadow.
// 
// We want to fill in missing areas of the depth map.  A common way for depths to be 
// missing is for them to be occluded from the laser, but visible to the depth sensor.
//
// We need to distinguish occluded pixels from mere 'empty' pixels, where no object exists.
// To make this distinction, we run a screenspace shadowing algorithm using the valid depths,
// producing a mask of all pixels which might possibly be occluded by objects visible to the 
// sensor.  
//---------------------------------------------------------------------------------------------------------
VOID Sample::MarkIRShadow( IDirect3DTexture9* pDepthTexture, IDirect3DTexture9*& pMaskTexture )
{
    PIXBeginNamedEvent( 0, "Mark IR shadow" );

    // We need to set a realistic far depth in order to limit the amount of IR shadow coverage.
    // If the sensor could detect depths from zero out to infinity, then any object could cast 
    // an arbitrarily wide shadow on the depth image.
    FLOAT fTwiceTanHalfHorizFOV = 2.0f * tan( XMConvertToRadians( m_fNuiHorizontalFOV / 2.0f ) );
    FLOAT fLaserToSensorOffsetTexcoord = m_fLaserToSensorOffsetWorld / ( m_fRealisticFarDepth * fTwiceTanHalfHorizFOV );

    // Create a shadow map by rendering point sprites from every depth pixel (at 1/2 dimensions)
    {
        PIXBeginNamedEvent( 0, "Generate shadow map" );

        IDirect3DSurface9* pOldRenderTarget;
        m_pd3dDevice->GetRenderTarget( 0, &pOldRenderTarget );
        IDirect3DSurface9* pOldDepthStencilSurface;
        m_pd3dDevice->GetDepthStencilSurface( &pOldDepthStencilSurface );

        XGTEXTURE_DESC IRShadowDesc;
        XGGetTextureDesc( m_pIRShadowTexture, 0, &IRShadowDesc );

        FLOAT vTexDims[] = { (FLOAT) IRShadowDesc.Width, (FLOAT) IRShadowDesc.Height, 1.0f / (FLOAT) IRShadowDesc.Width, 1.0f / (FLOAT) IRShadowDesc.Height, };
        m_pd3dDevice->SetVertexShaderConstantF( reg_TexDims, vTexDims, 1 );

        XMVECTOR vTwiceTanHalfFOV = { 
            2.0f * tan(  XMConvertToRadians( m_fNuiHorizontalFOV / 2.0f ) ), 
            2.0f * tan( -XMConvertToRadians( m_fNuiVerticalFOV   / 2.0f ) ), 
        };
        m_pd3dDevice->SetVertexShaderConstantF( reg_TwiceTanHalfFOV, ( FLOAT* )&vTwiceTanHalfFOV, 1 );

        XMVECTOR vLaserToSensorOffsetWorld = { m_fLaserToSensorOffsetWorld, 0.0f, 0.0f, };
        m_pd3dDevice->SetVertexShaderConstantF( reg_LaserToSensorOffsetWorld, ( FLOAT* )&vLaserToSensorOffsetWorld, 1 );

        XMVECTOR vLaserToSensorOffsetTexcoord = { fLaserToSensorOffsetTexcoord, 0.0f, 0.0f, };
        m_pd3dDevice->SetVertexShaderConstantF( reg_LaserToSensorOffsetTexcoord, ( FLOAT* )&vLaserToSensorOffsetTexcoord, 1 );

        m_pd3dDevice->SetVertexShader( m_pIRShadowGenerateVertexShader );
        m_pd3dDevice->SetTexture( D3DVERTEXTEXTURESAMPLER0 + reg_depth, pDepthTexture );
        m_pd3dDevice->SetTexture( D3DVERTEXTEXTURESAMPLER0 + reg_mask, pMaskTexture );

        m_pd3dDevice->SetPixelShader( NULL );

        m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, TRUE );
        m_pd3dDevice->SetRenderState( D3DRS_ZWRITEENABLE, TRUE );

        m_pd3dDevice->SetRenderTarget( 0, NULL );
        m_pd3dDevice->SetDepthStencilSurface( m_pIRShadowDepthStencilSurface );

        m_pd3dDevice->Clear( 0, NULL, D3DCLEAR_ZBUFFER, 0, 1.0f, 0 );
        m_pd3dDevice->DrawPrimitive( D3DPT_POINTLIST, 0, IRShadowDesc.Width * IRShadowDesc.Height );

        m_pd3dDevice->Resolve( D3DRESOLVE_DEPTHSTENCIL, NULL, m_pIRShadowTexture, NULL, 0, 0, NULL, 0.0f, 0, NULL );

        m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, FALSE );
        m_pd3dDevice->SetRenderState( D3DRS_ZWRITEENABLE, FALSE );

        m_pd3dDevice->SetRenderTarget( 0, pOldRenderTarget );
        m_pd3dDevice->SetDepthStencilSurface( pOldDepthStencilSurface );

        PIXEndNamedEvent();
    }

    // Apply the preceding shadow map back onto the depth image, marking which empty pixels could
    // have been occluded from the laser's point of view.
    {
        PIXBeginNamedEvent( 0, "Apply shadow map" );

        IDirect3DSurface9* pOldRenderTarget;
        m_pd3dDevice->GetRenderTarget( 0, &pOldRenderTarget );

        XGTEXTURE_DESC IRShadowDesc;
        XGGetTextureDesc( m_pIRShadowTexture, 0, &IRShadowDesc );

        FLOAT vTexDims[] = { (FLOAT) IRShadowDesc.Width, (FLOAT) IRShadowDesc.Height, 1.0f / (FLOAT) IRShadowDesc.Width, 1.0f / (FLOAT) IRShadowDesc.Height, };
        m_pd3dDevice->SetPixelShaderConstantF( reg_TexDims, vTexDims, 1 );

        XMVECTOR vLaserToSensorOffsetWorld = { m_fLaserToSensorOffsetWorld, 0.0f, 0.0f, };
        m_pd3dDevice->SetPixelShaderConstantF( reg_LaserToSensorOffsetWorld, ( FLOAT* )&vLaserToSensorOffsetWorld, 1 );

        XMVECTOR vLaserToSensorOffsetTexcoord = { fLaserToSensorOffsetTexcoord, 0.0f, 0.0f, };
        m_pd3dDevice->SetPixelShaderConstantF( reg_LaserToSensorOffsetTexcoord, ( FLOAT* )&vLaserToSensorOffsetTexcoord, 1 );

        m_pd3dDevice->SetPixelShaderConstantF( reg_RealisticFarDepth, &m_fRealisticFarDepth, 1 );

        m_pd3dDevice->SetTexture( reg_depth, pDepthTexture );
        m_pd3dDevice->SetTexture( reg_irshadow, m_pIRShadowTexture );
        m_pd3dDevice->SetTexture( reg_mask, pMaskTexture );

        m_pd3dDevice->SetRenderTarget( 0, m_pMaskRenderTarget );

        DrawQuad( m_pIRShadowApplyPixelShader );

        // Mask texture is about to be initialized
        pMaskTexture = m_pMaskTexture;

        m_pd3dDevice->Resolve( D3DRESOLVE_RENDERTARGET0, NULL, pMaskTexture, NULL, 0, 0, NULL, 0.0f, 0, NULL );

        m_pd3dDevice->SetRenderTarget( 0, pOldRenderTarget );

        PIXEndNamedEvent();
    }

    PIXEndNamedEvent();
}


//---------------------------------------------------------------------------------------------------------
// Name: SetViewportFromDesc()
// Desc: Set D3D viewport to matching dimensions for Desc
//---------------------------------------------------------------------------------------------------------
VOID Sample::SetViewportFromDesc( CONST XGTEXTURE_DESC* Desc )
{
    D3DVIEWPORT9 Viewport;
    Viewport.X = 0;
    Viewport.Y = 0;
    Viewport.Width = Desc->Width;
    Viewport.Height = Desc->Height;
    Viewport.MinZ = 0.0f;
    Viewport.MaxZ = 1.0f;
    m_pd3dDevice->SetViewport( &Viewport );
}


//---------------------------------------------------------------------------------------------------------
// Name: MarkSmallHoles()
// Desc: Marks pixels with invalid depth which are surrounded by valid depths.  (Also, applied in reverse,
// marks pixels with valid depths which are surrounded by invalid depths.)  The algorithm is inexact, but
// reasonably efficient.
//
// Our approach is to downsample the texture, keeping track of which pixels are bounded by valid depths 
// on the left, right, top, bottom.  At a certain level, we retain only those pixels which are bounded
// on all 4 sides.  Then we upsample and mask out all but these areas.
//---------------------------------------------------------------------------------------------------------
VOID Sample::MarkSmallHoles( IDirect3DTexture9* pDepthTexture, IDirect3DTexture9*& pMaskTexture, BOOL bHolesOrIslands, BOOL bLowResDepth )
{
    PIXBeginNamedEvent( 0, bHolesOrIslands ? "Mark Small Holes" : "Mark Small Islands" );

    // The defaults of 4 and 5 mean (very roughly) that holes smaller than 16x16 will be filled & islands smaller 
    // than 32x32 will be erased.  
    UINT iTerminalMipDepth = bHolesOrIslands ? m_SmallHolesScaleParam.GetValue() : m_SmallIslandsScaleParam.GetValue(); 
    if( bLowResDepth )  // equalize settings for low res
    {
        iTerminalMipDepth = Max( iTerminalMipDepth - 1, 1u );
    }

    IDirect3DSurface9* pOldRenderTarget;
    m_pd3dDevice->GetRenderTarget( 0, &pOldRenderTarget );

    PIXBeginNamedEvent( 0, "Downfill" );

    m_pd3dDevice->SetRenderTarget( 0, m_pMaskRenderTarget );

    IDirect3DTexture9* pOrigMaskTexture = pMaskTexture;

    // This doesn't work when downsampling
    m_pd3dDevice->SetRenderState( D3DRS_HALFPIXELOFFSET, FALSE );

    for( UINT i = 1; i <= iTerminalMipDepth; ++i )
    {
        UINT iSourceMip = i - 1;
        UINT iDestMip = i;

        m_pd3dDevice->SetTexture( reg_mask, pMaskTexture );
        m_pd3dDevice->SetSamplerState( reg_mask, D3DSAMP_MINMIPLEVEL, iSourceMip );

        FLOAT fTopLevel = ( i == 1 ) ? 1.0f : 0.0f;
        m_pd3dDevice->SetPixelShaderConstantF( reg_TopLevel, &fTopLevel, 1 );
        if( fTopLevel )
        {
            m_pd3dDevice->SetSamplerState( reg_mask, D3DSAMP_ADDRESSU, D3DTADDRESS_BORDER );
            m_pd3dDevice->SetSamplerState( reg_mask, D3DSAMP_ADDRESSV, D3DTADDRESS_BORDER );
            m_pd3dDevice->SetSamplerState( reg_mask, D3DSAMP_BORDERCOLOR, bHolesOrIslands ? 0xffffffff : 0x00000000 );
        }

        // Mask texture is about to be initialized
        pMaskTexture = m_pMaskTexture;

        XGTEXTURE_DESC MipDesc;
        XGGetTextureDesc( pMaskTexture, iDestMip, &MipDesc );

        // SetViewport
        SetViewportFromDesc( &MipDesc );

        DrawQuad( bHolesOrIslands ? m_pDownfillSmallHolesPixelShader : m_pDownfillSmallIslandsPixelShader );

        // Unset texture, because we are about to resolve to it
        m_pd3dDevice->SetTexture( reg_mask, NULL );

        m_pd3dDevice->Resolve( D3DRESOLVE_RENDERTARGET0, NULL, pMaskTexture, NULL, iDestMip, 0, NULL, 0.0f, 0, NULL );
    }

    m_pd3dDevice->SetSamplerState( reg_mask, D3DSAMP_MINMIPLEVEL, 13 );
    m_pd3dDevice->SetSamplerState( reg_mask, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP );
    m_pd3dDevice->SetSamplerState( reg_mask, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP );
    m_pd3dDevice->SetSamplerState( reg_mask, D3DSAMP_BORDERCOLOR, 0x00000000 );

    PIXEndNamedEvent( );

    PIXBeginNamedEvent( 0, "Upfill" );

    m_pd3dDevice->SetRenderTarget( 0, m_pMaskRenderTarget );

    // This shader makes special use of the half pixel offset state
    m_pd3dDevice->SetRenderState( D3DRS_HALFPIXELOFFSET, TRUE );

    for( UINT i = iTerminalMipDepth - 1; i != (UINT)-1; --i )
    {
        UINT iDestMip = i;

        XGTEXTURE_DESC MipDesc;
        XGGetTextureDesc( pMaskTexture, iDestMip, &MipDesc );

        FLOAT fTopLevel = ( i == 0 ) ? 1.0f : 0.0f;
        m_pd3dDevice->SetPixelShaderConstantF( reg_TopLevel, &fTopLevel, 1 );
        FLOAT vTexDims[] = { (FLOAT) MipDesc.Width, (FLOAT) MipDesc.Height, 1.0f / (FLOAT) MipDesc.Width, 1.0f / (FLOAT) MipDesc.Height, };
        m_pd3dDevice->SetPixelShaderConstantF( reg_TexDims, vTexDims, 1 );

        m_pd3dDevice->SetTexture( reg_mask, ( i == 0 ) ? pOrigMaskTexture : pMaskTexture );
        m_pd3dDevice->SetTexture( reg_masklod, pMaskTexture );

        // SetViewport
        SetViewportFromDesc( &MipDesc );

        DrawQuad( bHolesOrIslands ? m_pUpfillSmallHolesPixelShader : m_pUpfillSmallIslandsPixelShader );

        // Unset texture, because we are about to resolve to it
        m_pd3dDevice->SetTexture( reg_mask, NULL );

        m_pd3dDevice->Resolve( D3DRESOLVE_RENDERTARGET0, NULL, pMaskTexture, NULL, iDestMip, 0, NULL, 0.0f, 0, NULL );
    }

    m_pd3dDevice->SetRenderState( D3DRS_HALFPIXELOFFSET, TRUE );

    PIXEndNamedEvent( );

    m_pd3dDevice->SetRenderTarget( 0, pOldRenderTarget );

    PIXEndNamedEvent( );
}


//---------------------------------------------------------------------------------------------------------
// Name: FillInDepth()
// Desc: Replace unknown depth pixels (value 0) with an educated guess.
// 
// Due to the phyical arrangement of the laser/sensor, the occluded pixels have
// valid depths to their right (in the texture).  We propagate these valid depths leftward.
// For performance reasons, we first downsample for a certain number of mip levels, at
// the same time as we propagate.  
//---------------------------------------------------------------------------------------------------------
VOID Sample::FillInDepth( IDirect3DTexture9*& pDepthTexture, IDirect3DTexture9* pMaskTexture )
{
    PIXBeginNamedEvent( 0, "Fill in missing depth" );

    UINT iTerminalMipDepth = m_pDepthTiledTexture->GetLevelCount( ) - 1;

    // Fill in invalid depth areas --- downsample
    PIXBeginNamedEvent( 0, "Downfill" );

    IDirect3DSurface9* pOldRenderTarget;
    m_pd3dDevice->GetRenderTarget( 0, &pOldRenderTarget );

    m_pd3dDevice->SetRenderTarget( 0, m_pDepthRenderTarget );
    m_pd3dDevice->SetRenderTarget( 1, m_pMaskRenderTarget );

    IDirect3DTexture9* pOriginalDepthTexture = pDepthTexture;
    IDirect3DTexture9* pOrigMaskTexture = pMaskTexture;

    // This doesn't work when downsampling
    m_pd3dDevice->SetRenderState( D3DRS_HALFPIXELOFFSET, FALSE );

    // The shader will fetch only the SHOULD_BE_VALID channel
    m_pd3dDevice->SetTexture( reg_mask, pMaskTexture );

    for( UINT i = 1; i <= iTerminalMipDepth; ++i )
    {
        UINT iSourceMip = Min( i - 1, iTerminalMipDepth );
        UINT iDestMip = Min( i, iTerminalMipDepth );

        XGTEXTURE_DESC MipDesc;
        XGGetTextureDesc( m_pDepthTiledTexture, iDestMip, &MipDesc );

        // SetViewport
        SetViewportFromDesc( &MipDesc );

        FLOAT fTopLevel = ( i == 1 ) ? 1.0f : 0.0f;
        m_pd3dDevice->SetPixelShaderConstantF( reg_TopLevel, &fTopLevel, 1 );

        m_pd3dDevice->SetTexture( reg_depth, pDepthTexture );
        m_pd3dDevice->SetTexture( reg_mask, pMaskTexture );

        m_pd3dDevice->SetSamplerState( reg_depth, D3DSAMP_MINMIPLEVEL, iSourceMip );
        m_pd3dDevice->SetSamplerState( reg_mask, D3DSAMP_MINMIPLEVEL, iSourceMip );

        DrawQuad( m_pDownsampleDepthPixelShader );

        // Unset texture, because we are about to resolve to it
        m_pd3dDevice->SetTexture( reg_depth, NULL );
        m_pd3dDevice->SetTexture( reg_mask, NULL );

        // Now our copy becomes the source for future operations
        pDepthTexture = m_pDepthTiledTexture;

        m_pd3dDevice->Resolve( D3DRESOLVE_RENDERTARGET0, NULL, pDepthTexture, NULL, iDestMip, 0, NULL, 0.0f, 0, NULL );

        // Mask texture is about to be initialized
        pMaskTexture = m_pMaskTexture;

        m_pd3dDevice->Resolve( D3DRESOLVE_RENDERTARGET1, NULL, pMaskTexture, NULL, iDestMip, 0, NULL, 0.0f, 0, NULL );

        m_pd3dDevice->SetTexture( reg_depth, pDepthTexture );
        m_pd3dDevice->SetTexture( reg_mask, pMaskTexture );

        m_pd3dDevice->SetSamplerState( reg_depth, D3DSAMP_MINMIPLEVEL, iDestMip );
        m_pd3dDevice->SetSamplerState( reg_mask, D3DSAMP_MINMIPLEVEL, iDestMip );

        DrawQuad( m_pShiftDepthPixelShader );

        // Unset texture, because we are about to resolve to it
        m_pd3dDevice->SetTexture( reg_depth, NULL );
        m_pd3dDevice->SetTexture( reg_mask, NULL );

        m_pd3dDevice->Resolve( D3DRESOLVE_RENDERTARGET0, NULL, pDepthTexture, NULL, iDestMip, 0, NULL, 0.0f, 0, NULL );

        m_pd3dDevice->Resolve( D3DRESOLVE_RENDERTARGET1, NULL, pMaskTexture, NULL, iDestMip, 0, NULL, 0.0f, 0, NULL );
    }

    m_pd3dDevice->SetSamplerState( reg_depth, D3DSAMP_MINMIPLEVEL, 13 );
    m_pd3dDevice->SetSamplerState( reg_mask, D3DSAMP_MINMIPLEVEL, 13 );

    m_pd3dDevice->SetRenderTarget( 0, pOldRenderTarget );

    PIXEndNamedEvent();

    // Fill in invalid depth areas --- upfill
    PIXBeginNamedEvent( 0, "Upfill" );

    m_pd3dDevice->SetRenderTarget( 0, m_pDepthRenderTarget );

    for( UINT i = iTerminalMipDepth - 1; i != (UINT)-1; --i )
    {
        UINT iDestMip = i;

        FLOAT fTopLevel = ( i == 0 ) ? 1.0f : 0.0f;
        m_pd3dDevice->SetPixelShaderConstantF( reg_TopLevel, &fTopLevel, 1 );

        // The top mip of the destination is not yet initialized
        m_pd3dDevice->SetTexture( reg_depth, ( i == 0 ) ? pOriginalDepthTexture : pDepthTexture );
        m_pd3dDevice->SetTexture( reg_depthlod, pDepthTexture );
        m_pd3dDevice->SetTexture( reg_mask, ( i == 0 ) ? pOrigMaskTexture : pMaskTexture );

        XGTEXTURE_DESC MipDesc;
        XGGetTextureDesc( m_pDepthTiledTexture, iDestMip, &MipDesc );

        // SetViewport
        SetViewportFromDesc( &MipDesc );

        DrawQuad( m_pUpfillDepthPixelShader );

        // Unset texture, because we are about to resolve to it
        m_pd3dDevice->SetTexture( reg_depth, NULL );

        m_pd3dDevice->Resolve( D3DRESOLVE_RENDERTARGET0, NULL, pDepthTexture, NULL, iDestMip, 0, NULL, 0.0f, 0, NULL );
    }

    m_pd3dDevice->SetRenderState( D3DRS_HALFPIXELOFFSET, TRUE );

    PIXEndNamedEvent();

    m_pd3dDevice->SetRenderTarget( 0, pOldRenderTarget );

    PIXEndNamedEvent();
}


//---------------------------------------------------------------------------------------------------------
// Name: FilterDepth()
// Desc: Filter the depth map using the chosen filter, and number of iterations.
//---------------------------------------------------------------------------------------------------------
VOID Sample::FilterDepth( IDirect3DTexture9*& pDepthTexture, 
                         UINT iFilterDepthCount, 
                         UINT iFilterDepthType, 
                         FLOAT fBilateralEdgeBlurDepth  )
{
    IDirect3DSurface9* pOldRenderTarget;
    m_pd3dDevice->GetRenderTarget( 0, &pOldRenderTarget );

    m_pd3dDevice->SetRenderTarget( 0, m_pDepthRenderTarget );

    // Filter the depth map
    PIXBeginNamedEvent( 0, "Filter Depth Map" );
    for( UINT i = 0; i < iFilterDepthCount; ++i )
    {
        m_pd3dDevice->SetTexture( reg_filter, pDepthTexture );

        // Gaussian weights
        const static int g_iSize = 2;
        XMVECTOR fGaussianWeights[2*g_iSize+1] = { {1.0f/16.0f}, {4.0f/16.0f}, {6.0f/16.0f}, {4.0f/16.0f}, {1.0f/16.0f} };
        C_ASSERT( _countof(fGaussianWeights) == 2*g_iSize+1 );
        m_pd3dDevice->SetPixelShaderConstantF( reg_Weights, (FLOAT*)fGaussianWeights, _countof( fGaussianWeights ) );

        // The UI param is in log scale
        FLOAT fBilateralPeakThickness = expf( -fBilateralEdgeBlurDepth * logf( 10.0f ) );
        m_pd3dDevice->SetPixelShaderConstantF( reg_BilateralPeakThickness, (FLOAT*)&fBilateralPeakThickness, 1 );

        // Now our copy becomes the source for future operations
        pDepthTexture = m_pDepthTiledTexture;

        switch( iFilterDepthType )
        {
        case FILTER_TYPE_GAUSSIAN:
            DrawQuad( m_pDirectionalBlurDepthHorizontalPixelShader );

            // Unset texture, because we are about to override its header
            m_pd3dDevice->SetTexture( reg_filter, NULL );

            m_pd3dDevice->Resolve( D3DRESOLVE_RENDERTARGET0, NULL, pDepthTexture, NULL, 0, 0, NULL, 0.0f, 0, NULL );

            m_pd3dDevice->SetTexture( reg_filter, pDepthTexture );

            DrawQuad( m_pDirectionalBlurDepthVerticalPixelShader );

            // Unset texture, because we are about to override its header
            m_pd3dDevice->SetTexture( reg_filter, NULL );

            m_pd3dDevice->Resolve( D3DRESOLVE_RENDERTARGET0, NULL, pDepthTexture, NULL, 0, 0, NULL, 0.0f, 0, NULL );
            break;

        case FILTER_TYPE_GAUSSIAN_DIAGONAL:
            DrawQuad( m_pDirectionalBlurDepthDiagonalNWPixelShader );

            // Unset texture, because we are about to override its header
            m_pd3dDevice->SetTexture( reg_filter, NULL );

            m_pd3dDevice->Resolve( D3DRESOLVE_RENDERTARGET0, NULL, pDepthTexture, NULL, 0, 0, NULL, 0.0f, 0, NULL );

            m_pd3dDevice->SetTexture( reg_filter, pDepthTexture );

            DrawQuad( m_pDirectionalBlurDepthDiagonalSWPixelShader );

            // Unset texture, because we are about to override its header
            m_pd3dDevice->SetTexture( reg_filter, NULL );

            m_pd3dDevice->Resolve( D3DRESOLVE_RENDERTARGET0, NULL, pDepthTexture, NULL, 0, 0, NULL, 0.0f, 0, NULL );
            break;

        case FILTER_TYPE_BILATERAL_2D:
            DrawQuad( m_pBilateralFilterDepth2DPixelShader );

            // Unset texture, because we are about to override its header
            m_pd3dDevice->SetTexture( reg_filter, NULL );

            m_pd3dDevice->Resolve( D3DRESOLVE_RENDERTARGET0, NULL, pDepthTexture, NULL, 0, 0, NULL, 0.0f, 0, NULL );
            break;

        case FILTER_TYPE_BILATERAL_1D:
            DrawQuad( m_pBilateralFilterDepth1DHorizontalPixelShader );

            // Unset texture, because we are about to override its header
            m_pd3dDevice->SetTexture( reg_filter, NULL );

            m_pd3dDevice->Resolve( D3DRESOLVE_RENDERTARGET0, NULL, pDepthTexture, NULL, 0, 0, NULL, 0.0f, 0, NULL );

            m_pd3dDevice->SetTexture( reg_filter, pDepthTexture );

            DrawQuad( m_pBilateralFilterDepth1DVerticalPixelShader );

            // Unset texture, because we are about to override its header
            m_pd3dDevice->SetTexture( reg_filter, NULL );

            m_pd3dDevice->Resolve( D3DRESOLVE_RENDERTARGET0, NULL, pDepthTexture, NULL, 0, 0, NULL, 0.0f, 0, NULL );
            break;

        case FILTER_TYPE_BILATERAL_1D_DIAGONAL:
            DrawQuad( m_pBilateralFilterDepth1DDiagonalNWPixelShader );

            // Unset texture, because we are about to override its header
            m_pd3dDevice->SetTexture( reg_filter, NULL );

            m_pd3dDevice->Resolve( D3DRESOLVE_RENDERTARGET0, NULL, pDepthTexture, NULL, 0, 0, NULL, 0.0f, 0, NULL );

            m_pd3dDevice->SetTexture( reg_filter, pDepthTexture );

            DrawQuad( m_pBilateralFilterDepth1DDiagonalSWPixelShader );

            // Unset texture, because we are about to override its header
            m_pd3dDevice->SetTexture( reg_filter, NULL );

            m_pd3dDevice->Resolve( D3DRESOLVE_RENDERTARGET0, NULL, pDepthTexture, NULL, 0, 0, NULL, 0.0f, 0, NULL );
            break;
        }
    }

    m_pd3dDevice->SetRenderTarget( 0, pOldRenderTarget );

    PIXEndNamedEvent();
}


//---------------------------------------------------------------------------------------------------------
// Name: ClearAs16IfPointSampling()
// Desc: Find the non-_AS_16 equivalent for a given GPUTEXTUREFORMAT.
//---------------------------------------------------------------------------------------------------------
VOID Sample::ClearAs16IfPointSampling( IDirect3DTexture9* pNormalTexture )
{
    const static DWORD g_MapAs16ToNonAs16GpuFormat[] = 
    {
        GPUTEXTUREFORMAT_1_REVERSE,
        GPUTEXTUREFORMAT_1,
        GPUTEXTUREFORMAT_8,
        GPUTEXTUREFORMAT_1_5_5_5,
        GPUTEXTUREFORMAT_5_6_5,
        GPUTEXTUREFORMAT_6_5_5,
        GPUTEXTUREFORMAT_8_8_8_8,
        GPUTEXTUREFORMAT_2_10_10_10,
        GPUTEXTUREFORMAT_8_A,
        GPUTEXTUREFORMAT_8_B,
        GPUTEXTUREFORMAT_8_8,
        GPUTEXTUREFORMAT_Cr_Y1_Cb_Y0_REP,     
        GPUTEXTUREFORMAT_Y1_Cr_Y0_Cb_REP,      
        GPUTEXTUREFORMAT_16_16_EDRAM,          
        GPUTEXTUREFORMAT_8_8_8_8_A,
        GPUTEXTUREFORMAT_4_4_4_4,
        GPUTEXTUREFORMAT_10_11_11,
        GPUTEXTUREFORMAT_11_11_10,
        GPUTEXTUREFORMAT_DXT1,
        GPUTEXTUREFORMAT_DXT2_3,  
        GPUTEXTUREFORMAT_DXT4_5,
        GPUTEXTUREFORMAT_16_16_16_16_EDRAM,
        GPUTEXTUREFORMAT_24_8,
        GPUTEXTUREFORMAT_24_8_FLOAT,
        GPUTEXTUREFORMAT_16,
        GPUTEXTUREFORMAT_16_16,
        GPUTEXTUREFORMAT_16_16_16_16,
        GPUTEXTUREFORMAT_16_EXPAND,
        GPUTEXTUREFORMAT_16_16_EXPAND,
        GPUTEXTUREFORMAT_16_16_16_16_EXPAND,
        GPUTEXTUREFORMAT_16_FLOAT,
        GPUTEXTUREFORMAT_16_16_FLOAT,
        GPUTEXTUREFORMAT_16_16_16_16_FLOAT,
        GPUTEXTUREFORMAT_32,
        GPUTEXTUREFORMAT_32_32,
        GPUTEXTUREFORMAT_32_32_32_32,
        GPUTEXTUREFORMAT_32_FLOAT,
        GPUTEXTUREFORMAT_32_32_FLOAT,
        GPUTEXTUREFORMAT_32_32_32_32_FLOAT,
        GPUTEXTUREFORMAT_32_AS_8,
        GPUTEXTUREFORMAT_32_AS_8_8,
        GPUTEXTUREFORMAT_16_MPEG,
        GPUTEXTUREFORMAT_16_16_MPEG,
        GPUTEXTUREFORMAT_8_INTERLACED,
        GPUTEXTUREFORMAT_32_AS_8_INTERLACED,
        GPUTEXTUREFORMAT_32_AS_8_8_INTERLACED,
        GPUTEXTUREFORMAT_16_INTERLACED,
        GPUTEXTUREFORMAT_16_MPEG_INTERLACED,
        GPUTEXTUREFORMAT_16_16_MPEG_INTERLACED,
        GPUTEXTUREFORMAT_DXN,
        GPUTEXTUREFORMAT_8_8_8_8, //_AS_16_16_16_16,
        GPUTEXTUREFORMAT_DXT1, //_AS_16_16_16_16,
        GPUTEXTUREFORMAT_DXT2_3, //_AS_16_16_16_16,
        GPUTEXTUREFORMAT_DXT4_5, //_AS_16_16_16_16,
        GPUTEXTUREFORMAT_2_10_10_10, //_AS_16_16_16_16,
        GPUTEXTUREFORMAT_10_11_11, //_AS_16_16_16_16,
        GPUTEXTUREFORMAT_11_11_10, //_AS_16_16_16_16,
        GPUTEXTUREFORMAT_32_32_32_FLOAT,
        GPUTEXTUREFORMAT_DXT3A,
        GPUTEXTUREFORMAT_DXT5A,
        GPUTEXTUREFORMAT_CTX1,
        GPUTEXTUREFORMAT_DXT3A_AS_1_1_1_1,
        GPUTEXTUREFORMAT_8_8_8_8_GAMMA_EDRAM,
        GPUTEXTUREFORMAT_2_10_10_10_FLOAT_EDRAM,
    };

    XGTEXTURE_DESC NormalDesc;
    XGGetTextureDesc( pNormalTexture, 0, &NormalDesc );
    D3DVIEWPORT9 Viewport;
    m_pd3dDevice->GetViewport( &Viewport );
    if( NormalDesc.Width == Viewport.Width && NormalDesc.Height == Viewport.Height )
    {
        pNormalTexture->Format.DataFormat = 
            (GPUTEXTUREFORMAT) g_MapAs16ToNonAs16GpuFormat[pNormalTexture->Format.DataFormat];
    }
}


//---------------------------------------------------------------------------------------------------------
// Name: ResolveNormalMap()
// Desc: Build dummy normal map for Resolve.
// Can't resolve from unsigned into signed, so need to alias texture as unsigned.
//---------------------------------------------------------------------------------------------------------
VOID Sample::ResolveNormalMap( BOOL b16bppNormal, UINT iMip )
{
    GPUTEXTURE_FETCH_CONSTANT OldFormat = m_pNormalTexture->Format;

    DWORD dwBaseAddress = m_pNormalTexture->Format.BaseAddress << GPU_TEXTURE_ADDRESS_SHIFT;
    XGTEXTURE_DESC NormalDesc;
    XGGetTextureDesc( m_pNormalTexture, 0, &NormalDesc );

    IDirect3DTexture9 NormalResolveTexture;

    XGSetTextureHeaderEx( NormalDesc.Width,
        NormalDesc.Height,
        0,  // full mips for downsampling
        0,
        b16bppNormal ? D3DFMT_B6G5R5 : D3DFMT_A2B10G10R10,
        0,
        XGHEADEREX_NONPACKED,
        dwBaseAddress,
        XGHEADER_CONTIGUOUS_MIP_OFFSET,
        NormalDesc.RowPitch,
        &NormalResolveTexture,
        NULL,
        NULL );

    m_pNormalTexture->Format = NormalResolveTexture.Format;

    m_pd3dDevice->Resolve( D3DRESOLVE_RENDERTARGET0, NULL, m_pNormalTexture, NULL, iMip, 0, NULL, 0.0f, 0, NULL );

    m_pNormalTexture->Format = OldFormat;
}


//---------------------------------------------------------------------------------------------------------
// Name: InferNormalMap()
// Desc: Use the depth map to infer per-pixel normals.
//---------------------------------------------------------------------------------------------------------
VOID Sample::InferNormalMap( IDirect3DTexture9* pDepthTexture, 
                            BOOL b16bppNormal )
{
    // Generate the inferred normal map
    PIXBeginNamedEvent( 0, "Infer Normal Map" );

    IDirect3DSurface9* pOldRenderTarget;
    m_pd3dDevice->GetRenderTarget( 0, &pOldRenderTarget );

    XGTEXTURE_DESC DepthDesc;
    XGGetTextureDesc( pDepthTexture, 0, &DepthDesc );

    FLOAT vTexDims[] = { (FLOAT) DepthDesc.Width, (FLOAT) DepthDesc.Height, 1.0f / (FLOAT) DepthDesc.Width, 1.0f / (FLOAT) DepthDesc.Height, };
    m_pd3dDevice->SetPixelShaderConstantF( reg_TexDims, vTexDims, 1 );

    XMVECTOR vTwiceTanHalfFOV = { 
        2.0f * tan(  XMConvertToRadians( m_fNuiHorizontalFOV / 2.0f ) ), 
        2.0f * tan( -XMConvertToRadians( m_fNuiVerticalFOV   / 2.0f ) ), 
    };
    m_pd3dDevice->SetPixelShaderConstantF( reg_TwiceTanHalfFOV, ( FLOAT* )&vTwiceTanHalfFOV, 1 );

    m_pd3dDevice->SetTexture( reg_depth, pDepthTexture );

    m_pd3dDevice->SetRenderTarget( 0, m_pNormalRenderTarget );

    DrawQuad( m_pDepthToNormalPixelShader );

    ResolveNormalMap( b16bppNormal );

    m_pd3dDevice->SetRenderTarget( 0, pOldRenderTarget );

    PIXEndNamedEvent();
}


//---------------------------------------------------------------------------------------------------------
// Name: FilterNormal()
// Desc: Filter the normal map using the chosen filter, and the selected number 
// of iterations.
//---------------------------------------------------------------------------------------------------------
VOID Sample::FilterNormal( UINT iFilterNormalCount, 
                         UINT iFilterNormalType, 
                         FLOAT fBilateralEdgeBlurNormal, 
                         BOOL b16bppNormal )
{
    // Filter the normal map
    PIXBeginNamedEvent( 0, "Filter Normal Map" );

    IDirect3DSurface9* pOldRenderTarget;
    m_pd3dDevice->GetRenderTarget( 0, &pOldRenderTarget );

    m_pd3dDevice->SetRenderTarget( 0, m_pNormalRenderTarget );

    // For better performance, remove the '_AS_16' from the normal map if filtering is not needed
    GPUTEXTURE_FETCH_CONSTANT OldFormat = m_pNormalTexture->Format;
    ClearAs16IfPointSampling( m_pNormalTexture );

    for( UINT i = 0; i < iFilterNormalCount; ++i )
    {
        m_pd3dDevice->SetTexture( reg_filter, m_pNormalTexture );

        // Gaussian weights
        const static int g_iSize = 2;
        XMVECTOR fGaussianWeights[2*g_iSize+1] = { {1.0f/16.0f}, {4.0f/16.0f}, {6.0f/16.0f}, {4.0f/16.0f}, {1.0f/16.0f} };
        C_ASSERT( _countof(fGaussianWeights) == 2*g_iSize+1 );
        m_pd3dDevice->SetPixelShaderConstantF( reg_Weights, (FLOAT*)fGaussianWeights, _countof( fGaussianWeights ) );

        // The UI param is in log scale
        FLOAT fBilateralPeakThickness = expf( -fBilateralEdgeBlurNormal * logf( 10.0f ) );
        m_pd3dDevice->SetPixelShaderConstantF( reg_BilateralPeakThickness, (FLOAT*)&fBilateralPeakThickness, 1 );

        switch( iFilterNormalType )
        {
        case FILTER_TYPE_GAUSSIAN:
            DrawQuad( m_pDirectionalBlurNormalHorizontalPixelShader );

            // Unset texture, because we are about to override its header
            m_pd3dDevice->SetTexture( reg_filter, NULL );

            ResolveNormalMap( b16bppNormal );

            m_pd3dDevice->SetTexture( reg_filter, m_pNormalTexture );

            DrawQuad( m_pDirectionalBlurNormalVerticalPixelShader );

            // Unset texture, because we are about to override its header
            m_pd3dDevice->SetTexture( reg_filter, NULL );

            ResolveNormalMap( b16bppNormal );
            break;

        case FILTER_TYPE_GAUSSIAN_DIAGONAL:
            DrawQuad( m_pDirectionalBlurNormalDiagonalNWPixelShader );

            // Unset texture, because we are about to override its header
            m_pd3dDevice->SetTexture( reg_filter, NULL );

            ResolveNormalMap( b16bppNormal );

            m_pd3dDevice->SetTexture( reg_filter, m_pNormalTexture );

            DrawQuad( m_pDirectionalBlurNormalDiagonalSWPixelShader );

            // Unset texture, because we are about to override its header
            m_pd3dDevice->SetTexture( reg_filter, NULL );

            ResolveNormalMap( b16bppNormal );
            break;

        case FILTER_TYPE_BILATERAL_2D:
            DrawQuad( m_pBilateralFilterNormal2DPixelShader );

            // Unset texture, because we are about to override its header
            m_pd3dDevice->SetTexture( reg_filter, NULL );

            ResolveNormalMap( b16bppNormal );
            break;

        case FILTER_TYPE_BILATERAL_1D:
            DrawQuad( m_pBilateralFilterNormal1DHorizontalPixelShader );

            // Unset texture, because we are about to override its header
            m_pd3dDevice->SetTexture( reg_filter, NULL );

            ResolveNormalMap( b16bppNormal );

            m_pd3dDevice->SetTexture( reg_filter, m_pNormalTexture );

            DrawQuad( m_pBilateralFilterNormal1DVerticalPixelShader );

            // Unset texture, because we are about to override its header
            m_pd3dDevice->SetTexture( reg_filter, NULL );

            ResolveNormalMap( b16bppNormal );
            break;

        case FILTER_TYPE_BILATERAL_1D_DIAGONAL:
            DrawQuad( m_pBilateralFilterNormal1DDiagonalNWPixelShader );

            // Unset texture, because we are about to override its header
            m_pd3dDevice->SetTexture( reg_filter, NULL );

            ResolveNormalMap( b16bppNormal );

            m_pd3dDevice->SetTexture( reg_filter, m_pNormalTexture );

            DrawQuad( m_pBilateralFilterNormal1DDiagonalSWPixelShader );

            // Unset texture, because we are about to override its header
            m_pd3dDevice->SetTexture( reg_filter, NULL );

            ResolveNormalMap( b16bppNormal );
            break;
        }
    }

    m_pd3dDevice->SetRenderTarget( 0, pOldRenderTarget );

    m_pNormalTexture->Format = OldFormat;

    PIXEndNamedEvent();
}


//---------------------------------------------------------------------------------------------------------
// Name: DownsampleNormal()
// Desc: Downsample the normal map to create mips.
//---------------------------------------------------------------------------------------------------------
VOID Sample::DownsampleNormal( BOOL b16bppNormal )
{
    // Filter the normal map
    PIXBeginNamedEvent( 0, "Downsample Normal Map" );

    IDirect3DSurface9* pOldRenderTarget;
    m_pd3dDevice->GetRenderTarget( 0, &pOldRenderTarget );

    m_pd3dDevice->SetRenderTarget( 0, m_pNormalRenderTarget );

    // This doesn't work when downsampling
    m_pd3dDevice->SetRenderState( D3DRS_HALFPIXELOFFSET, FALSE );

    for( UINT i = 1; i < m_pNormalTexture->GetLevelCount( ); ++i )
    {
        UINT iSourceMip = i - 1;
        UINT iDestMip = i;

        XGTEXTURE_DESC MipDesc;
        XGGetTextureDesc( m_pNormalTexture, iDestMip, &MipDesc );

        // SetViewport
        SetViewportFromDesc( &MipDesc );

        m_pd3dDevice->SetTexture( reg_normal, m_pNormalTexture );

        m_pd3dDevice->SetSamplerState( reg_normal, D3DSAMP_MINMIPLEVEL, iSourceMip );

        DrawQuad( m_pDownsampleNormalPixelShader );

        // Unset texture, because we are about to resolve to it
        m_pd3dDevice->SetTexture( reg_normal, NULL );

        ResolveNormalMap( b16bppNormal, iDestMip );
    }

    m_pd3dDevice->SetSamplerState( reg_depth, D3DSAMP_MINMIPLEVEL, 13 );
    m_pd3dDevice->SetSamplerState( reg_mask, D3DSAMP_MINMIPLEVEL, 13 );

    m_pd3dDevice->SetRenderState( D3DRS_HALFPIXELOFFSET, TRUE );

    m_pd3dDevice->SetRenderTarget( 0, pOldRenderTarget );

    PIXEndNamedEvent();
}


//---------------------------------------------------------------------------------------------------------
// Name: DrawThumbnails()
// Desc: Draw the processed depth map and inferred normal map for reference.
//---------------------------------------------------------------------------------------------------------
VOID Sample::DrawThumbnails( IDirect3DTexture9* pDepthTexture, 
                            IDirect3DTexture9* pMaskTexture, 
                            BOOL bRenormalizeNormal ) 
{
    BOOL bColorCode = m_ColorCodeThumbnailsParam.GetValue();
    BOOL bMaskValid = pMaskTexture == m_pMaskTexture;  // otherwise Mask texture is uninitialized

    PIXBeginNamedEvent( 0, "Draw Picture-in-pictures" );

    // Draw the raw depth and image map as visualization.
    const FLOAT drawWidth = m_iDepthMapWidth;
    const FLOAT drawHeight = drawWidth * 3.0f / 4.0f;
    const FLOAT separatorHeight = 30.0f;
    FLOAT drawX = m_d3dpp.BackBufferWidth - 50.0f - drawWidth;
    FLOAT drawY = m_d3dpp.BackBufferHeight - 40.0f - 2.0f * drawHeight - 2.0f * separatorHeight;
    drawY += separatorHeight;

    {
        D3DVIEWPORT9 OldViewport;
        m_pd3dDevice->GetViewport( &OldViewport );

        D3DVIEWPORT9 DepthViewport;
        DepthViewport.X = (DWORD) drawX;
        DepthViewport.Y = (DWORD) drawY;
        DepthViewport.Width = (DWORD) drawWidth;
        DepthViewport.Height = (DWORD) drawHeight;
        DepthViewport.MinZ = 0.0f;
        DepthViewport.MaxZ = 1.0f;
        m_pd3dDevice->SetViewport( &DepthViewport );

        // Draw the processed depth map as visualization.
        m_pd3dDevice->SetTexture( reg_depth, pDepthTexture );
        m_pd3dDevice->SetTexture( reg_mask, m_pMaskTexture );

        // Set color coding on/off
        m_pd3dDevice->SetPixelShaderConstantB( reg_ColorCode, &bColorCode, 1 );
        m_pd3dDevice->SetPixelShaderConstantB( reg_MaskValid, &bMaskValid, 1 );
        if( bColorCode )
        {
            m_pd3dDevice->SetPixelShaderConstantF( reg_ColorCodeColors, (FLOAT*) g_vColorCodeColors, _countof( g_vColorCodeColors ) );
        }

        DrawQuad( m_pDepthThumbnailPixelShader );

        m_pd3dDevice->SetTexture( 0, NULL );

        m_pd3dDevice->SetViewport( &OldViewport );
    }

    drawY += drawHeight;
    drawY += separatorHeight;

    {
        D3DVIEWPORT9 OldViewport;
        m_pd3dDevice->GetViewport( &OldViewport );

        D3DVIEWPORT9 NormalViewport;
        NormalViewport.X = (DWORD) drawX;
        NormalViewport.Y = (DWORD) drawY;
        NormalViewport.Width = (DWORD) drawWidth;
        NormalViewport.Height = (DWORD) drawHeight;
        NormalViewport.MinZ = 0.0f;
        NormalViewport.MaxZ = 1.0f;
        m_pd3dDevice->SetViewport( &NormalViewport );

        m_pd3dDevice->SetPixelShaderConstantB( reg_Renormalize, &bRenormalizeNormal, 1 );

        // For better performance, remove the '_AS_16' from the normal map if filtering is not needed
        GPUTEXTURE_FETCH_CONSTANT OldFormat = m_pNormalTexture->Format;
        ClearAs16IfPointSampling( m_pNormalTexture );

        // Draw the inferred normal map as visualization.
        m_pd3dDevice->SetTexture( reg_normal, m_pNormalTexture );

        DrawQuad( m_pCopySignedToBiasPixelShader );

        m_pd3dDevice->SetTexture( reg_normal, NULL );

        m_pNormalTexture->Format = OldFormat;

        m_pd3dDevice->SetViewport( &OldViewport );
    }

    PIXEndNamedEvent();
}


//---------------------------------------------------------------------------------------------------------
// Name: VisualizeDepthRenderDepthMap()
// Desc: Draw the processed depth map into the visualization viewport --- repopulating the depth buffer
// and drawing intersection contours.
//---------------------------------------------------------------------------------------------------------
VOID Sample::VisualizeDepthRenderDepthMap( IDirect3DTexture9* pDepthTexture, 
                                          XMVECTOR vSweetSpotOriginAndRadius, 
                                          XMVECTOR vSweetSpotDirection, 
                                          XMVECTOR vFloorPlane, 
                                          XMVECTOR vSweeperPlane, 
                                          XMVECTOR vSweetSpotColor, 
                                          XMVECTOR vFloorPlaneColor, 
                                          XMVECTOR vSweeperPlaneColor, 
                                          BOOL bDrawFloorPlane, 
                                          BOOL bSweeperOn, 
                                          BOOL bFadeNormal, 
                                          FLOAT fFadeNormalScale, 
                                          BOOL bRenormalizeNormal )
{
    PIXBeginNamedEvent( 0, "Visualize Depth" );

    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_ZWRITEENABLE, TRUE );

    m_pd3dDevice->SetTexture( reg_depth, pDepthTexture );
    m_pd3dDevice->SetTexture( reg_normal, m_pNormalTexture );

    XMVECTOR vTwiceTanHalfFOV = { 
        2.0f * tan(  XMConvertToRadians( m_fNuiHorizontalFOV / 2.0f ) ), 
        2.0f * tan( -XMConvertToRadians( m_fNuiVerticalFOV   / 2.0f ) ), 
    };
    m_pd3dDevice->SetPixelShaderConstantF( reg_TwiceTanHalfFOV, ( FLOAT* )&vTwiceTanHalfFOV, 1 );

    m_pd3dDevice->SetPixelShaderConstantF( reg_SweetSpotOriginAndRadius, ( FLOAT* )&vSweetSpotOriginAndRadius, 1 );
    m_pd3dDevice->SetPixelShaderConstantF( reg_SweetSpotDirection, ( FLOAT* )&vSweetSpotDirection, 1 );

    XMVECTOR vNearFarDist = { 
        m_fNearPlaneDistance, 
        m_fFarPlaneDistance, 
    };
    m_pd3dDevice->SetPixelShaderConstantF( reg_NearFarDist, ( FLOAT* )&vNearFarDist, 1 );

    m_pd3dDevice->SetPixelShaderConstantF( reg_FloorPlane, ( FLOAT* )&vFloorPlane, 1 );

    m_pd3dDevice->SetPixelShaderConstantF( reg_SweeperPlane, ( FLOAT* )&vSweeperPlane, 1 );

    m_pd3dDevice->SetPixelShaderConstantF( reg_SweetSpotColor, (FLOAT*) &vSweetSpotColor, 1 );

    m_pd3dDevice->SetPixelShaderConstantF( reg_FloorPlaneColor, (FLOAT*) &vFloorPlaneColor, 1 );

    m_pd3dDevice->SetPixelShaderConstantF( reg_SweeperPlaneColor, (FLOAT*) &vSweeperPlaneColor, 1 );

    m_pd3dDevice->SetPixelShaderConstantF( reg_OutlineColor, (FLOAT*) &g_vOutlineColor, 1 );

    XMVECTOR vNormalFadeParams = { fFadeNormalScale, 0.0f, fFadeNormalScale, 0.0f };
    m_pd3dDevice->SetPixelShaderConstantF( reg_NormalFadeParams, (FLOAT*) &vNormalFadeParams, 1 );

    m_pd3dDevice->SetPixelShaderConstantB( reg_DrawFloorPlane, &bDrawFloorPlane, 1 );

    m_pd3dDevice->SetPixelShaderConstantB( reg_SweeperOn, &bSweeperOn, 1 );

    m_pd3dDevice->SetPixelShaderConstantB( reg_FadeNormal, &bFadeNormal, 1 );

    m_pd3dDevice->SetPixelShaderConstantB( reg_Renormalize, &bRenormalizeNormal, 1 );

    DrawQuad( m_pDepthMapVisualizePixelShader );

    m_pd3dDevice->SetTexture( reg_depth, NULL );
    m_pd3dDevice->SetTexture( reg_normal, NULL );

    PIXEndNamedEvent( );
}


//---------------------------------------------------------------------------------------------------------
// Name: VisualizeDepthRenderFloorPlane()
// Desc: Draw the simulated floor plane into the visualization viewport --- drawing intersection contours.
//---------------------------------------------------------------------------------------------------------
VOID Sample::VisualizeDepthRenderFloorPlane( XMVECTOR vSweetSpotOriginAndRadius, 
                                            XMVECTOR vSweetSpotDirection, 
                                            XMVECTOR vSweetSpotColor, 
                                            XMVECTOR vFloorPlaneColor )
{
    PIXBeginNamedEvent( 0, "Visualize Floor Plane" );

    // Grid
    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_SRCBLEND, D3DBLEND_SRCALPHA );
    m_pd3dDevice->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA );
    m_pd3dDevice->SetRenderState( D3DRS_BLENDOP, D3DBLENDOP_ADD );

    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_ZWRITEENABLE, TRUE );

    m_pd3dDevice->SetPixelShaderConstantF( reg_SweetSpotColor, (FLOAT*) &vSweetSpotColor, 1 );

    m_pd3dDevice->SetPixelShaderConstantF( reg_GenericColor, (FLOAT*) &vFloorPlaneColor, 1 );

    m_pd3dDevice->SetPixelShaderConstantF( reg_SweetSpotOriginAndRadius, ( FLOAT* )&vSweetSpotOriginAndRadius, 1 );
    m_pd3dDevice->SetPixelShaderConstantF( reg_SweetSpotDirection, ( FLOAT* )&vSweetSpotDirection, 1 );

    // Units in mm --- we draw the floor plane with radius large enough to reach the far plane, centered in front of the camera.
    FLOAT fFloorRadius = m_fFarPlaneDistance / 2.0f;
    XMMATRIX matWorldFloor = XMMatrixScaling( fFloorRadius, fFloorRadius, fFloorRadius ) 
        * XMMatrixRotationX( XM_PI / 2.0f ) 
        * XMMatrixTranslation( 0.0f, m_fFloorPlaneHeight, fFloorRadius )
        * XMMatrixRotationX( m_fCameraElevationRadians );
    XMMATRIX matWorldFloorT = XMMatrixTranspose( matWorldFloor );
    m_pd3dDevice->SetPixelShaderConstantF( reg_Matrix, ( FLOAT* )&matWorldFloorT, 3 );

    DrawGrid( m_pSyntheticPlanePixelShader, matWorldFloor );

    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );

    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_ZWRITEENABLE, FALSE );

    PIXEndNamedEvent( );
}


//---------------------------------------------------------------------------------------------------------
// Name: VisualizeDepthRenderSweeperPlane()
// Desc: Draw the sweeper plane into the visualization viewport --- drawing intersection contours.
//---------------------------------------------------------------------------------------------------------
VOID Sample::VisualizeDepthRenderSweeperPlane( XMMATRIX& matWorldSweep, 
                                              XMVECTOR vSweetSpotOriginAndRadius, 
                                              XMVECTOR vSweetSpotDirection, 
                                              XMVECTOR vSweetSpotColor, 
                                              XMVECTOR vSweeperPlaneColor )
{
    PIXBeginNamedEvent( 0, "Sweeper Grid" );

    // Grid
    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_SRCBLEND, D3DBLEND_SRCALPHA );
    m_pd3dDevice->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA );
    m_pd3dDevice->SetRenderState( D3DRS_BLENDOP, D3DBLENDOP_ADD );

    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_ZWRITEENABLE, FALSE );

    m_pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_NONE );

    m_pd3dDevice->SetPixelShaderConstantF( reg_SweetSpotColor, (FLOAT*) &vSweetSpotColor, 1 );

    m_pd3dDevice->SetPixelShaderConstantF( reg_GenericColor, (FLOAT*) &vSweeperPlaneColor, 1 );

    m_pd3dDevice->SetPixelShaderConstantF( reg_SweetSpotOriginAndRadius, ( FLOAT* )&vSweetSpotOriginAndRadius, 1 );
    m_pd3dDevice->SetPixelShaderConstantF( reg_SweetSpotDirection, ( FLOAT* )&vSweetSpotDirection, 1 );

    XMMATRIX matWorldSweepT = XMMatrixTranspose( matWorldSweep );
    m_pd3dDevice->SetPixelShaderConstantF( reg_Matrix, ( FLOAT* )&matWorldSweepT, 3 );

    DrawGrid( m_pSyntheticPlanePixelShader, matWorldSweep );

    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );

    m_pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_CCW );

    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, FALSE );

    PIXEndNamedEvent( );
}


//---------------------------------------------------------------------------------------------------------
// Name: VisualizeDepthRenderSweetSpot()
// Desc: Draw the simulated sweet spot into the visualization viewport.
//---------------------------------------------------------------------------------------------------------
VOID Sample::VisualizeDepthRenderSweetSpot( XMVECTOR vSweetSpotColor )
{
    PIXBeginNamedEvent( 0, "Visualize Sweet Spot" );

    // Alpha-blended sweet spot
    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_SRCBLEND, D3DBLEND_SRCALPHA );
    m_pd3dDevice->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA );
    m_pd3dDevice->SetRenderState( D3DRS_BLENDOP, D3DBLENDOP_ADD );

    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_ZWRITEENABLE, FALSE );

    m_pd3dDevice->SetPixelShaderConstantF( reg_GenericColor, (FLOAT*) &vSweetSpotColor, 1 );

    XMMATRIX matWorldSweetSpot = XMMatrixScaling( m_fPulsedSweetSpotRadius, m_fSweetSpotHeight, m_fPulsedSweetSpotRadius ) 
        * XMMatrixTranslation( m_fSweetSpotCenterX, m_fFloorPlaneHeight, m_fSweetSpotCenterZ )
        * XMMatrixRotationX( m_fCameraElevationRadians );

    DrawSweetSpot( m_pSolidColorPixelShader, matWorldSweetSpot );

    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );

    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, FALSE );

    PIXEndNamedEvent();
}


//---------------------------------------------------------------------------------------------------------
// Name: VisualizeDepthRenderArrows()
// Desc: Draw the arrows pointing at the sweetspot.
//---------------------------------------------------------------------------------------------------------
VOID Sample::VisualizeDepthRenderArrows( XMVECTOR vArrowColor )
{
    PIXBeginNamedEvent( 0, "Sweet Spot Arrows" );

    // Alpha-blended sweet spot
    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_SRCBLEND, D3DBLEND_SRCALPHA );
    m_pd3dDevice->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA );
    m_pd3dDevice->SetRenderState( D3DRS_BLENDOP, D3DBLENDOP_ADD );

    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, TRUE );

    m_pd3dDevice->SetPixelShaderConstantF( reg_GenericColor, (FLOAT*) &vArrowColor, 1 );

    // Units in mm --- the arrows are 1 m off the ground, 0.2 m long, and spaced by 15% of the sweetspot radius
    {
        XMVECTOR vArrowOffset = {
            m_fSweetSpotCenterX - 1.15f * m_fSweetSpotRadius, 
            m_fFloorPlaneHeight + 1000.f, 
            m_fSweetSpotCenterZ 
        };

        // Units in mm 
        XMMATRIX matWorldArrow = XMMatrixScaling( 200.0f, 200.0f, 200.0f ) 
            * XMMatrixRotationZ( XM_PI / 2.0f ) 
            * XMMatrixTranslationFromVector( vArrowOffset )
            * XMMatrixRotationX( m_fCameraElevationRadians );

        DrawArrow( m_pSolidColorPixelShader, matWorldArrow );
    }

    {
        XMVECTOR vArrowOffset = {
            m_fSweetSpotCenterX + 1.15f * m_fSweetSpotRadius, 
            m_fFloorPlaneHeight + 1000.f, 
            m_fSweetSpotCenterZ 
        };

        XMMATRIX matWorldArrow = XMMatrixScaling( 200.0f, 200.0f, 200.0f ) 
            * XMMatrixRotationZ( -XM_PI / 2.0f ) 
            * XMMatrixTranslationFromVector( vArrowOffset )
            * XMMatrixRotationX( m_fCameraElevationRadians );

        DrawArrow( m_pSolidColorPixelShader, matWorldArrow );
    }

    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );

    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, FALSE );

    PIXEndNamedEvent( );
}


//---------------------------------------------------------------------------------------------------------
// Name: VisualizeDepth()
// Desc: Draw the depth visualization.
//---------------------------------------------------------------------------------------------------------
VOID Sample::VisualizeDepth( IDirect3DTexture9* pDepthTexture, 
                            BOOL bSmallEffectViewport, 
                            BOOL bHideBorder, 
                            BOOL bFadeNormal, 
                            FLOAT fFadeNormalScale, 
                            BOOL bRenormalizeNormal, 
                            BOOL bDrawFloorPlane ) 
{
    FLOAT drawWidth = ( bSmallEffectViewport ) ? m_iDepthMapWidth :640.0f;
    FLOAT drawHeight = drawWidth * 3.0f / 4.0f;
    FLOAT drawX = 100.0f;
    FLOAT drawY = m_d3dpp.BackBufferHeight - 40.0f - drawHeight;

    D3DVIEWPORT9 OldViewport;
    m_pd3dDevice->GetViewport( &OldViewport );

    D3DVIEWPORT9 EffectViewport;
    EffectViewport.X = (DWORD) drawX;
    EffectViewport.Y = (DWORD) drawY;
    EffectViewport.Width = (DWORD) drawWidth;
    EffectViewport.Height = (DWORD) drawHeight;
    EffectViewport.MinZ = 0.0f;
    EffectViewport.MaxZ = 1.0f;
    m_pd3dDevice->SetViewport( &EffectViewport );

    // The margins of the image usually look bad, due to missing or uncertain depth
    // data.  We hide this area using a scissor rect.  That's slightly inefficient, 
    // since all the prelim work occurs on the full image, including portions which
    // we will crop.
    if( bHideBorder )
    {
        // The left side is missing 4 pixels
        // The right side has the worst IR shadow
        // The top and bottom sides are less problematic
        UINT iMissingColumns = 4 * ( EffectViewport.Width / m_iDepthMapWidth ); 
        FLOAT fCropFraction = 0.1f;

        m_pd3dDevice->SetRenderState( D3DRS_SCISSORTESTENABLE, TRUE );

        RECT rectScissor;
        rectScissor.left    = EffectViewport.X + iMissingColumns;                             
        rectScissor.right   = EffectViewport.X + UINT( drawWidth * ( 1.0f - fCropFraction ) ) + iMissingColumns;  
        rectScissor.top     = EffectViewport.Y + UINT( drawHeight * fCropFraction / 2 );    
        rectScissor.bottom  = EffectViewport.Y + UINT( drawHeight * ( 1.0f - fCropFraction / 2 ) );   

        m_pd3dDevice->SetScissorRect( &rectScissor );
    }

    PIXBeginNamedEvent( 0, "Depth Map Visualization" );

    // Get elapsed time
    static FLOAT fAppTime = 0.0f;
    if( !m_bPause )
    {
        fAppTime = (FLOAT) m_Timer.GetAppTime();
    }

    static FLOAT g_fSweetSpotOscillationPeriod = 1.0f;
    static FLOAT g_fFloorPlaneOscillationPeriod = 1.0f;
    static FLOAT g_fSweeperGridOscillationPeriod = 8.0f;
    static FLOAT g_fSweeperAxisPeriod = 6.0f * g_fSweeperGridOscillationPeriod;

    FLOAT fSweetSpotOscillation = 0.5f * cosf( fAppTime / g_fSweetSpotOscillationPeriod * XM_2PI ) + 0.5f;
    FLOAT fFloorPlaneOscillation = 0.5f * cosf( fAppTime / g_fFloorPlaneOscillationPeriod * XM_2PI ) + 0.5f;
    FLOAT fSweeperGridOscillation = 0.5f * cosf( fAppTime / g_fSweeperGridOscillationPeriod * XM_2PI ) + 0.5f;
    UINT iSweeperAxis = UINT( fmodf( fAppTime, g_fSweeperAxisPeriod ) / g_fSweeperGridOscillationPeriod );
    BOOL bSweeperOn = iSweeperAxis & 0x01;
    iSweeperAxis >>= 1;

    static FLOAT m_fSweetSpotRadiusPulsePeriod = 3.0f;
    FLOAT fSweetSpotRadiusPulse = cosf( fAppTime / m_fSweetSpotRadiusPulsePeriod * XM_2PI );
    m_fPulsedSweetSpotRadius = m_fSweetSpotRadius * ( 1.0f + 0.05f * fSweetSpotRadiusPulse );

    // World space basis vectors  
    //XMVECTOR vRight = XMVectorSet( 1.0f, 0.0f, 0.0f, 0.0f );
    XMVECTOR vUp = XMVectorSet( 0.0f, 1.0f, 0.0f, 0.0f );
    XMVECTOR vForward = XMVectorSet( 0.0f, 0.0f,1.0f, 0.0f );

    // The world-to-camera transform
    XMMATRIX matCameraElevation = XMMatrixRotationX( m_fCameraElevationRadians );

    // World vectors --- camera relative
    //XMVECTOR vWorldRightInCameraSpace = XMVector3Transform( vRight, matCameraElevation );
    XMVECTOR vWorldUpInCameraSpace = XMVector3Transform( vUp, matCameraElevation );
    //XMVECTOR vWorldForwardInCameraSpace = XMVector3Transform( vForward, matCameraElevation );

    XMVECTOR vSweetSpotOriginAndRadius = XMVector3Transform( 
        XMVectorSet( m_fSweetSpotCenterX, 0.0f, m_fSweetSpotCenterZ, 0.0f ), matCameraElevation );
    vSweetSpotOriginAndRadius.w = m_fPulsedSweetSpotRadius;
    XMVECTOR vSweetSpotDirection = vWorldUpInCameraSpace; 

    XMVECTOR vFloorPlane = vWorldUpInCameraSpace;
    vFloorPlane.w = -m_fFloorPlaneHeight;
    FLOAT fFloorRadius = m_fFarPlaneDistance / 2.0f;
    FLOAT fHalfFloorRadius = fFloorRadius / 2.0f;

    // matWorldSweep is the local-to-world transform for the Sweeper plane.
    // vSweeperPlane is the plane equation (.xyz is the normal, .z is the signed perp distance from the origin)
    FLOAT fStart = 0.0f, fStop = 0.0f;
    XMMATRIX matWorldSweep = XMMatrixIdentity();
    switch( iSweeperAxis )
    {
    case 0: // Vertical plane, diagonal
            fStart = 4000.0f;
            fStop = 500.0f;
            matWorldSweep = XMMatrixTranslation( -fFloorRadius, fHalfFloorRadius + m_fFloorPlaneHeight, 0.0f )
                * XMMatrixRotationY( XM_PI / 4.0f ) 
                * matCameraElevation;
        break;

    case 1: // Horizontal plane, up-and-down
            fStart = 1000.0f;
            fStop = m_fFloorPlaneHeight;
            matWorldSweep = XMMatrixTranslation( 0.0f, -fFloorRadius, 0.0f )
                * XMMatrixRotationX( -XM_PI / 2.0f ) 
                * matCameraElevation;
        break;

    case 2: // Vertical plane, diagonal
            fStart = 4000.0f;
            fStop = 500.0f;
            matWorldSweep = XMMatrixTranslation( fFloorRadius, fHalfFloorRadius + m_fFloorPlaneHeight, 0.0f )
                * XMMatrixRotationY( -XM_PI / 4.0f ) 
                * matCameraElevation;
        break;

    default:
        assert( FALSE );
        break;
    }
    FLOAT fSweep = ( fStart - fStop ) * fSweeperGridOscillation + fStop;    // Measures progression (in mm) along the sweep axis
    matWorldSweep = XMMatrixTranslation( 0.0f, 0.0f, fSweep )
        * XMMatrixScaling( fFloorRadius, fFloorRadius, 1.0f )
        * matWorldSweep;
    XMVECTOR vSweeperPlane = XMVector3TransformNormal( vForward, matWorldSweep );
    vSweeperPlane.w = -XMVector3Dot( vSweeperPlane, matWorldSweep.r[3] ).w;

    XMVECTOR vSweetSpotColor = g_vSweetSpotColor;
    XMVECTOR vFloorPlaneColor = g_vFloorPlaneColor;
    XMVECTOR vSweeperPlaneColor = g_vSweeperPlaneColor;
    XMVECTOR vArrowColor = g_vArrowColor;

    vSweetSpotColor.w = 0.3f * fSweetSpotOscillation + 0.1f;
    vFloorPlaneColor.w = 0.3f * fFloorPlaneOscillation + 0.1f;
    vSweeperPlaneColor.w = 0.2f;
    vArrowColor.w = 0.3f * fSweetSpotOscillation + 0.1f;

    m_pd3dDevice->Clear( 0L, NULL, D3DCLEAR_ZBUFFER, 0x00000000, 1.0f, 0L );

    VisualizeDepthRenderDepthMap( pDepthTexture, 
        vSweetSpotOriginAndRadius, 
        vSweetSpotDirection, 
        vFloorPlane, 
        vSweeperPlane, 
        vSweetSpotColor, 
        vFloorPlaneColor, 
        vSweeperPlaneColor, 
        bDrawFloorPlane, 
        bSweeperOn, 
        bFadeNormal, 
        fFadeNormalScale, 
        bRenormalizeNormal );

    if( bDrawFloorPlane )
    {
        VisualizeDepthRenderFloorPlane( vSweetSpotOriginAndRadius, vSweetSpotDirection, vSweetSpotColor, 
            vFloorPlaneColor );
    }

    if( bSweeperOn )
    {
        VisualizeDepthRenderSweeperPlane( matWorldSweep, vSweetSpotOriginAndRadius, vSweetSpotDirection, 
            vSweetSpotColor, vSweeperPlaneColor );
    }

    VisualizeDepthRenderSweetSpot( vSweetSpotColor );

    VisualizeDepthRenderArrows( vArrowColor );

    m_pd3dDevice->SetRenderState( D3DRS_SCISSORTESTENABLE, FALSE ); // We don't bother pushing/popping this

    m_pd3dDevice->SetViewport( &OldViewport );

    PIXEndNamedEvent( );
}

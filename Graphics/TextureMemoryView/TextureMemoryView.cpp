//-----------------------------------------------------------------------------
// TextureMemoryView.cpp
//
// Demonstrates how to allocate textures in the most memory efficient way
// (see OptimiseAllocationParameters). Also allows you to see texture layouts
// in memory and claim texture regions unused by the GPU (see GetUnusedAreas).
//
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#include <xtl.h>
#include <xgraphics.h>
#include <d3dx9.h>
#include <xbdm.h>

#include <AtgApp.h>
#include <AtgFont.h>
#include <AtgHelp.h>
#include <AtgInput.h>
#include <AtgUtil.h>

#include <algorithm>


//--------------------------------------------------------------------------------------
// Callouts for labelling the gamepad on the help screen
//--------------------------------------------------------------------------------------
ATG::HELP_CALLOUT g_HelpCallouts[] =
{
    { ATG::HELP_BACK_BUTTON,    ATG::HELP_PLACEMENT_2, L"Display\nhelp"  },
    { ATG::HELP_DPAD,           ATG::HELP_PLACEMENT_2, L"Change item focus (u/d)\nDecrease / increase active item (l/r)" },
    { ATG::HELP_LEFT_SHOULDER,  ATG::HELP_PLACEMENT_2, L"Decrease\nactive item in steps" },
    { ATG::HELP_RIGHT_SHOULDER, ATG::HELP_PLACEMENT_2, L"Increase\nactive item in steps" },
    { ATG::HELP_A_BUTTON,       ATG::HELP_PLACEMENT_1, L"Optimal alloc" },
    { ATG::HELP_X_BUTTON,       ATG::HELP_PLACEMENT_1, L"Output alloc table" },
    { ATG::HELP_Y_BUTTON,       ATG::HELP_PLACEMENT_1, L"Safety override" },
};
#define NUM_HELP_CALLOUTS ARRAYSIZE(g_HelpCallouts)



//--------------------------------------------------------------------------------------
// UI items defaults and ranges
//--------------------------------------------------------------------------------------
static const UINT   UI_TEXTURE_SIZE_DEFAULT = 256;
static const UINT   UI_TEXTURE_SIZE_MAXIMUM = 2048;
static const UINT   UI_TEXTURE_SIZE_MINIMUM = 1;
static const UINT   UI_NUM_MIPS_DEFAULT = 12;
static const UINT   UI_NUM_MIPS_MAXIMUM = 12;
static const UINT   UI_NUM_MIPS_MINIMUM = 1;
static const UINT   UI_TEXTURE_GAP_ALIGNMENT_LOG2_DEFAULT = 12;
static const UINT   UI_TEXTURE_GAP_ALIGNMENT_LOG2_MINIMUM = 4;
static const UINT   UI_TEXTURE_GAP_ALIGNMENT_LOG2_MAXIMUM = 12;


//--------------------------------------------------------------------------------------
// Some UI colors
//--------------------------------------------------------------------------------------
static const DWORD  CLR_WHITE       = D3DCOLOR_ARGB( 255, 255, 255, 255 );
static const DWORD  CLR_MID_GRAY    = D3DCOLOR_ARGB( 128, 128, 128, 128 );
static const DWORD  CLR_DARK_GRAY   = D3DCOLOR_ARGB( 64, 64, 64, 64 );
static const DWORD  CLR_RED         = D3DCOLOR_ARGB( 255, 255, 0, 0 );
static const DWORD  CLR_PURPLE      = D3DCOLOR_ARGB( 255, 255, 0, 255 );
static const DWORD  CLR_CYAN        = D3DCOLOR_ARGB( 255, 0, 255, 255 );
static const DWORD  CLR_GREEN       = D3DCOLOR_ARGB( 255, 0, 255, 0 );


//--------------------------------------------------------------------------------------
// Name: Vertex
// Desc: 2D UI vertex
//--------------------------------------------------------------------------------------
struct Vertex
{
    FLOAT   xy[ 2 ];
    FLOAT   uv[ 2 ];
};


//--------------------------------------------------------------------------------------
// Name: IUISelectorItem
// Desc: Interface for a UI Item
//--------------------------------------------------------------------------------------
class IUISelectorItem
{
public:
    virtual BOOL    Change( BOOL bLeft, BOOL bStep ) = 0;
    virtual BOOL    GetText( WCHAR* pBuffer, UINT uBufSize ) = 0;
};


//--------------------------------------------------------------------------------------
// Name: UISelectorItem
// Desc: Base class for a UI Item and keeps the element's title.
//--------------------------------------------------------------------------------------
class UISelectorItem : public IUISelectorItem
{
private:
    const WCHAR*    m_pTitle;
    BOOL            m_bVisible;

protected:

    const WCHAR*    GetTitle() const
    {
        return m_pTitle;
    }

public:
    UISelectorItem() :  m_pTitle( NULL ),
                        m_bVisible( TRUE )
    {
    }

    VOID    SetTitle( const WCHAR* pTitle )
    {
        m_pTitle = pTitle;
    }

    VOID    SetVisible( BOOL bVisible )
    {
        m_bVisible = bVisible;
    }

    BOOL    IsVisible() const
    {
        return m_bVisible;
    }
};


//--------------------------------------------------------------------------------------
// Name: UITextureSizeItem
// Desc: UI item for texture size
//--------------------------------------------------------------------------------------
class UITextureSizeItem : public UISelectorItem
{
private:
    UINT    m_uValue;

public:
    UITextureSizeItem();

    BOOL    Change( BOOL bLeft, BOOL bStep );
    BOOL    GetText( WCHAR* pBuffer, UINT uBufSize );

    UINT    GetValue() const;
};


//--------------------------------------------------------------------------------------
// Name: UINumMipsItem
// Desc: UI item for number of mip levels
//--------------------------------------------------------------------------------------
class UINumMipsItem : public UISelectorItem
{
private:
    UINT    m_uValue;

public:
    UINumMipsItem();

    BOOL    Change( BOOL bLeft, BOOL bStep );
    BOOL    GetText( WCHAR* pBuffer, UINT uBufSize );

    UINT    GetValue() const;
};



//--------------------------------------------------------------------------------------
// Name: UITextureFormatItem
// Desc: UI item for choosing one of the predefined texture formats
//--------------------------------------------------------------------------------------
class UITextureFormatItem : public UISelectorItem
{
private:
    enum TexFormat
    {
        TF_FIRST,
        TF_DXT1 = TF_FIRST,
        TF_LIN_DXT1,

        TF_DXT5,
        TF_LIN_DXT5,

        TF_L8,
        TF_LIN_L8,

        TF_R5G6B5,
        TF_LIN_R5G6B5,

        TF_A8R8G8B8,
        TF_LIN_A8R8G8B8,

        TF_A16B16G16R16,
        TF_LIN_A16B16G16R16,

        TF_A32B32G32R32,
        TF_LIN_A32B32G32R32,

        TF_NUM_FORMATS
    };

    TexFormat   m_format;

public:
    static const WCHAR*  s_tfNames[ TF_NUM_FORMATS ];
    static const D3DFORMAT s_tfFormats[ TF_NUM_FORMATS ];

    UITextureFormatItem();

    BOOL            Change( BOOL bLeft, BOOL bStep );
    BOOL            GetText( WCHAR* pBuffer, UINT uBufSize );

    D3DFORMAT       GetFormat() const;
};


//--------------------------------------------------------------------------------------
// texture names for UITextureFormatItem, corresponds to UITextureFormatItem::TexFormat
//--------------------------------------------------------------------------------------
const WCHAR*  UITextureFormatItem::s_tfNames[] =
{
    L"D3DFMT_DXT1",
    L"D3DFMT_LIN_DXT1",
    L"D3DFMT_DXT5",
    L"D3DFMT_LIN_DXT5",

    L"D3DFMT_L8",
    L"D3DFMT_LIN_L8",

    L"D3DFMT_R5G6B5",
    L"D3DFMT_LIN_R5G6B5",

    L"D3DFMT_A8R8G8B8",
    L"D3DFMT_LIN_A8R8G8B8",

    L"D3DFMT_A16B16G16R16",
    L"D3DFMT_LIN_A16B16G16R16",

    L"D3DFMT_A32B32G32R32",
    L"D3DFMT_LIN_A32B32G32R32",
};


//--------------------------------------------------------------------------------------
// D3DFORMAT for UITextureFormatItem, corresponds to UITextureFormatItem::TexFormat
//--------------------------------------------------------------------------------------
const D3DFORMAT UITextureFormatItem::s_tfFormats[] =
{
    D3DFMT_DXT1,
    D3DFMT_LIN_DXT1,

    D3DFMT_DXT5,
    D3DFMT_LIN_DXT5,

    D3DFMT_L8,
    D3DFMT_LIN_L8,

    D3DFMT_R5G6B5,
    D3DFMT_LIN_R5G6B5,

    D3DFMT_A8R8G8B8,
    D3DFMT_LIN_A8R8G8B8,

    D3DFMT_A16B16G16R16,
    D3DFMT_LIN_A16B16G16R16,

    D3DFMT_A32B32G32R32,
    D3DFMT_LIN_A32B32G32R32,
};


//--------------------------------------------------------------------------------------
// Name: UITexturePairUpItem
// Desc: UI item to select whether and how you want textures pairing up to happen
//--------------------------------------------------------------------------------------
class UITexturePairUpItem : public UISelectorItem
{
private:
    enum PairUp
    {
        DONT_PAIR_UP,
        PAIR_UP_CAP_64,
        PAIR_UP_CAP_128,
        PAIR_UP_MODES
    };

    PairUp  m_pairUpMode;

public:
    UITexturePairUpItem();

    BOOL    Change( BOOL bLeft, BOOL bStep );
    BOOL    GetText( WCHAR* pBuffer, UINT uBufSize );

    UINT    GetValue() const;
};


//--------------------------------------------------------------------------------------
// Name: UITextureGapAlignmentItem
// Desc: UI item to choose what alignment you're looking for texture memory gaps
//--------------------------------------------------------------------------------------
class UITextureGapAlignmentItem : public UISelectorItem
{
private:
    UINT    m_uValue;

public:
    UITextureGapAlignmentItem();

    BOOL    Change( BOOL bLeft, BOOL bStep );
    BOOL    GetText( WCHAR* pBuffer, UINT uBufSize );

    UINT    GetValue() const;
};


//--------------------------------------------------------------------------------------
// Name: UITexturePackedTailsItem
// Desc: UI item to choose whether you need packed mips
//--------------------------------------------------------------------------------------
class UITexturePackedTailsItem : public UISelectorItem
{
private:
    BOOL    m_bValue;

public:
    UITexturePackedTailsItem();

    BOOL    Change( BOOL bLeft, BOOL bStep );
    BOOL    GetText( WCHAR* pBuffer, UINT uBufSize );

    UINT    GetValue() const;
};


//-----------------------------------------------------------------------------
// Name: class Sample
// Desc: Main class to run this application. Most functionality is inherited
//       from the ATG::Application base class.
//-----------------------------------------------------------------------------
class Sample : public ATG::Application
{
private:
    static const UINT   MAX_NUMBER_OF_MIPLEVELS = 12;
    static const UINT   MAX_NUMBER_OF_GAPS = 32;
    static const UINT   NUM_UI_ITEMS = 7;

    ATG::Font   m_Font;                 // Font for drawing text
    ATG::Timer  m_Timer;                // Timer
    ATG::Help   m_Help;                 // Display help
    BOOL        m_bDrawHelp;

    // UI navigation and controls
    UINT                        m_uCurUIItem;
    UITextureSizeItem           m_textureSizes[ 2 ];    // Width and Height
    UINumMipsItem               m_textureMips;
    UITextureFormatItem         m_textureFormats;
    UITexturePairUpItem         m_pairUp;
    UITextureGapAlignmentItem   m_gapAlignment;
    UITexturePackedTailsItem    m_packedTails;
    UISelectorItem*             m_uiItems[ NUM_UI_ITEMS ];

    // texture related
    D3DTexture          m_textures[ 2 ];                // TexA and TexB headers
    VOID*               m_pTextureBase;                 // memory for base texture mip
    VOID*               m_pTextureMips;                 // memory for mip chain of a texture (could be NULL)
    D3DTexture          m_texMemoryViewBase;            // texture to view memory layout of base mip of the TexA and TexB
    D3DTexture          m_texMemoryViewMips;            // ditto for mips, could be unused if CONTIGUOUS_MIPS used
    BOOL                m_bOptimalAlloc;                // whether to use optimal texture allocation

    // used to render UI
    IDirect3DVertexDeclaration9*    m_pVDecl;
    IDirect3DVertexBuffer9*         m_pAllMipsQuadVb;
    IDirect3DPixelShader9*          m_pDefaultPs;
    IDirect3DPixelShader9*          m_pMemViewPs;
    IDirect3DVertexShader9*         m_pDefaultVs;

    // basic data filled up in RebuildTexture
    UINT            m_uBaseBytesWritten;
    UINT            m_uMipBytesWritten;
    UINT            m_uBaseBytesAllocated;
    UINT            m_uMipsBytesAllocated;
    const WCHAR*    m_pCantPairUpReason;
    BOOL            m_bSecondTextureValid;

    // information about gaps in texture allocation
    XGLAYOUT_REGION m_unusedAreasInBase[ MAX_NUMBER_OF_GAPS ];
    XGLAYOUT_REGION m_unusedAreasInMips[ MAX_NUMBER_OF_GAPS ];
    UINT            m_uNumUnusedAreasInBase;
    UINT            m_uNumUnusedAreasInMips;
    UINT            m_uNumUnusedPagesInTheMiddle;
    UINT            m_uNumUnusedPagesAtTheEnd;

    // some combinations don't work so we warn user
    BOOL            m_bSafetyUnlocked;
    BOOL            m_bSafetyOverride;
public:
    Sample();

    HRESULT Initialize();
    HRESULT Update();
    HRESULT Render();

private:
    HRESULT RebuildTextures();
    HRESULT RebuildTextures( UINT uTexWidth, UINT uTexHeight, UINT uNumMips, D3DFORMAT texFormat, UINT uPairedMaxSize, BOOL bOptimalAlloc, UINT uGapAlignment, BOOL bPackedTails );
    VOID    RenderMemoryView( FLOAT fX0, FLOAT fY0, FLOAT fWidth, FLOAT fHeight, D3DTexture* pTex, const XGLAYOUT_REGION* pRegions, UINT uNumRegions );
    VOID    CreateTextureViews();
    HRESULT FillMipmapsAndCountWrittenBytes();
    VOID    RunTestBatch();
    BOOL    SafetyCheck( UINT uTexWidth, UINT uTexHeight, UINT uNumMips, D3DFORMAT texFormat, BOOL bOptimalAlloc );
};



//--------------------------------------------------------------------------------------
// Name: NextPow2
// Desc: Finds next power of 2 of the given integer
//--------------------------------------------------------------------------------------
static inline
DWORD   NextPow2( DWORD v )
{
    // next pow2
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    v++;

    return v;
}

//--------------------------------------------------------------------------------------
// Name: Log2
// Desc: Find based 2 logarithm for the given integer
//--------------------------------------------------------------------------------------
static inline
DWORD   Log2( DWORD v )
{
    return 31 - _CountLeadingZeros( v );
}


//--------------------------------------------------------------------------------------
// Name: PrevPow2
// Desc: Finds previous power of 2 of the given integer
//--------------------------------------------------------------------------------------
static inline
DWORD   PrevPow2( DWORD v )
{
    return 1 << Log2( v );
}


//--------------------------------------------------------------------------------------
// Name: WrapDecrementUInt()
// Desc: 
//--------------------------------------------------------------------------------------
static
UINT    WrapDecrementUInt( UINT uValue, UINT uNumValues )
{
    return (uValue + uNumValues - 1) % uNumValues;
}


//--------------------------------------------------------------------------------------
// Name: WrapIncrementUInt()
// Desc: 
//--------------------------------------------------------------------------------------
static
UINT    WrapIncrementUInt( UINT uValue, UINT uNumValues )
{
    return (uValue + 1) % uNumValues;
}


//--------------------------------------------------------------------------------------
// Name: UITextureSizeItem()
// Desc: constructs UITextureSizeItem
//--------------------------------------------------------------------------------------
UITextureSizeItem::UITextureSizeItem() : m_uValue( UI_TEXTURE_SIZE_DEFAULT )
{
}

//--------------------------------------------------------------------------------------
// Name: Change()
// Desc: Changes the stored value
//--------------------------------------------------------------------------------------
BOOL UITextureSizeItem::Change( BOOL bLeft, BOOL bStep )
{
    BOOL    bReturn = FALSE;

    if( bLeft && m_uValue > UI_TEXTURE_SIZE_MINIMUM )
    {
        --m_uValue;
        bReturn = TRUE;
    }

    if( !bLeft && m_uValue < UI_TEXTURE_SIZE_MAXIMUM )
    {
        ++m_uValue;
        bReturn = TRUE;
    }

    if( bReturn && bStep )
    {
        m_uValue = bLeft ? PrevPow2( m_uValue ) : NextPow2( m_uValue );
        if( m_uValue < UI_TEXTURE_SIZE_MINIMUM )
        {
            m_uValue = UI_TEXTURE_SIZE_MINIMUM;
        }
    }

    return bReturn;
}

//--------------------------------------------------------------------------------------
// Name: GetText()
// Desc: Constructs the string for display
//--------------------------------------------------------------------------------------
BOOL UITextureSizeItem::GetText( WCHAR* pBuffer, UINT uBufSize )
{
    return swprintf_s( pBuffer, uBufSize, L"%s %d", GetTitle(), m_uValue ) >= 0;
}

//--------------------------------------------------------------------------------------
// Name: GetValue()
// Desc: Returns the item's value
//--------------------------------------------------------------------------------------
UINT UITextureSizeItem::GetValue() const
{
    return m_uValue;
}



//--------------------------------------------------------------------------------------
// Name: UINumMipsItem()
// Desc: constructs the class
//--------------------------------------------------------------------------------------
UINumMipsItem::UINumMipsItem() : m_uValue( UI_NUM_MIPS_DEFAULT )
{
}


//--------------------------------------------------------------------------------------
// Name: Change()
// Desc: Changes the stored value
//--------------------------------------------------------------------------------------
BOOL UINumMipsItem::Change( BOOL bLeft, BOOL bStep )
{
    BOOL    bReturn = FALSE;

    if( bLeft && m_uValue > UI_NUM_MIPS_MINIMUM )
    {
        --m_uValue;
        bReturn = TRUE;
    }

    if( !bLeft && m_uValue < UI_NUM_MIPS_MAXIMUM )
    {
        ++m_uValue;
        bReturn = TRUE;
    }

    if( bStep )
    {
        m_uValue = bLeft ? UI_NUM_MIPS_MINIMUM : UI_NUM_MIPS_MAXIMUM;
    }

    return bReturn;
}

//--------------------------------------------------------------------------------------
// Name: GetText()
// Desc: Constructs the string for display
//--------------------------------------------------------------------------------------
BOOL UINumMipsItem::GetText( WCHAR* pBuffer, UINT uBufSize )
{
    return swprintf_s( pBuffer, uBufSize, L"%s %d", GetTitle(), m_uValue ) >= 0;
}

//--------------------------------------------------------------------------------------
// Name: GetValue()
// Desc: Returns the item's value
//--------------------------------------------------------------------------------------
UINT UINumMipsItem::GetValue() const
{
    return m_uValue;
}



//--------------------------------------------------------------------------------------
// Name: UITextureFormatItem()
// Desc: constructs the class
//--------------------------------------------------------------------------------------
UITextureFormatItem::UITextureFormatItem() : m_format( TF_FIRST )
{
}


//--------------------------------------------------------------------------------------
// Name: Change()
// Desc: Changes the stored value
//--------------------------------------------------------------------------------------
BOOL UITextureFormatItem::Change( BOOL bLeft, BOOL bStep )
{
    if( bLeft )
    {
        m_format = static_cast< TexFormat >( WrapDecrementUInt( m_format, TF_NUM_FORMATS ) );
    } else
    {
        m_format = static_cast< TexFormat >( WrapIncrementUInt( m_format, TF_NUM_FORMATS ) );
    }

    return TRUE;
}

//--------------------------------------------------------------------------------------
// Name: GetText()
// Desc: Constructs the string for display
//--------------------------------------------------------------------------------------
BOOL UITextureFormatItem::GetText( WCHAR* pBuffer, UINT uBufSize )
{
    return swprintf_s( pBuffer, uBufSize, L"%s %s", GetTitle(), s_tfNames[ m_format ] ) >= 0;
}

//--------------------------------------------------------------------------------------
// Name: PrevPow2
// Desc: Finds previous power of 2 of the given integer
//--------------------------------------------------------------------------------------
D3DFORMAT UITextureFormatItem::GetFormat() const
{
    return s_tfFormats[ m_format ];
}



//--------------------------------------------------------------------------------------
// Name: UITexturePairUpItem()
// Desc: constructs the class
//--------------------------------------------------------------------------------------
UITexturePairUpItem::UITexturePairUpItem() : m_pairUpMode( PAIR_UP_CAP_64 )
{
}


//--------------------------------------------------------------------------------------
// Name: Change()
// Desc: Changes the stored value
//--------------------------------------------------------------------------------------
BOOL    UITexturePairUpItem::Change( BOOL bLeft, BOOL bStep )
{
    if( bLeft )
    {
        m_pairUpMode = static_cast< PairUp >( WrapDecrementUInt( m_pairUpMode, PAIR_UP_MODES ) );
    } else
    {
        m_pairUpMode = static_cast< PairUp >( WrapIncrementUInt( m_pairUpMode, PAIR_UP_MODES ) );
    }

    return TRUE;
}

//--------------------------------------------------------------------------------------
// Name: GetText()
// Desc: Constructs the string for display
//--------------------------------------------------------------------------------------
BOOL    UITexturePairUpItem::GetText( WCHAR* pBuffer, UINT uBufSize )
{
    const WCHAR*    msg;

    switch( m_pairUpMode )
    {
    default:
    case DONT_PAIR_UP:      msg = L"Don't pair up";         break;
    case PAIR_UP_CAP_128:   msg = L"Pair up size cap 128";  break;
    case PAIR_UP_CAP_64:    msg = L"Pair up size cap 64 (free)";   break;
    }

    return swprintf_s( pBuffer, uBufSize, msg ) >= 0;
}

//--------------------------------------------------------------------------------------
// Name: GetValue()
// Desc: Returns the item's value
//--------------------------------------------------------------------------------------
UINT    UITexturePairUpItem::GetValue() const
{
    switch( m_pairUpMode )
    {
    default:
    case DONT_PAIR_UP:      return 0;
    case PAIR_UP_CAP_128:   return 128;
    case PAIR_UP_CAP_64:    return 64;
    }
}



//--------------------------------------------------------------------------------------
// Name: UITextureGapAlignmentItem()
// Desc: Constructs the class
//--------------------------------------------------------------------------------------
UITextureGapAlignmentItem::UITextureGapAlignmentItem() : m_uValue( UI_TEXTURE_GAP_ALIGNMENT_LOG2_DEFAULT )
{
}


//--------------------------------------------------------------------------------------
// Name: Change()
// Desc: Changes the stored value
//--------------------------------------------------------------------------------------
BOOL UITextureGapAlignmentItem::Change( BOOL bLeft, BOOL bStep )
{
    BOOL    bReturn = FALSE;

    if( bLeft && m_uValue > UI_TEXTURE_GAP_ALIGNMENT_LOG2_MINIMUM )
    {
        --m_uValue;
        bReturn = TRUE;
    }

    if( !bLeft && m_uValue < UI_TEXTURE_GAP_ALIGNMENT_LOG2_MAXIMUM )
    {
        ++m_uValue;
        bReturn = TRUE;
    }

    if( bReturn && bStep )
    {
        m_uValue = UI_TEXTURE_GAP_ALIGNMENT_LOG2_DEFAULT;
    }

    return bReturn;
}

//--------------------------------------------------------------------------------------
// Name: GetText()
// Desc: Constructs the string for display
//--------------------------------------------------------------------------------------
BOOL UITextureGapAlignmentItem::GetText( WCHAR* pBuffer, UINT uBufSize )
{
    return swprintf_s( pBuffer, uBufSize, L"%s %d bytes", GetTitle(), GetValue() ) >= 0;
}

//--------------------------------------------------------------------------------------
// Name: GetValue()
// Desc: Returns the item's value
//--------------------------------------------------------------------------------------
UINT UITextureGapAlignmentItem::GetValue() const
{
    return 1 << m_uValue;
}


//--------------------------------------------------------------------------------------
// Name: UITexturePackedTailsItem()
// Desc: Constructrs the class
//--------------------------------------------------------------------------------------
UITexturePackedTailsItem::UITexturePackedTailsItem() : m_bValue( TRUE )
{
}


//--------------------------------------------------------------------------------------
// Name: Change()
// Desc: Changes the stored value
//--------------------------------------------------------------------------------------
BOOL UITexturePackedTailsItem::Change( BOOL bLeft, BOOL bStep )
{
    m_bValue = !m_bValue;
    return TRUE;
}

//--------------------------------------------------------------------------------------
// Name: GetText()
// Desc: Constructs the string for display
//--------------------------------------------------------------------------------------
BOOL UITexturePackedTailsItem::GetText( WCHAR* pBuffer, UINT uBufSize )
{
    return swprintf_s( pBuffer, uBufSize, GetValue() ? L"Pack mip tails" : L"Don't pack mip tails" ) >= 0;
}

//--------------------------------------------------------------------------------------
// Name: GetValue()
// Desc: Returns the item's value
//--------------------------------------------------------------------------------------
UINT UITexturePackedTailsItem::GetValue() const
{
    return m_bValue;
}


//-----------------------------------------------------------------------------
// Name: Sample::Sample
// Desc: Initializes the UI elements.
//-----------------------------------------------------------------------------
Sample::Sample() :  m_uCurUIItem( 0 ),
                    m_pCantPairUpReason( NULL ),
                    m_bOptimalAlloc( FALSE ),
                    m_bSafetyUnlocked( TRUE ),
                    m_bSafetyOverride( FALSE ),
                    m_pTextureBase( NULL ),
                    m_pTextureMips( NULL )
{
    m_textureSizes[ 0 ].SetTitle( L"Width" );
    m_textureSizes[ 1 ].SetTitle( L"Height" );
    m_textureMips.SetTitle( L"Num Mips" );
    m_textureFormats.SetTitle( L"Format" );
    m_pairUp.SetTitle( NULL );
    m_gapAlignment.SetTitle( L"Gap page alignment" );
    m_packedTails.SetTitle( NULL );

    static_assert( ARRAYSIZE( m_uiItems ) == 7, "we'll add 7 controls" );
    m_uiItems[ 0 ] = &m_textureSizes[ 0 ];
    m_uiItems[ 1 ] = &m_textureSizes[ 1 ];
    m_uiItems[ 2 ] = &m_textureMips;
    m_uiItems[ 3 ] = &m_textureFormats;
    m_uiItems[ 4 ] = &m_pairUp;
    m_uiItems[ 5 ] = &m_gapAlignment;
    m_uiItems[ 6 ] = &m_packedTails;

    XMemSet( m_textures, 0, sizeof( m_textures ) );
}


//-----------------------------------------------------------------------------
// Name: Initialize()
// Desc: Initialize app-dependent objects.
//-----------------------------------------------------------------------------
HRESULT Sample::Initialize()
{
    // Create the font
    if( FAILED( m_Font.Create( "game:\\Media\\Fonts\\Arial_16.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Confine text drawing to the title safe area
    m_Font.SetWindow( ATG::GetTitleSafeArea() );

    // Create the help
    if( FAILED( m_Help.Create( "game:\\Media\\Help\\Help.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    m_bDrawHelp = FALSE;

    // Create common vertex declaration used by all the screen-space effects
    static const D3DVERTEXELEMENT9 decl[] =
    {
        { 0, 0, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
        { 0, 8, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
        D3DDECL_END()
    };
    if( FAILED( m_pd3dDevice->CreateVertexDeclaration( decl, &m_pVDecl ) ) )
        return ATGAPPERR_MEDIANOTFOUND; // fix

    if( FAILED( m_pd3dDevice->CreateVertexBuffer( 4 * sizeof( Vertex ), 0, 0, 0, &m_pAllMipsQuadVb, NULL ) ) )
        return ATGAPPERR_MEDIANOTFOUND; // fix

    // create a unit quad centered at top left corner
    {
        Vertex* pVerts;
        m_pAllMipsQuadVb->Lock( 0, 0, reinterpret_cast< VOID** >( &pVerts ), 0 );

        pVerts[ 0 ].xy[ 0 ] = 0;
        pVerts[ 0 ].xy[ 1 ] = 0;
        pVerts[ 0 ].uv[ 0 ] = 0;
        pVerts[ 0 ].uv[ 1 ] = 0;

        pVerts[ 1 ].xy[ 0 ] = 1;
        pVerts[ 1 ].xy[ 1 ] = 0;
        pVerts[ 1 ].uv[ 0 ] = 1;
        pVerts[ 1 ].uv[ 1 ] = 0;

        pVerts[ 2 ].xy[ 0 ] = 1;
        pVerts[ 2 ].xy[ 1 ] = -1;
        pVerts[ 2 ].uv[ 0 ] = 1;
        pVerts[ 2 ].uv[ 1 ] = 1;

        pVerts[ 3 ].xy[ 0 ] = 0;
        pVerts[ 3 ].xy[ 1 ] = -1;
        pVerts[ 3 ].uv[ 0 ] = 0;
        pVerts[ 3 ].uv[ 1 ] = 1;

        m_pAllMipsQuadVb->Unlock();
    }

    // Create shaders
    if( FAILED( ATG::LoadVertexShader( "game:\\Media\\Shaders\\DefaultVS.xvu", &m_pDefaultVs ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    if( FAILED( ATG::LoadPixelShader( "game:\\Media\\Shaders\\GenericPS.xpu", &m_pDefaultPs ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    if( FAILED( ATG::LoadPixelShader( "game:\\Media\\Shaders\\MemViewPS.xpu", &m_pMemViewPs ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    HRESULT hr = RebuildTextures();
    if( FAILED( hr ) )
        return hr;

    return S_OK;
}


//-----------------------------------------------------------------------------
// Name: Update()
// Desc: Called once per frame, the call is the entry point for updating
//       the scene.
//-----------------------------------------------------------------------------
HRESULT Sample::Update()
{
    // Get the current gamepad status
    ATG::GAMEPAD* pGamepad = ATG::Input::GetMergedInput();

    // Toggle help
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_BACK )
    {
        m_bDrawHelp = !m_bDrawHelp;
    }

    // move up an down skipping disabled controls where needed
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_DPAD_UP )
    {
        do
        {
            m_uCurUIItem = WrapDecrementUInt( m_uCurUIItem, ARRAYSIZE( m_uiItems ) );
        } while( !m_uiItems[ m_uCurUIItem ]->IsVisible() );
    }

    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_DPAD_DOWN )
    {
        do
        {
            m_uCurUIItem = WrapIncrementUInt( m_uCurUIItem, ARRAYSIZE( m_uiItems ) );
        } while( !m_uiItems[ m_uCurUIItem ]->IsVisible() );
    }

    if( !m_bDrawHelp )
    {
        BOOL    bNeedToRebuildTextures = FALSE;

        if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_A )
        {
            bNeedToRebuildTextures |= TRUE;
            m_bOptimalAlloc = !m_bOptimalAlloc;
        }

        if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_Y )
        {
            if( !m_bSafetyUnlocked )
            {
                bNeedToRebuildTextures |= TRUE;
                m_bSafetyOverride = TRUE;
            }
        }

        if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_DPAD_LEFT )
        {
            bNeedToRebuildTextures |= m_uiItems[ m_uCurUIItem ]->Change( TRUE, FALSE );
        }

        if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_DPAD_RIGHT )
        {
            bNeedToRebuildTextures |= m_uiItems[ m_uCurUIItem ]->Change( FALSE, FALSE );
        }

        if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_LEFT_SHOULDER )
        {
            bNeedToRebuildTextures |= m_uiItems[ m_uCurUIItem ]->Change( TRUE, TRUE );
        }

        if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER )
        {
            bNeedToRebuildTextures |= m_uiItems[ m_uCurUIItem ]->Change( FALSE, TRUE );
        }

        if( bNeedToRebuildTextures )
        {
            HRESULT hr = RebuildTextures();
            if( FAILED( hr ) )
                return hr;
        }

        if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_X )
        {
            RunTestBatch();
        }
    }

    return S_OK;
}



//--------------------------------------------------------------------------------------
// Name: RebuildTextures()
// Desc: Loads settings from UI elements and calls the real RebuildTexture
//--------------------------------------------------------------------------------------
HRESULT Sample::RebuildTextures()
{
    return RebuildTextures( m_textureSizes[ 0 ].GetValue(),
                            m_textureSizes[ 1 ].GetValue(),
                            m_textureMips.GetValue(),
                            m_textureFormats.GetFormat(),
                            m_pairUp.GetValue(),
                            m_bOptimalAlloc,
                            m_gapAlignment.GetValue(),
                            m_packedTails.GetValue() );
}


//--------------------------------------------------------------------------------------
// Name: GetMipColor
// Desc: Returns a nicely distinguishable FLOAT3 color for coloring mips of TexA/TexB
//--------------------------------------------------------------------------------------
static
const FLOAT*    GetMipColor( UINT uMipLevel, UINT uTexNum )
{
    assert( uTexNum < 3 );

    // 3 floats in a color
    // cycle through 7 colors
    // 2 arrays -- one for TexA and another for texB
    static const FLOAT  colors[ 2 ][ 7 ][ 3 ] =
    {
        {
            1.f, 1.f, 1.f,
            1.f, 0.f, 0.f,
            0.f, 1.f, 0.f,
            0.f, 0.f, 1.f,
            1.f, 1.f, 0.f,
            1.f, 0.f, 1.f,
            0.f, 1.f, 1
        },
        {
            0.5f, 0.5f, 0.5f,
            0.5f, 0, 0,
            0, 0.5f, 0,
            0, 0, 0.5f,
            0.5f, 0.5f, 0,
            0.5f, 0, 0.5f,
            0, 0.5f, 0.5f
        },
    };

    return &colors[ uTexNum ][ uMipLevel % ARRAYSIZE( colors[ 0 ] ) ][ 0 ];
}


//--------------------------------------------------------------------------------------
// Name: F3To565
// Desc: Converts FLOAT3 color to 16bit RGB565 representation
//--------------------------------------------------------------------------------------
static
VOID    F3To565( VOID* p565, const FLOAT* pSrc )
{
    WORD* pDest = static_cast< WORD* >( p565 );

    pDest[ 0 ] =     static_cast< WORD >(pSrc[ 0 ] * 31)        |
                    (static_cast< WORD >(pSrc[ 1 ] * 63) << 5)  |
                    (static_cast< WORD >(pSrc[ 2 ] * 31) << 11);
}



//--------------------------------------------------------------------------------------
// Name: F3To8x4
// Desc: Converts FLOAT3 color to 32bit RGBA8888 representation
//--------------------------------------------------------------------------------------
static
VOID    F3To8x4( VOID* p8x4, const FLOAT* pSrc )
{
    BYTE*   pDest = static_cast< BYTE* >( p8x4 );

    pDest[ 0 ] = 0xff;
    pDest[ 1 ] = static_cast< BYTE >( pSrc[ 2 ] * 0xff );
    pDest[ 2 ] = static_cast< BYTE >( pSrc[ 1 ] * 0xff );
    pDest[ 3 ] = static_cast< BYTE >( pSrc[ 0 ] * 0xff );;
}


//--------------------------------------------------------------------------------------
// Name: F3To16x4
// Desc: Converts FLOAT3 color to 64bit RGBA16 representation
//--------------------------------------------------------------------------------------
static
VOID    F3To16x4( VOID* p16x4, const FLOAT* pSrc )
{
    WORD* pDest = static_cast< WORD* >( p16x4 );

    pDest[ 0 ] = static_cast< WORD >( pSrc[ 2 ] * 0xffff );
    pDest[ 1 ] = static_cast< WORD >( pSrc[ 1 ] * 0xffff );
    pDest[ 2 ] = static_cast< WORD >( pSrc[ 0 ] * 0xffff );
    pDest[ 3 ] = 0xffff;
}


//--------------------------------------------------------------------------------------
// Name: F3To32x4
// Desc: Converts FLOAT3 color to 128bit RGBA32 representation
//--------------------------------------------------------------------------------------
static
VOID    F3To32x4( VOID* p32x4, const FLOAT* pSrc )
{
    DWORD* pDest = static_cast< DWORD* >( p32x4 );

    const double  xffffffff = static_cast< double >( 0xffffffff );

    pDest[ 0 ] = static_cast< DWORD >( static_cast< double >( pSrc[ 2 ] ) * xffffffff );
    pDest[ 1 ] = static_cast< DWORD >( static_cast< double >( pSrc[ 1 ] ) * xffffffff );
    pDest[ 2 ] = static_cast< DWORD >( static_cast< double >( pSrc[ 0 ] ) * xffffffff );
    pDest[ 3 ] = 0xffffffff;
}


//--------------------------------------------------------------------------------------
// Name: CoordsToSafeNdc
// Desc: Converts pixel based screen coords to normalised device coordinates in safe
//       area. The coordinates are in virtual 1280x720 space so they are screen
//       resultion independent.
//--------------------------------------------------------------------------------------
static
VOID    CoordsToSafeNdc( FLOAT* pScaleOfs, FLOAT fX0, FLOAT fY0, FLOAT fWidth, FLOAT fHeight )
{
    static const FLOAT  fSafeArea = 0.2f;

    pScaleOfs[ 0 ] = fSafeArea + (2.f * (fX0 / 1280.f) - 1.f);
    pScaleOfs[ 1 ] = (1.f - 2.f * (fY0 / 720.f)) - fSafeArea;
    pScaleOfs[ 2 ] = 2.f * (fWidth / 1280.f);
    pScaleOfs[ 3 ] = 2.f * (fHeight / 720.f);
}


//--------------------------------------------------------------------------------------
// Name: RenderMemoryView
// Desc: Display Base or Mips memory view texture with gaps map
//--------------------------------------------------------------------------------------
VOID    Sample::RenderMemoryView( FLOAT fX0, FLOAT fY0, FLOAT fWidth, FLOAT fHeight,
                                    D3DTexture* pTex, const XGLAYOUT_REGION* pRegions, UINT uNumRegions )
{
    FLOAT scaleOfs[ 4 ];
    CoordsToSafeNdc( scaleOfs, fX0, fY0, fWidth, fHeight );

    m_pd3dDevice->SetVertexShader( m_pDefaultVs );
    m_pd3dDevice->SetVertexDeclaration( m_pVDecl );
    m_pd3dDevice->SetStreamSource( 0, m_pAllMipsQuadVb, 0, sizeof( Vertex ) );
    m_pd3dDevice->SetPixelShader( m_pMemViewPs );
    m_pd3dDevice->SetTextureFetchConstant( 0, pTex );
    m_pd3dDevice->SetVertexShaderConstantF( 0, scaleOfs, 1 );

    // load up gaps map
    if( uNumRegions )
    {
        D3DSURFACE_DESC desc;
        pTex->GetLevelDesc( 0, &desc );

        UINT    uBlockSx, uBlockSy;
        XGGetBlockDimensions( XGGetGpuFormat( desc.Format ), &uBlockSx, &uBlockSy );
        DWORD   dwBitsPerPixel = XGBitsPerPixelFromFormat( desc.Format );

        const UINT  uPixelsPerBlock = uBlockSx * uBlockSy;
        const UINT  uBytesPerBlock = uPixelsPerBlock * dwBitsPerPixel / 8;          // 8 - bits in a byte
        const UINT  uBase = pTex->Format.BaseAddress << GPU_TEXTURE_ADDRESS_SHIFT;

        FLOAT   freeAreasInUv[ ARRAYSIZE( m_unusedAreasInBase )][ 4 ];

        // prepare the shader parameters. for each gap we load a first texel of the gap
        // and the last texel of the gap. we also give the shader the number of gaps
        // the shader can calculate an index for each texel is samples, and highlight
        // it if it's in the gap
        for( UINT i=0; i < uNumRegions; ++i )
        {
            const UINT    uStartInBlocks = (pRegions[ i ].StartOffset - uBase) / uBytesPerBlock;
            const UINT    uEndInBlocks = (pRegions[ i ].EndOffset - uBase) / uBytesPerBlock;

            const UINT    uStartTexel = uStartInBlocks * uPixelsPerBlock;
            const UINT    uEndTexel = uEndInBlocks * uPixelsPerBlock;

            freeAreasInUv[ i ][ 0 ] = static_cast< FLOAT >( uStartTexel );
            freeAreasInUv[ i ][ 1 ] = static_cast< FLOAT >( uEndTexel );
            freeAreasInUv[ i ][ 2 ] = 0;
            freeAreasInUv[ i ][ 3 ] = 0;
        }

        // this controls the green glow effect and tells the shader how big the texture is
        FLOAT   effectControl[ 4 ] = {  static_cast< FLOAT >( desc.Width ),
                                        static_cast< FLOAT >( desc.Height ),
                                        sinf( static_cast< FLOAT >( m_Timer.GetAppTime() ) ) * 0.5f + 0.5f,
                                        0 };

        // we setup the iterator for the for() inside the shader so it iterates over
        // the gaps
        INT    counter[ 4 ] = { uNumRegions, 0, 1, 0 };

        m_pd3dDevice->SetPixelShaderConstantF( 0, effectControl, 1 );
        m_pd3dDevice->SetPixelShaderConstantF( 32, &freeAreasInUv[ 0 ][ 0 ], uNumRegions );
        m_pd3dDevice->SetPixelShaderConstantI( 0, counter, 1 );
    } else
    {
        INT    counter[ 4 ] = { 0 };
        m_pd3dDevice->SetPixelShaderConstantI( 0, counter, 1 );
    }
    m_pd3dDevice->DrawVertices( D3DPT_QUADLIST, 0, 4 );
}


//-----------------------------------------------------------------------------
// Name: Render()
// Desc: Called once per frame, to render the scene.
//-----------------------------------------------------------------------------
HRESULT Sample::Render()
{
    static const FLOAT  TEX_VIEW_X = 0;
    static const FLOAT  TEX_VIEW_Y = 290;
    static const FLOAT  TEX_VIEW_W = 500;
    static const FLOAT  TEX_VIEW_H = 246;

    static const FLOAT  MEM_VIEW_X = TEX_VIEW_X + TEX_VIEW_W + 20;
    static const FLOAT  MEM_VIEW_Y = 0;
    static const FLOAT  MEM_VIEW_W = 500;
    static const FLOAT  MEM_VIEW_H = 576;

    m_pd3dDevice->Clear( 0, NULL, D3DCLEAR_TARGET, CLR_DARK_GRAY, 1, 0xff );

    m_pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_NONE );

    // render all mips quad
    {
        const FLOAT   v0[ 4 ] = { static_cast< FLOAT >( m_textures[ 0 ].GetLevelCount() - 1 ),
                                    m_bSecondTextureValid ? static_cast< FLOAT >( m_textures[ 1 ].GetLevelCount() - 1 ) : 0 };
        FLOAT   scaleOfs[ 4 ];
        CoordsToSafeNdc( scaleOfs, TEX_VIEW_X, TEX_VIEW_Y, TEX_VIEW_W, TEX_VIEW_H );
        m_pd3dDevice->SetVertexShader( m_pDefaultVs );
        m_pd3dDevice->SetPixelShader( m_pDefaultPs );
        m_pd3dDevice->SetVertexDeclaration( m_pVDecl );
        m_pd3dDevice->SetStreamSource( 0, m_pAllMipsQuadVb, 0, sizeof( Vertex ) );
        m_pd3dDevice->SetTextureFetchConstant( 0, &m_textures[ 0 ] );
        m_pd3dDevice->SetTextureFetchConstant( 1, &m_textures[ 1 ] );
        m_pd3dDevice->SetPixelShaderConstantF( 0, v0, 1 );
        m_pd3dDevice->SetVertexShaderConstantF( 0, scaleOfs, 1 );
        m_pd3dDevice->DrawVertices( D3DPT_QUADLIST, 0, 4 );
    }

    // render either one or two views based on whether we have a single alloc or two of them
    if( m_pTextureMips )
    {
        RenderMemoryView( MEM_VIEW_X, MEM_VIEW_Y, MEM_VIEW_W, MEM_VIEW_H/3.f, &m_texMemoryViewBase, m_unusedAreasInBase, m_uNumUnusedAreasInBase );
        RenderMemoryView( MEM_VIEW_X, 8 + MEM_VIEW_H/3.f, MEM_VIEW_W, 2*MEM_VIEW_H/3.f, &m_texMemoryViewMips, m_unusedAreasInMips, m_uNumUnusedAreasInMips );
    } else
    {
        RenderMemoryView( MEM_VIEW_X, MEM_VIEW_Y, MEM_VIEW_W, MEM_VIEW_H, &m_texMemoryViewBase, m_unusedAreasInBase, m_uNumUnusedAreasInBase );
    }

    if( m_bDrawHelp )
    {
        m_Help.Render( &m_Font, g_HelpCallouts, NUM_HELP_CALLOUTS );
    } else
    {
        m_Font.Begin();

        m_Font.SetScaleFactors( 1.0f, 1.0f );
        m_Font.DrawText( 0, 0, CLR_PURPLE, L"TextureMemoryView" );

        m_Font.SetScaleFactors( 0.8f, 0.8f );

        WCHAR   buf[ 1024 ];

        // UI items
        FLOAT fParamY = 40.0f;
        for( UINT i=0; i < ARRAYSIZE( m_uiItems ); ++i )
        {
            const DWORD dwUnselectedClr = (m_uiItems[ i ]->IsVisible() ? CLR_MID_GRAY : CLR_DARK_GRAY);
            const DWORD dwClr = (i == m_uCurUIItem) ? CLR_CYAN : dwUnselectedClr;

            if( m_uiItems[ i ]->GetText( buf, ARRAYSIZE( buf ) ) )
            {
                m_Font.DrawText( 0, fParamY, dwClr, buf );
            }

            fParamY += 16;
        }

        m_Font.SetScaleFactors( 0.7f, 0.7f );
        m_Font.DrawText( 0, 20, CLR_WHITE, m_bOptimalAlloc ? L"  Optimal alloc" : L"  Default alloc" );

        if( !m_bSafetyUnlocked )
        {
            m_Font.DrawText( 480, 40, CLR_RED, L"Combination is unsafe" );
        }

        FLOAT   fInfoPaneBase = 170;

        // waste information
        const UINT  uBytesAllocated = (m_uBaseBytesAllocated + m_uMipsBytesAllocated);
        const UINT  uWasted = uBytesAllocated - (m_uBaseBytesWritten + m_uMipBytesWritten);
        if( m_pTextureMips )
        {
            swprintf_s( buf, ARRAYSIZE( buf ),
                        L"Alloc %dK, wasted %dK (%.2f%%) - 2 allocs\nbase %dK used out of %dK\nmips %dK used out of %dK",
                        uBytesAllocated / 1024,
                        uWasted / 1024,
                        100.f * static_cast< FLOAT >( uWasted ) / static_cast< FLOAT >( uBytesAllocated ),
                        m_uBaseBytesWritten / 1024, m_uBaseBytesAllocated / 1024,
                        m_uMipBytesWritten / 1024, m_uMipsBytesAllocated / 1024 );
        } else
        {
            swprintf_s( buf, ARRAYSIZE( buf ),
                        L"Alloc %dK, wasted %dK (%.2f%%) - contiguous alloc\nbase %dK used\nmips %dK used",
                        uBytesAllocated / 1024,
                        uWasted / 1024,
                        100.f * static_cast< FLOAT >( uWasted ) / static_cast< FLOAT >( uBytesAllocated ),
                        m_uBaseBytesWritten / 1024,
                        m_uMipBytesWritten / 1024 );
        }
        m_Font.DrawText( 0, fInfoPaneBase, CLR_WHITE, buf );

        // page reclamation info
        if( m_uNumUnusedPagesInTheMiddle + m_uNumUnusedPagesAtTheEnd > 0 )
        {
            swprintf_s( buf, ARRAYSIZE( buf ),
                        L"%d bytes in %d gaps",
                        (m_uNumUnusedPagesInTheMiddle + m_uNumUnusedPagesAtTheEnd) * m_gapAlignment.GetValue(),
                        m_uNumUnusedPagesInTheMiddle + m_uNumUnusedPagesAtTheEnd );
        } else
        {
            swprintf_s( buf, ARRAYSIZE( buf ), L"no gaps" );
        }
        m_Font.DrawText( 0, 50 + fInfoPaneBase, CLR_WHITE, buf );

        // pairing info
        if( m_pairUp.GetValue() )
        {
            if( m_pCantPairUpReason )
            {
                swprintf_s( buf, ARRAYSIZE( buf ), L"Can't pair up because %s", m_pCantPairUpReason );
                m_Font.DrawText( 0, 68 + fInfoPaneBase, CLR_RED, buf );
            } else
            {
                D3DSURFACE_DESC desc;
                m_textures[ 1 ].GetLevelDesc( 0, &desc );

                swprintf_s( buf, ARRAYSIZE( buf ), L"Tex A is paired up with %dx%d Tex B", desc.Width, desc.Height );
                m_Font.DrawText( 0, 68 + fInfoPaneBase, CLR_GREEN, buf );
            }
        }

        // textures legend
        if( m_bSecondTextureValid )
        {
            m_Font.DrawText( 90, 100 + fInfoPaneBase, CLR_WHITE, L"Tex A" );
            m_Font.DrawText( 320, 100 + fInfoPaneBase, CLR_WHITE, L"Tex B" );
        } else
        {
            m_Font.DrawText( 190, 100 + fInfoPaneBase, CLR_WHITE, L"Tex A" );
        }

        // mips legend
        {
            m_Font.DrawText( 0, TEX_VIEW_Y + TEX_VIEW_H + 4, CLR_WHITE, L"Tex A mip colors" );

            if( m_bSecondTextureValid )
            {
                m_Font.DrawText( 0, TEX_VIEW_Y + TEX_VIEW_H + 24, CLR_WHITE, L"Tex B mip colors" );
            }

            FLOAT   fX = 200;
            for( UINT i=0; i < 12; ++i )
            {
                swprintf_s( buf, ARRAYSIZE( buf ), L"%d", i );

                DWORD   dwClr;
                F3To8x4( &dwClr, GetMipColor( i, 0 ) );
                m_Font.DrawText( fX, 540, dwClr, buf );

                if( m_bSecondTextureValid )
                {
                    F3To8x4( &dwClr, GetMipColor( i, 1 ) );
                    m_Font.DrawText( fX, 560, dwClr, buf );
                }

                fX += i < 10 ? 14 : 20;
            }
        }

        m_Font.End();
    }

    // Present the scene
    m_pd3dDevice->Present( 0, 0, 0, 0 );
    m_pd3dDevice->UnsetAll();

    return S_OK;
}



//--------------------------------------------------------------------------------------
// Name: GetColorBlockForMipLevel
// Desc: Returns a texture block colored in requested color
//--------------------------------------------------------------------------------------
static
VOID   GetColorBlockForMipLevel( UINT& uBlockSizeInPixels, UINT& uBlockSizeInBytes,
                                    BYTE* pBlockData, UINT uMipLevel, D3DFORMAT fmt, UINT uTexNum )
{
    UINT   dummy;
    XGGetBlockDimensions( XGGetGpuFormat( fmt ), &uBlockSizeInPixels, &dummy );
    assert( uBlockSizeInPixels == dummy );

    uBlockSizeInBytes = XGBitsPerPixelFromFormat( fmt ) * uBlockSizeInPixels * uBlockSizeInPixels / 8;  // 8 bits in a byte

    switch( fmt )
    {
    default:    assert( !"incorrect format passed" );
                break;

    case D3DFMT_DXT1:
    case D3DFMT_LIN_DXT1:
        memset( pBlockData, 0x00, 8 );
        F3To565( &pBlockData[ 0 ], GetMipColor( uMipLevel, uTexNum ) );
        *((WORD*)&pBlockData[ 2 ]) = 0;
        break;

    case D3DFMT_DXT5:
    case D3DFMT_LIN_DXT5:
        memset( pBlockData, 0xff, 8 );
        memset( &pBlockData[ 8 ], 0x00, 8 );
        F3To565( &pBlockData[ 8 ], GetMipColor( uMipLevel, uTexNum ) );
        break;

    case D3DFMT_L8:
    case D3DFMT_LIN_L8:
        // we can't use rgb colour for one channel texture, so we make up
        // a 4 step interpolator to differentiate between mip levels
        *pBlockData = 255 - 200 * (uMipLevel & 0x03) / 4;
        break;

    case D3DFMT_R5G6B5:
    case D3DFMT_LIN_R5G6B5:
        F3To565( pBlockData, GetMipColor( uMipLevel, uTexNum ) );
        break;

    case D3DFMT_A8R8G8B8:
    case D3DFMT_LIN_A8R8G8B8:
        F3To8x4( pBlockData, GetMipColor( uMipLevel, uTexNum ) );
        break;

    case D3DFMT_A16B16G16R16:
    case D3DFMT_LIN_A16B16G16R16:
        F3To16x4( pBlockData, GetMipColor( uMipLevel, uTexNum ) );
        break;

    case D3DFMT_A32B32G32R32:
    case D3DFMT_LIN_A32B32G32R32:
        F3To32x4( pBlockData, GetMipColor( uMipLevel, uTexNum ) );
        break;
    }
}


//--------------------------------------------------------------------------------------
// Name: FillLevel
// Desc: Fills a texture mip level with a color, counts number of bytes written
//--------------------------------------------------------------------------------------
static
DWORD   FillLevel( DWORD* pBytesWritten, VOID* pBits, DWORD dwPitch, UINT uLevel,
                    const D3DSURFACE_DESC& desc, UINT uTexNum )
{
    UINT    uBlockSizeInPixels;
    UINT    uBlockSizeInBytes;
    BYTE    mipLevelcolorBlock[ 16 ];
    GetColorBlockForMipLevel( uBlockSizeInPixels, uBlockSizeInBytes, mipLevelcolorBlock, uLevel, desc.Format, uTexNum );

    const UINT uWidthInBlocks = static_cast< UINT >( ceilf( static_cast< FLOAT >( desc.Width ) / static_cast< FLOAT >( uBlockSizeInPixels ) ) );
    const UINT uHeightInBlocks = static_cast< UINT >( ceilf( static_cast< FLOAT >( desc.Height ) / static_cast< FLOAT >( uBlockSizeInPixels ) ) );
    const UINT uPitch = dwPitch ? dwPitch : (uWidthInBlocks * uBlockSizeInBytes);

    for( UINT k=0; k < uHeightInBlocks; ++k )
    {
        for( UINT j=0; j < uWidthInBlocks; ++j )
        {
            VOID*   pDest = reinterpret_cast< VOID* >( 
                                reinterpret_cast< UINT_PTR >( pBits )   +
                                    k * uPitch                          +
                                    j * uBlockSizeInBytes );

            memcpy( pDest, mipLevelcolorBlock, uBlockSizeInBytes );
        }
    }

    if( pBytesWritten )
    {
        *pBytesWritten = uBlockSizeInBytes * uWidthInBlocks * uHeightInBlocks;
    }

    return uPitch;
}


//--------------------------------------------------------------------------------------
// Name: GetTexturesLayout
// Desc: Fills two arrays with memory regions used by the texture and terminates them
//       with a region on the end. One array is for used regions in Base and another is
//       for Mips. Different uAlignment will return different regions.
//--------------------------------------------------------------------------------------
static
VOID    GetTexturesLayout(  XGLAYOUT_REGION* pOutBase, UINT& uNumOutSlotsBase,
                            XGLAYOUT_REGION* pOutMips, UINT& uNumOutSlotsMips,
                            UINT uAlignment, D3DTexture* pTexA, D3DTexture* pTexB,
                            UINT uBaseBytes, UINT uMipsBytes,
                            BOOL bContiguousMips )
{
    assert( pTexA );

    UINT       uBaseDataA, uMipsDataA;
    UINT uNumUsedBase = uNumOutSlotsBase;
    UINT uNumUsedMips = uNumOutSlotsMips;
    XGGetTextureLayout( pTexA,
                        &uBaseDataA, NULL, pOutBase, &uNumUsedBase, uAlignment,
                        &uMipsDataA, NULL, pOutMips, &uNumUsedMips, uAlignment );

    // offset each region of the base level by the base address of the texture
    for( UINT i=0; i < uNumUsedBase; ++i )
    {
        pOutBase[ i ].EndOffset += uBaseDataA;
        pOutBase[ i ].StartOffset += uBaseDataA;
    }

    // offset each region of the mip by the base address
    for( UINT i=0; i < uNumUsedMips; ++i )
    {
        pOutMips[ i ].EndOffset += uMipsDataA;
        pOutMips[ i ].StartOffset += uMipsDataA;
    }

    if( pTexB )
    {
        // retrieve regions for the second texture
        UINT    uBaseDataB, uMipsDataB;
        UINT    uNumBaseB = uNumOutSlotsBase - uNumUsedBase;    // making sure we don't overflow the output arrays
        UINT    uNumMipsB = uNumOutSlotsMips - uNumUsedMips;
        XGGetTextureLayout( pTexB,
                            &uBaseDataB, NULL, &pOutBase[ uNumUsedBase ], &uNumBaseB, uAlignment,
                            &uMipsDataB, NULL, &pOutMips[ uNumUsedMips ], &uNumMipsB, uAlignment );

        // offset each region of the base level of the second tex by the base address of the texture
        for( UINT i=0; i < uNumBaseB; ++i )
        {
#pragma warning( suppress: 6385 )
            pOutBase[ uNumUsedBase + i ].EndOffset += uBaseDataB;
            pOutBase[ uNumUsedBase + i ].StartOffset += uBaseDataB;
        }

        // offset the mip levels addresses
        for( UINT i=0; i < uNumMipsB; ++i )
        {
#pragma warning( suppress: 6385 )
            pOutMips[ uNumUsedMips + i ].EndOffset += uMipsDataB;
            pOutMips[ uNumUsedMips + i ].StartOffset += uMipsDataB;
        }

        uNumUsedBase += uNumBaseB;
        uNumUsedMips += uNumMipsB;
    }

    // add a block that spans between the end of texture and the end of memory
    if( uNumUsedBase )
    {
        assert( 1 + uNumUsedBase < uNumOutSlotsBase );

        pOutBase[ uNumUsedBase ].StartOffset = uBaseDataA + uBaseBytes;
        pOutBase[ uNumUsedBase ].EndOffset = 0xffffffff;
        ++uNumUsedBase;
    }

    // if we used contiguous mips don't terminate mips regions array
    // if we have no mips don't terminate either
    if( uNumUsedMips        &&
        !bContiguousMips    &&
        pTexA->GetLevelCount() > 1 )
    {
        assert( 1 + uNumUsedMips < uNumOutSlotsMips );

        // continuous mips use the same memory block as the base level
        pOutMips[ uNumUsedMips ].StartOffset = (bContiguousMips ? uBaseDataA : uMipsDataA) + uMipsBytes;
        pOutMips[ uNumUsedMips ].EndOffset = 0xffffffff;
        ++uNumUsedMips;
    }

    uNumOutSlotsBase = uNumUsedBase;
    uNumOutSlotsMips = uNumUsedMips;
}



//--------------------------------------------------------------------------------------
// Name: Coalesce
// Desc: Assuming A and B are sorted by StartOffset, coalesce the two regions
//--------------------------------------------------------------------------------------
static
BOOL    Coalesce( XGLAYOUT_REGION& regionA, const XGLAYOUT_REGION& regionB )
{
    // ensure sorted ranges
    assert( regionB.StartOffset >= regionA.StartOffset );

    // no relation?
    if( regionB.StartOffset > regionA.EndOffset )
        return FALSE;

    // A overlaps B or B contained in A
    regionA.EndOffset = max( regionA.EndOffset, regionB.EndOffset );

    return TRUE;
}



//--------------------------------------------------------------------------------------
// Name: SortAndCoalesce
// Desc: In place changes the given array to a sorted and coalesced array of regions
//--------------------------------------------------------------------------------------
static
UINT    SortAndCoalesce( XGLAYOUT_REGION* pUsedAreasInTexture, UINT uNumUsedAreas )
{
    if( !uNumUsedAreas )
        return 0;

    std::sort(  &pUsedAreasInTexture[ 0 ],
                &pUsedAreasInTexture[ uNumUsedAreas ],
                [] ( const XGLAYOUT_REGION& a, const XGLAYOUT_REGION& b ) { return a.StartOffset < b.StartOffset; } );

    // coalesce overlaps
    for( UINT i=0; i < uNumUsedAreas - 1; ++i )
    {
        for( UINT j=i + 1; j < uNumUsedAreas; ++j )
        {
            if( Coalesce( pUsedAreasInTexture[ i ], pUsedAreasInTexture[ j ] ) )
            {
                for( UINT k=j; k < uNumUsedAreas - 1; ++k )
                {
                    pUsedAreasInTexture[ k ] = pUsedAreasInTexture[ k + 1 ];
                }

                --j;
                --uNumUsedAreas;
            }
        }
    }

    return uNumUsedAreas;
}



//--------------------------------------------------------------------------------------
// Name: GenerateUnused
// Desc: Having used regions generates unused regions
//--------------------------------------------------------------------------------------
static
UINT    GenerateUnused( XGLAYOUT_REGION* pOut, UINT uMaxOut, const XGLAYOUT_REGION* usedAreasInTexture, UINT uNumUsedAreas )
{
    UINT    uNumOutputAreas = 0;
    UINT    uCurAddress = usedAreasInTexture[ 0 ].EndOffset;
    for( UINT i=1; i < uNumUsedAreas; ++i )
    {
        const UINT  uNextOccupiedAreaStart = usedAreasInTexture[ i ].StartOffset;
        if( uCurAddress < uNextOccupiedAreaStart )
        {
            assert( uNumOutputAreas < uMaxOut );

            pOut[ uNumOutputAreas ].StartOffset = uCurAddress;
            pOut[ uNumOutputAreas ].EndOffset = uNextOccupiedAreaStart;
            ++uNumOutputAreas;
        }

        uCurAddress = usedAreasInTexture[ i ].EndOffset;
    }

    return uNumOutputAreas;
}

//--------------------------------------------------------------------------------------
// Name: GetUnusedAreas
// Desc: Having two textures and the allocation info, fill Base and Mips unused regions
//--------------------------------------------------------------------------------------
static
VOID    GetUnusedAreas( XGLAYOUT_REGION* pOutBase, UINT& uNumOutSlotsBase,
                        XGLAYOUT_REGION* pOutMips, UINT& uNumOutSlotsMips,
                        UINT uAlignment,
                        D3DTexture* pTexA, D3DTexture* pTexB,
                        UINT uBaseBytes, UINT uMipsBytes,
                        BOOL bContiguousMips )
{
    // get used area
    XGLAYOUT_REGION usedAreasInBase[ 32 ] = { 0 };
    XGLAYOUT_REGION usedAreasInMips[ 32 ] = { 0 };
    UINT    uNumUsedInBase = ARRAYSIZE( usedAreasInBase );
    UINT    uNumUsedInMips = ARRAYSIZE( usedAreasInMips );
    GetTexturesLayout(  usedAreasInBase, uNumUsedInBase,
                        usedAreasInMips, uNumUsedInMips,
                        uAlignment,
                        pTexA, pTexB,
                        uBaseBytes, uMipsBytes,
                        bContiguousMips );

    if( !(uNumUsedInBase + uNumUsedInMips) )
    {
        uNumOutSlotsBase = 0;
        uNumOutSlotsMips = 0;
        return;
    }

    if( bContiguousMips )
    {
        assert( (uNumUsedInBase + uNumUsedInMips) <= ARRAYSIZE( usedAreasInBase ) );

        // merge mips lists into base list (mips list isn't terminated with an to-the-end-of-memory block)
        for( UINT i=0; i < uNumUsedInMips; ++i )
        {
            usedAreasInBase[ uNumUsedInBase++ ] = usedAreasInMips[ i ];
        }

        // merge blocks
        uNumUsedInBase = SortAndCoalesce( usedAreasInBase, uNumUsedInBase );

        // generate unused areas list
        uNumOutSlotsBase = GenerateUnused( pOutBase, uNumOutSlotsBase, usedAreasInBase, uNumUsedInBase );
        uNumOutSlotsMips = 0;
    } else
    {
        // merge blocks
        uNumUsedInBase = SortAndCoalesce( usedAreasInBase, uNumUsedInBase );
        uNumUsedInMips = SortAndCoalesce( usedAreasInMips, uNumUsedInMips );

        // generate unused areas list
        uNumOutSlotsBase = GenerateUnused( pOutBase, uNumOutSlotsBase, usedAreasInBase, uNumUsedInBase );
        uNumOutSlotsMips = GenerateUnused( pOutMips, uNumOutSlotsMips, usedAreasInMips, uNumUsedInMips );
    }

    // coalesce unused regions
    uNumUsedInBase = SortAndCoalesce( usedAreasInBase, uNumUsedInBase );
    uNumUsedInMips = SortAndCoalesce( usedAreasInMips, uNumUsedInMips );
}



//--------------------------------------------------------------------------------------
// Name: CountGaps
// Desc: Tally up unused regions
//--------------------------------------------------------------------------------------
static
VOID    CountGaps( UINT& uNumUnusedPagesInTheMiddle, UINT& uNumUnusedPagesAtTheEnd,
                    const XGLAYOUT_REGION* pUnusedAreas, UINT uNumUnusedAreas, UINT uEnd, UINT uGapAlignment )
{
    for( UINT i=0; i < uNumUnusedAreas; ++i )
    {
        const UINT  uNumPages = (pUnusedAreas[ i ].EndOffset - pUnusedAreas[ i ].StartOffset) / uGapAlignment;

        if( pUnusedAreas[ i ].EndOffset == uEnd )
        {
            uNumUnusedPagesAtTheEnd += uNumPages;
        } else
        {
            uNumUnusedPagesInTheMiddle += uNumPages;
        }
    }
}



//--------------------------------------------------------------------------------------
// Name: CreateTextureViews
// Desc: Generates either one texture to view both Base and Mips or two separate views
//--------------------------------------------------------------------------------------
VOID    Sample::CreateTextureViews()
{
    D3DSURFACE_DESC desc;
    m_textures[ 0 ].GetLevelDesc( 0, &desc );

    UINT    uBlockSx, uBlockSy;
    XGGetBlockDimensions( XGGetGpuFormat( desc.Format ), &uBlockSx, &uBlockSy );
    const DWORD   dwBitsPerPixel = XGBitsPerPixelFromFormat( desc.Format );

    const UINT  uBytesPerBlock = (uBlockSx * uBlockSy * dwBitsPerPixel / 8);
    const UINT  uWidthInBlocksUnaligned = static_cast< UINT >( ceilf( static_cast< FLOAT >( desc.Width ) / static_cast< FLOAT >( uBlockSx ) ) );
    const UINT  uWidthInBlocksTileAligned = XGNextMultiple( uWidthInBlocksUnaligned, GPU_TEXTURE_TILE_DIMENSION );
    const UINT  uWidthInBlocks = XGNextMultiple( uWidthInBlocksTileAligned * uBytesPerBlock, GPU_LINEAR_TEXTURE_PITCH_BYTE_ALIGNMENT ) / uBytesPerBlock;

    if( m_pTextureMips )
    {
        const UINT    uNumTotalBlocksBase = m_uBaseBytesAllocated / uBytesPerBlock;
        const UINT    uNewTextureHeightBase = uBlockSy * uNumTotalBlocksBase / uWidthInBlocks;
        XGSetTextureHeader( uWidthInBlocks * uBlockSx, uNewTextureHeightBase, 1,
                            0,
                            (D3DFORMAT)MAKELINFMT( desc.Format ),
                            0,
                            0,
                            0,
                            0,
                            &m_texMemoryViewBase,
                            NULL,
                            NULL );
        XGOffsetResourceAddress( &m_texMemoryViewBase, m_pTextureBase );

        const UINT    uNumTotalBlocksMips = m_uMipsBytesAllocated / uBytesPerBlock;
        const UINT    uNewTextureHeightMips = uBlockSy * uNumTotalBlocksMips / uWidthInBlocks;
        XGSetTextureHeader( uWidthInBlocks * uBlockSx, uNewTextureHeightMips, 1,
                            0,
                            (D3DFORMAT)MAKELINFMT( desc.Format ),
                            0,
                            0,
                            0,
                            0,
                            &m_texMemoryViewMips,
                            NULL,
                            NULL );
        XGOffsetResourceAddress( &m_texMemoryViewMips, m_pTextureMips );
    } else
    {
        const UINT    uNumTotalBlocks = (m_uBaseBytesAllocated + m_uMipsBytesAllocated) / uBytesPerBlock;
        const UINT    uNewTextureHeight = uBlockSy * uNumTotalBlocks / uWidthInBlocks;
        XGSetTextureHeader( uWidthInBlocks * uBlockSx, uNewTextureHeight, 1,
                            0,
                            (D3DFORMAT)MAKELINFMT( desc.Format ),
                            0,
                            0,
                            0,
                            0,
                            &m_texMemoryViewBase,
                            NULL,
                            NULL );
        XGOffsetResourceAddress( &m_texMemoryViewBase, m_pTextureBase );
    }
}


//--------------------------------------------------------------------------------------
// Name: TileFlagsFromTex
// Desc: XGTileTextureLevel needs correct flags so this function returns them
//--------------------------------------------------------------------------------------
static
DWORD   TileFlagsFromTex( D3DTexture* pTex )
{
    DWORD   dwFlags = 0;

    if( !XGIsPackedTexture( pTex ) )
        dwFlags |= XGTILE_NONPACKED;

    if( XGIsBorderTexture( pTex ) )
        dwFlags |= XGTILE_BORDER;

    return dwFlags;
}



//--------------------------------------------------------------------------------------
// Name: FillMipmapsAndCountWrittenBytes
// Desc: Handles filling up of TexA and TexB's mips with colors, tiles if necessary
//       returns FALSE if it runs out of memory, that's the only error we expect
//--------------------------------------------------------------------------------------
HRESULT Sample::FillMipmapsAndCountWrittenBytes()
{
    D3DSURFACE_DESC desc;
    m_textures[ 0 ].GetLevelDesc( 0, &desc );

    m_uBaseBytesWritten = 0;
    m_uMipBytesWritten = 0;

    // clear with 0
    memset( m_pTextureBase, 0, m_uBaseBytesAllocated );
    if( m_pTextureMips )
    {
        memset( m_pTextureMips, 0, m_uMipsBytesAllocated );
    }

    // allocate a conservatively sized scratch buffer
    const UINT  uScratchSize = desc.Width * desc.Height * 16;
    BYTE*       pScratch = new BYTE[ uScratchSize ];
    if( !pScratch )
        return E_OUTOFMEMORY;

    for( UINT j=0; j <= (UINT)m_bSecondTextureValid; ++j )
    {
        const UINT uNumLevels = m_textures[ j ].GetLevelCount();

        D3DSURFACE_DESC desc0;
        m_textures[ j ].GetLevelDesc( 0, &desc0 );

        for( UINT i=0; i < uNumLevels; ++i )
        {
            D3DSURFACE_DESC descLevel;
            m_textures[ j ].GetLevelDesc( i, &descLevel );

            D3DLOCKED_RECT  rc;
            m_textures[ j ].LockRect( i, &rc, NULL, 0 );

            DWORD   uNumBytesUsed;
            if( XGIsTiledFormat( descLevel.Format ) )
            {
                DWORD   uScratchPitch = FillLevel( &uNumBytesUsed, pScratch, 0, i, descLevel, j );
                XGTileTextureLevel( desc0.Width, desc0.Height, i,
                                    XGGetGpuFormat( desc0.Format ),
                                    TileFlagsFromTex( &m_textures[ j ] ),
                                    rc.pBits, NULL,
                                    pScratch, uScratchPitch, NULL );
            } else
            {
                FillLevel( &uNumBytesUsed, rc.pBits, rc.Pitch, i, descLevel, j );
            }

            m_textures[ j ].UnlockRect( i );

            if( i == 0 )
            {
                m_uBaseBytesWritten += uNumBytesUsed;
            } else
            {
                m_uMipBytesWritten += uNumBytesUsed;
            }
        }
    }

    delete[] pScratch;

    return S_OK;
}



//--------------------------------------------------------------------------------------
// Name: OptimiseAllocationParameters
// Desc: If an unused region happens to be at the end of a memory block for base or
//       mips memory area, we can just allocate less. This function takes two textures,
//       memory allocation info, and adjusts allocation sizes so they are optimal.
//--------------------------------------------------------------------------------------
static
VOID    OptimiseAllocationParameters( D3DTexture* pTexA, D3DTexture* pTexB, UINT& uBaseBytes, UINT& uMipsBytes, BOOL bContiguousAlloc )
{
    XGLAYOUT_REGION base[ 32 ];
    XGLAYOUT_REGION mips[ 32 ];
    
    UINT    uNumBase = ARRAYSIZE( base );
    UINT    uNumMips = ARRAYSIZE( mips );

    GetUnusedAreas( base, uNumBase,
                    mips, uNumMips,
                    4096,
                    pTexA, pTexB,
                    uBaseBytes, uMipsBytes,
                    bContiguousAlloc );

    UINT    uCutOffBaseEnd = 0;
    UINT    uCutOffMipsEnd = 0;

    for( UINT i=0; i < uNumBase; ++i )
    {
        if( base[ i ].EndOffset == uBaseBytes )
        {
            uCutOffBaseEnd += base[ i ].EndOffset - base[ i ].StartOffset;
        }
    }

    for( UINT i=0; i < uNumMips; ++i )
    {
        if( mips[ i ].EndOffset == uMipsBytes )
        {
            uCutOffMipsEnd += mips[ i ].EndOffset - mips[ i ].StartOffset;;
        }
    }

    uBaseBytes -= uCutOffBaseEnd;
    uMipsBytes -= uCutOffMipsEnd;
}


//--------------------------------------------------------------------------------------
// Name: RebuildTextures
// Desc: Sets up paired up or a single texture, fills its mips with colors, creates
//       texture views, generates unused memory regions information
//--------------------------------------------------------------------------------------
HRESULT Sample::RebuildTextures( UINT uTexWidth, UINT uTexHeight, UINT uNumMips, D3DFORMAT texFormat, UINT uPairedMaxSize, BOOL bOptimalAlloc, UINT uGapAlignment, BOOL bPackedTails )
{
    m_bSafetyUnlocked = SafetyCheck( uTexWidth, uTexHeight, uNumMips, texFormat, bOptimalAlloc );
    if( !m_bSafetyUnlocked )
        return S_OK;

    // make sure the GPU isn't using our textures before we start messing about
    m_pd3dDevice->BlockUntilIdle();

    if( m_pTextureBase )
    {
        XPhysicalFree( m_pTextureBase );
        m_pTextureBase = NULL;
        m_uBaseBytesAllocated = 0;
    }

    if( m_pTextureMips )
    {
        XPhysicalFree( m_pTextureMips );
        m_pTextureMips = NULL;
        m_uMipsBytesAllocated = 0;
    }

    // work out the num mipmaps cap
    uNumMips = ( uNumMips >= MAX_NUMBER_OF_MIPLEVELS ) ? 0 : min( uNumMips, 1 + Log2( max( uTexWidth, uTexHeight ) ) );

    // optionally don't pack mip tails -- generally a bad idea
    const UINT  uFlags = bPackedTails ? 0 : XGHEADEREX_NONPACKED;

    // check if it's possible to pair up the given texture
    m_pCantPairUpReason = NULL;
    m_bSecondTextureValid = FALSE;

    if( uPairedMaxSize )
    {
        if( !XGIsCompressedFormat( texFormat ) )
        {
            m_pCantPairUpReason = L"can only pair up compressed formats";
        } else if( NextPow2( uTexWidth ) != NextPow2( uTexHeight ) )
        {
            m_pCantPairUpReason = L"should be square in NextPow2";
        } else if( !bPackedTails )
        {
            m_pCantPairUpReason = L"Non-packed textures cannot currently be paired";
        }
        
        if( m_pCantPairUpReason )
        {
            uPairedMaxSize = 0;
        }
    }

    // can only pair up with packed tails, remove the UI option
    m_packedTails.SetVisible( !uPairedMaxSize );

    // create a weaved texture layout if can do and requested
    if( uPairedMaxSize )
    {
        // choose second texture's parameters so we can weave it in
        UINT    uSecondTextureWidth = min( uPairedMaxSize, NextPow2( uTexWidth ) );
        UINT    uSecondTextureHeight = min( uPairedMaxSize, NextPow2( uTexHeight ) );

        m_uBaseBytesAllocated = XGSetTextureHeaderPair( 0,                      //UINT BaseOffset,
                                XGHEADER_CONTIGUOUS_MIP_OFFSET, //UINT MipOffset,
                                uTexWidth,              //UINT Width1,
                                uTexHeight,             //UINT Height1,
                                uNumMips,               //UINT Levels1,
                                0,                      //DWORD Usage1,
                                texFormat,              //D3DFORMAT Format1,
                                0,                      //INT ExpBias1,
                                uFlags,                 //DWORD Flags1,
                                0,                      //UINT Pitch1,
                                &m_textures[ 0 ],       //IDirect3DTexture9 *pTexture1,
                                uSecondTextureWidth,    //UINT Width2,
                                uSecondTextureHeight,   //UINT Height2,
                                0,                      //UINT Levels2,
                                0,                      //DWORD Usage2,
                                texFormat,              //D3DFORMAT Format2,
                                0,                      //INT ExpBias2,
                                uFlags,                 //DWORD Flags2,
                                0,                      //UINT Pitch2,
                                &m_textures[ 1 ],       //IDirect3DTexture9 *pTexture2,
                                NULL,                   //UINT *pBaseSize,
                                NULL                    //UINT *pMipSize
                        );

        m_uMipsBytesAllocated = 0;

        if( bOptimalAlloc )
        {
            OptimiseAllocationParameters( &m_textures[ 0 ], &m_textures[ 1 ], m_uBaseBytesAllocated, m_uMipsBytesAllocated, TRUE );
        }

        m_pTextureBase = XPhysicalAlloc( m_uBaseBytesAllocated, MAXULONG_PTR, 0, PAGE_READWRITE | PAGE_WRITECOMBINE );
        if( !m_pTextureBase )
            return E_OUTOFMEMORY;

        XGOffsetResourceAddress( &m_textures[ 0 ], m_pTextureBase );
        XGOffsetResourceAddress( &m_textures[ 1 ], m_pTextureBase ); 

        m_bSecondTextureValid = TRUE;
    } else
    {
        XGSetTextureHeaderEx( uTexWidth,                        //UINT Width,
                            uTexHeight,                         //UINT Height,
                            uNumMips,                           //UINT Levels,
                            0,                                  //DWORD Usage,
                            texFormat,                          //D3DFORMAT Format,
                            0,                                  //INT ExpBias,
                            uFlags,                             //DWORD Flags,
                            0,                                  //UINT BaseOffset,
                            0,                                  //UINT MipOffset,
                            0,                                  //UINT Pitch,
                            &m_textures[ 0 ],                   //IDirect3DTexture9 *pTexture,
                            &m_uBaseBytesAllocated,             //UINT *pBaseSize,
                            &m_uMipsBytesAllocated );           //UINT *pMipSize
                                                                
        if( bOptimalAlloc )
        {
            OptimiseAllocationParameters( &m_textures[ 0 ], NULL, m_uBaseBytesAllocated, m_uMipsBytesAllocated, FALSE );
        }

        m_pTextureBase = XPhysicalAlloc( m_uBaseBytesAllocated, MAXULONG_PTR, 0, PAGE_READWRITE | PAGE_WRITECOMBINE );
        if( !m_pTextureBase )
            return E_OUTOFMEMORY;

        if( m_uMipsBytesAllocated )
        {
            m_pTextureMips = XPhysicalAlloc( m_uMipsBytesAllocated, MAXULONG_PTR, 0, PAGE_READWRITE | PAGE_WRITECOMBINE );
        }

        XGOffsetBaseTextureAddress( &m_textures[ 0 ], m_pTextureBase, m_pTextureMips );
    }

    // fill mips with happy colors and count how many bytes we wrote
    HRESULT hr = FillMipmapsAndCountWrittenBytes();
    if( FAILED( hr ) )
        return hr;

    // set sampler state
    for( UINT i=0; i < ARRAYSIZE( m_textures ); ++i )
    {
        XGSetSamplerFilterStates( &m_textures[ i ], D3DTEXF_LINEAR, D3DTEXF_LINEAR, D3DTEXF_LINEAR, 1 );
        XGSetSamplerAddressStates( &m_textures[ i ], D3DTADDRESS_WRAP, D3DTADDRESS_WRAP, D3DTADDRESS_WRAP );
    }

    // create a texture view for base and for mips if present
    CreateTextureViews();

    // collect gaps information
    m_uNumUnusedAreasInBase = ARRAYSIZE( m_unusedAreasInBase );
    m_uNumUnusedAreasInMips = ARRAYSIZE( m_unusedAreasInMips );
    GetUnusedAreas( m_unusedAreasInBase,
                    m_uNumUnusedAreasInBase,
                    m_unusedAreasInMips,
                    m_uNumUnusedAreasInMips,
                    uGapAlignment,
                    &m_textures[ 0 ],
                    m_bSecondTextureValid ? &m_textures[ 1 ] : NULL,
                    m_uBaseBytesAllocated,
                    m_uMipsBytesAllocated,
                    !m_pTextureMips );

    const UINT  uEndOfBase = (m_uBaseBytesAllocated + (UINT)m_pTextureBase);
    const UINT  uEndOfMips = m_pTextureMips ? ((m_uMipsBytesAllocated + (UINT)m_pTextureMips)) : 0;

    m_uNumUnusedPagesInTheMiddle = 0;
    m_uNumUnusedPagesAtTheEnd = 0;

    CountGaps( m_uNumUnusedPagesInTheMiddle, m_uNumUnusedPagesAtTheEnd, m_unusedAreasInBase, m_uNumUnusedAreasInBase, uEndOfBase, uGapAlignment );
    CountGaps( m_uNumUnusedPagesInTheMiddle, m_uNumUnusedPagesAtTheEnd, m_unusedAreasInMips, m_uNumUnusedAreasInMips, uEndOfMips, uGapAlignment );

    return S_OK;
}



//-----------------------------------------------------------------------------
// Name: RunTestBatch
// Desc: Outputs a CSV file with data on memory allocations for most interesting
//       texture size combinations. This function takes a long time in Debug
//-----------------------------------------------------------------------------
VOID    Sample::RunTestBatch()
{
    FILE* fp;
    fopen_s( &fp, "E:\\TextureMemoryLoss.csv", "wt" );

    if( fp )
        fwprintf( fp, L"TEST_NUMBER, WIDTH, HEIGHT, NUM_MIPS, FORMAT, PAIR_UP, BASE_BYTES, MIPS_BYTES, WASTE_BYTES, OPT_BASE_BYTES, OPT_MIPS_BYTES, OPT_WASTE_BYTES\n" );

    UINT    uTestNumber = 0;

    for( UINT uLogWidth = 1; uLogWidth < 11; ++uLogWidth )
    {
        for( UINT uLogHeight = 1; uLogHeight < 11; ++uLogHeight )
        {
            for( UINT uFormatIndex = 0; uFormatIndex < ARRAYSIZE( UITextureFormatItem::s_tfFormats ); ++uFormatIndex )
            {
                // need to present the screen from time to time
                Render();

                for( UINT uNumMips = 0; uNumMips < max( uLogWidth, uLogHeight ); ++uNumMips )
                {
                    // no pairing possible if not square anyway so don't bother
                    const UINT  uTopPairUp = (uLogHeight == uLogWidth) ? (128 + 64) : 64;

                    for( UINT uPairedMaxSize = 0; uPairedMaxSize < uTopPairUp; uPairedMaxSize += 64 )
                    {
                        const UINT  uWidth = 1 << uLogWidth;
                        const UINT  uHeight = 1 << uLogHeight;

                        // first try unoptimal alloc
                        HRESULT hr;
                        hr = RebuildTextures( uWidth, uHeight, uNumMips, UITextureFormatItem::s_tfFormats[ uFormatIndex ], uPairedMaxSize, FALSE, 4096, TRUE );
                        if( FAILED( hr ) )
                            continue;

                        const UINT  uUnoptimalBaseBytes = m_uBaseBytesAllocated;
                        const UINT  uUnoptimalMipsBytes = m_uMipsBytesAllocated;
                        const UINT  uUnoptimalUnusedBytes = (m_uNumUnusedPagesInTheMiddle + m_uNumUnusedPagesAtTheEnd) * 4096;

                        // then optimal alloc
                        hr = RebuildTextures( uWidth, uHeight, uNumMips, UITextureFormatItem::s_tfFormats[ uFormatIndex ], uPairedMaxSize, TRUE, 4096, TRUE );
                        if( FAILED( hr ) )
                            continue;

                        // print stats
                        if( fp )
                        {
                            if( m_bSafetyUnlocked )
                            {
                                fwprintf(   fp,
                                            L"%d,%d,%d,%d,%s,%d,%d,%d,%d,%d,%d,%d\n",
                                            uTestNumber, uWidth, uHeight, uNumMips, UITextureFormatItem::s_tfNames[ uFormatIndex ], uPairedMaxSize,
                                            uUnoptimalBaseBytes, uUnoptimalMipsBytes, uUnoptimalUnusedBytes,
                                            m_uBaseBytesAllocated, m_uMipsBytesAllocated, (m_uNumUnusedPagesInTheMiddle + m_uNumUnusedPagesAtTheEnd) * 4096 );
                            } else
                            {
                                fwprintf(   fp,
                                            L"%d,%d,%d,%d,%s,%d,%d,%d,%d,%d,%d,%d\n",
                                            uTestNumber, uWidth, uHeight, uNumMips, UITextureFormatItem::s_tfNames[ uFormatIndex ], uPairedMaxSize,
                                            -1, -1, -1,
                                            -1, -1, -1 );
                            }
                        }

                        ++uTestNumber;
                    }
                }
            }
        }
    }

    if( fp )
        fclose( fp );

    RebuildTextures();
}

//-----------------------------------------------------------------------------
// Name: main()
// Desc: Entry point to the program.
//-----------------------------------------------------------------------------
INT __cdecl main()
{
    DmMapDevkitDrive();

    Sample atgApp;
    ATG::GetVideoSettings( &atgApp.m_d3dpp.BackBufferWidth, &atgApp.m_d3dpp.BackBufferHeight );

    // Use fixed back buffer resolution regardless of output dimensions
    atgApp.m_d3dpp.BackBufferWidth = 1280;
    atgApp.m_d3dpp.BackBufferHeight = 720;
    atgApp.m_d3dpp.FrontBufferFormat = static_cast< D3DFORMAT >( MAKESRGBFMT( D3DFMT_LE_X8R8G8B8 ) );
    atgApp.m_d3dpp.EnableAutoDepthStencil = FALSE;

    atgApp.Run();
}



//--------------------------------------------------------------------------------------
// Name: SafetyCheck
// Desc: This function guards against XGGetTextureLayout
//--------------------------------------------------------------------------------------
BOOL    Sample::SafetyCheck( UINT uTexWidth, UINT uTexHeight, UINT uNumMips, D3DFORMAT texFormat, BOOL bOptimalAlloc )
{
    if( m_bSafetyOverride )
    {
        m_bSafetyOverride = FALSE;
        return TRUE;
    }

    if( bOptimalAlloc )
    {
        // hopefully will be fixed later
        if( XGIsCompressedFormat( texFormat )       &&
            (uTexWidth >= 32 && uTexHeight >= 32)   &&
            ((uTexWidth % 4) || (uTexHeight % 4)) )
        {
            return FALSE;
        }
    }

    return TRUE;
}

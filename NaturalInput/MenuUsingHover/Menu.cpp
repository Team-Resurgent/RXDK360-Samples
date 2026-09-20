//--------------------------------------------------------------------------------------
// Menu.cpp
//
// This sample demonstrates the use of Hover
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include <xtl.h>
#include <xnamath.h>
#include <xffb.h>
#include <xaudio2.h>
#include <AtgApp.h>
#include <AtgFont.h>
#include <AtgInput.h>
#include <AtgMesh.h>
#include <AtgResource.h>
#include <AtgUtil.h>
#include <AtgHelp.h>
#include <AtgSimpleShaders.h>
#include <AtgAudio.h>

#include "CameraManager.h"
#include "nuihandles.h"

static CONST FLOAT g_fTransparent = 0.6f;
static CONST INT g_iCursorSize = 80;
static CONST INT g_iMenuItems = 4;

NUI_HANDLES_ARMS g_HandlesArms;

struct WidthHeight
{
    INT Width;
    INT Height;
};

LPDIRECT3DVERTEXSHADER9      g_pVertexShader = NULL;
LPDIRECT3DPIXELSHADER9       g_pPixelShader = NULL;
LPDIRECT3DPIXELSHADER9       g_pPixelShaderColor = NULL;
LPDIRECT3DVERTEXDECLARATION9 g_pVertexDeclaration = NULL;


static CONST WidthHeight g_BackButtonLocation = { 1100, 100 };
static CONST WidthHeight g_MenuLocation = { 640, 360 };
static CONST WidthHeight g_StartButtonLocation = { 180, 620 };
static CONST WidthHeight g_ScrollLeftButtonLocation = { 440, 620 };
static CONST WidthHeight g_ScrollRightButtonLocation = { 840, 620 };
static CONST WidthHeight g_ScrollUpButtonLocation = { 640, 520 };

static CONST INT g_iSpaceBetweenMenuItems = 15;

// The background color is what you can modify with handles.
static CONST INT g_iTotalColors = 20;
D3DCOLOR g_BackGroundColors[ g_iTotalColors ];

INT g_iCurrentPositionInColorList1      = g_iTotalColors / 2;
FLOAT g_fCurrentPositionInColorList1    = 0.505f;
INT g_iCurrentPositionInColorList2      = g_iTotalColors / 2;
FLOAT g_fCurrentPositionInColorList2    = 0.505f;

D3DCOLOR g_GlobalBackgroundColor1   = 0xff3f6385;
D3DCOLOR g_GlobalBackgroundColor2   = 0xffcebdad;
D3DCOLOR g_CursorOnRailColor[2]     = { 0xffFA883F, 0xff91C942 };

XMFLOAT2 g_fLeftHandCursor = XMFLOAT2( 0.0f, 0.0f );
XMFLOAT2 g_fRightHandCursor = XMFLOAT2( 0.0f, 0.0f );

FLOAT g_fTimeLeftHovering = 0.0f;
FLOAT g_fTimeRightHovering = 0.0f;

FLOAT g_fElapsedTime = 0.0f;

static CONST FLOAT g_fHoverTimeToSubmit = 2.0f;

struct MeshVertexPT
{
public:
    XMFLOAT3 Position;
    XMFLOAT2 TexCoord;
    static size_t Size()
    {
        return sizeof( MeshVertexPT );
    }
};
D3DDevice *g_pSampleDevice;

//--------------------------------------------------------------------------------------
// Name: DrawImage()
// Desc: Draw a sprite with transparency and color
//--------------------------------------------------------------------------------------
VOID DrawImage( D3DRECT Rect, FLOAT fTransparent, D3DCOLOR color, LPDIRECT3DTEXTURE9 pTexture  )
{
    MeshVertexPT* pVertexData = NULL;
    g_pSampleDevice->SetRenderState( D3DRS_ZENABLE, FALSE );
    g_pSampleDevice->SetRenderState( D3DRS_ZWRITEENABLE, FALSE );
    g_pSampleDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_CCW );
    g_pSampleDevice->SetRenderState( D3DRS_VIEWPORTENABLE, FALSE );
    g_pSampleDevice->SetSamplerState( 0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP );
    g_pSampleDevice->SetSamplerState( 0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP );

    g_pSampleDevice->SetTexture( 0, pTexture );

    D3DXCOLOR d3dxColor = color;
    d3dxColor.a = fTransparent;
    g_pSampleDevice->SetPixelShaderConstantF( 0, d3dxColor, 1 );

    g_pSampleDevice->SetPixelShader( g_pPixelShader );
    g_pSampleDevice->SetVertexShader( g_pVertexShader );
    g_pSampleDevice->SetVertexDeclaration( g_pVertexDeclaration );

    g_pSampleDevice->BeginVertices( D3DPT_RECTLIST, 3, sizeof( MeshVertexPT ), ( VOID** )&pVertexData );    
    assert( pVertexData != NULL );
    pVertexData[0].Position = XMFLOAT3( ( FLOAT )Rect.x1, ( FLOAT )Rect.y1, 0 );
    pVertexData[0].TexCoord = XMFLOAT2( 0, 0 );
    pVertexData[1].Position = XMFLOAT3( ( FLOAT )Rect.x2, ( FLOAT )Rect.y1, 0 );
    pVertexData[1].TexCoord = XMFLOAT2( 1, 0 );
    pVertexData[2].Position = XMFLOAT3( ( FLOAT )Rect.x1, ( FLOAT )Rect.y2, 0 );
    pVertexData[2].TexCoord = XMFLOAT2( 0, 1 );
    g_pSampleDevice->EndVertices();

    g_pSampleDevice->SetRenderState( D3DRS_VIEWPORTENABLE, TRUE );
    g_pSampleDevice->SetRenderState( D3DRS_ZENABLE, TRUE );
    g_pSampleDevice->SetRenderState( D3DRS_ZWRITEENABLE, TRUE );
            
}

struct HoverButton
{   

//--------------------------------------------------------------------------------------
// Name: Init()
// Desc: setup the button 
//--------------------------------------------------------------------------------------
    VOID Init(
        LPDIRECT3DTEXTURE9 pSelectedTexture,
        LPDIRECT3DTEXTURE9 pUnSelectedTexture,
        WidthHeight DrawLocation )
    {
        m_pTextures[0] = pSelectedTexture;
        if ( pUnSelectedTexture == NULL ) 
        {   
            m_pTextures[1] = pSelectedTexture;
        }
        else
        { 
            m_pTextures[1] = pUnSelectedTexture;
        }
        D3DSURFACE_DESC SurfaceDESC;
        m_pTextures[0]->GetLevelDesc( 0, &SurfaceDESC );
        m_ButtonDims.Width = (INT) SurfaceDESC.Width;
        m_ButtonDims.Height = (INT) SurfaceDESC.Height;
        WidthHeight halfDims;
        halfDims.Width = m_ButtonDims.Width / 2;
        halfDims.Height = m_ButtonDims.Height / 2;
        m_DrawLocation.x1 = DrawLocation.Width - halfDims.Width;
        m_DrawLocation.x2 = DrawLocation.Width + halfDims.Width;
        m_DrawLocation.y1 = DrawLocation.Height - halfDims.Height;
        m_DrawLocation.y2 = DrawLocation.Height + halfDims.Height;
        Reset();
    };
    
//--------------------------------------------------------------------------------------
// Name: Reset()
// Desc: reset all the state.
//--------------------------------------------------------------------------------------
    VOID Reset()
    {
        m_fTimeAsSelectedRight = 0.0f;
        m_fTimeAsSelectedLeft = 0.0f;
        m_bSelected = FALSE;
        m_bRightCursorHovering = FALSE;
        m_bLeftCursorHovering = FALSE;
    }

//--------------------------------------------------------------------------------------
// Name: Update Selected()
// Desc: test the hands against the button.  update the hover radius when a button is selected.
//--------------------------------------------------------------------------------------
    BOOL UpdateSelected( )
    {
        m_bSelected = FALSE;
        BOOL bRightHandIntersected = TRUE;
        BOOL bLeftHandIntersected = TRUE;
        // do the collision tests for both hands.
        if ( g_fRightHandCursor.x < m_DrawLocation.x1 || g_fRightHandCursor.x > m_DrawLocation.x2
            || g_fRightHandCursor.y < m_DrawLocation.y1 || g_fRightHandCursor.y > m_DrawLocation.y2 )
        {
            bRightHandIntersected = FALSE;
        }
        if ( g_fLeftHandCursor.x < m_DrawLocation.x1 || g_fLeftHandCursor.x > m_DrawLocation.x2
            || g_fLeftHandCursor.y < m_DrawLocation.y1 || g_fLeftHandCursor.y > m_DrawLocation.y2 )
        {
            bLeftHandIntersected = FALSE;
        }

        // We're allready selected. Increment the time and test to see if we selected
        if ( bLeftHandIntersected && m_fTimeAsSelectedLeft > 0 )
        {
            m_fTimeAsSelectedLeft += g_fElapsedTime;
            if ( m_fTimeAsSelectedLeft > g_fHoverTimeToSubmit ) 
            {
                m_bSelected = TRUE; 
                m_bLeftCursorHovering = FALSE;
                g_fTimeLeftHovering = 0.0f;
            }
            else
            {
                g_fTimeLeftHovering = m_fTimeAsSelectedLeft;
            }
        }
        // we're newly selected, reset the time as selected
        else if ( bLeftHandIntersected )
        {
            m_fTimeAsSelectedLeft = g_fElapsedTime;
            g_fTimeLeftHovering = m_fTimeAsSelectedLeft;
            m_bLeftCursorHovering = TRUE;
        }
        else 
        {
            m_bLeftCursorHovering = FALSE;
            m_fTimeAsSelectedLeft = 0.0f;
        }

        if ( bRightHandIntersected && m_fTimeAsSelectedRight > 0 )
        {
            m_fTimeAsSelectedRight += g_fElapsedTime;
            if ( m_fTimeAsSelectedRight > g_fHoverTimeToSubmit ) 
            {
                m_bSelected = TRUE;
                m_bRightCursorHovering = FALSE;
                g_fTimeRightHovering = 0.0f;
            }
            else
            {
                g_fTimeRightHovering = m_fTimeAsSelectedRight;
            }
        }
        else if ( bRightHandIntersected )
        {
            m_fTimeAsSelectedRight = g_fElapsedTime;
            g_fTimeRightHovering = m_fTimeAsSelectedRight;
            m_bRightCursorHovering = TRUE;
        }
        else 
        {
            m_bRightCursorHovering = FALSE;
            m_fTimeAsSelectedRight = 0.0f;
        }

        return m_bSelected;
    };
//--------------------------------------------------------------------------------------
// Name: DrawButton()
// Desc: render the button based on weather it's active or not..
//--------------------------------------------------------------------------------------
    VOID DrawButton()
    {
        if ( m_bLeftCursorHovering || m_bRightCursorHovering ) 
        {
            DrawImage( m_DrawLocation, 1.0f, 0xFF00FFFF, m_pTextures[ 1 ] ); 
        }
        else 
        {
            DrawImage( m_DrawLocation, g_fTransparent, 0xFF00FFFF, m_pTextures[ 0 ] ); 
        }
    }
    

    LPDIRECT3DTEXTURE9 m_pTextures[2];
    WidthHeight m_ButtonDims;
    D3DRECT m_DrawLocation;
    FLOAT m_fTimeAsSelectedRight;
    FLOAT m_fTimeAsSelectedLeft;
    BOOL m_bSelected;
    BOOL m_bLeftCursorHovering;
    BOOL m_bRightCursorHovering;
};

enum GameState
{
    GAME_STATE_GAME,
    GAME_STATE_MENU,
    GAME_STATE_CONFIRM,
    GAME_STATE_BACKGROUND,
    GAME_STATE_EXIT
};

enum Sound
{
    SOUND_ATTACH_HANDLE,
    SOUND_DETACH_HANDLE,
    SOUND_SELECT,
    SOUND_COUNT
};

const CHAR* s_SoundFiles[ SOUND_COUNT ] = 
{
    "game:\\Media\\Sounds\\HandleAttach.wav",
    "game:\\Media\\Sounds\\HandleAttach.wav",
    "game:\\Media\\Sounds\\HandleDetach.wav"
};

// start with the menu up.
GameState g_GameState = GAME_STATE_MENU;
DWORD g_dwTrackinIdInvokingSystemGesture = 0;

const CHAR* m_strVertexShaderPassThru =
"struct VS_OUTPUT_TEX_COLOR             \n"
"{                                      \n"
"    float4  Pos     : POSITION;        \n"
"    float4  Color   : COLOR;           \n"
"    float2  Tex     : TEXCOORD0;       \n"
"};                                     \n"
"struct VS_INPUT_TEX_COLOR               \n"
"{                                       \n"
"    float4  vPos    : POSITION0;        \n"       
"    float4  vColor  : COLOR0;           \n"
"    float2  vTex    : TEXCOORD0;        \n"
"};                                      \n"
"                                        \n"
"VS_OUTPUT_TEX_COLOR main( VS_INPUT_TEX_COLOR In ) \n"
"{                              \n"
"    VS_OUTPUT_TEX_COLOR Out;   \n"
"    Out.Pos = In.vPos;         \n"
"    Out.Color = In.vColor;     \n"
"    Out.Tex = In.vTex;         \n"
"    return Out;                \n"
"};                             \n";


const CHAR* m_strPixelShader =
" sampler TextureSampler0 : register(s0);                                   \n"
" float4     g_fDrawColor : register(c0);                                   \n"
"                                                                           \n"
"float4 main( float2 InTex : TEXCOORD0 ) : COLOR                            \n"
"{                                                                          \n"
"                                                                           \n"
"    float4 depthsample = tex2D( TextureSampler0, InTex );                  \n"
"    if ( depthsample.a > g_fDrawColor.a ) depthsample.a = g_fDrawColor.a;  \n"
"    return depthsample;                                                    \n"
"};                                                                         \n";

const CHAR* m_strPixelShaderQuad =
" sampler TextureSampler0 : register(s0);                                   \n"
" float4     g_fDrawColor : register(c0);                                   \n"
"                                                                           \n"
"float4 main( float2 InTex : TEXCOORD0 ) : COLOR                            \n"
"{                                                                          \n"
"                                                                           \n"
"    float4 depthsample = tex2D( TextureSampler0, InTex );                  \n"
"    if ( depthsample.a > g_fDrawColor.a ) depthsample.a = g_fDrawColor.a;  \n"
"    return float4( g_fDrawColor.xyz, depthsample.a );                      \n"
"};                                                                         \n";

static const D3DVERTEXELEMENT9 g_PosTexCoordVertexElements[] =
{
    { 0,     0, D3DDECLTYPE_FLOAT3,     0,  D3DDECLUSAGE_POSITION,  0 },
    { 0,    12, D3DDECLTYPE_FLOAT2,     0,  D3DDECLUSAGE_TEXCOORD,  0 },
    D3DDECL_END()
};

//--------------------------------------------------------------------------------------
// Callouts for labeling the gamepad on the help screen
//--------------------------------------------------------------------------------------l
ATG::HELP_CALLOUT g_HelpCallouts[] =
{
    { ATG::HELP_BACK_BUTTON,  ATG::HELP_PLACEMENT_2, L"Display\nhelp" }
};
static const DWORD  NUM_HELP_CALLOUTS = sizeof( g_HelpCallouts ) / sizeof( g_HelpCallouts[0] );

//--------------------------------------------------------------------------------------
// Name: class Sample
// Desc: Main class to run this application. Most functionality is inherited
//       from the ATG::Application base class.
//--------------------------------------------------------------------------------------
class Sample : public ATG::Application
{
    ATG::Timer m_Timer;    // Timer
    ATG::Font m_Font;     // Font for drawing text
    ATG::Help m_Help;
    BOOL m_bDrawHelp;
    ATG::PackedResource m_Resource; // Bundled textures in a packed resource

    BOOL m_bDrawCursor;
    // Transform matrices
    XMMATRIX m_matView;
    XMMATRIX m_matProj;
   
    // Natural Input data
    CameraManager m_CameraManager;

    HANDLE m_hNotification;

    // see functions for comments
    HRESULT InitAudio();
    void LoadWavFile( Sound value );
    void DestroyAudio();
    HRESULT PlaySound( Sound value );
    HRESULT StopSound( Sound value );
    VOID UpdateHandles( FLOAT fElapsedTimeSinceLastSTUpdate );
    VOID DrawAndUpdateGameState();
    VOID DrawImageColored( D3DRECT Rect, FLOAT fTransparent, D3DCOLOR color, LPDIRECT3DTEXTURE9 pTexture );
    
    XMFLOAT2 GetXYForVector( FXMVECTOR vNormalizedSpace );
    VOID FindClosestGrip( NUI_HANDLES_GRIP** pClosestGrip, FLOAT& fLeftTransparency, FLOAT& fRightTransparency );

    VOID DrawAndUpdateStateGame ();
    VOID DrawAndUpdateStateMenu ();
    VOID DrawAndUpdateStateConfirm ();
    VOID DrawAndUpdateStateBackground ();

public:

    Sample();
    ~Sample();

    virtual HRESULT     Initialize();
    virtual HRESULT     Update();
    virtual HRESULT     Render();

private:

    VOID ResetGlobalHoverState();

    BOOL m_bPlayerEngaged;

    HoverButton m_BackButton;
    HoverButton m_StartButton;
    HoverButton m_ScrollRightButton;
    HoverButton m_ScrollLeftButton;
    HoverButton m_ScrollUpButton;
    HoverButton m_MenuButton[g_iMenuItems];

    // sprites for menu system
    LPDIRECT3DTEXTURE9 m_pCursorTexture;
    LPDIRECT3DTEXTURE9 m_pBackgroundTexture;
    WidthHeight m_CursorDims;
    
    IXAudio2*                    m_pXAudio2;
    IXAudio2MasteringVoice*      m_pMasteringVoice;
    IXAudio2SourceVoice*         m_pSourceVoice[SOUND_COUNT];
    XAUDIO2_BUFFER               m_audioBuffers[SOUND_COUNT];
    BOOL                         m_fPlayingSound[SOUND_COUNT];

};


//--------------------------------------------------------------------------------------
// Name: main()
// Desc: Entry point to the program
//--------------------------------------------------------------------------------------
VOID __cdecl main()
{
    Sample atgApp;
    ATG::GetVideoSettings( &atgApp.m_d3dpp.BackBufferWidth, &atgApp.m_d3dpp.BackBufferHeight );
    atgApp.m_d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_TWO;
    atgApp.Run();
}


//--------------------------------------------------------------------------------------
// Name: DrawImageColored()
// Desc: Draw a sprite with transparency and color
//--------------------------------------------------------------------------------------
VOID Sample::DrawImageColored( D3DRECT Rect, FLOAT fTransparent, D3DCOLOR color, LPDIRECT3DTEXTURE9 pTexture )
{
    MeshVertexPT* pVertexData = NULL;
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_ZWRITEENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_CCW );
    m_pd3dDevice->SetRenderState( D3DRS_VIEWPORTENABLE, FALSE );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP );
    m_pd3dDevice->SetTexture( 0, pTexture );

    D3DXCOLOR d3dxColor ;
    d3dxColor.r = D3DCOLOR_GETRED( color ) / 255.0f;
    d3dxColor.g = D3DCOLOR_GETGREEN( color ) / 255.0f;
    d3dxColor.b = D3DCOLOR_GETBLUE( color ) / 255.0f;
    d3dxColor.a = fTransparent;
    m_pd3dDevice->SetPixelShaderConstantF( 0, d3dxColor, 1 );

    m_pd3dDevice->SetPixelShader( g_pPixelShaderColor );
    m_pd3dDevice->SetVertexShader( g_pVertexShader );
    m_pd3dDevice->SetVertexDeclaration( g_pVertexDeclaration );

    m_pd3dDevice->BeginVertices( D3DPT_RECTLIST, 3, sizeof( MeshVertexPT ), ( VOID** )&pVertexData );    
    assert( pVertexData != NULL );
    pVertexData[0].Position = XMFLOAT3( ( FLOAT )Rect.x1, ( FLOAT )Rect.y1, 0 );
    pVertexData[0].TexCoord = XMFLOAT2( 0, 0 );
    pVertexData[1].Position = XMFLOAT3( ( FLOAT )Rect.x2, ( FLOAT )Rect.y1, 0 );
    pVertexData[1].TexCoord = XMFLOAT2( 1, 0 );
    pVertexData[2].Position = XMFLOAT3( ( FLOAT )Rect.x1, ( FLOAT )Rect.y2, 0 );
    pVertexData[2].TexCoord = XMFLOAT2( 0, 1 );
    m_pd3dDevice->EndVertices();

    m_pd3dDevice->SetRenderState( D3DRS_VIEWPORTENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_ZWRITEENABLE, TRUE );
            
}
//--------------------------------------------------------------------------------------
// Name: Initialize()
// Desc: Initialize app-dependent objects
//--------------------------------------------------------------------------------------
HRESULT Sample::Initialize()
{

    g_pSampleDevice = m_pd3dDevice;

    m_bDrawHelp = FALSE;
    m_bPlayerEngaged = FALSE;
    m_bDrawCursor = TRUE;

    HRESULT hr;

    if ( FAILED( hr = InitAudio() ) )
    {
        ATG_PrintError( "Couldn't initialize audio\n" );
        return hr;
    }

    // Initialize simple shaders and set the renderstates.
    ATG::SimpleShaders::Initialize( NULL, NULL );
    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_SRCBLEND, D3DBLEND_SRCALPHA );
    m_pd3dDevice->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA );

    // Create the font
    if( FAILED( hr = m_Font.Create( "game:\\Media\\Fonts\\Arial_16.xpr" ) ) )
    {
        ATG_PrintError( "Couldn't create font\n" );
        return hr;
    }

    // Create the help
    if( FAILED( hr = m_Help.Create( "game:\\Media\\Help\\Help.xpr" ) ) )
    {
        ATG_PrintError( "Couldn't create help\n" );
        return hr;
    }

    // Confine text drawing to the title safe area
    m_Font.SetWindow( ATG::GetTitleSafeArea() );

    // Create the textures resource
    if( FAILED( hr = m_Resource.Create( "game:\\Media\\Resource.xpr" ) ) )
    {
        ATG_PrintError( "Couldn't create Resource.xpr\n" );
        return hr;
    }

    // Load the sprites for the menu
    m_pCursorTexture = m_Resource.GetTexture("Cursor");
    m_BackButton.Init( m_Resource.GetTexture("MenuBackSelected"), m_Resource.GetTexture("MenuBackNeutral"), g_BackButtonLocation );
    m_StartButton.Init( m_Resource.GetTexture("MenuStartSelected"), m_Resource.GetTexture("MenuStartNeutral"), g_StartButtonLocation );
    m_ScrollRightButton.Init( m_Resource.GetTexture("MenuScrollRight"), NULL, g_ScrollRightButtonLocation );
    m_ScrollUpButton.Init( m_Resource.GetTexture("MenuScrollUp"), NULL, g_ScrollUpButtonLocation );
    m_ScrollLeftButton.Init( m_Resource.GetTexture("MenuScrollLeft"), NULL, g_ScrollLeftButtonLocation );

    m_MenuButton[0].m_pTextures[0] = m_Resource.GetTexture("Menu1Selected");
    m_MenuButton[0].m_pTextures[1] = m_Resource.GetTexture("Menu1Neutral");
    m_MenuButton[1].m_pTextures[0] = m_Resource.GetTexture("Menu2Selected");
    m_MenuButton[1].m_pTextures[1] = m_Resource.GetTexture("Menu2Neutral");
    m_MenuButton[2].m_pTextures[0] = m_Resource.GetTexture("Menu3Selected");
    m_MenuButton[2].m_pTextures[1] = m_Resource.GetTexture("Menu3Neutral");
    m_MenuButton[3].m_pTextures[0] = m_Resource.GetTexture("Menu4Selected");
    m_MenuButton[3].m_pTextures[1] = m_Resource.GetTexture("Menu4Neutral");
    
    D3DSURFACE_DESC SurfaceDESC;
    m_MenuButton[0].m_pTextures[0]->GetLevelDesc( 0, &SurfaceDESC );
    for ( INT iIndex = 0; iIndex < g_iMenuItems; ++iIndex )
    {
        m_MenuButton[iIndex].m_ButtonDims.Width = (INT) SurfaceDESC.Width;
        m_MenuButton[iIndex].m_ButtonDims.Height = (INT) SurfaceDESC.Height;
        m_MenuButton[iIndex].m_fTimeAsSelectedLeft = 0.0f;
        m_MenuButton[iIndex].m_fTimeAsSelectedRight= 0.0f;
    }    
    
    INT iStartY = g_MenuLocation.Height - ( ( m_MenuButton[0].m_ButtonDims.Height + g_iSpaceBetweenMenuItems ) * g_iMenuItems ) / 2;

    INT iMenuX1 = g_MenuLocation.Width - m_MenuButton[0].m_ButtonDims.Width /2;
    INT iMenuX2 = g_MenuLocation.Width + m_MenuButton[0].m_ButtonDims.Width /2;
    for ( INT iIndex = 0; iIndex < g_iMenuItems; ++iIndex )
    {
        m_MenuButton[iIndex].m_DrawLocation.x1 = iMenuX1;
        m_MenuButton[iIndex].m_DrawLocation.x2 = iMenuX2;
        m_MenuButton[iIndex].m_DrawLocation.y1 = iStartY;
        iStartY+=m_MenuButton[0].m_ButtonDims.Height;
        m_MenuButton[iIndex].m_DrawLocation.y2 = iStartY;
        iStartY+=g_iSpaceBetweenMenuItems;
    }

    m_pBackgroundTexture = m_Resource.GetTexture("BackGround");

    m_CursorDims.Width = g_iCursorSize;
    m_CursorDims.Height = g_iCursorSize;

    // Initialize the natural input device
    if( FAILED( hr = m_CameraManager.InitializeCamera( m_pd3dDevice ) ) )
    {
        ATG_PrintError( "Couldn't create the natural input device.\n" );
        return hr;
    }

    // Set the transform matrices
    FLOAT fAspectRatio = ( FLOAT )m_d3dpp.BackBufferWidth / ( FLOAT )m_d3dpp.BackBufferHeight;
    m_matProj = XMMatrixPerspectiveFovLH( XM_PI / 3, fAspectRatio, .10f, 10000.0f );
    
    NuiHandlesArmsInit( &g_HandlesArms );
    
    ID3DXBuffer* pShaderCode = NULL;
    ID3DXBuffer* pErrorMsg = NULL;

    // Create the vertex shader
    D3DXCompileShader( m_strVertexShaderPassThru, 
        ( UINT )strlen( m_strVertexShaderPassThru ),  NULL, NULL, "main", "vs.3.0", 0, 
        &pShaderCode, &pErrorMsg, NULL );
    m_pd3dDevice->CreateVertexShader( ( DWORD* )pShaderCode->GetBufferPointer(),
        &g_pVertexShader );
    pShaderCode->Release();

    // Create the pixel shader
    D3DXCompileShader( m_strPixelShader, 
        ( UINT )strlen( m_strPixelShader ), NULL, NULL, "main", "ps.3.0", 0,
        &pShaderCode, &pErrorMsg, NULL );        
    m_pd3dDevice->CreatePixelShader( ( DWORD* )pShaderCode->GetBufferPointer(),
        &g_pPixelShader );
    pShaderCode->Release();

    D3DXCompileShader( m_strPixelShaderQuad, 
        ( UINT )strlen( m_strPixelShader ), NULL, NULL, "main", "ps.3.0", 0,
        &pShaderCode, &pErrorMsg, NULL );        
    m_pd3dDevice->CreatePixelShader( ( DWORD* )pShaderCode->GetBufferPointer(),
        &g_pPixelShaderColor );
    pShaderCode->Release();

    m_pd3dDevice->CreateVertexDeclaration( g_PosTexCoordVertexElements, &g_pVertexDeclaration );

    // create some random colors for the backgrounds
    for ( int iIndex = 0; iIndex < g_iTotalColors; ++iIndex )
    {
        g_BackGroundColors[iIndex] = rand()%255;
        g_BackGroundColors[iIndex] <<= 8;
        g_BackGroundColors[iIndex] += rand()%255;
        g_BackGroundColors[iIndex] <<= 8;
        g_BackGroundColors[iIndex] += rand()%255;
        g_BackGroundColors[iIndex] <<= 8;
        g_BackGroundColors[iIndex] += rand()%255;
    }

    m_hNotification = XNotifyCreateListener( XNOTIFY_SYSTEM );
    if( m_hNotification == NULL || m_hNotification == INVALID_HANDLE_VALUE )
    { 
        ATG_PrintError( "Unable to create XNotify listener" );
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Update()
// Desc: Called once per frame, the call is the entry point for animating the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Update()
{
    static FLOAT fElapsedTimeSinceLastSTUpdate = 0;

    
    DWORD dwNotificationID;
    ULONG_PTR ulParam;

    if( XNotifyGetNext( m_hNotification, 0, &dwNotificationID, &ulParam ) )
    {
        switch( dwNotificationID )
        {
            case XN_SYS_NUIPAUSE:
                g_GameState = GAME_STATE_MENU;
                g_dwTrackinIdInvokingSystemGesture = (DWORD)ulParam;
            break;
        }
    }

    // Get the elapsed time since previous rendered frame
    g_fElapsedTime = ( FLOAT )m_Timer.GetElapsedTime();
    fElapsedTimeSinceLastSTUpdate += g_fElapsedTime;

    // Get the current gamepad state
    ATG::GAMEPAD* pGamepad = ATG::Input::GetMergedInput();

    // Toggle help
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_BACK )
        m_bDrawHelp = !m_bDrawHelp;

    // Update the natural input device
    BOOL bNewSkeletonAndMaps = m_CameraManager.CheckForNewSkeletonAndDepthMaps( g_fElapsedTime, 0 );
    
    if ( bNewSkeletonAndMaps )
    {
        // If the player is facing the camera, the player is engadged with the menu
        m_bPlayerEngaged = m_CameraManager.FacingCamera();

        // NOTE: This API require the time delta since the last updated
        // skeleton data, which could be different from time delta
        // between game frames            
        UpdateHandles( fElapsedTimeSinceLastSTUpdate );
        fElapsedTimeSinceLastSTUpdate = 0.0f;
        m_CameraManager.ReleaseDepthMaps();
    }   

    // Check if we're exiting the sample
    if ( g_GameState == GAME_STATE_EXIT )
    {
        // Let the select sound complete first
        Sleep( 200 );
        XLaunchNewImage( "", 0 );
    }

    // Update the view from the person's position and facing direction.
    return S_OK;
}

VOID Sample::UpdateHandles( FLOAT fElapsedTimeSinceLastSTUpdate )
{

    const NUI_SKELETON_DATA* pSkeletonData;
    // Update if skeleton was tracked
    pSkeletonData = m_CameraManager.GetTrackedSkeleton();
    if ( pSkeletonData == NULL )
    {
        return;
    }    
   
    static int cnt = 0;
    ++cnt;
    if ( cnt > 150 )
    {
        cnt = 0;
    }
    // Each frame, update the arms based on the joints
    // NOTE: This API requires raw/unfiltered joint positions.
    NuiHandlesArmsUpdate( &g_HandlesArms, m_CameraManager.GetActiveSkeletonSkeletonDataIndex(), m_CameraManager.GetSkeletonFrame(), m_CameraManager.GetDepthFrame320x240(), NULL,
        m_CameraManager.GetDepthFrame80x60(), NULL );

}

//--------------------------------------------------------------------------------------
// Name: GetXYForVector()
// Desc: Map normalized space to screen space.
//       
//--------------------------------------------------------------------------------------
XMFLOAT2 Sample::GetXYForVector( FXMVECTOR vNormalizedSpace )
{
    XMFLOAT2 rt;
    rt.x = XMVectorGetX( vNormalizedSpace ) * 640.0f + 640.0f;           
    rt.y = 720 - ( XMVectorGetY( vNormalizedSpace ) * 360.0f + 360.0f);
    return rt;
}

//--------------------------------------------------------------------------------------
// Name: DrawAndUpdateStateGame()
// Desc: Draw game play mode
//--------------------------------------------------------------------------------------
VOID Sample::DrawAndUpdateStateGame()
{
    
    PIXBeginNamedEvent( 0, "Game" );
   
    UINT uWidth, uHeight;
    ATG::GetVideoSettings( &uWidth, &uHeight );

    m_Font.Begin();
        m_Font.SetScaleFactors( 2.0f, 2.0f );
        m_Font.DrawText( 0.0f, 150.0f, 0xffffffff, L"Playing game..." );
    m_Font.End();

    if ( m_BackButton.UpdateSelected( ) )
    {
        g_GameState = GAME_STATE_MENU;
        m_BackButton.Reset();
    }

    if ( !m_BackButton.m_bLeftCursorHovering )
    {
        // reset hover when nothing is selected
        g_fTimeLeftHovering = 0.0f;
    }
    if ( !m_BackButton.m_bRightCursorHovering )
    {
        // reset hover when nothing is selected
        g_fTimeRightHovering = 0.0f;
    }

    m_BackButton.DrawButton();

    PIXEndNamedEvent();

}


//--------------------------------------------------------------------------------------
// Name: DrawAndUpdateStateMenu()
// Desc: Draw main menu
//--------------------------------------------------------------------------------------
VOID Sample::DrawAndUpdateStateMenu ()
{
 
    PIXBeginNamedEvent( 0, "Menu" );

    if ( m_MenuButton[0].UpdateSelected() )
    {
        g_GameState = GAME_STATE_CONFIRM;
        m_MenuButton[0].Reset();
    }
    if ( m_MenuButton[1].UpdateSelected() )
    {
        g_GameState = GAME_STATE_BACKGROUND;
        m_MenuButton[1].Reset();
    }
    if ( m_MenuButton[2].UpdateSelected() )
    {
        g_GameState = GAME_STATE_EXIT;
        m_MenuButton[2].Reset();
    }
    if ( m_MenuButton[3].UpdateSelected() )
    {
        XShowNuiGuideUI( g_dwTrackinIdInvokingSystemGesture );
        m_MenuButton[3].Reset();
    }
    
    BOOL bLeftHovering = FALSE;
    BOOL bRightHovering = FALSE;
    for ( UINT uIndex = 0; uIndex < g_iMenuItems; ++uIndex )
    {
        if ( m_MenuButton[uIndex].m_bLeftCursorHovering ) bLeftHovering = TRUE;
        if ( m_MenuButton[uIndex].m_bRightCursorHovering ) bRightHovering = TRUE;
    }

    if ( !bLeftHovering )
    {
        // reset hover when nothing is selected
        g_fTimeLeftHovering = 0.0f;
    }
    if ( !bRightHovering )
    {
        // reset hover when nothing is selected
        g_fTimeRightHovering = 0.0f;
    }

    for ( UINT uIndex = 0; uIndex < g_iMenuItems; ++uIndex )
    {
        m_MenuButton[uIndex].DrawButton();
    }
    PIXEndNamedEvent();
    
}


//--------------------------------------------------------------------------------------
// Name: DrawAndUpdateStateConfirm()
// Desc: Draw confirm mode
//--------------------------------------------------------------------------------------
VOID Sample::DrawAndUpdateStateConfirm ()
{

    PIXBeginNamedEvent( 0, "Confirm" );

    m_Font.Begin();
        m_Font.SetScaleFactors( 2.0f, 2.0f );
        m_Font.DrawText( 0.0f, 150.0f, 0xffffffff, L"To start a new game use the start button." );
        m_Font.DrawText( 0.0f, 200.0f, 0xffffffff, L"To go back to the menu use the back button." );
    m_Font.End();

    if ( m_BackButton.UpdateSelected( ) )
    {
        g_GameState = GAME_STATE_MENU;
        m_BackButton.Reset();
    }
    if ( m_StartButton.UpdateSelected( ) )
    {
        g_GameState = GAME_STATE_GAME;
        m_BackButton.Reset();
    }

    if ( !m_BackButton.m_bLeftCursorHovering && !m_StartButton.m_bLeftCursorHovering )
    {
        // reset hover when nothing is selected
        g_fTimeLeftHovering = 0.0f;
    }
    if ( !m_BackButton.m_bRightCursorHovering && !m_StartButton.m_bRightCursorHovering )
    {
        // reset hover when nothing is selected
        g_fTimeRightHovering = 0.0f;
    }

    m_StartButton.DrawButton();
    m_BackButton.DrawButton();

    PIXEndNamedEvent();

}


//--------------------------------------------------------------------------------------
// Name: DrawAndUpdateStateBackground()
// Desc: Draw background mode
//--------------------------------------------------------------------------------------
VOID Sample::DrawAndUpdateStateBackground ()
{

    PIXBeginNamedEvent( 0, "Background" );

    UINT uWidth, uHeight;
    ATG::GetVideoSettings( &uWidth, &uHeight );

    // Cursor state

    m_ScrollLeftButton.UpdateSelected( );
    m_ScrollRightButton.UpdateSelected( );

    if ( m_ScrollLeftButton.m_bLeftCursorHovering || m_ScrollLeftButton.m_bRightCursorHovering )
    {
        g_fCurrentPositionInColorList2 += 0.003f;
    }
    if ( m_ScrollRightButton.m_bLeftCursorHovering || m_ScrollRightButton.m_bRightCursorHovering )
    {
        g_fCurrentPositionInColorList2 -= 0.003f;
    }
    m_ScrollLeftButton.Reset();
    m_ScrollRightButton.Reset();
    g_fCurrentPositionInColorList2 = min( 1.0f, max ( 0.0f, g_fCurrentPositionInColorList2 ) );

    if ( m_ScrollUpButton.UpdateSelected( ) )
    {
        PlaySound( SOUND_SELECT );
        g_GlobalBackgroundColor2 = g_BackGroundColors[g_iCurrentPositionInColorList2];
        m_ScrollUpButton.Reset();
    }
    if ( m_BackButton.UpdateSelected( ) )
    {
        g_GameState = GAME_STATE_MENU;
        m_BackButton.Reset();
    }

    if ( !m_ScrollUpButton.m_bLeftCursorHovering && !m_BackButton.m_bLeftCursorHovering )
    {
        g_fTimeLeftHovering = 0.0f;
    }
    if ( !m_ScrollUpButton.m_bRightCursorHovering && !m_BackButton.m_bRightCursorHovering )
    {
        // reset hover when nothing is selected
        g_fTimeRightHovering = 0.0f;
    }

    m_ScrollUpButton.DrawButton();
    m_ScrollRightButton.DrawButton();
    m_ScrollLeftButton.DrawButton();
    m_BackButton.DrawButton();

    // This code renders the list of colors as they iterate by
    D3DRECT BackgroundColorRects;
    FLOAT f0To100Position = g_fCurrentPositionInColorList2 * (FLOAT)g_iTotalColors;
    g_iCurrentPositionInColorList2 =  (LONG)floor( f0To100Position );
    FLOAT fExcess = f0To100Position - (FLOAT)(g_iCurrentPositionInColorList2); 
    LONG iStartingOffset = (LONG)( fExcess * 200.0f );
    BackgroundColorRects.x1 = 440 + iStartingOffset - 600;
    BackgroundColorRects.y1 = 250 ;
    BackgroundColorRects.x2 = 640 + iStartingOffset - 600;
    BackgroundColorRects.y2 = 450 ;
    for ( int index = 3; index > -4; --index )
    {
        if ( g_iCurrentPositionInColorList2 + index >= 0
            && g_iCurrentPositionInColorList2 + index < g_iTotalColors )
        {
            DrawImageColored( BackgroundColorRects, 1.0f, 
                g_BackGroundColors[g_iCurrentPositionInColorList2 + index], m_MenuButton[0].m_pTextures[0]);
        }
        BackgroundColorRects.x1 += 200;
        BackgroundColorRects.x2 += 200;
    }          

    PIXEndNamedEvent();

}

//--------------------------------------------------------------------------------------
// Name: DrawAndUpdateGameState()
// Desc: Render the screen's UI controls
//--------------------------------------------------------------------------------------
VOID Sample::DrawAndUpdateGameState()
{
    PIXBeginNamedEvent( 0, "Draw UI" );

    m_pd3dDevice->SetVertexDeclaration( g_pVertexDeclaration );
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_ZWRITEENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_NONE );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MAXANISOTROPY, 16 );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_ANISOTROPIC );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_ANISOTROPIC );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MIPFILTER, D3DTEXF_POINT );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSU, D3DTADDRESS_MIRROR );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSV, D3DTADDRESS_MIRROR );

    m_bDrawCursor = TRUE;

    switch ( g_GameState ) 
    {
        case GAME_STATE_GAME:
        {
            DrawAndUpdateStateGame();
            break;
        }

        case GAME_STATE_MENU:
        {
            DrawAndUpdateStateMenu(); 
            break;
        }
        case GAME_STATE_CONFIRM:
        {
            DrawAndUpdateStateConfirm(); 
            break;
        }

        case GAME_STATE_BACKGROUND:
        {
            DrawAndUpdateStateBackground(); 
            break;
        }
    }

    // if the player is not engaged / facing the camera, then don't render the cursor
    // When a handle is attached we don't draw the cursor because it's being rendered along the rail or as the rail
    if ( m_bPlayerEngaged && m_bDrawCursor )
    {
        PIXBeginNamedEvent( 0, "Cursor" );
        D3DRECT rect;
        // Get the current cursor position
        g_fLeftHandCursor = GetXYForVector( NuiHandlesArmGetScreenSpaceLocation( &g_HandlesArms, NUI_HANDLES_ARMS_HANDEDNESS_LEFT_ARM ) );
        g_fRightHandCursor = GetXYForVector( NuiHandlesArmGetScreenSpaceLocation( &g_HandlesArms, NUI_HANDLES_ARMS_HANDEDNESS_RIGHT_ARM ) );

        rect.x1 = (LONG)g_fLeftHandCursor.x - m_CursorDims.Width / 2;
        rect.x2 = (LONG)g_fLeftHandCursor.x + m_CursorDims.Width / 2;          
        rect.y1 = (LONG)g_fLeftHandCursor.y - m_CursorDims.Height / 2;
        rect.y2 = (LONG)g_fLeftHandCursor.y + m_CursorDims.Height / 2; 
        DrawImage( rect, 0.8f, g_CursorOnRailColor[0], m_pCursorTexture );

        if ( g_fTimeLeftHovering > 0 )
        {
            rect.x2 = rect.x2 - (LONG)( (FLOAT)m_CursorDims.Width / 4.0f + g_fTimeLeftHovering * 100.0f);
            rect.x1 = rect.x1 + (LONG)( (FLOAT)m_CursorDims.Width / 4.0f + g_fTimeLeftHovering * 100.0f);
            rect.y1 = rect.y1 + (LONG)( (FLOAT)m_CursorDims.Width / 4.0f + g_fTimeLeftHovering * 100.0f);
            rect.y2 = rect.y2 - (LONG)( (FLOAT)m_CursorDims.Width / 4.0f + g_fTimeLeftHovering * 100.0f);
            DrawImageColored( rect, 1.0f, g_CursorOnRailColor[0], m_pCursorTexture );
        }
        rect.x1 = (LONG)g_fRightHandCursor.x - m_CursorDims.Width / 2;
        rect.x2 = (LONG)g_fRightHandCursor.x + m_CursorDims.Width / 2;          
        rect.y1 = (LONG)g_fRightHandCursor.y - m_CursorDims.Height / 2;
        rect.y2 = (LONG)g_fRightHandCursor.y + m_CursorDims.Height / 2; 
        DrawImage( rect, 0.8f, g_CursorOnRailColor[1], m_pCursorTexture );

        if ( g_fTimeRightHovering > 0 )
        {
            rect.x2 = rect.x2 - (LONG)( (FLOAT)m_CursorDims.Width / 4.0f + g_fTimeRightHovering * 100.0f);
            rect.x1 = rect.x1 + (LONG)( (FLOAT)m_CursorDims.Width / 4.0f + g_fTimeRightHovering * 100.0f);
            rect.y1 = rect.y1 + (LONG)( (FLOAT)m_CursorDims.Width / 4.0f + g_fTimeRightHovering * 100.0f);
            rect.y2 = rect.y2 - (LONG)( (FLOAT)m_CursorDims.Width / 4.0f + g_fTimeRightHovering * 100.0f);
            DrawImageColored( rect, 1.0f, g_CursorOnRailColor[1], m_pCursorTexture );
        }
        PIXEndNamedEvent();
    }

    PIXEndNamedEvent();
}


//--------------------------------------------------------------------------------------
// Name: Render()
// Desc: Called once per frame, the call is the entry point for 3D rendering. This 
//       function sets up render states, clears the viewport, and renders the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Render()
{
    // Draw a gradient filled background with texture on top
    ATG::RenderBackground( g_GlobalBackgroundColor1, g_GlobalBackgroundColor2 );

    // Show title, frame rate, and help
    m_Timer.MarkFrame();

    DrawAndUpdateGameState();

    m_CameraManager.DisplayPIP();

    // Render the HUD
    m_Font.Begin();
    m_Font.SetScaleFactors( 1.2f, 1.2f );
    m_Font.DrawText( 500.0f, 0.0f, 0xffffffff, L"Menu Using Hover", ATGFONT_CENTER_X );
    m_Font.SetScaleFactors( 1.0f, 1.0f );
    m_Font.DrawText( 500.0f, 25.0f, 0xffffff00, m_Timer.GetFrameRate(), ATGFONT_CENTER_X );
    
    // Render Smoothing and Tilt Correction options

    m_Font.End();

    if( m_bDrawHelp )
    {
        m_Help.Render( &m_Font, g_HelpCallouts, NUM_HELP_CALLOUTS );
    }

    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}


//----------------------------------------------------------------------------------------------------------------------
// Name: Sample::InitAudio
// Desc: Initializes XAudio2, and loads our menu sounds into memory.
//----------------------------------------------------------------------------------------------------------------------
HRESULT Sample::InitAudio()
{
    HRESULT hr;

    // Create XAudio2

    if( FAILED( hr = XAudio2Create( &m_pXAudio2, 0, XAUDIO2_DEFAULT_PROCESSOR ) ) )
        return hr;

    // Create a mastering voice

    if( FAILED( hr = m_pXAudio2->CreateMasteringVoice( &m_pMasteringVoice,
        XAUDIO2_DEFAULT_CHANNELS, XAUDIO2_DEFAULT_SAMPLERATE, 0, 0, NULL
        ) ) )
    {
        m_pXAudio2->Release();
        m_pXAudio2 = NULL;
    }

    LoadWavFile( SOUND_ATTACH_HANDLE );
    LoadWavFile( SOUND_DETACH_HANDLE );
    LoadWavFile( SOUND_SELECT );

    ATG::DebugSpew( "Initialized audio...\n" );

    return hr;
}


//----------------------------------------------------------------------------------------------------------------------
// Name: Sample::LoadWavFile
// Desc: Loads a wav file from disk.
//----------------------------------------------------------------------------------------------------------------------
void Sample::LoadWavFile( Sound value )
{
    assert( value < SOUND_COUNT );

    HRESULT hr;

    XAUDIO2_BUFFER& buffer = m_audioBuffers[ value ];
    if ( buffer.pAudioData != NULL )
    {
        ATG_PrintError( "Loading wav file in already used slot\n" );
        return;
    }

    LPCSTR strAudioFile = s_SoundFiles[ value ];
    
    // Read the wave file
    ATG::WaveFile WaveFile;
    if( FAILED( hr = WaveFile.Open( strAudioFile ) ) )
    {
        ATG::FatalError( "Error %#X opening WAV file %s\n", hr, strAudioFile );
    }

    // Read the format header

    WAVEFORMATEXTENSIBLE wfx = {0};
    if( FAILED( hr = WaveFile.GetFormat( &wfx ) ) )
    {
        ATG::FatalError( "Error %#X reading WAV format from %s\n", hr, strAudioFile );
    }

    // Calculate how many bytes and samples are in the wave

    DWORD cbWaveSize = 0;
    WaveFile.GetDuration( &cbWaveSize );

    // Read the sample data into memory

    BYTE* pbWaveData = (BYTE*)malloc( cbWaveSize );
    if( FAILED( hr = WaveFile.ReadSample( 0, pbWaveData, cbWaveSize, &cbWaveSize ) ) )
    {
        ATG::FatalError( "Error %#X reading WAV data\n", hr );
    }

    buffer.pAudioData = pbWaveData;
    buffer.Flags = XAUDIO2_END_OF_STREAM;  // mark the audio buffer as "ok for the source voice to starve"
    buffer.AudioBytes = cbWaveSize;
    buffer.LoopCount = 0;

    // If this is the first WAV file we've loaded, create the source voice.
    // NOTE: all WAVs loaded by this function must be of the same format.

    assert( !m_pSourceVoice[ value ] );
    if( FAILED( hr = m_pXAudio2->CreateSourceVoice( &( m_pSourceVoice[ value ] ),
        ( WAVEFORMATEX* )&wfx ) ) )
    {
        ATG::FatalError( "Error %#X creating source voice\n", hr );
    }
}


//----------------------------------------------------------------------------------------------------------------------
// Name: Sample::DestroyAudio
// Desc: Destroys the audio engine, and releases any resources we have used.
//----------------------------------------------------------------------------------------------------------------------
void Sample::DestroyAudio()
{
    if ( m_pXAudio2 )
    {
        m_pXAudio2->StopEngine();
    }

    if ( m_pMasteringVoice )
    {
        m_pMasteringVoice->DestroyVoice();
        m_pMasteringVoice = NULL;
    }

    for( int i = 0; i < SOUND_COUNT; ++i )
    {
        if ( m_audioBuffers[ i ].pAudioData != NULL )
        {
            if ( m_pSourceVoice[ i ] )
            {
                m_pSourceVoice[ i ] ->DestroyVoice();
                m_pSourceVoice[ i ] = NULL;
            }

            free( const_cast<BYTE*>(m_audioBuffers[ i ].pAudioData) );
            m_audioBuffers[ i ].pAudioData = NULL;
        }
    }

    ::ZeroMemory( &m_audioBuffers, sizeof(m_audioBuffers) );

    if ( m_pXAudio2 )
    {
        m_pXAudio2->Release();
        m_pXAudio2 = NULL;
    }

    ATG::DebugSpew( "Shutdown audio...\n" );
}


//----------------------------------------------------------------------------------------------------------------------
// Name: Sample::PlaySound
// Desc: Plays a specific one-shot sound.
//----------------------------------------------------------------------------------------------------------------------
HRESULT Sample::PlaySound( Sound value )
{
    HRESULT hr;

    if ( m_fPlayingSound[ value ] )
    {
        hr = StopSound( value );
        if ( FAILED( hr ) )
        {
            ATG_PrintError( "Couldn't play sound; StopSound failed\n" );
            return hr;
        }
    }

    hr = m_pSourceVoice[ value ]->SubmitSourceBuffer( &( m_audioBuffers[ value ] ), NULL );
    if ( FAILED( hr ) )
    {
        ATG::FatalError( "Couldn't submit source buffer %d\n", value );
    }

    m_pSourceVoice[ value ]->SetVolume( 0.5f, XAUDIO2_COMMIT_NOW );
    

    hr = m_pSourceVoice[ value ]->Start( 0, XAUDIO2_COMMIT_NOW );

    if ( SUCCEEDED( hr ) )
    {
        m_fPlayingSound[ value ] = TRUE;
    }
    else
    {
        ATG_PrintError( "Couldn't start source voice\n" );
    }

    return hr;
}


//----------------------------------------------------------------------------------------------------------------------
// Name: Sample::Sample
// Desc: Constructor for Sample object.
//----------------------------------------------------------------------------------------------------------------------
Sample::Sample()
: m_pXAudio2( NULL ),
  m_pMasteringVoice( NULL )
{
    ::ZeroMemory( m_pSourceVoice, sizeof( m_pSourceVoice ) );
    ::ZeroMemory( m_fPlayingSound, sizeof( m_fPlayingSound ) );
    ::ZeroMemory( m_audioBuffers, sizeof( m_audioBuffers ) );
}


//----------------------------------------------------------------------------------------------------------------------
// Name: Sample::~Sample
// Desc: Destroys the sample
//----------------------------------------------------------------------------------------------------------------------
Sample::~Sample()
{
    DestroyAudio();
}


//----------------------------------------------------------------------------------------------------------------------
// Name: Sample::StopSound
// Desc: Stops a playing sound.
//----------------------------------------------------------------------------------------------------------------------
HRESULT Sample::StopSound( Sound value )
{
    HRESULT hr = S_OK;
    
    if ( m_fPlayingSound[ value ] )
    {

        hr = m_pSourceVoice[ value ]->Stop( 0, XAUDIO2_COMMIT_NOW );

        if ( SUCCEEDED( hr ) )
        {
            m_fPlayingSound[ value ] = FALSE;
        }
        else
        {
            ATG_PrintError( "Couldn't stop source voice\n" );
            return hr;
        }

        hr = m_pSourceVoice[ value ]->FlushSourceBuffers();
        if ( FAILED( hr ) )
        {
            ATG_PrintError( "Couldn't flush source buffers\n" );
        }

    }

    return hr;
}
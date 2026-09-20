//--------------------------------------------------------------------------------------
// Xime.cpp
//
// The sample shows how to display the message box UI.
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include <xtl.h>
#include <xui.h>
#include <d3d9.h>
#pragma warning( push )
#pragma warning( disable: 4127 )    // Work around for compiler warning
#include <list>
#pragma warning( pop )
#include <string>
#include "AtgApp.h"
#include "AtgFont.h"
#include "AtgHelp.h"
#include "AtgInput.h"
#include "AtgResource.h"
#include "AtgUtil.h"
#include "AtgXime.h"

//--------------------------------------------------------------------------------------
// Color values
//--------------------------------------------------------------------------------------
#define SEL_COLOR           0xffff0000          // selection color
#define UNSEL_COLOR         0xffffffff          // unselection color

#define MSG_COLOR           0xffffffff          // message color
#define INFO_COLOR          0xffffff00          // information display color

#define TOP_BACK_COLOR      0xff0000ff          // background gradient colors
#define BOTTOM_BACK_COLOR   0xff000000

const float STRING_POSX = 100;
const float STRING_POSY = 140;
const float FONT_HEIGHT = 33.f;
const INT NUM_LINE = 5;

//--------------------------------------------------------------------------------------
// Globals
//--------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------
// Constants
//--------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------
// Callouts for labelling the gamepad on the help screen
//--------------------------------------------------------------------------------------
ATG::HELP_CALLOUT g_HelpCallouts[] =
{
    { ATG::HELP_BACK_BUTTON,    ATG::HELP_PLACEMENT_2, L"Display\nhelp" },
    { ATG::HELP_X_BUTTON,       ATG::HELP_PLACEMENT_1, L"Switch input language" },
    { ATG::HELP_Y_BUTTON,       ATG::HELP_PLACEMENT_1, L"Toggle XIME on/off" },
    { ATG::HELP_LEFT_SHOULDER,  ATG::HELP_PLACEMENT_2, L"Load user word XML file\n(Japanese only)" },
    { ATG::HELP_RIGHT_SHOULDER, ATG::HELP_PLACEMENT_2, L"Delete all user words\n(Japanese only)" },

};
static const DWORD NUM_HELP_CALLOUTS = sizeof(g_HelpCallouts)/sizeof(g_HelpCallouts[0]);


//--------------------------------------------------------------------------------------
// Name: Sample
// Desc: Main class to run this application. Most functionality is inherited
//       from the ATG::Application base class.
//--------------------------------------------------------------------------------------
class Sample : public ATG::Application
{
private:
    ATG::Timer          m_Timer;
    ATG::Font           m_Font;
    ATG::Help           m_Help;
    
    ATG::Xime           m_Xime;

    BOOL                m_bDrawHelp;

    HXUIDC              m_hDC;                      // Xui device context
    HXUIFONT            m_hFont;                    // Handle to Xui font
    bool                m_fXuiRenderInitialized;    // Initialization flags
    bool                m_fXuiInitialized;
    bool                m_bKeyboardConnected;

    std::wstring        m_TypedString[ NUM_LINE ];
    INT                 m_iLines;
    DWORD               m_dwCharLength;
private:
    virtual HRESULT Initialize();
    virtual HRESULT Update();
    virtual HRESULT Render();

    // Xui related methods
    HRESULT InitXui();
    void    UninitXui();
    void    DrawText( HXUIDC hdc, HXUIFONT hFont, D3DCOLOR color, float x, float y,
                      LPCWSTR text, float * fX = NULL, float * fY = NULL );
};

//--------------------------------------------------------------------------------------
// Name: main()
// Desc: Entry point to the program
//--------------------------------------------------------------------------------------
VOID __cdecl main()
{
    Sample atgApp;
    ATG::GetVideoSettings( &atgApp.m_d3dpp.BackBufferWidth,
                           &atgApp.m_d3dpp.BackBufferHeight );
    atgApp.Run();
}


//--------------------------------------------------------------------------------------
// Name: Initialize
// Desc: This creates all device-dependent display objects.
//--------------------------------------------------------------------------------------
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

    if( FAILED( InitXui() ) )
        return -1;
    
    m_dwCharLength = (m_d3dpp.BackBufferWidth == 640)? 20 : 30;

    ATG::IMEMODE  mode;
    switch( XGetLanguage() )
    {
    default:
    case XC_LANGUAGE_JAPANESE:   mode = ATG::MODE_JP; break;
    case XC_LANGUAGE_KOREAN:     mode = ATG::MODE_KR; break;
    case XC_LANGUAGE_TCHINESE:   mode = ATG::MODE_TC; break;
    }
    if( FAILED( m_Xime.Init( mode, m_dwCharLength, &m_d3dpp ) ) )
        return -1;

    m_bDrawHelp = FALSE;
    m_iLines = 0;
    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Update
// Desc: Called once per frame, the call is the entry point for animating
//       the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Update()
{
    // Get input from all the gamepads
    ATG::GAMEPAD* pGamepad = ATG::Input::GetMergedInput();

    HRESULT hResult = m_Xime.Update();

    if( hResult == ( HRESULT ) ERROR_DEVICE_NOT_CONNECTED )
        m_bKeyboardConnected = false;
    else
        m_bKeyboardConnected = true;

    // Retrieve string from IME helper
    std::wstring s;
    if( m_Xime.GetString( s ) == S_OK )
    {
        // insert IME string into own buffer.
        m_TypedString[ m_iLines ] += s;
    }

    // Retrieve keystroke from IME helper
    XINPUT_KEYSTROKE key;
    m_Xime.GetLastKey( &key );

    if( key.Flags & XINPUT_KEYSTROKE_KEYDOWN )
    {
        DWORD  dwTypedLength = m_TypedString[ m_iLines ].length();

        if( key.VirtualKey == VK_RETURN || key.HidCode == 0x58 )
        {
            // next line
            if( !m_TypedString[ m_iLines ].empty() )
            {
                if( m_iLines == NUM_LINE- 1 )
                {
                    // scroll buffer
                    for( INT i = 0; i< NUM_LINE - 1; ++i )
                    {
                        m_TypedString[ i ] = m_TypedString[ i + 1 ];
                    }
                }
                else
                    ++m_iLines;
            }

            m_TypedString[ m_iLines ].clear();
            dwTypedLength = 0;
        }
        else if( key.VirtualKey == VK_BACK )
        {
            if( dwTypedLength > 0 )
                m_TypedString[ m_iLines ].erase( --dwTypedLength );
        }
        else if( key.Unicode != 0 )
        {
            if( dwTypedLength < m_dwCharLength )
            {
                m_TypedString[ m_iLines ] += key.Unicode;
                ++dwTypedLength;
            }
        }
        else
        {
            // TO DO OTHER KEYS
        }

        // update acceptable character length for IME helper
        m_Xime.SetInputCharacterLength( m_dwCharLength - dwTypedLength );
    }


    // Toggle help
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_BACK )
        m_bDrawHelp = !m_bDrawHelp;

    // Show the signin UI
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_B )
    {
    }

    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_X )
    {
        ATG::Xime::SwitchLanguage(&m_Xime);
    }

    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_Y )
    {
        ATG::Xime::ToggleIME(&m_Xime);
    }

    if (m_Xime.GetCurrentLanguage() == ATG::MODE_JP )
    {
        if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_LEFT_SHOULDER )
        {
            m_Xime.LoadJPUserWordFile("game:\\media\\XimeJPUserWord.xml");
        }
        else if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_A )
        {
            // #145080
            if( m_Xime.m_dwUserIndex < XUSER_MAX_COUNT )
            {
                XShowWordRegisterUI( m_Xime.m_dwUserIndex );
            }
        }
        else if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_START )
        {
            ATG::Xime::m_bLoadTitleDictionary = !ATG::Xime::m_bLoadTitleDictionary;
        }
        else if ( pGamepad->wPressedButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER )
        {
            m_Xime.DeleteJPUserWordAll();
        }   
        else  if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_START )
        {
#ifdef DEBUG_FLUSH_LEARNING
            //ATG::Xime::FlushLearningArea();
#endif // DEBUG_FLUSH_LEARNING
       }
    }
    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Render
// Desc: Sets up render states, clears the viewport, and renders the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Render()
{
    // Draw a gradient filled background
    ATG::RenderBackground( TOP_BACK_COLOR, BOTTOM_BACK_COLOR );


    if( !m_bKeyboardConnected )
    {
        m_Font.Begin();
        m_Font.SetScaleFactors( 1.2f, 1.2f );
        m_Font.DrawText( 100, 300, MSG_COLOR, L"Keyboard is not connected" );

        m_Font.End();
    }

    // Begin Xui rendering
    XuiRenderBegin( m_hDC, D3DCOLOR_ARGB( 255, 0, 0, 0 ) );
 
    // Set the view
    D3DXMATRIX matView;
    D3DXMatrixIdentity( &matView );
    XuiRenderSetViewTransform( m_hDC, &matView );

    D3DCOLOR color = ATG::COLOR_NORMAL;
    //Show typed string
    float fPosY = STRING_POSY;
    float fHeight;

    int iRenderLines = m_iLines + 1;

    for( int i = 0; i < iRenderLines; ++i )
    {
        DrawText( m_hDC, m_hFont, color, STRING_POSX, fPosY,
                  m_TypedString[ i ].c_str(), NULL, &fHeight );

        if( m_Xime.IsIMEOn() && i == m_iLines )
        {
            XUIRect clipRect( 0, 0, (float)m_d3dpp.BackBufferWidth, (float)m_d3dpp.BackBufferHeight );
            XuiMeasureText( m_hFont, m_TypedString[ i ].c_str(), -1,
                            XUI_FONT_STYLE_NORMAL | XUI_FONT_STYLE_SINGLE_LINE,
                            0, &clipRect );

            m_Xime.Render( m_hDC, m_hFont, STRING_POSX+clipRect.right, fPosY );
        }

        fHeight = (float)__fsel( (double)fHeight - FONT_HEIGHT, fHeight, FONT_HEIGHT );
        fPosY += fHeight;
    }
    
    // Complete Xui rendering
    XuiRenderEnd( m_hDC );
    XuiRenderPresent( m_hDC, NULL, NULL, NULL );



    // Show title, frame rate, and help
    m_Timer.MarkFrame();
    if( m_bDrawHelp )
    {
        m_Help.Render( &m_Font, g_HelpCallouts, NUM_HELP_CALLOUTS );

        m_Font.Begin();
        m_Font.SetScaleFactors( 0.8f, 0.8f );
        float fY = 260.f;
        m_Font.DrawText( 0, fY+=20.f, MSG_COLOR, L"Use keyboard to type in words." );
        m_Font.DrawText( 0, fY+=20.f, MSG_COLOR, L"[Alt] Shift: switch language" );
        switch(m_Xime.GetCurrentLanguage())
        {
            case ATG::MODE_JP: 
                m_Font.DrawText( 0, fY+=20.f, MSG_COLOR, L"[Half/Full]: toggle Japanese XIME on/off" );
                m_Font.DrawText( 0, fY+=20.f, MSG_COLOR, L"Space: conversion                   Arrow: move focus" );
                m_Font.DrawText( 0, fY+=20.f, MSG_COLOR, L"Shift+Arrow: change clause length" );
                break;
            case ATG::MODE_TC: 
                m_Font.DrawText( 0, fY+=20.f, MSG_COLOR, L"[Ctrl]Space: Toggle T. Chinese XIME on/off" );
                m_Font.DrawText( 0, fY+=20.f, MSG_COLOR, L"Space: conversion" );
                break;
            case ATG::MODE_KR: 
                m_Font.DrawText( 0, fY+=20.f, MSG_COLOR, L"[HANGUL]: Toggle Korean XIME on/off" );
                break;
        }

        m_Font.End();

    }
    else
    {
        m_Font.Begin();
        m_Font.SetScaleFactors( 1.2f, 1.2f );
        m_Font.DrawText( 0, 0, MSG_COLOR, L"Xime" );
        m_Font.SetScaleFactors( 1.0f, 1.0f );
        m_Font.DrawText( 0, 0, INFO_COLOR, m_Timer.GetFrameRate(), ATGFONT_RIGHT );

        // Display message
        m_Font.SetScaleFactors( 0.8f, 0.8f );
        m_Font.DrawText (100, 10, MSG_COLOR, L"Dashboard:" );
        switch(XGetLanguage())
        {
        case XC_LANGUAGE_ENGLISH:    m_Font.DrawText(MSG_COLOR, L"English");        break;
        case XC_LANGUAGE_JAPANESE:   m_Font.DrawText(MSG_COLOR, L"Japanese");       break;
        case XC_LANGUAGE_GERMAN:     m_Font.DrawText(MSG_COLOR, L"German");         break;
        case XC_LANGUAGE_FRENCH:     m_Font.DrawText(MSG_COLOR, L"French");         break;
        case XC_LANGUAGE_SPANISH:    m_Font.DrawText(MSG_COLOR, L"Spanish");        break;
        case XC_LANGUAGE_ITALIAN:    m_Font.DrawText(MSG_COLOR, L"Italian");        break;
        case XC_LANGUAGE_KOREAN:     m_Font.DrawText(MSG_COLOR, L"Korean");         break;
        case XC_LANGUAGE_TCHINESE:   m_Font.DrawText(MSG_COLOR, L"T.Chinese");      break;
        case XC_LANGUAGE_SCHINESE:   m_Font.DrawText(MSG_COLOR, L"S.Chinese");      break;
        case XC_LANGUAGE_PORTUGUESE: m_Font.DrawText(MSG_COLOR, L"Portuguese");     break;
        default:                     m_Font.DrawText(MSG_COLOR, L"Other Language"); break;
        }

        float fY = 28.f;
        m_Font.DrawText( 0, fY, MSG_COLOR, L"XIME:" );
        if( m_Xime.IsIMEOn() )
            m_Font.DrawText( MSG_COLOR, L"ON");
        else
            m_Font.DrawText( MSG_COLOR, L"OFF" );
        
        if( m_Xime.GetCurrentLanguage() == ATG::MODE_JP ) {
            m_Font.DrawText( 100, fY, MSG_COLOR, L"Lang.:Japanese" );
            m_Font.DrawText( 240, fY, MSG_COLOR, L"Input:" );
            if (m_Xime.GetInputMode() == XIME_MODE_JP_HALFWIDTH_ALPHANUMERIC )
            {
                m_Font.DrawText( MSG_COLOR, L"AlphaNumeric");
            }
            else
            {
                m_Font.DrawText( MSG_COLOR, m_Xime.GetInputMode() & XIME_COMBINE_ROMAJI_HIRAGANA ?
                                  L"Hiragana":L"Katakana" );
                m_Font.DrawText( 380, fY, MSG_COLOR, L"Layout:" );
                m_Font.DrawText( MSG_COLOR, m_Xime.GetKeyboardLayout() & XIME_LAYOUT_KANA ?
                                 L"Kana":L"Romaji" );
            }
            WCHAR buffer[1024];
            swprintf_s(buffer, L"User Reg Words: %d     Static: %s", 
                        m_Xime.GetJPUserWordNumber(), 
                        ATG::Xime::g_bIsStaticXime ? L"Y" : L"N" );
            m_Font.DrawText( 100, 46, MSG_COLOR, buffer);
        }
        else if( m_Xime.GetCurrentLanguage() == ATG::MODE_KR ) {
            m_Font.DrawText( 100, fY, MSG_COLOR, L"Lang.:Korean" );
            m_Font.DrawText( 240, fY, MSG_COLOR, L"Input:" );
            m_Font.DrawText( MSG_COLOR, m_Xime.GetInputMode() & XIME_MODE_KR_HANGUL ?
                              L"Hangul":L"???" );
            m_Font.DrawText( 380, fY, MSG_COLOR, L"Layout:" );
            m_Font.DrawText( MSG_COLOR, L"Hangul");
        }
        else if( m_Xime.GetCurrentLanguage() == ATG::MODE_TC ) {
            m_Font.DrawText( 100, fY, MSG_COLOR, L"Lang.:T.Chinese" );
            m_Font.DrawText( 240, fY, MSG_COLOR, L"Input:" );
            m_Font.DrawText( MSG_COLOR, m_Xime.GetInputMode() & XIME_MODE_CHT_BOPOMOFO ?
                              L"Bopomofo":L"???" );
            m_Font.DrawText( 380, fY, MSG_COLOR, L"Layout:" );
            m_Font.DrawText( MSG_COLOR, L"Bopomofo");
        }
        m_Font.End();
    }

    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: InitXui
// Desc: Initialize the Xui runtime and render libraries.
//--------------------------------------------------------------------------------------
HRESULT Sample::InitXui()
{
    XUIInitParams initparams = {0};
    XUI_INIT_PARAMS( initparams );

    // Typeface descriptor specifies a name and file location for a font
    TypefaceDescriptor desc = {0};
    desc.szTypeface = L"Arial Unicode MS";
    desc.szLocator = L"file://game:/media/xarialuni.ttf";

    // Initialize Xui render library with our D3D device, 
    // and use a Xui-provided texture loader.
    HRESULT hr = XuiRenderInitShared( m_pd3dDevice, &m_d3dpp, XuiD3DXTextureLoader );
    if( FAILED( hr ) )
        goto error;
    m_fXuiRenderInitialized = true;
 
    // Create a Xui device context. The Xui text renderer uses many attributes
    // from this device context (position, color, shaders, etc.).
    hr = XuiRenderCreateDC( &m_hDC );
    if( FAILED( hr ) )
        goto error;

    // Initialize the Xui runtime library.  Typeface descriptors are registered
    // by the runtime library, and consumed by the render library.
    hr = XuiInit( &initparams );
    if( FAILED( hr ) )
        goto error;
    m_fXuiInitialized = true; 
 
    // Register our typeface name and font location.
    hr = XuiRegisterTypeface( &desc, TRUE );
    if( FAILED( hr ) )
        goto error;
 
    // Instantiate an 18pt font.
    hr = XuiCreateFont( L"Arial Unicode MS", 18.0f, 0, 0, &m_hFont );
    if( FAILED( hr ) )
        goto error;
 
    return hr;

error:
    UninitXui();
    return hr;
}


//--------------------------------------------------------------------------------------
// Name: UninitXui
// Desc: Release resources used by the Xui font, device context and render libraries.
//--------------------------------------------------------------------------------------
void Sample::UninitXui()
{
    if( m_hFont != 0 )
    {
        XuiReleaseFont( m_hFont );
        m_hFont = 0;
    }
    
    if( m_hDC != 0 )
    {
        XuiRenderDestroyDC( m_hDC );
        m_hDC = 0;
    }

    if( m_fXuiRenderInitialized )
    {
        XuiRenderUninit();
        m_fXuiRenderInitialized = false;
    }

    if( m_fXuiInitialized )
    {
        XuiUninit();
        m_fXuiInitialized = false;
    }
}


//--------------------------------------------------------------------------------------
// Name: DrawText
// Desc: Draw text at the given coordinates with the given color.
//--------------------------------------------------------------------------------------
void Sample::DrawText( HXUIDC hdc, HXUIFONT hFont, D3DCOLOR color, float x, float y,
                       LPCWSTR text, float * pfX, float * pfY )
{
    // Set the text position in the device context
    D3DXMATRIX matXForm;
    D3DXMatrixIdentity( &matXForm );
    matXForm._41 = x;
    matXForm._42 = y;
    XuiRenderSetTransform( hdc, &matXForm );
    
    // Measure the text
    XUIRect clipRect( 0, 0, m_d3dpp.BackBufferWidth - x, m_d3dpp.BackBufferHeight - y );
    XuiMeasureText( hFont, text, -1, XUI_FONT_STYLE_NORMAL, 0, &clipRect );
 
    // Select the font and color into the device context
    XuiSelectFont( hdc, hFont );
    XuiSetColorFactor( hdc, (DWORD)color );
    
    // Draw the text
    XuiDrawText( hdc, text, XUI_FONT_STYLE_NORMAL, 0, &clipRect );

    if( pfX != NULL ) *pfX = clipRect.GetWidth();
    if( pfY != NULL ) *pfY = clipRect.GetHeight();
    return;
}

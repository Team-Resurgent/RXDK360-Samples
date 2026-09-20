//--------------------------------------------------------------------------------------
// KeyboardScene.cpp
//
// This sample demonstrates a virtual keyboard using XUI.
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include <xtl.h>
#include <xui.h>
#include <xuiapp.h>
#include <d3d9.h>

#include "AtgMediaLocator.h"


//--------------------------------------------------------------------------------------
// Keyboard mapping data
//--------------------------------------------------------------------------------------
struct Keycap
{
    LPCWSTR szId;          // XuiButton Id property in the scene file
    UINT VK;            // Virtual Key code corresponding to the button
    LPCWSTR szNormalCap;   // Text displayed on button normally
    LPCWSTR szShiftCap;    // Text displayed on button with Shift active
};

//
// Map each "scan code" (XuiButton ID property) to a virtual key code.
//
static Keycap g_Keys [] =
{
    { L"Key.A", 'A', L"a", L"A" },
    { L"Key.B", 'B', L"b", L"B" },
    { L"Key.C", 'C', L"c", L"C" },
    { L"Key.D", 'D', L"d", L"D" },
    { L"Key.E", 'E', L"e", L"E" },
    { L"Key.F", 'F', L"f", L"F" },
    { L"Key.G", 'G', L"g", L"G" },
    { L"Key.H", 'H', L"h", L"H" },
    { L"Key.I", 'I', L"i", L"I" },
    { L"Key.J", 'J', L"j", L"J" },
    { L"Key.K", 'K', L"k", L"K" },
    { L"Key.L", 'L', L"l", L"L" },
    { L"Key.M", 'M', L"m", L"M" },
    { L"Key.N", 'N', L"n", L"N" },
    { L"Key.O", 'O', L"o", L"O" },
    { L"Key.P", 'P', L"p", L"P" },
    { L"Key.Q", 'Q', L"q", L"Q" },
    { L"Key.R", 'R', L"r", L"R" },
    { L"Key.S", 'S', L"s", L"S" },
    { L"Key.T", 'T', L"t", L"T" },
    { L"Key.U", 'U', L"u", L"U" },
    { L"Key.V", 'V', L"v", L"V" },
    { L"Key.W", 'W', L"w", L"W" },
    { L"Key.X", 'X', L"x", L"X" },
    { L"Key.Y", 'Y', L"y", L"Y" },
    { L"Key.Z", 'Z', L"z", L"Z" },
    { L"Key._", 0xFFFF, L"_", L"_" },           // the underscore should be VK_OEM_MINUS flagged with SHIFT. 
    { L"Key.-", VK_OEM_MINUS, L"-", L"-" },
    { L"Key.0", '0', L"0", L"0" },
    { L"Key.1", '1', L"1", L"1" },
    { L"Key.2", '2', L"2", L"2" },
    { L"Key.3", '3', L"3", L"3" },
    { L"Key.4", '4', L"4", L"4" },
    { L"Key.5", '5', L"5", L"5" },
    { L"Key.6", '6', L"6", L"6" },
    { L"Key.7", '7', L"7", L"7" },
    { L"Key.8", '8', L"8", L"8" },
    { L"Key.9", '9', L"9", L"9" },
    { L"Key..", VK_OEM_PERIOD, L".", L"." },
    { L"Key.OK", VK_RETURN, L"OK", L"OK" },
    { L"Key.Cancel", VK_CANCEL, L"Cancel", L"Cancel" },
    { L"Key.Clear", VK_CLEAR, L"Clear", L"Clear" },
    { L"Key.Space", VK_SPACE, L"Space", L"Space" },
    { L"Key.Shift", VK_SHIFT, L"Shift", L"Shift" },
    { L"Key.Backspace", VK_BACK, L"Backspace", L"Backspace" },
    { L"Key.Left", VK_LEFT, L"Left", L"Left" },
    { L"Key.Right", VK_RIGHT, L"Right", L"Right" },
};

const WCHAR g_successstr[] = L"success";
const UINT          g_successlen = sizeof( g_successstr ) / sizeof( WCHAR ) - 1;

//--------------------------------------------------------------------------------------
// Global display size
//--------------------------------------------------------------------------------------
UINT                g_DisplayHeight;
UINT                g_DisplayWidth;

//--------------------------------------------------------------------------------------
// Xui module declaration
//--------------------------------------------------------------------------------------
class CXuiKeyboardModule : public CXuiModule
{
protected:
    virtual HRESULT RegisterXuiClasses();
    virtual HRESULT UnregisterXuiClasses();
};

//--------------------------------------------------------------------------------------
// Keyboard scene declaration
//--------------------------------------------------------------------------------------
class CKeyboardScene : public CXuiSceneImpl
{
protected:

    HXUIOBJ m_btnMap[sizeof( g_Keys ) / sizeof( g_Keys[0] )];
    HXUIOBJ m_btnCancel;
    HXUIOBJ m_hEditCtl;
    LPWSTR m_laststr;          // last operation
    UINT m_lastcaret;        // last caret postion
    int m_nKeys;
    BOOL m_fShift;
    HXUIOBJ m_hOk;

public:
    XUI_IMPLEMENT_CLASS( CKeyboardScene, L"XuiKeyboardScene", XUI_CLASS_SCENE )

protected:
    // message map
    XUI_BEGIN_MSG_MAP()
        XUI_ON_XM_INIT( OnInit )
        XUI_ON_XM_NOTIFY_PRESSING( OnNotifyPressing )
        XUI_ON_XM_KEYDOWN( OnKeydown )
        XUI_ON_XM_KEYUP( OnKeyup )
    XUI_END_MSG_MAP()

    //----------------------------------------------------------------------------------
    // OnCreate is called after the constructor has been called and m_hObj
    // set to a valid object handle
    // this allows you to perform initialization that can fail
    //----------------------------------------------------------------------------------
    HRESULT OnCreate()
    {
        m_nKeys = sizeof( g_Keys ) / sizeof( g_Keys[0] );

        return S_OK;
    }


    //----------------------------------------------------------------------------------
    // Initialize XUI controls
    //----------------------------------------------------------------------------------
    HRESULT OnInit( XUIMessageInit* pInitData, BOOL& bHandled )
    {
        //
        // Center scene correctly in all video modes
        //
        FLOAT fSceneWidth, fSceneHeight;
        GetBounds( &fSceneWidth, &fSceneHeight );

        // Scale scene
        fSceneWidth = fSceneWidth * g_DisplayHeight / 480;
        fSceneHeight = fSceneHeight * g_DisplayHeight / 480;
        D3DXVECTOR3 objScale( ( FLOAT )g_DisplayHeight / 480.0f, ( FLOAT )g_DisplayHeight / 480.0f, 0.0f );
        SetScale( &objScale );
        SetBounds( fSceneWidth, fSceneHeight );

        D3DXVECTOR3 objPos( ( g_DisplayWidth - fSceneWidth ) / 2.0f, ( g_DisplayHeight - fSceneHeight ) / 2.0f, 0.0f );
        SetPosition( &objPos );

        m_laststr = NULL;
        m_lastcaret = 0;

        //
        // Find the edit control.
        //
        HRESULT hr = GetChildById( L"XuiEdit1", &m_hEditCtl );
        if( FAILED( hr ) )
            return hr;

        //
        // Find the handle for each key object, and remember it
        // in a button map.
        //
        for( int i = 0; i < m_nKeys; ++i )
        {
            hr = GetChildById( g_Keys[i].szId, &m_btnMap[i] );
            if( FAILED( hr ) )
                return hr;

            // remember the Ok button
            if( g_Keys[i].VK == VK_RETURN )
                m_hOk = m_btnMap[i];
        }

        //
        // Find the cancel button specially (we want to flash it
        // on a B button press, see OnKeydown).
        //
        hr = GetChildById( L"Key.Cancel", &m_btnCancel );
        if( FAILED( hr ) )
            return hr;

        //
        // Start the keyboard in lowercase (non-SHIFT) mode.
        //
        m_fShift = FALSE;
        RefreshKeycaps();

        return S_OK;
    }


    //----------------------------------------------------------------------------------
    // When key is pressed, dispatch input.
    //----------------------------------------------------------------------------------
    HRESULT OnNotifyPressing( HXUIOBJ hObjPressing, XUINotifyPressing* pNotifyPressingData, BOOL& bHandled )
    {
        //
        // Look through the handle map to find the hObj of the key pressed.
        // When found, the index will be the in the same position as its
        // other data in the g_Keys table.
        //
        for( int i = 0; i < m_nKeys; ++i )
        {
            if( hObjPressing == m_btnMap[i] )
            {
                DispatchInput( g_Keys[i].VK );
                break;
            }
        }

        return S_OK;
    }


    //----------------------------------------------------------------------------------
    // Handle actual USB keyboard input by routing it directly to
    // the edit control.
    //----------------------------------------------------------------------------------
    HRESULT OnKeydown( XUIMessageInput* pInputData, BOOL& bHandled )
    {
        //
        // If the B button is pressed anywhere, the buttons will
        // ignore it, but it's treated like a Cancel here.
        //
        if( pInputData->dwKeyCode == VK_PAD_B )
        {
            bHandled = TRUE;
            if( !( pInputData->dwFlags & XUI_INPUT_FLAG_REPEAT ) )
                XuiControlPress( m_btnCancel, XUSER_INDEX_ANY );
            return S_OK;
        }

        //
        // If the shift key is pressed, toggle the virtual
        // shift state.
        //
        if( !IsGamepadInput( pInputData->dwKeyCode ) &&
            pInputData->dwKeyCode == VK_SHIFT )
        {
            if( !m_fShift )
            {
                ToggleShift();
            }
        }

        //
        // Forward to edit control if from a keyboard.
        //
        if( !IsGamepadInput( pInputData->dwKeyCode ) )
        {
            //
            // Find and press the corresponding key button.
            // Set focus to the Ok button.
            //
            for( int i = 0; i < m_nKeys; ++i )
            {
                if( g_Keys[i].VK == pInputData->dwKeyCode )
                {
                    bHandled = TRUE;

                    if( !XuiElementHasFocus( m_hOk ) )
                    {
                        XuiElementSetFocus( m_hOk );
                    }

                    if( g_Keys[i].VK != VK_SHIFT )
                        XuiControlPress( m_btnMap[i], XUSER_INDEX_ANY );
                    break;
                }
            }
        }

        return S_OK;
    }

    //----------------------------------------------------------------------------------
    // Handle toggleging of the shift state.
    //----------------------------------------------------------------------------------
    HRESULT OnKeyup( XUIMessageInput* pInputData, BOOL& bHandled )
    {
        //
        // If the shift key is released, toggle the virtual
        // shift state.
        //
        if( !IsGamepadInput( pInputData->dwKeyCode ) &&
            pInputData->dwKeyCode == VK_SHIFT )
        {
            if( m_fShift )
            {
                ToggleShift();
            }
        }

        return S_OK;
    }


private:
    //----------------------------------------------------------------------------------
    // Clear a last text.
    //----------------------------------------------------------------------------------
    VOID    ClearLasttext()
    {
        if( m_laststr )
        {
            delete [] m_laststr;
            m_laststr = NULL;
        }
    }


    //----------------------------------------------------------------------------------
    // Save a last text.
    //----------------------------------------------------------------------------------
    VOID    SaveLasttext()
    {
        LPCWSTR str;
        size_t len;

        str = XuiControlGetText( m_hEditCtl );
        if( str )
        {
            len = wcslen( str ) + 1;
            ClearLasttext();
            m_laststr = new WCHAR[len];
            wcscpy_s( m_laststr, len, str );
        }
        else
        {
            ClearLasttext();
            m_laststr = new WCHAR[1];
            wcscpy_s( m_laststr, 1, L"" );
        }
        m_lastcaret = XuiEditGetCaretPosition( m_hEditCtl );
    }


    //----------------------------------------------------------------------------------
    // Restore a last text.
    //----------------------------------------------------------------------------------
    VOID    RestoreLasttext()
    {
        if( m_laststr )
        {
            LPCWSTR str;
            size_t len;
            LPWSTR tmp;
            UINT tmpcaret;

            tmpcaret = XuiEditGetCaretPosition( m_hEditCtl );
            str = XuiControlGetText( m_hEditCtl );
            if( str )
            {
                len = wcslen( str ) + 1;
                tmp = new WCHAR[len];
                wcscpy_s( tmp, len, str );
                XuiControlSetText( m_hEditCtl, m_laststr );
                ClearLasttext();
                m_laststr = new WCHAR[len];
                wcscpy_s( m_laststr, len, tmp );
                delete [] tmp;
            }
            else
            {
                XuiControlSetText( m_hEditCtl, m_laststr );
                ClearLasttext();
                m_laststr = new WCHAR[1];
                wcscpy_s( m_laststr, 1, L"" );
            }
            XuiEditSetCaretPosition( m_hEditCtl, m_lastcaret );
            m_lastcaret = tmpcaret;
        }
    }


    //----------------------------------------------------------------------------------
    // Is input coming from the gamepad or the USB keyboard?
    //----------------------------------------------------------------------------------
    bool    IsGamepadInput( UINT VK )
    {
        return ( VK >= VK_PAD_A && VK <= VK_PAD_RTHUMB_DOWNLEFT );
    }


    //----------------------------------------------------------------------------------
    // Send input character to edit control.
    //----------------------------------------------------------------------------------
    void    DispatchInput( UINT VK )
    {
        //
        // Process the input by routing messages to the edit control.
        // Control keys generally become KEYDOWN/KEYUP messages; characters
        // send those in addition to a CHAR message in the middle.
        //
        switch( VK )
        {
            case VK_SHIFT:
            {
                ToggleShift();
                break;
            }
            case VK_CANCEL:
                RestoreLasttext();
                break;
            case VK_RETURN:
                XuiControlSetText( m_hEditCtl, g_successstr );
                XuiEditSetCaretPosition( m_hEditCtl, g_successlen );
                ClearLasttext();
                break;
            case 0xFFFF:
            {
                //
                // Underscore is normally represented by VK_OEM_MINUS
                // modified by a shift flag.  We stored it in our table
                // with an FFFF marker to distinguish it from normal
                // VK_OEM_MINUS, which is a hyphen.  Send the right
                // thing now for correctness.
                //

                SaveLasttext();
                SendKeydown( VK_OEM_MINUS, XUI_INPUT_FLAG_SHIFT );
                SendChar( L'_' );
                SendKeyup( VK_OEM_MINUS, XUI_INPUT_FLAG_SHIFT );
                break;
            }
            default:
            {
                SaveLasttext();
                SendKeydown( VK );

                //
                // If the key is alphanumeric or a space, send the
                // appropriate char message.
                //
                if( VK >= 'A' && VK <= 'Z' )
                {
                    if( m_fShift )
                        SendChar( ( WCHAR )VK );
                    else
                        SendChar( towlower( ( WCHAR )VK ) );
                }
                else if( ( VK >= '0' && VK <= '9' ) || ( VK == VK_SPACE ) || ( VK == VK_BACK ) )
                {
                    SendChar( ( WCHAR )VK );
                }
                else if( VK == VK_OEM_PERIOD )
                {
                    SendChar( L'.' );
                }
                else if( VK == VK_OEM_MINUS )
                {
                    if( m_fShift )
                        SendChar( L'_' );
                    else
                        SendChar( L'-' );
                }

                SendKeyup( VK );

                break;
            }
        }
    }


    //----------------------------------------------------------------------------------
    // Toggle shift key status.
    //----------------------------------------------------------------------------------
    void    ToggleShift()
    {
        m_fShift = !m_fShift;
        RefreshKeycaps();
    }


    //----------------------------------------------------------------------------------
    // Set the keys to show either lower or uppercase character depending on the shift
    // state.
    //----------------------------------------------------------------------------------
    void    RefreshKeycaps()
    {
        for( int i = 0; i < m_nKeys; ++i )
        {
            DWORD dwId;
            XUIElementPropVal val;

            if( m_fShift )
                val.SetVal( g_Keys[i].szShiftCap );
            else
                val.SetVal( g_Keys[i].szNormalCap );

            XuiObjectGetPropertyId( m_btnMap[i], L"Text", &dwId );
            XuiObjectSetProperty( m_btnMap[i], dwId, 0, &val );
        }
    }


    //----------------------------------------------------------------------------------
    // Send key pressed event to edit control.
    //----------------------------------------------------------------------------------
    void    SendKeydown( UINT VK, DWORD dwFlags = 0 )
    {
        XUIMessage msg;
        XUIMessageInput msgExt;

        XuiMessageInput( &msg, &msgExt, XUI_KEYDOWN, VK, 0, dwFlags, 0 );
        XuiSendMessage( m_hEditCtl, &msg );
    }


    //----------------------------------------------------------------------------------
    // Send character to edit control.
    //----------------------------------------------------------------------------------
    void    SendChar( WCHAR wch )
    {
        XUIMessage msg;
        XUIMessageChar msgExt;

        XuiMessageChar( &msg, &msgExt, wch, 0, 0 );
        XuiSendMessage( m_hEditCtl, &msg );
    }


    //----------------------------------------------------------------------------------
    // Send key released event to edit control.
    //----------------------------------------------------------------------------------
    void    SendKeyup( UINT VK, DWORD dwFlags = 0 )
    {
        XUIMessage msg;
        XUIMessageInput msgExt;

        XuiMessageInput( &msg, &msgExt, XUI_KEYUP, VK, 0, dwFlags, 0 );
        XuiSendMessage( m_hEditCtl, &msg );
    }
};


//--------------------------------------------------------------------------------------
// Name: RegisterXuiClasses
// Desc: Register the keyboard scene class.
//--------------------------------------------------------------------------------------
HRESULT CXuiKeyboardModule::RegisterXuiClasses()
{
    XuiSoundXACTRegister();
    return CKeyboardScene::Register();
}


//--------------------------------------------------------------------------------------
// Name: UnregisterXuiClasses
// Desc: Unregister the keyboard scene class.
//--------------------------------------------------------------------------------------
HRESULT CXuiKeyboardModule::UnregisterXuiClasses()
{
    XuiSoundXACTUnregister();
    CKeyboardScene::Unregister();
    return S_OK;
}

//--------------------------------------------------------------------------------------
// Global instance of the Xui keyboard module.
//--------------------------------------------------------------------------------------
CXuiKeyboardModule  g_XuiKeyboardModule;


//--------------------------------------------------------------------------------------
// Name: InitUI
// Desc: Initializes the UI runtime for the specified CAtgApplication
//       Note that the function also loads the keyboard skin and the scene.
//--------------------------------------------------------------------------------------
HRESULT InitUI( LPDIRECT3DDEVICE9 pd3dDevice, D3DPRESENT_PARAMETERS* pd3dpp )
{
    HRESULT hr;

    // Declare helper necessary to locate resources inside an xzp archive.
    ATG::MediaLocator mediaLocator( L"file://game:/media/xuikeyboardclasslib.xzp" );
    WCHAR szResourceLocator[ ATG::LOCATOR_SIZE ];

    // Retreive display bounds
    g_DisplayWidth = pd3dpp->BackBufferWidth;
    g_DisplayHeight = pd3dpp->BackBufferHeight;

    // Init XUI so that it shares the D3D device.
    // Also disable USB keyboard UI navigation since we implement our own keyboard
    // logic for this application.  Most applications should not disable using the keyboard.
    XUIInitParams initparams =
    {
        0
    };
    initparams.cbSize = sizeof( initparams );
    initparams.dwFlags = XUI_INIT_PARAMS_FLAGS_NOKEYBOARD;

    hr = g_XuiKeyboardModule.InitShared( pd3dDevice, pd3dpp, XuiD3DXTextureLoader, &initparams );
    if( FAILED( hr ) )
        return hr;

    // Register a default typeface
    hr = g_XuiKeyboardModule.RegisterDefaultTypeface( L"Arial Unicode MS", L"file://game:/media/xarialuni.ttf" );
    if( FAILED( hr ) )
        return hr;

    //
    // Load skin. Skins can be loaded at any time, and they will apply to the currently
    // loaded scene. The following syntax extracts the skin_keyboard.xur file from
    // the xuikeyboard.xzp package file. The file extensions change from .xui to .xur
    // when packaged.
    //
    mediaLocator.ComposeResourceLocator( szResourceLocator, ARRAYSIZE( szResourceLocator ), L"xui/", L"skin_keyboard.xur" ); 
    g_XuiKeyboardModule.LoadSkin( szResourceLocator );

    // Load the first scene. The scene will use the above loaded skin.
    mediaLocator.ComposeResourceLocator( szResourceLocator, ARRAYSIZE( szResourceLocator ), L"xui/", NULL ); 
    g_XuiKeyboardModule.LoadFirstScene( szResourceLocator, L"us_keyboard.xur", NULL );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: RenderUI
// Desc: Renders one frame.
//--------------------------------------------------------------------------------------
VOID RenderUI()
{
    XuiTimersRun();
    g_XuiKeyboardModule.RunFrame();
    g_XuiKeyboardModule.Render();
}


//--------------------------------------------------------------------------------------
// Name: UninitUI
// Desc: Frees resources used by XUI.
//--------------------------------------------------------------------------------------
VOID UninitUI()
{
    g_XuiKeyboardModule.Uninit();
}

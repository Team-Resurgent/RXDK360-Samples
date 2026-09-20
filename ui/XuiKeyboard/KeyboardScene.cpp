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
#include <d3d9.h>
#include <AtgMediaLocator.h>


//--------------------------------------------------------------------------------------
// Keyboard mapping data
//--------------------------------------------------------------------------------------
struct KEYCAP
{
    LPCWSTR strId;          // XuiButton Id property in the scene file
    UINT dwVKey;         // Virtual Key code corresponding to the button
    LPCWSTR strNormalCap;   // Text displayed on button normally
    LPCWSTR strShiftCap;    // Text displayed on button with Shift active
};

// Map each "scan code" (XuiButton ID property) to a virtual key code.
static KEYCAP g_Keys[] =
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
    { L"Key._", 0xFFFF, L"_", L"_" },       // The underscore should be VK_OEM_MINUS
    // flagged with SHIFT.
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
const UINT  g_successlen = sizeof( g_successstr ) / sizeof( WCHAR ) - 1;

//--------------------------------------------------------------------------------------
// XUI globals
//--------------------------------------------------------------------------------------
HXUIOBJ     g_hObjRoot;         // Root object currently displayed
HXUIDC      g_hDC;              // Display context
LPWSTR      g_laststr;          // last operation
UINT        g_lastcaret;        // last caret postion

//--------------------------------------------------------------------------------------
// Global instance of the keyboard scene class.
//--------------------------------------------------------------------------------------
XUIClass    g_KeyboardSceneClass;


//--------------------------------------------------------------------------------------
// Members of keyboard scene
//--------------------------------------------------------------------------------------
struct KEYBOARDSCENE
{
    HXUIOBJ m_btnMap[sizeof( g_Keys ) / sizeof( g_Keys[0] )];
    HXUIOBJ m_btnCancel;
    HXUIOBJ m_hEditCtl;
    DWORD m_dwNumKeys;
    BOOL m_bShift;
    HXUIOBJ m_hOk;
};


//--------------------------------------------------------------------------------------
// Name: KeyboardScene_IsGamepadInput()
// Desc: Filters VK codes for gamepad input only.
//--------------------------------------------------------------------------------------
BOOL KeyboardScene_IsGamepadInput( UINT dwVKey )
{
    return ( dwVKey >= VK_PAD_A && dwVKey <= VK_PAD_RTHUMB_DOWNLEFT );
}


//--------------------------------------------------------------------------------------
// Name: KeyboardScene_RefreshKeycaps()
// Desc: Changes the key captions between lowercase and uppercase depending on the state
//       of the shift key flag.
//--------------------------------------------------------------------------------------
VOID KeyboardScene_RefreshKeycaps( KEYBOARDSCENE* pObject )
{
    for( DWORD i = 0; i < pObject->m_dwNumKeys; i++ )
    {
        DWORD dwId;
        XUIElementPropVal val;

        if( pObject->m_bShift )
            XUIElementPropVal_SetString( &val, g_Keys[i].strShiftCap );
        else
            XUIElementPropVal_SetString( &val, g_Keys[i].strNormalCap );

        XuiObjectGetPropertyId( pObject->m_btnMap[i], L"Text", &dwId );
        XuiObjectSetProperty( pObject->m_btnMap[i], dwId, 0, &val );
    }
}


//--------------------------------------------------------------------------------------
// Name: KeyboardScene_ToggleShift()
// Desc: Gets called when the Shift button is pressed
//--------------------------------------------------------------------------------------
VOID KeyboardScene_ToggleShift( KEYBOARDSCENE* pObject )
{
    pObject->m_bShift = !pObject->m_bShift;
    KeyboardScene_RefreshKeycaps( pObject );
}


//--------------------------------------------------------------------------------------
// Name: KeyboardScene_SendKeydown()
// Desc: Sends a keycode to the edit control.
//--------------------------------------------------------------------------------------
VOID KeyboardScene_SendKeydown( KEYBOARDSCENE* pObject, UINT dwVKey, DWORD dwFlags )
{
    XUIMessage msg;
    XUIMessageInput msgExt;

    XuiMessageInput( &msg, &msgExt, XUI_KEYDOWN, dwVKey, 0, dwFlags, 0 );
    XuiSendMessage( pObject->m_hEditCtl, &msg );
}


//--------------------------------------------------------------------------------------
// Name: KeyboardScene_SendChar()
// Desc: Sends a character to the edit control.
//--------------------------------------------------------------------------------------
VOID KeyboardScene_SendChar( KEYBOARDSCENE* pObject, WCHAR ch )
{
    XUIMessage msg;
    XUIMessageChar msgExt;

    XuiMessageChar( &msg, &msgExt, ch, 0, 0 );
    XuiSendMessage( pObject->m_hEditCtl, &msg );
}


//--------------------------------------------------------------------------------------
// Name: KeyboardScene_SendKeyup()
// Desc: Sends a keycode to the edit control.
//--------------------------------------------------------------------------------------
VOID KeyboardScene_SendKeyup( KEYBOARDSCENE* pObject, UINT dwVKey, DWORD dwFlags )
{
    XUIMessage msg;
    XUIMessageInput msgExt;

    XuiMessageInput( &msg, &msgExt, XUI_KEYUP, dwVKey, 0, dwFlags, 0 );
    XuiSendMessage( pObject->m_hEditCtl, &msg );
}

//--------------------------------------------------------------------------------------
// Name: KeyboardScene_ClearLasttext()
// Desc: Clear a last text.
//--------------------------------------------------------------------------------------
VOID KeyboardScene_ClearLasttext()
{
    if( g_laststr )
    {
        delete [] g_laststr;
        g_laststr = NULL;
    }
}

//--------------------------------------------------------------------------------------
// Name: KeyboardScene_SaveLasttext()
// Desc: Save a last text.
//--------------------------------------------------------------------------------------
VOID KeyboardScene_SaveLasttext( KEYBOARDSCENE* pObject )
{
    LPCWSTR str;
    size_t len;

    str = XuiControlGetText( pObject->m_hEditCtl );
    if( str )
    {
        len = wcslen( str ) + 1;
        KeyboardScene_ClearLasttext();
        g_laststr = new WCHAR[len];
        wcscpy_s( g_laststr, len, str );
    }
    else
    {
        KeyboardScene_ClearLasttext();
        g_laststr = new WCHAR[1];
        wcscpy_s( g_laststr, 1, L"" );
    }
    g_lastcaret = XuiEditGetCaretPosition( pObject->m_hEditCtl );
}

//--------------------------------------------------------------------------------------
// Name: KeyboardScene_RestoreLasttext()
// Desc: Restore a last text.
//--------------------------------------------------------------------------------------
VOID KeyboardScene_RestoreLasttext( KEYBOARDSCENE* pObject )
{
    if( g_laststr )
    {
        LPCWSTR str;
        size_t len;
        LPWSTR tmp;
        UINT tmpcaret;

        tmpcaret = XuiEditGetCaretPosition( pObject->m_hEditCtl );
        str = XuiControlGetText( pObject->m_hEditCtl );
        if( str )
        {
            len = wcslen( str ) + 1;
            tmp = new WCHAR[len];
            wcscpy_s( tmp, len, str );
            XuiControlSetText( pObject->m_hEditCtl, g_laststr );
            KeyboardScene_ClearLasttext();
            g_laststr = new WCHAR[len];
            wcscpy_s( g_laststr, len, tmp );
            delete [] tmp;
        }
        else
        {
            XuiControlSetText( pObject->m_hEditCtl, g_laststr );
            KeyboardScene_ClearLasttext();
            g_laststr = new WCHAR[1];
            wcscpy_s( g_laststr, 1, L"" );
        }
        XuiEditSetCaretPosition( pObject->m_hEditCtl, g_lastcaret );
        g_lastcaret = tmpcaret;
    }
}

//--------------------------------------------------------------------------------------
// Name: KeyboardScene_DispatchInput()
// Desc: Performs actions based on what button was pressed.
//--------------------------------------------------------------------------------------
VOID KeyboardScene_DispatchInput( KEYBOARDSCENE* pObject, UINT dwVKey )
{
    // Process the input by routing messages to the edit control.
    // Control keys generally become KEYDOWN/KEYUP messages; characters send those in
    // addition to a CHAR message in the middle.
    switch( dwVKey )
    {
        case VK_CANCEL:
            KeyboardScene_RestoreLasttext( pObject );
            break;
        case VK_RETURN:
            XuiControlSetText( pObject->m_hEditCtl, g_successstr );
            XuiEditSetCaretPosition( pObject->m_hEditCtl, g_successlen );
            KeyboardScene_ClearLasttext();
            break;

        case 0xFFFF:
        {
            // Underscore is normally represented by VK_OEM_MINUS modified by a shift
            // flag.  We stored it in our table with a 0xFFFF marker to distinguish it
            // from normal VK_OEM_MINUS, which is a hyphen.  Send the right thing now
            // for correctness.
            KeyboardScene_SaveLasttext( pObject );
            KeyboardScene_SendKeydown( pObject, VK_OEM_MINUS, XUI_INPUT_FLAG_SHIFT );
            KeyboardScene_SendChar( pObject, L'_' );
            KeyboardScene_SendKeyup( pObject, VK_OEM_MINUS, XUI_INPUT_FLAG_SHIFT );
            break;
        }

        default:
        {
            KeyboardScene_SaveLasttext( pObject );
            KeyboardScene_SendKeydown( pObject, dwVKey, 0 );

            // If the key is alphanumeric or a space, send the appropriate char message.
            if( dwVKey >= 'A' && dwVKey <= 'Z' )
            {
                if( pObject->m_bShift )
                    KeyboardScene_SendChar( pObject, ( WCHAR )dwVKey );
                else
                    KeyboardScene_SendChar( pObject, towlower( ( WCHAR )dwVKey ) );
            }
            else if( ( dwVKey >= '0' && dwVKey <= '9' ) || ( dwVKey == VK_SPACE ) || ( dwVKey == VK_BACK ) )
            {
                KeyboardScene_SendChar( pObject, ( WCHAR )dwVKey );
            }
            else if( dwVKey == VK_OEM_PERIOD )
            {
                KeyboardScene_SendChar( pObject, L'.' );
            }
            else if( dwVKey == VK_OEM_MINUS )
            {
                if( pObject->m_bShift )
                    KeyboardScene_SendChar( pObject, L'_' );
                else
                    KeyboardScene_SendChar( pObject, L'-' );
            }

            KeyboardScene_SendKeyup( pObject, dwVKey, 0 );

            break;
        }
    }
}


//--------------------------------------------------------------------------------------
// Name: KeyboardScene_OnInit()
// Desc: Get control handles and perform other initialization on the scene.
//--------------------------------------------------------------------------------------
HRESULT KeyboardScene_OnInit( HXUIOBJ hSceneObj, XUIMessageInit* pInitData,
                              BOOL& bHandled )
{
    KEYBOARDSCENE* pObject;
    XuiObjectFromHandle( hSceneObj, ( VOID** )&pObject );

    pObject->m_dwNumKeys = sizeof( g_Keys ) / sizeof( g_Keys[0] );

    // Find the edit control.
    XuiElementGetChildById( hSceneObj, L"XuiEdit1", &pObject->m_hEditCtl );

    // Find the handle for each key object, and remember it in a button map.
    for( DWORD i = 0; i < pObject->m_dwNumKeys; i++ )
    {
        XuiElementGetChildById( hSceneObj, g_Keys[i].strId, &pObject->m_btnMap[i] );

        // remember the Ok button
        if( g_Keys[i].dwVKey == VK_RETURN )
        {
            pObject->m_hOk = pObject->m_btnMap[i];
        }
    }

    // Find the cancel button specially (we want to flash it on a B button press).
    XuiElementGetChildById( hSceneObj, L"Key.Cancel", &pObject->m_btnCancel );

    // Start the keyboard in lowercase (non-SHIFT) mode.
    pObject->m_bShift = FALSE;
    KeyboardScene_RefreshKeycaps( pObject );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: KeyboardScene_OnNotifyPressing()
// Desc: Called when a control was pressed.
//--------------------------------------------------------------------------------------
HRESULT KeyboardScene_OnNotifyPressing( HXUIOBJ hSceneObj, HXUIOBJ hObjPressed,
                                        BOOL& bHandled )
{
    KEYBOARDSCENE* pObject;
    XuiObjectFromHandle( hSceneObj, ( VOID** )&pObject );

    // Look through the handle map to find the hObj of the key pressed. When found, the
    // index will be the in the same position as its other data in the g_Keys table.
    for( DWORD i = 0; i < pObject->m_dwNumKeys; i++ )
    {
        if( hObjPressed == pObject->m_btnMap[i] )
        {
            if( g_Keys[i].dwVKey == VK_SHIFT )
            {
                KeyboardScene_ToggleShift( pObject );
            }
            KeyboardScene_DispatchInput( pObject, g_Keys[i].dwVKey );
            break;
        }
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: KeyboardScene_OnKeyDown()
// Desc: A gamepad button is pressed.
//--------------------------------------------------------------------------------------
HRESULT KeyboardScene_OnKeyDown( HXUIOBJ hSceneObj, XUIMessageInput* pInputData,
                                 BOOL& bHandled )
{
    KEYBOARDSCENE* pObject;
    XuiObjectFromHandle( hSceneObj, ( VOID** )&pObject );

    // If the B button is pressed anywhere, the buttons will ignore it, but it's treated
    // like a Cancel here.
    if( pInputData->dwKeyCode == VK_PAD_B )
    {
        bHandled = TRUE;
        if( !( pInputData->dwFlags & XUI_INPUT_FLAG_REPEAT ) )
            XuiControlPress( pObject->m_btnCancel, XUSER_INDEX_ANY );
        return S_OK;
    }

    // If the shift key is pressed, toggle the virtual shift state.
    if( !KeyboardScene_IsGamepadInput( pInputData->dwKeyCode ) &&
        ( pInputData->dwKeyCode == VK_SHIFT ) )
    {
        if( !pObject->m_bShift )
        {
            KeyboardScene_ToggleShift( pObject );
        }
    }

    // Forward to edit control if from a keyboard.
    if( !KeyboardScene_IsGamepadInput( pInputData->dwKeyCode ) )
    {
        // Find and press the corresponding key button. Set the focus to that button.
        for( DWORD i = 0; i < pObject->m_dwNumKeys; i++ )
        {
            if( g_Keys[i].dwVKey == pInputData->dwKeyCode )
            {
                if( !XuiElementHasFocus( pObject->m_hOk ) )
                {
                    XuiElementSetFocus( pObject->m_hOk );
                }
                if( pInputData->dwKeyCode != VK_SHIFT )
                    XuiControlPress( pObject->m_btnMap[i], XUSER_INDEX_ANY );
                bHandled = TRUE;
                break;
            }
        }
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: KeyboardScene_OnKeyUp()
// Desc: A gamepad button is released.
//--------------------------------------------------------------------------------------
HRESULT KeyboardScene_OnKeyUp( HXUIOBJ hSceneObj, XUIMessageInput* pInputData,
                               BOOL& bHandled )
{
    KEYBOARDSCENE* pObject;
    XuiObjectFromHandle( hSceneObj, ( VOID** )&pObject );

    // If the shift key is released, toggle the virtual shift state.
    if( !KeyboardScene_IsGamepadInput( pInputData->dwKeyCode ) &&
        ( pInputData->dwKeyCode == VK_SHIFT ) )
    {
        if( pObject->m_bShift )
        {
            KeyboardScene_ToggleShift( pObject );
        }
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: KeyboardScene_ObjectProc()
// Desc: Main event dispatch procedure.
//--------------------------------------------------------------------------------------
static HRESULT KeyboardScene_ObjectProc( HXUIOBJ hObj, XUIMessage* pMessage, void* pvThis )
{
    switch( pMessage->dwMessage )
    {
        case XM_INIT:
        {
            XUIMessageInit* pData = ( XUIMessageInit* )pMessage->pvData;
            return KeyboardScene_OnInit( hObj, pData, pMessage->bHandled );
        }

        case XM_NOTIFY:
        {
            XUINotify* pNotify = ( XUINotify* )pMessage->pvData;
            if( pNotify->dwNotify == XN_PRESSING )
                return KeyboardScene_OnNotifyPressing( hObj, pNotify->hObjSource,
                                                       pMessage->bHandled );
            break;
        }

        case XM_KEYDOWN:
            return KeyboardScene_OnKeyDown( hObj, ( XUIMessageInput* )pMessage->pvData,
                                            pMessage->bHandled );

        case XM_KEYUP:
            return KeyboardScene_OnKeyUp( hObj, ( XUIMessageInput* )pMessage->pvData,
                                          pMessage->bHandled );
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: KeyboardScene_CreateInstance()
// Desc: Creates the struct for the scene.
//--------------------------------------------------------------------------------------
static HRESULT KeyboardScene_CreateInstance( HXUIOBJ hObj, VOID** ppObject )
{
    KEYBOARDSCENE* pObject = new KEYBOARDSCENE;
    if( pObject == NULL )
        return E_OUTOFMEMORY;

    ( *ppObject ) = ( VOID* )pObject;
    return S_OK;
}

//--------------------------------------------------------------------------------------
// Name: KeyboardScene_DestroyInstance
// Desc: Releases memory used by the scene.
//--------------------------------------------------------------------------------------
static HRESULT KeyboardScene_DestroyInstance( VOID* pObject )
{
    delete ( KEYBOARDSCENE* )pObject;
    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: InitUI
// Desc: Initializes the UI runtime for the specified CAtgApplication
//       Note that the function also loads the keyboard skin and the scene.
//--------------------------------------------------------------------------------------
HRESULT InitUI( LPDIRECT3DDEVICE9 pd3dDevice, D3DPRESENT_PARAMETERS* pd3dpp )
{
    HRESULT hr;

    // Initialize globals
    g_hObjRoot = NULL;
    g_hDC = NULL;
    g_laststr = NULL;
    g_lastcaret = 0;

    // Declare helper necessary to locate resources inside an xzp archive.
    ATG::MediaLocator mediaLocator( L"file://game:/media/xuikeyboard.xzp" );
    WCHAR szResourceLocator[ ATG::LOCATOR_SIZE ];

    // Initialize XUI to share the D3D device
    hr = XuiRenderInitShared( pd3dDevice, pd3dpp, XuiD3DXTextureLoader );
    if( FAILED( hr ) )
        return hr;

    // Create the XUI device context. There is only one DC per application.
    hr = XuiRenderCreateDC( &g_hDC );
    if( FAILED( hr ) )
        return hr;

    // Initialize XUI and disable USB keyboard UI navigation since we implement
    // our own keyboard logic for this application.  Most applications should not
    // disable using the keyboard.
    XUIInitParams initparams =
    {
        0
    };
    initparams.cbSize = sizeof( initparams );
    initparams.dwFlags = XUI_INIT_PARAMS_FLAGS_NOKEYBOARD;
    hr = XuiInit( &initparams );
    if( FAILED( hr ) )
        return hr;

    hr = XuiSoundXACTRegister();
    if( FAILED( hr ) )
        return hr;

    //
    // Create the keyboard scene class. The class name must match the class ID given
    // in the .xui file. This is how XUI associates classes with loaded scenes.
    //
    HXUICLASS hClass;

    ZeroMemory( &g_KeyboardSceneClass, sizeof( g_KeyboardSceneClass ) );
    g_KeyboardSceneClass.szClassName = L"XuiKeyboardScene";
    g_KeyboardSceneClass.szBaseClassName = XUI_CLASS_SCENE;
    g_KeyboardSceneClass.Methods.CreateInstance = KeyboardScene_CreateInstance;
    g_KeyboardSceneClass.Methods.DestroyInstance = KeyboardScene_DestroyInstance;
    g_KeyboardSceneClass.Methods.ObjectProc = KeyboardScene_ObjectProc;

    // Register the keyboard class.
    hr = XuiRegisterClass( &g_KeyboardSceneClass, &hClass );
    if( FAILED( hr ) )
        return hr;

    //
    // Create the main canvas. The returned handle is the handle to the root object
    // currently displayed.
    //
    hr = XuiCreateObject( L"XuiCanvas", &g_hObjRoot );
    if( FAILED( hr ) )
        return hr;

    // Set the bounds of the root element. Note that the bounds specified here should
    // match the bounds specified in the scene file.
    hr = XuiElementSetBounds( g_hObjRoot, 640.0f, 480.0f );
    if( FAILED( hr ) )
        return hr;


    // Register a default typeface
    TypefaceDescriptor typeface =
    {
        L"Arial Unicode MS", L"file://game:/media/xarialuni.ttf"
    };
    hr = XuiRegisterTypeface( &typeface, TRUE );
    if( FAILED( hr ) )
        return hr;

    //
    // Load skin. Skins can be loaded at any time, and they will apply to the currently
    // loaded scene. The following syntax extracts the skin_keyboard.xur file from
    // the xuikeyboard.xzp package file. The file extensions change from .xui to .xur
    // when packaged.
    //
    mediaLocator.ComposeResourceLocator( szResourceLocator, ARRAYSIZE( szResourceLocator ), L"xui/", L"skin_keyboard.xur" ); 
    hr = XuiLoadVisualFromBinary( szResourceLocator, NULL );
    if( FAILED( hr ) )
        return hr;


    HXUIOBJ hScene;

    // Load the first scene. The scene will use the above loaded skin.
    mediaLocator.ComposeResourceLocator( szResourceLocator, ARRAYSIZE( szResourceLocator ), L"xui/", NULL ); 
    hr = XuiSceneCreate( szResourceLocator, L"us_keyboard.xur", NULL, &hScene );
    if( FAILED( hr ) )
        return hr;

    //
    // Display the loaded scene. At this point the above defined procedures for the
    // keyboard class will be called to dispatch events.
    //
    hr = XuiSceneNavigateFirst( g_hObjRoot, hScene, XUSER_INDEX_ANY );
    if( FAILED( hr ) )
    {
        XuiDestroyObject( hScene );
        return hr;
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: DispatchUIKeystroke()
// Desc: Sends keystrokes to XUI.
//--------------------------------------------------------------------------------------
VOID DispatchUIKeystroke( XINPUT_KEYSTROKE* pKeystroke )
{
    XuiProcessInput( pKeystroke );
}


//--------------------------------------------------------------------------------------
// Name: RenderUI()
// Desc: Renders the XUI.
//--------------------------------------------------------------------------------------
VOID RenderUI( LPDIRECT3DDEVICE9 pd3dDevice, UINT dwWidth, UINT dwHeight, float fDeltaTime )
{
    // Run XUI animations.  Note fDeltaTime is in seconds and XUI expects the dt in m.s.
    XuiAnimRun( fDeltaTime * 1000.0f );

    //
    // Render the XUI scene. Since the scene was created for a fixed resolution,
    // here we scale it depending on the render target
    //
    XuiRenderBegin( g_hDC, D3DCOLOR_ARGB( 255, 0, 0, 0 ) );

    D3DXMATRIX matOrigView;
    XuiRenderGetViewTransform( g_hDC, &matOrigView );

    // scale depending on the height of the render target
    D3DXMATRIX matView;
    D3DXVECTOR2 vScaling = D3DXVECTOR2( dwHeight / 480.0f, dwHeight / 480.0f );
    D3DXVECTOR2 vTranslation = D3DXVECTOR2( ( dwWidth / 2 ) - ( 640.0f * dwHeight / ( 2.0f * 480.0f ) ), 0.0f );
    D3DXMatrixTransformation2D( &matView, NULL, 0.0f, &vScaling, NULL, 0.0f, &vTranslation );
    XuiRenderSetViewTransform( g_hDC, &matView );

    XUIMessage msg;
    XUIMessageRender msgRender;
    XuiMessageRender( &msg, &msgRender, g_hDC, 0xffffffff, XUI_BLEND_NORMAL );
    XuiSendMessage( g_hObjRoot, &msg );

    XuiRenderSetViewTransform( g_hDC, &matOrigView );

    XuiRenderEnd( g_hDC );

    XuiTimersRun();
}


//--------------------------------------------------------------------------------------
// Name: UninitUI()
// Desc: Frees resources used by XUI.
//--------------------------------------------------------------------------------------
VOID UninitUI()
{
    KeyboardScene_ClearLasttext();

    if( g_hObjRoot )
        XuiDestroyObject( g_hObjRoot );
    g_hObjRoot = NULL;

    XuiUnregisterClass( g_KeyboardSceneClass.szBaseClassName );

    if( g_hDC )
        XuiRenderDestroyDC( g_hDC );
    g_hDC = NULL;

    XuiSoundXACTUnregister();

    XuiUninit();

    XuiRenderUninit();
}

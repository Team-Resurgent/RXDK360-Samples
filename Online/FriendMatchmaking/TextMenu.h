//--------------------------------------------------------------------------------------
// TextMenu.h
//
// Helper class for implementing text-based menus
//
// Game Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

#include <xtl.h>
#include <AtgApp.h>
#include <AtgFont.h>

class TextMenu
{
public:
    // Constants
    static const UINT MAX_MENUITEMLEN     = 256;

    // structure for defining a text menu
    struct Item
    {
        UINT nID;
        LPCWSTR wstrLabel;
        DWORD dwFlags;
        DWORD dwUserData;
    };

    // flags
    enum Flags
    {
        FLAG_NORMAL         =   0x00,   // none
        FLAG_DYNAMIC        =   0x01,   // will callback to draw
        FLAG_WANTHORIZONTAL =   0x02,   // send message with horizontal controller presses
        FLAG_SELECTABLE     =   0x04,   // pressing A/start doesn't end menu
    };

    // Callback messages
    enum Message
    {                        // description                     dwParam1               dwParam2    return
        MENU_ITEMCHANGED,    // vertical controller movement    1 if up, 0 if down     new sel     unused
        MENU_HORIZONTAL,     // horizontal controller movement  1 if left, 0 if right  cur sel     unused
        MENU_DRAWITEM,       // dynamic item needs to be drawn  pointer to string      index       unused
        MENU_SELECTED,       // A/start pressed                 unused                 index       unused
    };

    // callback data type
    typedef DWORD   ( ATG::Application::*Callback )( Message type, DWORD dwParam1, DWORD dwParam2 );

    // methods
    HRESULT         BeginMenu( _Inout_ ATG::Font* pFont, _Inout_ Item* pItems, UINT cItems );
    HRESULT         SetCallback( _Inout_ ATG::Application* pApp, _Inout_ Callback callback );
    HRESULT         SetColors( D3DCOLOR colNormal, D3DCOLOR colHilight );

    HRESULT         Update( VOID );
    HRESULT         Render( VOID );

    BOOL            IsDone( VOID );
    BOOL            WasCanceled( VOID );
    UINT            GetSelectedIndex( VOID );
    UINT            GetSelectedID( VOID );
    Item* GetSelected( VOID );
    VOID            SetSelectedID( UINT );
    VOID            SetSelectedIndex( UINT );

                    TextMenu() : m_pFont( NULL ),
                                 m_pItems( NULL ),
                                 m_cItems( 0 ),
                                 m_callback( NULL ),
                                 m_pApp( NULL ),
                                 m_bInMenu( FALSE ),
                                 m_nIndex( 0 )
                    {
                    }

private:
    ATG::Font* m_pFont;
    Item* m_pItems;
    UINT m_cItems;
    BOOL m_bInMenu;
    BOOL m_bCanceled;
    UINT m_nIndex;

    // Callbacks
    ATG::Application* m_pApp;
    Callback m_callback;

    // Positioning elements
    UINT m_nCenterX;
    UINT m_nMenuTopY;
    UINT m_nMenuStepY;

    // colors
    D3DCOLOR m_colNormal;
    D3DCOLOR m_colHilight;
};

// LT-Start or A
#define IS_FORWARD(_ppd)    ( ((_ppd)->wPressedButtons & XINPUT_GAMEPAD_A ) || \
                            ( ((_ppd)->wPressedButtons & XINPUT_GAMEPAD_START ) && (_ppd)->bLeftTrigger  ) )


// LT-Back
#define IS_BACKWARD(_ppd)   ( ((_ppd)->wPressedButtons & XINPUT_GAMEPAD_BACK ) && (_ppd)->bLeftTrigger  )

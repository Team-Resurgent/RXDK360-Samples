//--------------------------------------------------------------------------------------
// TextMenu.cpp
//
// Helper class for implementing text-based menus
//
// Game Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include <xtl.h>
#include <AtgUtil.h>
#include <AtgInput.h>
#include "TextMenu.h"

//--------------------------------------------------------------------------------------
// Menu configuration methods
//--------------------------------------------------------------------------------------
HRESULT TextMenu::SetCallback( _Inout_ ATG::Application* pApp, _Inout_ TextMenu::Callback callback )
{
    if( ( m_pApp != 0 ) != ( m_callback != 0 ) )
    {
        ATG::FatalError( "App pointer without callback pointer, or vice versa" );
    }

    m_pApp = pApp;
    m_callback = callback;

    return S_OK;
}


HRESULT TextMenu::SetColors( D3DCOLOR colNormal, D3DCOLOR colHilight )
{
    m_colNormal = colNormal;
    m_colHilight = colHilight;

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Retrieve/set selection information
//--------------------------------------------------------------------------------------
BOOL TextMenu::IsDone( VOID )
{
    return !m_bInMenu;
}


BOOL TextMenu::WasCanceled( VOID )
{
    return m_bCanceled;
}


UINT TextMenu::GetSelectedIndex( VOID )
{
    return m_nIndex;
}


UINT TextMenu::GetSelectedID( VOID )
{
    return m_pItems[ m_nIndex ].nID;
}


TextMenu::Item* TextMenu::GetSelected( VOID )
{
    return &m_pItems[ m_nIndex ];
}


VOID TextMenu::SetSelectedIndex( UINT nIndex )
{
    if( nIndex >= m_cItems )
    {
        ATG::FatalError( "Invalid item index." );
    }

    m_nIndex = nIndex;
}


VOID TextMenu::SetSelectedID( UINT nID )
{
    UINT i;

    for( i = 0; i < m_cItems; i++ )
    {
        if( m_pItems[ i ].nID == nID )
        {
            SetSelectedIndex( i );
            break;
        }
    }

    if( i == m_cItems )
    {
        ATG::FatalError( "Could not find item ID." );
    }
}


//--------------------------------------------------------------------------------------
// Menu initialization
//--------------------------------------------------------------------------------------
HRESULT TextMenu::BeginMenu( _Inout_ ATG::Font* pFont, _Inout_ TextMenu::Item* pItems, UINT cItems )
{
    if( !( pFont && pItems && ( cItems > 1 ) ) )
    {
        return E_INVALIDARG;
    }

    // Save off the values
    m_pFont = pFont;
    m_pItems = pItems;
    m_cItems = cItems;

    // Calculate the drawing parameters
    D3DRECT rcSafeArea = ATG::GetTitleSafeArea();

    m_nCenterX = ( rcSafeArea.x2 - rcSafeArea.x1 ) / 2;

    // Gap between items is equal to the screen height, minus the total item height, 
    // divided by the number of gaps (=items-1)
    UINT nFontHeight = ( UINT )m_pFont->GetFontHeight();
    UINT nGapHeight = ( rcSafeArea.y2 - rcSafeArea.y1 ) - nFontHeight * cItems;
    nGapHeight /= cItems - 1;

    // Cap the gap to one line
    nGapHeight = min( nGapHeight, nFontHeight );

    // Total menu height = items * item height + gaps * gap height
    UINT nMenuHeight = cItems * nFontHeight + ( cItems - 1 ) * nGapHeight;

    // Center the menu in the safe area
    m_nMenuTopY = ( rcSafeArea.y2 - rcSafeArea.y1 - nMenuHeight ) / 2;

    // Step = gap + line height
    m_nMenuStepY = nGapHeight + nFontHeight;

    // Start at index zero
    m_nIndex = 0;

    m_bInMenu = TRUE;
    m_bCanceled = FALSE;

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Menu rendering
//--------------------------------------------------------------------------------------
HRESULT TextMenu::Render( VOID )
{
    HRESULT hr = S_OK;
    UINT m_nPosition = m_nMenuTopY;
    WCHAR wstrDynamic[ MAX_MENUITEMLEN ];

    // Check to make sure we're actually in a menu
    if( !m_bInMenu )
    {
        hr = S_FALSE;
    }

    // loop through the items
    for( UINT i = 0; i < m_cItems && SUCCEEDED( hr ); i++ )
    {
        LPCWSTR wstrDisplay;

        if( !( m_pItems[ i ].dwFlags & FLAG_DYNAMIC ) )
        {
            wstrDisplay = m_pItems[ i ].wstrLabel;
        }
        else
        {
            if( !( m_callback && m_pApp ) )
            {
                ATG::FatalError( "Dynamic menu item with no callback defined" );
            }
            wstrDynamic[ 0 ] = L'\0';
            wstrDisplay = wstrDynamic;

            if( m_pApp )
                ( m_pApp->*m_callback )( MENU_DRAWITEM, ( DWORD )wstrDisplay, i );
        }

        D3DCOLOR col = m_nIndex == i ? m_colHilight : m_colNormal;

        // Draw the item
        m_pFont->DrawText( ( FLOAT )m_nCenterX, ( FLOAT )m_nPosition, col, wstrDisplay, ATGFONT_CENTER_X );
        m_nPosition += m_nMenuStepY;
    }

    return hr;
}


//--------------------------------------------------------------------------------------
// Per-frame processing
//
// This method assumes that ATG::Input::GetMergedInput() has been called this frame
//--------------------------------------------------------------------------------------
HRESULT TextMenu::Update( VOID )
{
    // Check to make sure we're actually in a menu
    if( !m_bInMenu )
    {
        return S_FALSE;
    }

    ATG::GAMEPAD* pGamepad = &ATG::Input::m_DefaultGamepad;

    // Check for d-pad vertical motion
    if( pGamepad->wPressedButtons & ( XINPUT_GAMEPAD_DPAD_DOWN | XINPUT_GAMEPAD_DPAD_UP ) )
    {
        BOOL bUp = pGamepad->wPressedButtons & XINPUT_GAMEPAD_DPAD_UP;

        m_nIndex += bUp ? -1 : 1;
        m_nIndex += m_cItems;
        m_nIndex %= m_cItems;

        if( m_callback )
        {
            ( m_pApp->*m_callback )( MENU_ITEMCHANGED, bUp, m_nIndex );
        }
    }

    // Check for dpad horizontal motion
    if( m_pItems[ m_nIndex ].dwFlags & FLAG_WANTHORIZONTAL )
    {
        if( pGamepad->wPressedButtons & ( XINPUT_GAMEPAD_DPAD_RIGHT | XINPUT_GAMEPAD_DPAD_LEFT ) )
        {
            BOOL bLeft = pGamepad->wPressedButtons & XINPUT_GAMEPAD_DPAD_LEFT;

            if( m_callback )
            {
                ( m_pApp->*m_callback )( MENU_HORIZONTAL, bLeft, m_nIndex );
            }
        }
    }


    if( IS_FORWARD(pGamepad) )
    {
        if( m_pItems[ m_nIndex ].dwFlags & FLAG_SELECTABLE )
        {
            ( m_pApp->*m_callback )( MENU_SELECTED, NULL, m_nIndex );
        }
        else
        {
            // we're done!
            m_bInMenu = FALSE;
            m_bCanceled = FALSE;
        }
    }
    else if( IS_BACKWARD(pGamepad) )
    {
        // we're done!
        m_bInMenu = FALSE;
        m_bCanceled = TRUE;
    }


    return S_OK;
}

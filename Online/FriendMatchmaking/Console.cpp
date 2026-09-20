//--------------------------------------------------------------------------------------
// Console.cpp
//
// Helper class for displaying text messages
//
// Game Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include <malloc.h>
#include "Console.h"

//--------------------------------------------------------------------------------------
// Name: Initialize
// Desc: Set up member variables
//--------------------------------------------------------------------------------------
VOID Console::Initialize( _Inout_ ATG::Font* pFont, D3DCOLOR col )
{
    m_pFont = pFont;
    m_col = col;
}


//--------------------------------------------------------------------------------------
// Name: Printf
// Desc: Add a string to the console
//--------------------------------------------------------------------------------------
VOID Console::Printf( LPCWSTR wstrFormat, ... )
{
    va_list pArgList;
    va_start( pArgList, wstrFormat );

    // Count the required length of the string
    DWORD dwStrLen = _vscwprintf( wstrFormat, pArgList ) + 1;    // +1 = null terminator
    WCHAR* wstrMessage = ( WCHAR* )_malloca( sizeof( WCHAR ) * dwStrLen );
    vswprintf_s( wstrMessage, dwStrLen, wstrFormat, pArgList );

    _DisplayString str;
    str.wstr = wstrMessage;
    str.dwTickCount = GetTickCount();

    strings.push_front( str );

    _freea( wstrMessage );

    va_end( pArgList );
}


//--------------------------------------------------------------------------------------
// Name: Render
// Desc: Render the console
//--------------------------------------------------------------------------------------
VOID Console::Render( VOID )
{
    FLOAT flHeight = m_pFont->GetFontHeight();
    FLOAT flPosition = m_pFont->m_rcWindow.y2 - m_pFont->m_rcWindow.y1 - 2 * flHeight;

    DWORD dwTickCount = GetTickCount();

    for(
        std::list <_DisplayString>::iterator i = strings.begin();
        i != strings.end();
        i++ )
    {
        DWORD dwElapsed = dwTickCount - i->dwTickCount;
        if( dwElapsed > DISPLAY_INTERVAL + FADE_INTERVAL )
        {
            strings.erase( i, strings.end() );
            break;
        }

        // render the string
        D3DCOLOR col = m_col;

        if( dwElapsed > DISPLAY_INTERVAL )
        {
            // fade
            col &= ( 1 << 24 ) - 1;

            DWORD fade = ( 255 * ( FADE_INTERVAL - ( dwElapsed - DISPLAY_INTERVAL ) ) ) / FADE_INTERVAL;

            col |= fade << 24;
        }

        if( flPosition > 0 )
        {
            m_pFont->DrawText( 0, flPosition, col, i->wstr.c_str(), ATGFONT_LEFT );
            flPosition -= flHeight;
        }
    }
}

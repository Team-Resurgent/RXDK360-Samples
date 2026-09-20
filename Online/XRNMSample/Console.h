//--------------------------------------------------------------------------------------
// Console.h
//
// Helper class for displaying text messages
//
// Game Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include <xtl.h>
#include <AtgFont.h>
#include <string>
#pragma warning (disable:4127) // conditional expression constant in STL
#include <list>
#pragma warning (default:4127)

class Console
{
public:
    VOID    Initialize( _Inout_ ATG::Font* pFont, D3DCOLOR col );
    VOID    Printf( LPCWSTR wstrFormat, ... );
    VOID    Render( VOID );

private:
    // constants
    static const DWORD DISPLAY_INTERVAL = 10000;
    static const DWORD FADE_INTERVAL    =  5000;

    struct _DisplayString
    {
        DWORD dwTickCount;
        std::wstring wstr;
    };

    std::list <_DisplayString> strings;

    ATG::Font* m_pFont;
    D3DCOLOR m_col;
};

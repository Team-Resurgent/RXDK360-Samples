//-----------------------------------------------------------------------------
// DynamicLinkLibrary.cpp
//
// Demonstrates the use of Dynamic Link Library (aka DLL) by
// Using the Load/FreeLibrary API.
//
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#include <xtl.h>
#include <assert.h>

#include "AtgConsole.h"
#include "AtgUtil.h"
#include "AtgInput.h"


//-----------------------------------------------------------------------------
// Typedefs
//-----------------------------------------------------------------------------
typedef void (*LPFUNCTION )( ATG::Console* lpConsole );


//-----------------------------------------------------------------------------
// Globals
//-----------------------------------------------------------------------------
ATG::Console    g_console;                // console for output
const INT       MODULE_NAME = 1;             // Ordinal for ModuleName in the DLL
const INT       FUNCTION_1 = 2;             // Ordinal for Function1 in the DLL


//-----------------------------------------------------------------------------
// Name: LoadDLL()
// Desc: Load DLL module and retrieve function address.
//       The caller needs to know the name of the function.
//-----------------------------------------------------------------------------
VOID LoadDLL( HMODULE* pModule )
{
    HMODULE hDLL;

    // Load DLL
    hDLL = LoadLibrary( "game:\\dllmodule.dll" );
    if( !hDLL )
        hDLL = LoadLibrary( "game:\\dllmodule.xex" );

    if( !hDLL )
        ATG::FatalError( "Failed to load DLL\n" );

    g_console.Format( "DLL loaded... " );

    // Retrieve string address
    // Note: Only ordinals are supported now
    TCHAR* lpModuleName = ( TCHAR* )GetProcAddress( hDLL, ( LPCSTR )MODULE_NAME );
    if( !lpModuleName )
        ATG::FatalError( "Failed to obtain string address\n" );
    else
        g_console.Format( lpModuleName );
    // Retrieve function address
    // Note: Only ordinals are supported now
    LPFUNCTION lpFunction = ( LPFUNCTION )GetProcAddress( hDLL, ( LPCSTR )FUNCTION_1 );
    if( !lpFunction )
        ATG::FatalError( "Failed to obtain function address\n" );

    // Call it
    g_console.Format( "\nCalling Function1( 0x%p)\n", lpFunction );
    lpFunction( &g_console );

    *pModule = hDLL;
}


//-----------------------------------------------------------------------------
// Name: main()
// Desc: Entry point to the program.
//-----------------------------------------------------------------------------
INT __cdecl main()
{
    HMODULE hModule;

    // Initialize the console window
    g_console.Create( "game:\\Media\\Fonts\\Arial_12.xpr", 0xFF0000FF, 0xFFFFFFFF );
    g_console.Format( "Loading DLL\n" );

    LoadDLL( &hModule );

    g_console.Format( "Press LT + RT + RB to exit\n" );
    g_console.Format( "Press A to reload DLL\n" );

    for(; ; )
    {
        ATG::GAMEPAD* pGamepad = ATG::Input::GetMergedInput(); // Detect reboot keypress
        if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_A )
        {
            // Free library
            FreeLibrary( hModule );
            g_console.Format( "DLL Freed\n" );
            // Reload DLL
            LoadDLL( &hModule );
        }
    }
}

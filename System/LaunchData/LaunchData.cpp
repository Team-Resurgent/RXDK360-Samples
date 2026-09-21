//--------------------------------------------------------------------------------------
// LaunchData.cpp
//
// Shows usage of XEX launch data used to pass data from one executable to another.
// XEX launch data can be used by games directly. Launch data is also passed to games
// when they are launched in demo modes.
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include <xtl.h>
#include "AtgConsole.h"
#include "AtgUtil.h"
#include "AtgInput.h"


//--------------------------------------------------------------------------------------
// Custom struct showing how data can be passed from XEX to XEX. This struct
// can contain any type of layout as long as the total struct size is less than
// MAX_LAUNCH_DATA_SIZE. We recommend using a DWORD identifier as the first item 
// and setting it to something other than LAUNCH_DATA_DEMO_ID.
//--------------------------------------------------------------------------------------
struct CustomLaunchData
{
    static const DWORD MAX_COMMAND_LINE = MAX_LAUNCH_DATA_SIZE - sizeof( DWORD );

    DWORD dwID;
    CHAR szCommandLine[ MAX_COMMAND_LINE ];
};


//--------------------------------------------------------------------------------------
// Name: main
// Desc: Entry point to the program
//--------------------------------------------------------------------------------------
INT __cdecl main()
{
    // Initialize the console window
    ATG::Console console;
    console.Create( "game:\\Media\\Fonts\\Arial_12.xpr", 0xFF0000FF, 0xFFFFFFFF );

    LD_DEMO* pDemoData = NULL; // set if app launched by demo launcher
    CustomLaunchData* pCustomData = NULL; // set if custom data passed to us

    // Determine if launch data was passed to this XEX
    DWORD dwLaunchDataSize = 0;
    DWORD dwStatus = XGetLaunchDataSize( &dwLaunchDataSize );
    if( dwStatus == ERROR_SUCCESS )
    {
        // Store the launch data
        BYTE* pLaunchData = new BYTE [ dwLaunchDataSize ];
        dwStatus = XGetLaunchData( pLaunchData, dwLaunchDataSize );
        assert( dwStatus == ERROR_SUCCESS );

        // Determine who launched us
        if( ( ( LD_DEMO* )( pLaunchData ) )->dwID == LAUNCH_DATA_DEMO_ID )
            pDemoData = ( LD_DEMO* )( pLaunchData );
        else
            pCustomData = ( CustomLaunchData* )( pLaunchData );

        // Display the launch data
        if( pDemoData )
        {
            // Data passed from demo launcher
            console.Format( "LD_DEMO data\n"
                            "dwID: 0x%0X\n"
                            "dwRunmode: %lu (XLDEMO_RUNMODE_%s)\n"
                            "dwTimeout: %lu milliseconds (%lu seconds)\n"
                            "szLaunchedXEX: %s\n"
                            "szLauncherXEX: %s\n\n",
                            pDemoData->dwID, // LAUNCH_DATA_DEMO_ID
                            pDemoData->dwRunmode,
                            "USERSELECTED",
                            pDemoData->dwTimeout, pDemoData->dwTimeout / 1000,
                            pDemoData->szLaunchedXEX,
                            pDemoData->szLauncherXEX );
            if( pDemoData->dwTimeout > 0 )
                console.Format( "Press A to restart demo timer\n" );
        }
        else
        {
            // Data passed by XSetLaunchData/XLaunchNewImage
            console.Format( "CustomLaunchData data\n"
                            "dwID: 0x%0X\n"
                            "szCommandLine: %s\n\n",
                            pCustomData->dwID,
                            pCustomData->szCommandLine );
        }
    }
    else
    {
        // Testing of this sample can be done using the XDK Launcher
        console.Format( "Use the XDK Launcher Certification Tools to set the demo timeout value\n" );
        console.Format( "From the XDK Launcher, launch apps in demo mode using RT + A\n" );
    }

    // Start the demo timer
    console.Format( "Press X to exit\n" );
    console.Format( "Press Y to relaunch with custom data\n" );
    BOOL bCustomData = FALSE;
    DWORD dwStart = GetTickCount();
    DWORD dwLastElapsedSec = 0;
    for(; ; )
    {
        ATG::Input::GetMergedInput();

        // X = exit demo
        if( ATG::Input::m_DefaultGamepad.wPressedButtons & XINPUT_GAMEPAD_X )
            break;

        // Y = relaunch with custom data
        if( ATG::Input::m_DefaultGamepad.wPressedButtons & XINPUT_GAMEPAD_Y )
        {
            bCustomData = TRUE;
            break;
        }

        // A = reset demo timer            
        if( ATG::Input::m_DefaultGamepad.wPressedButtons & XINPUT_GAMEPAD_A )
            dwStart = GetTickCount();

        // If demo timer has elapsed, exit
        if( pDemoData != NULL && pDemoData->dwTimeout > 0 )
        {
            DWORD dwElapsedMs = GetTickCount() - dwStart;
            DWORD dwElapsedSec = dwElapsedMs / 1000;
            if( dwElapsedSec != dwLastElapsedSec )
            {
                // Display timer periodically
                console.Format( "Demo timer: %lu second%c\n", dwElapsedSec,
                                dwElapsedSec != 1 ? 's' : ' ' );
                dwLastElapsedSec = dwElapsedSec;
            }
            if( dwElapsedMs > pDemoData->dwTimeout )
                break;
        }
    }

    if( bCustomData )
    {
        // Example of how to pass custom data to another XEX
        CustomLaunchData CustomData = { 0 };
        CustomData.dwID = 0x13579BDF;
        strcpy_s( CustomData.szCommandLine, CustomLaunchData::MAX_COMMAND_LINE,
                  "Game custom launch data" );
        assert( sizeof( CustomData ) <= MAX_LAUNCH_DATA_SIZE );
        XSetLaunchData( &CustomData, sizeof( CustomData ) );
        XLaunchNewImage( "LaunchData.XEX", 0 );
    }
    else if( pDemoData != NULL )
    {
        // We were launched by a demo launcher; return the same data.
        // To test that you are exiting properly, copy the file 
        // finished.xex (located in %xedk%\bin\xbox) to the same directory 
        // that contains the game/demo XEX.
        XSetLaunchData( pDemoData, dwLaunchDataSize );
        XLaunchNewImage( pDemoData->szLauncherXEX, 0 );
    }
    else
    {
        // Return to the default application (XDK launcher or Dash)
        XLaunchNewImage( XLAUNCH_KEYWORD_DEFAULT_APP, 0 );
    }
}

//--------------------------------------------------------------------------------------
// ContentIntegrityVerification.cpp
//
// The sample shows how to use the Content Integrity Verification (CIV) system and APIs 
// to protect your title against piracy.
//
// Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include <xtl.h>
#include <xbdm.h>
#include <xcompress.h>
#include <AtgInput.h>
#include "AtgConsole.h"

//--------------------------------------------------------------------------------------
// Globals
//--------------------------------------------------------------------------------------

// Console for output
ATG::Console    g_Console;


//--------------------------------------------------------------------------------------
// Name: SimulateIO
// Desc: Simulate IO reads from the disc.
//--------------------------------------------------------------------------------------
VOID SimulateIO()
{
    // Use the font file to simulate IO reads
    HANDLE hFile = CreateFile( "game:\\Media\\Fonts\\Courier_New_11.xpr", GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL );
    if( hFile == INVALID_HANDLE_VALUE )
    {
        g_Console.Format( "Error: Could not open file for simulated IO.\n" );
        return;
    }

    DWORD dwFileSize = GetFileSize( hFile, NULL );

    VOID* pBuffer = malloc( dwFileSize );
    if( pBuffer == NULL )
    {
        g_Console.Format( "Error: Out of memory.\n" );
        CloseHandle( hFile );
        return;
    }

    DWORD dwBytes = 0;
    ReadFile( hFile, pBuffer, dwFileSize, &dwBytes, NULL );

    free( pBuffer );

    CloseHandle( hFile );
}


//--------------------------------------------------------------------------------------
// Name: main
// Desc: Entry point to the program
//--------------------------------------------------------------------------------------
VOID __cdecl main()
{
    srand( GetTickCount() );

    // Initialize the console window
    g_Console.Create( "game:\\Media\\Fonts\\Courier_New_11.xpr", 0xFF1F005F, 0xFFFFFFFF );

    // Initialize transparent decompression with default parameters
    g_Console.Format( "\nInitializing CIV...\n" );

    DWORD dwErr = XSecurityCreateProcess( CIV_DEFAULT_THREAD );
    if( dwErr != ERROR_SUCCESS )
    {
        g_Console.Format( "Error: Could not initialize CIV.\n" );
        return;
    }

    g_Console.Format( "\nPress A to inject hash errors\n" );
    g_Console.Format( "Press X to inject read errors\n" );
    g_Console.Format( "Hold LT + RT + RB to exit.\n" );

    DWORD dwFrameCount = 0;
    BOOL bFailureFlag = FALSE;
    DWORD dwFailureFrames = 0;


    for( ; ; )
    {
        ATG::GAMEPAD* pGamepad = ATG::Input::GetMergedInput(); // Detect reboot keypress
        
        // For testing purposes only, insert a CIV failures
        if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_A )
        {
            g_Console.Format( "Injecting hash errors...\n" );
        
            XSecurityInjectErrors( 1, 0 );
        }

        // For testing purposes only, insert a CIV failures
        if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_X )
        {
            g_Console.Format( "Injecting read errors...\n" );
        
            XSecurityInjectErrors( 0, 100 );
        }

        SimulateIO();

        // When no IO is happening, every once in a while, verify content
        if( ( dwFrameCount % 200 ) == 0 )
        {
            dwErr = XSecurityVerify( 500,       // Limit verification to this many ms
                                    NULL,       // Not using OVERLAPPED - API is always asynchronous
                                    NULL );     // No completion routine

            if( dwErr != ERROR_SUCCESS )
            {
                g_Console.Format( "Error: XSecurityVerify - Not a content integrity failure\n" );
            }
        }

        XSECURITY_FAILURE_INFORMATION FailureInformation = {0};
        FailureInformation.dwSize = sizeof( XSECURITY_FAILURE_INFORMATION );

        if( XSecurityGetFailureInfo( &FailureInformation ) == ERROR_SUCCESS )
        {
            // A CIV failure, that is a content integrity breach is considered when one or more hash failures
            // have happened or when more than 10% of the checked blocks can't be read.
            if( FailureInformation.dwFailedHashes > 0 ||
                ( FailureInformation.dwBlocksChecked > 0 && ( (FLOAT)FailureInformation.dwFailedReads / (FLOAT)FailureInformation.dwBlocksChecked ) > 0.1f ) )
            {
                // In case of a failure the game can take any action. However some soft transition or delayed action is
                // reccomended to hide when and where a CIV failure happened.
                bFailureFlag = TRUE;
                dwFailureFrames = rand() % 1000;
            }
        }

        // If a CIV failure has happened, and timeout has expired, show message box
        // A random timeout is used to hide when and where a failure happened
        if( bFailureFlag && dwFailureFrames == 0 )
        {
            XShowAntiPiracyUI( TRUE );  // Terminate title
        }

        // Simulate 30 Hz framerate
        Sleep( 33 );

        dwFrameCount++;
        if( dwFailureFrames > 0 ) dwFailureFrames--;
    }
}

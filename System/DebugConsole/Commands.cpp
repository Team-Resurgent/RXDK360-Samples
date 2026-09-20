//--------------------------------------------------------------------------------------
// Commands.cpp
//
// Remote Xbox Debug Console
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include "DebugConsole.h"
#include "xbdm.h"


//--------------------------------------------------------------------------------------
// Name: ExtNotifyFunc
// Desc: Notifier function registered via DmRegisterNotificationProcessor 
//       below. This is called to return output from the remote External
//       Command Processor.
//--------------------------------------------------------------------------------------
DWORD __stdcall ExtNotifyFunc( const CHAR* strNotification )
{
    EnqueueStringForPrinting( RGB( 0, 0, 255 ), "%s\n", strNotification );
    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: HandleDebugString
// Desc: Prints debug output to the output window, if desired
//--------------------------------------------------------------------------------------
DWORD __stdcall HandleDebugString( ULONG dwNotification, DWORD dwParam )
{
    if( g_bDebugMonitor )
    {
        PDMN_DEBUGSTR p = ( PDMN_DEBUGSTR )dwParam;

        // The string may not be null-terminated, so make a terminated copy
        // for printing
        CHAR* strTemp = new CHAR[ p->Length + 1 ];
        memcpy( strTemp, p->String, p->Length * sizeof( CHAR ) );
        strTemp[ p->Length ] = 0;
        EnqueueStringForPrinting( RGB( 160, 160, 160 ), "Dbg: %s\n", strTemp );

        delete[] strTemp;
    }

    // Don't let the compiler complain about unused parameters
    ( VOID )dwNotification;

    return 0;
}


//--------------------------------------------------------------------------------------
// Name: RCmdHelp
// Desc: Handles the "help" command.  If no args, prints a list of built-in
//       and remote commands (remote only if connected).  If a command
//       is specified, prints detailed help for that command
//--------------------------------------------------------------------------------------
BOOL RCmdHelp( int argc, char* argv[] )
{
    if( argc <= 1 )
    {
        // No arguments - print out our list of commands, 
        // 3 per line
        for( DWORD i = 0; i < g_dwNumRemoteCommands; i += 3 )
        {
            ConsoleWindowPrintf( CLR_INVALID, "%s\t%s\t%s\n",
                                 g_RemoteCommands[i].strCommand,
                                 ( i + 1 ) < g_dwNumRemoteCommands ? g_RemoteCommands[i + 1].strCommand : "",
                                 ( i + 2 ) < g_dwNumRemoteCommands ? g_RemoteCommands[i + 2].strCommand : "" );
        }

        if( g_bConnected && g_bECPConnected )
        {
            ConsoleWindowPrintf( CLR_INVALID, "Remote Commands:\n" );
            return FALSE;   // Pass the command to External Command Processor
        }
    }
    else
    {
        int cch = lstrlenA( argv[1] );

        // Print help description for all matches
        for( DWORD i = 0; i < g_dwNumRemoteCommands; ++i )
        {
            if( !_strnicmp( g_RemoteCommands[i].strCommand, argv[1], cch ) && g_RemoteCommands[i].strHelp )
            {
                ConsoleWindowPrintf( CLR_INVALID, "%s%s\n", g_RemoteCommands[i].strCommand,
                                     g_RemoteCommands[i].strHelp );
            }
        }
    }

    return TRUE;
}


//--------------------------------------------------------------------------------------
// Name: RCmdCls
// Desc: Handles the CLS command by clearing the output window
//--------------------------------------------------------------------------------------
BOOL RCmdCls( int argc, char* argv[] )
{
    SetWindowText( g_hwndOutputWindow, "" );

    // Don't let the compiler complain about unused parameters
    ( VOID )argc;
    ( VOID )argv;

    return TRUE;
}


//--------------------------------------------------------------------------------------
// Name: RCmdConnect
// Desc: Handles the connect command by first opening a connection to the
//       debug monitor on the Xbox and opening a notification session.  
//       Then we set notification handlers for debug output and our External
//       Command Processor. Finally, we send a special __connect__ command to
//       our External Command Processor to let it initialize itself, if an
//       ECP-enabled app is running.
//--------------------------------------------------------------------------------------
BOOL RCmdConnect( int argc, char* argv[] )
{
    HRESULT hr;

    // Set the Xbox machine name to connect to, if specified
    if( argc > 1 && argv[1][0] )
    {
        hr = DmSetXboxName( argv[1] );
        if( FAILED( hr ) )
            DisplayError( NULL, "DmSetXboxName", hr );
    }

    // Open our connection
    hr = DmOpenConnection( &g_pdmConnection );
    if( FAILED( hr ) )
    {
        DisplayError( NULL, "DmOpenConnection", hr );
        return TRUE;
    }

    g_bConnected = TRUE;

    // Make sure we'll be able to receive notifications
    hr = DmOpenNotificationSession( 0, &g_pdmnSession );
    if( FAILED( hr ) )
    {
        DisplayError( NULL, "DmOpenNotificationSession", hr );
        return TRUE;
    }
    hr = DmNotify( g_pdmnSession, DM_DEBUGSTR, HandleDebugString );
    if( FAILED( hr ) )
    {
        DisplayError( NULL, "DmNotify", hr );
        return TRUE;
    }
    g_bDebugMonitor = FALSE;

    hr = DmRegisterNotificationProcessor( g_pdmnSession, DEBUGCONSOLE_COMMAND_PREFIX, ExtNotifyFunc );
    if( FAILED( hr ) )
    {
        DisplayError( NULL, "DmRegisterNotificationProcessor", hr );
        return TRUE;
    }

    // Send initial connect command to the External Command Processor so it knows we're here
    {
        DWORD dwResponseLen = MAX_PATH;
        CHAR strResponse[MAX_PATH];

        hr = DmSendCommand( g_pdmConnection, DEBUGCONSOLE_COMMAND_PREFIX "!__connect__", strResponse, &dwResponseLen );
        if( FAILED( hr ) )
        {
            ConsoleWindowPrintf( RGB( 255, 0, 0 ),
                                 "Couldn't connect to Application - standard debug commands only\n" );
        }
        else
        {
            g_bECPConnected = TRUE;
            if( dwResponseLen )
                ConsoleWindowPrintf( RGB( 0, 0, 255 ), "%s\n", strResponse );
        }
    }

    return TRUE;
}


//--------------------------------------------------------------------------------------
// Name: RCmdDisconnect
// Desc: Handles the disconnect command by
//          1) Shut down our notifications
//          2) Close the notification session
//          3) Close the connection
//--------------------------------------------------------------------------------------
BOOL RCmdDisconnect( int argc, char* argv[] )
{
    if( g_bConnected )
    {
        ConsoleWindowPrintf( CLR_INVALID, "Closing connection\n" );
        DmNotify( g_pdmnSession, DM_NONE, NULL );
        DmCloseNotificationSession( g_pdmnSession );
        DmCloseConnection( g_pdmConnection );
        g_bConnected = FALSE;
        g_bECPConnected = FALSE;
    }

    // Don't let the compiler complain about unused parameters
    ( VOID )argc;
    ( VOID )argv;

    return TRUE;
}


//--------------------------------------------------------------------------------------
// Name: RCmdQuit
// Desc: Handles the quit command by posting a WM_CLOSE message to our window
//--------------------------------------------------------------------------------------
BOOL RCmdQuit( int argc, char* argv[] )
{
    PostMessage( g_hDlgMain, WM_CLOSE, 0, 0 );

    // Don't let the compiler complain about unused parameters
    ( VOID )argc;
    ( VOID )argv;

    return TRUE;
}


//--------------------------------------------------------------------------------------
// Name: RCmdSendFile
// Desc: Handles the sendfile command by sending the arguments along to
//       DmSendFile
//--------------------------------------------------------------------------------------
BOOL RCmdSendFile( int argc, char* argv[] )
{
    if( argc != 3 )
        return TRUE;

    return SUCCEEDED( DmSendFile( argv[1], argv[2] ) );
}


//--------------------------------------------------------------------------------------
// Name: RCmdGetFile
// Desc: Handles the getfile command by sending the arguments along to
//       DmReceiveFile
//--------------------------------------------------------------------------------------
BOOL RCmdGetFile( int argc, char* argv[] )
{
    if( argc != 3 )
        return TRUE;

    return SUCCEEDED( DmReceiveFile( argv[2], argv[1] ) );
}

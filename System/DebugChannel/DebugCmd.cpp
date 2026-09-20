//--------------------------------------------------------------------------------------
// DebugCmd.cpp
//
// Helps an application expose functionality through the debug channel
// to a debug console running on a remote dev machine.
//
// This source file handles communication between the console and the
// development system.
//
// Commands are sent from the remote debug console through the debug
// channel to the debug monitor on the Xbox machine.  The Xbox machine
// receives the commands on a separate thread through a registered command 
// processor callback function. The callback function will store commands
// in a buffer, and the app should poll this buffer once per frame and
// then decipher and handle the commands.
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include <xtl.h>
#include <xbdm.h>
#include <stdio.h>
#include "DebugCmd.h"


// Command prefix for things sent across the dbg channel
static const CHAR g_strDebugConsoleCommandPrefix[] = "XCMD";

// Global buffer to receive remote commands from the debug console. Note that
// since this data is accessed by the app's main thread, and the debug monitor
// thread, we need to protect access with a critical section
static CHAR g_strRemoteBuf[MAXRCMDLENGTH];

// The critical section used to protect data that is shared between threads
static CRITICAL_SECTION g_CriticalSection;

// A buffer to hold binary data received from the debug console
static BYTE*            g_pBinaryReceiveBuffer = NULL;

// Capacity of the receive buffer in bytes
static CONST
DWORD                   g_dwReceiveBufferCapacity = 262144;

// A buffer to hold binary data being sent to the debug console
static BYTE*            g_pBinarySendBuffer = NULL;

// Capacity of the send buffer in bytes
static CONST
DWORD                   g_dwSendBufferCapacity = 70000;

// Size of the data in the receive buffer in bytes
static DWORD            g_dwReceiveBufferDataSize = 0;

// Bool flag to indicate transfer in progress
static BOOL             g_bBinaryTransferInProgress = FALSE;

// List of app-defined remote commands
const REMOTE_COMMAND g_RemoteCommands[] =
{
    // Command,  Handler,     Help string
    { "help",    RCmdHelp,    " [CMD]: List commands / usage" },
    { "set",     RCmdSet,     " var [=] <val>: set a variable" },
    { "texture", RCmdTexture, " <filename>: Sets the texture to be used" },
    { "spin",    RCmdSpin,    " <rad/s>: Sets spin velocity in radians per second" },
};

const DWORD             g_dwNumRemoteCommands = ( sizeof( g_RemoteCommands ) / sizeof( g_RemoteCommands[0] ) );

// Our command processor callback function.
static HRESULT __stdcall DebugConsoleCmdProcessor( const CHAR* strCommand,
                                                   CHAR* strResponse, DWORD dwResponseLen,
                                                   PDM_CMDCONT pdmcc );


//--------------------------------------------------------------------------------------
// Name: InitializeCommandProcessing
// Desc: Call this function to initialize the command monitor and the critical
//       section that it uses to control data access.
//       This function can safely be called multiple times.
//--------------------------------------------------------------------------------------
static VOID InitializeCommandProcessing()
{
    // Initialize ourselves when we're first called.
    static BOOL s_bInitialized = FALSE;
    if( !s_bInitialized )
    {
        // Register our command handler with the debug monitor
        // The prefix is what uniquely identifies our command handler. Any
        // commands that are sent to the console with that prefix and a '!'
        // character (i.e.; XCMD!data data data) will be sent to
        // our callback.
        HRESULT hr = DmRegisterCommandProcessor( g_strDebugConsoleCommandPrefix,
                                                 DebugConsoleCmdProcessor );
        if( FAILED( hr ) )
            return;

        // We'll also need a critical section to protect access to g_strRemoteBuf
        InitializeCriticalSection( &g_CriticalSection );

        g_pBinaryReceiveBuffer = new BYTE[ g_dwReceiveBufferCapacity ];
        g_pBinarySendBuffer = new BYTE[ g_dwSendBufferCapacity ];

        s_bInitialized = TRUE;
    }
}


//--------------------------------------------------------------------------------------
// Name: RCmdHelp
// Desc: Callback handler for remote "help" command. Iterates over the list of
//       commands and displays a help string for each one
//--------------------------------------------------------------------------------------
VOID RCmdHelp( int argc, char* argv[] )
{
    for( DWORD i = 0; i < g_dwNumRemoteCommands; ++i )
    {
        DebugConsolePrintf( "%s\t%s\n", g_RemoteCommands[i].strCommand,
                            g_RemoteCommands[i].strHelp );
    }
}


//--------------------------------------------------------------------------------------
// Name: RCmdSet
// Desc: Callback handler for  remote "set" command. This function sets or
//       displays values of variables exposed by the app to the debug console
//--------------------------------------------------------------------------------------
VOID RCmdSet( int argc, char* argv[] )
{
    // If we aren't passed any arguments, then just list all the variables and
    // what their current values are.
    if( argc == 1 )
    {
        DebugConsolePrintf( "ERROR: Need to specify a variable name and value.\n" );
        DebugConsolePrintf( "Available variables and their values are:\n" );
        for( DWORD nIndex = 0; nIndex < g_dwNumRemoteVariables; ++nIndex )
        {
            const REMOTE_VARIABLE* pCommandVarDef = &g_RemoteVariables[nIndex];

            switch( pCommandVarDef->ddtDataType )
            {
                case SDOS_BOOL:
                    DebugConsolePrintf( "   %-8s = %d\n", pCommandVarDef->strName,
                                        *( BOOL* )pCommandVarDef->pAddr );
                    break;
                case SDOS_INT:
                    DebugConsolePrintf( "   %-8s = %d\n", pCommandVarDef->strName,
                                        *( INT* )pCommandVarDef->pAddr );
                    break;
                case SDOS_WORD:
                    DebugConsolePrintf( "   %-8s = %d\n", pCommandVarDef->strName,
                                        *( WORD* )pCommandVarDef->pAddr );
                    break;
                case SDOS_FLOAT:
                    DebugConsolePrintf( "   %-8s = %0.1f\n", pCommandVarDef->strName,
                                        *( FLOAT* )pCommandVarDef->pAddr );
                    break;
            }
        }

        return;
    }

    // Else, set the variable to the amount specified    

    // If the user did a set "foo = 2" move arg3 to arg2
    if( argv[2][0] == '=' )
        argv[2] = argv[3];

    // Find the entry for this variable, if we can
    for( DWORD nIndex = 0; nIndex < g_dwNumRemoteVariables; ++nIndex )
    {
        const REMOTE_VARIABLE* pCommandVarDef = &g_RemoteVariables[nIndex];

        if( !lstrcmpiA( argv[1], pCommandVarDef->strName ) )
        {
            CHAR* endptr;
            DWORD dwVal = ( argv[2][0] == '0' && argv[2][1] == 'x' ) ?
                strtoul( argv[2], &endptr, 16 ) : atoi( argv[2] );

            // Set the appropriate type of data
            VOID* pAddr = pCommandVarDef->pAddr;

            switch( pCommandVarDef->ddtDataType )
            {
                case SDOS_BOOL:
                    DebugConsolePrintf( "set %s = %d\n", argv[1], *( BOOL* )pAddr = !!dwVal );
                    break;
                case SDOS_INT:
                    DebugConsolePrintf( "set %s = %d\n", argv[1], *( INT* )pAddr = dwVal );
                    break;
                case SDOS_WORD:
                    DebugConsolePrintf( "set %s = %d\n", argv[1], *( WORD* )pAddr = ( WORD )dwVal );
                    break;
                case SDOS_FLOAT:
                    DebugConsolePrintf( "set %s = %0.1f\n", argv[1], *( FLOAT* )pAddr = ( float )atof( argv[2] ) );
                    break;
            }

            // Call the notif func if there was one
            if( pCommandVarDef->pfnNotifFunc )
                pCommandVarDef->pfnNotifFunc( pAddr );

            return;
        }
    }

    // If we get here, print a message saying we couldn't find the variable
    DebugConsolePrintf( "variable '%s' not found\n", argv[1] );
}


//--------------------------------------------------------------------------------------
// Name: CmdToArgv
// Desc: Parses a string into argv and return # of args.
//--------------------------------------------------------------------------------------
static int CmdToArgv( char* str, char* argv[], int maxargs )
{
    int argc = 0;
    int argcT = 0;
    char* strNil = str + strlen( str );

    while( argcT < maxargs )
    {
        // Eat whitespace
        while( *str && ( *str == ' ' ) )
            str++;

        if( !*str )
        {
            argv[argcT++] = ( char* )strNil;
        }
        else
        {
            // Find the end of this arg
            char chEnd = ( *str == '"' || *str == '\'' ) ? *str++ : ' ';
            char* strArgEnd = str;
            while( *strArgEnd && ( *strArgEnd != chEnd ) )
                strArgEnd++;

            // Record this bad boy
            argv[argcT++] = str;
            argc = argcT;

            // Move szArg to the next argument (or not)
            str = *strArgEnd ? strArgEnd + 1 : strArgEnd;
            *strArgEnd = 0;
        }
    }

    return argc;
}


//--------------------------------------------------------------------------------------
// Temporary replacement for CRT string funcs, since
// we can't call CRT functions on the debug monitor
// thread right now.
//--------------------------------------------------------------------------------------


//--------------------------------------------------------------------------------------
// Name: dbgstrlen
// Desc: Critical section safe strlen() function
//--------------------------------------------------------------------------------------
static int dbgstrlen( const CHAR* str )
{
    const CHAR* strEnd = str;

    while( *strEnd )
        strEnd++;

    return strEnd - str;
}


//--------------------------------------------------------------------------------------
// Name: dbgtolower
// Desc: Returns lowercase of char
//--------------------------------------------------------------------------------------
inline CHAR dbgtolower( CHAR ch )
{
    if( ch >= 'A' && ch <= 'Z' )
        return ch - ( 'A' - 'a' );
    else
        return ch;
}


//--------------------------------------------------------------------------------------
// Name: dbgstrnicmp
// Desc: Critical section safe string compare.
//       Returns zero if the strings match.
//--------------------------------------------------------------------------------------
static INT dbgstrnicmp( const CHAR* str1, const CHAR* str2, int n )
{
    while( n > 0 )
    {
        if( dbgtolower( *str1 ) != dbgtolower( *str2 ) )
            return *str1 - *str2;
        --n;
        ++str1;
        ++str2;
    }

    return 0;
}


//--------------------------------------------------------------------------------------
// Name: dbgstrcpy
// Desc: Critical section safe string copy
//--------------------------------------------------------------------------------------
static VOID dbgstrcpy( CHAR* strDest, const CHAR* strSrc )
{
    while( ( *strDest++ = *strSrc++ ) != 0 );
}


//--------------------------------------------------------------------------------------
// Name: DebugConsoleBinaryReceiveHandler
// Desc: Handles a binary receive operation from the debug channel.
//       This function will be called many times by the debug manager, since the binary 
//       transfer comes over in a series of small chunks (about 1KB - 8KB each).
//       When each chunk is received, the amount of data received is subtracted from the 
//       amount remaining, and when the amount remaining reaches 0, then the transfer is 
//       done.
//--------------------------------------------------------------------------------------
static HRESULT __stdcall DebugConsoleBinaryReceiveHandler( PDM_CMDCONT pdmcc, CHAR* strResponse, DWORD dwResponseLen )
{
    // Get a BYTE pointer to the buffer start
    BYTE* pSrcBuffer = ( BYTE* )pdmcc->Buffer;

    // Subtract the amount of data we read in
    pdmcc->BytesRemaining -= pdmcc->DataSize;

    // Increment the buffer pointer
    pSrcBuffer += pdmcc->DataSize;

    // Update the DM_CMDCONT structure's pointer
    pdmcc->Buffer = pSrcBuffer;

    // Increment our data size variable
    g_dwReceiveBufferDataSize += pdmcc->DataSize;

    // Check if this was the last transfer message
    if( 0 == pdmcc->BytesRemaining )
    {
        // Transfer is finished
        g_bBinaryTransferInProgress = FALSE;
        dbgstrcpy( strResponse, "Binary received." );
    }

    return XBDM_NOERR;
}


//--------------------------------------------------------------------------------------
// Name: DebugConsoleBinarySendHandler
// Desc: Handles a binary send operation to the debug channel.
//       This function sends data to the debug console in blocks of 8192 bytes.  The
//       function is called once per block.  You may experiment with different block
//       sizes based on your memory and performance constraints.
//       The source of the data being sent is g_pBinarySendBuffer, and the function 
//       simply advances a pointer through the buffer as each block is sent.  You can 
//       also use a small fixed buffer big enough to hold just one block, and then each 
//       time copy a bit of data from somewhere in your game into the buffer.
//--------------------------------------------------------------------------------------
static HRESULT __stdcall DebugConsoleBinarySendHandler( PDM_CMDCONT pdmcc, CHAR* strResponse, DWORD dwResponseLen )
{
    if( pdmcc->BytesRemaining <= 0 )
    {
        pdmcc->Buffer = NULL;
        pdmcc->DataSize = 0;
        g_bBinaryTransferInProgress = FALSE;
        dbgstrcpy( strResponse, "Binary sent." );
        return XBDM_ENDOFLIST;
    }

    // Determine how many bytes to send now
    CONST DWORD cBlockSize = 8192;
    DWORD cBytesToSend = min( pdmcc->BytesRemaining, cBlockSize );

    // Increment pointer to the start of the next block to send.
    pdmcc->DataSize = cBytesToSend;
    BYTE* pBuffer = g_pBinarySendBuffer + pdmcc->BufferSize - pdmcc->BytesRemaining;
    pdmcc->Buffer = pBuffer;
    // Update the remaining bytes to reflect this transfer.
    pdmcc->BytesRemaining -= cBytesToSend;

    return XBDM_NOERR;
}


//--------------------------------------------------------------------------------------
// Name: DebugConsoleCmdProcessor
// Desc: Command notification proc that is called by the Xbox debug monitor to
//       have us process a command.  What we'll actually attempt to do is tell
//       it to make calls to us on a separate thread, so that we can just block
//       until we're able to process a command.
//
// Note: Do NOT include newlines in the response string! To do so will confuse
//       the internal WinSock networking code used by the debug monitor API.
//
// Note: It is not possible to set breakpoints in this function with VisualC++.
//       This function is called in the context of a debug system thread that will
//       not stop on breakpoints.
// 
// Note: This function should do as little work as possible because there are
//       many XTL functions that cannot be safely called from this thread. The safest
//       thing to do is to copy the command to a global buffer - protected by a
//       critical section - and let the main thread process the command.
//       Even some of the CRT string handling functions cannot be safely called from
//       this function, which is why dbgstrcpy and dbgstrnicmp exist.
//--------------------------------------------------------------------------------------
static HRESULT __stdcall DebugConsoleCmdProcessor( const CHAR* strCommand,
                                                   CHAR* strResponse, DWORD dwResponseLen,
                                                   PDM_CMDCONT pdmcc )
{
    OutputDebugString( "Command received.\n" );
    // Skip over the command prefix and the exclamation mark
    strCommand += strlen( g_strDebugConsoleCommandPrefix ) + 1;

    // Check if this is the initial connect signal
    if( dbgstrnicmp( strCommand, "__connect__", 11 ) == 0 )
    {
        // If so, respond that we're connected
        lstrcpynA( strResponse, "Connected.", dwResponseLen );
        return XBDM_NOERR;
    }

    // Check if this is the binary receive command
    // Note: The binary receive command can be any string - you just have to agree on 
    // some distinct string between your dev kit app and your debug console app running 
    // on the PC.  Here, we use "__recvbinary__".
    if( dbgstrnicmp( strCommand, "__recvbinary__", 14 ) == 0 )
    {
        // The binary receive signal must be followed by a data size.
        // Read the data size and make sure we have enough buffer space here to hold
        // the data.
        DWORD dwDataSize = atoi( strCommand + 14 );
        if( dwDataSize > g_dwReceiveBufferCapacity )
            return XBDM_INVALIDARG;
        // Set data size to 0, since we're going to overwrite the contents of the buffer.
        g_dwReceiveBufferDataSize = 0;
        // Set in-progress flag
        g_bBinaryTransferInProgress = TRUE;

        // Respond using the PDM_CMDCONT command continuation struct.
        // This result tells the app running on the PC that it is OK to send binary data now.
        // Note that we are pointing the structure directly at our binary receive buffer.
        dbgstrcpy( strResponse, "Ready for binary." );
        pdmcc->HandlingFunction = DebugConsoleBinaryReceiveHandler;
        pdmcc->DataSize = 0;
        pdmcc->Buffer = g_pBinaryReceiveBuffer;
        pdmcc->BufferSize = g_dwReceiveBufferCapacity;
        pdmcc->BytesRemaining = dwDataSize;
        pdmcc->CustomData = NULL;
        // XBDM_READYFORBIN allows the debug console on the PC to call DmSendBinary().
        return XBDM_READYFORBIN;
    }

    // Check if this is the binary send command
    // Note: As with the binary receive command, the binary send command can be any 
    // string, as long as your app and your debug console on the PC agree on the same 
    // string.  You'll probably have many commands that trigger binary send, like 
    // "send backbuffer" or "dump AI state", etc.
    if( dbgstrnicmp( strCommand, "__sendbinary__", 14 ) == 0 )
    {
        // Fill buffer with a letter of the alphabet (will be displayed on the debug console)
        static BYTE CharIndex = 0;
        memset( g_pBinarySendBuffer + sizeof( DWORD ), ( BYTE )'A' + CharIndex, g_dwSendBufferCapacity );
        CharIndex = ( CharIndex + 1 ) % 26;

        // Set the first 4 bytes to the size of the buffer minus 4 bytes.
        *( DWORD* )g_pBinarySendBuffer = ( g_dwSendBufferCapacity - sizeof( DWORD ) );

        // Prepare the continuation structure to send the buffer we just created
        pdmcc->HandlingFunction = DebugConsoleBinarySendHandler;
        pdmcc->Buffer = g_pBinarySendBuffer;
        pdmcc->BufferSize = g_dwSendBufferCapacity;
        pdmcc->BytesRemaining = g_dwSendBufferCapacity;
        pdmcc->CustomData = NULL;
        g_bBinaryTransferInProgress = TRUE;

        return XBDM_BINRESPONSE;
    }

    // Check to see if the cmd exists
    BOOL bKnownCommand = FALSE;

    for( DWORD i = 0; i < g_dwNumRemoteCommands; ++i )
    {
        if( dbgstrnicmp( g_RemoteCommands[i].strCommand, strCommand, dbgstrlen( g_RemoteCommands[i].strCommand ) ) ==
            0 )
        {
            // If we find the string, copy it into the command buffer
            // to be examined by the polling function
            bKnownCommand = TRUE;
            break;
        }
    }

    if( bKnownCommand )
    {
        // g_strRemoteBuf needs to be protected by the critical section
        EnterCriticalSection( &g_CriticalSection );
        if( g_strRemoteBuf[0] )
        {
            // This means the application has probably stopped polling for debug commands
            dbgstrcpy( strResponse, "Cannot execute - previous command still pending" );
        }
        else
        {
            dbgstrcpy( g_strRemoteBuf, strCommand );
        }
        LeaveCriticalSection( &g_CriticalSection );
    }
    else
    {
        dbgstrcpy( strResponse, "unknown command" );
    }

    return XBDM_NOERR;
}


//--------------------------------------------------------------------------------------
// Name: DebugConsoleHandleCommands
// Desc: Poll routine called periodically (typically every frame) by the Xbox
//       app to see if there is a command waiting to be executed, and if so,
//       execute it.
//--------------------------------------------------------------------------------------
BOOL DebugConsoleHandleCommands()
{
    CHAR* argv[10];
    int argc;
    CHAR strLocalBuf[MAXRCMDLENGTH]; // local copy of command

    InitializeCommandProcessing();

    // If there's nothing waiting, return.
    if( !g_strRemoteBuf[0] )
        return FALSE;

    // Grab a local copy of the command received in the remote buffer
    EnterCriticalSection( &g_CriticalSection );

    strcpy_s( strLocalBuf, g_strRemoteBuf );
    g_strRemoteBuf[0] = 0;

    LeaveCriticalSection( &g_CriticalSection );

    // Now parse the newly received command
    argc = CmdToArgv( strLocalBuf, argv, 10 );

    // Find the entry in our command list
    for( DWORD i = 0; i < g_dwNumRemoteCommands; ++i )
    {
        if( !lstrcmpiA( g_RemoteCommands[i].strCommand, argv[0] ) )
        {
            g_RemoteCommands[i].pfnHandler( argc, argv );
            break;
        }
    }

    return TRUE;
}


//--------------------------------------------------------------------------------------
// Name: DebugConsolePrintf
// Desc: Asynchronous printf routine that sends the string to the remote debug
//       console. The prefix determines which client on the development machine
//       will receive the message. Clients who wish to receive messages must
//       register using DmRegisterNotificationProcessor.
//--------------------------------------------------------------------------------------
BOOL DebugConsolePrintf( const CHAR* strFormat, ... )
{
    // Copy command prefix into buffer
    CHAR strBuffer[MAXRCMDLENGTH];
    int length = sprintf_s( strBuffer, "%s!", g_strDebugConsoleCommandPrefix );

    // Format arguments
    va_list arglist;
    va_start( arglist, strFormat );
    vsprintf_s( strBuffer + length, ARRAYSIZE( strBuffer ) - length, strFormat, arglist );
    va_end( arglist );

    // Send it out the string
    DmSendNotificationString( strBuffer );

    return TRUE;
}


//--------------------------------------------------------------------------------------
// Name: GetBinaryDataSize
// Desc: Returns the amount of data currently in the binary receive buffer
//--------------------------------------------------------------------------------------
DWORD GetBinaryDataSize()
{
    return g_dwReceiveBufferDataSize;
}


//--------------------------------------------------------------------------------------
// Name: GetBinaryData
// Desc: Returns a pointer to the binary receive buffer
//--------------------------------------------------------------------------------------
BYTE* GetBinaryData()
{
    return g_pBinaryReceiveBuffer;
}


//--------------------------------------------------------------------------------------
// Name: IsBinaryTransferInProgress
// Desc: Returns TRUE if binary data is currently being received
//--------------------------------------------------------------------------------------
BOOL IsBinaryTransferInProgress()
{
    return g_bBinaryTransferInProgress;
}

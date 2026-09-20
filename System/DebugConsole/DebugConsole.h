//--------------------------------------------------------------------------------------
// DebugConsole.h
//
// Remote Xbox Debug Console header
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#pragma once
#ifndef DEBUGCONSOLE_H
#define DEBUGCONSOLE_H

#include <winsock2.h>
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <stdio.h>
#include <malloc.h>
#include <richedit.h>
#include <assert.h>
#include "xbdm.h"
#include "resource.h"


// The command prefix that is prepended to all communication between the Xbox
// app and the debug console app
#define DEBUGCONSOLE_COMMAND_PREFIX      "XCMD"


//--------------------------------------------------------------------------------------
// Application variables
//--------------------------------------------------------------------------------------
extern HWND             g_hDlgMain;      // Main hwnd
extern HWND             g_hwndOutputWindow;       // Output window


//--------------------------------------------------------------------------------------
// Debug monitor variables
//--------------------------------------------------------------------------------------
extern BOOL             g_bConnected;    // Connected to Xbox?
extern BOOL             g_bECPConnected; // Connected to External Command Processor in app?
extern BOOL             g_bDebugMonitor; // Display debug output?

extern PDMN_SESSION     g_pdmnSession;   // Debug Monitor Session
extern PDM_CONNECTION   g_pdmConnection; // Debug Monitor Connection


//--------------------------------------------------------------------------------------
// Callback handlers for the remote commands
//--------------------------------------------------------------------------------------
typedef BOOL (*RCMDHANDLER )( int argc, char* argv[] );

BOOL RCmdHelp( int argc, char* argv[] );
BOOL RCmdCls( int argc, char* argv[] );
BOOL RCmdConnect( int argc, char* argv[] );
BOOL RCmdDisconnect( int argc, char* argv[] );
BOOL RCmdQuit( int argc, char* argv[] );
BOOL RCmdSendFile( int argc, char* argv[] );
BOOL RCmdGetFile( int argc, char* argv[] );

// Array of remote commands
struct REMOTE_COMMAND
{
    const CHAR* strCommand;
    RCMDHANDLER pfnHandler;
    const CHAR* strHelp;
};

static const REMOTE_COMMAND g_RemoteCommands[] =
{
    // Command name,        Handler,        Help string

    // These are local commands
    { "help",               RCmdHelp,       " [CMD]: List commands / usage" },
    { "cls",                RCmdCls,        ": clear the screen" },
    { "connect",            RCmdConnect,    " [server] [portno]: connect to server @ portno" },
    { "disconnect",         RCmdDisconnect, ": Terminate Debug Console session" },
    { "quit",               RCmdQuit,       ": hasta luego" },

    // These are routed through DmSendFile/ReceiveFile to handle file I/O
    { "sendfile",           RCmdSendFile,   " <localfile> <remotefile>: Sends a file to Xbox" },
    { "getfile",            RCmdGetFile,    " <remotefile> <localfile>: Gets a file from Xbox" },

    // These are all sent directly through to the debug monitor and their output will be displayed
    { "break",              NULL,
        " addr=<address> | \n'Write'/'Read'/'Execute'=<address> size=<DataSize>\n['clear']: Sets/Clears a breakpoint"
    },
    { "bye",                NULL,           " : Closes connection" },
    { "continue",           NULL,
        " thread=<threadid>: resumes execution of a thread which has been stopped" },
    { "delete",             NULL,           " name=<remotefile>: Deletes a file on the Xbox" },
    { "dirlist",            NULL,           " name=<remotedir>: Lists the items in the directory" },
    { "getcontext",         NULL,
        " thread=<threadid> 'Control' | 'Int' | 'FP' | 'Full':  Gets the context of the thread" },
    { "getfileattributes",  NULL,           " name=<remotefile>: Gets attributes of a file" },
    { "getmem",             NULL,           " addr=<address> length=<len> Reads memory from the Xbox" },
    { "go",                 NULL,           " : Resumes suspended title threads" },
    { "halt",               NULL,           " thread=<threadid> Breaks a thread" },
    { "isstopped",          NULL,           " thread=<threadid>: Determines if a thread is stopped and why" },
    { "mkdir",              NULL,           " name=<remotedir>: Creates a new directory on the Xbox" },
    { "modlong",            NULL,           " name=<module>: Lists the long name of the module" },
    { "modsections",        NULL,           " name=<module>: Lists the sections in the module" },
    { "modules",            NULL,           " : Lists currently loaded modules" },
    { "reboot",             NULL,           " [cold] [wait]: Reboots the xbox" },
    { "rename",             NULL,           " name=<remotefile> newname=<newname>: Renames a file on the Xbox" },
    { "resume",             NULL,           " thread=<threadid>: Resumes thread execution" },
    { "setcontext",         NULL,           " thread=<threadid> Sets the context of the thread." },
    { "setfileattributes",  NULL,           " <remotefile> <attrs>: Sets attributes of a file" },
    { "setmem",             NULL,           " addr=<address> data=<rawdata>: Sets memory on the Xbox" },
    { "stop",               NULL,           " : Stops the process" },
    { "suspend",            NULL,           " thread=<threadid> Suspends the thread" },
    { "systime",            NULL,           " : gets the system time of the xbox" },
    { "threadinfo",         NULL,           " thread=<threadid>: Gets thread info" },
    { "threads",            NULL,           " : gets the thread list" },
    { "title",              NULL,
        " dir=<remotedir> name=<remotexbe> [cmdline=<cmdline>]: Sets title to run" },
    { "xbeinfo",            NULL,           " name=<remotexbe | 'running'>: Gets info on an xbe" },
};

static const DWORD      g_dwNumRemoteCommands = sizeof( g_RemoteCommands ) / sizeof( REMOTE_COMMAND );


//--------------------------------------------------------------------------------------
// A buffer to hold messages for printing
//--------------------------------------------------------------------------------------

// Storage constants for the print queue
#define NUM_STRINGS    50
#define MAX_STRING_LEN 512

// A buffer to hold messages for printing
struct PrintQueue
{
    CRITICAL_SECTION CriticalSection;   // Critical section
    DWORD dwNumMessages;     // # of messages

    // Array of strings
    COLORREF    aColors[NUM_STRINGS];  // Text color
    CHAR        astrMessages[NUM_STRINGS][MAX_STRING_LEN];
};

// Print functions
void EnqueueStringForPrinting( COLORREF color, const CHAR* strFormat, ... );
void ProcessEnqueuedStrings();


//--------------------------------------------------------------------------------------
// Helper functions
//--------------------------------------------------------------------------------------
void DisplayError( const CHAR* strResponse, const CHAR* strApiName, HRESULT hr );
int ConsoleWindowPrintf( COLORREF rgb, LPCTSTR lpFmt, ... );
int CmdToArgv( CHAR* str, char* argv[], int maxargs );


#endif // DEBUGCONSOLE_H

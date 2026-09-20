// TriggerProfile.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <string>


//----------------------------------------------------------------------------
// Name: LoadXBDM
// Desc: Adds %xedk%\bin\win32 to the path and loads XBDM.DLL.
//----------------------------------------------------------------------------
bool LoadXBDM()
{
    char* path;
    _dupenv_s( &path, NULL, "path" );

    char* xedkDir;
    _dupenv_s( &xedkDir, NULL, "xedk" );

    // Set the xedkDir value to blank if no %xedk% var exists in the
    // environment.
    if( !xedkDir )
        xedkDir = "";

    std::string newPath( "path=" );

    // "path=%s;%s\\bin\\win32", path, xedkDir);
    newPath += path;
    newPath += ";";
    newPath += xedkDir;
    newPath += "\\bin\\win32";

    // Update the system path with the new value.  This is so our LoadLibrary
    // call can find XBDM.DLL.
    _putenv( newPath.c_str() );

    // Call LoadLibrary on XBDM.DLL.
    HMODULE hXBDM = LoadLibrary( "xbdm.dll" );

    // Print an error message and return zero if XBDM.DLL didn't load.
    if( !hXBDM )
    {
        if( xedkDir[0] )
            printf( "\nERROR:\n\nCouldn't load xbdm.dll.\n" );
        else
            printf( "\nERROR:\n\nCouldn't load xbdm.dll\nXEDK environment variable not set.\n" );
        return false;
    }

    // XBDM.DLL loaded.  Return true for success.
    return true;
}


//-----------------------------------------------------------------------------
// Name: DisplayError()
// Desc: Display friendly error by translating the hr to a message
//-----------------------------------------------------------------------------
VOID DisplayError( const CHAR* strResponse, const CHAR* strApiName, HRESULT hr )
{
    const size_t errorSize = 200;
    CHAR strError[ errorSize ] = "";

    if( hr == XBDM_UNDEFINED )
        lstrcpyn( strError, strResponse, errorSize );
    else if( hr == XBDM_INVALIDCMD )
    {
        lstrcpyn( strError, "Invalid command - command processor not registered?",
                  errorSize );
    }
    else
        DmTranslateError( hr, strError, errorSize );

    if( strError[0] )
        printf( "%s failed: '%s'\n", strApiName, strError );
    else
        printf( "%s failed: 0x%08lx\n", strApiName, hr );
}


//----------------------------------------------------------------------------
// Name: Main
// Desc: Entry point for program
//----------------------------------------------------------------------------
int main( int argc, char* argv[] )
{
    if( argc < 2 )
    {
        printf( "Syntax: XbTrigger command\n" );
        return 0;
    }
    DWORD devkitNameSize = MAX_PATH;
    char devkitName[MAX_PATH];
    HRESULT hr;

    // Attempt to load XBDM.DLL.  The project settings specify to delay load
    // it so as to not break the linker.  So long as we load it manually
    // before we try to use it, all will be well.  Exit main on failure.
    if( !LoadXBDM() )
        return 0;

    // Retrieve the name of the default Xenon devkit.
    hr = DmGetNameOfXbox( devkitName, &devkitNameSize, TRUE );

    // Print results.
    if( FAILED( hr ) )
        printf( "\nERROR:\n\nCould not connect to default Xenon devkit or no devkit name set.\n" );
    else
        printf( "\nConnection succeeded to devkit \"%s\".\n", devkitName );

    // Debug Monitor Connection
    PDM_CONNECTION g_pdmConnection;

    // Open our connection
    hr = DmOpenConnection( &g_pdmConnection );
    if( FAILED( hr ) )
    {
        DisplayError( NULL, "DmOpenConnection", hr );
        return TRUE;
    }

    char strResponse[MAX_PATH];
    strResponse[0] = 0;
    DWORD dwResponseLen = MAX_PATH;

    // Send a profile command. Use the same command prefix as the DebugConsole sample.
    // This will fail if there isn't a command processor registered with the same
    // prefix. The command--the portion after the '!'--will be checked against the
    // string passed to Profiling_CheckCommand on the devkit.
    std::string command( "XCMD!" );

    // Build up a string with all of the command line arguments.
    for( int i = 1; i < argc; ++i )
    {
        command += argv[i];
        if( i < argc - 1 )
            command += " ";
    }

    // Send the full command.
    hr = DmSendCommand( g_pdmConnection, command.c_str(),
                        strResponse, &dwResponseLen );
    if( FAILED( hr ) )
        DisplayError( NULL, "DmSendCommand", hr );
    else
        printf( "Command sent.\nResponse is: '%s'.\n", strResponse );

    DmCloseConnection( g_pdmConnection );

    return 0;
}

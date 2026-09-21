//--------------------------------------------------------------------------------------
// CallStackRecording.cpp
//
// This sample illustrates how to record a call stack of return addresses, or any other
// arbitrary code addresses, along with the necessary information to allow a tool on
// the PC to decode the addresses to symbols. The only functions needed from this sample
// are WriteStackBackTrace, WriteAddresses, and ByteReverse.
//
// Decoding of symbols is demonstrated by the CallStackDisplay sample.
//
// Microsoft Game Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include <xtl.h>
#include <xboxmath.h>
#include <AtgApp.h>
#include <AtgFont.h>
#include <AtgHelp.h>
#include <AtgInput.h>
#include <AtgResource.h>
#include <AtgUtil.h>
#include <xbdm.h>
#include <algorithm>


// Call stack data can either be written in text or in binary. Binary is simpler and
// more efficient, but text is easier to decode.
//#define WRITE_AS_BINARY


// Length of time to leave messages on screen.
#define MESSAGE_DURATION    1.0


//--------------------------------------------------------------------------------------
// Callouts for labelling the gamepad on the help screen
//--------------------------------------------------------------------------------------
ATG::HELP_CALLOUT g_HelpCallouts[] =
{
    { ATG::HELP_BACK_BUTTON, ATG::HELP_PLACEMENT_2, L"Display\nhelp" },
    { ATG::HELP_A_BUTTON, ATG::HELP_PLACEMENT_2, L"Record short\ncallstack" },
    { ATG::HELP_B_BUTTON, ATG::HELP_PLACEMENT_2, L"Record long\ncallstack" },
};
static const DWORD  NUM_HELP_CALLOUTS = _countof( g_HelpCallouts );


//--------------------------------------------------------------------------------------
// Name of the file to save call stack information to.
//--------------------------------------------------------------------------------------
const CHAR*         g_filename = "e:\\callstacks.dat";


//--------------------------------------------------------------------------------------
// Name: ByteReverse
// Desc: Template function to byte-reverse in place any arbitrary block of data.
//--------------------------------------------------------------------------------------
template <typename T> void ByteReverse( T& data )
{
    BYTE* pData = ( BYTE* )&data;
    for( int i = 0; i < sizeof( data ) / 2; ++i )
    {
        std::swap( pData[i], pData[sizeof( data ) - 1 - i] );
    }
}


//--------------------------------------------------------------------------------------
// Name: WriteAddresses()
// Desc: This function writes an array of addresses to the output file. When it is
//       called for the first time it writes out information about the loaded code
//       modules--their addresses, sizes, timestamps, names, and PDB signatures.
//       Then it writes how many addresses there are, and then the addresses.
//       This function has very limited error checking.
//--------------------------------------------------------------------------------------
void WriteAddresses( ULONG numEntries, VOID** pAddresses )
{
    static BOOL s_initialized = FALSE;
    static BOOL s_initTried = FALSE;
    HANDLE hFile = INVALID_HANDLE_VALUE;
    DWORD writeCount = 0;
    if( !s_initialized )
    {
        // If we've already tried to initialize and failed, just exit.
        if( s_initTried )
            return;
        s_initTried = TRUE;

        // Allow writing to e:
        DmMapDevkitDrive();
        // Create the file to store the callstack data.
        hFile = CreateFile( g_filename, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL );

        if( hFile == INVALID_HANDLE_VALUE )
            return; // Failure

#ifdef  WRITE_AS_BINARY
        // Write a marker to identify this as a binary file.
        const char* moduleComment = "BINR";
        WriteFile( hFile, moduleComment, strlen(moduleComment), &writeCount, 0 );
#else
        // Write a comment for the reader.
        const char* moduleComment = "; List of loaded modules\r\n";
        WriteFile( hFile, moduleComment, strlen( moduleComment ), &writeCount, 0 );
#endif

        // Walk the list of loaded code modules.
        HRESULT error;
        PDM_WALK_MODULES pWalkMod = NULL;
        DMN_MODLOAD modLoad;

        while( XBDM_NOERR == ( error = DmWalkLoadedModules( &pWalkMod, &modLoad ) ) )
        {
            // Find the signature that describes the PDB file of the current module.
            DM_PDB_SIGNATURE signature = {0};
            DmFindPdbSignature( modLoad.BaseAddress, &signature );

#ifdef  WRITE_AS_BINARY
            // Each module is prefixed by a magic number, and the list of modules is
            // terminated with a DWORD zero.
            DWORD modulePrefixID = 0xABABCDCD;
            WriteFile( hFile, &modulePrefixID, sizeof(modulePrefixID), &writeCount, 0 );

            // Write the data that describes this module.
            WriteFile( hFile, &modLoad.BaseAddress, sizeof(modLoad.BaseAddress), &writeCount, 0 );
            WriteFile( hFile, &modLoad.Size, sizeof(modLoad.Size), &writeCount, 0 );
            WriteFile( hFile, &modLoad.TimeStamp, sizeof(modLoad.TimeStamp), &writeCount, 0 );
            WriteFile( hFile, &modLoad.Name, sizeof(modLoad.Name), &writeCount, 0 );
            WriteFile( hFile, &signature, sizeof(signature), &writeCount, 0 );
#else
            // Write the information about the current module as three lines of text.
            // The first line is the module file name, the second line is the full path to
            // the PDB file, and the third line is the load address, size, timestamp, GUID,
            // and PDB age.
            char buffer[1000];

            // Write the module name on its own line.
            sprintf_s( buffer, "%s\r\n", modLoad.Name );
            WriteFile( hFile, buffer, strlen( buffer ), &writeCount, 0 );

            // Write the pdb path on its own line.
            sprintf_s( buffer, "%s\r\n", signature.Path );
            WriteFile( hFile, buffer, strlen( buffer ), &writeCount, 0 );

            // Write the module address, size, and timestamp.
            sprintf_s( buffer, "%p, %08X, %08X, ", modLoad.BaseAddress, modLoad.Size,
                       modLoad.TimeStamp );
            WriteFile( hFile, buffer, strlen( buffer ), &writeCount, 0 );

            // ByteReverse the necessary elements of the GUID so it prints properly.
            ByteReverse( signature.Guid.Data1 );
            ByteReverse( signature.Guid.Data2 );
            ByteReverse( signature.Guid.Data3 );
            ByteReverse( signature.Age );
            // Print out the GUID and age in the standard format (as used by dumpbin /headers)
            // except with spaces between the bytes of Guid.Data4, to make for easier parsing.
            sprintf_s( buffer, "{%08X-%04X-%04X-%02X %02X-%02X %02X %02X %02X %02X %02X}, %d\r\n",
                       signature.Guid.Data1, signature.Guid.Data2, signature.Guid.Data3,
                       signature.Guid.Data4[0], signature.Guid.Data4[1],
                       signature.Guid.Data4[2], signature.Guid.Data4[3],
                       signature.Guid.Data4[4], signature.Guid.Data4[5],
                       signature.Guid.Data4[6], signature.Guid.Data4[7],
                       signature.Age );
            WriteFile( hFile, buffer, strlen( buffer ), &writeCount, 0 );
#endif
        }

        if( error != XBDM_ENDOFLIST )
        {
            // Handle errors here...
        }
        DmCloseLoadedModules( pWalkMod );

#ifdef  WRITE_AS_BINARY
        // Mark the end of the module list.
        DWORD endId = 0;
        WriteFile( hFile, &endId, sizeof(endId), &writeCount, 0 );
#else
        const char* moduleEndMarker = "ModuleEnd\r\n\r\n; CallStacks:\r\n";
        WriteFile( hFile, moduleEndMarker, strlen( moduleEndMarker ), &writeCount, 0 );
#endif

        // We have successfully initialized our stack tracing system.
        s_initialized = true;
    }
    else
    {
        // We have already initialized the tracing system.
        // Open the existing file and append to it.
        hFile = CreateFile( g_filename, GENERIC_WRITE, 0, NULL,
                            OPEN_EXISTING, 0, NULL );
        if( hFile == INVALID_HANDLE_VALUE )
            return; // Failure

        // Move to the end of the file for adding more data.
        SetFilePointer( hFile, 0, 0, FILE_END );
    }

#ifdef  WRITE_AS_BINARY
    // Write the count of how many addresses there are.
    WriteFile( hFile, &numEntries, sizeof(numEntries), &writeCount, 0 );
    // Write the array of addresses.
    WriteFile( hFile, pAddresses, sizeof(pAddresses[0]) * numEntries, &writeCount, 0 );
#else
    char buffer[100];
    sprintf_s( buffer, "%u entries:\r\n", numEntries );
    WriteFile( hFile, buffer, strlen( buffer ), &writeCount, 0 );
    for( ULONG i = 0; i < numEntries; ++i )
    {
        sprintf_s( buffer, "\t%p\r\n", pAddresses[ i ] );
        WriteFile( hFile, buffer, strlen( buffer ), &writeCount, 0 );
    }
#endif
    CloseHandle( hFile );
}


//--------------------------------------------------------------------------------------
// Name: WriteStackBackTrace()
// Desc: This function writes the current call stack to the output file.
//--------------------------------------------------------------------------------------
void WriteStackBackTrace()
{
    const DWORD maxBackTrace = 20;
    VOID* backTrace[maxBackTrace];

    // Capture a stack back trace.
    HRESULT hResult = DmCaptureStackBackTrace( _countof( backTrace ), backTrace );

    if( hResult == XBDM_NOERR )
    {
        // Find out how many valid entries are in the result.
        ULONG count = 0;
        for( ULONG i = 0; i < _countof( backTrace ) && backTrace[i]; ++i )
            count = i;

        // Write the valid entries to the output file.
        WriteAddresses( count, backTrace );
    }
}


//--------------------------------------------------------------------------------------
// Name: class Sample
// Desc: Main class to run this application. Most functionality is inherited
//       from the ATG::Application base class.
//--------------------------------------------------------------------------------------
class Sample : public ATG::Application
{
    ATG::Timer m_Timer;
    ATG::Font m_Font;
    ATG::PackedResource m_Resource;
    ATG::Help m_Help;
    BOOL m_bDrawHelp;
    DOUBLE m_messageEndTime;
    WCHAR* m_message;

private:

    virtual HRESULT Initialize();
    virtual HRESULT Update();
    virtual HRESULT Render();
};


//--------------------------------------------------------------------------------------
// Name: main()
// Desc: Entry point to the program
//--------------------------------------------------------------------------------------
INT __cdecl main()
{
    Sample atgApp;
    ATG::GetVideoSettings( &atgApp.m_d3dpp.BackBufferWidth, &atgApp.m_d3dpp.BackBufferHeight );
    atgApp.Run();
}


//--------------------------------------------------------------------------------------
// Name: Initialize()
// Desc: This creates all device-dependent display objects.
//--------------------------------------------------------------------------------------
HRESULT Sample::Initialize()
{
    m_bDrawHelp = FALSE;
    m_message = 0;

    // Create the font
    if( FAILED( m_Font.Create( "game:\\Media\\Fonts\\Arial_16.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Confine text drawing to the title safe area
    m_Font.SetWindow( ATG::GetTitleSafeArea() );

    // Create the help
    if( FAILED( m_Help.Create( "game:\\Media\\Help\\Help.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: FunctionB()
// Desc: Non-inline function that records a call stack.
//       __declspec(noinline) tells the compiler not to inline this function.
//--------------------------------------------------------------------------------------
void __declspec( noinline ) FunctionB()
{
    WriteStackBackTrace();
    // Put in a function call here or else the compiler will optimize the function
    // call to WriteStackBackTrace to just a branch, and it will leave no record
    // on the call stack.
    printf( "In " __FUNCTION__ "\n" );
}


//--------------------------------------------------------------------------------------
// Name: FunctionA()
// Desc: Non-inline function used to create a longer call stack, for test purposes.
//       __declspec(noinline) tells the compiler not to inline this function.
//--------------------------------------------------------------------------------------
void __declspec( noinline ) FunctionA()
{
    FunctionB();
    // Put in a function call here or else the compiler will optimize the function
    // call to WriteStackBackTrace to just a branch, and it will leave no record
    // on the call stack.
    printf( "In " __FUNCTION__ "\n" );
}


//--------------------------------------------------------------------------------------
// Name: Update()
// Desc: Called once per frame, the call is the entry point for animating the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Update()
{
    // Get the current gamepad state
    ATG::GAMEPAD* pGamepad = ATG::Input::GetMergedInput();

    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_A )
    {
        // Write a call stack from the current location, a fairly short call stack.
        WriteStackBackTrace();
        m_message = L"Short callstack recorded.";
        m_messageEndTime = m_Timer.GetAbsoluteTime() + MESSAGE_DURATION;
    }

    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_B )
    {
        // Call a few functions before recording a call stack, to get a longer
        // call stack.
        FunctionA();
        m_message = L"Long callstack recorded.";
        m_messageEndTime = m_Timer.GetAbsoluteTime() + MESSAGE_DURATION;
    }

    // Toggle help
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_BACK )
        m_bDrawHelp = !m_bDrawHelp;

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Render()
// Desc: Sets up render states, clears the viewport, and renders the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Render()
{
    // Draw a gradient filled background
    ATG::RenderBackground( 0xff0000ff, 0xff000000 );

    // Show title, frame rate, and help
    m_Timer.MarkFrame();
    if( m_bDrawHelp )
    {
        m_Help.Render( &m_Font, g_HelpCallouts, NUM_HELP_CALLOUTS );
    }
    else
    {
        m_Font.Begin();
        m_Font.SetScaleFactors( 1.2f, 1.2f );
        m_Font.DrawText( 0, 0, 0xffffffff, L"CallStackRecording" );
        m_Font.SetScaleFactors( 1.0f, 1.0f );
        m_Font.DrawText( 0, 0, 0xffffff00, m_Timer.GetFrameRate(), ATGFONT_RIGHT );

        if( m_message )
        {
            m_Font.DrawText( 0, 30, 0xffffff00, m_message );
            if( m_Timer.GetAbsoluteTime() > m_messageEndTime )
                m_message = 0;
        }
        m_Font.End();
    }

    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}

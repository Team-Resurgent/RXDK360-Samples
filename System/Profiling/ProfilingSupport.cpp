//-----------------------------------------------------------------------------
// File: ProfilingSupport.cpp
//
// Desc: Helper classes and functions to support easier profiling, especially
// triggering of profiles from your development machine.
//
// Hist: 06.16.05 - New for July 2005 XDK release
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#include <xtl.h>
#include "ProfilingSupport.h"
#include <xbdm.h>
#include <tracerecording.h>

// Link in the trace recording library.
#pragma comment(lib, "tracerecording.lib")

// The command prefix is compatible with the DebugConsole sample
// so we can use DebugConsole to trigger profiles as well as using
// the TriggerProfile tool.
static const CHAR*  s_CommandPrefix = "XCMD";

// Static global buffer to record received commands. This is not a
// robust way to receive many messages, but it suffices for manually
// sent commands.
static CHAR s_receivedCommand[100];
// Static int to record an optional integer parameter. This is commonly
// used with the performance counters to specify which profiling set to use.
static int          s_receivedNumber;


//-----------------------------------------------------------------------------
// Name: dbgtolower()
// Desc: Returns lowercase of ch. Used by dbgstrnicmp
// This is necessary because most CRT functions cannot safely be used from
// DPC (Deferred Procedure Call) functions.
//-----------------------------------------------------------------------------
static inline CHAR dbgtolower( CHAR ch )
{
    if( ch >= 'A' && ch <= 'Z' )
        return ch - ( 'A' - 'a' );
    else
        return ch;
}


//-----------------------------------------------------------------------------
// Name: dbgstrnicmp()
// Desc: Case-insensitive string compare that avoids potential deadlocks from
// the CRT strnicmp function.
// Note that this function returns TRUE if the strings match, which is the
// reverse of the CRT strnicmp function.
// This is necessary because most CRT functions cannot safely be used from
// DPC (Deferred Procedure Call) functions.
//-----------------------------------------------------------------------------
static BOOL dbgstrnicmp( const CHAR* str1, const CHAR* str2, int n )
{
    while( ( dbgtolower( *str1 ) == dbgtolower( *str2 ) ) && *str1 && n > 0 )
    {
        --n;
        ++str1;
        ++str2;
    }

    return( n == 0 || dbgtolower( *str1 ) == dbgtolower( *str2 ) );
}


//-----------------------------------------------------------------------------
// Name: ProfileCommandHandler()
// Desc: Command handler to hook in to the Debug Manager command system.
// This function is called from the Debug Manager DPC (Deferred Procedure Call)
// which means there are many CRT and XTL functions that cannot be used.
// It also means you cannot set breakpoints in this function using VC++.
// This function just looks for commands and copies them to s_receivedCommand.
//-----------------------------------------------------------------------------
static HRESULT __stdcall ProfileCommandHandler( const CHAR* strCommand,
                                                CHAR* strResponse, DWORD dwResponseLen,
                                                PDM_CMDCONT pdmcc )
{
    // Skip over the command prefix and the exclamation mark
    strCommand += strlen( s_CommandPrefix ) + 1;

    // Check if this is the initial connect signal
    if( dbgstrnicmp( strCommand, "__connect__", 11 ) )
    {
        // If so, respond that we're connected
        lstrcpynA( strResponse, "RemoteTrigger connected.", dwResponseLen );
        return XBDM_NOERR;
    }

    // Find the first NULL or space, without using CRT functions.
    const CHAR* endOfCommand = strCommand;
    while( *endOfCommand && *endOfCommand != ' ' )
        ++endOfCommand;

    // Copy the command into s_receivedCommand, stopping at the first space.
    DWORD commandLength = endOfCommand - strCommand;
    if( commandLength < ARRAYSIZE( s_receivedCommand ) )
    {
        // lstrcpynA always null terminates the destination.
        lstrcpynA( s_receivedCommand, strCommand, commandLength + 1 );
    }
    else
    {
        // lstrcpynA always null terminates the destination.
        lstrcpynA( s_receivedCommand, strCommand, ARRAYSIZE( s_receivedCommand ) );
    }

    // Skip over any remaining spaces.
    while( *endOfCommand == ' ' )
        ++endOfCommand;

    // Now parse out the optional numeric parameter. In an ideal world we would
    // just call scanf, but that isn't legal from a DPC.
    bool negative = false;
    if( *endOfCommand == '-' )
    {
        ++endOfCommand;
        negative = true;
    }

    int receivedNumber = 0;
    while( *endOfCommand >= '0' && *endOfCommand <= '9' )
    {
        receivedNumber = receivedNumber * 10 + *endOfCommand - '0';
        ++endOfCommand;
    }

    if( negative )
        receivedNumber = -receivedNumber;

    s_receivedNumber = receivedNumber;

    const CHAR* response = "Command received.";

    // Copy the response to the output buffer.
    OutputDebugString( response );
    OutputDebugString( "\n" );
    lstrcpynA( strResponse, response, dwResponseLen );

    return XBDM_NOERR;
}


//-----------------------------------------------------------------------------
// Name: Profiling_CheckCommand
// Desc: Returns TRUE if the specified command was requested, and resets the
// internal variable so it will return FALSE next time.
//-----------------------------------------------------------------------------
BOOL Profiling_CheckCommand( const CHAR* commandName )
{
    // Initialize the command processor if this hasn't been done yet.
    static BOOL s_Initialized = FALSE;
    if( !s_Initialized )
    {
        s_Initialized = TRUE;
        HRESULT hr = DmRegisterCommandProcessor( s_CommandPrefix, ProfileCommandHandler );
        if( FAILED( hr ) )
            OutputDebugString( "Error registering command processor.\n" );
    }

    BOOL Result = FALSE;
    if( s_receivedCommand[0] )
    {
        // Check and see if we have a match.
        Result = stricmp( commandName, s_receivedCommand ) == 0;
        // If we have a match, clear the string. Otherwise leave it for
        // a future call.
        if( Result )
            s_receivedCommand[ 0 ] = 0;
    }

    return Result;
}


//-----------------------------------------------------------------------------
// Name: Profiling_GetInt()
// Desc: After Profiling_CheckCommand() returns true this function will return
// the optional integer parameter specified after the command, or zero.
// 
//-----------------------------------------------------------------------------
int Profiling_GetInt()
{
    return s_receivedNumber;
}


//-----------------------------------------------------------------------------
// Name: BeginCapture()
// Desc: Start trace recording.
//     Call EndCapture() or let the destructor call it at the end of the
// frame.
//-----------------------------------------------------------------------------
void ProfileRecording::BeginCapture( const CHAR* traceFilename )
{
    XTraceStartRecording( traceFilename );
    m_recordingTrace = true;
}


//-----------------------------------------------------------------------------
// Name: EndCapture()
// Desc: End trace recording. Then you can manually copy the resulting
// .pix2 file to your PC and use tracedump, together with the .pe and .pdb
// files, to analyze your performance.
//-----------------------------------------------------------------------------
void ProfileRecording::EndCapture()
{
    // If we weren't doing recording, just exit.
    if( !m_recordingTrace )
        return;

    XTraceStopRecording();
    m_recordingTrace = false;
}


//-----------------------------------------------------------------------------
// Name: BeginPMC()
// Desc: Start a PMC recording
//     You can stop the PMC recording by calling EndPMC, or let
// the destructor call it at the end of the frame.
//-----------------------------------------------------------------------------
void ProfileRecording::BeginPMC( int whichSetup )
{
    // Check for invalid parameters.
    if( whichSetup < 0 || whichSetup >= PMC_SETUP_LAST )
    {
        printf( "Illegal PMC setup %d.\n", whichSetup );
        return;
    }

    printf( "Triggering PMC recording for setup %d.\n", whichSetup );
    // Set up the Performance Monitor Counters using one of the standard setups.
    // There are many to choose from. If the setup is changed then
    // GetCounterName() will no longer be correct.
    DmPMCInstallAndStart( ( ePMCSetup )whichSetup );
    m_lastSetup = ( ePMCSetup )whichSetup;

    m_recordingPMC = true;
}


//-----------------------------------------------------------------------------
// Name: CustomDumpCounters()
// Desc: Custom code to print the counters in the standard PMC format. This
//       code can be modified to adjust the format or to display it on-screen.
//       The results will be the same as PMCDumpCountersVerbose.
//-----------------------------------------------------------------------------
void ProfileRecording::CustomDumpCounters( const PMCState& pmcstate )
{
    // Create a table to map PMC sources to strings.
    static const char* sourceNames[PMC_count] =
    {
        "PB0T0",
        "PB0T1",
        "PB1T0",
        "PB1T1",
        "PB2T0",
        "PB2T1",
        "PMW",
        "Unknown",
    };

    // Print out the same header information printed by PMCDumpCountersVerbose
    const int PerfCounterFrequency = 50000000;  // The actual frequency is slightly lower than this.
    printf( "\n\nCPU ticks=|%I64d| interval=|%.0f| frequency=|%.1f|\n",
            pmcstate.ticks,
            pmcstate.interval,
            ( double )pmcstate.interval / ( ( double )pmcstate.ticks / ( double )PerfCounterFrequency )
            );

    // Calculate the recording interval in seconds.
    double fTime = ( double )( pmcstate.ticks ) / ( ( double )PerfCounterFrequency );
    printf( "time=|%.3f| ", fTime );
    printf( "FPS=|%7.2f|", 1.0 / fTime );
    printf( " using setup %2d", m_lastSetup );
    printf( "%11s( /cycle)[ /inst ]\r\n", " " );

    // Loop through all of the counters and print them in PMCDumpCountersVerbose format
    for( int i = 0; i < _countof( pmcstate.pmc ); ++i )
    {
        // perInstructionRate is either IPC, events per instruction, or
        // blank, depending on the counter and the setup.
        char perInstructionRate[100];
        const char* counterName = DmPMCGetCounterName( i );
        if( strcmp( counterName, "instructions committed" ) == 0 )
        {
            // For the instructions-committed counter calculate instructions per cycle
            sprintf_s( perInstructionRate, " IPC=|%5.3f|", pmcstate.pmc[i] / pmcstate.interval );
        }
        else if( pmcstate.instrCount != 0 )
        {
            // If we have an instruction count calculate events per instruction
            // If the counter set records instruction counts for multiple threads
            // then this is not particularly meaningful. A better implementation
            // would correlate counters to the appropriate thread.
            sprintf_s( perInstructionRate, "[%6.2f%%]", ( pmcstate.pmc[i] * 100.0 ) / pmcstate.instrCount );

        }
        else
            perInstructionRate[0] = 0;

        // Print a line that looks something like this:
        // [ 1]PB0T0(          i-cache miss cycles)=    143309 (  2.14%)[ 66.58%]
        printf( "[%2d]%-5s(%29s)=%10I64u (%6.2f%%)%s\r\n",
                i,
                sourceNames[DmPMCGetCounterSource( i )],
                counterName,
                pmcstate.pmc[i],    // Raw counter value.
                ( pmcstate.pmc[i] * 100.0 * DmPMCGetCounterCostEstimate( i ) ) / pmcstate.interval,
                perInstructionRate );
    }
}


//-----------------------------------------------------------------------------
// Name: EndPMC()
// Desc: End PMC recording. The results will be printed to the debug output.
//-----------------------------------------------------------------------------
void ProfileRecording::EndPMC()
{
    if( !m_recordingPMC )
        return;

    // The simplest way to stop and print the performance counters is to use
    // PMCStopAndReport. This shows to more flexible techniques.

    // Stop the Performance Monitor Counters, then record information about them.
    DmPMCStop();

    // PMCState holds the state information and recorded timer data for
    // the Performance Monitor Counters.
    PMCState pmcstate;

    // Get the counters.
    DmPMCGetCounters( &pmcstate );

    // Print out detailed information about all 16 Performance Monitor
    // Counters. This detailed analysis puts the counters in a more friendly
    // format, adjusts the values as needed, properly labels the counters, and
    // shows relationships between the counters and the number of cycles spent
    // and the number of instructions executed.
    //PMCDumpCountersVerbose( &pmcstate, PMC_VERBOSE_NOL2ECC );

    // Alternately we can print out the counter information ourselves. This
    // is most convenient if you want custom counters overlayed on your game.
    // This helper function prints out the same information as
    // PMCDumpCountersVerbose
    CustomDumpCounters( pmcstate );

    m_recordingPMC = false;
}

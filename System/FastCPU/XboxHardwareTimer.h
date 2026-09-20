//--------------------------------------------------------------------------------------
// File: XboxHardwareTimer.h
//
// Desc: Contains a class for doing performance measurements of the Xbox 360 CPU.
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#ifndef XBOX_HARDWARE_TIMER_H
#define XBOX_HARDWARE_TIMER_H

#include <assert.h>
#include <xtl.h>
#include <xbdm.h>

// Number of counters recorded.
const DWORD NumPerfCounters = 4;


//--------------------------------------------------------------------------------------
// Name: XboxEventSample
// Desc: An event sampling
//--------------------------------------------------------------------------------------
struct XboxEventSample
{
    // The name and ID identify a time range to be recorded.
    const char* strName;
    DWORD ID;
    DWORD Counters[NumPerfCounters];
};


//--------------------------------------------------------------------------------------
// Maximum number of Event Samplings and reports
//--------------------------------------------------------------------------------------
#define MAX_EVENT_SAMPLES 200
#define MAX_REPORTS (MAX_EVENT_SAMPLES/2)


//--------------------------------------------------------------------------------------
// Name: XboxHardwareTimer
// Desc: A class that handles recording and summarizing P3 timing events
//       A matching StartTimer/StopTimer pair - name and ID must match - define a
//       a range to be measured. When StopTiming() is called a series of reports are
//       generated.
//       Timers can be nested or can overlap in the time they are recording.
//       An XboxHardwareTimer object can only be used by one thread.
//--------------------------------------------------------------------------------------
class XboxHardwareTimer
{
public:
    VOID* operator new( size_t size );
    VOID            operator delete( VOID* ptr );

    // constructor
                    XboxHardwareTimer();

    // start and stop timing
    VOID            StartTiming();
    VOID            StopTiming();

    // start and stop a timer
    VOID            StartTimer( const CHAR* strName, DWORD ID = 0 );
    VOID            StopTimer( const CHAR* strName, DWORD ID = 0, bool verbose = false );

    // get the name of a specific counter
    const char* GetCounterName( UINT uiEvent ) const;

    // get report information
    UINT            GetNumReports() const;
    const XboxEventSample* GetReport( UINT uiReport ) const;

private:
    XboxEventSample m_EventSamples[MAX_EVENT_SAMPLES];
    UINT m_uiEventSampleIndex;
    XboxEventSample m_Reports[MAX_REPORTS];
    UINT m_uiReportIndex;
    DWORD           m_CalibrationCounters[NumPerfCounters];

    // build reports
    VOID            MakeReports();

    // adds an event sampling
    VOID            AddSample( const CHAR* strName, DWORD ID );

    // PMCState holds the state information and recorded timer data for
    // the Performance Monitor Counters.
    PMCState m_pmcstate;
};


//--------------------------------------------------------------------------------------
// inlines
//--------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------
// Name: StartTimer
// Desc: Starts timing a named section
//--------------------------------------------------------------------------------------
VOID __forceinline XboxHardwareTimer::StartTimer( const CHAR* strName, DWORD ID /* = 0*/ )
{
    // Reset the Performance Monitor Counters in preparation for a new sampling run.
    DmPMCResetCounters();
    AddSample( strName, ID );

    // Start up the Performance Monitor Counters.
    DmPMCStart();
}


//--------------------------------------------------------------------------------------
// Name: StopTimer
// Desc: Stops timing a named section - the name must match a previous call to
//       StartTimer
//--------------------------------------------------------------------------------------
VOID __forceinline XboxHardwareTimer::StopTimer( const CHAR* strName, DWORD ID/* = 0*/,
                                                 bool verbose /* = false*/ )
{
    // Stop the Performance Monitor Counters, then record information about them.
    DmPMCStop();
    AddSample( strName, ID );

    // Optionally print out detailed information about all 16 Performance Monitor
    // Counters. This detailed analysis puts the counters in a more friendly
    // format, adjusts the values as needed, properly labels the counters, and
    // shows relationships between the counters and the number of cycles spent
    // and the number of instructions executed.
    if( verbose )
        DmPMCDumpCountersVerbose( &m_pmcstate, PMC_VERBOSE_NOL2ECC );
}


//--------------------------------------------------------------------------------------
// Name: AddSample
// Desc: Adds a timing sample to the array of timing samples
//--------------------------------------------------------------------------------------
VOID __forceinline XboxHardwareTimer::AddSample( const CHAR* strName, DWORD ID )
{
    assert( m_uiEventSampleIndex < MAX_EVENT_SAMPLES );
    XboxEventSample* pCurrentSample = m_EventSamples + m_uiEventSampleIndex;
    pCurrentSample->strName = strName;
    pCurrentSample->ID = ID;

    // Get the four counters.
    DmPMCGetCounters( &m_pmcstate );

    // Get the cycle counter.
    pCurrentSample->Counters[ 0 ] = ( DWORD )m_pmcstate.interval;

    // These counter indices only work with the PMC_SETUP_OVERVIEW_PB0T0
    // counter setup.
    pCurrentSample->Counters[ 1 ] = ( DWORD )m_pmcstate.pmc[ 0 ];     // L1 miss cycles
    pCurrentSample->Counters[ 2 ] = ( DWORD )m_pmcstate.pmc[ 14 ];    // L2 misses
    pCurrentSample->Counters[ 3 ] = ( DWORD )m_pmcstate.pmc[ 4 ];     // Instructions

    ++m_uiEventSampleIndex;
}

#endif

//--------------------------------------------------------------------------------------
// ThreadWatchOnConsole.cpp
//
// The sample implements a watchdog mechanism that monitors the activity of a thread of
// interest. A watchdog thread part of the application checks periodically to see if the
// monitored thread spent any time running. A lack of activity can signify a possible
// frozen thread.
//
// Microsoft Game Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include <xtl.h>
#include <xbdm.h>

#include <assert.h>
#include <stdio.h>

#include "AtgConsole.h"
#include "AtgUtil.h"
#include "AtgInput.h"

//--------------------------------------------------------------------------------------
// ATG Framework related globals
//--------------------------------------------------------------------------------------
ATG::Console    g_console;            // console for output
//--------------------------------------------------------------------------------------
// Name: DetectRebootKeypress()
// Desc: Waits for the 'LT + RT + RB' controller combination
//--------------------------------------------------------------------------------------
void DetectRebootKeypress()
{
    g_console.Format( "Press LT + RT + RB to exit\n" );
    for( ;; )
    {
        ATG::Input::GetMergedInput(); // Detect reboot keypress
    }
}

//--------------------------------------------------------------------------------------
// Sample related globals
//--------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------
// Number of sample threads; only one of them will be monitored below
//--------------------------------------------------------------------------------------
const DWORD NUM_THREADS = 2;

//--------------------------------------------------------------------------------------
// Prototype for a multithread-safe function to print to the console screen (see
// implementation at the end of the source file)
//--------------------------------------------------------------------------------------
VOID __cdecl Print( const WCHAR* strFormat, ... );
//--------------------------------------------------------------------------------------
// Critical section object providing exclusive thread access to the Print() function
//--------------------------------------------------------------------------------------
CRITICAL_SECTION g_CsPrint;

//--------------------------------------------------------------------------------------
// Shared data access by multiple threads
//--------------------------------------------------------------------------------------
DWORD g_SharedDataA;
DWORD g_SharedDataB;
//--------------------------------------------------------------------------------------
// Critical section objects providing exclusive thread access to 
// the g_SharedDataA,...B variables above
//--------------------------------------------------------------------------------------
CRITICAL_SECTION g_CsSharedDataA;
CRITICAL_SECTION g_CsSharedDataB;

//--------------------------------------------------------------------------------------
// Handles for the running threads
//--------------------------------------------------------------------------------------
volatile HANDLE g_hThread1 = 0;
volatile HANDLE g_hThread2 = 0;

//--------------------------------------------------------------------------------------
// Array holding the thread handles (convenient to use in loops, but also used in
// a WaitForMultipleObjects call in main(), waiting for the threads to finish)
//--------------------------------------------------------------------------------------
HANDLE g_hThreads[ NUM_THREADS ] = { 0 };
//--------------------------------------------------------------------------------------
// Array holding the thread IDs (used for reporting/display output)
//--------------------------------------------------------------------------------------
DWORD g_tidThreads[ NUM_THREADS ] = { 0 };


//--------------------------------------------------------------------------------------
// the watchdog thread related code starts here.
// Note: Thread1 (with the handle 'g_hThread1') is being monitored in this sample code
// below.
// Aside from this outside dependency on the thread handle variable, the '#ifdef
// WDTHREAD' block below can be pasted as a whole in your application; of course, don't
// forget to add the other two related initialization/cleanup WDTHREAD conditional
// blocks inside the main function
//--------------------------------------------------------------------------------------
#define WDTHREAD
#ifdef WDTHREAD

//--------------------------------------------------------------------------------------
// store the HRT ("High Resolution Timer") tick period here 
//--------------------------------------------------------------------------------------
float g_fSecsPerTick = 0.0f;

//--------------------------------------------------------------------------------------
// the handle and the thread ID for the watchdog thread
//--------------------------------------------------------------------------------------
HANDLE g_WDThread = 0;
DWORD g_tidWDThread = 0;

//--------------------------------------------------------------------------------------
// the number of milliseconds between the thread state checks
//--------------------------------------------------------------------------------------
LONG g_lTimerTick = 73;

//--------------------------------------------------------------------------------------
// Name: ThreadKernelTimeInfo
// Desc: structure recording the thread activity; it holds the last kernel time
// retrieved (number of 100s of nanoseconds, as in the FILETIME structure) and
// the last system time when activity was detected, in High Resolution Timer
// ticks (HRT ticks)
//--------------------------------------------------------------------------------------
typedef struct ThreadKernelTimeInfo_s
{    
    ULARGE_INTEGER uliLastKernelTime;
    LARGE_INTEGER liLastActivity;
} TimeInfo;

TimeInfo g_ThreadKernelTimeInfo = { 0 };

//--------------------------------------------------------------------------------------
// the default amount of time (in seconds) of inactivity after which
// the thread will be considered frozen
//--------------------------------------------------------------------------------------
#define SECONDS_INACTIVE 1.0
//--------------------------------------------------------------------------------------
// if the thread doesn't execute for this long it will be considered "frozen" (these
// values can be set from other sources than the define above, e.g.  command-line or
// even varied dynamically depending on the needs of the application at a given moment)
//--------------------------------------------------------------------------------------
float g_ThreadMaxInactivity = SECONDS_INACTIVE; // in seconds
LARGE_INTEGER g_ThreadMaxInactivityInTicks;     // in HRT ticks

//--------------------------------------------------------------------------------------
// Name: InitThreadKernelTimeInfo()
// Desc: Initializes the TimeInfo structure
//--------------------------------------------------------------------------------------
void InitThreadKernelTimeInfo()
{    
    // Zero out the last activity timestamp
    g_ThreadKernelTimeInfo.uliLastKernelTime.QuadPart = 0; // in 100s of ns
    g_ThreadKernelTimeInfo.liLastActivity.QuadPart = 0;    // in HRT ticks
}

//--------------------------------------------------------------------------------------
// Name: InitMaxInactivityTicks()
// Desc: Initializes the inactivity-related duration variables
//--------------------------------------------------------------------------------------
void InitMaxInactivityTicks( float seconds )
{
    g_ThreadMaxInactivity = seconds;
    g_ThreadMaxInactivityInTicks.QuadPart = (LONGLONG)(seconds / g_fSecsPerTick) ;
}

//--------------------------------------------------------------------------------------
// Name: 
// Desc: helper function to get the kernel time
//--------------------------------------------------------------------------------------
BOOL GetKernelTimeForThread(HANDLE hThread, LPFILETIME pKernelTime)
{
    FILETIME ftCreation;
    FILETIME ftExit;
    FILETIME ftKernel;
    FILETIME ftUser;
    BOOL bResult = GetThreadTimes( hThread,&ftCreation,&ftExit,&ftKernel,&ftUser );
    if( bResult )
    {
        *pKernelTime=ftKernel;
    }
    return bResult;
}
#pragma warning( push )
// disable 'warning C4702: unreacheable code' raised after the end of the infinite
// loop inside the WDThreadProc function below, since as it is currently implemented
// now indeed it cannot be reached; however, that code could be reached in case the
// handling of the thread inactive situation changes and allows the control to break
// out of the loop
#pragma warning( disable : 4702 )
//--------------------------------------------------------------------------------------
// Name: WDThreadProc()
// Desc: Watchdog thread main function
//--------------------------------------------------------------------------------------
DWORD WINAPI WDThreadProc( LPVOID lpParameter )
{
    LARGE_INTEGER duetime;
    duetime.QuadPart = (__int64)-10*1000L*1000L; // 10 sec. ; not used actually

    HANDLE hTimer = CreateWaitableTimer( NULL, FALSE, "WatchDogTimer" );
    if( hTimer )
    {
        SetWaitableTimer( hTimer, &duetime, g_lTimerTick, NULL, NULL, TRUE );
    }
    else
    {
        Print( L"*** Error creating watchdog timer.\n" );
        return 0;
    }

    for ( ;; )
    {
        WaitForSingleObject( hTimer, INFINITE );

        if( g_hThread1 )
        {
            BOOL bResult;
            // get new kernel time for Thread1
            FILETIME newKernelTime; // = { 0 };
            bResult = GetKernelTimeForThread( g_hThread1,&newKernelTime );
            assert( bResult != 0 );
            // check if the thread is still alive
            DWORD exitcode; // = 0;
            bResult = GetExitCodeThread( g_hThread1, &exitcode );
            assert( bResult != 0 );

            // Note1: if the thread didn't get a chance to accumulate significant kernel
            // time and the newKernelTime value will be 0 (this could also happen at the
            // beginning of the thread, if it ran for a too short time) or if already
            // finished, simply ignore this check [ see also Note2 in the Thread1's main
            // function ]
            if( ( newKernelTime.dwLowDateTime==0 && newKernelTime.dwHighDateTime==0 )
                    || exitcode!=STILL_ACTIVE )
            {
                continue;
            }
            // move it into a ULARGE_INTEGER, as the documentation advises
            ULARGE_INTEGER uli=	*(ULARGE_INTEGER*)&(newKernelTime);

            // get current time ( in HRT ticks )
            LARGE_INTEGER currentTicks;
            QueryPerformanceCounter( &currentTicks );

            // check if there was any change in the kernel time spent
            if ( uli.QuadPart > g_ThreadKernelTimeInfo.uliLastKernelTime.QuadPart )
            {
                // there was a change: update both the last kernel time and the timestamp
                g_ThreadKernelTimeInfo.uliLastKernelTime=uli;
                g_ThreadKernelTimeInfo.liLastActivity=currentTicks;
            }
            else
            {
                if ( (currentTicks.QuadPart - g_ThreadKernelTimeInfo.liLastActivity.QuadPart > g_ThreadMaxInactivityInTicks.QuadPart) )
                {
                    // Thread1 exceeded the maximum allowed inactivity period
                    float SecondsInactive = (currentTicks.QuadPart - g_ThreadKernelTimeInfo.liLastActivity.QuadPart) * g_fSecsPerTick;
                    Print( L"========> Thread %08x was inactive for %f seconds.\n", g_tidThreads[0], SecondsInactive );
                    // waiting for the reboot controller key combination; in a real
                    // program, this could be replaced with a __debugbreak() or code
                    // to generate a dump file, or simply the information can be
                    // logged and the program will continue execution (if continuing,
                    // make sure the times above are reset appropriately)
                    DetectRebootKeypress();
                    // in the general case, the break below might or might not be
                    // reached, depending on the code above
                    break;
                }
            }
        }
    }

    CancelWaitableTimer( hTimer );

    return 0;
}
#pragma warning( pop )

#endif
//--------------------------------------------------------------------------------------
// End watchdog thread related code
//--------------------------------------------------------------------------------------


//--------------------------------------------------------------------------------------
// Name: DelayLoop()
// Desc: Busy loop to simulate work being done in the example threads
//--------------------------------------------------------------------------------------
void __declspec(noinline) DelayLoop( int count )
{
	int total=0;
	for ( int i=0; i<count*10000; i++ )
        {
            total+= (rand()-RAND_MAX/2);
        }
	Print( L"... random work done: total=%6d\n", total );
}

#define WORK(n) DelayLoop(n)

DWORD WINAPI ThreadProc1( LPVOID lpParameter )
{
    // Note2: without the WORK loops below, the sample threads might spend so short time
    // before control switches to a different thread that the kernel time recorded is
    // too low and it'll be basically truncated to 0
    WORK(100);
    Print( L"Thread 1 Running... (TID=%08x)\n", g_tidThreads[0] );

    EnterCriticalSection( &g_CsSharedDataA );
    Print( L"Thread 1 Inside A...\n" );
    WORK(1000);
    EnterCriticalSection( &g_CsSharedDataB );
    Print( L"Thread 1 Inside B...\n" );

    g_SharedDataA = 1;
    g_SharedDataB = 100;
    WORK(1000);

    // Release ownership of the critical section.  Any thread
    // waiting on the critical section may now be given access
    Print( L"Thread 1 preparing to Leave B...\n" );
    LeaveCriticalSection( &g_CsSharedDataB );
    WORK(1000);
    Print( L"Thread 1 preparing to Leave A...\n" );
    LeaveCriticalSection( &g_CsSharedDataA );

    Print( L"Thread 1 Exiting...\n" );

    return 0;
}

//--------------------------------------------------------------------------------------
// define controlling whether the Thread2 will try to aqcuire the critical sections in
// the reversed (wrong) order, thus potentially creating a deadlock with Thread1
//--------------------------------------------------------------------------------------
#define DEADLOCK 1

#ifndef DEADLOCK
DWORD WINAPI ThreadProc2( LPVOID lpParameter )
{
    WORK(100);
    Print( L"Thread 2 Running... (TID=%08x)\n", g_tidThreads[1] );

    EnterCriticalSection( &g_CsSharedDataA );
    Print( L"Thread 2 Inside A...\n" );
    WORK(1000);
    EnterCriticalSection( &g_CsSharedDataB );
    Print( L"Thread 2 Inside B...\n" );

    g_SharedDataA = 2;
    g_SharedDataB = 200;
    WORK(1000);

    // Release ownership of the critical section.  Any thread
    // waiting on the critical section may now be given access
    Print( L"Thread 2 preparing to Leave B...\n" );
    LeaveCriticalSection( &g_CsSharedDataB );
    WORK(1000);
    Print( L"Thread 2 preparing to Leave A...\n" );
    LeaveCriticalSection( &g_CsSharedDataA );

    Print( L"Thread 2 Exiting...\n" );

    return 0;
}
#else
DWORD WINAPI ThreadProc2( LPVOID lpParameter )
{
    Print( L"Thread 2 Running... (TID=%08x) ... and deadlocking with Thread 1\n", g_tidThreads[1] );

    // trying to acquire the critical section in opposite order than Thread1,
    // thus likely creating a deadlock
    EnterCriticalSection( &g_CsSharedDataB );
    Print( L"Thread 2 Inside B...\n" );
    WORK(1000);
    EnterCriticalSection( &g_CsSharedDataA );
    Print( L"Thread 2 Inside A...\n" );

    g_SharedDataA = 2;
    g_SharedDataB = 200;
    WORK(1000);

    // Release ownership of the critical section.  Any thread
    // waiting on the critical section may now be given access
    Print( L"Thread 2 preparing to Leave A...\n" );
    LeaveCriticalSection( &g_CsSharedDataA );
    WORK(1000);
    Print( L"Thread 2 preparing to Leave B...\n" );
    LeaveCriticalSection( &g_CsSharedDataB );

    Print( L"Thread 2 Exiting...\n" );

    return 0;
}
#endif

//--------------------------------------------------------------------------------------
// Name: main
// Desc: Entry point to the program.
//--------------------------------------------------------------------------------------
VOID __cdecl main()
{
    // Initialize the console window
    g_console.Create( "game:\\Media\\Fonts\\Arial_12.xpr", 0xFF0000FF, 0xFFFFFFFF );
    g_console.SendOutputToDebugChannel( TRUE );

    // Before a critical section can be used, it must be initialized using the
    // InitializeCriticalSection function.  For this sample, there are three critical
    // sections: two for protecting the access to the g_SharedDataA/...B global
    // variables, and another for providing serialized access to the Print()
    // function.
    InitializeCriticalSection( &g_CsSharedDataA );
    InitializeCriticalSection( &g_CsSharedDataB );
    InitializeCriticalSection( &g_CsPrint );

    const DWORD STACK_SIZE = 0; // default stack size

    // Create threads
    g_hThread1 = CreateThread( NULL, STACK_SIZE, ThreadProc1, (VOID *) NULL, 0, &g_tidThreads[0] );
    g_hThread2 = CreateThread( NULL, STACK_SIZE, ThreadProc2, (VOID *) NULL, 0, &g_tidThreads[1] );

    g_hThreads[0]=g_hThread1;
    g_hThreads[1]=g_hThread2;

    if( g_hThread1 == 0 || g_hThread2 == 0 )
    {
        Print( L"*** Error creating sample threads.\n" );
        DetectRebootKeypress();
        return; // to silence Code Analysis
    }

#ifdef WDTHREAD
    // Get the frequency of the timer
    LARGE_INTEGER qwTicksPerSec;
    QueryPerformanceFrequency( &qwTicksPerSec );
    g_fSecsPerTick = 1.0f / (float)qwTicksPerSec.QuadPart;

    InitThreadKernelTimeInfo();
    InitMaxInactivityTicks( SECONDS_INACTIVE );

    LARGE_INTEGER currentTicks;
    QueryPerformanceCounter( &currentTicks );

    g_ThreadKernelTimeInfo.liLastActivity=currentTicks;  // consider thread creation the last "activity"

    g_WDThread = CreateThread( NULL, STACK_SIZE, WDThreadProc, (VOID *) NULL, 0, &g_tidWDThread );
    if( g_WDThread )
    {
        SetThreadPriority( g_WDThread, THREAD_PRIORITY_HIGHEST );
    }
    else
    {
        Print( L"*** Error creating watchdog thread.\n" );
    }
#endif    

    // Wait for all threads to finish
    // When a thread terminates, the corresponding handle becomes signaled
    WaitForMultipleObjects( NUM_THREADS, g_hThreads, TRUE, INFINITE );

#ifdef WDTHREAD
    if( g_WDThread )
    {
        CloseHandle(g_WDThread);
    }
#endif

    // Close the other handles
    Print( L"Closing Thread1:\n" );
    CloseHandle( g_hThread1 );
    g_hThread1 = 0;
    Print( L"Thread1 closed.\n" );
    Print( L"Closing Thread2:\n" );
    CloseHandle( g_hThread2 );
    g_hThread2 = 0;
    Print( L"Thread2 closed.\n" );

    // The DeleteCriticalSection function releases all resources used by a 
    // critical section object.
    DeleteCriticalSection( &g_CsSharedDataA );
    DeleteCriticalSection( &g_CsSharedDataB );

    Print( L"g_SharedDataA=%4d, g_SharedDataB=%4d\n", g_SharedDataA, g_SharedDataB );
    DeleteCriticalSection( &g_CsPrint );

    Print( L"Normal program termination.\n" );

    DetectRebootKeypress();
}


//--------------------------------------------------------------------------------------
// Name: Print
// Desc: Helper function to send formatted output to the debug window and
// serialize access between the threads calling it
//--------------------------------------------------------------------------------------
VOID __cdecl Print( const WCHAR* strFormat, ... )
{
    va_list pArglist;
    
    va_start( pArglist, strFormat );   

    // Allow only a single thread at a time to write output
    EnterCriticalSection( &g_CsPrint );

    g_console.FormatV( strFormat, pArglist );

    LeaveCriticalSection( &g_CsPrint );
    va_end( pArglist );
}


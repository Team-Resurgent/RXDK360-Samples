//--------------------------------------------------------------------------------------
// Events.cpp
//
// Demonstrates the use of Event objects.  
// An event is a synchronization object used in multi-threaded titles.  An
// event can be in one of two states: signaled or nonsignaled.  A thread
// can wait for an event to become signaled before performing work.
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include <xtl.h>
#include <assert.h>


// Handle for the worker thread
HANDLE  g_hThread;

// Global synchronization event objects
HANDLE  g_hWorkEvent;          // Worker thread should perform work when signaled
HANDLE  g_hWorkCompleteEvent;  // Signaled when Worker thread has completed task 
HANDLE  g_hTerminateEvent;     // Worker thread should terminate when signaled

VOID __cdecl Print( const WCHAR* strFormat, ... );

// Structure used to name threads
typedef struct tagTHREADNAME_INFO
{
    DWORD dwType;     // must be 0x1000
    LPCSTR szName;    // pointer to name (in user address space)
    DWORD dwThreadID; // thread ID (-1 = caller thread)
    DWORD dwFlags;    // reserved for future use, must be zero
}       THREADNAME_INFO;


//--------------------------------------------------------------------------------------
// Name: SetThreadName()
// Desc: Set the name of the given thread so that it will show up in the Threads Window
//       in Visual Studio and in PIX timing captures.
//--------------------------------------------------------------------------------------
VOID SetThreadName( DWORD dwThreadID, LPCSTR strThreadName )
{
    THREADNAME_INFO info;
    info.dwType = 0x1000;
    info.szName = strThreadName;
    info.dwThreadID = dwThreadID;
    info.dwFlags = 0;

    __try
        {
        RaiseException( 0x406D1388, 0, sizeof( info ) / sizeof( DWORD ), ( DWORD* )&info );
        }
        __except( GetExceptionCode() == 0x406D1388 ?
                  EXCEPTION_CONTINUE_EXECUTION : EXCEPTION_EXECUTE_HANDLER )
        {
        __noop;
        }
}


//--------------------------------------------------------------------------------------
// Name: ThreadProc
// Desc: The thread worker function for this sample.  It waits on an event
//       to become signaled, displaying a message when it does.
//--------------------------------------------------------------------------------------
DWORD WINAPI ThreadProc( LPVOID lpParameter )
{
    // Give this thread a name
    SetThreadName( GetCurrentThreadId(), "Sample Thread" );

    Print( L"Worker thread started" );

    // Events that are waited on
    HANDLE hEvents[] =
    {
        g_hWorkEvent,           // Wait for notification of work to perform
        g_hTerminateEvent       // Wait for termination notification
    };

    for(; ; )
    {
        // The WaitForMultipleObjects function blocks until the one of the
        // two events becomes signaled.  
        // The timeout is set to INFINITE, which indicates that 
        // the thread should wait forever.

        DWORD dwResult = WaitForMultipleObjects(
            sizeof( hEvents ) / sizeof( hEvents[0] ),
            hEvents,
            FALSE,
            INFINITE );

        // The return value minus WAIT_OBJECT_0 is the index
        // into the hThreads array of the event which is signaled

        // Time to do some work...
        if( hEvents[ dwResult - WAIT_OBJECT_0 ] == g_hWorkEvent )
        {
            Print( L"Thread was awakened!" );

            Sleep( 5000 ); // Simulate work

            // Signal the g_hWorkCompleteEvent event to inform the
            // main thread that the work was completed
            SetEvent( g_hWorkCompleteEvent );
        }
        else
        {
            // Main thread would like us to terminate
            Print( L"Exiting worker thread" );
            break;
        }

    }

    return 0;
}


//--------------------------------------------------------------------------------------
// Name: main
// Desc: Entry point to the program.
//--------------------------------------------------------------------------------------
VOID __cdecl main()
{

    // Create the event objects
    // For demonstration purposes, three event objects are created.
    // When creating
    // an event, a title may decide on whether or not the event is initially
    // in a signaled state, and whether or not the event automatically
    // becomes nonsignaled after a single thread waiting on it becomes unblocked.

    // The first event is used to inform the worker thread that work should
    // be performed.  Since the manual reset parameter is FALSE (second parameter)
    // the event will go from signaled to nonsignaled once a thread waiting
    // on it becomes unblocked
    g_hWorkEvent = CreateEvent(
        NULL,   // Security attributes
        FALSE,  // Manual reset flag
        FALSE,  // Initially nonsignaled
        NULL ); // Name (NULL means none)

    // A second event is used by the worker thread to inform the main
    // thread that work is complete
    g_hWorkCompleteEvent = CreateEvent(
        NULL,   // Security attributes
        FALSE,  // Manual reset flag
        FALSE,  // Initially nonsignaled
        NULL ); // Name (NULL means none)

    // Finally, a third event is used to signal the worker thread that
    // it should terminate
    g_hTerminateEvent = CreateEvent(
        NULL,   // Security attributes
        FALSE,  // Manual reset flag
        FALSE,  // Initially nonsignaled
        NULL ); // Name (NULL means none)

    // Create worker thread
    // Initial size of the stack, in bytes. The system rounds this value to
    // the nearest page.  If this parameter is zero, the new thread uses the
    // default size for the executable. 

    const DWORD STACK_SIZE = 0;

    g_hThread = CreateThread( NULL, STACK_SIZE, ThreadProc, NULL, 0, NULL );
    assert(g_hThread != NULL);

    // For demonstartion purpose, go in a loop and signal the worker
    // thread to perform a task
    for( DWORD i = 0; i < 10; ++i )
    {
        Print( L"Main thread now setting the event" );

        SetEvent( g_hWorkEvent ); // Wake up the worker thread

        // For demonstration purposes, wait for the worker thread
        // to finish by waiting on the work complete event
        const DWORD dwTimeout = 0; // Milliseconds (zero means test immediately, 
        // INFINITE means wait forever)

        while( WaitForSingleObject( g_hWorkCompleteEvent, dwTimeout ) == WAIT_TIMEOUT )
        {
            // Do work related tasks...
        }
    }

    // Set the termination event, informing the worker thread that
    // it should exit
    SetEvent( g_hTerminateEvent );

    // Wait for the worker thread to terminate.  When a thread exits, it
    // becomes signaled
    WaitForSingleObject( g_hThread, INFINITE );

    // When an event is no longer needed, it should be released by calling
    // CloseHandle
    CloseHandle( g_hWorkEvent );
    CloseHandle( g_hWorkCompleteEvent );
    CloseHandle( g_hTerminateEvent );

    // Close the thread handle
    CloseHandle( g_hThread );

}


//--------------------------------------------------------------------------------------
// Name: Print
// Desc: Send formatted output to the debug window
//--------------------------------------------------------------------------------------
VOID __cdecl Print( const WCHAR* strFormat, ... )
{
    const int MAX_OUTPUT_STR = 512;
    WCHAR strBuffer[ MAX_OUTPUT_STR ];
    va_list pArglist;

    va_start( pArglist, strFormat );
    INT iChars = wvsprintfW( strBuffer, strFormat, pArglist );
    assert( iChars < MAX_OUTPUT_STR );

    OutputDebugStringW( L"\n*** ThreadSyncEvents: " );
    OutputDebugStringW( strBuffer );
    OutputDebugStringW( L"\n\n" );
    ( VOID )iChars; // avoid compiler warning

    va_end( pArglist );
}

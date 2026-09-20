//--------------------------------------------------------------------------------------
// SetProcessor.cpp
//
// Demonstrates the use of Multiple Processors and hardware threads by
// Using the XSetProcessor API.
//
// The kernel doesn't automatically migrate threads between processors.
// When a thread is created, it is assigned to the processor equal to the
// current processor.  The only way to move a thread to a different processor
// is to call XSetThreadProcessor.
//
// On the Xbox 360, there are 6 "virtual processors" (3 cores, 2 hardware threads per
// core).
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include <xtl.h>
#include <assert.h>


const DWORD         NUM_THREADS = 6;   // Number of threads to create

VOID __cdecl Print( const WCHAR* strFormat, ... );

// Handles for the running threads
HANDLE g_hThreads[ NUM_THREADS ] = { 0 };

// Critical section object providing exclusive thread access to 
// the Print() function (see below)
CRITICAL_SECTION    g_CsPrint;

// Structure used to name threads
typedef struct tagTHREADNAME_INFO
{
    DWORD dwType;     // must be 0x1000
    LPCSTR szName;    // pointer to name (in user address space)
    DWORD dwThreadID; // thread ID (-1 = caller thread)
    DWORD dwFlags;    // reserved for future use, must be zero
}                   THREADNAME_INFO;


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
// Desc: The thread worker function for this sample.
//--------------------------------------------------------------------------------------
DWORD WINAPI ThreadProc( LPVOID lpParameter )
{
    // Give this thread a name
    SetThreadName( GetCurrentThreadId(), "Sample Thread" );

    // The parameter is simply the index into the g_hThreads of
    // the handle associated with this thread
    DWORD dwThreadNumber = ( DWORD )lpParameter;

    Print( L"***Thread %lu Running on processors %lu***\n", dwThreadNumber, dwThreadNumber / 2 );
    Print( L"***Thread %lu Exiting***\n", dwThreadNumber );
    return 0;
}


//--------------------------------------------------------------------------------------
// Name: main
// Desc: Entry point to the program
//--------------------------------------------------------------------------------------
VOID __cdecl main()
{
    // Before a critical section can be used, it must be initialized using
    // the InitializeCriticalSection function.  
    InitializeCriticalSection( &g_CsPrint );

    Print( L"Creating Threads...\n\n" );

    // Create worker threads
    for( DWORD i = 0; i < NUM_THREADS; i++ )
    {
        // Initial size of the stack, in bytes. The system rounds this value to the nearest page. 
        // If this parameter is zero, the new thread uses the default size for the executable. 
        const DWORD STACK_SIZE = 0;

        Print( L"Creating thread %lu.\n", i );
        g_hThreads[i] = CreateThread( NULL, STACK_SIZE, ThreadProc, ( VOID* )i, CREATE_SUSPENDED, NULL );
        assert(g_hThreads[i] != NULL);

        // Set the processer of the thread.
        // 0 = Core 0, Thread 0
        // 1 = Core 0, Thread 1
        // 2 = Core 1, Thread 0
        // 3 = Core 1, Thread 1
        // 4 = Core 2, Thread 0
        // 5 = Core 2, Thread 1

        Print( L"Setting Thread %lu to processor %lu.\n", i, i );
        XSetThreadProcessor( g_hThreads[i], i );

        Print( L"Enabling thread %lu.\n", i );
        ResumeThread( g_hThreads[i] );

    }

    // Wait for all threads to finish
    // When a thread terminates, the corresponding handle becomes signaled
    WaitForMultipleObjects( NUM_THREADS, g_hThreads, TRUE, INFINITE );

    Print( L"\n\nAll Threads have terminated.\n\n" );

    // The DeleteCriticalSection function releases all resources used by an 
    // critical section object.
    DeleteCriticalSection( &g_CsPrint );

    // Close thread handles.
    for( DWORD i = 0; i < NUM_THREADS; i++ )
    {
        CloseHandle( g_hThreads[i] );
    }
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

    // Allow only a single thread at a time to write output
    EnterCriticalSection( &g_CsPrint );

    OutputDebugStringW( strBuffer );
    ( VOID )iChars; // avoid compiler warning

    LeaveCriticalSection( &g_CsPrint );
    va_end( pArglist );
}

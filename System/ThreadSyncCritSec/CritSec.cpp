//--------------------------------------------------------------------------------------
// CritSec.cpp
//
// Demonstrates the use of Critical Sections.  
// The threads of a single process can use a critical section object for 
// mutual-exclusion synchronization. There is no guarantee about the order in
// which threads will obtain ownership of the critical section, however,
// the system will be fair to all threads.
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include <xtl.h>
#include <assert.h>


const DWORD         NUM_THREADS = 10;   // Number of threads to create

VOID __cdecl Print( const WCHAR* strFormat, ... );

// Shared data access by multiple threads
DWORD               g_SharedData;

// Handles for the running threads
HANDLE g_hThreads[ NUM_THREADS ] = { 0 };

// Critical section object providing exclusive thread access to 
// the g_SharedData variable
CRITICAL_SECTION    g_CsSharedData;

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
// Desc: The thread worker function for this sample.  It merely takes
//       the shared data critical section and updates the g_SharedData global
//       variable before exiting.
//--------------------------------------------------------------------------------------
DWORD WINAPI ThreadProc( LPVOID lpParameter )
{
    // Give this thread a name
    SetThreadName( GetCurrentThreadId(), "Sample Thread" );

    // The parameter is simply the index into the g_hThreads of
    // the handle associated with this thread
    DWORD dwThreadNumber = ( DWORD )lpParameter;

    Print( L"Thread %lu Running...", dwThreadNumber );

    // The EnterCriticalSection is used to obtain exclusive
    // access to a resource.  The current thread will block
    // until access is granted.  After the function returns,
    // the thread has ownership, until it calls LeaveCriticalSection.
    // A thread must call LeaveCriticalSection once for each time 
    // that it calls EnterCriticalSection.

    EnterCriticalSection( &g_CsSharedData );

    // Only a single thread can execute this code, with all others
    // being blocked until it calls LeaveCriticalSection.
    g_SharedData = dwThreadNumber;

    // Release ownership of the critical section.  Any thread
    // waiting on the critical section may now be given access
    LeaveCriticalSection( &g_CsSharedData );

    Print( L"Thread %lu Exiting...", dwThreadNumber );

    return 0;
}


//--------------------------------------------------------------------------------------
// Name: main
// Desc: Entry point to the program.
//--------------------------------------------------------------------------------------
VOID __cdecl main()
{

    // Before a critical section can be used, it must be initialized using
    // the InitializeCriticalSection function.  For this sample, there
    // are two critical sections: one for protecting access to the g_SharedData
    // global variable, and another for providing serialized access to the
    // Print() function.
    InitializeCriticalSection( &g_CsSharedData );
    InitializeCriticalSection( &g_CsPrint );

    // Create worker threads
    for( DWORD i = 0; i < NUM_THREADS; ++i )
    {
        // Initial size of the stack, in bytes. The system rounds this value to the nearest page. 
        // If this parameter is zero, the new thread uses the default size for the executable. 
        const DWORD STACK_SIZE = 0;

        g_hThreads[i] = CreateThread( NULL, STACK_SIZE, ThreadProc, ( VOID* )i, 0, NULL );
        assert(g_hThreads[i] != NULL);
    }

    // Wait for all threads to finish
    // When a thread terminates, the corresponding handle becomes signaled
    WaitForMultipleObjects( NUM_THREADS, g_hThreads, TRUE, INFINITE );

    // Close all the handles

    for( DWORD i = 0; i < NUM_THREADS; ++i )
    {
        CloseHandle( g_hThreads[i] );
    }
    // The DeleteCriticalSection function releases all resources used by an 
    // critical section object.
    DeleteCriticalSection( &g_CsPrint );
    DeleteCriticalSection( &g_CsSharedData );

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

    OutputDebugStringW( L"\n*** ThreadSyncCritSec: " );
    OutputDebugStringW( strBuffer );
    OutputDebugStringW( L"\n\n" );
    ( VOID )iChars; // avoid compiler warning

    LeaveCriticalSection( &g_CsPrint );
    va_end( pArglist );
}

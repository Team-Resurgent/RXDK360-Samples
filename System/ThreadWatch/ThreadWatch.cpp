//--------------------------------------------------------------------------------------
// ThreadWatch.cpp
//
// The sample implements a watchdog mechanism that monitors the changes in the state of
// a thread of interest. The application connects to the console and periodically
// samples the state of the thread. A lack of changes can signify a possible frozen
// thread.
//
// Microsoft Game Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0400 
#endif
#include <windows.h>
#include <xbdm.h>
#include <xdevkit.h>

#include <stdio.h>
#include <tchar.h>

#define RELEASE_IF_NOT_NULL(p) if((p)!=NULL) (p)->Release();
#define STR(a) #a
#define ERRMSG_AND_GOTO(errlabel,fmt,...) { printf(fmt,__VA_ARGS__); goto errlabel; }


//--------------------------------------------------------------------------------------
// begin parameters section
//
// the following global values and defines can be adjusted in order to change the
// operation (see the documentation for more details)
// these values can be set from the command line or by any other methods
//--------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------
// For simplicity, store below the thread ID to watch
//--------------------------------------------------------------------------------------
DWORD g_ThreadIDwWatch = 0xf9000004;

//--------------------------------------------------------------------------------------
// the number of milliseconds between thread state checks
//--------------------------------------------------------------------------------------
LONG g_lTimerTick = 73;

//--------------------------------------------------------------------------------------
// how many times the thread state has to be unchanged in order to consider the thread
// as "frozen"
//--------------------------------------------------------------------------------------
#define MAX_SAMESTATE_HISTORY 4
//--------------------------------------------------------------------------------------
// counter for the states found identical so far
//--------------------------------------------------------------------------------------
int g_samestatecount = 0;

//--------------------------------------------------------------------------------------
// set below on true to generate a dump with full heap if the thread is considered
// "frozen"
//--------------------------------------------------------------------------------------
bool g_bGenerateDump = false;
//--------------------------------------------------------------------------------------
// enable the boolean below in order to inhibit restarting the console app,
// in case it has been stopped (i.e. it does what the name says)
//--------------------------------------------------------------------------------------
bool g_bLeaveTargetStopped = false;

//--------------------------------------------------------------------------------------
// How many times the thread checking is performed; set it to 0 to keep checking until a
// "frozen" condition is detected
//--------------------------------------------------------------------------------------
int g_iMaxCheckCount = 100;

//--------------------------------------------------------------------------------------
// flag controlling the display of extra information, such as the stack values detected
// for each check
//--------------------------------------------------------------------------------------
int g_bVerbose = false;

//--------------------------------------------------------------------------------------
// end parameters section
//--------------------------------------------------------------------------------------


//--------------------------------------------------------------------------------------
// the COM Debug Monitor Interfaces used
//--------------------------------------------------------------------------------------
IXboxManager* g_pIXboxManager = NULL;
IXboxConsole* g_pIXboxConsole = NULL;
IXboxDebugTarget* g_pIDebugTarget = NULL;
IXboxThreads* g_pIXboxThreads = NULL;
IXboxThread* g_pIXboxThread = NULL;
//--------------------------------------------------------------------------------------
// BSTR used
//--------------------------------------------------------------------------------------
BSTR g_bstrDefaultXboxName = NULL; 
BSTR g_bstrDebuggerName = NULL;
BSTR g_bstrDumpName = NULL;

//--------------------------------------------------------------------------------------
// after the detection loop exits, this will be true if the thread of interest is
// considered "frozen"
//--------------------------------------------------------------------------------------
bool g_bFrozen = false;
//--------------------------------------------------------------------------------------
// after the detection loop exits, this flag stores the state of the target application
//--------------------------------------------------------------------------------------
bool g_bTargetRunning = true;


//--------------------------------------------------------------------------------------
// Name: ThreadState
// Desc: structure for my thread state definition, fit for the purpose of this tool;
//       expand/enhance/augment as necessary
//--------------------------------------------------------------------------------------
#define MAXSTACK 32
typedef struct threadstate_s {
	XBOX_THREAD_INFO xti;
	int stackcount;
	LONG stack[MAXSTACK];
} ThreadState;

//--------------------------------------------------------------------------------------
// structures holding the last checked state and the newly retrieved state
//--------------------------------------------------------------------------------------
ThreadState oldts = { 0 };
ThreadState newts = { 0 };

//--------------------------------------------------------------------------------------
// Name: IsSameThreadSameState()
// Desc: Compares two ThreadState structures: returns true if there was no change
//       detected for the *same* thread; false otherwise
//       Side effects: saves the new state if there were changes
//                     increments the counter for the number of same states detected
//--------------------------------------------------------------------------------------
bool IsSameThreadSameState( ThreadState *poldts, ThreadState *pnewts )
{
    bool b_samestate = true;

    if( memcmp( &(poldts->xti.CreateTime), &(pnewts->xti.CreateTime), sizeof(VARIANT)) != 0 )
    {   
        // new creation time ? different thread with the same tID
        b_samestate = false; g_samestatecount = 0;
        printf( "New Time: newly created thread with the same ThreadID\n" );
    }

    if( b_samestate && (poldts->stackcount != pnewts->stackcount) )
    {   
        // changes in the stack depth ... not same state
        b_samestate = false; g_samestatecount = 0;
        printf( "Stack count has changed\n" );
    }

    if( b_samestate && (memcmp( poldts->stack,pnewts->stack,poldts->stackcount*sizeof(LONG)) != 0) )
    {
        // same stack depth, but different values... not same state
        b_samestate = false; g_samestatecount = 0;
        printf( "Stack has changed\n" );
    }

    if( !b_samestate )
    {
        *poldts = *pnewts;      // save new thread state
    }

    if( b_samestate )
    {
        g_samestatecount++;
    }

    if( g_samestatecount >= MAX_SAMESTATE_HISTORY ) 
    {
        // same thread ID, same creation time, same stack depth and values...
        // nothing changed for the last MAX_SAMESTATE_HISTORY state checks
        printf( "Last %d states were the same.\n", g_samestatecount );
        return true;            
    }

    return false;
}

//--------------------------------------------------------------------------------------
// Name: GetThreadStack()
// Desc: Retrieves in a ThreadState the call stack of a thread specified by its thead ID
//--------------------------------------------------------------------------------------
HRESULT GetThreadStack(	IXboxThread* pIXboxThread,
                        ThreadState * pts,
                        DWORD tID)  // last arg not really needed other than to
                                    // help in providing nicer error messages
{
    IXboxStackFrame* pIXboxStackFrame = NULL;
    IXboxStackFrame* pIXboxNextStackFrame = NULL;

    if( FAILED(pIXboxThread->get_TopOfStack( &pIXboxStackFrame )) )
    {
        ERRMSG_AND_GOTO( MyExit, "Thread ID %08lx: Cannot get top of stack\n", tID);
    }

    HRESULT hr = S_OK;

    int stack_level;
    for( stack_level = 0; hr == S_OK && stack_level < MAXSTACK; stack_level++ )
    {
        LONG iar = 0;
        VARIANT_BOOL vb;
        if( FAILED(pIXboxStackFrame->GetRegister32( eXboxRegisters32::iar, &iar, &vb )) )
        {
            ERRMSG_AND_GOTO( MyExit, "Thread ID %08lx: frame %2ld: Cannot IAR\n", tID, stack_level );
        }

        pts->stack[stack_level]=iar;

        if( g_bVerbose )
        {
            printf( "Thread ID %08lx:\t%2d\t%08lx\n", tID, stack_level, iar );
        }

        hr = pIXboxStackFrame->get_NextStackFrame( &pIXboxNextStackFrame );

        RELEASE_IF_NOT_NULL( pIXboxStackFrame );    // just in case the 'get_NextStackFrame' doesn't release and simply overwrites
        pIXboxStackFrame = pIXboxNextStackFrame;    // move it
        pIXboxNextStackFrame = NULL;                // keep it NULL and clean

    }

    pts->stackcount = stack_level;

    RELEASE_IF_NOT_NULL( pIXboxStackFrame );
    return S_OK;

MyExit:
    RELEASE_IF_NOT_NULL( pIXboxStackFrame );
    return E_FAIL;
}

//--------------------------------------------------------------------------------------
// Name: ScanThreads()
// Desc: Scans the threads of the target application and optionally records the state
//       of the thread of intereset (if it is found)
//
//       use a pts NULL argument as a signal to just scan and print information about all
//       threads; (the tID is ignored then);
//       used only once to display a list of all threads
//
//       use a proper pts argument in conjunction with a thread ID to save that thread's
//       state into the supplied thread state structure
//--------------------------------------------------------------------------------------
HRESULT ScanThreads( DWORD tID, ThreadState *pts )   
{
    if( FAILED(g_pIDebugTarget->get_Threads( &g_pIXboxThreads )) )
    {
        ERRMSG_AND_GOTO( MyExit, "Cannot get the threads enumeration interface\n" );
    }

    LONG lThreadCount = 0;
    if( FAILED(g_pIXboxThreads->get_Count( &lThreadCount )) )
    {
        ERRMSG_AND_GOTO( MyExit, "Cannot get thread count\n" );
    }

    if( g_bVerbose && pts==NULL )
    {
        printf( "%d threads on %S\n", lThreadCount,g_bstrDefaultXboxName );
    }

    if( g_bVerbose )
    {
        printf( "\n%8s\t%8s\t%8s\t%8s\n", STR(ThreadId), STR(TlsBase), STR(StartAddress), STR(StackBase), STR(StackLimit) );
    }

    for( long l = 0; l < lThreadCount; ++l )
    {
        if( FAILED(g_pIXboxThreads->get_Item( l, &g_pIXboxThread )) )
        {
            ERRMSG_AND_GOTO( MyExit, "Failed to get thread # %ld\n", l);
        }

        DWORD dwThreadID = 0;
        if( FAILED(g_pIXboxThread->get_ThreadId( &dwThreadID )) )
        {
            ERRMSG_AND_GOTO( MyExit, "Failed to get thread ID for thread # %ld\n", l);
        }

        if( dwThreadID == tID || pts==NULL )
        {

            XBOX_THREAD_INFO xti;

            if( FAILED(g_pIXboxThread->get_ThreadInfo( &xti )) )
            {
                ERRMSG_AND_GOTO( MyExit, "Failed to get thread info for thread # %ld\n", l );
            }

            if( g_bVerbose )
            {
                printf( "%08lx\t%08lx\t%08lx\t%08lx\t", xti.ThreadId, xti.TlsBase, xti.StartAddress, xti.StackBase, xti.StackLimit );

                if( xti.Name )
                {
                    printf( "%S", xti.Name );
                }

                if( xti.ThreadId == tID )
                {
                    printf( "*" );
                }

                printf( "\n" );
            }

            if( pts != NULL )
            {
                pts->xti = xti;
                if( FAILED(GetThreadStack( g_pIXboxThread, &newts, pts->xti.ThreadId )) )
                    ERRMSG_AND_GOTO( MyExit, "Failed to get thread stack for thread # %ld\n", l);
            }
        }

        RELEASE_IF_NOT_NULL( g_pIXboxThread );
        g_pIXboxThread=NULL;
    }
    RELEASE_IF_NOT_NULL( g_pIXboxThreads );
    g_pIXboxThreads=NULL;
    return S_OK;

MyExit:
    RELEASE_IF_NOT_NULL( g_pIXboxThread );
    g_pIXboxThread=NULL;
    RELEASE_IF_NOT_NULL( g_pIXboxThreads );
    g_pIXboxThreads=NULL;
    return E_FAIL;
}

//----------------------------------------------------------------------------
// Name: _tmain
// Desc: Entry point for program
//----------------------------------------------------------------------------
int _tmain( int argc, _TCHAR* argv[] )
{
    // minimal command line processing
    for ( int argi=1 ; argi < argc; argi++ )
    {
        if( strcmp(argv[argi],"-v")==0 )
                g_bVerbose = true;
        if( strcmp(argv[argi],"-d")==0 )
                g_bGenerateDump = true;
        if( strcmp(argv[argi],"-s")==0 )
                g_bLeaveTargetStopped = true;
    }

    HRESULT hr=S_OK;

    hr = CoInitialize( NULL );
    if ( hr == S_FALSE )
        printf( "Already CoInitialized ...\n" );
    if ( FAILED(hr) ) {
        printf( "CoInitialize failed...\n" );
        exit( 1 );
    }

    hr = CoCreateInstance(
            __uuidof(XboxManager),
            NULL,
            CLSCTX_INPROC_SERVER,
            __uuidof(IXboxManager),
            reinterpret_cast<void**>(&g_pIXboxManager) );

    if( FAILED(hr) )
    {
        ERRMSG_AND_GOTO( MyExit, "CoCreateInstance failed\n" );
    }

    // get default console name
    if( FAILED(g_pIXboxManager->get_DefaultConsole(&g_bstrDefaultXboxName)) )
    {
        ERRMSG_AND_GOTO(MyExit,"Cannot get default console name\n");
    }

    printf( "Using default console target: %S\n", g_bstrDefaultXboxName );

    // get the interface for the default console
    if( FAILED(g_pIXboxManager->OpenConsole( g_bstrDefaultXboxName, &g_pIXboxConsole )) )
    {
        ERRMSG_AND_GOTO( MyExit, "Cannot open default console %S\n", g_bstrDefaultXboxName );
    }

    // get a debug interface for the console
    if( FAILED(g_pIXboxConsole->get_DebugTarget( &g_pIDebugTarget )) )
    {
        ERRMSG_AND_GOTO( MyExit, "Cannot get debug target\n" );
    }

    if( g_bVerbose )
    {
        printf( "Connecting as a debugger...\n" );
    }

    g_bstrDebuggerName = SysAllocString( L"ThreadWatch" );
    if( g_bstrDebuggerName==NULL )
    {
        ERRMSG_AND_GOTO( MyExit, "SysAllocString failed\n" );
    }

    // connect as a debugger to the console
    if( FAILED(g_pIDebugTarget->ConnectAsDebugger( g_bstrDebuggerName, eXboxDebugConnectFlags::Force) ) )
    {
        ERRMSG_AND_GOTO( MyExit, "Cannot connect as a debugger to %S\n", g_bstrDefaultXboxName );
    }

    // scan the threads on the target (if verbose this will print a list of the
    // available threads
    if( FAILED(ScanThreads( g_ThreadIDwWatch, NULL) ) )
    {
        ERRMSG_AND_GOTO( MyExit, "ScanThreads failed\n" );
    }

    // ======== the watchdog loop starts here
    
    LARGE_INTEGER duetime;
    duetime.QuadPart = (__int64)-10*1000L*1000L; // not really much used actually;
                                                 // the g_lTimerTick will provide the periodic wake-up 

    HANDLE hTimer = CreateWaitableTimer( NULL, FALSE, "WatchDogTimer" );

    SetWaitableTimer( hTimer, &duetime, g_lTimerTick, NULL, NULL, TRUE );

    // set the g_iMaxCheckCount check limit on 0 to keep checking until a lack of changes
    // in the state of the watched thread is detected, case when it will break the loop
    for ( int counter = 0 ; g_iMaxCheckCount == 0 || counter < g_iMaxCheckCount ; counter++ )
    {
        WaitForSingleObject( hTimer, INFINITE );

        // stop the target ...
        VARIANT_BOOL bWasStopped;
        hr=g_pIDebugTarget->Stop( &bWasStopped );
        if( FAILED(hr) )
        {
            ERRMSG_AND_GOTO( MyExit, "Failed to stop the target\n" );
        }

        g_bTargetRunning = false;

        // scan threads on target and grab the currect ThreadState for the thread of
        // interest
        ScanThreads( g_ThreadIDwWatch, &newts );

        if( IsSameThreadSameState( &oldts, &newts ) )
        {
            g_bFrozen = true;
            break;
        }

        // restart the target ...
        VARIANT_BOOL bNotStopped;
        hr=g_pIDebugTarget->Go( &bNotStopped );
        if( FAILED(hr) )
        {
            ERRMSG_AND_GOTO( MyExit, "Failed to restart the target\n" );
        }
        
        g_bTargetRunning = true;        

        if( g_bVerbose )
        {
            printf( "Tick # %d\n", counter );
        }
    }

    CancelWaitableTimer( hTimer );

    // ======== end of the watchdog loop

    if( g_bFrozen ) 
    {
        printf( "Thread %08lx might be frozen\n", g_ThreadIDwWatch );

        if( g_bGenerateDump )
        {
            // get a dump of the target
            g_bstrDumpName = SysAllocString( L"twdump.dmp" );
            if ( g_bstrDumpName==NULL )
            {
                ERRMSG_AND_GOTO( MyExit, "SysAllocString failed\n" );
            }

            printf( "Generating dump with full heap\n" );

            hr=g_pIDebugTarget->WriteDump( g_bstrDumpName, eXboxDumpFlags::WithFullMemory );

            if( FAILED(hr) )
                ERRMSG_AND_GOTO(MyExit,"Error saving dump\n" );
        }
        
    }

    if( g_bTargetRunning == false && g_bLeaveTargetStopped == false )
    {
        // restart the target ...
        VARIANT_BOOL bNotStopped;
        hr=g_pIDebugTarget->Go( &bNotStopped );
        if(FAILED(hr))
        {
            ERRMSG_AND_GOTO( MyExit, "Failed restarting the target\n" );
        }
        
        g_bTargetRunning = true;
    }

MyExit:

    // cleanup
    if ( g_bstrDefaultXboxName != NULL)
    {
        SysFreeString( g_bstrDefaultXboxName );
    }
    if ( g_bstrDebuggerName != NULL)
    {
        SysFreeString( g_bstrDebuggerName );
    }
    if ( g_bstrDumpName != NULL)
    {
        SysFreeString( g_bstrDumpName );
    }
    if(g_pIDebugTarget)
    {
        g_pIDebugTarget->DisconnectAsDebugger();
    }

    RELEASE_IF_NOT_NULL( g_pIXboxThread );
    RELEASE_IF_NOT_NULL( g_pIXboxThreads );
    RELEASE_IF_NOT_NULL( g_pIDebugTarget );
    RELEASE_IF_NOT_NULL( g_pIXboxConsole );
    RELEASE_IF_NOT_NULL( g_pIXboxManager );

    CoUninitialize();

    return (int) g_bFrozen;
}


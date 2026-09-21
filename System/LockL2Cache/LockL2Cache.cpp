//--------------------------------------------------------------------------------------
// LockL2Cache.cpp
//
// Game Technology Group.
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
#include "AtgConsole.h"

#include <process.h>
#include <xbdm.h>

//--------------------------------------------------------------------------------------
// Globals
//--------------------------------------------------------------------------------------

// Console for output
ATG::Console    g_Console;

// Flag for crude signaling to worker thread that it is time to exit.
volatile bool   g_endThread;

// Global variable to store results in to stop the optimizer from removing calculations.
__int64         g_sum;


//--------------------------------------------------------------------------------------
// Name: PurgeCache()
// Desc: Flush all data from the L2 (and L1 d-caches) in order to get a fresh run.
//--------------------------------------------------------------------------------------
VOID PurgeCache()
{
    const DWORD cacheSize = 1024 * 1024;
    const DWORD numWays = 8;
    // Allocate a large buffer. Use physical alloc to ensure a known mapping to the L2
    // cache which is addressed with physical addresses.
    char* pBuffer = ( char* )XPhysicalAlloc( cacheSize, MAXULONG_PTR, 0,
                                             PAGE_READWRITE | MEM_LARGE_PAGES );

    __int64 sum = 0;

    // Scan through all 1024 sets in the L2 cache.
    for( DWORD i = 0; i < cacheSize / numWays; i += 128 )
    {
        // Load all eight cache lines in this set (one from each way) into the
        // cache.
        for( DWORD wayNum = 0; wayNum < numWays; ++wayNum )
        {
            sum += pBuffer[ i + wayNum * 128 * 1024 ];
        }
        // Purge all eight cache lines in this set, ensuring that the set is
        // now completely empty.
        for( DWORD wayNum = 0; wayNum < numWays; ++wayNum )
        {
            __dcbf( i + wayNum * 128 * 1024, pBuffer );
        }
    }
    // Make sure the optimizer doesn't completely remove the loop.
    g_sum = sum;

    XPhysicalFree( pBuffer );
}


//--------------------------------------------------------------------------------------
// Name: StartCPUPerfCounters()
// Desc: Start the performance counters with the specified set of counters.
//--------------------------------------------------------------------------------------
VOID StartCPUPerfCounters( ePMCSetup whichSetup )
{
    DmPMCInstallAndStart( whichSetup );
}


//--------------------------------------------------------------------------------------
// Name: StopCPUPerfCounters()
// Desc: Stop the libpmcpb performance counters and print the analysis to the debugger.
//--------------------------------------------------------------------------------------
VOID StopCPUPerfCounters()
{
    DmPMCStopAndReport();
}


//--------------------------------------------------------------------------------------
// Name: TimeReusingData()
// Desc: Walk the array of data multiple times, reading from every cache line and
//       summing up the data. If the data fits into the cache space available to it
//       then performance will be good.
//       If the data does not fit--either because it is bigger than L2 or it is
//       restricted to just part of L2--then performance will suffer.
//--------------------------------------------------------------------------------------
DWORD TimeReusingData( __int64* pArray, DWORD itemCount, DWORD iterationCount,
                       const char* descriptor )
{
    // Purge the array from the cache to ensure clean and consistent measurements.
    PurgeCache();

    const DWORD arraySize = itemCount * sizeof( pArray[ 0 ] );
    printf( "\nRepeated access to %d-KB array%s:", arraySize / 1024, descriptor );

    // Optionally use the performance counters to record and print extra information
    // about how the code is running. When running this test the most interesting
    // counter is counter 14 of the overview set, "EL2 P0 d miss", which
    // records how many L2 misses were incurred by core 0.
    // The perf counter information is displayed to the debugger output window.
    bool CPUPerfCounters = true;
    if( CPUPerfCounters )
        StartCPUPerfCounters( PMC_SETUP_OVERVIEW_PB0T0 );

    // Use a DWORD to avoid rare errors with bit 32 of __mftb. 32-bits of precision
    // is more than enough.
    DWORD startTime = ( DWORD )__mftb();

    __int64 sum = 0;

    // Scan the data multiple times.
    for( DWORD iter = 0; iter < iterationCount; ++iter )
    {
        for( DWORD i = 0; i < itemCount; i += 128 / sizeof( __int64 ) )
        {
            // "Process" one entire cache line. In this case we just read
            // a single value from it, to minimize calculation overhead and
            // simulate extremely well optimized code. This simplified loop
            // exaggerates the cache locking effect to make it more visible.
            sum += pArray[ i ];

            // Prefetching could be placed here. If the data is likely to be in main
            // memory then prefetching is a good idea. If the data is likely to be in
            // L2 then it is less important.
        }
    }
    // Make sure the optimizer doesn't completely remove the loop.
    g_sum = sum;

    DWORD elapsedTicks = ( DWORD )__mftb() - startTime;
    if( CPUPerfCounters )
        StopCPUPerfCounters();

    g_Console.Format( "Repeated access in %d-KB array%s:\n",
                      arraySize / 1024, descriptor );
    g_Console.Format( "    %d ticks.\n", elapsedTicks );

    return elapsedTicks;
}


//--------------------------------------------------------------------------------------
// Name: StreamProcessData()
// Desc: Walk the array of data multiple times, as quickly as possible. This function
//       assumes that the data will not fit in L2 so it prefetches it 1-KB ahead.
//       Depending on the value of data this function will either lock the data into
//       one way of the L2 cache, or not lock it at all.
//       This function runs on hardware thread 2.
//--------------------------------------------------------------------------------------
unsigned __stdcall StreamProcessData( void* data )
{
    // Put the memory scanning thread on hardware thread 2
    XSetThreadProcessor( GetCurrentThread(), 2 );

    g_endThread = false;
    bool lockL2 = ( bool )( data != 0 );

    DWORD arraySize = 2 * 1024 * 1024;
    const DWORD itemCount = arraySize / sizeof( __int64 );
    // Allocate a 1-MB buffer. Data that is locked into L2 must be allocated as
    // contiguous physical memory because the L2 cache uses physical addressing.
    __int64* pArray = ( __int64* )XPhysicalAlloc( arraySize, MAXULONG_PTR, arraySize,
                                                  PAGE_READWRITE | MEM_LARGE_PAGES );

    if( lockL2 )
    {
        // Lock the array into one way (128-KB) of the L2 cache. This prevents our
        // array from using the other seven ways.
        BOOL result = XLockL2( XLOCKL2_INDEX_TITLE, pArray, arraySize,
                               XLOCKL2_LOCK_SIZE_1_WAY, 0 );
        ( void )result;   // Avoid unreferenced variable warnings.
        assert( result );
    }

    // Walk through the array multiple times summing the data inside it. The array
    // is to big to fit in the cache so performance will be limited by memory
    // bandwidth and latency (prefetching would help).
    while( !g_endThread )
    {
        __int64 sum = g_sum;

        for( DWORD i = 0; i < itemCount; i += 128 / sizeof( __int64 ) )
        {
            // "Process" one entire cache line. In this case we just read
            // a single value from it, to minimize calculation overhead and
            // simulate extremely well optimized code. This simplified loop
            // exaggerates the cache locking effect to make it more visible.
            sum += pArray[ i ];

            // Prefetch 8 cache lines ahead.
            __dcbt( 1024, pArray + i );
        }
        // Make sure the optimizer doesn't completely remove the loop.
        g_sum = sum;
    }

    if( lockL2 )
        XUnlockL2( XLOCKL2_INDEX_TITLE );

    return 0;
}


//--------------------------------------------------------------------------------------
// Name: TestLockingStreamedData()
// Desc: Measure the effect of locking streamed data to one cache way on the performance
//       of other code. Locking the streamed data can have a beneficial effect on other
//       threads.
//--------------------------------------------------------------------------------------
VOID TestLockingStreamedData()
{
    const DWORD arraySize = 512 * 1024;
    const DWORD arrayCount = arraySize / sizeof( __int64 );

    // Allocate memory for the data to be reused.
    __int64* pArray = new __int64[ arrayCount ];

    DWORD elapsedTicks[ 2 ];

    for( DWORD lockStreamingData = 0; lockStreamingData < 2; ++lockStreamingData )
    {
        const DWORD stackSize = 0x10000;

        // Create a thread that constantly reads through a large block of memory,
        // with or without cache locking.
        HANDLE childThread = ( HANDLE )_beginthreadex( 0, stackSize, StreamProcessData,
                                                       ( void* )lockStreamingData, 0, 0 );

        // While that thread is running, the main thread repeatedly reads from a 512-KB block
        // of memory.
        const DWORD loopCount = 500;
        const char* message = lockStreamingData ? ", streaming data locked" : ", streaming data unlocked";
        elapsedTicks[ lockStreamingData ] = TimeReusingData( pArray, arrayCount, loopCount, message );

        // Trigger the child-thread to terminate. Not a robust general purpose
        // synchronization technique.
        g_endThread = true;
        WaitForSingleObject( childThread, INFINITE );
        CloseHandle( childThread );
    }

    g_Console.Format( "Locking streaming data gave a %1.2fx speedup to other code.\n\n",
                      double( elapsedTicks[ 0 ] ) / elapsedTicks[ 1 ] );

    delete [] pArray;
}


//--------------------------------------------------------------------------------------
// Name: TestLockingReusedData()
// Desc: Measure the effect of locking reused data to one cache way on the performance
//       of other code. Locking the data to too small an area can hurt performance.
//--------------------------------------------------------------------------------------
VOID TestLockingReusedData()
{
    const DWORD arraySize = 512 * 1024;
    const DWORD arrayCount = arraySize / sizeof( __int64 );

    // Allocate physical memory for the array that we want to lock into the cache.
    // The size must be a power of two, the alignment must be the same as the size,
    // and the memory must be cacheable. We use 64-KB pages (MEM_LARGE_PAGES) because
    // this is always a good idea.
    __int64* pArray = ( __int64* )XPhysicalAlloc( arraySize, MAXULONG_PTR, arraySize,
                                                  PAGE_READWRITE | MEM_LARGE_PAGES );

    const DWORD loopCount = 500;
    // Walk through the array multiple times summing the data inside it. After
    // the first pass the data will be in the L2 cache and performance will be
    // much better.
    DWORD elapsed = TimeReusingData( pArray, arrayCount, loopCount, " with reused data unlocked" );


    // Lock the array into one way (128-KB) of the L2 cache. This prevents our
    // array from using the other seven ways.
    BOOL result = XLockL2( XLOCKL2_INDEX_TITLE, pArray, arraySize, XLOCKL2_LOCK_SIZE_1_WAY, 0 );
    if( !result )
    {
        // This should never happen in correctly written code.
        g_Console.Format( "Error: Failed to lock cache.\n" );
        return;
    }

    // Walk through the array multiple times summing the data inside it. Because
    // the array is restricted to just using one of the eight ways of the L2 cache
    // the data does not fit, so each pass through the data has to reload it from
    // main memory.
    DWORD elapsedLocked = TimeReusingData( pArray, arrayCount, loopCount, " with data locked to one way" );

    g_Console.Format( "Locking reused data gave a %1.2fx slowdown.\n",
                      double( elapsedLocked ) / elapsed );

    // Unlock the cache and free the memory.
    XUnlockL2( XLOCKL2_INDEX_TITLE );
    XPhysicalFree( pArray );
}


//--------------------------------------------------------------------------------------
// Name: main
// Desc: Entry point to the program
//--------------------------------------------------------------------------------------
INT __cdecl main()
{
    // Initialize the console window
    g_Console.Create( "game:\\Media\\Fonts\\Courier_New_11.xpr", 0xFF1F005F, 0xFFFFFFFF );

    for(; ; )
    {
        TestLockingStreamedData();
        TestLockingReusedData();

        g_Console.Format( "Press LT + RT + RB to exit, A to run tests again.\n" );
        for(; ; )
        {
            ATG::GAMEPAD* pGamepad = ATG::Input::GetMergedInput(); // Detect reboot keypress
            if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_A )
            {
                g_Console.Format( "\n" );
                break;  // Rerun tests
            }
        }
    }
}

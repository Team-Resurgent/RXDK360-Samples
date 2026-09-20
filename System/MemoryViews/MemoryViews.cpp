//--------------------------------------------------------------------------------------
// MemoryViews.cpp
//
// This sample demonstrates the usage of the Xbox 360 memory view macros to treat one
// type of memory like another type.  In particular, write-combined memory can be viewed
// as cached read-only, which greatly improves read speed from that memory.
//
// This sample also demonstrates a CPU hardware hang that can occur when the cached
// read-only memory views are used on large page size allocations.  Take note of the
// text in the InvokeCPUHardwareBug function to learn how to avoid this CPU hang.
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

//--------------------------------------------------------------------------------------
// Globals
//--------------------------------------------------------------------------------------

// Console for output
ATG::Console    g_Console;

// Global variable to stop the optimizer from removing calculations.
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
    CHAR* pBuffer = ( char* )XPhysicalAlloc( cacheSize, MAXULONG_PTR, 0,
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
// Name: InitializeBuffer()
// Desc: Fills a buffer with increasing DWORD values.  Writes are generated in order due
//       to the volatile keyword.
//
//       Performance note:
//       This algorithm is not the fastest way to write to memory; it is simply a fast
//       way to write to many different types of memory with one algorithm, including
//       write-combined memory.  When only cached destinations are allowed, careful use 
//       of __dcbz128 can greatly increase write speed.
//--------------------------------------------------------------------------------------
VOID InitializeBuffer( volatile BYTE* __restrict pBuffer, DWORD dwSize )
 {
    assert( ( dwSize & 0x7F ) == 0 );

    DWORD dwValue = 0;
    volatile DWORD* p = ( volatile DWORD* )pBuffer;
    volatile DWORD* pEnd = ( volatile DWORD* )( pBuffer + dwSize );

    while( p < pEnd )
 {
        p[0] = dwValue;
        p[1] = dwValue + 1;
        p[2] = dwValue + 2;
        p[3] = dwValue + 3;

        p[4] = dwValue + 4;
        p[5] = dwValue + 5;
        p[6] = dwValue + 6;
        p[7] = dwValue + 7;

        p[8] = dwValue + 8;
        p[9] = dwValue + 9;
        p[10] = dwValue + 10;
        p[11] = dwValue + 11;

        p[12] = dwValue + 12;
        p[13] = dwValue + 13;
        p[14] = dwValue + 14;
        p[15] = dwValue + 15;

        p += 16;
    }
}


//--------------------------------------------------------------------------------------
// Name: ReadBuffer()
// Desc: Reads a buffer in memory very quickly.  Note that nothing is actually
//       done with the data.
//--------------------------------------------------------------------------------------
VOID ReadBuffer( const BYTE* pBuffer, DWORD dwSize )
{
    assert( ( dwSize & 0x7F ) == 0 );

    volatile XMVECTOR* p = ( XMVECTOR* )pBuffer;
    volatile XMVECTOR* pEnd = ( XMVECTOR* )( pBuffer + dwSize );

    while( p < pEnd )
    {
        // Prefetch 10 cache lines ahead.
        __dcbt( 1280, ( const VOID* )p );
        p[0];
        p[1];
        p[2];
        p[3];
        p[4];
        p[5];
        p[6];
        p[7];
        p += 8;
    }
}


//--------------------------------------------------------------------------------------
// Name: InvokeCPUHardwareBug()
// Desc: Reads a value from a noncached large page size destination twice, once from the 
//       cached read-only view and then once from the noncached view.  This function
//       will hard-hang the CPU core where the reads were executed, which may prevent 
//       even the debugger from operating.  A manual cold reboot is required to unhang
//       the CPU.
//--------------------------------------------------------------------------------------
VOID InvokeCPUHardwareBug()
{
    // Description of the hard hang
    g_Console.Format( "\n\nAbout to hard-hang the CPU.\n" );
    g_Console.Format( "This bug is encountered when memory in a large page size (64KB) "
                      "allocation is read from a\n"
                      "cached read-only view and then the same data is read from the "
                      "noncached view.\n" );
    g_Console.Format( "If the cached read-only data is still in the L1 cache, the "
                      "noncached read will cause the\n"
                      "hardware fault.  Note that the hardware fault only hangs the "
                      "CPU core where the reads\n"
                      "were executed; in this sample, that core is core 0.\n" );
    g_Console.Format( "To work around this issue, avoid reading data from noncached "
                      "large page memory, and \n"
                      "instead use the cached read-only view exclusively.  Remember "
                      "that the cached read-only\n"
                      "view is not cache coherent, so it may see stale data if a "
                      "previous write to the same\n"
                      "memory has not fully completed yet.\n" );
    g_Console.Format( "If this bug is suspected in a game title, use the "
                      "XEnableSmallPagesOverride API to force\n"
                      "the use of small 4KB pages all the time.  This API will "
                      "negatively impact memory performance,\nso use it only for "
                      "debugging purposes.  If the crash goes "
                      "away after switching to 4KB pages,\nthis bug is the likely "
                      "culprit.  Use trace recording to track down all noncached reads\n"
                      "and eliminate them.\n" );

    // Let the developer know how to recover the kit from this crash
    g_Console.Format( "\n\nThe development kit must be cold rebooted now.\n" );

    // Allocate a small chunk of write combined memory using 64KB pages
    volatile DWORD* pBuffer = ( volatile DWORD* )XPhysicalAlloc( 256 * 1024,
                                                                 MAXULONG_PTR, 0,
                                                                 PAGE_READWRITE |
                                                                 PAGE_WRITECOMBINE |
                                                                 MEM_LARGE_PAGES );

    // Create a cached read-only pointer to the same memory
    volatile DWORD* pBufferCRO = ( volatile DWORD* )GPU_CONVERT_CPU_TO_CPU_CACHED_READONLY_ADDRESS( ( VOID* )pBuffer );

    // Read from the cached read-only view
    DWORD dwDummyCRO = *pBufferCRO;

    // Read from the noncached view (this read will hang the CPU)
    DWORD dwDummy = *pBuffer;

    // This line will rarely be executed.
    g_Console.Format( "This message should not be visible.  %d %d\n", dwDummy, dwDummyCRO );
}


//--------------------------------------------------------------------------------------
// Name: main
// Desc: Entry point to the program
//--------------------------------------------------------------------------------------
VOID __cdecl main()
{
    // Initialize the console window
    g_Console.Create( "game:\\Media\\Fonts\\Courier_New_11.xpr", 0xFF1F005F, 0xFFFFFFFF );
    g_Console.SendOutputToDebugChannel( TRUE );

    // Tests will use a 8MB buffer size.
    const DWORD dwTestBufferSize = 8 * 1024 * 1024;

    // Flag to enable/disable usage of large pages for the tests.  The code below can
    // set or clear this flag and run the tests again.
    BOOL bUseLargePages = TRUE;

    for(; ; )
    {
        // Create memory allocations.
        DWORD dwLargePagesFlag = bUseLargePages ? MEM_LARGE_PAGES : 0;
        const CHAR* strLargePages = bUseLargePages ? " using 64KB large pages"
            : " using 4KB pages";

        g_Console.Format( "Creating a %d KB write-combined physical memory buffer%s.\n",
                          dwTestBufferSize / 1024, strLargePages );
        BYTE* pBufferWC = ( BYTE* )XPhysicalAlloc( dwTestBufferSize,
                                                   MAXULONG_PTR, 0,
                                                   PAGE_READWRITE | PAGE_WRITECOMBINE | dwLargePagesFlag );

        g_Console.Format( "Creating a %d KB cached physical memory buffer%s.\n",
                          dwTestBufferSize / 1024, strLargePages );
        BYTE* pBufferCP = ( BYTE* )XPhysicalAlloc( dwTestBufferSize,
                                                   MAXULONG_PTR, 0,
                                                   PAGE_READWRITE | dwLargePagesFlag );

        g_Console.Format( "Creating a %d KB cached memory buffer%s.\n",
                          dwTestBufferSize / 1024, strLargePages );
        BYTE* pBufferV = ( BYTE* )VirtualAlloc( NULL, dwTestBufferSize,
                                                MEM_COMMIT | dwLargePagesFlag,
                                                PAGE_READWRITE );
        assert( pBufferV != NULL );

        g_Console.Format( "\n" );

        const DWORD dwMegabyte = 1024 * 1024;

        // Run a write test on the write-combined memory.
        {
            PurgeCache();

            ATG::Timer ExperimentTime;

            InitializeBuffer( pBufferWC, dwTestBufferSize );

            FLOAT fTime = ( FLOAT )ExperimentTime.GetElapsedTime();
            FLOAT fRate = ( FLOAT )( dwTestBufferSize / dwMegabyte ) / fTime;
            g_Console.Format( "Writing to write-combined memory at %0.1f MB/sec.\n", fRate );
        }

        // Run a read test on the write-combined memory.
        {
            PurgeCache();

            ATG::Timer ExperimentTime;

            ReadBuffer( pBufferWC, dwTestBufferSize );

            FLOAT fTime = ( FLOAT )ExperimentTime.GetElapsedTime();
            FLOAT fRate = ( FLOAT )( dwTestBufferSize / dwMegabyte ) / fTime;
            g_Console.Format( "Reading from write-combined memory at %0.1f MB/sec.\n", fRate );
        }

        // Run a read test on the write-combined memory, using a cached-read only view.
        {
            PurgeCache();

            // Use the conversion function to create a cached read-only view of the
            // write combined memory.  Note that nothing actually changes in the page
            // table or system memory; this is merely some bit twiddling on the pointer.
            BYTE* pBufferWCCachedReadOnly = ( BYTE* )GPU_CONVERT_CPU_TO_CPU_CACHED_READONLY_ADDRESS( ( VOID* )
                                                                                                     pBufferWC );
            ATG::Timer ExperimentTime;

            ReadBuffer( pBufferWCCachedReadOnly, dwTestBufferSize );

            FLOAT fTime = ( FLOAT )ExperimentTime.GetElapsedTime();
            FLOAT fRate = ( FLOAT )( dwTestBufferSize / dwMegabyte ) / fTime;
            g_Console.Format( "Reading from cached read-only view of write-combined memory at %0.1f MB/sec.\n",
                              fRate );
        }

        g_Console.Format( "\n" );

        // Run a write test on cached physical memory.
        {
            PurgeCache();

            ATG::Timer ExperimentTime;

            InitializeBuffer( pBufferCP, dwTestBufferSize );

            FLOAT fTime = ( FLOAT )ExperimentTime.GetElapsedTime();
            FLOAT fRate = ( FLOAT )( dwTestBufferSize / dwMegabyte ) / fTime;
            g_Console.Format( "Writing to cached physical memory at %0.1f MB/sec.\n", fRate );
        }

        // Run a read test on cached physical memory.
        {
            PurgeCache();

            ATG::Timer ExperimentTime;

            ReadBuffer( pBufferCP, dwTestBufferSize );

            FLOAT fTime = ( FLOAT )ExperimentTime.GetElapsedTime();
            FLOAT fRate = ( FLOAT )( dwTestBufferSize / dwMegabyte ) / fTime;
            g_Console.Format( "Reading from cached physical memory at %0.1f MB/sec.\n", fRate );
        }

        // Run a read test on cached physical memory, using a cached read-only view.
        {
            PurgeCache();

            BYTE* pBufferCPCachedReadOnly = ( BYTE* )GPU_CONVERT_CPU_TO_CPU_CACHED_READONLY_ADDRESS( pBufferCP );
            ATG::Timer ExperimentTime;

            ReadBuffer( pBufferCPCachedReadOnly, dwTestBufferSize );

            FLOAT fTime = ( FLOAT )ExperimentTime.GetElapsedTime();
            FLOAT fRate = ( FLOAT )( dwTestBufferSize / dwMegabyte ) / fTime;
            g_Console.Format( "Reading from cached read-only view of cached physical memory at %0.1f MB/sec.\n",
                              fRate );
        }

        g_Console.Format( "\n" );

        // Run a write test on cached memory.
        {
            PurgeCache();

            ATG::Timer ExperimentTime;

            InitializeBuffer( pBufferV, dwTestBufferSize );

            FLOAT fTime = ( FLOAT )ExperimentTime.GetElapsedTime();
            FLOAT fRate = ( FLOAT )( dwTestBufferSize / dwMegabyte ) / fTime;
            g_Console.Format( "Writing to cached memory at %0.1f MB/sec.\n", fRate );
        }

        // Run a read test on cached memory.
        {
            PurgeCache();

            ATG::Timer ExperimentTime;

            ReadBuffer( pBufferV, dwTestBufferSize );

            FLOAT fTime = ( FLOAT )ExperimentTime.GetElapsedTime();
            FLOAT fRate = ( FLOAT )( dwTestBufferSize / dwMegabyte ) / fTime;
            g_Console.Format( "Reading from cached memory at %0.1f MB/sec.\n", fRate );
        }

        // Free memory buffers.
        XPhysicalFree( pBufferWC );
        XPhysicalFree( pBufferCP );
        VirtualFree( pBufferV, 0, MEM_RELEASE );

        g_Console.Format(
            "\nPress LT + RT + RB to exit, A to run tests again with large pages, B to run tests again with small pages, or press Y to demonstrate the cached read-only hardware hang.\n" );
        for(; ; )
        {
            ATG::GAMEPAD* pGamepad = ATG::Input::GetMergedInput(); // Detect reboot keypress
            if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_A )
            {
                // Rerun tests with large pages
                bUseLargePages = TRUE;
                g_Console.Format( "\n" );
                break;
            }
            if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_B )
            {
                // Rerun tests with small pages
                bUseLargePages = FALSE;
                g_Console.Format( "\n" );
                break;
            }
            if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_Y )
            {
                // Hang the CPU using cached read-only views on noncached memory
                InvokeCPUHardwareBug();
                break;
            }
        }
    }
}

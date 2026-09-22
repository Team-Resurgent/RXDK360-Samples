//--------------------------------------------------------------------------------------
// FastMemCopy.cpp
//
// This sample shows how to use the various memory copy APIs, and how to measure their 
// performance.
//
// XNA Developer Connection
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "stdafx.h"
#include "AtgConsole.h"
#include "AtgInput.h"

//--------------------------------------------------------------------------------------
// Variables which we can change at runtime using the controller:
//--------------------------------------------------------------------------------------

// What routine are we testing:
const char*         g_strMemCopyRoutineNames[] =
{
    "memcpy",
    "XMemCpy",
    "XMemCpy128",
    "XMemCpyStreaming",
    "memset",
    "XMemSet",
    "XMemSet128",
};
enum MemCopyRoutine
{
    MEM_COPY_ROUTINE_MEMCPY,
    MEM_COPY_ROUTINE_XMEMCPY,
    MEM_COPY_ROUTINE_XMEMCPY128,
    MEM_COPY_ROUTINE_XMEMCPYSTREAMING,
    MEM_COPY_ROUTINE_MEMSET,
    MEM_COPY_ROUTINE_XMEMSET,
    MEM_COPY_ROUTINE_XMEMSET128,

    MEM_COPY_ROUTINE_MAX,
};
static DWORD        g_dwMemCopyRoutine = MEM_COPY_ROUTINE_MEMCPY;

// Is the destination in cacheable or write-combined memory:
static BOOL         g_bDestCacheable = TRUE;

// What is the relative alignment between src and dst?
// If the following value is zero, then src and dst use the same sets of cache lines.  
// We guarantee this by allocating with 128 Kb alignment, since L2 cache address equals 
// virtual address modulo 128 Kb.
// Accesses to the same 'set' of the cache are serialized (i.e. accesses to addresses 
// separated by a multiple of 128 Kb).
const DWORD g_dwDefaultRelativeAlignments[] =
{
    0,
    1,      // misaligned buffers are very slow for memcpy
    2,
    4,
    8,
    16,
    128,    // one L1/L2 cache line
    1024,
    1024 + 128,
    8192,   // size of one 'way' of the L1 cache
    8192 + 128,
    65536,  // last must be largest
};
static DWORD        g_dwRelativeAlignmentIndex = 0;

// Is the data in the cache (if TRUE, src is prefetched and dst is pre-zeroed; if FALSE,
// src and dst are purged):
static BOOL         g_bCacheWarm = FALSE;

// What size buffer are we copying/setting:
const DWORD g_dwDefaultBufferSizes[] =
{
    64 * 1024,
    4 * 1024 * 1024, // last must be largest
};
const DWORD         g_dwLargestBufferSize = g_dwDefaultBufferSizes[ARRAYSIZE( g_dwDefaultBufferSizes ) - 1];
static DWORD        g_dwBufferSizeIndex = 0;

// If memset, is the value being set zero or non-zero:
static BYTE         g_cMemSetValue = 0;


//--------------------------------------------------------------------------------------
// Named constant values:
//--------------------------------------------------------------------------------------

// Include some padding to test relative alignments.  We allocate so that src and dst 
// will have a cache collision with each other by default.
const DWORD         g_dwAllocationPadding = g_dwDefaultRelativeAlignments[ARRAYSIZE( g_dwDefaultRelativeAlignments ) -
    1];

// The size of the allocations for the src/dst data:
const DWORD         g_dwAllocationSize = g_dwLargestBufferSize + g_dwAllocationPadding;

// Stats about the L2 cache:
const DWORD         g_dwCacheLineSize = 128;
const DWORD         g_dwL2CacheSize = 1024 * 1024;
const DWORD         g_dwL2CacheWays = 8;

// Units:
const DWORD         g_dwTicksPerSecond = 50000000;  // This isn't exact.
const DWORD         g_dwBytesPerGbyte = 1024 * 1024 * 1024;

// How many times we repeat the test to measure avg performance:
const DWORD         g_dwNumRepetitions = 64;


//--------------------------------------------------------------------------------------
// Non-constant globals:
//--------------------------------------------------------------------------------------

static ATG::Console g_Console;
static BOOL         g_bDoTests = TRUE;
static BOOL         g_bCheckCorrectness = TRUE;
static char*        g_pCacheableSrc = NULL;
static char*        g_pCacheableDst = NULL;
static char*        g_pWriteCombinedDst = NULL;


//--------------------------------------------------------------------------------------
// Helper functions:
//--------------------------------------------------------------------------------------

template <typename t_type> static inline BOOL AlignedToPowerOf2( const t_type& t, DWORD dwPowerOf2 )
{
    return ( ( ( DWORD )t ) & ( dwPowerOf2 - 1 ) ) == 0;
}

template <typename t_type> static inline t_type RoundDownToPowerOf2( const t_type& t, DWORD dwPowerOf2 )
{
    return ( t_type )( ( ( DWORD )t ) & ~( dwPowerOf2 - 1 ) );
}

template <typename t_type> static inline t_type RoundUpToPowerOf2( const t_type& t, DWORD dwPowerOf2 )
{
    return ( t_type )( ( ( ( DWORD )t ) + ( dwPowerOf2 - 1 ) ) & ~( dwPowerOf2 - 1 ) );
}

static double GBPerSec( __int64 ticks, size_t count )
{
    double sec = ticks / ( double )g_dwTicksPerSecond;
    return ( count / ( double )g_dwBytesPerGbyte ) / sec;
}


//--------------------------------------------------------------------------------------
// Cache modification functions:
//--------------------------------------------------------------------------------------

// Kick data out of the cache
static void Purge( const char* pData, DWORD dwSize )
{
    // Expand input range to be cache-line aligned:
    const char* pEnd = pData + dwSize;
    pData = RoundDownToPowerOf2( pData, g_dwCacheLineSize );
    dwSize = RoundUpToPowerOf2( pEnd, g_dwCacheLineSize ) - pData;

    for( size_t i = 0; i < dwSize; i += g_dwCacheLineSize )
    {
        __dcbf( i, pData );
    }
}

// Kick data into the cache.  
static void Prefetch( const char* pData, DWORD dwSize )
{
    // Expand input range to be cache-line aligned:
    const char* pEnd = pData + dwSize;
    pData = RoundDownToPowerOf2( pData, g_dwCacheLineSize );
    dwSize = RoundUpToPowerOf2( pEnd, g_dwCacheLineSize ) - pData;

    for( size_t i = 0; i < dwSize; i += g_dwCacheLineSize )
    {
        __dcbt( i, pData );
    }
}

// Initialize the cache to zero.  
static void PreZero( char* pData, DWORD dwSize )
{
    // For correctness, the inputs cannot share cache lines with other data.
    assert( AlignedToPowerOf2( pData, g_dwCacheLineSize ) );
    assert( AlignedToPowerOf2( dwSize, g_dwCacheLineSize ) );

    for( size_t i = 0; i < dwSize; i += g_dwCacheLineSize )
    {
        __dcbz128( i, pData );
    }
}


//--------------------------------------------------------------------------------------
// Name: DoTests
// Desc: Perform one of the memory tests using current settings
//--------------------------------------------------------------------------------------
DWORD DoTests( BOOL bCheckCorrectness )
{
    BOOL bAllowed = TRUE;   // Check if the operation meets constraints, or we'll crash 

    char* pSrc = g_pCacheableSrc;
    char* pDst = ( g_bDestCacheable ? g_pCacheableDst : g_pWriteCombinedDst )
        + g_dwDefaultRelativeAlignments[g_dwRelativeAlignmentIndex];

    if( bCheckCorrectness )
    {
        // Clear Dst to something other than the correct result
        for( DWORD i = 0; i < g_dwDefaultBufferSizes[g_dwBufferSizeIndex]; ++i )
        {
            pDst[i] = g_cMemSetValue + 1;
        }
    }

    // Even though we never read from pDst, having it in the cache makes a big 
    // difference for memcpy and memset.  The XMem functions internally zero out pDst 
    // cache lines with __dcbz128.
    if( g_bCacheWarm )
    {
        // Use at most half the cache for src and for dst
        Prefetch( pSrc, min( g_dwDefaultBufferSizes[g_dwBufferSizeIndex], g_dwL2CacheSize / 2 ) );
        if( g_bDestCacheable ) PreZero( pDst, min( g_dwDefaultBufferSizes[g_dwBufferSizeIndex],
                                                   g_dwL2CacheSize / 2 ) );
    }
    else
    {
        Purge( pSrc, g_dwDefaultBufferSizes[g_dwBufferSizeIndex] );
        if( g_bDestCacheable ) Purge( pDst, g_dwDefaultBufferSizes[g_dwBufferSizeIndex] );
    }

    // We incur some measurement overhead from the switch statement, function call etc.
    // Use a DWORD to avoid rare errors with bit 32 of __mftb. 32-bits of precision
    // is more than enough.
    DWORD start = ( DWORD )__mftb();

    switch( g_dwMemCopyRoutine )
    {
        case MEM_COPY_ROUTINE_MEMCPY:
            // No requirements on alignment, or cache-ability.
            memcpy( pDst, pSrc, g_dwDefaultBufferSizes[g_dwBufferSizeIndex] );
            break;

        case MEM_COPY_ROUTINE_XMEMCPY:
            // Dst must be cacheable.
            if( g_bDestCacheable )
                XMemCpy( pDst, pSrc, g_dwDefaultBufferSizes[g_dwBufferSizeIndex] );
            else
                bAllowed = FALSE;
            break;

        case MEM_COPY_ROUTINE_XMEMCPY128:
            // Dst must be cacheable; Src must be 16-byte aligned; Dst and Size must be 
            // cache-line aligned.
            if( g_bDestCacheable && AlignedToPowerOf2( pSrc, 16 )
                && AlignedToPowerOf2( pDst, g_dwCacheLineSize )
                && AlignedToPowerOf2( g_dwDefaultBufferSizes[g_dwBufferSizeIndex], g_dwCacheLineSize ) )
                XMemCpy128( pDst, pSrc, g_dwDefaultBufferSizes[g_dwBufferSizeIndex] );
            else
                bAllowed = FALSE;
            break;

        case MEM_COPY_ROUTINE_XMEMCPYSTREAMING:
            // No requirements on alignment, or cache-ability.
            XMemCpyStreaming( pDst, pSrc, g_dwDefaultBufferSizes[g_dwBufferSizeIndex] );
            break;

        case MEM_COPY_ROUTINE_MEMSET:
            // No requirements on alignment, or cache-ability.
            memset( pDst, g_cMemSetValue, g_dwDefaultBufferSizes[g_dwBufferSizeIndex] );
            break;

        case MEM_COPY_ROUTINE_XMEMSET:
            // Dst must be cacheable.
            if( g_bDestCacheable )
                XMemSet( pDst, g_cMemSetValue, g_dwDefaultBufferSizes[g_dwBufferSizeIndex] );
            else
                bAllowed = FALSE;
            break;

        case MEM_COPY_ROUTINE_XMEMSET128:
            // Dst must be cacheable; Src must be 16-byte aligned; Size must be 
            // cache-line aligned.
            if( g_bDestCacheable && AlignedToPowerOf2( pDst, g_dwCacheLineSize )
                && AlignedToPowerOf2( g_dwDefaultBufferSizes[g_dwBufferSizeIndex], g_dwCacheLineSize ) )
                XMemSet128( pDst, g_cMemSetValue, g_dwDefaultBufferSizes[g_dwBufferSizeIndex] );
            else
                bAllowed = FALSE;
            break;

        default:
            assert( false );    // unreachable
            bAllowed = FALSE;
    }

    DWORD lDuration = ( bAllowed ) ? ( ( ( DWORD )__mftb() ) - start ) : 0; // 0 signals invalid

    // Check correctness
    if( bAllowed && bCheckCorrectness )
    {
        switch( g_dwMemCopyRoutine )
        {
            case MEM_COPY_ROUTINE_MEMCPY:
            case MEM_COPY_ROUTINE_XMEMCPY:
            case MEM_COPY_ROUTINE_XMEMCPY128:
            case MEM_COPY_ROUTINE_XMEMCPYSTREAMING:
                for( DWORD i = 0; i < g_dwDefaultBufferSizes[g_dwBufferSizeIndex]; ++i )
                {
                    assert( pDst[i] == pSrc[i] );
                }
                break;

            case MEM_COPY_ROUTINE_MEMSET:
            case MEM_COPY_ROUTINE_XMEMSET:
            case MEM_COPY_ROUTINE_XMEMSET128:
                for( DWORD i = 0; i < g_dwDefaultBufferSizes[g_dwBufferSizeIndex]; ++i )
                {
                    assert( pDst[i] == g_cMemSetValue );
                }
                break;
        }
    }

    return lDuration;
}


//--------------------------------------------------------------------------------------
// Name: main
// Desc: Entry point to the program
//--------------------------------------------------------------------------------------
INT __cdecl main()
{
    assert( ARRAYSIZE( g_strMemCopyRoutineNames ) == MEM_COPY_ROUTINE_MAX );

    // Initialize the console window
    g_Console.Create( "game:\\Media\\Fonts\\Courier_New_11.xpr", 0xFF1F005F, 0xFFFFFFFF );

    // Allocate the buffers.  We use an alignment of 128 Kb, so that src and dst always
    // map to the same cache lines, unless we copy with an offset.
    g_pCacheableSrc = ( char* )XPhysicalAlloc( g_dwAllocationSize, MAXULONG_PTR,
                                               g_dwL2CacheSize / g_dwL2CacheWays, MEM_LARGE_PAGES | PAGE_READWRITE );
    g_pCacheableDst = ( char* )XPhysicalAlloc( g_dwAllocationSize, MAXULONG_PTR,
                                               g_dwL2CacheSize / g_dwL2CacheWays, MEM_LARGE_PAGES | PAGE_READWRITE );
    g_pWriteCombinedDst = ( char* )XPhysicalAlloc( g_dwAllocationSize, MAXULONG_PTR,
                                                   g_dwL2CacheSize / g_dwL2CacheWays,
                                                   MEM_LARGE_PAGES | PAGE_READWRITE | PAGE_WRITECOMBINE );

    // Initialize the source buffer with some distinctive data.
    for( DWORD i = 0; i < g_dwAllocationSize; ++i )
    {
        g_pCacheableSrc[i] = ( char )rand();
    }

    for(; ; )
    {
        // Check for input
        ATG::GAMEPAD* pGamepad = ATG::Input::GetMergedInput(); // Detect keypress
        if( pGamepad->wPressedButtons )
        {
            g_bDoTests = TRUE;

            if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_B )
            {
                g_bDestCacheable = !g_bDestCacheable;
            }

            if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_X )
            {
                g_cMemSetValue = ( ++g_cMemSetValue ) % 2;
            }

            if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_Y )
            {
                g_bCacheWarm = !g_bCacheWarm;
            }

            if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_LEFT_SHOULDER )
            {
                g_dwMemCopyRoutine = ( g_dwMemCopyRoutine + MEM_COPY_ROUTINE_MAX - 1 ) % MEM_COPY_ROUTINE_MAX;
            }
            else if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER )
            {
                g_dwMemCopyRoutine = ( ++g_dwMemCopyRoutine ) % MEM_COPY_ROUTINE_MAX;
            }

            if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_DPAD_DOWN )
            {
                g_dwRelativeAlignmentIndex = ( g_dwRelativeAlignmentIndex +
                                               ARRAYSIZE( g_dwDefaultRelativeAlignments ) -
                                               1 ) % ARRAYSIZE( g_dwDefaultRelativeAlignments );
            }
            if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_DPAD_UP )
            {
                g_dwRelativeAlignmentIndex = ( ++g_dwRelativeAlignmentIndex ) %
                    ARRAYSIZE( g_dwDefaultRelativeAlignments );
            }

            if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_DPAD_LEFT )
            {
                g_dwBufferSizeIndex = ( g_dwBufferSizeIndex + ARRAYSIZE( g_dwDefaultBufferSizes ) -
                                        1 ) % ARRAYSIZE( g_dwDefaultBufferSizes );
            }
            else if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_DPAD_RIGHT )
            {
                g_dwBufferSizeIndex = ( ++g_dwBufferSizeIndex ) % ARRAYSIZE( g_dwDefaultBufferSizes );
            }
        }

        // If there was input, run the tests
        if( g_bDoTests )
        {
            // Check duration.  Use __int64 to support a large number of repititions.
            __int64 lDuration = 0;
            for( DWORD i = 0; i < g_dwNumRepetitions; ++i )
            {
                lDuration += DoTests( FALSE );
            }
            if( g_bCheckCorrectness )
            {
                DoTests( TRUE );    // Only do this once, as it is slow.
            }

            // Print results & instructions to console
            g_Console.Format( "FastMemCopy --- current settings:\n" );
            g_Console.Format( "Copy routine:       %20s (change with LB/RB)\n",
                              g_strMemCopyRoutineNames[g_dwMemCopyRoutine] );
            g_Console.Format( "Destination:        %20s (change with B)\n",
                              g_bDestCacheable ? "cacheable" : "write-combined" );
            g_Console.Format( "Relative alignment: %20d (change with DPAD UP/DOWN)\n",
                              g_dwDefaultRelativeAlignments[g_dwRelativeAlignmentIndex] );
            g_Console.Format( "Cache:              %20s (change with Y)\n",
                              g_bCacheWarm ? "warm" : "cold" );
            g_Console.Format( "Buffer size:        %20d (change with DPAD RIGHT/LEFT)\n",
                              g_dwDefaultBufferSizes[g_dwBufferSizeIndex] );
            g_Console.Format( "MemSet value:       %20s (change with X)\n",
                              g_cMemSetValue ? "non-zero" : "zero" );
            g_Console.Format( "\n" );
            if( lDuration > 0 )
            {
                g_Console.Format( "Test results:       %20.4f Gb/sec\n",
                                  ( FLOAT )GBPerSec( lDuration, g_dwDefaultBufferSizes[g_dwBufferSizeIndex] *
                                                     g_dwNumRepetitions ) );
            }
            else
            {
                g_Console.Format( "Test results:       %20s\n", "(INVALID CONFIGURATION)" );
            }
            g_Console.Format( "\n" );
            g_Console.Format( "Press A to run test again.\n" );
            g_Console.Format( "Press LT + LB + RT + RB to exit.\n" );
            g_Console.Format( "\n" );
            g_Console.Format( "\n" );

            g_bDoTests = FALSE;
        }
    }

    // We can never really reach these lines...
    //XPhysicalFree( g_pCacheableSrc );
    //XPhysicalFree( g_pCacheableDst );
    //XPhysicalFree( g_pWriteCombinedDst );
}

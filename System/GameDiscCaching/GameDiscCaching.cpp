//--------------------------------------------------------------------------------------
// GameDiscCaching.cpp
//
// This sample demonstrates how to use game disc caching in your title. The sample 
// is set up to default to DVD emulation with "Accurate Seek Times".  For more 
// accuracy, you should run an optimized build and create a DVD layout in 
// xbGameDisc.
//
// Game Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include <xtl.h>
#include <xbdm.h>
#include <xfilecache.h>
#include <AtgInput.h>
#include "AtgConsole.h"
#include <stdio.h>
#include <cassert>

//--------------------------------------------------------------------------------------
// Globals
//--------------------------------------------------------------------------------------

// Console for output
ATG::Console    g_Console;

// Non-cached files
const CHAR*     g_strNonCachedFiles[] =
{
    "game:\\media\\test_file0.bin",
    "game:\\media\\test_file1.bin",
    "game:\\media\\test_file2.bin",
    "game:\\media\\test_file3.bin",
    "game:\\media\\test_file4.bin",
};


// Cached files
const CHAR*     g_strCachedFiles[] =
{
    "game:\\media\\test_file5.bin",
    "game:\\media\\test_file6.bin",
    "game:\\media\\test_file7.bin",
    "game:\\media\\test_file8.bin",
    "game:\\media\\test_file9.bin",
};


// Mark some files to not be cached. Normally streamed files should not be cached.
const CHAR*     g_strNonCacheableFiles[] =
{
    "*.wma",            // Do not cache media files
    "*.wmv",
};


//--------------------------------------------------------------------------------------
// Name: FileCacheCallback
// Desc: Callback that occurs when the background caching thread caches data. Depending
//       on the return value, the about to be cached file can be paused, or allowed
//       to be cached.  If we request a pause, the callback will be called again after
//       the requested pause duration.  We will receive one callback every time the
//       caching system wants to hit the disc --- which could be many times for a single
//       file.
//--------------------------------------------------------------------------------------
static DWORD FileCacheCallback( PVOID pContext, const CHAR* strPath, PLARGE_INTEGER pFileOffset,
                                DWORD dwNumberOfBytesToRead )
{
    // File 9 will be paused for 1 second the first time we get a cache request.
    static BOOL bFirstTime = TRUE;
    if( strPath != NULL && strcmp( strPath, "game:\\media\\test_file9.bin" ) == 0 && bFirstTime )
    {
        bFirstTime = FALSE;
        return XFILECACHE_READFILEADVISORY_HOLDOFF( 1000 );
    }
    else    // Everything else can be cached immediately
    {
        return XFILECACHE_READFILEADVISORY_OK;
    }
}

//--------------------------------------------------------------------------------------
// Name: main
// Desc: Entry point to the program
//--------------------------------------------------------------------------------------
INT __cdecl main()
{
    DWORD dwErr = ERROR_SUCCESS;

    // Initialize the console window
    g_Console.Create( "game:\\Media\\Fonts\\Courier_New_11.xpr", 0xFF1F005F, 0xFFFFFFFF );

    // Test whether we are being played from hard drive
    DWORD dwLicenseMask;
    BOOL bIsPlayedFromHardDrive = ( XContentGetLicenseMask( &dwLicenseMask, NULL ) == ERROR_SUCCESS );

    if ( !bIsPlayedFromHardDrive )
    {
        // Initialize the cache
        dwErr = XFileCacheInit( XFILECACHE_CLEAR_ALL,   // Clear HDD cache before using it
            0,                      // Use maximum available space (2 GB)
            5,                      // Hardware thread to use
            0x7ffff,                // Scratch buffer size
            0x00010000 );           // Title version DWORD

        if( dwErr != ERROR_SUCCESS )
        {
            g_Console.Format( "Error: Game Disc Caching could not be initialized." );
        }
        else
        {
            dwErr = XFileCacheControlFiles( XFILECACHE_DONTCACHE, g_strNonCacheableFiles, _countof(g_strNonCacheableFiles) );

            assert( dwErr == ERROR_SUCCESS );

            // Setup callback that occurs when the background caching thread caches data
            DWORD dwFileCacheCallbackContextUnused = 0;
            XFileCacheSetFileIoCallbacks( FileCacheCallback, &dwFileCacheCallbackContextUnused );

            g_Console.Format( "Waiting for files to load into cache...\n" );

            // Preload files to be cached
            dwErr = XFileCachePreloadFiles( XFILECACHE_NORMAL_FILES, g_strCachedFiles, _countof(g_strCachedFiles) );

            assert( dwErr == ERROR_SUCCESS );

            // Do something else, other than reading files from the game disc
            Sleep( 5000 );
        }
    }

    if ( dwErr == ERROR_SUCCESS )
    {
        const DWORD dwMB = 1 * 1024 * 1024;
        const DWORD dwBufCount = dwMB;
        char* pBuf = new char[ dwBufCount ];
        size_t dwBytesRead;

        __int64 PerfFrequency, StartTime, EndTime;
        QueryPerformanceFrequency( ( LARGE_INTEGER* )&PerfFrequency );


        // Read and time non-cached files
        dwBytesRead = 0;
        QueryPerformanceCounter( ( LARGE_INTEGER* )&StartTime );
        for( INT i = 0; i < _countof(g_strNonCachedFiles); ++i )
        {
            FILE* fp = NULL;
            fopen_s( &fp, g_strNonCachedFiles[i], "r" );
            if( fp )
            {
                dwBytesRead += fread( pBuf, sizeof( pBuf[0] ), dwBufCount, fp );
                fclose( fp );
            }
        }
        QueryPerformanceCounter( ( LARGE_INTEGER* )&EndTime );

        FLOAT fTimeNonCached = ( FLOAT )( ( double )( EndTime - StartTime ) / ( double )PerfFrequency );
        g_Console.Format( "Time taken for non-cached files: %0.6f s.\n", fTimeNonCached );
        FLOAT fThroughputNonCached = dwBytesRead / ( dwMB * fTimeNonCached );
        g_Console.Format( "Throughput for non-cached files: %0.2f Mb/s.\n", fThroughputNonCached );

        // Read and time cached files
        dwBytesRead = 0;
        QueryPerformanceCounter( ( LARGE_INTEGER* )&StartTime );
        for( INT i = 0; i < _countof(g_strCachedFiles); ++i )
        {
            FILE* fp = NULL;
            fopen_s( &fp, g_strCachedFiles[i], "r" );
            if( fp )
            {
                dwBytesRead += fread( pBuf, sizeof( pBuf[0] ), dwBufCount, fp );
                fclose( fp );
            }
        }

        QueryPerformanceCounter( ( LARGE_INTEGER* )&EndTime );

        FLOAT fTimeCached = ( FLOAT )( ( double )( EndTime - StartTime ) / ( double )PerfFrequency );
        g_Console.Format( "Time taken for cached files: %0.6f s.\n", fTimeCached );
        FLOAT fThroughputCached = dwBytesRead / ( dwMB * fTimeCached );
        g_Console.Format( "Throughput for cached files: %0.2f Mb/s.\n", fThroughputCached );

        delete [] pBuf;
    }

    g_Console.Format( "\nPress LT + RT + RB to exit.\n" );

    // Wait for the user to press a button.
    for(; ; )
    {
        ATG::Input::GetMergedInput(); // Detect reboot keypress
    }
}

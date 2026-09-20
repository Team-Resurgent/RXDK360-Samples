//--------------------------------------------------------------------------------------
// OpenMP.cpp
//
// Show various simple examples of using the OpenMP directives and APIs
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include <xtl.h>
#include <assert.h>
#include <omp.h>
#include <algorithm>

#pragma warning( disable: 4127 )

#include <list>

#include "AtgConsole.h"
#include "AtgUtil.h"
#include "AtgInput.h"

// Disable warning C6993: Code analysis ignores OpenMP constructs; analyzing single-threaded code
// This warning occurs only in the Analysis configuration.
#pragma warning(disable: 6993)

#ifdef _OPENMP // When defined, the compiler is looking for #pragma omp directives
    #pragma message( "Compiled by an OpenMP-compliant implementation" )
#else
#pragma message( "Use the /openmp compiler switch and link with vcomp[d].lib" )
#endif

// Disable warning C6211: Leaking memory <pointer> due to an exception.
// We "know" that we won't run out of memory in this sample
#pragma warning ( disable : 6211 )

//--------------------------------------------------------------------------------------
// Globals
//--------------------------------------------------------------------------------------
ATG::Console    g_console;                 // console for output
const INT       MAX_ITERATIONS = 10000000;    // enough to reasonably exercise OpenMP    
DOUBLE*         a;                              // globals used by the simple examples
DOUBLE*         b;
DOUBLE*         y;
DOUBLE*         z;


//--------------------------------------------------------------------------------------
// Name: Init
// Desc: Set global arrays to known values
//--------------------------------------------------------------------------------------
static VOID Init()
{
    srand( 0 );
    for( int i = 0; i < MAX_ITERATIONS; ++i )
    {
        a[i] = DOUBLE( i );
        z[i] = DOUBLE( rand() );
    }
}


//--------------------------------------------------------------------------------------
// Emulate "interesting" calculations that could be done in parallel
//--------------------------------------------------------------------------------------
static VOID xaxis()
{
    Sleep( 100 );
}
static VOID yaxis()
{
    Sleep( 110 );
}
static VOID zaxis()
{
    Sleep( 120 );
}


//--------------------------------------------------------------------------------------
// Name: skip
// Desc: Emulate work that could be done if an object was locked
//--------------------------------------------------------------------------------------
static VOID skip( INT /*iThreadID*/ )
{
    Sleep( 1 );
}


//--------------------------------------------------------------------------------------
// Name: work
// Desc: Emulate work that could be done once an object is unlocked
//--------------------------------------------------------------------------------------
static VOID work( INT /*iThreadID*/ )
{
    Sleep( 10 );
}


//--------------------------------------------------------------------------------------
// Name: process
// Desc: Emulate work done on an object i
//--------------------------------------------------------------------------------------
static VOID process( INT /*i*/ )
{
    Sleep( 1 );
}


//--------------------------------------------------------------------------------------
// Name: OmpFunctions
// Desc: Test some OpenMP APIs
//--------------------------------------------------------------------------------------
VOID OmpFunctions()
{
    // Max threads available to OpenMP
    INT iMaxThreads = omp_get_max_threads();
    g_console.Format( "Maximum OpenMP threads: %d\n", iMaxThreads );

    // Set the number of threads to use in parallel regions
    omp_set_num_threads( iMaxThreads );

    // In serial regions, always 1. In parallel regions, whatever value it
    // has been set to previously.
    assert( omp_get_num_threads() == 1 );

    // Number of processors on the system
    INT iNumProcs = omp_get_num_procs();
    g_console.Format( "Number of system processors: %d\n", iNumProcs );

    // In serial regions, always 0
    assert( !omp_in_parallel() );

    g_console.Format( "\n" );
}


//--------------------------------------------------------------------------------------
// Name: SimpleForLoop
// Desc: Example A.1 from OpenMP V2.0 C++ API specification
//--------------------------------------------------------------------------------------
VOID SimpleForLoop()
{
    g_console.Format( "SimpleForLoop\n" );

    // Test serial version of loop for comparison
    Init();
    LARGE_INTEGER Start;
    LARGE_INTEGER End;
    QueryPerformanceCounter( &Start );

    for( int i = 1; i < MAX_ITERATIONS; ++i )
        b[i] = ( a[i] + a[i - 1] ) / 2.0;

    QueryPerformanceCounter( &End );
    __int64 SerialDuration = End.QuadPart - Start.QuadPart;
    g_console.Format( "Serial version: %I64d ticks\n", SerialDuration );

    // Test parallel version
    Init();
    QueryPerformanceCounter( &Start );

#pragma omp parallel for
    for( int i = 1; i < MAX_ITERATIONS; ++i )
        b[i] = ( a[i] + a[i - 1] ) / 2.0;

    QueryPerformanceCounter( &End );
    __int64 ParallelDuration = End.QuadPart - Start.QuadPart;
    g_console.Format( "Parallel version: %I64d ticks", ParallelDuration );
    g_console.Format( " (%fX faster)\n\n", ( DOUBLE )SerialDuration / ( DOUBLE )ParallelDuration );
}


//--------------------------------------------------------------------------------------
// Name: ParallelForWork
// Desc: Example A.4 from OpenMP V2.0 C++ API specification
//--------------------------------------------------------------------------------------
VOID ParallelForWork()
{
    g_console.Format( "ParallelForLoop\n" );

    // Test serial version of loop for comparison
    Init();
    LARGE_INTEGER Start;
    LARGE_INTEGER End;
    QueryPerformanceCounter( &Start );
    for( int i = 1; i < MAX_ITERATIONS; ++i )
        b[i] = ( a[i] + a[i - 1] ) / 2.0;
    for( int i = 0; i < MAX_ITERATIONS; ++i )
        y[i] = sqrt( z[i] );
    QueryPerformanceCounter( &End );
    __int64 SerialDuration = End.QuadPart - Start.QuadPart;
    g_console.Format( "Serial version: %I64d ticks\n", SerialDuration );

    // Test parallel version
    Init();
    QueryPerformanceCounter( &Start );
#pragma omp parallel
    {
        assert( omp_in_parallel() );
#pragma omp for nowait
        for( int i = 1; i < MAX_ITERATIONS; ++i )
            b[i] = ( a[i] + a[i - 1] ) / 2.0;
#pragma omp for nowait
        for( int i = 0; i < MAX_ITERATIONS; ++i )
            y[i] = sqrt( z[i] );
    }
    QueryPerformanceCounter( &End );
    __int64 ParallelDuration = End.QuadPart - Start.QuadPart;
    g_console.Format( "Parallel version: %I64d ticks", ParallelDuration );
    g_console.Format( " (%fX faster)\n\n", ( DOUBLE )SerialDuration / ( DOUBLE )ParallelDuration );
}


//--------------------------------------------------------------------------------------
// Name: ParallelSections
// Desc: Example A.8 from OpenMP V2.0 C++ API specification
//--------------------------------------------------------------------------------------
VOID ParallelSections()
{
    g_console.Format( "ParallelSections\n" );

    // Test serial version of loop for comparison
    LARGE_INTEGER Start;
    LARGE_INTEGER End;
    QueryPerformanceCounter( &Start );
    xaxis();
    yaxis();
    zaxis();
    QueryPerformanceCounter( &End );
    __int64 SerialDuration = End.QuadPart - Start.QuadPart;
    g_console.Format( "Serial version: %I64d ticks\n", SerialDuration );

    // Test parallel version
    QueryPerformanceCounter( &Start );
#pragma omp parallel sections
    {
#pragma omp section
        xaxis();
#pragma omp section
        yaxis();
#pragma omp section
        zaxis();
    }
    QueryPerformanceCounter( &End );
    __int64 ParallelDuration = End.QuadPart - Start.QuadPart;
    g_console.Format( "Parallel version: %I64d ticks", ParallelDuration );
    g_console.Format( " (%fX faster)\n\n", ( DOUBLE )SerialDuration / ( DOUBLE )ParallelDuration );
}


//--------------------------------------------------------------------------------------
// Name: OmpLocks
// Desc: Example A.16 from OpenMP V2.0 C++ API specification
//--------------------------------------------------------------------------------------
VOID OmpLocks()
{
    g_console.Format( "OmpLocks\n" );

    omp_lock_t lock;
    omp_init_lock( &lock );
    INT iThreadID;

#pragma omp parallel shared( lock ) private( iThreadID )
    {
        iThreadID = omp_get_thread_num();

        // Only one thread can output at a time. Can't use the console because only
        // one thread can use D3D.
        omp_set_lock( &lock );
        ATG::DebugSpew( "My thread ID is %d\n", iThreadID );
        omp_unset_lock( &lock );

        // Attempt to set lock. On failure, do skip(). On success, do work().
        while( !omp_test_lock( &lock ) )
        {
            skip( iThreadID );
        }
        work( iThreadID );
        omp_unset_lock( &lock );
    }

    omp_destroy_lock( &lock );
    g_console.Format( "\n" );
}


//--------------------------------------------------------------------------------------
// Name: ComputePi
// Desc: Use numerical integration to compute pi
//--------------------------------------------------------------------------------------
VOID ComputePi()
{
    g_console.Format( "ComputePi\n" );

    // Test serial version of loop for comparison
    LARGE_INTEGER Start;
    LARGE_INTEGER End;
    QueryPerformanceCounter( &Start );

    DOUBLE fStep = 1.0 / ( DOUBLE )MAX_ITERATIONS;
    DOUBLE fSum = 0.0;
    for( int i = 1; i < MAX_ITERATIONS; ++i )
    {
        DOUBLE x = fStep * ( ( DOUBLE )i - 0.5 );
        fSum += 4.0 / ( 1.0 + ( x * x ) );
    }
    DOUBLE pi = fStep * fSum;

    QueryPerformanceCounter( &End );
    __int64 SerialDuration = End.QuadPart - Start.QuadPart;
    g_console.Format( "Serial version: %I64d ticks (pi = %f)\n", SerialDuration, pi );

    // Test parallel version
    QueryPerformanceCounter( &Start );

    fSum = 0.0;
#pragma omp parallel for reduction( +:fSum )
    for( int i = 1; i < MAX_ITERATIONS; ++i )
    {
        DOUBLE x = fStep * ( ( DOUBLE )i - 0.5 );
        fSum += 4.0 / ( 1.0 + ( x * x ) );
    }
    pi = fStep * fSum;

    QueryPerformanceCounter( &End );
    __int64 ParallelDuration = End.QuadPart - Start.QuadPart;
    g_console.Format( "Parallel version: %I64d ticks (pi = %f)", ParallelDuration, pi );
    g_console.Format( " (%fX faster)\n\n", ( DOUBLE )SerialDuration / ( DOUBLE )ParallelDuration );
}


//--------------------------------------------------------------------------------------
// Name: SetCPUOrder
// Desc: Test xomp_set_cpu_order
//--------------------------------------------------------------------------------------
VOID SetCPUOrder()
{
    // Test example A.1 from OpenMP V2.0 C++ API specification using the
    // primary hardware thread on each CPU core. The SimpleForLoop function
    // itself is called on core 0, so the master thread will also be on core 0.
    xomp_cpu_order_t new_cpu_order = { 0 };
    new_cpu_order.order[0] = 2; // hardware thread 0, core 1
    new_cpu_order.order[1] = 4; // hardware thread 0, core 2
    new_cpu_order.order[2] = 0; // hardware thread 0, core 0
    new_cpu_order.order[3] = 2; // hardware thread 0, core 1
    new_cpu_order.order[4] = 4; // hardware thread 0, core 2
    // order[5] is unused when the maximum number of threads is set to 6 (the default).
    new_cpu_order.order[5] = 0;
    xomp_cpu_order_t old_cpu_order = xomp_set_cpu_order( new_cpu_order );

    g_console.Format( "SetCPUOrder : " );
    SimpleForLoop();

    // Restore original
    xomp_set_cpu_order( old_cpu_order );
}


//--------------------------------------------------------------------------------------
// Name: LinkedListWalk
// Desc: Sometimes there really is a faster way to walk linked lists in parallel.
//--------------------------------------------------------------------------------------
VOID LinkedListWalk()
{
    g_console.Format( "LinkedListWalk\n" );

    // Build a nice list
    typedef std::list <int> ListT;
    typedef ListT::iterator itr;
    ListT listA;
    for( int i = 0; i < MAX_ITERATIONS / 10000; ++i )
        listA.push_back( i );

    // Test serial version of loop for comparison
    LARGE_INTEGER Start;
    LARGE_INTEGER End;
    QueryPerformanceCounter( &Start );

    for( itr i = listA.begin(); i != listA.end(); ++i )
        process( *i );

    QueryPerformanceCounter( &End );
    __int64 SerialDuration = End.QuadPart - Start.QuadPart;
    g_console.Format( "Serial version: %I64d ticks\n", SerialDuration );

    // Test parallel version
    QueryPerformanceCounter( &Start );

#pragma omp parallel
    for( itr i = listA.begin(); i != listA.end(); ++i )
    {
#pragma omp single nowait
        process( *i );
    }

    QueryPerformanceCounter( &End );
    __int64 ParallelDuration = End.QuadPart - Start.QuadPart;
    g_console.Format( "Parallel version: %I64d ticks", ParallelDuration );
    g_console.Format( " (%fX faster)\n\n", ( DOUBLE )SerialDuration / ( DOUBLE )ParallelDuration );
}


//--------------------------------------------------------------------------------------
// Name: ParallelSort
// Desc: Sorting in parallel
//--------------------------------------------------------------------------------------
VOID ParallelSort()
{
    g_console.Format( "ParallelSort\n" );

    const int MAX_SORT = MAX_ITERATIONS / 100;

    // Test serial version for comparison
    LARGE_INTEGER Start;
    LARGE_INTEGER End;
    Init();
    QueryPerformanceCounter( &Start );

    std::sort( z, z + MAX_SORT );

    QueryPerformanceCounter( &End );
    __int64 SerialDuration = End.QuadPart - Start.QuadPart;
    g_console.Format( "Serial version: %I64d ticks\n", SerialDuration );

    // Test parallel version
    Init();

    QueryPerformanceCounter( &Start );

    const int m1 = MAX_SORT / 6;
    const int m2 = MAX_SORT / 3;
    const int m3 = MAX_SORT / 2;
    const int m4 = 2 * MAX_SORT / 3;
    const int m5 = 5 * MAX_SORT / 6;

    DOUBLE* zm1 = new DOUBLE [ m2 ];
    DOUBLE* zm2 = new DOUBLE [ m4 - m2 ];
    DOUBLE* zm3 = new DOUBLE [ MAX_SORT - m4 ];
    DOUBLE* zm4 = new DOUBLE [ m4 ];

    // Split array into six sections, sort each section in parallel,
    // then merge the results. Requires 2N memory.
#pragma omp parallel sections
    {
#pragma omp section
        std::sort( z, z + m1 );

#pragma omp section
        std::sort( z + m1, z + m2 );

#pragma omp section
        std::sort( z + m2, z + m3 );

#pragma omp section
        std::sort( z + m3, z + m4 );

#pragma omp section
        std::sort( z + m4, z + m5 );

#pragma omp section
        std::sort( z + m5, z + MAX_SORT );
    }

#pragma omp parallel sections
    {
        // Merge first 2 sections into zm1
#pragma omp section
        std::merge( z, z + m1, z + m1, z + m2, zm1 );

        // Merge second 2 sections into zm2
#pragma omp section
        std::merge( z + m2, z + m3, z + m3, z + m4, zm2 );

        // Merge last 2 sections into zm3
#pragma omp section
        std::merge( z + m4, z + m5, z + m5, z + MAX_SORT, zm3 );
    }

    // Merge zm1 and zm2 into zm4, then zm3 and zm4 back to z
    std::merge( zm1, zm1 + m2, zm2, zm2 + m4 - m2, zm4 );
    std::merge( zm4, zm4 + m4, zm3, zm3 + MAX_SORT - m4, z );

    QueryPerformanceCounter( &End );
    __int64 ParallelDuration = End.QuadPart - Start.QuadPart;
    g_console.Format( "Parallel version: %I64d ticks", ParallelDuration );
    g_console.Format( " (%fX faster)\n\n", ( DOUBLE )SerialDuration / ( DOUBLE )ParallelDuration );

    delete [] zm1;
    delete [] zm2;
    delete [] zm3;
    delete [] zm4;

    // Verify that sorting worked correctly
    for( int i = 0; i < MAX_SORT - 1; ++i )
        assert( z[i] <= z[i + 1] );
}


//--------------------------------------------------------------------------------------
// Name: main
// Desc: Entry point to the program
//--------------------------------------------------------------------------------------
VOID __cdecl main()
{
    // Initialize the console window
    g_console.Create( "game:\\Media\\Fonts\\Arial_12.xpr", 0xFF0000FF, 0xFFFFFFFF );

    // Allocate some big arrays
    a = new DOUBLE [ MAX_ITERATIONS ];
    b = new DOUBLE [ MAX_ITERATIONS ];
    y = new DOUBLE [ MAX_ITERATIONS ];
    z = new DOUBLE [ MAX_ITERATIONS ];

    OmpFunctions();      // Try out some typical OpenMP APIs
    SimpleForLoop();     // Example A.1 from OpenMP V2.0 C++ API specification
    ParallelForWork();   // Example A.4
    ParallelSections();  // Example A.8
    OmpLocks();          // Example A.16
    ComputePi();         // Parallelized version of numerical integration
    SetCPUOrder();       // Test xomp_set_cpu_order
    LinkedListWalk();    // Linked-list walker
    ParallelSort();      // Fast sort

    delete [] a;
    delete [] b;
    delete [] y;
    delete [] z;

    g_console.Format( "Press LT + RT + RB to exit\n" );
    for(; ; )
        ATG::Input::GetMergedInput(); // Detect reboot keypress
}

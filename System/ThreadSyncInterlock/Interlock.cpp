//--------------------------------------------------------------------------------------
// Interlock.cpp
//
// Demonstrates the use of the Interlock function.  
// The interlocked functions provide a mechanism for synchronizing access
// to a variable that is shared by multiple threads
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include <xtl.h>
#include <assert.h>


VOID __cdecl Print( const WCHAR* strFormat, ... );


//--------------------------------------------------------------------------------------
// Name: main
// Desc: Entry point to the program.
//--------------------------------------------------------------------------------------
VOID __cdecl main()
{
    // Simple reads and writes to properly-aligned native type variables (char, short
    // int, __int64, float, double, and __vector4) are atomic.
    // In other words, when one thread is updating a native type variable, you will
    // not end up with only one portion of the variable updated; 
    // all bits are updated in an atomic fashion. 
    // However, a read/write pair is not guaranteed to be atomic.
    // If two threads are reading and writing from the same variable, 
    // you cannot determine if one thread will perform its read operation before
    // the other performs its write operation.

    // The InterlockedIncrement function increments (increases by one) a LONG and
    // returns the resulting value.  The pointer passed as an argument must
    // be properly aligned.
    // The function prevents more than one thread from using the same variable
    // simultaneously.

    LONG lValue = 0;

    InterlockedIncrement( &lValue );

    Print( L"Incremented value is now: %ld", lValue );

    // Interlocked operations are often used to control access to other data,
    // by using them to implement locks. However, these functions do not guarantee
    // consistency of other memory. The Xbox 360 CPU can rearrange reads and writes
    // across Interlocked functions. 

    // This is different from Windows where these functions are full read/write barriers
    // to both the CPU and the processor. This can lead to subtle synchronization bugs.
    // In most cases the Interlocked operations must either be preceded or followed by
    // an __lwsync, in order to guarantee that these related memory accesses are performed
    // in the expected order. See the Interlocked function documentation for more details.

    // The InterlockedDecrement function decrements the value atomically
    // As with InterlockedIncrement, the pointer passed in must be properly
    // aligned
    InterlockedDecrement( &lValue );

    Print( L"Decremented value is now: %ld", lValue );


    // The InterlockedExchange function atomically exchanges a pair of values. 
    // The function prevents more than one thread from using the same variable simultaneously.
    // The function takes two arguments.  The first is a pointer to a properly aligned LONG,
    // that will receive the value of the second argument.  The function returns the
    // original value of the first argument

    LONG lOldValue = InterlockedExchange( &lValue, 10 ); // Atomically assign 10 to lValue

    Print( L"After InterlockedExchange, lValue is now: %ld.  It was: %ld", lValue, lOldValue );

    //
    // The InterlockedExchangeAdd function performs an atomic addition of an increment 
    // value to an addend variable. The function prevents more than one thread from using
    // the same variable simultaneously.  The return value is the initial value of the
    // variable pointed to by the first parameter.

    lOldValue = InterlockedExchangeAdd( &lValue, 2 );  // Atomically add 2 to lValue

    Print( L"After InterlockedExchangeAdd, lValue is now: %ld.  It was: %ld", lValue, lOldValue );

    // The InterlockedCompareExchange function performs an atomic comparison of the 
    // value pointed to by the first argument with the third argument. 
    // If they are equal,  the second argument 
    // is stored in the address specified by the first argument. 
    // Otherwise, no operation is performed.

    // Assign 0 to lValue, if lValue == 12
    lOldValue = InterlockedCompareExchange( &lValue, 0, 12 );

    Print( L"After InterlockedCompareExchange, lValue is now: %ld.  It was: %ld", lValue, lOldValue );

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

    OutputDebugStringW( L"\n*** ThreadSyncInterlock: " );
    OutputDebugStringW( strBuffer );
    OutputDebugStringW( L"\n\n" );
    ( VOID )iChars; // avoid compiler warning

    va_end( pArglist );
}

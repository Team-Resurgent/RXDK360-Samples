//--------------------------------------------------------------------------------------
// EncryptedAlloc.cpp
//
// This sample demonstrates the use of the encrypted memory allocation features 
// provided by XEncryptedAlloc and XEncryptedFree.
//
// XNA Developer Connection.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include <xtl.h>
#include <tracerecording.h>
#include <AtgConsole.h>
#include <AtgInput.h>
#include <AtgUtil.h>
#include <AtgInput.h>

//--------------------------------------------------------------------------------------
// Globals
//--------------------------------------------------------------------------------------

// Console for output
ATG::Console    g_Console;

// Page size for a block of encrypted memory
const DWORD     ENCRYPTED_PAGE_SIZE = 65536;

// Different sizes of encrypted memory to allocate
const DWORD     ENCRYPTED_MEMORY_4K_BLOCK = 4096;
const DWORD     ENCRYPTED_MEMORY_64K_BLOCK = 65536;

// Define a known string pattern
const CHAR*     STRING_PATTERN = "-> This text is in encrypted memory <-";

// Total encrypted memory is less than 16 MB so calc most 4K blocks we could alloc
const DWORD     MAX_4K_ENCRYPTED_MEMORY_BLOCKS = 16 * 1024 * 1024 / ENCRYPTED_MEMORY_4K_BLOCK;

// Name the cpu trace capture file, we would normally need to call DmMapDevkitDrive()
// to mount the E: drive.  However the call to XTraceStartRecording() below
// does this if necessary
const CHAR*     CAPTURE_FILE_NAME = "e:\\CaptureEncrypted.pix2";

// An array of pointers is needed to hold and track the number of allocated memory blocks
CHAR*           g_pEncryptedMemoryBlocks[MAX_4K_ENCRYPTED_MEMORY_BLOCKS];


//--------------------------------------------------------------------------------------
// Name: DetermineMaxEncryptedMemory()
// Desc: Allocate encrypted memory blocks of requested size and report count 
//--------------------------------------------------------------------------------------
DWORD DetermineMaxEncryptedMemory( DWORD dwAllocSize )
{
    DWORD dwBlockCount;

    // Allocate as much encrypted memory as possible in dwAllocSize units
    for( dwBlockCount = 0; dwBlockCount < MAX_4K_ENCRYPTED_MEMORY_BLOCKS; ++dwBlockCount )
    {
        g_pEncryptedMemoryBlocks[dwBlockCount] = ( CHAR* )XEncryptedAlloc( dwAllocSize );
        if( g_pEncryptedMemoryBlocks[dwBlockCount] == NULL )
        {
            break;
        }
    }

    for( DWORD i = 0; i < dwBlockCount; ++i )
    {
        XEncryptedFree( g_pEncryptedMemoryBlocks[i] );
    }

    g_Console.Format( "Allocated a max of %d ( %dK ) blocks of encrypted memory.\n",
                      dwBlockCount,
                      dwAllocSize / 1024 );

    g_Console.Format( "Total encrypted memory allocated via %dK blocks: %dK.\n",
                      dwAllocSize / 1024,
                      dwBlockCount * dwAllocSize / 1024 );

    return dwBlockCount;
}


//--------------------------------------------------------------------------------------
// Name: ReadAndWriteEncryptedMemory()
// Desc: Verify that encrypted memory data is accessible as plain text to this process
//--------------------------------------------------------------------------------------
VOID ReadAndWriteEncryptedMemory( CHAR* pEncryptedMemory )
{
    g_Console.Format( "\nTest Encrypted Read/Write Access\n" );
    g_Console.Format( "=============================\n" );

    g_Console.Format( "Writing known text to encrypted memory.\n" );
    sprintf_s( pEncryptedMemory, ENCRYPTED_MEMORY_4K_BLOCK, STRING_PATTERN );

    g_Console.Format( "Reading back and comparing text from encrypted memory.\n" );
    if( strcmp( pEncryptedMemory, STRING_PATTERN ) )
    {
        ATG::FatalError( "Error: encrypted memory does not match!" );
    }

    g_Console.Format( "The plain-text strings match.\n" );
}


//--------------------------------------------------------------------------------------
// Name: ReportMemoryPageSizeAndAlignment()
// Desc: Encrypted memory should be 64K aligned with a 64K page size
//--------------------------------------------------------------------------------------
VOID ReportMemoryPageSizeAndAlignment( CHAR* pEncryptedMemory )
{
    g_Console.Format( "Encrypted Memory Properties\n=========================\n" );

    // The encrypted memory returned is in multiples of 64K in size 
    DWORD dwPageSize = XMemGetPageSize( pEncryptedMemory );
    assert( dwPageSize == ENCRYPTED_PAGE_SIZE );
    g_Console.Format( "Encrypted memory page size is: %dK.\n", dwPageSize / 1024 );

    // The encrypted memory returned should be 64K aligned
    assert( ( ( INT )pEncryptedMemory & ( ENCRYPTED_PAGE_SIZE - 1 ) ) == false );
    g_Console.Format( "Encrypted memory alignment is: 64K.\n" );
}


//--------------------------------------------------------------------------------------
// Name: RecordTrace()
// Desc: Record a cpu instruction trace on the dev kit
//--------------------------------------------------------------------------------------
VOID RecordTrace()
{
    // This will mount the E: drive if needed
    XTraceStartRecording( CAPTURE_FILE_NAME );

    CHAR* pMemory64 = ( CHAR* )XEncryptedAlloc( ENCRYPTED_MEMORY_64K_BLOCK );
    CHAR* pMemory4 = ( CHAR* )XEncryptedAlloc( ENCRYPTED_MEMORY_4K_BLOCK );

    // Write to encrypted memory so that it will show up in the trace recording
    strcpy_s( pMemory64, ENCRYPTED_MEMORY_64K_BLOCK, STRING_PATTERN );
    strcpy_s( pMemory4, ENCRYPTED_MEMORY_4K_BLOCK, STRING_PATTERN );

    XEncryptedFree( pMemory64 );
    XEncryptedFree( pMemory4 );

    XTraceStopRecording();

    g_Console.Format( "\nCPU trace recorded to %s\n", CAPTURE_FILE_NAME );
}


//--------------------------------------------------------------------------------------
// Name: main()
// Desc: Entry point to the program
//--------------------------------------------------------------------------------------
INT __cdecl main()
{
    DWORD dwBlockCount4K;
    DWORD dwBlockCount64K;

    // Initialize the console window
    g_Console.Create( "game:\\Media\\Fonts\\Arial_16.xpr", 0xFF1F005F, 0xFFFFFFFF );
    g_Console.SendOutputToDebugChannel( TRUE );

    CHAR* pEncryptedMemory = ( CHAR* )XEncryptedAlloc( ENCRYPTED_MEMORY_4K_BLOCK );
    if( pEncryptedMemory == NULL )
    {
        ATG::FatalError( "Error: unable to allocate encrypted memory!" );
    }

    ReportMemoryPageSizeAndAlignment( pEncryptedMemory );
    ReadAndWriteEncryptedMemory( pEncryptedMemory );

    XEncryptedFree( pEncryptedMemory );

    // Allocate encrypted memory based on two different allocation sizes
    g_Console.Format( "\nEncrypted Memory Available\n========================\n" );
    dwBlockCount4K = DetermineMaxEncryptedMemory( ENCRYPTED_MEMORY_4K_BLOCK );

    g_Console.Format( "Free all encrypted memory and test again.\n\n" );

    dwBlockCount64K = DetermineMaxEncryptedMemory( ENCRYPTED_MEMORY_64K_BLOCK );

    // Based on a 64K page size these should be the same
    assert( dwBlockCount4K == dwBlockCount64K );

    g_Console.Format( "\nWhen finished hit LT-RT-RB to return to the Launcher.\n" );
    g_Console.Format( "Hit A button to record trace for PIX analysis.\n" );

    for(; ; )
    {
        ATG::GAMEPAD* pGamepad = ATG::Input::GetMergedInput();

        if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_A )
        {
            RecordTrace();

            g_Console.Format( "\nWhen finished hit LT-RT-RB to return to the Launcher.\n" );
            g_Console.Format( "Hit A button to record trace for PIX analysis.\n" );
        }
    }
}

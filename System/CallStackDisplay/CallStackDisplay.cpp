//--------------------------------------------------------------------------------------
// CallStackDisplay.cpp
//
// This sample illustrates how to do symbol lookup on your PC. Run the
// CallStackRecording sample first and copy the callstacks.dat file from the devkit
// to the CallStackDisplay directory. You can optionally copy the pdb file from the
// CallStackRecording sample to the CallStackDisplay directory.
//
// DbgHelp will be used to locate system symbols in the symbol server, and DIA2 will
// be used to do symbol loading and lookups.
//
// Microsoft Game Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include "stdafx.h"
#include "SymbolHelper.h"

#include <fstream>
#include <string>
#include <time.h>

//--------------------------------------------------------------------------------------
// Name of the file to load call stack information from.
//--------------------------------------------------------------------------------------
const CHAR* g_filename = "callstacks.dat";



#pragma warning(push)
#pragma warning(disable : 6384) // warning C6384: Dividing sizeof a pointer by another value

//--------------------------------------------------------------------------------------
// Name: ByteReverse
// Desc: Template function to byte-reverse in place any arbitrary block of data.
//--------------------------------------------------------------------------------------
template <typename T> void ByteReverse( T& data )
{
    BYTE* pData = ( BYTE* )&data;
    for( int i = 0; i < sizeof( data ) / 2; ++i )
    {
        std::swap( pData[i], pData[sizeof( data ) - 1 - i] );
    }
}

#pragma warning(pop)


//--------------------------------------------------------------------------------------
// Name: ReadFileOrAbort
// Desc: Read data from a file and abort the program if the requested data is not read.
//--------------------------------------------------------------------------------------
VOID ReadFileOrAbort(
    __in        HANDLE hFile,
    __out_bcount_part(nNumberOfBytesToRead, *lpNumberOfBytesRead) LPVOID lpBuffer,
    __in        DWORD nNumberOfBytesToRead,
    __out_opt   LPDWORD lpNumberOfBytesRead,
    __inout_opt LPOVERLAPPED lpOverlapped
    )
{
    // Make sure we have a bytes-read pointer.
    DWORD bytesRead;
    if( !lpNumberOfBytesRead )
        lpNumberOfBytesRead = &bytesRead;

    BOOL result = ReadFile( hFile, lpBuffer, nNumberOfBytesToRead, lpNumberOfBytesRead, lpOverlapped );

    if( !result || nNumberOfBytesToRead != *lpNumberOfBytesRead )
    {
        printf( "Error reading from file--aborting\n" );
        abort();
    }
}


//--------------------------------------------------------------------------------------
// Name: ProcessBinaryFile
// Desc: Process a file containing binary data describing the loaded modules and the
//       addresses to look up.
//--------------------------------------------------------------------------------------
VOID ProcessBinaryFile( HANDLE hFile, SymbolHelper& symbols )
{
    DWORD readCount = 0;

    // Loop through all of the module information, loading symbols for
    // each module.
    for(; ; )
    {
        // Each module is prefixed by a magic number, and the list of modules is
        // terminated with a DWORD zero.
        const DWORD modulePrefixID = 0xABABCDCD;
        // Read in the ID. If it is zero then we are at the end of the list of
        // modules.
        DWORD id = 0;
        ReadFileOrAbort( hFile, &id, sizeof( id ), &readCount, 0 );
        ByteReverse( id );
        if( id != modulePrefixID )
        {
            if( id != 0 )
            {
                printf( "Unexpected data in '%s'--aborting\n", g_filename );
                return;
            }

            // We've hit a modulePrefixID of zero, which tells us we have reached
            // the end of the list of modules, so we exit the loop.
            break;
        }

        // Read in the data that describes this module.

        // Read the module address and byte reverse it.
        VOID* baseAddress = 0;
        ReadFileOrAbort( hFile, &baseAddress, sizeof( baseAddress ), &readCount, 0 );
        ByteReverse( baseAddress );

        // Read the module size and byte reverse it.
        size_t size = 0;
        ReadFileOrAbort( hFile, &size, sizeof( size ), &readCount, 0 );
        ByteReverse( size );

        // Read the module timestamp and byte reverse it.
        DWORD timeStamp = 0;
        ReadFileOrAbort( hFile, &timeStamp, sizeof( timeStamp ), &readCount, 0 );
        ByteReverse( timeStamp );

        // Read the module name.
        CHAR name[MAX_PATH] = {0};
        ReadFileOrAbort( hFile, &name, sizeof( name ), &readCount, 0 );

        // Read the module PDB signature.
        DM_PDB_SIGNATURE signature = {0};
        ReadFileOrAbort( hFile, &signature, sizeof( signature ), &readCount, 0 );

        // Load the symbols for this module.
        bool result = symbols.LoadSymbolsForModule( baseAddress,
                                                    size, timeStamp, &signature );
        printf( "Symbols loaded %s for %s\n",
                result ? "successfully" : "unsuccessfully", name );
    }

    // Loop through all of the callstacks.
    for(; ; )
    {
        // Load the count of how many addresses follow.
        DWORD numEntries = 0;
        BOOL result = ReadFile( hFile, &numEntries, sizeof( numEntries ), &readCount, 0 );
        // When we reach the end of the file the read of numEntries will fail.
        // Elsewhere I omit checking for read failures, to simplify the code.
        if( !result || readCount != sizeof( numEntries ) )
            break;
        ByteReverse( numEntries );

        // Check for bogus values.
        if( numEntries > 100 )
            break;

        // Loop through doing symbol lookups on all of the addresses.
        printf( "\nCall stack with %u entries:\n", numEntries );
        for( DWORD i = 0; i < numEntries; ++i )
        {
            // Read the address and byte reverse it.
            DWORD address = 0;
            ReadFileOrAbort( hFile, &address, sizeof( address ), &readCount, 0 );
            ByteReverse( address );

            // Look up the symbol information for this address and print it.
            symbols.PrintSymbolSummary( address );
        }
    }
}


//--------------------------------------------------------------------------------------
// Name: ProcessTextFile
// Desc: Process a file containing text describing the loaded modules and the addresses
//       to look up.
//--------------------------------------------------------------------------------------
VOID ProcessTextFile( const CHAR* textFilename, SymbolHelper& symbols )
{
    // Open the data file for text parsing. ifstream is used because of its support
    // for easily and safely loading text line by line.
    std::ifstream iFile( textFilename );
    if( !iFile )
    {
        printf( "Can't open '%s'\n", textFilename );
        return;
    }

    for( std::string line; std::getline( iFile, line ); /**/ )
    {
        // Ignore empty lines and lines that start with a semicolon
        if( line.length() > 0 && line[0] != ';' )
        {
            if( line == "ModuleEnd" )
                break;

            // The first line in the series is the module name.
            std::string moduleName = line;

            // The next line contains the full path to the PDB file.
            std::string pdbName;
            std::getline( iFile, pdbName );

            // The third line contains the numbers that uniquely identify
            // the PDB file.
            std::string numbers;
            std::getline( iFile, numbers );

            // Parse the numbers on the last line.
            VOID* baseAddress = 0;
            size_t size = 0;
            DWORD timeStamp = 0;
            DM_PDB_SIGNATURE signature = {0};

            // The 'numbers' line describes the module address, size, timestamp,
            // PDB GUID, and PDB 'age'
            // A typical numbers line looks like this:
            // 82000000, 7077888, 458C1F76, {393A724D-577C-4DC5-B499-069DE207353D}, 11
            // address,  size,    timestamp, Data1,  Data2,Data3, ---Data4---
            int count = sscanf_s( numbers.c_str(), "%p, %x, %x, {%x-%x-%x-%x %x-%x %x %x %x %x %x}, %d",
                                  &baseAddress, &size, &timeStamp,
                                  &signature.Guid.Data1, &signature.Guid.Data2, &signature.Guid.Data3,
                                  &signature.Guid.Data4[0], &signature.Guid.Data4[1],
                                  &signature.Guid.Data4[2], &signature.Guid.Data4[3],
                                  &signature.Guid.Data4[4], &signature.Guid.Data4[5],
                                  &signature.Guid.Data4[6], &signature.Guid.Data4[7],
                                  &signature.Age );
            assert( count == 15 );
            // Avoid unused variable warnings in release builds.
            ( void )count;
            strcpy_s( signature.Path, pdbName.c_str() );
            signature.Path[ _countof( signature.Path ) - 1 ] = 0;

            // Load the symbols for this module.
            bool result = symbols.LoadSymbolsForModule( baseAddress,
                                                        size, timeStamp, &signature );

            // Convert the 32-bit link time stamp to a readable format.
            tm linkTime;
            localtime_s( &linkTime, ( __time32_t* )&timeStamp );
            char asciiTime[26];
            asctime_s( asciiTime, _countof( asciiTime ), &linkTime );

            // Print a summary of the symbol loading, including the link time
            // for the executable.
            printf( "Symbols loaded %s for %s, link time %s",
                    result ? "successfully" : "unsuccessfully",
                    moduleName.c_str(), asciiTime );
        }
    }

    // Loop through all of the callstacks.
    for( std::string line; std::getline( iFile, line ); /**/ )
    {
        // Ignore empty lines and lines that start with a semicolon
        if( line.length() > 0 && line[0] != ';' )
        {
            int numEntries = 0;
            int count = sscanf_s( line.c_str(), "%d entries:", &numEntries );
            if( count == 1 )
            {
                // Check for bogus values.
                if( numEntries > 100 )
                    break;

                // Loop through doing symbol lookups on all of the addresses.
                printf( "\nCall stack with %u entries:\n", numEntries );
                for( int i = 0; i < numEntries; ++i )
                {
                    DWORD address = 0;
                    std::getline( iFile, line );
                    sscanf_s( line.c_str(), "%p", &address );

                    // Look up the symbol information for this address and print it.
                    symbols.PrintSymbolSummary( address );
                }
            }
        }
    }
}


//--------------------------------------------------------------------------------------
// Name: Main
// Desc: Entry point for program
//--------------------------------------------------------------------------------------
int main( int/*argc*/, CHAR* /*argv[]*/)
{
    // Declare a symbol helper object. This should be a local variable rather than
    // a global variable to avoid problems caused by heavyweight construction going
    // on during process load.
    SymbolHelper symbols;

    // Open the data file containing module information and addresses.
    HANDLE hFile = CreateFile( g_filename, GENERIC_READ, FILE_SHARE_READ, NULL,
                               OPEN_EXISTING, 0, NULL );
    if( hFile == INVALID_HANDLE_VALUE )
    {
        printf( "Can't open '%s'\n", g_filename );
        return 0;
    }

    // Check and see if the file is a text or binary data file. The "BINR" comment
    // is placed at the beginning of all binary format files created by CallStackRecording.
    const CHAR* binaryID = "BINR";
    CHAR fileID[4] = {0};
    ReadFileOrAbort( hFile, &fileID, sizeof( fileID ), 0, 0 );

    // Process the file as binary or as text.
    if( memcmp( binaryID, &fileID, sizeof( fileID ) ) == 0 )
    {
        printf( "Processing binary file.\n" );
        ProcessBinaryFile( hFile, symbols );
    }
    else
    {
        printf( "Processing text file.\n" );
        ProcessTextFile( g_filename, symbols );
    }

    // Clean up and exit.
    CloseHandle( hFile );

    return 0;
}

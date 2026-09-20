//--------------------------------------------------------------------------------------
// SymbolHelper.cpp
//
// Microsoft Game Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include "StdAfx.h"
#include "SymbolHelper.h"
#include <xdevkit.h>
#include <string>
#include <dbghelp.h>

// We link with dbghelp.lib to make it convenient to call functions in dbghelp.dll,
// however we specify delay loading of dbghelp.dll so that we can load a specific
// version in LoadDbgHelp. This is crucial since otherwise we may get the version in
// the system directory, and then dbghelp.dll will not load symsrv.dll.
#pragma comment(lib, "dbghelp.lib")

// Name: LoadDbgHelp
// Desc: Forcibly loads DbgHelp.dll from %XEDK%\bin\win32. This is done by calling
//       LoadLibrary with a fully specified path, since otherwise the version in the
//       system directory will take precedence.
//--------------------------------------------------------------------------------------
BOOL LoadDbgHelp()
{
    // Get the XEDK environment variable.
    CHAR* xedkDir;
    size_t xedkDirSize;
    errno_t err = _dupenv_s( &xedkDir, &xedkDirSize, "xedk" );
    if( err || !xedkDir )
    {
        printf( "Couldn't read xedk environment variable.\n" );
        return FALSE;
    }

    // Create a fully specified path to the XEDK version of dbghelp.dll
    // This is necessary because symsrv.dll must be in the same directory
    // as dbghelp.dll, and only a fully specified path can guarantee which
    // version of dbghelp.dll we load.
    std::string dbgHelpPath = std::string( xedkDir ) + "\\bin\\win32\\dbghelp.dll";

    // Free xedkDir
    if( xedkDir )
        free( xedkDir );

    // Call LoadLibrary on DbgHelp.DLL with our fully specified path.
    HMODULE hDbgHelp = LoadLibrary( dbgHelpPath.c_str() );

    // Print an error message and return FALSE if DbgHelp.DLL didn't load.
    if( !hDbgHelp )
    {
        printf( "ERROR: Couldn't load DbgHelp.dll from %%xedk%%\\bin\\win32.\n" );
        return FALSE;
    }

    // DbgHelp.DLL loaded.
    return TRUE;
}


//--------------------------------------------------------------------------------------
// Name: cbSymbol
// Desc: DbgHelp callback which can be used for printing DbgHelp diagnostics.
//--------------------------------------------------------------------------------------
static BOOL CALLBACK cbSymbol( HANDLE/*hProcess*/, ULONG ActionCode, PVOID CallbackData, PVOID /*UserContext*/ )
{
    switch( ActionCode )
    {
        case CBA_DEBUG_INFO:
            printf( "%s", ( PSTR )CallbackData );
            break;

        default:
            return false;
    }

    return true;
}


//--------------------------------------------------------------------------------------
// Name: SymbolHelper constructor
// Desc: Do necessary setup for locating and loading symbols. This includes initializing
//       COM, ensuring that the necessary DLLs are in the correct location, initializing
//       DbgHelp and passing the XEDK symbol server path on to DbgHelp.
//--------------------------------------------------------------------------------------
SymbolHelper::SymbolHelper()
{
    // Initialize COM.
    if( FAILED( CoInitialize( NULL ) ) )
    {
        printf( "CoInitialize failed.\n" );
        exit( 10 );
    }

    // Load DbgHelp. This must be done before any DbgHelp functions are used,
    // and it only works if DbgHelp.dll is delay loaded.
    if( !LoadDbgHelp() )
        exit( 10 );

    // Most DbgHelp functions use a 'process' handle to identify their context.
    // This can be virtually any number, except zero.
    m_DebugProcess = ( HANDLE )1;

    // Enable DbgHelp debug messages, make sure that DbgHelp only loads symbols that
    // exactly match, and do deferred symbol loading for greater efficiency.
    SymSetOptions( SYMOPT_DEBUG | SYMOPT_EXACT_SYMBOLS | SYMOPT_DEFERRED_LOADS );

    // Create an XboxManager object to let us get the symbol server path.
    CComPtr <IXboxManager> spManager;
    HRESULT hr1 = spManager.CoCreateInstance( __uuidof( XboxManager ) );
    if( !SUCCEEDED( hr1 ) )
    {
        printf( "Couldn't create XboxManager object.\n" );
        exit( 10 );
    }

    // Get the XEDK symbol server path.
    CComBSTR bstrSymbolServerPath = NULL;
    hr1 = spManager->get_SystemSymbolServerPath( &bstrSymbolServerPath );
    if( !SUCCEEDED( hr1 ) )
    {
        printf( "Couldn't get XEDK symbol server path.\n" );
        exit( 10 );
    }

    // Convert the symbol server path from wide characters to char.
    char symbolServerPath[MAX_PATH];
    sprintf_s( symbolServerPath, "%S", bstrSymbolServerPath.m_str );

    // Now build up a complete symbol search path to give to DbgHelp.

    // Add the XEDK symbol server to the symbol search path.
    std::string fullSearchPath = symbolServerPath;

    CHAR* ntSymbolPath;
    size_t symbolPathSize;
    errno_t err = _dupenv_s( &ntSymbolPath, &symbolPathSize, "_NT_SYMBOL_PATH" );
    if( !err && ntSymbolPath )
    {
        fullSearchPath += ";" + std::string( ntSymbolPath );
        free( ntSymbolPath );
    }

    // Add the current directory to the search path.
    fullSearchPath += std::string( ";." );

    // Pass the symbol search path on to DbgHelp.
    SymInitialize( m_DebugProcess, const_cast<char*>( fullSearchPath.c_str() ), FALSE );

    // Set up a callback to help debug symbol loading problems. If symsrv.dll can't be loaded
    // then this will print a message to that effect.
    //SymRegisterCallback( m_DebugProcess, cbSymbol, NULL );
}


//--------------------------------------------------------------------------------------
// Name: SymbolHelper destructor
// Desc: Cleanup the symbol helper class.
//--------------------------------------------------------------------------------------
SymbolHelper::~SymbolHelper()
{
    // Free our DIA2 objects prior to shutting down COM.
    m_ModuleList.clear();

    // Shut down COM.
    CoUninitialize();
}


//--------------------------------------------------------------------------------------
// Name: PrintSymbolSummary
// Desc: Lookup the specified address in the previously loaded symbol files, and print
//       out a readable description of information about that address. The symbol
//       lookup will only succeed if the PDB files associated with the code modules are
//       still available.
//--------------------------------------------------------------------------------------
VOID SymbolHelper::PrintSymbolSummary( DWORD address )
{
    bool success = false;
    CHAR symbolName[1000] = {0};
    ULONG displacement = 0;
    CHAR filename[500] = {0};
    ULONG lineNumber = 0;

    // Scan through the list of loaded modules to find the one that contains the
    // requested address.
    for( size_t i = 0; i < m_ModuleList.size(); ++i )
    {
        // Find what module's address range the address falls in.
        if( address > m_ModuleList[i].m_Address &&
            address < m_ModuleList[i].m_Address + m_ModuleList[i].m_Size )
        {
            CComPtr <IDiaSession>& pSession = m_ModuleList[i].m_psession;

            // Find the symbol using the virtual address--the raw address. This
            // only works if you have previously told DIA where the module was
            // loaded, using put_loadAddress.
            // Specify SymTagPublicSymbol instead of SymTagFunction if you want
            // the full decorated names.
            IDiaSymbol* pFunc;
            HRESULT result = pSession->findSymbolByVA( address, SymTagFunction, &pFunc );
            if( SUCCEEDED( result ) && pFunc )
            {
                // Get the name of the function.
                CComBSTR functionName = 0;
                pFunc->get_name( &functionName );
                if( functionName )
                {
                    // Convert the function name from wide characters to char.
                    sprintf_s( symbolName, "%S", functionName.m_str );

                    // Get the offset of the address from the symbol's address.
                    ULONGLONG symbolBaseAddress;
                    pFunc->get_virtualAddress( &symbolBaseAddress );
                    displacement = address - ( ULONG )symbolBaseAddress;
                    success = true;

                    // Now try to get the filename and line number.
                    // Get an enumerator that corresponds to this instruction.
                    CComPtr <IDiaEnumLineNumbers> pLines;
                    const DWORD instructionSize = 4;
                    if( SUCCEEDED( pSession->findLinesByVA( address, instructionSize, &pLines ) ) )
                    {
                        // We could loop over all of the source lines that map to this instruction,
                        // but there is probably at most one, and if there are multiple source
                        // lines we still only want one.
                        CComPtr <IDiaLineNumber> pLine;
                        DWORD celt;
                        if( SUCCEEDED( pLines->Next( 1, &pLine, &celt ) ) && celt == 1 )
                        {
                            // Get the line number.
                            pLine->get_lineNumber( &lineNumber );

                            // Get the source file object, and then its name.
                            CComPtr <IDiaSourceFile> pSrc;
                            pLine->get_sourceFile( &pSrc );
                            CComBSTR sourceName = 0;
                            pSrc->get_fileName( &sourceName );
                            // Convert from wide characters to ASCII.
                            sprintf_s( filename, "%S", sourceName.m_str );
                        }
                    }
                }
            }
        }
    }

    if( success )
    {
        // Print out the symbol name and the offset of the address from
        // that symbol.
        printf( "    %8X: %s+%u", address, symbolName, displacement );

        // Now print out the filename/linenumber information if we have it.
        if( filename[0] )
        {
            // Get the filename part of the path.
            const CHAR* filepart = strrchr( filename, '\\' );
            if( filepart )
                ++filepart;
            else
                filepart = filename;

            printf( " - %s(%u)\n", filepart, lineNumber );
        }
        else
            printf( " - error getting source hFile/line number\n" );
    }
    else
        printf( "    %8X: Symbol lookup failed.\n", address );
}


//--------------------------------------------------------------------------------------
// Name: LoadSymbolsForModule
// Desc: Load the specified symbols using DbgHelp and DIA2. Return true for success.
//--------------------------------------------------------------------------------------
bool SymbolHelper::LoadSymbolsForModule( const VOID* baseAddress,
                                         size_t size, DWORD/*timeStamp*/, const DM_PDB_SIGNATURE* signature )
{
    // Create a DIA2 data source
    CComPtr <IDiaDataSource> pSource;
    HRESULT hr = CoCreateInstance( __uuidof( DiaSource ),
                                   NULL,
                                   CLSCTX_INPROC_SERVER,
                                   __uuidof( IDiaDataSource ),
                                   ( void** )&pSource );

    if( FAILED( hr ) )
        return false;

    const char* pdbPath = signature->Path;

    // Ask DbgHelp to look for the PDB file. Because DbgHelp has previously been told
    // to use the XEDK symbol server this should locate system debug information.
    // Note: this function should ignore files whose signature doesn't match, but
    // in reality it does not. Thus, if a PDB file with the correct name but wrong
    // signature is found first, it will stop searching.
    char resultPath[MAX_PATH];
    BOOL findResult = SymFindFileInPath( m_DebugProcess, 0,
                                         const_cast<char*>( pdbPath ),
                                         const_cast<GUID*>( &signature->Guid ), signature->Age, 0,
                                         SSRVOPT_GUIDPTR,
                                         resultPath,
                                         NULL,
                                         NULL );

    // If DbgHelp found the symbols then adjust our path.
    if( findResult )
        pdbPath = resultPath;

    // Convert the filename to wide characters for use with DIA2.
    wchar_t wPdbPath[ _MAX_PATH ];
    size_t convertedChars;
    mbstowcs_s( &convertedChars, wPdbPath, pdbPath, _TRUNCATE );

    // See if there is a PDB file at the specified location, and if so,
    // load it, checking the GUID and age to make sure that it is the correct
    // PDB file. Note that the timeStamp is not used anymore.
    hr = pSource->loadAndValidateDataFromPdb( wPdbPath,
                                              const_cast<GUID*>( &signature->Guid ), 0, signature->Age );
    if( FAILED( hr ) )
    {
        // Check the error code for details on why the load failed, which
        // could be because the file doesn't exist, signature doesn't match,
        // etc.
        return false;
    }

    // Create a session for the just loaded PDB file.
    CComPtr <IDiaSession> psession;
    if( FAILED( pSource->openSession( &psession ) ) )
    {
        return false;
    }

    // Tell DIA2 where the module was loaded.
    psession->put_loadAddress( ( ULONG_PTR )baseAddress );

    // Add this session to a list of loaded modules.
    m_ModuleList.push_back( Module( ( ULONG_PTR )baseAddress, size, psession ) );
    return true;
}

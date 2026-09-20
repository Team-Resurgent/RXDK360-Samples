//--------------------------------------------------------------------------------------
// SymbolHelper.h
//
// Microsoft Game Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#ifndef SYMBOLHELPER_H
#define SYMBOLHELPER_H

#include <vector>
#include <atlbase.h>
#include "dia2.h"


//--------------------------------------------------------------------------------------
// Name: class Sample
// Desc: Structure for keeping track of loaded symbols when using DIA2.
//       Each Module object records the memory range (start address and size) covered by
//       this code module, along with a DIA2 session object to use for symbol lookups.
//--------------------------------------------------------------------------------------
struct Module
{
    Module( ULONG_PTR address, size_t size, CComPtr <IDiaSession>& psession ) : m_Address( address ),
                                                                                m_Size( size ),
                                                                                m_psession( psession )
    {
    }

    ULONG_PTR m_Address;
    size_t m_Size;
    CComPtr <IDiaSession> m_psession;
};


//--------------------------------------------------------------------------------------
// Name: class SymbolHelper
// Desc: Helper class for hiding the details of finding and loading symbols. Finding
//       symbols is done with DbgHelp and symbol lookups are done with DIA2. Additional
//       symbol lookup functions can be added as needed.
//--------------------------------------------------------------------------------------
class SymbolHelper
{
public:
            SymbolHelper();
            ~SymbolHelper();

    // Print a summary of the specified symbol including the address, symbol name and
    // offset, and source file(line-number).
    VOID    PrintSymbolSummary( DWORD address );

    // Attempt to load the symbols for the specified module, including looking in the
    // XEDK symbol server. This function prints a success/failure message and returns
    // true for success.
    bool    LoadSymbolsForModule( const VOID* baseAddress,
                                  size_t size, DWORD timeStamp, const DM_PDB_SIGNATURE* signature );

private:
    // List of code modules (DLLs and EXEs) to look for symbols in.
    std::vector <Module> m_ModuleList;

    // Unique identifier for DbgHelp functions.
    HANDLE m_DebugProcess;
};

#endif

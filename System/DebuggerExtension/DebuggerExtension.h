//--------------------------------------------------------------------------------------
// DebuggerExtension.h
//
// Top level header file for the debugger extension; this should be included in all files
// used by the extension.
//
// XNA Developer Connection.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include <windows.h>

#ifndef __field_ecount_opt
#define __field_ecount_opt(a)
#endif

//
// Define KDEXT_64BIT to make all wdbgexts APIs recognize 64 bit addresses
// It is recommended for extensions to use 64 bit headers from wdbgexts so
// the extensions could support 64 bit targets.
//
#define KDEXT_64BIT
#include "wdbgexts.h"

// IMPORTANT:
// The header above is normally part of the Debugger Tools for Windows package.
// It is included locally in this project for convenience and to keep the dependencies to a minimum.
// However, for more complex extensions, the full Debugger Tools for Windows package might be needed (check sample documentation for details)

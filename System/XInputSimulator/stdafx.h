//-------------------------------------------------------------------------------------
// stdafx.h
//
// Include file for standard system include files
// or project specific include files that are used frequently, but
// are changed infrequently
//
// Microsoft XNA Game Platform Extensions Group
// Copyright (C) Microsoft Corporation. All rights reserved.
//-------------------------------------------------------------------------------------

#pragma once

#undef UNICODE

#ifndef WINVER
#define WINVER 0x0501
#endif

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0501
#endif

#ifndef _WIN32_WINDOWS
#define _WIN32_WINDOWS 0x0410
#endif

#ifndef _WIN32_IE
#define _WIN32_IE 0x0600
#endif

// Exclude rarely-used stuff from Windows headers
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

// Windows Header Files
#include <windows.h>
#include <richedit.h>
#include <commctrl.h>
#include <shellapi.h>

// C RunTime Header Files
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <malloc.h>
#include <memory.h>

// STL Header Files
#include <vector>
#include <string>
#include <map>

// Xbox SDK Header Files
#include <assert.h>

// XSim header
#include "XSim.h"

//--------------------------------------------------------------------------------------
// stdafx.h
// 
// Precompiled header file. Includes commonly used header files for faster compilation.
//
// Developed by Microsoft Advanced Technology Group.
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

#ifndef STDAFX_H_GUARD
#define STDAFX_H_GUARD

#ifdef _DEBUG
# define NODEFAULT   assert(0); __assume(0);
#else
# define NODEFAULT   __assume(0)
#endif

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0500
#endif

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif 

#include <windows.h>
#include <assert.h>
#include <stdio.h>
#include <tchar.h>
#include <xstudio.h>
#include <xedfile.h>

#endif //STDAFX_H_GUARD

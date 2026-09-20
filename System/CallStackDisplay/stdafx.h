// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

// The time stamps stored in executables are 32-bits, so we need to use
// the 32-bit version of time_t.
#define _USE_32BIT_TIME_T
#include <time.h>

#include <iostream>
#include <windows.h>
#include "xbdm.h"

#include <assert.h>

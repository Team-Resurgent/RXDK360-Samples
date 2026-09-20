//--------------------------------------------------------------------------------------
// Common.h
//
// Defines and functions shared between projects and files
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

#ifdef _XBOX
#include <xtl.h>
#else
#include <stdlib.h>
#include <WTypes.h>
#endif

namespace ATGGestureDetector
{

//--------------------------------------------------------------------------------------
// Defines
//--------------------------------------------------------------------------------------

// Define how many players should be supported
#define NUM_PLAYERS         (NUI_SKELETON_MAX_TRACKED_COUNT)

// We keep 3 frames of history in order to calculate velocity and acceleration
#define NUM_HISTORY_FRAMES  3

// Helper macros
#ifndef RETURN_ON_FAIL
#define RETURN_ON_FAIL( fn ) { HRESULT hr; if ( FAILED( hr = ( fn ) ) ) return hr; }
#endif

#ifndef RETURN_ON_NULL
#define RETURN_ON_NULL( x ) { if ( ( x ) == NULL ) return E_FAIL; }
#endif

#ifndef RETURN_ON_FILE_ERROR
#define RETURN_ON_FILE_ERROR( pFile )   if ( ferror( pFile) ) return E_FAIL;
#endif


//--------------------------------------------------------------------------------------
// Name: ByteSwap32Bit
// Desc: Byte swap a 32bit value
//--------------------------------------------------------------------------------------

template <typename T>
T ByteSwap32Bit( const T srcValue )
{
    UINT uValue = *((UINT*)&srcValue);
    UINT uSwappedValue = _byteswap_ulong( uValue );
    return *((T*)&uSwappedValue);
}


//--------------------------------------------------------------------------------------
// Name: MemoryReader
// Desc: Simple memory reader used to load files from memory location
//--------------------------------------------------------------------------------------

HRESULT mopen( VOID* pSource );
size_t mread( VOID* pDest, size_t elementSize, size_t count, VOID* pSource );
VOID mclose();

}
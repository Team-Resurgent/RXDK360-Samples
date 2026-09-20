//--------------------------------------------------------------------------------------
// File: VMXSkinning1.cpp
//
// Desc: Contains unoptimized VMX implementations of matrix palette skinning.
//       Prefer the optimized VMX version.
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "skinning.h"
#include "ppcintrinsics.h"
#include "xboxmath.h"


// This constant is used to create a permute mask for merging the data being written
// with the destination data.
#define SELECT_0         0x00000000
#define SELECT_1         0xFFFFFFFF
static CONST
XMVECTORI g_Select1110 =
{
    SELECT_1, SELECT_1, SELECT_1, SELECT_0
};

//--------------------------------------------------------------------------------------
// Name: RMWStoreFloat3
// Desc: RMWStoreFloat3 is one way of storing three floats to memory. It can handle
//       any memory alignment. It does this by reading from the destination,
//       merging the data to be written using permutes and selects, and then
//       storing the merged results. This Read-Modify-Write process is inefficient
//       when writing to non-cacheable memory and causes L1 cache pollution when
//       writing to cacheable memory, so it should be used with caution.
//--------------------------------------------------------------------------------------
XMFINLINE
XMFLOAT3* RMWStoreFloat3( XMFLOAT3* pDestination, XMVECTOR V )
{
    // Load the 16 bytes starting at address ((BYTE*)pDestination + 11) & ~0xF
    XMVECTOR V1 = __lvx( ( VOID* )pDestination, 11 );
    // Load the 16 bytes starting at address pDestination & ~0xF
    XMVECTOR V0 = __lvx( ( VOID* )pDestination, 0 );
    // Compute the permute control vector for shift right
    XMVECTOR Permute = __lvsr( ( VOID* )pDestination, 0 );
    XMVECTOR Zero = __vspltisw( 0 );
    // Create the high order mask
    XMVECTOR Select0 = __vperm( Zero, *( XMVECTOR* )&g_Select1110, Permute );
    // Create the low order mask
    XMVECTOR Select1 = __vperm( *( XMVECTOR* )&g_Select1110, Zero, Permute );
    // Right rotate the vector
    XMVECTOR S = __vperm( V, V, Permute );
    // Insert the low order data
    V1 = __vsel( V1, S, Select1 );
    // Insert the high order data
    V0 = __vsel( V0, S, Select0 );

    // Store the merged results back to memory.
    __stvx( V1, pDestination, 11 );
    __stvx( V0, pDestination, 0 );

    return pDestination;
}


// Set up some defines and then include the header file that contains the shared
// VMX skinning implementation.
#define VMX_SKINNING_VERSION 1
#define SkinVMXShared SkinVMX1
#include "VMXSharedSkinning.h" // Shared implementation.

//--------------------------------------------------------------------------------------
// File: Skinning.h
//
// Desc: Declarations for various software skinning routines.
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#ifndef SKINNING_H
#define SKINNING_H

#include <assert.h>
#include <xtl.h>


//--------------------------------------------------------------------------------------
// One SkinInfo struct per vertex
//--------------------------------------------------------------------------------------
#pragma pack(  push, 1 )
struct SkinInfo
{
    // The VMX128 unpack instruction will easily expand four bytes or four FLOAT16s
    // out to a vector of four floats. This will reduce memory uasge and memory bandwidth 
    // and can improve performance.
    BYTE    Weights[4];
    // Store the indices as BYTEs to conserve memory and bandwidth.
    // Storing the indices as BYTEs means it may not be possible to premultiply them
    // by the size of the matrices (depending on the size of the matrix palette), 
    // but saving bandwidth is more important than saving calculations.
    BYTE    Indices[4]; // palette index,  0 means no more weights
};
#pragma pack( pop )

// Specify how far ahead to prefetch when skinning.
const DWORD PrefetchDistance = 512;



//--------------------------------------------------------------------------------------
// Name: SkinC
// Desc: C matrix palette skinning routine
//--------------------------------------------------------------------------------------
VOID SkinC( const XMFLOAT3* pInData,
            DWORD dwNumVerts,
            const SkinInfo* pSkinInfo,
            const XMMATRIX* pPalette,
            DWORD dwNumPaletteMatrices, bool bPreCacheMatrices,
            XMFLOAT3* pOutData );


//--------------------------------------------------------------------------------------
// Name: PreCachePalette
// Desc: Moves a matrix palette into the L1 and L2 cache
//--------------------------------------------------------------------------------------
VOID PreCachePalette( const XMMATRIX* pPalette, DWORD dwNumPaletteMatrices );



//--------------------------------------------------------------------------------------
// Name: SkinVMX1
// Desc: VMX matrix palette skinning routine
//--------------------------------------------------------------------------------------
VOID SkinVMX1( const XMFLOAT3* pXMInData,
               DWORD dwNumVerts,
               const SkinInfo* pSkinInfo,
               const XMMATRIX* pPalette,
               DWORD dwNumPaletteMatrices, bool bPreCacheMatrices,
               XMFLOAT3* pXMOutData );



//--------------------------------------------------------------------------------------
// Name: SkinVMX2
// Desc: Better VMX matrix palette skinning routine
//       May write eight bytes too many to the destination.
//--------------------------------------------------------------------------------------
VOID SkinVMX2( const XMFLOAT3* pXMInData,
               DWORD dwNumVerts,
               const SkinInfo* pSkinInfo,
               const XMMATRIX* pPalette,
               DWORD dwNumPaletteMatrices, bool bPreCacheMatrices,
               XMFLOAT3* pXMOutData );



//--------------------------------------------------------------------------------------
// Name: SkinVMX3
// Desc: Best VMX matrix palette skinning routine
//       May write eight bytes too many to the destination.
//--------------------------------------------------------------------------------------
VOID SkinVMX3( const XMFLOAT3* pXMInData,
               DWORD dwNumVerts,
               const SkinInfo* pSkinInfo,
               const XMMATRIX* pPalette,
               DWORD dwNumPaletteMatrices, bool bPreCacheMatrices,
               XMFLOAT3* pXMOutData );



//--------------------------------------------------------------------------------------
// Name: SkinFloat16
// Desc: Skinning function that uses FLOAT16 variables for input and output, to reduce
//       memory bandwidth. This conserves memory, leaves more cache and bandwidth for
//       other thread, and may improve performance of this thread (if performance is
//       bandwidth bound).
//--------------------------------------------------------------------------------------
VOID SkinFloat16( const XMFLOAT3* pXMInData,    // Actually expects HALF*
                  DWORD dwNumVerts,
                  const SkinInfo* pSkinInfo,
                  const XMMATRIX* pPalette,
                  DWORD dwNumPaletteMatrices, bool bPreCacheMatrices,
                  XMFLOAT3* pXMOutData );           // Actually produces HALF*


//--------------------------------------------------------------------------------------
// Name: StvewxStoreFloat3
// Desc: This is an alternate way of storing three elements of a vector register to
//       memory. By using stvewx the read-modify-write required by
//       RMWStoreFloat3 is avoided. This method requires more
//       writes than gathering up a batch of XMFLOAT3s but is a good option
//       when gathering up multiple writes is impractical.
//--------------------------------------------------------------------------------------
__forceinline void StvewxStoreFloat3( XMFLOAT3* __restrict pOut, XMVECTOR V )
 {
    // __stvewx stores whichever word is at the destination offset. Therefore you need
    // to splat the desired word to all elements to make sure it is written properly.
    XMVECTOR x = XMVectorSplatX( V );
    XMVECTOR y = XMVectorSplatY( V );
    XMVECTOR z = XMVectorSplatZ( V );
    // Note that __stvewx requires 4-byte alignment of the destination.
    __stvewx( x, pOut, 0 );
    __stvewx( y, pOut, 4 );
    __stvewx( z, pOut, 8 );
}

#endif // SKINNING_H


//--------------------------------------------------------------------------------------
// Name: IsDisjointMemory
// Desc: This function returns TRUE if the two memory ranges are disjoint--non
//       overlapping. It is designed to be used in asserts that validate that the
//       __restrict keyword is being used legally. It returns TRUE if the two memory
//       regions are disjoint, and FALSE otherwise. This function could be implemented
//       as a template function taking a T* instead of a void*, in which case the size
//       parameter could be replaced with an element count instead of a byte count.
//--------------------------------------------------------------------------------------
inline BOOL IsDisjointMemory( const void* ptr1, size_t size1, const void* ptr2, size_t size2 )
{
    // If ptr1 ends before ptr2 then there is no overlap. If the ranges touch it is okay.
    if( ( ( BYTE* )ptr1 ) + size1 <= ptr2 )
        return TRUE;

    // If ptr1 starts after ptr2 ends then there is no overlap. If the ranges touch it is okay.
    if( ptr1 >= ( ( BYTE* )ptr2 ) + size2 )
        return TRUE;

    // Otherwise the two blocks must overlap so the blocks are not disjoint.
    return FALSE;
}


//--------------------------------------------------------------------------------------
// Name: SkinVMXShared
// Desc: VMX matrix palette skinning routine - implemented using the Xbox
//       Math Library. This code is shared with multiple versions to avoid code
//       duplication. #ifdefs are used to select between different versions of the
//       code. The function name is replaced using the preprocessor to facilitate
//       debugging and avoid duplicate symbols.
//--------------------------------------------------------------------------------------
VOID SkinVMXShared( const XMFLOAT3* __restrict pInData,
              DWORD dwNumVerts,
              const SkinInfo* __restrict pSkinInfo,
              const XMMATRIX* __restrict pPalette,
              DWORD dwNumPaletteMatrices, bool bPreCacheMatrices,
              XMFLOAT3* __restrict pOutData )
 {
    assert( pInData != NULL && pSkinInfo != NULL && dwNumVerts != 0 );
    assert( pPalette != NULL && dwNumPaletteMatrices != 0 );
    assert( pOutData != NULL );

    // Make sure the in and out pointers don't overlap - the __restrict modifier
    // requires this. It is also illegal for pSkinInfo or pPalette to overlap,
    // but we don't explicitly check for that.
#if VMX_SKINNING_VERSION == 4
    assert( IsDisjointMemory( pOutData, dwNumVerts * sizeof(pOutData[0]),
                            pInData, dwNumVerts * sizeof(pInData[0]) ) );
#else
    assert( IsDisjointMemory( pOutData, dwNumVerts * 2 * sizeof( pOutData[0] ),
                            pInData, dwNumVerts * 2 * sizeof( pInData[0] ) ) );
#endif

    // palette must be 16 byte aligned
    assert( ( DWORD( pPalette ) & 0xF ) == 0 );

    // skininfo must be 16 byte aligned
    assert( ( DWORD( pSkinInfo ) & 0xF ) == 0 );

    // pOutData must be 16 byte aligned
    assert( ( DWORD( pOutData ) & 0xF ) == 0 );

    // If we aren't going to read the out data then it should be in non-cached
    // write-combined memory. This avoids wasting L2 cache space and L2 bandwidth.
    // If the output data will be read by the CPU in the near future then it should
    // be written to cacheable memory. If the data is written to cacheable memory
    // then __dcbz128 should be used to zero the memory without causing fetching.
    // This is made easier if the output data is 128 byte aligned.

    // precache matrix palette if requested
    if( bPreCacheMatrices )
 {
        PreCachePalette( pPalette, dwNumPaletteMatrices );
    }

    // Prefetch the input data and skin info to hide memory latency.
    // This will potentially prefetch too much data - additional checks
    // for the end of the arrays could prevent this in order to conserve
    // bandwidth.
    for( DWORD i = 0; i < PrefetchDistance; i += 128 )
        __dcbt( i, pInData );
    for( DWORD i = 0; i < PrefetchDistance; i += 128 )
        __dcbt( i, pSkinInfo );

#if VMX_SKINNING_VERSION == 2
    // Variables to cache the last result, for more efficient writing of
    // data. These variables are zeroed just to stop the compiler from
    // warning about possibly uninitialized variables.
    XMVECTOR LastV = __vzero();
    XMVECTOR LastN = __vzero();
#endif

    for( DWORD i = 0; i < dwNumVerts; ++i )
 {
    // As we loop through the vertices, continue prefetching.
    // The extra instructions to check for prefetching too far are often
    // free, because skinning code is bound by memory bandwidth and vector
    // math performance. Therefore the checks are often worthwhile, since they
    // will avoid fetching a total of PrefetchDistance * 2 bytes of
    // pointless data, which should improve the performance of other code.
    // In this particular case they slightly hurt performance, since there
    // is no other code to benefit from the extra cache space.
        if( ( dwNumVerts - i ) > PrefetchDistance / sizeof( pSkinInfo[0] ) )
 {
            __dcbt( PrefetchDistance, pSkinInfo + i );
            if( ( dwNumVerts - i ) > PrefetchDistance / sizeof( pInData[0] ) )
                __dcbt( PrefetchDistance, pInData );
        }

    // Unpack the weights as normalized floats:  x --> (FLOAT) (x / 255.0f)
        XMVECTOR Weights = XMLoadUByteN4( ( XMUBYTEN4* )pSkinInfo[i].Weights );

#if VMX_SKINNING_VERSION == 4
        // Load an XMFLOAT3 that contains six FLOAT16 values.
        XMVECTOR packed = XMLoadFloat3( pInData + 0 );

        // Unpack the second batch of four floats (vupkd3d unpacks from
        // the least significant bits, those at the highest address).
        // Only two of these will actually be used.
        XMVECTOR temp = __vupkd3d( packed, VPACK_FLOAT16_4 );

        // Rotate the packed register eight bytes so the first four
        // floats can be unpacked.
        packed = __vsldoi( packed, packed, 8 );

        // Unpack the first four floats. Three of these are the vertex position
        // and they are now in the correct variable.
        XMVECTOR V = __vupkd3d( packed, VPACK_FLOAT16_4 );

        // Take the 32-bytes of unpacked data and shift it down 12
        // bytes to properly align the second XMFLOAT3 of data, the normal.
        // This instruction appends V and temp to produce 32-bytes of data,
        // and then shifts this data down twelve bytes.
        XMVECTOR N = __vsldoi( V, temp, 12 );
#else
    // Load the vertex to be transformed into an XMVECTOR variable, which should go
    // in a VMX register. All calculations should be done on XMVECTOR variables
    // because they map directly to the hardware.
        XMVECTOR V = XMLoadFloat3( pInData + 0 );
    // Load the normal also.
    // The transformation of the normal will be scheduled by the compiler to
    // happen in parallel with the vertex transform.
        XMVECTOR N = XMLoadFloat3( pInData + 1 );
#endif



    // Load the matrices that will be used and stash them in registers (XMMATRIX
    // variables should go into VMX registers). If there is sufficient coherency
    // of matrix sets then these loaded matrices could be retained and used for
    // multiple vertices. A single __int64 check could test whether any of the
    // indices have changed. This optimization would avoid loading matrices so
    // frequently, but would limit code scheduling opportunities.
        XMMATRIX Matrix0;
        Matrix0 = XMLoadFloat4x4A16( ( XMFLOAT4X4A16* )( pPalette + pSkinInfo[i].Indices[0] ) );
    // Break the four element Weights vector into four separate vectors, suitable
    // for use in XMVectorMultiplyAdd operations.
        XMVECTOR Weight0 = XMVectorSplatX( Weights );

    // Transform the vector and normal by the palette matrix * weight
        XMVECTOR TempV = XMVector3Transform( V, Matrix0 );
        XMVECTOR OutDataV = XMVectorMultiply( TempV, Weight0 );

        XMVECTOR TempN = XMVector3TransformNormal( N, Matrix0 );
        XMVECTOR OutDataN = XMVectorMultiply( TempN, Weight0 );



    // The scalar version of the code exits early if no more matrices are used.
    // But conditional branches lower performance by interrupting the pipeline.
    // More importantly, they inhibit the compiler's ability to schedule code to hide
    // latency. Therefore, all four matrices are always applied, even if some
    // of them have zero weights. Because most of the calculation can happen
    // in parallel (due to the pipelining of the transforms) the four matrices
    // don't take much longer to apply than does one matrix. The Alpha hardware
    // will show less pipelining due to its having fewer registers.

    // Some additional optimization would be possible if vertices are sorted
    // according to how many bones affect them.

    // Load the next matrix and weight.
        Matrix0 = XMLoadFloat4x4A16( ( XMFLOAT4X4A16* )( pPalette + pSkinInfo[i].Indices[1] ) );
        Weight0 = XMVectorSplatY( Weights );

    // Transform the vector and normal by the palette matrix * weight
        TempV = XMVector3Transform( V, Matrix0 );
        OutDataV = XMVectorMultiplyAdd( TempV, Weight0, OutDataV );

        TempN = XMVector3TransformNormal( N, Matrix0 );
        OutDataN = XMVectorMultiplyAdd( TempN, Weight0, OutDataN );



    // Load the next matrix and weight.
        Matrix0 = XMLoadFloat4x4A16( ( XMFLOAT4X4A16* )( pPalette + pSkinInfo[i].Indices[2] ) );
        Weight0 = XMVectorSplatZ( Weights );

    // Transform the vector and normal by the palette matrix * weight
        TempV = XMVector3Transform( V, Matrix0 );
        OutDataV = XMVectorMultiplyAdd( TempV, Weight0, OutDataV );

        TempN = XMVector3TransformNormal( N, Matrix0 );
        OutDataN = XMVectorMultiplyAdd( TempN, Weight0, OutDataN );



    // Load the next matrix and weight.
        Matrix0 = XMLoadFloat4x4A16( ( XMFLOAT4X4A16* )( pPalette + pSkinInfo[i].Indices[3] ) );
        Weight0 = XMVectorSplatW( Weights );

    // Transform the vector and normal by the palette matrix * weight
        TempV = XMVector3Transform( V, Matrix0 );
        OutDataV = XMVectorMultiplyAdd( TempV, Weight0, OutDataV );

        TempN = XMVector3TransformNormal( N, Matrix0 );
        OutDataN = XMVectorMultiplyAdd( TempN, Weight0, OutDataN );


#if VMX_SKINNING_VERSION == 1
        // Storing misaligned vectors or storing partial vectors is very expensive.
        // Code to do a read-modify-write is required, costing several instructions
        // and adding many stalls. This code will be optimized in VMXSkin2.
        // If writing to uncached memory then RMWStoreFloat3 is *extremely* slow
        // because of the uncached reads. If writing to cached memory then
        // RMWStoreFloat3 is a bit slow because it causes the destination data to
        // be read into the L1 cache, evicting useful data.
        // Be wary of using RMWStoreFloat3 or any other misaligned/partial store
        // functions.

        // Store the result vector.
        RMWStoreFloat3( pOutData + 0, OutDataV );

        // Store the result normal.
        RMWStoreFloat3( pOutData + 1, OutDataN );

        pOutData += 2;
#elif VMX_SKINNING_VERSION == 2
        // Storing misaligned vectors or storing partial vectors is very expensive.
        // Code to do a read-modify-write is required, costing several instructions,
        // adding many stalls, and either requiring uncached reads, or polluting the
        // L1 cache with destination data.
        // On the first pass we just stash our results for later. On the second pass
        // we write out four XMFLOAT3s at a time. We repeat this process with every
        // pair of vertices, for vastly improved write performance.
        if( (i & 1) == 0 )
        {
            // Cache the results until later, so they can be written out efficiently
            LastV = OutDataV;
            LastN = OutDataN;
        }
        else
        {
            // LastV, LastN, OutDataV, and OutDataN each contain three floats of
            // valid data. We want to pack these twelve floats into three vectors
            // and then store the three vectors. This is more efficient than doing
            // partial vector stores.
            // In each of the four vectors it is the first three floats (x, y, and z)
            // that contain the valid data.
            // The rearrangements could be done using vperm, but vperm has hidden
            // costs associated with it which make it undesirable for this purpose.
            // In order to use vperm we have to load a permute constant, which takes
            // several instructions. If the vectors being permuted are not in
            // the first 32 VMX registers then the permute vector must be one of the
            // first 8 VMX registers.
            // Therefore, whenever possible we should try to use alternatives, such
            // as vsldoi, vpermwi, vrlimi, vmrghw, vmrglw, vsplt, etc.
            // If there is a single instruction replacement for vperm then it is always
            // worthwhile. If it takes two instructions then it is usually beneficial,
            // but only testing will show for sure.

            // Merge the x-component of LastN into the w component of LastV
            // A mask of 1 selects the 'w' component for insertion, and a shift amount
            // of 1 shifts the source 'x' component into the 'w' position.
            // vrlimi is most efficient when the destination and the first input are
            // the same register.
            LastV = __vrlimi( LastV, LastN, 1, 1 );

            // Now we need to shift the y, z components of LastN to x, y
            // and then insert the x, y components of OutDataV into the z, w
            // slots. First we shift LastN left four bytes.
            LastN = __vsldoi( LastN, LastN, 4 );
            // Then we rotate OutDataV left two slots and insert the x, y
            // components into z, w.
            LastN = __vrlimi( LastN, OutDataV, 3, 2 );

            // Finally we need to shift the z component of OutDataV into the x
            // slot, and then shift the x, y, z components of OutDataN into
            // the y, z, w slots, and insert them into OutDataV. First we
            // splat the x component of OutDataV to all components, although
            // we only need to move it to x.
            OutDataV = __vspltw( OutDataV, 2 );
            // Then we rotate OutDataN left three slots (equivalent to rotating
            // right one) and insert into y, z, and w.
            OutDataV = __vrlimi( OutDataV, OutDataN, 7, 3 );

            // Now store the three result vectors, containing four XMFLOAT3s
            __stvx( LastV, pOutData, 0 );
            __stvx( LastN, pOutData, 16 );
            __stvx( OutDataV, pOutData, 32 );

            // Skip ahead by four XMFLOAT3s
            pOutData += 4;
        }
#elif VMX_SKINNING_VERSION == 3
        // StvewxStoreFloat3 allows storing three elements of a vector without a
        // read-modify-write operation.

        // Store the result vector.
        StvewxStoreFloat3( pOutData + 0, OutDataV );

        // Store the result normal.
        StvewxStoreFloat3( pOutData + 1, OutDataN );

        pOutData += 2;
#elif VMX_SKINNING_VERSION == 4
        // We need to pack the 24 bytes of float data to be stored into twelve bytes
        // of FLOAT16 data, and then store that. Before packing the data we need to
        // make the bytes to be stored contiguous. We need to move the x-coordinate
        // of OutDataN into the w-coordinate of OutDataV, and we need to shift the
        // y and z-coordinates of OutDataN into the x and y-coordinates.
        // First, mask insert OutDataN's x-coordinate into OutDataV's w-coordinate.
        // The mask of 1 specifies the w-coordinate, and the shift amount of 1
        // word wraps x around to w.
        // The mask bits for insert are: x->8, y->4, z->2, w->1
        packed = __vrlimi( OutDataV, OutDataN, 1, 1 );

        // Shift y and z down to x and y. __vsldoi is one of several good choices
        // for this task because it takes an immediate shift amount.
        OutDataN = __vsldoi( OutDataN, OutDataN, 4 );

        // Pack four floats into 8-bytes of V, shifting them up to the most-significant
        // bits (so they will be stored first in memory).
        packed = __vpkd3d( packed, packed, VPACK_FLOAT16_4, VPACK_64LO, 2 );
        // Pack in four more floats (only two used) and mask insert them in the least
        // significant bits (stored later in memory).
        packed = __vpkd3d( packed, OutDataN, VPACK_FLOAT16_4, VPACK_64LO, 0 );

        // Store three floats--which actually contain six FLOAT16s.
        StvewxStoreFloat3( pOutData, packed );

        // When writing FLOAT16 data we only move through 12 bytes of data each iteration.
        pOutData += 1;
#else
#error  // Version not specified.
#endif

    // Move to the next input vertex.
#if VMX_SKINNING_VERSION == 4
        // When processing FLOAT16 data we only move through 12 bytes of data each iteration.
        pInData += 1;
#else
        pInData += 2;
#endif
    }
}

//---------------------------------------------------------------------------------------------------------
// FastUntileCPU.cpp
//
// XNA Developer Connection
// Copyright ( C ) Microsoft Corporation. All rights reserved.
//---------------------------------------------------------------------------------------------------------

#include <assert.h>
#include <PPCIntrinsics.h>

#include <xtl.h>
#include <xgraphics.h>

#include <AtgUtil.h>

#include "FastUntile.h"
#include "FastUntileCPU.h"

//---------------------------------------------------------------------------------------------------------
// Global compile-time constants
//---------------------------------------------------------------------------------------------------------
static const UINT g_iCacheLineSize = 128;               // L2 cache
static const UINT g_iStoreGatherQueueSize = 64;         // size of write-combining store gather buffers  


//---------------------------------------------------------------------------------------------------------
// Helper functions.  
//---------------------------------------------------------------------------------------------------------
// Copy operation based on the largest register type
template <unsigned int t_bytes, typename t_dest, typename t_source> 
static inline VOID CopyViaRegister( t_dest* __restrict dest, const t_source* __restrict source )
{
    switch( t_bytes )
    {
    case 8:
        {
            unsigned __int64 ulTemp = __loaddoublewordbytereverse( 0, source );
            __storevolatiledoublewordbytereverse( ulTemp, 0, dest );
        }
        break;
    case 16:
        {
            __vector4 vTemp = __lvx( source, 0 );
            __stvx_volatile( vTemp, dest, 0 );  // volatile to prevent compiler re-ordering of write-combined writes
        }
        break;

    default:
        assert( FALSE );
        break;
    }
}

template <typename t_type> static inline BOOL AlignedToPowerOf2( const t_type& t, DWORD dwPowerOf2 )
{
    return ( ( ( DWORD )t ) & ( dwPowerOf2 - 1 ) ) == 0;
}

template <typename t_type> static inline t_type RoundDownToPowerOf2( const t_type& t, DWORD dwPowerOf2 )
{
    return ( t_type )( ( ( DWORD )t ) & ~( dwPowerOf2 - 1 ) );
}

template <typename t_type> static inline t_type RoundUpToPowerOf2( const t_type& t, DWORD dwPowerOf2 )
{
    return ( t_type )( ( ( ( DWORD )t ) + ( dwPowerOf2 - 1 ) ) & ~( dwPowerOf2 - 1 ) );
}


//---------------------------------------------------------------------------------------------------------
// Name: Prefetch( )
// Desc: Kick data into the cache.  
//---------------------------------------------------------------------------------------------------------
static void Prefetch( const BYTE* pData, DWORD dwSize )
{
    // Expand input range to be cache-line aligned:
    const BYTE* pEnd = pData + dwSize;
    pData = RoundDownToPowerOf2( pData, g_iCacheLineSize );
    dwSize = RoundUpToPowerOf2( pEnd, g_iCacheLineSize ) - pData;

    for( size_t i = 0; i < dwSize; i += g_iCacheLineSize )
    {
        __dcbt( i, pData );
    }
}


//---------------------------------------------------------------------------------------------------------
// Name: PreZero( )
// Desc: Initialize the cache to zero.  
//---------------------------------------------------------------------------------------------------------
static void PreZero( BYTE* pData, DWORD dwSize )
{
    // For correctness, the inputs cannot share cache lines with other data.
    assert( AlignedToPowerOf2( pData, g_iCacheLineSize ) );
    assert( AlignedToPowerOf2( dwSize, g_iCacheLineSize ) );

    for( size_t i = 0; i < dwSize; i += g_iCacheLineSize )
    {
        __dcbz128( i, pData );
    }
}


//---------------------------------------------------------------------------------------------------------
// Name: PrimeCacheForNextRepeatBlock( )
// Desc: For the specified texture block, prefetch all the source texels.  Also, if the destination is
// cacheable, pre-zero all the destination texels.
//---------------------------------------------------------------------------------------------------------
static __forceinline VOID PrimeCacheForNextRepeatBlock( 
    const BYTE* pTileSourceData, 
    BYTE* pTileDestData, 
    UINT iRepeatBlockSizeInBytes, 
    UINT iRepeatBlockHeight, 
    UINT iPreZeroSize, 
    UINT iDestRowPitchInBytes, 
    BOOL bDestCacheable )
{
    Prefetch( pTileSourceData, iRepeatBlockSizeInBytes );
    if( bDestCacheable )
    {
        assert( XGNextMultiple( (UINT) pTileDestData, g_iCacheLineSize ) == (UINT) pTileDestData );

        // Destination data for a tile is spread out over different memory locations
        BYTE* pTexelRowDestData = pTileDestData;
        for( UINT i = 0; i < iRepeatBlockHeight; ++i )
        {
            PreZero( pTexelRowDestData, iPreZeroSize );
            pTexelRowDestData += iDestRowPitchInBytes;
        }
    }
}


//---------------------------------------------------------------------------------------------------------
// Name: UntileSingleRepeatBlock( )
// Desc: Untile a single repeat block worth of data.  This function is templatized off of the texel size, 
// otherwise the inner loop will have either an integer mul or an sraw (shift by variable amount), 
// both of which incur penalties for non-pipelined instructions.
//
// The function is also templatized off of the move size, so we can determine whether to move data
// via VMX registers or via int64 registers.
//---------------------------------------------------------------------------------------------------------
template <UINT t_Log2TexelSizeInBytes, UINT t_MoveSizeInBytes>
static __forceinline VOID UntileSingleRepeatBlock( 
    const BYTE* __restrict pSourceData, 
    BYTE* __restrict pDestData, 
    const WORD* __restrict pLookupTable, 
    UINT iRepeatBlockWidth, 
    UINT iRepeatBlockHeight, 
    UINT iDestRowPitchInBytes )
{
    UINT iTexelSizeInBytes = 1 << t_Log2TexelSizeInBytes;
    UINT iTexelsPerMove = t_MoveSizeInBytes / iTexelSizeInBytes;
    assert( t_MoveSizeInBytes % iTexelSizeInBytes == 0 );    // should be true for all texture formats
    assert( ( iRepeatBlockWidth * iTexelSizeInBytes ) % t_MoveSizeInBytes == 0 );    // should be true for all texture formats

    UINT iMoveAlignment;
    GetMoveAlignment( 1 << t_Log2TexelSizeInBytes, &iMoveAlignment );
    ATG_Unused( iMoveAlignment );   // avert compile warning about unused local variable
    assert( iMoveAlignment == t_MoveSizeInBytes );

    UINT iTexelSourceIndex = 0;
    BYTE* pTexelRowDestData = pDestData;

    for( UINT iTexelY = 0; iTexelY < iRepeatBlockHeight; ++iTexelY )
    {
        BYTE* pTexelDestData = pTexelRowDestData;

        switch( t_Log2TexelSizeInBytes )
        {
#ifdef _DEBUG
        default:
            // This is what the normal code would look like, but unrolling the loop manually below gives 
            // somewhat better performance.
            for( UINT iTexelX = 0; iTexelX < iRepeatBlockWidth; iTexelX += iTexelsPerMove )
            {
                const BYTE* pTexelSourceData = pSourceData + ( pLookupTable[ iTexelSourceIndex ] << t_Log2TexelSizeInBytes );

                CopyViaRegister<t_MoveSizeInBytes>( pTexelDestData, pTexelSourceData );

                iTexelSourceIndex += iTexelsPerMove;
                pTexelDestData += t_MoveSizeInBytes;
            }
            break;

#else

        case 0:
        case 3:
            // Effectively a template specialization for the case of 1-byte texels, which means 8 texels per __int64 
            // and 128 texels per repeat block row --> 16 __int64's per repeat block row.
            // Also a template specialization for the case of 8-byte texels, which means 2 texels per __vector4 
            // and 32 texels per repeat block row --> 16 __vector4's per repeat block row.
            {
                const BYTE* pTexelSourceData;

                pTexelSourceData = pSourceData + ( pLookupTable[ iTexelSourceIndex ] << t_Log2TexelSizeInBytes );
                CopyViaRegister<t_MoveSizeInBytes>( pTexelDestData, pTexelSourceData );
                iTexelSourceIndex += iTexelsPerMove;
                pTexelDestData += t_MoveSizeInBytes;

                pTexelSourceData = pSourceData + ( pLookupTable[ iTexelSourceIndex ] << t_Log2TexelSizeInBytes );
                CopyViaRegister<t_MoveSizeInBytes>( pTexelDestData, pTexelSourceData );
                iTexelSourceIndex += iTexelsPerMove;
                pTexelDestData += t_MoveSizeInBytes;

                pTexelSourceData = pSourceData + ( pLookupTable[ iTexelSourceIndex ] << t_Log2TexelSizeInBytes );
                CopyViaRegister<t_MoveSizeInBytes>( pTexelDestData, pTexelSourceData );
                iTexelSourceIndex += iTexelsPerMove;
                pTexelDestData += t_MoveSizeInBytes;

                pTexelSourceData = pSourceData + ( pLookupTable[ iTexelSourceIndex ] << t_Log2TexelSizeInBytes );
                CopyViaRegister<t_MoveSizeInBytes>( pTexelDestData, pTexelSourceData );
                iTexelSourceIndex += iTexelsPerMove;
                pTexelDestData += t_MoveSizeInBytes;

                pTexelSourceData = pSourceData + ( pLookupTable[ iTexelSourceIndex ] << t_Log2TexelSizeInBytes );
                CopyViaRegister<t_MoveSizeInBytes>( pTexelDestData, pTexelSourceData );
                iTexelSourceIndex += iTexelsPerMove;
                pTexelDestData += t_MoveSizeInBytes;

                pTexelSourceData = pSourceData + ( pLookupTable[ iTexelSourceIndex ] << t_Log2TexelSizeInBytes );
                CopyViaRegister<t_MoveSizeInBytes>( pTexelDestData, pTexelSourceData );
                iTexelSourceIndex += iTexelsPerMove;
                pTexelDestData += t_MoveSizeInBytes;

                pTexelSourceData = pSourceData + ( pLookupTable[ iTexelSourceIndex ] << t_Log2TexelSizeInBytes );
                CopyViaRegister<t_MoveSizeInBytes>( pTexelDestData, pTexelSourceData );
                iTexelSourceIndex += iTexelsPerMove;
                pTexelDestData += t_MoveSizeInBytes;

                pTexelSourceData = pSourceData + ( pLookupTable[ iTexelSourceIndex ] << t_Log2TexelSizeInBytes );
                CopyViaRegister<t_MoveSizeInBytes>( pTexelDestData, pTexelSourceData );
                iTexelSourceIndex += iTexelsPerMove;
                pTexelDestData += t_MoveSizeInBytes;

                pTexelSourceData = pSourceData + ( pLookupTable[ iTexelSourceIndex ] << t_Log2TexelSizeInBytes );
                CopyViaRegister<t_MoveSizeInBytes>( pTexelDestData, pTexelSourceData );
                iTexelSourceIndex += iTexelsPerMove;
                pTexelDestData += t_MoveSizeInBytes;

                pTexelSourceData = pSourceData + ( pLookupTable[ iTexelSourceIndex ] << t_Log2TexelSizeInBytes );
                CopyViaRegister<t_MoveSizeInBytes>( pTexelDestData, pTexelSourceData );
                iTexelSourceIndex += iTexelsPerMove;
                pTexelDestData += t_MoveSizeInBytes;

                pTexelSourceData = pSourceData + ( pLookupTable[ iTexelSourceIndex ] << t_Log2TexelSizeInBytes );
                CopyViaRegister<t_MoveSizeInBytes>( pTexelDestData, pTexelSourceData );
                iTexelSourceIndex += iTexelsPerMove;
                pTexelDestData += t_MoveSizeInBytes;

                pTexelSourceData = pSourceData + ( pLookupTable[ iTexelSourceIndex ] << t_Log2TexelSizeInBytes );
                CopyViaRegister<t_MoveSizeInBytes>( pTexelDestData, pTexelSourceData );
                iTexelSourceIndex += iTexelsPerMove;
                pTexelDestData += t_MoveSizeInBytes;

                pTexelSourceData = pSourceData + ( pLookupTable[ iTexelSourceIndex ] << t_Log2TexelSizeInBytes );
                CopyViaRegister<t_MoveSizeInBytes>( pTexelDestData, pTexelSourceData );
                iTexelSourceIndex += iTexelsPerMove;
                pTexelDestData += t_MoveSizeInBytes;

                pTexelSourceData = pSourceData + ( pLookupTable[ iTexelSourceIndex ] << t_Log2TexelSizeInBytes );
                CopyViaRegister<t_MoveSizeInBytes>( pTexelDestData, pTexelSourceData );
                iTexelSourceIndex += iTexelsPerMove;
                pTexelDestData += t_MoveSizeInBytes;

                pTexelSourceData = pSourceData + ( pLookupTable[ iTexelSourceIndex ] << t_Log2TexelSizeInBytes );
                CopyViaRegister<t_MoveSizeInBytes>( pTexelDestData, pTexelSourceData );
                iTexelSourceIndex += iTexelsPerMove;
                pTexelDestData += t_MoveSizeInBytes;

                pTexelSourceData = pSourceData + ( pLookupTable[ iTexelSourceIndex ] << t_Log2TexelSizeInBytes );
                CopyViaRegister<t_MoveSizeInBytes>( pTexelDestData, pTexelSourceData );
                iTexelSourceIndex += iTexelsPerMove;
                pTexelDestData += t_MoveSizeInBytes;
            }
            break;

        case 1:
        case 2:
            // Effectively a template specialization for the case of 4-byte texels, which means 4 texels per __vector4 
            // and 32 texels per repeat block row --> 8 __vector4's per repeat block row.
            // Also a template specialization for the case of 2-byte texels, which means 8 texels per __vector4 
            // and 64 texels per repeat block row --> 8 __vector4's per repeat block row.
            //
            // In Release configuration, this code compiles to 32 instructions, or 4 instructions per block.
            {
                const BYTE* pTexelSourceData;

                pTexelSourceData = pSourceData + ( pLookupTable[ iTexelSourceIndex ] << t_Log2TexelSizeInBytes );
                CopyViaRegister<t_MoveSizeInBytes>( pTexelDestData, pTexelSourceData );
                iTexelSourceIndex += iTexelsPerMove;
                pTexelDestData += t_MoveSizeInBytes;

                pTexelSourceData = pSourceData + ( pLookupTable[ iTexelSourceIndex ] << t_Log2TexelSizeInBytes );
                CopyViaRegister<t_MoveSizeInBytes>( pTexelDestData, pTexelSourceData );
                iTexelSourceIndex += iTexelsPerMove;
                pTexelDestData += t_MoveSizeInBytes;

                pTexelSourceData = pSourceData + ( pLookupTable[ iTexelSourceIndex ] << t_Log2TexelSizeInBytes );
                CopyViaRegister<t_MoveSizeInBytes>( pTexelDestData, pTexelSourceData );
                iTexelSourceIndex += iTexelsPerMove;
                pTexelDestData += t_MoveSizeInBytes;

                pTexelSourceData = pSourceData + ( pLookupTable[ iTexelSourceIndex ] << t_Log2TexelSizeInBytes );
                CopyViaRegister<t_MoveSizeInBytes>( pTexelDestData, pTexelSourceData );
                iTexelSourceIndex += iTexelsPerMove;
                pTexelDestData += t_MoveSizeInBytes;

                pTexelSourceData = pSourceData + ( pLookupTable[ iTexelSourceIndex ] << t_Log2TexelSizeInBytes );
                CopyViaRegister<t_MoveSizeInBytes>( pTexelDestData, pTexelSourceData );
                iTexelSourceIndex += iTexelsPerMove;
                pTexelDestData += t_MoveSizeInBytes;

                pTexelSourceData = pSourceData + ( pLookupTable[ iTexelSourceIndex ] << t_Log2TexelSizeInBytes );
                CopyViaRegister<t_MoveSizeInBytes>( pTexelDestData, pTexelSourceData );
                iTexelSourceIndex += iTexelsPerMove;
                pTexelDestData += t_MoveSizeInBytes;

                pTexelSourceData = pSourceData + ( pLookupTable[ iTexelSourceIndex ] << t_Log2TexelSizeInBytes );
                CopyViaRegister<t_MoveSizeInBytes>( pTexelDestData, pTexelSourceData );
                iTexelSourceIndex += iTexelsPerMove;
                pTexelDestData += t_MoveSizeInBytes;

                pTexelSourceData = pSourceData + ( pLookupTable[ iTexelSourceIndex ] << t_Log2TexelSizeInBytes );
                CopyViaRegister<t_MoveSizeInBytes>( pTexelDestData, pTexelSourceData );
                iTexelSourceIndex += iTexelsPerMove;
                pTexelDestData += t_MoveSizeInBytes;
            }
            break;

        case 4:
            // Effective a template specialization for the case of 16-byte texels, which means 1 texels per __vector4 
            // and 32 texels per repeat block row --> 32 __vector4's per repeat block row.
            {
                const BYTE* pTexelSourceData;

                pTexelSourceData = pSourceData + ( pLookupTable[ iTexelSourceIndex ] << t_Log2TexelSizeInBytes );
                CopyViaRegister<t_MoveSizeInBytes>( pTexelDestData, pTexelSourceData );
                iTexelSourceIndex += iTexelsPerMove;
                pTexelDestData += t_MoveSizeInBytes;

                pTexelSourceData = pSourceData + ( pLookupTable[ iTexelSourceIndex ] << t_Log2TexelSizeInBytes );
                CopyViaRegister<t_MoveSizeInBytes>( pTexelDestData, pTexelSourceData );
                iTexelSourceIndex += iTexelsPerMove;
                pTexelDestData += t_MoveSizeInBytes;

                pTexelSourceData = pSourceData + ( pLookupTable[ iTexelSourceIndex ] << t_Log2TexelSizeInBytes );
                CopyViaRegister<t_MoveSizeInBytes>( pTexelDestData, pTexelSourceData );
                iTexelSourceIndex += iTexelsPerMove;
                pTexelDestData += t_MoveSizeInBytes;

                pTexelSourceData = pSourceData + ( pLookupTable[ iTexelSourceIndex ] << t_Log2TexelSizeInBytes );
                CopyViaRegister<t_MoveSizeInBytes>( pTexelDestData, pTexelSourceData );
                iTexelSourceIndex += iTexelsPerMove;
                pTexelDestData += t_MoveSizeInBytes;

                pTexelSourceData = pSourceData + ( pLookupTable[ iTexelSourceIndex ] << t_Log2TexelSizeInBytes );
                CopyViaRegister<t_MoveSizeInBytes>( pTexelDestData, pTexelSourceData );
                iTexelSourceIndex += iTexelsPerMove;
                pTexelDestData += t_MoveSizeInBytes;

                pTexelSourceData = pSourceData + ( pLookupTable[ iTexelSourceIndex ] << t_Log2TexelSizeInBytes );
                CopyViaRegister<t_MoveSizeInBytes>( pTexelDestData, pTexelSourceData );
                iTexelSourceIndex += iTexelsPerMove;
                pTexelDestData += t_MoveSizeInBytes;

                pTexelSourceData = pSourceData + ( pLookupTable[ iTexelSourceIndex ] << t_Log2TexelSizeInBytes );
                CopyViaRegister<t_MoveSizeInBytes>( pTexelDestData, pTexelSourceData );
                iTexelSourceIndex += iTexelsPerMove;
                pTexelDestData += t_MoveSizeInBytes;

                pTexelSourceData = pSourceData + ( pLookupTable[ iTexelSourceIndex ] << t_Log2TexelSizeInBytes );
                CopyViaRegister<t_MoveSizeInBytes>( pTexelDestData, pTexelSourceData );
                iTexelSourceIndex += iTexelsPerMove;
                pTexelDestData += t_MoveSizeInBytes;

                pTexelSourceData = pSourceData + ( pLookupTable[ iTexelSourceIndex ] << t_Log2TexelSizeInBytes );
                CopyViaRegister<t_MoveSizeInBytes>( pTexelDestData, pTexelSourceData );
                iTexelSourceIndex += iTexelsPerMove;
                pTexelDestData += t_MoveSizeInBytes;

                pTexelSourceData = pSourceData + ( pLookupTable[ iTexelSourceIndex ] << t_Log2TexelSizeInBytes );
                CopyViaRegister<t_MoveSizeInBytes>( pTexelDestData, pTexelSourceData );
                iTexelSourceIndex += iTexelsPerMove;
                pTexelDestData += t_MoveSizeInBytes;

                pTexelSourceData = pSourceData + ( pLookupTable[ iTexelSourceIndex ] << t_Log2TexelSizeInBytes );
                CopyViaRegister<t_MoveSizeInBytes>( pTexelDestData, pTexelSourceData );
                iTexelSourceIndex += iTexelsPerMove;
                pTexelDestData += t_MoveSizeInBytes;

                pTexelSourceData = pSourceData + ( pLookupTable[ iTexelSourceIndex ] << t_Log2TexelSizeInBytes );
                CopyViaRegister<t_MoveSizeInBytes>( pTexelDestData, pTexelSourceData );
                iTexelSourceIndex += iTexelsPerMove;
                pTexelDestData += t_MoveSizeInBytes;

                pTexelSourceData = pSourceData + ( pLookupTable[ iTexelSourceIndex ] << t_Log2TexelSizeInBytes );
                CopyViaRegister<t_MoveSizeInBytes>( pTexelDestData, pTexelSourceData );
                iTexelSourceIndex += iTexelsPerMove;
                pTexelDestData += t_MoveSizeInBytes;

                pTexelSourceData = pSourceData + ( pLookupTable[ iTexelSourceIndex ] << t_Log2TexelSizeInBytes );
                CopyViaRegister<t_MoveSizeInBytes>( pTexelDestData, pTexelSourceData );
                iTexelSourceIndex += iTexelsPerMove;
                pTexelDestData += t_MoveSizeInBytes;

                pTexelSourceData = pSourceData + ( pLookupTable[ iTexelSourceIndex ] << t_Log2TexelSizeInBytes );
                CopyViaRegister<t_MoveSizeInBytes>( pTexelDestData, pTexelSourceData );
                iTexelSourceIndex += iTexelsPerMove;
                pTexelDestData += t_MoveSizeInBytes;

                pTexelSourceData = pSourceData + ( pLookupTable[ iTexelSourceIndex ] << t_Log2TexelSizeInBytes );
                CopyViaRegister<t_MoveSizeInBytes>( pTexelDestData, pTexelSourceData );
                iTexelSourceIndex += iTexelsPerMove;
                pTexelDestData += t_MoveSizeInBytes;

                pTexelSourceData = pSourceData + ( pLookupTable[ iTexelSourceIndex ] << t_Log2TexelSizeInBytes );
                CopyViaRegister<t_MoveSizeInBytes>( pTexelDestData, pTexelSourceData );
                iTexelSourceIndex += iTexelsPerMove;
                pTexelDestData += t_MoveSizeInBytes;

                pTexelSourceData = pSourceData + ( pLookupTable[ iTexelSourceIndex ] << t_Log2TexelSizeInBytes );
                CopyViaRegister<t_MoveSizeInBytes>( pTexelDestData, pTexelSourceData );
                iTexelSourceIndex += iTexelsPerMove;
                pTexelDestData += t_MoveSizeInBytes;

                pTexelSourceData = pSourceData + ( pLookupTable[ iTexelSourceIndex ] << t_Log2TexelSizeInBytes );
                CopyViaRegister<t_MoveSizeInBytes>( pTexelDestData, pTexelSourceData );
                iTexelSourceIndex += iTexelsPerMove;
                pTexelDestData += t_MoveSizeInBytes;

                pTexelSourceData = pSourceData + ( pLookupTable[ iTexelSourceIndex ] << t_Log2TexelSizeInBytes );
                CopyViaRegister<t_MoveSizeInBytes>( pTexelDestData, pTexelSourceData );
                iTexelSourceIndex += iTexelsPerMove;
                pTexelDestData += t_MoveSizeInBytes;

                pTexelSourceData = pSourceData + ( pLookupTable[ iTexelSourceIndex ] << t_Log2TexelSizeInBytes );
                CopyViaRegister<t_MoveSizeInBytes>( pTexelDestData, pTexelSourceData );
                iTexelSourceIndex += iTexelsPerMove;
                pTexelDestData += t_MoveSizeInBytes;

                pTexelSourceData = pSourceData + ( pLookupTable[ iTexelSourceIndex ] << t_Log2TexelSizeInBytes );
                CopyViaRegister<t_MoveSizeInBytes>( pTexelDestData, pTexelSourceData );
                iTexelSourceIndex += iTexelsPerMove;
                pTexelDestData += t_MoveSizeInBytes;

                pTexelSourceData = pSourceData + ( pLookupTable[ iTexelSourceIndex ] << t_Log2TexelSizeInBytes );
                CopyViaRegister<t_MoveSizeInBytes>( pTexelDestData, pTexelSourceData );
                iTexelSourceIndex += iTexelsPerMove;
                pTexelDestData += t_MoveSizeInBytes;

                pTexelSourceData = pSourceData + ( pLookupTable[ iTexelSourceIndex ] << t_Log2TexelSizeInBytes );
                CopyViaRegister<t_MoveSizeInBytes>( pTexelDestData, pTexelSourceData );
                iTexelSourceIndex += iTexelsPerMove;
                pTexelDestData += t_MoveSizeInBytes;

                pTexelSourceData = pSourceData + ( pLookupTable[ iTexelSourceIndex ] << t_Log2TexelSizeInBytes );
                CopyViaRegister<t_MoveSizeInBytes>( pTexelDestData, pTexelSourceData );
                iTexelSourceIndex += iTexelsPerMove;
                pTexelDestData += t_MoveSizeInBytes;

                pTexelSourceData = pSourceData + ( pLookupTable[ iTexelSourceIndex ] << t_Log2TexelSizeInBytes );
                CopyViaRegister<t_MoveSizeInBytes>( pTexelDestData, pTexelSourceData );
                iTexelSourceIndex += iTexelsPerMove;
                pTexelDestData += t_MoveSizeInBytes;

                pTexelSourceData = pSourceData + ( pLookupTable[ iTexelSourceIndex ] << t_Log2TexelSizeInBytes );
                CopyViaRegister<t_MoveSizeInBytes>( pTexelDestData, pTexelSourceData );
                iTexelSourceIndex += iTexelsPerMove;
                pTexelDestData += t_MoveSizeInBytes;

                pTexelSourceData = pSourceData + ( pLookupTable[ iTexelSourceIndex ] << t_Log2TexelSizeInBytes );
                CopyViaRegister<t_MoveSizeInBytes>( pTexelDestData, pTexelSourceData );
                iTexelSourceIndex += iTexelsPerMove;
                pTexelDestData += t_MoveSizeInBytes;

                pTexelSourceData = pSourceData + ( pLookupTable[ iTexelSourceIndex ] << t_Log2TexelSizeInBytes );
                CopyViaRegister<t_MoveSizeInBytes>( pTexelDestData, pTexelSourceData );
                iTexelSourceIndex += iTexelsPerMove;
                pTexelDestData += t_MoveSizeInBytes;

                pTexelSourceData = pSourceData + ( pLookupTable[ iTexelSourceIndex ] << t_Log2TexelSizeInBytes );
                CopyViaRegister<t_MoveSizeInBytes>( pTexelDestData, pTexelSourceData );
                iTexelSourceIndex += iTexelsPerMove;
                pTexelDestData += t_MoveSizeInBytes;

                pTexelSourceData = pSourceData + ( pLookupTable[ iTexelSourceIndex ] << t_Log2TexelSizeInBytes );
                CopyViaRegister<t_MoveSizeInBytes>( pTexelDestData, pTexelSourceData );
                iTexelSourceIndex += iTexelsPerMove;
                pTexelDestData += t_MoveSizeInBytes;

                pTexelSourceData = pSourceData + ( pLookupTable[ iTexelSourceIndex ] << t_Log2TexelSizeInBytes );
                CopyViaRegister<t_MoveSizeInBytes>( pTexelDestData, pTexelSourceData );
                iTexelSourceIndex += iTexelsPerMove;
                pTexelDestData += t_MoveSizeInBytes;
            }
            break;
#endif
#pragma warning(push)
#pragma warning(disable:4065)   // switch statement contains only default label
        }
#pragma warning(pop)

        pTexelRowDestData += iDestRowPitchInBytes;
    }
}


//---------------------------------------------------------------------------------------------------------
// Name: CPUUntiler::InitializeRemapping( )
// Desc: Initialize the linear-to-tiled lookup array.
//---------------------------------------------------------------------------------------------------------
VOID CPUUntiler::InitializeRemapping( UINT iTexelPitch )
{
    GetRepeatBlockDimensions( iTexelPitch, &m_iRepeatBlockWidth, &m_iRepeatBlockHeight );
    assert( _countof( m_pLinearToTiled2DAddress ) >= m_iRepeatBlockWidth * m_iRepeatBlockHeight );

    CalculateLinearToTiledRemapping( iTexelPitch, m_pLinearToTiled2DAddress );
}


//---------------------------------------------------------------------------------------------------------
// Name: CPUUntiler::Untile( )
// Desc: Untile using the CPU optimized method demonstrated in this sample.
//---------------------------------------------------------------------------------------------------------
VOID CPUUntiler::UntileTexture( IDirect3DTexture9* pSourceTexture, IDirect3DTexture9* pDestTexture )
{
    XGTEXTURE_DESC SourceDesc;
    XGGetTextureDesc( pSourceTexture, 0, &SourceDesc );
    XGTEXTURE_DESC DestDesc;
    XGGetTextureDesc( pDestTexture, 0, &DestDesc );

    // Find data pointers 
    VOID* pSourceData = (VOID*) ( pSourceTexture->Format.BaseAddress << GPU_TEXTURE_ADDRESS_SHIFT );
    VOID* pSourceDataCachedReadOnly = (VOID*) ( GPU_CONVERT_CPU_TO_CPU_CACHED_READONLY_ADDRESS( pSourceData ) );
    VOID* pDestData = (VOID*) ( pDestTexture->Format.BaseAddress << GPU_TEXTURE_ADDRESS_SHIFT );

    BOOL bDestCacheable = !( XQueryMemoryProtect( pDestData ) & PAGE_WRITECOMBINE );
    assert( !( XQueryMemoryProtect( pDestData ) & PAGE_NOCACHE ) ); // that would be horrible

    UINT iTexelSizeInBytes = SourceDesc.BytesPerBlock;
    UINT iIntraRepeatBlockRowSizeInBytes = m_iRepeatBlockWidth * iTexelSizeInBytes;
    UINT iRepeatBlockSizeInBytes = m_iRepeatBlockHeight * iIntraRepeatBlockRowSizeInBytes;
    UINT iPreZeroSize = XGNextMultiple( iIntraRepeatBlockRowSizeInBytes, g_iCacheLineSize );   // must do whole cache lines
    assert( iIntraRepeatBlockRowSizeInBytes % g_iStoreGatherQueueSize == 0 );  // otherwise, bad write-combining

    UINT iWidthInRepeatBlocks = XGNextMultiple( SourceDesc.Width, m_iRepeatBlockWidth ) / m_iRepeatBlockWidth;
    UINT iHeightInRepeatBlocks = XGNextMultiple( SourceDesc.Height, m_iRepeatBlockHeight ) / m_iRepeatBlockHeight;

    UINT iSourceRowPitchInBytes = SourceDesc.RowPitch;
    UINT iDestRowPitchInBytes = DestDesc.RowPitch;
    UINT iSourceRepeatBlockRowPitchInBytes = iSourceRowPitchInBytes * m_iRepeatBlockHeight;
    UINT iDestRepeatBlockRowPitchInBytes = iDestRowPitchInBytes * m_iRepeatBlockHeight;

    BYTE* pCurrentRepeatBlockRowSourceData = NULL;
    BYTE* pCurrentRepeatBlockRowDestData = NULL;
    BYTE* pNextRepeatBlockRowSourceData = (BYTE*) pSourceDataCachedReadOnly;
    BYTE* pNextRepeatBlockRowDestData = (BYTE*) pDestData;

    BYTE* pCurrentRepeatBlockSourceData = NULL;
    BYTE* pCurrentRepeatBlockDestData = NULL;
    BYTE* pNextRepeatBlockSourceData = pNextRepeatBlockRowSourceData;
    BYTE* pNextRepeatBlockDestData = pNextRepeatBlockRowDestData;

    // Prefetch 1 repeat block ahead.  For 32-bit formats, this consumes 4 KB of cache space
    // for source data and 4 KB for dest data.  At any given time, we have one repeat block
    // prefetched and another repeat block in use.  Also, we prefetch the 1 KB lookup table.  
    // So total cache footprint is at most 17 KB for 32-bit formats.
    //
    // Empirically, L1 misses become a problem for 128-bit formats, for which the footprint
    // exceeds the total size of the L1 data cache.  For such formats, finer-grained prefetch
    // would be called for.
    Prefetch( (BYTE*) m_pLinearToTiled2DAddress, 
        m_iRepeatBlockWidth * m_iRepeatBlockHeight * sizeof( *m_pLinearToTiled2DAddress ) );
    PrimeCacheForNextRepeatBlock( pNextRepeatBlockSourceData, 
        pNextRepeatBlockDestData, 
        iRepeatBlockSizeInBytes, 
        m_iRepeatBlockHeight, 
        iPreZeroSize, 
        iDestRowPitchInBytes, 
        bDestCacheable );

    for( UINT iRepeatBlockY = 0; iRepeatBlockY < iHeightInRepeatBlocks; ++iRepeatBlockY )
    {
        pCurrentRepeatBlockRowSourceData = pNextRepeatBlockRowSourceData;
        pCurrentRepeatBlockRowDestData = pNextRepeatBlockRowDestData;
        pNextRepeatBlockRowSourceData += iSourceRepeatBlockRowPitchInBytes;
        pNextRepeatBlockRowDestData += iDestRepeatBlockRowPitchInBytes;

        for( UINT iRepeatBlockX = 0; iRepeatBlockX < iWidthInRepeatBlocks; ++iRepeatBlockX )
        {
            pCurrentRepeatBlockSourceData = pNextRepeatBlockSourceData;
            pCurrentRepeatBlockDestData = pNextRepeatBlockDestData;
            pNextRepeatBlockSourceData = ( iRepeatBlockX == iWidthInRepeatBlocks - 1 ) 
                ? pNextRepeatBlockRowSourceData 
                : ( pNextRepeatBlockSourceData + iRepeatBlockSizeInBytes );
            pNextRepeatBlockDestData = ( iRepeatBlockX == iWidthInRepeatBlocks - 1 ) 
                ? pNextRepeatBlockRowDestData 
                : ( pNextRepeatBlockDestData + iIntraRepeatBlockRowSizeInBytes );

            if( ( iRepeatBlockX < iWidthInRepeatBlocks - 1 ) || ( iRepeatBlockY < iHeightInRepeatBlocks - 1 ) )
            {
                PrimeCacheForNextRepeatBlock( pNextRepeatBlockSourceData, 
                    pNextRepeatBlockDestData, 
                    iRepeatBlockSizeInBytes, 
                    m_iRepeatBlockHeight, 
                    iPreZeroSize, 
                    iDestRowPitchInBytes, 
                    bDestCacheable );
            }

#pragma warning(push)
#pragma warning(disable:6326)   // Code analysis warning from inside the template instantiations invoked below:  
                                // "Potential comparison of a constant with another constant"

            // The template argument here is the log2 of the texel size
            switch( iTexelSizeInBytes )
            {
            case 1:
                UntileSingleRepeatBlock<0, 8>( pCurrentRepeatBlockSourceData, 
                    pCurrentRepeatBlockDestData, 
                    m_pLinearToTiled2DAddress, 
                    m_iRepeatBlockWidth, 
                    m_iRepeatBlockHeight, 
                    iDestRowPitchInBytes );
                break;

            case 2:
                UntileSingleRepeatBlock<1, 16>( pCurrentRepeatBlockSourceData, 
                    pCurrentRepeatBlockDestData, 
                    m_pLinearToTiled2DAddress, 
                    m_iRepeatBlockWidth, 
                    m_iRepeatBlockHeight, 
                    iDestRowPitchInBytes );
                break;

            case 4:
                UntileSingleRepeatBlock<2, 16>( pCurrentRepeatBlockSourceData, 
                    pCurrentRepeatBlockDestData, 
                    m_pLinearToTiled2DAddress, 
                    m_iRepeatBlockWidth, 
                    m_iRepeatBlockHeight, 
                    iDestRowPitchInBytes );
                break;

            case 8:
                UntileSingleRepeatBlock<3, 16>( pCurrentRepeatBlockSourceData, 
                    pCurrentRepeatBlockDestData, 
                    m_pLinearToTiled2DAddress, 
                    m_iRepeatBlockWidth, 
                    m_iRepeatBlockHeight, 
                    iDestRowPitchInBytes );
                break;

            case 16:
                UntileSingleRepeatBlock<4, 16>( pCurrentRepeatBlockSourceData, 
                    pCurrentRepeatBlockDestData, 
                    m_pLinearToTiled2DAddress, 
                    m_iRepeatBlockWidth, 
                    m_iRepeatBlockHeight, 
                    iDestRowPitchInBytes );
                break;

            default:
                assert( FALSE );    // texel size not yet supported
            }

#pragma warning(pop)
        }
    }
}



//--------------------------------------------------------------------------------------
// GPUMemPool.cpp
//
// A simple brute force linear memory allocator.  Each Allocate call
// searches through the list of intervals and returns the first 
// sufficiently large free one.  The list of intervals is maintained in
// address-order.
//
// XNA Developer Connection
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "GPUMemPool.h"

#include <xtl.h>
#include <xgraphics.h>
#include <xboxmath.h>
#include <d3dx9.h>
#include <AtgApp.h>
#include <AtgUtil.h>

static BOOL             g_bDebugDefragInPlace = false;            // Do a memexport even when src == dst
static BOOL             g_bDebugTestMemExportSpeed = false;            // Do a memexport of the entire buffer (to profile in Pix)

static const D3DCOLOR   FREE_COLOR = D3DCOLOR_ARGB( 0x00, 0x80, 0x80, 0x80 );

// DUMMY VERTEX DECLARATION FOR MEMEXPORT
// We use 8 vertex elements and 8 memexports for efficiency.  At 4 memexports,
// the shader is set-up bound (number of verts).  At 8 memexports, it becomes
// bandwidth bound, which is what we want.
// We use D3DDECLTYPE_USHORT4 (or any 16-bit type) because no 32-bit type can 
// be fetched and memexported bit-for-bit (due to handling of NaN's).
static const DWORD      g_dwVertexSize = 64;
static const DWORD      g_dwVertexElementCount = 8;
static const D3DVERTEXELEMENT9 s_d3dVertexElements[] =
{
    { 0,  0,  D3DDECLTYPE_USHORT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
    { 0,  8,  D3DDECLTYPE_USHORT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 1 },
    { 0,  16, D3DDECLTYPE_USHORT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 2 },
    { 0,  24, D3DDECLTYPE_USHORT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 3 },
    { 0,  32, D3DDECLTYPE_USHORT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 4 },
    { 0,  40, D3DDECLTYPE_USHORT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 5 },
    { 0,  48, D3DDECLTYPE_USHORT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 6 },
    { 0,  56, D3DDECLTYPE_USHORT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 7 },
    D3DDECL_END()
};


//--------------------------------------------------------------------------------------
// Name: Initialize()
// Desc: Allocates a contiguous physical buffer which is then aliased as a vertex
//       buffer and a memexport target.  
//--------------------------------------------------------------------------------------
HRESULT GpuMemPool::Initialize( LPDIRECT3DDEVICE9 pd3dDevice, DWORD dwMemPoolSize,
                                DWORD dwAlignment )
{
    m_dwMemPoolSize = dwMemPoolSize;
    m_dwAlignment = dwAlignment;
    m_pMemPool = XPhysicalAlloc( m_dwMemPoolSize, MAXULONG_PTR, 0,
                                 PAGE_READWRITE | PAGE_WRITECOMBINE | MEM_LARGE_PAGES );
    if( m_pMemPool == NULL )
    {
        return E_FAIL;
    }

    MemInterval* defaultInterval = new MemInterval;
    defaultInterval->m_pStart = m_pMemPool;
    defaultInterval->m_dwSize = m_dwMemPoolSize;
    defaultInterval->m_bFree = TRUE;
    m_memIntervalList.push_back( defaultInterval );

    m_bPossiblyFragmented = FALSE;

    m_pd3dDevice = pd3dDevice;

    // Alias the memory as a vertex buffer
    XGSetVertexBufferHeader( m_dwMemPoolSize, 0, 0, 0, &m_VertexBuffer );
    XGOffsetResourceAddress( &m_VertexBuffer, m_pMemPool );

    HRESULT hr = m_pd3dDevice->CreateVertexDeclaration( s_d3dVertexElements, &m_pVertexDecl );
    if( FAILED( hr ) )
    {
        Destroy();
        return hr;
    }

    // Load the vertex and pixel shaders.
    if( FAILED( ATG::LoadVertexShader( "game:\\Media\\Shaders\\MemoryMove.xvu",
                                       &m_pMemoryMoveVS ) ) )
    {
        Destroy();
        return ATGAPPERR_MEDIANOTFOUND;
    }
    if( FAILED( ATG::LoadPixelShader( "game:\\Media\\Shaders\\Dummy.xpu",
                                      &m_pDummyPS ) ) )
    {
        Destroy();
        return ATGAPPERR_MEDIANOTFOUND;
    }

    // Set the address of the overall buffer as a stream constant
    GPU_SET_MEMEXPORT_STREAM_CONSTANT( &m_memExportStreamConstant,
                                       m_pMemPool,                                 // root address
                                       m_dwMemPoolSize / ( g_dwVertexSize / g_dwVertexElementCount ),   // size in elements
                                       SURFACESWAP_LOW_RED,                        // ARGB vs ABGR
                                       GPUSURFACENUMBER_UINTEGER,                  // values are unsigned int
                                       GPUCOLORFORMAT_16_16_16_16,                 // elements are 4 16-bit values
                                       GPUENDIAN128_8IN16 );                       // byte-swapping for 16-bit values

    return S_OK;
}


VOID GpuMemPool::Destroy()
{
    XPhysicalFree( m_pMemPool );
    // Shouldn't call "m_VertexBuffer.Release()" for memory we control

    ZeroMemory( &m_VertexBuffer, sizeof( m_VertexBuffer ) );
}


//--------------------------------------------------------------------------------------
// Name: Allocate()
// Desc: Return first free block of sufficient size.  
//--------------------------------------------------------------------------------------
MemInterval* GpuMemPool::Allocate( DWORD dwSize, D3DCOLOR dwDebugColor )
{
    // Round up to alignment
    dwSize = XGNextMultiple( dwSize, m_dwAlignment );

    for( MEMINTERVALLIST::iterator it = m_memIntervalList.begin(); it != m_memIntervalList.end(); ++it )
    {
        MemInterval* pInterval = *it;

        if( pInterval->m_bFree )
        {
            if( pInterval->m_dwSize >= dwSize )
            {
                pInterval->m_bFree = FALSE;
                pInterval->m_dwDebugColor = dwDebugColor;

                if( pInterval->m_dwSize > dwSize )
                {
                    MemInterval* pNewInterval = new MemInterval;
                    pNewInterval->m_pStart =
                        ( LPVOID )( ( ( DWORD )pInterval->m_pStart ) + dwSize );
                    pNewInterval->m_dwSize = pInterval->m_dwSize - dwSize;
                    pNewInterval->m_bFree = TRUE;

                    // 'insert' puts the new interval _before_ the given interval,
                    // so we need to increment first.
                    ++it;
                    m_memIntervalList.insert( it, pNewInterval );

                    pInterval->m_dwSize = dwSize;
                }

                return pInterval;
            }
        }
    }

    return NULL;
}


//--------------------------------------------------------------------------------------
// Name: Free()
// Desc: Free the given interval.  Must point to a valid allocation.  
//--------------------------------------------------------------------------------------
VOID GpuMemPool::Free( const MemInterval* pIntervalToFree )
{
    MemInterval* pPrevInterval = NULL;
    for( MEMINTERVALLIST::iterator it = m_memIntervalList.begin(); it != m_memIntervalList.end(); ++it )
    {
        MemInterval* pInterval = *it;

        if( pIntervalToFree == pInterval )
        {
            // This is the interval to free.  Assert that it's allocated.
            assert( !pInterval->m_bFree );

            MemInterval* pMergedInterval = pInterval;

            ++it;
            MemInterval* pNextInterval = ( it != m_memIntervalList.end() ) ? *it : NULL;

            if( pPrevInterval && pPrevInterval->m_bFree )
            {
                // If the previous interval is already free, merge it with this one
                pMergedInterval = pPrevInterval;
                pMergedInterval->m_dwSize += pInterval->m_dwSize;

                m_memIntervalList.remove( pInterval );
                delete pInterval;
            }
            else
            {
                pInterval->m_bFree = TRUE;
            }

            if( pNextInterval && pNextInterval->m_bFree )
            {
                // If the next interval is already free, merge it with this one
                pMergedInterval->m_dwSize += pNextInterval->m_dwSize;

                m_memIntervalList.remove( pNextInterval );
                delete pNextInterval;
            }
            else if( ( !pPrevInterval || !pPrevInterval->m_bFree )
                && ( pNextInterval && !pNextInterval->m_bFree ) )
            {
                // This operation created a new free fragment which is not the
                // tail of the pool.
                m_bPossiblyFragmented = TRUE;
            }

            return;
        }

        pPrevInterval = pInterval;
    }

    // Free should always succeed.
    assert( false );
}


//--------------------------------------------------------------------------------------
// Name: BeginDefrag()
// Desc: Setup for GPU copying with memexport
//--------------------------------------------------------------------------------------
VOID GpuMemPool::BeginDefrag()
{
    // Set the vertex shader to the memory move shader
    m_pd3dDevice->SetVertexShader( m_pMemoryMoveVS );
    m_pd3dDevice->SetPixelShader( m_pDummyPS );

    // Set the entire pool as a vertex stream
    m_pd3dDevice->SetVertexDeclaration( m_pVertexDecl );
    m_pd3dDevice->SetStreamSource( 0, &m_VertexBuffer, 0, g_dwVertexSize );

    // Set the Memory Export Stream Constant derived from the vertex buffer
    m_pd3dDevice->SetVertexShaderConstantF( 0, m_memExportStreamConstant.c, 1 );

    // Give max GPRs to the vertex shader (hasn't shown any improvement in practice)
    m_pd3dDevice->SetShaderGPRAllocation( 0, 112, 16 );
}


//--------------------------------------------------------------------------------------
// Name: MemCpy()
// Desc: Perform a single GPU memcpy using memexport.  Could occur in multiple stages
//       if src and dst overlap.
//--------------------------------------------------------------------------------------
VOID GpuMemPool::MemCpy( VOID* pDst, const VOID* pSrc, DWORD dwSize )
{
    assert( ( ( DWORD )pSrc ) % g_dwVertexSize == 0 );
    assert( ( ( DWORD )pDst ) % g_dwVertexSize == 0 );
    assert( dwSize % g_dwVertexSize == 0 );

    DWORD dwOffsetInVerts = ( ( ( DWORD )pSrc ) - ( DWORD )pDst ) / g_dwVertexSize;

    DWORD dwSrcStartInBytes = ( DWORD )pSrc - ( DWORD )m_pMemPool;
    DWORD dwSrcStartInVerts = dwSrcStartInBytes / g_dwVertexSize;
    DWORD dwVertsLeftToMove = dwSize / g_dwVertexSize;

    FLOAT fOffsetInVerts = ( FLOAT )dwOffsetInVerts;
    m_pd3dDevice->SetVertexShaderConstantF( 1, &fOffsetInVerts, 1 );

    while( dwVertsLeftToMove > 0 )
    {
        DWORD dwChunkSizeInVerts = ( dwOffsetInVerts > 0 )
            ? min( dwVertsLeftToMove, dwOffsetInVerts ) : dwVertsLeftToMove;

        // Ensures no side effects of memexports on earlier
        // Draw calls using the same resource.
        m_pd3dDevice->BeginExport( 0, &m_VertexBuffer, D3DBEGINEXPORT_VERTEXSHADER );

        m_pd3dDevice->DrawPrimitive( D3DPT_POINTLIST, dwSrcStartInVerts,
                                     dwChunkSizeInVerts );

        // Ensures no side effects of later Draw calls 
        // on memexports using the same resource.
        m_pd3dDevice->EndExport( 0, &m_VertexBuffer, 0 );

        dwSrcStartInVerts += dwChunkSizeInVerts;
        dwVertsLeftToMove -= dwChunkSizeInVerts;
    }
}


//--------------------------------------------------------------------------------------
// Name: EndDefrag()
// Desc: Cleanup for GPU copying with memexport
//--------------------------------------------------------------------------------------
VOID GpuMemPool::EndDefrag()
{
    // Restore default GPR allocation
    m_pd3dDevice->SetShaderGPRAllocation( 0, 0, 0 );
}


//--------------------------------------------------------------------------------------
// Name: Defragment()
// Desc: Shift each committed block as far to the left as possible, to concentrate
//       all free space at the end of the pool.  The shifts are executed as MemExport
//       draw calls.  If src and dst regions overlap, but are not identical, multiple
//       smaller MemExports may be necessary to avoid corruption.  Since the memory
//       is used for textures, defragmentation must be synchronized with the GPU.
//
//       Returns whether any defragmentation was performed
//--------------------------------------------------------------------------------------
BOOL GpuMemPool::Defragment()
{
    if( m_bPossiblyFragmented || g_bDebugTestMemExportSpeed || g_bDebugDefragInPlace )
    {
        BeginDefrag();

        // For speed tests only.
        if( g_bDebugTestMemExportSpeed )
        {
            MemCpy( m_pMemPool, m_pMemPool, m_dwMemPoolSize );
        }

        LPVOID pDstStart = m_pMemPool;
        MEMINTERVALLIST defragIntervalList;
        for( MEMINTERVALLIST::iterator it = m_memIntervalList.begin(); it != m_memIntervalList.end(); )
        {
            MemInterval* pInterval = *it;
            ++it;

            if( !pInterval->m_bFree )
            {
                defragIntervalList.push_back( pInterval );

                MemCpy( pDstStart, pInterval->m_pStart, pInterval->m_dwSize );

                pInterval->m_pStart = pDstStart;
                pDstStart = ( LPVOID )( ( ( DWORD )pInterval->m_pStart ) + pInterval->m_dwSize );
            }
            else
            {
                delete pInterval;
            }
        }

        if( pDstStart < ( LPVOID )( ( DWORD )m_pMemPool + m_dwMemPoolSize ) )
        {
            MemInterval* pTailInterval = new MemInterval;
            pTailInterval->m_bFree = TRUE;
            pTailInterval->m_dwDebugColor = FREE_COLOR;
            pTailInterval->m_pStart = pDstStart;
            pTailInterval->m_dwSize = ( DWORD )m_pMemPool + m_dwMemPoolSize - ( DWORD )pDstStart;
            defragIntervalList.push_back( pTailInterval );
        }

        // Destroy the old list and replace it with the defragmented list
        m_memIntervalList = defragIntervalList;

        EndDefrag();

        // Signal no need to defragment again
        m_bPossiblyFragmented = FALSE;

        return TRUE;    // Performed defragment
    }
    else
    {
        return FALSE;   // Did not need to defragment
    }
}


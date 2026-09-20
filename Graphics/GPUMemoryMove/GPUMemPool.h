//--------------------------------------------------------------------------------------
// GPUMemPool.h
//
// A simple brute force linear memory allocator.  Each Allocate call
// searches through the list of free intervals and returns the first 
// sufficiently large one.
//
// XNA Developer Connection
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#pragma once

#pragma warning(push)
// \Include\Xbox\list(1143) : warning C4127: conditional expression is constant
#pragma warning(disable : 4127)
#include <list>
#pragma warning(pop)

#include <xtl.h>

//--------------------------------------------------------------------------------------
// Name: struct MemInterval
// Desc: An allocation from the memory pool
//--------------------------------------------------------------------------------------
struct MemInterval
{
    VOID* m_pStart;
    DWORD m_dwSize;
    BOOL m_bFree;
    D3DCOLOR m_dwDebugColor;
};
typedef std::list <MemInterval*> MEMINTERVALLIST;

//--------------------------------------------------------------------------------------
// Name: class GpuMemPool
// Desc: The memory pool manager
//--------------------------------------------------------------------------------------
class GpuMemPool
{
public:
    HRESULT     Initialize( LPDIRECT3DDEVICE9 pd3dDevice, DWORD dwMemPoolSize,
                            DWORD dwAlignment );
    VOID        Destroy();

    // Allocate() returns a MemInterval* rather than an address, so that the location
    // can change following a Defragment().
    MemInterval* Allocate( DWORD dwSize, D3DCOLOR dwDebugColor );
    VOID        Free( const MemInterval* pIntervalToFree );

    // These functions are exposed for Debug rendering
    const MEMINTERVALLIST* GetMemIntervalList() const
    {
        return &m_memIntervalList;
    }
    const VOID* GetMemPoolStart() const
    {
        return m_pMemPool;
    }
    const DWORD GetMemPoolSize() const
    {
        return m_dwMemPoolSize;
    }

    VOID        BeginDefrag();
    VOID        MemCpy( VOID* pDst, const VOID* pSrc, DWORD dwSize );
    VOID        EndDefrag();

    BOOL        Defragment();

private:
    DWORD m_dwMemPoolSize;
    DWORD m_dwAlignment;
    VOID* m_pMemPool;
    MEMINTERVALLIST m_memIntervalList;

    LPDIRECT3DDEVICE9 m_pd3dDevice;
    IDirect3DVertexBuffer9 m_VertexBuffer;
    LPDIRECT3DVERTEXDECLARATION9 m_pVertexDecl;
    IDirect3DVertexShader9* m_pMemoryMoveVS;
    IDirect3DPixelShader9* m_pDummyPS;
    GPU_MEMEXPORT_STREAM_CONSTANT m_memExportStreamConstant;

    BOOL m_bPossiblyFragmented;
};


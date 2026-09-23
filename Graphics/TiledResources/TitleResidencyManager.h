//--------------------------------------------------------------------------------------
// TitleResidencyManager.h
//
// This class implements the streaming system for tiles within the sample.  It takes
// residency sample data from the residency sample renderer, builds a prioritized
// list of tiles to load from disk and unload from memory, and orchestrates the tile
// loader to load & map tiles.  Several worker threads are employed to execute the work
// in parallel.
//
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#pragma once

#include <xtl.h>
#include <xgraphics.h>
#include <xmcore.h>
#include <vector>
#include <deque>
#include <hash_map>
#include <list>       // std::list (TrackedTileSortList) -- MSVC <hash_map> dragged this
                      // in transitively; the libc++ <hash_map> compat does not
#include <stack>
#include <algorithm>

#include "d3d9tiled.h"

namespace ATG
{
    class Font;
}

#define ASSERT(x) assert(x)

typedef UINT ResourceSetID;

// Set this define to 1 to make loader debugging easier (only one instance will be active)
#define LOADER_DEBUG 0

#if LOADER_DEBUG
#define MAX_LOADER_THREAD_COUNT 1
#else
#define MAX_LOADER_THREAD_COUNT 4
#endif

#define MAX_UNLOADER_THREAD_COUNT 1

class TitleResidencyManager;

//--------------------------------------------------------------------------------------
// Name: TrackedTileID
// Desc: The public description of a tile that is being tracked by the title residency
//       manager.  It contains all information required to locate the tile within a
//       resource, load data into that tile, and manage its lifetime within the title
//       residency manager.
//--------------------------------------------------------------------------------------
struct TrackedTileID
{
    // The resource that this tile belongs to:
    D3DTiledTexture* pResource;

    // The virtual address of this tile (this will always be unique):
    D3DTILED_VIRTUAL_ADDRESS VTileID;

    // The physical address of this tile (this may not be unique, or may be NULL):
    D3DTILED_PHYSICAL_ADDRESS TileID;

    // The texture space U address where this tile was first observed:
    FLOAT U;

    // The texture space V address where this tile was first observed:
    FLOAT V;

    // The mip level that this tile inhabits:
    USHORT MipLevel;

    // The array slice index that this tile inhabits:
    USHORT ArraySlice;

    // If PinnedTile is TRUE, the title residency manager will never unmap or delete
    // this tile:
    BOOL PinnedTile;
};

//--------------------------------------------------------------------------------------
// Name: ITileLoader
// Desc: The interface for tile loader classes.  The title residency manager can track
//       one unique tile loader per resource, or it can share a tile loader among several
//       resources.
//--------------------------------------------------------------------------------------
class ITileLoader
{
private:
    friend class TitleResidencyManager;

    // The tile loader holds pointers to one context per thread.
    // The thread count is defined at runtime by the title residency manager.
    VOID* m_pLoaderContexts[MAX_LOADER_THREAD_COUNT];
    VOID* m_pUnloaderContexts[MAX_UNLOADER_THREAD_COUNT];

public:
    // A pointer to the single D3D tile manager:
    D3DTilePool* m_pTilePool;

    ITileLoader()
    {
        // Make sure unused contexts are NULL.
        for( UINT i = 0; i < ARRAYSIZE(m_pLoaderContexts); ++i )
        {
            m_pLoaderContexts[i] = NULL;
        }
        for( UINT i = 0; i < ARRAYSIZE(m_pUnloaderContexts); ++i )
        {
            m_pUnloaderContexts[i] = NULL;
        }
        m_pTilePool = NULL;
    }

    virtual ~ITileLoader()
    {
    }

    virtual VOID* CreateThreadContext() { return NULL; }
    virtual VOID DestroyThreadContext( VOID* pThreadContext ) {}
    virtual BOOL TileNeedsUniquePhysicalTile( TrackedTileID* pTileID ) { return TRUE; }

    // Pure virtual methods that must be implemented by the subclasses:
    virtual HRESULT LoadAndMapTile( TrackedTileID* pTileID, VOID* pThreadContext ) = NULL;
    virtual HRESULT UnmapTile( TrackedTileID* pTileID, VOID* pThreadContext ) = NULL;
};

//--------------------------------------------------------------------------------------
// Name: ITileActivityHandler
// Desc: A simple callback interface that allows external classes to be notified when
//       tiles are loaded or unloaded.
//--------------------------------------------------------------------------------------
class ITileActivityHandler
{
public:
    virtual VOID TileLoaded( const TrackedTileID* pTileID ) = NULL;
    virtual VOID TileUnloaded( const TrackedTileID* pTileID ) = NULL;
};

//--------------------------------------------------------------------------------------
// Name: ResidencyStats
// Desc: A struct that holds statistics related to tile management.  The title residency
//       manager updates statistics in this struct each frame.
//--------------------------------------------------------------------------------------
struct ResidencyStats
{
    BOOL OutOfPhysicalTiles;
    UINT NumTilesTracked;
    UINT NumTilesLoaded;
    UINT NumTilesQueuedForLoad;
    UINT NumTilesUnused;
    UINT LoaderThreadCount;
    UINT UnloaderThreadCount;
};

//--------------------------------------------------------------------------------------
// Name: TitleResidencyManager
// Desc: Manages tile residency and streaming across all of the tiled resources used by
//       the title.
//--------------------------------------------------------------------------------------
class TitleResidencyManager
{
    struct SamplingView
    {
        UINT RenderFrameIndex;
        D3DTexture* pTextureUVGradientID;
        D3DTexture* pTextureExtendedUVSlice;
        VOID* pTextureBufferUVGradientID;
        VOID* pTextureBufferExtendedUVSlice;
    };

    enum TrackedTileState
    {
        // The tile has been seen at least once:
        TPS_Seen,

        // The tile has been seen enough times to be queued for loading:
        TPS_QueuedForLoad,

        // The tile is currently being processed by a tile loader:
        TPS_Loading,

        // The tile is fully loaded and mapped:
        TPS_LoadedAndMapped,

        // The tile is no longer important, and needs to be unmapped:
        TPS_QueuedForUnmap,

        // The tile is currently being unmapped by a tile loader:
        TPS_Unmapping,

        // The tile has been unmapped, and the physical tile can be released or recycled:
        TPS_Unmapped
    };

    //--------------------------------------------------------------------------------------
    // Name: TrackedTile
    // Desc: The internal tracking structure for a tracked tile within the title residency
    //       manager.  In addition to the public information in the TrackedTileID struct,
    //       this enhanced record includes the state of the tile, the associated tile loader,
    //       and statistics related to how often the tile has been seen and how important
    //       the tile is to the rendered scene.
    //--------------------------------------------------------------------------------------
    struct TrackedTile
    {
        // Information that identifies the tile:
        TrackedTileID ID;

        // The state of the tile in the tracking system:
        TrackedTileState State;

        // The tile loader associated with the resource that this tile belongs to:
        ITileLoader* pTileLoader;

        // The number of times that the tile has shown up in residency samples:
        UINT SampleCount;

        // The last frame time this tile was seen in a residency pass:
        UINT LastTimeSeen;

        // A score that represents how close the tile was to the center of the render view last time it was seen:
        UINT ViewPositionScore;

        // A snapshot of the priority score when the tile was queued for loading:
        UINT InsertPriority;

        // The continuously updated priority score as the tile is queued for loading:
        UINT CurrentPriority;
    };

    //--------------------------------------------------------------------------------------
    // Name: ResourceSet
    // Desc: A set of tiled resources that share the same UV texture mapping in the scene
    //       content.  This way, one residency sample can be used to infer residency for
    //       several different resources, even if they differ in size and content.
    //--------------------------------------------------------------------------------------
    struct ResourceSet
    {
        ResourceSetID ID;
        D3DTiledTexture** ppResources;
        ITileLoader** ppTileLoaders;
        UINT ResourceCount;
    };

    //--------------------------------------------------------------------------------------
    // Name: ThreadEntryContext
    // Desc: A structure used to pass initialization information to the loader threads.
    //--------------------------------------------------------------------------------------
    struct ThreadEntryContext
    {
        TitleResidencyManager* pTRM;
        UINT ThreadIndex;
    };

protected:
    UINT m_FrameIndex;
    UINT m_CurrentFrameTime;

    D3DTilePool* m_pTilePool;

    // The title tile manager has a overall physical tile limit:
    UINT m_AllocatedTileCount;
    UINT m_MaxPhysicalTiles;

    // D3D members for the residency sample rendering:
    D3DSurface* m_pResidencyRenderTargetUVGradientID;
    D3DSurface* m_pResidencyRenderTargetExtendedUVSlice;
    D3DSurface* m_pResidencyDepthStencil;
    D3DVIEWPORT9 m_ResidencyViewport;

    std::vector<SamplingView> m_SamplingViews;
    UINT m_CurrentViewIndex;
    UINT m_NextViewIndex;

    // The list of tracked tiles, indexed by virtual address and also as a sorted list:
    typedef std::hash_map<D3DTILED_VIRTUAL_ADDRESS, TrackedTile*> TrackedTileMap;
    TrackedTileMap m_TrackedTileMap;
    typedef std::list<TrackedTile*> TrackedTileSortList;
    TrackedTileSortList m_TrackedTileSortList;

    // A stack of uninitialized TrackedTile structs to minimize new/delete on these small structs:
    std::stack<TrackedTile*> m_TrackedTileFreeList;

    // The list of resource sets:
    std::vector<ResourceSet> m_ResourceSets;

    // The list of tile activity handlers that are notified when tiles are loaded and unloaded:
    std::vector<ITileActivityHandler*> m_TileActivityHandlers;

    // The gradient scaling factor is used to encode and decode texture UV gradients captured
    // during the residency sample pass:
    FLOAT m_fGradientScalingFactor;

    // The queue for tiles waiting to be loaded:
    XLockFreePriorityQueue<TrackedTile> m_LoadQueue;

    // The queue for tiles waiting to be unmapped:
    XLockFreeQueue<TrackedTile> m_UnmapQueue;

    // The queue of physical tiles that have been unmapped and can be recycled:
    CRITICAL_SECTION m_TileRecycleQueueCS;
    std::deque<D3DTILED_PHYSICAL_ADDRESS> m_TileRecycleQueue;

    // A flag that indicates that tiles are waiting to be loaded, but there are no
    // free physical tiles:
    BOOL m_NeedTilesNow;

    // Loader thread handles:
    HANDLE m_hLoaderThreads[MAX_LOADER_THREAD_COUNT];

    // A signalling event that tells the loader threads that at least one entry is in the load queue:
    HANDLE m_hLoaderEvent;

    // A flag that tells the loader threads when it's time to terminate:
    BOOL m_LoaderRunning;

    // A thread count variable that each loader thread increments at init time, and decrements at
    // termination time:
    volatile LONG m_ThreadCount;

    // Unloader thread handles:
    HANDLE m_hUnloaderThreads[MAX_UNLOADER_THREAD_COUNT];

    // A flag that tells the unloader threads that there's at least one entry in the unmap queue:
    HANDLE m_hUnloaderEvent;

    // Residency statistics that are computed each frame:
    ResidencyStats m_ResidencyStats;

    // A flag that indicates if all streaming should be paused:
    BOOL m_bPaused;

public:
    TitleResidencyManager( D3DDevice* pd3dDevice, UINT MaxViewsPerFrame, UINT MaxPhysicalTiles, D3DTilePool* pTilePool );

    VOID Update( FLOAT fDeltaTime );
    const ResidencyStats& GetStats() const { return m_ResidencyStats; }

    VOID CreateResourceConstant( ResourceSetID RSID, XMFLOAT4* pConstant );

    UINT BeginResidencyView( D3DDevice* pd3dDevice );
    VOID EndResidencyView( D3DDevice* pd3dDevice, UINT ViewID );

    ResourceSetID CreateResourceSet( const D3DTiledTexture** ppResources, ITileLoader** ppTileLoaders, UINT ResourceCount );

    VOID RegisterTileActivityHandler( ITileActivityHandler* pHandler );

    UINT GetTrackedTileCount() const { return m_TrackedTileSortList.size(); }

    UINT DebugRenderTiles( D3DDevice* pd3dDevice, ATG::Font* pFont, D3DTiledTexture* pExamineTexture, UINT TileSizePixels, INT YOffsetPixels );

    VOID DebugRenderResidencyView( UINT XPos, UINT YPos );

protected:
    static DWORD LoaderThreadProc( VOID* pParam );
    VOID LoaderEntryPoint( const UINT ThreadIndex );

    static DWORD UnloaderThreadProc( VOID* pParam );
    VOID UnloaderEntryPoint( const UINT ThreadIndex );

    BOOL IsTrackedTileExpired( TrackedTile* pTP ) const;

    static bool TrackedTileSortPredicate( const TrackedTile* pA, const TrackedTile* pB );
    VOID UpdateTileStates();
    VOID CollectViewSamples( SamplingView& View );
    VOID ProcessSample( const XMUBYTEN4& UVGradientIDSample, const XMUBYTEN4& ExtendedUVSliceSample, const UINT ViewPositionScore );
    TrackedTile* IncrementSampleCount( D3DTILED_VIRTUAL_ADDRESS VTileID, FLOAT TexU, FLOAT TexV, UINT SliceIndex, UINT MipLevel, D3DTiledTexture* pResource, ITileLoader* pTileLoader, const UINT ViewPositionScore );
    TrackedTile* AddVirtualTile( D3DTILED_VIRTUAL_ADDRESS VTileID, FLOAT TexU, FLOAT TexV, UINT SliceIndex, UINT MipLevel, D3DTiledTexture* pResource, ITileLoader* pTileLoader, const UINT ViewPositionScore );

    VOID QueueTileForLoadAndMap( TrackedTile* pTP, BOOL RecomputePriority );
    VOID QueueTileForUnmap( TrackedTile* pTP );

    UINT ComputePriority( TrackedTile* pTP ) const;

    VOID SetResidencyRenderTargets( D3DDevice* pd3dDevice );

    VOID NotifyTileActivity( const TrackedTileID* pTileID, BOOL Loaded ) const;
};

const WCHAR* GetFormatName( const D3DFORMAT Format );

//--------------------------------------------------------------------------------------
// TitleResidencyManager.cpp
//
// This class implements the streaming system for tiles within the sample.  It takes
// residency sample data from the residency sample renderer, builds a prioritized
// list of tiles to load from disk and unload from memory, and orchestrates the tile
// loader to load & map tiles.  Several worker threads are employed to execute the work
// in parallel.
//
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include <assert.h>
#include "TitleResidencyManager.h"
#include <AtgUtil.h>
#include <AtgDebugDraw.h>
#include <AtgFont.h>

// The residency view has a fixed size (small fraction of 720p):
static const UINT g_ResidencyViewWidth = 256;
static const UINT g_ResidencyViewHeight = 144;
static const FLOAT g_TexelPitchMultiplier = (FLOAT)g_ResidencyViewWidth / (FLOAT)1280;
static const D3DFORMAT g_ResidencyViewFormat = D3DFMT_A8R8G8B8;

// We can only have 255 resource sets:
static const ResourceSetID g_MaxResourceSetID = 255;

//--------------------------------------------------------------------------------------
// Name: TitleResidencyManager constructor
//--------------------------------------------------------------------------------------
TitleResidencyManager::TitleResidencyManager( D3DDevice* pd3dDevice, UINT MaxViewsPerFrame, UINT MaxPhysicalTiles, D3DTilePool* pTilePool )
{
    m_pTilePool = pTilePool;
    m_FrameIndex = 0;
    m_bPaused = FALSE;

    D3DSURFACE_PARAMETERS Params = { 0 };
    HRESULT hr = pd3dDevice->CreateRenderTarget( g_ResidencyViewWidth, g_ResidencyViewHeight, g_ResidencyViewFormat, D3DMULTISAMPLE_NONE, 0, FALSE, &m_pResidencyRenderTargetUVGradientID, &Params );
    ASSERT( SUCCEEDED(hr) );

    Params.Base = GPU_EDRAM_TILES / 3;
    hr = pd3dDevice->CreateRenderTarget( g_ResidencyViewWidth, g_ResidencyViewHeight, g_ResidencyViewFormat, D3DMULTISAMPLE_NONE, 0, FALSE, &m_pResidencyRenderTargetExtendedUVSlice, &Params );
    ASSERT( SUCCEEDED(hr) );

    Params.Base = ( 2 * GPU_EDRAM_TILES ) / 3;
    hr = pd3dDevice->CreateDepthStencilSurface( g_ResidencyViewWidth, g_ResidencyViewHeight, D3DFMT_D24FS8, D3DMULTISAMPLE_NONE, 0, FALSE, &m_pResidencyDepthStencil, &Params );
    ASSERT( SUCCEEDED(hr) );

    m_ResidencyViewport.X = 0;
    m_ResidencyViewport.Y = 0;
    m_ResidencyViewport.Width = g_ResidencyViewWidth;
    m_ResidencyViewport.Height = g_ResidencyViewHeight;
    m_ResidencyViewport.MinZ = 1.0f;
    m_ResidencyViewport.MaxZ = 0.0f;

    MaxViewsPerFrame *= 2;
    for( UINT i = 0; i < MaxViewsPerFrame; ++i )
    {
        SamplingView View;
        View.RenderFrameIndex = (UINT)-1;

        View.pTextureUVGradientID = new D3DTexture;
        UINT BaseSize, MipSize;
        XGSetTextureHeader( g_ResidencyViewWidth, g_ResidencyViewHeight, 1, 0, g_ResidencyViewFormat, D3DPOOL_DEFAULT, 0, 0, 0, View.pTextureUVGradientID, &BaseSize, &MipSize );
        BaseSize += MipSize;

        View.pTextureBufferUVGradientID = XPhysicalAlloc( BaseSize, MAXULONG_PTR, GPU_TEXTURE_ALIGNMENT, PAGE_READWRITE | PAGE_WRITECOMBINE );
        ZeroMemory( View.pTextureBufferUVGradientID, BaseSize );

        XGOffsetBaseTextureAddress( View.pTextureUVGradientID, View.pTextureBufferUVGradientID, View.pTextureBufferUVGradientID );

        View.pTextureExtendedUVSlice = new D3DTexture;
        XGSetTextureHeader( g_ResidencyViewWidth, g_ResidencyViewHeight, 1, 0, g_ResidencyViewFormat, D3DPOOL_DEFAULT, 0, 0, 0, View.pTextureExtendedUVSlice, &BaseSize, &MipSize );
        BaseSize += MipSize;

        View.pTextureBufferExtendedUVSlice = XPhysicalAlloc( BaseSize, MAXULONG_PTR, GPU_TEXTURE_ALIGNMENT, PAGE_READWRITE | PAGE_WRITECOMBINE );
        ZeroMemory( View.pTextureBufferExtendedUVSlice, BaseSize );

        XGOffsetBaseTextureAddress( View.pTextureExtendedUVSlice, View.pTextureBufferExtendedUVSlice, View.pTextureBufferExtendedUVSlice );

        m_SamplingViews.push_back( View );
    }
    m_CurrentViewIndex = (UINT)-1;
    m_NextViewIndex = 0;

    ResourceSet EmptySet = { 0 };
    m_ResourceSets.push_back( EmptySet );

    m_MaxPhysicalTiles = MaxPhysicalTiles;
    m_AllocatedTileCount = 0;

    // Compute the gradient scaling factor.
    // The gradient scaling factor maps a range of ln(1) to ln(1/16384) into 0..1.
    m_fGradientScalingFactor = log( 1.0f / 16384.0f );

    m_LoadQueue.Initialize();
    m_UnmapQueue.Initialize();
    InitializeCriticalSection( &m_TileRecycleQueueCS );
    m_NeedTilesNow = FALSE;

    // Create the loader and unloader signaling events:
    m_hLoaderEvent = CreateEvent( NULL, FALSE, FALSE, NULL );
    m_hUnloaderEvent = CreateEvent( NULL, FALSE, FALSE, NULL );


    // Create the loader threads:
    m_LoaderRunning = TRUE;
    for( UINT i = 0; i < ARRAYSIZE(m_hLoaderThreads); ++i )
    {
        ThreadEntryContext* pEntryContext = new ThreadEntryContext();
        pEntryContext->pTRM = this;
        pEntryContext->ThreadIndex = i;
        m_hLoaderThreads[i] = CreateThread( NULL, 0, LoaderThreadProc, pEntryContext, CREATE_SUSPENDED, NULL );
        assert( m_hLoaderThreads[i] != 0 );
        XSetThreadProcessor( m_hLoaderThreads[i], 2 + ( i % 4 ) );
        ResumeThread( m_hLoaderThreads[i] );
    }

    for( UINT i = 0; i < ARRAYSIZE( m_hUnloaderThreads ); ++i )
    {
        ThreadEntryContext* pEntryContext = new ThreadEntryContext();
        pEntryContext->pTRM = this;
        pEntryContext->ThreadIndex = i;
        m_hUnloaderThreads[i] = CreateThread( NULL, 0, UnloaderThreadProc, pEntryContext, CREATE_SUSPENDED, NULL );
        XSetThreadProcessor( m_hUnloaderThreads[i], 2 );
        ResumeThread( m_hUnloaderThreads[i] );
    }
}

//--------------------------------------------------------------------------------------
// Name: TitleResidencyManager::LoaderThreadProc
// Desc: Static entry point for the loader thread.  Passes execution to a private method
//       on the title residency manager.
//--------------------------------------------------------------------------------------
DWORD TitleResidencyManager::LoaderThreadProc( VOID* pParam )
{
    ThreadEntryContext* pEntryContext = (ThreadEntryContext*)pParam;
    
    TitleResidencyManager* pTRM = pEntryContext->pTRM;
    UINT ThreadIndex = pEntryContext->ThreadIndex;
    delete pEntryContext;

    InterlockedIncrement( &pTRM->m_ThreadCount );
    pTRM->LoaderEntryPoint( ThreadIndex );
    InterlockedDecrement( &pTRM->m_ThreadCount );

    return 0;
}

//--------------------------------------------------------------------------------------
// Name: TitleResidencyManager::UnloaderThreadProc
// Desc: Static entry point for the unloader thread.  Passes execution to a private method
//       on the title residency manager.
//--------------------------------------------------------------------------------------
DWORD TitleResidencyManager::UnloaderThreadProc( VOID* pParam )
{
    ThreadEntryContext* pEntryContext = (ThreadEntryContext*)pParam;

    TitleResidencyManager* pTRM = pEntryContext->pTRM;
    UINT ThreadIndex = pEntryContext->ThreadIndex;
    delete pEntryContext;

    InterlockedIncrement( &pTRM->m_ThreadCount );
    pTRM->UnloaderEntryPoint( ThreadIndex );
    InterlockedDecrement( &pTRM->m_ThreadCount );

    return 0;
}

//--------------------------------------------------------------------------------------
// Name: TitleResidencyManager::UnloaderEntryPoint
// Desc: Main loop for a single unloader thread.  It pops a TrackedTile* off the
//       unloader queue and passes it to the appropriate tile loader class for processing.
//--------------------------------------------------------------------------------------
VOID TitleResidencyManager::UnloaderEntryPoint( const UINT ThreadIndex )
{
    while( m_LoaderRunning )
    {
        while( !m_UnmapQueue.IsEmpty() )
        {
            while( m_bPaused )
            {
                YieldProcessor();
            }

            PIXBeginNamedEvent( 0, "Tile Unloader Task" );
            TrackedTile* pTP = NULL;
            HRESULT hr = m_UnmapQueue.Remove( &pTP );
            if( SUCCEEDED(hr) && pTP != NULL )
            {

                // Validate the tracked tile is ready for unmapping:
                ASSERT( pTP->State == TPS_QueuedForUnmap );
                ASSERT( pTP->ID.PinnedTile == FALSE );
                ASSERT( pTP->ID.VTileID != D3DTILED_INVALID_VIRTUAL_ADDRESS );

                // Update the state:
                pTP->State = TPS_Unmapping;

                // Pass the tracked tile ID to the tile loader:
                ITileLoader* pTileLoader = pTP->pTileLoader;
                ASSERT( pTileLoader != NULL );
                ASSERT( ThreadIndex < ARRAYSIZE(pTileLoader->m_pUnloaderContexts) );
                VOID* pUnloaderContext = pTileLoader->m_pUnloaderContexts[ThreadIndex];

                pTileLoader->UnmapTile( &pTP->ID, pUnloaderContext );

                // Recycle the physical tile if one was used for this entry:
                if( pTP->ID.TileID != D3DTILED_INVALID_PHYSICAL_ADDRESS )
                {
                    //ATG::DebugSpew( "Recycling tile %I64u\n", pTP->ID.TileID );

                    // recycle physical tile
                    EnterCriticalSection( &m_TileRecycleQueueCS );
                    m_TileRecycleQueue.push_back( pTP->ID.TileID );
                    LeaveCriticalSection( &m_TileRecycleQueueCS );

                    pTP->ID.TileID = D3DTILED_INVALID_PHYSICAL_ADDRESS;
                }

                // Update the state:
                pTP->State = TPS_Unmapped;
                // Notify the tile activity handlers:
                NotifyTileActivity( &pTP->ID, FALSE );
            }
            PIXEndNamedEvent();
        }

        // Wait for the unloader queue signalling event:
        WaitForSingleObject( m_hUnloaderEvent, 50 );
    }
}

//--------------------------------------------------------------------------------------
// Name: TitleResidencyManager::LoaderEntryPoint
// Desc: Main loop for a single loader thread.  It pops a TrackedTile* off the loading
//       queue, determines if it needs a new physical tile, and then passes the tracked
//       tile ID to a tile loader for processing.
//--------------------------------------------------------------------------------------
VOID TitleResidencyManager::LoaderEntryPoint( const UINT ThreadIndex )
{
    while( m_LoaderRunning )
    {
        while( !m_LoadQueue.IsEmpty() )
        {
            while( m_bPaused )
            {
                YieldProcessor();
            }

            TrackedTile* pTP = NULL;
            HRESULT hr = m_LoadQueue.RemoveFirst( &pTP );
            if( SUCCEEDED(hr) && pTP != NULL )
            {
                // Check if the load request has already been loaded.
                // Since we can re-queue a load request if its priority changes, we can often
                // see entries in the queue that have already been loaded:
                if( pTP->State != TPS_QueuedForLoad )
                {
                    continue;
                }

                // Update tile state:
                pTP->State = TPS_Loading;

                ITileLoader* pTileLoader = pTP->pTileLoader;
                ASSERT( pTileLoader != NULL );
                ASSERT( ThreadIndex < ARRAYSIZE(pTileLoader->m_pLoaderContexts) );
                VOID* pLoaderContext = pTileLoader->m_pLoaderContexts[ThreadIndex];

                // Check if the load request is out of date.  The load request can become out of date
                // if the tile hasn't been seen for a while and we need new tiles right now for other loads:
                if( IsTrackedTileExpired( pTP ) )
                {
                    // This tile was never mapped, nor was a physical tile assigned; reassign to the unmapped state:
                    ASSERT( pTP->ID.TileID == D3DTILED_INVALID_PHYSICAL_ADDRESS );
                    pTP->State = TPS_Unmapped;
                }
                else
                {
                    // Check if the priority has changed for the worse since this tile was queued.
                    // This can happen if a tile was originally observed in a place that deemed it high priority, but
                    // while it was queued, the camera moved away and thus the request became lower priority.
                    if( pTP->InsertPriority > 10 && pTP->CurrentPriority >= ( pTP->InsertPriority * 2 ) )
                    {
                        // Re-queue the request at the current priority; we'll deal with it later when it comes off the
                        // queue again.
                        pTP->InsertPriority = pTP->CurrentPriority;
                        pTP->State = TPS_QueuedForLoad;
                        m_LoadQueue.Add( pTP->InsertPriority, pTP );
                        SetEvent( m_hLoaderEvent );
                        continue;
                    }

                    PIXBeginNamedEvent( 0, "Tile Loader Task" );

                    // Determine if the virtual tile needs to be mapped to a unique physical tile:
                    D3DTILED_PHYSICAL_ADDRESS TileID = D3DTILED_INVALID_PHYSICAL_ADDRESS;

                    BOOL NeedsUniquePhysicalAddress = pTileLoader->TileNeedsUniquePhysicalTile( &pTP->ID );

                    if( NeedsUniquePhysicalAddress )
                    {
                        // We need a unique physical tile.

                        ASSERT( TileID == D3DTILED_INVALID_PHYSICAL_ADDRESS );

                        // Try to get a physical tile from the tile recycle queue:
                        EnterCriticalSection( &m_TileRecycleQueueCS );
                        if( !m_TileRecycleQueue.empty() )
                        {
                            TileID = m_TileRecycleQueue.front();
                            m_TileRecycleQueue.pop_front();
                        }
                        LeaveCriticalSection( &m_TileRecycleQueueCS );

                        // Try to allocate a new physical tile from the tile manager:
                        if( TileID == D3DTILED_INVALID_PHYSICAL_ADDRESS )
                        {
                            D3DTILED_SURFACE_DESC BaseDesc;
                            pTP->ID.pResource->GetLevelDesc( 0, &BaseDesc );

                            HRESULT hrAlloc = m_pTilePool->AllocatePhysicalTile( &TileID, BaseDesc.Format );
                            if( !SUCCEEDED(hrAlloc) )
                            {
                                TileID = D3DTILED_INVALID_PHYSICAL_ADDRESS;
                            }
                        }
                    }

                    if( TileID != D3DTILED_INVALID_PHYSICAL_ADDRESS || !NeedsUniquePhysicalAddress )
                    {
                        // We have a physical tile; assign it to the tracked tile entry and pass it to the loader.

                        // Set the selected physical tile to the tile ID.  If the virtual tile doesn't require a
                        // unique physical tile, this physical tile will be INVALID:
                        pTP->ID.TileID = TileID;

                        // Pass the tile ID to the loader:
                        pTileLoader->LoadAndMapTile( &pTP->ID, pLoaderContext );

                        // Update tile state:
                        pTP->State = TPS_LoadedAndMapped;

                        // Reset the tiles now state:
                        m_NeedTilesNow = FALSE;

                        // Notify listeners that a tile has been loaded:
                        NotifyTileActivity( &pTP->ID, TRUE );
                    }
                    else
                    {
                        // We are out of physical tiles; re-add request to load queue so we can try again later:
                        pTP->InsertPriority = pTP->CurrentPriority;
                        pTP->State = TPS_QueuedForLoad;
                        m_LoadQueue.Add( pTP->InsertPriority, pTP );
                        SetEvent( m_hLoaderEvent );
                        m_NeedTilesNow = TRUE;
                    }

                    PIXEndNamedEvent();
                }
            }
        }
        // Wait for the unloader queue signalling event:
        WaitForSingleObject( m_hLoaderEvent, 50 );
    }
}

//--------------------------------------------------------------------------------------
// Name: TitleResidencyManager::Update
// Desc: Perform per-frame CPU tasks, including scanning the residency sample views for
//       samples, and updating tile states to move tiles through the streaming queues.
//--------------------------------------------------------------------------------------
VOID TitleResidencyManager::Update( FLOAT fDeltaTime )
{
    if( fDeltaTime <= 0.0f )
    {
        m_bPaused = TRUE;
        return;
    }
    else
    {
        m_bPaused = FALSE;
    }

    UINT LastFrameIndex = m_FrameIndex;
    ++m_FrameIndex;
    m_CurrentFrameTime = GetTickCount();

    // Iterate through the residency sample views and process each view that was updated
    // last frame:
    UINT ViewCount = m_SamplingViews.size();
    for( UINT i = 0; i < ViewCount; ++i )
    {
        if( m_SamplingViews[i].RenderFrameIndex != LastFrameIndex )
        {
            CollectViewSamples( m_SamplingViews[i] );
        }
    }

    // Update the tracked tile collection, scheduling them for loading and unmapping as necessary:
    UpdateTileStates();

    m_ResidencyStats.NumTilesUnused = m_TileRecycleQueue.size();
}

//--------------------------------------------------------------------------------------
// Name: TitleResidencyManager::CreateResourceConstant
// Desc: Creates a single shader constant for drawing residency samples for a given
//       resource ID.
//--------------------------------------------------------------------------------------
VOID TitleResidencyManager::CreateResourceConstant( ResourceSetID RSID, XMFLOAT4* pConstant )
{
    pConstant->x = (FLOAT)RSID / 255.0f;
    pConstant->y = 1.0f / m_fGradientScalingFactor;
    pConstant->z = 0.0f;
}

//--------------------------------------------------------------------------------------
// Name: TitleResidencyManager::SetResidencyRenderTargets
// Desc: Sets the two rendertargets and depth stencil for residency sample view rendering.
//--------------------------------------------------------------------------------------
VOID TitleResidencyManager::SetResidencyRenderTargets( D3DDevice* pd3dDevice )
{
    pd3dDevice->SetRenderTarget( 0, m_pResidencyRenderTargetUVGradientID );
    pd3dDevice->SetRenderTarget( 1, m_pResidencyRenderTargetExtendedUVSlice );
    pd3dDevice->SetRenderTarget( 2, NULL );
    pd3dDevice->SetRenderTarget( 3, NULL );
    pd3dDevice->SetDepthStencilSurface( m_pResidencyDepthStencil );

    pd3dDevice->SetViewport( &m_ResidencyViewport );

    pd3dDevice->SetRenderState( D3DRS_ZENABLE, TRUE );
    pd3dDevice->SetRenderState( D3DRS_ZWRITEENABLE, TRUE );
    pd3dDevice->SetRenderState( D3DRS_ZFUNC, D3DCMP_GREATEREQUAL );
    pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );
    pd3dDevice->SetRenderState( D3DRS_ALPHATESTENABLE, FALSE );
    pd3dDevice->SetRenderState( D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_ALL );

    pd3dDevice->Clear( 0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL, 0, 0.0f, 0 );
}

//--------------------------------------------------------------------------------------
// Name: TitleResidencyManager::BeginResidencyView
// Desc: Selects a residency sample view to render to, and returns its index.  Also sets
//       rendertargets for rendering.
//--------------------------------------------------------------------------------------
UINT TitleResidencyManager::BeginResidencyView( D3DDevice* pd3dDevice )
{
    m_CurrentViewIndex = m_NextViewIndex;
    m_NextViewIndex = ( m_NextViewIndex + 1 ) % m_SamplingViews.size();

    SetResidencyRenderTargets( pd3dDevice );

    return m_CurrentViewIndex;
}

//--------------------------------------------------------------------------------------
// Name: TitleResidencyManager::EndResidencyView
// Desc: Finishes a residency view render by resolving both rendertargets to the view
//       textures.
//--------------------------------------------------------------------------------------
VOID TitleResidencyManager::EndResidencyView( D3DDevice* pd3dDevice, UINT ViewID )
{
    ASSERT( ViewID == m_CurrentViewIndex );
    SamplingView& View = m_SamplingViews[ViewID];
    pd3dDevice->Resolve( D3DRESOLVE_RENDERTARGET0, NULL, View.pTextureUVGradientID, NULL, 0, 0, NULL, 0, 0, NULL );
    pd3dDevice->Resolve( D3DRESOLVE_RENDERTARGET1, NULL, View.pTextureExtendedUVSlice, NULL, 0, 0, NULL, 0, 0, NULL );
    View.RenderFrameIndex = m_FrameIndex;
    m_CurrentViewIndex = (UINT)-1;
}

//--------------------------------------------------------------------------------------
// Name: TitleResidencyManager::CreateResourceSet
// Desc: Given arrays of tiled resources and corresponding tile loaders, this method
//       creates a resource set.  The resulting resource set ID is used to identify these
//       resources in a residency sample view.
//--------------------------------------------------------------------------------------
ResourceSetID TitleResidencyManager::CreateResourceSet( const D3DTiledTexture** ppResources, ITileLoader** ppTileLoaders, UINT ResourceCount )
{
    if( ResourceCount == 0 || ppResources == NULL )
    {
        return 0;
    }

    // Cannot have more than 255 resource sets:
    if( m_ResourceSets.size() > g_MaxResourceSetID )
    {
        return 0;
    }

    for( UINT i = 0; i < ResourceCount; ++i )
    {
        ASSERT( ppResources[i] != NULL );
    }

    // Create a new resource set:
    ResourceSet NewSet = { 0 };
    NewSet.ID = (ResourceSetID)m_ResourceSets.size();
    NewSet.ResourceCount = ResourceCount;

    NewSet.ppResources = new D3DTiledTexture*[ResourceCount];
    memcpy( NewSet.ppResources, ppResources, sizeof(D3DTiledTexture*) * ResourceCount );

    NewSet.ppTileLoaders = new ITileLoader*[ResourceCount];
    memcpy( NewSet.ppTileLoaders, ppTileLoaders, sizeof(ITileLoader*) * ResourceCount );

    for( UINT i = 0; i < ResourceCount; ++i )
    {
        ITileLoader* pTileLoader = NewSet.ppTileLoaders[i];

        // Ensure that each tile loader has been initialized:
        if( pTileLoader != NULL )
        {
            pTileLoader->m_pTilePool = m_pTilePool;

            if( pTileLoader->m_pLoaderContexts[0] == NULL )
            {
                for( UINT j = 0; j < ARRAYSIZE(pTileLoader->m_pLoaderContexts); ++j )
                {
                    pTileLoader->m_pLoaderContexts[j] = pTileLoader->CreateThreadContext();
                }
                for( UINT j = 0; j < ARRAYSIZE(pTileLoader->m_pUnloaderContexts); ++j )
                {
                    pTileLoader->m_pUnloaderContexts[j] = pTileLoader->CreateThreadContext();
                }
            }
        }

        // Make sure the smallest mip level(s) of the resource never leaves residency
        D3DTiledTexture* pResource = NewSet.ppResources[i];

        if( pResource != NULL )
        {
            // Compute the highest mip level for the tiled resource:
            UINT LastMipLevel = pResource->GetLevelCount() - 1;

            if( pResource->GetType() == D3DSRTYPE_ARRAYTEXTURE )
            {
                D3DTiledArrayTexture* pSATexture = (D3DTiledArrayTexture*)pResource;
                // Loop through the array slices:
                UINT SliceCount = pSATexture->GetArraySize();
                for( UINT k = 0; k < SliceCount; ++k )
                {
                    // Find the virtual address of the single tile for this slice:
                    D3DTILED_VIRTUAL_ADDRESS VTileID = pSATexture->GetTileVirtualAddress( k, LastMipLevel, 0.5f, 0.5f );

                    // Create or retrieve the tracked tile entry for this virtual address:
                    TrackedTile* pTP = IncrementSampleCount( VTileID, 0.5f, 0.5f, k, LastMipLevel, pResource, pTileLoader, 0 );

                    // Set the pinned tile flag to TRUE, so the tile will be loaded high priority and will never be unmapped:
                    pTP->ID.PinnedTile = TRUE;                    
                }
            }
            else
            {
                // Find the virtual address of the single tile for this slice:
                D3DTILED_VIRTUAL_ADDRESS VTileID = pResource->GetTileVirtualAddress( LastMipLevel, 0.5f, 0.5f );

                // Create or retrieve the tracked tile entry for this virtual address:
                TrackedTile* pTP = IncrementSampleCount( VTileID, 0.5f, 0.5f, 0, LastMipLevel, pResource, pTileLoader, 0 );

                // Set the pinned tile flag to TRUE, so the tile will be loaded high priority and will never be unmapped:
                pTP->ID.PinnedTile = TRUE;
            }
        }
    }

    m_ResourceSets.push_back( NewSet );
    return NewSet.ID;
}

//--------------------------------------------------------------------------------------
// Name: ComputeViewPositionScore
// Desc: Computes a score that is based on how close the X and Y coordinates are to the
//       center of the given viewport width and height.
//--------------------------------------------------------------------------------------
inline UINT ComputeViewPositionScore( UINT X, UINT Y, UINT Width, UINT Height )
{
    INT XCenter = (INT)X - (INT)( Width >> 1 );
    INT YCenter = (INT)Y - (INT)( Height >> 1 );

    XCenter >>= 3;
    YCenter >>= 3;

    UINT Score = ( XCenter * XCenter ) + ( YCenter * YCenter );

    return Score;
}

//--------------------------------------------------------------------------------------
// Name: TitleResidencyManager::CollectViewSamples
// Desc: Locks a pair of residency view staging textures for reading, and selects a 
//       statistical sample of the texels to convert into residency samples.  Each selected
//       sample is processed and possibly converted into a tracked tile.
//--------------------------------------------------------------------------------------
VOID TitleResidencyManager::CollectViewSamples( SamplingView& View )
{
    // Lock the textures for reading:
    D3DLOCKED_RECT LockRectUVGradientID;
    View.pTextureUVGradientID->LockRect( 0, &LockRectUVGradientID, NULL, D3DLOCK_READONLY );
    const BYTE* pUVGradientIDBits = (const BYTE*)LockRectUVGradientID.pBits;

    D3DLOCKED_RECT LockRectExtendedUVSlice;
    View.pTextureExtendedUVSlice->LockRect( 0, &LockRectExtendedUVSlice, NULL, D3DLOCK_READONLY );
    const BYTE* pExtendedUVSliceBits = (const BYTE*)LockRectExtendedUVSlice.pBits;

    // Set up a hashing function to compute a "random" but repeating scan pattern
    // within a 64x4 window.
    // These constants were selected to grab 1/256 of the samples each frame, and to 
    // move the selection around as much as possible to get uniform coverage over
    // time.
    const UINT XFrequency = 64;
    const UINT XMultiplier = 27;
    const UINT YFrequency = 4;

    UINT Y = m_FrameIndex % YFrequency;

    for( ; Y < g_ResidencyViewHeight; Y += YFrequency )
    {
        // Compute the horizontal offset using the hashing constants:
        UINT Offset = ( ( m_FrameIndex + Y ) * XMultiplier ) % XFrequency;

        for( UINT X = 0; X < g_ResidencyViewWidth; X += XFrequency )
        {
            // Compute a view position score from the original X and Y coordinates of the sample relative to
            // the dimensions of the residency sample view:
            UINT ViewPositionScore = ComputeViewPositionScore( X + Offset, Y, g_ResidencyViewWidth, g_ResidencyViewHeight );

            // Determine the byte offset to the sample:
            UINT ByteOffset = XGAddress2DTiledOffset( X + Offset, Y, g_ResidencyViewWidth, 4 ) * sizeof(XMUBYTEN4);

            // Send the sample for processing:
            ProcessSample( *(const XMUBYTEN4*)( pUVGradientIDBits + ByteOffset ), *(const XMUBYTEN4*)( pExtendedUVSliceBits + ByteOffset ), ViewPositionScore );
        }
    }

    // Unlock the textures:
    View.pTextureExtendedUVSlice->UnlockRect( 0 );
    View.pTextureUVGradientID->UnlockRect( 0 );
}

//--------------------------------------------------------------------------------------
// Name: ComputeTextureLOD
// Desc: Given a tiled texture and a UV gradient from the residency sample render, this 
//       method computes a fractional mip LOD value.
//--------------------------------------------------------------------------------------
inline UINT ComputeTextureLOD( D3DTiledTexture* pResource, const FLOAT fGradient )
{
    // Compute the maximum dimension of the texture's base level (in texels):
    D3DTILED_SURFACE_DESC SurfDesc;
    pResource->GetLevelDesc( 0, &SurfDesc );
    FLOAT BaseMaxSize = (FLOAT)max( SurfDesc.TexelWidth, SurfDesc.TexelHeight );

    // Compute the amount of texels covered by the gradient value:
    FLOAT fTexelPitch = fGradient * BaseMaxSize * g_TexelPitchMultiplier;

    // Convert the texel count to a LOD value, using a base 2 logarithm:
    // frexpf is used to get a fast rounded down log base 2 of a floating point value
    INT Exponent;
    frexpf( fTexelPitch, &Exponent );

    // Clamp the LOD value to 0 (no negative values allowed):
    return (UINT)max( 0, Exponent );
}

//--------------------------------------------------------------------------------------
// Name: TitleResidencyManager::ProcessSample
// Desc: Given two raw 32 bit samples from the residency sample views, convert the bits
//       into (U, V, mip level, array slice) locations within a set of resources.  For
//       each of those combinations, pass the data along to be converted into tracked
//       tiles.
//--------------------------------------------------------------------------------------
VOID TitleResidencyManager::ProcessSample( const XMUBYTEN4& UVGradientIDSample, const XMUBYTEN4& ExtendedUVSliceSample, const UINT ViewPositionScore )
{
    // The X value on the UVGradientID sample is the resource set ID:
    ResourceSetID RSID = (ResourceSetID)UVGradientIDSample.x;
    if( RSID == 0 || RSID >= m_ResourceSets.size() )
    {
        return;
    }

    // Retrieve the resource set:
    ResourceSet& RSet = m_ResourceSets[RSID];
    ASSERT( RSet.ID == RSID );

    // Decode the UV gradient from the W component of the UVGradientID sample:
    const FLOAT fEncodedGradient = (FLOAT)UVGradientIDSample.w / 255.0f;
    const FLOAT fLogGradient = fEncodedGradient * m_fGradientScalingFactor;
    const FLOAT fGradient = expf( fLogGradient );

    // Decode the UV whole number components from the YZ components of the ExtendedUVSlice sample:
    const FLOAT fTexWholeU = (FLOAT)( (INT)ExtendedUVSliceSample.y - 128 );
    const FLOAT fTexWholeV = (FLOAT)( (INT)ExtendedUVSliceSample.z - 128 );

    // Compute the whole + fractional texture UV from the YZ components of the UVGradientID sample:
    FLOAT fTexU = fTexWholeU + (FLOAT)UVGradientIDSample.y / 255.0f;
    FLOAT fTexV = fTexWholeV + (FLOAT)UVGradientIDSample.z / 255.0f;

    const FLOAT fTexSliceNormalized = (FLOAT)ExtendedUVSliceSample.w / 255.0f;

    UINT ResourceCount = RSet.ResourceCount;
    for( UINT i = 0; i < ResourceCount; ++i )
    {
        // Retrieve the resource pointer.
        D3DTiledTexture* pResource = RSet.ppResources[i];
        if( pResource == NULL )
        {
            continue;
        }

        ASSERT( pResource != NULL );
        ITileLoader* pTileLoader = RSet.ppTileLoaders[i];

        // The Low LOD is the largest whole number less than the fractional LOD:
        UINT LowLOD = ComputeTextureLOD( pResource, fGradient );
         
        if( pResource->GetType() == D3DSRTYPE_ARRAYTEXTURE )
        {
            D3DTiledArrayTexture* pSATexture = (D3DTiledArrayTexture*)pResource;

            // Decode the array slice if the texture has array slices:
            UINT SliceIndex = (UINT)( fTexSliceNormalized * pSATexture->GetArraySize() );

            // If the resource is quilted, convert the extended (0..N) UV coordinates to
            // normalized (0..1) UV and array slice index from the quilting configuration:
            pSATexture->ConvertQuiltUVToArrayUVSlice( &fTexU, &fTexV, &SliceIndex );

            // Loop over the LOD levels up to the maximum LOD index:
            for( UINT LOD = LowLOD; LOD < 9; ++LOD )
            {

                // Determine the virtual address of the tile, given the subresource index and the normalized UV coordinates:
                D3DTILED_VIRTUAL_ADDRESS VTileID = pSATexture->GetTileVirtualAddress( SliceIndex, LOD, fTexU, fTexV );

                // If we have an invalid result, skip this resource.
                if( VTileID == D3DTILED_INVALID_VIRTUAL_ADDRESS )
                {
                    break;
                }

                // Pass the virtual address and all other computed information on to the next step for processing:
                IncrementSampleCount( VTileID, fTexU, fTexV, SliceIndex, LOD, pResource, pTileLoader, ViewPositionScore );
            }
        }
        else
        {

            // Determine the number of mip levels in the resource:
            UINT MipLevelCount = pResource->GetLevelCount();

            // Loop over the LOD levels up to the maximum LOD index:
            for( UINT LOD = LowLOD; LOD < MipLevelCount; ++LOD )
            {

                // Determine the virtual address of the tile, given the subresource index and the normalized UV coordinates:
                D3DTILED_VIRTUAL_ADDRESS VTileID = pResource->GetTileVirtualAddress( LOD, fTexU, fTexV );

                // If we have an invalid result, skip this resource.
                if( VTileID == D3DTILED_INVALID_VIRTUAL_ADDRESS )
                {
                    break;
                }

                // Pass the virtual address and all other computed information on to the next step for processing:
                IncrementSampleCount( VTileID, fTexU, fTexV, 0, LOD, pResource, pTileLoader, ViewPositionScore );
            }
        }
    }
}

//--------------------------------------------------------------------------------------
// Name: TitleResidencyManager::IncrementSampleCount
// Desc: Given a virtual tile address and all of the sample data that led to that virtual
//       address, find or create a tracked tile entry, populate it, and increment its
//       sample count.
//--------------------------------------------------------------------------------------
TitleResidencyManager::TrackedTile* TitleResidencyManager::IncrementSampleCount( D3DTILED_VIRTUAL_ADDRESS VTileID, FLOAT TexU, FLOAT TexV, UINT SliceIndex, UINT MipLevel, D3DTiledTexture* pResource, ITileLoader* pTileLoader, const UINT ViewPositionScore )
{
    // Validate the virtual tile address:
    if( VTileID == D3DTILED_INVALID_VIRTUAL_ADDRESS )
    {
        return NULL;
    }

    TrackedTile* pTP = NULL;

    // Use a one-element cache to reduce hashtable lookups:
    static D3DTILED_VIRTUAL_ADDRESS s_LastID = D3DTILED_INVALID_VIRTUAL_ADDRESS;
    static TrackedTile* s_pLastTile = NULL;

    if( VTileID == s_LastID )
    {
        pTP = s_pLastTile;
    }
    else
    {
        // Search for tile in hash table:
        TrackedTileMap::iterator iter = m_TrackedTileMap.find( VTileID );
        if( iter != m_TrackedTileMap.end() )
        {
            pTP = iter->second;
        }
        else
        {
            // Tile not found; create a new tile.
            pTP = AddVirtualTile( VTileID, TexU, TexV, SliceIndex, MipLevel, pResource, pTileLoader, ViewPositionScore );
        }
    }

    ASSERT( pTP != NULL );

    // Increment & update counters for this tile:
    pTP->SampleCount++;
    pTP->LastTimeSeen = m_CurrentFrameTime;
    pTP->ViewPositionScore = ViewPositionScore;

    // If the tile is scheduled for unmapping, change it back to Seen:
    if( pTP->State == TPS_Unmapped )
    {
        pTP->State = TPS_Seen;
    }

    // Update our one-element cache:
    s_LastID = VTileID;
    s_pLastTile = pTP;

    return pTP;
}

//--------------------------------------------------------------------------------------
// Name: TitleResidencyManager::AddVirtualTile
// Desc: Adds a new virtual tile entry to the title residency manager tile tracking lists.
//       This method does not check if the tile is already being tracked; that is the
//       caller's responsibility.
//--------------------------------------------------------------------------------------
TitleResidencyManager::TrackedTile* TitleResidencyManager::AddVirtualTile( D3DTILED_VIRTUAL_ADDRESS VTileID, FLOAT TexU, FLOAT TexV, UINT SliceIndex, UINT MipLevel, D3DTiledTexture* pResource, ITileLoader* pTileLoader, const UINT ViewPositionScore )
{
    ASSERT( VTileID != D3DTILED_INVALID_VIRTUAL_ADDRESS );

    // See if we have a tracked tile entry to reuse, instead of allocating a new one:
    TrackedTile* pTP = NULL;
    if( !m_TrackedTileFreeList.empty() )
    {
        // We can reuse a tracked tile entry:
        pTP = m_TrackedTileFreeList.top();
        m_TrackedTileFreeList.pop();
    }
    else
    {
        // We must allocate a new tracked tile entry:
        pTP = new TrackedTile();
    }
    ASSERT( pTP != NULL );

    // Fill in the tracked tile entry:
    pTP->ID.pResource = pResource;
    pTP->ID.VTileID = VTileID;
    pTP->ID.TileID = D3DTILED_INVALID_PHYSICAL_ADDRESS;
    pTP->ID.U = TexU;
    pTP->ID.V = TexV;
    pTP->ID.MipLevel = (USHORT)MipLevel;
    pTP->ID.ArraySlice = (USHORT)SliceIndex;
    pTP->ID.PinnedTile = FALSE;
    pTP->LastTimeSeen = m_CurrentFrameTime;
    pTP->SampleCount = 0;
    pTP->ViewPositionScore = ViewPositionScore;
    pTP->State = TPS_Seen;
    pTP->pTileLoader = pTileLoader;

    pTP->InsertPriority = 0;
    pTP->CurrentPriority = 0;

    // Add the tracked tile entry to the map, using the virtual address:
    m_TrackedTileMap[VTileID] = pTP;

    // Add the tracked tile entry to the end of the sorted list (the list will be re-sorted once per frame):
    m_TrackedTileSortList.push_back( pTP );

    return pTP;
}

//--------------------------------------------------------------------------------------
// Name: TitleResidencyManager::TrackedTileSortPredicate
// Desc: Sorting predicate for sorting the tracked tile list.  The sort predicate uses
//       the last time seen and the sample count for sorting.
//--------------------------------------------------------------------------------------
bool TitleResidencyManager::TrackedTileSortPredicate( const TrackedTile* pA, const TrackedTile* pB )
{
    if( pA->LastTimeSeen != pB->LastTimeSeen )
    {
        return pA->LastTimeSeen < pB->LastTimeSeen;
    }
    return pA->SampleCount < pB->SampleCount;
}

//--------------------------------------------------------------------------------------
// Name: TitleResidencyManager::ComputePriority
// Desc: Computes a priority score for a tracked tile, given a variety of factors.
//       Lower scores cause tiles to be loaded before tiles with higher scores.
//--------------------------------------------------------------------------------------
UINT TitleResidencyManager::ComputePriority( TrackedTile* pTP ) const
{
    // Tiles that are not currently visible are given a huge penalty:
    const UINT CurrentlyVisibleScore = ( pTP->LastTimeSeen == m_CurrentFrameTime ) ? 0 : 100000;

    // Higher mip levels are loaded first (this causes less popping onscreen):
    const UINT MipScore = max( 0, ( 8 - (INT)pTP->ID.MipLevel ) ) * 1000;

    // Tiles closer to the center of the render view are loaded before tiles on the edges
    // of the render view:
    const UINT ViewPositionScore = pTP->ViewPositionScore * 10;

    // Accumulate the scores and return:
    return MipScore + ViewPositionScore + CurrentlyVisibleScore;
}

//--------------------------------------------------------------------------------------
// Name: TitleResidencyManager::UpdateTileStates
// Desc: This method is executed once a frame to look over all tracked tiles and determine
//       if each tile needs to be pushed forward in the state machine.  Seen tiles are
//       queued for load, loaded tiles are queued for unmapping, and unmapped tiles are
//       destroyed.  This is also where most of the per-frame statistics are gathered.
//--------------------------------------------------------------------------------------
VOID TitleResidencyManager::UpdateTileStates()
{
    // Sort the tracked tile list:
    m_TrackedTileSortList.sort( TrackedTileSortPredicate );

    // Reset some of the stats counters:
    m_ResidencyStats.NumTilesQueuedForLoad = 0;
    m_ResidencyStats.NumTilesLoaded = 0;
    m_ResidencyStats.NumTilesTracked = 0;
    m_ResidencyStats.NumTilesUnused = 0;

    TrackedTileSortList::iterator iter = m_TrackedTileSortList.begin();
    TrackedTileSortList::iterator end = m_TrackedTileSortList.end();

    // UnmapCount is a circuit breaker that allows us to crudely reclaim a certain amount
    // of physical tiles per frame when we really, really need them:
    UINT UnmapCount = 0;
    if( m_NeedTilesNow )
    {
        UnmapCount = 10;
    }

    // Loop through the sorted list:
    while( iter != end )
    {
        TrackedTile* pTP = *iter;
        
        // Obtain the next iterator position, so we can delete from the list while we are iterating through it:
        TrackedTileSortList::iterator next_iter = iter;
        ++next_iter;

        // Increment the tiles tracked counter:
        ++m_ResidencyStats.NumTilesTracked;

        switch( pTP->State )
        {
        case TPS_Seen:
            // The residency sampler has seen this tile.  Once the sample count reaches 10, queue it for loading:
            if( pTP->SampleCount > 10 || pTP->ID.PinnedTile )
            {
                QueueTileForLoadAndMap( pTP, TRUE );
            }
            else if( ( m_CurrentFrameTime - pTP->LastTimeSeen ) >= ( 5 * 1000 ) )
            {
                // The residency sampler saw this tile, but we haven't seen it since for 5 seconds.
                // Set it to unmapped so it will be removed from tracking next frame:
                pTP->State = TPS_Unmapped;
            }
            break;
        case TPS_QueuedForLoad:
            ++m_ResidencyStats.NumTilesQueuedForLoad;

            // Determine if we need to requeue the request due to a lower priority.
            // This happens often; a tile is initially observed at the edge of the screen, giving
            // it a relatively low priority (high priority value) at the time that the request
            // is added to the load queue.  However, the camera can move while the request is 
            // queued, elevating the request's priority (lowering the CurrentPriority value).
            // If the current priority drops to less than half of the priority value at insertion
            // time, we will re-queue the request at the updated priority value.
            // Note that this will be a duplicate request in the queue for the same tile; care must
            // be taken in the loader thread to discard duplicate requests when they are encountered.
            pTP->CurrentPriority = ComputePriority( pTP );
            if( pTP->CurrentPriority <= ( pTP->InsertPriority >> 1 ) )
            {
                pTP->InsertPriority = pTP->CurrentPriority;
                QueueTileForLoadAndMap( pTP, FALSE );
            }
            break;
        case TPS_Loading:
            ++m_ResidencyStats.NumTilesQueuedForLoad;
            break;
        case TPS_LoadedAndMapped:
            ++m_ResidencyStats.NumTilesLoaded;

            // Determine if we need to unload a currently loaded tile.  UnmapCount is a circuit breaker that
            // limits the amount of unload requests per frame.
            if( IsTrackedTileExpired( pTP ) && UnmapCount > 0 )
            {
                ASSERT( pTP->ID.PinnedTile == FALSE );
                QueueTileForUnmap( pTP );
                --UnmapCount;
            }
            break;
        case TPS_Unmapped:
            {
                // This tile is fully unmapped.  Remove it from the hash table and the sort list.
                ASSERT( pTP->ID.TileID == D3DTILED_INVALID_PHYSICAL_ADDRESS );
                ASSERT( pTP->ID.PinnedTile == FALSE );

                TrackedTileMap::iterator tpmiter = m_TrackedTileMap.find( pTP->ID.VTileID );
                if( tpmiter != m_TrackedTileMap.end() )
                {
                    m_TrackedTileMap.erase( tpmiter );
                }
                m_TrackedTileSortList.erase( iter );

                m_TrackedTileFreeList.push( pTP );
                pTP = NULL;
            }
            break;
        }

        // Go to the next item in the list:
        iter = next_iter;
    }

    // The residency stats includes a boolean that reflects whether we are currently waiting 
    // for physical tiles to be freed:
    m_ResidencyStats.OutOfPhysicalTiles = m_NeedTilesNow;
}

//--------------------------------------------------------------------------------------
// Name: TitleResidencyManager::QueueTileForLoadAndMap
// Desc: Adds a tracked tile to the loader priority queue, after optionally recomputing 
//       its load priority.
//--------------------------------------------------------------------------------------
VOID TitleResidencyManager::QueueTileForLoadAndMap( TrackedTile* pTP, BOOL RecomputePriority )
{
    // a tile ready for load queueing should not have an assigned physical tile ID
    if( pTP->ID.TileID != D3DTILED_INVALID_PHYSICAL_ADDRESS )
    {
        return;
    }

    // Optionally recompute load priority:
    if( RecomputePriority )
    {
        pTP->State = TPS_QueuedForLoad;
        pTP->CurrentPriority = ComputePriority( pTP );
        pTP->InsertPriority = pTP->CurrentPriority;
    }

    // Add tile to the loader queue:
    m_LoadQueue.Add( pTP->InsertPriority, pTP );

    // Signal to the loader threads that the queue is not empty:
    SetEvent( m_hLoaderEvent );
}

//--------------------------------------------------------------------------------------
// Name: TitleResidencyManager::QueueTileForUnmap
// Desc: Adds a tracked tile to the unmap queue.
//--------------------------------------------------------------------------------------
VOID TitleResidencyManager::QueueTileForUnmap( TrackedTile* pTP )
{
    // Change tile state:
    pTP->State = TPS_QueuedForUnmap;

    // Add tile to unmap queue:
    m_UnmapQueue.Add( pTP );

    // Signal to unloader thread that the queue is not empty:
    SetEvent( m_hUnloaderEvent );
}

//--------------------------------------------------------------------------------------
// Name: TitleResidencyManager::IsTrackedTileExpired
// Desc: Determines if a tracked tile can be unmapped immediately if necessary.
//--------------------------------------------------------------------------------------
BOOL TitleResidencyManager::IsTrackedTileExpired( TrackedTile* pTP ) const
{
    if( pTP->ID.PinnedTile )
    {
        return FALSE;
    }
    if( m_NeedTilesNow )
    {
        // Emergency expire tiles that haven't been seen for a half second or more
        return ( m_CurrentFrameTime - pTP->LastTimeSeen ) >= 500;
    }
    else
    {
        return FALSE;
    }
}

//--------------------------------------------------------------------------------------
// Name: TitleResidencyManager::RegisterTileActivityHandler
// Desc: Adds a tile activity handler to the list.
//--------------------------------------------------------------------------------------
VOID TitleResidencyManager::RegisterTileActivityHandler( ITileActivityHandler* pHandler )
{
    m_TileActivityHandlers.push_back( pHandler );
}

//--------------------------------------------------------------------------------------
// Name: TitleResidencyManager::NotifyTileActivity
// Desc: Sends a loaded or unloaded notification to each of the tile activity handlers.
//--------------------------------------------------------------------------------------
VOID TitleResidencyManager::NotifyTileActivity( const TrackedTileID* pTileID, BOOL Loaded ) const
{
    UINT HandlerCount = m_TileActivityHandlers.size();
    for( UINT i = 0; i < HandlerCount; ++i )
    {
        if( Loaded )
        {
            m_TileActivityHandlers[i]->TileLoaded( pTileID );
        }
        else
        {
            m_TileActivityHandlers[i]->TileUnloaded( pTileID );
        }
    }
}

const WCHAR* GetFormatName( const D3DFORMAT Format )
{
    UINT GpuFormat = ( Format & D3DFORMAT_TEXTUREFORMAT_MASK ) >> D3DFORMAT_TEXTUREFORMAT_SHIFT;
    switch( GpuFormat )
    {
    case GPUTEXTUREFORMAT_1_REVERSE:
    case GPUTEXTUREFORMAT_1:
        // 1bpp
        return L"1";
    case GPUTEXTUREFORMAT_8:
    case GPUTEXTUREFORMAT_8_A:
    case GPUTEXTUREFORMAT_8_B:
        // 8bpp
        return L"L8/A8";
    case GPUTEXTUREFORMAT_1_5_5_5:
        return L"A1R5G5B5";
    case GPUTEXTUREFORMAT_5_6_5:
        return L"R5G6B5";
    case GPUTEXTUREFORMAT_6_5_5:
        return L"R6G5B5";
    case GPUTEXTUREFORMAT_8_8:
        return L"R8G8";
    case GPUTEXTUREFORMAT_Cr_Y1_Cb_Y0_REP:
    case GPUTEXTUREFORMAT_Y1_Cr_Y0_Cb_REP:
    case GPUTEXTUREFORMAT_16_16_EDRAM:
        break;
    case GPUTEXTUREFORMAT_4_4_4_4:
        return L"A4R4G4B4";
    case GPUTEXTUREFORMAT_16:
    case GPUTEXTUREFORMAT_16_EXPAND:
        return L"R16/L16";
    case GPUTEXTUREFORMAT_16_FLOAT:
        // 16bpp
        return L"R16F";
    case GPUTEXTUREFORMAT_8_8_8_8:
    case GPUTEXTUREFORMAT_8_8_8_8_A:
    case GPUTEXTUREFORMAT_8_8_8_8_AS_16_16_16_16:
    case GPUTEXTUREFORMAT_8_8_8_8_GAMMA_EDRAM:
        return L"A8R8G8B8";
    case GPUTEXTUREFORMAT_2_10_10_10:
    case GPUTEXTUREFORMAT_2_10_10_10_AS_16_16_16_16:
        return L"A2R10G10B10";
    case GPUTEXTUREFORMAT_2_10_10_10_FLOAT_EDRAM:
        return L"A2R10G10B10F";
    case GPUTEXTUREFORMAT_10_11_11:
    case GPUTEXTUREFORMAT_11_11_10:
    case GPUTEXTUREFORMAT_24_8:
    case GPUTEXTUREFORMAT_24_8_FLOAT:
    case GPUTEXTUREFORMAT_10_11_11_AS_16_16_16_16:
    case GPUTEXTUREFORMAT_11_11_10_AS_16_16_16_16:
        break;
    case GPUTEXTUREFORMAT_16_16:
    case GPUTEXTUREFORMAT_16_16_EXPAND:
        return L"R16G16";
    case GPUTEXTUREFORMAT_16_16_FLOAT:
        return L"R16G16F";
    case GPUTEXTUREFORMAT_32:
        return L"R32";
    case GPUTEXTUREFORMAT_32_FLOAT:
        return L"R32F";
    case GPUTEXTUREFORMAT_DXT1:
    case GPUTEXTUREFORMAT_DXT1_AS_16_16_16_16:
        // BC1
        return L"DXT1";
    case GPUTEXTUREFORMAT_DXT2_3:
    case GPUTEXTUREFORMAT_DXT2_3_AS_16_16_16_16:
        return L"DXT2/3";
    case GPUTEXTUREFORMAT_DXT4_5:
    case GPUTEXTUREFORMAT_DXT4_5_AS_16_16_16_16:
        // BC2 / BC3
        return L"DXT4/5";
    case GPUTEXTUREFORMAT_16_16_16_16_EDRAM:
    case GPUTEXTUREFORMAT_16_16_16_16:
    case GPUTEXTUREFORMAT_16_16_16_16_EXPAND:
        return L"A16R16G16B16";
    case GPUTEXTUREFORMAT_16_16_16_16_FLOAT:
        return L"A16R16G16B16F";
    case GPUTEXTUREFORMAT_32_32:
        return L"R32G32";
    case GPUTEXTUREFORMAT_32_32_FLOAT:
        // 64bpp
        return L"R32G32F";
    case GPUTEXTUREFORMAT_32_32_32_32:
        return L"A32R32G32B32";
    case GPUTEXTUREFORMAT_32_32_32_32_FLOAT:
        // 128bpp
        return L"A32R32G32B32F";
    case GPUTEXTUREFORMAT_DXN:
        // BCN
        return L"DXN";
    case GPUTEXTUREFORMAT_32_32_32_FLOAT:
        // 96bpp
        return L"R32G32B32F";
    case GPUTEXTUREFORMAT_DXT3A:
    case GPUTEXTUREFORMAT_DXT3A_AS_1_1_1_1:
        return L"DXT3A";
    case GPUTEXTUREFORMAT_DXT5A:
        return L"DXT5A";
    case GPUTEXTUREFORMAT_CTX1:
        return L"CTX1";
    case GPUTEXTUREFORMAT_32_AS_8:
    case GPUTEXTUREFORMAT_32_AS_8_8:
    case GPUTEXTUREFORMAT_16_MPEG:
    case GPUTEXTUREFORMAT_16_16_MPEG:
    case GPUTEXTUREFORMAT_8_INTERLACED:
    case GPUTEXTUREFORMAT_32_AS_8_INTERLACED:
    case GPUTEXTUREFORMAT_32_AS_8_8_INTERLACED:
    case GPUTEXTUREFORMAT_16_INTERLACED:
    case GPUTEXTUREFORMAT_16_MPEG_INTERLACED:
    case GPUTEXTUREFORMAT_16_16_MPEG_INTERLACED:
    default:
        // other
        break;
    }

    return L"UNKNOWN";
}

//--------------------------------------------------------------------------------------
// Name: TitleResidencyManager::DebugRenderResidencyView
// Desc: Renders the current residency view textures at the given location.
//--------------------------------------------------------------------------------------
VOID TitleResidencyManager::DebugRenderResidencyView( UINT XPos, UINT YPos )
{
    UINT CurrentViewIndex = ( m_NextViewIndex + m_SamplingViews.size() - 1 ) % m_SamplingViews.size();
    SamplingView& View = m_SamplingViews[CurrentViewIndex];

    if( View.pTextureUVGradientID != NULL )
    {
        D3DRECT TextureRect = { XPos, YPos, XPos + g_ResidencyViewWidth, YPos + g_ResidencyViewHeight };
        ATG::DebugDraw::DrawScreenSpaceTexturedRect( TextureRect, View.pTextureUVGradientID, FALSE );
        ATG::DebugDraw::DrawScreenSpaceRect( TextureRect, 2.0f, 0xFFFFFFFF );
    }

    if( View.pTextureExtendedUVSlice != NULL )
    {
        D3DRECT TextureRect = { XPos, YPos + g_ResidencyViewHeight + 10, XPos + g_ResidencyViewWidth, YPos + g_ResidencyViewHeight * 2 + 10 };
        ATG::DebugDraw::DrawScreenSpaceTexturedRect( TextureRect, View.pTextureExtendedUVSlice, FALSE );
        ATG::DebugDraw::DrawScreenSpaceRect( TextureRect, 2.0f, 0xFFFFFFFF );
    }
}

//--------------------------------------------------------------------------------------
// Name: TitleResidencyManager::DebugRenderTiles
// Desc: Renders the current state of all resources and tiles in the system, using the
//       ATG debug draw library.  A configurable Y offset allows the user to scroll
//       through the list of resources.
//--------------------------------------------------------------------------------------
UINT TitleResidencyManager::DebugRenderTiles( D3DDevice* pd3dDevice, ATG::Font* pFont, D3DTiledTexture* pExamineTexture, UINT TileSizePixels, INT YOffsetPixels )
{
    if( pExamineTexture == NULL )
    {
        return 0;
    }

    PIXBeginNamedEvent( 0, "Debug Render Tiles" );

    const UINT TileSize = TileSizePixels;
    const UINT MipSpacing = 4;
    const UINT XMargin = 100;
    const UINT TextWidth = 110;
    INT CurrentYPos = 100 + YOffsetPixels;

    UINT MaxXPos = 0;

    UINT LoadedTiles = 0;
    UINT TotalTiles = 0;

    TrackedTileSortList::iterator iter = m_TrackedTileSortList.begin();
    TrackedTileSortList::iterator end = m_TrackedTileSortList.end();
    while( iter != end )
    {
        TrackedTile* pTP = *iter;
        ++iter;

        D3DTiledTexture* pResource = pTP->ID.pResource;
        if( pResource != pExamineTexture )
        {
            continue;
        }

        UINT YPos = CurrentYPos;
        UINT XPos = XMargin + TextWidth;

        UINT MipLevel = pTP->ID.MipLevel;
        MipLevel = min( MipLevel, pResource->GetLevelCount() - 1 );

        UINT SliceIndex = pTP->ID.ArraySlice;

        D3DTILED_SURFACE_DESC MipDesc;
        ZeroMemory( &MipDesc, sizeof(MipDesc) );
        for( UINT i = 0; i <= MipLevel; ++i )
        {
            pResource->GetLevelDesc( i, &MipDesc );
            if( i < MipLevel )
            {
                XPos += MipDesc.TileWidth * TileSize + MipSpacing;
            }
            if( i == 0 && SliceIndex > 0 )
            {
                YPos += ( MipDesc.TileHeight * TileSize + MipSpacing ) * SliceIndex;
            }
        }

        UINT TileX = (UINT)( ( (FLOAT)MipDesc.TexelWidth / (FLOAT)MipDesc.TileTexelWidth ) * pTP->ID.U );
        TileX = min( TileX, MipDesc.TileWidth - 1 );
        UINT TileY = (UINT)( ( (FLOAT)MipDesc.TexelHeight / (FLOAT)MipDesc.TileTexelHeight ) * pTP->ID.V );
        TileY = min( TileY, MipDesc.TileHeight - 1 );

        ASSERT( TileX < MipDesc.TileWidth );
        ASSERT( TileY < MipDesc.TileHeight );

        D3DRECT TileRect = { TileX * TileSize + XPos, TileY * TileSize + YPos, 0, 0 };
        TileRect.x2 = TileRect.x1 + TileSize;
        TileRect.y2 = TileRect.y1 + TileSize;

        D3DCOLOR TileColor = 0;
        switch( pTP->State )
        {
        case TPS_Seen:
            TileColor = 0xFF808080;
            break;
        case TPS_QueuedForLoad:
            {
                INT PriorityScore = max( 0, ( 1024 - (INT)pTP->InsertPriority ) );
                UINT PriorityColor = ( PriorityScore / 8 ) + 128;
                TileColor = D3DCOLOR_ARGB( 255, PriorityColor / 2, PriorityColor / 2, PriorityColor );
            }
            break;
        case TPS_Loading:
        case TPS_Unmapping:
            TileColor = 0xFFFFFF00;
            break;
        case TPS_LoadedAndMapped:
            ++LoadedTiles;
            TileColor = 0xFF80FF80;
            if( pTP->ID.TileID == D3DTILED_INVALID_PHYSICAL_ADDRESS )
            {
                TileColor = 0xFF60C060;
            }
            if( pTP->ID.PinnedTile )
            {
                TileColor = 0xFFFF80FF;
            }
            break;
        case TPS_QueuedForUnmap:
            TileColor = 0xFFFF8080;
            break;
        case TPS_Unmapped:
            TileColor = 0xFFFF0000;
            break;
        }

        ATG::DebugDraw::DrawScreenSpaceRect( TileRect, -1.0f, TileColor );
    }

    {
        UINT YPos = CurrentYPos;

        D3DTiledTexture* pResource = pExamineTexture;

        UINT SliceCount = 1;
        if( pResource->GetType() == D3DSRTYPE_ARRAYTEXTURE )
        {
            D3DTiledArrayTexture* pSATexture = (D3DTiledArrayTexture*)pResource;
            SliceCount = pSATexture->GetArraySize();
        }

        UINT MipCount = pResource->GetLevelCount();

        for( UINT Slice = 0; Slice < SliceCount; ++Slice )
        {
            UINT XPos = XMargin + TextWidth;

            UINT BaseHeightTiles = 0;

            for( UINT j = 0; j < MipCount; ++j )
            {
                D3DTILED_SURFACE_DESC MipDesc;
                pResource->GetLevelDesc( j, &MipDesc );

                TotalTiles += ( MipDesc.TileWidth * MipDesc.TileHeight );

                D3DRECT MipRect;
                MipRect.x1 = XPos;
                MipRect.y1 = YPos;
                MipRect.x2 = MipRect.x1 + MipDesc.TileWidth * TileSize;
                MipRect.y2 = MipRect.y1 + MipDesc.TileHeight * TileSize;

                ATG::DebugDraw::DrawScreenSpaceRect( MipRect, 1.0f, 0xFFFFFFFF );

                XPos += MipDesc.TileWidth * TileSize + MipSpacing;

                MaxXPos = max( MaxXPos, XPos );

                if( j == 0 )
                {
                    BaseHeightTiles = MipDesc.TileHeight;
                }
            }

            YPos += ( BaseHeightTiles * TileSize + MipSpacing );
        }
    }

    WCHAR strText[200];
    pFont->Begin();
    pFont->SetScaleFactors( 0.6f, 0.6f );

    {
        UINT YPos = CurrentYPos;
        UINT XPos = XMargin;

        D3DTiledTexture* pResource = pExamineTexture;

        D3DTILED_SURFACE_DESC SurfDesc;
        pResource->GetLevelDesc( 0, &SurfDesc );

        UINT MipLevelCount = pResource->GetLevelCount();

        UINT SliceCount = 1;
        UINT QuiltWidth = 1;
        UINT QuiltHeight = 1;
        if( pResource->GetType() == D3DSRTYPE_ARRAYTEXTURE )
        {
            D3DTiledArrayTexture* pSATexture = (D3DTiledArrayTexture*)pResource;
            SliceCount = pSATexture->GetArraySize();

            pSATexture->GetQuiltSize( &QuiltWidth, &QuiltHeight );
        }

        if( YPos > 0 && YPos < 720 )
        {
            WCHAR strQuilt[30] = L"";
            if( QuiltWidth > 1 || QuiltHeight > 1 )
            {
                swprintf_s( strQuilt, L"%d x %d quilt", QuiltWidth, QuiltHeight );
            }
            swprintf_s( strText, L"Texture:\n%s\n%d x %d\n%d levels\n%d slices\n%s\n%u tiles loaded\n%u tiles total", GetFormatName( SurfDesc.Format ), SurfDesc.TexelWidth, SurfDesc.TexelHeight, MipLevelCount, SliceCount, strQuilt, LoadedTiles, TotalTiles );
            pFont->DrawText( (FLOAT)XPos, (FLOAT)YPos, 0xFFFFFFFF, strText, ATGFONT_LEFT );
        }

        if( SliceCount > 1 )
        {
            UINT SliceXPos = XPos + TextWidth - 2;
            for( UINT Slice = 0; Slice < SliceCount; ++Slice )
            {
                UINT SliceYPos = YPos + Slice * ( SurfDesc.TileHeight * TileSize + MipSpacing );
                if( SliceYPos > 0 && SliceYPos < 720 )
                {
                    if( QuiltWidth > 1 || QuiltHeight > 1 )
                    {
                        swprintf_s( strText, L"%d\n(%d, %d)", Slice, Slice % QuiltWidth, Slice / QuiltWidth );
                    }
                    else
                    {
                        swprintf_s( strText, L"%d", Slice );
                    }
                    pFont->DrawText( (FLOAT)SliceXPos, (FLOAT)SliceYPos, 0xFFFFFFFF, strText, ATGFONT_RIGHT );
                }
            }
        }
    }

    pFont->SetScaleFactors( 1.0f, 1.0f );
    pFont->End();

    PIXEndNamedEvent();

    return MaxXPos;
}

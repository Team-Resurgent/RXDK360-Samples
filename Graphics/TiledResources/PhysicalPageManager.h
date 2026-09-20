//--------------------------------------------------------------------------------------
// PhysicalPageManager.h
//
// The core of the tiled resource runtime; this class manages the collection of typed
// page pools and resources, and coordinates operations between them, such as adding and
// removing pages, and mapping/unmapping pages.  It also executes internal housekeeping
// operations, such as updating border texels of neighboring pages when pages are mapped 
// and unmapped.
//
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#pragma once

#include "TiledResourceCommon.h"
#include "d3d9tiled.h"

namespace TiledRuntime
{
    // In the current version, you cannot map a physical page to more than one virtual location.
    static const UINT MAX_PAGE_LOCATIONS = 1;

    //--------------------------------------------------------------------------------------
    // Name: PhysicalPageLocation
    // Desc: A location that specifies an index within a typed page pool.
    //--------------------------------------------------------------------------------------
    struct PhysicalPageLocation
    {
        PageDataFormat m_Format;
        UINT m_PoolIndex;
    };

    //--------------------------------------------------------------------------------------
    // Name: PhysicalPageEntry
    // Desc: A struct that represents a single physical page within the physical page manager.
    //--------------------------------------------------------------------------------------
    struct PhysicalPageEntry
    {
        PhysicalPageID m_PageID;
        PhysicalPageLocation m_Location;
        INT m_RefCount;
    };

    typedef std::hash_map<PhysicalPageID, PhysicalPageEntry> PhysicalPageMasterIndex;
    typedef std::hash_map<VirtualPageID, PhysicalPageID> VirtualToPhysicalIndex;

    typedef std::hash_map<UINT, TiledResourceBase*> ResourceIndex;

    //--------------------------------------------------------------------------------------
    // Name: EmulationParameters
    // Desc: A struct that defines various attributes that are used to initialize the 
    //       physical page manager's software emulation of tiled resources.
    //--------------------------------------------------------------------------------------
    struct EmulationParameters
    {
        D3DFORMAT DefaultResourceFormat;
        UINT ExpectedPageUpdatesPerFrame;
    };

    //--------------------------------------------------------------------------------------
    // Name: PhysicalPageManagerDesc
    // Desc: Initialization parameters for the physical page manager.
    //--------------------------------------------------------------------------------------
    struct PhysicalPageManagerDesc
    {
        // The maximum number of physical pages that can be tracked by the physical page manager:
        UINT MaxPhysicalPages;

        // Software emulation parameters:
        EmulationParameters EmulationParams;
    };

    //--------------------------------------------------------------------------------------
    // Name: PhysicalPageManager
    // Desc: The physical page manager represents the interface to a physical page pool as 
    //       well as the virtual-to-physical page mapping tables.  Through the physical page
    //       manager, you can allocate and free physical pages, fill physical pages, and map
    //       or unmap virtual pages to physical pages.
    //--------------------------------------------------------------------------------------
    class PhysicalPageManager
    {
    protected:
        // Initialization parameters for the physical page manager:
        PhysicalPageManagerDesc m_Desc;

        // The map of each physical page and its attributes:
        PhysicalPageMasterIndex m_MasterIndex;

        // The ID of the next physical page that will be allocated:
        PhysicalPageID m_NextPageID;

        // The authoritative mapping of virtual pages to physical pages.
        // This data is also replicated in a different form in the tiled resources' index maps:
        VirtualToPhysicalIndex m_VirtualToPhysicalIndex;

        // One typed page pool for each page pool format:
        TypedPagePool* m_pTypedPagePools[PDF_COUNT];

        // A map of tiled resources, each with their own resource ID:
        ResourceIndex m_Resources;

        // The resource ID that will be assigned to the next created resource:
        UINT m_NextResourceID;

        // The page renderer, which implements most of the actual memory mapping operations 
        // to operations on traditional textures:
        PageRenderer* m_pPageRenderer;

        // This critical section protects access to all of the data structures in this class:
        CRITICAL_SECTION m_CriticalSection;

    public:
        PhysicalPageManager( D3DDevice* pd3dDevice, const PhysicalPageManagerDesc* pDesc );
        ~PhysicalPageManager();

        const PhysicalPageManagerDesc& GetDesc() const { return m_Desc; }

        HRESULT AllocatePage( PhysicalPageID* pNewPageID, D3DFORMAT DefaultFormat = (D3DFORMAT)0 );
        HRESULT FreePage( PhysicalPageID PageID );

        HRESULT UpdateSinglePageContents( PhysicalPageID PageID, const VOID* pPageBuffer, D3DFORMAT BufferDataFormat = (D3DFORMAT)0 );

        HRESULT RegisterResource( TiledResourceBase* pResource );
        HRESULT UnregisterResource( TiledResourceBase* pResource );

        HRESULT MapVirtualPageToPhysicalPage( VirtualPageID VPageID, PhysicalPageID PageID );
        HRESULT UnmapVirtualPage( VirtualPageID VPageID ) { return MapVirtualPageToPhysicalPage( VPageID, INVALID_PHYSICAL_PAGE_ID ); }
        HRESULT UnmapPhysicalPage( PhysicalPageID PageID );

        VOID ExecutePageDataOperations();

        UINT GetPhysicalPageCount() const;

        PageRenderer* GetPageRenderer() const { return m_pPageRenderer; }

        VOID GetMemoryUsage( D3DTILED_MEMORY_USAGE* pMemoryUsage ) const;

    protected:
        VOID SetDefaultParameters();

        TypedPagePool* CreateTypedPagePool( PageDataFormat DataFormat );

        PhysicalPageEntry* FindPhysicalPage( PhysicalPageID PageID );

        TiledResourceBase* GetResource( VirtualPageID VPageID ) const;

        VOID EnterLock();
        VOID LeaveLock();
    };
}


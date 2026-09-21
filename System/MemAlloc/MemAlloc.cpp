//--------------------------------------------------------------------------------------
// MemAlloc.cpp
//
// This sample demonstrates how to replace the default memory allocator used by the
// Xbox 360 Title Libraries (XTL) for certain types of memory allocations.  Memory
// allocations performed by the XTL will be routed through the supplied functions.
// The title may use the XMemAllocDefault, XPhysicalAlloc and the HeapAlloc functions
// to provide the underlying heap support.
//
// All of the allocator replacement functions must be provided or none at all.  
// The required functions are XMemAlloc, XMemFree and XMemSize.
//
// Microsoft Game Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include <xtl.h>
#include <AtgConsole.h>
#include <AtgInput.h>
#include <AtgUtil.h>

//--------------------------------------------------------------------------------------
// Globals
//--------------------------------------------------------------------------------------

// Console for output
ATG::Console    g_Console;

// Track total calls to XMemAlloc
DWORD           g_dwTotalAllocations = 0;

// Test a large number of allocations
const DWORD     dwNumAllocations = 1024 * 10;

// While testing the custom allocator hold on to allocated memory so it can be freed
VOID*           g_pMemoryArray[dwNumAllocations];

// Size to use when allocating a large memory page
const DWORD     LARGE_MEMORY_PAGE_SIZE = 64 * 1024;

// The allocator ID used for all explicit allocations performed by the sample
const DWORD     g_dwGameDefaultAllocatorID = eXALLOCAllocatorId_GameMin;

// Virtual memory allocation attributes
const DWORD     g_dwHeapMemoryAttributes = MAKE_XALLOC_ATTRIBUTES
    (
    0,                                  // ObjectType
    0,                                  // HeapTracksAttributes
    0,                                  // MustSucceed
    0,                                  // FixedSize
    g_dwGameDefaultAllocatorID,         // AllocatorID
    XALLOC_ALIGNMENT_DEFAULT,           // Alignment
    XALLOC_MEMPROTECT_READWRITE,        // MemoryProtection
    0,                                  // ZeroInit
    XALLOC_MEMTYPE_HEAP                 // MemoryType
    );

// Physical memory allocation attributes
const DWORD     g_dwPhysicalMemoryAttributes = MAKE_XALLOC_ATTRIBUTES
    (
    0,                                  // ObjectType
    0,                                  // HeapTracksAttributes
    0,                                  // MustSucceed
    0,                                  // FixedSize
    g_dwGameDefaultAllocatorID,         // AllocatorID
    XALLOC_PHYSICAL_ALIGNMENT_DEFAULT,  // Alignment
    XALLOC_MEMPROTECT_READWRITE,        // MemoryProtection
    0,                                  // ZeroInit
    XALLOC_MEMTYPE_PHYSICAL             // MemoryType
    );


//--------------------------------------------------------------------------------------
// Name: FreeListAllocator
//
// Desc: This class implements a fixed block-size free list sub-allocator for a 
// certain class of physical memory.  The size of the fixed size blocks and the 
// memory attributes to use when allocating new memory pages are provided to the 
// constructor.  When adding new memory pages to the free list, XPhysicalAlloc is 
// used to acquire 64K size pages.  For the sample, the 64K pages are split into 
// 16 x 4K blocks and added to the free list in increasing memory address order.
//
// Note that the 64K pages returned from XPhysicalAlloc are page aligned.  When the 64K
// aligned pages are split into 4K blocks they become 4K aligned.  Alignment of 4K implies
// alignment on power of two sizes up to and including 4K.
//
// Requests for READWRITE physical memory that are less than or equal to 4K in 
// size, with alignment less than or equal to 4K, will use the custom sub-allocator.
//--------------------------------------------------------------------------------------
class FreeListAllocator
{

private:

    struct MemoryBlock
    {
        MemoryBlock* pNextMemoryBlock;
    };

    MemoryBlock* m_pFreeListHead;

    // Attributes to be used for allocations
    DWORD m_dwMemoryAttributes;

    // Some basic memory tracking stats
    DWORD m_dwBlockSize;
    DWORD m_dwNumPagesAllocated;
    DWORD m_dwNumBlocksAllocated;
    DWORD m_dwNumBlocksInUse;
    DWORD m_dwNumBlocksPerPage;

    // Given the address of a block of memory, return the address of the
    // memory page that contains the block.
    MemoryBlock* GetPageBase( MemoryBlock* pMemory )
    {
        return ( MemoryBlock* )( ( INT )pMemory & ~( LARGE_MEMORY_PAGE_SIZE - 1 ) );
    }

    // Given the address of a block of memory, return the offset in bytes
    // of this block within the memory page that contains the block.
    DWORD   GetPageOffset( MemoryBlock* pMemory )
    {
        return ( DWORD )pMemory - ( DWORD )GetPageBase( pMemory );
    }

    // Do not allow the default constructor, copying or assignment
            FreeListAllocator();
            FreeListAllocator( FreeListAllocator& );
    FreeListAllocator& operator=( const FreeListAllocator& );

public:

            FreeListAllocator( DWORD dwBlockSize, DWORD dwMemoryAttributes );
            ~FreeListAllocator();

    VOID    FreeAllUnusedMemory();

    VOID* AllocMemory( SIZE_T size );
    VOID    ReleaseMemory( VOID* pMemory );

    DWORD   GetNumPagesAllocated() const
    {
        return m_dwNumPagesAllocated;
    }
    DWORD   GetNumBlocksAllocated() const
    {
        return m_dwNumBlocksAllocated;
    }
    DWORD   GetNumBlocksInUse() const
    {
        return m_dwNumBlocksInUse;
    }
    DWORD   GetNumFreeBlocks() const
    {
        return GetNumBlocksAllocated() - GetNumBlocksInUse();
    }

    VOID    DisplayAllocatorStats();
};


// Declare an instance of the custom allocator for 4KB sized blocks.
const DWORD     SUBALLOCATOR_BLOCK_SIZE = 4 * 1024;
FreeListAllocator g_FreeListSubAllocator( SUBALLOCATOR_BLOCK_SIZE, g_dwPhysicalMemoryAttributes );


//--------------------------------------------------------------------------------------
// Name: FreeListAllocator()
// Desc: constructor
//--------------------------------------------------------------------------------------
FreeListAllocator::FreeListAllocator( DWORD dwBlockSize, DWORD dwMemoryAttributes )
{
    m_pFreeListHead = NULL;
    m_dwMemoryAttributes = dwMemoryAttributes;
    m_dwBlockSize = dwBlockSize;
    m_dwNumPagesAllocated = 0;
    m_dwNumBlocksAllocated = 0;
    m_dwNumBlocksInUse = 0;

    // Make sure block size is valid
    if( 0 == dwBlockSize || dwBlockSize > LARGE_MEMORY_PAGE_SIZE )
    {
        assert( 0 != dwBlockSize && dwBlockSize <= LARGE_MEMORY_PAGE_SIZE );
        dwBlockSize = LARGE_MEMORY_PAGE_SIZE;
    }

    m_dwNumBlocksPerPage = LARGE_MEMORY_PAGE_SIZE / m_dwBlockSize;
}


//--------------------------------------------------------------------------------------
// Name: ~FreeListAllocator()
// Desc: destructor
//--------------------------------------------------------------------------------------
FreeListAllocator::~FreeListAllocator()
{
    FreeAllUnusedMemory();
}


//--------------------------------------------------------------------------------------
// Name: FreeAllUnusedMemory()
// Desc: This returns unused memory back to the kernel
//--------------------------------------------------------------------------------------
VOID FreeListAllocator::FreeAllUnusedMemory()
{
    // Make sure all memory has been freed to avoid crashing
    assert( 0 == m_dwNumBlocksInUse );

    // Free all pages in the free list
    while( m_pFreeListHead != NULL )
    {
        MemoryBlock* pNextMemoryBlock = m_pFreeListHead->pNextMemoryBlock;

        // Only free the base memory page that was allocated via XMemAllocDefault
        if( 0 == GetPageOffset( m_pFreeListHead ) )
        {
            XMemFreeDefault( m_pFreeListHead, m_dwMemoryAttributes );
            --m_dwNumPagesAllocated;
        }

        --m_dwNumBlocksAllocated;
        m_pFreeListHead = pNextMemoryBlock;
    }
}


//--------------------------------------------------------------------------------------
// Name: AllocMemory()
// Desc: Allocate memory, checking the free list first
//--------------------------------------------------------------------------------------
VOID* FreeListAllocator::AllocMemory( SIZE_T size )
{
    // Make sure size is valid
    if( size > m_dwBlockSize )
    {
        assert( size <= m_dwBlockSize );
        return NULL;
    }

    // If there are no free page blocks allocate more memory and split into blocks
    if( NULL == m_pFreeListHead )
    {
        // XMemAllocDefault can only be used to allocate 64KB memory pages for physical
        // write combined memory.
        BYTE* pMemory = ( BYTE* )XPhysicalAlloc( LARGE_MEMORY_PAGE_SIZE, MAXULONG_PTR, 0,
                                                 PAGE_READWRITE | MEM_LARGE_PAGES );
        if( NULL == pMemory )
        {
            assert( pMemory );
            return NULL;
        }

        ++m_dwNumPagesAllocated;

        // Advance pointer to the last block in the page
        pMemory += LARGE_MEMORY_PAGE_SIZE - m_dwBlockSize;

        // Add blocks to the free list so that they are in order of increasing memory address
        DWORD iBlocks = m_dwNumBlocksPerPage;

        m_dwNumBlocksAllocated += iBlocks;
        m_dwNumBlocksInUse += iBlocks;

        for(; iBlocks > 0; --iBlocks )
        {
            ReleaseMemory( pMemory );
            pMemory -= m_dwBlockSize;
        }
    }

    // Make sure that there is memory available on the list
    if( NULL == m_pFreeListHead )
        return NULL;

    // Track total blocks returned
    ++m_dwNumBlocksInUse;

    // Take the first block off the freelist and return it
    VOID* pAllocatedMemory = m_pFreeListHead;
    m_pFreeListHead = m_pFreeListHead->pNextMemoryBlock;
    return pAllocatedMemory;
}


//--------------------------------------------------------------------------------------
// Name: ReleaseMemory()
// Desc: Memory is not actually freed simply added to the free list
//--------------------------------------------------------------------------------------
VOID FreeListAllocator::ReleaseMemory( VOID* pMemory )
{
    if( NULL == pMemory )
        return;

    --m_dwNumBlocksInUse;

    MemoryBlock* pBlock = ( MemoryBlock* )pMemory;
    pBlock->pNextMemoryBlock = m_pFreeListHead;
    m_pFreeListHead = pBlock;
}


//--------------------------------------------------------------------------------------
// Name: DisplayAllocatorStats()
// Desc: Display memory allocation statistics to the console
//--------------------------------------------------------------------------------------
VOID FreeListAllocator::DisplayAllocatorStats()
{
    DWORD dwBlockSize = m_dwBlockSize / 1024;

    g_Console.Format( "\nAllocator Stats\n" );
    g_Console.Format( "---------------\n" );
    g_Console.Format( "%dK Pages Allocated: %d\n", LARGE_MEMORY_PAGE_SIZE / 1024,
                      GetNumPagesAllocated() );
    g_Console.Format( "%dK Blocks Sub-Allocated: %d\n", dwBlockSize, GetNumBlocksAllocated() );
    g_Console.Format( "%dK Blocks In Use: %d\n", dwBlockSize, GetNumBlocksInUse() );
    g_Console.Format( "%dK Blocks Free: %d\n\n", dwBlockSize, GetNumFreeBlocks() );
}


//--------------------------------------------------------------------------------------
// Name: DumpMemoryAllocationAttributes()
// Desc: Dump the attributes of the memory allocation request
//--------------------------------------------------------------------------------------
VOID DumpMemoryAllocationAttributes( const XALLOC_ATTRIBUTES& allocAttributes )
{
    ATG::DebugSpew( "Alignment: %d, ", allocAttributes.dwAlignment );
    ATG::DebugSpew( "AllocatorID: %d, ", allocAttributes.dwAllocatorId );
    ATG::DebugSpew( "FixedSize: %d, ", allocAttributes.dwFixedSize );
    ATG::DebugSpew( "HeapTracksAttr: %d, ", allocAttributes.dwHeapTracksAttributes );
    ATG::DebugSpew( "MemoryProtect: %d, ", allocAttributes.dwMemoryProtect );
    ATG::DebugSpew( "MemoryType: %d, ", allocAttributes.dwMemoryType );
    ATG::DebugSpew( "MustSucceed: %d, ", allocAttributes.dwMustSucceed );
    ATG::DebugSpew( "ObjectType: %d, ", allocAttributes.dwObjectType );
    ATG::DebugSpew( "ZeroInit: %d\n\n", allocAttributes.dwZeroInitialize );
}


//--------------------------------------------------------------------------------------
// Name: UseFreeListSubAllocator()
// Desc: Helper function which returns TRUE if the given memory should be handled by the
// custom allocator
//--------------------------------------------------------------------------------------
inline BOOL UseFreeListSubAllocator( SIZE_T size, const XALLOC_ATTRIBUTES& AllocAttributes )
{
    return (
        AllocAttributes.dwMemoryType == XALLOC_MEMTYPE_PHYSICAL &&
        AllocAttributes.dwAlignment <= XALLOC_PHYSICAL_ALIGNMENT_4K &&
        AllocAttributes.dwMemoryProtect == XALLOC_MEMPROTECT_READWRITE &&
        size <= SUBALLOCATOR_BLOCK_SIZE
        );
}


//--------------------------------------------------------------------------------------
// Name: XMemAlloc
// Desc: This replaces the default XMemAlloc with our own implementation.
//--------------------------------------------------------------------------------------
VOID* WINAPI XMemAlloc( SIZE_T size, DWORD dwAllocAttributes )
{
    const XALLOC_ATTRIBUTES& AllocAttributes = *( XALLOC_ATTRIBUTES* )&dwAllocAttributes;
    VOID* pMemory = NULL;

    // Provide custom memory allocation for certain physical allocations
    if( UseFreeListSubAllocator( size, AllocAttributes ) )
    {
        pMemory = g_FreeListSubAllocator.AllocMemory( size );
        if( NULL == pMemory )
        {
            assert( NULL != pMemory );
            return NULL;
        }

        // Respect the zero initialize request.  The faster XMemSet128 can be used 
        // because memory is at least 128 byte aligned and is cache-able.
        if( AllocAttributes.dwZeroInitialize )
            XMemSet128( pMemory, 0, size );
    }
    else
    {
        // Use the default allocator for all other allocations
        pMemory = XMemAllocDefault( size, dwAllocAttributes );
    }

    ++g_dwTotalAllocations;
    return pMemory;
}


//--------------------------------------------------------------------------------------
// Name: XMemFree
// Desc: This replaces the default XMemFree with our own implementation.
//--------------------------------------------------------------------------------------
VOID WINAPI XMemFree( VOID* pMemory, DWORD dwAllocAttributes )
{
    if( NULL == pMemory )
    {
        return;
    }

    const XALLOC_ATTRIBUTES& AllocAttributes = *( XALLOC_ATTRIBUTES* )&dwAllocAttributes;

    // We need the size to determine if this allocation belongs to our custom allocator
    SIZE_T size = XMemSize( pMemory, dwAllocAttributes );

    if( UseFreeListSubAllocator( size, AllocAttributes ) )
    {
        g_FreeListSubAllocator.ReleaseMemory( pMemory );
    }
    else
    {
        XMemFreeDefault( pMemory, dwAllocAttributes );
    }
}


//--------------------------------------------------------------------------------------
// Name: XMemSize
// Desc: This replaces the default implementation of XMemSize with our own.
//--------------------------------------------------------------------------------------
SIZE_T WINAPI XMemSize( VOID* pAddress, DWORD dwAllocAttributes )
{
    const XALLOC_ATTRIBUTES& AllocAttributes = *( XALLOC_ATTRIBUTES* )&dwAllocAttributes;

    // Handle requests for memory that belongs to the custom allocator
    if( UseFreeListSubAllocator( 0, AllocAttributes ) )
        return SUBALLOCATOR_BLOCK_SIZE;

    // Use the default implementation for all other types of memory
    return XMemSizeDefault( pAddress, dwAllocAttributes );
}


//--------------------------------------------------------------------------------------
// Name: TestMemoryAllocations()
// Desc: Allocate, access and free some memory.  These allocations call the supplied
// XMemAlloc replacement.
//--------------------------------------------------------------------------------------
VOID TestMemoryAllocations( DWORD dwAllocationAttributes )
{
    const SIZE_T AllocationSize = SUBALLOCATOR_BLOCK_SIZE;

    for( INT i = 0; i < dwNumAllocations; ++i )
    {
        g_pMemoryArray[i] = XMemAlloc( AllocationSize, dwAllocationAttributes );
    }
    g_Console.Format( "%dK bytes allocated...", dwNumAllocations * AllocationSize / 1024 );

    for( INT i = 0; i < dwNumAllocations; ++i )
    {
        memset( g_pMemoryArray[i], 0, AllocationSize );
    }
    g_Console.Format( "accessed..." );

    for( INT i = 0; i < dwNumAllocations; ++i )
    {
        XMemFree( g_pMemoryArray[i], dwAllocationAttributes );
    }
    g_Console.Format( "freed.\n" );
}


//--------------------------------------------------------------------------------------
// Name: main()
// Desc: Entry point to the program
//--------------------------------------------------------------------------------------
INT __cdecl main()
{
    // Initialize the console window
    g_Console.Create( "game:\\Media\\Fonts\\Courier_New_11.xpr", 0xFF1F005F, 0xFFFFFFFF );
    g_Console.SendOutputToDebugChannel( TRUE );

    // Call XMemAlloc using attributes that will not use the custom sub-allocator
    g_Console.Format( "Virtual Memory..." );
    TestMemoryAllocations( g_dwHeapMemoryAttributes );

    // Call XMemAlloc using attributes that will test the custom sub-allocator
    g_Console.Format( "Physical Memory..." );
    TestMemoryAllocations( g_dwPhysicalMemoryAttributes );

    // Should not hit XPhysicalAlloc because the free list already contains enough blocks
    g_Console.Format( "Physical Memory..." );
    TestMemoryAllocations( g_dwPhysicalMemoryAttributes );

    g_FreeListSubAllocator.DisplayAllocatorStats();

    g_Console.Format( "Total memory allocations performed: %d", g_dwTotalAllocations );

    for(; ; )
    {
        // Detect exit sample request LT-RT-RB
        ATG::Input::GetMergedInput();
    }
}


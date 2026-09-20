//--------------------------------------------------------------------------------------
// DualFrameAsyncSwaps.cpp
//
// Class that handles presenting a composite frame, consisting of two frames. Async swaps
// is used to automatically schedule each frame's swap to occur at the next VBlank
// asynchronously, leaving the CPU and GPU free to continue processing. This class
// provides functionality of two things:
//      1) No tearing - It makes sure that tearing does not occur from either swapping mid
//         frame or resolving into a front buffer that has not yet been displayed or is
//         being displayed.
//
//      2) Throttling - It provides a throttle function with which the title can stall the
//         CPU and GPU in order to synchronize to a VBlank if the title wanted to run at a
//         regular frame rate.
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "DualFrameAsyncSwaps.h"
#include <AtgUtil.h>


//--------------------------------------------------------------------------------------
// Statics for class
//--------------------------------------------------------------------------------------

D3DDevice* DualFrameAsyncSwaps::s_pd3dDevice            = NULL;
UINT DualFrameAsyncSwaps::s_uCurrentFrontBufferIdx      = 0;
DWORD volatile DualFrameAsyncSwaps::s_dwThrottleSwap    = 0;
HANDLE DualFrameAsyncSwaps::s_hThrottleVBlankEvent      = NULL;
DualFrameAsyncSwaps::FrontBuffer DualFrameAsyncSwaps::s_FrontBuffers[ s_uNumFrames ] = { { NULL, NULL, 0 }, { NULL, NULL, 0 } };

#ifdef DUAL_FRAME_ASYNC_SWAPS_PIX_EVENTS
DWORD DualFrameAsyncSwaps::s_dwSwap                 = 0;
DWORD DualFrameAsyncSwaps::s_dwSwapVBlank           = 0;
DWORD DualFrameAsyncSwaps::s_dwVBlank               = 0;
DWORD DualFrameAsyncSwaps::s_dwVBlankSwap           = 0;
HANDLE DualFrameAsyncSwaps::s_hVBlankDebugEvent     = NULL;
HANDLE DualFrameAsyncSwaps::s_hVBlankDebugThread    = NULL;
HANDLE DualFrameAsyncSwaps::s_hSwapDebugEvent       = NULL;
HANDLE DualFrameAsyncSwaps::s_hSwapDebugThread      = NULL;
#endif


//--------------------------------------------------------------------------------------
// Name: Present
// Desc: Present a frame. When in async mode, we'll ensure that the front buffers are
//       not overwritten to avoid tearing.
//--------------------------------------------------------------------------------------

VOID DualFrameAsyncSwaps::Present()
{
    // First check if we are in synchronous mode. If so, do the regular present using the
    // SynchronizeToPresentationInterval() API, otherwise we do a custom present with async
    // swaps as described in the header of this file
    D3DDEVICE_CREATION_PARAMETERS parameters;
    s_pd3dDevice->GetCreationParameters( &parameters );

    if ( parameters.BehaviorFlags & D3DCREATE_ASYNCHRONOUS_SWAPS )
    {
        PresentAsynchronous();
    }
    else
    {
        PresentSynchronous();
    }
}


//--------------------------------------------------------------------------------------
// Name: PresentSynchronous
// Desc: Present function when in synchronous mode
//--------------------------------------------------------------------------------------

VOID DualFrameAsyncSwaps::PresentSynchronous()
{
    PIXBeginNamedEvent( 0, __FUNCTION__ );

    // If in synchronous mode, use automatic throttling from API. This will
    // do the correct blocking on the CPU and GPU to ensure we don't overwrite
    // the current front buffer to avoid tearing
    s_pd3dDevice->SynchronizeToPresentationInterval();

    // Resolve EDRAM to the available front buffer
    s_pd3dDevice->Resolve( 0, NULL, s_FrontBuffers[ s_uSyncModeFrontBufferIdx ].m_pTexture, NULL, 0, 0, NULL, 0, 0, NULL );

    // Swap
    Swap( s_uSyncModeFrontBufferIdx );

    PIXEndNamedEvent();
}


//--------------------------------------------------------------------------------------
// Name: PresentAsynchronous
// Desc: Present a frame in asynchronous mode.
//--------------------------------------------------------------------------------------

VOID DualFrameAsyncSwaps::PresentAsynchronous()
{
    PIXBeginNamedEvent( 0, __FUNCTION__ );

    // Block the CPU and GPU until it is safe to resolve and swap
    BlockToAvoidTearing();

    // Now it's safe to resolve into the current front buffer, since we know that this front buffer has
    // already been presented
    s_pd3dDevice->Resolve( 0, NULL, s_FrontBuffers[ s_uCurrentFrontBufferIdx ].m_pTexture, NULL, 0, 0, NULL, 0, 0, NULL );

    // Do the swap for this front buffer
    Swap( s_uCurrentFrontBufferIdx );

    // After the swap has completed, record the current swap count so that we can use it in the VBlank callback to
    // to signal an event for the following frame
    D3DSWAP_STATUS status;
    s_pd3dDevice->QuerySwapStatus( &status );

    // This is thread safe since m_dwSwap is volatile
    s_FrontBuffers[ s_uCurrentFrontBufferIdx ].m_dwSwap = status.Swap;

    // Flip to the next front buffer index
    s_uCurrentFrontBufferIdx++;
    s_uCurrentFrontBufferIdx %= s_uNumFrames;

    PIXEndNamedEvent();
}


//--------------------------------------------------------------------------------------
// Name: BlockToAvoidTearing
// Desc: Blocks the CPU and GPU to avoid tearing. This function must be called before
//       resolving and swapping a new front buffer in order to avoid overwriting a
//       front buffer that has not yet been displayed or is being displayed.
//--------------------------------------------------------------------------------------

VOID DualFrameAsyncSwaps::BlockToAvoidTearing()
{
    // We need to make sure that we don't tear the previous frame
    UINT uPreviousFrontBufferIdx = ( s_uCurrentFrontBufferIdx - 1 ) % s_uNumFrames;

    // The comments and documentation refers to frame 1 and 2, so use uPreviousFrontBufferIdx + 1 in the PIX event's string
    PIXBeginNamedEvent( 0, "Don't tear FrontBuffer %d at Swap %d", uPreviousFrontBufferIdx + 1, s_FrontBuffers[ uPreviousFrontBufferIdx ].m_dwSwap );

    // Use BlockOnAsyncResources to block the GPU until it is safe to resolve into the current front buffer.
    // Without this block we could resolve into a front buffer texture that has not yet been presented or
    // that is currently being presented, causing tearing.
    D3DASYNCBLOCK asyncBlock = s_pd3dDevice->InsertBlockOnAsyncResources( 0, NULL, 0, NULL, 0 );

    // Normally, a call to SynchronizeToPresentationInterval ensures that all preceding rendering
    // has been kicked off to the GPU.  Here, we must guarantee this manually with an InsertFence()
    s_pd3dDevice->InsertFence();

    // Make sure the current front buffer's VBlank event is reset so that the following frame can 
    // wait for it correctly
    ResetEvent( s_FrontBuffers[ s_uCurrentFrontBufferIdx ].m_hVBlankEvent );

    // Block the CPU until the previous frame's swap has completed. This ensures that we won't resolve or
    // swap over a frame that hasn't yet been presented or is currently presented.
    WaitForSingleObject( s_FrontBuffers[ uPreviousFrontBufferIdx ].m_hVBlankEvent, INFINITE );

    // After the CPU has finished blocking, we can now unblock the GPU by signalling the inserted BlockOnAsyncResources
    s_pd3dDevice->SignalAsyncResources( asyncBlock );

    PIXEndNamedEvent();
}


//--------------------------------------------------------------------------------------
// Name: Throttle
// Desc: Stalls the CPU and GPU until the VBlank of the current swap. The VBlank
//       callback has data about which swap is scheduled to be presented at the VBlank.
//       We simply use this data to signal an event once we get a matching swap that
//       we're waiting for. Technically speaking we don't have to block the GPU here,
//       since the GPU won't have any work to do anyway.
//--------------------------------------------------------------------------------------

VOID DualFrameAsyncSwaps::Throttle()
{
    PIXBeginNamedEvent( 0, __FUNCTION__ );

    // Use BlockOnAsyncResources to block the GPU
    D3DASYNCBLOCK asyncBlock = s_pd3dDevice->InsertBlockOnAsyncResources( 0, NULL, 0, NULL, 0 );
    s_pd3dDevice->InsertFence();

    // Record the current swap count. When the swap count in the VBlank callback matches or
    // exceeds this swap count, the s_hThrottleVBlankEvent event will be signalled. 
    D3DSWAP_STATUS status;
    s_pd3dDevice->QuerySwapStatus( &status );

    // This is thread safe since s_dwThrottleSwap is volatile
    s_dwThrottleSwap = status.Swap;
    
    // Ensure that the event is reset before waiting on it
    ResetEvent( s_hThrottleVBlankEvent );

    // Wait until the VBlank callback's swap count matches or exceeds s_dwThrottleSwap
    WaitForSingleObject( s_hThrottleVBlankEvent, INFINITE );

    // After the CPU has finished blocking, we can now unblock the GPU by signalling
    // the inserted BlockOnAsyncResources
    s_pd3dDevice->SignalAsyncResources( asyncBlock );

    PIXEndNamedEvent();
}


//--------------------------------------------------------------------------------------
// Name: Swap
// Desc: Calls D3D's Swap using the requested front buffer
//--------------------------------------------------------------------------------------

VOID DualFrameAsyncSwaps::Swap( const UINT uFrontBufferIdx )
{
#ifdef DUAL_FRAME_ASYNC_SWAPS_PIX_EVENTS
        D3DSWAP_STATUS status;
        s_pd3dDevice->QuerySwapStatus( &status );
        // Need to use status.Swap + 1 since the swap we want to show hasn't happened yet.
        // Also, in comments and documentation we refer to frame 1 and 2, so we use uFrontBufferIdx + 1 in string
        PIXBeginNamedEvent( 0, "Swap %d FrontBuffer %d", status.Swap + 1, uFrontBufferIdx + 1 );
#endif
        // Without async swaps active, a call to Swap means that the GPU will trigger a flip
        // when it reaches this point in the command buffer.  Normally, we stall the GPU until
        // the VBlank interval. With async swaps active, a call to Swap means the front buffer
        // will be flipped by the CPU at the next VBlank interrupt.
        s_pd3dDevice->Swap( s_FrontBuffers[ uFrontBufferIdx ].m_pTexture, NULL );

#ifdef DUAL_FRAME_ASYNC_SWAPS_PIX_EVENTS
        PIXEndNamedEvent();
#endif

        // Ensure that all GPU has been kicked off.
        s_pd3dDevice->InsertFence();
}


//--------------------------------------------------------------------------------------
// Name: VBlankCallback
// Desc: Callback when a VBlank occurs. This callback will be called at regular
//       intervals, every 16.6ms
//--------------------------------------------------------------------------------------

VOID DualFrameAsyncSwaps::VBlankCallback( D3DVBLANKDATA* pData )
{
    // This is thread safe since s_dwThrottleSwap is volatile
    DWORD dwThrottleSwap = s_dwThrottleSwap;

    // If the swap scheduled for this VBlank matches or exceeds the swap that we're waiting for
    // during throttling, then set the event. This will unblock the CPU and GPU. Checking for
    // exceeding rather that just matching, ensures that we won't get into a deadlock.
    // NOTE: To be safe, we really need to check that these values don't wrap, but for a DWORD to
    // wrap at 60Hz increments, it will take more than 800 days of continous running, so to keep
    // the sample easy to read, we don't check for wrapping.
    if ( pData->Swap >= dwThrottleSwap )
    {
        SetEvent( s_hThrottleVBlankEvent );
    }

    // If the swap scheduled for this VBlank mathces or exceeds the swap of one of the front buffers
    // then we signal an event so that the BlockToAvoidTearing() function knows that the front
    // buffer has been presented and it is safe for another front buffer to be resolved
    // or swapped without causing tearing.
    for ( UINT i = 0; i < s_uNumFrames; i++ )
    {
        // This is thread safe since m_dwSwap is volatile
        DWORD dwSwap = s_FrontBuffers[ i ].m_dwSwap;

        // NOTE: To be safe, we really need to check that these values don't wrap, but for a DWORD to
        // wrap at 60Hz increments, it will take more than 800 days of continous running, so to keep
        // the sample easy to read, we don't check for wrapping.
        if ( pData->Swap >= dwSwap )
        {
            SetEvent( s_FrontBuffers[ i ].m_hVBlankEvent );
        }
    }

#ifdef DUAL_FRAME_ASYNC_SWAPS_PIX_EVENTS
    // Get debug information that will be shown in PIX. As long as this data is set
    // before signalling the event, the data will be thread safe
    s_dwVBlank = pData->VBlank;
    s_dwVBlankSwap = pData->Swap;
    SetEvent( s_hVBlankDebugEvent );
#endif
}


#ifdef DUAL_FRAME_ASYNC_SWAPS_PIX_EVENTS

//--------------------------------------------------------------------------------------
// Name: VBlankDebugThread
// Desc: Thread that shows debug information from VBlank
//--------------------------------------------------------------------------------------

DWORD DualFrameAsyncSwaps::VBlankDebugThread( LPVOID lpThreadParameter )
{
    ATG::SetThreadName( (DWORD)-1, "VBlankCallback" );

    for ( ;; )
    {
        WaitForSingleObject( s_hVBlankDebugEvent, INFINITE );

        PIXBeginNamedEvent(0, "VBlank %d VBlankSwap %d", s_dwVBlank, s_dwVBlankSwap );
#ifdef DUAL_FRAME_ASYNC_SWAPS_LONG_PIX_EVENTS
        // Simulate some extra processing to more easily visualize these events in PIX
        for ( UINT i = 0; i < 1000; i++ )
        {
            sqrt( 10.0f );
        }
#endif
        PIXEndNamedEvent();      
    }
}


//--------------------------------------------------------------------------------------
// Name: SwapCallback
// Desc: Callback after each Swap that completes. This callback does not occur at
//       regular intervals like the VBlank callback, it occurs once the GPU is
//       done with the Swap() command.
//--------------------------------------------------------------------------------------

VOID DualFrameAsyncSwaps::SwapCallback( D3DSWAPDATA *pData )
{
    // Get debug info for current swap. As long as the data is set before the event
    // is signalled, the data should be thread safe
    s_dwSwap        = pData->Swap;
    s_dwSwapVBlank  = pData->SwapVBlank;

    // Wake up the thread that will add a PIX event with the debug data
    SetEvent( s_hSwapDebugEvent );
}


//--------------------------------------------------------------------------------------
// Name: SwapDebugThread
// Desc: Thread that adds PIX events with debug info about the current swap
//--------------------------------------------------------------------------------------

DWORD DualFrameAsyncSwaps::SwapDebugThread( LPVOID lpThreadParameter )
{
    ATG::SetThreadName( (DWORD)-1, "SwapCallback" );

    for ( ;; )
    {
        WaitForSingleObject( s_hSwapDebugEvent, INFINITE );

        PIXBeginNamedEvent(0, "Swap %d SwapVBlank %d", s_dwSwap, s_dwSwapVBlank );

#ifdef DUAL_FRAME_ASYNC_SWAPS_LONG_PIX_EVENTS
        // Simulate some extra processing to more easily visualize these events in PIX
        for ( UINT i = 0; i < 1000; i++ )
        {
            sqrt( 10.0f );
        }
#endif

        PIXEndNamedEvent();
    }
}

#endif


//--------------------------------------------------------------------------------------
// Name: DualFrameAsyncSwaps
// Desc: Constructor
//--------------------------------------------------------------------------------------

DualFrameAsyncSwaps::DualFrameAsyncSwaps()
{
    for ( UINT i = 0; i < s_uNumFrames; i++ )
    {
        s_FrontBuffers[ i ].m_pTexture     = NULL;
        s_FrontBuffers[ i ].m_hVBlankEvent = NULL;
        s_FrontBuffers[ i ].m_dwSwap       = 0;        
    }

    s_pd3dDevice            = NULL;
    s_uCurrentFrontBufferIdx= 0;
    s_dwThrottleSwap        = 0;
    s_hThrottleVBlankEvent  = NULL;

#ifdef DUAL_FRAME_ASYNC_SWAPS_PIX_EVENTS
    s_dwSwap                = 0;
    s_dwSwapVBlank          = 0;
    s_dwVBlank              = 0;
    s_dwVBlankSwap          = 0;
    s_hVBlankDebugEvent     = NULL;
    s_hVBlankDebugThread    = NULL;
    s_hSwapDebugEvent       = NULL;
    s_hSwapDebugThread      = NULL;
#endif

}


//--------------------------------------------------------------------------------------
// Name: ~DualFrameAsyncSwaps
// Desc: Destructor
//--------------------------------------------------------------------------------------

DualFrameAsyncSwaps::~DualFrameAsyncSwaps()
{
    Shutdown();
}


//--------------------------------------------------------------------------------------
// Name: Initialize
// Desc: Initialize the data
//--------------------------------------------------------------------------------------

HRESULT DualFrameAsyncSwaps::Initialize( D3DDevice* pDevice, D3DPRESENT_PARAMETERS const& d3dpp )
{
    RETURN_ON_NULL( s_pd3dDevice = pDevice );

    // Create front buffer textures and events
    UINT uWidth = d3dpp.BackBufferWidth;
    UINT uHeight = d3dpp.BackBufferHeight;
    D3DFORMAT format = d3dpp.FrontBufferFormat;
    for ( UINT i = 0; i < s_uNumFrames; i++ )
    {
        RETURN_ON_FAIL( s_pd3dDevice->CreateTexture( uWidth, uHeight, 1, 0, format, 0, &s_FrontBuffers[ i ].m_pTexture, NULL ) );
        RETURN_ON_NULL( s_FrontBuffers[ i ].m_hVBlankEvent = CreateEvent( NULL, FALSE, FALSE, NULL ) );
    }  

    // Create event for throttling
    RETURN_ON_NULL( s_hThrottleVBlankEvent = CreateEvent( NULL, FALSE, FALSE, NULL ) );

#ifdef DUAL_FRAME_ASYNC_SWAPS_PIX_EVENTS
    // Create events and threads for debugging
    RETURN_ON_NULL( s_hVBlankDebugEvent = CreateEvent( NULL, FALSE, FALSE, NULL ) );
    RETURN_ON_NULL( s_hVBlankDebugThread = CreateThread( NULL, 0, VBlankDebugThread, NULL, CREATE_SUSPENDED, NULL ) );
    XSetThreadProcessor( s_hVBlankDebugThread, s_dwDebugHWThread );
    SetThreadPriority( s_hVBlankDebugThread, THREAD_PRIORITY_TIME_CRITICAL );    

    RETURN_ON_NULL( s_hSwapDebugEvent = CreateEvent( NULL, FALSE, FALSE, NULL ) );
    RETURN_ON_NULL( s_hSwapDebugThread = CreateThread( NULL, 0, SwapDebugThread, NULL, CREATE_SUSPENDED, NULL ) );
    XSetThreadProcessor( s_hSwapDebugThread, s_dwDebugHWThread );
    SetThreadPriority( s_hSwapDebugThread, THREAD_PRIORITY_TIME_CRITICAL );

    ResumeThread( s_hVBlankDebugThread );
    ResumeThread( s_hSwapDebugThread );

    // Set callbacks. Make sure we set this after all the events and threads have been created.
    s_pd3dDevice->SetSwapCallback( SwapCallback );    
#endif   

    s_pd3dDevice->SetVerticalBlankCallback( VBlankCallback );

    // Turn async swaps on
    s_pd3dDevice->SetSwapMode( TRUE );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Shutdown
// Desc: Shutdown and release resources
//--------------------------------------------------------------------------------------

VOID DualFrameAsyncSwaps::Shutdown()
{
#ifdef DUAL_FRAME_ASYNC_SWAPS_PIX_EVENTS
    if ( s_hVBlankDebugThread )
    {
        CloseHandle( s_hVBlankDebugThread );
        s_hVBlankDebugThread = NULL;
    }

    if ( s_hSwapDebugThread )
    {
        CloseHandle( s_hSwapDebugThread );
        s_hSwapDebugThread = NULL;
    }

    if ( s_hVBlankDebugEvent )
    {
        CloseHandle( s_hVBlankDebugEvent );
        s_hVBlankDebugEvent = NULL;
    }

    if ( s_hSwapDebugEvent )
    {
        CloseHandle( s_hSwapDebugEvent );
        s_hSwapDebugEvent = NULL;
    }
#endif

    for ( UINT i = 0; i < s_uNumFrames; i++ )
    {
        SAFE_RELEASE( s_FrontBuffers[ i ].m_pTexture );

        s_FrontBuffers[ i ].m_dwSwap = 0;

        if ( s_FrontBuffers[ i ].m_hVBlankEvent )
        {
            CloseHandle( s_FrontBuffers[ i ].m_hVBlankEvent );
            s_FrontBuffers[ i ].m_hVBlankEvent = NULL;
        }
    }

    s_dwThrottleSwap = 0;
    s_uCurrentFrontBufferIdx = 0;

    if ( s_hThrottleVBlankEvent )
    {
        CloseHandle( s_hThrottleVBlankEvent );
        s_hThrottleVBlankEvent = NULL;
    }

    s_pd3dDevice        = NULL;

#ifdef DUAL_FRAME_ASYNC_SWAPS_PIX_EVENTS
    s_dwSwap            = 0;
    s_dwSwapVBlank      = 0;
    s_dwVBlank          = 0;
    s_dwVBlankSwap      = 0;
#endif

}
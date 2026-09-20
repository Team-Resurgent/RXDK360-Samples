//--------------------------------------------------------------------------------------
// DualFrameAsyncSwaps.h
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

#pragma once

#ifndef DUAL_FRAME_ASYNC_SWAPS_H
#define DUAL_FRAME_ASYNC_SWAPS_H

#include <xtl.h>

// Define this if you want to see extra PIX events for debugging purposes, e.g. shows 
// PIX events for each swap and VBlank
#define DUAL_FRAME_ASYNC_SWAPS_PIX_EVENTS

// Define this if you want to more clearly see where VBlanks and Swaps happen on the
// PIX time line. It will simply add extra processing time to make the events longer
// and easier to visualize
#define DUAL_FRAME_ASYNC_SWAPS_LONG_PIX_EVENTS


//--------------------------------------------------------------------------------------
// Name: class DualFrameAsyncSwaps
// Desc: Class to handle presenting a composite frame using async swaps
//--------------------------------------------------------------------------------------

class DualFrameAsyncSwaps
{
public:
    DualFrameAsyncSwaps();
    ~DualFrameAsyncSwaps();

    HRESULT Initialize( D3DDevice* pDevice, D3DPRESENT_PARAMETERS const& d3dpp );
    VOID Shutdown();

    VOID Present();
    VOID Throttle();

private:

    // Private structure for a front buffer
    struct FrontBuffer
    {
        D3DTexture*         m_pTexture;                         // The texture for the front buffer
        HANDLE              m_hVBlankEvent;                     // Event that will signal in the VBlank callback when this buffer is presented
        volatile DWORD      m_dwSwap;                           // The swap count for this front buffer
    };

    static const UINT       s_uNumFrames                = 2;    // This class uses 2 front buffers
    static const UINT       s_uSyncModeFrontBufferIdx   = 0;    // In synchronous mode, just use this index    

    static D3DDevice*       s_pd3dDevice;                       // D3D device
    static FrontBuffer      s_FrontBuffers[ s_uNumFrames ];     // Array of front buffers

    static HANDLE           s_hThrottleVBlankEvent;             // Event that will signal in the VBlank callback when the throttle swap is reached
    static volatile DWORD   s_dwThrottleSwap;                   // When the swap count in the VBlank callback matches this swap, it will signal s_hThrottleVBlankEvent
    static UINT             s_uCurrentFrontBufferIdx;           // The current front buffer index
    
    VOID Swap( const UINT uFrontBufferIdx );
    VOID PresentSynchronous();
    VOID PresentAsynchronous();
    VOID BlockToAvoidTearing();

    static VOID VBlankCallback( D3DVBLANKDATA* pData );

#ifdef DUAL_FRAME_ASYNC_SWAPS_PIX_EVENTS
    static const DWORD  s_dwDebugHWThread = 1;              // Create debug threads for VBlank and Swap on HW1
    static DWORD        s_dwSwap;                           // Latest swap count
    static DWORD        s_dwSwapVBlank;                     // VBlank for which the latest swap is sheduled for
    static DWORD        s_dwVBlank;                         // Latest VBlank count
    static DWORD        s_dwVBlankSwap;                     // The swap used by the latest VBlank
    static HANDLE       s_hVBlankDebugEvent;                // Event to signal after a VBlank has completed
    static HANDLE       s_hVBlankDebugThread;               // Thread that shows PIX events of the VBlank just completed
    static HANDLE       s_hSwapDebugEvent;                  // Event to signal after a swap has completed on the GPU
    static HANDLE       s_hSwapDebugThread;                 // Thread that shows PIX events of the swap just completed

    static VOID SwapCallback( D3DSWAPDATA *pData );
    static DWORD SwapDebugThread( LPVOID lpThreadParameter );    
    static DWORD VBlankDebugThread( LPVOID lpParameter );
#endif

};

#endif
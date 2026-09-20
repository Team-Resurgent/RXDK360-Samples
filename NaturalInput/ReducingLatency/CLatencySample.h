//--------------------------------------------------------------------------------------
// CLatencySample.h
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

#include <xtl.h>
#include <AtgUtil.h>
#include <AtgFont.h>
#include <AtgHelp.h>
#include <AtgInput.h>
#include "CRenderer.h"
#include "Common.h"

class CLatencySample
{
public:
    CLatencySample();

    VOID Run();

    virtual HRESULT Initialize();
    VOID Update( const UINT uFrameBufferIdx );
    virtual VOID Render( const UINT uFrameBufferIdx ) = 0;

    VOID UpdateAvatar( const UINT uFrameBufferIdx );
    BOOL UpdateSkeletonData( const UINT uFrameBufferIdx );

    inline ESampleType GetSampleType() const { return m_SampleType; }

protected:
    HRESULT CreateDevice();

    virtual VOID BeginRenderPass( const ERenderPass RenderPass, const UINT uFrameBufferIdx ) = 0;
    virtual VOID EndRenderPass( const ERenderPass RenderPass ) = 0;

    static DWORD RenderThread( LPVOID lpParameter );
    static DWORD GameUpdateThread( LPVOID lpParameter );
    static DWORD ColorUpdateThread( LPVOID lpParameter );
    static DWORD DepthUpdateThread( LPVOID lpParameter );
    static DWORD SkeletonTrackingUpdateThread( LPVOID lpParameter );

    static VOID UpdateActiveSkeleton( const UINT uFrameBufferIdx );

    virtual VOID FilterJointPositions( const UINT uFrameBufferIdx ) = 0;

    VOID RenderUI();

    VOID Reset();
   
    ESampleType             m_SampleType;

    static D3DDevice*       m_pd3dDevice;
    static D3DDevice*       m_pCmdBufferDevice;
    D3DPRESENT_PARAMETERS   m_d3dParams;

    static ATG::Font        m_Font;
    static ATG::Help        m_Help;
    static ATG::Timer       m_Timer;
    static CRITICAL_SECTION m_csTimer;
    static BOOL             m_bInitialized;

    static CRenderer        m_Renderer;

    static UINT             m_uCurrentFrameBufferIdx;
    static CRITICAL_SECTION m_csCurrentFrameBufferIdx;
    static CFrameBufferData m_FrameBufferData[ NUM_FRAME_BUFFERS ];

    static BOOL             m_bEnableJointFiltering;
    BOOL                    m_bDrawHelp;

    // Events
    static HANDLE           m_hNewFrameEvent;               // Indicates the start of a new frame on the CPU
    static HANDLE           m_hSwitchLatencyModeEvent;      // The user want to switch between high/low latency modes
    static HANDLE           m_hNewSkeletonDataEvent;        // New skeleton data has arrived from NUI
    static HANDLE           m_hNewColorDataEvent;           // New color data has arrived from NUI
    static HANDLE           m_hNewDepthDataEvent;           // New depth data has arrived from NUI
    static HANDLE           m_hAvatarUpdatedEvent;          // Avatar has been updated with new NUI data

    // Image Streams
    static HANDLE           m_hDepthStream;
    static HANDLE           m_hColorStream;

    // Threads
    HANDLE                  m_hRenderThread;
    HANDLE                  m_hGameUpdateThread;
    HANDLE                  m_hSkeletonTrackingUpdateThread;
    HANDLE                  m_hColorUpdateThread;
    HANDLE                  m_hDepthUpdateThread;

};

//--------------------------------------------------------------------------------------
// CLatencySample.cpp
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "CLatencySample.h"
#include "AtgNuiCommon.h"

//--------------------------------------------------------------------------------------
// Definitions of statics
//--------------------------------------------------------------------------------------

// For being able to easily switch between the two different samples, we need to
// declare some statics that will be shared between the two sample classes
D3DDevice* CLatencySample::m_pd3dDevice = NULL;
D3DDevice* CLatencySample::m_pCmdBufferDevice = NULL;
ATG::Font CLatencySample::m_Font = ATG::Font();
ATG::Help CLatencySample::m_Help = ATG::Help();
ATG::Timer CLatencySample::m_Timer = ATG::Timer();
CRITICAL_SECTION CLatencySample::m_csTimer = CRITICAL_SECTION();
CRenderer CLatencySample::m_Renderer = CRenderer();
BOOL CLatencySample::m_bInitialized = FALSE;
BOOL CLatencySample::m_bEnableJointFiltering = TRUE;
UINT CLatencySample::m_uCurrentFrameBufferIdx = 0;
CRITICAL_SECTION CLatencySample::m_csCurrentFrameBufferIdx = CRITICAL_SECTION();
CFrameBufferData CLatencySample::m_FrameBufferData[ NUM_FRAME_BUFFERS ];
HANDLE CLatencySample::m_hNewFrameEvent = NULL;
HANDLE CLatencySample::m_hSwitchLatencyModeEvent = NULL;
HANDLE CLatencySample::m_hNewSkeletonDataEvent = NULL;
HANDLE CLatencySample::m_hNewColorDataEvent = NULL;
HANDLE CLatencySample::m_hNewDepthDataEvent = NULL;
HANDLE CLatencySample::m_hAvatarUpdatedEvent = NULL;
HANDLE CLatencySample::m_hColorStream = NULL;
HANDLE CLatencySample::m_hDepthStream = NULL;


//--------------------------------------------------------------------------------------
// Callouts for labelling the gamepad on the help screen
//--------------------------------------------------------------------------------------

ATG::HELP_CALLOUT g_HelpCallouts[] =
{
    { ATG::HELP_BACK_BUTTON,  ATG::HELP_PLACEMENT_1, L"Display help"  },
    { ATG::HELP_A_BUTTON,     ATG::HELP_PLACEMENT_2, L"Toggle low/high\nlatency" },
    { ATG::HELP_X_BUTTON,     ATG::HELP_PLACEMENT_2, L"Toggle joint\nfiltering" }
};
#define NUM_HELP_CALLOUTS (sizeof(g_HelpCallouts) / sizeof(g_HelpCallouts[0]))


//--------------------------------------------------------------------------------------
// Name: CLatencySample
// Desc: Constructor for class
//--------------------------------------------------------------------------------------

CLatencySample::CLatencySample()
{
    m_SampleType                    = SAMPLE_TYPE_NONE;

    m_bDrawHelp                     = FALSE;

    m_hRenderThread                 = NULL;
    m_hGameUpdateThread             = NULL;
    m_hSkeletonTrackingUpdateThread = NULL;
    m_hColorUpdateThread            = NULL;
    m_hDepthUpdateThread            = NULL;
}


//--------------------------------------------------------------------------------------
// Name: Initialize
// Desc: Initialize the sample
//--------------------------------------------------------------------------------------

HRESULT CLatencySample::Initialize()
{
    // Initialize all statics
    if ( !m_bInitialized )
    {
        RETURN_ON_FAIL( CreateDevice() );
        RETURN_ON_FAIL( m_Font.Create( "game:\\Media\\Fonts\\Arial_16.xpr" ) );
        RETURN_ON_FAIL( m_Renderer.Initialize( &m_d3dParams, &m_Font ) );
        RETURN_ON_FAIL( m_Help.Create( "game:\\Media\\Help\\Help.xpr" ) );

        // Create events
        RETURN_ON_NULL( m_hNewFrameEvent = CreateEvent( NULL, TRUE, FALSE, NULL ) );
        RETURN_ON_NULL( m_hAvatarUpdatedEvent = CreateEvent( NULL, TRUE, FALSE, NULL ) );
        RETURN_ON_NULL( m_hSwitchLatencyModeEvent = CreateEvent( NULL, TRUE, FALSE, NULL ) );   
        RETURN_ON_NULL( m_hNewSkeletonDataEvent = CreateEvent( NULL, TRUE, FALSE, NULL ) );
        RETURN_ON_NULL( m_hNewColorDataEvent = CreateEvent( NULL, TRUE, FALSE, NULL ) );
        RETURN_ON_NULL( m_hNewDepthDataEvent = CreateEvent( NULL, TRUE, FALSE, NULL ) );

        // Initialize skeleton tracking
        RETURN_ON_FAIL( NuiInitialize( NUI_INITIALIZE_FLAG_USES_SKELETON |
                                       NUI_INITIALIZE_FLAG_USES_COLOR |     // NOTE: This sample doesn't do anything with color or depth, other than
                                       NUI_INITIALIZE_FLAG_USES_DEPTH,      // just retrieve the data in the most efficient way, about 7ms before joint data
                                       NUI_HW_THREAD ) );

        RETURN_ON_FAIL( NuiSkeletonTrackingEnable( m_hNewSkeletonDataEvent, NUI_SKELETON_TRACKING_FLAG_TITLE_SETS_TRACKED_SKELETONS ) );
       
        // Open color and depth streams with events
        RETURN_ON_FAIL( NuiImageStreamOpen( NUI_IMAGE_TYPE_COLOR, NUI_IMAGE_RESOLUTION_640x480, 0, 1, m_hNewColorDataEvent, &m_hColorStream ) );
        RETURN_ON_FAIL( NuiImageStreamOpen( NUI_IMAGE_TYPE_DEPTH, NUI_IMAGE_RESOLUTION_320x240, 0, 1, m_hNewDepthDataEvent, &m_hDepthStream ) );

        // Initialize critical sections
        InitializeCriticalSection( &m_csTimer );
        InitializeCriticalSection( &m_csCurrentFrameBufferIdx );

        m_bInitialized = TRUE;
    }

    // Initialize buffer data with initial values
    EnterCriticalSection( &m_csTimer );
    FLOAT fTime = ( FLOAT )m_Timer.GetAppTime();
    LeaveCriticalSection( &m_csTimer );

    for ( UINT i = 0; i < NUM_FRAME_BUFFERS; i++ )
    {
        m_Renderer.Update( &m_FrameBufferData[ i ], fTime );
        m_FrameBufferData[ i ].m_uFrameBufferIdx = i;
    }

    // Create thread functions
    const DWORD STACK_SIZE = 0;
    RETURN_ON_NULL( m_hRenderThread = CreateThread( NULL, STACK_SIZE, RenderThread, this, CREATE_SUSPENDED, NULL ) );
    XSetThreadProcessor( m_hRenderThread, RENDER_HW_THREAD );

    RETURN_ON_NULL( m_hGameUpdateThread = CreateThread( NULL, STACK_SIZE, GameUpdateThread, this, CREATE_SUSPENDED, NULL ) );
    XSetThreadProcessor( m_hGameUpdateThread, GAME_UPDATE_HW_THREAD );

    RETURN_ON_NULL( m_hColorUpdateThread = CreateThread( NULL, STACK_SIZE, ColorUpdateThread, this, CREATE_SUSPENDED, NULL ) );
    XSetThreadProcessor( m_hColorUpdateThread, COLOR_UPDATE_HW_THREAD );

    RETURN_ON_NULL( m_hDepthUpdateThread = CreateThread( NULL, STACK_SIZE, DepthUpdateThread, this, CREATE_SUSPENDED, NULL ) );
    XSetThreadProcessor( m_hDepthUpdateThread, DEPTH_UPDATE_HW_THREAD );

    RETURN_ON_NULL( m_hSkeletonTrackingUpdateThread = CreateThread( NULL, STACK_SIZE, SkeletonTrackingUpdateThread, this, CREATE_SUSPENDED, NULL ) );
    XSetThreadProcessor( m_hSkeletonTrackingUpdateThread, SKELETON_TRACKING_UPDATE_HW_THREAD );

    m_bDrawHelp = FALSE;

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Update
// Desc: Update game data and check controller input. Called from the GameUpdateThread.
//--------------------------------------------------------------------------------------

VOID CLatencySample::Update( const UINT uFrameBufferIdx )
{
    char szBuffer[ 128 ];
    sprintf_s( szBuffer, "Update %d", uFrameBufferIdx );
    PIXBeginNamedEvent( 0, szBuffer );

    EnterCriticalSection( &m_csTimer );
    FLOAT fTime = ( FLOAT )m_Timer.GetAppTime();
    LeaveCriticalSection( &m_csTimer );

    m_Renderer.Update( &m_FrameBufferData[ uFrameBufferIdx ], fTime );
    
    // Get the current gamepad status
    ATG::GAMEPAD* pGamepad = ATG::Input::GetMergedInput();

    // Toggle help
    if ( pGamepad->wPressedButtons & XINPUT_GAMEPAD_BACK )
    {
        m_bDrawHelp = !m_bDrawHelp;
    }

    // Toggle low/high latency sample
    if ( pGamepad->wPressedButtons & XINPUT_GAMEPAD_A )
    {
        SetEvent( m_hSwitchLatencyModeEvent );
    }

    // Toggle joint filtering
    if ( pGamepad->wPressedButtons & XINPUT_GAMEPAD_X )
    {
        m_bEnableJointFiltering = !m_bEnableJointFiltering;
    }

    PIXEndNamedEvent();
}


//--------------------------------------------------------------------------------------
// Name: UpdateAvatar
// Desc: Update the avatar data once we get new skeleton data
//--------------------------------------------------------------------------------------

VOID CLatencySample::UpdateAvatar( const UINT uFrameBufferIdx )
{
    char buffer[128];
    sprintf_s( buffer, "UpdateAvatar %d", uFrameBufferIdx );
    PIXBeginNamedEvent( 0, buffer );

    // Update avatar with new skeleton data
    EnterCriticalSection( &m_csTimer );
    FLOAT fTime = (FLOAT)m_Timer.GetAppTime();
    LeaveCriticalSection( &m_csTimer );

    m_Renderer.UpdateAvatar( &m_FrameBufferData[ uFrameBufferIdx ], fTime );

    PIXEndNamedEvent();
}


//--------------------------------------------------------------------------------------
// Name: GameUpdateThread
// Desc: GameUpdateThread function
//--------------------------------------------------------------------------------------

DWORD CLatencySample::GameUpdateThread( LPVOID lpParameter )
{  
    ATG::SetThreadName( GetCurrentThreadId(), "GameUpdateThread" );

    CLatencySample* pSample = (CLatencySample*)lpParameter;

    const HANDLE hEvents[] = { m_hSwitchLatencyModeEvent,
                               m_hNewFrameEvent };
    
    const DWORD dwNumEvents = ARRAYSIZE( hEvents );

    for ( ;; )
    {
        DWORD dwResult = WaitForMultipleObjects( dwNumEvents, hEvents, FALSE, INFINITE );
        DWORD dwIndex = dwResult - WAIT_OBJECT_0;

        if ( dwIndex < dwNumEvents )
        {
            if ( hEvents[ dwIndex ] == m_hNewFrameEvent )
            {
                // Get the current frame buffer index
                EnterCriticalSection( &m_csCurrentFrameBufferIdx );
                UINT uFrameBufferIdx = m_uCurrentFrameBufferIdx;
                LeaveCriticalSection( &m_csCurrentFrameBufferIdx );

                char szBuffer[ 128 ];
                sprintf_s( szBuffer, "GameUpdate %d ", uFrameBufferIdx );
                PIXBeginNamedEvent( 0, szBuffer );

                // Render thread sets the event and this thread resets it
                ResetEvent( m_hNewFrameEvent );

                // Update game data
                pSample->Update( uFrameBufferIdx );

                PIXEndNamedEvent();
            }
            else
            {
                break;
            }
        }
    }

    return 0;
}


//--------------------------------------------------------------------------------------
// Name: RenderThread
// Desc: RenderThread function
//--------------------------------------------------------------------------------------

DWORD CLatencySample::RenderThread( LPVOID lpParameter )
{
    ATG::SetThreadName( GetCurrentThreadId(), "RenderThread" );

    CLatencySample* pSample = (CLatencySample*)lpParameter;

    // Switch thread ownership to this thread
    m_pd3dDevice->AcquireThreadOwnership();
    m_pCmdBufferDevice->AcquireThreadOwnership();

    for ( ;; )
    {
        // Get the current frame buffer index
        EnterCriticalSection( &m_csCurrentFrameBufferIdx );
        UINT uFrameBufferIdx = ( m_uCurrentFrameBufferIdx + 1 ) % NUM_FRAME_BUFFERS;
        LeaveCriticalSection( &m_csCurrentFrameBufferIdx );

        char szBuffer[ 128 ];
        sprintf_s( szBuffer, "Render %d ", uFrameBufferIdx );
        PIXBeginNamedEvent( 0, szBuffer );

        // Render
        pSample->Render( uFrameBufferIdx );

        // Finished rendering this buffer so toggle to next buffer
        EnterCriticalSection( &m_csCurrentFrameBufferIdx );
        m_uCurrentFrameBufferIdx++;
        m_uCurrentFrameBufferIdx %= NUM_FRAME_BUFFERS;
        LeaveCriticalSection( &m_csCurrentFrameBufferIdx );

        // Tell game update thread that this buffer is now available again
        SetEvent( m_hNewFrameEvent );

        // Check if the user wants to switch to a different mode
        if ( WaitForSingleObject( m_hSwitchLatencyModeEvent, 0 ) == WAIT_OBJECT_0 )
        {
            PIXEndNamedEvent();
            break;
        }
        PIXEndNamedEvent();
    }

    // Release d3d thread ownership when the thread is done
    m_pd3dDevice->ReleaseThreadOwnership();
    m_pCmdBufferDevice->ReleaseThreadOwnership();

    return 0;
}


//--------------------------------------------------------------------------------------
// Name: SkeletonTrackingUpdateThread
// Desc: SkeletonTrackingUpdateThread function
//--------------------------------------------------------------------------------------

DWORD CLatencySample::SkeletonTrackingUpdateThread( LPVOID lpParameter )
{
    // Give this thread a name
    ATG::SetThreadName( GetCurrentThreadId(), "SkeletonTrackingUpdateThread" );

    CLatencySample* pSample = (CLatencySample*)lpParameter;

    const HANDLE hEvents[] = { m_hSwitchLatencyModeEvent,
                               m_hNewSkeletonDataEvent };

    const DWORD dwNumEvents = ARRAYSIZE( hEvents );

    for ( ;; )
    {
        DWORD dwResult = WaitForMultipleObjects( ARRAYSIZE( hEvents ), hEvents, FALSE, INFINITE );
        DWORD dwIndex = dwResult - WAIT_OBJECT_0;

        if ( dwIndex < dwNumEvents )
        {
            if ( hEvents[ dwIndex ] == m_hNewSkeletonDataEvent )
            {
                // For low latency sample, we single buffer the skeleton data, but for
                // high latency we double buffer the skeleton data.
                UINT uFrameBufferIdx = 0;
                if ( pSample->GetSampleType() == SAMPLE_TYPE_HIGH_LATENCY )
                {
                    EnterCriticalSection( &pSample->m_csCurrentFrameBufferIdx );
                    uFrameBufferIdx = pSample->m_uCurrentFrameBufferIdx;
                    LeaveCriticalSection( &pSample->m_csCurrentFrameBufferIdx );
                }

                char szBuffer[ 128 ];
                sprintf_s( szBuffer, "SkeletonTrackingUpdate %d ", uFrameBufferIdx );
                PIXBeginNamedEvent( 0, szBuffer );

                // We got new skeleton data, so retrieve it
                if ( pSample->UpdateSkeletonData( uFrameBufferIdx ) )
                {
                    // If we retrieved skeleton data succesfully, then update the avatar
                    pSample->UpdateAvatar( uFrameBufferIdx );
                }

                // Once update is done, signal the render thread that avatar has been updated
                // with up to date skeleton data
                SetEvent( m_hAvatarUpdatedEvent );

                PIXEndNamedEvent();

            }
            else
            {
                break;
            }
        }
    }

    return 0;
}


//--------------------------------------------------------------------------------------
// Name: ColorUpdateThread
// Desc: ColorUpdateThread function
//--------------------------------------------------------------------------------------

DWORD CLatencySample::ColorUpdateThread( LPVOID lpParameter )
{
    // Give this thread a name
    ATG::SetThreadName( GetCurrentThreadId(), "ColorUpdateThread" );

    CLatencySample* pSample = (CLatencySample*)lpParameter;

    for ( ;; )
    {
        if ( pSample->GetSampleType() == SAMPLE_TYPE_LOW_LATENCY )
        {
            // In low latency mode we use a seperate event per stream which tells us exactly when
            // the data is available
            PIXBeginNamedEvent( 0, "Wait for new color event" );
            WaitForSingleObject( m_hNewColorDataEvent, NUI_FRAME_END_TIMEOUT_DEFAULT );
            PIXEndNamedEvent();
        }
        else
        {
            // High latency mode will would use NuiSetFrameEndEvent() to set a FrameEnd event, not implemented in this sample,
            // but it would would only sginal after all color, depth and skeleton data is available. We're just using the
            // skeleton data event here for illustration purposes
            PIXBeginNamedEvent( 0, "Wait for FrameEnd event" );
            WaitForSingleObject( m_hNewSkeletonDataEvent, NUI_FRAME_END_TIMEOUT_DEFAULT );
            PIXEndNamedEvent();
        }

        PIXBeginNamedEvent( 0, "GetNewColorData" );
        const NUI_IMAGE_FRAME* pColorFrame;
        NuiImageStreamGetNextFrame( m_hColorStream, 0, &pColorFrame );
        if ( pColorFrame )
        {
            // Do something with the color here then release it
            NuiImageStreamReleaseFrame( m_hColorStream, pColorFrame );
        }
        PIXEndNamedEvent();

        ResetEvent( m_hNewColorDataEvent );

        // Check if the user wants to switch to a different mode
        if ( WaitForSingleObject( m_hSwitchLatencyModeEvent, 0 ) == WAIT_OBJECT_0 )
        {
            PIXEndNamedEvent();
            break;
        }
        
    }

    return 0;
}


//--------------------------------------------------------------------------------------
// Name: DepthUpdateThread
// Desc: DepthUpdateThread function
//--------------------------------------------------------------------------------------

DWORD CLatencySample::DepthUpdateThread( LPVOID lpParameter )
{
    // Give this thread a name
    ATG::SetThreadName( GetCurrentThreadId(), "DepthUpdateThread" );

    CLatencySample* pSample = (CLatencySample*)lpParameter;

    for ( ;; )
    {
        if ( pSample->GetSampleType() == SAMPLE_TYPE_LOW_LATENCY )
        {
            // In low latency mode we use a seperate event per stream which tells us exactly when
            // the data is available
            PIXBeginNamedEvent( 0, "Wait for new depth event" );
            WaitForSingleObject( m_hNewDepthDataEvent, NUI_FRAME_END_TIMEOUT_DEFAULT );
            PIXEndNamedEvent();
        }
        else
        {
            // High latency mode will would use NuiSetFrameEndEvent() to set a FrameEnd event, not implemented in this sample,
            // but it would would only sginal after all color, depth and skeleton data is available. We're just using the
            // skeleton data event here for illistration purposes
            PIXBeginNamedEvent( 0, "Wait for FrameEnd event" );
            WaitForSingleObject( m_hNewSkeletonDataEvent, NUI_FRAME_END_TIMEOUT_DEFAULT );
            PIXEndNamedEvent();
        }

        PIXBeginNamedEvent( 0, "GetNewDepthData" );
        const NUI_IMAGE_FRAME* pDepthFrame;
        NuiImageStreamGetNextFrame( m_hDepthStream, 0, &pDepthFrame );
        if ( pDepthFrame )
        {
            // Do something with the depth here then release it
            NuiImageStreamReleaseFrame( m_hDepthStream, pDepthFrame );
        }
        PIXEndNamedEvent();

        ResetEvent( m_hNewDepthDataEvent );

        // Check if the user wants to switch to a different mode
        if ( WaitForSingleObject( m_hSwitchLatencyModeEvent, 0 ) == WAIT_OBJECT_0 )
        {
            PIXEndNamedEvent();
            break;
        }
        
    }

    return 0;
}


//--------------------------------------------------------------------------------------
// Name: Run
// Desc: Run the sample
//--------------------------------------------------------------------------------------

VOID CLatencySample::Run()
{
    // Release d3d thread ownership so that render thread can acquire it
    m_pd3dDevice->ReleaseThreadOwnership();
    m_pCmdBufferDevice->ReleaseThreadOwnership();

    // Resume threads
    ResumeThread( m_hRenderThread );
    ResumeThread( m_hGameUpdateThread );
    ResumeThread( m_hColorUpdateThread );
    ResumeThread( m_hDepthUpdateThread );
    ResumeThread( m_hSkeletonTrackingUpdateThread );

    for ( ;; )
    {
        if ( WaitForSingleObject( m_hSwitchLatencyModeEvent, 0 ) == WAIT_OBJECT_0 )
        {
            // When the user switcheds to another mode, reset the sample
            Reset();
            break;
        }
    }
}


//--------------------------------------------------------------------------------------
// Name: Reset
// Desc: Reset the sample
//--------------------------------------------------------------------------------------

VOID CLatencySample::Reset()
{
    // Wait for the thread procedures to finish
    WaitForSingleObject( m_hRenderThread, INFINITE );
    WaitForSingleObject( m_hGameUpdateThread, INFINITE );
    WaitForSingleObject( m_hColorUpdateThread, INFINITE );
    WaitForSingleObject( m_hDepthUpdateThread, INFINITE );
    WaitForSingleObject( m_hSkeletonTrackingUpdateThread, INFINITE );
   
    // Close and clean threads
    CloseHandle( m_hRenderThread );
    CloseHandle( m_hGameUpdateThread );
    CloseHandle( m_hColorUpdateThread );
    CloseHandle( m_hDepthUpdateThread );
    CloseHandle( m_hSkeletonTrackingUpdateThread );
    m_hRenderThread = NULL;
    m_hGameUpdateThread = NULL;
    m_hColorUpdateThread = NULL;
    m_hDepthUpdateThread = NULL;
    m_hSkeletonTrackingUpdateThread = NULL;

    // Reset events
    ResetEvent( m_hSwitchLatencyModeEvent );
    ResetEvent( m_hNewSkeletonDataEvent );
    ResetEvent( m_hNewColorDataEvent );
    ResetEvent( m_hNewDepthDataEvent );
    ResetEvent( m_hAvatarUpdatedEvent );
    ResetEvent( m_hNewFrameEvent );

    // Render thread has release d3d thread ownership so acquire thread ownership again
    m_pd3dDevice->AcquireThreadOwnership();
    m_pCmdBufferDevice->AcquireThreadOwnership();

    // Reset the renderer
    m_Renderer.Reset();

}

//--------------------------------------------------------------------------------------
// Name: CreateDevice
// Desc: Create the d3d device
//--------------------------------------------------------------------------------------

HRESULT CLatencySample::CreateDevice()
{
    D3DRING_BUFFER_PARAMETERS RingBufferParams = { 0 };
    D3DVIDEO_SCALER_PARAMETERS ScalerParams = { 0 };

    m_d3dParams.BackBufferWidth             = 1280;
    m_d3dParams.BackBufferHeight            = 720;
    m_d3dParams.BackBufferFormat            =(D3DFORMAT)MAKESRGBFMT(D3DFMT_A8R8G8B8);
    m_d3dParams.BackBufferCount             = 1;
    m_d3dParams.MultiSampleType             = D3DMULTISAMPLE_2_SAMPLES;
    m_d3dParams.MultiSampleQuality          = 0;
    m_d3dParams.SwapEffect                  = D3DSWAPEFFECT_DISCARD;
    m_d3dParams.hDeviceWindow               = NULL;
    m_d3dParams.Windowed                    = FALSE;
    m_d3dParams.EnableAutoDepthStencil      = FALSE;
    m_d3dParams.AutoDepthStencilFormat      = D3DFMT_D24S8;
    m_d3dParams.Flags                       = 0;
    m_d3dParams.FullScreen_RefreshRateInHz  = 0;
    m_d3dParams.PresentationInterval        = D3DPRESENT_INTERVAL_TWO;
    m_d3dParams.DisableAutoBackBuffer       = TRUE;
    m_d3dParams.DisableAutoFrontBuffer      = TRUE;
    m_d3dParams.FrontBufferFormat           = (D3DFORMAT)MAKESRGBFMT(D3DFMT_LE_X8R8G8B8);
    m_d3dParams.FrontBufferColorSpace       = D3DCOLORSPACE_RGB;
    m_d3dParams.RingBufferParameters        = RingBufferParams;
    m_d3dParams.VideoScalerParameters       = ScalerParams;

    RETURN_ON_FAIL( m_Renderer.CreateDevice( &m_d3dParams ) );

    m_pd3dDevice = m_Renderer.m_pd3dDevice;
    m_pCmdBufferDevice = m_Renderer.m_pCmdBufferDevice;

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: UpdateSkeletonData
// Desc: Retrieve the most up to date skeleton data
//--------------------------------------------------------------------------------------

BOOL CLatencySample::UpdateSkeletonData( const UINT uFrameBufferIdx )
{
    if ( SUCCEEDED( NuiSkeletonGetNextFrame( 0, &m_FrameBufferData[ uFrameBufferIdx ].m_SkeletonFrame ) ) )
    {
        UpdateActiveSkeleton( uFrameBufferIdx );
        FilterJointPositions( uFrameBufferIdx );
        return TRUE;
    }

    return FALSE;
}


//--------------------------------------------------------------------------------------
// Name: UpdateActiveSkeleton
// Desc: Select the closest person as the active skeleton
//--------------------------------------------------------------------------------------

VOID CLatencySample::UpdateActiveSkeleton( const UINT uFrameBufferIdx )
{   
    NUI_SKELETON_FRAME* pSkeletonFrame = &m_FrameBufferData[ uFrameBufferIdx ].m_SkeletonFrame;

    // Intialize to no skeletons active
    DWORD dwTrackingIDs[ NUI_SKELETON_MAX_TRACKED_COUNT ] = { 0, 0 };

    INT iClosestBody = -1;
    FLOAT fMinDistance = FLT_MAX;

    // Find the closest skeleton to the center of the playspace
    for ( UINT i = 0; i < NUI_SKELETON_COUNT; i++ )
    {
        // If not tracked, then ignore
        if ( pSkeletonFrame->SkeletonData[ i ].eTrackingState == NUI_SKELETON_NOT_TRACKED )
        {
            continue;
        }

        // Find the positions closest to the camera
        XMVECTOR vTemp = XMVectorSubtract( pSkeletonFrame->SkeletonData[ i ].Position, XMVectorZero() );

        // Calculate the distance between the center and tracked position
        FLOAT fCurDistance = XMVector3LengthSq( vTemp ).x; 
        if ( fCurDistance < fMinDistance )
        {
            iClosestBody = i;
            fMinDistance = fCurDistance;
        }              
    }

    // Only swap if there is a closest and it is not already in the first slot
    if ( iClosestBody != -1 )
    {
        dwTrackingIDs[ 0 ] = pSkeletonFrame->SkeletonData[ iClosestBody ].dwTrackingID;
    }

    // if the new skeleton index is different from the old one, then recreate the avatar retargeting
    if ( iClosestBody != m_FrameBufferData[ uFrameBufferIdx ].m_iSkeletonIdx )
    {
        m_FrameBufferData[ uFrameBufferIdx ].m_iSkeletonIdx = iClosestBody;

        // This call will either specify no players to have skeletons for if none have been found
        // or the one closest to the 'center' as the one to have a skeleton
        NuiSkeletonSetTrackedSkeletons( dwTrackingIDs );
    }
}


//--------------------------------------------------------------------------------------
// Name: RenderUI
// Desc: Render the sample UI
//--------------------------------------------------------------------------------------

VOID CLatencySample::RenderUI()
{
    PIXBeginNamedEvent( 0, "RenderUI" );

    EnterCriticalSection( &m_csTimer );
    m_Timer.MarkFrame();
    LeaveCriticalSection( &m_csTimer );

    if ( m_bDrawHelp )
    {
        m_Help.Render( &m_Font, g_HelpCallouts, NUM_HELP_CALLOUTS );
    }
    else
    {
        FLOAT y = 40.0f;

        m_Font.Begin();
        m_Font.SetScaleFactors( 1.2f, 1.2f );
        m_Font.DrawText( 0, 0, 0xffffffff, L"Reducing Latency" );

        m_Font.SetScaleFactors( 1.0f, 1.0f );
        EnterCriticalSection( &m_csTimer );
        m_Font.DrawText( 0, 0, 0xffffff00, m_Timer.GetFrameRate(), ATGFONT_RIGHT );
        LeaveCriticalSection( &m_csTimer );

        m_Font.SetScaleFactors( 0.9f, 0.9f );
        m_Font.DrawText( 0, y, 0xffffffff, L"Sample Mode:" );
        m_Font.DrawText( 175, y, 0xffffff00, m_SampleType == SAMPLE_TYPE_LOW_LATENCY ? L"Low Latency" : L"High Latency");
        y += 20;

        m_Font.DrawText( 0, y, 0xffffffff, L"Joint Smoothing:" );
        m_Font.DrawText( 175, y, 0xffffff00, m_bEnableJointFiltering ? ( m_SampleType == SAMPLE_TYPE_LOW_LATENCY ? L"Taylor Series" : L"Average" ) : L"None" );
        y += 20;

        m_Font.DrawText( 0, y, 0xffffffff, L"Anti-Aliasing:" );
        m_Font.DrawText( 175, y, 0xffffff00, m_SampleType == SAMPLE_TYPE_LOW_LATENCY ? L"ScreenSpace AA" : L"2x MSAA" );
        y += 20;

        m_Font.DrawText( 0, y, 0xffffffff, L"Predicated Tiling:" );
        m_Font.DrawText( 175, y, 0xffffff00, m_SampleType == SAMPLE_TYPE_LOW_LATENCY ? L"None" : L"2 Tiles" );
        y += 20;

        m_Font.DrawText( 0, y, 0xffffffff, L"GPU_2_FRAMES:" );
        m_Font.DrawText( 175, y, 0xffffff00, m_SampleType == SAMPLE_TYPE_LOW_LATENCY ? L"No" : L"Yes" );
        y += 20;

        m_Font.DrawText( 0, y, 0xffffffff, L"Scene Resources:" );
        m_Font.DrawText( 175, y, 0xffffff00, L"Double Buffered" );
        y += 20;

        m_Font.DrawText( 0, y, 0xffffffff, L"Avatar Resources:" );
        m_Font.DrawText( 175, y, 0xffffff00, m_SampleType == SAMPLE_TYPE_LOW_LATENCY ? L"Single Buffered" : L"Double Buffered" );
        y += 20;

        m_Font.End();
    }
    PIXEndNamedEvent();

}
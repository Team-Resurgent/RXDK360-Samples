//--------------------------------------------------------------------------------------
//
// AdvancedXAV.cpp
//
// Sample showing the additional functionality of the XAV APIs
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include <xtl.h>
#include <xboxmath.h>
#include <xgraphics.h>
#include <AtgApp.h>
#include <AtgDebugDraw.h>
#include <AtgFont.h>
#include <AtgHelp.h>
#include <AtgInput.h>
#include <AtgResource.h>
#include <AtgSignin.h>
#include <AtgSimpleShaders.h>
#include <AtgUtil.h>
#include <xaudio2.h>
#include <xav.h>
#include <xauth.h>

const CHAR* g_strMovieName = "mbr://smooth.ch9.ms/ch9/b021/c55c43d3-9867-4140-a69b-9f420181b021/CB2011HerbSutterWhyCppFinal.ism/manifest";
const CHAR* g_strPlayReadyMovieName = "mbr://playready.directtaps.net/smoothstreaming/TTLSS720VC1PR/To_The_Limit_720.ism/Manifest";
BOOL g_bUsePlayReady = FALSE;

// XMemFree attributes for XAV's PlayReady module
static const DWORD s_dwMemAllocAttributes = MAKE_XALLOC_ATTRIBUTES(
    1,                                  /* ObjectType */
    TRUE,                               /* HeapTracksAttributes */
    FALSE,                              /* MustSucceed */
    FALSE,                              /* FixedSize */
    eXALLOCAllocatorId_XAVPipeline,     /* AllocatorId */
    XALLOC_ALIGNMENT_DEFAULT,           /* Alignment */
    XALLOC_MEMPROTECT_READWRITE,        /* MemoryProtect */
    FALSE,                              /* ZeroInitialize */
    XALLOC_MEMTYPE_HEAP                 /* MemoryType */
    );

#define SAFE_XAV_FREE(ptr) \
    XMemFree( ptr, s_dwMemAllocAttributes ); \
    ptr = NULL;

// Playback rates.  Negative is rewind
const FLOAT g_fPlaybackRates[] = {
    -16.0f,
    -8.0f,
    -4.0f,
    -2.0f,
    -1.0f,
    1.0f,
    2.0f,
    4.0f,
    8.0f,
    16.0f,
};
// Default playback rate of 1.0 is normal speed playback
const INT g_iDefaultPlaybackRate = 5;

// XAV using 100 nanosecond units to measure video position and duration
const LONGLONG HNStoSec = 10000000; // 100 Nanoseconds in a Second


//--------------------------------------------------------------------------------------
// Callouts for labelling the gamepad on the help screen
//--------------------------------------------------------------------------------------
ATG::HELP_CALLOUT g_HelpCallouts[] =
{
    { ATG::HELP_BACK_BUTTON, ATG::HELP_PLACEMENT_2, L"Display\nhelp" },
    { ATG::HELP_A_BUTTON, ATG::HELP_PLACEMENT_2, L"Play/Pause\nMovie" },
    { ATG::HELP_B_BUTTON, ATG::HELP_PLACEMENT_2, L"Stop Movie\nShutdown XAV" },
    { ATG::HELP_X_BUTTON, ATG::HELP_PLACEMENT_1, L"Signin" },
    { ATG::HELP_LEFT_SHOULDER, ATG::HELP_PLACEMENT_1, L"Seek +10s" },
    { ATG::HELP_RIGHT_SHOULDER, ATG::HELP_PLACEMENT_1, L"Seek -10s" },
    { ATG::HELP_LEFT_TRIGGER, ATG::HELP_PLACEMENT_1, L"Rewind" },
    { ATG::HELP_RIGHT_TRIGGER, ATG::HELP_PLACEMENT_1, L"Fast Forward" },
};
static const DWORD NUM_HELP_CALLOUTS = ARRAYSIZE( g_HelpCallouts );

//--------------------------------------------------------------------------------------
// Name: CleanLicenseStore()
// Desc: Removes unusable licenses from the PlayReady license store.
//--------------------------------------------------------------------------------------
DWORD WINAPI CleanLicenseStore( LPVOID lpThreadParameter )
{
    IXAVLicenseAcquisition *pLicAcq;
    HRESULT hr = CreateXAVLicenseAcquisition(&pLicAcq);
    if( FAILED(hr) )
    {
        ATG::FatalError( "Error %#X calling CreateXAVLicenseAcquisition\n", hr );
    }
    hr = pLicAcq->CleanLicenseStore();
    if( FAILED(hr) )
    {
        ATG::FatalError( "Error %#X calling CleanLicenseStore\n", hr );
    }
    pLicAcq->Release();
    return hr;
}

//--------------------------------------------------------------------------------------
// Name: struct XAVCallbackHelper
// Desc: Simple COM helper class for the XAV events callbacks
//--------------------------------------------------------------------------------------
class XAVCallbackHelper : public IXAVPlayerEvents {
    DWORD ReferenceCount;

    IXAVLicenseAcquisition *m_pLicAcq;

public:
    XAVCallbackHelper()
    {
        ReferenceCount = 1;
        m_pLicAcq = NULL;
    }

    // COM Query Interface for IXAVPlayerEvents
    virtual HRESULT QueryInterface(REFIID iid, LPVOID *ppv)
    {
        if( !ppv )
            return E_INVALIDARG;

        if( iid == __uuidof(IUnknown) || iid == __uuidof(IXAVPlayerEvents) )
        {
            *ppv = static_cast<IXAVPlayerEvents*>(this);
            return S_OK;
        }

        *ppv = NULL;
        return E_NOINTERFACE;
    }

    // COM AddRef/Release for Reference Counting
    virtual ULONG AddRef()
    {
        return (ULONG)InterlockedIncrement((LONG*)&ReferenceCount);
    }

    virtual ULONG Release()
    {
        ULONG ref = (ULONG)InterlockedDecrement((LONG*)&ReferenceCount);
        if (ref == 0)
            delete this;
        return ref;
    }

    // IXAVPlayerEvents Callbacks
    virtual void StatusChanged(const IXAVPlayerStatus* pStatus) {}
    virtual void PositionUpdate(const IXAVPlayerStatus* pStatus) {}

    virtual void Alert(XAVPlayerAlert* pAlert, const IXAVPlayerStatus* pStatus)
    {
        if ( pAlert->Level     == XAVPlayerAlert::Warning 
          && pAlert->Component == XAVPlayerAlert::Playback
          && pAlert->dwType    == XAVPlayerAlert::PlaybackNoLicense)
        {
            HRESULT hr = S_OK;
            // Create a LicenseAcquisition object from the IUnknown in the Alert.
            hr = CreateXAVLicenseAcquisition(pAlert->pUnkDetail, &m_pLicAcq);
            assert(hr == S_OK);
            DoLicenseAcquisition();
            m_pLicAcq->Release();
            // Clean up license store
            // Your application should clean up the license store if it plays back DRM content with temporary licenses
            // This should take place outside the UI thread
            // This should be done periodically.
            // A good rule of thumb is to clean the license store after a successful license acquisition
            HANDLE hThreadCleanLicenseStore = CreateThread( NULL, 0, CleanLicenseStore, NULL, 0, NULL );
            CloseHandle(hThreadCleanLicenseStore);
        }
    }

private:
    // Performs the PlayReady license acquisition
    // Best if done off the UI thread - it might take a while if it needs to retry
    HRESULT DoLicenseAcquisition()
    {
        HRESULT hr          = S_OK;
        HRESULT hrFromLicenseResponse = S_FALSE;
        CHAR   *pszUrl      = NULL;
        BYTE   *pbChallenge = NULL;
        DWORD   cbChallenge = 0;
        BYTE   *pbResponse  = NULL;
        DWORD   cbResponse  = 0;

        if ( SUCCEEDED(m_pLicAcq->GenerateLicenseAcquisitionChallenge(
            NULL,
            0,
            NULL,
            &cbChallenge,
            &pbChallenge,
            &pszUrl )) )
        {
            //S_FALSE means that the request was redirected. Start in this state and keep sending to the new URL until we get S_OK or fail.
            while( hrFromLicenseResponse == S_FALSE)
            {

                if (FAILED(m_pLicAcq->SendLicenseAcquisitionChallenge(
                    pszUrl ,
                    pbChallenge,
                    cbChallenge,
                    &cbResponse,
                    &pbResponse )))
                    break;

                // need to clear the memory here because ProcessLicenseAcquisitionResponse will allocate over it.
                SAFE_XAV_FREE( pszUrl );

                hrFromLicenseResponse = m_pLicAcq->ProcessLicenseAcquisitionResponse(
                    S_OK,
                    pbResponse,
                    cbResponse,
                    &pszUrl,
                    NULL );

                if( FAILED(hrFromLicenseResponse) )
                {
                    hr = hrFromLicenseResponse;
                    goto done;
                }
            }
        }

done:
        SAFE_XAV_FREE( pszUrl );
        SAFE_XAV_FREE( pbResponse );
        SAFE_XAV_FREE( pbChallenge );

        return hr;
    }
};

//--------------------------------------------------------------------------------------
// Name: class Sample
// Desc: Main class to run this application. Most functionality is inherited
//       from the ATG::Application base class.
//--------------------------------------------------------------------------------------
class Sample : public ATG::Application
{
    ATG::Timer m_Timer;
    ATG::Font m_Font;
    ATG::Help m_Help;
    BOOL m_bDrawHelp;

    IXAVPlayer* m_pXAVPlayer;
    IXAVPlayerStatus* m_pXAVPlayerStatus;
    IXAVPlayerGetTexture* m_pXAVGetTexture;
    IXAVPropertyStore* m_pXAVPropertyStore;
    BOOL m_bWaitForFirstFrameDirty;

    XAVCallbackHelper* m_pXAVCallbacks;

    INT m_iPlaybackRate;
    DWORD m_dwMovie;

private:
    virtual HRESULT Initialize();
    virtual HRESULT Update();
    virtual HRESULT Render();

    HRESULT InitializeXAV();
    HRESULT ShutdownXAV();

    void SeekRelative( LONGLONG llSeek );
    void RenderProgressBar( D3DRECT rectBar, D3DCOLOR dwColor, FLOAT fPercent );
    void PrintTime( FLOAT sx, FLOAT sy, D3DCOLOR dwColor, ULONGLONG ullTime );
};

//--------------------------------------------------------------------------------------
// Name: main()
// Desc: Entry point to the program
//--------------------------------------------------------------------------------------
VOID __cdecl main()
{
    // Run the application
    Sample atgApp;
    ATG::GetVideoSettings( &atgApp.m_d3dpp.BackBufferWidth, &atgApp.m_d3dpp.BackBufferHeight );
    atgApp.m_d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_ONE;
    atgApp.Run();
}


//--------------------------------------------------------------------------------------
// Name: Initialize()
// Desc: This creates all device-dependent display objects.
//--------------------------------------------------------------------------------------
HRESULT Sample::Initialize()
{
    // Initialize simple shaders.
    ATG::SimpleShaders::Initialize( NULL, NULL );

    // Create the font
    if( FAILED( m_Font.Create( "game:\\Media\\Fonts\\Arial_16.xpr" ) ) )
    {
        return ATGAPPERR_MEDIANOTFOUND;
    }

    // Confine text drawing to the title safe area
    m_Font.SetWindow( ATG::GetTitleSafeArea() );

    // Create the help
    m_bDrawHelp = FALSE;
    if( FAILED( m_Help.Create( "game:\\Media\\Help\\Help.xpr" ) ) )
    {
        return ATGAPPERR_MEDIANOTFOUND;
    }

    m_pXAVPlayer = NULL;
    m_pXAVPlayerStatus = NULL;
    m_pXAVGetTexture = NULL;
    m_pXAVCallbacks = NULL;

    m_iPlaybackRate = g_iDefaultPlaybackRate;
    m_dwMovie = 0;

    // Initialize login
    ATG::SignIn::Initialize( 1, 1, TRUE, 1 );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: InitializeXAV()
// Desc: This creates the XAV player and loads the movie
//--------------------------------------------------------------------------------------
HRESULT Sample::InitializeXAV()
{
    // Initialize XAuth for connecting to server to stream a movie
    XAUTH_SETTINGS settings;
    ZeroMemory( &settings, sizeof(settings) );
    settings.SizeOfStruct = sizeof(settings);
    // For simplicity and testing, this sample use BYPASS_SECURITY
    // A title will need to setup endpoints in the Network Security Authorization List (NSAL)
    // For more information on how to do this, please see the Simple XAuth Sample and XAuth documentation
    settings.Flags = XAUTH_FLAG_BYPASS_SECURITY;
    HRESULT hr = XAuthStartup(&settings);
    if( FAILED(hr) )
    {
        ATG::FatalError( "Error %#X calling XAuthStartup\n", hr );
    }

    // Create the XAV Interface
    // This should be done from the D3D owning thread
    XAVInit XAVInit;
    ZeroMemory( &XAVInit, sizeof(XAVInit) );
    XAVInit.pDevice = m_pd3dDevice;
    XAVInit.pPresentParam = &m_d3dpp;
    DWORD ThreadAffinities[XAVMaxThreadTypes];
    // XAV Thread Affinity
    // For best performance, you should ensure these are not assigned to the
    // same thread which owns the D3D device
    ThreadAffinities[XAVGlobalMediaWorkThread] = c_dwUseDefaultAffinity;
    ThreadAffinities[XAVPlayerWorkThread] = c_dwUseDefaultAffinity;
    ThreadAffinities[XAVVideoWorkThread] = c_dwUseDefaultAffinity;

    hr = XAVInitialize( &XAVInit, ThreadAffinities );
    if( FAILED(hr) )
    {
        ATG::FatalError( "Error %#X calling XAVInititialize\n", hr );
    }

    // Initialize the XAV Player Object
    m_pXAVCallbacks = new XAVCallbackHelper();
    hr = CreateXAVPlayer( &m_pXAVPlayer, m_pXAVCallbacks, NULL );
    if( FAILED(hr) )
    {
        ATG::FatalError( "Error %#X calling CreateXAVPlayer\n", hr );
    }

    // Obtain the PlayerStatus Interface
    hr = m_pXAVPlayer->GetStatus( &m_pXAVPlayerStatus );
    if( FAILED(hr) )
    {
        ATG::FatalError( "Error %#X calling GetStatus\n", hr );
    }

    // Obtain the GetTexture Interface
    hr = m_pXAVPlayer->GetPlayerFrameInfo( &m_pXAVGetTexture );
    if( FAILED(hr) )
    {
        ATG::FatalError( "Error %#X calling GetPlayerFrameInfo\n", hr );
    }

    // Obtain the PropertyStore Interface
    hr = m_pXAVPlayer->GetPlayerPropertyStore( &m_pXAVPropertyStore );
    if( FAILED(hr) )
    {
        ATG::FatalError( "Error %#X calling GetPlayerPropertyStore\n", hr );
    }

    // Open the Movie File
    const CHAR* strMovieName = NULL;
    if ( g_bUsePlayReady )
    {
        // Enable PlayReady support in the XAV player
        XAVPropertyValue val;
        val.m_Type = XAVPropertyValue::BOOLProperty;
        val.m_Value.fValue = TRUE;
        m_pXAVPropertyStore->SetValue( XAVOpenParamUsePlayReady, &val );
        strMovieName = g_strPlayReadyMovieName;
    }
    else
    {
        strMovieName = g_strMovieName;
    }
    hr = m_pXAVPlayer->Open( strMovieName );
    if( FAILED(hr) )
    {
        ATG::FatalError( "Error %#X calling IXAVPlayer::Open for %s\n", hr, strMovieName );
    }

    // Set play back to rewind to the beginning when stopped
    XAVPropertyValue val;
    val.m_Type = XAVPropertyValue::BOOLProperty;
    val.m_Value.fValue = TRUE;
    m_pXAVPropertyStore->SetValue( XAVPlayerResetPositionOnStop, &val );

    m_bWaitForFirstFrameDirty = TRUE;

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: ShutdownXAV()
// Desc: This destroys the XAV player - freeing up all used memory
//--------------------------------------------------------------------------------------
HRESULT Sample::ShutdownXAV()
{
    // Destroy XAV Player and Interfaces
    // This should be done from the D3D owning thread
    if( m_pXAVGetTexture ) m_pXAVGetTexture->Release();
    m_pXAVGetTexture = NULL;

    if( m_pXAVPlayerStatus ) m_pXAVPlayerStatus->Release();
    m_pXAVPlayerStatus = NULL;

    if( m_pXAVPlayer ) m_pXAVPlayer->Release();
    m_pXAVPlayer = NULL;

    if( m_pXAVCallbacks ) m_pXAVCallbacks->Release();
    m_pXAVCallbacks = NULL;

    if( m_pXAVPropertyStore ) m_pXAVPropertyStore->Release();
    m_pXAVPropertyStore = NULL;

    XAVShutdown();

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Update()
// Desc: Called once per frame, the call is the entry point for animating the scene.
//       The movie is played from here.
//--------------------------------------------------------------------------------------
HRESULT Sample::Update()
{
    // Get the current gamepad state
    ATG::GAMEPAD* pGamepad = ATG::Input::GetMergedInput();

    // If the movie is complete, shutdown XAV to freeup memory
    // If you wanted to replay or play a new movie, you could keep XAV initialized
    if( m_pXAVPlayerStatus )
    {
        // Get the latest status
        m_pXAVPlayerStatus->Refresh();
        // Check for Error
        // XAVPlayerStateError can occur if you signout while playing a streaming movie
        XAVPlayerState state = m_pXAVPlayer->GetPlayerState();
        if( m_pXAVPlayerStatus->IsEndOfMedia() || state == XAVPlayerStateError )
        {
            ShutdownXAV();
        }
    }

    // Press A to Play/Pause the Movie
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_A )
    {
        if( m_pXAVPlayer )
        {
            // XAV Playback Status
            XAVPlayerState state = m_pXAVPlayer->GetPlayerState();
            // Toggle Play / Pause
            if( state == XAVPlayerStatePlaying )
            {
                // Playing -> Paused
                m_pXAVPlayer->Pause();
            }
            else if( state == XAVPlayerStatePaused )
            {
                // Pause -> Playing
                // Do not assume the rate is still set after pausing
                m_pXAVPlayer->SetRate( g_fPlaybackRates[ m_iPlaybackRate ] );
                m_pXAVPlayer->Play();
            }
            else
            {
                // Stopped -> Playing
                m_bWaitForFirstFrameDirty = TRUE;
                // Do not assume the rate is still set after pausing
                m_pXAVPlayer->SetRate( g_fPlaybackRates[ m_iPlaybackRate ] );
                m_pXAVPlayer->Play();
            }
        }
        else
        {
            // Not Currently Playing - Start Playback
            InitializeXAV();
            // Reset the playback rate
            m_iPlaybackRate = g_iDefaultPlaybackRate;
            // Not Currently Playing - Start Playback
            m_bWaitForFirstFrameDirty = TRUE;
            m_pXAVPlayer->SetRate( g_fPlaybackRates[ m_iPlaybackRate ] );
            m_pXAVPlayer->Play();
        }
    }

    // Press Y to switch between UNENCRYPTED and PLAYREADY content
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_Y )
    {
        g_bUsePlayReady = !g_bUsePlayReady;
        if( m_pXAVPlayer )
        {
            // Stop and shutdown
            m_pXAVPlayer->Stop();
            ShutdownXAV();
        }
    }

    // Press B to Stop the Movie - and shutdown XAV
    // If you wanted to replay or play a new movie, you could keep XAV initialized
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_B )
    {
        if( m_pXAVPlayer )
        {
            // If we are already stopped, shutdown and cleanup XAV
            XAVPlayerState state = m_pXAVPlayer->GetPlayerState();
            if( state == XAVPlayerStateStopped || state == XAVPlayerStateError )
            {
                // Stop and shutdown
                m_pXAVPlayer->Stop();
                ShutdownXAV();
            }
            else
            {
                // Stop playback but leave XAV initialized
                // Play will resume from the movie's beginning due to XAVPlayerResetPositionOnStop
                m_pXAVPlayer->Stop();
            }
        }
    }

    // Press LB/RB to seek back/forward
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_LEFT_SHOULDER )
    {
        // Seek -10 seconds
        if( m_pXAVPlayer )
        {
            SeekRelative( -10 * HNStoSec );
        }
    }

    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER )
    {
        // Seek +10 seconds
        if( m_pXAVPlayer )
        {
            SeekRelative( 10 * HNStoSec );
        }
    }

    // Press LT/RT to rewind/fast forward
    if( pGamepad->bPressedRightTrigger )
    {
        if( m_pXAVPlayer )
        {
            // Speed up playback
            m_iPlaybackRate++;
            if( m_iPlaybackRate >= ARRAYSIZE( g_fPlaybackRates ) )
            {
                m_iPlaybackRate = ARRAYSIZE( g_fPlaybackRates ) - 1;
            }
            m_pXAVPlayer->SetRate( g_fPlaybackRates[ m_iPlaybackRate ] );
        }
    }

    if( pGamepad->bPressedLeftTrigger )
    {
        if( m_pXAVPlayer )
        {
            // Slow down playback
            m_iPlaybackRate--;
            if( m_iPlaybackRate < 0 )
            {
                m_iPlaybackRate = 0;
            }
            m_pXAVPlayer->SetRate( g_fPlaybackRates[ m_iPlaybackRate ] );
        }
    }

    // Update signin
    ATG::SignIn::Update();

    // Signin UI
    if( !ATG::SignIn::IsSystemUIShowing() )
    {
        // Show signin UI
        if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_X )
        {
            ATG::SignIn::ShowSignInUI();
        }
    }
    
    // Toggle help
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_BACK )
    {
        m_bDrawHelp = !m_bDrawHelp;
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: SeekRelative()
// Desc: Seeks the video forward/back relative to the current playback position
//--------------------------------------------------------------------------------------
void Sample::SeekRelative( LONGLONG llSeek )
{
    ULONGLONG ullPosition = m_pXAVPlayerStatus->GetPosition();
    ULONGLONG ullDuration = m_pXAVPlayerStatus->GetDuration();

    // Calculate the new position
    ullPosition += llSeek;

    if( (LONGLONG)ullPosition < 0ULL )
        ullPosition = 0; // Near to the start, seek to the start
    if( ullPosition > ullDuration )
        ullPosition = ullDuration; // Near to the end, seek to the end

    // Seek
    m_pXAVPlayer->SeekAbsolute( ullPosition );
}


//--------------------------------------------------------------------------------------
// Name: RenderProgressBar()
// Desc: Renders a simple progress bar
//--------------------------------------------------------------------------------------
void Sample::RenderProgressBar( D3DRECT rect, D3DCOLOR dwColor, FLOAT fPercent )
{
    PIXBeginNamedEvent( 0, "Progress Bar: %i%%", (DWORD)(100 * fPercent) );
    D3DRECT rectOutline = { rect.x1 - 2, rect.y1 - 2, rect.x2 + 1, rect.y2 + 2 };
    ATG::DebugDraw::DrawScreenSpaceRect( rectOutline, 1, dwColor );
    rect.x2 = rect.x1 + (LONG)((rect.x2 - rect.x1) * fPercent);
    ATG::DebugDraw::DrawScreenSpaceRect( rect, 0, dwColor );
    PIXEndNamedEvent();
}


//--------------------------------------------------------------------------------------
// Name: PrintTime()
// Desc: Prints the time as H:MM:SS
//--------------------------------------------------------------------------------------
void Sample::PrintTime( FLOAT sx, FLOAT sy, D3DCOLOR dwColor, ULONGLONG ullTime )
{
    WCHAR strBuffer[10];
    DWORD dwSec  = (DWORD)(ullTime / HNStoSec);
    DWORD dwMin  = dwSec / 60;
    DWORD dwHour = dwMin / 60;
    dwSec -= dwMin * 60;
    dwMin -= dwHour * 60;
    swprintf_s( strBuffer, L"%i:%02i:%02i", dwHour, dwMin, dwSec );
    m_Font.DrawText( sx, sy, dwColor, strBuffer );
}


//--------------------------------------------------------------------------------------
// Name: Render()
// Desc: Sets up render states, clears the viewport, and renders the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Render()
{
    // Draw a gradient filled background
    ATG::RenderBackground( 0xff0000ff, 0xff000000 );

    // Render the movie frame
    if( m_pXAVPlayer )
    {
        XAVPlayerState state = m_pXAVPlayer->GetPlayerState();
        if( state == XAVPlayerStatePlaying || state == XAVPlayerStatePaused || state == XAVPlayerStateStarting )
        {
            IDirect3DTexture9* pTexture = NULL;
            XAVRenderResultInfo renderResultInfo = {};

            // Do not render anything until the first frame is available
            // If you don't do this, then on a second play through of the movie, GetTexture() will
            // return the last decoded frame until the first frame is ready.
            if( m_bWaitForFirstFrameDirty && m_pXAVGetTexture->IsTextureDirty() )
            {
                // The texture is dirty, meaning a newly decoded frame is ready
                m_bWaitForFirstFrameDirty = FALSE;
            }
            if( !m_bWaitForFirstFrameDirty )
            {
                // Obtain the texture for the current frame of the movie
                if( SUCCEEDED(m_pXAVGetTexture->GetTexture( FALSE, &pTexture, &renderResultInfo )) )
                {
                    // Note: If you previously played a movie, then on a second play through, GetTexture() will
                    // return the previously decoded frame until the first frame is ready.  Use IsTextureDirty()
                    // to determine when the first frame of the new movie is available

                    // Retrieve texture dimension
                    D3DSURFACE_DESC textureDesc = {};
                    pTexture->GetLevelDesc( 0, &textureDesc );

                    // Important:
                    // The resulting decoded video frame might not fill the entire texture's
                    // dimensions.  XAVRenderResultInfo::resultRect describes the size of the
                    // decoded frame and can be used to calculate the correct texture coordinates
                    D3DRECT ScreenSpaceRect = { 0, 0, m_d3dpp.BackBufferWidth, m_d3dpp.BackBufferHeight };
                    FLOAT top = (FLOAT)renderResultInfo.resultRect.top / (FLOAT)textureDesc.Height;
                    FLOAT left = (FLOAT)renderResultInfo.resultRect.left / (FLOAT)textureDesc.Height;
                    FLOAT right = (FLOAT)renderResultInfo.resultRect.right / (FLOAT)textureDesc.Width;
                    FLOAT bottom = (FLOAT)renderResultInfo.resultRect.bottom / (FLOAT)textureDesc.Height;
                    
                    // Render the texture as a simple fullscreen quad
                    ATG::DebugDraw::DrawScreenSpaceTexturedRectPatch( ScreenSpaceRect,
                        XMFLOAT2( top, left ), XMFLOAT2( right, top ), XMFLOAT2( left, bottom ),
                        pTexture, FALSE );
                    // You must release the reference count on the texture returned from GetTexture
                    pTexture->Release();
                }
            }
        }

        ULONGLONG ullPosition = m_pXAVPlayerStatus->GetPosition();
        ULONGLONG ullDuration = m_pXAVPlayerStatus->GetDuration();

        // Display a playback progress bar
        // If the duration of the video is unknown, GetDuration will return c_ullDurationUnknown
        if( ullDuration != c_ullDurationUnknown )
        {
            m_Font.Begin();
            m_Font.SetScaleFactors( 1.0f, 1.0f );
            WCHAR strBuffer[1000];
            swprintf_s( strBuffer, L"Playback Rate: %ix", (INT)g_fPlaybackRates[ m_iPlaybackRate ] );
            // Position and duration are in 100ns units
            PrintTime( 0, 560, 0xFFFFFFFF, ullPosition );
            PrintTime( 100, 560, 0xFFFFFFFF, ullDuration );
            m_Font.DrawText( 200, 560, 0xFFFFFFFF, strBuffer );
            m_Font.End();

            // Progress bar
            D3DRECT rectBar = { 100, 600, 1180 , 620 };
            RenderProgressBar( rectBar, 0xFFFFFFFF, (FLOAT)ullPosition / (FLOAT)ullDuration );
        }

        // Display a buffering progress bar if the video is buffering
        if( m_pXAVPlayerStatus->IsBuffering() )
        {
            m_Font.Begin();
            m_Font.SetScaleFactors( 1.0f, 1.0f );
            m_Font.DrawText( 0, 460, 0xFFFF0000, L"Buffering..." );
            m_Font.End();

            // The buffering percentage is returned by GetFractionBufferingCompleted
            D3DRECT rectBar = { 100, 570, 1180 , 590 };
            RenderProgressBar( rectBar, 0xFFFF0000, m_pXAVPlayerStatus->GetFractionBufferingCompleted() );
        }
    }

    // Show title and help
    m_Timer.MarkFrame();
    if( m_bDrawHelp )
    {
        m_Help.Render( &m_Font, g_HelpCallouts, NUM_HELP_CALLOUTS );
    }
    else
    {
        m_Font.Begin();
        m_Font.SetScaleFactors( 1.2f, 1.2f );
        m_Font.DrawText( 0, 36, 0xffffffff, L"AdvancedXAV" );

        m_Font.SetScaleFactors( 1.2f, 1.2f );
        if ( g_bUsePlayReady )
        {
            m_Font.DrawText( 0, 100, 0xffffffff, L"Press " GLYPH_A_BUTTON L" to play/pause PLAYREADY content" );
        }
        else
        {
            m_Font.DrawText( 0, 100, 0xffffffff, L"Press " GLYPH_A_BUTTON L" to play/pause UNENCRYPTED content" );
        }
        m_Font.DrawText( 0, 140, 0xffffffff, L"Press " GLYPH_Y_BUTTON L" to toggle between UNENCRYPTED and PLAYREADY content" );
        m_Font.DrawText( 0, 180, 0xffffffff, L"Press " GLYPH_BACK_BUTTON L" for additional controls" );

        m_Font.End();
    }

    // Present the scene
    IDirect3DTexture9* pFrontBuffer = NULL;
    m_pd3dDevice->GetFrontBuffer( &pFrontBuffer );
    m_pd3dDevice->SynchronizeToPresentationInterval();
    m_pd3dDevice->Resolve( D3DRESOLVE_RENDERTARGET0, NULL, pFrontBuffer, NULL, 0, 0, NULL, 1.0, 0, NULL );

    // Pump the XAV movie decoding
    // XAVPostRender() must be called after the frame is completed and before swap
    // It may also change EDRAM contents, so it should be called after the title's
    // frame is resolved
    if( m_pXAVPlayer )
    {
        XAVPostRender();
    }

    m_pd3dDevice->Swap( pFrontBuffer, NULL );

    // Pump the XAV movie decoding
    // XAVPreRender() must be called before the next frame begins and after swap
    // It may also change EDRAM contents, so it should be called before the title
    // begins rendering anything
    if( m_pXAVPlayer )
        XAVPreRender();
    
    return S_OK;
}

//--------------------------------------------------------------------------------------
//
// SimpleXAV.cpp
//
// Sample showing the simplest way to play XMV movies
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
#include <AtgSimpleShaders.h>
#include <AtgUtil.h>
#include <xaudio2.h>
#include <xav.h>

const CHAR* g_strMovieName = "game:\\Media\\Video\\Sample.wmv";

//--------------------------------------------------------------------------------------
// Callouts for labelling the gamepad on the help screen
//--------------------------------------------------------------------------------------
ATG::HELP_CALLOUT g_HelpCallouts[] =
{
    { ATG::HELP_BACK_BUTTON, ATG::HELP_PLACEMENT_2, L"Display\nhelp" },
    { ATG::HELP_A_BUTTON, ATG::HELP_PLACEMENT_2, L"Play/Pause\nmovie" },
    { ATG::HELP_B_BUTTON, ATG::HELP_PLACEMENT_1, L"Stop movie" },
};
static const DWORD NUM_HELP_CALLOUTS = ARRAYSIZE( g_HelpCallouts );


//--------------------------------------------------------------------------------------
// Name: struct XAVCallbackHelper
// Desc: Simple COM helper class for the XAV events callbacks
//--------------------------------------------------------------------------------------
struct XAVCallbackHelper : public IXAVPlayerEvents {
    DWORD ReferenceCount;

    XAVCallbackHelper()
    {
        ReferenceCount = 1;
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
    virtual void Alert(XAVPlayerAlert* pAlert, const IXAVPlayerStatus* pStatus) {}
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

    XAVCallbackHelper* m_pXAVCallbacks;

private:
    virtual HRESULT Initialize();
    virtual HRESULT Update();
    virtual HRESULT Render();

    HRESULT InitializeXAV();
    HRESULT ShutdownXAV();

    ATG::Timer m_XAVPostPreTimer;
};


//--------------------------------------------------------------------------------------
// Name: main()
// Desc: Entry point to the program
//--------------------------------------------------------------------------------------
INT __cdecl main()
{
    Sample atgApp;
    ATG::GetVideoSettings( &atgApp.m_d3dpp.BackBufferWidth, &atgApp.m_d3dpp.BackBufferHeight );
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
        return ATGAPPERR_MEDIANOTFOUND;

    // Confine text drawing to the title safe area
    m_Font.SetWindow( ATG::GetTitleSafeArea() );

    // Create the help
    m_bDrawHelp = FALSE;
    if( FAILED( m_Help.Create( "game:\\Media\\Help\\Help.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    m_pXAVPlayer = NULL;
    m_pXAVPlayerStatus = NULL;
    m_pXAVGetTexture = NULL;
    m_pXAVCallbacks = NULL;

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: InitializeXAV()
// Desc: This creates the XAV player and loads the movie
//--------------------------------------------------------------------------------------
HRESULT Sample::InitializeXAV()
{
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

    HRESULT hr = XAVInitialize( &XAVInit, ThreadAffinities );
    if( FAILED(hr) )
        ATG::FatalError( "Error %#X calling XAVInititialize\n", hr );

    // Initialize the XAV Player Object
    m_pXAVCallbacks = new XAVCallbackHelper();
    hr = CreateXAVPlayer( &m_pXAVPlayer, m_pXAVCallbacks, NULL );
    if( FAILED(hr) )
        ATG::FatalError( "Error %#X calling CreateXAVPlayer\n", hr );

    // Obtain the PlayerStatus Interface
    hr = m_pXAVPlayer->GetStatus( &m_pXAVPlayerStatus );
    if( FAILED(hr) )
        ATG::FatalError( "Error %#X calling GetStatus\n", hr );

    // Obtain the GetTexture Interface
    hr = m_pXAVPlayer->GetPlayerFrameInfo( &m_pXAVGetTexture );
    if( FAILED(hr) )
        ATG::FatalError( "Error %#X calling GetPlayerFrameInfo\n", hr );

    // Open the Movie File
    hr = m_pXAVPlayer->Open( g_strMovieName );
    if( FAILED(hr) )
        ATG::FatalError( "Error %#X calling IXAVPlayer::Open for %s\n", hr, g_strMovieName );

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
        if( m_pXAVPlayerStatus->IsEndOfMedia() )
            ShutdownXAV();
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
                m_pXAVPlayer->Pause();
            else
                m_pXAVPlayer->Play();
        }
        else
        {
            // Not Currently Playing - Start Playback
            InitializeXAV();
            m_pXAVPlayer->Play();
        }
    }

    // Press B to Stop the Movie - and shutdown XAV
    // If you wanted to replay or play a new movie, you could keep XAV initialized
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_B )
    {
        if( m_pXAVPlayer )
        {
            m_pXAVPlayer->Stop();
            ShutdownXAV();
        }
    }
    
    // Toggle help
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_BACK )
        m_bDrawHelp = !m_bDrawHelp;

    return S_OK;
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

            // Obtain the texture for the current frame of the movie
            if( SUCCEEDED(m_pXAVGetTexture->GetTexture( FALSE, &pTexture, &renderResultInfo )) )
            {
                // Note: If you previously played a movie, then on a second play through, GetTexture() will
                // return the previously decoded frame until the first frame is ready.  Use IsTextureDirty()
                // to determine when the first frame of the new movie is available

                // Render the texture as a simple fullscreen quad
                D3DRECT ScreenSpaceRect = { 0, 0, m_d3dpp.BackBufferWidth, m_d3dpp.BackBufferHeight };
                ATG::DebugDraw::DrawScreenSpaceTexturedRect( ScreenSpaceRect, pTexture, FALSE );
                // You must release the reference count on the texture returned from GetTexture
                pTexture->Release();
            }
        }
    }

    // Show title, frame rate, and help
    m_Timer.MarkFrame();
    if( m_bDrawHelp )
    {
        m_Help.Render( &m_Font, g_HelpCallouts, NUM_HELP_CALLOUTS );
    }
    else
    {
        m_Font.Begin();
        m_Font.SetScaleFactors( 1.2f, 1.2f );
        m_Font.DrawText( 0, 36, 0xffffffff, L"SimpleXAV" );
        m_Font.SetScaleFactors( 1.0f, 1.0f );
        m_Font.DrawText( -1, 38, 0xffffff00, m_Timer.GetFrameRate(), ATGFONT_RIGHT );

        WCHAR strBuffer[1000];
        swprintf_s( strBuffer, L"Press " GLYPH_A_BUTTON L" to play\n%S", g_strMovieName );
        m_Font.SetScaleFactors( 1.2f, 1.2f );
        m_Font.DrawText( 0, 100, 0xffffffff, strBuffer );

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
        m_XAVPostPreTimer.GetElapsedTime();
        XAVPostRender();
    }

    m_pd3dDevice->Swap( pFrontBuffer, NULL );


    // Pump the XAV movie decoding
    // XAVPreRender() must be called before the next frame begins and after swap
    // It may also change EDRAM contents, so it should be called before the title
    // begins rendering anything
    if( m_pXAVPlayer )
    {
        // It is recommended to insert a short wait between the call to XAVPostRender and the call to XAVPreRender to avoid 
        // potential A/V sync issues. XAV uses the time between XAVPostRender and XAVPreRender to process new frames. If this 
        // time is too short, XAV will not have enough time to process the frame and instead will drop the current video frame. 
        // Therefore in cases that swap time is too short, XAV will be dropping video frames very frequently which will cause 
        // audio and video to drift apart and become out of sync. Note that the time XAV needs to process frames depends on 
        // the video resolution. 
        // The recommended minimum wait time between XAVPostRender and XAVPreRender calls is 8 msec for 1080p video content.
        #define WAIT_TIME_BETWEEN_POST_AND_PRE_RENDER ( 8.0f )

        DOUBLE fSleepTime = WAIT_TIME_BETWEEN_POST_AND_PRE_RENDER - m_XAVPostPreTimer.GetElapsedTime() / 1000.0f;
        if( fSleepTime > 0.0f )
        {
            Sleep( (DWORD) fSleepTime + 1 );
        }

        XAVPreRender();
    }
    
    return S_OK;
}

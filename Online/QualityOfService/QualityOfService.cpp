//--------------------------------------------------------------------------------------
// QualityOfService.cpp
//
// The sample illustrates how to use quality of service to measure network bandwidth
// and latency between the console and the Live service.
//
// Latency and bandwidth are evaluated simultaneously during a QoS probe. Bandwidth
// is measured by using a technique known as packet separation. The initiator issues
// two network packets back-to-back to the listener. The listener reports back the
// difference in the time that it received these two packets. Based on the reported
// timing gap, the initiator can estimate the bandwidth.
//
// The bandwidth algorithm makes the assumption that packets are being sent at a fixed
// speed. This is true for DSL modems for example, but cable modems typically buffer
// packets and send them in spurts. This can affect QoS bandwidth results, and for cable
// modems, the results can vary significantly, sometimes by as much as 128 Kb/s. For
// this reason, games should avoid displaying bandwidth. The returned bandwidth
// estimates should only be used for relative comparisons.
//
// QoS latency on the other hand, is much more accurate and dependable value.
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include <xtl.h>
#include <d3d9.h>
#include "xbox.h"
#include "xonline.h"
#include "AtgApp.h"
#include "AtgFont.h"
#include "AtgHelp.h"
#include "AtgInput.h"
#include "AtgResource.h"
#include "AtgSignIn.h"
#include "AtgUtil.h"


//--------------------------------------------------------------------------------------
// Color values
//--------------------------------------------------------------------------------------
#define INFO_COLOR          0xffffff00          // information display color

#define TOP_BACK_COLOR      0xff00007f          // background gradient colors
#define BOTTOM_BACK_COLOR   0xff000000



//--------------------------------------------------------------------------------------
// Callouts for labelling the gamepad on the help screen
//--------------------------------------------------------------------------------------
ATG::HELP_CALLOUT g_HelpCallouts[] =
{
    { ATG::HELP_BACK_BUTTON,  ATG::HELP_PLACEMENT_2, L"Display\nhelp" },
    { ATG::HELP_A_BUTTON, ATG::HELP_PLACEMENT_1, L"Display QoS" },
    { ATG::HELP_B_BUTTON, ATG::HELP_PLACEMENT_1, L"Signin" },
};
static const DWORD NUM_HELP_CALLOUTS = sizeof( g_HelpCallouts ) / sizeof( g_HelpCallouts[0] );


//--------------------------------------------------------------------------------------
// Name: Sample
// Desc: Main class to run this application. Most functionality is inherited
//       from the ATG::Application base class.
//--------------------------------------------------------------------------------------
class Sample : public ATG::Application
{
    ATG::Timer m_Timer;
    ATG::Font m_Font;
    ATG::Help m_Help;
    BOOL m_bDrawHelp;
    XNQOS* m_pxnServiceQos;        // Live service probe results
    XNetStartupParams m_netParams;            // XNetStartupParams option
    ULONGLONG m_ulNicXmitBytes;       // NIC total bytes sent option
    DWORD m_dwNicXmitFrames;      // NIC total frames sent option
    ULONGLONG m_ulNicRecvBytes;       // NIC total bytes received option
    DWORD m_dwNicRecvFrames;      // NIC total frames received option
    ULONGLONG m_ulCallerXmitBytes;    // Caller total bytes sent option
    DWORD m_dwCallerXmitFrames;   // Caller total frames sent option
    ULONGLONG m_ulCallerRecvBytes;    // Caller total bytes received option
    DWORD m_dwCallerRecvFrames;   // Caller total frames received option


    VOID            UpdateQoSInformation();

private:
    virtual HRESULT Initialize();
    virtual HRESULT Update();
    virtual HRESULT Render();
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
// Name: Initialize
// Desc: This creates all device-dependent display objects.
//--------------------------------------------------------------------------------------
HRESULT Sample::Initialize()
{
    m_bDrawHelp = FALSE;

    // Create the font
    if( FAILED( m_Font.Create( "game:\\Media\\Fonts\\Arial_16.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Confine text drawing to the title safe area
    m_Font.SetWindow( ATG::GetTitleSafeArea() );

    // Create the help
    if( FAILED( m_Help.Create( "game:\\Media\\Help\\Help.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Initialize Live
    if( FAILED( XOnlineStartup() ) )
        return E_FAIL;

    // Initialize login
    ATG::SignIn::Initialize( 1, 1, TRUE, 1 );

    m_pxnServiceQos = NULL;

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Update
// Desc: Called once per frame, the call is the entry point for animating
//       the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Update()
{
    // Get merged input to display help
    ATG::GAMEPAD* pGamepad = ATG::Input::GetMergedInput();

    // Toggle help
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_BACK )
        m_bDrawHelp = !m_bDrawHelp;

    // Update login
    ATG::SignIn::Update();

    if( !ATG::SignIn::IsSystemUIShowing() && ATG::SignIn::AreUsersSignedIn() )
    {
        // Display the Signin UI again
        if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_B )
        {
            ATG::SignIn::ShowSignInUI();
        }

        if( !m_pxnServiceQos || ( pGamepad->wPressedButtons & XINPUT_GAMEPAD_A ) )
        {
            UpdateQoSInformation();
        }
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Render
// Desc: Sets up render states, clears the viewport, and renders the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Render()
{
    // Draw a gradient filled background
    ATG::RenderBackground( TOP_BACK_COLOR, BOTTOM_BACK_COLOR );

    // Show title, frame rate, and help
    m_Timer.MarkFrame();
    if( m_bDrawHelp )
    {
        m_Help.Render( &m_Font, g_HelpCallouts, NUM_HELP_CALLOUTS );
    }
    else
    {
        // Draw title text
        m_Font.Begin();
        m_Font.SetScaleFactors( 1.2f, 1.2f );
        m_Font.DrawText( 0, 0, 0xffffffff, L"QualityOfService" );
        m_Font.SetScaleFactors( 1.0f, 1.0f );
        m_Font.DrawText( 0, 0, 0xffffff00, m_Timer.GetFrameRate(), ATGFONT_RIGHT );

        if( !ATG::SignIn::GetSignedInUserCount() )
        {
            m_Font.SetScaleFactors( 1.0f, 1.0f );
            m_Font.DrawText( 0, 64, 0xffffffff, L"There is no user signed in. Press " GLYPH_B_BUTTON L" to sign in." );
        }
        else
        {
            m_Font.SetScaleFactors( 0.8f, 0.8f );

            // Display QoS results
            if( m_pxnServiceQos )
            {
                WCHAR strInfo[256];

                swprintf_s( strInfo, L"Downstream bits/s: %d\n", ( INT )m_pxnServiceQos->axnqosinfo->dwDnBitsPerSec );
                m_Font.DrawText( 0, 50, INFO_COLOR, strInfo );

                swprintf_s( strInfo, L"Upstream bits/s: %d\n", ( INT )m_pxnServiceQos->axnqosinfo->dwUpBitsPerSec );
                m_Font.DrawText( INFO_COLOR, strInfo );

                swprintf_s( strInfo, L"Round trip average ms: %d\n",
                            ( INT )m_pxnServiceQos->axnqosinfo->wRttMedInMsecs );
                m_Font.DrawText( INFO_COLOR, strInfo );

                swprintf_s( strInfo, L"Round trip minimum ms: %d\n\n",
                            ( INT )m_pxnServiceQos->axnqosinfo->wRttMinInMsecs );
                m_Font.DrawText( INFO_COLOR, strInfo );

                //
                // Display counters
                //

                // Display XNetStartupParams partially
                swprintf_s( strInfo, L"Socket send buffer: %d KB\n", ( INT )m_netParams.cfgSockDefaultSendBufsizeInK );
                m_Font.DrawText( INFO_COLOR, strInfo );

                swprintf_s( strInfo, L"Socket receive buffer: %d KB\n",
                            ( INT )m_netParams.cfgSockDefaultRecvBufsizeInK );
                m_Font.DrawText( INFO_COLOR, strInfo );

                swprintf_s( strInfo, L"Qos probe timeout: %d s\n\n", ( INT )m_netParams.cfgQosProbeTimeoutInSeconds );
                m_Font.DrawText( INFO_COLOR, strInfo );

                // Display NIC counters
                swprintf_s( strInfo, L"NIC bytes sent: %I64u\n", m_ulNicXmitBytes );
                m_Font.DrawText( INFO_COLOR, strInfo );

                swprintf_s( strInfo, L"NIC frames sent: %u\n", m_dwNicXmitFrames );
                m_Font.DrawText( INFO_COLOR, strInfo );

                swprintf_s( strInfo, L"NIC bytes received: %I64u\n", m_ulNicRecvBytes );
                m_Font.DrawText( INFO_COLOR, strInfo );

                swprintf_s( strInfo, L"NIC frames received: %u\n\n", m_dwNicRecvFrames );
                m_Font.DrawText( INFO_COLOR, strInfo );

                // Display caller counters
                swprintf_s( strInfo, L"Caller bytes sent: %I64u\n", m_ulCallerXmitBytes );
                m_Font.DrawText( INFO_COLOR, strInfo );

                swprintf_s( strInfo, L"Caller frames sent: %u\n", m_dwCallerXmitFrames );
                m_Font.DrawText( INFO_COLOR, strInfo );

                swprintf_s( strInfo, L"Caller bytes received: %I64u\n", m_ulCallerRecvBytes );
                m_Font.DrawText( INFO_COLOR, strInfo );

                swprintf_s( strInfo, L"Caller frames received: %u\n\n", m_dwCallerRecvFrames );
                m_Font.DrawText( INFO_COLOR, strInfo );
            }
        }

        m_Font.End();
    }

    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: UpdateQoSInformation
// Desc: Retreives QoS network information.
//--------------------------------------------------------------------------------------
VOID Sample::UpdateQoSInformation()
{
    INT iResult;

    if( m_pxnServiceQos != NULL )
    {
        XNetQosRelease( m_pxnServiceQos );
        m_pxnServiceQos = NULL;
    }

    //
    // Sends 24 probes to make a bandwidth estimate
    //
    iResult = XNetQosServiceLookup( 0, NULL, &m_pxnServiceQos );
    assert( iResult == NO_ERROR );

    //
    // Retreive counters
    //
    DWORD dwSize;
    dwSize = sizeof( XNetStartupParams );
    iResult = XNetGetOpt( XNET_OPTID_STARTUP_PARAMS, ( BYTE* )&m_netParams, &dwSize );
    assert( iResult == NO_ERROR );

    //
    // NIC counters that keep track of bytes and frames since system boot
    //
    dwSize = sizeof( ULONGLONG );
    iResult = XNetGetOpt( XNET_OPTID_NIC_XMIT_BYTES, ( BYTE* )&m_ulNicXmitBytes, &dwSize );
    assert( iResult == NO_ERROR );

    dwSize = sizeof( DWORD );
    iResult = XNetGetOpt( XNET_OPTID_NIC_XMIT_FRAMES, ( BYTE* )&m_dwNicXmitFrames, &dwSize );
    assert( iResult == NO_ERROR );

    dwSize = sizeof( ULONGLONG );
    iResult = XNetGetOpt( XNET_OPTID_NIC_RECV_BYTES, ( BYTE* )&m_ulNicRecvBytes, &dwSize );
    assert( iResult == NO_ERROR );

    dwSize = sizeof( DWORD );
    iResult = XNetGetOpt( XNET_OPTID_NIC_RECV_FRAMES, ( BYTE* )&m_dwNicRecvFrames, &dwSize );
    assert( iResult == NO_ERROR );

    //
    // Caller counters that keep track of bytes and frames since XNetStartup
    //
    dwSize = sizeof( ULONGLONG );
    iResult = XNetGetOpt( XNET_OPTID_CALLER_XMIT_BYTES, ( BYTE* )&m_ulCallerXmitBytes, &dwSize );
    assert( iResult == NO_ERROR );

    dwSize = sizeof( DWORD );
    iResult = XNetGetOpt( XNET_OPTID_CALLER_XMIT_FRAMES, ( BYTE* )&m_dwCallerXmitFrames, &dwSize );
    assert( iResult == NO_ERROR );

    dwSize = sizeof( ULONGLONG );
    iResult = XNetGetOpt( XNET_OPTID_CALLER_RECV_BYTES, ( BYTE* )&m_ulCallerRecvBytes, &dwSize );
    assert( iResult == NO_ERROR );

    dwSize = sizeof( DWORD );
    iResult = XNetGetOpt( XNET_OPTID_CALLER_RECV_FRAMES, ( BYTE* )&m_dwCallerRecvFrames, &dwSize );
    assert( iResult == NO_ERROR );

}





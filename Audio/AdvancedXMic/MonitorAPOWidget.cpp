//--------------------------------------------------------------------------------------
// MonitorAPOWidget.cpp
//
//
//
//
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include <xtl.h>
#include <d3d9.h>

#include "AtgApp.h"
#include "AtgFont.h"
#include "AtgInput.h"
#include "AtgResource.h"
#include "AtgUtil.h"
#include <AtgConsole.h>
#include <XInput2.h>
#include <XMic.h>
#include "XAudio2.h"
#include <ATGSpectralDisplay.h>
#include <AtgDebugDraw.h>
#include "MonitorAPOWidget.h"

const D3DRECT g_rcMonitorDisplay = { 250, 8, 568, 73 };
const D3DRECT g_rcMonitorTIME = { 222, 22, 240, 40 };
const D3DRECT g_rcMonitorFREQ = { 222, 57, 240, 75 };


//--------------------------------------------------------------------------------------
// Name: CMonitorAPOWidget::CMonitorAPOWidget
// Desc: Constructor
//--------------------------------------------------------------------------------------
CMonitorAPOWidget::CMonitorAPOWidget( ATG::MonitorAPOPipe* pInput )
    : m_pInput( pInput )
    , m_monitorTimeDomain( true )
{

}

//--------------------------------------------------------------------------------------
// Name: CMonitorAPOWidget::Init
// Desc: Initialize the widget
//--------------------------------------------------------------------------------------
HRESULT CMonitorAPOWidget::Init( IDirect3DDevice9* pDevice, ATG::PackedResource* pResource )
{
    HRESULT hr = m_monitorDisplay.Initialize( pDevice, nSamples );
    if( FAILED( hr ) )
        ATG::FatalError("monitorDisplay failed to initialize.");

    m_pBackground = pResource->GetTexture( "UI_MonitorAPO" );
    m_pLED = pResource->GetTexture( "UI_YellowLED" );
    if( !m_pBackground || !m_pLED )
        ATG::FatalError( "Couldn't initialize UI for MonitorAPO" );

    return S_OK;
}

//--------------------------------------------------------------------------------------
// Name: CMonitorAPOWidget::Update
// Desc: Update the frequency graph with new data
//--------------------------------------------------------------------------------------
void CMonitorAPOWidget::Update( )
{
    __vector4 samples[nSamples/4];
    if( m_pInput->Read( samples, sizeof( samples ) ) )
    {
        m_monitorDisplay.Update( (float*)samples, m_monitorTimeDomain );
    }
}

  //--------------------------------------------------------------------------------------
// Name: CMonitorAPOWidget::Update
// Desc: Zeroes out data values.
//--------------------------------------------------------------------------------------
void CMonitorAPOWidget::Reset( )
{
    __vector4 samples[nSamples/4];
    ZeroMemory( samples, sizeof(samples) );
    m_monitorDisplay.Update( (float*)samples, m_monitorTimeDomain );
}


//--------------------------------------------------------------------------------------
// Name: CMonitorAPOWidget::Render
// Desc: Draws the widget
//--------------------------------------------------------------------------------------
void CMonitorAPOWidget::Render( IDirect3DDevice9* pDevice, const D3DRECT& bounds,
                               BOOL bActivated )
{
    ATG::DebugDraw::DrawScreenSpaceTexturedRect( bounds, m_pBackground );

    D3DRECT rcMonitor = g_rcMonitorDisplay;
    OffsetRect( (LPRECT)&rcMonitor, bounds.x1, bounds.y1 );

    if( bActivated )
    {
        m_monitorDisplay.Render( pDevice, rcMonitor );

        D3DRECT rcLED = m_monitorTimeDomain ? g_rcMonitorTIME : g_rcMonitorFREQ;
        OffsetRect((LPRECT)&rcLED, bounds.x1, bounds.y1 );
        ATG::DebugDraw::DrawScreenSpaceTexturedRect( rcLED, m_pLED );
    }
}

//--------------------------------------------------------------------------------------
// Name: CMonitorAPOWidget::HandleInput
// Desc: Change graph type in response to button press
//--------------------------------------------------------------------------------------
void CMonitorAPOWidget::HandleInput( ATG::GAMEPAD* pGamepad )
{
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_A )
    {
        m_monitorTimeDomain = !m_monitorTimeDomain;
        if( m_monitorTimeDomain )
        {
            m_monitorDisplay.SetRange( -1.0f, 1.0f );
        }
        else
        {
            m_monitorDisplay.SetRange( 0.0f, 1.0f );
        }
    }
}

//--------------------------------------------------------------------------------------
// UI.h
//
// XNA Developer Connection
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include "UI.h"


#include <AtgDebugDraw.h>
#include <XDSP.h>
#include <AtgFont.h>
#include <AtgInput.h>
#include <AtgResource.h>
#include <AtgUtil.h>

#include <utility>


    //--------------------------------------------------------------------------------------
    // Name: MapThumbstick
    // Desc: Maps a normalized thumbstick position into an arbitrary range
    //--------------------------------------------------------------------------------------
    void MapThumbstick( float gamepadValue, float* pParameter, float rangeMin, float rangeMax )
    {
        if( gamepadValue != 0.0f )
        {
            float range = rangeMax - rangeMin;
            *pParameter += gamepadValue / ( range * 100.f );
            if( rangeMin > rangeMax ) std::swap( rangeMin, rangeMax );
            *pParameter = min( rangeMax, max( rangeMin, *pParameter ) );
        }
    }

    const D3DRECT g_rcMonitorDisplay = { 250, 8, 568, 73 };
    const D3DRECT g_rcMonitorTIME = { 222, 22, 240, 40 };
    const D3DRECT g_rcMonitorFREQ = { 222, 57, 240, 75 };

    //
    // MonitorAPO UI Widget
    //

    //--------------------------------------------------------------------------------------
    // Name: CMonitorApoWidget::CMonitorApoWidget
    // Desc: Constructor
    //--------------------------------------------------------------------------------------
    CMonitorApoWidget::CMonitorApoWidget( ATG::MonitorAPOPipe* pInput )
        : m_pInput( pInput )
        , m_monitorTimeDomain( true )
    {

    }

    //--------------------------------------------------------------------------------------
    // Name: CMonitorApoWidget::Init
    // Desc: Destructor
    //--------------------------------------------------------------------------------------
    void CMonitorApoWidget::Init( IDirect3DDevice9* pDevice, ATG::PackedResource* pResource )
    {

        m_monitorDisplay.Initialize( pDevice, nSamples );
        m_pBackground = pResource->GetTexture( "UI_MonitorAPO" );
        m_pLED = pResource->GetTexture( "UI_YellowLED" );
        if( !m_pBackground || !m_pLED )
            ATG::FatalError( "Couldn't initialize UI for MonitorAPO" );
    }

    //--------------------------------------------------------------------------------------
    // Name: CMonitorApoWidget::Update
    // Desc: Update the frequency graph with new data
    //--------------------------------------------------------------------------------------
    void CMonitorApoWidget::Update( FLOAT )
    {
        __vector4 samples[nSamples/4];
        if( m_pInput->Read( samples, sizeof( samples ) ) )
        {
            m_monitorDisplay.Update( (float*)samples, m_monitorTimeDomain );
        }
    }

    //--------------------------------------------------------------------------------------
    // Name: CMonitorApoWidget::Render
    // Desc: Draws the widget
    //--------------------------------------------------------------------------------------
    void CMonitorApoWidget::Render( IDirect3DDevice9* pDevice, const D3DRECT& bounds )
    {
        ATG::DebugDraw::DrawScreenSpaceTexturedRect( bounds, m_pBackground );

        D3DRECT rcMonitor = g_rcMonitorDisplay;
        OffsetRect( (LPRECT)&rcMonitor, bounds.x1, bounds.y1 );
        m_monitorDisplay.Render( pDevice, rcMonitor );

        D3DRECT rcLED = m_monitorTimeDomain ? g_rcMonitorTIME : g_rcMonitorFREQ;
        OffsetRect((LPRECT)&rcLED, bounds.x1, bounds.y1 );
        ATG::DebugDraw::DrawScreenSpaceTexturedRect( rcLED, m_pLED );

    }

    //--------------------------------------------------------------------------------------
    // Name: CMonitorApoWidget::HandleInput
    // Desc: Change graph type in response to button press
    //--------------------------------------------------------------------------------------
    void CMonitorApoWidget::HandleInput( ATG::GAMEPAD* pGamepad )
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



    //
    // SimpleAPO UI Widget
    //

    const D3DRECT g_rcAmpGainTrack = { 311, 31, 534, 51 };
    const D3DRECT g_rcAmpSlider = { 0, 0, 39,23 };



    //--------------------------------------------------------------------------------------
    // Name: CSimpleApoWidget::CSimpleApoWidget
    // Desc: Constructor
    //--------------------------------------------------------------------------------------
    CSimpleApoWidget::CSimpleApoWidget( IXAudio2Voice* pVoice, int idxEffect )
        : m_pVoice( pVoice )
        , m_idxEffect( idxEffect )
    {
        if( m_pVoice )
        {
            m_pVoice->GetEffectParameters( idxEffect, &m_fxParams, sizeof( m_fxParams ) );
        }
    }

    //--------------------------------------------------------------------------------------
    // Name: CSimpleApoWidget::~CSimpleApoWidget
    // Desc: Destructor
    //--------------------------------------------------------------------------------------
    CSimpleApoWidget::~CSimpleApoWidget()
    {
    }

    //--------------------------------------------------------------------------------------
    // Name: CSimpleApoWidget::Init
    // Desc: Load the textures needed to draw this UI
    //--------------------------------------------------------------------------------------
    void CSimpleApoWidget::Init( IDirect3DDevice9* pDevice, ATG::PackedResource* pResource )
    {
        m_pBackground = pResource->GetTexture( "UI_AmpAPO" );
        m_pSlider = pResource->GetTexture( "UI_SliderH" );
    }

    //--------------------------------------------------------------------------------------
    // Name: CSimpleApoWidget::Update
    // Desc: Does nothing
    //--------------------------------------------------------------------------------------
    void CSimpleApoWidget::Update( FLOAT flElapsed )
    {
    }

    //--------------------------------------------------------------------------------------
    // Name: CSimpleApoWidget::Render
    // Desc: Draw this widget
    //--------------------------------------------------------------------------------------
    void CSimpleApoWidget::Render( IDirect3DDevice9* pDevice, const D3DRECT& bounds )
    {
        ATG::DebugDraw::DrawScreenSpaceTexturedRect( bounds, m_pBackground );
        int sliderX = (int)((float)( g_rcAmpGainTrack.x2 - g_rcAmpGainTrack.x1 ) * ( ( m_fxParams.gain + 2.0f ) / 4.0f ));
        sliderX -= ( g_rcAmpSlider.x2 - g_rcAmpSlider.x1 ) / 2;
        sliderX += g_rcAmpGainTrack.x1;

        D3DRECT rcSlider = g_rcAmpSlider;
        OffsetRect( (LPRECT)&rcSlider, bounds.x1 + sliderX, bounds.y1 + g_rcAmpGainTrack.y1 );
        ATG::DebugDraw::DrawScreenSpaceTexturedRect( rcSlider, m_pSlider );
    }

    //--------------------------------------------------------------------------------------
    // Name: CSimpleApoWidget::HandleInput
    // Desc: Update the APO parameters in response to thumbstick movement
    //--------------------------------------------------------------------------------------
    void CSimpleApoWidget::HandleInput( ATG::GAMEPAD* pGamepad )
    {
        MapThumbstick( pGamepad->fX1, &m_fxParams.gain, -2.0f, 2.0f );
        m_pVoice->SetEffectParameters( m_idxEffect, &m_fxParams, sizeof( m_fxParams ) );
    }







    //
    // Comp1APO UI Widget
    //
    const D3DRECT g_rcComp1Display = { 375, 10, 543, 73 };
    const D3DRECT g_rcComp1FnDisplay = { 450, 10, 543, 73 };
    const int nComp1Samples = 64;


    //--------------------------------------------------------------------------------------
    // Name: CComp1ApoWidget::CComp1ApoWidget
    // Desc: Constructor
    //--------------------------------------------------------------------------------------
    CComp1ApoWidget::CComp1ApoWidget( IXAudio2Voice* pVoice, int idxEffect )
        : m_pVoice( pVoice )
        , m_idxEffect( idxEffect )
    {
        if( m_pVoice )
        {
            m_pVoice->GetEffectParameters( idxEffect, &m_fxParams, sizeof( m_fxParams ) );
        }
    }

    //--------------------------------------------------------------------------------------
    // Name: CComp1ApoWidget::~CComp1ApoWidget
    // Desc: Destructor
    //--------------------------------------------------------------------------------------
    CComp1ApoWidget::~CComp1ApoWidget()
    {
    }

    //--------------------------------------------------------------------------------------
    // Name: CComp1ApoWidget::Init
    // Desc: Load resources needed to draw this widget
    //--------------------------------------------------------------------------------------
    void CComp1ApoWidget::Init( IDirect3DDevice9* pDevice, ATG::PackedResource* pResource )
    {
        m_pBackground = pResource->GetTexture( "UI_Comp1APO" );
        m_monitorDisplay.Initialize( pDevice, nComp1Samples );
        m_monitorDisplay.SetRange( 0.0f, 1.0f );

        if( FAILED( m_font.Create( "game:\\Media\\Fonts\\Fixedsys_12.xpr" ) ) )
            ATG::FatalError( "Couldn't create fixed point font" );
    }

    //--------------------------------------------------------------------------------------
    // Name: CComp1ApoWidget::Update
    // Desc: Get the current transfer function from the compressor APO
    //--------------------------------------------------------------------------------------
    void CComp1ApoWidget::Update( FLOAT flElapsed )
    {
        __vector4 samples[nComp1Samples / 4];
        Comp1APOParams params;
        m_pVoice->GetEffectParameters( m_idxEffect, &params, sizeof( params ) );
        CComp1APO::CalcTransferFunction( params, samples, nComp1Samples / 4 );

        m_monitorDisplay.Update( (float*)samples );
    }

    //--------------------------------------------------------------------------------------
    // Name: CComp1ApoWidget::Render
    // Desc: Draw this widget
    //--------------------------------------------------------------------------------------
    void CComp1ApoWidget::Render( IDirect3DDevice9* pDevice, const D3DRECT& bounds )
    {
        ATG::DebugDraw::DrawScreenSpaceTexturedRect( bounds, m_pBackground );

        D3DRECT rcMonitor = g_rcComp1FnDisplay;
        OffsetRect( (LPRECT)&rcMonitor, bounds.x1, bounds.y1 );
        m_monitorDisplay.Render( pDevice, rcMonitor );

        D3DRECT rcThreshold = rcMonitor;
        LONG w = rcThreshold.x2 - rcThreshold.x1;
        rcThreshold.x1 += (LONG)( m_fxParams.threshold * (float)w );
        rcThreshold.x2 = rcThreshold.x1;
        ATG::DebugDraw::DrawScreenSpaceRect( rcThreshold, 1.0f, 0x4400ff00 );

        LONG k = (LONG)( m_fxParams.knee * (float)w );

        rcThreshold.x2 = rcThreshold.x1 + k/2;
        rcThreshold.x1 -= k/2;
        ATG::DebugDraw::DrawScreenSpaceRect( rcThreshold, 1.0f, 0x2200ff00 );

        D3DRECT rcText = g_rcComp1Display;
        OffsetRect( (LPRECT)&rcText, bounds.x1, bounds.y1 );

        m_font.Begin();
        WCHAR str[256];
        swprintf_s( str, L"%1.2f \n%2.1f:1 \n%1.2f \n%1.2f",
            m_fxParams.threshold,
            1.0f / m_fxParams.ratio,
            m_fxParams.knee,
            m_fxParams.makeup );
        m_font.DrawText( (float)rcText.x1, (float)rcText.y1, 0x8800aa00, str, ATGFONT_LEFT );
        m_font.End();
    }

    //--------------------------------------------------------------------------------------
    // Name: CComp1ApoWidget::HandleInput
    // Desc: Update compressor params in response to thumbstick input
    //--------------------------------------------------------------------------------------
    void CComp1ApoWidget::HandleInput( ATG::GAMEPAD* pGamepad )
    {
        MapThumbstick( pGamepad->fX1, &m_fxParams.threshold, 0.0f, 1.0f );
        MapThumbstick( pGamepad->fY1, &m_fxParams.ratio, 1.0f, .01f );
        MapThumbstick( pGamepad->fX2, &m_fxParams.knee, 0.0f, .5f );
        MapThumbstick( pGamepad->fY2, &m_fxParams.makeup, 0.0f, 1.5f );
        m_pVoice->SetEffectParameters( m_idxEffect, &m_fxParams, sizeof( m_fxParams ) );
    }



	//
    // EQAPO UI Widget
    //
    const D3DRECT g_rcEQDisplay = { 375, 10, 543, 73 };
    const D3DRECT g_rcEQFnDisplay = { 450, 10, 543, 73 };


    //--------------------------------------------------------------------------------------
    // Name: CEQApoWidget::CEQApoWidget
    // Desc: Constructor
    //--------------------------------------------------------------------------------------
    CEQApoWidget::CEQApoWidget( IXAudio2Voice* pVoice, int idxEffect )
        : m_pVoice( pVoice )
        , m_idxEffect( idxEffect )
    {
		m_iCurrentBand =0;
        if( m_pVoice )
        {
            m_pVoice->GetEffectParameters( idxEffect, &m_fxParams, sizeof( m_fxParams ));
        }
    }

    //--------------------------------------------------------------------------------------
    // Name: CEQApoWidget::~CEQApoWidget
    // Desc: Destructor
    //--------------------------------------------------------------------------------------
    CEQApoWidget::~CEQApoWidget()
    {
    }

    //--------------------------------------------------------------------------------------
    // Name: CEQApoWidget::Init
    // Desc: Load resources needed to draw this widget
    //--------------------------------------------------------------------------------------
    void CEQApoWidget::Init( IDirect3DDevice9* pDevice, ATG::PackedResource* pResource )
    {
		_ASSERT(pDevice != NULL);
		_ASSERT(pResource != NULL);
        // Initialize unity roots lookup table used by FFT functions
        XDSP::FFTInitializeUnityTable(m_UnityTable, nBins);
        m_pBackground = pResource->GetTexture( "UI_EQAPO" );
        m_monitorDisplay.Initialize( pDevice, nEQSamples /2);
        m_monitorDisplay.SetRange( 0.0f, 1.0f );
        if( FAILED( m_font.Create( "game:\\Media\\Fonts\\Fixedsys_12.xpr" ) ) )
            ATG::FatalError( "Couldn't create fixed point font" );
    }

    //--------------------------------------------------------------------------------------
    // Name: CEQApoWidget::Update
    // Desc: Get the current transfer function from the compressor APO
    //--------------------------------------------------------------------------------------
    void CEQApoWidget::Update( FLOAT flElapsed )
    {
        float scratch[nBins] = {0};
		memset( imaginarySamples, 0, sizeof( imaginarySamples ) );
		memset( samples, 0, sizeof( samples ) );

		//Calculate the response of the equalizer given the current coeffcients and a pulse
		CEqualizerAPO::CalcSignalPulseFunction( m_fxParams, samples, sampleVectorCount);

		//Run the result through an FFT to analyze the frequency response.
        XDSP::FFT(samples, imaginarySamples, m_UnityTable, nBins);
        //Convert to polar form
        XDSP::FFTPolar(samples, samples, imaginarySamples, nBins);

        // The FFT produces samples out of order; get them back into order
        // of increasing frequency
		XDSP::FFTUnswizzle((XDSP::XVECTOR*)scratch, samples, nBinsLog2);

		//Display the resulting graph.
		m_monitorDisplay.Update( scratch );
    }

    //--------------------------------------------------------------------------------------
    // Name: CEQApoWidget::Render
    // Desc: Draw this widget
    //--------------------------------------------------------------------------------------
    void CEQApoWidget::Render( IDirect3DDevice9* pDevice, const D3DRECT& bounds )
    {
		_ASSERT(pDevice != NULL);
        ATG::DebugDraw::DrawScreenSpaceTexturedRect( bounds, m_pBackground );

		D3DRECT rcMonitor = g_rcEQFnDisplay;
		OffsetRect( (LPRECT)&rcMonitor, bounds.x1, bounds.y1 );
		m_monitorDisplay.Render( pDevice, rcMonitor );

		for(int i=0;i<m_fxParams.Count;i++)
		{
			float Width = 1.0f;

			//Calculate the horizontal position
			D3DRECT rcThreshold = rcMonitor;
			LONG w = rcThreshold.x2 - rcThreshold.x1;
			rcThreshold.x1 += (LONG)( m_fxParams.Params[i].Frequency * (float)w );
			rcThreshold.x2 = rcThreshold.x1;

			if (m_fxParams.Params[i].Type == BandPass)
			{
				//Calculate the width based on Q
				Width = (5.0f + ((1.0f - m_fxParams.Params[i].Q) * (w-5.0f) *.5f));

				//Calculate the vertical position based on Gain
				//Low/High pass does not use gain.
				LONG h = rcThreshold.y2 - rcThreshold.y1;
				rcThreshold.y2 -= (LONG)(( m_fxParams.Params[i].Gain * (float)h )+5.0f);
				rcThreshold.y1 = rcThreshold.y2+5;
			}

			//Draw the current band with a slightly brighter color
			if (i==m_iCurrentBand)
				ATG::DebugDraw::DrawScreenSpaceRect( rcThreshold, Width, 0x5F22ff00 );
			else
				ATG::DebugDraw::DrawScreenSpaceRect( rcThreshold, Width, 0x4400ff00 );
		}

        D3DRECT rcText = g_rcEQDisplay;
        OffsetRect( (LPRECT)&rcText, bounds.x1, bounds.y1 );

        m_font.Begin();
        WCHAR str[256];

		_ASSERT(m_fxParams.Params[m_iCurrentBand].Type <= HighPass);
		static WCHAR * szTypes[] = { L"Low", L"Band", L"High"};
		WCHAR * szType = szTypes[m_fxParams.Params[m_iCurrentBand].Type];
        swprintf_s( str, L"%1.2f \n%2.1f:1 \n%1.2f\n%s",
            m_fxParams.Params[m_iCurrentBand].Frequency,
            m_fxParams.Params[m_iCurrentBand].Gain,
			m_fxParams.Params[m_iCurrentBand].Q,
			szType);
        m_font.DrawText( (float)rcText.x1, (float)rcText.y1, 0x8800aa00, str, ATGFONT_LEFT );
        m_font.End();
    }

    //--------------------------------------------------------------------------------------
    // Name: CEQApoWidget::HandleInput
    // Desc: Update compressor params in response to thumbstick input
    //--------------------------------------------------------------------------------------
    void CEQApoWidget::HandleInput( ATG::GAMEPAD* pGamepad )
    {
		_ASSERT(pGamepad != NULL);
		_ASSERT(m_iCurrentBand < EQ_MAXBANDCOUNT);
		if (pGamepad->wPressedButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER && m_iCurrentBand < (m_fxParams.Count-1))
			m_iCurrentBand ++;
		else if(pGamepad->wPressedButtons & XINPUT_GAMEPAD_LEFT_SHOULDER && m_iCurrentBand > 0)
			m_iCurrentBand --;
        MapThumbstick( pGamepad->fX1, &m_fxParams.Params[m_iCurrentBand].Frequency, 0.001f, .999f );
        MapThumbstick( pGamepad->fY1,      &m_fxParams.Params[m_iCurrentBand].Gain, 0.000f,     .999f );
        MapThumbstick( pGamepad->fX2,         &m_fxParams.Params[m_iCurrentBand].Q, 0.001f,     1.0f );
        m_pVoice->SetEffectParameters( m_idxEffect, &m_fxParams, sizeof( m_fxParams) );
    }


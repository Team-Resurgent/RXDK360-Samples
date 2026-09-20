//--------------------------------------------------------------------------------------
// XMicUtil.cpp
//
// Utility functions for connectivity with Xbox360 wireless mic and other mics.
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
#include <AtgSimpleShaders.h>
#include <AtgDebugDraw.h>
#include <XAudio2.h>
#include "MonitorAPOWidget.h"
#include "XMicInclude.h"

// Callbacks for XAudio2 sounds
static VoiceCallback g_voiceCallback;
extern IXAudio2* g_pXAudio2;

//--------------------------------------------------------------------------------------
// Name: XMicSampleRenderAudio
// Desc: Copy the audio samples to a buffer then submit to an XAudio2 source voice
//--------------------------------------------------------------------------------------
void XMicSampleRenderAudio( IXAudio2SourceVoice* m_pVoice, DWORD dwAudioFrameBufferLen, BYTE* pCapturedAudioData, WORD wCaptureAudioDataLen[], DWORD dwMaxFrameCount )
{
    DWORD dwTotalAudioSamples = 0;
    BYTE AudioSamples[2048] = { 0 };
    XAUDIO2_BUFFER*  pAudioPacket;

    // Collect the audio frames into a single audio packet
    for( DWORD dwFrameIndex = 0; dwFrameIndex < dwMaxFrameCount; dwFrameIndex++ )
    {
        if( 0 != wCaptureAudioDataLen[ dwFrameIndex ] )
        {
            if( wCaptureAudioDataLen[ dwFrameIndex ] >= 2048 )
                ATG::FatalError("Captured data exceeds 2048 byte buffer size.");

            memcpy( &AudioSamples[ dwTotalAudioSamples ], pCapturedAudioData, wCaptureAudioDataLen[ dwFrameIndex ] );
            dwTotalAudioSamples += wCaptureAudioDataLen[ dwFrameIndex ];
        }

        pCapturedAudioData += dwAudioFrameBufferLen;
    }

    if( 0 < dwTotalAudioSamples )
    {
        // Create the audio packet
        pAudioPacket = new XAUDIO2_BUFFER;
        ZeroMemory( pAudioPacket, sizeof( XAUDIO2_BUFFER ) );

        pAudioPacket->pAudioData = new BYTE[ dwTotalAudioSamples ];
        memcpy( (void*)pAudioPacket->pAudioData, AudioSamples, dwTotalAudioSamples );

        pAudioPacket->AudioBytes    = dwTotalAudioSamples;
        pAudioPacket->pContext      = pAudioPacket;
        pAudioPacket->LoopCount     = 0;
        pAudioPacket->LoopLength    = 0;
        pAudioPacket->LoopBegin     = 0;
        pAudioPacket->Flags         = 0;

        // Submit the audio packet
        m_pVoice->SubmitSourceBuffer( pAudioPacket );
    }

}


//--------------------------------------------------------------------------------------
// Name: VoiceCallback::OnBufferEnd
// Desc: Destroy the audio buffer and packet
//--------------------------------------------------------------------------------------
void VoiceCallback::OnBufferEnd(void * pBufferContext)
{
    if( NULL != pBufferContext )
    {
        XAUDIO2_BUFFER *pPacket = (XAUDIO2_BUFFER*)pBufferContext;

        if( NULL != pPacket )
        {
            delete[] pPacket->pAudioData;
            delete pPacket;
        }
    }
}



//--------------------------------------------------------------------------------------
// Name: XMicSampleDataReady
// Desc: Deal with our data and then make another request to the microphone
//--------------------------------------------------------------------------------------
void XMicSampleDataReady( DWORD dwErrorCode, DWORD dwNumberOfBytesTransfereed,
                         XOVERLAPPED *pOverlapped )
{
    MicrophoneDataRequestCompletionData* pMCD =
        (MicrophoneDataRequestCompletionData*) pOverlapped->dwCompletionContext;

    if( NULL != pMCD )
    {
        // Render the audio data
        XMicSampleRenderAudio( pMCD->m_pVoice, pMCD->dwAudioFrameBufferLength,
            pMCD->pAudioFramesBuffer, pMCD->wFrameLengths, pMCD->dwFrameCount );

        // Make sure we still have a running microphone before making another request
        if( XMICSTATUS_STARTED == XMicGetStatus( pMCD->m_dwMicrophoneIndex ) )
        {
            pMCD->pOverlap->dwExtendedError = 0;
            pMCD->pOverlap->InternalContext = 0;
            pMCD->pOverlap->InternalHigh = 0;
            pMCD->pOverlap->InternalLow = 0;
            XMicRequestData( pMCD->m_dwMicrophoneIndex, pMCD->dwFrameCount,
                pMCD->pAudioFramesBuffer, pMCD->wFrameLengths, pMCD->pOverlap );
        }
    }
}



//--------------------------------------------------------------------------------------
// Name: XMicSampleProcessCapabilities
// Desc: Perform sample microphone get capabilities operations
//--------------------------------------------------------------------------------------
HRESULT XMicSampleProcessCapabilities( DWORD m_dwMicrophoneIndex, XMICCAPABILITIES* pCapabilities )
{
    DWORD dwError;

    // Query the capabilities of the microphone device
    dwError = XMicGetCapabilities( m_dwMicrophoneIndex, pCapabilities );

    if( ERROR_SUCCESS != dwError )
    {
        ATG::DebugSpew( "XMicSampleProcessCapabilities() XMicGetCapabilities() failed with (%d) for microphone (%x)\n",
            dwError, m_dwMicrophoneIndex );
        return E_FAIL;
    }

    ATG::DebugSpew( "\n[XMicGetCapabilities------------------]\n\n" );

    // Print out the audio characteristics of the microphone
    ATG::DebugSpew( "m_dwMicrophoneIndex      (%x)\n", m_dwMicrophoneIndex );
    ATG::DebugSpew( "wFormatTag             (%d)\n", pCapabilities->wFormatTag );
    ATG::DebugSpew( "wBitsPerSample         (%d)\n", pCapabilities->wBitsPerSample );
    ATG::DebugSpew( "nChannels              (%d)\n", pCapabilities->nChannels );
    ATG::DebugSpew( "dwFrameLength          (%d)\n", pCapabilities->dwFrameLength );

    // Print out the supported sampling rates
    if( XMICSAMPLERATE_32000 & pCapabilities->dwSampleRatesSupported )
    {
        ATG::DebugSpew( "dwSampleRatesSupported XMICSAMPLERATE_32000\n" );
    }
    if( XMICSAMPLERATE_48000 & pCapabilities->dwSampleRatesSupported )
    {
        ATG::DebugSpew( "dwSampleRatesSupported XMICSAMPLERATE_48000\n" );
    }

    // Print out the features of the microphone
    if( XMICFEATURE_WIRELESS & pCapabilities->dwFeatures )
    {
        ATG::DebugSpew( "dwFeatures             XMICFEATURE_WIRELESS\n" );
    }
    if( XMICFEATURE_ACCELEROMETER & pCapabilities->dwFeatures )
    {
        ATG::DebugSpew( "dwFeatures             XMICFEATURE_ACCELEROMETER\n" );
    }
    if( XMICFEATURE_OUTPUTLED & pCapabilities->dwFeatures )
    {
        ATG::DebugSpew( "dwFeatures             XMICFEATURE_OUTPUTLED\n" );
    }

    // Print out the color of the microphone
    switch( pCapabilities->bColor )
    {
    case XMICCOLOR_BLACK:
        ATG::DebugSpew( "bColor                 XMICCOLOR_BLACK\n" );
        break;

    case XMICCOLOR_WHITE:
        ATG::DebugSpew( "bColor                 XMICCOLOR_WHITE\n" );
        break;

    case XMICCOLOR_UNKNOWN:
        ATG::DebugSpew( "bColor                 XMICCOLOR_UNKNOWN\n" );
        break;
    }

    ATG::DebugSpew( "\n[-------------------------------------]\n" );
    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: XMicSampleCreateVoice
// Desc: Create a source voice for audio output
//--------------------------------------------------------------------------------------
HRESULT XMicSampleCreateVoice( DWORD m_dwMicrophoneIndex, IXAudio2SourceVoice** pm_pVoice,
                              DWORD dwMicrophoneSampleRate, ATG::MonitorAPOPipe *pmonitor)
{
    HRESULT hr;
    WAVEFORMATEX format;

    // Initialize the source voice
    ZeroMemory(&format, sizeof(format));

    switch ( dwMicrophoneSampleRate )
    {
    case XMICSAMPLERATE_32000:
        format.nSamplesPerSec = 32000;
        break;

    case XMICSAMPLERATE_48000:
        format.nSamplesPerSec = 48000;
        break;
    }

    format.nChannels = 1;
    format.wFormatTag = WAVE_FORMAT_PCM;
    format.nAvgBytesPerSec = format.nSamplesPerSec * 2;
    format.wBitsPerSample = 16;
    format.nBlockAlign = (format.nChannels + format.wBitsPerSample) / 8;
    format.cbSize = 0;

    hr = g_pXAudio2->CreateSourceVoice( pm_pVoice, &format, 0, XAUDIO2_DEFAULT_FREQ_RATIO , &g_voiceCallback );

    if( SUCCEEDED( hr ) )
    {
        (*pm_pVoice)->SetVolume( 1.0f );
        (*pm_pVoice)->Start( 0 );
    }
    else
    {
        ATG::DebugSpew( "XMicSampleCreateVoice() XAudioCreateSourceVoice() failed with (%x) for microphone (%x)\n", hr, m_dwMicrophoneIndex );
    }

    // Create the effect chain
    ATG::CMonitorAPO* pMonitorAPOPre = NULL;
    ATG::CMonitorAPO::CreateInstance( NULL, 0, &pMonitorAPOPre );

	XAUDIO2_EFFECT_DESCRIPTOR apoDesc[1] = {0};
    apoDesc[0].InitialState = true;
    apoDesc[0].OutputChannels = 1;
    apoDesc[0].pEffect = static_cast<IXAPO*>(pMonitorAPOPre);

    XAUDIO2_EFFECT_CHAIN chain = {0};
    chain.EffectCount = sizeof(apoDesc) / sizeof(apoDesc[0]);
    chain.pEffectDescriptors = apoDesc;

    hr = (*pm_pVoice)->SetEffectChain( &chain );

    // Don't need to keep them now that XAudio2 has ownership
    pMonitorAPOPre->Release();

    // Set initial effect params
    ATG::MonitorAPOParams fxParamsMon;
    fxParamsMon.pipe = pmonitor;
    if( FAILED( hr = (*pm_pVoice)->SetEffectParameters( 0, &fxParamsMon, sizeof( fxParamsMon ) ) ) )
        ATG::FatalError( "Couldn't set effect parameters for Monitor APO (%#X)", hr );

    return hr;
}


//--------------------------------------------------------------------------------------
// Name: XMicSampleProcessGain
// Desc: Perform some sample microphone gain operations
//--------------------------------------------------------------------------------------
HRESULT XMicSampleProcessGain( DWORD m_dwMicrophoneIndex )
{
    FLOAT fMinGain;
    FLOAT fMaxGain;
    FLOAT fCurrentGain;
    DWORD dwError;

    ATG::DebugSpew( "XMicSampleProcessGain() Reading/Setting gain for microphone (%x)\n", m_dwMicrophoneIndex );

    // Query the minimum gain value
    dwError = XMicGetGain( m_dwMicrophoneIndex, XMICGAIN_MIN, &fMinGain );

    if( ERROR_SUCCESS != dwError )
    {
        ATG::DebugSpew( "XMicSampleProcessGain() XMicGetGain(XMICGAIN_MIN) failed with (%d) for microphone (%x)\n",
            dwError, m_dwMicrophoneIndex );
        return E_FAIL;
    }

    // Query the maximum gain value
    dwError = XMicGetGain( m_dwMicrophoneIndex, XMICGAIN_MAX, &fMaxGain );

    if( ERROR_SUCCESS != dwError )
    {
        ATG::DebugSpew( "XMicSampleProcessGain() XMicGetGain(XMICGAIN_MAX) failed with (%d) for microphone (%d)\n",
            dwError, m_dwMicrophoneIndex );
        return E_FAIL;
    }

    // Query the current gain value
    dwError = XMicGetGain( m_dwMicrophoneIndex, XMICGAIN_CURRENT, &fCurrentGain );

    if( ERROR_SUCCESS != dwError )
    {
        ATG::DebugSpew( "XMicSampleProcessGain() XMicGetGain(XMICGAIN_CURRENT) failed with (%d) for microphone (%d)\n",
            dwError, m_dwMicrophoneIndex );
        return E_FAIL;
    }

    // Set the gain to the midpoint between the minimum gain and maximum gain.
    dwError = XMicSetGain( m_dwMicrophoneIndex, ( ( fMinGain + fMaxGain ) * 0.8f ), NULL );

    if( ERROR_SUCCESS != dwError )
    {

        ATG::DebugSpew( "XMicSampleProcessGain() XMicSetGain() failed with (%d) for microphone (%d)\n",
            dwError, m_dwMicrophoneIndex );
        return E_FAIL;
    }

    // Print out the current gain values.
    ATG::DebugSpew( "Minimum gain           (%1.5f)\n", fMinGain );
    ATG::DebugSpew( "Maximum gain           (%1.5f)\n", fMaxGain );
    ATG::DebugSpew( "Current gain           (%1.5f)\n", fCurrentGain );

    return S_OK;
}
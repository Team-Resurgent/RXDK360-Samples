//--------------------------------------------------------------------------------------
// ReducingLatency.cpp
//
// This sample demonstrates the difference in latency between the implementing a game
// using a more traditional game engine architecture vs. an architecture that could
// reduce latency for Kinect titles. It also demonstrates how to efficiently
// retrieve depth and color data, which is normally available about 5ms before
// skeleton data is ready. Therefore, if your title is preocessing the one of the
// image streams, you don't have to delay retrieving the data. Note that this sample
// doesn't do anythign with the depth and color data, but it will be displayed in
// PIX captures where and when the data is retrieved.
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include <xtl.h>
#include <xaudio2.h>
#include <AtgAudio.h>

#include "CLowLatencySample.h"
#include "CHighLatencySample.h"


//--------------------------------------------------------------------------------------
// Name: Sample
// Desc: Main class to run this application.
//--------------------------------------------------------------------------------------
class Sample
{
public:
    VOID Run();

private:
    HRESULT Initialize();
    HRESULT InitializeAudio();

    CLatencySample* m_pLowLatencySample;    // Pointer to instance of a low latency sample class
    CLatencySample* m_pHighLatencySample;   // Pointer to instance of a high latency sample class
    CLatencySample* m_pSample;              // Pointer to either m_pLowLatencySample or m_pHighLatencySample

    // Audio related data
    XAUDIO2_BUFFER m_AudioBuffer;
    IXAudio2SourceVoice* m_pSourceVoice; 

};


//--------------------------------------------------------------------------------------
// Global instance of the app
//--------------------------------------------------------------------------------------
Sample g_NuiSample;


//--------------------------------------------------------------------------------------
// Name: main()
// Desc: Entry point to the program
//--------------------------------------------------------------------------------------
VOID __cdecl main()
{
    g_NuiSample.Run();
}


//--------------------------------------------------------------------------------------
// Name: Initialize
// Desc: Initialize app-dependent objects
//--------------------------------------------------------------------------------------
VOID Sample::Run()
{
    if ( FAILED( Initialize() ) )
    {
        ATG::FatalError( "Couldn't initialize sample.\n" );
    }

    for ( ;; )
    {
        // Initialize the selected sample
        if ( FAILED( m_pSample->Initialize() ) )
        {
            ATG::FatalError( "Couldn't initialize sample.\n" );
        }

        // Run the selected sample
        m_pSample->Run();

        // When done, toggle switch between the two sample typs
        switch ( m_pSample->GetSampleType() )
        {
        case SAMPLE_TYPE_LOW_LATENCY:
            m_pSample = m_pHighLatencySample;
            break;

        case SAMPLE_TYPE_HIGH_LATENCY:
            m_pSample = m_pLowLatencySample;
            break;

        }
    }
}


//--------------------------------------------------------------------------------------
// Name: Initialize
// Desc: Initialize app-dependent objects
//--------------------------------------------------------------------------------------
HRESULT Sample::Initialize()
{
    // Initialize audio
    RETURN_ON_FAIL( InitializeAudio() );
    RETURN_ON_NULL( m_pHighLatencySample = new CHighLatencySample );
    RETURN_ON_NULL( m_pLowLatencySample = new CLowLatencySample );

    m_pSample = m_pLowLatencySample;
   
    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: InitializeAudio
// Desc: Initialize audio and start playing background music.
//--------------------------------------------------------------------------------------
HRESULT Sample::InitializeAudio()
{
    // Initialize XAudio2
    IXAudio2* pXAudio2 = NULL;
    UINT32 flags = 0;

#ifdef _DEBUG
    flags |= XAUDIO2_DEBUG_ENGINE;
#endif

    RETURN_ON_FAIL( XAudio2Create( &pXAudio2, flags, AUDIO_HW_THREAD ) );

    // Create a mastering voice
    IXAudio2MasteringVoice* pMasteringVoice = NULL;
    RETURN_ON_FAIL( pXAudio2->CreateMasteringVoice( &pMasteringVoice ) );

    // Load WAV file
    ATG::WaveFile XmaFile;
    RETURN_ON_FAIL( XmaFile.Open( "game:\\Media\\Sounds\\XENON_Light.xma" ) );

    // Read the format header
    WAVEFORMATEXTENSIBLE wfx = {0};
    XMA2WAVEFORMATEX xma2 = {0};
    RETURN_ON_FAIL( XmaFile.GetFormat( &wfx, &xma2 ) );

    if ( wfx.Format.wFormatTag != WAVE_FORMAT_XMA2 )
    {
        ATG::FatalError( "Error - Expected an XMA2 XAudio2 compatible file\n" );
    }

    // Calculate how many bytes and samples are in the wave
    DWORD cbWaveSize = 0;
    XmaFile.GetDuration( &cbWaveSize );

    // Read the sample data into memory (XMA packets must be 2K aligned)
    BYTE* pbWaveData = ( BYTE* )XPhysicalAlloc( cbWaveSize, MAXULONG_PTR, 2048, PAGE_READWRITE );
    RETURN_ON_FAIL( XmaFile.ReadSample( 0, pbWaveData, cbWaveSize, &cbWaveSize ) );

    // Create a source voice to play the wave
    RETURN_ON_FAIL( pXAudio2->CreateSourceVoice( &m_pSourceVoice, ( WAVEFORMATEX* )&xma2 ) );

    // submit the wave sample data using an XAUDIO2_BUFFER structure
    m_AudioBuffer.pAudioData = pbWaveData;
    m_AudioBuffer.Flags = XAUDIO2_END_OF_STREAM;  // tell the source voice not to expect any data after this buffer
    m_AudioBuffer.AudioBytes = cbWaveSize;
    m_AudioBuffer.LoopCount = XAUDIO2_LOOP_INFINITE;

    RETURN_ON_FAIL( m_pSourceVoice->SubmitSourceBuffer( &m_AudioBuffer ) );

    // Play the source voice
    RETURN_ON_FAIL( m_pSourceVoice->Start( 0 ) );

    return S_OK;
}
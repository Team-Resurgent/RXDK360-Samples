//--------------------------------------------------------------------------------------
// HeadsetAudio.cpp
//
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include <xtl.h>
#include <xaudio2.h>
#include <AtgConsole.h>
#include <AtgInput.h>
#include <AtgUtil.h>
#include <AtgAudio.h>
#include <xhv2.h>
#include "DataOutAPO.h"
#include "DataInAPO.h"
#include "APOBridge.h"

// Disable warning C6211: Leaking memory <pointer> due to an exception.
// We "know" we won't run out of memory in this sample
#pragma warning ( disable : 6211 )


//--------------------------------------------------------------------------------------
// Globals
//--------------------------------------------------------------------------------------
// The sample rate is set to 16125, to match the sampling rate of the XHV2 headset
#define SAMPLE_RATE 16125
// We only use mono audio, since we are sending to a single speaker on a headset
#define NUM_CHANNELS 1
// we will use this XUID for our fake remote talker, which will accept the XAudio2 data
#define REMOTE_XUID_1 1
// this sample assumes that the headset is plugged into the controller at index 0
#define GAMER_INDEX 0
// Loop the audio 4 times, after the first playthrough (so 5 total plays)
#define AUDIO_LOOP_COUNT 4

// Console for output
ATG::Console g_Console;

//--------------------------------------------------------------------------------------
// Name: main()
// Desc: Entry point to the program
//--------------------------------------------------------------------------------------
VOID __cdecl main()
{
    HRESULT hr;

    //
    // Initialize the console window
    //
    g_Console.Create( "game:\\Media\\Fonts\\Arial_16.xpr", 0xFF1F005F, 0xFFFFFFFF );
    g_Console.SendOutputToDebugChannel( TRUE );

    //
    // Initialize XAudio2
    //

    APOBridge * apoBridge = new APOBridge(SAMPLE_RATE, NUM_CHANNELS);

    //
    // Variables to hold pointers to our XAPO effects
    //
    IUnknown * pDataOut;
    IUnknown * pDataIn;

    // we are creating thte DataOutAPO such that the SubmixVoice is muted once we get the data out.
    pDataOut = new DataOutAPO(static_cast<IAPODataReceiver*>(apoBridge), true);

    pDataIn = new DataInAPO(static_cast<IAPODataSender*>(apoBridge));

    //
    // Create the descriptors that describe our custom XAPO effects
    //
    XAUDIO2_EFFECT_DESCRIPTOR dataOutDesc = {0};
    dataOutDesc.pEffect = pDataOut;
    dataOutDesc.InitialState = TRUE;
    dataOutDesc.OutputChannels = 1;

    XAUDIO2_EFFECT_DESCRIPTOR dataInDesc = {0};
    dataInDesc.pEffect = pDataIn;
    dataInDesc.InitialState = TRUE;
    dataInDesc.OutputChannels = 1;

    //
    // Create effect chains that point at our custom XAPO effects.  We need two chains because
    // we will be applying each XAPO to a different voice
    //
    XAUDIO2_EFFECT_CHAIN dataOutChain = {0};
    dataOutChain.EffectCount = 1;
    dataOutChain.pEffectDescriptors = &dataOutDesc;

    XAUDIO2_EFFECT_CHAIN dataInChain = {0};
    dataInChain.EffectCount = 1;
    dataInChain.pEffectDescriptors = &dataInDesc;

    //
    // When building DEBUG, we enable the debug XAudio2 engine, which provides
    // more debugging facilities
    //
    UINT32 flags = 0;
#ifdef _DEBUG
    flags |= XAUDIO2_DEBUG_ENGINE;
#endif

    //
    // Create the XAudio2 engine object.  This object will be shared by the XHV2 engine
    //
    IXAudio2* pXAudio2 = NULL;
    if( FAILED( hr = XAudio2Create( &pXAudio2, flags ) ) )
        ATG::FatalError( "Error %#X calling XAudio2Create\n", hr );

    //
    // Create a mastering voice
    //
    IXAudio2MasteringVoice* pMasteringVoice = NULL;
    if( FAILED( hr = pXAudio2->CreateMasteringVoice( &pMasteringVoice ) ) )
        ATG::FatalError( "Error %#X calling CreateMasteringVoice\n", hr );

    //
    // Create a submix voice that will convert our source assets to 16125 Hz
    //
    IXAudio2SubmixVoice * pSubmixVoice = NULL;
    if (FAILED( hr = pXAudio2->CreateSubmixVoice(&pSubmixVoice, 1, SAMPLE_RATE, 0, 0, NULL, &dataOutChain) ) )
        ATG::FatalError(" Error %#X calling CreateSubmixVoice\n", hr);

    //
    // Now that we have passed our XAPO to XAudio2, we can release it from our application.
    // This way, XAudio2 takes ownership of the object, and will release it at the correct time
    //
    SAFE_RELEASE(pDataOut);

    //
    // Initialize XHV2
    //
    PIXHV2ENGINE pXHV2;
    HANDLE hWorker;
    XHV_PROCESSING_MODE mode = XHV_VOICECHAT_MODE;

    XHV_INIT_PARAMS xhvParams = {0};
    xhvParams.dwMaxRemoteTalkers = XHV_MAX_REMOTE_TALKERS;
    xhvParams.dwMaxLocalTalkers = XHV_MAX_LOCAL_TALKERS;
    xhvParams.localTalkerEnabledModes = &mode;
    xhvParams.remoteTalkerEnabledModes = &mode;
    xhvParams.dwNumLocalTalkerEnabledModes = 1;
    xhvParams.dwNumRemoteTalkerEnabledModes = 1;
    xhvParams.pXAudio2 = pXAudio2;

    if( FAILED( hr = XHV2CreateEngine(&xhvParams, &hWorker, &pXHV2) ) )
        ATG::FatalError("Error %#X while creating XHV2 engine", hr);

    //
    // we are creating fake XUIDs for this sample.  Titles should get the XUID of the user on the box
    //
    XUID remoteXuid = REMOTE_XUID_1;

    //
    // Play XMA wave files
    //
    g_Console.Format( "Playing mono XMA via the headset...\n" );

    //
    // Read the wave file
    //
    ATG::WaveFile WaveFile;
    if( FAILED( hr = WaveFile.Open( "game:\\Media\\Sounds\\MaleDialog1.xma2" ) ) )
        ATG::FatalError( "Error %#X opening WAV file\n", hr );

    // Read the format header
    WAVEFORMATEXTENSIBLE wfx = {0};
    XMA2WAVEFORMATEX xma2 = {0};
    if( FAILED( hr = WaveFile.GetFormat( &wfx, &xma2 ) ) )
        ATG::FatalError( "Error %#X reading WAV format\n", hr );

    if( wfx.Format.wFormatTag != WAVE_FORMAT_XMA2 )
        ATG::FatalError( "Error - Expected an XMA2 XAudio2 compatible file\n" );

    // Calculate how many bytes and samples are in the wave
    DWORD cbWaveSize = 0;
    WaveFile.GetDuration( &cbWaveSize );

    // Read the sample data into memory (XMA packets must be 2K aligned)
    BYTE* pbWaveData = ( BYTE* )XPhysicalAlloc( cbWaveSize, MAXULONG_PTR, 2048, PAGE_READWRITE );
    if( FAILED( hr = WaveFile.ReadSample( 0, pbWaveData, cbWaveSize, &cbWaveSize ) ) )
        ATG::FatalError( "Error %#X reading WAV data\n", hr );

    //
    // Create the sendlist for the XAudio2 source voice, which will direct the output from the
    // voice to our previously created submix voice
    //
    XAUDIO2_SEND_DESCRIPTOR submixSend = {0};
    submixSend.pOutputVoice = pSubmixVoice;

    XAUDIO2_VOICE_SENDS submixSendList = {0};
    submixSendList.pSends = &submixSend;
    submixSendList.SendCount = 1;

    //
    // Create the IXAudio2SourceVoice that will be used to play the audio content
    //
    IXAudio2SourceVoice* pSourceVoice;
    if( FAILED( hr = pXAudio2->CreateSourceVoice( &pSourceVoice, ( WAVEFORMATEX* )&xma2, 0, XAUDIO2_DEFAULT_FREQ_RATIO, NULL, &submixSendList, NULL ) ) )
        ATG::FatalError( "Error %#X creating source voice\n", hr );

    //
    // Create a single local talker (titles may have up to 4 local talkers)
    //
    if( FAILED( hr = pXHV2->RegisterLocalTalker(GAMER_INDEX) ) )
        ATG::FatalError( "Error %#X registering local talker\n", hr );

    // it can take up to 20 ms after registering a remote talker before IsHeadsetConnected returns valid results
    Sleep(20);

    if (!pXHV2->IsHeadsetPresent(GAMER_INDEX))
    {
        g_Console.Format( "No headset detected, sending audio data to the speakers\n" );

        // send the output directly to the mastering voice, instead of the submix voice that
        // is working with XHV2
        if( FAILED( hr = pSourceVoice->SetOutputVoices(NULL) ) )
            ATG::FatalError( "Error %#X setting output voices\n", hr );
    }
	else
	{
		//
		// Create a fake remote talker.  This remote talker will be the place our game audio is submitted to XHV2
		//
		if( FAILED( hr = pXHV2->RegisterRemoteTalker(remoteXuid, &dataInChain, NULL, NULL) ) )
            ATG::FatalError( "Error %#X registering remote talker\n", hr );
	}

    //
    // Now that we have passed our XAPO to XAudio2, we can release it from our application.
    // This way, XAudio2 takes ownership of the object, and will release it at the correct time
    //
    SAFE_RELEASE(pDataIn);

    //
    // Play the wave using the source voice
    //

    // submit the wave sample data using an XAUDIO2_BUFFER structure
    XAUDIO2_BUFFER buffer = {0};
    buffer.pAudioData = pbWaveData;
    buffer.Flags = XAUDIO2_END_OF_STREAM;  // tell the source voice not to expect any more data after this buffer
    buffer.AudioBytes = cbWaveSize;
    buffer.LoopCount = AUDIO_LOOP_COUNT;

    if( FAILED( hr = pSourceVoice->SubmitSourceBuffer( &buffer ) ) )
        ATG::FatalError( "Error %#X submitting source buffer\n", hr );

    if( FAILED( hr = pSourceVoice->Start() ) )
        ATG::FatalError( "Error %#X starting the source voice\n", hr );

    g_Console.Format( "Hit LT-RT-RB to return to Launcher.\n" );

    BOOL isRunning = TRUE;
    while( isRunning )
    {
        XAUDIO2_VOICE_STATE state;
        pSourceVoice->GetState( &state, XAUDIO2_VOICE_NOSAMPLESPLAYED );
        isRunning = ( state.BuffersQueued > 0 );

        // Detect reboot keypress while the audio is playing
        ATG::Input::GetMergedInput();
    }

    //
    // Cleanup XHV2
    //
    SAFE_RELEASE( pXHV2 );

    //
    // Cleanup XAudio2
    //
    pSourceVoice->DestroyVoice();
    XPhysicalFree( pbWaveData );

    pSubmixVoice->DestroyVoice();

    pMasteringVoice->DestroyVoice();

    // When we release the XAudio2 engine, it will take care of releasing our XAPO objects for us
    SAFE_RELEASE( pXAudio2 );

    // Now that the APOs are freed, we can delete the apo Bridge object
    delete apoBridge;

    for(; ;)
    {
        // Detect exit sample request LT-RT-RB
        ATG::Input::GetMergedInput();
    }
}




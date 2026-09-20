//--------------------------------------------------------------------------------------
// XAudio2BasicSound.cpp
//
// Microsoft XNA Developer Connection.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include <xtl.h>
#include <xaudio2.h>
#include <AtgConsole.h>
#include <AtgInput.h>
#include <AtgUtil.h>
#include <AtgAudio.h>


//--------------------------------------------------------------------------------------
// Globals
//--------------------------------------------------------------------------------------

// Console for output
ATG::Console g_Console;


//--------------------------------------------------------------------------------------
// Forward references
//--------------------------------------------------------------------------------------
VOID PlayPCM( IXAudio2* pXaudio2, const char* szFilename );
VOID PlayXMA2( IXAudio2* pXaudio2, const char* szFilename );


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
    IXAudio2* pXAudio2 = NULL;

    UINT32 flags = 0;
#ifdef _DEBUG
    flags |= XAUDIO2_DEBUG_ENGINE;
#endif

    if( FAILED( hr = XAudio2Create( &pXAudio2, flags ) ) )
        ATG::FatalError( "Error %#X calling XAudio2Create\n", hr );

    //
    // Create a mastering voice
    //
    IXAudio2MasteringVoice* pMasteringVoice = NULL;
    if( FAILED( hr = pXAudio2->CreateMasteringVoice( &pMasteringVoice ) ) )
        ATG::FatalError( "Error %#X calling CreateMasteringVoice\n", hr );

    //
    // Play a PCM wave file
    //
    g_Console.Format( "Playing mono WAV...\n" );
    PlayPCM( pXAudio2, "game:\\Media\\Sounds\\MusicMono.wav" );

    g_Console.Format( "Playing 5.1 channel WAV...\n" );
    PlayPCM( pXAudio2, "game:\\Media\\Sounds\\MusicSurround.wav" );

    //
    // Play XMA wave files
    //
    g_Console.Format( "Playing mono XMA...\n" );
    PlayXMA2( pXAudio2, "game:\\Media\\Sounds\\MusicMono.xma2" );

    g_Console.Format( "Playing 5.1 channel XMA...\n" );
    PlayXMA2( pXAudio2, "game:\\Media\\Sounds\\MusicSurround.xma2" );

    //
    // Cleanup XAudio2
    //
    g_Console.Format( "Hit LT-RT-RB to return to Launcher.\n" );

    // All XAudio2 interfaces are released when the engine is destroyed, but being tidy
    pMasteringVoice->DestroyVoice();

    SAFE_RELEASE( pXAudio2 );

    for(; ; )
    {
        // Detect exit sample request LT-RT-RB
        ATG::Input::GetMergedInput();
    }
}


//--------------------------------------------------------------------------------------
// Name: PlayPCM
// Desc: Plays a wave and blocks until the wave finishes playing
//--------------------------------------------------------------------------------------
VOID PlayPCM( IXAudio2* pXaudio2, const char* szFilename )
{
    HRESULT hr = S_OK;

    //
    // Read the wave file
    //
    ATG::WaveFile WaveFile;
    if( FAILED( hr = WaveFile.Open( szFilename ) ) )
        ATG::FatalError( "Error %#X opening WAV file\n", hr );

    // Read the format header
    WAVEFORMATEXTENSIBLE wfx = {0};
    if( FAILED( hr = WaveFile.GetFormat( &wfx ) ) )
        ATG::FatalError( "Error %#X reading WAV format\n", hr );

    // Calculate how many bytes and samples are in the wave
    DWORD cbWaveSize = 0;
    WaveFile.GetDuration( &cbWaveSize );

    // Read the sample data into memory
    BYTE* pbWaveData = new BYTE[ cbWaveSize ];
    if( FAILED( hr = WaveFile.ReadSample( 0, pbWaveData, cbWaveSize, &cbWaveSize ) ) )
        ATG::FatalError( "Error %#X reading WAV data\n", hr );

    //
    // Play the wave using a new XAudio2SourceVoice
    //

    // Create the source voice
    IXAudio2SourceVoice* pSourceVoice;
    if( FAILED( hr = pXaudio2->CreateSourceVoice( &pSourceVoice, ( WAVEFORMATEX* )&wfx ) ) )
        ATG::FatalError( "Error %#X creating source voice\n", hr );

    // Submit the wave sample data using an XAUDIO2_BUFFER structure
    XAUDIO2_BUFFER buffer = {0};
    buffer.pAudioData = pbWaveData;
    buffer.Flags = XAUDIO2_END_OF_STREAM;  // tell the source voice not to expect any data after this buffer
    buffer.AudioBytes = cbWaveSize;

    if( FAILED( hr = pSourceVoice->SubmitSourceBuffer( &buffer ) ) )
        ATG::FatalError( "Error %#X submitting source buffer\n", hr );

    hr = pSourceVoice->Start( 0 );

    BOOL isRunning = TRUE;
    while( SUCCEEDED( hr ) && isRunning )
    {
        XAUDIO2_VOICE_STATE state;
        pSourceVoice->GetState( &state, XAUDIO2_VOICE_NOSAMPLESPLAYED );
        isRunning = ( state.BuffersQueued > 0 );

        // Detect reboot keypress
        ATG::Input::GetMergedInput();
    }

    pSourceVoice->DestroyVoice();
    SAFE_DELETE_ARRAY( pbWaveData );
}


//--------------------------------------------------------------------------------------
// Name: PlayXMA2
// Desc: Plays an .XMA2 wave and blocks until the wave finishes playing
//--------------------------------------------------------------------------------------
VOID PlayXMA2( IXAudio2* pXaudio2, const char* szFilename )
{
    HRESULT hr = S_OK;

    //
    // Read the wave file
    //
    ATG::WaveFile WaveFile;
    if( FAILED( hr = WaveFile.Open( szFilename ) ) )
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
    // Play the wave using a new XAudio2SourceVoice
    //

    // Create the source voice
    IXAudio2SourceVoice* pSourceVoice;
    if( FAILED( hr = pXaudio2->CreateSourceVoice( &pSourceVoice, ( WAVEFORMATEX* )&xma2 ) ) )
        ATG::FatalError( "Error %#X creating source voice\n", hr );

    // submit the wave sample data using an XAUDIO2_BUFFER structure
    XAUDIO2_BUFFER buffer = {0};
    buffer.pAudioData = pbWaveData;
    buffer.Flags = XAUDIO2_END_OF_STREAM;  // tell the source voice not to expect any data after this buffer
    buffer.AudioBytes = cbWaveSize;

    if( FAILED( hr = pSourceVoice->SubmitSourceBuffer( &buffer ) ) )
        ATG::FatalError( "Error %#X submitting source buffer\n", hr );

    hr = pSourceVoice->Start( 0 );

    BOOL isRunning = TRUE;
    while( SUCCEEDED( hr ) && isRunning )
    {
        XAUDIO2_VOICE_STATE state;
        pSourceVoice->GetState( &state, XAUDIO2_VOICE_NOSAMPLESPLAYED );
        isRunning = ( state.BuffersQueued > 0 );

        // Detect reboot keypress
        ATG::Input::GetMergedInput();
    }

    pSourceVoice->DestroyVoice();
    XPhysicalFree( pbWaveData );
}



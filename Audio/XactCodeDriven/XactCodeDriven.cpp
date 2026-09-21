//--------------------------------------------------------------------------------------
// XactCodeDriven.cpp
//
// This sample demonstrates the XACT code-driven API by showing
// how to create and play sounds from either a wave bank or a WAV file.
//
// XNA Developer Connection
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include <xtl.h>
#include <xact3.h>
#include <AtgAudio.h>
#include <AtgInput.h>
#include <AtgUtil.h>

const char* szWaveFilePath = "game:\\media\\sounds\\Dolphin4.wav";
const char* szWaveBankPath = "game:\\media\\sounds\\XactCodeDriven.xwb";


//--------------------------------------------------------------------------------------
// Name: PlayFromWaveBank()
// Desc: Play a sound from a wave bank
//--------------------------------------------------------------------------------------
VOID PlayFromWaveBank( IXACT3Engine* pXACTEngine )
{
    // Open the in memory wave bank
    DWORD dwFileSize = 0;
    VOID* pbWaveBank = NULL;

    // Make sure XMA contents must reside in physically contiguous memory
    HRESULT hr = ATG::LoadFilePhysicalMemory( szWaveBankPath, &pbWaveBank, &dwFileSize );
    if( FAILED( hr ) )
        ATG::FatalError( "Loading the wavebank failed with error %#X\n", hr );

    // Register the wave bank with XACT
    IXACT3WaveBank* pWaveBank;
    hr = pXACTEngine->CreateInMemoryWaveBank( pbWaveBank, dwFileSize, 0, 0, &pWaveBank );
    if( FAILED( hr ) )
        ATG::FatalError( "CreateInMemoryWaveBank failed with error %#X\n", hr );

    // Get the index of our wave
    XACTINDEX iWaveIndex = pWaveBank->GetWaveIndex( "Dolphin4" );
    if( iWaveIndex == XACTINDEX_INVALID )
        ATG::FatalError( "GetWaveIndex failed to find a valid index." );

    // Play the wave
    IXACT3Wave* pWave = NULL;
    hr = pWaveBank->Play( iWaveIndex, 0, 0, 0, &pWave );
    if( FAILED( hr ) )
        ATG::FatalError( "Play failed with error %#X\n", hr );

    // Set the volume
    pWave->SetVolume( 1.0f );

    // Loop while allowing XACT to do work until the wave completes.
    DWORD dwState = 0;
    while( dwState != XACT_STATE_STOPPED )
    {
        pXACTEngine->DoWork();

        pWave->GetState( &dwState );
    }

    // Destroy the wave and wavebank and release the memory
    pWave->Destroy();
    pWaveBank->Destroy();

    ATG::UnloadFilePhysicalMemory( pbWaveBank );
}


//--------------------------------------------------------------------------------------
// Name: PlayFromWaveFile()
// Desc: Play a sound from a WAV file
//--------------------------------------------------------------------------------------
VOID PlayFromWaveFile( IXACT3Engine* pXACTEngine )
{
    // Play a wave file
    ATG::WaveFile WaveFile;

    HRESULT hr = WaveFile.Open( szWaveFilePath );
    if( FAILED( hr ) )
        ATG::FatalError( "Error %#X opening wave file\n", hr );

    WAVEFORMATEXTENSIBLE wfx = { 0 };
    WaveFile.GetFormat( &wfx );

    DWORD dwWaveSize;
    WaveFile.GetDuration( &dwWaveSize );

    // Set our allocation to that size
    BYTE* pbData = new BYTE[ dwWaveSize ];

    // Read sample data from the file
    WaveFile.ReadSample( 0, pbData, dwWaveSize, &dwWaveSize );

    // Initialize WAVEBANKENTRY struct
    WAVEBANKENTRY entry;
    entry.Format.wFormatTag = WAVEBANKMINIFORMAT_TAG_PCM;
    entry.Format.wBitsPerSample = ( wfx.Format.wBitsPerSample == 16 ) ? WAVEBANKMINIFORMAT_BITDEPTH_16 :
        WAVEBANKMINIFORMAT_BITDEPTH_8;
    entry.Format.nChannels = wfx.Format.nChannels;
    entry.Format.wBlockAlign = wfx.Format.nChannels * ( wfx.Format.wBitsPerSample / 8 );
    entry.Format.nSamplesPerSec = wfx.Format.nSamplesPerSec;
    entry.Duration = dwWaveSize / ( wfx.Format.wBitsPerSample / 8 );
    entry.LoopRegion.dwStartSample = 0;
    entry.LoopRegion.dwTotalSamples = 0;
    entry.PlayRegion.dwOffset = 0;
    entry.PlayRegion.dwLength = dwWaveSize;
    entry.dwFlags = 0;

    // Create an in-memory IXACTWave interface using wave file data
    IXACT3Wave* pWave = NULL;
    hr = pXACTEngine->PrepareInMemoryWave( 0, entry, NULL, pbData, 0, 0, &pWave );
    if( FAILED( hr ) )
        ATG::FatalError( "PrepareInMemoryWave failed with error %#X\n", hr );

    // Set the volume
    pWave->SetVolume( 1.0f );

    // Play the wave
    hr = pWave->Play();
    if( FAILED( hr ) )
        ATG::FatalError( "Play failed with error %#X\n", hr );

    // Loop while allowing XACT to do work until the wave completes.
    DWORD dwState = 0;
    while( dwState != XACT_STATE_STOPPED )
    {
        pXACTEngine->DoWork();

        pWave->GetState( &dwState );
    }

    // Free the wave data
    pWave->Destroy();
    delete [] pbData;
}


//--------------------------------------------------------------------------------------
// Name: main()
// Desc: Entry point to the program
//--------------------------------------------------------------------------------------
INT __cdecl main()
{
    // Initialize the XACT runtime parameters
    XACT_RUNTIME_PARAMETERS xrParams = {0};
    xrParams.fnNotificationCallback = NULL;
    xrParams.lookAheadTime = XACT_ENGINE_LOOKAHEAD_DEFAULT;

    // Create the XACT runtime engine
    IXACT3Engine* pXACTEngine;

    // Create XACT Engine
    HRESULT hr = hr = XACT3CreateEngine( 0, &pXACTEngine );
    if( FAILED( hr ) )
        ATG::FatalError( "Could not create a XACT Engine with error %#X\n", hr );

    hr = pXACTEngine->Initialize( &xrParams );
    if( FAILED( hr ) )
        ATG::FatalError( "XACTInitialized failed with error %#X\n", hr );

    // Play a sound from a wave bank
    PlayFromWaveBank( pXACTEngine );

    // Play a sound from a WAV file
    PlayFromWaveFile( pXACTEngine );

    // Shut down and free XACT resources
    pXACTEngine->ShutDown();

    // Return to the launcher
    XLaunchNewImage( "", 0 );
}


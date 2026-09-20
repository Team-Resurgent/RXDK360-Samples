//--------------------------------------------------------------------------------------
// XactBasicSound.cpp
//
// This sample demonstrates the basic functionality of XACT by showing
// how to create and play sounds using the XACT runtime engine.
//
// XNA Developer Connection
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include <xtl.h>
#include <xact3.h>
#include <AtgUtil.h>
#include <AtgInput.h>

//--------------------------------------------------------------------------------------
// Name: XACTNotificationCallback()
// Desc: Received notifications from the XACT engine.  Assume that the pvContext
//         is an event handle which is signaled.
//--------------------------------------------------------------------------------------
void XACTNotificationCallback( const XACT_NOTIFICATION* pNotification )
{
    if( ( NULL != pNotification ) && ( NULL != pNotification->pvContext ) )
    {
        SetEvent( ( HANDLE )pNotification->pvContext );
    }
}

//--------------------------------------------------------------------------------------
// Name: main
// Desc: Entry point to the program
//--------------------------------------------------------------------------------------
VOID __cdecl main()
{
    // Initialize the XACT runtime parameters
    XACT_RUNTIME_PARAMETERS xrParams = {0};

    xrParams.fnNotificationCallback = XACTNotificationCallback;
    xrParams.lookAheadTime = XACT_ENGINE_LOOKAHEAD_DEFAULT;

    // Create the XACT runtime engine
    IXACT3Engine* pXACTEngine;

    // Create XACT Engine
    HRESULT hr;
    if( FAILED( hr = XACT3CreateEngine( 0, &pXACTEngine ) ) )
        ATG::FatalError( "Could not create a XACT Engine with error %#X\n", hr );

    hr = pXACTEngine->Initialize( &xrParams );
    if( FAILED( hr ) )
        ATG::FatalError( "XACTInitialized failed with error %#X\n", hr );

    // Open the in memory XMA wave bank
    DWORD dwFileSize = 0;
    VOID* pbWaveBank = NULL;
    // Make sure XMA contents must reside in physically contiguous memory
    if( FAILED( hr = ATG::LoadFilePhysicalMemory( "game:\\media\\sounds\\XactSounds.xwb", &pbWaveBank,
                                                  &dwFileSize ) ) )
        return;

    // Register the wave bank with XACT
    IXACT3WaveBank* pWaveBank;
    if( FAILED( hr = pXACTEngine->CreateInMemoryWaveBank( pbWaveBank, dwFileSize, 0, 0, &pWaveBank ) ) )
        ATG::FatalError( "CreateInMemoryWaveBank failed with error %#X\n", hr );

    // Load the sound bank
    VOID* pbSoundBank = NULL;
    if( FAILED( hr = ATG::LoadFile( "game:\\media\\sounds\\XactSounds.xsb", &pbSoundBank, &dwFileSize ) ) )
        return;

    // Register the sound bank with XACT
    IXACT3SoundBank* pSoundBank;
    if( FAILED( hr = pXACTEngine->CreateSoundBank( pbSoundBank, dwFileSize, 0, 0, &pSoundBank ) ) )
        ATG::FatalError( "CreateSoundBank failed with error %#X\n", hr );

    // Get the sound cue index from the sound bank
    XACTINDEX dwSoundCueIndex = 0;

    dwSoundCueIndex = pSoundBank->GetCueIndex( "MusicMono" ); // Null-terminated string representing the friendly
    if( dwSoundCueIndex == XACTINDEX_INVALID )
        ATG::FatalError( "GetCueIndex failed\n" );

    // Initialize XACT notification struct
    XACT_NOTIFICATION_DESCRIPTION xactNotificationDesc = {0};
    xactNotificationDesc.type = XACTNOTIFICATIONTYPE_CUESTOP;
    xactNotificationDesc.pSoundBank = pSoundBank;
    xactNotificationDesc.cueIndex = dwSoundCueIndex;
    xactNotificationDesc.pvContext = CreateEvent( NULL, FALSE, FALSE, NULL );

    if( NULL == xactNotificationDesc.pvContext )
    {
        ATG::FatalError( "Failed to create event object for XACTNOTIFICATIONTYPE_CUESTOP notification" );
    }

    // Register a stop notification with the XACT .
    // This will allow us to monitor when the cue stops playing.
    if( FAILED( hr = pXACTEngine->RegisterNotification( &xactNotificationDesc ) ) )
        ATG::FatalError( "Notification registration failed with error %#X\n", hr );

    // Play the sound cue
    IXACT3Cue* pCue;
    if( FAILED( hr = pSoundBank->Play( dwSoundCueIndex, 0, 0, &pCue ) ) )
        ATG::FatalError( "Play failed with error %#X\n", hr );

    do
    {
        pXACTEngine->DoWork();
        Sleep( 1 );
    } while( WAIT_TIMEOUT == WaitForSingleObject( ( HANDLE )xactNotificationDesc.pvContext, 4 ) );

    CloseHandle( ( HANDLE )xactNotificationDesc.pvContext );

    // Destroy interfaces
    pCue->Destroy();

    // Destroy soundbank
    pSoundBank->Destroy();
    ATG::UnloadFile( pbSoundBank );

    // Destroy wavebank
    pWaveBank->Destroy();
    ATG::UnloadFilePhysicalMemory( pbWaveBank );

    // Shut down and free XACT resources
    pXACTEngine->ShutDown();

    // Reboot the dev kit
    XLaunchNewImage( "", 0 );
}

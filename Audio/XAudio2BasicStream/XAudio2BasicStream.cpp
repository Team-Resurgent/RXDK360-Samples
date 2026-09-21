//--------------------------------------------------------------------------------------
// XAudio2BasicStream.cpp
//
// XNA Developer Connection
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include <xtl.h>
#include <AtgFont.h>
#include <AtgInput.h>
#include <AtgConsole.h>
#include <XAudio2.h>
#include <AtgUtil.h>
#include <AtgWavebank.h>

#define WAVEBANK_FILE_NAME "game:\\Media\\Sounds\\WaveBank.xwb"

#define STREAMING_BUFFER_SIZE 65536
#define MAX_BUFFER_COUNT 3

struct StreamingVoiceContext : public IXAudio2VoiceCallback
{
    void OnVoiceProcessingPassStart( UINT32 BytesRequired ){}
    void OnVoiceProcessingPassEnd(){}
    void OnStreamEnd(){}
    void OnBufferStart( void* ){}
    void OnBufferEnd( void* ){ SetEvent( hBufferEndEvent ); }
    void OnLoopEnd( void* ){}
    void OnVoiceError( void*, HRESULT ){}

    HANDLE hBufferEndEvent;

    StreamingVoiceContext(): hBufferEndEvent( CreateEvent( NULL, FALSE, FALSE, NULL ) ){}
    virtual ~StreamingVoiceContext(){ CloseHandle( hBufferEndEvent ); }
};

int main( void )
{
    HRESULT hr = S_OK;

    //
    // Initialize the console window
    //
    ATG::Console console;
    console.Create( "game:\\Media\\Fonts\\Arial_16.xpr", 0xFF1F005F, 0xFFFFFFFF );
    console.SendOutputToDebugChannel( TRUE );

    //
    // Initialize XAudio2
    //
    IXAudio2* pXAudio2 = NULL;
    if ( FAILED(hr = XAudio2Create( &pXAudio2 ) ) )
        ATG::FatalError( "Error %#X calling XAudio2Create\n", hr );

    //
    // Create a mastering voice
    //
    IXAudio2MasteringVoice* pMasterVoice = NULL;
    if ( FAILED(hr = pXAudio2->CreateMasteringVoice( &pMasterVoice, 6,
        48000, 0, 0, NULL ) ) )
        ATG::FatalError( "Error %#X calling CreateMasteringVoice\n", hr );

    //
    // Load the wavebank
    //
    ATG::CWavebank wavebank;
    if( FAILED( wavebank.Open( WAVEBANK_FILE_NAME ) ) )
        ATG::FatalError( "Error %#X opening wavebank\n", hr );

    //
    // Open the wavebank file with an async handle
    //
    // CWavebank::Open loaded the wavebank header data, but for streaming we need
    // a real, live file handle that has the sorts of properties that the aynchronous
    // file APIs (the "overlapped" functions) crave. We can then use this handle to
    // grab chunks of playback data without blocking our thread (much).
    //
    HANDLE hAsync = CreateFile(
        WAVEBANK_FILE_NAME,
        GENERIC_READ,
        FILE_SHARE_READ,
        NULL,
        OPEN_EXISTING,
        FILE_FLAG_OVERLAPPED|FILE_FLAG_NO_BUFFERING,
        NULL );

    if( hAsync == INVALID_HANDLE_VALUE )
        ATG::FatalError( "Couldn't open file for async read" );

    //
    // Now go through the entries and play each of them in turn.
    //
    for(;;)
    {
        for( DWORD i = 0; i < wavebank.GetEntryCount(); ++i )
        {
            console.Format( "Now playing wave entry %d", i );

            // Detect exit sample request LT-RT-RB
            ATG::Input::GetMergedInput();

            //
            // Get the info we need to play back this wave (need enough space for PCM, XMA2, and xWMA formats)
            //
            char formatBuff[ 64 ];
            WAVEFORMATEX *wfx = reinterpret_cast<WAVEFORMATEX*>(&formatBuff);

            if( FAILED( hr = wavebank.GetEntryFormat( i, wfx ) ) )
                ATG::FatalError( "Couldn't get wave format for entry %d: error 0x%x\n", i, hr );
            DWORD waveOffset = wavebank.GetEntryOffset( i );
            DWORD waveLength = wavebank.GetEntryLengthInBytes( i );

            //
            // Create an XAudio2 voice to stream this wave
            //
            StreamingVoiceContext voiceContext;
            IXAudio2SourceVoice* pSourceVoice;
            if( FAILED(hr = pXAudio2->CreateSourceVoice( &pSourceVoice, wfx,
                0, XAUDIO2_DEFAULT_FREQ_RATIO, &voiceContext, NULL, NULL ) ) )
                ATG::FatalError( "Error %#X creating source voice\n", hr );
            pSourceVoice->Start( 0, 0 );

            //
            // Create an overlapped structure to handle the async I/O
            //
            OVERLAPPED ovlCurrentRequest = {0};
            ovlCurrentRequest.hEvent = CreateEvent( NULL, TRUE, FALSE, NULL );

            if ( ovlCurrentRequest.hEvent == NULL )
                ATG::FatalError( "Error creating event for overlapped I/O\n" );

            //
            // I/O buffers -- declared static to avoid using excessive stack space
            //
            static BYTE buffers[MAX_BUFFER_COUNT][STREAMING_BUFFER_SIZE];
            DWORD currentDiskReadBuffer = 0;
            DWORD currentPosition = 0;

            //
            // This sample code shows the simplest way to manage asynchronous
            // streaming. There are three different processes involved. One is the management
            // process, which is what we're writing here. The other two processes are
            // essentially hardware operations: disk reads from the I/O system, and
            // audio processing from XAudio2. Disk reads and audio playback both happen
            // without much intervention from our application, so our job is just to make
            // sure that the data being read off the disk makes it over to the audio
            // processor in time to be played back.
            //
            // There are two events that can happen in this system. The disk I/O system can
            // signal that data is ready, and the audio system can signal that it's done
            // playing back data. We can handle either or both of these events either synchronously
            // (via polling) or asynchronously (via callbacks or by waiting on an event
            // object).
            //
            while( currentPosition < waveLength )
            {
                console.Format( "." );

                // Detect exit sample request LT-RT-RB
                ATG::Input::GetMergedInput();

                //
                // Issue a request.
                //
                // Note: although the file read will be done asynchronously, it is possible for the
                // call to ReadFileEx to block for longer than you might think. If the I/O system needs
                // to read the file allocation table in order to satisfy the read, it will do that
                // BEFORE returning from ReadFileEx. That means that this call could potentially
                // block for several milliseconds! In order to get "true" async I/O you should put
                // this entire loop on a separate thread.
                //
                // Second note: async requests have to be a multiple of the disk sector size. Rather than
                // handle this conditionally, make all reads the same size but remember how many
                // bytes we actually want and only submit that many to the voice.
                //
                DWORD cbValid = min( STREAMING_BUFFER_SIZE, waveLength - currentPosition );
                ovlCurrentRequest.Offset = waveOffset + currentPosition;
                if(!ReadFileEx( hAsync, buffers[currentDiskReadBuffer], STREAMING_BUFFER_SIZE, &ovlCurrentRequest, NULL ))
                    ATG::FatalError( "Couldn't start async read: error %#X\n", GetLastError() );
                currentPosition += cbValid;

                //
                // At this point the read is progressing in the background and we are free to do
                // other processing while we wait for it to finish. For the purposes of this sample,
                // however, we'll just go to sleep until the read is done.
                //
                DWORD cb;
                GetOverlappedResult( hAsync, &ovlCurrentRequest, &cb, TRUE );

                //
                // Now that the event has been signaled, we know we have audio available. The next
                // question is whether our XAudio2 source voice has played enough data for us to give
                // it another buffer full of audio. We'd like to keep no more than MAX_BUFFER_COUNT - 1
                // buffers on the queue, so that one buffer is always free for disk I/O.
                //
                XAUDIO2_VOICE_STATE state;
                for(;;)
                {
                    // Detect exit sample request LT-RT-RB
                    ATG::Input::GetMergedInput();

                    pSourceVoice->GetState( &state, XAUDIO2_VOICE_NOSAMPLESPLAYED );
                    if ( state.BuffersQueued < MAX_BUFFER_COUNT - 1 )
                        break;

                    WaitForSingleObject( voiceContext.hBufferEndEvent, INFINITE );
                }

                //
                // At this point we have a buffer full of audio and enough room to submit it, so
                // let's submit it and get another read request going.
                //
                XAUDIO2_BUFFER buf = {0};
                buf.AudioBytes = cbValid;
                buf.pAudioData = buffers[currentDiskReadBuffer];
                if( currentPosition >= waveLength )
                    buf.Flags = XAUDIO2_END_OF_STREAM;
                pSourceVoice->SubmitSourceBuffer( &buf );

                currentDiskReadBuffer++;
                currentDiskReadBuffer %= MAX_BUFFER_COUNT;
            }
            console.Format( "done streaming.." );
            XAUDIO2_VOICE_STATE state;
            for(;;)
            {
                // Detect exit sample request LT-RT-RB
                ATG::Input::GetMergedInput();

                pSourceVoice->GetState( &state, XAUDIO2_VOICE_NOSAMPLESPLAYED );
                if ( !state.BuffersQueued )
                    break;

                console.Format( "." );
                WaitForSingleObject( voiceContext.hBufferEndEvent, INFINITE );
            }

            //
            // Clean up
            //
            pSourceVoice->Stop( 0 );
            pSourceVoice->DestroyVoice();
            CloseHandle( ovlCurrentRequest.hEvent );

            console.Format( "stopped\n" );
            Sleep(500);
        }
    }
}

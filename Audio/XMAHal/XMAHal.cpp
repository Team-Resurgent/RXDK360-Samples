//--------------------------------------------------------------------------------------
// XMAHal.cpp
//
// This sample demonstrates how to use the XMA Hardware Abstraction Layer API to
// decode XMA data directly and obtain the results for further processing, rather
// than letting the XAudio2 API handle decoding internally.
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include <xtl.h>
#include <xaudio2.h>
#include <AtgApp.h>
#include <AtgAudio.h>
#include <AtgUtil.h>
#include <AtgInput.h>

#include "XMAHardwareAbstraction.h"

#define VERBOSE_DEBUG

IXAudio2* pXAudio2Engine = NULL;

//--------------------------------------------------------------------------------------
// Constants
//--------------------------------------------------------------------------------------
const int   XMA_BUFFER_SIZE = 131072;      // number of bytes to submit to XMA at a time.
// It's possible to submit up to 8MB at a time,
// and when you're submitting in-memory buffers
// you should generally submit the whole thing
// at once. This sample submits smaller buffers
// in order to show how to submit data that's
// broken up into little pieces--for instance,
// data that's being streamed from disk.

const int   SAMPLE_BUFFER_SIZE = 1024;    // number of samples in XMA output buffer

//--------------------------------------------------------------------------------------
// CreatePCMOutputVoicesForXMAFile
//
// In order to hear the output from the XMA decoder we need to pass it to XAudio2.
// Since the XMA decoder output is 16-bit PCM data, we can pass it directly
// to an XAudio2 PCM voice for output. This function creates a set of PCM voices,
// one for each XMA stream, and sets them up to output surround sound. The demo
// will pass buffers of decoded XMA sound to these voices.
//
// NOTE: this function is for demo purposes only. If the decoded data will not be
//       processed further before being sent to XAudio2, it is simpler and more
//       efficient to feed the original XMA data to XAudio2 directly.
//--------------------------------------------------------------------------------------
IXAudio2SourceVoice** CreatePCMOutputVoicesForXMAFile( const XMA2WAVEFORMATEX& SourceFormat)
{
    HRESULT hr = S_OK;

    WORD nNumStreams = SourceFormat.NumStreams;

    //Create the default mastering voice
    IXAudio2MasteringVoice* pMasteringVoice = 0;
    if( FAILED( hr = pXAudio2Engine->CreateMasteringVoice(&pMasteringVoice)))
        ATG::FatalError( "Error %#X calling CreateMasteringVoice\n", hr );


    IXAudio2SourceVoice** pSourceVoices = new IXAudio2SourceVoice*[ nNumStreams ];

    //
    // Create a source voice for each XMA stream
    // NOTE: It's also possible to create one multichannel voice
    // and manually interleave the XMA streams before handing them
    // to XAudio2. The method shown here is the simplest way to
    // get this functionality, not necessarily the best.
    //
    for( WORD i = 0; i < nNumStreams; ++i )
    {
        WORD nChannelCount = 2;

        // Create the channel mapping matrix.
        // In this sample, there cannot be more than 6 destination channels
        // and two source channels:
        float fLevelMatrix[6 * 2] = {0};

        // Set the first and second channel mapping to the appropriate destination:
        // [SourceChannelCount * (ChannelsPerStream * StreamIndex + ChannelIndex) + ChannelIndex]
        fLevelMatrix[2*(2*i+0)+0] = 1.0f;
        fLevelMatrix[2*(2*i+1)+1] = 1.0f;

        // If channel count is odd, set channel count
        // and level mapping for final stream,
        if ((i == (nNumStreams-1)) && (SourceFormat.wfx.nChannels % 2 == 1))
        {
            nChannelCount = 1;
            fLevelMatrix[2*(2*i+1)+1] = 0.0f;
        }

        WAVEFORMATEX pSourceFormat = {0};
        pSourceFormat.wFormatTag  = WAVE_FORMAT_PCM;
        pSourceFormat.nChannels = nChannelCount;
        pSourceFormat.nSamplesPerSec = SourceFormat.wfx.nSamplesPerSec;
        pSourceFormat.wBitsPerSample = 16;
        pSourceFormat.cbSize = 34;

        // Calculations good for PCM data only:
        pSourceFormat.nBlockAlign = pSourceFormat.nChannels * pSourceFormat.wBitsPerSample / 8;
        pSourceFormat.nAvgBytesPerSec = pSourceFormat.nBlockAlign * pSourceFormat.nSamplesPerSec;

        pSourceVoices[i] = NULL;

        if( FAILED( hr = pXAudio2Engine->CreateSourceVoice(&pSourceVoices[i],reinterpret_cast<WAVEFORMATEX *>(&pSourceFormat) ) ) )
            ATG::FatalError( "Error %#X calling XAudioCreateSourceVoice\n", hr );


        pSourceVoices[i]->SetOutputMatrix(NULL, nChannelCount, 6, fLevelMatrix);

    }

    return pSourceVoices;
}


//--------------------------------------------------------------------------------------
// Name: main
// Desc: Entry point to the program
//--------------------------------------------------------------------------------------
VOID __cdecl main()
{

    //
    // Initialize the XAudio2 Engine
    //

    HRESULT hr = XAudio2Create( &pXAudio2Engine );
    if( FAILED( hr ) )
        ATG::FatalError( "Error %#X calling XAudioInitialize\n", hr );

    //
    // Load an XMA file using WaveFile, which supports XMA2EX format files.
    //
    ATG::WaveFile XmaFile;
    if( FAILED( hr = XmaFile.Open( "game:\\Media\\Sounds\\MusicSurround.xma2" ) ) )
        ATG::FatalError( "Error %#X opening XMA2 file\n", hr );

    WAVEFORMATEXTENSIBLE waveFormat = {0};
    XMA2WAVEFORMATEX sourceFormat = {0};
    if (FAILED ( hr = XmaFile.GetFormat(&waveFormat, &sourceFormat)))
        ATG::FatalError( "Error %#X calling WaveFile.GetFormat\n",hr );


    //
    // Find out how big the new sample is and allocate enough memory to
    // hold it.
    //
    DWORD dwWaveSize;
    XmaFile.GetDuration( &dwWaveSize );

    BYTE* pbData = ( BYTE* )XPhysicalAlloc( dwWaveSize, MAXULONG_PTR, 0, PAGE_READWRITE | MEM_LARGE_PAGES );


    //
    // Read sample data from the file
    //
    XmaFile.ReadSample( 0, pbData, dwWaveSize, &dwWaveSize );


    //
    // Set up XMA decoder
    //
    WORD nNumStreams = sourceFormat.NumStreams;
    PXMAPLAYBACK pXmaPlayback = { 0 };
    XMA_PLAYBACK_INIT* xmaInit = new XMA_PLAYBACK_INIT[ nNumStreams ];

    for( WORD i = 0; i < nNumStreams; ++i )
    {
        BYTE nChannelCount = 2;
        //Set channel count for final stream, if channels are odd
        if ((i == (nNumStreams-1)) && (sourceFormat.wfx.nChannels % 2 == 1))
            nChannelCount = 1;

        xmaInit[i].channelCount = nChannelCount;
        xmaInit[i].outputBufferSizeInSamples = SAMPLE_BUFFER_SIZE;
        xmaInit[i].sampleRate = sourceFormat.wfx.nSamplesPerSec;
        xmaInit[i].subframesToDecode = 4;
    }

    XMAPlaybackInitialize();

    if( FAILED( XMAPlaybackCreate( nNumStreams, xmaInit, 0L, &pXmaPlayback ) ) )
        ATG::FatalError( "XMA Playback HAL creation failed" );
    delete[] xmaInit;


    //
    // PCM voice creation
    //
    // We use PCM voices to pump the output from the XMA encoder to
    // the XAudio2 main outputs.
    //
    IXAudio2SourceVoice** pSourceVoice = CreatePCMOutputVoicesForXMAFile( sourceFormat );


    //
    // Set up pointers for XMA submission
    //
    BYTE* pXmaData = pbData;
    DWORD xmaDataPos = 0;

    //
    // Start the source voices. They'll play silence until we give
    // them real data.
    //
    for( WORD i = 0; i < nNumStreams; ++i )
    {
        if( FAILED( hr = pSourceVoice[i]->Start( 0 ) ) )
            ATG::FatalError( "Error %#X calling Start\n", hr );
    }

    //
    // Submit the first buffer of XMA data to the hardware.
    // To do this we need to lock the hardware, then submit the buffer
    // to each separate XMA decoder context. Since the buffer is
    // interleaved on XMA packet (2KB) boundaries, we increment
    // the buffer pointer by 2K for each stream after the first one.
    // This means that stream 0 starts at buffer[0], stream 1 starts
    // at buffer[2048], stream 2 at buffer[4096]...
    //
    DWORD xmaSubmitSize = min( XMA_BUFFER_SIZE, dwWaveSize - xmaDataPos );
    XMAPlaybackRequestModifyLock( pXmaPlayback );
    XMAPlaybackWaitUntilModifyLockObtained( pXmaPlayback );

    for( WORD i = 0; i < nNumStreams; ++i )
    {
        BYTE* pStreamStart = pXmaData + ( i * XMA_BYTES_PER_PACKET );
        XMAPlaybackSubmitData(
            pXmaPlayback,
            i,
            pStreamStart,
            xmaSubmitSize - ( i * XMA_BYTES_PER_PACKET ) );
    }
    pXmaData += xmaSubmitSize;
    xmaDataPos += xmaSubmitSize;
    XMAPlaybackResumePlayback( pXmaPlayback );

    //
    // Now go into a loop that lasts as long as we're still playing data.
    //
    XAUDIO2_VOICE_STATE SourceState;
    bool FileHasEnded = false;
    bool done = false;
    while( !done )
    {

        //
        // Lock the XMA contexts associated with our data so that we can read
        // and modify them without worrying about data sync issues.
        //

        XMAPlaybackRequestModifyLock( pXmaPlayback );
        XMAPlaybackWaitUntilModifyLockObtained( pXmaPlayback );

        //
        // If we haven't yet run out of source XMA data, see if we
        // can submit some more of it.
        //
        xmaSubmitSize = min( XMA_BUFFER_SIZE, dwWaveSize - xmaDataPos );
        if( xmaSubmitSize > 0 )
        {
            //
            // Ask the decoder if it needs more data.
            // When working with interleaved XMA it's good practice
            // to submit the same data to each context; this simplifies
            // the case where the data is being streamed from disk. The
            // encoder is responsible for making sure that the data is
            // interleaved in such a way that no stream ever gets too
            // far ahead of the others.
            //
            bool readyData = true;
            for( WORD i = 0; i < nNumStreams; ++i )
            {
                if( !XMAPlaybackQueryReadyForMoreData( pXmaPlayback, i ) )
                {
                    // One of the streams isn't ready, so we're not going
                    // to submit anything this time around
                    readyData = false;
                    break;
                }
            }

            //
            // If our XMA contexts are ready for more data, then submit the
            // next piece of the buffer.
            //
            if( readyData )
            {
                // Submit the same data to each of the streams.
                for( WORD i = 0; i < nNumStreams; ++i )
                {
                    XMAPlaybackSubmitData( pXmaPlayback, i, pXmaData, xmaSubmitSize );
                    ATG::DebugSpew( "Submitted %d bytes to stream %d\n", xmaSubmitSize, i );
                }

                // keep track of where we are in the XMA wave.
                xmaDataPos += xmaSubmitSize;
                pXmaData += xmaSubmitSize;
            }
        }

        //
        // Now see if there's decoded data that we can send to XAudio2
        //
        done = true; // start by assuming all streams have finished
        for( WORD i = 0; i < nNumStreams; ++i )
        {
            //
            // Before we check the XMA decoder, check XAudio2 to see if it can
            // accept another packet. This effectively throttles our decode
            // rate to match the XAudio2 playback rate.
            //
            pSourceVoice[i]->GetState( &SourceState, XAUDIO2_VOICE_NOSAMPLESPLAYED );

            if( FileHasEnded && SourceState.BuffersQueued == 0 )
            {
                ATG::DebugSpew( "stream %d is done\n", i );
            }
            else
            {
                // Keep going as long as any voice is started.
                done = false;
            }

            if( SourceState.BuffersQueued < XAUDIO2_MAX_QUEUED_BUFFERS )
            {
                //
                // We have room for more data. See if there's any available.
                //
                DWORD samplesAvailable = XMAPlaybackQueryAvailableData( pXmaPlayback, i );
                if( samplesAvailable )
                {
                    //
                    // There are samples available! Allocate a buffer for them. The
                    // buffer will be deleted by the buffer completion callback.
                    //
                    // NOTE: in a production app we'd of course want to reuse
                    //       buffers rather than allocating a new buffer every
                    //       time.
                    //

                    WORD nChannels = 2;
                    // Set channel count for final stream, if channels are odd
                    if ((i == (nNumStreams-1)) && (sourceFormat.wfx.nChannels % 2 == 1))
                        nChannels = 1;


                    WORD* pBuffer = new WORD[ samplesAvailable * nChannels ];
                    if( !pBuffer )
                    {
                        ATG::FatalError( "Out of memory for decoded audio buffers" );
                    }

                    //
                    // Copy the samples from the XMA decoder's buffer into our own buffer.
                    // Because the XMA hardware uses a ring buffer to hold decoded data,
                    // we may not get all of the data the first time--loop until we've
                    // read all of the available samples.
                    //
                    WORD* pTempBuffer = pBuffer;
                    DWORD tempCount = samplesAvailable;
                    while( tempCount > 0 )
                    {
                        WORD* pData = 0;

                        // Call XMAPlaybackConsumeDecodedData() to get a pointer to the
                        // decoder's output buffer. This also advances the decoder's output
                        // read pointer, which is what it uses to keep track of how much of
                        // the buffer we've read. The change isn't committed to the hardware
                        // until we call XMAPlaybackResumePlayback().
                        //
                        DWORD samplesRead = XMAPlaybackConsumeDecodedData( pXmaPlayback, i,
                                                                           tempCount, ( void** )&pData );
#ifdef VERBOSE_DEBUG
                        ATG::DebugSpew( "Got %d samples from stream %d\n", tempCount, i );
#endif

                        // copy the samples from the decoder's buffer into memory that
                        // we own.
                        memcpy( pTempBuffer, pData, samplesRead * sizeof( WORD ) * nChannels );

                        // advance our own pointers.
                        pTempBuffer += samplesRead * nChannels;
                        tempCount -= samplesRead;
                    }


                    //
                    // Submit an XAUDIOPACKET structure that defines the buffer that we're
                    // submitting and the pointer to the busy flag for this buffer.
                    //
                    XAUDIO2_BUFFER buffer = { 0 };
                    buffer.AudioBytes = samplesAvailable * sizeof( WORD ) * nChannels;
                    buffer.pAudioData = (BYTE *)(pBuffer);
                    buffer.pContext = pBuffer;

                    //
                    // Submit the decoded data to the XAudio2 voice.
                    //
                    if( XMAPlaybackIsIdle( pXmaPlayback, i ) )
                    {
                        // Special case: idle XMA means that decoding has finished, and
                        // that means that this is the last buffer. We need to let the
                        // XAudio2 engine know not to expect any more buffers for this
                        // voice.
                        buffer.Flags = XAUDIO2_END_OF_STREAM;

                        // We set this flag here, and monitor when the final sample has finished playing above.
                        FileHasEnded = true;
                    }

                    if( FAILED( hr = pSourceVoice[i]->SubmitSourceBuffer( &buffer) ) )
                        ATG::FatalError( "Error %#X calling SubmitPacket\n", hr );

                }
            }
        }
        XMAPlaybackResumePlayback( pXmaPlayback );

        if( DWORD parseErr = XMAPlaybackGetParseError( pXmaPlayback, 0 ) )
        {
            ATG::FatalError( "XMA Parser error %d\n", parseErr );
            break;
        }

        // Detect reboot keypress
        ATG::Input::GetMergedInput();

        Sleep( 1 );
    }

    // Release XMA object
    XMAPlaybackDestroy( pXmaPlayback );

    // Release voice resources
    for( WORD i = 0; i < nNumStreams; ++i )
    {
        pSourceVoice[i]->DestroyVoice();
    }

    // Free allocated memory
    delete[] pSourceVoice;
    XPhysicalFree( pbData );



    // Shut down and free XAudio2 resources
    pXAudio2Engine->Release();

    // Reboot the dev kit
    XLaunchNewImage( "", 0 );

}

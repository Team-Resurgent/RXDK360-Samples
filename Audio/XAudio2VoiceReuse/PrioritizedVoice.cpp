//--------------------------------------------------------------------------------------
// PrioritizedVoice.cpp
//
// XNA Developer Connection
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include <xaudio2.h>
#include <AtgConsole.h>
#include <AtgUtil.h>
#include <AtgAudio.h>
#include "XAudio2VoiceReuse.h"

//--------------------------------------------------------------------------------------
// Class: PrioritizedVoice
//  Desc: This class handles reading data from the disk, managing the memory for an 
//        XAudio2 voice, and manages the XAudio2 voice itself. Of most interest is 
//        Play(), which handles flushing any existing buffers and enables voice reuse.
//--------------------------------------------------------------------------------------


//
//  Default wave formats that the voices will be started with.
//  These values should be similar to the content you plan to play through the voice.
//  Sampling rate can change after voice creation, but channel count, format tag, 
//  and BitsPerSample cannot.
//
static const WAVEFORMATEX DEFAULTWAVEFORMAT_MONOPCM = {
    1,    //WORD wFormatTag        -- Integer identifier of the format
    1,    //WORD nChannels         -- Number of audio channels
    44100,//DWORD nSamplesPerSec   -- Audio sample rate
    88200,//DWORD nAvgBytesPerSec  -- Bytes per second (possibly approximate)
    2,    //WORD nBlockAlign       -- Size in bytes of a sample block (all channels)
    16,   //WORD wBitsPerSample    -- Size in bits of a single per-channel sample
    0     //WORD cbSize            -- Bytes of extra data appended to this struct
};
static const XMA2WAVEFORMATEX DEFAULTWAVEFORMAT_MONOXMA = { 
    {358,  //WORD wFormatTag        -- Integer identifier of the format
    1,     //WORD nChannels         -- Number of audio channels
    32000, //DWORD nSamplesPerSec   -- Audio sample rate
    8457,  //DWORD nAvgBytesPerSec  -- Bytes per second (possibly approximate)
    4,     //WORD nBlockAlign       -- Size in bytes of a sample block (all channels)
    16,    //WORD wBitsPerSample    -- Size in bits of a single per-channel sample
    34},   //WORD cbSize            -- Bytes of extra data appended to this struct
    1,     //WORD  NumStreams;      -- Number of audio streams (1 or 2 channels each)
    1,     //DWORD ChannelMask;     -- Spatial positions of the channels in this file,
           //                            stored as SPEAKER_xxx values (see audiodefs.h)
    0,     //DWORD SamplesEncoded;  -- Total number of PCM samples the file decodes to
    65536, //DWORD BytesPerBlock;   -- XMA block size (but the last one may be shorter)
    0,     //DWORD PlayBegin;       -- First valid sample in the decoded audio
    0,     //DWORD PlayLength;      -- Length of the valid part of the decoded audio
    0,     //DWORD LoopBegin;       -- Beginning of the loop region in decoded sample terms
    0,     //DWORD LoopLength;      -- Length of the loop region in decoded sample terms
    0,     //BYTE  LoopCount;       -- Number of loop repetitions; 255 = infinite
    4,     //BYTE  EncoderVersion;  -- Version of XMA encoder that generated the file
    2      //WORD  BlockCount;      -- XMA blocks in file (and entries in its seek table)
};


//--------------------------------------------------------------------------------------
// Name: Create()
// Desc: Static factory method that creates a Prioritized Voice
//--------------------------------------------------------------------------------------
PrioritizedVoice* PrioritizedVoice::Create(IXAudio2* pXAudio2, 
                                        DWORD DefaultSamplesPerSec, 
                                        WORD ChannelCount, 
                                        PrioritizedVoiceType VoiceType,
                                        float MaxFrequencyRatio)
{
    assert(pXAudio2);

    HRESULT hr =S_OK;
    PrioritizedVoice* pv = new PrioritizedVoice();
    if (!pv)
        ATG::FatalError( "Failed to allocate Prioritized Voice\n" );

    //
    // Create an XAudio2 voice to handle playing our sounds,
    //  using a default wfx constant to pre-populate the most common fields.
    // Note that this is set to be the most common format for this sample,
    //  and will change depending on any given title's assets.
    //
    WAVEFORMATEX* pwfx;
    WAVEFORMATEX wfx = DEFAULTWAVEFORMAT_MONOPCM;
    XMA2WAVEFORMATEX xwfx = DEFAULTWAVEFORMAT_MONOXMA;
    
    if(TypePCM == VoiceType)
    {
        wfx.nChannels = ChannelCount;
        wfx.nSamplesPerSec = DefaultSamplesPerSec;
        wfx.nBlockAlign = ChannelCount * wfx.wBitsPerSample / 8;
        wfx.nAvgBytesPerSec = DefaultSamplesPerSec * wfx.nBlockAlign;
        pwfx = &wfx;
    }
    else if(TypeXMA == VoiceType)
    {
        xwfx.wfx.nChannels = ChannelCount;
        xwfx.wfx.nSamplesPerSec = DefaultSamplesPerSec;
        xwfx.wfx.nBlockAlign = ChannelCount * xwfx.wfx.wBitsPerSample / 8;
        xwfx.wfx.nAvgBytesPerSec = DefaultSamplesPerSec * xwfx.wfx.nBlockAlign;
        xwfx.NumStreams = (WORD)(((float)xwfx.wfx.nChannels) / 2.0f + .5f);
        xwfx.ChannelMask = 0;
        pwfx = (WAVEFORMATEX*)&xwfx;
    }
    else
        ATG::FatalError( "An unsupported content type was played");

    pv->m_iChannelCount = ChannelCount;
    pv->m_iBuffersPlaying = 0;
    pv->m_VoiceType = VoiceType;
    if( FAILED(hr = pXAudio2->CreateSourceVoice( &pv->m_pSourceVoice, pwfx,
        0, MaxFrequencyRatio, &pv->m_VoiceContext, NULL, NULL ) ) )
        ATG::FatalError( "Error %#X creating source voice\n", hr );
    pv->m_iPriority = 0;

    return pv;
}


//--------------------------------------------------------------------------------------
// Name: Play()
// Desc: Plays new content, flushing any current buffers as needed.
//--------------------------------------------------------------------------------------
void PrioritizedVoice::Play(int Priority, float PitchShift, const LPWSTR szFilename)
{
    HRESULT hr = S_OK;

    //
    // If a voice has been identified as one to reuse, we will stop the voice.
    // Note that this is an immediate stop without tails, and could cause a glitch.
    //
    m_pSourceVoice->Stop();



    //
    // If the voice has buffers queued, these buffers must be flushed
    //
    if (HasQueuedBuffers())
    {
        if( FAILED( hr = m_pSourceVoice->FlushSourceBuffers()))
            ATG::FatalError( "Error %#X flushing source voice buffers.\n", hr );

        // Wait until the source buffers have been flushed which can take up to 5ms. The sample 
        // will be notified through OnBufferEnd() when the source buffers have been flushed.
        // A better solution would queue the request and wait until the next frame to process it, 
        // giving time for the source buffers to be released while maximizing CPU usage for other 
        // game components.
        while( HasQueuedBuffers() )
            Sleep( 1 );
    }

    //
    // Keep track of how many buffers we've sent to this voice, so that we can identify
    // when the voice has finished playing. This is done with interlocked math, because
    // the OnBufferEnd callback is called from the XAudio2's realtime thread.
    // 
    InterlockedIncrement(&m_iBuffersPlaying);


    //
    // Set member properties, and create the file handle.
    //
    this->m_dwLastPlayed = GetTickCount();
    this->m_iPriority = Priority;

    WAVEFORMATEXTENSIBLE wfx = {0};
    ATG::WaveFile WaveFile;
    DWORD  cbWaveSize = 0;
    BYTE* pData    = 0;

    char strFilePath[ MAX_PATH ];
    sprintf_s( strFilePath, "game:\\Media\\Sounds\\%S", szFilename);
    if( FAILED( hr = WaveFile.Open( strFilePath) ) )
        ATG::FatalError( "Error %#X opening WAV file\n", hr );


    if (TypePCM == m_VoiceType)
    {
        //
        // Retrieve the format header
        //
        if( FAILED( hr = WaveFile.GetFormat( &wfx ) ) )
            ATG::FatalError( "Error %#X reading WAV format\n", hr );

        if (wfx.Format.nChannels != m_iChannelCount)
            ATG::FatalError( "Error: Content of %d channels was attempted to play on %d channel voice\n", wfx.Format.nChannels, m_iChannelCount );

        //
        // Calculate how many bytes and samples are in the wave, and read it into memory
        //
        WaveFile.GetDuration( &cbWaveSize );

        pData = ( BYTE* )XPhysicalAlloc( cbWaveSize, MAXULONG_PTR, 2048, PAGE_READWRITE );
        if( FAILED( hr = WaveFile.ReadSample( 0, pData, cbWaveSize, &cbWaveSize ) ) )
            ATG::FatalError( "Error %#X reading WAV data\n", hr );
    }
    else if (TypeXMA == m_VoiceType)
    {
        //
        // Retrieve the format header
        //
        XMA2WAVEFORMATEX xma2 = {0};
        if( FAILED( hr = WaveFile.GetFormat( &wfx, &xma2 ) ) )
            ATG::FatalError( "Error %#X reading WAV format\n", hr );

        if( wfx.Format.wFormatTag != WAVE_FORMAT_XMA2 )
            ATG::FatalError( "Error - Expected an XMA2 XAudio2 compatible file\n" );

        //
        // Calculate how many bytes and samples are in the wave, 
        // and read it into memory (XMA packets must be 2K aligned)
        //
        // NOTE: Refer to the XAudio2BasicStream sample for an example of proper 
        //       audio streaming techniques. Here we are submitting the entire
        //       stream in one submission.
        //
        WaveFile.GetDuration( &cbWaveSize );

        pData = ( BYTE* )XPhysicalAlloc( cbWaveSize, MAXULONG_PTR, 2048, PAGE_READWRITE );
        if( FAILED( hr = WaveFile.ReadSample( 0, pData, cbWaveSize, &cbWaveSize ) ) )
            ATG::FatalError( "Error %#X reading WAV data\n", hr );    
    }

    //
    // We store a pointer to the count of buffers to be used in OnBufferEnd,
    // to signify when the buffer has completed playing.
    // We also store a pointer to the data buffer itself, 
    // to be freed in OnBufferEnd
    //
    VoiceContextData* pVoiceData = new VoiceContextData(&this->m_iBuffersPlaying,pData, this->m_pSourceVoice);
    if (NULL == pVoiceData)
        ATG::FatalError( "Error %#X allocating voice context structure.\n", hr );

    //
    // Set the sampling rate for the voice to match the new content.
    //
    if( FAILED( hr = this->m_pSourceVoice->SetSourceSampleRate(wfx.Format.nSamplesPerSec)))
        ATG::FatalError( "Error %#X changing source voice sampling rate.\n", hr );
    
    //
    // Set the pitch shift of the voice
    //
    this->m_pSourceVoice->SetFrequencyRatio(PitchShift);

    //
    // Submit the voice data using an XAUDIO2_BUFFER structure
    // pContext is set to a handle of the VoiceContextData struct, 
    // so that the buffer can be freed in OnBufferEnd
    //
    XAUDIO2_BUFFER buffer = {0};
    buffer.pAudioData = pData;
    buffer.Flags = XAUDIO2_END_OF_STREAM;
    buffer.AudioBytes = cbWaveSize;
    buffer.pContext = pVoiceData; 
    if( FAILED( hr = m_pSourceVoice->SubmitSourceBuffer( &buffer ) ) )
        ATG::FatalError( "Error %#X submitting source buffer\n", hr );

    if( FAILED( hr = m_pSourceVoice->Start() ) )
        ATG::FatalError( "Error %#X starting voice.\n", hr );
}

//--------------------------------------------------------------------------------------
// Name: StreamingVoiceContext::OnBufferEnd()
// Desc: Callback that deletes the buffer when XAudio2 is finished with it
//--------------------------------------------------------------------------------------
void VoiceContext::OnBufferEnd(void* Context)
{
    assert(Context);
    VoiceContextData* vcd = ((VoiceContextData*) Context);

    // Decrement the buffers currently playing.
    InterlockedDecrement(vcd->m_plSourceVoiceBufferCounter);

    XPhysicalFree(vcd->m_pdwDataBuffer);

    delete Context;
}

//--------------------------------------------------------------------------------------
// Name: HasQueuedBuffers()
// Desc: Helper function which returns if the voice is currently playing from a buffer.
//--------------------------------------------------------------------------------------
bool PrioritizedVoice::HasQueuedBuffers()
{
    return (this->m_iBuffersPlaying != 0);
}
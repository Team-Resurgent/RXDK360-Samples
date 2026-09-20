//--------------------------------------------------------------------------------------
// XWMADecode.cpp
//
// This sample demonstrates how to use the XWMADecode API to decode XWMA data
// and obtain the results for further processing, rather than letting the XAudio2 API
// handle decoding internally.
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

#include <xwmadecode.h>

using namespace XAUDIO2;

IXAudio2* pXAudio2Engine = NULL;

//--------------------------------------------------------------------------------------
// Defines
//--------------------------------------------------------------------------------------

#define HRESULT_FROM_POINTER(p) \
    ((p) ? S_OK : E_OUTOFMEMORY)

#define HRFROMP(p) \
    HRESULT_FROM_POINTER(p)

#define VERBOSE_DEBUG

//--------------------------------------------------------------------------------------
// Globals
//--------------------------------------------------------------------------------------

typedef struct USER_CONTEXT { // the user context to be passed to the input callback
    BYTE* pXWMAData;					// pointer to the start of our XWMA data
    DWORD XWMADataTotalSize;			// total size, in bytes, of our XWMA data
    DWORD XWMABytesSoFar;			    // how many bytes of XWMA input data we have already given to the decoder
    XWMADECODE* pDecoder;				// the decoder object for this file
    WAVEFORMATEXTENSIBLE WaveFmt;		// our wave format structure
    BYTE* pPCM;						    // our decoded data
    DWORD PCMTotalSize;				 	// total size, in bytes, of our decoded data buffer
    DWORD PCMBytesSoFar;				// how many bytes of decoded XWMA data we have already copied from the decoder
    BYTE* pMemoryBlockToUse;			// the working buffer passed to the decoder
} USER_CONTEXT;
// This is the user's context. Typically the context contains enough information
// for the input callback to perform its job. The context will be accessible inside
// the input callback via the pUserContext parameter.

//--------------------------------------------------------------------------------------
// ParseXWMA
//
// This function parses the header of a XWMA file and extracts the format information,
// along with the XWMA data itself.
//
//--------------------------------------------------------------------------------------
HRESULT ParseXWMA(const char* pszFileName, WAVEFORMATEXTENSIBLE* fmt, BYTE** data, DWORD *cbSize, DWORD *cbDecodedSize)
{
    HRESULT hr = S_OK;

    ATG::WaveFile WaveFile;

    if( FAILED( hr = WaveFile.Open( pszFileName ) ) )
        ATG::FatalError( "Error %#X opening XWMA file %s\n", hr, pszFileName);

    WAVEFORMATEXTENSIBLE wfx = {0};

    if( FAILED( hr = WaveFile.GetFormat( &wfx, NULL ) ) )
        ATG::FatalError( "Error %#X reading XWMA format\n", hr );

    if( wfx.Format.wFormatTag != WAVE_FORMAT_EXTENSIBLE && wfx.Format.wFormatTag != WAVE_FORMAT_WMAUDIO3 && wfx.Format.wFormatTag != WAVE_FORMAT_WMAUDIO2)
        ATG::FatalError( "Error - Expected an XWMA compatible file\n" );

    WaveFile.GetDuration( cbSize );

    *data = new BYTE[*cbSize];

    // the decoder expects big endian - do not perform endianness conversion
    if( FAILED( hr = WaveFile.ReadSampleRaw( 0, *data, *cbSize, cbSize ) ) )
        ATG::FatalError( "Error %#X reading XWMA data\n", hr );

    *fmt = wfx;

    DWORD dwPacketCount;
    DWORD dwDpdsBufferSize;

    if FAILED( hr = WaveFile.GetPacketCumulativeBytesSize(&dwPacketCount, &dwDpdsBufferSize) )
        ATG::FatalError( "Error - unable to parse dpds structure\n");

    DWORD* pDpds = (DWORD*)new BYTE[dwDpdsBufferSize];

    if (FAILED( hr = WaveFile.GetPacketCumulativeBytes(pDpds) ))
        ATG::FatalError( "Error reading dpds chunk\n");

    *cbDecodedSize = pDpds[dwPacketCount-1];

    delete[] pDpds;

    return hr;
}

//--------------------------------------------------------------------------------------
// GetXWMAData
//
// This is our input callback.
// It keeps track of where in the XWMA input buffer we currently are, and feeds XWMA
// data from that point up to a maximum of the packet size bytes.
//
// The input callback below simply walks through our XWMA data pointer and feeds
// the right amount of data every time.
// It is possible to implement a more sophisticated input callback - for example, the input
// callback can stream data from disk and store relevant state in the user context.
// It is important to note though that there is no asynch operation going on during the decoding
// process. The decoder is blocked calling this input callback waiting for more data, and
// it assumed that the data is pointed to by pInputData when this callback returns.
//--------------------------------------------------------------------------------------
HRESULT GetXWMAData(void* pUserContext, XWMADECODE_INPUT_BUFFER_INFO* pInfo)
{
    USER_CONTEXT* pContext = (USER_CONTEXT*)pUserContext;

    //
    // The input begins at our next chunk
    //
    pInfo->pInputData = pContext->pXWMAData + pContext->XWMABytesSoFar;

    DWORD bytesRemaining =  pContext->XWMADataTotalSize - pContext->XWMABytesSoFar;

    //
    // Constrain our bytes to the packet size. We can't give more than one packet to the decoder.
    //
    DWORD bytesToGive = (bytesRemaining < pContext->pDecoder->XWMAPacketByteSize) ? (bytesRemaining) : (pContext->pDecoder->XWMAPacketByteSize);

    pContext->XWMABytesSoFar += bytesToGive;

    pInfo->InputDataBytes = bytesToGive;

    pInfo->NoMoreInput = (pContext->XWMABytesSoFar < pContext->XWMADataTotalSize) ? FALSE : TRUE;

    return S_OK; // we always return OK here - but we could return an error code and we would get it in XWMADecodeProcessData
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
        ATG::FatalError( "Error %#X calling XAudio2Create\n", hr );

    //
    // Initialize our context
    //

    USER_CONTEXT* pContext = new USER_CONTEXT;
    ZeroMemory(pContext, sizeof(USER_CONTEXT));

    if( FAILED( hr = ParseXWMA("game:\\Media\\Sounds\\MusicSurround.xwm", &(pContext->WaveFmt), &(pContext->pXWMAData), &(pContext->XWMADataTotalSize), &(pContext->PCMTotalSize)) ) )
        ATG::FatalError( "Error %#X opening XWMA file\n", hr );

    pContext->pPCM = new BYTE[pContext->PCMTotalSize];

    DWORD cBufferSize;
    cBufferSize = XWMADecodeGetRequiredBufferSize(&(pContext->WaveFmt.Format));

    pContext->pMemoryBlockToUse = new BYTE[cBufferSize];

    hr =  XWMADecodeCreate(&(pContext->WaveFmt.Format), GetXWMAData, pContext, pContext->pMemoryBlockToUse, cBufferSize, &(pContext->pDecoder));

	if (FAILED(hr))
        ATG::FatalError( "Error %#X creating XWMA decoding context\n", hr );

    DWORD cPCMBytes;
    while (pContext->pDecoder->DecoderState != XWMADECODE_STATE_DONE)
    {
        switch(pContext->pDecoder->DecoderState)
        {
        case XWMADECODE_STATE_READY_TO_PROCESS:
            hr = XWMADecodeProcessData(pContext->pDecoder);
            if ( FAILED( hr ) )
            {
                ATG::FatalError("Error %#X decoding data\n", hr);
            }
			//
			// The 'FramesReady' field tells us how many frames are ready for consumption *per channel*.
			// So to figure out the total number of bytes available for consumption we multiply that
			// by the number of channels then by the size, in bytes, of each sample.
			//
            cPCMBytes = pContext->pDecoder->FramesReady * pContext->pDecoder->OutputChannels * pContext->pDecoder->SampleSize;
			//
			// We accumulate all of our decoded data into one big buffer pointed to by 'pPCM'.
			//
            CopyMemory(pContext->pPCM + pContext->PCMBytesSoFar, pContext->pDecoder->pDecodedData, cPCMBytes);
            pContext->PCMBytesSoFar += cPCMBytes;
            break;
        default:
            ATG::FatalError("Undefined state: %d\n", pContext->pDecoder->DecoderState);
            break;
        }
    }

    OutputDebugString("Done!\n");

    //
    // Create a mastering voice
    //
    IXAudio2MasteringVoice* pMasteringVoice = NULL;
    if( FAILED( hr = pXAudio2Engine->CreateMasteringVoice( &pMasteringVoice ) ) )
        ATG::FatalError( "Error %#X calling CreateMasteringVoice\n", hr );

    //
    // Play the decoded data
    //
    OutputDebugString("Playing the decoded data.\n");

    {
        IXAudio2SourceVoice* pSourceVoice;

        WAVEFORMATEX pSourceFormat = {0};
        pSourceFormat.wFormatTag  = WAVE_FORMAT_PCM;
        pSourceFormat.nChannels = pContext->WaveFmt.Format.nChannels;
        pSourceFormat.nSamplesPerSec = pContext->WaveFmt.Format.nSamplesPerSec;
        pSourceFormat.wBitsPerSample = pContext->WaveFmt.Format.wBitsPerSample;
        pSourceFormat.cbSize = 34;

        // Calculations good for PCM data only:
        pSourceFormat.nBlockAlign = pSourceFormat.nChannels * pSourceFormat.wBitsPerSample / 8;
        pSourceFormat.nAvgBytesPerSec = pSourceFormat.nBlockAlign * pSourceFormat.nSamplesPerSec;

        if( FAILED( hr = pXAudio2Engine->CreateSourceVoice(&pSourceVoice,reinterpret_cast<WAVEFORMATEX *>(&pSourceFormat) ) ) )
            ATG::FatalError( "Error %#X calling XAudioCreateSourceVoice\n", hr );

        XAUDIO2_BUFFER buffer = {0};
        buffer.pAudioData = pContext->pPCM;
        buffer.Flags = XAUDIO2_END_OF_STREAM;  // tell the source voice not to expect any data after this buffer
        buffer.AudioBytes = pContext->PCMBytesSoFar;

        if( FAILED( hr = pSourceVoice->SubmitSourceBuffer( &buffer ) ) )
            ATG::FatalError( "Error %#X submitting source buffer\n", hr );

        hr = pSourceVoice->Start( 0 );

        BOOL isRunning = TRUE;
        while( SUCCEEDED( hr ) && isRunning )
        {
            XAUDIO2_VOICE_STATE state;
            pSourceVoice->GetState( &state, XAUDIO2_VOICE_NOSAMPLESPLAYED );
            isRunning = ( state.BuffersQueued > 0 );
        }

        pSourceVoice->DestroyVoice();

    }

    // All XAudio2 interfaces are released when the engine is destroyed, but being tidy
    pMasteringVoice->DestroyVoice();

    // Shut down and free XAudio2 resources
    pXAudio2Engine->Release();

    // Free XWMA-related resources
    XWMADecodeDestroy(pContext->pDecoder);
    delete[] pContext->pMemoryBlockToUse;

    // Free additional memory we allocated for our context
    delete[] pContext->pXWMAData;
    delete[] pContext->pPCM;
    delete pContext;

    // Reboot the dev kit
    XLaunchNewImage( "", 0 );
}

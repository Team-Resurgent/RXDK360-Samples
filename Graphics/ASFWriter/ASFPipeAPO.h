//--------------------------------------------------------------------------------------
// ASFPipeAPO.h
//
// Simple xAPO that records the buffer to the hard drive.
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once
#include <ATGAPOBase.h>
#include <ATGDsp.h>
#include <AtgLockFreePipe.h>
#include <AtgUtil.h>
#include <audiodefs.h>
#include <xbdm.h>
#include <xaudio2.h>

// The audio pipe is 2^19 bytes in size, which is large enough to hold several of the largest buffers
#define RECORD_APO_PIPE_LEN 19 
typedef ATG::LockFreePipe<RECORD_APO_PIPE_LEN> AudioPipe;

// Max per-quantum buffer size in bytes: 192,000 [48Khz, stereo, INT16 stream] * .05333ms [XAudio2 quantum on 360]
#define MAX_BUFFERSIZE_PER_XAUDIO2_QUANTUM 10240  

struct AudioPipeParams{};
static XALLOC_ATTRIBUTES AudioBufferAllocation = {0, TRUE, TRUE, FALSE, 0, XALLOC_ALIGNMENT_16, 
                                                  XALLOC_MEMPROTECT_READWRITE, FALSE, XALLOC_MEMTYPE_HEAP };

struct RecordingThreadArgs{
	HANDLE hExitThreadEvent;
	HANDLE hFile;
	AudioPipe* pPipe;
	DWORD BytesWritten;
	DWORD DataLengthOffset;
	DWORD BytesSentToPipe;
};

class __declspec( uuid( "{0A10CB5B-DFEF-48C6-8B70-1FB1BEDED6DC}" ) ) 
CASFPipeAPO
	: public ATG::CSampleXAPOBase<CASFPipeAPO, AudioPipeParams>
{
public:
	CASFPipeAPO(){}
	~CASFPipeAPO( void ){}
    AudioPipe* GeAudioPipe() {return &m_AudioPipe;}
private:
    
    // Overrides
    //
    void DoProcess(    const AudioPipeParams& params,
        FLOAT32* __restrict pData,
        UINT32 cFrames,
        UINT32 cChannels,
        BOOL   bIsEnabled );

	HRESULT LockForProcess(
         UINT32 InputLockedParameterCount,
         const XAPO_LOCKFORPROCESS_BUFFER_PARAMETERS* pInputLockedParameters,
         UINT32 OutputLockedParameterCount,
         const XAPO_LOCKFORPROCESS_BUFFER_PARAMETERS* pOutputLockedParameters );

	void UnlockForProcess();

    // Private member fields
    AudioPipe   m_AudioPipe;
    INT16       m_PerQuantumBuffer[MAX_BUFFERSIZE_PER_XAUDIO2_QUANTUM];
    UINT32       m_PipeBytesPerQuantum;
};

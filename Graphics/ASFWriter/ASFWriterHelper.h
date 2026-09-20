//--------------------------------------------------------------------------------------
// ASFWriterHelper.h
//
//
// XNA Developer Connection
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#pragma once
#include <xtl.h>
#include <AsfWriterApi.h>
#include <AtgUtil.h>
#include "ASFPipeAPO.h"

// Create a pipe that can store lots of frames at 1/2-size
const int XMVENCODER_GRAPHICS_PIPE_LENGTH =28;
typedef ATG::LockFreePipe<XMVENCODER_GRAPHICS_PIPE_LENGTH> XMVEncoderGraphicsPipe;

// A buffer that can store the largest frame we record, plus the timecode
// Note that you may chose other sizes for your title.
const int MAX_FRAME_BUFFERSIZE = 4 * (921600 + sizeof(LONGLONG));

class ASFWriterHelper: public IAsfWriterVideoStream, public IAsfWriterAudioStream
{
public:
    ASFWriterHelper();
    HRESULT Initialize( __in const char* pszDestinationFileName, 
                       __in const WAVEFORMATEX* AudioSourceFormat, 
                       __in const AsfWriterBitmapInfoHeader* VideoSourceFormat,
                       __in const AsfWriterAudioEncInfo* AudioEncoderFormat, 
                       __in const AsfWriterVideoEncInfo* VideoEncoderFormat,
                       __in AudioPipe* pAudioPipe,
                       __in XMVEncoderGraphicsPipe* m_GraphicsPipe );
    HRESULT GetNextFrame( __out_ecount(*puLength) BYTE* pFrame,
                         __inout UINT* puLength,
                         __out LONGLONG* phnsSampleTime );
    HRESULT GetNextSample( __out_ecount(*puLength) BYTE* pSample,
                          __inout UINT* puLength );
    HRESULT Start();
    HRESULT StopAsync();
    HRESULT Stop();
    BOOL    IsRecording()   {return m_bRecording;  }
    BOOL    IsStopping()    {return m_bStopping;   }
    BOOL    IsInitialized() {return m_bInitialized;}
    VOID    Update();
   ~ASFWriterHelper();
private:
    IAsfWriter*         m_pAsfWriter;
    AudioPipe*          m_pAudioPipe;

    XMVEncoderGraphicsPipe* m_pGraphicsPipe;
    
    BOOL                 m_bRecording;
    BOOL                 m_bStopping;
    XOVERLAPPED          m_ASFHandle;
    LONGLONG             m_hnsLatestTimeStamp;
    BOOL                 m_AudioStreamStopped;
    BOOL                 m_VideoStreamStopped;
    BOOL                 m_bInitialized;
    BYTE                 *m_pwSingleFrame;
};

typedef struct WMARateEntry_tag
{
    UINT32  nBitrate;             // bit rate
    UINT8   nBitDepth;
    UINT32  flags;
} WMARateEntry;

#pragma pack()

typedef struct WMARateList_tag
{
    UINT32 nSamplingRate;
    UINT16 nChannels;
    UINT32 nEntries;
    const WMARateEntry* pRates;
} WMARateList;

#define WMA2 0x0008

static const WMARateEntry WMAOPTIONS22050m[] = { // 448ms per packet, or 2.23 packets per second.
    { 20000, 16, WMA2 },// sub
    { 16000, 16, WMA2 }// sub
};

static const WMARateEntry WMAOPTIONS22050s[] = { 
    { 32000, 16, WMA2 }, // 371.5ms per packet, or 2.69 packets per second. 
    { 22000, 16, WMA2 }, // 650ms per packet, or 1.538 packets per second. 
    { 20000, 16, WMA2 }  // 650ms per packet, or 1.538 packets per second. 
};

static const WMARateEntry WMAOPTIONS32000m[] = { // 640ms per packet, or 1.5625 packets per second. 
    { 20000, 16, WMA2 } 
};

static const WMARateEntry WMAOPTIONS32000s[] = { // 371.5ms per packet, or 2.69 packets per second
    { 48000, 16, WMA2 },
    { 40000, 16, WMA2 },
    { 32000, 16, WMA2 } 
};

static const WMARateEntry WMAOPTIONS44100m[] = { // 371.5ms per packet, or 2.69 packets per second
    { 48000, 16, WMA2 },
    { 32000, 16, WMA2 },
    { 20000, 16, WMA2 }
};

static const WMARateEntry WMAOPTIONS44100s[] = { // 371.5ms per packet, or 2.69 packets per second
    { 320000, 16, WMA2 }, 
    { 256000, 16, WMA2 }, 
    { 192000, 16, WMA2 }, 
    { 160000, 16, WMA2 }, 
    { 128000, 16, WMA2 }, 
    {  96000, 16, WMA2 }, 
    {  80000, 16, WMA2 }, 
    {  64000, 16, WMA2 }, 
    {  48000, 16, WMA2 } 
};


static const WMARateEntry WMAOPTIONS48000s[] = { // 341ms per packet, or 2.93 packets per second
    { 192000, 16, WMA2 }, 
    { 191000, 16, WMA2 }, 
    { 160000, 16, WMA2 }, 
    { 128000, 16, WMA2 }, 
    { 127000, 16, WMA2 }, 
    {  96000, 16, WMA2 }, 
    {  95000, 16, WMA2 }, 
    {  64000, 16, WMA2 }, 
    {  63000, 16, WMA2 }, 
};

static const WMARateList WMAOPTIONS[] = {
    { 22050, 1, ARRAY_SIZE( WMAOPTIONS22050m ), &WMAOPTIONS22050m[0] },
    { 22050, 2, ARRAY_SIZE( WMAOPTIONS22050s ), &WMAOPTIONS22050s[0] },
    { 32000, 1, ARRAY_SIZE( WMAOPTIONS32000m ), &WMAOPTIONS32000m[0] },
    { 32000, 2, ARRAY_SIZE( WMAOPTIONS32000s ), &WMAOPTIONS32000s[0] },
    { 44100, 1, ARRAY_SIZE( WMAOPTIONS44100m ), &WMAOPTIONS44100m[0] },
    { 44100, 2, ARRAY_SIZE( WMAOPTIONS44100s ), &WMAOPTIONS44100s[0] },
    { 48000, 2, ARRAY_SIZE( WMAOPTIONS48000s ), &WMAOPTIONS48000s[0] },
};

static const UINT32 WMAOPTIONS_SIZE = sizeof( WMAOPTIONS ) / sizeof( WMARateList );

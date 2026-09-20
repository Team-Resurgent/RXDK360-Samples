//--------------------------------------------------------------------------------------
// XMic.h
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#pragma once

#ifndef XMIC_H
#define XMIC_H

#define XMIC_MAX_FRAME_COUNT            8
#define XMIC_WIRELESS_FRAME_COUNT       1
#define XMIC_WIRED_FRAME_COUNT          2
#define XMIC_START_TIMEOUT              5000

// prototypes for utility functions
HRESULT XMicSampleProcessCapabilities( DWORD m_dwMicrophoneIndex, 
                                      XMICCAPABILITIES* pCapabilities );

HRESULT XMicSampleCreateVoice( DWORD m_dwMicrophoneIndex, IXAudio2SourceVoice** pm_pVoice, 
                              DWORD dwMicrophoneSampleRate, ATG::MonitorAPOPipe *pmonitor);

HRESULT XMicSampleProcessGain( DWORD m_dwMicrophoneIndex );

void XMicSampleDataReady( DWORD dwErrorCode, DWORD dwNumberOfBytesTransfereed, 
                         XOVERLAPPED *pOverlapped );



//
// Data structure for asynchronously receiving data from the microphone
//
struct MicrophoneDataRequestCompletionData
{
    DWORD                m_dwMicrophoneIndex;
    XOVERLAPPED*         pOverlap;
    IXAudio2SourceVoice* m_pVoice;
    DWORD                dwFrameCount;
    DWORD                dwAudioFrameBufferLength;
    BYTE*                pAudioFramesBuffer;    
    WORD                 wFrameLengths[XMIC_MAX_FRAME_COUNT];
};


//
// Internal data strucutre to hold state of each connected mic
//
class XMicInstance
{
public:
    XMicInstance() 
    { 
        m_dwDeviceState = 0;
        m_fAsyncLEDReqPending = FALSE;
    };

    BOOL Initialize( DWORD dwUserIndex, D3DDevice *pd3ddevice, 
        ATG::PackedResource *presource );
    BOOL Update( const FLOAT fElapsedTime );
    BOOL Render( D3DDevice* pd3ddevice );
    BOOL Connect();
    void HandleInput( ATG::GAMEPAD* pGamepad );
    
    // Update visualization data (called from main thread)
    void UpdateWidget()
    {
        m_pWidget->Update();
    }

private:
    DWORD                                   m_dwDeviceIndex;
    DWORD                                   m_dwMicrophoneIndex;
    DWORD                                   m_dwDeviceState;
    IXAudio2SourceVoice*                    m_pVoice;
    MicrophoneDataRequestCompletionData     m_MCD1;
    MicrophoneDataRequestCompletionData     m_MCD2;
    XOVERLAPPED                             m_DataRequestOverlapped1;
    XOVERLAPPED                             m_DataRequestOverlapped2;
    // Async LED updating
    XOVERLAPPED                             m_LEDChangeOverlapped;
    BOOL                                    m_fAsyncLEDReqPending;
    DWORD                                   m_dwLEDIndex;         
    DWORD                                   m_dwLEDChangeTick;
    // XInput2 data
    XINPUT2CONTEXT                          m_DeviceContext;      
    D3DVECTOR                               m_XMicAccelVector;

    // Visualizer data
    CMonitorAPOWidget   *m_pWidget;
    ATG::MonitorAPOPipe *m_pMonitor;
};

//--------------------------------------------------------------------------------------
// Name: class XMicSample
// Desc: Main class to run this application.
//--------------------------------------------------------------------------------------
class XMicSample : public ATG::Application
{
public:
    ATG::Timer  m_Timer;

    // Cache out the microphone accellerometer vector
    XMicInstance     m_XMicInstance[XUSER_MAX_COUNT];

private:
    ATG::Font   m_Font;


    // Demonstrate asynchronous output
    XOVERLAPPED m_m_LEDChangeOverlapped;
    BOOL        m_fAsyncLEDReqPending;

    // Read capabilities of microphone and output to TTY
    HRESULT XMicSampleProcessCapabilities( DWORD m_dwMicrophoneIndex, XMICCAPABILITIES* pCapabilities );

    virtual     HRESULT Initialize();
    virtual     HRESULT Update();
    virtual     HRESULT Render();

    static DWORD WINAPI UpdateMicThread( LPVOID lpParameter );

    ATG::PackedResource m_resource;
    D3DTexture  *m_pTexture;

    HRESULT XMicSampleInitXAudio2( void );
};

//--------------------------------------------------------------------------------------
// Name: class VoiceCallback
// Desc: XAudio2 call backs
//--------------------------------------------------------------------------------------
class VoiceCallback : public IXAudio2VoiceCallback
{
public:
    HANDLE hBufferEndEvent;
    VoiceCallback(): hBufferEndEvent( CreateEvent( NULL, FALSE, FALSE, NULL ) ){}
    ~VoiceCallback(){ CloseHandle( hBufferEndEvent ); }

    // Called when the voice has just finished playing a contiguous audio stream.
    void OnStreamEnd() { SetEvent( hBufferEndEvent ); }

    // Unused methods are stubs
    void OnVoiceProcessingPassEnd() { }
    void OnVoiceProcessingPassStart(UINT32 SamplesRequired) {    }
    void OnBufferEnd(void * pBufferContext);
    void OnBufferStart(void * pBufferContext) {    }
    void OnLoopEnd(void * pBufferContext) {    }
    void OnVoiceError(void * pBufferContext, HRESULT Error) { }
};

#endif // XMIC_H
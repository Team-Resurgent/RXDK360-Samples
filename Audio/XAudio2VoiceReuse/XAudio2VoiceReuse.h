//--------------------------------------------------------------------------------------
// XAudio2VoiceReuse_Helpers.h
//
// XNA Developer Connection
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#pragma once
#include <XAudio2.h>

enum PrioritizedVoiceType
{
    TypeXWMA,
    TypeXMA,
    TypePCM
};

static const int MAX_LOG_ENTRIES = 15;
static const float MAX_PITCH = 1.2f; // This title never uses more than 1.2f frequency ratio

static const int LOG_EMPTY = -2;
static const int LOG_VOICE_UNPLAYED = -1;

//
// LogEntry: Helper structure used to track voice playback attempts in PrioritizedVoices::Play
//
struct LogEntry
{
    int VoiceIndex;
    int OldPri;
    int NewPri;
    LogEntry() {
        VoiceIndex=LOG_EMPTY;
        OldPri=0;
        NewPri=0;}
};

//
// VoiceContext: Helper structure used to monitor OnBufferEnd callbacks.
//
struct VoiceContext : public IXAudio2VoiceCallback
{
    void OnVoiceProcessingPassStart( UINT32 /*BytesRequired*/ ){}
    void OnVoiceProcessingPassEnd(){}
    void OnStreamEnd(){}
    void OnBufferStart( void* ){}
    void OnBufferEnd( void* Context );
    void OnLoopEnd( void* ){}
    void OnVoiceError( void*, HRESULT ){}

    VoiceContext(){}
    virtual ~VoiceContext(){}
};

//
// VoiceContextData: Helper structure used to store a pointer to the count of buffers
// currently queued (without the need to call the more expensive IXAudio2SourceVoice:GetState(),
// as well as to store a pointer to the buffer that needs to be freed when finished.
//
struct VoiceContextData
{
    long*   m_plSourceVoiceBufferCounter;
    BYTE* m_pdwDataBuffer;
    IXAudio2SourceVoice* m_pSourceVoice;

    VoiceContextData(long* plSourceVoiceBufferCounter, BYTE* pdwDataBuffer, IXAudio2SourceVoice* pSourceVoice): 
            m_plSourceVoiceBufferCounter(plSourceVoiceBufferCounter),
            m_pdwDataBuffer(pdwDataBuffer),
            m_pSourceVoice(pSourceVoice){}
};
class PrioritizedVoice
{
    public:
        int m_iPriority;
        DWORD m_dwLastPlayed;
        static PrioritizedVoice* Create(IXAudio2* pXAudio2, 
                                        DWORD DefaultSamplesPerSec, 
                                        WORD Channelcount, 
                                        PrioritizedVoiceType VoiceType,
                                        float MaxFrequencyRatio);
        void Play(int Priority, float PitchShift, const LPWSTR  szFilename);
        ~PrioritizedVoice(void)
        {
            m_pSourceVoice->DestroyVoice();
        }
        IXAudio2SourceVoice* m_pSourceVoice;
        bool HasQueuedBuffers();
private:
    VoiceContext m_VoiceContext;
    int m_iChannelCount;
    PrioritizedVoiceType m_VoiceType;
    long m_iBuffersPlaying;
};

#pragma warning( push )
#pragma warning( disable : 4351 ) // array will be default initialized (with 0s) (this is new behavior, and is what we 
                                  // actually want here)
class PrioritizedVoices
{
public:
    static  PrioritizedVoices* Create(IXAudio2* pXAudio2, DWORD DefaultSamplesPerSec, WORD Channelcount, PrioritizedVoiceType VoiceType, int MaxVoiceCount, float MaxFrequencyRatio);
    void Play(int Priority, float PitchShift, const LPWSTR  szFilename);
    int* GetPlayingCounts();
    int m_iMaxVoiceCount;

    // Variables for storing a brief history of what has happened.
    int m_iCurrentLogIndex;
    LogEntry m_ppLogs[MAX_LOG_ENTRIES];

    ~PrioritizedVoices()
    {
        for( int i = 0; i < m_iMaxVoiceCount; ++i )
        {
            delete m_ppPrioritizedVoiceArray[i];
        }

        delete[] m_ppPrioritizedVoiceArray;
    }

private:
    PrioritizedVoices( int iMaxVoiceCount )
        : m_iMaxVoiceCount( iMaxVoiceCount ),
          m_iCurrentLogIndex( 0 ),
          m_ppPrioritizedVoiceArray( NULL ),
          m_iPlayingCounts()
    {
    }


    PrioritizedVoice** m_ppPrioritizedVoiceArray;
    int m_iPlayingCounts[3];
};
#pragma warning( pop )

extern int Random(int min, int max);
extern float Random(float min, float max);

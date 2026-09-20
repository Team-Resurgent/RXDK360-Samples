//--------------------------------------------------------------------------------------
// PrioritizedVoices.cpp
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
// Class: PrioritizedVoices
//  Desc: This class stores multiple PrioritizedVoice entities, and chooses which one
//        should be preempetd if there is no free voice available for a new sound.
//--------------------------------------------------------------------------------------

// Erroneous Code Analysis warning here, claiming we'll leak memory due to exception throwing - even though
// we don't have exceptions on Xbox. So disable it...
#pragma warning( push ) 
#pragma warning( disable : 6211 )

//--------------------------------------------------------------------------------------
// Name: Create()
// Desc: Static factory method that creates a Prioritized Voice manager
//--------------------------------------------------------------------------------------
PrioritizedVoices* PrioritizedVoices::Create(IXAudio2* pXAudio2, DWORD DefaultSamplesPerSec, 
                                             WORD Channelcount, PrioritizedVoiceType VoiceType, 
                                             int MaxVoiceCount, float MaxFrequencyRatio)
{
    PrioritizedVoices* pVoices = new PrioritizedVoices( MaxVoiceCount );
    if (!pVoices)
        ATG::FatalError( "Failed to allocate prioritized voice manager\n" );

    pVoices->m_ppPrioritizedVoiceArray = new PrioritizedVoice*[ MaxVoiceCount ];

    if ( !pVoices->m_ppPrioritizedVoiceArray )
    {
        delete pVoices;
        ATG::FatalError( "Failed to allocate voice array\n" );
    }

    pVoices->m_iCurrentLogIndex = 0;
    for(int i = 0; i < MaxVoiceCount; i++)
    {
        pVoices->m_ppPrioritizedVoiceArray[i] = PrioritizedVoice::Create(pXAudio2,DefaultSamplesPerSec,Channelcount,VoiceType, MaxFrequencyRatio);
    }
    return pVoices;
}

#pragma warning( pop )


//--------------------------------------------------------------------------------------
// Name: GetPlayingCounts()
// Desc: Cycles through voices and returns how many are playing, in each priority category.
//--------------------------------------------------------------------------------------
int* PrioritizedVoices::GetPlayingCounts()
{
    m_iPlayingCounts[0] = 0;m_iPlayingCounts[1]=0; m_iPlayingCounts[2]=0;
    for(int i=0;i<m_iMaxVoiceCount;i++)
    {
        if (m_ppPrioritizedVoiceArray[i]->HasQueuedBuffers())
            m_iPlayingCounts[m_ppPrioritizedVoiceArray[i]->m_iPriority]++;
    }
    return m_iPlayingCounts;
}


//--------------------------------------------------------------------------------------
// Name: Play()
// Desc: Attempts to find an available voice to play new content. Should a voice not
//        be available, it will then find the oldest voice among currently playing voices
//        that are of the same priority or older.
//       This function may not play the content, in which case the last log entry
//        is updated to reflect the fact.
//--------------------------------------------------------------------------------------
void PrioritizedVoices::Play(int Priority, float PitchShift, const LPWSTR szFilename)
{
    //
    // First, look for a voice that is not currently playing
    //
    int i = 0;
    LogEntry* Log = &m_ppLogs[m_iCurrentLogIndex];
    m_iCurrentLogIndex = (m_iCurrentLogIndex+1)%MAX_LOG_ENTRIES;

    for(i=0;i<m_iMaxVoiceCount;i++)
    {
        if( !m_ppPrioritizedVoiceArray[i]->HasQueuedBuffers())
        {
            // A voice was found that had finished playing a previous voice.
            // log the success and continue.
            Log->NewPri = Priority;
            Log->OldPri = -1;
            Log->VoiceIndex = i;
            break;
        }
    }

    if (i==m_iMaxVoiceCount)
    {
        //
        // If all voices are playing, search for the oldest voice of the same priority or lower.
        // This ensures that higher priority voices are not overridden, and that the voice that has
        //  played the longest (and is theoretically less important) will be stopped.
        //
        int highestPri = Priority;
        DWORD PreviousYoungest = MAXDWORD; 
        for(int j=0;j<m_iMaxVoiceCount;j++)
        {
            if (m_ppPrioritizedVoiceArray[j]->m_iPriority >= highestPri &&
                PreviousYoungest > m_ppPrioritizedVoiceArray[j]->m_dwLastPlayed)
            {
                i=j;
                highestPri=m_ppPrioritizedVoiceArray[j]->m_iPriority;
                PreviousYoungest = m_ppPrioritizedVoiceArray[j]->m_dwLastPlayed;
            }
        }

        if (i==m_iMaxVoiceCount)
        {
            // No preemptable voice was found. Log the failure and return.
            Log->NewPri = Priority;
            Log->OldPri = -1;
            Log->VoiceIndex = LOG_VOICE_UNPLAYED;
            return;
        }
        else
        {
            // A valid voice was found, log the transition and continue
            Log->NewPri = Priority;
            Log->OldPri = m_ppPrioritizedVoiceArray[i]->m_iPriority;
            Log->VoiceIndex = i;
        }
    }

    //
    // Finally, we call PrioritizedVoice::Play. If it is currently playing,
    // the PrioritizedVoice class will handle stopping the current content
    // and loading the new content into the existing XAudio2 voice.
    //
    m_ppPrioritizedVoiceArray[i]->Play(Priority, PitchShift, szFilename);
}


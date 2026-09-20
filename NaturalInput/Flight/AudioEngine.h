//--------------------------------------------------------------------------------------
// File: AudioEngine.h
//
// A thin wrapper around XACT3 for playing audio in the Flight sample.  Special purpose
// code adjusts XACT variables for the engine audio cue.
// 
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#pragma once

#include <xact3.h>

class AudioEngine
{
protected:
    IXACT3Engine*       m_pEngine;
    BYTE*               m_pWaveBankData;
    IXACT3WaveBank*     m_pWaveBank;
    BYTE*               m_pSoundBankData;
    IXACT3SoundBank*    m_pSoundBank;
    BYTE*               m_pSettingsData;

    XACTINDEX           m_EngineCueIndex;
    IXACT3Cue*          m_pEngineCue;
    XACTVARIABLEINDEX   m_ThrottleVariable;
    XACTVARIABLEINDEX   m_VelocityVariable;

public:
    AudioEngine();
    virtual ~AudioEngine();

    HRESULT Initialize();
    VOID Update();

    VOID PlayCue( const CHAR* strCueName );
    VOID SetEngineParameters( FLOAT fThrottle, FLOAT fVerticalVelocity );

protected:
    VOID LoadContent();
};

extern AudioEngine* g_pAudioEngine;

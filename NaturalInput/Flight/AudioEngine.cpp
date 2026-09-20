//--------------------------------------------------------------------------------------
// File: AudioEngine.cpp
//
// A thin wrapper around XACT3 for playing audio in the Flight sample.  Special purpose
// code adjusts XACT variables for the engine audio cue.
// 
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include "AudioEngine.h"
#include <assert.h>
#include "AtgUtil.h"

AudioEngine* g_pAudioEngine = NULL;

AudioEngine::AudioEngine()
{
    m_pEngine = NULL;
    m_pWaveBank = NULL;
    m_pWaveBankData = NULL;
    m_pSoundBank = NULL;
    m_pSoundBankData = NULL;
    m_pSettingsData = NULL;
}

AudioEngine::~AudioEngine(void)
{
    if( m_pSoundBank != NULL )
    {
        m_pSoundBank->Destroy();
    }
    if( m_pWaveBank != NULL )
    {
        m_pWaveBank->Destroy();
    }
    if( m_pEngine != NULL )
    {
        m_pEngine->Release();
    }
    ATG::UnloadFilePhysicalMemory( m_pWaveBankData );
    ATG::UnloadFilePhysicalMemory( m_pSoundBankData );
    ATG::UnloadFile( m_pSettingsData );
}

HRESULT AudioEngine::Initialize()
{
    // Create our XACT3 engine
    DWORD dwXactFlags = XACT_FLAG_API_DEBUG_MODE;
    HRESULT hr = XACT3CreateEngine( dwXactFlags, &m_pEngine );

    // Load global settings from disk
    DWORD dwSettingsDataSize = 0;
    hr = ATG::LoadFile( "game:\\media\\sounds\\Flight.xgs", (VOID**)&m_pSettingsData, &dwSettingsDataSize );

    // Initialize XACT3 engine with our global settings
    XACT_RUNTIME_PARAMETERS RuntimeParams = { 0 };
    RuntimeParams.pGlobalSettingsBuffer = m_pSettingsData;
    RuntimeParams.globalSettingsBufferSize = dwSettingsDataSize;
    RuntimeParams.lookAheadTime = 250;
    hr = m_pEngine->Initialize( &RuntimeParams );
    if( FAILED(hr) )
    {
        // no audio, just return
        ATG::UnloadFile( m_pSettingsData );
        m_pSettingsData = NULL;
        m_pEngine->Release();
        m_pEngine = NULL;
        return S_OK;
    }

    // Load wave bank from disk
    DWORD dwWaveBankSize = 0;
    hr = ATG::LoadFilePhysicalMemory( "game:\\media\\sounds\\Flight.xwb", (VOID**)&m_pWaveBankData, &dwWaveBankSize );

    // Create wave bank
    hr = m_pEngine->CreateInMemoryWaveBank( m_pWaveBankData, dwWaveBankSize, 0, 0, &m_pWaveBank );

    // Load sound bank from disk
    DWORD dwSoundBankSize = 0;
    hr = ATG::LoadFilePhysicalMemory( "game:\\media\\sounds\\Flight.xsb", (VOID**)&m_pSoundBankData, &dwSoundBankSize );

    // Create sound bank
    hr = m_pEngine->CreateSoundBank( m_pSoundBankData, dwSoundBankSize, 0, 0, &m_pSoundBank );

    // Identify and prepare engine cue
    m_EngineCueIndex = m_pSoundBank->GetCueIndex( "Engine" );
    if( m_EngineCueIndex == XACTINDEX_INVALID )
    {
        return E_FAIL;
    }
    hr =  m_pSoundBank->Play( m_EngineCueIndex, 0, 0, &m_pEngineCue );
    if( m_pEngineCue == NULL )
    {
        return E_FAIL;
    }

    // Extract cue variables from engine cue
    m_ThrottleVariable = m_pEngineCue->GetVariableIndex( "Throttle" );
    if( m_ThrottleVariable == XACTVARIABLEINDEX_INVALID )
    {
        return E_FAIL;
    }
    m_VelocityVariable = m_pEngineCue->GetVariableIndex( "VerticalVelocity" );
    if( m_VelocityVariable == XACTVARIABLEINDEX_INVALID )
    {
        return E_FAIL;
    }

    SetEngineParameters( 0.0f, 0.0f );

    return S_OK;
}

VOID AudioEngine::Update()
{
    if( m_pEngine == NULL )
    {
        return;
    }

    DWORD dwCueState = 0;
    m_pEngineCue->GetState( &dwCueState );
    if( dwCueState != XACT_CUESTATE_PLAYING )
    {
        m_pEngineCue->Destroy();
        m_pEngineCue = NULL;
        HRESULT hr = m_pSoundBank->Play( m_EngineCueIndex, 0, 0, &m_pEngineCue );
        assert( SUCCEEDED(hr) );
        hr;
    }
    m_pEngine->DoWork();
}

VOID AudioEngine::PlayCue( const CHAR* strCueName )
{
    if( m_pEngine == NULL )
    {
        return;
    }

    assert( strCueName != NULL );
    XACTINDEX CueIndex = m_pSoundBank->GetCueIndex( strCueName );
    assert( CueIndex != XACTINDEX_INVALID );
    m_pSoundBank->Play( CueIndex, 0, 0, NULL );
}

VOID AudioEngine::SetEngineParameters( FLOAT fThrottle, FLOAT fVerticalVelocity )
{
    if( m_pEngine == NULL )
    {
        return;
    }

    fThrottle = max( 0.0f, min( 1.0f, fThrottle ) );
    m_pEngineCue->SetVariable( m_ThrottleVariable, fThrottle );

    FLOAT fProcessedVelocity = max( -100.0f, min( 100.0f, fVerticalVelocity ) );
    m_pEngineCue->SetVariable( m_VelocityVariable, fProcessedVelocity );
}

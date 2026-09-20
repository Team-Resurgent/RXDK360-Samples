//--------------------------------------------------------------------------------------
// Benchmark.cpp
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include <assert.h>
#include "Benchmark.h"
#include "SceneViewer2.h"
#include <AtgUtil.h>

const FLOAT g_fExperimentRunTime = 5.0f;

const ExperimentConfiguration g_Experiments[] =
{
    { L"Ubershader No MSAA",            g_fExperimentRunTime,   0,  0,  0 },
    { L"Ubershader 2x MSAA",            g_fExperimentRunTime,   0,  1,  0 },
    { L"Ubershader 4x MSAA",            g_fExperimentRunTime,   0,  2,  0 },
    { L"Deferred No MSAA",              g_fExperimentRunTime,   1,  0,  0 },
    { L"Pass Per Light No MSAA",        g_fExperimentRunTime,   2,  0,  0 },
    { L"Pass Per Light 2x MSAA",        g_fExperimentRunTime,   2,  1,  0 },
    { L"Pass Per Light 4x MSAA",        g_fExperimentRunTime,   2,  2,  0 },
    { L"Ubershader Library No MSAA",    g_fExperimentRunTime,   3,  0,  0 },
    { L"Ubershader Library 2x MSAA",    g_fExperimentRunTime,   3,  1,  0 },
    { L"Ubershader Library 4x MSAA",    g_fExperimentRunTime,   3,  2,  0 },
};

Benchmark::Benchmark() : m_pSceneViewer( NULL ),
                         m_CurrentState( IDLE ),
                         m_fCountdownTimer( 0.0f )
{
}

VOID Benchmark::Initialize( SceneViewer* pSceneViewer )
{
    m_pSceneViewer = pSceneViewer;
}

VOID Benchmark::StartTest()
{
    m_dwExperimentCount = ARRAYSIZE( g_Experiments );
    m_pExperimentConfigurations = g_Experiments;
    m_ResultsVector.clear();
    m_ResultsVector.reserve( m_dwExperimentCount );
    SetSceneViewerBenchmarkSettings();

    m_OriginalConfiguration.dwRenderModeSetting = ( DWORD )m_pSceneViewer->m_RenderMode;
    m_OriginalConfiguration.dwTilingModeSetting = ( DWORD )m_pSceneViewer->m_TilingMode;

    ATG::DebugSpew( "Starting benchmark testing of %d experiments.\n", m_dwExperimentCount );
    SetupExperiment( 0 );
}

VOID Benchmark::SetupExperiment( DWORD dwIndex )
{
    m_dwCurrentExperimentIndex = dwIndex;
    const ExperimentConfiguration& Config = m_pExperimentConfigurations[m_dwCurrentExperimentIndex];
    ATG::DebugSpew( "Setting up experiment %d: \"%S\".\n", m_dwCurrentExperimentIndex + 1, Config.strExperimentName );
    m_CurrentState = EXPERIMENT_SETUP;
    m_fCountdownTimer = 1.0f;
    ExperimentResults Results;
    ZeroMemory( &Results, sizeof( ExperimentResults ) );
    Results.fMaximumFrameTime = -1e10f;
    Results.fMinimumFrameTime = 1e10f;
    m_ResultsVector.push_back( Results );

    assert( m_pSceneViewer != NULL );
    m_pSceneViewer->m_RenderMode = ( SceneViewerRenderMode )Config.dwRenderModeSetting;
    m_pSceneViewer->m_TilingMode = ( SceneViewerTilingMode )Config.dwTilingModeSetting;
}

VOID Benchmark::ResetTest()
{
    if( m_CurrentState == IDLE )
        return;
    if( m_CurrentState == FINISHED )
    {
        m_CurrentState = IDLE;
        return;
    }
    if( m_ResultsVector.size() > 0 )
        m_ResultsVector.pop_back();
    ATG::DebugSpew( "Benchmark testing terminated.\n" );
    m_fCountdownTimer = 0;
    m_CurrentState = FINISHED;
    if( m_ResultsVector.size() == 0 )
        m_CurrentState = IDLE;
    GenerateReport();
    ResetDefaultSceneViewerSettings();
}

VOID Benchmark::Update( FLOAT fDeltaTime )
{
    assert( m_pSceneViewer != NULL );

    if( m_fCountdownTimer > 0 )
    {
        m_fCountdownTimer -= fDeltaTime;
        UpdateCurrentExperiment( fDeltaTime );
        return;
    }
    switch( m_CurrentState )
    {
        case IDLE:
        case FINISHED:
            return;
        case EXPERIMENT_SETUP:
        {
            // start running experiment now
            ATG::DebugSpew( "Running experiment %d.\n", m_dwCurrentExperimentIndex + 1 );
            m_CurrentState = RUNNING;
            m_fCountdownTimer = m_pExperimentConfigurations[ m_dwCurrentExperimentIndex ].fExperimentRuntime;
            return;
        }
        case RUNNING:
        {
            // switch to next experiment
            m_dwCurrentExperimentIndex++;
            if( m_dwCurrentExperimentIndex == m_dwExperimentCount )
            {
                // all experiments are finished, generate results report
                ATG::DebugSpew( "All experiments finished.\n" );
                ResetDefaultSceneViewerSettings();
                GenerateReport();
                m_CurrentState = FINISHED;
                return;
            }
            SetupExperiment( m_dwCurrentExperimentIndex );
            return;
        }
    }
}

VOID Benchmark::UpdateCurrentExperiment( FLOAT fDeltaTime )
{
    if( m_CurrentState != RUNNING )
        return;
    assert( m_ResultsVector.size() > 0 );
    ExperimentResults& CurrentResults = m_ResultsVector.back();
    CurrentResults.fRunningTime += fDeltaTime;
    CurrentResults.dwFrameCount++;
    CurrentResults.fMaximumFrameTime = max( CurrentResults.fMaximumFrameTime, fDeltaTime );
    CurrentResults.fMinimumFrameTime = min( CurrentResults.fMinimumFrameTime, fDeltaTime );
}

VOID Benchmark::SetSceneViewerBenchmarkSettings()
{
    assert( m_pSceneViewer != NULL );
    m_pSceneViewer->m_bDisableAllUI = TRUE;
    m_pSceneViewer->m_SettingsPanel.SetVisible( FALSE );
}

VOID Benchmark::ResetDefaultSceneViewerSettings()
{
    assert( m_pSceneViewer != NULL );
    m_pSceneViewer->m_bDisableAllUI = FALSE;

    m_pSceneViewer->m_RenderMode = ( SceneViewerRenderMode )m_OriginalConfiguration.dwRenderModeSetting;
    m_pSceneViewer->m_TilingMode = ( SceneViewerTilingMode )m_OriginalConfiguration.dwTilingModeSetting;
}

VOID Benchmark::GenerateReportLine( DWORD dwExperimentIndex, WCHAR* strBuffer, DWORD dwBufferSize )
{
    assert( m_pExperimentConfigurations != NULL );
    const ExperimentConfiguration& Config = m_pExperimentConfigurations[dwExperimentIndex];
    const ExperimentResults& Results = m_ResultsVector[dwExperimentIndex];
    FLOAT fAvgFrameTime = Results.fRunningTime / ( FLOAT )Results.dwFrameCount;
    FLOAT fAvgFPS = 1.0f / fAvgFrameTime;
    swprintf_s( strBuffer, dwBufferSize,
                L"%s: %0.3fms (%0.2f fps)\n",
                Config.strExperimentName,
                fAvgFrameTime * 1000.0f,
                fAvgFPS );
}

VOID Benchmark::GenerateReport()
{
    WCHAR strOutput[512];
    DWORD dwResultsCount = min( m_dwExperimentCount, m_ResultsVector.size() );
    for( DWORD i = 0; i < dwResultsCount; ++i )
    {
        GenerateReportLine( i, strOutput, ARRAYSIZE( strOutput ) );
        OutputDebugStringW( strOutput );
    }
}

VOID Benchmark::RenderReport( ATG::Font& FontObject )
{
    if( m_CurrentState != FINISHED )
        return;
    FontObject.SetScaleFactors( 0.8f, 0.8f );
    FLOAT fYPos = 50.0f;
    WCHAR strOutput[100];
    DWORD dwResultsCount = min( m_dwExperimentCount, m_ResultsVector.size() );
    const FLOAT fXPosName = 0;
    const FLOAT fXPosTime = 280.0f;
    const FLOAT fXPosFPS = 400.0f;
    for( DWORD i = 0; i < dwResultsCount; ++i )
    {
        const ExperimentConfiguration& Config = m_pExperimentConfigurations[i];
        const ExperimentResults& Results = m_ResultsVector[i];
        FLOAT fAvgFrameTime = Results.fRunningTime / ( FLOAT )Results.dwFrameCount;
        FLOAT fAvgFPS = 1.0f / fAvgFrameTime;

        FontObject.DrawText( fXPosName, fYPos, 0xFFFFFFFF, Config.strExperimentName, ATGFONT_LEFT );

        swprintf_s( strOutput, L"%0.3f ms", fAvgFrameTime * 1000.0f );
        FontObject.DrawText( fXPosTime, fYPos, 0xFFFFFF80, strOutput, ATGFONT_LEFT );

        swprintf_s( strOutput, L"%0.2f fps", fAvgFPS );
        FontObject.DrawText( fXPosFPS, fYPos, 0xFFFFFFFF, strOutput, ATGFONT_LEFT );

        fYPos += 25.0f;
    }
}

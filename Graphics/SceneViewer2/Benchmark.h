//--------------------------------------------------------------------------------------
// Benchmark.h
//
// This module provides benchmarking services for SceneViewer2.  It can run a series of
// benchmarking experiments and report the results.
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#ifndef BENCHMARK_H
#define BENCHMARK_H

#pragma once

#include <xtl.h>
#include <vector>
#include <AtgFont.h>

class SceneViewer;

struct ExperimentConfiguration
{
    const WCHAR* strExperimentName;
    FLOAT fExperimentRuntime;
    DWORD dwRenderModeSetting;
    DWORD dwTilingModeSetting;
    DWORD dwFlags;
};

struct ExperimentResults
{
    FLOAT fRunningTime;
    DWORD dwFrameCount;
    FLOAT fMinimumFrameTime;
    FLOAT fMaximumFrameTime;
};
typedef std::vector <ExperimentResults> ExperimentResultsVector;

class Benchmark
{
public:
    enum BenchmarkState
    {
        IDLE = 0,
        EXPERIMENT_SETUP,
        RUNNING,
        FINISHED
    };
public:
            Benchmark();

    VOID    Initialize( SceneViewer* pSceneViewer );

    BOOL    IsActive() const
    {
        return m_CurrentState != IDLE;
    }
    BOOL    IsFinished() const
    {
        return m_CurrentState == FINISHED;
    }
    VOID    StartTest();
    VOID    ResetTest();
    const WCHAR* GetResults();

    VOID    Update( FLOAT fDeltaTime );

    VOID    RenderReport( ATG::Font& FontObject );

private:
    VOID    SetupExperiment( DWORD dwIndex );
    VOID    UpdateCurrentExperiment( FLOAT fDeltaTime );
    VOID    SetSceneViewerBenchmarkSettings();
    VOID    ResetDefaultSceneViewerSettings();
    VOID    GenerateReport();
    VOID    GenerateReportLine( DWORD dwExperimentIndex, WCHAR* strBuffer, DWORD dwBufferSize );

private:
    SceneViewer* m_pSceneViewer;
    BenchmarkState m_CurrentState;
    FLOAT m_fCountdownTimer;
    DWORD m_dwCurrentExperimentIndex;
    DWORD m_dwExperimentCount;
    ExperimentResultsVector m_ResultsVector;
    const ExperimentConfiguration* m_pExperimentConfigurations;
    ExperimentConfiguration m_OriginalConfiguration;
};

#endif

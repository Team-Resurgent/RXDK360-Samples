//-----------------------------------------------------------------------------
// File: StopWatch.h
//
// Desc: StopWatch object using QueryPerformanceCounter
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#pragma once



//-----------------------------------------------------------------------------
// Name: class CStopwatch
// Desc: Simple timing object using stopwatch metaphor
//-----------------------------------------------------------------------------
class CStopwatch
{
    FLOAT m_fTimerPeriod;        // seconds per tick (1/Hz)
    LONGLONG m_nStartTick;          // time watch last started/reset
    LONGLONG m_nPrevElapsedTicks;   // time watch was previously running
    BOOL m_bIsRunning;          // TRUE if watch is running

    LONGLONG    GetTicks() const;

public:

                CStopwatch();

    VOID        Start();
    VOID        StartZero();
    VOID        Stop();
    VOID        Reset();

    BOOL        IsRunning() const;
    FLOAT       GetElapsedSeconds() const;
    FLOAT       GetElapsedMilliseconds() const;
};

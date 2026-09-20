//--------------------------------------------------------------------------------------
// File: SkeletonTracking.h
//
// A thin wrapper around the NUI API that converts skeleton data into flight control
// inputs.
// 
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#pragma once

#include <xtl.h>
#include <xnamath.h>
#include <NuiApi.h>

struct FlightStatus
{
    NUI_SKELETON_POSITION_TRACKING_STATE m_eConfidence;
    FLOAT m_fYAxis;
    FLOAT m_fXAxis;
    FLOAT m_fThrottle;

    FLOAT m_fRestingSpineAngle;
    FLOAT m_fYAxisDeadzone;
    FLOAT m_fMaxYAxis;
    FLOAT m_fXAxisDeadzone;
    FLOAT m_fMaxXAxis;

    FLOAT m_fThrottleDeadzone;
    FLOAT m_fMaxThrottle;
};

class SkeletonTracking
{
protected:
    FlightStatus m_FlightStatus;

public:
    SkeletonTracking();

    HRESULT Update( NUI_SKELETON_DATA* pSkeleton );

    const FlightStatus& GetFlightStatus() const { return m_FlightStatus; }

protected:
    VOID RecognizeFlightGestures( NUI_SKELETON_DATA* pSkeleton );
};

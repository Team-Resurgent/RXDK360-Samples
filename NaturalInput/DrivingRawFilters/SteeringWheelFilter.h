//-------------------------------------------------------------------------------------
// SteeringWheelFilter.h
//  
// A natural input filter that interprets joint data in the form of a steering wheel.
//  
// Microsoft XNA Developer Connection
// Copyright (c) Microsoft Corporation. All rights reserved.
//-------------------------------------------------------------------------------------

#pragma once

#ifndef _STEERING_WHEEL_FILTER_H_
#define _STEERING_WHEEL_FILTER_H_

#include <xtl.h>
#include <xnamath.h>
#include <NuiApi.h>

enum ArmsState
{
    ARMS_NEUTRAL,
    ARMS_RIGHT_OVER_LEFT,
    ARMS_LEFT_OVER_RIGHT
};

class SteeringWheelFilter
{
public:
    SteeringWheelFilter();
    virtual ~SteeringWheelFilter();

    VOID Update( float fElapsedTime, const NUI_SKELETON_DATA *pSkeleton );

private:
    BOOL IsHandValid( FXMVECTOR vHand, FXMVECTOR vLowerBack, FXMVECTOR vForward );

    //
    // Gameplay data
    //
    NUI_SKELETON_POSITION_TRACKING_STATE m_eConfidence;
    FLOAT m_fWheelRotation;

    //
    // Filter controls
    //
    FLOAT m_fMinimumRotationAngleDegrees;
    FLOAT m_fMaximumRotationAngleDegrees;
    FLOAT m_fZeroAccelerationDistance;
    FLOAT m_fMinimumHandBackDistance;
    ArmsState m_eArmsState;
    FLOAT m_fDeadZone;

public:
    //
    // Gameplay data accessors
    //
    inline FLOAT GetWheelRotation() const { return m_fWheelRotation; }
    inline NUI_SKELETON_POSITION_TRACKING_STATE GetConfidence() const { return m_eConfidence; }
};

#endif

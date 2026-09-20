//-------------------------------------------------------------------------------------
// AccelerationPedalFilter.h
//  
// A natural input filter that interprets joint data to control an acceleration pedal.
//  
// Microsoft XNA Developer Connection
// Copyright (c) Microsoft Corporation. All rights reserved.
//-------------------------------------------------------------------------------------

#pragma once

#ifndef _ACCELERATION_PEDAL_FILTER_H_
#define _ACCELERATION_PEDAL_FILTER_H_

#include <xtl.h>
#include <xnamath.h>
#include <NuiApi.h>


class AccelerationPedalFilter
{
public:
    AccelerationPedalFilter();
    virtual ~AccelerationPedalFilter();

    VOID Update( float fElapsedTime, const NUI_SKELETON_DATA* pSkeleton );

private:
    // 
    // Last known good values
    //
    XMVECTOR m_LeftLegPosition;
    XMVECTOR m_RightLegPosition;

    //
    // Gameplay data
    //
    NUI_SKELETON_POSITION_TRACKING_STATE m_eConfidence;
    FLOAT m_fAcceleration;
    FLOAT m_fLegDifference;
    FLOAT m_fHeightAboveFloor;

    //
    // Filter controls
    //
    FLOAT m_fLegDifferenceThreshold;
    FLOAT m_fMinimumHeightAboveFloor;

public:
    //
    // Gameplay data accessors
    //
    inline FLOAT GetAcceleration() const { return m_fAcceleration; }
    inline NUI_SKELETON_POSITION_TRACKING_STATE GetConfidence() const { return m_eConfidence; }

};

#endif

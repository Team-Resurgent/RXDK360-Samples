//-------------------------------------------------------------------------------------
// TurboFilter.h
//  
// A natural input filter that interprets joint data to control a turbo boost and e-brake.
//  
// Microsoft XNA Developer Connection
// Copyright (c) Microsoft Corporation. All rights reserved.
//-------------------------------------------------------------------------------------

#pragma once

#ifndef _TURBO_FILTER_H_
#define _TURBO_FILTER_H_

#include <xtl.h>
#include <xnamath.h>
#include <NuiApi.h>


class TurboFilter
{
public:
    TurboFilter();
    virtual ~TurboFilter();

    VOID Update( float fElapsedTime, const NUI_SKELETON_DATA *pSkeleton, const XMFLOAT4 velocities[] );

private:
    VOID Reset();

    // 
    // Last known good values
    //
    enum TURBO_FILTER_STATE
    {
        TURBO_WAITING,
        TURBO_BOOSTING_IN,
        TURBO_BOOSTING,
        TURBO_BOOSTING_OUT,
        TURBO_TOGGLED,
    };
    TURBO_FILTER_STATE m_eState;
    FLOAT m_fLastTrackedVelocity;
    FLOAT m_fElapsedGestureDuration;
    FLOAT m_fElapsedStateDuration;
    BOOL m_bTrackingRightHand; // TRUE = right hand tracked, FALSE = left hand tracked

    //
    // Gameplay data
    //
    NUI_SKELETON_POSITION_TRACKING_STATE m_eConfidence;
    BOOL m_bIsToggled;

    //
    // Filter controls
    //
    FLOAT m_fMinimumBoostingInVelocity;
    FLOAT m_fMinimumBoostingInDuration;
    FLOAT m_fMinimumBoostingOutVelocity;
    FLOAT m_fMinimumBoostingOutDuration;
    FLOAT m_fMaximumGestureDuration;

public:
    //
    // Gameplay data accessors
    //
    inline NUI_SKELETON_POSITION_TRACKING_STATE GetConfidence() const { return m_eConfidence; }
    inline BOOL IsToggled() const { return m_bIsToggled; }
    const WCHAR* GetState() const;

    //
    // Debug data accessors
    //
    inline FLOAT GetLastTrackedVelocity() const { return m_fLastTrackedVelocity; }
};


#endif
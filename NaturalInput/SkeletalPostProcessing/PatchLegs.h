//--------------------------------------------------------------------------------------
// PatchLegs.h
//
// Contains a set of filters designed to improve joint position consistency in the legs
// and hips.
// 
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

#include "PatchCommon.h"
#include "AtgNuiJointFilter.h"

// #define FILTER_DEBUG to 1 in this file to enable extra filter debug spew.
#ifndef FILTER_DEBUG
#define FILTER_DEBUG 0
#endif

//--------------------------------------------------------------------------------------
// Name: class FilterClippedLegs
// Desc: FilterClippedLegs smoothes out leg joint positions when the skeleton is clipped
// by the bottom of the camera FOV.  Inferred joint positions from the skeletal tracker
// can occasionally be noisy or erroneous, based on limited depth image pixels from the
// parts of the legs in view.  This filter applies a lot of smoothing using a double
// exponential filter, letting through just enough leg movement to show a kick or high step.
// Based on the amount of leg that is clipped/inferred, the smoothed data is feathered into the
// output data.
//--------------------------------------------------------------------------------------
class FilterClippedLegs : public ISkeletonFilter
{
protected:
#if FILTER_DEBUG
    FLOAT m_FilterDebug[16];
#endif

    ATG::FilterDoubleExponential m_FilterDoubleExp;
    TimedLerp m_LerpLeftKnee;
    TimedLerp m_LerpLeftAnkle;
    TimedLerp m_LerpLeftFoot;
    TimedLerp m_LerpRightKnee;
    TimedLerp m_LerpRightAnkle;
    TimedLerp m_LerpRightFoot;

public:
    virtual VOID Initialize();

    virtual const WCHAR* GetName() const { return L"Stabilize Clipped Legs"; }

    virtual BOOL FilterSkeleton( SkeletonFilterContext* pContext, const NUI_SKELETON_DATA* pInputSkeleton, NUI_SKELETON_DATA* pOutputSkeleton, const FLOAT fDeltaNuiTime );
    virtual VOID ResetFilterState();

    virtual VOID DebugRenderUI( IFilterDebugRenderer* pDebugRenderer );
};

//--------------------------------------------------------------------------------------
// Name: class FilterHipHeight
// Desc: FilterHipHeight is a filter that attempts to stabilize the vertical position of
// the hips on the skeleton.  It tracks running ratios between the torso length and the
// femur & leg lengths, and when the ratio changes in a short period of time, the hips
// are shifted up or down to counteract the movement.
//--------------------------------------------------------------------------------------
class FilterHipHeight : public ISkeletonFilter
{
protected:
    XMVECTOR m_vFemurToTorsoRatio;
    XMVECTOR m_vLegToTorsoRatio;
    FLOAT m_fAdjustPercentage;
    FLOAT m_fAdjustLength;
    XMVECTOR m_vHipHeight;
    XMVECTOR m_vTorsoLength;
    XMVECTOR m_vFemurLength;
    XMVECTOR m_vHTorsoLength;
    XMVECTOR m_vHFemurLength;
    XMVECTOR m_vLegLength;
    XMVECTOR m_vCurrentLegTorso;
    XMVECTOR m_vCurrentFemurTorso;

public:
    virtual VOID Initialize();

    virtual const WCHAR* GetName() const { return L"Stabilize Hip Height"; }

    virtual BOOL FilterSkeleton( SkeletonFilterContext* pContext, const NUI_SKELETON_DATA* pInputSkeleton, NUI_SKELETON_DATA* pOutputSkeleton, const FLOAT fDeltaNuiTime );
    virtual VOID ResetFilterState();

    virtual VOID DebugRenderUI( IFilterDebugRenderer* pDebugRenderer );
};

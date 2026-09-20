//--------------------------------------------------------------------------------------
// PatchArmsUp.h
//
// Implements a filter that detects when the user has one or both of their arms above 
// their head.  When this condition is detected, the arm(s), shoulder(s), and head are
// procedurally rebuilt in a stable pose.
// 
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

#include "PatchCommon.h"

// #define FILTER_DEBUG to 1 in this file to enable extra filter debug spew.
#ifndef FILTER_DEBUG
#define FILTER_DEBUG 0
#endif

//--------------------------------------------------------------------------------------
// Name: class FilterArmsUp
// Desc: Implements the arms up skeleton filter.
//--------------------------------------------------------------------------------------
class FilterArmsUp : public ISkeletonFilter
{
protected:
    TimedLerp m_PatchingLeftArm;
    TimedLerp m_PatchingRightArm;
#if FILTER_DEBUG
    FLOAT m_fFilterDebug[16];
#endif

public:
    virtual VOID Initialize();

    virtual const WCHAR* GetName() const { return L"Arms Up"; }

    virtual BOOL FilterSkeleton( SkeletonFilterContext* pContext, const NUI_SKELETON_DATA* pInputSkeleton, NUI_SKELETON_DATA* pOutputSkeleton, const FLOAT fDeltaNuiTime );
    virtual VOID ResetFilterState();

    virtual VOID DebugRenderUI( IFilterDebugRenderer* pDebugRenderer );
};


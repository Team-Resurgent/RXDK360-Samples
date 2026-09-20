//--------------------------------------------------------------------------------------
// PatchBaseballSwing.h
//
// Contains a filter that reposition the player's arms to look like the player has adopted 
// a natural hitter pose.
// 
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

#include "PatchCommon.h"
#include "AtgNuiJointFilter.h"

//--------------------------------------------------------------------------------------
// Class: BattingSide
// Dessc: Keeps track of the player's batting side.
//--------------------------------------------------------------------------------------
class BattingSide
{
public:
    BattingSide() :m_bIsBattingRight( TRUE ) {}
    ~BattingSide() {}

    VOID Update( const NUI_SKELETON_DATA* pSkeleton );
    BOOL IsBattingRight() const { return m_bIsBattingRight; }

    VOID Reset() { m_bIsBattingRight = TRUE; }

private:
    BattingSide( const BattingSide& rhs );
    BattingSide& operator =( const BattingSide& rhs );

    BOOL m_bIsBattingRight;
};


//--------------------------------------------------------------------------------------
// Class: Bat
// Dessc: Keeps track of the end points of a baseball bat in the skeletons coordinates 
//        world.
//--------------------------------------------------------------------------------------
class Bat
{
public:
    Bat() { m_vStart = XMVectorZero(); m_vEnd = XMVectorZero(); }
    ~Bat() {}

    VOID Update( const NUI_SKELETON_DATA* pSkeleton );

    inline XMVECTOR Start() const { return m_vStart; }
    inline XMVECTOR End() const { return m_vEnd; }

private:
    Bat( const Bat& bat );
    Bat& operator =( const Bat& bat );

    XMVECTOR m_vStart;
    XMVECTOR m_vEnd;
};


//--------------------------------------------------------------------------------------
// Name: class FilterBaseballSwing
// Desc: Implements the baseball swing skeleton filter.
//--------------------------------------------------------------------------------------
class FilterBaseballSwing : public ISkeletonFilter
{
protected:
    BOOL        m_bActive;
    BattingSide m_battingSide;

public:
    virtual VOID Initialize();

    virtual const WCHAR* GetName() const { return L"Baseball Swing"; }

    virtual BOOL FilterSkeleton( SkeletonFilterContext* pContext, 
                                 const NUI_SKELETON_DATA* pInputSkeleton, 
                                 NUI_SKELETON_DATA* pOutputSkeleton, 
                                 const FLOAT fDeltaNuiTime );

    virtual VOID ResetFilterState();

    virtual VOID DebugRenderUI( IFilterDebugRenderer* pDebugRenderer );
};


//--------------------------------------------------------------------------------------
// Name: class FilterJointSmoothing
// Desc: Implements a joint smoothing filter based on the double exponential joint
//       filter from the ATG framework.
//--------------------------------------------------------------------------------------
class FilterJointSmoothing : public ISkeletonFilter
{
protected:
    ATG::FilterDoubleExponential m_SmoothingFilter;

public:
    virtual VOID Initialize() { m_SmoothingFilter.Init( 0.25f, // fSmoothing
                                                        0.25f, // fCorrection
                                                        0.25f, // fPrediction
                                                        0.03f, // fJitterRadius
                                                        0.05f  // fMaxDeviationRadius
                                                       ); }

    virtual const WCHAR* GetName() const { return L"Joint Smoothing"; }

    virtual BOOL FilterSkeleton( SkeletonFilterContext* pContext, 
                                 const NUI_SKELETON_DATA* pInputSkeleton, 
                                 NUI_SKELETON_DATA* pOutputSkeleton, 
                                 const FLOAT fDeltaNuiTime ) 
            { XMemCpy( pOutputSkeleton, pInputSkeleton, sizeof( NUI_SKELETON_DATA ) );
                       m_SmoothingFilter.Update( pOutputSkeleton ); return TRUE; }

    virtual VOID ResetFilterState() { m_SmoothingFilter.Reset(); }

    virtual VOID DebugRenderUI( IFilterDebugRenderer* pDebugRenderer ) {}
};

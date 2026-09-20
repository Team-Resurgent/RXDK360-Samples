//--------------------------------------------------------------------------------------
// GestureDetectionFilters.h
//
// Gesture detection filters driven by probabilities from detection heuristics
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

#include "GestureDetectionHeuristics.h"
#include <vector>
#include <deque>


//--------------------------------------------------------------------------------------
// Name: class HeightBaseLine
// Desc: Base class for dynamically determining the height base line a joint
//--------------------------------------------------------------------------------------

// Allow 1.0cm deviation during 10 frames at 30Hz (0.33 seconds), i.e. the player will
// have to be standing still for 0.33 seconds to aquire a good base line
#define HEIGHT_BASE_LINE_RING_BUFFER_SIZE       10
#define HEIGHT_BASE_LINE_DEVIATION_THRESHOLD    0.01f

class HeightBaseLine
{
public:
    HeightBaseLine() { Reset(); }

    HRESULT Initialize( const NUI_SKELETON_POSITION_INDEX eJoint );
    VOID Reset();
    VOID Update( NUI_SKELETON_FRAME* pSkeletonFrame, Heuristic* pHeuristic = NULL );
    VOID OffsetHeight( const UINT uSkeletonIdx, const FLOAT fOffset );

    const FLOAT* GetHeightValues() const { return m_fFinalHeight; }
    FLOAT GetHeight( const UINT uSkeletonIdx ) const { return m_fFinalHeight[ uSkeletonIdx ]; }

protected:
    virtual VOID UpdateFunction( const UINT uSkeletonIdx, const FLOAT fValue ) = 0;

    FLOAT                       m_fFinalHeight[ NUI_SKELETON_COUNT ];
    FLOAT                       m_fOriginalHeight[ NUI_SKELETON_COUNT ];
    std::deque<FLOAT>           m_RingBuffer;
    NUI_SKELETON_POSITION_INDEX m_eJoint;
};


//--------------------------------------------------------------------------------------
// Name: class HeightBaseLine2
// Desc: Base class for dynamically determining the height base line of 2 joints
//--------------------------------------------------------------------------------------

class HeightBaseLine2
{
public:
    HeightBaseLine2();

    HRESULT Initialize( HeightBaseLine* pHeightBaseLineLeft, HeightBaseLine* pHeightBaseLineRight );
    VOID Reset();
    VOID Update( NUI_SKELETON_FRAME* pSkeletonFrame, Heuristic* pHeuristic = NULL );

    const FLOAT* GetHeightValues() const { return m_fHeight; }
    FLOAT GetHeight( const UINT uSkeletonIdx ) const { return m_fHeight[ uSkeletonIdx ]; }

protected:
    FLOAT               m_fHeight[ NUI_SKELETON_COUNT ];
    HeightBaseLine*    m_pHeightBaseLineLeft;
    HeightBaseLine*    m_pHeightBaseLineRight;
};


//--------------------------------------------------------------------------------------
// Name: class HeightBaseLineUsingMax
// Desc: class for dynamically determining the height base line using max()
//--------------------------------------------------------------------------------------

class HeightBaseLineUsingMax : public HeightBaseLine
{
protected:
    virtual VOID UpdateFunction( const UINT uSkeletonIdx, const FLOAT fValue )
    {
        m_fOriginalHeight[ uSkeletonIdx ] = max( m_fOriginalHeight[ uSkeletonIdx ], fValue );
    }
};


//--------------------------------------------------------------------------------------
// Name: class HeightBaseLineUsingMin
// Desc: class for dynamically determining the height base line using min()
//--------------------------------------------------------------------------------------

class HeightBaseLineUsingMin : public HeightBaseLine
{
protected:
    virtual VOID UpdateFunction( const UINT uSkeletonIdx, const FLOAT fValue )
    {
        m_fOriginalHeight[ uSkeletonIdx ] = min( m_fOriginalHeight[ uSkeletonIdx ], fValue );
    }
};


//--------------------------------------------------------------------------------------
// Name: class HeightBaseLineUsingMinWithAngleRestriction
// Desc: class for dynamically determining the height base line using min(), but
//       only if the there is a small angle between the bones formed by this middle joint
//--------------------------------------------------------------------------------------

class HeightBaseLineUsingMinWithAngleRestriction : public HeightBaseLineUsingMin
{
public:
    HeightBaseLineUsingMinWithAngleRestriction();

    HRESULT Initialize( const NUI_SKELETON_POSITION_INDEX eJoint,
                        const NUI_SKELETON_FRAME* pSkeletonFrame,
                        const NUI_SKELETON_POSITION_INDEX eBoneJoint1,
                        const NUI_SKELETON_POSITION_INDEX eBoneJointCenter,
                        const NUI_SKELETON_POSITION_INDEX eBoneJoint2 );

protected:
    virtual VOID UpdateFunction( const UINT uSkeletonIdx, const FLOAT fValue );

    const NUI_SKELETON_FRAME*   m_pSkeletonFrame;
    NUI_SKELETON_POSITION_INDEX m_eBoneJoint1;
    NUI_SKELETON_POSITION_INDEX m_eBoneJointCenter;
    NUI_SKELETON_POSITION_INDEX m_eBoneJoint2;
};


//--------------------------------------------------------------------------------------
// Name: class GestureDetectionFilter
// Desc: Base class for all gesture detection filters
//--------------------------------------------------------------------------------------

class GestureDetectionFilter
{
private:
    class HeuristicData
    {
    public:
        HeuristicData()
        {
            m_pHeuristic    = NULL;
            m_fWeight       = 0.0f;
        }

        HeuristicData( Heuristic* pHeuristic, const FLOAT fWeight )
        {
            m_pHeuristic    = pHeuristic;
            m_fWeight       = fWeight;
        }   

        Heuristic* m_pHeuristic;
        FLOAT       m_fWeight;
    };

public:
    GestureDetectionFilter();
    ~GestureDetectionFilter();

    HRESULT Initialize() { Reset(); }
    VOID Reset();
    VOID Update( NUI_SKELETON_FRAME* pSkeletonFrame );

    // NOTE: This class will delete the pointers that get added
    VOID AddHeuristic( Heuristic* pHeuristic, const FLOAT fWeight )
    {
        m_HeuristicData.push_back( HeuristicData( pHeuristic, fWeight ) );
    }

    FLOAT GetProbability( const UINT uSkeletonIdx ) const
    {
        return m_fProbability[ uSkeletonIdx ];
    }

    BOOL IsDetected( const UINT uSkeletonIdx, const FLOAT fProbabilityThreshold ) const
    {
        return GetProbability( uSkeletonIdx ) > fProbabilityThreshold;
    }

protected:
    VOID Reset( const UINT uSkeletonIdx );
    FLOAT CalcProbability( const UINT uSkeletonIdx ) const;

    std::vector<HeuristicData> m_HeuristicData;
    FLOAT  m_fProbability[ NUI_SKELETON_COUNT ];
};


//--------------------------------------------------------------------------------------
// Name: class DuckDetectionFilter
// Desc: class for detection when the player ducks
//--------------------------------------------------------------------------------------

class DuckDetectionFilter : public GestureDetectionFilter
{
public:
    DuckDetectionFilter();
    HRESULT Initialize();
    VOID Update( NUI_SKELETON_FRAME* pSkeletonFrame );

protected:
    FLOAT CalcProbability( const UINT uSkeletonIdx ) const;

    HeightBaseLineUsingMax             m_HeadHeightBaseLine;

    // Heuristics for false positives
    BodyFaceUpwards*                   m_pBodyFaceUpwards;
    HeightAboveBaseLine*               m_pHeadHeightAboveBaseLine;

    // Heurustics for true positives
    HeightBelowBaseLine*               m_pHeadHeightBelowBaseLine;
    HeightBelowBaseLine*               m_pHeadHeightFarBelowBaseLine;
    BodyFaceDownwards*                 m_pBodyFaceDownwards;
    UpperBodyAngleTowardsLowerBody*    m_pUpperBodyAngleTowardsLowerBody;
    HandsAboveHead*                    m_pHandsAboveHead;
};


//--------------------------------------------------------------------------------------
// Name: class JumpDetectionFilter
// Desc: class for detection when the player jumps
//--------------------------------------------------------------------------------------

class JumpDetectionFilter : public GestureDetectionFilter
{
public:    
    JumpDetectionFilter();
    HRESULT Initialize( NUI_SKELETON_FRAME* pSkeletonFrame );
    VOID Update( NUI_SKELETON_FRAME* pSkeletonFrame );

protected:
    FLOAT CalcProbability( const UINT uSkeletonIdx ) const;
    VOID OffsetHeadBaseLine();

    HeightBaseLineUsingMax              m_HeadHeightBaseLine;

    HeightBaseLineUsingMin              m_LeftAnkleHeightBaseLine;
    HeightBaseLineUsingMin              m_RightAnkleHeightBaseLine;
    HeightBaseLine2                     m_AnkleHeightBaseLine;

    HeightBaseLineUsingMinWithAngleRestriction m_LeftKneeHeightBaseLine;
    HeightBaseLineUsingMinWithAngleRestriction m_RightKneeHeightBaseLine;
    HeightBaseLine2                     m_KneeHeightBaseLine;

    // Heuristics for false positives
    HeightBelowBaseLine*                m_pHeadHeightBelowBaseLine;
    HeightBelowBaseLine*                m_pLeftAnkleHeightBelowBaseLine;
    HeightBelowBaseLine*                m_pRightAnkleHeightBelowBaseLine;
    HeightBelowBaseLine*                m_pLeftKneeHeightBelowBaseLine;
    HeightBelowBaseLine*                m_pRightKneeHeightBelowBaseLine;
    BodyFaceUpwards*                    m_pBodyFaceUpwards;

    // Heurustics for true positives
    HeightAboveBaseLineCumulativeSum*   m_pHeadHeightAboveBaseLineCumulativeSum;
    HeightAboveBaseLineCumulativeSum*   m_pHeadHeightFarAboveBaseLineCumulativeSum;
    HeightAboveBaseLineCumulativeSum*   m_pLeftKneeHeightAboveBaseLineCumulativeSum;
    HeightAboveBaseLineCumulativeSum*   m_pRightKneeHeightAboveBaseLineCumulativeSum;
    LegsStraightPreviouslyBent*         m_pLegsStraightPreviouslyBent;
    HandsAboveHead*                     m_pHandsAboveHead;

    static const FLOAT                  c_fMaxOffsetDueToArmsAboveHead;
};
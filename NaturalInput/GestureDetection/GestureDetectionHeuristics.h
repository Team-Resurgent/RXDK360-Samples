//--------------------------------------------------------------------------------------
// GestureDetectionHeuristics.h
//
// Heuristics used for gesture detection
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

#include <xtl.h>
#include <nuiapi.h>
#include <deque>


//--------------------------------------------------------------------------------------
// Name: class Heuristic
// Desc: Base class for all gesture detection heuristics
//--------------------------------------------------------------------------------------

class Heuristic
{
public:
    enum EType
    {
        TYPE_UNKNOWN,
        TYPE_DETECTION,         // Heuristic to determine true positives
        TYPE_FALSE_DETECTION    // Heuristic to determine false positives
    };

    Heuristic( const EType eType = TYPE_DETECTION );
    
    virtual HRESULT Initialize( const FLOAT fMin, const FLOAT fMax );
    virtual VOID Reset();
    virtual VOID Reset( const UINT uSkeletonIdx );
    virtual VOID Update( NUI_SKELETON_FRAME* pSkeletonFrame ) = 0;
    virtual VOID SetData( const VOID* pData ) {};

    FLOAT GetProbability( const UINT uSkeletonIdx ) const { return m_fProbability[ uSkeletonIdx ]; }
    EType GetType() const { return m_eType; }
    BOOL IsValid( const UINT uSkeletonIdx ) const { return m_bIsValid[ uSkeletonIdx ]; }

    HRESULT SetThresholds( const FLOAT fMin, const FLOAT fMax );

protected:
    inline FLOAT CalcProbability( const FLOAT fValue ) const
    {
        return min( 1.0f, max( 0.0f, fValue - m_fMin ) / ( m_fMax - m_fMin ) );        
    }
    
    EType   m_eType;
    FLOAT   m_fMin;
    FLOAT   m_fMax;
    FLOAT   m_fProbability[ NUI_SKELETON_COUNT ];
    BOOL    m_bIsValid[ NUI_SKELETON_COUNT ];
};


//--------------------------------------------------------------------------------------
// Name: class HandsAboveHead
// Desc: Determines how probable is it that the player's hands are above the head
//--------------------------------------------------------------------------------------

class HandsAboveHead : public Heuristic
{
public:
    HandsAboveHead( const EType eType = TYPE_DETECTION ) : Heuristic( eType ) {};
    virtual VOID Update( NUI_SKELETON_FRAME* pSkeletonFrame );
};


//--------------------------------------------------------------------------------------
// Name: class HeightAboveBaseLine
// Desc: Determines how probable is it that a joint height is above the base line
//--------------------------------------------------------------------------------------

class HeightAboveBaseLine : public Heuristic
{
public:
    HeightAboveBaseLine( const NUI_SKELETON_POSITION_INDEX eJoint, const EType eType = TYPE_DETECTION );
    virtual VOID Update( NUI_SKELETON_FRAME* pSkeletonFrame );
    virtual VOID SetData( const VOID* pData ) { m_pHeightBaseLine = (FLOAT*)pData; }

protected:
     const FLOAT*                   m_pHeightBaseLine;
     NUI_SKELETON_POSITION_INDEX    m_eJoint;
};


//--------------------------------------------------------------------------------------
// Name: class HeightAboveBaseLineCumulativeSum
// Desc: Determines how probable is it that a joint height is above the base line using CUSUM
//--------------------------------------------------------------------------------------

class HeightAboveBaseLineCumulativeSum : public HeightAboveBaseLine
{
public:
    HeightAboveBaseLineCumulativeSum( const NUI_SKELETON_POSITION_INDEX eJoint, const EType eType = TYPE_DETECTION );
    virtual VOID Reset();
    virtual VOID Reset( const UINT uSkeletonIdx );
    virtual VOID Update( NUI_SKELETON_FRAME* pSkeletonFrame );

protected:
    std::deque<FLOAT>   m_HistoryFrames[ NUI_SKELETON_COUNT ];

    static const UINT   c_uRingBufferSize = 5;   // The number of history frames used for CUSUM
};


//--------------------------------------------------------------------------------------
// Name: class HeightBelowBaseLine
// Desc: Determines how probable is it that a joint height is below the base line
//--------------------------------------------------------------------------------------

class HeightBelowBaseLine : public Heuristic
{
public:
    HeightBelowBaseLine( const NUI_SKELETON_POSITION_INDEX eJoint, const EType eType = TYPE_DETECTION );
    virtual VOID Update( NUI_SKELETON_FRAME* pSkeletonFrame );
    virtual VOID SetData( const VOID* pData ) { m_pHeightBaseLine = (FLOAT*)pData; }

protected:
     const FLOAT*                   m_pHeightBaseLine;
     NUI_SKELETON_POSITION_INDEX    m_eJoint;
};


//--------------------------------------------------------------------------------------
// Name: class HeightBelowBaseLineCumulativeSum
// Desc: Determines how probable is it that a joint height is below the base line using CUSUM
//--------------------------------------------------------------------------------------

class HeightBelowBaseLineCumulativeSum : public HeightBelowBaseLine
{
public:
    HeightBelowBaseLineCumulativeSum( const NUI_SKELETON_POSITION_INDEX eJoint, const EType eType = TYPE_DETECTION );
    virtual VOID Reset();
    virtual VOID Reset( const UINT uSkeletonIdx );
    virtual VOID Update( NUI_SKELETON_FRAME* pSkeletonFrame );

protected:
    std::deque<FLOAT>   m_HistoryFrames[ NUI_SKELETON_COUNT ];

    static const UINT   c_uRingBufferSize = 5;   // The number of history frames used for CUSUM
};


//--------------------------------------------------------------------------------------
// Name: class MovingUpwards
// Desc: Determines how probable is it that a joint is moving upwards
//--------------------------------------------------------------------------------------

class MovingUpwards : public Heuristic
{
public:
    MovingUpwards( const NUI_SKELETON_POSITION_INDEX eJoint, const EType eType = TYPE_DETECTION );
    virtual VOID Reset();
    virtual VOID Reset( const UINT uSkeletonIdx );
    virtual VOID Update( NUI_SKELETON_FRAME* pSkeletonFrame );

protected:
    FLOAT m_fPreviousHeight[ NUI_SKELETON_COUNT ];
    NUI_SKELETON_POSITION_INDEX m_eJoint;
    std::deque<FLOAT>           m_PreviousProbability[ NUI_SKELETON_COUNT ];

    static const UINT           m_uNumPreviousProbability = 5;
};


//--------------------------------------------------------------------------------------
// Name: class MovingDownwards
// Desc: Determines how probable is it that a joint is moving downwards
//--------------------------------------------------------------------------------------

class MovingDownwards : public Heuristic
{
public:
    MovingDownwards( const NUI_SKELETON_POSITION_INDEX eJoint, const EType eType = TYPE_DETECTION );
    virtual VOID Reset();
    virtual VOID Reset( const UINT uSkeletonIdx );
    virtual VOID Update( NUI_SKELETON_FRAME* pSkeletonFrame );

protected:
    NUI_SKELETON_POSITION_INDEX m_eJoint;
    FLOAT                       m_fPreviousHeight[ NUI_SKELETON_COUNT ];
    std::deque<FLOAT>           m_PreviousProbability[ NUI_SKELETON_COUNT ];

    static const UINT           m_uNumPreviousProbability = 5;
};


//--------------------------------------------------------------------------------------
// Name: class BodyFaceUpwards
// Desc: Determines how probable is it that the player's body is facing upwards
//--------------------------------------------------------------------------------------

class BodyFaceUpwards : public Heuristic
{
public:
    BodyFaceUpwards( const EType eType = TYPE_DETECTION ) : Heuristic( eType ) {};
    virtual VOID Update( NUI_SKELETON_FRAME* pSkeletonFrame );
};


//--------------------------------------------------------------------------------------
// Name: class BodyFaceDownwards
// Desc: Determines how probable is it that the player's body is facing upwards
//--------------------------------------------------------------------------------------

class BodyFaceDownwards : public Heuristic
{
public:
    BodyFaceDownwards( const EType eType = TYPE_DETECTION ) : Heuristic( eType ) {};
    virtual VOID Update( NUI_SKELETON_FRAME* pSkeletonFrame );
};


//--------------------------------------------------------------------------------------
// Name: class UpperBodyAngleTowardsLowerBody
// Desc: Determines how probable is it that the player's upper body is angling towards lower body
//--------------------------------------------------------------------------------------

class UpperBodyAngleTowardsLowerBody : public Heuristic
{
public:
    virtual VOID Update( NUI_SKELETON_FRAME* pSkeletonFrame );

};


//--------------------------------------------------------------------------------------
// Name: class LegsStraightPreviouslyBent
// Desc: Determines how probable is it that the player's legs are now straing but previously bent
//--------------------------------------------------------------------------------------

class LegsStraightPreviouslyBent : public Heuristic
{
public:
    LegsStraightPreviouslyBent( const EType eType = TYPE_DETECTION );
    virtual VOID Reset();
    virtual VOID Reset( const UINT uSkeletonIdx );
    virtual VOID Update( NUI_SKELETON_FRAME* pSkeletonFrame );

protected:
    std::deque<FLOAT>   m_PreviousFrames[ NUI_SKELETON_COUNT ];
    std::deque<FLOAT>   m_PreviousProbability[ NUI_SKELETON_COUNT ];

    static const UINT   m_uNumPreviousFrames = 3;       // number of previous frames to examine for bending
    static const UINT   m_uNumPreviousProbability = 12; // delay results

    static const FLOAT  c_fBendThreshold;
};

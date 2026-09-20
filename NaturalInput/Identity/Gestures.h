//--------------------------------------------------------------------------------------
// Gestures.h
//
// XNA Developer Connection.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once
#ifndef GESTURES_H
#define GESTURES_H

#include <xtl.h>
#include <NuiApi.h>
#include <AtgUtil.h>


//Note: All gestures included in this file are only provided in suport to the Identity sample.
//      They do not reflect in any way how gestures should behave or be implemented in shipping 
//      titles.


//--------------------------------------------------------------------------------------
// General sample control gestures 
//--------------------------------------------------------------------------------------

enum GESTURE_STATUS { GESTURE_NOTSTARTED, GESTURE_INPROGRESS, GESTURE_COMPLETED };


//--------------------------------------------------------------------------------------
// Name: FlickLeftGesture
// Desc:
//--------------------------------------------------------------------------------------
class FlickLeftGesture
{
public:

    FlickLeftGesture();
    FlickLeftGesture( FLOAT fYRange, FLOAT fXTarget );
    ~FlickLeftGesture() {}

    VOID Reset() { m_eStatus = GESTURE_NOTSTARTED; }
    VOID Update( const NUI_SKELETON_DATA& skeletonData );

    GESTURE_STATUS GetStatus() const { return m_eStatus; }

    FLOAT GetProgress() const;
    VOID GetBounds( XMVECTOR vBounds[ 2 ] ) const;
    const XMVECTOR& GetStartPosition() const { return m_vStartPosition; } 
    const XMVECTOR& GetCurrentPosition() const { return m_vCurrentPosition; } 

private:

    FlickLeftGesture( const FlickLeftGesture& rhs );
    FlickLeftGesture& operator =( const FlickLeftGesture& rhs );

    XMVECTOR m_vStartPosition;
    XMVECTOR m_vCurrentPosition;
    XMVECTOR m_vTargetPosition;

    GESTURE_STATUS m_eStatus;
    FLOAT m_fYRange;
    FLOAT m_fXTarget;
};


//--------------------------------------------------------------------------------------
// Name: FlickRightGesture
// Desc:
//--------------------------------------------------------------------------------------
class FlickRightGesture
{
public:

    FlickRightGesture();
    FlickRightGesture( FLOAT fYRange, FLOAT fXTarget );
    ~FlickRightGesture() {}

    VOID Reset() { m_eStatus = GESTURE_NOTSTARTED; }
    VOID Update( const NUI_SKELETON_DATA& skeletonData );

    GESTURE_STATUS GetStatus() const { return m_eStatus; }

    FLOAT GetProgress() const;
    VOID GetBounds( XMVECTOR vBounds[ 2 ] ) const;
    const XMVECTOR& GetStartPosition() const { return m_vStartPosition; } 
    const XMVECTOR& GetCurrentPosition() const { return m_vCurrentPosition; } 

private:

    FlickRightGesture( const FlickRightGesture& rhs );
    FlickRightGesture& operator =( const FlickRightGesture& rhs );
    XMVECTOR m_vStartPosition;
    XMVECTOR m_vCurrentPosition;
    XMVECTOR m_vTargetPosition;

    GESTURE_STATUS m_eStatus;
    FLOAT m_fYRange;
    FLOAT m_fXTarget;
};


//--------------------------------------------------------------------------------------
// Name: ListSelectionGesture
// Desc:
//--------------------------------------------------------------------------------------
class ListSelectionGesture
{
public:

    ListSelectionGesture();
    explicit ListSelectionGesture( DWORD dwNumItems );
    ListSelectionGesture( DWORD dwNumItems, FLOAT fRange, FLOAT fHoldSeconds, FLOAT fVelocityThreshold );
    ~ListSelectionGesture() {}

    VOID SetNumItems( DWORD dwNumItems );
    VOID Reset() { m_eStatus = GESTURE_NOTSTARTED; }
    VOID Update( const NUI_SKELETON_DATA& skeletonData );

    GESTURE_STATUS GetStatus() const { return m_eStatus; }
    DWORD GetSelection() const;
    DWORD GetCount() const { return m_dwNumItems; }

    FLOAT GetProgress() const;
    VOID GetBounds( XMVECTOR vBounds[ 2 ] ) const;
    VOID GetBoundsForSelection( XMVECTOR vBounds[ 2 ] ) const;
    const XMVECTOR& GetStartPosition() const { return m_vStartPosition; } 
    const XMVECTOR& GetCurrentPosition() const { return m_vCurrentPosition; } 

private:

    ListSelectionGesture( const ListSelectionGesture& rhs );
    ListSelectionGesture& operator =( const ListSelectionGesture& rhs );

    DWORD ComputeSelectionIndex() const;

    GESTURE_STATUS m_eStatus;
    DWORD m_dwNumItems;
    FLOAT m_fRange;
    FLOAT m_fHoldSeconds;
    FLOAT m_fVelocityThreshold;

    ATG::Timer m_Timer;

    FLOAT m_fItemHeight;
    DWORD m_dwSelectionIndex;

    XMVECTOR m_vStartPosition;
    XMVECTOR m_vCurrentPosition;
    XMVECTOR m_vBounds[ 2 ];

    DOUBLE   m_fCurrentTime;
    FLOAT    m_fCurrentVelocity;
};


//--------------------------------------------------------------------------------------
// Name: StopGesture
// Desc:
//--------------------------------------------------------------------------------------
class StopGesture
{
public:

    StopGesture();
    StopGesture( FLOAT fRange, FLOAT fHoldSeconds );
    ~StopGesture() {}

    VOID Reset() { m_eStatus = GESTURE_NOTSTARTED; }
    VOID Update( const NUI_SKELETON_DATA& skeletonData );

    GESTURE_STATUS GetStatus() const { return m_eStatus; }

    FLOAT GetProgress() const;
    VOID GetBoundsLeftHand( XMVECTOR vBounds[ 2 ] ) const;
    VOID GetBoundsRightHand( XMVECTOR vBounds[ 2 ] ) const;
    const XMVECTOR& GetStartPositionLeftHand() const { return m_vStartPositionLeftHand; } 
    const XMVECTOR& GetStartPositionRightHand() const { return m_vStartPositionRightHand; } 
    const XMVECTOR& GetCurrentPositionLeftHand() const { return m_vCurrentPositionLeftHand; } 
    const XMVECTOR& GetCurrentPositionRightHand() const { return m_vCurrentPositionRightHand; } 


private:

    StopGesture( const StopGesture& rhs );
    StopGesture& operator =( const StopGesture& rhs );

    XMVECTOR m_vStartPositionLeftHand;
    XMVECTOR m_vStartPositionRightHand;
    XMVECTOR m_vCurrentPositionLeftHand;
    XMVECTOR m_vCurrentPositionRightHand;

    GESTURE_STATUS m_eStatus;
    FLOAT m_fRange;
    FLOAT m_fHoldSeconds;

    ATG::Timer m_Timer;
};


//--------------------------------------------------------------------------------------
// Name: WaveGesture
// Desc:
//--------------------------------------------------------------------------------------
class WaveGesture
{
public:

    WaveGesture();
    WaveGesture( FLOAT fRange, FLOAT fHoldSeconds );
    ~WaveGesture() {}

    VOID Reset() { m_eStatus = GESTURE_NOTSTARTED; }
    VOID Update( const NUI_SKELETON_DATA& skeletonData );

    GESTURE_STATUS GetStatus() const { return m_eStatus; }

    FLOAT GetProgress() const;
    VOID GetBounds( XMVECTOR vBounds[ 2 ] ) const;
    const XMVECTOR& GetStartPosition() const { return m_vStartPosition; } 
    const XMVECTOR& GetCurrentPosition() const { return m_vCurrentPosition; } 


private:

    WaveGesture( const WaveGesture& rhs );
    WaveGesture& operator =( const WaveGesture& rhs );

    XMVECTOR m_vStartPosition;
    XMVECTOR m_vCurrentPosition;

    GESTURE_STATUS m_eStatus;
    FLOAT m_fRange;
    FLOAT m_fHoldSeconds;

    ATG::Timer m_Timer;
};


//--------------------------------------------------------------------------------------
// Gameplay gestures
//--------------------------------------------------------------------------------------


enum ANGLE_DIRECTION { ANGLE_UNKNOWNDIRECTION, ANGLE_CLOCKWISE, ANGLE_COUNTERCLOCKWISE };


//--------------------------------------------------------------------------------------
// Name: AirCirclesGesture
// Desc:
//--------------------------------------------------------------------------------------
class AirCirclesGesture
{
public:

    AirCirclesGesture();
    AirCirclesGesture( FLOAT fInnerRadius, FLOAT fOuterRadius, DWORD dwMaxFreePass );
    ~AirCirclesGesture() {}

    VOID Reset() { m_eStatus = GESTURE_NOTSTARTED; m_bStarting = FALSE; }
    VOID Update( const NUI_SKELETON_DATA& skeletonData );

    GESTURE_STATUS GetStatus() const { return m_eStatus; }

    FLOAT GetProgress() const;
    VOID GetBounds( XMVECTOR* pvCenter, FLOAT* pfInnerRadius, FLOAT* pfOuterRadius ) const;
    const XMVECTOR& GetStartPosition() const { return m_vStartPosition; } 
    const XMVECTOR& GetCurrentPosition() const { return m_vCurrentPosition; } 

private:

    AirCirclesGesture( const AirCirclesGesture& rhs );
    AirCirclesGesture& operator =( const AirCirclesGesture& rhs );

    XMVECTOR m_vCenter;
    XMVECTOR m_vStartPosition;
    XMVECTOR m_vCurrentPosition;

private:

    GESTURE_STATUS m_eStatus;
    FLOAT m_fInnerRadius;
    FLOAT m_fOuterRadius;

    ANGLE_DIRECTION m_eDirection;
    FLOAT           m_fProgress;

    BOOL  m_bStarting;
    DWORD m_dwFreePass;
    DWORD m_dwMaxFreePass;
};


# endif // GESTURES_H
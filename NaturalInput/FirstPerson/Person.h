//-------------------------------------------------------------------------------------
// Person.h
//  
// Represents the first person camera the user is controlling in this sample.
//  
// Microsoft XNA Developer Connection
// Copyright (c) Microsoft Corporation. All rights reserved.
//-------------------------------------------------------------------------------------

#pragma once

#ifndef _PERSON_H_
#define _PERSON_H_

#include <xtl.h>
#include <xnamath.h>
#include <nuiapi.h>
#include <AtgNuiJointFilter.h>

class Person
{
public:
    Person();
    virtual ~Person();

    VOID Update( FLOAT fElapsedTime );
    VOID UpdateNUI ( const NUI_SKELETON_DATA* pSkeletonData );

private:

    // See Person.cpp for descriptions
    XMVECTOR m_vSpineMin;
    XMVECTOR m_vSpineMax;
    ATG::FilterDoubleExponential m_JointFilter;
    XMFLOAT3 m_vPosition;
    XMFLOAT3 m_vFacing;
    FLOAT m_fForwardSpeed; 
    FLOAT m_fStrafeSpeed; 
    FLOAT m_fDeadZoneForwardBackward;
    FLOAT m_fDeadZoneTurn;
    FLOAT m_fTurnAngleLeftRightRealTime;
    FLOAT m_fTurnAngleLeftRightResidual;
    FLOAT m_fTurnAngleUpDown;
    FLOAT m_fTurnDampening;
    FLOAT m_fSpeedMultiplier;
    FLOAT m_fHandHeightToEngageLeftRightTurning;
    FLOAT m_fRightHandMin;
    DWORD m_dwWheelColor;
    BOOL m_bHandOnWheel;
    FLOAT m_fStrafeSpeedDeadZone;
    FLOAT m_fSpineOffsetX;

public:
    inline XMFLOAT3 GetPosition() { return m_vPosition; }
    inline XMFLOAT3 GetFacing() { return m_vFacing; }
    inline FLOAT GetForwardSpeed() { return m_fForwardSpeed; }
    inline FLOAT GetStrafeSpeed() { return m_fStrafeSpeed; }
    inline DWORD GetWheelColor(){ return m_dwWheelColor; }
    inline BOOL GetHandOnWheel() { return m_bHandOnWheel; }
};

#endif

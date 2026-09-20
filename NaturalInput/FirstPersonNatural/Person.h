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
    XMFLOAT3 m_vPosition;
    XMFLOAT3 m_vFacing;
    FLOAT m_fForwardSpeed; 
    FLOAT m_fStrafeSpeed; 
    ATG::FilterDoubleExponential m_JointFilter;

    FLOAT m_fDeadZoneForwardBackward;
    FLOAT m_fDeadZoneTurn;
    FLOAT m_fTurnAngleLeftRightDirection;
    FLOAT m_fTurnAnglePlayerFacingDirection;
    FLOAT m_fTurnAngleUpDown;
    FLOAT m_fTurnDampening;
    FLOAT m_fSpeedMultiplier;
    FLOAT m_fStrafeSpeedDeadZone;
    FLOAT m_fStrafeSpeedMultiplier;
    
    FLOAT m_fHeight;

public:
    inline XMFLOAT3 GetPosition() { return m_vPosition; }
    inline XMFLOAT3 GetFacing() { return m_vFacing; }
    inline FLOAT GetForwardSpeed() { return m_fForwardSpeed; }
    inline FLOAT GetStrafeSpeed() { return m_fStrafeSpeed; }
    inline FLOAT GetTurnAnglePlayerFacingDirection () { return m_fTurnAnglePlayerFacingDirection; }
};

#endif

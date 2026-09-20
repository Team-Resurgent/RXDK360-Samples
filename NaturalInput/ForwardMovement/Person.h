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

    VOID UpdateNUI ( const NUI_SKELETON_DATA* pSkeletonFrame );
private:
    // comments in cpp file
    ATG::FilterDoubleExponential m_FilterDouble;
    XMFLOAT3 m_fPosition;
    XMFLOAT3 m_fLastPosition;

    FLOAT m_fShiftySpeed;
    FLOAT m_fJumpySpeed;
    BOOL m_bReset;
    FLOAT m_fTurnDirection;


public:
    FLOAT GetSpeed() { return max( 0.001f, max( m_fJumpySpeed, m_fShiftySpeed ) ); };
    XMFLOAT3 GetPosition() { return m_fPosition; };
    XMFLOAT3 GetLastPosition() { return m_fLastPosition; };
    FLOAT GetTurnDirection( ) { return m_fTurnDirection; };
    VOID Reset()
    {
        m_fShiftySpeed = 0.0f;
        m_fJumpySpeed = 0.0f;
        m_bReset = TRUE;
        m_fTurnDirection = 0.0f;
        m_fPosition = XMFLOAT3( 0.0f, 4.0f, 0.0f );
        m_fLastPosition = m_fPosition;            
    };
};

#endif

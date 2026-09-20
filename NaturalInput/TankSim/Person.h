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
#include "BodyRelativeCoordinate.h"


class Environment;
//--------------------------------------------------------------------------------------
// Tracks Velocity and Accerlationfor a value
//--------------------------------------------------------------------------------------
class ScalerAccelerationTracker
{
public:
    ScalerAccelerationTracker() 
    {
        Clear();
    };
    ~ScalerAccelerationTracker() {};
    VOID Clear()
    {
        // Initialize values to max
        m_fAcceleration = 0.0f;
        m_fLastThreePositions[0] = FLT_MAX;
        m_fLastThreePositions[1] = FLT_MAX;
        m_fLastThreePositions[2] = FLT_MAX;

        m_fLastTwoVelocities[0] = 0.0f;
        m_fLastTwoVelocities[1] = 0.0f;
    }
    VOID Update( FLOAT fPosition )
    {
        // Rotate the three values
        m_fLastThreePositions[2] =  m_fLastThreePositions[1];
        m_fLastThreePositions[1] =  m_fLastThreePositions[0];
        m_fLastThreePositions[0] = fPosition;
        // If the postiion have been good data
        if ( m_fLastThreePositions[1] != FLT_MAX && m_fLastThreePositions[2] != FLT_MAX)
        {
            // set velocities and acceleration   
            m_fLastTwoVelocities[0] =   m_fLastThreePositions[0] - m_fLastThreePositions[1];                  
            m_fLastTwoVelocities[1] =   m_fLastThreePositions[1] - m_fLastThreePositions[2];
            m_fAcceleration = m_fLastTwoVelocities[0] - m_fLastTwoVelocities[1];
        }        
    };
    FLOAT m_fLastThreePositions[3];
    FLOAT m_fLastTwoVelocities[2];
    FLOAT m_fAcceleration;
};
//--------------------------------------------------------------------------------------
// Handles the physics of the tank
//--------------------------------------------------------------------------------------
class Tank
{
public:
    Tank():m_fVelocity( 0.0f ),
           m_fFriction( 0.0001f ),
           m_fAngularVelocity ( 0.0f ),
           m_fFacingAngle( 0.0f )
    {
        m_vFacingDirection = XMVectorSet( 0.0f, 0.0f, 1.0f, 0.0f );
        m_vPosition = XMVectorSet( 50.0f, 30.0f, 50.0f, 0.0f );
    };
    
    ~Tank(){};
    // see CPP file for comments
    VOID Update( FLOAT fLeftTread, FLOAT fRightTread );
    VOID SetEnvironmentPointer( Environment* pEnvironment );
    XMVECTOR m_vFacingDirection;
    XMVECTOR m_vPosition;
    Environment* m_pEnvironment;
    FLOAT m_fVelocity;
    FLOAT m_fFacingAngle;
    FLOAT m_fAngularVelocity;
    FLOAT m_fFriction;
};

//--------------------------------------------------------------------------------------
// Handles the UI for the throttles
//--------------------------------------------------------------------------------------
class ThrottleControl
{
public:
    
    ThrottleControl( ):
        m_bHandOn( FALSE ),
        m_fThrottleValue( 0.5f ),
        m_fSnapToGearPercent( 0.1f ),
        m_fNeutralDeadZone( 0.1f ),
        m_fThrottleValueWhenHandsStartedDetaching( -1.0f )
    {
        // Initially, there is no history.
        m_fThrottleHistory[0] = -1.0f;    
        m_fThrottleHistory[1] = -1.0f;    
    };
    
    ~ThrottleControl( ) {};

    // See cpp files for comments
    VOID Update( XMFLOAT4 fHand );
    VOID Init( XMFLOAT3 vMin, XMFLOAT3 vMax );
    XMFLOAT3 m_vMin;
    XMFLOAT3 m_vMax;
    FLOAT m_fThrottleValue;
    BOOL m_bHandOn;
    FLOAT m_fRangeZ;
    FLOAT m_fSnapToGearPercent;
    FLOAT m_fNeutralDeadZone;
    ScalerAccelerationTracker m_ScalerTrackerHandY;
    ScalerAccelerationTracker m_ScalerTrackerHandZ;
    FLOAT m_fThrottleValueWhenHandsStartedDetaching;
    FLOAT m_fThrottleHistory[2];

private:
    // helper function. sets the throttle between 0 and 1 or .5 for the dead zone.
    inline VOID UpdateThrottleValue( FLOAT fRelativeHandZ )
    {
        FLOAT fnewThrottle = min( 1.0f, max( 0.0f, ( fRelativeHandZ - m_vMin.z ) / m_fRangeZ ) );
        if ( fabs( fnewThrottle  - 0.5f ) < m_fNeutralDeadZone ) 
        {
            m_fThrottleValue = 0.5f;    
        }
        else 
        {
            m_fThrottleValue = fnewThrottle;
        }
    };

};


class Person
{
public:
    Person();
    virtual ~Person();

    VOID UpdateNUI ( const NUI_SKELETON_DATA* pSkeletonData );
private:
    // comments in cpp file
    XMFLOAT2 m_fvLeftHandScreenSpace;
    XMFLOAT2 m_fvRightHandScreenSpace;
    XMFLOAT2 m_fLeftHandReletive;
    XMFLOAT2 m_fRightHandReletive;
    ThrottleControl m_ThrottleLeft;
    ThrottleControl m_ThrottleRight;
    BodyReletiveCoordinateSystem m_BodyCoord;
    ATG::FilterDoubleExponential m_FilterDouble;

public:
    inline XMVECTOR GetTankPosition() { return m_Tank.m_vPosition; }
    inline XMVECTOR GetTankFacing() { return m_Tank.m_vFacingDirection; }
    inline FLOAT GetRightThrottleValue() { return m_ThrottleRight.m_fThrottleValue; }
    inline FLOAT GetLeftThrottleValue() { return m_ThrottleLeft.m_fThrottleValue; }
    inline BOOL GetRightThrottleHandOn() { return m_ThrottleRight.m_bHandOn; }
    inline BOOL GetLeftThrottleHandOn() { return m_ThrottleLeft.m_bHandOn; }
    inline XMFLOAT2 GetLeftHand() { return m_fvLeftHandScreenSpace; }
    inline XMFLOAT2 GetRightHand() { return m_fvRightHandScreenSpace; }
    Tank m_Tank;
};

#endif

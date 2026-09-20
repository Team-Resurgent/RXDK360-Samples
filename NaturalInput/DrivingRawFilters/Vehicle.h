//-------------------------------------------------------------------------------------
// Vehicle.h
//  
// Represents the vehicle that the user is controlling in this sample.
//  
// Microsoft XNA Developer Connection
// Copyright (c) Microsoft Corporation. All rights reserved.
//-------------------------------------------------------------------------------------

#pragma once

#ifndef _VEHICLE_H_
#define _VEHICLE_H_

#include <xtl.h>
#include <xnamath.h>
#include "AccelerationPedalFilter.h"
#include "SteeringWheelFilter.h"

class Vehicle
{
public:
    Vehicle();
    virtual ~Vehicle();

    HRESULT CreateFilters();
    VOID DestroyFilters();

    VOID Update( FLOAT fElapsedTime );

private:
    FLOAT CalculateTotalAcceleration();

    XMFLOAT3 m_vPosition;
    XMFLOAT3 m_vFacing;
    BOOL m_bUsingPedal;

    FLOAT m_fSpeed;
    FLOAT m_fYaw;

    AccelerationPedalFilter* m_pAccelerationPedalFilter;
    SteeringWheelFilter* m_pSteeringWheelFilter;

public:
    inline XMFLOAT3 GetPosition() { return m_vPosition; }
    inline XMFLOAT3 GetFacing() { return m_vFacing; }
    inline BOOL GetUsingPedal() const { return m_bUsingPedal; }

    inline AccelerationPedalFilter* GetAccelerationPedalFilter() { return m_pAccelerationPedalFilter; }
    inline SteeringWheelFilter* GetSteeringWheelFilter() { return m_pSteeringWheelFilter; }

};

#endif

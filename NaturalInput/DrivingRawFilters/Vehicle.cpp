//-------------------------------------------------------------------------------------
// Vehicle.cpp
//  
// Represents the vehicle that the user is controlling in this sample.
//  
// Microsoft XNA Developer Connection
// Copyright (c) Microsoft Corporation. All rights reserved.
//-------------------------------------------------------------------------------------

#include "Vehicle.h"


const FLOAT MAX_SPEED = 100.0f;
const FLOAT LOST_PLAYER_DECELERATION = 100.0f;
const FLOAT TURBO_ACCELERATION = 200.0f;
const FLOAT ACCELERATION_PEDAL_THRESHOLD = 7.0f;
const FLOAT STEERING_SENSITIVITY = 0.15f;


//--------------------------------------------------------------------------------------
// Constructor
//--------------------------------------------------------------------------------------
Vehicle::Vehicle() : 
    m_vPosition( 0.0f, 3.0f, 5.0f ),
    m_vFacing( 0.0f, 0.0f, 1.0f ),
    m_fYaw( 0.0f ),
    m_fSpeed( 0.0f ),
    m_bUsingPedal( FALSE )
{
}


//--------------------------------------------------------------------------------------
// Destructor that releases the filters.
//--------------------------------------------------------------------------------------
Vehicle::~Vehicle() 
{
    DestroyFilters();
}


//--------------------------------------------------------------------------------------
// Create the natural input filters.
//--------------------------------------------------------------------------------------
HRESULT Vehicle::CreateFilters()
{
    m_pAccelerationPedalFilter = new AccelerationPedalFilter();
    if ( m_pAccelerationPedalFilter == NULL )
    {
        return E_OUTOFMEMORY;
    }

    m_pSteeringWheelFilter = new SteeringWheelFilter(); 
    if ( m_pSteeringWheelFilter == NULL )
    {
        return E_OUTOFMEMORY;
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Destroys the natural input filters.
//--------------------------------------------------------------------------------------
VOID Vehicle::DestroyFilters()
{
    if ( m_pAccelerationPedalFilter != NULL )
    {
        delete m_pAccelerationPedalFilter;
        m_pAccelerationPedalFilter = NULL;
    }

    if ( m_pSteeringWheelFilter != NULL )
    {
        delete m_pSteeringWheelFilter;
        m_pSteeringWheelFilter = NULL;
    }

}


//--------------------------------------------------------------------------------------
// Updates the vehicle for the elapsed frame.
//--------------------------------------------------------------------------------------
VOID Vehicle::Update( FLOAT fElapsedTime )
{
    // We need a fully tracked player in order to control the car
    if ( m_pSteeringWheelFilter && m_pSteeringWheelFilter->GetConfidence() == NUI_SKELETON_TRACKED )
    {

        FLOAT fAcceleration = CalculateTotalAcceleration();
        m_fSpeed += fAcceleration * fElapsedTime;

        m_fSpeed = ( m_fSpeed > MAX_SPEED ) ? MAX_SPEED : ( ( m_fSpeed < 0.0f ) ? 0.0f : m_fSpeed );

        // Update the yaw by how much the wheel is turning
        if ( fElapsedTime > 0.0f )
        {
            m_fYaw += m_pSteeringWheelFilter->GetWheelRotation() * m_fSpeed * fElapsedTime * STEERING_SENSITIVITY;
        }
    }
    else
    {
        // The player is lost, so don't change direction and slow down the car.
        m_fSpeed -= fElapsedTime * LOST_PLAYER_DECELERATION;

        // Enforce bounds on the speed
        m_fSpeed = ( m_fSpeed < 0.0f ) ? 0.0f : m_fSpeed;
    }

    // Calculate the current direction the vehicle is facing
    m_vFacing = XMFLOAT3( sinf( m_fYaw ), 0.0f, cosf( m_fYaw ) );

    // Calculate the new vehicle position
    XMVECTOR vPosition = XMLoadFloat3( &m_vPosition );
    XMVECTOR vFacing = XMLoadFloat3( &m_vFacing );
    if ( fElapsedTime > 0.0f )
    {
        vPosition += vFacing * m_fSpeed * fElapsedTime;
    }
    XMStoreFloat3( &m_vPosition, vPosition );
}


//--------------------------------------------------------------------------------------
// Calculates the total current acceleration from all filters that provide this data.
//--------------------------------------------------------------------------------------
FLOAT Vehicle::CalculateTotalAcceleration()
{
    FLOAT fAcceleration = 0.0f;
    m_bUsingPedal = FALSE;
    
    // Add the acceleration from the pedal filter
    if ( m_pAccelerationPedalFilter && 
        ( m_pAccelerationPedalFilter->GetConfidence() == NUI_SKELETON_TRACKED ) )
    {
        fAcceleration += m_pAccelerationPedalFilter->GetAcceleration();
        if ( fabs( m_pAccelerationPedalFilter->GetAcceleration() ) > ACCELERATION_PEDAL_THRESHOLD )
        {
            m_bUsingPedal = TRUE;
        }
    }

    
    return fAcceleration;
}
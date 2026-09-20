//--------------------------------------------------------------------------------------
// BodyReletiveCoordinate.cpp
//
// This class maintains a body reletive coordainte system for NUI Samples
//
// Microsoft Game Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include "BodyRelativeCoordinate.h"

//--------------------------------------------------------------------------------------
// Constructor.  Calls the updates function with default parameters
//--------------------------------------------------------------------------------------
BodyReletiveCoordinateSystem::BodyReletiveCoordinateSystem() : 
    m_fAlignSkeletonToCameraAngle( 0.0f ), // Direction the player is facing
    m_fCoordinateSystemScale( 0.75f, 0.8f, 0.5f ) // Scale the coordiante system once it's been calculated
{
    m_vControlRegionScale = XMVectorSet( 1.0f, 1.0f, 1.0f, 1.0f ); // This variable is used to scale hte control regions based on the skeleton's proportions
    m_vCenter = XMVectorSet( 0.0f, 0.0f, 0.0f, 0.0f ); // The players center of mass
    m_vRightHandReletive = XMVectorSet( 0.0f, 0.0f, 0.0f, 0.0f ); // The right and left hand are the output of the coordinate system
    m_vLeftHandReletive = XMVectorSet( 0.0f, 0.0f, 0.0f, 0.0f );
    // Calls the updates function with default parameters
    SetUpdateRatesZeroToOne(); // Set the default updates rates
}

//--------------------------------------------------------------------------------------
// Destructor
//--------------------------------------------------------------------------------------
BodyReletiveCoordinateSystem::~BodyReletiveCoordinateSystem()
{

}

//--------------------------------------------------------------------------------------
// Initializes the rate that the coordinate system will change as the player rotates and moves around
//--------------------------------------------------------------------------------------
VOID BodyReletiveCoordinateSystem::SetUpdateRatesZeroToOne( 
    FLOAT fCenter, FLOAT fOrientation, FLOAT fScale, FLOAT fCenterByLerpBetweenShoulderAndHip )
{
    // The rate that the center of mass updates at
    fCenter = max( 0.0f, min( 1.0f, fCenter ) );
    // The rate that the orientation updates at
    fOrientation = max( 0.0f, min( 1.0f, fOrientation ) );
    // The rate that the scale updates at
    fScale = max( 0.0f, min( 1.0f, fScale ) );
    // The interpolation for the center of the screen calcualed as an interpolate between shoulders and hips
    fCenterByLerpBetweenShoulderAndHip = max( 0.0f, min( 1.0f, fCenterByLerpBetweenShoulderAndHip ) );

    // Calculate lerp values
    m_fUpdateCenterRate = fCenter;
    m_fUpdateCenterRate2 = 1.0f - fCenter;
    m_fUpdateOrientationRate = fOrientation;
    m_fUpdateOrientationRate2 = 1.0f - fOrientation;
    m_fUpdateScaleRate = fScale;
    m_fUpdateScaleRate2 = 1.0f - fScale;
    m_fCenterByLerpBetweenShoulderAndHip = fCenterByLerpBetweenShoulderAndHip;
    m_fCenterByLerpBetweenShoulderAndHip2 = 1.0f - fCenterByLerpBetweenShoulderAndHip;
}


//--------------------------------------------------------------------------------------
// Adds a new frame of positions, updates the coordiant system, and calculates left and right hand.
//--------------------------------------------------------------------------------------
VOID BodyReletiveCoordinateSystem::Update( const XMVECTOR* pSkeletonPosition )
{
    static XMVECTOR vDiv3 = XMVectorSet( 1.0f/3.0f, 1.0f/3.0f, 1.0f/3.0f, 1.0f/3.0f );
    static XMVECTOR vZeroYW = XMVectorSet( 1.0f, 0.0f, 1.0f, 0.0f );
    static XMVECTOR vFlipZ = XMVectorSet( 1.0f, 1.0f, -1.0f, 1.0f );

    // Load the joints that will be used
    XMVECTOR vShoulderL = pSkeletonPosition[ NUI_SKELETON_POSITION_SHOULDER_LEFT ];
    XMVECTOR vShoulderR = pSkeletonPosition[ NUI_SKELETON_POSITION_SHOULDER_RIGHT ];
    XMVECTOR vShoulderC = pSkeletonPosition[ NUI_SKELETON_POSITION_SHOULDER_CENTER ];
    XMVECTOR vHipL = pSkeletonPosition[ NUI_SKELETON_POSITION_HIP_LEFT ];
    XMVECTOR vHipR = pSkeletonPosition[ NUI_SKELETON_POSITION_HIP_RIGHT ];
    XMVECTOR vHipC = pSkeletonPosition[ NUI_SKELETON_POSITION_HIP_CENTER ];
    XMVECTOR vSpine = pSkeletonPosition[ NUI_SKELETON_POSITION_HIP_CENTER ];
    XMVECTOR vHandR = pSkeletonPosition[ NUI_SKELETON_POSITION_HAND_RIGHT ];
    XMVECTOR vHandL = pSkeletonPosition[ NUI_SKELETON_POSITION_HAND_LEFT ];
    
    // Calculate rotation angle before rotating
    XMVECTOR vShoulderLtoR = vShoulderR - vShoulderL;
    XMVECTOR vHipLtoR = vHipR - vHipL;
    FLOAT fTanShoulderLtoR = atan2f( XMVectorGetZ( vShoulderLtoR ), XMVectorGetX( vShoulderLtoR ) );
    FLOAT fTanvHipLtoR = atan2f( XMVectorGetZ( vHipLtoR ), XMVectorGetX( vHipLtoR ) );

    // Average the facing direction of the shoulder and of the hips to find the orientation
    FLOAT fRotationAngle = ( fTanShoulderLtoR + fTanvHipLtoR ) * 0.5f;

    // Calcaulte the ratation matrix.  We're going to rotate the set of joints about the center of the coordiante system
    XMFLOAT3 fLastKnownCenter;
    XMStoreFloat3( &fLastKnownCenter, m_vCenter );
    XMMATRIX mTransform = XMMatrixTranslation( -fLastKnownCenter.x, -fLastKnownCenter.y, -fLastKnownCenter.z ) *
        XMMatrixRotationY( m_fAlignSkeletonToCameraAngle ) * 
        XMMatrixTranslation( fLastKnownCenter.x, fLastKnownCenter.y, fLastKnownCenter.z );

    // Rotate
    vShoulderL = XMVector4Transform( vShoulderL, mTransform );
    vShoulderR = XMVector4Transform( vShoulderR, mTransform );
    vShoulderC = XMVector4Transform( vShoulderC, mTransform );
    vHipL = XMVector4Transform( vHipL, mTransform );
    vHipR = XMVector4Transform( vHipR, mTransform );
    vHipC = XMVector4Transform( vHipC, mTransform );
    vSpine = XMVector4Transform( vSpine, mTransform );
    vHandR = XMVector4Transform( vHandR, mTransform );
    vHandL = XMVector4Transform( vHandL, mTransform );

    // Calculate the center of mass in Y based on the lerp between the shoulder and the hip
    FLOAT fShoulderY = XMVectorGetY( vShoulderC );
    FLOAT fHipY = XMVectorGetY( vHipC );
    XMVECTOR vYCenterValue = XMVectorSet( 0.0f, 
        ( fShoulderY * m_fCenterByLerpBetweenShoulderAndHip + fHipY * m_fCenterByLerpBetweenShoulderAndHip2 ) 
        , 0.0f, 0.0f );
    // XZ for the center is baed on the average of  Shoulders and hips X and Z.
    XMVECTOR vCenter = vShoulderC;         
    vCenter += vHipC;         
    vCenter += vSpine;    
    vCenter *= vDiv3;
    vCenter *= vZeroYW;
    vCenter += vYCenterValue;

    // The first time through set the coordinate system.  
    static bool first = true;
    if ( first )
    {
        first = false;
        FLOAT fShoulder_fHip = 1.0f;
        if ( fShoulderY - fHipY > 0.0f )
        {
            fShoulder_fHip = fShoulderY - fHipY;         
        }
        FLOAT fControlRegionScaleBodyReletive = 1.0f / (fShoulder_fHip);        
        XMFLOAT4 fControlScale;
        // The scale value is a combination of the calculated body reletive scale and user supplied coordinate system scale
        fControlScale.x = fControlRegionScaleBodyReletive * m_fCoordinateSystemScale.x;
        fControlScale.y = fControlRegionScaleBodyReletive * m_fCoordinateSystemScale.y;
        fControlScale.z = fControlRegionScaleBodyReletive * m_fCoordinateSystemScale.z;
        fControlScale.w = 1.0f;
        m_vControlRegionScale = XMLoadFloat4 ( &fControlScale );  
        m_vCenter = vCenter;
        m_fAlignSkeletonToCameraAngle = fRotationAngle;

    }

    XMVECTOR vCenterChangedTest = vCenter - m_vCenter;
    vCenterChangedTest = XMVector4Dot( vCenterChangedTest, vCenterChangedTest );
    // If the center has moved, then shift the center based on the lerp rate
    // This value has to be small because it's squared
    if ( XMVectorGetX( vCenterChangedTest ) > 0.0002f )
    {
        // Adjust the center based on the lerp rates
        m_vCenter = m_fUpdateCenterRate * m_vCenter + m_fUpdateCenterRate2 * vCenter;
    }
    FLOAT fShoulder_fHip = 1.0f;
    // Make sure we don't divide by zero
    if ( fShoulderY - fHipY > 0.0f )
    {
        fShoulder_fHip = fShoulderY - fHipY;         
    }
    // Calculate center
    FLOAT fControlRegionScaleBodyReletive = 1.0f / (fShoulder_fHip);
    XMFLOAT4 fControlScale;
    // The scale value is a combination of the calculated body reletive scale and user supplied coordinate system scale
    fControlScale.x = fControlRegionScaleBodyReletive * m_fCoordinateSystemScale.x;
    fControlScale.y = fControlRegionScaleBodyReletive * m_fCoordinateSystemScale.y;
    fControlScale.z = fControlRegionScaleBodyReletive * m_fCoordinateSystemScale.z;
    fControlScale.w = 1.0f;

    XMVECTOR vTempControlRegionScale = XMLoadFloat4 ( &fControlScale );
    
    // If the scale has moved 1 millimeter then shift the center based on the lerp rate
    if ( fabsf( fControlScale.x - XMVectorGetX( m_vControlRegionScale ) ) > 0.001f )
    {
        m_vControlRegionScale = m_fUpdateScaleRate * m_vControlRegionScale + m_fUpdateScaleRate2 * vTempControlRegionScale;
    }
    // If the angle has moved then shift the center based on the lerp rate
    if ( fabsf( m_fAlignSkeletonToCameraAngle - fRotationAngle ) > 0.001f )
    {
        m_fAlignSkeletonToCameraAngle = m_fAlignSkeletonToCameraAngle * m_fUpdateOrientationRate 
            + fRotationAngle * m_fUpdateOrientationRate2;
    }

    // transform hands
    m_vLeftHandReletive = vHandL;
    m_vLeftHandReletive -= m_vCenter;
    m_vLeftHandReletive *= vFlipZ;
    m_vLeftHandReletive *= m_vControlRegionScale;

    m_vRightHandReletive = vHandR;
    m_vRightHandReletive -= m_vCenter;
    m_vRightHandReletive *= vFlipZ;

    m_vRightHandReletive *= m_vControlRegionScale;

}
//--------------------------------------------------------------------------------------
// Returns the Right hand in the body reletive coordinate system.
//--------------------------------------------------------------------------------------
XMVECTOR BodyReletiveCoordinateSystem::GetRightHandReletive()
{
    return m_vRightHandReletive;
}

//--------------------------------------------------------------------------------------
// Returns the left hand in the body reletive coordinate system.
//--------------------------------------------------------------------------------------
XMVECTOR BodyReletiveCoordinateSystem::GetLeftHandReletive() 
{
    return m_vLeftHandReletive;    
}

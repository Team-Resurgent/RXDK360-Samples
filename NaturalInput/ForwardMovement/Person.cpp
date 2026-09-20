//-------------------------------------------------------------------------------------
// Person.cpp
//  
// Represents the first person camera the user is controlling in this sample.
//  
// Microsoft XNA Developer Connection
// Copyright (c) Microsoft Corporation. All rights reserved.
//-------------------------------------------------------------------------------------

#include "Person.h"
#include <xnamath.h>
#include "Environment.h"


XMVECTOR g_vUnit = XMVectorSet( 0.0f, 0.0f, 1.0f, 0.0f );
//--------------------------------------------------------------------------------------
// Constructor
//--------------------------------------------------------------------------------------
Person::Person()
{
    Reset();
    m_FilterDouble.Init( 0.5f, 0.8f, 0.5f, 0.05f, 0.04f );
}

static CONST FLOAT g_fSpeedDeclineRate = 0.98f;
static CONST FLOAT g_fMaxSpeed = 0.5f;
static CONST FLOAT g_fShiftySpeedConstant = 0.75f;

//--------------------------------------------------------------------------------------
// Destructor
//--------------------------------------------------------------------------------------
Person::~Person() 
{

}

// global helper variables 
static const XMVECTOR g_vPreserveXZ = XMVectorSet( 1.0f, 0.0f, 1.0f, 0.0f );
static const XMVECTOR g_vWorldBoundMin = XMVectorSet( -FLT_MAX, -FLT_MAX, 1.9, -FLT_MAX );
static const XMVECTOR g_vWorldBoundMax = XMVectorSet( FLT_MAX, FLT_MAX, 3.5, FLT_MAX );

//--------------------------------------------------------------------------------------
// Update joints and pass them into the helper functions
//--------------------------------------------------------------------------------------

VOID Person::UpdateNUI ( CONST NUI_SKELETON_DATA* pSkeletonData )
{
    if( pSkeletonData->eTrackingState == NUI_SKELETON_TRACKED )
    {
        // If the necessary joints are tracked proceed
        if ( 
            pSkeletonData->eSkeletonPositionTrackingState[ NUI_SKELETON_POSITION_HEAD ] == NUI_SKELETON_POSITION_TRACKED
            && pSkeletonData->eSkeletonPositionTrackingState[ NUI_SKELETON_POSITION_SPINE ] == NUI_SKELETON_POSITION_TRACKED
            && pSkeletonData->eSkeletonPositionTrackingState[ NUI_SKELETON_POSITION_HIP_CENTER ] == NUI_SKELETON_POSITION_TRACKED )
        {
            
            XMFLOAT4 fHead; 

            XMVECTOR vShoulderLeft = pSkeletonData->SkeletonPositions[ NUI_SKELETON_POSITION_SHOULDER_LEFT];
            XMVECTOR vShoulderRight = pSkeletonData->SkeletonPositions[ NUI_SKELETON_POSITION_SHOULDER_RIGHT];
            XMFLOAT4 fShoulderLeft;
            XMFLOAT4 fShoulderRight;

            XMStoreFloat4( &fShoulderLeft, vShoulderLeft );
            XMStoreFloat4( &fShoulderRight, vShoulderRight );


            // Initialize
            static FLOAT fSpineY = XMVectorGetY( pSkeletonData->SkeletonPositions[ NUI_SKELETON_POSITION_SPINE] );
            
            // blend each frame 
            fSpineY = 0.1f * XMVectorGetY( pSkeletonData->SkeletonPositions[ NUI_SKELETON_POSITION_SPINE] ) 
                      + 0.9f * fSpineY;

            FLOAT fHipCenterX = XMVectorGetX( pSkeletonData->SkeletonPositions[ NUI_SKELETON_POSITION_HIP_CENTER] );
            XMStoreFloat4( &fHead, pSkeletonData->SkeletonPositions[ NUI_SKELETON_POSITION_HEAD] );
            static FLOAT fLastHead = max( 0.0f, fHead.y - fLastHead );
            FLOAT fHeadDiff = max( 0.0f, fHead.y - fLastHead );

            fLastHead = fHead.y; 

            static FLOAT fLastDiff = fHead.x - fHipCenterX;
            static FLOAT fDiff =  fHead.x - fHipCenterX;
            
            fLastDiff = fDiff;
            fDiff = fHead.x - fHipCenterX;

            m_fTurnDirection = ( fShoulderLeft.z - fShoulderRight.z ) * 0.1f;

            if ( !m_bReset )
            {
                m_fShiftySpeed = min( m_fShiftySpeed + ( fabsf( fLastDiff - fDiff ) * g_fShiftySpeedConstant ), g_fMaxSpeed );
                m_fJumpySpeed = min( m_fJumpySpeed + fHeadDiff, g_fMaxSpeed );
            }
            else 
            {
                m_bReset = FALSE;
            }
            m_fJumpySpeed *= g_fSpeedDeclineRate;
            m_fShiftySpeed *= g_fSpeedDeclineRate;

            m_fLastPosition = m_fPosition;
            m_fPosition.x = fHead.x*6.0f;
            m_fPosition.y = ( fHead.y - fSpineY ) * 10.0f;
            m_fPosition.z = - fHead.z * 2.0f + 2.0f;
            // Check for weight change
        }
        else 
        {
            m_fShiftySpeed *= g_fSpeedDeclineRate;
            m_fJumpySpeed *= g_fSpeedDeclineRate;
            m_bReset = TRUE;
        }

    }
    else
    {
        m_FilterDouble.Reset();
    }
}




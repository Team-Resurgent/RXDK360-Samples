//--------------------------------------------------------------------------------------
// ClassifierData.cpp
//
// This file defines all the features and classifier data that are used during the
// training process on the PC and the gesture detection and runtime on the Xbox 360.
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "ClassifierData.h"
#include <float.h>
#include <assert.h>

namespace ATGGestureDetector
{

//--------------------------------------------------------------------------------------
// Defines, constants and statics
//--------------------------------------------------------------------------------------

static const FLOAT g_fInvalidValue  = -FLT_MAX;

RingBuffer ClassifierData::m_SkeletonDataHistory[ NUM_PLAYERS ];
MUSCLE_FRAME ClassifierData::m_Muscles[ NUM_PLAYERS ];


//--------------------------------------------------------------------------------------
// Name: ClassifierData
// Desc: Constructor
//--------------------------------------------------------------------------------------

ClassifierData::ClassifierData()
{
    for ( UINT i = 0; i < NUM_PLAYERS; i++ )
    {
        m_fValue[ i ]   = g_fInvalidValue;
    }
    m_uID               = 0;
    m_bRejectInferred   = FALSE;
    m_Type              = NUM_FEATURES;
}


//--------------------------------------------------------------------------------------
// Name: Read
// Desc: Read data
//--------------------------------------------------------------------------------------

HRESULT ClassifierData::Read( FILE* pFile )
{
    fread( &m_uID, sizeof( m_uID ), 1, pFile );
    RETURN_ON_FILE_ERROR( pFile );

#ifndef _XBOX
    m_uID = ByteSwap32Bit( m_uID );
#endif

    fread( &m_bRejectInferred, sizeof( m_bRejectInferred ), 1, pFile );
    RETURN_ON_FILE_ERROR( pFile );

#ifndef _XBOX
    m_bRejectInferred = ByteSwap32Bit( m_bRejectInferred );
#endif
    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Read
// Desc: Read data
//--------------------------------------------------------------------------------------

HRESULT ClassifierData::Read( VOID* pBuffer )
{
    mread( &m_uID, sizeof( m_uID ), 1, pBuffer );

#ifndef _XBOX
    m_uID = ByteSwap32Bit( m_uID );
#endif

    mread( &m_bRejectInferred, sizeof( m_bRejectInferred ), 1, pBuffer );

#ifndef _XBOX
    m_bRejectInferred = ByteSwap32Bit( m_bRejectInferred );
#endif
    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Write
// Desc: Write data
//--------------------------------------------------------------------------------------

HRESULT ClassifierData::Write( FILE* pFile )
{
    UINT uBigEndianValue = ByteSwap32Bit( (UINT)m_Type );
    fwrite( &uBigEndianValue, sizeof( uBigEndianValue ), 1, pFile );
    RETURN_ON_FILE_ERROR( pFile );

    uBigEndianValue = ByteSwap32Bit( m_uID );
    fwrite( &uBigEndianValue, sizeof( uBigEndianValue ), 1, pFile );
    RETURN_ON_FILE_ERROR( pFile );

    uBigEndianValue = ByteSwap32Bit( m_bRejectInferred );
    fwrite( &uBigEndianValue, sizeof( uBigEndianValue ), 1, pFile );
    RETURN_ON_FILE_ERROR( pFile );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Copy
// Desc: Copy data from a source
//--------------------------------------------------------------------------------------

VOID ClassifierData::Copy( ClassifierData* pSource )
{
    for ( UINT i = 0; i < NUM_PLAYERS; i++ )
    {
        m_fValue[ i ] = pSource->m_fValue[ i ];
    }
    m_bRejectInferred = pSource->m_bRejectInferred;
}


//--------------------------------------------------------------------------------------
// Name: Initialize
// Desc: Initialize data
//--------------------------------------------------------------------------------------

VOID ClassifierData::Initialize()
{  
    for ( UINT i = 0; i < NUM_PLAYERS; i++ )
    {
#ifdef _XBOX
        XMemSet( &m_Muscles[ i ], 0, sizeof( MUSCLE_FRAME ) );
#else
        memset( &m_Muscles[ i ], 0, sizeof( MUSCLE_FRAME ) );
#endif
        m_SkeletonDataHistory[ i ].Clear();
    }
}


//--------------------------------------------------------------------------------------
// Name: CreatNewInstance
// Desc: Read the type from file and create a new instance based on that type
//--------------------------------------------------------------------------------------

ClassifierData* ClassifierData::CreatNewInstance( FILE* pFile )
{
    UINT uValue;
    fread( &uValue, sizeof( uValue ), 1, pFile );
    if ( ferror( pFile ) )
    {
        return NULL;
    }

#ifndef _XBOX
    uValue = ByteSwap32Bit( uValue );
#endif

    return CreatNewInstance( uValue );
}


//--------------------------------------------------------------------------------------
// Name: CreatNewInstance
// Desc: Read the type from file and create a new instance based on that type
//--------------------------------------------------------------------------------------

ClassifierData* ClassifierData::CreatNewInstance( VOID* pBuffer )
{
    UINT uValue;
    mread( &uValue, sizeof( uValue ), 1, pBuffer );

#ifndef _XBOX
    uValue = ByteSwap32Bit( uValue );
#endif

    return CreatNewInstance( uValue );
}


//--------------------------------------------------------------------------------------
// Name: CreatNewInstance
// Desc: Read the type from file and create a new instance based on that type
//--------------------------------------------------------------------------------------

ClassifierData* ClassifierData::CreatNewInstance( const UINT uValue )
{
    EType type = (EType)uValue;

    switch( type )
    {
#ifdef ADD_TYPE_DIFF_POSITION_X
        case TYPE_DIFF_POSITION_X:
            return new ClassifierDataUsingDiffPositionX;
#endif

#ifdef ADD_TYPE_DIFF_POSITION_Y
        case TYPE_DIFF_POSITION_Y:
            return new ClassifierDataUsingDiffPositionY;
#endif

#ifdef ADD_TYPE_DIFF_POSITION_Z
        case TYPE_DIFF_POSITION_Z:
            return new ClassifierDataUsingDiffPositionZ;
#endif

#ifdef ADD_TYPE_ANGLE
        case TYPE_ANGLE:
            return new ClassifierDataUsingAngles;
#endif

#ifdef ADD_TYPE_TIME_SPACE_ANGLE
        case TYPE_TIME_SPACE_ANGLE:
            return new ClassifierDataUsingTimeSpaceAngles;
#endif

#ifdef ADD_TYPE_POSITION_SPEED
        case TYPE_POSITION_SPEED:
            return new ClassifierDataUsingPositionSpeed;
#endif

#ifdef ADD_TYPE_POSITION_SPEED_SQ
        case TYPE_POSITION_SPEED_SQ:
            return new ClassifierDataUsingPositionSpeedSQ;
#endif

#ifdef ADD_TYPE_POSITION_ACCELERATION
        case TYPE_POSITION_ACCELERATION:
            return new ClassifierDataUsingPositionAcceleration;
#endif

#ifdef ADD_TYPE_POSITION_ACCELERATION_X
        case TYPE_POSITION_ACCELERATION_X:
            return new ClassifierDataUsingPositionAccelerationX;
#endif

#ifdef ADD_TYPE_POSITION_ACCELERATION_Y
        case TYPE_POSITION_ACCELERATION_Y:
            return new ClassifierDataUsingPositionAccelerationY;
#endif

#ifdef ADD_TYPE_POSITION_ACCELERATION_Z
        case TYPE_POSITION_ACCELERATION_Z:
            return new ClassifierDataUsingPositionAccelerationZ;
#endif

#ifdef ADD_TYPE_POSITION_VELOCITY_X
        case TYPE_POSITION_VELOCITY_X:
            return new ClassifierDataUsingPositionVelocityX;
#endif

#ifdef ADD_TYPE_POSITION_VELOCITY_Y
        case TYPE_POSITION_VELOCITY_Y:
            return new ClassifierDataUsingPositionVelocityY;
#endif

#ifdef ADD_TYPE_POSITION_VELOCITY_Z
        case TYPE_POSITION_VELOCITY_Z:
            return new ClassifierDataUsingPositionVelocityZ;
#endif

#ifdef ADD_TYPE_POSITION_VELOCITYSQ_X
        case TYPE_POSITION_VELOCITYSQ_X:
            return new ClassifierDataUsingPositionVelocitySQX;
#endif

#ifdef ADD_TYPE_POSITION_VELOCITYSQ_Y
        case TYPE_POSITION_VELOCITYSQ_Y:
            return new ClassifierDataUsingPositionVelocitySQY;
#endif

#ifdef ADD_TYPE_POSITION_VELOCITYSQ_Z
        case TYPE_POSITION_VELOCITYSQ_Z:
            return new ClassifierDataUsingPositionVelocitySQZ;
#endif

#ifdef ADD_TYPE_ANGLE_VELOCITY
        case TYPE_ANGLE_VELOCITY:
            return new ClassifierDataUsingAngleVelocities;
#endif

#ifdef ADD_TYPE_ANGLE_ACCELERATION
        case TYPE_ANGLE_ACCELERATION:
            return new ClassifierDataUsingAngleAcceleration;
#endif

#ifdef ADD_TYPE_MUSCLE_FORCES
        case TYPE_MUSCLE_FORCE_X:
            return new ClassifierDataUsingMuscleForceX;

        case TYPE_MUSCLE_FORCE_Y:
            return new ClassifierDataUsingMuscleForceY;

        case TYPE_MUSCLE_FORCE_Z:
            return new ClassifierDataUsingMuscleForceZ;
#endif

#ifdef ADD_TYPE_MUSCLE_TORQUES
        case TYPE_MUSCLE_TORQUE_X:
            return new ClassifierDataUsingMuscleTorqueX;

        case TYPE_MUSCLE_TORQUE_Y:
            return new ClassifierDataUsingMuscleTorqueY;

        case TYPE_MUSCLE_TORQUE_Z:
            return new ClassifierDataUsingMuscleTorqueZ;
#endif

#ifdef ADD_TYPE_MUSCLE_POWER
        case TYPE_MUSCLE_POWER:
            return new ClassifierDataUsingMusclePower;
#endif

#ifdef ADD_TYPE_DIFF_MUSCLE_FORCE_X
        case TYPE_DIFF_MUSCLE_FORCE_X:
            return new ClassifierDataUsingDiffMuscleForceX;
#endif

#ifdef ADD_TYPE_DIFF_MUSCLE_FORCE_Y
        case TYPE_DIFF_MUSCLE_FORCE_Y:
            return new ClassifierDataUsingDiffMuscleForceY;
#endif

#ifdef ADD_TYPE_DIFF_MUSCLE_FORCE_Z
        case TYPE_DIFF_MUSCLE_FORCE_Z:
            return new ClassifierDataUsingDiffMuscleForceZ;
#endif

#ifdef ADD_TYPE_BONE_LENGTH_CHANGES
        case TYPE_BONE_LENGTH_CHANGES:
            return new ClassifierDataUsingBoneLengthChanges;
#endif
    }

    return NULL;
}


//--------------------------------------------------------------------------------------
// Name: UpdateHistory
// Desc: Update the sliding window of skeleton frames
//--------------------------------------------------------------------------------------

VOID ClassifierData::UpdateHistory( const UINT uPlayerIdx, const NUI_SKELETON_DATA* pSkeletonData, const FLOAT fDeltaTimeInSeconds, BOOL* bReset )
{
    *bReset = ( fDeltaTimeInSeconds == 0.0f ) ? TRUE : FALSE;

    m_SkeletonDataHistory[ uPlayerIdx ].AddToFront( pSkeletonData );

    // Check that we have the same player, otherwise reset the states
    UINT uCurrentFrameIndex = m_SkeletonDataHistory[ uPlayerIdx ].GetCurrentFrameIndex();
    UINT uPreviousFrameIndex = m_SkeletonDataHistory[ uPlayerIdx ].GetPreviousFrameIndex( uCurrentFrameIndex );
    NUI_SKELETON_DATA* pPreviousSkeletonData = m_SkeletonDataHistory[ uPlayerIdx ].GetAt( uPreviousFrameIndex );
    uPreviousFrameIndex = m_SkeletonDataHistory[ uPlayerIdx ].GetPreviousFrameIndex( uPreviousFrameIndex );
    NUI_SKELETON_DATA* pPreviousSkeletonData2 = m_SkeletonDataHistory[ uPlayerIdx ].GetAt( uPreviousFrameIndex );

    assert( pSkeletonData );
    assert( pPreviousSkeletonData );

    if ( pSkeletonData->dwTrackingID != pPreviousSkeletonData->dwTrackingID ||
         pSkeletonData->eTrackingState != pPreviousSkeletonData->eTrackingState ||
         pPreviousSkeletonData->dwTrackingID != pPreviousSkeletonData2->dwTrackingID ||
         pPreviousSkeletonData->eTrackingState != pPreviousSkeletonData2->eTrackingState )
    {
        m_SkeletonDataHistory[ uPlayerIdx ].Splat( pSkeletonData );
        *bReset = TRUE;
    }

    if ( pSkeletonData->eTrackingState != NUI_SKELETON_TRACKED )
    {
        m_SkeletonDataHistory[ uPlayerIdx ].Splat( pSkeletonData );
#ifdef _XBOX
        XMemSet( &m_Muscles[ uPlayerIdx ], 0, sizeof( MUSCLE_FRAME ) );
#else
        memset( &m_Muscles[ uPlayerIdx ], 0, sizeof( MUSCLE_FRAME ) );
#endif
        return;
    }

    if ( *bReset )
    {
        m_SkeletonDataHistory[ uPlayerIdx ].Splat( pSkeletonData );
#ifdef _XBOX
        XMemSet( &m_Muscles[ uPlayerIdx ], 0, sizeof( MUSCLE_FRAME ) );
#else
        memset( &m_Muscles[ uPlayerIdx ], 0, sizeof( MUSCLE_FRAME ) );
#endif

        MuscleFrameCalculate( &m_Muscles[ uPlayerIdx ], 
                              NULL,
                              pSkeletonData->SkeletonPositions,
                              XMVectorSet( 0.0f, 1.0f, 0.0f, 0.0f ),
                              pSkeletonData->eSkeletonPositionTrackingState,
                              0.033f );

    }
    else
    {
        MuscleFrameCalculate( &m_Muscles[ uPlayerIdx ], 
                              &m_Muscles[ uPlayerIdx ],
                              pSkeletonData->SkeletonPositions,
                              XMVectorSet( 0.0f, 1.0f, 0.0f, 0.0f ),
                              pSkeletonData->eSkeletonPositionTrackingState,
                              fDeltaTimeInSeconds );
    }
}


//--------------------------------------------------------------------------------------
// Name: ClassifierDataBaseClassForOneJoint
// Desc: Constructor
//--------------------------------------------------------------------------------------

ClassifierDataBaseClassForOneJoint::ClassifierDataBaseClassForOneJoint( const NUI_SKELETON_POSITION_INDEX jointIndex ) : ClassifierData()
{
    m_jointIndex = jointIndex;
}


//--------------------------------------------------------------------------------------
// Name: Read
// Desc: Read data
//--------------------------------------------------------------------------------------

HRESULT ClassifierDataBaseClassForOneJoint::Read( FILE* pFile )
{
    RETURN_ON_FAIL( ClassifierData::Read( pFile ) );

    UINT uValue;
    fread( &uValue, sizeof( uValue ), 1, pFile );
    RETURN_ON_FILE_ERROR( pFile );

#ifndef _XBOX
    uValue = ByteSwap32Bit( uValue );
#endif
    m_jointIndex = (NUI_SKELETON_POSITION_INDEX)uValue;

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Read
// Desc: Read data
//--------------------------------------------------------------------------------------

HRESULT ClassifierDataBaseClassForOneJoint::Read( VOID* pBuffer )
{
    RETURN_ON_FAIL( ClassifierData::Read( pBuffer ) );

    UINT uValue;
    mread( &uValue, sizeof( uValue ), 1, pBuffer );

#ifndef _XBOX
    uValue = ByteSwap32Bit( uValue );
#endif
    m_jointIndex = (NUI_SKELETON_POSITION_INDEX)uValue;

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Write
// Desc: Write data
//--------------------------------------------------------------------------------------

HRESULT ClassifierDataBaseClassForOneJoint::Write( FILE* pFile )
{
    RETURN_ON_FAIL( ClassifierData::Write( pFile ) );

    UINT uBigEndianValue;
    uBigEndianValue = ByteSwap32Bit( (UINT)m_jointIndex );
    fwrite( &uBigEndianValue, sizeof( uBigEndianValue ), 1, pFile );
    RETURN_ON_FILE_ERROR( pFile );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: ClassifierDataBaseClassForTwoJoints
// Desc: Constrcutor
//--------------------------------------------------------------------------------------

ClassifierDataBaseClassForTwoJoints::ClassifierDataBaseClassForTwoJoints( const NUI_SKELETON_POSITION_INDEX jointIndex0,
                                                                          const NUI_SKELETON_POSITION_INDEX jointIndex1 ) : ClassifierData()
{
    m_jointIndices[ 0 ] = jointIndex0;
    m_jointIndices[ 1 ] = jointIndex1;
}


//--------------------------------------------------------------------------------------
// Name: Read
// Desc: Read data
//--------------------------------------------------------------------------------------

HRESULT ClassifierDataBaseClassForTwoJoints::Read( FILE* pFile )
{
    RETURN_ON_FAIL( ClassifierData::Read( pFile ) );

    UINT uValue;
    for ( UINT i = 0; i < 2; i++ )
    {
        fread( &uValue, sizeof( uValue ), 1, pFile );
        RETURN_ON_FILE_ERROR( pFile );

#ifndef _XBOX
        uValue = ByteSwap32Bit( uValue );
#endif
        m_jointIndices[ i ] = (NUI_SKELETON_POSITION_INDEX)uValue;
    }
    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Read
// Desc: Read data
//--------------------------------------------------------------------------------------

HRESULT ClassifierDataBaseClassForTwoJoints::Read( VOID* pBuffer )
{
    RETURN_ON_FAIL( ClassifierData::Read( pBuffer ) );

    UINT uValue;
    for ( UINT i = 0; i < 2; i++ )
    {
        mread( &uValue, sizeof( uValue ), 1, pBuffer );

#ifndef _XBOX
        uValue = ByteSwap32Bit( uValue );
#endif
        m_jointIndices[ i ] = (NUI_SKELETON_POSITION_INDEX)uValue;
    }
    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Write
// Desc: Write data
//--------------------------------------------------------------------------------------

HRESULT ClassifierDataBaseClassForTwoJoints::Write( FILE* pFile )
{
    RETURN_ON_FAIL( ClassifierData::Write( pFile ) );

    UINT uBigEndianValue;
    for ( UINT i = 0; i < 2; i++)
    {
        uBigEndianValue = ByteSwap32Bit( (UINT)m_jointIndices[ i ] );            
        fwrite( &uBigEndianValue, sizeof( uBigEndianValue ), 1, pFile );
        RETURN_ON_FILE_ERROR( pFile );
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: ClassifierDataBaseClassForThreeJoints
// Desc: Constructor
//--------------------------------------------------------------------------------------

ClassifierDataBaseClassForThreeJoints::ClassifierDataBaseClassForThreeJoints( const NUI_SKELETON_POSITION_INDEX jointIndex0,
                                                                              const NUI_SKELETON_POSITION_INDEX jointIndex1,
                                                                              const NUI_SKELETON_POSITION_INDEX jointIndex2 ) : ClassifierData()
{
    m_jointIndices[ 0 ] = jointIndex0;
    m_jointIndices[ 1 ] = jointIndex1;
    m_jointIndices[ 2 ] = jointIndex2;
}


//--------------------------------------------------------------------------------------
// Name: Read
// Desc: Read data
//--------------------------------------------------------------------------------------

HRESULT ClassifierDataBaseClassForThreeJoints::Read( FILE* pFile )
{
    RETURN_ON_FAIL( ClassifierData::Read( pFile ) );

    UINT uValue;
    for ( UINT i = 0; i < 3; i++ )
    {
        fread( &uValue, sizeof( uValue ), 1, pFile );
        RETURN_ON_FILE_ERROR( pFile );

#ifndef _XBOX
        uValue = ByteSwap32Bit( uValue );
#endif
        m_jointIndices[ i ] = (NUI_SKELETON_POSITION_INDEX)uValue;
    }
    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Read
// Desc: Read data
//--------------------------------------------------------------------------------------

HRESULT ClassifierDataBaseClassForThreeJoints::Read( VOID* pBuffer )
{
    RETURN_ON_FAIL( ClassifierData::Read( pBuffer ) );

    UINT uValue;
    for ( UINT i = 0; i < 3; i++ )
    {
        mread( &uValue, sizeof( uValue ), 1, pBuffer );

#ifndef _XBOX
        uValue = ByteSwap32Bit( uValue );
#endif
        m_jointIndices[ i ] = (NUI_SKELETON_POSITION_INDEX)uValue;
    }
    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Write
// Desc: Write data
//--------------------------------------------------------------------------------------

HRESULT ClassifierDataBaseClassForThreeJoints::Write( FILE* pFile )
{
    RETURN_ON_FAIL( ClassifierData::Write( pFile ) );

    UINT uBigEndianValue;
    for ( UINT i = 0; i < 3; i++)
    {
        uBigEndianValue = ByteSwap32Bit( (UINT)m_jointIndices[ i ] );            
        fwrite( &uBigEndianValue, sizeof( uBigEndianValue ), 1, pFile );
        RETURN_ON_FILE_ERROR( pFile );
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Update
// Desc: Update data
//--------------------------------------------------------------------------------------

VOID ClassifierDataUsingDiffPositionX::Update( const UINT uPlayerIdx, VOID* )
{
    // This classifier type operates only on the current frame
    NUI_SKELETON_DATA* pSkeletonData = GetCurrentSkeleton( uPlayerIdx );
    assert( pSkeletonData );

    if ( pSkeletonData->eTrackingState != NUI_SKELETON_TRACKED )
    {
        m_fValue[ uPlayerIdx ] = g_fInvalidValue;
        return;
    }

    // Get the tracking states for joints
    NUI_SKELETON_POSITION_TRACKING_STATE jointTrackingState0 = pSkeletonData->eSkeletonPositionTrackingState[ m_jointIndices[ 0 ] ];
    NUI_SKELETON_POSITION_TRACKING_STATE jointTrackingState1 = pSkeletonData->eSkeletonPositionTrackingState[ m_jointIndices[ 1 ] ];

    if ( m_bRejectInferred )
    {
        if ( jointTrackingState0 != NUI_SKELETON_POSITION_TRACKED ||
             jointTrackingState1 != NUI_SKELETON_POSITION_TRACKED )
        {
            m_fValue[ uPlayerIdx ] = g_fInvalidValue;
            return;
        }
    }
    else
    {
        if ( jointTrackingState0 == NUI_SKELETON_POSITION_NOT_TRACKED ||
             jointTrackingState1 == NUI_SKELETON_POSITION_NOT_TRACKED )
        {
            m_fValue[ uPlayerIdx ] = g_fInvalidValue;
            return;
        }
    }

    // Get the joint positions
    const XMVECTOR vJoint0 = pSkeletonData->SkeletonPositions[ m_jointIndices[ 0 ] ];
    const XMVECTOR vJoint1 = pSkeletonData->SkeletonPositions[ m_jointIndices[ 1 ] ];
    const XMVECTOR vDiff = vJoint0 - vJoint1;

    switch ( m_Type )
    {
#ifdef ADD_TYPE_DIFF_POSITION_X
    case TYPE_DIFF_POSITION_X:
        m_fValue[ uPlayerIdx ] = XMVectorGetX( vDiff );
        break;
#endif

#ifdef ADD_TYPE_DIFF_POSITION_Y
    case TYPE_DIFF_POSITION_Y:
        m_fValue[ uPlayerIdx ] = XMVectorGetY( vDiff );
        break;
#endif

#ifdef ADD_TYPE_DIFF_POSITION_Z
    case TYPE_DIFF_POSITION_Z:
        m_fValue[ uPlayerIdx ] = XMVectorGetZ( vDiff );
        break;
#endif
    }    
}


//--------------------------------------------------------------------------------------
// Name: Clone
// Desc: Clone the data
//--------------------------------------------------------------------------------------

ClassifierDataUsingDiffPositionX* ClassifierDataUsingDiffPositionX::Clone()
{
    ClassifierDataUsingDiffPositionX* pClone = new ClassifierDataUsingDiffPositionX( m_jointIndices[ 0 ], m_jointIndices[ 1 ] );
    if ( pClone )
    {
        pClone->Copy( this );
    }
    return pClone;
}


//--------------------------------------------------------------------------------------
// Name: Clone
// Desc: Clone the data
//--------------------------------------------------------------------------------------

ClassifierDataUsingDiffPositionY* ClassifierDataUsingDiffPositionY::Clone()
{
    ClassifierDataUsingDiffPositionY* pClone = new ClassifierDataUsingDiffPositionY( m_jointIndices[ 0 ], m_jointIndices[ 1 ] );
    if ( pClone )
    {
        pClone->Copy( this );
    }

    return pClone;
}


//--------------------------------------------------------------------------------------
// Name: Clone
// Desc: Clone the data
//--------------------------------------------------------------------------------------

ClassifierDataUsingDiffPositionZ* ClassifierDataUsingDiffPositionZ::Clone()
{
    ClassifierDataUsingDiffPositionZ* pClone = new ClassifierDataUsingDiffPositionZ( m_jointIndices[ 0 ], m_jointIndices[ 1 ] );
    if ( pClone )
    {
        pClone->Copy( this );
    }
    return pClone;
}


//--------------------------------------------------------------------------------------
// Name: Update
// Desc: Update the data
//--------------------------------------------------------------------------------------

VOID ClassifierDataUsingAngles::Update( const UINT uPlayerIdx, VOID* )
{
    // This classifier type operates only on the current frame
    NUI_SKELETON_DATA* pSkeletonData = GetCurrentSkeleton( uPlayerIdx );
    assert( pSkeletonData );

    if ( pSkeletonData->eTrackingState != NUI_SKELETON_TRACKED )
    {
        m_fValue[ uPlayerIdx ] = g_fInvalidValue;
        return;
    }

    // Get the tracking states for joints
    NUI_SKELETON_POSITION_TRACKING_STATE firstJointTrackingState    = pSkeletonData->eSkeletonPositionTrackingState[ m_jointIndices[ 0 ] ];
    NUI_SKELETON_POSITION_TRACKING_STATE secondJointTrackingState   = pSkeletonData->eSkeletonPositionTrackingState[ m_jointIndices[ 1 ] ];
    NUI_SKELETON_POSITION_TRACKING_STATE lastJointTrackingState     = pSkeletonData->eSkeletonPositionTrackingState[ m_jointIndices[ 2 ] ];

    if ( m_bRejectInferred )
    {
        if ( firstJointTrackingState != NUI_SKELETON_POSITION_TRACKED ||
             secondJointTrackingState != NUI_SKELETON_POSITION_TRACKED ||
             lastJointTrackingState != NUI_SKELETON_POSITION_TRACKED )
        {
            m_fValue[ uPlayerIdx ] = g_fInvalidValue;
            return;
        }
    }
    else
    {
        if ( firstJointTrackingState == NUI_SKELETON_POSITION_NOT_TRACKED ||
             secondJointTrackingState == NUI_SKELETON_POSITION_NOT_TRACKED ||
             lastJointTrackingState == NUI_SKELETON_POSITION_NOT_TRACKED )
        {
            m_fValue[ uPlayerIdx ] = g_fInvalidValue;
            return;
        }
    }

    // Get the joint positions that form the angle
    const XMVECTOR vFirstJoint    = pSkeletonData->SkeletonPositions[ m_jointIndices[ 0 ] ];
    const XMVECTOR vMiddleJoint   = pSkeletonData->SkeletonPositions[ m_jointIndices[ 1 ] ];
    const XMVECTOR vLastJoint     = pSkeletonData->SkeletonPositions[ m_jointIndices[ 2 ] ];

    // Calculate 2 normalized direction vectors
    const XMVECTOR vFirstNormal   = XMVector3NormalizeEst( vFirstJoint - vMiddleJoint );
    const XMVECTOR vSecondNormal  = XMVector3NormalizeEst( vLastJoint - vMiddleJoint );

    // Calculate angle in degrees
    const XMVECTOR vAngle = XMVector2AngleBetweenNormalsEst( vFirstNormal, vSecondNormal );
    m_fValue[ uPlayerIdx ] = XMConvertToDegrees( XMVectorGetX( vAngle ) );
}


//--------------------------------------------------------------------------------------
// Name: Clone
// Desc: Clone the data
//--------------------------------------------------------------------------------------

ClassifierDataUsingAngles* ClassifierDataUsingAngles::Clone()
{
    ClassifierDataUsingAngles* pClone = new ClassifierDataUsingAngles( m_jointIndices[ 0 ], m_jointIndices[ 1 ], m_jointIndices[ 2 ] );
    if ( pClone )
    {
        pClone->Copy( this );
    }
    return pClone;
}


//--------------------------------------------------------------------------------------
// Name: Update
// Desc: Update the data
//--------------------------------------------------------------------------------------

VOID ClassifierDataUsingTimeSpaceAngles::Update( const UINT uPlayerIdx, VOID* )
{
    // Check that all skeletons are tracked
    const UINT uIdx0 = m_SkeletonDataHistory[ uPlayerIdx ].GetCurrentFrameIndex();
    const UINT uIdx1 = m_SkeletonDataHistory[ uPlayerIdx ].GetPreviousFrameIndex( uIdx0 );
    const UINT uIdx2 = m_SkeletonDataHistory[ uPlayerIdx ].GetPreviousFrameIndex( uIdx1 );
    const NUI_SKELETON_DATA* pSkeletonData0 = m_SkeletonDataHistory[ uPlayerIdx ].GetAt( uIdx0 );
    const NUI_SKELETON_DATA* pSkeletonData1 = m_SkeletonDataHistory[ uPlayerIdx ].GetAt( uIdx1 );
    const NUI_SKELETON_DATA* pSkeletonData2 = m_SkeletonDataHistory[ uPlayerIdx ].GetAt( uIdx2 );

    assert( pSkeletonData0 );
    assert( pSkeletonData1 );
    assert( pSkeletonData2 );

    if ( pSkeletonData0->eTrackingState != NUI_SKELETON_TRACKED ||
         pSkeletonData1->eTrackingState != NUI_SKELETON_TRACKED ||
         pSkeletonData2->eTrackingState != NUI_SKELETON_TRACKED )
    {
        m_fValue[ uPlayerIdx ] = g_fInvalidValue;
        return;
    }

    // Get the tracking states for joints
    const NUI_SKELETON_POSITION_TRACKING_STATE firstJointTrackingState    = pSkeletonData0->eSkeletonPositionTrackingState[ m_jointIndex ];
    const NUI_SKELETON_POSITION_TRACKING_STATE secondJointTrackingState   = pSkeletonData1->eSkeletonPositionTrackingState[ m_jointIndex ];
    const NUI_SKELETON_POSITION_TRACKING_STATE lastJointTrackingState     = pSkeletonData2->eSkeletonPositionTrackingState[ m_jointIndex ];

    if ( m_bRejectInferred )
    {
        if ( firstJointTrackingState != NUI_SKELETON_POSITION_TRACKED ||
             secondJointTrackingState != NUI_SKELETON_POSITION_TRACKED ||
             lastJointTrackingState != NUI_SKELETON_POSITION_TRACKED )
        {
            m_fValue[ uPlayerIdx ] = g_fInvalidValue;
            return;
        }
    }
    else
    {
        if ( firstJointTrackingState == NUI_SKELETON_POSITION_NOT_TRACKED ||
             secondJointTrackingState == NUI_SKELETON_POSITION_NOT_TRACKED ||
             lastJointTrackingState == NUI_SKELETON_POSITION_NOT_TRACKED )
        {
            m_fValue[ uPlayerIdx ] = g_fInvalidValue;
            return;
        }
    }

    // Get the joint positions that form the angle
    const XMVECTOR vFirstJoint    = pSkeletonData0->SkeletonPositions[ m_jointIndex ];
    const XMVECTOR vMiddleJoint   = pSkeletonData1->SkeletonPositions[ m_jointIndex ];
    const XMVECTOR vLastJoint     = pSkeletonData2->SkeletonPositions[ m_jointIndex ];

    // Calculate 2 normalized direction vectors
    const XMVECTOR vFirstNormal   = XMVector3NormalizeEst( vFirstJoint - vMiddleJoint );
    const XMVECTOR vSecondNormal  = XMVector3NormalizeEst( vLastJoint - vMiddleJoint );

    // Calculate angle in degrees
    const XMVECTOR vAngle = XMVector2AngleBetweenNormalsEst( vFirstNormal, vSecondNormal );
    m_fValue[ uPlayerIdx ] = XMConvertToDegrees( XMVectorGetX( vAngle ) );
}


//--------------------------------------------------------------------------------------
// Name: Clone
// Desc: Clone data
//--------------------------------------------------------------------------------------

ClassifierDataUsingTimeSpaceAngles* ClassifierDataUsingTimeSpaceAngles::Clone()
{
    ClassifierDataUsingTimeSpaceAngles* pClone = new ClassifierDataUsingTimeSpaceAngles( m_jointIndex );
    if ( pClone )
    {
        pClone->Copy( this );
    }
    return pClone;
}


//--------------------------------------------------------------------------------------
// Name: Update
// Desc: Update data
//--------------------------------------------------------------------------------------

VOID ClassifierDataUsingPositionSpeed::Update( const UINT uPlayerIdx, VOID* pData )
{
    // This classifier type operates on the current and previous frames
    const UINT uCurrentFrameIndex	= m_SkeletonDataHistory[ uPlayerIdx ].GetCurrentFrameIndex();
    const UINT uPreviousFrameIndex	= m_SkeletonDataHistory[ uPlayerIdx ].GetPreviousFrameIndex( uCurrentFrameIndex );

    const NUI_SKELETON_DATA* pCurrentSkeletonData = m_SkeletonDataHistory[ uPlayerIdx ].GetAt( uCurrentFrameIndex );
    const NUI_SKELETON_DATA* pPreviousSkeletonData = m_SkeletonDataHistory[ uPlayerIdx ].GetAt( uPreviousFrameIndex );

    assert( pCurrentSkeletonData );
    assert( pPreviousSkeletonData );

    if ( pCurrentSkeletonData->eTrackingState != NUI_SKELETON_TRACKED ||
         pPreviousSkeletonData->eTrackingState != NUI_SKELETON_TRACKED )
    {
        m_fValue[ uPlayerIdx ] = g_fInvalidValue;
        return;
    }

    // Get the tracking state for joints
    const NUI_SKELETON_POSITION_TRACKING_STATE currentJointTrackingState = pCurrentSkeletonData->eSkeletonPositionTrackingState[ m_jointIndex ];
    const NUI_SKELETON_POSITION_TRACKING_STATE previousJointTrackingState = pPreviousSkeletonData->eSkeletonPositionTrackingState[ m_jointIndex ];

    if ( m_bRejectInferred )
    {
        if ( currentJointTrackingState != NUI_SKELETON_POSITION_TRACKED ||
             previousJointTrackingState != NUI_SKELETON_POSITION_TRACKED )
        {
            m_fValue[ uPlayerIdx ] = g_fInvalidValue;
            return;
        }
    }
    else
    {
        if ( currentJointTrackingState == NUI_SKELETON_POSITION_NOT_TRACKED ||
             previousJointTrackingState == NUI_SKELETON_POSITION_NOT_TRACKED )
        {
            m_fValue[ uPlayerIdx ] = g_fInvalidValue;
            return;
        }
    }

    // Get the joint positions
    const XMVECTOR vCurrentPosition     = pCurrentSkeletonData->SkeletonPositions[ m_jointIndex ];
    const XMVECTOR vPreviousPosition    = pPreviousSkeletonData->SkeletonPositions[ m_jointIndex ];

    // Calc the speed of joint position
    const FLOAT fDeltaTimeInSeconds     = *((FLOAT*)pData);
    const XMVECTOR vVelocity            = ( vCurrentPosition - vPreviousPosition ) / fDeltaTimeInSeconds;
    FLOAT fVelocity                     = 0.0f;

    switch( m_Type )
    {
#ifdef ADD_TYPE_POSITION_SPEED
    case TYPE_POSITION_SPEED:
        fVelocity   = fabsf( XMVectorGetX( XMVector3LengthEst( vVelocity ) ) );
        break;
#endif

#ifdef ADD_TYPE_POSITION_SPEED_SQ
    case TYPE_POSITION_SPEED_SQ:
        fVelocity   = fabsf( XMVectorGetX( XMVector3LengthEst( vVelocity ) ) );
		fVelocity	= fVelocity * fVelocity;
        break;
#endif

#ifdef ADD_TYPE_POSITION_VELOCITY_X
    case TYPE_POSITION_VELOCITY_X:
        fVelocity   = XMVectorGetX( vVelocity );
        break;
#endif

#ifdef ADD_TYPE_POSITION_VELOCITY_Y
    case TYPE_POSITION_VELOCITY_Y:
        fVelocity   = XMVectorGetY( vVelocity );
        break;
#endif

#ifdef ADD_TYPE_POSITION_VELOCITY_Z
    case TYPE_POSITION_VELOCITY_Z:
        fVelocity   = XMVectorGetZ( vVelocity );
        break;
#endif

#ifdef ADD_TYPE_POSITION_VELOCITYSQ_X
    case TYPE_POSITION_VELOCITYSQ_X:
        fVelocity   = XMVectorGetX( vVelocity );
		fVelocity	= fVelocity * fVelocity;
        break;
#endif

#ifdef ADD_TYPE_POSITION_VELOCITYSQ_Y
    case TYPE_POSITION_VELOCITYSQ_Y:
        fVelocity   = XMVectorGetY( vVelocity );
		fVelocity	= fVelocity * fVelocity;
        break;
#endif

#ifdef ADD_TYPE_POSITION_VELOCITYSQ_Z
    case TYPE_POSITION_VELOCITYSQ_Z:
        fVelocity   = XMVectorGetZ( vVelocity );
		fVelocity	= fVelocity * fVelocity;
        break;
#endif
    }

    // This should automatically compile to __fsel() on Xbox
    m_fValue[ uPlayerIdx ] = ( ( fDeltaTimeInSeconds - FLT_EPSILON ) >= 0.0f ) ? fVelocity : 0.0f;
}


//--------------------------------------------------------------------------------------
// Name: Clone
// Desc: Cone data
//--------------------------------------------------------------------------------------

ClassifierDataUsingPositionSpeed* ClassifierDataUsingPositionSpeed::Clone()
{
    ClassifierDataUsingPositionSpeed* pClone = new ClassifierDataUsingPositionSpeed( m_jointIndex );
    if ( pClone )
    {
        pClone->Copy( this );
    }
    return pClone;
}


//--------------------------------------------------------------------------------------
// Name: Update
// Desc: Update data
//--------------------------------------------------------------------------------------

VOID ClassifierDataUsingPositionAcceleration::Update( const UINT uPlayerIdx, VOID* pData )
{
    // This classifier type operates on 3 frames to calculate acceleration
    const UINT uCurrentFrameIndex	= m_SkeletonDataHistory[ uPlayerIdx ].GetCurrentFrameIndex();
    const UINT uPreviousFrameIndex	= m_SkeletonDataHistory[ uPlayerIdx ].GetPreviousFrameIndex( uCurrentFrameIndex );
    const UINT uOldFrameIndex       = m_SkeletonDataHistory[ uPlayerIdx ].GetPreviousFrameIndex( uPreviousFrameIndex );

    const NUI_SKELETON_DATA* pCurrentSkeletonData = m_SkeletonDataHistory[ uPlayerIdx ].GetAt( uCurrentFrameIndex );
    const NUI_SKELETON_DATA* pPreviousSkeletonData = m_SkeletonDataHistory[ uPlayerIdx ].GetAt( uPreviousFrameIndex );
    const NUI_SKELETON_DATA* pOldSkeletonData = m_SkeletonDataHistory[ uPlayerIdx ].GetAt( uOldFrameIndex );

    assert( pCurrentSkeletonData );
    assert( pPreviousSkeletonData );
    assert( pOldSkeletonData );

    if ( pCurrentSkeletonData->eTrackingState != NUI_SKELETON_TRACKED ||
         pPreviousSkeletonData->eTrackingState != NUI_SKELETON_TRACKED ||
         pOldSkeletonData->eTrackingState != NUI_SKELETON_TRACKED )
    {
        m_fValue[ uPlayerIdx ] = g_fInvalidValue;
        return;
    }

    // Get the tracking state for joints
    const NUI_SKELETON_POSITION_TRACKING_STATE currentJointTrackingState = pCurrentSkeletonData->eSkeletonPositionTrackingState[ m_jointIndex ];
    const NUI_SKELETON_POSITION_TRACKING_STATE previousJointTrackingState = pPreviousSkeletonData->eSkeletonPositionTrackingState[ m_jointIndex ];
    const NUI_SKELETON_POSITION_TRACKING_STATE oldJointTrackingState = pOldSkeletonData->eSkeletonPositionTrackingState[ m_jointIndex ];

    if ( m_bRejectInferred )
    {
        if ( currentJointTrackingState != NUI_SKELETON_POSITION_TRACKED ||
             previousJointTrackingState != NUI_SKELETON_POSITION_TRACKED ||
             oldJointTrackingState != NUI_SKELETON_POSITION_TRACKED )
        {
            m_fValue[ uPlayerIdx ] = g_fInvalidValue;
            return;
        }
    }
    else
    {
        if ( currentJointTrackingState == NUI_SKELETON_POSITION_NOT_TRACKED ||
             previousJointTrackingState == NUI_SKELETON_POSITION_NOT_TRACKED ||
             oldJointTrackingState == NUI_SKELETON_POSITION_NOT_TRACKED )
        {
            m_fValue[ uPlayerIdx ] = g_fInvalidValue;
            return;
        }
    }

    // Get the joint positions
    const XMVECTOR vCurrentPosition     = pCurrentSkeletonData->SkeletonPositions[ m_jointIndex ];
    const XMVECTOR vPreviousPosition    = pPreviousSkeletonData->SkeletonPositions[ m_jointIndex ];
    const XMVECTOR vOldPosition         = pOldSkeletonData->SkeletonPositions[ m_jointIndex ];

    // Calc the acceleration of joint position
    const FLOAT fDeltaTimeInSeconds     = *((FLOAT*)pData);
    const XMVECTOR vAcceleration        = ( vCurrentPosition - 2 * vPreviousPosition + vOldPosition) / ( 2.0f * fDeltaTimeInSeconds );

    FLOAT fAcceleration                 = 0.0f;
    	
    switch( m_Type )
    {
#ifdef ADD_TYPE_POSITION_ACCELERATION
    case TYPE_POSITION_ACCELERATION:
        fAcceleration = fabsf(XMVectorGetX( XMVector3LengthEst( vAcceleration ) ));
        break;
#endif

#ifdef ADD_TYPE_POSITION_ACCELERATION_X
    case TYPE_POSITION_ACCELERATION_X:
        fAcceleration = XMVectorGetX( vAcceleration );
        break;
#endif

#ifdef ADD_TYPE_POSITION_ACCELERATION_Y
    case TYPE_POSITION_ACCELERATION_Y:
        fAcceleration = XMVectorGetY( vAcceleration );
        break;
#endif

#ifdef ADD_TYPE_POSITION_ACCELERATION_Z
    case TYPE_POSITION_ACCELERATION_Z:
        fAcceleration = XMVectorGetZ( vAcceleration );
        break;
#endif
    }

    // This should automatically compile to __fsel() on Xbox
    m_fValue[ uPlayerIdx ] = ( ( fDeltaTimeInSeconds - FLT_EPSILON ) >= 0.0f ) ? fAcceleration : 0.0f;
}


//--------------------------------------------------------------------------------------
// Name: Clone
// Desc: Cone data
//--------------------------------------------------------------------------------------

ClassifierDataUsingPositionAcceleration* ClassifierDataUsingPositionAcceleration::Clone()
{
    ClassifierDataUsingPositionAcceleration* pClone = new ClassifierDataUsingPositionAcceleration( m_jointIndex );
    if ( pClone )
    {
        pClone->Copy( this );
    }
    return pClone;
}

//--------------------------------------------------------------------------------------
// Name: Clone
// Desc: Cone data
//--------------------------------------------------------------------------------------

ClassifierDataUsingPositionAccelerationX* ClassifierDataUsingPositionAccelerationX::Clone()
{
    ClassifierDataUsingPositionAccelerationX* pClone = new ClassifierDataUsingPositionAccelerationX( m_jointIndex );
    if ( pClone )
    {
        pClone->Copy( this );
    }
    return pClone;
}

//--------------------------------------------------------------------------------------
// Name: Clone
// Desc: Cone data
//--------------------------------------------------------------------------------------

ClassifierDataUsingPositionAccelerationY* ClassifierDataUsingPositionAccelerationY::Clone()
{
    ClassifierDataUsingPositionAccelerationY* pClone = new ClassifierDataUsingPositionAccelerationY( m_jointIndex );
    if ( pClone )
    {
        pClone->Copy( this );
    }
    return pClone;
}

//--------------------------------------------------------------------------------------
// Name: Clone
// Desc: Cone data
//--------------------------------------------------------------------------------------

ClassifierDataUsingPositionAccelerationZ* ClassifierDataUsingPositionAccelerationZ::Clone()
{
    ClassifierDataUsingPositionAccelerationZ* pClone = new ClassifierDataUsingPositionAccelerationZ( m_jointIndex );
    if ( pClone )
    {
        pClone->Copy( this );
    }
    return pClone;
}

//--------------------------------------------------------------------------------------
// Name: Clone
// Desc: Cone data
//--------------------------------------------------------------------------------------

ClassifierDataUsingPositionSpeedSQ* ClassifierDataUsingPositionSpeedSQ::Clone()
{
    ClassifierDataUsingPositionSpeedSQ* pClone = new ClassifierDataUsingPositionSpeedSQ( m_jointIndex );
    if ( pClone )
    {
        pClone->Copy( this );
    }
    return pClone;
}


//--------------------------------------------------------------------------------------
// Name: Clone
// Desc: Clone data
//--------------------------------------------------------------------------------------

ClassifierDataUsingPositionVelocityX* ClassifierDataUsingPositionVelocityX::Clone()
{
    ClassifierDataUsingPositionVelocityX* pClone = new ClassifierDataUsingPositionVelocityX( m_jointIndex );
    if ( pClone )
    {
        pClone->Copy( this );
    }
    return pClone;
}


//--------------------------------------------------------------------------------------
// Name: Clone
// Desc: Clone data
//--------------------------------------------------------------------------------------

ClassifierDataUsingPositionVelocityY* ClassifierDataUsingPositionVelocityY::Clone()
{
    ClassifierDataUsingPositionVelocityY* pClone = new ClassifierDataUsingPositionVelocityY( m_jointIndex );
    if ( pClone )
    {
        pClone->Copy( this );
    }
    return pClone;
}


//--------------------------------------------------------------------------------------
// Name: Clone
// Desc: Clone data
//--------------------------------------------------------------------------------------

ClassifierDataUsingPositionVelocityZ* ClassifierDataUsingPositionVelocityZ::Clone()
{
    ClassifierDataUsingPositionVelocityZ* pClone = new ClassifierDataUsingPositionVelocityZ( m_jointIndex );
    if ( pClone )
    {
        pClone->Copy( this );
    }
    return pClone;
}

//--------------------------------------------------------------------------------------
// Name: Clone
// Desc: Clone data
//--------------------------------------------------------------------------------------

ClassifierDataUsingPositionVelocitySQX* ClassifierDataUsingPositionVelocitySQX::Clone()
{
    ClassifierDataUsingPositionVelocitySQX* pClone = new ClassifierDataUsingPositionVelocitySQX( m_jointIndex );
    if ( pClone )
    {
        pClone->Copy( this );
    }
    return pClone;
}


//--------------------------------------------------------------------------------------
// Name: Clone
// Desc: Clone data
//--------------------------------------------------------------------------------------

ClassifierDataUsingPositionVelocitySQY* ClassifierDataUsingPositionVelocitySQY::Clone()
{
    ClassifierDataUsingPositionVelocitySQY* pClone = new ClassifierDataUsingPositionVelocitySQY( m_jointIndex );
    if ( pClone )
    {
        pClone->Copy( this );
    }
    return pClone;
}


//--------------------------------------------------------------------------------------
// Name: Clone
// Desc: Clone data
//--------------------------------------------------------------------------------------

ClassifierDataUsingPositionVelocitySQZ* ClassifierDataUsingPositionVelocitySQZ::Clone()
{
    ClassifierDataUsingPositionVelocitySQZ* pClone = new ClassifierDataUsingPositionVelocitySQZ( m_jointIndex );
    if ( pClone )
    {
        pClone->Copy( this );
    }
    return pClone;
}

//--------------------------------------------------------------------------------------
// Name: Update
// Desc: Update data
//--------------------------------------------------------------------------------------

VOID ClassifierDataUsingAngleVelocities::Update( const UINT uPlayerIdx, VOID* pData )
{
    const UINT uCurrentFrameIndex = m_SkeletonDataHistory[ uPlayerIdx ].GetCurrentFrameIndex();
    const UINT uPreviousFrameIndex = m_SkeletonDataHistory[ uPlayerIdx ].GetPreviousFrameIndex( uCurrentFrameIndex );

    // This classifier type operates on the current and previous frame
    const NUI_SKELETON_DATA* pCurrentSkeletonData = m_SkeletonDataHistory[ uPlayerIdx ].GetAt( uCurrentFrameIndex );
    const NUI_SKELETON_DATA* pPreviousSkeletonData = m_SkeletonDataHistory[ uPlayerIdx ].GetAt( uPreviousFrameIndex );

    assert( pCurrentSkeletonData );
    assert( pPreviousSkeletonData );

    if ( pCurrentSkeletonData->eTrackingState != NUI_SKELETON_TRACKED ||
         pPreviousSkeletonData->eTrackingState != NUI_SKELETON_TRACKED )
    {
        m_fValue[ uPlayerIdx ] = g_fInvalidValue;
        return;
    }

    // Get the tracking states for joints
    NUI_SKELETON_POSITION_TRACKING_STATE firstJointTrackingState    = pCurrentSkeletonData->eSkeletonPositionTrackingState[ m_jointIndices[ 0 ] ];
    NUI_SKELETON_POSITION_TRACKING_STATE secondJointTrackingState   = pCurrentSkeletonData->eSkeletonPositionTrackingState[ m_jointIndices[ 1 ] ];
    NUI_SKELETON_POSITION_TRACKING_STATE lastJointTrackingState     = pCurrentSkeletonData->eSkeletonPositionTrackingState[ m_jointIndices[ 2 ] ];

    if ( m_bRejectInferred )
    {
        if ( firstJointTrackingState != NUI_SKELETON_POSITION_TRACKED ||
             secondJointTrackingState != NUI_SKELETON_POSITION_TRACKED ||
             lastJointTrackingState != NUI_SKELETON_POSITION_TRACKED )
        {
            m_fValue[ uPlayerIdx ] = g_fInvalidValue;
            return;
        }
    }
    else
    {
        if ( firstJointTrackingState == NUI_SKELETON_POSITION_NOT_TRACKED ||
             secondJointTrackingState == NUI_SKELETON_POSITION_NOT_TRACKED ||
             lastJointTrackingState == NUI_SKELETON_POSITION_NOT_TRACKED )
        {
            m_fValue[ uPlayerIdx ] = g_fInvalidValue;
            return;
        }
    }

    // Get the tracking states for joints
    firstJointTrackingState     = pPreviousSkeletonData->eSkeletonPositionTrackingState[ m_jointIndices[ 0 ] ];
    secondJointTrackingState    = pPreviousSkeletonData->eSkeletonPositionTrackingState[ m_jointIndices[ 1 ] ];
    lastJointTrackingState      = pPreviousSkeletonData->eSkeletonPositionTrackingState[ m_jointIndices[ 2 ] ];

    if ( m_bRejectInferred )
    {
        if ( firstJointTrackingState != NUI_SKELETON_POSITION_TRACKED ||
             secondJointTrackingState != NUI_SKELETON_POSITION_TRACKED ||
             lastJointTrackingState != NUI_SKELETON_POSITION_TRACKED )
        {
            m_fValue[ uPlayerIdx ] = g_fInvalidValue;
            return;
        }
    }
    else
    {
        if ( firstJointTrackingState == NUI_SKELETON_POSITION_NOT_TRACKED ||
             secondJointTrackingState == NUI_SKELETON_POSITION_NOT_TRACKED ||
             lastJointTrackingState == NUI_SKELETON_POSITION_NOT_TRACKED )
        {
            m_fValue[ uPlayerIdx ] = g_fInvalidValue;
            return;
        }
    }

    // Get the joint positions that form the angle
    XMVECTOR vFirstJoint    = pCurrentSkeletonData->SkeletonPositions[ m_jointIndices[ 0 ] ];
    XMVECTOR vMiddleJoint   = pCurrentSkeletonData->SkeletonPositions[ m_jointIndices[ 1 ] ];
    XMVECTOR vLastJoint     = pCurrentSkeletonData->SkeletonPositions[ m_jointIndices[ 2 ] ];

    // Calculate 2 normalized direction vectors
    XMVECTOR vFirstNormal   = XMVector3NormalizeEst( vFirstJoint - vMiddleJoint );
    XMVECTOR vSecondNormal  = XMVector3NormalizeEst( vLastJoint - vMiddleJoint );

    // Calculate angle in degrees
    XMVECTOR vCurrentAngle = XMVector2AngleBetweenNormalsEst( vFirstNormal, vSecondNormal );

    // Get the joint positions that form the angle
    vFirstJoint    = pPreviousSkeletonData->SkeletonPositions[ m_jointIndices[ 0 ] ];
    vMiddleJoint   = pPreviousSkeletonData->SkeletonPositions[ m_jointIndices[ 1 ] ];
    vLastJoint     = pPreviousSkeletonData->SkeletonPositions[ m_jointIndices[ 2 ] ];

    // Calculate 2 normalized direction vectors
    vFirstNormal   = XMVector3NormalizeEst( vFirstJoint - vMiddleJoint );
    vSecondNormal  = XMVector3NormalizeEst( vLastJoint - vMiddleJoint );

    // Calculate angle in degrees
    XMVECTOR vPreviousAngle = XMVector2AngleBetweenNormalsEst( vFirstNormal, vSecondNormal );

    // Calc angle velocity
    FLOAT fDeltaTimeInSeconds = *((FLOAT*)pData);
    XMVECTOR vDifference = vCurrentAngle - vPreviousAngle;
    FLOAT fVelocity = XMVectorGetX( vDifference ) / fDeltaTimeInSeconds;

    // This should automatically compile to __fsel() on Xbox
    m_fValue[ uPlayerIdx ] = ( ( fDeltaTimeInSeconds - FLT_EPSILON ) >= 0.0f ) ? fVelocity : 0.0f;
}


//--------------------------------------------------------------------------------------
// Name: Clone
// Desc: Clone data
//--------------------------------------------------------------------------------------

ClassifierDataUsingAngleVelocities* ClassifierDataUsingAngleVelocities::Clone()
{
    ClassifierDataUsingAngleVelocities* pClone = new ClassifierDataUsingAngleVelocities( m_jointIndices[ 0 ], m_jointIndices[ 1 ], m_jointIndices[ 2 ] );
    if ( pClone )
    {
        pClone->Copy( this );
    }
    return pClone;
}


//--------------------------------------------------------------------------------------
// Name: Update
// Desc: Update data
//--------------------------------------------------------------------------------------

VOID ClassifierDataUsingAngleAcceleration::Update( const UINT uPlayerIdx, VOID* pData )
{
    const UINT uCurrentFrameIndex = m_SkeletonDataHistory[ uPlayerIdx ].GetCurrentFrameIndex();
    const UINT uPreviousFrameIndex = m_SkeletonDataHistory[ uPlayerIdx ].GetPreviousFrameIndex( uCurrentFrameIndex );
    const UINT uPreviousFrameIndex2 = m_SkeletonDataHistory[ uPlayerIdx ].GetPreviousFrameIndex( uPreviousFrameIndex );

    // This classifier type operates on the current and previous frame
    const NUI_SKELETON_DATA* pCurrentSkeletonData = m_SkeletonDataHistory[ uPlayerIdx ].GetAt( uCurrentFrameIndex );
    const NUI_SKELETON_DATA* pPreviousSkeletonData = m_SkeletonDataHistory[ uPlayerIdx ].GetAt( uPreviousFrameIndex );
    const NUI_SKELETON_DATA* pPreviousSkeletonData2 = m_SkeletonDataHistory[ uPlayerIdx ].GetAt( uPreviousFrameIndex2 );

    assert( pCurrentSkeletonData );
    assert( pPreviousSkeletonData );
    assert( pPreviousSkeletonData2 );

    if ( pCurrentSkeletonData->eTrackingState != NUI_SKELETON_TRACKED ||
         pPreviousSkeletonData->eTrackingState != NUI_SKELETON_TRACKED ||
         pPreviousSkeletonData2->eTrackingState != NUI_SKELETON_TRACKED )
    {
        m_fValue[ uPlayerIdx ] = g_fInvalidValue;
        return;
    }

    // Get the tracking states for joints
    NUI_SKELETON_POSITION_TRACKING_STATE firstJointTrackingState    = pCurrentSkeletonData->eSkeletonPositionTrackingState[ m_jointIndices[ 0 ] ];
    NUI_SKELETON_POSITION_TRACKING_STATE secondJointTrackingState   = pCurrentSkeletonData->eSkeletonPositionTrackingState[ m_jointIndices[ 1 ] ];
    NUI_SKELETON_POSITION_TRACKING_STATE lastJointTrackingState     = pCurrentSkeletonData->eSkeletonPositionTrackingState[ m_jointIndices[ 2 ] ];

    if ( m_bRejectInferred )
    {
        if ( firstJointTrackingState != NUI_SKELETON_POSITION_TRACKED ||
             secondJointTrackingState != NUI_SKELETON_POSITION_TRACKED ||
             lastJointTrackingState != NUI_SKELETON_POSITION_TRACKED )
        {
            m_fValue[ uPlayerIdx ] = g_fInvalidValue;
            return;
        }
    }
    else
    {
        if ( firstJointTrackingState == NUI_SKELETON_POSITION_NOT_TRACKED ||
             secondJointTrackingState == NUI_SKELETON_POSITION_NOT_TRACKED ||
             lastJointTrackingState == NUI_SKELETON_POSITION_NOT_TRACKED )
        {
            m_fValue[ uPlayerIdx ] = g_fInvalidValue;
            return;
        }
    }

    // Get the tracking states for joints
    firstJointTrackingState     = pPreviousSkeletonData->eSkeletonPositionTrackingState[ m_jointIndices[ 0 ] ];
    secondJointTrackingState    = pPreviousSkeletonData->eSkeletonPositionTrackingState[ m_jointIndices[ 1 ] ];
    lastJointTrackingState      = pPreviousSkeletonData->eSkeletonPositionTrackingState[ m_jointIndices[ 2 ] ];

    if ( m_bRejectInferred )
    {
        if ( firstJointTrackingState != NUI_SKELETON_POSITION_TRACKED ||
             secondJointTrackingState != NUI_SKELETON_POSITION_TRACKED ||
             lastJointTrackingState != NUI_SKELETON_POSITION_TRACKED )
        {
            m_fValue[ uPlayerIdx ] = g_fInvalidValue;
            return;
        }
    }
    else
    {
        if ( firstJointTrackingState == NUI_SKELETON_POSITION_NOT_TRACKED ||
             secondJointTrackingState == NUI_SKELETON_POSITION_NOT_TRACKED ||
             lastJointTrackingState == NUI_SKELETON_POSITION_NOT_TRACKED )
        {
            m_fValue[ uPlayerIdx ] = g_fInvalidValue;
            return;
        }
    }

    // Get the tracking states for joints
    firstJointTrackingState     = pPreviousSkeletonData2->eSkeletonPositionTrackingState[ m_jointIndices[ 0 ] ];
    secondJointTrackingState    = pPreviousSkeletonData2->eSkeletonPositionTrackingState[ m_jointIndices[ 1 ] ];
    lastJointTrackingState      = pPreviousSkeletonData2->eSkeletonPositionTrackingState[ m_jointIndices[ 2 ] ];

    if ( m_bRejectInferred )
    {
        if ( firstJointTrackingState != NUI_SKELETON_POSITION_TRACKED ||
             secondJointTrackingState != NUI_SKELETON_POSITION_TRACKED ||
             lastJointTrackingState != NUI_SKELETON_POSITION_TRACKED )
        {
            m_fValue[ uPlayerIdx ] = g_fInvalidValue;
            return;
        }
    }
    else
    {
        if ( firstJointTrackingState == NUI_SKELETON_POSITION_NOT_TRACKED ||
             secondJointTrackingState == NUI_SKELETON_POSITION_NOT_TRACKED ||
             lastJointTrackingState == NUI_SKELETON_POSITION_NOT_TRACKED )
        {
            m_fValue[ uPlayerIdx ] = g_fInvalidValue;
            return;
        }
    }

    // Get the joint positions that form the angle
    XMVECTOR vFirstJoint    = pCurrentSkeletonData->SkeletonPositions[ m_jointIndices[ 0 ] ];
    XMVECTOR vMiddleJoint   = pCurrentSkeletonData->SkeletonPositions[ m_jointIndices[ 1 ] ];
    XMVECTOR vLastJoint     = pCurrentSkeletonData->SkeletonPositions[ m_jointIndices[ 2 ] ];

    // Calculate 2 normalized direction vectors
    XMVECTOR vFirstNormal   = XMVector3NormalizeEst( vFirstJoint - vMiddleJoint );
    XMVECTOR vSecondNormal  = XMVector3NormalizeEst( vLastJoint - vMiddleJoint );

    // Calculate angle in degrees
    XMVECTOR vCurrentAngle = XMVector2AngleBetweenNormalsEst( vFirstNormal, vSecondNormal );

    // Get the joint positions that form the angle
    vFirstJoint    = pPreviousSkeletonData->SkeletonPositions[ m_jointIndices[ 0 ] ];
    vMiddleJoint   = pPreviousSkeletonData->SkeletonPositions[ m_jointIndices[ 1 ] ];
    vLastJoint     = pPreviousSkeletonData->SkeletonPositions[ m_jointIndices[ 2 ] ];

    // Calculate 2 normalized direction vectors
    vFirstNormal   = XMVector3NormalizeEst( vFirstJoint - vMiddleJoint );
    vSecondNormal  = XMVector3NormalizeEst( vLastJoint - vMiddleJoint );

    // Calculate angle in degrees
    const XMVECTOR vPreviousAngle = XMVector2AngleBetweenNormalsEst( vFirstNormal, vSecondNormal );

    // Get the joint positions that form the angle
    vFirstJoint    = pPreviousSkeletonData2->SkeletonPositions[ m_jointIndices[ 0 ] ];
    vMiddleJoint   = pPreviousSkeletonData2->SkeletonPositions[ m_jointIndices[ 1 ] ];
    vLastJoint     = pPreviousSkeletonData2->SkeletonPositions[ m_jointIndices[ 2 ] ];

    // Calculate 2 normalized direction vectors
    vFirstNormal   = XMVector3NormalizeEst( vFirstJoint - vMiddleJoint );
    vSecondNormal  = XMVector3NormalizeEst( vLastJoint - vMiddleJoint );

    // Calculate angle in degrees
    const XMVECTOR vPreviousAngle2 = XMVector2AngleBetweenNormalsEst( vFirstNormal, vSecondNormal );

    // Calc angle velocities
    const FLOAT fDeltaTimeInSeconds = *((FLOAT*)pData);
    const FLOAT fInvDeltaTimeInSeconds = 1.0f / fDeltaTimeInSeconds;

    XMVECTOR vDifference = vCurrentAngle - vPreviousAngle;
    const FLOAT fCurrentVelocity = XMVectorGetX( vDifference ) * fInvDeltaTimeInSeconds;
    vDifference = vPreviousAngle - vPreviousAngle2;
    const FLOAT fPreviousVelocity = XMVectorGetX( vDifference ) * fInvDeltaTimeInSeconds;
    const FLOAT fAccel = ( fCurrentVelocity - fPreviousVelocity ) * fInvDeltaTimeInSeconds;

    // This should automatically compile to __fsel() on Xbox
    m_fValue[ uPlayerIdx ] = ( ( fDeltaTimeInSeconds - FLT_EPSILON ) >= 0.0f ) ? fAccel : 0.0f;
}


//--------------------------------------------------------------------------------------
// Name: Clone
// Desc: Clone data
//--------------------------------------------------------------------------------------

ClassifierDataUsingAngleAcceleration* ClassifierDataUsingAngleAcceleration::Clone()
{
    ClassifierDataUsingAngleAcceleration* pClone = new ClassifierDataUsingAngleAcceleration( m_jointIndices[ 0 ], m_jointIndices[ 1 ], m_jointIndices[ 2 ] );
    if ( pClone )
    {
        pClone->Copy( this );
    }
    return pClone;
}


//--------------------------------------------------------------------------------------
// Name: Update
// Desc: Update data
//--------------------------------------------------------------------------------------

VOID ClassifierDataUsingMuscleForceX::Update( const UINT uPlayerIdx, VOID* )
{
    // Check that skeleton is tracked
    const NUI_SKELETON_DATA* pSkeletonData = GetCurrentSkeleton( uPlayerIdx );

    if ( pSkeletonData->eTrackingState != NUI_SKELETON_TRACKED )
    {
        m_fValue[ uPlayerIdx ] = g_fInvalidValue;
        return;
    }

    if ( m_bRejectInferred )
    {
        if ( pSkeletonData->eSkeletonPositionTrackingState[ m_jointIndex ] != NUI_SKELETON_POSITION_TRACKED )
        {
            m_fValue[ uPlayerIdx ] = g_fInvalidValue;
            return;
        }
    }
    else
    {
        if ( pSkeletonData->eSkeletonPositionTrackingState[ m_jointIndex ] == NUI_SKELETON_POSITION_NOT_TRACKED )
        {
            m_fValue[ uPlayerIdx ] = g_fInvalidValue;
            return;
        }
    }

    const FLOAT fNormalizeConstant = m_Muscles[ uPlayerIdx ].Muscles[ m_jointIndex ].mForceNormalizationConstant;
    FLOAT fForce = 0.0f;

#ifdef ADD_TYPE_MUSCLE_FORCES
    switch ( m_Type )
    {
    case TYPE_MUSCLE_FORCE_X:
        fForce = XMVectorGetX( m_Muscles[ uPlayerIdx ].Muscles[ m_jointIndex ].mDynamicExternalForce ) / fNormalizeConstant;
        break;

    case TYPE_MUSCLE_FORCE_Y:
        fForce = XMVectorGetY( m_Muscles[ uPlayerIdx ].Muscles[ m_jointIndex ].mDynamicExternalForce ) / fNormalizeConstant;
        break;

    case TYPE_MUSCLE_FORCE_Z:
        fForce = XMVectorGetZ( m_Muscles[ uPlayerIdx ].Muscles[ m_jointIndex ].mDynamicExternalForce ) / fNormalizeConstant;
        break;
    }
#endif

    // This should automatically compile to __fsel() on Xbox
    m_fValue[ uPlayerIdx ] = ( ( fNormalizeConstant - FLT_EPSILON ) >= 0.0f ) ? fForce : g_fInvalidValue;
}


//--------------------------------------------------------------------------------------
// Name: Clone
// Desc: Clone data
//--------------------------------------------------------------------------------------

ClassifierDataUsingMuscleForceX* ClassifierDataUsingMuscleForceX::Clone()
{
    ClassifierDataUsingMuscleForceX* pClone = new ClassifierDataUsingMuscleForceX( m_jointIndex );
    if ( pClone )
    {
        pClone->Copy( this );
    }
    return pClone;
}


//--------------------------------------------------------------------------------------
// Name: Clone
// Desc: Clone data
//--------------------------------------------------------------------------------------

ClassifierDataUsingMuscleForceY* ClassifierDataUsingMuscleForceY::Clone()
{
    ClassifierDataUsingMuscleForceY* pClone = new ClassifierDataUsingMuscleForceY( m_jointIndex );
    if ( pClone )
    {
        pClone->Copy( this );
    }
    return pClone;
}


//--------------------------------------------------------------------------------------
// Name: Clone
// Desc: Clone data
//--------------------------------------------------------------------------------------

ClassifierDataUsingMuscleForceZ* ClassifierDataUsingMuscleForceZ::Clone()
{
    ClassifierDataUsingMuscleForceZ* pClone = new ClassifierDataUsingMuscleForceZ( m_jointIndex );
    if ( pClone )
    {
        pClone->Copy( this );
    }
    return pClone;
}


//--------------------------------------------------------------------------------------
// Name: Update
// Desc: Update data
//--------------------------------------------------------------------------------------

VOID ClassifierDataUsingMuscleTorqueX::Update( const UINT uPlayerIdx, VOID* )
{
    // Check that skeleton is tracked
    const NUI_SKELETON_DATA* pSkeletonData = GetCurrentSkeleton( uPlayerIdx );

    if ( pSkeletonData->eTrackingState != NUI_SKELETON_TRACKED )
    {
        m_fValue[ uPlayerIdx ] = g_fInvalidValue;
        return;
    }

    if ( m_bRejectInferred )
    {
        if ( pSkeletonData->eSkeletonPositionTrackingState[ m_jointIndex ] != NUI_SKELETON_POSITION_TRACKED )
        {
            m_fValue[ uPlayerIdx ] = g_fInvalidValue;
            return;
        }
    }
    else
    {
        if ( pSkeletonData->eSkeletonPositionTrackingState[ m_jointIndex ] == NUI_SKELETON_POSITION_NOT_TRACKED )
        {
            m_fValue[ uPlayerIdx ] = g_fInvalidValue;
            return;
        }
    }

    const FLOAT fNormalizeConstant = m_Muscles[ uPlayerIdx ].Muscles[ m_jointIndex ].mTorqueNormalizationConstant;
    FLOAT fTorque = 0.0f;

#ifdef ADD_TYPE_MUSCLE_TORQUES
    switch ( m_Type )
    {
    case TYPE_MUSCLE_TORQUE_X:
        fTorque = XMVectorGetX( m_Muscles[ uPlayerIdx ].Muscles[ m_jointIndex ].mDynamicExternalTorque ) / fNormalizeConstant;
        break;

    case TYPE_MUSCLE_TORQUE_Y:
        fTorque = XMVectorGetY( m_Muscles[ uPlayerIdx ].Muscles[ m_jointIndex ].mDynamicExternalTorque ) / fNormalizeConstant;
        break;

    case TYPE_MUSCLE_TORQUE_Z:
        fTorque = XMVectorGetZ( m_Muscles[ uPlayerIdx ].Muscles[ m_jointIndex ].mDynamicExternalTorque ) / fNormalizeConstant;
        break;
    }
#endif

    // This should automatically compile to __fsel() on Xbox
    m_fValue[ uPlayerIdx ] = ( ( fNormalizeConstant - FLT_EPSILON ) >= 0.0f ) ? fTorque : g_fInvalidValue;

}


//--------------------------------------------------------------------------------------
// Name: Clone
// Desc: Clone data
//--------------------------------------------------------------------------------------

ClassifierDataUsingMuscleTorqueX* ClassifierDataUsingMuscleTorqueX::Clone()
{
    ClassifierDataUsingMuscleTorqueX* pClone = new ClassifierDataUsingMuscleTorqueX( m_jointIndex );
    if ( pClone )
    {
        pClone->Copy( this );
    }
    return pClone;
}


//--------------------------------------------------------------------------------------
// Name: Clone
// Desc: Clone data
//--------------------------------------------------------------------------------------

ClassifierDataUsingMuscleTorqueY* ClassifierDataUsingMuscleTorqueY::Clone()
{
    ClassifierDataUsingMuscleTorqueY* pClone = new ClassifierDataUsingMuscleTorqueY( m_jointIndex );
    if ( pClone )
    {
        pClone->Copy( this );
    }
    return pClone;
}


//--------------------------------------------------------------------------------------
// Name: Clone
// Desc: Clone data
//--------------------------------------------------------------------------------------

ClassifierDataUsingMuscleTorqueZ* ClassifierDataUsingMuscleTorqueZ::Clone()
{
    ClassifierDataUsingMuscleTorqueZ* pClone = new ClassifierDataUsingMuscleTorqueZ( m_jointIndex );
    if ( pClone )
    {
        pClone->Copy( this );
    }
    return pClone;
}


//--------------------------------------------------------------------------------------
// Name: Update
// Desc: Update data
//--------------------------------------------------------------------------------------

VOID ClassifierDataUsingMusclePower::Update( const UINT uPlayerIdx, VOID* )
{
    // Check that skeleton is tracked
    const NUI_SKELETON_DATA* pSkeletonData = GetCurrentSkeleton( uPlayerIdx );

    if ( pSkeletonData->eTrackingState != NUI_SKELETON_TRACKED )
    {
        m_fValue[ uPlayerIdx ] = g_fInvalidValue;
        return;
    }

    if ( m_bRejectInferred )
    {
        if ( pSkeletonData->eSkeletonPositionTrackingState[ m_jointIndex ] != NUI_SKELETON_POSITION_TRACKED )
        {
            m_fValue[ uPlayerIdx ] = g_fInvalidValue;
            return;
        }
    }
    else
    {
        if ( pSkeletonData->eSkeletonPositionTrackingState[ m_jointIndex ] == NUI_SKELETON_POSITION_NOT_TRACKED )
        {
            m_fValue[ uPlayerIdx ] = g_fInvalidValue;
            return;
        }
    }

    m_fValue[ uPlayerIdx ] = MuscleDataGetLinearPower( &m_Muscles[ uPlayerIdx ].Muscles[ m_jointIndex ] );
}


//--------------------------------------------------------------------------------------
// Name: Clone
// Desc: Clone data
//--------------------------------------------------------------------------------------

ClassifierDataUsingMusclePower* ClassifierDataUsingMusclePower::Clone()
{
    ClassifierDataUsingMusclePower* pClone = new ClassifierDataUsingMusclePower( m_jointIndex );
    if ( pClone )
    {
        pClone->Copy( this );
    }
    return pClone;
}


//--------------------------------------------------------------------------------------
// Name: Update
// Desc: Update data
//--------------------------------------------------------------------------------------

VOID ClassifierDataUsingDiffMuscleForceX::Update( const UINT uPlayerIdx, VOID* )
{
    // This classifier type operates only on one keyframe
    const NUI_SKELETON_DATA* pSkeletonData = GetCurrentSkeleton( uPlayerIdx );

    if ( pSkeletonData->eTrackingState != NUI_SKELETON_TRACKED )
    {
        m_fValue[ uPlayerIdx ] = g_fInvalidValue;
        return;
    }

    // Get the tracking states for joints
    const NUI_SKELETON_POSITION_TRACKING_STATE jointTrackingState0 = pSkeletonData->eSkeletonPositionTrackingState[ m_jointIndices[ 0 ] ];
    const NUI_SKELETON_POSITION_TRACKING_STATE jointTrackingState1 = pSkeletonData->eSkeletonPositionTrackingState[ m_jointIndices[ 1 ] ];

    if ( m_bRejectInferred )
    {
        if ( jointTrackingState0 != NUI_SKELETON_POSITION_TRACKED ||
             jointTrackingState1 != NUI_SKELETON_POSITION_TRACKED )
        {
            m_fValue[ uPlayerIdx ] = g_fInvalidValue;
            return;
        }
    }
    else
    {
        if ( jointTrackingState0 == NUI_SKELETON_POSITION_NOT_TRACKED ||
             jointTrackingState1 == NUI_SKELETON_POSITION_NOT_TRACKED )
        {
            m_fValue[ uPlayerIdx ] = g_fInvalidValue;
            return;
        }
    }

    const FLOAT fNormalizeConstant0 = m_Muscles[ uPlayerIdx ].Muscles[ m_jointIndices[ 0 ] ].mTorqueNormalizationConstant;
    const FLOAT fNormalizeConstant1 = m_Muscles[ uPlayerIdx ].Muscles[ m_jointIndices[ 1 ] ].mTorqueNormalizationConstant;

    const XMVECTOR vDiff = ( m_Muscles[ uPlayerIdx ].Muscles[ m_jointIndices[ 0 ] ].mDynamicExternalForce / fNormalizeConstant0 )  -
                           ( m_Muscles[ uPlayerIdx ].Muscles[ m_jointIndices[ 1 ] ].mDynamicExternalForce / fNormalizeConstant1 );

    FLOAT fDiff = 0.0f;

    switch( m_Type )
    {
#ifdef ADD_TYPE_DIFF_MUSCLE_FORCE_X
    case TYPE_DIFF_MUSCLE_FORCE_X:
        fDiff = XMVectorGetX( vDiff );
        break;
#endif

#ifdef ADD_TYPE_DIFF_MUSCLE_FORCE_Y
    case TYPE_DIFF_MUSCLE_FORCE_Y:
        fDiff = XMVectorGetY( vDiff );
        break;
#endif

#ifdef ADD_TYPE_DIFF_MUSCLE_FORCE_Z
    case TYPE_DIFF_MUSCLE_FORCE_Z:
        fDiff = XMVectorGetZ( vDiff );
        break;
#endif
    }

    // This should automatically compile to __fsel() on Xbox
    fDiff = ( ( fNormalizeConstant0 - FLT_EPSILON ) >= 0.0f ) ? fDiff : g_fInvalidValue;
    m_fValue[ uPlayerIdx ] = ( ( fNormalizeConstant1 - FLT_EPSILON ) >= 0.0f ) ? fDiff : g_fInvalidValue;

}


//--------------------------------------------------------------------------------------
// Name: Clone
// Desc: Clone data
//--------------------------------------------------------------------------------------

ClassifierDataUsingDiffMuscleForceX* ClassifierDataUsingDiffMuscleForceX::Clone()
{
    ClassifierDataUsingDiffMuscleForceX* pClone = new ClassifierDataUsingDiffMuscleForceX( m_jointIndices[ 0 ], m_jointIndices[ 1 ] );
    if ( pClone )
    {
        pClone->Copy( this );
    }
    return pClone;
}


//--------------------------------------------------------------------------------------
// Name: Clone
// Desc: Clone data
//--------------------------------------------------------------------------------------

ClassifierDataUsingDiffMuscleForceY* ClassifierDataUsingDiffMuscleForceY::Clone()
{
    ClassifierDataUsingDiffMuscleForceY* pClone = new ClassifierDataUsingDiffMuscleForceY( m_jointIndices[ 0 ], m_jointIndices[ 1 ] );
    if ( pClone )
    {
        pClone->Copy( this );
    }
    return pClone;
}


//--------------------------------------------------------------------------------------
// Name: Clone
// Desc: Clone data
//--------------------------------------------------------------------------------------

ClassifierDataUsingDiffMuscleForceZ* ClassifierDataUsingDiffMuscleForceZ::Clone()
{
    ClassifierDataUsingDiffMuscleForceZ* pClone = new ClassifierDataUsingDiffMuscleForceZ( m_jointIndices[ 0 ], m_jointIndices[ 1 ] );
    if ( pClone )
    {
        pClone->Copy( this );
    }
    return pClone;
}


//--------------------------------------------------------------------------------------
// Name: Update
// Desc: Update data
//--------------------------------------------------------------------------------------

VOID ClassifierDataUsingBoneLengthChanges::Update( const UINT uPlayerIdx, VOID* )
{
    // Calculates the % bone difference between 3 frames in time
    const UINT uCurrentFrameIndex	= m_SkeletonDataHistory[ uPlayerIdx ].GetCurrentFrameIndex();
    UINT uPreviousFrameIndex	    = m_SkeletonDataHistory[ uPlayerIdx ].GetPreviousFrameIndex( uCurrentFrameIndex );
    uPreviousFrameIndex	            = m_SkeletonDataHistory[ uPlayerIdx ].GetPreviousFrameIndex( uPreviousFrameIndex );

    const NUI_SKELETON_DATA* pCurrentSkeletonData = m_SkeletonDataHistory[ uPlayerIdx ].GetAt( uCurrentFrameIndex );
    const NUI_SKELETON_DATA* pPreviousSkeletonData = m_SkeletonDataHistory[ uPlayerIdx ].GetAt( uPreviousFrameIndex );

    assert( pCurrentSkeletonData );
    assert( pPreviousSkeletonData );

    if ( pCurrentSkeletonData->eTrackingState != NUI_SKELETON_TRACKED ||
         pPreviousSkeletonData->eTrackingState != NUI_SKELETON_TRACKED )
    {
        m_fValue[ uPlayerIdx ] = g_fInvalidValue;
        return;
    }

    // Get the tracking state for joints
    const NUI_SKELETON_POSITION_TRACKING_STATE currentParentJointTrackingState  = pCurrentSkeletonData->eSkeletonPositionTrackingState[ m_jointIndices[ 0 ] ];
    const NUI_SKELETON_POSITION_TRACKING_STATE currentChildJointTrackingState   = pCurrentSkeletonData->eSkeletonPositionTrackingState[ m_jointIndices[ 1 ] ];
    const NUI_SKELETON_POSITION_TRACKING_STATE prevParentJointTrackingState     = pPreviousSkeletonData->eSkeletonPositionTrackingState[ m_jointIndices[ 0 ] ];
    const NUI_SKELETON_POSITION_TRACKING_STATE prevChildJointTrackingState      = pPreviousSkeletonData->eSkeletonPositionTrackingState[ m_jointIndices[ 1 ] ];

    if ( m_bRejectInferred )
    {
        if ( currentParentJointTrackingState != NUI_SKELETON_POSITION_TRACKED ||
             currentChildJointTrackingState != NUI_SKELETON_POSITION_TRACKED ||
             prevParentJointTrackingState != NUI_SKELETON_POSITION_TRACKED ||
             prevChildJointTrackingState != NUI_SKELETON_POSITION_TRACKED )
        {
            m_fValue[ uPlayerIdx ] = g_fInvalidValue;
            return;
        }
    }
    else
    {
        if ( currentParentJointTrackingState == NUI_SKELETON_POSITION_NOT_TRACKED ||
             currentChildJointTrackingState == NUI_SKELETON_POSITION_NOT_TRACKED ||
             prevParentJointTrackingState == NUI_SKELETON_POSITION_NOT_TRACKED ||
             prevChildJointTrackingState == NUI_SKELETON_POSITION_NOT_TRACKED )
        {
            m_fValue[ uPlayerIdx ] = g_fInvalidValue;
            return;
        }
    }

    // Get the bone lengths
    const XMVECTOR vCurrentBone         = pCurrentSkeletonData->SkeletonPositions[ m_jointIndices[ 0 ] ] - pCurrentSkeletonData->SkeletonPositions[ m_jointIndices[ 1 ] ];
    const XMVECTOR vPreviousBone        = pPreviousSkeletonData->SkeletonPositions[ m_jointIndices[ 0 ] ] - pPreviousSkeletonData->SkeletonPositions[ m_jointIndices[ 1 ] ];
    const FLOAT fCurrentBoneLength      = XMVectorGetX( XMVector3LengthEst( vCurrentBone ) );
    const FLOAT fPreviousBoneLength     = XMVectorGetX( XMVector3LengthEst( vPreviousBone ) );

    // return the change
    m_fValue[ uPlayerIdx ] = fCurrentBoneLength / fPreviousBoneLength;
}


//--------------------------------------------------------------------------------------
// Name: Clone
// Desc: Clone the data
//--------------------------------------------------------------------------------------

ClassifierDataUsingBoneLengthChanges* ClassifierDataUsingBoneLengthChanges::Clone()
{
    ClassifierDataUsingBoneLengthChanges* pClone = new ClassifierDataUsingBoneLengthChanges( m_jointIndices[ 0 ], m_jointIndices[ 1 ] );
    if ( pClone )
    {
        pClone->Copy( this );
    }
    return pClone;
}

}
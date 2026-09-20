//--------------------------------------------------------------------------------------
// ClassifierData.h
//
// This file defines all the features and classifier data that are used during the
// training process on the PC and the gesture detection and runtime on the Xbox 360.
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

#include <stdio.h>
#include "Common.h"
#include "RingBuffer.h"
#include "MuscleTracking.h"

namespace ATGGestureDetector
{

//--------------------------------------------------------------------------------------
// Each feature used for training has a define so that it is easy to switch on/off
// features when experimenting with different feature sets. E.g. you might be interested
// only in pose detection rather than gesture detection, in which case you could simply
// comment out all the time dependent features, such as velocity.
//--------------------------------------------------------------------------------------

#define ADD_TYPE_DIFF_POSITION_X 1
#define ADD_TYPE_DIFF_POSITION_Y 1
#define ADD_TYPE_DIFF_POSITION_Z 1
#define ADD_TYPE_ANGLE 1
#define ADD_TYPE_TIME_SPACE_ANGLE 1
#define ADD_TYPE_POSITION_SPEED 1
#define ADD_TYPE_POSITION_VELOCITY_X 1
#define ADD_TYPE_POSITION_VELOCITY_Y 1
#define ADD_TYPE_POSITION_VELOCITY_Z 1
#define ADD_TYPE_ANGLE_VELOCITY 1
#define ADD_TYPE_ANGLE_ACCELERATION 1
#define ADD_TYPE_MUSCLE_FORCES 1
#define ADD_TYPE_MUSCLE_TORQUES 1
#define ADD_TYPE_MUSCLE_POWER 1
#define ADD_TYPE_DIFF_MUSCLE_FORCE_X 1
#define ADD_TYPE_DIFF_MUSCLE_FORCE_Y 1
#define ADD_TYPE_DIFF_MUSCLE_FORCE_Z 1
#define ADD_TYPE_POSITION_SPEED_SQ 1
#define ADD_TYPE_POSITION_VELOCITYSQ_X 1
#define ADD_TYPE_POSITION_VELOCITYSQ_Y 1
#define ADD_TYPE_POSITION_VELOCITYSQ_Z 1
#define ADD_TYPE_POSITION_ACCELERATION 1
#define ADD_TYPE_POSITION_ACCELERATION_X 1
#define ADD_TYPE_POSITION_ACCELERATION_Y 1
#define ADD_TYPE_POSITION_ACCELERATION_Z 1
#define ADD_TYPE_BONE_LENGTH_CHANGES 1

// We can add many other features, e.g. based on radial basis functions, signmoid, etc.


//--------------------------------------------------------------------------------------
// Name: ClassifierData
// Desc: Base class for the classifier data
//--------------------------------------------------------------------------------------

class ClassifierData
{
public:
    enum EType
    {
#ifdef ADD_TYPE_DIFF_POSITION_X
        TYPE_DIFF_POSITION_X,
#endif
#ifdef ADD_TYPE_DIFF_POSITION_Y
        TYPE_DIFF_POSITION_Y,
#endif
#ifdef ADD_TYPE_DIFF_POSITION_Z
        TYPE_DIFF_POSITION_Z,
#endif
#ifdef ADD_TYPE_ANGLE
        TYPE_ANGLE,
#endif
#ifdef ADD_TYPE_TIME_SPACE_ANGLE
        TYPE_TIME_SPACE_ANGLE,
#endif
#ifdef ADD_TYPE_POSITION_SPEED
        TYPE_POSITION_SPEED,
#endif
#ifdef ADD_TYPE_POSITION_VELOCITY_X
        TYPE_POSITION_VELOCITY_X,
#endif
#ifdef ADD_TYPE_POSITION_VELOCITY_Y
        TYPE_POSITION_VELOCITY_Y,
#endif
#ifdef ADD_TYPE_POSITION_VELOCITY_Z
        TYPE_POSITION_VELOCITY_Z,
#endif
#ifdef ADD_TYPE_ANGLE_VELOCITY
        TYPE_ANGLE_VELOCITY,
#endif
#ifdef ADD_TYPE_ANGLE_ACCELERATION
        TYPE_ANGLE_ACCELERATION,
#endif
#ifdef ADD_TYPE_MUSCLE_FORCES
        TYPE_MUSCLE_FORCE_X,
        TYPE_MUSCLE_FORCE_Y,
        TYPE_MUSCLE_FORCE_Z,
#endif
#ifdef ADD_TYPE_MUSCLE_TORQUES
        TYPE_MUSCLE_TORQUE_X,
        TYPE_MUSCLE_TORQUE_Y,
        TYPE_MUSCLE_TORQUE_Z,
#endif
#ifdef ADD_TYPE_MUSCLE_POWER
        TYPE_MUSCLE_POWER,
#endif
#ifdef ADD_TYPE_DIFF_MUSCLE_FORCE_X
        TYPE_DIFF_MUSCLE_FORCE_X,
#endif
#ifdef ADD_TYPE_DIFF_MUSCLE_FORCE_Y
        TYPE_DIFF_MUSCLE_FORCE_Y,
#endif
#ifdef ADD_TYPE_DIFF_MUSCLE_FORCE_Z
        TYPE_DIFF_MUSCLE_FORCE_Z,
#endif
#ifdef ADD_TYPE_POSITION_VELOCITYSQ_X
        TYPE_POSITION_VELOCITYSQ_X,
#endif
#ifdef ADD_TYPE_POSITION_VELOCITYSQ_Y
        TYPE_POSITION_VELOCITYSQ_Y,
#endif
#ifdef ADD_TYPE_POSITION_VELOCITYSQ_Z
        TYPE_POSITION_VELOCITYSQ_Z,
#endif
#ifdef ADD_TYPE_POSITION_SPEED_SQ
        TYPE_POSITION_SPEED_SQ,
#endif
#ifdef ADD_TYPE_POSITION_ACCELERATION
        TYPE_POSITION_ACCELERATION,
#endif
#ifdef ADD_TYPE_POSITION_ACCELERATION_X
        TYPE_POSITION_ACCELERATION_X,
#endif
#ifdef ADD_TYPE_POSITION_ACCELERATION_Y
        TYPE_POSITION_ACCELERATION_Y,
#endif
#ifdef ADD_TYPE_POSITION_ACCELERATION_Z
        TYPE_POSITION_ACCELERATION_Z,
#endif
#ifdef ADD_TYPE_BONE_LENGTH_CHANGES
        TYPE_BONE_LENGTH_CHANGES,
#endif
        NUM_FEATURES
    };

    friend class DebugOutput;

public:
    ClassifierData();

    virtual VOID Update( const UINT, VOID* ) {}

    virtual HRESULT Read( FILE* pFile );
    virtual HRESULT Read( VOID* pBuffer );
    virtual HRESULT Write( FILE* pFile );

    virtual ClassifierData* Clone() { return NULL; }
    VOID Copy( ClassifierData* pSource );

    inline VOID SetValue( const UINT uPlayerIdx, const FLOAT fValue ) { m_fValue[ uPlayerIdx ] = fValue; }
    inline FLOAT GetValue( const UINT uPlayerIdx ) const { return m_fValue[ uPlayerIdx ]; }

    inline VOID SetID( const UINT uID ) { m_uID = uID; }
    inline UINT GetID() const { return m_uID; }

    inline VOID SetRejectInfferedJoints( const BOOL bRejectInferred ) { m_bRejectInferred = bRejectInferred; }
    inline BOOL GetRejectInferredJoints() const { return m_bRejectInferred; }

    static VOID Initialize();
    static ClassifierData* CreatNewInstance( FILE* pFile );
    static ClassifierData* CreatNewInstance( VOID* pBuffer );
    static VOID UpdateHistory( const UINT uPlayerIdx, const NUI_SKELETON_DATA* pSkeletonData, const FLOAT fDeltaTimeInSeconds, BOOL* bReset );

    __forceinline static NUI_SKELETON_DATA* GetCurrentSkeleton( const UINT uPlayerIdx ) { return m_SkeletonDataHistory[ uPlayerIdx ].GetCurrentSkeleton(); }

protected:
    FLOAT   m_fValue[ NUM_PLAYERS ];    // The value updated by the data to be used in threshold comparisons in classifier
    EType   m_Type;                     // The type of data
    UINT    m_uID;                      // Unique ID for each so that we can link this to the classifiers
    BOOL    m_bRejectInferred;          // Reject all inferred joints or use them?
    
    static RingBuffer   m_SkeletonDataHistory[ NUM_PLAYERS ];  // Ring buffer holding sliding window of skeleton data
    static MUSCLE_FRAME m_Muscles[ NUM_PLAYERS ];

    static ClassifierData* CreatNewInstance( const UINT uValue );
};


//--------------------------------------------------------------------------------------
// Name: ClassifierDataBaseClassForOneJoint
// Desc: Base class for classifier data operating on one joint
//--------------------------------------------------------------------------------------

class ClassifierDataBaseClassForOneJoint : public ClassifierData
{
public:
    ClassifierDataBaseClassForOneJoint( const NUI_SKELETON_POSITION_INDEX jointIndex0 = NUI_SKELETON_POSITION_COUNT );

    virtual HRESULT Read( FILE* pFile );
    virtual HRESULT Read( VOID* pBuffer );
    virtual HRESULT Write( FILE* pFile );

protected:
    NUI_SKELETON_POSITION_INDEX m_jointIndex;

    friend class DebugOutput;
};


//--------------------------------------------------------------------------------------
// Name: ClassifierDataBaseClassForTwoJoints
// Desc: Base class for classifier data operating on two joints
//--------------------------------------------------------------------------------------

class ClassifierDataBaseClassForTwoJoints : public ClassifierData
{
public:
    ClassifierDataBaseClassForTwoJoints( const NUI_SKELETON_POSITION_INDEX jointIndex0 = NUI_SKELETON_POSITION_COUNT,
                                         const NUI_SKELETON_POSITION_INDEX jointIndex1 = NUI_SKELETON_POSITION_COUNT );

    virtual HRESULT Read( FILE* pFile );
    virtual HRESULT Read( VOID* pBuffer );
    virtual HRESULT Write( FILE* pFile );

protected:
    NUI_SKELETON_POSITION_INDEX m_jointIndices[ 2 ];

    friend class DebugOutput;
};


//--------------------------------------------------------------------------------------
// Name: ClassifierDataBaseClassForThreeJoints
// Desc: Base class for classifier data operating on three joints
//--------------------------------------------------------------------------------------

class ClassifierDataBaseClassForThreeJoints : public ClassifierData
{
public:
    ClassifierDataBaseClassForThreeJoints( const NUI_SKELETON_POSITION_INDEX jointIndex0 = NUI_SKELETON_POSITION_COUNT,
                                           const NUI_SKELETON_POSITION_INDEX jointIndex1 = NUI_SKELETON_POSITION_COUNT,
                                           const NUI_SKELETON_POSITION_INDEX jointIndex2 = NUI_SKELETON_POSITION_COUNT );

    virtual HRESULT Read( FILE* pFile );
    virtual HRESULT Read( VOID* pBuffer );
    virtual HRESULT Write( FILE* pFile );

protected:
    NUI_SKELETON_POSITION_INDEX m_jointIndices[ 3 ];

    friend class DebugOutput;
};


//--------------------------------------------------------------------------------------
// Name: ClassifierDataUsingDiffPositionX
// Desc: Classifier data based on differences in X between two joints
//--------------------------------------------------------------------------------------

class ClassifierDataUsingDiffPositionX : public ClassifierDataBaseClassForTwoJoints
{
public:
    ClassifierDataUsingDiffPositionX( const NUI_SKELETON_POSITION_INDEX jointIndex0 = NUI_SKELETON_POSITION_COUNT,
                                      const NUI_SKELETON_POSITION_INDEX jointIndex1 = NUI_SKELETON_POSITION_COUNT )
                                        : ClassifierDataBaseClassForTwoJoints( jointIndex0, jointIndex1 )
    {
#ifdef ADD_TYPE_DIFF_POSITION_X
        m_Type = TYPE_DIFF_POSITION_X;
#endif
    }

    virtual VOID Update( const UINT uPlayerIdx, VOID* pData );
    virtual ClassifierDataUsingDiffPositionX* Clone();
};


//--------------------------------------------------------------------------------------
// Name: ClassifierDataUsingDiffPositionY
// Desc: Classifier data based on differences in Y between two joints
//--------------------------------------------------------------------------------------

class ClassifierDataUsingDiffPositionY : public ClassifierDataUsingDiffPositionX
{
public:
    ClassifierDataUsingDiffPositionY( const NUI_SKELETON_POSITION_INDEX jointIndex0 = NUI_SKELETON_POSITION_COUNT,
                                      const NUI_SKELETON_POSITION_INDEX jointIndex1 = NUI_SKELETON_POSITION_COUNT )
                                        : ClassifierDataUsingDiffPositionX( jointIndex0, jointIndex1 )
    {
#ifdef ADD_TYPE_DIFF_POSITION_Y
        m_Type = TYPE_DIFF_POSITION_Y;
#endif
    }

    virtual ClassifierDataUsingDiffPositionY* Clone();
};


//--------------------------------------------------------------------------------------
// Name: ClassifierDataUsingDiffPositionZ
// Desc: Classifier data based on differences in Z between two joints
//--------------------------------------------------------------------------------------

class ClassifierDataUsingDiffPositionZ : public ClassifierDataUsingDiffPositionX
{
public:
    ClassifierDataUsingDiffPositionZ( const NUI_SKELETON_POSITION_INDEX jointIndex0 = NUI_SKELETON_POSITION_COUNT,
                                      const NUI_SKELETON_POSITION_INDEX jointIndex1 = NUI_SKELETON_POSITION_COUNT )
                                        : ClassifierDataUsingDiffPositionX( jointIndex0, jointIndex1 )
    {
#ifdef ADD_TYPE_DIFF_POSITION_Z
        m_Type = TYPE_DIFF_POSITION_Z;
#endif
    }

    virtual ClassifierDataUsingDiffPositionZ* Clone();
};


//--------------------------------------------------------------------------------------
// Name: ClassifierDataUsingAngles
// Desc: Classifier data based on angle formed between three joints
//--------------------------------------------------------------------------------------

class ClassifierDataUsingAngles : public ClassifierDataBaseClassForThreeJoints
{
public:
    ClassifierDataUsingAngles( const NUI_SKELETON_POSITION_INDEX jointIndex0 = NUI_SKELETON_POSITION_COUNT,
                               const NUI_SKELETON_POSITION_INDEX jointIndex1 = NUI_SKELETON_POSITION_COUNT,
                               const NUI_SKELETON_POSITION_INDEX jointIndex2 = NUI_SKELETON_POSITION_COUNT )
                                : ClassifierDataBaseClassForThreeJoints( jointIndex0, jointIndex1, jointIndex2 )
    {
#ifdef ADD_TYPE_ANGLE
        m_Type = TYPE_ANGLE;
#endif
    }

    virtual VOID Update( const UINT uPlayerIdx, VOID* pData );
    virtual ClassifierDataUsingAngles* Clone();
};


//--------------------------------------------------------------------------------------
// Name: ClassifierDataUsingTimeSpaceAngles
// Desc: Classifier data based on angle formed between the same joint at different times,
//       e.g. the angle form by hand at frame 0, frame 1 and frame 2
//--------------------------------------------------------------------------------------

class ClassifierDataUsingTimeSpaceAngles : public ClassifierDataBaseClassForOneJoint
{
public:
    ClassifierDataUsingTimeSpaceAngles( const NUI_SKELETON_POSITION_INDEX jointIndex = NUI_SKELETON_POSITION_COUNT )
                                            : ClassifierDataBaseClassForOneJoint( jointIndex )
    {
#ifdef ADD_TYPE_TIME_SPACE_ANGLE
        m_Type = TYPE_TIME_SPACE_ANGLE;
#endif
    }

    virtual VOID Update( const UINT uPlayerIdx, VOID* pData );
    virtual ClassifierDataUsingTimeSpaceAngles* Clone();
};


//--------------------------------------------------------------------------------------
// Name: ClassifierDataUsingPositionSpeed
// Desc: Classifier data based on the speed of a joint
//--------------------------------------------------------------------------------------

class ClassifierDataUsingPositionSpeed : public ClassifierDataBaseClassForOneJoint
{
public:
    ClassifierDataUsingPositionSpeed( const NUI_SKELETON_POSITION_INDEX jointIndex = NUI_SKELETON_POSITION_COUNT )
                                        : ClassifierDataBaseClassForOneJoint( jointIndex )
    {
#ifdef ADD_TYPE_POSITION_SPEED
        m_Type = TYPE_POSITION_SPEED;
#endif
    }

    virtual VOID Update( const UINT uPlayerIdx, VOID* pData );
    virtual ClassifierDataUsingPositionSpeed* Clone();
};


//--------------------------------------------------------------------------------------
// Name: ClassifierDataUsingPositionSpeedSQ
// Desc: Classifier data based on the squared speed of a joint
//--------------------------------------------------------------------------------------

class ClassifierDataUsingPositionSpeedSQ : public ClassifierDataUsingPositionSpeed
{
public:
    ClassifierDataUsingPositionSpeedSQ( const NUI_SKELETON_POSITION_INDEX jointIndex = NUI_SKELETON_POSITION_COUNT )
                                        : ClassifierDataUsingPositionSpeed( jointIndex )
    {
#ifdef ADD_TYPE_POSITION_SPEED_SQ
        m_Type = TYPE_POSITION_SPEED_SQ;
#endif
    }

    virtual ClassifierDataUsingPositionSpeedSQ* Clone();
};


//--------------------------------------------------------------------------------------
// Name: ClassifierDataUsingPositionAcceleration
// Desc: Classifier data based on the acceleration of a joint
//--------------------------------------------------------------------------------------

class ClassifierDataUsingPositionAcceleration : public ClassifierDataBaseClassForOneJoint
{
public:
    ClassifierDataUsingPositionAcceleration( const NUI_SKELETON_POSITION_INDEX jointIndex = NUI_SKELETON_POSITION_COUNT )
                                                : ClassifierDataBaseClassForOneJoint( jointIndex )
    {
#ifdef ADD_TYPE_POSITION_ACCELERATION
        m_Type = TYPE_POSITION_ACCELERATION;
#endif
    }

    virtual VOID Update( const UINT uPlayerIdx, VOID* pData );
    virtual ClassifierDataUsingPositionAcceleration* Clone();
};


//--------------------------------------------------------------------------------------
// Name: ClassifierDataUsingPositionAccelerationX
// Desc: Classifier data based on the X acceleration of a joint
//--------------------------------------------------------------------------------------

class ClassifierDataUsingPositionAccelerationX : public ClassifierDataUsingPositionAcceleration
{
public:
    ClassifierDataUsingPositionAccelerationX( const NUI_SKELETON_POSITION_INDEX jointIndex = NUI_SKELETON_POSITION_COUNT )
                                                : ClassifierDataUsingPositionAcceleration( jointIndex )
    {
#ifdef ADD_TYPE_POSITION_ACCELERATION_X
        m_Type = TYPE_POSITION_ACCELERATION_X;
#endif
    }

    virtual ClassifierDataUsingPositionAccelerationX* Clone();
};


//--------------------------------------------------------------------------------------
// Name: ClassifierDataUsingPositionAccelerationY
// Desc: Classifier data based on the Y acceleration of a joint
//--------------------------------------------------------------------------------------

class ClassifierDataUsingPositionAccelerationY : public ClassifierDataUsingPositionAcceleration
{
public:
    ClassifierDataUsingPositionAccelerationY( const NUI_SKELETON_POSITION_INDEX jointIndex = NUI_SKELETON_POSITION_COUNT )
                                                : ClassifierDataUsingPositionAcceleration( jointIndex )
    {
#ifdef ADD_TYPE_POSITION_ACCELERATION_Y
        m_Type = TYPE_POSITION_ACCELERATION_Y;
#endif
    }

    virtual ClassifierDataUsingPositionAccelerationY* Clone();
};


//--------------------------------------------------------------------------------------
// Name: ClassifierDataUsingPositionAccelerationZ
// Desc: Classifier data based on the Z acceleration of a joint
//--------------------------------------------------------------------------------------

class ClassifierDataUsingPositionAccelerationZ : public ClassifierDataUsingPositionAcceleration
{
public:
    ClassifierDataUsingPositionAccelerationZ( const NUI_SKELETON_POSITION_INDEX jointIndex = NUI_SKELETON_POSITION_COUNT )
                                                : ClassifierDataUsingPositionAcceleration( jointIndex )
    {
#ifdef ADD_TYPE_POSITION_ACCELERATION_Z
        m_Type = TYPE_POSITION_ACCELERATION_Z;
#endif
    }

    virtual ClassifierDataUsingPositionAccelerationZ* Clone();
};


//--------------------------------------------------------------------------------------
// Name: ClassifierDataUsingPositionVelocityX
// Desc: Classifier data based on the velocity of a joint in X
//--------------------------------------------------------------------------------------

class ClassifierDataUsingPositionVelocityX : public ClassifierDataUsingPositionSpeed
{
public:
    ClassifierDataUsingPositionVelocityX( const NUI_SKELETON_POSITION_INDEX jointIndex = NUI_SKELETON_POSITION_COUNT )
                                            : ClassifierDataUsingPositionSpeed( jointIndex )
    {
#ifdef ADD_TYPE_POSITION_VELOCITY_X
        m_Type = TYPE_POSITION_VELOCITY_X;
#endif
    }

    virtual ClassifierDataUsingPositionVelocityX* Clone();
};


//--------------------------------------------------------------------------------------
// Name: ClassifierDataUsingPositionVelocityY
// Desc: Classifier data based on the velocity of a joint in Y
//--------------------------------------------------------------------------------------

class ClassifierDataUsingPositionVelocityY : public ClassifierDataUsingPositionSpeed
{
public:
    ClassifierDataUsingPositionVelocityY( const NUI_SKELETON_POSITION_INDEX jointIndex = NUI_SKELETON_POSITION_COUNT )
                                            : ClassifierDataUsingPositionSpeed( jointIndex )
    {
#ifdef ADD_TYPE_POSITION_VELOCITY_Y
        m_Type = TYPE_POSITION_VELOCITY_Y;
#endif
    }

    virtual ClassifierDataUsingPositionVelocityY* Clone();
};


//--------------------------------------------------------------------------------------
// Name: ClassifierDataUsingPositionVelocityZ
// Desc: Classifier data based on the velocity of a joint in Z
//--------------------------------------------------------------------------------------

class ClassifierDataUsingPositionVelocityZ : public ClassifierDataUsingPositionSpeed
{
public:
    ClassifierDataUsingPositionVelocityZ( const NUI_SKELETON_POSITION_INDEX jointIndex = NUI_SKELETON_POSITION_COUNT )
                                            : ClassifierDataUsingPositionSpeed( jointIndex )
    {
#ifdef ADD_TYPE_POSITION_VELOCITY_Z
        m_Type = TYPE_POSITION_VELOCITY_Z;
#endif
    }

    virtual ClassifierDataUsingPositionVelocityZ* Clone();
};


//--------------------------------------------------------------------------------------
// Name: ClassifierDataUsingPositionVelocitySQX
// Desc: Classifier data based on the squared velocity of a joint in X
//--------------------------------------------------------------------------------------

class ClassifierDataUsingPositionVelocitySQX : public ClassifierDataUsingPositionSpeed
{
public:
    ClassifierDataUsingPositionVelocitySQX( const NUI_SKELETON_POSITION_INDEX jointIndex = NUI_SKELETON_POSITION_COUNT )
                                            : ClassifierDataUsingPositionSpeed( jointIndex )
    {
#ifdef ADD_TYPE_POSITION_VELOCITYSQ_X
        m_Type = TYPE_POSITION_VELOCITYSQ_X;
#endif
    }

    virtual ClassifierDataUsingPositionVelocitySQX* Clone();
};


//--------------------------------------------------------------------------------------
// Name: ClassifierDataUsingPositionVelocitySQY
// Desc: Classifier data based on the squared velocity of a joint in Y
//--------------------------------------------------------------------------------------

class ClassifierDataUsingPositionVelocitySQY : public ClassifierDataUsingPositionSpeed
{
public:
    ClassifierDataUsingPositionVelocitySQY( const NUI_SKELETON_POSITION_INDEX jointIndex = NUI_SKELETON_POSITION_COUNT )
                                            : ClassifierDataUsingPositionSpeed( jointIndex )
    {
#ifdef ADD_TYPE_POSITION_VELOCITYSQ_Y
        m_Type = TYPE_POSITION_VELOCITYSQ_Y;
#endif
    }

    virtual ClassifierDataUsingPositionVelocitySQY* Clone();
};


//--------------------------------------------------------------------------------------
// Name: ClassifierDataUsingPositionVelocitySQZ
// Desc: Classifier data based on the squared velocity of a joint in Z
//--------------------------------------------------------------------------------------

class ClassifierDataUsingPositionVelocitySQZ : public ClassifierDataUsingPositionSpeed
{
public:
    ClassifierDataUsingPositionVelocitySQZ( const NUI_SKELETON_POSITION_INDEX jointIndex = NUI_SKELETON_POSITION_COUNT )
                                            : ClassifierDataUsingPositionSpeed( jointIndex )
    {
#ifdef ADD_TYPE_POSITION_VELOCITYSQ_Z
        m_Type = TYPE_POSITION_VELOCITYSQ_Z;
#endif
    }

    virtual ClassifierDataUsingPositionVelocitySQZ* Clone();
};


//--------------------------------------------------------------------------------------
// Name: ClassifierDataUsingAngleVelocities
// Desc: Classifier data based on the velocity of the angle between three joints
//--------------------------------------------------------------------------------------

class ClassifierDataUsingAngleVelocities : public ClassifierDataBaseClassForThreeJoints
{
public:
    ClassifierDataUsingAngleVelocities( const NUI_SKELETON_POSITION_INDEX jointIndex0 = NUI_SKELETON_POSITION_COUNT,
                                        const NUI_SKELETON_POSITION_INDEX jointIndex1 = NUI_SKELETON_POSITION_COUNT,
                                        const NUI_SKELETON_POSITION_INDEX jointIndex2 = NUI_SKELETON_POSITION_COUNT )
                                            : ClassifierDataBaseClassForThreeJoints( jointIndex0, jointIndex1, jointIndex2 )
    {
#ifdef ADD_TYPE_ANGLE_VELOCITY
        m_Type = TYPE_ANGLE_VELOCITY;
#endif
    }

    virtual VOID Update( const UINT uPlayerIdx, VOID* pData );
    virtual ClassifierDataUsingAngleVelocities* Clone();
};


//--------------------------------------------------------------------------------------
// Name: ClassifierDataUsingAngleAcceleration
// Desc: Classifier data based on the acceleration of the angle between three joints
//--------------------------------------------------------------------------------------

class ClassifierDataUsingAngleAcceleration : public ClassifierDataBaseClassForThreeJoints
{
public:
    ClassifierDataUsingAngleAcceleration( const NUI_SKELETON_POSITION_INDEX jointIndex0 = NUI_SKELETON_POSITION_COUNT,
                                          const NUI_SKELETON_POSITION_INDEX jointIndex1 = NUI_SKELETON_POSITION_COUNT,
                                          const NUI_SKELETON_POSITION_INDEX jointIndex2 = NUI_SKELETON_POSITION_COUNT )
                                            : ClassifierDataBaseClassForThreeJoints( jointIndex0, jointIndex1, jointIndex2 )
    {
#ifdef ADD_TYPE_ANGLE_ACCELERATION
        m_Type = TYPE_ANGLE_ACCELERATION;
#endif
    }

    virtual VOID Update( const UINT uPlayerIdx, VOID* pData );
    virtual ClassifierDataUsingAngleAcceleration* Clone();
};


//--------------------------------------------------------------------------------------
// Name: ClassifierDataUsingMuscleForceX
// Desc: Classifier data based on the muscle force in X
//--------------------------------------------------------------------------------------

class ClassifierDataUsingMuscleForceX : public ClassifierDataBaseClassForOneJoint
{
public:
    ClassifierDataUsingMuscleForceX( const NUI_SKELETON_POSITION_INDEX jointIndex = NUI_SKELETON_POSITION_COUNT )
                                        : ClassifierDataBaseClassForOneJoint( jointIndex )
    {
#ifdef ADD_TYPE_MUSCLE_FORCES
        m_Type = TYPE_MUSCLE_FORCE_X;
#endif
    }

    virtual VOID Update( const UINT uPlayerIdx, VOID* pData );
    virtual ClassifierDataUsingMuscleForceX* Clone();
};


//--------------------------------------------------------------------------------------
// Name: ClassifierDataUsingMuscleForceY
// Desc: Classifier data based on the muscle force in Y
//--------------------------------------------------------------------------------------

class ClassifierDataUsingMuscleForceY : public ClassifierDataUsingMuscleForceX
{
public:
    ClassifierDataUsingMuscleForceY( const NUI_SKELETON_POSITION_INDEX jointIndex = NUI_SKELETON_POSITION_COUNT )
                                        : ClassifierDataUsingMuscleForceX( jointIndex )
    {
#ifdef ADD_TYPE_MUSCLE_FORCES
        m_Type = TYPE_MUSCLE_FORCE_Y;
#endif
    }

    virtual ClassifierDataUsingMuscleForceY* Clone();
};


//--------------------------------------------------------------------------------------
// Name: ClassifierDataUsingMuscleForceZ
// Desc: Classifier data based on the muscle force in Z
//--------------------------------------------------------------------------------------

class ClassifierDataUsingMuscleForceZ : public ClassifierDataUsingMuscleForceX
{
public:
    ClassifierDataUsingMuscleForceZ( const NUI_SKELETON_POSITION_INDEX jointIndex = NUI_SKELETON_POSITION_COUNT )
                                        : ClassifierDataUsingMuscleForceX( jointIndex )
    {
#ifdef ADD_TYPE_MUSCLE_FORCES
        m_Type = TYPE_MUSCLE_FORCE_Z;
#endif
    }

    virtual ClassifierDataUsingMuscleForceZ* Clone();
};


//--------------------------------------------------------------------------------------
// Name: ClassifierDataUsingMuscleTorqueX
// Desc: Classifier data based on the muscle torque in X
//--------------------------------------------------------------------------------------

class ClassifierDataUsingMuscleTorqueX : public ClassifierDataBaseClassForOneJoint
{
public:
    ClassifierDataUsingMuscleTorqueX( const NUI_SKELETON_POSITION_INDEX jointIndex = NUI_SKELETON_POSITION_COUNT )
                                        : ClassifierDataBaseClassForOneJoint( jointIndex )
    {
#ifdef ADD_TYPE_MUSCLE_TORQUES
        m_Type = TYPE_MUSCLE_TORQUE_X;
#endif
    }

    virtual VOID Update( const UINT uPlayerIdx, VOID* pData );
    virtual ClassifierDataUsingMuscleTorqueX* Clone();
};


//--------------------------------------------------------------------------------------
// Name: ClassifierDataUsingMuscleTorqueY
// Desc: Classifier data based on the muscle torque in Y
//--------------------------------------------------------------------------------------

class ClassifierDataUsingMuscleTorqueY : public ClassifierDataUsingMuscleTorqueX
{
public:
    ClassifierDataUsingMuscleTorqueY( const NUI_SKELETON_POSITION_INDEX jointIndex = NUI_SKELETON_POSITION_COUNT )
                                        : ClassifierDataUsingMuscleTorqueX( jointIndex )
    {
#ifdef ADD_TYPE_MUSCLE_TORQUES
        m_Type = TYPE_MUSCLE_TORQUE_Y;
#endif
    }

    virtual ClassifierDataUsingMuscleTorqueY* Clone();
};


//--------------------------------------------------------------------------------------
// Name: ClassifierDataUsingMuscleTorqueZ
// Desc: Classifier data based on the muscle torque in Z
//--------------------------------------------------------------------------------------

class ClassifierDataUsingMuscleTorqueZ : public ClassifierDataUsingMuscleTorqueX
{
public:
    ClassifierDataUsingMuscleTorqueZ( const NUI_SKELETON_POSITION_INDEX jointIndex = NUI_SKELETON_POSITION_COUNT )
                                        : ClassifierDataUsingMuscleTorqueX( jointIndex )
    {
#ifdef ADD_TYPE_MUSCLE_TORQUES
        m_Type = TYPE_MUSCLE_TORQUE_Z;
#endif
    }

    virtual ClassifierDataUsingMuscleTorqueZ* Clone();
};


//--------------------------------------------------------------------------------------
// Name: ClassifierDataUsingMusclePower
// Desc: Classifier data based on the muscle power
//--------------------------------------------------------------------------------------

class ClassifierDataUsingMusclePower : public ClassifierDataBaseClassForOneJoint
{
public:
    ClassifierDataUsingMusclePower( const NUI_SKELETON_POSITION_INDEX jointIndex = NUI_SKELETON_POSITION_COUNT )
                                        : ClassifierDataBaseClassForOneJoint( jointIndex )
    {
#ifdef ADD_TYPE_MUSCLE_POWER
        m_Type = TYPE_MUSCLE_POWER;
#endif        
    }

    virtual VOID Update( const UINT uPlayerIdx, VOID* pData );
    virtual ClassifierDataUsingMusclePower* Clone();
};


//--------------------------------------------------------------------------------------
// Name: ClassifierDataUsingDiffMuscleForceX
// Desc: Classifier data based on the differences in muscle force in X between two joints
//--------------------------------------------------------------------------------------

class ClassifierDataUsingDiffMuscleForceX : public ClassifierDataBaseClassForTwoJoints
{
public:
    ClassifierDataUsingDiffMuscleForceX( const NUI_SKELETON_POSITION_INDEX jointIndex0 = NUI_SKELETON_POSITION_COUNT,
                                         const NUI_SKELETON_POSITION_INDEX jointIndex1 = NUI_SKELETON_POSITION_COUNT )
                                            : ClassifierDataBaseClassForTwoJoints( jointIndex0, jointIndex1 )
    {
#ifdef ADD_TYPE_DIFF_MUSCLE_FORCE_X
        m_Type = TYPE_DIFF_MUSCLE_FORCE_X;
#endif
    }

    virtual VOID Update( const UINT uPlayerIdx, VOID* pData );
    virtual ClassifierDataUsingDiffMuscleForceX* Clone();
};


//--------------------------------------------------------------------------------------
// Name: ClassifierDataUsingDiffMuscleForceY
// Desc: Classifier data based on the differences in muscle force in Y between two joints
//--------------------------------------------------------------------------------------

class ClassifierDataUsingDiffMuscleForceY : public ClassifierDataUsingDiffMuscleForceX
{
public:
    ClassifierDataUsingDiffMuscleForceY( const NUI_SKELETON_POSITION_INDEX jointIndex0 = NUI_SKELETON_POSITION_COUNT,
                                         const NUI_SKELETON_POSITION_INDEX jointIndex1 = NUI_SKELETON_POSITION_COUNT )
                                            : ClassifierDataUsingDiffMuscleForceX( jointIndex0, jointIndex1 )
    {
#ifdef ADD_TYPE_DIFF_MUSCLE_FORCE_Y
        m_Type = TYPE_DIFF_MUSCLE_FORCE_Y;
#endif
    }

    virtual ClassifierDataUsingDiffMuscleForceY* Clone();
};


//--------------------------------------------------------------------------------------
// Name: ClassifierDataUsingDiffMuscleForceZ
// Desc: Classifier data based on the differences in muscle force in Z between two joints
//--------------------------------------------------------------------------------------

class ClassifierDataUsingDiffMuscleForceZ : public ClassifierDataUsingDiffMuscleForceX
{
public:
    ClassifierDataUsingDiffMuscleForceZ( const NUI_SKELETON_POSITION_INDEX jointIndex0 = NUI_SKELETON_POSITION_COUNT,
                                         const NUI_SKELETON_POSITION_INDEX jointIndex1 = NUI_SKELETON_POSITION_COUNT )
                                            : ClassifierDataUsingDiffMuscleForceX( jointIndex0, jointIndex1 )
    {
#ifdef ADD_TYPE_DIFF_MUSCLE_FORCE_Z
        m_Type = TYPE_DIFF_MUSCLE_FORCE_Z;
#endif
    }

    virtual ClassifierDataUsingDiffMuscleForceZ* Clone();
};


//--------------------------------------------------------------------------------------
// Name: ClassifierDataUsingBoneLengthChanges
// Desc: Classifier data based on bone length changes over time
//--------------------------------------------------------------------------------------

class ClassifierDataUsingBoneLengthChanges : public ClassifierDataBaseClassForTwoJoints
{
public:
    ClassifierDataUsingBoneLengthChanges( const NUI_SKELETON_POSITION_INDEX jointIndex0 = NUI_SKELETON_POSITION_COUNT,
                                          const NUI_SKELETON_POSITION_INDEX jointIndex1 = NUI_SKELETON_POSITION_COUNT )
                                            : ClassifierDataBaseClassForTwoJoints( jointIndex0, jointIndex1 )
    {
#ifdef ADD_TYPE_BONE_LENGTH_CHANGES
        m_Type = TYPE_BONE_LENGTH_CHANGES;
#endif
    }

    virtual VOID Update( const UINT uPlayerIdx, VOID* pData );
    virtual ClassifierDataUsingBoneLengthChanges* Clone();
};

}
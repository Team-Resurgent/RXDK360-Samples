//--------------------------------------------------------------------------------------
// AnimationAdapter.h
//
// Performs necessary conversion from BVH to NUI skeleton. We simply remap BVH joints
// to NUI joints, and resample the whole BVH animation into N 30 FPS NUI frames
// 
//
// Advanced Technology Group
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once


#include <xtl.h>
#include <NuiApi.h>
#include "BVHAnimation.h"

//--------------------------------------------------------------------------------------
// Name: NuiBoneName
// Desc: NUI bones enum. See comments for what joints are connected
//--------------------------------------------------------------------------------------
enum NuiBoneName
{
    NUI_BONE_PELVIS,        // HIP_CENTER -> SPINE
    NUI_BONE_SPINE,         // SPINE -> SHOULDER_CENTER
    NUI_BONE_HEAD,          // SHOULDER_CENTER -> HEAD
    NUI_BONE_COLLAR_LEFT,   // SHOULDER_CENTER -> SHOULDER_LEFT
    NUI_BONE_SHOULDER_LEFT, // SHOULDER_LEFT -> ELBOW_LEFT
    NUI_BONE_FOREARM_LEFT,  // ELBOW_LEFT -> WRIST_LEFT
    NUI_BONE_HAND_LEFT,     // WRIST_LEFT -> HAND_LEFT
    NUI_BONE_COLLAR_RIGHT,  // SHOULDER_CENTER -> SHOULDER_RIGHT
    NUI_BONE_SHOULDER_RIGHT,// SHOULDER_RIGHT -> ELBOW_RIGHT
    NUI_BONE_FOREARM_RIGHT, // ELBOW_RIGHT -> WRIST_RIGHT
    NUI_BONE_HAND_RIGHT,    // WRIST_RIGHT -> HAND_RIGHT
    NUI_BONE_PELVIS_LEFT,   // HIP_CENTER -> HIP_LEFT
    NUI_BONE_HIP_LEFT,      // HIP_LEFT -> KNEE_LEFT
    NUI_BONE_SHIN_LEFT,     // KNEE_LEFT -> ANKLE_LEFT
    NUI_BONE_FOOT_LEFT,     // ANKLE_LEFT -> FOOT_LEFT
    NUI_BONE_PELVIS_RIGHT,  // HIP_CENTER -> HIP_RIGHT
    NUI_BONE_HIP_RIGHT,     // HIP_RIGHT -> KNEE_RIGHT
    NUI_BONE_SHIN_RIGHT,    // KNEE_RIGHT -> ANKLE_RIGHT
    NUI_BONE_FOOT_RIGHT,    // ANKLE_RIGHT -> FOOT_RIGHT
    NUI_BONE_COUNT          
};


//--------------------------------------------------------------------------------------
// Name: NuiBone
// Desc: Used to store animation bone length
//--------------------------------------------------------------------------------------
struct NuiBone
{
    NuiBoneName    m_name;                   // BONE_COUNT if not found in the source anim
    FLOAT          m_fAnimationLength;       // bone length from the animation file
};

//--------------------------------------------------------------------------------------
// Name: AnimationAdapter
// Desc: Retargets BVH animation to NUI animation, stores joints offsets for each frame
//--------------------------------------------------------------------------------------
class AnimationAdapter
{
public:
    AnimationAdapter() :    m_pFrames( NULL )
    {
    }

    ~AnimationAdapter()
    {
        Clear();
    }

    BOOL        SetAnimation( const BVHAnimation* anim, BOOL bMirror );
    FLOAT       GetFrameDuration() const;
    UINT        GetNumFrames() const;
    FLOAT       GetDefaultBoneLength( UINT i ) const;
    const XMVECTOR* GetJointOffsetsForFrame( UINT uFrame ) const;
    void        Clear();

    static const NUI_SKELETON_POSITION_INDEX    ms_NuiBonesEndJoints[ NUI_BONE_COUNT ];
    static const NUI_SKELETON_POSITION_INDEX    ms_NuiJointParents[ NUI_SKELETON_POSITION_COUNT ];
    static const NuiBoneName                    ms_NuiJointToBoneName[ NUI_SKELETON_POSITION_COUNT ];
private:
    
    struct Frame
    {
        XMVECTOR    m_jointsOffsets[ NUI_SKELETON_POSITION_COUNT ];
    };
    
    
    // animation data
    Frame*              m_pFrames;
    NuiBone             m_nuiBones[ NUI_BONE_COUNT ];
    FLOAT               m_fFrameDuration;
    UINT                m_uNumFrames;

    void        CalculateNormalizedJointsOffsets(   XMVECTOR* pRetargetedJoints,
                                                    const XMVECTOR* pWorldPositions,
                                                    const UINT* nuiJointToAnimNodeIndex,
                                                    BOOL bMirror ) const;
    static BVHAnimation::NodeName NuiJointToBvhNodeName( NUI_SKELETON_POSITION_INDEX n, BOOL bMirror );
};

//--------------------------------------------------------------------------------------
// AnimationAdapter.cpp
//
// Performs necessary conversion from BVH to NUI skeleton. We simply remap BVH joints
// to NUI joints, and resample the whole BVH animation into N 30 FPS NUI frames
//
// Advanced Technology Group
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include <xtl.h>
#include <assert.h>
#include <vector>

#include <AtgUtil.h>

#include "AnimationAdapter.h"


// make sure it matches NuiBoneName
const NUI_SKELETON_POSITION_INDEX   AnimationAdapter::ms_NuiBonesEndJoints[ NUI_BONE_COUNT ] =
{
    NUI_SKELETON_POSITION_SPINE,                        // NUI_BONE_PELVIS,        // HIP_CENTER -> SPINE
    NUI_SKELETON_POSITION_SHOULDER_CENTER,              // NUI_BONE_SPINE,         // SPINE -> SHOULDER_CENTER
    NUI_SKELETON_POSITION_HEAD,                         // NUI_BONE_HEAD,          // SHOULDER_CENTER -> HEAD
    NUI_SKELETON_POSITION_SHOULDER_LEFT,                // NUI_BONE_COLLAR_LEFT,   // SHOULDER_CENTER -> SHOULDER_LEFT
    NUI_SKELETON_POSITION_ELBOW_LEFT,                   // NUI_BONE_SHOULDER_LEFT, // SHOULDER_LEFT -> ELBOW_LEFT
    NUI_SKELETON_POSITION_WRIST_LEFT,                   // NUI_BONE_FOREARM_LEFT,  // ELBOW_LEFT -> WRIST_LEFT
    NUI_SKELETON_POSITION_HAND_LEFT,                    // NUI_BONE_HAND_LEFT,     // WRIST_LEFT -> HAND_LEFT
    NUI_SKELETON_POSITION_SHOULDER_RIGHT,               // NUI_BONE_COLLAR_RIGHT,  // SHOULDER_CENTER -> SHOULDER_RIGHT
    NUI_SKELETON_POSITION_ELBOW_RIGHT,                  // NUI_BONE_SHOULDER_RIGHT,// SHOULDER_RIGHT -> ELBOW_RIGHT
    NUI_SKELETON_POSITION_WRIST_RIGHT,                  // NUI_BONE_FOREARM_RIGHT, // ELBOW_RIGHT -> WRIST_RIGHT
    NUI_SKELETON_POSITION_HAND_RIGHT,                   // NUI_BONE_HAND_RIGHT,    // WRIST_RIGHT -> HAND_RIGHT
    NUI_SKELETON_POSITION_HIP_LEFT,                     // NUI_BONE_PELVIS_LEFT,   // HIP_CENTER -> HIP_LEFT
    NUI_SKELETON_POSITION_KNEE_LEFT,                    // NUI_BONE_HIP_LEFT,      // HIP_LEFT -> KNEE_LEFT
    NUI_SKELETON_POSITION_ANKLE_LEFT,                   // NUI_BONE_SHIN_LEFT,     // KNEE_LEFT -> ANKLE_LEFT
    NUI_SKELETON_POSITION_FOOT_LEFT,                    // NUI_BONE_FOOT_LEFT,     // ANKLE_LEFT -> FOOT_LEFT
    NUI_SKELETON_POSITION_HIP_RIGHT,                    // NUI_BONE_PELVIS_RIGHT,  // HIP_CENTER -> HIP_RIGHT
    NUI_SKELETON_POSITION_KNEE_RIGHT,                   // NUI_BONE_HIP_RIGHT,     // HIP_RIGHT -> KNEE_RIGHT
    NUI_SKELETON_POSITION_ANKLE_RIGHT,                  // NUI_BONE_SHIN_RIGHT,    // KNEE_RIGHT -> ANKLE_RIGHT
    NUI_SKELETON_POSITION_FOOT_RIGHT,                   // NUI_BONE_FOOT_RIGHT,    // ANKLE_RIGHT -> FOOT_RIGHT
};

// make sure it matches NuiBoneName
const NuiBoneName   AnimationAdapter::ms_NuiJointToBoneName[ NUI_SKELETON_POSITION_COUNT ] =
{
    NUI_BONE_COUNT,             // NUI_SKELETON_POSITION_HIP_CENTER = 0,
    NUI_BONE_PELVIS,            // NUI_SKELETON_POSITION_SPINE,
    NUI_BONE_SPINE,             // NUI_SKELETON_POSITION_SHOULDER_CENTER,
    NUI_BONE_HEAD,              // NUI_SKELETON_POSITION_HEAD,
    NUI_BONE_COLLAR_LEFT,       // NUI_SKELETON_POSITION_SHOULDER_LEFT,
    NUI_BONE_SHOULDER_LEFT,     // NUI_SKELETON_POSITION_ELBOW_LEFT,
    NUI_BONE_FOREARM_LEFT,      // NUI_SKELETON_POSITION_WRIST_LEFT,
    NUI_BONE_HAND_LEFT,         // NUI_SKELETON_POSITION_HAND_LEFT,
    NUI_BONE_COLLAR_RIGHT,      // NUI_SKELETON_POSITION_SHOULDER_RIGHT,
    NUI_BONE_SHOULDER_RIGHT,    // NUI_SKELETON_POSITION_ELBOW_RIGHT,
    NUI_BONE_FOREARM_RIGHT,     // NUI_SKELETON_POSITION_WRIST_RIGHT,
    NUI_BONE_HAND_RIGHT,        // NUI_SKELETON_POSITION_HAND_RIGHT,
    NUI_BONE_PELVIS_LEFT,       // NUI_SKELETON_POSITION_HIP_LEFT,
    NUI_BONE_HIP_LEFT,          // NUI_SKELETON_POSITION_KNEE_LEFT,
    NUI_BONE_SHIN_LEFT,         // NUI_SKELETON_POSITION_ANKLE_LEFT,
    NUI_BONE_FOOT_LEFT,         // NUI_SKELETON_POSITION_FOOT_LEFT,
    NUI_BONE_PELVIS_RIGHT,      // NUI_SKELETON_POSITION_HIP_RIGHT,
    NUI_BONE_HIP_RIGHT,         // NUI_SKELETON_POSITION_KNEE_RIGHT,
    NUI_BONE_SHIN_RIGHT,        // NUI_SKELETON_POSITION_ANKLE_RIGHT,
    NUI_BONE_FOOT_RIGHT,        // NUI_SKELETON_POSITION_FOOT_RIGHT
};

// parents for the bones
const NUI_SKELETON_POSITION_INDEX   AnimationAdapter::ms_NuiJointParents[ NUI_SKELETON_POSITION_COUNT ] =
{
    NUI_SKELETON_POSITION_COUNT,            // NUI_SKELETON_POSITION_HIP_CENTER = 0,
    NUI_SKELETON_POSITION_HIP_CENTER,       // NUI_SKELETON_POSITION_SPINE,
    NUI_SKELETON_POSITION_SPINE,            // NUI_SKELETON_POSITION_SHOULDER_CENTER,
    NUI_SKELETON_POSITION_SHOULDER_CENTER,  // NUI_SKELETON_POSITION_HEAD,
    NUI_SKELETON_POSITION_SHOULDER_CENTER,  // NUI_SKELETON_POSITION_SHOULDER_LEFT,
    NUI_SKELETON_POSITION_SHOULDER_LEFT,    // NUI_SKELETON_POSITION_ELBOW_LEFT,
    NUI_SKELETON_POSITION_ELBOW_LEFT,       // NUI_SKELETON_POSITION_WRIST_LEFT,
    NUI_SKELETON_POSITION_WRIST_LEFT,       // NUI_SKELETON_POSITION_HAND_LEFT,
    NUI_SKELETON_POSITION_SHOULDER_CENTER,  // NUI_SKELETON_POSITION_SHOULDER_RIGHT,
    NUI_SKELETON_POSITION_SHOULDER_RIGHT,   // NUI_SKELETON_POSITION_ELBOW_RIGHT,
    NUI_SKELETON_POSITION_ELBOW_RIGHT,      // NUI_SKELETON_POSITION_WRIST_RIGHT,
    NUI_SKELETON_POSITION_WRIST_RIGHT,      // NUI_SKELETON_POSITION_HAND_RIGHT,
    NUI_SKELETON_POSITION_HIP_CENTER,       // NUI_SKELETON_POSITION_HIP_LEFT,
    NUI_SKELETON_POSITION_HIP_LEFT,         // NUI_SKELETON_POSITION_KNEE_LEFT,
    NUI_SKELETON_POSITION_KNEE_LEFT,        // NUI_SKELETON_POSITION_ANKLE_LEFT,
    NUI_SKELETON_POSITION_ANKLE_LEFT,       // NUI_SKELETON_POSITION_FOOT_LEFT,
    NUI_SKELETON_POSITION_HIP_CENTER,       // NUI_SKELETON_POSITION_HIP_RIGHT,
    NUI_SKELETON_POSITION_HIP_RIGHT,        // NUI_SKELETON_POSITION_KNEE_RIGHT,
    NUI_SKELETON_POSITION_KNEE_RIGHT,       // NUI_SKELETON_POSITION_ANKLE_RIGHT,
    NUI_SKELETON_POSITION_ANKLE_RIGHT,      // NUI_SKELETON_POSITION_FOOT_RIGHT
};




//--------------------------------------------------------------------------------------
// Name: NuiJointToBvhNodeName()
// Desc: Given NUI bone index remap it onto the BVH skeleton index
//--------------------------------------------------------------------------------------
BVHAnimation::NodeName AnimationAdapter::NuiJointToBvhNodeName( NUI_SKELETON_POSITION_INDEX n, BOOL bMirror )
{
    if( bMirror )
    {
        switch( n )
        {
            default: return BVHAnimation::BVH_NODE_COUNT;

            case     NUI_SKELETON_POSITION_HIP_CENTER:         return BVHAnimation::BVH_NODE_HIP_CENTER_RIGHT;
            case     NUI_SKELETON_POSITION_SPINE:              return BVHAnimation::BVH_NODE_SPINE;
            case     NUI_SKELETON_POSITION_SHOULDER_CENTER:    return BVHAnimation::BVH_NODE_SHOULDER_CENTER;
            case     NUI_SKELETON_POSITION_HEAD:               return BVHAnimation::BVH_NODE_HEAD;
            case     NUI_SKELETON_POSITION_SHOULDER_RIGHT:     return BVHAnimation::BVH_NODE_SHOULDER_LEFT;
            case     NUI_SKELETON_POSITION_ELBOW_RIGHT:        return BVHAnimation::BVH_NODE_ELBOW_LEFT;
            case     NUI_SKELETON_POSITION_WRIST_RIGHT:        return BVHAnimation::BVH_NODE_WRIST_LEFT;
            case     NUI_SKELETON_POSITION_HAND_RIGHT:         return BVHAnimation::BVH_NODE_HAND_LEFT;
            case     NUI_SKELETON_POSITION_SHOULDER_LEFT:      return BVHAnimation::BVH_NODE_SHOULDER_RIGHT;
            case     NUI_SKELETON_POSITION_ELBOW_LEFT:         return BVHAnimation::BVH_NODE_ELBOW_RIGHT;
            case     NUI_SKELETON_POSITION_WRIST_LEFT:         return BVHAnimation::BVH_NODE_WRIST_RIGHT;
            case     NUI_SKELETON_POSITION_HAND_LEFT:          return BVHAnimation::BVH_NODE_HAND_RIGHT;
            case     NUI_SKELETON_POSITION_HIP_RIGHT:          return BVHAnimation::BVH_NODE_HIP_LEFT;
            case     NUI_SKELETON_POSITION_KNEE_RIGHT:         return BVHAnimation::BVH_NODE_KNEE_LEFT;
            case     NUI_SKELETON_POSITION_ANKLE_RIGHT:        return BVHAnimation::BVH_NODE_ANKLE_LEFT;
            case     NUI_SKELETON_POSITION_FOOT_RIGHT:         return BVHAnimation::BVH_NODE_FOOT_LEFT;
            case     NUI_SKELETON_POSITION_HIP_LEFT:           return BVHAnimation::BVH_NODE_HIP_RIGHT;
            case     NUI_SKELETON_POSITION_KNEE_LEFT:          return BVHAnimation::BVH_NODE_KNEE_RIGHT;
            case     NUI_SKELETON_POSITION_ANKLE_LEFT:         return BVHAnimation::BVH_NODE_ANKLE_RIGHT;
            case     NUI_SKELETON_POSITION_FOOT_LEFT:          return BVHAnimation::BVH_NODE_FOOT_RIGHT;
        }
    } else
    {
        switch( n )
        {
            default: return BVHAnimation::BVH_NODE_COUNT;

            case     NUI_SKELETON_POSITION_HIP_CENTER:          return BVHAnimation::BVH_NODE_HIP_CENTER_RIGHT;
            case     NUI_SKELETON_POSITION_SPINE:               return BVHAnimation::BVH_NODE_SPINE;
            case     NUI_SKELETON_POSITION_SHOULDER_CENTER:     return BVHAnimation::BVH_NODE_SHOULDER_CENTER;
            case     NUI_SKELETON_POSITION_HEAD:                return BVHAnimation::BVH_NODE_HEAD;
            case     NUI_SKELETON_POSITION_SHOULDER_LEFT:       return BVHAnimation::BVH_NODE_SHOULDER_LEFT;
            case     NUI_SKELETON_POSITION_ELBOW_LEFT:          return BVHAnimation::BVH_NODE_ELBOW_LEFT;
            case     NUI_SKELETON_POSITION_WRIST_LEFT:          return BVHAnimation::BVH_NODE_WRIST_LEFT;
            case     NUI_SKELETON_POSITION_HAND_LEFT:           return BVHAnimation::BVH_NODE_HAND_LEFT;
            case     NUI_SKELETON_POSITION_SHOULDER_RIGHT:      return BVHAnimation::BVH_NODE_SHOULDER_RIGHT;
            case     NUI_SKELETON_POSITION_ELBOW_RIGHT:         return BVHAnimation::BVH_NODE_ELBOW_RIGHT;
            case     NUI_SKELETON_POSITION_WRIST_RIGHT:         return BVHAnimation::BVH_NODE_WRIST_RIGHT;
            case     NUI_SKELETON_POSITION_HAND_RIGHT:          return BVHAnimation::BVH_NODE_HAND_RIGHT;
            case     NUI_SKELETON_POSITION_HIP_LEFT:            return BVHAnimation::BVH_NODE_HIP_LEFT;
            case     NUI_SKELETON_POSITION_KNEE_LEFT:           return BVHAnimation::BVH_NODE_KNEE_LEFT;
            case     NUI_SKELETON_POSITION_ANKLE_LEFT:          return BVHAnimation::BVH_NODE_ANKLE_LEFT;
            case     NUI_SKELETON_POSITION_FOOT_LEFT:           return BVHAnimation::BVH_NODE_FOOT_LEFT;
            case     NUI_SKELETON_POSITION_HIP_RIGHT:           return BVHAnimation::BVH_NODE_HIP_RIGHT;
            case     NUI_SKELETON_POSITION_KNEE_RIGHT:          return BVHAnimation::BVH_NODE_KNEE_RIGHT;
            case     NUI_SKELETON_POSITION_ANKLE_RIGHT:         return BVHAnimation::BVH_NODE_ANKLE_RIGHT;
            case     NUI_SKELETON_POSITION_FOOT_RIGHT:          return BVHAnimation::BVH_NODE_FOOT_RIGHT;
        }
    }
}



//--------------------------------------------------------------------------------------
// Name: Clear()
// Desc: Reset precalculated frames
//--------------------------------------------------------------------------------------
void    AnimationAdapter::Clear()
{
    delete[] m_pFrames;
    m_pFrames = NULL;
}

//--------------------------------------------------------------------------------------
// Name: GetFrameDuration()
// Desc: Returns frame duration
//--------------------------------------------------------------------------------------
FLOAT   AnimationAdapter::GetFrameDuration() const
{
    return m_fFrameDuration;
}

//--------------------------------------------------------------------------------------
// Name: GetNumFrames()
// Desc: Returns number of frames
//--------------------------------------------------------------------------------------
UINT    AnimationAdapter::GetNumFrames() const
{
    return m_uNumFrames;
}

//--------------------------------------------------------------------------------------
// Name: GetDefaultBoneLength()
// Desc: Returns bone length as recorded in the animation
//--------------------------------------------------------------------------------------
FLOAT   AnimationAdapter::GetDefaultBoneLength( UINT i ) const
{
    assert( i < NUI_BONE_COUNT );

    return m_nuiBones[ i ].m_fAnimationLength;
}



//--------------------------------------------------------------------------------------
// Name: GetJointsForFrame()
// Desc: Returns an array of points for the joints for the given frame number
//--------------------------------------------------------------------------------------
const XMVECTOR* AnimationAdapter::GetJointOffsetsForFrame( UINT uFrame ) const
{
    assert( uFrame < m_uNumFrames );

    return m_pFrames[ uFrame ].m_jointsOffsets;
}



//--------------------------------------------------------------------------------------
// Name: CalculateNormalizedJointsOffsets()
// Desc: Retargets BVH animation frame to NUI with origin of 0 and unit bone lengths
//--------------------------------------------------------------------------------------
void    AnimationAdapter::CalculateNormalizedJointsOffsets( XMVECTOR* pOffsets,
                                                            const XMVECTOR* pWorldPositions,
                                                            const UINT* nuiJointToAnimNodeIndex,
                                                            BOOL bMirror ) const
{
    for( UINT i=0; i < NUI_SKELETON_POSITION_COUNT; ++i )
    {
        const NUI_SKELETON_POSITION_INDEX nuiParentJointName = AnimationAdapter::ms_NuiJointParents[ i ];

        if( NUI_SKELETON_POSITION_COUNT == nuiParentJointName )
        {
            // position the hip at the NUI's skeleton origin
            pOffsets[ i ] = __vzero();
            continue;
        }

        const UINT uChildNodeIndex = nuiJointToAnimNodeIndex[ i ];
        const UINT uParentNodeIndex = nuiJointToAnimNodeIndex[ nuiParentJointName ];

        const XMVECTOR vStartWorld = pWorldPositions[ uParentNodeIndex ];
        const XMVECTOR vEndWorld = pWorldPositions[ uChildNodeIndex ];

        pOffsets[ i ] = XMVector3Normalize( vEndWorld - vStartWorld );
        if( bMirror )
        {
            pOffsets[ i ].x = -pOffsets[ i ].x;
        }
    }
}


//--------------------------------------------------------------------------------------
// Name: SetAnimation()
// Desc: Precalculate joints positions for the given BVH animation
//--------------------------------------------------------------------------------------
BOOL    AnimationAdapter::SetAnimation( const BVHAnimation* anim, BOOL bMirror )
{
    Clear();

    m_fFrameDuration = anim->GetFrameDuration();
    m_uNumFrames = anim->GetNumFrames();

    UINT    nuiJointToAnimNodeIndex[ NUI_SKELETON_POSITION_COUNT ];

    const UINT numNodes = anim->GetNumNodes();

    // create a remap table to know how to map from BVH to NUI
    for( UINT i=0; i < NUI_SKELETON_POSITION_COUNT; ++i )
    {
        const BVHAnimation::NodeName animNodeName = NuiJointToBvhNodeName( static_cast< NUI_SKELETON_POSITION_INDEX >( i ), bMirror );

        nuiJointToAnimNodeIndex[ i ] = ~0ul;

        for( UINT j=0; j < numNodes; ++j )
        {
            if( anim->GetNodeName( j ) == animNodeName )
            {
                nuiJointToAnimNodeIndex[ i ] = j;
                break;
            }
        }

        if( ~0ul == nuiJointToAnimNodeIndex[ i ] )
        {
            ATG::DebugSpew( "all joints and bones should be recognized in the animation" );
            return FALSE;
        }
    }

    // remap bones
    for( UINT i=0; i < NUI_BONE_COUNT; ++i )
    {
        NuiBone& bone = m_nuiBones[ i ];

        // tables check
        assert( ms_NuiBonesEndJoints[ i ] < NUI_SKELETON_POSITION_COUNT );
        assert( ms_NuiJointParents[ ms_NuiBonesEndJoints[ i ] ] < NUI_SKELETON_POSITION_COUNT );

        // get two endpoints of the bone
        const NUI_SKELETON_POSITION_INDEX endJointName = ms_NuiBonesEndJoints[ i ];
        const NUI_SKELETON_POSITION_INDEX startJointName = ms_NuiJointParents[ endJointName ];
        const UINT  uEndNodeIndex = nuiJointToAnimNodeIndex[ endJointName ];
        const UINT  uStartNodeIndex = nuiJointToAnimNodeIndex[ startJointName ];

        // can map both onto the animation?
        if( uEndNodeIndex != ~0ul && uStartNodeIndex != ~0ul )
        {
            bone.m_name = static_cast< NuiBoneName >( i );
        } else
        {
            bone.m_name = NUI_BONE_COUNT;
            bone.m_fAnimationLength = 0;

            ATG::DebugSpew( "%d couldn't find bone\n", i );

            return FALSE;
        }
    }

    // fill in default bone sizes
    XMVECTOR*   pWorldPositions = new XMVECTOR[ numNodes ];
    assert( 0 == (((UINT)pWorldPositions) & 0x0f) );

    XMMATRIX*   pDummy = new XMMATRIX[ numNodes ];
    assert( 0 == (((UINT)pDummy) & 0x0f) );

    // load default rest pose
    anim->CalcNodePositions(    pWorldPositions,
                                pDummy,
                                reinterpret_cast< XMVECTOR* >( pDummy ),
                                XMMatrixIdentity(),
                                0 );

    // bone lengths
    for( UINT i=0; i < NUI_BONE_COUNT; ++i )
    {
        const NUI_SKELETON_POSITION_INDEX endJointName = ms_NuiBonesEndJoints[ i ];
        const NUI_SKELETON_POSITION_INDEX startJointName = ms_NuiJointParents[ endJointName ];
        const UINT  uEndNodeIndex = nuiJointToAnimNodeIndex[ endJointName ];
        const UINT  uStartNodeIndex = nuiJointToAnimNodeIndex[ startJointName ];

        assert( uEndNodeIndex != ~0ul && uStartNodeIndex != ~0ul );

        const XMVECTOR delta = pWorldPositions[ uEndNodeIndex ] - pWorldPositions[ uStartNodeIndex ];

        m_nuiBones[ i ].m_fAnimationLength = XMVector3Length( delta ).x;
    }

    m_pFrames = new Frame[ m_uNumFrames ];
    assert( 0 == (((UINT)m_pFrames) & 0x0f) );

    // convert all frames into our format
    for( UINT i=0; i < m_uNumFrames; ++i )
    {
        anim->CalcNodePositions( pWorldPositions, pDummy, reinterpret_cast< XMVECTOR* >( pDummy ), XMMatrixIdentity(), i );

        // store offsets so we can rescale later
        CalculateNormalizedJointsOffsets( m_pFrames[ i ].m_jointsOffsets, pWorldPositions, nuiJointToAnimNodeIndex, bMirror );
    }

    delete[] pDummy;
    delete[] pWorldPositions;

    return TRUE;
}



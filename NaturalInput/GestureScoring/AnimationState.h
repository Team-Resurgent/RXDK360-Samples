//--------------------------------------------------------------------------------------
// AnimationState.h
//
// AnimationState holds the current state of the animation and interfaces with
// AnimationAdapter on behalf of the user
//
// Advanced Technology Group
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

#include "AnimationAdapter.h"

//--------------------------------------------------------------------------------------
// Name: AnimationState
// Desc: Frame based animation storage, maintains current animation time
//--------------------------------------------------------------------------------------
class AnimationState
{
public:
    AnimationState() :  m_pAdapter( NULL )
    {
    }

    ~AnimationState()
    {
        Clear();
    }

    BOOL        SetAdapter( const AnimationAdapter* pAdapter );
    void        Update( FLOAT fDeltaTime, BOOL bLoop );
    void        SetAnimationFrame( UINT uFrame );
    void        CalculateJoints( const FLOAT* pBonesLengths, XMVECTOR vOrg );
    UINT        GetAnimationFrame() const;
    UINT        GetNumFrames() const;
    void        Clear();
    const XMVECTOR* GetJoints() const;
    const XMVECTOR* GetJointsOffsets() const;

    static VOID     BuildHierarchicalTransform( XMVECTOR* pAxisAngleLocal,
                                                FLOAT fScaleZ,
                                                const XMVECTOR* pJointsCurrentOffsets,
                                                const XMVECTOR* pJointsRestOffsets,
                                                DWORD dwMask = 0xffffffff );

private:

    // animation data
    XMVECTOR                    m_joints[ NUI_SKELETON_POSITION_COUNT ];
    const AnimationAdapter*     m_pAdapter;
    FLOAT                       m_fTimeAccumulator;
    UINT                        m_uCurAnimFrame;

    void        CalculateJoints( XMVECTOR* pDest, const FLOAT* pBonesLengths, XMVECTOR vOrg, const XMVECTOR* pSrcJointOffsets );
};

//--------------------------------------------------------------------------------------
// AnimationPlayback.h
//
// This is a simple animation playback engine for SceneViewer2.  It samples keyframed
// animation data using data structures in AtgAnimation.h, and composes animated
// transform matrices into ATG::Frame objects.
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#pragma once
#ifndef ANIMATIONPLAYBACK_H
#define ANIMATIONPLAYBACK_H

#include <xtl.h>
#include <AtgSceneAll.h>

struct AnimationFrameBinding
{
    ATG::AnimationTransformTrack* pTrack;
    ATG::Frame* pFrame;
    DWORD dwPositionIndex;
    DWORD dwOrientationIndex;
    DWORD dwScaleIndex;
};
typedef std::vector <AnimationFrameBinding> AnimationFrameBindingVector;

class SimpleAnimationPlayer
{
public:
            SimpleAnimationPlayer() : m_fCurrentTime( 0 ),
                                      m_fPlaybackSpeed( 1 ),
                                      m_bLooping( TRUE ),
                                      m_pAnimation( NULL )
            {
            }
    VOID    BindAnimationToFrames( ATG::Animation* pAnimation, ATG::Scene* pScene, ATG::Frame* pRootFrame );
    ATG::Animation* GetAnimation() const
    {
        return m_pAnimation;
    }
    VOID    Update( FLOAT fDeltaTime );
    VOID    Clear();

    VOID    SetPlaybackSpeed( FLOAT fSpeed )
    {
        m_fPlaybackSpeed = fSpeed;
    }
    FLOAT   GetPlaybackSpeed() const
    {
        return m_fPlaybackSpeed;
    }

    VOID    SetTime( FLOAT fTime )
    {
        m_fCurrentTime = fTime;
    }
    FLOAT   GetTime() const
    {
        return m_fCurrentTime;
    }

    VOID    SetLooping( BOOL bLooping )
    {
        m_bLooping = bLooping;
    }
    BOOL    IsLooping() const
    {
        return m_bLooping;
    }

protected:
    ATG::Animation* m_pAnimation;
    AnimationFrameBindingVector m_BindingList;
    FLOAT m_fCurrentTime;
    FLOAT m_fPlaybackSpeed;
    BOOL m_bLooping;
};

#endif

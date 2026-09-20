//--------------------------------------------------------------------------------------
// AnimationState.cpp
//
// AnimationState holds the current state of the animation and interfaces with
// AnimationAdapter on behalf of the user
//
// Advanced Technology Group
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include <xtl.h>
#include <assert.h>
#include <vector>
#include "AnimationState.h"


//--------------------------------------------------------------------------------------
// Name: Clear()
// Desc: Unbinds animation state from the adapter
//--------------------------------------------------------------------------------------
void    AnimationState::Clear()
{
    m_pAdapter = NULL;
}


//--------------------------------------------------------------------------------------
// Name: SetAdapter()
// Desc: Binds this state to the adapter
//--------------------------------------------------------------------------------------
BOOL    AnimationState::SetAdapter( const AnimationAdapter* pAdapter )
{
    Clear();

    m_pAdapter = pAdapter;
    m_fTimeAccumulator = 0;
    m_uCurAnimFrame = 0;

    return TRUE;
}


//--------------------------------------------------------------------------------------
// Name: SetAnimationFrame()
// Desc: Sets current animation frame
//--------------------------------------------------------------------------------------
void    AnimationState::SetAnimationFrame( UINT uFrame )
{
    if( !m_pAdapter )
        return;

    assert( m_pAdapter->GetNumFrames() > uFrame );

    m_uCurAnimFrame = uFrame;
}



//--------------------------------------------------------------------------------------
// Name: GetAnimationFrame()
// Desc: Gets current animation frame
//--------------------------------------------------------------------------------------
UINT    AnimationState::GetAnimationFrame() const
{
    return m_uCurAnimFrame;
}


//--------------------------------------------------------------------------------------
// Name: GetNumFrames()
// Desc: Get number of frames in the animation
//--------------------------------------------------------------------------------------
UINT    AnimationState::GetNumFrames() const
{
    if( !m_pAdapter )
        return 0;

    return m_pAdapter->GetNumFrames();
}

//--------------------------------------------------------------------------------------
// Name: GetJoints()
// Desc: Get current frame's joints' positions
//--------------------------------------------------------------------------------------
const XMVECTOR* AnimationState::GetJoints() const
{
    return m_joints;
}

//--------------------------------------------------------------------------------------
// Name: GetJointsOffsets()
// Desc: Get current frame's joints' offset
//--------------------------------------------------------------------------------------
const XMVECTOR* AnimationState::GetJointsOffsets() const
{
    return m_pAdapter->GetJointOffsetsForFrame( m_uCurAnimFrame );
}

//--------------------------------------------------------------------------------------
// Name: BuildHierarchicalTransform()
// Desc: calculate hierarchical transform from the rset pose to the current pose
//       we have it easy here because the skeleton is actually a bunch of points
//       without rotations
//--------------------------------------------------------------------------------------
void    AnimationState::BuildHierarchicalTransform( XMVECTOR* pAxisAngleXFormLocal,
                                                    FLOAT fScaleZ,
                                                    const XMVECTOR* pJointsCurrentOffsets,
                                                    const XMVECTOR* pJointsRestOffsets,
                                                    DWORD dwMask )
{
    // note that dwMask should contain full paths to joints

    pAxisAngleXFormLocal[ NUI_SKELETON_POSITION_HIP_CENTER ] = XMVectorZero();

    XMMATRIX    forwards[ NUI_SKELETON_POSITION_COUNT ];
    XMMATRIX    inverses[ NUI_SKELETON_POSITION_COUNT ];
    forwards[ NUI_SKELETON_POSITION_HIP_CENTER ] = XMMatrixIdentity();
    inverses[ NUI_SKELETON_POSITION_HIP_CENTER ] = XMMatrixIdentity();

    const XMVECTOR  vScaler = XMVectorSet( 1, 1, fScaleZ, 1 );

    for( UINT i=0; i < NUI_SKELETON_POSITION_COUNT; ++i, dwMask >>= 1 )
    {
        const NUI_SKELETON_POSITION_INDEX parentJointName = AnimationAdapter::ms_NuiJointParents[ i ];

        if( NUI_SKELETON_POSITION_COUNT == parentJointName )
            continue;

        if( !(dwMask & 1) )
            continue;

        // by construction
        assert( parentJointName < static_cast< NUI_SKELETON_POSITION_INDEX >( i ) );

        // get two vectors we're going to take a delta rotation between
        const XMVECTOR  vParentToChildFrame = pJointsCurrentOffsets[ i ];
        const XMVECTOR  vDirRest = pJointsRestOffsets[ i ];
        const XMVECTOR  vDirFrame = XMVector3TransformNormal( vParentToChildFrame, inverses[ parentJointName ] );

        // flatten if required
        const XMVECTOR  vDirRestFlattened = XMVector3Normalize( vDirRest * vScaler );
        const XMVECTOR  vDirFrameFlattened = XMVector3Normalize( vDirFrame * vScaler );
        
        // if this bone did't move we don't bother with local rotation calculation
        const XMVECTOR  vRotationAxis = XMVector3Cross( vDirRestFlattened, vDirFrameFlattened );
        if( XMVector3Length( vRotationAxis ).x < 0.00001f )
        {
            inverses[ i ] = inverses[ parentJointName ];
            forwards[ i ] = forwards[ parentJointName ];
            pAxisAngleXFormLocal[ i ] = XMVectorZero();
        } else
        {
            // calculate rotation matrix and its inverse
            const XMVECTOR  vRotAxisNormalized = XMVector3Normalize( vRotationAxis );
            const FLOAT     fAngle = XMVector3AngleBetweenVectors( vDirRestFlattened, vDirFrameFlattened ).x;

            pAxisAngleXFormLocal[ i ] = XMVectorSet( vRotAxisNormalized.x, vRotAxisNormalized.y, vRotAxisNormalized.z, fAngle );

            forwards[ i ] = XMMatrixRotationAxis( vRotAxisNormalized, fAngle ) * forwards[ parentJointName ];

            XMVECTOR    d;
            inverses[ i ] = XMMatrixInverse( &d, forwards[ i ] );
        }
    }
}



//--------------------------------------------------------------------------------------
// Name: CalculateJoints()
// Desc: Reconstructs joints positions from offsets of the adapter
//--------------------------------------------------------------------------------------
void    AnimationState::CalculateJoints( XMVECTOR* pDest, const FLOAT* pBonesLengths, XMVECTOR vOrg, const XMVECTOR* pSrcJointOffsets )
{
    // create a new animated skeleton with the unity bone lengths
    for( INT i=0; i < NUI_SKELETON_POSITION_COUNT; ++i )
    {
        const NUI_SKELETON_POSITION_INDEX nuiParentJointName = AnimationAdapter::ms_NuiJointParents[ i ];

        if( NUI_SKELETON_POSITION_COUNT == nuiParentJointName )
        {
            assert( i == NUI_SKELETON_POSITION_HIP_CENTER );
            pDest[ i ] = vOrg;
        } else
        {
            pDest[ i ] = pSrcJointOffsets[ i ] * pBonesLengths[ AnimationAdapter::ms_NuiJointToBoneName[ i ] ] +
                                pDest[ nuiParentJointName ];
        }
    }
}


//--------------------------------------------------------------------------------------
// Name: CalculateJoints()
// Desc: Given bone lengths and skelton origin calculates the joints positions
//--------------------------------------------------------------------------------------
void    AnimationState::CalculateJoints( const FLOAT* pBonesLengths, XMVECTOR vOrg )
{
    CalculateJoints( m_joints, pBonesLengths, vOrg, m_pAdapter->GetJointOffsetsForFrame( m_uCurAnimFrame ) );
}

//--------------------------------------------------------------------------------------
// Name: Update()
// Desc: Tracks animation frames
//--------------------------------------------------------------------------------------
void AnimationState::Update( FLOAT fDeltaTime, BOOL bLoop )
{
    if( !m_pAdapter )
        return;

    // the below code is terrible

    m_fTimeAccumulator += fDeltaTime;

    const UINT uNumFramesToStep = static_cast< UINT >( m_fTimeAccumulator / m_pAdapter->GetFrameDuration() );

    if( uNumFramesToStep )
    {
        m_fTimeAccumulator -= uNumFramesToStep * m_pAdapter->GetFrameDuration();

        UINT    uFrame = GetAnimationFrame() + uNumFramesToStep;

        if( bLoop )
        {
            uFrame %= m_pAdapter->GetNumFrames();
        } else
        {
            uFrame = min( uFrame, m_pAdapter->GetNumFrames() - 1 );
        }

        SetAnimationFrame( uFrame );
    }
}

//--------------------------------------------------------------------------------------
// Common.h
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

#include <xtl.h>
#include <xgraphics.h>
#include <xavatar.h>
#include <nuiapi.h>
#include <AtgUtil.h>

//--------------------------------------------------------------------------------------
// Define thread utilization for the sample
//--------------------------------------------------------------------------------------

#define RENDER_HW_THREAD                    0                                       // HW0
#define DEPTH_UPDATE_HW_THREAD              1                                       // HW1
#define COLOR_UPDATE_HW_THREAD              2                                       // HW2
#define SKELETON_TRACKING_UPDATE_HW_THREAD  2                                       // HW2
#define D3D_WORKER_HW_THREAD                D3DCREATE_CREATE_THREAD_ON_2            // HW2
#define GAME_UPDATE_HW_THREAD               3                                       // HW3
#define AUDIO_HW_THREAD                     XAUDIO2_DEFAULT_PROCESSOR               // HW4
#define NUI_HW_THREAD                       NUI_INITIALIZE_DEFAULT_HARDWARE_THREAD  // HW5
#define AVATAR_LOAD_HW_THREAD               5                                       // HW5

//--------------------------------------------------------------------------------------
// Define constants used in sampele
//--------------------------------------------------------------------------------------

const DWORD NUM_FRAME_BUFFERS       = 2;  // Double buffer data
const DWORD NUM_CAUSTIC_TEXTURES    = 32;
const DWORD NUM_FISH                = 15;

//--------------------------------------------------------------------------------------
// Define enum with different sample types
//--------------------------------------------------------------------------------------

enum ESampleType
{
    SAMPLE_TYPE_NONE,
    SAMPLE_TYPE_LOW_LATENCY,
    SAMPLE_TYPE_HIGH_LATENCY
};

//--------------------------------------------------------------------------------------
// Define enum with different render passes
//--------------------------------------------------------------------------------------

enum ERenderPass
{
    RENDER_PASS_NONE,           // none
    RENDER_PASS_SHADOW,         // regular shadow map
    RENDER_PASS_HYBRID_SHADOW,  // hybrid shadow map with 2 channels
    RENDER_PASS_Z,              // custom z pre-pass
    RENDER_PASS_OPAQUE_AND_Z,   // opaque with auto z pre-pass
    RENDER_PASS_OPAQUE,         // opaque
    RENDER_PASS_TRANSPARENT,    // transparent
};

//--------------------------------------------------------------------------------------
// Frame buffer data that needs to be double buffered
//--------------------------------------------------------------------------------------

class CFrameBufferData
{
public:
    CFrameBufferData()
    {
        m_matView = XMMatrixIdentity();
        m_matWorldAvatar = XMMatrixIdentity();
        m_pCurrentCausticTexture = NULL;
        m_iSkeletonIdx = -1;
        m_uFrameBufferIdx = 0;
        
        ZeroMemory( &m_SkeletonFrame, sizeof( NUI_SKELETON_FRAME ) );

        for ( UINT i = 0; i < NUM_FISH; i++ )
        {
            m_matModel[ i ] = XMMatrixIdentity();
            m_vBlendWeights[ i ] = XMVectorZero();
        }

        for ( UINT i = 0; i < XAVATAR_MAX_SKELETON_JOINTS; i++ )
        {
            ZeroMemory( &m_AvatarJointPose[ i ], sizeof( XAVATAR_SKELETON_POSE_JOINT ) );
        }
    }

    XMMATRIX m_matView;
    XMMATRIX m_matWorldAvatar;
    XMMATRIX m_matModel[ NUM_FISH ];
    XMVECTOR m_vBlendWeights[ NUM_FISH ];
    NUI_SKELETON_FRAME m_SkeletonFrame;
    D3DTexture* m_pCurrentCausticTexture;
    XAVATAR_SKELETON_POSE_JOINT m_AvatarJointPose[ XAVATAR_MAX_SKELETON_JOINTS ]; 
    INT m_iSkeletonIdx;
    UINT m_uFrameBufferIdx;
};

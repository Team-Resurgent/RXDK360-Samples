//--------------------------------------------------------------------------------------
// BVHAnimation.h
//
// This loads a BVH animation
//
// Advanced Technology Group
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once


//--------------------------------------------------------------------------------------
// Name: BVHAnimation
// Desc: Loads and plays back BVH animation files
//--------------------------------------------------------------------------------------
class BVHAnimation
{
public:

    //--------------------------------------------------------------------------------------
    // Name: 
    // Desc: This is taken from the output of XEDtoBVH. Some nodes like HIP_CENTER_LEFT/RIGHT
    //       may not be BVH standard.
    //--------------------------------------------------------------------------------------
    enum NodeName
    {
        BVH_NODE_HIP_CENTER_LEFT = 0    ,// NUI_SKELETON_POSITION_HIP_CENTER,
        BVH_NODE_HIP_CENTER_RIGHT       ,// NUI_SKELETON_POSITION_HIP_CENTER,
        BVH_NODE_SPINE                  ,// NUI_SKELETON_POSITION_SPINE,
        BVH_NODE_SPINE1                 ,// (NUI_SKELETON_POSITION_SPINE + NUI_SKELETON_POSITION_SHOULDER_CENTER) / 2,
        BVH_NODE_SHOULDER_CENTER        ,// NUI_SKELETON_POSITION_SHOULDER_CENTER,
        BVH_NODE_SHOULDER_CENTER_LEFT   ,// NUI_SKELETON_POSITION_SHOULDER_CENTER,
        BVH_NODE_SHOULDER_CENTER_RIGHT  ,// NUI_SKELETON_POSITION_SHOULDER_CENTER,
        BVH_NODE_HEAD                   ,// NUI_SKELETON_POSITION_HEAD,
        BVH_NODE_SHOULDER_LEFT          ,// NUI_SKELETON_POSITION_SHOULDER_LEFT,
        BVH_NODE_ELBOW_LEFT             ,// NUI_SKELETON_POSITION_ELBOW_LEFT,
        BVH_NODE_WRIST_LEFT             ,// NUI_SKELETON_POSITION_WRIST_LEFT,
        BVH_NODE_HAND_LEFT              ,// NUI_SKELETON_POSITION_HAND_LEFT,
        BVH_NODE_SHOULDER_RIGHT         ,// NUI_SKELETON_POSITION_SHOULDER_RIGHT,
        BVH_NODE_ELBOW_RIGHT            ,// NUI_SKELETON_POSITION_ELBOW_RIGHT,
        BVH_NODE_WRIST_RIGHT            ,// NUI_SKELETON_POSITION_WRIST_RIGHT,
        BVH_NODE_HAND_RIGHT             ,// NUI_SKELETON_POSITION_HAND_RIGHT,
        BVH_NODE_HIP_LEFT               ,// NUI_SKELETON_POSITION_HIP_LEFT,
        BVH_NODE_KNEE_LEFT              ,// NUI_SKELETON_POSITION_KNEE_LEFT,
        BVH_NODE_ANKLE_LEFT             ,// NUI_SKELETON_POSITION_ANKLE_LEFT,
        BVH_NODE_FOOT_LEFT              ,// NUI_SKELETON_POSITION_FOOT_LEFT,
        BVH_NODE_HIP_RIGHT              ,// NUI_SKELETON_POSITION_HIP_RIGHT,
        BVH_NODE_KNEE_RIGHT             ,// NUI_SKELETON_POSITION_KNEE_RIGHT,
        BVH_NODE_ANKLE_RIGHT            ,// NUI_SKELETON_POSITION_ANKLE_RIGHT,
        BVH_NODE_FOOT_RIGHT             ,// NUI_SKELETON_POSITION_FOOT_RIGHT,
        BVH_NODE_COUNT
    };

    //--------------------------------------------------------------------------------------
    // Name: 
    // Desc: 
    //--------------------------------------------------------------------------------------
    struct Bone
    {
        UINT        startNodeIndex;
        UINT        endNodeIndex;
    };

    BVHAnimation();
    ~BVHAnimation();

    UINT            GetNumNodes() const;
    XMVECTOR        GetNodeOffset( UINT idx ) const;
    UINT            GetNodeParentIndex( UINT idx ) const;
    NodeName        GetNodeName( UINT nodeIndex ) const;

    const Bone*     GetBones() const;
    UINT            GetNumBones() const;

    FLOAT           GetFrameDuration() const;
    UINT            GetNumFrames() const;
    BOOL            CalcNodePositions( XMVECTOR* pDest, XMMATRIX* pRotationMatrices, XMVECTOR* pTranslationVectors, XMMATRIX matRoot, UINT uFrameIndex ) const;

    static BVHAnimation*   LoadBVH( const char* pFilename );


private:

    struct Node;

    Node*       m_pNodes;
    Bone*       m_pBones;
    UINT        m_numNodes;
    UINT        m_numBones;

    FLOAT       m_frameDuration;
    UINT        m_numFrames;
    UINT        m_numFloatsPerFrame;
    FLOAT*      m_pAnimData;        // N frames of M floats per frame

    XMMATRIX*   m_pMatrices;

    static BVHAnimation*   BVHAnimation::ParseBVHFile( const char* pData );
};




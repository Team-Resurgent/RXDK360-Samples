//-------------------------------------------------------------------------------------
// HandOrientation.h
//  
// Tracks the orientation of the hand when the fist is facing the screen using image 
// moments.
//  
// Advanced Technology Group (ATG)
// Copyright (c) Microsoft Corporation. All rights reserved.
//-------------------------------------------------------------------------------------

#include <nuiapi.h>

#include "ATGNuiHandRefinement.h"
#define DEBUG_VISUALIZE_HAND_PRINTS 1

// This enum specifies if the hand orientation is known.  If its not known
// it can specific how the hand needs to be pointed at the screen to allow orientation
// to be calculated.

enum HAND_STATE
{
    HAND_STATE_UNKNOWN = 0,
    HAND_STATE_ORIENTATION_TRACKED,
    HAND_STATE_HAND_TOO_LOW,
    HAND_STATE_HAND_TOO_HIGH,
    HAND_STATE_HAND_TOO_FAR_LEFT,
    HAND_STATE_HAND_TOO_FAR_RIGHT
};  

//--------------------------------------------------------------------------------------
// Stores image moments associated with the hand.
//--------------------------------------------------------------------------------------
struct HandOrientationData
{
    HandOrientationData()
    {
        Reset();
    };

    VOID Reset()
    {
    #ifdef DEBUG_VISUALIZE_HAND_PRINTS
        memset( m_FrameData, 0, sizeof(m_FrameData) );
    #endif
        memset( &m_Mean, 0, sizeof( ATG::INT3Vector ) );
        m_Mean.x = 0.0f;
        m_Mean.y = 0.0f;
        m_Mean.z = 0.0f;
        m_fMoment00 = 0.0f;
        m_fMoment11 = 0.0f;
        m_fMoment22 = 0.0f;
        m_fMoment20 = 0.0f;
        m_fMoment02 = 0.0f;
        m_fOrientationBasedSkew = 0.0f;
        m_fStandardDeviationInX = 0.0f;
        m_fStandardDeviationInY = 0.0f;
        m_fOrientation = 0.0f;
        m_eHandState = HAND_STATE_UNKNOWN;
    }

    ~HandOrientationData(){};
    
    #ifdef DEBUG_VISUALIZE_HAND_PRINTS
        CHAR m_FrameData[ ATG::g_iNuiRefinementHalfKernelSize320x240 * 2 ][ ATG::g_iNuiRefinementHalfKernelSize320x240 * 2];
    #endif 

    XMFLOAT3 m_Mean;

    FLOAT m_fMoment00;
    FLOAT m_fMoment11;
    FLOAT m_fMoment22;
    FLOAT m_fMoment20;
    FLOAT m_fMoment02;

    FLOAT m_fOrientation;
    FLOAT m_fOrientation180;
    FLOAT m_fStandardDeviationInX;
    FLOAT m_fStandardDeviationInY;  
    FLOAT m_fOrientationBasedSkew;
    HAND_STATE m_eHandState;

};

//--------------------------------------------------------------------------------------
// Calculates the orientation of a fist with the thumb up pointed at the screen.
//--------------------------------------------------------------------------------------
class HandOrientation
{
public:
    HandOrientation() {};
    ~HandOrientation() {};

    VOID Reset();

    VOID Update( LPDIRECT3DTEXTURE9 pDepthAndSegmentationTexture320x240, 
                 INT iSkeletonDataSlot, 
                 const NUI_SKELETON_FRAME* pSkeletonFrame,
                 ATG::RefinementData* pRefinementData,
                 ATG::OptionalRefinementData* pExtraRefinementData );     


    HandOrientationData* GetRightData() { return &m_RightHandData; };

    HandOrientationData* GetLeftData() { return &m_LeftHandData; };

private:

    HRESULT UpdateHand( HandOrientationData* pHandControl, 
                        ATG::HandSpecificData *pHand,
                        ATG::OptionalRefinementData* pExtraData,
                        ATG::OptionalHandSpecificData* pExtraHand,
                        XMVECTOR vElbow );

    HandOrientationData         m_RightHandData;
    HandOrientationData         m_LeftHandData;

};
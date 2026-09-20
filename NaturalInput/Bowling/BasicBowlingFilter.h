//--------------------------------------------------------------------------------------
// BasicBowlingFilter.h
//
// Implements a filter that tries to infer left or right arm motion when the raw skeleton
// stops reporting accurate position. The filter assumes that the player is attempting 
// to throw a bowling ball.
// 
// Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

#include<nuiapi.h>

namespace ATG
{
class Font;
};


//--------------------------------------------------------------------------------------
// Name: ComputeWristPosition()
// Desc: Return angle in radian formed by the vector from the shoulder to the wrist 
//       projected on a circle centered at the shoulder and perpendicular to the vector
//       from its center to the opposite shoulder.
//       The angle of value PI / 2 correspond to the arm alligned along the players 
//       trunc.
//--------------------------------------------------------------------------------------
FLOAT ComputeWristPosition( const NUI_SKELETON_DATA* pSkeletonData, BOOL bIsLeftHanded );


//--------------------------------------------------------------------------------------
// Name: GetXXXXXSkeletonPositionIndex()
// Desc: Family of helper functions that return a position index for skeleton joints for 
//       either a left side or right side joints.
//--------------------------------------------------------------------------------------
inline NUI_SKELETON_POSITION_INDEX GetShoulderSkeletonPositionIndex( BOOL bIsLeftHanded ) 
    { return bIsLeftHanded ? NUI_SKELETON_POSITION_SHOULDER_LEFT : NUI_SKELETON_POSITION_SHOULDER_RIGHT; }

inline NUI_SKELETON_POSITION_INDEX GetElbowSkeletonPositionIndex( BOOL bIsLeftHanded ) 
    { return bIsLeftHanded ? NUI_SKELETON_POSITION_ELBOW_LEFT : NUI_SKELETON_POSITION_ELBOW_RIGHT; }

inline NUI_SKELETON_POSITION_INDEX GetWristSkeletonPositionIndex( BOOL bIsLeftHanded ) 
    { return bIsLeftHanded ? NUI_SKELETON_POSITION_WRIST_LEFT : NUI_SKELETON_POSITION_WRIST_RIGHT; }

inline NUI_SKELETON_POSITION_INDEX GetHandSkeletonPositionIndex( BOOL bIsLeftHanded ) 
    { return bIsLeftHanded ? NUI_SKELETON_POSITION_HAND_LEFT : NUI_SKELETON_POSITION_HAND_RIGHT; }

inline NUI_SKELETON_POSITION_INDEX GetHipSkeletonPositionIndex( BOOL bIsLeftHanded ) 
    { return bIsLeftHanded ? NUI_SKELETON_POSITION_HIP_LEFT : NUI_SKELETON_POSITION_HIP_RIGHT; }



// Data use by this filter to infer the position of the arm. 
// The data is relative to the hip of the same skeleton side of the data.
struct BOWLING_HIP_RELATIVE_DATA
{
    XMVECTOR vShoulder;
    XMVECTOR vElbow;
    XMVECTOR vWrist;
    XMVECTOR vHand;

    XMVECTOR vOppShoulder;

    BOOL bIsLeftArm;  // TRUE if this data correspond to the skeleton's left arm.
    
    FLOAT fDeltaTime; //Time between this and the previous frame.
};


// Actions that can be taken by the filter while processing a given skeleton frame. 
enum BOWLING_ACTION
{
    BOWLING_ACTION_BLEND = 0,       // Blend to the unfiltered arm, in any direction.
    // ELx not in use now, remove before shipping    BOWLING_ACTION_BLEND_FORWARD,   // Adjust forward speed to match unfirtered arm position more closely.
    BOWLING_ACTION_RESET,           // Cancel all states, we are not bowling anymore
    BOWLING_ACTION_RESET_ALL,       // Deep reset, in addition to all states, it also clears the arm history.
    BOWLING_ACTION_RETURN_SWING,    // The arm has reached is farthest point back and it is time to bring it forward again.
    BOWLING_ACTION_USE_INPUT,       // Simply return the unfiltered arm. We may or may not be 
    BOWLING_ACTION_USE_PREDICTED,   // Return the predicted arm.
    BOWLING_ACTION_MAX
};
    

//--------------------------------------------------------------------------------------
// Name: class BasicBowlingFilter
// Desc: Implements the arms up skeleton filter.
//--------------------------------------------------------------------------------------
class BasicBowlingFilter
{
public:
    BasicBowlingFilter(): m_bIsLeftHanded( FALSE ), m_bIsFilterActive( FALSE ) { ResetFilterState(); }
    ~BasicBowlingFilter() {}

    VOID Initialize( BOOL bIsLeftHanded ) { m_bIsLeftHanded = bIsLeftHanded;  ResetFilterState(); }
    VOID ResetFilterState();

    VOID Go() { m_bIsFilterActive = TRUE; }
    VOID Stop() { m_bIsFilterActive = FALSE; }

    BOOL FilterSkeleton( const NUI_SKELETON_DATA* pInputSkeleton, NUI_SKELETON_DATA* pOutputSkeleton, FLOAT fDeltaTime );

    FLOAT DebugOutput( ATG::Font* pFont, FLOAT fX, FLOAT fY, DWORD dwColor = 0xffffffff, DWORD dwFlags = 0 ) const;


private:
    BasicBowlingFilter( BasicBowlingFilter& );
    BasicBowlingFilter& operator =( const BasicBowlingFilter& );

    VOID Reset();
    VOID UpdateQualityData( const NUI_SKELETON_DATA* pInputSkeleton, const BOWLING_HIP_RELATIVE_DATA* pInputArm, const BOWLING_HIP_RELATIVE_DATA* pPredictedArm );
    VOID UpdateHistoryData( const BOWLING_HIP_RELATIVE_DATA* pBowlingArmData );

    BOWLING_ACTION GetNextAction( const BOWLING_HIP_RELATIVE_DATA* pInputArm, const BOWLING_HIP_RELATIVE_DATA* pPredictedArm ) const;

    const static DWORD BOWLING_ARM_HISTORY_MAX = 2;
    BOWLING_HIP_RELATIVE_DATA m_BowlingArmHistory[ BOWLING_ARM_HISTORY_MAX ];
    DWORD m_dwBowlingArmHistoryCount;

    BOOL m_bIsLeftHanded;
    BOOL m_bIsFilterActive;

    // Data related to quality
    FLOAT m_fAccuracy;   // Accuracy of the arm predicted by the filter vs. the unfiltered one. A smaller number indicate a better accuracy
    FLOAT m_fConfidence; // Confidence in the unfiltered skeleton
    DWORD m_dwOverride;  // Increases with repeated good accuracy tests

    // Events triggered during a bowling swing
    BOOL m_bIsBlending;  // We aren't bowling anymore, but we need to do some blending before ending all processing
    BOOL m_bIsReturning; // Back lmit was reached. The arm is returning to the front.

    DWORD m_dwSteps;     // Counts the number of steps performed under the control of this filter 
    DWORD m_dwMaxSteps;  // Max number of steps that can be performed before we lose confidence and start returning the raw skeleton again.
};
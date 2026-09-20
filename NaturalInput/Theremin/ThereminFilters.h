//--------------------------------------------------------------------------------------
// ThereminFilters.h
//
// Detects distance between each hand and default points in space for each hand, and 
// uses those distances to simulate a theremin. Left hand distance reflects volume,
// and right hand controls pitch.
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once
#include <NuiApi.h>

//////////////////////////////////////////////////////////////////////////
// Forward Declarations
//////////////////////////////////////////////////////////////////////////

namespace ATG
{
class Font;
}

#define NO_TRACKED_SKELETON (0xFFFFFFFF)

class CControls
{

public:

    // List of available control schemes
    enum CONTROLSCHEME
    {
        CONTROLSCHEME_ELBOWS,    // The theremin is controlled via elbow-joint angles
        CONTROLSCHEME_VIRTUAL,   // The theremin is controlled via a virtual theremin
                                 // setup, positioned relative to the shoulders and hips.
        CONTROLSCHEME_COUNT      // Number of control options.
    };

    CControls();

    HRESULT Update( const FLOAT fDeltaTime, const NUI_SKELETON_FRAME* pSkeletonFrame );
    FLOAT GetVolume() { return m_fVolume; }
    FLOAT GetTargetVolume() { return m_fTargetVolume; }
    FLOAT GetPitch() { return m_fPitch; }
    FLOAT GetTargetPitch() { return m_fTargetPitch; }
    BOOL IsOctaveMode() const { return m_bOctaveMode; };
    VOID SetOctaveMode( BOOL bMakeLinear ) { m_bOctaveMode = bMakeLinear; };

    const WCHAR* GetControlSchemeName();
    CONTROLSCHEME GetCurrentControlMethod() const { return m_eCurrentControlType; }
    VOID SetCurrentControlMethod(CONTROLSCHEME method) { m_eCurrentControlType = method; }
    VOID Reset();
    VOID NextControlScheme();
    VOID PrevControlScheme();

    BOOL IsTracking() const { return m_dwLastTrackedPlayerSkeleton != NO_TRACKED_SKELETON; }
    DWORD GetTrackedPlayer() const { return m_dwLastTrackedPlayerSkeleton; }

    VOID RenderDebug(ATG::Font& font) const;

protected:
    
    BOOL IsPlayerConfidenceGood( ) const { return m_bPlayerConfidenceGood; }
    BOOL IsWaistPosSettling() const { return m_fWaistSettleTime > 0.0f; }
    BOOL IsPlayerInNeutral() const { return m_bPlayerNeutral; }
    BOOL CheckPlayerConfidence( const NUI_SKELETON_DATA* pSkeleton );
    BOOL CheckPlayerNeutral( const NUI_SKELETON_DATA* pSkeleton );
    VOID UpdateTrackedSkeleton( const NUI_SKELETON_FRAME* pSkeletonFrame );
    VOID InterpolatePitchAndVolume(const FLOAT fDeltaTime);
    VOID ControlViaElbowAngles( const FLOAT fDeltaTime, const NUI_SKELETON_DATA* pSkeleton );
    VOID ControlViaVirtualPosition( const FLOAT fDeltaTime, const NUI_SKELETON_DATA* pSkeleton );
    FLOAT RangeAndLinearizeFreq( const FLOAT fLinearFreq );
    FLOAT UpdateWaistTracking( const FLOAT fDeltaTime, FXMVECTOR vHipCenter );
    VOID UpdateArmBoneLengths( const NUI_SKELETON_DATA* pSkeleton );

    XMVECTOR m_vWaistPosAvg;
    XMVECTOR m_vWaistPosSet;
    XMVECTOR m_vPitchAntennaPos;
    XMVECTOR m_vAmplitudeAntennaPos;
    XMVECTOR m_vLeftHandPos;
    XMVECTOR m_vRightHandPos;
    DWORD m_dwLastTrackedPlayerSkeleton;
    FLOAT m_fLeftElbowAngle;
    FLOAT m_fRightElbowAngle;
    FLOAT m_fVolume;
    FLOAT m_fTargetVolume;
    FLOAT m_fPitch;
    FLOAT m_fTargetPitch;
    FLOAT m_fWaistSettleTime;
    FLOAT m_fLUpperArmLength;
    FLOAT m_fRUpperArmLength;
    FLOAT m_fLLowerArmLength;
    FLOAT m_fRLowerArmLength;
    FLOAT m_fPitchDistance;
    FLOAT m_fPitchDistanceRange;
    FLOAT m_fVolumeDistance;
    FLOAT m_fVolumeDistanceRange;
    CONTROLSCHEME m_eCurrentControlType;
    BOOL m_bOctaveMode;
    BOOL m_bPlayerNeutral;
    BOOL m_bPlayerConfidenceGood;
};



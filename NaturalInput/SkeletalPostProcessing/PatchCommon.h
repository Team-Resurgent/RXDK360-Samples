//--------------------------------------------------------------------------------------
// PatchCommon.h
//
// Contains common code and infrastructure for skeleton postprocessing, including a
// skeleton filter stack, skeleton filter interface, depth image processing code,
// and several more classes & methods useful for implementing skeleton filters.
// 
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

#pragma warning( disable:4324 )

#include <xtl.h>
#include <nuiapi.h>
#include <vector>
#include <assert.h>

// A useful array that indicates the parent joint for each NUI skeleton joint.
static const NUI_SKELETON_POSITION_INDEX g_SkeletonParentIndex[ NUI_SKELETON_POSITION_COUNT ] =
{
    (NUI_SKELETON_POSITION_INDEX)-1,
    NUI_SKELETON_POSITION_HIP_CENTER,
    NUI_SKELETON_POSITION_SPINE,
    NUI_SKELETON_POSITION_SHOULDER_CENTER,
    NUI_SKELETON_POSITION_SHOULDER_CENTER,
    NUI_SKELETON_POSITION_SHOULDER_LEFT,
    NUI_SKELETON_POSITION_ELBOW_LEFT,
    NUI_SKELETON_POSITION_WRIST_LEFT,
    NUI_SKELETON_POSITION_SHOULDER_CENTER,
    NUI_SKELETON_POSITION_SHOULDER_RIGHT,
    NUI_SKELETON_POSITION_ELBOW_RIGHT,
    NUI_SKELETON_POSITION_WRIST_RIGHT,
    NUI_SKELETON_POSITION_HIP_CENTER,
    NUI_SKELETON_POSITION_HIP_LEFT,
    NUI_SKELETON_POSITION_KNEE_LEFT,
    NUI_SKELETON_POSITION_ANKLE_LEFT,
    NUI_SKELETON_POSITION_HIP_CENTER,
    NUI_SKELETON_POSITION_HIP_RIGHT,
    NUI_SKELETON_POSITION_KNEE_RIGHT,
    NUI_SKELETON_POSITION_ANKLE_RIGHT
};

// A useful string array that contains the names of each joint.
static const WCHAR* g_strJointNames[NUI_SKELETON_POSITION_COUNT] =
{
    L"HIP_CENTER",
    L"SPINE",
    L"SHOULDER_CENTER",
    L"HEAD",
    L"SHOULDER_LEFT",
    L"ELBOW_LEFT",
    L"WRIST_LEFT",
    L"HAND_LEFT",
    L"SHOULDER_RIGHT",
    L"ELBOW_RIGHT",
    L"WRIST_RIGHT",
    L"HAND_RIGHT",
    L"HIP_LEFT",
    L"KNEE_LEFT",
    L"ANKLE_LEFT",
    L"FOOT_LEFT",
    L"HIP_RIGHT",
    L"KNEE_RIGHT",
    L"ANKLE_RIGHT",
    L"FOOT_RIGHT",
};


//--------------------------------------------------------------------------------------
// Name: class TimedLerp
// Desc: Maintains a time-based lerp between 0 and a upper limit between 0 and 1.
//       The lerp speed parameter is in units of inverse time - therefore, a speed of 2.0
//       means that the lerp completes a full transition (0 to 1) in 0.5 seconds.
//--------------------------------------------------------------------------------------
class TimedLerp
{
protected:
    FLOAT m_fEnabled;
    FLOAT m_fValue;
    FLOAT m_fEaseInSpeed;
    FLOAT m_fEaseOutSpeed;

public:
    TimedLerp()
        : m_fEnabled( 0.0f ),
          m_fValue( 0.0f ),
          m_fEaseInSpeed( 1.0f ),
          m_fEaseOutSpeed( 1.0f )
    { }
    VOID SetSpeed( FLOAT fEaseInSpeed, FLOAT fEaseOutSpeed = 0.0f ) 
    { 
        m_fEaseInSpeed = fEaseInSpeed;
        if( fEaseOutSpeed <= 0.0f )
        {
            m_fEaseOutSpeed = fEaseInSpeed;
        }
        else
        {
            m_fEaseOutSpeed = fEaseOutSpeed;
        }
    }
    VOID SetEnabled( BOOL bEnabled ) { m_fEnabled = bEnabled ? 1.0f : 0.0f; }
    VOID SetEnabled( FLOAT fEnabled ) { m_fEnabled = max( 0.0f, min( 1.0f, fEnabled ) ); }
    VOID Reset() { m_fEnabled = 0.0f; m_fValue = 0.0f; }

    // GetEnabled reflects whether the target value is 0 or not.
    BOOL GetEnabled() const { return m_fEnabled > 0.0f; }

    // GetLerpEnabled reflects whether the current value is 0 or not.
    BOOL GetLerpEnabled() const { return GetEnabled() || ( m_fValue > 0.0f ); }

    // Tick needs to be called once per frame.
    VOID Tick( FLOAT fDeltaTime )
    {
        FLOAT fSpeed = m_fEaseInSpeed;
        if( m_fValue > m_fEnabled )
        {
            fSpeed = m_fEaseOutSpeed;
        }
        FLOAT fDelta = fSpeed * fDeltaTime;
        if( m_fEnabled > 0.0f )
        {
            m_fValue = min( m_fEnabled, m_fValue + fDelta );
        }
        else
        {
            m_fValue = max( 0.0f, m_fValue - fDelta );
        }
    }
    
    // GetLinearValue returns a raw, linearly interpolated value between 0 and the maximum value.
    FLOAT GetLinearValue() const { return m_fValue; }

    // GetSmoothValue returns the value between 0 and the maximum value, but applies a cosine-shaped smoothing function.
    FLOAT GetSmoothValue() const { return 0.5f - 0.5f * cosf( GetLinearValue() * XM_PI ); }
};

// LerpVector is a convenient function for modifying a vector in place with a lerp operation.
inline VOID LerpVector( XMVECTOR& vDestSrcValue, const XMVECTOR vLerpValue, FLOAT T )
{
    XMVECTOR vFinalValue = XMVectorLerp( vDestSrcValue, vLerpValue, T );
    vDestSrcValue = vFinalValue;
}

// CompareAgainstThreshold is a convenient function for comparing a value against min/max thesholds, with clamping.
inline FLOAT CompareAgainstThreshold( const FLOAT fValue, const FLOAT fMinValue, const FLOAT fMaxValue )
{
    FLOAT fComparison = ( fValue - fMinValue ) / ( fMaxValue - fMinValue );
    return max( 0.0f, min( 1.0f, fComparison ) );
}

// Macros for parsing the skeleton position tracking state.
inline BOOL IsTracked( const NUI_SKELETON_DATA* pSkeleton, NUI_SKELETON_POSITION_INDEX Index )
{
    return pSkeleton->eSkeletonPositionTrackingState[Index] == NUI_SKELETON_POSITION_TRACKED;
}

inline BOOL IsTrackedOrInferred( const NUI_SKELETON_DATA* pSkeleton, NUI_SKELETON_POSITION_INDEX Index )
{
    return pSkeleton->eSkeletonPositionTrackingState[Index] != NUI_SKELETON_POSITION_NOT_TRACKED;
}

class FrameFilterStack;

//--------------------------------------------------------------------------------------
// Name: class IFilterDebugRenderer
// Desc: Defines the interface by which skeleton filters emit debug output.
//--------------------------------------------------------------------------------------
class IFilterDebugRenderer
{
public:
    // SetTextYPos sets the current vertical position.  Note that drawing text lines or empty lines will also change the vertical position.
    virtual VOID SetTextYPos( FLOAT fYPos ) = NULL;

    // Retrieves the current vertical position.
    virtual FLOAT GetTextYPos() const = NULL;

    // Draws a line of text, with an optional newline inserted at the end.
    virtual VOID DrawTextLine( FLOAT fXScale, FLOAT fYScale, DWORD Color, FLOAT fRightOffset, BOOL bNewline, const WCHAR* strFormat, ... ) = NULL;

    // Increments the cursor by one vertical line.
    virtual VOID DrawEmptyLine( FLOAT fYScale ) = NULL;

protected:
    friend class FrameFilterStack;
    virtual VOID StartRendering() = NULL;
    virtual VOID EndRendering() = NULL;
};

//--------------------------------------------------------------------------------------
// Name: struct LineEvaluation
// Desc: Holds results of a linear walk across the depth image.
//--------------------------------------------------------------------------------------
struct LineEvaluation
{
    BOOL    m_bConsistentPlayerID;
    BOOL    m_bValidPlayerID;
    DWORD   m_dwPlayerID;
    FLOAT   m_fBeginDepth;
    FLOAT   m_fEndDepth;
    FLOAT   m_fMinDepth;
    FLOAT   m_fMaxDepth;
    BOOL    m_bConsistentLinearDepth;
};

//--------------------------------------------------------------------------------------
// Name: class DepthImageProcessing
// Desc: Contains functions for obtaining information from a sensor depth image.
//--------------------------------------------------------------------------------------
class DepthImageProcessing
{
protected:
    DWORD m_dwImageWidth;
    DWORD m_dwImageHeight;
    DWORD m_dwSmallImageWidth;
    DWORD m_dwSmallImageHeight;
    USHORT* m_pDepthImageCopy;
    USHORT* m_pSmallDepthImageCopy;

public:
    DepthImageProcessing( const DWORD dwImageWidth = 320, const DWORD dwImageHeight = 240, const DWORD dwImageWidthSmall = 80, const DWORD dwImageHeightSmall = 60 )
    {
        m_dwImageWidth = dwImageWidth;
        m_dwImageHeight = dwImageHeight;
        m_dwSmallImageWidth = dwImageWidthSmall;
        m_dwSmallImageHeight = dwImageHeightSmall;
        m_pDepthImageCopy = new USHORT[ m_dwImageWidth * m_dwImageHeight ];
        ZeroMemory( m_pDepthImageCopy, m_dwImageWidth * m_dwImageHeight * sizeof(USHORT) );
        m_pSmallDepthImageCopy = new USHORT[ m_dwSmallImageWidth * m_dwSmallImageHeight ];
        ZeroMemory( m_pSmallDepthImageCopy, m_dwSmallImageWidth * m_dwSmallImageHeight * sizeof(USHORT) );
    }
    ~DepthImageProcessing()
    {
        delete[] m_pDepthImageCopy;
        delete[] m_pSmallDepthImageCopy;
    }
    VOID CopyDepthTexture( IDirect3DTexture9* pDepthTexture, IDirect3DTexture9* pSmallDepthTexture );

    BOOL EvaluateLine( const XMVECTOR vBeginPos, const XMVECTOR vEndPos, LineEvaluation* pEvaluation, DWORD dwSampleCount = 0 );

protected:
    VOID CaptureLine( const XMVECTOR vBeginPos, const XMVECTOR vEndPos, USHORT* pPixels, DWORD dwSampleCount );
};

//--------------------------------------------------------------------------------------
// Name: class SkeletonCommonProcessing
// Desc: Contains common skeleton processing functions that filters can use as input data.
//       The functionality includes reformatted data from the NUI_SKELETON_DATA struct,
//       as well as higher-level data such as limb lengths, body basis vectors, arm positions,
//       and bone length historical data.
//--------------------------------------------------------------------------------------
class SkeletonCommonProcessing
{
protected:
    DWORD m_dwLastFrameProcessed;

public:
    template< class T >
    struct SkeletonJoints
    {
        T HipCenter;
        T Spine;
        T ShoulderCenter;
        T Head;
        T ShoulderLeft;
        T ElbowLeft;
        T WristLeft;
        T HandLeft;
        T ShoulderRight;
        T ElbowRight;
        T WristRight;
        T HandRight;
        T HipLeft;
        T KneeLeft;
        T AnkleLeft;
        T FootLeft;
        T HipRight;
        T KneeRight;
        T AnkleRight;
        T FootRight;

        VOID Initialize( const T InitialValue )
        {
            DWORD Size = sizeof(SkeletonJoints<T>) / sizeof(T);
            T* pValues = (T*)&HipCenter;
            for( DWORD i = 0; i < Size; ++i )
            {
                pValues[i] = InitialValue;
            }
        }

        T& operator[] (DWORD Index) { return *( (&HipCenter) + Index ); }
    };

    DWORD SkeletonIndex;

    // Joint information, reformatted from the skeleton
    SkeletonJoints<BOOL> Tracked;
    SkeletonJoints<BOOL> TrackedOrInferred;
    SkeletonJoints<XMVECTOR> Position;

    // Bone information
    XMVECTOR LeftFemur;
    XMVECTOR LeftFemurLength;
    XMVECTOR RightFemur;
    XMVECTOR RightFemurLength;
    XMVECTOR AverageFemurVector;
    XMVECTOR AverageFemurLength;

    XMVECTOR LeftLowerLeg;
    XMVECTOR LeftLowerLegLength;
    XMVECTOR RightLowerLeg;
    XMVECTOR RightLowerLegLength;

    XMVECTOR LeftClavicle;
    XMVECTOR LeftClavicleLength;
    XMVECTOR RightClavicle;
    XMVECTOR RightClavicleLength;

    // Body basis (orthonormal basis)
    BOOL BodyBasisValid;
    XMVECTOR BodyUp;
    XMVECTOR BodyRight;
    XMVECTOR BodyForward;

    // Arm information - the shoulder, elbow, and wrist positions are
    // analyzed and classified into one of four poses.
    // Note that tracking state is not considered in analyzing the joint
    // positions; the intent is to create a higher-level classification of
    // the arm joint positions, regardless of how well the position values
    // are tracked.  Filters may decide to combine this information with
    // tracking state to better decide how the position data should be trusted.
    enum ArmArrangement
    {
        AA_RaisedUp = 0,       // arm reaching up
        AA_Down,               // arm reaching down or at side
        AA_ElbowLowWristHigh,  // arm goes down and then back up
        AA_ElbowHighWristLow,  // arm goes up and then back down; this is a painful pose - scrutinize it for plausibility
    };
    enum ArmDotProduct
    {
        ADP_Shoulder,
        ADP_Elbow,
        ADP_Wrist,
        ADP_Count
    };

    ArmArrangement LeftArmArrangement;
    ArmArrangement RightArmArrangement;
    FLOAT LeftArmDotProducts[ADP_Count];
    FLOAT RightArmDotProducts[ADP_Count];

    // Historical values
    struct HistoricalValues
    {
        XMVECTOR TorsoLength;
        XMVECTOR SpineLength;
        XMVECTOR WristLength;
        XMVECTOR NeckLength;
        XMVECTOR LeftFemurLength;
        XMVECTOR RightFemurLength;
        XMVECTOR LeftLowerLegLength;
        XMVECTOR RightLowerLegLength;
        XMVECTOR LeftUpperArmLength;
        XMVECTOR RightUpperArmLength;
        XMVECTOR LeftLowerArmLength;
        XMVECTOR RightLowerArmLength;
    };
    HistoricalValues History;

public:
    SkeletonCommonProcessing();

    VOID Reset();
    
    // UpdateFromSkeleton may be called multiple times per frame, before the first filter executes and after each filter modifies the skeleton.
    VOID UpdateFromSkeleton( const NUI_SKELETON_DATA* pSkeleton );

    // UpdateHistoricalValues should be called once per frame, before any filters are executed.
    VOID UpdateHistoricalValues( const XMVECTOR vDeltaNuiTime );

    VOID DebugRenderUI( IFilterDebugRenderer* pRenderer );

    // Static method for a time-based lerp of an incoming sample with the current value (the destination).
    static VOID AccumulateHistoricalSample( XMVECTOR& vDestination, const XMVECTOR vSample, const XMVECTOR vDeltaTime, const XMVECTOR vTimeWindow );
};

//--------------------------------------------------------------------------------------
// Name: struct FrameFilterContext
// Desc: The data passed as per-frame input for a frame filter.
//--------------------------------------------------------------------------------------
struct FrameFilterContext
{
    FLOAT fDeltaTime;
    DepthImageProcessing* pDepthProcess;
    SkeletonCommonProcessing* pProcess[NUI_SKELETON_COUNT];
};

//--------------------------------------------------------------------------------------
// Name: class ISkeletonFilter
// Desc: Virtual base class used for all skeleton filters.
//--------------------------------------------------------------------------------------
class IFrameFilter
{
public:
    virtual VOID Initialize() {}
    virtual VOID Terminate() {}

    // Display name of the filter.
    virtual const WCHAR* GetName() const { return L"Untitled Filter"; }

    virtual VOID ResetFilterState() = NULL;

    // FilterSkeleton is where the filter does its work.
    // pProcess is a set of processed values based on input skeleton.  Historical values are based on the original raw skeleton before filters are applied.
    // pInputSkeleton is the current state of the skeleton, it could be the raw skeleton from NUI or it could be the result from
    // the previous filter.
    // pOutputSkeleton is initialized to be identical to pInputSkeleton, for convenience.  It can be modified by the filter implementation.
    // If pOutputSkeleton is modified at all, the method returns TRUE, else it returns FALSE for filters that simply inspect pInputSkeleton.
    virtual BOOL FilterFrame( FrameFilterContext* pContext, const NUI_SKELETON_FRAME* pInputFrame, NUI_SKELETON_FRAME* pOutputFrame ) = NULL;

    virtual VOID DebugRenderUI( IFilterDebugRenderer* pDebugRenderer ) {}
};

//--------------------------------------------------------------------------------------
// Name: struct SkeletonFilterContext
// Desc: The data passed as per-frame input for a skeleton filter.
//--------------------------------------------------------------------------------------
struct SkeletonFilterContext
{
    const SkeletonCommonProcessing* pProcess;
    DepthImageProcessing* pDepthProcess;
    const NUI_SKELETON_FRAME* pSkeletonFrame;
};

//--------------------------------------------------------------------------------------
// Name: class ISkeletonFilter
// Desc: Virtual base class used for all skeleton filters.
//--------------------------------------------------------------------------------------
class ISkeletonFilter
{
public:
    virtual VOID Initialize() {}
    virtual VOID Terminate() {}

    // Display name of the filter.
    virtual const WCHAR* GetName() const { return L"Untitled Filter"; }
    
    // If the underlying tracked skeleton changes (to a different player, etc) then ResetFilterState will be called.
    virtual VOID ResetFilterState() = NULL;

    // FilterSkeleton is where the filter does its work.
    // pProcess is a set of processed values based on input skeleton.  Historical values are based on the original raw skeleton before filters are applied.
    // pInputSkeleton is the current state of the skeleton, it could be the raw skeleton from NUI or it could be the result from
    // the previous filter.
    // pOutputSkeleton is initialized to be identical to pInputSkeleton, for convenience.  It can be modified by the filter implementation.
    // If pOutputSkeleton is modified at all, the method returns TRUE, else it returns FALSE for filters that simply inspect pInputSkeleton.
    virtual BOOL FilterSkeleton( SkeletonFilterContext* pContext, const NUI_SKELETON_DATA* pInputSkeleton, NUI_SKELETON_DATA* pOutputSkeleton, const FLOAT fDeltaNuiTime ) = NULL;

    virtual VOID DebugRenderUI( IFilterDebugRenderer* pDebugRenderer ) {}
};

//--------------------------------------------------------------------------------------
// Name: class FrameFilterStack
// Desc: Implements a stack of skeleton filters that are called in sequence to post process
// skeleton data.  Filters can be grouped by bit mask to selectively include and exclude
// individual filters.  The filter stack also automates some common processing functions
// before each filter invocation to provide more useful inputs to each filter.
//--------------------------------------------------------------------------------------
class FrameFilterStack
{
private:
    struct FilterEntry
    {
        DWORD Mask;
        IFrameFilter* pFilter;
    };
    typedef std::vector<FilterEntry> FrameFilterVector;

protected:
    NUI_SKELETON_FRAME m_SkeletonFrame[2];
    NUI_SKELETON_FRAME* m_pOutputSkeletonFrame;
    FrameFilterVector m_Filters;
    FrameFilterContext m_FilterContext;
    SkeletonCommonProcessing m_CommonProcessing[NUI_SKELETON_MAX_TRACKED_COUNT];
    DepthImageProcessing m_DepthProcessing;
    BOOL m_bEnabled;

public:
    FrameFilterStack();

    VOID Initialize();
    VOID AddFilter( IFrameFilter* pFilter, DWORD Mask = 0xFFFFFFFF );

    DWORD GetFilterCount() const { return m_Filters.size(); }
    DWORD GetFilterMask( DWORD Index ) const { return m_Filters[Index].Mask; }
    const IFrameFilter* GetFilter( DWORD Index ) const { return m_Filters[Index].pFilter; }

    BOOL IsEnabled() const { return m_bEnabled; }
    VOID SetEnabled( BOOL Enabled ) { m_bEnabled = Enabled; }

    VOID ResetFilterState();
    VOID FilterDepthTexture( IDirect3DTexture9* pDepthTexture, IDirect3DTexture9* pSmallDepthTexture );
    const NUI_SKELETON_FRAME* FilterFrame( const NUI_SKELETON_FRAME* pInputSkeletonFrame, const FLOAT fDeltaNuiTime, const DWORD Mask = 0xFFFFFFFF );
    const NUI_SKELETON_FRAME* GetSkeletonFrame() const { return m_pOutputSkeletonFrame; }

    VOID DebugRenderUI( IFilterDebugRenderer* pDebugRenderer, const DWORD Mask = 0xFFFFFFFF );
};

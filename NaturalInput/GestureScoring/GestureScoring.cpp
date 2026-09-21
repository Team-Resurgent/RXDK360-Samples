//--------------------------------------------------------------------------------------
// GestureScoring.cpp
//
// This sample demonstrates two ways to score a gesture. The gestures are motion capture
// animations in BVH files, and we match them with what the NUI skeleton is doing.
//
// The skeletons could be different for the animations as long as we can remap
// BVH to NUI.
//
// Scoring is done by searching the animation for the best matching frame. The
// best matching frame is the one that has minimal local angular difference from
// the animation being matched.
//
// Alternatively, scoring can be done by using a DTW classifier -- that works best
// on moving gestures, not static scenes. DTW classifier can handle out of time
// movements better.
//
// If ST fails for a joint, we use the depth buffer to see whether we can find it at
// the expected location.
//
// Advanced Technology Group
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include <xtl.h>
#include <xgraphics.h>
#include <x3daudio.h>
#include <xbdm.h>

#include <AtgApp.h>
#include <AtgFont.h>
#include <AtgHelp.h>
#include <AtgInput.h>
#include <AtgSimpleShaders.h>
#include <AtgUtil.h>
#include <AtgNuiCommon.h>
#include <AtgNuiVisualization.h>
#include <AtgDebugDraw.h>

#include <algorithm>

#include "BVHAnimation.h"
#include "AnimationAdapter.h"
#include "AnimationState.h"


//--------------------------------------------------------------------------------------
// Callouts for labeling the gamepad on the help screen
//--------------------------------------------------------------------------------------
ATG::HELP_CALLOUT g_HelpCallouts[] =
{
    { ATG::HELP_BACK_BUTTON,    ATG::HELP_PLACEMENT_1, L"Display Help" },
    { ATG::HELP_X_BUTTON,       ATG::HELP_PLACEMENT_1, L"Switch classifier" },
    { ATG::HELP_B_BUTTON,       ATG::HELP_PLACEMENT_1, L"Cancel Game" },
    { ATG::HELP_A_BUTTON,       ATG::HELP_PLACEMENT_1, L"Play Game/Skip Demo" },
    { ATG::HELP_Y_BUTTON,       ATG::HELP_PLACEMENT_1, L"Pause Animation" },
    { ATG::HELP_DPAD,           ATG::HELP_PLACEMENT_2, L"Left/right changes animation\nDown starts detect mode" }
};

// various UI and graphics related constants
static const DWORD  g_dwFrameWidth          = 1280;
static const DWORD  g_dwFrameHeight         = 720;
static const DWORD	CLR_GOOD_PSNR           = 0xff2020ff;
static const DWORD	CLR_PASS_PSNR           = 0xffff0000;
static const DWORD	CLR_SCORE               = 0xff00ff00;
static const DWORD  CLR_MARKER              = 0xff00ffff;
static const DWORD  CLR_DEPTH_INVALID       = 0xff0000ff;
static const DWORD  CLR_DEPTH_BAD           = 0xffff0000;
static const DWORD  CLR_DEPTH_GOOD          = 0xff00ff00;
static const DWORD  CLR_DEPTH_OCCLUDED      = 0xffffff00;
static const DWORD  CLR_WHITE               = 0xffffffff;
static const DWORD  CLR_GREY                = 0xff808080;
static const DWORD  WINDOW_START_X          = 0;
static const DWORD  WINDOW_START_Y          = 0;
static const DWORD  WINDOW_WIDTH            = 1280;
static const DWORD  WINDOW_HEIGHT           = 720;
static const FLOAT  PIP_DRAW_WIDTH          = 150.0f;
static const FLOAT  PIP_DRAW_HEIGHT         = PIP_DRAW_WIDTH * 3.0f / 4.0f;
static const FLOAT  PIP_DRAW_X              = WINDOW_WIDTH * 0.1f;
static const FLOAT  PIP_DRAW_Y              = WINDOW_HEIGHT * 0.9f - PIP_DRAW_HEIGHT;
static const FLOAT  HISTORY_START_X         = 350;
static const FLOAT  HISTORY_END_Y           = 70;
static const FLOAT  HISTORY_HEIGHT          = 70;
static const UINT   RESIZED_DEPTH_WIDTH     = 320;
static const UINT   RESIZED_DEPTH_HEIGHT    = 240;
static const FLOAT  DTW_DEBUG_X             = -256;
static const FLOAT  DTW_DEBUG_Y             = -256;
static const FLOAT  DTW_DEBUG_W             = 256;
static const FLOAT  DTW_DEBUG_H             = 256;
static const FLOAT  SCORE_LABEL_POS_X       = 1000;
static const FLOAT  SCORE_LABEL_POS_Y       = 20;
static const FLOAT  CENTER_LABEL_POS_X      = 500;
static const FLOAT  CENTER_LABEL_POS_Y      = 220;
static const FLOAT  JOINT_SPHERE_RADIUS     = 0.03f;
static const FLOAT  BOXMAN_LIMB_WIDTH       = 0.02f;
static const FLOAT  ICON_SIZE               = 80;

// a flag for the fence to indicate when we need to reset scoring
static const DWORD  FENCE_RESET_TRACKING    = 0x10000000;
static const DWORD  FENCE_ANIM_IDX_OFFSET   = 16;

// "game" related score interpretation and other "game" related constants
static const FLOAT  SCORE_FAIL              = 0.10f;
static const FLOAT  SCORE_GOOD              = 0.20f;
static const FLOAT  SCORE_VERY_GOOD         = 0.30f;
static const FLOAT  SCORE_PERFECT           = 0.40f;
static const FLOAT  TIMER_COUNTDOWN         = 3.f;
static const FLOAT  TIMER_COUNTDOWN_RESULTS = 1.f;
static const FLOAT  TIMER_COUNTDOWN_DISMISS_RESULTS = 3.f;

// what is the max error we take in our PSNR calculations
static const FLOAT  MAX_JOINT_ERROR         = 1.f;
static const FLOAT  MAX_MSE                 = 1.f;
static const INT    DTW_LAG_CORRIDOR        = 30;   // this is an implicit search "radius" in time

// different scores for the PSNR interpretation by the game
static const FLOAT	GOOD_PSNR               = 32.f; // excellent performance if PSNR is like this
static const FLOAT	PASS_PSNR_3             = 20.f;
static const FLOAT	PASS_PSNR_2             = 18.f;

// this amount of look back and look ahead should be enough for the KN tracker
static const INT    LAG_SEARCH_RANGE        = 10;
static const INT    LAG_SEARCH_AHEAD        = 2;
static const FLOAT  FRACTION_PENALTY_FRAMES = 4.f;
static const FLOAT  SCORE_PENALTY           = 0.2f;

// when the NUI skeleton is not found we render it at this position
static const XMVECTOR   DEFAULT_SKELETON_POS = XMVectorSet( 0, 0, 2.5f, 1 );

// reprojection constants for working with depth buffers
static const FLOAT  TAN_VERT_CAMERA_FOV        = tanf( XMConvertToRadians( 0.5f * NUI_CAMERA_DEPTH_NOMINAL_VERTICAL_FOV ) );
static const FLOAT  TAN_HORZ_CAMERA_FOV        = tanf( XMConvertToRadians( 0.5f * NUI_CAMERA_DEPTH_NOMINAL_HORIZONTAL_FOV ) );

static const FLOAT  FOVH_PER_PIXEL_320x240_INV = 0.5f * 320.f / TAN_HORZ_CAMERA_FOV;
static const FLOAT  FOVV_PER_PIXEL_320x240_INV = 0.5f * 240.f / TAN_VERT_CAMERA_FOV;

static const FLOAT  FOVH_PER_PIXEL_1x1_INV     = 0.5f / TAN_HORZ_CAMERA_FOV;
static const FLOAT  FOVV_PER_PIXEL_1x1_INV     = 0.5f / TAN_VERT_CAMERA_FOV;

static const FLOAT  FOVH_PER_PIXEL_320x240     = TAN_HORZ_CAMERA_FOV / (0.5f * 320.f);
static const FLOAT  FOVV_PER_PIXEL_320x240     = TAN_VERT_CAMERA_FOV / (0.5f * 240.f);


//--------------------------------------------------------------------------------------
// Name: PIXNamedEventScoped
// Desc: Starts and stops the pix marker
//--------------------------------------------------------------------------------------
struct PIXNamedEventScoped
{
    PIXNamedEventScoped( const CHAR* name )
    {
        PIXBeginNamedEvent( 0, name );
    }

    ~PIXNamedEventScoped()
    {
        PIXEndNamedEvent();
    }
};


#define MAKE_UNIQUE_PNE_NAME( line ) pne ## line
#define CREATE_UNIQUE_PNE_INSTANCE( name, line ) PIXNamedEventScoped MAKE_UNIQUE_PNE_NAME( line ) ( name )
#define PIX_SCOPED_EVENT( name ) CREATE_UNIQUE_PNE_INSTANCE( name, __LINE__ );

//--------------------------------------------------------------------------------------
// Name: Skeleton
// Desc: Our representation of the NUI skeleton
//--------------------------------------------------------------------------------------
struct Skeleton
{
    XMVECTOR m_skeletonOffsets[ NUI_SKELETON_POSITION_COUNT ];
    NUI_SKELETON_POSITION_TRACKING_STATE m_skeletonPositionTrackingState[ NUI_SKELETON_POSITION_COUNT ];
};

//--------------------------------------------------------------------------------------
// Name: PSNRRecord
// Desc: A useful way of passing around the amount of error per joint
//--------------------------------------------------------------------------------------
struct PSNRRecord
{
    FLOAT   m_jointDistanceFromRef[ NUI_SKELETON_POSITION_COUNT ];
    BOOL    m_jointReliable[ NUI_SKELETON_POSITION_COUNT ];
    BOOL    m_jointPartOfCalculation[ NUI_SKELETON_POSITION_COUNT ];

    FLOAT   CalculateMSE() const;
};


//--------------------------------------------------------------------------------------
// Name: KNClassifier
// Desc: K Nearest Classifier
//
// Classically, the "training set" is the set vectors in n-dimensional "feature space"
// and the K-Nearest classifier tries to find the closest match in that space.
//
// In our case, each vector is actually the animation itself of 20 bones over the
// duration of the animation.
//
// The search is done over that space using angular distance, one frame at a time.
//--------------------------------------------------------------------------------------
class KNClassifier
{
private:
    FLOAT                   m_fPSNR;                // current PSNR
    FLOAT                   m_fScoreAccum;          // current score
    FLOAT                   m_fNumFrames;           // number of seen frames
    UINT                    m_uNumPenaltyFrames;    // number of penalty frames
    PSNRRecord              m_psnrRecord;
public:

    KNClassifier();

    FLOAT   GetPSNR() const;
    FLOAT   GetScore() const;
    FLOAT   GetJointScore( UINT i ) const;
    BOOL    GetJointReliability( UINT i ) const;
    VOID    ScoreNextFrame( const Skeleton* pSkeleton,
                            const FLOAT* pBonesLength,
                            AnimationState& animToScore,
                            FLOAT fBadPSNR,
                            DWORD dwJointsMask,
                            XMVECTOR vOrg,
                            UINT uDisplayedAnimFrame,
                            BOOL bPenaliseLag,
                            const D3DLOCKED_RECT* pRcDepth,
                            const D3DLOCKED_RECT* pRcDebug );
    VOID    Reset();
};

//--------------------------------------------------------------------------------------
// Name: DTWClassifier
// Desc: Dynamic Time Warping classifier
//
// Uses a dynamic programming approach to figuring out how far two sequences are from
// each other
//--------------------------------------------------------------------------------------
class DTWClassifier
{
private:
    struct NuiFrame
    {
        // we have to cache cost matrix entries, we can't afford to calculate thousands
        // of pairwise distances per frame
        static const UINT   CACHE_SIZE = 128;           // this number should be >= number of
                                                        // frames in the longest animation

        Skeleton    m_skeleton;                 // skeleton offsets
        BOOL        m_bTracked;                 // whether valid
        FLOAT       m_costCache[ CACHE_SIZE ];  // cost cache
        UINT        m_costCacheValid[ ((31 + CACHE_SIZE) / 32) ];   // bit vector

        void    ResetCostCache()
        {
            for( UINT i=0; i < ARRAYSIZE( m_costCacheValid ); ++i )
                m_costCacheValid[ i ] = 0;
        }

        BOOL    GetCostCached( FLOAT& v, UINT uIdx ) const
        {
            assert( uIdx < ARRAYSIZE( m_costCache ) );

            const UINT  uSlotIdx = uIdx / 32;
            const UINT  uBitIdx = uIdx & 31;

            v = m_costCache[ uIdx ];

            return m_costCacheValid[ uSlotIdx ] & (1 << uBitIdx);
        }

        void    SetCostCached( FLOAT v, UINT uIdx )
        {
            assert( uIdx < ARRAYSIZE( m_costCache ) );

            const UINT  uSlotIdx = uIdx / 32;
            const UINT  uBitIdx = uIdx & 31;

            m_costCache[ uIdx ] = v;

            m_costCacheValid[ uSlotIdx ] |= (1 << uBitIdx);
        }
    };

    // time warping path step
    enum Step
    {
        STEP_NONE = 0,
        STEP_DIAG = 1,
        STEP_HORZ = 2,
        STEP_VERT = 3
    };

    NuiFrame                m_history[ 30 * 4 ];    // up to 4 seconds history
    UINT                    m_uHistoryTail;         // one past the last item
    FLOAT                   m_fRmse;                // current error averaged on the path
    FLOAT                   m_fRmseMax;             // max error on shortest path
    FLOAT                   m_fRmseMin;             // min error on shortest path
    FLOAT                   m_fScoreAccum;          // current score
    AnimationState*         m_pAnimToScore;         // animation we're scoring
    DWORD                   m_dwJointsMask;         // cached from GestureAnimation
    FLOAT                   m_fBadPSNR;             // bad PSNR for the anim
    FLOAT*                  m_pMatCost;             // global cost matrix
    FLOAT*                  m_pMatCostLocal;        // local cost matrix (debug)
    SHORT*                  m_pPath;                // path xy
    BYTE*                   m_pMatPath;             // path matrix
    UINT                    m_uLeftEdge;            // lowest score sequence left edge
    UINT                    m_uRightEdge;           // lowest score sequence right edge
    UINT                    m_uNumStepsInPath;      // how long the current path is
    UINT                    m_uSequenceASize;       // how many columns we have (NUI)
    UINT                    m_uSequenceBSize;       // how many rows we have (ANIM)
public:
    DTWClassifier();
    ~DTWClassifier();

    void            SetAnimation( AnimationState& animToScore, DWORD dwJoints, FLOAT fBadPSNR );
    FLOAT           GetPSNR() const;
    FLOAT           GetScore() const;
    VOID            ScoreNextFrame( const Skeleton* pSkeleton );
    VOID            Reset();
    VOID            RenderMatrix( D3DDevice* pDevice, FLOAT fX, FLOAT fY, FLOAT fWidth, FLOAT fHeight ) const;

private:
    UINT GetNumValidNuiFrames( UINT uMaxLength ) const;
    static UINT RetracePath( INT& iLeftEdge, SHORT* pPath, BYTE* pMatPath, INT ii, INT jj, UINT uSequenceASize );
};



//--------------------------------------------------------------------------------------
// Name: ScopedViewportChange
// Desc: This changes the viewport in the constructor and restores it in the destructor
//--------------------------------------------------------------------------------------
class ScopedViewportChange
{
private:
    D3DDevice*      m_pDevice;
	D3DVIEWPORT9	m_saveVp;

public:
    ScopedViewportChange( D3DDevice* pDev, FLOAT fX, FLOAT fY, FLOAT fWidth, FLOAT fHeight )
    {
        m_pDevice = pDev;

	    m_pDevice->GetViewport( &m_saveVp );

        const D3DRECT rcSafe = ATG::GetTitleSafeArea();

        D3DVIEWPORT9	newVp = m_saveVp;

        newVp.Height = static_cast< UINT >( fHeight );
        newVp.Width = static_cast< UINT >( fWidth );

        if( fX < 0 )
            newVp.X = static_cast< UINT >( rcSafe.x2 + fX );
        else
            newVp.X = static_cast< UINT >( fX + rcSafe.x1 );

        if( fY < 0 )
            newVp.Y = static_cast< UINT >( rcSafe.y2 + fY );
        else
            newVp.Y = static_cast< UINT >( fY + rcSafe.y1 );

        m_pDevice->SetViewport( &newVp );

        m_pDevice->SetRenderState( D3DRS_VIEWPORTENABLE, TRUE );
    }

    ~ScopedViewportChange()
    {
        m_pDevice->SetViewport( &m_saveVp );
    }
};


//--------------------------------------------------------------------------------------
// Name: Sample
// Desc: Main class to run this application.
//--------------------------------------------------------------------------------------
class Sample : public ATG::Application
{
public:
    Sample() :  m_bDrawHelp( FALSE )
    {
    }

    virtual ~Sample()
    {
        NuiSkeletonTrackingDisable();
        NuiShutdown( );
    }

private:
    static const UINT   NUM_KN_TRACKERS = 150;          // number of k-nearest trackers for gesture detection
    static const UINT   NUM_HISTORY_FRAMES = 330;       // how many frames of history to display
    static const UINT   NUM_ANIMATIONS = 7;             // number of animations we use

    // state of the game
    enum DEMO_STATE
    {
        STATE_DEFAULT,
        STATE_COUNTDOWN_BEFORE_SHOW,
        STATE_SHOW,
        STATE_COUNTDOWN_BEFORE_TRY,
        STATE_TRY,
        STATE_SHOW_RESULTS,
        STATE_DETECT_GESTURE
    };

    // significant animations, this indexes into ms_animations
    enum ANIMATION_NAME
    {
        ANIM_RIGHT_WAVE0 = 1,
        ANIM_RIGHT_WAVE1 = 2,
        ANIM_LEFT_WAVE0 =  4,
        ANIM_LEFT_WAVE1 =  5,
        ANIM_START      = 3,
    };

    // we use this to track when the frame is displayed to user
    struct FrameFence
    {
        DWORD   m_dwFence;                  // D3D fence value returned by InsertFence
        UINT    m_uAnimationData;           // associated data
    };

    // animation we're scoring against
    struct GestureAnimation
    {
        const CHAR*         m_pFilename;    // BVH filename
        BOOL                m_bMirror;      // whether or not to mirror the animation
        const WCHAR*        m_pDisplayName; // what it's called in the "game"
        UINT                m_uEndFrame;    // end frame of the animation
        FLOAT               m_fBadPSNR;     // what is considered to be bad PSNR for this anim
        DWORD               m_dwJointsMask; // bit mask of joints we score
        AnimationAdapter    m_adapter;      // BVH to NUI adapter
        AnimationState      m_animToShow;   // animation state we're showing
        AnimationState      m_animToScore;  // animation state we're scoring
    };

    // this keeps some data along with the classifier to help detect which gesture we're doing
    struct KNTracker
    {
        KNClassifier    m_classifier;       // tracker
        USHORT          m_uAnim;            // animation we're tracking
        USHORT          m_uCurFrame;        // current animation frame
    };

    virtual HRESULT Initialize();
    virtual HRESULT Update();
    virtual HRESULT Render();

    FLOAT           GetGameClassifierScore() const;
    FLOAT           GetGameClassifierPSNR() const;
    VOID            ResetGameClassifier();

    HRESULT         UpdateSkeletonTracking();
    VOID            UpdateGameStateMachine( const ATG::GAMEPAD* pGamepad, FLOAT fDeltaTime );
    BOOL            IsLookingForRightWaveGesture() const;
    BOOL            IsLookingForLeftWaveGesture() const;
    VOID            DrawEnterPlayspaceLabel() const;
    VOID            RenderUIOverlays( BOOL bBare ) const;
    VOID            RenderTextureOverlays( BOOL bDrawDebugTexture ) const;
    VOID            RenderMainView( BOOL bRenderAnimationSkeleton = TRUE ) const;
    VOID            RenderHistoryGraph() const;
    VOID            RenderDetectionUI() const;
    VOID            RenderAnimationIcon( FLOAT fX, FLOAT fY, FLOAT fWidth, FLOAT fHeight,
                                       UINT uAnimIndex, UINT uFrame, DWORD dwColor ) const;
    VOID            RenderSkeletonsAndLabels( XMMATRIX matViewProj, BOOL bRenderAnimationSkeleton ) const;
    VOID            RenderLabels( XMMATRIX matViewProj, const GestureAnimation& anim ) const;
    VOID            RenderNuiStickman( XMMATRIX matViewProj, const NUI_SKELETON_DATA* pSkeleton ) const;
    VOID            RenderBoxman( const XMVECTOR* pJoints, DWORD clr ) const;
    VOID            GetTrackerKNInfo( UINT* pNumTrackers, UINT* pBestTracker ) const;

    BOOL            CapturePlayersDepth( IDirect3DTexture9* pDepthTexture, const UINT uSkeletonIndex );
    VOID            SetCurAnimation( UINT uSeq );
    VOID            SetBonesLengthsFromAnimation( UINT uAnimIndex );
    VOID            SetBonesLengthsFromNUI();

    VOID            UpdateAnimationScoring( FLOAT fDeltaTime );
    VOID            UpdateVisibleAnimation( FLOAT fDeltaTime, BOOL bLoop );

    VOID            InsertFence();
    UINT            GetLastCrossedFence();

    VOID            DetectGestureKN();
    VOID            DetectGestureDTW();

    mutable ATG::Timer          m_Timer;
    mutable ATG::Font           m_Font16;
    mutable ATG::Help           m_Help;
    BOOL                        m_bDrawHelp;

    // Rendering surfaces and textures
	D3DTexture*					m_depthTexture;
	D3DTexture*					m_debugTexture;

    // NUI tracking related
    NUI_SKELETON_FRAME          m_SkeletonFrame;
    UINT                        m_uCurrentSkeletonIndex;
    UINT                        m_uCurrentTrackingID;
    HRESULT                     m_hrNuiResult;

    // Visualize the depth and color buffers
    CONST NUI_IMAGE_FRAME*      m_pImageFrame;
    CONST NUI_IMAGE_FRAME*      m_pDepthFrame;
    ATG::NuiVisualization       m_pip;
    HANDLE                      m_hImage;
    HANDLE                      m_hDepth;
    HANDLE                      m_hFrameEndEvent;

    // skeletons rendering
    FLOAT                       m_fModelAngle;
    XMMATRIX                    m_matNuiToDepth;

    // our tracker for the main game mode
    BOOL                        m_bUseKNClassifier;
    KNClassifier                m_classifierKN;
    DTWClassifier               m_classifierDTW;

    // PSNR history for displaying the graph
    FLOAT                       m_PSNRHistory[ NUM_HISTORY_FRAMES ];
    FLOAT                       m_ScoreHistory[ NUM_HISTORY_FRAMES ];
    UINT                        m_uCurPSNRHistoryTail;

    // NUI skeleton information
    BOOL                        m_bWatchingSkeleton;
    Skeleton                    m_skeletonData;
    NUI_SKELETON_DATA           m_nuiSkeletonData;

    // fences to gauge the system lag
    FrameFence                  m_frameFenceBuffer[ 4 ];
    UINT                        m_uCurFrame;
    BOOL                        m_bNextFenceResetsTracking;

    // game state
    DEMO_STATE                  m_state;
    FLOAT                       m_fStateTimerCountdown;
    FLOAT                       m_fScore;

    // animations
    static GestureAnimation     ms_animations[];
    UINT                        m_uCurShowAnimation;
    FLOAT                       m_skeletonBonesLengths[ NUI_BONE_COUNT ];
    BOOL                        m_bPausedAnimation;

    // gesture detection
    BOOL                        m_bNewDetectedGesture;
    UINT                        m_uDetectedGesture;
    UINT                        m_uFrameWhenGestureDetected;
    FLOAT                       m_fDetectedGestureScore;

    // gesture detection based on KNClassifier
    KNTracker                   m_KNTrackers[ NUM_KN_TRACKERS ];
    UINT                        m_uNumActiveKNTrackers;

    // gesture detection based on DTWClassifier
    DTWClassifier               m_DTWTrackers[ NUM_ANIMATIONS ];
};


//--------------------------------------------------------------------------------------
#define JOINT_BIT( name )   (1 << NUI_SKELETON_POSITION_##name)
Sample::GestureAnimation    Sample::ms_animations[ NUM_ANIMATIONS ] =
{
    {
        "game:\\media\\bvh\\Arms_Flap.bvh", FALSE,
        L"Flap arms", 0,
        PASS_PSNR_3,
        // joints of interest
        JOINT_BIT( SHOULDER_LEFT )      |
        JOINT_BIT( ELBOW_LEFT )         |
        JOINT_BIT( WRIST_LEFT )         |
        JOINT_BIT( SHOULDER_RIGHT )     |
        JOINT_BIT( ELBOW_RIGHT )        |
        JOINT_BIT( WRIST_RIGHT )
    },

    // right side
    {
        "game:\\media\\bvh\\Right_Wave.bvh", FALSE,
        L"Right wave", 0,
        PASS_PSNR_3,
        // joints of interest
        JOINT_BIT( SHOULDER_RIGHT )     |
        JOINT_BIT( ELBOW_RIGHT )        |
        JOINT_BIT( WRIST_RIGHT )
    },
    {
        "game:\\media\\bvh\\Right_Wave_var1.bvh", FALSE,
        L"Right wave 1", 0,
        PASS_PSNR_2,
        // joints of interest
        JOINT_BIT( SHOULDER_RIGHT )     |
        JOINT_BIT( ELBOW_RIGHT )        |
        JOINT_BIT( WRIST_RIGHT )
    },
    {
        "game:\\media\\bvh\\Left_Arm_Raise.bvh", TRUE,
        L"Right arm", 0,
        PASS_PSNR_3,
        // joints of interest
        JOINT_BIT( SHOULDER_LEFT )      |
        JOINT_BIT( ELBOW_LEFT )         |
        JOINT_BIT( SHOULDER_RIGHT )     |
        JOINT_BIT( ELBOW_RIGHT )        |
        JOINT_BIT( WRIST_RIGHT )
    },

    // left side
    {
        "game:\\media\\bvh\\Right_Wave.bvh", TRUE,
        L"Left wave", 0,
        PASS_PSNR_2,
        // joints of interest
        JOINT_BIT( SHOULDER_LEFT )     |
        JOINT_BIT( ELBOW_LEFT )        |
        JOINT_BIT( WRIST_LEFT )
    },
    {
        "game:\\media\\bvh\\Right_Wave_var1.bvh", TRUE,
        L"Left wave 1", 0,
        PASS_PSNR_2,
        // joints of interest
        JOINT_BIT( SHOULDER_LEFT )     |
        JOINT_BIT( ELBOW_LEFT )        |
        JOINT_BIT( WRIST_LEFT )
    },
    {
        "game:\\media\\bvh\\Left_Arm_Raise.bvh", FALSE,
        L"Left arm", 0,
        PASS_PSNR_3,
        // joints of interest
        JOINT_BIT( SHOULDER_LEFT )      |
        JOINT_BIT( ELBOW_LEFT )         |
        JOINT_BIT( WRIST_LEFT )         |
        JOINT_BIT( SHOULDER_RIGHT )     |
        JOINT_BIT( ELBOW_RIGHT )
    }
};
#undef  JOINT_BIT



//----------------------------------------------------------------------------------
// Name: ProjectWorldDistanceToScreen()
// Desc: Given the size in world space returns the size in screen space
//----------------------------------------------------------------------------------
static inline
FLOAT   ProjectWorldDistanceToScreen( FLOAT fSize, FLOAT fDistMeters )
{
    return fSize * FOVH_PER_PIXEL_320x240_INV / fDistMeters;
}

//----------------------------------------------------------------------------------
// Name: ProjectWorldToScreen()
// Desc: Given a point in world space returns the point in screenspace
//----------------------------------------------------------------------------------
static inline
XMVECTOR    ProjectWorldToScreen( XMVECTOR vWorld )
{
    XMVECTOR s;

    s.x = 0.5f + vWorld.x * FOVH_PER_PIXEL_1x1_INV / vWorld.z;
    s.y = 0.5f - vWorld.y * FOVV_PER_PIXEL_1x1_INV / vWorld.z;
    s.z = vWorld.z;

    return s;
}

//----------------------------------------------------------------------------------
// Name: TransformScreenDepthToWorld()
// Desc: Given the depth in screen space returns the depth in world space
//----------------------------------------------------------------------------------
static inline
XMVECTOR    TransformScreenDepthToWorld( FLOAT fX, FLOAT fY, FLOAT fZ )
{
    static_assert( 320 == RESIZED_DEPTH_WIDTH, "Fix this for different size" );

    XMVECTOR    w;

    fZ /= 1000.f;

    w.x = (fX -  RESIZED_DEPTH_WIDTH * 0.5f) * fZ * FOVH_PER_PIXEL_320x240;
    w.y = (RESIZED_DEPTH_HEIGHT * 0.5f - fY) * fZ * FOVV_PER_PIXEL_320x240;
    w.z = fZ;

    return w;
}

//--------------------------------------------------------------------------------------
// Name: D3dColourForScore()
// Desc: Returns nice colours for different score for UI
//--------------------------------------------------------------------------------------
static inline
DWORD   D3dColourForScore( FLOAT fScore )
{
    if( fScore <= SCORE_FAIL )
        return 0xffff0000;
    if( fScore >= SCORE_PERFECT )
        return 0xff00ff00;

    const DWORD v = static_cast< DWORD >( 255.f * (fScore - SCORE_FAIL) / (SCORE_PERFECT - SCORE_FAIL) );

    return 0xff000000 | (v << 8) | ((255 - v) << 16);
}

//--------------------------------------------------------------------------------------
// Name: DescribeScore()
// Desc: Returns a string "describing" the given score
//--------------------------------------------------------------------------------------
static inline
const WCHAR*    DescribeScore( FLOAT score )
{
    if( score < SCORE_FAIL )
        return L"Fail";
    else if( score < SCORE_GOOD )
        return L"Good";
    else if( score < SCORE_VERY_GOOD )
        return L"Very good";
    else if( score < SCORE_PERFECT )
        return L"Excellent";
    return L"Perfect";
}

//--------------------------------------------------------------------------------------
// Name: D3dColorFromTrackingState()
// Desc: Returns UI colour for the NUI tracking state
//--------------------------------------------------------------------------------------
static inline
DWORD   D3dColorFromTrackingState( NUI_SKELETON_POSITION_TRACKING_STATE state )
{
    switch( state )
    {
    default:
    case NUI_SKELETON_POSITION_NOT_TRACKED: return 0xff0000ff; break;
    case NUI_SKELETON_POSITION_TRACKED:     return 0xff00ff00; break;
    case NUI_SKELETON_POSITION_INFERRED:    return 0xffffff00; break;
    }
}

//--------------------------------------------------------------------------------------
// Name: SubUintWrap()
// Desc: Subtracts s from v modulo mod assuming s < max
//--------------------------------------------------------------------------------------
static inline
UINT    SubUintWrap( UINT v, UINT s, UINT mod )
{
    return (v + mod - s) % mod;
}

//--------------------------------------------------------------------------------------
// Name: DecUintWrap()
// Desc: Decrements v modulo mod
//--------------------------------------------------------------------------------------
static inline
UINT    DecUintWrap( UINT v, UINT mod )
{
    return (v + mod - 1) % mod;
}

//--------------------------------------------------------------------------------------
// Name: IncUintWrap()
// Desc: Increments v modulo mod
//--------------------------------------------------------------------------------------
static inline
UINT    IncUintWrap( UINT v, UINT mod )
{
    return (v + 1) % mod;
}

//--------------------------------------------------------------------------------------
// Name: AddUintWrap()
// Desc: Increments v modulo mod
//--------------------------------------------------------------------------------------
static inline
UINT    AddUintWrap( UINT v, UINT a, UINT mod )
{
    return (v + a) % mod;
}

//--------------------------------------------------------------------------------------
// Name: SetVertex()
// Desc: Sets PositionColor vertex's fields
//--------------------------------------------------------------------------------------
static inline
VOID    SetVertex( ATG::MeshVertexPC& Vertex, FLOAT fX, FLOAT fY, DWORD dwColor )
{
    Vertex.Position.x = fX;
    Vertex.Position.y = fY;
    Vertex.Position.z = 0;
    Vertex.Color = dwColor;
}

//--------------------------------------------------------------------------------------
// Name: SetFilledRect()
// Desc: Adds a filled rectangle to be rendered with D3DPT_RECTLIST
//--------------------------------------------------------------------------------------
static inline
VOID    SetFilledRect( ATG::MeshVertexPC* pVertices, FLOAT fX, FLOAT fY, FLOAT fWidth, FLOAT fHeight, DWORD dwColor )
{
    SetVertex( pVertices[ 0 ], fX, fY, dwColor );
    SetVertex( pVertices[ 1 ], fX + fWidth, fY, dwColor );
    SetVertex( pVertices[ 2 ], fX + fWidth, fY + fHeight, dwColor );
}

//--------------------------------------------------------------------------------------
// Name: DrawOrientedBox()
// Desc: Sets up a transform matrix for a cube so it looks like an oriented box and
//       renders it using ATG debug draw
//--------------------------------------------------------------------------------------
static
VOID	DrawOrientedBox( XMVECTOR vOrigin, XMVECTOR vEnd, DWORD dwClr )
{
    const XMVECTOR  vOfs = vEnd - vOrigin;
    
    XMVECTOR    vUp = XMVectorSet( 1, 1, 1, 0 );
    if( vOfs.z != 0 )
        vUp.z = -(vOfs.x + vOfs.y)/vOfs.z;
    else if( vOfs.y != 0 )
        vUp.y = -(vOfs.x + vOfs.z)/vOfs.y;
    else if( vOfs.x != 0 )
        vUp.x = -(vOfs.z + vOfs.y)/vOfs.x;
    
    const XMVECTOR  vHalfOfs = vOfs * 0.5f;
    const FLOAT fLength = XMVector3Length( vHalfOfs ).x;
    const FLOAT fGirth = BOXMAN_LIMB_WIDTH;

    vUp = XMVector3Normalize( vUp );
    XMVECTOR    vAlong = XMVector3Normalize( vOfs );
    XMVECTOR    vRight = XMVector3Cross( vUp, vAlong );

    XMMATRIX    matOrientation;
    matOrientation.r[ 0 ] = XMVectorAndInt( vRight,   g_XMMask3 );
    matOrientation.r[ 1 ] = XMVectorAndInt( vUp,      g_XMMask3 );
    matOrientation.r[ 2 ] = XMVectorAndInt( vAlong,   g_XMMask3 );
    matOrientation.r[ 3 ] = XMVectorSet( 0, 0, 0, 1 );

    const XMMATRIX  matSize = XMMatrixScaling( fGirth, fGirth, fLength );
    const XMMATRIX  matTranslateToCentre = XMMatrixTranslationFromVector( vHalfOfs + vOrigin );

    ATG::DebugDraw::DrawCubeWireframe( matSize * matOrientation * matTranslateToCentre, dwClr );
}



//--------------------------------------------------------------------------------------
// Name: FullPathsMaskFromMask()
// Desc: we always need to calculate parents of the bones the caller is interested in
//       that means we have to make sure the mask contains full paths to the given joints
//--------------------------------------------------------------------------------------
static
DWORD   FullPathsMaskFromMask( DWORD dwMask )
{
    const DWORD MASK_ALL_BONES = (1 << NUI_SKELETON_POSITION_COUNT) - 1;

    if( MASK_ALL_BONES != (dwMask & MASK_ALL_BONES) )
    {
        DWORD   dwPathMask = 0;
        DWORD   dwTempMask = dwMask;

        for( UINT i=0; i < NUI_SKELETON_POSITION_COUNT; ++i, dwTempMask >>= 1 )
        {
            if( dwTempMask & 1 )
            {
                UINT    uParent = AnimationAdapter::ms_NuiJointParents[ i ];
                while( uParent < NUI_SKELETON_POSITION_COUNT  )
                {
                    dwPathMask |= (1 << uParent);
                    uParent = AnimationAdapter::ms_NuiJointParents[ uParent ];
                }
            }
        }

        dwMask |= dwPathMask;
    }

    return dwMask;
}


//-------------------------------------------------------------------------------------
// Name: CalculateMatchScore()
// Desc: A helper function to calculate the score from PSNR
//-------------------------------------------------------------------------------------
static inline
FLOAT   CalculateMatchScore( FLOAT fPSNR, FLOAT fBadPSNR )
{
    FLOAT   fMatchScore = (fPSNR - fBadPSNR) / (GOOD_PSNR - fBadPSNR);
    return min( 1, max( 0, fMatchScore ) );
}

//--------------------------------------------------------------------------------------
// Name: PSNRFromMSE()
// Desc: Calculate Peak Signal to Noise Ratio
//--------------------------------------------------------------------------------------
static inline
FLOAT   PSNRFromMSE( FLOAT fMse, FLOAT fMax )
{
    if( fMse > 0 )
    {
        const FLOAT fMaxByMse = fMax / fMse;
        return 10 * logf( fMaxByMse ) / logf( 10 );
    }
    
    return -100;    // arbitrarily low PSNR
}


//--------------------------------------------------------------------------------------
// Name: CheckJointInDepth()
// Desc: Consults the depth buffer in an attempt to resolve a non tracked joint
//--------------------------------------------------------------------------------------
static
BOOL    CheckJointInDepth(  XMVECTOR vJointPos,
                            const D3DLOCKED_RECT* pRcDepthTex,
                            const D3DLOCKED_RECT* pRcDebugTex,
                            FLOAT& fDeltaGuessed )
{
    assert( pRcDepthTex );

    fDeltaGuessed = 1;

    // depth buffer extents from the world space sphere
    const FLOAT     fJointSphereSize = ProjectWorldDistanceToScreen( JOINT_SPHERE_RADIUS, vJointPos.z );
    const XMVECTOR  vJointScreenPos = ProjectWorldToScreen( vJointPos );

    INT minX = static_cast< INT >( (vJointScreenPos.x * static_cast< FLOAT >( RESIZED_DEPTH_WIDTH ) - fJointSphereSize ) );
    INT maxX = static_cast< INT >( (vJointScreenPos.x * static_cast< FLOAT >( RESIZED_DEPTH_WIDTH ) + fJointSphereSize ) );
    INT minY = static_cast< INT >( (vJointScreenPos.y * static_cast< FLOAT >( RESIZED_DEPTH_HEIGHT ) - fJointSphereSize ) );
    INT maxY = static_cast< INT >( (vJointScreenPos.y * static_cast< FLOAT >( RESIZED_DEPTH_HEIGHT ) + fJointSphereSize ) );
    INT minZ = static_cast< INT >( (vJointPos.z - JOINT_SPHERE_RADIUS) * 1000.f );
    INT maxZ = static_cast< INT >( (vJointPos.z + JOINT_SPHERE_RADIUS) * 1000.f );

    minX = max( 0, min( static_cast< INT >( RESIZED_DEPTH_WIDTH ), minX ) );
    maxX = max( 0, min( static_cast< INT >( RESIZED_DEPTH_WIDTH ), maxX ) );
    minY = max( 0, min( static_cast< INT >( RESIZED_DEPTH_HEIGHT ), minY ) );
    maxY = max( 0, min( static_cast< INT >( RESIZED_DEPTH_HEIGHT ), maxY ) );

    UINT        uNumValidPixels = 0;
    UINT        uNumGoodPixels = 0;
    UINT        uNumOccludedPixels = 0;
    XMVECTOR    vAvgSurfacePos = XMVectorZero();
    for( INT i = minY; i < maxY; ++i )
    {
        const USHORT*   pRow = reinterpret_cast< const USHORT* >( reinterpret_cast< UINT_PTR >( pRcDepthTex->pBits ) + pRcDepthTex->Pitch * i);
        DWORD*    pDbgRow = pRcDebugTex ? reinterpret_cast< DWORD* >( reinterpret_cast< UINT_PTR >( pRcDebugTex->pBits ) + pRcDebugTex->Pitch * i) : NULL;
        for( INT j = minX; j < maxX; ++j )
        {
            const INT uDepth = pRow[ j ];

            if( pDbgRow )
                pDbgRow[ j ] = CLR_DEPTH_INVALID;

            // unusable
            if( 0 == uDepth )
                continue;

            // could be occluded or bad or good
            ++uNumValidPixels;

            if( pDbgRow )
                pDbgRow[ j ] = CLR_DEPTH_BAD;

            // totally bad, the search volume is nearer than the depth
            if( maxZ < uDepth )
                continue;

            // could be good or occluded
            if( uDepth < minZ )
            {
                if( pDbgRow )
                    pDbgRow[ j ] = CLR_DEPTH_OCCLUDED;
                ++uNumOccludedPixels;
            } else
            {
                if( pDbgRow )
                    pDbgRow[ j ] = CLR_DEPTH_GOOD;
                ++uNumGoodPixels;

                // accumulate surface position
                vAvgSurfacePos += TransformScreenDepthToWorld( static_cast< FLOAT >( j ), static_cast< FLOAT >( i ), static_cast< FLOAT >( uDepth ) );
            }
        }
    }

    // the position of the point is on the surface of the body, so step half the sphere size into it
    // to get to the "bone" position
    if( uNumGoodPixels )
    {
        vAvgSurfacePos /= static_cast< FLOAT >( uNumGoodPixels );
        //vAvgSurfacePos.z += JOINT_SPHERE_RADIUS;
        const FLOAT fDist = XMVector3Length( vAvgSurfacePos - vJointPos ).x;
        fDeltaGuessed = min( MAX_JOINT_ERROR, fDist );
        return TRUE;
    }

    // all good if all samples missed and bad if some were occluded
    return !uNumOccludedPixels;
}


//--------------------------------------------------------------------------------------
// Name: CalculateAngularDistance()
// Desc: Calculates error metrics for each joint, then calculates Peak Signal to Noise
//       Ratio for the whole skeleton. If ST is unsure about a joint, checks depth
//       buffer in attempt to establish whether the joint is where we expect it to be.
//       It's alright not to pass any depth to it, the joint is going to get ignored
//       if ST has low confidence and we don't have depth to check it out.
//       fScaleZ flattens the whole skeleton in screen plane so to optionally amplify
//       2D aspect of the gesture
//--------------------------------------------------------------------------------------
static
VOID CalculateAngularDistance(  PSNRRecord& psnrOut,
                                const XMVECTOR* pvRetargetedJointsOffsets,
                                const Skeleton* pSkeleton,
                                DWORD dwComputeMask,
                                DWORD dwRelevanceMask,
                                const XMVECTOR* pvRetargetedJointsPositions = NULL,
                                const D3DLOCKED_RECT* pRcDepth = NULL,
                                const D3DLOCKED_RECT* pRcDebug = NULL )
{
    assert( dwRelevanceMask == (dwComputeMask & dwRelevanceMask) );

    const FLOAT fScaleZ = 1;

    // note that NUI skeleton can be turned slightly sideways so depending on the game
    // we may need to align the two (or not). we choose not to do it here, so we require
    // the player to stand face on to the sensor
    
    // build hierarchical transform for retargeted joints and for nui skeleton
    // this, essentially, is the hierarchical difference in transform for each joint
    XMVECTOR    animJointsXForm[ NUI_SKELETON_POSITION_COUNT ];
    AnimationState::BuildHierarchicalTransform( animJointsXForm,
                                                fScaleZ,
                                                pSkeleton->m_skeletonOffsets,
                                                pvRetargetedJointsOffsets,
                                                dwComputeMask );

    // calculate angular difference
    for( UINT i=0; i < NUI_SKELETON_POSITION_COUNT; ++i )
    {
        const BOOL  bRelevantJoint = dwRelevanceMask & 1;

        psnrOut.m_jointDistanceFromRef[ i ] = MAX_JOINT_ERROR;
        psnrOut.m_jointReliable[ i ] = FALSE;
        psnrOut.m_jointPartOfCalculation[ i ] = bRelevantJoint;

        if( bRelevantJoint )
        {
            if( NUI_SKELETON_POSITION_TRACKED == pSkeleton->m_skeletonPositionTrackingState[ i ] )
            {
                psnrOut.m_jointReliable[ i ] = TRUE;
                psnrOut.m_jointDistanceFromRef[ i ] = MAX_JOINT_ERROR * (animJointsXForm[ i ].w / XM_PI);
            } else if( pRcDepth )
            {
                psnrOut.m_jointReliable[ i ] = CheckJointInDepth(   pvRetargetedJointsPositions[ i ],
                                                                    pRcDepth,
                                                                    pRcDebug,
                                                                    psnrOut.m_jointDistanceFromRef[ i ] );
            }
        }

        dwRelevanceMask >>= 1;
    }
}


//-------------------------------------------------------------------------------------
// Name: main()
// Desc: The application's entry point
//-------------------------------------------------------------------------------------
INT __cdecl main()
{
    DmMapDevkitDrive();

    Sample* pMyAtgApp = new Sample;
    ZeroMemory( &pMyAtgApp->m_d3dpp, sizeof( pMyAtgApp->m_d3dpp ) );

    pMyAtgApp->m_d3dpp.BackBufferWidth        = g_dwFrameWidth;
    pMyAtgApp->m_d3dpp.BackBufferHeight       = g_dwFrameHeight;
    pMyAtgApp->m_d3dpp.BackBufferCount        = 1;
    pMyAtgApp->m_d3dpp.MultiSampleType        = D3DMULTISAMPLE_NONE;
    pMyAtgApp->m_d3dpp.EnableAutoDepthStencil = TRUE;
    pMyAtgApp->m_d3dpp.DisableAutoBackBuffer  = FALSE;
    pMyAtgApp->m_d3dpp.DisableAutoFrontBuffer = FALSE;
    pMyAtgApp->m_d3dpp.SwapEffect             = D3DSWAPEFFECT_DISCARD;
    pMyAtgApp->m_d3dpp.PresentationInterval   = D3DPRESENT_INTERVAL_TWO;
    pMyAtgApp->m_d3dpp.AutoDepthStencilFormat = D3DFMT_D24FS8;
    pMyAtgApp->m_d3dpp.FrontBufferFormat      = static_cast< D3DFORMAT >( MAKESRGBFMT( D3DFMT_LE_A8R8G8B8 ) );
    pMyAtgApp->m_d3dpp.BackBufferFormat       = static_cast< D3DFORMAT >( MAKESRGBFMT( D3DFMT_A8R8G8B8 ) );
    pMyAtgApp->m_dwDeviceCreationFlags       |= D3DCREATE_CREATE_THREAD_ON_1;

    pMyAtgApp->Run();

    // Run() never returns, so it would be pointless to release the memory allocated for pMyAtgApp here.
    // When the sample is exited, it is through a reset and the system will reclaim the memory at this time.

}

//--------------------------------------------------------------------------------------
// Name: Initialize()
// Desc: Creates all graphics resources and initializes rendering and animation systems.
//--------------------------------------------------------------------------------------
HRESULT Sample::Initialize()
{
    m_bUseKNClassifier = TRUE;
    m_bWatchingSkeleton = NULL;
    m_bPausedAnimation = FALSE;
    m_uCurrentSkeletonIndex = 0;
    m_uCurFrame = 0;
    ZeroMemory( &m_frameFenceBuffer, sizeof( m_frameFenceBuffer ) );
    m_state = STATE_DEFAULT;
    m_fModelAngle = 0;
    m_uDetectedGesture = ~0ul;
    m_uFrameWhenGestureDetected = 0;
    m_bNewDetectedGesture = FALSE;

    m_uCurPSNRHistoryTail = 0;
    ZeroMemory( m_PSNRHistory, sizeof( m_PSNRHistory ) );
    ZeroMemory( m_ScoreHistory, sizeof( m_ScoreHistory ) );

    ResetGameClassifier();

    // create event which will be signaled when frame processing ends
    m_hFrameEndEvent = CreateEvent( NULL, FALSE, FALSE, "NuiFrameEndEvent" );
    if ( !m_hFrameEndEvent )
    {
        ATG_PrintError( "Failed to create NuiFrameEndEvent\n" );
        return E_FAIL;
    }

    HRESULT hr = NuiInitialize( NUI_INITIALIZE_FLAG_USES_SKELETON |
                                NUI_INITIALIZE_FLAG_USES_COLOR |
                                NUI_INITIALIZE_FLAG_USES_DEPTH_AND_PLAYER_INDEX,
                                NUI_INITIALIZE_DEFAULT_HARDWARE_THREAD );

    if( FAILED( hr ))
    {
        ATG::NuiPrintError( hr, "NuiInitialize" );
        return E_FAIL;
    }

    // register frame end event with NUI
    hr = NuiSetFrameEndEvent( m_hFrameEndEvent, 0 );
    if( FAILED(hr) )
    {
        ATG::NuiPrintError( hr, "NuiSetFrameEndEvent" );
        return E_FAIL;
    }

    // Open the color stream
	hr = NuiImageStreamOpen( NUI_IMAGE_TYPE_COLOR, NUI_IMAGE_RESOLUTION_640x480, 0, 1, NULL, &m_hImage );
    if( FAILED (hr) )
    {
        ATG::NuiPrintError( hr, "NuiImageStreamOpen" );
        return E_FAIL;
    }

    // Open the depth stream
	hr = NuiImageStreamOpen( NUI_IMAGE_TYPE_DEPTH_AND_PLAYER_INDEX, NUI_IMAGE_RESOLUTION_320x240, 0, 1, NULL, &m_hDepth );
    if( FAILED (hr) )
    {
        ATG::NuiPrintError( hr, "NuiImageStreamOpen" );
        return E_FAIL;
    }

    hr = NuiSkeletonTrackingEnable( NULL, 0 );
    if( FAILED( hr ))
    {
        ATG::NuiPrintError( hr, "NuiSkeletonTrackingEnable" );
    }

    // Initialize view parameters
    XVIDEO_MODE VideoMode;
    XGetVideoMode( &VideoMode );

    // Create the font
    if( FAILED( m_Font16.Create( "game:\\Media\\Fonts\\Arial_16.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Confine text drawing to the title safe area
    m_Font16.SetWindow( ATG::GetTitleSafeArea() );

    // Create the help
    if( FAILED( m_Help.Create( "d:\\Media\\Help\\Help.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    ATG::SimpleShaders::Initialize( NULL, NULL );

    // Initialize the Picture in Picture visualization
    if( FAILED( m_pip.Initialize( m_pd3dDevice, NUI_INITIALIZE_FLAG_USES_COLOR |
                                                NUI_INITIALIZE_FLAG_USES_SKELETON, 
		                                        NUI_IMAGE_RESOLUTION_640x480 ) ) )
    {
        ATG_PrintError ( "Picture in Picture initialization failed" );
    }

    // load our BVH animations we're going to try to play with
    for( UINT i=0; i < NUM_ANIMATIONS; ++i )
    {
        GestureAnimation&   anim = ms_animations[ i ];

        const BVHAnimation* pAnim = BVHAnimation::LoadBVH( anim.m_pFilename );
        if( pAnim )
        {
            if( 0 == anim.m_uEndFrame )
                anim.m_uEndFrame = pAnim->GetNumFrames() - 1;

            // allocate space for matrices
            if( !anim.m_adapter.SetAnimation( pAnim, anim.m_bMirror ) ||
                !anim.m_animToScore.SetAdapter( &anim.m_adapter )   ||
                !anim.m_animToShow.SetAdapter( &anim.m_adapter ) )
            {
                ATG_PrintError( "Error while setting up the anim" );
            }

            delete pAnim;
        }
        else
        {
            ATG_PrintError( "Couldn't load mocap animation" );
        }
    }

    // activate the first animation
    SetCurAnimation( ANIM_START );

    // reset KN matching
    m_uNumActiveKNTrackers = 0;
    for( UINT i=0; i < NUM_KN_TRACKERS; ++i )
    {
        m_KNTrackers[ i ].m_classifier.Reset();
        m_KNTrackers[ i ].m_uAnim = 0;
        m_KNTrackers[ i ].m_uCurFrame = 0;
    }

    // assign animations to DTW classifiers
    for( UINT i=0; i < NUM_ANIMATIONS; ++i )
    {
        m_DTWTrackers[ i ].SetAnimation( ms_animations[ i ].m_animToScore,
                                         ms_animations[ i ].m_dwJointsMask,
                                         ms_animations[ i ].m_fBadPSNR );
    }

    if( FAILED( m_pd3dDevice->CreateTexture( RESIZED_DEPTH_WIDTH, RESIZED_DEPTH_HEIGHT, 1, 0, D3DFMT_LIN_L16, 0, &m_depthTexture, NULL ) ) )
    {
        ATG_PrintError( "Can't create depth texture" );
    }

    if( FAILED( m_pd3dDevice->CreateTexture( RESIZED_DEPTH_WIDTH, RESIZED_DEPTH_HEIGHT, 1, D3DUSAGE_CPU_CACHED_MEMORY /* want to use XMemSet128 */,
                                                D3DFMT_LIN_A8B8G8R8, 0, &m_debugTexture, NULL ) ) )
    {
        ATG_PrintError( "Can't create debug texture" );
    }

    return S_OK;
}

//--------------------------------------------------------------------------------------
// Name: ResetGameClassifier()
// Desc: DTW classifier doesn't need resetting
//--------------------------------------------------------------------------------------
VOID    Sample::ResetGameClassifier()
{
    m_classifierKN.Reset();
}

//--------------------------------------------------------------------------------------
// Name: GetGameClassifierScore()
// Desc: Returns the score of the current classifier
//--------------------------------------------------------------------------------------
FLOAT   Sample::GetGameClassifierScore() const
{
    return m_bUseKNClassifier ? m_classifierKN.GetScore() : m_classifierDTW.GetScore();
}

//--------------------------------------------------------------------------------------
// Name: GetGameClassifierPSNR()
// Desc: Returns the PSNR of the current classifier
//--------------------------------------------------------------------------------------
FLOAT   Sample::GetGameClassifierPSNR() const
{
    return m_bUseKNClassifier ? m_classifierKN.GetPSNR() : m_classifierDTW.GetPSNR();
}

//--------------------------------------------------------------------------------------
// Name: SetBonesLengthsFromAnimation()
// Desc: Load bone lengths from the animation into the current lengths array
//--------------------------------------------------------------------------------------
VOID    Sample::SetBonesLengthsFromAnimation( UINT uAnimIndex )
{
    assert( uAnimIndex < NUM_ANIMATIONS );

    for( UINT i=0; i < NUI_BONE_COUNT; ++i )
        m_skeletonBonesLengths[ i ] = ms_animations[ uAnimIndex ].m_adapter.GetDefaultBoneLength( i );
}

//--------------------------------------------------------------------------------------
// Name: SetBonesLengthsFromNUI()
// Desc: Load bone lengths from the tracked skeleton into the current lengths array
//--------------------------------------------------------------------------------------
VOID    Sample::SetBonesLengthsFromNUI()
{
    assert( m_bWatchingSkeleton );

    for( UINT i=0; i < NUI_BONE_COUNT; ++i )
    {
        const NUI_SKELETON_POSITION_INDEX endJointName = AnimationAdapter::ms_NuiBonesEndJoints[ i ];
        const NUI_SKELETON_POSITION_INDEX startJointName = AnimationAdapter::ms_NuiJointParents[ endJointName ];

        if( NUI_SKELETON_POSITION_TRACKED == m_nuiSkeletonData.eSkeletonPositionTrackingState[ endJointName ]  &&
            NUI_SKELETON_POSITION_TRACKED == m_nuiSkeletonData.eSkeletonPositionTrackingState[ startJointName ] )
        {
            m_skeletonBonesLengths[ i ] = XMVector3Length( m_nuiSkeletonData.SkeletonPositions[ endJointName ] -
                                                               m_nuiSkeletonData.SkeletonPositions[ startJointName ] ).x;
        }
    }
}

//--------------------------------------------------------------------------------------
// Name: CapturePlayersDepth()
// Desc: Cutout player's depth map using segmentation mask to only copy player's pixels
//--------------------------------------------------------------------------------------
BOOL Sample::CapturePlayersDepth( IDirect3DTexture9* pDepthTexture, const UINT uSkeletonIndex )
{
    static_assert( 320 == RESIZED_DEPTH_WIDTH, "Fix this for different size" );

    D3DLOCKED_RECT  srcRc, dstRc;
    m_depthTexture->LockRect( 0, &dstRc, NULL, 0 );
    pDepthTexture->LockRect( 0, &srcRc, NULL, 0 );

    for( UINT i=0; i < RESIZED_DEPTH_HEIGHT; ++i )
    {
        const USHORT* pSrc = reinterpret_cast< USHORT* >( reinterpret_cast< uintptr_t >( srcRc.pBits ) + i * srcRc.Pitch );
        USHORT* pDst = reinterpret_cast< USHORT* >( reinterpret_cast< uintptr_t >( dstRc.pBits ) + i * dstRc.Pitch );

        for( UINT j=0; j < RESIZED_DEPTH_WIDTH; ++j )
        {
            const UINT  uDepth = pSrc[ j ];
            pDst[ j ] = static_cast< USHORT >( ( uSkeletonIndex == (uDepth & NUI_IMAGE_PLAYER_INDEX_MASK) ) ? (uDepth >> NUI_IMAGE_PLAYER_INDEX_SHIFT) : 0 );
        }
    }

    m_depthTexture->UnlockRect( 0 );
    pDepthTexture->UnlockRect( 0 );

    return FALSE;
}

//--------------------------------------------------------------------------------------
// Name: UpdateSkeletonTracking()
// Desc: Read the data from the camera stream synchronously each frame and pass the
//       depthmap on to the skeleton tracking. 
//       The Nui API will wait up to NUI_CAMERA_TIMEOUT_DEFAULT ms for a new skeleton.
//--------------------------------------------------------------------------------------
HRESULT Sample::UpdateSkeletonTracking()
{
    PIX_SCOPED_EVENT( __FUNCTION__ );

    m_bWatchingSkeleton = FALSE;

    // wait for frame processing to end
    if( WAIT_OBJECT_0 != WaitForSingleObject( m_hFrameEndEvent, NUI_FRAME_END_TIMEOUT_DEFAULT ) )
    {
        PIXSetMarker( 0, "No NUI frame" );
        m_hrNuiResult = E_PENDING;
        return E_FAIL;
    }

    // Get data from the next image, depth and skeleton frames
    HRESULT hrImage = NuiImageStreamGetNextFrame( m_hImage, 0, &m_pImageFrame );
    HRESULT hrDepth = NuiImageStreamGetNextFrame( m_hDepth, 0, &m_pDepthFrame );
    m_hrNuiResult = NuiSkeletonGetNextFrame( 0, &m_SkeletonFrame );

    if ( SUCCEEDED( hrImage ) )
    {
        PIXSetMarker( 0, "Got colour frame %d, %ld", m_pImageFrame->dwFrameNumber, m_pImageFrame->liTimeStamp );
		m_pip.SetColorTexture( m_pImageFrame->pFrameTexture );
		NuiImageStreamReleaseFrame( m_hImage, m_pImageFrame );
    }
	
    if ( SUCCEEDED( hrDepth ) )
    {
        PIXSetMarker( 0, "Got depth frame %d, %ld", m_pDepthFrame->dwFrameNumber, m_pDepthFrame->liTimeStamp );
        CapturePlayersDepth( m_pDepthFrame->pFrameTexture, m_uCurrentSkeletonIndex + 1 );
		NuiImageStreamReleaseFrame( m_hDepth, m_pDepthFrame );
    }

    if ( m_hrNuiResult == E_PENDING || FAILED ( m_hrNuiResult ) )
    {
        PIXSetMarker( 0, "No NUI frame" );
        return m_hrNuiResult;
    } else
    {
        PIXSetMarker( 0, "Got skeleton frame %d, %ld", m_SkeletonFrame.dwFrameNumber, m_SkeletonFrame.liTimeStamp );

        // If the last tracking ID no longer exists in the skeleton data, then switch to
        // the first tracked skeleton we can find. If none is tracked, then leave the current 
        // tracking ID as is and try again next frame.
        for ( int i = NUI_SKELETON_COUNT - 1; i >= 0 ; --i )
        {
            if ( m_SkeletonFrame.SkeletonData[ i ].eTrackingState == NUI_SKELETON_TRACKED )
            {
                m_uCurrentSkeletonIndex = i;

                if ( m_uCurrentTrackingID == m_SkeletonFrame.SkeletonData[i].dwTrackingID )
                {
                    break;
                }
            }
        }

        m_uCurrentTrackingID = m_SkeletonFrame.SkeletonData[ m_uCurrentSkeletonIndex ].dwTrackingID;

        m_pip.SetSkeletons( &m_SkeletonFrame );

        if( NUI_SKELETON_TRACKED == m_SkeletonFrame.SkeletonData[ m_uCurrentSkeletonIndex ].eTrackingState )
        {
            m_bWatchingSkeleton = TRUE;
            XMemCpy( &m_nuiSkeletonData, &m_SkeletonFrame.SkeletonData[ m_uCurrentSkeletonIndex ], sizeof( NUI_SKELETON_DATA ) );

            // calculate our offset representation for quicker scoring
            XMemCpy( &m_skeletonData.m_skeletonPositionTrackingState[ 0 ], &m_nuiSkeletonData.eSkeletonPositionTrackingState[ 0 ],
                                sizeof( m_nuiSkeletonData.eSkeletonPositionTrackingState ) );
            for( UINT i=0; i < NUI_SKELETON_POSITION_COUNT; ++i )
            {
                XMVECTOR    vOfs = XMVectorZero();

                const UINT  uParentIndex = AnimationAdapter::ms_NuiJointParents[ i ];

                if( uParentIndex != NUI_SKELETON_POSITION_COUNT )
                {
                    vOfs = XMVector3Normalize( m_nuiSkeletonData.SkeletonPositions[ i ] -
                                    m_nuiSkeletonData.SkeletonPositions[ uParentIndex ] );
                }

                m_skeletonData.m_skeletonOffsets[ i ] = vOfs;
            }
        }
    }

    // need to choose some position if we're not tracking a skeleton at the moment
    const XMVECTOR    vSkeletonOrigin = m_bWatchingSkeleton ?
                                            ms_animations[ m_uCurShowAnimation ].m_animToShow.GetJoints()[ NUI_SKELETON_POSITION_HIP_CENTER ] :
                                            DEFAULT_SKELETON_POS;

    // matrix used for debug output
    XMMATRIX    matNuiToDepthProj;
    ZeroMemory( &matNuiToDepthProj, sizeof( matNuiToDepthProj ) );
    matNuiToDepthProj._11 = NUI_CAMERA_SKELETON_TO_DEPTH_IMAGE_MULTIPLIER_320x240 / 160.f;
    matNuiToDepthProj._22 = NUI_CAMERA_SKELETON_TO_DEPTH_IMAGE_MULTIPLIER_320x240 / 120.f;
    matNuiToDepthProj._33 = 1 / 5.f;
    matNuiToDepthProj._34 = 1;
    matNuiToDepthProj._43 = 0.8f * matNuiToDepthProj._33;

    // rotate on the spot (for debugging)
    m_matNuiToDepth =   XMMatrixTranslationFromVector( -vSkeletonOrigin ) *
                        XMMatrixRotationY( m_fModelAngle ) *
                        XMMatrixTranslationFromVector( vSkeletonOrigin ) *
                        matNuiToDepthProj;

    return S_OK;
}

//--------------------------------------------------------------------------------------
// Name: SetCurAnimation()
// Desc: Changes current animation index
//--------------------------------------------------------------------------------------
VOID    Sample::SetCurAnimation( UINT uSeq )
{
    assert( uSeq < NUM_ANIMATIONS );

    m_uCurShowAnimation = uSeq;
}

//--------------------------------------------------------------------------------------
// Name: UpdateGameStateMachine()
// Desc: State machine transitions and state updates are here
//--------------------------------------------------------------------------------------
VOID    Sample::UpdateGameStateMachine( const ATG::GAMEPAD* pGamepad, FLOAT fDeltaTime )
{
    PIX_SCOPED_EVENT( __FUNCTION__ );

    GestureAnimation& curAnim = ms_animations[ m_uCurShowAnimation ];

    // controls
    const BOOL    bPressedA = pGamepad->wPressedButtons & XINPUT_GAMEPAD_A;
    const BOOL    bPressedB = pGamepad->wPressedButtons & XINPUT_GAMEPAD_B;
    const BOOL    bPressedDown = pGamepad->wPressedButtons & XINPUT_GAMEPAD_DPAD_DOWN;
    const BOOL    bPressedX = pGamepad->wPressedButtons & XINPUT_GAMEPAD_X;

    // switch classifiers immediately when asked
    if( bPressedX )
        m_bUseKNClassifier = !m_bUseKNClassifier;

    // continuously look for gestures
    if( m_bUseKNClassifier )
        DetectGestureKN();
    else
        DetectGestureDTW();

    // can process gestures here
    if( m_bNewDetectedGesture )
    {
        // do something with detected gestures

        m_bNewDetectedGesture = FALSE;
    }

    switch( m_state )
    {
    case STATE_DEFAULT:
        {
            // this is the starting state of the demo

            // start a "game"?
            if( bPressedA )
            {
                m_fStateTimerCountdown = TIMER_COUNTDOWN;
                m_state = STATE_COUNTDOWN_BEFORE_SHOW;
                ResetGameClassifier();
                break;
            }

            // show tracking information?
            if( bPressedDown )
            {
                m_state = STATE_DETECT_GESTURE;
                m_uNumActiveKNTrackers = 0;
                ResetGameClassifier();
                break;
            }

            // loop only the interesting bit of the animation
            if( curAnim.m_animToShow.GetAnimationFrame() >= curAnim.m_uEndFrame )
            {
                curAnim.m_animToShow.SetAnimationFrame( 0 );
                m_bNextFenceResetsTracking = TRUE;
            }

            // score gesture continuously
            UpdateVisibleAnimation( fDeltaTime, TRUE );
            UpdateAnimationScoring( fDeltaTime );
        }
        break;

    case STATE_COUNTDOWN_BEFORE_SHOW:
    case STATE_COUNTDOWN_BEFORE_TRY:
        {
            // these states display the countdown on the screen

            // allow the user to cancel them
            if( bPressedB )
            {
                m_state = STATE_DEFAULT;
                ResetGameClassifier();
                break;
            }

            m_fStateTimerCountdown -= fDeltaTime;

            if( m_fStateTimerCountdown <= 0  ||
                bPressedA )
            {
                m_state = (STATE_COUNTDOWN_BEFORE_SHOW == m_state) ? STATE_SHOW : STATE_TRY;
                m_fStateTimerCountdown = 0;

                // set the Animation to play
                curAnim.m_animToShow.SetAnimationFrame( 0 );
                ResetGameClassifier();
                break;
            }
        }
        break;

    case STATE_SHOW:
    case STATE_TRY:
        {
            // these states run the "game" and record score

            // allow the user to cancel
            if( bPressedB )
            {
                m_state = STATE_DEFAULT;
                ResetGameClassifier();
                m_fScore = 0;
                break;
            }

            UpdateVisibleAnimation( fDeltaTime, FALSE );
            UpdateAnimationScoring( fDeltaTime );

            if( STATE_TRY == m_state )
                m_fScore = GetGameClassifierScore();

            // finished playing the Animation?
            if( curAnim.m_animToShow.GetAnimationFrame() >= curAnim.m_uEndFrame  ||
                bPressedA )
            {
                if( STATE_TRY == m_state )
                {
                    m_state = STATE_SHOW_RESULTS;
                    m_fScore = bPressedA ? 0 : GetGameClassifierScore();
                    m_fStateTimerCountdown = TIMER_COUNTDOWN_RESULTS;
                } else
                {
                    m_state = STATE_COUNTDOWN_BEFORE_TRY;
                    m_fStateTimerCountdown = TIMER_COUNTDOWN;
                }
                ResetGameClassifier();
                break;
            }
        }
        break;

    case STATE_SHOW_RESULTS:
        {
            // this state just shows the "game" results with a countdown

            m_fStateTimerCountdown  -= fDeltaTime;

            // disallow cancelling immediately after started,
            // after that allow cancelling or quit on timeout
            if( (m_fStateTimerCountdown <= 0 &&
                (bPressedB | bPressedA))    ||
                m_fStateTimerCountdown <= -TIMER_COUNTDOWN_DISMISS_RESULTS )
            {
                m_state = STATE_DEFAULT;
                m_fScore = 0;
                ResetGameClassifier();
                break;
            }
        }
        break;

    case STATE_DETECT_GESTURE:
        {
            // this really has no "game" state processing except for cancelling checks
            if( bPressedB )
            {
                m_state = STATE_DEFAULT;
                m_fScore = 0;
                break;
            }
        }
        break;
    }
}

//--------------------------------------------------------------------------------------
// Name: Update()
// Desc: Called once per frame.  Entry point for animating the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Update()
{
    PIX_SCOPED_EVENT( __FUNCTION__ );

    // Get the skeleton
    UpdateSkeletonTracking();

    // Get elapsed time
    const FLOAT fDeltaTime = static_cast< FLOAT >( m_Timer.GetElapsedTime() );

    // Get current gamepad state
    const ATG::GAMEPAD* pGamepad = ATG::Input::GetMergedInput();

    // The Back button toggles the help screen.
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_BACK )
        m_bDrawHelp = !m_bDrawHelp;

    // Y toggles animation pause
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_Y )
        m_bPausedAnimation = !m_bPausedAnimation;

    // debug rotate
    if( pGamepad->bLeftTrigger )
        m_fModelAngle -= 0.01f;
    if( pGamepad->bRightTrigger )
        m_fModelAngle += 0.01f;
    if( pGamepad->bRightTrigger & pGamepad->bLeftTrigger )
        m_fModelAngle = 0;

    // switch the animations
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_DPAD_LEFT )
        SetCurAnimation( DecUintWrap( m_uCurShowAnimation, NUM_ANIMATIONS ) );
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_DPAD_RIGHT )
        SetCurAnimation( IncUintWrap( m_uCurShowAnimation, NUM_ANIMATIONS ) );

    // handle state transitions and updates
    UpdateGameStateMachine( pGamepad, fDeltaTime );
  
    return S_OK;
}

//--------------------------------------------------------------------------------------
// Name: RenderHistoryGraph()
// Desc: Renders the score history graph with its legend
//--------------------------------------------------------------------------------------
VOID    Sample::RenderHistoryGraph() const
{
    const D3DRECT rcSafeArea = ATG::GetTitleSafeArea();

    ATG::MeshVertexPC Vertices[ 2 * ARRAYSIZE( m_PSNRHistory ) ];

    // each state has it's own PSNR which is considered bad
    const FLOAT fBadPSNR = ms_animations[ m_uCurShowAnimation ].m_fBadPSNR;

    for( UINT i=0; i < ARRAYSIZE( m_PSNRHistory ); ++i )
    {
        const UINT idx = AddUintWrap( i, m_uCurPSNRHistoryTail, ARRAYSIZE( m_PSNRHistory ) );

        const FLOAT fPSNR = m_PSNRHistory[ idx ];
        const FLOAT fShowPSNR = min( 1, max( 0, fPSNR - fBadPSNR ) / (GOOD_PSNR - fBadPSNR) );
        
        SetVertex( Vertices[ i ],
                    HISTORY_START_X + i + rcSafeArea.x1, HISTORY_END_Y - HISTORY_HEIGHT * fShowPSNR + rcSafeArea.y1, fPSNR > GOOD_PSNR ? CLR_GOOD_PSNR : CLR_PASS_PSNR );
        SetVertex( Vertices[ i + ARRAYSIZE( m_PSNRHistory ) ],
                    HISTORY_START_X + i + rcSafeArea.x1, HISTORY_END_Y - HISTORY_HEIGHT * m_ScoreHistory[ idx ] + rcSafeArea.y1, CLR_SCORE );
    }

    ATG::SimpleShaders::SetDeclPosColor();
    ATG::SimpleShaders::BeginShader_PreTransformed_VertexColor();
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_VIEWPORTENABLE, FALSE );
    m_pd3dDevice->DrawPrimitiveUP( D3DPT_LINESTRIP, ARRAYSIZE( m_PSNRHistory ) - 1, Vertices, sizeof( ATG::MeshVertexPC ) );
    m_pd3dDevice->DrawPrimitiveUP( D3DPT_LINESTRIP, ARRAYSIZE( m_PSNRHistory ) - 1, &Vertices[ ARRAYSIZE( m_PSNRHistory ) ], sizeof( ATG::MeshVertexPC ) );

    ATG::SimpleShaders::EndShader();

	m_pd3dDevice->SetRenderState( D3DRS_VIEWPORTENABLE, TRUE );
	m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, TRUE );

    // render the legend
    m_Font16.Begin();
    m_Font16.SetScaleFactors( 0.8f, 0.8f );
    m_Font16.DrawText( HISTORY_START_X - 5, HISTORY_END_Y - HISTORY_HEIGHT +  0, CLR_WHITE, L"HISTORY", ATGFONT_RIGHT );
    m_Font16.DrawText( HISTORY_START_X - 5, HISTORY_END_Y - HISTORY_HEIGHT + 18, CLR_SCORE, L"Score", ATGFONT_RIGHT );
    m_Font16.DrawText( HISTORY_START_X - 5, HISTORY_END_Y - HISTORY_HEIGHT + 36, CLR_PASS_PSNR, L"PSNR", ATGFONT_RIGHT );
    m_Font16.End();

    // show dtw matrix
    if( !m_bUseKNClassifier )
    {
        m_classifierDTW.RenderMatrix( m_pd3dDevice, DTW_DEBUG_X, DTW_DEBUG_Y, DTW_DEBUG_W, DTW_DEBUG_H );

        // legend
        m_Font16.Begin();
        m_Font16.SetScaleFactors( 0.6f, 0.6f );
        m_Font16.DrawText( DTW_DEBUG_X, DTW_DEBUG_Y - 12, CLR_WHITE, L"Oldest NUI frame", ATGFONT_LEFT );
        m_Font16.DrawText( DTW_DEBUG_X + DTW_DEBUG_W, DTW_DEBUG_Y - 12, CLR_WHITE, L"Youngest NUI frame", ATGFONT_RIGHT );
        m_Font16.DrawText( DTW_DEBUG_X, DTW_DEBUG_Y, CLR_WHITE, L"First anim frame", ATGFONT_RIGHT );
        m_Font16.DrawText( DTW_DEBUG_X, DTW_DEBUG_Y + DTW_DEBUG_H - 12, CLR_WHITE, L"Last anim frame", ATGFONT_RIGHT );
        m_Font16.End();
    }
}

//--------------------------------------------------------------------------------------
// Name: RenderAnimationIcon()
// Desc: Renders animation boxman  into a given viewport
//--------------------------------------------------------------------------------------
VOID    Sample::RenderAnimationIcon( FLOAT fX, FLOAT fY, FLOAT fWidth, FLOAT fHeight,
                                   UINT uAnimIndex, UINT uFrame, DWORD dwColor ) const
{
    PIX_SCOPED_EVENT( __FUNCTION__ );

    ScopedViewportChange    pushViewport( m_pd3dDevice, fX, fY, fWidth, fHeight );

    ATG::DebugDraw::SetViewProjection( m_matNuiToDepth );

    ms_animations[ uAnimIndex ].m_animToScore.SetAnimationFrame( uFrame % ms_animations[ uAnimIndex ].m_uEndFrame );
    ms_animations[ uAnimIndex ].m_animToScore.CalculateJoints( m_skeletonBonesLengths, DEFAULT_SKELETON_POS );
    RenderBoxman( ms_animations[ uAnimIndex ].m_animToScore.GetJoints(), dwColor );
}


//--------------------------------------------------------------------------------------
// Name: GetTrackerKNInfo()
// Desc: Scans through active trackers to find their number and the best matching one for
//       all animations
//--------------------------------------------------------------------------------------
VOID    Sample::GetTrackerKNInfo( UINT* pNumTrackers, UINT* pBestTracker ) const
{
    const UINT  uClearSize = sizeof( UINT ) * NUM_ANIMATIONS;

    XMemSet( pNumTrackers, 0, uClearSize );
    XMemSet( pBestTracker, 0, uClearSize );

    UINT    furthestFrame[ NUM_ANIMATIONS ] = { 0 };

	for( UINT i=0; i < m_uNumActiveKNTrackers; ++i )
	{
        const UINT uAnimIdx = m_KNTrackers[ i ].m_uAnim;

        if( furthestFrame[ uAnimIdx ] < m_KNTrackers[ i ].m_uCurFrame )
        {
            furthestFrame[ uAnimIdx ] = m_KNTrackers[ i ].m_uCurFrame;
            pBestTracker[ uAnimIdx ] = i;
        }

        ++pNumTrackers[ uAnimIdx ];
    }
}

//--------------------------------------------------------------------------------------
// Name: RenderDetectionUI()
// Desc: Renders all animations along the top edge so we can see all of them at the same
//       time with their current states as well
//--------------------------------------------------------------------------------------
VOID    Sample::RenderDetectionUI() const
{
    const FLOAT fScreenWidth = static_cast< FLOAT >( ATG::GetTitleSafeArea().x2 - ATG::GetTitleSafeArea().x1 );
    const FLOAT fHeight = ICON_SIZE;
    const FLOAT fStepWidth = fScreenWidth / static_cast< FLOAT >( NUM_ANIMATIONS );
    const FLOAT fWidth = 3 * fHeight / 2;
    const FLOAT fOfs = (fStepWidth - fWidth) / 2;

    m_Font16.Begin();
    m_Font16.SetScaleFactors( 0.8f, 0.8f );

    if( m_bUseKNClassifier )
    {
        UINT    numTrackers[ NUM_ANIMATIONS ];
        UINT    bestTracker[ NUM_ANIMATIONS ];
        GetTrackerKNInfo( numTrackers, bestTracker );

        for( UINT i=0; i < NUM_ANIMATIONS; ++i )
        {
            const FLOAT fDivider = static_cast< FLOAT >( ms_animations[ i ].m_uEndFrame );
            const FLOAT fBarHeight = static_cast< FLOAT >( m_KNTrackers[ bestTracker[ i ] ].m_uCurFrame ) / fDivider;

            WCHAR tmp[ 128 ];
            swprintf_s( tmp,
                        L"%s\n%2.0f%% %d %.0f",
                        ms_animations[ i ].m_pDisplayName,
                        fBarHeight * 100.f,
                        numTrackers[ i ],
                        m_KNTrackers[ bestTracker[ i ] ].m_classifier.GetScore() * 100 );

            m_Font16.DrawText( fStepWidth * i, HISTORY_END_Y - 30, numTrackers[ i ] ? CLR_WHITE : CLR_PASS_PSNR, tmp, 0 );
        }

        for( UINT i=0; i < NUM_ANIMATIONS; ++i )
        {
            RenderAnimationIcon(  fStepWidth * i + fOfs,
                                HISTORY_END_Y,
                                fWidth,
                                fHeight,
                                i,
                                numTrackers[ i ] ? m_KNTrackers[ bestTracker[ i ] ].m_uCurFrame : 0,
                                numTrackers[ i ] ? CLR_WHITE : CLR_PASS_PSNR );
        }
    } else
    {
        for( UINT i=0; i < NUM_ANIMATIONS; ++i )
        {
            const FLOAT fBarHeight = static_cast< FLOAT >( m_DTWTrackers[ i ].GetScore() );

            WCHAR tmp[ 128 ];
            swprintf_s( tmp, L"%s %.2f", ms_animations[ i ].m_pDisplayName, fBarHeight );
            m_Font16.DrawText( fStepWidth * i, HISTORY_END_Y - 30, CLR_WHITE, tmp, 0 );
        }

        for( UINT i=0; i < NUM_ANIMATIONS; ++i )
            m_DTWTrackers[ i ].RenderMatrix( m_pd3dDevice, fStepWidth * i + fOfs, HISTORY_END_Y, 128, 128 );
    }

    m_Font16.End();
}


//--------------------------------------------------------------------------------------
// Name: RenderNuiStickman()
// Desc: Renders a nui given NUI skeleton as a stickman with confidence levels
//--------------------------------------------------------------------------------------
VOID Sample::RenderNuiStickman( XMMATRIX matViewProj, const NUI_SKELETON_DATA* pSkeleton ) const
{
    PIX_SCOPED_EVENT( __FUNCTION__ );

    ATG::SimpleShaders::SetDeclPosColor();
    ATG::SimpleShaders::BeginShader_Transformed_VertexColor( matViewProj );

    for( UINT i=0; i < NUI_BONE_COUNT; ++i )
    {
        const NUI_SKELETON_POSITION_INDEX endJointName = AnimationAdapter::ms_NuiBonesEndJoints[ i ];
        const NUI_SKELETON_POSITION_INDEX startJointName = AnimationAdapter::ms_NuiJointParents[ endJointName ];

        ATG::MeshVertexPC verts[ 2 ];
        verts[ 0 ].Position = reinterpret_cast< const XMFLOAT3& >( pSkeleton->SkeletonPositions[ startJointName ] );
        verts[ 0 ].Color = D3dColorFromTrackingState( pSkeleton->eSkeletonPositionTrackingState[ startJointName ] );
        verts[ 1 ].Position = reinterpret_cast< const XMFLOAT3& >( pSkeleton->SkeletonPositions[ endJointName ] );
        verts[ 1 ].Color = D3dColorFromTrackingState( pSkeleton->eSkeletonPositionTrackingState[ endJointName ] );

        m_pd3dDevice->DrawPrimitiveUP( D3DPT_LINELIST, 1, verts, sizeof( ATG::MeshVertexPC ) );
    }

    ATG::SimpleShaders::EndShader();
}

//--------------------------------------------------------------------------------------
// Name: RenderBoxman()
// Desc: Renders a skeleton using oriented boxes
//--------------------------------------------------------------------------------------
VOID Sample::RenderBoxman( const XMVECTOR* pJoints, DWORD clr ) const
{
    PIX_SCOPED_EVENT( __FUNCTION__ );

    // retargeted skeleton we are trying to aim at
    for( UINT i=0; i < NUI_BONE_COUNT; ++i )
    {
        // get two endpoints of the bone
        const NUI_SKELETON_POSITION_INDEX endJointName = AnimationAdapter::ms_NuiBonesEndJoints[ i ];
        const NUI_SKELETON_POSITION_INDEX startJointName = AnimationAdapter::ms_NuiJointParents[ endJointName ];

        const XMVECTOR&     vStart = pJoints[ startJointName ];
        const XMVECTOR&     vEnd = pJoints[ endJointName ];

		DrawOrientedBox( vStart, vEnd, clr );
    }
}

//--------------------------------------------------------------------------------------
// Name: RenderLabels()
// Desc: Render attached labels on each tracked joint
//       there is no problem with DTW classifier giving per-joint information, it's just
//       not implemented for simplicity
//--------------------------------------------------------------------------------------
VOID Sample::RenderLabels( XMMATRIX matViewProj, const GestureAnimation& anim ) const
{
    PIX_SCOPED_EVENT( __FUNCTION__ );

    D3DVIEWPORT9    vp;
    m_pd3dDevice->GetViewport( &vp );

    // set the right font window for this
    D3DRECT    fullScreen;
    fullScreen.x1 = WINDOW_START_X;
    fullScreen.y1 = WINDOW_START_Y;
    fullScreen.x2 = WINDOW_START_X + WINDOW_WIDTH;
    fullScreen.y2 = WINDOW_START_Y + WINDOW_HEIGHT;

    DWORD   dwRelevanceMask = ms_animations[ m_uCurShowAnimation ].m_dwJointsMask;

    m_Font16.SetWindow( fullScreen );
    m_Font16.Begin();
    m_Font16.SetScaleFactors( 0.75f, 0.75f );

    for( UINT i=0; i < NUI_SKELETON_POSITION_COUNT; ++i )
    {
        if( dwRelevanceMask & 1 )
        {
            WCHAR tmp[ 128 ];
            swprintf_s( tmp, L"%.3f", m_classifierKN.GetJointScore( i ) );

            const XMVECTOR pos = XMVector3TransformCoord( anim.m_animToShow.GetJoints()[ i ], matViewProj );
            const FLOAT    x = vp.X + vp.Width * (0.5f * pos.x - 0.5f);
            const FLOAT    y = vp.Y + vp.Height * (0.5f - 0.5f * pos.y);

            if( m_bUseKNClassifier )
                m_Font16.DrawText( x, y, m_classifierKN.GetJointReliability( i ) ? CLR_WHITE : CLR_GREY, tmp, ATGFONT_CENTER_X | ATGFONT_CENTER_Y );
            else
                m_Font16.DrawText( x, y, CLR_WHITE, L"X", ATGFONT_CENTER_X | ATGFONT_CENTER_Y );
        }

        dwRelevanceMask >>= 1;
    }

    m_Font16.End();
    m_Font16.SetWindow( ATG::GetTitleSafeArea() );
}

//--------------------------------------------------------------------------------------
// Name: RenderSkeletonsAndLabels()
// Desc: Renders both tracked NUI stickman and animation boxman with labels
//--------------------------------------------------------------------------------------
VOID Sample::RenderSkeletonsAndLabels( XMMATRIX matViewProj, BOOL bRenderAnimationSkeleton ) const
{
    PIX_SCOPED_EVENT( __FUNCTION__ );

    const GestureAnimation& anim = ms_animations[ m_uCurShowAnimation ];

    if( bRenderAnimationSkeleton )
    {
        // render tracked skeleton bones
        ATG::DebugDraw::SetViewProjection( matViewProj );
        const DWORD   clr = ( GetGameClassifierPSNR() > GOOD_PSNR ) ? CLR_GOOD_PSNR : CLR_PASS_PSNR;
        RenderBoxman( anim.m_animToShow.GetJoints(), clr );
    }

    // nui skeleton
    if( m_bWatchingSkeleton )
    {
        RenderNuiStickman( matViewProj, &m_nuiSkeletonData );

        if( bRenderAnimationSkeleton )
            RenderLabels( matViewProj, anim );
    }
}

//--------------------------------------------------------------------------------------
// Name: RenderTextureOverlays()
// Desc: Renders depth surface and debug texture which shows where we sampled depth
//--------------------------------------------------------------------------------------
VOID Sample::RenderTextureOverlays( BOOL bDrawDebugTexture ) const
{
    PIX_SCOPED_EVENT( __FUNCTION__ );

    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_SRCBLEND, D3DBLEND_ONE );
    m_pd3dDevice->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_ONE );
    ATG::DebugDraw::DrawTexturedQuad(   XMFLOAT3( -1, -1, 0 ),
                                        XMFLOAT3(  1, -1, 0 ),
                                        XMFLOAT3( -1,  1, 0 ),
                                        XMFLOAT2( 1, -1 ),
                                        m_depthTexture );
    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );

    if( bDrawDebugTexture )
    {
        m_pd3dDevice->SetRenderState( D3DRS_ALPHATESTENABLE, TRUE );
        m_pd3dDevice->SetRenderState_Inline( D3DRS_ALPHAFUNC, D3DCMP_GREATEREQUAL );
        m_pd3dDevice->SetRenderState_Inline( D3DRS_ALPHAREF, 0x01 );
        ATG::DebugDraw::DrawTexturedQuad(   XMFLOAT3( -1, -1, 0 ),
                                            XMFLOAT3(  1, -1, 0 ),
                                            XMFLOAT3( -1,  1, 0 ),
                                            XMFLOAT2( 1, -1 ),
                                            m_debugTexture );
        m_pd3dDevice->SetRenderState( D3DRS_ALPHATESTENABLE, FALSE );
    }
}



//--------------------------------------------------------------------------------------
// Name: DrawEnterPlayspaceLabel()
// Desc: A helper UI function to render "Please enter playspace" string
//--------------------------------------------------------------------------------------
VOID Sample::DrawEnterPlayspaceLabel() const
{
    m_Font16.Begin();
    m_Font16.SetScaleFactors( 1.0f, 1.0f );
    m_Font16.DrawText( CENTER_LABEL_POS_X, CENTER_LABEL_POS_Y, 0xffff2020, L"Please enter playspace", ATGFONT_CENTER_X | ATGFONT_CENTER_Y );
    m_Font16.End();
}

//--------------------------------------------------------------------------------------
// Name: RenderMainView()
// Desc: Render function that renders skeleton
//--------------------------------------------------------------------------------------
VOID Sample::RenderMainView( BOOL bRenderAnimationSkeleton ) const
{
    PIX_SCOPED_EVENT( __FUNCTION__ );

	D3DVIEWPORT9	saveVp;
	m_pd3dDevice->GetViewport( &saveVp );

	D3DVIEWPORT9	newVp = saveVp;
	newVp.X = WINDOW_START_X;
	newVp.Y = WINDOW_START_Y;
	newVp.Width = WINDOW_WIDTH;
	newVp.Height = WINDOW_HEIGHT;
	m_pd3dDevice->SetViewport( &newVp );

    m_pd3dDevice->SetRenderState( D3DRS_HALFPIXELOFFSET, TRUE );

    ATG::DebugDraw::SetViewProjection( XMMatrixIdentity() );

    // draw underlaying images
    if( m_bWatchingSkeleton )
        RenderTextureOverlays( TRUE );

    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, TRUE );

    // render stickman from animation skeleton or from nui skeleton
    RenderSkeletonsAndLabels( m_matNuiToDepth, bRenderAnimationSkeleton );

    if( !m_bWatchingSkeleton )
        DrawEnterPlayspaceLabel();

	m_pd3dDevice->SetViewport( &saveVp );
}

//--------------------------------------------------------------------------------------
// Name: RenderUIOverlays()
// Desc: Show title, timers, and help.
//--------------------------------------------------------------------------------------
VOID Sample::RenderUIOverlays( BOOL bBare ) const
{
    PIX_SCOPED_EVENT( __FUNCTION__ );

    if( m_bDrawHelp )
    {
        m_Help.Render( &m_Font16, g_HelpCallouts, ARRAYSIZE( g_HelpCallouts ) );
    } else
    {
        m_Font16.Begin();

        if( !bBare )
        {
            const GestureAnimation& anim = ms_animations[ m_uCurShowAnimation ];

            WCHAR tmp[ 128 ];
            swprintf_s( tmp,
                        L"PSNR : %f\nanim %s\nframe : %d\n",
                        GetGameClassifierPSNR(),
                        anim.m_pDisplayName,
                        anim.m_animToShow.GetAnimationFrame() );

            m_Font16.SetScaleFactors( 0.8f, 0.8f );
            m_Font16.DrawText( 0, 25, CLR_WHITE, tmp );
        }

        m_Font16.SetScaleFactors( 1.0f, 1.0f );
        m_Font16.DrawText( 0, 0, 0xff00ffff, L"Gesture scoring" );
        m_Font16.DrawText( 0xffffffff, m_bUseKNClassifier ? L" (KN)" : L" (DTW)" );
        m_Font16.DrawText( 0, 0, 0xffffff00, m_Timer.GetFrameRate(), ATGFONT_RIGHT );

        m_Font16.End();
    }
}

//--------------------------------------------------------------------------------------
// Name: Render()
// Desc: Renders the scene differently based on the current state
//--------------------------------------------------------------------------------------
HRESULT Sample::Render()
{
    PIX_SCOPED_EVENT( __FUNCTION__ );

    // Render scene
    static const D3DVECTOR4 vClearColor = { 90/255.0f, 118/255.0f, 165/255.0f, 1.0f };
    m_pd3dDevice->ClearF( D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, NULL, &vClearColor, 1, 0 );

    switch( m_state )
    {
    case STATE_SHOW:
    case STATE_TRY:
    case STATE_DEFAULT:
        {
            // show the animation debug
            RenderMainView();
            RenderHistoryGraph();

            if( STATE_TRY == m_state )
            {
                m_Font16.Begin();
                m_Font16.SetScaleFactors( 1.5f, 1.5f );

                WCHAR tmp[ 128 ];
                swprintf_s( tmp, L"score %f", m_fScore );
                m_Font16.DrawText( SCORE_LABEL_POS_X, SCORE_LABEL_POS_Y, D3dColourForScore( m_fScore ), tmp, ATGFONT_RIGHT );

                m_Font16.End();
            }
        }
        break;

    case STATE_COUNTDOWN_BEFORE_SHOW:
    case STATE_COUNTDOWN_BEFORE_TRY:
        {
            RenderMainView( FALSE );

            m_Font16.Begin();
            m_Font16.SetScaleFactors( 1.5f, 1.5f );

            WCHAR tmp[ 128 ];
            swprintf_s( tmp,
                        L"%s\n%s\n%d\n", 
                        ms_animations[ m_uCurShowAnimation ].m_pDisplayName,
                        (STATE_COUNTDOWN_BEFORE_TRY == m_state) ? L"repeat in sync in" : L"demo in",
                        1 + static_cast< UINT >( m_fStateTimerCountdown ) );
            m_Font16.DrawText( CENTER_LABEL_POS_X, CENTER_LABEL_POS_Y, 0xff00ff00, tmp, ATGFONT_CENTER_X | ATGFONT_CENTER_Y );

            m_Font16.End();
        }
        break;

    case STATE_SHOW_RESULTS:
        {
            RenderMainView( FALSE );
            RenderHistoryGraph();

            m_Font16.Begin();
            m_Font16.SetScaleFactors( 1.5f, 1.5f );

            WCHAR tmp[ 128 ];
            swprintf_s( tmp, L"Your score is %.2f (%s)\n", m_fScore, DescribeScore( m_fScore ) );
            m_Font16.DrawText( CENTER_LABEL_POS_X, CENTER_LABEL_POS_Y, 0xff00ff00, tmp, ATGFONT_CENTER_X | ATGFONT_CENTER_Y );

            m_Font16.End();
        }
        break;

    case STATE_DETECT_GESTURE:
        {
            PIX_SCOPED_EVENT( "rendering detection view" );

            D3DVIEWPORT9	saveVp;
            m_pd3dDevice->GetViewport( &saveVp );

            D3DVIEWPORT9	newVp = saveVp;
            newVp.X = WINDOW_START_X;
            newVp.Y = WINDOW_START_Y;
            newVp.Width = WINDOW_WIDTH;
            newVp.Height = WINDOW_HEIGHT;
            m_pd3dDevice->SetViewport( &newVp );

            ATG::DebugDraw::SetViewProjection( XMMatrixIdentity() );

            if( m_bWatchingSkeleton )
            {
                RenderTextureOverlays( TRUE );
                RenderNuiStickman( m_matNuiToDepth, &m_nuiSkeletonData );
            } else
            {
                DrawEnterPlayspaceLabel();
            }

            RenderDetectionUI();

            m_Font16.Begin();
            m_Font16.SetScaleFactors( 1.0f, 1.0f );

            const FLOAT fVertOfs = 60.f * (m_uCurFrame - m_uFrameWhenGestureDetected) / 30.f;

            WCHAR tmp[ 128 ];
            swprintf_s( tmp, L"%s %.2f\n", ms_animations[ m_uDetectedGesture ].m_pDisplayName, m_fDetectedGestureScore );
            m_Font16.DrawText( CENTER_LABEL_POS_X, CENTER_LABEL_POS_Y + fVertOfs, 0xff00ff00, tmp, ATGFONT_CENTER_X | ATGFONT_CENTER_Y );
            m_Font16.End();

            m_pd3dDevice->SetViewport( &saveVp );
        }
        break;
    }

    // UI
    RenderUIOverlays( STATE_DETECT_GESTURE == m_state );

    // Draw the raw depth and image map with skeleton overlaid as visualization.
    m_pip.BeginRender();
    m_pip.RenderColorStream( PIP_DRAW_X, PIP_DRAW_Y, PIP_DRAW_WIDTH, PIP_DRAW_HEIGHT );
    m_pip.RenderSkeletons( PIP_DRAW_X, PIP_DRAW_Y, PIP_DRAW_WIDTH, PIP_DRAW_HEIGHT, TRUE );
    m_pip.EndRender();

    // Present the backbuffer contents to the display
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    // insert a fence after a swap to know when exactly the frame goes out to the tv
    // note that TVs have a lag as well, could be 30ms or more
    InsertFence();

    ++m_uCurFrame;

    m_Timer.MarkFrame();

    return S_OK;
}

//--------------------------------------------------------------------------------------
// Name: InsertFence()
// Desc: Puts a fence into the GPU command buffer with the current animation index
//       and frame and a bit to indicate a scoring reset
//--------------------------------------------------------------------------------------
VOID Sample::InsertFence()
{
    const DWORD dwFence = m_pd3dDevice->InsertFence();

    // we should never block on this, it's four frames back
    FrameFence& fe = m_frameFenceBuffer[ IncUintWrap( m_uCurFrame, ARRAYSIZE( m_frameFenceBuffer ) ) ];
    if( fe.m_dwFence    &&
        m_pd3dDevice->IsFencePending( fe.m_dwFence ) )
    {
        ATG_PrintError( "Fence is in the GPU still" );
    }
    fe.m_dwFence = dwFence;
    fe.m_uAnimationData =  ms_animations[ m_uCurShowAnimation ].m_animToShow.GetAnimationFrame()    |
                            (m_uCurShowAnimation << FENCE_ANIM_IDX_OFFSET);

    if( m_bNextFenceResetsTracking )
    {
        m_bNextFenceResetsTracking = FALSE;
        fe.m_uAnimationData |= FENCE_RESET_TRACKING;
    }

    PIXSetMarker( 0, "Fence 0x%x = anim data 0x%x", fe.m_dwFence, fe.m_uAnimationData );
}

//--------------------------------------------------------------------------------------
// Name: GetLastCrossedFence()
// Desc: Checks which GPU fence was crossed last
//--------------------------------------------------------------------------------------
UINT Sample::GetLastCrossedFence()
{
    for( UINT i=0; i < ARRAYSIZE( m_frameFenceBuffer ); ++i )
    {
        const FrameFence& fe = m_frameFenceBuffer[ SubUintWrap( m_uCurFrame, i, ARRAYSIZE( m_frameFenceBuffer ) ) ];
        if( !fe.m_dwFence )
            continue;

        if( !m_pd3dDevice->IsFencePending( fe.m_dwFence ) )
        {
            PIXSetMarker( 0, "Fence 0x%x out, frame 0x%x", fe.m_dwFence, fe.m_uAnimationData );
            return fe.m_uAnimationData;
        }
    }

    return 0;
}

//--------------------------------------------------------------------------------------
// Name: UpdateVisibleAnimation()
// Desc: Steps currently visible animation
//--------------------------------------------------------------------------------------
VOID Sample::UpdateVisibleAnimation( FLOAT fDeltaTime, BOOL bLoop )
{
    PIX_SCOPED_EVENT( __FUNCTION__ );

    // set the skeleton origin and bone lengths
    const XMVECTOR    vOrg = m_bWatchingSkeleton ? m_nuiSkeletonData.SkeletonPositions[ NUI_SKELETON_POSITION_HIP_CENTER ] :
                                DEFAULT_SKELETON_POS;

    ms_animations[ m_uCurShowAnimation ].m_animToShow.CalculateJoints( m_skeletonBonesLengths, vOrg );

    // don't update visible anim if paused
    if( !m_bPausedAnimation )
    {
        ms_animations[ m_uCurShowAnimation ].m_animToShow.Update( fDeltaTime, bLoop );
    }
}

//--------------------------------------------------------------------------------------
// Name: UpdateAnimationScoring()
// Desc: This is part of the Game mode. We ask the player to repeat the animation and
//       we score how the player is doing every frame.
//--------------------------------------------------------------------------------------
VOID Sample::UpdateAnimationScoring( FLOAT fDeltaTime )
{
    PIX_SCOPED_EVENT( __FUNCTION__ );

    // find out what fence has gone out last, this gives us the amount of app lag
    const UINT  uAnimationData = GetLastCrossedFence();
    const UINT  uDisplayedAnimFrame = uAnimationData & ((1 << FENCE_ANIM_IDX_OFFSET) - 1);
    const UINT  uAnim = ((uAnimationData & ~FENCE_RESET_TRACKING) >> FENCE_ANIM_IDX_OFFSET);

    // the high bit will tell us if we need to reset tracking
    if( uAnimationData & FENCE_RESET_TRACKING )
        ResetGameClassifier();

    // update nui bone lengths
    SetBonesLengthsFromAnimation( uAnim );
    if( m_bWatchingSkeleton )
        SetBonesLengthsFromNUI();

    D3DLOCKED_RECT  rcDebug;
    D3DLOCKED_RECT  rcDepth;

    m_debugTexture->LockRect( 0, &rcDebug, NULL, 0 );
    m_depthTexture->LockRect( 0, &rcDepth, NULL, D3DLOCK_READONLY );

    XMemSet128( rcDebug.pBits, 0, RESIZED_DEPTH_HEIGHT * rcDebug.Pitch );

    assert( uAnim < NUM_ANIMATIONS );
    GestureAnimation& anim = ms_animations[ uAnim ];

    if( m_bUseKNClassifier )
    {
        const XMVECTOR    vOrg = m_bWatchingSkeleton ? m_nuiSkeletonData.SkeletonPositions[ NUI_SKELETON_POSITION_HIP_CENTER ] :
                                    DEFAULT_SKELETON_POS;

        m_classifierKN.ScoreNextFrame(   m_bWatchingSkeleton ? &m_skeletonData : NULL,
                                    m_skeletonBonesLengths,
                                    anim.m_animToScore,
                                    anim.m_fBadPSNR,
                                    anim.m_dwJointsMask,
                                    vOrg,
                                    uDisplayedAnimFrame,
                                    TRUE,
                                    &rcDepth,
                                    &rcDebug );
    } else if( !m_bPausedAnimation )
    {
        m_classifierDTW.SetAnimation( anim.m_animToScore, anim.m_dwJointsMask, anim.m_fBadPSNR );
        m_classifierDTW.ScoreNextFrame( m_bWatchingSkeleton ? &m_skeletonData : NULL );
    }

    m_PSNRHistory[ m_uCurPSNRHistoryTail ] = GetGameClassifierPSNR();
    m_ScoreHistory[ m_uCurPSNRHistoryTail ] = GetGameClassifierScore();
    m_uCurPSNRHistoryTail = IncUintWrap( m_uCurPSNRHistoryTail, ARRAYSIZE( m_PSNRHistory ) );

    m_depthTexture->UnlockRect( 0 );
    m_debugTexture->UnlockRect( 0 );
}

//--------------------------------------------------------------------------------------
// Name: DetectGestureKN()
// Desc: This uses a bunch of TrackerKNs to detect which gesture is happening at the moment
//       We add a new KNTracker for each animation every LAG_SEARCH_RANGE frames and then score
//       them continuously. Whichever one wins is the gesture the player is trying to do
//--------------------------------------------------------------------------------------
VOID    Sample::DetectGestureKN()
{
    PIX_SCOPED_EVENT( __FUNCTION__ );

    if( !m_bWatchingSkeleton )
    {
        m_uNumActiveKNTrackers = 0;
        return;
    }

    // add new trackers. note that with classifier using LAG_SEARCH_RANGE it's pointless
    // to spawn them more frequently than every LAG_SEARCH_RANGE - 1 frames because
    // each classifier will look LAG_SEARCH_RANGE frames around the expected frame
    if( 0 == (m_uCurFrame % LAG_SEARCH_RANGE) )
    {
        for( UINT i=0; i < NUM_ANIMATIONS; ++i )
        {
            if( m_uNumActiveKNTrackers < NUM_KN_TRACKERS )
            {
                m_KNTrackers[ m_uNumActiveKNTrackers ].m_uAnim = static_cast< BYTE >( i );
                m_KNTrackers[ m_uNumActiveKNTrackers ].m_uCurFrame = 0;
                m_KNTrackers[ m_uNumActiveKNTrackers ].m_classifier.Reset();
                ++m_uNumActiveKNTrackers;
            }
        }
    }

    // process all current trackers
    for( UINT i=0; i < m_uNumActiveKNTrackers; ++i )
    {
        GestureAnimation& anim = ms_animations[ m_KNTrackers[ i ].m_uAnim ];

        // reached the end of it's lifetime without being killed
        if( m_KNTrackers[ i ].m_uCurFrame >= anim.m_uEndFrame   &&
            m_KNTrackers[ i ].m_classifier.GetScore() > SCORE_FAIL )
        {
            m_uDetectedGesture = m_KNTrackers[ i ].m_uAnim;
            m_bNewDetectedGesture = TRUE;
            m_uFrameWhenGestureDetected = m_uCurFrame;
            m_fDetectedGestureScore = m_KNTrackers[ i ].m_classifier.GetScore();

            // remove all occurances of that animation from tracking
            for( UINT j=0; j < m_uNumActiveKNTrackers; ++j )
            {
                if( m_KNTrackers[ j ].m_uAnim == m_KNTrackers[ i ].m_uAnim )
                {
                    m_KNTrackers[ j ] = m_KNTrackers[ m_uNumActiveKNTrackers - 1 ];
                    --m_uNumActiveKNTrackers;
                    --j;
                }
            }
            break;
        }

        // score the KNTracker
        m_KNTrackers[ i ].m_classifier.ScoreNextFrame(  &m_skeletonData,
                                                m_skeletonBonesLengths,
                                                anim.m_animToScore,
                                                anim.m_fBadPSNR,
                                                anim.m_dwJointsMask,
                                                m_nuiSkeletonData.SkeletonPositions[ NUI_SKELETON_POSITION_HIP_CENTER ],
                                                m_KNTrackers[ i ].m_uCurFrame,
                                                FALSE,
                                                NULL,
                                                NULL );
        ++m_KNTrackers[ i ].m_uCurFrame;

        // check if it's not doing very well and delete it
        if( m_KNTrackers[ i ].m_classifier.GetScore() <= 0 )
        {
            m_KNTrackers[ i ] = m_KNTrackers[ m_uNumActiveKNTrackers - 1 ];
            --m_uNumActiveKNTrackers;
            --i;
        }
    }
}

//--------------------------------------------------------------------------------------
// Name: DetectGestureDTW()
// Desc: Continuously tries to detect what gesture a player is doing by using a number of
//       DTW classifiers
//--------------------------------------------------------------------------------------
VOID    Sample::DetectGestureDTW()
{
    PIX_SCOPED_EVENT( __FUNCTION__ );

    static_assert( NUM_ANIMATIONS == ARRAYSIZE( m_DTWTrackers ), "ShouldBeTheSameSize" );

    if( !m_bWatchingSkeleton )
        return;

    FLOAT   fBestScore = 0;
    UINT    uBestAnimation = NUM_ANIMATIONS;
    for( UINT i=0; i < NUM_ANIMATIONS; ++i )
    {
        m_DTWTrackers[ i ].ScoreNextFrame( &m_skeletonData );

        const FLOAT fScore = m_DTWTrackers[ i ].GetScore();
        if( fScore > fBestScore )
        {
            fBestScore = fScore;
            uBestAnimation = i;
        }
    }

    if( fBestScore > SCORE_FAIL )
    {
        m_uDetectedGesture = uBestAnimation;
        m_bNewDetectedGesture = TRUE;
        m_uFrameWhenGestureDetected = m_uCurFrame;
        m_fDetectedGestureScore = fBestScore;

        for( UINT i=0; i < NUM_ANIMATIONS; ++i )
            m_DTWTrackers[ i ].Reset();
    }
}


//--------------------------------------------------------------------------------------
// Name: CalculateMSE()
// Desc: Calculate Mean Square Error
//--------------------------------------------------------------------------------------
FLOAT   PSNRRecord::CalculateMSE() const
{
    UINT    uCount = 0;
    FLOAT   fMse = 0;

    // abs position or rotation
    for( UINT i=0; i < NUI_SKELETON_POSITION_COUNT; ++i )
    {
        if( m_jointPartOfCalculation[ i ] )
        {
            fMse += m_jointDistanceFromRef[ i ] * m_jointDistanceFromRef[ i ];
            ++uCount;
        }
    }

    if( !uCount || fMse < 0.0000001f )
        return 0;

    fMse /= static_cast< FLOAT >( uCount );

    return fMse;
}

//--------------------------------------------------------------------------------------
// Name: KNClassifier()
// Desc: KNClassifier constructor
//--------------------------------------------------------------------------------------
KNClassifier::KNClassifier()
{
    Reset();
}

//--------------------------------------------------------------------------------------
// Name: GetPSNR()
// Desc: Returns current Peak Signal to Noise Ratio
//--------------------------------------------------------------------------------------
FLOAT   KNClassifier::GetPSNR() const
{
    return m_fPSNR;
}

//--------------------------------------------------------------------------------------
// Name: GetScore()
// Desc: Returns current KNTracker score
//--------------------------------------------------------------------------------------
FLOAT   KNClassifier::GetScore() const
{
    if( m_uNumPenaltyFrames > (m_fNumFrames / FRACTION_PENALTY_FRAMES) )
        return 0;

    return m_fScoreAccum / m_fNumFrames;
}

//--------------------------------------------------------------------------------------
// Name: GetJointScore()
// Desc: Returns error metric for each joint
//--------------------------------------------------------------------------------------
FLOAT   KNClassifier::GetJointScore( UINT i ) const
{
    assert( i < ARRAYSIZE( m_psnrRecord.m_jointDistanceFromRef ) );

    return m_psnrRecord.m_jointDistanceFromRef[ i ];
}

//--------------------------------------------------------------------------------------
// Name: GetJointReliability()
// Desc: Returns reliability for each joint
//--------------------------------------------------------------------------------------
BOOL    KNClassifier::GetJointReliability( UINT i ) const
{
    assert( i < ARRAYSIZE( m_psnrRecord.m_jointReliable ) );

    return m_psnrRecord.m_jointReliable[ i ];
}

//--------------------------------------------------------------------------------------
// Name: ScoreNextFrame()
// Desc: We search for the best score amongst a few past frames to compensate for
//       human lag. The player should move synchronously with the animation, this
//       classifier can't make up for out of sync movements.
//--------------------------------------------------------------------------------------
VOID    KNClassifier::ScoreNextFrame(   const Skeleton* pSkeleton,
                                        const FLOAT* pBonesLength,
                                        AnimationState& animToScore,
                                        FLOAT fBadPSNR,
                                        DWORD dwJointsMask,
                                        XMVECTOR vOrg,
                                        UINT uDisplayedAnimFrame,
                                        BOOL bPenaliseLag,
                                        const D3DLOCKED_RECT* pRcDepth,
                                        const D3DLOCKED_RECT* pRcDebug )
{
    PIX_SCOPED_EVENT( __FUNCTION__ );

    // search back a few frames if needed
    // we assume the player is trying to repeat what's on the screen
    // scoring function will penalise them for not even trying

    INT     iBestIdx = LAG_SEARCH_RANGE - LAG_SEARCH_AHEAD;
    FLOAT   fBest = 0;
    m_fPSNR = 0;

    if( pSkeleton )
    {
        const DWORD dwUpdateMask = FullPathsMaskFromMask( dwJointsMask );

        for( INT i=0; i < LAG_SEARCH_RANGE; ++i )
        {
            const INT iWantFrame = uDisplayedAnimFrame - i + LAG_SEARCH_AHEAD;

            // not enough frames played by now to look that far back?
            if( iWantFrame < 0 )
                break;

            // not enough frames in anim to look that far ahead?
            if( static_cast< UINT >( iWantFrame ) >= animToScore.GetNumFrames() )
                continue;

            // this is what we're going to compare against
            animToScore.SetAnimationFrame( iWantFrame );
            animToScore.CalculateJoints( pBonesLength, vOrg );

            CalculateAngularDistance(   m_psnrRecord,
                                        animToScore.GetJointsOffsets(),
                                        pSkeleton,
                                        dwUpdateMask,
                                        dwJointsMask,
                                        animToScore.GetJoints(),
                                        pRcDepth,
                                        pRcDebug );

            const FLOAT fScore = PSNRFromMSE( m_psnrRecord.CalculateMSE(), MAX_MSE );
            if( fScore > fBest )
            {
                fBest = fScore;
                iBestIdx = i - LAG_SEARCH_AHEAD;
            }
        }
    }

    // scoring
    m_fPSNR = fBest;

    ++m_fNumFrames;

    FLOAT   fMatchScore = CalculateMatchScore( m_fPSNR, fBadPSNR );
    if( 0 == fMatchScore )
    {
        m_fScoreAccum *= SCORE_PENALTY;
        ++m_uNumPenaltyFrames;
    }

    if( bPenaliseLag )
    {
        // +1 to make sure LAG_SEARCH_RANGE frame still gives non zero response in edge cases
        FLOAT   fLagScore = 1.f - static_cast< FLOAT >( abs( iBestIdx ) ) /
                                static_cast< FLOAT >( LAG_SEARCH_RANGE - LAG_SEARCH_AHEAD + 1 );
        fLagScore = min( 1, max( 0, fLagScore ) );

        fMatchScore *= fLagScore;
    }

    m_fScoreAccum += fMatchScore;
}

//--------------------------------------------------------------------------------------
// Name: Reset()
// Desc: Resets the KNTracker's scoring etc
//--------------------------------------------------------------------------------------
VOID    KNClassifier::Reset()
{
    m_fPSNR = 0;
    m_fScoreAccum = 0;
    m_fNumFrames = 0;
    m_uNumPenaltyFrames = 0;
}



//--------------------------------------------------------------------------------------
// Name: DTWClassifier
// Desc: Dynamic Time Warp classifier constructor
//--------------------------------------------------------------------------------------
DTWClassifier::DTWClassifier()
{
    Reset();

    const UINT  uNumColumns = ( ARRAYSIZE( m_history ) );
    const UINT  uNumRows = ( ARRAYSIZE( m_history[ 0 ].m_costCache ) );

    m_pMatCost = new FLOAT[ (uNumColumns + 1) * (uNumRows + 1) ];
    m_pMatCostLocal = new FLOAT[ (uNumColumns + 1) * (uNumRows + 1) ];
    m_pPath = new SHORT[ 2 * (uNumColumns + uNumRows) ];
    m_pMatPath = new BYTE[ uNumColumns * uNumRows ];
    m_uSequenceBSize = 0;
    m_uSequenceASize = 0;
    m_uNumStepsInPath = 0;
}

//--------------------------------------------------------------------------------------
// Name: DTWClassifier
// Desc: Dynamic Time Warp classifier constructor
//--------------------------------------------------------------------------------------
DTWClassifier::~DTWClassifier()
{
    delete[] m_pMatCost;
    delete[] m_pMatCostLocal;
    delete[] m_pPath;
    delete[] m_pMatPath;
}

//--------------------------------------------------------------------------------------
// Name: GetPSNR()
// Desc: Returns Peak Signal To Noise Ratio for the shortest path
//--------------------------------------------------------------------------------------
FLOAT DTWClassifier::GetPSNR() const
{
    return PSNRFromMSE( m_fRmse * m_fRmse, MAX_MSE );
}


//--------------------------------------------------------------------------------------
// Name: GetScore()
// Desc: A bit of heuristics to make sure we correctly score valid attempts
//--------------------------------------------------------------------------------------
FLOAT DTWClassifier::GetScore() const
{
    return m_fScoreAccum;
}

//--------------------------------------------------------------------------------------
// Name: SetAnimation()
// Desc: This DTW classifier uses caching, so it needs informing when the animation
//       changes. This function resets cost caches.
//--------------------------------------------------------------------------------------
void    DTWClassifier::SetAnimation( AnimationState& animToScore, DWORD dwJointsMask, FLOAT fBadPSNR )
{
    if( m_pAnimToScore == &animToScore )
        return;

    m_uHistoryTail = 0;
    XMemSet( m_history, 0, sizeof( m_history ) );
    m_fRmse = MAX_MSE;
    m_fRmseMax = MAX_MSE;
    m_fRmseMin = MAX_MSE;
    m_pAnimToScore = &animToScore;
    m_dwJointsMask = dwJointsMask;
    m_fBadPSNR = fBadPSNR;

    assert( ARRAYSIZE( m_history[ 0 ].m_costCache ) >= m_pAnimToScore->GetNumFrames() );
}


//--------------------------------------------------------------------------------------
// Name: GetNumValidNuiFrames()
// Desc: Find how many reliable frames we've got recorded in the NUI history buffer
//--------------------------------------------------------------------------------------
UINT DTWClassifier::GetNumValidNuiFrames( UINT uMaxLength ) const
{
    // start from the tail and step back until a non-tracked frame is found or we wrap around
    const UINT  uStartIndex = DecUintWrap( m_uHistoryTail, ARRAYSIZE( m_history ) );
    UINT    uCount = 0;
    UINT uTestIndex = uStartIndex;
    do
    {
        if( !m_history[ uTestIndex ].m_bTracked )
            break;

        ++uCount;

        uTestIndex = DecUintWrap( uTestIndex, ARRAYSIZE( m_history ) );
    } while( uTestIndex != uStartIndex && uCount < uMaxLength );


    return uCount;
}

//--------------------------------------------------------------------------------------
// Name: RetracePath()
// Desc: Trace back to the top left corner using the recorded warping path
//--------------------------------------------------------------------------------------
UINT    DTWClassifier::RetracePath( INT& iLeftEdge, SHORT* pPath, BYTE* pMatPath, INT iX, INT iY, UINT uSequenceASize )
{
    UINT    uNumSteps = 0;

    pPath[ 0 ] = static_cast< SHORT >( iX + 1 );
    pPath[ 1 ] = static_cast< SHORT >( iY + 1 );

    while( (iX > 0) && (iY > 1) )
    {
        switch( pMatPath[ iX + iY * uSequenceASize ] )
        {
        default:
            // diagonal can't be walked, increase search band so the diagonal is walkable
            ATG::DebugSpew( "no path %d\n", uNumSteps );
            assert( 0 );
            break;

        case STEP_DIAG:
            --iX;
            --iY;
            break;

        case STEP_HORZ:
            --iX;
            break;

        case STEP_VERT:
            --iY;
            break;
        }

        pPath[ 0 ] = static_cast< SHORT >( iX + 1 );
        pPath[ 1 ] = static_cast< SHORT >( iY + 1 );
        pPath += 2;

        ++uNumSteps;
    }

    iLeftEdge = iX;

    return uNumSteps;
}

//--------------------------------------------------------------------------------------
// Name: ScoreNextFrame()
// Desc: This works differently from KN classifier by using Dynamic Time Warping
//       algorithm to score the sequence of NUI frames against an animation
//--------------------------------------------------------------------------------------
VOID DTWClassifier::ScoreNextFrame( const Skeleton* pSkeleton )
{
    PIX_SCOPED_EVENT( __FUNCTION__ );

    assert( m_pAnimToScore );

    if( pSkeleton )
    {
        // add one frame to the matching history
        XMemCpy( &m_history[ m_uHistoryTail ].m_skeleton, pSkeleton, sizeof( *pSkeleton ) );
        m_history[ m_uHistoryTail ].m_bTracked = TRUE;
        m_history[ m_uHistoryTail ].ResetCostCache();
        m_uHistoryTail = IncUintWrap( m_uHistoryTail, ARRAYSIZE( m_history ) );
    } else
    {
        // mark frame as not tracked but still step the counter and don't do work
        m_history[ m_uHistoryTail ].m_bTracked = FALSE;
        m_uHistoryTail = IncUintWrap( m_uHistoryTail, ARRAYSIZE( m_history ) );
        return;
    }

    // this is number of rows in our matrix -- each row is a frame of animation
    const UINT uSequenceBSize = m_pAnimToScore->GetNumFrames();

    // find the longest valid sequence in the NUI history, limiting the look back distance for shorter animations
    const UINT uSequenceASize = GetNumValidNuiFrames( uSequenceBSize + LAG_SEARCH_RANGE );
    if( !uSequenceASize )
        return;

    const UINT uHistoryBase = SubUintWrap( m_uHistoryTail, uSequenceASize, ARRAYSIZE( m_history ) );
    const UINT uSequenceASize1 = uSequenceASize + 1;

    // can never happen
    assert( uSequenceASize <= ARRAYSIZE( m_history ) );
    assert( uSequenceBSize <= ARRAYSIZE( m_history[ 0 ].m_costCache ) );

    // the algorithm won't go anywhere where there is FLT_MAX and it should terminate at 0,0
    for( UINT i=uSequenceASize1; i < uSequenceASize1 * (uSequenceBSize + 1); ++i )
        m_pMatCost[ i ] = FLT_MAX;

    // this is a modification to the original DTW
    for( UINT i=0; i < uSequenceASize1; ++i )
        m_pMatCost[ i ] = 0;

    // copy into local cost matrix for path analysis
    XMemCpy( m_pMatCostLocal, m_pMatCost, uSequenceASize1 * (uSequenceBSize + 1) * 4 );

    // initialize the DTW path
    XMemSet( m_pMatPath, STEP_NONE, uSequenceASize * uSequenceBSize );

    // cache skeleton update flags for the cost function
    const DWORD dwUpdateMask = FullPathsMaskFromMask( m_dwJointsMask );

    // inclinations of the corridor -- right hand line goes from bottom right corner
    const FLOAT fAtoB = (FLOAT)uSequenceASize / (FLOAT)uSequenceBSize;

    for( UINT i=1; i <= uSequenceBSize; ++i )
    {
        const UINT  uAnimFrame = i - 1;

        // this is what we're going to compare against
        // note that we don't update joints positions as we're only going to use
        // offsets representation
        m_pAnimToScore->SetAnimationFrame( uAnimFrame );

        // this is a simplification of Sakoe-Chiba band restriction
        const INT iDiagonal = static_cast< INT >( fAtoB * i );
        const UINT uStartColumn = static_cast< UINT >( max( 1, iDiagonal - DTW_LAG_CORRIDOR ) );
        const UINT uEndColumn = static_cast< UINT >( min( (INT)uSequenceASize, iDiagonal + DTW_LAG_CORRIDOR * 2 ) );

        for( UINT j = uStartColumn; j <= uEndColumn; ++j )
        {
            // start from the FIRST frame in the sequence which follows AFTER the tail
            const UINT  uHistoryIndex = AddUintWrap( (j - 1), uHistoryBase, ARRAYSIZE( m_history ) );

            assert( m_history[ uHistoryIndex ].m_bTracked );

            const Skeleton* pHistorySkeleton = &m_history[ uHistoryIndex ].m_skeleton;
            
            // check its cost is in the cache, calculate and store it otherwise
            FLOAT   fRMSECost;
            if( !m_history[ uHistoryIndex ].GetCostCached( fRMSECost, uAnimFrame ) )
            {
                PSNRRecord  psnrRecord;
                CalculateAngularDistance(   psnrRecord,
                                            m_pAnimToScore->GetJointsOffsets(),
                                            pHistorySkeleton,
                                            dwUpdateMask,
                                            m_dwJointsMask );

                fRMSECost = sqrtf( psnrRecord.CalculateMSE() );

                m_history[ uHistoryIndex ].SetCostCached( fRMSECost, uAnimFrame );
            }

            // retrieve cost for 3 neighboring cells, they will be already filled in
            const FLOAT fDiag = m_pMatCost[ (j - 1) + (i - 1) * uSequenceASize1 ];
            const FLOAT fLeft = m_pMatCost[ (j - 1) + (i)     * uSequenceASize1 ];
            const FLOAT fUp   = m_pMatCost[ (j)     + (i - 1) * uSequenceASize1 ];

            // choose the minimum of 3
            FLOAT   fMinimum = fDiag;
            Step    step = STEP_DIAG;

            if( fLeft < fMinimum )
            {
                fMinimum = fLeft;
                step = STEP_HORZ;
            }

            if( fUp < fMinimum )
            {
                fMinimum = fUp;
                step = STEP_VERT;
            }

            // store cost for this cell and also store the path, note the path matrix
            // is smaller by 1 in both dimensions
            m_pMatCostLocal[ j + i * uSequenceASize1 ] = fRMSECost;
            m_pMatCost[ j + i * uSequenceASize1 ] = fRMSECost + fMinimum;
            m_pMatPath[ (j - 1) + (i - 1) * uSequenceASize ] = static_cast< BYTE >( step );
        }
    }

    // scan along the bottom row and find the best score. if we can find it that means there was a
    // better sequence previously
    FLOAT fMinSubCost = m_pMatCost[ uSequenceASize + uSequenceBSize * uSequenceASize1 ];
    UINT uMinIndexColumn = uSequenceASize;
    for( INT i = uSequenceASize - 1; i >= 0; i-- )
    {
        const FLOAT fCost = m_pMatCost[ i + uSequenceBSize * uSequenceASize1 ];

        if( fMinSubCost > fCost )
        {
            fMinSubCost = fCost;
            uMinIndexColumn = i;
        }

        if( fCost == FLT_MAX )
            break;
    }

    INT iLeftEdge = 0;
    m_uNumStepsInPath = RetracePath( iLeftEdge, m_pPath, m_pMatPath, uMinIndexColumn - 1, uSequenceBSize - 1, uSequenceASize );
    m_fRmse = fMinSubCost / static_cast< FLOAT >( m_uNumStepsInPath );
    m_uSequenceASize = uSequenceASize;
    m_uSequenceBSize = uSequenceBSize;
    m_uLeftEdge = iLeftEdge;
    m_uRightEdge = uMinIndexColumn;

    // score the path. if the path is too short then score is 0 because no data has been collected yet
    // we copy the KN logic here and score down sequences that go either horizontally or vertically
    // a horizontal line in the path means the player hasn't moved at all or quickly enough and the
    // vertical line means the player moved too quickly. ideal path is diagonal here.
    const UINT  uNumFramesInBestRun = m_uRightEdge - m_uLeftEdge + 1;
    m_fScoreAccum = 0;
    if( uNumFramesInBestRun > uSequenceBSize / 2 )
    {
        UINT    uNumPenaltyFrames = 0;
        for( UINT i=0; i < m_uNumStepsInPath; ++i )
        {
            const INT  uX = m_pPath[ 2 * i + 0 ];
            const INT  uY = m_pPath[ 2 * i + 1 ];

            const FLOAT fRMSE = m_pMatCostLocal[ uX + uY * uSequenceASize1 ];
            const FLOAT fPSNR = PSNRFromMSE( fRMSE * fRMSE, MAX_MSE );

            FLOAT   fMatchScore = CalculateMatchScore( fPSNR, m_fBadPSNR );
            if( 0 == fMatchScore )
            {
                m_fScoreAccum *= SCORE_PENALTY;
                ++uNumPenaltyFrames;
            }

            m_fScoreAccum += fMatchScore;

            if( uNumPenaltyFrames > (uNumFramesInBestRun / FRACTION_PENALTY_FRAMES) )
            {
                m_fScoreAccum = 0;
                break;
            }
        }

        m_fScoreAccum /= static_cast< FLOAT >( m_uNumStepsInPath );
    }
}

//--------------------------------------------------------------------------------------
// Name: Reset()
// Desc: Resets the score
//--------------------------------------------------------------------------------------
VOID DTWClassifier::Reset()
{
    m_fRmseMin = MAX_MSE;
    m_fRmseMax = MAX_MSE;
    m_fRmse = MAX_MSE;
    m_fScoreAccum = 0;
    m_history[ 0 ].m_bTracked = FALSE;
    m_uHistoryTail = 1;
}


//--------------------------------------------------------------------------------------
// Name: RenderDTWMatrix()
// Desc: Render the heatmap of cost matrix and the shortest path through it
//--------------------------------------------------------------------------------------
VOID DTWClassifier::RenderMatrix( D3DDevice* pDevice, FLOAT fX, FLOAT fY, FLOAT fWidth, FLOAT fHeight ) const
{
    PIX_SCOPED_EVENT( __FUNCTION__ );

    ScopedViewportChange    pushViewport( pDevice, fX, fY, fWidth, fHeight );

    const FLOAT fStepX = 2.f / static_cast< FLOAT >( m_uSequenceASize );
    const FLOAT fStepY = 2.f / static_cast< FLOAT >( m_uSequenceBSize );

    ATG::SimpleShaders::SetDeclPosColor();
    ATG::SimpleShaders::BeginShader_PreTransformed_VertexColor();

    pDevice->SetRenderState( D3DRS_ZENABLE, FALSE );

    D3DBLENDSTATE blendState;
    blendState.SrcBlend       = D3DBLEND_SRCALPHA;
    blendState.BlendOp        = D3DBLENDOP_ADD;
    blendState.DestBlend      = D3DBLEND_INVSRCALPHA;
    blendState.SrcBlendAlpha  = D3DBLEND_ONE;
    blendState.BlendOpAlpha   = D3DBLENDOP_ADD;
    blendState.DestBlendAlpha = D3DBLEND_INVSRCALPHA;
    pDevice->SetBlendState( 0, blendState );

    ATG::MeshVertexPC   vertices[ 256 ];
    UINT    uNumVerts = 0;

    SetFilledRect( vertices, -1, -1, 2, 2, D3DCOLOR_RGBA( 255, 255, 255, 64 ) );
    pDevice->DrawVerticesUP( D3DPT_RECTLIST, 3, vertices, sizeof( ATG::MeshVertexPC ) );

    // draw the cost matrix
    for( UINT i=0; i <= m_uSequenceBSize; ++i )
    {
        for( UINT j=0; j <= m_uSequenceASize; ++j )
        {
            const FLOAT fCost = m_pMatCostLocal[ j + i * (m_uSequenceASize + 1) ];

            if( fCost == FLT_MAX )
                continue;

            DWORD   dwClr = 0;
            if( fCost != FLT_MAX )
            {
                const DWORD dwValue = min( 255, static_cast< DWORD >( 768 * fCost ) );
                dwClr = D3DCOLOR_RGBA( dwValue, 255 - dwValue, 0, 208 );
            }

            if( uNumVerts + 3 > ARRAYSIZE( vertices ) )
            {
                pDevice->DrawVerticesUP( D3DPT_RECTLIST, uNumVerts, vertices, sizeof( ATG::MeshVertexPC ) );
                uNumVerts = 0;
            }

            SetFilledRect( &vertices[ uNumVerts ], -1 + j * fStepX, 1 - i * fStepY, fStepX, fStepY, dwClr );
            uNumVerts += 3;
        }
    }

    if( uNumVerts )
        pDevice->DrawVerticesUP( D3DPT_RECTLIST, uNumVerts, vertices, sizeof( ATG::MeshVertexPC ) );

    // draw the path
    if( m_uNumStepsInPath )
    {
        assert( ARRAYSIZE( vertices ) >= m_uNumStepsInPath );

        for( UINT i=0; i < m_uNumStepsInPath; ++i )
        {
            const UINT  uX = m_pPath[ i * 2 + 0 ];
            const UINT  uY = m_pPath[ i * 2 + 1 ];
            const FLOAT fCost = m_pMatCostLocal[ uX + uY * (m_uSequenceASize + 1) ];
            const FLOAT fPSNR = PSNRFromMSE( fCost * fCost, MAX_MSE );

            const FLOAT fMatchScore = CalculateMatchScore( fPSNR, m_fBadPSNR );
            const DWORD dwValue = min( 255, static_cast< DWORD >( 512 * fMatchScore ) );
            const DWORD dwClr = D3DCOLOR_RGBA( dwValue, dwValue, dwValue, 255 );

            SetVertex( vertices[ i ], -1 + uX * fStepX, 1 - uY * fStepY, dwClr );
        }

        SetVertex( vertices[ m_uNumStepsInPath ], -1, 1, CLR_WHITE );

        pDevice->DrawVerticesUP( D3DPT_LINESTRIP, 1 + m_uNumStepsInPath, vertices, sizeof( ATG::MeshVertexPC ) );
    }

    // draw the found sequence limits
    SetVertex( vertices[ 0 ], -1 + m_uRightEdge * fStepX, -1, CLR_WHITE );
    SetVertex( vertices[ 1 ], -1 + m_uRightEdge * fStepX,  1, CLR_WHITE );
    SetVertex( vertices[ 2 ], -1 + m_uLeftEdge * fStepX, -1, CLR_WHITE );
    SetVertex( vertices[ 3 ], -1 + m_uLeftEdge * fStepX,  1, CLR_WHITE );
    pDevice->DrawVerticesUP( D3DPT_LINELIST, 4, vertices, sizeof( ATG::MeshVertexPC ) );

    pDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );
    ATG::SimpleShaders::EndShader();
}

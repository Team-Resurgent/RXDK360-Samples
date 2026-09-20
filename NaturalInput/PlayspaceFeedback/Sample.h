//----------------------------------------------------------------------------------------------------------------------
// Sample.h
// 
// Defines the Sample class, which is used to control the main game loop.
//
// Advanced Technology Group (ATG)
// Copyright (c) Microsoft Corporation. All rights reserved.
//----------------------------------------------------------------------------------------------------------------------

#pragma once

#ifndef SAMPLE_H_GUARD
#define SAMPLE_H_GUARD

//----------------------------------------------------------------------------------------------------------------------
// Forward Declarations
//----------------------------------------------------------------------------------------------------------------------

class AvatarRenderer;

//----------------------------------------------------------------------------------------------------------------------
// Name: enum GAMEPLAYERID
//----------------------------------------------------------------------------------------------------------------------

enum GAMEPLAYERID
{
    GAME_PLAYER_ONE = 0,
//  GAME_PLAYER_TWO = 1,	// Only one player is supported currently.
    GAME_PLAYER_COUNT
};

//----------------------------------------------------------------------------------------------------------------------
// Name: enum AVATARANIM
// Desc: List of color animations which can be applied to the avatar.
//----------------------------------------------------------------------------------------------------------------------
enum AVATARANIM
{
    AVATARANIM_NONE,
    AVATARANIM_SWEETSPOTGLOW,
    AVATARANIM_EDGEOFBOUNDARYCYCLE,
    AVATARANIM_OUTOFBOUNDSCYCLE,
};

//----------------------------------------------------------------------------------------------------------------------
// Name: enum TILTMODE
// Desc: List of values which can be applied to the tilt camera.
//----------------------------------------------------------------------------------------------------------------------
enum TILTMODE
{
    TILT_MODE_FULL_SKELETON = 0,
    TILT_MODE_UPPER_BODY,
    TILT_MODE_HANDS_OVER_HEAD,
    TILT_MODE_FORCE_FAR_SPACE,
    TILT_MODE_FORCE_NEAR_SPACE,
    TILT_MODE_COUNT
};

//----------------------------------------------------------------------------------------------------------------------
// Name: enum TILTSTATE
// Desc: List of states that the tilt adjustment can be in.
//----------------------------------------------------------------------------------------------------------------------
enum TILTSTATE
{
    TILT_STATE_NOINFO,  // The initial state, on system startup, before any calls to NuiCameraAdjustTilt have been made.
    TILT_STATE_BUSY,    // We're in the middle of a tilt
    TILT_STATE_IDLE     // We've tried to tilt, and have finished.
};


//----------------------------------------------------------------------------------------------------------------------
// Name: struct PlayspaceHUDGlyph
// Desc: Shows an outline, a "man" icon, and either a check mark or a question mark, depending on if the player is
//       "lost" on the HUD.
//----------------------------------------------------------------------------------------------------------------------
struct PlayspaceHUDGlyph
{
    D3DRECT                     m_rectGlyph;
    D3DRECT                     m_rectHintLeft;
    D3DRECT                     m_rectHintRight;
    D3DRECT                     m_rectHintTop;
    D3DRECT                     m_rectHintBottom;

    D3DCOLOR                    m_colDavinciOutline;
    D3DCOLOR                    m_colDavinciMan;
    D3DCOLOR                    m_colDavinciLost;
    D3DCOLOR                    m_colDavinciGood;

    D3DCOLOR                    m_colHint;

    AlphaTrack                  m_animDavinciOutline;
    AlphaTrack                  m_animDavinciMan;
    AlphaTrack                  m_animDavinciLost;
    AlphaTrack                  m_animDavinciGood;

    AlphaTrack                  m_animHints;

    DWORD                       m_dwFrustumFlags;
    PLAYSPACEHINTEFFECT         m_effect;
};


//----------------------------------------------------------------------------------------------------------------------
// Name: struct PlayspaceHUDResources
// Desc: Holds glyph textures used by the onscreen HUD.
//----------------------------------------------------------------------------------------------------------------------
struct PlayspaceHUDResources
{
    LPDIRECT3DTEXTURE9          m_pTextureDavinciOutline;
    LPDIRECT3DTEXTURE9          m_pTextureDavinciMan;
    LPDIRECT3DTEXTURE9          m_pTextureDavinciLost;
    LPDIRECT3DTEXTURE9          m_pTextureDavinciGood;

    LPDIRECT3DTEXTURE9          m_pTextureHintMoveLeft;
    LPDIRECT3DTEXTURE9          m_pTextureHintMoveRight;
    LPDIRECT3DTEXTURE9          m_pTextureHintMoveFwd;
    LPDIRECT3DTEXTURE9          m_pTextureHintMoveBack;
};

//--------------------------------------------------------------------------------------
// Name: class Sample
// Desc: Main class to run this application.
//--------------------------------------------------------------------------------------
class Sample : public ATG::Application
{
public:
    Sample() :
        m_bDrawHelp( FALSE ),
        m_dwCurFrontBuffer(0),
        m_fCurTime(0.0f),
        m_pAvatarRenderer( NULL ),
        m_pAvatarNuiMapper( NULL ),
        m_vAvatarColorFilter( g_XMOne ),
        m_vAvatarPos( g_XMZero ),
        m_bShowPlayspace( FALSE ),
        m_AvatarBodyType( XAVATAR_BODY_TYPE_UNKNOWN ),
        m_currentTiltMode( TILT_MODE_FORCE_FAR_SPACE ), // All titles start in FAR_SPACE mode.
        m_tiltState( TILT_STATE_NOINFO )                // We've not tilted at all yet.
    {
        ZeroMemory( &m_ovTiltComplete, sizeof(XOVERLAPPED) );

        m_colPlayspaceFrustum[ DebugPlayspaceVerts::DBGPSFACE_FRONT ] = D3DCOLOR_ARGB( 0x80, 0xFF, 0x00, 0x00 ); // Red, 50% opacity
        m_colPlayspaceFrustum[ DebugPlayspaceVerts::DBGPSFACE_LEFT ] = D3DCOLOR_ARGB( 0x80, 0x00, 0xFF, 0x00 ); // Green, 50% opacity
        m_colPlayspaceFrustum[ DebugPlayspaceVerts::DBGPSFACE_RIGHT ] = D3DCOLOR_ARGB( 0x80, 0x00, 0x00, 0xFF ); // Blue, 50% opacity
        m_colPlayspaceFrustum[ DebugPlayspaceVerts::DBGPSFACE_BACK ] = D3DCOLOR_ARGB( 0x20, 0xFF, 0xFF, 0xFF ); // White, 12% opacity
    }

    virtual ~Sample();

    void SetAvatarOpacity( FLOAT fOpacity = 1.0f );
    void StartSweetSpotGlow();
    void StopSweetSpotGlow();
    void ShowHUDPlayspaceHint( GAMEPLAYERID id, PLAYSPACEHINTEFFECT effect, DWORD dwFrustumPositionFlags );
    void ShowHUDPlayerLost( GAMEPLAYERID id );
    void ShowHUDPlayerFound( GAMEPLAYERID id );

private:
    virtual HRESULT Initialize();
    virtual HRESULT Update();

    void UpdateInput();

    HRESULT UpdateSkeletonTracking();
    VOID CreateRenderTargets();
    virtual HRESULT Render();
    HRESULT RenderBackground();
    VOID RenderOverlays();
    VOID DrawDebugSkeleton();

    void RenderSweetSpotTarget();
    void RenderPlayspaceFrustum();

    HRESULT UpdateAnimation( FLOAT fDeltaTime );
    HRESULT UpdateAvatarBasis( );

    void SetAvatarAnim( AVATARANIM animation );
    void UpdateAvatarAnim( );

    void InitHUDFeedback();
    void ResetHUDFeedback();
    void AnimHUDFeedback( FLOAT fDeltaTime );
    
    void HideHUDPlayerIcon( GAMEPLAYERID id, BOOL fImmediate );
    void RenderHUDFeedback();

    void UpdateCameraPos( BOOL bTrackingPlayer );
    FLOAT CalcCameraElevationDegrees();

    void ChangeTiltMode();
    void UpdateTiltState();
    inline BOOL IsTilting() { return m_tiltState == TILT_STATE_BUSY; }
    inline BOOL IsTiltObjectDataValid();

    ATG::Timer                  m_Timer;
    ATG::Font                   m_Font;
    ATG::Help                   m_Help;
    BOOL                        m_bDrawHelp;

    FLOAT                       m_fCurTime;

    // View parameters
    XMMATRIX                    m_matView;
    XMMATRIX                    m_matProj; 
    XMMATRIX                    m_matWorld;
    XMVECTOR                    m_vCameraPos;
    XMVECTOR                    m_vAvatarPos;

    // Avatar color filter
    XMVECTOR                    m_vAvatarColorFilter;
    FLOAT                       m_fAvatarAnimStart;
    AVATARANIM                  m_runningAnim;

    // Animation data - updated every frame from the NUI skeleton
    XAVATAR_SKELETON_POSE_JOINT m_AvatarJointPose[ XAVATAR_MAX_SKELETON_JOINTS ]; 

    // Rendering surfaces and textures
    D3DSurface*                 m_pBackBuffer;
    D3DSurface*                 m_pDepthBuffer;
    D3DTexture*                 m_pFrontBuffer[2];
    DWORD                       m_dwCurFrontBuffer;

    LPDIRECT3DTEXTURE9              m_pTextureGrass;
    LPDIRECT3DTEXTURE9              m_pTextureSky;
    LPDIRECT3DTEXTURE9              m_pSweetSpotTarget;
    LPDIRECT3DVERTEXDECLARATION9    m_pBackgroundVertexDeclaration;
    LPDIRECT3DVERTEXSHADER9         m_pBackgroundVertexShader;
    LPDIRECT3DPIXELSHADER9          m_pBackgroundPixelShader;
    ATG::PackedResource             m_Resource;

    PlayspaceFrustum           m_playspace;

    PlayspaceHUDGlyph           m_HUDPlayerGlyphs[ GAME_PLAYER_COUNT ];
    PlayspaceHUDResources       m_HUDResources;
    DebugPlayspaceVerts         m_debugPlayspace;
    D3DCOLOR                    m_colPlayspaceFrustum[ DebugPlayspaceVerts::DBGPSFACE_COUNT ];
    BOOL                        m_bShowPlayspace;

    IXAvatarNuiMapper*          m_pAvatarNuiMapper;

    HANDLE                      m_hFrameEndEvent;
    NUI_SKELETON_FRAME          m_Skeleton;
    UINT                        m_iCurrentSkeletonIndex;

    HRESULT                     m_hrNuiResult;

    AvatarRenderer*		        m_pAvatarRenderer;
    XAVATAR_BODY_TYPE           m_AvatarBodyType;

    // Visualize the depth and color buffers
    CONST NUI_IMAGE_FRAME*      m_pImageFrame;
    CONST NUI_IMAGE_FRAME*      m_pDepthFrame;

    ATG::NuiVisualization       m_pip;
    HANDLE                      m_hImage;
    HANDLE                      m_hDepth;

    TILTMODE                    m_currentTiltMode;
    TILTSTATE                   m_tiltState;
    XOVERLAPPED                 m_ovTiltComplete;
    NUI_TILT_OBJECTS            m_tiltObjects;
};

//----------------------------------------------------------------------------------------------------------------------
// Name: Sample::IsTiltObjectDataValid
// Desc: Checks if the tilt object data (m_tiltObjects) is valid yet.
//----------------------------------------------------------------------------------------------------------------------
BOOL Sample::IsTiltObjectDataValid()
{
    // If we're in the middle of a tilt adjustment (TILT_STATE_BUSY), m_tiltObjects won't be valid as it could be
    // overwritten. It'll also be invalid if we've not called it yet (TILT_STATE_NOINFO). 
    return m_tiltState == TILT_STATE_IDLE;
}

#endif //SAMPLE_H_GUARD
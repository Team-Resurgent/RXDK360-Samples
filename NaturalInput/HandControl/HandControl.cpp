//--------------------------------------------------------------------------------------
// HandRefinement.cpp
//
// This sample demonstrates how to use the depth map to remove hand-jitter.
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include <xtl.h>
#include <xnamath.h>
#include <xffb.h>
#include <AtgApp.h>
#include <AtgFont.h>
#include <AtgInput.h>
#include <AtgMesh.h>
#include <AtgResource.h>
#include <AtgUtil.h>
#include <AtgHelp.h>
#include <AtgDebugDraw.h>
#include <AtgSimpleShaders.h>
#include <AtgResource.h>
#include <AtgSceneAll.h>
#include <AtgNuiJointFilter.h>
#include <AtgNuiVisualization.h>
#include <AtgNuiHandRefinement.h>
#include "CameraManager.h"
#include <ATGNuiRelativeCoordinates.h>
#include "HandOrientation.h"

static CONST INT g_iDebugVisualizeHandRefinementFrameCount = 5;

//--------------------------------------------------------------------------------------
// Callouts for labeling the gamepad on the help screen
//--------------------------------------------------------------------------------------
ATG::HELP_CALLOUT g_HelpCallouts[] =
{
    { ATG::HELP_BACK_BUTTON,  ATG::HELP_PLACEMENT_2, L"Display\nhelp" },
};
static const DWORD  NUM_HELP_CALLOUTS = sizeof( g_HelpCallouts ) / sizeof( g_HelpCallouts[0] );

// Simple struct to organize a 2d coord
struct DataPoint
{
  LONG iX;
  LONG iY;
};

//--------------------------------------------------------------------------------------
//  Name: CursorData
//  Desc: class used to convert the refinened hand position into an actualy pixel location
//--------------------------------------------------------------------------------------
class CursorData
{
public:
    CursorData() 
    {
        m_Data.iX = 0;
        m_Data.iY = 0;
        m_fOffset = XMFLOAT2( 0.0f, 0.0f );
        m_fScale = XMFLOAT2( 1.0f, 1.0f );
    };

    VOID Init( XMFLOAT2 fScale, XMFLOAT2 fOffset )
    {
        m_fOffset = fOffset;
        m_fScale = fScale;
        UpdateHandPosition( XMVectorSet( 0.0f, 0.0f, 0.0f, 0.0f ) ); 
    };

    DataPoint GetCurrentLocation() 
    {
        return m_Data;
    };

    VOID UpdateHandPosition ( XMVECTOR vHand )
    {
        XMFLOAT2 fHandVis;
        XMStoreFloat2( &fHandVis, vHand );
        fHandVis.x *= m_fScale.x;
        fHandVis.x += m_fOffset.x;
        fHandVis.y *= m_fScale.y ;    
        fHandVis.y +=  m_fOffset.y; 
        m_Data.iX = (LONG)fHandVis.x;
        m_Data.iY = (LONG)fHandVis.y;        
    };
    XMFLOAT2 m_fOffset;
    XMFLOAT2 m_fScale;

    ~CursorData() {};
    DataPoint m_Data;
};

//--------------------------------------------------------------------------------------
// Name: class Sample
// Desc: Main class to run this application. Most functionality is inherited
//       from the ATG::Application base class.
//--------------------------------------------------------------------------------------
class Sample : public ATG::Application
{
    ATG::Timer m_Timer;    // Timer
    ATG::Font m_Font;     // Font for drawing text
    ATG::Help m_Help;
    BOOL m_bDrawHelp;
    ATG::PackedResource m_Resource; // Bundled textures in a packed resource

    // Transform matrices
    XMMATRIX m_matView;
    XMMATRIX m_matProj;

    LPDIRECT3DTEXTURE9 m_pThumbsUp;

    CameraManager m_CameraManager;
    ATG::RefinementData m_RefinementData;
    ATG::OptionalRefinementData m_ExtraRefinementData;

    CursorData m_RefinedPositionLeft;
    CursorData m_RefinedPositionRight;
    XMFLOAT2 m_fThumbsUpOffset;

    // Natural Input data
    ATG::SpineRelativeCameraSpaceCoordinateSystem m_RefineCoord;
    HandOrientation m_HandOrientation;

    VOID DrawHandSprite( XMVECTOR vRefined, DataPoint HandPoint, HandOrientationData* pHandOrientation, BOOL bFlipX );

#ifdef DEBUG_VISUALIZE_HAND_PRINTS
    IDirect3DTexture9* m_pVisualizeLeftHand[ g_iDebugVisualizeHandRefinementFrameCount ];
    IDirect3DTexture9* m_pVisualizeRightHand[ g_iDebugVisualizeHandRefinementFrameCount ];
    INT m_iCurrentVisualizeNUIHand;
#endif

public:
    virtual HRESULT     Initialize();
    virtual HRESULT     Update();
    virtual HRESULT     Render();
};

//--------------------------------------------------------------------------------------
// Name: main()
// Desc: Entry point to the program
//--------------------------------------------------------------------------------------
VOID __cdecl main()
{
    Sample atgApp;
    ATG::GetVideoSettings( &atgApp.m_d3dpp.BackBufferWidth, &atgApp.m_d3dpp.BackBufferHeight );
    atgApp.m_d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_TWO;
    atgApp.Run();
}

//--------------------------------------------------------------------------------------
// Name: Initialize()
// Desc: Initialize app-dependent objects
//--------------------------------------------------------------------------------------
HRESULT Sample::Initialize()
{
    HRESULT hr;
    m_bDrawHelp = FALSE;
#ifdef DEBUG_VISUALIZE_HAND_PRINTS
    m_iCurrentVisualizeNUIHand = 0;
#endif

    m_RefinedPositionLeft.Init( XMFLOAT2( 1000.0f, 1000.0f ), XMFLOAT2( 320.0f, 100.0f ) );
    m_RefinedPositionRight.Init( XMFLOAT2( 1000.0f, 1000.0f ), XMFLOAT2( 960.0f, 100.0f ) );

    // Initialize simple shaders.
    ATG::SimpleShaders::Initialize( NULL, NULL );

    // Create the font
    if( FAILED( hr = m_Font.Create( "game:\\Media\\Fonts\\Arial_16.xpr" ) ) )
    {
        ATG_PrintError( "Couldn't create font\n" );
        return hr;
    }

    // Create the help
    if( FAILED( hr = m_Help.Create( "game:\\Media\\Help\\Help.xpr" ) ) )
    {
        ATG_PrintError( "Couldn't create help\n" );
        return hr;
    }

    // Confine text drawing to the title safe area
    m_Font.SetWindow( ATG::GetTitleSafeArea() );

    // Create the textures resource
    if( FAILED( hr = m_Resource.Create( "game:\\Media\\Resource.xpr" ) ) )
    {
        ATG_PrintError( "Couldn't create Resource.xpr\n" );
        return hr;
    }

    m_pThumbsUp = m_Resource.GetTexture("ThumbsUp");
    D3DSURFACE_DESC ThumbSurface;
    m_pThumbsUp->GetLevelDesc( 0, &ThumbSurface );

    m_fThumbsUpOffset.x = (FLOAT)ThumbSurface.Width / 2.0f;
    m_fThumbsUpOffset.y = (FLOAT)ThumbSurface.Height / 2.0f;


#ifdef DEBUG_VISUALIZE_HAND_PRINTS
    for ( int iIndex = 0; iIndex < g_iDebugVisualizeHandRefinementFrameCount; ++iIndex )
    {
        INT iSide = ATG::g_iNuiRefinementHalfKernelSize320x240 * 2;

        m_pd3dDevice->CreateTexture( 
            iSide, iSide, 1, 0,
            D3DFMT_LIN_X8R8G8B8, D3DPOOL_MANAGED, &m_pVisualizeLeftHand[iIndex], NULL );

        m_pd3dDevice->CreateTexture( 
            iSide, iSide, 1, 0,
            D3DFMT_LIN_X8R8G8B8, D3DPOOL_MANAGED, &m_pVisualizeRightHand[iIndex], NULL );
    }
#endif

    m_CameraManager.InitializeCamera( m_pd3dDevice );

    return S_OK;
}



//--------------------------------------------------------------------------------------
// Name: Update()
// Desc: Called once per frame, the call is the entry point for animating the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Update()
{
    // Get the elapsed time
    FLOAT fElapsedTime = ( FLOAT )m_Timer.GetElapsedTime();

    // Get the current gamepad state
    ATG::GAMEPAD* pGamepad = ATG::Input::GetMergedInput();

    // Toggle help
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_BACK )
        m_bDrawHelp = !m_bDrawHelp;

    // Update the natural input device
    BOOL bNewTrackedSkeletonReceived = m_CameraManager.CheckForNewSkeletonAndDepthMaps( fElapsedTime, NULL );
    if ( bNewTrackedSkeletonReceived )
    {

        PIXBeginNamedEvent( 0, "RefineHands" );

        ATG::RefineHands (
            m_CameraManager.GetDepthMap320x240(),
            m_CameraManager.GetDepthMap80x60(),
            m_CameraManager.GetTrackedSkeletonIndex(),
            m_CameraManager.GetSkeletonFrame(),
            ATG_REFINE_HANDS_FLAG_CALCULATE320x240MASK,
            &m_RefinementData,
            NULL,
            NULL,
            &m_ExtraRefinementData
        );
   
        PIXEndNamedEvent( );

        m_HandOrientation.Update(             
            m_CameraManager.GetDepthMap320x240(),
            m_CameraManager.GetTrackedSkeletonIndex(),
            m_CameraManager.GetSkeletonFrame(),
            &m_RefinementData,
            &m_ExtraRefinementData
        );
        
#ifdef DEBUG_VISUALIZE_HAND_PRINTS
        INT iSide = ATG::g_iNuiRefinementHalfKernelSize320x240 * 2;

        --m_iCurrentVisualizeNUIHand;
        m_iCurrentVisualizeNUIHand += g_iDebugVisualizeHandRefinementFrameCount; 
        m_iCurrentVisualizeNUIHand %= g_iDebugVisualizeHandRefinementFrameCount;
        
        D3DLOCKED_RECT RectRight;
        m_pVisualizeRightHand[ m_iCurrentVisualizeNUIHand ]->LockRect( 0, &RectRight, NULL, 0 );  
        DWORD *pTextureDataRight = (DWORD*)RectRight.pBits;
        INT iPitchRight = RectRight.Pitch / 4;

        D3DLOCKED_RECT RectLeft;
        m_pVisualizeLeftHand[ m_iCurrentVisualizeNUIHand ]->LockRect( 0, &RectLeft, NULL, 0 );  
        DWORD *pTextureDataLeft = (DWORD*)RectLeft.pBits;
        INT iPitchLeft = RectLeft.Pitch / 4;

        for ( INT iY = 0; iY < iSide; ++iY )
        {
            for ( INT iX = 0; iX < iSide; ++iX )
            {
                if ( m_ExtraRefinementData.m_RightHand.m_uHandDepthValues320x240[iY * 48 +iX] != 0 )
                {
                    pTextureDataRight[ iY * iPitchRight + iX] = 0xFF008F00;
                }
                else {
                    pTextureDataRight[ iY * iPitchRight + iX] = 0xff451289;
                }

                if ( m_ExtraRefinementData.m_LeftHand.m_uHandDepthValues320x240[iY * 48 +iX] != 0 )
                {
                    pTextureDataLeft[ iY * iPitchLeft + iX] = 0xFF008F00;
                }
                else {
                    pTextureDataLeft[ iY * iPitchLeft + iX] = 0xff451289;
                }
            }
        }
        m_pVisualizeRightHand[ m_iCurrentVisualizeNUIHand ]->UnlockRect( 0 );  
        m_pVisualizeLeftHand[ m_iCurrentVisualizeNUIHand ]->UnlockRect( 0 );  

#endif

        m_CameraManager.ReleaseDepthMaps();

        CONST XMVECTOR* pPositions = &m_CameraManager.GetTrackedSkeleton()->SkeletonPositions[0];
        static XMVECTOR PositionCopy[NUI_SKELETON_POSITION_COUNT];

        /// protottype refinement
        memcpy( &PositionCopy, pPositions, sizeof(XMVECTOR) * NUI_SKELETON_POSITION_COUNT );
        PositionCopy[NUI_SKELETON_POSITION_HAND_LEFT] =
            m_RefinementData.m_LeftHandData.m_vRefinedHand;
        PositionCopy[NUI_SKELETON_POSITION_HAND_RIGHT] =
            m_RefinementData.m_RightHandData.m_vRefinedHand;
        
        m_RefineCoord.Update( m_CameraManager.GetSkeletonFrame(), m_CameraManager.GetTrackedSkeletonIndex(), m_RefinementData.GetRefinedLeft(), m_RefinementData.GetRefinedRight() );

        m_RefinedPositionRight.UpdateHandPosition( m_RefineCoord.GetRightHandReletive() );
        m_RefinedPositionLeft.UpdateHandPosition( m_RefineCoord.GetLeftHandReletive() );

    }
    else
    {
        m_HandOrientation.Reset();
    }

    return S_OK;
}

//--------------------------------------------------------------------------------------
// Name: DrawHandSprite()
// Desc: Render the Sprite when a valid hand is detected.
//--------------------------------------------------------------------------------------
VOID Sample::DrawHandSprite( XMVECTOR vRefined, DataPoint HandPoint, HandOrientationData* pHandOrientation, BOOL bFlipX )
{

    FLOAT fScale =  0.25f + XMVectorGetZ( vRefined );
    XMFLOAT3 Corner1 = XMFLOAT3( -m_fThumbsUpOffset.x, -m_fThumbsUpOffset.y, 0.0f );
    XMFLOAT3 Corner2 = XMFLOAT3( m_fThumbsUpOffset.x , -m_fThumbsUpOffset.y, 0.0f );
    XMFLOAT3 Corner3 = XMFLOAT3( -m_fThumbsUpOffset.x, m_fThumbsUpOffset.y, 0.0f );
    
    XMFLOAT2 uvRepeat = XMFLOAT2( 1.0f, -1.0f );
    if ( bFlipX ) uvRepeat.x *= -1.0f;
    XMVECTOR vScale = XMVectorSet( fScale, fScale, 1.0f, 1.0f );

    XMVECTOR vRotationOrigin = XMVectorSet( 0, 0, 0.0f, 0.0f );
    XMMATRIX mOrtho = XMMatrixOrthographicOffCenterLH( 0.0f, 1280.0f, 0.0f, 720.0f, 1.0f, -1.0f );
    
    XMVECTOR vTranslation = XMVectorSet( (FLOAT)HandPoint.iX, 
        (FLOAT)HandPoint.iY, 0.0f, 0.0f );

    XMMATRIX mAffine = XMMatrixAffineTransformation2D( 
        vScale, vRotationOrigin, 
        -pHandOrientation->m_fOrientation, vTranslation );

    ATG::DebugDraw::SetViewProjection( mAffine * mOrtho );
    ATG::DebugDraw::DrawTexturedQuad( Corner1, Corner2, Corner3, uvRepeat, m_pThumbsUp );

}


//--------------------------------------------------------------------------------------
// Name: Render()
// Desc: Called once per frame, the call is the entry point for 3D rendering. This 
//       function sets up render states, clears the viewport, and renders the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Render()
{
    // Draw a gradient filled background black 
    ATG::RenderBackground( 0xff000022, 0xff213300 );

    // Show title, frame rate, and help
    m_Timer.MarkFrame();

    m_CameraManager.DisplayPIP();

#ifdef DEBUG_VISUALIZE_HAND_PRINTS

    PIXBeginNamedEvent( 0, "Render Hand Print" );
    D3DRECT DrawPos;

    static CONST INT iXBeginHandPrints = 730;
    static CONST INT iYBeginHandPrints = 100;
    static CONST INT iSpaceBetweenHandPrintRows = 100;
    for ( INT iX = 0; iX < g_iDebugVisualizeHandRefinementFrameCount; ++iX )
    {
        DrawPos.x1 = iXBeginHandPrints + iX * ATG::g_iNuiRefinementHalfKernelSize320x240 * 3;
        DrawPos.y1 = iYBeginHandPrints;
        DrawPos.x2 = iXBeginHandPrints + (iX+1) * ATG::g_iNuiRefinementHalfKernelSize320x240 * 3;
        DrawPos.y2 = iYBeginHandPrints + ATG::g_iNuiRefinementHalfKernelSize320x240 * 3;

        ATG::DebugDraw::DrawScreenSpaceTexturedRect( 
            DrawPos, m_pVisualizeRightHand[ ( m_iCurrentVisualizeNUIHand +(iX)) % g_iDebugVisualizeHandRefinementFrameCount ] );
        ATG::DebugDraw::DrawScreenSpaceRect( DrawPos, 1.0f, 0xFF7f7f7f );

        DrawPos.x1 = iXBeginHandPrints + iX * ATG::g_iNuiRefinementHalfKernelSize320x240 * 3;
        DrawPos.y1 = iYBeginHandPrints + iSpaceBetweenHandPrintRows;
        DrawPos.x2 = iXBeginHandPrints + (iX+1) * ATG::g_iNuiRefinementHalfKernelSize320x240 * 3;
        DrawPos.y2 = iYBeginHandPrints + iSpaceBetweenHandPrintRows + ATG::g_iNuiRefinementHalfKernelSize320x240 * 3;

        ATG::DebugDraw::DrawScreenSpaceTexturedRect( 
            DrawPos, m_pVisualizeLeftHand[ ( m_iCurrentVisualizeNUIHand +(iX)) % g_iDebugVisualizeHandRefinementFrameCount ] );
        ATG::DebugDraw::DrawScreenSpaceRect( DrawPos, 1.0f, 0xFF7f7f7f );

    }
    PIXEndNamedEvent( );

#endif

    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_SRCBLEND, D3DBLEND_SRCALPHA );
    m_pd3dDevice->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA );

    if ( m_HandOrientation.GetLeftData()->m_eHandState == HAND_STATE_ORIENTATION_TRACKED ) 
    {
        DrawHandSprite( m_RefineCoord.GetLeftHandReletive(), m_RefinedPositionLeft.GetCurrentLocation(),  
            m_HandOrientation.GetLeftData(), TRUE );
    }
    if ( m_HandOrientation.GetRightData()->m_eHandState == HAND_STATE_ORIENTATION_TRACKED ) 
    {
        DrawHandSprite( m_RefineCoord.GetRightHandReletive(), m_RefinedPositionRight.GetCurrentLocation(),  
            m_HandOrientation.GetRightData(), FALSE );
    }

    // Render the HUD
    m_Font.Begin();
    m_Font.SetScaleFactors( 1.2f, 1.2f );
    m_Font.DrawText( 0.0f, 0.0f, 0xffffffff, L"Hand Control " );
    m_Font.DrawText( 230.0f, 0.0f, 0xffffffff, m_Timer.GetFrameRate() );

    HandOrientationData* pRightHand = m_HandOrientation.GetRightData();
    HandOrientationData* pLeftHand = m_HandOrientation.GetLeftData();

    if ( pRightHand->m_eHandState == HAND_STATE_HAND_TOO_LOW )
    {
        m_Font.SetScaleFactors( 2.2f, 2.2f );
        m_Font.DrawText( 700.0f, 350.0f, 0xFFFF0000, L"Hand pointed \ntoo low" );    
    }
    else if ( pRightHand->m_eHandState == HAND_STATE_HAND_TOO_HIGH )
    {
        m_Font.SetScaleFactors( 2.2f, 2.2f );
        m_Font.DrawText( 700.0f, 350.0f, 0xFFFF0000, L"Hand pointed \ntoo high" );    
    }
    else if ( pRightHand->m_eHandState == HAND_STATE_HAND_TOO_FAR_LEFT )
    {
        m_Font.SetScaleFactors( 2.2f, 2.2f );
        m_Font.DrawText( 700.0f, 350.0f, 0xFFFF0000, L"Hand pointed \ntoo far left" );    
    }
    else if ( pRightHand->m_eHandState == HAND_STATE_HAND_TOO_FAR_RIGHT )
    {
        m_Font.SetScaleFactors( 2.2f, 2.2f );
        m_Font.DrawText( 700.0f, 350.0f, 0xFFFF0000, L"Hand too pointed \ntoo far right" );    
    }
    else if ( pRightHand->m_eHandState == HAND_STATE_ORIENTATION_TRACKED &&
        pRightHand->m_fOrientation180 < -XM_PIDIV4 || pRightHand->m_fOrientation180 > XM_PIDIV4 )
    {
        m_Font.SetScaleFactors( 2.2f, 2.2f );
        m_Font.DrawText( 800.0f, 550.0f, 0xFFFF00FF, L"Thumb Up!" );
    }
    
    if ( pLeftHand->m_eHandState == HAND_STATE_HAND_TOO_LOW )
    {
        m_Font.SetScaleFactors( 2.2f, 2.2f );
        m_Font.DrawText( 50.0f, 350.0f, 0xFFFF0000, L"Hand pointed \ntoo low" );    
    }
    else if ( pLeftHand->m_eHandState == HAND_STATE_HAND_TOO_HIGH )
    {
        m_Font.SetScaleFactors( 2.2f, 2.2f );
        m_Font.DrawText( 50.0f, 350.0f, 0xFFFF0000, L"Hand pointed \ntoo high" );    
    }
    else if ( pLeftHand->m_eHandState == HAND_STATE_HAND_TOO_FAR_LEFT )
    {
        m_Font.SetScaleFactors( 2.2f, 2.2f );
        m_Font.DrawText( 50.0f, 350.0f, 0xFFFF0000, L"Hand pointed \ntoo far left" );    
    }
    else if ( pLeftHand->m_eHandState == HAND_STATE_HAND_TOO_FAR_RIGHT )
    {
        m_Font.SetScaleFactors( 2.2f, 2.2f );
        m_Font.DrawText( 50.0f, 350.0f, 0xFFFF0000, L"Hand pointed \ntoo far Right" );    
    }
    else if ( pLeftHand->m_eHandState == HAND_STATE_ORIENTATION_TRACKED && 
        pLeftHand->m_fOrientation180 < -XM_PIDIV4 || pLeftHand->m_fOrientation180 > XM_PIDIV4 )
    {
        m_Font.SetScaleFactors( 2.2f, 2.2f );
        m_Font.DrawText( 0.0f, 550.0f, 0xFFFF00FF, L"Thumb Up!" );
    }

    m_Font.End();

    if( m_bDrawHelp )
        m_Help.Render( &m_Font, g_HelpCallouts, NUM_HELP_CALLOUTS );
    
    
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}



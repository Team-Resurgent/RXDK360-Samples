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

BOOL g_bVisualizeRight = TRUE;
static CONST INT g_iTrailHistoryCount = 30;
static CONST INT g_iCursorSize = 70;
static CONST INT g_iCursorSizeDiv2 = g_iCursorSize / 2;
static CONST FLOAT g_fHandXOffset = 40.0f;

static CONST INT g_iDebugVisualizeHandRefinementFrameCount = 36;

//--------------------------------------------------------------------------------------
// Callouts for labeling the gamepad on the help screen
//--------------------------------------------------------------------------------------
ATG::HELP_CALLOUT g_HelpCallouts[] =
{
    { ATG::HELP_BACK_BUTTON,  ATG::HELP_PLACEMENT_2, L"Display\nhelp" },
};
static const DWORD  NUM_HELP_CALLOUTS = sizeof( g_HelpCallouts ) / sizeof( g_HelpCallouts[0] );

struct HistoryPoint
{
  LONG iX;
  LONG iY;
};

class CursorData
{
public:
    CursorData()
    {
        iCurrent = -1;
        for ( int iIndex=0; iIndex < g_iTrailHistoryCount; ++iIndex )
        {
            m_History[iIndex].iX = 640; 
            m_History[iIndex].iY = 360; 
        }
    };

    VOID UpdateHandPosition ( XMVECTOR vHand )
    {
        BOOL bFirst = FALSE;
        if ( iCurrent == -1 )
        {
            bFirst = TRUE;
        }
        ++iCurrent;
        iCurrent %= g_iTrailHistoryCount;

        XMFLOAT2 fHandVis;
        XMStoreFloat2( &fHandVis, vHand );

        fHandVis.x *= 1000.0f;
        fHandVis.x -= g_iCursorSizeDiv2;
        fHandVis.x += 720.0f;
        fHandVis.y *= 1000.0f;    
        fHandVis.y +=  360.0f;  
        fHandVis.y -= ( g_iCursorSizeDiv2 );
        // Flip coordinate system toconvert to screen space 
        fHandVis.y = 720.0f - fHandVis.y;

        m_Position.x1 = ( LONG )( fHandVis.x );
        m_Position.x2 = ( ( LONG )fHandVis.x + g_iCursorSize );
        m_Position.y1 = ( LONG )( fHandVis.y );
        m_Position.y2 = ( ( LONG )fHandVis.y + g_iCursorSize );
        m_History[iCurrent].iX = (LONG)fHandVis.x + g_iCursorSizeDiv2;
        m_History[iCurrent].iY = (LONG)fHandVis.y + g_iCursorSizeDiv2;
        if ( bFirst )
        {
            for ( INT iIndex=0; iIndex < g_iTrailHistoryCount; ++iIndex )
            {
                m_History[iIndex].iX = (LONG)fHandVis.x + g_iCursorSizeDiv2;
                m_History[iIndex].iY = (LONG)fHandVis.y + g_iCursorSizeDiv2;
            }
        }
        
    }

    ~CursorData() {};
    D3DRECT m_Position;   
    HistoryPoint m_History[ g_iTrailHistoryCount ];
    INT iCurrent;
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

    LPDIRECT3DTEXTURE9 m_pSmile1;
    LPDIRECT3DTEXTURE9 m_pSmile2;

    CameraManager m_CameraManager;
    ATG::RefinementData m_RefinementData;


    CursorData m_UnFilteredPositionLeft;
    CursorData m_UnFilteredPositionRight;

    CursorData m_RefinedPositionLeft;
    CursorData m_RefinedPositionRight;
    USHORT* pMemoryFor80x60MaxMap;

    // Natural Input data

    ATG::SpineRelativeCameraSpaceCoordinateSystem m_UnfilteredCoord;
    ATG::SpineRelativeCameraSpaceCoordinateSystem m_RefineCoord;

#ifdef DEBUG_COMPUTE_AND_SHOW_VISUALIZATION
    IDirect3DTexture9* m_pVisualizeHands[ g_iDebugVisualizeHandRefinementFrameCount ];
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
INT __cdecl main()
{
    Sample atgApp;
    ATG::GetVideoSettings( &atgApp.m_d3dpp.BackBufferWidth, &atgApp.m_d3dpp.BackBufferHeight );
    atgApp.m_d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_ONE;
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
#ifdef DEBUG_COMPUTE_AND_SHOW_VISUALIZATION
    m_iCurrentVisualizeNUIHand = 0;
#endif
    XMVECTOR vInitPos = XMVectorSet( 0.0f, 0.0f, 0.0f, 0.0f );
    m_UnFilteredPositionLeft.UpdateHandPosition( vInitPos );
    m_UnFilteredPositionRight.UpdateHandPosition( vInitPos );
    m_RefinedPositionLeft.UpdateHandPosition( vInitPos );
    m_RefinedPositionRight.UpdateHandPosition( vInitPos );

    pMemoryFor80x60MaxMap = (USHORT*)XMemAlloc( 80 * 60 * 2, 
        MAKE_XALLOC_ATTRIBUTES( 0, FALSE, TRUE, TRUE, 0, XALLOC_ALIGNMENT_16, XALLOC_MEMPROTECT_READWRITE, FALSE, XALLOC_MEMTYPE_PHYSICAL ) );

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

    m_pSmile1 = m_Resource.GetTexture("Smile1");
    m_pSmile2 = m_Resource.GetTexture("Smile2");


#ifdef DEBUG_COMPUTE_AND_SHOW_VISUALIZATION
    for ( int iIndex = 0; iIndex < g_iDebugVisualizeHandRefinementFrameCount; ++iIndex )
    {
        INT iSide = ATG::g_iNuiRefinementHalfKernelSize320x240 * 2;

        m_pd3dDevice->CreateTexture( 
            iSide, iSide, 1, 0,
            D3DFMT_LIN_X8R8G8B8, D3DPOOL_MANAGED, &m_pVisualizeHands[iIndex], NULL );
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
    
    BOOL bNewTrackedSkeletonReceived ;
    bNewTrackedSkeletonReceived = m_CameraManager.CheckForNewSkeletonAndDepthMaps( fElapsedTime, NULL );
    if ( bNewTrackedSkeletonReceived )
    {

        LARGE_INTEGER startTime;
        LARGE_INTEGER stopTime;
        LARGE_INTEGER frequency;
        QueryPerformanceFrequency(&frequency);
        QueryPerformanceCounter(&startTime);

        PIXBeginNamedEvent( 0, "RefineHands" );
        
        ATG::RefineHands (
            m_CameraManager.GetDepthMap320x240(),
            m_CameraManager.GetDepthMap80x60(),
            m_CameraManager.GetTrackedSkeletonIndex(),
            m_CameraManager.GetSkeletonFrame(),
            0,
            &m_RefinementData,
            NULL,
            NULL,
            NULL
        );
   
        PIXEndNamedEvent( );
        QueryPerformanceCounter( &stopTime );

        
#ifdef DEBUG_COMPUTE_AND_SHOW_VISUALIZATION


        ATG::HandSpecificData* pHandSpecificData;
        if ( g_bVisualizeRight )
        {
            pHandSpecificData = &m_RefinementData.m_RightHandData;
        }
        else 
        {
            pHandSpecificData = &m_RefinementData.m_LeftHandData;
        }


        m_iCurrentVisualizeNUIHand += (g_iDebugVisualizeHandRefinementFrameCount-1); 
        m_iCurrentVisualizeNUIHand %= g_iDebugVisualizeHandRefinementFrameCount;
        
        D3DLOCKED_RECT rect;
        m_pVisualizeHands[ m_iCurrentVisualizeNUIHand ]->LockRect( 0, &rect, NULL, 0 );  
        DWORD *pTextureData = (DWORD*)rect.pBits;
        INT iPitch = rect.Pitch / 4;
        INT iSide = ATG::g_iNuiRefinementHalfKernelSize320x240 * 2;
        ATG::VisualizeHandFramesData *pData = NULL;
        if ( g_bVisualizeRight )
        {
            pData = &m_RefinementData.m_RightHandData.m_VisualizeHandFramesData;
        }
        else 
        {
            pData = &m_RefinementData.m_LeftHandData.m_VisualizeHandFramesData;
        }
        
        for ( INT iY = 0; iY < iSide; ++iY )
        {
            for ( INT iX = 0; iX < iSide; ++iX )
            {
                if ( pData->m_FrameData[iY][iX] == 1 )
                {
                    pTextureData[ iY * iPitch + iX] = 0xFF008F00;
                }
                else if ( pData->m_FrameData[iY][iX] == 0 )
                {
                    pTextureData[ iY * iPitch + iX] = 0xFFFFAA00;
                }
                else {
                    if ( pHandSpecificData->m_bRevertedToRawData )
                    {
                        pTextureData[ iY * iPitch + iX] = 0xffFF0000;
                    }
                    else
                    {
                        pTextureData[ iY * iPitch + iX] = 0xff451289;
                    }
                }
            }
        }
        m_pVisualizeHands[ m_iCurrentVisualizeNUIHand ]->UnlockRect( 0 );  

#endif

        m_CameraManager.ReleaseDepthMaps();

        CONST XMVECTOR* pPositions = &m_CameraManager.GetTrackedSkeleton()->SkeletonPositions[0];
        m_UnfilteredCoord.Update( m_CameraManager.GetSkeletonFrame(), m_CameraManager.GetTrackedSkeletonIndex(),
            pPositions[NUI_SKELETON_POSITION_HAND_LEFT], pPositions[NUI_SKELETON_POSITION_HAND_RIGHT]);

        m_UnFilteredPositionLeft.UpdateHandPosition(  m_UnfilteredCoord.GetLeftHandReletive() );
        m_UnFilteredPositionRight.UpdateHandPosition(  m_UnfilteredCoord.GetRightHandReletive() ); 

        
        m_RefineCoord.Update( m_CameraManager.GetSkeletonFrame(), m_CameraManager.GetTrackedSkeletonIndex(),
            m_RefinementData.GetRefinedLeft(), m_RefinementData.GetRefinedRight() );

        m_RefinedPositionLeft.UpdateHandPosition( m_RefineCoord.GetLeftHandReletive() );

        m_RefinedPositionRight.UpdateHandPosition( m_RefineCoord.GetRightHandReletive() );

    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Render()
// Desc: Called once per frame, the call is the entry point for 3D rendering. This 
//       function sets up render states, clears the viewport, and renders the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Render()
{
    // Draw a gradient filled background black 
    ATG::RenderBackground( 0xff000055, 0xff000055 );

    // Show title, frame rate, and help
    m_Timer.MarkFrame();

    m_CameraManager.DisplayPIP();


    XMFLOAT2 fCenterOf320x240;
    ATG::INT2Range RangeX;
    ATG::INT2Range RangeY;
 
    ATG::HandSpecificData* pHandSpecificData;
    if ( g_bVisualizeRight )
    {
        pHandSpecificData = &m_RefinementData.m_RightHandData;
        fCenterOf320x240 = XMFLOAT2( 
            (FLOAT)m_RefinementData.m_RightHandData.m_INT3ScreenSpaceHandRefined80x60.iX * 4,
            (FLOAT)m_RefinementData.m_RightHandData.m_INT3ScreenSpaceHandRefined80x60.iY * 4 );
    }
    else 
    {
        pHandSpecificData = &m_RefinementData.m_LeftHandData;
        fCenterOf320x240 = XMFLOAT2( 
            (FLOAT)m_RefinementData.m_LeftHandData.m_INT3ScreenSpaceHandRefined80x60.iX * 4,
            (FLOAT)m_RefinementData.m_LeftHandData.m_INT3ScreenSpaceHandRefined80x60.iY * 4 );
    }
    RangeX.iMin = max( 0, min( 319, (INT)fCenterOf320x240.x - 24 ) );
    RangeX.iMax = max( 0, min( 319, (INT)fCenterOf320x240.x + 24 ) );
    RangeY.iMin = max( 0, min( 239, (INT)fCenterOf320x240.y - 24 ) );
    RangeY.iMax = max( 0, min( 239, (INT)fCenterOf320x240.y + 24 ) );

    fCenterOf320x240.x /= 320.0f;
    fCenterOf320x240.y /= 240.0f;

    XMFLOAT2 fCenterOf80x60 = XMFLOAT2( 
        (FLOAT)pHandSpecificData->m_INT3ScreenSpaceHandRefined80x60.iX, 
        (FLOAT)pHandSpecificData->m_INT3ScreenSpaceHandRefined80x60.iY );

    fCenterOf80x60.x /= 80.0f;
    fCenterOf80x60.y /= 60.0f;
    fCenterOf80x60.x += 1.0f / 160.0f; // center it
    fCenterOf80x60.y += 1.0f / 120.0f;

    D3DRECT handCentered;
    handCentered.x1 = 400;
    handCentered.x2 = 400 + 160;
    handCentered.y1 = 100;
    handCentered.y2 = 100 + 160;
    XMFLOAT2 xRange;
    XMFLOAT2 yRange;

    if ( fCenterOf80x60.x == 0 && fCenterOf80x60.y == 0.0f ) 
    {
        xRange = XMFLOAT2( 0.5f - 8.0f / 80.0f, 0.5f + 8.0f / 80.0f );
        yRange = XMFLOAT2( 0.5f - 8.0f / 60.0f , 0.5f + 8.0f / 60.0f );
    }
    else 
    {
        xRange = XMFLOAT2( fCenterOf80x60.x - 8.0f / 80.0f, fCenterOf80x60.x + 8.0f / 80.0f );
        yRange = XMFLOAT2( fCenterOf80x60.y - 8.0f / 60.0f, fCenterOf80x60.y + 8.0f / 60.0f );    
    }

    ATG::DebugDraw::DrawScreenSpaceTexturedRectPatchPointSampled( 
        handCentered, XMFLOAT2( xRange.x, yRange.x ), XMFLOAT2( xRange.y, yRange.x ), 
        XMFLOAT2( xRange.x, yRange.y ), m_CameraManager.GetDepthVis80x60Texture() ); 

    handCentered.x1 = 400;
    handCentered.x2 = 400 + 320;
    handCentered.y1 = 270;
    handCentered.y2 = 270 + 320;


    ATG::DebugDraw::DrawScreenSpaceTexturedRectPatchPointSampled( handCentered, XMFLOAT2( xRange.x, yRange.x ), XMFLOAT2( xRange.y, yRange.x ), 
       XMFLOAT2( xRange.x, yRange.y ), m_CameraManager.GetDepthVis320x240Texture() ); 

    PIXBeginNamedEvent( 0, "Render Search Area" );

    XMFLOAT2 slot1, slot2;

    // 3.0 added to compensate for 6.0 width.
    // add 2.0 to account for moving between 80x60 and 320x240
    ATG::DebugDraw::DrawScreenSpaceRect( 
        XMFLOAT2( (FLOAT)pHandSpecificData->m_INT3ScreenSpaceHandRaw80x60.iX * 4.0f + 2.0f - 1.0f + 70.0f, 
                  (FLOAT)pHandSpecificData->m_INT3ScreenSpaceHandRaw80x60.iY  * 4.0f + 2.0f - 1.0f + 150.0f ), 
        XMFLOAT2(2.0f, 2.0f), 0, 0xFF00FFFF );

#ifdef DEBUG_COMPUTE_AND_SHOW_VISUALIZATION
    ATG::DebugDraw::DrawScreenSpaceRect( 
        XMFLOAT2( (FLOAT)pHandSpecificData->m_INT3ScreenSpaceHandRaw80x60.iX * 4.0f + 2.0f - 1.0f + 70.0f, 
                  (FLOAT)pHandSpecificData->m_INT3ScreenSpaceHandRaw80x60.iY  * 4.0f + 2.0f - 1.0f + 150.0f ), 
        XMFLOAT2(2.0f, 2.0f), 0, 0xFF000000 );
    ATG::DebugDraw::DrawScreenSpaceRect( 
        XMFLOAT2( (FLOAT)pHandSpecificData->m_INT3ScreenSpaceHandJumpOnArm80x60.iX * 4.0f + 2.0f - 1.0f + 70.0f, 
                  (FLOAT)pHandSpecificData->m_INT3ScreenSpaceHandJumpOnArm80x60.iY  * 4.0f + 2.0f - 1.0f + 150.0f ), 
        XMFLOAT2(2.0f, 2.0f), 0, 0xFF0000FF );
    ATG::DebugDraw::DrawScreenSpaceRect( 
        XMFLOAT2( (FLOAT)pHandSpecificData->m_INT3ScreenSpaceHandWalkToEndofArm80x60.iX * 4.0f + 2.0f - 1.0f + 70.0f, 
                  (FLOAT)pHandSpecificData->m_INT3ScreenSpaceHandWalkToEndofArm80x60.iY  * 4.0f + 2.0f - 1.0f + 150.0f ), 
        XMFLOAT2(2.0f, 2.0f), 0, 0xFF7f7f7f );
    ATG::DebugDraw::DrawScreenSpaceRect( 
        XMFLOAT2( (FLOAT)pHandSpecificData->m_INT3ScreenSpaceHandRefined80x60.iX * 4.0f -1 + 2.0f + 70.0f, 
                  (FLOAT)pHandSpecificData->m_INT3ScreenSpaceHandRefined80x60.iY * 4.0f -1 + 2.0f + 150.0f ), 
        XMFLOAT2(2.0f, 2.0f), 0, 0xFFFFFFFF );
#endif
    
    

    XMFLOAT2 vOrigin = XMFLOAT2( 560.0f, 430.0f );
    // * 4 because our blow up image is blown up by 4 pixels, 
    // / 2 because we're finding 1/2 width
    INT iHalfWidth = ( RangeX.iMax - RangeX.iMin ) * 4 / 2;  // multiply by 4 because this is a 4x zoom  
    INT iHalfHeight = ( RangeY.iMax - RangeY.iMin ) * 4 / 2;  
    ATG::DebugDraw::DrawScreenSpaceLine( 
        XMFLOAT2( vOrigin.x - (FLOAT)iHalfWidth, vOrigin.y - (FLOAT)iHalfHeight ),
        XMFLOAT2( vOrigin.x + (FLOAT)iHalfWidth, vOrigin.y - (FLOAT)iHalfHeight ), 0xFFFFFFFF, 3 ); 
    ATG::DebugDraw::DrawScreenSpaceLine( 
        XMFLOAT2( vOrigin.x + (FLOAT)iHalfWidth, vOrigin.y - (FLOAT)iHalfHeight ),
        XMFLOAT2( vOrigin.x + (FLOAT)iHalfWidth, vOrigin.y + (FLOAT)iHalfHeight ), 0xFFFFFFFF, 3 ); 
    ATG::DebugDraw::DrawScreenSpaceLine( 
        XMFLOAT2( vOrigin.x + (FLOAT)iHalfWidth, vOrigin.y + (FLOAT)iHalfHeight ),
        XMFLOAT2( vOrigin.x - (FLOAT)iHalfWidth, vOrigin.y + (FLOAT)iHalfHeight ), 0xFFFFFFFF, 3 ); 
    ATG::DebugDraw::DrawScreenSpaceLine( 
        XMFLOAT2( vOrigin.x - (FLOAT)iHalfWidth, vOrigin.y + (FLOAT)iHalfHeight ),
        XMFLOAT2( vOrigin.x - (FLOAT)iHalfWidth, vOrigin.y - (FLOAT)iHalfHeight ), 0xFFFFFFFF, 3 ); 

    PIXEndNamedEvent();

#ifdef DEBUG_COMPUTE_AND_SHOW_VISUALIZATION

    PIXBeginNamedEvent( 0, "Render Hand Print" );
    INT iHandMaskSize = (INT) sqrtf ( (FLOAT)g_iDebugVisualizeHandRefinementFrameCount );

    D3DRECT DrawPos;
    for ( INT iY = 0; iY < iHandMaskSize; ++iY )
    {
        for ( INT iX = 0; iX < iHandMaskSize; ++iX )
        {
            DrawPos.x1 = 730 + iX * ATG::g_iNuiRefinementHalfKernelSize320x240 * 3;
            DrawPos.y1 = 270 + iY * ATG::g_iNuiRefinementHalfKernelSize320x240 * 3;
            DrawPos.x2 = 730 + (iX+1) * ATG::g_iNuiRefinementHalfKernelSize320x240 * 3;
            DrawPos.y2 = 270 + (iY+1) * ATG::g_iNuiRefinementHalfKernelSize320x240 * 3;

            ATG::DebugDraw::DrawScreenSpaceTexturedRectPatchPointSampled( 
                DrawPos, XMFLOAT2( 0.0f,0.0f), XMFLOAT2( 1.0f, 0.0f), XMFLOAT2(0.0f, 1.0f),
                m_pVisualizeHands[ ( m_iCurrentVisualizeNUIHand +(iY*iHandMaskSize+iX)) % g_iDebugVisualizeHandRefinementFrameCount]
                );
            ATG::DebugDraw::DrawScreenSpaceRect( DrawPos, 1.0f, 0xFF7f7f7f );
        }
    }
    PIXEndNamedEvent( );

#endif

    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_SRCBLEND, D3DBLEND_SRCALPHA );
    m_pd3dDevice->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA );

    D3DRECT KeyRect;
    KeyRect.x1 = 970;
    KeyRect.x2 = 1010;
    KeyRect.y1 = 120;
    KeyRect.y2 = 160;
    
    ATG::DebugDraw::DrawScreenSpaceTexturedRect( KeyRect, m_pSmile1 ); 
    KeyRect.y1 += 50;
    KeyRect.y2 += 50;
    ATG::DebugDraw::DrawScreenSpaceTexturedRect( KeyRect, m_pSmile2 ); 
    
    if ( g_bVisualizeRight )
    {
        ATG::DebugDraw::DrawScreenSpaceTexturedRect( m_UnFilteredPositionRight.m_Position, m_pSmile1 ); 
        ATG::DebugDraw::DrawScreenSpaceTexturedRect( m_RefinedPositionRight.m_Position, m_pSmile2 ); 
    }
    else 
    {
        ATG::DebugDraw::DrawScreenSpaceTexturedRect( m_UnFilteredPositionLeft.m_Position, m_pSmile1 ); 
        ATG::DebugDraw::DrawScreenSpaceTexturedRect( m_RefinedPositionLeft.m_Position, m_pSmile2 );
    }

    PIXBeginNamedEvent( 0, "Render Lines" );
    for ( INT iIndex = m_UnFilteredPositionLeft.iCurrent + 1; iIndex < m_UnFilteredPositionRight.iCurrent + g_iTrailHistoryCount; ++iIndex )
    {
        if ( g_bVisualizeRight )
        {
            HistoryPoint p1 = m_UnFilteredPositionRight.m_History[ iIndex % g_iTrailHistoryCount ];
            HistoryPoint p2 = m_UnFilteredPositionRight.m_History[( iIndex + 1 ) % g_iTrailHistoryCount ] ;
            ATG::DebugDraw::DrawScreenSpaceLine( XMFLOAT2( (FLOAT)p1.iX, (FLOAT)p1.iY ), XMFLOAT2( (FLOAT)p2.iX, (FLOAT)p2.iY ), 0xFFFF0000, 5 );

            p1 = m_RefinedPositionRight.m_History[ iIndex % g_iTrailHistoryCount ];
            p2 = m_RefinedPositionRight.m_History[( iIndex + 1 ) % g_iTrailHistoryCount ] ;
            ATG::DebugDraw::DrawScreenSpaceLine( XMFLOAT2( (FLOAT)p1.iX, (FLOAT)p1.iY  ), XMFLOAT2( (FLOAT)p2.iX, (FLOAT)p2.iY ), 0xFFFFFFFF, 5 );
        }
        else 
        {
            HistoryPoint p1 = m_UnFilteredPositionLeft.m_History[ iIndex % g_iTrailHistoryCount ];
            HistoryPoint p2 = m_UnFilteredPositionLeft.m_History[( iIndex + 1 ) % g_iTrailHistoryCount ] ;
            ATG::DebugDraw::DrawScreenSpaceLine( XMFLOAT2( (FLOAT)p1.iX, (FLOAT)p1.iY ), XMFLOAT2( (FLOAT)p2.iX, (FLOAT)p2.iY ), 0xFFFF0000, 5 );

            p1 = m_RefinedPositionLeft.m_History[ iIndex % g_iTrailHistoryCount ];
            p2 = m_RefinedPositionLeft.m_History[( iIndex + 1 ) % g_iTrailHistoryCount ] ;
            ATG::DebugDraw::DrawScreenSpaceLine( XMFLOAT2( (FLOAT)p1.iX, (FLOAT)p1.iY  ), XMFLOAT2( (FLOAT)p2.iX, (FLOAT)p2.iY ), 0xFFFFFFFF, 5 );
        }
    }
    PIXEndNamedEvent( );

    static CONST FLOAT drawWidth = 320.0f;
    static CONST FLOAT drawHeight = 240.0f;
    static CONST FLOAT drawX = 45.0f;
    static CONST FLOAT drawY = 45.0f;

    // Render the HUD
    m_Font.Begin();
    m_Font.SetScaleFactors( 1.2f, 1.2f );
    m_Font.DrawText( 0.0f, 0.0f, 0xffffffff, L"Hand Refinement" );
    m_Font.SetScaleFactors( 1.0f, 1.0f );
    m_Font.DrawText( 900.0f, 55.0f, 0xffffffff, L"Unfiltered Hands" );    
    m_Font.DrawText( 900.0f, 105.0f, 0xffffffff, L"Refined Hands" );    
    m_Font.DrawText( 0.0f, 0.0f, 0xffffff00, m_Timer.GetFrameRate(), ATGFONT_RIGHT );
   
    if ( pHandSpecificData->m_bRevertedToRawData )
    {
        m_Font.DrawText( 600.0f, 0.0f, 0xFFFF0000, L"Reverting to Raw Data " );
    }

    m_Font.End();

    if( m_bDrawHelp )
    {
        m_Help.Render( &m_Font, g_HelpCallouts, NUM_HELP_CALLOUTS );
    }
    

    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );
    
    DWORD fence = m_pd3dDevice->InsertFence();
    m_pd3dDevice->BlockOnFence( fence );

    return S_OK;
}



//--------------------------------------------------------------------------------------
// GestureGame.cpp
//
// The sample illustrates how to use the IXCamMotionEgnine API to use the Xbox 360
// camera to detect motion. Motion detection works by taking the absolute difference
// of two consecutive frames. After this difference is thresholded and optionally
// persisted for a short amount of time, it can be used to detect motion in rectangular,
// circular or arbitrary regions. 
// 
// The sample is a simple game where a moving ball is controlled by motion. The ball
// is defined as a circular region. As the region is touched, the resulting direction
// vector dictates where the ball goes. 
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include <xtl.h>
#include <xboxmath.h>
#include <xgraphics.h>
#include <xcam.h>
#include <AtgApp.h>
#include <AtgFont.h>
#include <AtgHelp.h>
#include <AtgInput.h>
#include <AtgResource.h>
#include <AtgUtil.h>
#include <AtgDebugDraw.h>
#include <AtgSimpleShaders.h>


//--------------------------------------------------------------------------------------
// Game structs
//--------------------------------------------------------------------------------------
struct StaticObject
{
    D3DPOINT Position;
    BOOL bVisible;
};

struct BallObject
{
    FLOAT fPosX;
    FLOAT fPosY;
    FLOAT fVelX;
    FLOAT fVelY;
};

//--------------------------------------------------------------------------------------
// Callouts for labelling the gamepad on the help screen
//--------------------------------------------------------------------------------------
ATG::HELP_CALLOUT g_HelpCallouts[] =
{
    { ATG::HELP_BACK_BUTTON,  ATG::HELP_PLACEMENT_2, L"Display\nhelp" },
    { ATG::HELP_A_BUTTON, ATG::HELP_PLACEMENT_1, L"New game" },
    { ATG::HELP_DPAD, ATG::HELP_PLACEMENT_2, L"Change motion\nthreshold" },
};
static const DWORD      NUM_HELP_CALLOUTS = sizeof( g_HelpCallouts ) / sizeof( g_HelpCallouts[ 0 ] );

//--------------------------------------------------------------------------------------
// Camera setttings
//--------------------------------------------------------------------------------------
const DWORD             g_dwCameraImageWidth = 640;
const DWORD             g_dwCameraImageHeight = 480;
const XCAMRESOLUTION    g_CameraResolution = XCAMRESOLUTION_640x480;

//--------------------------------------------------------------------------------------
// Game constants
//--------------------------------------------------------------------------------------
const DWORD             g_dwMaxPrizes = 8;
const DWORD             g_dwMaxTraps = 4;
const DWORD             g_dwObjectRadius = 30;

//--------------------------------------------------------------------------------------
// Name: class Sample
// Desc: Main class to run this application. Most functionality is inherited
//       from the ATG::Application base class.
//--------------------------------------------------------------------------------------
class Sample : public ATG::Application
{
    ATG::Timer m_Timer;
    ATG::Font m_Font;
    ATG::PackedResource m_Resource;
    ATG::Help m_Help;
    BOOL m_bDrawHelp;
    BOOL m_bCameraAttached;

    LPDIRECT3DTEXTURE9 m_pBallTexture;
    LPDIRECT3DTEXTURE9 m_pPrizeTexture;
    LPDIRECT3DTEXTURE9 m_pTrapTexture;
    LPDIRECT3DTEXTURE9  m_pMotionTextures[2];
    DWORD m_dwMotionTextureIndex;

    IXCamMotionEngine* m_pMotionEngine;
    DWORD m_dwMovementThreshold;

    DWORD m_dwScore;
    BOOL m_bGameOverLost;
    BOOL m_bGameOverWon;
    StaticObject        m_Prizes[g_dwMaxPrizes];
    StaticObject        m_Traps[g_dwMaxTraps];
    BallObject m_Ball;

private:
    VOID                InitializeGame();

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
    atgApp.Run();
}


//--------------------------------------------------------------------------------------
// Name: Initialize()
// Desc: This creates all device-dependent display objects.
//--------------------------------------------------------------------------------------
HRESULT Sample::Initialize()
{
    // Initialize variables
    m_bDrawHelp = FALSE;

    srand( GetTickCount() );

    // Create the font
    if( FAILED( m_Font.Create( "game:\\Media\\Fonts\\Arial_16.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Confine text drawing to the title safe area
    m_Font.SetWindow( ATG::GetTitleSafeArea() );

    // Create the textures resource
    HRESULT hr;
    if( FAILED( hr = m_Resource.Create( "game:\\Media\\Resource.xpr" ) ) )
    {
        ATG_PrintError( "Couldn't create Resource.xpr\n" );
        return hr;
    }

    // Load & create textures

    m_pBallTexture = m_Resource.GetTexture( "BallTexture" );
    m_pPrizeTexture = m_Resource.GetTexture( "PrizeTexture" );
    m_pTrapTexture = m_Resource.GetTexture( "DeathTexture" );

    // Create motion map textures. The texture is in LIN_L8 format.
    m_dwMotionTextureIndex = 0;
    for( DWORD i = 0; i < 2; i++ )
    {
        if( FAILED( hr = m_pd3dDevice->CreateTexture( g_dwCameraImageWidth, g_dwCameraImageHeight, 1, 0,
                                                      D3DFMT_LIN_L8, 0, &m_pMotionTextures[i], NULL ) ) )
        {
            ATG_PrintError( "Could not create motion texture\n" );
            return hr;
        }
    }

    if( XCamGetStatus() != XCAMDEVICESTATE_CONNECTED )
    {
        ATG_PrintError( "Camera is either not connected or non-operational.\n" );
        m_bCameraAttached = FALSE;
    }
    else
    {
        m_bCameraAttached = TRUE;
    }

    // Create motion engine

    // There are two parameters that need to be set for motion detection: the threshold 
    // value and the persistance factor. The threshold value governs the sensitivity 
    // of motion detection. Setting the value too low, will detect camera noise, and 
    // setting it too high, will result in no motion detection.
    // The persistence factor is a time in seconds of how long to keep the motion
    // trails in the motion map. It can be 0 to disable persistence. Having a small
    // value ensures that even a small amount motion is detected.
    m_dwMovementThreshold = 30;

    XCAM_MOTION_ENGINE_INIT_PARAMS MotionParams = {0};

    MotionParams.GetDataDirectFromCamera = TRUE;    // Basic mode
    MotionParams.MovementThreshold = ( BYTE )m_dwMovementThreshold;
    MotionParams.pD3DDevice = m_pd3dDevice;
    MotionParams.PersistenceValue = 0.2;
    MotionParams.ThreadProcessorID = 3;
    MotionParams.VideoResolution = g_CameraResolution;

    if( XCamCreateMotionEngine( &MotionParams, &m_pMotionEngine ) != ERROR_SUCCESS )
    {
        ATG_PrintError( "Could not create IXCamMotionEngine\n" );
        return E_FAIL;
    }

    // Create the help
    if( FAILED( m_Help.Create( "game:\\Media\\Help\\Help.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Initialize simple shaders
    ATG::SimpleShaders::Initialize( NULL, NULL );

    InitializeGame();

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Update()
// Desc: Called once per frame, the call is the entry point for animating the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Update()
{
    // Get the current and elapsed time
    FLOAT fTime = ( FLOAT )m_Timer.GetAppTime();
    FLOAT fElapsedTime = ( FLOAT )m_Timer.GetElapsedTime();

    // Get the current gamepad state
    ATG::GAMEPAD* pGamepad = ATG::Input::GetMergedInput();

    // Toggle help
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_BACK )
        m_bDrawHelp = !m_bDrawHelp;

    // Change motion threshold
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_DPAD_UP )
    {
        m_dwMovementThreshold++;
        if( m_dwMovementThreshold > 255 )
            m_dwMovementThreshold = 255;

        m_pMotionEngine->SetThreshold( ( BYTE )m_dwMovementThreshold );
    }

    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_DPAD_DOWN )
    {
        m_dwMovementThreshold--;
        if( m_dwMovementThreshold < 1 )
            m_dwMovementThreshold = 1;

        m_pMotionEngine->SetThreshold( ( BYTE )m_dwMovementThreshold );
    }

    // Retrieve motion texture. The motion texture is an 8 bit greyscale bitmap
    // containing 0xff in areas where there is motion and 0x00 in areas without
    // motion. The bitmap is retreived for the sole purpose of drawing it as 
    // a background.
    m_dwMotionTextureIndex ^= 1;
    D3DLOCKED_RECT TextureRect;
    m_pMotionTextures[m_dwMotionTextureIndex]->LockRect( 0, &TextureRect, NULL, 0 );
    m_pMotionEngine->GetMotionMapData( &TextureRect );
    m_pMotionTextures[m_dwMotionTextureIndex]->UnlockRect( 0 );

    if( m_bGameOverWon == FALSE && m_bGameOverLost == FALSE )
    {
        // Update game objects
        D3DVECTOR pDirection;

        // Detect if the ball was hit by testing motion in an elliptical region.
        // If the ball was hit, the direction vector is returned and that is
        // used to apply a velocity
        if( m_pMotionEngine->EllipseFilledTouch( ( INT )m_Ball.fPosX, ( INT )m_Ball.fPosY,
                                                 ( FLOAT )g_dwObjectRadius, ( FLOAT )g_dwObjectRadius, &pDirection ) &&
            fTime > 5.0f )
        {
            m_Ball.fVelX = pDirection.x * 10.0f;
            m_Ball.fVelY = pDirection.y * 10.0f;
        }

        // Move the ball
        m_Ball.fPosX += m_Ball.fVelX * fElapsedTime;
        m_Ball.fPosY += m_Ball.fVelY * fElapsedTime;

        // Bounce the ball off of the screen rectangle
        if( m_Ball.fPosX < ( FLOAT )g_dwObjectRadius )
        {
            m_Ball.fPosX = ( FLOAT )g_dwObjectRadius;
            m_Ball.fVelX = -m_Ball.fVelX;
        }
        if( m_Ball.fPosX > ( FLOAT )( g_dwCameraImageWidth - g_dwObjectRadius ) )
        {
            m_Ball.fPosX = ( FLOAT )( g_dwCameraImageWidth - g_dwObjectRadius );
            m_Ball.fVelX = -m_Ball.fVelX;
        }
        if( m_Ball.fPosY < ( FLOAT )g_dwObjectRadius )
        {
            m_Ball.fPosY = ( FLOAT )g_dwObjectRadius;
            m_Ball.fVelY = -m_Ball.fVelY;
        }
        if( m_Ball.fPosY > ( FLOAT )( g_dwCameraImageHeight - g_dwObjectRadius ) )
        {
            m_Ball.fPosY = ( FLOAT )( g_dwCameraImageHeight - g_dwObjectRadius );
            m_Ball.fVelY = -m_Ball.fVelY;
        }

        // Check if ball has hit a prize
        BOOL bAnyVisible = FALSE;
        for( DWORD i = 0; i < g_dwMaxPrizes; i++ )
        {
            if( m_Prizes[i].bVisible )
            {
                LONG dx = m_Prizes[i].Position.x - ( LONG )m_Ball.fPosX;
                LONG dy = m_Prizes[i].Position.y - ( LONG )m_Ball.fPosY;

                if( ( dx * dx + dy * dy ) < ( g_dwObjectRadius * g_dwObjectRadius ) )
                {
                    m_Prizes[i].bVisible = FALSE;
                    m_dwScore += 100;
                }
                else
                {
                    bAnyVisible = TRUE;
                }
            }
        }

        if( bAnyVisible == FALSE )
        {
            m_bGameOverWon = TRUE;
        }
        else
        {
            // Check if ball has hit a trap
            for( DWORD i = 0; i < g_dwMaxTraps; i++ )
            {
                if( m_Traps[i].bVisible )
                {
                    LONG dx = m_Traps[i].Position.x - ( LONG )m_Ball.fPosX;
                    LONG dy = m_Traps[i].Position.y - ( LONG )m_Ball.fPosY;

                    if( ( dx * dx + dy * dy ) < ( g_dwObjectRadius * g_dwObjectRadius ) )
                    {
                        m_Traps[i].bVisible = FALSE;
                        m_bGameOverLost = TRUE;
                        break;
                    }
                }
            }
        }
    }
    else
    {
        // Play new game
        if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_A )
        {
            InitializeGame();
        }
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Render()
// Desc: Sets up render states, clears the viewport, and renders the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Render()
{
    // Draw a gradient filled background
    ATG::RenderBackground( 0xff000000, 0xff000000 );

    m_Timer.MarkFrame();

    // Show title, frame rate, and help
    if( m_bDrawHelp )
    {
        m_Help.Render( &m_Font, g_HelpCallouts, NUM_HELP_CALLOUTS );
    }
    else
    {
        if ( m_bCameraAttached )
        {
            // Draw motion texture
            ATG::DebugDraw::DrawScreenSpaceTexturedRect( ATG::GetTitleSafeArea(),
                                                         m_pMotionTextures[m_dwMotionTextureIndex ^ 1] );
        }
        m_pd3dDevice->SetTexture( 0, NULL );

        // Draw game objects
        m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, TRUE );
        m_pd3dDevice->SetRenderState( D3DRS_SRCBLEND, D3DBLEND_SRCALPHA );
        m_pd3dDevice->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA );

        D3DRECT Rect;
        D3DSURFACE_DESC SurfaceDesc;
        FLOAT fScaleX = ( FLOAT )( ATG::GetTitleSafeArea().x2 - ATG::GetTitleSafeArea().x1 ) / ( FLOAT )
            g_dwCameraImageWidth;
        FLOAT fScaleY = ( FLOAT )( ATG::GetTitleSafeArea().y2 - ATG::GetTitleSafeArea().y1 ) / ( FLOAT )
            g_dwCameraImageHeight;

        // Draw Prizes
        m_pPrizeTexture->GetLevelDesc( 0, &SurfaceDesc );
        for( DWORD i = 0; i < g_dwMaxPrizes; i++ )
        {
            if( m_Prizes[i].bVisible )
            {
                Rect.x1 = ATG::GetTitleSafeArea().x1 + ( LONG )( ( FLOAT )m_Prizes[i].Position.x * fScaleX ) -
                    ( SurfaceDesc.Width / 2 );
                Rect.x2 = ATG::GetTitleSafeArea().x1 + ( LONG )( ( FLOAT )m_Prizes[i].Position.x * fScaleX ) +
                    ( SurfaceDesc.Width / 2 );
                Rect.y1 = ATG::GetTitleSafeArea().y1 + ( LONG )( ( FLOAT )m_Prizes[i].Position.y * fScaleY ) -
                    ( SurfaceDesc.Height / 2 );
                Rect.y2 = ATG::GetTitleSafeArea().y1 + ( LONG )( ( FLOAT )m_Prizes[i].Position.y * fScaleY ) +
                    ( SurfaceDesc.Height / 2 );
                ATG::DebugDraw::DrawScreenSpaceTexturedRect( Rect, m_pPrizeTexture );
            }
        }

        // Draw Traps
        m_pTrapTexture->GetLevelDesc( 0, &SurfaceDesc );
        for( DWORD i = 0; i < g_dwMaxTraps; i++ )
        {
            if( m_Traps[i].bVisible )
            {
                Rect.x1 = ATG::GetTitleSafeArea().x1 + ( LONG )( ( FLOAT )m_Traps[i].Position.x * fScaleX ) -
                    ( SurfaceDesc.Width / 2 );
                Rect.x2 = ATG::GetTitleSafeArea().x1 + ( LONG )( ( FLOAT )m_Traps[i].Position.x * fScaleX ) +
                    ( SurfaceDesc.Width / 2 );
                Rect.y1 = ATG::GetTitleSafeArea().y1 + ( LONG )( ( FLOAT )m_Traps[i].Position.y * fScaleY ) -
                    ( SurfaceDesc.Height / 2 );
                Rect.y2 = ATG::GetTitleSafeArea().y1 + ( LONG )( ( FLOAT )m_Traps[i].Position.y * fScaleY ) +
                    ( SurfaceDesc.Height / 2 );
                ATG::DebugDraw::DrawScreenSpaceTexturedRect( Rect, m_pTrapTexture );
            }
        }

        // Draw ball
        m_pBallTexture->GetLevelDesc( 0, &SurfaceDesc );
        Rect.x1 = ATG::GetTitleSafeArea().x1 + ( LONG )( m_Ball.fPosX * fScaleX ) - ( SurfaceDesc.Width / 2 );
        Rect.x2 = ATG::GetTitleSafeArea().x1 + ( LONG )( m_Ball.fPosX * fScaleX ) + ( SurfaceDesc.Width / 2 );
        Rect.y1 = ATG::GetTitleSafeArea().y1 + ( LONG )( m_Ball.fPosY * fScaleY ) - ( SurfaceDesc.Height / 2 );
        Rect.y2 = ATG::GetTitleSafeArea().y1 + ( LONG )( m_Ball.fPosY * fScaleY ) + ( SurfaceDesc.Height / 2 );
        ATG::DebugDraw::DrawScreenSpaceTexturedRect( Rect, m_pBallTexture );

        m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );

        // Draw HUD
        m_Font.Begin();
        m_Font.SetScaleFactors( 1.2f, 1.2f );
        m_Font.DrawText( 0, 0, 0xffffffff, L"GestureGame" );
        WCHAR strScore[128];
        swprintf_s( strScore, L"Score: %u\n", m_dwScore );
        m_Font.DrawText( ( FLOAT )( ( ATG::GetTitleSafeArea().x2 - ATG::GetTitleSafeArea().x1 ) / 2 ), 0,
                         0xffff0000, strScore, ATGFONT_CENTER_X );

        if( m_bGameOverWon )
        {
            m_Font.DrawText( ( FLOAT )( ( ATG::GetTitleSafeArea().x2 - ATG::GetTitleSafeArea().x1 ) / 2 ),
                             ( FLOAT )( ( ATG::GetTitleSafeArea().y2 - ATG::GetTitleSafeArea().y1 ) / 2 ),
                             0xffff0000, L"GAME OVER\nYou Won!", ATGFONT_CENTER_X );
        }
        if( m_bGameOverLost )
        {
            m_Font.DrawText( ( FLOAT )( ( ATG::GetTitleSafeArea().x2 - ATG::GetTitleSafeArea().x1 ) / 2 ),
                             ( FLOAT )( ( ATG::GetTitleSafeArea().y2 - ATG::GetTitleSafeArea().y1 ) / 2 ),
                             0xffff0000, L"GAME OVER\nYou Lost", ATGFONT_CENTER_X );
        }
        if( !m_bCameraAttached )
        {
            m_Font.SetScaleFactors( 1.5f, 1.5f );
            m_Font.DrawText( ( FLOAT )( ( ATG::GetTitleSafeArea().x2 - ATG::GetTitleSafeArea().x1 ) / 2 ),
                             ( FLOAT )( ( ATG::GetTitleSafeArea().y2 - ATG::GetTitleSafeArea().y1 ) / 4 ),
                             0xffff0000, L"Camera is either not connected\nor non-operational\n", ATGFONT_CENTER_X );
        }

        m_Font.SetScaleFactors( 1.0f, 1.0f );
        m_Font.DrawText( 0, 0, 0xffffff00, m_Timer.GetFrameRate(), ATGFONT_RIGHT );

        WCHAR strTemp[128];
        swprintf_s( strTemp, L"Threshold: %u\n", m_dwMovementThreshold );
        m_Font.DrawText( 0, 30, 0xffffff00, strTemp, ATGFONT_RIGHT );

        m_Font.End();
    }

    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: InitializeGame()
// Desc: Initialize game objects for a new game.
//--------------------------------------------------------------------------------------
VOID Sample::InitializeGame()
{
    m_dwScore = 0;
    m_bGameOverLost = FALSE;
    m_bGameOverWon = FALSE;

    // Initialize game objects: put ball in the center of the screen motionless and
    // add random prize and trap objects.
    // All objects are in camera coordinates
    m_Ball.fPosX = ( FLOAT )( g_dwCameraImageWidth / 2 );
    m_Ball.fPosY = ( FLOAT )( g_dwCameraImageHeight / 2 );
    m_Ball.fVelX = 0.0f;
    m_Ball.fVelY = 0.0f;

    for( DWORD i = 0; i < g_dwMaxPrizes; i++ )
    {
        m_Prizes[i].bVisible = TRUE;
        m_Prizes[i].Position.x = g_dwObjectRadius + ( rand() % ( g_dwCameraImageWidth - ( g_dwObjectRadius * 2 ) ) );
        m_Prizes[i].Position.y = g_dwObjectRadius + ( rand() % ( g_dwCameraImageHeight - ( g_dwObjectRadius * 2 ) ) );
    }

    for( DWORD i = 0; i < g_dwMaxTraps; i++ )
    {
        m_Traps[i].bVisible = TRUE;
        m_Traps[i].Position.x = g_dwObjectRadius + ( rand() % ( g_dwCameraImageWidth - ( g_dwObjectRadius * 2 ) ) );
        m_Traps[i].Position.y = g_dwObjectRadius + ( rand() % ( g_dwCameraImageHeight - ( g_dwObjectRadius * 2 ) ) );
    }
}



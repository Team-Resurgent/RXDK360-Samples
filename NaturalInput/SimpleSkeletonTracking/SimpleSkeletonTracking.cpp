//--------------------------------------------------------------------------------------
// SimpleSkeletonTracking.cpp
//
// XNA Developer Connection.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include <xtl.h>
#include <AtgApp.h>
#include <AtgFont.h>
#include <AtgHelp.h>
#include <AtgInput.h>
#include <AtgSimpleShaders.h>
#include <AtgUtil.h>

#include "SkeletonTracking.h"
#include "Visualization.h"


//--------------------------------------------------------------------------------------
// Demonstrates how to retrieve the color and depth streams and the skeleton data from 
// the Kinect hardware.
//--------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------
// Callouts for labelling the gamepad on the help screen
//--------------------------------------------------------------------------------------
ATG::HELP_CALLOUT g_HelpCallouts[] = 
{
    { ATG::HELP_BACK_BUTTON, ATG::HELP_PLACEMENT_2, L"Display\nhelp" },
    { ATG::HELP_A_BUTTON, ATG::HELP_PLACEMENT_2, L"Toggle tilt correction" },
    { ATG::HELP_B_BUTTON, ATG::HELP_PLACEMENT_2, L"Toggle smoothing" },    
    { ATG::HELP_X_BUTTON, ATG::HELP_PLACEMENT_2, L"Toggle auto/manual exposure" },
    { ATG::HELP_Y_BUTTON, ATG::HELP_PLACEMENT_2, L"Toggle head tracking mode" },
    { ATG::HELP_DPAD, ATG::HELP_PLACEMENT_2, L"Up/Down: Adjust exposure time" },    
};
static const DWORD NUM_HELP_CALLOUTS = ARRAYSIZE(g_HelpCallouts);


//--------------------------------------------------------------------------------------
// Color values used in this sample
//--------------------------------------------------------------------------------------
#define TOP_BACK_COLOR      0xff00007f          // Background gradient colors
#define BOTTOM_BACK_COLOR   0xff000000

#define TEXT_COLOR          0xffffffff

//--------------------------------------------------------------------------------------
// Name: class Sample
// Desc: Main class to run this application. Most functionality is inherited
//       from the ATG::Application base class.
//--------------------------------------------------------------------------------------
class Sample : public ATG::Application
{
private:
    virtual HRESULT Initialize();
    virtual HRESULT Update();
    virtual HRESULT Render();

    // General sample data
    ATG::Timer m_Timer;
    ATG::Font  m_Font;
    ATG::Help  m_Help;
    BOOL       m_bDrawHelp;    

    BOOL       m_bManualExposure;
    FLOAT      m_fExposureTime;
};

extern NUI_SKELETON_FRAME          g_SkeletonFrame;
extern BOOL                        g_bColorFrameSucceeded;
BOOL                               g_bHeadTracking = FALSE;

//--------------------------------------------------------------------------------------
// Name: main()
// Desc: Entry point to the program
//--------------------------------------------------------------------------------------
VOID __cdecl main()
{
    Sample atgApp;
    ATG::GetVideoSettings( &atgApp.m_d3dpp.BackBufferWidth, &atgApp.m_d3dpp.BackBufferHeight );
    atgApp.m_d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_TWO;

    // Make sure display is gamma correct.
    atgApp.m_d3dpp.BackBufferFormat =  ( D3DFORMAT )MAKESRGBFMT( D3DFMT_A8R8G8B8 );
    atgApp.m_d3dpp.FrontBufferFormat = ( D3DFORMAT )MAKESRGBFMT( D3DFMT_LE_X8R8G8B8 );

    atgApp.Run();
}


//--------------------------------------------------------------------------------------
// Name: Initialize()
// Desc: This creates all device-dependent display objects.
//--------------------------------------------------------------------------------------
HRESULT Sample::Initialize()
{
    m_bDrawHelp = FALSE;
    m_bManualExposure = FALSE;
    m_fExposureTime = NUI_CAMERA_PROPERTYF_EXPOSURE_TIME_DEFAULT;

    // Create the font
    if( FAILED( m_Font.Create( "game:\\Media\\Fonts\\Arial_16.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Confine text drawing to the title safe area
    m_Font.SetWindow( ATG::GetTitleSafeArea() );

        // Initialize the simple shaders
    ATG::SimpleShaders::Initialize( NULL, NULL );

    // Create the help
    if( FAILED( m_Help.Create( "game:\\Media\\Help\\Help.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Initialize the camera and skeleton tracking
    if( FAILED( InitializeSkeletonTracking( m_pd3dDevice) ) )
        return E_FAIL;

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Update()
// Desc: Called once per frame, the call is the entry point for animating the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Update()
{
    // Update skeleton tracking
    UpdateSkeletonTracking( m_pd3dDevice );

    // Get input from all the gamepads
    ATG::GAMEPAD* pGamepad = ATG::Input::GetMergedInput();

    // Exit help screen if a player presses BACK
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_BACK )
        m_bDrawHelp = !m_bDrawHelp;

    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_A )
        SetTiltCorrectionState( !GetTiltCorrectionState() );

    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_B )
        SetSmoothingState( !GetSmoothingState() ); 

    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_X )
    {
        m_bManualExposure = !m_bManualExposure;    
        
        if ( !m_bManualExposure )
            NuiCameraSetPropertyF( NUI_CAMERA_TYPE_COLOR, NUI_CAMERA_PROPERTYF_EXPOSURE_TIME, NUI_CAMERA_PROPERTYF_EXPOSURE_TIME_DEFAULT );

        NuiCameraSetProperty( NUI_CAMERA_TYPE_COLOR, 
                              NUI_CAMERA_PROPERTY_AE_AWB_MODE, 
                              m_bManualExposure ? NUI_CAMERA_PROPERTY_AE_AWB_MODE_OFF : NUI_CAMERA_PROPERTY_AE_AWB_MODE_STANDARD );

        if ( m_bManualExposure )
            NuiCameraSetPropertyF( NUI_CAMERA_TYPE_COLOR, NUI_CAMERA_PROPERTYF_EXPOSURE_TIME, m_fExposureTime );
    }

    if ( m_bManualExposure && (pGamepad->wPressedButtons & XINPUT_GAMEPAD_DPAD_UP || pGamepad->wPressedButtons & XINPUT_GAMEPAD_DPAD_DOWN) )
    {
        FLOAT fDelta = 0;
        if ( pGamepad->wPressedButtons & XINPUT_GAMEPAD_DPAD_UP )
            fDelta = 1;
        else
            fDelta = -1;                       
        
        m_fExposureTime += fDelta;
        m_fExposureTime = __min( m_fExposureTime, NUI_CAMERA_PROPERTYF_EXPOSURE_TIME_MAXIMUM );
        m_fExposureTime = __max( m_fExposureTime, NUI_CAMERA_PROPERTYF_EXPOSURE_TIME_MINIMUM );
        NuiCameraSetPropertyF( NUI_CAMERA_TYPE_COLOR, NUI_CAMERA_PROPERTYF_EXPOSURE_TIME, m_fExposureTime );
    }    

    if ( pGamepad->wPressedButtons & XINPUT_GAMEPAD_Y )
    {
        g_bHeadTracking = !g_bHeadTracking;

        if ( g_bHeadTracking == FALSE )
        {
            NUI_IMAGE_VIEW_AREA ViewArea;
            ViewArea.lCenterX = 0;
            ViewArea.lCenterY = 0;
            ViewArea.eDigitalZoom = NUI_IMAGE_DIGITAL_ZOOM_1X;
            NuiCameraSetColorImageViewArea( &ViewArea );
        }                
    }

    if ( g_bHeadTracking )
    {
        NUI_IMAGE_VIEW_AREA ViewArea;
        ViewArea.lCenterX = 0;
        ViewArea.lCenterY = 0;
        ViewArea.eDigitalZoom = NUI_IMAGE_DIGITAL_ZOOM_2X;
        
        for ( INT i = 0; i < NUI_SKELETON_COUNT; ++i )
        {
            if ( g_SkeletonFrame.SkeletonData[i].eTrackingState == NUI_SKELETON_TRACKED )
            {
                NUI_SKELETON_DATA *pSkeletonData = &g_SkeletonFrame.SkeletonData[i];
                if ( pSkeletonData->eSkeletonPositionTrackingState[NUI_SKELETON_POSITION_HEAD] != NUI_SKELETON_POSITION_NOT_TRACKED )
                {
                    LONG plDepthX, plDepthY, plColorX, plColorY;
                    USHORT usDepthValue;
                    NuiTransformSkeletonToDepthImage( pSkeletonData->SkeletonPositions[ NUI_SKELETON_POSITION_HEAD ],
                                                      &plDepthX, &plDepthY, &usDepthValue );

                    NuiImageGetColorPixelCoordinatesFromDepthPixel( NUI_IMAGE_RESOLUTION_640x480,
                                                                    NULL,
                                                                    plDepthX,
                                                                    plDepthY,
                                                                    usDepthValue,
                                                                    &plColorX,
                                                                    &plColorY );

                    ViewArea.lCenterX = plColorX - 320;
                    ViewArea.lCenterY = plColorY - 240;
                    if ( ViewArea.lCenterX <= -320 ) ViewArea.lCenterX = -319;
                    if ( ViewArea.lCenterX >= 320 ) ViewArea.lCenterX = 319;
                    if ( ViewArea.lCenterY <= -240 ) ViewArea.lCenterY = -239;
                    if ( ViewArea.lCenterY >= 240 ) ViewArea.lCenterY = 239;                    
                }                
                
                break;
            }
        }

        // The if statement here can be safely omitted, 
        // however it is recommended because the NuiCameraSetColorImageViewArea function call fails 
        // when it is called more frequent than the successful arrival of color frame
        if ( g_bColorFrameSucceeded )
            NuiCameraSetColorImageViewArea( &ViewArea );
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
    ATG::RenderBackground( TOP_BACK_COLOR, BOTTOM_BACK_COLOR );

    // Show title, frame rate, and help
    m_Timer.MarkFrame();
    if( m_bDrawHelp )
        m_Help.Render( &m_Font, g_HelpCallouts, NUM_HELP_CALLOUTS );
    else
    {
        WCHAR tempBuffer[ 200 ];

        BOOL pbTracking[ NUI_SKELETON_COUNT ];
        // Visualize the data streaming from the camera and the skeletons
        VisualizeSkeletonTracking( m_pd3dDevice, pbTracking );

        // Draw title text
        m_Font.Begin();
        m_Font.SetScaleFactors( 1.2f, 1.2f );
        m_Font.DrawText( 0, 0, 0xffffffff,  L"Simple Skeleton Tracking" );

        m_Font.SetScaleFactors( 1.0f, 1.0f );
        m_Font.DrawText( 0, 0, 0xffffff00, m_Timer.GetFrameRate(), ATGFONT_RIGHT );
  
        m_Font.DrawText( 140, 460, 0xffffff00, L"Tilt correction:", ATGFONT_RIGHT );
        m_Font.DrawText( 365, 460, 0xffffff00, L"Smoothing:", ATGFONT_RIGHT );

        m_Font.DrawText( 150, 460, 0xffffff00, GetTiltCorrectionState() ? L"On" : L"Off"  );
        m_Font.DrawText( 375, 460, 0xffffff00, GetSmoothingState() ? L"On" : L"Off" );

        m_Font.DrawText( 185, 460, 0xffffffff, GLYPH_A_BUTTON );
        m_Font.DrawText( 420, 460, 0xffffffff, GLYPH_B_BUTTON );

        m_Font.DrawText( 140, 510, 0xffffff00, L"Auto exposure:", ATGFONT_RIGHT );
        m_Font.DrawText( 150, 510, 0xffffff00, m_bManualExposure ? L"Off" : L"On" );
        m_Font.DrawText( 185, 510, 0xffffffff, GLYPH_X_BUTTON );

        m_Font.DrawText( 365, 510, 0xffffff00, L"Head tracking:", ATGFONT_RIGHT );
        m_Font.DrawText( 375, 510, 0xffffff00, g_bHeadTracking ? L"On" : L"Off" );
        m_Font.DrawText( 420, 510, 0xffffffff, GLYPH_Y_BUTTON );

        if ( m_bManualExposure )
        {
            m_Font.DrawText( 0, 560, 0xffffff00, L"Use DPad up/down to adjust exposure time" );
        }

        UINT uNumTracked = 0;
        for ( UINT i = 0 ; i < NUI_SKELETON_COUNT ; ++i )
        {
            if (pbTracking[i])
            {
                swprintf_s ( tempBuffer, 200, L"Skeleton %d is Tracked", i );
                m_Font.DrawText( 0, (FLOAT)(505 + uNumTracked * 30), 0xFFFFFFFF, tempBuffer, ATGFONT_RIGHT );
                uNumTracked++;
            }
        }

        m_Font.End();
    }

    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}
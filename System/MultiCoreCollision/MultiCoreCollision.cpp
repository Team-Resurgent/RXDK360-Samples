//--------------------------------------------------------------------------------------
// MultiCoreCollision.cpp
//
// This sample shows how to use the optimized collision library found in the sample
// framework on multiple cores. The movement and collision detection are done on 
// cores 1 - 5.
//
// The meat of the sample is in the Collide() function below.  Other common code is
// also used, including the SimpleShader library for setting up standard vertex
// declarations, vertex shaders, and pixel shaders, and the DebugDraw library for
// rendering the collision volumes.
//
// XNA Developer Connection.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include <xtl.h>
#include <xboxmath.h>
#include <xbdm.h>

#include <AtgApp.h>
#include <AtgFont.h>
#include <AtgHelp.h>
#include <AtgInput.h>
#include <AtgUtil.h>
#include <AtgBound.h>
#include <AtgCollision.h>
#include <AtgSimpleShaders.h>
#include <AtgDebugDraw.h>

#include "Scheduler.h"

//--------------------------------------------------------------------------------------
// Callouts for labelling the gamepad on the help screen
//--------------------------------------------------------------------------------------
ATG::HELP_CALLOUT g_HelpCallouts[] =
{
    {
        ATG::HELP_B_BUTTON,
        ATG::HELP_PLACEMENT_2,
        L"Ortho/\npersp camera"
    },
    {
        ATG::HELP_X_BUTTON,
        ATG::HELP_PLACEMENT_2,
        L"Toggle single and multiple cores"
    },
    {
        ATG::HELP_RIGHTSTICK,
        ATG::HELP_PLACEMENT_1,
        L"Move camera"
    },
    {
        ATG::HELP_BACK_BUTTON,
        ATG::HELP_PLACEMENT_2,
        L"Display\nhelp"
    },
    {
        ATG::HELP_MISC_CALLOUT,
        ATG::HELP_PLACEMENT_1,
        L"Triggers zoom in/out"
    },
};
#define NUM_HELP_CALLOUTS (sizeof(g_HelpCallouts) / sizeof(g_HelpCallouts[0]))

// Define the Thread safe log that can be written to from any thread.
XLOCKFREE_LOG   collisionLog;

//--------------------------------------------------------------------------------------
// MultiCore object structs - these hold a collision object and a bool flag indicating
// whether the object has collided with another object
//--------------------------------------------------------------------------------------
struct CollisionSphere
{
    INT m_offset;
    ATG::Sphere m_Sphere;
    INT m_iCollision;
};

struct CollisionBox
{
    INT m_offset;
    ATG::OrientedBox m_OBox;
    INT m_iCollision;
};

struct CollisionAABox
{
    INT m_offset;
    ATG::AxisAlignedBox m_AABox;
    INT m_iCollision;
};



struct CollisionTriangle
{
    INT m_offset;
    XMVECTOR m_PointA;
    XMVECTOR m_PointB;
    XMVECTOR m_PointC;
    INT m_iCollision;
};

class Sample;

#include "MultiCoreTasks.h"

//--------------------------------------------------------------------------------------
// Global constants that set up the collision groups and camera angles
//-------------------------------------------------------------------------------------

const INT       ITEM_COUNT = 2500;  // Number of sphere/s, boxes and triangles to use.
const INT       TASK_COUNT = 50;    // Number of tasks used to move the collision calculation
// to other cores.

//--------------------------------------------------------------------------------------
// Name: class Sample
// Desc: The Sample class implements the MultiCore sample.
//--------------------------------------------------------------------------------------
class Sample : public ATG::Application
{
public:
            Sample();

    HRESULT Initialize();
    HRESULT Update();
    HRESULT Render();

    VOID    IncrementCollision()
    {
        m_Collisions++;
    }

protected:
    VOID    InitializeObjects();
    VOID    Collide();
    VOID    Dispatch();
    VOID    UpdateCamera();
    VOID    RenderObjects();
    VOID    DrawObjects();

    XMMATRIX m_matWorld;
    XMMATRIX m_matView;
    XMMATRIX m_matProj;

    // ATG help display
    ATG::Font m_Font;
    ATG::Help m_Help;
    ATG::Timer m_Timer;
    BOOL m_bDrawHelp;

    // Primary collision objects
    ATG::Frustum m_PrimaryFrustum;

    // Secondary collision objects - these are animated and tested for collision with the primary objects
    CollisionSphere* m_SecondarySpheres;
    CollisionBox* m_SecondaryOrientedBoxes;
    CollisionAABox* m_SecondaryAABoxes;
    CollisionTriangle* m_SecondaryTriangles;

    // Camera origins for each of the primary collision objects
    XMVECTOR m_CameraOrigin;

    // Spacing between cameras for each primary object
    FLOAT m_fCameraSpacing;
    BOOL m_bCameraOrtho;
    FLOAT m_fAspectRatio;

    // Scheduler and Tasks
    ATG::TaskScheduler m_scheduler;

    MoveSphereAroundFrustumTask* m_SphereFrustumTask;
    MoveOrientedBoxAroundFrustumTask* m_OrientedBoxFrustumTask;
    MoveAABoxAroundFrustumTask* m_AABoxFrustumTask;
    MoveTriangleAroundFrustumTask* m_TriangleFrustumTask;

    UINT m_Count;
    UINT m_Collisions;
    BOOL m_bUseMultipleCores;
};


//--------------------------------------------------------------------------------------
// Name: Sample() constructor
//--------------------------------------------------------------------------------------
Sample::Sample() : m_fCameraSpacing( 50.0f ),
                   m_bCameraOrtho( FALSE ),
                   m_bDrawHelp( FALSE ),
                   m_fAspectRatio( 16.0f / 9.0f ),
                   m_SphereFrustumTask( NULL ),
                   m_OrientedBoxFrustumTask( NULL ),
                   m_AABoxFrustumTask( NULL ),
                   m_TriangleFrustumTask( NULL ),
                   m_Count( 0 ),
                   m_Collisions( 0 ),
                   m_bUseMultipleCores( TRUE )
{
}


//--------------------------------------------------------------------------------------
// Name: Initialize()
// Desc: Loads some content and initializes all of the collision data
//--------------------------------------------------------------------------------------
HRESULT Sample::Initialize()
{
    // World matrix (identity in this sample)
    m_matWorld = XMMatrixIdentity();

    // Initialize the simple shaders (used by the debug draw code)
    ATG::SimpleShaders::Initialize( NULL, NULL );

    DmMapDevkitDrive();

    // Set the thread safe log, it will print to MultiCoreLog.txt, the lines cannot be 
    //   longer the 120 bytes, and it will only have 4000 messages pending to write. If
    //   more then 4000 messages are to be written it will drop messages.
    if( FAILED( XLFStartLog( 0, "e:\\MultiCoreLog.txt", 120, 4000, FALSE, &collisionLog ) ) )
        return E_FAIL;

    if( FAILED( m_scheduler.Initialize( collisionLog ) ) )
        return E_FAIL;

    // Initialize the starting positions of the collision objects
    InitializeObjects();

    // Initialize the font
    if( FAILED( m_Font.Create( "game:\\Media\\Fonts\\Arial_16.xpr" ) ) )
        return E_FAIL;

    // Confine text drawing to the title safe area
    m_Font.SetWindow( ATG::GetTitleSafeArea() );

    // Initialize the help
    if( FAILED( m_Help.Create( "game:\\Media\\Help\\Help.xpr" ) ) )
        return E_FAIL;

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Update()
// Desc: Collects input, animates and collides the collision objects, and moves the 
//       camera
//--------------------------------------------------------------------------------------
HRESULT Sample::Update()
{
    m_Collisions = 0;

    // Update the timer
    m_Timer.MarkFrame();

    // Grab input state from controllers
    ATG::Input::GetMergedInput();

    // X button isolates the current group, or shows all groups
    if( ATG::Input::m_DefaultGamepad.wPressedButtons & XINPUT_GAMEPAD_X )
    {
        if( m_bUseMultipleCores )
            m_bUseMultipleCores = FALSE;
        else
            m_bUseMultipleCores = TRUE;
    }

    // Back button toggles help display
    if( ATG::Input::m_DefaultGamepad.wPressedButtons & XINPUT_GAMEPAD_BACK )
    {
        m_bDrawHelp = !m_bDrawHelp;
    }

    if( m_bUseMultipleCores )
    {
        Dispatch();
    }

    // Update the camera
    UpdateCamera();

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Render()
// Desc: Draws the collision objects and the UI
//--------------------------------------------------------------------------------------
HRESULT Sample::Render()
{
    WCHAR buffer[132] = L"";

    // Clear the backbuffer
    m_pd3dDevice->Clear( 0L, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL, D3DCOLOR_ARGB( 0, 0, 0, 0 ),
                         1.0f, 0L );

    // Render all of the objects
    RenderObjects();

    // Show title, frame rate, and help
    if( m_bDrawHelp )
    {
        m_Help.Render( &m_Font, g_HelpCallouts, NUM_HELP_CALLOUTS );
    }
    else
    {
        // Render a FPS indicator and title
        m_Font.Begin();
        m_Font.SetScaleFactors( 1.2f, 1.2f );
        if( m_bUseMultipleCores )
        {
            swprintf_s( buffer, L"Multiple Cores (X to toggle)\n%5d collisions out of %5d tests", m_Collisions,
                        ITEM_COUNT * 4 );
        }
        else
        {
            swprintf_s( buffer, L"Single Core (X to toggle)\n%5d collisions out of %5d tests", m_Collisions,
                        ITEM_COUNT * 4 );
        }
        m_Font.DrawText( 0, 0, 0xffffffff, buffer );
        m_Font.SetScaleFactors( 1.0f, 1.0f );
        m_Font.DrawText( 0, 0, 0xffffff00, m_Timer.GetFrameRate(), ATGFONT_RIGHT );
        m_Font.End();
    }

    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: InitializeObjects()
// Desc: Sets up the initial state and orientation of each collision object
//--------------------------------------------------------------------------------------
VOID Sample::InitializeObjects()
{
    const XMVECTOR XMZero =
    {
        0,
        0,
        0,
        0
    };

    // Set up the primary frustum object from a D3D projection matrix
    // NOTE: This can also be done on your camera's projection matrix.  The projection
    // matrix built here is somewhat contrived so it renders well.
    XMMATRIX xmProj = XMMatrixPerspectiveFovLH( XM_PIDIV4, 1.77778f, 0.5f, 10.0f );
    ComputeFrustumFromProjection( &m_PrimaryFrustum, &xmProj );
    m_PrimaryFrustum.Origin.z = -7.0f;
    m_CameraOrigin = XMVectorSet( 0, 0, 0, 0 );

    m_SecondarySpheres = new CollisionSphere[ITEM_COUNT];
    m_SecondaryOrientedBoxes = new CollisionBox[ITEM_COUNT];
    m_SecondaryAABoxes = new CollisionAABox[ITEM_COUNT];
    m_SecondaryTriangles = new CollisionTriangle[ITEM_COUNT];

    // Initialize all of the secondary objects with default values
    for( UINT j = 0; j < ITEM_COUNT; j++ )
    {
        m_SecondarySpheres[j].m_offset = j;
        m_SecondarySpheres[j].m_Sphere.Radius = 1.0f;
        m_SecondarySpheres[j].m_Sphere.Center = XMFLOAT3( 0, 0, 0 );
        m_SecondarySpheres[j].m_iCollision = FALSE;

        m_SecondaryOrientedBoxes[j].m_offset = j;
        m_SecondaryOrientedBoxes[j].m_OBox.Center = XMFLOAT3( 0, 0, 0 );
        m_SecondaryOrientedBoxes[j].m_OBox.Extents = XMFLOAT3( 0.5f, 0.5f, 0.5f );
        m_SecondaryOrientedBoxes[j].m_OBox.Orientation = XMFLOAT4( 0, 0, 0, 1 );
        m_SecondaryOrientedBoxes[j].m_iCollision = FALSE;

        m_SecondaryAABoxes[j].m_offset = j;
        m_SecondaryAABoxes[j].m_AABox.Center = XMFLOAT3( 0, 0, 0 );
        m_SecondaryAABoxes[j].m_AABox.Extents = XMFLOAT3( 0.5f, 0.5f, 0.5f );
        m_SecondaryAABoxes[j].m_iCollision = FALSE;

        m_SecondaryTriangles[j].m_offset = j;
        m_SecondaryTriangles[j].m_PointA = XMZero;
        m_SecondaryTriangles[j].m_PointB = XMZero;
        m_SecondaryTriangles[j].m_PointC = XMZero;
        m_SecondaryTriangles[j].m_iCollision = FALSE;

    }

    m_SphereFrustumTask = new MoveSphereAroundFrustumTask[TASK_COUNT];
    m_OrientedBoxFrustumTask = new MoveOrientedBoxAroundFrustumTask[TASK_COUNT];
    m_AABoxFrustumTask = new MoveAABoxAroundFrustumTask[TASK_COUNT];
    m_TriangleFrustumTask = new MoveTriangleAroundFrustumTask[TASK_COUNT];

    INT increment = ITEM_COUNT / TASK_COUNT;
    INT index = 0;
    for( UINT i = 0; i < TASK_COUNT; i++ )
    {
        m_SphereFrustumTask[i].Initialize( &m_PrimaryFrustum, &( m_SecondarySpheres[index] ), &m_Collisions,
                                           increment );
        m_OrientedBoxFrustumTask[i].Initialize( &m_PrimaryFrustum, &( m_SecondaryOrientedBoxes[index] ), &m_Collisions,
                                                increment );
        m_AABoxFrustumTask[i].Initialize( &m_PrimaryFrustum, &( m_SecondaryAABoxes[index] ), &m_Collisions,
                                          increment );
        m_TriangleFrustumTask[i].Initialize( &m_PrimaryFrustum, &( m_SecondaryTriangles[index] ), &m_Collisions,
                                             increment );
        index += increment;
    }
}


//--------------------------------------------------------------------------------------
// Name: UpdateCamera()
// Desc: Modifies camera settings based on controller input, sets up the camera view
//       matrix, and sends results to shader system.
//--------------------------------------------------------------------------------------
VOID Sample::UpdateCamera()
{
    // Variables for camera position and movement speeds
    static FLOAT s_fYaw = 0.75f * XM_PI;
    static FLOAT s_fPitch = XM_PIDIV4;
    static FLOAT s_fDistance = 40.0f;
    const FLOAT fYawRate = 1.0f;   // Radians per second
    const FLOAT fPitchRate = 0.75f;  // Radians per second
    const FLOAT fDistanceRate = 10.0f;   // Meters per second

    FLOAT fDelta = ( FLOAT )m_Timer.GetElapsedTime();
    // Change yaw and pitch based on right thumbstick input
    s_fYaw += ATG::Input::m_DefaultGamepad.fX2 * fDelta * fYawRate;
    s_fPitch += ATG::Input::m_DefaultGamepad.fY2 * fDelta * fPitchRate;

    // Clamp pitch to (-90, 90) degrees
    if( s_fPitch > XM_PIDIV2 )
        s_fPitch = XM_PIDIV2;
    else if( s_fPitch < -XM_PIDIV2 )
        s_fPitch = -XM_PIDIV2;

    // Zoom in and out based on trigger input
    if( ATG::Input::m_DefaultGamepad.bLeftTrigger )
        s_fDistance += fDistanceRate * fDelta;
    if( ATG::Input::m_DefaultGamepad.bRightTrigger )
        s_fDistance -= fDistanceRate * fDelta;

    // Clamp distance
    if( s_fDistance < 2.0f )
        s_fDistance = 2.0f;
    else if( s_fDistance > 80.0f )
        s_fDistance = 80.0f;

    // Switch cameras with the A button
    if( ATG::Input::m_DefaultGamepad.wPressedButtons & XINPUT_GAMEPAD_A )
    {
    }

    // Reset camera angle/distance with the Y button
    if( ATG::Input::m_DefaultGamepad.wPressedButtons & XINPUT_GAMEPAD_Y )
    {
        s_fYaw = 0.75f * XM_PI;
        s_fPitch = XM_PIDIV4;
        s_fDistance = 40.0f;
    }

    // Switch between perspective and ortho camera with the B button.
    // Ortho camera is useful for lining up the camera on an object's axis, and verifying
    // that collisions do not register when the objects are not colliding.
    if( ATG::Input::m_DefaultGamepad.wPressedButtons & XINPUT_GAMEPAD_B )
        m_bCameraOrtho = !m_bCameraOrtho;

    // Smoothly lerp camera target as it changes
    static XMVECTOR s_CameraTarget = m_CameraOrigin;
    FLOAT LerpFactor = min( 4.0f * fDelta, 1.0f );
    s_CameraTarget = ( LerpFactor * m_CameraOrigin ) +
        ( ( 1.0f - LerpFactor ) * s_CameraTarget );

    // Compute view matrix
    XMVECTOR vEyePt = XMVectorSet( s_fDistance * cosf( s_fPitch ) * sinf( s_fYaw ),
                                   s_fDistance * sinf( s_fPitch ), s_fDistance * cosf( s_fPitch ) *
                                   cosf( s_fYaw ), 0.0f );
    XMVECTOR vLookatPt = s_CameraTarget;
    XMVECTOR vUp = XMVectorSet( 0.0f, 1.0f, 0.0f, 0.0f );
    vEyePt += vLookatPt;
    m_matView = XMMatrixLookAtLH( vEyePt, vLookatPt, vUp );

    // Set up projection matrix (ortho or perspective)
    if( m_bCameraOrtho )
        m_matProj = XMMatrixOrthographicLH( m_fAspectRatio * s_fDistance, s_fDistance, 1.0f, 200.0f );
    else
        m_matProj = XMMatrixPerspectiveFovLH( XM_PIDIV4, m_fAspectRatio, 1.0f, 200.0f );

    // Send view*projection to debug draw system
    XMMATRIX matVP = XMMatrixMultiply( m_matView, m_matProj );
    ATG::DebugDraw::SetViewProjection( matVP );
}


//--------------------------------------------------------------------------------------
// Name: Dispatch()
// Desc: Schedule all tasks.
//--------------------------------------------------------------------------------------
VOID Sample::Dispatch()
{
    m_Count = 0;

    float fTime = ( FLOAT )m_Timer.GetAbsoluteTime() * 0.2f;
    for( UINT j = 0; j < TASK_COUNT; j++ )
    {
        m_SphereFrustumTask[j].Setup( fTime );
        m_OrientedBoxFrustumTask[j].Setup( fTime );
        m_AABoxFrustumTask[j].Setup( fTime );
        m_TriangleFrustumTask[j].Setup( fTime, m_CameraOrigin.x );

        m_scheduler.ScheduleTask( &m_SphereFrustumTask[j] );
        m_scheduler.ScheduleTask( &m_OrientedBoxFrustumTask[j] );
        m_scheduler.ScheduleTask( &m_AABoxFrustumTask[j] );
        m_scheduler.ScheduleTask( &m_TriangleFrustumTask[j] );
        m_Count += 4;
    }
}


//--------------------------------------------------------------------------------------
// Name: RenderObjects()
// Desc: Renders the collision volumes, using color coding to represent the results of
//       the collision testing
//--------------------------------------------------------------------------------------
VOID Sample::RenderObjects()
{
    // Set up some color constants
    const D3DCOLOR ColorWhite = 0xFFFFFFFF;
    const D3DCOLOR ColorGround = 0xFF0000C0;


    // Draw ground planes

    XMFLOAT3 vXAxis( 20, 0,  0 );
    XMFLOAT3 vYAxis( 0, 0, 20 );
    XMFLOAT3 vOrigin( m_CameraOrigin.x, m_CameraOrigin.y - 10, m_CameraOrigin.z );
    INT iXDivisions = 20;
    INT iYDivisions = 20;
    ATG::DebugDraw::DrawGrid( vXAxis, vYAxis, vOrigin, iXDivisions, iYDivisions, ColorGround );

    // Draw primary collision objects in white
    ATG::DebugDraw::DrawFrustum( m_PrimaryFrustum, ColorWhite );
    if( m_bUseMultipleCores )
    {
        if( FAILED( m_scheduler.ProcessResults( m_Count ) ) )
        {
            XLFEndLog( collisionLog );
            XLaunchNewImage( XLAUNCH_KEYWORD_DEFAULT_APP, 0 );
        }
    }
    else
    {
        DrawObjects();
    }
}


//--------------------------------------------------------------------------------------
// Name: DrawObjects()
// Desc: Draw all objects.
//--------------------------------------------------------------------------------------
VOID Sample::DrawObjects()
{
    float fTime = ( FLOAT )m_Timer.GetAbsoluteTime() * 0.2f;
    for( UINT i = 0; i < ITEM_COUNT; i++ )
    {
        /**/
        // Sphere
        m_SecondarySpheres[i].m_Sphere.Center.x = 10 * sinf( 3 * fTime * i );
        m_SecondarySpheres[i].m_Sphere.Center.y = 7 * cosf( 5 * fTime * i );
        m_SecondarySpheres[i].m_iCollision = ATG::IntersectSphereFrustum( &( m_SecondarySpheres[i].m_Sphere ),
                                                                          &m_PrimaryFrustum );
        if( m_SecondarySpheres[i].m_iCollision > 0 )
        {
            ATG::DebugDraw::DrawSphere( m_SecondarySpheres[i].m_Sphere,
                                        GetCollisionColor( m_SecondarySpheres[i].m_iCollision, 0 ) );
            m_Collisions++;
        }
        /**/

        // Oriented box
        m_SecondaryOrientedBoxes[i].m_OBox.Center.x = ( float )( 10.0 *
                                                                 sinf( float( 3.5 * fTime *
                                                                              m_SecondaryOrientedBoxes[i].m_offset ) )
                                                                 );
        m_SecondaryOrientedBoxes[i].m_OBox.Center.y = 7 * cosf( float( 5.1 * fTime *
                                                                       m_SecondaryOrientedBoxes[i].m_offset ) );
        XMStoreFloat4( &( m_SecondaryOrientedBoxes[i].m_OBox.Orientation ), XMQuaternionRotationRollPitchYaw( fTime *
                                                                                                              1.4f,
                                                                                                              fTime *
                                                                                                              0.2f,
                                                                                                              fTime )
                       );
        m_SecondaryOrientedBoxes[i].m_iCollision = ATG::IntersectOrientedBoxFrustum
            ( &( m_SecondaryOrientedBoxes[i].m_OBox ), &m_PrimaryFrustum );
        if( m_SecondaryOrientedBoxes[i].m_iCollision )
        {
            ATG::DebugDraw::DrawObb( m_SecondaryOrientedBoxes[i].m_OBox,
                                     GetCollisionColor( m_SecondaryOrientedBoxes[i].m_iCollision, 0 ) );
            m_Collisions++;
        }

        /**/
        // AA box
        m_SecondaryAABoxes[i].m_AABox.Center.x = 10 * sinf( 2.1f * fTime * m_SecondaryAABoxes[i].m_offset );
        m_SecondaryAABoxes[i].m_AABox.Center.y = 7 * cosf( 3.8f * fTime * m_SecondaryAABoxes[i].m_offset );

        m_SecondaryAABoxes[i].m_iCollision = ATG::IntersectAxisAlignedBoxFrustum( &( m_SecondaryAABoxes[i].m_AABox ),
                                                                                  &m_PrimaryFrustum );
        if( m_SecondaryAABoxes[i].m_iCollision )
        {
            ATG::DebugDraw::DrawAabb( m_SecondaryAABoxes[i].m_AABox,
                                      GetCollisionColor( m_SecondaryAABoxes[i].m_iCollision, 0 ) );
            m_Collisions++;
        }

        // Triangle
        XMMATRIX TriangleCoords = XMMatrixRotationRollPitchYaw( fTime * 1.4f, fTime * 2.5f, fTime );
        XMMATRIX Translation = XMMatrixTranslation( 5 * sinf( float( 5.3f * fTime *
                                                                     m_SecondaryTriangles[i].m_offset ) ) +
                                                    float( m_CameraOrigin.x ), 5 * cosf( float( 2.3f * fTime *
                                                                                                m_SecondaryTriangles[i].m_offset ) ), 5 * sinf( float( 3.4f * fTime *
                                                                                                                                                       m_SecondaryTriangles[i].m_offset ) ) );
        TriangleCoords = XMMatrixMultiply( TriangleCoords, Translation );
        m_SecondaryTriangles[i].m_PointA = XMVector3Transform( TrianglePointA, TriangleCoords );
        m_SecondaryTriangles[i].m_PointB = XMVector3Transform( TrianglePointB, TriangleCoords );
        m_SecondaryTriangles[i].m_PointC = XMVector3Transform( TrianglePointC, TriangleCoords );

        m_SecondaryTriangles[i].m_iCollision = ATG::IntersectTriangleFrustum( m_SecondaryTriangles[i].m_PointA,
                                                                              m_SecondaryTriangles[i].m_PointB,
                                                                              m_SecondaryTriangles[i].m_PointC,
                                                                              &m_PrimaryFrustum );

        if( m_SecondaryTriangles[i].m_iCollision )
        {
            XMFLOAT3 Verts[3];
            XMStoreFloat3( &Verts[0], m_SecondaryTriangles[i].m_PointA );
            XMStoreFloat3( &Verts[1], m_SecondaryTriangles[i].m_PointB );
            XMStoreFloat3( &Verts[2], m_SecondaryTriangles[i].m_PointC );
            ATG::DebugDraw::DrawTriangle( Verts[0], Verts[1], Verts[2],
                                          GetCollisionColor( m_SecondaryTriangles[i].m_iCollision, 0 ) );
            m_Collisions++;
        }
        /**/

    }
}

//--------------------------------------------------------------------------------------
// Name: main()
// Desc: Entry point to the program
//--------------------------------------------------------------------------------------
INT __cdecl main()
{
    Sample sample;

    ATG::GetVideoSettings( &sample.m_d3dpp.BackBufferWidth, &sample.m_d3dpp.BackBufferHeight );

    sample.Run();
}

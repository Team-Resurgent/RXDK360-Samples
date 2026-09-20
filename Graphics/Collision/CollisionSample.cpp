//--------------------------------------------------------------------------------------
// CollisionSample.cpp
//
// This sample shows how to use ATG's optimized collision library.
//
// The collision library is part of the ATG framework, located in
// common/AtgCollision.h and common/AtgCollision.cpp.  This library is optimized for
// Xbox 360, using the VMX-128 vector floating point instructions extensively.
//
// The meat of the sample is in the Collide() function below.  Other ATG common code is
// also used, including the SimpleShader library for setting up standard vertex
// declarations, vertex shaders, and pixel shaders, and the DebugDraw library for
// rendering the collision volumes.
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include <xtl.h>
#include <xboxmath.h>

#include <AtgApp.h>
#include <AtgFont.h>
#include <AtgHelp.h>
#include <AtgInput.h>
#include <AtgUtil.h>
#include <AtgBound.h>
#include <AtgCollision.h>
#include <AtgSimpleShaders.h>
#include <AtgDebugDraw.h>


//--------------------------------------------------------------------------------------
// Callouts for labelling the gamepad on the help screen
//--------------------------------------------------------------------------------------
ATG::HELP_CALLOUT g_HelpCallouts[] =
{
    { ATG::HELP_A_BUTTON,     ATG::HELP_PLACEMENT_2, L"Switch\ncamera" },
    { ATG::HELP_B_BUTTON,     ATG::HELP_PLACEMENT_2, L"Ortho/\npersp camera" },
    { ATG::HELP_Y_BUTTON,     ATG::HELP_PLACEMENT_2, L"Reset\ncamera" },
    { ATG::HELP_X_BUTTON,     ATG::HELP_PLACEMENT_2, L"Isolate\ngroup" },
    { ATG::HELP_RIGHTSTICK,   ATG::HELP_PLACEMENT_1, L"Move camera" },
    { ATG::HELP_BACK_BUTTON,  ATG::HELP_PLACEMENT_2, L"Display\nhelp"  },
    { ATG::HELP_MISC_CALLOUT, ATG::HELP_PLACEMENT_1, L"Triggers zoom in/out" },
};
#define NUM_HELP_CALLOUTS (sizeof(g_HelpCallouts) / sizeof(g_HelpCallouts[0]))


//--------------------------------------------------------------------------------------
// Collision object structs - these hold a collision object and a bool flag indicating
// whether the object has collided with another object
//--------------------------------------------------------------------------------------
struct CollisionSphere
{
    ATG::Sphere m_Sphere;
    INT m_iCollision;
};

struct CollisionBox
{
    ATG::OrientedBox m_OBox;
    INT m_iCollision;
};

struct CollisionAABox
{
    ATG::AxisAlignedBox m_AABox;
    INT m_iCollision;
};

struct CollisionFrustum
{
    ATG::Frustum m_Frustum;
    INT m_iCollision;
};

struct CollisionTriangle
{
    XMVECTOR m_PointA;
    XMVECTOR m_PointB;
    XMVECTOR m_PointC;
    INT m_iCollision;
};

struct CollisionRay
{
    XMVECTOR m_Origin;
    XMVECTOR m_Direction;
};


//--------------------------------------------------------------------------------------
// Global constants that set up the collision groups and camera angles
//--------------------------------------------------------------------------------------
const INT   GROUP_COUNT = 4;
const INT   CAMERA_COUNT = 4;


//--------------------------------------------------------------------------------------
// Name: class Sample
// Desc: The Sample class implements the Collision sample.
//--------------------------------------------------------------------------------------
class Sample : public ATG::Application
{
public:
                        Sample();

    HRESULT             Initialize();
    HRESULT             Update();
    HRESULT             Render();

protected:
    VOID                InitializeObjects();
    VOID                Animate();
    VOID                Collide();
    VOID                UpdateCamera();
    VOID                RenderObjects();

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
    ATG::OrientedBox m_PrimaryOrientedBox;
    ATG::AxisAlignedBox m_PrimaryAABox;
    CollisionRay m_PrimaryRay;

    // Visual groups of collision objects - not used in any computation, just for aesthetics in the sample
    INT m_iIsolateGroup;

    // Secondary collision objects - these are animated and tested for collision with the primary objects
    CollisionSphere     m_SecondarySpheres[GROUP_COUNT];
    CollisionBox        m_SecondaryOrientedBoxes[GROUP_COUNT];
    CollisionAABox      m_SecondaryAABoxes[GROUP_COUNT];
    CollisionTriangle   m_SecondaryTriangles[GROUP_COUNT];

    // Ray testing results display object
    CollisionAABox m_RayHitResultBox;


    // Camera origins for each of the primary collision objects
    XMVECTOR            m_CameraOrigins[CAMERA_COUNT];

    // Spacing between cameras for each primary object
    FLOAT m_fCameraSpacing;
    BOOL m_bCameraOrtho;
    FLOAT m_fAspectRatio;
};


//--------------------------------------------------------------------------------------
// Name: Sample() constructor
//--------------------------------------------------------------------------------------
Sample::Sample() : m_fCameraSpacing( 50.0f ),
                   m_bCameraOrtho( FALSE ),
                   m_bDrawHelp( FALSE ),
                   m_fAspectRatio( 16.0f / 9.0f ),
                   m_iIsolateGroup( -1 )
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
    // Update the timer
    m_Timer.MarkFrame();

    // Grab input state from controllers
    ATG::Input::GetMergedInput();

    // Animate objects
    Animate();

    // Test and record all of the collisions
#ifdef  _DEBUG
    const int NumCollideTests = 1;
#else
    const int NumCollideTests = 3000;
#endif
    // Run the collision tests multiple times so that they
    // do a more realistic amount of work, so that they show up
    // on profiles better.
    for( int i = 0; i < NumCollideTests; ++i )
    {
        Collide();
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
    // Clear the backbuffer
    m_pd3dDevice->Clear( 0L, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL,
                         D3DCOLOR_ARGB( 0, 0, 0, 0 ), 1.0f, 0L );

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
        m_Font.DrawText( 0, 0, 0xffffffff, L"Collision" );
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
    const XMVECTOR XMZero = { 0, 0, 0, 0 };

    // Set up the primary frustum object from a D3D projection matrix
    // NOTE: This can also be done on your camera's projection matrix.  The projection
    // matrix built here is somewhat contrived so it renders well.
    XMMATRIX xmProj = XMMatrixPerspectiveFovLH( XM_PIDIV4, 1.77778f, 0.5f, 10.0f );
    ComputeFrustumFromProjection( &m_PrimaryFrustum, &xmProj );
    m_PrimaryFrustum.Origin.z = -7.0f;
    m_CameraOrigins[0] = XMVectorSet( 0, 0, 0, 0 );

    // Set up the primary axis aligned box
    m_PrimaryAABox.Center = XMFLOAT3( m_fCameraSpacing, 0, 0 );
    m_PrimaryAABox.Extents = XMFLOAT3( 5, 5, 5 );
    m_CameraOrigins[1] = XMVectorSet( m_fCameraSpacing, 0, 0, 0 );

    // Set up the primary oriented box with some rotation
    m_PrimaryOrientedBox.Center = XMFLOAT3( -m_fCameraSpacing, 0, 0 );
    m_PrimaryOrientedBox.Extents = XMFLOAT3( 5, 5, 5 );
    XMStoreFloat4( &m_PrimaryOrientedBox.Orientation, XMQuaternionRotationRollPitchYaw( XM_PIDIV4, XM_PIDIV4, 0 ) );
    m_CameraOrigins[2] = XMVectorSet( -m_fCameraSpacing, 0, 0, 0 );

    // Set up the primary ray
    m_PrimaryRay.m_Origin = XMVectorSet( 0, 0, m_fCameraSpacing, 0 );
    m_PrimaryRay.m_Direction = XMVectorSet( 0, 0, 1, 0 );
    m_CameraOrigins[3] = XMVectorSet( 0, 0, m_fCameraSpacing, 0 );

    // Initialize all of the secondary objects with default values
    for( UINT i = 0; i < GROUP_COUNT; i++ )
    {
        m_SecondarySpheres[i].m_Sphere.Radius = 1.0f;
        m_SecondarySpheres[i].m_Sphere.Center = XMFLOAT3( 0, 0, 0 );
        m_SecondarySpheres[i].m_iCollision = FALSE;

        m_SecondaryOrientedBoxes[i].m_OBox.Center = XMFLOAT3( 0, 0, 0 );
        m_SecondaryOrientedBoxes[i].m_OBox.Extents = XMFLOAT3( 0.5f, 0.5f, 0.5f );
        m_SecondaryOrientedBoxes[i].m_OBox.Orientation = XMFLOAT4( 0, 0, 0, 1 );
        m_SecondaryOrientedBoxes[i].m_iCollision = FALSE;

        m_SecondaryAABoxes[i].m_AABox.Center = XMFLOAT3( 0, 0, 0 );
        m_SecondaryAABoxes[i].m_AABox.Extents = XMFLOAT3( 0.5f, 0.5f, 0.5f );
        m_SecondaryAABoxes[i].m_iCollision = FALSE;

        m_SecondaryTriangles[i].m_PointA = XMZero;
        m_SecondaryTriangles[i].m_PointB = XMZero;
        m_SecondaryTriangles[i].m_PointC = XMZero;
        m_SecondaryTriangles[i].m_iCollision = FALSE;
    }

    // Set up ray hit result box
    m_RayHitResultBox.m_AABox.Center = XMFLOAT3( 0, 0, 0 );
    m_RayHitResultBox.m_AABox.Extents = XMFLOAT3( 0.05f, 0.05f, 0.05f );
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
    static UINT s_iCameraIndex = 0;
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
        s_iCameraIndex = ( s_iCameraIndex + 1 ) % CAMERA_COUNT;
        // Switching cameras disables the isolated group setting
        m_iIsolateGroup = -1;
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

    // X button isolates the current group, or shows all groups
    if( ATG::Input::m_DefaultGamepad.wPressedButtons & XINPUT_GAMEPAD_X )
    {
        if( m_iIsolateGroup == -1 )
            m_iIsolateGroup = s_iCameraIndex;
        else
            m_iIsolateGroup = -1;
    }

    // Back button toggles help display
    if( ATG::Input::m_DefaultGamepad.wPressedButtons & XINPUT_GAMEPAD_BACK )
    {
        m_bDrawHelp = !m_bDrawHelp;
    }

    // Smoothly lerp camera target as it changes
    static XMVECTOR s_CameraTarget = m_CameraOrigins[ s_iCameraIndex ];
    FLOAT LerpFactor = min( 4.0f * fDelta, 1.0f );
    s_CameraTarget = ( LerpFactor * m_CameraOrigins[ s_iCameraIndex ] ) +
        ( ( 1.0f - LerpFactor ) * s_CameraTarget );

    // Compute view matrix
    XMVECTOR vEyePt = XMVectorSet( s_fDistance * cosf( s_fPitch ) * sinf( s_fYaw ),
                                   s_fDistance * sinf( s_fPitch ),
                                   s_fDistance * cosf( s_fPitch ) * cosf( s_fYaw ), 0.0f );
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
// Name: Animate()
// Desc: Animates the collision objects.  This function doesn't really demonstrate any
//       particular Xbox APIs, but it sets up some nice collision test cases.
//--------------------------------------------------------------------------------------
VOID Sample::Animate()
{
    float t = ( FLOAT )m_Timer.GetAbsoluteTime() * 0.2f;

    const float camera0OriginX = XMVectorGetX( m_CameraOrigins[0] );
    const float camera1OriginX = XMVectorGetX( m_CameraOrigins[1] );
    const float camera2OriginX = XMVectorGetX( m_CameraOrigins[2] );
    const float camera3OriginX = XMVectorGetX( m_CameraOrigins[3] );
    const float camera3OriginZ = XMVectorGetZ( m_CameraOrigins[3] );

    // animate sphere 0 around the frustum
    m_SecondarySpheres[0].m_Sphere.Center.x = 10 * sinf( 3 * t );
    m_SecondarySpheres[0].m_Sphere.Center.y = 7 * cosf( 5 * t );

    // animate oriented box 0 around the frustum
    m_SecondaryOrientedBoxes[0].m_OBox.Center.x = 8 * sinf( 3.5f * t );
    m_SecondaryOrientedBoxes[0].m_OBox.Center.y = 5 * cosf( 5.1f * t );
    XMStoreFloat4( &( m_SecondaryOrientedBoxes[0].m_OBox.Orientation ), XMQuaternionRotationRollPitchYaw( t * 1.4f,
                                                                                                          t * 0.2f,
                                                                                                          t ) );

    // animate aligned box 0 around the frustum
    m_SecondaryAABoxes[0].m_AABox.Center.x = 10 * sinf( 2.1f * t );
    m_SecondaryAABoxes[0].m_AABox.Center.y = 7 * cosf( 3.8f * t );

    // animate sphere 1 around the aligned box
    m_SecondarySpheres[1].m_Sphere.Center.x = 8 * sinf( 2.9f * t ) + camera1OriginX;
    m_SecondarySpheres[1].m_Sphere.Center.y = 8 * cosf( 4.6f * t );
    m_SecondarySpheres[1].m_Sphere.Center.z = 8 * cosf( 1.6f * t );
  
    // animate oriented box 1 around the aligned box
    m_SecondaryOrientedBoxes[1].m_OBox.Center.x = 8 * sinf( 3.2f * t ) + camera1OriginX;
    m_SecondaryOrientedBoxes[1].m_OBox.Center.y = 8 * cosf( 2.1f * t );
    m_SecondaryOrientedBoxes[1].m_OBox.Center.z = 8 * sinf( 1.6f * t );
    XMStoreFloat4( &( m_SecondaryOrientedBoxes[1].m_OBox.Orientation ), XMQuaternionRotationRollPitchYaw( t * 0.7f,
                                                                                                          t * 1.3f,
                                                                                                          t ) );

    // animate aligned box 1 around the aligned box
    m_SecondaryAABoxes[1].m_AABox.Center.x = 8 * sinf( 1.1f * t ) + camera1OriginX;
    m_SecondaryAABoxes[1].m_AABox.Center.y = 8 * cosf( 5.8f * t );
    m_SecondaryAABoxes[1].m_AABox.Center.z = 8 * cosf( 3.0f * t );

    // animate sphere 2 around the oriented box
    m_SecondarySpheres[2].m_Sphere.Center.x = 8 * sinf( 2.2f * t ) + camera2OriginX;
    m_SecondarySpheres[2].m_Sphere.Center.y = 8 * cosf( 4.3f * t );
    m_SecondarySpheres[2].m_Sphere.Center.z = 8 * cosf( 1.8f * t );

    // animate oriented box 2 around the oriented box
    m_SecondaryOrientedBoxes[2].m_OBox.Center.x = 8 * sinf( 3.7f * t ) + camera2OriginX;
    m_SecondaryOrientedBoxes[2].m_OBox.Center.y = 8 * cosf( 2.5f * t );
    m_SecondaryOrientedBoxes[2].m_OBox.Center.z = 8 * sinf( 1.1f * t );
    XMStoreFloat4( &( m_SecondaryOrientedBoxes[2].m_OBox.Orientation ), XMQuaternionRotationRollPitchYaw( t * 0.9f,
                                                                                                          t * 1.8f,
                                                                                                          t ) );

    // animate aligned box 2 around the oriented box
    m_SecondaryAABoxes[2].m_AABox.Center.x = 8 * sinf( 1.3f * t ) + camera2OriginX;
    m_SecondaryAABoxes[2].m_AABox.Center.y = 8 * cosf( 5.2f * t );
    m_SecondaryAABoxes[2].m_AABox.Center.z = 8 * cosf( 3.5f * t );

    // triangle points in local space - equilateral triangle with radius of 2
    const XMVECTOR TrianglePointA = { 0, 2, 0, 0 };
    const XMVECTOR TrianglePointB = { 1.732f, -1, 0, 0 };
    const XMVECTOR TrianglePointC = { -1.732f, -1, 0, 0 };

    // animate triangle 0 around the frustum
    XMMATRIX TriangleCoords = XMMatrixRotationRollPitchYaw( t * 1.4f, t * 2.5f, t );
    XMMATRIX Translation = XMMatrixTranslation( 5 * sinf( 5.3f * t ) + camera0OriginX,
                                                5 * cosf( 2.3f * t ),
                                                5 * sinf( 3.4f * t ) );
    TriangleCoords = XMMatrixMultiply( TriangleCoords, Translation );
    m_SecondaryTriangles[0].m_PointA = XMVector3Transform( TrianglePointA, TriangleCoords );
    m_SecondaryTriangles[0].m_PointB = XMVector3Transform( TrianglePointB, TriangleCoords );
    m_SecondaryTriangles[0].m_PointC = XMVector3Transform( TrianglePointC, TriangleCoords );

    // animate triangle 1 around the aligned box
    TriangleCoords = XMMatrixRotationRollPitchYaw( t * 1.4f, t * 2.5f, t );
    Translation = XMMatrixTranslation( 8 * sinf( 5.3f * t ) + camera1OriginX,
                                       8 * cosf( 2.3f * t ),
                                       8 * sinf( 3.4f * t ) );
    TriangleCoords = XMMatrixMultiply( TriangleCoords, Translation );
    m_SecondaryTriangles[1].m_PointA = XMVector3Transform( TrianglePointA, TriangleCoords );
    m_SecondaryTriangles[1].m_PointB = XMVector3Transform( TrianglePointB, TriangleCoords );
    m_SecondaryTriangles[1].m_PointC = XMVector3Transform( TrianglePointC, TriangleCoords );

    // animate triangle 2 around the oriented box
    TriangleCoords = XMMatrixRotationRollPitchYaw( t * 1.4f, t * 2.5f, t );
    Translation = XMMatrixTranslation( 8 * sinf( 5.3f * t ) + camera2OriginX,
                                       8 * cosf( 2.3f * t ),
                                       8 * sinf( 3.4f * t ) );
    TriangleCoords = XMMatrixMultiply( TriangleCoords, Translation );
    m_SecondaryTriangles[2].m_PointA = XMVector3Transform( TrianglePointA, TriangleCoords );
    m_SecondaryTriangles[2].m_PointB = XMVector3Transform( TrianglePointB, TriangleCoords );
    m_SecondaryTriangles[2].m_PointC = XMVector3Transform( TrianglePointC, TriangleCoords );

    // animate primary ray (this is the only animated primary object)
    m_PrimaryRay.m_Direction = XMVectorSet( sinf( t * 3 ), 0, cosf( t * 3 ), 0 );

    // animate sphere 3 around the ray
    m_SecondarySpheres[3].m_Sphere.Center = XMFLOAT3( camera3OriginX - 3,
                                                      0.5f * sinf( t * 5 ),
                                                      camera3OriginZ );

    // animate aligned box 3 around the ray
    m_SecondaryAABoxes[3].m_AABox.Center = XMFLOAT3( camera3OriginX + 3,
                                                     0.5f * sinf( t * 4 ),
                                                     camera3OriginZ );

    // animate oriented box 3 around the ray
    m_SecondaryOrientedBoxes[3].m_OBox.Center = XMFLOAT3( camera3OriginX,
                                                          0.5f * sinf( t * 4.5f ),
                                                          camera3OriginZ + 3 );
    XMStoreFloat4( &( m_SecondaryOrientedBoxes[3].m_OBox.Orientation ), XMQuaternionRotationRollPitchYaw( t * 0.9f,
                                                                                                          t * 1.8f,
                                                                                                          t ) );

    // animate triangle 3 around the ray
    TriangleCoords = XMMatrixRotationRollPitchYaw( t * 1.4f, t * 2.5f, t );
    Translation = XMMatrixTranslation( camera3OriginX,
                                       0.5f * cosf( 4.3f * t ),
                                       camera3OriginZ - 3 );
    TriangleCoords = XMMatrixMultiply( TriangleCoords, Translation );
    m_SecondaryTriangles[3].m_PointA = XMVector3Transform( TrianglePointA, TriangleCoords );
    m_SecondaryTriangles[3].m_PointB = XMVector3Transform( TrianglePointB, TriangleCoords );
    m_SecondaryTriangles[3].m_PointC = XMVector3Transform( TrianglePointC, TriangleCoords );
}


//--------------------------------------------------------------------------------------
// Name: Collide()
// Desc: Tests collisions between pairs of collision objects
//       These functions plus a few more in common/AtgCollision.h make up the
//       collision library.
//--------------------------------------------------------------------------------------
VOID Sample::Collide()
{
    // test collisions between objects and frustum
    m_SecondarySpheres[0].m_iCollision = ATG::IntersectSphereFrustum( &m_SecondarySpheres[0].m_Sphere,
                                                                      &m_PrimaryFrustum );
    m_SecondaryOrientedBoxes[0].m_iCollision = ATG::IntersectOrientedBoxFrustum( &m_SecondaryOrientedBoxes[0].m_OBox,
                                                                                 &m_PrimaryFrustum );
    m_SecondaryAABoxes[0].m_iCollision = ATG::IntersectAxisAlignedBoxFrustum( &m_SecondaryAABoxes[0].m_AABox,
                                                                              &m_PrimaryFrustum );
    m_SecondaryTriangles[0].m_iCollision = ATG::IntersectTriangleFrustum( m_SecondaryTriangles[0].m_PointA,
                                                                          m_SecondaryTriangles[0].m_PointB,
                                                                          m_SecondaryTriangles[0].m_PointC,
                                                                          &m_PrimaryFrustum );

    // test collisions between objects and aligned box
    m_SecondarySpheres[1].m_iCollision = ATG::IntersectSphereAxisAlignedBox( &m_SecondarySpheres[1].m_Sphere,
                                                                             &m_PrimaryAABox );
    m_SecondaryOrientedBoxes[1].m_iCollision = ATG::IntersectAxisAlignedBoxOrientedBox
        ( &m_PrimaryAABox, &m_SecondaryOrientedBoxes[1].m_OBox );
    m_SecondaryAABoxes[1].m_iCollision = ATG::IntersectAxisAlignedBoxAxisAlignedBox( &m_SecondaryAABoxes[1].m_AABox,
                                                                                     &m_PrimaryAABox );
    m_SecondaryTriangles[1].m_iCollision = ATG::IntersectTriangleAxisAlignedBox( m_SecondaryTriangles[1].m_PointA,
                                                                                 m_SecondaryTriangles[1].m_PointB,
                                                                                 m_SecondaryTriangles[1].m_PointC,
                                                                                 &m_PrimaryAABox );

    // test collisions between objects and oriented box
    m_SecondarySpheres[2].m_iCollision = ATG::IntersectSphereOrientedBox( &m_SecondarySpheres[2].m_Sphere,
                                                                          &m_PrimaryOrientedBox );
    m_SecondaryOrientedBoxes[2].m_iCollision = ATG::IntersectOrientedBoxOrientedBox( &m_SecondaryOrientedBoxes[2].m_OBox, &m_PrimaryOrientedBox );
    m_SecondaryAABoxes[2].m_iCollision = ATG::IntersectAxisAlignedBoxOrientedBox( &m_SecondaryAABoxes[2].m_AABox,
                                                                                  &m_PrimaryOrientedBox );
    m_SecondaryTriangles[2].m_iCollision = ATG::IntersectTriangleOrientedBox( m_SecondaryTriangles[2].m_PointA,
                                                                              m_SecondaryTriangles[2].m_PointB,
                                                                              m_SecondaryTriangles[2].m_PointC,
                                                                              &m_PrimaryOrientedBox );

    // test collisions between objects and ray
    FLOAT fDistance = -1.0f;
    m_SecondarySpheres[3].m_iCollision = ATG::IntersectRaySphere( m_PrimaryRay.m_Origin,
                                                                  m_PrimaryRay.m_Direction,
                                                                  &m_SecondarySpheres[3].m_Sphere,
                                                                  &fDistance );
    m_SecondaryOrientedBoxes[3].m_iCollision = ATG::IntersectRayOrientedBox( m_PrimaryRay.m_Origin,
                                                                             m_PrimaryRay.m_Direction,
                                                                             &m_SecondaryOrientedBoxes[3].m_OBox,
                                                                             &fDistance );
    m_SecondaryAABoxes[3].m_iCollision = ATG::IntersectRayAxisAlignedBox( m_PrimaryRay.m_Origin,
                                                                          m_PrimaryRay.m_Direction,
                                                                          &m_SecondaryAABoxes[3].m_AABox,
                                                                          &fDistance );
    m_SecondaryTriangles[3].m_iCollision = ATG::IntersectRayTriangle( m_PrimaryRay.m_Origin,
                                                                      m_PrimaryRay.m_Direction,
                                                                      m_SecondaryTriangles[3].m_PointA,
                                                                      m_SecondaryTriangles[3].m_PointB,
                                                                      m_SecondaryTriangles[3].m_PointC,
                                                                      &fDistance );

    // If one of the ray intersection tests was successful, fDistance will be positive.
    // If so, compute the intersection location and store it in m_RayHitResultBox.
    if( fDistance > 0 )
    {
        // The primary ray's direction is assumed to be normalized.
        XMVECTOR HitLocation = XMVectorMultiplyAdd( m_PrimaryRay.m_Direction, XMVectorReplicate( fDistance ),
                                                    m_PrimaryRay.m_Origin );
        XMStoreFloat3( &m_RayHitResultBox.m_AABox.Center, HitLocation );
        m_RayHitResultBox.m_iCollision = TRUE;
    }
    else
    {
        m_RayHitResultBox.m_iCollision = FALSE;
    }
}


//--------------------------------------------------------------------------------------
// Name: GetCollisionColor()
// Desc: Returns the correct collision color based on the collision result value and the
//       group number.  Frustum tests (group 0) return 0, 1, or 2 for outside, partially
//       inside, and fully inside; all other tests return 0 or 1 for no collision or
//       collision.
//--------------------------------------------------------------------------------------
D3DCOLOR GetCollisionColor( INT iCollisionValue, INT iGroupNumber )
{
    const D3DCOLOR ColorCollide = 0xFFFF0000;
    const D3DCOLOR ColorPartialCollide = 0xFFFFFF00;
    const D3DCOLOR ColorNoCollide = 0xFF80C080;

    // special case: a value of 1 for groups 1 and higher needs to register as a full collision
    if( iGroupNumber > 0 && iCollisionValue > 0 )
        iCollisionValue = 2;

    switch( iCollisionValue )
    {
        case 0:
            return ColorNoCollide;
        case 1:
            return ColorPartialCollide;
        case 2:
        default:
            return ColorCollide;
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
    const D3DCOLOR ColorYellow = 0xFFFFFF00;

    // Draw ground planes
    for( INT i = 0; i < CAMERA_COUNT; i++ )
    {
        // skip non-isolated groups if the isolated group is valid
        if( m_iIsolateGroup != -1 && i != m_iIsolateGroup )
            continue;

        XMFLOAT3 vXAxis( 20, 0,  0 );
        XMFLOAT3 vYAxis( 0, 0, 20 );
        XMFLOAT3 vOrigin;
        XMStoreFloat3( &vOrigin, m_CameraOrigins[i] );
        vOrigin.y -= 10;
        int iXDivisions = 20;
        int iYDivisions = 20;
        ATG::DebugDraw::DrawGrid( vXAxis, vYAxis, vOrigin, iXDivisions, iYDivisions, ColorGround );
    }

    // Draw primary collision objects in white
    if( m_iIsolateGroup == -1 || m_iIsolateGroup == 0 )
        ATG::DebugDraw::DrawFrustum( m_PrimaryFrustum, ColorWhite );
    if( m_iIsolateGroup == -1 || m_iIsolateGroup == 1 )
        ATG::DebugDraw::DrawAabb( m_PrimaryAABox, ColorWhite );
    if( m_iIsolateGroup == -1 || m_iIsolateGroup == 2 )
        ATG::DebugDraw::DrawObb( m_PrimaryOrientedBox, ColorWhite );
    if( m_iIsolateGroup == -1 || m_iIsolateGroup == 3 )
    {
        XMFLOAT3 Origin;
        XMFLOAT3 Direction;
        XMStoreFloat3( &Origin, m_PrimaryRay.m_Origin );
        XMStoreFloat3( &Direction, XMVectorScale( m_PrimaryRay.m_Direction, 10.0f ) );
        ATG::DebugDraw::DrawRay( Origin, Direction, FALSE, 0xFF505050 );
        XMStoreFloat3( &Direction, m_PrimaryRay.m_Direction );
        ATG::DebugDraw::DrawRay( Origin, Direction, FALSE, ColorWhite );
    }

    // Draw secondary collision objects in colors based on collision results
    for( INT i = 0; i < GROUP_COUNT; i++ )
    {
        // skip non-isolated groups if the isolated group is valid
        if( m_iIsolateGroup != -1 && i != m_iIsolateGroup )
            continue;

        D3DCOLOR c = GetCollisionColor( m_SecondarySpheres[i].m_iCollision, i );
        ATG::DebugDraw::DrawSphere( m_SecondarySpheres[i].m_Sphere, c );

        c = GetCollisionColor( m_SecondaryOrientedBoxes[i].m_iCollision, i );
        ATG::DebugDraw::DrawObb( m_SecondaryOrientedBoxes[i].m_OBox, c );

        c = GetCollisionColor( m_SecondaryAABoxes[i].m_iCollision, i );
        ATG::DebugDraw::DrawAabb( m_SecondaryAABoxes[i].m_AABox, c );

        c = GetCollisionColor( m_SecondaryTriangles[i].m_iCollision, i );
        XMFLOAT3 Verts[3];
        XMStoreFloat3( &Verts[0], m_SecondaryTriangles[i].m_PointA );
        XMStoreFloat3( &Verts[1], m_SecondaryTriangles[i].m_PointB );
        XMStoreFloat3( &Verts[2], m_SecondaryTriangles[i].m_PointC );
        ATG::DebugDraw::DrawTriangle( Verts[0], Verts[1], Verts[2], c );
    }

    // Draw results of ray-object intersection, if there was a hit this frame
    if( m_iIsolateGroup == -1 || m_iIsolateGroup == 3 )
    {
        if( m_RayHitResultBox.m_iCollision )
        {
            ATG::DebugDraw::DrawAabb( m_RayHitResultBox.m_AABox, ColorYellow );
        }
    }
}


//--------------------------------------------------------------------------------------
// Name: main()
// Desc: Entry point to the program
//--------------------------------------------------------------------------------------
VOID __cdecl main()
{
    Sample sample;

    D3DPRESENT_PARAMETERS& d3dpp = sample.m_d3dpp;
    ZeroMemory( &d3dpp, sizeof( D3DPRESENT_PARAMETERS ) );
    d3dpp.BackBufferWidth = 1280;
    d3dpp.BackBufferHeight = 720;
    d3dpp.BackBufferFormat = ( D3DFORMAT )MAKESRGBFMT( D3DFMT_A8R8G8B8 );
    d3dpp.MultiSampleType = D3DMULTISAMPLE_NONE;
    d3dpp.MultiSampleQuality = 0;
    d3dpp.BackBufferCount = 1;
    d3dpp.EnableAutoDepthStencil = TRUE;
    d3dpp.AutoDepthStencilFormat = D3DFMT_D24S8;
    d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_ONE;
    d3dpp.FrontBufferFormat = ( D3DFORMAT )MAKESRGBFMT( D3DFMT_LE_X8R8G8B8 );

    sample.Run();
}

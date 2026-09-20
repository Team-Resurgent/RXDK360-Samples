//--------------------------------------------------------------------------------------
// Viewer.cpp
//
// The NuiViewer sample demonstrates how to implement functionality similar to that 
// found in the "NuiView" application.
//
// Advanced Technology Group
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include <xtl.h>
#include <xnamath.h>
#include <xgraphics.h>
#include <AtgApp.h>
#include <AtgFont.h>
#include <AtgHelp.h>
#include <AtgInput.h>
#include <AtgResource.h>
#include <AtgUtil.h>
#include <AtgSimpleShaders.h>
#include <AtgNuiVisualization.h>
#include <AtgNuiCommon.h>
#include <AtgDebugDraw.h>
#include <AtgCamera.h>
#include <NuiApi.h>

//--------------------------------------------------------------------------------------
// Callouts for labelling the gamepad on the help screen
//--------------------------------------------------------------------------------------
ATG::HELP_CALLOUT g_HelpCallouts[] = 
{
    { ATG::HELP_BACK_BUTTON, ATG::HELP_PLACEMENT_2, L"Display\nhelp" },
    { ATG::HELP_START_BUTTON, ATG::HELP_PLACEMENT_1, L"Pause" },
    { ATG::HELP_LEFTSTICK, ATG::HELP_PLACEMENT_1, L"Move Camera" },
    { ATG::HELP_RIGHTSTICK, ATG::HELP_PLACEMENT_1, L"Rotate Camera" },
    { ATG::HELP_A_BUTTON, ATG::HELP_PLACEMENT_2, L"Toggle seated/\nstanding pipeline" },
    { ATG::HELP_LEFT_SHOULDER, ATG::HELP_PLACEMENT_2, L"Previous Display\nTechnique" },
    { ATG::HELP_RIGHT_SHOULDER, ATG::HELP_PLACEMENT_2, L"Next Display\nTechnique" }
};
const DWORD NUM_HELP_CALLOUTS = ARRAYSIZE(g_HelpCallouts);

//---------------------------------------------------------------------------------------------------------
// Constant declarations
//---------------------------------------------------------------------------------------------------------
const INT g_nDepthWidth    = 320;
const INT g_nDepthHeight   = 240;
const INT g_nColorWidth    = 640;
const INT g_nColorHeight   = 480;

const DWORD g_dwNaN = 0x7FBFFFFF;   // Define NaN by bit-pattern. This gets converted to a float value later
                                    // and used in a shader to discard quads.

//---------------------------------------------------------------------------------------------------------
// Flags for rendering modes
//---------------------------------------------------------------------------------------------------------
enum RENDER_MODE
{
    RENDER_DEPTH_LIT            = 0x00000001,
    RENDER_DEPTH_COLOR          = 0x00000002,
    RENDER_EXCLUDE_BACKGROUND   = 0x00000004,
    RENDER_SKELETON             = 0x00000008
};
const DWORD g_dwDepthModeMask = 0x00000003;
const DWORD g_dwExcludeBackgroundMask = 0x00000004;
const DWORD g_dwSkeletonModeMask = 0x00000008;

//---------------------------------------------------------------------------------------------------------
// List of different rendering modes, that may be cycled through.
//---------------------------------------------------------------------------------------------------------
const UINT32 g_RenderModes[] = 
{
    RENDER_DEPTH_LIT,
    RENDER_DEPTH_COLOR,
    RENDER_DEPTH_LIT | RENDER_EXCLUDE_BACKGROUND,
    RENDER_DEPTH_COLOR | RENDER_EXCLUDE_BACKGROUND,
    RENDER_SKELETON,
    RENDER_DEPTH_LIT | RENDER_SKELETON,
    RENDER_DEPTH_COLOR | RENDER_SKELETON,
    RENDER_DEPTH_LIT | RENDER_SKELETON | RENDER_EXCLUDE_BACKGROUND,
    RENDER_DEPTH_COLOR | RENDER_SKELETON | RENDER_EXCLUDE_BACKGROUND,
};

//---------------------------------------------------------------------------------------------------------
// List of titles to display with rendering modes.
//---------------------------------------------------------------------------------------------------------
const WCHAR * g_RenderModeTitles[] = 
{
    L"Lit Depth Mesh",
    L"Depth Mesh With Color",
    L"Lit Depth Mesh, Background Removed",
    L"Depth Mesh With Color, Background Removed",
    L"Skeleton Only",
    L"Lit Depth Mesh + Skeleton",
    L"Depth Mesh With Color + Skeleton",
    L"Lit Depth Mesh + Skeleton, Background Removed",
    L"Depth Mesh With Color, Background Removed",
};

//---------------------------------------------------------------------------------------------------------
// Vertex structure used for skeleton components.
//---------------------------------------------------------------------------------------------------------
struct SkeletonMeshVertex
{
    FLOAT x, y, z;
    FLOAT nx, ny, nz;
};

//--------------------------------------------------------------------------------------
// Name: class Sample
// Desc: Main class to run this application. Most functionality is inherited
//       from the ATG::Application base class.
//--------------------------------------------------------------------------------------
class Sample : public ATG::Application
{
    ATG::Timer                  m_Timer;
    ATG::Font                   m_Font;
    ATG::Help                   m_Help;
    BOOL                        m_bDrawHelp;
    BOOL                        m_bPaused;

    // Picture-in-picture NUI vizualization
    ATG::NuiVisualization       m_pip;

    // Nui Handles
    HANDLE                      m_hDepthDoneEvent;
    HANDLE                      m_hSkeletonDoneEvent;
    HANDLE                      m_hImage;
    HANDLE                      m_hDepth;

    const NUI_IMAGE_FRAME *     m_pImageFrame;
    const NUI_IMAGE_FRAME *     m_pDepthFrame;

    // Cylinder vertex buffers (for skeleton rendering)
    IDirect3DVertexBuffer9 *    m_pVBCylinder;
    IDirect3DIndexBuffer9 *     m_pIBCylinder;
    UINT32                      m_nCylinderTriangleCount;

    // Skeleton data
    NUI_SKELETON_FRAME          m_SkeletonFrame;
    DWORD                       m_dwTrackingFlags;

    // Shaders
    LPDIRECT3DVERTEXDECLARATION9 m_pSkeletonGeometryVertexDecl;
    LPDIRECT3DVERTEXSHADER9     m_pDepthMeshVS;
    LPDIRECT3DPIXELSHADER9      m_pTexturedPS;
    LPDIRECT3DPIXELSHADER9      m_pBasicLitPS;
    LPDIRECT3DVERTEXSHADER9     m_pSkeletonVS;
    LPDIRECT3DPIXELSHADER9      m_pSkeletonPS;

    // Camera variables
    FLOAT                       m_fAspectRatio;
    XMMATRIX                    m_matWorld;
    XMMATRIX                    m_matCameraView;
    XMMATRIX                    m_matCameraProj;
    XMVECTOR                    m_vPosition;
    XMVECTOR                    m_vVelocity;
    FLOAT                       m_fYaw;
    FLOAT                       m_fYawVelocity;
    FLOAT                       m_fPitch;
    FLOAT                       m_fPitchVelocity;

    // NUI FOV values
    FLOAT                       m_fNuiHorizontalFOV;
    FLOAT                       m_fNuiVerticalFOV;

    // Current rendering mode index (into the array of available modes)
    UINT32                      m_nRenderModeIndex;

    XMVECTOR                    m_vLightDir;

private:

    HRESULT CreateSkeletonGeometry();
    HRESULT Initialize();
    HRESULT Update();
    HRESULT Render();
    HRESULT RenderSkeleton();

    HRESULT InitializeNui();
};

//--------------------------------------------------------------------------------------
// Name: main()
// Desc: Entry point to the program
//--------------------------------------------------------------------------------------
VOID __cdecl main()
{
    Sample atgApp;
    ATG::GetVideoSettings( &atgApp.m_d3dpp.BackBufferWidth, &atgApp.m_d3dpp.BackBufferHeight );

    atgApp.m_d3dpp.BackBufferFormat = ( D3DFORMAT )MAKESRGBFMT( D3DFMT_A8R8G8B8 );
    atgApp.m_d3dpp.FrontBufferFormat = ( D3DFORMAT )MAKESRGBFMT( D3DFMT_LE_X8R8G8B8 );
    atgApp.m_d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_TWO;
    atgApp.Run();
}

//--------------------------------------------------------------------------------------
// Name: Sample::CreateSkeletonGeometry
// Desc: Creates the cylinder used to represent the NUI skeleton. The cylinder is created
// with unit dimensions, and is scaled during rendering to create the skeleton bones.
//--------------------------------------------------------------------------------------
HRESULT Sample::CreateSkeletonGeometry()
{
    static const D3DVERTEXELEMENT9 decl[] = 
    {
        { 0, 0,  D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
        { 0, 12, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_NORMAL, 0 },
        D3DDECL_END()
    };

    // Number of slices to use in the cylinder mesh
    static const UINT nSlices = 16;

    // Create the vertex declaration to use for cylinder rendering
    HRESULT hr = m_pd3dDevice->CreateVertexDeclaration( decl, &m_pSkeletonGeometryVertexDecl );
    if( FAILED( hr ) )
    {
        ATG_PrintError( "Failed to create skeleton mesh vertex declaration!" );
        return hr;
    }

    // Create the vertex and index buffers for the cylinder used in skeleton rendering.
    hr = m_pd3dDevice->CreateVertexBuffer( ( sizeof( SkeletonMeshVertex ) * ( ( nSlices * 4 ) + 2 ) ), 0, 0, 0, &m_pVBCylinder, NULL );
    if( FAILED( hr ) )
    {
        ATG_PrintError( "Failed to create cylinder vertex buffer!" );
        return hr;
    }

    hr = m_pd3dDevice->CreateIndexBuffer( sizeof( WORD ) * nSlices * 4 * 3, 0, D3DFMT_INDEX16, 0, &m_pIBCylinder, NULL );
    if( FAILED( hr ) )
    {
        ATG_PrintError( "Failed to create cylinder index buffer!" );
        return hr;
    }

    // Generate the vertices.
    SkeletonMeshVertex * aVertices;
    hr = m_pVBCylinder->Lock( 0, 0, ( void ** )&aVertices, 0 );
    if( FAILED( hr ) )
    {
        return hr;
    }

    // Generate the cylinder sides. We'll create 4 sets of vertices, two sets for each end. For each
    // end, one set has normals facing outward from the rounded edges, the other has normals
    // facing outward from the end. 
    for( UINT32 i = 0; i < nSlices; ++i )
    {
        FLOAT fTheta = ( FLOAT )i / nSlices * 2 * XM_PI;

        float x = cosf( fTheta );
        float y = sinf( fTheta );

        for( UINT32 j = 0; j < nSlices * 4; j += nSlices )
        {
            aVertices[ i + j ].x = x;
            aVertices[ i + j ].y = y;

            if( j < nSlices * 2 )
            {
                aVertices[ i + j ].nx = x;
                aVertices[ i + j ].ny = y;
                aVertices[ i + j ].nz = 0;
            }
            else
            {
                aVertices[ i + j ].nx = aVertices[ i + j ].ny = 0;
                if( j > nSlices * 2 )
                {
                    aVertices[ i + j ].nz = -1;
                }
                else
                {
                    aVertices[ i + j ].nz = 1;
                }
            }
        }

        aVertices[ i ].z = 1.0f;
        aVertices[ i + nSlices ].z = 0;
        aVertices[ i + ( nSlices * 2 ) ].z = 1.0f;
        aVertices[ i + ( nSlices * 3 ) ].z = 0;
    }

    // Generate the vertices at the cylinder center.
    SkeletonMeshVertex * aVertex = &aVertices[ nSlices * 4 ];
    aVertex->x = aVertex->y = 0;
    aVertex->z = 1;
    aVertex->nx = aVertex->ny = 0;
    aVertex->nz = 1.0f;
    ++aVertex;
    aVertex->x = aVertex->y = 0;
    aVertex->z = 0;
    aVertex->nx = aVertex->ny = 0;
    aVertex->nz = -1.0f;

    m_pVBCylinder->Unlock();

    // Generate the cylinder indices.
    WORD * aIndices;
    hr = m_pIBCylinder->Lock( 0, 0, ( void ** )&aIndices, 0 );
    if( FAILED( hr ) )
    {
        return hr;
    }

    // Generate triangles for the cylinder sides
    for( WORD i = 0; i < ( WORD )nSlices; ++i )
    {
        UINT32 nRootIndex = i * 6;
        // Triangle 1.
        aIndices[ nRootIndex ] = i;
        aIndices[ nRootIndex + 1 ] = i + ( WORD )nSlices;
        aIndices[ nRootIndex + 2 ] = i < nSlices-1 ? i + 1 : 0;

        // Triangle 2.
        aIndices[ nRootIndex + 3 ] = i < nSlices-1 ? i + 1 : 0;
        aIndices[ nRootIndex + 4 ] = i + ( WORD )nSlices;
        aIndices[ nRootIndex + 5 ] = i < nSlices-1 ? i + ( WORD )nSlices + 1 : ( WORD )nSlices;
    }

    // Generate triangles for the caps
    WORD nBase1 = ( WORD )nSlices * 2;
    WORD nBase2 = ( WORD )nSlices * 3;
    WORD nCenter1 = ( WORD )nSlices * 4;
    WORD nCenter2 = ( WORD )nCenter1 + 1;
    for( UINT32 i = 0; i < nSlices; ++i )
    {
        UINT32 nRootIndex1 = ( WORD )nSlices * 6 + i * 3;
        UINT32 nRootIndex2 = ( WORD )nSlices * 9 + i * 3;

        aIndices[ nRootIndex1 ] = nCenter1;
        aIndices[ nRootIndex1 + 1 ] = nBase1;
        aIndices[ nRootIndex1 + 2 ] = i < nSlices - 1? nBase1 + 1 : ( WORD )nSlices * 2;

        aIndices[ nRootIndex2 ] = i < nSlices - 1? nBase2 + 1 : ( WORD )nSlices * 3;
        aIndices[ nRootIndex2 + 1 ] = nBase2;
        aIndices[ nRootIndex2 + 2 ] = nCenter2;

        ++nBase1;
        ++nBase2;
    }

    m_pIBCylinder->Unlock();

    m_nCylinderTriangleCount = nSlices * 4;

    return S_OK;
}

//--------------------------------------------------------------------------------------
// Name: InitializeNui()
// Desc: Initialize the NUI camera
//--------------------------------------------------------------------------------------
HRESULT Sample::InitializeNui()
{
    HRESULT hr = S_OK;

    m_hSkeletonDoneEvent = CreateEvent( NULL,
                                        FALSE,  // Auto-reset
                                        FALSE,  // Initially unsignalled
                                        "NuiSkeletonDoneEvent" );
    if( NULL == m_hSkeletonDoneEvent )
    {
        ATG_PrintError( "Initialization of NuiSkeletonDoneEvent failed\n" );
        return E_FAIL;
    }

    m_hDepthDoneEvent = CreateEvent( NULL,
                                     FALSE,  // Auto-reset
                                     FALSE,  // Initially unsignalled
                                     "NuiDepthDoneEvent" );
    if( NULL == m_hDepthDoneEvent )
    {
        ATG_PrintError( "Initialization of NuiDepthDoneEvent failed\n" );
        return E_FAIL;
    }

    // Initialize the Natural Input System on the default thread.
    hr = NuiInitialize( NUI_INITIALIZE_FLAG_USES_COLOR | 
                        NUI_INITIALIZE_FLAG_USES_DEPTH_AND_PLAYER_INDEX | 
                        NUI_INITIALIZE_FLAG_USES_SKELETON |
                        NUI_INITIALIZE_FLAG_EXTRAPOLATE_FLOOR_PLANE,
                        NUI_INITIALIZE_DEFAULT_HARDWARE_THREAD );
    if( FAILED( hr ) )
    {
        ATG::NuiPrintError( hr, "NuiInitialize" );
        return hr;
    }

    // Open the color stream
    hr = NuiImageStreamOpen( NUI_IMAGE_TYPE_COLOR_IN_DEPTH_SPACE,   // Get image data in depth-space
                             NUI_IMAGE_RESOLUTION_640x480,          // Get image at 640x480 
                             0,                                     // No flags
                             2,                                     // Buffer 2 frames of image data
                             NULL,                                  // We don't use a next-frame event
                             &m_hImage );                           // Image handle
    if( FAILED( hr ) )
    {
        ATG::NuiPrintError( hr, "NuiImageStreamOpen" );
        return hr;
    }

    // Open the depth stream
    hr = NuiImageStreamOpen( NUI_IMAGE_TYPE_DEPTH_AND_PLAYER_INDEX, // Get depth and player (segmentation) info
                             NUI_IMAGE_RESOLUTION_320x240,          // Get depth at 320x240
                             0,                                     // No flags
                             2,                                     // Buffer 2 frames of depth data
                             m_hDepthDoneEvent,                    // We don't use a next-frame event
                             &m_hDepth );                           // Image handle
    if( FAILED( hr ) )
    {
        ATG::NuiPrintError( hr, "NuiImageStreamOpen" );
        return hr;
    }

    m_pImageFrame = NULL;
    m_pDepthFrame = NULL;

    m_dwTrackingFlags = 0;

    // Enable skeleton tracking
    hr = NuiSkeletonTrackingEnable( m_hSkeletonDoneEvent, m_dwTrackingFlags );
    if( FAILED( hr ) )
    {
        ATG::NuiPrintError( hr, "NuiSkeletonTrackingEnable" );
        return hr;
    }

    // Initialize the Picture-in-Picture vizualization.
    hr = m_pip.Initialize( m_pd3dDevice, 
                           NUI_INITIALIZE_FLAG_USES_COLOR | NUI_INITIALIZE_FLAG_USES_DEPTH_AND_PLAYER_INDEX | NUI_INITIALIZE_FLAG_USES_SKELETON,
                           NUI_IMAGE_RESOLUTION_640x480 );
    if( FAILED( hr ) )
    {
        ATG_PrintError( "Picture-in-Picture initialization failed!" );
        return hr;
    }

    return hr;
}

//--------------------------------------------------------------------------------------
// Name: Initialize()
// Desc: This initializes all objects and values used by the sample.
//--------------------------------------------------------------------------------------
HRESULT Sample::Initialize()
{
    HRESULT hr = S_OK;

    m_bDrawHelp = FALSE;
    m_bPaused = FALSE;

    hr = InitializeNui();
    if( FAILED( hr ) )
    {
        return hr;
    }

    // Initialize the skeleton
    XMemSet( &m_SkeletonFrame, 0, sizeof( m_SkeletonFrame ) );

    // Calculate NUI-camera to world space values.

    // NUI constants
    m_fNuiHorizontalFOV  = NUI_CAMERA_DEPTH_NOMINAL_HORIZONTAL_FOV;
    m_fNuiVerticalFOV    = NUI_CAMERA_DEPTH_NOMINAL_VERTICAL_FOV;

    // Double-check FOV & aspect ratio consistent with each other (should be roughly correct for default values)
    FLOAT FovAngleY      = XMConvertToRadians( m_fNuiVerticalFOV );
    FLOAT AspectRatio    = ( FLOAT )g_nDepthWidth / ( FLOAT )g_nDepthHeight;
    FLOAT FovAngleX      = 2.0f * atanf( AspectRatio * tan( FovAngleY / 2.0f ) ); 
    m_fNuiHorizontalFOV  = XMConvertToDegrees( FovAngleX ); // actually around 57.8

    // Set our initial rendering mode...
    m_nRenderModeIndex = 0;

    // Create the font
    if( FAILED( m_Font.Create( "game:\\Media\\Fonts\\Arial_16.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Confine text drawing to the title safe area
    m_Font.SetWindow( ATG::GetTitleSafeArea() );

    // Create the help
    if( FAILED( m_Help.Create( "game:\\Media\\Help\\Help.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Set up view transform
    m_vPosition = XMVectorZero(); 
    m_vVelocity = XMVectorZero(); 
    m_fYaw = 0.0f;
    m_fYawVelocity = 0.0f;
    m_fPitch = 0.0f;
    m_fPitchVelocity = 0.0f;

    m_vLightDir = XMVector3Normalize( XMVectorSet( 0.5f, 0, 1.5f, 0 ) );

    m_matCameraView = XMMatrixIdentity();

    // Set up world transform.
    m_matWorld = XMMatrixIdentity();

    // Set up projection matrix
    const FLOAT fZNear = 0.1f;
    const FLOAT fZFar = 250.0f;     // Far depth-plane at 250 meters.
    FLOAT fAspectRatio = ( FLOAT )m_d3dpp.BackBufferWidth / (FLOAT)m_d3dpp.BackBufferHeight;
    m_matCameraProj = XMMatrixPerspectiveFovLH( XM_PI / 2.5f, fAspectRatio, fZNear, fZFar );

    // Create the skeleton geometry.
    hr = CreateSkeletonGeometry();
    if( FAILED( hr ) )
    {
        return hr;
    }

    // Create shaders

    // Initialize simple shaders.
    ATG::SimpleShaders::Initialize( NULL, NULL );

    // ...and load our shaders...
    hr = ATG::LoadVertexShader( "game:\\Media\\Shaders\\TransformDepthMesh.xvu", &m_pDepthMeshVS );
    if( FAILED( hr ) )
    {
        ATG_PrintError( "Failed to load TransformDepthMesh.xvu" );
        return ATGAPPERR_MEDIANOTFOUND;
    }

    hr = ATG::LoadPixelShader( "game:\\Media\\Shaders\\Textured.xpu", &m_pTexturedPS );
    if( FAILED( hr ) )
    {
        ATG_PrintError( "Failed to load Textured.xpu" );
        return ATGAPPERR_MEDIANOTFOUND;
    }

    hr = ATG::LoadPixelShader( "game:\\Media\\Shaders\\Lit.xpu", &m_pBasicLitPS );
    if( FAILED( hr ) )
    {
        ATG_PrintError( "Failed to load Lit.xpu" );
        return ATGAPPERR_MEDIANOTFOUND;
    }

    hr = ATG::LoadVertexShader( "game:\\Media\\Shaders\\TransformSkeletonBone.xvu", &m_pSkeletonVS );
    if( FAILED( hr ) )
    {
        ATG_PrintError( "Failed to load TransformSkeletonBone.xvu" );
        return ATGAPPERR_MEDIANOTFOUND;
    }

    hr = ATG::LoadPixelShader( "game:\\Media\\Shaders\\PSSkeletonBone.xpu", &m_pSkeletonPS );
    if( FAILED( hr ) )
    {
        ATG_PrintError( "Failed to load PSSkeletonBone.xpu" );
        return ATGAPPERR_MEDIANOTFOUND;
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Update()
// Desc: Called once per frame, the call is the entry point for animating the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Update()
{
    HRESULT hr = S_OK;

    // Get the current time
    FLOAT fElapsedTime = (FLOAT)m_Timer.GetElapsedTime();

    // Get the current gamepad state
    ATG::GAMEPAD* pGamepad = ATG::Input::GetMergedInput();

    // Pause the timer
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_START )
    {
        m_bPaused = !m_bPaused;

        if( m_bPaused )   
        {
            m_Timer.Stop();
        }
        else
        {
            m_Timer.Start();
        }
    }

    // Toggle help
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_BACK )
        m_bDrawHelp = !m_bDrawHelp;

    // Move forward through the available rendering modes if the right shoulder button is pressed,
    // rotate back to the start if we hit the end
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER )
    {
        ++m_nRenderModeIndex;
        if( m_nRenderModeIndex >= ARRAYSIZE( g_RenderModes ) )
            m_nRenderModeIndex = 0;
    }

    // Move backward through the available rendering modes if the right shoulder button is pressed.
    // rotate back to the end if we hit the start
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_LEFT_SHOULDER )
    {
        if( 0 == m_nRenderModeIndex )
        {
            m_nRenderModeIndex = ARRAYSIZE( g_RenderModes ) - 1;
        }
        else
        {
            --m_nRenderModeIndex;
        }
    }

    // Go no further if the app is paused.
    if( m_bPaused )
        return S_OK;

    // Switch skeletal tracking mode if gamepad "A" is pressed.
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_A )
    {
        if( 0 == m_dwTrackingFlags )
        {
            m_dwTrackingFlags = NUI_SKELETON_TRACKING_FLAG_ENABLE_SEATED_SUPPORT;
        }
        else
        {
            m_dwTrackingFlags = 0;
        }

        // Re-initialize the skeleton tracking, based on the new pipeline (we can 
        // toggle between seated and standing).
        NuiSkeletonTrackingDisable();
        NuiSkeletonTrackingEnable( m_hSkeletonDoneEvent, m_dwTrackingFlags );
    }

    const FLOAT fCameraSpeed = 20.0f;
    const FLOAT fMovementMultiplier = 5.0f;

    // Update camera position.
    m_vVelocity.x = fCameraSpeed * fElapsedTime * pGamepad->fX1;
    m_vVelocity.z = fCameraSpeed * fElapsedTime * pGamepad->fY1;

    m_fYawVelocity = fCameraSpeed * fElapsedTime * pGamepad->fX2;
    m_fPitchVelocity = fCameraSpeed * fElapsedTime * pGamepad->fY2;

    m_fYaw += fMovementMultiplier * fElapsedTime * m_fYawVelocity;
    m_fPitch += fMovementMultiplier * fElapsedTime * m_fPitchVelocity;

    // Calculate the orientation matrix
    XMVECTOR vDeterminant;
    XMVECTOR qR;
    qR = XMQuaternionRotationRollPitchYaw( m_fPitch, m_fYaw, 0.0f );
    XMMATRIX matOrientation = XMMatrixAffineTransformation( XMVectorSet( 1.0f, 1.0f, 1.0f, 1.0f ), XMVectorZero(), qR, m_vPosition );

    // Calculate the translation matrix
    XMVECTOR vT = m_vVelocity * fMovementMultiplier * fElapsedTime;
    vT = XMVector3TransformNormal( vT, matOrientation );
    m_vPosition += vT;

    // Get the camera view matrix
    m_matCameraView = XMMatrixInverse( &vDeterminant, matOrientation );

    // Wait for NUI information to be complete. The event we wait on is determined by the data we use. If we're not doing any skeleton tracking,
    // then we can wait on the depth done event, which occurs first. Otherwise, wait on the skeleton done event.
    BOOL bRenderSkeleton = ( g_RenderModes[ m_nRenderModeIndex ] & g_dwSkeletonModeMask ) == 0 ? FALSE : TRUE;
    if( bRenderSkeleton )
    {
        if( WAIT_OBJECT_0 != WaitForSingleObject( m_hSkeletonDoneEvent, NUI_FRAME_END_TIMEOUT_DEFAULT ) )
        {
            return E_FAIL;
        }
    }
    else
    {
        if( WAIT_OBJECT_0 != WaitForSingleObject( m_hDepthDoneEvent, NUI_FRAME_END_TIMEOUT_DEFAULT ) )
        {
            return E_FAIL;
        }
    }

    // Release any previous frame color reference
    if( NULL != m_pImageFrame )
    {
        NuiImageStreamReleaseFrame( m_hImage, m_pImageFrame );
        m_pImageFrame = NULL; 
    }

    // Grab a copy of the color surface for this frame.
    hr = NuiImageStreamGetNextFrame( m_hImage, 0, &m_pImageFrame );

    // Release any previous frame depth reference
    if( NULL != m_pDepthFrame )
    {
        NuiImageStreamReleaseFrame( m_hDepth, m_pDepthFrame );
        m_pDepthFrame = NULL;
    }

    // Grab a copy of the depth surface for this frame.
    hr = NuiImageStreamGetNextFrame( m_hDepth, 0, &m_pDepthFrame );

    // Get the next frame's skeleton data (if we have any)
    hr = NuiSkeletonGetNextFrame( NUI_CAMERA_TIMEOUT_DEFAULT, &m_SkeletonFrame );
    if( SUCCEEDED( hr ) )
    {
        NuiTransformSmooth( &m_SkeletonFrame, NULL );
        m_pip.SetSkeletons( &m_SkeletonFrame );
    }

    return S_OK;
}

//--------------------------------------------------------------------------------------
// Name: Render()
// Desc: Sets up render states, clears the viewport, and renders the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Render()
{
    // Draw a solid black background
    ATG::RenderBackground( 0xff000000, 0xff000000 );

    XMVECTOR vTwiceTanHalfFOV = { 
        2.0f * tan(  XMConvertToRadians( m_fNuiHorizontalFOV / 2.0f ) ), 
        2.0f * tan( -XMConvertToRadians( m_fNuiVerticalFOV   / 2.0f ) ), 
    };

    DWORD dwDepthRenderMode = g_RenderModes[ m_nRenderModeIndex ] & g_dwDepthModeMask;
    BOOL bRemoveBackground = ( g_RenderModes[ m_nRenderModeIndex ] & g_dwExcludeBackgroundMask ) == 0 ? FALSE : TRUE;
    BOOL bRenderSkeleton = ( g_RenderModes[ m_nRenderModeIndex ] & g_dwSkeletonModeMask ) == 0 ? FALSE : TRUE;

    // Update the clip-space transform and set the VS constant.
    XMMATRIX matCameraWVP = XMMatrixTranspose( m_matWorld * m_matCameraView * m_matCameraProj );
    m_pd3dDevice->SetVertexShaderConstantF( 0, ( FLOAT* )&matCameraWVP, 4 );
    m_pd3dDevice->SetVertexShaderConstantF( 4, ( FLOAT* )&m_matWorld, 4 );
    m_pd3dDevice->SetVertexShaderConstantF( 8, (float*)&g_dwNaN, 1 );
    m_pd3dDevice->SetVertexShaderConstantF( 9, (float*)&vTwiceTanHalfFOV, 1 );

    if( dwDepthRenderMode != 0 && m_pDepthFrame != NULL && m_pImageFrame != NULL )
    {
        // Don't cull the depth mesh, so we can see it from behind.
        m_pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_NONE );

        // These states serve all depth mesh render modes
        m_pd3dDevice->SetVertexShader( m_pDepthMeshVS );

        m_pd3dDevice->SetTexture( D3DVERTEXTEXTURESAMPLER0, m_pDepthFrame->pFrameTexture );
        m_pd3dDevice->SetTexture( 0, m_pDepthFrame->pFrameTexture );
        m_pd3dDevice->SetSamplerFilterStates( D3DVERTEXTEXTURESAMPLER0, D3DTEXF_POINT, D3DTEXF_POINT, D3DTEXF_POINT, 1 );
        m_pd3dDevice->SetTexture( 1, m_pImageFrame->pFrameTexture );
        m_pd3dDevice->SetSamplerFilterStates( 0, D3DTEXF_LINEAR, D3DTEXF_LINEAR, D3DTEXF_LINEAR, 1 );
        m_pd3dDevice->SetSamplerFilterStates( 1, D3DTEXF_LINEAR, D3DTEXF_LINEAR, D3DTEXF_LINEAR, 1 );
        m_pd3dDevice->SetIndices( NULL );
        m_pd3dDevice->SetStreamSource( 0, NULL, 0, 0 );

        // Light and depth buffer dimension info for constants.
        float pData[] = {
            XMVectorGetX( m_vLightDir ), XMVectorGetY( m_vLightDir ), XMVectorGetZ( m_vLightDir ), 0.0f,             // light_dir
            1.0f / g_nDepthWidth, 1.0f / g_nDepthHeight, 0.0f, 0.0f,      // TextureStep
        };

        // Flag for removing background
        BOOL bRemoveBackgroundConstant[4] = { bRemoveBackground, 0, 0, 0 };
        m_pd3dDevice->SetVertexShaderConstantB( 0, bRemoveBackgroundConstant, 1 );

        if( dwDepthRenderMode == RENDER_DEPTH_COLOR )
        {
            m_pd3dDevice->SetPixelShader( m_pTexturedPS );
        }
        else
        {
            m_pd3dDevice->SetPixelShaderConstantF( 0, pData, 2 );
            m_pd3dDevice->SetPixelShaderConstantF( 9, ( float* )&vTwiceTanHalfFOV, 1 );
            m_pd3dDevice->SetPixelShader( m_pBasicLitPS );
        }

        // Number of quads is the width * the height (minus one)
        static const UINT nQuads = ( g_nDepthWidth - 1 ) * ( g_nDepthHeight - 1 );
        m_pd3dDevice->DrawPrimitive( D3DPT_QUADLIST, 0, nQuads );

        // Unset textures.
        m_pd3dDevice->SetTexture( D3DVERTEXTEXTURESAMPLER0, NULL );
        m_pd3dDevice->SetTexture( 0, NULL );
        m_pd3dDevice->SetTexture( 1, NULL );
    }

    if( bRenderSkeleton )
    {
        // Render the skeleton with culling, so the blended pass doesn't generate artifacts.
        m_pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_CCW );

        // Render the skeleton with alpha on and depth off, so we can see the bits that are behind the depth mesh
        m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, FALSE );
        m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, TRUE );
        D3DBLENDSTATE BlendState;
        BlendState.BlendOp = D3DBLENDOP_ADD;
        BlendState.BlendOpAlpha = D3DBLENDOP_ADD;
        BlendState.DestBlend = D3DBLEND_ONE;
        BlendState.DestBlendAlpha = D3DBLEND_ONE;
        BlendState.SrcBlend = D3DBLEND_ONE;
        BlendState.SrcBlendAlpha = D3DBLEND_ONE;
        m_pd3dDevice->SetBlendState( 0, BlendState );
        RenderSkeleton();

        // Re-render with depth enabled for any portion of the skeleton that is in front of the depth mesh
        BlendState.DestBlend = D3DBLEND_ZERO;
        m_pd3dDevice->SetBlendState( 0, BlendState );
        m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, TRUE );
        RenderSkeleton();

        m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );
    }

    // Render the picture-in-picture components (color and depth, plus first tracked skeleton)
    if( m_pImageFrame != NULL && m_pDepthFrame != NULL )
    {
        const FLOAT drawWidth = 200.0f;
        const FLOAT drawHeight = drawWidth * 3.0f / 4.0f;
        const FLOAT drawY = 720.0f - 50.0f - drawHeight;
        FLOAT drawX = 50.0f;

        m_pip.SetColorTexture( m_pImageFrame->pFrameTexture );
        m_pip.SetDepthTexture( m_pDepthFrame->pFrameTexture );

        m_pip.RenderColorStream( drawX, drawY, drawWidth, drawHeight );

        // Render any tracked skeletons...
        for( INT dwSkeleton = 0; dwSkeleton < NUI_SKELETON_COUNT; ++dwSkeleton )
        {
            m_pip.RenderSingleSkeleton( dwSkeleton, drawX, drawY, drawWidth, drawHeight, FALSE, FALSE );
        }

        drawX += drawWidth + 20;
        m_pip.RenderDepthStream( drawX, drawY, drawWidth, drawHeight );

        m_pip.SetColorTexture( NULL );
        m_pip.SetDepthTexture( NULL );
    }

    // Show title, frame rate, and help
    m_Timer.MarkFrame();
    if( m_bDrawHelp )
    {
        m_Help.Render( &m_Font, g_HelpCallouts, NUM_HELP_CALLOUTS );
    }
    else
    {
        m_Font.Begin();
        m_Font.SetScaleFactors( 1.2f, 1.2f );
        m_Font.DrawText( 0, 0, 0xffffffff,  g_RenderModeTitles[ m_nRenderModeIndex ] );
        m_Font.DrawText( 0, 28, 0xffffffff, m_dwTrackingFlags? L"Skeleton Seated Pipeline" : L"Skeleton Standing Pipeline" );
        m_Font.SetScaleFactors( 1.0f, 1.0f );
        m_Font.DrawText( 0, 0, 0xffffff00, m_Timer.GetFrameRate(), ATGFONT_RIGHT );
        m_Font.End();
    }

    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}

//--------------------------------------------------------------------------------------
// Name: RenderSkeleton()
// Desc: Renders the skeleton bones as a series of cylinders, scaled and positioned 
// based on joint transforms.
//--------------------------------------------------------------------------------------
HRESULT Sample::RenderSkeleton()
{
    struct BONE_JOINTS
    {
        NUI_SKELETON_POSITION_INDEX     StartJoint;
        NUI_SKELETON_POSITION_INDEX     EndJoint;
    };

    // Define the bones in the skeleton using joint indices
    static const BONE_JOINTS s_Bones[] =
    {
        // Head
        { NUI_SKELETON_POSITION_HEAD, NUI_SKELETON_POSITION_SHOULDER_CENTER },              // Top of head to top of neck

        // Right arm
        { NUI_SKELETON_POSITION_SHOULDER_CENTER, NUI_SKELETON_POSITION_SHOULDER_RIGHT },    // Neck bottom to right shoulder internal
        { NUI_SKELETON_POSITION_SHOULDER_RIGHT, NUI_SKELETON_POSITION_ELBOW_RIGHT },        // Right shoulder internal to right elbow
        { NUI_SKELETON_POSITION_ELBOW_RIGHT, NUI_SKELETON_POSITION_HAND_RIGHT },            // Right elbow to right wrist

        // Left arm
        { NUI_SKELETON_POSITION_SHOULDER_CENTER, NUI_SKELETON_POSITION_SHOULDER_LEFT },     // Neck bottom to left shoulder internal
        { NUI_SKELETON_POSITION_SHOULDER_LEFT, NUI_SKELETON_POSITION_ELBOW_LEFT },          // Left shoulder internal to left elbow
        { NUI_SKELETON_POSITION_ELBOW_LEFT, NUI_SKELETON_POSITION_HAND_LEFT },              // Left elbow to left wrist

        // Right leg and foot
        { NUI_SKELETON_POSITION_HIP_RIGHT, NUI_SKELETON_POSITION_KNEE_RIGHT },              // Right hip internal to right knee
        { NUI_SKELETON_POSITION_KNEE_RIGHT, NUI_SKELETON_POSITION_ANKLE_RIGHT },            // Right knee to right ankle

        // Left leg and foot
        { NUI_SKELETON_POSITION_HIP_LEFT, NUI_SKELETON_POSITION_KNEE_LEFT },                // Left hip internal to left knee
        { NUI_SKELETON_POSITION_KNEE_LEFT, NUI_SKELETON_POSITION_ANKLE_LEFT },              // Left knee to left ankle

        // Spine
        { NUI_SKELETON_POSITION_SHOULDER_CENTER, NUI_SKELETON_POSITION_SPINE },             // Neck bottom to spine
        { NUI_SKELETON_POSITION_SPINE, NUI_SKELETON_POSITION_HIP_CENTER },                  // Spine to hip center

        // Hips
        { NUI_SKELETON_POSITION_HIP_RIGHT, NUI_SKELETON_POSITION_HIP_CENTER },              // Right hip to hip center
        { NUI_SKELETON_POSITION_HIP_CENTER, NUI_SKELETON_POSITION_HIP_LEFT }                // Hip center to left hip
    };

    const DWORD dwNumBones = m_dwTrackingFlags == NUI_SKELETON_TRACKING_FLAG_ENABLE_SEATED_SUPPORT? 7 : ARRAYSIZE( s_Bones );

    m_pd3dDevice->SetVertexDeclaration( m_pSkeletonGeometryVertexDecl );
    m_pd3dDevice->SetVertexShader( m_pSkeletonVS );
    m_pd3dDevice->SetPixelShader( m_pSkeletonPS );
    m_pd3dDevice->SetStreamSource( 0, m_pVBCylinder, 0, sizeof( SkeletonMeshVertex ) );
    m_pd3dDevice->SetIndices( m_pIBCylinder );
    m_pd3dDevice->SetPixelShaderConstantF( 0, ( FLOAT* )&m_vLightDir, 1 );

    // Iterate through each skeleton (starting with the first identified as being tracked)...
    for( INT dwSkeleton = 0; dwSkeleton < NUI_SKELETON_COUNT; ++dwSkeleton )
    {
        // Skip any non-tracked skeleton
        if( m_SkeletonFrame.SkeletonData[ dwSkeleton ].eTrackingState != NUI_SKELETON_POSITION_TRACKED )
            continue;

        // OK, this one is tracked. Iterate through the bones.
        for( DWORD i = 0; i < dwNumBones; ++i )
        {
            // Get the start and end position of each bone.
            const NUI_SKELETON_POSITION_TRACKING_STATE *pSkeletonTrackingState = &m_SkeletonFrame.SkeletonData[ dwSkeleton ].eSkeletonPositionTrackingState[ 0 ];

            // If a bone is not tracked, don't draw it.
            if( pSkeletonTrackingState[ s_Bones[ i ].StartJoint ] == NUI_SKELETON_POSITION_NOT_TRACKED || pSkeletonTrackingState[ s_Bones[ i ].EndJoint ] == NUI_SKELETON_POSITION_NOT_TRACKED )
                continue;

            // Get the start and end vectors, scaled to include the x8 factor from the 3 segmentation bits in depth...
            static const float fScaleToIncludeSegmentation = 8.0f;
            XMVECTOR vStart = m_SkeletonFrame.SkeletonData[ dwSkeleton ].SkeletonPositions[ s_Bones[ i ].StartJoint ] * fScaleToIncludeSegmentation;
            XMVECTOR vEnd   = m_SkeletonFrame.SkeletonData[ dwSkeleton ].SkeletonPositions[ s_Bones[ i ].EndJoint ] * fScaleToIncludeSegmentation;

            // Calculate bone axis and length.
            XMVECTOR vAxis = vEnd - vStart;
            XMVECTOR vBoneLength = XMVector3Length( vAxis );
            if( XMVectorGetX( vBoneLength ) <= FLT_EPSILON )
                continue;

            // Get the matrix to rotate a unit cylinder (with one end at the origin in model-space)
            // to orient through the two bone joints for the current bone.
            vAxis = XMVector3Normalize( vAxis );
            XMVECTOR vZ = XMVectorSet( 0.0f, 0.0f, 1.0f, 0.0f );
            XMVECTOR vNormal = XMVector3Cross( vAxis, vZ );
            FLOAT fSinAngle = XMVectorGetX( XMVector3Length( vNormal ) );
            FLOAT fCosAngle = XMVectorGetX( XMVector3Dot( vAxis, vZ ) );
            vNormal = XMVector3Normalize( vNormal );
            XMMATRIX matRotWorld = XMMatrixRotationAxis( vNormal, -atan2( fSinAngle, fCosAngle ) );

            // Generate the world matrix, by including scaling (to make the bones the appropriate thickness and length) and our previously 
            // calculated orientation.
            XMMATRIX matWorld = XMMatrixScaling( 0.1f, 0.1f, XMVectorGetX( vBoneLength ) ) * matRotWorld * XMMatrixTranslationFromVector( vStart );
            XMMATRIX matCameraWVP = XMMatrixTranspose( matWorld * m_matCameraView * m_matCameraProj );
            matRotWorld = XMMatrixTranspose( matRotWorld );
        
            // Set shader constants for world rotation and clip-space transforms.
            m_pd3dDevice->SetVertexShaderConstantF( 0, ( FLOAT* )&matCameraWVP, 4 );
            m_pd3dDevice->SetVertexShaderConstantF( 4, ( FLOAT* )&matRotWorld, 4 );

            // Set the end colors.
            float aBoneColors[] = { 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f };
            if( pSkeletonTrackingState[ s_Bones[ i ].StartJoint ] == NUI_SKELETON_TRACKED )
            {
                aBoneColors[ 1 ] = 1.0f;        // Green - tracked.
            }
            else
            {
                aBoneColors[ 0 ] = 1.0f;        // Red - not tracked
            }

            if( pSkeletonTrackingState[ s_Bones[ i ].EndJoint ] == NUI_SKELETON_TRACKED )
            {
                aBoneColors[ 5 ] = 1.0f;
            }
            else
            {
                aBoneColors[ 4 ] = 1.0f;
            }

            m_pd3dDevice->SetVertexShaderConstantF( 10, ( FLOAT* )aBoneColors, 2 );

            // Draw the bone.
            m_pd3dDevice->DrawIndexedPrimitive( D3DPT_TRIANGLELIST, 0, 0, 0, 0, m_nCylinderTriangleCount );
        }
    }

    return S_OK;
}

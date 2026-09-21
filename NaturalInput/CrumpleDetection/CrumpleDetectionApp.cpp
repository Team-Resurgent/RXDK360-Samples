//--------------------------------------------------------------------------------------
// CrumpleDetectionApp.cpp
//
// This sample demonstrates the use of a tool that scores the quality of 
// skeletons tracked by ST.
//
// Microsoft Game Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include <xtl.h>
#include <AtgApp.h>
#include <AtgFont.h>
#include <AtgHelp.h>
#include <AtgInput.h>
#include <AtgSimpleShaders.h>
#include <AtgUtil.h>
#include <AtgNuiCommon.h>
#include <AtgDebugDraw.h>

#include "CrumpleDetection.h"

//--------------------------------------------------------------------------------------
// Callouts for labeling the gamepad on the help screen
//--------------------------------------------------------------------------------------
ATG::HELP_CALLOUT g_HelpCallouts[] =
{
    { ATG::HELP_BACK_BUTTON,    ATG::HELP_PLACEMENT_1, L"Display help" },
    { ATG::HELP_LEFT_STICK,     ATG::HELP_PLACEMENT_1, L"Move Camera" },
    { ATG::HELP_RIGHT_STICK,    ATG::HELP_PLACEMENT_1, L"Rotate Camera" },
    { ATG::HELP_A_BUTTON,       ATG::HELP_PLACEMENT_1, L"Toggle 3D Points" },
    { ATG::HELP_Y_BUTTON,       ATG::HELP_PLACEMENT_1, L"Toggle Draw Mode" },
};

static const DWORD NUM_HELP_CALLOUTS = ARRAYSIZE( g_HelpCallouts );

#define TOP_BACK_COLOR      0xff000000          // Background gradient colors
#define BOTTOM_BACK_COLOR   0xff000000

// Each skeleton bone has two joints
struct BoneJoints
{
    NUI_SKELETON_POSITION_INDEX   StartJoint;
    NUI_SKELETON_POSITION_INDEX   EndJoint;
};

// Define the bones in the skeleton using joint indices
const BoneJoints g_Bones[] =
{
    // Head
    { NUI_SKELETON_POSITION_HEAD, NUI_SKELETON_POSITION_SHOULDER_CENTER },                // Top of head to top of neck

    // Right arm
    { NUI_SKELETON_POSITION_SHOULDER_CENTER, NUI_SKELETON_POSITION_SHOULDER_RIGHT },      // Neck bottom to right shoulder internal
    { NUI_SKELETON_POSITION_SHOULDER_RIGHT, NUI_SKELETON_POSITION_ELBOW_RIGHT },          // Right shoulder internal to right elbow
    { NUI_SKELETON_POSITION_ELBOW_RIGHT, NUI_SKELETON_POSITION_WRIST_RIGHT},              // Right elbow to right wrist
    { NUI_SKELETON_POSITION_WRIST_RIGHT, NUI_SKELETON_POSITION_HAND_RIGHT },              // Right wrist to right hand

    // Left arm
    { NUI_SKELETON_POSITION_SHOULDER_CENTER, NUI_SKELETON_POSITION_SHOULDER_LEFT },       // Neck bottom to left shoulder internal
    { NUI_SKELETON_POSITION_SHOULDER_LEFT, NUI_SKELETON_POSITION_ELBOW_LEFT},             // Left shoulder internal to left elbow
    { NUI_SKELETON_POSITION_ELBOW_LEFT, NUI_SKELETON_POSITION_WRIST_LEFT},                // Left elbow to left wrist
    { NUI_SKELETON_POSITION_WRIST_LEFT, NUI_SKELETON_POSITION_HAND_LEFT },                // Left wrist to left hand

    // Right leg and foot
    { NUI_SKELETON_POSITION_HIP_RIGHT, NUI_SKELETON_POSITION_KNEE_RIGHT },                // Right hip internal to right knee
    { NUI_SKELETON_POSITION_KNEE_RIGHT, NUI_SKELETON_POSITION_ANKLE_RIGHT },              // Right knee to right ankle

    // Left leg and foot
    { NUI_SKELETON_POSITION_HIP_LEFT, NUI_SKELETON_POSITION_KNEE_LEFT },                  // Left hip internal to left knee
    { NUI_SKELETON_POSITION_KNEE_LEFT, NUI_SKELETON_POSITION_ANKLE_LEFT},                 // Left knee to left ankle

    // Spine
    { NUI_SKELETON_POSITION_SHOULDER_CENTER, NUI_SKELETON_POSITION_SPINE},                // Neck bottom to spine
    { NUI_SKELETON_POSITION_SPINE, NUI_SKELETON_POSITION_HIP_CENTER },                    // Spine to hip center

    // Hips
    { NUI_SKELETON_POSITION_HIP_RIGHT, NUI_SKELETON_POSITION_HIP_CENTER },                // Right hip to hip center
    { NUI_SKELETON_POSITION_HIP_CENTER, NUI_SKELETON_POSITION_HIP_LEFT}                   // Hip center to left hip
};

const UINT g_uNumBones = ARRAYSIZE( g_Bones );

//-----------------------------------------------------------------------------
// Name:  xmfloat3()
// Desc:  convert a XMVECTOR into a XMFLOAT3
//-----------------------------------------------------------------------------
XMFLOAT3 xmfloat3( const XMVECTOR& vect )
{
    return _XMFLOAT3( XMVectorGetX(vect),
        XMVectorGetY(vect),
        XMVectorGetZ(vect) );
}

//-----------------------------------------------------------------------------
// Name:  renderSkeleton
// Desc:  renders a skeleton with given color, and translated it in the x direction
//        by offX
//-----------------------------------------------------------------------------
void renderSkeleton(const NUI_SKELETON_FRAME& skel, FLOAT offX, D3DCOLOR color)
{	
    // Project each of the tracked skeletons.
    for( UINT j = 0; j < NUI_SKELETON_COUNT; ++j )
    {
        if (skel.SkeletonData[j].eTrackingState != NUI_SKELETON_TRACKED)
            continue;

        for( UINT i = 0; i < g_uNumBones; ++i )
        {
            XMVECTOR pa = skel.SkeletonData[ j ].SkeletonPositions[ g_Bones[ i ].StartJoint ];
            XMVECTOR pb = skel.SkeletonData[ j ].SkeletonPositions[ g_Bones[ i ].EndJoint ];

            pa += XMVectorSet( offX, 0,0,0);
            pb += XMVectorSet( offX, 0,0,0);

            ATG::DebugDraw::DrawLineSegment( xmfloat3(pa), xmfloat3(pb), color );
        }
    }
}


//-----------------------------------------------------------------------------
// Name:  CCrumpleDetectionApp
//-----------------------------------------------------------------------------
class CCrumpleDetectionApp : public ATG::Application
{
public:	

    CCrumpleDetectionApp() : ATG::Application()
    {
        m_fCameraYaw = 0;
        m_fCameraPitch= 0;
        m_fCameraX= 0;
        m_fCameraY= 0;
        m_fCameraZ= -1.0f;

        m_dbTimeProcessCrumpleDetection = 0;
        m_dbTimeProcessCrumpleDetectionMax = 0;

        m_fLastElapsedAbsoluteTime = m_Timer.GetAbsoluteTime();
        m_DrawMode = DRAW_MODE_SKELETON;

        m_bDraw3DPoints = TRUE;

        m_pDepthBuffer = new USHORT[DEPTH_CAMERA_RESOLUTION_X*DEPTH_CAMERA_RESOLUTION_Y];
    }

    ~CCrumpleDetectionApp()
    {
        delete[] m_pDepthBuffer;
    }

private:

    virtual HRESULT Initialize();
    virtual HRESULT Update();
    virtual HRESULT Render();

    HANDLE m_hDepthStream;
    HANDLE m_hFrameEndEvent;

    XMMATRIX WorldMatrix();
    XMMATRIX CameraMatrix();

    enum DRAW_MODE
    {
        DRAW_MODE_SKELETON=0,
        DRAW_MODE_SCORE_DETAILS,
        DRAW_MODE_DEBUG_GRAPHICS,
        DRAW_MODE_MAX
    };

    FLOAT 	m_fCameraYaw;
    FLOAT 	m_fCameraPitch;
    FLOAT 	m_fCameraX;
    FLOAT 	m_fCameraY;
    FLOAT 	m_fCameraZ;	

    USHORT *m_pDepthBuffer;

    DOUBLE m_fLastElapsedAbsoluteTime;

    ATG::Timer m_Timer;
    ATG::Font m_Font;
    ATG::Help  m_Help;

    DOUBLE m_dbTimeProcessCrumpleDetection;
    DOUBLE m_dbTimeProcessCrumpleDetectionMax;

    CCrumpleDetector m_CrumpleDetector;

    DRAW_MODE m_DrawMode;
    BOOL     m_bDraw3DPoints;
    BOOL     m_bDrawHelp;


    NUI_SKELETON_FRAME m_NuiFrame;

    LPDIRECT3DVERTEXDECLARATION9    m_pSimpleVertexDecl;
    LPDIRECT3DVERTEXDECLARATION9    m_pColoredVertexDecl;
    LPDIRECT3DVERTEXSHADER9         m_pSingleDiffuseColorVertexShader;
    LPDIRECT3DVERTEXSHADER9         m_pDiffuseColorVertexShader;
    LPDIRECT3DPIXELSHADER9          m_pSimpleColorPixelShader;

};


//-----------------------------------------------------------------------------
// D3D Vertex Element array describing the input vertex format used for the
// skeleton visualization.  For this effect, each vertex is an XVMECTOR
// structure, which consists of 4 floats.
//-----------------------------------------------------------------------------
static const D3DVERTEXELEMENT9 g_SimpleVertexElementsMain[] =
{
    { 0, 0, D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
    D3DDECL_END()
};

static const D3DVERTEXELEMENT9 g_ColoredVertexElements[] =
{
    { 0, 0, D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
    { 0, 16,D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR, 0 },
    D3DDECL_END()
};


//-----------------------------------------------------------------------------
// Pixel shader definition for a simple color pass through
//-----------------------------------------------------------------------------
static const char* g_strRenderUtilities_SimpleColorPixelShaderHLSLMain =
" struct VS_OUT                                                           "
" {                                                                       "
"     float4 Position : POSITION;                                         "
"     float4 Color    : COLOR0;                                           "
" };                                                                      "
"                                                                         "
" float4 main( VS_OUT Input ) : COLOR                                     "
" {                                                                       "
"     return Input.Color;                                                 "
" }                                                                       ";


//-----------------------------------------------------------------------------
// Vector shader definition for diffuse color visualization
//-----------------------------------------------------------------------------
static const char* g_strRenderUtilities_SingleDiffuseColorVertexShaderHLSLMain =
" struct VS_OUT                                                           "
" {                                                                       "
"     float4 Position : POSITION;                                         "
"     float4 Color    : COLOR0;                                           "
" };                                                                      "
"                                                                         "
" float4x4   c_WVP         : register(c0);                                "
" float4   c_JointColors : register(c4);                                  "
"                                                                         "
" VS_OUT main( float4 JointPosition : POSITION )                          "
" {                                                                       "
"     VS_OUT Output;                                                      "
"     Output.Position = mul( c_WVP, float4( JointPosition.xyz, 1 ) );     "
"     Output.Color = c_JointColors;                                       "
"     return Output;                                                      "
" }                                                                       ";

static const char* g_strRenderUtilities_DiffuseColorVertexShaderHLSLMain =
" struct VS_OUT                                                           "
" {                                                                       "
"     float4 Position : POSITION;                                         "
"     float4 Color    : COLOR0;                                           "
" };                                                                      "
"                                                                         "
" float4x4   c_WVP         : register(c0);                                "
"                                                                         "
" VS_OUT main( float4 JointPosition : POSITION, float4 JointColor : COLOR0 )   "
" {                                                                       "
"     VS_OUT Output;                                                      "
"     Output.Position = mul( c_WVP, float4( JointPosition.xyz, 1 ) );     "
"     Output.Color = JointColor;                                          "
"     return Output;                                                      "
" }                                                                       ";


//-----------------------------------------------------------------------------
// LoadVertexShader()
// Helper function to compile a vertex shader from HLSL.
//-----------------------------------------------------------------------------
static HRESULT
LoadVertexShader(
                 _In_ LPDIRECT3DDEVICE9 pDevice,
                 _In_z_ LPCSTR szHLSL,
                 _Out_ LPDIRECT3DVERTEXSHADER9* ppShader)
{
    HRESULT hr = S_OK;
    ID3DXBuffer* pShaderCode;
    ID3DXBuffer* pErrorMsg;

    D3DXCompileShader(
        szHLSL, (UINT)strlen( szHLSL ),
        NULL, NULL,
        "main",
        "vs_3_0",
        0,
        &pShaderCode,
        &pErrorMsg,
        NULL);

    pDevice->CreateVertexShader(
        (DWORD*)pShaderCode->GetBufferPointer(),
        ppShader);
    return hr;
}


//-----------------------------------------------------------------------------
// LoadPixelShader()
// Helper function to compile a pixel shader from HLSL.
//-----------------------------------------------------------------------------
static HRESULT
LoadPixelShader(
                _In_ LPDIRECT3DDEVICE9 pDevice,
                _In_z_ LPCSTR szHLSL,
                _Out_ LPDIRECT3DPIXELSHADER9* ppShader)
{
    HRESULT hr = S_OK;
    ID3DXBuffer* pShaderCode;
    ID3DXBuffer* pErrorMsg;

    D3DXCompileShader(
        szHLSL, (UINT)strlen( szHLSL ),
        NULL, NULL,
        "main",
        "ps_3_0",
        0,
        &pShaderCode,
        &pErrorMsg,
        NULL);

    pDevice->CreatePixelShader(
        (DWORD*)pShaderCode->GetBufferPointer(),
        ppShader);

    return hr;
}


//-----------------------------------------------------------------------------
// Name:  Initialize
//-----------------------------------------------------------------------------
HRESULT CCrumpleDetectionApp::Initialize()
{
    // Initialize the simple shaders
    ATG::SimpleShaders::Initialize( NULL, NULL );

    // Create event which will be signaled when frame processing ends
    m_hFrameEndEvent = CreateEvent( NULL,
        FALSE,  // auto-reset
        FALSE,  // create unsignaled
        "NuiFrameEndEvent" );

    if ( !m_hFrameEndEvent )
    {
        ATG_PrintError( "Failed to create NuiFrameEndEvent\n" );
        return E_FAIL;
    }

    HRESULT hr = NuiInitialize( NUI_INITIALIZE_FLAG_USES_SKELETON |		
        NUI_INITIALIZE_FLAG_USES_DEPTH_AND_PLAYER_INDEX | 
        NUI_INITIALIZE_FLAG_EXTRAPOLATE_FLOOR_PLANE ,
        NUI_INITIALIZE_DEFAULT_HARDWARE_THREAD );

    if ( FAILED(hr) )
    {
        return hr;		
    }

    // Register frame end event with NUI
    hr = NuiSetFrameEndEvent( m_hFrameEndEvent, 0 );
    if( FAILED(hr) )
    {
        return E_FAIL;
    }

    hr = NuiImageStreamOpen( NUI_IMAGE_TYPE_DEPTH_AND_PLAYER_INDEX_80x60,
        NUI_IMAGE_RESOLUTION_80x60,
        0, 
        1, 
        NULL, 
        &m_hDepthStream); 

    if ( FAILED(hr) )
    {
        return hr;		
    }

    hr = NuiSkeletonTrackingEnable( NULL, 0 );

    if ( FAILED(hr) )
    {
        return hr;		
    }

    // Initialize the simple shaders
    ATG::SimpleShaders::Initialize( NULL, NULL );

    m_pd3dDevice->CreateVertexDeclaration(
        g_SimpleVertexElementsMain,
        &m_pSimpleVertexDecl);
    m_pd3dDevice->CreateVertexDeclaration(
        g_ColoredVertexElements,
        &m_pColoredVertexDecl);

    LoadVertexShader(
        m_pd3dDevice,
        g_strRenderUtilities_SingleDiffuseColorVertexShaderHLSLMain,
        &m_pSingleDiffuseColorVertexShader);

    LoadVertexShader(
        m_pd3dDevice,
        g_strRenderUtilities_DiffuseColorVertexShaderHLSLMain,
        &m_pDiffuseColorVertexShader);

    LoadPixelShader(
        m_pd3dDevice,
        g_strRenderUtilities_SimpleColorPixelShaderHLSLMain,
        &m_pSimpleColorPixelShader);

    // Create the font
    hr = m_Font.Create( "game:\\Media\\Fonts\\Arial_16.xpr" );
    if ( FAILED(hr))
    {
        return hr;
    }

    // Create the help
    hr = m_Help.Create( "game:\\Media\\Help\\Help.xpr" );
    if( FAILED(hr) )
    {
        ATG_PrintError( "Couldn't create help\n" );
        return hr;
    }

    return S_OK;
}


//-----------------------------------------------------------------------------
// Name:  Update
//-----------------------------------------------------------------------------
HRESULT CCrumpleDetectionApp::Update()
{
    PIXBeginNamedEvent( 0, __FUNCTION__ );

    // Wait for frame processing to end
    if( WAIT_OBJECT_0 != WaitForSingleObject( m_hFrameEndEvent, NUI_FRAME_END_TIMEOUT_DEFAULT ) )
    {
        return E_FAIL;
    }

    // update skelton tracking;
    const NUI_IMAGE_FRAME* pDepthImageFrame;
    HRESULT hr = NuiImageStreamGetNextFrame( m_hDepthStream, 0, &pDepthImageFrame );

    if( FAILED( hr ) )
    {
        return hr;
    }

    // get the skeleton;
    NUI_SKELETON_FRAME skel;
    hr = NuiSkeletonGetNextFrame( 0, &skel);

    if( FAILED( hr ) )
    {
        return hr;
    }

    m_NuiFrame = skel;

    // copy out depth buffer for display
    D3DLOCKED_RECT lockedDepthRect;
    pDepthImageFrame->pFrameTexture->LockRect(0, &lockedDepthRect, NULL, D3DLOCK_READONLY);

    USHORT*  pixel = m_pDepthBuffer;

    USHORT * dataRowStart = (USHORT *)lockedDepthRect.pBits;
    INT pixelsPerRow = lockedDepthRect.Pitch/sizeof(USHORT);

    for( int r = 0; r < DEPTH_CAMERA_RESOLUTION_Y; ++r )
    {
        memcpy( pixel, dataRowStart, sizeof(USHORT)*DEPTH_CAMERA_RESOLUTION_X );
        dataRowStart += pixelsPerRow;
        pixel += DEPTH_CAMERA_RESOLUTION_X;
    }
    pDepthImageFrame->pFrameTexture->UnlockRect(0);

    NuiImageStreamReleaseFrame( m_hDepthStream, pDepthImageFrame  );

    // deal with inputs
    ATG::GAMEPAD* pGamepad = ATG::Input::GetMergedInput();

    // The Back button toggles the help screen.
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_BACK )
    {
        m_bDrawHelp = !m_bDrawHelp;
    }

    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_A )
    {
        m_bDraw3DPoints = !m_bDraw3DPoints;	
    }

    if ( pGamepad->wPressedButtons & XINPUT_GAMEPAD_Y)
    {
        m_DrawMode = (DRAW_MODE) ( (m_DrawMode+1)% DRAW_MODE_MAX );
    }

    // adjust view port
    DOUBLE currTime = m_Timer.GetAbsoluteTime();
    FLOAT cameraSpeed =  (FLOAT)( currTime - m_fLastElapsedAbsoluteTime );

    m_fLastElapsedAbsoluteTime = currTime;
    m_fCameraYaw -= pGamepad->sThumbRX * .00002f * cameraSpeed;
    m_fCameraPitch += pGamepad->sThumbRY * .00002f * cameraSpeed;

    //view port position
    m_fCameraX += pGamepad->sThumbLX *.00005f* cameraSpeed*cosf(m_fCameraYaw);
    m_fCameraZ += pGamepad->sThumbLX *.00005f* cameraSpeed*sinf(m_fCameraYaw);
    m_fCameraX += pGamepad->sThumbLY *.00005f* cameraSpeed*cosf(m_fCameraYaw + XM_PIDIV2);
    m_fCameraZ += pGamepad->sThumbLY *.00005f* cameraSpeed*sinf(m_fCameraYaw + XM_PIDIV2);


    DOUBLE time = m_Timer.GetAbsoluteTime();

    //evaluate skelton
    m_CrumpleDetector.EvaluteSkeleton( m_NuiFrame, m_pDepthBuffer );

    m_dbTimeProcessCrumpleDetection = m_Timer.GetAbsoluteTime() - time;

    if ( m_dbTimeProcessCrumpleDetectionMax < m_dbTimeProcessCrumpleDetection ) m_dbTimeProcessCrumpleDetectionMax = m_dbTimeProcessCrumpleDetection;

    PIXEndNamedEvent();
    return S_OK;
}


//-----------------------------------------------------------------------------
// Name:  WorldMatrix()
//-----------------------------------------------------------------------------
XMMATRIX CCrumpleDetectionApp::WorldMatrix()
{
    return  XMMatrixTranslation(-m_fCameraX,0,-m_fCameraZ) *  XMMatrixRotationY(m_fCameraYaw) * XMMatrixRotationX(m_fCameraPitch);
}


//-----------------------------------------------------------------------------
// Name:  CameraMatrix()
//-----------------------------------------------------------------------------
XMMATRIX CCrumpleDetectionApp::CameraMatrix()
{
    FLOAT fAspectRatio = ( FLOAT )m_d3dpp.BackBufferWidth / ( FLOAT )m_d3dpp.BackBufferHeight;
    return XMMatrixPerspectiveFovLH(	XM_PI/4.0f, fAspectRatio, 0.1f, 1000.0f);
}


//-----------------------------------------------------------------------------
// Name:  Render()
//-----------------------------------------------------------------------------
HRESULT CCrumpleDetectionApp::Render()
{
    PIXBeginNamedEvent( 0, __FUNCTION__ );

    m_Timer.MarkFrame();

    // Draw a gradient filled background
    ATG::RenderBackground( TOP_BACK_COLOR, BOTTOM_BACK_COLOR );

    if( m_bDrawHelp )
    {
        m_Help.Render( &m_Font, g_HelpCallouts, NUM_HELP_CALLOUTS );
    }

    XMFLOAT3 start(0,0,0), end(10,0,0);
    XMMATRIX matProjection = WorldMatrix() * CameraMatrix() ;
    ATG::DebugDraw::SetViewProjection( matProjection );

    if ( m_bDraw3DPoints )
    {
        XMVECTOR colors[8];

        colors[0] = XMVectorSet(1,1,1,1);
        colors[1] = XMVectorSet(1,0,0,1);
        colors[2] = XMVectorSet(0,1,0,1);
        colors[3] = XMVectorSet(0,0,1,1);
        colors[4] = XMVectorSet(0,1,1,1);
        colors[5] = XMVectorSet(1,0,1,1);
        colors[6] = XMVectorSet(1,1,0,1);
        colors[7] = XMVectorSet(.5,.5,.5,1);

        m_pd3dDevice->SetVertexShaderConstantF(0, (FLOAT*)&matProjection, 4);

        m_pd3dDevice->SetVertexDeclaration(m_pColoredVertexDecl);
        m_pd3dDevice->SetVertexShader(m_pDiffuseColorVertexShader);
        m_pd3dDevice->SetPixelShader(m_pSimpleColorPixelShader);

        COLORED_VERTEX colorVertexBuffer[ colorVertexBufferSize ];
        INT numVertsInColorBuffer=0;

        for(INT i =0;i < DEPTH_IMAGE_HEIGHT; i++)
        {
            for(INT j=0; j < DEPTH_IMAGE_WIDTH; j++)
            {
                USHORT depthpixel = m_pDepthBuffer[i * DEPTH_IMAGE_WIDTH  + j];
                if(  ! ISZERO( depthpixel ) )
                {
                    COLORED_VERTEX vert;
                    XMVECTOR point3d;
                    FLOAT depthVal = GETDEPTH( depthpixel );

                    INT idx = PLAYERINDEX( depthpixel );
                    point3d =  Unproject( j, i, depthVal );

                    vert.position.x =  XMVectorGetX( point3d );
                    vert.position.y =  XMVectorGetY( point3d );
                    vert.position.z =  XMVectorGetZ( point3d );
                    vert.position.w =  1;

                    if ( m_DrawMode == DRAW_MODE_DEBUG_GRAPHICS )
                    {
                        // if the skelton with this id is being tracked modulate it with the curmple score;
                        vert.color = colors[idx];
                        if  ( idx > 0 )
                        {
                            FLOAT weight =  m_CrumpleDetector.OveralScore( idx-1);                               
                            if ( m_NuiFrame.SkeletonData[idx-1].eTrackingState == NUI_SKELETON_TRACKED  )
                            {
                                vert.color = vert.color * weight  +   XMVectorSet(1,1,1,1) * ( 1.0f - weight  );
                            }
                        }
                    }
                    else
                    {
                        vert.color = colors[0];
                    }
                    colorVertexBuffer[numVertsInColorBuffer++] = vert;

                    if ( numVertsInColorBuffer == colorVertexBufferSize )
                    {
                        m_pd3dDevice->DrawPrimitiveUP(D3DPT_POINTLIST, numVertsInColorBuffer, colorVertexBuffer, sizeof(COLORED_VERTEX));
                        numVertsInColorBuffer = 0;
                    }
                }
            }
        }
        if ( numVertsInColorBuffer > 0 )
        {
            m_pd3dDevice->DrawPrimitiveUP(D3DPT_POINTLIST, numVertsInColorBuffer, colorVertexBuffer, sizeof(COLORED_VERTEX));
            numVertsInColorBuffer = 0;
        }
    }

    renderSkeleton( m_NuiFrame, 0 , D3DCOLOR_RGBA(0, 255, 0,255));

    if ( m_DrawMode == DRAW_MODE_DEBUG_GRAPHICS || m_DrawMode == DRAW_MODE_SCORE_DETAILS )
    {          
        m_CrumpleDetector.DrawDebugGraphics(m_pd3dDevice, m_pDepthBuffer);    
    }
    for(INT k=0; k < NUI_SKELETON_COUNT; k++)
    {
        if ( m_NuiFrame.SkeletonData[k].eTrackingState == NUI_SKELETON_TRACKED  )
        {
            // draw a circle on the head proportinal to score
            XMVECTOR pos = m_NuiFrame.SkeletonData[k].SkeletonPositions[NUI_SKELETON_POSITION_HEAD];
            XMFLOAT3 pos3 ( XMVectorGetX( pos), XMVectorGetY( pos), XMVectorGetZ( pos) );

            ATG::DebugDraw::DrawSphere( pos3,
                (1.0f - m_CrumpleDetector.OveralScore(k)  )*.2f,
                D3DCOLOR_RGBA(255,0,0,255));
        }
    }

    // draw labels
    m_Font.Begin();
    m_Font.SetScaleFactors(1,1);
    WCHAR msg[1024];

    FLOAT currYPos=20;

    swprintf_s( msg,L"Frame Rate=%s", m_Timer.GetFrameRate() );
    m_Font.DrawText( 100,currYPos,0xffffffff,  msg );
    currYPos+=20;

    swprintf_s( msg,L"detect-time==%lf ms, maxtime == %lf", m_dbTimeProcessCrumpleDetection   * 1000, m_dbTimeProcessCrumpleDetectionMax * 1000  );
    m_Font.DrawText( 100,currYPos,0xffffffff,  msg );
    currYPos+=20;

    switch ( m_DrawMode )
    {
    case DRAW_MODE_SKELETON:
        swprintf_s( msg,L"skeleton" );
        break;
    case DRAW_MODE_SCORE_DETAILS:
        swprintf_s( msg,L"score-details" );
        break;
    case DRAW_MODE_DEBUG_GRAPHICS:
        swprintf_s( msg,L"debug-graphics" );
        break;
    };

    m_Font.DrawText( 100,currYPos,0xffffffff,  msg );	
    currYPos+=20;

    FLOAT xPos = 100;
    m_CrumpleDetector.DrawScores(m_Font, xPos, currYPos, m_DrawMode == DRAW_MODE_SCORE_DETAILS );

    m_Font.End();

    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    PIXEndNamedEvent();
    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: main()
// Desc: Entry point to the program
//--------------------------------------------------------------------------------------
INT __cdecl main()
{
    CCrumpleDetectionApp atgApp;
    ATG::GetVideoSettings( &atgApp.m_d3dpp.BackBufferWidth, &atgApp.m_d3dpp.BackBufferHeight );
    atgApp.m_d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_TWO;

    // Make sure display is gamma correct.
    atgApp.m_d3dpp.BackBufferFormat =  ( D3DFORMAT )MAKESRGBFMT( D3DFMT_A8R8G8B8 );
    atgApp.m_d3dpp.FrontBufferFormat = ( D3DFORMAT )MAKESRGBFMT( D3DFMT_LE_X8R8G8B8 );

    atgApp.Run();
}

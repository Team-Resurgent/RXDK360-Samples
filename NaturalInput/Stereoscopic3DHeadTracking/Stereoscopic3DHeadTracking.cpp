//--------------------------------------------------------------------------------------
// Stereoscopic3DHeadTracking.cpp
//
// This sample shows you how to set up projection matrices for a Stereoscopic 3D (S3D) scene.
// Two unique views are rendered (one for each eye) and the 
// result is placed in a frame-packed front buffer for consumption by the display device.
// This sample also shows how to move the viewpoint of the scene based on the head position
// returned by Kinect skeletal tracking.
//
// Microsoft Advanced Technology Group
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include <xtl.h>
#include <xnamath.h>
#include <xgraphics.h>
#include <AtgApp.h>
#include <AtgFont.h>
#include <AtgInput.h>
#include <AtgUtil.h>
#include <AtgHelp.h>
#include <AtgSceneAll.h>
#include <AtgNuiJointFilter.h>
#include <AtgSimpleShaders.h>
#include <AtgDebugDraw.h>
#include <stdio.h>

#include "CameraManager.h"

// These measurements are all in meters.  It doesn't matter what units are used, as long as it stays consistent for all 
// parameters.
const FLOAT HEAD_YOFFSET = -.18f;  // Offset to account for camera position above/below center of display

const FLOAT STEREO_NEARZ = 2.6f;
const FLOAT STEREO_FARZ = 200.0f;
const FLOAT STEREO_NEARZPHYSICAL = 1.3f;
const FLOAT STEREO_FARZPHYSICAL = 200.0f;
const FLOAT STEREO_ZPHYSICAL = 2.0f;
const FLOAT STEREO_DISPLAYDIAGONAL = .762f; // 30 inches

struct STEREO_PARAMETERS
{
    FLOAT fInterocular;         // Distance between the user's eyes
    FLOAT fFovY;                // Field of view of the camera
    FLOAT fAspectRatio;         // Aspect ratio of the camera
    FLOAT fNearZ;               // Near clip plane distance
    FLOAT fFarZ;                // Far clip plane distance
    FLOAT fNearZPhysical;       // Distance from viewer to closest extent of physical scene
    FLOAT fFarZPhysical;        // Distance from viewer to farthest extent of physical scene
    FLOAT fZPhysical;           // Distance from viewer to display
    FLOAT fDisplayDiagonal;     // Size of the display screen diagonal
};

//--------------------------------------------------------------------------------------
// Name: XMMatrixPerspectiveFovStereoLH()
// Desc: Generate projection matrices for the left and right stereoscopic 3D views
//       This is adapted from the paper "Controlling Perceived Depth in Stereoscopic Images"
//       by G. Jones, D. Lee, N. Holliman, and D. Ezra.
//--------------------------------------------------------------------------------------
HRESULT MatrixPerspectiveFovStereoLH( STEREO_PARAMETERS stereoParams, XMVECTOR pos, XMMATRIX* pLeftProj, XMMATRIX* pRightProj )
{
    if( stereoParams.fInterocular <= 0.0f ||
        stereoParams.fFovY <= 0.0f ||
        stereoParams.fFovY >= XM_PI ||
        stereoParams.fAspectRatio <= 0.0f  ||
        stereoParams.fDisplayDiagonal < 0.0f ||
        XMScalarNearEqual(stereoParams.fFarZ, stereoParams.fNearZ, 0.00001f) ||
        XMScalarNearEqual(stereoParams.fNearZPhysical, stereoParams.fFarZPhysical, 0.00001f) ||
        (stereoParams.fZPhysical - stereoParams.fNearZPhysical) >= -pos.z )
    {
        return E_FAIL;
    }

    FLOAT fDisplayWidth = stereoParams.fDisplayDiagonal * 16.0f/18.358f;   // Assuming 16:9 display
    FLOAT E = stereoParams.fInterocular;         // interocular distance
    FLOAT Z = stereoParams.fZPhysical;           // distance from viewer to physical display

    FLOAT F = stereoParams.fFarZPhysical - stereoParams.fZPhysical;
    FLOAT N = stereoParams.fZPhysical - stereoParams.fNearZPhysical;
   
    FLOAT dN = (N * E) / (Z - N);   // physical disparity at near plane
    FLOAT dF = (F * E) / (Z + F);   // physical disparity at far plane


    FLOAT R = dN/dF; // Ratio of near to far disparity.  This ratio is the same in the virtual world.

    FLOAT N_prime = stereoParams.fNearZ;                         // virtual near plane
    FLOAT F_prime = stereoParams.fFarZ;                          // virtual far plane
    FLOAT Z_prime = (R+1)/(1/N_prime + R/F_prime);  // depth of convergence (virtual)
    FLOAT H_prime = 2*Z_prime*tan(stereoParams.fFovY/2);         // height of virtual display
    FLOAT W_prime = H_prime*stereoParams.fAspectRatio;           // width of virtual display
    FLOAT W = fDisplayWidth;                        // width of physical display
    FLOAT S = W_prime/W;                            // ratio between virtual and physical display widths

    // Calculate the virtual camera separation.  
    FLOAT dN_prime = S * dN;
    FLOAT A = dN_prime*N_prime/(Z_prime-N_prime);  // separation of virtual cameras


    // We now have our baseline for the depth effect.  For a static viewpoint, we would stop here and create the projection
    // matrices.  Since we want to allow a moving viewpoint, we'll adjust the values we've created to  
    // the actual viewpoint.  We create a baseline first so that the depth effect stays consistent as the physical
    // viewpoint changes.  If we simply used the same near and far z with a moving viewpoint, for example, the depth effect would shift
    // as the physical viewpoint changed.

    FLOAT Z_ref = Z_prime;          // reference Z depth
    FLOAT F_ref = F_prime;          // far Z reference depth
    FLOAT N_ref = N_prime;          // near Z reference depth

    // Save the near and far depth amounts for the reference position.  This is what we need to keep consistent as the 
    // user's viewpoint moves in order to keep the depth effect consistent.
    FLOAT N_star = Z_ref-N_ref;     
    FLOAT F_star = F_ref-Z_ref;

    Z = -pos.z;                      // Actual physical distance from the display

    // Calculate the new disparities.  Note that this could violate the maximum and minimum eye angles passed to the function.
    // We allow this to happen because we are more interested in keeping the depth effect consistent, and the
    // angles will change only slightly.  
    dN = (N * E) / (Z - N);         // adjusted near disparity
    dF = (F * E) / (Z + F);         // adjusted far disparity

    R = dN/dF;                      // Adjusted disparity ratio

    // Calculate the new virtual Z_prime value and use this to adjust the near and far clip planes
    Z_prime = (R+1)*N_star*F_star/(R*F_star-N_star);  // Adjusted virtual distance to convergence
    N_prime = Z_prime - N_star;                             // Adjusted near clip plane
    F_prime = Z_prime + F_star;                             // Adjusted far clip plane

    // Calculate the virtual camera separation.  
    dN_prime = S * dN;
    A = dN_prime*N_prime/(Z_prime-N_prime);  // separation of virtual cameras
    
    FLOAT Q = A/E;

    // Calculate the left projection 
    // Calculate the off-center limits at the near plane: left, right, top, bottom
    FLOAT l = (-W_prime/2           // left side of the virtual display 
                + A/2               // shifted to the right by half the camera separation
                - pos.x * Q )       // add on the viewer's position, scaled by the ratio of interocular to camera separation
                * N_prime/Z_prime;  // scale the result so that it sits on the near plane

    // The other paramters are found similarly
    FLOAT r =  (W_prime/2+A/2-pos.x*Q) * N_prime/Z_prime;
    FLOAT t =  (H_prime/2-pos.y*Q) * N_prime/Z_prime;
    FLOAT b =  (-H_prime/2-pos.y*Q) * N_prime/Z_prime;

    // Simply doing an off-center projection will place the two viewpoints at the same origin.  We also 
    // need to translate the view point to the correct location.
    *pLeftProj = XMMatrixMultiply( XMMatrixTranslation( A/2 - pos.x*Q, -pos.y*Q, (-stereoParams.fZPhysical - pos.z)*Z_prime/Z),
                                XMMatrixPerspectiveOffCenterLH( l,r,b,t,N_prime, F_prime));


    // Calculate the right projection in the same manner as the left projection
    l = (-W_prime/2-A/2-pos.x*Q) * N_prime/Z_prime;
    r = (W_prime/2-A/2-pos.x*Q) * N_prime/Z_prime;
    t = (H_prime/2-pos.y*Q) * N_prime/Z_prime;
    b = (-H_prime/2-pos.y*Q) * N_prime/Z_prime;

    *pRightProj = XMMatrixMultiply( XMMatrixTranslation( -A/2 - pos.x*Q, -pos.y*Q, (-stereoParams.fZPhysical - pos.z)*Z_prime/Z),
                                XMMatrixPerspectiveOffCenterLH( l,r,b,t,N_prime, F_prime));

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Callouts for labeling the gamepad on the help screen
//--------------------------------------------------------------------------------------
ATG::HELP_CALLOUT g_HelpCallouts[] =
{
    { ATG::HELP_BACK_BUTTON,    ATG::HELP_PLACEMENT_2, L"Display\nhelp"                 },
    { ATG::HELP_Y_BUTTON,       ATG::HELP_PLACEMENT_1, L"Toggle 3D mode"                },
    { ATG::HELP_LEFT_TRIGGER,   ATG::HELP_PLACEMENT_1, L"Decrease object distance"      },
    { ATG::HELP_RIGHT_TRIGGER,  ATG::HELP_PLACEMENT_1, L"Increase object distance"      },
    { ATG::HELP_LEFT_SHOULDER,  ATG::HELP_PLACEMENT_1, L"Increase rotation velocity"    },
    { ATG::HELP_RIGHT_SHOULDER, ATG::HELP_PLACEMENT_1, L"Decrease rotation velocity"    },
    { ATG::HELP_DPAD,           ATG::HELP_PLACEMENT_1, L"Increase/decrease interocular distance"},
};

static const DWORD      NUM_HELP_CALLOUTS = sizeof( g_HelpCallouts ) / sizeof( g_HelpCallouts[0] );

const INT               NUM_FRONT_BUFFERS = 2;  // Double buffered front buffer to avoid stalls


//--------------------------------------------------------------------------------------
// Vertex shader for the Environment
//--------------------------------------------------------------------------------------
const CHAR* m_strVertexShaderProgramScene =
    " float4x4 matWVP : register(c0);              \n"
    " float4x4 g_matWorld : register(c4);          \n"
    "                                              \n"
    " struct VS_IN                                 \n"
    "                                              \n"
    " {                                            \n"
    "     float4 ObjPos   : POSITION;              \n"
    "     float2 Texture  : TEXCOORD0;             \n"
    "     float3 Normal   : NORMAL;                \n"
    " };                                           \n"
    "                                              \n"
    " struct VS_OUT                                \n"
    " {                                            \n"
    "     float4 ProjPos  : POSITION;              \n"
    "     float2 Texture  : TEXCOORD0;             \n"
    "     float3 Normal   : TEXCOORD1;             \n"
    "     float  Alpha    : TEXCOORD2;             \n"
    " };                                           \n"
    "                                              \n"
    " VS_OUT main( VS_IN In )                      \n"
    " {                                            \n"
    "     VS_OUT Out;                              \n"
    "     Out.ProjPos = mul( matWVP, In.ObjPos );  \n"
    "     Out.Normal = In.Normal;                  \n"
    "     Out.Texture = In.Texture;                \n"
    "     Out.Alpha = 1.0f;                        \n"
    "     return Out;                              \n"
    " }                                            \n";


//-------------------------------------------------------------------------------------
// Scene Pixel shader
//-------------------------------------------------------------------------------------
const CHAR* m_strPixelShaderProgramScene =
    " struct PS_IN                                                          \n"
    " {                                                                     \n"
    "     float2 Texture : TEXCOORD0;                                       \n"
    "     float3 vNormal : TEXCOORD1;                                       \n"
    "     float  Alpha   : TEXCOORD2;                                       \n"
    " };                                                                    \n"
    "                                                                       \n"
    " sampler TextureSampler0 : register(s0);                               \n"
    "                                                                       \n"
    " float4 main( PS_IN In ) : COLOR                                       \n"
    " {                                                                     \n"
    "     float4 textureColor = tex2D( TextureSampler0, In.Texture );       \n"
    "     textureColor *= In.Alpha;                                         \n"
    "     float3 vLightDir1 = float3( -1.0f, 1.0f, -1.0f );                 \n"
    "     float3 vLightDir2 = float3( 1.0f, 1.0f, -1.0f );                  \n"
    "     float3 vLightDir3 = float3( 0.0f, -1.0f, 0.0f );                  \n"
    "     float3 vLightDir4 = float3( 1.0f, 1.0f, 1.0f );                   \n" 
    "     float fLighting = 0.4f +                                          \n"
    "                  saturate( dot( vLightDir1 , In.vNormal ) )*0.25f +   \n"
    "                  saturate( dot( vLightDir2 , In.vNormal ) )*0.25f +   \n"
    "                  saturate( dot( vLightDir3 , In.vNormal ) )*0.25f +   \n"
    "                  saturate( dot( vLightDir4 , In.vNormal ) )*0.25f ;   \n"
    "     return textureColor * fLighting;                                  \n"
    " }                                                                     \n";


//-------------------------------------------------------------------------------------
// Anaglyph Pixel shader
//
// Anaglyph should be avoided in games, but it can be useful during development
// for people that don't have a 3D enabled display 
//-------------------------------------------------------------------------------------
const CHAR* m_strPixelShaderProgramAnaglyph =
    " struct PS_IN                                                          \n"
    " {                                                                     \n"
    "     float4 scrPos   : VPOS;                                           \n"
    " };                                                                    \n"
    "                                                                       \n"
    " sampler TextureSampler0 : register(s0);                               \n"
    " sampler TextureSampler1 : register(s1);                               \n"
    "                                                                       \n"
    " float2 fScreenSize: register(c0);                                     \n"
    "                                                                       \n"
    " float4 main( PS_IN In ) : COLOR                                       \n"
    " {                                                                     \n"
    "     float2 uv = (In.scrPos.xy + float2(.5f, .5f)) / float2(fScreenSize.x, fScreenSize.y);  \n"
    "     float4 color0 = tex2D( TextureSampler0, uv );                     \n"
    "     float4 color1 = tex2D( TextureSampler1, uv );                     \n"
    "     return ( float4( color0.x * 0.0 + color0.y*.7 + color0.z*.3,0,0, 1.0f) +  \n"
    "              float4( 0, color1.g, color1.b, 1.0f) );                  \n"
    " }                                                                     \n";

// Defines what mode the user has selected for stereoscopic rendering
enum S3D_MODE
{
    S3D_ANAGLYPH,
    S3D_FRAME_PACKED,
    S3D_OFF
};

//--------------------------------------------------------------------------------------
// Name: class Sample
// Desc: Main class to run this application. Most functionality is inherited
//       from the ATG::Application base class.
//--------------------------------------------------------------------------------------
class Sample : public ATG::Application
{
private:
    ATG::Timer  m_Timer;             // Timer
    ATG::Font   m_Font;              // Font for drawing text
    ATG::Help   m_Help;
    BOOL        m_bDrawHelp;

    ATG::Scene* m_pCharacterScene;

    XMFLOAT3    m_vPosition;        // Position of the object in the scene
    XMVECTOR    m_vHeadPosition;    // Position of the user's head

    // variables for constantly rotating the character scene
    FLOAT       m_fRotAngle;
    FLOAT       m_fRotationVel;

    // Transform matrices
    XMMATRIX    m_matView;
    XMMATRIX    m_matProj;
    XMMATRIX    m_matWorld;
    
    LPDIRECT3DVERTEXSHADER9         m_pVS;
    LPDIRECT3DPIXELSHADER9          m_pPS;
    LPDIRECT3DVERTEXDECLARATION9    m_pVertexDecl;

    // Support for anaglyph rendering
    LPDIRECT3DPIXELSHADER9  m_pPSAnaglyph;
    IDirect3DTexture9*      m_pTexLeftEye;
    IDirect3DTexture9*      m_pTexRightEye;

    IDirect3DTexture9* m_pFrontBuffer[NUM_FRONT_BUFFERS];
    INT m_nCurFrontBuffer;

    D3DSurface* m_pSurface;
    D3DSurface* m_pDepthStencilSurface;

    S3D_MODE m_S3DMode;
    BOOL     m_bFramePackedS3DCapable;
    DWORD    m_VideoCaps;

    FLOAT m_fInterocular;

    // NUI
    CameraManager m_CameraManager;

public:
    // This struct holds information about how to render a frame packed S3D surface
    // it is queried for at initialization time, and then used in later rendering.
    XGSTEREOPARAMETERS m_StereoParams;
    BOOL m_bSensorConnected;
    
    
private:
    VOID UpdateNUI ( const NUI_SKELETON_DATA* pSkeletonData );
    VOID DrawScene( CXMMATRIX matView, CXMMATRIX matProj, XGSTEREOREGION* pRegion );
    VOID Set3DMode( S3D_MODE S3DMode );
    HRESULT InitD3D();

public:
    virtual HRESULT     Initialize();
    virtual HRESULT     Update();
    virtual HRESULT     Render();    
};

//--------------------------------------------------------------------------------------
// Name: main()
// Desc: Entry point to the program
//--------------------------------------------------------------------------------------
VOID main() 
{
    Sample AtgApp;
    
    UINT nWidth;
    UINT nHeight;
    ATG::GetVideoSettings(&nWidth, &nHeight, NULL);

    AtgApp.m_d3dpp.BackBufferWidth = nWidth;
    AtgApp.m_d3dpp.BackBufferHeight = nHeight;
    
    // Make sure display is gamma correct.
    AtgApp.m_d3dpp.BackBufferFormat =  ( D3DFORMAT )MAKESRGBFMT( D3DFMT_A8R8G8B8 );
    AtgApp.m_d3dpp.FrontBufferFormat = ( D3DFORMAT )MAKESRGBFMT( D3DFMT_LE_X8R8G8B8 );

    // If using the NUI sensor, use the same 30 Hz refresh rate as the sensor.
    if( XNuiGetHardwareStatus() & (XNUI_HARDWARE_STATUS_CONNECTED | XNUI_HARDWARE_STATUS_READY) )
    {
        AtgApp.m_bSensorConnected = TRUE;
        AtgApp.m_d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_TWO;
    }
    else
    {
        AtgApp.m_bSensorConnected = FALSE;
        AtgApp.m_d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_ONE;
    }

    AtgApp.m_dwDeviceCreationFlags |= D3DCREATE_BUFFER_2_FRAMES;
    
    // Disable automatic frame buffer creation.  The S3D frame-packed buffer is a special case that
    // needs special handling.
    AtgApp.m_d3dpp.DisableAutoBackBuffer = TRUE;
    AtgApp.m_d3dpp.DisableAutoFrontBuffer = TRUE;
    AtgApp.m_d3dpp.EnableAutoDepthStencil = FALSE;

    AtgApp.Run();
}


//--------------------------------------------------------------------------------------
// Name: Initialize()
// Desc: Initialize app-dependent objects
//--------------------------------------------------------------------------------------
HRESULT Sample::Initialize()
{
    // Default parameter values
    m_fRotationVel = .1f;
    m_fRotAngle = 0.0f;
    m_bDrawHelp = FALSE;
    m_vPosition = XMFLOAT3( 0.0f, -.05f, 3.88f ); 
    m_vHeadPosition = XMVectorSet( 0.0f, 0.0f, -1.0f, 0.0f ); 
    m_S3DMode = S3D_FRAME_PACKED;
    m_bFramePackedS3DCapable = FALSE;
    m_fInterocular = .05f;
    m_nCurFrontBuffer = 0;

    for( INT i = 0; i < NUM_FRONT_BUFFERS; i++)
    {
        m_pFrontBuffer[i] = NULL;
    }

    m_pSurface = NULL;
    m_pDepthStencilSurface = NULL;

    HRESULT hr;

    // Create the font
    if( FAILED( hr = m_Font.Create( "game:\\Media\\Fonts\\Arial_16.xpr" ) ) )
    {
        ATG_PrintError( "Couldn't create font\n" );
        return hr;
    }

   // Confine text drawing to the title safe area
    m_Font.SetWindow( ATG::GetTitleSafeArea() );

    // Create the help
    if( FAILED( hr = m_Help.Create( "game:\\Media\\Help\\Help.xpr" ) ) )
    {
        ATG_PrintError( "Couldn't create help\n" );
        return hr;
    }

    InitD3D();

    // Get the capabilities of the display.  This determines if we can switch to frame-packed S3D mode.
    m_VideoCaps = XGetVideoCapabilities();
    m_bFramePackedS3DCapable = ( m_VideoCaps & XC_VIDEO_STEREOSCOPIC_3D_ENABLED )  &&    // S3D is enabled in the dashboard (default is enabled)
                               ( m_VideoCaps & XC_VIDEO_STEREOSCOPIC_3D_720P_60HZ );     // Display device supports 720p60Hz S3D
    Set3DMode( m_S3DMode );

    static const D3DVERTEXELEMENT9 decl[] =
    {
        { 0,  0, D3DDECLTYPE_FLOAT4,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
        { 0, 16, D3DDECLTYPE_FLOAT2,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
        { 0, 24, D3DDECLTYPE_FLOAT3,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_NORMAL,   0 },
        D3DDECL_END()
    };

    m_pd3dDevice->CreateVertexDeclaration( decl, &m_pVertexDecl );

    // Initialize the NUI sensor
    if( m_bSensorConnected )
    {
        if( FAILED( hr = m_CameraManager.InitializeCamera( m_pd3dDevice ) ) )
        {
            ATG_PrintError( "Couldn't create the natural input device.\n" );
            return hr;
        }
    }
 
    // Create the vertex shader
    ID3DXBuffer* pShaderCode = NULL;
    ID3DXBuffer* pErrorMsg = NULL;
    D3DXCompileShader( m_strVertexShaderProgramScene, 
        ( UINT )strlen( m_strVertexShaderProgramScene ),  NULL, NULL, "main", "vs.3.0", 0, 
        &pShaderCode, &pErrorMsg, NULL );
    m_pd3dDevice->CreateVertexShader( ( DWORD* )pShaderCode->GetBufferPointer(),
        &m_pVS );
    pShaderCode->Release();

    // Create pixel shaders
    D3DXCompileShader( m_strPixelShaderProgramScene, 
        ( UINT )strlen( m_strPixelShaderProgramScene ), NULL, NULL, "main", "ps.3.0", 0,
        &pShaderCode, &pErrorMsg, NULL );        
    m_pd3dDevice->CreatePixelShader( ( DWORD* )pShaderCode->GetBufferPointer(),
        &m_pPS );
    pShaderCode->Release();

    D3DXCompileShader( m_strPixelShaderProgramAnaglyph, 
        ( UINT )strlen( m_strPixelShaderProgramAnaglyph ), NULL, NULL, "main", "ps.3.0", 0,
        &pShaderCode, &pErrorMsg, NULL );        
    m_pd3dDevice->CreatePixelShader( ( DWORD* )pShaderCode->GetBufferPointer(),
        &m_pPSAnaglyph );
    pShaderCode->Release();

    ATG::SimpleShaders::Initialize( NULL, NULL);

    // Load the scene
    m_pCharacterScene = new ATG::Scene();
    assert( m_pCharacterScene );
    
    hr = ATG::SceneFileParser::LoadXATGFile( "game:\\media\\scenes\\rex.xatg", m_pCharacterScene, NULL,
        ATG::XATGLOADER_DONOTINITIALIZEMATERIALS, NULL );
    if( FAILED( hr ) )
    {
        ATG_PrintError( "Couldn't load Scene\n" );
        return hr;
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Update()
// Desc: Called once per frame.  This call is the entry point for animating the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Update()
{
    // Get the elapsed time
    FLOAT fElapsedTime = ( FLOAT )m_Timer.GetElapsedTime();
    m_fRotAngle += m_fRotationVel * fElapsedTime;

    m_matWorld =  XMMatrixRotationAxis( XMVectorSet(1,0,0,1), XMConvertToRadians(90.0f)) * XMMatrixRotationAxis( XMVectorSet( 0, 1, 0, 0), m_fRotAngle );

    // Get the current gamepad state
    ATG::GAMEPAD* pGamepad = ATG::Input::GetMergedInput();

    // Toggle help
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_BACK )
        m_bDrawHelp = !m_bDrawHelp;

    static const FLOAT dPosition = 5.0f;
    if( pGamepad->bLeftTrigger )
    {
        m_vPosition.z -= dPosition*fElapsedTime;
    }
    if( pGamepad->bRightTrigger )
    {
        m_vPosition.z += dPosition*fElapsedTime;
    }

    static const FLOAT fRotationAccel = .05f;
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER )
    {
        m_fRotationVel += fRotationAccel;
    }
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_LEFT_SHOULDER )
    {
        m_fRotationVel -= fRotationAccel;
    }

    static const FLOAT fInterocularIncrement = .005f;
    static const FLOAT fMaxInterocular = .07f;
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_DPAD_UP )
    {
        m_fInterocular += fInterocularIncrement;
    }
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_DPAD_DOWN )
    {
        m_fInterocular -= fInterocularIncrement;
    }
    m_fInterocular = max( m_fInterocular, fInterocularIncrement );
    m_fInterocular = min( m_fInterocular, fMaxInterocular );

    // Update NUI
    BOOL bSkeletonTracked = FALSE;
    if( m_bSensorConnected )
    {
        if ( m_CameraManager.Update( fElapsedTime, NULL ) )
        {
            NUI_TRANSFORM_SMOOTH_PARAMETERS smoothingParams;
            smoothingParams.fCorrection = .5f;
            smoothingParams.fJitterRadius = .05f;
            smoothingParams.fMaxDeviationRadius = .05f;
            smoothingParams.fPrediction = .5f;
            smoothingParams.fSmoothing = .5f;

            NUI_SKELETON_FRAME* pSkeletonFrame = m_CameraManager.GetSkeleton();
            NuiTransformSmooth( pSkeletonFrame, &smoothingParams );

            // Update if skeleton was tracked
            for (UINT uCurrentSkeleton = 0; uCurrentSkeleton < NUI_SKELETON_COUNT; uCurrentSkeleton++)
            {
                // In this sample, we just take the first skeleton available to simplify things.  You may want to do something more advanced
                // in a full title
                if ( m_CameraManager.GetSkeleton()->SkeletonData[uCurrentSkeleton].eTrackingState == NUI_SKELETON_TRACKED )
                {
                    UpdateNUI( &m_CameraManager.GetSkeleton()->SkeletonData[uCurrentSkeleton] );
                    bSkeletonTracked = TRUE;
                    break;
                }
            }
        }
        else
        {
            // Reset the position if tracking is lost
	        m_vHeadPosition.x = 0.0f;
	        m_vHeadPosition.y = 0.0f;
	        m_vHeadPosition.z = -1.0f;
        }
    }

    // Toggle S3D mode
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_Y )
    {
        switch( m_S3DMode )
        {
        case S3D_ANAGLYPH:
            Set3DMode( S3D_FRAME_PACKED );
            break;

        case S3D_FRAME_PACKED:
            Set3DMode( S3D_OFF );
            break;

        case S3D_OFF:
        default:
            Set3DMode( S3D_ANAGLYPH );
            break;
        }
    }

    // Create the view matrix
    XMVECTOR vPosition = XMVectorSet( m_vPosition.x, m_vPosition.y, m_vPosition.z, 0.0f );
    XMVECTOR vLookAt = XMVectorSet( m_vPosition.x, m_vPosition.y, 0.0f, 0.0f );
    XMVECTOR vUp = XMVectorSet( 0.0f, 1.0f, 0.0f, 0.0f );

    m_matView = XMMatrixLookAtLH( vPosition, vLookAt, vUp );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Set3DMode()
// Desc: Sets the 3D mode of D3D.  This also causes a signal to be sent to the HDMI S3D
//       display to change into or out of stereoscopic 3D mode.  As the device is 
//       released and then recreated, display output will be interrupted for a moment.
//--------------------------------------------------------------------------------------
VOID Sample::Set3DMode( S3D_MODE S3DMode ) 
{
    // Switch the device's stereoscopic flag on or off.  Leave all other flags the same
    if( S3DMode == S3D_FRAME_PACKED && m_bFramePackedS3DCapable )
    {
        if( m_d3dpp.Flags & D3DPRESENTFLAG_STEREOSCOPIC_720P_60HZ )
        {
            // Device is already in the right mode
            m_S3DMode = S3DMode;
            return;
        }
        // WARNING: If the console is not connected to an HDMI S3D display (via HDMI)
        // and this flag is enabled, D3D will crash during the next swap. 
        m_d3dpp.Flags |= D3DPRESENTFLAG_STEREOSCOPIC_720P_60HZ;
    }
    else
    {
        if( !(m_d3dpp.Flags & D3DPRESENTFLAG_STEREOSCOPIC_720P_60HZ) )
        {
            // Device is already in the right mode
            m_S3DMode = S3DMode;
            return;
        }

        m_d3dpp.Flags &= ~D3DPRESENTFLAG_STEREOSCOPIC_720P_60HZ;
    }

    m_S3DMode = S3DMode;

    UINT nWidth, nHeight;
    ATG::GetVideoSettings( &nWidth, &nHeight );
       
    if( m_S3DMode == S3D_FRAME_PACKED && m_bFramePackedS3DCapable )
    {
        // Get the stereo parameters based on the desired frame buffer size
        XGGetStereoParameters( nWidth, nHeight, D3DMULTISAMPLE_NONE, 0, &m_StereoParams );

        m_d3dpp.BackBufferWidth = m_StereoParams.FrontBufferWidth;
        m_d3dpp.BackBufferHeight = m_StereoParams.FrontBufferHeight;
    }
    else
    {
        m_d3dpp.BackBufferWidth = nWidth;
        m_d3dpp.BackBufferHeight = nHeight;
    }

    if( FAILED( m_pd3dDevice->Reset( &m_d3dpp ) ) ) 
    {
        ATG_PrintError( "Unable to reset D3D device!\n" );
        DebugBreak();
    }

    InitD3D();
}


//--------------------------------------------------------------------------------------
// Name: InitD3D()
// Desc: Initialize Direct3D
//--------------------------------------------------------------------------------------
HRESULT Sample::InitD3D()
{
    HRESULT hr = S_OK;

    // Create the front buffer
    m_pd3dDevice->UnsetAll();

    for( INT i = 0; i < NUM_FRONT_BUFFERS; i++)
    {
        SAFE_RELEASE( m_pFrontBuffer[i] );

        if( FAILED(hr = m_pd3dDevice->CreateTexture( m_d3dpp.BackBufferWidth, m_d3dpp.BackBufferHeight, 1, 0,D3DFMT_LE_X8R8G8B8, 0, &m_pFrontBuffer[i], NULL)))
            return hr;
    }
    m_nCurFrontBuffer = 0;

    SAFE_RELEASE( m_pSurface );
    SAFE_RELEASE( m_pDepthStencilSurface );

    if( m_S3DMode == S3D_FRAME_PACKED && m_bFramePackedS3DCapable )
    {

        // Create the render target and depth stencil surface.  As we don't need to do any tiling, the size of these surfaces is taken directly from the eye buffer sizes
        if( FAILED( hr = m_pd3dDevice->CreateRenderTarget( m_StereoParams.EyeBufferWidth, m_StereoParams.EyeBufferHeight, D3DFMT_A8R8G8B8, D3DMULTISAMPLE_NONE, 0, 0, &m_pSurface, NULL) ) )
            return hr;
        if( FAILED( hr = m_pd3dDevice->CreateDepthStencilSurface( m_StereoParams.EyeBufferWidth, m_StereoParams.EyeBufferHeight, D3DFMT_D24S8, D3DMULTISAMPLE_NONE, 0, 0, &m_pDepthStencilSurface, NULL) ) )
            return hr;
    }
    else
    {
        if( FAILED( hr = m_pd3dDevice->CreateRenderTarget( m_d3dpp.BackBufferWidth, m_d3dpp.BackBufferHeight, D3DFMT_A8R8G8B8, D3DMULTISAMPLE_NONE, 0, 0, &m_pSurface, NULL) ) )
            return hr;
        if( FAILED( hr = m_pd3dDevice->CreateDepthStencilSurface( m_d3dpp.BackBufferWidth, m_d3dpp.BackBufferHeight, D3DFMT_D24S8, D3DMULTISAMPLE_NONE, 0, 0, &m_pDepthStencilSurface, NULL) ) )
            return hr;

        // Left and right eye textures.  These aren't needed for S3D_OFF, but it reduces branching in the render path a little.
        m_pd3dDevice->CreateTexture( m_d3dpp.BackBufferWidth, m_d3dpp.BackBufferHeight, 1, 0,D3DFMT_A8R8G8B8, 0, &m_pTexLeftEye, NULL);
        m_pd3dDevice->CreateTexture( m_d3dpp.BackBufferWidth, m_d3dpp.BackBufferHeight, 1, 0,D3DFMT_A8R8G8B8, 0, &m_pTexRightEye, NULL);
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
// UpdateNUI()
//
// Update the head location based on NUI skeletal tracking
//--------------------------------------------------------------------------------------
VOID Sample::UpdateNUI ( const NUI_SKELETON_DATA* pSkeletonData )
{

    if( pSkeletonData->eTrackingState == NUI_SKELETON_TRACKED )
    {
         if( pSkeletonData->eSkeletonPositionTrackingState[ NUI_SKELETON_POSITION_HEAD ] != 
                NUI_SKELETON_POSITION_NOT_TRACKED ) 
        {
            XMFLOAT4 fHead; 
            
            XMVECTOR vHead = pSkeletonData->SkeletonPositions[ NUI_SKELETON_POSITION_HEAD ];

            XMStoreFloat4( &fHead, vHead );
            
            m_vHeadPosition.x = vHead.x;
            m_vHeadPosition.y = ( vHead.y ) - HEAD_YOFFSET; 
            m_vHeadPosition.z = -vHead.z;       // Switch to left-handed coordinates
            
        }
    }
    else
    {
        m_vHeadPosition.x = 0.0f;
        m_vHeadPosition.y = 0.0f;
        m_vHeadPosition.z = -1.0f;
    }
}


//--------------------------------------------------------------------------------------
// Name: Render()
// Desc: Sets up render states, clears the viewport, and renders the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Render()
{
    // Show title, frame rate, and help
    m_Timer.MarkFrame();

    // Setup render targets
    m_pd3dDevice->SetRenderTarget(0, m_pSurface );
    m_pd3dDevice->SetDepthStencilSurface( m_pDepthStencilSurface );
       
    UINT nWidth, nHeight;
    ATG::GetVideoSettings( &nWidth, &nHeight );

    // Create the projection matrix
    STEREO_PARAMETERS stereoParams;
    stereoParams.fInterocular = m_fInterocular;
    stereoParams.fFovY = XMConvertToRadians(20.0f);
    stereoParams.fAspectRatio = ( FLOAT )nWidth / ( FLOAT )nHeight;

    stereoParams.fNearZ = STEREO_NEARZ;
    stereoParams.fFarZ = STEREO_FARZ;
    stereoParams.fNearZPhysical = STEREO_NEARZPHYSICAL;
    stereoParams.fFarZPhysical = STEREO_FARZPHYSICAL;
    stereoParams.fZPhysical = STEREO_ZPHYSICAL;
    stereoParams.fDisplayDiagonal = STEREO_DISPLAYDIAGONAL;


    static FLOAT fNearZ = 2.6f;     // const
    static FLOAT fFarZ  = 200.0f;

    if( m_S3DMode == S3D_ANAGLYPH || ( m_S3DMode == S3D_FRAME_PACKED && m_bFramePackedS3DCapable) )
    {
        XMMATRIX leftProj, rightProj;
        HRESULT hr = MatrixPerspectiveFovStereoLH(  stereoParams,
                                                      m_vHeadPosition,
                                                      &leftProj,
                                                      &rightProj );
        if( FAILED(hr) )
        {
            return hr;
        }

        if( m_S3DMode == S3D_FRAME_PACKED )
        {
            // Clear the blank area of the front buffer.
            //
            // Note: The left and right eye blank regions must be the same color as the invariant blank area.  D3D will
            // display warning debug text in debug and profile builds if this is not the case.  In release and LTCG
            // builds, this check is not performed, but the title should still conform to this rule, or its stereo
            // 3D output may not work with some TVs.  This will also fail stereoscopic TCRs and the HDMI standard.
            m_pd3dDevice->Clear( 1, &m_StereoParams.BlankRegion.BlankRectTop, D3DCLEAR_TARGET, 0, 0, 0);

            for (int i = 0; i < NUM_FRONT_BUFFERS; i++)
            {
                m_pd3dDevice->Resolve(  D3DRESOLVE_ALLFRAGMENTS, &m_StereoParams.BlankRegion.ResolveSourceRect, 
                                        m_pFrontBuffer[i], &m_StereoParams.BlankRegion.ResolveDestPoint, 0, 0,
                                        NULL, 0, 0, NULL);
            }

        }

        PIXBeginNamedEvent(0, "Render Views");
        {
            D3DVECTOR4 clearColor = {0.0f, 0.0f, 0.0f, 0.0f };
            if( m_S3DMode == S3D_FRAME_PACKED )
            {
                DrawScene( m_matWorld * m_matView, leftProj, &m_StereoParams.LeftEye );
                m_pd3dDevice->Resolve( D3DRESOLVE_RENDERTARGET0 | D3DRESOLVE_CLEARRENDERTARGET | D3DRESOLVE_CLEARDEPTHSTENCIL, &m_StereoParams.LeftEye.ResolveSourceRect, m_pFrontBuffer[m_nCurFrontBuffer], &m_StereoParams.LeftEye.ResolveDestPoint, 0, 0, &clearColor, 1.0f, 0, NULL );
                DrawScene( m_matWorld * m_matView, rightProj, &m_StereoParams.RightEye );
                m_pd3dDevice->Resolve( D3DRESOLVE_RENDERTARGET0, &m_StereoParams.RightEye.ResolveSourceRect, m_pFrontBuffer[m_nCurFrontBuffer], &m_StereoParams.RightEye.ResolveDestPoint, 0, 0, NULL, 0.0f, 0, NULL );
            }
            else if ( m_S3DMode == S3D_ANAGLYPH )
            {
                DrawScene( m_matWorld * m_matView, leftProj, NULL );
                m_pd3dDevice->Resolve( D3DRESOLVE_RENDERTARGET0 | D3DRESOLVE_CLEARRENDERTARGET | D3DRESOLVE_CLEARDEPTHSTENCIL, NULL, m_pTexLeftEye, NULL, 0, 0, &clearColor, 1.0f, 0, NULL );
                DrawScene( m_matWorld * m_matView, rightProj, NULL );
                m_pd3dDevice->Resolve( D3DRESOLVE_RENDERTARGET0, NULL, m_pTexRightEye, NULL, 0, 0, NULL, 0.0f, 0, NULL );
  
                PIXBeginNamedEvent(0, "Merge Views");
                {
                    m_pd3dDevice->SetPixelShader( m_pPSAnaglyph ); 
                    m_pd3dDevice->SetVertexShader( m_pVS );
                    m_pd3dDevice->SetVertexDeclaration( m_pVertexDecl );

                    m_pd3dDevice->SetTexture( 0, m_pTexLeftEye );
                    m_pd3dDevice->SetTexture( 1, m_pTexRightEye );
                    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, FALSE );

                    XMMATRIX matVP = XMMatrixIdentity();
                    m_pd3dDevice->SetVertexShaderConstantF( 0, ( FLOAT* )&matVP, 4 );

                    XMFLOAT4 fScreenSize;
                    fScreenSize.x = (FLOAT)m_d3dpp.BackBufferWidth;
                    fScreenSize.y = (FLOAT)m_d3dpp.BackBufferHeight;
                    m_pd3dDevice->SetPixelShaderConstantF( 0, ( FLOAT* )&fScreenSize, 1 );

                    struct VERTEX
                    {
                        XMFLOAT4 pos;
                        XMFLOAT2 uv;
                        XMFLOAT3 norm;
                    };
        
                    VERTEX verts[3];
                    verts[0].pos = XMFLOAT4( (FLOAT)m_d3dpp.BackBufferWidth, (FLOAT)m_d3dpp.BackBufferHeight, 1.0f, 0.0f);
                    verts[1].pos = XMFLOAT4(                           0.0f, (FLOAT)m_d3dpp.BackBufferHeight, 1.0f, 0.0f);
                    verts[2].pos = XMFLOAT4(                           0.0f,                            0.0f, 1.0f, 0.0f);

                    m_pd3dDevice->SetRenderState( D3DRS_VIEWPORTENABLE, FALSE );

                    m_pd3dDevice->DrawPrimitiveUP( D3DPT_RECTLIST, 1, verts, sizeof(VERTEX) );
                    m_pd3dDevice->Resolve( D3DRESOLVE_RENDERTARGET0, NULL, m_pFrontBuffer[m_nCurFrontBuffer], NULL, 0, 0, NULL, 0.0f, 0, NULL );

                }
                PIXEndNamedEvent();
            }

        }
        PIXEndNamedEvent();
    }
    else  // No S3D
    {
        XMMATRIX matProj = XMMatrixPerspectiveFovLH( stereoParams.fFovY, stereoParams.fAspectRatio, fNearZ, fFarZ );
        DrawScene( m_matWorld * m_matView, matProj, NULL );
        m_pd3dDevice->Resolve( D3DRESOLVE_RENDERTARGET0, NULL, m_pFrontBuffer[m_nCurFrontBuffer], NULL, 0, 0, 0, 0, 0, NULL );
    }

    m_pd3dDevice->SynchronizeToPresentationInterval();

    // NOTE:  D3D will crash during the first swap call after enabling HDMI frame packed mode if the 
    // console is not connected to an S3D compatible device via an HDMI cable.  Functionality for
    // HDMI S3D mode enumeration will be added in a future release.
    m_pd3dDevice->Swap( m_pFrontBuffer[m_nCurFrontBuffer], NULL );

    m_nCurFrontBuffer = (m_nCurFrontBuffer+1) % NUM_FRONT_BUFFERS;

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: DrawScene()
// Desc: Draws the scene
//--------------------------------------------------------------------------------------
VOID Sample::DrawScene( CXMMATRIX matView, CXMMATRIX matProj, XGSTEREOREGION* pRegion )
{
    // Initialize default device states
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_NONE );
    m_pd3dDevice->SetRenderState( D3DRS_VIEWPORTENABLE, TRUE );
    m_pd3dDevice->SetSamplerFilterStates(0, D3DTEXF_ANISOTROPIC, D3DTEXF_ANISOTROPIC, D3DTEXF_LINEAR, 16);
    m_pd3dDevice->SetSamplerFilterStates(1, D3DTEXF_ANISOTROPIC, D3DTEXF_ANISOTROPIC, D3DTEXF_LINEAR, 16);

    XMMATRIX matWVP = matView * matProj ;
    m_pd3dDevice->SetVertexShaderConstantF( 0, ( FLOAT* )&matWVP, 4 );
 
    // Setup viewport for S3D rendering
    if( pRegion )
    {
        UINT nWidth, nHeight;
        ATG::GetVideoSettings( &nWidth, &nHeight );

        // Clear the top blank region ( which is outside of our normal viewport).
        //
        // NOTE: The left and right eye blank regions must be the same color as the invariant blank area.  D3D will
        // display warning debug text in debug and profile builds if this is not the case.  In release and ltcg
        // builds, this check is not performed, but the title should still conform to this rule, or its stereo
        // 3D output may not work with some TVs.
        if( pRegion->BlankRectTop.x2 != pRegion->BlankRectTop.x1 )
        {
            D3DVIEWPORT9 blankViewport = { 0, pRegion->BlankRectTop.y1, nWidth, pRegion->BlankRectTop.y2 - pRegion->BlankRectTop.y1 };
            m_pd3dDevice->SetViewport(&blankViewport);
            m_pd3dDevice->Clear(0, NULL, D3DCLEAR_TARGET, 0, 0, 0);
        }

        // Clear the bottom blank region (which is outside of our normal viewport).
        if( pRegion->BlankRectBottom.x2 != pRegion->BlankRectBottom.x1 )
        {
            D3DVIEWPORT9 blankViewport = { 0, pRegion->BlankRectBottom.y1, nWidth, pRegion->BlankRectBottom.y2 - pRegion->BlankRectBottom.y1 };

            m_pd3dDevice->SetViewport(&blankViewport);
            m_pd3dDevice->Clear( 0, NULL, D3DCLEAR_TARGET, 0, 0, 0);
        }

        // Set the normal viewport for this eye
        D3DVIEWPORT9 renderViewport = { 0, pRegion->ViewportYOffset, nWidth, nHeight, 0, 1 };

        m_pd3dDevice->SetViewport(&renderViewport);
        m_pd3dDevice->Clear( 0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER , 0, 1.0, 0);
    }
    else
    {
        m_pd3dDevice->Clear( 0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, 0, 1.0, 0);
    }

    // Render the scene
    m_pd3dDevice->SetPixelShader( m_pPS );    
    m_pd3dDevice->SetVertexShader( m_pVS );
    m_pd3dDevice->SetVertexDeclaration( m_pVertexDecl );
    
    PIXBeginNamedEvent( 0, "Render Scene" );

    // Render the meshes
    ATG::NameIndexedCollection::iterator i;
    for( i = m_pCharacterScene->GetInstanceList()->begin(); i != m_pCharacterScene->GetInstanceList()->end(); i++ )
    {
        // Select models from the object list.
        if( ( *i )->IsDerivedFrom( ATG::Model::TypeID ) )
        {
            ATG::Model* pModel = ( ATG::Model* )( *i );

            // Loop over mesh mappings.
            DWORD dwMeshMappingCount = pModel->GetNumMeshMappings();
            for( DWORD dwMapIndex = 0; dwMapIndex < dwMeshMappingCount; ++dwMapIndex )
            {
                ATG::MeshMapping& mm = pModel->GetMeshMapping( dwMapIndex );
                ATG::BaseMesh* pMesh = mm.pMesh;

                // Loop over mesh subsets.
                DWORD dwSubsetCount = pMesh->GetNumSubsets();
                for( DWORD dwSubsetIndex = 0; dwSubsetIndex < dwSubsetCount; ++dwSubsetIndex )
                {
                    ATG::MaterialInstance* pMaterial = mm.Materials[ dwSubsetIndex ];

                    for( DWORD j = 0; j < pMaterial->GetRawParameterCount(); ++j )
                    {
                        // Retrieve diffuse and normal maps and set
                        ATG::MaterialParameter& param = pMaterial->GetRawParameter( j );
                        if( param.pValue != NULL )
                        {
                            ATG::Texture2D* pTex2D = ( ATG::Texture2D* )param.pValue;

                            m_pd3dDevice->SetTexture( j, pTex2D->GetD3DTexture() );
                        }
                    }

                    // Render the mesh subset.
                    pMesh->RenderSubset( dwSubsetIndex, m_pd3dDevice );
                }
            }
        }
    }
    
    // Render the UI at zero pixel disparity.  This will place the UI at screen depth.
    PIXBeginNamedEvent(0, "UI");
    {
        // Add an offset to the UI for stereoscopic views
        FLOAT yOffset = 0.0f;
        if( pRegion )
            yOffset = (FLOAT)pRegion->ViewportYOffset;

        if( m_bSensorConnected )
        {
            m_CameraManager.DisplayPIP( yOffset );
        }
      
        // Render the HUD
        m_Font.Begin();
 
        m_Font.SetScaleFactors( 1.2f, 1.2f );
        m_Font.DrawText( 0.0f, yOffset, 0xffffffff, L"Stereoscopic 3D Head Tracking Sample" );
        m_Font.SetScaleFactors( 1.0f, 1.0f );
        m_Font.DrawText( 0.0f, yOffset, 0xffffff00, m_Timer.GetFrameRate(), ATGFONT_RIGHT );
        
        switch ( m_S3DMode )
        {
        case S3D_ANAGLYPH:
            m_Font.DrawText( 0.0f, 40.0f + yOffset, 0xffffffff, L"Mode: Anaglyph S3D");
            break;
            
        case S3D_FRAME_PACKED:
            m_Font.DrawText( 0.0f, 40.0f + yOffset, 0xffffffff, L"Mode: Frame-packed S3D");
            break;

        case S3D_OFF:
        default:
            m_Font.DrawText( 0.0f, 40.0f + yOffset, 0xffffffff, L"Mode: S3D Off");
            break;
        }

        WCHAR text[256];
        swprintf_s( text, L"Interocular: %1.3f", m_fInterocular );
        m_Font.DrawText( 0.0f, 60.0f+yOffset, 0xffffffff, text );
        
        if( m_S3DMode == S3D_FRAME_PACKED && !m_bFramePackedS3DCapable )
        {
            if( m_bFramePackedS3DCapable )    // Display device supports 720p60Hz S3D
            {
                m_Font.DrawText( 0.0f, 40.0f + yOffset, 0xffffffff, L"Press Y to enable Stereoscopic 3D Mode", ATGFONT_RIGHT );
            }
            else
            {
           
                m_Font.DrawText( 0.0f, 40.0f + yOffset, 0xffff0000, L"Unable to use frame-packed mode:", ATGFONT_RIGHT );
                if( !(m_VideoCaps & XC_VIDEO_STEREOSCOPIC_3D_ENABLED) )
                {
                    m_Font.DrawText( 0.0f, 60.0f + yOffset, 0xffff0000, L"You must enable S3D in the dashboard", ATGFONT_RIGHT);
                }
                if( !(m_VideoCaps & XC_VIDEO_STEREOSCOPIC_3D_720P_60HZ) )
                {
                    m_Font.DrawText( 0.0f, 80.0f + yOffset, 0xffff0000, L"You must have a 720p60Hz S3D capable display", ATGFONT_RIGHT);
                }
            }
        }
        
        m_Font.End();
    }
    PIXEndNamedEvent();

    if( m_bDrawHelp )
    {
        m_Help.Render( &m_Font, g_HelpCallouts, NUM_HELP_CALLOUTS );
    }

    PIXEndNamedEvent();
}

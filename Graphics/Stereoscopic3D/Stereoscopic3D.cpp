//--------------------------------------------------------------------------------------
// Stereoscopic3D.cpp
//
// This sample shows you how to render a Stereoscopic 3D (S3D) scene for output on an HDMI 
// Stereoscopic 3D TV.  Two unique views are rendered (one for each eye) and the 
// result is placed in a frame-packed front buffer for consumption by the display device.
// This sample also shows how to switch between S3D and non-S3D modes while the title 
// is running.
//
// Microsoft Advanced Technology Group
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include <xtl.h>
#include <xgraphics.h>
#include <AtgApp.h>
#include <AtgFont.h>
#include <AtgInput.h>
#include <AtgUtil.h>
#include <AtgHelp.h>
#include <AtgSceneAll.h>

//--------------------------------------------------------------------------------------
// Callouts for labeling the gamepad on the help screen
//--------------------------------------------------------------------------------------
ATG::HELP_CALLOUT g_HelpCallouts[] =
{
    { ATG::HELP_BACK_BUTTON,    ATG::HELP_PLACEMENT_2, L"Display\nhelp"                 },
    { ATG::HELP_Y_BUTTON,       ATG::HELP_PLACEMENT_1, L"Toggle 3D display"             },
    { ATG::HELP_LEFT_TRIGGER,   ATG::HELP_PLACEMENT_1, L"Decrease rotation velocity"    },
    { ATG::HELP_RIGHT_TRIGGER,  ATG::HELP_PLACEMENT_1, L"Increase rotation velocity"    },
    { ATG::HELP_DPAD,           ATG::HELP_PLACEMENT_1, L"Increase/decrease eye separation"},
};

static const DWORD      NUM_HELP_CALLOUTS = sizeof( g_HelpCallouts ) / sizeof( g_HelpCallouts[0] );

static const INT        NUM_SCENES = 2;  // The sample contains two different scenes
const INT NUM_FRONT_BUFFERS = 2;  // Double buffered front buffer to avoid stalls

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
    // Fade out the geometry in the distance. 
    "     float fMaxZValue = 1.2f;                             \n"
    "     if ( In.ObjPos.z < -fMaxZValue )                     \n"
    "       Out.Alpha = 1.0f - ( ( -In.ObjPos.z - fMaxZValue )  / fMaxZValue ); \n"
    "     return Out;                                          \n"
    " }                                                        \n";


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

    XMFLOAT3    m_vPosition;

    // variables for constantly rotating the character scene
    FLOAT       m_fRotAngle;
    FLOAT       m_fRotationVel;
    FLOAT       m_fSeparation;

    // Transform matrices
    XMMATRIX    m_matView;
    XMMATRIX    m_matProj;
    
    LPDIRECT3DVERTEXSHADER9         m_pVS;
    LPDIRECT3DPIXELSHADER9          m_pPS;
    LPDIRECT3DVERTEXDECLARATION9    m_pVertexDecl;

    IDirect3DTexture9* m_pFrontBuffer[NUM_FRONT_BUFFERS];
    INT m_nCurFrontBuffer;

    D3DSurface* m_pSurface;
    D3DSurface* m_pDepthStencilSurface;

    BOOL m_bS3DFramePackedMode;
    DWORD m_VideoCaps;

public:
    // This struct holds information about how to render a frame packed S3D surface
    // it is queried for at initialization time, and then used in later rendering.
    XGSTEREOPARAMETERS m_StereoParams;

private:
    VOID DrawScene( CXMMATRIX matView, CXMMATRIX matProj, XGSTEREOREGION* pRegion );
    VOID Set3DMode( BOOL bFramePacked );
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
VOID __cdecl main()
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

    AtgApp.m_d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_ONE;
    
    // Disable automatic frame buffer creation.  The S3D frame-packed buffer is a special case that
    // we'll need to handle on our own.
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
    m_vPosition = XMFLOAT3( 0.0f, 0.0f, 45.0f ); 
    m_bS3DFramePackedMode = FALSE;
    m_fSeparation = -.4f;

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

    if( FAILED( hr = InitD3D() ) )
    {
        ATG_PrintError( "Couldn't initialize D3D resources\n");
    }

    static const D3DVERTEXELEMENT9 decl[] =
    {
        { 0,  0, D3DDECLTYPE_FLOAT4,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
        { 0, 16, D3DDECLTYPE_FLOAT2,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
        { 0, 24, D3DDECLTYPE_FLOAT3,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_NORMAL,   0 },
        D3DDECL_END()
    };

    m_pd3dDevice->CreateVertexDeclaration( decl, &m_pVertexDecl );

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

    m_pCharacterScene = new ATG::Scene();
    assert( m_pCharacterScene );
    
    hr = ATG::SceneFileParser::LoadXATGFile( "game:\\media\\scenes\\spike.xatg", m_pCharacterScene, NULL,
        ATG::XATGLOADER_DONOTINITIALIZEMATERIALS, NULL );
    if( FAILED( hr ) )
    {
        ATG_PrintError( "Couldn't load Scene\n" );
        return hr;
    }

    // Get the capabilities of the display.  This determines if we can switch to S3D mode.
    m_VideoCaps = XGetVideoCapabilities();

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
    m_fRotAngle += m_fRotationVel * fElapsedTime;

    // Get the current gamepad state
    ATG::GAMEPAD* pGamepad = ATG::Input::GetMergedInput();

    // Toggle help
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_BACK )
        m_bDrawHelp = !m_bDrawHelp;

    // Modify rotation velocity
    FLOAT fRotationAccel = 1.0f;
    if( pGamepad->bLeftTrigger )
    {
        m_fRotationVel -= fRotationAccel * fElapsedTime;
    }
    if( pGamepad->bRightTrigger )
    {
        m_fRotationVel += fRotationAccel * fElapsedTime;
    }

    if( pGamepad->wPressedButtons & ( XINPUT_GAMEPAD_DPAD_LEFT | XINPUT_GAMEPAD_DPAD_DOWN ) )
    {
        m_fSeparation -= .025f;
    }
    if( pGamepad->wPressedButtons & ( XINPUT_GAMEPAD_DPAD_RIGHT | XINPUT_GAMEPAD_DPAD_UP ) )
    {
        m_fSeparation += .025f;
    }

    // Clamp to reasonable values;
    m_fSeparation = min(max(m_fSeparation,-1.0f), .5f);

    // Toggle S3D
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_Y )
    {
        if( (m_VideoCaps & XC_VIDEO_STEREOSCOPIC_3D_ENABLED)  &&    // S3D is enabled in the dashboard (default is enabled)
            (m_VideoCaps & XC_VIDEO_STEREOSCOPIC_3D_720P_60HZ) )    // Display device supports 720p60Hz S3D
        {
            Set3DMode( !m_bS3DFramePackedMode );
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
VOID Sample::Set3DMode( BOOL bS3DFramePackedMode ) 
{
    HRESULT hr;

    m_bS3DFramePackedMode = bS3DFramePackedMode;

    // Switch the stereoscopic flag on or off.  Leave all other flags the same
    if( m_bS3DFramePackedMode )
    {
        // WARNING: If the console is not connected to an HDMI S3D display (via HDMI)
        // and this flag is enabled, D3D will crash during the next swap. 
        m_d3dpp.Flags |= D3DPRESENTFLAG_STEREOSCOPIC_720P_60HZ;
    }
    else
    {
        m_d3dpp.Flags &= ~D3DPRESENTFLAG_STEREOSCOPIC_720P_60HZ;
    }

    
    UINT nWidth, nHeight;
    ATG::GetVideoSettings( &nWidth, &nHeight );
   
    if( m_bS3DFramePackedMode )
    {
        // Get the stereo parameters based on the desired frame buffer size
        XGGetStereoParameters(nWidth, nHeight, D3DMULTISAMPLE_NONE, 0, &m_StereoParams);

        m_d3dpp.BackBufferWidth = m_StereoParams.FrontBufferWidth;
        m_d3dpp.BackBufferHeight = m_StereoParams.FrontBufferHeight;
    }
    else
    {
        m_d3dpp.BackBufferWidth = nWidth;
        m_d3dpp.BackBufferHeight = nHeight;
    }

    m_pd3dDevice->UnsetAll();
    m_pd3dDevice->Reset( &m_d3dpp );

    if( FAILED( hr = InitD3D() ) )
    {
        ATG_PrintError( "Could not initialize D3D resources.\n");
    }

}


//--------------------------------------------------------------------------------------
// Name: InitD3D()
// Desc: Initialize Direct3D
//--------------------------------------------------------------------------------------
HRESULT Sample::InitD3D()
{
    HRESULT hr = S_OK;

    // Create the front buffer
    for( INT i = 0; i < NUM_FRONT_BUFFERS; i++)
    {
        SAFE_RELEASE( m_pFrontBuffer[i] );

        if( FAILED(hr = m_pd3dDevice->CreateTexture( m_d3dpp.BackBufferWidth, m_d3dpp.BackBufferHeight, 1, 0,D3DFMT_LE_X8R8G8B8, 0, &m_pFrontBuffer[i], NULL)))
            return hr;
    }
    m_nCurFrontBuffer = 0;

    SAFE_RELEASE( m_pSurface );
    SAFE_RELEASE( m_pDepthStencilSurface );

    if( m_bS3DFramePackedMode )
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
    }

    return S_OK;
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
    FLOAT fAspectRatio = ( FLOAT )nWidth / ( FLOAT )nHeight;


    FLOAT fFovY = .25f;
    FLOAT fNearZ = .1f;
    FLOAT fFarZ = 100.0f;

    if( m_bS3DFramePackedMode )
    {
        // Clear the blank area of the front buffer.
        //
        // Note: The left and right eye blank regions must be the same color as the invariant blank area.  D3D will
        // display warning debug text in debug and profile builds if this is not the case.  In release and LTCG
        // builds, this check is not performed, but the title should still conform to this rule, or its stereo
        // 3D output may not work with some TVs.
        m_pd3dDevice->Clear( 1, &m_StereoParams.BlankRegion.BlankRectTop, D3DCLEAR_TARGET, 0, 0, 0);

        for (int i = 0; i < NUM_FRONT_BUFFERS; i++)
        {
            m_pd3dDevice->Resolve(D3DRESOLVE_ALLFRAGMENTS, &m_StereoParams.BlankRegion.ResolveSourceRect, 
                       m_pFrontBuffer[i], &m_StereoParams.BlankRegion.ResolveDestPoint, 0, 0,
                       NULL, 0, 0, NULL);
        }

        // Render the eye views.  In this sample, the projections for the eyes are parallel to each other and separated horizontally.
        PIXBeginNamedEvent(0, "Render Left Eye View");
        {

            XMMATRIX leftProj = XMMatrixMultiply( XMMatrixTranslation( -m_fSeparation, 0, 0 ), XMMatrixPerspectiveFovLH( fFovY, fAspectRatio, fNearZ, fFarZ ) );

            DrawScene( m_matView, leftProj, &m_StereoParams.LeftEye );
            D3DVECTOR4 clearColor = {0.0f, 0.0f, 0.0f, 0.0f };
            m_pd3dDevice->Resolve( D3DRESOLVE_RENDERTARGET0 | D3DRESOLVE_CLEARRENDERTARGET | D3DRESOLVE_CLEARDEPTHSTENCIL, &m_StereoParams.LeftEye.ResolveSourceRect, m_pFrontBuffer[m_nCurFrontBuffer], &m_StereoParams.LeftEye.ResolveDestPoint, 0, 0, &clearColor, 1.0f, 0, NULL );
        }
        PIXEndNamedEvent();

        PIXBeginNamedEvent(0, "Render Right Eye View");
        {
            XMMATRIX rightProj = XMMatrixMultiply( XMMatrixTranslation( m_fSeparation, 0, 0 ), XMMatrixPerspectiveFovLH( fFovY, fAspectRatio, fNearZ, fFarZ ) );

            DrawScene( m_matView, rightProj, &m_StereoParams.RightEye );
            m_pd3dDevice->Resolve( D3DRESOLVE_RENDERTARGET0, &m_StereoParams.RightEye.ResolveSourceRect, m_pFrontBuffer[m_nCurFrontBuffer], &m_StereoParams.RightEye.ResolveDestPoint, 0, 0, NULL, 0.0f, 0, NULL );
        }
        PIXEndNamedEvent();

    }
    else
    {
        XMMATRIX matProj = XMMatrixPerspectiveFovLH( fFovY, fAspectRatio, fNearZ, fFarZ );
        DrawScene( m_matView, matProj, NULL );
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
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MAXANISOTROPY, 16 );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_ANISOTROPIC );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_ANISOTROPIC );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetSamplerState( 1, D3DSAMP_MAXANISOTROPY, 16 );
    m_pd3dDevice->SetSamplerState( 1, D3DSAMP_MINFILTER, D3DTEXF_ANISOTROPIC );
    m_pd3dDevice->SetSamplerState( 1, D3DSAMP_MAGFILTER, D3DTEXF_ANISOTROPIC );
    m_pd3dDevice->SetSamplerState( 1, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR );

    // Set shader constants
	FLOAT fScale = matView._43/matProj._11*2.0f; // Scale the scene to the size of the display

    XMMATRIX matWorld = XMMatrixIdentity();

    // Rotate the character scene
    matWorld = XMMatrixRotationAxis( XMVectorSet( 0.0f, 1.0f, 0.0f, 0.0f), m_fRotAngle );
    fScale *= .4f;

    XMMATRIX matWVP = XMMatrixScaling( fScale, fScale, fScale ) * matWorld * matView * matProj;
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
    
    // Render the UI
    PIXBeginNamedEvent(0, "UI");
    {
        m_Font.Begin();
 
        // Add an offset to the text for stereoscopic views
        FLOAT yOffset = 0.0f;
        if( pRegion )
            yOffset = (FLOAT)pRegion->ViewportYOffset;

        m_Font.SetScaleFactors( 1.2f, 1.2f );
        m_Font.DrawText( 0.0f, yOffset, 0xffffffff, L"Stereoscopic 3D Sample" );
        m_Font.SetScaleFactors( 1.0f, 1.0f );
        m_Font.DrawText( 0.0f, yOffset, 0xffffff00, m_Timer.GetFrameRate(), ATGFONT_RIGHT );
        
        if( !m_bS3DFramePackedMode )
        {
            if( (m_VideoCaps & XC_VIDEO_STEREOSCOPIC_3D_ENABLED)  &&    // S3D is enabled in the dashboard (default is enabled)
                (m_VideoCaps & XC_VIDEO_STEREOSCOPIC_3D_720P_60HZ) )    // Display device supports 720p60Hz S3D
            {
                m_Font.DrawText( 0.0f, 40.0f, 0xffffffff, L"Press Y to enable Stereoscopic 3D Mode", ATGFONT_RIGHT );
            }
            else
            {
           
                m_Font.DrawText( 0.0f, 40.0f, 0xffff0000, L"Unable to use S3D mode:", ATGFONT_RIGHT );
                if( !(m_VideoCaps & XC_VIDEO_STEREOSCOPIC_3D_ENABLED) )
                {
                    m_Font.DrawText( 0.0f, 60.0f, 0xffff0000, L"You must enable S3D in the dashboard", ATGFONT_RIGHT);
                }
                if( !(m_VideoCaps & XC_VIDEO_STEREOSCOPIC_3D_720P_60HZ) )
                {
                    m_Font.DrawText( 0.0f, 80.0f, 0xffff0000, L"You must have a 720p60Hz S3D capable display", ATGFONT_RIGHT);
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

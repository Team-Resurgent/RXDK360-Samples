//--------------------------------------------------------------------------------------
// DepthOfField.cpp
//
// Shows off depth-of-field rendering
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include <xtl.h>
#include <xgraphics.h>
#include <xboxmath.h>
#include <AtgApp.h>
#include <AtgFont.h>
#include <AtgHelp.h>
#include <AtgInput.h>
#include <AtgMesh.h>
#include <AtgPostProcess.h>
#include <AtgResource.h>
#include <AtgUtil.h>
#include "ChessSet.h"


//--------------------------------------------------------------------------------------
// Callouts for labelling the gamepad on the help screen
//--------------------------------------------------------------------------------------
ATG::HELP_CALLOUT g_HelpCallouts[] =
{
    { ATG::HELP_A_BUTTON,     ATG::HELP_PLACEMENT_2, L"Toggle\nDOF effect" },
    { ATG::HELP_B_BUTTON,     ATG::HELP_PLACEMENT_2, L"Toggle\nfocus mode" },
    { ATG::HELP_LEFT_SHOULDER, ATG::HELP_PLACEMENT_2, L"Increase\naperature" },
    { ATG::HELP_RIGHT_SHOULDER, ATG::HELP_PLACEMENT_2, L"Decrease\naperature" },
    { ATG::HELP_DPAD,         ATG::HELP_PLACEMENT_2, L"Move focal\nselection" },
    { ATG::HELP_MISC_CALLOUT, ATG::HELP_PLACEMENT_1, L"Use triggers to move focal plane" },
    { ATG::HELP_LEFTSTICK,    ATG::HELP_PLACEMENT_2, L"Move\ncamera" },
    { ATG::HELP_RIGHTSTICK,   ATG::HELP_PLACEMENT_2, L"Rotate\ncamera" },
    { ATG::HELP_START_BUTTON, ATG::HELP_PLACEMENT_2, L"Reset\ncamera" },
    { ATG::HELP_BACK_BUTTON,  ATG::HELP_PLACEMENT_2, L"Display\nhelp" },
};
#define NUM_HELP_CALLOUTS (sizeof(g_HelpCallouts) / sizeof(g_HelpCallouts[0]))


//--------------------------------------------------------------------------------------
// External variables
//--------------------------------------------------------------------------------------
XMMATRIX    g_matView; // Global copy of the view matrix
XMMATRIX    g_matProj; // Global copy of the projection matrix

const DWORD VSCONST_vCameraPt = 1; // Camera position in object space
const DWORD VSCONST_matWorldView = 8; // World-view matrix
const DWORD VSCONST_matWorldViewProj = 12; // World-view-projection matrix

const DWORD PSCONST_vLightDir = 0; // Light direction in object space
const DWORD PSCONST_fReflectivity = 2; // Reflectivity coefficient
const DWORD PSCONST_vConstantColor = 4; // Constant color

const DWORD PSCONST_vPixelSizeLow = 0; // Constants for the DOF pshader
const DWORD PSCONST_vPixelSizeHigh = 1; //
const DWORD PSCONST_Poisson = 2; // The poisson kernel filter taps
const DWORD PSCONST_vMaxCoC = 10; // Maximum circle-of-confusion
const DWORD PSCONST_fRadiusScale = 11; // Radius of blur disc
const DWORD PSCONST_fFocalPlaneDistance = 20; // Focal plane distance
const DWORD PSCONST_fNearBlurPlaneDistance = 21; // Near blur plance distance
const DWORD PSCONST_fFarBlurPlaneDistance = 22; // Far blur plane distance
const DWORD PSCONST_fFarBlurLimit = 23; // Far blur limit [0,1]


//--------------------------------------------------------------------------------------
// Name: class Sample
// Desc: Main class to run this application.
//--------------------------------------------------------------------------------------
class Sample : public ATG::Application
{
    ATG::Timer m_Timer;
    ATG::Font m_Font;
    ATG::PackedResource m_Resource;          // Bundled textures in a packed resource
    ATG::PostProcess m_PostProcess;       // Commonly used effects (blur, etc.)
    ATG::Help m_Help;
    BOOL m_bDrawHelp;

    ChessBoard m_ChessBoard;        // A scene to render

    XMMATRIX m_matLight;          // Properties for the light
    XMVECTOR m_vRotLight;

    XMVECTOR m_vEyePt;            // Camera properties
    XMVECTOR m_vLookatDir;
    XMVECTOR m_vUp;
    FLOAT m_fNearPlane;
    FLOAT m_fFarPlane;

    LPDIRECT3DTEXTURE9 m_pSceneTexture;         // Scaled scene textures for the blur pass
    LPDIRECT3DTEXTURE9 m_pScaledSceneTexture;

    BOOL m_bShowOptionalReflectionEffect;
    BOOL m_bUseDepthOfFieldEffect; // Toggles the DOF effect
    BOOL m_bDebugShowFocalPlanes;  // Whether to show lines for the focal plane
    BOOL m_bUseFixedFocalPoint;    // Whether to use a focal point that's fixed on point in space
    DWORD m_dwGridSelectionX;
    DWORD m_dwGridSelectionZ;

    FLOAT m_fFocalPlaneDistance;   // Depth-of-field effect parameters
    FLOAT m_fNearBlurPlaneDistance;
    FLOAT m_fFarBlurPlaneDistance;
    FLOAT m_fFarBlurLimit;

    LPDIRECT3DVERTEXDECLARATION9 m_pConstantColorVtxDecl; // Vertex declarations
    LPDIRECT3DVERTEXSHADER9 m_pDOFVertexShader; // Custom vertex shaders
    LPDIRECT3DVERTEXSHADER9 m_pConstantColorVS;
    LPDIRECT3DPIXELSHADER9 m_pDOFPixelShader;  // Custom pixel shaders
    LPDIRECT3DPIXELSHADER9 m_pConstantColorPS;
    LPDIRECT3DPIXELSHADER9 m_pPoissonDOFFilterPS;
    LPDIRECT3DPIXELSHADER9 m_pReflectiveSurfacePS;

    HRESULT RenderScene();
    HRESULT RenderDOFEffect();

private:
    HRESULT Initialize();
    HRESULT Update();
    HRESULT Render();
};


//--------------------------------------------------------------------------------------
// Name: main()
// Desc: Entry point to the program
//--------------------------------------------------------------------------------------
INT __cdecl main()
{
    Sample atgApp;
    ATG::GetVideoSettings( &atgApp.m_d3dpp.BackBufferWidth, &atgApp.m_d3dpp.BackBufferHeight );

    // Make sure display is gamma correct.
    atgApp.m_d3dpp.BackBufferFormat =  ( D3DFORMAT )MAKESRGBFMT( D3DFMT_A8R8G8B8 );
    atgApp.m_d3dpp.FrontBufferFormat = ( D3DFORMAT )MAKESRGBFMT( D3DFMT_LE_X8R8G8B8 );

    atgApp.Run();
}


//--------------------------------------------------------------------------------------
// Name: Initialize()
// Desc: Initializes the app
//--------------------------------------------------------------------------------------
HRESULT Sample::Initialize()
{
    HRESULT hr;

    // Initialize base member variables
    m_matLight = XMMatrixIdentity();
    m_bDrawHelp = FALSE;

    m_bShowOptionalReflectionEffect = TRUE;
    m_bUseDepthOfFieldEffect = TRUE;
    m_bDebugShowFocalPlanes = TRUE;
    m_bUseFixedFocalPoint = FALSE;
    m_dwGridSelectionX = 0;
    m_dwGridSelectionZ = 0;

    m_fFocalPlaneDistance = 7.0f;
    m_fNearBlurPlaneDistance = 1.0f;  // 10.0f
    m_fFarBlurPlaneDistance = 20.0f;  // 100.0f
    m_fFarBlurLimit = 0.95f;

    // Create the font
    if( FAILED( m_Font.Create( "game:\\Media\\Fonts\\Arial_16.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Confine text drawing to the title safe area
    m_Font.SetWindow( ATG::GetTitleSafeArea() );

    // Create the help
    if( FAILED( m_Help.Create( "game:\\Media\\Help\\Help.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Create the textures resource
    if( FAILED( hr = m_Resource.Create( "game:\\Media\\Resource.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Initialize the post-processing effects library (for blur, bloom, etc.)
    if( FAILED( m_PostProcess.Initialize() ) )
    {
        ATG_PrintError( "DX9Sample: Couldn't initialize the effects library\n" );
        return E_FAIL;
    }
    // Create the chess board
    if( FAILED( m_ChessBoard.Create( &m_Resource ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Build the instanced set of chess pieces
    m_ChessBoard.AddChessPiece( ChessPiece::BLACK, ChessPiece::PAWN, 1, 7 );
    m_ChessBoard.AddChessPiece( ChessPiece::BLACK, ChessPiece::PAWN, 2, 7 );
    m_ChessBoard.AddChessPiece( ChessPiece::BLACK, ChessPiece::PAWN, 4, 4 );
    m_ChessBoard.AddChessPiece( ChessPiece::BLACK, ChessPiece::PAWN, 7, 5 );
    m_ChessBoard.AddChessPiece( ChessPiece::BLACK, ChessPiece::PAWN, 8, 5 );
    m_ChessBoard.AddChessPiece( ChessPiece::BLACK, ChessPiece::QUEEN, 3, 1 );
    m_ChessBoard.AddChessPiece( ChessPiece::BLACK, ChessPiece::KING, 8, 6 );
    m_ChessBoard.AddChessPiece( ChessPiece::WHITE, ChessPiece::PAWN, 6, 2 );
    m_ChessBoard.AddChessPiece( ChessPiece::WHITE, ChessPiece::PAWN, 7, 3 );
    m_ChessBoard.AddChessPiece( ChessPiece::WHITE, ChessPiece::PAWN, 8, 2 );
    m_ChessBoard.AddChessPiece( ChessPiece::WHITE, ChessPiece::BISHOP, 4, 5 );
    m_ChessBoard.AddChessPiece( ChessPiece::WHITE, ChessPiece::QUEEN, 6, 7 );
    m_ChessBoard.AddChessPiece( ChessPiece::WHITE, ChessPiece::KING, 7, 2 );

    // Set the view matrix
    XMVECTOR vCenter = m_ChessBoard.GetCenter();
    FLOAT fGridSize = m_ChessBoard.GetGridSize();
    m_vEyePt = vCenter + XMVectorSet( 0 * fGridSize, 5 * fGridSize, -10 * fGridSize, 1.0f );
    m_vLookatDir = XMVectorSet( 1.0f, -0.5f, 1.0f, 1.0f );
    m_vUp = XMVectorSet( 0.0f, 1.0f, 0.0f, 1.0f );
    g_matView = XMMatrixLookAtLH( m_vEyePt, m_vEyePt + m_vLookatDir, m_vUp );

    // Determine the aspect ratio
    FLOAT fAspectRatio = ( FLOAT )m_d3dpp.BackBufferWidth / ( FLOAT )m_d3dpp.BackBufferHeight;

    // Set the projection matrix
    m_fNearPlane = 0.1f * 1.0f * fGridSize;
    m_fFarPlane = 2.5f * 8.0f * fGridSize;
    g_matProj = XMMatrixPerspectiveFovLH( XM_PI / 4, fAspectRatio, m_fNearPlane, m_fFarPlane );

    static const D3DVERTEXELEMENT9 declConstantColor[] =
    {
        { 0, 0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
        D3DDECL_END()
    };

    m_pd3dDevice->CreateVertexDeclaration( declConstantColor, &m_pConstantColorVtxDecl );

    if( FAILED( hr = ATG::LoadVertexShader( "game:\\Media\\Shaders\\ShadeDOFVertex.xvu", &m_pDOFVertexShader ) ) )
        return hr;

    if( FAILED( hr = ATG::LoadPixelShader( "game:\\Media\\Shaders\\ShadeDOFPixel.xpu", &m_pDOFPixelShader ) ) )
        return hr;

    if( FAILED( hr = ATG::LoadVertexShader( "game:\\Media\\Shaders\\ConstantColor.xvu", &m_pConstantColorVS ) ) )
        return hr;

    if( FAILED( hr = ATG::LoadPixelShader( "game:\\Media\\Shaders\\ConstantColor.xpu", &m_pConstantColorPS ) ) )
        return hr;

    if( FAILED( hr = ATG::LoadPixelShader( "game:\\Media\\Shaders\\PoissonDOFFilter.xpu", &m_pPoissonDOFFilterPS ) ) )
        return hr;

    if( FAILED( hr = ATG::LoadPixelShader( "game:\\Media\\Shaders\\ReflectiveSurface.xpu",
                                           &m_pReflectiveSurfacePS ) ) )
        return hr;

    // Create textures for rendering a 1/16th sized version of the scene
    m_pd3dDevice->CreateTexture( m_d3dpp.BackBufferWidth, m_d3dpp.BackBufferHeight,
                                 1, D3DUSAGE_RENDERTARGET, ( D3DFORMAT )MAKESRGBFMT( D3DFMT_A8R8G8B8 ),
                                 D3DPOOL_DEFAULT, &m_pSceneTexture, NULL );

    m_pd3dDevice->CreateTexture( m_d3dpp.BackBufferWidth / 4, m_d3dpp.BackBufferHeight / 4,
                                 1, D3DUSAGE_RENDERTARGET, ( D3DFORMAT )MAKESRGBFMT( D3DFMT_A8R8G8B8 ),
                                 D3DPOOL_DEFAULT, &m_pScaledSceneTexture, NULL );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Update()
// Desc: Called once per frame to update the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Update()
{
    FLOAT m_fElapsedTime = ( FLOAT )m_Timer.GetElapsedTime();

    // Get the current gamepad status
    ATG::GAMEPAD* pGamepad = ATG::Input::GetMergedInput();

    // Toggle help
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_BACK )
        m_bDrawHelp = !m_bDrawHelp;

    // Set the view matrix
    static FLOAT fTheta = -0.2f * XM_PI;
    static FLOAT fPhi = +0.0f * XM_PI;

    fPhi += pGamepad->fX2 * m_fElapsedTime * 0.3f * XM_PI;
    fTheta += pGamepad->fY2 * m_fElapsedTime * 0.3f * XM_PI;

    m_vLookatDir.x = cosf( fTheta ) * sinf( fPhi );
    m_vLookatDir.y = sinf( fTheta );
    m_vLookatDir.z = cosf( fTheta ) * cosf( fPhi );

    XMVECTOR vCrossDir;
    vCrossDir.x = +cosf( fPhi );
    vCrossDir.y = 0.0;
    vCrossDir.z = -sinf( fPhi );

    // Chessboard properties
    XMVECTOR vCenter = m_ChessBoard.GetCenter();
    FLOAT fGridSize = m_ChessBoard.GetGridSize();

    m_vEyePt += m_vLookatDir * pGamepad->fY1 * m_fElapsedTime * 4 * fGridSize;
    m_vEyePt += vCrossDir * pGamepad->fX1 * m_fElapsedTime * 4 * fGridSize;
    g_matView = XMMatrixLookAtLH( m_vEyePt, m_vEyePt + m_vLookatDir, m_vUp );

    // Set light position for the vertex shader
    XMVECTOR vDeterminant;
    static XMVECTOR vRotLight = XMVectorSet( 0.0f, 1.0f, 0.0f, 1.0f );
    XMMATRIX matLightInv = XMMatrixInverse( &vDeterminant, m_matLight );
    XMMATRIX matWorldInv = XMMatrixInverse( &vDeterminant, XMMatrixIdentity() );
    vRotLight = XMVector3TransformCoord( vRotLight, m_matLight );
    m_vRotLight = XMVector3Normalize( XMVector3TransformCoord( vRotLight, matWorldInv ) );

    static FLOAT fDOF = 0.35f;
    static FLOAT fAperature = 0.2f;

    // Reset the camera
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_START )
    {
        m_vEyePt = vCenter + XMVectorSet( 0 * fGridSize, 5 * fGridSize, -10 * fGridSize, 1.0f );
        fTheta = -0.2f * XM_PI;
        fPhi = +0.0f * XM_PI;
        fDOF = 0.35f;
        fAperature = 0.2f;
    }

    // Use controls to modify the focal plane and aperature
    fDOF -= m_fElapsedTime * pGamepad->bLeftTrigger / 255.0f;
    fDOF += m_fElapsedTime * pGamepad->bRightTrigger / 255.0f;
    fDOF = min( 1.0f, max( 0.0f, fDOF ) );

    fAperature -= 0.2f * m_fElapsedTime * ( pGamepad->wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER ? 1.0f : 0.0f );
    fAperature += 0.2f * m_fElapsedTime * ( pGamepad->wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER ? 1.0f : 0.0f );
    fAperature = min( 1.0f, max( 0.0f, fAperature ) );

    m_fFocalPlaneDistance = m_fNearPlane + fDOF * ( m_fFarPlane - m_fNearPlane );
    m_fNearBlurPlaneDistance = m_fNearPlane + ( fDOF - 0.2f * fAperature ) * ( m_fFarPlane - m_fNearPlane );
    m_fFarBlurPlaneDistance = m_fNearPlane + ( fDOF + 0.8f * fAperature ) * ( m_fFarPlane - m_fNearPlane );
    m_fFarBlurLimit = 0.95;;

    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_A )
    {
        m_bUseDepthOfFieldEffect = !m_bUseDepthOfFieldEffect;
    }

    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_B )
    {
        m_bDebugShowFocalPlanes = !m_bDebugShowFocalPlanes;
        m_bUseFixedFocalPoint = !m_bUseFixedFocalPoint;
    }

    if( m_bUseFixedFocalPoint )
    {
        // Move the selected focal point
        if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_DPAD_LEFT )
            m_dwGridSelectionX = ( m_dwGridSelectionX > 0 ) ? m_dwGridSelectionX - 1 : m_dwGridSelectionX;
        if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_DPAD_RIGHT )
            m_dwGridSelectionX = ( m_dwGridSelectionX < 7 ) ? m_dwGridSelectionX + 1 : m_dwGridSelectionX;
        if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_DPAD_DOWN )
            m_dwGridSelectionZ = ( m_dwGridSelectionZ > 0 ) ? m_dwGridSelectionZ - 1 : m_dwGridSelectionZ;
        if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_DPAD_UP )
            m_dwGridSelectionZ = ( m_dwGridSelectionZ < 7 ) ? m_dwGridSelectionZ + 1 : m_dwGridSelectionZ;

        XMVECTOR vSquarePos = m_ChessBoard.GetOrigin();
        vSquarePos.x += fGridSize * m_dwGridSelectionX;
        vSquarePos.y += fGridSize * 1.0f;
        vSquarePos.z += fGridSize * m_dwGridSelectionZ;

        // Adjust focal planes based on the new focal point
        FLOAT fDOFnew = ( XMVector3Length( m_vEyePt - vSquarePos ).x - m_fNearPlane ) / ( m_fFarPlane - m_fNearPlane );
        m_fFocalPlaneDistance = m_fNearPlane + ( fDOFnew )*( m_fFarPlane - m_fNearPlane );
        m_fNearBlurPlaneDistance = m_fNearPlane + ( fDOFnew - fAperature ) * ( m_fFarPlane - m_fNearPlane );
        m_fFarBlurPlaneDistance = m_fNearPlane + ( fDOFnew + fAperature ) * ( m_fFarPlane - m_fNearPlane );
        m_fFarBlurLimit = 0.95f;
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: RenderScene()
// Desc: Renders the scene, using special shaders to output a depth-blur value in the
//       alpha channel
//--------------------------------------------------------------------------------------
HRESULT Sample::RenderScene()
{
    // Draw a gradient filled background
    ATG::RenderBackground( 0x000000ff, 0x0000ffff );

    // Set default states
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_ALPHATESTENABLE, FALSE );

    m_pd3dDevice->SetTexture( 0, NULL );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP );

    // For an optional effect (not related to the DOF effect), render the chessboard's
    // surface and store the reflectivity in the alpha channel
    if( m_bShowOptionalReflectionEffect )
    {
        // Set the shaders to store a per-pixel fresnel reflection value.
        // Note that the shaders require additional constants that are set in the mesh
        // callback function
        static FLOAT fReflectivity = 0.2f;
        m_pd3dDevice->SetVertexShader( m_pDOFVertexShader );
        m_pd3dDevice->SetPixelShader( m_pReflectiveSurfacePS );
        m_pd3dDevice->SetPixelShaderConstantF( PSCONST_vLightDir, ( FLOAT* )&m_vRotLight, 1 );
        m_pd3dDevice->SetPixelShaderConstantF( PSCONST_fReflectivity, ( FLOAT* )&fReflectivity, 1 );

        // Render the chessboard
        m_ChessBoard.Render( XMMatrixIdentity() );

        // Clear the z-buffer after this pass, so that reflected pieces pass the z-test
        m_pd3dDevice->Clear( 0L, NULL, D3DCLEAR_ZBUFFER, 0L, 1.0f, 0L );
    }

    // Render the chess pieces
    {
        // Set the shaders to render the meshes AND store a depth-blur value in the
        // alpha channel.
        // Note that the shaders require additional constants that are set in the mesh
        // callback function
        m_pd3dDevice->SetVertexShader( m_pDOFVertexShader );
        m_pd3dDevice->SetPixelShader( m_pDOFPixelShader );
        m_pd3dDevice->SetPixelShaderConstantF( PSCONST_vLightDir, ( FLOAT* )&m_vRotLight, 1 );

        // Render the chessboard pieces
        m_ChessBoard.RenderPieces( XMMatrixIdentity() );
    }

    // Optionally, for debugging, render a highlight under the selected sqaure when
    // using a fixed focal point.
    if( m_bUseFixedFocalPoint )
    {
        FLOAT fGridSize = m_ChessBoard.GetGridSize();
        XMVECTOR vSquarePos = m_ChessBoard.GetOrigin();
        vSquarePos.x += m_dwGridSelectionX * fGridSize;
        vSquarePos.y += 0.0f;
        vSquarePos.z += m_dwGridSelectionZ * fGridSize;

        struct VERTEX
        {
            FLOAT x, y, z;
        } v[4] =
        {
            { vSquarePos.x - fGridSize / 2, vSquarePos.y, vSquarePos.z + fGridSize / 2, },
            { vSquarePos.x + fGridSize / 2, vSquarePos.y, vSquarePos.z + fGridSize / 2, },
            { vSquarePos.x + fGridSize / 2, vSquarePos.y, vSquarePos.z - fGridSize / 2, },
            { vSquarePos.x - fGridSize / 2, vSquarePos.y, vSquarePos.z - fGridSize / 2, },
        };

        // Set the vertex shader
        XMMATRIX matWorldViewProj = XMMatrixMultiply( g_matView, g_matProj );
        XMMATRIX matWorldViewProjT = XMMatrixTranspose( matWorldViewProj );
        m_pd3dDevice->SetVertexShader( m_pConstantColorVS );
        m_pd3dDevice->SetVertexShaderConstantF( VSCONST_matWorldViewProj, ( FLOAT* )&matWorldViewProjT, 4 );

        // Set the pixel shader
        XMVECTOR vConstantColor = XMVectorSet( 1, 0, 0, 1 );
        m_pd3dDevice->SetPixelShader( m_pConstantColorPS );
        m_pd3dDevice->SetPixelShaderConstantF( PSCONST_vConstantColor, ( FLOAT* )&vConstantColor, 1 );

        m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, TRUE );
        m_pd3dDevice->SetRenderState( D3DRS_SRCBLEND, D3DBLEND_ONE );
        m_pd3dDevice->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_INVDESTCOLOR );

        m_pd3dDevice->SetVertexDeclaration( m_pConstantColorVtxDecl );
        m_pd3dDevice->DrawPrimitiveUP( D3DPT_QUADLIST, 1, v, sizeof( v[0] ) );

        // Restore state
        m_pd3dDevice->SetVertexShader( m_pDOFVertexShader );
        m_pd3dDevice->SetVertexShader( m_pDOFVertexShader );
        m_pd3dDevice->SetPixelShader( m_pDOFPixelShader );
        m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );
    }

    // For the optional reflection effect, render the reflected pieces using the
    // fresnel reflection value in the alpha channel from above.
    if( m_bShowOptionalReflectionEffect )
    {
        // Set properties for drawing inverted meshes
        static XMMATRIX matInvert = XMMatrixScaling( 1, -1, 1 );
        m_pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_CW );

        // Do a Z-prepass so that all pieces render correctly in the alpha pass
        m_pd3dDevice->SetRenderState( D3DRS_COLORWRITEENABLE, 0L );
        m_ChessBoard.RenderPieces( matInvert );

        // Render the pieces using the destination alpha for transparency
        m_pd3dDevice->SetRenderState( D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_ALL );
        m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, TRUE );
        m_pd3dDevice->SetRenderState( D3DRS_SRCBLEND, D3DBLEND_DESTALPHA );
        m_pd3dDevice->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_INVDESTALPHA );
        m_ChessBoard.RenderPieces( matInvert );

        // Restore state
        m_pd3dDevice->SetRenderState( D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_ALL );
        m_pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_CCW );
        m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );
    }

    // Rewrite the chess board's depth values to the alpha channel. We need to do this
    // here because we used the alpha channel for something else above.
    // Note that the reason we did things this way was to show how you can still use the
    // alpha channel for other effects, and still get the depth blur effect at the end.
    {
        m_pd3dDevice->SetRenderState( D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_ALPHA );
        m_ChessBoard.Render( XMMatrixIdentity() );
        m_pd3dDevice->SetRenderState( D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_ALL );
    }

    // Optionally, draw the focal plane and aperature
    if( m_bDebugShowFocalPlanes )
    {
        // Set the vertex shader
        XMMATRIX matWorldViewProj = XMMatrixMultiply( XMMatrixIdentity(), g_matProj );
        XMMATRIX matWorldViewProjT = XMMatrixTranspose( matWorldViewProj );
        m_pd3dDevice->SetVertexShader( m_pConstantColorVS );
        m_pd3dDevice->SetVertexShaderConstantF( VSCONST_matWorldViewProj, ( FLOAT* )&matWorldViewProjT, 4 );

        // Set the pixel shader
        XMVECTOR vConstantColor = XMVectorSet( 0, 0, 0, 1 );
        m_pd3dDevice->SetPixelShader( m_pConstantColorPS );
        m_pd3dDevice->SetPixelShaderConstantF( PSCONST_vConstantColor, ( FLOAT* )&vConstantColor, 1 );

        struct VERTEX
        {
            FLOAT x, y, z;
        } v[] =
        {
            {   0,      100, m_fFocalPlaneDistance    },
            {   0,     -100, m_fFocalPlaneDistance    },
            { 100,        0, m_fFocalPlaneDistance    },
            {-100,        0, m_fFocalPlaneDistance    },
            {   0.05f,    0, m_fNearBlurPlaneDistance },
            {   0.05f, -100, m_fNearBlurPlaneDistance },
            {   0.05f,  100, m_fFarBlurPlaneDistance  },
            {   0.05f,    0, m_fFarBlurPlaneDistance  },
            {  -0.05f,    0, m_fNearBlurPlaneDistance },
            {  -0.05f, -100, m_fNearBlurPlaneDistance },
            {  -0.05f,  100, m_fFarBlurPlaneDistance  },
            {  -0.05f,    0, m_fFarBlurPlaneDistance  },
        };

        m_pd3dDevice->SetVertexDeclaration( m_pConstantColorVtxDecl );
        m_pd3dDevice->DrawPrimitiveUP( D3DPT_LINELIST, sizeof( v ) / ( 2 * sizeof( v[0] ) ), v, sizeof( v[0] ) );
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: RenderDOFEffect()
// Desc: Renders the depth-of-field effect
//--------------------------------------------------------------------------------------
HRESULT Sample::RenderDOFEffect()
{
    FLOAT vPixelSizeLow[4] =
    {
        0.25f / m_d3dpp.BackBufferWidth, 0.25f / m_d3dpp.BackBufferHeight, 0.0f, 0.0f
    };
    FLOAT vPixelSizeHigh[4] =
    {
        1.00f / m_d3dpp.BackBufferWidth, 1.00f / m_d3dpp.BackBufferHeight, 0.0f, 0.0f
    };
    m_pd3dDevice->SetPixelShaderConstantF( PSCONST_vPixelSizeLow, ( FLOAT* )vPixelSizeLow, 1 );
    m_pd3dDevice->SetPixelShaderConstantF( PSCONST_vPixelSizeHigh, ( FLOAT* )vPixelSizeHigh, 1 );

    static FLOAT g_Poisson[][4] =
    {
        { 0.000000f, 0.000000f },
        { 0.527837f,-0.085868f },
        {-0.040088f, 0.536087f },
        {-0.670445f,-0.179949f },
        {-0.419418f,-0.616039f },
        { 0.440453f,-0.639399f },
        {-0.757088f, 0.349334f },
        { 0.574619f, 0.685879f },
    };

    static FLOAT g_vMaxCoC[4] = { 5.0f, 10.0f };
    static FLOAT g_fRadiusScale[4] = { 0.4f };

    m_pd3dDevice->SetPixelShaderConstantF( PSCONST_Poisson, ( FLOAT* )g_Poisson, 8 );
    m_pd3dDevice->SetPixelShaderConstantF( PSCONST_vMaxCoC, ( FLOAT* )g_vMaxCoC, 1 );
    m_pd3dDevice->SetPixelShaderConstantF( PSCONST_fRadiusScale, ( FLOAT* )g_fRadiusScale, 1 );

    m_pd3dDevice->SetPixelShader( m_pPoissonDOFFilterPS );
    m_pd3dDevice->SetTexture( 0, m_pSceneTexture );        // Original texture
    m_pd3dDevice->SetTexture( 1, m_pScaledSceneTexture );  // Blurred texture
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_POINT );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_POINT );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP );
    m_pd3dDevice->SetSamplerState( 1, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetSamplerState( 1, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetSamplerState( 1, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP );
    m_pd3dDevice->SetSamplerState( 1, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP );

    // The pixel shader we're using, at the time of this writing, uses 19
    // general-purpose-registers when compiled with the current HLSL compiler.
    // By default D3D assigns 64 sets of GPRs to the pixel shader and 64
    // to the vertex shader.  The number of pixel shader threads is then
    // floor(64/19)*3 = 9.  Remember that the GPU's threading is what
    // hides memory latency for fetches, and 9 threads isn't enough to hide 
    // all of the latency, so for a good portion of time the pixel shader 
    // would be stalled waiting for fetches. When the number of threads is
    // lower than 12 then other internal latencies also can't be hidden, so
    // 9 threads will waste performance.
    //
    // The SetShaderGPRAllocation API can be used to change the default GPR 
    // allocations.  It's expensive in that the GPU has to idle all of its
    // pipes and do 24 dummy DrawPrims before it can make the change.  But 
    // on this draw call it is well worth doing because the savings are much
    // greater than the cost, and it makes this expensive draw call run about
    // 20% faster.
    //
    // We have to leave enough vertex shader GPRs to let the current vertex 
    // shader run (i.e. as many as the vertex shader uses), or 16, whichever
    // is more.  We can move all of the rest of the GPRs to the pixel shader.
    // This will give us floor(112/19)*3 = 15 threads.
    m_pd3dDevice->SetShaderGPRAllocation( 0, 16, GPU_GPRS - 16 );

    // Draw the fullscreen quad
    D3DVIEWPORT9 vp;
    m_pd3dDevice->GetViewport( &vp );
    m_PostProcess.DrawScreenSpaceQuad( ( FLOAT )vp.Width, ( FLOAT )vp.Height, 1.0f, 1.0f );

    m_pd3dDevice->SetPixelShader( NULL );

    // Restore the GPR allocations to the defaults. This must be done before
    // calling Present or Swap.
    m_pd3dDevice->SetShaderGPRAllocation( 0, 0, 0 );
    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Render()
// Desc: Called once per frame to render the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Render()
{
    // First, render the scene using the special shaders that output a per-pixel
    // depth blur value in the alpha channel
    {
        m_pd3dDevice->SetPixelShaderConstantF( PSCONST_fFocalPlaneDistance, &m_fFocalPlaneDistance, 1 );
        m_pd3dDevice->SetPixelShaderConstantF( PSCONST_fNearBlurPlaneDistance, &m_fNearBlurPlaneDistance, 1 );
        m_pd3dDevice->SetPixelShaderConstantF( PSCONST_fFarBlurPlaneDistance, &m_fFarBlurPlaneDistance, 1 );
        m_pd3dDevice->SetPixelShaderConstantF( PSCONST_fFarBlurLimit, &m_fFarBlurLimit, 1 );

        RenderScene();
    }

    // Then, optionally apply the DOF effect as a post-processing pass
    if( m_bUseDepthOfFieldEffect )
    {
        // Resolve the current scene from the EDRAM
        m_pd3dDevice->Resolve( D3DRESOLVE_RENDERTARGET0, NULL, m_pSceneTexture, NULL,
                               0, 0, NULL, 0.0f, 0, NULL );

        // Create a 1/16th downsampled version of the scene, using a 4x4 box filter.
        m_PostProcess.Downsample4x4Texture( m_pSceneTexture, m_pScaledSceneTexture );

        // Blur the scene texture using a 5x5 guassian blur
        m_PostProcess.GaussBlur5x5Texture( m_pScaledSceneTexture, m_pScaledSceneTexture );

        // Render the Depth-of-field effect
        RenderDOFEffect();
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
        m_Font.DrawText( 0, 0, 0xffffffff, L"DepthOfField" );
        m_Font.SetScaleFactors( 1.0f, 1.0f );
        m_Font.DrawText( 0, 0, 0xffffff00, m_Timer.GetFrameRate(), ATGFONT_RIGHT );
        m_Font.End();
    }

    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}


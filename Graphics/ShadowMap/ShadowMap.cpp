//--------------------------------------------------------------------------------------
// ShadowMap.cpp
//
// Shadow mapping sample using mip-mapped shadow maps and bilinear PCF sampling.
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include <xtl.h>
#include <d3d9.h>
#include <xgraphics.h>
#include "AtgApp.h"
#include "AtgFont.h"
#include "AtgHelp.h"
#include "AtgInput.h"
#include "AtgMesh.h"
#include "AtgResource.h"
#include "AtgUtil.h"

#include "ChessSet.h"


//--------------------------------------------------------------------------------------
// Callouts for labelling the gamepad on the help screen
//--------------------------------------------------------------------------------------
ATG::HELP_CALLOUT g_HelpCallouts[] =
 {
 { ATG::HELP_LEFT_STICK,   ATG::HELP_PLACEMENT_1, L"Move camera" },
 { ATG::HELP_BOTTOM_LEFT,  ATG::HELP_PLACEMENT_1, L"Use " GLYPH_LEFT_BUTTON GLYPH_RIGHT_BUTTON L" triggers to zoom in/out" },
 { ATG::HELP_RIGHTSTICK,   ATG::HELP_PLACEMENT_1, L"Move light" },
 { ATG::HELP_A_BUTTON,     ATG::HELP_PLACEMENT_2, L"Change Sampling\nMethod" },
 { ATG::HELP_BACK_BUTTON,  ATG::HELP_PLACEMENT_2, L"Display\nhelp" },
};
static const DWORD  NUM_HELP_CALLOUTS = sizeof( g_HelpCallouts ) / sizeof( g_HelpCallouts[0] );


//--------------------------------------------------------------------------------------
// Defines for the shadow map and the shaders.
//--------------------------------------------------------------------------------------
#define SHADOW_MAP_SIZE     1024
#define SHADOW_MAP_LEVELS   8


//-------------------------------------------------------------------------------------
// Structure to hold simple vertex data.
//-------------------------------------------------------------------------------------
struct SIMPLEVERTEX
{
    float   Position[3];
    float   TexCoord[2];
};


XMMATRIX            g_matViewProj;         // Global view*projection matrix
XMMATRIX            g_matShadowViewProj;   // Global shadow view*projection matrix
XMVECTOR            g_vLightDirection;     // Glogal light direction
XMVECTOR            g_vViewPosition;       // Global viewer position


//--------------------------------------------------------------------------------------
// Name: class CAtgSample
// Desc: Main class to run this application.
//--------------------------------------------------------------------------------------
class Sample : public ATG::Application
{
    ATG::Timer m_Timer;
    ATG::Font m_Font;
    ATG::Help m_Help;
    BOOL m_bDrawHelp;
    ATG::PackedResource m_xprResource;              // Packed resources (textures)

    ChessBoard m_ChessBoard;               // A scene to render

    XMMATRIX m_matView;                  // View matrix
    XMMATRIX m_matProj;                  // Projection matrix

    XMVECTOR m_vEyePt;                   // View parameters
    XMVECTOR m_vLookAtPt;
    XMVECTOR m_vUpVec;

    XMVECTOR m_vLightDirection;          // Light direction vector

    INT m_SamplingMethod;           // Sampling method to use

    IDirect3DTexture9* m_pDepthTexture;            // Depth map
    IDirect3DSurface9* m_pDepthStencilSurface;     // Depth-stencil surface for the shadow

    IDirect3DVertexDeclaration9* m_pSimpleVertexDecl;

    IDirect3DVertexShader9* m_pWriteDepthVS;

    IDirect3DTexture9* m_pRotationMap;
    IDirect3DArrayTexture9* m_pOffsetMap;

    IDirect3DVertexShader9* m_pDownsampleDepthVS;
    IDirect3DPixelShader9* m_pDownsampleDepthPS;

    IDirect3DVertexShader9* m_pLightWithShadowsVS;
    IDirect3DPixelShader9* m_pLightWithShadowsPointPS;
    IDirect3DPixelShader9* m_pLightWithShadowsBilinearPS;
    IDirect3DPixelShader9* m_pLightWithShadowsRotatedPoissonPS;
    IDirect3DPixelShader9* m_pLightWithShadowsStratifiedPS;

private:
    void            BuildShadowMapMipMaps( IDirect3DTexture9* pShadowMap );

    HRESULT         CreateRotationMap( DWORD dwSampleGridSize );

    HRESULT         CreateDepthTexture();

    virtual HRESULT Initialize();
    virtual HRESULT Update();
    virtual HRESULT Render();
};


//--------------------------------------------------------------------------------------
// Name: main()
// Desc: Entry point to the program
//--------------------------------------------------------------------------------------
VOID __cdecl main()
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
// Desc: This creates all device-dependent display objects.
//--------------------------------------------------------------------------------------
HRESULT Sample::Initialize()
{
    HRESULT hr;

    m_bDrawHelp = FALSE;
    m_SamplingMethod = 1;

    // Set the view parameters
    m_vEyePt = XMVectorSet( 0.0f, 0.280240387f, -0.558691025f, 1.0f );
    m_vLookAtPt = XMVectorSet( 0.0f, 0.0f, 0.0f, 1.0f );
    m_vUpVec = XMVectorSet( 0.0f, 1.0f, 0.0f, 0.0f );

    // Set the light direction
    m_vLightDirection = XMVectorSet( 0.604104698f, 0.531634569f, -0.585469127f, 0.0f );

    // Create the font
    if( FAILED( m_Font.Create( "game:\\Media\\Fonts\\Arial_16.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Confine text drawing to the title safe area
    m_Font.SetWindow( ATG::GetTitleSafeArea() );

    // Create the help
    if( FAILED( m_Help.Create( "game:\\Media\\Help\\Help.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Create the resources
    if( FAILED( m_xprResource.Create( "game:\\Media\\Resource.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Create the chess board
    if( FAILED( m_ChessBoard.Create( &m_xprResource ) ) )
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

    // Load the vertex and pixel shaders.
    if( FAILED( ATG::LoadVertexShader( "game:\\media\\shaders\\WriteDepthVS.xvu",
                                       &m_pWriteDepthVS ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    if( FAILED( ATG::LoadVertexShader( "game:\\media\\shaders\\DownsampleDepthVS.xvu",
                                       &m_pDownsampleDepthVS ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    if( FAILED( ATG::LoadPixelShader( "game:\\media\\shaders\\DownsampleDepthPS.xpu",
                                      &m_pDownsampleDepthPS ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    if( FAILED( ATG::LoadVertexShader( "game:\\media\\shaders\\LightWithShadowsVS.xvu",
                                       &m_pLightWithShadowsVS ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    if( FAILED( ATG::LoadPixelShader( "game:\\media\\shaders\\LightWithShadowsPointPS.xpu",
                                      &m_pLightWithShadowsPointPS ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    if( FAILED( ATG::LoadPixelShader( "game:\\media\\shaders\\LightWithShadowsBilinearPS.xpu",
                                      &m_pLightWithShadowsBilinearPS ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    if( FAILED( ATG::LoadPixelShader( "game:\\media\\shaders\\LightWithShadowsRotatedPoissonPS.xpu",
                                      &m_pLightWithShadowsRotatedPoissonPS ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    if( FAILED( ATG::LoadPixelShader( "game:\\media\\shaders\\LightWithShadowsStratifiedPS.xpu",
                                      &m_pLightWithShadowsStratifiedPS ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    static const D3DVERTEXELEMENT9 VertexElements[] =
    {
        { 0,  0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
        { 0, 12, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
        D3DDECL_END()
    };

    m_pd3dDevice->CreateVertexDeclaration( VertexElements, &m_pSimpleVertexDecl );

    // Create the depth texture.
    if( FAILED( hr = CreateDepthTexture() ) )
        return E_FAIL;

    if( FAILED( hr = CreateRotationMap( 32 ) ) )
        return E_FAIL;

    // Create a depth buffer to write out shadow into.  Note that his overlaps in EDRAM 
    // with the main render target and z-buffer.
    D3DSURFACE_PARAMETERS SurfaceParameters;

    memset( &SurfaceParameters, 0, sizeof( D3DSURFACE_PARAMETERS ) );
    SurfaceParameters.Base = 0;
    SurfaceParameters.HierarchicalZBase = 0;

    hr = m_pd3dDevice->CreateDepthStencilSurface( SHADOW_MAP_SIZE, SHADOW_MAP_SIZE, D3DFMT_D24S8,
                                                  D3DMULTISAMPLE_NONE, 0, FALSE,
                                                  &m_pDepthStencilSurface, &SurfaceParameters );
    if( FAILED( hr ) )
        return E_FAIL;

    // Determine the aspect ratio
    FLOAT fAspectRatio = ( FLOAT )m_d3dpp.BackBufferWidth / ( FLOAT )m_d3dpp.BackBufferHeight;

    // Set the projection matrix
    m_matProj = XMMatrixPerspectiveFovLH( D3DX_PI / 4, fAspectRatio, 0.02f, 5.0f );

    // Set the view matrix
    m_matView = XMMatrixLookAtLH( m_vEyePt, m_vLookAtPt, m_vUpVec );

    // Adjust fill convention so that pixel centers are at (0.5, 0.5)
    m_pd3dDevice->SetRenderState( D3DRS_HALFPIXELOFFSET, TRUE );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: CreateRotationMap
// Desc:
//--------------------------------------------------------------------------------------
HRESULT Sample::CreateRotationMap( DWORD dwSampleGridSize )
{
    HRESULT hr;

    // Create our psuedorandom rotation map.
    hr = m_pd3dDevice->CreateTexture( dwSampleGridSize, dwSampleGridSize, 1,
                                      0, D3DFMT_LIN_L8, 0, &m_pRotationMap, NULL );

    if( FAILED( hr ) )
        return E_FAIL;

    {
        D3DLOCKED_RECT LockRect;
        m_pRotationMap->LockRect( 0, &LockRect, NULL, 0 );

        BYTE* pBits = ( BYTE* )LockRect.pBits;

        // Fill in the sampling offset map.
        for( DWORD i = 0; i < dwSampleGridSize; i++ )
        {
            for( DWORD j = 0; j < dwSampleGridSize; j++ )
            {
                // Create a random rotation.
                float rotation = rand() / float( RAND_MAX );

                BYTE* pPixel = pBits + j * LockRect.Pitch + i;

                *pPixel = BYTE( rotation * 255.0 );
            }
        }

        m_pRotationMap->UnlockRect( 0 );
    }

    // Create our stratified sampling offset map.
    hr = m_pd3dDevice->CreateArrayTexture( dwSampleGridSize, dwSampleGridSize, 32, 1,
                                           0, D3DFMT_LIN_Q8W8V8U8, 0, &m_pOffsetMap, NULL );

    if( FAILED( hr ) )
        return E_FAIL;

    // Setup the base offests (8 x 8)
    float base_offsets[32][4];
    UINT index = 4;

    // X,Y locations of 4 good representative samples
    const int rep_samples[4][2] =
    {
        { 3, 0 }, { 7, 3 }, { 4, 7 }, { 0, 4 }
    };

    for( int y = 0; y < 8; y++ )
    {
        for( int x = 0; x < 8; x++ )
        {
            float ox = float( x ) - 3.5f;
            float oy = float( y ) - 3.5f;

            if( sqrtf( ox * ox + oy * oy ) < 4.0f )
            {
                bool bRepSample = false;

                for( UINT ri = 0; ri < 4; ri++ )
                {
                    if( x == rep_samples[ri][0] && y == rep_samples[ri][1] )
                    {
                        // Representative sample
                        base_offsets[ri / 2][( ri % 2 ) * 2 + 0] = ox;
                        base_offsets[ri / 2][( ri % 2 ) * 2 + 1] = oy;

                        bRepSample = true;
                        break;
                    }
                }

                if( !bRepSample )
                {
                    // Regular sample
                    base_offsets[index / 2][( index % 2 ) * 2 + 0] = ox;
                    base_offsets[index / 2][( index % 2 ) * 2 + 1] = oy;
                    index++;
                }
            }
        }
    }

    // Fill in the sampling offset map.
    for( UINT i = 0; i < index / 2; i++ )
    {
        D3DLOCKED_RECT LockRect;
        m_pOffsetMap->LockRect( i, 0, &LockRect, NULL, 0 );

        signed char* pBits = ( signed char* )LockRect.pBits;

        for( DWORD x = 0; x < dwSampleGridSize; x++ )
        {
            for( DWORD y = 0; y < dwSampleGridSize; y++ )
            {
                signed char* pPixel = pBits + y * LockRect.Pitch + x * 4;

                for( UINT j = 0; j < 4; j++ )
                {
                    // Add a random offset (-0.5 .. 0.5) to the base offset.
                    float offset = base_offsets[i][j] + ( rand() / float( RAND_MAX ) ) - 0.5f;

                    // Our coordinate is now in the range [-4, 4], so scale it down.
                    offset *= ( 1.0f / 4.0f );

                    // Store the offsets as a signed byte
                    pPixel[j] = ( signed char )( offset * 127.0 );
                }
            }
        }

        m_pOffsetMap->UnlockRect( i, 0 );
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: CreateDepthTexture
// Desc: Create a D25S8 depth texture with a nonpacked mip-map chain.
//--------------------------------------------------------------------------------------
HRESULT Sample::CreateDepthTexture()
{
    m_pDepthTexture = new IDirect3DTexture9;

    DWORD dwTextureSize = XGSetTextureHeaderEx( SHADOW_MAP_SIZE,
                                                SHADOW_MAP_SIZE,
                                                SHADOW_MAP_LEVELS,
                                                0,
                                                D3DFMT_D24S8,
                                                0,
                                                XGHEADEREX_NONPACKED,
                                                0,
                                                XGHEADER_CONTIGUOUS_MIP_OFFSET,
                                                0,
                                                m_pDepthTexture,
                                                NULL,
                                                NULL );

    void* pBuffer = XPhysicalAlloc( dwTextureSize, MAXULONG_PTR, 0,
                                    PAGE_READWRITE | PAGE_WRITECOMBINE );

    if( pBuffer == NULL )
        return E_OUTOFMEMORY;

    XGOffsetResourceAddress( m_pDepthTexture, pBuffer );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: BuildShadowMapMipMaps
// Desc: Build a mip-map chain for a shadow map by taking the minimum of 4 samples at 
//       each mip-map level.
//--------------------------------------------------------------------------------------
void Sample::BuildShadowMapMipMaps( IDirect3DTexture9* pShadowMap )
{
    // Note, the depth buffer (m_pDepthStencilSurface) is still set at this point.

    static SIMPLEVERTEX Vertices[] =
    {
        { -1.0f,  1.0f, 0.5f, 0.0f, 0.0f }, // x, y, z, color, s, t
        {  1.0f,  1.0f, 0.5f, 1.0f, 0.0f },
        {  1.0f, -1.0f, 0.5f, 1.0f, 1.0f },
        { -1.0f, -1.0f, 0.5f, 0.0f, 1.0f },
    };

    m_pd3dDevice->SetVertexShader( m_pDownsampleDepthVS );
    m_pd3dDevice->SetPixelShader( m_pDownsampleDepthPS );

    // Set the projection matrix.
    XMMATRIX matProjection = XMMatrixOrthographicLH( 2.0f, 2.0f, 0.0f, 1.0f );
    m_pd3dDevice->SetVertexShaderConstantF( 0, ( FLOAT* )&matProjection, 4 );

    m_pd3dDevice->SetTexture( 0, m_pDepthTexture );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_POINT );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_POINT );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MIPFILTER, D3DTEXF_POINT );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP );

    int levels = pShadowMap->GetLevelCount();

    for( int i = 1; i < levels; i++ )
    {
        // Sample from the next highest mip-level.
        m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MINMIPLEVEL, i - 1 );
        m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MAXMIPLEVEL, i - 1 );

        // Adjust the viewport to match the size of the mip-level.
        D3DSURFACE_DESC level_desc;
        pShadowMap->GetLevelDesc( i, &level_desc );

        D3DVIEWPORT9 vp;
        vp.X = 0;
        vp.Y = 0;
        vp.Width = level_desc.Width;
        vp.Height = level_desc.Height;
        vp.MinZ = 0.0f;
        vp.MaxZ = 1.0f;
        m_pd3dDevice->SetViewport( &vp );

        // Clear the depth buffer.
        m_pd3dDevice->ClearF( D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL, NULL, NULL, 1.0f, 0L );

        // Draw a full screen rectangle for the downsample.
        m_pd3dDevice->SetVertexDeclaration( m_pSimpleVertexDecl );

        m_pd3dDevice->DrawPrimitiveUP( D3DPT_QUADLIST, 1, Vertices, sizeof( SIMPLEVERTEX ) );

        // Resolve to the mip-level of our texture.
        m_pd3dDevice->Resolve( D3DRESOLVE_DEPTHSTENCIL, NULL, m_pDepthTexture, NULL, i, 0,
                               NULL, 1.0f, 0, NULL );
    }

    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MINMIPLEVEL, 13 );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MAXMIPLEVEL, 0 );

    return;
}


//--------------------------------------------------------------------------------------
// Name: Update()
// Desc: Called once per frame, the call is the entry point for animating the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Update()
{
    // Get the elapsed time for the last frame.
    FLOAT fElapsedTime = ( FLOAT )m_Timer.GetElapsedTime();

    // Get the current gamepad status
    ATG::GAMEPAD* pGamepad = ATG::Input::GetMergedInput();

    // Toggle help
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_BACK )
        m_bDrawHelp = !m_bDrawHelp;

    // Change sampling method
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_A )
        m_SamplingMethod = ( m_SamplingMethod + 1 ) % 4;

    // Rotate the view based on the left stick.
    XMMATRIX matRotate = XMMatrixRotationAxis( m_vUpVec, pGamepad->fX1 * fElapsedTime );
    m_vEyePt = XMVector3TransformCoord( m_vEyePt, matRotate );

    // Rotate eye points around side axis
    XMVECTOR vView = ( m_vLookAtPt - m_vEyePt );
    FLOAT dist = XMVector3Length( vView ).x;

    vView = XMVector3Normalize( vView );

    // Place limits so we don't go over the top or under the bottom
    FLOAT dot = XMVector3Dot( vView, m_vUpVec ).x;
    if( ( dot < 0.0f || pGamepad->fY1 < 0.0f ) && ( dot > -0.99f || pGamepad->fY1 > 0.0f ) )
    {
        XMVECTOR vAxis = XMVector3Cross( vView, m_vUpVec );

        matRotate = XMMatrixRotationAxis( vAxis, pGamepad->fY1 * fElapsedTime );
        m_vEyePt = XMVector3TransformCoord( m_vEyePt, matRotate );
    }

    // Move in/out
    FLOAT fIn = ( pGamepad->bRightTrigger / 255.0f );
    FLOAT fOut = ( pGamepad->bLeftTrigger / 255.0f );

    if( fIn > 0.1f && dist > 0.1f )
        m_vEyePt += vView * 2.0f * fIn * fElapsedTime;

    if( fOut > 0.1f )
        m_vEyePt -= vView * 2.0f * fOut * fElapsedTime;

    m_matView = XMMatrixLookAtLH( m_vEyePt, m_vLookAtPt, m_vUpVec );

    // Rotate the light direction based on the right stick.
    matRotate = XMMatrixRotationAxis( m_vUpVec, pGamepad->fX2 * 2.0f * fElapsedTime );
    m_vLightDirection = XMVector3TransformNormal( m_vLightDirection, matRotate );

    // Place limits so we don't go over the top or under the bottom
    dot = XMVector3Dot( m_vLightDirection, m_vUpVec ).x;
    if( ( dot > 0.01f || pGamepad->fY2 > 0.0f ) && ( dot < 0.99f || pGamepad->fY2 < 0.0f ) )
    {
        XMVECTOR vAxis = XMVector3Cross( m_vLightDirection, m_vUpVec );

        matRotate = XMMatrixRotationAxis( vAxis, pGamepad->fY2 * fElapsedTime );
        m_vLightDirection = XMVector3TransformNormal( m_vLightDirection, matRotate );
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Render()
// Desc: Sets up render states, clears the viewport, and renders the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Render()
{
    // Compute the light view and light projection matrices.
    XMMATRIX matLightView, matLightProjection;

    FLOAT fBoardSize = m_ChessBoard.GetGridSize() * 8.0f;
    FLOAT fRadius = sqrt( ( fBoardSize * 0.5f ) * ( fBoardSize * 0.5f ) * 2.0f );

    float fNear = 0.1f;
    float fFar = fNear + fRadius * 2.0f;

    // Light orientation (looks at zero)
    XMVECTOR vFrom = m_vLightDirection * ( fRadius + fNear );
    XMVECTOR vTo = XMVectorSet( 0.0f, 0.0f, 0.0f, 1.0f );
    XMVECTOR vUp = XMVectorSet( 0.0f, 1.0f, 0.0001f, 0.0f );
    matLightView = XMMatrixLookAtLH( vFrom, vTo, vUp );

    // Include the entire world in the orthographic projection.
    FLOAT fWidth = fRadius * 2.0f;
    FLOAT fHeight = fRadius * 2.0f;

    matLightProjection = XMMatrixOrthographicLH( fWidth, fHeight, fNear, fFar );

    // Setup the texture matrix.
    XMMATRIX matTexture( 0.5f,  0.0f,  0.0f,  0.0f,
                         0.0f, -0.5f,  0.0f,  0.0f,
                         0.0f,  0.0f,  1.0f,  0.0f,
                         0.5f,  0.5f,  0.0f,  1.0f );

    // Save the back buffer, and z-buffer.
    IDirect3DSurface9* pBackBufferSurface;
    IDirect3DSurface9* pZBufferSurface;
    m_pd3dDevice->GetRenderTarget( 0, &pBackBufferSurface );
    m_pd3dDevice->GetDepthStencilSurface( &pZBufferSurface );

    // Initialize some render states.
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_ZWRITEENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_ZFUNC, D3DCMP_LESSEQUAL );

    //
    // Draw the shadow map.  Use backfaces only.
    //
    m_pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_CW );

    m_pd3dDevice->SetRenderState( D3DRS_DEPTHBIAS, ATG::FtoDW( -0.001f ) );
    m_pd3dDevice->SetRenderState( D3DRS_SLOPESCALEDEPTHBIAS, ATG::FtoDW( -2.0f ) );

    m_pd3dDevice->SetRenderState( D3DRS_COLORWRITEENABLE, 0 );
    m_pd3dDevice->SetRenderTarget( 0, NULL );
    m_pd3dDevice->SetDepthStencilSurface( m_pDepthStencilSurface );

    // Clear the z-buffer.
    m_pd3dDevice->ClearF( D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL, NULL, NULL, 1.0f, 0L );

    // Set the vertex shader to our depth only shader
    m_pd3dDevice->SetVertexShader( m_pWriteDepthVS );

    // Set the pixel shader to NULL to use double-depth mode.
    m_pd3dDevice->SetPixelShader( NULL );

    // Render the chess pieces
    {
        g_matViewProj = matLightView * matLightProjection;

        m_ChessBoard.RenderPieces( XMMatrixIdentity() );
    }

    // Restore states.
    m_pd3dDevice->SetRenderState( D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_ALL );
    m_pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_NONE );
    m_pd3dDevice->SetRenderState( D3DRS_DEPTHBIAS, 0 );
    m_pd3dDevice->SetRenderState( D3DRS_SLOPESCALEDEPTHBIAS, 0 );

    // Resolve depth to our texture.
    m_pd3dDevice->Resolve( D3DRESOLVE_DEPTHSTENCIL, NULL, m_pDepthTexture,
                           NULL, 0, 0, NULL, 1.0f, 0, NULL );

    // Build mip-maps for the depth texture.
    BuildShadowMapMipMaps( m_pDepthTexture );

    // Restore the main render/depth targets.
    m_pd3dDevice->SetRenderTarget( 0, pBackBufferSurface );
    m_pd3dDevice->SetDepthStencilSurface( pZBufferSurface );

    pBackBufferSurface->Release();
    pZBufferSurface->Release();


    // Draw a gradient filled background and clear the z-buffer.
    ATG::RenderBackground( 0xff0000ff, 0xff000000 );

    // Restore some render states that are whacked by the above function.
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_ZWRITEENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_ZFUNC, D3DCMP_LESSEQUAL );


    //
    // Draw the scene using the shadow map.
    //
    m_pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_CCW );

    // Set the vertex shader
    m_pd3dDevice->SetVertexShader( m_pLightWithShadowsVS );

    // Set the pixel shader
    if( m_SamplingMethod == 0 )
    {
        m_pd3dDevice->SetPixelShader( m_pLightWithShadowsPointPS );
    }
    else if( m_SamplingMethod == 1 )
    {
        m_pd3dDevice->SetPixelShader( m_pLightWithShadowsBilinearPS );
    }
    else if( m_SamplingMethod == 2 )
    {
        m_pd3dDevice->SetPixelShader( m_pLightWithShadowsRotatedPoissonPS );

        m_pd3dDevice->SetTexture( 2, m_pRotationMap );

        // Point sample with wrap.
        m_pd3dDevice->SetSamplerState( 2, D3DSAMP_MINFILTER, D3DTEXF_POINT );
        m_pd3dDevice->SetSamplerState( 2, D3DSAMP_MAGFILTER, D3DTEXF_POINT );
        m_pd3dDevice->SetSamplerState( 2, D3DSAMP_MIPFILTER, D3DTEXF_NONE );
        m_pd3dDevice->SetSamplerState( 2, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP );
        m_pd3dDevice->SetSamplerState( 2, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP );

        XMVECTOR FilterScale = XMVectorSet( 1.0f / SHADOW_MAP_SIZE, 0.0f, 0.0f, 0.0f );
        m_pd3dDevice->SetPixelShaderConstantF( 2, ( float* )&FilterScale, 1 );
    }
    else
    {
        m_pd3dDevice->SetPixelShader( m_pLightWithShadowsStratifiedPS );

        m_pd3dDevice->SetTexture( 3, m_pOffsetMap );

        // Point sample with wrap.
        m_pd3dDevice->SetSamplerState( 3, D3DSAMP_MINFILTER, D3DTEXF_POINT );
        m_pd3dDevice->SetSamplerState( 3, D3DSAMP_MAGFILTER, D3DTEXF_POINT );
        m_pd3dDevice->SetSamplerState( 3, D3DSAMP_MINFILTERZ, D3DTEXF_POINT );
        m_pd3dDevice->SetSamplerState( 3, D3DSAMP_MAGFILTERZ, D3DTEXF_POINT );
        m_pd3dDevice->SetSamplerState( 3, D3DSAMP_MIPFILTER, D3DTEXF_NONE );
        m_pd3dDevice->SetSamplerState( 3, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP );
        m_pd3dDevice->SetSamplerState( 3, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP );

        XMVECTOR FilterScale = XMVectorSet( 2.0f / SHADOW_MAP_SIZE, 0.0f, 0.0f, 0.0f );
        m_pd3dDevice->SetPixelShaderConstantF( 2, ( float* )&FilterScale, 1 );
    }

    // State for the diffuse texture
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP );

    // Set the depth texture
    m_pd3dDevice->SetTexture( 1, m_pDepthTexture );

    // Use a border color that is equivalent to 1.0 depth
    m_pd3dDevice->SetSamplerState( 1, D3DSAMP_BORDERCOLOR, 0xffffffff );

    // Point sample the depth texture.
    m_pd3dDevice->SetSamplerState( 1, D3DSAMP_MINFILTER, D3DTEXF_POINT );
    m_pd3dDevice->SetSamplerState( 1, D3DSAMP_MAGFILTER, D3DTEXF_POINT );
    m_pd3dDevice->SetSamplerState( 1, D3DSAMP_MIPFILTER, D3DTEXF_POINT );
    m_pd3dDevice->SetSamplerState( 1, D3DSAMP_ADDRESSU, D3DTADDRESS_BORDER );
    m_pd3dDevice->SetSamplerState( 1, D3DSAMP_ADDRESSV, D3DTADDRESS_BORDER );

    // Render the chess pieces and board
    {
        g_matViewProj = m_matView * m_matProj;
        g_matShadowViewProj = matLightView * matLightProjection * matTexture;
        g_vLightDirection = m_vLightDirection;
        g_vViewPosition = m_vEyePt;

        m_ChessBoard.RenderPieces( XMMatrixIdentity() );
        m_ChessBoard.Render( XMMatrixIdentity() );
    }

    // Cleanup
    m_pd3dDevice->SetTexture( 0, NULL );
    m_pd3dDevice->SetTexture( 1, NULL );

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
        m_Font.DrawText( 0, 0, 0xffffffff, L"ShadowMap" );

        m_Font.SetScaleFactors( 1.0f, 1.0f );
        m_Font.DrawText( 0, 0, 0xffffff00, m_Timer.GetFrameRate(), ATGFONT_RIGHT );

        const WCHAR* strSamplingMethodNames[] = { L"Point", L"Bilinear", L"Rotated Poisson", L"Stratified" };
        m_Font.DrawText( 0, 30, 0xffffffff, strSamplingMethodNames[m_SamplingMethod] );

        m_Font.End();
    }

    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}

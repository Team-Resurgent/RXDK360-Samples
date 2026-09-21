//--------------------------------------------------------------------------------------
// DisplacementMap.cpp
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include <xtl.h>
#include <xboxmath.h>
#include <xgraphics.h>
#include <AtgApp.h>
#include <AtgFont.h>
#include <AtgHelp.h>
#include <AtgInput.h>
#include <AtgResource.h>
#include <AtgUtil.h>


//--------------------------------------------------------------------------------------
// Callouts for labelling the gamepad on the help screen
//--------------------------------------------------------------------------------------
ATG::HELP_CALLOUT g_HelpCallouts[] =
{
    { ATG::HELP_BACK_BUTTON,  ATG::HELP_PLACEMENT_2, L"Display\nhelp" },
    { ATG::HELP_LEFT_STICK,   ATG::HELP_PLACEMENT_2, L"Move\ncamera" },
    { ATG::HELP_RIGHT_STICK,  ATG::HELP_PLACEMENT_2, L"Rotate\ncamera" },
    { ATG::HELP_A_BUTTON,     ATG::HELP_PLACEMENT_2, L"Toggle Adaptive\nTesselltaion" },
    { ATG::HELP_B_BUTTON,     ATG::HELP_PLACEMENT_1, L"Toggle Wireframe" },
    { ATG::HELP_DPAD,         ATG::HELP_PLACEMENT_2, L"Change Tessellation\nLevel" },
};
const DWORD NUM_HELP_CALLOUTS = sizeof( g_HelpCallouts ) / sizeof( g_HelpCallouts[0] );


// Width and height of the grid of quad patches
const UINT  GridWidth = 100;
const UINT  GridHeight = 100;

// Scale for height values
const FLOAT g_fHeightScale = 0.03f;

// Scale factor for computing the adaptive level of an edge based on its length.
const FLOAT fAdaptiveScaleInPixels = 4.0f;


//--------------------------------------------------------------------------------------
// Name: class Sample
// Desc: Main class to run this application. Most functionality is inherited
//       from the ATG::Application base class.
//--------------------------------------------------------------------------------------
class Sample : public ATG::Application
{
    ATG::Timer m_Timer;
    ATG::Font m_Font;
    ATG::Help m_Help;

    ATG::PackedResource m_Resource;

    BOOL m_bDrawHelp;

    BOOL m_bWireframe;               // Wireframe view for debugging

    BOOL m_bAdaptiveTessellation;    // Adaptively tessellate

    FLOAT m_fTessellationLevel;       // Max tessellation level.

    XMMATRIX m_matWorld;
    XMMATRIX m_matView;
    XMMATRIX m_matProj;

    XMVECTOR m_vCameraPos;
    FLOAT m_fCameraPitch;
    FLOAT m_fCameraYaw;

    IDirect3DTexture9* m_pDiffuseMap;
    IDirect3DTexture9* m_pDisplacementMap;

    // Mem export shader to compute adaptive tessellation factors.
    IDirect3DVertexShader9* m_pMemExportShader;

    // Displacement mapping vertex shader
    IDirect3DVertexShader9* m_pVertexShader;

    // Simple pixel shader for rendering
    IDirect3DPixelShader9* m_pPixelShader;

    // Index buffer for tessellation factors
    IDirect3DIndexBuffer9* m_pTessFactorBuffer;

    // Pointer to the index buffer memory, used for settting up the memory export.    
    VOID* m_pTessFactorPtr;

    HRESULT         UpdatePerEdgeFactors();

private:
    virtual HRESULT Initialize();
    virtual HRESULT Update();
    virtual HRESULT Render();
};


//--------------------------------------------------------------------------------------
// Name: main()
// Desc: Entry point to the program
//--------------------------------------------------------------------------------------
INT __cdecl main()
{
    Sample atgApp;
    ATG::GetVideoSettings( &atgApp.m_d3dpp.BackBufferWidth, &atgApp.m_d3dpp.BackBufferHeight );

    atgApp.m_d3dpp.BackBufferFormat =  ( D3DFORMAT )MAKESRGBFMT( D3DFMT_A8R8G8B8 );
    atgApp.m_d3dpp.FrontBufferFormat = ( D3DFORMAT )MAKESRGBFMT( D3DFMT_LE_X8R8G8B8 );

    atgApp.Run();
}


//--------------------------------------------------------------------------------------
// Name: UpdatePerEdgeFactors()
// Desc: Get a pointer to the tessellation factor buffer so we can use memexport later.
//--------------------------------------------------------------------------------------
HRESULT Sample::UpdatePerEdgeFactors()
{
    // Lock the buffer (and save the address).
    if( FAILED( m_pTessFactorBuffer->Lock( 0, 0, &m_pTessFactorPtr, 0 ) ) )
        return E_FAIL;

    // Each quad patch uses 4 tessellation factors, one for each edge.
    UINT NumTessFactors = GridWidth * GridHeight * 4;

    // Setup some default values.
    FLOAT* pTessFactors = ( FLOAT* )m_pTessFactorPtr;
    for( UINT i = 0; i < NumTessFactors; i++ )
    {
        pTessFactors[i] = 1.0f;
    }

    m_pTessFactorBuffer->Unlock();

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Initialize()
// Desc: This creates all device-dependent display objects.
//--------------------------------------------------------------------------------------
HRESULT Sample::Initialize()
{
    m_bDrawHelp = FALSE;
    m_bAdaptiveTessellation = TRUE;
    m_bWireframe = FALSE;

    m_fTessellationLevel = 15.0f;

    m_vCameraPos = XMVectorSet( 0.0f, 0.05f, -0.5f, 1.0f );
    m_fCameraPitch = 0.34f;
    m_fCameraYaw = 0.0f;

    // Create the font
    if( FAILED( m_Font.Create( "game:\\Media\\Fonts\\Arial_16.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Confine text drawing to the title safe area
    m_Font.SetWindow( ATG::GetTitleSafeArea() );

    // Create the help
    if( FAILED( m_Help.Create( "game:\\media\\help\\Help.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Create the resources
    if( FAILED( m_Resource.Create( "game:\\Media\\Resource.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    m_pDiffuseMap = m_Resource.GetTexture( "DiffuseMap" );
    m_pDisplacementMap = m_Resource.GetTexture( "DisplacementMap" );

    // Load the memory export shader that calculates the tessellation factors
    if( FAILED( ATG::LoadVertexShader( "game:\\Media\\Shaders\\ComputeTessFactorsVS.xvu",
                                       &m_pMemExportShader ) ) )
    {
        ATG_PrintError( "Couldn't load %s\n", "game:\\Media\\Shaders\\ComputeTessFactorsVS.xvu" );
    }

    // Load the displacement mapping vertex shader
    if( FAILED( ATG::LoadVertexShader( "game:\\Media\\Shaders\\DisplacementMapVS.xvu",
                                       &m_pVertexShader ) ) )
    {
        ATG_PrintError( "Couldn't load %s\n", "game:\\Media\\Shaders\\DisplacementMapVS.xvu" );
    }

    // Load a simple pixel shader
    if( FAILED( ATG::LoadPixelShader( "game:\\Media\\Shaders\\DisplacementMapPS.xpu",
                                      &m_pPixelShader ) ) )
    {
        ATG_PrintError( "Couldn't load %s\n", "game:\\Media\\Shaders\\DisplacementMapPS.xpu" );
        return E_FAIL;
    }

    // Each quad patch uses 4 tessellation factors, one for each edge.
    UINT NumTessFactors = GridWidth * GridHeight * 4;

    // Create an index buffer to hold the tessellation factors. Use of an index buffer
    // with per-edge tessellation factors is only valid when drawing non-indexed patches
    // with D3DRS_TESSELLATIONMODEis set to D3DTM_PEREDGE.
    if( FAILED( m_pd3dDevice->CreateIndexBuffer( NumTessFactors * sizeof( float ),
                                                 D3DUSAGE_WRITEONLY,
                                                 D3DFMT_INDEX32,
                                                 D3DPOOL_DEFAULT,
                                                 &m_pTessFactorBuffer,
                                                 NULL ) ) )
        return E_FAIL;

    // Update the per-edge tessellation factors.
    UpdatePerEdgeFactors();

    // World matrix.
    m_matWorld = XMMatrixTranslation( -0.5f, -0.5f, 0.0f ) * XMMatrixRotationX( -XM_PI / 2 );

    // View matrix
    m_matView = XMMatrixTranslationFromVector( -m_vCameraPos ) *
        XMMatrixRotationY( -m_fCameraYaw ) *
        XMMatrixRotationX( -m_fCameraPitch );

    // Determine the aspect ratio
    XVIDEO_MODE VideoMode;
    ZeroMemory( &VideoMode, sizeof( VideoMode ) );
    XGetVideoMode( &VideoMode );

    FLOAT fAspect = VideoMode.fIsWideScreen ? ( 16.0f / 9.0f ) : ( 4.0f / 3.0f );

    // Projection matrix
    m_matProj = XMMatrixPerspectiveFovLH( XM_PI / 4, fAspect, 0.01f, 10.0f );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Update()
// Desc: Called once per frame, the call is the entry point for animating
//       the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Update()
{
    FLOAT m_fElapsedTime = ( FLOAT )m_Timer.GetElapsedTime();

    // Get the current gamepad status
    ATG::GAMEPAD* pGamepad = ATG::Input::GetMergedInput();

    // Toggle help
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_BACK )
        m_bDrawHelp = !m_bDrawHelp;

    // Toggle adaptive tessellation
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_A )
        m_bAdaptiveTessellation = !m_bAdaptiveTessellation;

    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_B )
        m_bWireframe = !m_bWireframe;

    // Adjust the tessellation level.
    {
        if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_DPAD_UP )
            m_fTessellationLevel += 1.0f;
        else if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_DPAD_DOWN )
            m_fTessellationLevel -= 1.0f;

        if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_DPAD_LEFT )
            m_fTessellationLevel += 0.1f;
        else if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_DPAD_RIGHT )
            m_fTessellationLevel -= 0.1f;

        // Clamp tessellation level to be a legal value.
        const FLOAT fMaxLevel = 15.0f;

        if( m_fTessellationLevel > fMaxLevel )
            m_fTessellationLevel = fMaxLevel;
        else if( m_fTessellationLevel < 1.0f )
            m_fTessellationLevel = 1.0f;
    }

    // Move the camera
    {
        // Rotate the camera
        m_fCameraPitch += pGamepad->fY2 * m_fElapsedTime;
        m_fCameraYaw += pGamepad->fX2 * m_fElapsedTime;

        // Translate the camera
        XMVECTOR vDet;
        XMMATRIX matInverseView = XMMatrixInverse( &vDet, m_matView );

        XMVECTOR vForward = XMVectorSet( 0.0f, 0.0f, 1.0f, 0.0f );
        XMVECTOR vCross = XMVectorSet( 1.0f, 0.0f, 0.0f, 0.0f );

        vForward = XMVector3TransformNormal( vForward, matInverseView );
        vCross = XMVector3TransformNormal( vCross, matInverseView );

        m_vCameraPos += pGamepad->fX1 * m_fElapsedTime * 0.5f * vCross;
        m_vCameraPos += pGamepad->fY1 * m_fElapsedTime * 0.5f * vForward;

        // Build the view matrix
        m_matView = XMMatrixTranslationFromVector( -m_vCameraPos ) *
            XMMatrixRotationY( -m_fCameraYaw ) *
            XMMatrixRotationX( -m_fCameraPitch );
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
    ATG::RenderBackground( 0xff0000ff, 0xff000000 );

    m_pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_NONE );

    // Set the tessellation level.
    m_pd3dDevice->SetRenderState( D3DRS_MAXTESSELLATIONLEVEL,
                                  ATG::FtoDW( m_fTessellationLevel ) );

    // Wireframe for debugging.
    if( m_bWireframe )
        m_pd3dDevice->SetRenderState( D3DRS_FILLMODE, D3DFILL_WIREFRAME );

    // Set the composite world*view*projection matrix.
    XMMATRIX matWVP = m_matWorld * m_matView * m_matProj;
    m_pd3dDevice->SetVertexShaderConstantF( 0, ( FLOAT* )&matWVP, 4 );

    // Set GridWidth
    XMFLOAT4 vGridWidth = XMFLOAT4( ( FLOAT )GridWidth, 1.0f / GridWidth, 0.5f / GridWidth, 0.0f );
    m_pd3dDevice->SetVertexShaderConstantF( 5, ( FLOAT* )&vGridWidth, 1 );

    // Set TexCoordScale
    XMFLOAT4 vTexCoordScale = XMFLOAT4( 1.0f / GridWidth, 1.0f / GridHeight, 0.0f, 0.0f );
    m_pd3dDevice->SetVertexShaderConstantF( 6, ( FLOAT* )&vTexCoordScale, 1 );

    // Set PositionScale
    XMFLOAT4 vPositionScale = XMFLOAT4( 1.0f / GridWidth, 1.0f / GridHeight, g_fHeightScale, 1.0f );
    m_pd3dDevice->SetVertexShaderConstantF( 7, ( FLOAT* )&vPositionScale, 1 );

    // Set TessEdgeLenScale
    FLOAT fWidthScale = ( m_d3dpp.BackBufferWidth * 0.5f ) / fAdaptiveScaleInPixels;
    FLOAT fHeightScale = ( m_d3dpp.BackBufferHeight * 0.5f ) / fAdaptiveScaleInPixels;
    XMFLOAT4 vTessEdgeLenScale = XMFLOAT4( fWidthScale, fHeightScale, 0.0f, 0.0f );
    m_pd3dDevice->SetVertexShaderConstantF( 8, ( FLOAT* )&vTessEdgeLenScale, 1 );

    // Set TextureDimensions
    D3DSURFACE_DESC Desc;
    m_pDisplacementMap->GetLevelDesc( 0, &Desc );
    XMFLOAT4 vTextureDimensions = XMFLOAT4( ( FLOAT )Desc.Width, ( FLOAT )Desc.Height, 0.0f, 0.0f );
    m_pd3dDevice->SetVertexShaderConstantF( 9, ( FLOAT* )&vTextureDimensions, 1 );

    // If we are adaptively tessellating, compute the per-edge tessellation factors.
    if( m_bAdaptiveTessellation )
    {
        // Notify the device that an export is beginning into the given vertex buffer
        m_pd3dDevice->BeginExport( 0, m_pTessFactorBuffer, D3DBEGINEXPORT_VERTEXSHADER );

        // Set up the vertex shader
        m_pd3dDevice->SetVertexShader( m_pMemExportShader );

        UINT NumQuads = GridWidth * GridHeight;

        // Set up the Memory Export Stream Constant using the macro defined in d3d9gpu.h
        GPU_MEMEXPORT_STREAM_CONSTANT streamConstant;
        GPU_SET_MEMEXPORT_STREAM_CONSTANT( &streamConstant,
                                           ( ( BYTE* )m_pTessFactorPtr ),                 // pointer to the data
                                           NumQuads,                                   // max index = # of vertices * stride
                                           SURFACESWAP_LOW_RED,                        // whether to output ABGR or ARGB           
                                           GPUSURFACENUMBER_FLOAT,                     // data type 
                                           GPUCOLORFORMAT_32_32_32_32_FLOAT,           // data format
                                           GPUENDIAN128_8IN32 );                        // endian swap

        m_pd3dDevice->SetVertexShaderConstantF( 4, streamConstant.c, 1 );

        // Draw the vertices.
        m_pd3dDevice->DrawPrimitive( D3DPT_POINTLIST, 0, NumQuads );

        m_pd3dDevice->EndExport( 0, m_pTessFactorBuffer, 0 );
    }

    // Set the shaders.
    m_pd3dDevice->SetVertexShader( m_pVertexShader );
    m_pd3dDevice->SetPixelShader( m_pPixelShader );

    // Set the textures.
    m_pd3dDevice->SetTexture( 0, m_pDiffuseMap );

    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP );

    m_pd3dDevice->SetTexture( D3DDMAPSAMPLER, m_pDisplacementMap );

    m_pd3dDevice->SetSamplerState( 16, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetSamplerState( 16, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetSamplerState( 16, D3DSAMP_MIPFILTER, D3DTEXF_NONE );
    m_pd3dDevice->SetSamplerState( 16, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP );
    m_pd3dDevice->SetSamplerState( 16, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP );

    // The number of primitives to draw
    UINT PrimitiveCount = GridWidth * GridHeight;

    // Render the patch data.
    if( m_bAdaptiveTessellation )
    {
        // Set the tessellation mode.
        m_pd3dDevice->SetRenderState( D3DRS_TESSELLATIONMODE,
                                      D3DTM_PEREDGE );

        // Set the tessellation factor buffer.
        m_pd3dDevice->SetIndices( m_pTessFactorBuffer );

        m_pd3dDevice->DrawIndexedTessellatedPrimitive( D3DTPT_QUADPATCH, 0, 0,
                                                       PrimitiveCount );
    }
    else
    {
        // Set the tessellation mode.
        m_pd3dDevice->SetRenderState( D3DRS_TESSELLATIONMODE,
                                      D3DTM_CONTINUOUS );

        m_pd3dDevice->DrawTessellatedPrimitive( D3DTPT_QUADPATCH, 0, PrimitiveCount );
    }

    m_pd3dDevice->SetIndices( NULL );

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
        m_Font.DrawText( 0, 0, 0xffffffff, L"DisplacementMap" );
        m_Font.SetScaleFactors( 1.0f, 1.0f );
        m_Font.DrawText( 0, 0, 0xffffff00, m_Timer.GetFrameRate(), ATGFONT_RIGHT );

        const WCHAR* strModeNames[] = { L"Non-Adaptive", L"Adaptive" };
        m_Font.DrawText( 0, 30, 0xffffffff, strModeNames[m_bAdaptiveTessellation ? 1 : 0] );

        if( m_bAdaptiveTessellation )
        {
            WCHAR str[64];
            swprintf_s( str, L"Max Tess. Level: %0.01f\n", m_fTessellationLevel );
            m_Font.DrawText( 0, 30, 0xffffffff, str, ATGFONT_RIGHT );
        }
        else
        {
            WCHAR str[64];
            swprintf_s( str, L"Tess. Level: %0.01f", m_fTessellationLevel );
            m_Font.DrawText( 0, 30, 0xffffffff, str, ATGFONT_RIGHT );
        }

        m_Font.End();
    }

    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}

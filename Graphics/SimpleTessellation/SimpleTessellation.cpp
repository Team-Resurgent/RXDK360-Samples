//--------------------------------------------------------------------------------------
// SimpleTessellation.cpp
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
#include <AtgResource.h>
#include <AtgUtil.h>


//--------------------------------------------------------------------------------------
// Callouts for labelling the gamepad on the help screen
//--------------------------------------------------------------------------------------
ATG::HELP_CALLOUT g_HelpCallouts[] =
{
    { ATG::HELP_BACK_BUTTON,  ATG::HELP_PLACEMENT_2, L"Display\nhelp" },
    { ATG::HELP_A_BUTTON,     ATG::HELP_PLACEMENT_1, L"Next Test" },
    { ATG::HELP_B_BUTTON,     ATG::HELP_PLACEMENT_1, L"Previous Test" },
    { ATG::HELP_X_BUTTON,     ATG::HELP_PLACEMENT_1, L"Toggle Indexed" },
    { ATG::HELP_DPAD,         ATG::HELP_PLACEMENT_2, L"Change Tessellation\nLevel" },
    { ATG::HELP_RIGHT_STICK,  ATG::HELP_PLACEMENT_2, L"Select per-edge\nfactor to adjust" },
};
static const DWORD  NUM_HELP_CALLOUTS = sizeof( g_HelpCallouts ) / sizeof( g_HelpCallouts[0] );


//--------------------------------------------------------------------------------------
// Structure to hold vertex data.
//--------------------------------------------------------------------------------------
struct COLORVERTEX
{
    FLOAT Position[3];
    DWORD Color;
};


//--------------------------------------------------------------------------------------
// Vertex declarations.
//--------------------------------------------------------------------------------------
const D3DVERTEXELEMENT9 ColorVertexElements[] =
{
    { 0,  0, D3DDECLTYPE_FLOAT3,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
    { 0, 12, D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR,    0 },
    D3DDECL_END()
};

const D3DVERTEXELEMENT9 LinePatchVertexElements[] =
{
    { 0,  0, D3DDECLTYPE_FLOAT3,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
    { 0, 12, D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR,    0 },
    { 0, 16, D3DDECLTYPE_FLOAT3,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 1 },
    { 0, 28, D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR,    1 },
    D3DDECL_END()
};

const D3DVERTEXELEMENT9 TriPatchVertexElements[] =
{
    { 0,  0, D3DDECLTYPE_FLOAT3,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
    { 0, 12, D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR,    0 },
    { 0, 16, D3DDECLTYPE_FLOAT3,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 1 },
    { 0, 28, D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR,    1 },
    { 0, 32, D3DDECLTYPE_FLOAT3,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 2 },
    { 0, 44, D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR,    2 },
    D3DDECL_END()
};

const D3DVERTEXELEMENT9 QuadPatchVertexElements[] =
{
    { 0,  0, D3DDECLTYPE_FLOAT3,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
    { 0, 12, D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR,    0 },
    { 0, 16, D3DDECLTYPE_FLOAT3,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 1 },
    { 0, 28, D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR,    1 },
    { 0, 32, D3DDECLTYPE_FLOAT3,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 2 },
    { 0, 44, D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR,    2 },
    { 0, 48, D3DDECLTYPE_FLOAT3,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 3 },
    { 0, 60, D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR,    3 },
    D3DDECL_END()
};


//--------------------------------------------------------------------------------------
// Vertex data.
//--------------------------------------------------------------------------------------
COLORVERTEX LineSourceVertices[] =
{
    { -1.0f, 0.0f, 0.0f, 0x00FF0000 }, // Left
    {  1.0f, 0.0f, 0.0f, 0x0000FF00 }, // Right
};


COLORVERTEX TriSourceVertices[] =
{
    { -1.0f, -1.0f, 0.0f, 0x00FF0000 }, // Bottom Left
    {  0.0f,  1.0f, 0.0f, 0x0000FF00 }, // Top
    {  1.0f, -1.0f, 0.0f, 0x000000FF }  // Bottom Right
};


COLORVERTEX TriStripSourceVertices[] =
{
    {  1.0f,  1.0f, 0.0f, 0x00FF0000 }, // Top Right
    {  1.0f, -1.0f, 0.0f, 0x0000FF00 }, // Bottom Right
    { -1.0f,  1.0f, 0.0f, 0x00FFFFFF }, // Top Left
    { -1.0f, -1.0f, 0.0f, 0x000000FF }  // Bottom Left
};


COLORVERTEX QuadSourceVertices[] =
{
    { -1.0f,  1.0f, 0.0f, 0x00FFFFFF }, // Top Left
    {  1.0f,  1.0f, 0.0f, 0x00FF0000 }, // Top Right
    {  1.0f, -1.0f, 0.0f, 0x0000FF00 }, // Bottom Right
    { -1.0f, -1.0f, 0.0f, 0x000000FF }  // Bottom Left
};


COLORVERTEX TriPatchSourceVertices[] =
{
    {  0.0f,  1.0f, 0.0f, 0x0000FF00 }, // Top
    {  1.0f, -1.0f, 0.0f, 0x000000FF }, // Bottom Right
    { -1.0f, -1.0f, 0.0f, 0x00FF0000 }  // Bottom Left
};


COLORVERTEX QuadPatchSourceVertices[] =
{
    { -1.0f,  1.0f, 0.0f, 0x00FFFFFF }, // Top Left
    {  1.0f,  1.0f, 0.0f, 0x00FF0000 }, // Top Right
    { -1.0f, -1.0f, 0.0f, 0x000000FF }, // Bottom Left
    {  1.0f, -1.0f, 0.0f, 0x0000FF00 }  // Bottom Right
};


//--------------------------------------------------------------------------------------
// Structure to hold the test data for each primtive type.
//--------------------------------------------------------------------------------------
struct TestPrimitive
{
    D3DTESSPRIMITIVETYPE Primitive;
    UINT PrimitiveCount;

    const VOID* pVertexData;
    const D3DVERTEXELEMENT9* pVertexElements;
    UINT VertexSize;
    UINT VertexCount;

    const CHAR* strShaderFile;
};


//--------------------------------------------------------------------------------------
// Test data for each primitive type.
//--------------------------------------------------------------------------------------
TestPrimitive TestPrimitives[] =
{
    {
        D3DTPT_LINELIST, 1,
        LineSourceVertices, ColorVertexElements, sizeof( COLORVERTEX ), 2,
        "game:\\Media\\Shaders\\LineListVS.xvu"
    },

    {
        D3DTPT_LINESTRIP, 1,
        LineSourceVertices, ColorVertexElements, sizeof( COLORVERTEX ), 2,
        "game:\\Media\\Shaders\\LineListVS.xvu"
    },

    {
        D3DTPT_TRIANGLELIST, 1,
        TriSourceVertices, ColorVertexElements, sizeof( COLORVERTEX ), 3,
        "game:\\Media\\Shaders\\TriListVS.xvu"
    },

    {
        D3DTPT_TRIANGLEFAN, 2,
        QuadSourceVertices, ColorVertexElements, sizeof( COLORVERTEX ), 4,
        "game:\\Media\\Shaders\\TriListVS.xvu"
    },

    {
        D3DTPT_TRIANGLESTRIP, 2,
        TriStripSourceVertices, ColorVertexElements, sizeof( COLORVERTEX ), 4,
        "game:\\Media\\Shaders\\TriListVS.xvu"
    },

    {
        D3DTPT_QUADLIST, 1,
        QuadSourceVertices, ColorVertexElements, sizeof( COLORVERTEX ), 4,
        "game:\\Media\\Shaders\\QuadListVS.xvu"
    },

    {
        D3DTPT_LINEPATCH, 1,
        LineSourceVertices, LinePatchVertexElements, sizeof( COLORVERTEX ) * 2, 1,
        "game:\\Media\\Shaders\\LinePatchVS.xvu"
    },

    {
        D3DTPT_TRIPATCH, 1,
        TriPatchSourceVertices, TriPatchVertexElements, sizeof( COLORVERTEX ) * 3,
        1, "game:\\Media\\Shaders\\TriPatchVS.xvu"
    },

    {
        D3DTPT_QUADPATCH, 1,
        QuadPatchSourceVertices, QuadPatchVertexElements, sizeof( COLORVERTEX ) * 4,
        1, "game:\\Media\\Shaders\\QuadPatchVS.xvu"
    },
};

const UINT          NUM_PRIMITIVES = sizeof( TestPrimitives ) / sizeof( TestPrimitives[0] );


//--------------------------------------------------------------------------------------
// Structure to hold data for each test.
//--------------------------------------------------------------------------------------
struct TessellationTest
{
    const WCHAR* Name;
    D3DTESSELLATIONMODE Mode;
    D3DTESSPRIMITIVETYPE Primitive;
};


//--------------------------------------------------------------------------------------
// Data for each test.
//--------------------------------------------------------------------------------------
TessellationTest Tests[] =
{
    { L"Line List, Continuous Tess.\n",  D3DTM_CONTINUOUS, D3DTPT_LINELIST },
    { L"Line Strip, Continuous Tess.\n", D3DTM_CONTINUOUS, D3DTPT_LINESTRIP },
    { L"Tri List, Discrete Tess.\n",     D3DTM_DISCRETE,   D3DTPT_TRIANGLELIST },
    { L"Tri Fan, Discrete Tess.\n",      D3DTM_DISCRETE,   D3DTPT_TRIANGLEFAN },
    { L"Tri Strip, Discrete Tess.\n",    D3DTM_DISCRETE,   D3DTPT_TRIANGLESTRIP },
    { L"Tri List, Continuous Tess.\n",   D3DTM_CONTINUOUS, D3DTPT_TRIANGLELIST },
    { L"Tri Fan, Continuous Tess.\n",    D3DTM_CONTINUOUS, D3DTPT_TRIANGLEFAN },
    { L"Tri Strip, Continuous Tess.\n",  D3DTM_CONTINUOUS, D3DTPT_TRIANGLESTRIP },
    { L"Quad List, Continuous Tess.\n",  D3DTM_CONTINUOUS, D3DTPT_QUADLIST },
    { L"Line Patch, Continuous Tess.\n", D3DTM_CONTINUOUS, D3DTPT_LINEPATCH },
    { L"Tri Patch, Continuous Tess.\n",  D3DTM_CONTINUOUS, D3DTPT_TRIPATCH },
    { L"Quad Patch, Continuous Tess.\n", D3DTM_CONTINUOUS, D3DTPT_QUADPATCH },
    { L"Line Patch, Per-Edge Tess.\n",   D3DTM_PEREDGE,    D3DTPT_LINEPATCH },
    { L"Tri Patch, Per-Edge Tess.\n",    D3DTM_PEREDGE,    D3DTPT_TRIPATCH },
    { L"Quad Patch, Per-Edge Tess.\n",   D3DTM_PEREDGE,    D3DTPT_QUADPATCH },
};

const UINT          NUM_TESTS = sizeof( Tests ) / sizeof( Tests[0] );


//--------------------------------------------------------------------------------------
// Per-edge tessellation factors.
//--------------------------------------------------------------------------------------
FLOAT g_TessFactors[] =
    {
        2.0f, 4.0f, 6.0f, 8.0f
    };


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
    BOOL m_bDrawHelp;

    FLOAT m_fTessellationLevel;   // Max tessellation level.
    BOOL m_bIndexedPrimitives;   // TRUE for drawing indexed primitives.
    UINT m_CurrentTest;          // Current test settings.
    UINT m_PerEdgeSelMask;       // The current per-edge tess. factors selected

    XMMATRIX m_matWorld;
    XMMATRIX m_matView;
    XMMATRIX m_matProj;

    // Vertex declartaions, vertex buffers, and vertex shaders for each primitive type.
    IDirect3DVertexDeclaration9* m_pVertexDecl[NUM_PRIMITIVES];
    IDirect3DVertexBuffer9* m_pVB[NUM_PRIMITIVES];
    IDirect3DVertexShader9* m_pVertexShader[NUM_PRIMITIVES];

    // Simple pixel shader for rendering.
    IDirect3DPixelShader9* m_pPixelShader;

    // Index buffer for tessellation factors.
    IDirect3DIndexBuffer9* m_pTessFactorBuffer;

    // Index buffer filled with sequential indices.
    IDirect3DIndexBuffer9* m_pSeqIndexBuffer;

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

    // Make sure display is gamma correct.
    atgApp.m_d3dpp.BackBufferFormat =  ( D3DFORMAT )MAKESRGBFMT( D3DFMT_A8R8G8B8 );
    atgApp.m_d3dpp.FrontBufferFormat = ( D3DFORMAT )MAKESRGBFMT( D3DFMT_LE_X8R8G8B8 );

    atgApp.Run();
}


//--------------------------------------------------------------------------------------
// Name: UpdatePerEdgeFactors()
// Desc: Update the per-edge tessellation factors.
//--------------------------------------------------------------------------------------
HRESULT Sample::UpdatePerEdgeFactors()
{
    FLOAT* pTessFactors;
    if( FAILED( m_pTessFactorBuffer->Lock( 0, 0, ( VOID** )&pTessFactors, 0 ) ) )
        return E_FAIL;

    memcpy( pTessFactors, g_TessFactors, sizeof( g_TessFactors ) );

    m_pTessFactorBuffer->Unlock();

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Initialize()
// Desc: This creates all device-dependent display objects.
//--------------------------------------------------------------------------------------
HRESULT Sample::Initialize()
{
    // Create the font
    if( FAILED( m_Font.Create( "game:\\Media\\Fonts\\Arial_16.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Confine text drawing to the title safe area
    m_Font.SetWindow( ATG::GetTitleSafeArea() );

    // Create the help
    if( FAILED( m_Help.Create( "game:\\media\\help\\Help.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    m_bDrawHelp = FALSE;

    m_fTessellationLevel = 8.0f;
    m_bIndexedPrimitives = FALSE;
    m_CurrentTest = 2;
    m_PerEdgeSelMask = 0;

    //
    // Create the vertex declarations, vertex buffers, and vertex shaders for each
    // primtive type.
    //
    for( UINT i = 0; i < NUM_PRIMITIVES; i++ )
    {
        // Create the vertex declaration.
        m_pd3dDevice->CreateVertexDeclaration( TestPrimitives[i].pVertexElements,
                                               &m_pVertexDecl[i] );

        // Create the vertex buffer.
        UINT BufferSize = TestPrimitives[i].VertexCount * TestPrimitives[i].VertexSize;
        if( FAILED( m_pd3dDevice->CreateVertexBuffer( BufferSize,
                                                      D3DUSAGE_WRITEONLY,
                                                      NULL,
                                                      D3DPOOL_DEFAULT,
                                                      &m_pVB[i],
                                                      NULL ) ) )
            return E_FAIL;

        // Copy our source vertex data into the vertex buffer.
        VOID* pVertices;
        if( FAILED( m_pVB[i]->Lock( 0, 0, &pVertices, 0 ) ) )
            return E_FAIL;

        memcpy( pVertices, TestPrimitives[i].pVertexData, BufferSize );

        m_pVB[i]->Unlock();

        // Load the tessellation vertex shader.
        if( FAILED( ATG::LoadVertexShader( TestPrimitives[i].strShaderFile, &m_pVertexShader[i] ) ) )
        {
            ATG_PrintError( "Couldn't load %s\n", TestPrimitives[i].strShaderFile );
        }
    }

    // Load a simple pixel shader.
    if( FAILED( ATG::LoadPixelShader( "game:\\Media\\Shaders\\TestPS.xpu", &m_pPixelShader ) ) )
    {
        ATG_PrintError( "Couldn't load %s\n", "game:\\Media\\Shaders\\TestPS.xpu" );
        return E_FAIL;
    }

    // Create an index buffer to hold the tessellation factors. Use of an index buffer
    // with per-edge tessellation factors is only valid when drawing non-indexed patches
    // with D3DRS_TESSELLATIONMODEis set to D3DTM_PEREDGE.
    if( FAILED( m_pd3dDevice->CreateIndexBuffer( sizeof( g_TessFactors ),
                                                 D3DUSAGE_WRITEONLY,
                                                 D3DFMT_INDEX32,
                                                 D3DPOOL_DEFAULT,
                                                 &m_pTessFactorBuffer,
                                                 NULL ) ) )
        return E_FAIL;

    // Update the per-edge tessellation factors.
    UpdatePerEdgeFactors();

    // Sequentailly increasing index buffer.
    if( FAILED( m_pd3dDevice->CreateIndexBuffer( sizeof( WORD ) * 4,
                                                 D3DUSAGE_WRITEONLY,
                                                 D3DFMT_INDEX16,
                                                 D3DPOOL_DEFAULT,
                                                 &m_pSeqIndexBuffer,
                                                 NULL ) ) )
        return E_FAIL;

    WORD* pIndices;
    if( FAILED( m_pSeqIndexBuffer->Lock( 0, 0, ( VOID** )&pIndices, 0 ) ) )
        return E_FAIL;

    pIndices[0] = 0;
    pIndices[1] = 1;
    pIndices[2] = 2;
    pIndices[3] = 3;

    m_pSeqIndexBuffer->Unlock();

    // World matrix (identity in this sample)
    m_matWorld = XMMatrixIdentity();

    // View matrix
    XMVECTOR vEyePt = XMVectorSet( 0.0f, 0.0f, -5.0f, 0.0f );
    XMVECTOR vLookatPt = XMVectorSet( 0.0f, 0.0f, 0.0f, 0.0f );
    XMVECTOR vUp = XMVectorSet( 0.0f, 1.0f, 0.0f, 0.0f );
    m_matView = XMMatrixLookAtLH( vEyePt, vLookatPt, vUp );

    // Determine the aspect ratio
    XVIDEO_MODE VideoMode;
    ZeroMemory( &VideoMode, sizeof( VideoMode ) );
    XGetVideoMode( &VideoMode );

    FLOAT fAspect = VideoMode.fIsWideScreen ? ( 16.0f / 9.0f ) : ( 4.0f / 3.0f );

    // Projection matrix
    m_matProj = XMMatrixPerspectiveFovLH( XM_PI / 4, fAspect, 1.0f, 200.0f );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Update()
// Desc: Called once per frame, the call is the entry point for animating
//       the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Update()
{
    // Get the current gamepad status
    ATG::GAMEPAD* pGamepad = ATG::Input::GetMergedInput();

    // Toggle help
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_BACK )
        m_bDrawHelp = !m_bDrawHelp;

    // Next test.
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_A )
        m_CurrentTest = ( m_CurrentTest + 1 ) % NUM_TESTS;

    // Previous test.
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_B )
    {
        if( m_CurrentTest > 0 )
            m_CurrentTest = ( m_CurrentTest - 1 );
        else
            m_CurrentTest = NUM_TESTS - 1;
    }

    // Toggle index / non-index primitives.
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_X )
        m_bIndexedPrimitives = !m_bIndexedPrimitives;

    if( Tests[m_CurrentTest].Mode == D3DTM_PEREDGE )
    {
        m_PerEdgeSelMask = ( UINT( pGamepad->fY2 < -0.5 ) << 2 ) |
            ( UINT( pGamepad->fY2 > 0.5 ) << 0 ) |
            ( UINT( pGamepad->fX2 < -0.5 ) << 3 ) |
            ( UINT( pGamepad->fX2 > 0.5 ) << 1 );

    }
    else
    {
        m_PerEdgeSelMask = 0;
    }

    if( m_PerEdgeSelMask )
    {
        // Adjust the per-edge tessellation factors.
        for( UINT i = 0; i < 4; i++ )
        {
            if( m_PerEdgeSelMask & ( 1 << i ) )
            {
                if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_DPAD_UP )
                    g_TessFactors[i] += 1.0f;
                else if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_DPAD_DOWN )
                    g_TessFactors[i] -= 1.0f;

                if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_DPAD_LEFT )
                    g_TessFactors[i] += 0.1f;
                else if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_DPAD_RIGHT )
                    g_TessFactors[i] -= 0.1f;
            }
        }

        UpdatePerEdgeFactors();
    }
    else
    {
        // Adjust the tessellation level.
        if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_DPAD_UP )
            m_fTessellationLevel += 1.0f;
        else if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_DPAD_DOWN )
            m_fTessellationLevel -= 1.0f;

        if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_DPAD_LEFT )
            m_fTessellationLevel += 0.1f;
        else if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_DPAD_RIGHT )
            m_fTessellationLevel -= 0.1f;

        // Clamp tessellation level to be a legal value.
        FLOAT fMaxLevel = ( ( Tests[m_CurrentTest].Mode == D3DTM_DISCRETE ) ? 14.0f : 15.0f );

        if( m_fTessellationLevel > fMaxLevel )
            m_fTessellationLevel = fMaxLevel;
        else if( m_fTessellationLevel < 1.0f )
            m_fTessellationLevel = 1.0f;
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

    const D3DTESSPRIMITIVETYPE Primitive = Tests[m_CurrentTest].Primitive;

    // Find the index of the primitive data for the current test.
    INT PrimDataIndex = 0;
    while( PrimDataIndex < NUM_PRIMITIVES )
    {
        if( Primitive == TestPrimitives[PrimDataIndex].Primitive )
            break;

        PrimDataIndex++;
    }

    assert( PrimDataIndex < NUM_PRIMITIVES );

    // Set the tessellation level.
    m_pd3dDevice->SetRenderState( D3DRS_MAXTESSELLATIONLEVEL,
                                  ATG::FtoDW( m_fTessellationLevel ) );

    // Set the tessellation mode.
    m_pd3dDevice->SetRenderState( D3DRS_TESSELLATIONMODE,
                                  Tests[m_CurrentTest].Mode );

    // Wireframe for debugging.
    m_pd3dDevice->SetRenderState( D3DRS_FILLMODE, D3DFILL_WIREFRAME );

    // Set the shaders.
    m_pd3dDevice->SetVertexShader( m_pVertexShader[PrimDataIndex] );
    m_pd3dDevice->SetPixelShader( m_pPixelShader );

    // Set the vertex declaration.
    m_pd3dDevice->SetVertexDeclaration( m_pVertexDecl[PrimDataIndex] );

    // Set the vertex buffer.
    m_pd3dDevice->SetStreamSource( 0, m_pVB[PrimDataIndex], 0,
                                   TestPrimitives[PrimDataIndex].VertexSize );

    // Set the composite world*view*projection matrix.
    XMMATRIX matWVP = m_matWorld * m_matView * m_matProj;
    m_pd3dDevice->SetVertexShaderConstantF( 0, ( FLOAT* )&matWVP, 4 );

    UINT PrimitiveCount = TestPrimitives[PrimDataIndex].PrimitiveCount;

    // Render the patch data.
    if( Tests[m_CurrentTest].Mode == D3DTM_PEREDGE )
    {
        // Set the tessellation factor buffer.
        m_pd3dDevice->SetIndices( m_pTessFactorBuffer );

        m_pd3dDevice->DrawIndexedTessellatedPrimitive( Primitive, 0, 0,
                                                       PrimitiveCount );
    }
    else
    {
        if( m_bIndexedPrimitives )
        {
            m_pd3dDevice->SetIndices( m_pSeqIndexBuffer );

            m_pd3dDevice->DrawIndexedTessellatedPrimitive( Primitive, 0, 0,
                                                           PrimitiveCount );
        }
        else
        {
            m_pd3dDevice->DrawTessellatedPrimitive( Primitive, 0, PrimitiveCount );
        }
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
        m_Font.DrawText( 0, 0, 0xffffffff, L"SimpleTessellation" );
        m_Font.SetScaleFactors( 1.0f, 1.0f );
        m_Font.DrawText( 0, 0, 0xffffff00, m_Timer.GetFrameRate(), ATGFONT_RIGHT );

        m_Font.DrawText( 0, 30, 0xffffffff, Tests[m_CurrentTest].Name );

        const WCHAR* IndexedModeNames[] = { L"Non-Indexed", L"Indexed" };
        m_Font.DrawText( 0xffffffff, IndexedModeNames[m_bIndexedPrimitives ? 1 : 0] );

        if( Tests[m_CurrentTest].Mode == D3DTM_PEREDGE )
        {
            WCHAR str[64];

            swprintf_s( str, L"Max Tess. Level: %0.01f\n", m_fTessellationLevel );

            m_Font.DrawText( 0, 30, 0xffffffff, str, ATGFONT_RIGHT );

            FLOAT x = 0.0f;

            for( INT i = 3; i >= 0; i-- )
            {
                DWORD dwColor = 0xffffffff;

                if( m_PerEdgeSelMask & ( 1 << i ) )
                    dwColor = 0xffff0000;

                swprintf_s( str, ( i < 3 ) ? L"%0.01f, " : L"%0.01f", g_TessFactors[i] );

                m_Font.DrawText( x, 52, dwColor, str, ATGFONT_RIGHT );

                x -= m_Font.GetTextWidth( str );
            }

            m_Font.DrawText( x, 52, 0xffffffff, L"Tess. Factors: ", ATGFONT_RIGHT );
        }
        else
        {
            WCHAR str[32];
            swprintf_s( str, L"Tess. Level: %0.01f", m_fTessellationLevel );
            m_Font.DrawText( 0, 30, 0xffffffff, str, ATGFONT_RIGHT );
        }

        m_Font.End();
    }

    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}

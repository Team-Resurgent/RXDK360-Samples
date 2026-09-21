//--------------------------------------------------------------------------------------
// Strip.cpp
//
// Sample to show off tri-stripping performance results. This sample
// creates a mesh, stripifies it, then displays several copies of it
// along with performance data.
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include <xtl.h>
#include <xgraphics.h>
#include <AtgApp.h>
#include <AtgFont.h>
#include <AtgHelp.h>
#include <AtgInput.h>
#include <AtgMesh.h>
#include <AtgResource.h>
#include <AtgUtil.h>
#include "AtgStrip.h"

// Disable warning C6211: Leaking memory <pointer> due to an exception.
// We "know" that we won't run out of memory in this sample
#pragma warning ( disable : 6211 )


//--------------------------------------------------------------------------------------
// Help support
//--------------------------------------------------------------------------------------
ATG::HELP_CALLOUT g_HelpCallouts[] =
{
    { ATG::HELP_A_BUTTON,     ATG::HELP_PLACEMENT_2, L"Toggle\nmesh type" },
    { ATG::HELP_LEFTSTICK,    ATG::HELP_PLACEMENT_2, L"Rotate\nmodel" },
    { ATG::HELP_BACK_BUTTON,  ATG::HELP_PLACEMENT_2, L"Display\nhelp" },
};
#define MAX_HELP_CALLOUTS (sizeof(g_HelpCallouts)/sizeof(g_HelpCallouts[0]))


//--------------------------------------------------------------------------------------
// Global variables and definitions
//--------------------------------------------------------------------------------------

// Constants definitions
#define COLOR_WHITE                 0xffffffff
#define COLOR_YELLOW                0xffffff00
#define COLOR_CYAN                  0xff00ffff


// Structure declarations
struct MODELVERT
{
    XMFLOAT3 p;
    XMFLOAT3 n;
    FLOAT tu, tv;
};


struct MODELDATA
{
    D3DMATRIX* pMatrix;
    DWORD dwNumVertices;
    MODELVERT* pVertices;
    DWORD dwNumIndices;
    WORD* pIndices;
};


enum MESHTYPE
{
    MESHTYPE_ORIGINAL,
    MESHTYPE_TRISTRIPPED
};


struct ROBOTSTATS
{
    DWORD dwNumVertices;
    DWORD dwCacheHits;
    DWORD dwPagesCrossed;
    DWORD dwDegenerateTris;

    double fdAvgTriPerSec;
    double fdRealAvgTriPerSec;
    double fdMaxTriPerSec;
    double fdMinTriPerSec;
    DWORD dwAvgCount;
    DWORD dwTriCount;
    DWORD dwIndCount;
    DWORD dwTime;
};

struct MESHINFO
{
    D3DPRIMITIVETYPE dwPrimType;   // primitive type

    DWORD dwIndexCount; // index count
    WORD* pwIndices;    // index list
    D3DIndexBuffer* pIndexBuffer; // dx9 index buffer

    DWORD dwNumVertices; // num verts
    D3DVertexBuffer* pVertexBuffer; // vbs

    DWORD dwPrimitiveCount;

    DWORD dwDegenerateTris;
    DWORD dwCacheHits;
    DWORD dwPagesCrossed;
};


// Render a 3 x 4 grid of robots
static const int                C_ROBOTS_X = 4;
static const int                C_ROBOTS_Y = 3;

// Mesh data from MODELDATA.CPP
extern DWORD                    g_cModelData;
extern MODELDATA g_ModelData[];

D3DTexture*                     g_pTexture = NULL;
D3DCubeTexture*                 g_pEnvMap = NULL;

LPDIRECT3DVERTEXDECLARATION9    g_pVertexDeclaration;
LPDIRECT3DVERTEXSHADER9         g_pVertexShader;
LPDIRECT3DPIXELSHADER9          g_pPixelShader;

XMMATRIX                        g_matWorld;
XMMATRIX                        g_matView;
XMMATRIX                        g_matProj;


//--------------------------------------------------------------------------------------
// Name: class CRobotGeometry
// Desc: Class to render geometry for a Robot
//--------------------------------------------------------------------------------------
class CRobotGeometry
{
    DWORD m_dwNumMeshes;  // Count of VBs to draw
    MESHINFO* m_pMeshes;      // Our list of VBs

public:
    DWORD m_dwVertexSize; // Vertex size

    HRESULT Init( D3DDevice*, MESHTYPE );
    VOID    Release();

    HRESULT SetStates( D3DDevice* );
    HRESULT RestoreStates( D3DDevice* );
    HRESULT Render( D3DDevice*, ROBOTSTATS* = NULL );
    VOID SaveMesh( D3DDevice* );

            CRobotGeometry();
            ~CRobotGeometry()
            {
                Release();
            }
};


//--------------------------------------------------------------------------------------
// Name: class Sample
// Desc: Application class.
//--------------------------------------------------------------------------------------
class Sample : public ATG::Application
{
    ATG::Timer m_Timer;
    ATG::Font m_Font;
    ATG::PackedResource m_Resource;
    ATG::Help m_Help;
    BOOL m_bDrawHelp;

    // The robot object
    ROBOTSTATS m_Stats;
    CRobotGeometry m_OriginalRobot;     // The original robot geometry
    CRobotGeometry m_TriStrippedRobot;  // The tri-stripped version of the robot

    CRobotGeometry* m_pRobot;            // Which version of the robot to render
    BOOL m_bUseTriStrippedMesh;

    VOID    DisplayArgs();

public:
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
// Desc: Initialize all dependencies and states
//--------------------------------------------------------------------------------------
HRESULT Sample::Initialize()
{
    HRESULT hr;

    m_bDrawHelp = FALSE;

    ZeroMemory( &m_Stats, sizeof( m_Stats ) );
    m_Stats.dwAvgCount = 0;
    m_Stats.fdAvgTriPerSec = 0.0;
    m_Stats.fdRealAvgTriPerSec = 0.0;
    m_Stats.fdMaxTriPerSec = 0.0;
    m_Stats.fdMinTriPerSec = 1e99;

    m_bUseTriStrippedMesh = TRUE; // Start with tri-strips
    m_pRobot = &m_TriStrippedRobot;

    // Create the font
    if( FAILED( hr = m_Font.Create( "game:\\Media\\Fonts\\Arial_16.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Confine text drawing to the title safe area
    m_Font.SetWindow( ATG::GetTitleSafeArea() );

    // Create the help subsystem
    if( FAILED( hr = m_Help.Create( "game:\\Media\\Help\\Help.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Create the textures resource
    if( FAILED( m_Resource.Create( "game:\\Media\\Resource.xpr" ) ) )
    {
        ATG::DebugSpew( "Couldn't create Resource.xpr\n" );
        return ATGAPPERR_MEDIANOTFOUND;
    }

    g_pTexture = m_Resource.GetTexture( "Robot" );
    g_pEnvMap = m_Resource.GetCubemap( "EnvMap" );

    // Create vertex shader
    static const D3DVERTEXELEMENT9 decl[] =
    {
        { 0,  0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT,  D3DDECLUSAGE_POSITION, 0 },
        { 0, 12, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT,  D3DDECLUSAGE_NORMAL,   0 },
        { 0, 24, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT,  D3DDECLUSAGE_TEXCOORD, 0 },
        D3DDECL_END()
    };

    if( FAILED( hr = m_pd3dDevice->CreateVertexDeclaration( decl, &g_pVertexDeclaration ) ) )
        return hr;

    VOID* pCode = NULL;
    if( FAILED( hr = ATG::LoadFile( "game:\\Media\\Shaders\\TriStripVS.xvu", &pCode ) ) )
        return hr;
    if( FAILED( hr = m_pd3dDevice->CreateVertexShader( ( DWORD* )pCode, &g_pVertexShader ) ) )
    {
        ATG::UnloadFile( pCode );
        return hr;
    }
    ATG::UnloadFile( pCode );

    pCode = NULL;
    if( FAILED( hr = ATG::LoadFile( "game:\\Media\\Shaders\\TriStripPS.xpu", &pCode ) ) )
        return hr;
    if( FAILED( hr = m_pd3dDevice->CreatePixelShader( ( DWORD* )pCode, &g_pPixelShader ) ) )
    {
        ATG::UnloadFile( pCode );
        return hr;
    }
    ATG::UnloadFile( pCode );

    // Display initial wait screen
    m_pd3dDevice->Clear( 0, NULL, D3DCLEAR_TARGET, 0xff0000ff, 1.0f, 0L );
    m_Font.SetScaleFactors( 1.2f, 1.2f );
    m_Font.DrawText( 0, 0, 0xffffffff, L"TriStrip" );
    m_Font.SetScaleFactors( 1.0f, 1.0f );
    m_Font.DrawText( 16, 40, COLOR_CYAN, L"Calculating tri-strips. Please wait..." );
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    // Initialize robot data
    hr = m_OriginalRobot.Init( m_pd3dDevice, MESHTYPE_ORIGINAL );
    if( FAILED( hr ) )
        return hr;
    hr = m_TriStrippedRobot.Init( m_pd3dDevice, MESHTYPE_TRISTRIPPED );
    if( FAILED( hr ) )
        return hr;

    // Determine the aspect ratio
    FLOAT fAspectRatio = ( FLOAT )m_d3dpp.BackBufferWidth / ( FLOAT )m_d3dpp.BackBufferHeight;

    // Set the projection matrix
    g_matProj = XMMatrixPerspectiveFovLH( XM_PI / 4, fAspectRatio, 1.0f, 200.0f );

    // Set the view matrix
    XMVECTOR vEyePt = XMVectorSet( 0.0f, 0.0f, -10.0f, 0.0f );
    XMVECTOR vLookatPt = XMVectorSet( 0.0f, 0.0f, 0.0f, 0.0f );
    XMVECTOR vUp = XMVectorSet( 0.0f, 1.0f, 0.0f, 0.0f );
    g_matView = XMMatrixLookAtLH( vEyePt, vLookatPt, vUp );

    // Initial world matrix
    g_matWorld = XMMatrixIdentity();

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Update()
// Desc: Called once per frame, the call is the entry point for animating the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Update()
{
    FLOAT m_fElapsedTime = ( FLOAT )m_Timer.GetElapsedTime();

    // Get the current gamepad status
    ATG::GAMEPAD* pGamepad = ATG::Input::GetMergedInput();

    // Toggle help
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_BACK )
        m_bDrawHelp = !m_bDrawHelp;

    // Handle the A button
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_A )
    {
        // Toggle between tri-stripped mesh and the original mesh
        m_bUseTriStrippedMesh = !m_bUseTriStrippedMesh;

        // Make sure we point to the right robot mesh to render
        if( m_bUseTriStrippedMesh )
            m_pRobot = &m_TriStrippedRobot;
        else
            m_pRobot = &m_OriginalRobot;

        // Clear the stats
        m_Stats.dwAvgCount = 0;
        m_Stats.fdAvgTriPerSec = 0.0;
        m_Stats.fdRealAvgTriPerSec = 0.0;
        m_Stats.fdMaxTriPerSec = 0.0;
        m_Stats.fdMinTriPerSec = 1e99;
    }

    // Perform object rotation
    XMMATRIX matRotate;
    FLOAT fXRotate1 = pGamepad->fX1 * m_fElapsedTime * XM_PI * 0.5f;
    FLOAT fYRotate1 = pGamepad->fY1 * m_fElapsedTime * XM_PI * 0.5f;
    matRotate = XMMatrixRotationRollPitchYaw( -fYRotate1, -fXRotate1, 0.0f );
    g_matWorld = XMMatrixMultiply( g_matWorld, matRotate );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Render()
// Desc: This function sets up render states, clears the viewport, and renders
//       the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Render()
{
    static WCHAR s_strStats[MAX_PATH] = L"Running...";

    // Clear the viewport
    m_pd3dDevice->Clear( 0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, 0xff0000ff,
                         1.0f, 0L );

    // Clear the geometry stats
    m_Stats.dwTriCount = 0;
    m_Stats.dwIndCount = 0;
    m_Stats.dwNumVertices = 0;
    m_Stats.dwCacheHits = 0;
    m_Stats.dwPagesCrossed = 0;
    m_Stats.dwDegenerateTris = 0;

    DWORD dwStartTime = GetTickCount();

    // Render 100 robots
    m_pRobot->SetStates( m_pd3dDevice );
    for( DWORD i = 0; i < 100; i++ )
    {
        m_pRobot->Render( m_pd3dDevice, &m_Stats );
    }
    m_pRobot->RestoreStates( m_pd3dDevice );

    DWORD dwStopTime = GetTickCount();

    // Update the timing stats
    FLOAT fElapsedTime = 0.001f * ( dwStopTime - dwStartTime );
    FLOAT tps = 1e-6f * ( m_Stats.dwTriCount ) / fElapsedTime;
    FLOAT sps = 1e-6f * ( m_Stats.dwIndCount ) / fElapsedTime;
    swprintf_s( s_strStats, L"%6.1f MTri/s,   %6.1f MVerts/s", tps, sps );

    m_Stats.dwAvgCount++;
    m_Stats.fdAvgTriPerSec += tps;
    m_Stats.fdRealAvgTriPerSec += 1e-6f * ( m_Stats.dwTriCount - m_Stats.dwDegenerateTris ) / fElapsedTime;
    m_Stats.fdMaxTriPerSec = max( tps, m_Stats.fdMaxTriPerSec );
    m_Stats.fdMinTriPerSec = min( tps, m_Stats.fdMinTriPerSec );
    m_Stats.dwTime = dwStopTime - dwStartTime;

    // Draw the help or main screen text output
    m_Timer.MarkFrame();
    if( m_bDrawHelp )
    {
        m_Help.Render( &m_Font, g_HelpCallouts, MAX_HELP_CALLOUTS );
    }
    else
    {
        m_Font.Begin();

        // Draw title and frame rate
        m_Font.SetScaleFactors( 1.2f, 1.2f );
        m_Font.DrawText( 0, 0, 0xffffffff, L"TriStrip" );
        m_Font.SetScaleFactors( 1.0f, 1.0f );
        m_Font.DrawText( 0, 0, 0xffffff00, m_Timer.GetFrameRate(), ATGFONT_RIGHT );

        // Draw stats
        m_Font.DrawText( 16, 40, COLOR_CYAN, s_strStats );

        // Display args
        DisplayArgs();

        m_Font.End();
    }

    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: DisplayArgs()
// Desc: Display the various stats and options.
//--------------------------------------------------------------------------------------
VOID Sample::DisplayArgs()
{
    WCHAR str[128];
    FLOAT fX[4] = { 16.0f, 252.0f, 272.0f, 528.0f };
    FLOAT fY = 120.0f;
    FLOAT fHeight = ( FLOAT )m_Font.GetFontHeight();

    // Draw left-hand column
    fY = 84.0f;

    static const WCHAR* rgMeshType[2] = { L"Original", L"Tri-stripped" };
    swprintf_s( str, L"%s", rgMeshType[m_bUseTriStrippedMesh] );
    m_Font.DrawText( fX[0], fY, COLOR_WHITE, L"Mesh type:" );
    m_Font.DrawText( fX[1], fY, COLOR_YELLOW, str, ATGFONT_RIGHT );
    fY += fHeight;

    swprintf_s( str, L"%d", m_Stats.dwTriCount );
    m_Font.DrawText( fX[0], fY, COLOR_WHITE, L"# triangles:" );
    m_Font.DrawText( fX[1], fY, COLOR_YELLOW, str, ATGFONT_RIGHT );
    fY += fHeight;

    swprintf_s( str, L"%d", m_Stats.dwDegenerateTris );
    m_Font.DrawText( fX[0], fY, COLOR_WHITE, L"# degenerate:" );
    m_Font.DrawText( fX[1], fY, COLOR_YELLOW, str, ATGFONT_RIGHT );
    fY += fHeight;

    swprintf_s( str, L"%d", m_Stats.dwNumVertices );
    m_Font.DrawText( fX[0], fY, COLOR_WHITE, L"# vertices:" );
    m_Font.DrawText( fX[1], fY, COLOR_YELLOW, str, ATGFONT_RIGHT );
    fY += fHeight;

    swprintf_s( str, L"%d", m_Stats.dwIndCount );
    m_Font.DrawText( fX[0], fY, COLOR_WHITE, L"# indices:" );
    m_Font.DrawText( fX[1], fY, COLOR_YELLOW, str, ATGFONT_RIGHT );
    fY += fHeight;

    // Draw right-hand column
    fY = 84.0f;

    swprintf_s( str, L"%8.3f", m_Stats.fdMaxTriPerSec );
    m_Font.DrawText( fX[2], fY, COLOR_WHITE, L"Max tri/s" );
    m_Font.DrawText( fX[3], fY, COLOR_YELLOW, str, ATGFONT_RIGHT );
    fY += fHeight;

    swprintf_s( str, L"%8.3f", m_Stats.fdMinTriPerSec );
    m_Font.DrawText( fX[2], fY, COLOR_WHITE, L"Min tri/s" );
    m_Font.DrawText( fX[3], fY, COLOR_YELLOW, str, ATGFONT_RIGHT );
    fY += fHeight;

    swprintf_s( str, L"%8.3f", m_Stats.fdAvgTriPerSec / m_Stats.dwAvgCount );
    m_Font.DrawText( fX[2], fY, COLOR_WHITE, L"Avg tri/s" );
    m_Font.DrawText( fX[3], fY, COLOR_YELLOW, str, ATGFONT_RIGHT );
    fY += fHeight;

    swprintf_s( str, L"%8.3f", m_Stats.fdRealAvgTriPerSec / m_Stats.dwAvgCount );
    m_Font.DrawText( fX[2], fY, COLOR_WHITE, L"Real avg tri/s:" );
    m_Font.DrawText( fX[3], fY, COLOR_YELLOW, str, ATGFONT_RIGHT );
    fY += fHeight;

    swprintf_s( str, L"%d", m_Stats.dwCacheHits );
    m_Font.DrawText( fX[2], fY, COLOR_WHITE, L"GPU cache hits:" );
    m_Font.DrawText( fX[3], fY, COLOR_YELLOW, str, ATGFONT_RIGHT );
    fY += fHeight;

    DWORD vbpages = ( m_Stats.dwNumVertices + m_Stats.dwNumVertices * m_pRobot->m_dwVertexSize ) / ( 1024 * 4 );
    swprintf_s( str, L"%d", vbpages );
    m_Font.DrawText( fX[2], fY, COLOR_WHITE, L"VB pages:" );
    m_Font.DrawText( fX[3], fY, COLOR_YELLOW, str, ATGFONT_RIGHT );
    fY += fHeight;

    swprintf_s( str, L"%d", m_Stats.dwPagesCrossed );
    m_Font.DrawText( fX[2], fY, COLOR_WHITE, L"Pages crossed:" );
    m_Font.DrawText( fX[3], fY, COLOR_YELLOW, str, ATGFONT_RIGHT );
    fY += fHeight;

    swprintf_s( str, L"%d ms", m_Stats.dwTime );
    m_Font.DrawText( fX[2], fY, COLOR_WHITE, L"Time/frame:" );
    m_Font.DrawText( fX[3], fY, COLOR_YELLOW, str, ATGFONT_RIGHT );
    fY += fHeight;
}


//--------------------------------------------------------------------------------------
// Name: CRobotGeometry()
// Desc: Constructor
//--------------------------------------------------------------------------------------
CRobotGeometry::CRobotGeometry()
{
    m_dwNumMeshes = 0;
    m_pMeshes = NULL;
}


//--------------------------------------------------------------------------------------
// Name: Release()
// Desc: Release allocated objects
//--------------------------------------------------------------------------------------
VOID CRobotGeometry::Release()
{
    for( DWORD i = 0; i < m_dwNumMeshes; i++ )
    {
        m_pMeshes[i].pIndexBuffer->Release();
        m_pMeshes[i].pVertexBuffer->Release();
    }

    delete[] m_pMeshes;
    m_dwNumMeshes = 0;
}


//--------------------------------------------------------------------------------------
// Name: Init()
// Desc: Initialize robot dependencies
//--------------------------------------------------------------------------------------
HRESULT CRobotGeometry::Init( LPDIRECT3DDEVICE9 pd3dDevice, MESHTYPE MeshType )
{
    // Release any previously allocated meshes
    for( DWORD i = 0; i < m_dwNumMeshes; i++ )
    {
        m_pMeshes[i].pIndexBuffer->Release();
        m_pMeshes[i].pVertexBuffer->Release();
    }
    delete[] m_pMeshes;
    m_dwNumMeshes = 0;

    m_dwVertexSize = sizeof( MODELVERT );

    // Initialize new mesh array
    m_dwNumMeshes = g_cModelData;
    m_pMeshes = new MESHINFO[m_dwNumMeshes];
    ZeroMemory( m_pMeshes, m_dwNumMeshes * sizeof( MESHINFO ) );

    for( DWORD i = 0; i < m_dwNumMeshes; i++ )
    {
        MODELDATA* pModelData = &g_ModelData[i];
        MESHINFO* pMesh = &m_pMeshes[i];

        if( MeshType == MESHTYPE_TRISTRIPPED )
        {
            DWORD dwNumVertices = pModelData->dwNumVertices;
            DWORD dwStrippedIndexCount;   // Tristrip count
            WORD* pwStrippedIndices;      // Tristrip indices
            WORD* pwVertexPermutation;    // Array for sorting

            // Run the tri-list through our tri-stripper
            TriStripper::MakeStrips( pModelData->dwNumIndices / 3, pModelData->pIndices,
                                     &dwStrippedIndexCount, &pwStrippedIndices,
                                     ATGTRISTRIPPER_USE_RESET_INDEX );

            // Sort the vertices...
            TriStripper::ComputeVertexPermutation( dwStrippedIndexCount, pwStrippedIndices,
                                                   dwNumVertices, &pwVertexPermutation );

            // Create a vertex buffer
            pd3dDevice->CreateVertexBuffer( dwNumVertices * m_dwVertexSize,
                                            D3DUSAGE_WRITEONLY, 0,
                                            D3DPOOL_DEFAULT, &pMesh->pVertexBuffer, NULL );

            // Lock and fill the vertex buffer, remapping vertices through the
            // vertex permutation array.
            MODELVERT* pVertices;
            pMesh->pVertexBuffer->Lock( 0, 0, ( VOID** )&pVertices, 0 );
            for( DWORD j = 0; j < dwNumVertices; j++ )
                pVertices[j] = pModelData->pVertices[pwVertexPermutation[j]];
            pMesh->pVertexBuffer->Unlock();

            // Free the array allocated by the ComputeVertexPermutation() call
            delete[] pwVertexPermutation;

            // Create an index buffer for using DrawIndexedPrimitive.
            pd3dDevice->CreateIndexBuffer( dwStrippedIndexCount * sizeof( WORD ),
                                           D3DUSAGE_WRITEONLY, D3DFMT_INDEX16,
                                           D3DPOOL_DEFAULT, &pMesh->pIndexBuffer, NULL );

            // Lock and fill the index buffer
            WORD* pIndices;
            pMesh->pIndexBuffer->Lock( 0, 0, ( VOID** )&pIndices, 0 );
            memcpy( pIndices, pwStrippedIndices, dwStrippedIndexCount * sizeof( WORD ) );
            pMesh->pIndexBuffer->Unlock();

            // Free the array allocated by the MakeStrips() call
            delete [] pwStrippedIndices;

            // Save info for rendering the mesh
            pMesh->dwNumVertices = pModelData->dwNumVertices;
            pMesh->pwIndices = pIndices;
            pMesh->dwPrimType = D3DPT_TRIANGLESTRIP;
            pMesh->dwPrimitiveCount = dwStrippedIndexCount - 2;
            pMesh->dwIndexCount = dwStrippedIndexCount;
        }
        else // MeshType != MESHTYPE_TRISTRIPPED
        {
            // Create a vertex buffer
            pd3dDevice->CreateVertexBuffer( pModelData->dwNumVertices * m_dwVertexSize,
                                            D3DUSAGE_WRITEONLY, 0,
                                            D3DPOOL_DEFAULT, &pMesh->pVertexBuffer, NULL );

            // Lock and fill the vertex buffer
            MODELVERT* pVertices;
            pMesh->pVertexBuffer->Lock( 0, 0, ( VOID** )&pVertices, 0 );
            memcpy( pVertices, pModelData->pVertices, pModelData->dwNumVertices * m_dwVertexSize );
            pMesh->pVertexBuffer->Unlock();

            // Create an index buffer for using DrawIndexedPrimitive.
            pd3dDevice->CreateIndexBuffer( pModelData->dwNumIndices * sizeof( WORD ),
                                           D3DUSAGE_WRITEONLY, D3DFMT_INDEX16,
                                           D3DPOOL_DEFAULT, &pMesh->pIndexBuffer, NULL );

            // Lock and fill the index buffer
            WORD* pIndices;
            pMesh->pIndexBuffer->Lock( 0, 0, ( VOID** )&pIndices, 0 );
            memcpy( pIndices, pModelData->pIndices, pModelData->dwNumIndices * sizeof( WORD ) );
            pMesh->pIndexBuffer->Unlock();

            pMesh->dwNumVertices = pModelData->dwNumVertices;
            pMesh->pwIndices = pIndices;
            pMesh->dwPrimType = D3DPT_TRIANGLELIST;
            pMesh->dwPrimitiveCount = pModelData->dwNumIndices / 3;
            pMesh->dwIndexCount = pModelData->dwNumIndices;
        }

        // Figure out how many degenerate triangles and cache hits we've got
        TriStripper::CalcCacheHits( pMesh->dwPrimType, m_dwVertexSize, pMesh->pwIndices,
                                    pMesh->dwIndexCount, &pMesh->dwDegenerateTris,
                                    &pMesh->dwCacheHits, &pMesh->dwPagesCrossed );
    }

    // Transform the model to a better coordinate system
    static BOOL bFirstTime = TRUE;

    if( bFirstTime )
    {
        bFirstTime = FALSE;

        XMFLOAT3 vCenter;
        vCenter.x = g_ModelData[0x2c].pMatrix->_41;
        vCenter.y = g_ModelData[0x2c].pMatrix->_42;
        vCenter.z = g_ModelData[0x2c].pMatrix->_43;

        XMMATRIX matRotateX = XMMatrixRotationX( -XM_PI / 2 );
        XMMATRIX matRotateY = XMMatrixRotationY( +XM_PI );

        for( DWORD i = 0; i < m_dwNumMeshes; i++ )
        {
            g_ModelData[i].pMatrix->_41 -= vCenter.x;
            g_ModelData[i].pMatrix->_42 -= vCenter.y;
            g_ModelData[i].pMatrix->_43 -= vCenter.z;

            XMMATRIX mat = XMLoadFloat4x4( ( XMFLOAT4X4* )g_ModelData[i].pMatrix );
            mat = XMMatrixMultiply( mat, matRotateX );
            mat = XMMatrixMultiply( mat, matRotateY );
            XMStoreFloat4x4( ( XMFLOAT4X4* )g_ModelData[i].pMatrix, mat );
        }
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: SetStates()
// Desc: Set states that will be used to render the scene
//--------------------------------------------------------------------------------------
HRESULT CRobotGeometry::SetStates( LPDIRECT3DDEVICE9 m_pd3dDevice )
{
    // Transform incoming camera space reflection vectors to world space
    XMVECTOR vDeterminant;
    XMMATRIX matTex = g_matView;
    matTex.m[3][0] = 0.0f;
    matTex.m[3][1] = 0.0f;
    matTex.m[3][2] = 0.0f;
    matTex = XMMatrixInverse( &vDeterminant, matTex );
    matTex = XMMatrixTranspose( matTex );
    m_pd3dDevice->SetVertexShaderConstantF( 8, ( FLOAT* )&matTex, 4 );

    m_pd3dDevice->SetTexture( 0, g_pTexture );
    m_pd3dDevice->SetTexture( 1, g_pEnvMap );

    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetSamplerState( 1, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetSamplerState( 1, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetSamplerState( 1, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR );

    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_CCW );
    m_pd3dDevice->SetRenderState( D3DRS_FILLMODE, D3DFILL_SOLID );
    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_ALPHATESTENABLE, FALSE );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: RestoreStates()
// Desc: Restore all used states
//--------------------------------------------------------------------------------------
HRESULT CRobotGeometry::RestoreStates( LPDIRECT3DDEVICE9 m_pd3dDevice )
{
    m_pd3dDevice->SetTexture( 0, NULL );
    m_pd3dDevice->SetTexture( 1, NULL );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: SaveMesh()
// Desc: Saves the mesh to disk
//--------------------------------------------------------------------------------------
VOID CRobotGeometry::SaveMesh( LPDIRECT3DDEVICE9 m_pd3dDevice )
{
    ATG::Mesh RobotMesh;
    RobotMesh.m_dwNumFrames = m_dwNumMeshes;
    RobotMesh.m_pFrames = new ATG::MESH_FRAME[ RobotMesh.m_dwNumFrames ];

    static const D3DVERTEXELEMENT9 VertexElements[] =
    {
        { 0,  0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION,  0 },
        { 0, 12, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_NORMAL,    0 },
        { 0, 24, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD,  0 },
        D3DDECL_END()
    };

    for( DWORD i = 0; i < m_dwNumMeshes; i++ )
    {
        ATG::MESH_FRAME* pFrame = &RobotMesh.m_pFrames[i];
        ZeroMemory( pFrame, sizeof( ATG::MESH_FRAME ) );
        sprintf_s( pFrame->m_strName, "Frame %d", i );
        pFrame->m_matTransform = XMLoadFloat4x4( ( XMFLOAT4X4* )g_ModelData[i].pMatrix );
        pFrame->m_pMeshData = new ATG::MESH_DATA;
        pFrame->m_pChild = NULL;
        pFrame->m_pNext = ( i + 1 < m_dwNumMeshes ) ? &RobotMesh.m_pFrames[i + 1] : NULL;

        ATG::MESH_DATA* pMesh = pFrame->m_pMeshData;
        ZeroMemory( pMesh, sizeof( ATG::MESH_DATA ) );
        memcpy( &pMesh->m_VB, m_pMeshes[i].pVertexBuffer, sizeof( D3DVertexBuffer ) );
        pMesh->m_dwNumVertices = m_pMeshes[i].dwNumVertices;
        memcpy( &pMesh->m_IB, m_pMeshes[i].pIndexBuffer, sizeof( D3DIndexBuffer ) );
        pMesh->m_dwNumIndices = m_pMeshes[i].dwIndexCount;
        memcpy( pMesh->m_VertexElements, VertexElements, sizeof( VertexElements ) );
        pMesh->m_dwVertexSize = m_dwVertexSize;
        pMesh->m_dwPrimType = m_pMeshes[i].dwPrimType;
        pMesh->m_pVertexDecl = NULL;
        pMesh->m_dwNumSubsets = 1;

        ATG::MESH_SUBSET* pSubset = &pMesh->m_pSubsets[0];
        pSubset->mtrl.Diffuse.r = pSubset->mtrl.Diffuse.g = pSubset->mtrl.Diffuse.b = pSubset->mtrl.Diffuse.a =
            1.0f;
        pSubset->mtrl.Ambient.r = pSubset->mtrl.Ambient.g = pSubset->mtrl.Ambient.b = pSubset->mtrl.Ambient.a =
            1.0f;
        pSubset->pTexture = NULL;
        sprintf_s( pSubset->strTexture, "" );
        pSubset->dwVertexStart = 0L;
        pSubset->dwVertexCount = m_pMeshes[i].dwNumVertices;
        pSubset->dwIndexStart = 0L;
        pSubset->dwIndexCount = m_pMeshes[i].dwIndexCount;
        pSubset->dwPrimitiveCount = m_pMeshes[i].dwPrimitiveCount;
    }

    RobotMesh.Write( "game:\\Robot.xbg" );

    // delete the memory
    for( DWORD i = 0; i < m_dwNumMeshes; i++ )
    {
        ATG::MESH_FRAME* pFrame = &RobotMesh.m_pFrames[ i ];
        delete pFrame->m_pMeshData;
        pFrame->m_pMeshData = NULL;
    }

    delete [] RobotMesh.m_pFrames;
    RobotMesh.m_pFrames = NULL;

}

//--------------------------------------------------------------------------------------
// Name: Render()
// Desc: Renders the robot mesh 12 times and calc stats
//--------------------------------------------------------------------------------------
HRESULT CRobotGeometry::Render( LPDIRECT3DDEVICE9 m_pd3dDevice, ROBOTSTATS* pStats )
{
    static BOOL bSaveMesh = FALSE;
    if ( bSaveMesh )
    {
        SaveMesh( m_pd3dDevice );
        bSaveMesh = FALSE;
    }

    // Enable the primitive reset index.
    m_pd3dDevice->SetRenderState( D3DRS_PRIMITIVERESETENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_PRIMITIVERESETINDEX, 0xffff );

    // Render each part
    for( DWORD i = 0; i < m_dwNumMeshes; i++ )
    {
        // Set our world transform
        XMMATRIX matObject = XMLoadFloat4x4( ( XMFLOAT4X4* )g_ModelData[i].pMatrix );
        matObject = XMMatrixMultiply( matObject, g_matWorld );

        XMMATRIX matWorldView, matWorldViewProj, matWorldViewProjT;
        matWorldView = XMMatrixMultiply( matObject, g_matView );
        matWorldViewProj = XMMatrixMultiply( matWorldView, g_matProj );
        matWorldViewProjT = XMMatrixTranspose( matWorldViewProj );
        m_pd3dDevice->SetVertexShaderConstantF( 4, ( FLOAT* )&matWorldViewProjT, 4 );

        static FLOAT vLight[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
        m_pd3dDevice->SetVertexShaderConstantF( 1, vLight, 1 );

        // Draw robot
        m_pd3dDevice->SetVertexDeclaration( g_pVertexDeclaration );
        m_pd3dDevice->SetVertexShader( g_pVertexShader );
        m_pd3dDevice->SetPixelShader( g_pPixelShader );
        m_pd3dDevice->SetStreamSource( 0, m_pMeshes[i].pVertexBuffer, 0, m_dwVertexSize );
        m_pd3dDevice->SetIndices( m_pMeshes[i].pIndexBuffer );
        m_pd3dDevice->DrawIndexedVertices( m_pMeshes[i].dwPrimType, 0,
                                           0, m_pMeshes[i].dwIndexCount );

        // Record stats
        pStats->dwTriCount += m_pMeshes[i].dwPrimitiveCount;
        pStats->dwIndCount += m_pMeshes[i].dwIndexCount;
        pStats->dwNumVertices += m_pMeshes[i].dwNumVertices;
        pStats->dwCacheHits += m_pMeshes[i].dwCacheHits;
        pStats->dwPagesCrossed += m_pMeshes[i].dwPagesCrossed;
        pStats->dwDegenerateTris += m_pMeshes[i].dwDegenerateTris;
    }

    m_pd3dDevice->SetRenderState( D3DRS_PRIMITIVERESETENABLE, FALSE );

    return S_OK;
}

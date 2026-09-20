//--------------------------------------------------------------------------------------
// Fur.cpp
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
// Copyright (C) Tomohide Kano. All rights reserved.
//--------------------------------------------------------------------------------------
#include <xtl.h>
#include <xboxmath.h>
#include <AtgApp.h>
#include <AtgFont.h>
#include <AtgHelp.h>
#include <AtgInput.h>
#include <AtgUtil.h>
#include "FurMesh.h"
#include "FurTexture.h"
#include "NormalMap.h"
#include "MeshFunc.h"


//--------------------------------------------------------------------------------------
// Callouts for labelling the gamepad on the help screen
//--------------------------------------------------------------------------------------
ATG::HELP_CALLOUT g_HelpCallouts[] =
{
    { ATG::HELP_A_BUTTON,     ATG::HELP_PLACEMENT_2, L"Change\nmodel" },
    { ATG::HELP_B_BUTTON,     ATG::HELP_PLACEMENT_2, L"Toggle\nwireframe" },
    { ATG::HELP_X_BUTTON,     ATG::HELP_PLACEMENT_2, L"Auto-rotate\nlight" },
    { ATG::HELP_Y_BUTTON,     ATG::HELP_PLACEMENT_2, L"Toggle\ngravity" },
    { ATG::HELP_RIGHT_SHOULDER, ATG::HELP_PLACEMENT_2, L"Show\ntextures" },
    { ATG::HELP_DPAD,         ATG::HELP_PLACEMENT_2, L"Change fur\nlayers/length" },
    { ATG::HELP_LEFTSTICK,    ATG::HELP_PLACEMENT_2, L"Move\nmodel" },
    { ATG::HELP_RIGHTSTICK,   ATG::HELP_PLACEMENT_2, L"Rotate\nmodel" },
    { ATG::HELP_START_BUTTON, ATG::HELP_PLACEMENT_2, L"Toggle\nsimulation" },
    { ATG::HELP_BACK_BUTTON,  ATG::HELP_PLACEMENT_2, L"Display\nhelp" },
    { ATG::HELP_MISC_CALLOUT, ATG::HELP_PLACEMENT_1, L"Use triggers to zoom in/out" },
};
#define NUM_HELP_CALLOUTS (sizeof(g_HelpCallouts) / sizeof(g_HelpCallouts[0]))


//--------------------------------------------------------------------------------------
// Static variables
//--------------------------------------------------------------------------------------

// Miscellaneous states
static DWORD                    g_dwNumLayers = 20;
static FLOAT                    g_fFurLength = 0.7f;
static BOOL                     g_bWireframe = FALSE;
static BOOL                     g_bRotateLight = FALSE;
static BOOL                     g_bUseAnisotropy = TRUE;
static BOOL                     g_bCullBackfaces = TRUE;
static BOOL                     g_bUseAlphaTest = FALSE;
static BOOL                     g_bUseSimulation = TRUE;
static BOOL                     g_bUseGravity = TRUE;
static BOOL                     g_bShowTextures = FALSE;

// Object orientation (modelview transformation)
static XMVECTOR                 g_vTrans;
static XMVECTOR                 g_vTransDelta;
static XMVECTOR                 g_qRot;
static XMVECTOR                 g_qRotDelta;

// Projection parameters
static FLOAT                    s_NearZ = 0.5f;
static FLOAT                    s_FarZ = 50.0f;
static FLOAT                    s_FovY = XM_PI / 6;

XMMATRIX                        g_matWorldView;
XMMATRIX                        g_matWorldViewInverse;
XMMATRIX                        g_matProj;

// Model meshes
static FurMesh*                 s_pMeshes = NULL;
static FurMesh*                 s_pCurrentMesh = NULL;
static INT                      s_CurrentMesh = 0;

// Textures
static FurTexture*              s_pFurTexture = NULL;
static NormalMap*               s_pNormalMap = NULL;
static LPDIRECT3DVOLUMETEXTURE9 g_pLighting3DTexture = 0;

// Vertex/fragment shaders
static LPDIRECT3DVERTEXSHADER9  g_pSkinVertexShader = 0;
static LPDIRECT3DPIXELSHADER9   g_pSkinPixelShader = 0;

static LPDIRECT3DVERTEXSHADER9  g_pFurVertexShader = 0;
static LPDIRECT3DPIXELSHADER9   g_pFurPixelShader = 0;

// Vertex shader invariants
const DWORD                     VSCONST_MODELVIEWPOS = 0;
const DWORD                     VSCONST_CAMERA_POS = 4;
const DWORD                     VSCONST_LIGHT_DIR = 5;
const DWORD                     VSCONST_FUR_LENGTH = 6;
const DWORD                     VSCONST_OFFSET_SCALE = 7;

XMVECTOR                        g_vLightDir;


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

    HRESULT RenderFurMesh();
    HRESULT ShowTexture( FLOAT x, FLOAT y, FLOAT w, FLOAT h, LPDIRECT3DTEXTURE9 pTexture );

private:
    HRESULT Initialize();
    HRESULT Update();
    HRESULT Render();
};


//--------------------------------------------------------------------------------------
// Name: main()
// Desc: Entry point to the program.
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
// Name: InitShaders()
// Desc:
//--------------------------------------------------------------------------------------
HRESULT InitShaders()
{
    // Vertex shader for skin
    if( FAILED( ATG::LoadVertexShader( "game:\\Media\\Shaders\\SkinVS.xvu", &g_pSkinVertexShader ) ) )
        return E_FAIL;

    // Pixel shader for skin
    if( FAILED( ATG::LoadPixelShader( "game:\\Media\\Shaders\\SkinPS.xpu", &g_pSkinPixelShader ) ) )
        return E_FAIL;

    // Vertex shader for fur
    if( FAILED( ATG::LoadVertexShader( "game:\\Media\\Shaders\\FurVS.xvu", &g_pFurVertexShader ) ) )
        return E_FAIL;

    // Pixel shader for fur
    if( FAILED( ATG::LoadPixelShader( "game:\\Media\\Shaders\\FurPS.xpu", &g_pFurPixelShader ) ) )
        return E_FAIL;

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Initialize()
// Desc:
//--------------------------------------------------------------------------------------
HRESULT Sample::Initialize()
{
    m_bDrawHelp = FALSE;

    // Create the font
    if( FAILED( m_Font.Create( "game:\\Media\\Fonts\\Arial_16.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Confine text drawing to the title safe area
    m_Font.SetWindow( ATG::GetTitleSafeArea() );

    // Create the help
    if( FAILED( m_Help.Create( "game:\\Media\\Help\\Help.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Modelview transformation
    g_vTrans = XMVectorSet( 0.0f, 0.0f, 17.0f, 0.0f );
    g_vTransDelta = XMVectorSet( 0.0f, 0.0f, 0.0f, 0.0f );
    g_qRot = XMQuaternionRotationRollPitchYaw( 0.0f, 0.0f, 0.0f );
    g_qRotDelta = XMQuaternionIdentity();

    // model meshes
    s_pMeshes = new FurMesh[NUM_MESHES];
    for( DWORD i = 0; i < NUM_MESHES; i++ )
    {
        if( FAILED( s_pMeshes[i].Init( g_MeshParams[i] ) ) )
            return E_FAIL;
    }
    s_pCurrentMesh = &s_pMeshes[s_CurrentMesh];

    // Fur texture
    s_pFurTexture = new FurTexture();
    if( FAILED( s_pFurTexture->Init( ( DWORD )m_Timer.GetTime(), 128, 20 ) ) )
        return E_FAIL;

    // Normal map
    s_pNormalMap = new NormalMap();
    if( FAILED( s_pNormalMap->Init( 256, 256 ) ) )
        return E_FAIL;

    // Lighting texture
    if( FAILED( BuildFurLightingTexture( &g_pLighting3DTexture ) ) )
        return E_FAIL;

    // Vertex/pixel shaders
    if( FAILED( InitShaders() ) )
        return E_FAIL;

    // Determine the aspect ratio
    FLOAT fAspectRatio = ( FLOAT )m_d3dpp.BackBufferWidth / ( FLOAT )m_d3dpp.BackBufferHeight;

    // Setup global matrices
    g_matWorldView = XMMatrixIdentity();
    g_matProj = XMMatrixPerspectiveFovLH( s_FovY, fAspectRatio, s_NearZ, s_FarZ );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Update()
// Desc:
//--------------------------------------------------------------------------------------
HRESULT Sample::Update()
{
    FLOAT fElapsedTime = ( FLOAT )m_Timer.GetElapsedTime();

    // Get the current gamepad status
    ATG::GAMEPAD* pGamepad = ATG::Input::GetMergedInput();

    // Toggle help
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_BACK )
        m_bDrawHelp = !m_bDrawHelp;

    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_START )
    {
        g_bUseSimulation = !g_bUseSimulation;
        s_pNormalMap->ResetInertia();
    }

    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_A )
    {
        s_CurrentMesh = ++s_CurrentMesh % NUM_MESHES;
        s_pCurrentMesh = &s_pMeshes[s_CurrentMesh];
    }

    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_B )
        g_bWireframe = !g_bWireframe;

    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_X )
        g_bRotateLight = !g_bRotateLight;

    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_Y )
        g_bUseGravity = !g_bUseGravity;

    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER )
        g_bShowTextures = !g_bShowTextures;

    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_DPAD_RIGHT )
        g_dwNumLayers++;

    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_DPAD_LEFT )
        if( g_dwNumLayers > 0 ) g_dwNumLayers--;

    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_DPAD_DOWN )
        g_fFurLength = max( 0.0f, g_fFurLength - 0.02f );

    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_DPAD_UP )
        g_fFurLength = min( 1.0f, g_fFurLength + 0.02f );

    // Time step
    FLOAT dt = max( 0.01f, min( 0.1f, fElapsedTime ) );

    // Filtered time step
    static FLOAT dt2 = 0.025f;
    dt2 = 0.9f * dt2 + 0.1f * dt;

    // Update object orientation
    {
        XMVECTOR vTemp = XMVectorSet( 0.0f, 0.0f, 0.0f, 0.0f );
        XMVECTOR qTemp = XMVectorSet( 0.0f, 0.0f, 0.0f, 1.0f );

        // Translate
        {
            vTemp.x = 5.0f * pGamepad->fX1 * fElapsedTime;
            vTemp.y = 5.0f * pGamepad->fY1 * fElapsedTime;
        }
        // Zoom
        {
            g_vTrans.z += 0.2f * pGamepad->bLeftTrigger * fElapsedTime;
            g_vTrans.z -= 0.2f * pGamepad->bRightTrigger * fElapsedTime;
            g_vTrans.z = min( 0.9f * s_FarZ, max( 0.1f * s_FarZ, g_vTrans.z ) );
        }

        // Rotate
        {
            XMVECTOR vScreenAxis = XMVectorSet( pGamepad->fX2, pGamepad->fY2, 0.0f, 0.0f );
            FLOAT fMagnitude = XMVector3Length( vScreenAxis ).x;

            if( fMagnitude )
            {
                XMVECTOR vZAxis = XMVectorSet( 0.0f, 0.0f, 1.0f, 0.0f );
                XMVECTOR vRotationAxis;
                vRotationAxis = XMVector3Cross( vScreenAxis, vZAxis );
                qTemp = XMQuaternionRotationAxis( vRotationAxis, XM_PI * fMagnitude * fElapsedTime );
            }
        }

        FLOAT fDamping = min( 1.0f, dt / 0.125f );

        // Update modelview traslation and rotation using damping factor
        g_vTransDelta = ( 1 - fDamping ) * g_vTransDelta + fDamping * vTemp;
        g_vTrans += g_vTransDelta;
        g_qRotDelta = XMQuaternionSlerp( g_qRotDelta, qTemp, fDamping );
        g_qRotDelta = XMQuaternionNormalize( g_qRotDelta );
        g_qRot = XMQuaternionMultiply( g_qRot, g_qRotDelta );
        g_qRot = XMQuaternionNormalize( g_qRot );
    }

    // Calculate modelview matrix
    {
        XMVECTOR dummy;
        XMMATRIX matRotate = XMMatrixRotationQuaternion( g_qRot );
        XMMATRIX matTrans = XMMatrixTranslation( g_vTrans.x, g_vTrans.y, g_vTrans.z );
        g_matWorldView = XMMatrixMultiply( matRotate, matTrans );
        g_matWorldViewInverse = XMMatrixInverse( &dummy, g_matWorldView );
    }

    // Calculate light direction in model space
    {
        static FLOAT angle1 = 0.5f * XM_PI;
        static FLOAT angle2 = 0.2f * XM_PI;
        if( g_bRotateLight )
        {
            angle1 += 1.00f * dt;
            angle2 += 0.35f * dt;
        }
        XMVECTOR vDir = XMVectorSet( sinf( angle1 ), sinf( angle2 ), -cosf( angle1 ), 0.0f );
        g_vLightDir = XMVector4Transform( vDir, g_matWorldViewInverse );
        g_vLightDir = XMVector4Normalize( g_vLightDir );
    }

    // Update normal map
    if( !g_bWireframe && g_bUseSimulation )
    {
        // Gravity in model space
        XMVECTOR vGravity = XMVectorSet( 0.0f, g_bUseGravity ? -0.4f : -0.02f, 0.0f, 0.0f );
        vGravity = XMVector4Transform( vGravity, g_matWorldViewInverse );

        // Translational velocity in model space
        XMVECTOR vVelocity;
        vVelocity.x = +g_vTransDelta.x / dt2;
        vVelocity.y = +g_vTransDelta.y / dt2;
        vVelocity.z = -g_vTransDelta.z / dt2;
        vVelocity.w = 0.0f;
        vVelocity = XMVector4Transform( vVelocity, g_matWorldViewInverse );

        // Angular velocity in model space
        XMVECTOR vOmega;
        vOmega = XMQuaternionLn( g_qRotDelta );

        XMVECTOR vOmegaDT = 2 * vOmega / dt2;
        vOmega = XMVector4Transform( vOmegaDT, g_matWorldViewInverse );

        s_pNormalMap->Update( s_pCurrentMesh, dt2, vGravity, vVelocity, vOmega );
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: RenderFurMesh()
// Desc:
//--------------------------------------------------------------------------------------
HRESULT Sample::RenderFurMesh()
{
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, TRUE );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_POINT );

    if( g_bWireframe )
        m_pd3dDevice->SetRenderState( D3DRS_FILLMODE, D3DFILL_WIREFRAME );
    else
        m_pd3dDevice->SetRenderState( D3DRS_FILLMODE, D3DFILL_SOLID );

    XMMATRIX matAll = XMMatrixTranspose( XMMatrixMultiply( g_matWorldView, g_matProj ) );
    m_pd3dDevice->SetVertexShaderConstantF( VSCONST_MODELVIEWPOS, ( FLOAT* )&matAll, 4 );

    // Draw skin
    {
        m_pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_CCW );
        m_pd3dDevice->SetVertexShader( g_pSkinVertexShader );
        m_pd3dDevice->SetPixelShader( g_pSkinPixelShader );
        m_pd3dDevice->SetVertexShaderConstantF( VSCONST_MODELVIEWPOS, ( FLOAT* )&matAll, 4 );
        m_pd3dDevice->SetVertexShaderConstantF( VSCONST_CAMERA_POS, ( FLOAT* )&g_matWorldViewInverse._41, 1 );
        m_pd3dDevice->SetVertexShaderConstantF( VSCONST_LIGHT_DIR, ( FLOAT* )&g_vLightDir, 1 );
        s_pCurrentMesh->Draw();
    }

    // Draw fur
    if( g_dwNumLayers > 0 )
    {
        m_pd3dDevice->SetRenderState( D3DRS_CULLMODE, g_bCullBackfaces ? D3DCULL_CCW : D3DCULL_NONE );

        m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, TRUE );
        m_pd3dDevice->SetRenderState( D3DRS_SRCBLEND, D3DBLEND_SRCALPHA );
        m_pd3dDevice->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA );

        if( g_bUseAlphaTest )
        {
            m_pd3dDevice->SetRenderState( D3DRS_ALPHATESTENABLE, TRUE );
            m_pd3dDevice->SetRenderState( D3DRS_ALPHAREF, 0x10 );
            m_pd3dDevice->SetRenderState( D3DRS_ALPHAFUNC, D3DCMP_GREATEREQUAL );
        }

        // Vertex shader
        m_pd3dDevice->SetVertexShader( g_pFurVertexShader );
        m_pd3dDevice->SetVertexShaderConstantF( VSCONST_CAMERA_POS, ( FLOAT* )&g_matWorldViewInverse._41, 1 );
        m_pd3dDevice->SetVertexShaderConstantF( VSCONST_MODELVIEWPOS, ( FLOAT* )&matAll, 4 );
        m_pd3dDevice->SetVertexShaderConstantF( VSCONST_LIGHT_DIR, ( FLOAT* )&g_vLightDir, 1 );

        // Pixel shader
        m_pd3dDevice->SetPixelShader( g_pFurPixelShader );

        // Offset map
        m_pd3dDevice->SetTexture( 0, s_pNormalMap->GetOffsetMap() );
        m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
        m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
        m_pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP );
        m_pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP );

        // Lighting texture
        m_pd3dDevice->SetTexture( 1, g_pLighting3DTexture );
        m_pd3dDevice->SetSamplerState( 1, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
        m_pd3dDevice->SetSamplerState( 1, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
        m_pd3dDevice->SetSamplerState( 1, D3DSAMP_ADDRESSU, D3DTADDRESS_MIRRORONCE );
        m_pd3dDevice->SetSamplerState( 1, D3DSAMP_ADDRESSV, D3DTADDRESS_MIRRORONCE );
        m_pd3dDevice->SetSamplerState( 1, D3DSAMP_ADDRESSW, D3DTADDRESS_CLAMP );

        // Fur texture
        m_pd3dDevice->SetSamplerState( 2, D3DSAMP_MIPMAPLODBIAS, ATG::FtoDW( 0.5f ) );

        if( g_bUseAnisotropy )
        {
            m_pd3dDevice->SetSamplerState( 2, D3DSAMP_MAGFILTER, D3DTEXF_ANISOTROPIC );
            m_pd3dDevice->SetSamplerState( 2, D3DSAMP_MINFILTER, D3DTEXF_ANISOTROPIC );
            m_pd3dDevice->SetSamplerState( 2, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR );// D3DTEXF_ANISOTROPIC );
            m_pd3dDevice->SetSamplerState( 2, D3DSAMP_MAXANISOTROPY, 16 );
            m_pd3dDevice->SetSamplerState( 3, D3DSAMP_MAGFILTER, D3DTEXF_ANISOTROPIC );
            m_pd3dDevice->SetSamplerState( 3, D3DSAMP_MINFILTER, D3DTEXF_ANISOTROPIC );
            m_pd3dDevice->SetSamplerState( 3, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR );// D3DTEXF_ANISOTROPIC );
            m_pd3dDevice->SetSamplerState( 3, D3DSAMP_MAXANISOTROPY, 16 );
        }
        else
        {
            m_pd3dDevice->SetSamplerState( 2, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
            m_pd3dDevice->SetSamplerState( 2, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
            m_pd3dDevice->SetSamplerState( 2, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR );
            m_pd3dDevice->SetSamplerState( 2, D3DSAMP_MAXANISOTROPY, 1 );
            m_pd3dDevice->SetSamplerState( 3, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
            m_pd3dDevice->SetSamplerState( 3, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
            m_pd3dDevice->SetSamplerState( 3, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR );
            m_pd3dDevice->SetSamplerState( 3, D3DSAMP_MAXANISOTROPY, 1 );
        }

        // Draw fur shells
        for( DWORD i = 0; i < g_dwNumLayers; i++ )
        {
            FLOAT fLayer = ( FLOAT )( i + 1 ) / g_dwNumLayers;
            FLOAT fLength = g_fFurLength * fLayer;
            FLOAT fScale = -g_fFurLength * ( 1.0f * fLayer * fLayer + 0.4f * fLayer );
            m_pd3dDevice->SetTexture( 2, s_pFurTexture->GetLayerTexture( fLayer ) );

            m_pd3dDevice->SetSamplerState( 2, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP );
            m_pd3dDevice->SetSamplerState( 2, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP );

            m_pd3dDevice->SetVertexShaderConstantF( VSCONST_FUR_LENGTH, ( FLOAT* )&fLength, 1 );
            m_pd3dDevice->SetVertexShaderConstantF( VSCONST_OFFSET_SCALE, ( FLOAT* )&fScale, 1 );
            s_pCurrentMesh->Draw();
        }

        m_pd3dDevice->SetSamplerState( 2, D3DSAMP_MIPMAPLODBIAS, ATG::FtoDW( 0.0f ) );
        m_pd3dDevice->SetSamplerState( 2, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
        m_pd3dDevice->SetSamplerState( 2, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
        m_pd3dDevice->SetSamplerState( 2, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR );
        m_pd3dDevice->SetSamplerState( 2, D3DSAMP_MAXANISOTROPY, 1 );
    }

    // Restore state
    m_pd3dDevice->SetRenderState( D3DRS_FILLMODE, D3DFILL_SOLID );
    m_pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_CCW );
    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_ALPHATESTENABLE, FALSE );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: ShowTexture()
// Desc:
//--------------------------------------------------------------------------------------
HRESULT Sample::ShowTexture( FLOAT x, FLOAT y, FLOAT w, FLOAT h,
                             LPDIRECT3DTEXTURE9 pTexture )
{
    x += ( x < 0 ) ? m_Font.m_rcWindow.x2 : m_Font.m_rcWindow.x1;
    y += ( y < 0 ) ? m_Font.m_rcWindow.y2 : m_Font.m_rcWindow.y1;

    FLOAT v[4][6] =
    {
        // sx   sy   tu    tv
        { x + 0, y + 0, 0.0f, 0.0f },
        { x + w, y + 0, 1.0f, 0.0f },
        { x + w, y + h, 1.0f, 1.0f },
        { x + 0, y + h, 0.0f, 1.0f },
    };

    static LPDIRECT3DVERTEXDECLARATION9 m_pShowTextureVertexDecl = NULL;
    static LPDIRECT3DVERTEXSHADER9 m_pShowTextureVS = NULL;
    static LPDIRECT3DPIXELSHADER9 m_pShowTexturePS = NULL;

    if( NULL == m_pShowTextureVS )
    {
        static const D3DVERTEXELEMENT9 decl[] =
        {
            { 0, 0, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT,  D3DDECLUSAGE_POSITION, 0 },
            { 0, 8, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT,  D3DDECLUSAGE_TEXCOORD, 0 },
            D3DDECL_END()
        };
        m_pd3dDevice->CreateVertexDeclaration( decl, &m_pShowTextureVertexDecl );

        if( FAILED( ATG::LoadVertexShader( "game:\\Media\\Shaders\\ShowTextureVS.xvu", &m_pShowTextureVS ) ) )
            return E_FAIL;
        if( FAILED( ATG::LoadPixelShader( "game:\\Media\\Shaders\\ShowTexturePS.xpu", &m_pShowTexturePS ) ) )
            return E_FAIL;
    }

    FLOAT sw = ( FLOAT )ATG::Application::m_d3dpp.BackBufferWidth;
    FLOAT sh = ( FLOAT )ATG::Application::m_d3dpp.BackBufferHeight;
    FLOAT fScreenSpaceScaleAndOffset[8] =
    {
        2.0f / sw, -2.0f / sh, 0.0f, 0.0f,
        -1.0f - 1.0f / sw, 1.0f - 1.0f / sh, 0.0f, 1.0f
    };
    m_pd3dDevice->SetVertexShaderConstantF( 0, fScreenSpaceScaleAndOffset, 2 );
    m_pd3dDevice->SetVertexDeclaration( m_pShowTextureVertexDecl );
    m_pd3dDevice->SetVertexShader( m_pShowTextureVS );
    m_pd3dDevice->SetPixelShader( m_pShowTexturePS );
    m_pd3dDevice->SetTexture( 0, pTexture );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, FALSE );
    m_pd3dDevice->DrawPrimitiveUP( D3DPT_QUADLIST, 1, v, sizeof( v[0] ) );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Render()
// Desc:
//--------------------------------------------------------------------------------------
HRESULT Sample::Render()
{
    // Clear the render target and the zbuffer
    m_pd3dDevice->Clear( 0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL,
                         0xff676767, 1.0f, 0L );

    // Render the furry mesh
    RenderFurMesh();

    // For debugging, show the normal and offset maps
    if( g_bShowTextures )
    {
        ShowTexture( -128, -128, 128, 128, s_pNormalMap->GetOffsetMap() );
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
        m_Font.DrawText( 0, 0, 0xffffffff, L"Fur" );
        m_Font.SetScaleFactors( 1.0f, 1.0f );
        m_Font.DrawText( 0, 0, 0xffffff00, m_Timer.GetFrameRate(), ATGFONT_RIGHT );
        m_Font.End();

        m_Font.SetCursorPosition( 16, 50 );
        m_Font.DrawText( 0xffffffff, L"Object:\n" );
        m_Font.DrawText( 0xffffffff, L"Fur Layers:\n" );
        m_Font.DrawText( 0xffffffff, L"Fur Length:\n\n" );
        m_Font.DrawText( 0xffffffff, L"Simulation:\n" );
        m_Font.DrawText( 0xffffffff, L"Gravity:\n\n" );
        m_Font.DrawText( 0xffffffff, L"Anisotropic Filter:\n" );
        m_Font.DrawText( 0xffffffff, L"Backface Culling:\n" );
        m_Font.DrawText( 0xffffffff, L"Alpha Test:\n\n" );

        WCHAR str[80];
#define ONOFF(x) ((x) ? L"ON" : L"OFF")

        m_Font.SetCursorPosition( 205, 50 );
        swprintf_s( str, L"%s\n", s_pCurrentMesh->GetName() ); m_Font.DrawText( 0xffffff00, str );
        swprintf_s( str, L"%d\n", g_dwNumLayers );              m_Font.DrawText( 0xffffff00, str );
        swprintf_s( str, L"%.2f\n\n", g_fFurLength );           m_Font.DrawText( 0xffffff00, str );
        swprintf_s( str, L"%s\n", ONOFF(g_bUseSimulation) );  m_Font.DrawText( 0xffffff00, str );
        swprintf_s( str, L"%s\n\n", ONOFF(g_bUseGravity) );     m_Font.DrawText( 0xffffff00, str );
        swprintf_s( str, L"%s\n", ONOFF(g_bUseAnisotropy) );  m_Font.DrawText( 0xff808000, str );
        swprintf_s( str, L"%s\n", ONOFF(g_bCullBackfaces) );  m_Font.DrawText( 0xff808000, str );
        swprintf_s( str, L"%s\n\n", ONOFF(g_bUseAlphaTest) );   m_Font.DrawText( 0xff808000, str );
    }

    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}


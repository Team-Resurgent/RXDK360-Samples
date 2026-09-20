//--------------------------------------------------------------------------------------
// FXLite.cpp
//
// The FXLite sample demonstrates how to use a custom version of the Direct3D effects
// framework.
// FX Lite is a run-time library for the effects framework that has been written and
// optimized for the Xbox 360 platform. It includes the subset of effects run-time features
// that are conducive to speed, but it is a completely separate library from the XM
// effects library. FX Lite can even coexist with the XM run-time if so desired.
// The library is compatible with the .fx source file format for all supported components.
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include <xtl.h>
#include <xboxmath.h>
#include <fxl.h>
#include <AtgApp.h>
#include <AtgFont.h>
#include <AtgHelp.h>
#include <AtgInput.h>
#include <AtgMesh.h>
#include <AtgResource.h>
#include <AtgUtil.h>


//--------------------------------------------------------------------------------------
// Globals
//--------------------------------------------------------------------------------------
LPCSTR g_pEffectFiles[] =
{
    "game:\\Media\\Effects\\Hemisphere.fx",                     // Hemi sphere lighting
    "game:\\Media\\Effects\\HemisphereOrennayerLighting.fx",    // Oren-Nayer diffuse model
};

LPCWSTR g_pEffectNames[] =
{
    L"Hemisphere",                        // Hemi sphere lighting
    L"Hemisphere/O-N",                    // Oren-Nayer diffuse model
};
static const DWORD  NUM_EFFECT = sizeof( g_pEffectFiles ) / sizeof( g_pEffectFiles[0] );

const INT           TEX_SIZE = 256;


//--------------------------------------------------------------------------------------
// Callback functions
//--------------------------------------------------------------------------------------
VOID WINAPI SinTan( D3DXVECTOR4* pOut, const D3DXVECTOR2* pTexCoord,
                    const D3DXVECTOR2* pTexelSize, VOID* pData );


//--------------------------------------------------------------------------------------
// Callouts for labelling the gamepad on the help screen
//--------------------------------------------------------------------------------------
ATG::HELP_CALLOUT g_HelpCallouts[] =
{
    { ATG::HELP_LEFTSTICK,    ATG::HELP_PLACEMENT_1, L"Rotate object" },
    { ATG::HELP_LEFT_BUTTON,  ATG::HELP_PLACEMENT_1, L"Trigger rotates object" },
    { ATG::HELP_RIGHT_BUTTON, ATG::HELP_PLACEMENT_1, L"Trigger rotates object" },
    { ATG::HELP_RIGHTSTICK,   ATG::HELP_PLACEMENT_1, L"Move light" },
    { ATG::HELP_X_BUTTON,     ATG::HELP_PLACEMENT_1, L"Toggle effect" },
    { ATG::HELP_A_BUTTON,     ATG::HELP_PLACEMENT_2, L"Change\ntechnique" },
    { ATG::HELP_BACK_BUTTON,  ATG::HELP_PLACEMENT_1, L"Display help" },


};
static const DWORD  NUM_HELP_CALLOUTS = sizeof( g_HelpCallouts ) / sizeof( g_HelpCallouts[0] );


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

    // FX objects
    FXLEffect* m_pEffectFXL;

    // Handles for the effect
    BOOL m_bEffectUpdated;
    FXLHANDLE m_hWorldView;
    FXLHANDLE m_hProjection;
    FXLHANDLE m_hDirFromLight;
    FXLHANDLE m_hTechnique;
    FXLHANDLE m_hPass;
    FXLHANDLE m_hSamplerSinTan;

    ATG::Mesh* m_pObject;     // Object to render

    DWORD           m_Pad0[1];

    // Object roataion quaternion
    XMVECTOR m_qRotation;

    // Directional light parameter
    XMVECTOR m_vLightDirection;

    // FX statistics
    INT m_iCurrentEffect;
    INT m_iNumberOfTechniques;
    INT m_iCurrentTechnique;
    WCHAR           m_strTechnique[512];

    // Table texture for O-N Lighting model
    LPDIRECT3DTEXTURE9 m_pLightingTexture;

    // Transform matrices
    XMMATRIX m_matWorld;
    XMMATRIX m_matView;
    XMMATRIX m_matProj;

    XMVECTOR        RotationArc( XMVECTOR v0, XMVECTOR v1 );

public:
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
    atgApp.Run();
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
    m_bDrawHelp = FALSE;

    if( FAILED( m_Help.Create( "game:\\Media\\Help\\Help.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Load effect
    m_iCurrentEffect = 0;

    VOID* pCode;
    DWORD dwSize;
    if( FAILED( ATG::LoadFile( g_pEffectFiles[ m_iCurrentEffect ], &pCode, &dwSize ) ) )
        ATG::FatalError( "Couldn't load file\n" );

    // Compile an effect
    LPD3DXBUFFER pEffectData;
    LPD3DXBUFFER pErrorList;

    DWORD CompileFlags = D3DXSHADER_FXLPARAMETERS_AS_VARIABLE_NAMES;

    if( FAILED( FXLCompileEffect( ( CHAR* )pCode, dwSize,
                                  NULL, NULL,
                                  CompileFlags,
                                  &pEffectData, &pErrorList ) ) )
    {
        pErrorList->Release();
        ATG::FatalError( "Couldn't compile effect\n" );
    }

    // Create effect
    if( FAILED( FXLCreateEffect( m_pd3dDevice, pEffectData->GetBufferPointer(),
                                 NULL, &m_pEffectFXL ) ) )
        ATG::FatalError( "Couldn't compile effect\n" );

    ATG::UnloadFile( pCode );
    pEffectData->Release();

    // Create a texture for O-N lighting
    if( FAILED( m_pd3dDevice->CreateTexture( TEX_SIZE, TEX_SIZE, 1, 0, D3DFMT_R32F,
                                             D3DPOOL_DEFAULT, &m_pLightingTexture, NULL ) ) )
        return S_FALSE;
    if( FAILED( D3DXFillTexture( m_pLightingTexture, SinTan, NULL ) ) )
        return S_FALSE;

    // Retrieve technique desc
    FXLEFFECT_DESC desc;
    m_pEffectFXL->GetEffectDesc( &desc );
    m_iNumberOfTechniques = desc.Techniques;
    m_iCurrentTechnique = 0;

    FXLTECHNIQUE_DESC techdesc;
    m_pEffectFXL->GetTechniqueDesc( m_pEffectFXL->GetTechniqueHandleFromIndex(
                                    m_iCurrentTechnique ), &techdesc );
    swprintf_s( m_strTechnique, L"%S", techdesc.pName );

    // Load some meshes
    m_pObject = new ATG::Mesh();
    if( FAILED( m_pObject->Create( "game:\\Media\\Meshes\\Skullocc.xbg", NULL ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Determine the aspect ratio
    FLOAT fAspectRatio = ( FLOAT )m_d3dpp.BackBufferWidth / ( FLOAT )m_d3dpp.BackBufferHeight;

    // Set the transform matrices
    XMVECTOR vEyePt = XMVectorSet( 0.0f, 0.0f, -16.0f, 0.0f );
    XMVECTOR vLookatPt = XMVectorSet( 0.0f, 0.0f, 0.0f, 0.0f );
    XMVECTOR vUpVec = XMVectorSet( 0.0f, 1.0f, 0.0f, 0.0f );
    m_matWorld = XMMatrixIdentity();
    m_matView = XMMatrixLookAtLH( vEyePt, vLookatPt, vUpVec );
    m_matProj = XMMatrixPerspectiveFovLH( XM_PI / 3, fAspectRatio, 1.0f, 10000.0f );

    // Set up rotation parameters
    m_qRotation.x = 0.0f;
    m_qRotation.y = 0.0f;
    m_qRotation.z = 0.0f;
    m_qRotation.w = 1.0f;

    m_bEffectUpdated = TRUE;

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

    // Switch technique
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_A )
    {
        FXLTECHNIQUE_DESC desc;
        m_iCurrentTechnique = ( m_iCurrentTechnique + 1 ) % m_iNumberOfTechniques;
        m_pEffectFXL->GetTechniqueDesc( m_pEffectFXL->GetTechniqueHandleFromIndex( m_iCurrentTechnique ),
                                        &desc );
        swprintf_s( m_strTechnique, L"%S", desc.pName );
        m_bEffectUpdated = TRUE;
    }

    // Load new effect
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_X )
    {
        m_pEffectFXL->Release();
        m_iCurrentEffect = ( m_iCurrentEffect + 1 ) % NUM_EFFECT;

        VOID* pCode;
        DWORD dwSize;
        if( FAILED( ATG::LoadFile( g_pEffectFiles[ m_iCurrentEffect ], &pCode, &dwSize ) ) )
            ATG::FatalError( "Couldn't load file\n" );

        DWORD CompileFlags = D3DXSHADER_FXLPARAMETERS_AS_VARIABLE_NAMES;

        LPD3DXBUFFER pEffectData;
        LPD3DXBUFFER pErrorList;
        if( FAILED( FXLCompileEffect( ( CHAR* )pCode, dwSize,
                                      NULL, NULL, CompileFlags,
                                      &pEffectData, &pErrorList ) ) )
        {
            pErrorList->Release();
            ATG::FatalError( "Couldn't compile effect\n" );
        }

        // Create effect
        if( FAILED( FXLCreateEffect( m_pd3dDevice, pEffectData->GetBufferPointer(),
                                     NULL, &m_pEffectFXL ) ) )
            ATG::FatalError( "Couldn't compile effect\n" );

        ATG::UnloadFile( pCode );
        pEffectData->Release();

        FXLTECHNIQUE_DESC desc;
        m_pEffectFXL->GetTechniqueDesc( m_pEffectFXL->GetTechniqueHandleFromIndex( m_iCurrentTechnique ), &desc );
        swprintf_s( m_strTechnique, L"%S", desc.pName );
        m_bEffectUpdated = TRUE;
    }

    if( m_bEffectUpdated )
    {
        // Retrieve handles of effect
        m_hWorldView = m_pEffectFXL->GetParameterHandle( "WorldView" );
        m_hProjection = m_pEffectFXL->GetParameterHandle( "Projection" );
        m_hDirFromLight = m_pEffectFXL->GetParameterHandle( "DirFromLight" );
        m_hTechnique = m_pEffectFXL->GetTechniqueHandleFromIndex( m_iCurrentTechnique );
        m_hPass = m_pEffectFXL->GetPassHandleFromIndex( m_hTechnique, 0 );
        m_hSamplerSinTan = m_pEffectFXL->GetParameterHandle( "SinTanTex" );
        if( m_hSamplerSinTan )
            m_pEffectFXL->SetSampler( m_hSamplerSinTan, m_pLightingTexture );

        m_bEffectUpdated = FALSE;
    }

    // Update the object rotation
    XMVECTOR qR;
    XMVECTOR vOrig = XMVectorSet( 0.0f, 0.0f, -1.0f, 0.0f );
    XMVECTOR vDest = XMVectorSet( pGamepad->fX1 * m_fElapsedTime * 4.f,
                                  pGamepad->fY1 * m_fElapsedTime * 4.f, -1.0f, 0.0f );
    if( pGamepad->fX1 || pGamepad->fY1 )
    {
        qR = RotationArc( vOrig, vDest );
        m_qRotation = XMQuaternionMultiply( m_qRotation, qR );
    }

    vOrig.x = 1.0f;
    vOrig.y = 0.0f;
    vOrig.z = 0.0f;
    vDest.x = 1.0f;
    vDest.y = ( pGamepad->bLeftTrigger - pGamepad->bRightTrigger ) * m_fElapsedTime / 30.f;
    vDest.z = 0.0f;
    if( vDest.y )
    {
        qR = RotationArc( vOrig, vDest );
        m_qRotation = XMQuaternionMultiply( m_qRotation, qR );
    }

    m_matWorld = XMMatrixAffineTransformation( XMVectorSet( 1, 1, 1, 1 ), XMVectorZero(),
                                               m_qRotation, XMVectorZero() );

    // Set up directional light parameter
    m_vLightDirection.x = pGamepad->fX2;
    m_vLightDirection.y = pGamepad->fY2;
    m_vLightDirection.z = 1.0f;
    m_vLightDirection = XMVector3Normalize( m_vLightDirection );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Render()
// Desc: Sets up render states, clears the viewport, and renders the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Render()
{
    // Set default render states
    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, TRUE );

    // Draw a gradient filled background
    ATG::RenderBackground( 0xff0000ff, 0xff000000 );

    // Set misc renderstates
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, TRUE );

    // Draw mesh
    if( m_pEffectFXL )
    {
        D3DXMATRIX matWorldView;
        D3DXMATRIX matProj;
        D3DXVECTOR4 vLightDirection;
        XMStoreFloat4x4( ( XMFLOAT4X4* )&matWorldView, XMMatrixMultiply( m_matWorld, m_matView ) );
        XMStoreFloat4x4( ( XMFLOAT4X4* )&matProj, m_matProj );
        XMStoreFloat4( ( XMFLOAT4* )&vLightDirection, m_vLightDirection );

        // Set effect parameters
        m_pEffectFXL->SetMatrixF4x4( m_hWorldView, matWorldView );
        m_pEffectFXL->SetMatrixF4x4( m_hProjection, matProj );
        m_pEffectFXL->SetVectorF( m_hDirFromLight, vLightDirection );

        m_pEffectFXL->BeginTechnique( m_hTechnique, 0 );
        m_pEffectFXL->BeginPass( m_hPass );

        m_pEffectFXL->Commit();

        m_pObject->Render();

        m_pEffectFXL->EndPass();
        m_pEffectFXL->EndTechnique();
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
        m_Font.DrawText( 0, 0, 0xffffffff, L"FXLite" );
        m_Font.SetScaleFactors( 1.0f, 1.0f );
        m_Font.DrawText( 0, 0, 0xffffff00, m_Timer.GetFrameRate(), ATGFONT_RIGHT );

        m_Font.DrawText( 0, -50, 0xffffffff, L"Effect: " );
        m_Font.DrawText( 0xffffff00, g_pEffectNames[ m_iCurrentEffect ] );
        m_Font.DrawText( 0, -25, 0xffffffff, L"Technique: " );
        m_Font.DrawText( 0xffffff00, m_strTechnique );
        m_Font.End();
    }

    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: RotationArc()
// Desc: Calc rotation arc
//--------------------------------------------------------------------------------------
XMVECTOR Sample::RotationArc( XMVECTOR v0, XMVECTOR v1 )
{
    v0 = XMVector3Normalize( v0 );
    v1 = XMVector3Normalize( v1 );

    XMVECTOR cp = XMVector3Cross( v0, v1 );

    FLOAT s = sqrtf( ( 1 + XMVector3Dot( v0, v1 ).x ) * 2 );
    XMVECTOR q;
    q.x = cp.x / s;
    q.y = cp.y / s;
    q.z = cp.z / s;
    q.w = s / 2.0f;
    return q;
}


//--------------------------------------------------------------------------------------
// Name: SinTan()
// Desc: Callback creating Sin&Tan table for O-N lighting model
//--------------------------------------------------------------------------------------
VOID WINAPI SinTan( D3DXVECTOR4* pOut, const D3DXVECTOR2* pTexCoord,
                    const D3DXVECTOR2* pTexelSize, VOID* pData )
{
    FLOAT x = pTexCoord->x;
    FLOAT y = pTexCoord->y;
    FLOAT min = 2.0f * ( ( x < y ) ? x : y ) - 1.0f;
    FLOAT max = 2.0f * ( ( x < y ) ? y : x ) - 1.0f;
    pOut->x = sinf( acosf( min ) ) * tanf( acosf( max ) );
}

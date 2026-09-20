//--------------------------------------------------------------------------------------
// GamepadMesh.cpp
//
// Code to render a gamepad controller
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include <xtl.h>
#include <xgraphics.h>
#include <assert.h>
#include "AtgInput.h"
#include "AtgUtil.h"
#include "AtgResource.h"
#include "GamepadMesh.h"
#include <AtgApp.h>


// Offsets for building matrices which are used to animate the gamepad controls.
XMFLOAT3            g_vGamepadOffset = XMFLOAT3( 0.00f, 0.00f, 0.00f );
XMFLOAT3            g_vLeftTriggerAxis = XMFLOAT3( 1.00f, 0.00f, 0.00f );
XMFLOAT3            g_vRightTriggerAxis = XMFLOAT3( 1.00f, 0.00f, 0.00f );
XMFLOAT3            g_vLeftTriggerOffset = XMFLOAT3( -41.40f, 3.75f, 46.23f );
XMFLOAT3            g_vRightTriggerOffset = XMFLOAT3( 41.40f, 3.75f, 46.23f );
XMFLOAT3            g_vDPadOffset = XMFLOAT3( -21.35f, 15.72f, -3.50f );
XMFLOAT3            g_vLeftStickOffset = XMFLOAT3( 0.00f, 15.56f, 0.00f );
XMFLOAT3            g_vRightStickOffset = XMFLOAT3( 0.00f, 15.56f, 0.00f );


// Structures for animating, highlighting, and texturing the gamepad controls.
BOOL*               g_ControlActive;
XMMATRIX*           g_ControlMatrix;

DWORD               CONTROL_UNKNOWN;
DWORD               CONTROL_BODY;
DWORD               CONTROL_LEFT_THUMBSTICK;
DWORD               CONTROL_RIGHT_THUMBSTICK;
DWORD               CONTROL_DPAD;
DWORD               CONTROL_BACK_BUTTON;
DWORD               CONTROL_START_BUTTON;
DWORD               CONTROL_A_BUTTON;
DWORD               CONTROL_B_BUTTON;
DWORD               CONTROL_X_BUTTON;
DWORD               CONTROL_Y_BUTTON;
DWORD               CONTROL_A_COVER;
DWORD               CONTROL_B_COVER;
DWORD               CONTROL_X_COVER;
DWORD               CONTROL_Y_COVER;
DWORD               CONTROL_LEFT_TRIGGER;
DWORD               CONTROL_RIGHT_TRIGGER;
DWORD               CONTROL_LEFT_SHOULDER;
DWORD               CONTROL_RIGHT_SHOULDER;


LPDIRECT3DTEXTURE9  g_pTexture = NULL;
LPDIRECT3DTEXTURE9  g_pEnvMapTexture = NULL;


//--------------------------------------------------------------------------------------
// Name: RenderCallback()
// Desc: Overridden from the base class so that we can animate and highlight
//       individual mesh subsets before rendering them.
//--------------------------------------------------------------------------------------
BOOL GamepadMesh::RenderCallback( DWORD dwSubset, const ATG::MESH_SUBSET* pSubset,
                                  DWORD dwFlags )
{
    // Set matrix
    XMMATRIX mat = m_matWorld;
    mat._41 += g_vGamepadOffset.x;
    mat._42 += g_vGamepadOffset.y;
    mat._43 += g_vGamepadOffset.z;

    // Set transforms
    XMMATRIX matWorldView = XMMatrixMultiply( mat, m_matView );
    XMMATRIX matWorldViewProj = XMMatrixMultiply( matWorldView, m_matProj );

    matWorldViewProj = XMMatrixTranspose( matWorldViewProj );
    ATG::g_pd3dDevice->SetVertexShaderConstantF( 0, ( FLOAT* )&matWorldViewProj, 4 );

    matWorldView = XMMatrixTranspose( matWorldView );
    ATG::g_pd3dDevice->SetVertexShaderConstantF( 4, ( FLOAT* )&matWorldView, 4 );

    if( pSubset->mtrl.Diffuse.a < 1.0f )
        *( FLOAT* )&pSubset->mtrl.Diffuse.a = 0.75f;

    ATG::g_pd3dDevice->SetVertexShaderConstantF( 8, ( FLOAT* )&pSubset->mtrl.Diffuse, 1 );
    ATG::g_pd3dDevice->SetVertexShaderConstantF( 9, ( FLOAT* )&pSubset->mtrl.Specular, 1 );

    // Set texture
    ATG::g_pd3dDevice->SetTexture( 0, g_pTexture );
    ATG::g_pd3dDevice->SetTexture( 1, g_pEnvMapTexture );

    return TRUE;
};


//--------------------------------------------------------------------------------------
// Name: Create()
// Desc: Initializes the gamepad mesh
//--------------------------------------------------------------------------------------
HRESULT GamepadMesh::Create( const CHAR* strFilename, const ATG::PackedResource* pResource )
{
    // Load the gamepad mesh
    if( FAILED( ATG::Mesh::Create( strFilename ) ) )
        return E_FAIL;

    for( DWORD i = 0; i < m_dwNumFrames; i++ )
    {
        if( !_strcmpi( m_pFrames[i].m_strName, "cntrl_body" ) )
            CONTROL_BODY = i;
        else if( !_strcmpi( m_pFrames[i].m_strName, "cntrl_back_button" ) )
            CONTROL_BACK_BUTTON = i;
        else if( !_strcmpi( m_pFrames[i].m_strName, "cntrl_start_button" ) )
            CONTROL_START_BUTTON = i;
        else if( !_strcmpi( m_pFrames[i].m_strName, "cntrl_d_pad" ) )
            CONTROL_DPAD = i;
        else if( !_strcmpi( m_pFrames[i].m_strName, "cntrl_a_button" ) )
            CONTROL_A_BUTTON = i;
        else if( !_strcmpi( m_pFrames[i].m_strName, "cntrl_b_button" ) )
            CONTROL_B_BUTTON = i;
        else if( !_strcmpi( m_pFrames[i].m_strName, "cntrl_x_button" ) )
            CONTROL_X_BUTTON = i;
        else if( !_strcmpi( m_pFrames[i].m_strName, "cntrl_y_button" ) )
            CONTROL_Y_BUTTON = i;
        else if( !_strcmpi( m_pFrames[i].m_strName, "cntrl_a_cover" ) )
            CONTROL_A_COVER = i;
        else if( !_strcmpi( m_pFrames[i].m_strName, "cntrl_b_cover" ) )
            CONTROL_B_COVER = i;
        else if( !_strcmpi( m_pFrames[i].m_strName, "cntrl_x_cover" ) )
            CONTROL_X_COVER = i;
        else if( !_strcmpi( m_pFrames[i].m_strName, "cntrl_y_cover" ) )
            CONTROL_Y_COVER = i;
        else if( !_strcmpi( m_pFrames[i].m_strName, "cntrl_left_thumbstick" ) )
            CONTROL_LEFT_THUMBSTICK = i;
        else if( !_strcmpi( m_pFrames[i].m_strName, "cntrl_right_thumbstick" ) )
            CONTROL_RIGHT_THUMBSTICK = i;
        else if( !_strcmpi( m_pFrames[i].m_strName, "cntrl_left_trigger" ) )
            CONTROL_LEFT_TRIGGER = i;
        else if( !_strcmpi( m_pFrames[i].m_strName, "cntrl_right_trigger" ) )
            CONTROL_RIGHT_TRIGGER = i;
        else if( !_strcmpi( m_pFrames[i].m_strName, "cntrl_left_shoulder" ) )
            CONTROL_LEFT_SHOULDER = i;
        else if( !_strcmpi( m_pFrames[i].m_strName, "cntrl_right_shoulder" ) )
            CONTROL_RIGHT_SHOULDER = i;
        else
            CONTROL_UNKNOWN = i;
    }

    // Initialize the control highlight states, matrices, and textures
    g_ControlActive = new BOOL[m_dwNumFrames];
    g_ControlMatrix = new XMMATRIX[m_dwNumFrames];

    for( DWORD i = 0; i < m_dwNumFrames; i++ )
    {
        g_ControlActive[i] = FALSE;
        g_ControlMatrix[i] = m_pFrames[i].m_matTransform;
    }

    g_pTexture = pResource->GetTexture( "MatteGray" );
    g_pEnvMapTexture = pResource->GetTexture( "BlackGlass" );

    // Load the shaders used for rendering the gamepad model
    if( FAILED( ATG::LoadVertexShader( "game:\\Media\\Shaders\\Gamepad.xvu", &m_pVS ) ) )
        return E_FAIL;
    if( FAILED( ATG::LoadPixelShader( "game:\\Media\\Shaders\\Gamepad.xpu", &m_pPS ) ) )
        return E_FAIL;

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Update()
// Desc: Animates the controls of the gamepad.
//--------------------------------------------------------------------------------------
HRESULT GamepadMesh::Update( ATG::GAMEPAD* pGamepad )
{
    m_pFrames[CONTROL_BODY].m_pMeshData->m_pSubsets[0].mtrl.Diffuse.r = 0.6f;
    m_pFrames[CONTROL_BODY].m_pMeshData->m_pSubsets[0].mtrl.Diffuse.g = 0.6f;
    m_pFrames[CONTROL_BODY].m_pMeshData->m_pSubsets[0].mtrl.Diffuse.b = 0.6f;
    m_pFrames[CONTROL_BODY].m_pMeshData->m_pSubsets[0].mtrl.Specular.r = 0.1f;
    m_pFrames[CONTROL_BODY].m_pMeshData->m_pSubsets[0].mtrl.Specular.g = 0.1f;
    m_pFrames[CONTROL_BODY].m_pMeshData->m_pSubsets[0].mtrl.Specular.b = 0.1f;
    m_pFrames[CONTROL_LEFT_THUMBSTICK].m_pMeshData->m_pSubsets[0].mtrl.Specular.r = 0.1f;
    m_pFrames[CONTROL_LEFT_THUMBSTICK].m_pMeshData->m_pSubsets[0].mtrl.Specular.g = 0.1f;
    m_pFrames[CONTROL_LEFT_THUMBSTICK].m_pMeshData->m_pSubsets[0].mtrl.Specular.b = 0.1f;
    m_pFrames[CONTROL_RIGHT_THUMBSTICK].m_pMeshData->m_pSubsets[0].mtrl.Specular.r = 0.1f;
    m_pFrames[CONTROL_RIGHT_THUMBSTICK].m_pMeshData->m_pSubsets[0].mtrl.Specular.g = 0.1f;
    m_pFrames[CONTROL_RIGHT_THUMBSTICK].m_pMeshData->m_pSubsets[0].mtrl.Specular.b = 0.1f;

    m_pFrames[CONTROL_A_COVER].m_pMeshData->m_pSubsets[0].mtrl.Specular.r = 0.0f;
    m_pFrames[CONTROL_A_COVER].m_pMeshData->m_pSubsets[0].mtrl.Specular.g = 0.0f;
    m_pFrames[CONTROL_A_COVER].m_pMeshData->m_pSubsets[0].mtrl.Specular.b = 0.0f;
    m_pFrames[CONTROL_B_COVER].m_pMeshData->m_pSubsets[0].mtrl.Specular.r = 0.0f;
    m_pFrames[CONTROL_B_COVER].m_pMeshData->m_pSubsets[0].mtrl.Specular.g = 0.0f;
    m_pFrames[CONTROL_B_COVER].m_pMeshData->m_pSubsets[0].mtrl.Specular.b = 0.0f;
    m_pFrames[CONTROL_X_COVER].m_pMeshData->m_pSubsets[0].mtrl.Specular.r = 0.0f;
    m_pFrames[CONTROL_X_COVER].m_pMeshData->m_pSubsets[0].mtrl.Specular.g = 0.0f;
    m_pFrames[CONTROL_X_COVER].m_pMeshData->m_pSubsets[0].mtrl.Specular.b = 0.0f;
    m_pFrames[CONTROL_Y_COVER].m_pMeshData->m_pSubsets[0].mtrl.Specular.r = 0.0f;
    m_pFrames[CONTROL_Y_COVER].m_pMeshData->m_pSubsets[0].mtrl.Specular.g = 0.0f;
    m_pFrames[CONTROL_Y_COVER].m_pMeshData->m_pSubsets[0].mtrl.Specular.b = 0.0f;


    // Record which controls are active
    g_ControlActive[CONTROL_LEFT_THUMBSTICK] = ( pGamepad->wButtons & XINPUT_GAMEPAD_LEFT_THUMB ||
                                                 pGamepad->fX1 || pGamepad->fY1 ) ? TRUE : FALSE;
    g_ControlActive[CONTROL_RIGHT_THUMBSTICK] = ( pGamepad->wButtons & XINPUT_GAMEPAD_RIGHT_THUMB ||
                                                  pGamepad->fX2 || pGamepad->fY2 ) ? TRUE : FALSE;
    g_ControlActive[CONTROL_BACK_BUTTON] = ( pGamepad->wButtons & XINPUT_GAMEPAD_BACK ) ? TRUE : FALSE;
    g_ControlActive[CONTROL_START_BUTTON] = ( pGamepad->wButtons & XINPUT_GAMEPAD_START ) ? TRUE : FALSE;
    g_ControlActive[CONTROL_DPAD] = ( pGamepad->wButtons & XINPUT_GAMEPAD_DPAD_UP ||
                                      pGamepad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN ||
                                      pGamepad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT ||
                                      pGamepad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT ) ? TRUE : FALSE;
    g_ControlActive[CONTROL_A_BUTTON] = pGamepad->wButtons & XINPUT_GAMEPAD_A;
    g_ControlActive[CONTROL_B_BUTTON] = pGamepad->wButtons & XINPUT_GAMEPAD_B;
    g_ControlActive[CONTROL_X_BUTTON] = pGamepad->wButtons & XINPUT_GAMEPAD_X;
    g_ControlActive[CONTROL_Y_BUTTON] = pGamepad->wButtons & XINPUT_GAMEPAD_Y;
    g_ControlActive[CONTROL_LEFT_SHOULDER] = pGamepad->wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER;
    g_ControlActive[CONTROL_RIGHT_SHOULDER] = pGamepad->wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER;
    g_ControlActive[CONTROL_LEFT_TRIGGER] = pGamepad->bLeftTrigger;
    g_ControlActive[CONTROL_RIGHT_TRIGGER] = pGamepad->bRightTrigger;

    // Set base transforms
    for( DWORD i = 0; i < m_dwNumFrames; i++ )
    {
        m_pFrames[i].m_matTransform = g_ControlMatrix[i];
    }

    // Animate buttons
    {
        m_pFrames[CONTROL_LEFT_THUMBSTICK].m_matTransform *= XMMatrixTranslation( 0.0f,
                                                                                  (
                                                                                  pGamepad->wButtons &
                                                                                  XINPUT_GAMEPAD_LEFT_THUMB  ? -1.0f :
                                                                                  0.0f ), 0.0f );
        m_pFrames[CONTROL_RIGHT_THUMBSTICK].m_matTransform *= XMMatrixTranslation( 0.0f,
                                                                                   (
                                                                                   pGamepad->wButtons &
                                                                                   XINPUT_GAMEPAD_RIGHT_THUMB ? -1.0f :
                                                                                   0.0f ), 0.0f );
        m_pFrames[CONTROL_BACK_BUTTON].m_matTransform *= XMMatrixTranslation( 0.0f,
                                                                              ( pGamepad->wButtons &
                                                                                XINPUT_GAMEPAD_BACK  ? -1.0f : 0.0f ),
                                                                              0.0f );
        m_pFrames[CONTROL_START_BUTTON].m_matTransform *= XMMatrixTranslation( 0.0f,
                                                                               ( pGamepad->wButtons &
                                                                                 XINPUT_GAMEPAD_START ? -1.0f : 0.0f ),
                                                                               0.0f );
        m_pFrames[CONTROL_A_BUTTON].m_matTransform *= XMMatrixTranslation( 0.0f,
                                                                           ( pGamepad->wButtons &
                                                                             XINPUT_GAMEPAD_A ? -1.0f : 0.0f ), 0.0f );
        m_pFrames[CONTROL_B_BUTTON].m_matTransform *= XMMatrixTranslation( 0.0f,
                                                                           ( pGamepad->wButtons &
                                                                             XINPUT_GAMEPAD_B ? -1.0f : 0.0f ), 0.0f );
        m_pFrames[CONTROL_X_BUTTON].m_matTransform *= XMMatrixTranslation( 0.0f,
                                                                           ( pGamepad->wButtons &
                                                                             XINPUT_GAMEPAD_X ? -1.0f : 0.0f ), 0.0f );
        m_pFrames[CONTROL_Y_BUTTON].m_matTransform *= XMMatrixTranslation( 0.0f,
                                                                           ( pGamepad->wButtons &
                                                                             XINPUT_GAMEPAD_Y ? -1.0f : 0.0f ), 0.0f );
        m_pFrames[CONTROL_LEFT_SHOULDER].m_matTransform *= XMMatrixTranslation( 0.0f, 0.0f,
                                                                                ( pGamepad->wButtons &
                                                                                  XINPUT_GAMEPAD_LEFT_SHOULDER  ? -
                                                                                  1.0f : 0.0f ) );
        m_pFrames[CONTROL_RIGHT_SHOULDER].m_matTransform *= XMMatrixTranslation( 0.0f, 0.0f,
                                                                                 ( pGamepad->wButtons &
                                                                                   XINPUT_GAMEPAD_RIGHT_SHOULDER ? -
                                                                                   1.0f : 0.0f ) );
    }

    // Animate left trigger
    {
        XMVECTOR vAxis = XMLoadFloat3( &g_vLeftTriggerAxis );
        XMMATRIX matTrans1, matRotate, matTrans2, matAll;
        matTrans1 = XMMatrixTranslation( -g_vLeftTriggerOffset.x, -g_vLeftTriggerOffset.y, -g_vLeftTriggerOffset.z );
        matRotate = XMMatrixRotationAxis( vAxis, ( XM_PI / 12 ) * pGamepad->bLeftTrigger / 255.0f );
        matTrans2 = XMMatrixTranslation( g_vLeftTriggerOffset.x, g_vLeftTriggerOffset.y, g_vLeftTriggerOffset.z );
        matAll = XMMatrixMultiply( matTrans1, matRotate );
        matAll = XMMatrixMultiply( matAll, matTrans2 );
        m_pFrames[CONTROL_LEFT_TRIGGER].m_matTransform *= matAll;
    }

    // Animate right trigger
    {
        XMVECTOR vAxis = XMLoadFloat3( &g_vRightTriggerAxis );
        XMMATRIX matTrans1, matRotate, matTrans2, matAll;
        matTrans1 = XMMatrixTranslation( -g_vRightTriggerOffset.x, -g_vRightTriggerOffset.y,
                                         -g_vRightTriggerOffset.z );
        matRotate = XMMatrixRotationAxis( vAxis, ( XM_PI / 12 ) * pGamepad->bRightTrigger / 255.0f );
        matTrans2 = XMMatrixTranslation( g_vRightTriggerOffset.x, g_vRightTriggerOffset.y, g_vRightTriggerOffset.z );
        matAll = XMMatrixMultiply( matTrans1, matRotate );
        matAll = XMMatrixMultiply( matAll, matTrans2 );
        m_pFrames[CONTROL_RIGHT_TRIGGER].m_matTransform *= matAll;
    }

    // Animate DPAD
    {
        XMVECTOR vAxis = XMVectorZero();

        if( pGamepad->wButtons & XINPUT_GAMEPAD_DPAD_UP )    vAxis.x = +1.0f;
        if( pGamepad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN )  vAxis.x = -1.0f;
        if( pGamepad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT )  vAxis.z = +1.0f;
        if( pGamepad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT ) vAxis.z = -1.0f;

        if( vAxis.x || vAxis.y || vAxis.z )
        {
            XMMATRIX matTrans1, matRotate, matTrans2, matAll;
            matTrans2 = XMMatrixTranslation( -g_vDPadOffset.x, -g_vDPadOffset.y, -g_vDPadOffset.z );
            matRotate = XMMatrixRotationAxis( vAxis, XM_PI / 25 );
            matTrans1 = XMMatrixTranslation( m_pFrames[CONTROL_DPAD].m_matTransform._41 + g_vDPadOffset.x,
                                             m_pFrames[CONTROL_DPAD].m_matTransform._42 + g_vDPadOffset.y,
                                             m_pFrames[CONTROL_DPAD].m_matTransform._43 + g_vDPadOffset.z );
            m_pFrames[CONTROL_DPAD].m_matTransform._41 = 0.0f;
            m_pFrames[CONTROL_DPAD].m_matTransform._42 = 0.0f;
            m_pFrames[CONTROL_DPAD].m_matTransform._43 = 0.0f;
            m_pFrames[CONTROL_DPAD].m_matTransform = matTrans2 * matRotate * m_pFrames[CONTROL_DPAD].m_matTransform *
                matTrans1;
        }
    }

    // Animate left thumbstick
    {
        XMVECTOR vAxis = XMVectorZero();
        vAxis.x = +pGamepad->fY1;
        vAxis.z = -pGamepad->fX1;

        FLOAT fStickTrans = pGamepad->wButtons & XINPUT_GAMEPAD_LEFT_THUMB ? -1.0f : 0.0f;

        if( vAxis.x || vAxis.y || vAxis.z )
        {
            XMMATRIX matTrans1, matRotate, matTrans2, matAll;
            matTrans2 = XMMatrixTranslation( g_vLeftStickOffset.x, g_vLeftStickOffset.y, g_vLeftStickOffset.z );
            matRotate = XMMatrixRotationAxis( vAxis, XMVector3Length( vAxis ).x / 3 );
            matTrans1 = XMMatrixTranslation( m_pFrames[CONTROL_LEFT_THUMBSTICK].m_matTransform._41 -
                                             g_vLeftStickOffset.x,
                                             m_pFrames[CONTROL_LEFT_THUMBSTICK].m_matTransform._42 -
                                             g_vLeftStickOffset.y,
                                             m_pFrames[CONTROL_LEFT_THUMBSTICK].m_matTransform._43 -
                                             g_vLeftStickOffset.z );
            m_pFrames[CONTROL_LEFT_THUMBSTICK].m_matTransform._41 = 0.0f;
            m_pFrames[CONTROL_LEFT_THUMBSTICK].m_matTransform._42 = 0.0f;
            m_pFrames[CONTROL_LEFT_THUMBSTICK].m_matTransform._43 = 0.0f;
            m_pFrames[CONTROL_LEFT_THUMBSTICK].m_matTransform = matTrans2 * matRotate *
                m_pFrames[CONTROL_LEFT_THUMBSTICK].m_matTransform * matTrans1;
        }
        else
            m_pFrames[CONTROL_LEFT_THUMBSTICK].m_matTransform *= XMMatrixTranslation( 0.0f, fStickTrans, 0.0f );
    }

    // Animate right thumbstick
    {
        XMVECTOR vAxis = XMVectorZero();

        vAxis.x = +pGamepad->fY2;
        vAxis.z = -pGamepad->fX2;

        FLOAT fStickTrans = pGamepad->wButtons & XINPUT_GAMEPAD_RIGHT_THUMB ? -1.0f : 0.0f;

        if( vAxis.x || vAxis.y || vAxis.z )
        {
            XMMATRIX matTrans1, matRotate, matTrans2, matAll;
            matTrans2 = XMMatrixTranslation( g_vRightStickOffset.x, g_vRightStickOffset.y, g_vRightStickOffset.z );
            matRotate = XMMatrixRotationAxis( vAxis, XMVector3Length( vAxis ).x / 3 );
            matTrans1 = XMMatrixTranslation( m_pFrames[CONTROL_RIGHT_THUMBSTICK].m_matTransform._41 -
                                             g_vRightStickOffset.x,
                                             m_pFrames[CONTROL_RIGHT_THUMBSTICK].m_matTransform._42 -
                                             g_vRightStickOffset.y,
                                             m_pFrames[CONTROL_RIGHT_THUMBSTICK].m_matTransform._43 -
                                             g_vRightStickOffset.z );
            m_pFrames[CONTROL_RIGHT_THUMBSTICK].m_matTransform._41 = 0.0f;
            m_pFrames[CONTROL_RIGHT_THUMBSTICK].m_matTransform._42 = 0.0f;
            m_pFrames[CONTROL_RIGHT_THUMBSTICK].m_matTransform._43 = 0.0f;
            m_pFrames[CONTROL_RIGHT_THUMBSTICK].m_matTransform = matTrans2 * matRotate *
                m_pFrames[CONTROL_RIGHT_THUMBSTICK].m_matTransform * matTrans1;
        }
        else
            m_pFrames[CONTROL_RIGHT_THUMBSTICK].m_matTransform *= XMMatrixTranslation( 0.0f, fStickTrans, 0.0f );
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: RenderGamepad()
// Desc: Renders the gamepad mesh.
//--------------------------------------------------------------------------------------
HRESULT GamepadMesh::RenderGamepad()
{
    // Set some default state
    ATG::g_pd3dDevice->SetRenderState( D3DRS_MULTISAMPLEANTIALIAS, TRUE );
    ATG::g_pd3dDevice->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
    ATG::g_pd3dDevice->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
    ATG::g_pd3dDevice->SetRenderState( D3DRS_ZENABLE, TRUE );

    // Set shaders - the vertex shader is a custom shader that converts
    // camera-space normal to texture space for lookup in a spherical
    // environment map.  The pixel shader simply outputs the result of this
    // texture lookup.
    ATG::g_pd3dDevice->SetVertexShader( m_pVS );
    ATG::g_pd3dDevice->SetPixelShader( m_pPS );

    // Draw the object's opaque subsets
    ATG::g_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );
    Render( ATG::MESH_NOTEXTURES | ATG::MESH_OPAQUEONLY );

    // Draw the object's alpha subsets
    ATG::g_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, TRUE );
    ATG::g_pd3dDevice->SetRenderState( D3DRS_SRCBLEND, D3DBLEND_SRCALPHA );
    ATG::g_pd3dDevice->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA );
    Render( ATG::MESH_NOTEXTURES | ATG::MESH_ALPHAONLY );

    // Restore render states
    ATG::g_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );
    ATG::g_pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_CCW );
    ATG::g_pd3dDevice->SetRenderState( D3DRS_MULTISAMPLEANTIALIAS, FALSE );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: GamepadMesh()
// Desc: Initializes member variables
//--------------------------------------------------------------------------------------
GamepadMesh::GamepadMesh()
{
    m_pVS = NULL;
    m_pPS = NULL;
}


//--------------------------------------------------------------------------------------
// Name: ~GamepadMesh()
// Desc: Cleans up member variables
//--------------------------------------------------------------------------------------
GamepadMesh::~GamepadMesh()
{
    if( m_pVS )
    {
        m_pVS->Release();
        m_pVS = NULL;
    }
    if( m_pPS )
    {
        m_pPS->Release();
        m_pPS = NULL;
    }
}


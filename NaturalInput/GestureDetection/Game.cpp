//--------------------------------------------------------------------------------------
// Game.h
//
// Declares functions to show how a simple game can use the gesture detection filters
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include <xtl.h>
#include <AtgUtil.h>
#include <AtgSimpleShaders.h>
#include "Game.h"

//--------------------------------------------------------------------------------------
// Defines and consants
//--------------------------------------------------------------------------------------

const FLOAT g_fMaxDistance  = 5.0f; // Maximum offset distance from neutral when moving up or down

const FLOAT Biplane::c_fPropellerSpeed         = 0.5f;
const FLOAT Background::c_fTextureScrollSpeed  = 0.0015f;

ATG::Scene* Token::s_pScene    = NULL;
ATG::Model* Token::s_pModel    = NULL;
const FLOAT Token::c_fSpeed    = 0.5f;


//--------------------------------------------------------------------------------------
// Name: RenderModel()
// Desc: Renders an ATG model object given the world view projection matrix
//--------------------------------------------------------------------------------------

static VOID RenderModel( D3DDevice* pd3dDevice, ATG::Model* pModel, XMMATRIX matWorldViewProj )
{
    XMMATRIX matWVPTransposed = XMMatrixTranspose( matWorldViewProj );
    pd3dDevice->SetVertexShaderConstantF( 0, (FLOAT*)&matWVPTransposed, 4 );

    const XMVECTOR vAmbient     = XMVectorReplicate( 0.2f );
    const XMVECTOR vLightColor  = XMVectorSet( 1, 1, 0.9f, 1 );
    const XMVECTOR vLightDir    = XMVector3Normalize( XMVectorSet( -1, 1, 1, 0 ));
    const BOOL bEmissive        = TRUE;

    // Loop over mesh mappings.
    DWORD dwMeshMappingCount = pModel->GetNumMeshMappings();
    for ( DWORD dwMapIndex = 0; dwMapIndex < dwMeshMappingCount; dwMapIndex++ )
    {
        ATG::MeshMapping& mm = pModel->GetMeshMapping( dwMapIndex );
        ATG::BaseMesh* pMesh = mm.pMesh;

        // Loop over mesh subsets.
        DWORD dwSubsetCount = pMesh->GetNumSubsets();
        for ( DWORD dwSubsetIndex = 0; dwSubsetIndex < dwSubsetCount; dwSubsetIndex++ )
        {
            ATG::MaterialInstance* pMaterial = mm.Materials[ dwSubsetIndex ];

            // Set the FXLite material.
            pMaterial->BeginMaterialSinglePass( pd3dDevice );

            // Set shader constants
            pd3dDevice->SetVertexShaderConstantF( 0, (FLOAT*)&matWVPTransposed, 4 );
            pd3dDevice->SetPixelShaderConstantF( 0, (FLOAT*)&vAmbient, 1 );
            pd3dDevice->SetPixelShaderConstantF( 2, (FLOAT*)&vLightDir, 1 );
            pd3dDevice->SetPixelShaderConstantF( 3, (FLOAT*)&vLightColor, 1 );
            pd3dDevice->SetPixelShaderConstantB( 12, &bEmissive, 1 );
            
            // Render the mesh subset.
            pMesh->RenderSubset( dwSubsetIndex, pd3dDevice );

            // End the FXLite material.
            pMaterial->EndMaterialSinglePass();
        }
    }
}


//--------------------------------------------------------------------------------------
// Name: Game()
// Desc: Constructor
//--------------------------------------------------------------------------------------

Game::Game()
{
    m_matView = XMMatrixIdentity();
    m_matProj = XMMatrixIdentity();

    m_uScore = 0;
    m_uHighScore = 0;
    m_uCurrentSkeletonIdx = 0;
}


//--------------------------------------------------------------------------------------
// Name: Game::Initialize()
// Desc: Initialize the data
//--------------------------------------------------------------------------------------

HRESULT Game::Initialize( D3DDevice* pd3dDevice, ATG::PackedResource* pResource )
{
    // Initialize render objects
    RETURN_ON_FAIL( m_Biplane.Initialize( pd3dDevice, pResource ) );
    RETURN_ON_FAIL( m_Background.Initialize( pd3dDevice, pResource ) );
    RETURN_ON_FAIL( m_Tokens.Initialize( pd3dDevice, pResource ) );

    // Determine the aspect ratio
    UINT uWidth, uHeight;
    ATG::GetVideoSettings( &uWidth, &uHeight );
    FLOAT fAspectRatio = (FLOAT)uWidth / (FLOAT)uHeight;

    // Setup projection matrix
    m_matProj = XMMatrixPerspectiveFovLH( XM_PI / 4, fAspectRatio, 1.0f, 100.0f );

    // Setup view matrix
    XMVECTOR vEyePt = XMVectorSet( g_fMaxDistance * g_fMaxDistance, 0.0f, g_fMaxDistance, 0.0f );
    XMVECTOR vLookatPt = XMVectorSet( 0.0f, 0.0f, g_fMaxDistance, 0.0f );
    XMVECTOR vUpVec = XMVectorSet( 0.0f, 1.0f, 0.0f, 0.0f );
    m_matView = XMMatrixLookAtLH( vEyePt, vLookatPt, vUpVec );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Game::Reset()
// Desc: Reset data
//--------------------------------------------------------------------------------------

VOID Game::Reset()
{
    m_Biplane.Reset();
    m_Tokens.Reset();

    m_uScore = 0;
}


//--------------------------------------------------------------------------------------
// Name: Game::Update()
// Desc: Update data
//--------------------------------------------------------------------------------------

VOID Game::Update( EMovement eMovement )
{
    PIXBeginNamedEvent( 0, __FUNCTION__ );

    m_Biplane.Update( eMovement );
    m_Background.Update();

    BOOL bCollectToken = m_Tokens.Update( m_Biplane.GetPosition() );

    if ( bCollectToken )
    {
        m_uScore++;
        m_uHighScore = max( m_uHighScore, m_uScore );
    }

    PIXEndNamedEvent();
}


//--------------------------------------------------------------------------------------
// Name: Game::Render()
// Desc: Render the game
//--------------------------------------------------------------------------------------

VOID Game::Render()
{
    PIXBeginNamedEvent( 0, __FUNCTION__ );

    m_Background.Render();
    m_Biplane.Render( m_matView, m_matProj );
    m_Tokens.Render( m_matView, m_matProj );

    PIXEndNamedEvent();
}


//--------------------------------------------------------------------------------------
// Name: Biplane()
// Desc: Constructor
//--------------------------------------------------------------------------------------

Biplane::Biplane()
{
    m_matWorld          = XMMatrixIdentity();
    m_pd3dDevice        = NULL;
    m_pScene            = NULL;
    m_pBody             = NULL;
    m_pWings            = NULL;
    m_pPropeller        = NULL;
    m_fPosition         = 0.0f;
    m_fPropellerAngle   = 0.0f;
}


//--------------------------------------------------------------------------------------
// Name: Biplane::Initialize()
// Desc: Initialize data
//--------------------------------------------------------------------------------------

HRESULT Biplane::Initialize( D3DDevice* pd3dDevice, ATG::PackedResource* pResource )
{
    RETURN_ON_NULL( m_pd3dDevice = pd3dDevice );
    RETURN_ON_NULL( pResource );
    RETURN_ON_NULL( m_pScene = new ATG::Scene() );

    // Load the ubershader into the scene resource database.
    ATG::FXLiteMaterialImplementation::SetParameterPool( m_pScene->GetEffectParameterPool() );
    ATG::BaseMaterial* pUbershaderBaseMaterial = NULL;
    RETURN_ON_NULL( pUbershaderBaseMaterial =
        ATG::BaseMaterial::CreateFXLiteMaterial( L"Default", L"game:\\media\\effects\\ubershader_final.fxobj", L"Ubershader_Nested" ) );

    pUbershaderBaseMaterial->InitializeImplementation();
    pUbershaderBaseMaterial->ChangeDevice( m_pd3dDevice );
    
    m_pScene->GetResourceDatabase()->AddResource( pUbershaderBaseMaterial );

    // Create default resources.
    m_pScene->GetResourceDatabase()->CreateDefaultResources();

    RETURN_ON_FAIL( ATG::SceneFileParser::LoadXATGFile( "game:\\media\\scenes\\biplane.xatg", m_pScene, NULL, 0, NULL ) );
    RETURN_ON_NULL( m_pBody = (ATG::Model*)m_pScene->FindObjectOfType( L"Body", ATG::Model::TypeID ) );
    RETURN_ON_NULL( m_pWings = (ATG::Model*)m_pScene->FindObjectOfType( L"WingsEtc", ATG::Model::TypeID ) );
    RETURN_ON_NULL( m_pPropeller = (ATG::Model*)m_pScene->FindObjectOfType( L"Propeller", ATG::Model::TypeID ) );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Biplane::Reset()
// Desc: Reset data
//--------------------------------------------------------------------------------------

VOID Biplane::Reset()
{
    m_fPosition = 0.0f;
}


//--------------------------------------------------------------------------------------
// Name: Biplane::Update()
// Desc: Update data
//--------------------------------------------------------------------------------------

VOID Biplane::Update( const EMovement eMovement )
{
    PIXBeginNamedEvent( 0, __FUNCTION__ );

    const FLOAT fNumSteps   = 4.0f; // Number of steps to get from neutral to g_fMaxDistance
    const FLOAT fOffset     = g_fMaxDistance / fNumSteps;

    m_fPropellerAngle += c_fPropellerSpeed;

    switch ( eMovement )
    {
    case MOVEMENT_UP:
        m_fPosition = min( g_fMaxDistance, m_fPosition + fOffset );
        m_matWorld = XMMatrixTranslation( 0.0f, m_fPosition, 0.0f );
        break;

    case MOVEMENT_DOWN:
        m_fPosition = max( -g_fMaxDistance, m_fPosition - fOffset );
        m_matWorld = XMMatrixTranslation( 0.0f, m_fPosition, 0.0f );
        break;

    default:
        if ( fabsf( m_fPosition - 0.0f ) < 0.0001f )
        {
            m_fPosition = 0.0f;
        }
        else if ( m_fPosition > 0.0f )
        {
            m_fPosition -= fOffset;
        }
        else
        {
            m_fPosition += fOffset;
        }
        m_matWorld = XMMatrixTranslation( 0.0f, m_fPosition, 0.0f );
    }

    PIXEndNamedEvent();
}


//--------------------------------------------------------------------------------------
// Name: Biplane::Render()
// Desc: Render plane
//--------------------------------------------------------------------------------------

VOID Biplane::Render( XMMATRIX matView, XMMATRIX matProj )
{
    PIXBeginNamedEvent( 0, __FUNCTION__ );

    // Set default renderstate.
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_ZWRITEENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_ZFUNC, D3DCMP_LESSEQUAL );
    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );

    // Set some default textures for correct lighting.
    D3DBaseTexture* pWhiteTexture = m_pScene->GetResourceDatabase()->GetWhiteTexture()->GetD3DTexture();
    for( DWORD i = 8; i < 16; ++i )
    {
        m_pd3dDevice->SetTexture( i, pWhiteTexture );
    }

    // The propeller is centered at ( 0, 0.25, 3.25 ) in the content file
    XMMATRIX matPropeller = XMMatrixRotationZ( m_fPropellerAngle ) * XMMatrixTranslation( 0, 0.25f, 3.25f );

    XMMATRIX matWVP = m_matWorld * matView * matProj;

    RenderModel( m_pd3dDevice, m_pBody, matWVP );
    RenderModel( m_pd3dDevice, m_pWings, matWVP );
    RenderModel( m_pd3dDevice, m_pPropeller, matPropeller * matWVP );

    PIXEndNamedEvent();
}


//--------------------------------------------------------------------------------------
// Name: Background()
// Desc: Constructor
//--------------------------------------------------------------------------------------

Background::Background()
{
    m_pd3dDevice            = NULL;
    m_pTexture              = NULL;
    m_uBackBufferWidth      = 0;
    m_uBackBufferHeight     = 0;
    m_fTextureScroll        = 0.0f;
}


//--------------------------------------------------------------------------------------
// Name: Background::Initialize()
// Desc: Initialize data
//--------------------------------------------------------------------------------------

HRESULT Background::Initialize( D3DDevice* pd3dDevice, ATG::PackedResource* pResource )
{
    RETURN_ON_NULL( m_pd3dDevice = pd3dDevice );
    RETURN_ON_NULL( pResource );
    RETURN_ON_NULL( m_pTexture = pResource->GetTexture( "Sky" ) );
    
    ATG::GetVideoSettings( &m_uBackBufferWidth, &m_uBackBufferHeight );

    m_fTextureScroll = 0.0f;

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Background::Update()
// Desc: Update data
//--------------------------------------------------------------------------------------

VOID Background::Update()
{
    PIXBeginNamedEvent( 0, __FUNCTION__ );

    // Scroll the UV coordinate for the background texture
    m_fTextureScroll += c_fTextureScrollSpeed;

    PIXEndNamedEvent();
}


//--------------------------------------------------------------------------------------
// Name: Background::Render()
// Desc: Render background
//--------------------------------------------------------------------------------------

VOID Background::Render()
{
    PIXBeginNamedEvent( 0, __FUNCTION__ );

    ATG::MeshVertexPT* pVertexData = NULL;

    ATG::SimpleShaders::SetDeclPosTex();

    ATG::SimpleShaders::BeginShader_PreTransformed_Textured( m_pTexture );
    
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_ZWRITEENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_CCW );
    m_pd3dDevice->SetRenderState( D3DRS_VIEWPORTENABLE, FALSE );
   
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP );

    HRESULT hr = m_pd3dDevice->BeginVertices( D3DPT_RECTLIST, 3, sizeof( ATG::MeshVertexPT ), ( VOID** )&pVertexData );

    // The ring buffer may run out of space when tiling, doing z-prepasses,
    // or using BeginCommandBuffer. If so, make the buffer larger.
    if( FAILED( hr ) )
    {
        ATG::FatalError( "Ring buffer out of memory.\n" );
    }

    assert( pVertexData );

    // Top Left
    pVertexData[0].Position = XMFLOAT3( 0.0f, 0.0f, 0 );
    pVertexData[0].TexCoord = XMFLOAT2( 0.0f + m_fTextureScroll, 0.0f );

    // Top Right
    pVertexData[1].Position = XMFLOAT3( ( FLOAT )m_uBackBufferWidth, 0.0f, 0 );
    pVertexData[1].TexCoord = XMFLOAT2( 0.4f + m_fTextureScroll, 0.0f );

    // Bottom Left
    pVertexData[2].Position = XMFLOAT3( ( FLOAT )0, ( FLOAT )m_uBackBufferHeight, 0 );
    pVertexData[2].TexCoord = XMFLOAT2( 0.0f + m_fTextureScroll, 1.0f );

    m_pd3dDevice->EndVertices();

    ATG::SimpleShaders::EndShader();

    m_pd3dDevice->SetRenderState( D3DRS_VIEWPORTENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_ZWRITEENABLE, TRUE );

    PIXEndNamedEvent();
}


//--------------------------------------------------------------------------------------
// Name: Token()
// Desc: Constructor
//--------------------------------------------------------------------------------------

Token::Token()
{
    Reset();
    m_pd3dDevice = NULL;
}


//--------------------------------------------------------------------------------------
// Name: Token::Reset()
// Desc: Reset data
//--------------------------------------------------------------------------------------

VOID Token::Reset()
{
    const FLOAT fOffset = 2.0f;     // z offset to start rings off screen
    m_matWorld  = XMMatrixIdentity();
    m_vPosition = XMVectorSet( 0.0f, 0.0f, g_fMaxDistance * g_fMaxDistance + fOffset, 0.0f );
    m_bActive   = FALSE;
}


//--------------------------------------------------------------------------------------
// Name: Token::Initialize()
// Desc: Initialize data
//--------------------------------------------------------------------------------------

HRESULT Token::Initialize( D3DDevice* pd3dDevice, ATG::PackedResource* pResource )
{
    RETURN_ON_NULL( m_pd3dDevice = pd3dDevice );
    RETURN_ON_NULL( pResource );

    if ( !s_pScene )
    {
        RETURN_ON_NULL( s_pScene = new ATG::Scene() );

        // Load the ubershader into the scene resource database.
        ATG::FXLiteMaterialImplementation::SetParameterPool( s_pScene->GetEffectParameterPool() );
        ATG::BaseMaterial* pUbershaderBaseMaterial = NULL;
        RETURN_ON_NULL( pUbershaderBaseMaterial =
            ATG::BaseMaterial::CreateFXLiteMaterial( L"Default", L"game:\\media\\effects\\ubershader_final.fxobj", L"Ubershader_Nested" ) );

        pUbershaderBaseMaterial->InitializeImplementation();
        pUbershaderBaseMaterial->ChangeDevice( m_pd3dDevice );
        
        s_pScene->GetResourceDatabase()->AddResource( pUbershaderBaseMaterial );
        s_pScene->GetResourceDatabase()->CreateDefaultResources();

        RETURN_ON_FAIL( ATG::SceneFileParser::LoadXATGFile( "game:\\media\\scenes\\ring.xatg", s_pScene, NULL, 0, NULL ) );

        if ( !s_pModel )
        {
            RETURN_ON_NULL( s_pModel = (ATG::Model*)s_pScene->FindObjectOfType( L"Ring", ATG::Model::TypeID ) );
        }
    }   

    Reset();

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Token::Spawn()
// Desc: Spawn a new token
//--------------------------------------------------------------------------------------

VOID Token::Spawn()
{
    // Spawn the height, either 1, 0, or -1
    m_vPosition.y   = ( rand() % 3 ) - 1.0f;
    m_vPosition.y   *= g_fMaxDistance;

    const FLOAT fOffset = 0.5f;     // Y offset to lift rings to center them around plane wings
    m_vPosition.y   += fOffset;

    // Set to active so that it will get updated
    m_bActive       = TRUE;
}


//--------------------------------------------------------------------------------------
// Name: Token::Update()
// Desc: Update data
//--------------------------------------------------------------------------------------

BOOL Token::Update( XMVECTOR vTargetPosition )
{
    PIXBeginNamedEvent( 0, __FUNCTION__ );

    BOOL bHitTarget = FALSE;

    if ( m_bActive )
    {
        m_vPosition.z -= c_fSpeed;

        // Simple bounding box check
        bHitTarget = fabsf( m_vPosition.x - vTargetPosition.x ) < 0.1f &&
                     fabsf( m_vPosition.y - vTargetPosition.y ) < 1.0f &&
                     fabsf( m_vPosition.z - vTargetPosition.z - 1.0f ) < 2.5f;

        if ( bHitTarget ||
             m_vPosition.z  < ( -g_fMaxDistance * g_fMaxDistance ) )
        {
            Reset();
        }
    }

    // Scale the tokens wider so that plane's wings can fit through them
    m_matWorld = XMMatrixScaling( 0.4f, 0.25f, 0.25f ) * XMMatrixTranslation( m_vPosition.x, m_vPosition.y, m_vPosition.z );

    PIXEndNamedEvent();

    return bHitTarget;
}


//--------------------------------------------------------------------------------------
// Name: Token::Render()
// Desc: Render the token
//--------------------------------------------------------------------------------------

VOID Token::Render( XMMATRIX matView, XMMATRIX matProj )
{
    PIXBeginNamedEvent( 0, __FUNCTION__ );

    if ( m_bActive )
    {
        XMMATRIX matWVP = m_matWorld * matView * matProj;
        RenderModel( m_pd3dDevice, s_pModel, matWVP );
    }

    PIXEndNamedEvent();
}


//--------------------------------------------------------------------------------------
// Name: Tokens()
// Desc: Constructor
//--------------------------------------------------------------------------------------

Tokens::Tokens()
{
    Reset();
}


//--------------------------------------------------------------------------------------
// Name: Tokens::Reset()
// Desc: Reset data
//--------------------------------------------------------------------------------------

VOID Tokens::Reset()
{
    m_Tokens.resize( m_uMaxTokens );

    UINT uNumTokens = m_Tokens.size();
    for ( UINT i = 0; i < uNumTokens; i++ )
    {
        m_Tokens[ i ].Reset();
    }

    m_uFrameNumber = 0;
}


//--------------------------------------------------------------------------------------
// Name: Tokens::Initialize()
// Desc: Initialize data
//--------------------------------------------------------------------------------------

HRESULT Tokens::Initialize( D3DDevice* pd3dDevice, ATG::PackedResource* pResource )
{
    Reset();

    UINT uNumTokens = m_Tokens.size();
    for ( UINT i = 0; i < uNumTokens; i++ )
    {
        m_Tokens[ i ].Initialize( pd3dDevice, pResource );
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Tokens::Update()
// Desc: Update data
//--------------------------------------------------------------------------------------

BOOL Tokens::Update( XMVECTOR vTargetPosition )
{
    PIXBeginNamedEvent( 0, __FUNCTION__ );

    BOOL bHitTarget = FALSE;
    UINT uNumTokens = m_Tokens.size();

    // Update all tokens
    for ( UINT i = 0; i < uNumTokens; i++ )
    {
        bHitTarget |= m_Tokens[ i ].Update( vTargetPosition );
    }

    // Check if we can spawn a new token
    m_uFrameNumber++;
    BOOL bSpawnNewToken = !( m_uFrameNumber % m_uSpawnFrameRate );
    for ( UINT i = 0; i < uNumTokens; i++ )
    {
        if ( bSpawnNewToken &&
             !m_Tokens[ i ].IsActive() )
        {
            m_Tokens[ i ].Spawn();
            break;
        }
    }

    PIXEndNamedEvent();

    return bHitTarget;
}


//--------------------------------------------------------------------------------------
// Name: Tokens::Render()
// Desc: Render tokens
//--------------------------------------------------------------------------------------

VOID Tokens::Render( XMMATRIX matView, XMMATRIX matProj )
{
    PIXBeginNamedEvent( 0, __FUNCTION__ );

    UINT uNumTokens = m_Tokens.size();
    for ( UINT i = 0; i < uNumTokens; i++ )
    {
        m_Tokens[ i ].Render( matView, matProj );
    }

    PIXEndNamedEvent();
}

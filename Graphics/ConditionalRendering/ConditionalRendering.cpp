//--------------------------------------------------------------------------------------
// ConditionalRendering.cpp
//
// This sample demonstrates the usage of the Direct3D conditional rendering APIs.
//
// Conditional rendering is performed in two passes - a survey rendering pass, and a
// conditional rendering pass.  In the first pass, survey geometry is rendered within
// a BeginConditionalSurvey/EndConditionalSurvey bracket.  The GPU will determine if
// any pixels from this survey are visible, and record the result in one of 64 survey
// records.  The survey geometry can be anything, from scene geometry itself to a 
// simpler yet representative impostor like the one used in this sample.
//
// The second pass is the conditional rendering.  The actual scene geometry is rendered
// using the survey index used for the survey pass.  If the survey recorded successful
// pixels, the conditional geometry is rendered.  The conditional rendering is performed
// inside a BeginConditionalRendering/EndConditionalRendering bracket.
//
// Unlike traditional Direct3D visibility queries, conditional surveys are recorded and
// tracked entirely within the GPU.  No results are sent to or are available to the CPU.
// This frees the title from having to synchronize or cache query results.
//
// Microsoft Game Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include <xtl.h>
#include <xgraphics.h>
#include <xboxmath.h>
#include <vector>
#include <AtgFont.h>
#include <AtgInput.h>
#include <AtgUtil.h>
#include <AtgHelp.h>
#include <AtgApp.h>
#include <AtgSimpleShaders.h>
#include <AtgDebugDraw.h>
#include <AtgSceneAll.h>

#include "ParameterPool.h"

//--------------------------------------------------------------------------------------
// Callouts for labelling the gamepad on the help screen
//--------------------------------------------------------------------------------------
ATG::HELP_CALLOUT g_HelpCallouts[] =
{
    { ATG::HELP_BACK_BUTTON,  ATG::HELP_PLACEMENT_2, L"Display\nhelp" },
    { ATG::HELP_B_BUTTON,     ATG::HELP_PLACEMENT_1, L"Change survey\ngeometry type" },
    { ATG::HELP_A_BUTTON,     ATG::HELP_PLACEMENT_1, L"Toggle conditional\nrendering" },
    { ATG::HELP_X_BUTTON,     ATG::HELP_PLACEMENT_1, L"Toggle animation" },
};
#define NUM_HELP_CALLOUTS (sizeof(g_HelpCallouts) / sizeof(g_HelpCallouts[0]))


//--------------------------------------------------------------------------------------
// Name: class Sample
// Desc: The skinned character sample class.
//--------------------------------------------------------------------------------------
class Sample : public ATG::Application
{
public:
    HRESULT Initialize();
    HRESULT Update();
    HRESULT Render();

private:
    VOID    SetupLights( ATG::Model* pModel );
    VOID    RenderModel( ATG::Model* pModel );
    VOID    RenderModelImpostor( ATG::Model* pModel );
    VOID    RenderModelZPass( ATG::Model* pModel );
    VOID    RenderUI();

private:
    // Sample framework objects
    ATG::Font m_Font;
    ATG::Timer m_Timer;
    ATG::Help m_Help;
    BOOL m_bDrawHelp;
    FLOAT m_fDeltaTime;

    // Scene members
    ATG::Scene* m_pScene;
    UbershaderParameterPool m_ParameterPool;
    ATG::Camera* m_pCamera;
    ATG::Frame* m_pCharacterRootFrame;
    std::vector <ATG::Model*> m_SceneModelVector;
    std::vector <ATG::Model*> m_CharacterModelVector;
    std::vector <ATG::PointLight*> m_ScenePointLights;
    XMMATRIX m_matVP;

    // Sample options
    BOOL m_bSurveyRenderImpostor;
    BOOL m_bEnableSurvey;
    BOOL m_bEnableAnimation;
};


//--------------------------------------------------------------------------------------
// Name: main()
// Desc: Entry point to the sample.
//--------------------------------------------------------------------------------------
VOID __cdecl main()
{
    Sample CRSample;
    ATG::GetVideoSettings( &CRSample.m_d3dpp.BackBufferWidth, &CRSample.m_d3dpp.BackBufferHeight );
    CRSample.Run();
}


//--------------------------------------------------------------------------------------
// Name: Initialize()
// Desc: Initializes the objects for the sample, and loads various resources.
//--------------------------------------------------------------------------------------
HRESULT Sample::Initialize()
{
    // Initialize member variables.
    m_bDrawHelp = FALSE;
    m_bSurveyRenderImpostor = TRUE;
    m_bEnableSurvey = TRUE;
    m_bEnableAnimation = TRUE;

    // Create font.
    if( FAILED( m_Font.Create( "game:\\Media\\Fonts\\Arial_16.xpr" ) ) )
        ATG::FatalError( "Could not load font." );

    // Confine text drawing to the title safe area
    m_Font.SetWindow( ATG::GetTitleSafeArea() );

    // Create help screen.
    if( FAILED( m_Help.Create( "game:\\Media\\Help\\Help.xpr" ) ) )
        ATG::FatalError( "Could not load help resources." );

    // Initialize simple shaders.
    ATG::SimpleShaders::Initialize( NULL, NULL );

    // Create scene object.
    m_pScene = new ATG::Scene();

    // Load the ubershader into the scene resource database.
    ATG::FXLiteMaterialImplementation::SetParameterPool( m_pScene->GetEffectParameterPool() );
    ATG::BaseMaterial* pUbershaderBaseMaterial = ATG::BaseMaterial::CreateFXLiteMaterial( L"Default",
                                                                                          L"game:\\media\\effects\\ubershader_final.fxobj", L"Ubershader_Nested" );
    pUbershaderBaseMaterial->InitializeImplementation();
    pUbershaderBaseMaterial->ChangeDevice( m_pd3dDevice );
    ATG::ResourceDatabase* pRDB = m_pScene->GetResourceDatabase();
    pRDB->AddResource( pUbershaderBaseMaterial );

    // Bind the parameter pool interface to the FXLite parameter pool.
    // This allows us to easily set the shader constants for the ubershader.
    m_ParameterPool.Initialize( m_pScene->GetEffectParameterPool() );

    // Create default resources.
    pRDB->CreateDefaultResources();

    // Load the scene file.
    HRESULT hr = ATG::SceneFileParser::LoadXATGFile( "game:\\media\\scenes\\ConditionalRendering.xatg", m_pScene, NULL,
                                                     0, NULL );
    if( FAILED( hr ) )
        ATG::FatalError( "Could not load scene file." );

    // Create a frame which will be the root of the character.
    m_pCharacterRootFrame = new ATG::Frame( L"CharacterRoot", XMMatrixIdentity() );
    m_pScene->AddChild( m_pCharacterRootFrame );

    // Load the character scene file, rooted at the new frame we just created.
    hr = ATG::SceneFileParser::LoadXATGFile( "game:\\media\\scenes\\SkinnedCharacter.xatg", m_pScene,
                                             m_pCharacterRootFrame, 0, NULL );
    if( FAILED( hr ) )
        ATG::FatalError( "Could not load scene file." );

    // Find all of the point lights in the scene and store pointers to them in a vector.
    ATG::NameIndexedCollection::iterator iter;
    for( iter = m_pScene->GetInstanceList()->begin(); iter != m_pScene->GetInstanceList()->end(); iter++ )
    {
        if( ( *iter )->IsDerivedFrom( ATG::PointLight::TypeID ) )
        {
            ATG::PointLight* pLight = ( ATG::PointLight* )( *iter );
            m_ScenePointLights.push_back( pLight );
        }
    }

    // Find models that represent the scene.
    const WCHAR* strSceneModels[] =
    {
        L"pPlane1", L"polySurface2"
    };
    for( DWORD i = 0; i < ARRAYSIZE( strSceneModels ); ++i )
    {
        ATG::Model* pModel = ( ATG::Model* )m_pScene->FindObjectOfType( strSceneModels[i], ATG::Model::TypeID );
        if( pModel != NULL )
            m_SceneModelVector.push_back( pModel );
    }
    // Find models that represent the character.
    const WCHAR* strCharacterModels[] =
    {
        L"Head", L"body", L"L_eyeBall", L"R_eyeBall"
    };
    for( DWORD i = 0; i < ARRAYSIZE( strCharacterModels ); ++i )
    {
        ATG::Model* pModel = ( ATG::Model* )m_pScene->FindObjectOfType( strCharacterModels[i], ATG::Model::TypeID );
        if( pModel != NULL )
            m_CharacterModelVector.push_back( pModel );
    }

    // Find the camera.
    m_pCamera = ( ATG::Camera* )m_pScene->FindObjectOfType( L"camera1", ATG::Camera::TypeID );
    if( m_pCamera == NULL )
    {
        ATG::FatalError( "Could not find camera." );
    }
    else
    {
        // Set up the projection matrix for the camera.
        FLOAT fAspect = ( FLOAT )m_d3dpp.BackBufferWidth / ( FLOAT )m_d3dpp.BackBufferHeight;
        ATG::Projection proj;
        proj.SetFovYAspect( XM_PIDIV4, fAspect, 0.01f, 100.0f );
        m_pCamera->SetProjection( proj );
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Update()
// Desc: Gets controller input and updates the camera.
//--------------------------------------------------------------------------------------
HRESULT Sample::Update()
{
    // Update FPS in the timer class.
    m_Timer.MarkFrame();

    // Get frame delta time.
    m_fDeltaTime = ( FLOAT )m_Timer.GetElapsedTime();

    // Get input from controllers (check for exit button sequence).
    ATG::GAMEPAD* pGamepad = ATG::Input::GetMergedInput();

    // The Back button toggles the help screen.
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_BACK )
        m_bDrawHelp = !m_bDrawHelp;

    // The B button toggles the survey geometry type.
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_B )
        m_bSurveyRenderImpostor = !m_bSurveyRenderImpostor;

    // The A button enables/disables conditional rendering.
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_A )
        m_bEnableSurvey = !m_bEnableSurvey;

    // The X button pauses the animation.
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_X )
        m_bEnableAnimation = !m_bEnableAnimation;

    // Update camera view * projection matrix.
    m_matVP = m_pCamera->GetWorldView() * m_pCamera->GetProjection().GetMatrix();

    // Set camera matrix into the debug draw system.
    ATG::DebugDraw::SetViewProjection( m_matVP );

    // Animate the position of the character.
    static FLOAT fTime = 0;
    if( m_bEnableAnimation )
        fTime += m_fDeltaTime;
    FLOAT fXPos = 0.8f * sinf( fTime * 0.5f );
    XMMATRIX matCharacter = XMMatrixTranslation( fXPos, 0.0f, 0.5f );
    m_pCharacterRootFrame->SetLocalTransform( matCharacter );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: SetupLights()
// Desc: Collides all point light volumes with the model volume, and places their 
//       settings into shader constants.
//--------------------------------------------------------------------------------------
VOID Sample::SetupLights( ATG::Model* pModel )
{
    // Compute inverse world transform for the model.
    XMVECTOR vDummy;
    XMMATRIX matInvWorld = XMMatrixInverse( &vDummy, pModel->GetWorldTransform() );

    const ATG::Bound& ModelBound = pModel->GetWorldBound();

    DWORD dwLightIndex = 0;

    // Loop over the point light vector.
    DWORD dwLightCount = ( DWORD )m_ScenePointLights.size();
    for( DWORD i = 0; i < dwLightCount; ++i )
    {
        // Collide point light with model.
        ATG::PointLight* pLight = m_ScenePointLights[i];
        if( !ModelBound.Collide( pLight->GetWorldBound() ) )
            continue;

        // Transform light position into object space.
        XMVECTOR vObjLightPos;
        vObjLightPos = XMVector3TransformCoord( pLight->GetWorldPosition(), matInvWorld );
        vObjLightPos.w = 1.0f / ( pLight->GetWorldRange() );

        // Set point light position and color into shader constants.
        m_ParameterPool.SetPointLightPosition( dwLightIndex, vObjLightPos );
        m_ParameterPool.SetPointLightColor( dwLightIndex, pLight->GetColor() );

        ++dwLightIndex;
    }

    // Set the total number of point lights into the shader.
    m_ParameterPool.SetPointLightCount( dwLightIndex );

    // No other light types are supported in this sample.
    m_ParameterPool.SetSpotLightCount( 0 );
    m_ParameterPool.SetShadowedSpotLightCount( 0 );
    m_ParameterPool.SetDirLightCount( 0 );

    // Set ambient light.
    m_ParameterPool.SetAmbient( XMVectorReplicate( 0.1f ) );
}


//--------------------------------------------------------------------------------------
// Name: RenderModelImpostor()
// Desc: Draws a model's bounding box.
//--------------------------------------------------------------------------------------
VOID Sample::RenderModelImpostor( ATG::Model* pModel )
{
    XMMATRIX matWorld;
    if( pModel->GetWorldBound().GetType() == ATG::Bound::AABB_Bound )
    {
        // AABB bound.
        const ATG::AxisAlignedBox& box = pModel->GetWorldBound().GetAabb();
        // Build a transform matrix corresponding to the AABB extents.
        matWorld = XMMatrixScaling( box.Extents.x, box.Extents.y, box.Extents.z );
        XMVECTOR position = XMLoadFloat3( &box.Center );
        matWorld.r[3] = XMVectorSelect( matWorld.r[3], position, XMVectorSelectControl( 1, 1, 1, 0 ) );
    }
    else if( pModel->GetWorldBound().GetType() == ATG::Bound::OBB_Bound )
    {
        // OBB bound.
        const ATG::OrientedBox& obb = pModel->GetWorldBound().GetObb();
        // Build a transform matrix corresponding to the OBB extents and orientation.
        matWorld = XMMatrixRotationQuaternion( XMLoadFloat4( &obb.Orientation ) );
        XMMATRIX matScale = XMMatrixScaling( obb.Extents.x, obb.Extents.y, obb.Extents.z );
        matWorld = XMMatrixMultiply( matScale, matWorld );
        XMVECTOR position = XMLoadFloat3( &obb.Center );
        matWorld.r[3] = XMVectorSelect( matWorld.r[3], position, XMVectorSelectControl( 1, 1, 1, 0 ) );
    }

    static const XMFLOAT3 Points[] =
    {
        XMFLOAT3( -1, -1, -1 ),
        XMFLOAT3( 1, -1, -1 ),
        XMFLOAT3( 1, -1, 1 ),
        XMFLOAT3( -1, -1, 1 ),
        XMFLOAT3( -1, 1, -1 ),
        XMFLOAT3( 1, 1, -1 ),
        XMFLOAT3( 1, 1, 1 ),
        XMFLOAT3( -1, 1, 1 ),
    };

    static const WORD Indices[] =
    {
        4, 5, 1, 0,
        6, 7, 3, 2,
        5, 6, 2, 1,
        7, 4, 0, 3,
        0, 1, 2, 3,
        7, 6, 5, 4
    };

    // Set render state.
    m_pd3dDevice->SetRenderState( D3DRS_ZWRITEENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_CCW );
    m_pd3dDevice->SetRenderState( D3DRS_FILLMODE, D3DFILL_SOLID );

    // Draw the 6 quads, using double-depth mode.
    ATG::SimpleShaders::BeginShader_Transformed_DepthOnly( matWorld * m_matVP );
    m_pd3dDevice->DrawIndexedPrimitiveUP( D3DPT_QUADLIST, 0, 8, 6, Indices, D3DFMT_INDEX16,
                                          Points, sizeof( XMFLOAT3 ) );
    ATG::SimpleShaders::EndShader();

    // Restore render state.
    m_pd3dDevice->SetRenderState( D3DRS_ZWRITEENABLE, TRUE );
}


//--------------------------------------------------------------------------------------
// Name: RenderModel()
// Desc: Draws a model.  The models are bound to materials specified in the content file.
//--------------------------------------------------------------------------------------
VOID Sample::RenderModel( ATG::Model* pModel )
{
    // Setup lighting for this model.
    SetupLights( pModel );

    // Compute world * view * projection matrix for this model and set into constants.
    XMMATRIX matWVP = pModel->GetWorldTransform() * m_matVP;
    m_ParameterPool.SetWorldViewProjMatrix( matWVP );

    // Compute object space position and direction for camera and set into constants.
    XMVECTOR vDummy;
    XMMATRIX matInvWorld = XMMatrixInverse( &vDummy, pModel->GetWorldTransform() );
    XMVECTOR vObjViewDir, vObjViewPos;
    vObjViewDir = XMVector3Normalize( XMVector3Transform( m_pCamera->GetWorldDirection(), matInvWorld ) );
    m_ParameterPool.SetObjectViewDirection( vObjViewDir );
    vObjViewPos = XMVector3TransformCoord( m_pCamera->GetWorldPosition(), matInvWorld );
    m_ParameterPool.SetObjectViewPosition( vObjViewPos );

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

            // Set the FXLite material.
            pMaterial->BeginMaterialSinglePass( m_pd3dDevice );
            // Render the mesh subset.
            pMesh->RenderSubset( dwSubsetIndex, m_pd3dDevice );
            // End the FXLite material.
            pMaterial->EndMaterialSinglePass();
        }
    }
}


//--------------------------------------------------------------------------------------
// Name: RenderModelZPass()
// Desc: Draws a model.  The materials are overridden with a simple depth-only shader.
//--------------------------------------------------------------------------------------
VOID Sample::RenderModelZPass( ATG::Model* pModel )
{
    // Compute world * view * projection matrix for this model and set into constants.
    XMMATRIX matWVP = pModel->GetWorldTransform() * m_matVP;

    // Begin depth-only rendering.
    ATG::SimpleShaders::BeginShader_Transformed_DepthOnly( matWVP );

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
            // Render the mesh subset.
            pMesh->RenderSubset( dwSubsetIndex, m_pd3dDevice );
        }
    }

    // End shader.
    ATG::SimpleShaders::EndShader();
}


//--------------------------------------------------------------------------------------
// Name: RenderUI()
// Desc: Draws some statistics and other UI elements
//--------------------------------------------------------------------------------------
VOID Sample::RenderUI()
{
    if( m_bDrawHelp )
    {
        m_Help.Render( &m_Font, g_HelpCallouts, NUM_HELP_CALLOUTS );
    }
    else
    {
        // Draw a title and FPS indicator.
        m_Font.Begin();
        m_Font.SetScaleFactors( 1.2f, 1.2f );
        m_Font.DrawText( 0, 0, 0xffffffff, L"ConditionalRendering" );
        m_Font.SetScaleFactors( 1.0f, 1.0f );
        m_Font.DrawText( 0, 0, 0xffffff00, m_Timer.GetFrameRate(), ATGFONT_RIGHT );

        // Draw some settings UI.
        if( m_bEnableSurvey )
        {
            m_Font.DrawText( 0, 40, 0xFF00C0C0, L"Survey Geometry:" );
            if( m_bSurveyRenderImpostor )
                m_Font.DrawText( 190, 40, 0xFF00FFFF, L"Impostor" );
            else
                m_Font.DrawText( 190, 40, 0xFF00FFFF, L"Z Pass Mesh" );

            // Draw character body part names, using conditional survey results.
            m_Font.DrawText( 0, 65, 0xFF00C0C0, L"Visible Body Parts:" );
            const WCHAR* strModelDisplayNames[] = { L"Head", L"Body", L"Left Eye", L"Right Eye" };
            for( DWORD i = 0; i < ARRAYSIZE( strModelDisplayNames ); ++i )
            {
                m_pd3dDevice->BeginConditionalRendering( i );
                m_Font.DrawText( 0, 90 + 25 * ( FLOAT )i, 0xFF00FFFF, strModelDisplayNames[i] );
                m_pd3dDevice->EndConditionalRendering();
            }
        }

        m_Font.End();
    }
}


//--------------------------------------------------------------------------------------
// Name: Render()
// Desc: Renders the scene and UI.
//--------------------------------------------------------------------------------------
HRESULT Sample::Render()
{
    m_pd3dDevice->BeginScene();

    m_pd3dDevice->Clear( 0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, 0, 1.0f, 0 );

    // Set default renderstate.
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_ZFUNC, D3DCMP_LESSEQUAL );
    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );

    // Set some default textures.
    D3DBaseTexture* pWhiteTexture = m_pScene->GetResourceDatabase()->GetWhiteTexture()->GetD3DTexture();
    for( DWORD i = 8; i < 16; ++i )
        m_pd3dDevice->SetTexture( i, pWhiteTexture );

    // Draw the background scene.
    DWORD dwModelCount = m_SceneModelVector.size();
    for( DWORD i = 0; i < dwModelCount; ++i )
    {
        RenderModel( m_SceneModelVector[i] );
    }

    if( m_bEnableSurvey )
    {
        // Draw the survey geometry for the character.
        // Begin/End ConditionalSurvey APIs are used to record the survey geometry.
        dwModelCount = m_CharacterModelVector.size();
        for( DWORD i = 0; i < dwModelCount; ++i )
        {
            // BeginConditionalSurvey starts a survey rendering.
            // One survey is used for each body part of the character.
            // D3DSURVEYBEGIN_CULLGEOMETRY ensures that the survey geometry will not affect
            // the rendertarget or depth buffer.
            m_pd3dDevice->BeginConditionalSurvey( i, D3DSURVEYBEGIN_CULLGEOMETRY );

            // Render the model.  Note that multiple DrawIndexedPrimitive calls can be
            // performed within a single survey.
            if( m_bSurveyRenderImpostor )
                RenderModelImpostor( m_CharacterModelVector[i] );
            else
                RenderModelZPass( m_CharacterModelVector[i] );

            // End the conditional survey.
            m_pd3dDevice->EndConditionalSurvey( 0 );
        }

        // Draw the character, conditioned on the survey results recorded earlier.
        for( DWORD i = 0; i < dwModelCount; ++i )
        {
            // BeginConditionalRendering predicates the following rendering based on
            // a specific survey result.  If the survey is not complete yet, the GPU will
            // perform the conditional rendering.
            m_pd3dDevice->BeginConditionalRendering( i );
            RenderModel( m_CharacterModelVector[i] );
            m_pd3dDevice->EndConditionalRendering();
        }
    }
    else
    {
        dwModelCount = m_CharacterModelVector.size();
        // Draw the character.
        for( DWORD i = 0; i < dwModelCount; ++i )
        {
            RenderModel( m_CharacterModelVector[i] );
        }
    }

    // Render UI.
    RenderUI();

    m_pd3dDevice->EndScene();

    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}

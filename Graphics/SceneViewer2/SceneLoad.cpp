//--------------------------------------------------------------------------------------
// SceneLoad.cpp
//
// Contains methods used during scene loading.  Most of this code runs on a loader
// thread.
// 
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include "SceneViewer2.h"

Task            g_LoaderTask;
BOOL            g_bAsyncLoadingInProgress = FALSE;
CHAR g_strAsyncLoadFilename[MAX_PATH];
extern DWORD    g_dwLoaderThreadID;

//--------------------------------------------------------------------------------------
// Name: LoaderThreadEntry()
// Desc: Entry point for the threaded loader.
//--------------------------------------------------------------------------------------
DWORD WINAPI LoaderThreadEntry( LPVOID pParam )
{
    ATG::SetThreadName( ( DWORD )-1, "SceneViewer2 Load Thread" );
    SceneViewer* pSceneViewer = ( SceneViewer* )pParam;
    while( true )
    {
        // Wait for task event to be fired.
        g_LoaderTask.WaitForBegin();

        // Set the loading in progress flag.
        g_bAsyncLoadingInProgress = TRUE;

        // Start loading the scene.
        pSceneViewer->LoadScene( g_strAsyncLoadFilename );

        // We're done loading, clear the loading in progress flag.
        g_bAsyncLoadingInProgress = FALSE;

        // Loading task is complete.
        g_LoaderTask.TaskCompleted();
    }
    return 0;
}


//--------------------------------------------------------------------------------------
// Name: ReloadScene()
// Desc: Triggers an asynchronous reload of the current scene.
//--------------------------------------------------------------------------------------
HRESULT SceneViewer::ReloadScene()
{
    if( m_pScene == NULL )
        return E_FAIL;
    if( strlen( m_strSceneFileName ) == 0 )
        return E_FAIL;
    return LoadSceneAsync( m_strSceneFileName );
}


VOID SceneViewer::InitializeSceneDefaultAssets( ATG::Scene* pScene )
{
    ATG::ResourceDatabase* pRDB = pScene->GetResourceDatabase();

    // Create default resources for the scene.
    pRDB->CreateDefaultResources();

    ATG::FXLiteMaterialImplementation::SetParameterPool( pScene->GetEffectParameterPool() );
    ATG::FXLiteMaterialImplementation::SetNullSamplerTexture( ( D3DTexture* )pRDB->GetBlackTexture()->GetD3DTexture
                                                              () );

    // Set media root path for loading the default shaders.
    ATG::BaseMaterial::SetMediaRootPath( "game:\\media\\" );

    // Load the ubershader as the default shader.
    m_pUbershaderBaseMaterial = ATG::BaseMaterial::CreateFXLiteMaterial( L"Default", L"ubershader_final.fxobj",
                                                                         L"Ubershader_Nested" );
    m_pUbershaderBaseMaterial->InitializeImplementation();
    m_pUbershaderBaseMaterial->ChangeDevice( m_pd3dDevice );
    pRDB->AddResource( m_pUbershaderBaseMaterial );

    // Load the layered ubershader as the default layered shader.
    m_pLayeredBaseMaterial = ATG::BaseMaterial::CreateFXLiteMaterial( L"DefaultLayered", L"ubershader_final.fxobj",
                                                                      L"Ubershader_MultiTexture" );
    m_pLayeredBaseMaterial->InitializeImplementation();
    m_pLayeredBaseMaterial->ChangeDevice( m_pd3dDevice );
    pRDB->AddResource( m_pLayeredBaseMaterial );

    // Load shaders for deferred, pass-per-light, and shader library modes.
    m_pPassPerLightBaseMaterial = ATG::BaseMaterial::CreateFXLiteMaterial( L"PassPerLight", L"passperlight.fxobj" );
    m_pPassPerLightBaseMaterial->InitializeImplementation();
    m_pPassPerLightBaseMaterial->ChangeDevice( m_pd3dDevice );
    pRDB->AddResource( m_pPassPerLightBaseMaterial );
    m_pDeferredBaseMaterial = ATG::BaseMaterial::CreateFXLiteMaterial( L"Deferred", L"deferred.fxobj" );
    m_pDeferredBaseMaterial->InitializeImplementation();
    m_pDeferredBaseMaterial->ChangeDevice( m_pd3dDevice );
    pRDB->AddResource( m_pDeferredBaseMaterial );
    m_pShaderLibraryBaseMaterial = ATG::BaseMaterial::CreateFXLiteMaterial( L"UbershaderLibrary",
                                                                            L"ubershader_library.fxobj" );
    m_pShaderLibraryBaseMaterial->InitializeImplementation();
    m_pShaderLibraryBaseMaterial->ChangeDevice( m_pd3dDevice );
    pRDB->AddResource( m_pShaderLibraryBaseMaterial );
}


//--------------------------------------------------------------------------------------
// Name: LoadScene()
// Desc: Loads a new scene.  This function may take a long time to return,
//       which is why it is only called by the loader thread right now.
//--------------------------------------------------------------------------------------
HRESULT SceneViewer::LoadScene( const CHAR* strSceneFileName )
{
    // Make sure we're being called on the loader thread.
    assert( GetCurrentThreadId() == g_dwLoaderThreadID );

    // Unload existing scene, if one is loaded.
    UnloadScene();

    // Create a new scene.
    ATG::Scene* pScene = new ATG::Scene();

    // Load a set of default assets for the scene.
    InitializeSceneDefaultAssets( pScene );

    // Clear pointer to parse error message.
    m_strSceneParseErrorMsg = NULL;

    // Load the XML scene file.
    HRESULT hr = ATG::SceneFileParser::LoadXATGFile( strSceneFileName, pScene, NULL, 0, &m_dwAsyncLoadProgress );

    // If we failed, return.
    if( FAILED( hr ) )
    {
        delete pScene;
        m_strSceneParseErrorMsg = ATG::SceneFileParser::GetParseErrorMessage();
        return E_FAIL;
    }

    // Rebind materials if we've loaded the scene in deferred or pass-per-light mode.
    if( m_RenderMode != SVRM_NORMAL )
    {
        RebindMaterialShaders( pScene );
    }

    // Save the current file name.
    strcpy_s( m_strSceneFileName, strSceneFileName );

    // Hook up the sample parameter pool interface to the FXLite parameter pool.
    // This also sets up the shader library handles.
    ATG::FXLiteMaterialImplementation* pFXLiteMaterialImpl = ( ATG::FXLiteMaterialImplementation* )
        m_pShaderLibraryBaseMaterial->GetMaterialImplementation();
    assert( pFXLiteMaterialImpl != NULL );
    m_SampleParameterPool.Initialize( pScene->GetEffectParameterPool(), pFXLiteMaterialImpl->m_pEffect );

    // Hook up the deferred and pass-per-light pool interfaces.
    pFXLiteMaterialImpl = ( ATG::FXLiteMaterialImplementation* )m_pDeferredBaseMaterial->GetMaterialImplementation();
    m_DeferredParameterPool.Initialize( pScene->GetEffectParameterPool(), pFXLiteMaterialImpl->m_pEffect );
    pFXLiteMaterialImpl = ( ATG::FXLiteMaterialImplementation* )m_pPassPerLightBaseMaterial->GetMaterialImplementation
        ();
    m_PassPerLightParameterPool.Initialize( pScene->GetEffectParameterPool(), pFXLiteMaterialImpl->m_pEffect );

    // Find cameras and lights, and update the settings UI with the camera names.
    FindCameras( pScene );
    SettingsGroup* pCameraGroup = m_SettingsPanel.FindGroup( L"Camera" );
    assert( pCameraGroup != NULL );
    DWORD dwSettingIndex = pCameraGroup->FindEntryIndex( L"Current Camera" );
    assert( dwSettingIndex != ( DWORD )-1 );
    pCameraGroup->ClearEnumEntries( dwSettingIndex );
    for( DWORD i = 0; i < m_SceneCameraList.size(); i++ )
    {
        ATG::Camera* pCamera = m_SceneCameraList[i];
        pCameraGroup->AddEnumEntry( dwSettingIndex, pCamera->GetName(), i );
    }

    // Set up the object tweaker.
    m_ObjectTweaker.Initialize( pScene, this );

    // Bind animation, if one exists.
    ATG::Animation* pAnimation = ( ATG::Animation* )pScene->FindObject( L"Untitled" );
    if( pAnimation != NULL )
    {
        m_AnimationPlayer.BindAnimationToFrames( pAnimation, pScene, NULL );
    }

    // Reset light rig.
    m_dwDefaultLightRigIndex = 0;

    // We're done loading the scene, update the member pointer so the renderer can go.
    m_pScene = pScene;

    // Update the default light rig.
    UpdateDefaultLightRig();

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: LoadSceneAsync()
// Desc: Triggers the loader thread to begin loading a new scene.  This function returns
//       quickly, and the load continues on another thread.
//--------------------------------------------------------------------------------------
HRESULT SceneViewer::LoadSceneAsync( const CHAR* strSceneFileName )
{
    // If we're currently loading, we can't start another load.
    if( g_bAsyncLoadingInProgress )
        return E_FAIL;

    // Set up global variables for the loader thread to use.
    m_dwAsyncLoadProgress = 0;
    strcpy_s( g_strAsyncLoadFilename, strSceneFileName );

    // Wake up the loader thread.
    g_LoaderTask.BeginTask();

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: UnloadScene()
// Desc: Unloads the current scene data, and clears the state of many scene-related
//       structures, such as the animation engine and the effect parameter pool.
//--------------------------------------------------------------------------------------
VOID SceneViewer::UnloadScene()
{
    // We need a critical section here so we aren't rendering the scene while we
    // delete the scene.
    AcquireScene();

    // Keep a pointer to the scene.
    ATG::Scene* pScene = m_pScene;

    // Nuke all of the scene state.
    m_pScene = NULL;
    m_ObjectTweaker.Initialize( NULL, this );
    m_SceneCameraList.clear();
    assert( m_SettingsPanel.GetGroupCount() >= 2 );
    m_SettingsPanel.GetGroup( 1 )->ClearEnumEntries( 0 );
    m_dwActiveCameraIndex = 0;
    m_strSceneFileName[0] = '\0';
    m_AnimationPlayer.Clear();
    m_strSceneParseErrorMsg = NULL;
    m_pDeferredBaseMaterial = NULL;
    m_pPassPerLightBaseMaterial = NULL;
    m_pShaderLibraryBaseMaterial = NULL;
    if( pScene != NULL )
    {
        // Make sure D3D doesn't keep any pointers to scene assets.
        AcquireD3D();
        for( DWORD i = 0; i < 16; ++i )
            m_pd3dDevice->SetTexture( i, NULL );
        m_pd3dDevice->SetIndices( NULL );
        m_pd3dDevice->SetStreamSource( 0, NULL, 0, 0 );
        m_pd3dDevice->SetPixelShader( NULL );
        m_pd3dDevice->SetVertexShader( NULL );
        m_pd3dDevice->SetVertexDeclaration( NULL );
        ReleaseD3D();

        // Unbind the parameter pool interface.
        m_SampleParameterPool.Terminate();

        // Delete the scene.  This should deallocate all of the asset memory, hopefully.
        delete pScene;
        pScene = NULL;
    }

    ReleaseScene();
}


//--------------------------------------------------------------------------------------
// Name: FindCameras()
// Desc: Finds all of the cameras in the scene and keeps track of them in a list.
//--------------------------------------------------------------------------------------
VOID SceneViewer::FindCameras( ATG::Scene* pScene )
{
    m_SceneCameraList.clear();
    m_dwActiveCameraIndex = 0;
    m_pCurrentSceneCamera = NULL;
    ATG::NameIndexedCollection::iterator i;
    for( i = pScene->GetInstanceList()->begin(); i != pScene->GetInstanceList()->end(); i++ )
    {
        if( ( *i )->IsDerivedFrom( ATG::Camera::TypeID ) )
        {
            ATG::Camera* pCamera = ( ATG::Camera* )( *i );
            if( pCamera->GetName() == L"persp" )
            {
                m_dwActiveCameraIndex = m_SceneCameraList.size();
                m_pCurrentSceneCamera = pCamera;
            }

            m_SceneCameraList.push_back( ( ATG::Camera* )( *i ) );
        }
    }
    if( m_SceneCameraList.size() == 0 )
    {
        ATG::Camera* pCamera = CreateDefaultCamera();
        pScene->AddChild( pCamera );
        pScene->AddObject( pCamera );
        m_SceneCameraList.push_back( pCamera );
        m_pCurrentSceneCamera = pCamera;
        m_dwActiveCameraIndex = 0;
    }
}


ATG::Camera* SceneViewer::CreateDefaultCamera()
{
    ATG::Camera* pCamera = new ATG::Camera;
    pCamera->SetName( L"Default" );
    pCamera->SetLocalTransform( XMMatrixIdentity() );

    ATG::Projection Proj;
    Proj.SetFovYAspect( XM_PIDIV4, m_fAspectRatio, m_fCameraZFar * 1e-4f, m_fCameraZFar );
    pCamera->SetProjection( Proj );

    return pCamera;
}



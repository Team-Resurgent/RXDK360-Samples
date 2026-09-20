//--------------------------------------------------------------------------------------
// Update.cpp
//
// Contains methods used for updating the scene.  Most of this code runs on core 1.
// Also contains methods useful for camera and frame manipulation.
// 
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include "SceneViewer2.h"

//--------------------------------------------------------------------------------------
// Name: Update()
// Desc: Called once per frame, the call is the entry point for animating the scene.
//--------------------------------------------------------------------------------------
HRESULT SceneViewer::UpdateLogic()
{
    // Get the current time.
    m_fAppTime = ( FLOAT )m_Timer.GetAppTime();
    m_fDeltaTime = ( FLOAT )m_Timer.GetElapsedTime();

    // Increment timer frame count.
    m_Timer.MarkFrame();

    // Get the current gamepad state
    ATG::GAMEPAD* pGamepad = ATG::Input::GetMergedInput();

    if( m_dwActiveCameraIndex < m_SceneCameraList.size() )
    {
        m_pCurrentSceneCamera = m_SceneCameraList[ m_dwActiveCameraIndex ];
    }
    else
    {
        m_pCurrentSceneCamera = NULL;
    }

    BOOL bUpdateCameraPos = TRUE;
    DWORD dwResult = 0;
    if( m_XuiApp.IsActive() )
    {
        m_SettingsPanel.Update( NULL, m_fDeltaTime, m_fAppTime );
        m_ObjectTweaker.Update( NULL, m_fDeltaTime, m_fAppTime );
        bUpdateCameraPos = FALSE;
    }
    else if( m_SettingsPanel.HasFocus() )
    {
        dwResult = m_SettingsPanel.Update( pGamepad, m_fDeltaTime, m_fAppTime );
        m_ObjectTweaker.Update( NULL, m_fDeltaTime, m_fAppTime );
        bUpdateCameraPos = FALSE;
    }
    else if( m_ObjectTweaker.HasFocus() )
    {
        m_SettingsPanel.Update( NULL, m_fDeltaTime, m_fAppTime );
        m_ObjectTweaker.Update( pGamepad, m_fDeltaTime, m_fAppTime );
        bUpdateCameraPos = !m_ObjectTweaker.IsMovementActive();
    }
    else
    {
        dwResult = m_SettingsPanel.Update( pGamepad, m_fDeltaTime, m_fAppTime );
        if( !m_SettingsPanel.HasFocus() )
        {
            m_ObjectTweaker.Update( pGamepad, m_fDeltaTime, m_fAppTime );
            if( !m_ObjectTweaker.HasFocus() )
            {
                if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_BACK )
                    m_SettingsPanel.ShowMenuItem( 0, 0 );
            }
        }
    }

    switch( dwResult )
    {
        case SVUI_LOADSCENE:
            m_SettingsPanel.SetVisible( FALSE );
            m_XuiApp.ShowFileDialog();
            break;
    }

    // Update benchmark module.
    m_BenchmarkModule.Update( m_fDeltaTime );
    if( m_BenchmarkModule.IsActive() && !m_BenchmarkModule.IsFinished() )
        bUpdateCameraPos = FALSE;


    if( m_pScene != NULL )
    {
        AcquireScene();

        // Update adaptive camera speed.
        if( m_bAdaptiveCameraSpeed )
        {
            // Compute diameter of scene.
            FLOAT fSceneSize = 2 * m_ShadowSceneState.EntireSceneBounds.GetMaxRadius();
            // Should be able to cross the scene in 20 seconds of driving, unless the scene is really tiny.
            FLOAT fSpeed = fSceneSize / 20.0f;
            fSpeed = max( fSpeed, 0.25f );
            // Lerp to the new camera speed.
            FLOAT fLerp = m_fDeltaTime * 10.0f;
            fLerp = min( 1.0f, fLerp );
            m_fCameraMoveSpeed = fLerp * fSpeed + ( 1.0f - fLerp ) * m_fCameraMoveSpeed;
        }

        // Move camera based on thumbstick settings.
        if( bUpdateCameraPos && m_pCurrentSceneCamera != NULL )
            MoveCamera( pGamepad, m_fDeltaTime, m_pCurrentSceneCamera );

        // Animate scene.
        if( m_bEnableAnimations )
        {
            m_AnimationPlayer.SetPlaybackSpeed( m_fAnimationSpeed );
            m_AnimationPlayer.Update( m_fDeltaTime );
        }

        static DWORD s_dwLastLightRigIndex = ( DWORD )-1;
        if( s_dwLastLightRigIndex != m_dwDefaultLightRigIndex )
        {
            s_dwLastLightRigIndex = m_dwDefaultLightRigIndex;
            UpdateDefaultLightRig();
        }

        ReleaseScene();
    }

    return S_OK;
}


ATG::Light* FindOrCreateLight( ATG::Scene* pScene, ATG::Frame* pRoot, const ATG::StringID strName,
                               const ATG::StringID strType )
{
    ATG::Frame* pChild = pRoot->GetFirstChild();
    while( pChild != NULL )
    {
        if( pChild->IsDerivedFrom( ATG::Light::TypeID ) )
        {
            ATG::Light* pLight = ( ATG::Light* )pChild;
            if( pLight->IsDerivedFrom( strType ) && pLight->GetName() == strName )
                return pLight;
        }
        pChild = pChild->GetNextSibling();
    }

    ATG::Light* pResult = NULL;
    if( strType == ATG::PointLight::TypeID )
    {
        pResult = new ATG::PointLight();
    }
    else if( strType == ATG::SpotLight::TypeID )
    {
        pResult = new ATG::SpotLight();
    }
    else if( strType == ATG::DirectionalLight::TypeID )
    {
        pResult = new ATG::DirectionalLight();
    }
    else
    {
        assert( false );
    }

    pResult->SetName( strName );
    pRoot->AddChild( pResult );
    pScene->AddObject( pResult );
    return pResult;
}


VOID SceneViewer::UpdateDefaultLightRig()
{
    static const ATG::StringID strRootName( L"__DefaultLightRig_Root" );
    static const ATG::StringID strDirLightName( L"__DefaultLightRig_DirLight" );
    static const ATG::StringID strSpotLightName( L"__DefaultLightRig_SpotLight" );
    static const ATG::StringID strPointLightName0( L"__DefaultLightRig_PointLight0" );
    static const ATG::StringID strPointLightName1( L"__DefaultLightRig_PointLight1" );
    static const ATG::StringID strPointLightName2( L"__DefaultLightRig_PointLight2" );

    ATG::Frame* pRigRoot = ( ATG::Frame* )m_pScene->FindObjectOfType( strRootName, ATG::Frame::TypeID );
    if( pRigRoot == NULL )
    {
        pRigRoot = new ATG::Frame( strRootName, XMMatrixIdentity() );
        m_pScene->AddChild( pRigRoot );
        m_pScene->AddObject( pRigRoot );
    }

    ATG::Frame* pChild = pRigRoot->GetFirstChild();
    while( pChild != NULL )
    {
        if( pChild->IsDerivedFrom( ATG::Light::TypeID ) )
        {
            ATG::Light* pLight = ( ATG::Light* )pChild;
            pLight->SetFlag( ATG::Light::IsDisabled );
        }
        pChild = pChild->GetNextSibling();
    }

    switch( m_dwDefaultLightRigIndex )
    {
        case 0:
            return;
        case 1:
        {
            // single directional light
            ATG::DirectionalLight* pDirLight = ( ATG::DirectionalLight* )FindOrCreateLight( m_pScene, pRigRoot,
                                                                                            strDirLightName,
                                                                                            ATG::DirectionalLight::
                                                                                            TypeID );
            XMMATRIX matLight = XMMatrixRotationX( XM_PI * 0.25f );
            pDirLight->SetWorldTransform( matLight );
            XMVECTOR vColor = { 1, 1, 1, 1 };
            pDirLight->SetColor( vColor );
            pDirLight->ClearFlag( ATG::Light::IsDisabled );
            return;
        }
        case 2:
        {
            // single spot light
            ATG::SpotLight* pSpotLight = ( ATG::SpotLight* )FindOrCreateLight( m_pScene, pRigRoot, strSpotLightName,
                                                                               ATG::SpotLight::TypeID );
            XMMATRIX matLight = XMMatrixRotationX( XM_PI * 0.25f );
            XMVECTOR vPos = { 0, 1, -1, 0 };
            FLOAT fRange = m_ShadowSceneState.EntireSceneBounds.GetMaxRadius();
            vPos = XMVector3Normalize( vPos );
            vPos *= ( fRange * 2 );
            XMFLOAT3 SceneCenter = m_ShadowSceneState.EntireSceneBounds.GetCenter();
            vPos += XMLoadFloat3( &SceneCenter );
            vPos.w = 1;
            matLight.r[3] = vPos;
            pSpotLight->SetWorldTransform( matLight );
            XMVECTOR vColor = { 1, 1, 1, 1 };
            pSpotLight->SetColor( vColor );
            pSpotLight->SetInnerAngle( XM_PI * 0.25f );
            pSpotLight->SetOuterAngle( XM_PI * 0.27f );
            pSpotLight->SetWorldRange( fRange * 4 );
            pSpotLight->ClearFlag( ATG::Light::IsDisabled );
            return;
        }
        case 3:
        {
            // three point lights above scene
            FLOAT fRadius = m_ShadowSceneState.EntireSceneBounds.GetMaxRadius();
            XMFLOAT3 SceneCenter = m_ShadowSceneState.EntireSceneBounds.GetCenter();
            XMVECTOR vCenter = XMLoadFloat3( &SceneCenter );
            XMVECTOR vPos0 = vCenter + XMVectorSet( 0, 0.7071f, 0.7071f, 0 ) * fRadius;
            XMVECTOR vPos1 = vCenter + XMVectorSet( 0.5f, 0.7071f, -0.5f, 0 ) * fRadius;
            XMVECTOR vPos2 = vCenter + XMVectorSet( -0.5f, 0.7071f, -0.5f, 0 ) * fRadius;
            ATG::PointLight* pPoint0 = ( ATG::PointLight* )FindOrCreateLight( m_pScene, pRigRoot, strPointLightName0,
                                                                              ATG::PointLight::TypeID );
            ATG::PointLight* pPoint1 = ( ATG::PointLight* )FindOrCreateLight( m_pScene, pRigRoot, strPointLightName1,
                                                                              ATG::PointLight::TypeID );
            ATG::PointLight* pPoint2 = ( ATG::PointLight* )FindOrCreateLight( m_pScene, pRigRoot, strPointLightName2,
                                                                              ATG::PointLight::TypeID );
            pPoint0->SetWorldPosition( vPos0 );
            pPoint1->SetWorldPosition( vPos1 );
            pPoint2->SetWorldPosition( vPos2 );
            XMVECTOR vColor = { 1, 1, 1, 1 };
            pPoint0->SetColor( vColor );
            pPoint1->SetColor( vColor );
            pPoint2->SetColor( vColor );
            pPoint0->SetWorldRange( fRadius * 2 );
            pPoint1->SetWorldRange( fRadius * 2 );
            pPoint2->SetWorldRange( fRadius * 2 );
            pPoint0->ClearFlag( ATG::Light::IsDisabled );
            pPoint1->ClearFlag( ATG::Light::IsDisabled );
            pPoint2->ClearFlag( ATG::Light::IsDisabled );
            return;
        }
    }
}


//--------------------------------------------------------------------------------------
// Name: MoveCamera()
// Desc: Converts gamepad input into camera movement.  The input model is a free-cam
//       model, where the left stick controls movement, and the right stick controls
//       orientation.  The shoulder buttons modify the movement speed.
//--------------------------------------------------------------------------------------
VOID SceneViewer::MoveCamera( ATG::GAMEPAD* pGamepad, FLOAT fDeltaTime, ATG::Camera* pCamera )
{
    // Make sure the camera's projection matrix is OK.
    ATG::Projection proj = pCamera->GetProjection();
    proj.SetFovYAspect( XM_PIDIV4, m_fAspectRatio, m_fCameraZFar * 1e-4f, m_fCameraZFar );
    pCamera->SetProjection( proj );

    MoveFrame( pGamepad, fDeltaTime, pCamera );
}


//--------------------------------------------------------------------------------------
// Name: MoveFrame()
// Desc: Converts gamepad input into frame movement.  The input model is a free-cam
//       model, where the left stick controls movement, and the right stick controls
//       orientation.  The shoulder buttons modify the movement speed.
//--------------------------------------------------------------------------------------
VOID SceneViewer::MoveFrame( ATG::GAMEPAD* pGamepad, FLOAT fDeltaTime, ATG::Frame* pFrame )
{
    assert( pFrame != NULL && pGamepad != NULL );

    static const XMVECTOR vX = XMVectorSet( 1, 0, 0, 0 );
    static const XMVECTOR vY = XMVectorSet( 0, 1, 0, 0 );
    static const XMVECTOR vZ = XMVectorSet( 0, 0, 1, 0 );

    XMVECTOR vWorldUpVector = vY;
    if( m_dwUpAxis == 1 )
        vWorldUpVector = vZ;

    // Obtain the current vectors for the frame.
    XMVECTOR vForward = pFrame->GetWorldDirection();
    XMVECTOR vUp = pFrame->GetWorldUp();
    XMVECTOR vRight = pFrame->GetWorldRight();

    // Compute movement speed.
    FLOAT fSpeed = m_fCameraMoveSpeed;
    if( pGamepad->wLastButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER )
        fSpeed *= 10.0f;
    else if( pGamepad->wLastButtons & XINPUT_GAMEPAD_LEFT_SHOULDER )
        fSpeed *= 0.1f;

    // Get the thumbstick settings.  Square them so small adjustments are easier.
    FLOAT fX1 = pGamepad->fX1 * fabs( pGamepad->fX1 );
    FLOAT fY1 = pGamepad->fY1 * fabs( pGamepad->fY1 );
    FLOAT fX2 = pGamepad->fX2 * fabs( pGamepad->fX2 );
    FLOAT fY2 = pGamepad->fY2 * fabs( pGamepad->fY2 );
    if( m_bInvertRotationYAxis )
        fY2 = -fY2;

    BOOL bUpdateWorld = FALSE;

    // Adjust frame position based on the left thumbstick.
    XMVECTOR vPos = pFrame->GetWorldPosition();
    if( fY1 != 0 || fX1 != 0 )
    {
        bUpdateWorld = TRUE;
        FLOAT fAmount = fDeltaTime * fSpeed;
        if( pGamepad->wLastButtons & XINPUT_GAMEPAD_LEFT_THUMB )
        {
            vPos += XMVectorScale( vUp, fAmount * fY1 );
        }
        else
        {
            vPos += XMVectorScale( vForward, fAmount * fY1 );
            vPos += XMVectorScale( vRight, fAmount * fX1 );
        }
    }

    // Adjust frame orientation based on right thumbstick.
    XMVECTOR RotationQ = XMQuaternionIdentity();
    XMVECTOR RotationX = XMQuaternionIdentity();
    XMVECTOR RotationY = XMQuaternionIdentity();
    XMVECTOR RotationZ = XMQuaternionIdentity();
    FLOAT fRotationSpeed = XM_PIDIV2 * fDeltaTime;
    if( fX2 != 0 || fY2 != 0 )
    {
        bUpdateWorld = TRUE;
        if( pGamepad->wLastButtons & XINPUT_GAMEPAD_RIGHT_THUMB )
        {
            RotationZ = XMQuaternionRotationAxis( vZ, fRotationSpeed * fX2 );
            RotationQ = RotationZ;
        }
        else
        {
            RotationX = XMQuaternionRotationAxis( vX, fRotationSpeed * fY2 );
            RotationQ = RotationX;
            RotationY = XMQuaternionRotationAxis( vY, fRotationSpeed * fX2 );
            RotationQ = XMQuaternionMultiply( RotationQ, RotationY );
        }
    }

    // Iteratively level the frame if the Y button is being held down.
    if( pGamepad->wLastButtons & XINPUT_GAMEPAD_Y )
    {
        bUpdateWorld = TRUE;
        XMVECTOR vDot = XMVector3Dot( vRight, vWorldUpVector );
        FLOAT fAmount = -vDot.x;
        fAmount *= ( fDeltaTime * 4.0f );
        XMVECTOR RotationRestoreUp = XMQuaternionRotationAxis( vZ, fAmount );
        RotationQ = XMQuaternionMultiply( RotationQ, RotationRestoreUp );
    }

    // Compose a new world matrix based on the rotation quaternion and position.
    if( bUpdateWorld )
    {
        XMMATRIX matWorld = pFrame->GetWorldTransform();
        if( m_dwCameraControlType == 0 )
        {
            matWorld = XMMatrixMultiply( XMMatrixRotationQuaternion( RotationQ ), matWorld );
        }
        else
        {
            XMMATRIX matRotX = XMMatrixRotationQuaternion( RotationX );
            XMMATRIX matRotY = XMMatrixRotationQuaternion( RotationY );
            matWorld = matRotX * matWorld * matRotY;
        }
        matWorld.r[3] = XMVectorSelect( vPos, XMQuaternionIdentity(), XMVectorSelectControl( 0, 0, 0, 1 ) );
        pFrame->SetWorldTransform( matWorld );
    }
}


HRESULT SceneViewer::UpdateGameState()
{
    PIXBeginNamedEvent( 0, "Update Thread" );

    PIXBeginNamedEvent( 0, "Wait For Update Event" );
    m_TaskUpdate.WaitForBegin();
    PIXEndNamedEvent();

    PIXBeginNamedEvent( 0, "Logic" );
    UpdateLogic();
    PIXEndNamedEvent();

    m_TaskUpdate.TaskCompleted();

    PIXEndNamedEvent();

    return S_OK;
}

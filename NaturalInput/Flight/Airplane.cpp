//--------------------------------------------------------------------------------------
// File: Airplane.cpp
//
// Implements airplane flight physics and rendering.
// 
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include "Airplane.h"
#include <AtgUtil.h>
#include "AudioEngine.h"

ATG::Scene* Airplane::s_pScene = NULL;
ATG::Model* Airplane::s_pBiplaneModels[2] = { NULL, NULL };
ATG::Model* Airplane::s_pPropellerModel = NULL;

const FLOAT g_fCollisionRadius = 1.5f;
const FLOAT g_fStartingEnginePower = 30.0f;
const FLOAT g_fDragCoefficient = 0.005f;
const FLOAT g_fLiftConstant = 0.002f;
const FLOAT g_fGravity = 9.8f;
const FLOAT g_fLiftMultiplierMagnitude = 1.0f;
const FLOAT g_fBankingLerpSpeed = 2.0f;
const FLOAT g_fCrashSlowingSpeed = 5.0f;
const FLOAT g_fPropellerThrottleMultiplier = 30.0f;
const FLOAT g_fPitchSensitivity = 1.0f / 7500.0f;
const FLOAT g_fYawSensitivity = 1.0f / 10000.0f;
const FLOAT g_fThrottleMultiplier = 0.5f;

//--------------------------------------------------------------------------------------
// Static method to load the biplane mesh content & textures
//--------------------------------------------------------------------------------------
HRESULT Airplane::LoadContent( ATG::BaseMaterial* pBaseMaterial )
{
    HRESULT hr;

    s_pScene = new ATG::Scene();
    s_pScene->GetResourceDatabase()->AddResource( pBaseMaterial );
    s_pScene->GetResourceDatabase()->CreateDefaultResources();

    hr = ATG::SceneFileParser::LoadXATGFile( "game:\\media\\scenes\\biplane.xatg", s_pScene, NULL, 0, NULL );
    if( FAILED(hr) )
    {
        ATG::FatalError( "Could not load biplane content file." );
    }

    s_pBiplaneModels[0] = (ATG::Model*)s_pScene->FindObjectOfType( L"Body", ATG::Model::TypeID );
    s_pBiplaneModels[1] = (ATG::Model*)s_pScene->FindObjectOfType( L"WingsEtc", ATG::Model::TypeID );
    s_pPropellerModel = (ATG::Model*)s_pScene->FindObjectOfType( L"Propeller", ATG::Model::TypeID );
    if( s_pBiplaneModels[0] == NULL || s_pBiplaneModels[1] == NULL || s_pPropellerModel == NULL )
    {
        ATG::FatalError( "Could not find biplane model." );
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Constructor
//--------------------------------------------------------------------------------------
Airplane::Airplane()
{
    Reset();
}


//--------------------------------------------------------------------------------------
// Resets the airplane to the start of the runway
//--------------------------------------------------------------------------------------
VOID Airplane::Reset()
{
    // The start of the runway is at ( 0, 0, -20 ) in world space
    Reset( XMVectorSet( 0, 0, -20, 0 ), 0.0f, XMVectorZero() );
}


//--------------------------------------------------------------------------------------
// Resets the airplane to an arbitrary location
//--------------------------------------------------------------------------------------
VOID Airplane::Reset( XMVECTOR vStartingPos, FLOAT fThrottle, XMVECTOR vVelocity )
{
    m_fControlThrottle = fThrottle;
    m_fThrottle = fThrottle;
    m_fEnginePower = g_fStartingEnginePower;

    XMVECTOR vMinPos = XMVectorSet( -FLT_MAX, g_fCollisionRadius, -FLT_MAX, 0 );
    vStartingPos = XMVectorMax( vStartingPos, vMinPos );

    XMMATRIX matWorld = XMMatrixIdentity();
    matWorld.r[3] = vStartingPos;
    matWorld._44 = 1.0f;
    XMStoreFloat4x4A( &m_matWorld, matWorld );

    XMStoreFloat4A( &m_vLinearVelocity, vVelocity );
    m_fBankingAngle = 0.0f;

    if( XMVectorGetY( vStartingPos ) > g_fCollisionRadius )
    {
        m_FlightState = Flying;
    }
    else
    {
        m_FlightState = Taxi;
    }
}


//--------------------------------------------------------------------------------------
// Processes raw control inputs and sets member variables
//--------------------------------------------------------------------------------------
VOID Airplane::SetControls( FLOAT fThrottle, FLOAT fYAxis, FLOAT fXAxis )
{
    m_fControlThrottle = fThrottle;
    m_fControlYAxis = fYAxis * fabs( fYAxis );
    m_fControlXAxis = fXAxis * fabs( fXAxis );
}


//--------------------------------------------------------------------------------------
// Updates the state of the airplane using a simplified flight model
// Note: Even though there are physics-related terms in this method, the flight model
// here is not physically accurate
//--------------------------------------------------------------------------------------
VOID Airplane::Update( FLOAT fDeltaTime )
{
    const XMVECTOR vDeltaTime = XMVectorReplicate( fDeltaTime );
    const XMVECTOR vLiftConstant = XMVectorReplicate( g_fLiftConstant );

    // Update throttle
    m_fThrottle += ( g_fThrottleMultiplier * m_fControlThrottle ) * fDeltaTime;
    m_fThrottle = min( 1.0f, max( 0.0f, m_fThrottle ) );

    // Update banking angle (used for rendering only)
    if( m_FlightState != Crashed )
    {
        FLOAT fLerp = min( 1.0f, fDeltaTime * g_fBankingLerpSpeed );
        FLOAT fDesiredBankingAngle = m_fControlXAxis * -XM_PIDIV4;
        if( m_FlightState == Taxi )
        {
            fDesiredBankingAngle = 0.0f;
        }
        m_fBankingAngle = ( m_fBankingAngle * ( 1.0f - fLerp ) ) + ( fDesiredBankingAngle * fLerp );
    }

    XMMATRIX matWorld = XMLoadFloat4x4A( &m_matWorld );
    XMVECTOR vLinearVelocity = XMLoadFloat4A( &m_vLinearVelocity );
    FLOAT fVelocity = XMVectorGetX( XMVector3LengthEst( vLinearVelocity ) );

    // Update plane heading based on controls
    // Pitch and yaw are proportional to velocity squared, so the plane turns faster when flying faster
    FLOAT fPitch = m_fControlYAxis * fDeltaTime * ( ( fVelocity * fVelocity ) * g_fPitchSensitivity );
    FLOAT fYaw = m_fControlXAxis * fDeltaTime * ( ( fVelocity * fVelocity ) * g_fYawSensitivity );
    XMVECTOR qPitch = XMQuaternionRotationAxis( XMVectorSet( 1, 0, 0, 0 ), fPitch );
    XMVECTOR qYaw = XMQuaternionRotationAxis( XMVectorSet( 0, 1, 0, 0 ), fYaw );

    // Apply heading adjustments to world matrix (retaining the world position)
    XMVECTOR vPlanePosition = matWorld.r[3];
    matWorld = XMMatrixRotationQuaternion( qPitch ) * matWorld * XMMatrixRotationQuaternion( qYaw );
    matWorld.r[3] = vPlanePosition;

    // Extract basis vectors for use in lift and thrust computations
    XMVECTOR vPlaneForward = matWorld.r[2];
    XMVECTOR vPlaneUp = matWorld.r[1];

    // Compute forward velocity and speed
    XMVECTOR vForwardVelocity = vPlaneForward * XMVector3Dot( vLinearVelocity, vPlaneForward );
    XMVECTOR vForwardSpeed = XMVector3LengthEst( vForwardVelocity );

    // Engine thrust is proportional to throttle control
    XMVECTOR vThrustForce = vPlaneForward * XMVectorReplicate( m_fEnginePower * m_fThrottle );

    // Gravity is downwards
    XMVECTOR vGravityForce = XMVectorSet( 0, -g_fGravity, 0, 0 );

    // Lift is proportional to the forward speed squared
    // The "lift multiplier" maintains roughly level flight when turning, this makes the plane more enjoyable to fly
    FLOAT fLiftMultiplier = 1.0f + fabs( m_fControlXAxis ) * g_fLiftMultiplierMagnitude;
    XMVECTOR vLiftForce = vPlaneUp * vLiftConstant * vForwardSpeed * vForwardSpeed * XMVectorReplicate( fLiftMultiplier );

    // Add up all of the forces
    XMVECTOR vCombinedForce = vThrustForce + vGravityForce + vLiftForce;

    // Apply drag to the plane if we have a positive velocity
    if( fVelocity > 0.0f )
    {
        FLOAT fDragAmount = -g_fDragCoefficient * fVelocity * fVelocity;
        XMVECTOR vDragForce = fDragAmount * XMVector3Normalize( vLinearVelocity );
        vCombinedForce += vDragForce;
    }

    XMVECTOR vAcceleration = vCombinedForce;  // mass is 1.0

    // Integrate acceleration into velocity
    vLinearVelocity += vAcceleration * vDeltaTime;

    // Apply a rapid slowing factor if we have crashed
    if( m_FlightState == Crashed )
    {
        vLinearVelocity *= max( 0.0f, ( 1.0f - fDeltaTime * g_fCrashSlowingSpeed ) );
    }

    // Integrate velocity into position
    vPlanePosition += vLinearVelocity * vDeltaTime;

    // Detect collision with ground and react appropriately
    DetectCollision( vPlanePosition, vLinearVelocity, vPlaneUp );

    // Apply new world position back into the world matrix
    XMVectorSetW( vPlanePosition, 1 );
    matWorld.r[3] = vPlanePosition;

    XMStoreFloat4A( &m_vLinearVelocity, vLinearVelocity );
    XMStoreFloat4x4A( &m_matWorld, matWorld );

    // Update the propeller angle (used for rendering only)
    m_fPropellerAngle -= ( m_fThrottle * fDeltaTime * g_fPropellerThrottleMultiplier );
    m_fPropellerAngle = fmodf( m_fPropellerAngle, XM_2PI );

    // Enforce a minimum audible throttle when we're flying
    FLOAT fAudioThrottle = m_fThrottle;
    if( m_FlightState == Flying )
    {
        fAudioThrottle = max( 0.05f, fAudioThrottle );
    }
    // Convert vertical velocity from 20..-20 to -100..100
    FLOAT fVerticalVelocity = XMVectorGetY( vLinearVelocity );
    FLOAT fAudioVerticalVelocity = ( fVerticalVelocity / 20.0f ) * -100.0f;

    // Send throttle and velocity data to audio engine
    g_pAudioEngine->SetEngineParameters( fAudioThrottle, fAudioVerticalVelocity );
}


//--------------------------------------------------------------------------------------
// Detects if we have a collision with the ground, and reacts accordingly
//--------------------------------------------------------------------------------------
VOID Airplane::DetectCollision( XMVECTOR& vPosition, XMVECTOR& vVelocity, const XMVECTOR vPlaneUp )
{
    // Detect collision with the ground
    if( XMVectorGetY( vPosition ) < g_fCollisionRadius )
    {
        BOOL bCrashed = ( m_FlightState == Crashed );

        // If the vertical velocity is too fast, we have crashed
        FLOAT fVerticalVelocity = XMVectorGetY( vVelocity );
        if( fVerticalVelocity < -5.0f && m_FlightState != Taxi )
        {
            bCrashed = TRUE;
        }

        // If the plane is not close to right side up, we have crashed
        if( XMVectorGetX( XMVector3Dot( XMVectorSet( 0, 1, 0, 0 ), vPlaneUp ) ) < 0.8f )
        {
            bCrashed = TRUE;
        }

        if( bCrashed )
        {
            if( m_FlightState != Crashed )
            {
                g_pAudioEngine->PlayCue( "Crash" );
            }

            // Apply the crash
            m_fEnginePower = 0.0f;
            m_fThrottle = 0.0f;
            m_FlightState = Crashed;
        }
        else
        {
            m_FlightState = Taxi;
        }

        vPosition = XMVectorSetY( vPosition, g_fCollisionRadius );
        vVelocity = XMVectorSetY( vVelocity, 0 );
    }
    else if( m_FlightState != Crashed )
    {
        m_FlightState = Flying;
    }
}


//--------------------------------------------------------------------------------------
// Renders the airplane
//--------------------------------------------------------------------------------------
VOID Airplane::Render( IDirect3DDevice9* pd3dDevice, UbershaderParameterPool* pParameterPool, XMMATRIX matViewProjection, XMVECTOR vDirLightWorldDirection )
{
    // Load world transform matrix and apply the banking angle (for visual effect only)
    XMMATRIX matPlaneWorld = XMLoadFloat4x4A( &m_matWorld );
    XMVECTOR qRoll = XMQuaternionRotationAxis( XMVectorSet( 0, 0, 1, 0 ), m_fBankingAngle );
    matPlaneWorld = XMMatrixRotationQuaternion( qRoll ) * matPlaneWorld;

    XMMATRIX matInvWorld = XMMatrixInverse( &qRoll, matPlaneWorld );
    XMVECTOR vDirLightObjDirection = XMVector3TransformNormal( vDirLightWorldDirection, matInvWorld );
    pParameterPool->SetDirLightObjectDir( 0, vDirLightObjDirection );

    // The propeller is centered at ( 0, 0.25, 3.25 ) in the content file
    XMMATRIX matPropeller = XMMatrixRotationZ( m_fPropellerAngle ) * XMMatrixTranslation( 0, 0.25f, 3.25f );

    XMMATRIX matWVP = matPlaneWorld * matViewProjection;

    RenderModel( pd3dDevice, pParameterPool, s_pBiplaneModels[0], matWVP );
    RenderModel( pd3dDevice, pParameterPool, s_pBiplaneModels[1], matWVP );
    RenderModel( pd3dDevice, pParameterPool, s_pPropellerModel, matPropeller * matWVP );
}

VOID Airplane::RenderModel( IDirect3DDevice9* pd3dDevice, UbershaderParameterPool* pParameterPool, ATG::Model* pModel, XMMATRIX matWorldViewProjection )
{
    // Compute world * view * projection matrix for this model and set into constants.
    pParameterPool->SetWorldViewProjMatrix( matWorldViewProjection );

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
            pMaterial->BeginMaterialSinglePass( pd3dDevice );
            // Render the mesh subset.
            pMesh->RenderSubset( dwSubsetIndex, pd3dDevice );
            // End the FXLite material.
            pMaterial->EndMaterialSinglePass();
        }
    }
}

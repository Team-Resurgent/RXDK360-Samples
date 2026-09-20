//--------------------------------------------------------------------------------------
// File: RingLayout.cpp
//
// Implements a series of rings in 3D space, and tracks the player's progress through
// the series of rings.
// 
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include "RingLayout.h"
#include <AtgUtil.h>
#include "AudioEngine.h"

ATG::Scene* RingLayout::s_pScene = NULL;
ATG::Model* RingLayout::s_pRingModel = NULL;
ATG::Model* RingLayout::s_pConeModel = NULL;

//--------------------------------------------------------------------------------------
// Represents a segment endpoint for laying out a ring course
//--------------------------------------------------------------------------------------
struct Knot
{
    XMFLOAT3 vPos;
    XMFLOAT3 vNormal;
};
typedef std::vector<Knot> KnotVector;

//--------------------------------------------------------------------------------------
// Static method that loads the ring and cone meshes & textures
//--------------------------------------------------------------------------------------
HRESULT RingLayout::LoadContent( ATG::BaseMaterial* pBaseMaterial )
{
    s_pScene = new ATG::Scene();
    s_pScene->GetResourceDatabase()->AddResource( pBaseMaterial );
    s_pScene->GetResourceDatabase()->CreateDefaultResources();

    HRESULT hr = ATG::SceneFileParser::LoadXATGFile( "game:\\media\\scenes\\ring.xatg", s_pScene, NULL, 0, NULL );
    if( FAILED(hr) )
    {
        ATG::FatalError( "Could not load ring content file." );
    }

    s_pRingModel = (ATG::Model*)s_pScene->FindObjectOfType( L"Ring", ATG::Model::TypeID );
    if( s_pRingModel == NULL )
    {
        ATG::FatalError( "Could not find ring model." );
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Constructor
//--------------------------------------------------------------------------------------
RingLayout::RingLayout(void)
{
    m_strMessage[0] = L'\0';
    Reset();
}


//--------------------------------------------------------------------------------------
// Destructor deallocates the ring structs
//--------------------------------------------------------------------------------------
RingLayout::~RingLayout(void)
{
    Clear();
}


//--------------------------------------------------------------------------------------
// Helper function for adding a segment to an existing ring course specification vector
//--------------------------------------------------------------------------------------
VOID AddKnot( KnotVector& Knots, FLOAT XOffset, FLOAT YOffset, FLOAT ZOffset, FLOAT XNormal, FLOAT YNormal, FLOAT ZNormal )
{
    if( Knots.empty() )
    {
        Knot StartKnot;
        StartKnot.vPos = XMFLOAT3( XOffset, YOffset, ZOffset );
        XMStoreFloat3( &StartKnot.vNormal, XMVector3Normalize( XMVectorSet( XNormal, YNormal, ZNormal, 0 ) ) );
        Knots.push_back( StartKnot );
    }
    else
    {
        Knot NewKnot = Knots.back();
        XMVECTOR vNewPos = XMLoadFloat3( &NewKnot.vPos ) + XMVectorSet( XOffset, YOffset, ZOffset, 0 );
        XMStoreFloat3( &NewKnot.vPos, vNewPos );
        XMStoreFloat3( &NewKnot.vNormal, XMVector3Normalize( XMVectorSet( XNormal, YNormal, ZNormal, 0 ) ) );
        Knots.push_back( NewKnot );
    }
}


//--------------------------------------------------------------------------------------
// Sets up ring course specifications and generates the rings inside the ring layout object
//--------------------------------------------------------------------------------------
HRESULT RingLayout::CreateCourse( DWORD dwLevelIndex )
{
    KnotVector Knots;

    const FLOAT fCurveRadius = 1500.0f;

    const WCHAR* strCourseTitle = L"";

    switch( dwLevelIndex )
    {
    case 0:
        // easy course
        AddKnot( Knots, 0, 50, 500, 0, 0, -1 );
        AddKnot( Knots, 0, 50, 2000, 0, 0, -1 );
        AddKnot( Knots, fCurveRadius, 100, fCurveRadius, -1, 0, 0 );
        AddKnot( Knots, fCurveRadius, 100, -fCurveRadius, 0, 0, 1 );
        AddKnot( Knots, 0, -300, -3000, 0, 0, 1 );
        strCourseTitle = L"Easy";
        break;
    case 1:
        // difficult course
        AddKnot( Knots, 0, 50, 500, 0, -0.2f, -1 );
        AddKnot( Knots, 0, 50, 2000, 0, 0, -1 );
        AddKnot( Knots, fCurveRadius, 100, fCurveRadius, -1, 0, 0 );
        AddKnot( Knots, fCurveRadius, 100, -fCurveRadius, 0, 0, 1 );
        AddKnot( Knots, fCurveRadius, 100, -fCurveRadius, -1, 0, 0 );
        AddKnot( Knots, fCurveRadius, 100, -fCurveRadius, 0, 0, 1 );
        AddKnot( Knots, -fCurveRadius, 100, -fCurveRadius, 1, 0, 0 );
        AddKnot( Knots, -fCurveRadius, -200, 0, 1, 0, 0 );
        AddKnot( Knots, -fCurveRadius, 200, 0, 1, 0, 0 );
        AddKnot( Knots, -fCurveRadius, 0, -fCurveRadius, 0, 0, 1 );
        AddKnot( Knots, fCurveRadius, -100, -fCurveRadius, -1, 0, 0 );
        AddKnot( Knots, fCurveRadius, -100, -fCurveRadius, 0, 0, 1 );
        AddKnot( Knots, -fCurveRadius, -100, -fCurveRadius, 1, 0, 0 );
        AddKnot( Knots, -fCurveRadius, -100, fCurveRadius, 0, 0, -1 );
        AddKnot( Knots, 0, -150, fCurveRadius * 4, 0, 0, -1 );
        strCourseTitle = L"Difficult";
        break;
    default:
        return E_FAIL;
    }

    Clear();

    const FLOAT fRingSpacing = 500.0f;
    DWORD dwCount = (DWORD)Knots.size();
    for( DWORD i = 0; i < ( dwCount - 1 ); ++i )
    {
        const Knot& CurrentKnot = Knots[ i ];
        const Knot& NextKnot = Knots[ i + 1 ];
        FLOAT fDistance = XMVectorGetX( XMVector3LengthEst( XMLoadFloat3( &NextKnot.vPos ) - XMLoadFloat3( &CurrentKnot.vPos ) ) );
        AddRingCurve( XMLoadFloat3( &CurrentKnot.vPos ), 
                      XMLoadFloat3( &CurrentKnot.vNormal ), 
                      XMLoadFloat3( &NextKnot.vPos ), 
                      XMLoadFloat3( &NextKnot.vNormal ), 
                      (DWORD)( fDistance / fRingSpacing ) );
    }

    Reset();
    NewCourseMessage( strCourseTitle );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Adds a single ring at the given position and facing direction
//--------------------------------------------------------------------------------------
VOID RingLayout::AddRing( FXMVECTOR vPosition, FXMVECTOR vFacingDirection )
{
    XMVECTOR vCameraTarget = vPosition + vFacingDirection;
    XMMATRIX matView = XMMatrixLookAtLH( vPosition, vCameraTarget, XMVectorSet( 0, 1, 0, 0 ) );
    AddRing( XMMatrixInverse( &vCameraTarget, matView ) );
}


//--------------------------------------------------------------------------------------
// Adds a ring at the given world transform
//--------------------------------------------------------------------------------------
VOID RingLayout::AddRing( CXMMATRIX matWorld )
{
    RingPlacement* pRing = new RingPlacement;
    XMStoreFloat4x4( &pRing->m_matWorld, matWorld );
    pRing->m_fActive = 1.0f;
    m_Rings.push_back( pRing );
}


//--------------------------------------------------------------------------------------
// Adds a series of rings defined by the start & end positions, using Hermite interpolation
//--------------------------------------------------------------------------------------
VOID RingLayout::AddRingCurve( FXMVECTOR vStartPosition, FXMVECTOR vStartFacingDirection, FXMVECTOR vEndPosition, CXMVECTOR vEndFacingDirection, DWORD dwNumRings )
{
    XMVECTOR vDistance = XMVector3LengthEst( vStartPosition - vEndPosition );
    XMVECTOR vNormalScale = vDistance * 0.75f;
    FLOAT fDelta = 1.0f / (FLOAT)dwNumRings;
    FLOAT fParam = 0.0f;
    for( DWORD i = 0; i < dwNumRings; ++i )
    {
        XMVECTOR vPos = XMVectorHermite( vStartPosition, -vStartFacingDirection * vNormalScale, vEndPosition, -vEndFacingDirection * vNormalScale, fParam );
        XMVECTOR vFacing = XMVectorLerp( vStartFacingDirection, vEndFacingDirection, fParam );
        AddRing( vPos, XMVector3Normalize( vFacing ) );
        fParam += fDelta;
    }
}


//--------------------------------------------------------------------------------------
// Clears the ring structs
//--------------------------------------------------------------------------------------
VOID RingLayout::Clear()
{
    m_dwCurrentObjective = 0;
    for( DWORD i = 0; i < m_Rings.size(); ++i )
    {
        delete m_Rings[i];
    }
    m_Rings.clear();
    m_strMessage[0] = L'\0';
}


//--------------------------------------------------------------------------------------
// Resets the player's progress through the rings
//--------------------------------------------------------------------------------------
VOID RingLayout::Reset()
{
    m_fRaceTime = -1.0f;
    m_dwScore = 0;

    if( m_Rings.empty() )
    {
        return;
    }

    m_Rings[0]->m_fActive = 1.0f;
    DWORD Count = (DWORD)m_Rings.size();
    for( DWORD i = 1; i < Count; ++i )
    {
        m_Rings[i]->m_fActive = 0.25f;
    }
    if( Count >= 2 )
    {
        m_Rings[1]->m_fActive = 1.0f;
    }

    m_dwCurrentObjective = 0;
}


//--------------------------------------------------------------------------------------
// Tracks the user's progress through the rings, using the airplane's position and heading
//--------------------------------------------------------------------------------------
VOID RingLayout::Update( FLOAT fDeltaTime, const XMMATRIX& matAirplaneWorld )
{
    m_fTime += fDeltaTime;

    // If we have finished the final ring, we don't track anything until after a reset
    if( m_dwCurrentObjective >= m_Rings.size() )
    {
        return;
    }

    // Keep track of the race elapsed time
    if( m_fRaceTime >= 0.0f )
    {
        m_fRaceTime += fDeltaTime;
        UpdateMessage( 0 );
    }

    // Extract the current ring's position and facing vector
    RingPlacement* pRing = m_Rings[m_dwCurrentObjective];
    XMMATRIX matRing = XMLoadFloat4x4( &pRing->m_matWorld );
    XMVECTOR vRingPos = matRing.r[3];
    XMVECTOR vRingFacing = matRing.r[2];

    // Extract the airplane's position and forward vector
    XMVECTOR vAirplanePos = matAirplaneWorld.r[3];
    XMVECTOR vAirplaneForward = matAirplaneWorld.r[2];

    // Determine if the airplane is close enough to the ring
    XMVECTOR vDistance = XMVector3LengthEst( vAirplanePos - vRingPos );
    if( XMVectorGetX( vDistance ) <= 15.0f )
    {
        // Determine if the player is passing through the ring correctly or not
        XMVECTOR vDot = XMVector3Dot( vAirplaneForward, vRingFacing );
        BOOL bCorrectDirection = ( XMVectorGetX( vDot ) < -0.5f );

        // Record the ring success
        ObjectiveAttained( TRUE, bCorrectDirection );
    }
    else
    {
        // Determine if the airplane has skipped the ring
        // Compute distance from the ring's plane
        XMVECTOR vPlaneToRing = vAirplanePos - vRingPos;
        FLOAT fDistanceFromRingPlane = fabs( XMVectorGetX( XMVector3Dot( vPlaneToRing, vRingFacing ) ) );
        vPlaneToRing = XMVector3Normalize( vPlaneToRing );
        XMVECTOR vDot = XMVector3Dot( vPlaneToRing, vRingFacing );

        // Compute distance to next ring
        FLOAT fDistanceToNextRing = 500.0f;
        if( m_dwCurrentObjective < ( m_Rings.size() - 1 ) )
        {
            XMMATRIX matNextRing = XMLoadFloat4x4( &m_Rings[m_dwCurrentObjective + 1]->m_matWorld );
            XMVECTOR vNextRingPos = matNextRing.r[3];
            fDistanceToNextRing = XMVectorGetX( XMVector3LengthEst( vNextRingPos - vRingPos ) );
        }

        // If the airplane has covered 60% of the distance to the next ring, skip the ring
        if( fDistanceFromRingPlane > ( fDistanceToNextRing * 0.6f ) && XMVectorGetX( vDot ) < -0.5f )
        {
            ObjectiveAttained( FALSE, FALSE );
        }
    }
}


//--------------------------------------------------------------------------------------
// Record that the current ring has been flown through or skipped
//--------------------------------------------------------------------------------------
VOID RingLayout::ObjectiveAttained( BOOL bThrough, BOOL bCorrectDirection )
{
    assert( m_dwCurrentObjective < m_Rings.size() );

    // Start the race when the first ring is passed
    if( m_dwCurrentObjective == 0 )
    {
        m_fRaceTime = 0.0f;
    }

    // Compute a score for this ring
    if( bThrough )
    {
        g_pAudioEngine->PlayCue( "RingPass" );
        m_dwScore += 50;
        if( bCorrectDirection )
        {
            m_dwScore += 50;
        }
    }
    else
    {
        g_pAudioEngine->PlayCue( "RingFail" );
    }

    // Update the message displayed to the user
    UpdateMessage( 0 );

    // Make the current ring disappear and go to the next ring
    m_Rings[m_dwCurrentObjective]->m_fActive = 0.0f;
    ++m_dwCurrentObjective;

    // Check if we have completed the last ring
    if( m_dwCurrentObjective >= m_Rings.size() )
    {
        // completed the last ring!
        m_dwScore += 1000;
        UpdateMessage( 1 );
    }
    else
    {
        // Make the next two rings active
        m_Rings[m_dwCurrentObjective]->m_fActive = 1.0f;
        if( m_dwCurrentObjective < ( m_Rings.size() - 1 ) )
        {
            m_Rings[m_dwCurrentObjective + 1]->m_fActive = 1.0f;
        }
    }
}


//--------------------------------------------------------------------------------------
// Update the status message to reflect that a new course has been selected
//--------------------------------------------------------------------------------------
VOID RingLayout::NewCourseMessage( const WCHAR* strTitle )
{
    swprintf_s( m_strMessage, L"Playing course \"%s\" with %d rings", strTitle, m_Rings.size() );
}


//--------------------------------------------------------------------------------------
// Update the status message displayed to the player
//--------------------------------------------------------------------------------------
VOID RingLayout::UpdateMessage( DWORD dwEvent )
{
    switch( dwEvent )
    {
    case 0:
        if( m_fRaceTime >= 0.0f )
        {
            swprintf_s( m_strMessage, L"Time: %0.1f sec  Score: %d  Rings: %d / %d", m_fRaceTime, m_dwScore, m_dwCurrentObjective, m_Rings.size() );
        }
        break;
    case 1:
        swprintf_s( m_strMessage, L"Complete!  Time: %0.1f sec  Score: %d", m_fRaceTime, m_dwScore );
        break;
    }
}


//--------------------------------------------------------------------------------------
// Render the series of rings
//--------------------------------------------------------------------------------------
VOID RingLayout::Render( IDirect3DDevice9* pd3dDevice, UbershaderParameterPool* pParameterPool, XMMATRIX matViewProjection, XMVECTOR vDirLightWorldDirection )
{
    // Render the rings
    DWORD Count = (DWORD)m_Rings.size();
    for( DWORD i = 0; i < Count; ++i )
    {
        RingPlacement* pRing = m_Rings[i];
        if( pRing->m_fActive <= 0.0f )
        {
            continue;
        }

        XMMATRIX matWorld = XMLoadFloat4x4( &pRing->m_matWorld );
        
        XMMATRIX matScaling = XMMatrixScaling( pRing->m_fActive, pRing->m_fActive, pRing->m_fActive );
        matWorld = matScaling * matWorld;

        XMVECTOR vDeterminant;
        XMMATRIX matInvWorld = XMMatrixInverse( &vDeterminant, matWorld );
        XMVECTOR vDirLightObjDirection = XMVector3TransformNormal( vDirLightWorldDirection, matInvWorld );
        pParameterPool->SetDirLightObjectDir( 0, vDirLightObjDirection );

        XMMATRIX matWVP = matWorld * matViewProjection;
        RenderModel( pd3dDevice, pParameterPool, s_pRingModel, matWVP );
    }

    if( m_dwCurrentObjective >= m_Rings.size() )
    {
        return;
    }

    /*
    // Render direction indicators in front of the current ring, going towards the ring
    RingPlacement* pActiveRing = m_Rings[m_dwCurrentObjective];
    XMMATRIX matWorld = XMLoadFloat4x4( &pActiveRing->m_matWorld );

    const FLOAT fRingSpacing = 20.0f;
    const FLOAT fRingIntervalSeconds = 0.5f;
    const DWORD dwNumRings = 5;

    FLOAT fRingOffset = fmodf( m_fTime, fRingIntervalSeconds );
    for( DWORD i = 0; i < dwNumRings; ++i )
    {
        FLOAT fAlpha = 0.25f;
        if( i == 0 )
        {
            FLOAT fFadeOut = ( fRingIntervalSeconds - fRingOffset ) / fRingIntervalSeconds;
            fAlpha *= fFadeOut;
        }
        else if( i == ( dwNumRings - 1 ) )
        {
            FLOAT fFadeIn = ( fRingOffset ) / fRingIntervalSeconds;
            fAlpha *= fFadeIn;
        }

        FLOAT fDistance = (FLOAT)( i + 1 ) * fRingSpacing + ( fRingOffset * ( -fRingSpacing / fRingIntervalSeconds ) );
        XMVECTOR vOffset = matWorld.r[2] * fDistance;
        matWorld.r[3] += vOffset;
    }
    */
}

VOID RingLayout::RenderModel( IDirect3DDevice9* pd3dDevice, UbershaderParameterPool* pParameterPool, ATG::Model* pModel, XMMATRIX matWorldViewProjection )
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

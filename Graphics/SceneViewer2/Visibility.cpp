//--------------------------------------------------------------------------------------
// Visibility.cpp
//
// Contains methods used for building the sorted scene visibility list every frame.
// These methods are called at the beginning of the render loop, on core 0.
// 
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include "SceneViewer2.h"

FLOAT DistanceFromCamera( XMVECTOR vCameraPos, XMVECTOR vModelPos, FLOAT fMaxRadius )
{
    XMVECTOR vDist = XMVector3LengthEst( vModelPos - vCameraPos );
    FLOAT fDist = vDist.x - fMaxRadius;
    return fabs( fDist );
}


struct ModelDistance
{
    ATG::Model* pModel;
    FLOAT fDistanceFromCamera;
    DWORD dwSceneIndex;
};
typedef std::vector <ModelDistance> ModelDistanceList;

BOOL ModelSortFunction( const ModelDistance& A, const ModelDistance& B )
{
    return A.fDistanceFromCamera < B.fDistanceFromCamera;
}

BOOL ModelTransparentSortFunction( const ModelDistance& A, const ModelDistance& B )
{
    return B.fDistanceFromCamera < A.fDistanceFromCamera;
}

VOID SceneViewer::BuildVisibilityList( ATG::Camera* pCamera )
{
    ATG::Bound ViewBound = pCamera->GetWorldBound();

    DWORD dwModelCount = m_ShadowSceneState.AllModels.size();

    ModelDistanceList ModelSortList;
    ModelSortList.clear();
    ModelSortList.reserve( dwModelCount );
    ModelDistanceList ModelTransparentSortList;
    ModelTransparentSortList.clear();
    ModelTransparentSortList.reserve( dwModelCount );

    XMVECTOR vCameraPos = pCamera->GetWorldPosition();

    ModelDistance md;
    for( DWORD i = 0; i < dwModelCount; ++i )
    {
        const ATG::Bound& WorldBound = m_ShadowSceneState.AllModelWorldBounds[i];
        const XMMATRIX matWorld = m_ShadowSceneState.AllModelWorldTransforms[i];

        md.pModel = m_ShadowSceneState.AllModels[i];
        md.dwSceneIndex = i;
        md.fDistanceFromCamera = DistanceFromCamera( vCameraPos, matWorld.r[3], WorldBound.GetMaxRadius() );

        if( m_bFrustumCulling )
        {
            BOOL bViewResult = WorldBound.Collide( ViewBound );
            if( bViewResult )
            {
                if( md.pModel->ContainsTransparentSubsets() )
                    ModelTransparentSortList.push_back( md );
                if( md.pModel->ContainsOpaqueSubsets() )
                    ModelSortList.push_back( md );
            }
        }
        else
        {
            if( md.pModel->ContainsTransparentSubsets() )
                ModelTransparentSortList.push_back( md );
            if( md.pModel->ContainsOpaqueSubsets() )
                ModelSortList.push_back( md );
        }
    }

    if( m_iIsolatedModelIndex != -1 && m_iIsolatedModelIndex < ( INT )ModelSortList.size() )
    {
        md = ModelSortList[ m_iIsolatedModelIndex ];
        m_ShadowSceneState.VisibleModels.push_back( md.pModel );
        m_ShadowSceneState.VisibleModelWorldBounds.push_back(
            m_ShadowSceneState.AllModelWorldBounds[ md.dwSceneIndex ] );
        m_ShadowSceneState.VisibleModelWorldTransforms.push_back(
            m_ShadowSceneState.AllModelWorldTransforms[ md.dwSceneIndex ] );
        return;
    }

    std::sort( ModelSortList.begin(), ModelSortList.end(), ModelSortFunction );

    DWORD dwVisibleModelCount = ( DWORD )ModelSortList.size();
    m_ShadowSceneState.VisibleModels.reserve( dwVisibleModelCount );
    m_ShadowSceneState.VisibleModelWorldBounds.reserve( dwVisibleModelCount );
    m_ShadowSceneState.VisibleModelWorldTransforms.reserve( dwVisibleModelCount );

    for( DWORD dwIndex = 0; dwIndex < dwVisibleModelCount; ++dwIndex )
    {
        md = ModelSortList[ dwIndex ];
        m_ShadowSceneState.VisibleModels.push_back( md.pModel );
        m_ShadowSceneState.VisibleModelWorldBounds.push_back(
            m_ShadowSceneState.AllModelWorldBounds[ md.dwSceneIndex ] );
        m_ShadowSceneState.VisibleModelWorldTransforms.push_back(
            m_ShadowSceneState.AllModelWorldTransforms[ md.dwSceneIndex ] );
    }

    if( ModelTransparentSortList.size() > 0 )
    {
        std::sort( ModelTransparentSortList.begin(), ModelTransparentSortList.end(), ModelTransparentSortFunction );

        dwVisibleModelCount = ( DWORD )ModelTransparentSortList.size();
        m_ShadowSceneState.VisibleTransparentModels.reserve( dwVisibleModelCount );
        m_ShadowSceneState.VisibleTransparentModelWorldBounds.reserve( dwVisibleModelCount );
        m_ShadowSceneState.VisibleTransparentModelWorldTransforms.reserve( dwVisibleModelCount );

        for( DWORD dwIndex = 0; dwIndex < dwVisibleModelCount; ++dwIndex )
        {
            md = ModelTransparentSortList[ dwIndex ];
            m_ShadowSceneState.VisibleTransparentModels.push_back( md.pModel );
            m_ShadowSceneState.VisibleTransparentModelWorldBounds.push_back(
                m_ShadowSceneState.AllModelWorldBounds[ md.dwSceneIndex ] );
            m_ShadowSceneState.VisibleTransparentModelWorldTransforms.push_back(
                m_ShadowSceneState.AllModelWorldTransforms[ md.dwSceneIndex ] );
        }
    }
}

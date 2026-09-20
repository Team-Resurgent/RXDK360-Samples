//--------------------------------------------------------------------------------------
// ParameterPool.cpp
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include <assert.h>
#include "ParameterPool.h"

VOID SampleParameterPool::Initialize( FXLEffectPool* pEffectPool, FXLEffect* pShaderLibraryEffect )
{
    m_pEffectPool = pEffectPool;

    m_hWorldViewProjMatrix = m_pEffectPool->GetParameterHandle( "world_view_proj_matrix" );

    SetupArrayHandles( "point_light_obj_pos_ranges",
                       m_hPointLightPos,
                       g_dwMaxPointLightCount );
    SetupArrayHandles( "point_light_colors",
                       m_hPointLightColor,
                       g_dwMaxPointLightCount );

    SetupArrayHandles( "dir_light_obj_dirs",
                       m_hDirLightDirs,
                       g_dwMaxDirLightCount );
    SetupArrayHandles( "dir_light_colors",
                       m_hDirLightColor,
                       g_dwMaxDirLightCount );
    SetupArrayHandles( "dir_light_shadow_proj_matrix",
                       m_hDirLightProjMatricesTight,
                       g_dwMaxDirLightCount );
    SetupArrayHandles( "dir_light_shadow_textures",
                       m_hDirLightShadowTexturesTight,
                       g_dwMaxDirLightCount );
    SetupArrayHandles( "dir_light_shadow_proj_matrix_scene",
                       m_hDirLightProjMatricesScene,
                       g_dwMaxDirLightCount );
    SetupArrayHandles( "dir_light_shadow_textures_scene",
                       m_hDirLightShadowTexturesScene,
                       g_dwMaxDirLightCount );

    m_hShadowedSpotLightCount = m_pEffectPool->GetParameterHandle( "NumSpotLightShadowCasters" );
    SetupArrayHandles( "spot_light_obj_pos_ranges",
                       m_hSpotLightPos,
                       g_dwMaxSpotLightCount );
    SetupArrayHandles( "spot_light_colors",
                       m_hSpotLightColor,
                       g_dwMaxSpotLightCount );
    SetupArrayHandles( "spot_light_obj_dirs",
                       m_hSpotLightDirs,
                       g_dwMaxSpotLightCount );
    SetupArrayHandles( "spot_light_inner_outer_angles",
                       m_hSpotLightAngles,
                       g_dwMaxSpotLightCount );
    SetupArrayHandles( "spot_light_shadow_textures",
                       m_hSpotLightShadowTextures,
                       g_dwMaxSpotLightCount );
    SetupArrayHandles( "spot_light_shadow_proj_matrix",
                       m_hSpotLightProjMatrices,
                       g_dwMaxSpotLightCount );

    SetupArrayHandles( "bPointLightCount", m_hPointLightSwitches, g_dwMaxPointLightCount );
    SetupArrayHandles( "bSpotLightCount", m_hSpotLightSwitches, g_dwMaxSpotLightCount );
    SetupArrayHandles( "bDirLightCount", m_hDirLightSwitches, g_dwMaxDirLightCount );

    for( DWORD dwDir = 0; dwDir <= g_dwMaxDirLightCount; ++dwDir )
    {
        for( DWORD dwPoint = 0; dwPoint <= g_dwMaxPointLightCount; ++dwPoint )
        {
            for( DWORD dwSpot = 0; dwSpot <= g_dwMaxSpotLightCount; ++dwSpot )
            {
                DWORD dwTechniqueIndex = ( dwDir * ( g_dwMaxSpotLightCount + 1 ) * ( g_dwMaxPointLightCount + 1 ) )
                    + ( dwPoint * ( g_dwMaxSpotLightCount + 1 ) )
                    + dwSpot;
                m_hShaderLibraryTechniques[dwDir][dwPoint][dwSpot] = pShaderLibraryEffect->GetTechniqueHandleFromIndex(
                    dwTechniqueIndex );
            }
        }
    }

    m_hObjViewDir = m_pEffectPool->GetParameterHandle( "obj_view_direction" );
    m_hObjViewPos = m_pEffectPool->GetParameterHandle( "obj_view_position" );
    m_hAmbient = m_pEffectPool->GetParameterHandle( "ambient" );
    m_hShadowMapMethod = m_pEffectPool->GetParameterHandle( "g_iShadowMapMethod" );
}

VOID SampleParameterPool::SetupArrayHandles( const CHAR* strRootHandleName, FXLHANDLE* pElementHandles,
                                             DWORD dwElementCount )
{
    ZeroMemory( pElementHandles, dwElementCount * sizeof( FXLHANDLE ) );
    FXLHANDLE hRootParam = m_pEffectPool->GetParameterHandle( strRootHandleName );
    if( hRootParam == NULL )
        return;

    FXLPARAMETER_DESC ParamDesc;
    m_pEffectPool->GetParameterDesc( hRootParam, &ParamDesc );
    dwElementCount = min( dwElementCount, ParamDesc.Elements );

    for( DWORD i = 0; i < dwElementCount; ++i )
    {
        pElementHandles[i] = m_pEffectPool->GetElementHandle( hRootParam, i );
    }
}

VOID SampleParameterPool::Terminate()
{
    ZeroMemory( this, sizeof( SampleParameterPool ) );
}

VOID DeferredParameterPool::Initialize( FXLEffectPool* pEffectPool, FXLEffect* pEffect )
{
    m_pEffectPool = pEffectPool;
    if( m_pEffectPool == NULL || pEffect == NULL )
        return;

    m_hWorldViewProjMatrix = pEffectPool->GetParameterHandle( "world_view_proj_matrix" );
    m_hWorldMatrix = pEffectPool->GetParameterHandle( "world_matrix" );
    m_hInvViewProjMatrix = pEffectPool->GetParameterHandle( "inv_view_proj_matrix" );

    m_hLightColor = pEffectPool->GetParameterHandle( "light_color" );
    m_hLightWorldPosRange = pEffectPool->GetParameterHandle( "light_world_pos_range" );
    m_hLightWorldDir = pEffectPool->GetParameterHandle( "light_world_direction" );
    m_hWorldViewDirection = pEffectPool->GetParameterHandle( "world_view_direction" );
    m_hSpotLightAngles = pEffectPool->GetParameterHandle( "spot_light_angles" );

    m_hTechniqueBufferConstruction = pEffect->GetTechniqueHandle( "BuildBuffers" );
    m_hTechniquePointLight = pEffect->GetTechniqueHandle( "PointLight" );
    m_hTechniqueSpotLight = pEffect->GetTechniqueHandle( "SpotLight" );
    m_hTechniqueDirLight = pEffect->GetTechniqueHandle( "DirLight" );
    m_hTechniqueAmbientLight = pEffect->GetTechniqueHandle( "AmbientLight" );

    m_hDepthBufferTexture = pEffectPool->GetParameterHandle( "depth_buffer_texture" );
    m_hColorBufferTexture = pEffectPool->GetParameterHandle( "color_buffer_texture" );
    m_hNormalBufferTexture = pEffectPool->GetParameterHandle( "normal_buffer_texture" );

    m_hShadowingEnabled = pEffectPool->GetParameterHandle( "bShadowedLight" );
    m_hLightWVPMatrix = pEffectPool->GetParameterHandle( "light_world_view_proj_matrix" );
    m_hShadowBufferTexture = pEffectPool->GetParameterHandle( "shadow_buffer_texture" );
    m_hLightWVPMatrixScene = pEffectPool->GetParameterHandle( "light_world_view_proj_matrix_scene" );
    m_hShadowBufferTextureScene = pEffectPool->GetParameterHandle( "shadow_buffer_texture_scene" );
    m_hShadowMapMethod = m_pEffectPool->GetParameterHandle( "g_iShadowMapMethod" );
}

VOID PassPerLightParameterPool::Initialize( FXLEffectPool* pEffectPool, FXLEffect* pEffect )
{
    m_pEffectPool = pEffectPool;
    if( m_pEffectPool == NULL || pEffect == NULL )
        return;

    m_hWorldViewProjMatrix = pEffectPool->GetParameterHandle( "world_view_proj_matrix" );

    m_hLightColor = pEffectPool->GetParameterHandle( "light_color" );
    m_hLightObjPosRange = pEffectPool->GetParameterHandle( "light_obj_pos_range" );
    m_hLightObjDir = pEffectPool->GetParameterHandle( "light_obj_direction" );
    m_hSpotLightAngles = pEffectPool->GetParameterHandle( "spot_light_angles" );
    m_hObjViewDir = pEffectPool->GetParameterHandle( "obj_view_direction" );

    m_hTechniquePointLight = pEffect->GetTechniqueHandle( "PointLight" );
    m_hTechniqueSpotLight = pEffect->GetTechniqueHandle( "SpotLight" );
    m_hTechniqueDirLight = pEffect->GetTechniqueHandle( "DirLight" );
    m_hTechniqueAmbientLight = pEffect->GetTechniqueHandle( "AmbientLight" );

    m_hShadowingEnabled = pEffectPool->GetParameterHandle( "bShadowedLight" );
    m_hLightWVPMatrix = pEffectPool->GetParameterHandle( "light_world_view_proj_matrix" );
    m_hShadowBufferTexture = pEffectPool->GetParameterHandle( "shadow_buffer_texture" );
    m_hLightWVPMatrixScene = pEffectPool->GetParameterHandle( "light_world_view_proj_matrix_scene" );
    m_hShadowBufferTextureScene = pEffectPool->GetParameterHandle( "shadow_buffer_texture_scene" );
    m_hShadowMapMethod = m_pEffectPool->GetParameterHandle( "g_iShadowMapMethod" );
}

VOID PostEffectParameterPool::Initialize( FXLEffect* pEffect )
{
    m_pEffect = pEffect;
    if( m_pEffect == NULL )
        return;

    m_hColorBuffer = pEffect->GetParameterHandle( "color_sampler" );
    m_hDepthBuffer = pEffect->GetParameterHandle( "depth_sampler" );

    m_hFocalSettings = pEffect->GetParameterHandle( "g_FocalSettings" );
}

//--------------------------------------------------------------------------------------
// ParameterPool.cpp
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include <assert.h>
#include "ParameterPool.h"

VOID UbershaderParameterPool::Initialize( FXLEffectPool* pEffectPool )
{
    m_pEffectPool = pEffectPool;

    m_hWorldViewProjMatrix = m_pEffectPool->GetParameterHandle( "world_view_proj_matrix" );

    m_hPointLightCount = m_pEffectPool->GetParameterHandle( "NumPointLights" );
    SetupArrayHandles( "point_light_obj_pos_ranges",
                       m_hPointLightPos,
                       g_dwMaxPointLightCount );
    SetupArrayHandles( "point_light_colors",
                       m_hPointLightColor,
                       g_dwMaxPointLightCount );

    m_hDirLightCount = m_pEffectPool->GetParameterHandle( "NumDirLights" );
    SetupArrayHandles( "dir_light_obj_dirs",
                       m_hDirLightDirs,
                       g_dwMaxDirLightCount );
    SetupArrayHandles( "dir_light_colors",
                       m_hDirLightColor,
                       g_dwMaxDirLightCount );

    m_hSpotLightCount = m_pEffectPool->GetParameterHandle( "NumSpotLights" );
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

    m_hObjViewDir = m_pEffectPool->GetParameterHandle( "obj_view_direction" );
    m_hObjViewPos = m_pEffectPool->GetParameterHandle( "obj_view_position" );
    m_hAmbient = m_pEffectPool->GetParameterHandle( "ambient" );
    m_hShadowMapMethod = m_pEffectPool->GetParameterHandle( "g_iShadowMapMethod" );
}

VOID UbershaderParameterPool::SetupArrayHandles( const CHAR* strRootHandleName, FXLHANDLE* pElementHandles,
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

VOID UbershaderParameterPool::Terminate()
{
    ZeroMemory( this, sizeof( UbershaderParameterPool ) );
}

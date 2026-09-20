//--------------------------------------------------------------------------------------
// AppXuiModule.cpp
//
// Application Xui Module class for samples
//
// Xbox Advanced Technology Group.
// Copyright ( C ) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "stdafx.h"

#include "AppXuiModule.h"
#include "GameConnectScene.h"
#include "GameEndScene.h"
#include "GameListScene.h"
#include "GameLobbyScene.h"
#include "GameOptionsScene.h"
#include "GamePlayScene.h"
#include "GameStartScene.h"
#include "GameTypeScene.h"
#include "HelpOptionsScene.h"
#include "LeaderboardScene.h"
#include "MainMenuScene.h"
#include "QuickMatchScene.h"


namespace ArcadeSample
{
//--------------------------------------------------------------------------------------
// Runs once every frame.
//--------------------------------------------------------------------------------------
HRESULT AppXuiModule::Render( const D3DPRESENT_PARAMETERS& d3dpp, IDirect3DDevice9* d3dDevice )
{
    // Render Xui
    D3DXMATRIX matView;
    if( d3dpp.BackBufferWidth == 960 )
    {
        D3DXMatrixTranslation( &matView, 0.5f * ( 960 - 1280 ), 0.0f, 0.0f );
    }
    else
    {
        D3DXMatrixIdentity( &matView );
    }
    XuiRenderSetViewTransform( this->GetDC(), &matView );

    d3dDevice->BeginScene();

    CXuiModule::Render();

    d3dDevice->EndScene();

    return S_OK;
}

//--------------------------------------------------------------------------------------
// Register Xui classes.
//--------------------------------------------------------------------------------------
HRESULT AppXuiModule::RegisterXuiClasses()
{
    CGameConnectScene::Register();
    CGameEndList::Register();
    CGameEndScene::Register();
    CGameList::Register();
    CGameListScene::Register();
    CGameLobbyList::Register();
    CGameLobbyScene::Register();
    CGameOptionsScene::Register();
    CGamePlayList::Register();
    CGamePlayScene::Register();
    CGameStartScene::Register();
    CGameTypeScene::Register();
    CHelpOptionsScene::Register();
    CLeaderboardList::Register();
    CLeaderboardScene::Register();
    CMainMenuScene::Register();
    CQuickMatchScene::Register();

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Unregister Xui classes.
//--------------------------------------------------------------------------------------
HRESULT AppXuiModule::UnregisterXuiClasses()
{
    CGameConnectScene::Unregister();
    CGameEndList::Unregister();
    CGameEndScene::Unregister();
    CGameList::Unregister();
    CGameListScene::Unregister();
    CGameLobbyList::Unregister();
    CGameLobbyScene::Unregister();
    CGameOptionsScene::Unregister();
    CGamePlayList::Unregister();
    CGamePlayScene::Unregister();
    CGameStartScene::Unregister();
    CGameTypeScene::Unregister();
    CHelpOptionsScene::Unregister();
    CLeaderboardList::Unregister();
    CLeaderboardScene::Unregister();
    CMainMenuScene::Unregister();
    CQuickMatchScene::Unregister();

    return S_OK;
}

} // namespace ArcadeSample

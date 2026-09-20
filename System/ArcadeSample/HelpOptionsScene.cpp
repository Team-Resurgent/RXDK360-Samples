//--------------------------------------------------------------------------------------
// HelpOptionsScene.cpp
//--------------------------------------------------------------------------------------

#include "stdafx.h"
#include "HelpOptionsScene.h"


namespace ArcadeSample
{
//--------------------------------------------------------------------------------------
// Initialize the scene
//--------------------------------------------------------------------------------------
HRESULT CHelpOptionsScene::OnInit( XUIMessageInit* pInitData, BOOL& bHandled )
{
    HRESULT hResult = AppXuiScene::OnInit( pInitData, bHandled );

    hResult = GetChildById( L"btnBack", &m_btnBack );

    return hResult;
}

//--------------------------------------------------------------------------------------
// Handle a button press
//--------------------------------------------------------------------------------------
HRESULT CHelpOptionsScene::OnNotifyPressEx( HXUIOBJ hObjSource, XUINotifyPress* pNotifyPress, BOOL& bHandled )
{
    HRESULT hResult = AppXuiScene::OnNotifyPressEx( hObjSource, pNotifyPress, bHandled );
    if( !bHandled )
    {
        // Which button did they press?
        if( hObjSource == m_btnBack )
        {
            hResult = NavigateBack();
        }
        bHandled = TRUE;
    }
    return hResult;
}

} // namespace ArcadeSample

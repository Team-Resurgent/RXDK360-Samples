//--------------------------------------------------------------------------------------
// AppXuiRectControl.h
//
// Application Xui Rect Control class for samples
//
// Xbox Advanced Technology Group.
// Copyright ( C ) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "stdafx.h"

#include "AppXuiRectControl.h"


namespace ArcadeSample
{
//--------------------------------------------------------------------------------------
// Query the Control for the Rect member
//--------------------------------------------------------------------------------------
VOID AppXuiRectControl::OnQueryControl()
{
    if( m_Ctrl )
    {
        D3DXVECTOR3 vPos;
        m_Ctrl->GetPosition( &vPos );
        m_Rect.x = vPos.x;
        m_Rect.y = vPos.y;

        m_Ctrl->GetBounds( &m_Rect.width, &m_Rect.height );
    }
    else
    {
        ZeroMemory( &m_Rect, sizeof( m_Rect ) );
    }
}

//--------------------------------------------------------------------------------------
// Update the Control with the Rect member
//--------------------------------------------------------------------------------------
VOID AppXuiRectControl::OnUpdateControl()
{
    D3DXVECTOR3 vPos( m_Rect.x, m_Rect.y , 0 );
    m_Ctrl->SetPosition( &vPos );
}

} // namespace ArcadeSample

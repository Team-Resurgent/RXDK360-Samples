//--------------------------------------------------------------------------------------
// AppXuiTextControl.h
//
// Application Xui Text Control class for samples
//
// Xbox Advanced Technology Group.
// Copyright ( C ) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "stdafx.h"

#include "AppXuiTextControl.h"


namespace ArcadeSample
{
//--------------------------------------------------------------------------------------
// Query the Control for the Rect member
//--------------------------------------------------------------------------------------
VOID AppXuiTextControl::OnQueryControl()
{
    if( m_Ctrl )
        m_Text = m_Ctrl->GetText();
    else
        m_Text.clear();
}

//--------------------------------------------------------------------------------------
// Update the Control with the Rect member
//--------------------------------------------------------------------------------------
VOID AppXuiTextControl::OnUpdateControl()
{
    m_Ctrl->SetText( m_Text.c_str() );
}

} // namespace ArcadeSample

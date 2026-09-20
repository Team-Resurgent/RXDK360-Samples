//--------------------------------------------------------------------------------------
// AppXuiControl.h
//
// Application Xui Control class for samples
//
// Xbox Advanced Technology Group.
// Copyright ( C ) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#pragma once
#ifndef ARCADESAMPLE_APPXUICONTROL_H
#define ARCADESAMPLE_APPXUICONTROL_H


namespace ArcadeSample
{
//--------------------------------------------------------------------------------------
// IAppXuiControl interface
//--------------------------------------------------------------------------------------
class IAppXuiControl
{
public:
    virtual         ~IAppXuiControl()
    {
    }

    VOID            UpdateXui()
    {
        this->OnUpdateXui();
    }

protected:
    virtual VOID    OnUpdateXui()
    {
    }
};

//--------------------------------------------------------------------------------------
// AppXuiControl base class
//--------------------------------------------------------------------------------------
class AppXuiControl : public IAppXuiControl
{
public:
                    AppXuiControl() : m_Ctrl( NULL )
                    {
                    }
    virtual         ~AppXuiControl()
    {
    }

    VOID            BindControl( CXuiControl* ctrl );
    VOID            QueryControl();
    VOID            UpdateControl();

    VOID            SetShow( BOOL bShow );

protected:
    virtual VOID    OnUpdateXui()
    {
        this->UpdateControl();
    }
    virtual VOID    OnQueryControl()
    {
    }
    virtual VOID    OnUpdateControl()
    {
    }

protected:
    CXuiControl* m_Ctrl;
};

//--------------------------------------------------------------------------------------
// Bind to the specified CXuiControl
//--------------------------------------------------------------------------------------
inline VOID AppXuiControl::BindControl( CXuiControl* ctrl )
{
    m_Ctrl = ctrl;
    this->QueryControl();
}

//--------------------------------------------------------------------------------------
// Query the bound CXuiControl for details
//--------------------------------------------------------------------------------------
inline VOID AppXuiControl::QueryControl()
{
    this->OnQueryControl();
}

//--------------------------------------------------------------------------------------
// Update the bound CXuiControl with details
//--------------------------------------------------------------------------------------
inline VOID AppXuiControl::UpdateControl()
{
    ATG_Verify( m_Ctrl );
    this->OnUpdateControl();
}

//--------------------------------------------------------------------------------------
// Set the Show property of the bound CXuiControl
//--------------------------------------------------------------------------------------
inline VOID AppXuiControl::SetShow( BOOL bShow )
{
    if( m_Ctrl ) m_Ctrl->SetShow( bShow );
}

} // namespace ArcadeSample

#endif // ARCADESAMPLE_APPXUICONTROL_H

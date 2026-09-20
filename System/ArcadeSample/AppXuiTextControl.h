//--------------------------------------------------------------------------------------
// AppXuiTextControl.h
//
// Application Xui Text Control class for samples
//
// Xbox Advanced Technology Group.
// Copyright ( C ) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#pragma once
#ifndef ARCADESAMPLE_APPXUITEXTCONTROL_H
#define ARCADESAMPLE_APPXUITEXTCONTROL_H

#include "AppXuiControl.h"

namespace ArcadeSample
{
//--------------------------------------------------------------------------------------
// AppXuiTextControl base class
//--------------------------------------------------------------------------------------
class AppXuiTextControl : public AppXuiControl
{
public:
                    AppXuiTextControl()
                    {
                    }
    virtual         ~AppXuiTextControl()
    {
    }

protected:
    virtual VOID    OnQueryControl();
    virtual VOID    OnUpdateControl();

protected:
    std::wstring m_Text;
};

} // namespace ArcadeSample

#endif // ARCADESAMPLE_APPXUITEXTCONTROL_H

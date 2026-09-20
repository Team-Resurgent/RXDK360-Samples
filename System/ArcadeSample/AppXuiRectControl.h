//--------------------------------------------------------------------------------------
// AppXuiRectControl.h
//
// Application Xui Rect Control class for samples
//
// Xbox Advanced Technology Group.
// Copyright ( C ) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#pragma once
#ifndef ARCADESAMPLE_APPXUIRECTCONTROL_H
#define ARCADESAMPLE_APPXUIRECTCONTROL_H

#include "AppXuiControl.h"

namespace ArcadeSample
{
//--------------------------------------------------------------------------------------
// AppXuiRectControl base class
//--------------------------------------------------------------------------------------
class AppXuiRectControl : public AppXuiControl
{
public:
                    AppXuiRectControl()
                    {
                    }
    virtual         ~AppXuiRectControl()
    {
    }

    Rect const& GetRect() const
    {
        return m_Rect;
    }
    VOID            SetRect( Rect const& rect )
    {
        m_Rect = rect;
    }

protected:
    virtual VOID    OnQueryControl();
    virtual VOID    OnUpdateControl();

protected:
    Rect m_Rect;
};

} // namespace ArcadeSample

#endif // ARCADESAMPLE_APPXUIRECTCONTROL_H

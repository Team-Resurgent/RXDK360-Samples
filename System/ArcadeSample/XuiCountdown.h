//--------------------------------------------------------------------------------------
// XuiCountdown.h
//
// XuiCountdown class for samples
//
// Xbox Advanced Technology Group.
// Copyright ( C ) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#pragma once
#ifndef ARCADESAMPLE_XUICOUNTDOWN_H
#define ARCADESAMPLE_XUICOUNTDOWN_H

#include "AppXuiTextControl.h"

namespace ArcadeSample
{
//--------------------------------------------------------------------------------------
// XuiCountdown Class
//--------------------------------------------------------------------------------------
class XuiCountdown : public AppXuiTextControl
{
public:
                    XuiCountdown() : m_iCountdown( 0 )
                    {
                    }
    virtual         ~XuiCountdown()
    {
    }

    VOID            SetCountdown( INT iCountdown );
    INT             UpdateCountdown( DWORD dwTickDelta );

protected:
    virtual VOID    OnUpdateXui();

private:
    INT m_iCountdown;
};

//--------------------------------------------------------------------------------------
// Set countdown to time specified
//--------------------------------------------------------------------------------------
inline VOID XuiCountdown::SetCountdown( INT iCountdown )
{
    m_iCountdown = iCountdown;
}

} // namespace ArcadeSample

#endif // ARCADESAMPLE_XUICOUNTDOWN_H

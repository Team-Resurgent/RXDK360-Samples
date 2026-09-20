//--------------------------------------------------------------------------------------
// XuiScore.h
//
// XuiScore class for samples
//
// Xbox Advanced Technology Group.
// Copyright ( C ) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#pragma once
#ifndef ARCADESAMPLE_XUISCORE_H
#define ARCADESAMPLE_XUISCORE_H

#include "AppXuiTextControl.h"

namespace ArcadeSample
{
//--------------------------------------------------------------------------------------
// XuiScore Class
//--------------------------------------------------------------------------------------
class XuiScore : public AppXuiTextControl
{
public:
                    XuiScore() : m_dwScore( 0 )
                    {
                    }
    virtual         ~XuiScore()
    {
    }

    DWORD           GetScore() const
    {
        return m_dwScore;
    }
    VOID            SetScore( DWORD dwScore )
    {
        m_dwScore = dwScore;
    }
    VOID            AddScore( INT delta )
    {
        m_dwScore += delta;
    }

protected:
    virtual VOID    OnUpdateXui();

private:
    DWORD m_dwScore;
};

} // namespace ArcadeSample

#endif // ARCADESAMPLE_XUISCORE_H

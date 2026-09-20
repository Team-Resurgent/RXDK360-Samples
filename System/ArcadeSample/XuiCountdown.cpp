//--------------------------------------------------------------------------------------
// XuiCountdown.cpp
//
// XuiCountdown class for samples
//
// Xbox Advanced Technology Group.
// Copyright ( C ) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "stdafx.h"
#include "XuiCountdown.h"

#include <sstream>

namespace ArcadeSample
{
//--------------------------------------------------------------------------------------
// Update the countdown according to the tick delta specified
// Returns the current countdown timer, a value of:
//   countdown > 0 : countdown has not expired
//   countdown == 0 : countdown just expired
//   countdown < 0 : countdown has been expired
//--------------------------------------------------------------------------------------
INT XuiCountdown::UpdateCountdown( DWORD dwTickDelta )
{
    if( m_iCountdown <= 0 )
    {
        m_iCountdown = -1;
    }
    else
    {
        m_iCountdown -= dwTickDelta;
        if( m_iCountdown <= 0 )
        {
            m_iCountdown = 0;
        }
    }
    return m_iCountdown;
}

//--------------------------------------------------------------------------------------
// Update the Xui parts
//--------------------------------------------------------------------------------------
VOID XuiCountdown::OnUpdateXui()
{
    std::wostringstream os;
    os << ( ( m_iCountdown / 1000 ) + 1 );
    m_Text = os.str();

    SetShow( m_iCountdown > 0 );
    AppXuiTextControl::OnUpdateXui();
}

} // namespace ArcadeSample


//--------------------------------------------------------------------------------------
// XuiScore.cpp
//
// XuiScore class for samples
//
// Xbox Advanced Technology Group.
// Copyright ( C ) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "stdafx.h"
#include "XuiScore.h"

#include <sstream>

namespace ArcadeSample
{
//--------------------------------------------------------------------------------------
// XuiScore Class
//--------------------------------------------------------------------------------------
VOID XuiScore::OnUpdateXui()
{
    std::wostringstream os;
    os << m_dwScore;
    m_Text = os.str();

    AppXuiTextControl::OnUpdateXui();
}

} // namespace ArcadeSample


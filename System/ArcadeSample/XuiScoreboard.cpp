//--------------------------------------------------------------------------------------
// XuiScoreboard.h
//
// XuiScoreboard class for samples
//
// Xbox Advanced Technology Group.
// Copyright ( C ) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "stdafx.h"
#include "XuiScoreboard.h"


namespace ArcadeSample
{
//--------------------------------------------------------------------------------------
// Update all Xui parts
//--------------------------------------------------------------------------------------
VOID XuiScoreboard::OnUpdateXui()
{
    for( eTeam team = eTeam_First; team < eTeam_Count; team = static_cast<eTeam>( team + 1 ) )
        m_TeamXuiScores[ team ].UpdateXui();

    m_XuiCountdown.UpdateXui();
}

} // namespace ArcadeSample


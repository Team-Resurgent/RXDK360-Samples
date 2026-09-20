//--------------------------------------------------------------------------------------
// XuiScoreboard.h
//
// XuiScoreboard class for samples
//
// Xbox Advanced Technology Group.
// Copyright ( C ) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#pragma once
#ifndef ARCADESAMPLE_XUISCOREBOARD_H
#define ARCADESAMPLE_XUISCOREBOARD_H

#include "AppXuiControl.h"
#include "XuiScore.h"
#include "XuiCountdown.h"

namespace ArcadeSample
{
//--------------------------------------------------------------------------------------
// Scoreboard Class
//--------------------------------------------------------------------------------------
class XuiScoreboard : public IAppXuiControl
{
public:
                    XuiScoreboard() : m_dwEndGameScore( 3 )
                    {
                    }
    virtual         ~XuiScoreboard()
    {
    }

    VOID            BindXuiScoreControl( eTeam team, CXuiControl* ctrl );
    VOID            BindXuiCountdownControl( CXuiControl* ctrl );

    DWORD           GetScore( eTeam team ) const;
    VOID            SetScore( eTeam team, DWORD dwScore );
    VOID            ClearScore();

    DWORD           GetEndGameScore() const;
    VOID            SetEndGameScore( DWORD dwScore );

    BOOL            IsWinningTeam( eTeam team ) const;
    BOOL            IsGameOver() const;

    VOID            SetCountdown( INT iCountdown );
    INT             UpdateCountdown( DWORD dwTickDelta );

protected:
    virtual VOID    OnUpdateXui();

private:
    DWORD m_dwEndGameScore;

    XuiScore        m_TeamXuiScores[ eTeam_Count ];
    XuiCountdown m_XuiCountdown;
};

//--------------------------------------------------------------------------------------
// Bind specified CXuiControl to XuiScore for given team
//--------------------------------------------------------------------------------------
inline VOID XuiScoreboard::BindXuiScoreControl( eTeam team, CXuiControl* ctrl )
{
    m_TeamXuiScores[ team ].BindControl( ctrl );
}

//--------------------------------------------------------------------------------------
// Bind specified CXuiControl to XuiCountdown 
//--------------------------------------------------------------------------------------
inline VOID XuiScoreboard::BindXuiCountdownControl( CXuiControl* ctrl )
{
    m_XuiCountdown.BindControl( ctrl );
}

//--------------------------------------------------------------------------------------
// Get Score for given team 
//--------------------------------------------------------------------------------------
inline DWORD XuiScoreboard::GetScore( eTeam team ) const
{
    return m_TeamXuiScores[ team ].GetScore();
}

//--------------------------------------------------------------------------------------
// Set Score for given team 
//--------------------------------------------------------------------------------------
inline VOID XuiScoreboard::SetScore( eTeam team, DWORD dwScore )
{
    return m_TeamXuiScores[ team ].SetScore( dwScore );
}

//--------------------------------------------------------------------------------------
// Clear Score for all teams
//--------------------------------------------------------------------------------------
inline VOID XuiScoreboard::ClearScore()
{
    m_TeamXuiScores[ eTeam_Left ].SetScore( 0 );
    m_TeamXuiScores[ eTeam_Right ].SetScore( 0 );
}

//--------------------------------------------------------------------------------------
// Get current end game score
//--------------------------------------------------------------------------------------
inline DWORD XuiScoreboard::GetEndGameScore() const
{
    return m_dwEndGameScore;
}

//--------------------------------------------------------------------------------------
// Set end game score
//--------------------------------------------------------------------------------------
inline VOID XuiScoreboard::SetEndGameScore( DWORD dwScore )
{
    m_dwEndGameScore = dwScore;
}

//--------------------------------------------------------------------------------------
// Is specified team winning?
//--------------------------------------------------------------------------------------
inline BOOL XuiScoreboard::IsWinningTeam( eTeam team ) const
{
    return ( team == eTeam_Left )
        ? ( GetScore( eTeam_Left ) > GetScore( eTeam_Right ) )
        : ( GetScore( eTeam_Left ) < GetScore( eTeam_Right ) );
}

//--------------------------------------------------------------------------------------
// Is game over?
//--------------------------------------------------------------------------------------
inline BOOL XuiScoreboard::IsGameOver() const
{
    return ( ( GetScore( eTeam_Left ) >= m_dwEndGameScore ) || ( GetScore( eTeam_Right ) >= m_dwEndGameScore ) );
}

//--------------------------------------------------------------------------------------
// Set the countdown to the time specified
//--------------------------------------------------------------------------------------
inline VOID XuiScoreboard::SetCountdown( INT iCountdown )
{
    m_XuiCountdown.SetCountdown( iCountdown );
}

//--------------------------------------------------------------------------------------
// Update the countdown appropriately
//--------------------------------------------------------------------------------------
inline INT XuiScoreboard::UpdateCountdown( DWORD dwTickDelta )
{
    return m_XuiCountdown.UpdateCountdown( dwTickDelta );
}

} // namespace ArcadeSample

#endif // ARCADESAMPLE_XUISCOREBOARD_H

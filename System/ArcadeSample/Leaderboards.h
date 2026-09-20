//--------------------------------------------------------------------------------------
// Leaderboards.h
//
//--------------------------------------------------------------------------------------
#pragma once
#ifndef ARCADESAMPLE_LEADERBOARDS_H
#define ARCADESAMPLE_LEADERBOARDS_H

#include "XALeaderboards.h"


namespace ArcadeSample
{
//--------------------------------------------------------------------------------------
// LeaderboardItem
//--------------------------------------------------------------------------------------
class LeaderboardItem
{
public:
    // Member getters
    DWORD       GetRank() const
    {
        return m_dwRank;
    }
    LONGLONG    GetRating() const
    {
        return m_i64Rating;
    }
    LONGLONG    GetLosses() const
    {
        return m_i64Losses;
    }
    XUID        GetXUID() const
    {
        return m_Xuid;
    }
    const WCHAR* GetGamertag() const
    {
        return m_wstrGamertag;
    }

    // Member setters
    VOID        Clear()
    {
        ZeroMemory( this, sizeof( LeaderboardItem ) );
    }

    VOID        SetRank( DWORD dwRank )
    {
        m_dwRank = dwRank;
    }
    VOID        SetRating( LONGLONG i64Rating )
    {
        m_i64Rating = i64Rating;
    }
    VOID        SetLosses( LONGLONG i64Losses )
    {
        m_i64Losses = i64Losses;
    }
    VOID        SetXUID( XUID xuid )
    {
        m_Xuid = xuid;
    }
    VOID        SetGamertag( const CHAR* strGamertag );
    VOID        SetGamertag( const WCHAR* wstrGamertag );

private:
    DWORD m_dwRank;
    LONGLONG m_i64Rating;
    LONGLONG m_i64Losses;
    XUID m_Xuid;
    WCHAR       m_wstrGamertag[ XUSER_NAME_SIZE + 1 ];
};

//--------------------------------------------------------------------------------------
// Set wstr Gamertag from str
//--------------------------------------------------------------------------------------
inline VOID LeaderboardItem::SetGamertag( const CHAR* strGamertag )
{
    size_t convertedChars = 0;
    mbstowcs_s( &convertedChars, m_wstrGamertag, XUSER_NAME_SIZE + 1, strGamertag, XUSER_NAME_SIZE );
    m_wstrGamertag[ XUSER_NAME_SIZE ] = L'\0';
}

//--------------------------------------------------------------------------------------
// Set wstr Gamertag from wstr
//--------------------------------------------------------------------------------------
inline VOID LeaderboardItem::SetGamertag( const WCHAR* wstrGamertag )
{
    wcsncpy_s( m_wstrGamertag, XUSER_NAME_SIZE + 1, wstrGamertag, XUSER_NAME_SIZE );
    m_wstrGamertag[ XUSER_NAME_SIZE ] = L'\0';
}

//--------------------------------------------------------------------------------------
// Leaderboards
//--------------------------------------------------------------------------------------
class Leaderboards : public CXALeaderboards <LeaderboardItem, 50, 5>
{
    HRESULT FillItemFromRow( LeaderboardItem* pItem, PXUSER_STATS_ROW pRow )
    {
        pItem->Clear();
        pItem->SetRank( pRow->dwRank );
        pItem->SetRating( pRow->i64Rating );
        pItem->SetXUID( pRow->xuid );
        pItem->SetGamertag( pRow->szGamertag );

        HRESULT hResult = ERROR_INVALID_PARAMETER;
        if( pRow->dwNumColumns == 1 )
        {
            if( pRow->pColumns[ 0 ].wColumnId == STATS_COLUMN_OVERALL_LOSSES )
            {
                pItem->SetLosses( pRow->pColumns[ 0 ].Value.i64Data );
                hResult = S_OK;
            }
        }

        return hResult;
    }
};

} // namespace ArcadeSample

#endif // ARCADESAMPLE_LEADERBOARDS_H

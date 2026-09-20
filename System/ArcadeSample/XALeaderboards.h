//--------------------------------------------------------------------------------------
// XALeaderboards.h
//
// Example usage:
//
// struct MyItem
// {
//     DWORD m_dwRank;
//     LONGLONG m_i64Rating;
//     LONGLONG m_i64Losses;
//     XUID m_Xuid;
//     WCHAR m_wstrGamertag[ XUSER_NAME_SIZE + 1 ];
// };
//
// class CMyLeaderboards : public CXALeaderboards<MyItem>
// {
//     HRESULT FillItemFromRow( MyItem* pItem, PXUSER_STATS_ROW pRow )
//     {
//         pItem->Clear();
//         pItem->SetRank( pRow->dwRank );
//         pItem->SetRating( pRow->i64Rating );
//         pItem->SetXUID( pRow->xuid );
//         pItem->SetGamertag( pRow->szGamertag );

//         HRESULT hResult = ERROR_INVALID_PARAMETER;
//         if( pRow->dwNumColumns == 1 )
//         { 
//             if( pRow->pColumns[ 0 ].wColumnId == STATS_COLUMN_OVERALL_LOSSES )
//             {
//                 pItem->SetLosses( pRow->pColumns[ 0 ].Value.i64Data );
//                 hResult = S_OK;
//             }
//         }
//         return hResult;
//     }
// };
//
// Xbox Advanced Technology Group.
// Copyright ( C ) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#pragma once
#ifndef ARCADESAMPLE_XALEADERBOARDS_H
#define ARCADESAMPLE_XALEADERBOARDS_H

#include <algorithm>


template <class Item, DWORD dwMaxItemsPerBlock = 50, DWORD dwMaxBlocks = 5> class CXALeaderboards
{
protected:
    //--------------------------------------------------------------------------------------
    // TO BE IMPLEMENTED IN A DERIVED CLASS.
    // Fill in a leaderboard item from a stats row. 
    //--------------------------------------------------------------------------------------
    virtual HRESULT FillItemFromRow( Item* pItem, PXUSER_STATS_ROW pRow ) = 0;

public:
    //--------------------------------------------------------------------------------------
    // Constructor / Destructor
    //--------------------------------------------------------------------------------------
                    CXALeaderboards() : m_State( State_Idle ),
                                        m_dwUserIndex( XUSER_INDEX_NONE ),
                                        m_Xuid( 0 ),
                                        m_dwDefaultItem( 0 ),
                                        m_dwTotalItems( 0 ),
                                        m_hEnum( NULL ),
                                        m_pEnumMemory( NULL ),
                                        m_dwBytesEnumMemory( 0 ),
                                        m_dwNumFriends( 0 ),
                                        m_bFriendsValid( FALSE ),
                                        m_dwTick( 0 ),
                                        m_bRefreshing( FALSE )
                    {
                        ZeroMemory( &m_Blocks, sizeof( m_Blocks ) );
                        ZeroMemory( &m_Overlapped, sizeof( m_Overlapped ) );
                        ZeroMemory( &m_StatsSpec, sizeof( m_StatsSpec ) );
                        ZeroMemory( &m_FriendsXuids, sizeof( m_FriendsXuids ) );
                    }

                    ~CXALeaderboards()
                    {
                        Reset();
                    }

    //--------------------------------------------------------------------------------------
    // Cancel any pending operations and clear data.
    //--------------------------------------------------------------------------------------
    VOID            Reset()
    {
        if( m_State != State_Idle )
        {
            XCancelOverlapped( &m_Overlapped );

            if( m_hEnum != NULL )
            {
                CloseHandle( m_hEnum );
                m_hEnum = NULL;
            }

            m_State = State_Idle;
        }

        ZeroMemory( &m_Blocks, sizeof( m_Blocks ) );
        m_dwDefaultItem = 0;
        m_dwTotalItems = 0;
        m_bFriendsValid = FALSE;
    }

    //--------------------------------------------------------------------------------------
    // Mark the friends list as invalid. It will be reloaded at the next refresh.
    // Call then when the friends list changes.
    //--------------------------------------------------------------------------------------
    VOID            InvalidateFriends()
    {
        m_bFriendsValid = FALSE;
    }

    //--------------------------------------------------------------------------------------
    // Do work. Call this every frame.
    //--------------------------------------------------------------------------------------
    VOID            DoWork()
    {
        ++m_dwTick;

        if( ( m_State != State_Idle ) && XHasOverlappedIoCompleted( &m_Overlapped ) )
        {
            switch( m_State )
            {
                case State_FriendsEnum:
                    DoWork_FriendsEnum();
                    break;

                case State_StatsEnumByFriend:
                    DoWork_StatsEnumByFriend();
                    break;

                case State_StatsEnumByRank:
                case State_StatsEnumByXuid:
                    DoWork_StatsEnumByRankOrXuid();
                    break;
            }
        }
    }

    //--------------------------------------------------------------------------------------
    // Refresh the leaderboard.
    // Call this when the active user or leaderboard changes.
    //--------------------------------------------------------------------------------------
    enum StatsFilter
    {
        StatsFilter_Overall,
        StatsFilter_MyScore,
        StatsFilter_Friends
    };

    HRESULT         Refresh( DWORD dwUserIndex, XUSER_STATS_SPEC* pStatsSpec, StatsFilter statsFilter )
    {
        HRESULT hResult = S_OK;
        Reset();

        m_dwUserIndex = dwUserIndex;
        memcpy( &m_StatsSpec, pStatsSpec, sizeof( XUSER_STATS_SPEC ) );

        DWORD dwResult = XUserGetXUID( m_dwUserIndex, &m_Xuid );
        if( dwResult != ERROR_SUCCESS )
        {
            hResult = HRESULT_FROM_WIN32( dwResult );
        }

        if( SUCCEEDED( hResult ) )
        {
            switch( statsFilter )
            {
                case StatsFilter_Overall:
                    hResult = StatsEnumByRank( 1 );
                    break;

                case StatsFilter_MyScore:
                    hResult = StatsEnumByXuid( m_Xuid );
                    break;

                case StatsFilter_Friends:
                    if( !m_bFriendsValid )
                    {
                        hResult = FriendsEnum( m_dwUserIndex );
                    }
                    else
                    {
                        hResult = StatsEnumByFriend();
                    }
                    break;
            }
        }

        if( SUCCEEDED( hResult ) )
        {
            m_bRefreshing = TRUE;
        }

        return hResult;
    }

    //--------------------------------------------------------------------------------------
    // Is the leaderboard refreshing?
    //--------------------------------------------------------------------------------------
    BOOL            IsRefreshing()
    {
        return m_bRefreshing;
    }

    //--------------------------------------------------------------------------------------
    // Get the default item for this leaderboard
    //--------------------------------------------------------------------------------------
    DWORD           GetDefaultItem()
    {
        return m_dwDefaultItem;
    }

    //--------------------------------------------------------------------------------------
    // Get the total items for this leaderboard
    //--------------------------------------------------------------------------------------
    DWORD           GetTotalItemCount()
    {
        return m_dwTotalItems;
    }

    //--------------------------------------------------------------------------------------
    // Get a specific item on this leaderboard. Returns NULL if the item is not
    // currently loaded.
    //--------------------------------------------------------------------------------------
    const Item* GetItem( DWORD dwItem )
    {
        if( ( dwItem < 0 ) || ( dwItem >= m_dwTotalItems ) )
        {
            // Out of range
            return NULL;
        }

        for( DWORD dwBlock = 0; dwBlock < dwMaxBlocks; ++dwBlock )
        {
            if( ( m_Blocks[ dwBlock ].bValid ) &&
                ( m_Blocks[ dwBlock ].dwStart <= dwItem ) &&
                ( ( m_Blocks[ dwBlock ].dwStart + m_Blocks[ dwBlock ].dwCount ) > dwItem ) )
            {
                // Found a block containing the item
                m_Blocks[ dwBlock ].dwTick = m_dwTick;

                DWORD dwIndex = dwItem - m_Blocks[ dwBlock ].dwStart;
                return &m_Blocks[ dwBlock ].items[ dwIndex ];
            }
        }

        // We didn't find an item, kick off a new enumeration
        if( m_State == State_Idle )
        {
            StatsEnumByRank( GetBlockStart( dwItem ) + 1 );
        }

        return NULL;
    }

protected:
    enum State
    {
        State_Idle,
        State_FriendsEnum,
        State_StatsEnumByFriend,
        State_StatsEnumByRank,
        State_StatsEnumByXuid
    };

    struct ItemBlock
    {
        BOOL bValid;
        DWORD dwTick;
        DWORD dwStart;
        DWORD dwCount;
        Item items[ dwMaxItemsPerBlock ];
    };

    //--------------------------------------------------------------------------------------
    // Get an available item block
    //--------------------------------------------------------------------------------------
    ItemBlock* GetAvailableBlock()
    {
        DWORD dwBestBlock = 0;
        for( DWORD dwBlock = 0; dwBlock < dwMaxBlocks; ++dwBlock )
        {
            if( !m_Blocks[ dwBlock ].bValid )
            {
                // Found an unused block, return it
                return &m_Blocks[ dwBlock ];
            }

            if( m_Blocks[ dwBlock ].dwTick < m_Blocks[ dwBestBlock ].dwTick )
            {
                // Found a block that is less recently used than the previous best
                dwBestBlock = dwBlock;
            }
        }

        // Return the least recently used block
        m_Blocks[ dwBestBlock ].bValid = FALSE;
        return &m_Blocks[ dwBestBlock ];
    }

    //--------------------------------------------------------------------------------------
    // Get an appropriate starting item for a block
    //--------------------------------------------------------------------------------------
    DWORD           GetBlockStart( DWORD dwItem )
    {
        return ( dwItem / dwMaxItemsPerBlock ) * dwMaxItemsPerBlock;
    }

    //--------------------------------------------------------------------------------------
    // Allocate the enumeration buffer
    //--------------------------------------------------------------------------------------
    HRESULT         AllocEnumBuffer( DWORD dwBytesEnumMemory )
    {
        HRESULT hResult = S_OK;

        if( m_dwBytesEnumMemory < dwBytesEnumMemory )
        {
            if( m_pEnumMemory != NULL )
            {
                delete [] m_pEnumMemory;
                m_pEnumMemory = NULL;
                m_dwBytesEnumMemory = 0;
            }

            m_pEnumMemory = new BYTE[ dwBytesEnumMemory ];
            if( m_pEnumMemory == NULL )
            {
                hResult = E_OUTOFMEMORY;
            }
            else
            {
                m_dwBytesEnumMemory = dwBytesEnumMemory;
            }
        }

        return hResult;
    }

    //--------------------------------------------------------------------------------------
    // Enumerate friends
    //--------------------------------------------------------------------------------------
    HRESULT         FriendsEnum( DWORD dwUserIndex )
    {
        HRESULT hResult = S_OK;
        DWORD dwBytesEnumMemory = 0;

        if( m_State != State_Idle )
        {
            return E_UNEXPECTED;
        }

        // Create the enumerator
        DWORD dwResult = XFriendsCreateEnumerator(
            dwUserIndex,
            0, MAX_FRIENDS,
            &dwBytesEnumMemory,
            &m_hEnum
            );

        if( dwResult != ERROR_SUCCESS )
        {
            hResult = HRESULT_FROM_WIN32( dwResult );
        }

        if( SUCCEEDED( hResult ) )
        {
            // Allocate the buffer
            hResult = AllocEnumBuffer( dwBytesEnumMemory );
        }

        if( SUCCEEDED( hResult ) )
        {
            // Start the enumeration
            ZeroMemory( &m_Overlapped, sizeof( m_Overlapped ) );
            dwResult = XEnumerate(
                m_hEnum, m_pEnumMemory, m_dwBytesEnumMemory, NULL, &m_Overlapped
                );

            if( dwResult != ERROR_IO_PENDING )
            {
                hResult = HRESULT_FROM_WIN32( dwResult );
            }
        }

        if( SUCCEEDED( hResult ) )
        {
            m_State = State_FriendsEnum;
        }
        else
        {
            if( m_hEnum != NULL )
            {
                CloseHandle( m_hEnum );
                m_hEnum = NULL;
            }
        }

        return hResult;
    }

    //--------------------------------------------------------------------------------------
    // Enumerate stats by friends
    //--------------------------------------------------------------------------------------
    HRESULT         StatsEnumByFriend()
    {
        HRESULT hResult = S_OK;
        DWORD dwBytesEnumMemory = 0;

        if( m_State != State_Idle )
        {
            return E_UNEXPECTED;
        }

        // Get the buffer size
        DWORD dwResult = XUserReadStats(
            0,
            m_dwNumFriends, m_FriendsXuids,
            1, &m_StatsSpec,
            &dwBytesEnumMemory, NULL,
            NULL
            );

        if( dwResult != ERROR_INSUFFICIENT_BUFFER )
        {
            hResult = HRESULT_FROM_WIN32( dwResult );
        }

        if( SUCCEEDED( hResult ) )
        {
            // Allocate the buffer
            hResult = AllocEnumBuffer( dwBytesEnumMemory );
        }

        if( SUCCEEDED( hResult ) )
        {
            // Start the enumeration
            ZeroMemory( &m_Overlapped, sizeof( m_Overlapped ) );
            dwResult = XUserReadStats(
                0,
                m_dwNumFriends, m_FriendsXuids,
                1, &m_StatsSpec,
                &dwBytesEnumMemory,
                ( PXUSER_STATS_READ_RESULTS )m_pEnumMemory,
                &m_Overlapped
                );

            if( dwResult != ERROR_IO_PENDING )
            {
                hResult = HRESULT_FROM_WIN32( dwResult );
            }
        }

        if( SUCCEEDED( hResult ) )
        {
            m_State = State_StatsEnumByFriend;
        }

        return hResult;
    }

    //--------------------------------------------------------------------------------------
    // Enumerate stats by rank
    //--------------------------------------------------------------------------------------
    HRESULT         StatsEnumByRank( DWORD dwRankStart )
    {
        HRESULT hResult = S_OK;
        DWORD dwBytesEnumMemory = 0;

        if( m_State != State_Idle )
        {
            return E_UNEXPECTED;
        }

        // Create the enumerator
        DWORD dwResult = XUserCreateStatsEnumeratorByRank(
            0, dwRankStart, dwMaxItemsPerBlock, 1, &m_StatsSpec, &dwBytesEnumMemory, &m_hEnum
            );

        if( dwResult != ERROR_SUCCESS )
        {
            hResult = HRESULT_FROM_WIN32( dwResult );
        }

        if( SUCCEEDED( hResult ) )
        {
            // Allocate the buffer
            hResult = AllocEnumBuffer( dwBytesEnumMemory );
        }

        if( SUCCEEDED( hResult ) )
        {
            // Start the enumeration
            ZeroMemory( &m_Overlapped, sizeof( m_Overlapped ) );
            dwResult = XEnumerate(
                m_hEnum, m_pEnumMemory, m_dwBytesEnumMemory, NULL, &m_Overlapped
                );

            if( dwResult != ERROR_IO_PENDING )
            {
                hResult = HRESULT_FROM_WIN32( dwResult );
            }
        }

        if( SUCCEEDED( hResult ) )
        {
            m_State = State_StatsEnumByRank;
        }
        else
        {
            if( m_hEnum != NULL )
            {
                CloseHandle( m_hEnum );
                m_hEnum = NULL;
            }
        }

        return hResult;
    }

    //--------------------------------------------------------------------------------------
    // Enumerate stats by xuid
    //--------------------------------------------------------------------------------------
    HRESULT         StatsEnumByXuid( XUID xuid )
    {
        HRESULT hResult = S_OK;
        DWORD dwBytesEnumMemory = 0;

        if( m_State != State_Idle )
        {
            return E_UNEXPECTED;
        }

        // Create the enumerator
        DWORD dwResult = XUserCreateStatsEnumeratorByXuid(
            0, xuid, dwMaxItemsPerBlock, 1, &m_StatsSpec, &dwBytesEnumMemory, &m_hEnum
            );

        if( dwResult != ERROR_SUCCESS )
        {
            hResult = HRESULT_FROM_WIN32( dwResult );
        }

        if( SUCCEEDED( hResult ) )
        {
            // Allocate the buffer
            hResult = AllocEnumBuffer( dwBytesEnumMemory );
        }

        if( SUCCEEDED( hResult ) )
        {
            // Start the enumeration
            ZeroMemory( &m_Overlapped, sizeof( m_Overlapped ) );
            dwResult = XEnumerate(
                m_hEnum, m_pEnumMemory, m_dwBytesEnumMemory, NULL, &m_Overlapped
                );

            if( dwResult != ERROR_IO_PENDING )
            {
                hResult = HRESULT_FROM_WIN32( dwResult );
            }
        }

        if( SUCCEEDED( hResult ) )
        {
            m_State = State_StatsEnumByXuid;
        }
        else
        {
            if( m_hEnum != NULL )
            {
                CloseHandle( m_hEnum );
                m_hEnum = NULL;
            }
        }

        return hResult;
    }

    //--------------------------------------------------------------------------------------
    // Handle a friends enumeration
    //--------------------------------------------------------------------------------------
    VOID            DoWork_FriendsEnum()
    {
        DWORD dwCount = 0;
        DWORD dwResult = XGetOverlappedResult(
            &m_Overlapped,
            &dwCount,
            TRUE
            );

        // Get a list of friends XUIDs that includes ourselves and
        // exludes friend invitations that are not accepted yet
        if( ( dwResult == ERROR_SUCCESS ) ||
            ( dwResult == ERROR_NO_MORE_FILES ) ||
            ( dwResult == ERROR_FUNCTION_FAILED ) )
        {
            // Note: the XDK says that ERROR_NO_MORE_FILES means that you
            // have no friends. In practice it seems to return
            // ERROR_FUNCTION_FAILED in this case.
            m_bFriendsValid = TRUE;
            m_dwNumFriends = 0;
            m_FriendsXuids[ m_dwNumFriends++] = m_Xuid;
        }

        if( dwResult == ERROR_SUCCESS )
        {
            PXONLINE_FRIEND pFriendsEnum = ( PXONLINE_FRIEND )m_pEnumMemory;
            for( DWORD dw = 0; dw < dwCount; ++dw )
            {
                if( ( ( pFriendsEnum[ dw ].dwFriendState & XONLINE_FRIENDSTATE_FLAG_RECEIVEDREQUEST ) == 0 ) &&
                    ( ( pFriendsEnum[ dw ].dwFriendState & XONLINE_FRIENDSTATE_FLAG_SENTREQUEST ) == 0 ) )
                {
                    m_FriendsXuids[ m_dwNumFriends++] = pFriendsEnum[ dw ].xuid;
                }
            }
        }

        CloseHandle( m_hEnum );
        m_hEnum = NULL;
        m_State = State_Idle;

        if( ( m_dwNumFriends > 0 ) && m_bFriendsValid )
        {
            StatsEnumByFriend();
        }
        else
        {
            m_bRefreshing = FALSE;
        }
    }

    //--------------------------------------------------------------------------------------
    // Handle stats enumeration for friends
    //--------------------------------------------------------------------------------------
    VOID            DoWork_StatsEnumByFriend()
    {
        DWORD dwCount = 0;
        DWORD dwResult = XGetOverlappedResult(
            &m_Overlapped,
            &dwCount,
            TRUE
            );

        if( dwResult == ERROR_SUCCESS )
        {
            PXUSER_STATS_READ_RESULTS pStatsEnum = ( PXUSER_STATS_READ_RESULTS )m_pEnumMemory;

            if( pStatsEnum->dwNumViews > 0 )
            {
                PXUSER_STATS_VIEW pView = pStatsEnum->pViews;
                m_dwTotalItems = 0;
                m_dwDefaultItem = 0;

                if( pView->dwNumRows > 0 )
                {
                    // Sort the friends by rank
                    //qsort( pView->pRows, pView->dwNumRows, sizeof( XUSER_STATS_ROW ), CompareRows );
                    std::sort( pView->pRows, &pView->pRows[ pView->dwNumRows ], &CompareRows );

                    // Fill in item blocks with the new rows
                    ItemBlock* pBlock = NULL;
                    for( DWORD dwRow = 0; dwRow < pView->dwNumRows; ++dwRow )
                    {
                        PXUSER_STATS_ROW pRow = &pView->pRows[ dwRow ];
                        if( pRow->dwRank != 0 )
                        {
                            if( ( pBlock == NULL ) ||
                                ( pBlock->dwCount == dwMaxItemsPerBlock ) )
                            {
                                pBlock = GetAvailableBlock();
                                pBlock->bValid = TRUE;
                                pBlock->dwTick = m_dwTick;
                                pBlock->dwStart = m_dwTotalItems;
                                pBlock->dwCount = 0;
                            }

                            if( pRow->xuid == m_Xuid )
                            {
                                m_dwDefaultItem = m_dwTotalItems;
                            }
                            ++m_dwTotalItems;

                            DWORD dwBlkCount = pBlock->dwCount++;
                            FillItemFromRow( &pBlock->items[ dwBlkCount ], pRow );
                        }
                    }
                }
            }
        }

        m_bRefreshing = FALSE;
        m_State = State_Idle;
    }

    //--------------------------------------------------------------------------------------
    // Handle stats enumeration for rank or xuid
    //--------------------------------------------------------------------------------------
    VOID            DoWork_StatsEnumByRankOrXuid()
    {
        DWORD dwCount = 0;
        DWORD dwResult = XGetOverlappedResult(
            &m_Overlapped,
            &dwCount,
            TRUE
            );

        if( dwResult == ERROR_SUCCESS )
        {
            PXUSER_STATS_READ_RESULTS pStatsEnum = ( PXUSER_STATS_READ_RESULTS )m_pEnumMemory;

            if( pStatsEnum->dwNumViews > 0 )
            {
                PXUSER_STATS_VIEW pView = pStatsEnum->pViews;

                if( m_bRefreshing )
                {
                    // Set the total number of items
                    m_dwTotalItems = 0;
                    if( pView->dwNumRows != 0 )
                    {
                        m_dwTotalItems = pView->dwTotalViewRows;
                    }

                    // Set the default item
                    m_dwDefaultItem = 0;
                    if( m_State == State_StatsEnumByXuid )
                    {
                        for( DWORD dwRow = 0; dwRow < pView->dwNumRows; ++dwRow )
                        {
                            if( pView->pRows[ dwRow ].xuid == m_Xuid )
                            {
                                m_dwDefaultItem = pView->pRows[ 0 ].dwRank + dwRow - 1;
                            }
                        }
                    }
                }

                if( pView->dwNumRows > 0 )
                {
                    // Fill in a block with the new rows
                    ItemBlock* pBlock = GetAvailableBlock();
                    pBlock->bValid = TRUE;
                    pBlock->dwTick = m_dwTick;
                    pBlock->dwStart = pView->pRows[ 0 ].dwRank - 1;
                    pBlock->dwCount = pView->dwNumRows;

                    for( DWORD dwRow = 0; dwRow < pView->dwNumRows; ++dwRow )
                    {
                        FillItemFromRow( &pBlock->items[ dwRow ], &pView->pRows[ dwRow ] );
                    }
                }
            }
        }

        CloseHandle( m_hEnum );
        m_hEnum = NULL;
        m_bRefreshing = FALSE;
        m_State = State_Idle;
    }

    //--------------------------------------------------------------------------------------
    // Compare two stats rows for sorting
    //--------------------------------------------------------------------------------------
    static BOOL     CompareRows( const XUSER_STATS_ROW& a, const XUSER_STATS_ROW& b )
    {
        // Special case out unranked players
        if( a.dwRank == 0 && b.dwRank == 0 ) return FALSE; // If neither has played, they're equal
        else if( a.dwRank == 0 ) return FALSE;// If A hasn't played, return B first
        else if( b.dwRank == 0 ) return TRUE; // If B hasn't played, return A first

        // If A's rank is lower ( a better score ), this will be negative and A will be first in the list
        return ( a.dwRank < b.dwRank );
    }

protected:
    ItemBlock       m_Blocks[ dwMaxBlocks ];
    DWORD m_dwDefaultItem;
    DWORD m_dwTotalItems;

    State m_State;

    XUID m_Xuid;
    DWORD m_dwUserIndex;

    XOVERLAPPED m_Overlapped;
    HANDLE m_hEnum;
    BYTE* m_pEnumMemory;
    DWORD m_dwBytesEnumMemory;

    XUSER_STATS_SPEC m_StatsSpec;

    XUID            m_FriendsXuids[ MAX_FRIENDS + 1 ];
    DWORD m_dwNumFriends;
    BOOL m_bFriendsValid;

    DWORD m_dwTick;
    BOOL m_bRefreshing;
};

#endif // ARCADESAMPLE_XALEADERBOARDS_H

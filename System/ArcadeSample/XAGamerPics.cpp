//--------------------------------------------------------------------------------------
// XAGamerPics.cpp
//
// GamerPics helper class
//
// Xbox Advanced Technology Group.
// Copyright ( C ) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "stdafx.h"
#include "XAGamerPics.h"


//--------------------------------------------------------------------------------------
// Constructor
//--------------------------------------------------------------------------------------
CXAGamerPicsBase::CXAGamerPicsBase() : m_State( State_Idle ),
                                       m_pGamerPics( NULL ),
                                       m_dwNumGamerPics( 0 ),
                                       m_dwUserIndex( XUSER_INDEX_NONE ),
                                       m_Xuid( 0 ),
                                       m_dwKeyID( XPROFILE_GAMERCARD_PICTURE_KEY ),
                                       m_pbKey( NULL ),
                                       m_cbKey( 0 ),
                                       m_pTexture( NULL ),
                                       m_pBlankTexture( NULL ),
                                       m_pDevice( NULL )
{
    ZeroMemory( &m_Overlapped, sizeof( m_Overlapped ) );
}

//--------------------------------------------------------------------------------------
// Initialize with a GamerPics memory buffer
//   NOTE: Uninitialize() must be called to free resources installed in GamerPics buffer
//         before CXAGamerPicsBase destruction
//--------------------------------------------------------------------------------------
HRESULT CXAGamerPicsBase::Initialize( GamerPic* pGamerPics, DWORD dwNumGamerPics )
{
    HRESULT hResult = ERROR_ALREADY_INITIALIZED;

    if( m_pGamerPics == NULL )
    {
        m_pGamerPics = pGamerPics;
        m_dwNumGamerPics = dwNumGamerPics;

        hResult = S_OK;
    }

    return hResult;
}

//--------------------------------------------------------------------------------------
// Uninitialize() GamerPics memory buffer
//   NOTE: Uninitialize() must be called to free resources installed in GamerPics buffer
//         before CXAGamerPicsBase destruction
//--------------------------------------------------------------------------------------
VOID CXAGamerPicsBase::Uninitialize()
{
    this->Reset();

    m_pGamerPics = NULL;
    m_dwNumGamerPics = 0;
}

//--------------------------------------------------------------------------------------
// Cancel any pending operations and clear data.
//--------------------------------------------------------------------------------------
VOID CXAGamerPicsBase::Reset()
{
    if( m_State != State_Idle )
    {
        // Cancel overlapped IO
        XCancelOverlapped( &m_Overlapped );
        m_State = State_Idle;
    }

    for( DWORD dwPic = 0; dwPic < m_dwNumGamerPics; ++dwPic )
    {
        // Free any gamerpics
        if( m_pGamerPics[ dwPic ].bValid )
        {
            m_pGamerPics[ dwPic ].bValid = FALSE;

            if( m_pGamerPics[ dwPic ].pTexture != NULL )
            {
                m_pGamerPics[ dwPic ].pTexture->Release();
                m_pGamerPics[ dwPic ].pTexture = NULL;
            }

            if( m_pGamerPics[ dwPic ].hBrush != NULL )
            {
                XuiDestroyBrush( m_pGamerPics[ dwPic ].hBrush );
                m_pGamerPics[ dwPic ].hBrush = NULL;
            }
        }
    }

    // Free the key buffer
    free( m_pbKey );
    m_pbKey = NULL;
    m_cbKey = 0;

    // Free the textures
    if( m_pTexture != NULL )
    {
        m_pTexture->Release();
        m_pTexture = NULL;
    }

    if( m_pBlankTexture != NULL )
    {
        m_pBlankTexture->Release();
        m_pBlankTexture = NULL;
    }
}

//--------------------------------------------------------------------------------------
// Mark the specified user as invalid. It will be reloaded when next requested.
//--------------------------------------------------------------------------------------
VOID CXAGamerPicsBase::Invalidate( XUID xuid )
{
    for( DWORD dwPic = 0; dwPic < m_dwNumGamerPics; ++dwPic )
    {
        if( ( m_pGamerPics[ dwPic ].bValid ) &&
            ( m_pGamerPics[ dwPic ].xuid == xuid ) )
        {
            m_pGamerPics[ dwPic ].bDirty = TRUE;
            break;
        }
    }
}

//--------------------------------------------------------------------------------------
// Do work. Call this every frame.
//--------------------------------------------------------------------------------------
VOID CXAGamerPicsBase::DoWork()
{
    if( m_State != State_Idle )
    {
        if( XHasOverlappedIoCompleted( &m_Overlapped ) )
        {
            DWORD dwResult = XGetOverlappedResult( &m_Overlapped, NULL, TRUE );

            if( m_State == State_ReadGamerPic )
            {
                // Don't forget to unlock the texture
                m_pTexture->UnlockRect( 0 );
            }

            if( dwResult == ERROR_SUCCESS )
            {
                if( m_State == State_ReadKey )
                {
                    // We have the key, now read the gamerpic
                    ReadGamerPic( FALSE );
                }
                else if( m_State == State_ReadGamerPic )
                {
                    // We successfully read the gamerpic, now put
                    // it in the cache.
                    GamerPic* pGamerPic = FindAvailableGamerPic( m_Xuid );
                    if( pGamerPic != NULL )
                    {
                        pGamerPic->bValid = TRUE;
                        pGamerPic->bDirty = FALSE;
                        pGamerPic->dwTick = GetTickCount();
                        pGamerPic->xuid = m_Xuid;
                        pGamerPic->pTexture = m_pTexture;
                        m_pTexture = NULL;

                        XuiAttachTextureBrush( pGamerPic->pTexture, &pGamerPic->hBrush );
                    }

                    m_State = State_Idle;
                }
            }
            else
            {
                // Failure, put a blank gamerpic in the cache.
                GamerPic* pGamerPic = FindAvailableGamerPic( m_Xuid );
                if( pGamerPic != NULL )
                {
                    pGamerPic->bValid = TRUE;
                    pGamerPic->bDirty = FALSE;
                    pGamerPic->dwTick = GetTickCount();
                    pGamerPic->xuid = m_Xuid;
                    pGamerPic->pTexture = NULL;

                    if( m_pTexture != NULL )
                    {
                        m_pTexture->Release();
                        m_pTexture = NULL;
                    }

                    XuiAttachTextureBrush( GetBlankTexture(), &pGamerPic->hBrush );
                }

                m_State = State_Idle;
            }
        }
    }
}

//--------------------------------------------------------------------------------------
// Refresh the gamerpic cache.
// Call this when the active user changes.
//--------------------------------------------------------------------------------------
HRESULT CXAGamerPicsBase::Refresh( DWORD dwUserIndex, IDirect3DDevice9* pDevice )
{
    if( dwUserIndex != m_dwUserIndex )
    {
        Reset();
    }

    m_dwUserIndex = dwUserIndex;
    m_pDevice = pDevice;

    return S_OK;
}

//--------------------------------------------------------------------------------------
// Get the blank gamerpic texture
//--------------------------------------------------------------------------------------
IDirect3DTexture9* CXAGamerPicsBase::GetBlankTexture()
{
    if( m_pBlankTexture == NULL )
    {
        // Create the blank texture
        HRESULT hResult = m_pDevice->CreateTexture( 1, 1, 1, 0, D3DFMT_LIN_A8R8G8B8, 0, &m_pBlankTexture, NULL );
        if( SUCCEEDED( hResult ) )
        {
            D3DLOCKED_RECT rect = {0};
            m_pBlankTexture->LockRect( 0, &rect, NULL, 0 );
            *( ( DWORD* )rect.pBits ) = 0x00000000;
            m_pBlankTexture->UnlockRect( 0 );
        }
    }

    return m_pBlankTexture;
}

//--------------------------------------------------------------------------------------
// Get the texture for a gamerpic.
//--------------------------------------------------------------------------------------
IDirect3DTexture9* CXAGamerPicsBase::GetGamerPicTexture( XUID xuid )
{
    GamerPic* pGamerPic = FindGamerPic( xuid );
    if( ( pGamerPic != NULL ) && ( pGamerPic->pTexture != NULL ) )
    {
        // Return the gamerpic texture
        return pGamerPic->pTexture;
    }

    // Return the blank texture
    return GetBlankTexture();
}

//--------------------------------------------------------------------------------------
// Get the XUI brush for a gamerpic.
//--------------------------------------------------------------------------------------
HXUIBRUSH CXAGamerPicsBase::GetGamerPicBrush( XUID xuid )
{
    GamerPic* pGamerPic = FindGamerPic( xuid );
    if( pGamerPic != NULL )
    {
        // Return the gamerpic brush
        return pGamerPic->hBrush;
    }

    return NULL;
}

//--------------------------------------------------------------------------------------
// Find the gamerpic for the specified xuid, or return null.
//--------------------------------------------------------------------------------------
CXAGamerPicsBase::GamerPic* CXAGamerPicsBase::FindGamerPic( XUID xuid )
{
    GamerPic* pGamerPic = NULL;

    // Try to find the gamerpic
    for( DWORD dwPic = 0; dwPic < m_dwNumGamerPics; ++dwPic )
    {
        if( ( m_pGamerPics[ dwPic ].bValid ) &&
            ( m_pGamerPics[ dwPic ].xuid == xuid ) )
        {
            pGamerPic = &m_pGamerPics[ dwPic ];
            pGamerPic->dwTick = GetTickCount();
            break;
        }
    }

    // Try to read the gamerpic
    if( ( m_State == State_Idle ) &&
        ( ( pGamerPic == NULL ) || ( pGamerPic->bDirty ) ) )
    {
        m_Xuid = xuid;

        XUID localXuid = 0;
        XUserGetXUID( m_dwUserIndex, &localXuid );
        if( localXuid == m_Xuid )
        {
            // Read the gamerpic locally
            ReadGamerPic( TRUE );
        }
        else
        {
            // Read the gamerpic key for a remote gamerpic
            ReadKey();
        }
    }

    return pGamerPic;
}

//--------------------------------------------------------------------------------------
// Find an available gamerpic. Find the LRU if the cache is full.
//--------------------------------------------------------------------------------------
CXAGamerPicsBase::GamerPic* CXAGamerPicsBase::FindAvailableGamerPic( XUID xuid )
{
    GamerPic* pGamerPic = NULL;

    // Find if we already have a cache slot for this xuid
    for( DWORD dwPic = 0; dwPic < m_dwNumGamerPics; ++dwPic )
    {
        if( ( m_pGamerPics[ dwPic ].bValid ) &&
            ( m_pGamerPics[ dwPic ].xuid == xuid ) )
        {
            pGamerPic = &m_pGamerPics[ dwPic ];
            break;
        }
    }

    if( pGamerPic == NULL )
    {
        // Find the least recently used cache slot
        DWORD dwBest = 0;
        DWORD dwBestTick = GetTickCount();

        for( DWORD dwPic = 0; dwPic < m_dwNumGamerPics; ++dwPic )
        {
            if( !m_pGamerPics[ dwPic ].bValid )
            {
                dwBest = dwPic;
                break;
            }

            if( m_pGamerPics[ dwPic ].dwTick < dwBestTick )
            {
                dwBest = dwPic;
                dwBestTick = m_pGamerPics[ dwPic ].dwTick;
            }
        }

        pGamerPic = &m_pGamerPics[ dwBest ];
    }

    if( pGamerPic != NULL )
    {
        pGamerPic->bValid = FALSE;
        pGamerPic->bDirty = FALSE;
        pGamerPic->dwTick = 0;
        pGamerPic->xuid = 0;

        if( pGamerPic->pTexture != NULL )
        {
            pGamerPic->pTexture->Release();
            pGamerPic->pTexture = NULL;
        }

        if( pGamerPic->hBrush != NULL )
        {
            XuiDestroyBrush( pGamerPic->hBrush );
            pGamerPic->hBrush = NULL;
        }
    }

    return pGamerPic;
}

//--------------------------------------------------------------------------------------
// Start reading the gamerpic key from the profile.
//--------------------------------------------------------------------------------------
HRESULT CXAGamerPicsBase::ReadKey()
{
    if( m_State != State_Idle )
    {
        return E_UNEXPECTED;
    }

    HRESULT hResult = S_OK;
    DWORD cbKey = 0;

    // Determine the required size of the key buffer
    DWORD dwResult = XUserReadProfileSettingsByXuid(
        0, m_dwUserIndex,
        1, &m_Xuid,
        1, &m_dwKeyID,
        &cbKey,
        NULL,
        NULL
        );

    if( dwResult != ERROR_INSUFFICIENT_BUFFER )
    {
        hResult = HRESULT_FROM_WIN32( dwResult );
    }

    // Allocate the key buffer if needed
    if( ( SUCCEEDED( hResult ) ) && ( cbKey > m_cbKey ) )
    {
        free( m_pbKey );
        m_cbKey = 0;

        m_pbKey = ( BYTE* )malloc( cbKey );
        if( m_pbKey == NULL )
        {
            hResult = E_OUTOFMEMORY;
        }
        else
        {
            m_cbKey = cbKey;
        }
    }

    if( SUCCEEDED( hResult ) )
    {
        // Read the gamerpic key
        ZeroMemory( &m_Overlapped, sizeof( m_Overlapped ) );
        dwResult = XUserReadProfileSettingsByXuid(
            0, m_dwUserIndex,
            1, &m_Xuid,
            1, &m_dwKeyID,
            &m_cbKey,
            ( PXUSER_READ_PROFILE_SETTING_RESULT )m_pbKey,
            &m_Overlapped
            );

        if( dwResult != ERROR_IO_PENDING )
        {
            hResult = HRESULT_FROM_WIN32( dwResult );
        }
    }

    m_State = SUCCEEDED( hResult ) ? State_ReadKey : State_Idle;

    return hResult;
}

//--------------------------------------------------------------------------------------
// Start reading the gamerpic.
//--------------------------------------------------------------------------------------
HRESULT CXAGamerPicsBase::ReadGamerPic( BOOL local )
{
    if( ( !local && ( m_State != State_ReadKey ) ) ||
        ( local && ( m_State != State_Idle ) ) )
    {
        return E_UNEXPECTED;
    }

    HRESULT hResult = S_OK;
    DWORD dwResult = ERROR_SUCCESS;

    // Allocate the texture
    hResult = m_pDevice->CreateTexture( 64, 64, 1, 0, D3DFMT_LIN_A8R8G8B8, 0, &m_pTexture, NULL );
    if( SUCCEEDED( hResult ) )
    {
        D3DLOCKED_RECT rect = {0};
        m_pTexture->LockRect( 0, &rect, NULL, 0 );
        ZeroMemory( rect.pBits, rect.Pitch * 64 );
        ZeroMemory( &m_Overlapped, sizeof( m_Overlapped ) );

        if( local )
        {
            // Read the gamerpic texture locally
            dwResult = XUserReadGamerPicture(
                m_dwUserIndex,
                FALSE,
                ( BYTE* )rect.pBits,
                rect.Pitch,
                64,
                &m_Overlapped
                );
        }
        else
        {
            // Read the gamerpic texture remotely
            dwResult = XUserReadGamerPictureByKey(
                &( ( PXUSER_READ_PROFILE_SETTING_RESULT )m_pbKey )->pSettings->data,
                FALSE,
                ( BYTE* )rect.pBits,
                rect.Pitch,
                64,
                &m_Overlapped
                );
        }

        if( dwResult != ERROR_IO_PENDING )
        {
            m_pTexture->UnlockRect( 0 );
            hResult = HRESULT_FROM_WIN32( dwResult );
        }
    }

    m_State = SUCCEEDED( hResult ) ? State_ReadGamerPic : State_Idle;

    return hResult;
}


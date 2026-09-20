//--------------------------------------------------------------------------------------
// XAGamerPics.h
//
// GamerPics helper class
//
// Xbox Advanced Technology Group.
// Copyright ( C ) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#pragma once
#ifndef ARCADESAMPLE_XAGAMERPICS_H
#define ARCADESAMPLE_XAGAMERPICS_H



//--------------------------------------------------------------------------------------
// GamerPics helper base class
//--------------------------------------------------------------------------------------
class CXAGamerPicsBase
{
public:
    // GamerPic
    struct GamerPic
    {
        BOOL bValid;
        BOOL bDirty;
        DWORD dwTick;
        XUID xuid;
        IDirect3DTexture9* pTexture;
        HXUIBRUSH hBrush;
    };

    // Constructor
                CXAGamerPicsBase();

    // Destructor
    virtual     ~CXAGamerPicsBase()
    {
    }

    // Initialize with a GamerPics memory buffer
    //   NOTE: Uninitialize() must be called to free resources installed in GamerPics buffer
    //         before CXAGamerPicsBase destruction
    HRESULT     Initialize( GamerPic* pGamerPics, DWORD dwNumGamerPics );

    // Uninitialize() GamerPics memory buffer
    //   NOTE: Uninitialize() must be called to free resources installed in GamerPics buffer
    //         before CXAGamerPicsBase destruction
    VOID        Uninitialize();

    // Cancel any pending operations and clear data.
    VOID        Reset();

    // Mark the specified user as invalid. It will be reloaded when next requested.
    VOID        Invalidate( XUID xuid );

    // Do work. Call this every frame.
    VOID        DoWork();

    // Refresh the gamerpic cache.
    // Call this when the active user changes.
    HRESULT     Refresh( DWORD dwUserIndex, IDirect3DDevice9* pDevice );

    // Get the blank gamerpic texture
    IDirect3DTexture9* GetBlankTexture();

    // Get the texture for a gamerpic.
    IDirect3DTexture9* GetGamerPicTexture( XUID xuid );

    // Get the XUI brush for a gamerpic.
    HXUIBRUSH   GetGamerPicBrush( XUID xuid );

protected:
    // Find the gamerpic for the specified xuid, or return null.
    GamerPic* FindGamerPic( XUID xuid );

    // Find an available gamerpic. Find the LRU if the cache is full.
    GamerPic* FindAvailableGamerPic( XUID xuid );

    // Start reading the gamerpic key from the profile.
    HRESULT     ReadKey();

    // Start reading the gamerpic.
    HRESULT     ReadGamerPic( BOOL bLocal );

protected:
    enum State
    {
        State_Idle,
        State_ReadKey,
        State_ReadGamerPic
    };

    State m_State;

    GamerPic* m_pGamerPics;
    DWORD m_dwNumGamerPics;

    XOVERLAPPED m_Overlapped;
    DWORD m_dwUserIndex;
    XUID m_Xuid;
    DWORD m_dwKeyID;
    BYTE* m_pbKey;
    DWORD m_cbKey;
    IDirect3DTexture9* m_pTexture;
    IDirect3DTexture9* m_pBlankTexture;
    IDirect3DDevice9* m_pDevice;
};


//--------------------------------------------------------------------------------------
// Template CXAGamerPics helper Class
//--------------------------------------------------------------------------------------
template <DWORD dwNumGamerPics = 256> class CXAGamerPics : public CXAGamerPicsBase
{
public:
    // Number of gamer pics in this template instance
    static const DWORD NUM_GAMERPICS = dwNumGamerPics;

    // Constructor
                CXAGamerPics() : CXAGamerPicsBase()
                {
                    ZeroMemory( m_GamerPics, sizeof( m_GamerPics ) );
                    this->Initialize( m_GamerPics, NUM_GAMERPICS );
                }

    // Destructor
                ~CXAGamerPics()
                {
                    this->Uninitialize();
                }

protected:
    GamerPic    m_GamerPics[ NUM_GAMERPICS ];
};

#endif // ARCADESAMPLE_XAGAMERPICS_H

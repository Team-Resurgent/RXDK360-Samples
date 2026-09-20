//--------------------------------------------------------------------------------------
// AvatarRenderer.cpp
//
// This sample demonstrates the use of the IXAvatarRenderer and IXAvatarAnimation 
// interfaces to render and animate Avatars.
//
// XNA Developer Connection
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include <vector>
#include <xtl.h>            
#include <xavatar.h>

#include <AtgApp.h>
#include <AtgFont.h>
#include <AtgHelp.h>
#include <AtgInput.h>
#include <AtgSignIn.h>
#include <AtgUtil.h>

//--------------------------------------------------------------------------------------
// Callouts for labeling the gamepad on the help screen
//--------------------------------------------------------------------------------------
ATG::HELP_CALLOUT g_HelpCallouts[] =
{
    { ATG::HELP_BACK_BUTTON,    ATG::HELP_PLACEMENT_1, L"Display help" },
    { ATG::HELP_LEFT_STICK,     ATG::HELP_PLACEMENT_1, L"Move Camera" },
    { ATG::HELP_RIGHT_STICK,    ATG::HELP_PLACEMENT_1, L"Rotate Camera" },
    { ATG::HELP_A_BUTTON,       ATG::HELP_PLACEMENT_1, L"Clap" },
    { ATG::HELP_B_BUTTON,       ATG::HELP_PLACEMENT_1, L"Wave" },
    { ATG::HELP_X_BUTTON,       ATG::HELP_PLACEMENT_1, L"Celebrate" },
    { ATG::HELP_Y_BUTTON,       ATG::HELP_PLACEMENT_1, L"Stand" },
    { ATG::HELP_RIGHT_SHOULDER, ATG::HELP_PLACEMENT_1, L"Load Custom Asset" },
};

static const DWORD NUM_HELP_CALLOUTS = ARRAYSIZE( g_HelpCallouts );
static const DWORD NUM_ANIMATIONS       = 4;
static const DWORD SAMPLE_ALLOCATOR_ID  = 0;
static const DWORD MAX_AVATAR_FRIENDS   = 10;

static const DWORD AVATAR_ASSET_BUFFER_ATTRIBUTES = MAKE_XALLOC_ATTRIBUTES( 0,
                                                                            TRUE,
                                                                            TRUE,
                                                                            FALSE,
                                                                            SAMPLE_ALLOCATOR_ID,
                                                                            XALLOC_ALIGNMENT_16,
                                                                            XALLOC_MEMPROTECT_READWRITE,
                                                                            FALSE,
                                                                            XALLOC_MEMTYPE_HEAP);

static const DWORD g_dwTileWidth   = 1280;
static const DWORD g_dwTileHeight  = 256;
static const DWORD g_dwFrameWidth  = 1280;
static const DWORD g_dwFrameHeight = 720;
static const D3DRECT g_tiles[3] = 
{
    {             0,              0,  g_dwTileWidth,  g_dwTileHeight },
    {             0, g_dwTileHeight,  g_dwTileWidth, g_dwTileHeight * 2 },
    {             0, g_dwTileHeight * 2,  g_dwTileWidth, g_dwFrameHeight },
};
                                   

//--------------------------------------------------------------------------------------
// Name: class Sample
// Desc: Main class to run this application.
//--------------------------------------------------------------------------------------
class Sample : public ATG::Application
{
public:
    Sample() : m_bDrawHelp( FALSE ), m_dwCurFrontBuffer( 0 ), 
               m_dwFriendsCount( 0 ), m_pSettingResults( NULL ), m_bFriendsPresenceDataReady( FALSE ),
               m_hFriendsEnum( INVALID_HANDLE_VALUE ), m_pAvatarAssetBuffer( NULL )
    {
    }

    virtual ~Sample()
    {
        FreeAvatarResources( TRUE );

        // Shutdown XAvatar and release memory
        XAvatarShutdown();
    }

private:
    // Avatar data representation for local user and all friends
    struct UserAvatarData
    {
        XUID                        m_xuid;
        XAVATAR_METADATA            m_metadata;
        static LPXAVATARANIMATION   s_pAnimations[ NUM_ANIMATIONS ]; // Reuse IXAvatarAnimation instances for all loaded Avatars
        static BOOL                 s_bAnimationsLoaded;
        LPXAVATARRENDERER           m_pRenderer;
        XAVATAR_WAITING_EFFECT      m_WaitingEffect;
        BOOL                        m_bRenderWaitingEffect;
        BOOL                        m_bWaitingEffectFadeOut;
        XMMATRIX                    m_matWorld;
    };

    virtual HRESULT Initialize();
    
    HRESULT         ReloadLocalUserAndFriendsAvatarData( const BOOL bIncludingFriends );
    HRESULT         LoadAvatar( UserAvatarData& avatarData );
    HRESULT         LoadLocalUserAvatarMetadata( UserAvatarData& avatarData );
    HRESULT         LoadAnimationFromFile( LPCSTR szAnimFilename, LPXAVATARANIMATION* ppAnim );
    HRESULT         LoadCustomAvatarAsset(UserAvatarData& avatarData);
    virtual HRESULT Update();
    VOID            InitializeFriendAvatarEnumerationData();
    VOID            CreateRenderTargets();
    VOID            FreeAvatarResources( const BOOL bAlsoFreeAnimations );
    virtual HRESULT Render();
    VOID            RenderOverlays();
    
  
private:
    ATG::Timer                  m_Timer;
    ATG::Font                   m_Font;
    ATG::Help                   m_Help;
    BOOL                        m_bDrawHelp;

    // View parameters
    FLOAT                       m_fLookPitch;
    FLOAT                       m_fLookYaw;
    XMVECTOR                    m_vEyePt;
    XMVECTOR                    m_vUp;
    XMMATRIX                    m_matView;
    XMMATRIX                    m_matProj; 
   
    // Friends
    XONLINE_FRIEND                     m_Friends[ MAX_AVATAR_FRIENDS ];
    XUID                               m_FriendsXuids[ MAX_AVATAR_FRIENDS ];
    DWORD                              m_dwFriendsCount;
    XAVATAR_METADATA                   m_FriendsAvatarMetadata[ MAX_AVATAR_FRIENDS ];
    XUSER_READ_PROFILE_SETTING_RESULT* m_pSettingResults;
    BOOL                               m_bFriendsPresenceDataReady;
    BOOL                               m_bFriendsAvatarMetadataReady;
    XOVERLAPPED                        m_Overlapped;
    HANDLE                             m_hFriendsEnum;
    BYTE*                              m_pAvatarAssetBuffer;

    std::vector<UserAvatarData*>  m_vAvatarData;

    typedef std::vector<UserAvatarData*>::const_iterator  AvatarDataCIter;
    typedef std::vector<UserAvatarData*>::iterator        AvatarDataIter;

    // Rendering surfaces and textures
    D3DSurface*                 m_pBackBuffer;
    D3DSurface*                 m_pDepthBuffer;
    D3DTexture*                 m_pFrontBuffer[2];
    DWORD                       m_dwCurFrontBuffer;

    HANDLE                      m_hNotification;            // System notification handle
};

BOOL Sample::UserAvatarData::s_bAnimationsLoaded = FALSE;
LPXAVATARANIMATION Sample::UserAvatarData::s_pAnimations[ NUM_ANIMATIONS ];

//-------------------------------------------------------------------------------------
// Name: main()
// Desc: The application's entry point
//-------------------------------------------------------------------------------------
void __cdecl main()
{
    Sample atgApp;
    ZeroMemory( &atgApp.m_d3dpp, sizeof( atgApp.m_d3dpp ) );

    atgApp.m_d3dpp.BackBufferWidth        = 1280;
    atgApp.m_d3dpp.BackBufferHeight       = 720;
    atgApp.m_d3dpp.BackBufferCount        = 1;
    atgApp.m_d3dpp.MultiSampleType        = D3DMULTISAMPLE_4_SAMPLES;
    atgApp.m_d3dpp.EnableAutoDepthStencil = FALSE;
    atgApp.m_d3dpp.DisableAutoBackBuffer  = TRUE;
    atgApp.m_d3dpp.DisableAutoFrontBuffer = TRUE;
    atgApp.m_d3dpp.SwapEffect             = D3DSWAPEFFECT_DISCARD;
    atgApp.m_d3dpp.PresentationInterval   = D3DPRESENT_INTERVAL_IMMEDIATE;

    atgApp.m_dwDeviceCreationFlags       |= D3DCREATE_BUFFER_2_FRAMES | D3DCREATE_CREATE_THREAD_ON_1;

    atgApp.Run();
}


//-------------------------------------------------------------------------------------
// Name:  RetrieveFriendsPresenceData
// Desc:  Standalone function to enumerate and retrieve friends' presence data using
//        the XFriendsCreateEnumerator function.
// Notes: When called synchronously (NULL pOverlapped), phFriendsEnum is ignored.
//        When called asynchronously (non-NULL pOverlapped), phFriendsEnum must
//        be non-NULL.
//-------------------------------------------------------------------------------------
HRESULT RetrieveFriendsPresenceData( const DWORD dwUserIndex, 
                                     XONLINE_FRIEND* pArrayFriends, 
                                     const UINT cFriendsArraySize, 
                                     DWORD* pcFriendsRetrieved,
                                     XOVERLAPPED* pOverlapped = NULL,
                                     HANDLE* phFriendsEnum = NULL )
{
    // Just check that pOverlapped and phFriendsEnum are consistent with each other. 
    // Leave all other parameter checking to the XBox 360 APIs that take those parameters
    if( pOverlapped && !phFriendsEnum )
    {
        ATG::FatalError( "If pOverlapped is non-NULL, phFriendsEnum must be non-NULL\n" );
    }

    //
    // Enumerate friends
    //
    HANDLE hFriendsEnum = INVALID_HANDLE_VALUE;
    DWORD cbBuffer = 0;
    DWORD dwErr = ERROR_SUCCESS;
    dwErr = XFriendsCreateEnumerator(
        dwUserIndex,                    // enumerate friends of this user
        0,                              // starting index
        MAX_AVATAR_FRIENDS,             // number of friends we're querying for
        &cbBuffer,                      // size of buffer needed
        &hFriendsEnum );

    // Friends' presence information not yet available
    if( dwErr != ERROR_SUCCESS  )
    {
        ATG::FatalError( "XEnumerate did not return ERROR_SUCCESS. Instead returned %d\n", dwErr );
    }

    //
    // Retrieve all friends' data in one call to XEnumerate.
    //

    // Zero overlapped memory before calling XUserReadProfileSettingsByXuid asynchronously
    // and return friends enumeration handle in *phFriendsEnum
    if( pOverlapped )
    {
        ZeroMemory( pOverlapped, sizeof( XOVERLAPPED ) );
        *phFriendsEnum = hFriendsEnum;
    }

    //
    // Call XEnumerate asynchronously or synchronously
    // depending on caller preference.
    // When XEnumerate is called asynchronously,
    // *pcFriendsRetrieved is zero. The number of friend
    // items returned can be retrieved from
    // the InternalHigh of the passed-in XOVERLAPPED structure 
    // when the asynchronous call has completed
    // Note that in the asynchronous case, resources held by
    // hFriendsEnum shouldn't be released until after
    // the overlapped operation has completed.
    //
    if( pcFriendsRetrieved )
    {
        *pcFriendsRetrieved = 0;
    }

    ZeroMemory( pArrayFriends, cFriendsArraySize * sizeof( pArrayFriends ) );

    dwErr = XEnumerate( hFriendsEnum, 
                        pArrayFriends, 
                        cbBuffer, 
                        pcFriendsRetrieved,
                        pOverlapped );

    if( pOverlapped && ( dwErr != ERROR_IO_PENDING ) )
    {
        ATG::FatalError( "XEnumerate did not return ERROR_IO_PENDING when called asynchronously. Instead returned %d\n", dwErr );
    }
    else if( !pOverlapped && dwErr != ERROR_SUCCESS )
    {
        ATG::FatalError( "XEnumerate did not return ERROR_SUCCESS when called synchronously. Instead returned %d\n", dwErr );
    }

    if( !pOverlapped )
    {
        // Ok to free resources held by hFriendsEnum
        XCloseHandle( hFriendsEnum );
    }

    return dwErr;
}

//-------------------------------------------------------------------------------------
// Name:  GetFriendsAvatarMetadataFromUserProfileSettings
// Desc:  Standalone function to extract friends' Avatar metadata from user profile
//        settings data retrieved previously from a call to XUserReadProfileSettings
//        or the RetrieveFriendsAvatarMetadata function defined in this .cpp file.
//-------------------------------------------------------------------------------------
VOID GetFriendsAvatarMetadataFromUserProfileSettings( XAVATAR_METADATA* pArrayAvatarMetadata,
                                                      const XUSER_READ_PROFILE_SETTING_RESULT* const pSettingResults )
{
    // This sample requires the player at controller 0 to be signed into LIVE.
    XUSER_SIGNIN_STATE signinState = XUserGetSigninState( 0 );
    if( signinState != eXUserSigninState_SignedInToLive )
    {
        return;
    }

    if( !pArrayAvatarMetadata || !pSettingResults )
    {
        ATG::FatalError( "Both pArrayAvatarMetadata and pSettingResults must be non-NULL\n" );
    }

    for( DWORD i = 0; i < pSettingResults->dwSettingsLen; ++i )
    {
        XUSER_PROFILE_SETTING& profileSettings = pSettingResults->pSettings[i];
        if( profileSettings.dwSettingId != XPROFILE_AVATAR_METADATA )
        {
            // Should never hit this since we only requested a single setting ID
            ATG::FatalError( "profileSettings.dwSettingId != XPROFILE_AVATAR_METADATA\n" );
        }

        if( profileSettings.data.type != XUSER_DATA_TYPE_BINARY )
        {
            ATG::FatalError( "profileSettings.data.type != XUSER_DATA_TYPE_BINARY\n" );
        }

        XMemCpy( &pArrayAvatarMetadata[i], profileSettings.data.binary.pbData, profileSettings.data.binary.cbData );
    }
}

//-------------------------------------------------------------------------------------
// Name:  RetrieveFriendsAvatarMetadata
// Desc:  Standalone function to retrieve friends' Avatar metadata using
//        the XUserReadProfileSettings function.
// Notes: When called synchronously (NULL pOverlapped), pArrayAvatarMetadata must
//        be non-NULL and ppSettingResults is ignored.
//        When called asynchronously (non-NULL pOverlapped), ppSettingResults must
//        be non-NULL and pArrayAvatarMetadata is ignored.
//-------------------------------------------------------------------------------------
HRESULT RetrieveFriendsAvatarMetadata( const DWORD dwUserIndex, 
                                       const XUID* const pArrayFriendsXuids, 
                                       const UINT cFriendsCount,
                                       XAVATAR_METADATA* pArrayAvatarMetadata = NULL,
                                       XUSER_READ_PROFILE_SETTING_RESULT** ppSettingResults = NULL,
                                       XOVERLAPPED* pOverlapped = NULL )
{
    if( !cFriendsCount )
    {
        return S_FALSE;
    }

    // Sanity check parameters
    if( pOverlapped && !ppSettingResults )
    {
        ATG::FatalError( "pOverlapped is non-NULL, so ppSettingResults must also be non-NULL\n" );
    }
    if( !pOverlapped && !pArrayAvatarMetadata )
    {
        ATG::FatalError( "pOverlapped is NULL, so pArrayAvatarMetadata must be non-NULL\n" );
    }
    if( !pArrayFriendsXuids )
    {
        ATG::FatalError( "pArrayFriendsXuids is incorrectly NULL\n" );
    }

    static const DWORD arSettingID[] = { XPROFILE_AVATAR_METADATA };

    // Determine the maximum read buffer size and allocate space for it
    // Determine buffer size by passing a zero settings size
    DWORD dwSettingSizeMax = 0;
    DWORD dwErr = ERROR_SUCCESS;
    dwErr = XUserReadProfileSettingsByXuid( 0,                  // A title in your family or 0 for the current title
                                            0,                  // User index of requesting user
                                            cFriendsCount,      // Count of XUIDs
                                            pArrayFriendsXuids, // Pointer to array of XUIDs to request settings for
                                            1,                  // Count of setting ids in pdwSettingIds
                                            arSettingID,        // Pointer to array of settings to retrieve
                                            &dwSettingSizeMax,  // Size of pResults buffer. Initially 0 to retrieve required buffer size
                                            NULL,               // Results buffer. Not used when retrieving buffer size
                                            NULL );             // Overlapped (not used)

    assert( dwErr == ERROR_INSUFFICIENT_BUFFER );
    assert( dwSettingSizeMax > 0 );

    // Allocate memory to hold profile settings data retrieved from LIVE
    BYTE* pData = new BYTE [ dwSettingSizeMax ];
    ZeroMemory( pData, dwSettingSizeMax );

    // Zero overlapped memory before calling XUserReadProfileSettingsByXuid asynchronously
    if( pOverlapped )
    {
        ZeroMemory( pOverlapped, sizeof( XOVERLAPPED ) );
    }

    dwErr = XUserReadProfileSettingsByXuid( 0,                  // A title in your family or 0 for the current title
                                            0,                  // User index of requesting user
                                            cFriendsCount,      // Count of XUIDs
                                            pArrayFriendsXuids, // Pointer to array of XUIDs to request settings for
                                            1,                  // Count of setting ids in pdwSettingIds
                                            arSettingID,        // Pointer to array of settings to retrieve
                                            &dwSettingSizeMax,  // Size of pResults buffer.  If *pcbResults is 0 then required size is returned.
                                            (XUSER_READ_PROFILE_SETTING_RESULT*)pData, // Results
                                            pOverlapped );      // Overlapped

    if( pOverlapped && ( dwErr != ERROR_IO_PENDING ) )
    {
        ATG::FatalError( "XUserReadProfileSettingsByXuid did not return ERROR_IO_PENDING when called asynchronously. Instead returned %d\n", dwErr );
    }
    else if( !pOverlapped && dwErr != ERROR_SUCCESS )
    {
        ATG::FatalError( "XUserReadProfileSettingsByXuid did not return ERROR_SUCCESS when called synchronously. Instead returned %d\n", dwErr );
    }

    // Always return the results data to the caller. It's the caller's responsibility to free
    // this data. When this function is called asynchronously, it's also the caller's
    // responsibility for retrieving the Avatar metadata from the results stored 
    // in pData and then freeing the memory
    *ppSettingResults = (XUSER_READ_PROFILE_SETTING_RESULT*)pData;

    // If we've been called synchronously, return Avatar metadata now
    if( !pOverlapped )
    {
        GetFriendsAvatarMetadataFromUserProfileSettings( pArrayAvatarMetadata, (XUSER_READ_PROFILE_SETTING_RESULT*)pData );
    }

    return S_OK;
}

//--------------------------------------------------------------------------------------
// Name: Initialize()
// Desc: Creates all graphics resources and initializes rendering and animation systems.
//--------------------------------------------------------------------------------------
HRESULT Sample::Initialize()
{
    // Start up Xbox Live functionality using default Secure Network Layer settings
    if( XOnlineStartup() != ERROR_SUCCESS )
    {
        ATG::FatalError( "Failed to start Xbox Live\n" );
    }

    // Initialize all data used to enumerate friends and retrieve friends' Avatar metadata
    // to sensible values
    InitializeFriendAvatarEnumerationData();

    // Initialize signin
    ATG::SignIn::Initialize( 1, 4, TRUE, 1 );

    // Initialize the Avatar library
    static const DWORD dwAssetLoadHardwareThread = 5;
    static const DWORD dwJointsBufferCount = 2;
    if( FAILED( XAvatarInitialize(      XAVATAR_COORDINATE_SYSTEM_RIGHT_HANDED, 
                                        XAVATAR_INITIALIZE_FLAGS_ENABLERENDERER, 
                                        dwAssetLoadHardwareThread,
                                        dwJointsBufferCount,
                                        m_pd3dDevice) ) )
    {
        ATG_PrintError( "Unable to load Avatar asset pack\n");
        return E_FAIL;
    }

    // Load Avatar instances for the local user and friends
    RETURN_ON_FAIL( ReloadLocalUserAndFriendsAvatarData( TRUE ) );

    // Now create the animation interfaces for the local user. We'll re-use these
    // interfaces for all Avatar instances
    ZeroMemory( &Sample::UserAvatarData::s_pAnimations[0], sizeof( Sample::UserAvatarData::s_pAnimations ) );

    XAvatarLoadAnimation( &XAVATAR_ANIMATION_GENERIC_CLAP, 0, &UserAvatarData::s_pAnimations[0] );
    XAvatarLoadAnimation( &XAVATAR_ANIMATION_GENERIC_WAVE, 0, &UserAvatarData::s_pAnimations[1] );
    LoadAnimationFromFile( "game:\\Media\\Anim\\anim_celebration.bin", &UserAvatarData::s_pAnimations[2] );
    LoadAnimationFromFile( "game:\\Media\\Anim\\anim_stand.bin", &UserAvatarData::s_pAnimations[3] );
    Sample::UserAvatarData::s_bAnimationsLoaded = TRUE;

    // Initialize view parameters
    XVIDEO_MODE VideoMode;
    XGetVideoMode( &VideoMode );
    FLOAT fAspectRatio = VideoMode.fIsWideScreen == TRUE ? (16.0f / 9.0f) : (4.0f / 3.0f);
    m_matProj    = XMMatrixPerspectiveFovRH( XM_PI/4, fAspectRatio, 0.01f, 20.0f );    
    m_vEyePt     = XMVectorSet( 0.0f, 1.0f, 2.0f, 0.0f );
    m_vUp        = XMVectorSet( 0.0f, 1.0f, 0.0f, 0.0f );
    m_fLookPitch = 0.0f;
    m_fLookYaw   = 0.0f;

    CreateRenderTargets();

    // Create the font
    if( FAILED( m_Font.Create( "d:\\Media\\Fonts\\Arial_16.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Confine text drawing to the title safe area
    m_Font.SetWindow( ATG::GetTitleSafeArea() );

    // Create the help
    if( FAILED( m_Help.Create( "d:\\Media\\Help\\Help.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Register the notification listener
    m_hNotification = XNotifyCreateListener( XNOTIFY_SYSTEM );
    if( m_hNotification == NULL || m_hNotification == INVALID_HANDLE_VALUE )
    { 
       return E_FAIL;
    }

    return S_OK;
}
//--------------------------------------------------------------------------------------
// Name: InitializeFriendAvatarEnumerationData()
// Desc: Initializes all data used to enumerate friends and retrieve friends' Avatar metadata
//       to sensible values
//--------------------------------------------------------------------------------------
VOID Sample::InitializeFriendAvatarEnumerationData()
{
    ZeroMemory( m_Friends, sizeof( m_Friends ) );
    ZeroMemory( m_FriendsXuids, sizeof( m_FriendsXuids ) );
    ZeroMemory( m_FriendsAvatarMetadata, sizeof( m_FriendsAvatarMetadata ) );

    m_dwFriendsCount = 0;
    m_pSettingResults = NULL;
    m_bFriendsPresenceDataReady = FALSE;
    m_bFriendsAvatarMetadataReady = FALSE;
    m_hFriendsEnum = INVALID_HANDLE_VALUE;
}

//--------------------------------------------------------------------------------------
// Name: FreeAvatarResources()
// Desc: Free Avatar resources for the local user and all friends
//--------------------------------------------------------------------------------------
VOID Sample::FreeAvatarResources( const BOOL bAlsoFreeAnimations )
{
    if( bAlsoFreeAnimations && UserAvatarData::s_bAnimationsLoaded )
    {
        for( DWORD i = 0; i < NUM_ANIMATIONS; i++ )
        {
            if( UserAvatarData::s_pAnimations[i] )
            {
                UserAvatarData::s_pAnimations[i]->Release();
            }
        }

        UserAvatarData::s_bAnimationsLoaded = FALSE;
    }

    for( AvatarDataIter iter = m_vAvatarData.begin(); iter != m_vAvatarData.end(); ++iter )
    {
        UserAvatarData& avatarData = *(*iter);

        if( avatarData.m_pRenderer )
        {
            // Release renderer
            avatarData.m_pRenderer->Release();   
            avatarData.m_pRenderer = NULL;
        }

        delete (*iter);
    }

    m_vAvatarData.clear();
}


//--------------------------------------------------------------------------------------
// Name: LoadLocalUserAvatarMetadata()
// Desc: Load the Avatar metadata of the local user
//--------------------------------------------------------------------------------------
HRESULT Sample::LoadLocalUserAvatarMetadata( UserAvatarData& avatarData )
{
    // Obtain the Avatar metadata for the user.  If the call fails, (the user doesn't
    // have an Avatar associated with their profile yet or no one is 
    // signed in) then just load a random Avatar.
    if( ERROR_SUCCESS != XAvatarGetMetadataLocalUser( 0, &avatarData.m_metadata, NULL ) )
    {
        if ( ERROR_SUCCESS != XAvatarGetMetadataRandom( XAVATAR_BODY_TYPE_ALL, 1, &avatarData.m_metadata, NULL ) )
        {
            return E_FAIL;
        }
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: ReloadLocalUserAndFriendsAvatarData()
// Desc: Reloads Avatar of local user and also of friends if requested
//--------------------------------------------------------------------------------------
HRESULT Sample::ReloadLocalUserAndFriendsAvatarData( const BOOL bIncludingFriends )
{
    FreeAvatarResources( FALSE );

    UserAvatarData& avatarData  = *new UserAvatarData();
    ZeroMemory( &avatarData, sizeof( UserAvatarData ) ); 

    XUserGetXUID( 0, &avatarData.m_xuid );
    avatarData.m_matWorld = XMMatrixIdentity();
    avatarData.m_pRenderer = NULL;

    RETURN_ON_FAIL( LoadLocalUserAvatarMetadata( avatarData ) );
    RETURN_ON_FAIL( LoadAvatar( avatarData ) );

    m_vAvatarData.push_back( &avatarData );

    if( bIncludingFriends )
    {
        XUSER_SIGNIN_STATE signinState = XUserGetSigninState( 0 );
        if( signinState == eXUserSigninState_SignedInToLive )
        {
            m_bFriendsPresenceDataReady = FALSE;
            RetrieveFriendsPresenceData( 0, m_Friends, _countof( m_Friends ), NULL, &m_Overlapped, &m_hFriendsEnum ); 
        }
    }
    
    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: LoadAvatar()
// Desc: Load the Avatar
//--------------------------------------------------------------------------------------
HRESULT Sample::LoadAvatar( UserAvatarData& avatarData )
{
    RETURN_ON_FAIL( XAvatarCreateRenderer( &avatarData.m_metadata,
                                           XAVATAR_COMPONENT_MASK_ALL,
                                           XAVATAR_SHADOW_SIZE_LARGE,
                                           XAVATAR_MIPMAP_RECOMMENDED_BUFFER_SIZE,
                                           0,
                                           &avatarData.m_pRenderer ) );

    // Start the waiting effect
    XAvatarWaitingEffectStart( &avatarData.m_WaitingEffect );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: CreateRenderTargets()
// Desc: Create RenderTargets, Back/Front Buffers, and Depth Stencil
//--------------------------------------------------------------------------------------
VOID Sample::CreateRenderTargets()
{
    D3DSURFACE_PARAMETERS params = {0};

    m_pd3dDevice->CreateRenderTarget(
        g_dwTileWidth, g_dwTileHeight, D3DFMT_X8R8G8B8, D3DMULTISAMPLE_4_SAMPLES, 0, 0, &m_pBackBuffer, &params );

    params.Base = m_pBackBuffer->Size / GPU_EDRAM_TILE_SIZE;
    params.HierarchicalZBase = 0;
    m_pd3dDevice->CreateDepthStencilSurface(
        g_dwTileWidth, g_dwTileHeight, D3DFMT_D24S8, D3DMULTISAMPLE_4_SAMPLES, 0, 0, &m_pDepthBuffer, &params );

    m_pd3dDevice->CreateTexture(
        g_dwFrameWidth, g_dwFrameHeight, 1, 0, D3DFMT_LE_X8R8G8B8, 0, &m_pFrontBuffer[0], NULL );
    m_pd3dDevice->CreateTexture(
        g_dwFrameWidth, g_dwFrameHeight, 1, 0, D3DFMT_LE_X8R8G8B8, 0, &m_pFrontBuffer[1], NULL );
    
}


//--------------------------------------------------------------------------------------
// Name: LoadAnimationFromFile()
// Desc: Creates an IXAvatarAnimation from a file
//--------------------------------------------------------------------------------------
HRESULT Sample::LoadAnimationFromFile( LPCTSTR szAnimFilename, LPXAVATARANIMATION* ppAnim )
{
    HANDLE hAnimFile                = NULL;
    BYTE* pAnimBuffer               = NULL;
    DWORD dwBytesRead               = 0;
    DWORD dwFileSize                = 0; 

    hAnimFile = CreateFile( szAnimFilename,
                            GENERIC_READ,
                            FILE_SHARE_READ,
                            NULL,
                            OPEN_EXISTING,
                            FILE_ATTRIBUTE_NORMAL,
                            NULL);

    if( hAnimFile != INVALID_HANDLE_VALUE )
    {  
        // Load the file into a memory buffer
        dwFileSize = GetFileSize(hAnimFile, NULL);
        pAnimBuffer = reinterpret_cast<BYTE*>( XMemAlloc( dwFileSize, 
                                                          MAKE_XALLOC_ATTRIBUTES( 0,
                                                                                  TRUE,
                                                                                  TRUE,
                                                                                  FALSE,
                                                                                  SAMPLE_ALLOCATOR_ID,
                                                                                  XALLOC_ALIGNMENT_16,
                                                                                  XALLOC_MEMPROTECT_READWRITE,
                                                                                  FALSE,
                                                                                  XALLOC_MEMTYPE_HEAP)));

        // Create the animation assets from the file buffer
        if ( pAnimBuffer != NULL )
        {
            BOOL bSucceeded = ReadFile(hAnimFile, pAnimBuffer, dwFileSize, &dwBytesRead, NULL);
            assert(bSucceeded && (dwBytesRead == dwFileSize));
            if( bSucceeded && (dwBytesRead == dwFileSize) )
            {
                RETURN_ON_FAIL( XAvatarLoadAnimationFromBuffer( dwBytesRead, pAnimBuffer, ppAnim ) );
            }            
        }

        CloseHandle( hAnimFile );
    }
    else
    {
        return E_FAIL;
    }

    return S_OK;
}

//--------------------------------------------------------------------------------------
// Name: LoadCustomAvatarAsset()
// Desc: Loads a custom avatar asset .bin file onto an Avatar.
//--------------------------------------------------------------------------------------
HRESULT Sample::LoadCustomAvatarAsset(UserAvatarData& avatarData)
{
    // Don't try to load another asset if one is currently loading
    if ( NULL != m_pAvatarAssetBuffer )
    {
        OutputDebugString( "Not ready to load another asset.\n" );
        return E_FAIL;
    }

    static DWORD dwCustomAssetIndex = 0;
    DWORD dwAvatarAssetSize         = 0;
    CHAR strFileName[] = "game:\\Media\\Assets\\avatar_tshirt.bin";

    // Load file to m_pAvatarAssetBuffer
    {

        HANDLE hFile = CreateFile(strFileName, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                                       FILE_FLAG_SEQUENTIAL_SCAN, NULL );
        if( hFile == INVALID_HANDLE_VALUE )
        {
            OutputDebugString( "Could not find  file.\n" );
            return E_FAIL;
        }
        dwAvatarAssetSize = GetFileSize( hFile, NULL );
        m_pAvatarAssetBuffer = reinterpret_cast<BYTE*>( XMemAlloc( dwAvatarAssetSize,                                                           
                                                        AVATAR_ASSET_BUFFER_ATTRIBUTES));
        assert( dwAvatarAssetSize > 0 );  

        if ( m_pAvatarAssetBuffer != NULL )
        {
            DWORD dwBytesRead = 0;
            BOOL bSucceeded = ReadFile( hFile, m_pAvatarAssetBuffer, dwAvatarAssetSize, &dwBytesRead, 0 );
            assert(bSucceeded && (dwBytesRead == dwAvatarAssetSize));
            if( !bSucceeded )
            {
                XMemFree( m_pAvatarAssetBuffer, AVATAR_ASSET_BUFFER_ATTRIBUTES );
                m_pAvatarAssetBuffer = NULL;
                return E_FAIL;
            }
        }
        CloseHandle( hFile );
    }  

    if ( m_pAvatarAssetBuffer != NULL )
    {
        // Set the custom asset into the Avatar metadata
        //  This will fail if the gender of the current avatar is not the expected type for the asset being applied
        HRESULT result = XAvatarSetCustomAsset(dwAvatarAssetSize, m_pAvatarAssetBuffer, 0, NULL, &avatarData.m_metadata);
        if (result != S_OK)
        {
            OutputDebugString( "XAvatarSetCustomAsset failed.\n" );
            XMemFree( m_pAvatarAssetBuffer, AVATAR_ASSET_BUFFER_ATTRIBUTES );
            m_pAvatarAssetBuffer = NULL;
            return E_FAIL;
        }
    }

    // Reload the AvatarRenderer
    LoadAvatar( avatarData);

    return S_OK;    
}


//--------------------------------------------------------------------------------------
// Name: Update()
// Desc: Called once per frame.  Entry point for animating the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Update()
{
    PIXBeginNamedEvent( 0, __FUNCTION__ );

    // Get a reference to the local user's Avatar data
    UserAvatarData& localUserAvatarData = *m_vAvatarData.front();

    // Check for "Avatar changed" notifications
    DWORD dwNotificationID;
    ULONG_PTR ulParam;

    static XUSER_SIGNIN_STATE prevState = XUserGetSigninState( 0 );
    if( XNotifyGetNext( m_hNotification, 0, &dwNotificationID, &ulParam ) )
    {
        switch( dwNotificationID )
        {
            case XN_SYS_AVATARCHANGED:
            {
                ReloadLocalUserAndFriendsAvatarData( FALSE );
            }
            break;
            case XN_SYS_SIGNINCHANGED:
            {
                XUSER_SIGNIN_STATE state = XUserGetSigninState( 0 );
                if( ( state != prevState ) )
                {
                    prevState = state;
                    ReloadLocalUserAndFriendsAvatarData( TRUE );
                }
            }
            break;
        }
    }

    // Get elapsed time
    FLOAT fDeltaTime = ( FLOAT )m_Timer.GetElapsedTime();

    // Get current gamepad state
    ATG::GAMEPAD* pGamepad = ATG::Input::GetMergedInput();

    // The Back button toggles the help screen.
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_BACK )
        m_bDrawHelp = !m_bDrawHelp;

    // Rotate view
    m_fLookYaw     -= pGamepad->fX2 * fDeltaTime;
    m_fLookPitch   -= pGamepad->fY2 * fDeltaTime;
    m_fLookYaw     = fmodf( m_fLookYaw, XM_2PI );
    m_fLookPitch   = fmodf( m_fLookPitch, XM_2PI );

    XMMATRIX lookAtMatrix   = XMMatrixRotationRollPitchYaw( m_fLookPitch, m_fLookYaw, 0.0f );
    XMVECTOR m_vLookToZ     = XMVector3Transform(XMVectorSet( 0.0f, 0.0f, -1.0f, 1.0f ), lookAtMatrix);

    // Move viewing position
    m_vEyePt = XMVectorAdd(m_vEyePt, XMVectorScale(lookAtMatrix.r[0], fDeltaTime * pGamepad->fX1));
    m_vEyePt = XMVectorAdd(m_vEyePt, XMVectorScale(lookAtMatrix.r[2], fDeltaTime * -pGamepad->fY1));

    m_matView               = XMMatrixLookToRH( m_vEyePt, m_vLookToZ, m_vUp );

    //
    // Tell the our local user's renderer to play an animation when a button is pressed

    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_A )
    {
        RETURN_ON_FAIL( localUserAvatarData.m_pRenderer->PlayAnimations(1, &UserAvatarData::s_pAnimations[0], XAVATAR_PLAYANIMATIONS_FLAGS_DEFAULT) );
    }

    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_B )
    {
        RETURN_ON_FAIL( localUserAvatarData.m_pRenderer->PlayAnimations(1, &UserAvatarData::s_pAnimations[1], XAVATAR_PLAYANIMATIONS_FLAGS_DEFAULT) );
    }

    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_X )
    {
        RETURN_ON_FAIL( localUserAvatarData.m_pRenderer->PlayAnimations(1, &UserAvatarData::s_pAnimations[2], XAVATAR_PLAYANIMATIONS_FLAGS_DEFAULT) );
    }

    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_Y )
    {
        RETURN_ON_FAIL( localUserAvatarData.m_pRenderer->PlayAnimations(1, &UserAvatarData::s_pAnimations[3], XAVATAR_PLAYANIMATIONS_FLAGS_DEFAULT) );
    }
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER )
    {
        RETURN_ON_FAIL( LoadCustomAvatarAsset(localUserAvatarData));
    }

    // Force random Avatar to be generated for the local user
    if(pGamepad->wPressedButtons & XINPUT_GAMEPAD_START )
    {
        ReloadLocalUserAndFriendsAvatarData( FALSE );
    }

    //
    // Update all Avatar renderers
    for( AvatarDataIter iter = m_vAvatarData.begin(); iter != m_vAvatarData.end(); ++iter )
    {
        UserAvatarData& avatarData = *(*iter);

        RETURN_ON_FAIL( avatarData.m_pRenderer->Update( fDeltaTime ) );

        // Render the waiting effect if the renderer isn't ready yet
        if( avatarData.m_pRenderer->GetStatus() == E_PENDING )
        {
            avatarData.m_bRenderWaitingEffect = TRUE;
            avatarData.m_bWaitingEffectFadeOut = FALSE;
        }
    }    

    
    PIXEndNamedEvent();

    //
    // The rest requires the local user to be signed into LIVE
    XUSER_SIGNIN_STATE state = XUserGetSigninState( 0 );
    
    const BOOL bIsSignedIntoLive = ( state == eXUserSigninState_SignedInToLive );

    // Friends presence data ready?
    if( bIsSignedIntoLive && !m_bFriendsPresenceDataReady && XHasOverlappedIoCompleted( &m_Overlapped ) )
    {
        m_bFriendsPresenceDataReady = TRUE;

        // For asynchronous calls to XFriendsCreateEnumerator, the number of 
        // friend items enumerated is returned in the InternalHigh member
        // of our overlapped
        m_dwFriendsCount = m_Overlapped.InternalHigh;

        // Store friends' XUIDs in the m_FriendsXuids array
        for( DWORD i = 0; i < m_dwFriendsCount; ++i )
        {
            m_FriendsXuids[i] = m_Friends[i].xuid;
        }

        CloseHandle( m_hFriendsEnum );
        m_hFriendsEnum = INVALID_HANDLE_VALUE;

        // Retrieve friends Avatar metadata
        m_bFriendsAvatarMetadataReady = FALSE;
        RetrieveFriendsAvatarMetadata( 0, 
                                       m_FriendsXuids, 
                                       m_dwFriendsCount, 
                                       m_FriendsAvatarMetadata, 
                                       &m_pSettingResults, 
                                       &m_Overlapped );
    }
    else if( bIsSignedIntoLive && m_bFriendsPresenceDataReady && 
             m_dwFriendsCount && !m_bFriendsAvatarMetadataReady && XHasOverlappedIoCompleted( &m_Overlapped ) )
    {
        // Friends Avatar metadata data ready? If so, create our friends' Avatar instances from it
        m_bFriendsAvatarMetadataReady = TRUE;

        // Retrieve metadata from profile settings
        GetFriendsAvatarMetadataFromUserProfileSettings( m_FriendsAvatarMetadata, m_pSettingResults );
    }

    // Avatar metadata from profile settings ready? If so, create Avatar instances from
    // the metadata
    if( m_bFriendsPresenceDataReady && m_bFriendsAvatarMetadataReady && m_pSettingResults )
    {
        // Create Avatar instances from metadata
        for( DWORD i = 0; i < m_pSettingResults->dwSettingsLen; ++i )
        {
            UserAvatarData& avatarData = *new UserAvatarData();
            ZeroMemory( &avatarData, sizeof( UserAvatarData ) ); 
            
            avatarData.m_xuid = m_Friends[i].xuid;

            XMemCpy( &avatarData.m_metadata, &m_FriendsAvatarMetadata[i], sizeof( m_FriendsAvatarMetadata[i] ) );

            // Load Avatar for the friend. Note that we use the same IXAvatarAnimation instances for
            // the local user and friends, so there's no need to load animations for the friend.
            if( ERROR_SUCCESS != LoadAvatar( avatarData ) )
            {
                ATG::FatalError( "LoadAvatar failed!" );
            }

            // World matrix for this Avatar
            if( i % 2 == 0 )
            {
                avatarData.m_matWorld = XMMatrixTranslation( ( i + 1 ) * 0.5f, 0.0f, -( ( i + 1 ) * 0.5f ) );
            }
            else
            {
                avatarData.m_matWorld = XMMatrixTranslation( -( ( i + 1 ) * 0.5f ), 0.0f, -( ( i + 1 ) * 0.5f ) );
            }

            // Add friend's Avatar data to our vector
            m_vAvatarData.push_back( &avatarData );
        }

        // No longer need memory associated with profile settings
        delete[] (BYTE*)m_pSettingResults;
        m_pSettingResults = NULL;
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Render()
// Desc: Renders the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Render()
{
    PIXBeginNamedEvent( 0, __FUNCTION__ );

    // Create the shadow before rendering scene.  The shadow uses EDRAM, so if this is 
    // called in the middle of scene rendering, EDRAM will be corrupted.
    PIXBeginNamedEvent( 0xFFFFFFFF, "Avatar Shadow Creation" );
    for( AvatarDataCIter citer = m_vAvatarData.begin(); citer != m_vAvatarData.end(); ++citer )
    {
        const UserAvatarData& avatarData = *(*citer);
        avatarData.m_pRenderer->RenderShadow( m_pd3dDevice, avatarData.m_matWorld, m_matView, m_matProj );
    }    
    PIXEndNamedEvent();

    // Render scene
    const D3DVECTOR4 clearColor = { 0.75f, 0.78f, 0.86f, 1.0f };
    m_pd3dDevice->SetRenderTarget( 0, m_pBackBuffer );
    m_pd3dDevice->SetDepthStencilSurface( m_pDepthBuffer );
    m_pd3dDevice->BeginTiling( 0, ARRAYSIZE(g_tiles), g_tiles, &clearColor, 1, 0 );

    PIXBeginNamedEvent( 0xFFFFFFFF, "Avatar Render" );
    for( size_t i = 0; i < m_vAvatarData.size(); ++i )
    {
        UserAvatarData& avatarData = *m_vAvatarData.at( i );

        avatarData.m_pRenderer->Render( m_pd3dDevice, avatarData.m_matWorld, m_matView, m_matProj );

        if( avatarData.m_bRenderWaitingEffect  )
        {
            HRESULT status = XAvatarWaitingEffectRender( &avatarData.m_WaitingEffect, m_pd3dDevice, avatarData.m_matWorld, m_matView, m_matProj );

            // Stop rendering the waiting effect once it has finished fading out
            if(status == S_OK && avatarData.m_bWaitingEffectFadeOut )
            {
                avatarData.m_bRenderWaitingEffect = FALSE;
            }

            // If the renderer is ready and the waiting effect is done fading in, tell it to fade out
            if( avatarData.m_pRenderer->GetStatus() == S_OK && status == S_OK )
            {
                // Once the renderer is ready, stop the waiting effect
                if( XAvatarWaitingEffectStop( &avatarData.m_WaitingEffect ) == S_OK )
                {
                    // Note that the waiting effect is still fading out, so we need to keep rendering it
                    avatarData.m_bWaitingEffectFadeOut = TRUE;

                    // Dispose of the asset buffer now that it's done loading
                    if ( NULL != m_pAvatarAssetBuffer )
                    {
                        XMemFree( m_pAvatarAssetBuffer, AVATAR_ASSET_BUFFER_ATTRIBUTES);
                        m_pAvatarAssetBuffer = NULL;
                    }
                }
            }

        }
    }    
    PIXEndNamedEvent();

    RenderOverlays();

    m_pd3dDevice->EndTiling( 0, NULL, m_pFrontBuffer[m_dwCurFrontBuffer], NULL, 1, 0, NULL );
    

    // Present the backbuffer contents to the display
    m_pd3dDevice->SynchronizeToPresentationInterval();
    m_pd3dDevice->Swap( m_pFrontBuffer[ m_dwCurFrontBuffer ], NULL );

    m_dwCurFrontBuffer = ( m_dwCurFrontBuffer + 1 ) % 2;

    m_Timer.MarkFrame();

    PIXEndNamedEvent();
    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: RenderOverlays()
// Desc: Show title, timers, and help.
//--------------------------------------------------------------------------------------
VOID Sample::RenderOverlays()
{
    PIXBeginNamedEvent( 0, __FUNCTION__ );

        if( m_bDrawHelp )
        {
            m_Help.Render( &m_Font, g_HelpCallouts, NUM_HELP_CALLOUTS );
        }
        else
        {
            m_Font.Begin();

            m_Font.SetScaleFactors( 1.2f, 1.2f );
            m_Font.DrawText( 0, 0, 0xffffffff, L"Avatar Renderer" );

            m_Font.SetScaleFactors( 1.0f, 1.0f );
            m_Font.DrawText( 0, 0, 0xffffff00, m_Timer.GetFrameRate(), ATGFONT_RIGHT );

            m_Font.End();
        }
    
    PIXEndNamedEvent();
}
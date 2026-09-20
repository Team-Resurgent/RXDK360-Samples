//--------------------------------------------------------------------------------------
// AvatarGetAssets.cpp
//
// This sample demonstrates the use of the XAvatarGetAssets API to render and animate
// avatars.
//
// XNA Developer Connection
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include <vector>
#include <xtl.h>            
#include <xavatar.h>
#include <xgraphics.h>

#include <AtgApp.h>
#include <AtgFont.h>
#include <AtgHelp.h>
#include <AtgInput.h>
#include <AtgSignIn.h>
#include <AtgUtil.h>

static const DWORD MIPMAP_BUFFER_SIZE   = XAVATAR_MIPMAP_RECOMMENDED_BUFFER_SIZE;

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
    { ATG::HELP_START_BUTTON,   ATG::HELP_PLACEMENT_1, L"Load random local avatar" },
};

static const DWORD NUM_HELP_CALLOUTS = ARRAYSIZE( g_HelpCallouts );
static const DWORD NUM_ANIMATIONS       = 4;
static const DWORD MAX_AVATAR_FRIENDS   = 10;

#define GPU_MEMORY_BUFFER_ALIGN 4096        // 4k alignment for textures
static const DWORD SAMPLE_ALLOCATOR_ID = 0;

inline DWORD RoundUpToAlign( DWORD value, DWORD align )
{
    assert( ( align & ( align - 1 ) ) == 0 );   // checks the align is a power of two
    return ( ( value + align - 1 ) & ~( align - 1 ) );
}

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
       
static const DWORD JOINT_BUFFER_COUNT = 2;  // double buffered to allow skinning using vfetch

//--------------------------------------------------------------------------------------
// Name: class Sample
// Desc: Main class to run this application.
//--------------------------------------------------------------------------------------
class Sample : public ATG::Application
{
public:
    Sample() : m_bDrawHelp( FALSE ), m_dwCurFrontBuffer( 0 ), 
               m_dwFriendsCount( 0 ), m_pSettingResults( NULL ), m_bFriendsPresenceDataReady( FALSE ),
               m_hFriendsEnum( INVALID_HANDLE_VALUE )
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

        // Animations. Reuse IXAvatarAnimation instances for all loaded Avatars
        static LPXAVATARANIMATION   s_pAnimations[ NUM_ANIMATIONS ];
        static BOOL                 s_bAnimationsLoaded;
        LPXAVATARANIMATION          m_pAnim;	// current animation
        XAVATAR_ANIMATION_CURSOR    m_Cursor;

        XAVATAR_WAITING_EFFECT      m_WaitingEffect;
        BOOL                        m_bGeometryReady;
        BOOL                        m_bTexturesReady;
        DWORD                       m_dwMipMapBufferOffset;
        XOVERLAPPED                 m_GetAssetsOverlapped;
        XOVERLAPPED                 m_MipMapOverlapped;
        BOOL                        m_bRenderWaitingEffect;
        BOOL                        m_bWaitingEffectFadeOut;
        XMMATRIX                    m_matWorld;

        // Vertex buffers, index buffers, and texture headers for each of the components
        D3DVertexBuffer             m_VertexBuffers[ XAVATAR_COMPONENT_COUNT ];
        D3DIndexBuffer              m_IndexBuffers [ XAVATAR_COMPONENT_COUNT ];
        D3DTexture                  m_Textures     [ XAVATAR_COMPONENT_COUNT ][ XAVATAR_MAX_TEXTURES_PER_MODEL ][ XAVATAR_MAX_LAYERS_PER_TEXTURE ];

        // Raw avatar asset buffers.  These hold the data retrieved by XAvatarGetAssets()
        XAVATAR_ASSETS*             m_pAssets;            // asset buffer
        BYTE*                       m_pGpuBuffer;         // GPU buffer
        BYTE*                       m_pJointBuffer;

        // Animation data - updated every frame from the animation assets
        XAVATAR_SKELETON_POSE_JOINT m_AvatarJointPose[ XAVATAR_MAX_SKELETON_JOINTS    ]; // animated pos/rot/scale for the cur frame
        DWORD                       m_dwTextureLayers[ XAVATAR_ANIMATED_TEXTURE_COUNT ]; // animated texture IDs for the cur frame

        // Joints - calculated from animation data
        D3DVertexBuffer             m_JointBufferVBs[ JOINT_BUFFER_COUNT ];  // Joints ready to be consumed by GPU
        DWORD                       m_dwJointBufferIndex;                    // current joint buffer in use
    };

    virtual HRESULT Initialize();
    
    HRESULT         ReloadLocalUserAndFriendsAvatarData( const BOOL bIncludingFriends );
    HRESULT         LoadAvatar( UserAvatarData& avatarData );
    HRESULT         CreateAvatar( UserAvatarData& avatarData );
    HRESULT         CompleteIO( UserAvatarData& avatarData );
    HRESULT         SetupTextures( UserAvatarData& avatarData );
    VOID            DestroyAvatar( UserAvatarData& avatarData );
    HRESULT         LoadLocalUserAvatarMetadata( UserAvatarData& avatarData );
    HRESULT         LoadAnimationFromFile( LPCSTR szAnimFilename, LPXAVATARANIMATION* ppAnim );
    virtual HRESULT Update();
    VOID            InitializeFriendAvatarEnumerationData();
    VOID            CreateRenderTargets();
    VOID            FreeAvatarResources( const BOOL bAlsoFreeAnimations );
    virtual HRESULT Render();
    VOID            RenderAvatar( const UserAvatarData& avatarData );
    VOID            RenderOverlays();
    HRESULT         RebuildJoints( UserAvatarData& avatarData );
    HRESULT         UpdateAnimation( FLOAT fDeltaTime, UserAvatarData& avatarData );
    HRESULT         ClearShaderTexturesAndConstants( );


    WORD GetShaderTextureIndex( const XAVATAR_SHADER_PARAM& param, XAVATAR_SHADER shader ) const;
    VOID SetShaderTexturesAndConstants( const UserAvatarData&           avatarData,
                                        DWORD                           dwModelIndex,
                                        const XAVATAR_SHADER_INSTANCE*  shaderInstance,
                                        const XAVATAR_TEXTURE*          modelTextures,
                                        const DWORD*                    animatedTextureLayers ) ;

    BYTE GetPixelConstantRegister(  const XAVATAR_SHADER_PARAM* param ) const;
    BYTE GetVertexConstantRegister( const XAVATAR_SHADER_PARAM* param,
                                    XAVATAR_SHADER              shader ) const;


    HRESULT PreRender(          const XMMATRIX*     modelTransform,
                                const XMMATRIX*     viewTransform,
                                const XMMATRIX*     projectionTransform );
    HRESULT RenderOpaque(       const UserAvatarData& avatarData,
                                DWORD dwModelIndex );
    HRESULT RenderTransparent(  const UserAvatarData& avatarData,
                                DWORD dwModelIndex );
    HRESULT PostRender( );
  
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
   
    // Shaders and vertex decls
    struct D3dShaders_t
    {
        D3DVertexDeclaration*   m_VertexDeclaration;
        D3DVertexShader*        m_VertexShader;
        D3DPixelShader*         m_PixelShader;
    };
    D3dShaders_t                m_D3dShaders[ XAVATAR_SHADER_COUNT ];

    // Lookup tables for converting shader parameters to GPU registers
    BYTE                        m_ShaderTextureIndexMap    [ XAVATAR_SHADER_PARAM_USAGE_COUNT ][ XAVATAR_SHADER_COUNT ];
    BYTE                        m_VertexConstantRegisterMap[ XAVATAR_SHADER_PARAM_USAGE_COUNT ][ XAVATAR_SHADER_COUNT ];
    BYTE                        m_PixelConstantRegisterMap [ XAVATAR_SHADER_PARAM_USAGE_COUNT ];
    
    // Friends
    XONLINE_FRIEND                     m_Friends[ MAX_AVATAR_FRIENDS ];
    DWORD                              m_dwFriendsCount;
    XAVATAR_METADATA                   m_FriendsAvatarMetadata[ MAX_AVATAR_FRIENDS ];
    XUSER_READ_PROFILE_SETTING_RESULT* m_pSettingResults;
    BOOL                               m_bFriendsPresenceDataReady;
    BOOL                               m_bFriendsAvatarMetadataReady;
    XOVERLAPPED                        m_FriendsOverlapped;
    HANDLE                             m_hFriendsEnum;

    std::vector<UserAvatarData*>       m_vAvatarData;

    typedef std::vector<UserAvatarData*>::const_iterator  AvatarDataCIter;
    typedef std::vector<UserAvatarData*>::iterator        AvatarDataIter;

    // Lights
    XMVECTOR                    m_vLightColor[3];
    XMVECTOR                    m_vLightDirection[3];

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
                                       const XONLINE_FRIEND* const pArrayFriends, 
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
    if( !pArrayFriends )
    {
        ATG::FatalError( "pArrayFriends is incorrectly NULL\n" );
    }

    //
    // Build vector of friends XUIDs from passed-in friends presence data
    //
    std::vector<XUID> vXuids;
    for( DWORD i = 0; i < cFriendsCount; ++i )
    {
        vXuids.push_back( pArrayFriends[i].xuid );
    }

    static const DWORD arSettingID[] = { XPROFILE_AVATAR_METADATA };

    // Determine the maximum read buffer size and allocate space for it
    // Determine buffer size by passing a zero settings size
    DWORD dwSettingSizeMax = 0;
    DWORD dwErr = ERROR_SUCCESS;
    dwErr = XUserReadProfileSettingsByXuid( 0,                 // A title in your family or 0 for the current title
                                            0,                 // User index of requesting user
                                            vXuids.size(),     // Count of XUIDs
                                            &vXuids[0],        // Pointer to array of XUIDs to request settings for
                                            1,                 // Count of setting ids in pdwSettingIds
                                            arSettingID,       // Pointer to array of settings to retrieve
                                            &dwSettingSizeMax, // Size of pResults buffer. Initially 0 to retrieve required buffer size
                                            NULL,              // Results buffer. Not used when retrieving buffer size
                                            NULL );            // Overlapped (not used)

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

    dwErr = XUserReadProfileSettingsByXuid( 0,                 // A title in your family or 0 for the current title
                                            0,                 // User index of requesting user
                                            vXuids.size(),     // Count of XUIDs
                                            &vXuids[0],        // Pointer to array of XUIDs to request settings for
                                            1,                 // Count of setting ids in pdwSettingIds
                                            arSettingID,       // Pointer to array of settings to retrieve
                                            &dwSettingSizeMax, // Size of pResults buffer.  If *pcbResults is 0 then required size is returned.
                                            (XUSER_READ_PROFILE_SETTING_RESULT*)pData, // Results
                                            pOverlapped );     // Overlapped

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

    ZeroMemory( &m_FriendsOverlapped, sizeof( XOVERLAPPED ) );

    // Initialize all data used to enumerate friends and retrieve friends' Avatar metadata
    // to sensible values
    InitializeFriendAvatarEnumerationData();

    // Initialize signin
    ATG::SignIn::Initialize( 1, 4, TRUE, 1 );

    // Initialize the avatar library
    static const DWORD dwAssetLoadHardwareThread = 5;
    if( FAILED( XAvatarInitialize(      XAVATAR_COORDINATE_SYSTEM_RIGHT_HANDED, 
                                        0, 
                                        dwAssetLoadHardwareThread,
                                        0,    
                                        m_pd3dDevice) ) ) // The device is only necessary to enable the waiting effect.
    {
        ATG_PrintError( "Unable to load Avatar asset pack\n");
        return E_FAIL;
    }

    // Now create the animation interfaces for the local user. We'll re-use these
    // interfaces for all Avatar instances
    ZeroMemory( &Sample::UserAvatarData::s_pAnimations[0], sizeof( Sample::UserAvatarData::s_pAnimations ) );

    XAvatarLoadAnimation( &XAVATAR_ANIMATION_GENERIC_CLAP, 0, &UserAvatarData::s_pAnimations[0] );
    XAvatarLoadAnimation( &XAVATAR_ANIMATION_GENERIC_WAVE, 0, &UserAvatarData::s_pAnimations[1] );
    LoadAnimationFromFile( "game:\\Media\\Anim\\anim_celebration.bin", &UserAvatarData::s_pAnimations[2] );
    LoadAnimationFromFile( "game:\\Media\\Anim\\anim_stand.bin", &UserAvatarData::s_pAnimations[3] );
    Sample::UserAvatarData::s_bAnimationsLoaded = TRUE;

    // Load Avatar instances for the local user and friends
    RETURN_ON_FAIL( ReloadLocalUserAndFriendsAvatarData( TRUE ) );

    // Zero the shader structure before we begin to load shaders.
    ZeroMemory( m_D3dShaders, sizeof( m_D3dShaders ) );

    // Define the vertex structure for the "head" shaders. These have six
    // UV sets, for a total of 52 bytes per vertex in stream 0.
    const D3DVERTEXELEMENT9 headDeclDesc[] =
    {
        { 0,  0, D3DDECLTYPE_FLOAT3,    D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION,     0 }, // XAVATAR_VERTEX_POSITION
        { 0, 12, D3DDECLTYPE_HEND3N,    D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_NORMAL,       0 }, // XAVATAR_VERTEX_NORMAL
        { 0, 16, D3DDECLTYPE_UBYTE4N,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_BLENDWEIGHT,  0 }, // XAVATAR_VERTEX_WEIGHTS
        { 0, 20, D3DDECLTYPE_UBYTE4,    D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_BLENDINDICES, 0 }, // XAVATAR_VERTEX_BINDINGS
        { 0, 24, D3DDECLTYPE_D3DCOLOR,  D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR,        0 }, // XAVATAR_VERTEX_COLOR
        { 0, 28, D3DDECLTYPE_FLOAT16_2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD,     0 }, // XAVATAR_VERTEX_UV
        { 0, 32, D3DDECLTYPE_FLOAT16_2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD,     1 }, // XAVATAR_VERTEX_UV
        { 0, 36, D3DDECLTYPE_FLOAT16_2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD,     2 }, // XAVATAR_VERTEX_UV
        { 0, 40, D3DDECLTYPE_FLOAT16_2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD,     3 }, // XAVATAR_VERTEX_UV
        { 0, 44, D3DDECLTYPE_FLOAT16_2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD,     4 }, // XAVATAR_VERTEX_UV
        { 0, 48, D3DDECLTYPE_FLOAT16_2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD,     5 }, // XAVATAR_VERTEX_UV
        { 1,  0, D3DDECLTYPE_FLOAT4,    D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD,     6 }, // Joint buffer.
        { 1, 16, D3DDECLTYPE_FLOAT4,    D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD,     7 }, // Joint buffer.
        { 1, 32, D3DDECLTYPE_FLOAT4,    D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD,     8 }, // Joint buffer.
        D3DDECL_END()
    };

    // Define the vertex structure for the "body" shaders. These have three
    // UV sets, for a total of 40 bytes per vertex in stream 0.
    const D3DVERTEXELEMENT9 bodyDeclDesc[] =
    {
        { 0,  0, D3DDECLTYPE_FLOAT3,    D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION,     0 }, // XAVATAR_VERTEX_POSITION
        { 0, 12, D3DDECLTYPE_HEND3N,    D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_NORMAL,       0 }, // XAVATAR_VERTEX_NORMAL
        { 0, 16, D3DDECLTYPE_UBYTE4N,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_BLENDWEIGHT,  0 }, // XAVATAR_VERTEX_WEIGHTS
        { 0, 20, D3DDECLTYPE_UBYTE4,    D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_BLENDINDICES, 0 }, // XAVATAR_VERTEX_BINDINGS
        { 0, 24, D3DDECLTYPE_D3DCOLOR,  D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR,        0 }, // XAVATAR_VERTEX_COLOR
        { 0, 28, D3DDECLTYPE_FLOAT16_2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD,     0 }, // XAVATAR_VERTEX_UV
        { 0, 32, D3DDECLTYPE_FLOAT16_2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD,     1 }, // XAVATAR_VERTEX_UV
        { 0, 36, D3DDECLTYPE_FLOAT16_2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD,     2 }, // XAVATAR_VERTEX_UV
        { 1,  0, D3DDECLTYPE_FLOAT4,    D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD,     6 }, // Joint buffer.
        { 1, 16, D3DDECLTYPE_FLOAT4,    D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD,     7 }, // Joint buffer.
        { 1, 32, D3DDECLTYPE_FLOAT4,    D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD,     8 }, // Joint buffer.
        D3DDECL_END()
    };

    // Define the vertex structure for the "shiny body" shaders. These have two
    // UV sets, for a total of 36 bytes per vertex in stream 0.
    const D3DVERTEXELEMENT9 shinyDeclDesc[] =
    {
        { 0,  0, D3DDECLTYPE_FLOAT3,    D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION,     0 }, // XAVATAR_VERTEX_POSITION
        { 0, 12, D3DDECLTYPE_HEND3N,    D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_NORMAL,       0 }, // XAVATAR_VERTEX_NORMAL
        { 0, 16, D3DDECLTYPE_UBYTE4N,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_BLENDWEIGHT,  0 }, // XAVATAR_VERTEX_WEIGHTS
        { 0, 20, D3DDECLTYPE_UBYTE4,    D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_BLENDINDICES, 0 }, // XAVATAR_VERTEX_BINDINGS
        { 0, 24, D3DDECLTYPE_D3DCOLOR,  D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR,        0 }, // XAVATAR_VERTEX_COLOR
        { 0, 28, D3DDECLTYPE_FLOAT16_2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD,     0 }, // XAVATAR_VERTEX_UV
        { 0, 32, D3DDECLTYPE_FLOAT16_2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD,     1 }, // XAVATAR_VERTEX_UV
        { 1,  0, D3DDECLTYPE_FLOAT4,    D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD,     6 }, // Joint buffer.
        { 1, 16, D3DDECLTYPE_FLOAT4,    D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD,     7 }, // Joint buffer.
        { 1, 32, D3DDECLTYPE_FLOAT4,    D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD,     8 }, // Joint buffer.
        D3DDECL_END()
    };

    IDirect3DDevice9& device = *m_pd3dDevice;

    HRESULT hr;

    // Create the head and body vertex declarations. We will add appropriate
    // references for each shader later.
    D3DVertexDeclaration *headDecl, *bodyDecl, *shinyDecl;
    if ( !SUCCEEDED( hr = device.CreateVertexDeclaration( bodyDeclDesc,  &bodyDecl  ) ) ||
         !SUCCEEDED( hr = device.CreateVertexDeclaration( headDeclDesc,  &headDecl  ) ) ||
         !SUCCEEDED( hr = device.CreateVertexDeclaration( shinyDeclDesc, &shinyDecl ) )  )
    {
        return E_FAIL;
    }

    // Create all vertex shaders. We will add appropriate references for 
    // each shader later.
    D3DVertexShader *bodyVs, *headOpaqueVs,*bodyShinyVs;
    ATG::LoadVertexShader( "game:\\media\\shaders\\body_vs.xvu", &bodyVs );
    ATG::LoadVertexShader( "game:\\media\\shaders\\head_opaque_vs.xvu", &headOpaqueVs );
    ATG::LoadVertexShader( "game:\\media\\shaders\\body_shiny_vs.xvu", &bodyShinyVs );

    // Create all pixel shaders. We will add appropriate references for each
    // shader later.
    D3DPixelShader *bodyOpaquePs, *headOpaquePs, *bodyTranspPs;
    D3DPixelShader *bodyShinyOpaquePs, *bodyShinyTranspPs;
    ATG::LoadPixelShader( "game:\\media\\shaders\\body_opaque_ps.xpu", &bodyOpaquePs );
    ATG::LoadPixelShader( "game:\\media\\shaders\\head_opaque_ps.xpu", &headOpaquePs );
    ATG::LoadPixelShader( "game:\\media\\shaders\\body_transparent_ps.xpu", &bodyTranspPs );
    ATG::LoadPixelShader( "game:\\media\\shaders\\body_shiny_opaque_ps.xpu", &bodyShinyOpaquePs );
    ATG::LoadPixelShader( "game:\\media\\shaders\\body_shiny_transparent_ps.xpu", &bodyShinyTranspPs );

    // Set declaration, vertex and pixel shaders for each material.
    m_D3dShaders[ XAVATAR_SHADER_BODY_OPAQUE            ].m_VertexDeclaration  = bodyDecl;
    m_D3dShaders[ XAVATAR_SHADER_BODY_OPAQUE            ].m_VertexShader       = bodyVs;
    m_D3dShaders[ XAVATAR_SHADER_BODY_OPAQUE            ].m_PixelShader        = bodyOpaquePs;

    m_D3dShaders[ XAVATAR_SHADER_BODY_TRANSPARENT       ].m_VertexDeclaration  = bodyDecl;
    m_D3dShaders[ XAVATAR_SHADER_BODY_TRANSPARENT       ].m_VertexShader       = bodyVs;
    m_D3dShaders[ XAVATAR_SHADER_BODY_TRANSPARENT       ].m_PixelShader        = bodyTranspPs;

    m_D3dShaders[ XAVATAR_SHADER_BODY_SHINY_OPAQUE      ].m_VertexDeclaration  = shinyDecl;
    m_D3dShaders[ XAVATAR_SHADER_BODY_SHINY_OPAQUE      ].m_VertexShader       = bodyShinyVs;
    m_D3dShaders[ XAVATAR_SHADER_BODY_SHINY_OPAQUE      ].m_PixelShader        = bodyShinyOpaquePs;

    m_D3dShaders[ XAVATAR_SHADER_BODY_SHINY_TRANSPARENT ].m_VertexDeclaration  = shinyDecl;
    m_D3dShaders[ XAVATAR_SHADER_BODY_SHINY_TRANSPARENT ].m_VertexShader       = bodyShinyVs;
    m_D3dShaders[ XAVATAR_SHADER_BODY_SHINY_TRANSPARENT ].m_PixelShader        = bodyShinyTranspPs;

    m_D3dShaders[ XAVATAR_SHADER_HEAD_OPAQUE            ].m_VertexDeclaration  = headDecl;
    m_D3dShaders[ XAVATAR_SHADER_HEAD_OPAQUE            ].m_VertexShader       = headOpaqueVs;
    m_D3dShaders[ XAVATAR_SHADER_HEAD_OPAQUE            ].m_PixelShader        = headOpaquePs;
 
    // Add appropriate references to each declaration and shader.
    for ( int s = 0; s < XAVATAR_SHADER_COUNT; ++s )
    {
        m_D3dShaders[ s ].m_VertexDeclaration ->AddRef();
        m_D3dShaders[ s ].m_VertexShader      ->AddRef();
        m_D3dShaders[ s ].m_PixelShader       ->AddRef();
    }

    // Set light color. 
    m_vLightColor    [ 0 ] = XMVectorSet( +0.200f, +0.100f, +0.050f, 1.0f );
    m_vLightColor    [ 1 ] = XMVectorSet( +0.100f, +0.100f, +0.200f, 1.0f );
    m_vLightColor    [ 2 ] = XMVectorSet( +0.400f, +0.400f, +0.400f, 1.0f );

    // Set light direction. 
    m_vLightDirection[ 0 ] = XMVectorSet( +0.342f, +0.000f, -0.940f, 1.0f );
    m_vLightDirection[ 1 ] = XMVectorSet( +1.000f, +0.000f, +0.000f, 1.0f );
    m_vLightDirection[ 2 ] = XMVectorSet( -0.500f, -0.612f, -0.612f, 1.0f );

    // Clear the shader texture index and constant register maps to invalid
    // values so we will know immediately if we get an invalid value.
    FillMemory( m_ShaderTextureIndexMap,     sizeof( m_ShaderTextureIndexMap     ), 0xff );
    FillMemory( m_PixelConstantRegisterMap,  sizeof( m_PixelConstantRegisterMap  ), 0xff );
    FillMemory( m_VertexConstantRegisterMap, sizeof( m_VertexConstantRegisterMap ), 0xff );

    // Set up mappings for texture usages to the appropriate shader index.
    // We only provide mappings for textures that have no UVs. Textures with
    // UVs will automatically be mapped to the corresponding texture index.
    m_ShaderTextureIndexMap[ XAVATAR_SHADER_PARAM_USAGE_TEXTURE_REFLECTION ][ XAVATAR_SHADER_BODY_SHINY_OPAQUE      ] = 2;
    m_ShaderTextureIndexMap[ XAVATAR_SHADER_PARAM_USAGE_TEXTURE_REFLECTION ][ XAVATAR_SHADER_BODY_SHINY_TRANSPARENT ] = 2;

    // Set up mappings for all pixel constants to the appropriate register.
    m_PixelConstantRegisterMap[ XAVATAR_SHADER_PARAM_USAGE_PIXEL_CONSTANT_COLOR_CUSTOM_0        ] = 210;
    m_PixelConstantRegisterMap[ XAVATAR_SHADER_PARAM_USAGE_PIXEL_CONSTANT_COLOR_CUSTOM_1        ] = 211;
    m_PixelConstantRegisterMap[ XAVATAR_SHADER_PARAM_USAGE_PIXEL_CONSTANT_COLOR_CUSTOM_2        ] = 212;
    m_PixelConstantRegisterMap[ XAVATAR_SHADER_PARAM_USAGE_PIXEL_CONSTANT_TRANSPARENCY          ] = 213;
    m_PixelConstantRegisterMap[ XAVATAR_SHADER_PARAM_USAGE_PIXEL_CONSTANT_REFLECTIVITY          ] = 214;
    m_PixelConstantRegisterMap[ XAVATAR_SHADER_PARAM_USAGE_PIXEL_CONSTANT_COLOR_SKIN            ] = 215;
    m_PixelConstantRegisterMap[ XAVATAR_SHADER_PARAM_USAGE_PIXEL_CONSTANT_COLOR_HAIR            ] = 216;
    m_PixelConstantRegisterMap[ XAVATAR_SHADER_PARAM_USAGE_PIXEL_CONSTANT_COLOR_MOUTH           ] = 217;
    m_PixelConstantRegisterMap[ XAVATAR_SHADER_PARAM_USAGE_PIXEL_CONSTANT_COLOR_IRIS            ] = 218;
    m_PixelConstantRegisterMap[ XAVATAR_SHADER_PARAM_USAGE_PIXEL_CONSTANT_COLOR_EYEBROW         ] = 219;
    m_PixelConstantRegisterMap[ XAVATAR_SHADER_PARAM_USAGE_PIXEL_CONSTANT_COLOR_EYE_SHADOW      ] = 220;    
    m_PixelConstantRegisterMap[ XAVATAR_SHADER_PARAM_USAGE_PIXEL_CONSTANT_COLOR_FACIAL_HAIR     ] = 221;    
    m_PixelConstantRegisterMap[ XAVATAR_SHADER_PARAM_USAGE_PIXEL_CONSTANT_COLOR_SKIN_FEATURE_1  ] = 222;
    m_PixelConstantRegisterMap[ XAVATAR_SHADER_PARAM_USAGE_PIXEL_CONSTANT_COLOR_SKIN_FEATURE_2  ] = 223;
    m_PixelConstantRegisterMap[ XAVATAR_SHADER_PARAM_USAGE_PIXEL_CONSTANT_COLOR_RIMLIGHT        ] = 5;

    // We can now remove the local reference to each declaration and shader.
    SAFE_RELEASE( headDecl );
    SAFE_RELEASE( bodyDecl );
    SAFE_RELEASE( shinyDecl );
    SAFE_RELEASE( bodyVs );
    SAFE_RELEASE( headOpaqueVs );
    SAFE_RELEASE( bodyOpaquePs );
    SAFE_RELEASE( headOpaquePs );
    SAFE_RELEASE( bodyTranspPs );
    SAFE_RELEASE( bodyShinyOpaquePs );
    SAFE_RELEASE( bodyShinyTranspPs );
    SAFE_RELEASE( bodyShinyVs );
    
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
        UserAvatarData& avatarData = (**iter);
        DestroyAvatar( avatarData );
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

    avatarData.m_pAnim = UserAvatarData::s_pAnimations[3];
    avatarData.m_pAnim->InitializeCursor( 1.0f, XAVATAR_ANIMATION_PLAYMODE_LOOP, XAVATAR_MOTION_MEASUREMODE_ABSOLUTE, &avatarData.m_Cursor );

    RETURN_ON_FAIL( LoadLocalUserAvatarMetadata( avatarData ) );
    RETURN_ON_FAIL( LoadAvatar( avatarData ) );

    m_vAvatarData.push_back( &avatarData );

    if( bIncludingFriends )
    {
        XUSER_SIGNIN_STATE signinState = XUserGetSigninState( 0 );
        if( signinState == eXUserSigninState_SignedInToLive )
        {
            m_bFriendsPresenceDataReady = FALSE;
            RetrieveFriendsPresenceData( 0, m_Friends, _countof( m_Friends ), NULL, &m_FriendsOverlapped, &m_hFriendsEnum ); 
        }
    }
    
    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: CreateAvatar()
// Desc: Create the avatar using XAvatarGetAssets
//--------------------------------------------------------------------------------------
HRESULT Sample::CreateAvatar( UserAvatarData& avatarData )
{
    DestroyAvatar( avatarData );

    // Set all pose joints to identity and all texture layers to frame zero on construction. 
    XAVATAR_SKELETON_POSE_JOINT jointIdentity;
    jointIdentity.Position = XMVectorSet( 0.0f, 0.0f, 0.0f, 1.0f );
    jointIdentity.Rotation = XMQuaternionIdentity();
    jointIdentity.Scale    = XMVectorSet( 1.0f, 1.0f, 1.0f, 1.0f );
    for ( int j = 0; j < XAVATAR_MAX_SKELETON_JOINTS; ++j )
    {
        avatarData.m_AvatarJointPose[ j ] = jointIdentity;
    }
    ZeroMemory( avatarData.m_dwTextureLayers, sizeof( avatarData.m_dwTextureLayers ) );

    // Retrieve the required CPU and GPU buffer sizes. If we cannot get
    // these sizes then the initialization fails.
    DWORD cpuBufferSize, gpuBufferSize;
    RETURN_ON_FAIL( XAvatarGetAssetsResultSize( XAVATAR_COMPONENT_MASK_ALL, &cpuBufferSize, &gpuBufferSize ) );
    
    // Calculate the joint buffer size required per joint buffer, and then
    // add this to the overall GPU allocation size.
    avatarData.m_dwMipMapBufferOffset         = RoundUpToAlign( gpuBufferSize, GPU_MEMORY_BUFFER_ALIGN );
    const DWORD jointBufferSize    = XAVATAR_MAX_SKELETON_JOINTS * 12 * sizeof( FLOAT );
    const DWORD gpuAllocSize       = avatarData.m_dwMipMapBufferOffset + MIPMAP_BUFFER_SIZE;
    
    // Attempt to allocate the CPU and GPU buffers with the appropriate 
    // alignments. We add a little extra to the GPU buffer for joints.
    avatarData.m_pAssets = reinterpret_cast<XAVATAR_ASSETS*>( XMemAlloc( cpuBufferSize, MAKE_XALLOC_ATTRIBUTES( 0, TRUE, TRUE, FALSE, SAMPLE_ALLOCATOR_ID, XALLOC_ALIGNMENT_4, XALLOC_MEMPROTECT_READWRITE, FALSE, XALLOC_MEMTYPE_HEAP ) ) );
    avatarData.m_pGpuBuffer = reinterpret_cast<BYTE*>( XPhysicalAlloc( gpuAllocSize, MAXULONG_PTR, GPU_MEMORY_BUFFER_ALIGN, PAGE_READWRITE | PAGE_WRITECOMBINE ) );
    avatarData.m_pJointBuffer = reinterpret_cast<BYTE*>( XPhysicalAlloc( jointBufferSize * JOINT_BUFFER_COUNT, MAXULONG_PTR, 4, PAGE_READWRITE | PAGE_WRITECOMBINE ) );

    // If we failed to allocate memory then fail.
    if ( !avatarData.m_pAssets || !avatarData.m_pGpuBuffer || !avatarData.m_pJointBuffer )
    {
        return E_OUTOFMEMORY;
    }

    // Construct the joint vertex buffers in place in the GPU buffer.
    for ( int jbi =  0; jbi < JOINT_BUFFER_COUNT; ++jbi )
    {
        XGSetVertexBufferHeader( jointBufferSize, 0, 0, 0, &avatarData.m_JointBufferVBs[ jbi ] );
        XGOffsetResourceAddress( &avatarData.m_JointBufferVBs[ jbi ], &avatarData.m_pJointBuffer[ jbi * jointBufferSize ] );
    }

    avatarData.m_dwJointBufferIndex = 0;

    // Get the avatar assets
    if ( ERROR_IO_PENDING != XAvatarGetAssets( &avatarData.m_metadata, XAVATAR_COMPONENT_MASK_ALL, 0, cpuBufferSize, avatarData.m_pAssets, gpuAllocSize, avatarData.m_pGpuBuffer, &avatarData.m_GetAssetsOverlapped ) )
    {
        return E_FAIL;
    }

    return S_OK;
}

//--------------------------------------------------------------------------------------
// Name: DestroyAvatar()
// Desc: Destroy the avatar and release memory
//--------------------------------------------------------------------------------------
VOID Sample::DestroyAvatar( UserAvatarData& avatarData )
{
    avatarData.m_bGeometryReady = FALSE;
    avatarData.m_bTexturesReady = FALSE;

    if( avatarData.m_pAssets || avatarData.m_pGpuBuffer )
    {
        while( !XHasOverlappedIoCompleted( &avatarData.m_GetAssetsOverlapped ) );
        while( !XHasOverlappedIoCompleted( &avatarData.m_MipMapOverlapped ) );
    }

    if( avatarData.m_pAssets )
    {
        XMemFree( avatarData.m_pAssets, MAKE_XALLOC_ATTRIBUTES( 0, 
                                                                TRUE, 
                                                                TRUE, 
                                                                 FALSE, 
                                                                 SAMPLE_ALLOCATOR_ID, 
                                                                 XALLOC_ALIGNMENT_4, 
                                                                 XALLOC_MEMPROTECT_READWRITE, 
                                                                 FALSE, XALLOC_MEMTYPE_HEAP ) );

        avatarData.m_pAssets = NULL;
    }

    if( avatarData.m_pGpuBuffer )
    {
        XPhysicalFree( avatarData.m_pGpuBuffer );
        avatarData.m_pGpuBuffer = NULL;
    }

    if( avatarData.m_pJointBuffer )
    {
        XPhysicalFree( avatarData.m_pJointBuffer );
        avatarData.m_pJointBuffer = NULL;
    }

}

//--------------------------------------------------------------------------------------
// Name: CompleteIO()
// Desc: Called once XAvatarGetAssets has completed.  This function sets up vertex and
//       index buffers and from the data return from XAvatarGetAssets and calls into the
//       Avatar libraries to generate mipmaps.
//--------------------------------------------------------------------------------------
HRESULT Sample::CompleteIO( UserAvatarData& avatarData )
{

    // Generate mipmaps asynchronously
    BYTE* pMipMapBuffer = avatarData.m_pGpuBuffer + avatarData.m_dwMipMapBufferOffset;
    if( ERROR_IO_PENDING != XAvatarGenerateMipMaps( avatarData.m_pAssets, 0, MIPMAP_BUFFER_SIZE, pMipMapBuffer, &avatarData.m_MipMapOverlapped ) )
    {
        return E_FAIL;
    }

    // Populate vertex buffers, index buffers, and texture headers for all component models.
    for ( DWORD cmi = 0; cmi < avatarData.m_pAssets->ComponentCount; ++cmi )
    {
        // Reference this component model.
        const XAVATAR_MODEL* model = &avatarData.m_pAssets->pComponentModels[ cmi ];

        // Skip models with no geometry
        if ( model->GlobalIndexBufferSize == 0 )
        {
            continue;
        }

        // Construct the vertex buffer in place, pointing at the global
        // vertex buffer for this model.
        XGSetVertexBufferHeader( model->GlobalVertexBufferSize, 0, 0, 0, &avatarData.m_VertexBuffers[ cmi ] );
        XGOffsetResourceAddress( &avatarData.m_VertexBuffers[ cmi ], model->pGlobalVertexBuffer );

        // Construct the index buffer in place, pointing at the global
        // index buffer for this model.
        XGSetIndexBufferHeader ( model->GlobalIndexBufferSize, 0, D3DFMT_INDEX16, 0, 0, &avatarData.m_IndexBuffers[ cmi ] );
        XGOffsetResourceAddress( &avatarData.m_IndexBuffers[ cmi ], model->pGlobalIndexBuffer );

    }

    avatarData.m_bGeometryReady = TRUE;

    return S_OK;
}

//--------------------------------------------------------------------------------------
// Name: SetupTextures()
// Desc: Sets up texture headers after the texture assets have been created.
//--------------------------------------------------------------------------------------
HRESULT Sample::SetupTextures( UserAvatarData& avatarData )
{
    // Populate vertex buffers, index buffers, and texture headers for all component models.
    for ( DWORD cmi = 0; cmi < avatarData.m_pAssets->ComponentCount; ++cmi )
    {
        // Reference this component model.
        const XAVATAR_MODEL* model = &avatarData.m_pAssets->pComponentModels[ cmi ];

        // Skip models with no geometry
        if ( model->GlobalIndexBufferSize == 0 )
        {
            continue;
        }

        // Construct textures.
        for ( DWORD ti = 0; ti < model->TextureCount; ++ti )
        {
            // Reference this texture.
            const XAVATAR_TEXTURE* texture = &model->pTextures[ ti ];

            // Iterate over all layers of this texture, creating the D3D
            // textures that reference that layer data.
            for ( DWORD li = 0; li < texture->LayerCount; ++li )
            {
                // Construct the layer texture, pointing at the
                // layer data for this texture and layer.
                XGSetTextureHeader( texture->Width,
                                    texture->Height, 
                                    texture->MipLevels, 
                                    0, texture->Format, 0, 
                                    ( UINT )( texture->pBaseData + li * texture->BaseSize ), 
                                    ( UINT )( texture->pMipData + li * texture->MipSize ), 
                                    0, &avatarData.m_Textures[ cmi ][ ti ][ li ], 0, 0 );
            }
        }
    }
    
    avatarData.m_bTexturesReady = TRUE;

    return S_OK;
}

//--------------------------------------------------------------------------------------
// Name: LoadAvatar()
// Desc: Load the Avatar
//--------------------------------------------------------------------------------------
HRESULT Sample::LoadAvatar( UserAvatarData& avatarData )
{
    avatarData.m_bGeometryReady = FALSE;
    avatarData.m_bTexturesReady = FALSE;

    ZeroMemory( &avatarData.m_GetAssetsOverlapped, sizeof(XOVERLAPPED) );
    ZeroMemory( &avatarData.m_MipMapOverlapped, sizeof(XOVERLAPPED) );

    // Create the avatar with the XAvatarGetAssets API
    RETURN_ON_FAIL( CreateAvatar( avatarData ) );

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
// Name: Update()
// Desc: Called once per frame.  Entry point for animating the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Update()
{
    PIXBeginNamedEvent( 0, __FUNCTION__ );

    for( size_t i = 0; i < m_vAvatarData.size(); ++i )
    {
        UserAvatarData& avatarData = *m_vAvatarData.at( i );

        // For each Avatar we're managing, check if we've retrieved all geometry
        // data. If we have, call CompleteIO to set up vertex and index buffers
        // for rendering the Avatar.
        if( !avatarData.m_bGeometryReady && XHasOverlappedIoCompleted( &avatarData.m_GetAssetsOverlapped ) )
        {
            CompleteIO( avatarData );
        }

        if( avatarData.m_bGeometryReady && !avatarData.m_bTexturesReady && XHasOverlappedIoCompleted( &avatarData.m_MipMapOverlapped ) )
        {
            SetupTextures( avatarData );    
        }
    }

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

    // Play an animation for the local user's Avatar when a button is pressed
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_A )
    {
        localUserAvatarData.m_pAnim = UserAvatarData::s_pAnimations[0];
        localUserAvatarData.m_pAnim->InitializeCursor( 1.0f, XAVATAR_ANIMATION_PLAYMODE_LOOP, XAVATAR_MOTION_MEASUREMODE_ABSOLUTE, &localUserAvatarData.m_Cursor );
    }

    if(pGamepad->wPressedButtons & XINPUT_GAMEPAD_B )
    {
        localUserAvatarData.m_pAnim = UserAvatarData::s_pAnimations[1];
        localUserAvatarData.m_pAnim->InitializeCursor( 1.0f, XAVATAR_ANIMATION_PLAYMODE_LOOP, XAVATAR_MOTION_MEASUREMODE_ABSOLUTE, &localUserAvatarData.m_Cursor );
    }

    if(pGamepad->wPressedButtons & XINPUT_GAMEPAD_X )
    {
        localUserAvatarData.m_pAnim = UserAvatarData::s_pAnimations[2];
        localUserAvatarData.m_pAnim->InitializeCursor( 1.0f, XAVATAR_ANIMATION_PLAYMODE_LOOP, XAVATAR_MOTION_MEASUREMODE_ABSOLUTE, &localUserAvatarData.m_Cursor );
    }

    if(pGamepad->wPressedButtons & XINPUT_GAMEPAD_Y )
    {
        localUserAvatarData.m_pAnim = UserAvatarData::s_pAnimations[3];
        localUserAvatarData.m_pAnim->InitializeCursor( 1.0f, XAVATAR_ANIMATION_PLAYMODE_LOOP, XAVATAR_MOTION_MEASUREMODE_ABSOLUTE, &localUserAvatarData.m_Cursor );
    }

    // Force random Avatar to be generated for the local user
    if(pGamepad->wPressedButtons & XINPUT_GAMEPAD_START )
    {
        ReloadLocalUserAndFriendsAvatarData( FALSE );
    }

    // Update all Avatar animations and rebuild joints
    for( AvatarDataIter iter = m_vAvatarData.begin(); iter != m_vAvatarData.end(); ++iter )
    {
        UserAvatarData& avatarData = (**iter);

        RETURN_ON_FAIL( UpdateAnimation( fDeltaTime, avatarData ) );
        RETURN_ON_FAIL( RebuildJoints( avatarData ) );
    }    


    PIXEndNamedEvent();


    //
    // The rest requires the local user to be signed into LIVE
    XUSER_SIGNIN_STATE state = XUserGetSigninState( 0 );
    
    const BOOL bIsSignedIntoLive = ( state == eXUserSigninState_SignedInToLive );

    // Friends presence data ready?
    if( bIsSignedIntoLive && 
        localUserAvatarData.m_bGeometryReady &&
        localUserAvatarData.m_bTexturesReady && 
        !m_bFriendsPresenceDataReady         && 
        XHasOverlappedIoCompleted( &m_FriendsOverlapped ) )
    {
        m_bFriendsPresenceDataReady = TRUE;

        // For asynchronous calls to XFriendsCreateEnumerator, the number of 
        // friend items enumerated is returned in the InternalHigh member
        // of our overlapped
        m_dwFriendsCount = m_FriendsOverlapped.InternalHigh;

        CloseHandle( m_hFriendsEnum );
        m_hFriendsEnum = INVALID_HANDLE_VALUE;

        // Retrieve friends Avatar metadata
        m_bFriendsAvatarMetadataReady = TRUE;
        RetrieveFriendsAvatarMetadata( 0, 
                                       m_Friends, 
                                       m_dwFriendsCount, 
                                       m_FriendsAvatarMetadata, 
                                       &m_pSettingResults, 
                                       NULL );
    }
    else if( bIsSignedIntoLive && 
             m_bFriendsPresenceDataReady && 
             m_dwFriendsCount && 
             !m_bFriendsAvatarMetadataReady && 
             XHasOverlappedIoCompleted( &m_FriendsOverlapped ) )
    {
        // Friends Avatar metadata data ready? If so, create our friends' Avatar instances from it
        //        
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
            
            avatarData.m_xuid  = m_Friends[i].xuid;
            avatarData.m_pAnim = UserAvatarData::s_pAnimations[3];

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
// Name: UpdateAnimation()
// Desc: Updates the animation stream and blends between the previous and next frames.
//       Called once per frame.
//--------------------------------------------------------------------------------------
HRESULT Sample::UpdateAnimation( FLOAT fDeltaTime, UserAvatarData& avatarData )
{
    if( !avatarData.m_pAnim )
        return E_FAIL;

    FLOAT fLength = 1.0f;
    DWORD nJointCount, nCarryableJointCount, nMotionDataCount, nTextureCount;
    
    avatarData.m_pAnim->IncrementCursor( fDeltaTime, &avatarData.m_Cursor );

    // Get the pose and texture layers
    avatarData.m_pAnim->GetAttributes( &fLength, &nJointCount, &nCarryableJointCount, &nMotionDataCount, &nTextureCount);
    avatarData.m_pAnim->GetPose( &avatarData.m_Cursor, 1.0f, nJointCount, avatarData.m_AvatarJointPose, 0, NULL, 0, NULL, nTextureCount, avatarData.m_dwTextureLayers );    

    // Return success
    return S_OK;
}

//--------------------------------------------------------------------------------------
// Name: RebuildJoints()
// Desc: Rebuilds the joints from the current pose. 
//--------------------------------------------------------------------------------------
HRESULT Sample::RebuildJoints( UserAvatarData& avatarData )
{
    if( !avatarData.m_bGeometryReady )
        return S_OK;

    // Intermediate storage for the joint transforms and 12-float packed
    // transposes, which is how we present the data to the vertex shader.    
    static XMMATRIX  matJointTransforms[ XAVATAR_MAX_SKELETON_JOINTS ];
    static XMFLOAT4A transposes[ XAVATAR_MAX_SKELETON_JOINTS * 3 ];
    
    // First, build joint transforms in world-space.
    for ( DWORD j = 0; j < avatarData.m_pAssets->pSkeleton->Count; ++j )
    {
        // Reference the relevant components for building this joint.
        const XAVATAR_SKELETON_POSE_JOINT&     offset    = avatarData.m_AvatarJointPose[ j ];
        const XAVATAR_SKELETON_JOINT&          skelJoint = avatarData.m_pAssets->pSkeleton->pJoints[ j ];
        const XAVATAR_SKELETON_POSE_JOINT&     bindPose  = skelJoint.BindPose.Local;
        const XAVATAR_SKELETON_HIERARCHY_JOINT hierarchy = skelJoint.Hierarchy;

        // Make sure that the parent has already been processed
        assert( j == 0 || hierarchy.Parent < j );

        // Generate the bindpose transform for this joint.
        XMMATRIX bindPoseT;
        bindPoseT = XMMatrixRotationQuaternion( bindPose.Rotation );
        bindPoseT = XMMatrixMultiply( bindPoseT, XMMatrixScalingFromVector( bindPose.Scale ) );
        bindPoseT.r[ 3 ] = bindPose.Position;

        // Generate the offset transform for this joint.
        XMMATRIX offsetT;
        offsetT = XMMatrixRotationQuaternion( offset.Rotation );
        offsetT = XMMatrixMultiply( offsetT, XMMatrixScalingFromVector( offset.Scale ) );
        offsetT.r[ 3 ] = offset.Position;

        // Retrieve the parent transform, or the basis for joint zero.
        XMMATRIX parentT = ( j ? matJointTransforms[ hierarchy.Parent ] : XMMatrixIdentity() );

        // Generate the joint transform by applying the bindpose to the 
        // offset, and then transforming by the parent transform.
        matJointTransforms[ j ] = XMMatrixMultiply( XMMatrixMultiply( offsetT, bindPoseT ), parentT );
    }

    // Now pre-multiply these joint transforms by the inverse bind-pose 
    // transform for that joint. This gives us a final transform that 
    // converts model-space vertices into skinned vertices in one multiply.
    // We take the transposes of these transforms and pack them into 12
    // floats for presentation to the vertex shader.
    for ( DWORD j = 0; j < avatarData.m_pAssets->pSkeleton->Count; ++j )
    {
        // Reference the bindpose data for this joint.
        const XAVATAR_SKELETON_BINDPOSE_JOINT& bindPose = avatarData.m_pAssets->pSkeleton->pJoints[ j ].BindPose;

        // Generate the bindpose transform for this joint, and preapply
        // it to the joint transform to generate the final transform.
        XMVECTOR determinant;
        XMMATRIX invBindPoseT;
        invBindPoseT = XMMatrixRotationQuaternion( bindPose.Rotation );
        invBindPoseT.r[ 3 ] = bindPose.Position;
        invBindPoseT = XMMatrixInverse( &determinant, invBindPoseT );
        XMMATRIX transform = XMMatrixMultiply ( invBindPoseT, matJointTransforms[ j ] );

        // Take the transpose and pack it into 12 floats, which is the
        // required format for our vertex shader.
        XMMATRIX transpose = XMMatrixTranspose( transform );
        XMStoreFloat4A( &transposes[ j * 3 + 0 ], transpose.r[ 0 ] );
        XMStoreFloat4A( &transposes[ j * 3 + 1 ], transpose.r[ 1 ] );
        XMStoreFloat4A( &transposes[ j * 3 + 2 ], transpose.r[ 2 ] );
    }

    // We now loop until we get an exclusive lock on the current joint
    // buffer. We only increment the buffer while we hold the lock.
    VOID*            pCurrJointData;
    D3DVertexBuffer* pCurrJointBuffer;
    for ( ; ; )
    {
        // Select the next joint buffer, into which we copy the data.
        DWORD dwCurrBufferIndex = avatarData.m_dwJointBufferIndex;
        DWORD dwNextBufferIndex = ( dwCurrBufferIndex + 1 ) % JOINT_BUFFER_COUNT;
        pCurrJointBuffer       = &avatarData.m_JointBufferVBs[ dwNextBufferIndex ];

        // Warn on joint buffer contention
        #ifdef _DEBUG
        {
            // If the current joint buffer is still busy, give
            // a warning about contention for the buffer.
            if( pCurrJointBuffer->IsBusy( ) )
            {
                // Regulates how often warnings like this appear.
                static const FLOAT JOINT_BUFFER_SET_WARN_INTERVAL_S = 10.0f;

                // Ensure we use a constant tick count throughout this
                // block.
                DWORD tickCount = GetTickCount();

                // Start out "last warning" time as double what we need
                // for a warning to spew
                static DWORD tickCountAtLastWarning = tickCount - (DWORD)(JOINT_BUFFER_SET_WARN_INTERVAL_S * 1000.0f * 2.0f);

                // Short-hand for the time that has passed (in seconds) 
                // since last warning was issued.
                FLOAT timeSinceLastWarning = (FLOAT)(tickCount - tickCountAtLastWarning) / 1000.0f;

                // If we haven't spewed out warning for a while, do so.
                if( timeSinceLastWarning > JOINT_BUFFER_SET_WARN_INTERVAL_S )
                {
                    ATG::DebugSpew( "WARNING: Avatar joint buffer is still in use by the device during update. Increase the joint buffer count." );
                    tickCountAtLastWarning = tickCount;
                }            
            }
        }
        #endif // _DEBUG

        // Lock the buffer ready to copy in the transposed transforms.
        if ( !SUCCEEDED( pCurrJointBuffer->Lock( 0, 0, &pCurrJointData, 0 ) ) )
        {
            return E_FAIL;
        }

        if ( avatarData.m_dwJointBufferIndex == dwCurrBufferIndex )
        {
            avatarData.m_dwJointBufferIndex = dwNextBufferIndex;
            break;
        }
    }

    // Copy the 12-float packed, transposed joint transforms into the
    // joint buffer, ready to be presented to the vertex shader.
    XMemCpyStreaming_WriteCombined( pCurrJointData, transposes, sizeof( transposes ) );

    // Unlock the joint buffer.
    if ( !SUCCEEDED( pCurrJointBuffer->Unlock() ) )
    {
        return E_FAIL;
    }
    
    // Return success.
    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Render()
// Desc: Renders the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Render()
{
    PIXBeginNamedEvent( 0, __FUNCTION__ );

    // Render scene
    const D3DVECTOR4 clearColor = { 0.75f, 0.78f, 0.86f, 1.0f };
    m_pd3dDevice->SetRenderTarget( 0, m_pBackBuffer );
    m_pd3dDevice->SetDepthStencilSurface( m_pDepthBuffer );
    m_pd3dDevice->BeginTiling( 0, ARRAYSIZE(g_tiles), g_tiles, &clearColor, 1, 0 );

    PIXBeginNamedEvent( 0xFFFFFFFF, "Avatar Render" );
    for( size_t i = 0; i < m_vAvatarData.size(); ++i )
    {
        UserAvatarData& avatarData = *m_vAvatarData.at( i );

        RenderAvatar( avatarData );
        
        if( avatarData.m_bRenderWaitingEffect  )
        {
            HRESULT status = XAvatarWaitingEffectRender( &avatarData.m_WaitingEffect, m_pd3dDevice, avatarData.m_matWorld, m_matView, m_matProj );
            
            // status will be E_PENDING if the waiting effect is fading in or out, and S_OK otherwise.

            // Stop rendering the waiting effect once it has finished fading out
            if( status == S_OK && avatarData.m_bWaitingEffectFadeOut )
            {
                avatarData.m_bRenderWaitingEffect = TRUE;
            }

            // If the avatar is ready and the waiting effect has finished fading in, stop the effect
            if( avatarData.m_bGeometryReady && avatarData.m_bTexturesReady && status == S_OK )
            {
                if( XAvatarWaitingEffectStop( &avatarData.m_WaitingEffect ) == S_OK )
                {
                    // Note that the waiting effect is still fading out, so we need to keep rendering it
                    avatarData.m_bWaitingEffectFadeOut = TRUE;
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
// Name: RenderAvatar()
// Desc: Renders the avatar.
//--------------------------------------------------------------------------------------
VOID Sample::RenderAvatar( const UserAvatarData& avatarData )
{
    if( !avatarData.m_bGeometryReady || !avatarData.m_bTexturesReady )
        return;

    // Set up device state and shader constants
    PreRender( &avatarData.m_matWorld, &m_matView, &m_matProj );

    // Iterate over all component models and render opaque materials.
    for ( DWORD cmi = 0; cmi < avatarData.m_pAssets->ComponentCount; ++cmi )
    {
        RenderOpaque( avatarData,
                      cmi );
    }

    // Iterate over all component models and render transparent materials.
    for ( DWORD cmi = 0; cmi < avatarData.m_pAssets->ComponentCount; ++cmi )
    {
        RenderTransparent( avatarData,
                           cmi );
    }

    // End the render pass
    PostRender( );
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
            m_Font.DrawText( 0, 0, 0xffffffff, L"AvatarGetAssets" );

            m_Font.SetScaleFactors( 1.0f, 1.0f );
            m_Font.DrawText( 0, 0, 0xffffff00, m_Timer.GetFrameRate(), ATGFONT_RIGHT );

            m_Font.End();
        }
    
    PIXEndNamedEvent();
}

//--------------------------------------------------------------------------------------
// Name: PreRender()
// Desc: Begin the rendering pass. This should be invoked before the
//       RenderOpaque() and RenderTransparent() functions, and
//       matched with a call to PostRender() once the pass is complete.
//--------------------------------------------------------------------------------------
HRESULT Sample::PreRender(  const XMMATRIX*     modelTransform,
                            const XMMATRIX*     viewTransform,
                            const XMMATRIX*     projectionTransform )
{
    XMMATRIX modelViewTransform = *modelTransform * *viewTransform;

    m_pd3dDevice->SetVertexShaderConstantF( 0, (FLOAT*)&modelViewTransform, 4 );
    m_pd3dDevice->SetVertexShaderConstantF( 4, (FLOAT*)projectionTransform, 4 );

    // set lights
    FLOAT ambientColor [ 4 ] = { 0.55f, 0.55f, 0.550f, 1.0f };

    XMVECTOR lightDirection[ 3 ];
    lightDirection[ 0 ] = XMVector3Normalize( m_vLightDirection[ 0 ] );
    lightDirection[ 1 ] = XMVector3Normalize( m_vLightDirection[ 1 ] );
    lightDirection[ 2 ] = XMVector3Normalize( XMVector3TransformNormal( XMVectorScale( m_vLightDirection[ 2 ], -1.0f ), *viewTransform ) );

    m_pd3dDevice->SetPixelShaderConstantF(   0, &m_vLightColor [ 2 ].x, 1 );
    m_pd3dDevice->SetPixelShaderConstantF(   1, &lightDirection[ 2 ].x, 1 );
    m_pd3dDevice->SetPixelShaderConstantF(   2, &m_vLightColor [ 0 ].x, 1 );
    m_pd3dDevice->SetPixelShaderConstantF(   3, &lightDirection[ 0 ].x, 1 );
    m_pd3dDevice->SetPixelShaderConstantF( 100, &m_vLightColor [ 1 ].x, 1 );
    m_pd3dDevice->SetPixelShaderConstantF( 101, &lightDirection[ 1 ].x, 1 );
    m_pd3dDevice->SetPixelShaderConstantF(   4, ambientColor,           1 );

    m_pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_CW );

    m_pd3dDevice->SetSamplerState_Inline( 0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetSamplerState_Inline( 0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetSamplerState_Inline( 0, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetSamplerState_Inline( 1, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetSamplerState_Inline( 1, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetSamplerState_Inline( 1, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetSamplerState_Inline( 2, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetSamplerState_Inline( 2, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetSamplerState_Inline( 2, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetSamplerState_Inline( 3, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetSamplerState_Inline( 3, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetSamplerState_Inline( 3, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetSamplerState_Inline( 4, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetSamplerState_Inline( 4, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetSamplerState_Inline( 4, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetSamplerState_Inline( 5, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetSamplerState_Inline( 5, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetSamplerState_Inline( 5, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetSamplerState_Inline( 6, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetSamplerState_Inline( 6, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetSamplerState_Inline( 6, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetSamplerState_Inline( 7, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetSamplerState_Inline( 7, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetSamplerState_Inline( 7, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetSamplerState_Inline( 8, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetSamplerState_Inline( 8, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetSamplerState_Inline( 8, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: RenderOpaque()
// Desc: Render an opaque model. Make sure to call the PreRender()
//       function to set up the render state appropriately for the pass.
//--------------------------------------------------------------------------------------
HRESULT Sample::RenderOpaque( const UserAvatarData&     avatarData,
                              DWORD                     dwModelIndex )
{
    const XAVATAR_MODEL*      pModel        = &avatarData.m_pAssets->pComponentModels[ dwModelIndex ];
    const D3DVertexBuffer*    pVertexBuffer = &avatarData.m_VertexBuffers[ dwModelIndex ];
    const D3DIndexBuffer*     pIndexBuffer  = &avatarData.m_IndexBuffers[ dwModelIndex ];
    const D3DVertexBuffer*    pJointBuffer  = &avatarData.m_JointBufferVBs[ avatarData.m_dwJointBufferIndex ];

    // If the model is empty then we have nothing to do here.
    if ( !pModel->GlobalIndexBufferSize )
    {
        return S_OK;
    }

    // Defines the shaders we consider when performing the opaque pass.
    static const XAVATAR_SHADER OPAQUE_SHADERS[] = 
    { 
        XAVATAR_SHADER_BODY_SHINY_OPAQUE,
        XAVATAR_SHADER_BODY_OPAQUE, 
        XAVATAR_SHADER_HEAD_OPAQUE,
    };

    // Set up the render state for the opaque render pass.
    m_pd3dDevice->SetRenderState_Inline( D3DRS_ALPHATESTENABLE,  TRUE                );
    m_pd3dDevice->SetRenderState_Inline( D3DRS_ALPHAFUNC,        D3DCMP_GREATEREQUAL );
    m_pd3dDevice->SetRenderState_Inline( D3DRS_ALPHAREF,         0x80                );
    m_pd3dDevice->SetRenderState_Inline( D3DRS_ZENABLE,          TRUE                );
    m_pd3dDevice->SetRenderState_Inline( D3DRS_ZWRITEENABLE,     TRUE                );

    D3DBLENDSTATE blendState;
    blendState.SrcBlend       = D3DBLEND_ONE;
    blendState.BlendOp        = D3DBLENDOP_ADD;
    blendState.DestBlend      = D3DBLEND_ZERO;
    blendState.SrcBlendAlpha  = D3DBLEND_ONE;
    blendState.BlendOpAlpha   = D3DBLENDOP_ADD;
    blendState.DestBlendAlpha = D3DBLEND_ZERO;
    m_pd3dDevice->SetBlendState( 0, blendState );

    m_pd3dDevice->SetIndices( const_cast<D3DIndexBuffer*>(pIndexBuffer) );

    m_pd3dDevice->SetStreamSource( 1, const_cast<D3DVertexBuffer*>(pJointBuffer), 0, 12 * sizeof( FLOAT ) );

    // Render all appropriate batches of this model as normal opaque.
    for ( int osi = 0; osi < ARRAYSIZE( OPAQUE_SHADERS ); ++osi )
    {
        // The current shader we are looking for.
        const XAVATAR_SHADER currentShader = OPAQUE_SHADERS[ osi ];

        // Set the vertex declaration and shader for the current shader.
        m_pd3dDevice->SetVertexDeclaration( m_D3dShaders[ currentShader ].m_VertexDeclaration  );
        m_pd3dDevice->SetVertexShader     ( m_D3dShaders[ currentShader ].m_VertexShader       );
        m_pd3dDevice->SetPixelShader      ( m_D3dShaders[ currentShader ].m_PixelShader        );

        // Render all batches of this model that use this shader.
        for ( DWORD bi = 0; bi < pModel->BatchCount; ++bi )
        {
            // Reference the current batch.
            const XAVATAR_TRIANGLE_BATCH& batch = pModel->pBatches[ bi ];

            // If this batch uses the current shader then we render it.
            if ( batch.ShaderInstance.Shader == currentShader )
            {
                SetShaderTexturesAndConstants( avatarData, dwModelIndex, &batch.ShaderInstance, pModel->pTextures, avatarData.m_dwTextureLayers );
                m_pd3dDevice->SetStreamSource( 0, const_cast<D3DVertexBuffer*>(pVertexBuffer), batch.pVertices - pModel->pGlobalVertexBuffer, batch.VertexStride );
                m_pd3dDevice->DrawIndexedPrimitive( D3DPT_TRIANGLELIST, 0, 0, 0, ( WORD* )batch.pIndices - ( WORD* )pModel->pGlobalIndexBuffer, batch.TriangleCount );
                ClearShaderTexturesAndConstants( );
            }
        }
    }

    // Reset the device. Clear vertex declaration and shaders set above.
    m_pd3dDevice->SetVertexDeclaration( 0 );
    m_pd3dDevice->SetVertexShader     ( 0 );
    m_pd3dDevice->SetPixelShader      ( 0 );

    // Reset the device. Clear vertex buffer stream sources set above.
    m_pd3dDevice->SetStreamSource( 0, 0, 0, 0 );
    m_pd3dDevice->SetStreamSource( 1, 0, 0, 0 );

    // Reset the device: Clear the index buffer source set above.
    m_pd3dDevice->SetIndices( 0 );

    // Return success.
    return S_OK;
}

//--------------------------------------------------------------------------------------
// Name: RenderTransparent()
// Desc: Render a model in the transparent pass.  Make sure to call the PreRender()
//       function to set up the render state appropriately for the pass.
//--------------------------------------------------------------------------------------
HRESULT Sample::RenderTransparent( const UserAvatarData&    avatarData,
                                   DWORD                    dwModelIndex )
{
    const XAVATAR_MODEL*      pModel        = &avatarData.m_pAssets->pComponentModels[ dwModelIndex ];
    const D3DVertexBuffer*    pVertexBuffer = &avatarData.m_VertexBuffers[ dwModelIndex ];
    const D3DIndexBuffer*     pIndexBuffer  = &avatarData.m_IndexBuffers[ dwModelIndex ];
    const D3DVertexBuffer*    pJointBuffer  = &avatarData.m_JointBufferVBs[ avatarData.m_dwJointBufferIndex ];

    // If the model is empty then we have nothing to do here.
    if ( !pModel->GlobalIndexBufferSize )
    {
        return S_OK;
    }

    // Defines the shaders we consider when performing the transparent pass.
    const XAVATAR_SHADER TRANSPARENT_SHADERS[] = 
    { 
        XAVATAR_SHADER_BODY_SHINY_TRANSPARENT,
        XAVATAR_SHADER_BODY_TRANSPARENT, 
    };

    // Set up the render state for the transparent render pass.
    m_pd3dDevice->SetRenderState_Inline( D3DRS_ALPHATESTENABLE,          FALSE );
    m_pd3dDevice->SetRenderState_Inline( D3DRS_ZENABLE,                  TRUE  );
    m_pd3dDevice->SetRenderState_Inline( D3DRS_ZWRITEENABLE,             FALSE );
    m_pd3dDevice->SetRenderState_Inline( D3DRS_HIGHPRECISIONBLENDENABLE, TRUE  );

    D3DBLENDSTATE blendState;
    blendState.SrcBlend       = D3DBLEND_SRCALPHA;
    blendState.BlendOp        = D3DBLENDOP_ADD;
    blendState.DestBlend      = D3DBLEND_INVSRCALPHA;
    blendState.SrcBlendAlpha  = D3DBLEND_ONE;
    blendState.BlendOpAlpha   = D3DBLENDOP_ADD;
    blendState.DestBlendAlpha = D3DBLEND_INVSRCALPHA;
    m_pd3dDevice->SetBlendState( 0, blendState );

    m_pd3dDevice->SetIndices( const_cast<D3DIndexBuffer*>(pIndexBuffer) );

    m_pd3dDevice->SetStreamSource( 1, const_cast<D3DVertexBuffer*>(pJointBuffer), 0, 12 * sizeof( FLOAT ) );

    // Render all appropriate batches of this model as transparent.
    for ( int tsi = 0; tsi < ARRAYSIZE( TRANSPARENT_SHADERS ); ++tsi )
    {
        // The current shader we are looking for.
        const XAVATAR_SHADER currentShader = TRANSPARENT_SHADERS[ tsi ];

        // Set the vertex declaration and shader for the current shader.
        m_pd3dDevice->SetVertexDeclaration( m_D3dShaders[ currentShader ].m_VertexDeclaration  );
        m_pd3dDevice->SetVertexShader     ( m_D3dShaders[ currentShader ].m_VertexShader       );
        m_pd3dDevice->SetPixelShader      ( m_D3dShaders[ currentShader ].m_PixelShader        );

        // Render all batches of this model that use this shader.
        for ( DWORD bi = 0; bi < pModel->BatchCount; ++bi )
        {
            // Reference the current batch.
            const XAVATAR_TRIANGLE_BATCH& batch = pModel->pBatches[ bi ];

            // If this batch uses the current shader then we render it.
            if ( batch.ShaderInstance.Shader == currentShader )
            {
                SetShaderTexturesAndConstants( avatarData, dwModelIndex, &batch.ShaderInstance, pModel->pTextures, avatarData.m_dwTextureLayers );
                m_pd3dDevice->SetStreamSource( 0, const_cast<D3DVertexBuffer*>(pVertexBuffer), batch.pVertices - pModel->pGlobalVertexBuffer, batch.VertexStride );
                m_pd3dDevice->DrawIndexedPrimitive( D3DPT_TRIANGLELIST, 0, 0, 0, ( WORD* )batch.pIndices - ( WORD* )pModel->pGlobalIndexBuffer, batch.TriangleCount );
                ClearShaderTexturesAndConstants( );
            }
        }
    }

    // Reset the device. Clear vertex declaration and shaders set above.
    m_pd3dDevice->SetVertexDeclaration( 0 );
    m_pd3dDevice->SetVertexShader     ( 0 );
    m_pd3dDevice->SetPixelShader      ( 0 );

    // Reset the device. Clear vertex buffer stream sources set above.
    m_pd3dDevice->SetStreamSource( 0, 0, 0, 0 );
    m_pd3dDevice->SetStreamSource( 1, 0, 0, 0 );

    // Reset the device: Clear the index buffer source set above.
    m_pd3dDevice->SetIndices( 0 );

    // Return success.
    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: PostRender()
// Desc: Complete the normal rendering pass.
//--------------------------------------------------------------------------------------
HRESULT 
Sample::PostRender( )
{
    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: SetShaderTexturesAndConstants()
// Desc: Complete the normal rendering pass. Removes textures and shader settings
//       from the render state.
//--------------------------------------------------------------------------------------
VOID Sample::SetShaderTexturesAndConstants( const UserAvatarData&           avatarData,
                                            DWORD                           dwModelIndex,
                                            const XAVATAR_SHADER_INSTANCE*  shaderInstance,
                                            const XAVATAR_TEXTURE*          modelTextures,
                                            const DWORD*                    animatedTextureLayers )
{
    assert( modelTextures );
    assert( animatedTextureLayers );

    const XAVATAR_SHADER shader = shaderInstance->Shader;

    for ( int pi = 0; pi < XAVATAR_SHADER_INSTANCE_MAX_PARAMS; ++pi )
    {
        const XAVATAR_SHADER_PARAM& param = shaderInstance->Params[ pi ];

        switch ( param.Type )
        {
            case ( XAVATAR_SHADER_PARAM_TYPE_NONE ):
            {
                assert( param.Usage == XAVATAR_SHADER_PARAM_USAGE_NONE );
                break;
            }

            case ( XAVATAR_SHADER_PARAM_TYPE_VERTEX_CONSTANT ):
            {
                const BYTE reg = GetVertexConstantRegister( &param, shader );
                m_pd3dDevice->SetVertexShaderConstantF( reg, param.Data.Constant.Value, 1 );
                break;
            }

            case ( XAVATAR_SHADER_PARAM_TYPE_PIXEL_CONSTANT ):
            {
                const BYTE reg = GetPixelConstantRegister( &param );
                m_pd3dDevice->SetPixelShaderConstantF( reg, param.Data.Constant.Value, 1 );
                break;
            }

            case ( XAVATAR_SHADER_PARAM_TYPE_TEXTURE ):
            {
                const XAVATAR_TEXTURE& texture = modelTextures[ param.Data.Texture.Index ];

                DWORD layerIndex   = 0; 
                switch ( param.Usage )
                {
                    case ( XAVATAR_SHADER_PARAM_USAGE_TEXTURE_EYEBROW_LEFT  ):
                    {
                        layerIndex = animatedTextureLayers[ XAVATAR_ANIMATED_TEXTURE_EYEBROW_LEFT ];
                        break;
                    }
                    case ( XAVATAR_SHADER_PARAM_USAGE_TEXTURE_EYEBROW_RIGHT ):
                    {
                        layerIndex = animatedTextureLayers[ XAVATAR_ANIMATED_TEXTURE_EYEBROW_RIGHT ];
                        break;
                    }
                    case ( XAVATAR_SHADER_PARAM_USAGE_TEXTURE_EYE_LEFT  ):
                    {
                        layerIndex = animatedTextureLayers[ XAVATAR_ANIMATED_TEXTURE_EYE_LEFT ];
                        break;
                    }
                    case ( XAVATAR_SHADER_PARAM_USAGE_TEXTURE_EYE_RIGHT ):
                    {
                        layerIndex = animatedTextureLayers[ XAVATAR_ANIMATED_TEXTURE_EYE_RIGHT ];
                        break;
                    }
                    case ( XAVATAR_SHADER_PARAM_USAGE_TEXTURE_MOUTH ):
                    {
                        layerIndex = animatedTextureLayers[ XAVATAR_ANIMATED_TEXTURE_MOUTH ];
                        break;
                    }
 
                }
            
                layerIndex = ( layerIndex < texture.LayerCount ? layerIndex : 0 );
                const D3DTexture* textureHeader = &avatarData.m_Textures[ dwModelIndex ][ param.Data.Texture.Index ][ layerIndex ];

                const WORD index = GetShaderTextureIndex( param, shader );
                m_pd3dDevice->SetTexture( index, const_cast<D3DTexture*>(textureHeader) );
                m_pd3dDevice->SetSamplerState( index, D3DSAMP_ADDRESSU, ( ( param.Data.Texture.Flags & XAVATAR_TEXTURE_FLAGS_WRAP_U ) ? D3DTADDRESS_WRAP : D3DTADDRESS_CLAMP ) );
                m_pd3dDevice->SetSamplerState( index, D3DSAMP_ADDRESSV, ( ( param.Data.Texture.Flags & XAVATAR_TEXTURE_FLAGS_WRAP_V ) ? D3DTADDRESS_WRAP : D3DTADDRESS_CLAMP ) );
                break;
            }

            default:
            {
                // There are no other types
                assert( 0 );
            }
        }
    }
}


//--------------------------------------------------------------------------------------
// Name: GetVertexConstantRegister()
// Desc: Retrieve the constant register for a given vertex shader and parameter usage. 
//--------------------------------------------------------------------------------------
inline BYTE Sample::GetVertexConstantRegister(  const XAVATAR_SHADER_PARAM* param,
                                                XAVATAR_SHADER              shader ) const
{
    assert( shader < XAVATAR_SHADER_COUNT );
    assert( param->Type == XAVATAR_SHADER_PARAM_TYPE_VERTEX_CONSTANT );
    assert( param->Usage < XAVATAR_SHADER_PARAM_USAGE_COUNT );
    assert( m_VertexConstantRegisterMap[ param->Usage ][ shader ] != 0xffff );
    return  m_VertexConstantRegisterMap[ param->Usage ][ shader ];
}


//--------------------------------------------------------------------------------------
// Name: GetPixelConstantRegister()
// Desc: Retrieve the texture index or constant start register for a
//       given parameter usage. Note: Pixel constants are uniform 
//       across all shaders.
//--------------------------------------------------------------------------------------
inline BYTE Sample::GetPixelConstantRegister( const XAVATAR_SHADER_PARAM* param ) const
{
    assert( param->Type == XAVATAR_SHADER_PARAM_TYPE_PIXEL_CONSTANT );
    assert( param->Usage < XAVATAR_SHADER_PARAM_USAGE_COUNT );
    assert( m_PixelConstantRegisterMap[ param->Usage ] != 0xffff );
    return  m_PixelConstantRegisterMap[ param->Usage ];
}

//--------------------------------------------------------------------------------------
// Name: GetShaderTextureIndex()
// Desc: Retrieve the texture index for a given shader and parameter usage. 
//--------------------------------------------------------------------------------------
inline WORD Sample::GetShaderTextureIndex(  const XAVATAR_SHADER_PARAM& param,
                                            XAVATAR_SHADER              shader ) const
{
    assert( shader < XAVATAR_SHADER_COUNT );
    assert( param.Type == XAVATAR_SHADER_PARAM_TYPE_TEXTURE );
    assert( param.Usage < XAVATAR_SHADER_PARAM_USAGE_COUNT );

    if ( param.Data.Texture.UvIndex != XAVATAR_INVALID_UV_INDEX )
    {
        assert( m_ShaderTextureIndexMap[ param.Usage ][ shader ] == 0xff );
        return param.Data.Texture.UvIndex;
    }
    else
    {
        assert( m_ShaderTextureIndexMap[ param.Usage ][ shader ] != 0xff );
        return  m_ShaderTextureIndexMap[ param.Usage ][ shader ];
    }
}


//--------------------------------------------------------------------------------------
// Name: ClearShaderTexturesAndConstants()
// Desc: Remove textures and shader settings from the render state.
//--------------------------------------------------------------------------------------
HRESULT Sample::ClearShaderTexturesAndConstants( )
{
    m_pd3dDevice->SetTexture( 0, 0 );
    m_pd3dDevice->SetTexture( 1, 0 );
    m_pd3dDevice->SetTexture( 2, 0 );
    m_pd3dDevice->SetTexture( 3, 0 );
    m_pd3dDevice->SetTexture( 4, 0 );
    m_pd3dDevice->SetTexture( 5, 0 );

    return S_OK;
}
//--------------------------------------------------------------------------------------
// ActivePassiveSkeletons.cpp
//
// This sample demonstrates different policies in choosing an active skeleton, e.g. kiosk mode
//
// Note that throughout the source code of this sample, three terms will be used frequently
//
//  PlayerIndex         - index in range of [0..NUI_SKELETON_COUNT], including active and passive players
//  ActivePlayerIndex   - index in range of [0..NUI_SKELETON_MAX_TRACKED_COUNT], including only actively tracked players
//  TrackingID          - SkeletonFrame.SkeletonData.dwTrackingID of an actively tracked skeleton
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include <xtl.h>
#include <xnamath.h>
#include <AtgApp.h>
#include <AtgFont.h>
#include <AtgInput.h>
#include <AtgUtil.h>
#include <AtgHelp.h>
#include <AtgSimpleShaders.h>
#include <AtgNuiVisualization.h>
#include <AtgNuiCommon.h>


//--------------------------------------------------------------------------------------
// Global and constats
//--------------------------------------------------------------------------------------

enum TrackingMode  
{
  TRACKING_MODE_1_PLAYER_KIOSK,
  TRACKING_MODE_2_PLAYER_KIOSK,
  TRACKING_MODE_MAINTAIN_1_PLAYER,
  TRACKING_MODE_MAINTAIN_2_PLAYER,
  TRACKING_MODE_COUNT
};

WCHAR* g_TrackingModeText []=
{
    L"Tracking Mode = 1 Player Kiosk",
    L"Tracking Mode = 2 Player Kiosk",
    L"Tracking Mode = 1 Player Sticky Mode",
    L"Tracking Mode = 2 Player Sticky Mode"
};

static CONST UINT g_uDepthBufferWidth = 320;
static CONST UINT g_uDepthBufferHeight = 240;
static CONST XMVECTOR g_vRemoveY = XMVectorSet( 1.0f, 0.0f, 1.0f, 1.0f );
static CONST FLOAT g_fSnapBackOntoSkeletonThreshold = 0.3f;

INT g_iLastGoodActivePlayerIndex[ NUI_SKELETON_MAX_TRACKED_COUNT ] = { -1, -1 };
XMVECTOR g_vLastGoodActivePlayerPosition[ NUI_SKELETON_MAX_TRACKED_COUNT ] = { XMVectorZero(), XMVectorZero() };


//--------------------------------------------------------------------------------------
// Callouts for labeling the gamepad on the help screen
//--------------------------------------------------------------------------------------

ATG::HELP_CALLOUT g_HelpCallouts[] =
{
    { ATG::HELP_BACK_BUTTON,  ATG::HELP_PLACEMENT_2, L"Display\nhelp" },
    { ATG::HELP_A_BUTTON,  ATG::HELP_PLACEMENT_1, L"Toggle Skeleton Tracking Policy" }
};
static const DWORD NUM_HELP_CALLOUTS = ARRAY_SIZE( g_HelpCallouts );

struct DepthBufferVertex
{    
    XMFLOAT3 m_vPosition;
    XMFLOAT3 m_vNormal;
};

LPDIRECT3DVERTEXBUFFER9 g_pDepthVertBuffer;
LPDIRECT3DINDEXBUFFER9 g_pDepthIndBuffer;


//--------------------------------------------------------------------------------------
// Vertex shader for the Environment
//--------------------------------------------------------------------------------------

const CHAR* m_strVertexShaderProgram =
    " float4x4 matWVP : register(c0);              \n"
    " float4x4 g_matWorld : register(c4);          \n"
    "                                              \n"
    "    sampler s0  : register(s0);               \n"
    " struct VS_IN                                 \n"
    "                                              \n"
    " {                                            \n"
    "     float4 vObjPos   : POSITION;              \n"
    "     float3 vNormal   : NORMAL;                \n"
    " };                                           \n"
    "                                              \n"
    " struct VS_OUT                                \n"
    " {                                            \n"
    "     float4 vProjPos  : POSITION;              \n"
    "     float3 vNormal   : TEXCOORD1;             \n"
    "     float2 vTexture  : TEXCOORD2;             \n"
    " };                                           \n"
    "                                              \n"
    " VS_OUT main( VS_IN In )                      \n"
    " {                                            \n"
    "     VS_OUT Out;                              \n"
    "     float4 texCoord = float4( In.vObjPos.x + 0.5f, 1.0f - (In.vObjPos.y + 0.5f), 0.0f, 0.0f );    \n"
    "     float4 InPosition = In.vObjPos;  \n"
    "     InPosition.w = 1.0f;  \n"
    "     float Depth =  tex2Dlod( s0, texCoord ).r / 8; \n"
    "     InPosition.z = Depth * -0.0001f;  \n"
    "     float3 Right = float3( 1.0f, 0.0f, 0.0f ); \n"
    "     Right.z =  tex2Dlod( s0, float4( texCoord.x + 0.003125, texCoord.y, 0.0f, 0.0f ) ).r / 8 - Depth; \n"
    "     float3 Down = float3( 0.0f, 1.0f, 0.0f ); \n"
    "     Down.z =  tex2Dlod( s0, float4( texCoord.x, texCoord.y + 0.004167, 0.0f, 0.0f )  ).r / 8 - Depth; \n"
    "       \n"
    "     Out.vProjPos = mul( matWVP, InPosition );  \n"
    "     Out.vNormal = normalize( cross( Right, Down ) );  \n"
    "     Out.vTexture = texCoord.xy;              \n"
    "     return Out;                              \n"
    " }                                            \n";


//-------------------------------------------------------------------------------------
// Atrium Pixel shader
//-------------------------------------------------------------------------------------

const CHAR* m_strPixelShaderProgramScene =
    " struct PS_IN                                                          \n"
    " {                                                                     \n"
    "     float4 ProjPos  : POSITION;              \n"
    "     float3 vNormal   : TEXCOORD1;             \n"
    "     float2 vTexture  : TEXCOORD2;             \n"
    " };                                                                    \n"
    "                                                                       \n"
    "    sampler s0  : register(s0);               \n"
    "                                                                       \n"
    " static const float4 LookUpColors[] = {                                \n"
    "    float4(1.0f, 1.0f, 1.0f, 1.0f),                                    \n"
    "    float4(0.0f, 0.0f, 1.0f, 1.0f),                                    \n"
    "    float4(0.0f, 1.0f, 0.0f, 1.0f),                                    \n"
    "    float4(0.0f, 1.0f, 1.0f, 1.0f),                                    \n"
    "    float4(1.0f, 0.0f, 0.0f, 1.0f),                                    \n"
    "    float4(1.0f, 0.0f, 1.0f, 1.0f),                                    \n"
    "    float4(1.0f, 1.0f, 0.0f, 1.0f),                                    \n"
    "    float4(0.5f, 0.5f, 1.0f, 1.0f)                                     \n"
    " };                                                                    \n"             
    " float4 main( PS_IN In ) : COLOR                                       \n"
    " {                                                                     \n"
    "     float fVal = tex2D( s0, In.vTexture ).x;                          \n"
    "     fVal /= 8;                                                        \n"
    "     fVal = frac( fVal );                                             \n"
    "     fVal *= 8;                                                        \n"
    "     float3 vLightDir1 = float3( -1.0f, 1.0f, -1.0f );                 \n"
    "     float3 vLightDir2 = float3( 1.0f, 1.0f, -1.0f );                  \n"
    "     float3 vLightDir3 = float3( 0.0f, -1.0f, 0.0f );                  \n"
    "     float3 vLightDir4 = float3( 1.0f, 1.0f, 1.0f );                   \n" 
    "     float fLighting = 0.1f +                                          \n"
    "                  saturate( dot( vLightDir1 , In.vNormal ) )*0.25f +   \n"
    "                  saturate( dot( vLightDir2 , In.vNormal ) )*0.25f +   \n"
    "                  saturate( dot( vLightDir3 , In.vNormal ) )*0.25f +   \n"
    "                  saturate( dot( vLightDir4 , In.vNormal ) )*0.25f;    \n"
    " return fLighting * LookUpColors[fVal];                                \n"
    " }                                                                     \n";


//--------------------------------------------------------------------------------------
// Name: UpdateTrackingIDsOnePlayerKioskMode
// Desc: provide a common function for kiosk mode behavior
//--------------------------------------------------------------------------------------

void UpdateTrackingIDsOnePlayerKioskMode( __in NUI_SKELETON_FRAME* pSkeletonFrame,
                                          __out DWORD pTrackingIDs[ NUI_SKELETON_MAX_TRACKED_COUNT ] )
{
    // reset the postions on mode change;
    for ( UINT iActivePlayerIndex = 0; iActivePlayerIndex < NUI_SKELETON_MAX_TRACKED_COUNT; iActivePlayerIndex++ )
    {
        g_iLastGoodActivePlayerIndex[ iActivePlayerIndex ] = -1;
    }

    INT iClosestPlayerIndex = -1;

    FLOAT fMinDistance = FLT_MAX;

    for ( INT iPlayerIndex = 0; iPlayerIndex < NUI_SKELETON_COUNT; iPlayerIndex++ )
    {
        NUI_SKELETON_DATA* pSkeletonData = &pSkeletonFrame->SkeletonData[ iPlayerIndex ];

        if ( pSkeletonData->eTrackingState == NUI_SKELETON_NOT_TRACKED )
        {
            continue;
        }

        XMVECTOR vCurrentPosition = pSkeletonData->Position * g_vRemoveY;

        // calculate the modified distance between the center and 'center' of player
        FLOAT fCurDistance = XMVectorGetX( XMVector3LengthSq( vCurrentPosition ) ); 

        if ( fCurDistance < fMinDistance )
        {
            iClosestPlayerIndex = iPlayerIndex;
            fMinDistance = fCurDistance;
        }              
    }

    // only swap if there is a closest and it is not already in the first slot
    if ( iClosestPlayerIndex != -1 )
    {
        pTrackingIDs[ 0 ] = pSkeletonFrame->SkeletonData[ iClosestPlayerIndex ].dwTrackingID;
    }
    
    pTrackingIDs[ 1 ] = 0;
}


//--------------------------------------------------------------------------------------
// Name: UpdateTrackingIDsTwoPlayerKioskMode
// Desc: Common function used to find the two closest players to the camera
//--------------------------------------------------------------------------------------

void UpdateTrackingIDsTwoPlayerKioskMode( __in NUI_SKELETON_FRAME* pSkeletonFrame,
                                          __out DWORD pTrackingIDs[ NUI_SKELETON_MAX_TRACKED_COUNT ] )
{
    __analysis_assume( sizeof(pTrackingIDs) ==  NUI_SKELETON_MAX_TRACKED_COUNT * sizeof( DWORD ) );

    // reset the postions on mode change;
    for ( UINT iActivePlayerIndex = 0; iActivePlayerIndex < NUI_SKELETON_MAX_TRACKED_COUNT; iActivePlayerIndex++ )
    {
        g_iLastGoodActivePlayerIndex[ iActivePlayerIndex ] = -1;
    }

    INT pClosestPlayerIndex[ NUI_SKELETON_MAX_TRACKED_COUNT ] = { -1, -1 };
    FLOAT pMinDistance[ NUI_SKELETON_MAX_TRACKED_COUNT ] = { FLT_MAX, FLT_MAX };

    for ( INT iPlayerIndex = 0; iPlayerIndex < NUI_SKELETON_COUNT; iPlayerIndex++ )
    {
        NUI_SKELETON_DATA* pSkeletonData = &pSkeletonFrame->SkeletonData[ iPlayerIndex ];

        if ( pSkeletonData->eTrackingState == NUI_SKELETON_NOT_TRACKED )
        {
            continue;
        }

        XMVECTOR vCurrentPosition = pSkeletonData->Position * g_vRemoveY;

        // calculate the modified distance between the camera and 'center' of player.
        FLOAT fCurDistance  = XMVectorGetX( XMVector3LengthEst( vCurrentPosition ) ); 
        INT iCurPlayerIndex = iPlayerIndex;

        // if the player is active, bring them forward by ~1 foot
        // this forces passive players to move past active players by some additional distance before they become active
        if ( pSkeletonData->eTrackingState == NUI_SKELETON_TRACKED )
        {
            fCurDistance -= g_fSnapBackOntoSkeletonThreshold;
        }
        
        // need to keep the closest two players
        // do a 2 element insertion sort
        for ( INT iActivePlayerIndex = 0; iActivePlayerIndex < NUI_SKELETON_MAX_TRACKED_COUNT; iActivePlayerIndex++ )
        {        
            // if the current player is closer than the distance in the current slot
            // then update the slot and use the data in the slot as the next cur player
            if ( fCurDistance < pMinDistance[ iActivePlayerIndex ] )
            {
                FLOAT fOldDistance = pMinDistance[ iActivePlayerIndex ];
                INT iOldPlayerIndex = pClosestPlayerIndex[ iActivePlayerIndex ];
            
                pClosestPlayerIndex[ iActivePlayerIndex ] = iCurPlayerIndex;
                pMinDistance[ iActivePlayerIndex ] = fCurDistance;
                
                // swap the old player in as the current player to see if it fits in the next slot
                iCurPlayerIndex = iOldPlayerIndex;
                fCurDistance = fOldDistance;
            }     
        }    
    }

    for ( INT iActivePlayerIndex = 0; iActivePlayerIndex < NUI_SKELETON_MAX_TRACKED_COUNT; iActivePlayerIndex++ )
    {        
        INT iPlayerIndex = pClosestPlayerIndex[ iActivePlayerIndex ];

        if ( iPlayerIndex != -1 )
        {
            pTrackingIDs[ iActivePlayerIndex ] = pSkeletonFrame->SkeletonData[ iPlayerIndex ].dwTrackingID;
        }
        else
        {
            pTrackingIDs[ iActivePlayerIndex ] = 0;
        }
    }    
}


//--------------------------------------------------------------------------------------
// Name: UpdateTrackingIDsOnePlayerStickyMode
// Desc: Same as one player mode, but tries to "stick" the the currently selected
//       player and not switch immediately
//--------------------------------------------------------------------------------------

void UpdateTrackingIDsOnePlayerStickyMode( __in NUI_SKELETON_FRAME* pSkeletonFrame,
                                           __out DWORD pTrackingIDs[ NUI_SKELETON_MAX_TRACKED_COUNT ] )
{
    for ( INT iPlayerIndex = 0; iPlayerIndex < NUI_SKELETON_COUNT; iPlayerIndex++ )
    {
        NUI_SKELETON_DATA* pSkeletonData = &pSkeletonFrame->SkeletonData[ iPlayerIndex ];

        // If we find a tracked skeleton keep doing what you're doing.
        if ( pSkeletonData->eTrackingState == NUI_SKELETON_TRACKED )
        {
            pTrackingIDs[ 0 ] = pSkeletonData->dwTrackingID;
            pTrackingIDs[ 1 ] = 0;

            g_vLastGoodActivePlayerPosition[ 0 ] = pSkeletonData->Position;
            g_iLastGoodActivePlayerIndex[ 0 ] = iPlayerIndex;

            return;
        }
    }

    // no skeleton is actively being tracked.
    
    // If we don't have a good last position to search around then we default to kiosk mode.
    if ( g_iLastGoodActivePlayerIndex[ 0 ] == -1 ) 
    {
        UpdateTrackingIDsOnePlayerKioskMode( pSkeletonFrame, pTrackingIDs );

        // don't update the position untill we're actually tracked.
        return;
    }

    // Otherwise we check the current skeletons to see if there is one 
    // that is close enough to the last known skeleton position (poor man's identity)
    INT iClosestPlayerIndex = -1;
    FLOAT fClosestDistance = FLT_MAX;
    for ( INT iPlayerIndex = 0; iPlayerIndex < NUI_SKELETON_COUNT; iPlayerIndex++ )
    {
        NUI_SKELETON_DATA* pSkeletonData = &pSkeletonFrame->SkeletonData[ iPlayerIndex ];

        if ( pSkeletonData->eTrackingState != NUI_SKELETON_NOT_TRACKED )
        {
            XMVECTOR vCurrentPosition = pSkeletonData->Position * g_vRemoveY;
            XMVECTOR vLastGoodPosition = g_vLastGoodActivePlayerPosition[ 0 ] * g_vRemoveY;

            FLOAT fDistance = XMVectorGetX( XMVector3LengthEst( vLastGoodPosition - vCurrentPosition ) );

            if ( fDistance < fClosestDistance )
            {
                fClosestDistance = fDistance;
                iClosestPlayerIndex = iPlayerIndex;
            }
        }
    }

    if ( fClosestDistance < g_fSnapBackOntoSkeletonThreshold )
    {
        g_iLastGoodActivePlayerIndex[ 0 ] = iClosestPlayerIndex;
        g_vLastGoodActivePlayerPosition[ 0 ] = pSkeletonFrame->SkeletonData[ iClosestPlayerIndex ].Position;
    }

    pTrackingIDs[ 0 ] = pSkeletonFrame->SkeletonData[ g_iLastGoodActivePlayerIndex[ 0 ] ].dwTrackingID;
    pTrackingIDs[ 1 ] = 0;
}


//--------------------------------------------------------------------------------------
// Name: GetActivePlayer
// Desc: Get the first actively tracked skeleton
//--------------------------------------------------------------------------------------

BOOL GetActivePlayer( __in NUI_SKELETON_FRAME* pSkeletonFrame, __in INT iSkipPlayerIndex, __out INT* pPlayerIndex )
{
    for ( INT iPlayerIndex = 0; iPlayerIndex < NUI_SKELETON_COUNT; iPlayerIndex++ )
    {
        if ( iPlayerIndex != iSkipPlayerIndex )
        {
            if ( pSkeletonFrame->SkeletonData[ iPlayerIndex ].eTrackingState == NUI_SKELETON_TRACKED )
            {
                *pPlayerIndex = iPlayerIndex;
                return TRUE;
            }
        }
    }
    
    return FALSE;
}


//--------------------------------------------------------------------------------------
// Name: GetPassivePlayerClosestToCamera
// Desc: Returns the closest player to the camera of a passive player
//--------------------------------------------------------------------------------------

BOOL GetPassivePlayerClosestToCamera( __in NUI_SKELETON_FRAME* pSkeletonFrame, __in INT iSkipPlayerIndex,
                                      __out INT* pPlayerIndex, __out INT* pActivePlayerIndex )
{
    FLOAT fClosestValue = FLT_MAX;
    INT iClosestPlayerIndex = -1;
    INT iClosestActivePlayerIndex = 0;

    for ( INT iPlayerIndex = 0; iPlayerIndex < NUI_SKELETON_COUNT; iPlayerIndex++ )
    {
        if ( iPlayerIndex != iSkipPlayerIndex )
        {
            NUI_SKELETON_DATA* pSkeletonData = &pSkeletonFrame->SkeletonData[ iPlayerIndex ];

            // Only interested in passive players
            if ( pSkeletonData->eTrackingState == NUI_SKELETON_POSITION_ONLY )
            {
                XMVECTOR vCurrentPosition = pSkeletonData->Position * g_vRemoveY;               
                
                for ( INT iActivePlayerIndex = 0; iActivePlayerIndex < NUI_SKELETON_MAX_TRACKED_COUNT; iActivePlayerIndex++ )
                {
                    INT iIndex = g_iLastGoodActivePlayerIndex[ iActivePlayerIndex ];

                    if ( iIndex != -1 &&
                         iIndex != iSkipPlayerIndex )
                    {
                        XMVECTOR vLastGoodPosition = g_vLastGoodActivePlayerPosition[ iActivePlayerIndex ] * g_vRemoveY;

                        FLOAT fDistance = XMVectorGetX( XMVector3LengthEst( vLastGoodPosition - vCurrentPosition ) );
                        
                        if ( fDistance < fClosestValue )
                        {
                            fClosestValue = fDistance;
                            iClosestPlayerIndex = iPlayerIndex;
                            iClosestActivePlayerIndex = iActivePlayerIndex;
                        }
                    }
                }
            }
        }
    }

    if ( fClosestValue < g_fSnapBackOntoSkeletonThreshold )
    {
        *pPlayerIndex = iClosestPlayerIndex;
        *pActivePlayerIndex = iClosestActivePlayerIndex;

        return TRUE;
    }

    return FALSE;
}


//--------------------------------------------------------------------------------------
// Name: UpdateTrackingIDsTwoPlayerStickyMode
// Desc: Same as two player kiosk but tries to keep the current players active
//--------------------------------------------------------------------------------------

void UpdateTrackingIDsTwoPlayerStickyMode( __in NUI_SKELETON_FRAME* pSkeletonFrame,
                                           __out DWORD pTrackingIDs[ NUI_SKELETON_MAX_TRACKED_COUNT ] )
{
    __analysis_assume( sizeof(pTrackingIDs) ==  NUI_SKELETON_MAX_TRACKED_COUNT * sizeof( DWORD ) );

    INT pPlayerIndex[ NUI_SKELETON_MAX_TRACKED_COUNT ] = { -1, -1 };
    INT pClosestActivePlayerIndex[ NUI_SKELETON_MAX_TRACKED_COUNT ] = { -1, -1 };

    if ( GetActivePlayer( pSkeletonFrame, -1, &pPlayerIndex[ 0 ] ) )
    {
        if ( !GetActivePlayer( pSkeletonFrame, pPlayerIndex[ 0 ], &pPlayerIndex[ 1 ] ) )
        {
            GetPassivePlayerClosestToCamera( pSkeletonFrame, -1, &pPlayerIndex[ 1 ], &pClosestActivePlayerIndex[ 1 ] );
        }
    }
    else
    {
        if ( GetPassivePlayerClosestToCamera( pSkeletonFrame, -1, &pPlayerIndex[ 0 ], &pClosestActivePlayerIndex[ 0 ] ) )
        {
            GetPassivePlayerClosestToCamera( pSkeletonFrame, pPlayerIndex[ 0 ], &pPlayerIndex[ 1 ], &pClosestActivePlayerIndex[ 1 ] );
        }
    }

    if ( pPlayerIndex[ 0 ] != -1 &&
         pPlayerIndex[ 1 ] != -1 )
    {
        BOOL bIDsTheSame = pTrackingIDs[ 0 ] == pSkeletonFrame->SkeletonData[ pPlayerIndex[ 0 ] ].dwTrackingID &&
                           pTrackingIDs[ 1 ] == pSkeletonFrame->SkeletonData[ pPlayerIndex[ 1 ] ].dwTrackingID;

        bIDsTheSame |= pTrackingIDs[ 0 ] == pSkeletonFrame->SkeletonData[ pPlayerIndex[ 1 ] ].dwTrackingID &&
                       pTrackingIDs[ 1 ] == pSkeletonFrame->SkeletonData[ pPlayerIndex[ 0 ] ].dwTrackingID;

        if ( !bIDsTheSame )
        {
            pTrackingIDs[ 0 ] = pSkeletonFrame->SkeletonData[ pPlayerIndex[ 0 ] ].dwTrackingID;
            pTrackingIDs[ 1 ] = pSkeletonFrame->SkeletonData[ pPlayerIndex[ 1 ] ].dwTrackingID;
        }
    }
    else if ( pPlayerIndex[ 0 ] != -1 )
    {
        pTrackingIDs[ 0 ] = pSkeletonFrame->SkeletonData[ pPlayerIndex[ 0 ] ].dwTrackingID;
        pTrackingIDs[ 1 ] = 0;
        
        if ( g_iLastGoodActivePlayerIndex[ 1 ]  == -1 )
        {
            // try to find the second position and track it because there is no closest position to work from.
            for ( INT iPlayerIndex = 0; iPlayerIndex < NUI_SKELETON_COUNT && pPlayerIndex[ 1 ] == -1; iPlayerIndex++ )
            {
                if ( iPlayerIndex != pPlayerIndex[ 0 ] &&
                     pSkeletonFrame->SkeletonData[ iPlayerIndex ].eTrackingState == NUI_SKELETON_POSITION_ONLY )
                {
                    pPlayerIndex[ 1 ] = iPlayerIndex;
                    pTrackingIDs[ 1 ] = pSkeletonFrame->SkeletonData[ iPlayerIndex ].dwTrackingID;
                }
            }
        }
    }
    else 
    {
        pTrackingIDs[ 0 ] = 0;
        pTrackingIDs[ 1 ] = 0;

        if ( g_iLastGoodActivePlayerIndex[ 0 ] == -1 )
        {
            // try to find the first position and track it because there is no closest position to work from.
            for ( INT iPlayerIndex = 0; iPlayerIndex < NUI_SKELETON_COUNT && pPlayerIndex[ 0 ] == -1; iPlayerIndex++ )
            {
                if ( pSkeletonFrame->SkeletonData[ iPlayerIndex ].eTrackingState == NUI_SKELETON_POSITION_ONLY )
                {
                    pPlayerIndex[ 0 ] = iPlayerIndex;
                    pTrackingIDs[ 0 ] = pSkeletonFrame->SkeletonData[ iPlayerIndex ].dwTrackingID;
                }
            }
        }

        if ( g_iLastGoodActivePlayerIndex[ 1 ] == -1 && pPlayerIndex[ 0 ] != -1 )
        {
            // try to find the second position and track it because there is no closest position to work from.
            for ( INT iPlayerIndex = 0; iPlayerIndex < NUI_SKELETON_COUNT && pPlayerIndex[ 1 ] == -1; iPlayerIndex++ )
            {
                if ( iPlayerIndex != pPlayerIndex[ 0 ] &&
                     pSkeletonFrame->SkeletonData[ iPlayerIndex ].eTrackingState == NUI_SKELETON_POSITION_ONLY )
                {
                    pPlayerIndex[ 1 ] = iPlayerIndex;
                    pTrackingIDs[ 1 ] = pSkeletonFrame->SkeletonData[ iPlayerIndex ].dwTrackingID;
                }
            }
        }
    }

    // save position
    for ( INT iActivePlayerIndex = 0; iActivePlayerIndex < NUI_SKELETON_MAX_TRACKED_COUNT; iActivePlayerIndex++ )
    {
        INT iPlayerIndex = pPlayerIndex[ iActivePlayerIndex ];

        if ( iPlayerIndex != -1 &&
             pSkeletonFrame->SkeletonData[ iPlayerIndex ].eTrackingState == NUI_SKELETON_TRACKED )
        {
            FLOAT pDistance[ NUI_SKELETON_MAX_TRACKED_COUNT ] = { FLT_MAX, FLT_MAX };
            XMVECTOR vCurrentPosition = pSkeletonFrame->SkeletonData[ iPlayerIndex ].Position * g_vRemoveY;

            for ( INT iIndex = 0; iIndex < NUI_SKELETON_MAX_TRACKED_COUNT; iIndex++ )
            {
                if ( g_iLastGoodActivePlayerIndex[ iIndex ] != -1 )
                {
                    XMVECTOR vLastPosition = g_vLastGoodActivePlayerPosition[ iIndex ] * g_vRemoveY;
                    pDistance[ iIndex ] = XMVectorGetX( XMVector3LengthEst( vLastPosition - vCurrentPosition ) );
                }
            }

            if ( pDistance[ 0 ] <= pDistance[ 1 ] )
            {
                g_iLastGoodActivePlayerIndex[ 0 ] = iPlayerIndex;
                g_vLastGoodActivePlayerPosition[ 0 ] = pSkeletonFrame->SkeletonData[ iPlayerIndex ].Position;
            }
            else 
            {
                g_iLastGoodActivePlayerIndex[ 1 ] = iPlayerIndex;
                g_vLastGoodActivePlayerPosition[ 1 ] = pSkeletonFrame->SkeletonData[ iPlayerIndex ].Position;
            }
        }
    }   
}


//--------------------------------------------------------------------------------------
// Name: class Sample
// Desc: Main class to run this application. Most functionality is inherited
//       from the ATG::Application base class.
//--------------------------------------------------------------------------------------

class Sample : public ATG::Application
{
    ATG::Timer m_Timer;    // Timer
    ATG::Font m_Font;      // Font for drawing text
    ATG::Help m_Help;
    BOOL m_bDrawHelp;
    ATG::PackedResource m_Resource; // Bundled textures in a packed resource

    // Transform matrices
    XMMATRIX m_matView;
    XMMATRIX m_matProj;

    LPDIRECT3DVERTEXSHADER9     m_pVertexShader;
    LPDIRECT3DPIXELSHADER9      m_pPixelShader;

	HANDLE                      m_hFrameEndEvent;
	
	HANDLE                      m_hDepth;
    CONST NUI_IMAGE_FRAME*      m_pDepthFrame;
    NUI_SKELETON_FRAME          m_SkeletonFrame;
    INT                         m_iNUINextDepthFrame;
    D3DTexture*                 m_pCopyOfDepthTextureTemporaryShouldLetNUIBufferThese;

    // Natural Input data
    TrackingMode                m_TrackingMode;

	ATG::NuiVisualization       m_pip;

    void UpdateNUI();

public:
    virtual HRESULT     Initialize();
    virtual HRESULT     Update();
    virtual HRESULT     Render();
};


//--------------------------------------------------------------------------------------
// Name: main()
// Desc: Entry point to the program
//--------------------------------------------------------------------------------------

VOID __cdecl main()
{

    Sample atgApp;
    ATG::GetVideoSettings( &atgApp.m_d3dpp.BackBufferWidth, &atgApp.m_d3dpp.BackBufferHeight );
    atgApp.m_d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_TWO;
    atgApp.Run();
}


//--------------------------------------------------------------------------------------
// Name: Initialize()
// Desc: Initialize app-dependent objects
//--------------------------------------------------------------------------------------

HRESULT Sample::Initialize()
{
    HRESULT hr;

    m_bDrawHelp = FALSE;
    m_TrackingMode = TRACKING_MODE_1_PLAYER_KIOSK;

    // Initialize simple shaders.
    ATG::SimpleShaders::Initialize( NULL, NULL );
    
    // Create the font
    if ( FAILED( hr = m_Font.Create( "game:\\Media\\Fonts\\Arial_16.xpr" ) ) )
    {
        ATG_PrintError( "Couldn't create font\n" );
        return hr;
    }

    // Create the help
    if ( FAILED( hr = m_Help.Create( "game:\\Media\\Help\\Help.xpr" ) ) )
    {
        ATG_PrintError( "Couldn't create help\n" );
        return hr;
    }

    // Confine text drawing to the title safe area
    m_Font.SetWindow( ATG::GetTitleSafeArea() );

    // Create the textures resource
    if ( FAILED( hr = m_Resource.Create( "game:\\Media\\Resource.xpr" ) ) )
    {
        ATG_PrintError( "Couldn't create Resource.xpr\n" );
        return hr;
    }

    hr = m_pip.Initialize( m_pd3dDevice, NUI_INITIALIZE_FLAG_USES_DEPTH_AND_PLAYER_INDEX |
                                         NUI_INITIALIZE_FLAG_USES_SKELETON,
                                         NUI_IMAGE_RESOLUTION_640x480 );

    UINT uQuadWidth = g_uDepthBufferWidth - 1;
    UINT uQuadHeight = g_uDepthBufferHeight - 1;
    m_pd3dDevice->CreateIndexBuffer( uQuadWidth * uQuadHeight * 4 * 4, 0, D3DFMT_INDEX32, 0, &g_pDepthIndBuffer, NULL );

    UINT* pIndData;
    g_pDepthIndBuffer->Lock( 0, 0, (VOID**)&pIndData, 0 );
    
    for ( UINT y = 0; y < uQuadHeight; y++ )
    {
        for ( UINT x = 0; x < uQuadWidth; x++ )
        {
            pIndData[ y * uQuadWidth * 4 + ( x * 4 ) + 0 ] = (UINT)( y * g_uDepthBufferWidth + x );                       
            pIndData[ y * uQuadWidth * 4 + ( x * 4 ) + 1 ] = (UINT)( y * g_uDepthBufferWidth + x + 1 );
            pIndData[ y * uQuadWidth * 4 + ( x * 4 ) + 2 ] = (UINT)( ( y + 1 ) * g_uDepthBufferWidth + x + 1 );
            pIndData[ y * uQuadWidth * 4 + ( x * 4 ) + 3 ] = (UINT)( ( y + 1 ) * g_uDepthBufferWidth + x );
        }
    }

    g_pDepthIndBuffer->Unlock();

    INT iSize = g_uDepthBufferWidth * g_uDepthBufferHeight * sizeof( DepthBufferVertex );
    m_pd3dDevice->CreateVertexBuffer( iSize, 0, 0, 0, &g_pDepthVertBuffer, NULL );

    DepthBufferVertex* pDepthBufferVerticies;
    g_pDepthVertBuffer->Lock( 0, g_uDepthBufferHeight * g_uDepthBufferWidth * sizeof(DepthBufferVertex), 
                             (VOID**)&pDepthBufferVerticies, 0 );

    DepthBufferVertex* pCurrentVert;

    XMFLOAT3 fRecipDiv = XMFLOAT3( 1.0f / (FLOAT)g_uDepthBufferWidth, 1.0f / (FLOAT)g_uDepthBufferHeight, 1.0f / 65536.0f );

    for ( UINT y = 0; y < g_uDepthBufferHeight; y++ )
    {
        for ( UINT x = 0; x < g_uDepthBufferWidth; x++ )
        {
            pCurrentVert = &pDepthBufferVerticies[ y * g_uDepthBufferWidth + x ];
            pCurrentVert->m_vPosition.x = x * fRecipDiv.x - 0.5f;
            pCurrentVert->m_vPosition.y = y * fRecipDiv.y - 0.5f;
            pCurrentVert->m_vPosition.z = 0.0f;
        }
    }

    g_pDepthVertBuffer->Unlock();

    // Set the transform matrices
    ID3DXBuffer* pShaderCode = NULL;
    ID3DXBuffer* pErrorMsg = NULL;

    // Create the vertex shader
    D3DXCompileShader( m_strVertexShaderProgram, ( UINT )strlen( m_strVertexShaderProgram ),  NULL, NULL,
                       "main", "vs.3.0", 0, &pShaderCode, &pErrorMsg, NULL );
    m_pd3dDevice->CreateVertexShader( ( DWORD* )pShaderCode->GetBufferPointer(), &m_pVertexShader );
    pShaderCode->Release();

    // Create the pixel shader
    D3DXCompileShader( m_strPixelShaderProgramScene, ( UINT )strlen( m_strPixelShaderProgramScene ), NULL, NULL,
                       "main", "ps.3.0", 0, &pShaderCode, &pErrorMsg, NULL );        
    m_pd3dDevice->CreatePixelShader( ( DWORD* )pShaderCode->GetBufferPointer(), &m_pPixelShader );
    pShaderCode->Release();

    hr = m_pd3dDevice->CreateTexture( g_uDepthBufferWidth, g_uDepthBufferHeight, 1, 0, D3DFMT_LIN_D16,
                                      0, &m_pCopyOfDepthTextureTemporaryShouldLetNUIBufferThese, NULL );

    // Create event which will be signaled when frame processing ends
    m_hFrameEndEvent = CreateEvent( NULL,
                                    FALSE,  // auto-reset
                                    FALSE,  // create unsignaled
                                    "NuiFrameEndEvent" );

    if ( !m_hFrameEndEvent )
    {
        ATG_PrintError( "Failed to create NuiFrameEndEvent\n" );
        return E_FAIL;
    }
    
    hr = NuiInitialize( NUI_INITIALIZE_FLAG_USES_SKELETON | 
                        NUI_INITIALIZE_FLAG_USES_DEPTH_AND_PLAYER_INDEX,
                        NUI_INITIALIZE_DEFAULT_HARDWARE_THREAD );

    if ( FAILED( hr ) )
    {
        ATG::NuiPrintError( hr, "NuiInitialize" );
        return E_FAIL;
    }
    
    // Register frame end event with NUI
    hr = NuiSetFrameEndEvent( m_hFrameEndEvent, 0 );   
    if ( FAILED( hr ) )
    {
        ATG::NuiPrintError( hr, "NuiSetFrameEndEvent" );
        return E_FAIL;
    }

    // This flag specifies that we want to overide the default skeleton selection
    hr = NuiSkeletonTrackingEnable( NULL, NUI_SKELETON_TRACKING_FLAG_TITLE_SETS_TRACKED_SKELETONS );
    if ( FAILED ( hr ) )
    {
        ATG::NuiPrintError( hr, "NuiSkeletonTrackingEnable" );
    }

    // Open the depth stream
	hr = NuiImageStreamOpen( NUI_IMAGE_TYPE_DEPTH_AND_PLAYER_INDEX, NUI_IMAGE_RESOLUTION_320x240, 0, 1, NULL, &m_hDepth );
    if ( FAILED ( hr ) )
    {
        ATG::NuiPrintError( hr, "NuiImageStreamOpen" );
        return E_FAIL;
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: UpdateNUI()
// Desc: Called once per frame, check to see if a new skeleton has been received
//--------------------------------------------------------------------------------------

void Sample::UpdateNUI ()
{
    // Wait for frame processing to end
    if ( WAIT_OBJECT_0 != WaitForSingleObject( m_hFrameEndEvent, NUI_FRAME_END_TIMEOUT_DEFAULT ) )
    {
        return;
    }

    // Get data from the next camera depth frame
    HRESULT hrImage = NuiImageStreamGetNextFrame( m_hDepth, 0, &m_pDepthFrame );

    // Get data from the next skeleton frame
    HRESULT hrSkeleton = NuiSkeletonGetNextFrame( 0, &m_SkeletonFrame );

    // Process depth frame
    if ( SUCCEEDED( hrImage ) )
    {
        D3DLOCKED_RECT CopyToRect;
        D3DLOCKED_RECT CopyFromRect;
        m_pip.SetDepthTexture( m_pDepthFrame->pFrameTexture );
        m_pCopyOfDepthTextureTemporaryShouldLetNUIBufferThese->LockRect( 0, &CopyToRect, NULL, 0 ); 
        m_pDepthFrame->pFrameTexture->LockRect( 0, &CopyFromRect, NULL, D3DLOCK_READONLY );

        // Get copy of Depth
        if ( CopyToRect.Pitch == CopyFromRect.Pitch )
        {
            XMemCpyStreaming( CopyToRect.pBits, CopyFromRect.pBits, CopyToRect.Pitch * g_uDepthBufferHeight );
        }
        else 
        {
            size_t usePitch = min( CopyToRect.Pitch, CopyFromRect.Pitch );
            for (INT iIndex = 0; iIndex < g_uDepthBufferHeight; ++iIndex )
            {
                memcpy( (VOID*)((UINT)CopyToRect.pBits + CopyToRect.Pitch * iIndex), 
                        (VOID*)((UINT)CopyFromRect.pBits + CopyFromRect.Pitch * iIndex), usePitch );
            }
        }

        m_pCopyOfDepthTextureTemporaryShouldLetNUIBufferThese->UnlockRect( 0 );
        m_pDepthFrame->pFrameTexture->UnlockRect( 0 );
        NuiImageStreamReleaseFrame( m_hDepth, m_pDepthFrame );
    }        

    // Process skeleton frame
    if ( SUCCEEDED( hrSkeleton ) )
    {
        static DWORD dwStaticTrackedIds[ NUI_SKELETON_MAX_TRACKED_COUNT ] = { 0xffffffff, 0xffffffff };
        DWORD dwTrackedIds[ NUI_SKELETON_MAX_TRACKED_COUNT ] = { 0xffffffff, 0xffffffff };
        
        switch ( m_TrackingMode )
        {
            case TRACKING_MODE_1_PLAYER_KIOSK:
            {
                UpdateTrackingIDsOnePlayerKioskMode( &m_SkeletonFrame, dwTrackedIds );
            }
            break;

            case TRACKING_MODE_2_PLAYER_KIOSK:
            {
                UpdateTrackingIDsTwoPlayerKioskMode( &m_SkeletonFrame, dwTrackedIds );
            }
            break;

            case TRACKING_MODE_MAINTAIN_1_PLAYER:
            {
                UpdateTrackingIDsOnePlayerStickyMode( &m_SkeletonFrame, dwTrackedIds );
            }
            break;

            case TRACKING_MODE_MAINTAIN_2_PLAYER:
            {
                for ( UINT i = 0; i < NUI_SKELETON_MAX_TRACKED_COUNT; i++ )
                {
                    dwTrackedIds[ i ] = dwStaticTrackedIds[ i ];
                }
                UpdateTrackingIDsTwoPlayerStickyMode( &m_SkeletonFrame, dwTrackedIds );
            }
            break;
        
        }

        BOOL bTrackingChanged = FALSE;
        for ( UINT i = 0; i < NUI_SKELETON_MAX_TRACKED_COUNT; i++ )
        {
            bTrackingChanged |= dwTrackedIds[ i ] != dwStaticTrackedIds[ i ];
        }

        if ( bTrackingChanged )
        {
            // only call this when tracked changes.
            for ( UINT i = 0; i < NUI_SKELETON_MAX_TRACKED_COUNT; i++ )
            {
                dwStaticTrackedIds[ i ] = dwTrackedIds[ i ];
            }
        }

        // The API that will select the tracking using the selected IDs
        NuiSkeletonSetTrackedSkeletons( dwStaticTrackedIds );
        
        m_pip.SetSkeletons( &m_SkeletonFrame );
    }
    
    return;
}


//--------------------------------------------------------------------------------------
// Name: Update()
// Desc: Called once per frame, the call is the entry point for animating the scene.
//--------------------------------------------------------------------------------------

HRESULT Sample::Update()
{
    // Get the current gamepad state
    ATG::GAMEPAD* pGamepad = ATG::Input::GetMergedInput();

    // Toggle help
    if ( pGamepad->wPressedButtons & XINPUT_GAMEPAD_BACK )
    {
        m_bDrawHelp = !m_bDrawHelp;
    }

    if ( pGamepad->wPressedButtons & XINPUT_GAMEPAD_A )
    {   
        INT iMode = (INT)m_TrackingMode;
        ++iMode;
        iMode %= ((INT)TRACKING_MODE_COUNT);
        m_TrackingMode = (TrackingMode)iMode;
    }

    // Update the natural input device
    UpdateNUI();

    const XMVECTOR vUp = XMVectorSet( 0.0f, 1.0f, 0.0f, 0.0f );
    const XMVECTOR vPosition = XMVectorSet( 0.0f, 0.0f, -1.5f, 0.0f );
    const XMVECTOR vAt = XMVectorSet( 0.0f, 0.0f, 0.0f, 0.0f );
    m_matView = XMMatrixLookAtLH( vPosition, vAt, vUp );

    FLOAT fAspectRatio = (FLOAT)m_d3dpp.BackBufferWidth / (FLOAT)m_d3dpp.BackBufferHeight;
    m_matProj = XMMatrixPerspectiveFovLH( XM_PI / 3.0f, fAspectRatio, 0.10f, 1000.0f );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Render()
// Desc: Called once per frame, the call is the entry point for 3D rendering. This 
//       function sets up render states, clears the viewport, and renders the scene.
//--------------------------------------------------------------------------------------

HRESULT Sample::Render()
{
    // Draw a gradient filled background black 
    ATG::RenderBackground( 0xff000000, 0xff000000 );

    // Show title, frame rate, and help
    m_Timer.MarkFrame();

    m_pd3dDevice->SetRenderState( D3DRS_HALFPIXELOFFSET, TRUE);
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_ZWRITEENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_NONE );

    
    XMMATRIX matVP = m_matView * m_matProj;
    m_pd3dDevice->SetVertexShaderConstantF( 0, ( FLOAT* )&matVP, 4 );
    
    m_pd3dDevice->SetTexture( 16, m_pCopyOfDepthTextureTemporaryShouldLetNUIBufferThese );
    m_pd3dDevice->SetTexture( 0, m_pCopyOfDepthTextureTemporaryShouldLetNUIBufferThese );
    m_pd3dDevice->SetPixelShader( m_pPixelShader );    
    m_pd3dDevice->SetVertexShader( m_pVertexShader );
    m_pd3dDevice->SetIndices( g_pDepthIndBuffer );
    m_pd3dDevice->SetStreamSource( 0, g_pDepthVertBuffer, 0, sizeof( DepthBufferVertex ) );
    m_pd3dDevice->SetFVF( D3DFVF_XYZ | D3DFVF_NORMAL );
    m_pd3dDevice->DrawIndexedVertices( D3DPT_QUADLIST, 0, 0, ( g_uDepthBufferWidth - 1 ) * ( g_uDepthBufferHeight - 1 ) * 4 );
    m_pd3dDevice->SetIndices( NULL );
    m_pd3dDevice->SetStreamSource( 0, NULL, 0, 0);
    m_pd3dDevice->SetRenderState( D3DRS_HALFPIXELOFFSET, FALSE);
    m_pd3dDevice->SetTexture( 0, NULL );
    m_pd3dDevice->SetTexture( 16, NULL );

    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_ZWRITEENABLE, FALSE );

    static const FLOAT fDrawWidth = 320.0f;
    static const FLOAT fDrawHeight = 240.0f;
    static const FLOAT fDrawX = 100.0f;
    static const FLOAT fDrawY = 200.0f;
   
    m_pip.BeginRender();
    m_pip.RenderDepthStream( fDrawX, fDrawY, fDrawWidth, fDrawHeight );
    m_pip.RenderSkeletons( fDrawX, fDrawY, fDrawWidth, fDrawHeight );
    m_pip.EndRender();


    // Render the HUD
    m_Font.Begin();
    m_Font.SetScaleFactors( 1.2f, 1.2f );
    m_Font.DrawText( 0.0f, 0.0f, 0xffffffff, L"Active Passive Skeletons" );
    m_Font.SetScaleFactors( 1.0f, 1.0f );
    m_Font.DrawText( 0.0f, 0.0f, 0xffffff00, m_Timer.GetFrameRate(), ATGFONT_RIGHT );
    m_Font.DrawText( 0.0f, 30.0f, 0xFFFFFF00, g_TrackingModeText[ (INT)m_TrackingMode ], ATGFONT_LEFT );

    m_Font.End();

    if ( m_bDrawHelp )
    {
        m_Help.Render( &m_Font, g_HelpCallouts, NUM_HELP_CALLOUTS );
    }    

    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}


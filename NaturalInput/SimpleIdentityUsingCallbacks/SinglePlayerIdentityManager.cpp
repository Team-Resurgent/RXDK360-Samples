//--------------------------------------------------------------------------------------
// SinglePlayerIdentityManager.cpp
//
// Microsoft Advanced Technology Group
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------


#include <xtl.h>
#include <AtgUtil.h>  // for assert() and DebugSpew()

#include <NuiApi.h>
#include "SinglePlayerIdentityManager.h"

 
//--------------------------------------------------------------------------------------
// Name: Timeout constant values
// Desc: Defines timeout values for different identity opperations
//--------------------------------------------------------------------------------------
const DWORD IDENTIFY_TIMEOUT_IN_MS = 15 * 1000;  // 15 seconds
const DWORD ENROLL_TIMEOUT_IN_MS   = 15 * 1000;  // 15 seconds


//--------------------------------------------------------------------------------------
// Name: IsEnrollmentIndexValid
// Desc: File private helper function that returns TRUE if dwEnrollmentIndex is valid.
//--------------------------------------------------------------------------------------
inline BOOL IsEnrollmentIndexValid( DWORD dwEnrollmentIndex )
{
    return dwEnrollmentIndex < NUI_IDENTITY_MAX_ENROLLMENT_COUNT;
}


//--------------------------------------------------------------------------------------
// Name: IsUserIndexValid
// Desc: File private helper function that returns TRUE if dwUserIndex is valid.
//--------------------------------------------------------------------------------------
inline BOOL IsUserIndexValid( DWORD dwUserIndex )
{
    return dwUserIndex < XUSER_MAX_COUNT;
}


//--------------------------------------------------------------------------------------
// Name: SinglePlayerIdentityManager::SinglePlayerIdentityManager()
// Desc: Ensures the object is correctly constructed.
//--------------------------------------------------------------------------------------
SinglePlayerIdentityManager::SinglePlayerIdentityManager()
: m_uliTimeStamp( 0 )
{
    InitializeCriticalSection( &m_cs );

    ZeroMemory( m_SkeletonIdentityInfo, sizeof( m_SkeletonIdentityInfo ) );
}


//--------------------------------------------------------------------------------------
// Name: SinglePlayerIdentityManager::~SinglePlayerIdentityManager()
// Desc: Ensures resources a freed properly.
//--------------------------------------------------------------------------------------
SinglePlayerIdentityManager::~SinglePlayerIdentityManager()
{
    DeleteCriticalSection( &m_cs );
}


//--------------------------------------------------------------------------------------
// Name: SinglePlayerIdentityManager::Update()
// Desc: Processes identity for each tracked skeleton. Identify skeletons that need to 
//       be identified and enroll skeletons that have a matching biometric profile and 
//       for which the player has shown intent to play. It synthesizes the state of each 
//       fully tracked skeleton based on received skeleton data and the class’ internal 
//       state into a few well defined states.
//
// Note: Call this function everytime a new NUI skeleton frame is available.
//--------------------------------------------------------------------------------------
VOID SinglePlayerIdentityManager::Update( const NUI_SKELETON_FRAME* pSkeletonFrame )
{
    assert( pSkeletonFrame != NULL );

    EnterCriticalSection( &m_cs );

    // Using the timestamp associated with the skeleton frame makes it easier to single step through 
    // the code using XStudio
    m_uliTimeStamp = pSkeletonFrame->liTimeStamp.QuadPart;

    // Process all fully tracked skeletons.
    assert( _countof( pSkeletonFrame->SkeletonData ) == NUI_SKELETON_COUNT );
    for( DWORD dwSkeletonIndex = 0; dwSkeletonIndex < NUI_SKELETON_COUNT; ++ dwSkeletonIndex )
    {
        const NUI_SKELETON_DATA* pSkeletonData = &pSkeletonFrame->SkeletonData[ dwSkeletonIndex ];
        SKELETON_IDENTITY_INFO*  pIdentityInfo = &m_SkeletonIdentityInfo[ dwSkeletonIndex ];

        if( pSkeletonData->eTrackingState == NUI_SKELETON_TRACKED )
        {
            // Reset internal data if this isn't the skeleton we were tracking
            if( pIdentityInfo->dwTrackingID != pSkeletonData->dwTrackingID )
	        {
                ZeroMemory( pIdentityInfo, sizeof( SKELETON_IDENTITY_INFO ) );
                pIdentityInfo->dwTrackingID = pSkeletonData->dwTrackingID;
            }
            
            // Process identity for this skeleton
            // We rely on the enrollment index as we can't trust our internal state to be in perfect sync 
            // in all situations. Something outside this class ( the title or the Guide) could initiate 
            // NuiIdentity operations causing our internal state data to be out of sync and unreliable.  
            switch( pSkeletonData->dwEnrollmentIndex )
            {
                // Identify any skeleton that needs to be identified
                case NUI_IDENTITY_ENROLLMENT_INDEX_CALL_IDENTIFY:
                {
                    pIdentityInfo->uliOperationEndTimeInMs = pIdentityInfo->bShownIntentToPlay ? 
                                                                 m_uliTimeStamp + IDENTIFY_TIMEOUT_IN_MS : MAXULONGLONG;

                    HRESULT hr = NuiIdentityIdentify( pSkeletonData->dwTrackingID,                   // Identify this specific skeleton
                                                      0,                                             // Flags, reserved - must be 0
                                                      SinglePlayerIdentityManager::IdentityCallback, // NuiIdentity will call this function to report progress
                                                      this );                       

                    if( hr == E_PENDING )
                    {
                        ATG::DebugSpew( "SinglePlayerIdentityManager - TrackingID: 0x%lx - Call to NuiIdentityIdentify() succeeded\n",
                            pSkeletonData->dwTrackingID );

        	            pIdentityInfo->eState = SKELETON_IDENTITY_STATE_BUSY;
                    }
                    else
                    {
                        // This is an unexpected situation. IdentityManager will try again next time around.
                        ATG::DebugSpew( "SinglePlayerIdentityManager - TrackingID: 0x%lx - Call to NuiIdentityIdentify() failed with 0x%lx\n",
                            pSkeletonData->dwTrackingID, hr );

        	            pIdentityInfo->eState = SKELETON_IDENTITY_STATE_INVALID;
                    }
                    break;
                }

                // Enroll an identified player that has a matching biometric profile and has show intent to play.
                // Basically, we want to enroll once the user has shown intent to play and we know who they are.
                // The act of enrolling is where the biometric sign-in is triggered.
                case NUI_IDENTITY_ENROLLMENT_INDEX_UNKNOWN:
                {
                    if( pIdentityInfo->bShownIntentToPlay && pIdentityInfo->bHasProfileMatch )
	                {
                        pIdentityInfo->uliOperationEndTimeInMs = m_uliTimeStamp + ENROLL_TIMEOUT_IN_MS;

                        HRESULT hr = NuiIdentityEnroll( pSkeletonData->dwTrackingID,                     // Enroll this specific skeleton
                                                        0,                                               // Ignored, unless force-enrolling
                                                        NUI_IDENTITY_ENROLL_SKELETON,                    // Enroll using skeleton data
                                                        SinglePlayerIdentityManager::IdentityCallback,   // NuiIdentity will call this function to report progress
                                                        this );                       

                        if( hr == E_PENDING )
                        {
                            ATG::DebugSpew( "SinglePlayerIdentityManager - TrackingID: 0x%lx - Call to NuiIdentityEnroll() succeeded\n",
                                pSkeletonData->dwTrackingID );

            	            pIdentityInfo->eState = SKELETON_IDENTITY_STATE_BUSY;
                        }
                        else
                        {
                            // This is an unexpected situation. IdentityManager will try again next time around.
                            // Meanwhile we treat this player as a Guest since an identify attemps has been 
                            // made (and even succeeded)
                            ATG::DebugSpew( "SinglePlayerIdentityManager - TrackingID: 0x%lx - Call to NuiIdentityEnroll() failed with 0x%lx\n",
                                pSkeletonData->dwTrackingID, hr );

        	                pIdentityInfo->eState = SKELETON_IDENTITY_STATE_GUEST;
                        }
                    }
                    else
                    {
       	                pIdentityInfo->eState = SKELETON_IDENTITY_STATE_GUEST;
                    }

                    break;
                }

                // A player that couldn't be identified is a guest.
                case NUI_IDENTITY_ENROLLMENT_INDEX_FAILURE:
		            pIdentityInfo->eState = SKELETON_IDENTITY_STATE_GUEST;
                    break;

                case NUI_IDENTITY_ENROLLMENT_INDEX_BUSY:
		            pIdentityInfo->eState = SKELETON_IDENTITY_STATE_BUSY;
                    break;

                // If the player is enrolled...
                default:
                {
                    assert( IsEnrollmentIndexValid( pSkeletonData->dwEnrollmentIndex ) );
    		
                    // The player is signed-in if there is a valid user index associated to the skeleton.
                    if( IsUserIndexValid( pSkeletonData->dwUserIndex ) )
	    	        {
		    	        pIdentityInfo->eState = SKELETON_IDENTITY_STATE_SIGNED_IN;
		            }
                    // If the  player is enrolled and has a matching profile, we know identity is in the process of signing-in
		            else if( pIdentityInfo->bHasProfileMatch ) 
		            {
                        // User is being signed-in. The state will be set to SKELETON_IDENTITY_STATE_SIGNED_IN in a future 
                        // call to Update once the user index becomes valid.
			            pIdentityInfo->eState = SKELETON_IDENTITY_STATE_BUSY;
		            }
                    // A player enroll without a matching profile is being managed outside of this class.
		            else
		            {
                        // This situation can only happen if a title directly calls the NuiIdentity API to enroll or
                        // force enroll a player.
                        pIdentityInfo->eState = SKELETON_IDENTITY_STATE_GUEST;
		            }
                }
            }
        }
        // We invalidate the data for skeletons that aren't tracked.
        else
        {
            if( pIdentityInfo->dwTrackingID != NUI_SKELETON_INVALID_TRACKING_ID )
	        {
                ZeroMemory( pIdentityInfo, sizeof( SKELETON_IDENTITY_INFO ) );
            }
        }
    }

    LeaveCriticalSection( &m_cs );
}


//--------------------------------------------------------------------------------------
// Name: SinglePlayerIdentityManager::PlayerHasShownIntentToPlay()
// Desc: Indicates the player wants to engage with the title. If the player is 
//       identified and has a matching biometric profile, he or she will be 
//       automatically enrolled.
//
//       Returns S_OK or E_NUI_IDENTITY_LOST_TRACK if no data matches dwTrackingID.
//--------------------------------------------------------------------------------------
HRESULT SinglePlayerIdentityManager::PlayerHasShownIntentToPlay( DWORD dwTrackingID )
{
    assert( dwTrackingID != NUI_SKELETON_INVALID_TRACKING_ID );

    HRESULT hResult = E_NUI_IDENTITY_LOST_TRACK;

    EnterCriticalSection( &m_cs );

    SKELETON_IDENTITY_INFO* pSkeletonInfo = const_cast< SKELETON_IDENTITY_INFO* >( GetSkeletonInfoFromTrackingID( dwTrackingID ) );
    if( pSkeletonInfo )
    {
        // If an identification operation was kicked off before the player had shown intent 
        // to play, then it has an infinite timeout. Ensure the operation will timeout after 
        //the appropriate amount of time has elapsed.
        if( pSkeletonInfo->eState ==  SKELETON_IDENTITY_STATE_BUSY )
        {
            pSkeletonInfo->uliOperationEndTimeInMs = m_uliTimeStamp + IDENTIFY_TIMEOUT_IN_MS;
        }

        pSkeletonInfo->bShownIntentToPlay = TRUE;
        hResult = S_OK;
    }

    LeaveCriticalSection( &m_cs );
    return hResult;
}


//--------------------------------------------------------------------------------------
// Name: SinglePlayerIdentityManager::GetPlayerState()
// Desc: Retrieves the current player state for the player specified by dwTrackingID
//
//       Returns S_OK or E_NUI_IDENTITY_LOST_TRACK if no data matches dwTrackingID.
//--------------------------------------------------------------------------------------
HRESULT SinglePlayerIdentityManager::GetPlayerState( DWORD dwTrackingID, SKELETON_IDENTITY_STATE* pState ) const
{
    assert( dwTrackingID != NUI_SKELETON_INVALID_TRACKING_ID );

    HRESULT hResult = E_NUI_IDENTITY_LOST_TRACK;

    EnterCriticalSection( &m_cs );

    const SKELETON_IDENTITY_INFO* pSkeletonInfo = GetSkeletonInfoFromTrackingID( dwTrackingID );
    if( pSkeletonInfo )
    {
        *pState = pSkeletonInfo->eState;
        hResult = S_OK;
    }

    LeaveCriticalSection( &m_cs );
    return hResult;
}


//--------------------------------------------------------------------------------------
// Name: SinglePlayerIdentityManager::GetQualityFlags()
// Desc: Retrieves the current quality flags for the player specified by dwTrackingID.
//
//       Returns S_OK or E_NUI_IDENTITY_LOST_TRACK if no data matches dwTrackingID.
//--------------------------------------------------------------------------------------
HRESULT SinglePlayerIdentityManager::GetQualityFlags( DWORD dwTrackingID, DWORD* pdwQualityFlags ) const
{
    assert( dwTrackingID != NUI_SKELETON_INVALID_TRACKING_ID );

    HRESULT hResult = E_NUI_IDENTITY_LOST_TRACK;

    EnterCriticalSection( &m_cs );

    const SKELETON_IDENTITY_INFO* pSkeletonInfo = GetSkeletonInfoFromTrackingID( dwTrackingID );
    if( pSkeletonInfo )
    {
        *pdwQualityFlags = pSkeletonInfo->dwQualityFlags;
        hResult = S_OK;
    }

    LeaveCriticalSection( &m_cs );
    return hResult;
}


//--------------------------------------------------------------------------------------
// Name: SinglePlayerIdentityManager::GetSkeletonInfoFromTrackingID()
// Desc: Returns a const pointer to an internal SKELETON_IDENTITY_INFO structure. It 
//       returns NULL if no corresponding structure is found. 
//--------------------------------------------------------------------------------------
const SKELETON_IDENTITY_INFO* SinglePlayerIdentityManager::GetSkeletonInfoFromTrackingID( DWORD dwTrackingID ) const
{
    assert( dwTrackingID != NUI_SKELETON_INVALID_TRACKING_ID );

    const SKELETON_IDENTITY_INFO* pSkeletonIdentityInfo = NULL;

    EnterCriticalSection( &m_cs );

    for(DWORD i = 0; i < _countof( m_SkeletonIdentityInfo ); ++ i )
    {
        if( m_SkeletonIdentityInfo[ i ].dwTrackingID == dwTrackingID )
        {
            pSkeletonIdentityInfo = &m_SkeletonIdentityInfo[ i ];
            break;
        }
    }
    
    LeaveCriticalSection( &m_cs );
    return pSkeletonIdentityInfo;
}


//--------------------------------------------------------------------------------------
// Name: SinglePlayerIdentityManager::IdentityCallback
// Desc: Callback used to obtain feedback from calls to NuiIdentityIdentify() and 
//       NuiIdentityEnroll().
//--------------------------------------------------------------------------------------
BOOL SinglePlayerIdentityManager::IdentityCallback( PVOID pvContext, NUI_IDENTITY_MESSAGE* pMessage )
{
    assert( pvContext != NULL );
    assert( pMessage != NULL );

    BOOL bContinue = TRUE;

    SinglePlayerIdentityManager* pIdentityManager = ( SinglePlayerIdentityManager* ) pvContext;

    EnterCriticalSection( &pIdentityManager->m_cs );

    SKELETON_IDENTITY_INFO* pSkeletonInfo = const_cast< SKELETON_IDENTITY_INFO* >( 
                                                pIdentityManager->GetSkeletonInfoFromTrackingID( pMessage->dwTrackingID ) );

    if( pSkeletonInfo )
    {
        switch( pMessage->MessageId )
        {
            case NUI_IDENTITY_MESSAGE_ID_FRAME_PROCESSED:
            {
                pSkeletonInfo->dwQualityFlags = pMessage->Data.FrameProcessed.dwQualityFlags;

                if( pIdentityManager->m_uliTimeStamp > pSkeletonInfo->uliOperationEndTimeInMs )
                {
                    bContinue = FALSE;
                }

                break;
            }

            case NUI_IDENTITY_MESSAGE_ID_COMPLETE:
            {
                if( SUCCEEDED( pMessage->Data.Complete.hrResult ) )
                {
                    ATG::DebugSpew( "SinglePlayerIdentityManager - TrackingID: 0x%lx - Identity operation has completed succeessfully\n",
                        pMessage->dwTrackingID );
                }
                else
                {
                    ATG::DebugSpew( "SinglePlayerIdentityManager - TrackingID: 0x%lx - Identity operation has completed with 0x%lx\n",
                        pMessage->dwTrackingID, pMessage->Data.Complete.hrResult );
                }

                pSkeletonInfo->bHasProfileMatch = pMessage->Data.Complete.bProfileMatched;
                break;
            }

            default:
            {
                assert( false );
            }
        }

    }
    else
    {
        ATG::DebugSpew( "SinglePlayerIdentityManager - TrackingID: 0x%lx - Identity operation was terminated because the skeleton was lost.\n",
            pMessage->dwTrackingID );
    }

    LeaveCriticalSection( &pIdentityManager->m_cs );
    return bContinue;
}
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
{
    ZeroMemory( m_SkeletonIdentityInfo, sizeof( m_SkeletonIdentityInfo ) );
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
            
            // Update the class' identity state based on the latest skeleton data
            switch( pSkeletonData->dwEnrollmentIndex )
            {
                case NUI_IDENTITY_ENROLLMENT_INDEX_CALL_IDENTIFY:
                    // Should never happen in automatic mode
		            pIdentityInfo->eState = SKELETON_IDENTITY_STATE_INVALID;
                    break;

                case NUI_IDENTITY_ENROLLMENT_INDEX_UNKNOWN:
                case NUI_IDENTITY_ENROLLMENT_INDEX_FAILURE:
                    // A player that couldn't be assoiated to a enrollment index is a guest.
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
                    // Otherwise, the  player is enrolled and we know identity is in the process of signing-in
		            else
		            {
                        // User is being signed-in. The state will be set to SKELETON_IDENTITY_STATE_SIGNED_IN in a future 
                        // call to Update once the user index becomes valid.
			            pIdentityInfo->eState = SKELETON_IDENTITY_STATE_BUSY;
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

    RetrieveIdentityFeedback();
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

    return NuiIdentityDetectedIntentToPlay( dwTrackingID );
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

    const SKELETON_IDENTITY_INFO* pSkeletonInfo = GetSkeletonInfoFromTrackingID( dwTrackingID );
    if( pSkeletonInfo )
    {
        *pState = pSkeletonInfo->eState;
        hResult = S_OK;
    }

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

    const SKELETON_IDENTITY_INFO* pSkeletonInfo = GetSkeletonInfoFromTrackingID( dwTrackingID );
    if( pSkeletonInfo )
    {
        *pdwQualityFlags = pSkeletonInfo->dwQualityFlags;
        hResult = S_OK;
    }

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

    for( DWORD i = 0; i < _countof( m_SkeletonIdentityInfo ); ++ i )
    {
        if( m_SkeletonIdentityInfo[ i ].dwTrackingID == dwTrackingID )
        {
            pSkeletonIdentityInfo = &m_SkeletonIdentityInfo[ i ];
            break;
        }
    }
    
    return pSkeletonIdentityInfo;
}


//--------------------------------------------------------------------------------------
// Name: SinglePlayerIdentityManager::RetrieveIdentityFeedback
// Desc: Retreive the latest identity states 
//--------------------------------------------------------------------------------------
HRESULT SinglePlayerIdentityManager::RetrieveIdentityFeedback()
{
    NUI_IDENTITY_MESSAGE Message;
    
    // We retrieve every message from the identity message queue
    while( NuiIdentityGetNextMessage( &Message ) == S_OK )
    {
        SKELETON_IDENTITY_INFO* pSkeletonInfo = const_cast< SKELETON_IDENTITY_INFO* >( 
                                                const_cast< const SinglePlayerIdentityManager* >( this )->GetSkeletonInfoFromTrackingID( Message.dwTrackingID ) );

        if( pSkeletonInfo )
        {
            switch( Message.MessageId )
            {
                case NUI_IDENTITY_MESSAGE_ID_FRAME_PROCESSED:
                {
                    pSkeletonInfo->dwQualityFlags = Message.Data.FrameProcessed.dwQualityFlags;
                    break;
                }

                case NUI_IDENTITY_MESSAGE_ID_COMPLETE:
                {
                    if( SUCCEEDED( Message.Data.Complete.hrResult ) )
                    {   
                        ATG::DebugSpew( "SinglePlayerIdentityManager - TrackingID: 0x%lx - Identity operation has completed succeessfully\n",
                            Message.dwTrackingID );
                    }
                    else
                    {
                        ATG::DebugSpew( "SinglePlayerIdentityManager - TrackingID: 0x%lx - Identity operation has completed with 0x%lx\n",
                            Message.dwTrackingID, Message.Data.Complete.hrResult );
                    }

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
                Message.dwTrackingID );
        }
    }

    return S_OK;
}
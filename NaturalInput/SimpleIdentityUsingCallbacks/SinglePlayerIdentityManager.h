//--------------------------------------------------------------------------------------
// SinglePlayerIdentityManager.h
//
// Microsoft Advanced Technology Group
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

#ifndef SINGLE_PLAYER_IDENTITY_MANAGER_H_
#define SINGLE_PLAYER_IDENTITY_MANAGER_H_

#include <NuiApi.h>


//--------------------------------------------------------------------------------------
// Name: enum SKELETON_IDENTITY_STATE
// Desc: Defines the identity states for individual skeletons
//--------------------------------------------------------------------------------------
enum SKELETON_IDENTITY_STATE
{
    SKELETON_IDENTITY_STATE_INVALID = 0,       // Player is not identified.
    SKELETON_IDENTITY_STATE_BUSY,              // Identity operation in progress for this player
    SKELETON_IDENTITY_STATE_GUEST,             // Player is either unknown without a matching biometric profile or
                                               // the identification operation has failed.
    SKELETON_IDENTITY_STATE_SIGNED_IN          // Player is signed-in
};


//--------------------------------------------------------------------------------------
// Name: struct SKELETON_IDENTITY_INFO
// Desc: Per skeleton information used internally to manage player identification.
//--------------------------------------------------------------------------------------
struct SKELETON_IDENTITY_INFO
{
    DWORD dwTrackingID;       // Last known trackingID
    DWORD dwQualityFlags;     // Latest quality flags from the identification or enrollment operation in progress.

    BOOL bShownIntentToPlay;  // TRUE if the player has shown intent to play.
    BOOL bHasProfileMatch;    // TRUE if the player has a biometric profile that will be automatically signed-in
                              // if they show intent to play. 

    ULONGLONG uliOperationEndTimeInMs; // Absolute time at which the operation should be cancelled.
                                       // The time is based on the skeleton frame time.

    SKELETON_IDENTITY_STATE eState; // Player state computed by the most recent call to Update()
};


//--------------------------------------------------------------------------------------
// Name: class SinglePlayerIdentityManager
// Desc: IdentityManager will automatically launch identification operations for any new 
//       players and will automatically enroll any player who has shown intent to play 
//       and has a biometric profile.
//
// Usage: Call Update every time a new NUI_SKELETON_FRAME data structure is available 
//        from NUI.
//        Use the various GET memeber functions to retrieve the state of the player's 
//        identity.
//--------------------------------------------------------------------------------------
class SinglePlayerIdentityManager
{
public:
    SinglePlayerIdentityManager();
    ~SinglePlayerIdentityManager();

    VOID Update( const NUI_SKELETON_FRAME* pSkeletonFrame );

    HRESULT PlayerHasShownIntentToPlay( DWORD dwTrackingID );
    
    HRESULT GetPlayerState( DWORD dwTrackingID, SKELETON_IDENTITY_STATE* pState ) const;
    HRESULT GetQualityFlags( DWORD dwTrackingID, DWORD* dwQualityFlags ) const;


private:
    SinglePlayerIdentityManager( const SinglePlayerIdentityManager& rhs );
    SinglePlayerIdentityManager& operator =( const SinglePlayerIdentityManager& rhs );

    const SKELETON_IDENTITY_INFO* GetSkeletonInfoFromTrackingID( DWORD dwTrackingID ) const;

    static BOOL IdentityCallback( PVOID pvContext, NUI_IDENTITY_MESSAGE* pMessage );


    // We need to ensure the thread safety of this class because two entities will be calling functions on the same
    // instance of this class from different threads.  NuiIdentity uses callback functions to communicate feedback 
    // during asynchronous operations. Like all NUI operations, NuiIdentity operations run on the hardware thread 
    // specified by the title in the call to NuiInitialize(). Thus the need for thread safety: NuiIdentity will call
    // the title’s callback functions from this NUI thread while the title will be managing its identity state on 
    // another thread, usually the title’s main thread.
    mutable CRITICAL_SECTION m_cs;      // Class wide critical section to allow safe access from different threads.

    ULONGLONG m_uliTimeStamp;            // Time stamp from the last skeleton frame pass to Update().
    SKELETON_IDENTITY_INFO m_SkeletonIdentityInfo[ NUI_SKELETON_COUNT ];
};


#endif // SINGLE_PLAYER_IDENTITY_MANAGER_H_
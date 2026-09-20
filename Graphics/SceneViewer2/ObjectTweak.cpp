//--------------------------------------------------------------------------------------
// ObjectTweak.cpp
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include <assert.h>
#include "ObjectTweak.h"
#include "SceneViewer2.h"

ObjectTweaker::ObjectTweaker() : m_pScene( NULL ),
                                 m_iSelectedObjectIndex( -1 ),
                                 m_pSceneViewer( NULL ),
                                 m_dwFilterIndex( 0 ),
                                 m_bActive( FALSE ),
                                 m_bMovementActive( FALSE ),
                                 m_pSelectedObject( NULL )
{
    m_ObjectFilters.push_back( ATG::NamedTypedObject::TypeID );
    m_ObjectFilters.push_back( ATG::Frame::TypeID );
    m_ObjectFilters.push_back( ATG::Light::TypeID );
    m_ObjectFilters.push_back( ATG::PointLight::TypeID );
    m_ObjectFilters.push_back( ATG::SpotLight::TypeID );
    m_ObjectFilters.push_back( ATG::Camera::TypeID );
    m_ObjectFilters.push_back( ATG::Model::TypeID );
    m_ObjectFilters.push_back( ATG::Resource::TypeID );
}

VOID ObjectTweaker::Initialize( ATG::Scene* pScene, SceneViewer* pSceneViewer )
{
    m_bActive = FALSE;
    m_pScene = pScene;
    m_pSceneViewer = pSceneViewer;
    m_dwFilterIndex = 0;
    SelectObject( 0 );
    m_bMovementActive = FALSE;
}

VOID ObjectTweaker::SelectObject( INT iIndex )
{
    if( iIndex < 0 || m_pScene == NULL )
    {
        m_iSelectedObjectIndex = -1;
        m_pSelectedObject = NULL;
        return;
    }
    ATG::NameIndexedCollection::iterator i;
    INT iCurrentIndex = iIndex;
    for( i = m_pScene->GetInstanceList()->begin(); i != m_pScene->GetInstanceList()->end(); i++ )
    {
        if( iCurrentIndex == 0 )
        {
            m_iSelectedObjectIndex = iIndex;
            m_pSelectedObject = *i;
            return;
        }
        --iCurrentIndex;
    }
    m_iSelectedObjectIndex = -1;
    m_pSelectedObject = NULL;
}

VOID ObjectTweaker::IncrementSelectedObject()
{
    INT iStartIndex = m_iSelectedObjectIndex;
    if( iStartIndex < 0 )
        iStartIndex = 0;
    INT iObjectCount = ( INT )m_pScene->GetInstanceList()->Size();
    ATG::StringID FilterType = m_ObjectFilters[ m_dwFilterIndex ];
    do
    {
        m_iSelectedObjectIndex = ( m_iSelectedObjectIndex + 1 ) % iObjectCount;
        SelectObject( m_iSelectedObjectIndex );
        assert( m_pSelectedObject != NULL );
    } while( m_iSelectedObjectIndex != iStartIndex && !m_pSelectedObject->IsDerivedFrom( FilterType ) );

    if( m_pSelectedObject != NULL && !m_pSelectedObject->IsDerivedFrom( FilterType ) )
    {
        m_pSelectedObject = NULL;
    }
}

VOID ObjectTweaker::DecrementSelectedObject()
{
    INT iStartIndex = m_iSelectedObjectIndex;
    if( iStartIndex < 0 )
        iStartIndex = 0;
    INT iObjectCount = ( INT )m_pScene->GetInstanceList()->Size();
    ATG::StringID FilterType = m_ObjectFilters[ m_dwFilterIndex ];
    do
    {
        m_iSelectedObjectIndex = ( m_iSelectedObjectIndex - 1 + iObjectCount ) % iObjectCount;
        SelectObject( m_iSelectedObjectIndex );
        assert( m_pSelectedObject != NULL );
    } while( m_iSelectedObjectIndex != iStartIndex && !m_pSelectedObject->IsDerivedFrom( FilterType ) );

    if( m_pSelectedObject != NULL && !m_pSelectedObject->IsDerivedFrom( FilterType ) )
    {
        m_pSelectedObject = NULL;
    }
}

VOID ObjectTweaker::IncrementFilter()
{
    DWORD dwFilterCount = m_ObjectFilters.size();
    m_dwFilterIndex = ( m_dwFilterIndex + 1 + dwFilterCount ) % dwFilterCount;
    ATG::StringID FilterType = m_ObjectFilters[ m_dwFilterIndex ];
    if( m_pSelectedObject == NULL || !m_pSelectedObject->IsDerivedFrom( FilterType ) )
    {
        IncrementSelectedObject();
    }
}

VOID ObjectTweaker::DecrementFilter()
{
    DWORD dwFilterCount = m_ObjectFilters.size();
    m_dwFilterIndex = ( m_dwFilterIndex + dwFilterCount - 1 ) % dwFilterCount;
    ATG::StringID FilterType = m_ObjectFilters[ m_dwFilterIndex ];
    if( m_pSelectedObject == NULL || !m_pSelectedObject->IsDerivedFrom( FilterType ) )
    {
        IncrementSelectedObject();
    }
}

VOID ObjectTweaker::Update( ATG::GAMEPAD* pGamepad, FLOAT fDeltaTime, FLOAT fAppTime )
{
    // No scene - no action.
    if( m_pScene == NULL )
    {
        SelectObject( -1 );
        return;
    }

    m_fAppTime = fAppTime;

    if( pGamepad == NULL )
        return;

    // Turn the tweaker on with the B button.
    if( !IsVisible() )
    {
        if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_B )
        {
            m_bMovementActive = FALSE;
            m_bActive = TRUE;
        }
        return;
    }

    // Turn the tweaker off with the B button.
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_B )
    {
        m_bActive = FALSE;
        return;
    }

    // Open the help screen with the Back button.
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_BACK )
    {
        m_bActive = FALSE;
        m_pSceneViewer->m_SettingsPanel.ShowMenuItem( 0, 2 );
        pGamepad->wPressedButtons &= ~XINPUT_GAMEPAD_BACK;
        return;
    }

    // The dpad selects objects and changes the object filter.
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_DPAD_UP )
    {
        DecrementSelectedObject();
    }
    else if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_DPAD_DOWN )
    {
        IncrementSelectedObject();
    }
    else if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_DPAD_RIGHT )
    {
        IncrementFilter();
    }
    else if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_DPAD_LEFT )
    {
        DecrementFilter();
    }

    // The X button enables and disables frame transform manipulation.
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_X )
    {
        m_bMovementActive = !m_bMovementActive;
    }

    if( m_pSelectedObject == NULL )
        return;

    if( m_bMovementActive && m_pSelectedObject->IsDerivedFrom( ATG::Frame::TypeID ) )
    {
        // Adjust frame transforms using the analog sticks.
        ATG::Frame* pFrame = ( ATG::Frame* )m_pSelectedObject;
        m_pSceneViewer->MoveFrame( pGamepad, fDeltaTime, pFrame );
    }
    if( m_pSelectedObject->IsDerivedFrom( ATG::Light::TypeID ) )
    {
        // Enable and disable lights using the A button.
        ATG::Light* pLight = ( ATG::Light* )m_pSelectedObject;
        BOOL bDisabled = pLight->TestFlag( ATG::Light::IsDisabled );
        if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_A )
        {
            bDisabled = !bDisabled;
            if( bDisabled )
                pLight->SetFlag( ATG::Light::IsDisabled );
            else
                pLight->ClearFlag( ATG::Light::IsDisabled );
        }
    }
    if( !m_bMovementActive )
    {
        // Grow and shrink light parameters using the triggers.
        FLOAT fShrink = ( FLOAT )pGamepad->bLeftTrigger / 255.0f;
        FLOAT fGrow = ( FLOAT )pGamepad->bRightTrigger / 255.0f;
        FLOAT fChange = ( fGrow - fShrink ) * fDeltaTime;
        // 1 meter per second distance change rate.
        const FLOAT fDistanceChangeRate = 1.0f;
        // 10 degrees per second angle change rate.
        const FLOAT fAngleChangeRate = 0.1745f;

        if( m_pSelectedObject->IsDerivedFrom( ATG::PointLight::TypeID ) )
        {
            // Change the range of the point light.
            ATG::PointLight* pPoint = ( ATG::PointLight* )m_pSelectedObject;
            FLOAT fRange = pPoint->GetWorldRange() + fChange * fDistanceChangeRate;
            if( fRange < 0 )
                fRange = 0;
            pPoint->SetWorldRange( fRange );
        }
        if( m_pSelectedObject->IsDerivedFrom( ATG::SpotLight::TypeID ) )
        {
            ATG::SpotLight* pSpot = ( ATG::SpotLight* )m_pSelectedObject;
            if( pGamepad->wLastButtons & XINPUT_GAMEPAD_LEFT_SHOULDER )
            {
                // Change inner angle.
                FLOAT fAngle = pSpot->GetInnerAngle() + fChange * fAngleChangeRate;
                if( fAngle < 0 )
                    fAngle = 0;
                if( fAngle > pSpot->GetOuterAngle() )
                    fAngle = pSpot->GetOuterAngle();
                pSpot->SetInnerAngle( fAngle );
            }
            else if( pGamepad->wLastButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER )
            {
                // Change outer angle, and set inner angle as a proportion of the
                // new outer angle.
                FLOAT fRatio = pSpot->GetInnerAngle() / pSpot->GetOuterAngle();
                FLOAT fAngle = pSpot->GetOuterAngle() + fChange * fAngleChangeRate;
                if( fAngle < 0 )
                    fAngle = 0;
                if( fAngle > XM_PI )
                    fAngle = XM_PI;
                pSpot->SetOuterAngle( fAngle );
                pSpot->SetInnerAngle( fAngle * fRatio );
            }
            else
            {
                // Change the range of the spotlight.
                FLOAT fRange = pSpot->GetWorldRange() + fChange * fDistanceChangeRate;
                if( fRange < 0 )
                    fRange = 0;
                pSpot->SetWorldRange( fRange );
            }
        }
    }
}

VOID ObjectTweaker::Render( ATG::Font* pFont )
{
    if( !IsVisible() )
        return;

    pFont->SetScaleFactors( 0.7f, 0.7f );

    FLOAT fYPos = 300;
    const FLOAT fLineSpacing = 20.0f;

    // Display the object filter.
    WCHAR strText[300];
    swprintf_s( strText, L"Filter: < %s >", m_ObjectFilters[m_dwFilterIndex].GetSafeString() );
    pFont->DrawText( 0, fYPos, 0xFFFFFFFF, strText, ATGFONT_RIGHT );
    fYPos += fLineSpacing;

    // Display the object name and type.
    const WCHAR* strName = L"none";
    const WCHAR* strType = L"none";
    if( m_pSelectedObject != NULL )
    {
        strName = m_pSelectedObject->GetName();
        if( wcslen( strName ) == 0 )
            strName = L"<untitled>";
        strType = m_pSelectedObject->Type();
    }
    swprintf_s( strText, L"%d: %s (%s)", m_iSelectedObjectIndex, strName, strType );
    pFont->DrawText( 0, fYPos, 0xFFFFFFFF, strText, ATGFONT_RIGHT );
    fYPos += fLineSpacing;

    if( m_pSelectedObject == NULL )
        return;

    // Display object properties based on the object's type.
    if( m_pSelectedObject->IsDerivedFrom( ATG::Frame::TypeID ) )
    {
        ATG::Frame* pFrame = ( ATG::Frame* )m_pSelectedObject;

        // Render a 3D manipulation widget for frames.
        RenderFrameWidget( pFrame );

        // Render the world position of the frame.
        XMVECTOR vWorldPos = pFrame->GetWorldPosition();
        const WCHAR* strEditing = L"";
        if( m_bMovementActive )
            strEditing = L"EDITING ";
        swprintf_s( strText, L"%sWorldPos: < %0.3f, %0.3f, %0.3f >", strEditing, vWorldPos.x, vWorldPos.y,
                    vWorldPos.z );
        pFont->DrawText( 0, fYPos, 0xFFFFC000, strText, ATGFONT_RIGHT );
        fYPos += fLineSpacing;
    }
    if( m_pSelectedObject->IsDerivedFrom( ATG::Light::TypeID ) )
    {
        ATG::Light* pLight = ( ATG::Light* )m_pSelectedObject;
        // Display the light color, and if it is enabled or disabled.
        XMVECTOR vColor = pLight->GetColor();
        const WCHAR* strDisabled = L"";
        if( pLight->TestFlag( ATG::Light::IsDisabled ) )
            strDisabled = L" (Disabled)";
        swprintf_s( strText, L"Color: < %0.2f, %0.2f, %0.2f, %0.2f >%s", vColor.x, vColor.y, vColor.z, vColor.w,
                    strDisabled );
        pFont->DrawText( 0, fYPos, 0xFFFFF000, strText, ATGFONT_RIGHT );
        fYPos += fLineSpacing;
    }
    if( m_pSelectedObject->IsDerivedFrom( ATG::PointLight::TypeID ) )
    {
        ATG::PointLight* pPointLight = ( ATG::PointLight* )m_pSelectedObject;
        // Display the point light range.
        swprintf_s( strText, L"Range: %0.2f", pPointLight->GetWorldRange() );
        pFont->DrawText( 0, fYPos, 0xFFFFF000, strText, ATGFONT_RIGHT );
        fYPos += fLineSpacing;

        XMFLOAT3 Center;
        XMStoreFloat3( &Center, pPointLight->GetWorldPosition() );

        ATG::DebugDraw::DrawSphere( Center, pPointLight->GetWorldRange(), pPointLight->GetD3DColor() );
    }
    if( m_pSelectedObject->IsDerivedFrom( ATG::SpotLight::TypeID ) )
    {
        ATG::SpotLight* pSpotLight = ( ATG::SpotLight* )m_pSelectedObject;

        // Display the spot light inner and outer angles.
        FLOAT fInnerAngleDeg = pSpotLight->GetInnerAngle() * 180.0f * XM_1DIVPI;
        FLOAT fOuterAngleDeg = pSpotLight->GetOuterAngle() * 180.0f * XM_1DIVPI;
        swprintf_s( strText, L"Inner: %0.1f Outer: %0.1f Range: %0.2f", fInnerAngleDeg, fOuterAngleDeg,
                    pSpotLight->GetWorldRange() );
        pFont->DrawText( 0, fYPos, 0xFFFFF000, strText, ATGFONT_RIGHT );
        fYPos += fLineSpacing;

        FLOAT fRange = 1.0f;
        FLOAT fOuterAngle = pSpotLight->GetOuterAngle() * 0.5f;
        XMVECTOR vDir = pSpotLight->GetWorldDirection();

        FLOAT fTopRadius = 1000.0f;
        if( fOuterAngle < XM_PIDIV2 )
            fTopRadius = tanf( fOuterAngle ) * fRange;

        XMFLOAT3 Pos;
        XMStoreFloat3( &Pos, pSpotLight->GetWorldPosition() );
        XMFLOAT3 Axis;
        XMStoreFloat3( &Axis, XMVectorScale( vDir, fRange ) );

        ATG::DebugDraw::DrawConeWireframe( Pos, Axis, 0.0f, fTopRadius, pSpotLight->GetD3DColor() );
    }
    /*
      if( m_pSelectedObject->IsDerivedFrom( ATG::Material::TypeID ) )
      {
      pFont->DrawText( 0, fYPos, 0xFFFFF000, L"Press A to dump material to console.", ATGFONT_RIGHT );
      fYPos += fLineSpacing;
      }
     */
}

VOID ObjectTweaker::RenderFrameWidget( ATG::Frame* pFrame )
{
    FLOAT fRadius = pFrame->GetWorldBound().GetMaxRadius();
    if( fRadius < 0.5f )
        fRadius = 0.5f;
    if( pFrame->IsDerivedFrom( ATG::Light::TypeID ) )
        fRadius = 0.5f;

    // Draw XYZ axes at the frame location.
    XMMATRIX matWorld = pFrame->GetWorldTransform();
    FLOAT fScale = fRadius;
    // If movement updates are active, animate the axes.
    if( m_bMovementActive )
        fScale += sinf( m_fAppTime * 20.0f ) * 0.1f;
    XMMATRIX matScale = XMMatrixScaling( fScale, fScale, fScale );
    ATG::DebugDraw::DrawAxes( matScale * matWorld );

    if( !m_bMovementActive )
        return;

    // Draw rotation widget rings around the object.
    XMVECTOR vOrigin = pFrame->GetWorldPosition();
    XMVECTOR vForward = XMVector3Normalize( pFrame->GetWorldDirection() ) * fRadius;
    XMVECTOR vRight = XMVector3Normalize( pFrame->GetWorldRight() ) * fRadius;
    XMVECTOR vUp = XMVector3Normalize( pFrame->GetWorldUp() ) * fRadius;

    XMFLOAT3 Origin3, Forward3, Right3, Up3;
    XMStoreFloat3( &Origin3, vOrigin );
    XMStoreFloat3( &Forward3, vForward );
    XMStoreFloat3( &Right3, vRight );
    XMStoreFloat3( &Up3, vUp );

    ATG::DebugDraw::DrawRing( Origin3, Forward3, Up3, 0xFFFF0000 );
    ATG::DebugDraw::DrawRing( Origin3, Forward3, Right3, 0xFF00FF00 );
    ATG::DebugDraw::DrawRing( Origin3, Right3, Up3, 0xFF0000FF );
}

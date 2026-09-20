//--------------------------------------------------------------------------------------
// ObjectTweak.h
//
// This class allows for limited real-time manipulation of scene content within
// SceneViewer2.  It translates controller input into transformations on frames, and
// also allows editing of several different light parameters.
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#pragma once
#ifndef OBJECTTWEAK_H
#define OBJECTTWEAK_H

#include <xtl.h>
#include <AtgFont.h>
#include <AtgInput.h>
#include <vector>
#include "AtgSceneAll.h"

class SceneViewer;

class ObjectTweaker
{
public:
                ObjectTweaker();
    VOID        Initialize( ATG::Scene* pScene, SceneViewer* pSceneViewer );
    VOID        Update( ATG::GAMEPAD* pGamepad, FLOAT fDeltaTime, FLOAT fAppTime );
    VOID        Render( ATG::Font* pFont );
    BOOL        IsVisible() const
    {
        return m_bActive;
    }
    BOOL        HasFocus() const
    {
        return m_bActive || m_bMovementActive;
    }
    BOOL        IsMovementActive() const
    {
        return m_bMovementActive;
    }
    VOID        SelectObject( INT iIndex );
    ATG::NamedTypedObject* GetSelectedObject() const
    {
        return m_pSelectedObject;
    }
    VOID        IncrementSelectedObject();
    VOID        DecrementSelectedObject();
    VOID        IncrementFilter();
    VOID        DecrementFilter();

    static VOID MoveFrameFromGamepadInput( ATG::Frame* pFrame, ATG::GAMEPAD* pGamepad, FLOAT fDeltaTime,
                                           XMVECTOR vWorldUpVector );

private:
    VOID        RenderFrameWidget( ATG::Frame* pFrame );

private:
    BOOL m_bActive;
    BOOL m_bMovementActive;
    INT m_iSelectedObjectIndex;
    ATG::NamedTypedObject* m_pSelectedObject;
    ATG::Scene* m_pScene;
    SceneViewer* m_pSceneViewer;
    FLOAT m_fAppTime;
    std::vector <ATG::StringID> m_ObjectFilters;
    DWORD m_dwFilterIndex;
};

#endif

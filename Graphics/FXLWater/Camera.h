//--------------------------------------------------------------------------------------
// Camera.h
//
// Simple First Person Camera
//
// Microsoft Game Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

#include <xtl.h>



//--------------------------------------------------------------------------------------
// Name: Camera
// Desc: Interface definition describing cameras.  Derived classes will use this
//       interface.
//--------------------------------------------------------------------------------------
class Camera
{
public:
    virtual VOID    Update( FLOAT fElapsedTime ) = 0;
    virtual VOID    Destroy()
    {
        delete this;
    }
    virtual UINT    GetPosition( XMVECTOR& vPosition ) = 0;
    virtual UINT    GetOrientation( XMVECTOR& vQuaternion ) = 0;

    // Retrieve the current view matrix, and return a timestamp.
    // Consumers of the camera may do lengthy calculations based on whether or not
    // the view has changed.  Maintaining and updating a timestamp when it does change
    // allows client objects to avoid such calculations when not necessary.
    inline  UINT    GetViewMatrix( XMMATRIX& matView )
    {
        matView = m_matView;
        return m_nViewTimestamp;
    }
protected:
    // Private constructor - only derived classes may instantiate the base class
                    Camera() : m_nViewTimestamp( 0 )
                    {
                        m_matView = XMMatrixIdentity();
                    }

protected:
    XMMATRIX m_matView;
    UINT m_nViewTimestamp;

public:
    // Creates a First Person camera type
    static Camera* CreateFirstPerson( const XMVECTOR& fPosition,
                                      FLOAT fYaw,
                                      FLOAT fPitch,
                                      FLOAT fSpeed );

};

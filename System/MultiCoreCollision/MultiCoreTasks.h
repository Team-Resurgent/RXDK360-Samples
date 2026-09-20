//--------------------------------------------------------------------------------------
// MultiCoreTasks.h
//
// Contains specific tasks that need to be scheduled on multiple cores.
//
// XNA Developer Connection.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

const XMVECTOR  TrianglePointA =
{
    0,
    2,
    0,
    0
};

const XMVECTOR  TrianglePointB =
{
    1.732f,
    -1,
    0,
    0
};

const XMVECTOR  TrianglePointC =
{
    -1.732f,
    -1,
    0,
    0
};


//--------------------------------------------------------------------------------------
// Name: GetCollisionColor()
// Desc: Returns the correct collision color based on the collision result value and the
//       group number.  Frustum tests (group 0) return 0, 1, or 2 for outside, partially
//       inside, and fully inside; all other tests return 0 or 1 for no collision or
//       collision.
//--------------------------------------------------------------------------------------
D3DCOLOR GetCollisionColor( INT iCollisionValue,
                            INT iGroupNumber )
{
    const D3DCOLOR ColorCollide = 0xFFFF0000;
    const D3DCOLOR ColorPartialCollide = 0xFFFFFF00;
    const D3DCOLOR ColorNoCollide = 0xFF80C080;

    // special case: a value of 1 for groups 1 and higher needs to register as a full collision
    if( iGroupNumber > 0 && iCollisionValue > 0 )
        iCollisionValue = 2;

    switch( iCollisionValue )
    {
        case 0:
            return ColorNoCollide;
        case 1:
            return ColorPartialCollide;
        case 2:
        default:
            return ColorCollide;
    }
}


//--------------------------------------------------------------------------------------
// Name: MoveSphereAroundFrustumTask
// Desc: Sphere - frustrum intersection task.
//--------------------------------------------------------------------------------------
class MoveSphereAroundFrustumTask : public ATG::Task
{
public:

                    MoveSphereAroundFrustumTask() : fTime( 0.0 ),
                                                    frustum( NULL ),
                                                    sphere( NULL )
                    {
                    }

    float fTime;
    const ATG::Frustum* frustum;
    CollisionSphere* sphere;
    int count;                // Number of sphere objects to process;
    UINT* collisionCount;


    void            Initialize( const ATG::Frustum* fr,
                                CollisionSphere* sp,
                                UINT* collision,
                                UINT number )
    {
        frustum = fr;
        sphere = sp;
        count = number;
        collisionCount = collision;
    }

    void            Setup( float ft )
    {
        fTime = ft;
    }

    virtual HRESULT Start()
    {
        if( sphere != NULL )
            return S_OK;
        else
        {
            XLFLogPrint( collisionLog, "[%s] Sphere was empty\n", "MoveSphereAroundFrustumTask" );
            return E_FAIL;
        }
    }

    virtual HRESULT Process()
    {
        CollisionSphere* ptr = sphere;
        for( int i = 0; i < count; i++ )
        {
            ptr->m_Sphere.Center.x = 10 * sinf( 3 * fTime * ptr->m_offset );
            ptr->m_Sphere.Center.y = 7 * cosf( 5 * fTime * ptr->m_offset );

            ptr->m_iCollision = ATG::IntersectSphereFrustum( &( ptr->m_Sphere ), frustum );
            ptr++;
        }
        return S_OK;
    }

    virtual HRESULT Complete()
    {
        CollisionSphere* ptr = sphere;
        for( int i = 0; i < count; i++ )
        {
            if( ptr->m_iCollision )
            {
                ATG::DebugDraw::DrawSphere( ptr->m_Sphere, GetCollisionColor( ptr->m_iCollision, 0 ) );
                ( *collisionCount )++;
            }
            ptr++;
        }
        return S_OK;
    }
};


//--------------------------------------------------------------------------------------
// Name: MoveOrientedBoxAroundFrustumTask
// Desc: Oriented box - frustrum intersection task.
//--------------------------------------------------------------------------------------
class MoveOrientedBoxAroundFrustumTask : public ATG::Task
{
public:
                    MoveOrientedBoxAroundFrustumTask() : fTime( 0.0 ),
                                                         frustum( NULL ),
                                                         box( NULL )
                    {
                    }

    float fTime;
    const ATG::Frustum* frustum;
    CollisionBox* box;
    int count;
    UINT* collisionCount;

    void            Initialize( const ATG::Frustum* fr,
                                CollisionBox* sp,
                                UINT* collision,
                                UINT number )
    {
        frustum = fr;
        box = sp;
        count = number;
        collisionCount = collision;
    }

    void            Setup( float ft )
    {
        fTime = ft;
    }

    virtual HRESULT Start()
    {
        if( box != NULL )
            return S_OK;
        else
        {
            XLFLogPrint( collisionLog, "[%s] box was empty\n", "MoveOrientedBoxAroundFrustumTask" );
            return E_FAIL;
        }
    }

    virtual HRESULT Process()
    {
        CollisionBox* ptr = box;
        for( int i = 0; i < count; i++ )
        {
            ptr->m_OBox.Center.x = ( float )( 10.0 * sinf( float( 3.5 * fTime * ptr->m_offset ) ) );
            ptr->m_OBox.Center.y = 7 * cosf( float( 5.1 * fTime * ptr->m_offset ) );
            XMStoreFloat4( &( ptr->m_OBox.Orientation ), XMQuaternionRotationRollPitchYaw( fTime * 1.4f, fTime * 0.2f,
                                                                                           fTime ) );

            ptr->m_iCollision = ATG::IntersectOrientedBoxFrustum( &( ptr->m_OBox ), frustum );
            ptr++;
        }
        return S_OK;
    }

    virtual HRESULT Complete()
    {
        CollisionBox* ptr = box;
        for( int i = 0; i < count; i++ )
        {
            if( ptr->m_iCollision )
            {
                ATG::DebugDraw::DrawObb( ptr->m_OBox, GetCollisionColor( ptr->m_iCollision, 0 ) );
                ( *collisionCount )++;
            }
            ptr++;
        }
        return S_OK;
    }
};


//--------------------------------------------------------------------------------------
// Name: MoveAABoxAroundFrustumTask
// Desc: Axis aligned box - frustrum intersection task.
//--------------------------------------------------------------------------------------
class MoveAABoxAroundFrustumTask : public ATG::Task
{
public:
                    MoveAABoxAroundFrustumTask() : fTime( 0.0 ),
                                                   frustum( NULL ),
                                                   box( NULL )
                    {
                    }

    float fTime;
    const ATG::Frustum* frustum;
    CollisionAABox* box;
    int count;
    UINT* collisionCount;

    void            Initialize( const ATG::Frustum* fr,
                                CollisionAABox* sp,
                                UINT* collision,
                                UINT number )
    {
        frustum = fr;
        box = sp;
        count = number;
        collisionCount = collision;
    }

    void            Setup( float ft )
    {
        fTime = ft;
    }

    virtual HRESULT Start()
    {
        if( box != NULL )
            return S_OK;
        else
        {
            XLFLogPrint( collisionLog, "[%s] box was empty\n", "MoveAABoxAroundFrustumTask" );
            return E_FAIL;
        }
    }

    virtual HRESULT Process()
    {
        CollisionAABox* ptr = box;
        for( int i = 0; i < count; i++ )
        {
            ptr->m_AABox.Center.x = 10 * sinf( 2.1f * fTime * ptr->m_offset );
            ptr->m_AABox.Center.y = 7 * cosf( 3.8f * fTime * ptr->m_offset );

            ptr->m_iCollision = ATG::IntersectAxisAlignedBoxFrustum( &( ptr->m_AABox ), frustum );
            ptr++;
        }
        return S_OK;
    }

    virtual HRESULT Complete()
    {
        CollisionAABox* ptr = box;
        for( int i = 0; i < count; i++ )
        {
            if( ptr->m_iCollision )
            {
                ATG::DebugDraw::DrawAabb( ptr->m_AABox, GetCollisionColor( ptr->m_iCollision, 0 ) );
                ( *collisionCount )++;
            }
            ptr++;
        }
        return S_OK;
    }
};


//--------------------------------------------------------------------------------------
// Name: MoveTriangleAroundFrustumTask
// Desc: Triangle - frustrum intersection task.
//--------------------------------------------------------------------------------------
class MoveTriangleAroundFrustumTask : public ATG::Task
{
public:
                    MoveTriangleAroundFrustumTask() : fTime( 0.0 ),
                                                      frustum( NULL ),
                                                      triangle( NULL )
                    {
                    }

    float fTime;
    double cameraX;
    const ATG::Frustum* frustum;
    CollisionTriangle* triangle;
    int count;
    UINT* collisionCount;

    void            Initialize( const ATG::Frustum* fr,
                                CollisionTriangle* sp,
                                UINT* collision,
                                UINT number )
    {
        frustum = fr;
        triangle = sp;
        count = number;
        collisionCount = collision;
    }

    void            Setup( float ft,
                           double cX )
    {
        fTime = ft;
        cameraX = cX;
    }

    virtual HRESULT Start()
    {
        if( triangle != NULL )
            return S_OK;
        else
        {
            XLFLogPrint( collisionLog, "[%s] triangle was empty\n", "MoveTriangleAroundFrustumTask" );
            return E_FAIL;
        }
    }

    virtual HRESULT Process()
    {
        CollisionTriangle* ptr = triangle;
        for( int i = 0; i < count; i++ )
        {
            XMMATRIX TriangleCoords = XMMatrixRotationRollPitchYaw( fTime * 1.4f, fTime * 2.5f, fTime );
            XMMATRIX Translation = XMMatrixTranslation( 5 * sinf( float( 5.3f * fTime * ptr->m_offset ) ) +
                                                        float( cameraX ),
                                                        5 * cosf( float( 2.3f * fTime * ptr->m_offset ) ),
                                                        5 * sinf( float( 3.4f * fTime * ptr->m_offset ) ) );
            TriangleCoords = XMMatrixMultiply( TriangleCoords, Translation );
            ptr->m_PointA = XMVector3Transform( TrianglePointA, TriangleCoords );
            ptr->m_PointB = XMVector3Transform( TrianglePointB, TriangleCoords );
            ptr->m_PointC = XMVector3Transform( TrianglePointC, TriangleCoords );

            ptr->m_iCollision = ATG::IntersectTriangleFrustum( ptr->m_PointA, ptr->m_PointB, ptr->m_PointC, frustum );
            ptr++;
        }
        return S_OK;
    }

    virtual HRESULT Complete()
    {
        CollisionTriangle* ptr = triangle;
        for( int i = 0; i < count; i++ )
        {
            if( ptr->m_iCollision )
            {
                XMFLOAT3 Verts[3];
                XMStoreFloat3( &Verts[0], ptr->m_PointA );
                XMStoreFloat3( &Verts[1], ptr->m_PointB );
                XMStoreFloat3( &Verts[2], ptr->m_PointC );
                ATG::DebugDraw::DrawTriangle( Verts[0], Verts[1], Verts[2], GetCollisionColor( ptr->m_iCollision,
                                                                                               0 ) );
                ( *collisionCount )++;
            }
            ptr++;
        }
        return S_OK;
    }
};



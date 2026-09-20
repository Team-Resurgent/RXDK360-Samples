//--------------------------------------------------------------------------------------
// Game.h
//
// Declares functions to show how a simple game can use the gesture detection filters
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

#include <AtgResource.h>
#include <AtgSceneAll.h>
#include <vector>


//--------------------------------------------------------------------------------------
// Name: EMovement
// Desc: Describe movement from either a controller or Kinect
//--------------------------------------------------------------------------------------

enum EMovement
{
    MOVEMENT_NONE,
    MOVEMENT_UP,
    MOVEMENT_DOWN
};


//--------------------------------------------------------------------------------------
// Name: Biplane
// Desc: A class to handle updating and rending of a plane
//--------------------------------------------------------------------------------------

class Biplane
{
public:
    Biplane();

    HRESULT Initialize( D3DDevice* pd3dDevice, ATG::PackedResource* pResource );
    VOID Reset();
    VOID Update( const EMovement eMovement );
    VOID Render( XMMATRIX matView, XMMATRIX matProj );

    XMVECTOR GetPosition() const { return XMVectorSet( 0.0f, m_fPosition, 0.0f, 0.0f ); }

protected:
    XMMATRIX    m_matWorld;
    FLOAT       m_fPosition;            // y-position
    FLOAT       m_fPropellerAngle;

    D3DDevice*  m_pd3dDevice;

    ATG::Scene* m_pScene;
    ATG::Model* m_pBody;
    ATG::Model* m_pWings;
    ATG::Model* m_pPropeller;

    static const FLOAT c_fPropellerSpeed;
};


//--------------------------------------------------------------------------------------
// Name: Background
// Desc: A class to handle updating and rending of a scrolling background
//--------------------------------------------------------------------------------------

class Background
{
public:
    Background();

    HRESULT Initialize( D3DDevice* pd3dDevice, ATG::PackedResource* pResource );
    VOID Update();
    VOID Render();

protected:
    D3DDevice*      m_pd3dDevice;
    D3DBaseTexture* m_pTexture;

    UINT            m_uBackBufferWidth;
    UINT            m_uBackBufferHeight;

    FLOAT           m_fTextureScroll;
    FLOAT           m_fTextureScrollSpeed;

    static const FLOAT  c_fTextureScrollSpeed;
};


//--------------------------------------------------------------------------------------
// Name: Token
// Desc: A class to handle updating and rending of a token
//--------------------------------------------------------------------------------------

class Token
{
public:
    Token();

    HRESULT Initialize( D3DDevice* pd3dDevice, ATG::PackedResource* pResource );
    VOID Reset();
    BOOL Update( XMVECTOR vTargetPosition );
    VOID Render( XMMATRIX matView, XMMATRIX matProj );
    VOID Spawn();

    BOOL IsActive() const { return m_bActive; }
    XMVECTOR GetPosition() const { return m_vPosition; }

protected:
    XMMATRIX    m_matWorld;
    XMVECTOR    m_vPosition;
    D3DDevice*  m_pd3dDevice;
    BOOL        m_bActive;

    static ATG::Scene*  s_pScene;
    static ATG::Model*  s_pModel;

    static const FLOAT  c_fSpeed;

};


//--------------------------------------------------------------------------------------
// Name: Tokens
// Desc: A class to handle multiple tokens
//--------------------------------------------------------------------------------------

class Tokens
{
public:
    Tokens();

    HRESULT Initialize( D3DDevice* pd3dDevice, ATG::PackedResource* pResource );
    VOID Reset();
    BOOL Update( XMVECTOR vTargetPosition );
    VOID Render( XMMATRIX matView, XMMATRIX matProj );

protected:
    UINT                m_uFrameNumber;
    std::vector<Token>  m_Tokens;

    static const UINT   m_uMaxTokens        = 5;
    static const UINT   m_uSpawnFrameRate   = 20;       // every 20 frames a new token get's spawned
};


//--------------------------------------------------------------------------------------
// Name: Game
// Desc: A class to handle updating and rending of a simple game
//--------------------------------------------------------------------------------------

class Game
{
public:
    Game();

    HRESULT Initialize( D3DDevice* pd3dDevice, ATG::PackedResource* pResource );
    VOID Reset();
    VOID Update( EMovement eMovement );
    VOID Render();

    UINT GetScore() const { return m_uScore; }
    UINT GetHighScore() const { return m_uHighScore; }
    
protected:
    Biplane    m_Biplane;
    Background m_Background;
    Tokens     m_Tokens;

    XMMATRIX    m_matView;
    XMMATRIX    m_matProj;

    UINT        m_uScore;
    UINT        m_uHighScore;
    UINT        m_uCurrentSkeletonIdx;

};





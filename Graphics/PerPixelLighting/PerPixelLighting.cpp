//--------------------------------------------------------------------------------------
// PerPixelLighting.cpp
//
// Example code showing how to do per-pixel lighting using vertex shaders
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include <xtl.h>
#include <xboxmath.h>
#include <AtgApp.h>
#include <AtgFont.h>
#include <AtgHelp.h>
#include <AtgInput.h>
#include <AtgMesh.h>
#include <AtgResource.h>
#include <AtgUtil.h>


//--------------------------------------------------------------------------------------
// Callouts for labelling the gamepad on the help screen
//--------------------------------------------------------------------------------------
ATG::HELP_CALLOUT g_HelpCallouts[] =
{
    { ATG::HELP_LEFTSTICK,    ATG::HELP_PLACEMENT_2, L"Move point\nlight" },
    { ATG::HELP_RIGHTSTICK,   ATG::HELP_PLACEMENT_1, L"Move dir. light" },
    { ATG::HELP_X_BUTTON,     ATG::HELP_PLACEMENT_2, L"Toggle base\ntexture" },
    { ATG::HELP_Y_BUTTON,     ATG::HELP_PLACEMENT_2, L"Toggle ambient\nlight" },
    { ATG::HELP_A_BUTTON,     ATG::HELP_PLACEMENT_2, L"Toggle dir.\nlight" },
    { ATG::HELP_B_BUTTON,     ATG::HELP_PLACEMENT_2, L"Toggle point\nlight" },
    { ATG::HELP_BACK_BUTTON,  ATG::HELP_PLACEMENT_1, L"Display help" },
    { ATG::HELP_START_BUTTON, ATG::HELP_PLACEMENT_1, L"Pause" },
};

#define NUM_HELP_CALLOUTS (sizeof(g_HelpCallouts) / sizeof(g_HelpCallouts[0]))


//--------------------------------------------------------------------------------------
// A position, normal, and tex coords for each vertex
//--------------------------------------------------------------------------------------
struct CUSTOMVERTEX
{
    XMFLOAT3 p;         // Position
    XMFLOAT3 n;         // Normal
    FLOAT tu, tv;    // Texture coords
};


//--------------------------------------------------------------------------------------
// Three orthogonal tangent space vectors for each vertex
//--------------------------------------------------------------------------------------
struct TANGENTSPACE
{
    XMFLOAT3 vTangent;
    XMFLOAT3 vBinormal;
    XMFLOAT3 vNormal;
};


//--------------------------------------------------------------------------------------
// Name: CBumpyObject
//--------------------------------------------------------------------------------------
class CBumpyObject
{
    LPDIRECT3DDEVICE9 m_pd3dDevice;        // Local copy of the d3d device

    LPDIRECT3DVERTEXBUFFER9 m_pTangentSpaceVB;   // Hold tangent space vectors
    LPDIRECT3DVERTEXBUFFER9 m_pSphereVerticesVB; // Hold geometry
    LPDIRECT3DINDEXBUFFER9 m_pSphereIndicesIB;
    DWORD m_dwNumVertices;
    DWORD m_dwNumIndices;

    LPDIRECT3DTEXTURE9 m_pBaseTexture;      // Base texture
    LPDIRECT3DTEXTURE9 m_pNormalMap;        // Normal texture (bumpmap)
    LPDIRECT3DCUBETEXTURE9 m_pCubeMap;          // Normalization cubemap

    LPDIRECT3DVERTEXDECLARATION9 m_pVertexDeclaration;
    LPDIRECT3DVERTEXSHADER9 m_pVertexShader;    // Custom vertex shader
    LPDIRECT3DPIXELSHADER9 m_pPixelShader;     // Custom pixel shader

public:
            CBumpyObject();
    virtual ~CBumpyObject();

    HRESULT Init( LPDIRECT3DDEVICE9 pd3dDevice, ATG::PackedResource* pResource );
    VOID    InitSphere( FLOAT radius, DWORD nLat, DWORD nLong );
    VOID    CreateBasisMatrices();

    VOID    ProcessVertices( XMVECTOR vLightDir, XMVECTOR vLightPos );

    HRESULT Render( LPDIRECT3DDEVICE9 pd3dDevice );
};


//--------------------------------------------------------------------------------------
// Globally accessed attributes
//--------------------------------------------------------------------------------------
XMVECTOR    g_vZero = XMVectorZero();
XMVECTOR    g_vOne = XMVectorSet( 1.0f, 1.0f, 1.0f, 1.0f );
XMVECTOR    g_vAmbientLightColor = XMVectorSet( 0.20f, 0.20f, 0.20f, 1.0f );
XMVECTOR    g_vDirLightDirection; // Directional light direction
XMVECTOR    g_vDirectionalLightColor = XMVectorSet( 0.4f, 0.4f, 0.9f, 1.0f );
XMVECTOR    g_vPointLightPos;        // Point light position
XMVECTOR    g_vPointLightColor = XMVectorSet( 1.0f, 1.0f, 1.0f, 1.0f );
XMVECTOR    g_vCrosshairColor = XMVectorSet( 1.0f, 0.0f, 0.0f, 1.0f );

BOOL        g_bEnableBaseTexturePass = TRUE;
BOOL        g_bEnableAmbientLightPass = TRUE;
BOOL        g_bEnableDirLightPass = TRUE;
BOOL        g_bEnablePointLightPass = TRUE;

XMMATRIX    g_matWorld;
XMMATRIX    g_matView;
XMMATRIX    g_matProj;


//--------------------------------------------------------------------------------------
// Name: class Sample
// Desc: Main class to run this application. Most functionality is inherited
//       from the ATG::Application base class.
//--------------------------------------------------------------------------------------
class Sample : public ATG::Application
{
    ATG::Timer m_Timer;
    ATG::Font m_Font;
    ATG::PackedResource m_Resource;
    ATG::Help m_Help;
    BOOL m_bDrawHelp;
    BOOL m_bPaused;

    LPDIRECT3DVERTEXSHADER9 m_pVSPosition;
    LPDIRECT3DPIXELSHADER9 m_pPSConstantColor;

    CBumpyObject m_BumpyObject;

    VOID            DrawLight();

public:
    virtual HRESULT Initialize();
    virtual HRESULT Update();
    virtual HRESULT Render();
};


//--------------------------------------------------------------------------------------
// Name: main()
// Desc: Entry point to the program
//--------------------------------------------------------------------------------------
INT __cdecl main()
{
    Sample atgApp;
    ATG::GetVideoSettings( &atgApp.m_d3dpp.BackBufferWidth, &atgApp.m_d3dpp.BackBufferHeight );

    // Make sure display is gamma correct.
    atgApp.m_d3dpp.BackBufferFormat =  ( D3DFORMAT )MAKESRGBFMT( D3DFMT_A8R8G8B8 );
    atgApp.m_d3dpp.FrontBufferFormat = ( D3DFORMAT )MAKESRGBFMT( D3DFMT_LE_X8R8G8B8 );

    atgApp.Run();
}


//--------------------------------------------------------------------------------------
// Name: Initialize()
// Desc: Initializes the app
//--------------------------------------------------------------------------------------
HRESULT Sample::Initialize()
{
    HRESULT hr;

    m_bDrawHelp = FALSE;
    m_bPaused = FALSE;

    // Initial light attributes
    g_vPointLightPos = XMVectorSet( -1.0f, 1.0f, -2.75f, 0.0f );
    g_vDirLightDirection = XMVectorSet( 1.0f, 1.0f, 1.50f, 0.0f );

    // Create the font
    if( FAILED( m_Font.Create( "game:\\Media\\Fonts\\Arial_16.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Confine text drawing to the title safe area
    m_Font.SetWindow( ATG::GetTitleSafeArea() );

    // Create the help
    if( FAILED( hr = m_Help.Create( "game:\\Media\\Help\\Help.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Create the resources
    if( FAILED( m_Resource.Create( "game:\\Media\\Resource.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Create the bumpy object
    if( FAILED( hr = m_BumpyObject.Init( m_pd3dDevice, &m_Resource ) ) )
        return hr;

    // Determine the aspect ratio
    FLOAT fAspectRatio = ( FLOAT )m_d3dpp.BackBufferWidth / ( FLOAT )m_d3dpp.BackBufferHeight;

    // Set the transform matrices
    g_matWorld = XMMatrixIdentity();
    g_matView = XMMatrixTranslation( 0.0f, 0.0f, 5.0f );
    g_matProj = XMMatrixPerspectiveFovLH( XM_PI / 3, fAspectRatio, 1.0f, 20.0f );

    if( FAILED( hr = ATG::LoadVertexShader( "game:\\Media\\Shaders\\Position.xvu", &m_pVSPosition ) ) )
        return hr;
    if( FAILED( hr = ATG::LoadPixelShader( "game:\\Media\\Shaders\\ConstantColor.xpu", &m_pPSConstantColor ) ) )
        return hr;

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Update()
// Desc: Called once per frame, the call is the entry point for animating the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Update()
{
    FLOAT m_fElapsedTime = ( FLOAT )m_Timer.GetElapsedTime();

    // Get the current gamepad status
    ATG::GAMEPAD* pGamepad = ATG::Input::GetMergedInput();

    // Toggle help
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_BACK )
        m_bDrawHelp = !m_bDrawHelp;

    // Toggle rotation
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_START )
        m_bPaused = !m_bPaused;

    // Rotate the scene
    XMMATRIX matRotate;
    matRotate = XMMatrixRotationY( m_bPaused ? 0.0f : -m_fElapsedTime / 2 );
    g_matWorld = XMMatrixMultiply( g_matWorld, matRotate );

    // Toggle the render passes
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_A )
        g_bEnableDirLightPass = !g_bEnableDirLightPass;
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_B )
        g_bEnablePointLightPass = !g_bEnablePointLightPass;
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_X )
        g_bEnableBaseTexturePass = !g_bEnableBaseTexturePass;
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_Y )
        g_bEnableAmbientLightPass = !g_bEnableAmbientLightPass;

    // Adjust the point light's position
    static FLOAT fPhi1 = 0.5f;
    fPhi1 += 3.0f * m_fElapsedTime * pGamepad->fX1;
    if( fPhi1 < -2.3f ) fPhi1 = -2.3f;
    if( fPhi1 > -0.9f ) fPhi1 = -0.9f;

    static FLOAT fTheta1 = 0.0f;
    fTheta1 -= 3.0f * m_fElapsedTime * pGamepad->fY1;
    if( fTheta1 < +0.9f ) fTheta1 = +0.9f;
    if( fTheta1 > +2.3f ) fTheta1 = +2.3f;

    g_vPointLightPos.x = 2 * cosf( fPhi1 );
    g_vPointLightPos.y = 2 * cosf( fTheta1 );
    g_vPointLightPos.z = 2 * sinf( fPhi1 ) * sinf( fTheta1 );

    // Adjust the directional light's direction
    static FLOAT fPhi2 = 0.5f;
    fPhi2 += 3.0f * m_fElapsedTime * pGamepad->fX2;
    if( fPhi2 < -2.3f ) fPhi2 = -2.3f;
    if( fPhi2 > -0.9f ) fPhi2 = -0.9f;

    static FLOAT fTheta2 = 0.0f;
    fTheta2 -= 3.0f * m_fElapsedTime * pGamepad->fY2;
    if( fTheta2 < +0.9f ) fTheta2 = +0.9f;
    if( fTheta2 > +2.3f ) fTheta2 = +2.3f;

    g_vDirLightDirection.x = -2 * cosf( fPhi2 );
    g_vDirLightDirection.y = -2 * cosf( fTheta2 );
    g_vDirLightDirection.z = -2 * sinf( fPhi2 ) * sinf( fTheta2 );
    g_vDirLightDirection = XMVector3Normalize( g_vDirLightDirection );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: DrawLight()
// Desc: Draws crosshairs to show the light pos
//--------------------------------------------------------------------------------------
VOID Sample::DrawLight()
{
    // Get the inverse of the world matrix
    XMVECTOR vDeterminant;
    XMMATRIX matInvWorld = XMMatrixInverse( &vDeterminant, g_matWorld );

    // Setup some points to draw crosshairs
    XMFLOAT3 line[6];
    const XMFLOAT3 vXP( g_vPointLightPos.x + 0.2f, g_vPointLightPos.y + 0.0f, g_vPointLightPos.z + 0.0f );
    const XMFLOAT3 vXN( g_vPointLightPos.x + -0.2f, g_vPointLightPos.y + 0.0f, g_vPointLightPos.z + 0.0f );
    const XMFLOAT3 vYP( g_vPointLightPos.x + 0.0f, g_vPointLightPos.y + 0.2f, g_vPointLightPos.z + 0.0f );
    const XMFLOAT3 vYN( g_vPointLightPos.x + 0.0f, g_vPointLightPos.y + -0.2f, g_vPointLightPos.z + 0.0f );
    const XMFLOAT3 vZP( g_vPointLightPos.x + 0.0f, g_vPointLightPos.y + 0.0f, g_vPointLightPos.z + 0.2f );
    const XMFLOAT3 vZN( g_vPointLightPos.x + 0.0f, g_vPointLightPos.y + 0.0f, g_vPointLightPos.z + -0.2f );
    XMStoreFloat3( &line[0], XMVector3TransformCoord( XMLoadFloat3( &vXP ), matInvWorld ) );
    XMStoreFloat3( &line[1], XMVector3TransformCoord( XMLoadFloat3( &vXN ), matInvWorld ) );
    XMStoreFloat3( &line[2], XMVector3TransformCoord( XMLoadFloat3( &vYP ), matInvWorld ) );
    XMStoreFloat3( &line[3], XMVector3TransformCoord( XMLoadFloat3( &vYN ), matInvWorld ) );
    XMStoreFloat3( &line[4], XMVector3TransformCoord( XMLoadFloat3( &vZP ), matInvWorld ) );
    XMStoreFloat3( &line[5], XMVector3TransformCoord( XMLoadFloat3( &vZN ), matInvWorld ) );

    // Set the crosshair's color
    m_pd3dDevice->SetPixelShader( m_pPSConstantColor );
    m_pd3dDevice->SetPixelShaderConstantF( 0, ( FLOAT* )&g_vCrosshairColor, 1 );

    // Draw the crosshairs
    m_pd3dDevice->SetFVF( D3DFVF_XYZ );
    m_pd3dDevice->SetVertexShader( m_pVSPosition );

    // Set combined transform matrix
    XMMATRIX matWorldViewProj = XMMatrixTranspose( g_matWorld * g_matView * g_matProj );
    m_pd3dDevice->SetVertexShaderConstantF( 0, ( FLOAT* )&matWorldViewProj, 4 );

    m_pd3dDevice->DrawPrimitiveUP( D3DPT_LINELIST, 3, line, sizeof( line[0] ) );
}


//--------------------------------------------------------------------------------------
// Name: Render()
// Desc: Called once per frame, the call is the entry point for 3D rendering.
//--------------------------------------------------------------------------------------
HRESULT Sample::Render()
{
    // Draw a gradient filled background
    ATG::RenderBackground( 0xff000000, 0xff888844 );

    // Draw the main object
    m_BumpyObject.Render( m_pd3dDevice );

    // Draw the position of the point light
    if( g_bEnablePointLightPass )
        DrawLight();

    // Show title, frame rate, and help
    m_Timer.MarkFrame();
    if( m_bDrawHelp )
    {
        m_Help.Render( &m_Font, g_HelpCallouts, NUM_HELP_CALLOUTS );
    }
    else
    {
        m_Font.Begin();
        m_Font.SetScaleFactors( 1.2f, 1.2f );
        m_Font.DrawText( 0, 0, 0xffffffff, L"PerPixelLighting" );
        m_Font.SetScaleFactors( 1.0f, 1.0f );
        m_Font.DrawText( 0, 0, 0xffffff00, m_Timer.GetFrameRate(), ATGFONT_RIGHT );
        m_Font.End();
    }

    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: CBumpyObject()
// Desc: Initialize object to empty state
//--------------------------------------------------------------------------------------
CBumpyObject::CBumpyObject()
{
    m_pd3dDevice = NULL;
    m_pTangentSpaceVB = NULL;
    m_pSphereVerticesVB = NULL;
    m_pSphereIndicesIB = NULL;
    m_pBaseTexture = NULL;
    m_pNormalMap = NULL;
    m_pCubeMap = NULL;
    m_pVertexDeclaration = NULL;
    m_pVertexShader = NULL;
    m_pPixelShader = NULL;
}


//--------------------------------------------------------------------------------------
// Name: ~CBumpyObject()
// Desc: Free resources
//--------------------------------------------------------------------------------------
CBumpyObject::~CBumpyObject()
{
    // Textures that were used from the packed resource should not be released via D3D.
    // The desctructor of the packed resource will free the associated memory.
    m_pBaseTexture = NULL;

    if( m_pTangentSpaceVB != NULL )     m_pTangentSpaceVB->Release();
    if( m_pSphereVerticesVB != NULL )   m_pSphereVerticesVB->Release();
    if( m_pSphereIndicesIB != NULL )    m_pSphereIndicesIB->Release();
    if( m_pNormalMap != NULL )          m_pNormalMap->Release();
    if( m_pCubeMap != NULL )            m_pCubeMap->Release();
    if( m_pVertexDeclaration != NULL )  m_pVertexDeclaration->Release();
    if( m_pVertexShader != NULL )       m_pVertexShader->Release();
    if( m_pPixelShader != NULL )        m_pPixelShader->Release();
}


//--------------------------------------------------------------------------------------
// Name: Init()
// Desc: Load resources and create geometry for the object
//--------------------------------------------------------------------------------------
HRESULT CBumpyObject::Init( LPDIRECT3DDEVICE9 pd3dDevice,
                            ATG::PackedResource* pResource )
{
    HRESULT hr;

    // Keep track of the device
    m_pd3dDevice = pd3dDevice;

    // Initialize the sphere's geometry, and create it's basis matrices
    InitSphere( 1.7f, 24, 24 );
    CreateBasisMatrices();

    // Create the base texture
    m_pBaseTexture = pResource->GetTexture( "Earth" );

    // Load grayscale texture to be used for making the normal map
    LPDIRECT3DTEXTURE9 pHeightMap = pResource->GetTexture( "EarthBmp" );

    // Compute the normal map from the gray scale texture
    D3DSURFACE_DESC desc;
    D3DLOCKED_RECT lockSrc;
    D3DLOCKED_RECT lockDest;
    pHeightMap->GetLevelDesc( 0, &desc );

    // We use the Q8W8V8U8 signed format so that we don't have to convert
    // between signed and unsigned vectors in the pixel shader
    if( FAILED( pd3dDevice->CreateTexture( desc.Width, desc.Height, 1, 0, D3DFMT_LIN_Q8W8V8U8,
                                           D3DPOOL_DEFAULT, &m_pNormalMap, NULL ) ) )
        return E_FAIL;

    pHeightMap->LockRect( 0, &lockSrc, 0, 0L );
    m_pNormalMap->LockRect( 0, &lockDest, 0, 0 );

    DWORD* pDstBits = ( DWORD* )lockDest.pBits;
    DWORD* pSrcBits = ( DWORD* )lockSrc.pBits;

    for( DWORD y = 0; y < desc.Height; y++ )
    {
        for( DWORD x = 0; x < desc.Width; x++ )
        {
            DWORD x0 = x, x1 = ( x + 1 < desc.Width )  ? x + 1 : 0;
            DWORD y0 = y, y1 = ( y + 1 < desc.Height ) ? y + 1 : 0;

            DWORD* p00 = ( ( DWORD* )pSrcBits ) + x0 + desc.Width * y0;
            DWORD* p10 = ( ( DWORD* )pSrcBits ) + x1 + desc.Width * y0;
            DWORD* p01 = ( ( DWORD* )pSrcBits ) + x0 + desc.Width * y1;

            FLOAT fHeight00 = ( FLOAT )( ( ( *p00 ) & 0x00ff0000 ) >> 16 ) / 255.0f;
            FLOAT fHeight10 = ( FLOAT )( ( ( *p10 ) & 0x00ff0000 ) >> 16 ) / 255.0f;
            FLOAT fHeight01 = ( FLOAT )( ( ( *p01 ) & 0x00ff0000 ) >> 16 ) / 255.0f;

            XMVECTOR vPoint00 = XMVectorSet( x + 0.0f, y + 0.0f, fHeight00, 0.0f );
            XMVECTOR vPoint10 = XMVectorSet( x + 0.1f, y + 0.0f, fHeight10, 0.0f );
            XMVECTOR vPoint01 = XMVectorSet( x + 0.0f, y + 0.1f, fHeight01, 0.0f );
            XMVECTOR v10 = vPoint10 - vPoint00;
            XMVECTOR v01 = vPoint01 - vPoint00;

            XMVECTOR v = XMVector3Normalize( XMVector3Cross( v10, v01 ) );

            ( *pDstBits++ ) = ATG::VectorToQWVU( v );
        }
    }
    m_pNormalMap->UnlockRect( 0 );
    pHeightMap->UnlockRect( 0 );

    // Create the normalization cube map
    ATG::CreateNormalizationCubeMap( 256, &m_pCubeMap );

    // Create vertex declaration
    static const D3DVERTEXELEMENT9 decl[] =
    {
        // Position, normal, and tex coords
        { 0,  0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT,  D3DDECLUSAGE_POSITION, 0 },
        { 0, 12, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT,  D3DDECLUSAGE_NORMAL,   0 },
        { 0, 24, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT,  D3DDECLUSAGE_TEXCOORD, 0 },
        // Tangent space vectors
        { 1,  0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT,  D3DDECLUSAGE_TANGENT,  0 },
        { 1, 12, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT,  D3DDECLUSAGE_BINORMAL, 0 },
        { 1, 24, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT,  D3DDECLUSAGE_NORMAL,   1 },
        D3DDECL_END()
    };
    if( FAILED( hr = m_pd3dDevice->CreateVertexDeclaration( decl, &m_pVertexDeclaration ) ) )
        return hr;

    VOID* pCode = NULL;
    if( FAILED( hr = ATG::LoadFile( "game:\\Media\\Shaders\\PerPixelLighting.xvu", &pCode ) ) )
        return hr;
    if( FAILED( hr = m_pd3dDevice->CreateVertexShader( ( DWORD* )pCode, &m_pVertexShader ) ) )
    {
        ATG::UnloadFile( pCode );
        return hr;
    }
    ATG::UnloadFile( pCode );

    hr = ATG::LoadPixelShader( "game:\\Media\\Shaders\\PerPixelLighting.xpu", &m_pPixelShader );
    if( FAILED( hr ) )
        return hr;

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: InitSphere()
// Desc: Create geometry for a sphere
//--------------------------------------------------------------------------------------
VOID CBumpyObject::InitSphere( FLOAT fRadius, DWORD dwNumSphereRings,
                               DWORD dwNumSphereSegments )
{
    // Establish constants used in sphere generation
    FLOAT fDeltaRingAngle = ( XM_PI / dwNumSphereRings );
    FLOAT fDeltaSegAngle = ( 2.0f * XM_PI / dwNumSphereSegments );

    m_dwNumVertices = dwNumSphereRings * ( dwNumSphereSegments + 1 ) * 2;
    m_dwNumIndices = 3 * ( m_dwNumVertices - 2 );

    // Create the vertex buffer and fill it
    m_pd3dDevice->CreateVertexBuffer( m_dwNumVertices * sizeof( CUSTOMVERTEX ),
                                      D3DUSAGE_WRITEONLY, 0L, D3DPOOL_MANAGED,
                                      &m_pSphereVerticesVB, NULL );

    CUSTOMVERTEX* pSrcVertices = new CUSTOMVERTEX[m_dwNumVertices];
    CUSTOMVERTEX* pVertices = pSrcVertices;

    // Generate the group of rings for the sphere
    for( DWORD ring = 0; ring < dwNumSphereRings; ring++ )
    {
        FLOAT r0 = sinf( ( ring + 0 ) * fDeltaRingAngle );
        FLOAT r1 = sinf( ( ring + 1 ) * fDeltaRingAngle );
        FLOAT y0 = cosf( ( ring + 0 ) * fDeltaRingAngle );
        FLOAT y1 = cosf( ( ring + 1 ) * fDeltaRingAngle );

        // Generate the group of segments for the current ring
        for( DWORD seg = 0; seg < ( dwNumSphereSegments + 1 ); seg++ )
        {
            FLOAT x0 = r0 * sinf( seg * fDeltaSegAngle );
            FLOAT z0 = r0 * cosf( seg * fDeltaSegAngle );
            FLOAT x1 = r1 * sinf( seg * fDeltaSegAngle );
            FLOAT z1 = r1 * cosf( seg * fDeltaSegAngle );

            // Add two vertices to the strip which makes up the sphere
            // (using the transformed normal to generate texture coords)
            pVertices->p = XMFLOAT3( fRadius * x0, fRadius * y0, fRadius * z0 );
            pVertices->n = XMFLOAT3( x0, y0, z0 );
            pVertices->tu = -( ( FLOAT )seg ) / dwNumSphereSegments;
            pVertices->tv = ( ring + 0 ) / ( FLOAT )dwNumSphereRings;
            pVertices++;

            pVertices->p = XMFLOAT3( fRadius * x1, fRadius * y1, fRadius * z1 );
            pVertices->n = XMFLOAT3( x1, y1, z1 );
            pVertices->tu = -( ( FLOAT )seg ) / dwNumSphereSegments;
            pVertices->tv = ( ring + 1 ) / ( FLOAT )dwNumSphereRings;
            pVertices++;
        }
    }

    CUSTOMVERTEX* pDstVertices;
    m_pSphereVerticesVB->Lock( 0, 0, ( VOID** )&pDstVertices, 0 );
    memcpy( pDstVertices, pSrcVertices, sizeof( CUSTOMVERTEX ) * m_dwNumVertices );
    m_pSphereVerticesVB->Unlock();
    delete[] pSrcVertices;

    // Create the index buffer and fill it
    m_pd3dDevice->CreateIndexBuffer( m_dwNumIndices * sizeof( WORD ),
                                     D3DUSAGE_WRITEONLY, D3DFMT_INDEX16,
                                     D3DPOOL_MANAGED, &m_pSphereIndicesIB, NULL );

    WORD* pIndices;
    m_pSphereIndicesIB->Lock( 0, 0, ( VOID** )&pIndices, 0 );

    for( DWORD i = 0; i < m_dwNumVertices - 2; i++ )
    {
        ( *pIndices++ ) = ( WORD )( i + 0 );
        ( *pIndices++ ) = ( WORD )( i + 1 + ( i % 2 ) );
        ( *pIndices++ ) = ( WORD )( i + 2 - ( i % 2 ) );
    }

    m_pSphereIndicesIB->Unlock();
}


//--------------------------------------------------------------------------------------
// Name: CreateBasisMatrices()
// Desc: Create the tangent space basis vectors for the object
//--------------------------------------------------------------------------------------
VOID CBumpyObject::CreateBasisMatrices()
{
    WORD i, j;

    // Store the source vertices for cacheable access
    CUSTOMVERTEX* pSrcVertices;
    m_pSphereVerticesVB->Lock( 0, 0, ( VOID** )&pSrcVertices, 0 );
    CUSTOMVERTEX* pVertices = new CUSTOMVERTEX[m_dwNumVertices];
    memcpy( pVertices, pSrcVertices, sizeof( CUSTOMVERTEX ) * m_dwNumVertices );
    pSrcVertices = pVertices;
    m_pSphereVerticesVB->Unlock();

    // Store the source indices for cacheable access
    WORD* pSrcIndices;
    m_pSphereIndicesIB->Lock( 0, 0, ( VOID** )&pSrcIndices, 0 );
    WORD* pIndices = new WORD[m_dwNumIndices];
    memcpy( pIndices, pSrcIndices, sizeof( WORD ) * m_dwNumIndices );
    pSrcIndices = pIndices;
    m_pSphereIndicesIB->Unlock();

    // Create some tangent space vectors
    TANGENTSPACE* pSrcTangentSpace = new TANGENTSPACE[m_dwNumVertices];
    TANGENTSPACE* pTangentSpace = pSrcTangentSpace;

    // Clear out the tangent space area
    ZeroMemory( pTangentSpace, m_dwNumVertices * sizeof( TANGENTSPACE ) );

    // Loop through all triangles, accumulating du and dv offsets to build
    // basis vectors
    for( i = 0; i < m_dwNumIndices; i += 3 )
    {
        WORD i0 = pIndices[i + 0];
        WORD i1 = pIndices[i + 1];
        WORD i2 = pIndices[i + 2];

        if( i0 < m_dwNumVertices && i1 < m_dwNumVertices && i2 < m_dwNumVertices )
        {
            CUSTOMVERTEX* v0 = &pVertices[i0];
            CUSTOMVERTEX* v1 = &pVertices[i1];
            CUSTOMVERTEX* v2 = &pVertices[i2];
            XMVECTOR du = XMVectorZero();
            XMVECTOR dv = XMVectorZero();
            XMVECTOR edge01;
            XMVECTOR edge02;
            XMVECTOR cp;

            // Skip degenerate triangles
            if( fabs( v0->p.x - v1->p.x ) < 1e-6 && fabs( v0->p.y - v1->p.y ) < 1e-6 &&
                fabs( v0->p.z - v1->p.z ) < 1e-6 )
                continue;
            if( fabs( v1->p.x - v2->p.x ) < 1e-6 && fabs( v1->p.y - v2->p.y ) < 1e-6 &&
                fabs( v1->p.z - v2->p.z ) < 1e-6 )
                continue;
            if( fabs( v2->p.x - v0->p.x ) < 1e-6 && fabs( v2->p.y - v0->p.y ) < 1e-6 &&
                fabs( v2->p.z - v0->p.z ) < 1e-6 )
                continue;

            edge01 = XMVectorSet( v1->p.x - v0->p.x, v1->tu - v0->tu, v1->tv - v0->tv, 0.0f );
            edge02 = XMVectorSet( v2->p.x - v0->p.x, v2->tu - v0->tu, v2->tv - v0->tv, 0.0f );
            cp = XMVector3Cross( edge01, edge02 );
            if( fabs( cp.x ) > 1e-8 )
            {
                du.x = -cp.y / cp.x;
                dv.x = -cp.z / cp.x;
            }

            edge01 = XMVectorSet( v1->p.y - v0->p.y, v1->tu - v0->tu, v1->tv - v0->tv, 0.0f );
            edge02 = XMVectorSet( v2->p.y - v0->p.y, v2->tu - v0->tu, v2->tv - v0->tv, 0.0f );
            cp = XMVector3Cross( edge01, edge02 );
            if( fabs( cp.x ) > 1e-8 )
            {
                du.y = -cp.y / cp.x;
                dv.y = -cp.z / cp.x;
            }

            edge01 = XMVectorSet( v1->p.z - v0->p.z, v1->tu - v0->tu, v1->tv - v0->tv, 0.0f );
            edge02 = XMVectorSet( v2->p.z - v0->p.z, v2->tu - v0->tu, v2->tv - v0->tv, 0.0f );
            cp = XMVector3Cross( edge01, edge02 );
            if( fabs( cp.x ) > 1e-8 )
            {
                du.z = -cp.y / cp.x;
                dv.z = -cp.z / cp.x;
            }

            XMStoreFloat3( &pTangentSpace[i0].vTangent, XMLoadFloat3( &pTangentSpace[i0].vTangent ) + du );
            XMStoreFloat3( &pTangentSpace[i1].vTangent, XMLoadFloat3( &pTangentSpace[i1].vTangent ) + du );
            XMStoreFloat3( &pTangentSpace[i2].vTangent, XMLoadFloat3( &pTangentSpace[i2].vTangent ) + du );

            XMStoreFloat3( &pTangentSpace[i0].vNormal, XMLoadFloat3( &pTangentSpace[i0].vNormal ) + dv );
            XMStoreFloat3( &pTangentSpace[i1].vNormal, XMLoadFloat3( &pTangentSpace[i1].vNormal ) + dv );
            XMStoreFloat3( &pTangentSpace[i2].vNormal, XMLoadFloat3( &pTangentSpace[i2].vNormal ) + dv );
        }
    }

    for( i = 0; i < m_dwNumVertices; i++ )
    {
        // vBinormal = vTangent x vNormal
        XMVECTOR vTangent = XMVector3Normalize( XMLoadFloat3( &pTangentSpace[i].vTangent ) );
        XMVECTOR vNormal = XMVector3Normalize( XMLoadFloat3( &pTangentSpace[i].vNormal ) );
        XMVECTOR vBinormal = XMVector3Cross( vTangent, vNormal );

        // Get the vertex normal (make sure it's normalized)
        XMVECTOR vVtxNormal = XMVector3Normalize( XMLoadFloat3( &pVertices[i].n ) );

        // Make sure the basis vector and normal point in the same direction
        if( XMVector3Dot( vBinormal, vVtxNormal ).x < 0.0f )
            vBinormal = -vBinormal;

        XMStoreFloat3( &pTangentSpace[i].vTangent, vTangent );
        XMStoreFloat3( &pTangentSpace[i].vNormal, vNormal );
        XMStoreFloat3( &pTangentSpace[i].vBinormal, vBinormal );
    }

    // Find duplicate vertices in the mesh, and average their tangent spaces
    // together. This is necessary to avoid discontinuities at the seams.
    for( i = 0; i < m_dwNumVertices; i++ )
    {
        XMVECTOR vT = XMLoadFloat3( &pTangentSpace[i].vTangent );
        XMVECTOR vB = XMLoadFloat3( &pTangentSpace[i].vBinormal );
        XMVECTOR vN = XMLoadFloat3( &pTangentSpace[i].vNormal );

        for( j = i + 1; j < m_dwNumVertices; j++ )
        {
            XMVECTOR v3 = XMLoadFloat3( &pVertices[i].p ) - XMLoadFloat3( &pVertices[j].p );
            FLOAT dist = XMVector3LengthSq( v3 ).x;

            if( dist < 1.0e-8f )
            {
                vT += XMLoadFloat3( &pTangentSpace[j].vTangent );
                vB += XMLoadFloat3( &pTangentSpace[j].vBinormal );
                vN += XMLoadFloat3( &pTangentSpace[j].vNormal );
            }
        }

        // Normalize the vectors of the basis matrix
        vT = XMVector3Normalize( vT );
        vB = XMVector3Normalize( vB );
        vN = XMVector3Normalize( vN );

        for( j = i; j < m_dwNumVertices; j++ )
        {
            XMVECTOR v3 = XMLoadFloat3( &pVertices[i].p ) - XMLoadFloat3( &pVertices[j].p );
            FLOAT dist = XMVector3LengthSq( v3 ).x;

            if( dist < 1.0e-8f )
            {
                XMStoreFloat3( &pTangentSpace[j].vTangent, vT );
                XMStoreFloat3( &pTangentSpace[j].vBinormal, vB );
                XMStoreFloat3( &pTangentSpace[j].vNormal, vN );
            }
        }
    }

    // Create a vertex buffer for the newly created tangent space vectors
    m_pd3dDevice->CreateVertexBuffer( m_dwNumVertices * sizeof( TANGENTSPACE ),
                                      D3DUSAGE_WRITEONLY, 0L,
                                      D3DPOOL_MANAGED, &m_pTangentSpaceVB, NULL );
    TANGENTSPACE* pDstTangentSpace;
    m_pTangentSpaceVB->Lock( 0, 0, ( VOID** )&pDstTangentSpace, 0 );
    memcpy( pDstTangentSpace, pSrcTangentSpace, sizeof( TANGENTSPACE ) * m_dwNumVertices );
    m_pTangentSpaceVB->Unlock();

    // Cleanup
    delete[] pSrcTangentSpace;
    delete[] pSrcVertices;
    delete[] pSrcIndices;
}


//--------------------------------------------------------------------------------------
// Name: Render()
// Desc: Render the object with per-pixel lighting
//--------------------------------------------------------------------------------------
HRESULT CBumpyObject::Render( LPDIRECT3DDEVICE9 pd3dDevice )
{
    // Process the vertices for the per-pixel lighting effect.
    ProcessVertices( g_vDirLightDirection, g_vPointLightPos );

    pd3dDevice->SetTexture( 0, m_pBaseTexture );
    pd3dDevice->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
    pd3dDevice->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
    pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP );
    pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP );

    pd3dDevice->SetTexture( 1, m_pNormalMap );
    pd3dDevice->SetSamplerState( 1, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
    pd3dDevice->SetSamplerState( 1, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
    pd3dDevice->SetSamplerState( 1, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP );
    pd3dDevice->SetSamplerState( 1, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP );

    pd3dDevice->SetTexture( 2, m_pCubeMap );
    pd3dDevice->SetSamplerState( 2, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
    pd3dDevice->SetSamplerState( 2, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
    pd3dDevice->SetSamplerState( 2, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP );
    pd3dDevice->SetSamplerState( 2, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP );

    // Set our custom shaders
    pd3dDevice->SetVertexDeclaration( m_pVertexDeclaration );
    pd3dDevice->SetVertexShader( m_pVertexShader );
    pd3dDevice->SetPixelShader( m_pPixelShader );

    // Turn each light on/off
    // NOTE: This is for illustration purposes only - for optimum performance,
    // shaders should be specifically compiled and optimized for the number
    // of active lights
    if( g_bEnableAmbientLightPass )
        pd3dDevice->SetPixelShaderConstantF( 0, ( FLOAT* )&g_vAmbientLightColor, 1 );
    else
        pd3dDevice->SetPixelShaderConstantF( 0, ( FLOAT* )&g_vZero, 1 );

    if( g_bEnableDirLightPass )
        pd3dDevice->SetPixelShaderConstantF( 1, ( FLOAT* )&g_vDirectionalLightColor, 1 );
    else
        pd3dDevice->SetPixelShaderConstantF( 1, ( FLOAT* )&g_vZero, 1 );

    if( g_bEnablePointLightPass )
        pd3dDevice->SetPixelShaderConstantF( 2, ( FLOAT* )&g_vPointLightColor, 1 );
    else
        pd3dDevice->SetPixelShaderConstantF( 2, ( FLOAT* )&g_vZero, 1 );

    if( g_bEnableBaseTexturePass )
        pd3dDevice->SetPixelShaderConstantF( 3, ( FLOAT* )&g_vOne, 1 );
    else
        pd3dDevice->SetPixelShaderConstantF( 3, ( FLOAT* )&g_vZero, 1 );

    // Render the object
    pd3dDevice->SetStreamSource( 0, m_pSphereVerticesVB, 0, sizeof( CUSTOMVERTEX ) );
    pd3dDevice->SetStreamSource( 1, m_pTangentSpaceVB, 0, sizeof( TANGENTSPACE ) );
    pd3dDevice->SetIndices( m_pSphereIndicesIB );
    pd3dDevice->DrawIndexedPrimitive( D3DPT_TRIANGLELIST, 0, 0, 0, 0, m_dwNumIndices / 3 );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: ProcessVertices()
// Desc: Setup vertex shader constants for the per-pixel lighting effect
//--------------------------------------------------------------------------------------
VOID CBumpyObject::ProcessVertices( XMVECTOR vDirLightDir, XMVECTOR vPtLightPos )
{
    // Compute the matrix set
    XMMATRIX matMatrixSet = XMMatrixTranspose( g_matWorld * g_matView * g_matProj );

    // Get inverse of world matrix
    XMVECTOR vDeterminant;
    XMMATRIX matInvWorld = XMMatrixInverse( &vDeterminant, g_matWorld );

    // Transform point light position into object space
    XMVECTOR vPointtLightWorldPos = XMVector3TransformCoord( vPtLightPos, matInvWorld );

    // Transform directional light direction into object space
    matInvWorld._41 = matInvWorld._42 = matInvWorld._43 = 0;
    XMVECTOR vDirLightWorldDir = XMVector3TransformCoord( -vDirLightDir, matInvWorld );
    vDirLightWorldDir = XMVector3Normalize( vDirLightWorldDir );

    // If we are using a vertex shader, we simply pass variables to
    // the vertex shader, and the vertex shader will do the rest.
    m_pd3dDevice->SetVertexShaderConstantF( 4, ( FLOAT* )&matMatrixSet, 4 );
    m_pd3dDevice->SetVertexShaderConstantF( 10, ( FLOAT* )&vDirLightWorldDir, 1 );
    m_pd3dDevice->SetVertexShaderConstantF( 11, ( FLOAT* )&vPointtLightWorldPos, 1 );
}


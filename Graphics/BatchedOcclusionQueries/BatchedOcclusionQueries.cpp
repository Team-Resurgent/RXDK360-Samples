//--------------------------------------------------------------------------------------
// BatchedOcclusionQueries.cpp
//
// The new batched occlusion query interface provides a simpler and faster API for
// issuing a large number occlusion queries in rapid succession.  This is useful
// for titles which use occlusion queries for a large number of game objects for
// visibility culling or other techniques.
//
// The new batched occlusion query API is faster in two major ways.
// The first is when issuing the query.
//
// With normal occlusion queries, the pattern looks like:
// pQueryA->Issue( D3DISSUE_BEGIN );
// Draw( A );
// pQueryA->Issue( D3DISSUE_END );
// pQueryB->Issue( D3DISSUE_BEGIN );
// Draw( B );
// pQueryB->Issue( D3DISSUE_END );
//
// With batched occlusion queries, the pattern looks like:
// pBatchedQuery->Issue(); // Slot 0
// Draw( A );
// pBatchedQuery->Issue(); // Slot 1
// Draw( B );
// pBatchedQuery->Issue(); // Slot 2
//
// This reduces the number of Issues() calls from 2*N to N+1 which saves a
// significant amount of CPU resources when querying a large number of objects
// back to back.
//
// The second performance improvement is when retrieving the query results.
//
// With normal occlusion queries, the pattern looks like:
// pQueryA->GetData( &A_pixel_count, sizeof(A_pixel_count) );
// pQueryB->GetData( &B_pixel_count, sizeof(B_pixel_count) );
//
// With batched occlusion queries, the pattern looks like:
// pBatchedQuery->Lock();
// A_pixel_count = pBatchedQuery->GetPixelCount( 1 ) - pBatchedQuery->GetPixelCount( 0 );
// B_pixel_count = pBatchedQuery->GetPixelCount( 2 ) - pBatchedQuery->GetPixelCount( 1 );
// pBatchedQuery->Unlock();
//
// The normal query API must perform a cache flush on every GetData() call
// so that it retrieves the correct results written to memory from the GPU.
// As a result, there is an unavoidable cache miss every time.
// The new batched API can flush the entire batch of queries all at once when you call
// Lock(). This significantly reduces the number of cache misses when retrieving
// a large number of results with GetPixelCount() and GetOcclusionStats()
//
// XNA Developer Connection
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include <xtl.h>
#include <XGraphics.h>

#include <AtgApp.h>
#include <AtgDebugDraw.h>
#include <AtgFont.h>
#include <AtgHelp.h>
#include <AtgInput.h>
#include <AtgSimpleShaders.h>
#include <AtgUtil.h>
#include <xgraphics.h>

//--------------------------------------------------------------------------------------
// Callouts for labeling the gamepad on the help screen
//--------------------------------------------------------------------------------------
ATG::HELP_CALLOUT g_HelpCallouts[] =
{
    { ATG::HELP_BACK_BUTTON,    ATG::HELP_PLACEMENT_2, L"Display\nhelp" },
    { ATG::HELP_LEFT_SHOULDER,  ATG::HELP_PLACEMENT_1, L"Fewer asteroids" },
    { ATG::HELP_RIGHT_SHOULDER, ATG::HELP_PLACEMENT_1, L"More asteroids" },
    { ATG::HELP_LEFT_STICK,     ATG::HELP_PLACEMENT_1, L"Rotate camera" },
    { ATG::HELP_MISC_CALLOUT,   ATG::HELP_PLACEMENT_1, L"Triggers move camera in/out" },
    { ATG::HELP_A_BUTTON,       ATG::HELP_PLACEMENT_2, L"Toggle Query\nMethod" },
};

static const DWORD  NUM_HELP_CALLOUTS = ARRAYSIZE( g_HelpCallouts );

//--------------------------------------------------------------------------------------
// Name: enum QueryMethod
// Desc: Declares the different instancing methods
//--------------------------------------------------------------------------------------
enum QueryMethod
{
    BATCHED_OCCLUSION_QUERIES = 0,
    NORMAL_OCCLUSION_QUERIES,
    NUM_QUERY_METHODS,
    FORCE_DWORD         = 0xFFFFFFFF,
};

WCHAR* g_QueryTypeName[] =
{
    L"Batched Occlusion Queries",
    L"Normal Occlusion Queries",
};

// GPU resources that are read/modified on the CPU should be double buffered
// so that the CPU does not block waiting on the GPU to finish with the resource
const DWORD BUFFER_FRAMES = 3;

// Asteroid Data
const DWORD MAX_ASTEROID_COUNT = 10000;

struct InstanceSeed
{
    FLOAT m_fRadius;
    FLOAT m_fAngle;
    FLOAT m_fSize;
    FLOAT m_fY;
};

struct InstanceData
{
    XMFLOAT3 m_vPosition;
    FLOAT m_fScale;
};

InstanceData g_instanceData[ MAX_ASTEROID_COUNT ]; // Updated every frame
InstanceSeed g_instanceSeed[ MAX_ASTEROID_COUNT ]; // Frame independent instance data

VOID GenerateTexturedSphereGeometry( DWORD dwNumSlices, DWORD dwNumStacks,
                                     ATG::MeshVertexPT* pData, FLOAT fUVScaler );
VOID GenerateSphereIndices( DWORD dwNumSlices, DWORD dwNumStacks, WORD* pIndices );

//--------------------------------------------------------------------------------------
// Name: class Sample
// Desc: Main class to run this application. Most functionality is inherited
//       from the ATG::Application base class.
//--------------------------------------------------------------------------------------
class Sample : public ATG::Application
{
public:
    Sample() : m_bDrawHelp( FALSE )
    {
    }

    virtual ~Sample()
    {
        SAFE_RELEASE( m_pPlanetVB );
        SAFE_RELEASE( m_pPlanetIB );
        SAFE_RELEASE( m_pPlanetDecl );
        SAFE_RELEASE( m_pSkydomeVB );
        SAFE_RELEASE( m_pSkydomeIB );

        delete [] m_pNormalQueries;
    }

private:
    virtual HRESULT Initialize();
    virtual HRESULT Update();
    virtual HRESULT Render();

    VOID CreateInstancedMesh();
    VOID CreatePlanetMesh();
    VOID CreateSkydomeMesh();

    VOID InitializeInstanceData();
    VOID UpdateInstanceData( FLOAT fTime );

    VOID RenderOverlays();
    VOID RenderAsteroids();

    VOID AggregateQueries( DWORD& dwNumVisible, DWORD& dwNumPixels );

private:
    ATG::Timer m_Timer;
    ATG::Font m_Font;
    ATG::Help m_Help;
    BOOL m_bDrawHelp;
    FLOAT m_fViewDistance;

    XMMATRIX m_matWorld;
    XMMATRIX m_matView;
    XMMATRIX m_matProj;
    XMMATRIX m_matWVP;
    XMMATRIX m_matWorld_Planet;
    XMMATRIX m_matWVP_Planet;
    XMMATRIX m_matWVP_Sky;

    INT m_nQueryMethod;         // Current query method

    // Planet rendering data
    D3DVertexBuffer* m_pPlanetVB;
    D3DIndexBuffer* m_pPlanetIB;
    D3DVertexDeclaration* m_pPlanetDecl;
    DWORD m_dwNumPlanetIndices;
    D3DVertexShader* m_pPlanetVS;
    D3DPixelShader* m_pPlanetPS;
    D3DTexture* m_pPlanetTexture;

    // Skydome rendering data
    D3DVertexBuffer* m_pSkydomeVB;
    D3DIndexBuffer* m_pSkydomeIB;
    D3DVertexDeclaration* m_pSkydomeDecl;
    DWORD m_dwNumSkydomeIndices;
    D3DTexture* m_pSkydomeTex;

    // Asteroid Rendering Data
    INT m_nAsteroidCount;
    
    D3DVertexBuffer* m_pAsteroidVB;
    D3DIndexBuffer* m_pAsteroidIB;
    D3DVertexDeclaration* m_pAsteroidDecl;
    DWORD m_dwNumAsteroidIndices;
    D3DTexture* m_pCraterTexture;
    D3DTexture* m_pDisplacementMap;

    // Timing counters
    FLOAT m_fIssueTime;
    FLOAT m_fAggregateTime;

    FXLEffect* m_pEffect;
    FXLHANDLE m_hWVPMatrix;
    FXLHANDLE m_hInstanceData;

    // Occlusion Queries
    DWORD m_dwBufferNumber;
    // Normal Queries - One per asteroid.  Double buffered
    IDirect3DQuery9* ( * m_pNormalQueries )[ MAX_ASTEROID_COUNT ]; // Will be allocated on the heap in Initialize()
    DWORD m_dwNormalQueriesIssued[BUFFER_FRAMES];
    // Batched Queries - One slot per asteroid + 1.  Double buffered
    IDirect3DOcclusionQueryBatch9* m_pBatchedQueries[BUFFER_FRAMES];
};


//--------------------------------------------------------------------------------------
// Name: main()
// Desc: Entry point to the program
//--------------------------------------------------------------------------------------
INT __cdecl main()
{
    Sample atgApp;

    // This sample runs exclusively at 720p.  The video scaler will handle all other
    // output resolutions.
    atgApp.m_d3dpp.BackBufferWidth = 1280;
    atgApp.m_d3dpp.BackBufferHeight = 720;

    atgApp.m_dwDeviceCreationFlags |= D3DCREATE_BUFFER_2_FRAMES;

    atgApp.m_d3dpp.BackBufferFormat = ( D3DFORMAT )MAKESRGBFMT( D3DFMT_LE_X8R8G8B8 );
    atgApp.m_d3dpp.FrontBufferFormat = ( D3DFORMAT )MAKESRGBFMT( D3DFMT_LE_X8R8G8B8 );

    atgApp.Run();
}

//--------------------------------------------------------------------------------------
// Name: GenerateTexturedSphere()
// Desc: Creates a sphere with texture coordinates
//--------------------------------------------------------------------------------------
VOID GenerateTexturedSphere( DWORD numSlices, DWORD numStacks, D3DVertexBuffer** pVB,
                             D3DIndexBuffer** pIB, DWORD* numIndices,
                             D3DVertexDeclaration** pDecl, FLOAT fUVScaler )
{
    // Create a vertex buffer and copy the mesh vertex data into it
    ATG::g_pd3dDevice->BlockUntilIdle();
    ATG::g_pd3dDevice->CreateVertexBuffer(
        sizeof( ATG::MeshVertexPT ) * ( numSlices + 1 ) * ( numStacks + 1 ),
        0, 0, D3DPOOL_DEFAULT, pVB, NULL );
    ATG::MeshVertexPT* pVBData = NULL;
    ( *pVB )->Lock( 0, 0, ( VOID** )&pVBData, 0 );
    GenerateTexturedSphereGeometry( numSlices, numStacks, pVBData, fUVScaler );
    ( *pVB )->Unlock();

    *numIndices = 4 * numSlices * numStacks;

    // Create an index buffer and copy in the mesh index data.
    ATG::g_pd3dDevice->CreateIndexBuffer( *numIndices * sizeof( WORD ),
                                          0, D3DFMT_INDEX16, D3DPOOL_DEFAULT,
                                          pIB, NULL );
    WORD* pIBData = NULL;
    ( *pIB )->Lock( 0, 0, ( VOID** )&pIBData, 0 );
    GenerateSphereIndices( numSlices, numStacks, pIBData );
    ( *pIB )->Unlock();

    // Create the vertex declaration.
    static const D3DVERTEXELEMENT9 TexturedSphereDecl[] =
    {
        { 0,  0, D3DDECLTYPE_FLOAT3, 0, D3DDECLUSAGE_POSITION, 0 },
        { 0, 12, D3DDECLTYPE_FLOAT2, 0, D3DDECLUSAGE_TEXCOORD, 0 },
        D3DDECL_END()
    };

    ATG::g_pd3dDevice->CreateVertexDeclaration( TexturedSphereDecl, pDecl );
}


//--------------------------------------------------------------------------------------
// Name: GenerateTexturedSphereGeometry()
// Desc: Creates geometry for a sphere
//--------------------------------------------------------------------------------------
VOID GenerateTexturedSphereGeometry( DWORD numSlices, DWORD numStacks,
                                     ATG::MeshVertexPT* pData, FLOAT fUVScaler )
{
    for( DWORD i = 0; i < numSlices + 1; i++ )
    {
        for( DWORD j = 0; j < numStacks + 1; j++ )
        {
            FLOAT fTheta = FLOAT( i ) / numSlices * 2 * XM_PI;
            FLOAT fPhi = ( FLOAT( j ) / numStacks * 2 - 1.0f ) * XM_PIDIV2;
            pData->TexCoord.x = FLOAT( i ) / numSlices * fUVScaler;
            pData->TexCoord.y = FLOAT( j ) / numStacks * fUVScaler;
            pData->Position.x = cosf( fTheta ) * cosf( fPhi );
            pData->Position.z = sinf( fTheta ) * cosf( fPhi );
            pData->Position.y = sinf( fPhi );

            pData++;
        }
    }
}


//--------------------------------------------------------------------------------------
// Name: GenerateSphereIndices()
// Desc: Creates indices for a sphere
//--------------------------------------------------------------------------------------
VOID GenerateSphereIndices( DWORD dwNumSlices, DWORD dwNumStacks, WORD* pIndices )
{
    WORD i0 = ( WORD )0;
    WORD i1 = ( WORD )1;
    WORD i2 = ( WORD )dwNumStacks + 2;
    WORD i3 = ( WORD )dwNumStacks + 1;

    for( DWORD i = 0; i < dwNumSlices; i++ )
    {
        for( DWORD j = 0; j < dwNumStacks; j++ )
        {
            pIndices[0] = i0;
            pIndices[1] = i1;
            pIndices[2] = i2;
            pIndices[3] = i3;

            pIndices += 4;
            i0++;
            i1++;
            i2++;
            i3++;
        }
        i0++;
        i1++;
        i2++;
        i3++;
    }
}


//--------------------------------------------------------------------------------------
// Name: Initialize()
// Desc: Creates all graphics resources and initializes the instanced mesh system.
//--------------------------------------------------------------------------------------
HRESULT Sample::Initialize()
{
    // Normal Queries - One per asteroid.  Double buffered
    m_pNormalQueries = new IDirect3DQuery9* [ BUFFER_FRAMES ][ MAX_ASTEROID_COUNT ];

    m_nQueryMethod = BATCHED_OCCLUSION_QUERIES;
    m_nAsteroidCount = 5000;

    // Resize the Direct3D device ring buffer.
    // Resizing the ring buffer supports a larger or smaller amount of draw calls per scene.
    // The default is 2MB of secondary ring buffer; here we'll change that to 4MB.
    D3DRING_BUFFER_PARAMETERS RBParams = { 0 };
    RBParams.PrimarySize = 0;  // Direct3D will use the default size of 32KB
    RBParams.SecondarySize = 4 * 1024 * 1024;
    RBParams.SegmentCount = 0; // Direct3D will use the default segment count of 32

    // Setting the pPrimary and pSecondary members to NULL means that Direct3D will
    // allocate the ring buffers itself.  You can optionally provide a buffer that
    // you allocated yourself (it must be write-combined physical memory, aligned to
    // GPU_COMMAND_BUFFER_ALIGNMENT).
    RBParams.pPrimary = NULL;
    RBParams.pSecondary = NULL;

    m_pd3dDevice->SetRingBufferParameters( &RBParams );

    // Create the font
    if( FAILED( m_Font.Create( "d:\\Media\\Fonts\\Arial_16.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Confine text drawing to the title safe area
    m_Font.SetWindow( ATG::GetTitleSafeArea() );

    // Create the help
    if( FAILED( m_Help.Create( "d:\\Media\\Help\\Help.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Build the projection and world matrices
    m_matProj = XMMatrixPerspectiveFovLH( XM_PI / 3.0f, ( 16.0f / 9.0f ), .01f, 20000.0f );
    m_matWorld = XMMatrixIdentity();
    m_fViewDistance = 6.0f;

    // Initialize the simple shaders library
    ATG::SimpleShaders::Initialize( NULL, NULL );


    // Load textures
    HRESULT hr = D3DXCreateTextureFromFile( m_pd3dDevice,
                                            "game:\\media\\textures\\pgas06L.dds",
                                            &m_pPlanetTexture );
    if( FAILED( hr ) )
        return ATGAPPERR_MEDIANOTFOUND;

    hr = D3DXCreateTextureFromFile( m_pd3dDevice,
                                    "game:\\media\\textures\\starfield.dds",
                                    &m_pSkydomeTex );
    if( FAILED( hr ) )
        return ATGAPPERR_MEDIANOTFOUND;

    hr = D3DXCreateTextureFromFile( m_pd3dDevice,
                                    "game:\\media\\textures\\craters.dds",
                                    &m_pCraterTexture );
    if( FAILED( hr ) )
        return ATGAPPERR_MEDIANOTFOUND;

    hr = D3DXCreateTextureFromFile( m_pd3dDevice,
                                    "game:\\media\\textures\\displacement.dds",
                                    &m_pDisplacementMap );
    if( FAILED( hr ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Load Shaders
    hr = ATG::LoadVertexShader( "game:\\media\\shaders\\PlanetVS.xvu", &m_pPlanetVS );
    if( FAILED( hr ) )
        return ATGAPPERR_MEDIANOTFOUND;

    hr = ATG::LoadPixelShader( "game:\\media\\shaders\\PlanetPS.xpu", &m_pPlanetPS );
    if( FAILED( hr ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Load FXLite effect.
    BYTE* pEffectData = NULL;
    ATG::LoadFile( "game:\\media\\effects\\instancing.fxobj", ( VOID** )&pEffectData,
                   NULL );
    FXLCreateEffect( m_pd3dDevice, pEffectData, NULL, &m_pEffect );

    // Cache FXLite handles.
    m_hWVPMatrix = m_pEffect->GetParameterHandle( "world_view_proj_matrix" );
    m_hInstanceData = m_pEffect->GetParameterHandle( "instance_data" );

    // Force the texture formats to AS_16 sRGB formats.
    // Do this so there's no loss of precision when sampling the textures in shaders. If using
    // standard SRGB formats, the sRGB->Linear conversion happens with 8 bit precision, resulting 
    // in a loss of data. Using AS_16 sRGB formats causes the conversion to happen at 16-bit precision,
    // meaning no precision is lost.
    ATG::ConvertTextureToAs16SRGBFormat( m_pPlanetTexture );
    ATG::ConvertTextureToAs16SRGBFormat( m_pSkydomeTex );
    ATG::ConvertTextureToAs16SRGBFormat( m_pCraterTexture );

    // Create geometry
    CreateInstancedMesh();
    CreatePlanetMesh();
    CreateSkydomeMesh();

    InitializeInstanceData();

    // Create Queries
    m_dwBufferNumber = 0;
    for (UINT i = 0 ; i < BUFFER_FRAMES ; i++)
    {
        for (UINT q = 0 ; q < MAX_ASTEROID_COUNT ; q++)
        {
            m_pd3dDevice->CreateQuery( D3DQUERYTYPE_OCCLUSION, &m_pNormalQueries[i][q] );
        }
        m_dwNormalQueriesIssued[i] = 0;
        m_pd3dDevice->CreateOcclusionQueryBatch( MAX_ASTEROID_COUNT+1, 1, &m_pBatchedQueries[i] ); 
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: CreateInstancedMesh()
// Desc: This creates vertex and index data for a instance, and loads that data into the
//       instanced mesh system.
//--------------------------------------------------------------------------------------
VOID Sample::CreateInstancedMesh()
{
    const DWORD dwNumStacks = 16;
    const DWORD dwNumSlices = 16;
    GenerateTexturedSphere( dwNumStacks, dwNumSlices, &m_pAsteroidVB, &m_pAsteroidIB,
                            &m_dwNumAsteroidIndices, &m_pAsteroidDecl, 1.0f );
}


//--------------------------------------------------------------------------------------
// Name: CreatePlanetMesh()
// Desc: Creates vertex and index data for the planet
//--------------------------------------------------------------------------------------
VOID Sample::CreatePlanetMesh()
{
    const DWORD dwNumStacks = 100;
    const DWORD dwNumSlices = 50;
    GenerateTexturedSphere( dwNumStacks, dwNumSlices, &m_pPlanetVB, &m_pPlanetIB,
                            &m_dwNumPlanetIndices, &m_pPlanetDecl, 1.0f );
}


//--------------------------------------------------------------------------------------
// Name: CreateSkydomeMesh()
// Desc: Creates vertex and index data for the skydome
//--------------------------------------------------------------------------------------
VOID Sample::CreateSkydomeMesh()
{
    const DWORD dwNumStacks = 100;
    const DWORD dwNumSlices = 50;
    GenerateTexturedSphere( dwNumStacks, dwNumSlices, &m_pSkydomeVB, &m_pSkydomeIB,
                            &m_dwNumSkydomeIndices, &m_pSkydomeDecl, 2.0f );
}


//--------------------------------------------------------------------------------------
// Name: Update()
// Desc: Called once per frame.  Entry point for animating the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Update()
{
    // Get the current time.
    FLOAT fTime = ( FLOAT )m_Timer.GetAppTime();
    FLOAT fDeltaTime = ( FLOAT )m_Timer.GetElapsedTime();

    // Get the current gamepad state.
    ATG::GAMEPAD* pGamepad = ATG::Input::GetMergedInput();

    // Compute camera rotation.
    static FLOAT s_fRotateY = XM_PIDIV2;
    static FLOAT s_fRotateX = 0.3f;
    s_fRotateY += pGamepad->fX1 * fDeltaTime * XM_PIDIV2;
    s_fRotateX += pGamepad->fY1 * fDeltaTime * XM_PIDIV2;
    s_fRotateX = min( max( s_fRotateX, -XM_PI / 5.0f ), XM_PI / 5.0f );
    XMVECTOR qRotateY = XMQuaternionRotationAxis( XMVectorSet( 0, 1, 0, 0 ), s_fRotateY );
    XMVECTOR qRotateX = XMQuaternionRotationAxis( XMVectorSet( 1, 0, 0, 0 ), s_fRotateX );
    XMVECTOR qRotation = XMQuaternionMultiply( qRotateX, qRotateY );

    // Compute camera distance.
    m_fViewDistance += 8.0f * fDeltaTime * ( ( FLOAT )pGamepad->bLeftTrigger / 255.0f );
    m_fViewDistance -= 8.0f * fDeltaTime * ( ( FLOAT )pGamepad->bRightTrigger / 255.0f );
    if( m_fViewDistance < 2.0f )
        m_fViewDistance = 2.0f;

    // Compose view matrix from camera settings.
    XMVECTOR vEyePt = XMVectorSet( 0.0f, 0.0f, -m_fViewDistance, 0.0f );
    vEyePt = XMVector3Rotate( vEyePt, qRotation );
    XMVECTOR vLookatPt = XMVectorSet( 0.0f, 0.0f, 0.0f, 0.0f );
    XMVECTOR vUpVec = XMVectorSet( 0.0f, 1.0f, 0.0f, 0.0f );
    vUpVec = XMVector3Rotate( vUpVec, qRotation );
    vEyePt += vLookatPt;
    m_matView = XMMatrixLookAtLH( vEyePt, vLookatPt, vUpVec );

    // Update the world view projection matrix for the instances
    m_matWVP = m_matWorld * m_matView * m_matProj;

    // Update the matrices for the planet
    static FLOAT fYRot = 0.0f;
    const FLOAT rotSpeed = -.05f;
    fYRot += rotSpeed * fDeltaTime;
    const FLOAT fXRot = .4f;
    XMMATRIX matRotY = XMMatrixRotationY( fYRot );
    XMMATRIX matRotX = XMMatrixRotationX( fXRot );
    m_matWorld_Planet = matRotY * matRotX * m_matWorld;
    m_matWVP_Planet = matRotY * matRotX * m_matWorld * m_matView * m_matProj;

    // Update the matrices for the skydome
    const FLOAT fSkyDomeScale = 1000.0f;
    XMMATRIX matScale = XMMatrixScaling( fSkyDomeScale, fSkyDomeScale, fSkyDomeScale );
    m_matWVP_Sky = matScale * m_matWorld * m_matView * m_matProj;

    // The Back button toggles the help screen.
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_BACK )
        m_bDrawHelp = !m_bDrawHelp;

    // The shoulder buttons change the mesh instance count.
    if( pGamepad->wLastButtons & XINPUT_GAMEPAD_LEFT_SHOULDER )
    {
        m_nAsteroidCount = max( 1, m_nAsteroidCount - 10 );
        InitializeInstanceData();
    }
    else if( pGamepad->wLastButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER )
    {
        m_nAsteroidCount = min( MAX_ASTEROID_COUNT, m_nAsteroidCount + 10 );
        InitializeInstanceData();
    }

    // A and B change the query method
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_A )
    {
        m_nQueryMethod = ( m_nQueryMethod + 1 ) % NUM_QUERY_METHODS;
    }

    // Update the mesh instance data.
    UpdateInstanceData( fTime );

    return S_OK;
}

//--------------------------------------------------------------------------------------
// Name: InitializeInstanceData()
// Desc: Creates new instance data for the instanced meshes.  This function is called
//       only when the settings change.
//--------------------------------------------------------------------------------------
VOID Sample::InitializeInstanceData()
{
    XMemSet( &g_instanceData, 0, sizeof( g_instanceData ) );

    // Seed the random number generator to stay consistent across changes in settings
    srand( 1 );

    const FLOAT fRadius = 3.0f;
    for( INT i = 0; i < m_nAsteroidCount; ++i )
    {
        FLOAT frand1 = ( rand() / FLOAT( RAND_MAX ) ) * 2.0f - 1.0f;
        FLOAT frand2 = ( rand() / FLOAT( RAND_MAX ) ) * 2.0f - 1.0f;
        FLOAT frand3 = ( rand() / FLOAT( RAND_MAX ) ) * 2.0f - 1.0f;
        FLOAT frand4 = ( rand() / FLOAT( RAND_MAX ) ) * 2.0f - 1.0f;
        static FLOAT yScaler = .1f;

        g_instanceSeed[i].m_fRadius = fRadius + frand1 * .5f;
        g_instanceSeed[i].m_fAngle = frand3 * XM_PI;

        // Scale and position can be set now, since they don't change per frame
        g_instanceData[i].m_fScale = .007f * frand2 + .01f;
        g_instanceData[i].m_vPosition.y = frand4 * yScaler;
    }
}


//--------------------------------------------------------------------------------------
// Name: UpdateInstanceData()
// Desc: Creates new instance data (positions and sizes) for the instanced meshes.
//       Called once per frame
//--------------------------------------------------------------------------------------
VOID Sample::UpdateInstanceData( FLOAT fTime )
{
    FLOAT fAngle = fTime * ( XM_2PI * 0.01f );

    for( INT i = 0; i < m_nAsteroidCount; ++i )
    {
        float fCurAngle = fAngle + g_instanceSeed[i].m_fAngle;

        g_instanceData[i].m_vPosition.x = g_instanceSeed[i].m_fRadius * sinf( fCurAngle );
        g_instanceData[i].m_vPosition.z = g_instanceSeed[i].m_fRadius * cosf( fCurAngle );
    }
}


//--------------------------------------------------------------------------------------
// Name: Render()
// Desc: Sets up render states, clears the viewport, and renders the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Render()
{
    m_pd3dDevice->Clear( 0, NULL, D3DCLEAR_TARGET0 | D3DCLEAR_ZBUFFER,
                              0x00000000, 1.0f, 0 );

    // Draw the planet
    PIXBeginNamedEvent( 0xFFFFFFFF, "Planet" );
    m_pd3dDevice->SetVertexShaderConstantF( 0, ( FLOAT* )&m_matWVP_Planet, 4 );
    m_pd3dDevice->SetVertexShaderConstantF( 4, ( FLOAT* )&m_matWorld_Planet, 4 );
    m_pd3dDevice->SetVertexShader( m_pPlanetVS );
    m_pd3dDevice->SetPixelShader( m_pPlanetPS );
    m_pd3dDevice->SetStreamSource( 0, m_pPlanetVB, 0, sizeof( ATG::MeshVertexPT ) );
    m_pd3dDevice->SetIndices( m_pPlanetIB );
    m_pd3dDevice->SetVertexDeclaration( m_pPlanetDecl );
    m_pd3dDevice->SetTexture( 0, m_pPlanetTexture );
    m_pd3dDevice->DrawIndexedVertices( D3DPT_QUADLIST, 0, 0, m_dwNumPlanetIndices );
    PIXEndNamedEvent();

    // Draw the background
    PIXBeginNamedEvent( 0xFFFFFFFF, "Background" );
    ATG::SimpleShaders::BeginShader_Transformed_Textured( m_matWVP_Sky, m_pSkydomeTex );
    m_pd3dDevice->SetStreamSource( 0, m_pSkydomeVB, 0, sizeof( ATG::MeshVertexPT ) );
    m_pd3dDevice->SetIndices( m_pSkydomeIB );
    m_pd3dDevice->SetVertexDeclaration( m_pSkydomeDecl );
    m_pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_CW );
    m_pd3dDevice->DrawIndexedVertices( D3DPT_QUADLIST, 0, 0, m_dwNumSkydomeIndices );
    ATG::SimpleShaders::EndShader();
    PIXEndNamedEvent();

    RenderAsteroids();

    RenderOverlays();

    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: RenderAsteroids()
// Desc: Render the instances using the currently selected method
//--------------------------------------------------------------------------------------
VOID Sample::RenderAsteroids()
{
    ATG::Timer CPUTimer;

    // Draw the mesh instances.
    PIXBeginNamedEvent( 0, "Render Asteroids" );

    m_pEffect->BeginTechniqueFromIndex( 1, 0 );
    m_pEffect->BeginPassFromIndex( 0 );

    // Set WVP matrix and instance params to effect.
    m_pEffect->SetMatrixF4x4A( m_hWVPMatrix, ( const FXLFLOATA* )&m_matWVP );
    m_pEffect->Commit();

    // Set the vertex stream, indices, and vertex decl.
    m_pd3dDevice->SetTexture( D3DVERTEXTEXTURESAMPLER1, m_pDisplacementMap );
    m_pd3dDevice->SetStreamSource( 0, m_pAsteroidVB, 0, sizeof( ATG::MeshVertexPT ) );
    m_pd3dDevice->SetIndices( m_pAsteroidIB );
    m_pd3dDevice->SetVertexDeclaration( m_pAsteroidDecl );

    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSW, D3DTADDRESS_WRAP );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetTexture( 0, m_pCraterTexture );

    // Next Double Buffer
    // That way the CPU can read the results from the previous frame without blocking
    m_dwBufferNumber = (m_dwBufferNumber + 1) % BUFFER_FRAMES;

    if (m_nQueryMethod == NORMAL_OCCLUSION_QUERIES) {
        for( INT i = 0; i < m_nAsteroidCount; i++ )
        {
            // When using Normal Occlusion Queries,
            // You must Issue() with D3DISSUE_BEGIN/D3DISSUE_END surrounding what you want to measure
            m_pNormalQueries[m_dwBufferNumber][i]->Issue( D3DISSUE_BEGIN );
            m_pd3dDevice->SetVertexShaderConstantF( 5, ( FLOAT* )&g_instanceData[i], 1 );
            m_pd3dDevice->DrawIndexedVertices( D3DPT_QUADLIST, 0, 0, m_dwNumAsteroidIndices );
            m_pNormalQueries[m_dwBufferNumber][i]->Issue( D3DISSUE_END );
        }
        m_dwNormalQueriesIssued[m_dwBufferNumber] = m_nAsteroidCount;
    }
    else if (m_nQueryMethod == BATCHED_OCCLUSION_QUERIES) {
        m_pBatchedQueries[m_dwBufferNumber]->Reset();
        m_pBatchedQueries[m_dwBufferNumber]->Issue();
        for( INT i = 0; i < m_nAsteroidCount; i++ )
        {
            // Batched Occlusion Queries are designed to be used back to back,
            // So that the ending issue for the previous measurement is the starting issue
            // for the next one.  In this sample, the ending Issue() from the previous Asteroid
            // functions as the start Issue() for this one
            m_pd3dDevice->SetVertexShaderConstantF( 5, ( FLOAT* )&g_instanceData[i], 1 );
            m_pd3dDevice->DrawIndexedVertices( D3DPT_QUADLIST, 0, 0, m_dwNumAsteroidIndices );     
            m_pBatchedQueries[m_dwBufferNumber]->Issue();
        }
    }

    m_pEffect->EndPass();
    m_pEffect->EndTechnique();

    PIXEndNamedEvent();

    m_fIssueTime = ( FLOAT )CPUTimer.GetElapsedTime();
    m_Timer.MarkFrame();
}


//--------------------------------------------------------------------------------------
// Name: RenderOverlays()
// Desc: Aggragates the Occlusion Query statistics
//--------------------------------------------------------------------------------------
VOID Sample::AggregateQueries( DWORD& dwNumVisible, DWORD& dwNumPixels )
{
    DWORD dwReadBuffer = (m_dwBufferNumber + 1) % BUFFER_FRAMES;
    DWORD dwVisibleSum = 0;
    DWORD dwPixelSum = 0;
    
    ATG::Timer CPUTimer;
    
    if (m_nQueryMethod == NORMAL_OCCLUSION_QUERIES) {
        // When using Normal Occlusion Queries,
        // the results are retrieved with the GetData function
        // GetData will ensure that the cache is flushed so that the correct
        // results that the GPU wrote are read from memory
        for( DWORD i = 0; i < m_dwNormalQueriesIssued[dwReadBuffer]; i++ )
        {
            DWORD pixels = 0;
            m_pNormalQueries[dwReadBuffer][i]->GetData( &pixels, sizeof(pixels), 0 );
            dwPixelSum += pixels;
            if (pixels > 0) { dwVisibleSum++; }
        }
        m_dwNormalQueriesIssued[dwReadBuffer] = 0;
    }
    else if (m_nQueryMethod == BATCHED_OCCLUSION_QUERIES) {
        // With Batched Occlusion Queries,
        // Lock() performs the job of flushing the cache to ensure correct
        // results from the GPU.  This significatly reduces the number
        // of cache misses when calling GetPixelCount/GetOcclusionStats
        VOID* pBuffer = NULL;
        m_pBatchedQueries[dwReadBuffer]->Lock(&pBuffer);
        for( DWORD i = 0; (i+1) < m_pBatchedQueries[dwReadBuffer]->GetNumSlotsUsed(); i++ )
        {
            // Unlike normal queries, GetPixelCount does not return the number
            // of pixels for that single draw call.  Instead, it returns the total number of
            // pixels rendered by the GPU.  You must collect the total before and after the
            // draw and subtract them to get that draw call's total.
            UINT start = m_pBatchedQueries[dwReadBuffer]->GetPixelCount(i);
            UINT end = m_pBatchedQueries[dwReadBuffer]->GetPixelCount(i+1);
            DWORD pixels = end - start;
            dwPixelSum += pixels;
            if (pixels > 0) { dwVisibleSum++; }
        }
        m_pBatchedQueries[dwReadBuffer]->Unlock();
    }
    
    m_fAggregateTime = ( FLOAT )CPUTimer.GetElapsedTime();

    dwNumVisible = dwVisibleSum;
    dwNumPixels = dwPixelSum;
}

//--------------------------------------------------------------------------------------
// Name: RenderOverlays()
// Desc: Show title, timers, and help.
//--------------------------------------------------------------------------------------
VOID Sample::RenderOverlays()
{
    PIXBeginNamedEvent( 0xFFFFFFFF, "Overlays" );
    if( m_bDrawHelp )
    {
        m_Help.Render( &m_Font, g_HelpCallouts, NUM_HELP_CALLOUTS );
    }
    else
    {
        m_Font.Begin();

        m_Font.SetScaleFactors( 1.2f, 1.2f );
        m_Font.DrawText( 0, 0, 0xffffffff, L"Batched Occlusion Queries" );

        m_Font.SetScaleFactors( 1.0f, 1.0f );
        m_Font.DrawText( 0, 0, 0xffffff00, m_Timer.GetFrameRate(), ATGFONT_RIGHT );
        m_Font.SetScaleFactors( 0.8f, 0.8f );

        FLOAT fYPos = 30;
        WCHAR strText[100];
        m_Font.DrawText( 0, fYPos, 0xff00ffff, g_QueryTypeName[ m_nQueryMethod ],
                         ATGFONT_RIGHT );
        fYPos += 40;

        DWORD dwNumVisible = 0;
        DWORD dwNumPixels = 0;
        AggregateQueries( dwNumVisible, dwNumPixels );

        swprintf_s( strText, L"Objects: %u", m_nAsteroidCount );
        m_Font.DrawText( 0, fYPos, 0xFF8080FF, strText, ATGFONT_RIGHT );
        fYPos += 20;

        swprintf_s( strText, L"Objects Visible: %u", dwNumVisible );
        m_Font.DrawText( 0, fYPos, 0xFF8080FF, strText, ATGFONT_RIGHT );
        fYPos += 20;
        
        swprintf_s( strText, L"Pixels Visible: %u", dwNumPixels );
        m_Font.DrawText( 0, fYPos, 0xFF8080FF, strText, ATGFONT_RIGHT );
        fYPos += 40;

        swprintf_s( strText, L"CPU Issue() Time: %0.4f ms", m_fIssueTime * 1000.0f );
        m_Font.DrawText( 0, fYPos, 0xFFFFFF80, strText, ATGFONT_RIGHT );
        fYPos += 20;

        swprintf_s( strText, L"CPU GetData/GetPixelCount() Time: %0.4f ms", m_fAggregateTime * 1000.0f );
        m_Font.DrawText( 0, fYPos, 0xFFFFFF80, strText, ATGFONT_RIGHT );
        fYPos += 20;

        m_Font.End();
    }
    PIXEndNamedEvent();
}

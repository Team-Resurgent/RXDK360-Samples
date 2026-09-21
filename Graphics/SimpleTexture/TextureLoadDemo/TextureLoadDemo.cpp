// TextureLoadDemo.cpp : Defines the entry point for the application.
//

#include "stdafx.h"
#include <xgraphics.h>

//-------------------------------------------------------------------------------------
// Vertex shader
// We use the register semantic here to directly define the input register
// matWVP.  Conversely, we could let the HLSL compiler decide and check the
// constant table.
//-------------------------------------------------------------------------------------
const char*                                 g_strVertexShaderProgram =
    " float4x4 matWVP : register(c0);              "
    "                                              "
    " struct VS_IN                                 "
    " {                                            "
    "     float4 ObjPos   : POSITION;              "  // Object space position 
    "     float2 TexCoord : TEXCOORD;              "
    " };                                           "
    "                                              "
    " struct VS_OUT                                "
    " {                                            "
    "     float4 ProjPos  : POSITION;              "  // Projected space position 
    "     float2 TexCoord : TEXCOORD;              "
    " };                                           "
    "                                              "
    " VS_OUT main( VS_IN In )                      "
    " {                                            "
    "     VS_OUT Out;                              "
    "     Out.ProjPos = mul( matWVP, In.ObjPos );  "  // Transform vertex into
    "     Out.TexCoord = In.TexCoord;              "
    "     return Out;                              "
    " }                                            ";

//-------------------------------------------------------------------------------------
// Pixel shader
//-------------------------------------------------------------------------------------
const char*                                 g_strPixelShaderProgram =
    " struct PS_IN                                 "
    " {                                            "
    "     float2 TexCoord : TEXCOORD;              "
    " };                                           "  // the vertex shader
    "                                              "
    " sampler detail;                              "
    "                                              "
    " float4 main( PS_IN In ) : COLOR              "
    " {                                            "
    "     return tex2D( detail, In.TexCoord );     "  // Output color
    " }                                            ";

//-------------------------------------------------------------------------------------
// Structure to hold vertex data.
//-------------------------------------------------------------------------------------
struct COLORVERTEX
{
    float   Position[3];
    float   TexCoord[2];
};

//-------------------------------------------------------------------------------------
// Time             Since fAppTime is a float, we need to keep the quadword app time 
//                  as a LARGE_INTEGER so that we don't lose precision after running
//                  for a long time.
//-------------------------------------------------------------------------------------
struct TimeInfo
{
    LARGE_INTEGER qwTime;
    LARGE_INTEGER qwAppTime;

    float fAppTime;
    float fElapsedTime;

    float fSecsPerTick;
};

//-------------------------------------------------------------------------------------
// CInterfacePtr<T> Helper class to make sure we always release our interfaces.
//                  This is a simple resource management class- there are subtleties
//                  when doing assignment or copy of smart pointers, so we make sure 
//                  they aren't called by making copy and assign private.
//-------------------------------------------------------------------------------------
template <class T> class CInterfacePtr
{
public:
            CInterfacePtr()
            {
                m_ptr = NULL;
            }
            CInterfacePtr( T* p )
            {
                m_ptr = p;
            }
            ~CInterfacePtr()
            {
                if( m_ptr ) m_ptr->Release();
            }
    CInterfacePtr& operator =( T* p )
    {
        m_ptr = p; return *this;
    }

    T** operator&()
    {
        return &m_ptr;
    }
    T* operator->()
    {
        return m_ptr;
    }
            operator T*()
            {
                return m_ptr;
            }
    bool    operator!()
    {
        return ( m_ptr == NULL );
    }

private:
            CInterfacePtr( const CInterfacePtr& p );                  // unimplemented copy
    CInterfacePtr& operator =( const CInterfacePtr& p );     // unimplemented assign

    T* m_ptr;
};

//-------------------------------------------------------------------------------------
// Global variables
//-------------------------------------------------------------------------------------
CInterfacePtr <IDirect3DDevice9>            g_pd3dDevice;    // Our rendering device
CInterfacePtr <IDirect3DVertexBuffer9>      g_pVB;           // Buffer to hold vertices
CInterfacePtr <IDirect3DVertexDeclaration9> g_pVertexDecl;   // Vertex format decl
CInterfacePtr <IDirect3DVertexShader9>      g_pVertexShader; // Vertex Shader
CInterfacePtr <IDirect3DPixelShader9>       g_pPixelShader;  // Pixel Shader

D3DXMATRIX                                  g_matWorld;
D3DXMATRIX                                  g_matProj;
D3DXMATRIX                                  g_matView;

D3DTexture                                  g_Texture;

TimeInfo                                    g_Time;

BOOL                                        g_bWidescreen = TRUE;

BYTE*                                       g_pTextureData;

//-------------------------------------------------------------------------------------
// Name: InitTime()
// Desc: Initializes the timer variables
//-------------------------------------------------------------------------------------
void InitTime()
{

    // Get the frequency of the timer
    LARGE_INTEGER qwTicksPerSec;
    QueryPerformanceFrequency( &qwTicksPerSec );
    g_Time.fSecsPerTick = 1.0f / ( float )qwTicksPerSec.QuadPart;

    // Save the start time
    QueryPerformanceCounter( &g_Time.qwTime );

    // Zero out the elapsed and total time
    g_Time.qwAppTime.QuadPart = 0;
    g_Time.fAppTime = 0.0f;
    g_Time.fElapsedTime = 0.0f;
}


//-------------------------------------------------------------------------------------
// Name: InitD3D()
// Desc: Initializes Direct3D
//-------------------------------------------------------------------------------------
HRESULT InitD3D()
{
    // Create the D3D object.
    CInterfacePtr <IDirect3D9> pD3D;
    pD3D = Direct3DCreate9( D3D_SDK_VERSION );
    if( !pD3D )
        return E_FAIL;

    // Set up the structure used to create the D3DDevice.
    D3DPRESENT_PARAMETERS d3dpp;
    ZeroMemory( &d3dpp, sizeof( d3dpp ) );
    XVIDEO_MODE VideoMode;
    XGetVideoMode( &VideoMode );
    g_bWidescreen = VideoMode.fIsWideScreen;
    d3dpp.BackBufferWidth = 1280;
    d3dpp.BackBufferHeight = 720;
    d3dpp.BackBufferFormat  = ( D3DFORMAT )MAKESRGBFMT( D3DFMT_X8R8G8B8 );
    d3dpp.FrontBufferFormat = ( D3DFORMAT )MAKESRGBFMT( D3DFMT_LE_X8R8G8B8 );
    d3dpp.BackBufferCount = 1;
    d3dpp.EnableAutoDepthStencil = TRUE;
    d3dpp.AutoDepthStencilFormat = D3DFMT_D24S8;
    d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_ONE;

    // Create the Direct3D device.
    if( FAILED( pD3D->CreateDevice( 0, D3DDEVTYPE_HAL, NULL,
                                    D3DCREATE_HARDWARE_VERTEXPROCESSING,
                                    &d3dpp, &g_pd3dDevice ) ) )
        return E_FAIL;

    return S_OK;
}


//-------------------------------------------------------------------------------------
// Name: InitScene()
// Desc: Creates the scene.  First we compile our shaders. For the final version
//       of a game, you should store the shaders in binary form; don't call 
//       D3DXCompileShader at runtime. Next, we declare the format of our 
//       vertices, and then create a vertex buffer. The vertex buffer is basically
//       just a chunk of memory that holds vertices. After creating it, we must 
//       Lock()/Unlock() it to fill it. Finally, we set up our world, projection,
//       and view matrices.
//-------------------------------------------------------------------------------------
HRESULT InitScene()
{
    // Compile vertex shader.
    CInterfacePtr <ID3DXBuffer> pVertexShaderCode;
    CInterfacePtr <ID3DXBuffer> pVertexErrorMsg;
    HRESULT hr = D3DXCompileShader( g_strVertexShaderProgram,
                                    ( UINT )strlen( g_strVertexShaderProgram ),
                                    NULL,
                                    NULL,
                                    "main",
                                    "vs_2_0",
                                    0,
                                    &pVertexShaderCode,
                                    &pVertexErrorMsg,
                                    NULL );
    if( FAILED( hr ) )
    {
        if( pVertexErrorMsg )
            OutputDebugString( ( char* )pVertexErrorMsg->GetBufferPointer() );
        return E_FAIL;
    }

    // Create vertex shader.
    g_pd3dDevice->CreateVertexShader( ( DWORD* )pVertexShaderCode->GetBufferPointer(),
                                      &g_pVertexShader );

    // Compile pixel shader.
    CInterfacePtr <ID3DXBuffer> pPixelShaderCode;
    CInterfacePtr <ID3DXBuffer> pPixelErrorMsg;
    hr = D3DXCompileShader( g_strPixelShaderProgram,
                            ( UINT )strlen( g_strPixelShaderProgram ),
                            NULL,
                            NULL,
                            "main",
                            "ps_2_0",
                            0,
                            &pPixelShaderCode,
                            &pPixelErrorMsg,
                            NULL );
    if( FAILED( hr ) )
    {
        if( pPixelErrorMsg )
            OutputDebugString( ( char* )pPixelErrorMsg->GetBufferPointer() );
        return E_FAIL;
    }

    // Create pixel shader.
    g_pd3dDevice->CreatePixelShader( ( DWORD* )pPixelShaderCode->GetBufferPointer(),
                                     &g_pPixelShader );

    // Define the vertex elements and
    // Create a vertex declaration from the element descriptions.
    static const D3DVERTEXELEMENT9 VertexElements[3] =
    {
        { 0,  0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
        { 0, 12, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
        D3DDECL_END()
    };
    g_pd3dDevice->CreateVertexDeclaration( VertexElements, &g_pVertexDecl );

    // Create the vertex buffer. Here we are allocating enough memory
    // (from the default pool) to hold all our 3 custom vertices. 
    if( FAILED( g_pd3dDevice->CreateVertexBuffer( 3 * sizeof( COLORVERTEX ),
                                                  D3DUSAGE_WRITEONLY,
                                                  NULL,
                                                  D3DPOOL_MANAGED,
                                                  &g_pVB,
                                                  NULL ) ) )
        return E_FAIL;

    // Now we fill the vertex buffer. To do this, we need to Lock() the VB to
    // gain access to the vertices. This mechanism is required because the
    // vertex buffer may still be in use by the GPU. This can happen if the
    // CPU gets ahead of the GPU. The GPU could still be rendering the previous
    // frame.
    COLORVERTEX g_Vertices[] =
    {
        {  0.0f, -1.1547f, 0.0f, 0.5f, 0 }, // x, y, z, color
        { -1.0f,  0.5777f, 0.0f, 0, 1 },
        {  1.0f,  0.5777f, 0.0f, 1, 1 },
    };

    COLORVERTEX* pVertices;
    if( FAILED( g_pVB->Lock( 0, 0, ( void** )&pVertices, 0 ) ) )
        return E_FAIL;
    memcpy( pVertices, g_Vertices, 3 * sizeof( COLORVERTEX ) );
    g_pVB->Unlock();

    // Initialize the world matrix
    D3DXMatrixIdentity( &g_matWorld );

    // Initialize the projection matrix
    FLOAT fAspect = ( g_bWidescreen ) ? ( 16.0f / 9.0f ) : ( 4.0f / 3.0f );
    D3DXMatrixPerspectiveFovLH( &g_matProj, D3DX_PI / 4, fAspect, 1.0f, 200.0f );

    // Initialize the view matrix
    D3DXVECTOR3 vEyePt = D3DXVECTOR3( 0.0f, 0.0f, -7.0f );
    D3DXVECTOR3 vLookatPt = D3DXVECTOR3( 0.0f, 0.0f, 0.0f );
    D3DXVECTOR3 vUp = D3DXVECTOR3( 0.0f, 1.0f, 0.0f );
    D3DXMatrixLookAtLH( &g_matView, &vEyePt, &vLookatPt, &vUp );

    // Now load the texture.
    HANDLE hFile = CreateFile( "game:\\stonewall.tex",
                               GENERIC_READ,
                               FILE_SHARE_READ,
                               NULL,
                               OPEN_EXISTING,
                               FILE_ATTRIBUTE_NORMAL,
                               NULL );
    if( INVALID_HANDLE_VALUE == hFile )
    {
        OutputDebugString( "Failed to load texture file!" );
        return E_FAIL;
    }

    // Get the file size so we know how much space to allocate:
    DWORD dwSize = GetFileSize( hFile, NULL );
    if( !dwSize )
    {
        ::CloseHandle( hFile );
        return E_UNEXPECTED;
    }

    DWORD dwRead = 0;
    DWORD dwTextureDataSize = dwSize - sizeof( D3DTexture );

    // First load the D3DTexture structure contents so we have a valid texture header. 
    if( !ReadFile( hFile, &g_Texture, sizeof( g_Texture ), &dwRead, NULL ) || dwRead != sizeof( D3DTexture ) )
    {
        ::CloseHandle( hFile );
        return E_FAIL;
    }

    // Now allocate space for the texture data. This needs to be allocated on 4K boundary in physical memory. 
    DWORD dwAllocAttributes = MAKE_XALLOC_ATTRIBUTES( 0, FALSE, FALSE, FALSE, 0, XALLOC_PHYSICAL_ALIGNMENT_4K,
                                                      XALLOC_MEMPROTECT_WRITECOMBINE, FALSE, XALLOC_MEMTYPE_PHYSICAL );
    g_pTextureData = ( BYTE* )XMemAlloc( dwTextureDataSize, dwAllocAttributes );
    if( NULL == g_pTextureData )
    {
        ::CloseHandle( hFile );
        return E_OUTOFMEMORY;
    }

    // Load the texture data into our allocated space.
    if( !ReadFile( hFile, g_pTextureData, dwTextureDataSize, &dwRead, NULL ) || dwRead != dwTextureDataSize )
    {
        ::CloseHandle( hFile );
        return E_FAIL;
    }

    ::CloseHandle( hFile );

    // Now fix up the texture header to point to the base and mip addresses inside the texture data.
    // This is the final step - once this is done, the texture is ready to be used!
    D3DBaseTexture* pBaseTexture = ( D3DBaseTexture* )&g_Texture;
    XGOffsetBaseTextureAddress( pBaseTexture, g_pTextureData, g_pTextureData );

    return S_OK;
}


//-------------------------------------------------------------------------------------
// Name: UpdateTime()
// Desc: Updates the elapsed time since our last frame.
//-------------------------------------------------------------------------------------
void UpdateTime()
{
    LARGE_INTEGER qwNewTime;
    LARGE_INTEGER qwDeltaTime;

    QueryPerformanceCounter( &qwNewTime );
    qwDeltaTime.QuadPart = qwNewTime.QuadPart - g_Time.qwTime.QuadPart;

    g_Time.qwAppTime.QuadPart += qwDeltaTime.QuadPart;
    g_Time.qwTime.QuadPart = qwNewTime.QuadPart;

    g_Time.fElapsedTime = g_Time.fSecsPerTick * ( ( FLOAT )( qwDeltaTime.QuadPart ) );
    g_Time.fAppTime = g_Time.fSecsPerTick * ( ( FLOAT )( g_Time.qwAppTime.QuadPart ) );
}


//-------------------------------------------------------------------------------------
// Name: Update()
// Desc: Updates the world for the next frame
//-------------------------------------------------------------------------------------
void Update()
{
    // Set the world matrix
    float fAngle = fmodf( -g_Time.fAppTime, 2.0f * D3DX_PI );
    D3DXMatrixRotationZ( &g_matWorld, fAngle );
}


//-------------------------------------------------------------------------------------
// Name: Render()
// Desc: Draws the scene
//-------------------------------------------------------------------------------------
void Render()
{
    // Clear the backbuffer to a blue color
    g_pd3dDevice->Clear( 0L, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL,
                         D3DCOLOR_XRGB( 0, 0, 255 ), 1.0f, 0L );

    // Draw the triangles in the vertex buffer. This is broken into a few steps:

    // We are passing the vertices down a "stream", so first we need
    // to specify the source of that stream, which is our vertex buffer. 
    // Then we need to let D3D know what vertex and pixel shaders to use. 
    g_pd3dDevice->SetVertexDeclaration( g_pVertexDecl );
    g_pd3dDevice->SetStreamSource( 0, g_pVB, 0, sizeof( COLORVERTEX ) );
    g_pd3dDevice->SetVertexShader( g_pVertexShader );
    g_pd3dDevice->SetPixelShader( g_pPixelShader );

    // Build the world-view-projection matrix and pass it into the vertex shader
    D3DXMATRIX matWVP = g_matWorld * g_matView * g_matProj;
    g_pd3dDevice->SetVertexShaderConstantF( 0, ( FLOAT* )&matWVP, 4 );

    g_pd3dDevice->SetTexture( 0, &g_Texture );

    g_pd3dDevice->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_ANISOTROPIC );
    g_pd3dDevice->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_ANISOTROPIC );
    g_pd3dDevice->SetSamplerState( 0, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR );

    // Draw the vertices in the vertex buffer
    g_pd3dDevice->DrawPrimitive( D3DPT_TRIANGLELIST, 0, 1 );

    // Present the backbuffer contents to the display
    g_pd3dDevice->Present( NULL, NULL, NULL, NULL );
}


//-------------------------------------------------------------------------------------
// Name: main()
// Desc: The application's entry point
//-------------------------------------------------------------------------------------
int __cdecl main()
{
    // Initialize Direct3D
    if( FAILED( InitD3D() ) )
        return 0;

    // Initialize the vertex buffer
    if( FAILED( InitScene() ) )
        return 0;

    InitTime();

    for(; ; ) // loop forever
    {
        // What time is it?
        UpdateTime();
        // Update the world
        Update();
        // Render the scene
        Render();
    }
}


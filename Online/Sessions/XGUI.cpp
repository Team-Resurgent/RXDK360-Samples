//--------------------------------------------------------------------------------------
// File: XGUI.cpp
//
// Desc: Xbox 360 GUI primitive drawing functions.
//
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include "stdafx.h"
#include "XGUI.h"

//--------------------------------------------------------------------------------------
// Name: const g_strShaders
// Desc: The HSHL source code for the gradient and MuSigma shader.
//--------------------------------------------------------------------------------------
static const CHAR*                                  g_strShaders =
    "struct VS_IN                                                  \n"
    "{                                                             \n"
    "   float2   Pos          : POSITION;                          \n"
    "   float2   Tex          : TEXCOORD0;                         \n"
    "};                                                            \n"
    "                                                              \n"
    "struct VS_OUT                                                 \n"
    "{                                                             \n"
    "   float4 Position       : POSITION;                          \n"
    "   float4 Diffuse        : COLOR0;                            \n"
    "   float2 TexCoord0      : TEXCOORD0;                         \n"
    "};                                                            \n"
    "                                                              \n"
    "uniform float4   Color    : register(c1);                     \n"
    "                                                              \n"
    "VS_OUT XVertexShader( VS_IN In )                              \n"
    "{                                                             \n"
    "   VS_OUT Out;                                                \n"
    "   Out.Position.x  = ( In.Pos.x - 0.5 );                      \n"
    "   Out.Position.y  = ( In.Pos.y - 0.5 );                      \n"
    "   Out.Position.z  = ( 0.0 );                                 \n"
    "   Out.Position.w  = ( 1.0 );                                 \n"
    "   Out.Diffuse     = Color;                                   \n"
    "   Out.TexCoord0.x = In.Tex.x;                                \n"
    "   Out.TexCoord0.y = In.Tex.y;                                \n"
    "   return Out;                                                \n"
    "}                                                             \n"
    "                                                              \n"
    "uniform float   Mixture    : register(c2);                    \n"
    "                                                              \n"
    " float4 XGradientPixelShader( VS_OUT In ) : COLOR0            \n"
    " {                                                            \n"
    "     return ( (1.0 - In.TexCoord0.y)*Mixture +                \n"
    "              (1.0 - Mixture) ) * In.Diffuse;                 \n"
    " }                                                            \n"
    "                                                              \n"
    " float N( float x, float fMu, float fSigma )                  \n"
    "{                                                             \n"
    "    return                                                    \n"
    "        exp( -(fMu - x) * (fMu - x) /                         \n"
    "                    (2.0 * fSigma * fSigma) );                \n"
    "}                                                             \n"
    "                                                              \n"
    "uniform float4  MuSigmaMixture : register(c3);                \n"
    "uniform float4  MuSigmaColour1 : register(c4);                \n"
    "uniform float4  MuSigmaColour2 : register(c5);                \n"
    "                                                              \n"
    " float4 XMuSigmaPixelShader( VS_OUT In ) : COLOR0             \n"
    " {                                                            \n"
    "       float fProb = pow( N( In.TexCoord0.x * 6.0,            \n"
    "            MuSigmaMixture.x, MuSigmaMixture.y ), 1.0 / 5.0 );\n"
    "       if( In.TexCoord0.x*6.0<MuSigmaMixture.x )              \n"
    "        return ( 1.0 - In.TexCoord0.y)*MuSigmaMixture.z +     \n"
    "                (1.0 - MuSigmaMixture.z) * (fProb *           \n"
    "                In.Diffuse + (1.0 - fProb) * MuSigmaColour1 );\n"
    "      else                                                    \n"
    "        return ( 1.0 - In.TexCoord0.y) * MuSigmaMixture.z +   \n"
    "                (1.0 - MuSigmaMixture.z) * (fProb *           \n"
    "                In.Diffuse + (1.0 - fProb) * MuSigmaColour2 );\n"
    " }                                                            \n";

static const D3DVERTEXELEMENT9 g_VertexElements[] =
{
    { 0, 0, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
    { 0, 8, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
    D3DDECL_END()
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
            CInterfacePtr( const CInterfacePtr& p );                // unimplemented copy
    CInterfacePtr& operator =( const CInterfacePtr& p );   // unimplemented assign

    T* m_ptr;
};

//--------------------------------------------------------------------------------------
// Name: struct Vertex2D
// Desc: A 2D point with texture coordinates
//--------------------------------------------------------------------------------------
struct Vertex2D
{
    FLOAT x, y;
    FLOAT u, v;
};

//--------------------------------------------------------------------------------------
// Global variables & pointer to the global D3D device
//--------------------------------------------------------------------------------------
static CInterfacePtr <IDirect3DVertexDeclaration9>  g_pVertexDecl;           // Vertex format decl
static CInterfacePtr <IDirect3DVertexShader9>       g_pVertexShader;         // Vertex Shader
static CInterfacePtr <IDirect3DPixelShader9>        g_pGradientPixelShader;  // Gradient Pixel Shader
static CInterfacePtr <IDirect3DPixelShader9>        g_pMuSigmaPixelShader;   // MuSigma Pixel Shader

//--------------------------------------------------------------------------------------
// Name: Initialise()
// Desc: Initialises the D3D interface. Primarily, it compiles and sets the 
//       vertex and pixel shaders necessary for drawing the boxes and lines.
//--------------------------------------------------------------------------------------
bool XGUI::Initialise( LPDIRECT3DDEVICE9 pd3dDevice )
{
    CInterfacePtr <ID3DXBuffer> pVertexShaderCode;
    CInterfacePtr <ID3DXBuffer> pVertexErrorMsg;
    if( FAILED( D3DXCompileShader( g_strShaders,
                                   ( UINT )strlen( g_strShaders ),
                                   NULL,
                                   NULL,
                                   "XVertexShader",
                                   "vs_2_0",
                                   0,
                                   &pVertexShaderCode,
                                   &pVertexErrorMsg,
                                   NULL ) ) )
        return false;

    // Create vertex shader.
    pd3dDevice->CreateVertexShader( ( DWORD* )pVertexShaderCode->GetBufferPointer(),
                                    &g_pVertexShader );
    pd3dDevice->CreateVertexDeclaration( g_VertexElements, &g_pVertexDecl );

    // Compile gradient pixel shader.
    CInterfacePtr <ID3DXBuffer> pGradientPixelShaderCode;
    CInterfacePtr <ID3DXBuffer> pGradientPixelErrorMsg;
    if( FAILED( D3DXCompileShader( g_strShaders,
                                   ( UINT )strlen( g_strShaders ),
                                   NULL,
                                   NULL,
                                   "XGradientPixelShader",
                                   "ps_3_0",
                                   0,
                                   &pGradientPixelShaderCode,
                                   &pGradientPixelErrorMsg,
                                   NULL ) ) )
        return false;

    // Create gradient pixel shader.
    pd3dDevice->CreatePixelShader( ( DWORD* )pGradientPixelShaderCode->GetBufferPointer(),
                                   &g_pGradientPixelShader );

    // Compile MuSigma pixel shader.
    CInterfacePtr <ID3DXBuffer> pMuSigmaPixelShaderCode;
    CInterfacePtr <ID3DXBuffer> pMuSigmaPixelErrorMsg;
    if( FAILED( D3DXCompileShader( g_strShaders,
                                   ( UINT )strlen( g_strShaders ),
                                   NULL,
                                   NULL,
                                   "XMuSigmaPixelShader",
                                   "ps_3_0",
                                   0,
                                   &pMuSigmaPixelShaderCode,
                                   &pMuSigmaPixelErrorMsg,
                                   NULL ) ) )
        return false;

    // Create MuSigma pixel shader.
    pd3dDevice->CreatePixelShader( ( DWORD* )pMuSigmaPixelShaderCode->GetBufferPointer(),
                                   &g_pMuSigmaPixelShader );

    return( true );
}

//--------------------------------------------------------------------------------------
// Name: DrawGradientPrimitives()
// Desc: Draws a number of D3D primites using the gradient pixel shader.
//--------------------------------------------------------------------------------------
static void DrawGradientPrimitives( LPDIRECT3DDEVICE9 pd3dDevice, Vertex2D aVertices[],
                                    D3DPRIMITIVETYPE ePrimType, int iCount,
                                    DWORD dwColour, FLOAT fMixture )
{
    // set the shader constants
    FLOAT afColor[ 4 ];
    afColor[ 0 ] = ( ( dwColour & 0x00ff0000 ) >> 16L ) / 255.0f;
    afColor[ 1 ] = ( ( dwColour & 0x0000ff00 ) >> 8L ) / 255.0f;
    afColor[ 2 ] = ( ( dwColour & 0x000000ff ) >> 0L ) / 255.0f;
    afColor[ 3 ] = ( ( dwColour & 0xff000000 ) >> 24L ) / 255.0f;
    FLOAT fMix = fMixture;

    pd3dDevice->SetRenderState( D3DRS_ZENABLE, FALSE );
    pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );
    pd3dDevice->SetRenderState( D3DRS_ALPHATESTENABLE, FALSE );
    pd3dDevice->SetRenderState( D3DRS_FILLMODE, D3DFILL_SOLID );
    pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_CCW );

    pd3dDevice->SetVertexShader( g_pVertexShader );
    pd3dDevice->SetPixelShader( g_pGradientPixelShader );
    pd3dDevice->SetVertexDeclaration( g_pVertexDecl );

    pd3dDevice->SetSamplerState( 0, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR );
    pd3dDevice->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
    pd3dDevice->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );

    pd3dDevice->SetVertexShaderConstantF( 1, afColor, 1 );
    pd3dDevice->SetPixelShaderConstantF( 2, &fMix, 1 );
    pd3dDevice->SetRenderState( D3DRS_VIEWPORTENABLE, FALSE );
    pd3dDevice->DrawPrimitiveUP( ePrimType, iCount, aVertices, sizeof( aVertices[ 0 ] ) );
    pd3dDevice->SetRenderState( D3DRS_VIEWPORTENABLE, TRUE );

    return;
}

//--------------------------------------------------------------------------------------
// Name: DrawMuSigmaPrimitives()
// Desc: Draws a number of D3D primites based on the MuSigma pixel shader.
//--------------------------------------------------------------------------------------
static void DrawMuSigmaPrimitives( LPDIRECT3DDEVICE9 pd3dDevice, Vertex2D aVertices[],
                                   D3DPRIMITIVETYPE ePrimType, int iCount,
                                   DWORD dwColour1, DWORD dwColour2, DWORD dwColour3,
                                   FLOAT fMu, FLOAT fSigma, FLOAT fMixture )
{
    // set the shader constants
    FLOAT afColor1[ 4 ];
    afColor1[ 0 ] = ( ( dwColour1 & 0x00ff0000 ) >> 16L ) / 255.0f;
    afColor1[ 1 ] = ( ( dwColour1 & 0x0000ff00 ) >> 8L ) / 255.0f;
    afColor1[ 2 ] = ( ( dwColour1 & 0x000000ff ) >> 0L ) / 255.0f;
    afColor1[ 3 ] = ( ( dwColour1 & 0xff000000 ) >> 24L ) / 255.0f;

    FLOAT afColor2[ 4 ];
    afColor2[ 0 ] = ( ( dwColour2 & 0x00ff0000 ) >> 16L ) / 255.0f;
    afColor2[ 1 ] = ( ( dwColour2 & 0x0000ff00 ) >> 8L ) / 255.0f;
    afColor2[ 2 ] = ( ( dwColour2 & 0x000000ff ) >> 0L ) / 255.0f;
    afColor2[ 3 ] = ( ( dwColour2 & 0xff000000 ) >> 24L ) / 255.0f;

    FLOAT afColor3[ 4 ];
    afColor3[ 0 ] = ( ( dwColour3 & 0x00ff0000 ) >> 16L ) / 255.0f;
    afColor3[ 1 ] = ( ( dwColour3 & 0x0000ff00 ) >> 8L ) / 255.0f;
    afColor3[ 2 ] = ( ( dwColour3 & 0x000000ff ) >> 0L ) / 255.0f;
    afColor3[ 3 ] = ( ( dwColour3 & 0xff000000 ) >> 24L ) / 255.0f;

    FLOAT afMuSigma[ 4 ];
    afMuSigma[ 0 ] = fMu;
    afMuSigma[ 1 ] = fSigma;
    afMuSigma[ 2 ] = fMixture;

    pd3dDevice->SetRenderState( D3DRS_ZENABLE, FALSE );
    pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );
    pd3dDevice->SetRenderState( D3DRS_ALPHATESTENABLE, FALSE );
    pd3dDevice->SetRenderState( D3DRS_FILLMODE, D3DFILL_SOLID );
    pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_CCW );

    pd3dDevice->SetVertexShader( g_pVertexShader );
    pd3dDevice->SetPixelShader( g_pMuSigmaPixelShader );
    pd3dDevice->SetVertexDeclaration( g_pVertexDecl );

    pd3dDevice->SetSamplerState( 0, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR );
    pd3dDevice->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
    pd3dDevice->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );

    pd3dDevice->SetVertexShaderConstantF( 1, afColor1, 1 );
    pd3dDevice->SetPixelShaderConstantF( 3, afMuSigma, 1 );
    pd3dDevice->SetPixelShaderConstantF( 4, afColor2, 1 );
    pd3dDevice->SetPixelShaderConstantF( 5, afColor3, 1 );
    pd3dDevice->SetRenderState( D3DRS_VIEWPORTENABLE, FALSE );
    pd3dDevice->DrawPrimitiveUP( ePrimType, iCount, aVertices, sizeof( aVertices[ 0 ] ) );
    pd3dDevice->SetRenderState( D3DRS_VIEWPORTENABLE, TRUE );

    return;
}

//--------------------------------------------------------------------------------------
// Name: SolidRectangle()
// Desc: Draws a rectangle filled with a gradient texture
//--------------------------------------------------------------------------------------
void XGUI::SolidRectangle( LPDIRECT3DDEVICE9 pd3dDevice, FLOAT fX, FLOAT fY,
                           FLOAT fWidth, FLOAT fHeight, DWORD dwColour, FLOAT fGradient )
{
    Vertex2D aVertices[ 4 ];

    aVertices[ 0 ].x = fX;
    aVertices[ 0 ].y = fY;
    aVertices[ 0 ].u = 0.0f;
    aVertices[ 0 ].v = 0.0f;

    aVertices[ 1 ].x = fX + fWidth;
    aVertices[ 1 ].y = fY;
    aVertices[ 1 ].u = 1.0f;
    aVertices[ 1 ].v = 0.0f;

    aVertices[ 2 ].x = fX + fWidth;
    aVertices[ 2 ].y = fY + fHeight;
    aVertices[ 2 ].u = 1.0f;
    aVertices[ 2 ].v = 1.0f;

    aVertices[ 3 ].x = fX;
    aVertices[ 3 ].y = fY + fHeight;
    aVertices[ 3 ].u = 0.0f;
    aVertices[ 3 ].v = 1.0f;

    DrawGradientPrimitives( pd3dDevice, aVertices, D3DPT_QUADLIST, 1, dwColour,
                            fGradient );

    return;
}

//--------------------------------------------------------------------------------------
// Name: MuSigmaRectangle()
// Desc: Draws a rectangle filled with a MuSigma texture
//--------------------------------------------------------------------------------------
void XGUI::MuSigmaRectangle( LPDIRECT3DDEVICE9 pd3dDevice, FLOAT fX, FLOAT fY,
                             FLOAT fWidth, FLOAT fHeight, DWORD dwColour1,
                             DWORD dwColour2, DWORD dwColour3, FLOAT fMu, FLOAT fSigma,
                             FLOAT fGradient )
{
    Vertex2D aVertices[ 4 ];

    aVertices[ 0 ].x = fX;
    aVertices[ 0 ].y = fY;
    aVertices[ 0 ].u = 0.0f;
    aVertices[ 0 ].v = 0.0f;

    aVertices[ 1 ].x = fX + fWidth;
    aVertices[ 1 ].y = fY;
    aVertices[ 1 ].u = 1.0f;
    aVertices[ 1 ].v = 0.0f;

    aVertices[ 2 ].x = fX + fWidth;
    aVertices[ 2 ].y = fY + fHeight;
    aVertices[ 2 ].u = 1.0f;
    aVertices[ 2 ].v = 1.0f;

    aVertices[ 3 ].x = fX;
    aVertices[ 3 ].y = fY + fHeight;
    aVertices[ 3 ].u = 0.0f;
    aVertices[ 3 ].v = 1.0f;

    DrawMuSigmaPrimitives( pd3dDevice, aVertices, D3DPT_QUADLIST, 1, dwColour1,
                           dwColour2, dwColour3, fMu, fSigma, fGradient );

    return;
}

//--------------------------------------------------------------------------------------
// Name: OutlinedRectangle()
// Desc: Draws a rectangular outline
//--------------------------------------------------------------------------------------
void XGUI::OutlinedRectangle( LPDIRECT3DDEVICE9 pd3dDevice, FLOAT fX, FLOAT fY,
                              FLOAT fWidth, FLOAT fHeight, DWORD dwColour )
{
    Vertex2D aVertices[ 5 ];

    aVertices[ 0 ].x = fX;
    aVertices[ 0 ].y = fY;
    aVertices[ 0 ].u = 0.0f;
    aVertices[ 0 ].v = 0.0f;

    aVertices[ 1 ].x = fX + fWidth;
    aVertices[ 1 ].y = fY;
    aVertices[ 1 ].u = 1.0f;
    aVertices[ 1 ].v = 0.0f;

    aVertices[ 2 ].x = fX + fWidth;
    aVertices[ 2 ].y = fY + fHeight;
    aVertices[ 2 ].u = 1.0f;
    aVertices[ 2 ].v = 1.0f;

    aVertices[ 3 ].x = fX;
    aVertices[ 3 ].y = fY + fHeight;
    aVertices[ 3 ].u = 0.0f;
    aVertices[ 3 ].v = 1.0f;

    aVertices[ 4 ].x = fX;
    aVertices[ 4 ].y = fY;
    aVertices[ 4 ].u = 0.0f;
    aVertices[ 4 ].v = 0.0f;

    DrawGradientPrimitives( pd3dDevice, aVertices, D3DPT_LINESTRIP, 4, dwColour, 0.0f );

    return;
}

//--------------------------------------------------------------------------------------
// Name: DrawLine()
// Desc: Draws a single line from (x1,y1) to (x2,y2)
//--------------------------------------------------------------------------------------
void XGUI::DrawLine( LPDIRECT3DDEVICE9 pd3dDevice, FLOAT fX1, FLOAT fY1,
                     FLOAT fX2, FLOAT fY2, DWORD dwColour )
{
    Vertex2D aVertices[ 2 ];

    aVertices[ 0 ].x = fX1;
    aVertices[ 0 ].y = fY1;
    aVertices[ 0 ].u = 0.0f;
    aVertices[ 0 ].v = 0.0f;

    aVertices[ 1 ].x = fX2;
    aVertices[ 1 ].y = fY2;
    aVertices[ 1 ].u = 1.0f;
    aVertices[ 1 ].v = 0.0f;

    DrawGradientPrimitives( pd3dDevice, aVertices, D3DPT_LINELIST, 1, dwColour, 0.0f );

    return;
}

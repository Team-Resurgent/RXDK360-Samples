//--------------------------------------------------------------------------------------
// TextureCompression.cpp
//
// Show how to use the texture compression / decompression API's.
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include <xtl.h>
#include <xbdm.h>
#include <xboxmath.h>
#include <XGraphics.h>

#include <assert.h>
#include <AtgFont.h>
#include <AtgHelp.h>
#include <AtgInput.h>
#include <AtgUtil.h>
#include <AtgApp.h>

//--------------------------------------------------------------------------------------
// Callouts for labelling the gamepad on the help screen
//--------------------------------------------------------------------------------------
ATG::HELP_CALLOUT g_HelpCallouts[] =
{
    { ATG::HELP_A_BUTTON,       ATG::HELP_PLACEMENT_2, L"Toggle\ncomparison" },
    { ATG::HELP_X_BUTTON,       ATG::HELP_PLACEMENT_1, L"Alpha" },
    { ATG::HELP_Y_BUTTON,       ATG::HELP_PLACEMENT_2, L"Save Compressed\nImage" },
    { ATG::HELP_LEFT_SHOULDER,  ATG::HELP_PLACEMENT_2, L"Cycle\ncompression" },
    { ATG::HELP_RIGHT_SHOULDER, ATG::HELP_PLACEMENT_2, L"Cycle\ncompression" },
    { ATG::HELP_LEFT_TRIGGER,   ATG::HELP_PLACEMENT_2, L"Cycle\ntextures" },
    { ATG::HELP_RIGHT_TRIGGER,  ATG::HELP_PLACEMENT_2, L"Cycle\ntextures" },
    { ATG::HELP_BACK_BUTTON,    ATG::HELP_PLACEMENT_2, L"Display\nhelp" },
};
#define NUM_HELP_CALLOUTS (sizeof(g_HelpCallouts) / sizeof(g_HelpCallouts[0]))


//--------------------------------------------------------------------------------------
// Vertex shader
// We use the register semantic here to directly define the input register
// matWVP.  Conversely, we could let the HLSL compiler decide and check the
// constant table.
//--------------------------------------------------------------------------------------
const char*                     g_strVertexShaderProgram =
    " float4x4 matWVP : register(c0);              "
    "                                              "
    " struct VS_IN                                 "
    " {                                            "
    "     float4 ObjPos   : POSITION;              "  // Object space position
    "     float2 TexCoord : TEXCOORD0;             "  // Texture coordinate
    " };                                           "
    "                                              "
    " struct VS_OUT                                "
    " {                                            "
    "     float4 ProjPos  : POSITION;              "  // Projected space position
    "     float2 TexCoord : TEXCOORD0;             "
    " };                                           "
    "                                              "
    " VS_OUT main( VS_IN In )                      "
    " {                                            "
    "     VS_OUT Out;                              "
    "     Out.ProjPos = mul( matWVP, In.ObjPos );  "  // Transform vertex into
    "     Out.TexCoord = In.TexCoord;              "  // Projected space and
    "     return Out;                              "  // Transfer textrue coordinate
    " }                                            ";


//--------------------------------------------------------------------------------------
// Pixel shader
//--------------------------------------------------------------------------------------
const char*                     g_strPixelShaderProgram =
    " struct PS_IN                                           "
    " {                                                      "
    "     float2 TexCoord : TEXCOORD0;                       "
    " };                                                     "
    "                                                        "
    " sampler2D PtcImage : register(s0);                     "
    " sampler2D SrcImage : register(s1);                     "
    " float4 DoCompare : register(c0);                       "
    "                                                        "
    " float4 main( PS_IN In ) : COLOR                        "
    " {                                                      "
    "    float4 Color;                                       "
    "    if(DoCompare.x != 0)                                "
    "    {                                                   "
    "       float4 Difference = tex2D(PtcImage, In.TexCoord) "
    "                         - tex2D(SrcImage, In.TexCoord);"
    "       Color = .5 + (Difference * DoCompare.x);         "
    "    }                                                   "
    "    else                                                "
    "    {                                                   "
    "       Color = tex2D(SrcImage, In.TexCoord);            "
    "    }                                                   "
    "    return (DoCompare.z) ? Color.w : Color.xyzw;        "
    " }                                                      ";


//--------------------------------------------------------------------------------------
// Structure to hold vertex data.
//--------------------------------------------------------------------------------------
struct COLORVERTEX
{
    float   Position[3];
    float   TexCoord[2];
};


//--------------------------------------------------------------------------------------
// TimeInfo         We need to keep the quadword app time as a LARGE_INTEGER so that we
//                  don't lose precision after running for a long time.
//--------------------------------------------------------------------------------------
struct TimeInfo
{
    LARGE_INTEGER qwTime;
    LARGE_INTEGER qwAppTime;

    float fElapsedTime;

    float fSecsPerTick;
};


//--------------------------------------------------------------------------------------
// CInterfacePtr<T> Helper class to make sure we always release our interfaces.
//                  This is a simple resource management class- there are subtleties
//                  when doing assignment or copy of smart pointers, so we make sure
//                  they aren't called by making copy and assign private.
//--------------------------------------------------------------------------------------
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


//--------------------------------------------------------------------------------------
// Global variables
//--------------------------------------------------------------------------------------
IDirect3DVertexBuffer9*         g_pVB;             // Buffer to hold vertices
IDirect3DVertexDeclaration9*    g_pVertexDecl;     // Vertex format decl
IDirect3DVertexShader9*         g_pVertexShader;   // Vertex Shader
IDirect3DPixelShader9*          g_pPixelShader;    // Pixel Shader
ATG::Font                       g_Font;            // Font for drawing text
TimeInfo                        g_Time;            // Global time
ATG::Help                       g_Help;            // Sample help object

float                           g_fTimeSinceUpdate = 0.0f;    // Time elapsed since the file times were checked
float                           g_fTriggerTimer = 0.0f;  
float                           g_fDecompressionTime = 0.0f;
float                           g_fRecompressionTime = 0.0f;
float                           g_fMemDecompressTime = 0.0f;
UINT                            g_iTexWidth = 0;
UINT                            g_iTexHeight = 0;
UINT                            g_iUncompressedSize = 0;
UINT                            g_iCompressedSize = 0;
float                           g_fCompressionRatio = 1.0f;
float                           g_fSaveDisplayTime = 0.0f;    // Time to diplay the save message.
bool                            g_bLoadSuccessful = false;
bool                            g_bSaveSuccessful = false;

XMMATRIX                        g_matWorld;
XMMATRIX                        g_matView;
XMMATRIX                        g_matProj;

enum CompressionModes
{
    MODE_ZTC, 
    MODE_DXT, 
    MODE_ZTC_DXT,   // ZTC+LZX compressed (offline) --> uncompressed --> DXT compressed (runtime)
    MODE_MCT, 
    MODE_PTC,
    MODE_MAX
};

struct CompressionModeInfo
{
    const WCHAR* strCompressionName;
    const WCHAR* strOriginalTextureLabel;
    const WCHAR* strCompressedTextureLabel;
    const char* strCompressedExtension;
};

CompressionModeInfo g_CompressionModeInfo[] =
{
    {
        L"ZTC", 
        L"Uncompressed Texture", 
        L"ZTC&LZX Compressed Texture", 
        ".ztc", 
    },
    {
        L"***", 
        L"Uncompressed Texture", 
        L"DXT Compressed Texture", 
        ".dxt", 
    },
    {
        L"ZTC", 
        L"Uncompressed Texture", 
        L"ZTC&LZX->DXT Compressed Texture", 
        ".ztc", 
    },
    {
        L"MCT", 
        L"Uncompressed Texture",
        L"MCT Compressed Texture", 
        ".mct", 
    },
    {
        L"PTC", 
        L"Uncompressed Texture",
        L"PTC&LZX Compresssed Texture", 
        ".ptc", 
    },
};

struct BaseAndExt
{
    const char* strBase;
    const char* strExt; 
};

// Add other images here, .bmp or .dds format
// Currently all images are assumed to start as 8:8:8:8 
// For formats which use DXT compression the dest format is assumed to be DXT5
const BaseAndExt strSourceFileNames[] = 
{
    { "TextureCompression_TestImage",           ".bmp" }, 
};
#define SOURCE_FILE_COUNT (ARRAYSIZE(strSourceFileNames))

// The original texture (pointer, file time)
IDirect3DTexture9*              g_pOriginalTexture;
FILETIME                        g_OriginalUpdateTime;

// The PTC compressed texture (pointer, file time)
IDirect3DTexture9*              g_pCompressedTexture;
void*                           g_pCompressedTextureBuffer;
FILETIME                        g_CompressedUpdateTime;

// Variables controlling the sample UI.
BOOL                            g_bDrawHelp = FALSE;
FLOAT                           g_DoComparison = 0;
BOOL                            g_bShowAlpha = FALSE;
UINT                            g_CompressionMode = MODE_ZTC;
UINT                            g_SourceIndex = 0;


//--------------------------------------------------------------------------------------
// Name: InitTime()
// Desc: Initializes the timer variables
//--------------------------------------------------------------------------------------
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
    g_Time.fElapsedTime = 0.0f;
}


//--------------------------------------------------------------------------------------
// Name: UpdateTime()
// Desc: Updates the elapsed time since our last frame.
//--------------------------------------------------------------------------------------
void UpdateTime()
{
    LARGE_INTEGER qwNewTime;
    LARGE_INTEGER qwDeltaTime;

    QueryPerformanceCounter( &qwNewTime );
    qwDeltaTime.QuadPart = qwNewTime.QuadPart - g_Time.qwTime.QuadPart;

    g_Time.qwAppTime.QuadPart += qwDeltaTime.QuadPart;
    g_Time.qwTime.QuadPart = qwNewTime.QuadPart;

    g_Time.fElapsedTime = g_Time.fSecsPerTick * ( ( FLOAT )( qwDeltaTime.QuadPart ) );
}


//--------------------------------------------------------------------------------------
// Name: InitD3D()
// Desc: Initializes Direct3D
//--------------------------------------------------------------------------------------
HRESULT InitD3D()
{
    // Create the D3D object.
    CInterfacePtr <IDirect3D9> pD3D;
    pD3D = Direct3DCreate9( D3D_SDK_VERSION );
    if( !pD3D )
        return E_FAIL;

    D3DPRESENT_PARAMETERS d3dpp;
    ZeroMemory( &d3dpp, sizeof( d3dpp ) );
    d3dpp.BackBufferWidth = 1280;
    d3dpp.BackBufferHeight = 720;
    d3dpp.BackBufferFormat =  ( D3DFORMAT )MAKESRGBFMT( D3DFMT_X8R8G8B8 );
    d3dpp.FrontBufferFormat = ( D3DFORMAT )MAKESRGBFMT( D3DFMT_LE_X8R8G8B8 );
    d3dpp.BackBufferCount = 1;
    d3dpp.EnableAutoDepthStencil = TRUE;
    d3dpp.AutoDepthStencilFormat = D3DFMT_D24S8;
    d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_ONE;

    // Create the Direct3D device.
    if( FAILED( pD3D->CreateDevice( 0, D3DDEVTYPE_HAL, NULL,
                                    D3DCREATE_HARDWARE_VERTEXPROCESSING,
                                    &d3dpp, ( D3DDevice** )&ATG::g_pd3dDevice ) ) )
        return E_FAIL;

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: GetFileWriteTime()
// Desc: Get the write time of a file.
//--------------------------------------------------------------------------------------
void GetFileWriteTime( const char* strFileName, FILETIME* pFileTime )
{
    assert( strFileName );
    assert( pFileTime );

    HANDLE hFile = CreateFile( strFileName,
                               GENERIC_READ,
                               FILE_SHARE_READ,
                               NULL,
                               OPEN_EXISTING,
                               FILE_ATTRIBUTE_NORMAL,
                               NULL );

    if( hFile != INVALID_HANDLE_VALUE )
    {
        GetFileTime( hFile, NULL, NULL, pFileTime );
        CloseHandle( hFile );
    }
}


//--------------------------------------------------------------------------------------
// Name: FileChanged()
// Desc: Check if a files write time has changed and update the time if so.
//--------------------------------------------------------------------------------------
bool FileChanged( const char* strFileName, FILETIME* pFileTime )
{
    assert( strFileName );
    assert( pFileTime );

    HANDLE hFile = CreateFile( strFileName,
                               GENERIC_READ,
                               FILE_SHARE_READ,
                               NULL,
                               OPEN_EXISTING,
                               FILE_ATTRIBUTE_NORMAL,
                               NULL );

    if( hFile != INVALID_HANDLE_VALUE )
    {
        FILETIME chkFileTime;
        if( GetFileTime( hFile, NULL, NULL, &chkFileTime ) )
        {
            CloseHandle( hFile );

            if( ( chkFileTime.dwLowDateTime != pFileTime->dwLowDateTime ) ||
                ( chkFileTime.dwHighDateTime != pFileTime->dwHighDateTime ) )
            {
                *pFileTime = chkFileTime;
                return true;
            }
        }
        else
        {
            CloseHandle( hFile );
            return false;
        }
    }

    return false;
}


//--------------------------------------------------------------------------------------
// Name: LoadCompressedFile()
// Desc: Read the compressed file into memory, and allocate the appropriate buffer size
//--------------------------------------------------------------------------------------
HRESULT LoadCompressedFile( const char* strCompressedTexture, BYTE** ppBuffer, DWORD* pSize )
{
    // Load the compressed file.
    HANDLE SrcFileHandle = CreateFile( strCompressedTexture, GENERIC_READ, 0, 0,
                                       OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN,
                                       NULL );

    if( SrcFileHandle == INVALID_HANDLE_VALUE )
    {
        ATG::DebugSpew( "TextureCompression: Failed to open optional compressed file (%s). Compressing at runtime instead.\n",
                        strCompressedTexture );

        return E_FAIL;
    }

    *pSize = GetFileSize( SrcFileHandle, 0 );
    *ppBuffer = (BYTE*) malloc( *pSize );
    if( !*ppBuffer )
    {
        ATG::FatalError( "TextureCompression: Failed to allocate memory for file (%s)\n",
                        strCompressedTexture );
    }

    if( !ReadFile( SrcFileHandle, *ppBuffer, *pSize, pSize, NULL ) )
    {
        ATG::FatalError( "TextureCompression: Failed to read from file (%s)\n",
                        strCompressedTexture );
    }

    CloseHandle( SrcFileHandle );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: MemDecompressBlob()
// Desc: Decompress a file which was generically LZX-compressed using XMemCompress
//--------------------------------------------------------------------------------------
HRESULT MemDecompressBlob( CONST BYTE* pSrcBuffer, DWORD dwSrcSize, BYTE** pDecompressedBuffer, DWORD* pDecompressedSize )
{
    HRESULT hr = S_OK;

    XMEMDECOMPRESSION_CONTEXT MemDecompressContext;
    hr = XMemCreateDecompressionContext( XMEMCODEC_DEFAULT, NULL, 0, &MemDecompressContext );

    *pDecompressedSize = *(DWORD*)pSrcBuffer;   // by convention store size in first DWORD 
    *pDecompressedBuffer = (BYTE*) malloc( *pDecompressedSize );
    if( *pDecompressedBuffer == NULL )
    {
        ATG::FatalError("TextureCompression: allocation failure\n" );
    }

    UpdateTime();

    // Allow 1 DWORD to record the uncompressed size
    if( FAILED( hr = XMemDecompress( MemDecompressContext, 
        *pDecompressedBuffer, 
        pDecompressedSize, 
        pSrcBuffer + sizeof(DWORD), 
        dwSrcSize - sizeof(DWORD) ) ) ) 
    {
        ATG::FatalError("TextureCompression: XMemDecompress returned error code (0x%x)\n",
                        hr );
    }

    UpdateTime();

    g_fMemDecompressTime = g_Time.fElapsedTime;
    
    if ( *pDecompressedSize != *(DWORD*)pSrcBuffer )
    {
        ATG::DebugSpew( "TextureCompression: Mismatch in recorded uncompressed size vs. actual uncompressed size (%d vs. %d)\n",
                        *(DWORD*)pSrcBuffer, *pDecompressedSize );
    }

    if ( *pDecompressedSize + sizeof(DWORD) < dwSrcSize )
    {
        ATG::DebugSpew( "TextureCompression: XMemCompress made data size larger (%d --> %d)\n",
                        *pDecompressedSize, dwSrcSize );
    }

    XMemDestroyDecompressionContext( MemDecompressContext );

    return hr;
}


//--------------------------------------------------------------------------------------
// Name: PTC_DecompressImage()
// Desc: Decompress a texture which was compressed using XGPTCCompressSurface
//--------------------------------------------------------------------------------------
HRESULT PTC_DecompressImage( BYTE* pSrcBuffer, DWORD dwSrcSize, IDirect3DTexture9** ppDstTexture, BYTE** ppDstBuffer, BOOL bFromFile )
{
    HRESULT hr = S_OK;

    // First undo the lossless compression
    BYTE* pTempBuffer = NULL;
    DWORD dwTempSize = 0;
    hr = MemDecompressBlob( pSrcBuffer, dwSrcSize, &pTempBuffer, &dwTempSize );

    // Create the texture and decompress.
    UINT dwWidth = 0;                  // Texture width
    UINT dwHeight = 0;                 // Texture height
    D3DFORMAT Format;
    hr = XGGetPTCImageDesc( pTempBuffer, dwTempSize, &dwWidth, &dwHeight, &Format );

    if( FAILED( hr ) )
        goto error_exit;

    *ppDstTexture = new IDirect3DTexture9;

    if( !( *ppDstTexture ) )
    {
        ATG::FatalError("TextureCompression: allocation failure\n" );
    }

    DWORD dwTextureSize = XGSetTextureHeader( dwWidth, 
        dwHeight, 
        1, 
        D3DUSAGE_CPU_CACHED_MEMORY, 
        Format, 
        0, 
        0,
        XGHEADER_CONTIGUOUS_MIP_OFFSET, 
        0,
        *ppDstTexture, 
        NULL, 
        NULL );

    *ppDstBuffer = (BYTE*) XPhysicalAlloc( dwTextureSize, MAXULONG_PTR, 0, PAGE_READWRITE );

    if( !( *ppDstBuffer ) )
    {
        ATG::FatalError("TextureCompression: allocation failure\n" );
    }

    XGOffsetResourceAddress( *ppDstTexture, *ppDstBuffer );

    XGTEXTURE_DESC Desc;
    XGGetTextureDesc( *ppDstTexture, 0, &Desc );

    D3DLOCKED_RECT LockedRect;
    ( *ppDstTexture )->LockRect( 0, &LockedRect, NULL, 0 );

    UpdateTime();

    if ( FAILED( hr = XGPTCDecompressSurface( LockedRect.pBits, LockedRect.Pitch,
                                 dwWidth, dwHeight, Format, NULL,
                                 pTempBuffer, dwTempSize ) ) )
    {
        ATG::FatalError( "TextureCompression: XGPTCDecompressSurface returned error code (0x%x)\n",
                        hr );
    }

    UpdateTime();

    g_fDecompressionTime = g_Time.fElapsedTime;
    g_fRecompressionTime = 0.0f;
    g_iTexWidth = dwWidth;
    g_iTexHeight = dwHeight;
    g_iUncompressedSize = dwTextureSize;
    g_iCompressedSize = dwSrcSize;
    g_fCompressionRatio = (100.0f * g_iCompressedSize) / g_iUncompressedSize;

    printf( "Elapsed time for XGPTCDecompressSurface = %5.5f sec\n", g_fDecompressionTime );
    printf( "Size of PTC compressed texture = %d bytes\n", g_iCompressedSize );
    printf( "Compression ratio for XGPTCDecompressSurface = %2.1f%%\n", g_fCompressionRatio );

    ( *ppDstTexture )->UnlockRect( 0 );

error_exit:
    free( pSrcBuffer );
    free( pTempBuffer );

    return hr;
}


//--------------------------------------------------------------------------------------
// Name: MCT_DecompressImage()
// Desc: Decompress a texture which was compressed using XGMCTCompressTexture
//--------------------------------------------------------------------------------------
HRESULT MCT_DecompressImage( BYTE* pSrcBuffer, DWORD dwSrcSize, IDirect3DTexture9** ppDstTexture, BYTE** ppDstBuffer )
{
    HRESULT hr = S_OK;

    // Create the texture and decompress.
    *ppDstTexture = new IDirect3DTexture9;

    if( !( *ppDstTexture ) )
    {
        ATG::FatalError("TextureCompression: allocation failure\n" );
    }

    DWORD dwTextureSize = XGMCTSetBaseTextureHeader( pSrcBuffer, dwSrcSize, 0, 0, 0,
                                                     XGHEADER_CONTIGUOUS_MIP_OFFSET, 0,
                                                     *ppDstTexture, NULL, NULL );

    *ppDstBuffer = (BYTE*) XPhysicalAlloc( dwTextureSize, MAXULONG_PTR, 0, PAGE_READWRITE );

    if( !( *ppDstBuffer ) )
    {
        ATG::FatalError("TextureCompression: allocation failure\n" );
    }

    XGOffsetResourceAddress( *ppDstTexture, *ppDstBuffer );

    XGTEXTURE_DESC Desc;
    XGGetTextureDesc( *ppDstTexture, 0, &Desc );

    UpdateTime();

    hr = XGMCTDecompressTexture( NULL, *ppDstTexture, NULL, pSrcBuffer, dwSrcSize, 0 );

    UpdateTime();

    g_fMemDecompressTime = 0.0f;
    g_fDecompressionTime = g_Time.fElapsedTime;
    g_fRecompressionTime = 0.0f;
    g_iTexWidth = Desc.Width;
    g_iTexHeight = Desc.Height;
    g_iUncompressedSize = dwTextureSize * ( 32 / Desc.BitsPerPixel );
    g_iCompressedSize = dwSrcSize;
    g_fCompressionRatio = (100.0f * g_iCompressedSize) / g_iUncompressedSize;

    printf( "Elapsed time for XGMCTDecompressTexture = %5.5f sec\n", g_fDecompressionTime );
    printf( "Size of MCT compressed texture = %d bytes\n", g_iCompressedSize );
    printf( "Compression ratio for XGMCTDecompressTexture = %2.1f%%\n", 
        g_fCompressionRatio );

    free( pSrcBuffer );

    return hr;
}


//--------------------------------------------------------------------------------------
// Name: ZTC_DecompressImage()
// Desc: Decompress a texture which was compressed using XGZTCCompressTexture
//--------------------------------------------------------------------------------------
HRESULT ZTC_DecompressImage( BYTE* pSrcBuffer, DWORD dwSrcSize, IDirect3DTexture9** ppDstTexture, BYTE** ppDstBuffer )
{
    HRESULT hr = S_OK;

    // First undo the lossless compression
    BYTE* pTempBuffer = NULL;
    DWORD dwTempSize = 0;
    hr = MemDecompressBlob( pSrcBuffer, dwSrcSize, &pTempBuffer, &dwTempSize );

    // Initialize the decompressor
    XGZTCInitializeDecompression();

    // Create the texture and decompress.
    ZTCTextureInfo TextureInfo;
    hr = XGZTCGetTextureInfo( (BYTE*) pTempBuffer, dwTempSize, &TextureInfo );

    D3DFORMAT dwFormat = D3DFMT_A8R8G8B8;
    switch( TextureInfo.Format )
    {
    case ZTC_FORMAT_A8R8G8B8:
        dwFormat = D3DFMT_LIN_A8R8G8B8;
        break;
    default:
        assert(false);  // unsupported in this sample
        break;
    }

    if( FAILED( hr ) )
        goto error_exit;

    *ppDstTexture = new IDirect3DTexture9;

    if( !( *ppDstTexture ) )
    {
        ATG::FatalError("TextureCompression: allocation failure\n" );
    }

    // Write-combined or uncached is a catastrophe here...
    DWORD dwTextureSize = XGSetTextureHeader( TextureInfo.Width, 
        TextureInfo.Height, 
        1, 
        D3DUSAGE_CPU_CACHED_MEMORY, 
        dwFormat, 
        0, 
        0, 
        XGHEADER_CONTIGUOUS_MIP_OFFSET, 
        TextureInfo.Pitch, 
        *ppDstTexture, 
        NULL, 
        NULL );
    *ppDstBuffer = (BYTE*) XPhysicalAlloc( dwTextureSize, MAXULONG_PTR, 0, PAGE_READWRITE );

    if( !( *ppDstBuffer ) )
    {
        ATG::FatalError("TextureCompression: allocation failure\n" );
    }

    XGOffsetResourceAddress( *ppDstTexture, *ppDstBuffer );

    D3DLOCKED_RECT LockedRect;
    ( *ppDstTexture )->LockRect( 0, &LockedRect, NULL, 0 );

    UpdateTime();

    hr = XGZTCDecompressTexture( (BYTE*) LockedRect.pBits, dwTextureSize, pTempBuffer, dwTempSize );
    
    UpdateTime();

    g_fDecompressionTime = g_Time.fElapsedTime;
    g_fRecompressionTime = 0.0f;
    g_iTexWidth = TextureInfo.Width;
    g_iTexHeight = TextureInfo.Height;
    g_iUncompressedSize = dwTextureSize;
    g_iCompressedSize = dwSrcSize;
    g_fCompressionRatio = (100.0f * g_iCompressedSize) / g_iUncompressedSize;

    printf( "Elapsed time for XGZTCDecompressTexture = %5.5f sec\n", g_fDecompressionTime );
    printf( "Size of ZTC compressed texture = %d bytes\n", g_iCompressedSize );
    printf( "Compression ratio for XGZTCDecompressTexture = %2.1f%%\n", 
        g_fCompressionRatio );

    ( *ppDstTexture )->UnlockRect( 0 );

error_exit:
    free( pTempBuffer );
    free( pSrcBuffer );

    return hr;
}


//--------------------------------------------------------------------------------------
// Name: DXTCompress()
// Desc: Compress texture to a DXT format using XGCompressSurface
//--------------------------------------------------------------------------------------
VOID DXTCompress( IDirect3DTexture9* pInTexture, IDirect3DTexture9** ppOutTexture, VOID** ppOutBuffer )
{
    assert( pInTexture );

    D3DSURFACE_DESC SrcDesc;
    ZeroMemory( &SrcDesc, sizeof( D3DSURFACE_DESC ) );
    pInTexture->GetLevelDesc( 0, &SrcDesc );

    // Create compressed texture and allocate memory for it
    IDirect3DTexture9* pTempTexture = new IDirect3DTexture9;

    if( !pTempTexture )
    {
        ATG::FatalError( "Memory allocation failure!" );
    }

    D3DFORMAT dwDstFormat = D3DFMT_LIN_DXT1;
    switch( SrcDesc.Format )
    {
//    case D3DFMT_LIN_X8R8G8B8:
//        dwDstFormat = D3DFMT_LIN_DXT1;
//        break;
    case D3DFMT_LIN_A8R8G8B8:
        dwDstFormat = D3DFMT_LIN_DXT5;
        break;
    case D3DFMT_LIN_A8:
        dwDstFormat = D3DFMT_LIN_DXT5A;
        break;
//    case D3DFMT_LIN_G8R8:
//        dwDstFormat = D3DFMT_LIN_DXN;
//        break;

    default:
        assert( FALSE );    // not supported by this sample
        break;
    }

    DWORD dwTextureSize = XGSetTextureHeader( SrcDesc.Width, 
        SrcDesc.Height, 
        1, 
        D3DUSAGE_CPU_CACHED_MEMORY, 
        dwDstFormat, 
        0, 
        0, 
        XGHEADER_CONTIGUOUS_MIP_OFFSET, 
        0, 
        pTempTexture, 
        NULL, 
        NULL );

    VOID *pTempBuffer = XPhysicalAlloc( dwTextureSize, MAXULONG_PTR, 0, PAGE_READWRITE );

    if( !pTempBuffer )
    {
        ATG::FatalError( "Memory allocation failure!" );
    }

    XGOffsetResourceAddress( pTempTexture, pTempBuffer );

    D3DLOCKED_RECT SrcRect = { 0, 0 };
    pInTexture->LockRect( 0, &SrcRect, NULL, D3DLOCK_READONLY );
    D3DLOCKED_RECT DstRect = { 0, 0 };
    pTempTexture->LockRect( 0, &DstRect, NULL, 0 );

    UpdateTime();

    XGCompressSurface( DstRect.pBits,
        DstRect.Pitch,
        SrcDesc.Width, 
        SrcDesc.Height,
        dwDstFormat,
        NULL,
        SrcRect.pBits,
        SrcRect.Pitch,
        SrcDesc.Format,
        NULL,
        0,
        0.0f );

    UpdateTime();

    g_fRecompressionTime = g_Time.fElapsedTime;
    printf( "Elapsed time for %s = %5.5f sec\n", 
        "XGCompressSurface", 
        g_fRecompressionTime );

    // Unlock the surfaces
    pInTexture->UnlockRect( 0 );
    pTempTexture->UnlockRect( 0 );

    if( *ppOutTexture )
        delete *ppOutTexture;
    if( *ppOutBuffer )
        XPhysicalFree( *ppOutBuffer );

    *ppOutTexture = pTempTexture;
    *ppOutBuffer = pTempBuffer;
}


//--------------------------------------------------------------------------------------
// Name: MemCompressBlob()
// Desc: Compress generic data by LZX using XMemCompress
//--------------------------------------------------------------------------------------
HRESULT MemCompressBlob( CONST BYTE* pSrcBuffer, DWORD dwSrcSize, BYTE** ppCompressedBuffer, DWORD* pCompressedSize )
{
    HRESULT hr = S_OK;

    XMEMCOMPRESSION_CONTEXT MemCompressContext;
    hr = XMemCreateCompressionContext( XMEMCODEC_DEFAULT, NULL, 0, &MemCompressContext );

    // Assume final dest will be smaller, or at least not much larger...
    *pCompressedSize = dwSrcSize * 2;
    *ppCompressedBuffer = (BYTE*) malloc( *pCompressedSize );
    assert( *ppCompressedBuffer != NULL );

    // Allow 1 DWORD to record the size
    *pCompressedSize -= sizeof(DWORD);
    if ( FAILED( hr = XMemCompress( MemCompressContext, 
        *ppCompressedBuffer + sizeof(DWORD), 
        pCompressedSize, 
        pSrcBuffer, 
        dwSrcSize ) ) )
    {
        ATG::FatalError( "TextureCompression: XMemCompress returned error code (0x%x)\n",
                        hr );
    }

    *(DWORD*)*ppCompressedBuffer = dwSrcSize;
    *pCompressedSize += sizeof(DWORD);

    if ( *pCompressedSize > dwSrcSize )
    {
        ATG::DebugSpew( "TextureCompression: XMemCompress made data size larger (%d --> %d)\n",
                        dwSrcSize, *pCompressedSize );
    }

    // Destroy the compression context
    XMemDestroyCompressionContext( MemCompressContext );

    return hr;
}


//--------------------------------------------------------------------------------------
// Name: PTC_CompressImage()
// Desc: Compress a texture using XGPTCCompressSurface
//--------------------------------------------------------------------------------------
HRESULT PTC_CompressImage( IDirect3DTexture9* pTexture, BYTE** ppCompressedBuffer, DWORD* pCompressedSize )
{
    HRESULT hr = S_OK;

    IDirect3DSurface9* pSurface;
    pTexture->GetSurfaceLevel( 0, &pSurface );

    D3DSURFACE_DESC Desc;
    pSurface->GetDesc( &Desc );

    D3DLOCKED_RECT LockedRect;
    pSurface->LockRect( &LockedRect, NULL, D3DLOCK_READONLY );

    // Compress the surface.
    BYTE* pTempBuffer = NULL;
    DWORD dwTempSize = 0;
    if ( FAILED( hr = XGPTCCompressSurface( (VOID**) &pTempBuffer, (UINT*) &dwTempSize, LockedRect.pBits, LockedRect.Pitch,
                          Desc.Width, Desc.Height, Desc.Format, NULL, 100 ) ) )
    {
        ATG::FatalError( "TextureCompression: XGPTCCompressSurface returned error code (0x%x)\n",
                        hr );
    }

    pSurface->UnlockRect();

    pSurface->Release();

    // Now do lossless compression of the results
    MemCompressBlob( pTempBuffer, dwTempSize, ppCompressedBuffer, pCompressedSize );

    XGPTCFreeMemory( (VOID*) pTempBuffer );

    return hr;
}


//--------------------------------------------------------------------------------------
// Name: MCT_CompressImage()
// Desc: Compress a texture using XGMCTCompressTexture
//--------------------------------------------------------------------------------------
HRESULT MCT_CompressImage( IDirect3DTexture9* pTexture, BYTE** ppCompressedBuffer, DWORD* pCompressedSize )
{
    HRESULT hr = S_OK;

    XGTEXTURE_DESC Desc;
    XGGetTextureDesc( pTexture, 0, &Desc );

    // Support uncompressed source data by first DXT-compressing
    IDirect3DTexture9* pTempTexture = NULL;
    VOID* pTempBuffer = NULL;
    if ( !XGIsCompressedFormat( Desc.Format ) )
    {
        DXTCompress( pTexture, &pTempTexture, &pTempBuffer );
        XGGetTextureDesc( pTempTexture, 0, &Desc );
    }
    else
    {
        pTempTexture = pTexture;
    }

    // Allocate the memory
    DWORD dwUncompressedSize = Desc.WidthInBlocks * Desc.HeightInBlocks * Desc.BytesPerBlock;
    *ppCompressedBuffer = (BYTE*) malloc( dwUncompressedSize );
    *pCompressedSize = dwUncompressedSize;

#ifdef _DEBUG
//    D3D__DisableBreakOnError = TRUE; // Work around for bug to be fixed in future build
#endif

    // Compress once to determine the sizes.
    hr = XGMCTCompressTexture( NULL, *ppCompressedBuffer, (UINT*) pCompressedSize, NULL, NULL, D3DFMT_UNKNOWN,
                               pTempTexture, NULL, XGCOMPRESS_MCT_CONTIGUOUS_MIP_LEVELS, NULL );

#ifdef _DEBUG
//    D3D__DisableBreakOnError = FALSE;
#endif

    // Make sure lossless compression of the results doesn't gain us anything...
    BYTE* pTestBuffer = NULL;
    DWORD dwTestSize = 0;
    MemCompressBlob( *ppCompressedBuffer, *pCompressedSize, &pTestBuffer, &dwTestSize );
    free( pTestBuffer );

    if ( dwTestSize < 0.98f * *pCompressedSize )
    {
        ATG::DebugSpew( "TextureCompression: XGMCTCompressTexture gave results which were further compressible (%d --> %d)\n",
                        *pCompressedSize, dwTestSize );
    }

    if( pTempTexture != pTexture )
        delete pTempTexture;
    if( pTempBuffer )
        XPhysicalFree( pTempBuffer );

    return hr;
}


//--------------------------------------------------------------------------------------
// Name: ZTC_CompressImage()
// Desc: Compress a texture using XGZTCCompressTexture
//--------------------------------------------------------------------------------------
HRESULT ZTC_CompressImage( IDirect3DTexture9* pTexture, BYTE** ppCompressedBuffer, DWORD* pCompressedSize )
{
    HRESULT hr = S_OK;

    // Initialize the compression context
    VOID* pCompressionContext;
    XGZTCInitializeCompressionContext(&pCompressionContext, 4096, 4096);

    XGTEXTURE_DESC Desc;
    XGGetTextureDesc( pTexture, 0, &Desc );

    assert( !XGIsTiledFormat( Desc.Format ) );  // ZTC compression requires a linear format

    DWORD dwGPUFormat = XGGetGpuFormat( Desc.Format );
    DWORD dwZTCFormat = ZTC_FORMAT_A8R8G8B8;

    switch( dwGPUFormat )
    {
    case GPUTEXTUREFORMAT_8_8_8_8:
    case GPUTEXTUREFORMAT_8_8_8_8_AS_16_16_16_16:
        dwZTCFormat = ZTC_FORMAT_A8R8G8B8;
        break;
    //case GPUTEXTUREFORMAT_8:
    //    dwZTCFormat = ZTC_FORMAT_D8;
    //    break;
    default:
        assert(false);  // unsupported
        break;
    }

    ZTCTextureInfo ZTCInfo = 
    {
        Desc.Width, 
        Desc.RowPitch, 
        Desc.Height, 
        dwZTCFormat, 
    };

    ZTCQuality Quality = 
    {
        90,     // DWORD       dwQuality;       
        90,     // DWORD       dwAlphaQuality;  
        TRUE,   // BOOL        bSubsampleColor; 
    };

    D3DLOCKED_RECT LockedRect;
    pTexture->LockRect( 0, &LockedRect, NULL, D3DLOCK_READONLY );

    // Compress once to compute required size
    DWORD dwTempSize = 0;
    hr = XGZTCCompressTexture( pCompressionContext, NULL, &dwTempSize, (BYTE*)LockedRect.pBits, &ZTCInfo, &Quality );
    if( hr != D3DERR_MOREDATA )
    {
        ATG::FatalError( "TextureCompression: XGZTCCompressTexture returned error code (0x%x)\n",
                        hr );
    }

    BYTE* pTempBuffer = (BYTE*) malloc( dwTempSize );

    // Compress the surface.
    if ( FAILED( hr = 
        XGZTCCompressTexture( pCompressionContext, pTempBuffer, &dwTempSize, (BYTE*)LockedRect.pBits, &ZTCInfo, &Quality ) ) )
    {
        ATG::FatalError( "TextureCompression: XGZTCCompressTexture returned error code (0x%x)\n",
                        hr );
    }

    pTexture->UnlockRect( 0 );

    // Now do lossless compression of the results
    MemCompressBlob( pTempBuffer, dwTempSize, ppCompressedBuffer, pCompressedSize );

    free( pTempBuffer );

    // Destroy the compression context
    XGZTCDestroyCompressionContext( pCompressionContext );

    return hr;
}


//--------------------------------------------------------------------------------------
// Name: SaveCompressedFile()
// Desc: Save a compressed data blob to a file
//--------------------------------------------------------------------------------------
HRESULT SaveCompressedFile( CONST BYTE* pCompressedBuffer, DWORD dwCompressedSize, const char* strFileName )
{
    HRESULT hr = S_OK;

    // Save the compressed data.
    HANDLE FileHandle = CreateFile( strFileName, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0,
                                    NULL );

    if( FileHandle == INVALID_HANDLE_VALUE )
    {
        if( ERROR_ACCESS_DENIED == GetLastError() )
        {
            ATG::DebugSpew( "TextureCompression: Failed to open file (%s) for write --- are you running from DVD emulation?\n",
                strFileName );
        }
        else
        {
            ATG::DebugSpew( "TextureCompression: Failed to open file (%s) for write\n",
                strFileName );
        }

        return E_FAIL;
    }

    DWORD BytesWritten;
    if( WriteFile( FileHandle, pCompressedBuffer, dwCompressedSize, &BytesWritten, NULL ) == 0 )
        hr = GetLastError();

    CloseHandle( FileHandle );

    // Free the memory.
    free( (VOID*) pCompressedBuffer );

    return hr;
}


//--------------------------------------------------------------------------------------
// Name: ReloadImages()
// Desc: Called at init-time and every .5 seconds: checks if file time-stamps
//       have changed, and reloads them if they have been updated.
//       It generates the same number of mip-levels for the Source image as were
//       created in the PTC-Compressed image.
//--------------------------------------------------------------------------------------
HRESULT ReloadImages( bool bForceLoad = false )
{
    HRESULT hr = S_OK;

    const char* strBase = strSourceFileNames[g_SourceIndex].strBase;
    const char* strOriginalExt = strSourceFileNames[g_SourceIndex].strExt;
    const char* strCompressedExt = g_CompressionModeInfo[g_CompressionMode].strCompressedExtension;

    char strOriginalTexture[1024];
    char strCompressedTexture[1024];

    sprintf_s( strOriginalTexture, ARRAYSIZE(strOriginalTexture), "game:\\Media\\Textures\\%s%s", strBase, strOriginalExt );
    sprintf_s( strCompressedTexture, ARRAYSIZE(strCompressedTexture), "game:\\Media\\Textures\\%s%s", strBase, strCompressedExt );

    if( FileChanged( strOriginalTexture, &g_OriginalUpdateTime ) || bForceLoad )
    {
        IDirect3DTexture9* pTempTexture = NULL;

        // Load the uncompressed file.
        HANDLE SrcFileHandle = CreateFile( strOriginalTexture, GENERIC_READ, 0, 0,
                                           OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN,
                                           NULL );

        if( SrcFileHandle == INVALID_HANDLE_VALUE )
        {
            ATG::DebugSpew( "TextureCompression: Failed to open file (%s). Compressing at runtime instead.\n",
                            strOriginalTexture );
            g_bLoadSuccessful = false;

            return E_FAIL;
        }

        DWORD SrcFileSize = GetFileSize( SrcFileHandle, 0 );
        void* pSrcFileMem = malloc( SrcFileSize );
        if( !pSrcFileMem )
        {
            ATG::DebugSpew( "TextureCompression: Failed to allocate memory for file (%s)\n",
                            strOriginalTexture );

            CloseHandle( SrcFileHandle );
            g_bLoadSuccessful = false;

            return E_FAIL;
        }

        DWORD BytesRead;
        if( !ReadFile( SrcFileHandle, pSrcFileMem, SrcFileSize, &BytesRead, NULL ) )
        {
            ATG::DebugSpew( "TextureCompression: Failed to read from file (%s)\n",
                            strCompressedTexture );

            CloseHandle( SrcFileHandle );
            g_bLoadSuccessful = false;

            return E_FAIL;
        }

        CloseHandle( SrcFileHandle );

        // Create a texture from the data.
        hr = D3DXCreateTextureFromFileInMemoryEx( ATG::g_pd3dDevice, pSrcFileMem, BytesRead,
                                                  D3DX_DEFAULT_NONPOW2,
                                                  D3DX_DEFAULT_NONPOW2,
                                                  1, 0, D3DFMT_UNKNOWN,
                                                  D3DPOOL_MANAGED,
                                                  D3DX_DEFAULT, D3DX_DEFAULT,
                                                  0, NULL, NULL, &pTempTexture );

        free( pSrcFileMem );

        if( FAILED( hr ) )
        {
            ATG::DebugSpew( "TextureCompression: Failed to load new texture (%s)\n",
                            strOriginalTexture );

            g_bLoadSuccessful = false;
            return hr;
        }
        else
        {
            if( g_pOriginalTexture )
                g_pOriginalTexture->Release();

            g_pOriginalTexture = pTempTexture;
            g_bLoadSuccessful = true;
        }
    }

    if( FileChanged( strCompressedTexture, &g_CompressedUpdateTime ) || bForceLoad )
    {
        IDirect3DTexture9* pTempTexture = NULL;
        BYTE* pTempSrcBuffer = NULL;
        BYTE* pTempDstBuffer = NULL;
        DWORD dwTempSrcBufferSize = 0;

        hr = LoadCompressedFile( strCompressedTexture, &pTempSrcBuffer, &dwTempSrcBufferSize );
        BOOL bFileExists = SUCCEEDED( hr );

        switch( g_CompressionMode )
        {
        case MODE_PTC:
            if( !bFileExists )
            {
                hr = PTC_CompressImage( g_pOriginalTexture, &pTempSrcBuffer, &dwTempSrcBufferSize );
            }
            hr = PTC_DecompressImage( pTempSrcBuffer, dwTempSrcBufferSize, &pTempTexture, &pTempDstBuffer, bFileExists ); 
            break;
        case MODE_MCT:
            if( !bFileExists )
            {
                hr = MCT_CompressImage( g_pOriginalTexture, &pTempSrcBuffer, &dwTempSrcBufferSize );
            }
            hr = MCT_DecompressImage( pTempSrcBuffer, dwTempSrcBufferSize, &pTempTexture, &pTempDstBuffer ); 
            break;
        case MODE_DXT:
            pTempTexture = g_pOriginalTexture;
            hr = S_OK;
            break;
        case MODE_ZTC:
        case MODE_ZTC_DXT:
            if( !bFileExists )
            {
                hr = ZTC_CompressImage( g_pOriginalTexture, &pTempSrcBuffer, &dwTempSrcBufferSize );
            }
            hr = ZTC_DecompressImage( pTempSrcBuffer, dwTempSrcBufferSize, &pTempTexture, &pTempDstBuffer ); 
            break;
        default:
            assert( false );    // unreachable
            break;
        }

        if( FAILED( hr ) )
        {
            ATG::DebugSpew( "TextureCompression: Failed to load new texture (%s)\n",
                strCompressedTexture );

            if( pTempTexture )
                delete pTempTexture;
            if( pTempDstBuffer )
                XPhysicalFree( pTempDstBuffer );

            return hr;
        }

        if( g_pCompressedTexture )
            delete g_pCompressedTexture;
        if( g_pCompressedTextureBuffer )
            XPhysicalFree( g_pCompressedTextureBuffer );

        g_pCompressedTexture = pTempTexture;
        g_pCompressedTextureBuffer = pTempDstBuffer;

        // Compress source and ZTC as DXT to see cumulative errors.
        // The source is compressed using the slow API and the ZTC is
        // compressed using the fast, low-fidelity API.
        if ( g_CompressionMode == MODE_DXT && !bFileExists )
        {
            pTempTexture = NULL;
            DXTCompress( g_pCompressedTexture, &pTempTexture, &g_pCompressedTextureBuffer );
            g_pCompressedTexture = pTempTexture;

            XGTEXTURE_DESC Desc;
            XGGetTextureDesc( g_pCompressedTexture, 0, &Desc );

            g_fMemDecompressTime = 0.0f;
            g_fDecompressionTime = 0.0f;
            g_fRecompressionTime = 0.0f;
            g_iTexWidth = Desc.Width;
            g_iTexHeight = Desc.Height;
            g_iCompressedSize = Desc.WidthInBlocks * Desc.HeightInBlocks * Desc.BytesPerBlock;
            g_iUncompressedSize = g_iCompressedSize * ( 32 / Desc.BitsPerPixel );
            g_fCompressionRatio = (100.0f * g_iCompressedSize) / g_iUncompressedSize;
        }
        if ( g_CompressionMode == MODE_ZTC_DXT )
        {
            DXTCompress( g_pCompressedTexture, &g_pCompressedTexture, &g_pCompressedTextureBuffer );
        }
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: InitScene()
// Desc: Creates the scene.  First we compile our shaders. For the final version
//       of a game, you should store the shaders in binary form; don't call
//       D3DXCompileShader at runtime. Next, we declare the format of our
//       vertices, and then create a vertex buffer. The vertex buffer is basically
//       just a chunk of memory that holds vertices. After creating it, we must
//       Lock()/Unlock() it to fill it. Finally, we set up our world, projection,
//       and view matrices.
//--------------------------------------------------------------------------------------
HRESULT InitScene()
{
    // Create the help
    if( FAILED( g_Help.Create( "game:\\Media\\Help\\Help.xpr" ) ) )
    {
        ATG::DebugSpew( "PTC_Test: Couldn't load help.xpr\n" );
        return E_FAIL;
    }

    // Create the font
    if( FAILED( g_Font.Create( "game:\\Media\\Fonts\\Arial_16.xpr" ) ) )
    {
        ATG::DebugSpew( "PTC_Test: Couldn't create font\n" );
        return E_FAIL;
    }

    // Confine text drawing to the title safe area
    g_Font.SetWindow( ATG::GetTitleSafeArea() );

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
        {
            ATG::DebugSpew( ( char* )pVertexErrorMsg->GetBufferPointer() );
            ATG::DebugSpew( "\n" );
        }

        return E_FAIL;
    }

    // Create vertex shader.
    ATG::g_pd3dDevice->CreateVertexShader( ( DWORD* )pVertexShaderCode->GetBufferPointer(),
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
        {
            ATG::DebugSpew( ( char* )pPixelErrorMsg->GetBufferPointer() );
            ATG::DebugSpew( "\n" );
        }

        return E_FAIL;
    }

    // Create pixel shader.
    ATG::g_pd3dDevice->CreatePixelShader( ( DWORD* )pPixelShaderCode->GetBufferPointer(),
                                          &g_pPixelShader );

    // Define the vertex elements and
    // Create a vertex declaration from the element descriptions.
    static const D3DVERTEXELEMENT9 VertexElements[3] =
    {
        { 0,  0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
        { 0, 12, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
        D3DDECL_END()
    };
    ATG::g_pd3dDevice->CreateVertexDeclaration( VertexElements, &g_pVertexDecl );

    // Create the vertex buffer. Here we are allocating enough memory
    // (from the default pool) to hold all our 3 custom vertices.
    if( FAILED( ATG::g_pd3dDevice->CreateVertexBuffer( 4 * sizeof( COLORVERTEX ),
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
        { -1.0f,  1.0f, 0.0f, 0.0f, 0.0f }, // x, y, z, u, v
        {  1.0f,  1.0f, 0.0f, 1.0f, 0.0f },
        {  1.0f, -1.0f, 0.0f, 1.0f, 1.0f },
        { -1.0f, -1.0f, 0.0f, 0.0f, 1.0f },
    };

    COLORVERTEX* pVertices;
    if( FAILED( g_pVB->Lock( 0, 0, ( void** )&pVertices, 0 ) ) )
        return E_FAIL;

    memcpy( pVertices, g_Vertices, 4 * sizeof( COLORVERTEX ) );

    g_pVB->Unlock();

    // Initialize the world matrix
    g_matWorld = XMMatrixIdentity();

    // Initialize the projection matrix
    XVIDEO_MODE VideoMode;
    ZeroMemory( &VideoMode, sizeof( VideoMode ) );
    XGetVideoMode( &VideoMode );

    FLOAT fAspectRatio = ( VideoMode.fIsWideScreen ) ? ( 16.0f / 9.0f ) : ( 4.0f / 3.0f );

    g_matProj = XMMatrixPerspectiveFovLH( XM_PI / 4, fAspectRatio, 1.0f, 200.0f );

    // Initialize the view matrix
    XMVECTOR vEyePt = XMVectorSet( 0.0f, 0.0f, -4.0f, 0.0f );
    XMVECTOR vLookatPt = XMVectorSet( 0.0f, 0.0f, 0.0f, 0.0f );
    XMVECTOR vUp = XMVectorSet( 0.0f, 1.0f, 0.0f, 0.0f );
    g_matView = XMMatrixLookAtLH( vEyePt, vLookatPt, vUp );

    // Create
    hr = ReloadImages( true );
    if( FAILED( hr ) )
    {
        return hr;
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Update()
// Desc: Updates the world for the next frame
//--------------------------------------------------------------------------------------
void Update()
{
    // Check to see if the texture files have ben updated periodically.
    g_fTimeSinceUpdate += g_Time.fElapsedTime;

    if( g_fTimeSinceUpdate >= 0.5f )
    {
        ReloadImages();
        g_fTimeSinceUpdate = 0.0f;
    }

    g_fTriggerTimer -= g_Time.fElapsedTime;

    ATG::GAMEPAD* pGamepad = ATG::Input::GetMergedInput();

    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_A )
    {
        if( g_DoComparison == 0.0f )
        {
            g_DoComparison = 1.0f;
        }
        else if( g_DoComparison == 1.0f )
        {
            g_DoComparison = 8.0f;
        }
        else if( g_DoComparison == 8.0f )
        {
            g_DoComparison = -1.0f;
        }
        else
        {
            g_DoComparison = 0.0f;
        }
    }

    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_BACK )
        g_bDrawHelp = !g_bDrawHelp;

    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_X )
        g_bShowAlpha = !g_bShowAlpha;

    g_fSaveDisplayTime -= g_Time.fElapsedTime;

    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_Y )
    {
        // Map the devkit drive (this will only work on a devkit).
        HRESULT hr = DmMapDevkitDrive();

        if( hr == S_OK )
        {
            BYTE* pCompressedBuffer = NULL;
            DWORD dwCompressedSize = 0;

            const char* strBase = strSourceFileNames[g_SourceIndex].strBase;
            const char* strCompressedExt = g_CompressionModeInfo[g_CompressionMode].strCompressedExtension;
            char strFileName[1024];
            sprintf_s( strFileName, ARRAYSIZE(strFileName), "game:\\Media\\Textures\\%s%s", strBase, strCompressedExt );

            switch( g_CompressionMode )
            {
            case MODE_PTC:
                hr = PTC_CompressImage( g_pOriginalTexture, &pCompressedBuffer, &dwCompressedSize );
                break;
            case MODE_MCT:
                hr = MCT_CompressImage( g_pOriginalTexture, &pCompressedBuffer, &dwCompressedSize );
                break;
            case MODE_DXT:
                // not supported
                hr = E_FAIL;
                break;
            case MODE_ZTC:
            case MODE_ZTC_DXT:
                hr = ZTC_CompressImage( g_pOriginalTexture, &pCompressedBuffer, &dwCompressedSize );
                break;
            }

            if( SUCCEEDED(hr) && pCompressedBuffer != NULL && dwCompressedSize > 0 )
            {
                hr = SaveCompressedFile( pCompressedBuffer, dwCompressedSize, strFileName );
            }
        }

        g_fSaveDisplayTime = 4.0f;
        g_fTimeSinceUpdate = 0.0f;  // don't immediately load the texture back...
        g_bSaveSuccessful = ( hr == S_OK );

        // Update time to allow for the time it took to compress and save the image.
        UpdateTime();
    }

    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_LEFT_SHOULDER )
    {
        g_fTriggerTimer = 0.5f;
        g_CompressionMode = ( g_CompressionMode == 0 ) ? ( MODE_MAX - 1 ) : ( g_CompressionMode - 1 );
        ReloadImages( true );
    }
    else if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER )
    {
        g_fTriggerTimer = 0.5f;
        g_CompressionMode = ( g_CompressionMode == ( MODE_MAX - 1 ) ) ? 0 : ( g_CompressionMode + 1 );
        ReloadImages( true );
    }
    else if( g_fTriggerTimer <= 0.0f && pGamepad->bLeftTrigger > 0 )
    {
        g_SourceIndex = ( g_SourceIndex == 0 ) ? ( SOURCE_FILE_COUNT - 1 ) : ( g_SourceIndex - 1 );
        ReloadImages( true );
    }
    else if( g_fTriggerTimer <= 0.0f && pGamepad->bRightTrigger > 0 )
    {
        g_SourceIndex = ( g_SourceIndex == ( SOURCE_FILE_COUNT - 1 ) ) ? 0 : ( g_SourceIndex + 1 );
        ReloadImages( true );
    }
}


//--------------------------------------------------------------------------------------
// Name: Render()
// Desc: Draws the scene
//--------------------------------------------------------------------------------------
void Render()
{
    assert( g_CompressionMode == MODE_PTC || g_CompressionMode == MODE_MCT 
        || g_CompressionMode == MODE_ZTC || g_CompressionMode == MODE_DXT 
        || g_CompressionMode == MODE_ZTC_DXT );

    D3DDISPLAYMODE DisplayMode;
    ATG::g_pd3dDevice->GetDisplayMode( 0, &DisplayMode );

    XMMATRIX matWVP;

    // Clear the backbuffer to a blue color
    ATG::g_pd3dDevice->Clear( 0L, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL,
                              D3DCOLOR_XRGB( 0, 0, 255 ), 1.0f, 0L );

    // Draw the triangles in the vertex buffer. This is broken into a few steps:
    if( g_pOriginalTexture && g_pCompressedTexture )
    {
        // Force the texture formats to AS_16 sRGB formats for gamma correction.
        // Do this so there's no loss of precision when sampling the texture in the shader. If using
        // a standard SRGB format, the sRGB->Linear conversion happens with 8 bit precision, resulting 
        // in a loss of data. Using an AS_16 sRGB format causes the conversion to happen at 16-bit precision,
        // meaning no precision is lost.
        IDirect3DTexture9 OriginalTextureSRGB = *g_pOriginalTexture;
        IDirect3DTexture9 CompressedTextureSRGB = *g_pCompressedTexture;
        ATG::ConvertTextureToAs16SRGBFormat( &OriginalTextureSRGB );
        ATG::ConvertTextureToAs16SRGBFormat( &CompressedTextureSRGB );

        // We are passing the vertices down a "stream", so first we need
        // to specify the source of that stream, which is our vertex buffer.
        // Then we need to let D3D know what vertex and pixel shaders to use.
        ATG::g_pd3dDevice->SetVertexDeclaration( g_pVertexDecl );
        ATG::g_pd3dDevice->SetStreamSource( 0, g_pVB, 0, sizeof( COLORVERTEX ) );
        ATG::g_pd3dDevice->SetVertexShader( g_pVertexShader );
        ATG::g_pd3dDevice->SetPixelShader( g_pPixelShader );

        // Setup to draw the uncompressed texture.
        g_matWorld = XMMatrixTranslation( -1.05f, 0.0f, 0.0f );

        // Build the world-view-projection matrix and pass it into the vertex shader
        matWVP = g_matWorld * g_matView * g_matProj;
        ATG::g_pd3dDevice->SetVertexShaderConstantF( 0, ( FLOAT* )&matWVP, 4 );

        float DoCompare[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
        DoCompare[2] = ( g_bShowAlpha ) ? 1.0f : 0.0f;

        ATG::g_pd3dDevice->SetPixelShaderConstantF( 0, &DoCompare[0], 1 );

        // Set the original and compressed textures
        ATG::g_pd3dDevice->SetTexture( 0, &CompressedTextureSRGB );
        ATG::g_pd3dDevice->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_POINT );
        ATG::g_pd3dDevice->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_POINT );
        ATG::g_pd3dDevice->SetSamplerState( 0, D3DSAMP_MIPFILTER, D3DTEXF_POINT );
        ATG::g_pd3dDevice->SetTexture( 1, &OriginalTextureSRGB );
        ATG::g_pd3dDevice->SetSamplerState( 1, D3DSAMP_MINFILTER, D3DTEXF_POINT );
        ATG::g_pd3dDevice->SetSamplerState( 1, D3DSAMP_MAGFILTER, D3DTEXF_POINT );
        ATG::g_pd3dDevice->SetSamplerState( 1, D3DSAMP_MIPFILTER, D3DTEXF_POINT );

        // The address mode must be set to clamp in case we have a linear texture.
        ATG::g_pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP );
        ATG::g_pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP );
        ATG::g_pd3dDevice->SetSamplerState( 1, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP );
        ATG::g_pd3dDevice->SetSamplerState( 1, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP );

        ATG::g_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, 
            ( g_bShowAlpha || g_DoComparison > 0.0f ) ? FALSE : TRUE );
        ATG::g_pd3dDevice->SetRenderState( D3DRS_SRCBLEND, D3DBLEND_SRCALPHA );
        ATG::g_pd3dDevice->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA );
        ATG::g_pd3dDevice->SetRenderState( D3DRS_BLENDOP, D3DBLENDOP_ADD );

        // Draw the vertices in the vertex buffer
        ATG::g_pd3dDevice->DrawPrimitive( D3DPT_QUADLIST, 0, 1 );

        // Setup to draw the compressed texture.
        g_matWorld = XMMatrixTranslation( 1.05f, 0.0f, 0.0f );

        // Build the world-view-projection matrix and pass it into the vertex shader
        matWVP = g_matWorld * g_matView * g_matProj;
        ATG::g_pd3dDevice->SetVertexShaderConstantF( 0, ( FLOAT* )&matWVP, 4 );

        int PtcSamplerNumber;
        if( g_DoComparison > 0.0f )
        {
            PtcSamplerNumber = 0;
            DoCompare[0] = g_DoComparison;
        }
        else if( g_DoComparison == 0.0f )
        {
            DoCompare[0] = 0.0f;
            PtcSamplerNumber = 1;
        }
        else //if( g_DoComparison < 0.0f )    // flashing
        {
            DoCompare[0] = 0.0f;
            PtcSamplerNumber = ( g_fTimeSinceUpdate > 0.25 ) ? 1 : 0;
        }

        ATG::g_pd3dDevice->SetPixelShaderConstantF( 0, &DoCompare[0], 1 );
        ATG::g_pd3dDevice->SetTexture( PtcSamplerNumber, &CompressedTextureSRGB );

        // Draw the vertices in the vertex buffer
        ATG::g_pd3dDevice->DrawPrimitive( D3DPT_QUADLIST, 0, 1 );

        // Clear textures.
        ATG::g_pd3dDevice->SetTexture( 0, NULL );
        ATG::g_pd3dDevice->SetTexture( 1, NULL );
    }
    else
    {
        g_Font.Begin();

        // Get the title safe are so we can center within it.
        D3DRECT rect = ATG::GetTitleSafeArea();

        g_Font.SetScaleFactors( 1.2f, 1.2f );
        g_Font.DrawText( ( rect.x1 + rect.x2 ) / 2.0f - rect.x1,
                         ( rect.y1 + rect.y2 ) / 2.0f - rect.y1,
                         0xffffff00,
                         L"Copy original image to \n"
                         L"game:\\media\\textures\\PTC_TestImage.bmp \n"
                         L"and compressed image to \n"
                         L"game:\\media\\textures\\PTC_TestImage.ptc \n"
                         L"in order to view",
                         ATGFONT_CENTER_X | ATGFONT_CENTER_Y );

        g_Font.End();
    }

    if( g_bDrawHelp )
    {
        g_Help.Render( &g_Font, g_HelpCallouts, NUM_HELP_CALLOUTS );
    }
    else
    {
        WCHAR strBaseW[1024];
        mbstowcs_s( NULL, strBaseW, ARRAYSIZE(strBaseW), strSourceFileNames[g_SourceIndex].strBase, 
            ARRAYSIZE(strBaseW) );
        WCHAR strCompressedExtW[1024];
        mbstowcs_s( NULL, strCompressedExtW, ARRAYSIZE(strCompressedExtW), 
            g_CompressionModeInfo[g_CompressionMode].strCompressedExtension, ARRAYSIZE(strCompressedExtW) );
        WCHAR strFileW[1024]; 
        swprintf_s( strFileW, ARRAYSIZE(strFileW), L"%s%s", strBaseW, strCompressedExtW );
        
        WCHAR strTitleSuccessW[1024]; 
        swprintf_s( strTitleSuccessW, ARRAYSIZE(strTitleSuccessW), L"TextureCompression: %s", strFileW );
        WCHAR strTitleFailureW[1024]; 
        swprintf_s( strTitleFailureW, ARRAYSIZE(strTitleFailureW), L"%s - LOAD FAILED!", strTitleSuccessW );

        g_Font.Begin();

        // Figure out where to put the image labels.
        XVIDEO_MODE VideoMode;
        ZeroMemory( &VideoMode, sizeof( VideoMode ) );
        XGetVideoMode( &VideoMode );

        FLOAT fTextOffsetScale = ( VideoMode.fIsWideScreen ) ? 0.75f : 1.0f;

        FLOAT fLeftTextPos = 0.5f - 0.25f * fTextOffsetScale;
        FLOAT fRightTextPos = 0.5f + 0.25f * fTextOffsetScale;

        // Get the title safe are so we can adjust for it.
        D3DRECT rect = ATG::GetTitleSafeArea();

        g_Font.SetScaleFactors( 1.2f, 1.2f );
        g_Font.DrawText( 0, 0, 0xffffffff, g_bLoadSuccessful ? strTitleSuccessW : strTitleFailureW );

        g_Font.SetScaleFactors( 1.0f, 1.0f );

        g_Font.DrawText( ( FLOAT )DisplayMode.Width * fLeftTextPos - rect.x1, 
            40, 0xff00ff00, g_bShowAlpha ? L"Alpha" : L"Color", ATGFONT_CENTER_X );

        if( g_DoComparison == 1.0f )
        {
            g_Font.DrawText( ( FLOAT )DisplayMode.Width * fRightTextPos - rect.x1, 
                40, 0xff00ff00, L"Ptc - Src", ATGFONT_CENTER_X );
        }
        else if( g_DoComparison == 8.0f )
        {
            g_Font.DrawText( ( FLOAT )DisplayMode.Width * fRightTextPos - rect.x1, 
                40, 0xff00ff00, L"8 * Ptc - Src", ATGFONT_CENTER_X );
        }
        else if( g_DoComparison == -1.0f )
        {
            g_Font.DrawText( ( FLOAT )DisplayMode.Width * fRightTextPos - rect.x1, 
                40, 0xff00ff00, L"Flashing Diffs", ATGFONT_CENTER_X );
        }

        // Diplay save status.
        if( g_fSaveDisplayTime > 0.0f )
        {
            if( g_bSaveSuccessful )
            {
                WCHAR strSuccessW[1024]; 
                swprintf_s( strSuccessW, ARRAYSIZE(strSuccessW), 
                    L"Successfully saved compressed image to game:\\Media\\Textures\\%s", strFileW );

                g_Font.DrawText( ( rect.x1 + rect.x2 ) / 2.0f - rect.x1,
                                 ( rect.y1 + rect.y2 ) / 2.0f - rect.y1,
                                 0xffffff00,
                                 strSuccessW,
                                 ATGFONT_CENTER_X | ATGFONT_CENTER_Y );
            }
            else
            {
                g_Font.DrawText( ( rect.x1 + rect.x2 ) / 2.0f - rect.x1,
                                 ( rect.y1 + rect.y2 ) / 2.0f - rect.y1,
                                 0xffffff00,
                                 L"Save failed\n",
                                 ATGFONT_CENTER_X | ATGFONT_CENTER_Y );
            }
        }

        g_Font.DrawText( ( FLOAT )DisplayMode.Width * fLeftTextPos - rect.x1,
                         ( FLOAT )DisplayMode.Height - 120 - rect.y1,
                         0xffffff00,
                         g_CompressionModeInfo[g_CompressionMode].strOriginalTextureLabel,
                         ATGFONT_CENTER_X );

        g_Font.DrawText( ( FLOAT )DisplayMode.Width * fRightTextPos - rect.x1,
                         ( FLOAT )DisplayMode.Height - 120 - rect.y1,
                         0xffffff00,
                         g_CompressionModeInfo[g_CompressionMode].strCompressedTextureLabel,
                         ATGFONT_CENTER_X );

        WCHAR strDimsW[1024];
        swprintf_s(strDimsW, ARRAYSIZE(strDimsW), L"Dimensions = %d X %d", g_iTexWidth, g_iTexHeight );
        WCHAR strUncompressedSizeW[1024];
        swprintf_s(strUncompressedSizeW, ARRAYSIZE(strUncompressedSizeW), L"Uncompressed size = %d bytes", g_iUncompressedSize );

        g_Font.DrawText( ( FLOAT )DisplayMode.Width * fLeftTextPos - rect.x1,
                         ( FLOAT )DisplayMode.Height - 90 - rect.y1,
                         0xffff00ff,
                         strDimsW,
                         ATGFONT_CENTER_X );

        g_Font.DrawText( ( FLOAT )DisplayMode.Width * fLeftTextPos - rect.x1,
                         ( FLOAT )DisplayMode.Height - 60 - rect.y1,
                         0xffff00ff,
                         strUncompressedSizeW,
                         ATGFONT_CENTER_X );

        WCHAR strDecompressionTimeW[1024];
        swprintf_s(strDecompressionTimeW, ARRAYSIZE(strDecompressionTimeW), L"Processing: (%s/%s/%s) = %2.1f / %2.1f / %2.1f ms", 
            ( g_fMemDecompressTime > 0.0f ) ? L"LZX" : L"***", 
            g_CompressionModeInfo[g_CompressionMode].strCompressionName, 
            ( g_fRecompressionTime > 0.0f ) ? L"DXT" : L"***", 
            1000 * g_fMemDecompressTime, 
            1000 * g_fDecompressionTime, 
            1000 * g_fRecompressionTime);
        WCHAR strCompressedSizeW[1024];
        swprintf_s(strCompressedSizeW, ARRAYSIZE(strCompressedSizeW), L"Compressed size = %d bytes", g_iCompressedSize );
        WCHAR strCompressionRatioW[1024];
        swprintf_s(strCompressionRatioW, ARRAYSIZE(strCompressionRatioW), L"Compression ratio = %2.1f%%", g_fCompressionRatio );

        g_Font.DrawText( ( FLOAT )DisplayMode.Width * fRightTextPos - rect.x1,
                         ( FLOAT )DisplayMode.Height - 90 - rect.y1,
                         0xffff00ff,
                         strDecompressionTimeW,
                         ATGFONT_CENTER_X );

        g_Font.DrawText( ( FLOAT )DisplayMode.Width * fRightTextPos - rect.x1,
                         ( FLOAT )DisplayMode.Height - 60 - rect.y1,
                         0xffff00ff,
                         strCompressedSizeW,
                         ATGFONT_CENTER_X );

        g_Font.DrawText( ( FLOAT )DisplayMode.Width * fRightTextPos - rect.x1,
                         ( FLOAT )DisplayMode.Height - 30 - rect.y1,
                         0xffff00ff,
                         strCompressionRatioW,
                         ATGFONT_CENTER_X );

        g_Font.End();
    }

    // Present the backbuffer contents to the display
    ATG::g_pd3dDevice->Present( NULL, NULL, NULL, NULL );
}


//--------------------------------------------------------------------------------------
// Name: main()
// Desc: The application's entry point
//--------------------------------------------------------------------------------------
void __cdecl main()
{
    InitTime();

    // Initialize Direct3D
    if( FAILED( InitD3D() ) )
        return;

    // Initialize the vertex buffer
    if( FAILED( InitScene() ) )
        return;

    for(; ; )
    {
        // What time is it?
        UpdateTime();

        // Update the world
        Update();

        // Render the scene
        Render();
    }
}

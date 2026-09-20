//-----------------------------------------------------------------------------
// GPUMemoryMove.cpp
//
// Uses the GPU to move data within main memory.
//
// XNA Developer Connection
// Copyright (C) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------

#pragma warning(push)
// \Include\Xbox\list(1143) : warning C4127: conditional expression is constant
#pragma warning(disable : 4127)
#include <list>
#pragma warning(pop)

#include <xtl.h>
#include <xgraphics.h>
#include <xboxmath.h>
#include <d3dx9.h>
#include <AtgApp.h>
#include <AtgDebugDraw.h>
#include <AtgFont.h>
#include <AtgHelp.h>
#include <AtgInput.h>
#include <AtgPostProcess.h>
#include <AtgSimpleShaders.h>
#include <AtgUtil.h>
#include "GPUMemPool.h"

using std::list;

//--------------------------------------------------------------------------------------
// A selection of colors for the memory bar graph
//--------------------------------------------------------------------------------------
static const D3DCOLOR MEM_DEBUG_COLORS[] =
{
    D3DCOLOR_ARGB( 0x00, 0x40, 0x40, 0x40 ),
    D3DCOLOR_ARGB( 0x00, 0xc0, 0x40, 0x40 ),
    D3DCOLOR_ARGB( 0x00, 0x40, 0xc0, 0x40 ),
    D3DCOLOR_ARGB( 0x00, 0x40, 0x40, 0xc0 ),
    D3DCOLOR_ARGB( 0x00, 0xc0, 0xc0, 0x40 ),
    D3DCOLOR_ARGB( 0x00, 0xc0, 0x40, 0xc0 ),
    D3DCOLOR_ARGB( 0x00, 0x40, 0xc0, 0xc0 ),
    D3DCOLOR_ARGB( 0x00, 0xc0, 0xc0, 0xc0 ),
};
#define NUM_MEM_DEBUG_COLORS (ARRAYSIZE(MEM_DEBUG_COLORS))

static const D3DCOLOR           BACKGROUND_COLOR = D3DCOLOR_ARGB( 0x00, 0x80, 0x80, 0x80 );

static DWORD                    g_dwMemDebugColorIndex = 0;

static const FLOAT              RECT_LIFETIME_MIN_IN_SEC = 2.0f;
static const FLOAT              RECT_LIFETIME_MAX_IN_SEC = 8.0f;

static const DWORD              MEM_POOL_SIZE = 4 * 1024 * 1024;  // Must be at least (1280 x 720 x 4) =~ 4.0 Mb
static const DWORD              MEM_POOL_ALIGNMENT = 4 * 1024;         // Required alignment for A8R8G8B8 textures


//--------------------------------------------------------------------------------------
// Callouts for labeling the gamepad on the help screen
//--------------------------------------------------------------------------------------
ATG::HELP_CALLOUT g_HelpCallouts[] =
{
    { ATG::HELP_BACK_BUTTON,  ATG::HELP_PLACEMENT_2, L"Display\nhelp"  },
    { ATG::HELP_B_BUTTON,     ATG::HELP_PLACEMENT_2, L"Toggle\nfreeze" },
    { ATG::HELP_X_BUTTON,     ATG::HELP_PLACEMENT_2, L"Toggle\ndefragment" },
};
#define NUM_HELP_CALLOUTS (ARRAYSIZE(g_HelpCallouts))


//--------------------------------------------------------------------------------------
// Small helper functions
//--------------------------------------------------------------------------------------
static inline FLOAT rand_float_0_1()
{
    return ( FLOAT )rand() / ( FLOAT )( RAND_MAX + 1 );
}
template <typename type_t> static inline type_t lerp( type_t a, type_t b, FLOAT t )
{
    return a * t + b * ( 1 - t );
}


//--------------------------------------------------------------------------------------
// Name: class RecedingRect
// Desc: A utility class representing a random captured rectangle of the background 
// image, animating by shrinking towards a random point at a random rate.
//--------------------------------------------------------------------------------------
class RecedingRect
{
public:
    VOID PickAtRandom( DWORD dwWidth, DWORD dwHeight );

    IDirect3DTexture9 m_textureHeader;
    MemInterval* m_pInterval;
    DWORD m_dwTexMemSize;
    FLOAT m_fTimerInSeconds;
    FLOAT m_fLifetimeInSeconds;
    D3DRECT m_rcSnapshotArea;
    XMFLOAT2 m_vVanishingPoint;
    D3DCOLOR m_dwDebugColor;
};
typedef list <RecedingRect*>    RECTLIST;

VOID RecedingRect::PickAtRandom( DWORD dwWidth, DWORD dwHeight )
{
    // Random vanishing point
    m_vVanishingPoint.x = rand_float_0_1() * dwWidth;
    m_vVanishingPoint.y = rand_float_0_1() * dwHeight;

    // Random lifetime
    m_fLifetimeInSeconds = rand_float_0_1()
        * ( RECT_LIFETIME_MAX_IN_SEC - RECT_LIFETIME_MIN_IN_SEC )
        + RECT_LIFETIME_MIN_IN_SEC;
    m_fTimerInSeconds = m_fLifetimeInSeconds;

    // Random tile-aligned snapshot rect
    static const DWORD dwTileSize = 32;

    m_rcSnapshotArea.x1 = ( ( LONG )floorf( rand_float_0_1() * dwWidth / dwTileSize ) )
        * dwTileSize;
    m_rcSnapshotArea.y1 = ( ( LONG )floorf( rand_float_0_1() * dwHeight / dwTileSize ) )
        * dwTileSize;
    m_rcSnapshotArea.x2 = ( ( LONG )floorf( rand_float_0_1() * dwWidth / dwTileSize ) )
        * dwTileSize;
    m_rcSnapshotArea.y2 = ( ( LONG )floorf( rand_float_0_1() * dwHeight / dwTileSize ) )
        * dwTileSize;

    if( m_rcSnapshotArea.x1 == m_rcSnapshotArea.x2 )
    {
        m_rcSnapshotArea.x2 = min( m_rcSnapshotArea.x1 + dwTileSize, dwWidth );
    }
    else if( m_rcSnapshotArea.x1 > m_rcSnapshotArea.x2 )
    {
        LONG temp = m_rcSnapshotArea.x1;
        m_rcSnapshotArea.x1 = m_rcSnapshotArea.x2;
        m_rcSnapshotArea.x2 = temp;
    }

    if( m_rcSnapshotArea.y1 == m_rcSnapshotArea.y2 )
    {
        m_rcSnapshotArea.y2 = min( m_rcSnapshotArea.y1 + dwTileSize, dwHeight );
    }
    else if( m_rcSnapshotArea.y1 > m_rcSnapshotArea.y2 )
    {
        LONG temp = m_rcSnapshotArea.y1;
        m_rcSnapshotArea.y1 = m_rcSnapshotArea.y2;
        m_rcSnapshotArea.y2 = temp;
    }
}


//--------------------------------------------------------------------------------------
// Name: class Sample
// Desc: Main class to run this application. Most functionality is inherited
//       from the ATG::Application base class.
//--------------------------------------------------------------------------------------
class Sample : public ATG::Application
{
    ATG::Font m_Font;                     // Font for drawing text
    ATG::Timer m_Timer;                    // Timer
    ATG::Help m_Help;                     // Display help
    ATG::PackedResource m_xprResource;              // Packed resources (textures)
    D3DTexture* m_pBackgroundTexture;       // Background image
    ATG::PostProcess m_PostProcess;              // Post-process manager

    GpuMemPool m_memPool;                  // The memory manager

    RECTLIST m_rectList;                 // List of active snapshots
    RecedingRect m_rcNext;                   // The next snapshot to take

    BOOL m_bDrawHelp;                // Should we display help
    BOOL m_bFreeze;                  // Halt all animation, snapshots, defrags
    BOOL m_bDefragment;              // Perform defragmentation each frame

    VOID    PickNextRect();             // Choose the next snapshot rect
    VOID    DrawBackground() const;     // Draw the default background
    VOID    DrawSnapshots() const;      // Draw the animating snapshot rects
    VOID    Defragment();               // Perform defragmentation
    VOID    TakeSnapshot();             // Attempt a new snapshot

    VOID    DebugRender() const;        // Render the texture allocation graph

public:
    HRESULT Initialize();
    HRESULT Update();
    HRESULT Render();
};


//--------------------------------------------------------------------------------------
// Name: PickNextRect()
// Desc: Choose the next RecedingRect instance.
//--------------------------------------------------------------------------------------
VOID Sample::PickNextRect()
{
    m_rcNext.PickAtRandom( m_d3dpp.BackBufferWidth, m_d3dpp.BackBufferHeight );

    m_rcNext.m_dwDebugColor = MEM_DEBUG_COLORS[g_dwMemDebugColorIndex];
    g_dwMemDebugColorIndex = ( g_dwMemDebugColorIndex + 1 ) % NUM_MEM_DEBUG_COLORS;

    DWORD dwSnapshotWidth = m_rcNext.m_rcSnapshotArea.x2 - m_rcNext.m_rcSnapshotArea.x1;
    DWORD dwSnapshotHeight = m_rcNext.m_rcSnapshotArea.y2 - m_rcNext.m_rcSnapshotArea.y1;

    // Calculate required allocation size
    UINT dwBaseSize;
    XGSetTextureHeader( dwSnapshotWidth, dwSnapshotHeight, 1, 0, ( D3DFORMAT )MAKESRGBFMT( D3DFMT_A8R8G8B8 ), 0,
                        0, 0, 0, &m_rcNext.m_textureHeader, &dwBaseSize, NULL );
    m_rcNext.m_dwTexMemSize = dwBaseSize;
}


//--------------------------------------------------------------------------------------
// Name: DrawBackground()
// Desc: Draw the background image which is the original source for snapshots.
//--------------------------------------------------------------------------------------
VOID Sample::DrawBackground() const
{
    PIXBeginNamedEvent( D3DCOLOR_ARGB( 0x00, 0x80, 0x80, 0x80 ), "Draw Background" );

    D3DRECT d3drect =
    {
        0,                          // x1
        0,                          // y1
        m_d3dpp.BackBufferWidth,    // x2
        m_d3dpp.BackBufferHeight,   // y2
    };

    ATG::DebugDraw::DrawScreenSpaceTexturedRect( d3drect, m_pBackgroundTexture );

    PIXEndNamedEvent();
}


//--------------------------------------------------------------------------------------
// Name: DrawSnapshots()
// Desc: Draw the currently animating snapshot rects.
//--------------------------------------------------------------------------------------
VOID Sample::DrawSnapshots() const
{
    PIXBeginNamedEvent( D3DCOLOR_ARGB( 0x00, 0x80, 0x80, 0x80 ), "Draw Snapshots" );

    for( RECTLIST::const_iterator it = m_rectList.begin(); it != m_rectList.end(); ++it )
    {
        const RecedingRect* rect = *it;

        if( rect->m_fTimerInSeconds > 0.0f )
        {
            FLOAT fNormalizedTimer =
                rect->m_fTimerInSeconds / rect->m_fLifetimeInSeconds;

            XMFLOAT2 vRectCenter(
                0.5f * ( rect->m_rcSnapshotArea.x1 + rect->m_rcSnapshotArea.x2 ),
                0.5f * ( rect->m_rcSnapshotArea.y1 + rect->m_rcSnapshotArea.y2 )
                );
            XMFLOAT2 vRectHalfDims(
                0.5f * ( rect->m_rcSnapshotArea.x1 - rect->m_rcSnapshotArea.x2 ),
                0.5f * ( rect->m_rcSnapshotArea.y1 - rect->m_rcSnapshotArea.y2 )
                );
            XMFLOAT2 vMovingCenter(
                lerp( vRectCenter.x, rect->m_vVanishingPoint.x, fNormalizedTimer ),
                lerp( vRectCenter.y, rect->m_vVanishingPoint.y, fNormalizedTimer )
                );
            XMFLOAT2 vMovingHalfDims(
                vRectHalfDims.x * fNormalizedTimer,
                vRectHalfDims.y * fNormalizedTimer
                );

            {
                D3DRECT d3drect =
                {
                    ( LONG )( vMovingCenter.x - vMovingHalfDims.x ),   // x1
                    ( LONG )( vMovingCenter.y - vMovingHalfDims.y ),   // y1
                    ( LONG )( vMovingCenter.x + vMovingHalfDims.x ),   // x2
                    ( LONG )( vMovingCenter.y + vMovingHalfDims.y ),   // y2
                };

                ATG::DebugDraw::DrawScreenSpaceRect( d3drect, 2, rect->m_dwDebugColor );
            }

            {
                D3DRECT d3drect =
                {
                    ( LONG )( LONG )( vMovingCenter.x + vMovingHalfDims.x ),   // x1
                    ( LONG )( LONG )( vMovingCenter.y + vMovingHalfDims.y ),   // y1
                    ( LONG )( LONG )( vMovingCenter.x - vMovingHalfDims.x ),   // x2
                    ( LONG )( LONG )( vMovingCenter.y - vMovingHalfDims.y ),   // y2
                };

                ATG::DebugDraw::DrawScreenSpaceTexturedRect( d3drect, &rect->m_textureHeader );
            }
        }
    }

    PIXEndNamedEvent();
}


//--------------------------------------------------------------------------------------
// Name: Defragment()
// Desc: Defragment the snapshot memory pool to acquire more contiguous space.
//--------------------------------------------------------------------------------------
VOID Sample::Defragment()
{
    PIXBeginNamedEvent( D3DCOLOR_ARGB( 0x00, 0x80, 0x80, 0x80 ), "Defragment" );

    // Defragment the memory manager and move the memory
    BOOL bDefragged = m_memPool.Defragment();

    // Refresh the texture headers with the new defragmented addresses
    if( bDefragged )
    {
        for( RECTLIST::iterator it = m_rectList.begin(); it != m_rectList.end(); ++it )
        {
            RecedingRect* rect = *it;

            rect->m_textureHeader.Format.BaseAddress = NULL;
            // If we had mips then also "rect->m_textureHeader.Format.MipAddress = NULL"

            XGOffsetBaseTextureAddress( &rect->m_textureHeader, rect->m_pInterval->m_pStart,
                                        rect->m_pInterval->m_pStart );

            // Since we alias textures on top of a vertex buffer, and we
            // have just modified the vertex buffer using memexport,
            // all values in the texture cache are potentially stale.
            m_pd3dDevice->InvalidateResourceGpuCache( &rect->m_textureHeader, 0 );
        }
    }

    PIXEndNamedEvent();
}


//--------------------------------------------------------------------------------------
// Name: TakeSnapshot()
// Desc: Resolve a rectangle of the current image for animation.
//--------------------------------------------------------------------------------------
VOID Sample::TakeSnapshot()
{
    PIXBeginNamedEvent( D3DCOLOR_ARGB( 0x00, 0x80, 0x80, 0x80 ), "Take snapshot" );

    m_rcNext.m_pInterval = m_memPool.Allocate( m_rcNext.m_dwTexMemSize,
                                               m_rcNext.m_dwDebugColor );
    if( m_rcNext.m_pInterval != NULL )
    {
        // Snapshot success, push new rect onto list
        RecedingRect* rect = new RecedingRect;
        *rect = m_rcNext;
        m_rectList.push_back( rect );

        XGOffsetBaseTextureAddress( &rect->m_textureHeader, rect->m_pInterval->m_pStart,
                                    rect->m_pInterval->m_pStart );

        m_pd3dDevice->Resolve( D3DRESOLVE_RENDERTARGET0 | D3DRESOLVE_ALLFRAGMENTS,
                               &rect->m_rcSnapshotArea, &rect->m_textureHeader, NULL, 0, 0, NULL, 0, 0, NULL );

        PickNextRect();
    }

    PIXEndNamedEvent();
}


//--------------------------------------------------------------------------------------
// Name: DebugRender()
// Desc: Draw a bar graph showing free and committed blocks.  
//--------------------------------------------------------------------------------------
VOID Sample::DebugRender() const
{
    PIXBeginNamedEvent( D3DCOLOR_ARGB( 0x00, 0x80, 0x80, 0x80 ), "Draw Bar Graph" );

    static const LONG dwOutlineThickness = 1;

    D3DRECT rcBar =
    {
        ( LONG )( m_d3dpp.BackBufferWidth * 0.1f ),                // x1
        ( LONG )( m_d3dpp.BackBufferHeight * 0.2f ),               // y1
        m_d3dpp.BackBufferWidth - rcBar.x1,                     // x2
        rcBar.y1 + ( LONG )( m_d3dpp.BackBufferHeight * 0.05f ),   // y2
    };
    ATG::DebugDraw::DrawScreenSpaceRect( rcBar, 0, BACKGROUND_COLOR );

    // Draw the bar graph
    const MEMINTERVALLIST* pMemIntervalList = m_memPool.GetMemIntervalList();
    for( MEMINTERVALLIST::const_iterator it = pMemIntervalList->begin(); it != pMemIntervalList->end(); ++it )
    {
        const MemInterval* pInterval = *it;

        if( !pInterval->m_bFree )
        {
            DWORD dwOffset = ( ( DWORD )pInterval->m_pStart ) - ( ( DWORD )m_memPool.GetMemPoolStart() );
            FLOAT fStartLine = ( FLOAT )dwOffset / ( FLOAT )m_memPool.GetMemPoolSize();
            FLOAT fEndLine = ( FLOAT )( dwOffset + pInterval->m_dwSize )
                / ( FLOAT )m_memPool.GetMemPoolSize();

            D3DRECT rcInterval =
            {
                ( DWORD )( fStartLine * ( rcBar.x2 - rcBar.x1 ) + rcBar.x1 ) + dwOutlineThickness, // x1
                rcBar.y1 + dwOutlineThickness,                                                   // y1
                ( DWORD )( fEndLine * ( rcBar.x2 - rcBar.x1 ) + rcBar.x1 ) - dwOutlineThickness,   // x2
                rcBar.y2 - dwOutlineThickness,                                                   // y2
            };

            ATG::DebugDraw::DrawScreenSpaceRect( rcInterval, 0,
                                                 pInterval->m_dwDebugColor );
        }
    }

    // Draw the next allocation underneath the tail end of the graph, showing
    // by how much it exceeds available memory.
    {
        FLOAT fSize = ( ( FLOAT )m_rcNext.m_dwTexMemSize ) / ( ( FLOAT )m_memPool.GetMemPoolSize() );

        D3DRECT rcNext =
        {
            rcBar.x2 - ( LONG )( fSize * ( rcBar.x2 - rcBar.x1 ) ),   // x1
            rcBar.y2 + ( LONG )( 0.3f * ( rcBar.y2 - rcBar.y1 ) ),       // y1
            rcBar.x2,                                               // x2
            rcNext.y1 + rcBar.y2 - rcBar.y1,                        // y2
        };

        ATG::DebugDraw::DrawScreenSpaceRect( rcNext, ( FLOAT )dwOutlineThickness,
                                             BACKGROUND_COLOR );

        rcNext.x1 += dwOutlineThickness;
        rcNext.x2 -= dwOutlineThickness;
        rcNext.y1 += dwOutlineThickness;
        rcNext.y2 -= dwOutlineThickness;

        ATG::DebugDraw::DrawScreenSpaceRect( rcNext, 0, m_rcNext.m_dwDebugColor );
    }

    PIXEndNamedEvent();
}


//--------------------------------------------------------------------------------------
// Name: Initialize()
// Desc: Initialize app-dependent objects.
//--------------------------------------------------------------------------------------
HRESULT Sample::Initialize()
{
    // Create the font
    if( FAILED( m_Font.Create( "game:\\Media\\Fonts\\Arial_16.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Confine text drawing to the title safe area
    m_Font.SetWindow( ATG::GetTitleSafeArea() );

    // Create the help
    if( FAILED( m_Help.Create( "game:\\Media\\Help\\Help.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Create the resources
    if( FAILED( m_xprResource.Create( "game:\\Media\\Resource.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Create the background image
    m_pBackgroundTexture = m_xprResource.GetTexture( "PTC_TestImage.bmp" );
    if( m_pBackgroundTexture == NULL )
        return ATGAPPERR_MEDIANOTFOUND;

    // Initialize simple shaders.
    ATG::SimpleShaders::Initialize( NULL, NULL );

    // Initialize the post-processing effects library (for blur, bloom, etc.)
    if( FAILED( m_PostProcess.Initialize() ) )
    {
        ATG_PrintError( "Couldn't initialize the effects library\n" );
        return E_FAIL;
    }

    // Set default render states
    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, FALSE );

    m_bDrawHelp = FALSE;
    m_bFreeze = FALSE;
    m_bDefragment = TRUE;

    srand( __mftb32() );

    PickNextRect();

    // Allocate the shared memory pool 
    return m_memPool.Initialize( m_pd3dDevice, MEM_POOL_SIZE, MEM_POOL_ALIGNMENT );
}


//--------------------------------------------------------------------------------------
// Name: Update()
// Desc: Called once per frame, the call is the entry point for animating the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Update()
{
    FLOAT fElapsedTime = ( FLOAT )m_Timer.GetElapsedTime();

    // Get the current gamepad status
    ATG::GAMEPAD* pGamepad = ATG::Input::GetMergedInput();

    // Toggle help
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_BACK )
    {
        m_bDrawHelp = !m_bDrawHelp;
    }

    // Freeze
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_B )
    {
        m_bFreeze = !m_bFreeze;
    }

    // Defrag
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_X )
    {
        m_bDefragment = !m_bDefragment;
    }

    // Update ages for floating rects
    if( !m_bFreeze )
    {
        for( RECTLIST::iterator it = m_rectList.begin(); it != m_rectList.end(); )
        {
            RecedingRect* rect = *it;

            // Increment must come here, not in the 'for' statement, since we might remove 
            // the entry from the list.
            ++it;

            rect->m_fTimerInSeconds -= fElapsedTime;

            if( rect->m_fTimerInSeconds <= 0.0f )
            {
                // Before the freed memory can be accessed by the CPU, we would need to call 
                // "rect->m_textureHeader.BlockUntilNotBusy()".

                // Shouldn't call "rect->m_textureHeader.Release()" since we provided the memory.

                m_memPool.Free( rect->m_pInterval );
                m_rectList.remove( rect );
                delete rect;
            }
        }
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Render()
// Desc: Called once per frame, to render the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Render()
{
    // Draw a pattern
    DrawBackground();

    // Draw the floating rects
    DrawSnapshots();

    // Clear the texture slot, so we can safely free the texture memory later.
    m_pd3dDevice->SetTexture( 0, NULL );

    // Defragment the memory pool
    if( m_bDefragment )
    {
        Defragment();
    }

    // Take a snapshot of the current screen (minus the help text)
    TakeSnapshot();

    // Output statistics
    m_Timer.MarkFrame();

    if( m_bDrawHelp )
    {
        m_Help.Render( &m_Font, g_HelpCallouts, NUM_HELP_CALLOUTS );
    }
    else
    {
        m_Font.Begin();
        m_Font.SetScaleFactors( 1.2f, 1.2f );
        m_Font.DrawText( 0, 0, 0xffffffff, L"GPUMemoryMove" );
        m_Font.SetScaleFactors( 1.0f, 1.0f );
        m_Font.DrawText( 0, 0, 0xffffff00, m_Timer.GetFrameRate(), ATGFONT_RIGHT );
        m_Font.End();
    }

    // Draw the texture allocation graph
    DebugRender();

    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: main()
// Desc: Entry point to the program.
//--------------------------------------------------------------------------------------
VOID __cdecl main()
{
    Sample atgApp;
    ATG::GetVideoSettings( &atgApp.m_d3dpp.BackBufferWidth,
                           &atgApp.m_d3dpp.BackBufferHeight );
    atgApp.m_d3dpp.EnableAutoDepthStencil = FALSE;

    // Make sure display is gamma correct.
    atgApp.m_d3dpp.BackBufferFormat =  ( D3DFORMAT )MAKESRGBFMT( D3DFMT_A8R8G8B8 );
    atgApp.m_d3dpp.FrontBufferFormat = ( D3DFORMAT )MAKESRGBFMT( D3DFMT_LE_X8R8G8B8 );

    atgApp.Run();
}



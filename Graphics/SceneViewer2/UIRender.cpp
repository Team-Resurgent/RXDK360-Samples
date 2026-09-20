//--------------------------------------------------------------------------------------
// UIRender.cpp
//
// Contains methods for rendering the user interface, including the HUD and performance
// charts.  Also contains all of the rendering debug UI code.
// 
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include "SceneViewer2.h"
#include <xgraphics.h>

extern BOOL g_bAsyncLoadingInProgress;

VOID ComputePerfValueDelta( const D3DPERFCOUNTER_VALUES& BaseValues, D3DPERFCOUNTER_VALUES& DeltaValues )
{
    const DWORD dwCount = sizeof( D3DPERFCOUNTER_VALUES ) / sizeof( ULARGE_INTEGER );
    ULARGE_INTEGER* pDeltaInts = ( ULARGE_INTEGER* )&DeltaValues;
    const ULARGE_INTEGER* pBaseInts = ( const ULARGE_INTEGER* )&BaseValues;

    for( DWORD i = 0; i < dwCount; ++i )
    {
        pDeltaInts[i].QuadPart -= pBaseInts[i].QuadPart;
    }
}


VOID SceneViewer::RenderPerfChart()
{
#ifndef _RELEASED3D
    if( !m_bCapturePerfData )
        return;
    D3DPERFCOUNTER_VALUES Values[SVPE_SIZEOF];
    BOOL bShowValue[SVPE_SIZEOF];
    FLOAT fSectionMsec[SVPE_SIZEOF];
    for( DWORD i = 0; i < SVPE_SIZEOF; ++i )
        bShowValue[i] = GetPerfValues( i, Values[i] );
    for( DWORD i = 1; i < SVPE_SIZEOF; ++i )
    {
        ComputePerfValueDelta( Values[0], Values[i] );
    }
    const FLOAT fBarWidth = 800.0f;
    const FLOAT fBarTop = 600.0f;
    const FLOAT fBarHeight = 45.0f;
    const DOUBLE fTotalCycles = ( DOUBLE )Values[SVPE_END_RESOLVE].CP[0].QuadPart;

    FLOAT fStartPos = ( 1280.0f - fBarWidth ) * 0.5f;
    FLOAT fEndPos[SVPE_SIZEOF];
    fEndPos[0] = 0.0f;
    fSectionMsec[0] = 0.0f;
    for( DWORD i = 1; i < SVPE_SIZEOF; ++i )
    {
        if( !bShowValue[i] )
        {
            fEndPos[i] = 0;
            fSectionMsec[i] = 0;
            continue;
        }
        DOUBLE fValue = ( DOUBLE )Values[i].CP[0].QuadPart;
        FLOAT fWidth = ( FLOAT )( fValue / fTotalCycles ) * fBarWidth;
        fWidth = max( 0, fWidth );
        fWidth = min( fBarWidth, fWidth );
        fEndPos[i] = fWidth;
        fSectionMsec[i] = ( FLOAT )( fValue * 2e-6 );
    }

    const D3DCOLOR Colors[SVPE_SIZEOF] =
    {
        0xFF000000,
        0xFF404040,
        0xFF00FF00,
        0xFFFF0000,
        0xFFFFFF00,
        0xFFFF00FF,
        0xFF0000FF,
        0xFF404040,
    };

    XMFLOAT2 Origin = XMFLOAT2( fStartPos, fBarTop );
    XMFLOAT2 Size = XMFLOAT2( 0, fBarHeight );
    for( DWORD i = SVPE_SIZEOF - 1; i > 0; --i )
    {
        if( !bShowValue[i] )
            continue;
        Size.x = fEndPos[i];
        ATG::DebugDraw::DrawScreenSpaceRect( Origin, Size, 0, Colors[i] );
    }
    m_Font.Begin();
    m_Font.SetWindow( 0, 0, m_d3dpp.BackBufferWidth, m_d3dpp.BackBufferHeight );
    m_Font.SetScaleFactors( 0.6f, 0.6f );

    WCHAR strText[200];

    FLOAT fChartX = 128.0f;
    FLOAT fChartY = 200.0f;
    ULONGLONG LastValue = 0;
    for( DWORD i = 1; i < SVPE_SIZEOF; ++i )
    {
        if( !bShowValue[i] )
            continue;
        ULONGLONG CurrentValue = Values[i].MH[0].QuadPart + Values[i].MH[1].QuadPart;
        ULONGLONG CurrentValueBytes = ( CurrentValue - LastValue ) * 32;
        FLOAT fSectionTime = fSectionMsec[i] - fSectionMsec[ i - 1 ];
        FLOAT fBytesPerSecond = ( FLOAT )CurrentValueBytes / ( fSectionTime * 0.001f );
        FLOAT fMBPerSecond = fBytesPerSecond / 1048576.0f;
        swprintf_s( strText, L"%I64u bytes  %0.3f MB/sec", CurrentValueBytes, fMBPerSecond );
        LastValue = CurrentValue;
        m_Font.DrawText( fChartX + 220.0f, fChartY, 0xFFC0C0FF, strText, ATGFONT_LEFT );
        m_Font.DrawText( fChartX, fChartY, 0xFFFFFFC0, g_strPerfSections[i], ATGFONT_LEFT );
        fChartY += 20.0f;
    }

    DOUBLE fBusyCycles = ( DOUBLE )Values[SVPE_END_RESOLVE].RBBM[0].QuadPart;
    swprintf_s( strText, L"NRT busy/CP cycles: %I64u / %I64u (%0.1lf%%)",
                Values[SVPE_END_RESOLVE].RBBM[0].QuadPart,
                Values[SVPE_END_RESOLVE].CP[0].QuadPart,
                100.0 * ( fBusyCycles / fTotalCycles ) );
    m_Font.DrawText( fStartPos, fBarTop - 30.0f, 0xFFFFFFFF, strText, ATGFONT_LEFT );

    DOUBLE fPixelCycles = ( DOUBLE )Values[SVPE_END_RESOLVE].SQ[0].QuadPart;
    DOUBLE fVertexCycles = ( DOUBLE )Values[SVPE_END_RESOLVE].SQ[1].QuadPart;
    swprintf_s( strText, L"Pixel shaders busy %I64u / %I64u (%0.1lf%%)",
                Values[SVPE_END_RESOLVE].SQ[0].QuadPart,
                Values[SVPE_END_RESOLVE].RBBM[0].QuadPart,
                100.0 * ( fPixelCycles / fBusyCycles ) );
    m_Font.DrawText( fStartPos, fBarTop - 50.0f, 0xFFFFFFFF, strText, ATGFONT_LEFT );
    swprintf_s( strText, L"Vertex shaders busy %I64u / %I64u (%0.1lf%%)",
                Values[SVPE_END_RESOLVE].SQ[1].QuadPart,
                Values[SVPE_END_RESOLVE].RBBM[0].QuadPart,
                100.0 * ( fVertexCycles / fBusyCycles ) );
    m_Font.DrawText( fStartPos, fBarTop - 65.0f, 0xFFFFFFFF, strText, ATGFONT_LEFT );


    WCHAR strTime[50];
    FLOAT fYPos = fBarTop - 15.0f;
    FLOAT fDrawnXPos = 1e10f;
    for( DWORD i = SVPE_SIZEOF - 1; i > 0; --i )
    {
        if( !bShowValue[i] )
            continue;
        swprintf_s( strTime, L"%0.3f ms", fSectionMsec[i] - fSectionMsec[ i - 1 ] );
        FLOAT fXPos = fStartPos + fEndPos[i];
        FLOAT fWidth = m_Font.GetTextWidth( strTime );
        if( fXPos >= fDrawnXPos )
        {
            fYPos -= 15.0f;
        }
        else
        {
            fYPos = fBarTop - 15.0f;
        }
        fDrawnXPos = fXPos - fWidth;
        m_Font.DrawText( fStartPos + fEndPos[i], fYPos, 0xFFFFC080, strTime, ATGFONT_RIGHT );
    }
    m_Font.SetWindow( m_TitleSafeRect );
    m_Font.End();
#endif
}


VOID SceneViewer::RenderProgressBar( FLOAT fProgress, const D3DRECT& RectSafe )
{
    const FLOAT fMaxWidth = 400.0f;
    const FLOAT fMaxHeight = 30.0f;
    const FLOAT fBorderSize = 3.0f;

    FLOAT fWidth = fProgress * fMaxWidth;
    FLOAT fXPos = ( FLOAT )( RectSafe.x1 + RectSafe.x2 ) * 0.5f;
    FLOAT fYPos = ( FLOAT )( RectSafe.y2 ) - fMaxHeight * 2.0f;
    fXPos -= fMaxWidth * 0.5f;
    fYPos -= fMaxHeight * 0.5f;

    ATG::DebugDraw::DrawScreenSpaceRect( XMFLOAT2( fXPos - fBorderSize * 2,
                                                   fYPos - fBorderSize * 2 ),
                                         XMFLOAT2( fMaxWidth + fBorderSize * 4,
                                                   fMaxHeight + fBorderSize * 4 ),
                                         0, 0 );
    ATG::DebugDraw::DrawScreenSpaceRect( XMFLOAT2( fXPos - fBorderSize * 2,
                                                   fYPos - fBorderSize * 2 ),
                                         XMFLOAT2( fMaxWidth + fBorderSize * 4,
                                                   fMaxHeight + fBorderSize * 4 ),
                                         fBorderSize, 0xFFFFFFFF );
    ATG::DebugDraw::DrawScreenSpaceRect( XMFLOAT2( fXPos, fYPos ),
                                         XMFLOAT2( fWidth, fMaxHeight ),
                                         0, 0xFFFFFFFF );
}


VOID SceneViewer::RenderStagingScreen()
{
    // Render a gradient background.
    ATG::RenderBackground( 0xFF606060, 0xFF202020 );

    // Draw a message in the center of the screen.
    if( !g_bAsyncLoadingInProgress && !m_XuiApp.IsActive() )
    {
        m_Font.Begin();
        m_Font.SetScaleFactors( 1.5f, 1.5f );
        FLOAT fCenterX = ( FLOAT )( m_TitleSafeRect.x2 - m_TitleSafeRect.x1 ) * 0.5f;
        FLOAT fCenterY = ( FLOAT )( m_TitleSafeRect.y2 - m_TitleSafeRect.y1 ) * 0.5f;

        // Draw an instructional message prompting the user to load a scene.
        if( !m_SettingsPanel.IsVisible() )
            m_Font.DrawText( fCenterX, fCenterY, 0xFFFFFF00, L"Press " GLYPH_START_BUTTON L" for options.", ATGFONT_CENTER_X | ATGFONT_CENTER_Y );
        if( m_strSceneParseErrorMsg != NULL )
        {
            // Draw the last scene parse error if one exists.
            WCHAR strText[300];
            swprintf_s( strText, L"Scene parse error: %S", m_strSceneParseErrorMsg );
            m_Font.SetScaleFactors( 0.9f, 0.9f );
            FLOAT fYPos = ( FLOAT )( m_TitleSafeRect.y2 - m_TitleSafeRect.y1 - 30 );
            m_Font.DrawText( fCenterX, fYPos, 0xFFFFFFFF, strText, ATGFONT_CENTER_X | ATGFONT_CENTER_Y );
        }

        m_Font.End();
    }
}




VOID SceneViewer::RenderUI()
{
    PIXBeginNamedEvent( 0xFF0000FF, "UI" );

    // Update XUI dialogs
    m_XuiApp.Update( m_fDeltaTime );

    if( m_pScene == NULL )
    {
        // Render staging screen (blue gradient background).
        if( m_RenderMode != SVRM_DEFERRED )
            RenderStagingScreen();
    }

    BOOL bDrawUI = !m_bDisableAllUI && !m_SettingsPanel.IsVisible();

    if( bDrawUI )
    {
        // Render XUI dialogs.
        // Scale depending on the width of the render target.
        D3DXMATRIX matView;
        D3DXMatrixScaling( &matView, ( FLOAT )m_d3dpp.BackBufferWidth / 1280.0f, ( FLOAT )m_d3dpp.BackBufferHeight /
                           720.0f, 1 );
        XuiRenderSetViewTransform( m_XuiApp.GetDC(), &matView );
        m_XuiApp.Render();

        if( g_bAsyncLoadingInProgress )
        {
            // Draw a loading progress message.
            FLOAT fCenterX = ( FLOAT )( m_TitleSafeRect.x2 - m_TitleSafeRect.x1 ) * 0.5f;
            FLOAT fCenterY = ( FLOAT )( m_TitleSafeRect.y2 - m_TitleSafeRect.y1 ) * 0.75f;
            FLOAT fComplete = ( FLOAT )m_dwAsyncLoadProgress * 0.1f;
            WCHAR strText[100];
            swprintf_s( strText, L"Loading... %0.1f%% complete", fComplete );
            m_Font.Begin();
            m_Font.SetScaleFactors( 1.2f, 1.2f );
            m_Font.DrawText( fCenterX, fCenterY, 0xFFFFFF00, strText, ATGFONT_CENTER_X | ATGFONT_CENTER_Y );
            m_Font.End();

            RenderProgressBar( fComplete * 0.01f, m_TitleSafeRect );
        }

        // Draw object tweaker UI.
        m_ObjectTweaker.Render( &m_Font );

        // Show title and statistics.
        m_Font.Begin();
        m_Font.SetScaleFactors( 1.2f, 1.2f );
        m_Font.DrawText( 0, 0, 0xffffffff, L"SceneViewer 2" );
        if( m_EndTilingResult == E_OUTOFMEMORY )
        {
            m_Font.SetScaleFactors( 1.2f, 1.2f );
            FLOAT fXPos = 0.5f * ( m_TitleSafeRect.x2 - m_TitleSafeRect.x1 );
            m_Font.DrawText( fXPos, -100, 0xFFFF4040, L"Tiling Error: Out of Command Buffer Space", ATGFONT_CENTER_X );
        }
        RenderStats();
        m_BenchmarkModule.RenderReport( m_Font );
        m_Font.End();
        if( m_bDisplayPerfChart )
            RenderPerfChart();
        if( m_bDrawTilingStats )
        {
            for( DWORD i = 0; i < m_dwTilingRectCount; ++i )
            {
                ATG::DebugDraw::DrawScreenSpaceRect( m_TilingRects[i], 1.0f, 0xFF808080 );
            }
        }

        if( m_bShowDebugBuffers )
        {
            const DWORD dwImageHeight = 150;
            RenderTextureBox( m_TitleSafeRect.x1,
                              m_TitleSafeRect.y1 + 100,
                              dwImageHeight,
                              m_pDepthTexture,
                              TRUE );
            if( m_RenderMode == SVRM_DEFERRED )
            {
                RenderTextureBox( m_TitleSafeRect.x1,
                                  m_TitleSafeRect.y1 + 110 + dwImageHeight,
                                  dwImageHeight,
                                  m_pDeferredColorBuffer,
                                  FALSE );
                RenderTextureBox( m_TitleSafeRect.x1,
                                  m_TitleSafeRect.y1 + 120 + dwImageHeight * 2,
                                  dwImageHeight,
                                  m_pDeferredNormalBuffer,
                                  FALSE );
            }
        }

        if( m_iShowShadowMap >= 0 && m_iShowShadowMap < ( INT )m_ShadowMapBank.size() )
        {
            D3DTexture* pTexture = m_ShadowMapBank[m_iShowShadowMap];
            if( pTexture != NULL )
                RenderTextureBox( m_TitleSafeRect.x1, m_TitleSafeRect.y1 + 200, 256, pTexture, TRUE );
        }
    }

    m_SettingsPanel.Render();

    if( m_bDrawSafeRect )
    {
        ATG::DebugDraw::DrawScreenSpaceRect( m_TitleSafeRect, 3.0f, 0xFFFF0000 );
    }

    PIXEndNamedEvent();
}




//--------------------------------------------------------------------------------------
// Name: RenderStats()
// Desc: Draws various rendering statistics on screen.
//--------------------------------------------------------------------------------------
VOID SceneViewer::RenderStats()
{
    WCHAR strTemp[200];
    if( m_bDrawStats )
    {
        m_Font.SetScaleFactors( 1.0f * m_fTextScalingFactor, 1.0f * m_fTextScalingFactor );
        m_Font.DrawText( 0, 0, 0xffffff00, m_Timer.GetFrameRate(), ATGFONT_RIGHT );
        swprintf_s( strTemp, L"%0.3f ms", m_fDeltaTime * 1000.0f );
        m_Font.SetScaleFactors( 0.8f * m_fTextScalingFactor, 0.8f * m_fTextScalingFactor );
        m_Font.DrawText( 0, 25 * m_fTextScalingFactor, 0xffffff00, strTemp, ATGFONT_RIGHT );
    }

    FLOAT fYpos = 50.0f * m_fTextScalingFactor;
    const FLOAT fYSpacing = 15.0f * m_fTextScalingFactor;

    if( m_bDrawStats && m_pScene != NULL )
    {
        m_Font.SetScaleFactors( 0.6f * m_fTextScalingFactor, 0.6f * m_fTextScalingFactor );
        if( m_dwPrimitivesRendered > 0 )
        {
            swprintf_s( strTemp, L"%d primitives%s", m_dwPrimitivesRendered,
                        ( ( m_ZPassMode != SVZP_NONE ) ? L" w/Z pass" : L"" ) );
            m_Font.DrawText( 0, fYpos, 0x80FFFFFF, strTemp, ATGFONT_RIGHT );
            fYpos += fYSpacing;
            FLOAT fPrimsPerSecond = ( FLOAT )m_dwPrimitivesRendered / m_fDeltaTime;
            swprintf_s( strTemp, L"%0.0f primitives/sec", fPrimsPerSecond );
            m_Font.DrawText( 0, fYpos, 0x80FFFFFF, strTemp, ATGFONT_RIGHT );
            fYpos += fYSpacing;
        }
        swprintf_s( strTemp, L"Visible: %d of %d models", m_dwModelsVisible, m_dwTotalModels );
        m_Font.DrawText( 0, fYpos, 0x80FFFFFF, strTemp, ATGFONT_RIGHT );
        fYpos += fYSpacing;
        if( m_dwSubsetsRendered > 0 )
        {
            swprintf_s( strTemp, L"Rendered: %d subsets, %d models", m_dwSubsetsRendered, m_dwModelsRendered );
            m_Font.DrawText( 0, fYpos, 0x80FFFFFF, strTemp, ATGFONT_RIGHT );
            fYpos += fYSpacing;
        }
        if( m_dwModelsRendered > 0 )
        {
            swprintf_s( strTemp, L"Shadows: %d models, %d maps", m_dwModelsRenderedToShadowMaps,
                        m_dwShadowMapsRendered );
            m_Font.DrawText( 0, fYpos, 0x80FFFFFF, strTemp, ATGFONT_RIGHT );
            fYpos += fYSpacing;
        }
        if( m_bEnableLighting && m_dwModelsRendered > 0 )
        {
            swprintf_s( strTemp, L"Lights: %d active, %d influences", m_dwActiveLights, m_dwLightInfluences );
            m_Font.DrawText( 0, fYpos, 0x80FFFFFF, strTemp, ATGFONT_RIGHT );
            fYpos += fYSpacing;
            if( m_RenderMode != SVRM_DEFERRED )
            {
                FLOAT fAvgLightsPerModel = ( FLOAT )m_dwLightInfluences / ( FLOAT )m_dwModelsRendered;
                swprintf_s( strTemp, L"%0.1f avg lights per model", fAvgLightsPerModel );
                m_Font.DrawText( 0, fYpos, 0x80FFFFFF, strTemp, ATGFONT_RIGHT );
                fYpos += fYSpacing;
            }
        }
    }

    if( m_bDrawTilingStats )
    {
        m_Font.SetScaleFactors( 0.6f * m_fTextScalingFactor, 0.6f * m_fTextScalingFactor );
        for( DWORD i = 0; i < m_dwTilingRectCount; ++i )
        {
            swprintf_s( strTemp, L"[ %d %d ] [ %d %d ]",
                        m_TilingRects[i].x1, m_TilingRects[i].y1,
                        m_TilingRects[i].x2, m_TilingRects[i].y2 );
            m_Font.DrawText( 0, fYpos, 0x80FFA0FF, strTemp, ATGFONT_RIGHT );
            fYpos += fYSpacing;
        }
    }

    if( m_bDrawMemoryStats )
    {
        m_Font.SetScaleFactors( 0.6f * m_fTextScalingFactor, 0.6f * m_fTextScalingFactor );
        fYpos += ( fYSpacing * 0.5f );
        MEMORYSTATUS MemStatus;
        GlobalMemoryStatus( &MemStatus );
        swprintf_s( strTemp, L"%d / %d physical", MemStatus.dwAvailPhys, MemStatus.dwTotalPhys );
        m_Font.DrawText( 0, fYpos, 0x80FFC0A0, strTemp, ATGFONT_RIGHT );
        fYpos += fYSpacing;
        swprintf_s( strTemp, L"%d / %d virtual", MemStatus.dwAvailVirtual, MemStatus.dwTotalVirtual );
        m_Font.DrawText( 0, fYpos, 0x80FFC0A0, strTemp, ATGFONT_RIGHT );
        fYpos += fYSpacing;
    }

    if( m_pScene == NULL )
    {
        const WCHAR* strFlavor = L"";
#ifdef _DEBUG
        strFlavor = L"Debug";
#endif
#ifdef NDEBUG
#ifdef LTCG
        strFlavor = L"Release LTCG";
#else
        strFlavor = L"Release";
#endif
#endif
#ifdef PROFILE
        strFlavor = L"Profile";
#endif
        swprintf_s( strTemp, L"Build %d (%s) %S %S", _XDK_VER, strFlavor, __DATE__, __TIME__ );
        m_Font.SetScaleFactors( 0.6f, 0.6f );
        m_Font.DrawText( 0, -15.0f, 0x80808080, strTemp, ATGFONT_LEFT );
    }
}


VOID SceneViewer::RenderTextureBox( DWORD dwXPos, DWORD dwYPos, DWORD dwHeight, D3DBaseTexture* pTexture,
                                    BOOL bDepthTexture )
{
    XGTEXTURE_DESC TextureDesc;
    XGGetTextureDesc( pTexture, 0, &TextureDesc );
    D3DRECT ScreenRect;
    ScreenRect.x1 = dwXPos;
    ScreenRect.y1 = dwYPos;
    ScreenRect.y2 = ScreenRect.y1 + dwHeight;
    ScreenRect.x2 = ScreenRect.x1 + ( dwHeight * TextureDesc.Width ) / TextureDesc.Height;
    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );
    ATG::DebugDraw::DrawScreenSpaceTexturedRect( ScreenRect, pTexture, bDepthTexture );
}



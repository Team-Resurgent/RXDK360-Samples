//--------------------------------------------------------------------------------------
// TrueSkillBar.cpp
//
// GUI component for Sessions sample. Draws a bar representing skill levels given
// TrueSkill(TM) values.
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include "stdafx.h"
#include <cassert>
#include "TrueSkillBar.h"
#include "XGUI.h"

//--------------------------------------------------------------------------------------
// Name: Render()
// Desc: Renders a skill on the screen
//--------------------------------------------------------------------------------------
void TrueSkillBar::Render( LPDIRECT3DDEVICE9 pd3dDevice )
{
    // draw rectangles and lines

    // Draw black background 
    XGUI::SolidRectangle( pd3dDevice, m_fX, m_fY, m_fWidth, m_fHeight,
                          0xFF000000 );

    // Draw the potential bar
    if( m_bUseAlphaBlend )
    {
        // Use the fancy pixel shader for this
        XGUI::MuSigmaRectangle( pd3dDevice, m_fX + 1.0f, m_fY + 1.0f,
                                m_fWidth - 2.0f, m_fHeight - 2.0f,
                                D3DCOLOR_XRGB( 246, 174, 78 ),
                                D3DCOLOR_XRGB( 96, 96, 255 ),
                                D3DCOLOR_XRGB( 196, 236, 255 ),
                                ( FLOAT )m_paTrueSkill->GetMu(),
                                ( FLOAT )m_paTrueSkill->GetSigma(), 0.3f );

        // Draw two markers for the conservative and optimsitic level estimate
        XGUI::DrawLine( pd3dDevice, m_fX + 1.0f +
                        ( FLOAT )( m_paTrueSkill->GetConservativeLevel() *
                                   ( m_fWidth - 2.0f ) / 50.0f ), m_fY + 1.0f,
                        m_fX + 1.0f +
                        ( FLOAT )( m_paTrueSkill->GetConservativeLevel() *
                                   ( m_fWidth - 2.0f ) / 50.0f ), m_fY + m_fHeight - 1.0f,
                        0xFF000000 );
        XGUI::DrawLine( pd3dDevice, m_fX + 1.0f +
                        ( FLOAT )( m_paTrueSkill->GetPotentialLevel() *
                                   ( m_fWidth - 2.0f ) / 50.0f ), m_fY + 1.0f,
                        m_fX + 1.0f +
                        ( FLOAT )( m_paTrueSkill->GetPotentialLevel() *
                                   ( m_fWidth - 2.0f ) / 50.0f ), m_fY + m_fHeight - 1.0f,
                        0xFF000000 );
    }
    else
    {
        // Draw orange 'achievable' bar if required
        XGUI::SolidRectangle( pd3dDevice, m_fX + 1.0f +
                              ( FLOAT )( m_paTrueSkill->GetConservativeLevel() *
                                         ( m_fWidth - 2.0f ) / 50.0f ), m_fY + 1.0f,
                              ( FLOAT )( ( m_paTrueSkill->GetPotentialLevel() -
                                           m_paTrueSkill->GetConservativeLevel() ) *
                                         ( m_fWidth - 2.0f ) / 50.0f ), m_fHeight - 2.0f,
                              D3DCOLOR_XRGB( 246, 174, 78 ), 0.5f );

        // Draw light 'remainder' bar
        XGUI::SolidRectangle( pd3dDevice, m_fX + 1.0f +
                              ( FLOAT )( m_paTrueSkill->GetPotentialLevel() *
                                         ( m_fWidth - 2.0f ) / 50.0f ), m_fY + 1.0f,
                              ( FLOAT )( ( 50.0f - m_paTrueSkill->GetPotentialLevel() ) *
                                         ( m_fWidth - 2.0f ) / 50.0f ), m_fHeight - 2.0f,
                              D3DCOLOR_XRGB( 196, 236, 255 ), 0.5f );

        // Draw blue value bar
        XGUI::SolidRectangle( pd3dDevice, m_fX + 1.0f, m_fY + 1.0f,
                              ( FLOAT )( m_paTrueSkill->GetConservativeLevel() *
                                         ( m_fWidth - 2.0f ) / 50.0f ), m_fHeight - 2.0f,
                              D3DCOLOR_XRGB( 96, 96, 255 ), 0.5f );
    }

    // Draw conservative level 
    WCHAR szBuffer[ 8 ] = L"";
    swprintf_s( szBuffer, L"%d",
                ( INT )m_paTrueSkill->GetConservativeLevel( m_iMaxNoLevels ) );
    FLOAT fOldScaleX = m_font.m_fXScaleFactor;
    FLOAT fOldScaleY = m_font.m_fYScaleFactor;
    FLOAT fScale = ( m_fHeight - 4.0f ) / m_font.GetFontHeight();
    m_font.SetScaleFactors( fScale, fScale );
    m_font.DrawText( m_fX + 4.0f - m_font.m_rcWindow.x1, m_fY + 2.0f -
                     m_font.m_rcWindow.y1, 0xFFFFFFFF, szBuffer );
    m_font.SetScaleFactors( fOldScaleX, fOldScaleY );

    // done rendering!
    return;
}

//--------------------------------------------------------------------------------------
// Name: RenderOutcomeProbabilities()
// Desc: Renders the outcome probabilities between this skill and another skill.
//       This function assumes that the two skill bars have the same height and 
//       that this skill bar lies above the skill bar parameterised.
//--------------------------------------------------------------------------------------
void TrueSkillBar::RenderOutcomeProbabilities( LPDIRECT3DDEVICE9 pd3dDevice,
                                               TrueSkillBar* paTrueSkillBar,
                                               double dDrawProbability )
{
    // compute the outcome probabilities and generate the outcome strings
    INT iTeam1WinPercent;
    INT iTeam2WinPercent;
    INT iDrawPercent;
    GetPercentages( ComputeOutcomeProbabilities( this->GetTrueSkill(),
                                                 1, paTrueSkillBar->GetTrueSkill(), 1, dDrawProbability ),
                    iTeam1WinPercent, iTeam2WinPercent, iDrawPercent );

    WCHAR szWinProb1[ 5 ] = L"";
    swprintf_s( szWinProb1, L"%d%%", iTeam1WinPercent );
    WCHAR szWinProb2[ 5 ] = L"";
    swprintf_s( szWinProb2, L"%d%%", iTeam2WinPercent );
    WCHAR szDrawProb[ 5 ] = L"";
    swprintf_s( szDrawProb, L"%d%%", iDrawPercent );

    // work out all coordiantes
    FLOAT fDummy = 0.0f;
    FLOAT fBoxHeight = this->GetHeight();
    FLOAT fDrawBoxWidth, fWinProb1BoxWidth, fWinProb2BoxWidth;
    FLOAT fOldScaleX = m_font.m_fXScaleFactor;
    FLOAT fOldScaleY = m_font.m_fYScaleFactor;
    m_font.SetScaleFactors( ( fBoxHeight - 4.0f ) / m_font.GetFontHeight(),
                            ( fBoxHeight - 4.0f ) / m_font.GetFontHeight() );
    m_font.GetTextExtent( szWinProb1, &fWinProb1BoxWidth, &fDummy );
    m_font.GetTextExtent( szWinProb2, &fWinProb2BoxWidth, &fDummy );
    m_font.GetTextExtent( szDrawProb, &fDrawBoxWidth, &fDummy );
    fWinProb1BoxWidth += 4.0f;
    fWinProb2BoxWidth += 4.0f;
    fDrawBoxWidth += 4.0f;

    FLOAT fHorizontalLine1X = this->GetX() + this->GetWidth();
    FLOAT fHorizontalLine1Y = this->GetY() + this->GetHeight() / 2.0f;
    FLOAT fHorizontalLine2X = paTrueSkillBar->GetX() +
        paTrueSkillBar->GetWidth();
    FLOAT fHorizontalLine2Y = paTrueSkillBar->GetY() +
        paTrueSkillBar->GetHeight() / 2.0f;
    FLOAT fWinProbBoxX = this->GetWidth() / 6.0f;
    if( fHorizontalLine1X < fHorizontalLine2X )
        fWinProbBoxX += fHorizontalLine2X;
    else
        fWinProbBoxX += fHorizontalLine1X;
    FLOAT fRightLineX = fWinProbBoxX + fDrawBoxWidth / 2.0f + 4.0f;
    if( fWinProb1BoxWidth < fWinProb2BoxWidth )
        fRightLineX += fWinProb2BoxWidth;
    else
        fRightLineX += fWinProb1BoxWidth;
    FLOAT fDrawBoxX = fRightLineX - fDrawBoxWidth / 2.0f;
    FLOAT fDrawBoxY = ( fHorizontalLine1Y + fHorizontalLine2Y - fBoxHeight ) /
        2.0f;

    // draw all lines
    XGUI::DrawLine( pd3dDevice, fHorizontalLine1X, fHorizontalLine1Y,
                    fWinProbBoxX, fHorizontalLine1Y, 0xFFFFFFFF );
    XGUI::DrawLine( pd3dDevice, fHorizontalLine2X, fHorizontalLine2Y,
                    fWinProbBoxX, fHorizontalLine2Y, 0xFFFFFFFF );
    XGUI::OutlinedRectangle( pd3dDevice, fWinProbBoxX,
                             fHorizontalLine1Y - fBoxHeight / 2.0f,
                             fWinProb1BoxWidth, fBoxHeight, 0xFFFFFFFF );
    XGUI::OutlinedRectangle( pd3dDevice, fWinProbBoxX,
                             fHorizontalLine2Y - fBoxHeight / 2.0f,
                             fWinProb2BoxWidth, fBoxHeight, 0xFFFFFFFF );
    XGUI::DrawLine( pd3dDevice, fWinProbBoxX + fWinProb1BoxWidth,
                    fHorizontalLine1Y, fRightLineX, fHorizontalLine1Y,
                    0xFFFFFFFF );
    XGUI::DrawLine( pd3dDevice, fWinProbBoxX + fWinProb2BoxWidth,
                    fHorizontalLine2Y, fRightLineX, fHorizontalLine2Y,
                    0xFFFFFFFF );
    XGUI::DrawLine( pd3dDevice, fRightLineX, fHorizontalLine1Y,
                    fRightLineX, fDrawBoxY, 0xFFFFFFFF );
    XGUI::DrawLine( pd3dDevice, fRightLineX, fHorizontalLine2Y,
                    fRightLineX, fDrawBoxY + fBoxHeight, 0xFFFFFFFF );
    XGUI::OutlinedRectangle( pd3dDevice, fDrawBoxX, fDrawBoxY,
                             fDrawBoxWidth, fBoxHeight, 0xFFFFFFFF );

    // show the text
    m_font.DrawText( fWinProbBoxX + 2.0f - m_font.m_rcWindow.x1,
                     fHorizontalLine1Y - fBoxHeight / 2.0f + 2.0f -
                     m_font.m_rcWindow.y1, 0xFFFFFFFF, szWinProb1 );
    m_font.DrawText( fWinProbBoxX + 2.0f - m_font.m_rcWindow.x1,
                     fHorizontalLine2Y - fBoxHeight / 2.0f + 2.0f -
                     m_font.m_rcWindow.y1, 0xFFFFFFFF, szWinProb2 );
    m_font.DrawText( fDrawBoxX + 2.0f - m_font.m_rcWindow.x1,
                     fDrawBoxY + 2.0f - m_font.m_rcWindow.y1,
                     0xFFFFFFFF, szDrawProb );

    // restore font size
    m_font.SetScaleFactors( fOldScaleX, fOldScaleY );

    return;
}

//--------------------------------------------------------------------------------------
// Name: RenderWinningProbabilities()
// Desc: Renders the winning probabilities between each of the given skills
//--------------------------------------------------------------------------------------
void TrueSkillBar::RenderWinningProbabilities( LPDIRECT3DDEVICE9 pd3dDevice,
                                               TrueSkillBar** paTrueSkillBars,
                                               INT iNumberOfBars )
{
    // compute the winning probabilities and generate the outcome strings
    TrueSkill** pdTrueSkills = new TrueSkill* [iNumberOfBars];
    for( INT i = 0; i < iNumberOfBars; ++i )
        pdTrueSkills[ i ] = paTrueSkillBars[ i ]->GetTrueSkill();
    DOUBLE* pdWinningProbabilities = ComputeWinningProbabilities( pdTrueSkills,
                                                                  iNumberOfBars );
    INT* piWinningProbabilities = GetPercentages( pdWinningProbabilities,
                                                  iNumberOfBars );

    // work out all coordiantes
    for( INT i = 0; i < iNumberOfBars; ++i )
    {
        WCHAR szWinProb[ 5 ];
        swprintf_s( szWinProb, L"%d%%",
                    piWinningProbabilities[ i ] );

        // set new font size
        ATG::Font& aFont = paTrueSkillBars[ i ]->GetFont();
        FLOAT fBoxHeight = paTrueSkillBars[ i ]->GetHeight();
        FLOAT fOldScaleX = aFont.m_fXScaleFactor;
        FLOAT fOldScaleY = aFont.m_fYScaleFactor;
        aFont.SetScaleFactors( ( fBoxHeight - 4.0f ) / aFont.GetFontHeight(),
                               ( fBoxHeight - 4.0f ) / aFont.GetFontHeight() );

        // work out coordinates
        FLOAT fDummy = 0.0f;
        FLOAT fWinProbBoxWidth = 0.0f;
        aFont.GetTextExtent( szWinProb, &fWinProbBoxWidth, &fDummy );
        fWinProbBoxWidth += 4.0f;

        FLOAT fHorizontalLineX = paTrueSkillBars[ i ]->GetX() +
            paTrueSkillBars[ i ]->GetWidth();
        FLOAT fHorizontalLineY = paTrueSkillBars[ i ]->GetY() +
            paTrueSkillBars[ i ]->GetHeight() / 2.0f;
        FLOAT fWinProbBoxX = fHorizontalLineX + paTrueSkillBars[ i ]->GetWidth() / 6.0f;

        // draw all lines and text
        XGUI::DrawLine( pd3dDevice, fHorizontalLineX, fHorizontalLineY,
                        fWinProbBoxX, fHorizontalLineY, 0xFFFFFFFF );
        XGUI::OutlinedRectangle( pd3dDevice, fWinProbBoxX,
                                 fHorizontalLineY - fBoxHeight / 2.0f,
                                 fWinProbBoxWidth, fBoxHeight, 0xFFFFFFFF );
        aFont.DrawText( fWinProbBoxX + 2.0f - aFont.m_rcWindow.x1,
                        fHorizontalLineY - fBoxHeight / 2.0f + 2.0f -
                        aFont.m_rcWindow.y1, 0xFFFFFFFF, szWinProb );

        // restore font size
        aFont.SetScaleFactors( fOldScaleX, fOldScaleY );
    }

    // free memory
    delete[] pdTrueSkills;
    delete[] pdWinningProbabilities;
    delete[] piWinningProbabilities;

    return;
}

//--------------------------------------------------------------------------------------
// Name: GetPercentages()
// Desc: Gets the win/draw percentages for the 2 teams, ensuring that they sum
//       to 100% in the presence of rounding/truncation.
//--------------------------------------------------------------------------------------
void TrueSkillBar::GetPercentages( OutcomeProbabilities probs,
                                   INT& iTeam1, INT& iTeam2, INT& iDraw )
{
    FLOAT fTeam1 = ( FLOAT )( 100.0f * probs.GetTeam1WinProbability() );
    FLOAT fDraw = ( FLOAT )( 100.0f * probs.GetDrawProbability() );

    iDraw = ( INT )fDraw;
    iTeam1 = ( INT )( fDraw + fTeam1 ) - iDraw;
    iTeam2 = 100 - iTeam1 - iDraw;

    return;
}

//--------------------------------------------------------------------------------------
// Name: GetPercentages()
// Desc: Gets the winning percentages for a fixed number of players, ensuring that they
//       sum to 100% in the presence of rounding/truncation.
//--------------------------------------------------------------------------------------
INT* TrueSkillBar::GetPercentages( DOUBLE* pdWinningProbabilities,
                                   INT iNumberOfPlayers )
{
    INT* piWinningProbabilities = new INT[ iNumberOfPlayers ];
    DOUBLE dCumulativeWinningProbability = 0.0;
    INT iCumulativeWinningProbability = 0;

    for( INT i = 0; i < iNumberOfPlayers - 1; ++i )
    {
        dCumulativeWinningProbability += pdWinningProbabilities[ i ];
        INT iNewCumulativeWinningProbability =
            ( INT )( dCumulativeWinningProbability * 100.0 );
        piWinningProbabilities[ i ] = iNewCumulativeWinningProbability -
            iCumulativeWinningProbability;
        iCumulativeWinningProbability = iNewCumulativeWinningProbability;
    }
    piWinningProbabilities[ iNumberOfPlayers - 1 ] = 100 - iCumulativeWinningProbability;

    return piWinningProbabilities;
}

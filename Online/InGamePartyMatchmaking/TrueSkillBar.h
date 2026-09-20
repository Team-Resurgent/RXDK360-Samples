//--------------------------------------------------------------------------------------
// TrueSkillBar.h
//
// GUI component for Sessions sample. Draws a bar representing skill levels given
// TrueSkill(TM) values.
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#pragma once

#ifndef TRUESKILLBAR_H
#define TRUESKILLBAR_H

#define UNICODE

#include <xtl.h>
#include <AtgFont.h>
#include <vector>
#include "TrueSkill.h"


//-----------------------------------------------------------------------------
// Name: class TrueSkillBar
// Desc: A named of unnamed bar for displaying the skill associated with
//       a TrueSkill(TM) value.
//-----------------------------------------------------------------------------
class TrueSkillBar
{
public:

    TrueSkillBar( TrueSkill *paTrueSkill, ATG::Font &font,
                  FLOAT fX, FLOAT fY, FLOAT fWidth, FLOAT fHeight,
                  INT iMaxNoLevels=50, BOOL bUseAlphaBlending=FALSE ) :
        m_font ( font )
    {
        m_paTrueSkill = paTrueSkill;
        m_fX = fX;
        m_fY = fY;
        m_fWidth = fWidth;
        m_fHeight = fHeight;
        m_iMaxNoLevels = iMaxNoLevels;
        m_bUseAlphaBlend = bUseAlphaBlending;
    }

    ~TrueSkillBar() { };

    // standard assignment operator
    TrueSkillBar &operator=( const TrueSkillBar &aTrueSkillBar )
    {
        m_paTrueSkill = aTrueSkillBar.m_paTrueSkill;
        m_font = aTrueSkillBar.m_font;
        m_fX = aTrueSkillBar.m_fX;
        m_fY = aTrueSkillBar.m_fY;
        m_fWidth = aTrueSkillBar.m_fWidth;            
        m_fHeight = aTrueSkillBar.m_fHeight;            
        m_iMaxNoLevels = aTrueSkillBar.m_iMaxNoLevels;    
        m_bUseAlphaBlend = aTrueSkillBar.m_bUseAlphaBlend;

        return *this;
    }

    // read accessors
    FLOAT GetX( void ) { return m_fX; }
    FLOAT GetY( void ) { return m_fY; }
    FLOAT GetWidth( void ) { return m_fWidth; }
    FLOAT GetHeight( void ) { return m_fHeight; }
    TrueSkill* GetTrueSkill( void ) { return m_paTrueSkill; }
    ATG::Font &GetFont( void ) {  return m_font; }
    BOOL IsAlphaBlending ( void ) { return m_bUseAlphaBlend; }

    // write accessors
    void SetX( FLOAT fX ) { m_fX = fX; return; }
    void SetY( FLOAT fY ) { m_fY = fY; return; }
    void SetWidth( FLOAT fWidth ) { m_fWidth = fWidth; return; }
    void SetHeight( FLOAT fHeight ) { m_fHeight = fHeight; return; }
    void SetAlphaBlending( BOOL bUseAlphaBlending )
            { m_bUseAlphaBlend = bUseAlphaBlending; return; }

    // renders a skill on the screen
    void Render( LPDIRECT3DDEVICE9 pd3dDevice );

    // renders the outcome probabilities between this skill and another skill
    void RenderOutcomeProbabilities( LPDIRECT3DDEVICE9 pd3dDevice,
                                     TrueSkillBar *paTrueSkillBar,
                                     double dDrawProbability );

    // renders the winning probabilities for a set of skills
    static void RenderWinningProbabilities( LPDIRECT3DDEVICE9 pd3dDevice,
                                            TrueSkillBar** paTrueSkillBars,
                                            INT iNumberOfBars );

private:
    TrueSkill*               m_paTrueSkill;     // pointer to the TrueSkill
                                                // object to render
    ATG::Font&               m_font;            // a pointer to the font
                                                // object to use for drawing
    FLOAT                    m_fX;              // X coordinate of the bar
    FLOAT                    m_fY;              // Y coordiante of the bar
    FLOAT                    m_fWidth;          // Width of the skill bar
    FLOAT                    m_fHeight;         // Height of the skill bar
    INT                      m_iMaxNoLevels;    // Maximal number of levels
    BOOL                     m_bUseAlphaBlend;  // Use alpha blending for the
                                                // bars?

    // Get the win/draw percentages for the 2 teams, ensuring that they sum to
    // 100% in the presence of rounding/truncation.
    static void GetPercentages( OutcomeProbabilities probs,
                                INT &iTeam1, INT &iTeam2, INT &iDraw );

    // gets the win percentages for a fixed number of players, ensuring
    // that they sum to 100% in the presence of rounding/truncation.
    static INT* GetPercentages( DOUBLE* pdWinningProbabilities,
                                INT iNumberOfPlayers );
};


#endif // TRUESKILLBAR_H
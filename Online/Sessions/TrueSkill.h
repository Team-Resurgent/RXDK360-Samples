//--------------------------------------------------------------------------------------
// TrueSkill.h
//
// Client-side TrueSkill(TM) ranking system routines
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#pragma once
#ifndef TRUESKILL_H
#define TRUESKILL_H

#include "GaussianNumerics.h"


//--------------------------------------------------------------------------------------
// Name: class TrueSkill
// Desc: Class to manage the skill belief of a single player or team
//--------------------------------------------------------------------------------------
class TrueSkill
{
public:

            TrueSkill( DOUBLE dMu = 3.0, DOUBLE dSigma = 1.0, INT iMultiplicity = 1 )
            {
                m_dMu = dMu;
                m_dSigma = dSigma;
                m_iMultiplicity = iMultiplicity;
            }
            ~TrueSkill()
            {
            };

    // Read accessors
    DOUBLE  GetMu()
    {
        return m_dMu;
    }
    DOUBLE  GetSigma()
    {
        return m_dSigma;
    }
    INT     GetMultiplicity()
    {
        return m_iMultiplicity;
    }

    // Write accessors
    VOID    SetMu( DOUBLE dMu )
    {
        m_dMu = dMu; return;
    }
    VOID    SetSigma( DOUBLE dSigma )
    {
        m_dSigma = dSigma; return;
    }
    VOID    SetMultiplicity( INT iMultiplicity )
    {
        m_iMultiplicity = iMultiplicity; return;
    }

    // Compute the conservative level estimate (given a maximal number of levels)
    DOUBLE  GetConservativeLevel( INT iMaxNoLevels = 50 )
    {
        return ClipLevel( m_dMu - 3.0 * m_dSigma ) * ( ( DOUBLE )iMaxNoLevels ) / 6.0;
    }

    // Compute the pontential level estimate (given a maximal number of levels)
    DOUBLE  GetPotentialLevel( INT iMaxNoLevels = 50 )
    {
        return ClipLevel( m_dMu + 3.0 * m_dSigma ) * ( ( DOUBLE )iMaxNoLevels ) / 6.0;
    }

private:

    DOUBLE m_dMu;           // the mean skill
    DOUBLE m_dSigma;        // the uncertainty in the skill
    INT m_iMultiplicity;    // the number of players that were combined to generate
    // this TrueSkill

    // Clip a normalised level (which should be between 0.0 and 6.0)
    DOUBLE  ClipLevel( DOUBLE dLevel )
    {
        if( dLevel < 0.0 )
            return 0.0;
        else if( dLevel > 6.0 )
            return 6.0;
        else
            return dLevel;
    }

}; // class TrueSkill


//--------------------------------------------------------------------------------------
// Name: class OutcomeProbabilities
// Desc: Class to compute the outcome probabilities for a two team game
//--------------------------------------------------------------------------------------
class OutcomeProbabilities
{
public:

            OutcomeProbabilities( DOUBLE dTeam1Wins, DOUBLE dTeam2Wins, DOUBLE dDraw )
            {
                m_dTeam1Wins = dTeam1Wins;
                m_dTeam2Wins = dTeam2Wins;
                m_dDraw = dDraw;
            }
            ~OutcomeProbabilities()
            {
            };

    // create a new outcome probability object where the draw is automatically computed
            OutcomeProbabilities( DOUBLE dTeam1Wins, DOUBLE dTeam2Wins )
            {
                m_dTeam1Wins = dTeam1Wins;
                m_dTeam2Wins = dTeam2Wins;
                m_dDraw = 1.0 - dTeam1Wins - dTeam2Wins;
            }

    // assignment operator
    OutcomeProbabilities& operator=( OutcomeProbabilities& aOutcomeProbabilities )
    {
        m_dTeam1Wins = aOutcomeProbabilities.m_dTeam1Wins;
        m_dTeam2Wins = aOutcomeProbabilities.m_dTeam2Wins;
        m_dDraw = aOutcomeProbabilities.m_dDraw;

        return *this;
    }

    // read accessors
    DOUBLE  GetTeam1WinProbability()
    {
        return m_dTeam1Wins;
    }
    DOUBLE  GetTeam2WinProbability()
    {
        return m_dTeam2Wins;
    }
    DOUBLE  GetDrawProbability()
    {
        return m_dDraw;
    }

private:

    DOUBLE m_dTeam1Wins;        // probability that Team 1 wins
    DOUBLE m_dTeam2Wins;        // probability that Team 2 wins
    DOUBLE m_dDraw;             // probability that both teams draw

}; // class OutcomeProbabilities


//--------------------------------------------------------------------------------------
// Name: ComputeOutcomeProbabilities()
// Desc: Computes the outcome probabilities for a two team game
//--------------------------------------------------------------------------------------
OutcomeProbabilities ComputeOutcomeProbabilities(
    TrueSkill* aTrueSkillsTeam1, INT iNumberOfPlayersTeam1,
    TrueSkill* aTrueSkillsTeam2, INT iNumberOfPlayersTeam2,
    DOUBLE dDrawProbability );

//--------------------------------------------------------------------------------------
// Name: CombineSkills()
// Desc: Combine skills (important for team considerations)
//--------------------------------------------------------------------------------------
TrueSkill CombineSkills( TrueSkill* aTrueSkillsTeam, INT iNumberOfPlayers );

//--------------------------------------------------------------------------------------
// Name: ComputeWinningProbabilities()
// Desc: Computes the winning probabilities of each player
//--------------------------------------------------------------------------------------
DOUBLE* ComputeWinningProbabilities( TrueSkill** paTrueSkills, INT iNumberOfPlayers );

//--------------------------------------------------------------------------------------
// Name: MatchmakingHostQuality()
// Desc: Computes the quality of a session host for a querying client
//--------------------------------------------------------------------------------------
DOUBLE MatchmakingHostQuality( TrueSkill* aTrueSkillClient, TrueSkill* aTrueSkillHost );

#endif // TRUESKILL_H

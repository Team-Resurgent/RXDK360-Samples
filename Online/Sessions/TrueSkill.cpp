//--------------------------------------------------------------------------------------
// TrueSkill.cpp
//
// Client-side TrueSkill(TM) ranking system routines
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include "stdafx.h"
#include "TrueSkill.h"

// Disable warning C6211: Leaking memory <pointer> due to an exception.
// We "know" that we won't run out of memory in this sample
#pragma warning ( disable : 6211 )

// used internally in Xbox 360 Live leaderboards for the PERFORMANCE VARIATION
const DOUBLE    dBetaXbox360Live = 0.5;

// used internally in Xbox 360 Live leaderboards for the LEARNING FACTOR
const DOUBLE    dTauXbox360Live = 0.01;

//--------------------------------------------------------------------------------------
// Name: getEpsilon()
// Desc: Computes the draw margin (named Epsilon) from the draw probability
//       Returns NAN if the arguments are invalid
//--------------------------------------------------------------------------------------
inline DOUBLE getEpsilon( DOUBLE dDrawProbability, INT iTotalNumberOfPlayers,
                          DOUBLE dBeta )
{
    if( dDrawProbability < 0.0 || dDrawProbability > 1.0 )
        return log( 0.0 );
    if( iTotalNumberOfPlayers < 2 )
        return log( 0.0 );
    return( -2.0 / sqrt( ( DOUBLE )iTotalNumberOfPlayers ) * dBeta *
            PhiInverse( ( 1.0 - dDrawProbability ) / 2.0 ) );
}

//--------------------------------------------------------------------------------------
// Name: ComputeOutcomeProbabilities()
// Desc: Computes the outcome probabilities for a two team game
//       Returns a structure full of -1.0 if there is a problem
//--------------------------------------------------------------------------------------
OutcomeProbabilities ComputeOutcomeProbabilities(
                         TrueSkill* aTrueSkillsTeam1, INT iNumberOfPlayersTeam1, 
                         TrueSkill* aTrueSkillsTeam2, INT iNumberOfPlayersTeam2, 
                         DOUBLE dDrawProbability )
{
    // standard return value in case of problems
    OutcomeProbabilities aReturnValue( -1.0, -1.0, -1.0 );

    DOUBLE dBeta = dBetaXbox360Live;

    // consistency checks
    if( ( iNumberOfPlayersTeam1 < 1 ) || ( iNumberOfPlayersTeam2 < 1 ) )
        return aReturnValue;
    if( ( dDrawProbability < 0.0 ) || ( dDrawProbability >= 1.0 ) )
        return aReturnValue;

    // pre-compute the total team skills (dMu1 and dMu2) and the overall
    // uncertainity (dSigma)
    DOUBLE dSigma = 0.0, dMu1 = 0.0, dMu2 = 0.0;
    INT iN1 = 0, iN2 = 0;

    for( INT i = 0; i < iNumberOfPlayersTeam1; ++i )
    {
        dSigma += ( ( DOUBLE )aTrueSkillsTeam1[ i ].GetMultiplicity() ) *
            ( dBeta * dBeta + aTrueSkillsTeam1[ i ].GetSigma() *
              aTrueSkillsTeam1[ i ].GetSigma() );
        dMu1 += ( ( DOUBLE )aTrueSkillsTeam1[ i ].GetMultiplicity() ) *
            aTrueSkillsTeam1[ i ].GetMu();
        iN1 += aTrueSkillsTeam1 [i].GetMultiplicity();
    }

    for( INT i = 0; i < iNumberOfPlayersTeam2; ++i )
    {
        dSigma += ( ( DOUBLE )aTrueSkillsTeam1[ i ].GetMultiplicity() ) *
            ( dBeta * dBeta + aTrueSkillsTeam2[ i ].GetSigma() *
              aTrueSkillsTeam2[ i ].GetSigma() );
        dMu2 += ( ( DOUBLE )aTrueSkillsTeam1[ i ].GetMultiplicity() ) *
            aTrueSkillsTeam2[ i ].GetMu();
        iN2 += aTrueSkillsTeam1[ i ].GetMultiplicity();
    }
    dSigma = sqrt( dSigma );

    // pre-compute the draw margin as a function of the draw probability
    DOUBLE dEpsilon = getEpsilon( dDrawProbability, iN1 + iN2, dBeta ) *
        ( ( DOUBLE )( iN1 + iN2 ) ) / 2.0;

    // compute the outcome probability
    return OutcomeProbabilities( Phi( ( dMu1 - dMu2 - dEpsilon ) / dSigma ),
                                 Phi( ( dMu2 - dMu1 - dEpsilon ) / dSigma ) );
}

//--------------------------------------------------------------------------------------
// Name: CombineSkills()
// Desc: Combine skills (important for team considerations)
//       Returns a TrueSkill of all zeros if there is a problem
//--------------------------------------------------------------------------------------
TrueSkill CombineSkills( TrueSkill* aTrueSkillsTeam, INT iNumberOfPlayers )
{
    // parameter checks
    if( iNumberOfPlayers < 1 )
        return TrueSkill( 0.0, 0.0, 0 );

    // compute the average mean and variance
    DOUBLE dMu = 0.0, dSigma = 0.0;
    for( INT i = 0; i < iNumberOfPlayers; ++i )
    {
        dMu += aTrueSkillsTeam[ i ].GetMu();
        dSigma += aTrueSkillsTeam[ i ].GetSigma() * aTrueSkillsTeam[ i ].GetSigma();
    }
    dSigma = sqrt( dSigma / ( DOUBLE )iNumberOfPlayers );
    dMu /= ( DOUBLE )iNumberOfPlayers;

    // return the result
    return TrueSkill( dMu, dSigma, iNumberOfPlayers );
}

//--------------------------------------------------------------------------------------
// Name: ComputeWinningProbabilities()
// Desc: Computes the winning probabilities of each player.
//--------------------------------------------------------------------------------------
DOUBLE* ComputeWinningProbabilities( TrueSkill** paTrueSkills, INT iNumberOfPlayers )
{
    // compute the prior mean and variances for all skills
    DOUBLE* pdPriorMu = new DOUBLE[ iNumberOfPlayers ];
    DOUBLE* pdPriorSigma2 = new DOUBLE[ iNumberOfPlayers ];
    for( INT i = 0; i < iNumberOfPlayers; ++i )
    {
        pdPriorMu[ i ] = paTrueSkills[ i ]->GetMu() *
            ( DOUBLE )paTrueSkills[ i ]->GetMultiplicity();
        pdPriorSigma2[ i ] = paTrueSkills[ i ]->GetSigma() *
            paTrueSkills[ i ]->GetSigma() *
            ( DOUBLE )paTrueSkills[ i ]->GetMultiplicity();
    }
    DOUBLE dBeta2 = dBetaXbox360Live * dBetaXbox360Live;

    // prepare the output list
    DOUBLE* pdWinningProbabilities = new DOUBLE[ iNumberOfPlayers ];

    // prepare the expectation propagation algorithm (EP) in the outer loop
    INT iNoFactors = iNumberOfPlayers - 1;
    DOUBLE* pdSiteMu = new DOUBLE[ iNoFactors ];
    DOUBLE* pdSitePi = new DOUBLE[ iNoFactors ];
    DOUBLE* pdSiteS = new DOUBLE[ iNoFactors ];

    for( INT iWinnerIndex = 0; iWinnerIndex < iNumberOfPlayers; ++iWinnerIndex )
    {
        // initialise prior and site parameters
        DOUBLE dMuHat = pdPriorMu[ iWinnerIndex ];
        DOUBLE dSigmaHat = pdPriorSigma2[ iWinnerIndex ] + dBeta2;
        for( INT i = 0; i < iNoFactors; ++i )
        {
            pdSiteMu[ i ] = 0.0;
            pdSitePi[ i ] = 0.0;
        }

        // run EP
        DOUBLE dDelta = DBL_MAX;
        const DOUBLE dPrecision = 1e-6;
        const INT iMaxIterations = 128;

        for( INT iIterations = 0; ( dDelta > dPrecision ) &&
             ( iIterations < iMaxIterations ); ++iIterations )
        {
            dDelta = 0.0;
            for( INT iFactor = 0, iIndex = 0; iFactor < iNumberOfPlayers; ++iFactor )
            {
                if( iFactor != iWinnerIndex )
                {
                    // pre-computations
                    DOUBLE dMuDiff = dMuHat - pdSiteMu[ iIndex ];
                    DOUBLE dD = dSigmaHat * pdSitePi[ iIndex ];
                    DOUBLE dE = 1.0 / ( 1.0 - dD );
                    DOUBLE dPhi = dMuHat + dD * dE * dMuDiff;
                    DOUBLE dPsi = dE * dSigmaHat;

                    DOUBLE dPhi2 = dPhi - pdPriorMu[ iFactor ];
                    DOUBLE dPsi2 = dPsi + pdPriorSigma2[ iFactor ] + dBeta2;
                    DOUBLE dSPsi2 = sqrt( dPsi2 );
                    DOUBLE dZ = Phi( dPhi2 / dSPsi2 );
                    DOUBLE dAlpha = NormalDensity( dPhi2 / dSPsi2 ) /
                        ( dZ * dSPsi2 );
                    DOUBLE dAlphaPhi2Psi2 = dAlpha + dPhi2 / dPsi2;
                    DOUBLE dGamma = dAlpha * dAlphaPhi2Psi2;

                    // ADF update
                    DOUBLE dMuDelta = ( dE * ( pdSitePi[ iIndex ] * dMuDiff + dAlpha ) ) *
                        dSigmaHat;
                    DOUBLE dSigmaDelta = ( dE * dE * ( pdSitePi[ iIndex ] * ( 1.0 - dD ) -
                                                       dGamma ) ) * dSigmaHat * dSigmaHat;
                    if( abs( dMuDelta ) > dDelta )
                        dDelta = abs( dMuDelta );
                    if( abs( dSigmaDelta ) > dDelta )
                        dDelta = abs( dSigmaDelta );
                    dMuHat += dMuDelta;
                    dSigmaHat += dSigmaDelta;

                    // factor update
                    pdSitePi[ iIndex ] = dGamma / ( 1.0 - dGamma * dPsi );
                    pdSiteMu[ iIndex ] = 1.0 / dAlphaPhi2Psi2 + dPhi;
                    pdSiteS[ iIndex ] = dZ * exp( dAlpha / ( 2.0 * dAlphaPhi2Psi2 ) ) /
                        sqrt( 1.0 - dPsi * dGamma );

                    // increase the actual index
                    iIndex++;
                }
            }
        }

        // compute the winning probability
        DOUBLE dB = 0.0;
        for( INT i = 0; i < iNoFactors; ++i )
            dB += pdSitePi[ i ] * ( pdSiteMu[ i ] * pdSiteMu[ i ] );
        dB += pdPriorMu[ iWinnerIndex ] * pdPriorMu[ iWinnerIndex ] /
            ( pdPriorSigma2[ iWinnerIndex ] + dBeta2 );
        dB -= dMuHat * dMuHat / dSigmaHat;

        pdWinningProbabilities[ iWinnerIndex ] = 1.0;
        for( INT i = 0; i < iNoFactors; ++i )
            pdWinningProbabilities[ iWinnerIndex ] *= pdSiteS[ i ];
        pdWinningProbabilities[ iWinnerIndex ] *=
            sqrt( dSigmaHat / ( pdPriorSigma2[ iWinnerIndex ] + dBeta2 ) ) *
            exp( -0.5 * dB );
    }

    // re-normalise the distribution
    DOUBLE dSum = 0.0;
    for( INT i = 0; i < iNumberOfPlayers; ++i ) dSum += pdWinningProbabilities[ i ];
    for( INT i = 0; i < iNumberOfPlayers; ++i ) pdWinningProbabilities[ i ] /= dSum;

    // free all memory
    delete [] pdSiteMu;
    delete [] pdSitePi;
    delete [] pdSiteS;

    delete [] pdPriorMu;
    delete [] pdPriorSigma2;

    // return the result
    return( pdWinningProbabilities );
}

//--------------------------------------------------------------------------------------
// Name: MatchmakingHostQuality()
// Desc: Computes the quality of a session host for a querying client
//--------------------------------------------------------------------------------------
DOUBLE MatchmakingHostQuality( TrueSkill* aTrueSkillClient, TrueSkill* aTrueSkillHost )
{
    DOUBLE dClientMu = aTrueSkillClient->GetMu() *
        ( DOUBLE )aTrueSkillClient->GetMultiplicity();
    DOUBLE dClientSigma2 = aTrueSkillClient->GetSigma() *
        aTrueSkillClient->GetSigma() *
        ( DOUBLE )aTrueSkillClient->GetMultiplicity();
    DOUBLE dHostMu = aTrueSkillHost->GetMu() *
        ( DOUBLE )aTrueSkillHost->GetMultiplicity();
    DOUBLE dHostSigma2 = aTrueSkillHost->GetSigma() *
        aTrueSkillHost->GetSigma() *
        ( DOUBLE )aTrueSkillHost->GetMultiplicity();
    DOUBLE dTotalBeta2 = dBetaXbox360Live * dBetaXbox360Live *
        ( DOUBLE )( aTrueSkillClient->GetMultiplicity() +
                    aTrueSkillHost->GetMultiplicity() );

    DOUBLE dMuDifference = dClientMu - dHostMu;
    DOUBLE dClientWithPerfectHostVariance = dTotalBeta2 + dClientSigma2;
    DOUBLE dClientWithCurrentHostVariance = dClientWithPerfectHostVariance + dHostSigma2;

    return exp( -0.5 * dMuDifference * dMuDifference / dClientWithCurrentHostVariance ) *
        sqrt( dClientWithPerfectHostVariance / dClientWithCurrentHostVariance );
}

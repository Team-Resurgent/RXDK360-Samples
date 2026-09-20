//-----------------------------------------------------------------------------
// File: LEDTracker.cpp
//
// Desc: LED tracker image processing class. For clarity, the processing is 
//       done on the CPU. The GPU will potentially give a better performance.
//       The LEDs are tracked by using a mask which is XOR-ed with the
//       thresholded image. The confidence is determined from how many pixels
//       matched the LED spot from the mask. 
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------

#include <xtl.h>
#include <assert.h>
#include <AtgUtil.h>
#include "LEDTracker.h"


//-----------------------------------------------------------------------------
// Name: Initialize()
// Desc: Initializes the LED tracker. The average LED radius in pixels must
//       be supplied. The minimum LED matching confidence specifies a value
//       from 0.0 (not confident) to 1.0 (most confident) of potential LEDs
//       to be selected. The dwThresholdValue specifies a value from 0 to 255
//       for thresholding the image.
//-----------------------------------------------------------------------------
HRESULT CLEDTracker::Initialize( DWORD dwLEDRadius, FLOAT fMinConfidence, DWORD dwThresholdValue )
{
    assert( dwLEDRadius > 0 );
    assert( fMinConfidence >= 0.0f && fMinConfidence <= 1.0f );
    assert( dwThresholdValue >= 0 && dwThresholdValue <= 255 );

    m_fMinConfidence = fMinConfidence;
    m_dwThresholdValue = dwThresholdValue;
    m_dwLEDRadius = dwLEDRadius;

    // Allocate search mask
    m_dwSearchMaskWidth = dwLEDRadius * 4;
    if( m_dwSearchMaskWidth % 4 )
        m_dwSearchMaskWidth += 4;
    m_dwSearchMaskWidth &= 0xfffffffc;

    m_pSearchMask = new BYTE[m_dwSearchMaskWidth * m_dwSearchMaskWidth];
    if( !m_pSearchMask )
        return E_FAIL;

    // Generate search mask. The search mask is a black rectangle with 
    // a white circle in the center.
    ZeroMemory( m_pSearchMask, m_dwSearchMaskWidth * m_dwSearchMaskWidth );
    for( DWORD y = 0; y < m_dwSearchMaskWidth; ++y )
    {
        for( DWORD x = 0; x < m_dwSearchMaskWidth; ++x )
        {
            INT iDistanceX = x - ( m_dwSearchMaskWidth / 2 );
            INT iDistanceY = y - ( m_dwSearchMaskWidth / 2 );
            if( ( iDistanceX * iDistanceX + iDistanceY * iDistanceY ) <= ( INT )( dwLEDRadius * dwLEDRadius ) )
                m_pSearchMask[x + ( y * m_dwSearchMaskWidth )] = 255;
        }
    }

    return S_OK;
}


//-----------------------------------------------------------------------------
// Name: ProcessImage()
// Desc: Performs image processing on the frame received from the camera. The
//       image must be in the native YUY2 format. The returned coordinates
//       specify where the LEDs are located. The pdwCoordinateCount value is
//       an input/output value specifying the size of the pCoordinates array
//       and the number of coordinates returned.
//-----------------------------------------------------------------------------
VOID CLEDTracker::ProcessImage( BYTE* pFrameBuffer, DWORD dwWidth, DWORD dwHeight, LEDCoordinate* pCoordinates,
                                DWORD* pdwCoordinateCount )
{
    if( !pFrameBuffer )
        return;

    // Threshold the luminance (Y) component of the image. Also skip over
    // the color bytes. Because the YUY2 linear texture format is organized
    // such that a 16 bit pixel contains an 8 bit Y value and an 8 bit U and
    // V alternating value, looking at the most significant 8 bits will 
    // return Y.
    WORD* pwFrameBuffer = ( WORD* )pFrameBuffer;
    DWORD dwPixels = dwWidth * dwHeight;
    for( DWORD i = 0; i < dwPixels; ++i )
    {
        if( ( DWORD )( pwFrameBuffer[i] >> 8 ) > m_dwThresholdValue )
            pFrameBuffer[i] = 255;
        else
            pFrameBuffer[i] = 0;
    }

    // Run the search mask over the image to determine where LEDs are
    DWORD dwMaxCount = *pdwCoordinateCount;
    DWORD dwCount = 0;
    DWORD dwSearchMaskHalf = m_dwSearchMaskWidth / 2;

    DWORD dwConfidenceLimit = ( DWORD )( ( 1.0f - m_fMinConfidence ) * m_dwSearchMaskWidth * m_dwSearchMaskWidth *
                                         255 );

    // Loop through the full image
    for( DWORD y = 0; y < ( dwHeight - m_dwSearchMaskWidth ); ++y )
    {
        for( DWORD x = 0; x < ( dwWidth - m_dwSearchMaskWidth ); ++x )
        {
            // For performance, search only if there is a potential LED spot
            if( pFrameBuffer[x + dwSearchMaskHalf + ( y + dwSearchMaskHalf ) * dwWidth] )
            {
                // Perform mask search. Because both the image and the mask are
                // binary images (1 is represented by 255 and 0 by 0), XOR
                // is used to count how many pixels are different.
                BYTE* pImage = &pFrameBuffer[x + y * dwWidth];
                BYTE* pSearchMask = m_pSearchMask;
                DWORD dwMatchSum = 0;
                for( DWORD yy = 0; yy < m_dwSearchMaskWidth; ++yy )
                {
                    for( DWORD xx = 0; xx < m_dwSearchMaskWidth; xx += 4 )
                    {
                        dwMatchSum += pImage[xx] ^ pSearchMask[xx];
                        dwMatchSum += pImage[xx + 1] ^ pSearchMask[xx + 1];
                        dwMatchSum += pImage[xx + 2] ^ pSearchMask[xx + 2];
                        dwMatchSum += pImage[xx + 3] ^ pSearchMask[xx + 3];
                    }
                    pImage += dwWidth;
                    pSearchMask += m_dwSearchMaskWidth;
                }

                // If fewer than the confidence limit pixels are different, this is a new LED
                // coordinate. If the coordinate is farther away than twice the radius of
                // an LED, add it to the pCoordinates array. If the new coordinate is close 
                // to an existing coordinate, it is consolidated by applying an averaging filter.
                if( dwMatchSum <= dwConfidenceLimit && dwCount < dwMaxCount )
                {
                    FLOAT fLEDx = ( FLOAT )( x + dwSearchMaskHalf );
                    FLOAT fLEDy = ( FLOAT )( y + dwSearchMaskHalf );
                    FLOAT fConfidence = 1.0f - ( ( FLOAT )dwMatchSum / ( FLOAT )( m_dwSearchMaskWidth *
                                                                                  m_dwSearchMaskWidth * 255 ) );
                    FLOAT fRadius = ( FLOAT )( ( m_dwLEDRadius * 2 ) * ( m_dwLEDRadius * 2 ) );
                    BOOL bConsolidated = FALSE;

                    for( DWORD i = 0; i < dwCount; ++i )
                    {
                        FLOAT fDistx = fLEDx - pCoordinates[i].x;
                        FLOAT fDisty = fLEDy - pCoordinates[i].y;

                        if( ( fDistx * fDistx + fDisty * fDisty ) <= fRadius )
                        {
                            pCoordinates[i].x = ( pCoordinates[i].x + fLEDx ) * 0.5f;
                            pCoordinates[i].y = ( pCoordinates[i].y + fLEDy ) * 0.5f;
                            pCoordinates[i].fConfidence = ( pCoordinates[i].fConfidence + fConfidence ) * 0.5f;
                            bConsolidated = TRUE;
                        }
                    }

                    if( !bConsolidated )
                    {
                        pCoordinates[dwCount].x = fLEDx;
                        pCoordinates[dwCount].y = fLEDy;
                        pCoordinates[dwCount].fConfidence = fConfidence;
                        dwCount++;
                    }
                }
            }
        }
    }

    *pdwCoordinateCount = dwCount;
}


//-----------------------------------------------------------------------------
// File: LEDTracker.h
//
// Desc: LED tracker image processing class.
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#pragma once


struct LEDCoordinate
{
    FLOAT x;              // screen space coordinates of the LED
    FLOAT y;
    FLOAT fConfidence;    // normalized confidence of LED, 1.0 = most confident
};

//-----------------------------------------------------------------------------
// Name: class CLEDTracker
// Desc: LED tracking implementation. LED tracking is performed by processing
//       each frame and returning X,Y coordinates of LEDs.
//-----------------------------------------------------------------------------
class CLEDTracker
{
    BYTE* m_pSearchMask;          // LED search mask (match pattern), 8 bits/pixel greyscale
    DWORD m_dwSearchMaskWidth;    // LED search mask width and height in pixels

    FLOAT m_fMinConfidence;       // Minimum confidence for LED to be tracked
    DWORD m_dwThresholdValue;     // Image luminosity threshold value (0 to 255)
    DWORD m_dwLEDRadius;          // Allowed LED radius in pixels

public:

    HRESULT Initialize( DWORD dwLEDRadius, FLOAT fMinConfidence, DWORD dwThresholdValue );
    VOID    ProcessImage( BYTE* pFrameBuffer, DWORD dwWidth, DWORD dwHeight, LEDCoordinate* pCoordinates,
                          DWORD* pdwCoordinateCount );
};

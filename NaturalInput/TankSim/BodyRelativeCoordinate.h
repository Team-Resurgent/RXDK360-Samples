//--------------------------------------------------------------------------------------
// BodyReletiveCoordinate.h
//
// This class maintains a body reletive coordainte system for NUI Samples
//
// Microsoft Game Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once
#ifndef _BODY_RELETIVE_COORDINATE_SYSTEM
#define _BODY_RELETIVE_COORDINATE_SYSTEM

#include <xtl.h>
#include <xnamath.h>
#include <nuiapi.h>

// Variables and functions commented in cpp file
class BodyReletiveCoordinateSystem
{
public:
    BodyReletiveCoordinateSystem();
    ~BodyReletiveCoordinateSystem();
    VOID Update( const XMVECTOR* pSkeletonPosition );
    VOID SetUpdateRatesZeroToOne( FLOAT fCenter = 0.9f, 
        FLOAT fOrientation = 0.9f, FLOAT fScale = 0.9f,
        FLOAT fCenterByLerpBetweenShoulderAndHip = 0.33f );
    XMVECTOR GetRightHandReletive();
    XMVECTOR GetLeftHandReletive();

    XMVECTOR m_vCenter;
    XMVECTOR m_vRightHandReletive;
    XMVECTOR m_vLeftHandReletive;
    XMVECTOR m_vControlRegionScale;
    FLOAT m_fAlignSkeletonToCameraAngle;
private:
    XMFLOAT3 m_fCoordinateSystemScale;
    FLOAT m_fUpdateCenterRate;
    FLOAT m_fUpdateCenterRate2;
    FLOAT m_fUpdateOrientationRate;
    FLOAT m_fUpdateOrientationRate2;
    FLOAT m_fUpdateScaleRate;
    FLOAT m_fUpdateScaleRate2;
    FLOAT m_fCenterByLerpBetweenShoulderAndHip;
    FLOAT m_fCenterByLerpBetweenShoulderAndHip2;

};



#endif
//--------------------------------------------------------------------------------------
// SegmentationProcessingProxy.cpp
//
//
// Advanced Technology Group
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include <xtl.h>
#include <assert.h>

#include "DepthMap.h"
#include "ColorMap.h"
#include "IntensityMap.h"
#include "BinaryMap.h"
#include "CumulativeMovingAverageBuffer.h"

#include "FastDepthMap.h"
#include "FastColorMap.h"
#include "FastIntensityMap.h"
#include "FastBinaryMap.h"
#include "FastCumulativeMovingAverageBuffer.h"

#include "SegmentationProcessingProxy.h"

//--------------------------------------------------------------------------------------

ClearSegmentationProcessingProxy::ClearSegmentationProcessingProxy()
{
    m_pDepthMap = new DepthMap();
    m_pOriginalColorMap = new ColorMap();

    m_pTempColorMap = new ColorMap();
    m_pFinalImage = new ColorMap();

    m_pBinaryMap = new BinaryMap();

    m_pCumulativeMovingAverageBuffer = new CumulativeMovingAverageBuffer();

    m_pSegmentationMap = new BinaryMap();
    m_pSegmentationMapErode = new BinaryMap();
    m_pSegmentationMapDilate = new BinaryMap();

    m_pSilhouetteMap = new BinaryMap();
    m_pForegroundMap = new BinaryMap();
    m_pBackgroundMap = new BinaryMap();

    m_pCumulativeAverageIntensity = new IntensityMap();
    m_pOriginalImageIntensity = new IntensityMap();
    m_pOriginalImageEdges = new IntensityMap();
    m_pCumulativeAverageBufferEdges = new IntensityMap();
    m_pIntensitySobelDifference = new IntensityMap();
    m_bUpdateCMA = FALSE;
}

VOID ClearSegmentationProcessingProxy::CopyDepthFrame( const USHORT* pValues, const UINT stride, const UINT frame )
{
    m_pDepthMap->Fill( pValues, stride );
}

//--------------------------------------------------------------------------------------

VOID ClearSegmentationProcessingProxy::CopyColorFrame( const DWORD* pValues, const UINT stride, const UINT frame )
{
    m_pOriginalColorMap->Fill( pValues, stride );
}

//--------------------------------------------------------------------------------------

VOID ClearSegmentationProcessingProxy::DoIndependentDepthProcessing()
{
    // Depth processing
    PIXBeginNamedEvent( 0, "Segmentation mask select" );
	// Use 7 as a mask to pick up all players. Perfectly possible to put the player's tracking index instead
    m_bUpdateCMA = m_pSegmentationMap->SegmentationMaskSelect( 0x7, *m_pDepthMap );
    PIXEndNamedEvent();

    // Dilate and erode (closing) to fill holes in depth map
    PIXBeginNamedEvent( 0, "Closing" );
    m_pBinaryMap->Dilate3x3( *m_pSegmentationMap );			// m_pSegmentationMapDilate is the 3x3 dilate of m_pSegmentationMap
    m_pClosedSegmentationMap->Erode3x3( *m_pBinaryMap );	// m_pBinaryMap is the erode of the dilate, ie closing
    PIXEndNamedEvent();

    // Dilate to make a mask which contains all of possible foreground area
    PIXBeginNamedEvent( 0, "Dilate" );
    m_pBinaryMap->Dilate3x3( *m_pClosedSegmentationMap );
    m_pSegmentationMapDilate->Dilate3x3( *m_pBinaryMap );
    m_pBinaryMap->Dilate3x3( *m_pSegmentationMapDilate );
    m_pSegmentationMapDilate->Dilate3x3( *m_pBinaryMap );
    PIXEndNamedEvent();

    // Erode to make mask containing only pixels which are foreground
    PIXBeginNamedEvent( 0, "Erode" );
    m_pBinaryMap->Erode3x3( *m_pClosedSegmentationMap );
    m_pSegmentationMapErode->Erode3x3( *m_pBinaryMap );
    PIXEndNamedEvent();

    // Make mask which is background pixels only
    PIXBeginNamedEvent( 0, "Complement" );
    m_pBackgroundMap->Complement( *m_pSegmentationMapDilate );
    PIXEndNamedEvent();

    // Make mask which is foreground pixels only
    PIXBeginNamedEvent( 0, "Copy binary" );
    m_pForegroundMap->Copy( *m_pSegmentationMapErode );
    PIXEndNamedEvent();

    // Make mask which is undecided area, ie remainder
    PIXBeginNamedEvent( 0, "Xor binary" );
    m_pSilhouetteMap->Xor( *m_pSegmentationMapErode, *m_pSegmentationMapDilate );
    PIXEndNamedEvent();

}

//--------------------------------------------------------------------------------------

VOID ClearSegmentationProcessingProxy::DoIndependentColorProcessing()
{
    // Depth processing
    PIXBeginNamedEvent( 0, "Segmentation mask select" );
	// Use 7 as a mask to pick up all players. Perfectly possible to put the player's tracking index instead
    m_bUpdateCMA = m_pSegmentationMap->SegmentationMaskSelect( 0x7, *m_pDepthMap );
    PIXEndNamedEvent();

    // Dilate and erode (closing) to fill holes in depth map
    PIXBeginNamedEvent( 0, "Closing" );
    m_pBinaryMap->Dilate3x3( *m_pSegmentationMap );			// m_pSegmentationMapDilate is the 3x3 dilate of m_pSegmentationMap
    m_pClosedSegmentationMap->Erode3x3( *m_pBinaryMap );	// m_pBinaryMap is the erode of the dilate, ie closing
    PIXEndNamedEvent();

    // Dilate to make a mask which contains all of possible foreground area
    PIXBeginNamedEvent( 0, "Dilate" );
    m_pBinaryMap->Dilate3x3( *m_pClosedSegmentationMap );
    m_pSegmentationMapDilate->Dilate3x3( *m_pBinaryMap );
    m_pBinaryMap->Dilate3x3( *m_pSegmentationMapDilate );
    m_pSegmentationMapDilate->Dilate3x3( *m_pBinaryMap );
    PIXEndNamedEvent();

    // Erode to make mask containing only pixels which are foreground
    PIXBeginNamedEvent( 0, "Erode" );
    m_pBinaryMap->Erode3x3( *m_pClosedSegmentationMap );
    m_pSegmentationMapErode->Erode3x3( *m_pBinaryMap );
    PIXEndNamedEvent();

    // Make mask which is background pixels only
    PIXBeginNamedEvent( 0, "Complement" );
    m_pBackgroundMap->Complement( *m_pSegmentationMapDilate );
    PIXEndNamedEvent();

    // Make mask which is foreground pixels only
    PIXBeginNamedEvent( 0, "Copy binary" );
    m_pForegroundMap->Copy( *m_pSegmentationMapErode );
    PIXEndNamedEvent();

    // Make mask which is undecided area, ie remainder
    PIXBeginNamedEvent( 0, "Xor binary" );
    m_pSilhouetteMap->Xor( *m_pSegmentationMapErode, *m_pSegmentationMapDilate );
    PIXEndNamedEvent();
}

//--------------------------------------------------------------------------------------

VOID ClearSegmentationProcessingProxy::DoFinalJointProcessing()
{
    PIXBeginNamedEvent( 0, "Select foreground" );
    m_pFinalImage->SelectForeground( *m_pOriginalColorMap, *m_pIntensitySobelDifference,
                                     *m_pForegroundMap, *m_pSilhouetteMap,
                                     *m_pCumulativeMovingAverageBuffer, *m_pSegmentationMap );
    PIXEndNamedEvent();

    PIXBeginNamedEvent( 0, "Cumulative average" );

    if( m_bUpdateCMA )
    {
        m_pCumulativeMovingAverageBuffer->UpdateSelected( *m_pOriginalColorMap, *m_pBackgroundMap );
    }

    PIXEndNamedEvent();
}

//--------------------------------------------------------------------------------------

VOID ClearSegmentationProcessingProxy::FillDepthMap( IDirect3DTexture9* pDepthTexture, DepthDisplayType mode ) const
{
    PIXBeginNamedEvent( 0, "Copy depth map to output textures" );

    switch( mode )
    {
        case DEPTH_MAP:
            m_pDepthMap->CopyToTexture( pDepthTexture );
            break;

        case SEGMENTATION_MASK:
            m_pSegmentationMap->CopyToTexture( pDepthTexture );
            break;

        case SEGMENTATION_MASK_ERODE:
            m_pSegmentationMapErode->CopyToTexture( pDepthTexture );
            break;

        case SEGMENTATION_MASK_DILATE:
            m_pSegmentationMapDilate->CopyToTexture( pDepthTexture );
            break;

        case DEFINITE_FOREGROUND:
            m_pForegroundMap->CopyToTexture( pDepthTexture );
            break;

        case DEFINITE_BACKGROUND:
            m_pBackgroundMap->CopyToTexture( pDepthTexture );
            break;

        case EDGE_AREA:
            m_pSilhouetteMap->CopyToTexture( pDepthTexture );
            break;
    }

    PIXEndNamedEvent();
}

//--------------------------------------------------------------------------------------

VOID ClearSegmentationProcessingProxy::FillColorMap( IDirect3DTexture9* pColorTexture, ColorDisplayType colorMode,
                                                     DepthDisplayType depthMode ) const
{
    PIXBeginNamedEvent( 0, "Copy color map to output textures" );		

    switch( colorMode )
    {
        case ORIGINAL_COLOR:
            m_pOriginalColorMap->CopyToTexture( pColorTexture );	
            break;

        case SELECT_FROM_CURRENT_DEPTH:
            // Depending on the depth mode, selectively mask the color
            switch( depthMode )
            {
                case DEPTH_MAP:
                    m_pTempColorMap->Fill( RGBAValue( 0 ) );
                    break;

                case SEGMENTATION_MASK:
					// Use 7 as a mask to pick up all players. Perfectly possible to put the player's tracking index instead
                    m_pBinaryMap->SegmentationMaskSelect( 0x7, *m_pDepthMap );
                    m_pTempColorMap->SelectPixels( *m_pOriginalColorMap, *m_pBinaryMap );
                    break;

                case SEGMENTATION_MASK_ERODE:
                    m_pTempColorMap->SelectPixels( *m_pOriginalColorMap, *m_pSegmentationMapErode );
                    break;

                case SEGMENTATION_MASK_DILATE:
                    m_pTempColorMap->SelectPixels( *m_pOriginalColorMap, *m_pSegmentationMapDilate );
                    break;

                case DEFINITE_FOREGROUND:
                    m_pTempColorMap->SelectPixels( *m_pOriginalColorMap, *m_pForegroundMap );
                    break;

                case DEFINITE_BACKGROUND:
                    m_pTempColorMap->SelectPixels( *m_pOriginalColorMap, *m_pBackgroundMap );
                    break;

                case EDGE_AREA:
                    m_pTempColorMap->SelectPixels( *m_pOriginalColorMap, *m_pSilhouetteMap );
                    break;
            }
            m_pTempColorMap->CopyToTexture( pColorTexture );
            break;

        case INTENSITY_ORIGINAL:
            m_pOriginalImageIntensity->CopyToTexture( pColorTexture );
            break;

        case SOBEL_ORIGINAL:
            m_pOriginalImageEdges->CopyToTexture( pColorTexture );
            break;

        case CUMULATIVE_AVERAGE:
            m_pCumulativeMovingAverageBuffer->GetColorMap().CopyToTexture( pColorTexture );
            break;

        case SOBEL_CMA:
            m_pCumulativeAverageBufferEdges->CopyToTexture( pColorTexture );
            break;

        case SOBEL_DIFFERENCE:
            m_pIntensitySobelDifference->CopyToTexture( pColorTexture );
            break;
        case FINAL_SEGMENTATION:
            m_pFinalImage->CopyToTexture( pColorTexture );
            break;
    }
    PIXEndNamedEvent();

}

//--------------------------------------------------------------------------------------

FastSegmentationProcessingProxy::FastSegmentationProcessingProxy()
{
    m_pDepthMap = new FastDepthMap();
    m_pOriginalColorMap = new FastColorMap();

    m_pTempColorMap = new FastColorMap();
    m_pFinalImage = new FastColorMap();

    m_pBinaryMap = new FastBinaryMap();

    m_pCumulativeMovingAverageBuffer = new FastCumulativeMovingAverageBuffer();

    m_pSegmentationMap = new FastBinaryMap();
	m_pClosedSegmentationMap = new FastBinaryMap();
    m_pSegmentationMapErode = new FastBinaryMap();
    m_pSegmentationMapDilate = new FastBinaryMap();

    m_pSilhouetteMap = new FastBinaryMap();
    m_pForegroundMap = new FastBinaryMap();
    m_pBackgroundMap = new FastBinaryMap();

    m_pCumulativeAverageIntensity = new FastIntensityMap();
    m_pOriginalImageIntensity = new FastIntensityMap();
    m_pOriginalImageEdges = new FastIntensityMap();
    m_pCumulativeAverageBufferEdges = new FastIntensityMap();
    m_pIntensitySobelDifference = new FastIntensityMap();

    m_bUpdateCMA = FALSE;
}

//--------------------------------------------------------------------------------------

VOID FastSegmentationProcessingProxy::CopyDepthFrame( const USHORT* pValues, const UINT stride, const UINT frame )
{
    m_pDepthMap->Fill( pValues, stride );
}

//--------------------------------------------------------------------------------------

VOID FastSegmentationProcessingProxy::CopyColorFrame( const DWORD* pValues, const UINT stride, const UINT frame )
{
    m_pOriginalColorMap->Fill( pValues, stride );
}

//--------------------------------------------------------------------------------------

VOID FastSegmentationProcessingProxy::DoIndependentDepthProcessing()
{
    // Depth processing
    PIXBeginNamedEvent( 0, "Segmentation mask select" );
	// Use 7 as a mask to pick up all players. Perfectly possible to put the player's tracking index instead
    m_bUpdateCMA = m_pSegmentationMap->SegmentationMaskSelect( 0x7, *m_pDepthMap );
    PIXEndNamedEvent();

    // Dilate and erode (closing) to fill holes in depth map
    PIXBeginNamedEvent( 0, "Closing" );
    m_pBinaryMap->Dilate3x3( *m_pSegmentationMap );			// m_pSegmentationMapDilate is the 3x3 dilate of m_pSegmentationMap
    m_pClosedSegmentationMap->Erode3x3( *m_pBinaryMap );	// m_pBinaryMap is the erode of the dilate, ie closing
    PIXEndNamedEvent();

    // Dilate to make a mask which contains all of possible foreground area
    PIXBeginNamedEvent( 0, "Dilate" );
    m_pBinaryMap->Dilate3x3( *m_pClosedSegmentationMap );
    m_pSegmentationMapDilate->Dilate3x3( *m_pBinaryMap );
    m_pBinaryMap->Dilate3x3( *m_pSegmentationMapDilate );
    m_pSegmentationMapDilate->Dilate3x3( *m_pBinaryMap );
    PIXEndNamedEvent();

    // Erode to make mask containing only pixels which are foreground
    PIXBeginNamedEvent( 0, "Erode" );
    m_pBinaryMap->Erode3x3( *m_pClosedSegmentationMap );
    m_pSegmentationMapErode->Erode3x3( *m_pBinaryMap );
    PIXEndNamedEvent();

    // Make mask which is background pixels only
    PIXBeginNamedEvent( 0, "Complement" );
    m_pBackgroundMap->Complement( *m_pSegmentationMapDilate );
    PIXEndNamedEvent();

    // Make mask which is foreground pixels only
    PIXBeginNamedEvent( 0, "Copy binary" );
    m_pForegroundMap->Copy( *m_pSegmentationMapErode );
    PIXEndNamedEvent();

    // Make mask which is undecided area, ie remainder
    PIXBeginNamedEvent( 0, "Xor binary" );
    m_pSilhouetteMap->Xor( *m_pSegmentationMapErode, *m_pSegmentationMapDilate );
    PIXEndNamedEvent();
}

//--------------------------------------------------------------------------------------

VOID FastSegmentationProcessingProxy::DoIndependentColorProcessing()
{
    // Make greyscale of original image
    PIXBeginNamedEvent( 0, "Intensity map" );
    m_pOriginalImageIntensity->Fill( *m_pOriginalColorMap );
    PIXEndNamedEvent();

    // Sobel edge detection on greyscale
    PIXBeginNamedEvent( 0, "Original intensity map sobel" );
    m_pOriginalImageEdges->SobelEdgeDetect( *m_pOriginalImageIntensity );
    PIXEndNamedEvent();

    // Make greyscale of CMA buffer image
    PIXBeginNamedEvent( 0, "Fill map with CMA buffer" );
    m_pCumulativeAverageIntensity->Fill( m_pCumulativeMovingAverageBuffer->GetColorMap() );
    PIXEndNamedEvent();

    // Sobel edge detection on CMA buffer image
    PIXBeginNamedEvent( 0, "CMA Sobel" );
    m_pCumulativeAverageBufferEdges->SobelEdgeDetect( *m_pCumulativeAverageIntensity );
    PIXEndNamedEvent();

    // Make difference of edges
    PIXBeginNamedEvent( 0, "Intensity difference" );
    m_pIntensitySobelDifference->IntensityDifference( *m_pCumulativeAverageBufferEdges, *m_pOriginalImageEdges );
    PIXEndNamedEvent();
}

//--------------------------------------------------------------------------------------

VOID FastSegmentationProcessingProxy::DoFinalJointProcessing()
{
    PIXBeginNamedEvent( 0, "Select foreground" );
    m_pFinalImage->SelectForeground( *m_pOriginalColorMap, *m_pIntensitySobelDifference,
                                     *m_pForegroundMap, *m_pSilhouetteMap,
                                     *m_pCumulativeMovingAverageBuffer, *m_pSegmentationMap );
    PIXEndNamedEvent();

    PIXBeginNamedEvent( 0, "Cumulative average" );

    if( m_bUpdateCMA )
    {
        m_pCumulativeMovingAverageBuffer->UpdateSelected( *m_pOriginalColorMap, *m_pBackgroundMap );
    }

    PIXEndNamedEvent();
}

//--------------------------------------------------------------------------------------

VOID FastSegmentationProcessingProxy::FillDepthMap( IDirect3DTexture9* pDepthTexture, DepthDisplayType mode ) const
{
    PIXBeginNamedEvent( 0, "Copy depth map to output textures" );

    switch( mode )
    {
        case DEPTH_MAP:
            m_pDepthMap->CopyToTexture( pDepthTexture );
            break;

        case SEGMENTATION_MASK:
            m_pSegmentationMap->CopyToTexture( pDepthTexture );
            break;

        case SEGMENTATION_MASK_ERODE:
            m_pSegmentationMapErode->CopyToTexture( pDepthTexture );
            break;

        case SEGMENTATION_MASK_DILATE:
            m_pSegmentationMapDilate->CopyToTexture( pDepthTexture );
            break;

        case DEFINITE_FOREGROUND:
            m_pForegroundMap->CopyToTexture( pDepthTexture );
            break;

        case DEFINITE_BACKGROUND:
            m_pBackgroundMap->CopyToTexture( pDepthTexture );
            break;

        case EDGE_AREA:
            m_pSilhouetteMap->CopyToTexture( pDepthTexture );
            break;

    }

    PIXEndNamedEvent();
}

//--------------------------------------------------------------------------------------

VOID FastSegmentationProcessingProxy::FillColorMap( IDirect3DTexture9* pColorTexture, ColorDisplayType colorMode,
                                                    DepthDisplayType depthMode ) const
{
    PIXBeginNamedEvent( 0, "Copy color map to output textures" );		

    switch( colorMode )
    {
        case ORIGINAL_COLOR:
            m_pOriginalColorMap->CopyToTexture( pColorTexture );	
            break;

        case SELECT_FROM_CURRENT_DEPTH:
            // Depending on the depth mode, selectively mask the color
            switch( depthMode )
            {
                case DEPTH_MAP:
                    m_pTempColorMap->Fill( RGBAValue( 0 ) );
                    break;

                case SEGMENTATION_MASK:
                    m_pBinaryMap->SegmentationMaskSelect( 0x1, *m_pDepthMap );
                    m_pTempColorMap->SelectPixels( *m_pOriginalColorMap, *m_pBinaryMap );
                    break;

                case SEGMENTATION_MASK_ERODE:
                    m_pTempColorMap->SelectPixels( *m_pOriginalColorMap, *m_pSegmentationMapErode );
                    break;

                case SEGMENTATION_MASK_DILATE:
                    m_pTempColorMap->SelectPixels( *m_pOriginalColorMap, *m_pSegmentationMapDilate );
                    break;

                case DEFINITE_FOREGROUND:
                    m_pTempColorMap->SelectPixels( *m_pOriginalColorMap, *m_pForegroundMap );
                    break;

                case DEFINITE_BACKGROUND:
                    m_pTempColorMap->SelectPixels( *m_pOriginalColorMap, *m_pBackgroundMap );
                    break;

                case EDGE_AREA:
                    m_pTempColorMap->SelectPixels( *m_pOriginalColorMap, *m_pSilhouetteMap );
                    break;
            }
            m_pTempColorMap->CopyToTexture( pColorTexture );
            break;

        case INTENSITY_ORIGINAL:
            m_pOriginalImageIntensity->CopyToTexture( pColorTexture );
            break;

        case SOBEL_ORIGINAL:
            m_pOriginalImageEdges->CopyToTexture( pColorTexture );
            break;

        case CUMULATIVE_AVERAGE:
            m_pCumulativeMovingAverageBuffer->GetColorMap().CopyToTexture( pColorTexture );
            break;

        case SOBEL_CMA:
            m_pCumulativeAverageBufferEdges->CopyToTexture( pColorTexture );
            break;

        case SOBEL_DIFFERENCE:
            m_pIntensitySobelDifference->CopyToTexture( pColorTexture );
            break;
        case FINAL_SEGMENTATION:
            m_pFinalImage->CopyToTexture( pColorTexture );
            break;
    }
    PIXEndNamedEvent();
}

//--------------------------------------------------------------------------------------
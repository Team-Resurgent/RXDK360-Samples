//--------------------------------------------------------------------------------------
// SegmentationProcessingProxy.h
//
// Declares a base class for doing image segmentation work in 3 parts:
// Depth processing right after the depth frame arrives
// Color processing right after the color frame arrives 
// (the previous two will be in parallel, assuming appropriate thread assignments)
// Final processing, which only starts when the previous two are finished
//
// The class also encapsulates filling D3D textures with various color and depth
// maps for display (or use elsewhere).
//
// This class is fairly specific; its real purpose is to allow swapping between slow
// and optimized paths for segmentation, and comparing the results.
//
// Advanced Technology Group
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

#ifndef SEGMENTATION_PROCESSING_PROXY_H
#define SEGMENTATION_PROCESSING_PROXY_H

//--------------------------------------------------------------------------------------
// Forward declarations
//--------------------------------------------------------------------------------------

struct IDirect3DTexture9;

class BinaryMap;
class ColorMap;
class IntensityMap;
class DepthMap;
class CumulativeMovingAverageBuffer;

class FastBinaryMap;
class FastColorMap;
class FastIntensityMap;
class FastDepthMap;
class FastCumulativeMovingAverageBuffer;

//--------------------------------------------------------------------------------------
// Color display type enum
//--------------------------------------------------------------------------------------
enum ColorDisplayType
{
    ORIGINAL_COLOR = 0,
    SELECT_FROM_CURRENT_DEPTH,
    INTENSITY_ORIGINAL,
    SOBEL_ORIGINAL,
    CUMULATIVE_AVERAGE,
    SOBEL_CMA,
    SOBEL_DIFFERENCE,
    FINAL_SEGMENTATION,

    COLOR_DISPLAY_MAX
};

//--------------------------------------------------------------------------------------
// Color display type enum
//--------------------------------------------------------------------------------------
enum DepthDisplayType
{
    DEPTH_MAP = 0,
    SEGMENTATION_MASK,
    SEGMENTATION_MASK_ERODE,
    SEGMENTATION_MASK_DILATE,
    DEFINITE_FOREGROUND,
    DEFINITE_BACKGROUND,
    EDGE_AREA,

    DEPTH_DISPLAY_MAX
};

//--------------------------------------------------------------------------------------
// Base proxy class for handling the various bits and pieces of doing segmentation
//--------------------------------------------------------------------------------------
class SegmentationProcessingProxy
{
	public:
		SegmentationProcessingProxy()
		{
		}
		virtual BOOL	PlayersInView() const = 0;
		virtual VOID    CopyDepthFrame( const USHORT* pValues, const UINT stride, const UINT frame ) = 0;
		virtual VOID    CopyColorFrame( const DWORD* pValues, const UINT stride, const UINT frame ) = 0;

		virtual VOID    DoIndependentDepthProcessing() = 0;
		virtual VOID    DoIndependentColorProcessing() = 0;
		virtual VOID    DoFinalJointProcessing() = 0;

		virtual VOID    FillDepthMap( IDirect3DTexture9* pTexture, DepthDisplayType mode ) const = 0;
		virtual VOID    FillColorMap( IDirect3DTexture9* pTexture, ColorDisplayType mode, DepthDisplayType depthMode ) const = 0;
	private:
};

//--------------------------------------------------------------------------------------
// Slow but understandable
//--------------------------------------------------------------------------------------
class ClearSegmentationProcessingProxy : public SegmentationProcessingProxy
{
	public:
		ClearSegmentationProcessingProxy();
		virtual BOOL	PlayersInView() const
		{
			return m_bUpdateCMA;
		}
		virtual VOID    CopyDepthFrame( const USHORT* pValues, const UINT stride, const UINT frame );
		virtual VOID    CopyColorFrame( const DWORD* pValues, const UINT stride, const UINT frame );

		virtual VOID    FillDepthMap( IDirect3DTexture9* pTexture, DepthDisplayType mode ) const;
		virtual VOID    FillColorMap( IDirect3DTexture9* pTexture, ColorDisplayType mode, DepthDisplayType depthMode ) const;

		virtual VOID    DoIndependentDepthProcessing();
		virtual VOID    DoIndependentColorProcessing();
		virtual VOID    DoFinalJointProcessing();

	private:
		DepthMap* m_pDepthMap;              // Depth map from NUI
		ColorMap* m_pOriginalColorMap;      // Original RGB map from NUI

		ColorMap* m_pTempColorMap;          // Temporary map for output
		ColorMap* m_pFinalImage;            // Final segmented colour image

		BinaryMap* m_pBinaryMap;            // Temporary bit map

		// Cumulative moving average buffer for modelling background information
		CumulativeMovingAverageBuffer* m_pCumulativeMovingAverageBuffer;

		BinaryMap* m_pSegmentationMap;       // Bitmap of segmentation mask
		BinaryMap* m_pClosedSegmentationMap; // Bitmap of segmentation mask after closing
		BinaryMap* m_pSegmentationMapErode;  // Eroded segmentation mask
		BinaryMap* m_pSegmentationMapDilate; // Dilated segmentation mask

		BinaryMap* m_pSilhouetteMap;         // Mask for silhouette pixels
		BinaryMap* m_pForegroundMap;         // Mask for foreground pixels
		BinaryMap* m_pBackgroundMap;         // Mask for background pixels

		IntensityMap* m_pCumulativeAverageIntensity;    // Greyscale version of CMA
		IntensityMap* m_pOriginalImageIntensity;        // Greyscale of original RGB
		IntensityMap* m_pOriginalImageEdges;            // Sobel of original
		IntensityMap* m_pCumulativeAverageBufferEdges;  // Sobel of CMA
		IntensityMap* m_pIntensitySobelDifference;		// Intensity edge differences

		BOOL m_bUpdateCMA;
};

//--------------------------------------------------------------------------------------
// Fast but difficult to understand (at least internally to the classes)
//--------------------------------------------------------------------------------------
class FastSegmentationProcessingProxy : public SegmentationProcessingProxy
{
	public:
		FastSegmentationProcessingProxy();
		virtual BOOL	PlayersInView() const
		{
			return m_bUpdateCMA;
		}

		virtual VOID    CopyDepthFrame( const USHORT* pValues, const UINT stride, const UINT frame );
		virtual VOID    CopyColorFrame( const DWORD* pValues, const UINT stride, const UINT frame );

		virtual VOID    FillDepthMap( IDirect3DTexture9* pTexture, DepthDisplayType mode ) const;
		virtual VOID    FillColorMap( IDirect3DTexture9* pTexture, ColorDisplayType mode, DepthDisplayType depthMode ) const;

		virtual VOID    DoIndependentDepthProcessing();
		virtual VOID    DoIndependentColorProcessing();
		virtual VOID    DoFinalJointProcessing();

	private:
		FastDepthMap* m_pDepthMap;              // Depth map from NUI
		FastColorMap* m_pOriginalColorMap;      // Original RGB map from NUI

		FastColorMap* m_pTempColorMap;          // Temporary map for output
		FastColorMap* m_pFinalImage;            // Final segmented colour image

		FastBinaryMap* m_pBinaryMap;            // Temporary bit map

		// Cumulative moving average buffer for modelling background information
		FastCumulativeMovingAverageBuffer* m_pCumulativeMovingAverageBuffer;

		FastBinaryMap* m_pSegmentationMap;       // Bitmap of segmentation mask
		FastBinaryMap* m_pClosedSegmentationMap; // Bitmap of segmentation mask after closing
		FastBinaryMap* m_pSegmentationMapErode;  // Eroded segmentation mask
		FastBinaryMap* m_pSegmentationMapDilate; // Dilated segmentation mask

		FastBinaryMap* m_pSilhouetteMap;         // Mask for silhouette pixels
		FastBinaryMap* m_pForegroundMap;         // Mask for foreground pixels
		FastBinaryMap* m_pBackgroundMap;         // Mask for background pixels

		FastIntensityMap* m_pCumulativeAverageIntensity;    // Greyscale version of CMA
		FastIntensityMap* m_pOriginalImageIntensity;        // Greyscale of original RGB
		FastIntensityMap* m_pOriginalImageEdges;            // Sobel of original
		FastIntensityMap* m_pCumulativeAverageBufferEdges;  // Sobel of CMA
		FastIntensityMap* m_pIntensitySobelDifference;		// Intensity edge differences

		BOOL m_bUpdateCMA;
};

//--------------------------------------------------------------------------------------

#endif

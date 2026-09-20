//--------------------------------------------------------------------------------------
// IntensityMap.cpp
//
// Implementation of intensity fixed size map for use with NUI color images. 
// Intended to be an optimized path; currently only partially optimized.
//
// Advanced Technology Group
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include <xtl.h>
#include <assert.h>

#include "RGBAValue.h"
#include "FastIntensityMap.h"
#include "FastColorMap.h"
#include "FastBinaryMap.h"

//-------------------------------------------------------------------------------------
// Name: CopyToTexture()
// Copies grey scale data out to D3D texture
//-------------------------------------------------------------------------------------
VOID FastIntensityMap::CopyToTexture( IDirect3DTexture9* pTexture ) const
{
    assert( pTexture != NULL );
    assert( pTexture->GetLevelCount() == 1 );

    D3DSURFACE_DESC surfaceDesc;
    pTexture->GetLevelDesc( 0, &surfaceDesc );

    assert( surfaceDesc.Width == INTENSITY_MAP_WIDTH );
    assert( surfaceDesc.Height == INTENSITY_MAP_HEIGHT );

    D3DLOCKED_RECT lockRect;
    pTexture->LockRect( 0, &lockRect, NULL, 0 );

    BYTE* pPosition = ( BYTE* )lockRect.pBits;
    switch( surfaceDesc.Format )
    {
        case D3DFMT_LIN_X8R8G8B8:
        case D3DFMT_LIN_A8R8G8B8:

            for( UINT y = 0; y < INTENSITY_MAP_HEIGHT; y++ )
            {
                DWORD* pRow = ( DWORD* )pPosition;
                for( UINT x = 0; x < INTENSITY_MAP_WIDTH; x++ )
                {
                    const UINT intensity = Value( x, y );
                    pRow[x] = RGBAValue( intensity, intensity, intensity );
                }

                pPosition += lockRect.Pitch;
            }
            break;
        default:
            assert( false );     // No implementation for this texture type
            break;
    }

    pTexture->UnlockRect( 0 );
}

//-------------------------------------------------------------------------------------

static __forceinline __vector4 MakeIntensities(const __vector4 colors, const __vector4 zeroes, const __vector4 signfix, const __vector4 redMul, const __vector4 greenMul, const __vector4 blueMul)
{
	__vector4 temp0 = __vand(__vupklsb(colors),signfix);
	__vector4 temp1 = __vand(__vupkhsb(colors),signfix);

	const __vector4 colorMatrix0 = __vmrghh(zeroes,temp1);
	const __vector4 colorMatrix1 = __vmrglh(zeroes,temp1);
	const __vector4 colorMatrix2 = __vmrghh(zeroes,temp0);
	const __vector4 colorMatrix3 = __vmrglh(zeroes,temp0);

	__vector4 colorMatrixReds = __vsldoi(colorMatrix0,zeroes,4);
	colorMatrixReds = __vrlimi(colorMatrixReds,colorMatrix1,0x4,0);
	colorMatrixReds = __vrlimi(colorMatrixReds,__vsldoi(zeroes,colorMatrix2,8),0x2,1);
	colorMatrixReds = __vrlimi(colorMatrixReds,__vsldoi(zeroes,colorMatrix3,8),0x1,0);

	__vector4 colorMatrixGreens = __vsldoi(colorMatrix0,zeroes,8);
	colorMatrixGreens = __vrlimi(colorMatrixGreens,__vsldoi(colorMatrix1,zeroes,4),0x4,0);
	colorMatrixGreens = __vrlimi(colorMatrixGreens,colorMatrix2,0x2,0);
	colorMatrixGreens = __vrlimi(colorMatrixGreens,__vsldoi(zeroes,colorMatrix3,12),0x1,0);

	__vector4 colorMatrixBlues = __vsldoi(colorMatrix0,zeroes,12);
	colorMatrixBlues = __vrlimi(colorMatrixBlues,__vsldoi(colorMatrix1,zeroes,8),0x4,0);
	colorMatrixBlues = __vrlimi(colorMatrixBlues,__vsldoi(colorMatrix2,zeroes,4),0x2,0);
	colorMatrixBlues = __vrlimi(colorMatrixBlues,colorMatrix3,0x1,0);


	colorMatrixReds = __vcuxwfp(colorMatrixReds,0);
	colorMatrixGreens = __vcuxwfp(colorMatrixGreens,0);
	colorMatrixBlues = __vcuxwfp(colorMatrixBlues,0);

	colorMatrixReds = __vmulfp(colorMatrixReds,redMul);
	colorMatrixReds = __vmaddfp(colorMatrixGreens,greenMul,colorMatrixReds);
	colorMatrixReds = __vmaddfp(colorMatrixBlues,blueMul,colorMatrixReds);

	colorMatrixReds = __vcfpuxws(colorMatrixReds,0);


	__vector4 compact = __vpkswus(colorMatrixReds,zeroes);

	compact = __vpkuhum(compact,zeroes);
	return compact;
}

//--------------------------------------------------------------------------------------
// Name: Fill()
// Fills map from a ColorMap; conversion to intensity handled by RGBAValue
//-------------------------------------------------------------------------------------
VOID FastIntensityMap::Fill( const FastColorMap& other )
{
	const __vector4* __restrict readPosition = (const __vector4* __restrict)other.m_Colors;
	__vector4* __restrict writePosition = (__vector4* __restrict)m_Intensities;
	const __vector4 zeroes = __vzero();


	// Read from color map; 2560 bytes a row (640x 4 byte colors)
	// Write to intensity map 640 bytes a row
	// Do 4 vector4s at a time

	__vector4 signfix;
	signfix.u[0] = signfix.u[1] = signfix.u[2] = signfix.u[3] =	0x00ff00ff;

	__vector4 redMul;
	redMul.v[0] = redMul.v[1] = redMul.v[2] = redMul.v[3] = 0.2989f;
	__vector4 greenMul;
	greenMul.v[0] = greenMul.v[1] = greenMul.v[2] = greenMul.v[3] = 0.587f;
	__vector4 blueMul;
	blueMul.v[0] = blueMul.v[1] = blueMul.v[2] = blueMul.v[3] = 0.114f;

	for (int y = 0; y < INTENSITY_MAP_HEIGHT; y++)
	{
		// 40 groups of 4 vector 4s, 
		for (int loop = 0; loop < INTENSITY_MAP_WIDTH/16; loop++)
		{
			__vector4 colors0 = readPosition[0];
			__vector4 colors1 = readPosition[1];
			__vector4 colors2 = readPosition[2];
			__vector4 colors3 = readPosition[3];

			__vector4 colorWord0 = MakeIntensities( colors0, zeroes, signfix, redMul, greenMul, blueMul);
			__vector4 colorWord1 = MakeIntensities( colors1, zeroes, signfix, redMul, greenMul, blueMul);
			__vector4 colorWord2 = MakeIntensities( colors2, zeroes, signfix, redMul, greenMul, blueMul);
			__vector4 colorWord3 = MakeIntensities( colors3, zeroes, signfix, redMul, greenMul, blueMul);

			__vector4 output = colorWord0;
			output = __vor( colorWord0, __vsldoi( zeroes, colorWord1, 12 ) );
			output = __vor( output, __vsldoi( zeroes, colorWord2, 8) );
			output = __vor( output, __vsldoi( zeroes, colorWord3, 4) );

			*writePosition = output;
			readPosition += 4;
			writePosition += 1;
		}
	}

}

//-------------------------------------------------------------------------------------

VOID FastIntensityMap::Fill( const FastColorMap& other, const FastBinaryMap& selectMap )
{
    for( UINT y = 0; y < INTENSITY_MAP_HEIGHT; y++ )
    {
        for( UINT x = 0; x < INTENSITY_MAP_WIDTH; x++ )
		{
			if (selectMap.Value( x >> 1, y >> 1 ) > 0 )
			{
	            const RGBAValue color = other.Value( x, y );
				SetValue( x, y, ( BYTE )color.GetIntensity() );
			}
		}
    }
}

//--------------------------------------------------------------------------------------
// Name: Fill()
// Fills map from an array of BYTE values with an optional stride
//-------------------------------------------------------------------------------------
VOID FastIntensityMap::Fill( const BYTE* pValues, const UINT stride )
{
    if( stride == INTENSITY_MAP_STRIDE )
    {
        // Memcpy in one go.
        XMemCpy( m_Intensities, pValues, ( INTENSITY_MAP_HEIGHT * INTENSITY_MAP_WIDTH ) );
    }
    else
    {
        // Memcpy row by row as we have a stride bigger than our width, ie junk on the end of rows
        const BYTE* pPosition = pValues;
        for( UINT y = 0; y < INTENSITY_MAP_HEIGHT; y++ )
        {
            XMemCpy( m_Intensities[y], pPosition, INTENSITY_MAP_WIDTH );
            pPosition += stride;
        }
    }
}

//--------------------------------------------------------------------------------------
// Name: Sobel()
// Carries out Sobel operator on input map and sets this map to results
//-------------------------------------------------------------------------------------
VOID FastIntensityMap::SobelEdgeDetect( const FastIntensityMap& inputMap )
{
    // Start 1 pixel in, because we know the kernel will fit inside the map
    // and thus dump a load of error checking.
    for( UINT y = 1; y < INTENSITY_MAP_HEIGHT - 1; y++ )
    {
        for( UINT x = 1; x < INTENSITY_MAP_WIDTH - 1; x++ )
        {
            // Cache required intensities.
            const UINT xmym = inputMap.Value( x - 1, y - 1 );
            const UINT x0ym = inputMap.Value( x + 0, y - 1 );
            const UINT xpym = inputMap.Value( x + 1, y - 1 );

            const UINT xmy0 = inputMap.Value( x - 1, y + 0 );
            const UINT xpy0 = inputMap.Value( x + 1, y + 0 );

            const UINT xmyp = inputMap.Value( x - 1, y + 1 );
            const UINT x0yp = inputMap.Value( x + 0, y + 1 );
            const UINT xpyp = inputMap.Value( x + 1, y + 1 );

            // Make horizontal and vertical detection sums
            INT hsum = 0;
            INT vsum = 0;

            hsum += xmym * -1;
            hsum += x0ym * -2;
            hsum += xpym * -1;
            hsum += xmyp * 1;
            hsum += x0yp * 2;
            hsum += xpyp * 1;

            vsum += xmym * -1;
            vsum += xmy0 * -2;
            vsum += xmyp * -1;
            vsum += xpym * 1;
            vsum += xpy0 * 2;
            vsum += xpyp * 1;

            // Sum abs values and clamp
            UINT intensity = min( abs( hsum ) + abs( vsum ), UCHAR_MAX );
            SetValue( x, y, intensity );
        }
    }
}

//--------------------------------------------------------------------------------------
// Name: Sobel()
// Carries out Sobel operator on input map and sets this map to results
// Only returns edge detector results in pixels indicated by the BinaryMap selectMap
//-------------------------------------------------------------------------------------
VOID FastIntensityMap::SobelEdgeDetect( const FastIntensityMap& inputMap, const FastBinaryMap& selectMap )
{
    for( UINT y = 1; y < INTENSITY_MAP_HEIGHT - 1; y++ )
    {
        for( UINT x = 1; x < INTENSITY_MAP_WIDTH - 1; x++ )
        {
            // Apply Sobel operator on pixels selected only
            if( selectMap.Value( x >> 1, y >> 1 ) > 0 )
            {
                const INT xmym = inputMap.Value( x - 1, y - 1 );
                const INT x0ym = inputMap.Value( x + 0, y - 1 );
                const INT xpym = inputMap.Value( x + 1, y - 1 );

                const INT xmy0 = inputMap.Value( x - 1, y + 0 );
                const INT xpy0 = inputMap.Value( x + 1, y + 0 );

                const INT xmyp = inputMap.Value( x - 1, y + 1 );
                const INT x0yp = inputMap.Value( x + 0, y + 1 );
                const INT xpyp = inputMap.Value( x + 1, y + 1 );

                INT hsum = 0;
                INT vsum = 0;

                hsum += xmym * -1;
                hsum += x0ym * -2;
                hsum += xpym * -1;
                hsum += xmyp * 1;
                hsum += x0yp * 2;
                hsum += xpyp * 1;

                vsum += xmym * -1;
                vsum += xmy0 * -2;
                vsum += xmyp * -1;
                vsum += xpym * 1;
                vsum += xpy0 * 2;
                vsum += xpyp * 1;

                UINT intensity = abs( hsum ) + abs( vsum );
                intensity = min( intensity, 255 );

                SetValue( x, y, intensity );
            }
			else
			{
				SetValue(x,y,255);
			}
        }
    }
}

//--------------------------------------------------------------------------------------
// Name: Difference()
// Sets this map to the abs difference between this and the input map.
// Values are thresholded. (This method should perhaps belong to BinaryMap)
//-------------------------------------------------------------------------------------

VOID FastIntensityMap::IntensityDifference( const FastIntensityMap& inputMap0, const FastIntensityMap& inputMap1 )
{
	__vector4* __restrict writePosition = (__vector4* __restrict)m_Intensities;
	__vector4* __restrict readPosition0 = (__vector4* __restrict)inputMap0.m_Intensities;
	__vector4* __restrict readPosition1 = (__vector4* __restrict)inputMap1.m_Intensities;


	__vector4 threshold;
	threshold.u[0] = threshold.u[1] = threshold.u[2] = threshold.u[3] = 0x3c3c3c3c; 

	__vector4 bgIntensity = __vzero();			// black
	__vector4 fgIntensity;
	fgIntensity.u[0] = fgIntensity.u[1] = fgIntensity.u[2] = fgIntensity.u[3] = 0x7f7f7f7f;

	for (int y = 0; y < INTENSITY_MAP_HEIGHT; y++)
	{
		// 64 intensities at a time, ie 4 __vector4
		for (int loop = 0; loop < INTENSITY_MAP_WIDTH/64; loop++)
		{
			const __vector4 intensities0_0 = readPosition0[0];
			const __vector4 intensities0_1 = readPosition0[1];
			const __vector4 intensities0_2 = readPosition0[2];
			const __vector4 intensities0_3 = readPosition0[3];

			const __vector4 intensities1_0 = readPosition1[0];
			const __vector4 intensities1_1 = readPosition1[1];
			const __vector4 intensities1_2 = readPosition1[2];
			const __vector4 intensities1_3 = readPosition1[3];

			__vector4 max, min,diff;

			max = __vmaxub(intensities0_0,intensities1_0);
			min = __vminub(intensities0_0,intensities1_0);
			diff = __vsububs(max,min);
			diff = __vcmpgtub(diff,threshold);
			writePosition[0] = __vsel(bgIntensity,fgIntensity,diff);

			max = __vmaxub(intensities0_1,intensities1_1);
			min = __vminub(intensities0_1,intensities1_1);
			diff = __vsububs(max,min);
			diff = __vcmpgtub(diff,threshold);
			writePosition[1] = __vsel(bgIntensity,fgIntensity,diff);

			max = __vmaxub(intensities0_2,intensities1_2);
			min = __vminub(intensities0_2,intensities1_2);
			diff = __vsububs(max,min);
			diff = __vcmpgtub(diff,threshold);
			writePosition[2] = __vsel(bgIntensity,fgIntensity,diff);

			max = __vmaxub(intensities0_3,intensities1_3);
			min = __vminub(intensities0_3,intensities1_3);
			diff = __vsububs(max,min);
			diff = __vcmpgtub(diff,threshold);
			writePosition[3] = __vsel(bgIntensity,fgIntensity,diff);

			readPosition0+=4;
			readPosition1+=4;
			writePosition+=4;
		}
	}
}

//--------------------------------------------------------------------------------------
// FastCPU.cpp
//
// This sample shows how to use the math library, and how to measure its performance.
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include <xtl.h>
#include <assert.h>
#include <algorithm>
#include "AtgConsole.h"
#include "AtgUtil.h"
#include "AtgInput.h"
#include "Skinning.h"
#include "XboxHardwareTimer.h"
#include "ppcintrinsics.h"

// Disable NULL dereference warnings because my memory allocations will not fail.
// Dereferencing NULL pointer 'pSkinInfo', Dereferencing NULL pointer 'pInVerts'
#pragma warning(disable : 6011)

//--------------------------------------------------------------------------------------
// Globals
//--------------------------------------------------------------------------------------

// Set if there is a verification error - if there is a verification error (bad
// results) then no timing information is displayed.
BOOL                g_bVerificationError = FALSE;
XboxHardwareTimer*  g_pXboxTimer = NULL;

// Console for output
ATG::Console        g_Console;

// CPU frequency
__int64             g_frequency;

const INT           NUM_VERTS = 1000;
const INT           NUM_NORMALS_PER_VERT = 1;
const INT           NUM_PALETTE_MATRICES = 50;
// If the SkinInfo structure is the same size as the vertices being processed
// then they will stay aligned relative to each other as vertices are processed,
// causing cache line contention. Offseting the skin info by one cache line will
// help performance in this case.
//#define OFFSET_SKIN_INFO
#ifdef  OFFSET_SKIN_INFO
    const INT SKIN_OFFSET = 128;
#else
const INT           SKIN_OFFSET = 0;
#endif

// Typedef for the skinning functions.
typedef VOID (*SkinningFunction )( const XMFLOAT3* pXMInData,
                                   DWORD dwNumVerts,
                                   const SkinInfo* pSkinInfo,
                                   const XMMATRIX* pPalette,
                                   DWORD dwNumPaletteMatrices, bool bPreCacheMatrices,
                                   XMFLOAT3* pXMOutData );

// Structure to describe a skinning function.
struct SkinFunctionInfo
{
    SkinningFunction pSkinningFunction;
    const CHAR* pDescription;
};

// A list of all of the skinning functions being tested.
SkinFunctionInfo SkinningFunctions[] =
{
    { &SkinC, "SkinC" },
    { &SkinVMX1, "SkinVMX1" },
    { &SkinVMX2, "SkinVMX2" },
    { &SkinVMX3, "SkinVMX3" },
    { 0, "SkinFloat16" },
};

const INT           NUM_SKINNING_FUNCTIONS = ( sizeof( SkinningFunctions ) / sizeof( SkinningFunctions[0] ) );


//--------------------------------------------------------------------------------------
// Name: PrintLine
// Desc: Output a line to the console and to the debugger output window.
//--------------------------------------------------------------------------------------
VOID __cdecl PrintLine( const WCHAR* strFormat, ... )
{
    va_list pArgList;
    va_start( pArgList, strFormat );

    WCHAR buffer[1000];
    wvsprintfW( buffer, strFormat, pArgList );
    wcscat_s( buffer, L"\n" );
    OutputDebugStringW( buffer );

    g_Console.Format( L"%s", buffer );

    va_end( pArgList );
}


//--------------------------------------------------------------------------------------
// Name: CopyFloatsToHalfs
// Desc: This function copies FLOAT elements from pSrc to pDest, converting
//       them to HALF (FLOAT16) as it goes. It is designed for simplicity
//       not performance. It copies one element at a time to avoid requiring
//       that the source and destination be 16-byte aligned.
//       pSrc and pDest must both be naturally aligned.
//--------------------------------------------------------------------------------------
void CopyFloatsToHalfs( HALF* pDest, CONST FLOAT* pSrc, UINT count )
 {
    for( UINT i = 0; i < count; ++i )
 {
    // Use __lvlx to load a FLOAT. __lvlx is handy because it can load data
    // that isn't 16-byte aligned, as long as it doesn't cross a 16-byte
    // boundary.
        __vector4 input = __lvlx( pSrc + i, 0 );

    // We only need to pack one FLOAT16, but VMX128 only supports
    // packing two or four. This instruction packs four floats, then shifts the
    // result over two words so that the first word of input goes into the first
    // half-word of result.
        __vector4 result = __vpkd3d( input, input, VPACK_FLOAT16_4, VPACK_64LO, 2 );

    // Splat element zero to all elements, preparing for storing
        result = __vsplth( result, 0 );

    // Store a half-word (16-bit) element to the destination buffer.
        __stvehx( result, pDest + i, 0 );
    }
}


//--------------------------------------------------------------------------------------
// Name: CopyHalfsToFloats
// Desc: This function copies HALF (FLOAT16) elements from pSrc to pDest,
//       converting them to FLOAT as it goes. It is designed for simplicity
//       not performance. It copies one element at a time to avoid requiring
//       that the source and destination be 16-byte aligned.
//       pSrc and pDest must both be naturally aligned.
//--------------------------------------------------------------------------------------
VOID CopyHalfsToFloats( FLOAT* pDest, CONST HALF* pSrc, UINT count )
 {
    for( UINT i = 0; i < count; ++i )
 {
    // Use __lvlx to load a HALF. __lvlx is handy because it can load data
    // that isn't 16-byte aligned, as long as it doesn't cross a 16-byte
    // boundary.
        __vector4 input = __lvlx( pSrc + i, 0 );

    // Shift the loaded data into words 2 and 3, because vupkd3d unpacks
    // the high-numbered (least significant) bits.
        input = __vsldoi( input, input, 8 );

    // We only need to unpack one FLOAT16, but VMX128 only supports
    // unpacking two or four.
        __vector4 result = __vupkd3d( input, VPACK_FLOAT16_4 );

    // Splat element zero to all elements, preparing for storing
        result = __vspltw( result, 0 );

    // Store a float element to the destination buffer.
        __stvewx( result, pDest + i, 0 );
    }
}


//--------------------------------------------------------------------------------------
// Name: ClearCaches
// Desc: This function gets the caches into a known state, with no useful data in them.
//       It doesn't clear the instruction caches, but the data caches should be
//       thoroughly flushed.
//--------------------------------------------------------------------------------------
// Turn off all optimizations so that the useless code below won't be optimized away.
#pragma optimize("", off)
void ClearCaches()
{
    const size_t MemSize = 2000000;
    char* pMemory = new char[MemSize];

    // Zero the newly allocated memory - this gets as much of it as will
    // fit into the L2 cache. This step isn't strictly necessary, but it avoids
    // any potential warnings about reading from uninitialized memory.
    XMemSet( pMemory, 0, MemSize );

    // Now loop through, reading from every byte. This pulls the data into the
    // L1 cache - as much of it as will fit.
    DWORD gSum = 0;
    for( int i = 0; i < MemSize; ++i )
    {
        gSum += pMemory[i];
    }

    // Now flush the data out of the caches. This should leave the L1 and L2
    // caches virtually empty.
    for( int i = 0; i < MemSize; i += 128 )
        __dcbf( i, pMemory );

    delete [] pMemory;
}
// Restore optimizations.
#pragma optimize("", on)


//--------------------------------------------------------------------------------------
// Name: SkinningTest
// Desc: tests CPU and assembly skinning with verification
//--------------------------------------------------------------------------------------
VOID SkinningTest()
{
    // create input verts
    CONST DWORD float3Count = NUM_VERTS * ( 1 + NUM_NORMALS_PER_VERT );
    CONST DWORD floatCount = float3Count * 3;
    XMFLOAT3* pInVerts =
        ( XMFLOAT3* )VirtualAlloc( NULL, floatCount * sizeof( FLOAT ),
                                   MEM_COMMIT | MEM_NOZERO, PAGE_READWRITE );

    // generate random verts and normals
    for( UINT i = 0; i < float3Count; i++ )
    {
        pInVerts[i].x = FLOAT( rand() ) / FLOAT( RAND_MAX ) * 1000.0f;
        pInVerts[i].y = FLOAT( rand() ) / FLOAT( RAND_MAX ) * 1000.0f;
        pInVerts[i].z = FLOAT( rand() ) / FLOAT( RAND_MAX ) * 1000.0f;
    }

    // In some cases full float precision is not needed for vertices. FLOAT16, also known as HALF,
    // gives enough range and precision for many models. Using HALF instead of FLOAT cuts memory
    // usage, memory bandwidth usage, and cache usage in half, which may improve overall
    // performance. As long as the final transformation to world space is done by the GPU at full
    // FLOAT precision it should be possible to use HALF variables for many vertices.
    // The D3D pack/unpack instructions (__vpkd3d and __vupkd3d) can be used to efficiently
    // convert to and from HALF format.
    HALF* pInVertsHalf = ( HALF* )VirtualAlloc( NULL, floatCount * sizeof( HALF ),
                                                MEM_COMMIT | MEM_NOZERO, PAGE_READWRITE );

    // Copy the FLOAT data to a FLOAT16 buffer.
    CopyFloatsToHalfs( pInVertsHalf, ( FLOAT* )pInVerts, floatCount );

    // create skininfo structures (one per vertex), allocating SKIN_OFFSET extra bytes
    // to allow for alignment adjustments.
    SkinInfo* pSkinInfoBase =
        ( SkinInfo* )VirtualAlloc( NULL, NUM_VERTS * sizeof( SkinInfo ) + SKIN_OFFSET,
                                   MEM_COMMIT | MEM_NOZERO, PAGE_READWRITE );
    // Possibly adjust the pointer to avoid L1 cache contention.
    SkinInfo* pSkinInfo = ( SkinInfo* )( ( ( BYTE* )pSkinInfoBase ) + SKIN_OFFSET );

    // generate random skinning data
    UINT RepeatCount = 0;
    for( UINT i = 0; i < NUM_VERTS; ++i )
    {
        if( RepeatCount > 0 )
        {
            // Reuse the previous set of weights.
            for( UINT j = 0; j < 4; ++j )
            {
                pSkinInfo[i].Indices[j] = pSkinInfo[i - 1].Indices[j];
                pSkinInfo[i].Weights[j] = pSkinInfo[i - 1].Weights[j];
            }

            --RepeatCount;
        }
        else
        {
            FLOAT Weights[4];

            // Repeat each combination of weights 10-20 times. This reflects the fact
            // that the same set of matrices are used for multiple vertices.
            RepeatCount = 10 + rand() % 10;
            // Start out with vertices affected by just one bone, and then increase the
            // number of bones.
            UINT NumWeights = 1 + i / ( NUM_VERTS / 4 );
            UINT j;
            for( j = 0; j < NumWeights; ++j )
            {
                int index = rand() % NUM_PALETTE_MATRICES;
                // If an index of zero appears in a slot other than the first one then
                // this indicates (for some versions of the skinning code) that there are
                // no more palettes affecting this vector. To avoid accidentally indicating
                // this we change palette indices of zero to one. A skinned animation export
                // tool would need to rearrange the palette indices so that zero always
                // came first.
                if( index == 0 && j != 0 )
                    ++index;
                pSkinInfo[i].Indices[j] = ( BYTE )index;
                Weights[j] = 1.0f / NumWeights;
            }

            // Set unused indices and weights to zero - this allows code to transform
            // using these matrices if avoiding branches is desireable.
            // Since the 0 is also used to indicate to some versions that there are no
            // more bones it is important that matrix 0 always go in slot 0 if it is
            // genuinely wanted.
            for(; j < 4; ++j )
            {
                pSkinInfo[i].Indices[j] = 0;
                Weights[j] = 0;
            }

            // Pack weights into BYTEs.  We could use XMStoreUByteN4 here, 
            // except that rounding might cause the total weight to deviate from 1.
            FLOAT WeightTotalFloat = 0.0f;
            BYTE WeightTotalByte = 0;
            for( j = 0; j < 4; ++j )
            {
                WeightTotalFloat += Weights[j];
                BYTE PackedTotal = ( BYTE )( 255.0f * WeightTotalFloat + 0.5f );
                pSkinInfo[i].Weights[j] = PackedTotal - WeightTotalByte;
                WeightTotalByte = PackedTotal;
            }
            assert( WeightTotalByte == 255 );
        }
    }


    // create matrix palette
    XMMATRIX* pPalette =
        ( XMMATRIX* )VirtualAlloc( NULL,
                                   NUM_PALETTE_MATRICES * sizeof( XMMATRIX ),
                                   MEM_COMMIT | MEM_NOZERO, PAGE_READWRITE );
    // matrix palette must be 16 byte aligned
    assert( ( DWORD( pPalette ) & 0xF ) == 0 );

    // generate random palette matrices
    for( UINT i = 0; i < NUM_PALETTE_MATRICES; i++ )
    {
        // Do all of the initialization calculations on an XMVECTOR, then store it
        // as an XMFLOAT3.
        XMVECTOR RotVec =
        {
            FLOAT( rand() ) / FLOAT( RAND_MAX / 2 ) - 1.0f,
            FLOAT( rand() ) / FLOAT( RAND_MAX / 2 ) - 1.0f,
            FLOAT( rand() ) / FLOAT( RAND_MAX / 2 ) - 1.0f, 0.0f
        };

        RotVec = XMVector4Normalize( RotVec );

        FLOAT RotRad = FLOAT( rand() / FLOAT( RAND_MAX ) * 2 * XM_PI );

        XMVECTOR Rot = XMQuaternionRotationAxis( RotVec, RotRad );

        XMVECTOR Pos =
        {
            FLOAT( rand() ) / FLOAT( RAND_MAX / 2 ) - 1.0f,
            FLOAT( rand() ) / FLOAT( RAND_MAX / 2 ) - 1.0f,
            FLOAT( rand() ) / FLOAT( RAND_MAX / 2 ) - 1.0f, 0.0f
        };

        XMVECTOR Origin =
        {
            0.0f, 0.0f, 0.0f, 0.0f
        };
        XMVECTOR Scaling =
        {
            1.0f, 1.0f, 1.0f, 1.0f
        };
        pPalette[i] = XMMatrixAffineTransformation( Scaling, Origin, Rot, Pos );
    }


    // Run each skinning test for two types of destination memory.
    for( INT MemoryType = 0; MemoryType < 2; ++MemoryType )
    {
        // create output verts in cacheable or in uncached write combining memory
        DWORD Flags = PAGE_READWRITE;
        if( MemoryType == 0 )
            Flags |= PAGE_WRITECOMBINE;
        XMFLOAT3* pOutVerts =
            ( XMFLOAT3* )XPhysicalAlloc( float3Count *
                                         sizeof( XMFLOAT3 ),
                                         MAXULONG_PTR, 0,
                                         Flags );

        // Run each test
        for( INT test = 0; test < NUM_SKINNING_FUNCTIONS; ++test )
        {
            ClearCaches();
            g_pXboxTimer->StartTimer( SkinningFunctions[test].pDescription, MemoryType );
            if( SkinningFunctions[test].pSkinningFunction )
            {
                SkinningFunctions[test].pSkinningFunction( pInVerts, NUM_VERTS,
                                                           pSkinInfo,
                                                           pPalette, NUM_PALETTE_MATRICES, true,
                                                           pOutVerts );
            }
            else
            {
                // Run the FLOAT16 function manually because it takes
                // different inputs.
                SkinFloat16( ( XMFLOAT3* )pInVertsHalf, NUM_VERTS,
                             pSkinInfo,
                             pPalette, NUM_PALETTE_MATRICES, true,
                             pOutVerts );
            }
            g_pXboxTimer->StopTimer( SkinningFunctions[test].pDescription, MemoryType, false );
        }

        XPhysicalFree( pOutVerts );
    }

    XMFLOAT3* pOutVerts =
        ( XMFLOAT3* )XPhysicalAlloc( float3Count *
                                     sizeof( XMFLOAT3 ),
                                     MAXULONG_PTR, 0,
                                     PAGE_READWRITE );

    //
    // Verification
    //
    XMFLOAT3* pTestVertsC = new XMFLOAT3[float3Count];
    XMFLOAT3* pTestVertsASM = new XMFLOAT3[float3Count];
    ZeroMemory( pTestVertsC, float3Count * sizeof( XMFLOAT3 ) );
    ZeroMemory( pTestVertsASM, float3Count * sizeof( XMFLOAT3 ) );

    // Do the skinning using the C function.
    ZeroMemory( pOutVerts, float3Count * sizeof( XMFLOAT3 ) );
    SkinC( pInVerts, NUM_VERTS,
           pSkinInfo,
           pPalette, NUM_PALETTE_MATRICES, true,
           pOutVerts );
    memcpy( pTestVertsC, pOutVerts,
            float3Count * sizeof( XMFLOAT3 ) );

    // Now do the skinning with each of the other functions and verify that the results are
    // close to identical.
    for( INT test = 1; test < NUM_SKINNING_FUNCTIONS; ++test )
    {
        ZeroMemory( pOutVerts, float3Count * sizeof( XMFLOAT3 ) );

        FLOAT maxErrorRatio = 0.01f;     // Normal error tolerance is 1%

        if( SkinningFunctions[test].pSkinningFunction )
        {
            SkinningFunctions[test].pSkinningFunction( pInVerts, NUM_VERTS,
                                                       pSkinInfo,
                                                       pPalette, NUM_PALETTE_MATRICES, true,
                                                       pOutVerts );
            // Copy the results to a different buffer.
            memcpy( pTestVertsASM, pOutVerts,
                    float3Count * sizeof( XMFLOAT3 ) );
        }
        else
        {
            // Run the FLOAT16 function manually because it takes
            // different inputs.
            SkinFloat16( ( XMFLOAT3* )pInVertsHalf, NUM_VERTS,
                         pSkinInfo,
                         pPalette, NUM_PALETTE_MATRICES, true,
                         pOutVerts );
            // Copy and convert the results to a different buffer.
            CopyHalfsToFloats( ( FLOAT* )pTestVertsASM, ( HALF* )pOutVerts, floatCount );

            // The precision of a HALF variable is typically 0.1% to 0.05%, because there
            // is a ten-bit mantissa, with an implied one for eleven bits.
            // However, with any floating-point math cancellation can cause large
            // relative errors. The error tolerance for the FLOAT16 calculation was
            // experimentally determined to be sufficient. In 99.9% of cases the 1%
            // error tolerance would be sufficient, but occasionally greater error was
            // seen.
            maxErrorRatio = 0.025f;  // Error tolerance for FLOAT16 has to be higher.
        }


        for( UINT i = 0; i < float3Count; i++ )
        {
            // Use D3DX for the math here because it is simpler, and not performance critical.
            D3DXVECTOR3 DiffV = *( D3DXVECTOR3* )&pTestVertsASM[i] - *( D3DXVECTOR3* )&pTestVertsC[i];
            XMFLOAT3 Diff = *( XMFLOAT3* )&DiffV;
            FLOAT NormDif = D3DXVec3Length( ( D3DXVECTOR3* )&Diff ) /
                D3DXVec3Length( ( D3DXVECTOR3* )&pTestVertsC[i] );
            if( fabsf( NormDif ) > maxErrorRatio )
            {
                g_bVerificationError = TRUE;
            }
        }
    }

    // free resources
    XPhysicalFree( pOutVerts );
    VirtualFree( pInVerts, 0, MEM_RELEASE );
    VirtualFree( pInVertsHalf, 0, MEM_RELEASE );
    // Free the skin info base, which may be different from pSkinInfo
    VirtualFree( pSkinInfoBase, 0, MEM_RELEASE );
    VirtualFree( pPalette, 0, MEM_RELEASE );
    delete [] pTestVertsC;
    delete [] pTestVertsASM;
}


//--------------------------------------------------------------------------------------
// Name: DoTests
// Desc: Runs the various test and displays the results
//--------------------------------------------------------------------------------------
VOID DoTests()
{
    g_bVerificationError = FALSE;

    // maintain constistent randomness across tests
    srand( 0 );

    // Initialize the timing system.
    g_pXboxTimer->StartTiming();

    // Run tests
    // Currently the only test we run is CPU skinning. Additional tests and
    // demonstrations will be added as appropriate.
    SkinningTest();

    // Shutdown the timing system, and generate reports showing the statistics
    // gathered from the performance counters.
    g_pXboxTimer->StopTiming();

    if( g_bVerificationError )
    {
        // report verification errors
        PrintLine( L"Verification Error!" );
    }
    else
    {
        // display stats
        const WCHAR* strHeaderFormat = L"%11.11S %8.8S %5.5S %14.14S %9.9S %8.8S";
        const WCHAR* strDataFormat0 = L"%11S %8I64i       %14I64i %9I64i %8I64i";
        const WCHAR* strDataFormat1 = L"%11S %8I64i %5.2f %14I64i %9I64i %8I64i";

        // Print the headings.
        PrintLine( L"CPU frequency: %I64d", g_frequency );
        PrintLine( strHeaderFormat, "Operation", g_pXboxTimer->GetCounterName( 0 ), "Ratio",
                   g_pXboxTimer->GetCounterName( 1 ), g_pXboxTimer->GetCounterName( 2 ),
                   g_pXboxTimer->GetCounterName( 3 ) );
        PrintLine( strHeaderFormat,
                   "------------------------------",
                   "------------------------------",
                   "------------------------------",
                   "------------------------------",
                   "------------------------------",
                   "------------------------------" );

        PrintLine( L"Uncached destination:" );

        // Print the first result.
        const XboxEventSample* pReport1 = g_pXboxTimer->GetReport( 0 );
        PrintLine( strDataFormat0,
                   pReport1->strName,
                   pReport1->Counters[0], pReport1->Counters[1],
                   pReport1->Counters[2], pReport1->Counters[3] );

        // Print subsequent results, along with how they compare to the first result.
        for( UINT i = 1; i < g_pXboxTimer->GetNumReports(); ++i )
        {
            if( i == g_pXboxTimer->GetNumReports() / 2 )
                PrintLine( L"Cached destination:" );

            const XboxEventSample* pReport2 = g_pXboxTimer->GetReport( i );
            PrintLine( strDataFormat1,
                       pReport2->strName, pReport2->Counters[0],
                       FLOAT( pReport1->Counters[0] ) / FLOAT( pReport2->Counters[0] ),
                       pReport2->Counters[1],
                       pReport2->Counters[2], pReport2->Counters[3] );
        }
        PrintLine( L"" );
    }
}


//--------------------------------------------------------------------------------------
// Name: main
// Desc: Entry point to the program
//--------------------------------------------------------------------------------------
INT __cdecl main()
{
    // CPU frequency is a constant.
    g_frequency = 3200000000;
    g_pXboxTimer = new XboxHardwareTimer;

    // Initialize the console window
    g_Console.Create( "game:\\Media\\Fonts\\Courier_New_11.xpr", 0xFF1F005F, 0xFFFFFFFF );

    for(; ; )
    {
        DoTests();

        g_Console.Format( "More performance examples to come in the future...\n\n" );

        g_Console.Format( "Press LT + RT + RB to exit, A to run tests again.\n" );
        for(; ; )
        {
            ATG::GAMEPAD* pGamepad = ATG::Input::GetMergedInput(); // Detect reboot keypress
            if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_A )
            {
                g_Console.Format( "\n" );
                break;  // Rerun tests
            }
        }
    }
}

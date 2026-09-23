//--------------------------------------------------------------------------------------
// InlineAssembly.cpp
//
// This sample demonstrates how to use inline assembly language. See the documentation
// for more details.
//
// Game Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include <xtl.h>
#include <AtgInput.h>
#include "AtgConsole.h"
#include <cassert>

//--------------------------------------------------------------------------------------
// Globals
//--------------------------------------------------------------------------------------

// Console for output
ATG::Console    g_Console;

// Global float value to use in demonstrations of loading from global variables.
const FLOAT     g_fAdder = 3.543123e-2;


//--------------------------------------------------------------------------------------
// Name: GetValue()
// Desc: This simple function is used to demonstrate how you can call functions from
//       inline assembly. The calling process is fairly straightforward, but the symbol
//       will not be found unless the function is declared as extern "C".
//       This function is marked as noinline so that when we call it from C/C++ you can
//       see how functions are called.
//--------------------------------------------------------------------------------------
extern "C"
    int __declspec( noinline ) GetValue()
{
    return 17;
}


//--------------------------------------------------------------------------------------
// Name: SimpleAdd()
// Desc: This function adds its two parameters and returns the result. It is provided
//       for comparison with SimpleAddAssem.
//       This function is marked as noinline so that you can see what code the compiler
//       generates for this code as a function.
//--------------------------------------------------------------------------------------
int __declspec( noinline ) SimpleAddCPP( int x, int y )
{
    return x + y;
}


//--------------------------------------------------------------------------------------
// Name: SimpleAddAssem()
// Desc: This simple assembly language function adds its two parameters and returns
//       the sum. It works because the first two integer parameters are always placed
//       in r3 and r4, and integer return values go in r3.
//       Because this function is marked as __declspec( naked ) and no parameters are
//       referenced by name the compiler doesn't add any extra instructions, so the
//       entire functions is just two instructions.
//       Note that in release builds the compiler will generate identical code for
//       SimpleAddCPP as what we have here.
//--------------------------------------------------------------------------------------
int SimpleAddAssem( int x, int y )
{
    // Clang uses GNU extended inline assembly rather than MSVC's __asm{}/naked
    // functions. The compiler picks registers for the operands (%0..%2) and handles
    // the prologue/epilogue and return; we just describe the instruction.
    int result;
    __asm__(
        "add %0, %1, %2\n\t"
        // This NOP exists purely so the internals of this function differ from
        // SimpleAddCPP, so /opt:icf-style identical-code folding does not merge the
        // two and make the disassembly confusing.
        "nop"
        : "=r"( result )        // output: result in some GPR
        : "r"( x ), "r"( y ) ); // inputs: x, y in GPRs
    return result;
}


//--------------------------------------------------------------------------------------
// Name: AddCallsFunctionCPP()
// Desc: This function adds its two parameters and the value return by GetValue() and
//       returns the result. It is provided for comparison with AddCallsFunctionAssem.
//       This function is marked as noinline so that you can see what code the compiler
//       generates for this code as a function.
//--------------------------------------------------------------------------------------
int __declspec( noinline ) AddCallsFunctionCPP( int x, int y )
{
    return x + y + GetValue();
}


//--------------------------------------------------------------------------------------
// Name: AddCallsFunctionAssem()
// Desc: This assembly language function demonstrates the additional steps needed if
//       you want to call a function from your assembly language function. You must
//       preserve all volatile registers whose values you need--in this case r3, r4, and
//       the lr (the link register). It is also necessary to setup a stack frame, which
//       some functions depend on.
//       If you look at the code for AddCallsFunctionCPP you will actually see slightly
//       simpler code, because since GetValue() is in the same translation unit the
//       compiler knows what registers it uses and can generate more efficient code.
//--------------------------------------------------------------------------------------
int AddCallsFunctionAssem( int x, int y )
{
    // The MSVC version hand-wrote the stack frame, saved volatiles, and issued a
    // `bl GetValue` from inside the __asm block. With GNU inline assembly the clean
    // idiom is to make the call in C -- the compiler builds the frame and preserves
    // whatever it needs across the call -- and then do the arithmetic in inline asm,
    // which is what the function is really demonstrating.
    int g = GetValue();
    int result;
    __asm__(
        "add %0, %1, %2\n\t"    // x + y
        "add %0, %0, %3"        // + GetValue()
        : "=&r"( result )       // early-clobber: written before all inputs are read
        : "r"( x ), "r"( y ), "r"( g ) );
    return result;
}


//--------------------------------------------------------------------------------------
// Name: ArrayMathFunctionCPP()
// Desc: This function is an unoptimized C++ implementation of math on an array of
//       floats. This function calculates the reciprocal square root estimate of each
//       array element plus g_fAdder, and adds all these individual results together.
//       This is not intended to be a useful calculation, merely a simple enough
//       calculation that can demonstrate some optimization techniques.
//       The same function is implemented in two different assembly language versions
//       which give identical results.
//       This C++ version could be optimized substantially through loop unrolling,
//       prefetching, etc.
//       This function really should use VMX operations.
//--------------------------------------------------------------------------------------
FLOAT ArrayMathFunctionCPP( FLOAT* pData, DWORD count )
{
    // Accumulate the sum in a double precision variable for greater
    // accuracy. The only performance penalty is a round-to-float
    // operation at the end of the function.
    double sum = 0;

    for( DWORD i = 0; i < count; ++i )
    {
        // Calculate our expression and accumulate the results.
        sum += __frsqrte( pData[0] + ( double )g_fAdder );
        ++pData;
    }

    // Cast the result to a FLOAT for returning it.
    return ( FLOAT )sum;
}


//--------------------------------------------------------------------------------------
// Name: ArrayMathFunctionAssem()
// Desc: This function implements the same algorithm as ArrayMathFunctionCPP, but in
//       assembly language. It is a straightforward translation of ArrayMathFunctionCPP
//       with no optimizations.
//       This function really should use VMX operations.
//--------------------------------------------------------------------------------------
FLOAT ArrayMathFunctionAssem( FLOAT* pData, int count )
{
    // The same algorithm as ArrayMathFunctionCPP -- sum of frsqrte(pData[i] +
    // g_fAdder), accumulated in double precision -- expressed in GNU inline assembly.
    // The compiler loads g_fAdder for us (the "f" input); no hand-written address
    // arithmetic (lau/lal) is needed. Local numeric labels (1:/2:) are used so the
    // block is safe even if the function is inlined at more than one call site.
    const float adder = g_fAdder;
    double     sum;
    __asm__(
        "fsub    %0, %2, %2\n\t"     // sum = 0.0  (adder - adder; PPC has no float immediates)
        "cmpwi   %3, 0\n\t"          // if count == 0, the sum stays 0 and we skip the loop
        "beq     2f\n\t"
        "mtctr   %3\n\t"             // loop count into CTR
        "1:\n\t"
        "lfs     0, 0(%1)\n\t"       // load pData[i] into fr0
        "fadds   0, 0, %2\n\t"       // + g_fAdder
        "frsqrte 0, 0\n\t"           // reciprocal square-root estimate
        "fadd    %0, %0, 0\n\t"      // accumulate (double precision for accuracy)
        "addi    %1, %1, 4\n\t"      // advance to the next element
        "bdnz    1b\n\t"             // decrement CTR, loop while non-zero
        "2:"
        : "=&f"( sum ), "+b"( pData )        // sum (early-clobber); pData advances
        : "f"( adder ), "r"( count )
        : "f0", "ctr", "cr0", "memory" );
    // Round the double accumulator back to float, as the original frsp did.
    return ( FLOAT )sum;
}


//--------------------------------------------------------------------------------------
// Name: AssemData
// Desc: This struct is used to hold a collection of global values needed by 
//       ArrayMathFunctionAssemFast. By putting these values in a struct we can load
//       the address of the structure and then efficiently access all of the members
//       using offsets from this base address.
//--------------------------------------------------------------------------------------
struct AssemData
{
    FLOAT fMultiplier;
    FLOAT fZero;
}               g_assemData =
    {
        g_fAdder,
        0.0f,
    };


//--------------------------------------------------------------------------------------
// Name: ArrayMathFunctionAssemFast()
// Desc: This function implements the same algorithm as ArrayMathFunctionCPP, but in
//       assembly language. It is an optimized version that unrolls the loop four times
//       and tries to do efficient instruction scheduling. Further performance is still
//       available either by unwinding the loop further or by having the loop process
//       two loops in parallel--the addition for iteration 'n+1', and the frsqrte for
//       iteration n, similar to the way that this function does loading on the previous
//       loop and does some of the summing on the next loop.
//       Some of the assumptions that this function makes in order to allow more
//       optimizations with simpler code are that count is a multiple of four, and that
//       it is okay to read four elements beyond the end of the array.
//
//       The critical path in this function is an fadd and an frsqrte. Each instruction
//       has a latency of 20 cycles so without resorting to interleaving trickery or
//       unwinding the maximum throughput is 20 cycles per item. When we unwind a loop it
//       generally costs one extra cycle for each item we process per loop, due to
//       structural hazards, so if we unwind our loop four times it should take about 23
//       cycles. This loop comes very close to that theoretical maximum. Additional
//       unwinding or interleaving could gain even more performance.
//
//       If the data is not in L1 then prefetching should also be done.
//       This function really should use VMX operations.
//--------------------------------------------------------------------------------------
FLOAT ArrayMathFunctionAssemFast( FLOAT* pData, int count )
{
    // The same algorithm as ArrayMathFunctionCPP, manually unrolled four times so the
    // long-latency fadd/frsqrte chains from independent elements can overlap in the
    // pipeline. Assumes (as the original sample documents) that count is a non-zero
    // multiple of four and that four elements past the end of the array are readable.
    //
    // The MSVC version hand-scheduled every instruction and annotated its issue cycle;
    // clang's scheduler reorders GNU inline asm far less freely, so this port keeps the
    // faithful unrolled data flow and lets the four independent lanes (fr0-fr3) provide
    // the instruction-level parallelism, rather than reproducing the exact hand cycle
    // assignment. g_assemData.fMultiplier is just g_fAdder; the compiler loads it for us.
    const float mult = g_assemData.fMultiplier;
    double      sum;
    __asm__(
        "fsub    %0, %3, %3\n\t"     // sum = 0.0  (mult - mult)
        "srawi   %2, %2, 2\n\t"      // iterations = count / 4
        "mtctr   %2\n\t"
        "1:\n\t"
        // Load four consecutive elements into independent lanes.
        "lfs     0, 0(%1)\n\t"
        "lfs     1, 4(%1)\n\t"
        "lfs     2, 8(%1)\n\t"
        "lfs     3, 12(%1)\n\t"
        // Add g_fAdder to each (single precision).
        "fadds   0, 0, %3\n\t"
        "fadds   1, 1, %3\n\t"
        "fadds   2, 2, %3\n\t"
        "fadds   3, 3, %3\n\t"
        // Reciprocal square-root estimate of each.
        "frsqrte 0, 0\n\t"
        "frsqrte 1, 1\n\t"
        "frsqrte 2, 2\n\t"
        "frsqrte 3, 3\n\t"
        // Accumulate all four into the running sum (double precision for accuracy).
        "fadd    %0, %0, 0\n\t"
        "fadd    %0, %0, 1\n\t"
        "fadd    %0, %0, 2\n\t"
        "fadd    %0, %0, 3\n\t"
        "addi    %1, %1, 16\n\t"     // advance past the four elements
        "bdnz    1b"
        : "=&f"( sum ), "+b"( pData ), "+r"( count )
        : "f"( mult )
        : "f0", "f1", "f2", "f3", "ctr", "cr0", "memory" );
    // Round the double accumulator back to float, as the original frsp did.
    return ( FLOAT )sum;
}


//--------------------------------------------------------------------------------------
// Name: main
// Desc: Entry point to the program
//--------------------------------------------------------------------------------------
INT __cdecl main()
{
    // Initialize the console window
    g_Console.Create( "game:\\Media\\Fonts\\Courier_New_11.xpr", 0xFF1F005F, 0xFFFFFFFF );

    // Data array for our tests. Fill it with any random data.
    // The array must have a multiple of four elements, and there must be sixteen bytes
    // readable after the array (should be safe on the stack, would require extra allocation
    // on the heap).
    FLOAT data[256];
    for( int i = 0; i < ARRAYSIZE( data ); ++i )
    {
        data[i] = ( FLOAT )i * i + 0.251234234f;
    }

    for(; ; )
    {
        // Call our two integer math assembly language functions, and their C++
        // counterparts.
        int input1 = 11;
        int input2 = 23;
        int x = SimpleAddAssem( input1, input2 );
        int expectedX = SimpleAddCPP( input1, input2 );
        int y = AddCallsFunctionAssem( input1, input2 );
        int expectedY = AddCallsFunctionCPP( input1, input2 );
        g_Console.Format( "SimpleAddAssem returned %d (should be %d)\n", x, expectedX );
        g_Console.Format( "AddCallsFunctionAssem returned %d (should be %d)\n", y, expectedY );

        // Verify that our input array meets the requirements of the unwound assembly
        // language function.
        assert( ( ARRAYSIZE( data ) % 4 ) == 0 );
        // Call our two array math assembly language functions, and their C++
        // counterparts.
        FLOAT result1 = ArrayMathFunctionCPP( data, ARRAYSIZE( data ) );
        FLOAT result2 = ArrayMathFunctionAssem( data, ARRAYSIZE( data ) );
        FLOAT result3 = ArrayMathFunctionAssemFast( data, ARRAYSIZE( data ) );
        g_Console.Format( "Array math results are %1.8f, %1.8f, %1.8f\n    (should be identical)\n",
                          result1, result2, result3 );

        g_Console.Format( "Press LT + RT + RB to exit, A to run tests again.\n" );

        // Wait for the user to press a button.
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

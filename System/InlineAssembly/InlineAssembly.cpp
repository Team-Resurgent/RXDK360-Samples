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
int __declspec( naked ) SimpleAddAssem( int x, int y )
{
    __asm
        {
        // x is in r3
        // y is in r4
        add r3, r3, r4

        // This NOP exists purely to make it so that the internals of this function are
        // different from SimpleAddCPP. Without this the linker (when called with
        // /opt:icf, typical in release builds) will discard one of the duplicates
        // which makes looking at the result more confusing, since both calls would
        // then go to the same location.
        nop

        // The return value is in r3
        blr
        }
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
int __declspec( naked ) AddCallsFunctionAssem( int x, int y )
{
    __asm
        {
        // x is in r3
        // y is in r4

        // Function prologue. Set up a stack frame and
        // preserve r30, r31, and the link register
        mflr    r12
        stw     r12,-8( r1 )
        std     r30,-18h( r1 )
        std     r31,-10h( r1 )
        // See the Stack Frame Layout section of the documentation for details on how
        // much space should be reserved.
        stwu    r1,-70h( r1 )

        // Preserve r3 and r4 so their values aren't lost by
        // the function call.
        mr      r31,r3
        mr      r30,r4

        // Call our function call--it is assumed to trash all
        // volatile variables, including the link register.
        bl      GetValue
        // Add the result of GetValue to x
        add     r11,r3,r31
        // Add in y and store the result in r3
        add     r3,r11,r30

        // Function epilogue. Tear down the stack frame and
        // restore r30, r31, and the link register.
        addi    r1,r1,70h
        lwz     r12,-8( r1 )
        mtlr    r12
        ld      r30,-18h( r1 )
        ld      r31,-10h( r1 )

        // The return value is in r3
        blr
        }
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
FLOAT __declspec( naked ) ArrayMathFunctionAssem( FLOAT* pData, int count )
{
    __asm
        {
        // pData is in r3
        // count is in r4

        // Load the address of our global float variable. The lau mnemonic
        // loads the high 16-bits of the address. The low 16-bits can either
        // be loaded with lal (see the commented out code below) or, in
        // instructions that support it, can be part of the offset of the
        // load instruction, as shown in the lfs instruction below.
        lau     r5, g_fAdder
        //lal     r5, r5, g_fAdder
        // Load our global float variable. Note that there is no type
        // checking, so make sure you know whether you have a float, double,
        // or something else. Also note that the inline assembler now
        // lets you use the low 16-bits of the variable's address as an
        // offset in this addressing mode.
        lfs     fr2, g_fAdder( r5 )

        // Do a check to see if our loop should execute at all.
        cmplwi  cr6, r4, 0

        // Generate a zero. PowerPC has no immediate float constants so numbers
        // such as 0.0 and 1.0 are usually loaded from global variables.
        // If you know you have a 'normal' (not NaN or infinite) number then
        // you can conserve instruction space by using subtraction to create
        // a zero, although the latency will be greater.
        fsub    fr1, fr2, fr2

        // If count is zero then return immediately. The return value of zero
        // is already setup in fr1.
        beqlr   cr6
        LoopTop:
        // Implement the inner loop.
        // Load the next array element
        lfs     fr0, 0( r3 )
        // Add g_fAdder (from fr2)
        fadd    fr0, fr0, fr2
        // Calculate the reciprocal square root estimate
        frsqrte fr0, fr0
        // Accumulate the result into the sum in fr1, using double precision
        // math for maximum accuracy.
        fadd   fr1, fr1, fr0
        // Increment our array pointer to the next element.
        addi    r3,r3,4

        // Subtract one from our loop count, then branch
        // to the top of the loop if we haven't hit zero yet.
        addic.r4, r4, -1
        bne     LoopTop

        // Our return type is float so we need to round our result to float
        // precision to avoid errors.
        frsp    fr1, fr1
        // The return value is in fr1
        blr
        }
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
FLOAT __declspec( naked ) ArrayMathFunctionAssemFast( FLOAT* pData, int count )
{
    __asm
        {
        // pData is in r3
        // count is in r4

        // Load the address of our global float variable. This is a two
        // stage process because only 16-bits of immediate data can be
        // contained in a PowerPC instruction. We need to load the full
        // 32-bits of address, rather than using the offset field as
        // we load the elements, because a field offset plus the lower 16-bits
        // might overflow, leading to incorrect addressing.
        lau     r5, g_assemData
        lal     r5, r5, g_assemData
        // Load our global float variable. Note that there is no type
        // checking, so make sure you know whether you have a float, double,
        // or something else.
        lfs     fr2, offset AssemData.fMultiplier( r5 )

        // Assume that count is always non-zero and a multiple of our
        // unwind count, to avoid extra checks in this function. This
        // saves time and code space.

        // Load a zero from our assembly language data structure.
        // Note that now that we have the address of our structure we can load additional
        // data with a single instruction.
        lfs     fr1, offset AssemData.fZero( r5 )
        // Put a zero in the two partial sum registers so that we
        // get correct results on the first iteration. We could use
        // fmr in order to co-issue with some of the lfs instructions,
        // but fmr has ten cycle latency instead of the two cycle
        // latency of a load that hits in L1.
        lfs     fr7, offset AssemData.fZero( r5 )
        lfs     fr8, offset AssemData.fZero( r5 )

        // Load our first two data items before the loop, to improve
        // scheduling opportunities.
        lfs     fr3, 0( r3 )
        lfs     fr4, 4( r3 )
        lfs     fr5, 8( r3 )
        lfs     fr6, 12( r3 )

        // Make sure we know the code alignment of our loop top. Having a
        // known alignment lets us control our instruction pairing.
        // Having as much alignment as possible lets us maximize
        // instruction fetch efficiency after the branch.
        // nopalign 8 will insert a no-op if necessary to ensure 8-byte alignment.
        nopalign    8
        LoopTop:
        // Double precision adds (fadd instead of fadds) are used to increase the
        // precision.

        // Calculate the sum of: __frsqrte(pData[0] + g_fAdd1)
        // First add fr2 (g_fAdder) to our four data values.
        // These two instructions co-issue--we'll call their issue time cycle 0.
        fadd    fr3, fr3, fr2
        // Update our data pointer. This instruction needs to be issued as early
        // as possible to avoid data dependency stalls at IS2 on any loads that use
        // this address, which must issue from IS2 five cycles later than this
        // instruction.
        addi    r3, r3, 16

        // These two instructions co-issue on cycle 1.
        fadd    fr4, fr4, fr2
        // Subtract from our loop count. This instruction issues on
        // cycle 5. It can't co-issue because both instructions use
        // the integer pipeline.
        addic.r4, r4, -4

        // This instruction by itself on cycle 2.
        fadd    fr5, fr5, fr2
        // This instruction by itself on cycle 3. It can't co-issue with the
        // previous instruction because they both use the same pipeline--there is
        // a structural hazard.
        fadd    fr6, fr6, fr2

        // Add in the sum of fr3 and fr4 from the previous loop.
        // This issues around cycle 6, waiting a few cycles for the result from
        // the previous loop to be ready,
        // A short stall doesn't matter because the next instruction
        // would stall anyway.
        fadd    fr1, fr1, fr7

        // No instructions are issued on cycles 7, 8, and 9.
        // More work could be done here, either by having this loop do
        // additional calculations, or by unwinding the loop even more.

        // These instructions issue on cycles 10, 11, 12, and 13.
        // They are limited by data dependencies: the results of their
        // respective fmuls ten cycles earlier.
        frsqrte fr9, fr3
        frsqrte fr10, fr4
        frsqrte fr11, fr5
        frsqrte fr12, fr6
        // These instructions could be interleaved with the frsqrte instructions,
        // loading each register immediately after it issues, but this would risk
        // having them stall at IS2 because of a data dependency on R3, which would
        // then stall the following frsqrte instructions. Because load and store
        // instructions dual-issue to the load/store pipe and to VIQ they have to
        // worry about multiple types of stalls.
        lfs     fr3, 0( r3 )
        lfs     fr4, 4( r3 )
        lfs     fr5, 8( r3 )
        lfs     fr6, 12( r3 )

        // No instructions are issued on cycles 15-20. More work is needed to
        // fully utilize the pipelines.

        // Merge our results using an add tree: combine fr9 and fr10 and combine
        // fr11 and fr12. The combined results are added to the sum later.
        // This instruction issues on cycle 21, ten cycles after the instruction to
        // calculate fr10 was issued.
        fadd    fr7, fr9, fr10

        // Add in the sum of fr5 and fr6 from the previous loop.
        // This issues on cycle 24, using a spare slot just before fr8 is filled
        // with its new value.
        fadd    fr1, fr1, fr8

        // Second part of the add tree. This issues on cycle 23, ten cycles after
        // the instruction to calculate fr12 was issued.
        fadd    fr8, fr11, fr12

        // Branch to the top of our loop if our decrement earlier went to zero.
        // Because of stalls earlier in the loop the branch predictor should
        // make this branch essentially free--about eight cycles or more of stalls
        // per iteration can overlap with the cost of a correctly predicted branch.
        // This branch issues on cycle 24 (it can't pair with the previous instruction
        // because it is in a different instruction pair).
        bne     LoopTop

        // We have to add in the results of the last iteration.
        fadd    fr1, fr1, fr7
        fadd    fr1, fr1, fr8

        // Our return type is float so we need to round our result to float
        // precision to avoid errors.
        frsp    fr1, fr1
        // The return value is in fr1
        blr
        }
}


//--------------------------------------------------------------------------------------
// Name: main
// Desc: Entry point to the program
//--------------------------------------------------------------------------------------
VOID __cdecl main()
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

//--------------------------------------------------------------------------------------
// XuiFontRenderer.cpp
//
// Shows how to implement a replacement Xui font renderer.
//
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include <xtl.h>
#include <xui.h>
#include <xuiapp.h>
#include <xgraphics.h>
#include "fontrenderer.h"


#ifndef SAFE_RELEASE
#define SAFE_RELEASE(e) { if(e) { (e)->Release(); (e)=NULL; } }
#endif


//
// Define a vector font.  Each character is a line segment list.  When rendering,
// we'll render each line segment as a quad.
//

const int       MAX_LINES_PER_CHARACTER = 9;
const int       BASE_CHAR = ' ';

typedef struct
{
    FLOAT x1, y1, x2, y2;
}               LINESEGMENT;

typedef struct
{
    int numLines;
    LINESEGMENT lines[MAX_LINES_PER_CHARACTER];
}               CHARACTER;

CHARACTER characters_[] =
{
    {0, { {0.000,0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, {0.000,0.000,0.000,
                0.000}, {0.000,0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, {0.000,
                0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, }}, //  
    {2, { {0.333,0.889,0.333,0.778}, {0.333,0.556,0.333,0.000}, {0.000,0.000,0.000,0.000}, {0.000,0.000,0.000,
                0.000}, {0.000,0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, {0.000,
                0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, }}, // !
    {2, { {0.222,0.222,0.222,0.000}, {0.444,0.222,0.444,0.000}, {0.000,0.000,0.000,0.000}, {0.000,0.000,0.000,
                0.000}, {0.000,0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, {0.000,
                0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, }}, // "
    {4, { {0.111,0.889,0.333,0.000}, {0.333,0.889,0.556,0.000}, {0.000,0.556,0.667,0.556}, {0.111,0.333,0.667,
                0.333}, {0.000,0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, {0.000,
                0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, }}, // #
    {6, { {0.333,0.000,0.333,0.889}, {0.111,0.778,0.556,0.778}, {0.556,0.778,0.556,0.444}, {0.556,0.444,0.111,
                0.444}, {0.111,0.444,0.111,0.111}, {0.111,0.111,0.556,0.111}, {0.000,0.000,0.000,0.000}, {0.000,
                0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, }}, // $
    {9, { {0.111,0.889,0.556,0.000}, {0.111,0.111,0.111,0.222}, {0.111,0.222,0.222,0.222}, {0.222,0.222,0.222,
                0.111}, {0.222,0.111,0.111,0.111}, {0.556,0.778,0.444,0.778}, {0.444,0.778,0.444,0.667}, {0.444,
                0.667,0.556,0.667}, {0.556,0.667,0.556,0.778}, }}, // %
    {0, { {0.000,0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, {0.000,0.000,0.000,
                0.000}, {0.000,0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, {0.000,
                0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, }}, // &
    {1, { {0.333,0.222,0.333,0.000}, {0.000,0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, {0.000,0.000,0.000,
                0.000}, {0.000,0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, {0.000,
                0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, }}, // '
    {3, { {0.444,0.889,0.333,0.556}, {0.333,0.556,0.333,0.333}, {0.333,0.333,0.444,0.000}, {0.000,0.000,0.000,
                0.000}, {0.000,0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, {0.000,
                0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, }}, // (
    {3, { {0.333,0.889,0.444,0.556}, {0.444,0.556,0.444,0.333}, {0.444,0.333,0.333,0.000}, {0.000,0.000,0.000,
                0.000}, {0.000,0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, {0.000,
                0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, }}, // )
    {4, { {0.111,0.444,0.556,0.444}, {0.333,0.111,0.333,0.778}, {0.111,0.222,0.556,0.667}, {0.111,0.667,0.556,
                0.222}, {0.000,0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, {0.000,
                0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, }}, // *
    {2, { {0.333,0.778,0.333,0.111}, {0.111,0.444,0.556,0.444}, {0.000,0.000,0.000,0.000}, {0.000,0.000,0.000,
                0.000}, {0.000,0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, {0.000,
                0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, }}, // +
    {2, { {0.333,0.889,0.444,0.667}, {0.444,0.667,0.444,0.444}, {0.000,0.000,0.000,0.000}, {0.000,0.000,0.000,
                0.000}, {0.000,0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, {0.000,
                0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, }}, // ,
    {1, { {0.000,0.444,0.667,0.444}, {0.000,0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, {0.000,0.000,0.000,
                0.000}, {0.000,0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, {0.000,
                0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, }}, // -
    {1, { {0.333,0.889,0.333,0.778}, {0.000,0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, {0.000,0.000,0.000,
                0.000}, {0.000,0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, {0.000,
                0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, }}, // .
    {1, { {0.111,0.889,0.556,0.000}, {0.000,0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, {0.000,0.000,0.000,
                0.000}, {0.000,0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, {0.000,
                0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, }}, // /
    {4, { {0.000,0.889,0.667,0.889}, {0.667,0.889,0.667,0.000}, {0.667,0.000,0.000,0.000}, {0.000,0.000,0.000,
                0.889}, {0.000,0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, {0.000,
                0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, }}, // 0
    {1, { {0.333,0.889,0.333,0.000}, {0.000,0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, {0.000,0.000,0.000,
                0.000}, {0.000,0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, {0.000,
                0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, }}, // 1
    {5, { {0.000,0.000,0.667,0.000}, {0.667,0.000,0.667,0.444}, {0.667,0.444,0.000,0.444}, {0.000,0.444,0.000,
                0.889}, {0.000,0.889,0.667,0.889}, {0.000,0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, {0.000,
                0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, }}, // 2
    {4, { {0.000,0.000,0.667,0.000}, {0.667,0.000,0.667,0.889}, {0.667,0.889,0.000,0.889}, {0.000,0.444,0.667,
                0.444}, {0.000,0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, {0.000,
                0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, }}, // 3
    {3, { {0.000,0.000,0.000,0.444}, {0.000,0.444,0.667,0.444}, {0.667,0.000,0.667,0.889}, {0.000,0.000,0.000,
                0.000}, {0.000,0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, {0.000,
                0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, }}, // 4
    {5, { {0.000,0.889,0.667,0.889}, {0.667,0.889,0.667,0.444}, {0.667,0.444,0.000,0.444}, {0.000,0.444,0.000,
                0.000}, {0.000,0.000,0.667,0.000}, {0.000,0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, {0.000,
                0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, }}, // 5
    {4, { {0.000,0.000,0.000,0.889}, {0.000,0.889,0.667,0.889}, {0.667,0.889,0.667,0.444}, {0.667,0.444,0.000,
                0.444}, {0.000,0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, {0.000,
                0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, }}, // 6
    {2, { {0.000,0.000,0.667,0.000}, {0.667,0.000,0.667,0.889}, {0.000,0.000,0.000,0.000}, {0.000,0.000,0.000,
                0.000}, {0.000,0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, {0.000,
                0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, }}, // 7
    {5, { {0.000,0.889,0.000,0.000}, {0.000,0.000,0.667,0.000}, {0.667,0.000,0.667,0.889}, {0.667,0.889,0.000,
                0.889}, {0.000,0.444,0.667,0.444}, {0.000,0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, {0.000,
                0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, }}, // 8
    {4, { {0.667,0.889,0.667,0.000}, {0.667,0.000,0.000,0.000}, {0.000,0.000,0.000,0.444}, {0.000,0.444,0.667,
                0.444}, {0.000,0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, {0.000,
                0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, }}, // 9
    {2, { {0.333,0.889,0.333,0.778}, {0.333,0.333,0.333,0.222}, {0.000,0.000,0.000,0.000}, {0.000,0.000,0.000,
                0.000}, {0.000,0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, {0.000,
                0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, }}, // :
    {0, { {0.000,0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, {0.000,0.000,0.000,
                0.000}, {0.000,0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, {0.000,
                0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, }}, // ;
    {0, { {0.000,0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, {0.000,0.000,0.000,
                0.000}, {0.000,0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, {0.000,
                0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, }}, // <
    {0, { {0.000,0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, {0.000,0.000,0.000,
                0.000}, {0.000,0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, {0.000,
                0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, }}, // =
    {0, { {0.000,0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, {0.000,0.000,0.000,
                0.000}, {0.000,0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, {0.000,
                0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, }}, // >
    {0, { {0.000,0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, {0.000,0.000,0.000,
                0.000}, {0.000,0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, {0.000,
                0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, }}, // ?
    {0, { {0.000,0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, {0.000,0.000,0.000,
                0.000}, {0.000,0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, {0.000,
                0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, }}, // @
    {5, { {0.000,0.889,0.000,0.222}, {0.000,0.222,0.333,0.000}, {0.333,0.000,0.667,0.222}, {0.667,0.222,0.667,
                0.889}, {0.000,0.556,0.667,0.556}, {0.000,0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, {0.000,
                0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, }}, // A
    {8, { {0.000,0.889,0.000,0.000}, {0.000,0.000,0.444,0.000}, {0.444,0.000,0.667,0.222}, {0.667,0.222,0.444,
                0.444}, {0.444,0.444,0.000,0.444}, {0.444,0.444,0.667,0.667}, {0.667,0.667,0.444,0.889}, {0.444,
                0.889,0.000,0.889}, {0.000,0.000,0.000,0.000}, }}, // B
    {3, { {0.667,0.889,0.000,0.889}, {0.000,0.889,0.000,0.000}, {0.000,0.000,0.667,0.000}, {0.000,0.000,0.000,
                0.000}, {0.000,0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, {0.000,
                0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, }}, // C
    {6, { {0.000,0.889,0.000,0.000}, {0.000,0.000,0.333,0.000}, {0.333,0.000,0.667,0.333}, {0.667,0.333,0.667,
                0.556}, {0.667,0.556,0.333,0.889}, {0.333,0.889,0.000,0.889}, {0.000,0.000,0.000,0.000}, {0.000,
                0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, }}, // D
    {4, { {0.667,0.889,0.000,0.889}, {0.000,0.889,0.000,0.000}, {0.000,0.000,0.667,0.000}, {0.000,0.444,0.667,
                0.444}, {0.000,0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, {0.000,
                0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, }}, // E
    {3, { {0.000,0.889,0.000,0.000}, {0.000,0.000,0.667,0.000}, {0.000,0.444,0.667,0.444}, {0.000,0.000,0.000,
                0.000}, {0.000,0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, {0.000,
                0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, }}, // F
    {6, { {0.667,0.222,0.667,0.000}, {0.667,0.000,0.000,0.000}, {0.000,0.000,0.000,0.889}, {0.000,0.889,0.667,
                0.889}, {0.667,0.889,0.667,0.556}, {0.667,0.556,0.222,0.556}, {0.000,0.000,0.000,0.000}, {0.000,
                0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, }}, // G
    {3, { {0.000,0.889,0.000,0.000}, {0.000,0.444,0.667,0.444}, {0.667,0.889,0.667,0.000}, {0.000,0.000,0.000,
                0.000}, {0.000,0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, {0.000,
                0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, }}, // H
    {3, { {0.000,0.889,0.667,0.889}, {0.333,0.889,0.333,0.000}, {0.000,0.000,0.667,0.000}, {0.000,0.000,0.000,
                0.000}, {0.000,0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, {0.000,
                0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, }}, // I
    {3, { {0.667,0.000,0.667,0.889}, {0.667,0.889,0.333,0.889}, {0.333,0.889,0.000,0.556}, {0.000,0.000,0.000,
                0.000}, {0.000,0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, {0.000,
                0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, }}, // J
    {3, { {0.000,0.889,0.000,0.000}, {0.000,0.444,0.667,0.000}, {0.000,0.444,0.667,0.889}, {0.000,0.000,0.000,
                0.000}, {0.000,0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, {0.000,
                0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, }}, // K
    {2, { {0.000,0.000,0.000,0.889}, {0.000,0.889,0.667,0.889}, {0.000,0.000,0.000,0.000}, {0.000,0.000,0.000,
                0.000}, {0.000,0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, {0.000,
                0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, }}, // L
    {4, { {0.000,0.889,0.000,0.000}, {0.000,0.000,0.333,0.333}, {0.333,0.333,0.667,0.000}, {0.667,0.000,0.667,
                0.889}, {0.000,0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, {0.000,
                0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, }}, // M
    {3, { {0.000,0.889,0.000,0.000}, {0.000,0.000,0.667,0.889}, {0.667,0.889,0.667,0.000}, {0.000,0.000,0.000,
                0.000}, {0.000,0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, {0.000,
                0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, }}, // N
    {4, { {0.000,0.889,0.000,0.000}, {0.000,0.000,0.667,0.000}, {0.667,0.000,0.667,0.889}, {0.667,0.889,0.000,
                0.889}, {0.000,0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, {0.000,
                0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, }}, // O
    {4, { {0.000,0.889,0.000,0.000}, {0.000,0.000,0.667,0.000}, {0.667,0.000,0.667,0.444}, {0.667,0.444,0.000,
                0.444}, {0.000,0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, {0.000,
                0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, }}, // P
    {6, { {0.667,0.556,0.333,0.889}, {0.333,0.889,0.000,0.889}, {0.000,0.889,0.000,0.000}, {0.000,0.000,0.667,
                0.000}, {0.667,0.000,0.667,0.556}, {0.333,0.556,0.667,0.889}, {0.000,0.000,0.000,0.000}, {0.000,
                0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, }}, // Q
    {5, { {0.000,0.889,0.000,0.000}, {0.000,0.000,0.667,0.000}, {0.667,0.000,0.667,0.444}, {0.667,0.444,0.000,
                0.444}, {0.222,0.444,0.667,0.889}, {0.000,0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, {0.000,
                0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, }}, // R
    {5, { {0.000,0.889,0.667,0.889}, {0.667,0.889,0.667,0.444}, {0.667,0.444,0.000,0.444}, {0.000,0.444,0.000,
                0.000}, {0.000,0.000,0.667,0.000}, {0.000,0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, {0.000,
                0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, }}, // S
    {2, { {0.333,0.889,0.333,0.000}, {0.000,0.000,0.667,0.000}, {0.000,0.000,0.000,0.000}, {0.000,0.000,0.000,
                0.000}, {0.000,0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, {0.000,
                0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, }}, // T
    {3, { {0.000,0.000,0.000,0.889}, {0.000,0.889,0.667,0.889}, {0.667,0.889,0.667,0.000}, {0.000,0.000,0.000,
                0.000}, {0.000,0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, {0.000,
                0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, }}, // U
    {2, { {0.000,0.000,0.333,0.889}, {0.333,0.889,0.667,0.000}, {0.000,0.000,0.000,0.000}, {0.000,0.000,0.000,
                0.000}, {0.000,0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, {0.000,
                0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, }}, // V
    {4, { {0.000,0.000,0.000,0.889}, {0.000,0.889,0.333,0.556}, {0.333,0.556,0.667,0.889}, {0.667,0.889,0.667,
                0.000}, {0.000,0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, {0.000,
                0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, }}, // W
    {2, { {0.000,0.000,0.667,0.889}, {0.000,0.889,0.667,0.000}, {0.000,0.000,0.000,0.000}, {0.000,0.000,0.000,
                0.000}, {0.000,0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, {0.000,
                0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, }}, // X
    {3, { {0.000,0.000,0.333,0.333}, {0.333,0.333,0.667,0.000}, {0.333,0.333,0.333,0.889}, {0.000,0.000,0.000,
                0.000}, {0.000,0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, {0.000,
                0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, }}, // Y
    {3, { {0.000,0.000,0.667,0.000}, {0.667,0.000,0.000,0.889}, {0.000,0.889,0.667,0.889}, {0.000,0.000,0.000,
                0.000}, {0.000,0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, {0.000,
                0.000,0.000,0.000}, {0.000,0.000,0.000,0.000}, }}, // Z
};

const int       NUM_CHARACTERS = sizeof( characters_ ) / sizeof( characters_[0] );

//
// These are the metrics for this font. Font metrics are scaled by point size.
// Character metrics are typically per character, but since this is a all upper-
// case fixed-pitch font, the character metrics are the same for all characters.
// These g_CharMetrics values will also be scaled by the point size per font.
//

XUIFontMetrics  g_FontMetrics =
{
    1.4f, 1.1f, 0.3f, 0.78f, 1.0f, 1.0f
};
XUICharMetrics  g_CharMetrics =
{
    0.0f, 0.0f, 0.78f, 1.0f, 1.0f
};



//
// Vertex and Pixel shaders
//

const char g_VertexShaderSource[] =
    "struct VSOUT\n"
    "{\n"
        "float4 Position : POSITION;\n"
    "};\n"
    ""
    "uniform float4x4 WorldViewProj : register(c0);\n"
    ""
    "VSOUT VShader(const float2 Position0 : POSITION0)\n"
    "{\n"
        "VSOUT Output;\n"
        "float4 pos;\n"
        "Output.Position = mul(float4(Position0.x, Position0.y, 0, 1), WorldViewProj);\n"
        "return Output;\n"
    "}";


const char g_PixelShaderSource[] =
    "uniform float4 Color : register(c1);\n"
    ""
    "float4 PShader(float4 Position : POSITION) : COLOR\n"
    "{\n"
        "return Color;\n"
    "}";



//--------------------------------------------------------------------------------------
// Name: FontInfo
// Desc: Font instance info.  A font consists of a typeface, point size and style. Since
//       this is a fixed typeface, we only care about point size.
//--------------------------------------------------------------------------------------
struct FontInfo
{
    FontInfo( FLOAT fPointSize ) : m_fPointSize( fPointSize )
    {
    }

    FLOAT m_fPointSize;
};


//--------------------------------------------------------------------------------------
// Name: MyXuiFontRenderer
// Desc: Constructor.  Initializes the shaders needed for this renderer
//--------------------------------------------------------------------------------------
MyXuiFontRenderer::MyXuiFontRenderer( IDirect3DDevice9* pDevice ) : m_pVertexShader( NULL ),
                                                                    m_pPixelShader( NULL ),
                                                                    m_pVertexDeclaration( NULL ),
                                                                    m_fDpi( 72.0f ),
                                                                    m_RenderMode( DrawToTexture )
{
    //
    // Create the shaders and vertex declaration needed to render our font
    //

    LPD3DXBUFFER pShader = NULL;
    HRESULT hr = D3DXCompileShader(
        g_VertexShaderSource, sizeof( g_VertexShaderSource )-1, NULL, NULL,
        "VShader", "vs_2_0", D3DXSHADER_DEBUG, &pShader, NULL, NULL );
    if( FAILED( hr ) )
    {
        if( pShader != NULL )
        {
            pShader->Release();
            pShader = NULL;
        }
        return;
    }

    hr = pDevice->CreateVertexShader( ( DWORD* )pShader->GetBufferPointer(), &m_pVertexShader );
    pShader->Release();
    pShader = NULL;
    if( FAILED( hr ) )
    {
        return;
    }

    hr = D3DXCompileShader(
        g_PixelShaderSource, sizeof( g_PixelShaderSource )-1, NULL, NULL,
        "PShader", "ps_2_0", D3DXSHADER_DEBUG, &pShader, NULL, NULL );
    if( FAILED( hr ) )
    {
        if( pShader != NULL )
        {
            pShader->Release();
            pShader = NULL;
        }
        m_pVertexShader->Release();
        m_pVertexShader = NULL;
        return;
    }

    hr = pDevice->CreatePixelShader( ( DWORD* )pShader->GetBufferPointer(), &m_pPixelShader );
    pShader->Release();
    pShader = NULL;
    if( FAILED( hr ) )
    {
        m_pVertexShader->Release();
        m_pVertexShader = NULL;
        return;
    }

    static const D3DVERTEXELEMENT9 VertexElements[] =
    {
        { 0,  0, D3DDECLTYPE_FLOAT2,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
        D3DDECL_END()
    };

    hr = pDevice->CreateVertexDeclaration( VertexElements, &m_pVertexDeclaration );
}


//--------------------------------------------------------------------------------------
// Name: ~MyXuiFontRenderer
// Desc: Destructor
//--------------------------------------------------------------------------------------
MyXuiFontRenderer::~MyXuiFontRenderer( VOID )
{
    SAFE_RELEASE( m_pVertexShader );
    SAFE_RELEASE( m_pPixelShader );
    SAFE_RELEASE( m_pVertexDeclaration );
}


//--------------------------------------------------------------------------------------
// Name: SetRendererMode
// Desc: Sets the renderer mode to render to the Xui Cache or direct to device
//--------------------------------------------------------------------------------------
void MyXuiFontRenderer::SetRendererMode( RenderMode mode )
{
    m_RenderMode = mode;
}


//--------------------------------------------------------------------------------------
// Name: Init (IXuiFontRenderer)
// Desc: Initializes the font renderer.  On XBox, fDpi is always 96.0f.
//--------------------------------------------------------------------------------------
HRESULT
STDMETHODCALLTYPE MyXuiFontRenderer::Init( FLOAT fDpi )
{
    m_fDpi = fDpi;
    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Term (IXuiFontRenderer)
// Desc: Completes any final cleanup
//--------------------------------------------------------------------------------------
VOID
STDMETHODCALLTYPE MyXuiFontRenderer::Term()
{
    return;
}


//--------------------------------------------------------------------------------------
// Name: GetCaps (IXuiFontRenderer)
// Desc: Returns this font renderer's capability flags
//--------------------------------------------------------------------------------------
HRESULT
STDMETHODCALLTYPE MyXuiFontRenderer::GetCaps( DWORD* pdwCaps )
{
    //
    // If the renderer is fixed-size only (e.g. a bitmap font), omit the 
    // XUI_FONT_RENDERER_CAP_POINT_SIZE_RESPECTED flag. To bypass the XUI glyph 
    // texture cache, specify the XUI_FONT_RENDERER_CAP_INTERNAL_GLYPH_CACHE flag.
    //

    if( pdwCaps != NULL )
    {
        switch( m_RenderMode )
        {
            case DrawToTexture:
                // setting this means XUI calls the DrawCharToTexture method
                *pdwCaps = XUI_FONT_RENDERER_CAP_POINT_SIZE_RESPECTED;
                break;
            case DrawToDevice:
                // setting this means XUI calls the DrawCharsToDevice method
                *pdwCaps = XUI_FONT_RENDERER_CAP_INTERNAL_GLYPH_CACHE |
                    XUI_FONT_RENDERER_CAP_POINT_SIZE_RESPECTED;
                break;
        }
    }
    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: CreateFont (IXuiFontRenderer)
// Desc: Creates a font based on the specified typeface descriptor, point size, style
//       and returns an opaque handle <hFont>.
//--------------------------------------------------------------------------------------
HRESULT
STDMETHODCALLTYPE MyXuiFontRenderer::CreateFont( const TypefaceDescriptor* pTypefaceDescriptor,
                                                 FLOAT fPointSize, DWORD dwStyle, DWORD dwReserved,
                                                 HFONTOBJ* phFont )
{
    //
    // Since this renderer supports only a single typeface, we ignore the typeface
    // descriptor here. If this renderer supported multiple typefaces, the typface
    // descriptor would point to the typeface to use.
    //

    //
    // Xui keeps a handle to each font it uses. The handles are opaque to Xui, so
    // we'll return our font info pointer as the handle.  When Xui calls back to 
    // render or get metrics, we'll be able to just cast the font handle to get
    // to the font info...
    //

    FontInfo* pFontInfo = new FontInfo( fPointSize );

    *phFont = ( HFONTOBJ )pFontInfo;

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: ReleaseFont (IXuiFontRenderer)
// Desc: Releases the font referenced by the opaque handle <hFont>
//--------------------------------------------------------------------------------------
VOID
STDMETHODCALLTYPE MyXuiFontRenderer::ReleaseFont( HFONTOBJ hFont )
{
    if( hFont == 0 )
        return;

    delete ( ( FontInfo* )hFont );

    return;
}


//--------------------------------------------------------------------------------------
// Name: GetFontMetrics (IXuiFontRenderer)
// Desc: Returns information about a specific font. Xui uses this for layout, etc.
//--------------------------------------------------------------------------------------
HRESULT
STDMETHODCALLTYPE MyXuiFontRenderer::GetFontMetrics( HFONTOBJ hFont, XUIFontMetrics* pFontMetrics )
{
    if( hFont == 0 || pFontMetrics == 0 )
        return E_INVALIDARG;

    FLOAT fPointSize = ( ( FontInfo* )hFont )->m_fPointSize;

    pFontMetrics->fLineHeight = g_FontMetrics.fLineHeight * fPointSize;
    pFontMetrics->fMaxAscent = g_FontMetrics.fMaxAscent * fPointSize;
    pFontMetrics->fMaxDescent = g_FontMetrics.fMaxDescent * fPointSize;
    pFontMetrics->fMaxWidth = g_FontMetrics.fMaxWidth * fPointSize;
    pFontMetrics->fMaxHeight = g_FontMetrics.fMaxHeight * fPointSize;
    pFontMetrics->fMaxAdvance = g_FontMetrics.fMaxAdvance * fPointSize;

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: GetCharMetrics (IXuiFontRenderer)
// Desc: Returns information about a particular character from a specific font. Xui uses
//       this for horizontal layout, and for setting up its glyph texture cache.  If the
//       requested character is not in the font, returning S_FALSE will invoke Xui's 
//       font fallback mechanism (see TypefaceDescriptor in the XDK documentation).
//--------------------------------------------------------------------------------------
HRESULT
STDMETHODCALLTYPE MyXuiFontRenderer::GetCharMetrics( HFONTOBJ hFont, WCHAR wch, XUICharMetrics* pCharMetrics )
{
    if( hFont == 0 || pCharMetrics == 0 )
        return E_INVALIDARG;

    //
    // This is a fixed pitch, upper-case font.  All the character
    // metrics are the same.  Otherwise, we would return the specific
    // values for the requested character.
    //

    FLOAT fPointSize = ( ( FontInfo* )hFont )->m_fPointSize;

    pCharMetrics->fMinX = g_CharMetrics.fMinX * fPointSize;
    pCharMetrics->fMinY = g_CharMetrics.fMinY * fPointSize;
    pCharMetrics->fMaxX = g_CharMetrics.fMaxX * fPointSize;
    pCharMetrics->fMaxY = g_CharMetrics.fMaxY * fPointSize;
    pCharMetrics->fAdvance = g_CharMetrics.fAdvance * fPointSize;

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: RenderCharacter (internal function)
// Desc: Renders a character to the IDirect3DDevice9, with the specified point size,
//       position and color.
//--------------------------------------------------------------------------------------
HRESULT MyXuiFontRenderer::RenderCharacter( IDirect3DDevice9* pDevice, WCHAR wch, FLOAT fPointSize,
                                            FLOAT x, FLOAT y, D3DXCOLOR color )
{
    //
    // This font is a vector font. Each character is a list of line segments.
    // We'll render the line segments as quads here.
    //

    int charIndex;
    if( wch >= L'a' && wch <= 'z' )
        charIndex = wch - L'a' + L'A' - L' ';
    else
        charIndex = wch - L' ';
    if( charIndex < 0 || charIndex >= NUM_CHARACTERS )
        charIndex = L'*' - L' ';

    const CHARACTER *pCharacter = &characters_[charIndex];
    int numLines = pCharacter->numLines;
    if( numLines > 0 )
    {
        pDevice->SetPixelShaderConstantF( 1, ( FLOAT* )&color, 1 );

        // Render each line segment as a quad

        struct MYQUAD
        {
            FLOAT x1, y1, x2, y2, x3, y3, x4, y4;
        };

        MYQUAD quadList[MAX_LINES_PER_CHARACTER];

        const LINESEGMENT *pLineSegment = pCharacter->lines;
        MYQUAD *pQuadList = quadList;
        for( int i = 0; i < numLines; ++i )
        {
            XMVECTOR vec = XMVectorSet(
                pLineSegment->x1 - pLineSegment->x2,
                pLineSegment->y1 - pLineSegment->y2,
                0, 0 );

            vec = XMVector2Normalize( vec );
            vec = XMVector2Orthogonal( vec );
            if( fPointSize < 24 )
            {
                // lighten the weight for smaller point sizes
                vec *= ( 1.0f / 48.0f ) * fPointSize + 0.5f;
            }

            pQuadList->x1 = pLineSegment->x1 * fPointSize + vec.x + x;
            pQuadList->y1 = pLineSegment->y1 * fPointSize + vec.y + y;
            pQuadList->x2 = pLineSegment->x2 * fPointSize + vec.x + x;
            pQuadList->y2 = pLineSegment->y2 * fPointSize + vec.y + y;
            pQuadList->x3 = pLineSegment->x2 * fPointSize - vec.x + x;
            pQuadList->y3 = pLineSegment->y2 * fPointSize - vec.y + y;
            pQuadList->x4 = pLineSegment->x1 * fPointSize - vec.x + x;
            pQuadList->y4 = pLineSegment->y1 * fPointSize - vec.y + y;
            ++pLineSegment;
            ++pQuadList;
        }
        pDevice->DrawPrimitiveUP( D3DPT_QUADLIST, numLines, quadList, 2 * sizeof( FLOAT ) );
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: DrawCharsToDevice (IXuiFontRenderer)
// Desc: Renders a character to a D3D device with the specified point size, position and
//       color.  pCharData is an array of characters to render. pClipRect is a rectangle
//       to clip to (Xui clips to top and bottom only, horizontal layout prevents 
//       characters from needing to clip left or right). The IDirect3DDevice9* is
//       retrieved from the HXUIDC, as well as other settings, such as dropshadow color.
//       Use pWorldViewProj to transform specified coordinates to screen-space.
//--------------------------------------------------------------------------------------
HRESULT
STDMETHODCALLTYPE MyXuiFontRenderer::DrawCharsToDevice( HFONTOBJ hFont, CharData* pCharData, DWORD dwCount,
                                                        RECT* pClipRect, HXUIDC hDC,
                                                        D3DXMATRIX* pWorldViewProj )
{
    if( hFont == NULL || hDC == NULL )
    {
        return E_INVALIDARG;
    }

    if( dwCount == 0 )
    {
        return S_OK;
    }

    //
    // Retrieve the D3D device from the HXUIDC. We will release this 
    // device pointer when we're done with it.  We could also retrieve
    // the dropshadow color from the dc via XuiGetTextDropShadowColor.
    //

    IDirect3DDevice9* pDevice = NULL;
    HRESULT hr = XuiRenderGetDevice( hDC, &pDevice );
    if( FAILED( hr ) || pDevice == NULL )
    {
        return E_INVALIDARG;
    }

    //
    // Render a list of characters directly to the device.  XUI passes
    // in the world/view/projection matrix--we'll just modify it to 
    // suite our shaders.
    //

    FLOAT fPointSize = ( ( FontInfo* )hFont )->m_fPointSize;

    pDevice->SetVertexShader( m_pVertexShader );
    pDevice->SetPixelShader( m_pPixelShader );
    pDevice->SetVertexDeclaration( m_pVertexDeclaration );

    D3DXMATRIX matInvViewPort;
    D3DXMatrixIdentity( &matInvViewPort );

    //
    // Generate the necessary matrix to get our coordinates into clip space
    //
    D3DVIEWPORT9 viewPort;
    pDevice->GetViewport( &viewPort );

    matInvViewPort._11 = 2.0f / viewPort.Width;
    matInvViewPort._22 = -2.0f / viewPort.Height;
    matInvViewPort._33 = 1.0f / ( viewPort.MaxZ - viewPort.MinZ );
    matInvViewPort._41 = -1;
    matInvViewPort._42 = 1;

    D3DXMATRIX matTrans;
    D3DXMatrixMultiply( &matTrans, pWorldViewProj, &matInvViewPort );
    D3DXMatrixTranspose( &matTrans, &matTrans );

    pDevice->SetVertexShaderConstantF( 0, ( FLOAT* )&matTrans, 4 );


    //
    // We're given a clipping rectangle. Xui clips top and bottom.  We'll use 
    // clip planes here to do the clipping
    //

    FLOAT oldClipPlane0[4], oldClipPlane1[4];
    DWORD oldClipState;

    D3DXVECTOR3 vTopLeft( ( FLOAT )pClipRect->left, ( FLOAT )pClipRect->top, 0 );
    D3DXVec3TransformCoord( &vTopLeft, &vTopLeft, pWorldViewProj );
    D3DXVec3TransformCoord( &vTopLeft, &vTopLeft, &matInvViewPort );
    D3DXPLANE clipPlaneTop( 0, -1, 0, vTopLeft.y );

    D3DXVECTOR3 vBottomRight( ( FLOAT )pClipRect->right, ( FLOAT )pClipRect->bottom, 0 );
    D3DXVec3TransformCoord( &vBottomRight, &vBottomRight, pWorldViewProj );
    D3DXVec3TransformCoord( &vBottomRight, &vBottomRight, &matInvViewPort );
    D3DXPLANE clipPlaneBottom( 0, 1, 0, -vBottomRight.y );

    pDevice->GetClipPlane( 0, oldClipPlane0 );
    pDevice->GetClipPlane( 1, oldClipPlane1 );
    pDevice->GetRenderState( D3DRS_CLIPPLANEENABLE, &oldClipState );

    pDevice->SetClipPlane( 0, clipPlaneTop );
    pDevice->SetClipPlane( 1, clipPlaneBottom );
    pDevice->SetRenderState( D3DRS_CLIPPLANEENABLE, D3DCLIPPLANE0 | D3DCLIPPLANE1 );

    //
    // Render the characters
    //

    for( DWORD i = 0; i < dwCount; i++ )
    {
        RenderCharacter( pDevice, pCharData[i].wch, fPointSize,
                         pCharData[i].x, pCharData[i].y, pCharData[i].dwColor );
    }

    //
    // Reset old clip plane info
    //

    pDevice->SetClipPlane( 0, oldClipPlane0 );
    pDevice->SetClipPlane( 1, oldClipPlane1 );
    pDevice->SetRenderState( D3DRS_CLIPPLANEENABLE, oldClipState );

    //
    // Release the IDirect3DDevice9 pointer
    //

    pDevice->Release();

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: DrawCharToTexture (IXuiFontRenderer)
// Desc: Render a single character to XUI's glyph texture cache.  The glyph cache is an 
//       alpha-only texture, D3DFMT_LIN_A8.  We'll render to a multisampled render 
//       target, resolve to a tiled texture, untile to a linear texture, then copy to 
//       XUI's glyph texture.  This sample isn't about performance...
//       Renderers should clear the block (x, y, x+width, y+height) on pTexture, then 
//       render to (x+insetX, y+insetY, x+width-insetX, y+height-insetY.
//--------------------------------------------------------------------------------------
HRESULT
STDMETHODCALLTYPE MyXuiFontRenderer::DrawCharToTexture( HFONTOBJ hFont, WCHAR wch, HXUIDC hDC,
                                                        IXuiTexture* pXuiTexture, UINT x, UINT y,
                                                        UINT width, UINT height, UINT insetX, UINT insetY )
{
    if( hFont == 0 || pXuiTexture == NULL )
    {
        return E_INVALIDARG;
    }

    IDirect3DDevice9* pDevice = NULL;
    HRESULT hr = XuiRenderGetDevice( hDC, &pDevice );
    if( FAILED( hr ) || pDevice == NULL )
    {
        return E_INVALIDARG;
    }

    IDirect3DTexture9* pTexture = pXuiTexture->GetD3D9Texture();
    if( NULL == pTexture )
    {
        return E_INVALIDARG;
    }

    FLOAT fPointSize = ( ( FontInfo* )hFont )->m_fPointSize;
    D3DXCOLOR color( 0xFFFFFFFF );

    // Get current state
    IDirect3DVertexShader9* pOldVertexShader = NULL;
    IDirect3DPixelShader9* pOldPixelShader = NULL;
    IDirect3DVertexDeclaration9* pOldVertexDeclaration = NULL;
    pDevice->GetVertexShader( &pOldVertexShader );
    pDevice->GetPixelShader( &pOldPixelShader );
    pDevice->GetVertexDeclaration( &pOldVertexDeclaration );

    IDirect3DSurface9* pRenderTarget;
    pDevice->CreateRenderTarget( 256, 256, D3DFMT_A8R8G8B8, D3DMULTISAMPLE_4_SAMPLES,
                                 0, 0, &pRenderTarget, NULL );

    // Save the old render target
    D3DSurface* pOldRenderTarget;
    pDevice->GetRenderTarget( 0, &pOldRenderTarget );

    // Set the render target
    pDevice->SetRenderTarget( 0, pRenderTarget );

    // Clear the target
    pDevice->Clear( 0, NULL, D3DCLEAR_TARGET, 0x0000000, 1.0f, 0 );

    // Render the character
    pDevice->SetVertexShader( m_pVertexShader );
    pDevice->SetPixelShader( m_pPixelShader );
    pDevice->SetVertexDeclaration( m_pVertexDeclaration );

    D3DXMATRIX matInvViewPort;
    D3DXMatrixIdentity( &matInvViewPort );
    D3DVIEWPORT9 viewPort;
    pDevice->GetViewport( &viewPort );
    matInvViewPort._11 = 2.0f / viewPort.Width;
    matInvViewPort._22 = -2.0f / viewPort.Height;
    matInvViewPort._33 = 1.0f / ( viewPort.MaxZ - viewPort.MinZ );
    matInvViewPort._41 = -1;
    matInvViewPort._42 = 1;

    D3DXMATRIX matTrans = matInvViewPort;
    D3DXMatrixTranspose( &matTrans, &matTrans );
    pDevice->SetVertexShaderConstantF( 0, ( FLOAT* )&matTrans, 4 );

    // Offset by one to allow for multisampled edges
    RenderCharacter( pDevice, wch, fPointSize, ( FLOAT )( insetX + 1 ), ( FLOAT )( insetY + 1 ), color );

    // Resolve the render target to a tiled texture
    IDirect3DTexture9* pTiledTexture, *pLinearTexture;
    pDevice->CreateTexture( 256, 256, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &pTiledTexture, NULL );
    pDevice->CreateTexture( 256, 256, 1, 0, D3DFMT_LIN_A8R8G8B8, D3DPOOL_MANAGED, &pLinearTexture, NULL );
    pDevice->Resolve( D3DRESOLVE_RENDERTARGET0, NULL, pTiledTexture, NULL, 0, 0, NULL, 0, 0, NULL );

    // Set the render target back to the back buffer
    pDevice->SetRenderTarget( 0, pOldRenderTarget );
    pRenderTarget->Release();

    // Copy tiled texture to a linear texture
    D3DLOCKED_RECT tiledLockedRect, linearLockedRect;
    pTiledTexture->LockRect( 0, &tiledLockedRect, NULL, 0 );
    pLinearTexture->LockRect( 0, &linearLockedRect, NULL, 0 );

    RECT srcRect =
    {
        0, 0, 256, 256
    };
    POINT dstPoint =
    {
        0, 0
    };
    XGUntileSurface( linearLockedRect.pBits, linearLockedRect.Pitch, &dstPoint,
                     tiledLockedRect.pBits, 256, 256, &srcRect, 4 );

    // Copy linear texture to XUI's texture cache
    XGTEXTURE_DESC descDst;
    XGGetTextureDesc( pTexture, 0, &descDst );

    D3DLOCKED_RECT dstLockedRect;
    pTexture->LockRect( 0, &dstLockedRect, NULL, 0 );

    dstPoint.x = x;
    dstPoint.y = y;
    srcRect.right = width;
    srcRect.bottom = height;

    XGCopySurface( dstLockedRect.pBits, dstLockedRect.Pitch, descDst.Width, descDst.Height, descDst.Format, &dstPoint,
                   linearLockedRect.pBits, linearLockedRect.Pitch, D3DFMT_LIN_A8R8G8B8, &srcRect, 0, 0 );


    pTexture->UnlockRect( 0 );
    pLinearTexture->UnlockRect( 0 );
    pTiledTexture->UnlockRect( 0 );
    pTiledTexture->Release();
    pLinearTexture->Release();

    // Restore state
    pDevice->SetVertexShader( pOldVertexShader );
    pDevice->SetPixelShader( pOldPixelShader );
    pDevice->SetVertexDeclaration( pOldVertexDeclaration );

    return S_OK;
}

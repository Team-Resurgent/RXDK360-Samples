//--------------------------------------------------------------------------------------
// Shaders for the GPUMemoryMove sample
//--------------------------------------------------------------------------------------

static float4 const01= {0, 1, 0, 0};

extern float4 streamConstant    : register(c0);
extern int offsetInVerts        : register(c1);

struct DUMMY_VERTEX
{
    float4  vData0  : POSITION0;
    float4  vData1  : POSITION1;
    float4  vData2  : POSITION2;
    float4  vData3  : POSITION3;
    float4  vData4  : POSITION4;
    float4  vData5  : POSITION5;
    float4  vData6  : POSITION6;
    float4  vData7  : POSITION7;
};

//--------------------------------------------------------------------------------------
// Name: MemoryMoveVS()
// Desc: Copies 64 bytes of arbitrary data from one place to another using memexport.
//       We use 8 vertex elements and 8 memexports for efficiency.  At 4 memexports,
//       the shader is set-up bound (number of verts).  At 8 memexports, it becomes
//       bandwidth bound, which is what we want.
//--------------------------------------------------------------------------------------
void MemoryMoveVS( DUMMY_VERTEX vertex, int index : INDEX )
{
    float4 output0 = vertex.vData0;
    float4 output1 = vertex.vData1;
    float4 output2 = vertex.vData2;
    float4 output3 = vertex.vData3;
    float4 output4 = vertex.vData4;
    float4 output5 = vertex.vData5;
    float4 output6 = vertex.vData6;
    float4 output7 = vertex.vData7;
    int outputIndex0 = 8 * ( index - offsetInVerts );
    asm 
    {
        alloc export=2
        mad eA, outputIndex0, const01, streamConstant
        mov eM0, output0
        mov eM1, output1
        mov eM2, output2
        mov eM3, output3
    };
    int outputIndex1 = outputIndex0 + 4;
    asm 
    {
        alloc export=2
        mad eA, outputIndex1, const01, streamConstant
        mov eM0, output4
        mov eM1, output5
        mov eM2, output6
        mov eM3, output7
    };
    return;
}

//--------------------------------------------------------------------------------------
// Name: DummyPS()
// Desc: Can't do memexport in the vertex shader while pixel shader is NULL.
//       So just need any pixel shader at all.
//--------------------------------------------------------------------------------------
float4 DummyPS( void ) : COLOR
{
    return 0.0f;
}


//--------------------------------------------------------------------------------------
// SimpleTessellation.hlsl
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------


// Composite world, view, projection transform matrix
float4x4 CompositeTransform : register(c0);


//--------------------------------------------------------------------------------------
// Name: LineListVS
// Desc: Vertex shader for line lists
//--------------------------------------------------------------------------------------
void LineListVS( in  int2   vIndex      : INDEX,
                 in  float  vU          : BARYCENTRIC,
                 out float4 oPosition   : POSITION,
                 out float4 oColor      : COLOR )
{
    // Fetch the corners of the base line
    float4 pos0, color0, pos1, color1;
    asm {
        vfetch pos0, vIndex.x, position
        vfetch color0, vIndex.x, color

        vfetch pos1, vIndex.y, position
        vfetch color1, vIndex.y, color
    };
    
    // Compute the weights from the parametric coordinate
    float2 weights = { 1.0 - vU, vU };
    
    // Weight them by the barycentric coordinates
    float4 pos = pos0 * weights.x + pos1 * weights.y;
    oColor = color0 * weights.x + color1 * weights.y;

    oPosition = mul( CompositeTransform, pos );
}


//--------------------------------------------------------------------------------------
// Name: TriListVS
// Desc: Vertex shader for triangle lists
//--------------------------------------------------------------------------------------
void TriListVS( in  int3   vIndex       : INDEX,
                in  float3 vUVW         : BARYCENTRIC,
                out float4 oPosition    : POSITION,
                out float4 oColor       : COLOR )
{
    // Fetch the corners of the base triangle
    float4 pos0, color0, pos1, color1, pos2, color2;
    asm {
        vfetch pos0, vIndex.x, position
        vfetch color0, vIndex.x, color

        vfetch pos1, vIndex.y, position
        vfetch color1, vIndex.y, color
        
        vfetch pos2, vIndex.z, position
        vfetch color2, vIndex.z, color
    };
    
    // Weight them by the barycentric coordinates
    float4 pos = pos0 * vUVW.z + pos1 * vUVW.y + pos2 * vUVW.x;
    oColor = color0 * vUVW.z + color1 * vUVW.y + color2 * vUVW.x;

    oPosition = mul( CompositeTransform, pos );
}


//--------------------------------------------------------------------------------------
// Name: QuadListVS
// Desc: Vertex shader for quad lists
//--------------------------------------------------------------------------------------
void QuadListVS( in  int4   vIndex      : INDEX,
                 in  float2 vUV         : BARYCENTRIC,
                 out float4 oPosition   : POSITION,
                 out float4 oColor      : COLOR )
{
    // Fetch the corners of the base quad.
    float4 pos0, color0, pos1, color1, pos2, color2, pos3, color3;
    asm {
        vfetch pos0, vIndex.x, position
        vfetch color0, vIndex.x, color

        vfetch pos1, vIndex.y, position
        vfetch color1, vIndex.y, color
        
        vfetch pos2, vIndex.z, position
        vfetch color2, vIndex.z, color

        vfetch pos3, vIndex.w, position
        vfetch color3, vIndex.w, color
    };
    
    // Compute the weights from the parametric coordinates
    float4 weights = { (1.0 - vUV.x) * (1.0 - vUV.y),
                       vUV.x         * (1.0 - vUV.y),
                       (1.0 - vUV.x) * vUV.y,
                       vUV.x         * vUV.y };
                       
    // Weight them by the parametric coordinates
    float4 pos = pos0 * weights.x + pos1 * weights.y + pos2 * weights.w + pos3 * weights.z;
    oColor = color0 * weights.x + color1 * weights.y + color2 * weights.w + color3 * weights.z;

    oPosition = mul( CompositeTransform, pos );
}


//--------------------------------------------------------------------------------------
// Name: LinePatchVS
// Desc: Vertex shader for line patches
//--------------------------------------------------------------------------------------
void LinePatchVS( in  int    vIndex     : INDEX,
                  in  float  vU         : BARYCENTRIC,
                  in  int    vQuadID    : QUADID,
                  out float4 oPosition  : POSITION,
                  out float4 oColor     : COLOR )
{
    // Fetch the corners of the base line
    float4 pos0, color0, pos1, color1;
    asm {
        vfetch pos0, vIndex.x, position0
        vfetch color0, vIndex.x, color0

        vfetch pos1, vIndex.x, position1
        vfetch color1, vIndex.x, color1
    };
    
    // Compute the weights from the parametric coordinate
    float2 weights = { 1.0 - vU, vU };
    
    // Re-order the weights based on the QuadID
    float2 coeffs = weights * ( vQuadID == 0 );
    coeffs += weights.yx * ( vQuadID == 1 );
    
    // Weight them by the barycentric coordinates
    float4 pos = pos0 * coeffs.x + pos1 * coeffs.y;
    oColor = color0 * coeffs.x + color1 * coeffs.y;

    oPosition = mul( CompositeTransform, pos );
}


//--------------------------------------------------------------------------------------
// Name: TriPatchVS
// Desc: Vertex shader for triangle patches
//--------------------------------------------------------------------------------------
void TriPatchVS( in  int    vIndex      : INDEX,
                 in  float3 vUVW        : BARYCENTRIC,
                 in  int    vQuadID     : QUADID,
                 out float4 oPosition   : POSITION,
                 out float4 oColor      : COLOR )
{
    // Fetch the corners of the base triangle
    float4 pos0, color0, pos1, color1, pos2, color2;
    asm {
        vfetch pos0, vIndex.x, position0
        vfetch color0, vIndex.x, color0

        vfetch pos1, vIndex.x, position1
        vfetch color1, vIndex.x, color1
        
        vfetch pos2, vIndex.x, position2
        vfetch color2, vIndex.x, color2
    };

    // Re-order the weights based on the QuadID
    float3 uvw = vUVW * (vQuadID == 0);
    uvw += vUVW.zxy * (vQuadID == 1);
    uvw += vUVW.yzx * (vQuadID == 2); 
    uvw += vUVW.xzy * (vQuadID == 4); 
    uvw += vUVW.yxz * (vQuadID == 5); 
    uvw += vUVW.zyx * (vQuadID == 6); 
    
    // Weight them by the barycentric coordinates
    float4 pos = pos0 * uvw.z + pos1 * uvw.y + pos2 * uvw.x;
    oColor = color0 * uvw.z + color1 * uvw.y + color2 * uvw.x;

    oPosition = mul( CompositeTransform, pos );
}


//--------------------------------------------------------------------------------------
// Name: QuadPatchVS
// Desc: Vertex shader for quad patches
//--------------------------------------------------------------------------------------
void QuadPatchVS( in  int    vIndex     : INDEX,
                  in  float2 vUV        : BARYCENTRIC,
                  in  int    vQuadID    : QUADID,
                  out float4 oPosition  : POSITION,
                  out float4 oColor     : COLOR )
{
    // Fetch the corners of the base quad.
    float4 pos0, color0, pos1, color1, pos2, color2, pos3, color3;
    asm {
        vfetch pos0, vIndex.x, position0
        vfetch color0, vIndex.x, color0

        vfetch pos1, vIndex.x, position1
        vfetch color1, vIndex.x, color1
        
        vfetch pos2, vIndex.x, position2
        vfetch color2, vIndex.x, color2

        vfetch pos3, vIndex.x, position3
        vfetch color3, vIndex.x, color3
    };
    
    // Compute the weights from the parametric coordinates
    float4 weights = { (1.0 - vUV.x) * (1.0 - vUV.y),
                       vUV.x         * (1.0 - vUV.y),
                       (1.0 - vUV.x) * vUV.y,
                       vUV.x         * vUV.y };
    
    // Re-order the weights based on the QuadID
    float4 coeffs = weights * ( vQuadID == 0 );
    coeffs += weights.yxwz * ( vQuadID == 1 ); 
    coeffs += weights.wzyx * ( vQuadID == 2 );
    coeffs += weights.zwxy * ( vQuadID == 3 );

    // Weight them by the parametric coordinates
    float4 pos = pos0 * coeffs.x + pos1 * coeffs.y + pos2 * coeffs.z + pos3 * coeffs.w;
    oColor = color0 * coeffs.x + color1 * coeffs.y + color2 * coeffs.z + color3 * coeffs.w;

    oPosition = mul( CompositeTransform, pos );
}


//--------------------------------------------------------------------------------------
// Name: TestPS
// Desc: Simple pixel shader that just passes through the color
//--------------------------------------------------------------------------------------
float4 TestPS( float4 vColor : COLOR ) : COLOR
{
    return vColor;
}

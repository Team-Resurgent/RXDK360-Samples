//--------------------------------------------------------------------------------------
// InstanceMesh.fx
//
// This FXLite effect draws instanced indexed meshes.  The vertex shader takes an
// automatically generated index as input, and performs dependent vertex fetches
// to obtain instance data, index data and then the actual vertex data.  Once the 
// vertex data is fetched, the vertex shader resembles any other standard vertex shader.
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

// World * view * projection matrix
shared float4x4     world_view_proj_matrix : register(c0);

// Instance data holds the index count of one instance, as well as an index offset
// for drawing index buffer subsets.
float4              instance_data : register(c4);

struct VS_OUTPUT
{
    float4  Position: POSITION;
    float4  Color : COLOR;
    float2  Tex0 : TEXCOORD0;
};

struct PS_INPUT
{
    float4  Color : COLOR;
    float2  Tex0 : TEXCOORD0;
};

VS_OUTPUT vs_main( int Index : INDEX )
{
    // Compute the instance index
    int iInstanceIndex = ( Index + 0.5 ) / instance_data.x;
    
    // Fetch the instance data
    float4 vInstancePositionScale;
    asm
    {
        vfetch vInstancePositionScale, iInstanceIndex, position2;
    };
    
    // Compute the mesh index - this is the index to fetch within the current instance
    int iMeshIndex = Index - ( iInstanceIndex * instance_data.x ) + instance_data.y;
    
    // Fetch the mesh index value
    int4 iMeshIndexValue = 0;
    asm
    {
        vfetch iMeshIndexValue, iMeshIndex, position1;
    };
    
    // Now fetch the actual mesh vertex data
    float4 vMeshPosition;
    float4 vMeshColor;
    float4 vMeshUV;
    asm
    {
        vfetch vMeshPosition, iMeshIndexValue.x, position0;
        vfetch vMeshColor, iMeshIndexValue.x, color0;
        vfetch vMeshUV, iMeshIndexValue.x, texcoord0;
    };
    
    // Combine the instance position with the mesh position
    vMeshPosition.xyz *= vInstancePositionScale.w;
    vMeshPosition.xyz += vInstancePositionScale.xyz;
    
    // Output the vertex data
    // This part looks like a regular vertex shader.
    VS_OUTPUT Out;
    Out.Position = mul( vMeshPosition, world_view_proj_matrix );
    Out.Tex0 = vMeshUV;
    Out.Color = vMeshColor;
    return Out;
};

float4 ps_main( float4 Color : COLOR ) : COLOR
{
    return Color;
};

technique InstancedMesh
{
    pass
    {
        cullmode = ccw;
        fillmode = solid;
        zenable = true;
        zwriteenable = true;
        zfunc = lessequal;
        alphablendenable = true;
        srcblend = srcalpha;
        destblend = zero;
        vertexshader = compile vs_3_0 vs_main();
        pixelshader = compile ps_3_0 ps_main();
    }
}

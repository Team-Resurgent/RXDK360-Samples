//--------------------------------------------------------------------------------------
// Instancing.fx
//
// This FXLite effect draws instanced meshes.  Each technique performs instancing in 
// a unique way.
//
// XNA Developer Connection
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#define MAX_GPU_CONST_BATCH_SIZE 200

// World * view * projection matrix
shared float4x4     world_view_proj_matrix : register( c0 );

// Instance data holds the index count of one instance, as well as an index offset
// for drawing index buffer subsets.
float4              instancing_params : register( c4 );
float4              instance_data : register( c5 );
float4              instance_data_batched[ MAX_GPU_CONST_BATCH_SIZE ] : register( c6 );

//--------------------------------------------------------------------------------------

sampler crater_sampler : register(s0) = sampler_state
{
    MipFilter   = LINEAR; 
    MinFilter   = LINEAR; 
    MagFilter   = LINEAR; 
    AddressU    = WRAP; 
    AddressV    = WRAP; 
};

sampler displacement_sampler : register(s1) = sampler_state 
{ 
    MipFilter   = LINEAR; 
    MinFilter   = LINEAR; 
    MagFilter   = LINEAR; 
    AddressU    = WRAP; 
    AddressV    = WRAP; 
};

struct VS_OUTPUT
{
    float4  vPosition : POSITION;
    float4  vColor    : COLOR;
    float2  vUV       : TEXCOORD0_centroid;
};

struct PS_INPUT
{
    float4  vColor   : COLOR;
    float2  vUV      : TEXCOORD0_centroid;
};

const float PI      = 3.141592654f;
const float PIDIV2  = 1.570796327f;

VS_OUTPUT CalculateVSOutput( float4 vMeshPos, float2 vMeshUV, float3 vInstPos,
                             float fInstScale, float3 vNorm, bool bTransformInst );
                             
//--------------------------------------------------------------------------------------
// Name: vs_custom_vfetch()
// Desc: This method uses the automatically generated index ( which is simply 
//       incremented by one for each vertex processed ) to determine offsets into the
//       vertex and instance data.  A modulo operation is done on the index to determine
//       which instance to read data from, and this is passed to a set of vfetches.
//--------------------------------------------------------------------------------------
VS_OUTPUT vs_custom_vfetch( int nIndex : INDEX )
{
    float4 vMeshPos;
    float4 vMeshUV;
    float4 vInstancePositionScale;
    
    // Compute the instance index
    int nNumVertsPerInstance = instancing_params.x;
    int nInstanceIndex = ( nIndex + .5 ) / nNumVertsPerInstance;
    
    // Compute the index of the mesh index
    int nIndexOfIndex = nIndex - ( nInstanceIndex * nNumVertsPerInstance );

    // Fetch the mesh index
    int4 vMeshIndexValue;
    asm
    {
        // Fetch using the texture cache to reduce vertex cache stress
        vfetch vMeshIndexValue, nIndexOfIndex, position1, UseTextureCache = true;
    };
    
    // Now fetch the actual mesh vertex data
    asm
    {
        vfetch vMeshPos, vMeshIndexValue.x, position0;
        vfetch vMeshUV,  vMeshIndexValue.x, texcoord0;
    };
    
    // Fetch the instance data
    asm
    {
        vfetch vInstancePositionScale, nInstanceIndex, position2;
    };
    
    float3 vInstPos     = vInstancePositionScale.xyz;
    float  fInstScale   = vInstancePositionScale.w;
    float3 vMeshNorm    = vMeshPos;  
      
    return CalculateVSOutput( vMeshPos, vMeshUV, vInstPos, fInstScale, vMeshNorm, true );
};


//--------------------------------------------------------------------------------------
// Name: vs_one_per_draw()
// Desc: This method draws one instance per draw call in order to use the 
//       post-transform vertex cache more efficiently.  Since each vertex can map to
//       multiple indices they can hit the post-transform cache.  This does cause some
//       overhead as each instance requires its own draw call.  Instance data is stored
//       either in GPU constants or an extra vertex buffer.
//--------------------------------------------------------------------------------------
VS_OUTPUT vs_one_per_draw( float4 vMeshPos     : POSITION0,
                           float4 vMeshUV      : TEXCOORD0, 
                           uniform bool bUseVFetch )
{
    // Get the instance data
    float4 vInstPosScale;
    if( bUseVFetch )
    {
        const int nInstIndex = 0;   // only one instance per draw call, so this is always 0
        asm
        {
            vfetch vInstPosScale, nInstIndex, position2;
        };
    }
    else
    {
        vInstPosScale = instance_data;
    }
    
    float3 vInstPos   = vInstPosScale.xyz;
    float  fInstScale = vInstPosScale.w;
    float3 vMeshNorm  = vMeshPos.xyz;
    
    return CalculateVSOutput( vMeshPos, vMeshUV, vInstPos, fInstScale, vMeshNorm, true );
}


//--------------------------------------------------------------------------------------
// Name: vs_batched()
// Desc: This method batches groups of instances together.  To perform instancing, the
//       index buffer holds one copy of the index data for each instance in the batch.
//       This allows use of the post-transform cache while still allowing multiple
//       instances to be drawn per draw call.  Instance data is stored
//       either in GPU constants or an extra vertex buffer.
//--------------------------------------------------------------------------------------
VS_OUTPUT vs_batched( int nIndex : INDEX, uniform bool bUseVFetch )
{
    // Compute the instance index
    int nNumIndicesPerInstance = instancing_params.x;
    int nInstIndex = ( nIndex + 0.5 ) / nNumIndicesPerInstance;
    int nMeshIndex = nIndex - nInstIndex * nNumIndicesPerInstance;

    // Fetch the mesh vertex data
    float4 vMeshPos;
    float4 vMeshUV;
    asm
    {
        vfetch vMeshPos, nMeshIndex, position0;
        vfetch vMeshUV,  nMeshIndex, texcoord0;
    };
    
    // Get the instance data
    float4 vInstPosScale;
    if( bUseVFetch )
    {   
        // Fetch the instance data
        asm
        {
            vfetch vInstPosScale, nInstIndex, position2;
        };
    }
    else
    {
        // Get the instance data from the GPU constants
        vInstPosScale = instance_data_batched[ nInstIndex ];
    }
        
    float3 vInstPos   = vInstPosScale.xyz;
    float  fInstScale = vInstPosScale.w;
    float3 vMeshNorm  = vMeshPos;
    
    return CalculateVSOutput( vMeshPos, vMeshUV, vInstPos, fInstScale, vMeshNorm, true );
} 


//--------------------------------------------------------------------------------------
// Name: vs_tessellator()
// Desc: In the tessellator method, each quad patch output from the tessellator is 
//       treated as an instance. The barycentric coordinates are used to generate vertex
//       positions on a sphere, and then pass this with the per instance data to the
//       transformation and lighting function
//--------------------------------------------------------------------------------------
VS_OUTPUT vs_tessellator(   int    nIndex     : INDEX,
                            float2 vPatchUV   : BARYCENTRIC,
                            int    nQuadID    : QUADID,
                            uniform bool bUseVFetch )
{
    // Convert the UVs from the tesselator output range to a more useful range
    float2 vMeshUV = float2(        vPatchUV.x, 1.0f - vPatchUV.y ) * ( nQuadID == 0 ) +
                     float2( 1.0f - vPatchUV.x, 1.0f - vPatchUV.y ) * ( nQuadID == 1 ) +
                     float2( 1.0f - vPatchUV.x,        vPatchUV.y ) * ( nQuadID == 2 ) +
                     float2(        vPatchUV.x,        vPatchUV.y ) * ( nQuadID == 3 );
              
    // Make a sphere   
    float fTheta = ( vMeshUV.x ) * 2.0f * PI;
    float fPhi   = ( vMeshUV.y * 2.0f - 1.0f ) * PIDIV2;

    float4 vMeshPos;
    vMeshPos.x = cos( fTheta ) * cos( fPhi );
    vMeshPos.z = sin( fTheta ) * cos( fPhi );
    vMeshPos.y = sin( fPhi );
    vMeshPos.w = 1.0f;

    float4 vInstPosScale;
    if( bUseVFetch )
    {
        // fetch the instance data
        int numIndicesPerInstance = instancing_params.x;
        int iInstanceIndex = nIndex;    // nIndex is the index of the patch, not the vert
        asm
        {
            vfetch vInstPosScale, iInstanceIndex, position0;
        };    
    }
    else
    {
        // The instance data is pulled off of GPU constants
        vInstPosScale = instance_data_batched[ nIndex ];
    }
    
    float3 vInstPos   = vInstPosScale.xyz;
    float  fInstScale = vInstPosScale.w;
    float3 vMeshNorm  = vMeshPos;
    
    return CalculateVSOutput( vMeshPos, vMeshUV, vInstPos, fInstScale, vMeshNorm, true );
}

//--------------------------------------------------------------------------------------
// Name: vs_XPS()
// Desc: When using XPS, the per instance vertex data has already been calculated on the
//       CPU, so all that's left to do is transform and light the vertex
//--------------------------------------------------------------------------------------
VS_OUTPUT vs_XPS( float4 vMeshPos   : POSITION,
                  float2 vMeshUV    : TEXCOORD,
                  float  fInstScale : COLOR,
                  float3 vMeshNorm  : NORMAL )  
{
    
     
    return CalculateVSOutput( vMeshPos, vMeshUV, float3( 0,0,0 ), fInstScale, vMeshNorm, false );
}
 
//--------------------------------------------------------------------------------------
// Name: CalcLighting()
// Desc: Generic lighting function
//--------------------------------------------------------------------------------------
const float3 g_lightDir = float3( -.707f, 0.0f, .707f);
float4 CalcLighting( float3 vNormal )
{
    float4 result;
    
    // Blown out lighting to give a more "planetary" feel
    result = lerp( clamp( dot( vNormal, g_lightDir )*2, 0, 2), 1.0f, .2f);
    result.a = 1.0f;
    return result;
}

//--------------------------------------------------------------------------------------
// Name: CalculateVSOutput()
// Desc: Caclulate the vertex shader output.  This is called by each method after
//       fetching the instance and vertex data.
//--------------------------------------------------------------------------------------
VS_OUTPUT CalculateVSOutput( float4 vMeshPos,
                             float2 vMeshUV,
                             float3 vInstPos,
                             float  fInstScale,
                             float3 vMeshNorm,
                             bool   bTransformInstance )
{
    VS_OUTPUT Out;
    float3 normal = normalize(vMeshNorm.xyz );
    
    // offset the uv to vary between instances, reuse the scale as the offset amount
    vMeshUV.xy += fInstScale * 100;        

    // Displace the vertex position with the displacement map
    const float fDispScaler = .7f;
    float4 vDispSample;
    asm
    {
        tfetch2D vDispSample, vMeshUV, displacement_sampler, UseComputedLOD = false
    };
    
    // set displacement range to [-1,1] * fDispScaler
    float fDisplacement = ( vDispSample.x * 2.0f - 1.0f ) * fDispScaler;   
    
    // If the instance hasn't been transformed yet ( i.e. on the CPU ), transform it
    if( bTransformInstance )
    {
        // Use the instance position and scale to position the mesh
        vMeshPos.xyz *= fInstScale;
        vMeshPos.xyz += vInstPos;
    }

    vMeshPos.xyz += vMeshNorm * fDisplacement * fInstScale;
    
    // Output the vertex data
    Out.vPosition = mul( vMeshPos, world_view_proj_matrix );
    Out.vUV = vMeshUV;

    // Lighting    
    Out.vColor = CalcLighting( normal );
    Out.vColor.xyz *= vDispSample.x;   // Fake ambient occlusion with displacement
    
    return Out;
}
    
//--------------------------------------------------------------------------------------
// Name: ps_main()
// Desc: Generic pixel shader
//--------------------------------------------------------------------------------------
float4 ps_main( PS_INPUT In ) : COLOR
{
    return In.vColor * tex2D( crater_sampler, In.vUV );
};

technique OnePerDraw_VFetchInstanceData
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
        vertexshader = compile vs_3_0 vs_one_per_draw( true );
        pixelshader = compile ps_3_0 ps_main();
    }
}

technique OnePerDraw_ConstantInstanceData
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
        vertexshader = compile vs_3_0 vs_one_per_draw( false );
        pixelshader = compile ps_3_0 ps_main();
    }
}

technique CustomVFetch
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
        vertexshader = compile vs_3_0 vs_custom_vfetch( );
        pixelshader = compile ps_3_0 ps_main();
    }
}

technique Batched_VFetchInstanceData
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
        vertexshader = compile vs_3_0 vs_batched( true );
        pixelshader = compile ps_3_0 ps_main( );
    }
}

technique Batched_ConstantInstanceData
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
        vertexshader = compile vs_3_0 vs_batched( false );
        pixelshader = compile ps_3_0 ps_main();
    }
}

technique XPS
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
        vertexshader = compile vs_3_0 vs_XPS();
        pixelshader = compile ps_3_0 ps_main();
    }
}

technique Tessellator_VFetchInstanceData
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
        tessellationmode = continuous;
        vertexshader = compile vs_3_0 vs_tessellator( true );
        pixelshader = compile ps_3_0 ps_main();
    }
}

technique Tessellator_ConstantInstanceData
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
        tessellationmode = continuous;
        vertexshader = compile vs_3_0 vs_tessellator( false );
        pixelshader = compile ps_3_0 ps_main();
    }
}


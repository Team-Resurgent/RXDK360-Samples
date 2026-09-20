const float4x4 c_modelview    : register( c0 );
const float4x4 c_projection   : register( c4 );

struct Vertex_c
{
    float4      position        : POSITION;         // stream 0
    float3      normal          : NORMAL;           // stream 0
    float4      blendWeight     : BLENDWEIGHT;      // stream 0
    int4        blendIndex      : BLENDINDICES;     // stream 0
    float4      color           : COLOR;            // stream 0
    float2      uv0             : TEXCOORD0;        // stream 0
};

void
ApplySkinning(
	Vertex_c	vertex,
	out float3	viewPos,
	out float4	projPos,
	out float3	viewNormal )
{
	float4 OutPos = { 0.0f, 0.0f, 0.0f, 0.0f };
	float3 Normal = 0;
	
	float4 skinningMatrix[ 3 ] = 
	{ 
	    float4( 0, 0, 0, 0 ), 
	    float4( 0, 0, 0, 0 ),
	    float4( 0, 0, 0, 0 )
    };
	
	for( int i = 0; i < 4; ++i )
    {
		[ predicateBlock ]
		if( vertex.blendWeight[ i ] > 0 )
		{
			float4 fetch1;
			float4 fetch2;
			float4 fetch3;
			
			int blendIndex = vertex.blendIndex[ i ];
			
			asm
			{
				vfetch fetch1, blendIndex, texcoord6, UseTextureCache=true;		
				vfetch fetch2, blendIndex, texcoord7, UseTextureCache=true;        
				vfetch fetch3, blendIndex, texcoord8, UseTextureCache=true;				
			};
			
			skinningMatrix[ 0 ] += vertex.blendWeight[ i ] * fetch1;
			skinningMatrix[ 1 ] += vertex.blendWeight[ i ] * fetch2;
			skinningMatrix[ 2 ] += vertex.blendWeight[ i ] * fetch3;
		}
    }
    
    OutPos.x = dot( skinningMatrix[ 0 ], vertex.position );
    OutPos.y = dot( skinningMatrix[ 1 ], vertex.position );
    OutPos.z = dot( skinningMatrix[ 2 ], vertex.position );
    OutPos.w = 1;
    
    Normal.x = dot( skinningMatrix[ 0 ].xyz, vertex.normal );
    Normal.y = dot( skinningMatrix[ 1 ].xyz, vertex.normal );
    Normal.z = dot( skinningMatrix[ 2 ].xyz, vertex.normal );
    
    float4 viewPos1 = mul( c_modelview, OutPos );
    viewPos = viewPos1.xyz;
	projPos = mul( c_projection, viewPos1 );
	viewNormal = mul( c_modelview, Normal );
}


//--------------------------------------------------------------------------------------
// ChessSet.h
//
// Code to render a chessset
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------


//--------------------------------------------------------------------------------------
// External variables
//--------------------------------------------------------------------------------------
extern XMMATRIX g_matViewProj;          // Global view*projection matrix
extern XMMATRIX g_matShadowViewProj;    // Global shadow view*projection matrix
extern XMVECTOR g_vLightDirection;      // Glogal light direction
extern XMVECTOR g_vViewPosition;        // Global viewer position


//--------------------------------------------------------------------------------------
// Name: class CustomMesh
// Desc: Overridden mesh class for rendering a mesh using a custom shader
//--------------------------------------------------------------------------------------
class CustomMesh : public ATG::Mesh
{
public:
    BOOL RenderCallback( DWORD dwSubset, const ATG::MESH_SUBSET* pSubset, DWORD dwFlags )
    {
        // Pass the  matrices to the vertex shader
        const DWORD VSCONST_matWorldView = 0;  // World-View matrix
        const DWORD VSCONST_matShadow = 4;  // Shadow-Map matrix
        const DWORD VSCONST_vLightDir = 8;  // Light direction in object space
        const DWORD VSCONST_vViewPos = 9;  // View position in object space

        XMMATRIX matWVP = m_matWorld * g_matViewProj;
        matWVP = XMMatrixTranspose( matWVP );

        XMMATRIX matShadowTex = m_matWorld * g_matShadowViewProj;
        matShadowTex = XMMatrixTranspose( matShadowTex );

        // Compute the inverse world matrix.
        XMVECTOR vDet;
        XMMATRIX matInvWorld = XMMatrixInverse( &vDet, m_matWorld );
        assert( vDet.x > 0.0f );

        // Transform the light direction into object space.
        XMVECTOR vLocalLightDir = XMVector4Transform( g_vLightDirection, matInvWorld );
        vLocalLightDir = XMVector4Normalize( vLocalLightDir );

        // Transform the viewposition into object space.
        XMVECTOR vLocalViewPos = XMVector4Transform( g_vViewPosition, matInvWorld );

        ATG::g_pd3dDevice->SetVertexShaderConstantF( VSCONST_matWorldView, ( float* )&matWVP, 4 );
        ATG::g_pd3dDevice->SetVertexShaderConstantF( VSCONST_matShadow, ( float* )&matShadowTex, 4 );
        ATG::g_pd3dDevice->SetVertexShaderConstantF( VSCONST_vLightDir, ( float* )&vLocalLightDir, 1 );
        ATG::g_pd3dDevice->SetVertexShaderConstantF( VSCONST_vViewPos, ( float* )&vLocalViewPos, 1 );

        return TRUE;
    }
};


//--------------------------------------------------------------------------------------
// Name: class ChessPiece
// Desc: Class to instance a chess piece
//--------------------------------------------------------------------------------------
class ChessPiece
{
public:

    enum PIECE
    {
        PAWN,
        ROOK,
        KNIGHT,
        BISHOP,
        QUEEN,
        KING,
        NUM_PIECES
    };
    enum COLOR
    {
        BLACK,
        WHITE
    };

    static CustomMesh   m_Mesh[NUM_PIECES];

    CustomMesh* m_pMesh;
    DWORD m_X;
    DWORD m_Z;
    COLOR m_Color;
    XMMATRIX m_matObject;

                        ChessPiece( COLOR color, PIECE piece, DWORD x, DWORD z )
                        {
                            m_pMesh = &m_Mesh[piece];
                            m_Color = color;
                            m_X = x - 1;
                            m_Z = z - 1;
                        }

    VOID                Render( XMMATRIX matWorld )
    {
        const DWORD PSCONST_TexModFactors = 3;
        static XMVECTOR TexModWhite = XMVectorSet( 0, 1, 0, 0 );
        static XMVECTOR TexModBlack = XMVectorSet( 1, -1, 0, 0 );

        if( m_Color == WHITE )
            ATG::g_pd3dDevice->SetPixelShaderConstantF( PSCONST_TexModFactors, ( FLOAT* )&TexModWhite, 1 );
        else // m_Color == BLACK
            ATG::g_pd3dDevice->SetPixelShaderConstantF( PSCONST_TexModFactors, ( FLOAT* )&TexModBlack, 1 );

        m_pMesh->m_matWorld = matWorld * m_matObject;
        m_pMesh->Render( ATG::MESH_NOMATERIALS );
    }
};

CustomMesh ChessPiece::m_Mesh[ ChessPiece::NUM_PIECES ];


//--------------------------------------------------------------------------------------
// Name: class ChessBoard
// Desc: Class to instance a chess board
//--------------------------------------------------------------------------------------
class ChessBoard
{
    XMVECTOR m_vCenter;           // Properties of the chess board
    XMVECTOR m_vOrigin;
    FLOAT m_fGridSize;

    CustomMesh m_Mesh;              // Mesh for the chess board
    ChessPiece* m_pChessPieces[32];  // Instances of the chess pieces

public:
    XMVECTOR    GetCenter()
    {
        return m_vCenter;
    }
    XMVECTOR    GetOrigin()
    {
        return m_vOrigin;
    }
    FLOAT       GetGridSize()
    {
        return m_fGridSize;
    }

    // Create the chess board
    HRESULT     Create( ATG::PackedResource* pResource )
    {
        // Create a mesh for the chess board
        if( FAILED( m_Mesh.Create( "d:\\Media\\Meshes\\ChessBoard.xbg", pResource ) ) )
            return ATGAPPERR_MEDIANOTFOUND;

        // Create meshes for the chess pieces
        if( FAILED( ChessPiece::m_Mesh[ChessPiece::PAWN].Create( "d:\\Media\\Meshes\\ChessPawn.xbg", pResource ) ) )
            return ATGAPPERR_MEDIANOTFOUND;
        if( FAILED( ChessPiece::m_Mesh[ChessPiece::ROOK].Create( "d:\\Media\\Meshes\\ChessRook.xbg", pResource ) ) )
            return ATGAPPERR_MEDIANOTFOUND;
        if( FAILED( ChessPiece::m_Mesh[ChessPiece::KNIGHT].Create( "d:\\Media\\Meshes\\ChessKnight.xbg",
                                                                   pResource ) ) )
            return ATGAPPERR_MEDIANOTFOUND;
        if( FAILED( ChessPiece::m_Mesh[ChessPiece::BISHOP].Create( "d:\\Media\\Meshes\\ChessBishop.xbg",
                                                                   pResource ) ) )
            return ATGAPPERR_MEDIANOTFOUND;
        if( FAILED( ChessPiece::m_Mesh[ChessPiece::QUEEN].Create( "d:\\Media\\Meshes\\ChessQueen.xbg", pResource ) ) )
            return ATGAPPERR_MEDIANOTFOUND;
        if( FAILED( ChessPiece::m_Mesh[ChessPiece::KING].Create( "d:\\Media\\Meshes\\ChessKing.xbg", pResource ) ) )
            return ATGAPPERR_MEDIANOTFOUND;

        // Clear the chess board
        ZeroMemory( m_pChessPieces, sizeof( m_pChessPieces ) );

        // Get chessboard dimensions from the proxy mesh
        XMVECTOR vMin, vMax;
        m_Mesh.ComputeMeshBoundingBox( m_Mesh.GetFrame( 0 )->m_pMeshData, m_Mesh.GetFrame( 0 )->m_matTransform, vMin,
                                       vMax );
        m_vCenter = ( vMax + vMin ) / 2;
        m_fGridSize = ( vMax.x - vMin.x ) / 8;
        m_vOrigin = m_vCenter - 3.5f * ( vMax - vMin ) / 8;
        m_Mesh.m_matWorld = XMMatrixIdentity();

        // Turn off display of the chessboard proxy mesh
        m_Mesh.GetFrame( 0 )->m_pMeshData->m_dwNumSubsets = 0;

        return S_OK;
    }

    // Add a piece to the chess board
    VOID        AddChessPiece( ChessPiece::COLOR color, ChessPiece::PIECE piece, DWORD x, DWORD z )
    {
        ChessPiece* pChessPiece = new ChessPiece( color, piece, x, z );

        // Find a free slot
        ChessPiece** pChessPieces = m_pChessPieces;
        while( ( *pChessPieces ) != NULL )
            pChessPieces++;
        ( *pChessPieces ) = pChessPiece;

        // Set the matrix transform for this piece's mesh
        XMVECTOR vObjectPos;
        vObjectPos.x = m_vOrigin.x + pChessPiece->m_X * m_fGridSize;
        vObjectPos.y = m_vOrigin.y;
        vObjectPos.z = m_vOrigin.z + pChessPiece->m_Z * m_fGridSize;
        pChessPiece->m_matObject = XMMatrixTranslation( vObjectPos.x, vObjectPos.y, vObjectPos.z );

        // Rotate white pieces to face away
        if( pChessPiece->m_Color == ChessPiece::WHITE )
        {
            pChessPiece->m_matObject = XMMatrixRotationY( XM_PI ) * pChessPiece->m_matObject;
        }
    }

    // Render the chessboard only
    VOID        Render( XMMATRIX matWorld )
    {
        m_Mesh.m_matWorld = matWorld;
        m_Mesh.Render( ATG::MESH_NOMATERIALS );
    }

    // Render the chess pieces only
    VOID        RenderPieces( XMMATRIX matWorld )
    {
        for( DWORD i = 0; i < 32; i++ )
        {
            if( m_pChessPieces[i] )
                m_pChessPieces[i]->Render( matWorld );
        }
    }
};

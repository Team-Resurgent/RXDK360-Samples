//--------------------------------------------------------------------------------------
// ResidencySampleRender.cpp
//
// This namespace includes functions that render scene views with a special shader that
// lays down texture coverage and mip LOD information.  These render methods work in
// tandem with the title residency manager, which provides the rendertargets, resolve
// textures, and processes the resulting residency coverage samples.
//
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include "ResidencySampleRender.h"

#include <AtgUtil.h>

namespace ResidencySampleRender
{
    D3DVertexShader* g_pVertexShaderTransform = NULL;
    D3DPixelShader* g_pPixelShaderResidencySample = NULL;

    //--------------------------------------------------------------------------------------
    // Name: Initialize
    // Desc: Creates the D3D objects for residency sample rendering.
    //--------------------------------------------------------------------------------------
    VOID Initialize( D3DDevice* pd3dDevice )
    {
        ATG::LoadVertexShader( "game:\\media\\shaders\\VSResidencyTransform.xvu", &g_pVertexShaderTransform );
        ATG::LoadPixelShader( "game:\\media\\shaders\\PSResidencySampleTex0.xpu", &g_pPixelShaderResidencySample );
    }

    //--------------------------------------------------------------------------------------
    // Name: BeginScene
    // Desc: Sets up a residency sample view for rendering.
    //--------------------------------------------------------------------------------------
    UINT BeginScene( D3DDevice* pd3dDevice, TitleResidencyManager* pResidencyManager )
    {
        PIXBeginNamedEvent( 0, "Residency Sample View" );

        // Call into the title residency manager to allocate a residency sample view and
        // set up rendertargets.
        UINT ViewID = pResidencyManager->BeginResidencyView( pd3dDevice );

        pd3dDevice->SetVertexShader( NULL );
        pd3dDevice->SetPixelShader( NULL );
        pd3dDevice->SetVertexDeclaration( NULL );

        return ViewID;
    }

    //--------------------------------------------------------------------------------------
    // Name: EndScene
    // Desc: Ends the rendering to a residency sample view.
    //--------------------------------------------------------------------------------------
    VOID EndScene( UINT BeginSceneID, D3DDevice* pd3dDevice, TitleResidencyManager* pResidencyManager )
    {
        // Call into the title residency manager to resolve the residency sample view
        // targets.
        pResidencyManager->EndResidencyView( pd3dDevice, BeginSceneID );

        PIXEndNamedEvent();
    }

    //--------------------------------------------------------------------------------------
    // Name: SetPixelShader
    // Desc: Sets the residency sample view pixel shader and the pixel shader constant buffer,
    //       which incorporates the given resource set ID:
    //--------------------------------------------------------------------------------------
    VOID SetPixelShader( D3DDevice* pd3dDevice, TitleResidencyManager* pResidencyManager, ResourceSetID RSID )
    {
        pd3dDevice->SetPixelShader( g_pPixelShaderResidencySample );

        // create a resource shader constant using the current resource set ID
        XMFLOAT4 ResidencyConstant;
        pResidencyManager->CreateResourceConstant( RSID, &ResidencyConstant );
        pd3dDevice->SetPixelShaderConstantF( 0, (FLOAT*)&ResidencyConstant, 1 );
    }

    //--------------------------------------------------------------------------------------
    // Name: Render
    // Desc: Renders a residency sample view of the given scene objects, using the given
    //       view and projection matrices.
    //--------------------------------------------------------------------------------------
    VOID Render( D3DDevice* pd3dDevice, TitleResidencyManager* pResidencyManager, const SceneObjectVector& SceneObjects, XMMATRIX matView, XMMATRIX matProjection )
    {
        // Get a new residency sample view from the title residency manager, which also sets the rendertargets:
        UINT ViewID = BeginScene( pd3dDevice, pResidencyManager );

        // Compute a view projection matrix:
        const FLOAT fFOVScaling = 1.0f;
        XMMATRIX matCameraVP = matView * matProjection;
        matCameraVP = matCameraVP * XMMatrixScaling( fFOVScaling, fFOVScaling, 1.0f );

        // Iterate through scene objects:
        DWORD dwVisibleCount = SceneObjects.size();
        for( DWORD dwModelIndex = 0; dwModelIndex < dwVisibleCount; ++dwModelIndex )
        {
            const SceneObject* pSceneObject = SceneObjects[dwModelIndex];

            SetPixelShader( pd3dDevice, pResidencyManager, pSceneObject->RSID );

            // Compute world * view * projection matrix for this model and set into constants.
            XMMATRIX matWVP = pSceneObject->matWorld * matCameraVP;
            matWVP = XMMatrixTranspose( matWVP );

            pd3dDevice->SetVertexShaderConstantF( 0, (FLOAT*)&matWVP, 4 );

            pd3dDevice->SetVertexDeclaration( pSceneObject->pVertexDeclaration );
            pd3dDevice->SetVertexShader( g_pVertexShaderTransform );

            pd3dDevice->SetStreamSource( 0, pSceneObject->pVertexBuffer, 0, pSceneObject->VertexStrideBytes );
            // Draw the object:
            if( pSceneObject->pIndexBuffer != NULL )
            {
                pd3dDevice->SetIndices( pSceneObject->pIndexBuffer );
                pd3dDevice->DrawIndexedPrimitive( pSceneObject->PrimitiveType, 0, 0, 0, 0, pSceneObject->PrimitiveCount );
            }
            else
            {
                pd3dDevice->DrawPrimitive( pSceneObject->PrimitiveType, 0, pSceneObject->PrimitiveCount );
            }
        }

        // End the residency sample view, releasing it to be processed next frame:
        EndScene( ViewID, pd3dDevice, pResidencyManager );
    }
}

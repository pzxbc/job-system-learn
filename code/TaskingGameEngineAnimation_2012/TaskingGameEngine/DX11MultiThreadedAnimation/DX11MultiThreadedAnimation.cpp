/*!
    \file DX11MultiThreadedAnimation.cpp

    This sample illusrates using a task-based approach to animating a set 
    of meshes using the DXSDK mesh object.
        
    Copyright 2010 Intel Corporation
    All Rights Reserved

    Permission is granted to use, copy, distribute and prepare derivative works of this
    software for any purpose and without fee, provided, that the above copyright notice
    and this statement appear in all copies.  Intel makes no representations about the
    suitability of this software for any purpose.  THIS SOFTWARE IS PROVIDED ""AS IS.""
    INTEL SPECIFICALLY DISCLAIMS ALL WARRANTIES, EXPRESS OR IMPLIED, AND ALL LIABILITY,
    INCLUDING CONSEQUENTIAL AND OTHER INDIRECT DAMAGES, FOR THE USE OF THIS SOFTWARE,
    INCLUDING LIABILITY FOR INFRINGEMENT OF ANY PROPRIETARY RIGHTS, AND INCLUDING THE
    WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.  Intel does not
    assume any responsibility for any errors which may appear in this software nor any
    responsibility to update it.
*/

#include "TaskMgrTBB.h"

//  Includes for DXT
#include "DXUT.h"
#include "DXUTcamera.h"
#include "DXUTgui.h"
#include "DXUTsettingsDlg.h"
#include "SDKmisc.h"
#include "SDKMesh.h"

const UINT                  MAX_BONE_MATRICES   = 200;  // Max bone matrices in constant buffer.
const UINT                  MAX_MODELS          = 150;  // Max number of giants to render.

struct AnimatedModel
{
    CDXUTSDKMesh            Mesh;
    DOUBLE                  dTimeOffset;    // random offset to current time to 
                                            // have a unique animation per model.

    D3DXMATRIXA16           AnimatedBones[ 2 ][ MAX_BONE_MATRICES ];
};

struct PerFrameAnimationInfo
{
    DOUBLE                  dTime;          // Current animation time
};

//--------------------------------------------------------------------------------------
// Global variables
//--------------------------------------------------------------------------------------
CDXUTDialogResourceManager  gDialogResourceManager; // manager for shared resources of dialogs
CModelViewerCamera          gCamera;                // A model viewing camera
CD3DSettingsDlg             gD3DSettingsDlg;        // Device settings dialog
CDXUTDialog                 gHUD;                   // manages the 3D   
CDXUTDialog                 gSampleUI;              // dialog for sample specific controls
CDXUTTextHelper*            gpTxtHelper = NULL;     // used to render text
BOOL                        gbShowHelp = FALSE;     // true if user selected help text

PerFrameAnimationInfo       gAnimationInfo;         // Animation taskset data
AnimatedModel               gModels[ MAX_MODELS ];  // Array of animated models

TASKSETHANDLE               ghAnimateSet = TASKSETHANDLE_INVALID; 
                                                    // handle to the current
                                                    // animation taskset

ID3D11InputLayout*          gpVertexLayout11 = NULL;// vertex decl of the soldier model
ID3D11VertexShader*         gpVertexShader = NULL;  // VS of the soldier model
ID3D11PixelShader*          gpPixelShader = NULL;   // PS of the soldier model

ID3D11SamplerState*         gpSamLinear = NULL;     // sampler state for albedo texture

ID3D11Buffer*               gpBoneBuffer = NULL;    // constant buffer for animated 
                                                    // bone matricies.

BOOL                        gbIsInDeviceSelector = FALSE; // TRUE if the device selection
                                                    // screen is visible.  Used to pause animiation.
  
struct CB_VS_PER_OBJECT
{
    D3DXMATRIX              mWorldViewProj;
    D3DXMATRIX              mWorld;
};
UINT                        giCBVSPerObjectBind = 0; // Bind slot for per model VS CB

struct CB_PS_PER_OBJECT
{
    D3DXVECTOR4             vObjectColor;
};
UINT                        giCBPSPerObjectBind = 0; // Bind slot for per model PS CB

struct CB_PS_PER_FRAME
{
    D3DXVECTOR4             vLightPosAmbient;
    D3DXVECTOR3             vEyePt;
    FLOAT                   fPadding;
};
UINT                        giCBPSPerFrameBind = 1; // Bind slot for per frame PS CB

ID3D11Buffer*               gpcbVSPerObject = NULL; // Per object VS constant buffer
ID3D11Buffer*               gpcbPSPerObject = NULL; // Per object PS constant buffer
ID3D11Buffer*               gpcbPSPerFrame = NULL;  // Per frame PS constant buffer

CDXUTCheckBox*              gpTaskingEnableUI = NULL; // DXUT UI checkbox to enable tasking

//--------------------------------------------------------------------------------------
// Globals controling sample parameters
//--------------------------------------------------------------------------------------
UINT                        guModels = 50;          // Current soldier model count

BOOL                        gbUseTasking = TRUE;    // TRUE to use the tasking path

BOOL                        gbForceCPUBound = FALSE;// TRUE to force the sample to 
                                                    // be GPU bound by rendering only one
                                                    // triangle per model.

//--------------------------------------------------------------------------------------
// UI control IDs
//--------------------------------------------------------------------------------------
#define IDC_TOGGLEFULLSCREEN    1
#define IDC_TOGGLEREF           3
#define IDC_CHANGEDEVICE        4

#define IDC_MODELSLIDERTEXT     10
#define IDC_MODELSLIDER         11

#define IDC_TASKENABLE          14

#define IDC_FORCECPUBOUND       15

//--------------------------------------------------------------------------------------
// Update UI state based on user settings
//--------------------------------------------------------------------------------------
void UpdateUI()
{
    gpTaskingEnableUI->SetChecked( !!gbUseTasking );
}

//--------------------------------------------------------------------------------------
// Render the help and statistics text
//--------------------------------------------------------------------------------------
void
RenderText( FLOAT fElapsedTime )
{
    UINT nBackBufferHeight = ( DXUTIsAppRenderingWithD3D9() ) ? DXUTGetD3D9BackBufferSurfaceDesc()->Height :
            DXUTGetDXGIBackBufferSurfaceDesc()->Height;

    static FLOAT sfTimeSinceDbgUpdate = 0;

    WCHAR       wszSampleParams[ 512 ];
    gpTxtHelper->Begin();
    gpTxtHelper->SetInsertionPos( 2, 0 );
    gpTxtHelper->SetForegroundColor( D3DXCOLOR( 1.0f, 1.0f, 0.0f, 1.0f ) );
    gpTxtHelper->DrawTextLine( DXUTGetFrameStats( DXUTIsVsyncEnabled() ) );
    gpTxtHelper->DrawTextLine( DXUTGetDeviceStats() );

    //  Update debugger every 5 seconds if it is attached.  Used primarily for seeing
    //  framerate when the NULL device is selcted.  Selecting the NULL device removes
    //  all GPU driver overhead and measures only the CPU overhead of rendering the 
    //  frame.
    if( sfTimeSinceDbgUpdate > 5.f )
    {
        sfTimeSinceDbgUpdate = 0;

        if( IsDebuggerPresent() )
        {
            wsprintf( 
                wszSampleParams,
                L"%s\n",
                DXUTGetFrameStats( TRUE ) );

            OutputDebugStringW( wszSampleParams );
        }
    }
    sfTimeSinceDbgUpdate += fElapsedTime;

    // Draw help
    if( gbShowHelp )
    {
        gpTxtHelper->SetInsertionPos( 2, nBackBufferHeight - 20 * 9 );
        gpTxtHelper->SetForegroundColor( D3DXCOLOR( 1.0f, 0.75f, 0.0f, 1.0f ) );
        gpTxtHelper->DrawTextLine( L"Controls:" );

        gpTxtHelper->SetInsertionPos( 20, nBackBufferHeight - 20 * 8 );
        gpTxtHelper->DrawTextLine(  L"Change animating model count via the 'Model Count' scrollbar\n"
                                    L"Toggle Tasking: Check 'Enable Tasking' checkbox\n"
                                    L"Make Sample CPU Bound: Check 'Force CPU Bound' checkbox\n"
                                    L"Rotate model: Left mouse button\n"
                                    L"Rotate light: Right mouse button\n"
                                    L"Rotate camera: Middle mouse button\n"
                                    L"Zoom camera: Mouse wheel scroll\n" );

        gpTxtHelper->SetInsertionPos( 550, nBackBufferHeight - 20 * 5 );
        gpTxtHelper->DrawTextLine( L"Hide help: F1\n"
                                    L"Quit: ESC\n" );
    }
    else
    {
        gpTxtHelper->SetForegroundColor( D3DXCOLOR( 1.0f, 1.0f, 1.0f, 1.0f ) );
        gpTxtHelper->DrawTextLine( L"Press F1 for help" );

        if( gbUseTasking )
        {
            wsprintf( 
                wszSampleParams,
                L"%d giants animating with tasking, rendered with immediate context\n",
                guModels );

        }
        else
        {
            wsprintf( 
                wszSampleParams,
                L"%d giants animating single threaded, rendered with immediate context\n",
                guModels );
        }
        gpTxtHelper->DrawTextLine( wszSampleParams );
    }

    gpTxtHelper->End();
}

//--------------------------------------------------------------------------------------
// Release D3D11 resources created in OnD3D11CreateDevice 
//--------------------------------------------------------------------------------------
void CALLBACK
OnD3D11DestroyDevice(
    void*                       pUserContext )
{
    gDialogResourceManager.OnD3D11DestroyDevice();
    gD3DSettingsDlg.OnD3D11DestroyDevice();
    DXUTGetGlobalResourceCache().OnDestroyDevice();
    SAFE_DELETE( gpTxtHelper );

    for( UINT uModel = 0; uModel < ARRAYSIZE( gModels ); ++uModel )
    {
        gModels[ uModel ].Mesh.Destroy();
    }

    SAFE_RELEASE( gpVertexLayout11 );
    SAFE_RELEASE( gpVertexShader );
    SAFE_RELEASE( gpPixelShader );
    SAFE_RELEASE( gpSamLinear );

    SAFE_RELEASE( gpcbVSPerObject );
    SAFE_RELEASE( gpcbPSPerObject );
    SAFE_RELEASE( gpcbPSPerFrame );

    SAFE_RELEASE( gpBoneBuffer );

}

//--------------------------------------------------------------------------------------
// Release D3D11 resources created in OnD3D11ResizedSwapChain 
//--------------------------------------------------------------------------------------
void CALLBACK
OnD3D11ReleasingSwapChain(
    void*                       pUserContext )
{
    gDialogResourceManager.OnD3D11ReleasingSwapChain();
}

//--------------------------------------------------------------------------------------
// Function to render scene models
//--------------------------------------------------------------------------------------
void
RenderModels(
    ID3D11DeviceContext*        pd3dContext )
{
    HRESULT hr;
       
    D3D11_MAPPED_SUBRESOURCE    MappedResource;

    UINT                        uGridWidth;
    FLOAT                       fGridCenter;

    D3DXMATRIX                  mModel;
    D3DXMATRIX                  mScale;
    D3DXMATRIX                  mInvWorldViewProjection;
    D3DXMATRIX                  mWorldViewProjection;
    
    D3DXMATRIX                  mWorld;
    D3DXMATRIX                  mInvWorld;
    D3DXMATRIX                  mView;
    D3DXMATRIX                  mProj;

    //  Compute transform matricies.
    mWorld = *gCamera.GetWorldMatrix();
    mProj = *gCamera.GetProjMatrix();
    mView = *gCamera.GetViewMatrix();
    
    D3DXMatrixInverse( 
        &mInvWorld, 
        NULL, 
        &mWorld );
    
    uGridWidth  = max( 1, (UINT)( sqrt( (FLOAT)guModels ) + .5f ) );

    pd3dContext->PSSetConstantBuffers( giCBPSPerFrameBind, 1, &gpcbPSPerFrame );

    // Set the shaders
    pd3dContext->VSSetShader( gpVertexShader, NULL, 0 );
    pd3dContext->PSSetShader( gpPixelShader, NULL, 0 );
    
    fGridCenter = (FLOAT)( uGridWidth / 2 );

    for( UINT uModel = 0; uModel < guModels; ++uModel )
    {
        D3DXMATRIX mPreTranslate;

        D3DXMatrixIdentity( &mPreTranslate );
        D3DXMatrixIdentity( &mScale );
        D3DXVECTOR3 vCenter = gModels[ uModel ].Mesh.GetMeshBBoxCenter( 1 );        
        D3DXVECTOR3 vExtents = gModels[ uModel ].Mesh.GetMeshBBoxExtents( 1 );

        D3DXMatrixTranslation(
            &mPreTranslate,
            -vCenter.x,
            -vCenter.y,
            -vCenter.z );
            
        // Set the per object constant data
        D3DXMatrixScaling(
            &mScale,
            2 / vExtents.y,
            2 / vExtents.y,
            2 / vExtents.y );
            
        D3DXMatrixTranslation(
            &mModel,
            4.f * ( (FLOAT)uModel / uGridWidth - fGridCenter ),
            0,
            4.f * ( (FLOAT)( ( uModel ) % uGridWidth ) - fGridCenter ) );
       
        mWorldViewProjection = mPreTranslate * mScale * mModel * mWorld * mView * mProj;
        
        // VS Per object
        V( pd3dContext->Map( gpcbVSPerObject, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource ) );
        CB_VS_PER_OBJECT* pVSPerObject = ( CB_VS_PER_OBJECT* )MappedResource.pData;
        D3DXMatrixTranspose( &mInvWorldViewProjection, &mWorldViewProjection );
    
        pVSPerObject->mWorldViewProj = mInvWorldViewProjection;
        pVSPerObject->mWorld = mInvWorld;

        pd3dContext->Unmap( gpcbVSPerObject, 0 );

        pd3dContext->VSSetConstantBuffers( giCBVSPerObjectBind, 1, &gpcbVSPerObject );

        // PS Per object
        V( pd3dContext->Map( gpcbPSPerObject, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource ) );
        CB_PS_PER_OBJECT* pPSPerObject = ( CB_PS_PER_OBJECT* )MappedResource.pData;
        pPSPerObject->vObjectColor = D3DXVECTOR4( 1, 1, 1, 1 );
        pd3dContext->Unmap( gpcbPSPerObject, 0 );

        pd3dContext->PSSetConstantBuffers( giCBPSPerObjectBind, 1, &gpcbPSPerObject );


        pd3dContext->PSSetSamplers( 0, 1, &gpSamLinear );

        // Render each mesh
        for( UINT uMesh = 0; uMesh < gModels[ uModel ].Mesh.GetNumMeshes(); ++uMesh )
        {
            SDKMESH_MATERIAL*       pMat;
            
            ID3D11Buffer*           pVB[ 1 ];
            UINT                    uStride;
            UINT                    uOffset;
            
            //IA setup
            pd3dContext->IASetInputLayout( gpVertexLayout11 );
            
            pVB[ 0 ] = gModels[ uModel ].Mesh.GetVB11( uMesh, 0 );
            uStride = ( UINT )gModels[ uModel ].Mesh.GetVertexStride( uMesh, 0 );
            uOffset = 0;

            pd3dContext->IASetVertexBuffers( 
                0, 
                1, 
                pVB, 
                &uStride, 
                &uOffset );

            pd3dContext->IASetIndexBuffer( 
                gModels[ uModel ].Mesh.GetIB11( uMesh ), 
                gModels[ uModel ].Mesh.GetIBFormat11( uMesh ), 
                0 );

            //  Set bone matrices into the constant buffer and bind to the context.
            D3D11_MAPPED_SUBRESOURCE Resource;
            V( pd3dContext->Map(
                gpBoneBuffer,
                0,
                D3D11_MAP_WRITE_DISCARD, 
                0, 
                &Resource ) );

            memcpy(
                Resource.pData,
                &gModels[ uModel ].AnimatedBones[ uMesh ],
                sizeof( D3DXMATRIX ) * gModels[ uModel ].Mesh.GetNumInfluences( uMesh ) );
                 
            pd3dContext->Unmap( 
                gpBoneBuffer, 
                0 );

            pd3dContext->VSSetConstantBuffers(
                1,
                1,
                &gpBoneBuffer );

            for( UINT uSubSet = 0; uSubSet < gModels[ uModel ].Mesh.GetNumSubsets( uMesh ); ++uSubSet )
            {
                SDKMESH_SUBSET*         pSubset = NULL;
                D3D11_PRIMITIVE_TOPOLOGY PrimType;

                // Get the uSubSet
                pSubset = gModels[ uModel ].Mesh.GetSubset( 
                    uMesh, 
                    uSubSet );

                PrimType = CDXUTSDKMesh::GetPrimitiveType11( ( SDKMESH_PRIMITIVE_TYPE )pSubset->PrimitiveType );
                pd3dContext->IASetPrimitiveTopology( PrimType );

                pMat = gModels[ uModel ].Mesh.GetMaterial( pSubset->MaterialID );

                //  Set material properties into the context.
                if( pMat )
                {
                    pd3dContext->PSSetShaderResources( 
                        0, 
                        1, 
                        &pMat->pDiffuseRV11 );

                    pd3dContext->PSSetShaderResources( 
                        1, 
                        1, 
                        &pMat->pNormalRV11 );

                    pd3dContext->PSSetShaderResources( 
                        2, 
                        1, 
                        &pMat->pSpecularRV11 );
                }

                if( FALSE == gbForceCPUBound )
                {
                    //  draw full subset
                    pd3dContext->DrawIndexed( 
                        ( UINT )pSubset->IndexCount, 
                        ( UINT )pSubset->IndexStart, 
                        ( UINT )pSubset->VertexStart );
                }
                else
                {
                    // draw something minimal to remove the GPU as the bottleneck.
                    pd3dContext->DrawIndexed( 
                        6, 
                        0, 
                        ( UINT )pSubset->VertexStart );
                }
            }
        }
    }
}

//--------------------------------------------------------------------------------------
// Render the scene using the D3D11 device
//--------------------------------------------------------------------------------------
void CALLBACK 
OnD3D11FrameRender( 
    ID3D11Device*               pd3dDevice, 
    ID3D11DeviceContext*        pd3dImmediateContext, 
    double                      fTime,
    float                       fElapsedTime, 
    void*                       pUserContext )
{
    D3D11_MAPPED_SUBRESOURCE    MappedResource;
    CB_PS_PER_FRAME*            pPerFrame;
    float                       fAmbient = 0.1f;
    
    HRESULT                     hr;

    // If the settings dialog is being shown, then render it instead of rendering the app's scene
    if( gD3DSettingsDlg.IsActive() )
    {
        gD3DSettingsDlg.OnRender( fElapsedTime );
        gbIsInDeviceSelector = TRUE;
		if( ghAnimateSet != TASKSETHANDLE_INVALID )
		{
			gTaskMgr.ReleaseHandle( ghAnimateSet );
			ghAnimateSet = TASKSETHANDLE_INVALID;
		}
        return;
    }
    else
    {
        gbIsInDeviceSelector = FALSE;
    }

    ProfileBeginFrame( "RenderFrame" );

    // Clear the render target and depth stencil
    float ClearColor[4] = { 0.0f, 0.25f, 0.25f, 0.55f };
    ID3D11RenderTargetView* pRTV = DXUTGetD3D11RenderTargetView();
    pd3dImmediateContext->ClearRenderTargetView( pRTV, ClearColor );
    ID3D11DepthStencilView* pDSV = DXUTGetD3D11DepthStencilView();
    pd3dImmediateContext->ClearDepthStencilView( pDSV, D3D11_CLEAR_DEPTH, 1.0, 0 );

    // Per frame cb update
    V( pd3dImmediateContext->Map( 
        gpcbPSPerFrame, 
        0, 
        D3D11_MAP_WRITE_DISCARD, 
        0, 
        &MappedResource ) );
    
    pPerFrame = ( CB_PS_PER_FRAME* )MappedResource.pData;
    pPerFrame->vLightPosAmbient = D3DXVECTOR4( 0, 5, 50, fAmbient );
    pPerFrame->vEyePt = *gCamera.GetEyePt();
    
    pd3dImmediateContext->Unmap( gpcbPSPerFrame, 0 );
    
    //  When tasking is enabled, wait for the animation tasks to complete.
    if( ghAnimateSet != TASKSETHANDLE_INVALID )
    {
        gTaskMgr.WaitForSet( ghAnimateSet );
        gTaskMgr.ReleaseHandle( ghAnimateSet );
        ghAnimateSet = TASKSETHANDLE_INVALID;
    }

    RenderModels( pd3dImmediateContext );

    //  Setup immediate context for UI rendering
    ProfileBeginTask( "Render UI");
    DXUTSetupD3D11Views( pd3dImmediateContext );

    gHUD.OnRender( fElapsedTime );
    gSampleUI.OnRender( fElapsedTime );
    RenderText( fElapsedTime );    
    ProfileEndTask();
        
    //  End Frame render task
    ProfileEndFrame();
}

//--------------------------------------------------------------------------------------
// Create any D3D11 resources that depend on the back buffer
//--------------------------------------------------------------------------------------
HRESULT CALLBACK 
OnD3D11ResizedSwapChain( 
    ID3D11Device*               pd3dDevice, 
    IDXGISwapChain*             pSwapChain,
    const DXGI_SURFACE_DESC*    pBackBufferSurfaceDesc, 
    void*                       pUserContext )
{
    HRESULT hr;

    V_RETURN( gDialogResourceManager.OnD3D11ResizedSwapChain( pd3dDevice, pBackBufferSurfaceDesc ) );
    V_RETURN( gD3DSettingsDlg.OnD3D11ResizedSwapChain( pd3dDevice, pBackBufferSurfaceDesc ) );

    // Setup the camera's projection parameters
    float fAspectRatio = pBackBufferSurfaceDesc->Width / ( FLOAT )pBackBufferSurfaceDesc->Height;
    gCamera.SetProjParams( D3DX_PI / 4, fAspectRatio, 0.1f, 5000.0f );
    gCamera.SetWindow( pBackBufferSurfaceDesc->Width, pBackBufferSurfaceDesc->Height );
    gCamera.SetButtonMasks( MOUSE_MIDDLE_BUTTON, MOUSE_WHEEL, MOUSE_LEFT_BUTTON );

    gHUD.SetLocation( pBackBufferSurfaceDesc->Width - 170, 0 );
    gHUD.SetSize( 170, 170 );
    gSampleUI.SetLocation( pBackBufferSurfaceDesc->Width - 170, pBackBufferSurfaceDesc->Height - 300 );
    gSampleUI.SetSize( 170, 300 );

    return S_OK;
}

HRESULT 
CompileShaderFromFile( 
    WCHAR*                      szFileName,
    LPCSTR                      szEntryPoint,
    LPCSTR                      szShaderModel,
    ID3DBlob**                  ppBlobOut )
{
    WCHAR                       wsPath[ MAX_PATH ];
    BYTE*                       pbFileData = NULL;
    
    LARGE_INTEGER               llFileSize;
    DWORD                       dwBytesRead;
    HANDLE                      hFile = INVALID_HANDLE_VALUE;
    ID3DBlob*                   pErrorBlob = NULL;
    
    HRESULT                     hr = S_OK;
    
    // find the file
    hr = DXUTFindDXSDKMediaFileCch( wsPath, MAX_PATH, szFileName );
    
    if( FAILED( hr ) ) { goto Cleanup; }

    // open the file
    hFile = CreateFile( wsPath, GENERIC_READ, FILE_SHARE_READ, 
        NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL );

    if( INVALID_HANDLE_VALUE == hFile )
    {
        hr = E_FAIL;
        goto Cleanup;
    }

    // Get the file size
    GetFileSizeEx( hFile, &llFileSize );

    // create enough space for the file data
    pbFileData = new BYTE[ llFileSize.LowPart ];
    
    if( !pbFileData )
    {
        hr = E_OUTOFMEMORY;
        goto Cleanup;
    }

    // read the data in
    if( !ReadFile( hFile, pbFileData, llFileSize.LowPart, &dwBytesRead, NULL ) )
    {
        hr = E_FAIL; 
        goto Cleanup;
    }

    CloseHandle( hFile );
    hFile = INVALID_HANDLE_VALUE;

    // Compile the shader
    hr = D3DCompile( pbFileData, llFileSize.LowPart, "none", NULL, NULL, szEntryPoint, szShaderModel, D3D10_SHADER_ENABLE_STRICTNESS, 0, ppBlobOut, &pErrorBlob );

    if( FAILED(hr) )
    {
        OutputDebugStringA( (char*)pErrorBlob->GetBufferPointer() );
        goto Cleanup;
    }

Cleanup:    
    
    SAFE_RELEASE( pErrorBlob );

    if( INVALID_HANDLE_VALUE != hFile )
    {
        CloseHandle( hFile );
    }

    delete [] pbFileData;

    return S_OK;
}

//--------------------------------------------------------------------------------------
// Create any D3D11 resources that aren't dependant on the back buffer
//--------------------------------------------------------------------------------------
HRESULT CALLBACK 
OnD3D11CreateDevice( 
    ID3D11Device*               pd3dDevice,
    const DXGI_SURFACE_DESC*    pBackBufferSurfaceDesc,
    void*                       pUserContext )
{
    
    ID3D11DeviceContext*        pd3dImmediateContext;
    D3D11_BUFFER_DESC           Desc;
    D3D11_SAMPLER_DESC          SamDesc;
        
    ID3DBlob*                   pVertexShaderBuffer = NULL;
    ID3DBlob*                   pPixelShaderBuffer = NULL;

    HRESULT hr;

    pd3dImmediateContext = DXUTGetD3D11DeviceContext();

    V_RETURN( gDialogResourceManager.OnD3D11CreateDevice( 
        pd3dDevice, 
        pd3dImmediateContext ) );
    V_RETURN( gD3DSettingsDlg.OnD3D11CreateDevice( pd3dDevice ) );

    gpTxtHelper = new CDXUTTextHelper( 
        pd3dDevice, 
        pd3dImmediateContext, 
        &gDialogResourceManager, 
        15 );

    // Compile the shaders to a model based on the feature level we acquired
    V_RETURN( CompileShaderFromFile( L"DX11MultiThreadedAnimation_VS.hlsl", "VSMain", "vs_4_0" , &pVertexShaderBuffer ) );
    V_RETURN( CompileShaderFromFile( L"DX11MultiThreadedAnimation_PS.hlsl", "PSMain", "ps_4_0" , &pPixelShaderBuffer ) );
    
    // Create the shaders
    V_RETURN( pd3dDevice->CreateVertexShader( 
        pVertexShaderBuffer->GetBufferPointer(),
            pVertexShaderBuffer->GetBufferSize(), 
            NULL, 
            &gpVertexShader ) );

    V_RETURN( pd3dDevice->CreatePixelShader( 
        pPixelShaderBuffer->GetBufferPointer(),
        pPixelShaderBuffer->GetBufferSize(), 
        NULL, 
        &gpPixelShader ) );

    // Create our vertex input layout
    const D3D11_INPUT_ELEMENT_DESC Layout[] =
    {
        { "POSITION",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0,     0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "WEIGHTS",    0, DXGI_FORMAT_R8G8B8A8_UNORM,  0,    12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "BONES",      0, DXGI_FORMAT_R8G8B8A8_UINT,   0,    16, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL",     0, DXGI_FORMAT_R32G32B32_FLOAT, 0,    20, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD",   0, DXGI_FORMAT_R32G32_FLOAT,    0,    32, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TANGENT",    0, DXGI_FORMAT_R32G32B32_FLOAT, 0,    40, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };

    V_RETURN( pd3dDevice->CreateInputLayout( 
        Layout, 
        ARRAYSIZE( Layout ), 
        pVertexShaderBuffer->GetBufferPointer(),
        pVertexShaderBuffer->GetBufferSize(), 
        &gpVertexLayout11 ) );

    SAFE_RELEASE( pVertexShaderBuffer );
    SAFE_RELEASE( pPixelShaderBuffer );

    for( UINT uModel = 0; uModel < ARRAYSIZE( gModels ); ++uModel )
    {
        // Load the mesh
         V_RETURN( gModels[ uModel ].Mesh.Create( 
            pd3dDevice, 
            L"Giant\\GraspingWalkLow_TGA.sdkmesh", 
            true ) );

        V_RETURN( gModels[ uModel ].Mesh.LoadAnimation( L"Giant\\GraspingWalkLow_TGA.sdkmesh_anim" ) );
        
        D3DXMATRIX mIdentity;
        D3DXMatrixIdentity( &mIdentity );
        gModels[ uModel ].Mesh.TransformBindPose( &mIdentity );

        //  setup random animation offset
        gModels[ uModel ].dTimeOffset = 3.0 * (DOUBLE)rand() / RAND_MAX;
    }

    // Create a bone matrix buffer
    // It will be updated more than once per frame (in a typical game) so make it dynamic
    D3D11_BUFFER_DESC vbdesc =
    {
        MAX_BONE_MATRICES * sizeof( D3DXMATRIX ),
        D3D11_USAGE_DYNAMIC,
        D3D11_BIND_CONSTANT_BUFFER,
        D3D11_CPU_ACCESS_WRITE,
        0
    };

    V_RETURN( pd3dDevice->CreateBuffer( &vbdesc, NULL, &gpBoneBuffer ) );

    // Create a sampler state
    SamDesc.Filter          = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    SamDesc.AddressU        = D3D11_TEXTURE_ADDRESS_WRAP;
    SamDesc.AddressV        = D3D11_TEXTURE_ADDRESS_WRAP;
    SamDesc.AddressW        = D3D11_TEXTURE_ADDRESS_WRAP;
    SamDesc.MipLODBias      = 0.0f;
    SamDesc.MaxAnisotropy   = 1;
    SamDesc.ComparisonFunc  = D3D11_COMPARISON_ALWAYS;
    SamDesc.BorderColor[0]  = SamDesc.BorderColor[1] = SamDesc.BorderColor[2] = SamDesc.BorderColor[3] = 0;
    SamDesc.MinLOD          = 0;
    SamDesc.MaxLOD          = D3D11_FLOAT32_MAX;

    V_RETURN( pd3dDevice->CreateSamplerState( 
        &SamDesc, 
        &gpSamLinear ) );

    // Setup constant buffers
    Desc.Usage = D3D11_USAGE_DYNAMIC;
    Desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    Desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    Desc.MiscFlags = 0;

    Desc.ByteWidth = sizeof( CB_VS_PER_OBJECT );
    V_RETURN( pd3dDevice->CreateBuffer( &Desc, NULL, &gpcbVSPerObject ) );

    Desc.ByteWidth = sizeof( CB_PS_PER_OBJECT );
    V_RETURN( pd3dDevice->CreateBuffer( &Desc, NULL, &gpcbPSPerObject ) );

    Desc.ByteWidth = sizeof( CB_PS_PER_FRAME );
    V_RETURN( pd3dDevice->CreateBuffer( &Desc, NULL, &gpcbPSPerFrame ) );
    
    // Setup the camera's view parameters
    D3DXVECTOR3 vecEye( 0.f, 0.0f, -50.0f );
    D3DXVECTOR3 vecAt ( 0.f, 0.0f, 0.f );
    
    gCamera.SetViewParams( &vecEye, &vecAt );
    
    return S_OK;
}


//--------------------------------------------------------------------------------------
// Reject any D3D11 devices that aren't acceptable by returning false
//--------------------------------------------------------------------------------------
bool CALLBACK 
IsD3D11DeviceAcceptable( 
    const CD3D11EnumAdapterInfo*    AdapterInfo, 
    UINT                    Output, 
    const CD3D11EnumDeviceInfo*     DeviceInfo,
    DXGI_FORMAT             BackBufferFormat, 
    bool                    bWindowed, 
    void*                   pUserContext )
{
    if( DeviceInfo->MaxLevel >= D3D_FEATURE_LEVEL_10_0 ) 
    {
        return true;
    }
    else
    {
        return false;
    }
}

//--------------------------------------------------------------------------------------
// Handles the GUI events
//--------------------------------------------------------------------------------------
void CALLBACK 
OnGUIEvent( 
    UINT                    nEvent,
    int                     nControlID,
    CDXUTControl*           pControl,
    void*                   pUserContext )
{
    switch( nControlID )
    {
        case IDC_TOGGLEFULLSCREEN:
            DXUTToggleFullScreen(); break;
        case IDC_TOGGLEREF:
            DXUTToggleREF(); break;
        case IDC_CHANGEDEVICE:
            gD3DSettingsDlg.SetActive( !gD3DSettingsDlg.IsActive() ); break;
        case IDC_MODELSLIDER:
            {
                CDXUTSlider* pSlider = (CDXUTSlider*)pControl;

                guModels = pSlider->GetValue();
                break;
            }
        case IDC_TASKENABLE:
            {
                CDXUTCheckBox* pBox = (CDXUTCheckBox*)pControl;

                gbUseTasking = pBox->GetChecked();
                
                break;
            }
        case IDC_FORCECPUBOUND:
            {
                CDXUTCheckBox* pBox = (CDXUTCheckBox*)pControl;

                gbForceCPUBound = pBox->GetChecked();
                break;
            }
    }
    
    UpdateUI();
}

//--------------------------------------------------------------------------------------
// Handle key presses
//--------------------------------------------------------------------------------------
void CALLBACK 
OnKeyboard( 
    UINT                    nChar,
    bool                    bKeyDown,
    bool                    bAltDown,
    void*                   pUserContext )
{
    if( bKeyDown )
    {
        switch( nChar )
        {
            case VK_F1:
                gbShowHelp = !gbShowHelp; break;
        }
    }
}

//--------------------------------------------------------------------------------------
// Handle messages to the application
//--------------------------------------------------------------------------------------
LRESULT CALLBACK 
MsgProc( 
    HWND                    hWnd,
    UINT                    uMsg,
    WPARAM                  wParam,
    LPARAM                  lParam,
    bool*                   pbNoFurtherProcessing,
    void*                   pUserContext )
{
    // Pass messages to dialog resource manager calls so GUI state is updated correctly
    *pbNoFurtherProcessing = gDialogResourceManager.MsgProc( hWnd, uMsg, wParam, lParam );
    if( *pbNoFurtherProcessing )
        return 0;

    // Pass messages to settings dialog if its active
    if( gD3DSettingsDlg.IsActive() )
    {
        gD3DSettingsDlg.MsgProc( hWnd, uMsg, wParam, lParam );
        return 0;
    }

    // Give the dialogs a chance to handle the message first
    *pbNoFurtherProcessing = gHUD.MsgProc( hWnd, uMsg, wParam, lParam );
    if( *pbNoFurtherProcessing )
        return 0;
    *pbNoFurtherProcessing = gSampleUI.MsgProc( hWnd, uMsg, wParam, lParam );
    if( *pbNoFurtherProcessing )
        return 0;

    // Pass all remaining windows messages to camera so it can respond to user input
    gCamera.HandleMessages( hWnd, uMsg, wParam, lParam );

    return 0;
}

//--------------------------------------------------------------------------------------
// Animate each model based on the current time plus its offset
//--------------------------------------------------------------------------------------
void
AnimateModel(
    VOID*                       pvInfo,
    INT                         iContext,
    UINT                        uModel,
    UINT                        uTaskCount )
{
    D3DXMATRIXA16               mIdentity;
    PerFrameAnimationInfo*      pInfo = (PerFrameAnimationInfo*)pvInfo;

    D3DXMatrixIdentity( &mIdentity );
    
    gModels[ uModel ].Mesh.TransformMesh( 
        &mIdentity, 
        pInfo->dTime + gModels[ uModel ].dTimeOffset );

    for( UINT uMesh = 0; uMesh < gModels[ uModel ].Mesh.GetNumMeshes(); ++uMesh )
    {
        for( UINT uMat = 0; uMat < gModels[ uModel ].Mesh.GetNumInfluences( uMesh ); ++uMat )
        {
            const D3DXMATRIX    *pMat;

            pMat = gModels[ uModel ].Mesh.GetMeshInfluenceMatrix( 
                    uMesh, 
                    uMat );

            D3DXMatrixTranspose(
                &gModels[ uModel ].AnimatedBones[ uMesh ][ uMat ],
                pMat );
        }
    }
}
//--------------------------------------------------------------------------------------
// Handle updates to the scene.  This is called regardless of which D3D API is used
//--------------------------------------------------------------------------------------
void CALLBACK 
OnFrameMove( 
    double                      dTime, 
    float                       fElapsedTime, 
    void*                       pUserContext )
{
    if( gbIsInDeviceSelector ) return;

    ProfileBeginTask( "MoveFrame" );

    // Update the camera's position based on user input 
    gCamera.FrameMove( fElapsedTime );
    
    gAnimationInfo.dTime = dTime;

    if( gbUseTasking )
    {
        gTaskMgr.CreateTaskSet(
            AnimateModel,
            &gAnimationInfo,
            guModels,
            NULL,
            0,
            "Animate Models",
            &ghAnimateSet );
    } 
    else  // Not using tasking
    {
        for( UINT uModel = 0; uModel < guModels; ++uModel )
        {
            AnimateModel( 
                &gAnimationInfo,
                0, 
                uModel,
                ARRAYSIZE( gModels ) );
        }
    }

    ProfileEndTask();
}

//--------------------------------------------------------------------------------------
// Called right before creating a D3D9 or D3D11 device, allowing the app to modify the device settings as needed
//--------------------------------------------------------------------------------------
bool CALLBACK 
ModifyDeviceSettings( 
    DXUTDeviceSettings*         pDeviceSettings, 
    void*                       pUserContext )
{
    // Uncomment this to get debug information from D3D11
    //pDeviceSettings->d3d11.CreateFlags |= D3D11_CREATE_DEVICE_DEBUG;

    // For the first device created if its a REF device, optionally display a warning dialog box
    static bool sbFirstTime = true;

    if( sbFirstTime )
    {
        sbFirstTime = false;

        if( ( DXUT_D3D11_DEVICE == pDeviceSettings->ver &&
              pDeviceSettings->d3d11.DriverType == D3D_DRIVER_TYPE_REFERENCE ) )
        {
            DXUTDisplaySwitchingToREFWarning( pDeviceSettings->ver );
        }
    }

    return true;
}

//--------------------------------------------------------------------------------------
// Initialize the app 
//--------------------------------------------------------------------------------------
void 
InitApp()
{
    INT                 iY = 10;

    // Initialize dialogs
    gD3DSettingsDlg.Init( &gDialogResourceManager );
    gHUD.Init( &gDialogResourceManager );
    gSampleUI.Init( &gDialogResourceManager );

    gHUD.SetCallback( OnGUIEvent ); 
    gHUD.AddButton( IDC_TOGGLEFULLSCREEN, L"Toggle full screen", 0, iY, 170, 23 );
    gHUD.AddButton( IDC_TOGGLEREF, L"Toggle REF (F3)", 0, iY += 26, 170, 23, VK_F3 );
    gHUD.AddButton( IDC_CHANGEDEVICE, L"Change device (F2)", 0, iY += 26, 170, 23, VK_F2 );

    //  Setup sample UI
    gSampleUI.SetCallback( OnGUIEvent ); 
    iY = 10;

    gSampleUI.AddStatic( 
        IDC_MODELSLIDERTEXT, L"Model Count", 
        0, iY, 170, 23 );

    gSampleUI.AddSlider( 
        IDC_MODELSLIDER, 
        0, iY += 26, 160, 23, 
        1, MAX_MODELS, guModels );

    gSampleUI.AddCheckBox( 
        IDC_TASKENABLE, L"Enable Tasking", 
        0, iY += 26, 160, 23, 
        !!gbUseTasking, 
        0, false,
        &gpTaskingEnableUI );
    
    gSampleUI.AddCheckBox( 
        IDC_FORCECPUBOUND, L"Force CPU Bound", 
        0, iY += 26, 170, 23, 
        !!gbForceCPUBound );

    UpdateUI();

    //  initialize the task manager
    gTaskMgr.Init();
}

int CALLBACK 
WinMain(
    HINSTANCE               hInstance,
    HINSTANCE               hPrevInstance,
    LPSTR                   lpCmdLine,
    int                     nCmdShow )
{
        // Enable run-time memory check for debug builds.
#if defined(DEBUG) | defined(_DEBUG)
    _CrtSetDbgFlag( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF );
#endif

    // DXUT will create and use the best device (either D3D9 or D3D11) 
    // that is available on the system depending on which D3D callbacks are set below

    // Set DXUT callbacks
    DXUTSetCallbackDeviceChanging( ModifyDeviceSettings );
    DXUTSetCallbackMsgProc( MsgProc );
    DXUTSetCallbackKeyboard( OnKeyboard );
    DXUTSetCallbackFrameMove( OnFrameMove );

    DXUTSetCallbackD3D11DeviceAcceptable( IsD3D11DeviceAcceptable );
    DXUTSetCallbackD3D11DeviceCreated( OnD3D11CreateDevice );
    DXUTSetCallbackD3D11SwapChainResized( OnD3D11ResizedSwapChain );
    DXUTSetCallbackD3D11FrameRender( OnD3D11FrameRender );
    DXUTSetCallbackD3D11SwapChainReleasing( OnD3D11ReleasingSwapChain );
    DXUTSetCallbackD3D11DeviceDestroyed( OnD3D11DestroyDevice );

    InitApp();
    DXUTInit( true, true, NULL ); // Parse the command line, show msgboxes on error, no extra command line params
    DXUTSetCursorSettings( true, true ); // Show the cursor and clip it when in full screen
    DXUTCreateWindow( L"D3D11 Multithreaded Animation" );
    DXUTCreateDevice( D3D_FEATURE_LEVEL_9_2, true, 800, 600 );

    DXUTMainLoop(); // Enter into the DXUT render loop

    gTaskMgr.Shutdown();

    return DXUTGetExitCode();

}



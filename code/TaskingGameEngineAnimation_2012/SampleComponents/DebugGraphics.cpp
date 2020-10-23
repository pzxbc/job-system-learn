//#include "stdafx.h"
// #include "Particle_common.h"

//#include "D3DX11.h"
//#include "D3D11.h"
// use this macro to silence compiler warning C4100 "unreferenced formal parameter"
#define UNREF_PARAM( param_ ) static_cast<void>( param_ )

#include "DXUT.h"
#include "d3dx11effect.h"
#include "DebugGraphics.h"
#include "SDKmisc.h"


#ifdef ENABLE_DEBUG_GRAPHICS

HRESULT CreateEffectFromFile( const wchar_t* fxFile, ID3D11Device* pd3dDevice, const D3D10_SHADER_MACRO* pDefines, DWORD dwShaderFlags, ID3DX11Effect** ppEffect );
#define VERIFY(x)

// TODO: move to static class members (instead of globals)
bool gEnableDebugInfo = true;

#define MAX_THREADS 4

int    gCurrentDebugFrameIndex = 0;
int    gLastDebugFrame = 0;
LONG   gDebugGraphicsIndex[NUM_DEBUG_FRAMES] = {-1,-1};
UINT   gDebugGraphicsColor[NUM_DEBUG_FRAMES][MAX_DEBUG_GRAPHICS_TIME_ENTRIES];
UINT   gDebugGraphicsThreadIndex[NUM_DEBUG_FRAMES][MAX_DEBUG_GRAPHICS_TIME_ENTRIES];
char  *gDebugGraphicsName[NUM_DEBUG_FRAMES][MAX_DEBUG_GRAPHICS_TIME_ENTRIES];
LARGE_INTEGER gDebugGraphicsStartTime[NUM_DEBUG_FRAMES][MAX_DEBUG_GRAPHICS_TIME_ENTRIES];
LARGE_INTEGER gDebugGraphicsEndTime[NUM_DEBUG_FRAMES][MAX_DEBUG_GRAPHICS_TIME_ENTRIES];

LONG    glDebugStartIdx[ MAX_THREADS ];
LONG    glDebugEndIdx[ MAX_THREADS ];
LONG    glDebugDepth[ MAX_THREADS ];
LARGE_INTEGER gllDebugStartTime[ MAX_THREADS ][ MAX_DEBUG_GRAPHICS_TIME_ENTRIES ];
LARGE_INTEGER gllDebugEndTime[ MAX_THREADS ][ MAX_DEBUG_GRAPHICS_TIME_ENTRIES ];
DWORD   gldwCPU[ MAX_THREADS ][ MAX_DEBUG_GRAPHICS_TIME_ENTRIES ];
// TODO: move all this sprite stuff to a sprite class.  And, use that also for render targets, etc...
ID3DX11Effect          *gpSpriteD3dEffect = NULL;
ID3DX11EffectTechnique *gpSpriteTechnique = NULL;
ID3D11InputLayout      *gpSpriteInputLayout = NULL;
ID3D11Buffer           *gpSpriteVertexBuffer = NULL;

// ***********************************************
class SpriteVertex
{
public:
    float mpPos[3];
	DWORD mColor;
};

// ***********************************************
int StartDebugMeter( UINT uContext, UINT color, char *pName )
{
    UNREF_PARAM( pName );

    if( !gEnableDebugInfo )
    {
        return 0;
    }

    // int index = ++gDebugGraphicsIndex;
    DWORD dwCPU = GetCurrentProcessorNumber(); 
    LONG index = glDebugStartIdx[ uContext ]++;
    index &= ( MAX_DEBUG_GRAPHICS_TIME_ENTRIES - 1 );

    QueryPerformanceCounter( &gllDebugStartTime[ uContext ][ index ] );
    gldwCPU[ uContext ][ index ] = dwCPU;

    return index;
}

// ***********************************************
void EndDebugMeter( UINT uContext, int index )
{
    if( !gEnableDebugInfo )
    {
        return;
    }

    LONG index = glDebugEndIdx[ uContext ]++;
    index &= ( MAX_DEBUG_GRAPHICS_TIME_ENTRIES - 1 );

    QueryPerformanceCounter( &gllDebugEndTime[ uContext ][ index ] );
}

// ***********************************************
void DrawDebugGraphics()
{
    if( !gEnableDebugInfo )
    {
        return;
    }

    // previous render state
    static LONG    slPrevDebugEndIdx[ MAX_THREADS ];
    LONG    lCurrDebugEndIdx[ MAX_THREADS ];
    static bool sbFirst = true;
    if( sbFirst )
    {
        memset( slPrevDebugEndIdx, 0, sizeof( slPrevDebugEndIdx ) );
        memset( lCurrDebugEndIdx, 0, sizeof( lCurrDebugEndIdx ) );
        sbFirst = false;
    }

    __int64 frameStartTime =  LLONG_MAX;
    __int64 frameEndTime = 0;

    for( UINT uIdx = 0; uIdx < MAX_THREADS; ++uIdx )
    {
        //  read start then end for each thread 
        lCurrDebugEndIdx[ uIdx ] = glDebugEndIdx[ uIdx ] & (MAX_DEBUG_GRAPHICS_TIME_ENTRIES-1);
        LONG lDbgEndIdx = lCurrDebugEndIdx[ uIdx ];
        LONG lDbgStartIdx = slPrevDebugEndIdx[ uIdx ];

        if( gllDebugStartTime[ uIdx ][ lDbgStartIdx ].QuadPart < frameStartTime )
        {
            frameStartTime = gllDebugStartTime[ uIdx ][ lDbgStartIdx ].QuadPart; 
        }
        if( gllDebugEndTime[ uIdx ][ lDbgEndIdx ].QuadPart > frameEndTime )
        {
            frameEndTime = gllDebugEndTime[ uIdx ][ lDbgEndIdx ].QuadPart; 
        }
    }

    //  read current 
    ID3D11Device* pd3dDevice = DXUTGetD3D11Device();
    ID3D11DeviceContext* pd3dImmediateContext = DXUTGetD3D11DeviceContext();
        
    HRESULT hr;

    if( !gpSpriteD3dEffect )
    {
        V( CreateEffectFromFile( L"UI\\SolidColorSprite.fx", pd3dDevice, NULL, 0, &gpSpriteD3dEffect ) );

        gpSpriteTechnique = gpSpriteD3dEffect->GetTechniqueByName( "TransformAndTexture" );

        // Define the input layout
        D3D11_INPUT_ELEMENT_DESC layout[] =
        {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "COLOR",    0, DXGI_FORMAT_R8G8B8A8_UNORM,  0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 }
        };
        UINT numElements = sizeof( layout ) / sizeof( layout[0] );

        // Create the input layout
        D3DX11_PASS_DESC PassDesc;
        gpSpriteTechnique->GetPassByIndex( 0 )->GetDesc( &PassDesc );
        hr = pd3dDevice->CreateInputLayout( 
            layout,
            numElements,
            PassDesc.pIAInputSignature,
            PassDesc.IAInputSignatureSize,
            &gpSpriteInputLayout
        );
        VERIFY(hr);

        // ***************************************************
        // Create Vertex Buffers
        // ***************************************************
        D3D11_BUFFER_DESC bd;
        bd.Usage = D3D11_USAGE_DYNAMIC;
        bd.ByteWidth = sizeof(SpriteVertex) * 6 * MAX_DEBUG_GRAPHICS_TIME_ENTRIES; // 2 tris, 3 verts each vertices * max entries
        bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        bd.MiscFlags = 0;

        hr = pd3dDevice->CreateBuffer( &bd, NULL, &gpSpriteVertexBuffer );
        VERIFY(hr);
    }

    SYSTEM_INFO info;
    GetSystemInfo( &info );
    DWORD numProcessors = MAX_THREADS;//info.dwNumberOfProcessors;

    double totalTime = (double)(frameEndTime - frameStartTime);

    float pPos[6][3] = { {0.0f,0.0f,0.5f}, {1.0f,0.0f,0.5f}, {0.0f,1.0f,0.5f}, {1.0f,0.0f,0.5f}, {1.0f,1.0f,0.5f}, {0.0f,1.0f,0.5f} };
    D3D11_MAPPED_SUBRESOURCE pTempData;
    V( pd3dImmediateContext->Map( gpSpriteVertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &pTempData ) );

    SpriteVertex *pData = (SpriteVertex*)pTempData.pData;
   

    const DWORD guideColors[2] = {0x80808080, 0x80FFFFFF }; // ABGR
    const float viewportStartX = 0.1f, viewportStartY = 0.7f, viewportWidth = 0.8f, viewportHeight = 0.2f;

    // Draw debug overlay guides
    int ii;
    for( ii=0; ii<(int)numProcessors; ii++ )
    {
        DWORD color = guideColors[ii%2];
        for( UINT vv=0; vv<6; vv++ )
        {
            pData->mpPos[0] = (pPos[vv][0] * viewportWidth + viewportStartX) * 2.0f - 1.0f; // X position
            pData->mpPos[1] = ((ii + pPos[vv][1])/(float)numProcessors * viewportHeight + viewportStartY) * -2.0f + 1.0f; // Y position
            pData->mpPos[2] = pPos[vv][2]; // Z position
            pData->mColor   = color;
            pData++;
        }
    }

    for( UINT uProcessor = 0; uProcessor < numProcessors; ++uProcessor )
    {
        UINT uCount;
        if( slPrevDebugEndIdx[ uProcessor ] > lCurrDebugEndIdx[ uProcessor ] )
        { 
            uCount = MAX_DEBUG_GRAPHICS_TIME_ENTRIES - slPrevDebugEndIdx[ uProcessor ] +  slPrevDebugEndIdx[ uProcessor ];
        }
        else
        {
            uCount = lCurrDebugEndIdx[ uProcessor ] - slPrevDebugEndIdx[ uProcessor ];
        }
	    for( ii=0; ii<=gDebugGraphicsIndex[gLastDebugFrame]; ii++ )
	    {
            __int64 meterStartTime = gDebugGraphicsStartTime[gLastDebugFrame][ii].QuadPart;
            __int64 meterEndTime   = gDebugGraphicsEndTime[gLastDebugFrame][ii].QuadPart;
            double deltaTime = (double)(meterEndTime - meterStartTime);
            float  processor = (float)uProcessor;

            for( UINT vv=0; vv<6; vv++ )
            {
                pData->mpPos[0] = (float)((double)(((meterStartTime-frameStartTime) + pPos[vv][0]*deltaTime)/totalTime * viewportWidth + viewportStartX) * 2.0f - 1.0f); // X position
                pData->mpPos[1] = ((processor + pPos[vv][1])/(float)numProcessors * viewportHeight + viewportStartY) * -2.0f + 1.0f; // Y position
                pData->mpPos[2] = pPos[vv][2]; // Z position
                pData->mColor   = gDebugGraphicsColor[gLastDebugFrame][ii];
                pData++;
            }
	    }
    }
    pd3dImmediateContext->Unmap( gpSpriteVertexBuffer, 0 );

    UINT stride = sizeof( SpriteVertex );
    UINT offset = 0;
    pd3dImmediateContext->IASetVertexBuffers( 0, 1, &gpSpriteVertexBuffer, &stride, &offset );
    pd3dImmediateContext->IASetInputLayout( gpSpriteInputLayout );
    pd3dImmediateContext->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST );

    D3DX11_TECHNIQUE_DESC techDesc;
    gpSpriteTechnique->GetDesc( &techDesc );
    for( UINT pp = 0; pp < techDesc.Passes; ++pp )
    {
        gpSpriteTechnique->GetPassByIndex( pp )->Apply( 0, pd3dImmediateContext );
        pd3dImmediateContext->Draw( 6 * (gDebugGraphicsIndex[gLastDebugFrame]+1) + 6*numProcessors, 0 );
    }

    static int count = 0;
    if( count++ % 1000 == 0 )
    {
        LARGE_INTEGER qpfreq;
        QueryPerformanceFrequency(&qpfreq);
        for( UINT tt=0; tt<nextTotalTimeIndex; tt++ )
        {
            char tmp[1024];
            sprintf( tmp, "%4.2f, %s (note: not normalized for core count)\n", 1000.0f * (accumulatedTime[tt]/qpfreq.QuadPart), pNames[tt] );
            OutputDebugStringA( tmp );
        }
    }
}

// ***********************************************
void ResetDebugGraphics()
{
    memset( glDebugStartIdx, 0x0, sizeof( glDebugStartIdx ) );
    memset( glDebugEndIdx, 0x0, sizeof( glDebugEndIdx ) );
    memset( glDebugDepth, 0x0, sizeof( glDebugDepth ) );

    
    memset( gllDebugStartTime, 0x0, sizeof( gllDebugStartTime ) );
    memset( gllDebugEndTime, 0x0, sizeof( gllDebugEndTime ) );

    gLastDebugFrame = gCurrentDebugFrameIndex;
    gCurrentDebugFrameIndex = (gCurrentDebugFrameIndex+1) % NUM_DEBUG_FRAMES;
    gDebugGraphicsIndex[gCurrentDebugFrameIndex] = -1;

    int meter = StartDebugMeter( gCurrentDebugFrameIndex, 0 );
    EndDebugMeter( gCurrentDebugFrameIndex, meter);
}

// ***********************************************
void DestroyDebugGraphics()
{
    SAFE_RELEASE( gpSpriteD3dEffect );
    SAFE_RELEASE( gpSpriteInputLayout );
    SAFE_RELEASE( gpSpriteVertexBuffer );
}


//-----------------------------------------------------------------------------
// CreateEffectFromFile()
//-----------------------------------------------------------------------------
HRESULT CreateEffectFromFile( const wchar_t* fxFile, ID3D11Device* pd3dDevice, const D3D10_SHADER_MACRO* pDefines, DWORD dwShaderFlags, ID3DX11Effect** ppEffect )
{
    assert( fxFile );
    assert( pd3dDevice );
    assert( ppEffect );

    HRESULT hr;
    WCHAR str[MAX_PATH];
    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, fxFile ) );

    ID3D10Blob *pBuffer = NULL;
    ID3D10Blob *pError  = NULL;
    hr = D3DX11CompileFromFile( str, pDefines, NULL, NULL, "fx_5_0", dwShaderFlags, 0, NULL, &pBuffer, &pError, NULL );
    
    if( FAILED( hr ) )
    {
        // Print the error to the debug-output window.
        char *pErrorMsg = (char*)pBuffer->GetBufferPointer();

        // Display the error in a message box.
        wchar_t msg[ 4096 ];
        int nChars = swprintf_s( msg, L"Error loading effect '%s':\n ", str );

        // pError is a multi-byte string. Convert and write it to the wide character msg string.
        mbstowcs_s( NULL, msg + nChars, _countof( msg ) - nChars, pErrorMsg, _TRUNCATE );
        wcscat_s( msg, L"\n" );

        MessageBox( NULL, msg, L"Error", NULL );
        pBuffer->Release();
        return hr;
    }

    V_RETURN( D3DX11CreateEffectFromMemory( pBuffer->GetBufferPointer(), pBuffer->GetBufferSize(), 0, pd3dDevice, ppEffect ) );

    return S_OK;
}


#endif // ENABLE_DEBUG_GRAPHICS

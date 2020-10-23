
//--------------------------------------------------------------------------------------
struct VS_OUTPUT10
{
    float4 Position : SV_POSITION;
    float4 Color : TEXCOORD1;
};

//--------------------------------------------------------------------------------------
VS_OUTPUT10 VSMain10( 
    float4 inPosition : POSITION0,
    float4 inColor    : COLOR0
 )
{
    VS_OUTPUT10 Out;

    Out.Position = inPosition;
    Out.Position.z = 0.5;
    Out.Position.w = 1.0;
    Out.Color = inColor;
    return Out;
}

//--------------------------------------------------------------------------------------
float4 PSMain10( VS_OUTPUT10 In ) : SV_Target
{
    return In.Color;
}

//--------------------------------------------------------------------------------------
BlendState SimpleBlendStates
{
    BlendEnable[0] = true;
    SrcBlend       = SRC_ALPHA;
    DestBlend      = INV_SRC_ALPHA;
    BlendOp        = ADD;
    // RenderTargetWriteMask[0] = 0x7; // disable alpha write
};

//--------------------------------------------------------------------------------------
RasterizerState SimpleRasterizerStates
{
    CullMode         = NONE;
};

//--------------------------------------------------------------------------------------
DepthStencilState SimpleDepthStencilStates
{
    DepthEnable          = false;
    StencilEnable        = false;
    DepthWriteMask = 0;
    //DepthWriteEnable     = true;
    DepthFunc            = ALWAYS;
};

//--------------------------------------------------------------------------------------
technique10 TransformAndTexture
{
    pass P0
    {
        SetVertexShader( CompileShader( vs_4_0, VSMain10() ) );
        SetGeometryShader( NULL );
        SetPixelShader( CompileShader( ps_4_0, PSMain10() ) );
        SetBlendState( SimpleBlendStates, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
        SetRasterizerState( SimpleRasterizerStates );
        SetDepthStencilState( SimpleDepthStencilStates, 0 );
    }
}
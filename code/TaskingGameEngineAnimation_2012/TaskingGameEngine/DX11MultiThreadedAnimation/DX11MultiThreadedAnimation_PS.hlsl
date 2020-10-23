//--------------------------------------------------------------------------------------
// File: DX11MultiThreadedAnimation_PS.hlsl
//
// The pixel shader file for the DX11MultiThreadedAnimation sample.
// 
// Copyright 2010 Intel Corporation
// All Rights Reserved
//
// Permission is granted to use, copy, distribute and prepare derivative works of this
// software for any purpose and without fee, provided, that the above copyright notice
// and this statement appear in all copies.  Intel makes no representations about the
// suitability of this software for any purpose.  THIS SOFTWARE IS PROVIDED ""AS IS.""
// INTEL SPECIFICALLY DISCLAIMS ALL WARRANTIES, EXPRESS OR IMPLIED, AND ALL LIABILITY,
// INCLUDING CONSEQUENTIAL AND OTHER INDIRECT DAMAGES, FOR THE USE OF THIS SOFTWARE,
// INCLUDING LIABILITY FOR INFRINGEMENT OF ANY PROPRIETARY RIGHTS, AND INCLUDING THE
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.  Intel does not
// assume any responsibility for any errors which may appear in this software nor any
// responsibility to update it.
//--------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------
// Globals
//--------------------------------------------------------------------------------------
cbuffer cbPerObject : register( b0 )
{
    float4      gvObjectColor           : packoffset( c0 );
};

cbuffer cbPerFrame : register( b1 )
{
    float3      gvLightPos              : packoffset( c0 );
    float       gfAmbient               : packoffset( c0.w );
    float3      gvEyePt                 : packoffset( c1 );
};

//--------------------------------------------------------------------------------------
// Textures and Samplers
//--------------------------------------------------------------------------------------
Texture2D       g_txAlbedo  : register( t0 );
Texture2D       g_txNormal  : register( t1 );
Texture2D       g_txSpec    : register( t2 );
SamplerState    g_samLinear : register( s0 );

//--------------------------------------------------------------------------------------
// Input / Output structures
//--------------------------------------------------------------------------------------
struct PS_INPUT
{
    float3 vWorldPos    : POSWORLD;
    float3 vNormal      : NORMAL;
    float3 vTangent     : TANGENT;
    float2 vTexcoord    : TEXCOORD0;
};

//--------------------------------------------------------------------------------------
// Pixel Shader
//--------------------------------------------------------------------------------------
float4 PSMain( PS_INPUT Input ) : SV_TARGET
{
    // sample the Albedo and normal map
    float4 vAlbedo = g_txAlbedo.Sample( g_samLinear, Input.vTexcoord );
    float3 vNormal = g_txNormal.Sample( g_samLinear, Input.vTexcoord );
    float4 vSpecColor = g_txSpec.Sample( g_samLinear, Input.vTexcoord );
    vNormal = ( vNormal * 2 ) - 1;

    //  Create tangent space matrix and transform normal
    float3 vBiNorm = normalize( cross( Input.vNormal, Input.vTangent ) );
    float3x3 mBTN = float3x3( vBiNorm, Input.vTangent, Input.vNormal );
    vNormal = normalize( mul( vNormal, mBTN ) );

    //  Calculate diffuse
    float3 vLightDir = normalize( Input.vWorldPos - gvLightPos );
    float fLighting = saturate( dot( vLightDir, vNormal ) );
    fLighting = max( fLighting, gfAmbient );
    
    //  Calculate specular power
    float3 vViewDir = normalize( gvEyePt - Input.vWorldPos );
    float3 vHalfAngle = normalize( vViewDir + vLightDir );
    float4 vSpec = pow( saturate(dot( vHalfAngle, Input.vNormal )), 64 );

    return vAlbedo * fLighting + vSpecColor * vSpec;
}


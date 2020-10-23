//--------------------------------------------------------------------------------------
// File: DX11MultiThreadedAnimation_VS.hlsl
//
// The vertex shader file for the DX11MultiThreadedAnimation sample.  
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

#define MAX_BONE_MATRICES 100
//--------------------------------------------------------------------------------------
// Globals
//--------------------------------------------------------------------------------------
cbuffer cbPerObject : register( b0 )
{
    matrix      g_mWorldViewProjection  : packoffset( c0 );
    matrix      g_mWorld                : packoffset( c4 );
};

cbuffer cbBoneMatricies: register( b1 )
{
    matrix      gBoneMatrices[ MAX_BONE_MATRICES ];
};


//--------------------------------------------------------------------------------------
// Input / Output structures
//--------------------------------------------------------------------------------------
struct VS_INPUT
{
    float3 vPosition    : POSITION;
    float4 vWeights     : WEIGHTS;
    uint4  uBones       : BONES;
    float3 vNormal      : NORMAL;
    float2 vTexcoord    : TEXCOORD0;
    float3 vTangent     : TANGENT;
};

struct VS_OUTPUT
{
    float3 vWorldPos    : POSWORLD;
    float3 vNormal      : NORMAL;
    float3 vTangent     : TANGENT;
    float2 vTexcoord    : TEXCOORD0;
    float4 vPosition    : SV_POSITION;
};

//--------------------------------------------------------------------------------------
// Helper struct for passing back skinned vertex information
//--------------------------------------------------------------------------------------
struct SkinnedInfo
{
    float4 vPosition;
    float3 vNormal;
    float3 vTangent;
};


//--------------------------------------------------------------------------------------
// SkinVert skins a single vertex
//--------------------------------------------------------------------------------------
SkinnedInfo
SkinVert( VS_INPUT Input )
{
    SkinnedInfo Output = (SkinnedInfo)0;
    
    float4 Pos = float4(Input.vPosition,1);
    float3 Norm = Input.vNormal;
    float3 Tan = Input.vTangent;
    
    //Bone0
    uint iBone = Input.uBones.x;
    float fWeight = Input.vWeights.x;
    matrix m = gBoneMatrices[ iBone ];
    Output.vPosition += fWeight * mul( Pos, m );
    Output.vNormal += fWeight * mul( Norm, (float3x3)m );
    Output.vTangent += fWeight * mul( Tan, (float3x3)m );
    
    //Bone1
    iBone = Input.uBones.y;
    fWeight = Input.vWeights.y;
    m = gBoneMatrices[ iBone ];
    Output.vPosition += fWeight * mul( Pos, m );
    Output.vNormal += fWeight * mul( Norm, (float3x3)m );
    Output.vTangent += fWeight * mul( Tan, (float3x3)m );
    
    //Bone2
    iBone = Input.uBones.z;
    fWeight = Input.vWeights.z;
    m = gBoneMatrices[ iBone ];
    Output.vPosition += fWeight * mul( Pos, m );
    Output.vNormal += fWeight * mul( Norm, (float3x3)m );
    Output.vTangent += fWeight * mul( Tan, (float3x3)m );
    
    //Bone3
    iBone = Input.uBones.w;
    fWeight = Input.vWeights.w;
    m = gBoneMatrices[ iBone ];
    Output.vPosition += fWeight * mul( Pos, m );
    Output.vNormal += fWeight * mul( Norm, (float3x3)m );
    Output.vTangent += fWeight * mul( Tan, (float3x3)m );
    
    return Output;
}

//--------------------------------------------------------------------------------------
// Vertex Shader
//--------------------------------------------------------------------------------------
VS_OUTPUT VSMain( VS_INPUT Input )
{
    VS_OUTPUT Output;

    SkinnedInfo vSkinned = SkinVert( Input );
    
    //  Transform skinned parameters
    Output.vPosition = mul( vSkinned.vPosition, g_mWorldViewProjection );
    Output.vWorldPos = mul( vSkinned.vPosition, (float3x3)g_mWorld );
    Output.vNormal = mul( vSkinned.vNormal, (float3x3)g_mWorld );
    Output.vTangent = mul( vSkinned.vTangent, (float3x3)g_mWorld );
    Output.vTexcoord = Input.vTexcoord;
    
    return Output;
}


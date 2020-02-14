#pragma once

#include "CoreMinimal.h"
#include "ShaderDeclarationDemoModule.h"
#include "RHIResources.h"

struct FComputeShaderVertexOutputStruct
{
	FVertexBufferRHIRef PositionVB;
	FVertexBufferRHIRef ColorVB;
};

struct FComputeShaderOutputUAVs
{
	FUnorderedAccessViewRHIRef VertexPositionUAV;
	FUnorderedAccessViewRHIRef VertexColorUAV;
};

/**************************************************************************************/
/* This is just an interface we use to keep all the pixel shading code in one file.   */
/**************************************************************************************/
class FVertexFromCSExample
{
public:
	static void RunVertexFromCS_RenderThread(FRHICommandListImmediate& RHICmdList, const FShaderUsageExampleParameters& DrawParameters);

	static void RunComputeShader_RenderThread(FRHICommandListImmediate& RHICmdList, const FShaderUsageExampleParameters& DrawParameters, FComputeShaderOutputUAVs& ComputeShaderOutputUAVs);
	static void DrawToRenderTarget_RenderThread(FRHICommandListImmediate& RHICmdList, const FShaderUsageExampleParameters& DrawParameters, const FComputeShaderVertexOutputStruct& ComputeShaderOutput);
};

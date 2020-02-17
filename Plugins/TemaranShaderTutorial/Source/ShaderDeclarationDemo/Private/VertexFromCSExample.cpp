#include "VertexFromCSExample.h"
#include "ShaderParameterUtils.h"
#include "RHIStaticStates.h"
#include "Shader.h"
#include "GlobalShader.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "ShaderParameterStruct.h"
#include "UniformBuffer.h"
#include "RHICommandList.h"
#include "PipelineStateCache.h"

#define NUM_VERTS 524288

class FVertexFromCSExampleCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FVertexFromCSExampleCS);
	SHADER_USE_PARAMETER_STRUCT(FVertexFromCSExampleCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_TEXTURE(Texture2D, SrcTexture)
		SHADER_PARAMETER_UAV(RWBuffer<float>, VertexPosition)
		SHADER_PARAMETER_UAV(RWBuffer<float>, VertexColor)
		SHADER_PARAMETER(float, Radius)
		SHADER_PARAMETER(uint32, TotalSize)
	END_SHADER_PARAMETER_STRUCT()

public:

	enum { ThreadGroupSize = 64 };

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::ES3_1);
	}

	static inline void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);

		OutEnvironment.SetDefine(TEXT("THREADGROUPSIZE1"), ThreadGroupSize);
	}
};

IMPLEMENT_GLOBAL_SHADER(FVertexFromCSExampleCS, "/TutorialShaders/Private/VertexFromCs_ComputeShader.usf", "MainComputeShader", SF_Compute);

void FVertexFromCSExample::RunVertexFromCS_RenderThread(FRHICommandListImmediate& RHICmdList, const FShaderUsageExampleParameters& DrawParameters)
{
	if (!DrawParameters.RenderTarget)
	{
		return;
	}

	QUICK_SCOPE_CYCLE_COUNTER(STAT_ShaderPlugin_Render); // Used to gather CPU profiling data for the UE4 session frontend
	SCOPED_DRAW_EVENT(RHICmdList, ShaderPlugin_Render); // Used to profile GPU activity and add metadata to be consumed by for example RenderDoc

	FRWBuffer VertexPositionRWBuffer;
	VertexPositionRWBuffer.Initialize(sizeof(float), (NUM_VERTS + 1) * 4, PF_R32_FLOAT);
	FRWBuffer VertexColorRWBuffer;
	VertexColorRWBuffer.Initialize(sizeof(float), (NUM_VERTS + 1) * 4, PF_R32_FLOAT);

	FComputeShaderOutputUAVs OutputUAVs;
	OutputUAVs.VertexPositionUAV = VertexPositionRWBuffer.UAV;
	OutputUAVs.VertexColorUAV = VertexColorRWBuffer.UAV;

	FComputeShaderVertexOutputStruct OutputVertex;
	OutputVertex.PositionVB = VertexPositionRWBuffer.Buffer;
	OutputVertex.ColorVB = VertexColorRWBuffer.Buffer;

	FVector4* PositionBufferData = static_cast<FVector4*>(RHILockVertexBuffer(VertexPositionRWBuffer.Buffer, 0, sizeof(float) * (NUM_VERTS + 1) * 4, EResourceLockMode::RLM_WriteOnly));
	PositionBufferData[NUM_VERTS] = FVector4(0.0, 0.0, 0.0, 1.0);
	RHIUnlockVertexBuffer(VertexPositionRWBuffer.Buffer);

	FVector4* BufferData = static_cast<FVector4*>(RHILockVertexBuffer(VertexColorRWBuffer.Buffer, 0, sizeof(float) * (NUM_VERTS + 1) * 4, EResourceLockMode::RLM_WriteOnly));
	BufferData[NUM_VERTS] = FVector4(1.0, 1.0, 0.0, 1.0);
	RHIUnlockVertexBuffer(VertexColorRWBuffer.Buffer);

	RunComputeShader_RenderThread(RHICmdList, DrawParameters, OutputUAVs);

	DrawToRenderTarget_RenderThread(RHICmdList, DrawParameters, OutputVertex);
}

void FVertexFromCSExample::RunComputeShader_RenderThread(FRHICommandListImmediate& RHICmdList, const FShaderUsageExampleParameters& DrawParameters, FComputeShaderOutputUAVs& ComputeShaderOutputUAVs)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_ShaderPlugin_VertexCompute); // Used to gather CPU profiling data for the UE4 session frontend
	SCOPED_DRAW_EVENT(RHICmdList, ShaderPlugin_VertexCompute); // Used to profile GPU activity and add metadata to be consumed by for example RenderDoc

	UnbindRenderTargets(RHICmdList);
	RHICmdList.TransitionResource(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EGfxToCompute, ComputeShaderOutputUAVs.VertexPositionUAV);
	RHICmdList.TransitionResource(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EGfxToCompute, ComputeShaderOutputUAVs.VertexColorUAV);

	FVertexFromCSExampleCS::FParameters PassParameters;
	PassParameters.SrcTexture = GBlackTexture->TextureRHI;
	PassParameters.VertexPosition = ComputeShaderOutputUAVs.VertexPositionUAV;
	PassParameters.VertexColor = ComputeShaderOutputUAVs.VertexColorUAV;
	PassParameters.Radius = DrawParameters.ComputeRadius;
	PassParameters.TotalSize = NUM_VERTS;

	TShaderMapRef<FVertexFromCSExampleCS> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));

	FComputeShaderUtils::Dispatch(RHICmdList, *ComputeShader, PassParameters,
		FIntVector(NUM_VERTS / (int)FVertexFromCSExampleCS::ThreadGroupSize,
			1, 1));

	RHICmdList.TransitionResource(EResourceTransitionAccess::EReadable, EResourceTransitionPipeline::EComputeToGfx, ComputeShaderOutputUAVs.VertexPositionUAV);
	RHICmdList.TransitionResource(EResourceTransitionAccess::EReadable, EResourceTransitionPipeline::EComputeToGfx, ComputeShaderOutputUAVs.VertexColorUAV);
}

class FVertexFromCSVertexDeclaration : public FRenderResource
{
public:
	FVertexDeclarationRHIRef VertexDeclarationRHI;

	/** Destructor. */
	virtual ~FVertexFromCSVertexDeclaration() {}

	virtual void InitRHI()
	{
		FVertexDeclarationElementList Elements;
		uint32 Stride = sizeof(float) * 4;
		Elements.Add(FVertexElement(0, 0, VET_Float4, 0, Stride));
		Elements.Add(FVertexElement(1, 0, VET_Float4, 1, Stride));
		VertexDeclarationRHI = PipelineStateCache::GetOrCreateVertexDeclaration(Elements);
	}

	virtual void ReleaseRHI()
	{
		VertexDeclarationRHI.SafeRelease();
	}
};

TGlobalResource<FVertexFromCSVertexDeclaration> GVertexFromCSVertexDeclaration;

class FVertexFromCSExampleVS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FVertexFromCSExampleVS);

public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return true;
	}

	FVertexFromCSExampleVS() { }
	FVertexFromCSExampleVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer) : FGlobalShader(Initializer) { }
};

class FVertexFromCSExamplePS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FVertexFromCSExamplePS);
	SHADER_USE_PARAMETER_STRUCT(FVertexFromCSExamplePS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(FVector2D, TextureSize) // Metal doesn't support GetDimensions(), so we send in this data via our parameters.
	END_SHADER_PARAMETER_STRUCT()

public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::ES3_1);
	}
};

// This will tell the engine to create the shader and where the shader entry point is.
//                           ShaderType                            ShaderPath                Shader function name    Type
IMPLEMENT_GLOBAL_SHADER(FVertexFromCSExampleVS, "/TutorialShaders/Private/VertexFromCS_UseShader.usf", "MainVertexShader", SF_Vertex);
IMPLEMENT_GLOBAL_SHADER(FVertexFromCSExamplePS, "/TutorialShaders/Private/VertexFromCS_UseShader.usf", "MainPixelShader", SF_Pixel);

void FVertexFromCSExample::DrawToRenderTarget_RenderThread(FRHICommandListImmediate& RHICmdList, const FShaderUsageExampleParameters& DrawParameters, const FComputeShaderVertexOutputStruct& ComputeShaderOutput)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_ShaderPlugin_VertexFromCSVertexPixel); // Used to gather CPU profiling data for the UE4 session frontend
	SCOPED_DRAW_EVENT(RHICmdList, ShaderPlugin_VertexFromCSVertexPixel); // Used to profile GPU activity and add metadata to be consumed by for example RenderDoc

	RHICmdList.TransitionResource(EResourceTransitionAccess::EWritable, DrawParameters.RenderTarget->GetRenderTargetResource()->GetRenderTargetTexture());

	FRHIRenderPassInfo RenderPassInfo(DrawParameters.RenderTarget->GetRenderTargetResource()->GetRenderTargetTexture(), ERenderTargetActions::Clear_Store);
	RHICmdList.BeginRenderPass(RenderPassInfo, TEXT("ShaderPlugin_OutputToRenderTarget"));

	auto ShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
	TShaderMapRef<FVertexFromCSExampleVS> VertexShader(ShaderMap);
	TShaderMapRef<FVertexFromCSExamplePS> PixelShader(ShaderMap);

	// Set the graphic pipeline state.
	FGraphicsPipelineStateInitializer GraphicsPSOInit;
	RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
	GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
	GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
	GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
	GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GVertexFromCSVertexDeclaration.VertexDeclarationRHI;
	GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
	GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
	GraphicsPSOInit.PrimitiveType = PT_TriangleList;
	SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);

	// Setup the pixel shader
	FVertexFromCSExamplePS::FParameters PassParameters;
	PassParameters.TextureSize = FVector2D(DrawParameters.GetRenderTargetSize().X, DrawParameters.GetRenderTargetSize().Y);
	SetShaderParameters(RHICmdList, *PixelShader, PixelShader->GetPixelShader(), PassParameters);

	TArray<uint32> Indices;
	Indices.SetNum(NUM_VERTS * 3);
	for (int i = 0; i < NUM_VERTS; ++i)
	{
		Indices[i * 3 + 0] = NUM_VERTS;
		Indices[i * 3 + 1] = (i + 1) % NUM_VERTS;
		Indices[i * 3 + 2] = i;
	}

	FRHIResourceCreateInfo CreateInfo;
	FIndexBufferRHIRef IndexBufferRHI = RHICreateIndexBuffer(sizeof(uint32), sizeof(uint32) * 3 * NUM_VERTS, BUF_Volatile, CreateInfo);
	void* VoidPtr2 = RHILockIndexBuffer(IndexBufferRHI, 0, sizeof(uint32) * 3 * NUM_VERTS, RLM_WriteOnly);
	FPlatformMemory::Memcpy(VoidPtr2, Indices.GetData(), sizeof(uint32) * 3 * NUM_VERTS);
	RHIUnlockIndexBuffer(IndexBufferRHI);

	// Draw
	RHICmdList.SetStreamSource(0, ComputeShaderOutput.PositionVB, 0);
	RHICmdList.SetStreamSource(1, ComputeShaderOutput.ColorVB, 0);
	//RHICmdList.DrawPrimitive(0, NUM_VERTS / 2, 1);
	RHICmdList.DrawIndexedPrimitive(IndexBufferRHI, 0, 0, NUM_VERTS + 1, 0, NUM_VERTS, 1);

	IndexBufferRHI.SafeRelease();

	RHICmdList.EndRenderPass();

	RHICmdList.TransitionResource(EResourceTransitionAccess::EReadable, DrawParameters.RenderTarget->GetRenderTargetResource()->GetRenderTargetTexture());
}


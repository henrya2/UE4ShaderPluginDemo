// Copyright 2016-2020 Cadic AB. All Rights Reserved.
// @Author	Fredrik Lindh [Temaran] (temaran@gmail.com) {https://github.com/Temaran}
///////////////////////////////////////////////////////////////////////////////////////

#include "ShaderDeclarationDemoModule.h"

#include "ComputeShaderExample.h"
#include "PixelShaderExample.h"

#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "RHI.h"
#include "GlobalShader.h"
#include "RHICommandList.h"
#include "RenderGraphBuilder.h"
#include "RenderTargetPool.h"
#include "Runtime/Core/Public/Modules/ModuleManager.h"
#include "Engine/Engine.h"
#include "VertexFromCSExample.h"

IMPLEMENT_MODULE(FShaderDeclarationDemoModule, ShaderDeclarationDemo)

// Declare some GPU stats so we can track them later
DECLARE_GPU_STAT_NAMED(ShaderPlugin_Render, TEXT("ShaderPlugin: Root Render"));
DECLARE_GPU_STAT_NAMED(ShaderPlugin_Compute, TEXT("ShaderPlugin: Render Compute Shader"));
DECLARE_GPU_STAT_NAMED(ShaderPlugin_Pixel, TEXT("ShaderPlugin: Render Pixel Shader"));

DECLARE_GPU_STAT_NAMED(ShaderPlugin_VertexCompute, TEXT("ShaderPlugin: Render Compute Shader for Vertex"))
DECLARE_GPU_STAT_NAMED(ShaderPlugin_VertexFromCSVertexPixel, TEXT("ShaderPlugin: Render VertexFromC Vertex and Pixel Shader"))

void FShaderDeclarationDemoModule::StartupModule()
{
	OnPostResolvedSceneColorHandle.Reset();
	bCachedParametersValid = false;

	// Maps virtual shader source directory to the plugin's actual shaders directory.
	FString PluginShaderDir = FPaths::Combine(FPaths::ProjectPluginsDir(), TEXT("TemaranShaderTutorial/Shaders"));
	AddShaderSourceDirectoryMapping(TEXT("/TutorialShaders"), PluginShaderDir);
}

void FShaderDeclarationDemoModule::ShutdownModule()
{
	EndRendering();
}

void FShaderDeclarationDemoModule::BeginRendering()
{
	if (OnPostResolvedSceneColorHandle.IsValid())
	{
		return;
	}

	bCachedParametersValid = false;

	//HandlePreRenderHandle = GEngine->GetPreRenderDelegate().AddRaw(this, &FShaderDeclarationDemoModule::HandlePreRender);

	const FName RendererModuleName("Renderer");
	IRendererModule* RendererModule = FModuleManager::GetModulePtr<IRendererModule>(RendererModuleName);
	if (RendererModule)
	{
		//OnPostResolvedSceneColorHandle = RendererModule->GetResolvedSceneColorCallbacks().AddRaw(this, &FShaderDeclarationDemoModule::PostResolveSceneColor_RenderThread);
	}
}

void FShaderDeclarationDemoModule::EndRendering()
{
	if (!OnPostResolvedSceneColorHandle.IsValid())
	{
		return;
	}

	GEngine->GetPreRenderDelegate().Remove(HandlePreRenderHandle);
	HandlePreRenderHandle.Reset();

	const FName RendererModuleName("Renderer");
	IRendererModule* RendererModule = FModuleManager::GetModulePtr<IRendererModule>(RendererModuleName);
	if (RendererModule)
	{
		//RendererModule->GetResolvedSceneColorCallbacks().Remove(OnPostResolvedSceneColorHandle);
	}

	OnPostResolvedSceneColorHandle.Reset();
}

void FShaderDeclarationDemoModule::UpdateParameters(FShaderUsageExampleParameters& DrawParameters)
{
	//RenderEveryFrameLock.Lock();
	CachedShaderUsageExampleParameters = DrawParameters;
	bCachedParametersValid = true;
	//RenderEveryFrameLock.Unlock();
}

void FShaderDeclarationDemoModule::DrawTarget(EShaderTestSampleType TestType /*= EShaderTestSampleType::ComputeAndPixel*/)
{
	if (!bCachedParametersValid)
		return;

	FShaderUsageExampleParameters Copy = CachedShaderUsageExampleParameters;
	auto* ThisPtr = this;

	ENQUEUE_RENDER_COMMAND(DrawTargetCommand)(
		[ThisPtr, Copy, TestType](FRHICommandListImmediate& RHICmdList)
	{
		ThisPtr->Draw_RenderThread(RHICmdList, Copy, TestType);
	}
	);
}

void FShaderDeclarationDemoModule::PostResolveSceneColor_RenderThread(FRHICommandListImmediate& RHICmdList, class FSceneRenderTargets& SceneContext)
{
	if (!bCachedParametersValid)
	{
		return;
	}

	// Depending on your data, you might not have to lock here, just added this code to show how you can do it if you have to.
	RenderEveryFrameLock.Lock();
	FShaderUsageExampleParameters Copy = CachedShaderUsageExampleParameters;
	RenderEveryFrameLock.Unlock();

	Draw_RenderThread(RHICmdList, Copy);
}

void FShaderDeclarationDemoModule::Draw_RenderThread(FRHICommandListImmediate& RHICmdList, const FShaderUsageExampleParameters& DrawParameters, EShaderTestSampleType Type /*= EShaderTestSampleType::ComputeAndPixel*/)
{
	check(IsInRenderingThread());

	switch (Type)
	{
	case EShaderTestSampleType::ComputeAndPixel:
		RunComputeAndPixelSample_RenderThread(RHICmdList, DrawParameters);
		break;

	case EShaderTestSampleType::ComputeToVertexBuffer:
		FVertexFromCSExample::RunVertexFromCS_RenderThread(RHICmdList, DrawParameters);
		break;
	}
}

void FShaderDeclarationDemoModule::RunComputeAndPixelSample_RenderThread(FRHICommandListImmediate& RHICmdList, const FShaderUsageExampleParameters& DrawParameters)
{
	if (!DrawParameters.RenderTarget)
	{
		return;
	}

	QUICK_SCOPE_CYCLE_COUNTER(STAT_ShaderPlugin_Render); // Used to gather CPU profiling data for the UE4 session frontend
	SCOPED_DRAW_EVENT(RHICmdList, ShaderPlugin_Render); // Used to profile GPU activity and add metadata to be consumed by for example RenderDoc

	if (!ComputeShaderOutput.IsValid())
	{
		FPooledRenderTargetDesc ComputeShaderOutputDesc(FPooledRenderTargetDesc::Create2DDesc(DrawParameters.GetRenderTargetSize(), PF_R8G8B8A8, FClearValueBinding::None, TexCreate_None, TexCreate_RenderTargetable | TexCreate_UAV, false));
		ComputeShaderOutputDesc.DebugName = TEXT("ShaderPlugin_ComputeShaderOutput");
		GRenderTargetPool.FindFreeElement(RHICmdList, ComputeShaderOutputDesc, ComputeShaderOutput, TEXT("ShaderPlugin_ComputeShaderOutput"));
	}

	FRWBuffer TestRWBuffer;
	TestRWBuffer.Initialize(sizeof(float) * 4, DrawParameters.GetRenderTargetSize().X * DrawParameters.GetRenderTargetSize().Y, PF_A32B32G32R32F);

	FComputeShaderExample::RunComputeShader_RenderThread(RHICmdList, DrawParameters, ComputeShaderOutput->GetRenderTargetItem().UAV, TestRWBuffer.UAV);

	FPixelShaderExample::DrawToRenderTarget_RenderThread(RHICmdList, DrawParameters, ComputeShaderOutput->GetRenderTargetItem().TargetableTexture, TestRWBuffer.SRV);
}

void FShaderDeclarationDemoModule::HandlePreRender()
{
	if (!bCachedParametersValid)
	{
		return;
	}

	// Depending on your data, you might not have to lock here, just added this code to show how you can do it if you have to.
	RenderEveryFrameLock.Lock();
	FShaderUsageExampleParameters Copy = CachedShaderUsageExampleParameters;
	RenderEveryFrameLock.Unlock();

	FRHICommandListImmediate& RHICmdList = GRHICommandList.GetImmediateCommandList();

	Draw_RenderThread(RHICmdList, Copy);
}

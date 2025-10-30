// Copyright (C) Microsoft. All rights reserved.

#include "RealtimeStyleTransferViewExtension.h"

#include "Modules/ModuleManager.h"
#include "PostProcess/PostProcessMaterial.h"
#include "PostProcess/SceneRenderTargets.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "StyleTransferShaders.h"
#include "HAL/IConsoleManager.h"
#include "NNERuntimeRDG.h"

DEFINE_LOG_CATEGORY_STATIC(LogRealtimeStyleTransfer, Log, All);

namespace RealtimeStyleTransfer
{
	static int32 IsActive = 0;
	static FAutoConsoleVariableRef CVarStyleTransferIsActive(
		TEXT("r.RealtimeStyleTransfer.Enable"),
		IsActive,
		TEXT("Allows an additional rendering pass that will apply a neural style to the frame.\n")
		TEXT("=0:off (default), >0: on"),
		ECVF_Cheat | ECVF_RenderThreadSafe);
}

TStrongObjectPtr<UMyNeuralNetwork> FRealtimeStyleTransferViewExtension::ModelOwner;
FStyleTransferProxyPtr FRealtimeStyleTransferViewExtension::ModelProxy;

FRealtimeStyleTransferViewExtension::FRealtimeStyleTransferViewExtension(const FAutoRegister& AutoRegister)
	: FSceneViewExtensionBase(AutoRegister)
{
	const FString RHIName = GDynamicRHI ? GDynamicRHI->GetName() : FString(TEXT("Unknown"));
	ViewExtensionIsActive = (RHIName == TEXT("D3D12") || RHIName == TEXT("D3D11"));

	UE_LOG(LogRealtimeStyleTransfer, Log, TEXT("RealtimeStyleTransferViewExtension created. RHI='%s', Active=%s"),
		*RHIName,
		ViewExtensionIsActive ? TEXT("true") : TEXT("false"));
}

void FRealtimeStyleTransferViewExtension::SetStyle(UNNEModelData* ModelData, FName RuntimeName)
{
	if (!ModelData)
	{
		UE_LOG(LogRealtimeStyleTransfer, Log, TEXT("Style transfer disabled (no model)."));
		ModelOwner.Reset();
		ModelProxy.Reset();

		RealtimeStyleTransfer::IsActive = 0;

		if (IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.RealtimeStyleTransfer.Enable")))
		{
			CVar->Set(0, ECVF_SetByCode);
		}
		return;
	}

	UMyNeuralNetwork* Instance = NewObject<UMyNeuralNetwork>();
	if (!Instance->Initialize(ModelData, RuntimeName))
	{
		UE_LOG(LogRealtimeStyleTransfer, Error, TEXT("Failed to initialize NNE model '%s'"), *ModelData->GetName());
		ModelOwner.Reset();
		ModelProxy.Reset();
		return;
	}

	ModelOwner.Reset(Instance);
	ModelProxy = Instance->GetProxy();

	if (!ModelProxy.IsValid())
	{
		UE_LOG(LogRealtimeStyleTransfer, Error, TEXT("Model proxy returned invalid after initialization of '%s'."), *ModelData->GetName());
		return;
	}

	UE_LOG(LogRealtimeStyleTransfer, Log, TEXT("Style transfer enabled with model '%s' (runtime: %s). InputTensor %ux%ux%u%u, OutputTensor %ux%ux%u%u"),
		*ModelData->GetName(),
		RuntimeName.IsNone() ? TEXT("default") : *RuntimeName.ToString(),
		ModelProxy->InputTensorShape.GetData()[0],
		ModelProxy->InputTensorShape.GetData()[1],
		ModelProxy->InputTensorShape.GetData()[2],
		ModelProxy->InputTensorShape.GetData()[3],
		ModelProxy->OutputTensorShape.GetData()[0],
		ModelProxy->OutputTensorShape.GetData()[1],
		ModelProxy->OutputTensorShape.GetData()[2],
		ModelProxy->OutputTensorShape.GetData()[3]);

	RealtimeStyleTransfer::IsActive = 1;

	if (IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.RealtimeStyleTransfer.Enable")))
	{
		CVar->Set(1, ECVF_SetByCode);
	}
}

bool FRealtimeStyleTransferViewExtension::IsActiveThisFrame_Internal(const FSceneViewExtensionContext& Context) const
{
	UE_LOG(LogRealtimeStyleTransfer, VeryVerbose, TEXT("IsActiveThisFrame_Internal -> %s"), ViewExtensionIsActive ? TEXT("true") : TEXT("false"));
	return ViewExtensionIsActive;
}

void FRealtimeStyleTransferViewExtension::BeginRenderViewFamily(FSceneViewFamily& InViewFamily)
{
	UE_LOG(LogRealtimeStyleTransfer, VeryVerbose, TEXT("BeginRenderViewFamily: %p"), static_cast<const void*>(&InViewFamily));
}

void FRealtimeStyleTransferViewExtension::PreRenderViewFamily_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneViewFamily& InViewFamily)
{
	UE_LOG(LogRealtimeStyleTransfer, VeryVerbose, TEXT("PreRenderViewFamily_RenderThread: %p"), static_cast<const void*>(&InViewFamily));
}

void FRealtimeStyleTransferViewExtension::PreRenderView_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneView& InView)
{
	UE_LOG(LogRealtimeStyleTransfer, VeryVerbose, TEXT("PreRenderView_RenderThread: View=%p"), static_cast<const void*>(&InView));
}

namespace
{
	FIntVector MakeGroupCount(FIntPoint Resolution)
	{
		const int32 GroupSize = FPStyleTransferShaders::kThreadGroupSize;
		return FIntVector(
			FMath::DivideAndRoundUp(Resolution.X, GroupSize),
			FMath::DivideAndRoundUp(Resolution.Y, GroupSize),
			1);
	}
}

FRDGTextureRef FRealtimeStyleTransferViewExtension::ExecuteStyleTransfer(
	FRDGBuilder& GraphBuilder,
	FRDGTextureRef SourceTexture,
	const FIntRect& ViewRect,
	FRDGTextureRef DestinationTexture)
{
	if (!ModelProxy.IsValid() || SourceTexture == nullptr || !ViewRect.Area())
	{
		if (!ModelProxy.IsValid())
		{
			UE_LOG(LogRealtimeStyleTransfer, VeryVerbose, TEXT("Skipping style transfer: model proxy invalid."));
		}
		else if (SourceTexture == nullptr)
		{
			UE_LOG(LogRealtimeStyleTransfer, Warning, TEXT("Skipping style transfer: source texture is null."));
		}
		else
		{
			UE_LOG(LogRealtimeStyleTransfer, VeryVerbose, TEXT("Skipping style transfer: empty ViewRect (size %dx%d)."),
				ViewRect.Width(),
				ViewRect.Height());
		}

		return DestinationTexture ? DestinationTexture : SourceTexture;
	}

	UE_LOG(LogRealtimeStyleTransfer, VeryVerbose, TEXT("ExecuteStyleTransfer start: source=%p, view=(%d,%d)-(%d,%d)."),
		static_cast<const void*>(SourceTexture),
		ViewRect.Min.X,
		ViewRect.Min.Y,
		ViewRect.Max.X,
		ViewRect.Max.Y);

	const FStyleTransferProxyPtr LocalProxy = ModelProxy;
	IConsoleVariable* EnableCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.RealtimeStyleTransfer.Enable"));
	const bool bCVarEnabled = !EnableCVar || EnableCVar->GetInt() > 0;
	if (!bCVarEnabled)
	{
		UE_LOG(LogRealtimeStyleTransfer, VeryVerbose, TEXT("Skipping style transfer: console variable disabled."));
		return DestinationTexture ? DestinationTexture : SourceTexture;
	}

	const FIntPoint ModelResolution = LocalProxy->InputResolution;
	if (ModelResolution.X <= 0 || ModelResolution.Y <= 0)
	{
		UE_LOG(LogRealtimeStyleTransfer, Warning, TEXT("Model resolution invalid: %dx%d"), ModelResolution.X, ModelResolution.Y);
		return SourceTexture;
	}

	const uint32 ChannelCount = static_cast<uint32>(FMath::Max(LocalProxy->InputChannels, 1));
	const uint32 PlaneSize = ModelResolution.X * ModelResolution.Y;
	const uint32 TensorElementCount = PlaneSize * ChannelCount;

	FRDGBufferRef InputTensor = GraphBuilder.CreateBuffer(
		FRDGBufferDesc::CreateBufferDesc(sizeof(float), TensorElementCount),
		TEXT("StyleTransfer.InputTensor"));

	FRDGBufferRef OutputTensor = GraphBuilder.CreateBuffer(
		FRDGBufferDesc::CreateBufferDesc(sizeof(float), TensorElementCount),
		TEXT("StyleTransfer.OutputTensor"));

	// Encode screen texture into CHW tensor
	{
		UE_LOG(LogRealtimeStyleTransfer, VeryVerbose, TEXT("Scheduling encode pass: ModelResolution=%dx%d, ViewSize=%dx%d."),
			ModelResolution.X,
			ModelResolution.Y,
			ViewRect.Width(),
			ViewRect.Height());
		auto* Parameters = GraphBuilder.AllocParameters<FPStyleTransferShaders::FEncodeCS::FParameters>();
		Parameters->ModelResolution = ModelResolution;
		Parameters->ViewMin = FVector2f(ViewRect.Min.X, ViewRect.Min.Y);
		Parameters->ViewSize = FVector2f(ViewRect.Width(), ViewRect.Height());
		Parameters->SourceExtent = FVector2f(SourceTexture->Desc.Extent.X, SourceTexture->Desc.Extent.Y);
		Parameters->EncodeScale = 1.0f;
		Parameters->EncodeBias = 0.0f;
		Parameters->ChannelCount = ChannelCount;
		Parameters->SourceTexture = SourceTexture;
		Parameters->SourceSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
		Parameters->OutputTensor = GraphBuilder.CreateUAV(FRDGBufferUAVDesc(InputTensor, PF_R32_FLOAT));

		TShaderMapRef<FPStyleTransferShaders::FEncodeCS> Shader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("StyleTransfer.Encode"),
			Shader,
			Parameters,
			MakeGroupCount(ModelResolution));
	}

	// Inference
	{
		TArray<UE::NNE::FTensorBindingRDG> InputBindings;
		TArray<UE::NNE::FTensorBindingRDG> OutputBindings;
		InputBindings.Emplace_GetRef().Buffer = InputTensor;
		OutputBindings.Emplace_GetRef().Buffer = OutputTensor;

		const UE::NNE::IModelInstanceRDG::EEnqueueRDGStatus Status =
			LocalProxy->ModelInstance->EnqueueRDG(GraphBuilder, InputBindings, OutputBindings);

	if (Status != UE::NNE::IModelInstanceRDG::EEnqueueRDGStatus::Ok)
	{
		UE_LOG(LogRealtimeStyleTransfer, Warning, TEXT("Failed to enqueue NNE inference, status=%d"), static_cast<int32>(Status));
		return SourceTexture;
	}

	UE_LOG(LogRealtimeStyleTransfer, VeryVerbose, TEXT("NNE inference enqueued successfully."));
	}

	// Decode tensor to low-res texture
	FRDGTextureDesc StylizedDesc = FRDGTextureDesc::Create2D(
		ModelResolution,
		PF_FloatRGBA,
		FClearValueBinding::Transparent,
		TexCreate_ShaderResource | TexCreate_UAV);
	FRDGTextureRef StylizedTexture = GraphBuilder.CreateTexture(StylizedDesc, TEXT("StyleTransfer.StylizedLowRes"));

	{
		UE_LOG(LogRealtimeStyleTransfer, VeryVerbose, TEXT("Scheduling decode pass to texture %p."),
			static_cast<const void*>(StylizedTexture));
		auto* Parameters = GraphBuilder.AllocParameters<FPStyleTransferShaders::FDecodeCS::FParameters>();
		Parameters->ModelResolution = ModelResolution;
		Parameters->DecodeScale = 1.0f / 255.0f;
		Parameters->DecodeBias = 0.0f;
		Parameters->ChannelCount = ChannelCount;
		Parameters->InputTensor = GraphBuilder.CreateSRV(FRDGBufferSRVDesc(OutputTensor, PF_R32_FLOAT));
		Parameters->StylizedOutput = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(StylizedTexture));

		TShaderMapRef<FPStyleTransferShaders::FDecodeCS> Shader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("StyleTransfer.Decode"),
			Shader,
			Parameters,
			MakeGroupCount(ModelResolution));
	}

	// Upscale and composite back to the scene texture size
	FRDGTextureDesc OutputDesc = SourceTexture->Desc;
	OutputDesc.Flags |= TexCreate_ShaderResource | TexCreate_UAV;
	FRDGTextureRef OutputTexture = GraphBuilder.CreateTexture(OutputDesc, TEXT("StyleTransfer.Output"));

	{
		auto* Parameters = GraphBuilder.AllocParameters<FPStyleTransferShaders::FUpscaleCS::FParameters>();
		Parameters->SourceResolution = ModelResolution;
		Parameters->TargetResolution = ViewRect.Size();
		Parameters->TargetOffset = ViewRect.Min;
		Parameters->SourceTexture = StylizedTexture;
		Parameters->SourceSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
		Parameters->TargetTexture = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(OutputTexture));

		UE_LOG(LogRealtimeStyleTransfer, VeryVerbose, TEXT("Scheduling upscale pass: %dx%d -> %dx%d."),
			ModelResolution.X,
			ModelResolution.Y,
			ViewRect.Width(),
			ViewRect.Height());

		TShaderMapRef<FPStyleTransferShaders::FUpscaleCS> Shader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("StyleTransfer.UpScale"),
			Shader,
			Parameters,
			MakeGroupCount(ViewRect.Size()));
	}

	if (DestinationTexture)
	{
		if (DestinationTexture != OutputTexture)
		{
			AddCopyTexturePass(GraphBuilder, OutputTexture, DestinationTexture);
			UE_LOG(LogRealtimeStyleTransfer, VeryVerbose, TEXT("Copying stylized texture back into destination."));
		}
		else
		{
			UE_LOG(LogRealtimeStyleTransfer, Warning, TEXT("Destination texture is identical to output texture; skipping copy."));
		}
		return DestinationTexture;
	}

	return OutputTexture;
}

void FRealtimeStyleTransferViewExtension::SubscribeToPostProcessingPass(
	EPostProcessingPass PassId,
	const FSceneView& InView,
	FAfterPassCallbackDelegateArray& InOutPassCallbacks,
	bool bIsPassEnabled)
{
    if (!bIsPassEnabled)
    {
    	UE_LOG(LogRealtimeStyleTransfer, VeryVerbose, TEXT("Post-processing pass %d disabled; skipping subscription."), PassId);
    	return;
    }

    IConsoleVariable* EnableCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.RealtimeStyleTransfer.Enable"));
    const bool bCVarEnabled = !EnableCVar || EnableCVar->GetInt() > 0;
    const bool bModelAvailable = ModelProxy.IsValid();

    if (!bModelAvailable)
    {
    	UE_LOG(LogRealtimeStyleTransfer, Verbose, TEXT("Skipping subscription for pass %d (no model)."), PassId);
    	return;
    }

    if (!bCVarEnabled)
    {
    	UE_LOG(LogRealtimeStyleTransfer, Verbose, TEXT("Skipping subscription for pass %d (console variable disabled)."), PassId);
    	return;
    }

    if (PassId == EPostProcessingPass::Tonemap)
    {
    	UE_LOG(LogRealtimeStyleTransfer, Verbose, TEXT("Subscribing to Tonemap pass (View=%p)."), static_cast<const void*>(&InView));
    	InOutPassCallbacks.Add(FAfterPassCallbackDelegate::CreateRaw(this, &FRealtimeStyleTransferViewExtension::AfterTonemap_RenderThread));
    }
}

FScreenPassTexture FRealtimeStyleTransferViewExtension::ApplyStyleTransfer(
	FRDGBuilder& GraphBuilder,
	const FSceneView& View,
	const FPostProcessMaterialInputs& InOutInputs,
	const FString& DDSFileName)
{
	if (!ModelProxy.IsValid())
	{
		UE_LOG(LogRealtimeStyleTransfer, Verbose, TEXT("ApplyStyleTransfer skipped: no active model."));
		return InOutInputs.OverrideOutput.IsValid()
			? FScreenPassTexture(InOutInputs.OverrideOutput)
			: FScreenPassTexture(InOutInputs.Textures[(uint32)EPostProcessMaterialInput::SceneColor]);
	}

	FScreenPassTexture SceneColor = InOutInputs.OverrideOutput.IsValid()
		? FScreenPassTexture(InOutInputs.OverrideOutput)
		: FScreenPassTexture(InOutInputs.Textures[(uint32)EPostProcessMaterialInput::SceneColor]);

	if (!SceneColor.IsValid())
	{
		UE_LOG(LogRealtimeStyleTransfer, Warning, TEXT("ApplyStyleTransfer skipped: SceneColor invalid."));
		return SceneColor;
	}

	UE_LOG(LogRealtimeStyleTransfer, VeryVerbose, TEXT("ApplyStyleTransfer executing on %p, rect=(%d,%d)-(%d,%d)."),
		static_cast<const void*>(SceneColor.Texture),
		SceneColor.ViewRect.Min.X,
		SceneColor.ViewRect.Min.Y,
		SceneColor.ViewRect.Max.X,
		SceneColor.ViewRect.Max.Y);

	ExecuteStyleTransfer(GraphBuilder, SceneColor.Texture, SceneColor.ViewRect, SceneColor.Texture);
	return SceneColor;
}

FScreenPassTexture FRealtimeStyleTransferViewExtension::AfterTonemap_RenderThread(
	FRDGBuilder& GraphBuilder,
	const FSceneView& View,
	const FPostProcessMaterialInputs& InOutInputs)
{
	RDG_EVENT_SCOPE(GraphBuilder, "RealtimeStyleTransfer_AfterTonemap");
	UE_LOG(LogRealtimeStyleTransfer, VeryVerbose, TEXT("AfterTonemap_RenderThread invoked (ViewFamily=%p)"),
		static_cast<const void*>(View.Family));
	return ApplyStyleTransfer(GraphBuilder, View, InOutInputs, TEXT("StyleTransferTonemap"));
}



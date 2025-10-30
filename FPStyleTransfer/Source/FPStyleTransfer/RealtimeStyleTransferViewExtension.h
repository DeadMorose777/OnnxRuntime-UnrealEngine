// Copyright (C) Microsoft. All rights reserved.

#pragma once


#include "SceneViewExtension.h"
#include "MyNeuralNetwork.h"
#include "UObject/StrongObjectPtr.h"
#include "NNEModelData.h"

class FRealtimeStyleTransferViewExtension : public FSceneViewExtensionBase
{
public:
	FRealtimeStyleTransferViewExtension(const FAutoRegister& AutoRegister);

	static void SetStyle(UNNEModelData* ModelData, FName RuntimeName);
	
	//~ ISceneViewExtension interface
	virtual void SetupViewFamily(FSceneViewFamily& InViewFamily) override {}
	virtual void SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView) override {}
	virtual void BeginRenderViewFamily(FSceneViewFamily& InViewFamily) override;
	virtual void PreRenderViewFamily_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneViewFamily& InViewFamily) override;
	virtual void PreRenderView_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneView& InView) override;
	virtual bool IsActiveThisFrame_Internal(const FSceneViewExtensionContext& Context) const override;
	virtual void SubscribeToPostProcessingPass(EPostProcessingPass PassId, const FSceneView& InView, FAfterPassCallbackDelegateArray& InOutPassCallbacks, bool bIsPassEnabled) override;
	
	FScreenPassTexture AfterTonemap_RenderThread(FRDGBuilder& GraphBuilder, const FSceneView& View, const FPostProcessMaterialInputs& InOutInputs);

private:

	bool ViewExtensionIsActive;
	static TStrongObjectPtr<UMyNeuralNetwork> ModelOwner;
	static FStyleTransferProxyPtr ModelProxy;

	FRDGTextureRef ExecuteStyleTransfer(FRDGBuilder& GraphBuilder, FRDGTextureRef SourceTexture, const FIntRect& ViewRect, FRDGTextureRef DestinationTexture);

protected:
	FScreenPassTexture ApplyStyleTransfer(FRDGBuilder& GraphBuilder, const FSceneView& View, const FPostProcessMaterialInputs& InOutInputs, const FString& DDSFileName);
};


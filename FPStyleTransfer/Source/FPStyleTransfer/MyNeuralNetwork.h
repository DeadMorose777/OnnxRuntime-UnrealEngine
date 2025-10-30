// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "NNETypes.h"
#include "NNERuntimeRDG.h"
#include "MyNeuralNetwork.generated.h"

class UNNEModelData;

struct FStyleTransferProxy
{
	TSharedPtr<UE::NNE::IModelInstanceRDG> ModelInstance;
	FIntPoint InputResolution = FIntPoint::ZeroValue;
	FIntPoint OutputResolution = FIntPoint::ZeroValue;
	int32 InputChannels = 0;
	int32 OutputChannels = 0;
	UE::NNE::FTensorShape InputTensorShape;
	UE::NNE::FTensorShape OutputTensorShape;
};

using FStyleTransferProxyPtr = TSharedPtr<FStyleTransferProxy, ESPMode::ThreadSafe>;

UCLASS()
class FPSTYLETRANSFER_API UMyNeuralNetwork : public UObject
{
	GENERATED_BODY()

public:
	bool Initialize(UNNEModelData* ModelData, FName RuntimeName);
	FStyleTransferProxyPtr GetProxy() const { return Proxy; }

private:
	FStyleTransferProxyPtr Proxy;
};




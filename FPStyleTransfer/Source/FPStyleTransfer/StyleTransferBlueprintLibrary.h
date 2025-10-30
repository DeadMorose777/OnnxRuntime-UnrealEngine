// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "NNEModelData.h"
#include "StyleTransferBlueprintLibrary.generated.h"

UCLASS()
class FPSTYLETRANSFER_API UStyleTransferBlueprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_UCLASS_BODY()
	
	UFUNCTION(Exec, BlueprintCallable, Category = "Style Transfer")
	static void SetStyle(UNNEModelData* ModelData, FName RuntimeName = NAME_None);
};

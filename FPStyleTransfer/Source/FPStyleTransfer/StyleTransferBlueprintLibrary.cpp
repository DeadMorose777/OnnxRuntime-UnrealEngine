// Fill out your copyright notice in the Description page of Project Settings.


#include "StyleTransferBlueprintLibrary.h"

#include "RealtimeStyleTransferViewExtension.h"

UStyleTransferBlueprintLibrary::UStyleTransferBlueprintLibrary(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}
void UStyleTransferBlueprintLibrary::SetStyle(UNNEModelData* ModelData, FName RuntimeName)
{
	FRealtimeStyleTransferViewExtension::SetStyle(ModelData, RuntimeName);
}

// Copyright Epic Games, Inc. All Rights Reserved.

#include "FPStyleTransfer.h"
#include "Modules/ModuleManager.h"
#include "RealtimeStyleTransferViewExtension.h"
#include "Misc/Paths.h"
#include "ShaderCore.h"
#include "Misc/CoreDelegates.h"

IMPLEMENT_PRIMARY_GAME_MODULE(FPStyleTransferModule, FPStyleTransfer, "FPStyleTransfer" );

void FPStyleTransferModule::StartupModule()
{
	const FString ShaderDir = FPaths::Combine(FPaths::ProjectDir(), TEXT("Shaders"));
	AddShaderSourceDirectoryMapping(TEXT("/FPStyleTransfer"), ShaderDir);

	if (!PostEngineInitHandle.IsValid())
	{
		PostEngineInitHandle = FCoreDelegates::OnPostEngineInit.AddRaw(this, &FPStyleTransferModule::RegisterViewExtension);
	}
	else
	{
		RegisterViewExtension();
	}
}

void FPStyleTransferModule::ShutdownModule()
{
	if (PostEngineInitHandle.IsValid())
	{
		FCoreDelegates::OnPostEngineInit.Remove(PostEngineInitHandle);
		PostEngineInitHandle.Reset();
	}

	RealtimeStyleTransferViewExtension.Reset();
}

void FPStyleTransferModule::RegisterViewExtension()
{
	if (!RealtimeStyleTransferViewExtension.IsValid())
	{
		UE_LOG(LogTemp, Log, TEXT("Registering FRealtimeStyleTransferViewExtension."));
		RealtimeStyleTransferViewExtension = FSceneViewExtensions::NewExtension<FRealtimeStyleTransferViewExtension>();
	}
}
 

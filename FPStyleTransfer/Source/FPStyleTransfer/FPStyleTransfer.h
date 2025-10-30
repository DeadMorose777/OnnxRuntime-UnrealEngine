// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RealtimeStyleTransferViewExtension.h"

class FPStyleTransferModule : public IModuleInterface
{
public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

protected:
	void RegisterViewExtension();

	FDelegateHandle PostEngineInitHandle;
	TSharedPtr<FRealtimeStyleTransferViewExtension, ESPMode::ThreadSafe> RealtimeStyleTransferViewExtension;
};

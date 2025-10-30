// Copyright Epic Games, Inc. All Rights Reserved.

#include "FPStyleTransferGameMode.h"
#include "FPStyleTransferCharacter.h"
#include "UObject/ConstructorHelpers.h"

AFPStyleTransferGameMode::AFPStyleTransferGameMode(): Super()
{
	DefaultPawnClass = AFPStyleTransferCharacter::StaticClass();
}

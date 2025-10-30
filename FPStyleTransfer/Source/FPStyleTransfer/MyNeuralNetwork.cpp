#include "MyNeuralNetwork.h"

#include "NNE.h"
#include "NNEModelData.h"
#include "NNERuntimeRDG.h"
#include "Logging/LogMacros.h"

DEFINE_LOG_CATEGORY_STATIC(LogStyleTransferNNE, Log, All);

namespace
{
	constexpr TCHAR DefaultRuntimeName[] = TEXT("NNERuntimeORTDml");
}

bool UMyNeuralNetwork::Initialize(UNNEModelData* ModelData, FName RuntimeName)
{
	if (!ModelData)
	{
		UE_LOG(LogStyleTransferNNE, Warning, TEXT("Initialize called with null model data."));
		return false;
	}

	const FString RuntimeToUse = RuntimeName.IsNone() ? DefaultRuntimeName : RuntimeName.ToString();

	TWeakInterfacePtr<INNERuntimeRDG> RuntimeRDG = UE::NNE::GetRuntime<INNERuntimeRDG>(RuntimeToUse);
	if (!RuntimeRDG.IsValid())
	{
		UE_LOG(LogStyleTransferNNE, Error, TEXT("Unable to find RDG runtime '%s'."), *RuntimeToUse);
		return false;
	}

	if (RuntimeRDG->CanCreateModelRDG(ModelData) != INNERuntimeRDG::ECanCreateModelRDGStatus::Ok)
	{
		UE_LOG(LogStyleTransferNNE, Error, TEXT("Runtime '%s' cannot create a model from '%s'."), *RuntimeToUse, *ModelData->GetName());
		return false;
	}

	TSharedPtr<UE::NNE::IModelRDG> ModelRDG = RuntimeRDG->CreateModelRDG(ModelData);
	if (!ModelRDG.IsValid())
	{
		UE_LOG(LogStyleTransferNNE, Error, TEXT("Failed to create RDG model for '%s' using runtime '%s'."), *ModelData->GetName(), *RuntimeToUse);
		return false;
	}

	TSharedPtr<UE::NNE::IModelInstanceRDG> ModelInstance = ModelRDG->CreateModelInstanceRDG();
	if (!ModelInstance.IsValid())
	{
		UE_LOG(LogStyleTransferNNE, Error, TEXT("Failed to create model instance for '%s'."), *ModelData->GetName());
		return false;
	}

	const TConstArrayView<UE::NNE::FTensorDesc> InputDescs = ModelInstance->GetInputTensorDescs();
	if (InputDescs.IsEmpty())
	{
		UE_LOG(LogStyleTransferNNE, Error, TEXT("Model '%s' does not expose any input tensors."), *ModelData->GetName());
		return false;
	}

	const UE::NNE::FSymbolicTensorShape InputShapeSymbolic = InputDescs[0].GetShape();
	if (InputShapeSymbolic.Rank() != 4)
	{
		UE_LOG(LogStyleTransferNNE, Error, TEXT("Model '%s' must expose a 4D input tensor (NCHW)."), *ModelData->GetName());
		return false;
	}

	constexpr uint32 DefaultBatch = 1u;
	constexpr uint32 DefaultChannels = 3u;
	constexpr uint32 DefaultSpatial = 224u;

	TArray<uint32> ResolvedInputDimensions;
	ResolvedInputDimensions.SetNum(InputShapeSymbolic.Rank());

	for (int32 DimIndex = 0; DimIndex < InputShapeSymbolic.Rank(); ++DimIndex)
	{
		int64 DimValue = InputShapeSymbolic.GetData()[DimIndex];

		if (DimValue <= 0)
		{
			if (DimIndex == 0)
			{
				DimValue = DefaultBatch;
			}
			else if (DimIndex == 1)
			{
				DimValue = DefaultChannels;
			}
			else
			{
				DimValue = DefaultSpatial;
			}
		}

		ResolvedInputDimensions[DimIndex] = static_cast<uint32>(DimValue);
	}

	const UE::NNE::FTensorShape InputShape = UE::NNE::FTensorShape::Make(ResolvedInputDimensions);

	if (ModelInstance->SetInputTensorShapes({ InputShape }) != UE::NNE::IModelInstanceRDG::ESetInputTensorShapesStatus::Ok)
	{
		UE_LOG(LogStyleTransferNNE, Error, TEXT("Failed to set input tensor shape for model '%s'."), *ModelData->GetName());
		return false;
	}

	TConstArrayView<UE::NNE::FTensorShape> OutputShapes = ModelInstance->GetOutputTensorShapes();
	if (OutputShapes.IsEmpty())
	{
		UE_LOG(LogStyleTransferNNE, Error, TEXT("Unable to resolve output tensor shape for model '%s'."), *ModelData->GetName());
		return false;
	}

	const UE::NNE::FTensorShape& RawOutputShape = OutputShapes[0];

	TArray<uint32> ResolvedOutputDimensions;
	ResolvedOutputDimensions.SetNum(RawOutputShape.Rank());
	for (int32 DimIndex = 0; DimIndex < RawOutputShape.Rank(); ++DimIndex)
	{
		uint32 DimValue = RawOutputShape.GetData()[DimIndex];
		if (DimValue == 0)
		{
			if (DimIndex == 0)
			{
				DimValue = DefaultBatch;
			}
			else if (DimIndex == 1)
			{
				DimValue = DefaultChannels;
			}
			else
			{
				DimValue = DefaultSpatial;
			}
		}
		ResolvedOutputDimensions[DimIndex] = DimValue;
	}

	const UE::NNE::FTensorShape OutputShape = UE::NNE::FTensorShape::Make(ResolvedOutputDimensions);

	if (ResolvedInputDimensions[1] < 3 || ResolvedOutputDimensions[1] < 3)
	{
		UE_LOG(LogStyleTransferNNE, Error, TEXT("Model '%s' must have at least 3 channels on both input and output."), *ModelData->GetName());
		return false;
	}

	FStyleTransferProxyPtr NewProxy = MakeShared<FStyleTransferProxy, ESPMode::ThreadSafe>();
	NewProxy->ModelInstance = ModelInstance;
	NewProxy->InputResolution = FIntPoint(ResolvedInputDimensions[3], ResolvedInputDimensions[2]);
	NewProxy->OutputResolution = FIntPoint(ResolvedOutputDimensions[3], ResolvedOutputDimensions[2]);
	NewProxy->InputChannels = static_cast<int32>(ResolvedInputDimensions[1]);
	NewProxy->OutputChannels = static_cast<int32>(ResolvedOutputDimensions[1]);
	NewProxy->InputTensorShape = InputShape;
	NewProxy->OutputTensorShape = OutputShape;

	Proxy = MoveTemp(NewProxy);

	UE_LOG(LogStyleTransferNNE, Log, TEXT("Initialized style model '%s' (runtime: %s, NCHW input: %u x %u x %u x %u)."),
		*ModelData->GetName(),
		*RuntimeToUse,
		ResolvedInputDimensions[0],
		ResolvedInputDimensions[1],
		ResolvedInputDimensions[2],
		ResolvedInputDimensions[3]);

	return true;
}

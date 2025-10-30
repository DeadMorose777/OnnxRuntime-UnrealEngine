// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GlobalShader.h"
#include "ShaderParameterStruct.h"

class FRDGTexture;
class FRDGBuffer;

namespace FPStyleTransferShaders
{
	static constexpr int32 kThreadGroupSize = 8;

	class FEncodeCS : public FGlobalShader
	{
	public:
		DECLARE_GLOBAL_SHADER(FEncodeCS);
		SHADER_USE_PARAMETER_STRUCT(FEncodeCS, FGlobalShader);

		BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
			SHADER_PARAMETER(FIntPoint, ModelResolution)
			SHADER_PARAMETER(FVector2f, ViewMin)
			SHADER_PARAMETER(FVector2f, ViewSize)
			SHADER_PARAMETER(FVector2f, SourceExtent)
			SHADER_PARAMETER(float, EncodeScale)
			SHADER_PARAMETER(float, EncodeBias)
			SHADER_PARAMETER(uint32, ChannelCount)
			SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float4>, SourceTexture)
			SHADER_PARAMETER_SAMPLER(SamplerState, SourceSampler)
			SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<float>, OutputTensor)
		END_SHADER_PARAMETER_STRUCT()

		static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters);
		static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment);
	};

	class FDecodeCS : public FGlobalShader
	{
	public:
		DECLARE_GLOBAL_SHADER(FDecodeCS);
		SHADER_USE_PARAMETER_STRUCT(FDecodeCS, FGlobalShader);

		BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
			SHADER_PARAMETER(FIntPoint, ModelResolution)
			SHADER_PARAMETER(float, DecodeScale)
			SHADER_PARAMETER(float, DecodeBias)
			SHADER_PARAMETER(uint32, ChannelCount)
			SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<float>, InputTensor)
			SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, StylizedOutput)
		END_SHADER_PARAMETER_STRUCT()

		static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters);
		static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment);
	};

	class FUpscaleCS : public FGlobalShader
	{
	public:
		DECLARE_GLOBAL_SHADER(FUpscaleCS);
		SHADER_USE_PARAMETER_STRUCT(FUpscaleCS, FGlobalShader);

		BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
			SHADER_PARAMETER(FIntPoint, SourceResolution)
			SHADER_PARAMETER(FIntPoint, TargetResolution)
			SHADER_PARAMETER(FIntPoint, TargetOffset)
			SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float4>, SourceTexture)
			SHADER_PARAMETER_SAMPLER(SamplerState, SourceSampler)
			SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, TargetTexture)
		END_SHADER_PARAMETER_STRUCT()

		static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters);
		static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment);
	};
}

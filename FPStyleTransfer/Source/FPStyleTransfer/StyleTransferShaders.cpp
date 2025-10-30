#include "StyleTransferShaders.h"

#include "RenderGraphUtils.h"
#include "ShaderCompilerCore.h"

namespace FPStyleTransferShaders
{
	bool FEncodeCS::ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return true;
	}

	void FEncodeCS::ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("STYLE_TRANSFER_THREADGROUP_SIZE"), kThreadGroupSize);
		OutEnvironment.SetDefine(TEXT("STYLE_TRANSFER_VARIANT_ENCODE"), 1);
		OutEnvironment.SetDefine(TEXT("STYLE_TRANSFER_VARIANT_DECODE"), 0);
		OutEnvironment.SetDefine(TEXT("STYLE_TRANSFER_VARIANT_UPSCALE"), 0);
	}

	IMPLEMENT_GLOBAL_SHADER(FEncodeCS, "/FPStyleTransfer/StyleTransfer.usf", "StyleTransferEncodeCS", SF_Compute);

	bool FDecodeCS::ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return true;
	}

	void FDecodeCS::ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("STYLE_TRANSFER_THREADGROUP_SIZE"), kThreadGroupSize);
		OutEnvironment.SetDefine(TEXT("STYLE_TRANSFER_VARIANT_ENCODE"), 0);
		OutEnvironment.SetDefine(TEXT("STYLE_TRANSFER_VARIANT_DECODE"), 1);
		OutEnvironment.SetDefine(TEXT("STYLE_TRANSFER_VARIANT_UPSCALE"), 0);
	}

	IMPLEMENT_GLOBAL_SHADER(FDecodeCS, "/FPStyleTransfer/StyleTransfer.usf", "StyleTransferDecodeCS", SF_Compute);

	bool FUpscaleCS::ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return true;
	}

	void FUpscaleCS::ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("STYLE_TRANSFER_THREADGROUP_SIZE"), kThreadGroupSize);
		OutEnvironment.SetDefine(TEXT("STYLE_TRANSFER_VARIANT_ENCODE"), 0);
		OutEnvironment.SetDefine(TEXT("STYLE_TRANSFER_VARIANT_DECODE"), 0);
		OutEnvironment.SetDefine(TEXT("STYLE_TRANSFER_VARIANT_UPSCALE"), 1);
	}

	IMPLEMENT_GLOBAL_SHADER(FUpscaleCS, "/FPStyleTransfer/StyleTransfer.usf", "StyleTransferUpscaleCS", SF_Compute);
}

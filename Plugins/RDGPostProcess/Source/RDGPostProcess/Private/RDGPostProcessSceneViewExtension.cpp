#include "RDGPostProcessSceneViewExtension.h"

#include "DataDrivenShaderPlatformInfo.h"
#include "GlobalShader.h"
#include "RenderGraphUtils.h"
#include "SceneView.h"
#include "ScreenPass.h"
#include "ShaderParameterStruct.h"

static TAutoConsoleVariable<int32> CVarRDGPostProcessEnable(
	TEXT("r.RDGPostProcess.Enable"),
	0,
	TEXT("Enables the RDG post-process example scene view extension."),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarRDGPostProcessKuwaharaRadius(
	TEXT("r.RDGPostProcess.KuwaharaRadius"),
	4,
	TEXT("Sets the Kuwahara filter radius used by the RDG compute pass."),
	ECVF_RenderThreadSafe);

class FRDGPostProcessKuwaharaCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FRDGPostProcessKuwaharaCS);
	SHADER_USE_PARAMETER_STRUCT(FRDGPostProcessKuwaharaCS, FGlobalShader);

	static constexpr int32 ThreadGroupSize = 32;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(uint32, Radius)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D<float4>, InputTexture)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, OutputTexture)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}
};

IMPLEMENT_GLOBAL_SHADER(
	FRDGPostProcessKuwaharaCS,
	"/Plugin/RDGPostProcess/Private/KuwaharaFilterCS.usf",
	"MainCS",
	SF_Compute);

FRDGPostProcessSceneViewExtension::FRDGPostProcessSceneViewExtension(const FAutoRegister& AutoRegister)
	: FSceneViewExtensionBase(AutoRegister)
{
}

void FRDGPostProcessSceneViewExtension::SetupViewFamily(FSceneViewFamily& InViewFamily)
{
}

void FRDGPostProcessSceneViewExtension::SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView)
{
}

void FRDGPostProcessSceneViewExtension::BeginRenderViewFamily(FSceneViewFamily& InViewFamily)
{
}

void FRDGPostProcessSceneViewExtension::PostRenderViewFamily_RenderThread(
	FRDGBuilder& GraphBuilder,
	FSceneViewFamily& InViewFamily)
{
	if (CVarRDGPostProcessEnable.GetValueOnRenderThread() == 0)
	{
		return;
	}

	if (InViewFamily.Views.Num() == 0)
	{
		return;
	}

	FRDGTextureRef SceneColorTexture = TryCreateViewFamilyTexture(GraphBuilder, InViewFamily);
	if (!SceneColorTexture)
	{
		return;
	}

	if (SceneColorTexture->Desc.NumSamples != 1)
	{
		return;
	}

	const FIntPoint TextureExtent = SceneColorTexture->Desc.Extent;
	if (TextureExtent.X <= 0 || TextureExtent.Y <= 0)
	{
		return;
	}

	FRDGTextureDesc OutputDesc = FRDGTextureDesc::Create2D(
		TextureExtent,
		SceneColorTexture->Desc.Format,
		FClearValueBinding::None,
		TexCreate_ShaderResource | TexCreate_UAV);
	OutputDesc.Flags |= SceneColorTexture->Desc.Flags & TexCreate_SRGB;

	FRDGTextureRef OutputTexture = GraphBuilder.CreateTexture(OutputDesc, TEXT("RDGPostProcess.KuwaharaOutput"));

	const uint32 Radius = static_cast<uint32>(FMath::Max(CVarRDGPostProcessKuwaharaRadius.GetValueOnRenderThread(), 0));

	FRDGPostProcessKuwaharaCS::FParameters* PassParameters =
		GraphBuilder.AllocParameters<FRDGPostProcessKuwaharaCS::FParameters>();
	PassParameters->Radius = Radius;
	PassParameters->InputTexture = GraphBuilder.CreateSRV(FRDGTextureSRVDesc::Create(SceneColorTexture));
	PassParameters->OutputTexture = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(OutputTexture));

	const FGlobalShaderMap* ShaderMap = GetGlobalShaderMap(InViewFamily.GetFeatureLevel());
	TShaderMapRef<FRDGPostProcessKuwaharaCS> ComputeShader(ShaderMap);
	if (!ComputeShader.IsValid())
	{
		return;
	}

	// RDG infers the SRV, UAV, and copy transitions from these pass bindings.
	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("RDGPostProcess::KuwaharaFilter"),
		ComputeShader,
		PassParameters,
		FComputeShaderUtils::GetGroupCount(
			TextureExtent,
			FIntPoint(
				FRDGPostProcessKuwaharaCS::ThreadGroupSize,
				FRDGPostProcessKuwaharaCS::ThreadGroupSize)));

	AddCopyTexturePass(GraphBuilder, OutputTexture, SceneColorTexture);
}

bool FRDGPostProcessSceneViewExtension::IsActiveThisFrame_Internal(const FSceneViewExtensionContext& Context) const
{
	return CVarRDGPostProcessEnable.GetValueOnAnyThread() != 0;
}

#include "RDGPostProcessSceneViewExtension.h"

#include "RDGPostProcessSettingsProvider.h"

#include "DataDrivenShaderPlatformInfo.h"
#include "Engine/PostProcessVolume.h"
#include "EngineUtils.h"
#include "GlobalShader.h"
#include "RenderGraphUtils.h"
#include "RenderingThread.h"
#include "SceneInterface.h"
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

namespace
{
FRDGPostProcessSceneViewExtension::FKuwaharaSettingsSnapshot BuildSettingsSnapshot(FSceneViewFamily& InViewFamily)
{
	FRDGPostProcessSceneViewExtension::FKuwaharaSettingsSnapshot Snapshot;
	Snapshot.bEnabled = CVarRDGPostProcessEnable.GetValueOnGameThread() != 0;
	Snapshot.Radius = FMath::Max(static_cast<float>(CVarRDGPostProcessKuwaharaRadius.GetValueOnGameThread()), 0.0f);

	if (InViewFamily.Scene == nullptr || InViewFamily.Views.Num() == 0 || InViewFamily.Views[0] == nullptr)
	{
		return Snapshot;
	}

	UWorld* World = InViewFamily.Scene->GetWorld();
	if (!IsValid(World))
	{
		return Snapshot;
	}

	const FVector ViewLocation = InViewFamily.Views[0]->ViewLocation;

	APostProcessVolume* BestVolume = nullptr;
	float BestPriority = -TNumericLimits<float>::Max();
	float BestDistance = TNumericLimits<float>::Max();

	for (TActorIterator<APostProcessVolume> VolumeIt(World); VolumeIt; ++VolumeIt)
	{
		APostProcessVolume* Volume = *VolumeIt;
		if (!IsValid(Volume) || !Volume->bEnabled || Volume->BlendWeight <= 0.0f)
		{
			continue;
		}

		float DistanceToPoint = 0.0f;
		const bool bAffectsView = Volume->bUnbound || Volume->EncompassesPoint(ViewLocation, 0.0f, &DistanceToPoint);
		if (!bAffectsView)
		{
			continue;
		}

		if (BestVolume == nullptr ||
			Volume->Priority > BestPriority ||
			(FMath::IsNearlyEqual(Volume->Priority, BestPriority) && DistanceToPoint < BestDistance))
		{
			BestVolume = Volume;
			BestPriority = Volume->Priority;
			BestDistance = DistanceToPoint;
		}
	}

	const ARDGPostProcessSettingsProvider* GlobalProvider = nullptr;
	const ARDGPostProcessSettingsProvider* MatchedProvider = nullptr;

	for (TActorIterator<ARDGPostProcessSettingsProvider> ProviderIt(World); ProviderIt; ++ProviderIt)
	{
		const ARDGPostProcessSettingsProvider* Provider = *ProviderIt;
		if (!IsValid(Provider))
		{
			continue;
		}

		if (Provider->TargetVolume == BestVolume)
		{
			MatchedProvider = Provider;
			break;
		}

		if (Provider->TargetVolume == nullptr && GlobalProvider == nullptr)
		{
			GlobalProvider = Provider;
		}
	}

	const ARDGPostProcessSettingsProvider* Provider = MatchedProvider != nullptr ? MatchedProvider : GlobalProvider;
	if (Provider != nullptr)
	{
		Provider->ResolveSettings(Snapshot.bEnabled, Snapshot.Radius);
		Snapshot.Radius = FMath::Max(Snapshot.Radius, 0.0f);
	}

	return Snapshot;
}
}

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

FRDGPostProcessSceneViewExtension::~FRDGPostProcessSceneViewExtension()
{
	if (!SettingsUploadFence.IsFenceComplete())
	{
		SettingsUploadFence.Wait();
	}
}

void FRDGPostProcessSceneViewExtension::SetupViewFamily(FSceneViewFamily& InViewFamily)
{
}

void FRDGPostProcessSceneViewExtension::SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView)
{
}

void FRDGPostProcessSceneViewExtension::BeginRenderViewFamily(FSceneViewFamily& InViewFamily)
{
	const FKuwaharaSettingsSnapshot Snapshot = BuildSettingsSnapshot(InViewFamily);

	// Snapshot provider values on the game thread and hand them to the render thread by value.
	ENQUEUE_RENDER_COMMAND(RDGPostProcess_UpdateSettings)(
		[this, Snapshot](FRHICommandListImmediate& RHICmdList)
		{
			RenderThreadSettings = Snapshot;
		});
	SettingsUploadFence.BeginFence();
}

void FRDGPostProcessSceneViewExtension::PostRenderViewFamily_RenderThread(
	FRDGBuilder& GraphBuilder,
	FSceneViewFamily& InViewFamily)
{
	const FKuwaharaSettingsSnapshot Settings = RenderThreadSettings;
	if (!Settings.bEnabled)
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

	const uint32 Radius = static_cast<uint32>(FMath::RoundToInt(FMath::Max(Settings.Radius, 0.0f)));

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
	return true;
}

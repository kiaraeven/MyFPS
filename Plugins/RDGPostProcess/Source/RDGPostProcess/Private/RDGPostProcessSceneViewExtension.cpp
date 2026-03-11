#include "RDGPostProcessSceneViewExtension.h"

#include "RDGPostProcessSettingsProvider.h"

#include "DataDrivenShaderPlatformInfo.h"
#include "Engine/PostProcessVolume.h"
#include "EngineUtils.h"
#include "GlobalShader.h"
#include "PostProcess/PostProcessInputs.h"
#include "RenderGraphUtils.h"
#include "RenderingThread.h"
#include "SceneRendering.h"
#include "SceneInterface.h"
#include "SceneTextures.h"
#include "SceneView.h"
#include "ScreenPass.h"
#include "ShaderParameterMetadata.h"
#include "ShaderParameterStruct.h"
#include "SystemTextures.h"

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

static TAutoConsoleVariable<float> CVarOutlineSoftness(
	TEXT("r.Outline.Softness"),
	-1.0f,
	TEXT("Overrides outline edge softness. Negative values keep the resolved scene setting."),
	ECVF_RenderThreadSafe);

namespace
{
FRDGResolvedPostProcessSettings BuildDefaultSettings()
{
	FRDGResolvedPostProcessSettings Settings;
	Settings.bEnabled = CVarRDGPostProcessEnable.GetValueOnGameThread() != 0;
	Settings.Radius = FMath::Max(static_cast<float>(CVarRDGPostProcessKuwaharaRadius.GetValueOnGameThread()), 0.0f);
	return Settings;
}

void ApplyRenderThreadOverrides(FRDGResolvedPostProcessSettings& Settings)
{
	Settings.Radius = FMath::Max(Settings.Radius, 0.0f);
	Settings.OutlineSettings.EdgeThreshold = FMath::Max(Settings.OutlineSettings.EdgeThreshold, 0.0f);
	Settings.OutlineSettings.GlowIntensity = FMath::Max(Settings.OutlineSettings.GlowIntensity, 0.0f);
	Settings.OutlineSettings.PulseSpeed = FMath::Max(Settings.OutlineSettings.PulseSpeed, 0.0f);
	Settings.OutlineSettings.Softness = FMath::Clamp(Settings.OutlineSettings.Softness, 0.0f, 1.0f);

	const float SoftnessOverride = CVarOutlineSoftness.GetValueOnRenderThread();
	if (SoftnessOverride >= 0.0f)
	{
		Settings.OutlineSettings.Softness = FMath::Clamp(SoftnessOverride, 0.0f, 1.0f);
	}
}

APostProcessVolume* FindBestActiveVolume(UWorld* World, const FVector& ViewLocation)
{
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

	return BestVolume;
}

const ARDGPostProcessSettingsProvider* FindSettingsProvider(UWorld* World, const APostProcessVolume* ActiveVolume)
{
	const ARDGPostProcessSettingsProvider* GlobalProvider = nullptr;

	for (TActorIterator<ARDGPostProcessSettingsProvider> ProviderIt(World); ProviderIt; ++ProviderIt)
	{
		const ARDGPostProcessSettingsProvider* Provider = *ProviderIt;
		if (!IsValid(Provider))
		{
			continue;
		}

		if (Provider->TargetVolume == ActiveVolume)
		{
			return Provider;
		}

		if (Provider->TargetVolume == nullptr && GlobalProvider == nullptr)
		{
			GlobalProvider = Provider;
		}
	}

	return GlobalProvider;
}

FRDGResolvedPostProcessSettings BuildSettingsSnapshot(FSceneViewFamily& InViewFamily)
{
	FRDGResolvedPostProcessSettings Snapshot = BuildDefaultSettings();

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
	const APostProcessVolume* BestVolume = FindBestActiveVolume(World, ViewLocation);
	const ARDGPostProcessSettingsProvider* Provider = FindSettingsProvider(World, BestVolume);
	if (Provider != nullptr)
	{
		Provider->ResolveSettings(Snapshot);
	}

	return Snapshot;
}

const FSceneTextures* TryGetSceneTextures(const FSceneViewFamily& InViewFamily)
{
	const FViewFamilyInfo& ViewFamilyInfo = static_cast<const FViewFamilyInfo&>(InViewFamily);
	return ViewFamilyInfo.GetSceneTexturesChecked();
}

FRDGTextureDesc CreatePostProcessOutputDesc(const FRDGTextureRef SceneColorTexture)
{
	FRDGTextureDesc OutputDesc = FRDGTextureDesc::Create2D(
		SceneColorTexture->Desc.Extent,
		SceneColorTexture->Desc.Format,
		FClearValueBinding::None,
		TexCreate_ShaderResource | TexCreate_UAV);
	OutputDesc.Flags |= SceneColorTexture->Desc.Flags & TexCreate_SRGB;
	return OutputDesc;
}
}

BEGIN_SHADER_PARAMETER_STRUCT(FKuwaharaPassParameters, )
	SHADER_PARAMETER(uint32, Radius)
	SHADER_PARAMETER(FIntPoint, ViewRectMin)
	SHADER_PARAMETER(FIntPoint, ViewRectSize)
	SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D<float4>, InputTexture)
	SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, OutputTexture)
END_SHADER_PARAMETER_STRUCT()

BEGIN_SHADER_PARAMETER_STRUCT(FPostProcessPassParameters, )
	SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
	SHADER_PARAMETER(FIntPoint, ViewRectMin)
	SHADER_PARAMETER(FIntPoint, ViewRectSize)
	SHADER_PARAMETER(FVector4f, OutlineColor)
	SHADER_PARAMETER(float, EdgeThreshold)
	SHADER_PARAMETER(float, Softness)
	SHADER_PARAMETER(float, GlowIntensity)
	SHADER_PARAMETER(float, PulseSpeed)
	SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D<float4>, InputTexture)
	SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D<float>, SceneDepthTexture)
	SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D<float4>, SceneNormalTexture)
	SHADER_PARAMETER_SAMPLER(SamplerState, InputSampler)
	SHADER_PARAMETER_SAMPLER(SamplerState, SceneDepthSampler)
	SHADER_PARAMETER_SAMPLER(SamplerState, SceneNormalSampler)
	SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, OutputTexture)
END_SHADER_PARAMETER_STRUCT()

class FRDGPostProcessKuwaharaCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FRDGPostProcessKuwaharaCS);
	SHADER_USE_PARAMETER_STRUCT(FRDGPostProcessKuwaharaCS, FGlobalShader);

	static constexpr int32 ThreadGroupSize = 32;

	using FParameters = FKuwaharaPassParameters;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}
};

class FRDGPostProcessOutlineCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FRDGPostProcessOutlineCS);
	SHADER_USE_PARAMETER_STRUCT(FRDGPostProcessOutlineCS, FGlobalShader);

	static constexpr int32 ThreadGroupSize = 32;

	using FParameters = FPostProcessPassParameters;

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

IMPLEMENT_GLOBAL_SHADER(
	FRDGPostProcessOutlineCS,
	"/Plugin/RDGPostProcess/Private/OutlineCompositeCS.usf",
	"MainCS",
	SF_Compute);

namespace
{
FRDGTextureRef AddPostProcessChainForView(
	FRDGBuilder& GraphBuilder,
	const FViewInfo& View,
	FRDGTextureRef SceneColorTexture,
	FRDGTextureRef SceneDepthTexture,
	FRDGTextureRef SceneNormalTexture,
	const FRDGResolvedPostProcessSettings& Settings)
{
	if ((!Settings.bEnabled && !Settings.bOutlineEnabled) || SceneColorTexture == nullptr)
	{
		return SceneColorTexture;
	}

	if (SceneColorTexture->Desc.NumSamples != 1 || SceneDepthTexture == nullptr || SceneDepthTexture->Desc.NumSamples != 1)
	{
		return SceneColorTexture;
	}

	if (SceneNormalTexture == nullptr || SceneNormalTexture->Desc.NumSamples != 1)
	{
		return SceneColorTexture;
	}

	const FIntRect ViewRect = View.ViewRect;
	const FIntPoint ViewRectSize = ViewRect.Size();
	if (ViewRectSize.X <= 0 || ViewRectSize.Y <= 0)
	{
		return SceneColorTexture;
	}

	TShaderMapRef<FRDGPostProcessKuwaharaCS> ComputeShader(View.ShaderMap);
	TShaderMapRef<FRDGPostProcessOutlineCS> OutlineShader(View.ShaderMap);

	FRDGTextureRef ChainedColorTexture = SceneColorTexture;
	const FRDGTextureDesc OutputDesc = CreatePostProcessOutputDesc(SceneColorTexture);

	if (Settings.bEnabled)
	{
		FRDGTextureRef KuwaharaOutputTexture = GraphBuilder.CreateTexture(
			OutputDesc,
			TEXT("RDGPostProcess.KuwaharaOutput.PreTAA"));

		AddCopyTexturePass(GraphBuilder, ChainedColorTexture, KuwaharaOutputTexture);

		FKuwaharaPassParameters* KuwaharaParameters = GraphBuilder.AllocParameters<FKuwaharaPassParameters>();
		KuwaharaParameters->Radius = static_cast<uint32>(FMath::RoundToInt(FMath::Max(Settings.Radius, 0.0f)));
		KuwaharaParameters->ViewRectMin = ViewRect.Min;
		KuwaharaParameters->ViewRectSize = ViewRectSize;
		KuwaharaParameters->InputTexture = GraphBuilder.CreateSRV(FRDGTextureSRVDesc::Create(ChainedColorTexture));
		KuwaharaParameters->OutputTexture = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(KuwaharaOutputTexture));

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("RDGPostProcess::KuwaharaFilterPreTAA"),
			ComputeShader,
			KuwaharaParameters,
			FComputeShaderUtils::GetGroupCount(
				ViewRectSize,
				FIntPoint(
					FRDGPostProcessKuwaharaCS::ThreadGroupSize,
					FRDGPostProcessKuwaharaCS::ThreadGroupSize)));

		ChainedColorTexture = KuwaharaOutputTexture;
	}

	if (Settings.bOutlineEnabled)
	{
		FRDGTextureRef OutlineOutputTexture = GraphBuilder.CreateTexture(
			OutputDesc,
			TEXT("RDGPostProcess.OutlineOutput.PreTAA"));

		AddCopyTexturePass(GraphBuilder, ChainedColorTexture, OutlineOutputTexture);

		FPostProcessPassParameters* OutlineParameters = GraphBuilder.AllocParameters<FPostProcessPassParameters>();
		OutlineParameters->View = View.ViewUniformBuffer;
		OutlineParameters->ViewRectMin = ViewRect.Min;
		OutlineParameters->ViewRectSize = ViewRectSize;
		OutlineParameters->OutlineColor = FVector4f(Settings.OutlineSettings.OutlineColor);
		OutlineParameters->EdgeThreshold = Settings.OutlineSettings.EdgeThreshold;
		OutlineParameters->Softness = Settings.OutlineSettings.Softness;
		OutlineParameters->GlowIntensity = Settings.OutlineSettings.GlowIntensity;
		OutlineParameters->PulseSpeed = Settings.OutlineSettings.PulseSpeed;
		OutlineParameters->InputTexture = GraphBuilder.CreateSRV(FRDGTextureSRVDesc::Create(ChainedColorTexture));
		OutlineParameters->SceneDepthTexture = GraphBuilder.CreateSRV(
			FRDGTextureSRVDesc::CreateForMetaData(SceneDepthTexture, ERDGTextureMetaDataAccess::Depth));
		OutlineParameters->SceneNormalTexture = GraphBuilder.CreateSRV(FRDGTextureSRVDesc::Create(SceneNormalTexture));
		OutlineParameters->InputSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
		OutlineParameters->SceneDepthSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
		OutlineParameters->SceneNormalSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
		OutlineParameters->OutputTexture = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(OutlineOutputTexture));

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("RDGPostProcess::OutlineCompositePreTAA"),
			OutlineShader,
			OutlineParameters,
			FComputeShaderUtils::GetGroupCount(
				ViewRectSize,
				FIntPoint(
					FRDGPostProcessOutlineCS::ThreadGroupSize,
					FRDGPostProcessOutlineCS::ThreadGroupSize)));

		ChainedColorTexture = OutlineOutputTexture;
	}

	if (ChainedColorTexture != SceneColorTexture)
	{
		AddCopyTexturePass(GraphBuilder, ChainedColorTexture, SceneColorTexture, ViewRect.Min, ViewRect.Min, ViewRectSize);
	}

	return SceneColorTexture;
}
}

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
	// Resolve active volume/provider data on the game thread; actor iteration is not render-thread safe.
	const FRDGResolvedPostProcessSettings Snapshot = BuildSettingsSnapshot(InViewFamily);

	ENQUEUE_RENDER_COMMAND(RDGPostProcess_UpdateSettings)(
		[this, Snapshot](FRHICommandListImmediate& RHICmdList)
		{
			RenderThreadSettings = Snapshot;
		});
	SettingsUploadFence.BeginFence();
}

void FRDGPostProcessSceneViewExtension::PreRenderViewFamily_RenderThread(
	FRDGBuilder& GraphBuilder,
	FSceneViewFamily& InViewFamily)
{
	(void)GraphBuilder;
	(void)InViewFamily;

	// Scene settings are resolved on the game thread; only render-thread-safe overrides belong here.
	ApplyRenderThreadOverrides(RenderThreadSettings);
}

void FRDGPostProcessSceneViewExtension::PrePostProcessPass_RenderThread(
	FRDGBuilder& GraphBuilder,
	const FSceneView& View,
	const FPostProcessingInputs& Inputs)
{
	const FRDGResolvedPostProcessSettings Settings = RenderThreadSettings;
	if (!Settings.bEnabled && !Settings.bOutlineEnabled)
	{
		return;
	}

	FRDGTextureRef SceneColorTexture = (*Inputs.SceneTextures)->SceneColorTexture;
	FRDGTextureRef SceneDepthTexture = (*Inputs.SceneTextures)->SceneDepthTexture;
	FRDGTextureRef SceneNormalTexture = (*Inputs.SceneTextures)->GBufferATexture;
	if (SceneNormalTexture == nullptr)
	{
		SceneNormalTexture = GSystemTextures.GetBlackDummy(GraphBuilder);
	}

	AddPostProcessChainForView(
		GraphBuilder,
		static_cast<const FViewInfo&>(View),
		SceneColorTexture,
		SceneDepthTexture,
		SceneNormalTexture,
		Settings);
}

void FRDGPostProcessSceneViewExtension::PostRenderViewFamily_RenderThread(
	FRDGBuilder& GraphBuilder,
	FSceneViewFamily& InViewFamily)
{
	(void)GraphBuilder;
	(void)InViewFamily;
}

bool FRDGPostProcessSceneViewExtension::IsActiveThisFrame_Internal(const FSceneViewExtensionContext& Context) const
{
	return true;
}

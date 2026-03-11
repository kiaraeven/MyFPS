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
constexpr float GlobalProviderPriority = -1000000.0f;

FRDGResolvedPostProcessSettings BuildDefaultSettings()
{
	FRDGResolvedPostProcessSettings Settings;
	Settings.bEnabled = CVarRDGPostProcessEnable.GetValueOnGameThread() != 0;
	Settings.Radius = FMath::Max(static_cast<float>(CVarRDGPostProcessKuwaharaRadius.GetValueOnGameThread()), 0.0f);
	return Settings;
}

void SanitizeSettings(FRDGResolvedPostProcessSettings& Settings)
{
	Settings.Radius = FMath::Max(Settings.Radius, 0.0f);
	Settings.OutlineSettings.EdgeThreshold = FMath::Max(Settings.OutlineSettings.EdgeThreshold, 0.0f);
	Settings.OutlineSettings.GlowIntensity = FMath::Max(Settings.OutlineSettings.GlowIntensity, 0.0f);
	Settings.OutlineSettings.PulseSpeed = FMath::Max(Settings.OutlineSettings.PulseSpeed, 0.0f);
	Settings.OutlineSettings.Softness = FMath::Clamp(Settings.OutlineSettings.Softness, 0.0f, 1.0f);
	Settings.OutlineSettings.GlitchIntensity = FMath::Clamp(Settings.OutlineSettings.GlitchIntensity, 0.0f, 1.0f);
	Settings.OutlineSettings.RGBShiftAmount = FMath::Max(Settings.OutlineSettings.RGBShiftAmount, 0.0f);
	Settings.OutlineSettings.BlockSliceAmount = FMath::Clamp(Settings.OutlineSettings.BlockSliceAmount, 0.0f, 1.0f);
	Settings.OutlineSettings.StaticNoiseLevel = FMath::Clamp(Settings.OutlineSettings.StaticNoiseLevel, 0.0f, 1.0f);
}

void ApplyRenderThreadOverrides(FRDGResolvedPostProcessSettings& Settings)
{
	SanitizeSettings(Settings);

	const float SoftnessOverride = CVarOutlineSoftness.GetValueOnRenderThread();
	if (SoftnessOverride >= 0.0f)
	{
		Settings.OutlineSettings.Softness = FMath::Clamp(SoftnessOverride, 0.0f, 1.0f);
	}
}

bool HasGlitchEffect(const FRDGResolvedPostProcessSettings& Settings)
{
	return Settings.bGlitchEnabled &&
		Settings.OutlineSettings.GlitchIntensity > KINDA_SMALL_NUMBER &&
		(Settings.OutlineSettings.RGBShiftAmount > KINDA_SMALL_NUMBER ||
			Settings.OutlineSettings.BlockSliceAmount > KINDA_SMALL_NUMBER ||
			Settings.OutlineSettings.StaticNoiseLevel > KINDA_SMALL_NUMBER);
}

bool HasNPRFinalPass(const FRDGResolvedPostProcessSettings& Settings)
{
	return Settings.bOutlineEnabled || HasGlitchEffect(Settings);
}

float ComputeVolumeBlendWeight(APostProcessVolume* Volume, const FVector& ViewLocation)
{
	if (!IsValid(Volume) || !Volume->bEnabled || Volume->BlendWeight <= 0.0f)
	{
		return 0.0f;
	}

	float DistanceToPoint = 0.0f;
	const bool bAffectsView = Volume->bUnbound || Volume->EncompassesPoint(ViewLocation, Volume->BlendRadius, &DistanceToPoint);
	if (!bAffectsView)
	{
		return 0.0f;
	}

	const float BaseWeight = FMath::Clamp(Volume->BlendWeight, 0.0f, 1.0f);
	if (Volume->bUnbound || Volume->BlendRadius <= 0.0f)
	{
		return BaseWeight;
	}

	const float BlendRadius = FMath::Max(Volume->BlendRadius, 1.0f);
	const float Falloff = 1.0f - FMath::Clamp(DistanceToPoint / BlendRadius, 0.0f, 1.0f);
	return BaseWeight * Falloff;
}

struct FWeightedSettingsContribution
{
	FRDGResolvedPostProcessSettings Settings;
	float Weight = 0.0f;
	float Priority = GlobalProviderPriority;
};

void BlendSettingsContribution(
	FRDGResolvedPostProcessSettings& InOutSettings,
	float& InOutKuwaharaFactor,
	float& InOutOutlineFactor,
	float& InOutGlitchFactor,
	const FWeightedSettingsContribution& Contribution)
{
	const float Weight = FMath::Clamp(Contribution.Weight, 0.0f, 1.0f);
	if (Weight <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	const FRDGResolvedPostProcessSettings& Source = Contribution.Settings;
	InOutKuwaharaFactor = FMath::Lerp(InOutKuwaharaFactor, Source.bEnabled ? 1.0f : 0.0f, Weight);
	InOutOutlineFactor = FMath::Lerp(InOutOutlineFactor, Source.bOutlineEnabled ? 1.0f : 0.0f, Weight);
	InOutGlitchFactor = FMath::Lerp(InOutGlitchFactor, Source.bGlitchEnabled ? 1.0f : 0.0f, Weight);
	InOutSettings.Radius = FMath::Lerp(InOutSettings.Radius, Source.Radius, Weight);
	InOutSettings.OutlineSettings.OutlineColor = FMath::Lerp(InOutSettings.OutlineSettings.OutlineColor, Source.OutlineSettings.OutlineColor, Weight);
	InOutSettings.OutlineSettings.EdgeThreshold = FMath::Lerp(InOutSettings.OutlineSettings.EdgeThreshold, Source.OutlineSettings.EdgeThreshold, Weight);
	InOutSettings.OutlineSettings.Softness = FMath::Lerp(InOutSettings.OutlineSettings.Softness, Source.OutlineSettings.Softness, Weight);
	InOutSettings.OutlineSettings.GlowIntensity = FMath::Lerp(InOutSettings.OutlineSettings.GlowIntensity, Source.OutlineSettings.GlowIntensity, Weight);
	InOutSettings.OutlineSettings.PulseSpeed = FMath::Lerp(InOutSettings.OutlineSettings.PulseSpeed, Source.OutlineSettings.PulseSpeed, Weight);
	InOutSettings.OutlineSettings.GlitchIntensity = FMath::Lerp(InOutSettings.OutlineSettings.GlitchIntensity, Source.OutlineSettings.GlitchIntensity, Weight);
	InOutSettings.OutlineSettings.RGBShiftAmount = FMath::Lerp(InOutSettings.OutlineSettings.RGBShiftAmount, Source.OutlineSettings.RGBShiftAmount, Weight);
	InOutSettings.OutlineSettings.BlockSliceAmount = FMath::Lerp(InOutSettings.OutlineSettings.BlockSliceAmount, Source.OutlineSettings.BlockSliceAmount, Weight);
	InOutSettings.OutlineSettings.StaticNoiseLevel = FMath::Lerp(InOutSettings.OutlineSettings.StaticNoiseLevel, Source.OutlineSettings.StaticNoiseLevel, Weight);
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
	TArray<FWeightedSettingsContribution, TInlineAllocator<8>> Contributions;

	for (TActorIterator<ARDGPostProcessSettingsProvider> ProviderIt(World); ProviderIt; ++ProviderIt)
	{
		const ARDGPostProcessSettingsProvider* Provider = *ProviderIt;
		if (!IsValid(Provider))
		{
			continue;
		}

		FRDGResolvedPostProcessSettings ProviderSettings;
		if (!Provider->ResolveSettings(ProviderSettings))
		{
			continue;
		}

		const float Weight = Provider->TargetVolume != nullptr
			? ComputeVolumeBlendWeight(Provider->TargetVolume, ViewLocation)
			: 1.0f;
		if (Weight <= KINDA_SMALL_NUMBER)
		{
			continue;
		}

		FWeightedSettingsContribution& Contribution = Contributions.AddDefaulted_GetRef();
		Contribution.Settings = ProviderSettings;
		Contribution.Weight = Weight;
		Contribution.Priority = Provider->TargetVolume != nullptr
			? Provider->TargetVolume->Priority
			: GlobalProviderPriority;
	}

	if (Contributions.Num() == 0)
	{
		SanitizeSettings(Snapshot);
		return Snapshot;
	}

	Contributions.Sort([](const FWeightedSettingsContribution& A, const FWeightedSettingsContribution& B)
	{
		if (!FMath::IsNearlyEqual(A.Priority, B.Priority))
		{
			return A.Priority < B.Priority;
		}

		return A.Weight < B.Weight;
	});

	float KuwaharaFactor = Snapshot.bEnabled ? 1.0f : 0.0f;
	float OutlineFactor = Snapshot.bOutlineEnabled ? 1.0f : 0.0f;
	float GlitchFactor = Snapshot.bGlitchEnabled ? 1.0f : 0.0f;
	for (const FWeightedSettingsContribution& Contribution : Contributions)
	{
		BlendSettingsContribution(Snapshot, KuwaharaFactor, OutlineFactor, GlitchFactor, Contribution);
	}

	Snapshot.bEnabled = KuwaharaFactor > 0.5f;
	Snapshot.bOutlineEnabled = OutlineFactor > 0.5f;
	Snapshot.bGlitchEnabled = GlitchFactor > 0.5f;
	SanitizeSettings(Snapshot);
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

BEGIN_SHADER_PARAMETER_STRUCT(FNPRFinalPassParameters, )
	SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
	SHADER_PARAMETER(FIntPoint, ViewRectMin)
	SHADER_PARAMETER(FIntPoint, ViewRectSize)
	SHADER_PARAMETER(float, OutlineIntensity)
	SHADER_PARAMETER(FVector4f, OutlineColor)
	SHADER_PARAMETER(float, EdgeThreshold)
	SHADER_PARAMETER(float, Softness)
	SHADER_PARAMETER(float, GlowIntensity)
	SHADER_PARAMETER(float, PulseSpeed)
	SHADER_PARAMETER(float, GlitchIntensity)
	SHADER_PARAMETER(float, RGBShiftAmount)
	SHADER_PARAMETER(float, BlockSliceAmount)
	SHADER_PARAMETER(float, StaticNoiseLevel)
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

class FRDGPostProcessNPRFinalCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FRDGPostProcessNPRFinalCS);
	SHADER_USE_PARAMETER_STRUCT(FRDGPostProcessNPRFinalCS, FGlobalShader);

	static constexpr int32 ThreadGroupSize = 32;

	using FParameters = FNPRFinalPassParameters;

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
	FRDGPostProcessNPRFinalCS,
	"/Plugin/RDGPostProcess/Private/NPRFinalPassCS.usf",
	"MainCS",
	SF_Compute);

namespace
{
FRDGTextureRef AddMergedPostProcessChainForView(
	FRDGBuilder& GraphBuilder,
	const FViewInfo& View,
	FRDGTextureRef SceneColorTexture,
	FRDGTextureRef SceneDepthTexture,
	FRDGTextureRef SceneNormalTexture,
	const FRDGResolvedPostProcessSettings& Settings)
{
	const bool bShouldRunKuwahara = Settings.bEnabled;
	const bool bShouldRunNPRFinalPass = HasNPRFinalPass(Settings);
	if ((!bShouldRunKuwahara && !bShouldRunNPRFinalPass) || SceneColorTexture == nullptr)
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
	TShaderMapRef<FRDGPostProcessNPRFinalCS> NPRFinalShader(View.ShaderMap);

	FRDGTextureRef ChainedColorTexture = SceneColorTexture;
	const FRDGTextureDesc OutputDesc = CreatePostProcessOutputDesc(SceneColorTexture);

	if (bShouldRunKuwahara)
	{
		FRDGTextureRef KuwaharaOutputTexture = GraphBuilder.CreateTexture(
			OutputDesc,
			TEXT("RDGPostProcess.KuwaharaOutput.PreTAA"));

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

	if (bShouldRunNPRFinalPass)
	{
		FRDGTextureRef NPRFinalOutputTexture = GraphBuilder.CreateTexture(
			OutputDesc,
			TEXT("RDGPostProcess.NPRFinalOutput.PreTAA"));

		FNPRFinalPassParameters* NPRFinalParameters = GraphBuilder.AllocParameters<FNPRFinalPassParameters>();
		NPRFinalParameters->View = View.ViewUniformBuffer;
		NPRFinalParameters->ViewRectMin = ViewRect.Min;
		NPRFinalParameters->ViewRectSize = ViewRectSize;
		NPRFinalParameters->OutlineIntensity = Settings.bOutlineEnabled ? 1.0f : 0.0f;
		NPRFinalParameters->OutlineColor = FVector4f(Settings.OutlineSettings.OutlineColor);
		NPRFinalParameters->EdgeThreshold = Settings.OutlineSettings.EdgeThreshold;
		NPRFinalParameters->Softness = Settings.OutlineSettings.Softness;
		NPRFinalParameters->GlowIntensity = Settings.OutlineSettings.GlowIntensity;
		NPRFinalParameters->PulseSpeed = Settings.OutlineSettings.PulseSpeed;
		NPRFinalParameters->GlitchIntensity = Settings.bGlitchEnabled ? Settings.OutlineSettings.GlitchIntensity : 0.0f;
		NPRFinalParameters->RGBShiftAmount = Settings.OutlineSettings.RGBShiftAmount;
		NPRFinalParameters->BlockSliceAmount = Settings.OutlineSettings.BlockSliceAmount;
		NPRFinalParameters->StaticNoiseLevel = Settings.OutlineSettings.StaticNoiseLevel;
		NPRFinalParameters->InputTexture = GraphBuilder.CreateSRV(FRDGTextureSRVDesc::Create(ChainedColorTexture));
		NPRFinalParameters->SceneDepthTexture = GraphBuilder.CreateSRV(
			FRDGTextureSRVDesc::CreateForMetaData(SceneDepthTexture, ERDGTextureMetaDataAccess::Depth));
		NPRFinalParameters->SceneNormalTexture = GraphBuilder.CreateSRV(FRDGTextureSRVDesc::Create(SceneNormalTexture));
		NPRFinalParameters->InputSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
		NPRFinalParameters->SceneDepthSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
		NPRFinalParameters->SceneNormalSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
		NPRFinalParameters->OutputTexture = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(NPRFinalOutputTexture));

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("RDGPostProcess::NPRFinalPassPreTAA"),
			NPRFinalShader,
			NPRFinalParameters,
			FComputeShaderUtils::GetGroupCount(
				ViewRectSize,
				FIntPoint(
					FRDGPostProcessNPRFinalCS::ThreadGroupSize,
					FRDGPostProcessNPRFinalCS::ThreadGroupSize)));

		ChainedColorTexture = NPRFinalOutputTexture;
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
	if (!Settings.bEnabled && !HasNPRFinalPass(Settings))
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

	AddMergedPostProcessChainForView(
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

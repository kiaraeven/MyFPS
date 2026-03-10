#pragma once

#include "CoreMinimal.h"
#include "RenderCommandFence.h"
#include "SceneViewExtension.h"

class FRDGBuilder;

class FRDGPostProcessSceneViewExtension final : public FSceneViewExtensionBase
{
public:
	struct FKuwaharaSettingsSnapshot
	{
		bool bEnabled = false;
		float Radius = 0.0f;
	};

	FRDGPostProcessSceneViewExtension(const FAutoRegister& AutoRegister);
	virtual ~FRDGPostProcessSceneViewExtension() override;

	virtual void SetupViewFamily(FSceneViewFamily& InViewFamily) override;

	virtual void SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView) override;

	virtual void BeginRenderViewFamily(FSceneViewFamily& InViewFamily) override;

	virtual void PostRenderViewFamily_RenderThread(FRDGBuilder& GraphBuilder, FSceneViewFamily& InViewFamily) override;

	virtual bool IsActiveThisFrame_Internal(const FSceneViewExtensionContext& Context) const override;

private:
	FKuwaharaSettingsSnapshot RenderThreadSettings;
	FRenderCommandFence SettingsUploadFence;
};

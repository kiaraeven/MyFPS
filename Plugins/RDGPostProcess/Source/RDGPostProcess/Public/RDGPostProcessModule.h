#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

DECLARE_LOG_CATEGORY_EXTERN(LogRDGPostProcess, Log, All);

class FRDGPostProcessModule final : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	void OnPostEngineInit();

	TSharedPtr<class FRDGPostProcessSceneViewExtension, ESPMode::ThreadSafe> SceneViewExtension;
};

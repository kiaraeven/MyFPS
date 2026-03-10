#include "RDGPostProcessModule.h"

#include "RDGPostProcessSceneViewExtension.h"

#include "Engine/Engine.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/App.h"
#include "Misc/CoreDelegates.h"
#include "Misc/Paths.h"
#include "ShaderCore.h"

DEFINE_LOG_CATEGORY(LogRDGPostProcess);

void FRDGPostProcessModule::StartupModule()
{
	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("RDGPostProcess"));
	check(Plugin.IsValid());

	const FString ShaderDirectory = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Shaders"));
	if (!FPaths::DirectoryExists(ShaderDirectory))
	{
		UE_LOG(LogRDGPostProcess, Error, TEXT("Shader directory not found: %s"), *ShaderDirectory);
		return;
	}

	AddShaderSourceDirectoryMapping(TEXT("/Plugin/RDGPostProcess"), ShaderDirectory);

	if (!FApp::CanEverRender())
	{
		return;
	}

	if (GEngine != nullptr)
	{
		OnPostEngineInit();
	}
	else
	{
		FCoreDelegates::OnPostEngineInit.AddRaw(this, &FRDGPostProcessModule::OnPostEngineInit);
	}
}

void FRDGPostProcessModule::ShutdownModule()
{
	SceneViewExtension.Reset();
	FCoreDelegates::OnPostEngineInit.RemoveAll(this);
}

void FRDGPostProcessModule::OnPostEngineInit()
{
	if (!FApp::CanEverRender() || SceneViewExtension.IsValid())
	{
		return;
	}

	SceneViewExtension = FSceneViewExtensions::NewExtension<FRDGPostProcessSceneViewExtension>();
	UE_LOG(LogRDGPostProcess, Log, TEXT("Registered RDG post-process scene view extension."));
}

IMPLEMENT_MODULE(FRDGPostProcessModule, RDGPostProcess)

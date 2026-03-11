using System.IO;
using UnrealBuildTool;

public class RDGPostProcess : ModuleRules
{
	public RDGPostProcess(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new[]
			{
				"Core",
				"CoreUObject",
				"Engine",
			});

		PrivateDependencyModuleNames.AddRange(
			new[]
			{
				"Projects",
				"RenderCore",
				"Renderer",
				"RHI",
			});

		PrivateIncludePaths.AddRange(
			new[]
			{
				Path.Combine(EngineDirectory, "Source", "Runtime", "Renderer", "Internal"),
				Path.Combine(EngineDirectory, "Source", "Runtime", "Renderer", "Private"),
			});
	}
}

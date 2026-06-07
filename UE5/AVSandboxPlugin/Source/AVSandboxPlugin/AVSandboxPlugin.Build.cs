using UnrealBuildTool;

public class AVSandboxPlugin : ModuleRules
{
	public AVSandboxPlugin(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"Sockets",
			"Networking",
			"XMLParser",
			"ChaosVehicles",
			"NavigationSystem",
			"Spline"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"CoreUObject",
			"Engine",
			"Slate",
			"SlateCore"
		});

		bEnforceIWYU = false;
	}
}

using System.IO;
using UnrealBuildTool;

public class DPPLibrary : ModuleRules
{
	public DPPLibrary(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;
		
		// This is equivalent to setting C++ Language Standard in Visual Studio (Image 3)
		CppStandard = CppStandardVersion.Cpp20;
		
		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			// Path to the DPP library - relative to this module directory
			// Users should extract libdpp-10.1.4-win64 to Source/ThirdParty/DPPLibrary/
			string DPPPath = Path.Combine(ModuleDirectory, "libdpp-10.1.4-win64");
			
			// This is equivalent to "Include Directories" in Visual Studio (Image 1)
			PublicIncludePaths.Add(Path.Combine(DPPPath, "include", "dpp-10.1"));
			
			// This is equivalent to "Library Directories" in Visual Studio (Image 2)
			string LibPath = Path.Combine(DPPPath, "lib", "dpp-10.1");
			
			// This is equivalent to "Additional Dependencies" (dpp.lib) in Visual Studio (Image 4)
			PublicAdditionalLibraries.Add(Path.Combine(LibPath, "dpp.lib"));
			
			// Runtime dependencies - copy DLLs to mod's Binaries folder
			// Don't use PublicDelayLoadDLLs - it requires DLLs in Windows search path
			string BinPath = Path.Combine(DPPPath, "bin");
			
			// Stage DLLs to the mod's Binaries/Win64 folder
			RuntimeDependencies.Add("$(PluginDir)/Binaries/Win64/dpp.dll", Path.Combine(BinPath, "dpp.dll"));
			RuntimeDependencies.Add("$(PluginDir)/Binaries/Win64/libcrypto-1_1-x64.dll", Path.Combine(BinPath, "libcrypto-1_1-x64.dll"));
			RuntimeDependencies.Add("$(PluginDir)/Binaries/Win64/libssl-1_1-x64.dll", Path.Combine(BinPath, "libssl-1_1-x64.dll"));
			RuntimeDependencies.Add("$(PluginDir)/Binaries/Win64/opus.dll", Path.Combine(BinPath, "opus.dll"));
			RuntimeDependencies.Add("$(PluginDir)/Binaries/Win64/zlib1.dll", Path.Combine(BinPath, "zlib1.dll"));
		}
	}
}

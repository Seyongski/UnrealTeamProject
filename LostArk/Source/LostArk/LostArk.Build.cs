// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class LostArk : ModuleRules
{
	public LostArk(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore",
			"GameplayAbilities","GameplayTags","GameplayTasks","NavigationSystem", "AIModule", "Niagara", "EnhancedInput", "ProceduralMeshComponent" });

		// 모듈 루트(LostArkGameMode.h 등 템플릿 헤더)를 인클루드 경로에 추가 -> 하위 폴더 헤더에서 참조 가능
		PublicIncludePaths.Add(ModuleDirectory);
    }
}

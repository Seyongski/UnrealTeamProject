// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class LostArk : ModuleRules
{
	public LostArk(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore",
			"GameplayAbilities","GameplayTags","GameplayTasks","NavigationSystem", "AIModule", "Niagara", "EnhancedInput", "ProceduralMeshComponent",
			"UMG" });	// UMG: C++에서 UWidgetComponent(머리 위 전하 아이콘/발밑 게이지)를 폰에 붙이기 위함

		// 모듈 루트(LostArk.h 모듈 헤더)를 인클루드 경로에 추가.
		// 그 외 헤더는 Public/<Category>/ 아래에 있으므로 "Category/Header.h" 서브패스로 참조.
		PublicIncludePaths.Add(ModuleDirectory);
    }
}

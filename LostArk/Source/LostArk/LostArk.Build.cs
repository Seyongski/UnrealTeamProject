// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class LostArk : ModuleRules
{
	public LostArk(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore",
			"GameplayAbilities","GameplayTags","GameplayTasks","NavigationSystem", "AIModule", "Niagara", "EnhancedInput", "ProceduralMeshComponent",
			"UMG", "Slate", "SlateCore" });	// UMG: 폰에 UWidgetComponent 부착 / Slate,SlateCore: Player HUD·데미지텍스트 UI

		// 모듈 루트(LostArk.h 모듈 헤더)를 인클루드 경로에 추가.
		// 그 외 헤더는 Public/<Category>/ 아래에 있으므로 "Category/Header.h" 서브패스로 참조.
		PublicIncludePaths.Add(ModuleDirectory);
    }
}

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "SwordSpiritVfxEditorLibrary.generated.h"

class UMaterialInterface;
class UStaticMesh;

/**
 * Authors the special sword-spirit assets: the fourteen NS_SS_* Niagara systems spawned through LiveSwordVfx
 * (shared contract: User.Color plus the size parameters of each system), sword mesh normalisation and the procedural giant axe.
 */
UCLASS()
class BOUNDLESSIMMORTALITYEDITOR_API USwordSpiritVfxEditorLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    /**
     * Builds every system of Spec in Folder (existing assets must be deleted first); one report line per system plus
     * compile errors. Spec is the JSON written by Tools/SpecialSwords/special_sword_vfx.py: systems with their user
     * parameters and emitters whose attributes are custom HLSL; Materials maps the emitters' material keys.
     */
    UFUNCTION(BlueprintCallable,Category="Live Weapons|Editor")
    static TArray<FString> BuildSwordSpiritEffects(const FString& Folder,const TMap<FString,UMaterialInterface*>& Materials,const FString& Spec);

    /** Blocks until pending shader and asset compilation is done, so new materials are saved complete. */
    UFUNCTION(BlueprintCallable,Category="Live Weapons|Editor")
    static void FinishPendingCompilation();

    /** Turns the longest axis into +Z with the blade tip up and the guard at the origin, scales to Length, caps LOD0 triangles. */
    UFUNCTION(BlueprintCallable,Category="Live Weapons|Editor")
    static FString NormalizeSwordMesh(UStaticMesh* Mesh,float Length,int32 MaxTriangles=40000);

    /** Procedural great axe for the 无机态那 slam: grip end at the origin, head toward +Z, about 330 cm. */
    UFUNCTION(BlueprintCallable,Category="Live Weapons|Editor")
    static UStaticMesh* CreateGiantAxeMesh(const FString& PackagePath,UMaterialInterface* Material);
};

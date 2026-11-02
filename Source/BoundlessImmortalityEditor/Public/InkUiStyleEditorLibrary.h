#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "InkUiStyleEditorLibrary.generated.h"

/** UI asset maintenance: move assets inside /Game/UI and fix up redirectors left behind. */
UCLASS()
class BOUNDLESSIMMORTALITYEDITOR_API UInkUiStyleEditorLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    /** Explicit UI-only relocation for native CDO-referenced assets; retains redirectors. */
    UFUNCTION(BlueprintCallable,Category="Ink UI|Editor") static bool RelocateUiAsset(UObject* Asset,const FString& DestinationPackage);
    UFUNCTION(BlueprintCallable,Category="Ink UI|Editor") static void FixUiRedirectors();
};

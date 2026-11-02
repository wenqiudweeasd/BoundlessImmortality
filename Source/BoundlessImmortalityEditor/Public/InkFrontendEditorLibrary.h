#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "InputCoreTypes.h"
#include "InkFrontendEditorLibrary.generated.h"
class ULiveGMOverlayWidget;
/** PIE helpers for the verification scripts: start a sized PIE session, resize it, capture it, click GM buttons. */
UCLASS()
class BOUNDLESSIMMORTALITYEDITOR_API UInkFrontendEditorLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable,Category="Ink Frontend|Editor") static FString VerifyDevelopmentMutationRefusal(UObject* WorldContext);
    UFUNCTION(BlueprintCallable,Category="Ink Frontend|Editor") static bool ClickGMAction(ULiveGMOverlayWidget* Widget,FName Action);
    /** Dispatches without moving focus, for focused editable-text keyboard routing checks. */
    UFUNCTION(BlueprintCallable,Category="Ink Frontend|Editor") static bool PressFocusedFrontendKey(FKey Key);
    UFUNCTION(BlueprintCallable,Category="Ink Frontend|Editor") static bool SetPIEViewportSize(int32 Width,int32 Height);
    UFUNCTION(BlueprintCallable,Category="Ink Frontend|Editor") static bool BeginFrontendPIE(int32 Width,int32 Height);
    UFUNCTION(BlueprintCallable,Category="Ink Frontend|Editor") static bool CaptureFrontendViewport(const FString& Filename);
};

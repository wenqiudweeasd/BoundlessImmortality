#pragma once
#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "LiveAnchorController.generated.h"
class ACameraActor;
class USpringArmComponent;
class ULiveAnchorAbilityComponent;
UCLASS()
class BOUNDLESSIMMORTALITY_API ALiveAnchorController : public APlayerController
{
    GENERATED_BODY()
public:
    ALiveAnchorController();
    UFUNCTION(Exec) void LiveSpawnMonsters(int32 Count);
    virtual void PlayerTick(float DeltaSeconds)override;
    bool IsUsingGameplayCamera()const;
    /**
     * Mouse-wheel zoom as a share of the game framing (1): the pawn camera's boom length, or the orthographic width.
     * Scrolling up pulls the camera in (each notch x0.85) down to MinZoom, to look at effects up close.
     */
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Camera",meta=(ClampMin="0.05",ClampMax="1")) float MinZoom=.15f;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Camera",meta=(ClampMin="1",ClampMax="4")) float MaxZoom=1.5f;
    UFUNCTION(BlueprintCallable,Category="Camera") void SetCameraZoom(float NewZoom,bool bInstant=false);
    UFUNCTION(BlueprintPure,Category="Camera") float GetCameraZoom()const{return Zoom;}
protected:
    virtual void OnPossess(APawn* InPawn)override;
private:
    UPROPERTY(Transient) TObjectPtr<ACameraActor> LiveCamera;
    UPROPERTY(Transient) TObjectPtr<ULiveAnchorAbilityComponent> Ability;
    TWeakObjectPtr<USpringArmComponent> Boom;
    float Zoom=1.f,ZoomGoal=1.f,BoomLength=0.f,OrthoWidth=2600.f;
    void TickZoom(float DeltaSeconds);
};

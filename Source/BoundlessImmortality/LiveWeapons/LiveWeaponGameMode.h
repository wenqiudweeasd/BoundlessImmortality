#pragma once
#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "LiveWeaponGameMode.generated.h"
class ALiveWeaponSwarm;
class ALiveMonster;
class ULiveAudienceAdapterComponent;
UCLASS(Blueprintable)
class BOUNDLESSIMMORTALITY_API ALiveWeaponGameMode : public AGameModeBase
{
    GENERATED_BODY()
public:
    ALiveWeaponGameMode();
    UPROPERTY(EditAnywhere,BlueprintReadWrite) TSubclassOf<ALiveWeaponSwarm> SwarmClass;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) TSubclassOf<ALiveMonster> MonsterClass;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) int32 InitialMonsters=12;
    /** Overhead orthographic live camera; off keeps the view on the pawn's own camera, as in the main game. */
    UPROPERTY(EditAnywhere,BlueprintReadWrite) bool bOrthographicCamera=true;
    /** Passed to the swarm before it takes the anchor, so a mode without the anchor's own sword never shows it. */
    UPROPERTY(EditAnywhere,BlueprintReadWrite) bool bAnchorPersonalSword=true;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TObjectPtr<ALiveWeaponSwarm> Swarm;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TObjectPtr<ULiveAudienceAdapterComponent> Audience;
    UFUNCTION(BlueprintCallable) void SpawnMonsters(int32 Count,bool bHighHealth=false);
    virtual void Tick(float DeltaSeconds)override;
private:
    bool bStarted=false;
};

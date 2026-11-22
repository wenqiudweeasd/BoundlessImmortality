#pragma once
#include "CoreMinimal.h"
#include "LiveWeaponGameMode.h"
#include "LiveWeapons/SpecialSwords/LiveSpecialSwordTypes.h"
#include "LiveSwordTestGameMode.generated.h"
class ALiveMonster;
class ALiveScarecrow;
class ALiveSpecialSword;
/**
 * Standalone game mode for the sword test range (/Game/LiveWeapons/Maps/L_SwordTestRange).
 */
UCLASS(Blueprintable)
class BOUNDLESSIMMORTALITY_API ALiveSwordTestGameMode : public ALiveWeaponGameMode
{
    GENERATED_BODY()
public:
    ALiveSwordTestGameMode();
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Test") TSoftClassPtr<APawn> CultivatorClass{FSoftObjectPath(TEXT("/Game/Isometric_World/Demo/Blueprints/BP_TopDownCharacter.BP_TopDownCharacter_C"))};
    /** Summon all five special swords at start. */
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Test") bool bSummonSpecialSwords=false;
    /** Ordinary flying swords joining alongside the special ones. */
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Test",meta=(ClampMin="0",ClampMax="100")) int32 ExtraViewers=0;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Test",meta=(ClampMin="0.5")) float AnchorReviveDelay=3.f;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Test") bool bGiftAttributeBonuses=false;
    UFUNCTION(BlueprintCallable,Category="Test") bool SetSpecialSword(ELiveSpecialSword Kind,bool bPresent);
    UFUNCTION(BlueprintCallable,Category="Test") void SetAllSpecialSwords(bool bPresent);
    UFUNCTION(BlueprintPure,Category="Test") bool HasSpecialSword(ELiveSpecialSword Kind)const;
    UFUNCTION(BlueprintPure,Category="Test") ALiveSpecialSword* FindSpecialSword(ELiveSpecialSword Kind)const;
    UFUNCTION(BlueprintPure,Category="Test") double GetSpecialSwordDamage(ELiveSpecialSword Kind)const;
    UFUNCTION(BlueprintPure,Category="Test") bool IsTestReady()const{return bTestReady;}
    UFUNCTION(BlueprintCallable,Category="Test") ALiveMonster* SpawnTestTarget(float MaxHealth,float Defense,int32 Realm,FVector Location);
    UFUNCTION(BlueprintCallable,Category="Test") bool StartTribulationTest();
    UFUNCTION(BlueprintCallable,Category="Test") bool AssistTribulation();
    UFUNCTION(BlueprintPure,Category="Test") bool IsTribulationActive()const;
    UFUNCTION(BlueprintPure,Category="Test") int32 GetAnchorRealm()const;
    UFUNCTION(BlueprintPure,Category="Test") FText GetTribulationStatus()const;
    UFUNCTION(BlueprintCallable,Category="Test") void SetTribulationOutcome(int32 Outcome);
    UFUNCTION(BlueprintPure,Category="Test") int32 GetTribulationOutcome()const{return TribOutcome;}
    UFUNCTION(BlueprintCallable,Category="Test") void SetStarWardOutcome(int32 Outcome);
    UFUNCTION(BlueprintPure,Category="Test") int32 GetStarWard()const;
    UFUNCTION(BlueprintPure,Category="Test") int32 GetStarWardChance()const;
    UFUNCTION(BlueprintCallable,Category="Test") void SetSwordRealm(ELiveSpecialSword Kind,int32 Realm);
    UFUNCTION(BlueprintCallable,Category="Test") void SetAllSwordRealms(int32 Realm);
    UFUNCTION(BlueprintPure,Category="Test") int32 GetSwordRealm(ELiveSpecialSword Kind)const;
    UFUNCTION(BlueprintPure,Category="Test") int32 GetCommonSwordRealm()const;
    UFUNCTION(BlueprintCallable,Category="Test") void StepSwordRealms(int32 Direction);
    UFUNCTION(BlueprintPure,Category="Test") FLiveSpecialSwordGrowth GetSwordGrowth(ELiveSpecialSword Kind)const;
    UFUNCTION(BlueprintPure,Category="Test") TArray<ALiveScarecrow*> GetScarecrows()const;
    UFUNCTION(BlueprintPure,Category="Test") ALiveScarecrow* GetNearestScarecrow()const;
    virtual void InitGame(const FString& MapName,const FString& Options,FString& ErrorMessage)override;
    virtual void Tick(float DeltaSeconds)override;
private:
    bool bTestReady=false;
    TWeakObjectPtr<class UCultivationHealthComponent> AnchorHealth;
    FTimerHandle ReviveTimer,RehearsalTimer,RealmTimer;
    int32 RehearsalRealm=INDEX_NONE,AssistHelpers=0,TribOutcome=0;
    TMap<FString,int32> RehearsalFailures;
    double RehearsalDaoHeart=0;
    TMap<ELiveSpecialSword,int32> SwordRealms;
    void EndRehearsal();
    FString ViewerId(ELiveSpecialSword Kind)const;
    void HoldSwordRealms();
    UFUNCTION() void HandleAnchorDied();
    void ReviveAnchor();
};

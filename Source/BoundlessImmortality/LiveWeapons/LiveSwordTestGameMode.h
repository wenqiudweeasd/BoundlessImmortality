#pragma once
#include "CoreMinimal.h"
#include "LiveWeaponGameMode.h"
#include "LiveWeapons/SpecialSwords/LiveSpecialSwordTypes.h"
#include "LiveSwordTestGameMode.generated.h"
class ALiveGMOverlay;
class ALiveMonster;
class ALiveScarecrow;
class ALiveSpecialSword;
class ULiveSwordTestPanel;
/**
 * Game mode of the sword test range (/Game/LiveWeapons/Maps/L_SwordTestRange): the main game's cultivator and camera,
 * no wandering monsters (the level's realm scarecrows, one every other room, are the targets), no anchor personal sword,
 * and local audience simulation switched on. The 试剑台 panel adds and removes the five special swords one at a time,
 * sets the realm they fight at, shows the nearest scarecrow, opens the 剑谱 and runs a tribulation (渡劫) to watch its
 * storm strike the anchor; F10 opens the GM panel, the mouse wheel zooms the camera.
 */
UCLASS(Blueprintable)
class BOUNDLESSIMMORTALITY_API ALiveSwordTestGameMode : public ALiveWeaponGameMode
{
    GENERATED_BODY()
public:
    ALiveSwordTestGameMode();
    /** The main map's gameplay cultivator (BP_TopDownGameMode's pawn), whose camera frames the game. /Game/1/BP_TopDown is only the menu's presentation copy. */
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Test") TSoftClassPtr<APawn> CultivatorClass{FSoftObjectPath(TEXT("/Game/Isometric_World/Demo/Blueprints/BP_TopDownCharacter.BP_TopDownCharacter_C"))};
    /** Summon all five at start; off by default, the panel adds them one at a time. */
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Test") bool bSummonSpecialSwords=false;
    /** Ordinary audience swords joining alongside the special ones. */
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Test",meta=(ClampMin="0",ClampMax="100")) int32 ExtraViewers=0;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Test") bool bShowSwordPanel=true;
    /** The range has no defeat flow: a downed anchor (GM damage tests) stands back up at full health after this delay. */
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Test",meta=(ClampMin="0.5")) float AnchorReviveDelay=3.f;
    /**
     * Off (default): gifts still unlock the sword forms, but their attribute power stays out of the range's numbers (超能喷射's
     * flat bonus, the 稀有宝箱 soul aura and 神秘空投's enemy reduction), so swords and scarecrows show their realm attributes.
     */
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Test") bool bGiftAttributeBonuses=false;
    UPROPERTY(Transient,VisibleAnywhere,BlueprintReadOnly,Category="Test") TObjectPtr<ALiveGMOverlay> Overlay;
    UPROPERTY(Transient,VisibleAnywhere,BlueprintReadOnly,Category="Test") TObjectPtr<ULiveSwordTestPanel> SwordPanel;
    /**
     * Each special sword flies for its own local test viewer, named after the sword: adding joins the viewer and gifts
     * its sword, removing takes the viewer out of the swarm so no ordinary audience sword is left behind.
     */
    UFUNCTION(BlueprintCallable,Category="Test") bool SetSpecialSword(ELiveSpecialSword Kind,bool bPresent);
    UFUNCTION(BlueprintCallable,Category="Test") void SetAllSpecialSwords(bool bPresent);
    UFUNCTION(BlueprintPure,Category="Test") bool HasSpecialSword(ELiveSpecialSword Kind)const;
    /** The flying sword, once the special-sword manager picked the viewer up (it scans every 0.3 s). */
    UFUNCTION(BlueprintPure,Category="Test") ALiveSpecialSword* FindSpecialSword(ELiveSpecialSword Kind)const;
    /** Damage the sword's viewer has dealt this session. */
    UFUNCTION(BlueprintPure,Category="Test") double GetSpecialSwordDamage(ELiveSpecialSword Kind)const;
    UFUNCTION(BlueprintPure,Category="Test") bool IsTestReady()const{return bTestReady;}
    /** A plain stationary target with the given health, defense and realm (-1 for none), for damage-formula checks. */
    UFUNCTION(BlueprintCallable,Category="Test") ALiveMonster* SpawnTestTarget(float MaxHealth,float Defense,int32 Realm,FVector Location);
    /**
     * A tribulation rehearsal: puts the anchor at 金丹六阶 圆满 with full health (GM 渡劫测试准备) and starts the
     * tribulation, 15 s of gathering clouds and then the 60 s trial whose lightning strikes the anchor. Once it has
     * settled and its storm is gone the anchor returns to the realm it had, so sword tests keep their numbers.
     * False when one is already running.
     */
    UFUNCTION(BlueprintCallable,Category="Test") bool StartTribulationTest();
    /** Ten comment-only helpers send "助力" during the trial: +10% success chance (50 at most), and the storm eases. */
    UFUNCTION(BlueprintCallable,Category="Test") bool AssistTribulation();
    /** A tribulation prepares, runs or is being settled, or its storm is still fading. */
    UFUNCTION(BlueprintPure,Category="Test") bool IsTribulationActive()const;
    UFUNCTION(BlueprintPure,Category="Test") int32 GetAnchorRealm()const;
    /** One line for the dock: trial, time left, success chance, the storm's pressure and the bolts so far. */
    UFUNCTION(BlueprintPure,Category="Test") FText GetTribulationStatus()const;
    /** Rehearsal outcome: 0 rolls at random, 1 always fails (to watch the backlash), 2 always succeeds. */
    UFUNCTION(BlueprintCallable,Category="Test") void SetTribulationOutcome(int32 Outcome);
    UFUNCTION(BlueprintPure,Category="Test") int32 GetTribulationOutcome()const{return TribOutcome;}
    /** 七星挡劫 roll for scripts: 0 at random, 1 always broken, 2 always held (when the sword's realm gives any chance). */
    UFUNCTION(BlueprintCallable,Category="Test") void SetStarWardOutcome(int32 Outcome);
    /** The last backlash's 七星挡劫: 0 none, 1 held, 2 broken; and its chance in basis points. */
    UFUNCTION(BlueprintPure,Category="Test") int32 GetStarWard()const;
    UFUNCTION(BlueprintPure,Category="Test") int32 GetStarWardChance()const;

    /**
     * The realm index a test sword fights at: its viewer is held there (growth from hits would otherwise lift it), so
     * the sword's attack, 会心, 会心伤害, 破甲 and the realm suppression against a scarecrow follow this realm. 练气一阶 at start.
     */
    UFUNCTION(BlueprintCallable,Category="Test") void SetSwordRealm(ELiveSpecialSword Kind,int32 Realm);
    UFUNCTION(BlueprintCallable,Category="Test") void SetAllSwordRealms(int32 Realm);
    UFUNCTION(BlueprintPure,Category="Test") int32 GetSwordRealm(ELiveSpecialSword Kind)const;
    /** The realm the five share, or -1 when the 剑谱 set them apart. */
    UFUNCTION(BlueprintPure,Category="Test") int32 GetCommonSwordRealm()const;
    /** Every test sword to the next (+1) or previous (-1) major realm: 练气一阶, 筑基初期, 结晶一阶 … 混元无极. */
    UFUNCTION(BlueprintCallable,Category="Test") void StepSwordRealms(int32 Direction);
    /** Devour growth of a test sword's viewer (噬魂 only grows). */
    UFUNCTION(BlueprintPure,Category="Test") FLiveSpecialSwordGrowth GetSwordGrowth(ELiveSpecialSword Kind)const;
    /** The range's scarecrows, lowest realm first. */
    UFUNCTION(BlueprintPure,Category="Test") TArray<ALiveScarecrow*> GetScarecrows()const;
    /** The scarecrow nearest the cultivator. */
    UFUNCTION(BlueprintPure,Category="Test") ALiveScarecrow* GetNearestScarecrow()const;
    virtual void InitGame(const FString& MapName,const FString& Options,FString& ErrorMessage)override;
    virtual void Tick(float DeltaSeconds)override;
private:
    bool bTestReady=false;
    TWeakObjectPtr<class UCultivationHealthComponent> AnchorHealth;
    FTimerHandle ReviveTimer,RehearsalTimer,RealmTimer;
    int32 RehearsalRealm=INDEX_NONE,AssistHelpers=0,TribOutcome=0;
    // The save's failure streaks and Dao heart before the rehearsal, handed back when it ends.
    TMap<FString,int32> RehearsalFailures;
    double RehearsalDaoHeart=0;
    TMap<ELiveSpecialSword,int32> SwordRealms;
    void EndRehearsal();
    FString ViewerId(ELiveSpecialSword Kind)const;
    /** Puts every present test viewer back on its sword realm (cheap; runs on a timer). */
    void HoldSwordRealms();
    UFUNCTION() void HandleAnchorDied();
    void ReviveAnchor();
};

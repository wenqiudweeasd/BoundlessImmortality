#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "LiveAnchorBuffSubsystem.generated.h"
class UCultivationHealthComponent;

/** One anchor buff or debuff. Effects are data so the HUD, combat and tests read the same entry. */
USTRUCT(BlueprintType)
struct BOUNDLESSIMMORTALITY_API FLiveAnchorBuff
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) FName Id;
    /** Granting sword spirit or system; the same Id from two sources stacks side by side. */
    UPROPERTY(BlueprintReadOnly) FString Source;
    UPROPERTY(BlueprintReadOnly) FText Name;
    UPROPERTY(BlueprintReadOnly) FText Description;
    /** One calligraphy character painted as the icon. */
    UPROPERTY(BlueprintReadOnly) FString Glyph;
    UPROPERTY(BlueprintReadOnly) FLinearColor Color=FLinearColor::White;
    UPROPERTY(BlueprintReadOnly) bool bDebuff=false;
    /** 驱煞 dispels may remove it; a broken Dao heart only heals with time. */
    UPROPERTY(BlueprintReadOnly) bool bDispellable=true;
    UPROPERTY(BlueprintReadOnly) int32 Stacks=1;
    UPROPERTY(BlueprintReadOnly) int32 MaxStacks=1;
    /** Zero means it lasts until its source removes it. */
    UPROPERTY(BlueprintReadOnly) float Duration=0;
    UPROPERTY(BlueprintReadOnly) float Remaining=0;
    UPROPERTY(BlueprintReadOnly) double Shield=0;
    UPROPERTY(BlueprintReadOnly) double ShieldMax=0;
    /** Defense change per stack: +0.05 for one machine-soul layer, negative for armour break. */
    UPROPERTY(BlueprintReadOnly) double DefensePerStack=0;
    UPROPERTY(BlueprintReadOnly) double DamageReduction=0;
    /** Healing per second as a fraction of current health. */
    UPROPERTY(BlueprintReadOnly) double RegenCurrentPerSecond=0;
    /** Damage per second as a fraction of maximum health, bypassing defense. */
    UPROPERTY(BlueprintReadOnly) double DotMaxPerSecond=0;
    /** Extra damage taken (0.5 = +50%), summed over entries: the 每日一签 凶. */
    UPROPERTY(BlueprintReadOnly) double DamageTaken=0;
    double AddedAt=0;
};

DECLARE_MULTICAST_DELEGATE_OneParam(FLiveShieldBroken,const FString&);

UCLASS()
class BOUNDLESSIMMORTALITY_API ULiveAnchorBuffSubsystem : public UTickableWorldSubsystem
{
    GENERATED_BODY()
public:
    static ULiveAnchorBuffSubsystem* Get(const UObject* WorldContext);
    virtual bool DoesSupportWorldType(const EWorldType::Type WorldType)const override{return WorldType==EWorldType::Game||WorldType==EWorldType::PIE;}
    virtual void Tick(float DeltaTime)override;
    virtual TStatId GetStatId()const override;

    /** Adds or refreshes the entry with the same Id and Source. */
    void Apply(const FLiveAnchorBuff& Buff);
    bool Remove(FName Id,const FString& Source);
    void RemoveSource(const FString& Source);
    /** Removes the oldest debuff; false when the anchor is clean. */
    bool DispelOne(FText& OutName);
    /** Monster strike side effect: poison, burn or armour break. */
    void ApplyRandomMonsterDebuff(FRandomStream& Random);
    /** Returns the damage left after shields; broken shields notify their source. Without bStarShields the 七星 light shields are passed over. */
    double AbsorbDamage(double Damage,bool bStarShields=true);
    double GetDefenseMultiplier()const;
    double GetDamageReduction()const;
    /** 1 + the summed DamageTaken of every entry. */
    double GetDamageTakenMultiplier()const;
    void GetShield(double& Current,double& Maximum)const;
    int32 GetStacks(FName Id,const FString& Source=FString())const;
    int32 CountDebuffs()const;
    const TArray<FLiveAnchorBuff>& GetBuffs()const{return Buffs;}
    /** Changes whenever an entry is added, removed or restacked (not on timer ticks). */
    int32 GetRevision()const{return Revision;}
    FLiveShieldBroken OnShieldBroken;

    static FLiveAnchorBuff MakeMechaSoul(const FString& Source,int32 Stacks);
    static FLiveAnchorBuff MakeStarShield(const FString& Source,double Amount,float Duration);
    static FLiveAnchorBuff MakeStarRegen(const FString& Source,float Duration);
    static FLiveAnchorBuff MakeGuard(const FString& Source,double Reduction,float Duration);
    /** 0 poison, 1 burn, 2 armour break. */
    static FLiveAnchorBuff MakeDebuff(int32 Kind);
    /** 道心破碎 after outlasting a tribulation backlash: defense down by DefenseLoss, not dispellable. */
    static FLiveAnchorBuff MakeDaoHeart(double DefenseLoss,float Duration);
    /** 每日一签 for the rest of the local day (source "fortune"): luck, a tribulation omen or extra damage taken; not dispellable. */
    static FLiveAnchorBuff MakeFortune(const FText& Name,const FText& Effect,const FString& Glyph,FLinearColor Color,bool bOmen,double DamageTaken,float Duration);
    /** 仙途 props (source "prop"): a shield of Amount, or an entry that only shows (incense, lantern light). */
    static FLiveAnchorBuff MakePropShield(const FText& Name,double Amount,float Duration);
    static FLiveAnchorBuff MakePropVisual(FName Id,const FText& Name,const FText& Description,const FString& Glyph,FLinearColor Color,float Duration);
private:
    TArray<FLiveAnchorBuff> Buffs;
    int32 Revision=0;
    double Clock=0,DotPending=0;
    float DotTimer=0;
    UCultivationHealthComponent* AnchorHealth()const;
};

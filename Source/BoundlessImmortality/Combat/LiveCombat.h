#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "LiveCombat.generated.h"

/**
 * One hit as the core damage formula (design v1.4 §3) sees it:
 *   伤害 = A²/(A+D_eff) × 招式系数 × (1+增伤) × 会心 × 境界系数
 *   D_eff = max(0, D × (1−无视防御) × (1−min(破甲, 60%)))
 * The defense term and damage-reduction buffs together never remove more than 60% (总减伤上限). True damage skips
 * defense, reduction and realm. 无视防御 is the special-sword passive (弑神·诛神破甲), audited apart from the 破甲 cap.
 */
USTRUCT(BlueprintType)
struct BOUNDLESSIMMORTALITY_API FLiveHit
{
    GENERATED_BODY()
    /** A: the attacker's attack stat. */
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="伤害") float Attack=0;
    /** 招式系数: 1 for a normal strike, skills scale it (光芒剑气 4.88). */
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="伤害") float SkillCoefficient=1;
    /** 增伤, additive: 0.25 is +25%. */
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="伤害") float DamageBonus=0;
    /** 破甲 0..1, capped at 60% by the formula. */
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="伤害") float ArmorPenetration=0;
    /** 无视防御 0..1 from special-sword passives, applied before 破甲. */
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="伤害") float DefenseIgnore=0;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="伤害") float CriticalChance=0;
    /** 会心伤害 multiplier: 2 doubles a critical hit. */
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="伤害") float CriticalDamage=2;
    /** Extra true damage as a share of the formula damage (弑神·真意 5%). */
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="伤害") float TrueDamageRatio=0;
    /** Flat true damage (七星 bolts: 5% of the target's current health on top of their 80% attack). */
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="伤害") float TrueDamage=0;
    /** Attacker realm index for the world-trial suppression; -1 skips it. */
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="伤害") int32 AttackerRealm=-1;
};

USTRUCT(BlueprintType)
struct BOUNDLESSIMMORTALITY_API FLiveHitResult
{
    GENERATED_BODY()
    /** Formula damage plus true damage, before the target's remaining health clamps it. */
    UPROPERTY(BlueprintReadOnly,Category="伤害") float Damage=0;
    UPROPERTY(BlueprintReadOnly,Category="伤害") float TrueDamage=0;
    UPROPERTY(BlueprintReadOnly,Category="伤害") float EffectiveDefense=0;
    /** Share of A the defense term and reduction leave, after the 60% cap (1 = nothing removed). */
    UPROPERTY(BlueprintReadOnly,Category="伤害") float Mitigation=1;
    UPROPERTY(BlueprintReadOnly,Category="伤害") float RealmFactor=1;
    UPROPERTY(BlueprintReadOnly,Category="伤害") bool bCritical=false;
};

namespace LiveCombat
{
    constexpr float MaxArmorPenetration=.6f;
    /** 总减伤上限: defense and damage-reduction buffs together keep at least 40% of a hit. */
    constexpr float MaxDamageReduction=.6f;
    /** 0 练气, 1 筑基, 2 结晶, 3 金丹, 4 元婴, 5 化神, 6 元神, 7 炼虚, 8 大乘, 9 飞升, 10 混元无极. */
    BOUNDLESSIMMORTALITY_API int32 MajorRealm(int32 RealmIndex);
    BOUNDLESSIMMORTALITY_API float EffectiveDefense(float Defense,float ArmorPenetration,float DefenseIgnore);
    /** A/(A+D_eff) × (1−Reduction), never below 1−MaxDamageReduction. */
    BOUNDLESSIMMORTALITY_API float Mitigation(float Attack,float EffectiveDefense,float Reduction=0);
    /**
     * World-trial suppression when a lower realm hits a higher one: same major realm max(0.70, 1−0.03×小阶差),
     * across major realms max(0.25, 0.8^大境界差). 1 when the attacker is not lower or either realm is unknown.
     */
    BOUNDLESSIMMORTALITY_API float RealmFactor(int32 AttackerRealm,int32 DefenderRealm);
    /** Resolves one hit against a defender. Without a random stream the hit never crits. */
    BOUNDLESSIMMORTALITY_API FLiveHitResult Resolve(const FLiveHit& Hit,float Defense,int32 DefenderRealm=-1,float Reduction=0,FRandomStream* Random=nullptr);
}

/** Script access to the core damage formula, for tests and tooling. */
UCLASS()
class BOUNDLESSIMMORTALITY_API ULiveCombatLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    /** Deterministic resolution: the hit crits only when bForceCritical. */
    UFUNCTION(BlueprintPure,Category="战斗|伤害") static FLiveHitResult ResolveHit(const FLiveHit& Hit,float Defense,int32 DefenderRealm,float Reduction,bool bForceCritical);
    UFUNCTION(BlueprintPure,Category="战斗|伤害") static float RealmFactor(int32 AttackerRealm,int32 DefenderRealm);
};

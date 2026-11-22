#pragma once
#include "CoreMinimal.h"
#include "Cultivation/CultivationTypes.h"
#include "LiveSpecialSwordTypes.generated.h"

/** Gift-unlocked sword spirit forms. The equipped title (TitleId) decides the form a viewer's spirit takes. */
UENUM(BlueprintType,meta=(ScriptName="LiveSpecialSwordKind"))
enum class ELiveSpecialSword : uint8 { None, SoulEater, Inorganic, DivineBlade, SevenStar, Godslayer };

USTRUCT(BlueprintType)
struct BOUNDLESSIMMORTALITY_API FLiveSpecialSkillState
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) FName SkillId;
    UPROPERTY(BlueprintReadOnly) FText Name;
    UPROPERTY(BlueprintReadOnly) bool bPassive=false;
    UPROPERTY(BlueprintReadOnly) bool bActive=false;
    UPROPERTY(BlueprintReadOnly) float Cooldown=0;
    UPROPERTY(BlueprintReadOnly) float Remaining=0;
};

/**
 * A sword spirit form's combat sheet: 基础属性 at realm index 0 (练气一阶) and 成长属性 gained per realm index.
 * The normal spirit (None) is the reference: its attack is the viewer's own sword attack, which already carries the
 * realm curve, gifts and methods. A special form multiplies that attack by its own curve over the normal curve.
 */
USTRUCT(BlueprintType)
struct BOUNDLESSIMMORTALITY_API FLiveSwordSpiritStats
{
    GENERATED_BODY()
    /** 基础攻击 (normal spirit 10). */
    UPROPERTY(BlueprintReadOnly,Category="剑灵属性") float Attack=10;
    /** 攻击成长: share of the base attack gained per realm (normal spirit 12%). */
    UPROPERTY(BlueprintReadOnly,Category="剑灵属性") float AttackGrowth=.12f;
    /** 会心率, added to the viewer's own critical chance. */
    UPROPERTY(BlueprintReadOnly,Category="剑灵属性") float CriticalChance=0;
    UPROPERTY(BlueprintReadOnly,Category="剑灵属性") float CriticalGrowth=0;
    /** 会心伤害 multiplier (2 = 200%). */
    UPROPERTY(BlueprintReadOnly,Category="剑灵属性") float CriticalDamage=2;
    UPROPERTY(BlueprintReadOnly,Category="剑灵属性") float CriticalDamageGrowth=0;
    /** 破甲; the damage formula caps the total at 60%. */
    UPROPERTY(BlueprintReadOnly,Category="剑灵属性") float ArmorPenetration=0;
    UPROPERTY(BlueprintReadOnly,Category="剑灵属性") float PenetrationGrowth=0;
    /** 出手间隔 between normal strikes, seconds. */
    UPROPERTY(BlueprintReadOnly,Category="剑灵属性") float AttackInterval=1;
};

/** Per-viewer growth that outlives a session: devoured attributes and machine-soul tempering. */
USTRUCT(BlueprintType)
struct BOUNDLESSIMMORTALITY_API FLiveSpecialSwordGrowth
{
    GENERATED_BODY()
    /** 噬魂: attack taken from devoured enemies (1% of their attack plus 5% of their maximum health), added to the sword's attack. */
    UPROPERTY(EditAnywhere,BlueprintReadWrite) double DevouredAttack=0;
    /** 噬魂: 1% of each devoured enemy's defense and maximum health, kept as the sword's own. */
    UPROPERTY(EditAnywhere,BlueprintReadWrite) double DevouredDefense=0;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) double DevouredHealth=0;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) int32 Devoured=0;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) double MechaDamageBonus=0;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) int32 AxeSlams=0;
};

USTRUCT()
struct BOUNDLESSIMMORTALITY_API FLiveSpecialSwordGrowthFile
{
    GENERATED_BODY()
    UPROPERTY() int32 Version=1;
    UPROPERTY() TMap<FString,FLiveSpecialSwordGrowth> Participants;
};

namespace LiveSpecialSwords
{
    struct FSkillInfo
    {
        FName Id;
        FText Name;
        FText Description;
        float Cooldown=0;
        bool bPassive=false;
    };
    BOUNDLESSIMMORTALITY_API ELiveSpecialSword FromTitle(FName TitleId);
    BOUNDLESSIMMORTALITY_API FName TitleOf(ELiveSpecialSword Kind);
    BOUNDLESSIMMORTALITY_API ECultivationGift GiftOf(ELiveSpecialSword Kind);
    BOUNDLESSIMMORTALITY_API FString GiftName(ELiveSpecialSword Kind);
    BOUNDLESSIMMORTALITY_API FText Name(ELiveSpecialSword Kind);
    BOUNDLESSIMMORTALITY_API FText Motto(ELiveSpecialSword Kind);
    /** Signature HDR tint used by the trail, hits and the UI accent. */
    BOUNDLESSIMMORTALITY_API FLinearColor Color(ELiveSpecialSword Kind);
    BOUNDLESSIMMORTALITY_API const TArray<FSkillInfo>& Skills(ELiveSpecialSword Kind);
    /** The form's attribute sheet; None is the normal sword spirit. */
    BOUNDLESSIMMORTALITY_API const FLiveSwordSpiritStats& Stats(ELiveSpecialSword Kind);
    /** Sheet values at a realm index: attack Attack×(1+AttackGrowth×r); critical, critical damage and 破甲 grow linearly. */
    BOUNDLESSIMMORTALITY_API float AttackAt(ELiveSpecialSword Kind,int32 Realm);
    BOUNDLESSIMMORTALITY_API float CriticalAt(ELiveSpecialSword Kind,int32 Realm);
    BOUNDLESSIMMORTALITY_API float CriticalDamageAt(ELiveSpecialSword Kind,int32 Realm);
    BOUNDLESSIMMORTALITY_API float PenetrationAt(ELiveSpecialSword Kind,int32 Realm);
    /** How much stronger this form strikes than a normal spirit at the same realm: AttackAt(Kind)/AttackAt(None). */
    BOUNDLESSIMMORTALITY_API float AttackScale(ELiveSpecialSword Kind,int32 Realm);
    BOUNDLESSIMMORTALITY_API TArray<ELiveSpecialSword> All();

    /** 噬魂 devour: below DevourHealthBelow of its health a damaged enemy is swallowed with DevourChance. */
    constexpr float DevourHealthBelow=.15f,DevourChance=.3f;
    /** Share of every attribute a devour takes, and the share of the enemy's maximum health turned into attack. */
    constexpr double DevourShare=.01,DevourHealthToAttack=.05;
    /** What one devour gives: Attack already includes HealthAttack (the health turned into attack). */
    struct FDevourGain{double Attack=0,Defense=0,Health=0,HealthAttack=0;};
    BOUNDLESSIMMORTALITY_API FDevourGain DevourGain(double EnemyAttack,double EnemyDefense,double EnemyMaxHealth);

    /**
     * 七星 lightning, the seven bolts and the strikes 光芒剑气 calls down alike: BoltAttack of the sword's attack through the
     * core formula plus BoltHealthShare of the target's current health as true damage.
     */
    constexpr float BoltAttack=.8f,BoltHealthShare=.05f;
    /** 光芒剑气: the sword qi's 招式系数, and the chance one release calls down RadianceStrikes lightning strikes. */
    constexpr float RadianceCoefficient=4.88f,RadianceStrikeChance=.45f;
    constexpr int32 RadianceStrikes=3;
}

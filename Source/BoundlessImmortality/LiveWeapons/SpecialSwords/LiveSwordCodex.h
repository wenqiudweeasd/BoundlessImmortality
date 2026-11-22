#pragma once
#include "CoreMinimal.h"
#include "LiveWeapons/SpecialSwords/LiveSpecialSwordTypes.h"

/** Text for the special-sword codex screens (试剑台 剑谱, 仙途 剑灵谱 特殊剑): attribute lines, roles and realm names. */
namespace LiveSwordCodex
{
    struct FStatLine
    {
        FText Label;
        /** 基础 at 练气一阶, 成长 per realm index ("—" when the attribute does not grow), value at the asked realm. */
        FString Base,Growth,At;
    };
    /** 攻击, 会心率, 会心伤害, 破甲, 出手间隔 of a form's sheet; None gives the normal sword spirit. */
    BOUNDLESSIMMORTALITY_API TArray<FStatLine> StatLines(ELiveSpecialSword Kind,int32 Realm);
    /** One line on the form's role and why its sheet sits where it does among the five. */
    BOUNDLESSIMMORTALITY_API FText Role(ELiveSpecialSword Kind);
    /** Compact one-line sheets for cards: "攻击 12 · 会心 3.0% …" and "每境界 攻击 +13% · 会心 +0.10% …". */
    BOUNDLESSIMMORTALITY_API FText BaseSummary(ELiveSpecialSword Kind);
    BOUNDLESSIMMORTALITY_API FText GrowthSummary(ELiveSpecialSword Kind);
    /** First realm index of each major realm: 练气一阶, 筑基初期 … 混元无极. */
    BOUNDLESSIMMORTALITY_API const TArray<int32>& MajorStops();
    /** The default realm names (练气一阶 … 混元无极). */
    BOUNDLESSIMMORTALITY_API FText RealmName(int32 Realm);
    /** Damage and attack amounts: 9876, 1.2万, 3456万, 1.23亿. */
    BOUNDLESSIMMORTALITY_API FString Amount(double Value);
}

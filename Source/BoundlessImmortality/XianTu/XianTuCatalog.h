#pragma once
#include "CoreMinimal.h"

/**
 * Item economy of the 仙途 ledger (design v1.4 §15 tables/items-v1.4, plus the 2026-11-01 currency and shop request):
 * four currencies beside 灵石, the 50 design items (ten breakthrough stages × core / 避凶吉兆 / 滋润灵物 / 天材地宝, and
 * ten crafted props), quality grades and bands, the three shops, the 仙道殿 cosmetics and the daily fortune draw.
 * Breakthrough materials exist in four grades; the ledger key of one stack is "<species>_G<grade>".
 */
namespace XianTuCatalog
{
    /** 灵玉 buys in the 灵玉商城 (仙途), 武灵 in the 武灵商城 (山门), 金元 in the 仙道殿; 元魂 is earned by breakthroughs and kept for later use. */
    enum class ECurrency : uint8 { Jade, Wuling, Soul, Gold, Count };
    struct FCurrency{ECurrency Id=ECurrency::Jade;FName Key;FText Name;FName Icon;FText Source;FText Use;FLinearColor Tint=FLinearColor::White;};
    BOUNDLESSIMMORTALITY_API const TArray<FCurrency>& Currencies();
    BOUNDLESSIMMORTALITY_API const FCurrency& Currency(ECurrency Id);
    /** CUR_JADE / CUR_WULING / CUR_SOUL / CUR_GOLD, or None. */
    BOUNDLESSIMMORTALITY_API bool ParseCurrency(FName Key,ECurrency& Out);

    /** 核心材料 (破境资格), 避凶吉兆 (护道), 滋润灵物 (精华), 天材地宝 (奇珍). */
    enum class ECategory : uint8 { Core, Guard, Essence, Treasure, Count };
    BOUNDLESSIMMORTALITY_API FText CategoryName(ECategory Category);
    BOUNDLESSIMMORTALITY_API FText CategoryRole(ECategory Category);
    BOUNDLESSIMMORTALITY_API FText GradeName(int32 Grade);
    /** quality_grades.csv: score of one item of Grade (1..4) in its category. */
    BOUNDLESSIMMORTALITY_API int32 GradeScore(ECategory Category,int32 Grade);
    /** Category caps of the quality score: 35 / 15 / 30 / 20. */
    BOUNDLESSIMMORTALITY_API int32 CategoryCap(ECategory Category);

    struct FSpecies{FName Id;FText Name;FName Icon;FText Theme;};
    struct FStage
    {
        FName Id;
        /** 0..9 for BR01..BR10. */
        int32 Index=0;
        int32 From=0,To=0,MinMajor=0;
        FText Region,Subarea;
        FSpecies Species[int32(ECategory::Count)];
        /** Slots of each category on the breakthrough screen; the core is always one. */
        int32 Caps[int32(ECategory::Count)]={1,0,0,0};
        bool bTribulation=false;
    };
    BOUNDLESSIMMORTALITY_API const TArray<FStage>& Stages();
    /** The major stage that breaks out of FromRealm; nullptr for a minor (小境界) step. */
    BOUNDLESSIMMORTALITY_API const FStage* StageFrom(int32 FromRealm);
    BOUNDLESSIMMORTALITY_API const FStage* Stage(FName Id);
    /** Ledger key of one graded stack, e.g. MAT_BR01_C01_G2. */
    BOUNDLESSIMMORTALITY_API FName MaterialKey(const FStage& Stage,ECategory Category,int32 Grade);
    BOUNDLESSIMMORTALITY_API bool ParseMaterial(FName Key,const FStage*& OutStage,ECategory& OutCategory,int32& OutGrade);
    /** recipes.csv: 灵石 to refine two of Grade-1 into one of Grade (10 × stage number × grade). */
    BOUNDLESSIMMORTALITY_API int32 RefineCost(const FStage& Stage,int32 Grade);

    /** quality_bands.csv. Factor is the live snapshot factor 1.00..1.10 of a breakthrough at that band. */
    struct FBand{FText Name;int32 Min=0,Max=0;double Factor=1;FLinearColor Color=FLinearColor::White;};
    BOUNDLESSIMMORTALITY_API const TArray<FBand>& Bands();
    BOUNDLESSIMMORTALITY_API int32 BandIndex(int32 Quality);
    /**
     * Q = core score + floor(Σguard / guard cap) + floor(Σessence / essence cap) + treasure score; each category
     * clamped to its cap, the total to 100. Grades are 1..4; zeros are empty slots.
     */
    BOUNDLESSIMMORTALITY_API int32 QualityScore(const FStage& Stage,int32 CoreGrade,const TArray<int32>& Guard,const TArray<int32>& Essence,const TArray<int32>& Treasure);
    /** 1 + min(0.10, Σ((band factor − 1) / 10)) over the best snapshot of each stage: attack, health and defense. */
    BOUNDLESSIMMORTALITY_API double QualityFactor(const TArray<int32>& BestBandPerStage);

    struct FProp{FName Id;FText Name;int32 MinMajor=0;FName Icon;int32 Quality=1;FText Effect;FName EffectKey;float Magnitude=0;float Duration=0;float Cooldown=0;};
    /** prop_items.csv, the ten props of this phase. */
    BOUNDLESSIMMORTALITY_API const TArray<FProp>& Props();
    BOUNDLESSIMMORTALITY_API const FProp* Prop(FName Id);

    struct FGuildMaterial{FName Id;FText Name;FName Icon;};
    /** guild_build_materials.csv: owned by the 武灵's vault, never personal. */
    BOUNDLESSIMMORTALITY_API const TArray<FGuildMaterial>& GuildMaterials();

    /** One build level of a guild_buildings.csv building. */
    struct FBuildLevel{int32 Supply=0;FName MatA,MatB;int32 MatCount=0;FText Visual;};
    struct FGuildBuilding{FName Id;FText Name;FText Theme;int32 Scene=0;FBuildLevel Levels[3];};
    BOUNDLESSIMMORTALITY_API const TArray<FGuildBuilding>& GuildBuildings();

    /** equipment_items.csv (four non-weapon slots) and fabao_items.csv (one 法宝 slot). Slot 4 is the 法宝. */
    struct FGear{FName Id;FText Name;int32 Slot=0;int32 Tier=1;int32 MinMajor=0;FText Stat;float Value=0;float Cooldown=0;FText Effect;FName Icon;};
    BOUNDLESSIMMORTALITY_API const TArray<FGear>& Gear();
    BOUNDLESSIMMORTALITY_API const FGear* GearItem(FName Id);
    BOUNDLESSIMMORTALITY_API FText GearSlotName(int32 Slot);
    constexpr int32 GearSlots=5;

    enum class EShop : uint8 { Jade, Wuling, Gold };
    struct FSku
    {
        FName Id;
        EShop Shop=EShop::Jade;
        /** Tab inside the shop: 0 突破, 1 辅材/道具, 2 杂货 (meaning per shop). */
        int32 Tab=0;
        FName Item;
        int32 Count=1;
        ECurrency Currency=ECurrency::Jade;
        int32 Price=0;
        /** Per local day; 0 is unlimited. */
        int32 DailyLimit=0;
        int32 MinMajor=0;
        /** "hot" / "new" / "limited" ribbons on the card. */
        FName Tag;
    };
    BOUNDLESSIMMORTALITY_API const TArray<FSku>& Skus();
    BOUNDLESSIMMORTALITY_API const FSku* Sku(FName Id);

    /** 仙道殿 looks: no stat ever changes. */
    enum class ECosmetic : uint8 { Sword, Frame, Title, Aura, Count };
    struct FCosmetic{FName Id;ECosmetic Kind=ECosmetic::Sword;FText Name;FText Description;int32 Price=0;int32 Quality=3;FLinearColor Primary=FLinearColor::White,Secondary=FLinearColor::White;bool bLimited=false;};
    BOUNDLESSIMMORTALITY_API const TArray<FCosmetic>& Cosmetics();
    BOUNDLESSIMMORTALITY_API const FCosmetic* Cosmetic(FName Id);
    BOUNDLESSIMMORTALITY_API FText CosmeticKindName(ECosmetic Kind);

    /** 每日一签: one free draw per local day (Asia/Hong_Kong midnight); no paid redraw. */
    enum class EFortune : uint8 { None, Great, Good, Poor, Calamity, Ominous, Count };
    struct FFortune
    {
        EFortune Id=EFortune::None;
        FText Name;
        /** Four-character verse on the stick and the two-line 签文 read out with it. */
        FText Verse;
        FText Poem;
        FText Effect;
        /** Draw weight out of 100. */
        int32 Weight=0;
        /** Luck bonus (0.5 = +50%). */
        float Luck=0;
        /** Basis points taken off the tribulation success chance. */
        int32 TribulationPenalty=0;
        /** Extra damage taken (0.5 = +50%). */
        float DamageTaken=0;
        FLinearColor Color=FLinearColor::White;
        bool bOmen=false;
    };
    BOUNDLESSIMMORTALITY_API const TArray<FFortune>& Fortunes();
    BOUNDLESSIMMORTALITY_API const FFortune& Fortune(EFortune Id);
    /** Maps a roll in [0,100) to a fortune by weight. */
    BOUNDLESSIMMORTALITY_API EFortune FortuneForRoll(int32 Roll);
    /** World-trial 机缘: chance per completed stage of one extra breakthrough material, raised by luck. */
    constexpr float FortuneChanceBase=.2f;
}

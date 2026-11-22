#pragma once
#include "CoreMinimal.h"
#include "WanJian/WanJianTypes.h"

/** Design tables of GDD v0.2. All numbers are its 初始建议值 and meant to be tuned. */
namespace WanJianCatalog
{
    namespace Asset
    {
        inline constexpr const TCHAR* Qi=TEXT("Qi");
        inline constexpr const TCHAR* Stone=TEXT("Stone");
        inline constexpr const TCHAR* Ticket=TEXT("SwordTicket");
        inline constexpr const TCHAR* Shard=TEXT("SwordShard");
        inline constexpr const TCHAR* Herb=TEXT("Herb");
        inline constexpr const TCHAR* Ore=TEXT("Ore");
        inline constexpr const TCHAR* BreakStone=TEXT("BreakStone");
        inline constexpr const TCHAR* Crystal=TEXT("SwordCrystal");
        inline constexpr const TCHAR* Equipment=TEXT("Equipment");
        inline constexpr const TCHAR* Artifact=TEXT("Artifact");
        inline constexpr const TCHAR* Consumable=TEXT("Consumable");
    }

    enum class EAttack : uint8 { Homing, Pierce, Boomerang, Area, Chain, Support };
    enum class EProc : uint8 { None, Burn, Slow, ArmorBreak, Knockback, Heal, ElementFocus, StarGuard };

    struct FSword
    {
        FName Id;
        const TCHAR* Name=TEXT("");
        const TCHAR* Role=TEXT("");
        EWanJianElement Element=EWanJianElement::None;
        EWanJianRarity Rarity=EWanJianRarity::Common;
        EAttack Attack=EAttack::Homing;
        EProc Proc=EProc::None;
        int32 CooldownMs=1000;
        double Coefficient=1;
        int32 MaxInLineup=3;
        bool bInPool=true;
    };

    struct FWorld { int32 Id=0; const TCHAR* Name=TEXT(""); const TCHAR* Theme=TEXT(""); bool bOpen=false; };

    struct FMethod
    {
        FName Id;
        const TCHAR* Name=TEXT("");
        const TCHAR* Effect=TEXT("");
        /** Critical points needed before the standing cast; 0 is active from the start of the battle. */
        int32 Critical=0;
        int32 Fragments=20;
        bool bFirstRelease=false;
    };

    struct FDungeon
    {
        FName Id;
        const TCHAR* Name=TEXT("");
        const TCHAR* Focus=TEXT("");
        /** Drop chance of one player alone; a team multiplies it (GDD 17.3). */
        double BaseChance=0;
        /** Clears that pay one fixed material, so nobody is stuck on bad rolls. */
        int32 FixedEvery=10;
        bool bOpen=false;
    };

    struct FBuilding { FName Id; const TCHAR* Name=TEXT(""); const TCHAR* Effect=TEXT(""); };

    struct FSynergy { EWanJianElement A=EWanJianElement::None; EWanJianElement B=EWanJianElement::None; const TCHAR* Name=TEXT(""); const TCHAR* Effect=TEXT(""); };

    BOUNDLESSIMMORTALITY_API const TArray<FSword>& Swords();
    BOUNDLESSIMMORTALITY_API const FSword* Sword(FName Id);
    BOUNDLESSIMMORTALITY_API FName StarterSword();
    BOUNDLESSIMMORTALITY_API FName SevenStarSword();
    BOUNDLESSIMMORTALITY_API const TArray<FWorld>& Worlds();
    BOUNDLESSIMMORTALITY_API const TArray<FMethod>& Methods();
    BOUNDLESSIMMORTALITY_API const FMethod* Method(FName Id);
    BOUNDLESSIMMORTALITY_API FName MethodFragmentAsset(FName Method);
    BOUNDLESSIMMORTALITY_API const TArray<FDungeon>& Dungeons();
    BOUNDLESSIMMORTALITY_API const FDungeon* Dungeon(FName Id);
    BOUNDLESSIMMORTALITY_API const TArray<FBuilding>& Buildings();
    BOUNDLESSIMMORTALITY_API const TArray<FSynergy>& Synergies();
    BOUNDLESSIMMORTALITY_API const TCHAR* ElementName(EWanJianElement Element);
    /** Shards returned when an extra sword is melted. */
    BOUNDLESSIMMORTALITY_API int32 MeltShards(EWanJianRarity Rarity);
}

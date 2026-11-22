#pragma once
#include "CoreMinimal.h"
#include "WanJianTypes.generated.h"

/**
 * 万剑仙宗: the single-player WeChat branch (GDD v0.2). It keeps its own account under Saved/WanJian and shares nothing
 * with the live-stream 仙途 and 武灵 ledgers. Everything here is local verification data: a public release needs the
 * server authority of GDD 10.4 before gacha, rewarded-ad or first-clear tickets may be enabled.
 */

UENUM() enum class EWanJianElement : uint8 { None, Metal, Wood, Water, Fire, Earth };
UENUM() enum class EWanJianRarity : uint8 { Common, Fine, Rare, Legendary };

USTRUCT() struct BOUNDLESSIMMORTALITY_API FWanJianLedgerEntry
{
    GENERATED_BODY()
    UPROPERTY() double Utc=0;
    UPROPERTY() FName Asset;
    UPROPERTY() int64 Delta=0;
    UPROPERTY() int64 Balance=0;
    UPROPERTY() FString Source;
    UPROPERTY() FString Request;
};

USTRUCT() struct BOUNDLESSIMMORTALITY_API FWanJianSwordInstance
{
    GENERATED_BODY()
    UPROPERTY() int32 Instance=0;
    UPROPERTY() FName Sword;
    UPROPERTY() int32 Level=1;
};

/** The 500-draw first-acquisition guarantee: account-wide, never reset by days, pools or disconnects. */
USTRUCT() struct BOUNDLESSIMMORTALITY_API FWanJianPity
{
    GENERATED_BODY()
    /** Draws counted toward the first 七星光芒剑; stops once it is owned. */
    UPROPERTY() int32 Draws=0;
    UPROPERTY() int32 TotalDraws=0;
    UPROPERTY() bool bSevenStarOwned=false;
    UPROPERTY() int32 AcquiredAtDraw=0;
};

USTRUCT() struct BOUNDLESSIMMORTALITY_API FWanJianDrawResult
{
    GENERATED_BODY()
    UPROPERTY() FName Sword;
    UPROPERTY() int32 Instance=0;
    UPROPERTY() bool bGuaranteed=false;
    UPROPERTY() int32 PityAfter=0;
};

USTRUCT() struct BOUNDLESSIMMORTALITY_API FWanJianDrawReceipt
{
    GENERATED_BODY()
    UPROPERTY() FString Request;
    UPROPERTY() double Utc=0;
    UPROPERTY() int32 PoolVersion=0;
    UPROPERTY() TArray<FWanJianDrawResult> Results;
};

USTRUCT() struct BOUNDLESSIMMORTALITY_API FWanJianBuilding
{
    GENERATED_BODY()
    UPROPERTY() FName Id;
    UPROPERTY() int32 Level=1;
    UPROPERTY() double LastClaimUtc=0;
};

USTRUCT() struct BOUNDLESSIMMORTALITY_API FWanJianForgeJob
{
    GENERATED_BODY()
    UPROPERTY() bool bActive=false;
    UPROPERTY() double ReadyUtc=0;
};

/** Counters that roll over at Asia/Hong_Kong midnight (day) and on Mondays (week). */
USTRUCT() struct BOUNDLESSIMMORTALITY_API FWanJianPeriod
{
    GENERATED_BODY()
    UPROPERTY() FString Day;
    UPROPERTY() FString Week;
    UPROPERTY() int32 WinsToday=0;
    UPROPERTY() int32 WinsThisWeek=0;
    UPROPERTY() bool bDailyTasksClaimed=false;
    UPROPERTY() bool bSecretClaimed=false;
    UPROPERTY() bool bWeeklyClaimed=false;
    UPROPERTY() bool bAdClaimed=false;
    UPROPERTY() bool bRootChanged=false;
    /** Successful dungeon clears today that still earned the team bonus. */
    UPROPERTY() int32 DungeonBonusUsed=0;
};

USTRUCT() struct BOUNDLESSIMMORTALITY_API FWanJianSave
{
    GENERATED_BODY()
    UPROPERTY() int32 Schema=1;
    /** expected_version of GDD 5.3: a write that names an older version is rejected. */
    UPROPERTY() int64 Version=0;
    UPROPERTY() int32 Seed=0;
    UPROPERTY() double CreatedUtc=0;
    UPROPERTY() bool bTutorialRewarded=false;
    UPROPERTY() EWanJianElement Root=EWanJianElement::None;
    UPROPERTY() bool bFreeRootChangeUsed=false;
    UPROPERTY() int32 Realm=0;
    UPROPERTY() TMap<FName,int64> Wallet;
    UPROPERTY() TArray<FWanJianLedgerEntry> Ledger;
    UPROPERTY() TArray<FWanJianSwordInstance> Swords;
    UPROPERTY() int32 NextInstance=1;
    /** Instance per slot, 0 for an empty slot; always GDD's 18 entries. */
    UPROPERTY() TArray<int32> Lineup;
    UPROPERTY() FWanJianPity Pity;
    UPROPERTY() TArray<FWanJianDrawReceipt> Receipts;
    /** Request ids of every non-draw claim already applied. */
    UPROPERTY() TArray<FString> Requests;
    UPROPERTY() TArray<int32> ClearedStages;
    UPROPERTY() FWanJianPeriod Period;
    UPROPERTY() TArray<FWanJianBuilding> Buildings;
    UPROPERTY() FWanJianForgeJob Forge;
    UPROPERTY() TArray<FName> LearnedMethods;
    UPROPERTY() TArray<FName> EquippedMethods;
    /** Research level per element name. */
    UPROPERTY() TMap<FName,int32> Research;
    /** Clears toward the next fixed material of each dungeon. */
    UPROPERTY() TMap<FName,int32> DungeonProgress;
};

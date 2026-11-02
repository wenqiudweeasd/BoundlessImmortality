#pragma once
#include "CoreMinimal.h"
#include "XianTuTypes.generated.h"

/**
 * 仙途 local ledger: world trials, spirit-vein contests, leases, the 武灵 guild, methods, tribulation and the
 * market, persisted as one JSON document under Saved/XianTu. Everything here is local verification data;
 * no record is a platform receipt.
 */

UENUM() enum class EXianTuWorldState : uint8 { Idle, Preparing, Running, Settling, Completed, Interrupted };
UENUM() enum class EXianTuRaceState : uint8 { None, Queue, Matched, Ready, Running, Frozen, Settled, Cancelled };
UENUM() enum class EXianTuLeaseState : uint8 { Pending, Active, Expired, Revoked };
/** Backlash (天罚) follows a failed roll: ten seconds of lightning that settles the failure as a fall or a broken Dao heart. */
UENUM() enum class EXianTuTribState : uint8 { None, Prepare, Trial, Frozen, Succeeded, Failed, Cancelled, Backlash };
/** 七星挡劫 in a backlash: no 七星光芒剑 has stood against it yet, its guard held the heavens off, or the lightning broke it. */
UENUM() enum class EXianTuWard : uint8 { None, Held, Broken };
UENUM() enum class EXianTuOrderState : uint8 { Listed, Sold, Cancelled, Expired };

USTRUCT() struct BOUNDLESSIMMORTALITY_API FXianTuReward
{
    GENERATED_BODY()
    UPROPERTY() FName Item;
    UPROPERTY() int32 Count=0;
};

USTRUCT() struct BOUNDLESSIMMORTALITY_API FXianTuRecord
{
    GENERATED_BODY()
    UPROPERTY() double Utc=0;
    UPROPERTY() FString Kind;
    UPROPERTY() FString Text;
};

USTRUCT() struct BOUNDLESSIMMORTALITY_API FXianTuMember
{
    GENERATED_BODY()
    UPROPERTY() FString Id;
    UPROPERTY() FString Name;
    UPROPERTY() bool bFormal=true;
    UPROPERTY() double JoinedUtc=0;
    UPROPERTY() double LeftUtc=0;
    /** Cannot formally join any guild before this UTC (24 h after leaving or removal). */
    UPROPERTY() double CooldownUntil=0;
    UPROPERTY() int32 DailyMaterials=0;
    UPROPERTY() int32 DailySelected=0;
    UPROPERTY() FString DailyKey;
    UPROPERTY() int32 Contribution=0;
    UPROPERTY(Transient) bool bPresent=false;
};

USTRUCT() struct BOUNDLESSIMMORTALITY_API FXianTuGuild
{
    GENERATED_BODY()
    UPROPERTY() bool bCreated=false;
    UPROPERTY() FString Id;
    UPROPERTY() FString Name;
    /** Name waiting for (local) review; the approved name stays in use meanwhile. */
    UPROPERTY() FString PendingName;
    UPROPERTY() double PendingSince=0;
    UPROPERTY() FString RejectReason;
    UPROPERTY() double CreatedUtc=0;
    UPROPERTY() double LastRenameUtc=0;
    UPROPERTY() int32 Materials=0;
    UPROPERTY() int32 Reputation=0;
    UPROPERTY() int32 DailyMaterials=0;
    UPROPERTY() int32 DailyReputation=0;
    UPROPERTY() FString DailyKey;
    UPROPERTY() TArray<FName> Buildings;
    /** 武灵建设 levels 0..3 per guild_buildings.csv building, and the vault of construction materials. */
    UPROPERTY() TMap<FName,int32> BuildLevels;
    UPROPERTY() TMap<FName,int32> Vault;
    UPROPERTY() TArray<FXianTuMember> Members;
    UPROPERTY() TArray<FXianTuRecord> Records;
};

USTRUCT() struct BOUNDLESSIMMORTALITY_API FXianTuWorld
{
    GENERATED_BODY()
    UPROPERTY() EXianTuWorldState State=EXianTuWorldState::Idle;
    UPROPERTY() FName Region=TEXT("qingyun");
    UPROPERTY() FName Preset=TEXT("ENC_SOLO");
    UPROPERTY() FName Route=TEXT("guard");
    /** Seconds since the round began (0..600 on the design clock). */
    UPROPERTY() double Elapsed=0;
    /** 0 before the first stage, 1..3 while running. */
    UPROPERTY() int32 Stage=0;
    UPROPERTY() TArray<int32> StageResults;
    UPROPERTY() int32 Wave=0;
    UPROPERTY() int32 Progress=0;
    UPROPERTY() int32 Target=0;
    UPROPERTY() double RecoverRemaining=0;
    UPROPERTY() int32 RoundsToday=0;
    UPROPERTY() FString DayKey;
    UPROPERTY() int32 CompletedTotal=0;
    UPROPERTY() TArray<FXianTuReward> LastRewards;
    UPROPERTY() FString LastReceipt;
    UPROPERTY() double LastQi=0;
    UPROPERTY() int32 LastGuildMaterials=0;
    UPROPERTY() int32 LastReputation=0;
};

USTRUCT() struct BOUNDLESSIMMORTALITY_API FXianTuRaceTeam
{
    GENERATED_BODY()
    UPROPERTY() FString Name;
    UPROPERTY() bool bSelf=false;
    UPROPERTY() int32 Members=0;
    UPROPERTY() int32 TaskScore=0;
    UPROPERTY() int32 AnchorScore=0;
    UPROPERTY() int32 MemberScore=0;
    UPROPERTY() int32 Total=0;
    UPROPERTY() int32 Rank=0;
    UPROPERTY() int32 LeaseMinutes=0;
};

USTRUCT() struct BOUNDLESSIMMORTALITY_API FXianTuRace
{
    GENERATED_BODY()
    UPROPERTY() EXianTuRaceState State=EXianTuRaceState::None;
    UPROPERTY() FName Node;
    UPROPERTY() double StateElapsed=0;
    UPROPERTY() int32 Tier=0;
    UPROPERTY() double MatchAfter=4;
    UPROPERTY() TArray<FXianTuRaceTeam> Teams;
    UPROPERTY() TArray<FString> Representatives;
    UPROPERTY() TArray<FString> RepresentativeNames;
    /** Valid responses per representative (0..2). */
    UPROPERTY() TMap<FString,int32> Responses;
    UPROPERTY() TArray<int32> TaskResults;
    UPROPERTY() int32 Progress=0;
    UPROPERTY() int32 Target=0;
    UPROPERTY() TArray<int32> OpResults;
    UPROPERTY() FString Receipt;
    UPROPERTY() int32 Seed=0;
};

USTRUCT() struct BOUNDLESSIMMORTALITY_API FXianTuLease
{
    GENERATED_BODY()
    UPROPERTY() FString Id;
    UPROPERTY() FName Node;
    UPROPERTY() int32 Minutes=0;
    UPROPERTY() double Start=0;
    UPROPERTY() double End=0;
    UPROPERTY() EXianTuLeaseState State=EXianTuLeaseState::Pending;
    UPROPERTY() double OnlineSeconds=0;
    UPROPERTY() int32 ItemsGranted=0;
    UPROPERTY() double QiGranted=0;
    UPROPERTY() int32 Rank=0;
    UPROPERTY() int32 Score=0;
    UPROPERTY() FString Receipt;
};

USTRUCT() struct BOUNDLESSIMMORTALITY_API FXianTuMethods
{
    GENERATED_BODY()
    UPROPERTY() FName Main;
    UPROPERTY() TArray<FName> Support;
    UPROPERTY() TMap<FName,int32> Levels;
    /** Study progress toward the next level (scrolls invested). */
    UPROPERTY() TMap<FName,int32> Progress;
};

USTRUCT() struct BOUNDLESSIMMORTALITY_API FXianTuTribulation
{
    GENERATED_BODY()
    UPROPERTY() EXianTuTribState State=EXianTuTribState::None;
    UPROPERTY() double Elapsed=0;
    UPROPERTY() int32 FromRealm=0;
    UPROPERTY() FString AttemptId;
    UPROPERTY() TArray<FString> Assists;
    UPROPERTY() TArray<FString> AssistNames;
    /** Frozen success probability in basis points (5000 = 50%). */
    UPROPERTY() int32 Probability=5000;
    UPROPERTY() int32 Roll=-1;
    UPROPERTY() bool bSuccess=false;
    UPROPERTY() double Loss=0;
    /** The anchor fell during the backlash: the failure costs cultivation. */
    UPROPERTY() bool bDied=false;
    /** The anchor outlasted the backlash: cultivation is kept, the Dao heart breaks. */
    UPROPERTY() bool bDaoHeartBroken=false;
    /** Backlash bolts that struck and the health they removed. */
    UPROPERTY() int32 BacklashStrikes=0;
    UPROPERTY() double BacklashDamage=0;
    /** 七星挡劫: the backlash's one roll for the strongest 七星光芒剑 on the field (its realm, chance and roll in basis points). */
    UPROPERTY() EXianTuWard Ward=EXianTuWard::None;
    UPROPERTY() int32 WardRealm=-1;
    UPROPERTY() int32 WardChance=0;
    UPROPERTY() int32 WardRoll=-1;
    /** Sim time the broken Dao heart heals: until then no tribulation and the anchor's defense is down. Survives new attempts. */
    UPROPERTY() double DaoHeartUntil=0;
    UPROPERTY() TMap<FString,int32> Failures;
    UPROPERTY() double LastSettled=0;
    UPROPERTY() double RevealRemaining=0;
    UPROPERTY() FString Receipt;
};

USTRUCT() struct BOUNDLESSIMMORTALITY_API FXianTuOrder
{
    GENERATED_BODY()
    UPROPERTY() FString Id;
    UPROPERTY() FName Item;
    UPROPERTY() int32 Count=0;
    UPROPERTY() int32 UnitPrice=0;
    UPROPERTY() double Listed=0;
    UPROPERTY() double Expires=0;
    UPROPERTY() EXianTuOrderState State=EXianTuOrderState::Listed;
    UPROPERTY() bool bMine=false;
    UPROPERTY() FString Seller;
    UPROPERTY() int32 Income=0;
    UPROPERTY() int32 Tax=0;
};

/** The four currencies beside 灵石. Seeds are local verification amounts; a live service replaces the fill, not the screens. */
USTRUCT() struct BOUNDLESSIMMORTALITY_API FXianTuWallet
{
    GENERATED_BODY()
    UPROPERTY() int64 Jade=680;
    UPROPERTY() int64 Wuling=120;
    UPROPERTY() int64 Soul=0;
    UPROPERTY() int64 Gold=1288;
};

/** 每日一签 of one local day. */
USTRUCT() struct BOUNDLESSIMMORTALITY_API FXianTuFortune
{
    GENERATED_BODY()
    UPROPERTY() FString DayKey;
    /** XianTuCatalog::EFortune. */
    UPROPERTY() uint8 Kind=0;
    UPROPERTY() int32 Roll=-1;
    UPROPERTY() double DrawnUtc=0;
    UPROPERTY() int32 Draws=0;
    /** Latest first, e.g. "2026-11-01 上上签". */
    UPROPERTY() TArray<FString> History;
};

/** Materials laid out on the breakthrough screen for one major stage: grades per slot, 0 for an empty slot. */
USTRUCT() struct BOUNDLESSIMMORTALITY_API FXianTuBreakPlan
{
    GENERATED_BODY()
    UPROPERTY() FName Stage;
    UPROPERTY() int32 Core=0;
    UPROPERTY() TArray<int32> Guard;
    UPROPERTY() TArray<int32> Essence;
    UPROPERTY() TArray<int32> Treasure;
};

/** Best quality snapshot of one major stage (design: the highest Q of each stage counts once). */
USTRUCT() struct BOUNDLESSIMMORTALITY_API FXianTuBreakRecord
{
    GENERATED_BODY()
    UPROPERTY() FName Stage;
    UPROPERTY() int32 Quality=0;
    UPROPERTY() int32 Band=0;
    UPROPERTY() int32 FromRealm=0;
    UPROPERTY() double Utc=0;
    UPROPERTY() FString Receipt;
    /** 回影玉简 markers set on this record. */
    UPROPERTY() int32 Markers=0;
};

USTRUCT() struct BOUNDLESSIMMORTALITY_API FXianTuSave
{
    GENERATED_BODY()
    UPROPERTY() int32 Version=1;
    /** Simulated clock (seconds). Advances with real time × TimeScale, also across sessions. */
    UPROPERTY() double SimNow=0;
    UPROPERTY() double SavedUtc=0;
    UPROPERTY() float TimeScale=1.f;
    UPROPERTY() int64 Stones=300;
    UPROPERTY() TMap<FName,int32> Items;
    UPROPERTY() FXianTuGuild Guild;
    UPROPERTY() FXianTuWorld World;
    UPROPERTY() FXianTuRace Race;
    UPROPERTY() TArray<FXianTuLease> Leases;
    UPROPERTY() FXianTuMethods Methods;
    UPROPERTY() FXianTuTribulation Trib;
    UPROPERTY() TArray<FXianTuOrder> Orders;
    UPROPERTY() double NextMarketTick=0;
    UPROPERTY() TArray<FXianTuRecord> Receipts;
    UPROPERTY() int32 Sequence=0;
    UPROPERTY() FXianTuWallet Wallet;
    /** Shop purchases counted against daily limits; reset when ShopDay is not today. */
    UPROPERTY() FString ShopDay;
    UPROPERTY() TMap<FName,int32> ShopBought;
    UPROPERTY() FXianTuFortune Fortune;
    UPROPERTY() FXianTuBreakPlan Plan;
    UPROPERTY() TArray<FXianTuBreakRecord> BreakRecords;
    /** 仙道殿 looks owned, and the one worn per kind (key: Sword / Frame / Title / Aura). */
    UPROPERTY() TArray<FName> Cosmetics;
    UPROPERTY() TMap<FName,FName> Wearing;
    /** 装备法宝 shell: slot index ("0".."4") to the item tried on locally. */
    UPROPERTY() TMap<FName,FName> Gear;
    /** 问道灯 / 无极香 lantern light over breakthroughs until this sim time. */
    UPROPERTY() double CeremonyUntil=0;
};

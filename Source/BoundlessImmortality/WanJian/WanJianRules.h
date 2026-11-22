#pragma once
#include "CoreMinimal.h"
#include "WanJian/WanJianTypes.h"

/** Pure rules of GDD v0.2, free of game state so that deterministic tests and a future server can share them. */
namespace WanJianRules
{
    inline constexpr int32 WorldCount=8;
    inline constexpr int32 OpenWorldCount=2;
    inline constexpr int32 StagesPerWorld=120;
    inline constexpr int32 StagesPerChapter=30;
    inline constexpr int32 TutorialStages=10;
    inline constexpr int32 BaseTemplates=16;
    inline constexpr int32 MaxLineup=18;
    inline constexpr int32 MaxTeam=4;
    inline constexpr int32 PityDraws=500;
    inline constexpr int32 MaxEquippedMethods=3;
    inline constexpr int32 DailyTeamBonus=3;
    inline constexpr int32 MaxSwordLevel=40;
    inline constexpr int32 PoolVersion=1;
    inline constexpr double MaxOfflineSeconds=8.0*3600.0;
    /** One critical point is 120 units, so a kill divides exactly among 1 to 4 players with no rounding drift. */
    inline constexpr int32 CriticalUnitsPerPoint=120;
    inline constexpr int32 CriticalCapUnits=100*CriticalUnitsPerPoint;

    enum class EKill : uint8 { Normal, Elite, Boss };

    struct FStage
    {
        int32 Id=0;
        int32 World=0;
        int32 Index=0;
        /** 1-based position across all worlds, the difficulty axis. */
        int32 Ordinal=0;
        int32 Chapter=0;
        int32 Template=0;
        bool bOpen=false;
        bool bTutorial=false;
        bool bBoss=false;
        bool bWorldBoss=false;
        int32 Duration=60;
        int32 Normals=0;
        int32 Elites=0;
        double NormalHealth=1;
        double Defense=0;
        double Attack=1;
        double PathSeconds=30;
    };

    BOUNDLESSIMMORTALITY_API int32 StageId(int32 World,int32 Index);
    BOUNDLESSIMMORTALITY_API bool SplitStageId(int32 Id,int32& OutWorld,int32& OutIndex);
    BOUNDLESSIMMORTALITY_API bool MakeStage(int32 World,int32 Index,FStage& Out);
    /** The stage that must be cleared first, 0 for the very first stage. */
    BOUNDLESSIMMORTALITY_API int32 PreviousStage(int32 StageId);
    /** 2, then 6, 12 and 18 slots as stages are cleared. */
    BOUNDLESSIMMORTALITY_API int32 UnlockedSlots(int32 ClearedStages);

    /** The loadout the difficulty curve assumes for a stage. */
    BOUNDLESSIMMORTALITY_API double ReferenceAttack(int32 Ordinal);
    BOUNDLESSIMMORTALITY_API double ReferenceHealth(int32 Ordinal);
    BOUNDLESSIMMORTALITY_API double PlayerAttack(int32 Realm,double AverageSwordLevel);
    BOUNDLESSIMMORTALITY_API double PlayerHealth(int32 Realm);
    BOUNDLESSIMMORTALITY_API double DefenseFactor(double Defense);
    BOUNDLESSIMMORTALITY_API int64 BreakthroughQi(int32 Realm);
    BOUNDLESSIMMORTALITY_API int32 BreakthroughStones(int32 Realm);

    /** One draw in order: counts toward the guarantee, then forces 七星光芒剑 on draw 500 if it is still missing. */
    BOUNDLESSIMMORTALITY_API FName RollDraw(FRandomStream& Stream,FWanJianPity& Pity,bool& bOutGuaranteed);

    BOUNDLESSIMMORTALITY_API int32 KillCriticalUnits(EKill Kill,int32 Team);
    BOUNDLESSIMMORTALITY_API double TeamDropChance(double BaseChance,int32 Team);
    /** Alone a successful drop pays 1; a team rolls 1-4 with weights 70/20/8/2. */
    BOUNDLESSIMMORTALITY_API int32 RollDropQuantity(FRandomStream& Stream,int32 Team);
    BOUNDLESSIMMORTALITY_API double ExpectedDropQuantity(int32 Team);
    BOUNDLESSIMMORTALITY_API double SpawnMultiplier(int32 Team);
    BOUNDLESSIMMORTALITY_API double TeamHealthMultiplier(int32 Team);

    /** Goods finished since the last claim: capped by storage, and by 8 hours of offline time. */
    BOUNDLESSIMMORTALITY_API int32 Production(double ElapsedSeconds,double IntervalSeconds,int32 PerInterval,int32 Storage);
    BOUNDLESSIMMORTALITY_API double GardenIntervalSeconds(int32 Level);
    BOUNDLESSIMMORTALITY_API int32 GardenStorage(int32 Level);

    /** Monday-based week of Asia/Hong_Kong, the same clock as XianTuRules::DayKey. */
    BOUNDLESSIMMORTALITY_API FString WeekKey(double Utc);
}

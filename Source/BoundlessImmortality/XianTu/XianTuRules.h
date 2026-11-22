#pragma once
#include "CoreMinimal.h"
#include "XianTu/XianTuTypes.h"

/** Static design tables from docs/design/2026-10-21-wuling-resource-v1.4/tables (methods, nodes, rewards, encounters, market). */
namespace XianTuRules
{
    struct FRegion{FName Id;FText Name;FText Theme;FText Phase;bool bOpen=false;int32 Scene=0;};
    struct FPreset{FName Id;FText Name;int32 Reference=0;int32 Waves=3,PerWave=4;float Normal=20,Obstacle=40,Boss=360,Defense=10,Attack=10,Interval=4,Speed=55,BossAttack=20,BossInterval=3;int32 Obstacles=2;};
    /** Kind is the bag category: Material, Consumable (usable props) or Currency. */
    struct FItem{FName Id;FText Name;FName Icon;int32 Quality=0;FText Description;bool bTradable=false;FName Kind=TEXT("Material");};
    struct FNode{FName Id;FText Name;float QiBonus=0;FName Item;int32 Count=0;int32 Interval=0;int32 LeaseMaxSeconds=1800;FText Owner;int32 Scene=0;};
    struct FMethod{FName Id;FText Name;FText Route;bool bOpen=false;FText Main;FText Support;FText Live;};
    struct FBuilding{FName Id;FText Name;FText Description;int32 Cost=100;};

    BOUNDLESSIMMORTALITY_API const TArray<FRegion>& Regions();
    BOUNDLESSIMMORTALITY_API const TArray<FPreset>& Presets();
    BOUNDLESSIMMORTALITY_API const FPreset& Preset(FName Id);
    BOUNDLESSIMMORTALITY_API const TArray<FItem>& Items();
    BOUNDLESSIMMORTALITY_API const FItem* Item(FName Id);
    BOUNDLESSIMMORTALITY_API FText ItemName(FName Id);
    BOUNDLESSIMMORTALITY_API const TArray<FNode>& Nodes();
    BOUNDLESSIMMORTALITY_API const FNode* Node(FName Id);
    BOUNDLESSIMMORTALITY_API const TArray<FMethod>& Methods();
    BOUNDLESSIMMORTALITY_API const FMethod* Method(FName Id);
    BOUNDLESSIMMORTALITY_API const TArray<FBuilding>& Buildings();
    /** world_rewards.csv: fixed items for one completed stage (1..3). */
    BOUNDLESSIMMORTALITY_API TArray<FXianTuReward> StageRewards(int32 Stage);
    BOUNDLESSIMMORTALITY_API FText StageName(FName Route,int32 Stage);
    BOUNDLESSIMMORTALITY_API FText LevelLabel(int32 Level);
    /** Study scrolls needed to reach Level from Level-1 (1/2/3/4/6). */
    BOUNDLESSIMMORTALITY_API int32 StudyNeeded(int32 Level);
    /** Main-mechanic coefficient multiplier: x1.10 at level 3, x1.20 at level 6. */
    BOUNDLESSIMMORTALITY_API float LevelScale(int32 Level);
    /** Realm transitions (from-realm index) that call down a tribulation: BR04..BR10. */
    BOUNDLESSIMMORTALITY_API bool IsTribulation(int32 FromRealm);
    /**
     * 七星挡劫: chance in basis points that a 七星光芒剑 at GuardRealm holds off the backlash of the tribulation from FromRealm
     * (the heavens of the realm it reaches). None until the sword stands a full major realm above that realm; one major
     * realm above holds half the time, and every further major realm halves the chance of breaking (75%, 87.5% …).
     */
    BOUNDLESSIMMORTALITY_API int32 StarWardChance(int32 GuardRealm,int32 FromRealm);
    BOUNDLESSIMMORTALITY_API int32 SupportSlots(int32 Realm);
    BOUNDLESSIMMORTALITY_API void MarketRange(FName Item,int32& Min,int32& Max,int32& Recycle);
    BOUNDLESSIMMORTALITY_API FString DayKey(double Utc);
    BOUNDLESSIMMORTALITY_API FText Clock(double Seconds);
    BOUNDLESSIMMORTALITY_API float NameLength(const FString& Name);
    BOUNDLESSIMMORTALITY_API FText StateText(EXianTuWorldState State);
    BOUNDLESSIMMORTALITY_API FText StateText(EXianTuRaceState State);
    BOUNDLESSIMMORTALITY_API FText StateText(EXianTuLeaseState State);
    BOUNDLESSIMMORTALITY_API FText StateText(EXianTuTribState State);
    BOUNDLESSIMMORTALITY_API FText StateText(EXianTuOrderState State);

    constexpr double WorldPrepare=60,WorldStageEnd1=240,WorldStageEnd2=420,WorldStageEnd3=540,WorldEnd=600,WorldRecover=30;
    constexpr double RacePrepare=120,RaceRun=360,RaceSettle=120;
    constexpr double TribPrepare=15,TribTrial=60,TribReveal=8;
    /** Backlash after a failed roll: a bolt every TribBacklashInterval for TribBacklash seconds; surviving it breaks the Dao heart. */
    constexpr double TribBacklash=10,TribBacklashInterval=.2,DaoHeartDuration=600;
    constexpr int32 TribBacklashStrikes=50;
    /** Defense lost while the Dao heart is broken. */
    constexpr double DaoHeartDefense=.3;
}

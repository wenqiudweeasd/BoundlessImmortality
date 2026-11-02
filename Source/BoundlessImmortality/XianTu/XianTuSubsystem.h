#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Tickable.h"
#include "XianTu/XianTuTypes.h"
#include "LiveInteractionSubsystem.h"
#include "XianTu/XianTuCatalog.h"
#include "XianTuSubsystem.generated.h"
class ALiveMonster;
class ALiveWeaponSwarm;
class APawn;
class UCultivationSubsystem;
class AXianTuTribulationStorm;

/**
 * Local implementation of the 仙途 hub (design v1.4): world trials, spirit-vein contests and their leases,
 * the 武灵 guild, methods, tribulation with audience assists, and the prototype market. Real gameplay
 * drives it (monsters spawned and killed in the live scene, audience comments, the anchor's health);
 * opponents and market counterparties are clearly simulated. A time scale accelerates local checks.
 */
UCLASS()
class BOUNDLESSIMMORTALITY_API UXianTuSubsystem : public UGameInstanceSubsystem,public FTickableGameObject
{
    GENERATED_BODY()
public:
    virtual void Initialize(FSubsystemCollectionBase& Collection)override;
    virtual void Deinitialize()override;
    virtual void Tick(float DeltaTime)override;
    virtual TStatId GetStatId()const override;
    virtual bool IsTickable()const override;
    virtual bool IsTickableWhenPaused()const override{return false;}
    virtual UWorld* GetTickableGameObjectWorld()const override;

    TSharedRef<const FXianTuSave> GetState()const{return State;}
    /** Changes when the structure the UI lays out changes; timers tick without bumping it. */
    int32 GetRevision()const{return Revision;}
    struct FNotice{FText Text;int32 Kind=0;FText Title;TArray<FXianTuReward> Rewards;};
    TArray<FNotice> TakeNotices(){TArray<FNotice> Out=MoveTemp(Notices);Notices.Reset();return Out;}
    /** UI and GM entry point. Verbs: World*, Race*, Guild*, Method*, Trib*, Market*, Shop*, Cosmetic*, Fortune*, Prop*, Break*, Gear*, Scale. */
    bool Execute(const FString& Verb,const TArray<FString>& Args,FText& OutMessage);

    bool InGameplay()const;
    bool IsBusy(FText* OutReason=nullptr)const;
    bool WantsAmbientPaused()const;
    int32 AnchorRealm()const;
    FText AnchorRealmName(int32 Offset=0)const;
    double AnchorQi()const;
    double AnchorQiRequired()const;
    bool CanStartTribulation(FText* OutReason)const;
    /** Basis points: 5000 + 100 per distinct assist (max 50) + 1000 per earlier failure (max 5000). */
    int32 TribulationProbability(int32 Assists)const;
    int32 TribulationFailures()const;
    /** The storm of the running tribulation, while one is on screen. */
    AXianTuTribulationStorm* GetTribulationStorm()const{return Storm.Get();}
    /** Sim seconds until a broken Dao heart heals; 0 when it is whole. */
    double DaoHeartRemaining()const{return FMath::Max(0.0,State->Trib.DaoHeartUntil-State->SimNow);}
    /** The storm reports each backlash bolt it landed (health removed). */
    void NoteBacklashStrike(double Damage);
    /**
     * 七星挡劫 of the running backlash (None outside one, or while no 七星光芒剑 flies for the anchor). Held: the sword's
     * guard keeps the backlash from felling the anchor. Broken: the heavens shatter its light, and the guard's shield and
     * the 草莓甜点 star healing do nothing against the bolts. Rolled once per backlash, when the first such sword is there.
     */
    EXianTuWard GetStarWard()const{return State->Trib.State==EXianTuTribState::Backlash?State->Trib.Ward:EXianTuWard::None;}
    /** Realm of the strongest 七星光芒剑 flying for the anchor, -1 without one. */
    int32 StarWardRealm()const;
    int32 ItemCount(FName Item)const;
    int64 Stones()const{return State->Stones;}
    int32 SupportSlots()const;
    int32 FormalMembers(bool bPresentOnly)const;
    int32 TemporaryHelpers()const;
    const FXianTuLease* ActiveLease()const;
    /** Race clock helpers for the live page: timing op and representative response windows. */
    bool OpWindow(int32& OutIndex,double& OutRemaining)const;
    bool ResponseWindow(int32& OutIndex,double& OutRemaining)const;
    int32 RaceTask()const;
    double SimNow()const{return State->SimNow;}

    // ---- item economy (XianTuCatalog): currencies, shops, 每日一签, props and the v1.4 breakthrough
    int64 Wallet(XianTuCatalog::ECurrency Currency)const;
    /** Units of a SKU bought today (daily limits roll over at Asia/Hong_Kong midnight). */
    int32 BoughtToday(FName Sku)const;
    /** 0 练气 .. 10 混元无极. */
    int32 AnchorMajor()const;
    /** Today's draw; None before the first draw of the local day. */
    XianTuCatalog::EFortune TodayFortune()const;
    /** Luck bonus of today's draw (0.5 = +50%): raises the 机缘 chance of world trials. */
    float LuckBonus()const;
    /** Basis points the day's omen takes off the tribulation chance (灾). */
    int32 FortunePenalty()const;
    /** Real seconds until the next local day. */
    double SecondsToNextDay()const;
    /** The major stage the anchor's next breakthrough opens; nullptr for a minor (小境界) step. */
    const XianTuCatalog::FStage* NextStage()const;
    /** The next major stage at or above the anchor's realm (materials worth gathering now). */
    const XianTuCatalog::FStage* UpcomingStage()const;
    const FXianTuBreakPlan& GetPlan()const{return State->Plan;}
    /** Quality score Q of the laid-out materials (0 without a core). */
    int32 PlanQuality()const;
    /** Graded stock left for a slot after what the plan already holds. */
    int32 FreeCount(const XianTuCatalog::FStage& Stage,XianTuCatalog::ECategory Category,int32 Grade)const;
    /** A breakthrough the breakthrough screen can commit now (minor, or a major below the tribulation stages). */
    bool CanBreakthrough(FText* OutReason)const;
    const FXianTuBreakRecord* BreakRecord(FName Stage)const;
    /** Attack, health and defense factor of the best breakthrough snapshots (1..1.10). */
    double QualityFactor()const;
    /** Real seconds until a prop can be used again. */
    float PropCooldown(FName Prop)const;
    bool CeremonyActive()const{return State->CeremonyUntil>State->SimNow;}
    /** Tests: the next draws land on this fortune (1..5); 0 draws at random again. */
    void DebugForceFortune(int32 Kind){ForcedFortune=FMath::Clamp(Kind,0,5);}
    /** Tests: forgets today's draw so the page can draw again. */
    void DebugResetFortune();

    /** Method effects on the anchor's own sword: returns the boosted hit and queues echo damage. */
    float ModifyPersonalHit(ALiveMonster* Target,float Damage,const FString& OwnerId);

    void DebugGrantBundle();
    /** Also heals a broken Dao heart, so a rehearsal can run again at once. */
    bool DebugPrepareTribulation();
    /** Tests: puts back the failure streaks and Dao heart a rehearsal found, so it leaves neither pity nor a broken heart on the save. */
    void DebugRestoreTribulation(const TMap<FString,int32>& Failures,double DaoHeartUntil);
    /** Tests: every following roll uses this value (0..10000; 10000 always fails, even at 100%, 0 always succeeds); negative rolls at random again. */
    void DebugForceRoll(int32 Roll){ForcedRoll=Roll<0?-1:FMath::Min(Roll,10000);}
    int32 DebugGetForcedRoll()const{return ForcedRoll;}
    /** Tests: every following 七星挡劫 roll uses this value (0 holds whenever the chance is above 0, 10000 always breaks); negative rolls at random. */
    void DebugForceWardRoll(int32 Roll){ForcedWardRoll=Roll<0?-1:FMath::Min(Roll,10000);}
    int32 DebugAssists(int32 Count);
    int32 DebugResponses();

private:
    TSharedRef<FXianTuSave> State=MakeShared<FXianTuSave>();
    int32 Revision=0;
    bool bInitialized=false,bDirty=false,bCaughtUp=false;
    float SaveTimer=0,SyncTimer=0,MethodTimer=0;
    double LastRealUtc=0;
    TArray<FNotice> Notices;
    TArray<TWeakObjectPtr<ALiveMonster>> Spawned;
    int32 SpawnedKilled=0,SpawnedTotal=0;
    int32 PersonalHits=0;
    double LastPersonalHit=-100,EchoSupportReady=0,GuardReady=0;
    struct FDelayed{TWeakObjectPtr<ALiveMonster> Target;float Amount=0;double At=0;FString Owner;};
    TArray<FDelayed> Delayed;
    TWeakObjectPtr<ULiveInteractionSubsystem> Interaction;

    void Touch(){++Revision;bDirty=true;}
    void Notify(const FText& Text,int32 Kind=0,const FText& Title=FText(),const TArray<FXianTuReward>& Rewards={});
    void Record(const FString& Kind,const FString& Text);
    FString NextId(const TCHAR* Prefix);
    UCultivationSubsystem* Cultivation()const;
    APawn* AnchorPawn()const;
    ALiveWeaponSwarm* Swarm()const;
    void Grant(const TArray<FXianTuReward>& Rewards);
    bool Spend(FName Item,int32 Count);
    int64* WalletSlot(XianTuCatalog::ECurrency Currency);
    void RollShopDay();
    bool BuySku(FName Sku,int32 Quantity,FText& Out);
    bool BuyCosmetic(FName Id,FText& Out);
    bool WearCosmetic(FName Id,FText& Out);
    bool DrawFortune(FText& Out);
    /** Keeps the day's fortune on the anchor's buff list (and takes it off at midnight). */
    void SyncFortune();
    int32 ForcedFortune=0;
    /** Fortune currently on the buff list (EFortune), so a new draw replaces it. */
    uint8 AppliedFortune=0;
    bool UseProp(FName Id,FText& Out);
    TMap<FName,double> PropReady;
    /** Resets the plan when the anchor stands before another stage and keeps slot arrays at the stage's caps. */
    void NormalizePlan();
    bool EditPlan(const FString& Verb,const TArray<FString>& Args,FText& Out);
    bool PlanAvailable(FText* OutReason)const;
    /** Puts the best owned core in an empty core slot (the 天劫 page starts without the breakthrough screen). */
    void EnsurePlanCore();
    void ConsumePlan();
    bool BreakNow(FText& Out);
    bool Refine(int32 Category,int32 Grade,FText& Out);
    /** Stores the stage's snapshot when it beats the old one, and grants 元魂. Returns the 元魂 granted. */
    int32 RecordBreak(const XianTuCatalog::FStage& Stage,int32 Quality,int32 FromRealm);
    void SyncQualityFactor();
    bool BuildGuild(FName Building,FText& Out);
    bool TryGear(FName Item,FText& Out);
    void HandleRealmAdvanced(const FString& Id,int32 Previous,int32 Current);
    FDelegateHandle RealmAdvancedHandle;
    /** World-trial 机缘: one extra material per completed stage at FortuneChanceBase × (1 + luck). */
    void RollFortuneDrops(int32 Stages,TArray<FXianTuReward>& Rewards);
    void RollDaily();
    void Load();
    void Save();
    void CatchUpAfterLoad();

    /** Realm ≥ 0 gives the target a realm, so lower-realm swords are suppressed by the core damage formula (world trials only). */
    ALiveMonster* SpawnMonster(float Health,float Attack,float Interval,float Speed,float Defense,bool bStationary,float Scale,float MinRadius,float MaxRadius,int32 Realm=-1);
    void ClearSpawned();
    int32 CountKilled();

    bool StartWorld(FName Region,FName Preset,FName Route,FText& Out);
    void TickWorld(double Dt);
    void BeginStage(int32 Stage);
    void SpawnWave();
    void EndStage(bool bDone);
    void SettleWorld();

    bool QueueRace(FName Node,FText& Out);
    void TickRace(double Dt);
    void SpawnRaceTask();
    void SettleRace();
    void TickLeases(double Dt);

    void SyncGuild();
    void TickGuildReview();
    bool CreateGuild(const FString& Name,FText& Out);

    bool StartTribulation(FText& Out);
    void TickTribulation(double Dt);
    /** Keeps a storm in the anchor's world while a tribulation prepares or runs; the storm fades out on its own. */
    void UpdateStorm();
    /** The anchor sits in meditation, unable to move, from the tribulation's preparation until it is settled. */
    void SyncTribulationMeditation();
    TWeakObjectPtr<AXianTuTribulationStorm> Storm;
    /** One roll; a failure turns into the backlash, or (bBacklash false, e.g. catching up after a restart) settles at once. */
    void ResolveTribulation(bool bBacklash=true);
    /** Settles a failure: a fallen anchor loses cultivation, one that outlasted the backlash has its Dao heart broken. */
    void SettleBacklash(bool bDied);
    /** Keeps the 道心破碎 debuff on the anchor while the heart is broken (the buff list is per world). */
    void SyncDaoHeart();
    TWeakObjectPtr<class UCultivationHealthComponent> BacklashHealth;
    int32 ForcedRoll=-1,ForcedWardRoll=-1;
    /** Rolls 七星挡劫 once a 七星光芒剑 flies for the anchor during the backlash (it may be summoned after the backlash began). */
    void UpdateStarWard();
    /** " · 七星挡劫 …" / " · 天雷破盾 …" for records and notices, empty without a ward roll. */
    FString WardNote()const;
    float DaoHeartTimer=0;
    bool bQualitySynced=false;
    UFUNCTION() void HandleAnchorDied();
    void ReleaseAssistLeases();

    void TickMarket(double Dt);
    void RefillBoard();

    void TickMethods(float RealDelta);

    UFUNCTION() void HandleComment(const FLiveComment& Comment);
};

#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "WanJian/WanJianBattle.h"
#include "WanJian/WanJianPlatform.h"
#include "WanJian/WanJianTypes.h"
#include "WanJianSubsystem.generated.h"

/**
 * The single-player account of 万剑仙宗. Every write is one transaction with a ledger entry; a repeated request id
 * returns its earlier result without charging or granting twice. Claims return true when the request has been applied,
 * now or by an earlier call with the same id.
 */
UCLASS()
class BOUNDLESSIMMORTALITY_API UWanJianSubsystem:public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    virtual void Initialize(FSubsystemCollectionBase& Collection)override;
    virtual void Deinitialize()override;

    const FWanJianSave& GetState()const{return State;}
    int64 Balance(FName Asset)const{return State.Wallet.FindRef(Asset);}
    IWanJianPlatform& GetPlatform(){return *Platform;}

    /** Starts a clean account in memory, without touching the save file. */
    void ResetForTests(int32 Seed=7);
    void SetClockForTests(TFunction<double()> Clock){ClockOverride=MoveTemp(Clock);}
    void SetPlatformForTests(TSharedRef<IWanJianPlatform> InPlatform){Platform=InPlatform;}
    FString ExportState()const;
    bool ImportState(const FString& Json);

    bool ClaimTutorial(const FString& Request,FText& Out);
    bool ChooseRoot(EWanJianElement Element,FText& Out);

    const FWanJianDrawReceipt* DrawSwords(const FString& Request,int32 Count,int64 ExpectedVersion,FText& Out);

    int32 UnlockedSlots()const;
    bool Equip(int32 Instance,int32 Slot,FText& Out);
    bool Unequip(int32 Slot,FText& Out);
    bool MeltSword(int32 Instance,bool bConfirmed,FText& Out);
    bool LevelUpSword(int32 Instance,FText& Out);

    bool IsStageUnlocked(int32 StageId,FText* OutReason=nullptr)const;
    bool IsStageCleared(int32 StageId)const{return State.ClearedStages.Contains(StageId);}
    /** Battle inputs from the current account: lineup, methods, root and attack. */
    WanJianBattle::FSetup MakeBattle(int32 StageId,int32 Team=1)const;
    bool ClaimStageClear(const FString& Request,int32 StageId,const WanJianBattle::FResult& Battle,FText& Out);

    bool ClaimDailyTasks(const FString& Request,FText& Out);
    bool ClaimDailySecret(const FString& Request,FText& Out);
    bool ClaimWeekly(const FString& Request,FText& Out);
    bool ClaimRewardedAd(const FString& Request,FText& Out);

    int32 GardenReady()const;
    bool ClaimGarden(const FString& Request,FText& Out);
    bool UpgradeBuilding(FName Building,FText& Out);
    bool StartForge(const FString& Request,FText& Out);
    bool ClaimForge(const FString& Request,FText& Out);
    bool Research(EWanJianElement Element,const FString& Request,FText& Out);

    bool ClaimDungeon(const FString& Request,FName Dungeon,int32 Team,FName FragmentMethod,const WanJianBattle::FResult& Battle,FText& Out);
    bool LearnMethod(FName Method,FText& Out);
    bool EquipMethods(const TArray<FName>& Methods,FText& Out);

    bool Breakthrough(const FString& Request,FText& Out);

    /** GDD 7.2: replays the ledger and reports any balance it cannot explain. */
    bool Reconcile(FString& OutReport)const;

private:
    double Now()const;
    void NewAccount(int32 Seed);
    void Load();
    void Commit();
    void Grant(FName Asset,int64 Delta,const FString& Source,const FString& Request);
    bool Charge(FName Asset,int64 Amount,const FString& Source,const FString& Request);
    void RefreshPeriod();
    bool AlreadyApplied(const FString& Request)const{return State.Requests.Contains(Request);}
    void Remember(const FString& Request){State.Requests.Add(Request);}
    FWanJianSwordInstance* FindInstance(int32 Instance);
    FWanJianBuilding* FindBuilding(FName Id);
    FRandomStream StreamFor(const FString& Request)const;
    int32 AddSword(FName Sword);

    FWanJianSave State;
    TSharedRef<IWanJianPlatform> Platform=MakeShared<FWanJianLocalPlatform>();
    TFunction<double()> ClockOverride;
    bool bPersist=false;
};

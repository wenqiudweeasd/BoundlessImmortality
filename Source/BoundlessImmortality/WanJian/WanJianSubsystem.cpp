#include "WanJian/WanJianSubsystem.h"
#include "Cultivation/CultivationSubsystem.h"
#include "JsonObjectConverter.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "WanJian/WanJianCatalog.h"
#include "WanJian/WanJianRules.h"
#include "XianTu/XianTuRules.h"

#define LOCTEXT_NAMESPACE "WanJian"

using namespace WanJianCatalog;
using namespace WanJianRules;

namespace
{
    constexpr int32 TutorialTickets=10;
    constexpr int32 FirstClearTickets=1;
    constexpr int32 DailyTaskTickets=8;
    constexpr int32 DailySecretTickets=3;
    constexpr int32 WeeklyTickets=14;
    constexpr int32 AdTickets=10;
    constexpr int32 DailyTaskWins=3;
    constexpr int32 WeeklyTaskWins=15;
    constexpr int32 ForgeOre=10;
    constexpr int32 ForgeStone=40;
    constexpr double ForgeSeconds=3.0*3600.0;
    constexpr int32 MaxResearch=5;

    FString SavePath(){return FPaths::ProjectSavedDir()/TEXT("WanJian/wanjian-local.json");}
    FName ElementKey(EWanJianElement Element){return FName(ElementName(Element));}
}

void UWanJianSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    bPersist=true;
    Load();
}

void UWanJianSubsystem::Deinitialize()
{
    Commit();
    bPersist=false;
    Super::Deinitialize();
}

double UWanJianSubsystem::Now()const{return ClockOverride?ClockOverride():UCultivationSubsystem::UtcNow();}

void UWanJianSubsystem::NewAccount(int32 Seed)
{
    State=FWanJianSave();
    State.Seed=Seed;
    State.CreatedUtc=Now();
    State.Lineup.Init(0,MaxLineup);
    State.Lineup[0]=AddSword(StarterSword());
    for(const FBuilding& Def:Buildings())
    {
        FWanJianBuilding B;
        B.Id=Def.Id;
        B.LastClaimUtc=State.CreatedUtc;
        State.Buildings.Add(B);
    }
    RefreshPeriod();
}

void UWanJianSubsystem::ResetForTests(int32 Seed)
{
    bPersist=false;
    NewAccount(Seed);
}

void UWanJianSubsystem::Load()
{
    FString Json;
    if(FFileHelper::LoadFileToString(Json,*SavePath())&&ImportState(Json))
    {
        RefreshPeriod();
        return;
    }
    NewAccount(int32(GetTypeHash(FGuid::NewGuid())));
    Commit();
}

FString UWanJianSubsystem::ExportState()const
{
    FString Json;
    FJsonObjectConverter::UStructToJsonObjectString(State,Json);
    return Json;
}

bool UWanJianSubsystem::ImportState(const FString& Json)
{
    FWanJianSave Loaded;
    if(!FJsonObjectConverter::JsonObjectStringToUStruct(Json,&Loaded)||Loaded.Lineup.Num()!=MaxLineup)return false;
    State=MoveTemp(Loaded);
    return true;
}

void UWanJianSubsystem::Commit()
{
    ++State.Version;
    if(!bPersist)return;
    FFileHelper::SaveStringToFile(ExportState(),*SavePath(),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}

void UWanJianSubsystem::Grant(FName Asset,int64 Delta,const FString& Source,const FString& Request)
{
    if(Delta==0)return;
    int64& Value=State.Wallet.FindOrAdd(Asset);
    Value+=Delta;
    FWanJianLedgerEntry Entry;
    Entry.Utc=Now();
    Entry.Asset=Asset;
    Entry.Delta=Delta;
    Entry.Balance=Value;
    Entry.Source=Source;
    Entry.Request=Request;
    State.Ledger.Add(MoveTemp(Entry));
}

bool UWanJianSubsystem::Charge(FName Asset,int64 Amount,const FString& Source,const FString& Request)
{
    if(Balance(Asset)<Amount)return false;
    Grant(Asset,-Amount,Source,Request);
    return true;
}

void UWanJianSubsystem::RefreshPeriod()
{
    const FString Day=XianTuRules::DayKey(Now());
    const FString Week=WeekKey(Now());
    if(State.Period.Day!=Day)
    {
        const FString KeptWeek=State.Period.Week;
        const int32 KeptWins=State.Period.WinsThisWeek;
        const bool bKeptWeekly=State.Period.bWeeklyClaimed;
        State.Period=FWanJianPeriod();
        State.Period.Day=Day;
        State.Period.Week=KeptWeek;
        State.Period.WinsThisWeek=KeptWins;
        State.Period.bWeeklyClaimed=bKeptWeekly;
    }
    if(State.Period.Week!=Week)
    {
        State.Period.Week=Week;
        State.Period.WinsThisWeek=0;
        State.Period.bWeeklyClaimed=false;
    }
}

FWanJianSwordInstance* UWanJianSubsystem::FindInstance(int32 Instance)
{
    return State.Swords.FindByPredicate([Instance](const FWanJianSwordInstance& S){return S.Instance==Instance;});
}

FWanJianBuilding* UWanJianSubsystem::FindBuilding(FName Id)
{
    return State.Buildings.FindByPredicate([Id](const FWanJianBuilding& B){return B.Id==Id;});
}

FRandomStream UWanJianSubsystem::StreamFor(const FString& Request)const
{
    return FRandomStream(int32(HashCombine(GetTypeHash(Request),uint32(State.Seed))));
}

int32 UWanJianSubsystem::AddSword(FName Sword)
{
    FWanJianSwordInstance Instance;
    Instance.Instance=State.NextInstance++;
    Instance.Sword=Sword;
    State.Swords.Add(Instance);
    return Instance.Instance;
}

bool UWanJianSubsystem::ClaimTutorial(const FString& Request,FText& Out)
{
    if(State.bTutorialRewarded)return true;
    State.bTutorialRewarded=true;
    Grant(Asset::Ticket,TutorialTickets,TEXT("tutorial"),Request);
    Commit();
    Out=LOCTEXT("Tutorial","新手引导完成：获得 10 张剑券");
    return true;
}

bool UWanJianSubsystem::ChooseRoot(EWanJianElement Element,FText& Out)
{
    if(Element==EWanJianElement::None)return false;
    RefreshPeriod();
    if(State.Root==Element)return true;
    if(State.Root!=EWanJianElement::None)
    {
        // One free correction for new players, then one free change per day.
        if(State.bFreeRootChangeUsed&&State.Period.bRootChanged)
        {
            Out=LOCTEXT("RootLimit","今日已切换过灵根");
            return false;
        }
        if(!State.bFreeRootChangeUsed)State.bFreeRootChangeUsed=true;
        else State.Period.bRootChanged=true;
    }
    State.Root=Element;
    Commit();
    return true;
}

const FWanJianDrawReceipt* UWanJianSubsystem::DrawSwords(const FString& Request,int32 Count,int64 ExpectedVersion,FText& Out)
{
    if(const FWanJianDrawReceipt* Existing=State.Receipts.FindByPredicate([&Request](const FWanJianDrawReceipt& R){return R.Request==Request;}))
    {
        return Existing;
    }
    if(Count!=1&&Count!=10)
    {
        Out=LOCTEXT("DrawCount","只能抽一次或十连");
        return nullptr;
    }
    if(ExpectedVersion!=State.Version)
    {
        Out=LOCTEXT("DrawVersion","账号状态已变化，请刷新后再抽");
        return nullptr;
    }
    if(!Charge(Asset::Ticket,Count,TEXT("draw"),Request))
    {
        Out=LOCTEXT("DrawTicket","剑券不足");
        return nullptr;
    }

    FWanJianDrawReceipt Receipt;
    Receipt.Request=Request;
    Receipt.Utc=Now();
    Receipt.PoolVersion=PoolVersion;
    FRandomStream Stream=StreamFor(Request);
    for(int32 Draw=0;Draw<Count;++Draw)
    {
        FWanJianDrawResult Result;
        Result.Sword=RollDraw(Stream,State.Pity,Result.bGuaranteed);
        Result.Instance=AddSword(Result.Sword);
        Result.PityAfter=State.Pity.Draws;
        Receipt.Results.Add(Result);
    }
    State.Receipts.Add(MoveTemp(Receipt));
    Commit();
    return &State.Receipts.Last();
}

int32 UWanJianSubsystem::UnlockedSlots()const{return WanJianRules::UnlockedSlots(State.ClearedStages.Num());}

bool UWanJianSubsystem::Equip(int32 Instance,int32 Slot,FText& Out)
{
    if(Slot<0||Slot>=UnlockedSlots())
    {
        Out=LOCTEXT("SlotLocked","剑位尚未解锁");
        return false;
    }
    const FWanJianSwordInstance* Owned=FindInstance(Instance);
    if(!Owned)
    {
        Out=LOCTEXT("NotOwned","未拥有这柄剑");
        return false;
    }
    const FSword* Def=Sword(Owned->Sword);
    if(!Def)return false;

    int32 SameKind=0;
    for(int32 I=0;I<State.Lineup.Num();++I)
    {
        if(State.Lineup[I]==0||I==Slot)continue;
        if(State.Lineup[I]==Instance)
        {
            Out=LOCTEXT("AlreadyEquipped","这柄剑已在其他剑位");
            return false;
        }
        const FWanJianSwordInstance* Other=FindInstance(State.Lineup[I]);
        if(Other&&Other->Sword==Owned->Sword)++SameKind;
    }
    if(SameKind>=Def->MaxInLineup)
    {
        Out=LOCTEXT("TooManySame","同种剑灵已达上阵上限");
        return false;
    }
    State.Lineup[Slot]=Instance;
    Commit();
    return true;
}

bool UWanJianSubsystem::Unequip(int32 Slot,FText& Out)
{
    if(!State.Lineup.IsValidIndex(Slot))return false;
    State.Lineup[Slot]=0;
    Commit();
    return true;
}

bool UWanJianSubsystem::MeltSword(int32 Instance,bool bConfirmed,FText& Out)
{
    if(!bConfirmed)
    {
        Out=LOCTEXT("MeltConfirm","熔炼不可撤销，需要二次确认");
        return false;
    }
    if(State.Lineup.Contains(Instance))
    {
        Out=LOCTEXT("MeltEquipped","请先从剑阵卸下");
        return false;
    }
    const FWanJianSwordInstance* Owned=FindInstance(Instance);
    const FSword* Def=Owned?Sword(Owned->Sword):nullptr;
    if(!Def)return false;
    Grant(Asset::Shard,MeltShards(Def->Rarity),TEXT("melt"),FString::Printf(TEXT("melt-%d"),Instance));
    State.Swords.RemoveAll([Instance](const FWanJianSwordInstance& S){return S.Instance==Instance;});
    Commit();
    return true;
}

bool UWanJianSubsystem::LevelUpSword(int32 Instance,FText& Out)
{
    FWanJianSwordInstance* Owned=FindInstance(Instance);
    if(!Owned)return false;
    if(Owned->Level>=MaxSwordLevel)
    {
        Out=LOCTEXT("MaxLevel","已达等级上限");
        return false;
    }
    const int64 Shards=5*Owned->Level;
    const int64 Stones=40*Owned->Level;
    if(Balance(Asset::Shard)<Shards||Balance(Asset::Stone)<Stones)
    {
        Out=LOCTEXT("LevelCost","剑魂碎片或灵石不足");
        return false;
    }
    const FString Request=FString::Printf(TEXT("level-%d-%d"),Instance,Owned->Level);
    Grant(Asset::Shard,-Shards,TEXT("sword-level"),Request);
    Grant(Asset::Stone,-Stones,TEXT("sword-level"),Request);
    ++Owned->Level;
    Commit();
    return true;
}

bool UWanJianSubsystem::IsStageUnlocked(int32 Id,FText* OutReason)const
{
    int32 World=0,Index=0;
    FStage Stage;
    if(!SplitStageId(Id,World,Index)||!MakeStage(World,Index,Stage)||!Stage.bOpen)
    {
        if(OutReason)*OutReason=LOCTEXT("WorldLocked","后续开放");
        return false;
    }
    const int32 Previous=PreviousStage(Id);
    if(Previous!=0&&!State.ClearedStages.Contains(Previous))
    {
        if(OutReason)*OutReason=LOCTEXT("StageLocked","请先通关上一关");
        return false;
    }
    return true;
}

WanJianBattle::FSetup UWanJianSubsystem::MakeBattle(int32 StageId,int32 Team)const
{
    WanJianBattle::FSetup Setup;
    Setup.StageId=StageId;
    Setup.Team=Team;
    Setup.Root=State.Root;
    Setup.Methods=State.EquippedMethods;
    Setup.Seed=HashCombine(uint32(State.Seed),uint32(StageId))^uint32(State.Version);
    int32 Levels=0;
    int32 Count=0;
    for(const int32 Instance:State.Lineup)
    {
        const FWanJianSwordInstance* Owned=State.Swords.FindByPredicate([Instance](const FWanJianSwordInstance& S){return S.Instance==Instance;});
        if(Instance==0||!Owned)continue;
        Setup.Swords.Add(Owned->Sword);
        Levels+=Owned->Level;
        ++Count;
    }
    Setup.Attack=PlayerAttack(State.Realm,Count?double(Levels)/Count:1.0);
    Setup.MaxHealth=PlayerHealth(State.Realm);
    return Setup;
}

bool UWanJianSubsystem::ClaimStageClear(const FString& Request,int32 StageId,const WanJianBattle::FResult& Battle,FText& Out)
{
    if(AlreadyApplied(Request))return true;
    RefreshPeriod();
    FText Reason;
    if(!IsStageUnlocked(StageId,&Reason))
    {
        Out=Reason;
        return false;
    }
    if(!Battle.bValid||!Battle.bVictory)
    {
        Out=LOCTEXT("NotWon","未通关，没有奖励");
        return false;
    }

    int32 World=0,Index=0;
    FStage Stage;
    SplitStageId(StageId,World,Index);
    MakeStage(World,Index,Stage);
    const FString Source=FString::Printf(TEXT("stage-%d"),StageId);

    Grant(Asset::Stone,20+Stage.Ordinal*2,Source,Request);
    Grant(Asset::Qi,24+Stage.Ordinal*3,Source,Request);
    Grant(Asset::Ore,1+Stage.Chapter,Source,Request);
    Grant(Asset::Herb,1,Source,Request);

    FRandomStream Stream=StreamFor(Request);
    if(Stream.FRand()<0.25)Grant(Asset::Equipment,1,Source,Request);
    if(Stream.FRand()<0.08)Grant(Asset::Artifact,1,Source,Request);
    if(Stream.FRand()<0.30)Grant(Asset::Consumable,1,Source,Request);

    if(!State.ClearedStages.Contains(StageId))
    {
        State.ClearedStages.Add(StageId);
        Grant(Asset::Ticket,FirstClearTickets,Source,Request);
        // Chapter bosses pay a fixed progress material so progress never rests on a random drop alone.
        if(Stage.bBoss)Grant(Asset::BreakStone,2,Source,Request);
    }
    ++State.Period.WinsToday;
    ++State.Period.WinsThisWeek;
    Remember(Request);
    Commit();
    return true;
}

bool UWanJianSubsystem::ClaimDailyTasks(const FString& Request,FText& Out)
{
    if(AlreadyApplied(Request))return true;
    RefreshPeriod();
    if(State.Period.bDailyTasksClaimed)
    {
        Out=LOCTEXT("DailyDone","今日已领取");
        return false;
    }
    if(State.Period.WinsToday<DailyTaskWins)
    {
        Out=LOCTEXT("DailyWins","今日完成 3 场胜利后可领取");
        return false;
    }
    State.Period.bDailyTasksClaimed=true;
    Grant(Asset::Ticket,DailyTaskTickets,TEXT("daily-tasks"),Request);
    Remember(Request);
    Commit();
    return true;
}

bool UWanJianSubsystem::ClaimDailySecret(const FString& Request,FText& Out)
{
    if(AlreadyApplied(Request))return true;
    RefreshPeriod();
    if(State.Period.bSecretClaimed)
    {
        Out=LOCTEXT("SecretDone","今日秘境首通奖励已领取");
        return false;
    }
    State.Period.bSecretClaimed=true;
    Grant(Asset::Ticket,DailySecretTickets,TEXT("daily-secret"),Request);
    Remember(Request);
    Commit();
    return true;
}

bool UWanJianSubsystem::ClaimWeekly(const FString& Request,FText& Out)
{
    if(AlreadyApplied(Request))return true;
    RefreshPeriod();
    if(State.Period.bWeeklyClaimed)
    {
        Out=LOCTEXT("WeeklyDone","本周已领取");
        return false;
    }
    if(State.Period.WinsThisWeek<WeeklyTaskWins)
    {
        Out=LOCTEXT("WeeklyWins","本周完成 15 场胜利后可领取");
        return false;
    }
    State.Period.bWeeklyClaimed=true;
    Grant(Asset::Ticket,WeeklyTickets,TEXT("weekly"),Request);
    Remember(Request);
    Commit();
    return true;
}

bool UWanJianSubsystem::ClaimRewardedAd(const FString& Request,FText& Out)
{
    if(AlreadyApplied(Request))return true;
    RefreshPeriod();
    if(State.Period.bAdClaimed)
    {
        Out=LOCTEXT("AdDone","今日广告奖励已领取");
        return false;
    }
    const FWanJianAdResult Ad=Platform->ShowRewardedAd();
    if(!Ad.bCompleted)
    {
        Out=LOCTEXT("AdIncomplete","完整观看后才会发放奖励");
        return false;
    }
#if UE_BUILD_SHIPPING
    if(Ad.bMock)
    {
        Out=LOCTEXT("AdMock","发行版不接受本地模拟的广告结果");
        return false;
    }
#endif
    State.Period.bAdClaimed=true;
    Grant(Asset::Ticket,AdTickets,Ad.bMock?TEXT("ad-local-mock"):TEXT("ad"),Request);
    Remember(Request);
    Commit();
    return true;
}

int32 UWanJianSubsystem::GardenReady()const
{
    const FWanJianBuilding* Garden=State.Buildings.FindByPredicate([](const FWanJianBuilding& B){return B.Id==TEXT("Garden");});
    if(!Garden)return 0;
    return Production(Now()-Garden->LastClaimUtc,GardenIntervalSeconds(Garden->Level),2,GardenStorage(Garden->Level));
}

bool UWanJianSubsystem::ClaimGarden(const FString& Request,FText& Out)
{
    if(AlreadyApplied(Request))return true;
    const int32 Ready=GardenReady();
    if(Ready<=0)
    {
        Out=LOCTEXT("GardenEmpty","灵圃还没有可收取的灵草");
        return false;
    }
    FindBuilding(TEXT("Garden"))->LastClaimUtc=Now();
    Grant(Asset::Herb,Ready,TEXT("garden"),Request);
    Remember(Request);
    Commit();
    return true;
}

bool UWanJianSubsystem::UpgradeBuilding(FName Building,FText& Out)
{
    FWanJianBuilding* Target=FindBuilding(Building);
    if(!Target)return false;
    if(Target->Level>=3)
    {
        Out=LOCTEXT("BuildingMax","已达最高等级");
        return false;
    }
    const int64 Cost=Target->Level==1?500:2000;
    const FString Request=FString::Printf(TEXT("upgrade-%s-%d"),*Building.ToString(),Target->Level);
    if(!Charge(Asset::Stone,Cost,TEXT("building-upgrade"),Request))
    {
        Out=LOCTEXT("BuildingCost","灵石不足");
        return false;
    }
    ++Target->Level;
    Commit();
    return true;
}

bool UWanJianSubsystem::StartForge(const FString& Request,FText& Out)
{
    if(AlreadyApplied(Request))return true;
    if(State.Forge.bActive)
    {
        Out=LOCTEXT("ForgeBusy","炼器坊正在工作");
        return false;
    }
    if(Balance(Asset::Ore)<ForgeOre||Balance(Asset::Stone)<ForgeStone)
    {
        Out=LOCTEXT("ForgeCost","需要 10 玄铁和 40 灵石");
        return false;
    }
    Grant(Asset::Ore,-ForgeOre,TEXT("forge"),Request);
    Grant(Asset::Stone,-ForgeStone,TEXT("forge"),Request);
    State.Forge.bActive=true;
    State.Forge.ReadyUtc=Now()+ForgeSeconds;
    Remember(Request);
    Commit();
    return true;
}

bool UWanJianSubsystem::ClaimForge(const FString& Request,FText& Out)
{
    if(AlreadyApplied(Request))return true;
    if(!State.Forge.bActive||Now()<State.Forge.ReadyUtc)
    {
        Out=LOCTEXT("ForgeNotReady","炼制尚未完成");
        return false;
    }
    State.Forge=FWanJianForgeJob();
    Grant(Asset::Crystal,1,TEXT("forge"),Request);
    Remember(Request);
    Commit();
    return true;
}

bool UWanJianSubsystem::Research(EWanJianElement Element,const FString& Request,FText& Out)
{
    if(AlreadyApplied(Request))return true;
    if(Element==EWanJianElement::None)return false;
    int32& Level=State.Research.FindOrAdd(ElementKey(Element));
    if(Level>=MaxResearch)
    {
        Out=LOCTEXT("ResearchMax","这条研究已到上限");
        return false;
    }
    const int64 Stones=60*(Level+1);
    const int64 Herbs=4*(Level+1);
    if(Balance(Asset::Stone)<Stones||Balance(Asset::Herb)<Herbs)
    {
        Out=LOCTEXT("ResearchCost","灵石或灵草不足");
        return false;
    }
    Grant(Asset::Stone,-Stones,TEXT("research"),Request);
    Grant(Asset::Herb,-Herbs,TEXT("research"),Request);
    ++Level;
    Remember(Request);
    Commit();
    return true;
}

bool UWanJianSubsystem::ClaimDungeon(const FString& Request,FName DungeonId,int32 Team,FName FragmentMethod,const WanJianBattle::FResult& Battle,FText& Out)
{
    if(AlreadyApplied(Request))return true;
    RefreshPeriod();
    const FDungeon* Def=Dungeon(DungeonId);
    if(!Def||!Def->bOpen)
    {
        Out=LOCTEXT("DungeonLocked","后续开放");
        return false;
    }
    // A failed run has no drops and spends none of the day's team bonuses (GDD 17.2).
    if(!Battle.bValid||!Battle.bVictory)
    {
        Out=LOCTEXT("DungeonFailed","挑战失败，没有掉落，也不消耗加成次数");
        return false;
    }

    FName Drop=Asset::BreakStone;
    if(DungeonId==TEXT("Scripture"))
    {
        if(!Method(FragmentMethod))return false;
        Drop=MethodFragmentAsset(FragmentMethod);
    }
    // Only a real team spends one of the day's bonuses; a solo run keeps them for later.
    const bool bBonus=Team>1&&State.Period.DungeonBonusUsed<DailyTeamBonus;
    const int32 EffectiveTeam=bBonus?Team:1;
    FRandomStream Stream=StreamFor(Request);
    const FString Source=FString::Printf(TEXT("dungeon-%s"),*DungeonId.ToString());
    if(Stream.FRand()<TeamDropChance(Def->BaseChance,EffectiveTeam))
    {
        Grant(Drop,RollDropQuantity(Stream,EffectiveTeam),Source,Request);
    }
    if(bBonus)++State.Period.DungeonBonusUsed;

    int32& Progress=State.DungeonProgress.FindOrAdd(DungeonId);
    if(++Progress>=Def->FixedEvery)
    {
        Progress=0;
        Grant(Drop,1,TEXT("dungeon-fixed"),Request);
    }
    Grant(Asset::Stone,30,Source,Request);
    ++State.Period.WinsToday;
    ++State.Period.WinsThisWeek;
    Remember(Request);
    Commit();
    return true;
}

bool UWanJianSubsystem::LearnMethod(FName MethodId,FText& Out)
{
    const FMethod* Def=Method(MethodId);
    if(!Def||State.LearnedMethods.Contains(MethodId))return false;
    if(!Def->bFirstRelease)
    {
        Out=LOCTEXT("MethodLater","后续开放");
        return false;
    }
    const FString Request=FString::Printf(TEXT("learn-%s"),*MethodId.ToString());
    if(!Charge(MethodFragmentAsset(MethodId),Def->Fragments,TEXT("method-learn"),Request))
    {
        Out=LOCTEXT("MethodFragments","功法残页不足");
        return false;
    }
    State.LearnedMethods.Add(MethodId);
    Commit();
    return true;
}

bool UWanJianSubsystem::EquipMethods(const TArray<FName>& Methods,FText& Out)
{
    if(Methods.Num()>MaxEquippedMethods)
    {
        Out=LOCTEXT("MethodSlots","最多配置 3 门功法");
        return false;
    }
    for(const FName Id:Methods)
    {
        if(!State.LearnedMethods.Contains(Id))
        {
            Out=LOCTEXT("MethodUnknown","尚未学会这门功法");
            return false;
        }
    }
    State.EquippedMethods=Methods;
    Commit();
    return true;
}

bool UWanJianSubsystem::Breakthrough(const FString& Request,FText& Out)
{
    if(AlreadyApplied(Request))return true;
    const int64 Qi=BreakthroughQi(State.Realm);
    const int64 Stones=BreakthroughStones(State.Realm);
    if(Balance(Asset::Qi)<Qi||Balance(Asset::BreakStone)<Stones)
    {
        Out=LOCTEXT("BreakCost","修为或破境石不足");
        return false;
    }
    Grant(Asset::Qi,-Qi,TEXT("breakthrough"),Request);
    Grant(Asset::BreakStone,-Stones,TEXT("breakthrough"),Request);
    ++State.Realm;
    Remember(Request);
    Commit();
    return true;
}

bool UWanJianSubsystem::Reconcile(FString& OutReport)const
{
    TMap<FName,int64> Replayed;
    for(const FWanJianLedgerEntry& Entry:State.Ledger)Replayed.FindOrAdd(Entry.Asset)+=Entry.Delta;
    bool bClean=true;
    OutReport.Reset();
    for(const TPair<FName,int64>& Item:State.Wallet)
    {
        const int64 Expected=Replayed.FindRef(Item.Key);
        if(Expected!=Item.Value)
        {
            bClean=false;
            OutReport+=FString::Printf(TEXT("%s: wallet %lld, ledger %lld\n"),*Item.Key.ToString(),Item.Value,Expected);
        }
    }
    return bClean;
}

#undef LOCTEXT_NAMESPACE

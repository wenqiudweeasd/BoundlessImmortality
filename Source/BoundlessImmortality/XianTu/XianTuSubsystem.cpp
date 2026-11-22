#include "XianTu/XianTuSubsystem.h"
#include "XianTu/XianTuTribulationStorm.h"
#include "XianTu/XianTuRules.h"
#include "XianTu/XianTuCatalog.h"
#include "Cultivation/CultivationSubsystem.h"
#include "Cultivation/CultivationHealthComponent.h"
#include "LiveWeapons/LiveMonster.h"
#include "LiveWeapons/LiveWeaponSwarm.h"
#include "LiveWeapons/SpecialSwords/LiveAnchorBuffSubsystem.h"
#include "LiveWeapons/SpecialSwords/LiveSpecialSword.h"
#include "LiveWeapons/SpecialSwords/LiveSpecialSwordComponent.h"
#include "LiveWeapons/SpecialSwords/LiveSwordVfx.h"
#include "CultivationGameplayComponent.h"
#include "JsonObjectConverter.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "GameFramework/Pawn.h"

#define LOCTEXT_NAMESPACE "XianTu"

using namespace XianTuRules;

namespace
{
    FString Path(){return FPaths::ProjectSavedDir()/TEXT("XianTu/xiantu-local.json");}
    double StageStart(int32 Stage){return Stage<=1?WorldPrepare:Stage==2?WorldStageEnd1:WorldStageEnd2;}
    double StageEnd(int32 Stage){return Stage<=1?WorldStageEnd1:Stage==2?WorldStageEnd2:WorldStageEnd3;}
    /** 0 waves, 1 obstacles, 2 boss. The gather route takes the obstacles first. */
    int32 StageKind(FName Route,int32 Stage){if(Stage>=3)return 2;const bool bGather=Route==TEXT("gather");return (Stage==1)!=bGather?0:1;}
    const TCHAR* OpponentNames[]={TEXT("云栖剑阁"),TEXT("赤霄山门"),TEXT("落星宗"),TEXT("寒潭剑宗"),TEXT("紫微书院"),TEXT("玄霜武灵"),TEXT("青鸾阁"),TEXT("天枢观")};
    const TCHAR* SellerNames[]={TEXT("坊市·白鹿"),TEXT("坊市·青竹"),TEXT("坊市·老秦"),TEXT("坊市·苏掌柜"),TEXT("坊市·药童"),TEXT("坊市·铁匠")};
    int32 Bits(int32 V){int32 N=0;while(V){N+=V&1;V>>=1;}return N;}
}

void UXianTuSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Collection.InitializeDependency<UCultivationSubsystem>();
    Super::Initialize(Collection);
    Load();
    if(UCultivationSubsystem* C=Cultivation())RealmAdvancedHandle=C->OnRealmAdvanced.AddUObject(this,&ThisClass::HandleRealmAdvanced);
    bInitialized=true;
}

void UXianTuSubsystem::Deinitialize()
{
    if(UCultivationSubsystem* C=Cultivation())C->OnRealmAdvanced.Remove(RealmAdvancedHandle);
    ClearSpawned();
    if(bDirty)Save();
    bInitialized=false;
    Super::Deinitialize();
}

TStatId UXianTuSubsystem::GetStatId()const{RETURN_QUICK_DECLARE_CYCLE_STAT(UXianTuSubsystem,STATGROUP_Tickables);}
UWorld* UXianTuSubsystem::GetTickableGameObjectWorld()const{return GetWorld();}
bool UXianTuSubsystem::IsTickable()const{return bInitialized&&!HasAnyFlags(RF_ClassDefaultObject)&&GetWorld()&&GetWorld()->IsGameWorld();}

UCultivationSubsystem* UXianTuSubsystem::Cultivation()const{return GetGameInstance()?GetGameInstance()->GetSubsystem<UCultivationSubsystem>():nullptr;}

APawn* UXianTuSubsystem::AnchorPawn()const
{
    const UCultivationSubsystem* C=Cultivation();const UCultivationHealthComponent* H=C?C->GetAnchorHealth():nullptr;
    return H?Cast<APawn>(H->GetOwner()):nullptr;
}

ALiveWeaponSwarm* UXianTuSubsystem::Swarm()const
{
    APawn* P=AnchorPawn();if(!P)return nullptr;
    for(TActorIterator<ALiveWeaponSwarm> It(P->GetWorld());It;++It)if(It->GetPresentationMode()==ELiveWeaponPresentationMode::Gameplay&&It->GetAnchor()==P)return *It;
    return nullptr;
}

bool UXianTuSubsystem::InGameplay()const
{
    const UCultivationSubsystem* C=Cultivation();const UCultivationHealthComponent* H=C?C->GetAnchorHealth():nullptr;
    if(!H||H->bDead||!H->GetWorld()||!H->GetWorld()->IsGameWorld())return false;
    return true;
}

bool UXianTuSubsystem::IsBusy(FText* Reason)const
{
    auto Set=[&](FText T){if(Reason)*Reason=T;return true;};
    const auto& W=State->World.State;
    if(W==EXianTuWorldState::Preparing||W==EXianTuWorldState::Running||W==EXianTuWorldState::Settling||W==EXianTuWorldState::Interrupted)return Set(LOCTEXT("BusyWorld","世界历练进行中"));
    const auto& R=State->Race.State;
    if(R==EXianTuRaceState::Queue||R==EXianTuRaceState::Matched||R==EXianTuRaceState::Ready||R==EXianTuRaceState::Running||R==EXianTuRaceState::Frozen)return Set(LOCTEXT("BusyRace","灵脉竞采进行中"));
    const auto& T=State->Trib.State;
    if(T==EXianTuTribState::Prepare||T==EXianTuTribState::Trial||T==EXianTuTribState::Frozen)return Set(LOCTEXT("BusyTrib","天劫进行中"));
    if(T==EXianTuTribState::Backlash)return Set(LOCTEXT("BusyBacklash","天罚未息"));
    return false;
}

bool UXianTuSubsystem::WantsAmbientPaused()const
{
    const auto W=State->World.State;const auto R=State->Race.State;
    return W==EXianTuWorldState::Preparing||W==EXianTuWorldState::Running||W==EXianTuWorldState::Interrupted||R==EXianTuRaceState::Ready||R==EXianTuRaceState::Running;
}

int32 UXianTuSubsystem::AnchorRealm()const
{
    const UCultivationSubsystem* C=Cultivation();return C?C->GetParticipantSnapshot(C->GetAnchorId()).RealmIndex:0;
}

FText UXianTuSubsystem::AnchorRealmName(int32 Offset)const
{
    const UCultivationSubsystem* C=Cultivation();return C?C->GetRealmName(AnchorRealm()+Offset):FText::GetEmpty();
}

double UXianTuSubsystem::AnchorQi()const{const UCultivationSubsystem* C=Cultivation();return C?C->GetParticipantSnapshot(C->GetAnchorId()).Cultivation:0;}
double UXianTuSubsystem::AnchorQiRequired()const{const UCultivationSubsystem* C=Cultivation();return C?C->GetParticipantSnapshot(C->GetAnchorId()).RequiredCultivation:0;}

int32 UXianTuSubsystem::ItemCount(FName Item)const
{
    if(Item==TEXT("CUR_STONE"))return int32(FMath::Min<int64>(State->Stones,MAX_int32));
    if(XianTuCatalog::ECurrency Cur;XianTuCatalog::ParseCurrency(Item,Cur))return int32(FMath::Min<int64>(Wallet(Cur),MAX_int32));
    // Construction materials live in the guild vault once a guild exists.
    if(State->Guild.bCreated&&Item.ToString().StartsWith(TEXT("GUILD_MAT_")))return State->Guild.Vault.FindRef(Item);
    if(Item==TEXT("MAT_BREAK"))
    {
        // 破境石 is the same stock the character page breakthrough spends.
        const UCultivationSubsystem* C=Cultivation();return C?C->GetParticipantSnapshot(C->GetAnchorId()).BreakthroughMaterialCount:0;
    }
    return State->Items.FindRef(Item);
}

int32 UXianTuSubsystem::SupportSlots()const{return FMath::Min(2,XianTuRules::SupportSlots(AnchorRealm()));}

int32 UXianTuSubsystem::FormalMembers(bool bPresentOnly)const
{
    int32 N=0;for(const auto& M:State->Guild.Members)if(M.bFormal&&(!bPresentOnly||M.bPresent))++N;return N;
}

int32 UXianTuSubsystem::TemporaryHelpers()const
{
    int32 N=0;for(const auto& M:State->Guild.Members)if(!M.bFormal&&M.bPresent)++N;
    if(!State->Guild.bCreated)if(const ALiveWeaponSwarm* S=Swarm())N=S->GetParticipantCount();
    return N;
}

const FXianTuLease* UXianTuSubsystem::ActiveLease()const
{
    return State->Leases.FindByPredicate([](const FXianTuLease& L){return L.State==EXianTuLeaseState::Active;});
}

void UXianTuSubsystem::Notify(const FText& Text,int32 Kind,const FText& Title,const TArray<FXianTuReward>& Rewards)
{
    FNotice N;N.Text=Text;N.Kind=Kind;N.Title=Title;N.Rewards=Rewards;Notices.Add(N);
}

void UXianTuSubsystem::Record(const FString& Kind,const FString& Text)
{
    FXianTuRecord R;R.Utc=UCultivationSubsystem::UtcNow();R.Kind=Kind;R.Text=Text;
    State->Receipts.Insert(R,0);if(State->Receipts.Num()>80)State->Receipts.SetNum(80);
    if(State->Guild.bCreated){State->Guild.Records.Insert(R,0);if(State->Guild.Records.Num()>60)State->Guild.Records.SetNum(60);}
}

FString UXianTuSubsystem::NextId(const TCHAR* Prefix){return FString::Printf(TEXT("%s-%05d"),Prefix,++State->Sequence);}

void UXianTuSubsystem::Grant(const TArray<FXianTuReward>& Rewards)
{
    UCultivationSubsystem* C=Cultivation();
    for(const FXianTuReward& R:Rewards)
    {
        if(R.Count<=0)continue;
        if(R.Item==TEXT("CUR_STONE"))State->Stones+=R.Count;
        else if(R.Item==TEXT("MAT_BREAK")){if(C)C->GrantMaterial(C->GetAnchorId(),C->Rules.BreakthroughMaterialId,R.Count);}
        else if(XianTuCatalog::ECurrency Cur;XianTuCatalog::ParseCurrency(R.Item,Cur)){if(int64* Slot=WalletSlot(Cur))*Slot+=R.Count;}
        else if(State->Guild.bCreated&&R.Item.ToString().StartsWith(TEXT("GUILD_MAT_")))State->Guild.Vault.FindOrAdd(R.Item)+=R.Count;
        else State->Items.FindOrAdd(R.Item)+=R.Count;
    }
    Touch();
}

bool UXianTuSubsystem::Spend(FName Item,int32 Count)
{
    if(Count<=0)return true;
    if(Item==TEXT("CUR_STONE")){if(State->Stones<Count)return false;State->Stones-=Count;return true;}
    if(XianTuCatalog::ECurrency Cur;XianTuCatalog::ParseCurrency(Item,Cur)){int64* Slot=WalletSlot(Cur);if(!Slot||*Slot<Count)return false;*Slot-=Count;return true;}
    if(State->Guild.bCreated&&Item.ToString().StartsWith(TEXT("GUILD_MAT_"))){int32& Have=State->Guild.Vault.FindOrAdd(Item);if(Have<Count)return false;Have-=Count;return true;}
    int32& Have=State->Items.FindOrAdd(Item);if(Have<Count)return false;Have-=Count;return true;
}

void UXianTuSubsystem::RollDaily()
{
    const FString Key=DayKey(UCultivationSubsystem::UtcNow());
    auto& G=State->Guild;
    if(G.DailyKey!=Key){G.DailyKey=Key;G.DailyMaterials=0;G.DailyReputation=0;for(auto& M:G.Members){M.DailyMaterials=0;M.DailySelected=0;M.DailyKey=Key;}}
    if(State->World.DayKey!=Key){State->World.DayKey=Key;State->World.RoundsToday=0;}
    RollShopDay();
}

// ------------------------------------------------------------------ persistence

void UXianTuSubsystem::Load()
{
    FString Json;FXianTuSave Loaded;
    if(FFileHelper::LoadFileToString(Json,*Path())&&FJsonObjectConverter::JsonObjectStringToUStruct(Json,&Loaded))*State=MoveTemp(Loaded);
    State->TimeScale=FMath::Clamp(State->TimeScale,.1f,60.f);
    if(State->World.StageResults.Num()!=3)State->World.StageResults={-1,-1,-1};
    // Leases and orders expire on the clock even while the game is closed.
    const double Now=UCultivationSubsystem::UtcNow();
    if(State->SavedUtc>0&&Now>State->SavedUtc)State->SimNow+=(Now-State->SavedUtc)*State->TimeScale;
    LastRealUtc=Now;
}

void UXianTuSubsystem::Save()
{
    State->SavedUtc=UCultivationSubsystem::UtcNow();
    FString Json;if(!FJsonObjectConverter::UStructToJsonObjectString(*State,Json))return;
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path()),true);
    if(FFileHelper::SaveStringToFile(Json,*Path(),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))bDirty=false;
}

void UXianTuSubsystem::CatchUpAfterLoad()
{
    // Activities interrupted by quitting resolve with what was already confirmed; nothing is replayed.
    const auto W=State->World.State;
    if(W==EXianTuWorldState::Preparing||W==EXianTuWorldState::Running||W==EXianTuWorldState::Interrupted||W==EXianTuWorldState::Settling)SettleWorld();
    const auto R=State->Race.State;
    if(R==EXianTuRaceState::Queue||R==EXianTuRaceState::Matched||R==EXianTuRaceState::Ready){State->Race=FXianTuRace();Touch();}
    else if(R==EXianTuRaceState::Running||R==EXianTuRaceState::Frozen)SettleRace();
    const auto T=State->Trib.State;
    if(T==EXianTuTribState::Prepare){State->Trib.State=EXianTuTribState::Cancelled;Touch();}
    // Nobody stood under the storm while the game was closed: a failure settles as outlasted, without its bolts.
    else if(T==EXianTuTribState::Trial||T==EXianTuTribState::Frozen)ResolveTribulation(false);
    else if(T==EXianTuTribState::Backlash)SettleBacklash(false);
}

// ------------------------------------------------------------------ tick

void UXianTuSubsystem::Tick(float DeltaTime)
{
    if(!FMath::IsFinite(DeltaTime)||DeltaTime<=0)return;
    if(!bCaughtUp&&Cultivation()&&Cultivation()->GetAnchorHealth()){bCaughtUp=true;CatchUpAfterLoad();}
    DeltaTime=FMath::Min(DeltaTime,.25f);
    const double Dt=double(DeltaTime)*FMath::Clamp(State->TimeScale,.1f,60.f);
    State->SimNow+=Dt;
    RollDaily();
    if((SyncTimer-=DeltaTime)<=0.f){SyncTimer=1.f;SyncGuild();TickGuildReview();NormalizePlan();if(!bQualitySynced)SyncQualityFactor();}
    TickWorld(Dt);
    TickRace(Dt);
    TickLeases(Dt);
    TickTribulation(Dt);
    UpdateStorm();
    SyncTribulationMeditation();
    if((DaoHeartTimer-=DeltaTime)<=0.f){DaoHeartTimer=.5f;SyncDaoHeart();SyncFortune();}
    TickMarket(Dt);
    TickMethods(DeltaTime);
    if(bDirty&&(SaveTimer-=DeltaTime)<=0.f){SaveTimer=5.f;Save();}
}

// ------------------------------------------------------------------ monsters

ALiveMonster* UXianTuSubsystem::SpawnMonster(float Health,float Attack,float Interval,float Speed,float Defense,bool bStationary,float Scale,float MinRadius,float MaxRadius,int32 Realm)
{
    APawn* Pawn=AnchorPawn();UWorld* World=Pawn?Pawn->GetWorld():nullptr;if(!World)return nullptr;
    TSubclassOf<ALiveMonster> Class=ALiveMonster::StaticClass();
    const FVector Origin=Pawn->GetActorLocation();
    FCollisionQueryParams Query(SCENE_QUERY_STAT(XianTuSpawnFloor),false,Pawn);
    for(int32 Attempt=0;Attempt<24;++Attempt)
    {
        const float Angle=FMath::FRandRange(0.f,2*PI),Radius=FMath::FRandRange(MinRadius,MaxRadius);
        FVector Position=Origin+FVector(FMath::Cos(Angle)*Radius,FMath::Sin(Angle)*Radius,0);
        FHitResult Floor;
        if(World->LineTraceSingleByObjectType(Floor,Position+FVector(0,0,500),Position-FVector(0,0,1000),FCollisionObjectQueryParams(ECC_WorldStatic),Query))
        {
            if(Floor.ImpactNormal.Z<.6f||FMath::Abs(Floor.ImpactPoint.Z-Origin.Z)>600)continue;
            Position=Floor.ImpactPoint+FVector(0,0,45*Scale);
        }
        const FTransform Transform(FRotator(0,FMath::FRandRange(0.f,360.f),0),Position,FVector(Scale));
        ALiveMonster* M=World->SpawnActorDeferred<ALiveMonster>(Class,Transform,nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
        if(!M)return nullptr;
        M->MaxHealth=FMath::Max(1.f,Health);M->AttackDamage=FMath::Max(0.f,Attack);M->AttackInterval=FMath::Max(.5f,Interval);M->MoveSpeed=Speed;M->Defense=Defense;M->Realm=Realm;
        M->bStationary=bStationary;M->FollowTarget=Pawn;M->Tags.Add(TEXT("XianTu"));
        M->FinishSpawning(Transform);
        Spawned.Add(M);++SpawnedTotal;
        LiveSwordVfx::SpawnAt(M,LiveSwordVfx::EFx::Ring,Position-FVector(0,0,40*Scale),FLinearColor(.9f,.25f,.2f),120.f*Scale);
        return M;
    }
    return nullptr;
}

void UXianTuSubsystem::ClearSpawned()
{
    for(const auto& W:Spawned)if(ALiveMonster* M=W.Get();M&&M->IsAlive())M->Destroy();
    Spawned.Reset();SpawnedKilled=0;SpawnedTotal=0;
}

int32 UXianTuSubsystem::CountKilled()
{
    int32 Killed=0;for(const auto& W:Spawned)if(!W.IsValid()||!W->IsAlive())++Killed;return Killed;
}

// ------------------------------------------------------------------ world trial

bool UXianTuSubsystem::StartWorld(FName Region,FName PresetId,FName Route,FText& Out)
{
    const FRegion* R=Regions().FindByPredicate([&](const FRegion& X){return X.Id==Region;});
    if(!R||!R->bOpen){Out=LOCTEXT("RegionLocked","该区域尚未开放（P4 阶段）");return false;}
    if(IsBusy(&Out))return false;
    if(!InGameplay()){Out=LOCTEXT("NeedPlay","进入游戏并保持存活后才能开始历练");return false;}
    FXianTuWorld& W=State->World;
    const int32 Rounds=W.RoundsToday,Total=W.CompletedTotal;const FString Day=W.DayKey;
    W=FXianTuWorld();W.RoundsToday=Rounds;W.CompletedTotal=Total;W.DayKey=Day;
    W.Region=Region;W.Preset=Preset(PresetId).Id;W.Route=Route==TEXT("gather")?FName(TEXT("gather")):FName(TEXT("guard"));
    W.State=EXianTuWorldState::Preparing;W.StageResults={-1,-1,-1};
    ClearSpawned();
    Record(TEXT("历练"),FString::Printf(TEXT("%s · %s 开始"),*R->Name.ToString(),*Preset(PresetId).Name.ToString()));
    Out=FText::Format(LOCTEXT("WorldStarted","{0} · 准备 60 秒，迎接新人入阵"),R->Name);
    Touch();return true;
}

void UXianTuSubsystem::BeginStage(int32 Stage)
{
    FXianTuWorld& W=State->World;
    W.Stage=Stage;W.State=EXianTuWorldState::Running;W.Elapsed=FMath::Max(W.Elapsed,StageStart(Stage));
    ClearSpawned();W.Wave=0;W.Progress=0;
    const FPreset& P=Preset(W.Preset);
    switch(StageKind(W.Route,Stage))
    {
    case 0:W.Target=P.Waves*P.PerWave;SpawnWave();break;
    case 1:
        W.Target=P.Obstacles;
        for(int32 I=0;I<P.Obstacles;++I)SpawnMonster(P.Obstacle,0,4,0,P.Defense,true,1.7f,500,800,AnchorRealm());
        break;
    default:
        W.Target=1;SpawnMonster(P.Boss,P.BossAttack,P.BossInterval,P.Speed*.8f,P.Defense,false,2.3f,800,1000,AnchorRealm());
        break;
    }
    Notify(FText::Format(LOCTEXT("StageBegin","第{0}阶段 · {1}"),FText::AsNumber(Stage),StageName(W.Route,Stage)));
    Touch();
}

void UXianTuSubsystem::SpawnWave()
{
    FXianTuWorld& W=State->World;const FPreset& P=Preset(W.Preset);
    ++W.Wave;
    // World-trial targets stand at the anchor's realm: lower-realm viewers' swords are suppressed but never below 25%.
    for(int32 I=0;I<P.PerWave;++I)SpawnMonster(P.Normal,P.Attack,P.Interval,P.Speed,P.Defense,false,1.f,700,1100,AnchorRealm());
}

void UXianTuSubsystem::EndStage(bool bDone)
{
    FXianTuWorld& W=State->World;const int32 Stage=FMath::Clamp(W.Stage,1,3);
    W.StageResults[Stage-1]=bDone?1:0;ClearSpawned();
    Notify(bDone?FText::Format(LOCTEXT("StageDone","第{0}阶段完成"),FText::AsNumber(Stage)):FText::Format(LOCTEXT("StageMiss","第{0}阶段时间已到，未完成"),FText::AsNumber(Stage)),bDone?1:2);
    if(Stage<3){W.Elapsed=FMath::Max(W.Elapsed,StageEnd(Stage));BeginStage(Stage+1);return;}
    // The design clock reserves 540..600 for settlement; an early finish settles after a short ceremony.
    W.State=EXianTuWorldState::Settling;W.Elapsed=FMath::Max(W.Elapsed,WorldEnd-5);Touch();
}

void UXianTuSubsystem::TickWorld(double Dt)
{
    FXianTuWorld& W=State->World;
    switch(W.State)
    {
    case EXianTuWorldState::Preparing:
        W.Elapsed+=Dt;
        if(W.Elapsed>=WorldPrepare)BeginStage(1);
        break;
    case EXianTuWorldState::Running:
    {
        if(!InGameplay())
        {
            W.State=EXianTuWorldState::Interrupted;W.RecoverRemaining=WorldRecover;ClearSpawned();
            Notify(LOCTEXT("WorldDown","主播战败 · 30 秒恢复，已完成阶段保留"),2);Touch();break;
        }
        W.Elapsed+=Dt;
        const int32 Killed=CountKilled();W.Progress=FMath::Min(Killed,W.Target);
        const FPreset& P=Preset(W.Preset);
        if(StageKind(W.Route,W.Stage)==0&&Killed>=Spawned.Num()&&W.Wave<P.Waves&&W.Progress<W.Target)SpawnWave();
        if(W.Progress>=W.Target&&W.Target>0)EndStage(true);
        else if(W.Elapsed>=StageEnd(W.Stage))EndStage(false);
        break;
    }
    case EXianTuWorldState::Interrupted:
        W.RecoverRemaining-=Dt;
        if(W.RecoverRemaining<=0)
        {
            if(InGameplay()&&W.Elapsed<WorldStageEnd3){Notify(LOCTEXT("WorldResume","恢复完毕 · 重返历练"),1);BeginStage(FMath::Clamp(W.Stage,1,3));}
            else SettleWorld();
        }
        break;
    case EXianTuWorldState::Settling:
        W.Elapsed+=Dt;
        if(W.Elapsed>=WorldEnd)SettleWorld();
        break;
    default:break;
    }
}

void UXianTuSubsystem::SettleWorld()
{
    FXianTuWorld& W=State->World;
    ClearSpawned();
    TArray<FXianTuReward> Rewards;bool bFull=true;
    for(int32 Stage=1;Stage<=3;++Stage)
    {
        if(!W.StageResults.IsValidIndex(Stage-1)||W.StageResults[Stage-1]!=1){bFull=false;continue;}
        for(const FXianTuReward& R:StageRewards(Stage))
        {
            if(FXianTuReward* E=Rewards.FindByPredicate([&](const FXianTuReward& X){return X.Item==R.Item;}))E->Count+=R.Count;else Rewards.Add(R);
        }
    }
    int32 Cleared=0;for(int32 R:W.StageResults)Cleared+=R==1?1:0;
    auto AddReward=[&](FName Item,int32 Count){if(Count<=0)return;if(FXianTuReward* E=Rewards.FindByPredicate([&](const FXianTuReward& X){return X.Item==Item;}))E->Count+=Count;else{FXianTuReward R;R.Item=Item;R.Count=Count;Rewards.Add(R);}};
    // 灵玉 for every cleared stage and a full round; 武灵 when the round was the guild's; 机缘 rides on today's luck.
    AddReward(TEXT("CUR_JADE"),Cleared*10+(bFull?20:0));
    if(bFull&&State->Guild.bCreated)AddReward(TEXT("CUR_WULING"),10);
    RollFortuneDrops(Cleared,Rewards);
    Grant(Rewards);
    W.LastQi=0;W.LastGuildMaterials=0;W.LastReputation=0;
    if(bFull)
    {
        // A complete ten-minute event: +60 cultivation for the first six rounds of the day.
        if(W.RoundsToday<6)if(UCultivationSubsystem* C=Cultivation();C&&C->AddCultivation(C->GetAnchorId(),60))W.LastQi=60;
        ++W.RoundsToday;
        auto& G=State->Guild;
        if(G.bCreated)
        {
            int32 Present=0;
            for(auto& M:G.Members)if(M.bFormal&&M.bPresent)
            {
                ++Present;
                if(M.DailyMaterials<6&&G.DailyMaterials<100){++M.DailyMaterials;++M.Contribution;++G.Materials;++G.DailyMaterials;++W.LastGuildMaterials;}
            }
            if(Present>=2&&G.DailyReputation<60){G.Reputation+=10;G.DailyReputation+=10;W.LastReputation=10;}
        }
    }
    ++W.CompletedTotal;W.LastRewards=Rewards;W.LastReceipt=NextId(TEXT("WR"));W.State=EXianTuWorldState::Completed;W.Stage=0;
    int32 Done=0;for(int32 R:W.StageResults)Done+=R==1?1:0;
    Record(TEXT("历练"),FString::Printf(TEXT("历练结算 %d/3 阶段 · 资材 +%d · 声望 +%d · 收据 %s"),Done,W.LastGuildMaterials,W.LastReputation,*W.LastReceipt));
    Notify(FText::Format(LOCTEXT("WorldSettled","历练结算 · 完成 {0}/3 阶段"),FText::AsNumber(Done)),1,LOCTEXT("WorldTitle","历练结算"),Rewards);
    Touch();
}

// ------------------------------------------------------------------ spirit-vein contest

bool UXianTuSubsystem::QueueRace(FName NodeId,FText& Out)
{
    if(!Node(NodeId)){Out=LOCTEXT("NoNode","未知资源点");return false;}
    if(!State->Guild.bCreated||State->Guild.Name.IsEmpty()){Out=LOCTEXT("NeedGuild","需先创建并通过审核的武灵才能报名竞采");return false;}
    if(FormalMembers(true)<1){Out=LOCTEXT("NeedMember","至少一名在场正式成员方可报名（临时助战不计入）");return false;}
    if(ActiveLease()){Out=LOCTEXT("HasLease","已有生效中的开采权，到期后才能申请下一轮");return false;}
    if(IsBusy(&Out))return false;
    if(!InGameplay()){Out=LOCTEXT("NeedPlayRace","进入游戏后才能报名");return false;}
    FXianTuRace& R=State->Race;R=FXianTuRace();
    R.State=EXianTuRaceState::Queue;R.Node=NodeId;R.Seed=FMath::Rand();R.MatchAfter=3+FMath::FRandRange(0.f,4.f);
    const int32 Present=FormalMembers(true);R.Tier=Present>=50?2:Present>=10?1:0;
    Out=LOCTEXT("Queued","报名成功（免费）· 正在匹配同档武灵（本地模拟对手）");
    Touch();return true;
}

void UXianTuSubsystem::SpawnRaceTask()
{
    FXianTuRace& R=State->Race;
    const int32 Next=R.TaskResults.IndexOfByKey(-1);if(Next==INDEX_NONE)return;
    ClearSpawned();R.Progress=0;R.Target=4;
    // Unified trial targets: identical for every guild, untouched by gifts or prior leases.
    for(int32 I=0;I<R.Target;++I)SpawnMonster(80.f,4.f,4.f,45.f,0.f,false,1.1f,600,1000);
    Notify(FText::Format(LOCTEXT("RaceTask","共同任务 {0}/6 · 击破 4 个试炼目标"),FText::AsNumber(Next+1)));
}

void UXianTuSubsystem::TickRace(double Dt)
{
    FXianTuRace& R=State->Race;
    switch(R.State)
    {
    case EXianTuRaceState::Queue:
        R.StateElapsed+=Dt;
        if(R.StateElapsed>=R.MatchAfter)
        {
            FRandomStream Rand(R.Seed);
            R.Teams.Reset();
            FXianTuRaceTeam Self;Self.Name=State->Guild.Name;Self.bSelf=true;Self.Members=FormalMembers(true);R.Teams.Add(Self);
            const int32 Opponents=1+Rand.RandRange(0,2);
            TArray<int32> Picks;for(int32 I=0;I<UE_ARRAY_COUNT(OpponentNames);++I)Picks.Add(I);
            for(int32 I=0;I<Opponents;++I)
            {
                const int32 Pick=Picks[Rand.RandRange(0,Picks.Num()-1)];Picks.Remove(Pick);
                FXianTuRaceTeam T;T.Name=FString(OpponentNames[Pick])+TEXT("（本地模拟）");T.Members=FMath::Max(1,Self.Members+Rand.RandRange(-3,3));R.Teams.Add(T);
            }
            R.State=EXianTuRaceState::Matched;R.StateElapsed=0;
            Notify(FText::Format(LOCTEXT("Matched","匹配成功 · {0} 支武灵同场竞采"),FText::AsNumber(R.Teams.Num())),1);Touch();
        }
        break;
    case EXianTuRaceState::Matched:
        R.StateElapsed+=Dt;
        if(R.StateElapsed>=2)
        {
            // Freeze up to ten present formal members; least-selected today first, then a stable key.
            TArray<FXianTuMember*> Pool;for(auto& M:State->Guild.Members)if(M.bFormal&&M.bPresent)Pool.Add(&M);
            Pool.Sort([](const FXianTuMember& A,const FXianTuMember& B){return A.DailySelected!=B.DailySelected?A.DailySelected<B.DailySelected:A.Id<B.Id;});
            R.Representatives.Reset();R.RepresentativeNames.Reset();
            for(int32 I=0;I<Pool.Num()&&I<10;++I){R.Representatives.Add(Pool[I]->Id);R.RepresentativeNames.Add(Pool[I]->Name);++Pool[I]->DailySelected;}
            R.State=EXianTuRaceState::Ready;R.StateElapsed=0;
            Notify(FText::Format(LOCTEXT("Ready","准备期 120 秒 · 已冻结 {0} 名协作代表"),FText::AsNumber(R.Representatives.Num())));Touch();
        }
        break;
    case EXianTuRaceState::Ready:
        R.StateElapsed+=Dt;
        if(R.StateElapsed>=RacePrepare)
        {
            R.State=EXianTuRaceState::Running;R.StateElapsed=0;R.TaskResults={-1,-1,-1,-1,-1,-1};R.OpResults={-1,-1};R.Responses.Reset();
            SpawnRaceTask();Touch();
        }
        break;
    case EXianTuRaceState::Running:
    {
        R.StateElapsed+=Dt;
        const int32 Window=FMath::Clamp(int32(R.StateElapsed/60.0),0,6);
        const int32 Current=R.TaskResults.IndexOfByKey(-1);
        if(Current!=INDEX_NONE)
        {
            R.Progress=FMath::Min(CountKilled(),R.Target);
            if(Spawned.Num()&&R.Target>0&&R.Progress>=R.Target){R.TaskResults[Current]=1;ClearSpawned();Notify(LOCTEXT("TaskDone","共同任务完成 +100"),1);Touch();}
            else if(Window>Current){R.TaskResults[Current]=0;ClearSpawned();Touch();}
            const int32 Next=R.TaskResults.IndexOfByKey(-1);
            if(Next!=INDEX_NONE&&Window>=Next&&Spawned.IsEmpty()&&R.StateElapsed<RaceRun)SpawnRaceTask();
        }
        static const double OpStarts[]={90,270};
        for(int32 K=0;K<2;++K)if(R.OpResults.IsValidIndex(K)&&R.OpResults[K]==-1&&R.StateElapsed>=OpStarts[K]+8){R.OpResults[K]=0;Touch();}
        if(R.StateElapsed>=RaceRun)
        {
            for(int32& T:R.TaskResults)if(T==-1)T=0;
            for(int32& O:R.OpResults)if(O==-1)O=0;
            ClearSpawned();R.State=EXianTuRaceState::Frozen;R.StateElapsed=0;Touch();
        }
        break;
    }
    case EXianTuRaceState::Frozen:
        R.StateElapsed+=Dt;
        if(R.StateElapsed>=1.5)SettleRace();
        break;
    default:break;
    }
}

bool UXianTuSubsystem::OpWindow(int32& OutIndex,double& OutRemaining)const
{
    const FXianTuRace& R=State->Race;if(R.State!=EXianTuRaceState::Running)return false;
    static const double Starts[]={90,270};
    for(int32 K=0;K<2;++K)if(R.OpResults.IsValidIndex(K)&&R.OpResults[K]==-1&&R.StateElapsed>=Starts[K]&&R.StateElapsed<Starts[K]+8){OutIndex=K;OutRemaining=Starts[K]+8-R.StateElapsed;return true;}
    return false;
}

bool UXianTuSubsystem::ResponseWindow(int32& OutIndex,double& OutRemaining)const
{
    const FXianTuRace& R=State->Race;if(R.State!=EXianTuRaceState::Running)return false;
    static const double Starts[]={45,225};
    for(int32 K=0;K<2;++K)if(R.StateElapsed>=Starts[K]&&R.StateElapsed<Starts[K]+25){OutIndex=K;OutRemaining=Starts[K]+25-R.StateElapsed;return true;}
    return false;
}

int32 UXianTuSubsystem::RaceTask()const{return State->Race.TaskResults.IndexOfByKey(-1);}

void UXianTuSubsystem::SettleRace()
{
    FXianTuRace& R=State->Race;
    for(int32& T:R.TaskResults)if(T==-1)T=0;
    for(int32& O:R.OpResults)if(O==-1)O=0;
    ClearSpawned();
    FRandomStream Rand(R.Seed^0x2f3a);
    int32 Valid=0;for(const auto& Pair:R.Responses)Valid+=Bits(Pair.Value);
    const int32 Reps=FMath::Max(1,R.Representatives.Num());
    for(FXianTuRaceTeam& T:R.Teams)
    {
        if(T.bSelf)
        {
            T.TaskScore=0;for(int32 V:R.TaskResults)T.TaskScore+=V==1?100:0;
            T.AnchorScore=0;for(int32 V:R.OpResults)T.AnchorScore+=V==1?100:0;
            T.MemberScore=R.Representatives.Num()?FMath::FloorToInt(200.f*Valid/(2.f*Reps)):0;
        }
        else
        {
            // Simulated rivals: deterministic per contest, labelled as local data in the UI.
            T.TaskScore=100*(3+Rand.RandRange(0,3));T.AnchorScore=100*Rand.RandRange(0,2);T.MemberScore=20*Rand.RandRange(4,10);
        }
        T.Total=T.TaskScore+T.AnchorScore+T.MemberScore;
    }
    TArray<int32> Distinct;for(const auto& T:R.Teams)Distinct.AddUnique(T.Total);
    Distinct.Sort([](int32 A,int32 B){return A>B;});
    static const int32 Minutes[]={30,20,10};
    for(FXianTuRaceTeam& T:R.Teams)
    {
        const int32 Group=Distinct.IndexOfByKey(T.Total);T.Rank=Group+1;
        T.LeaseMinutes=T.Total>=600?Minutes[FMath::Min(Group,2)]:0;
    }
    R.Receipt=NextId(TEXT("RACE"));R.State=EXianTuRaceState::Settled;R.StateElapsed=0;
    const FXianTuRaceTeam* Self=R.Teams.FindByPredicate([](const FXianTuRaceTeam& T){return T.bSelf;});
    const FNode* N=Node(R.Node);
    if(Self&&Self->LeaseMinutes>0&&N)
    {
        // The lease activates at settlement on its own clock; claiming only acknowledges it.
        FXianTuLease L;L.Id=NextId(TEXT("LEASE"));L.Node=R.Node;L.Minutes=Self->LeaseMinutes;L.Start=State->SimNow;L.End=State->SimNow+Self->LeaseMinutes*60.0;
        L.State=EXianTuLeaseState::Active;L.Rank=Self->Rank;L.Score=Self->Total;L.Receipt=R.Receipt;State->Leases.Insert(L,0);
        if(State->Leases.Num()>20)State->Leases.SetNum(20);
        Record(TEXT("竞采"),FString::Printf(TEXT("%s 第%d名 %d分 · 获得 %d 分钟开采权"),*N->Name.ToString(),Self->Rank,Self->Total,Self->LeaseMinutes));
        Notify(FText::Format(LOCTEXT("RaceWin","竞采结算 · 第{0}名 {1} 分 · {2} 开采权 {3} 分钟已生效"),FText::AsNumber(Self->Rank),FText::AsNumber(Self->Total),N->Name,FText::AsNumber(Self->LeaseMinutes)),1);
    }
    else if(Self)
    {
        Record(TEXT("竞采"),FString::Printf(TEXT("%s 第%d名 %d分 · 未达600分开采资格"),N?*N->Name.ToString():TEXT(""),Self->Rank,Self->Total));
        Notify(FText::Format(LOCTEXT("RaceLose","竞采结算 · {0} 分，未达 600 分开采资格（无惩罚，可回到历练）"),FText::AsNumber(Self->Total)),2);
    }
    Touch();
}

void UXianTuSubsystem::TickLeases(double Dt)
{
    const bool bOnline=InGameplay();
    UCultivationSubsystem* C=Cultivation();
    for(FXianTuLease& L:State->Leases)
    {
        if(L.State!=EXianTuLeaseState::Active)continue;
        if(State->SimNow>=L.End)
        {
            L.State=EXianTuLeaseState::Expired;
            Notify(FText::Format(LOCTEXT("LeaseEnd","{0} 开采权已到期"),Node(L.Node)?Node(L.Node)->Name:FText::GetEmpty()));Touch();continue;
        }
        // Only time the anchor is actually in valid gameplay produces anything.
        if(!bOnline)continue;
        L.OnlineSeconds+=Dt;
        const FNode* N=Node(L.Node);if(!N)continue;
        if(N->QiBonus>0&&C)
        {
            const FCultivationParticipantSnapshot A=C->GetParticipantSnapshot(C->GetAnchorId());
            const double Gain=A.EffectiveCultivationPerSecond*N->QiBonus*Dt;
            if(Gain>0&&C->AddCultivation(C->GetAnchorId(),Gain))L.QiGranted+=Gain;
            for(const auto& M:State->Guild.Members)if(M.bFormal&&M.bPresent)
            {
                const FCultivationParticipantSnapshot S=C->GetParticipantSnapshot(M.Id);
                if(!S.ParticipantId.IsEmpty())C->AddCultivation(M.Id,S.EffectiveCultivationPerSecond*N->QiBonus*Dt);
            }
        }
        else if(N->Interval>0)
        {
            const int32 Due=int32(L.OnlineSeconds/N->Interval)*N->Count;
            if(Due>L.ItemsGranted)
            {
                FXianTuReward R;R.Item=N->Item;R.Count=Due-L.ItemsGranted;L.ItemsGranted=Due;Grant({R});
                Notify(FText::Format(LOCTEXT("LeaseYield","{0} 产出 {1} ×{2}"),N->Name,ItemName(R.Item),FText::AsNumber(R.Count)),1);
            }
        }
    }
}

// ------------------------------------------------------------------ guild

void UXianTuSubsystem::SyncGuild()
{
    auto& G=State->Guild;
    TSet<FString> Present;
    if(const ALiveWeaponSwarm* S=Swarm())
        for(const FLiveWeaponState& P:S->GetRoster().GetStates())
        {
            if(!P.bParticipating||P.bPersonalSword)continue;
            Present.Add(P.Profile.ParticipantId);
            if(!G.bCreated)continue;
            FXianTuMember* M=G.Members.FindByPredicate([&](const FXianTuMember& X){return X.Id==P.Profile.ParticipantId;});
            if(!M)
            {
                // Joining the room with 加入 makes a viewer a formal member of the anchor's guild.
                FXianTuMember New;New.Id=P.Profile.ParticipantId;New.Name=P.Profile.DisplayName;New.bFormal=true;New.JoinedUtc=State->SimNow;New.DailyKey=G.DailyKey;
                G.Members.Add(New);Record(TEXT("成员"),New.Name+TEXT(" 加入武灵"));Touch();
            }
            else M->Name=P.Profile.DisplayName;
        }
    int32 Changed=0;
    for(auto& M:G.Members){const bool bNow=Present.Contains(M.Id);if(bNow!=M.bPresent){M.bPresent=bNow;++Changed;}}
    if(Changed)++Revision;
}

void UXianTuSubsystem::TickGuildReview()
{
    auto& G=State->Guild;
    if(!G.bCreated||G.PendingName.IsEmpty()||State->SimNow-G.PendingSince<5.0)return;
    // Local review stand-in: obvious impersonation words are rejected, everything else passes.
    static const TCHAR* Banned[]={TEXT("官方"),TEXT("管理"),TEXT("客服"),TEXT("抖音"),TEXT("系统"),TEXT("GM")};
    FString Reason;for(const TCHAR* Word:Banned)if(G.PendingName.Contains(Word))Reason=FString::Printf(TEXT("名称包含“%s”，审核未通过，可修改后重新提交"),Word);
    if(Reason.IsEmpty())
    {
        const bool bFirst=G.Name.IsEmpty();
        G.Name=G.PendingName;G.RejectReason.Empty();
        Record(TEXT("名称"),bFirst?TEXT("武灵名称审核通过：")+G.Name:TEXT("改名审核通过：")+G.Name);
        Notify(FText::Format(LOCTEXT("NameOk","武灵「{0}」名称审核通过"),FText::FromString(G.Name)),1);
    }
    else{G.RejectReason=Reason;Record(TEXT("名称"),Reason);Notify(FText::FromString(Reason),2);}
    G.PendingName.Empty();Touch();
}

bool UXianTuSubsystem::CreateGuild(const FString& InName,FText& Out)
{
    auto& G=State->Guild;const FString Name=InName.TrimStartAndEnd();
    const float Length=NameLength(Name);
    if(Length<2||Length>12){Out=LOCTEXT("NameLength","名称需为2至12个汉字等效长度");return false;}
    if(G.bCreated&&!G.PendingName.IsEmpty()){Out=LOCTEXT("Reviewing","名称审核中，请稍候");return false;}
    if(!G.bCreated)
    {
        G=FXianTuGuild();G.bCreated=true;G.Id=NextId(TEXT("GUILD"));G.CreatedUtc=State->SimNow;G.DailyKey=DayKey(UCultivationSubsystem::UtcNow());
        G.PendingName=Name;G.PendingSince=State->SimNow;
        Record(TEXT("创建"),TEXT("免费创建武灵，名称提交审核：")+Name);
        Out=LOCTEXT("Created","武灵已创建 · 名称审核中（本地审核约 5 秒）");
        SyncGuild();Touch();return true;
    }
    if(!G.Name.IsEmpty()&&G.LastRenameUtc>0&&State->SimNow-G.LastRenameUtc<7*86400.0)
    {
        Out=FText::Format(LOCTEXT("RenameWait","每 7 日可免费改名一次，还需 {0}"),Clock(7*86400.0-(State->SimNow-G.LastRenameUtc)));return false;
    }
    if(!G.Name.IsEmpty())G.LastRenameUtc=State->SimNow;
    G.PendingName=Name;G.PendingSince=State->SimNow;G.RejectReason.Empty();
    Record(TEXT("名称"),TEXT("提交名称审核：")+Name);
    Out=LOCTEXT("Renamed","名称已提交审核，通过前保留原名");Touch();return true;
}

// ------------------------------------------------------------------ tribulation

int32 UXianTuSubsystem::TribulationFailures()const{return State->Trib.Failures.FindRef(FString::FromInt(AnchorRealm()));}

int32 UXianTuSubsystem::TribulationProbability(int32 Assists)const
{
    const int32 Fails=State->Trib.State==EXianTuTribState::None||State->Trib.FromRealm!=AnchorRealm()?TribulationFailures():State->Trib.Failures.FindRef(FString::FromInt(State->Trib.FromRealm));
    return FMath::Clamp(5000+FMath::Min(Assists,50)*100+FMath::Min(Fails*1000,5000)-FortunePenalty(),0,10000);
}

bool UXianTuSubsystem::CanStartTribulation(FText* Reason)const
{
    auto Fail=[&](FText T){if(Reason)*Reason=T;return false;};
    const UCultivationSubsystem* C=Cultivation();if(!C)return Fail(LOCTEXT("NoLedger","修为账本未就绪"));
    const FCultivationParticipantSnapshot A=C->GetParticipantSnapshot(C->GetAnchorId());
    if(A.ParticipantId.IsEmpty())return Fail(LOCTEXT("NoAnchor","进入游戏后才能渡劫"));
    if(!IsTribulation(A.RealmIndex))return Fail(LOCTEXT("NoTrib","当前境界突破无需渡劫（金丹六阶冲击元婴起才有天劫）"));
    if(A.Cultivation<A.RequiredCultivation)return Fail(FText::Format(LOCTEXT("NeedQi","修为未满：{0}/{1}"),FText::AsNumber(FMath::FloorToInt(A.Cultivation)),FText::AsNumber(FMath::FloorToInt(A.RequiredCultivation))));
    // v1.4: a major stage pays with its own core (and the aux laid out on the breakthrough screen), not with 破境石.
    if(const XianTuCatalog::FStage* Stage=XianTuCatalog::StageFrom(A.RealmIndex))
    {
        int32 Cores=0;for(int32 G=1;G<=4;++G)Cores+=ItemCount(XianTuCatalog::MaterialKey(*Stage,XianTuCatalog::ECategory::Core,G));
        if(Cores<=0)return Fail(FText::Format(LOCTEXT("NeedCore","缺少核心材料：{0}（{1}·{2}）"),Stage->Species[0].Name,Stage->Region,Stage->Subarea));
        FText Why;if(State->Plan.Stage==Stage->Id&&State->Plan.Core>0&&!PlanAvailable(&Why))return Fail(Why);
    }
    else if(A.BreakthroughMaterialCount<A.BreakthroughMaterialCost)return Fail(FText::Format(LOCTEXT("NeedStone","破境石不足：{0}/{1}"),FText::AsNumber(A.BreakthroughMaterialCount),FText::AsNumber(A.BreakthroughMaterialCost)));
    const UCultivationHealthComponent* H=C->GetAnchorHealth();
    if(!H||H->bDead||H->GetHealthFraction()<.5)return Fail(LOCTEXT("NeedHealth","生命需不低于50%"));
    FText Busy;if(IsBusy(&Busy))return Fail(Busy);
    if(DaoHeartRemaining()>0)return Fail(FText::Format(LOCTEXT("DaoHeartWait","道心破碎，静养 {0} 后方可再渡天劫"),Clock(DaoHeartRemaining())));
    if(State->Trib.LastSettled>0&&State->SimNow-State->Trib.LastSettled<60)return Fail(FText::Format(LOCTEXT("TribWait","距上次渡劫结算需满60秒，还需 {0}"),Clock(60-(State->SimNow-State->Trib.LastSettled))));
    return true;
}

bool UXianTuSubsystem::StartTribulation(FText& Out)
{
    if(!CanStartTribulation(&Out))return false;
    NormalizePlan();EnsurePlanCore();
    if(!PlanAvailable(&Out))return false;
    FXianTuTribulation& T=State->Trib;
    const TMap<FString,int32> Failures=T.Failures;const double Last=T.LastSettled,Heart=T.DaoHeartUntil;
    T=FXianTuTribulation();T.Failures=Failures;T.LastSettled=Last;T.DaoHeartUntil=Heart;
    T.State=EXianTuTribState::Prepare;T.FromRealm=AnchorRealm();T.AttemptId=NextId(TEXT("TRIB"));T.Probability=TribulationProbability(0);
    Record(TEXT("天劫"),FString::Printf(TEXT("%s → %s 渡劫准备"),*AnchorRealmName().ToString(),*AnchorRealmName(1).ToString()));
    Out=LOCTEXT("TribPrepare","准备 15 秒 · 修为与材料已预留，开始前可无损取消");Touch();return true;
}

void UXianTuSubsystem::TickTribulation(double Dt)
{
    FXianTuTribulation& T=State->Trib;
    switch(T.State)
    {
    case EXianTuTribState::Prepare:
        T.Elapsed+=Dt;
        if(T.Elapsed>=TribPrepare)
        {
            const UCultivationSubsystem* C=Cultivation();const FCultivationParticipantSnapshot A=C?C->GetParticipantSnapshot(C->GetAnchorId()):FCultivationParticipantSnapshot();
            const bool bMaterials=XianTuCatalog::StageFrom(T.FromRealm)?PlanAvailable(nullptr):A.BreakthroughMaterialCount>=A.BreakthroughMaterialCost;
            if(!C||A.RealmIndex!=T.FromRealm||A.Cultivation<A.RequiredCultivation||!bMaterials)
            {T.State=EXianTuTribState::Cancelled;Notify(LOCTEXT("TribVoid","预留失效，本次渡劫取消（未扣任何资源）"),2);Touch();break;}
            T.State=EXianTuTribState::Trial;T.Elapsed=0;
            Notify(LOCTEXT("TribTrial","三重天劫降临 · 60 秒内观众发送“助力”可提升成功率"),1);Touch();
        }
        break;
    case EXianTuTribState::Trial:
        T.Elapsed+=Dt;T.Probability=TribulationProbability(T.Assists.Num());
        if(T.Elapsed>=TribTrial){T.State=EXianTuTribState::Frozen;T.Elapsed=0;Touch();ResolveTribulation();}
        break;
    case EXianTuTribState::Backlash:
    {
        // The storm lands the bolts on this clock; whatever fells the anchor (bolt or monster) settles it as a fall.
        T.Elapsed+=Dt;
        UpdateStarWard();
        const UCultivationHealthComponent* H=BacklashHealth.Get();
        if(H&&H->bDead)SettleBacklash(true);
        else if(T.Elapsed>=TribBacklash)SettleBacklash(false);
        break;
    }
    case EXianTuTribState::Succeeded:case EXianTuTribState::Failed:
        T.RevealRemaining=FMath::Max(0.0,T.RevealRemaining-Dt);
        break;
    default:break;
    }
}

void UXianTuSubsystem::ResolveTribulation(bool bBacklash)
{
    FXianTuTribulation& T=State->Trib;UCultivationSubsystem* C=Cultivation();
    const FString Key=FString::FromInt(T.FromRealm);
    // One persisted uniform roll in [0,9999] against the frozen basis-point probability.
    T.Probability=FMath::Clamp(5000+FMath::Min(T.Assists.Num(),50)*100+FMath::Min(T.Failures.FindRef(Key)*1000,5000)-FortunePenalty(),0,10000);
    T.Roll=ForcedRoll>=0?ForcedRoll:FMath::RandRange(0,9999);T.bSuccess=T.Roll<T.Probability;T.Receipt=NextId(TEXT("TR"));
    const FText From=C?C->GetRealmName(T.FromRealm):FText::GetEmpty(),To=C?C->GetRealmName(T.FromRealm+1):FText::GetEmpty();
    if(T.bSuccess)
    {
        // A major stage commits its laid-out materials only now: a failed roll keeps every core and aux.
        const XianTuCatalog::FStage* Stage=XianTuCatalog::StageFrom(T.FromRealm);
        const int32 Quality=Stage?PlanQuality():0;
        if(C&&C->GetParticipantSnapshot(C->GetAnchorId()).RealmIndex==T.FromRealm&&(!Stage||PlanAvailable(nullptr))&&C->TryAnchorBreakthroughEx(Stage==nullptr))
        {
            T.State=EXianTuTribState::Succeeded;T.Failures.Remove(Key);
            int32 Soul=0;FText Band=FText::GetEmpty();
            if(Stage){ConsumePlan();Soul=RecordBreak(*Stage,Quality,T.FromRealm);Band=XianTuCatalog::Bands()[XianTuCatalog::BandIndex(Quality)].Name;}
            Record(TEXT("天劫"),FString::Printf(TEXT("渡劫成功 %s → %s · 概率 %.0f%% · 品质 %s Q%d · 收据 %s"),*From.ToString(),*To.ToString(),T.Probability/100.f,*Band.ToString(),Quality,*T.Receipt));
            TArray<FXianTuReward> Shown;if(Soul>0){FXianTuReward R;R.Item=TEXT("CUR_SOUL");R.Count=Soul;Shown.Add(R);}
            Notify(Stage?FText::Format(LOCTEXT("TribOkQ","渡劫成功 · 晋升 {0} · 突破品质 {1}（Q{2}）"),To,Band,FText::AsNumber(Quality)):FText::Format(LOCTEXT("TribOk","渡劫成功 · 晋升 {0}"),To),1,LOCTEXT("TribOkTitle","渡劫成功"),Shown);
        }
        else
        {
            T.State=EXianTuTribState::Cancelled;
            Notify(LOCTEXT("TribReconcile","渡劫对账：突破条件已变化，本次作废，未扣修为"),2);
        }
    }
    else
    {
        // The heavens do not let go: ten seconds of ever harder lightning decide what the failure costs.
        UCultivationHealthComponent* H=C?C->GetAnchorHealth():nullptr;
        ReleaseAssistLeases();
        if(!bBacklash||!H||H->bDead){SettleBacklash(H&&H->bDead);return;}
        T.State=EXianTuTribState::Backlash;T.Elapsed=0;T.BacklashStrikes=0;T.BacklashDamage=0;
        T.Ward=EXianTuWard::None;T.WardRealm=-1;T.WardChance=0;T.WardRoll=-1;
        BacklashHealth=H;H->OnDied.AddUniqueDynamic(this,&UXianTuSubsystem::HandleAnchorDied);
        Notify(LOCTEXT("TribBacklash","渡劫失败 · 天雷反噬十息！倒下则损失修为，扛住则道心破碎"),2,LOCTEXT("TribBacklashTitle","天罚降临"));
        // Decided before the storm's first bolt, which lands on its next tick.
        UpdateStarWard();
        Touch();return;
    }
    T.LastSettled=State->SimNow;T.RevealRemaining=TribReveal;
    ReleaseAssistLeases();Touch();
}

void UXianTuSubsystem::SettleBacklash(bool bDied)
{
    FXianTuTribulation& T=State->Trib;UCultivationSubsystem* C=Cultivation();
    if(UCultivationHealthComponent* H=BacklashHealth.Get())H->OnDied.RemoveDynamic(this,&UXianTuSubsystem::HandleAnchorDied);
    BacklashHealth.Reset();
    const FString Key=FString::FromInt(T.FromRealm);
    T.Failures.FindOrAdd(Key)+=1;T.State=EXianTuTribState::Failed;T.bDied=bDied;T.bDaoHeartBroken=!bDied;
    const int32 Pity=FMath::Min(T.Failures.FindRef(Key)*10,50);
    if(bDied)
    {
        T.Loss=C?C->ApplyCultivationLoss(C->GetAnchorId(),1500):0;
        Record(TEXT("天劫"),FString::Printf(TEXT("渡劫失败 · 陨落于第 %d 道天罚%s · 损失修为 %.0f · 材料保留 · 下次保底 +%d%%"),T.BacklashStrikes,*WardNote(),T.Loss,Pity));
        Notify(FText::Format(LOCTEXT("TribDied","渡劫失败 · 主播陨落于天罚之下，损失 {0} 修为；材料保留、不掉境界，下次保底 +10%"),FText::AsNumber(FMath::RoundToInt(T.Loss))),2,LOCTEXT("TribDiedTitle","陨落"));
    }
    else
    {
        T.Loss=0;T.DaoHeartUntil=State->SimNow+DaoHeartDuration;
        Record(TEXT("天劫"),FString::Printf(TEXT("渡劫失败 · 扛过 %d 道天罚%s · 道心破碎 %d 分钟 · 修为与材料保留 · 下次保底 +%d%%"),T.BacklashStrikes,*WardNote(),int32(DaoHeartDuration/60),Pity));
        Notify(FText::Format(LOCTEXT("TribHeart","渡劫失败 · 扛过天罚，修为保留，但道心破碎：防御 −{0}%，{1} 分钟内无法再渡劫"),
            FText::AsNumber(FMath::RoundToInt(DaoHeartDefense*100)),FText::AsNumber(int32(DaoHeartDuration/60))),2,LOCTEXT("TribHeartTitle","道心破碎"));
        SyncDaoHeart();
    }
    T.LastSettled=State->SimNow;T.RevealRemaining=TribReveal;Touch();
}

void UXianTuSubsystem::HandleAnchorDied(){if(State->Trib.State==EXianTuTribState::Backlash)SettleBacklash(true);}

int32 UXianTuSubsystem::StarWardRealm()const
{
    const APawn* Pawn=AnchorPawn();
    const ULiveSpecialSwordComponent* Manager=Pawn?ULiveSpecialSwordComponent::Find(Pawn):nullptr;if(!Manager)return -1;
    TArray<ALiveSpecialSword*> Swords;Manager->GetSwords(Swords);
    int32 Best=-1;
    for(const ALiveSpecialSword* Sword:Swords)if(Sword&&Sword->GetKind()==ELiveSpecialSword::SevenStar)Best=FMath::Max(Best,Sword->GetRealm());
    return Best;
}

void UXianTuSubsystem::UpdateStarWard()
{
    FXianTuTribulation& T=State->Trib;
    if(T.State!=EXianTuTribState::Backlash||T.Ward!=EXianTuWard::None)return;
    const int32 Realm=StarWardRealm();if(Realm<0)return;
    // One roll for the whole backlash: the guard either holds the heavens off or is broken for good.
    T.WardRealm=Realm;T.WardChance=StarWardChance(Realm,T.FromRealm);
    T.WardRoll=ForcedWardRoll>=0?ForcedWardRoll:FMath::RandRange(0,9999);
    T.Ward=T.WardRoll<T.WardChance?EXianTuWard::Held:EXianTuWard::Broken;
    const UCultivationSubsystem* C=Cultivation();
    const FText Sword=C?C->GetRealmName(Realm):FText::GetEmpty(),Heaven=C?C->GetRealmName(T.FromRealm+1):FText::GetEmpty();
    const FText Chance=FText::FromString(FString::Printf(TEXT("%g%%"),T.WardChance/100.0));
    if(T.Ward==EXianTuWard::Held)
        Notify(FText::Format(LOCTEXT("WardHeld","七星挡劫 · {0}剑灵挡下{1}天罚（挡劫 {2}），天雷不致陨落"),Sword,Heaven,Chance),1,LOCTEXT("WardHeldTitle","七星挡劫"));
    else
        Notify(FText::Format(LOCTEXT("WardBroken","天雷破盾 · {0}剑灵挡不住{1}天罚（挡劫 {2}），七星护持对天雷无效"),Sword,Heaven,Chance),2,LOCTEXT("WardBrokenTitle","天雷破盾"));
    Touch();
}

FString UXianTuSubsystem::WardNote()const
{
    const FXianTuTribulation& T=State->Trib;if(T.Ward==EXianTuWard::None)return FString();
    const UCultivationSubsystem* C=Cultivation();
    const FString Sword=C?C->GetRealmName(T.WardRealm).ToString():FString();
    return FString::Printf(TEXT(" · %s（%s剑灵 · 挡劫 %g%%）"),T.Ward==EXianTuWard::Held?TEXT("七星挡劫"):TEXT("天雷破盾"),*Sword,T.WardChance/100.0);
}

void UXianTuSubsystem::NoteBacklashStrike(double Damage)
{
    FXianTuTribulation& T=State->Trib;if(T.State!=EXianTuTribState::Backlash)return;
    ++T.BacklashStrikes;T.BacklashDamage+=FMath::Max(0.0,Damage);
}

void UXianTuSubsystem::SyncDaoHeart()
{
    const UCultivationSubsystem* C=Cultivation();const UCultivationHealthComponent* H=C?C->GetAnchorHealth():nullptr;
    ULiveAnchorBuffSubsystem* Buffs=H?ULiveAnchorBuffSubsystem::Get(H):nullptr;if(!Buffs)return;
    const double Left=DaoHeartRemaining();const bool bHas=Buffs->GetStacks(TEXT("DaoHeart"),TEXT("tribulation"))>0;
    // The buff counts real seconds; the heart heals on the sim clock, so the remaining time is converted at the current scale.
    if(Left>0&&!bHas&&!H->bDead)Buffs->Apply(ULiveAnchorBuffSubsystem::MakeDaoHeart(DaoHeartDefense,float(Left/FMath::Clamp(State->TimeScale,.1f,60.f))));
    else if(Left<=0&&bHas)Buffs->Remove(TEXT("DaoHeart"),TEXT("tribulation"));
}

void UXianTuSubsystem::UpdateStorm()
{
    const EXianTuTribState S=State->Trib.State;
    if(Storm.IsValid()||(S!=EXianTuTribState::Prepare&&S!=EXianTuTribState::Trial&&S!=EXianTuTribState::Frozen&&S!=EXianTuTribState::Backlash))return;
    APawn* Pawn=AnchorPawn();UWorld* World=Pawn?Pawn->GetWorld():nullptr;if(!World||!World->IsGameWorld())return;
    FActorSpawnParameters Params;Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    if(AXianTuTribulationStorm* NewStorm=World->SpawnActor<AXianTuTribulationStorm>(AXianTuTribulationStorm::StaticClass(),FTransform(Pawn->GetActorLocation()),Params))
    {NewStorm->Bind(this);Storm=NewStorm;}
}

void UXianTuSubsystem::SyncTribulationMeditation()
{
    const EXianTuTribState S=State->Trib.State;
    const UCultivationSubsystem* C=Cultivation();const UCultivationHealthComponent* H=C?C->GetAnchorHealth():nullptr;
    const bool bOn=(S==EXianTuTribState::Prepare||S==EXianTuTribState::Trial||S==EXianTuTribState::Frozen||S==EXianTuTribState::Backlash)&&H&&!H->bDead;
    APawn* Pawn=AnchorPawn();
    UCultivationGameplayComponent* Action=Pawn?Pawn->FindComponentByClass<UCultivationGameplayComponent>():nullptr;
    if(Action&&Action->IsTribulationMeditation()!=bOn)Action->SetTribulationMeditation(bOn);
}

void UXianTuSubsystem::ReleaseAssistLeases()
{
    if(ALiveWeaponSwarm* S=Swarm())for(const FString& Id:State->Trib.Assists)S->ReleaseAlertLease(Id,State->Trib.AttemptId);
}

// ------------------------------------------------------------------ market

void UXianTuSubsystem::RefillBoard()
{
    int32 Listed=0;for(const auto& O:State->Orders)if(!O.bMine&&O.State==EXianTuOrderState::Listed)++Listed;
    while(Listed<6)
    {
        FXianTuOrder O;O.Id=NextId(TEXT("MKT"));O.Item=FMath::RandBool()?FName(TEXT("MAT_HERB_T")):FName(TEXT("MAT_IRON_T"));
        int32 Min,Max,Recycle;MarketRange(O.Item,Min,Max,Recycle);
        O.Count=FMath::RandRange(5,30);O.UnitPrice=FMath::RandRange(Min,Max);O.Listed=State->SimNow;O.Expires=State->SimNow+FMath::RandRange(6,24)*3600.0;
        O.Seller=SellerNames[FMath::RandRange(0,UE_ARRAY_COUNT(SellerNames)-1)];State->Orders.Add(O);++Listed;
    }
    State->Orders.RemoveAll([](const FXianTuOrder& O){return !O.bMine&&O.State!=EXianTuOrderState::Listed;});
}

void UXianTuSubsystem::TickMarket(double Dt)
{
    bool bChanged=false;
    for(FXianTuOrder& O:State->Orders)
    {
        if(O.State!=EXianTuOrderState::Listed||State->SimNow<O.Expires)continue;
        O.State=EXianTuOrderState::Expired;bChanged=true;
        if(O.bMine){State->Items.FindOrAdd(O.Item)+=O.Count;Notify(FText::Format(LOCTEXT("OrderExpired","挂单过期 · {0} ×{1} 已退回"),ItemName(O.Item),FText::AsNumber(O.Count)));}
    }
    if(State->SimNow>=State->NextMarketTick)
    {
        State->NextMarketTick=State->SimNow+60;
        for(FXianTuOrder& O:State->Orders)
        {
            if(!O.bMine||O.State!=EXianTuOrderState::Listed)continue;
            int32 Min,Max,Recycle;MarketRange(O.Item,Min,Max,Recycle);
            const float Chance=.08f+.3f*float(Max-O.UnitPrice)/FMath::Max(1,Max-Min);
            if(FMath::FRand()>=Chance)continue;
            // Simulated buyer: whole order, tax ceil(5%) from the seller, listing fee already burnt.
            const int32 Total=O.Count*O.UnitPrice;O.Tax=FMath::CeilToInt(Total*.05f);O.Income=Total-O.Tax;
            State->Stones+=O.Income;O.State=EXianTuOrderState::Sold;bChanged=true;
            Record(TEXT("坊市"),FString::Printf(TEXT("寄售成交 %s ×%d · 总价 %d · 税 %d · 到手 %d"),*ItemName(O.Item).ToString(),O.Count,Total,O.Tax,O.Income));
            Notify(FText::Format(LOCTEXT("Sold","寄售成交 · {0} ×{1} · 到手 {2} 灵石（本地模拟买家）"),ItemName(O.Item),FText::AsNumber(O.Count),FText::AsNumber(O.Income)),1);
        }
        for(FXianTuOrder& O:State->Orders)if(!O.bMine&&O.State==EXianTuOrderState::Listed&&FMath::FRand()<.1f){O.State=EXianTuOrderState::Sold;bChanged=true;}
    }
    const int32 Before=State->Orders.Num();RefillBoard();
    if(bChanged||Before!=State->Orders.Num())Touch();
}

// ------------------------------------------------------------------ methods

float UXianTuSubsystem::ModifyPersonalHit(ALiveMonster* Target,float Damage,const FString& OwnerId)
{
    const UWorld* World=Target?Target->GetWorld():nullptr;if(!World||!FMath::IsFinite(Damage))return Damage;
    const FXianTuMethods& M=State->Methods;
    const double Now=World->GetTimeSeconds();
    const bool bCharged=Now-LastPersonalHit>=3.0;LastPersonalHit=Now;++PersonalHits;
    float Out=Damage;
    if(M.Main==TEXT("METHOD_EDGE")&&PersonalHits%5==0)
    {
        Out*=1.8f*LevelScale(M.Levels.FindRef(TEXT("METHOD_EDGE")));
        LiveSwordVfx::SpawnAt(Target,LiveSwordVfx::EFx::Slash,Target->GetActorLocation(),FLinearColor(1.f,.8f,.3f)*2.f,150.f);
    }
    if(M.Support.Contains(FName(TEXT("METHOD_EDGE")))&&bCharged)Out*=1.1f;
    auto Echo=[&](float Amount)
    {
        FDelayed D;D.Target=Target;D.Amount=Amount;D.At=Now+.6;D.Owner=OwnerId;Delayed.Add(D);
    };
    if(M.Main==TEXT("METHOD_ECHO")&&PersonalHits%4==0)Echo(Out*.5f*LevelScale(M.Levels.FindRef(TEXT("METHOD_ECHO"))));
    if(M.Support.Contains(FName(TEXT("METHOD_ECHO")))&&Now>=EchoSupportReady){EchoSupportReady=Now+20;Echo(Out*.1f);}
    return Out;
}

void UXianTuSubsystem::TickMethods(float RealDelta)
{
    APawn* Pawn=AnchorPawn();const UWorld* World=Pawn?Pawn->GetWorld():nullptr;
    if(!World){Delayed.Reset();return;}
    const double Now=World->GetTimeSeconds();
    for(int32 I=Delayed.Num()-1;I>=0;--I)
    {
        if(Delayed[I].At>Now)continue;
        const FDelayed D=Delayed[I];Delayed.RemoveAt(I);
        // Echo damage lands directly and never triggers another echo.
        if(ALiveMonster* T=D.Target.Get();T&&T->IsAlive())
        {
            T->ApplyWeaponDamage(D.Amount,D.Owner);
            LiveSwordVfx::SpawnAt(T,LiveSwordVfx::EFx::Burst,T->GetActorLocation(),FLinearColor(.6f,.4f,1.f)*2.f,70.f);
        }
    }
    if(!InGameplay())return;
    ULiveAnchorBuffSubsystem* Buffs=ULiveAnchorBuffSubsystem::Get(Pawn);if(!Buffs)return;
    const FXianTuMethods& M=State->Methods;
    if(M.Main==TEXT("METHOD_GUARD")&&Now>=GuardReady)
    {
        GuardReady=Now+30;
        Buffs->Apply(ULiveAnchorBuffSubsystem::MakeGuard(TEXT("method"),.2*LevelScale(M.Levels.FindRef(TEXT("METHOD_GUARD"))),6.f));
        LiveSwordVfx::SpawnAt(Pawn,LiveSwordVfx::EFx::Ring,Pawn->GetActorLocation()-FVector(0,0,80),FLinearColor(1.f,.75f,.3f),180.f);
    }
    const UCultivationSubsystem* C=Cultivation();const UCultivationHealthComponent* H=C?C->GetAnchorHealth():nullptr;
    const bool bLow=M.Support.Contains(FName(TEXT("METHOD_GUARD")))&&H&&H->GetHealthFraction()<.5;
    if(bLow&&!Buffs->GetStacks(TEXT("MethodGuardLow"),TEXT("method")))
    {
        FLiveAnchorBuff B;B.Id=TEXT("MethodGuardLow");B.Source=TEXT("method");B.Name=LOCTEXT("GuardLow","玄罡·固本");B.Glyph=TEXT("固");
        B.Description=LOCTEXT("GuardLowDesc","玄罡体经辅修：生命低于50%时防御提高10%。");B.Color=FLinearColor(.95f,.7f,.3f);B.DefensePerStack=.1;Buffs->Apply(B);
    }
    else if(!bLow&&Buffs->GetStacks(TEXT("MethodGuardLow"),TEXT("method")))Buffs->Remove(TEXT("MethodGuardLow"),TEXT("method"));
}

// ------------------------------------------------------------------ local assist & race responses

bool UXianTuSubsystem::AddTribulationAssist(const FString& Id,const FString& Name)
{
    const UCultivationSubsystem* C=Cultivation();
    if(Id.IsEmpty()||(C&&Id==C->GetAnchorId()))return false;
    FXianTuTribulation& T=State->Trib;
    if(T.State==EXianTuTribState::Prepare)
    {
        Notify(FText::Format(LOCTEXT("AssistEarly","{0}：天劫尚未开始，稍后再助力"),FText::FromString(Name)));
        return false;
    }
    if(T.State!=EXianTuTribState::Trial||T.Assists.Contains(Id))return false;
    T.Assists.Add(Id);T.AssistNames.Add(Name);
    T.Probability=TribulationProbability(T.Assists.Num());
    if(ALiveWeaponSwarm* S=Swarm())S->AcquireAlertLease(Id,T.AttemptId,UCultivationSubsystem::UtcNow()+(TribTrial-T.Elapsed)/FMath::Max(.1f,State->TimeScale)+1.0,1);
    Notify(T.Assists.Num()>50?FText::Format(LOCTEXT("AssistCap","{0} 助力已收到，概率已满"),FText::FromString(Name))
        :FText::Format(LOCTEXT("Assist","{0} 助力 · 成功率 {1}%"),FText::FromString(Name),FText::AsNumber(T.Probability/100)));
    Touch();
    return true;
}

bool UXianTuSubsystem::RecordRaceResponse(const FString& Id)
{
    FXianTuRace& R=State->Race;int32 Window=0;double Remaining=0;
    if(R.State!=EXianTuRaceState::Running||!R.Representatives.Contains(Id)||!ResponseWindow(Window,Remaining))return false;
    int32& Mask=R.Responses.FindOrAdd(Id);
    if(Mask&(1<<Window))return false;
    Mask|=1<<Window;Touch();
    return true;
}

// ------------------------------------------------------------------ commands

bool UXianTuSubsystem::Execute(const FString& Verb,const TArray<FString>& Args,FText& Out)
{
    auto Arg=[&](int32 I){return Args.IsValidIndex(I)?Args[I]:FString();};
    auto Int=[&](int32 I){return FCString::Atoi(*Arg(I));};
    if(Verb==TEXT("Scale")){State->TimeScale=FMath::Clamp(FCString::Atof(*Arg(0)),.1f,60.f);Out=FText::Format(LOCTEXT("Scale","仙途本地时间流速 ×{0}"),FText::AsNumber(State->TimeScale));Touch();return true;}
    // World trial
    if(Verb==TEXT("WorldStart"))return StartWorld(FName(*Arg(0)),FName(*Arg(1)),FName(*Arg(2)),Out);
    if(Verb==TEXT("WorldAbandon"))
    {
        const auto S=State->World.State;
        if(S!=EXianTuWorldState::Preparing&&S!=EXianTuWorldState::Running&&S!=EXianTuWorldState::Interrupted){Out=LOCTEXT("NoWorld","当前没有进行中的历练");return false;}
        SettleWorld();Out=LOCTEXT("Abandoned","已提前结束历练，按已完成阶段结算");return true;
    }
    if(Verb==TEXT("WorldAck")){if(State->World.State==EXianTuWorldState::Completed){State->World.State=EXianTuWorldState::Idle;Touch();}return true;}
    // Contest
    if(Verb==TEXT("RaceQueue"))return QueueRace(FName(*Arg(0)),Out);
    if(Verb==TEXT("RaceCancel"))
    {
        FXianTuRace& R=State->Race;
        if(R.State==EXianTuRaceState::Queue||R.State==EXianTuRaceState::Matched||R.State==EXianTuRaceState::Ready)
        {R=FXianTuRace();R.State=EXianTuRaceState::Cancelled;Out=LOCTEXT("RaceCancelled","已取消报名，无任何费用");Touch();return true;}
        if(R.State==EXianTuRaceState::Running){SettleRace();Out=LOCTEXT("RaceQuit","已退出竞采，按当前得分结算");return true;}
        Out=LOCTEXT("NoRace","当前没有可取消的竞采");return false;
    }
    if(Verb==TEXT("RaceOp"))
    {
        int32 K=0;double Remaining=0;
        if(!OpWindow(K,Remaining)){Out=LOCTEXT("NoOp","当前不在时机窗口，请等待预告");return false;}
        State->Race.OpResults[K]=1;Out=LOCTEXT("OpOk","时机把握 · 主播操作 +100");Touch();return true;
    }
    if(Verb==TEXT("RaceAck")){const auto S=State->Race.State;if(S==EXianTuRaceState::Settled||S==EXianTuRaceState::Cancelled){State->Race=FXianTuRace();Touch();}return true;}
    // Guild
    if(Verb==TEXT("GuildCreate"))return CreateGuild(Arg(0),Out);
    if(Verb==TEXT("GuildBuild"))
    {
        auto& G=State->Guild;const FName Id(*Arg(0));
        const FBuilding* B=Buildings().FindByPredicate([&](const FBuilding& X){return X.Id==Id;});
        if(!G.bCreated||!B){Out=LOCTEXT("NoBuild","需先创建武灵");return false;}
        if(G.Buildings.Contains(Id)){Out=LOCTEXT("Built","该建设已完成");return false;}
        if(G.Materials<B->Cost){Out=FText::Format(LOCTEXT("NeedMat","公共资材不足：{0}/{1}"),FText::AsNumber(G.Materials),FText::AsNumber(B->Cost));return false;}
        G.Materials-=B->Cost;G.Buildings.Add(Id);Record(TEXT("建设"),FString::Printf(TEXT("建成 %s · 支出公共资材 %d"),*B->Name.ToString(),B->Cost));
        Out=FText::Format(LOCTEXT("BuildOk","{0} 建成，山门外观已更新"),B->Name);Touch();return true;
    }
    if(Verb==TEXT("GuildRemove"))
    {
        FXianTuMember* M=State->Guild.Members.FindByPredicate([&](const FXianTuMember& X){return X.Id==Arg(0);});
        if(!M||!M->bFormal){Out=LOCTEXT("NoMember","该成员不在武灵中");return false;}
        M->bFormal=false;M->LeftUtc=State->SimNow;M->CooldownUntil=State->SimNow+86400;
        Record(TEXT("成员"),M->Name+TEXT(" 被移出武灵（个人境界与剑灵权益保留）"));
        Out=FText::Format(LOCTEXT("Removed","已移出 {0}，其个人权益保留"),FText::FromString(M->Name));Touch();return true;
    }
    if(Verb==TEXT("GuildDisband"))
    {
        const auto R=State->Race.State;
        if(R==EXianTuRaceState::Queue||R==EXianTuRaceState::Matched||R==EXianTuRaceState::Ready||R==EXianTuRaceState::Running||R==EXianTuRaceState::Frozen){Out=LOCTEXT("DisbandRace","竞采进行中，不能解散武灵");return false;}
        const FString Name=State->Guild.Name;State->Guild=FXianTuGuild();
        Record(TEXT("武灵"),TEXT("武灵已解散：")+Name+TEXT("（成员个人财产不受影响）"));
        Out=LOCTEXT("Disbanded","武灵已解散，成员个人境界与权益保留");Touch();return true;
    }
    // Methods
    if(Verb.StartsWith(TEXT("Method")))
    {
        const FName Id(*Arg(0));const FMethod* M=Method(Id);
        if(!M){Out=LOCTEXT("NoMethod","未知功法");return false;}
        if(!M->bOpen){Out=LOCTEXT("MethodClosed","该功法首版未开放，后续版本按境界解锁");return false;}
        const auto W=State->World.State;const auto R=State->Race.State;const auto T=State->Trib.State;
        const bool bLocked=W==EXianTuWorldState::Running||W==EXianTuWorldState::Settling||W==EXianTuWorldState::Interrupted||R==EXianTuRaceState::Running||R==EXianTuRaceState::Frozen||T==EXianTuTribState::Trial||T==EXianTuTribState::Backlash;
        FXianTuMethods& Ms=State->Methods;
        if(Verb==TEXT("MethodStudy"))
        {
            const int32 Level=FMath::Max(1,Ms.Levels.FindRef(Id));
            if(Level>=3){Out=LOCTEXT("MaxLevel","首版功法最高地阶（3级），天阶以上待后续开放");return false;}
            if(ItemCount(TEXT("MAT_PAGE"))<2||State->Stones<50){Out=FText::Format(LOCTEXT("NeedStudy","研习需 2 功法残页 + 50 灵石（现有残页 {0}，灵石 {1}）"),FText::AsNumber(ItemCount(TEXT("MAT_PAGE"))),FText::AsNumber(State->Stones));return false;}
            Spend(TEXT("MAT_PAGE"),2);Spend(TEXT("CUR_STONE"),50);
            int32& Progress=Ms.Progress.FindOrAdd(Id);++Progress;
            if(Progress>=StudyNeeded(Level+1)){Progress=0;Ms.Levels.Add(Id,Level+1);Out=FText::Format(LOCTEXT("LevelUp","{0} 升至{1}阶"),M->Name,LevelLabel(Level+1));Record(TEXT("功法"),FString::Printf(TEXT("%s 升至%s阶"),*M->Name.ToString(),*LevelLabel(Level+1).ToString()));}
            else Out=FText::Format(LOCTEXT("Studied","{0} 研习进度 {1}/{2}"),M->Name,FText::AsNumber(Progress),FText::AsNumber(StudyNeeded(Level+1)));
            Touch();return true;
        }
        if(bLocked){Out=LOCTEXT("MethodBusy","换装只在准备期：当前有进行中的历练、竞采或天劫");return false;}
        if(!Ms.Levels.Contains(Id))Ms.Levels.Add(Id,1);
        if(Verb==TEXT("MethodMain"))
        {
            Ms.Support.Remove(Id);Ms.Main=Id;Out=FText::Format(LOCTEXT("MainSet","主修 · {0}"),M->Name);Touch();return true;
        }
        if(Verb==TEXT("MethodSupport"))
        {
            if(Ms.Support.Contains(Id)){Ms.Support.Remove(Id);Out=FText::Format(LOCTEXT("SupportOff","卸下辅修 · {0}"),M->Name);Touch();return true;}
            if(Ms.Main==Id){Out=LOCTEXT("SupportMain","同一部功法不能同时占主修与辅修");return false;}
            if(Ms.Support.Num()>=SupportSlots()){Out=FText::Format(LOCTEXT("SupportFull","辅修槽已满（{0}）"),FText::AsNumber(SupportSlots()));return false;}
            Ms.Support.Add(Id);Out=FText::Format(LOCTEXT("SupportOn","辅修 · {0}"),M->Name);Touch();return true;
        }
        if(Verb==TEXT("MethodClear")){if(Ms.Main==Id)Ms.Main=NAME_None;Ms.Support.Remove(Id);Out=LOCTEXT("Cleared","已卸下");Touch();return true;}
    }
    // Tribulation
    if(Verb==TEXT("TribStart"))return StartTribulation(Out);
    if(Verb==TEXT("TribCancel"))
    {
        if(State->Trib.State!=EXianTuTribState::Prepare){Out=LOCTEXT("TribNoCancel","天劫已开始，不能取消已开始的风险");return false;}
        State->Trib.State=EXianTuTribState::Cancelled;Out=LOCTEXT("TribCancelled","已在准备期无损取消，预留已释放");Touch();return true;
    }
    if(Verb==TEXT("TribAck"))
    {
        const auto S=State->Trib.State;
        if(S==EXianTuTribState::Succeeded||S==EXianTuTribState::Failed||S==EXianTuTribState::Cancelled){State->Trib.State=EXianTuTribState::None;Touch();}
        return true;
    }
    // Market (anchor-only, whitelisted tradables)
    if(Verb==TEXT("MarketList"))
    {
        const FName Item(*Arg(0));const int32 Count=Int(1),Price=Int(2);
        const FItem* Def=XianTuRules::Item(Item);
        if(!Def||!Def->bTradable){Out=LOCTEXT("NotTradable","只有可交易灵草、玄铁可以寄售");return false;}
        int32 Min,Max,Recycle;MarketRange(Item,Min,Max,Recycle);
        if(Count<1||Count>100){Out=LOCTEXT("BadCount","单笔数量 1 至 100");return false;}
        if(Price<Min||Price>Max){Out=FText::Format(LOCTEXT("BadPrice","单价需在 {0} 至 {1} 灵石之间"),FText::AsNumber(Min),FText::AsNumber(Max));return false;}
        int32 Active=0;for(const auto& O:State->Orders)Active+=O.bMine&&O.State==EXianTuOrderState::Listed?1:0;
        if(Active>=5){Out=LOCTEXT("TooMany","同时挂牌最多 5 笔");return false;}
        if(ItemCount(Item)<Count){Out=LOCTEXT("NotEnough","库存不足");return false;}
        if(State->Stones<1){Out=LOCTEXT("NoFee","挂牌费 1 灵石不足");return false;}
        Spend(Item,Count);Spend(TEXT("CUR_STONE"),1);
        FXianTuOrder O;O.Id=NextId(TEXT("MINE"));O.Item=Item;O.Count=Count;O.UnitPrice=Price;O.Listed=State->SimNow;O.Expires=State->SimNow+86400;O.bMine=true;O.Seller=TEXT("我");
        State->Orders.Insert(O,0);Record(TEXT("坊市"),FString::Printf(TEXT("挂牌 %s ×%d 单价 %d · 挂牌费 1"),*Def->Name.ToString(),Count,Price));
        Out=LOCTEXT("Listed","已托管并挂牌（挂牌费 1 灵石不退）");Touch();return true;
    }
    if(Verb==TEXT("MarketCancel"))
    {
        FXianTuOrder* O=State->Orders.FindByPredicate([&](const FXianTuOrder& X){return X.Id==Arg(0);});
        if(!O||!O->bMine||O->State!=EXianTuOrderState::Listed){Out=LOCTEXT("NoOrder","订单不可撤回");return false;}
        O->State=EXianTuOrderState::Cancelled;State->Items.FindOrAdd(O->Item)+=O->Count;
        Record(TEXT("坊市"),FString::Printf(TEXT("撤单 %s ×%d 已退回"),*ItemName(O->Item).ToString(),O->Count));
        Out=LOCTEXT("OrderCancelled","已撤单，货物退回库存");Touch();return true;
    }
    if(Verb==TEXT("MarketBuy"))
    {
        FXianTuOrder* O=State->Orders.FindByPredicate([&](const FXianTuOrder& X){return X.Id==Arg(0);});
        if(!O||O->State!=EXianTuOrderState::Listed){Out=LOCTEXT("Gone","订单已成交或下架");return false;}
        if(O->bMine){Out=LOCTEXT("OwnOrder","不能购买自己的订单");return false;}
        const int32 Total=O->Count*O->UnitPrice;
        if(State->Stones<Total){Out=FText::Format(LOCTEXT("NeedStones","灵石不足：需要 {0}"),FText::AsNumber(Total));return false;}
        State->Stones-=Total;State->Items.FindOrAdd(O->Item)+=O->Count;O->State=EXianTuOrderState::Sold;
        Record(TEXT("坊市"),FString::Printf(TEXT("买入 %s ×%d · 总价 %d"),*ItemName(O->Item).ToString(),O->Count,Total));
        Out=FText::Format(LOCTEXT("Bought","买入 {0} ×{1}，花费 {2} 灵石"),ItemName(O->Item),FText::AsNumber(O->Count),FText::AsNumber(Total));Touch();return true;
    }
    if(Verb==TEXT("MarketRecycle")||Verb==TEXT("MarketBind"))
    {
        const FName Item(*Arg(0));const int32 Count=FMath::Max(1,Int(1));
        const FItem* Def=XianTuRules::Item(Item);
        if(!Def||!Def->bTradable){Out=LOCTEXT("NotTradable2","只能处理可交易灵草、玄铁");return false;}
        if(ItemCount(Item)<Count){Out=LOCTEXT("NotEnough2","库存不足");return false;}
        Spend(Item,Count);
        if(Verb==TEXT("MarketRecycle"))
        {
            int32 Min,Max,Recycle;MarketRange(Item,Min,Max,Recycle);State->Stones+=Recycle*Count;
            Out=FText::Format(LOCTEXT("Recycled","回收 {0} ×{1}，得 {2} 灵石"),Def->Name,FText::AsNumber(Count),FText::AsNumber(Recycle*Count));
        }
        else
        {
            const FName Bound=Item==TEXT("MAT_HERB_T")?FName(TEXT("MAT_HERB_B")):FName(TEXT("MAT_IRON_B"));
            State->Items.FindOrAdd(Bound)+=Count;Out=FText::Format(LOCTEXT("Bound","{0} ×{1} 已绑定，可用于配方"),Def->Name,FText::AsNumber(Count));
        }
        Touch();return true;
    }
    // Item economy
    if(Verb==TEXT("ShopBuy"))return BuySku(FName(*Arg(0)),FMath::Max(1,Int(1)),Out);
    if(Verb==TEXT("CosmeticBuy"))return BuyCosmetic(FName(*Arg(0)),Out);
    if(Verb==TEXT("CosmeticWear"))return WearCosmetic(FName(*Arg(0)),Out);
    if(Verb==TEXT("FortuneDraw"))return DrawFortune(Out);
    if(Verb==TEXT("PropUse"))return UseProp(FName(*Arg(0)),Out);
    if(Verb.StartsWith(TEXT("Break")))return EditPlan(Verb,Args,Out);
    if(Verb==TEXT("GuildBuildUp"))return BuildGuild(FName(*Arg(0)),Out);
    if(Verb==TEXT("GearTry"))return TryGear(FName(*Arg(0)),Out);
    Out=LOCTEXT("Unknown","未识别的仙途指令");return false;
}

// ------------------------------------------------------------------ local verification helpers

void UXianTuSubsystem::DebugGrantBundle()
{
    auto R=[](const TCHAR* Item,int32 Count){FXianTuReward X;X.Item=Item;X.Count=Count;return X;};
    auto RN=[](FName Item,int32 Count){FXianTuReward X;X.Item=Item;X.Count=Count;return X;};
    TArray<FXianTuReward> Bundle={R(TEXT("CUR_STONE"),500),R(TEXT("MAT_HERB_T"),12),R(TEXT("MAT_IRON_T"),12),R(TEXT("MAT_PAGE"),8),R(TEXT("MAT_HERB_B"),6),R(TEXT("MAT_IRON_B"),6),R(TEXT("MAT_BREAK"),30),
        R(TEXT("CUR_JADE"),600),R(TEXT("CUR_WULING"),200),R(TEXT("CUR_SOUL"),60),R(TEXT("CUR_GOLD"),1500)};
    // The stage ahead in every grade, so the breakthrough screen can be laid out and refined at once.
    if(const XianTuCatalog::FStage* Stage=UpcomingStage())
    {
        using namespace XianTuCatalog;
        Bundle.Add(RN(MaterialKey(*Stage,ECategory::Core,1),2));Bundle.Add(RN(MaterialKey(*Stage,ECategory::Core,3),1));
        Bundle.Add(RN(MaterialKey(*Stage,ECategory::Guard,1),3));Bundle.Add(RN(MaterialKey(*Stage,ECategory::Guard,2),2));
        Bundle.Add(RN(MaterialKey(*Stage,ECategory::Essence,1),4));Bundle.Add(RN(MaterialKey(*Stage,ECategory::Essence,3),2));
        Bundle.Add(RN(MaterialKey(*Stage,ECategory::Treasure,2),1));
    }
    for(const XianTuCatalog::FProp& P:XianTuCatalog::Props())Bundle.Add(RN(P.Id,2));
    Grant(Bundle);
    if(State->Guild.bCreated)
    {
        State->Guild.Materials+=100;
        for(const XianTuCatalog::FGuildMaterial& G:XianTuCatalog::GuildMaterials())State->Guild.Vault.FindOrAdd(G.Id)+=12;
        Record(TEXT("测试"),TEXT("GM 测试资材 +100 · 建设材料各 +12"));
    }
    Notify(LOCTEXT("Bundle","GM 本地测试物资已发放"),1);
}

bool UXianTuSubsystem::DebugPrepareTribulation()
{
#if !UE_BUILD_SHIPPING
    UCultivationSubsystem* C=Cultivation();if(!C)return false;
    if(!C->DebugSetRealm(C->GetAnchorId(),21,true,400))return false;
    // v1.4 tribulations pay with the stage's core: keep one ready for the rehearsal.
    if(const XianTuCatalog::FStage* Stage=XianTuCatalog::StageFrom(21))
    {
        int32 Cores=0;for(int32 G=1;G<=4;++G)Cores+=ItemCount(XianTuCatalog::MaterialKey(*Stage,XianTuCatalog::ECategory::Core,G));
        if(!Cores)State->Items.FindOrAdd(XianTuCatalog::MaterialKey(*Stage,XianTuCatalog::ECategory::Core,1))+=1;
        NormalizePlan();EnsurePlanCore();
    }
    if(UCultivationHealthComponent* H=C->GetAnchorHealth()){if(H->bDead)H->Revive();else H->Heal(H->MaxHealth);}
    State->Trib.LastSettled=0;State->Trib.DaoHeartUntil=0;SyncDaoHeart();Touch();
    Notify(LOCTEXT("TribReady","GM：主播已置于金丹六阶圆满，可尝试渡劫冲击元婴"),1);return true;
#else
    return false;
#endif
}

void UXianTuSubsystem::DebugRestoreTribulation(const TMap<FString,int32>& Failures,double DaoHeartUntil)
{
#if !UE_BUILD_SHIPPING
    State->Trib.Failures=Failures;State->Trib.DaoHeartUntil=DaoHeartUntil;
    // The rehearsal's own heart debuff goes first, so a heart that was already broken comes back with its own time left.
    const UCultivationSubsystem* C=Cultivation();const UCultivationHealthComponent* H=C?C->GetAnchorHealth():nullptr;
    if(ULiveAnchorBuffSubsystem* Buffs=H?ULiveAnchorBuffSubsystem::Get(H):nullptr)Buffs->Remove(TEXT("DaoHeart"),TEXT("tribulation"));
    SyncDaoHeart();Touch();
#endif
}

int32 UXianTuSubsystem::DebugAssists(int32 Count)
{
    const ALiveWeaponSwarm* S=Swarm();if(!S)return 0;
    int32 Sent=0;
    for(const FLiveWeaponState& P:S->GetRoster().GetStates())
    {
        if(Sent>=Count)break;
        if(!P.bParticipating||P.bPersonalSword||State->Trib.Assists.Contains(P.Profile.ParticipantId))continue;
        if(AddTribulationAssist(P.Profile.ParticipantId,P.Profile.DisplayName))++Sent;
    }
    return Sent;
}

int32 UXianTuSubsystem::DebugResponses()
{
    int32 Sent=0;const FXianTuRace& R=State->Race;
    for(int32 K=0;K<R.Representatives.Num();++K)
    {
        if(RecordRaceResponse(R.Representatives[K]))++Sent;
    }
    return Sent;
}

#undef LOCTEXT_NAMESPACE

#include "LiveSwordTestGameMode.h"
#include "LiveWeaponSwarm.h"
#include "LiveMonster.h"
#include "LiveScarecrow.h"
#include "LiveWeapons/SpecialSwords/LiveSpecialSword.h"
#include "LiveWeapons/SpecialSwords/LiveSpecialSwordComponent.h"
#include "LiveWeapons/SpecialSwords/LiveSwordCodex.h"
#include "XianTu/XianTuRules.h"
#include "EngineUtils.h"
#include "Cultivation/CultivationSubsystem.h"
#include "Cultivation/CultivationHealthComponent.h"
#include "XianTu/XianTuSubsystem.h"
#include "XianTu/XianTuTribulationStorm.h"
#include "TimerManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Guid.h"
#include "Misc/Parse.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
namespace
{
    FString SwordRangeViewer(ELiveSpecialSword Kind){return FString::Printf(TEXT("local:sword_range_%d"),int32(Kind));}
}
ALiveSwordTestGameMode::ALiveSwordTestGameMode(){InitialMonsters=0;bOrthographicCamera=false;bAnchorPersonalSword=false;}
void ALiveSwordTestGameMode::InitGame(const FString& MapName,const FString& Options,FString& ErrorMessage)
{
    Super::InitGame(MapName,Options,ErrorMessage);
#if !UE_BUILD_SHIPPING
    if(FParse::Param(FCommandLine::Get(),TEXT("SwordRangeOrtho")))bOrthographicCamera=true;
#endif
    if(UClass* Pawn=CultivatorClass.LoadSynchronous())DefaultPawnClass=Pawn;
    if(!bGiftAttributeBonuses)if(auto* Cultivation=GetGameInstance()?GetGameInstance()->GetSubsystem<UCultivationSubsystem>():nullptr)
    {
        Cultivation->Rules.JetFlatBonus=0;Cultivation->Rules.SoulPersonalMultiplier=1;
        Cultivation->Rules.SoulAudienceShare=0;Cultivation->Rules.SoulAnchorShare=0;Cultivation->Rules.EnemyReduction=0;
    }
}
void ALiveSwordTestGameMode::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if(bTestReady||!Swarm)return;
    bTestReady=true;SetActorTickEnabled(false);
    if(Swarm->SpecialSwords)for(const ELiveSpecialSword Kind:LiveSpecialSwords::All())Swarm->SpecialSwords->ResetGrowth(ViewerId(Kind));
    if(auto* Cultivation=GetGameInstance()?GetGameInstance()->GetSubsystem<UCultivationSubsystem>():nullptr)
        if((AnchorHealth=Cultivation->GetAnchorHealth()).IsValid())AnchorHealth->OnDied.AddUniqueDynamic(this,&ALiveSwordTestGameMode::HandleAnchorDied);
    GetWorldTimerManager().SetTimer(RealmTimer,this,&ALiveSwordTestGameMode::HoldSwordRealms,.25f,true);
    for(int32 I=0;I<ExtraViewers;++I)
    {
        FLiveParticipantProfile Profile;
        Profile.ParticipantId=FString::Printf(TEXT("local:range_spirit_%d"),I);
        Profile.DisplayName=FString::Printf(TEXT("剑灵%d"),I+1);
        Swarm->JoinParticipant(Profile);
    }
    if(bSummonSpecialSwords)SetAllSpecialSwords(true);
}
void ALiveSwordTestGameMode::HandleAnchorDied(){GetWorldTimerManager().SetTimer(ReviveTimer,this,&ALiveSwordTestGameMode::ReviveAnchor,FMath::Max(.5f,AnchorReviveDelay),false);}
void ALiveSwordTestGameMode::ReviveAnchor(){if(auto* Health=AnchorHealth.Get();Health&&Health->bDead)Health->Revive();}
FString ALiveSwordTestGameMode::ViewerId(ELiveSpecialSword Kind)const
{
    return Kind!=ELiveSpecialSword::None?SwordRangeViewer(Kind):FString();
}
bool ALiveSwordTestGameMode::SetSpecialSword(ELiveSpecialSword Kind,bool bPresent)
{
    auto* Cultivation=GetGameInstance()?GetGameInstance()->GetSubsystem<UCultivationSubsystem>():nullptr;
    if(Kind==ELiveSpecialSword::None||!Swarm||!Cultivation)return false;
    const FString Id=ViewerId(Kind),Name=LiveSpecialSwords::Name(Kind).ToString();
    if(!bPresent)return Swarm->RemoveParticipant(Id);
    FLiveParticipantProfile Profile;Profile.ParticipantId=Id;Profile.DisplayName=Name;
    if(!Swarm->JoinParticipant(Profile).IsValid())return false;
    HoldSwordRealms();
    if(LiveSpecialSwords::FromTitle(Cultivation->GetParticipantSnapshot(Id).TitleId)==Kind)return true;
    FCultivationGiftEvent Gift;
    Gift.ParticipantId=Id;
    Gift.MessageId=FGuid::NewGuid().ToString(EGuidFormats::Digits);
    Gift.GiftId=LiveSpecialSwords::GiftOf(Kind);
    Gift.Count=1;
    Gift.EventUtcSeconds=UCultivationSubsystem::UtcNow();
    Gift.bDevelopment=true;
    return Cultivation->ProcessGift(Gift);
}
void ALiveSwordTestGameMode::HoldSwordRealms()
{
    auto* Cultivation=GetGameInstance()?GetGameInstance()->GetSubsystem<UCultivationSubsystem>():nullptr;if(!Cultivation)return;
    for(const ELiveSpecialSword Kind:LiveSpecialSwords::All())
    {
        const FString Id=ViewerId(Kind);if(Id.IsEmpty())continue;
        const int32 Want=GetSwordRealm(Kind);const FCultivationParticipantSnapshot S=Cultivation->GetParticipantSnapshot(Id);
        if(S.ParticipantId.IsEmpty()||S.RealmIndex==Want)continue;
#if !UE_BUILD_SHIPPING
        Cultivation->DebugSetRealm(Id,Want,false,0);Cultivation->ApplyCultivationLoss(Id,1.e30);
#endif
    }
}
void ALiveSwordTestGameMode::SetSwordRealm(ELiveSpecialSword Kind,int32 Realm)
{
    if(Kind==ELiveSpecialSword::None)return;
    SwordRealms.Add(Kind,FMath::Clamp(Realm,0,33));HoldSwordRealms();
}
void ALiveSwordTestGameMode::SetAllSwordRealms(int32 Realm){for(const ELiveSpecialSword Kind:LiveSpecialSwords::All())SwordRealms.Add(Kind,FMath::Clamp(Realm,0,33));HoldSwordRealms();}
int32 ALiveSwordTestGameMode::GetSwordRealm(ELiveSpecialSword Kind)const{const int32* R=SwordRealms.Find(Kind);return R?*R:0;}
int32 ALiveSwordTestGameMode::GetCommonSwordRealm()const
{
    const int32 First=GetSwordRealm(ELiveSpecialSword::SoulEater);
    for(const ELiveSpecialSword Kind:LiveSpecialSwords::All())if(GetSwordRealm(Kind)!=First)return -1;
    return First;
}
void ALiveSwordTestGameMode::StepSwordRealms(int32 Direction)
{
    int32 From=33;for(const ELiveSpecialSword Kind:LiveSpecialSwords::All())From=FMath::Min(From,GetSwordRealm(Kind));
    const int32 Common=GetCommonSwordRealm();
    const TArray<int32>& Stops=LiveSwordCodex::MajorStops();int32 Next=Common<0?From:Common;
    if(Direction>0){for(const int32 S:Stops)if(S>Next){Next=S;break;}}
    else for(int32 I=Stops.Num()-1;I>=0;--I)if(Stops[I]<Next){Next=Stops[I];break;}
    SetAllSwordRealms(Common<0&&Direction>0?From:Next);
}
FLiveSpecialSwordGrowth ALiveSwordTestGameMode::GetSwordGrowth(ELiveSpecialSword Kind)const
{
    return Swarm&&Swarm->SpecialSwords?Swarm->SpecialSwords->GetGrowth(ViewerId(Kind)):FLiveSpecialSwordGrowth();
}
TArray<ALiveScarecrow*> ALiveSwordTestGameMode::GetScarecrows()const
{
    TArray<ALiveScarecrow*> Out;for(TActorIterator<ALiveScarecrow> It(GetWorld());It;++It)Out.Add(*It);
    Out.Sort([](const ALiveScarecrow& A,const ALiveScarecrow& B){return A.RealmPreset!=B.RealmPreset?A.RealmPreset<B.RealmPreset:A.GetName()<B.GetName();});
    return Out;
}
ALiveScarecrow* ALiveSwordTestGameMode::GetNearestScarecrow()const
{
    const APlayerController* PC=GetWorld()->GetFirstPlayerController();const APawn* Pawn=PC?PC->GetPawn():nullptr;if(!Pawn)return nullptr;
    ALiveScarecrow* Best=nullptr;double BestDistance=MAX_dbl;
    for(TActorIterator<ALiveScarecrow> It(GetWorld());It;++It)
    {const double D=FVector::DistSquared(It->GetActorLocation(),Pawn->GetActorLocation());if(D<BestDistance){BestDistance=D;Best=*It;}}
    return Best;
}
void ALiveSwordTestGameMode::SetTribulationOutcome(int32 Outcome)
{
    TribOutcome=FMath::Clamp(Outcome,0,2);
    if(auto* X=GetGameInstance()?GetGameInstance()->GetSubsystem<UXianTuSubsystem>():nullptr)X->DebugForceRoll(TribOutcome==1?10000:TribOutcome==2?0:-1);
}
void ALiveSwordTestGameMode::SetStarWardOutcome(int32 Outcome)
{
    if(auto* X=GetGameInstance()?GetGameInstance()->GetSubsystem<UXianTuSubsystem>():nullptr)X->DebugForceWardRoll(Outcome==1?10000:Outcome==2?0:-1);
}
int32 ALiveSwordTestGameMode::GetStarWard()const
{
    const UXianTuSubsystem* X=GetGameInstance()?GetGameInstance()->GetSubsystem<UXianTuSubsystem>():nullptr;
    return X?int32(X->GetState()->Trib.Ward):0;
}
int32 ALiveSwordTestGameMode::GetStarWardChance()const
{
    const UXianTuSubsystem* X=GetGameInstance()?GetGameInstance()->GetSubsystem<UXianTuSubsystem>():nullptr;
    return X?X->GetState()->Trib.WardChance:0;
}
void ALiveSwordTestGameMode::SetAllSpecialSwords(bool bPresent){for(const ELiveSpecialSword Kind:LiveSpecialSwords::All())SetSpecialSword(Kind,bPresent);}
bool ALiveSwordTestGameMode::HasSpecialSword(ELiveSpecialSword Kind)const
{
    const FString Id=ViewerId(Kind);const FLiveWeaponState* State=Swarm&&!Id.IsEmpty()?Swarm->GetRoster().Find(Id):nullptr;
    return State&&State->bParticipating;
}
ALiveSpecialSword* ALiveSwordTestGameMode::FindSpecialSword(ELiveSpecialSword Kind)const
{
    return Swarm&&Swarm->SpecialSwords?Swarm->SpecialSwords->FindSword(ViewerId(Kind)):nullptr;
}
ALiveMonster* ALiveSwordTestGameMode::SpawnTestTarget(float MaxHealth,float Defense,int32 Realm,FVector Location)
{
    const FTransform Transform(Location);
    ALiveMonster* Target=GetWorld()->SpawnActorDeferred<ALiveMonster>(ALiveMonster::StaticClass(),Transform,nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
    if(!Target)return nullptr;
    Target->MaxHealth=FMath::Max(1.f,MaxHealth);Target->Defense=FMath::Max(0.f,Defense);Target->Realm=FMath::Clamp(Realm,-1,33);
    Target->bStationary=true;Target->AttackDamage=0;
    Target->FinishSpawning(Transform);
    return Target;
}
bool ALiveSwordTestGameMode::IsTribulationActive()const
{
    const UXianTuSubsystem* X=GetGameInstance()?GetGameInstance()->GetSubsystem<UXianTuSubsystem>():nullptr;if(!X)return false;
    const EXianTuTribState S=X->GetState()->Trib.State;
    return S==EXianTuTribState::Prepare||S==EXianTuTribState::Trial||S==EXianTuTribState::Frozen||S==EXianTuTribState::Backlash||X->GetTribulationStorm()!=nullptr;
}
int32 ALiveSwordTestGameMode::GetAnchorRealm()const
{
    const auto* Cultivation=GetGameInstance()?GetGameInstance()->GetSubsystem<UCultivationSubsystem>():nullptr;
    return Cultivation?Cultivation->GetParticipantSnapshot(Cultivation->GetAnchorId()).RealmIndex:INDEX_NONE;
}
bool ALiveSwordTestGameMode::StartTribulationTest()
{
    const auto* Cultivation=GetGameInstance()?GetGameInstance()->GetSubsystem<UCultivationSubsystem>():nullptr;
    auto* X=GetGameInstance()?GetGameInstance()->GetSubsystem<UXianTuSubsystem>():nullptr;
    if(!X||!Cultivation||IsTribulationActive())return false;
    if(RehearsalRealm==INDEX_NONE)
    {
        RehearsalRealm=Cultivation->GetParticipantSnapshot(Cultivation->GetAnchorId()).RealmIndex;
        RehearsalFailures=X->GetState()->Trib.Failures;
        RehearsalDaoHeart=X->GetState()->Trib.DaoHeartUntil;
    }
    SetTribulationOutcome(TribOutcome);
    FText Msg;
    const bool bLaunched=X->DebugPrepareTribulation()&&X->Execute(TEXT("TribStart"),{},Msg);
    if(bLaunched)GetWorldTimerManager().SetTimer(RehearsalTimer,this,&ALiveSwordTestGameMode::EndRehearsal,1.f,true);
    else EndRehearsal();
    return bLaunched;
}
void ALiveSwordTestGameMode::EndRehearsal()
{
    if(IsTribulationActive())return;
    GetWorldTimerManager().ClearTimer(RehearsalTimer);
#if !UE_BUILD_SHIPPING
    if(auto* Cultivation=GetGameInstance()?GetGameInstance()->GetSubsystem<UCultivationSubsystem>():nullptr;Cultivation&&RehearsalRealm!=INDEX_NONE)
        Cultivation->DebugSetRealm(Cultivation->GetAnchorId(),RehearsalRealm,false,0);
    if(auto* X=GetGameInstance()?GetGameInstance()->GetSubsystem<UXianTuSubsystem>():nullptr;X&&RehearsalRealm!=INDEX_NONE)
        X->DebugRestoreTribulation(RehearsalFailures,RehearsalDaoHeart);
#endif
    RehearsalRealm=INDEX_NONE;
}
bool ALiveSwordTestGameMode::AssistTribulation()
{
    auto* X=GetGameInstance()?GetGameInstance()->GetSubsystem<UXianTuSubsystem>():nullptr;
    if(!X||X->GetState()->Trib.State!=EXianTuTribState::Trial)return false;
    const int32 Before=X->GetState()->Trib.Assists.Num();
    for(int32 I=0;I<10;++I)
    {
        const int32 N=++AssistHelpers;
        X->AddTribulationAssist(FString::Printf(TEXT("local:trib_helper_%d"),N),FString::Printf(TEXT("护法%d"),N));
    }
    return X->GetState()->Trib.Assists.Num()>Before;
}
FText ALiveSwordTestGameMode::GetTribulationStatus()const
{
    const UXianTuSubsystem* X=GetGameInstance()?GetGameInstance()->GetSubsystem<UXianTuSubsystem>():nullptr;if(!X)return FText::GetEmpty();
    const FXianTuTribulation& T=X->GetState()->Trib;const AXianTuTribulationStorm* Storm=X->GetTribulationStorm();
    const FString Bolts=Storm?FString::Printf(TEXT(" · 已落雷 %d 道 · 共伤 %.0f"),Storm->GetBoltsLanded(),Storm->GetDamageDealt()):FString();
    const auto* Cultivation=GetGameInstance()->GetSubsystem<UCultivationSubsystem>();
    const int32 WardRealm=X->StarWardRealm();
    const FString WardOdds=WardRealm>=0?FString::Printf(TEXT(" · 七星挡劫 %g%%"),XianTuRules::StarWardChance(WardRealm,T.FromRealm)/100.0):FString();
    const FString Ward=T.Ward==EXianTuWard::None?FString():FString::Printf(TEXT(" · %s（%s剑灵 %g%%）"),T.Ward==EXianTuWard::Held?TEXT("七星挡劫"):TEXT("天雷破盾"),
        Cultivation?*Cultivation->GetRealmName(T.WardRealm).ToString():TEXT(""),T.WardChance/100.0);
    switch(T.State)
    {
    case EXianTuTribState::Prepare:
        return FText::FromString(FString::Printf(TEXT("天劫 · 劫云汇聚 %.0f秒 · 成功率 %d%%%s"),FMath::Max(0.0,15.0-T.Elapsed),T.Probability/100,*WardOdds));
    case EXianTuTribState::Trial:case EXianTuTribState::Frozen:
    {
        static const TCHAR* Phases[]={TEXT("肉身劫"),TEXT("神魂劫"),TEXT("心魔劫")};
        const int32 Phase=FMath::Clamp(int32(T.Elapsed/20.0),0,2);
        const FXianTuTribulationPressure P=AXianTuTribulationStorm::Pressure(T.Probability,Phase,Storm?Storm->DamageFraction:.05f);
        return FText::FromString(FString::Printf(TEXT("%s · 剩余 %.0f秒 · 成功率 %d%% · 劫威 %.0f%% · 每 %.1f秒 %d 道 · 每道 %.1f%% 生命%s%s"),
            Phases[Phase],FMath::Max(0.0,60.0-T.Elapsed),T.Probability/100,P.Intensity*100.f,P.Interval,P.Bolts,P.DamageFraction*100.f,*WardOdds,*Bolts));
    }
    case EXianTuTribState::Backlash:
    {
        const float Intensity=FMath::Clamp((10000-T.Probability)/5000.f,0.f,1.f);
        const int32 Next=FMath::Min(T.BacklashStrikes,XianTuRules::TribBacklashStrikes-1);
        return FText::FromString(FString::Printf(TEXT("天罚 · 剩余 %.1f秒 · 第 %d/%d 道 · 本道 %.1f%% 生命 · 已伤 %.0f%s · 倒下损失修为，扛住道心破碎"),
            FMath::Max(0.0,XianTuRules::TribBacklash-T.Elapsed),T.BacklashStrikes,XianTuRules::TribBacklashStrikes,
            AXianTuTribulationStorm::BacklashFraction(Next,Intensity)*100.f,T.BacklashDamage,Ward.IsEmpty()?TEXT(" · 无七星护持"):*Ward));
    }
    case EXianTuTribState::Succeeded:return T.RevealRemaining>0||Storm?FText::FromString(TEXT("渡劫成功 · 劫云散去")+Bolts):FText::GetEmpty();
    case EXianTuTribState::Failed:
        if(T.RevealRemaining<=0&&!Storm)return FText::GetEmpty();
        if(T.bDied)return FText::FromString(FString::Printf(TEXT("渡劫失败 · 陨落于第 %d 道天罚%s，损失修为 %.0f"),T.BacklashStrikes,*Ward,T.Loss));
        if(T.bDaoHeartBroken)return FText::FromString(FString::Printf(TEXT("渡劫失败 · 扛过 %d 道天罚%s，修为保留 · 道心破碎：防御 −%.0f%%，%.0f 分钟内不可渡劫"),
            T.BacklashStrikes,*Ward,XianTuRules::DaoHeartDefense*100.0,XianTuRules::DaoHeartDuration/60.0));
        return FText::FromString(TEXT("渡劫失败")+Bolts);
    default:return Storm?FText::FromString(TEXT("劫云散去")+Bolts):FText::GetEmpty();
    }
}
double ALiveSwordTestGameMode::GetSpecialSwordDamage(ELiveSpecialSword Kind)const
{
    return Swarm&&Swarm->SpecialSwords?Swarm->SpecialSwords->GetDamageDealt(ViewerId(Kind)):0.0;
}

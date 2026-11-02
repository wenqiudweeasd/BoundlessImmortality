#include "InkUI/InkGameUIWidget.h"
#include "InkUI/InkUIRoot.h"
#include "InkUI/InkUIModel.h"
#include "CultivationGameplayComponent.h"
#include "Cultivation/CultivationHealthComponent.h"
#include "Cultivation/CultivationSubsystem.h"
#include "Frontend/InkMenuDirector.h"
#include "Frontend/InkSaveSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Framework/Application/SlateApplication.h"
#include "Input/HittestGrid.h"
#include "Widgets/SWindow.h"
#include "Widgets/Layout/SScrollBox.h"
#include "GameFramework/GameUserSettings.h"
#include "GameFramework/Pawn.h"
#include "Kismet/KismetSystemLibrary.h"
#include "XianTu/XianTuSubsystem.h"
#include "XianTu/XianTuRules.h"
#include "LiveWeapons/LiveWeaponSwarm.h"
#include "LiveWeapons/SpecialSwords/LiveSpecialSword.h"
#include "LiveWeapons/SpecialSwords/LiveSpecialSwordComponent.h"
#include "LiveWeapons/SpecialSwords/LiveAnchorBuffSubsystem.h"
#include "EngineUtils.h"

#define LOCTEXT_NAMESPACE "InkGameUI"

using namespace InkUI;

namespace
{
    const TCHAR* SlotNumerals[]={TEXT("一"),TEXT("二"),TEXT("三"),TEXT("四"),TEXT("五"),TEXT("六")};

    bool ParseSlotCommand(const FString& Command,FString& OutVerb,int32& OutSlot)
    {
        FString Number;
        if(!Command.Split(TEXT("_"),&OutVerb,&Number)||!Number.IsNumeric())return false;
        OutSlot=FCString::Atoi(*Number);
        return UInkSaveSubsystem::IsValidSlotIndex(OutSlot);
    }
}

UInkGameUIWidget::UInkGameUIWidget(const FObjectInitializer& ObjectInitializer):Super(ObjectInitializer)
{
    SetIsFocusable(true);
}

void UInkGameUIWidget::Configure(AInkMenuDirector* InDirector)
{
    Director=InDirector;
    if(!Model){Model=MakeShared<FModel>();Model->SeedLocal();}
    if(UGameInstance* GI=GetGameInstance())Model->XianTu=GI->GetSubsystem<UXianTuSubsystem>();
    PullSettings();
    PullSaves();
}

TSharedRef<SWidget> UInkGameUIWidget::RebuildWidget()
{
    if(!Model){Model=MakeShared<FModel>();Model->SeedLocal();}
    Art::Preload();
    const TWeakObjectPtr<UInkGameUIWidget> Self(this);
    SAssignNew(Root,SInkUIRoot).Model(Model).Visibility(EVisibility::SelfHitTestInvisible)
        .OnCommand_Lambda([Self](FName Command){if(UInkGameUIWidget* W=Self.Get())W->HandleCommand(Command);});
    Root->HostTick=[Self](float DeltaSeconds){if(UInkGameUIWidget* W=Self.Get())W->HostTick(DeltaSeconds);};
    if(PendingBase==1)Root->ShowBase(EBase::MainMenu);
    else if(PendingBase==2)Root->ShowBase(EBase::Hud);
    return Root.ToSharedRef();
}

void UInkGameUIWidget::ReleaseSlateResources(bool bReleaseChildren)
{
    Super::ReleaseSlateResources(bReleaseChildren);
    if(Root)Root->HostTick=nullptr;
    Root.Reset();
}

void UInkGameUIWidget::ShowMainMenu()
{
    PendingBase=1;bInputBlocked=false;
    PullSaves();
    if(Root)Root->ShowBase(EBase::MainMenu);
}

void UInkGameUIWidget::ShowHud()
{
    PendingBase=2;bInputBlocked=false;
    PullLiveState();
    if(Root)Root->ShowBase(EBase::Hud);
}

bool UInkGameUIWidget::OpenRoute(FName Route){return Root&&Root->OpenPage(Route);}

void UInkGameUIWidget::CloseTopRoute(){if(Root)Root->CloseTopPage();}

void UInkGameUIWidget::ShowToast(const FText& Message,int32 Kind){if(Root)Root->Toast(Message,Kind);}

void UInkGameUIWidget::SetReducedMotion(bool bReduced){Motion::SetReduced(bReduced);if(Model)Model->Settings.bReducedMotion=bReduced;}

FName UInkGameUIWidget::GetTopRoute() const{return Root?Root->GetTopRoute():NAME_None;}

bool UInkGameUIWidget::WantsGameInputBlocked() const{return Root&&Root->WantsGameInputBlocked();}

bool UInkGameUIWidget::IsHudVisible() const
{
    return Root&&IsInViewport()&&IsVisible()&&GetRenderOpacity()>0&&Root->GetBase()==InkUI::EBase::Hud&&!Root->HasPages();
}

bool UInkGameUIWidget::IsReducedMotion() const{return Motion::IsReduced();}

void UInkGameUIWidget::HostTick(float DeltaSeconds)
{
    if(!Model)return;
    PullElapsed+=DeltaSeconds;
    if(PullElapsed>=.2f){PullElapsed=0.f;PullLiveState();PullXianTu();PullBuffs();}
    SpiritElapsed+=DeltaSeconds;
    if(SpiritElapsed>=1.f&&Root&&Root->GetTopRoute()==TEXT("SwordSpirit")){SpiritElapsed=0.f;PullSpirits();}
    FPlayerState& P=Model->Player;
    // Without a running cultivation service the HUD still demonstrates meditation, clearly as local data.
    if(!P.bLive&&P.bCultivating&&P.MaxQi>0)P.Qi=FMath::Min(P.MaxQi,P.Qi+P.QiPerSecond*DeltaSeconds);
    if(Director&&!Director->bMenuActive&&Root)
    {
        const bool bWanted=Root->WantsGameInputBlocked();
        if(bWanted!=bInputBlocked){bInputBlocked=bWanted;Director->SetFrontendInputBlocked(bWanted);if(bWanted)Root->FocusRoot();}
    }
}

void UInkGameUIWidget::PullLiveState()
{
    if(!Model||bPreviewVitals)return;
    FPlayerState& P=Model->Player;
    UGameInstance* GI=GetGameInstance();
    UCultivationSubsystem* Cultivation=GI?GI->GetSubsystem<UCultivationSubsystem>():nullptr;
    if(Cultivation&&Director&&!Director->bMenuActive)
    {
        const FCultivationParticipantSnapshot S=Cultivation->GetParticipantSnapshot(Cultivation->GetAnchorId());
        if(!S.ParticipantId.IsEmpty())
        {
            if(!S.DisplayName.IsEmpty())P.Name=FText::FromString(S.DisplayName);
            P.Realm=S.RealmName;P.RealmIndex=S.RealmIndex;
            P.Qi=S.Cultivation;P.MaxQi=S.RequiredCultivation;P.QiPerSecond=S.EffectiveCultivationPerSecond;
            if(const UCultivationHealthComponent* Health=Cultivation->GetAnchorHealth()){P.Health=Health->Health;P.MaxHealth=Health->MaxHealth;}
            P.bLive=true;
        }
    }
    if(const APawn* Pawn=GetOwningPlayerPawn())
        if(const auto* Action=Pawn->FindComponentByClass<UCultivationGameplayComponent>())
            if(P.bLive)P.bCultivating=Action->State==ECultivationActionState::MeditationStart||Action->State==ECultivationActionState::MeditationLoop;
}

void UInkGameUIWidget::PullXianTu()
{
    if(!Model)return;
    UXianTuSubsystem* X=Model->XianTu.Get();
    if(!X){if(UGameInstance* GI=GetGameInstance())Model->XianTu=X=GI->GetSubsystem<UXianTuSubsystem>();if(!X)return;}
    for(const UXianTuSubsystem::FNotice& N:X->TakeNotices())
    {
        if(!Root)continue;
        if(N.Rewards.Num())
        {
            TArray<FReward> Rewards;
            for(const FXianTuReward& R:N.Rewards)
            {
                const XianTuRules::FItem* Item=XianTuRules::Item(R.Item);
                FReward W;W.Icon=Item?Item->Icon:FName(TEXT("crystal"));W.Name=XianTuRules::ItemName(R.Item);W.Count=R.Count;W.Quality=EQuality(Item?FMath::Clamp(Item->Quality,0,5):0);Rewards.Add(W);
            }
            Root->ShowRewards(Rewards,N.Title);
        }
        Root->Toast(N.Text,N.Kind);
    }
    // Pages sync XianTuRev themselves after an action, so the wallet keeps its own revision.
    if(const int32 Rev=X->GetRevision();Rev!=WalletRev||Rev!=Model->XianTuRev){WalletRev=Rev;Model->XianTuRev=Rev;ProjectWallet();Model->Touch();}
}

void UInkGameUIWidget::ProjectWallet()
{
    const UXianTuSubsystem* X=Model?Model->XianTu.Get():nullptr;if(!X)return;
    if(FCurrency* Stone=Model->Currencies.FindByPredicate([](const FCurrency& C){return C.Id==TEXT("SpiritStone");}))Stone->Amount=X->Stones();
    for(FCurrency& C:Model->Currencies)if(C.Id.ToString().StartsWith(TEXT("CUR_")))C.Amount=X->ItemCount(C.Id);
    // Seed items that the ledger now carries for real (灵石, 筑基丹, 功法残页) leave the bag once a game runs.
    static const FName Superseded[]={TEXT("SpiritStoneLow"),TEXT("SpiritStoneMid"),TEXT("FoundationPill"),TEXT("ManualShard"),TEXT("CopperCoin")};
    Model->Items.RemoveAll([](const FItem& I){if(I.Id.ToString().StartsWith(TEXT("XT_")))return true;for(const FName Id:Superseded)if(I.Id==Id)return true;return false;});
    for(const XianTuRules::FItem& Def:XianTuRules::Items())
    {
        if(Def.Id==TEXT("CUR_STONE")||Def.Kind==TEXT("Currency"))continue;
        const int32 Count=X->ItemCount(Def.Id);if(Count<=0)continue;
        FItem I;I.Key=FGuid::NewDeterministicGuid(Def.Id.ToString());I.Id=FName(*(TEXT("XT_")+Def.Id.ToString()));I.Name=Def.Name;I.Description=Def.Description;
        I.Category=Def.Kind;I.bUsable=Def.Kind==TEXT("Consumable");I.Icon=Def.Icon;I.Quality=EQuality(FMath::Clamp(Def.Quality,0,5));I.Count=Count;Model->Items.Add(I);
    }
    // The ledger has no slot limit: the bag grows a row ahead of what it holds.
    Model->BagCapacity=FMath::Max(40,FMath::DivideAndRoundUp(Model->Items.Num()+4,4)*4);
    ++Model->ItemsRev;
}

void UInkGameUIWidget::PullBuffs()
{
    if(!Model)return;
    const ULiveAnchorBuffSubsystem* B=ULiveAnchorBuffSubsystem::Get(GetOwningPlayer());
    TArray<FBuffView> Next;double Shield=0,ShieldMax=0;
    if(B&&Director&&!Director->bMenuActive)
    {
        B->GetShield(Shield,ShieldMax);
        for(const FLiveAnchorBuff& Buff:B->GetBuffs())
        {
            // Machine-soul layers from several swords read as one stacked icon.
            if(FBuffView* Same=Next.FindByPredicate([&](const FBuffView& V){return V.Id==Buff.Id;}))
            {Same->Stacks+=Buff.Stacks;Same->Remaining=FMath::Max(Same->Remaining,Buff.Remaining);continue;}
            FBuffView V;V.Id=Buff.Id;V.Glyph=Buff.Glyph;V.Name=Buff.Name;V.Description=Buff.Description;V.Color=Buff.Color;V.Stacks=Buff.Stacks;
            V.Remaining=Buff.Remaining;V.Duration=Buff.Duration;V.bDebuff=Buff.bDebuff;Next.Add(V);
        }
        Next.Sort([](const FBuffView& A,const FBuffView& C){return A.bDebuff!=C.bDebuff?!A.bDebuff:A.Id.LexicalLess(C.Id);});
    }
    Model->Shield=Shield;Model->ShieldMax=ShieldMax;
    bool bSame=Next.Num()==Model->Buffs.Num();
    for(int32 I=0;bSame&&I<Next.Num();++I)bSame=Next[I].Id==Model->Buffs[I].Id&&Next[I].Stacks==Model->Buffs[I].Stacks;
    if(bSame){for(int32 I=0;I<Next.Num();++I)Model->Buffs[I].Remaining=Next[I].Remaining;return;}
    Model->Buffs=MoveTemp(Next);++Model->BuffsRev;Model->Touch();
}

void UInkGameUIWidget::PullSpirits()
{
    if(!Model)return;
    ALiveWeaponSwarm* Swarm=Director?Director->GameplaySwarm.Get():nullptr;
    UGameInstance* GI=GetGameInstance();const UCultivationSubsystem* Cultivation=GI?GI->GetSubsystem<UCultivationSubsystem>():nullptr;
    TArray<FSpiritView> Next;
    if(Swarm&&Cultivation)
    {
        Model->AudienceMeshes=Swarm->GetAudienceMeshCount();
        for(const FLiveWeaponState& S:Swarm->GetRoster().GetStates())
        {
            if(!S.bParticipating||S.bPersonalSword)continue;
            FSpiritView V;V.Id=S.Profile.ParticipantId;V.Name=FText::FromString(S.Profile.DisplayName);V.Mesh=Swarm->GetParticipantMeshName(V.Id);
            const FCultivationParticipantSnapshot Snapshot=Cultivation->GetParticipantSnapshot(V.Id);
            V.Realm=Snapshot.RealmName;V.RealmIndex=Snapshot.RealmIndex;V.Attack=Snapshot.Attributes.Attack+Snapshot.Attributes.SpiritDamage;
            if(const ALiveSpecialSword* Sword=Swarm->SpecialSwords?Swarm->SpecialSwords->FindSword(V.Id):nullptr)
            {
                V.Special=uint8(Sword->GetKind());V.Status=Sword->GetStatusText();V.Meter=Sword->GetMeterFraction();V.MeterLabel=Sword->GetMeterLabel();
                V.Attack=Sword->AttackPower();V.Damage=Swarm->SpecialSwords->GetDamageDealt(V.Id);
                TArray<FLiveSpecialSkillState> Skills;Sword->GetSkillStates(Skills);
                for(const auto& K:Skills){FSpiritSkillView W;W.Name=K.Name;W.Remaining=K.Remaining;W.Cooldown=K.Cooldown;W.bPassive=K.bPassive;W.bActive=K.bActive;V.Skills.Add(W);}
            }
            Next.Add(MoveTemp(V));
        }
        Next.Sort([](const FSpiritView& A,const FSpiritView& B){return A.Special!=B.Special?A.Special>B.Special:A.RealmIndex>B.RealmIndex;});
    }
    bool bSame=Next.Num()==Model->Spirits.Num();
    for(int32 I=0;bSame&&I<Next.Num();++I)bSame=Next[I].Id==Model->Spirits[I].Id&&Next[I].Special==Model->Spirits[I].Special;
    if(bSame){Model->Spirits=MoveTemp(Next);return;}
    Model->Spirits=MoveTemp(Next);++Model->SpiritsRev;Model->Touch();
}

void UInkGameUIWidget::PullSaves()
{
    UGameInstance* GI=GetGameInstance();
    UInkSaveSubsystem* Saves=GI?GI->GetSubsystem<UInkSaveSubsystem>():nullptr;
    if(!Saves||!Model)return;
    TArray<FSaveSlot> Slots;
    for(const FInkSaveSlotInfo& Info:Saves->GetSlots())
    {
        FSaveSlot S;S.Index=Info.SlotIndex;S.bUsed=Info.bExists;S.bValid=!Info.bExists||Info.bValid;
        S.Title=FText::Format(LOCTEXT("SlotTitle","存档{0}"),FText::FromString(SlotNumerals[FMath::Clamp(Info.SlotIndex,0,5)]));
        if(!Info.bExists)S.Detail=LOCTEXT("SlotEmpty","空位 · 可保存当前行程");
        else if(!Info.bValid)S.Detail=FText::Format(LOCTEXT("SlotBroken","存档无法读取：{0}"),FText::FromString(Info.ValidationError));
        else
        {
            const int32 Minutes=FMath::FloorToInt(Info.PlayedSeconds/60.0);
            S.Detail=FText::Format(LOCTEXT("SlotDetail","{0} · 已游历 {1}时{2}分"),FText::FromString(Info.DisplayName.IsEmpty()?Info.MapName:Info.DisplayName),FText::AsNumber(Minutes/60),FText::AsNumber(Minutes%60));
            S.Time=FText::FromString(Info.SavedAt.ToString(TEXT("%m-%d %H:%M")));
        }
        Slots.Add(S);
    }
    bool bSame=Model->bLiveSaves&&Slots.Num()==Model->Saves.Num();
    for(int32 I=0;bSame&&I<Slots.Num();++I)bSame=Slots[I].bUsed==Model->Saves[I].bUsed&&Slots[I].Time.EqualTo(Model->Saves[I].Time)&&Slots[I].Detail.EqualTo(Model->Saves[I].Detail);
    if(bSame)return;
    Model->Saves=MoveTemp(Slots);Model->bLiveSaves=true;
    ++Model->SavesRev;Model->Touch();
}

void UInkGameUIWidget::PullSettings()
{
    if(!Model)return;
    if(const UGameUserSettings* Settings=UGameUserSettings::GetGameUserSettings())
    {
        const int32 Level=Settings->GetOverallScalabilityLevel();
        Model->Settings.Quality=Level<0?1:FMath::Clamp(Level,0,2);
        const float Limit=Settings->GetFrameRateLimit();
        Model->Settings.FrameRate=Limit>0.f&&Limit<45.f?30:60;
    }
    Model->Settings.bReducedMotion=Motion::IsReduced();
}

void UInkGameUIWidget::ApplySettings()
{
    if(!Model)return;
    if(UGameUserSettings* Settings=UGameUserSettings::GetGameUserSettings())
    {
        Settings->SetOverallScalabilityLevel(FMath::Clamp(Model->Settings.Quality,0,2));
        Settings->SetFrameRateLimit(float(Model->Settings.FrameRate));
        if(GEngine)GEngine->bUseFixedFrameRate=false;
        Settings->ApplyNonResolutionSettings();
        Settings->SaveSettings();
    }
    Motion::SetReduced(Model->Settings.bReducedMotion);
}

void UInkGameUIWidget::HandleCommand(FName Command)
{
    UE_LOG(LogTemp,Display,TEXT("InkUI command %s"),*Command.ToString());
    if(!Root||!Model)return;
    UGameInstance* GI=GetGameInstance();
    UInkSaveSubsystem* Saves=GI?GI->GetSubsystem<UInkSaveSubsystem>():nullptr;
    const bool bPlaying=Director&&!Director->bMenuActive;
    if(Command==TEXT("StartGame"))
    {
        if(!Director||!Director->StartGame())Root->Toast(LOCTEXT("StartFailed","暂时无法进入游戏，请稍后再试"),2);
        return;
    }
    if(Command==TEXT("ContinueGame"))
    {
        const int32 Latest=Saves?Saves->FindLatestSlot():INDEX_NONE;
        if(Latest<0){Root->Toast(LOCTEXT("NoContinue","暂无可继续的存档"),2);return;}
        HandleCommand(FName(*FString::Printf(TEXT("LoadSlot_%d"),Latest)));
        return;
    }
    if(Command==TEXT("ReturnToMainMenu")){if(Director)Director->ReturnToMainMenu();return;}
    if(Command==TEXT("QuitGame")){UKismetSystemLibrary::QuitGame(this,GetOwningPlayer(),EQuitPreference::Quit,false);return;}
    if(Command==TEXT("ApplySettings")){ApplySettings();return;}
    if(Command==TEXT("ToggleCultivation"))
    {
        APawn* Pawn=GetOwningPlayerPawn();
        UCultivationGameplayComponent* Action=Pawn?Pawn->FindComponentByClass<UCultivationGameplayComponent>():nullptr;
        if(bPlaying&&Action&&Model->Player.bLive){Action->RequestCultivationToggle();PullLiveState();return;}
        Model->Player.bCultivating=!Model->Player.bCultivating;
        Root->Toast(Model->Player.bCultivating?LOCTEXT("LocalMeditate","入定修炼 · 本地演示"):LOCTEXT("LocalRelease","收功回神"),Model->Player.bCultivating?1:0);
        return;
    }
    if(Command==TEXT("Breakthrough"))
    {
        FPlayerState& P=Model->Player;
        // In play every breakthrough goes through the 境界突破 page: minor steps, major stages and their materials, the 天劫 hand-off.
        if(P.bLive){Root->OpenPage(TEXT("Breakthrough"));return;}
        UCultivationSubsystem* Cultivation=GI?GI->GetSubsystem<UCultivationSubsystem>():nullptr;
        bool bDone=false;
        if(P.bLive&&Cultivation){bDone=Cultivation->TryAnchorBreakthrough();PullLiveState();}
        else if(P.MaxQi>0&&P.Qi>=P.MaxQi)
        {
            // Local verification only: advances the presentation model, never a save or a service ledger.
            P.RealmIndex=FMath::Min(P.RealmIndex+1,33);P.Qi=0;P.MaxQi=FMath::RoundToDouble(P.MaxQi*1.6);
            P.Realm=FText::Format(LOCTEXT("RealmN","第{0}境"),FText::AsNumber(P.RealmIndex+1));
            bDone=true;++Model->AttributesRev;Model->Touch();
        }
        if(!bDone){Root->Toast(LOCTEXT("BreakFail","尚未满足突破条件：需修为圆满与本层突破材料"),2);return;}
        FReward R;R.Icon=TEXT("lotus_orb");R.Name=P.Realm;R.Quality=EQuality::Mythic;
        Root->ShowRewards({R},LOCTEXT("BreakDone","境界突破"));
        return;
    }
    FString Verb;int32 SlotIndex=INDEX_NONE;
    if(!ParseSlotCommand(Command.ToString(),Verb,SlotIndex))return;
    if(!Saves){Root->Toast(LOCTEXT("SaveUnavailable","本地存档尚未就绪"),2);return;}
    const TArray<FInkSaveSlotInfo> Slots=Saves->GetSlots();
    const FInkSaveSlotInfo* Info=Slots.FindByPredicate([SlotIndex](const FInkSaveSlotInfo& S){return S.SlotIndex==SlotIndex;});
    const TWeakObjectPtr<UInkGameUIWidget> Self(this);
    if(Verb==TEXT("SaveSlot")||Verb==TEXT("SaveSlotConfirmed"))
    {
        if(!bPlaying){Root->Toast(LOCTEXT("SaveGameplayOnly","进入游戏后可保存当前行程"),2);return;}
        if(Verb==TEXT("SaveSlot")&&Info&&Info->bExists)
        {
            Root->Confirm(LOCTEXT("OverwriteTitle","覆盖旧迹"),FText::Format(LOCTEXT("OverwriteBody","确定覆盖存档{0}？原有记录将被当前行程替换。"),FText::FromString(SlotNumerals[FMath::Clamp(SlotIndex,0,5)])),
                FSimpleDelegate::CreateLambda([Self,SlotIndex]{if(UInkGameUIWidget* W=Self.Get())W->HandleCommand(FName(*FString::Printf(TEXT("SaveSlotConfirmed_%d"),SlotIndex)));}));
            return;
        }
        const bool bSaved=Saves->SaveSlot(SlotIndex,GetOwningPlayerPawn());
        PullSaves();
        Root->Toast(bSaved?LOCTEXT("Saved","行程已保存"):FText::FromString(Saves->LastError),bSaved?1:2);
        return;
    }
    if(Verb==TEXT("LoadSlot"))
    {
        if(!Info||!Info->bValid){Root->Toast(LOCTEXT("LoadInvalid","此存档无法读取"),2);return;}
        const bool bWasMenu=Director&&Director->bMenuActive;
        if(!Director||(bWasMenu&&!Director->StartGame())){Root->Toast(LOCTEXT("StartFailed","暂时无法进入游戏，请稍后再试"),2);return;}
        if(!Saves->LoadSlot(SlotIndex,GetOwningPlayerPawn()))
        {
            const FText Error=FText::FromString(Saves->LastError);
            if(bWasMenu)Director->ReturnToMainMenu();
            Root->Toast(Error,2);
            return;
        }
        Root->CloseAllPages();
        Root->Toast(LOCTEXT("Loaded","已读取存档，重回仙途"),1);
        return;
    }
    if(Verb==TEXT("DeleteSlot"))
    {
        if(!Info||!Info->bExists){Root->Toast(LOCTEXT("EmptySlot","尚未留迹"),2);return;}
        const bool bDeleted=Saves->DeleteSlot(SlotIndex);
        PullSaves();
        Root->Toast(bDeleted?LOCTEXT("Deleted","存档已删除"):FText::FromString(Saves->LastError),bDeleted?0:2);
    }
}

void UInkGameUIWidget::SetCharacterData(const FText& Name,const TArray<FInkCharacterAttribute>& Attributes)
{
    if(!Model)return;
    if(!Name.IsEmpty())Model->Player.Name=Name;
    TArray<FAttribute> Next;
    for(const FInkCharacterAttribute& A:Attributes){FAttribute R;R.Id=A.AttributeId;R.Label=A.Label;R.Value=A.Value;Next.Add(R);}
    bool bSame=Next.Num()==Model->Attributes.Num();
    for(int32 I=0;bSame&&I<Next.Num();++I)bSame=Next[I].Id==Model->Attributes[I].Id&&Next[I].Value.EqualTo(Model->Attributes[I].Value);
    if(bSame||!Next.Num())return;
    Model->Attributes=MoveTemp(Next);
    ++Model->AttributesRev;Model->Touch();
}

void UInkGameUIWidget::SetLeaderboardEntries(const TArray<FInkLeaderboardEntry>& Entries)
{
    if(!Model||!Entries.Num())return;
    UGameInstance* GI=GetGameInstance();
    const UCultivationSubsystem* Cultivation=GI?GI->GetSubsystem<UCultivationSubsystem>():nullptr;
    const FString AnchorId=Cultivation?Cultivation->GetAnchorId():FString();
    TMap<FName,TArray<FRankRow>> Next;
    for(const FInkLeaderboardEntry& E:Entries)
    {
        FRankRow R;R.Rank=E.Rank;R.Name=E.DisplayName;R.Realm=E.Realm;R.Metric=E.Metric;R.bSelf=!AnchorId.IsEmpty()&&E.ParticipantId==AnchorId;
        Next.FindOrAdd(E.Category).Add(R);
    }
    // Metrics refresh in place; only a changed roster or order rebuilds the page.
    bool bStructure=!Model->bLiveBoards||Next.Num()!=Model->Boards.Num();
    for(const auto& Pair:Next)
    {
        const TArray<FRankRow>* Old=Model->Boards.Find(Pair.Key);
        if(!Old||Old->Num()!=Pair.Value.Num()){bStructure=true;break;}
        for(int32 I=0;I<Old->Num();++I)if(!(*Old)[I].Name.EqualTo(Pair.Value[I].Name)){bStructure=true;break;}
    }
    Model->Boards=MoveTemp(Next);Model->bLiveBoards=true;
    if(bStructure){++Model->BoardsRev;Model->Touch();}
}

bool UInkGameUIWidget::ClickTestTarget(FName Id){return Testing::Click(Id);}

bool UInkGameUIWidget::SelectTab(FName TabBar,int32 Index){return SInkTabBar::ClickTab(TabBar,Index);}

bool UInkGameUIWidget::PressBack(){return Root&&Root->HandleBack();}

FString UInkGameUIWidget::DescribeUI() const{return Root?Root->Describe():TEXT("{}");}

bool UInkGameUIWidget::RevealTestTarget(FName Id)
{
    const TSharedPtr<SWidget> Target=Testing::Find(Id);if(!Target)return false;
    for(TSharedPtr<SWidget> W=Target->GetParentWidget();W;W=W->GetParentWidget())
        if(W->GetType()==TEXT("SScrollBox")){StaticCastSharedPtr<SScrollBox>(W)->ScrollDescendantIntoView(Target,false,EDescendantScrollDestination::Center);return true;}
    return false;
}

int32 UInkGameUIWidget::GetPlayerRealmIndex() const{return Model&&Model->Player.bLive?Model->Player.RealmIndex:INDEX_NONE;}

void UInkGameUIWidget::SetPreviewVitals(float HealthFraction,float QiFraction,bool bCultivating)
{
    if(!Model)return;
    bPreviewVitals=true;
    FPlayerState& P=Model->Player;
    if(P.MaxHealth<=0)P.MaxHealth=4150;
    if(P.MaxQi<=0)P.MaxQi=5400;
    P.Health=P.MaxHealth*FMath::Clamp(HealthFraction,0.f,1.f);P.Qi=P.MaxQi*FMath::Clamp(QiFraction,0.f,1.f);P.bCultivating=bCultivating;
}

#undef LOCTEXT_NAMESPACE

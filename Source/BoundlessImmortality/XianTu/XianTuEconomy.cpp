// Item economy of the 仙途 ledger: currencies, shops, 仙道殿 looks, 每日一签, props, the v1.4 major breakthrough with its
// quality snapshots, 武灵建设 and the 装备法宝 shell. Split from XianTuSubsystem.cpp; same class, same ledger.

#include "XianTu/XianTuSubsystem.h"
#include "XianTu/XianTuRules.h"
#include "XianTu/XianTuCatalog.h"
#include "XianTu/XianTuBreakthroughFx.h"
#include "Cultivation/CultivationSubsystem.h"
#include "Cultivation/CultivationHealthComponent.h"
#include "Combat/LiveCombat.h"
#include "LiveWeapons/SpecialSwords/LiveAnchorBuffSubsystem.h"
#include "GameFramework/Pawn.h"
#include "HAL/PlatformTime.h"

#define LOCTEXT_NAMESPACE "XianTuEconomy"

using namespace XianTuCatalog;

namespace
{
    FText N(int64 V){return FText::AsNumber(V);}

    /** Next Asia/Hong_Kong midnight in UTC seconds, the same rollover as XianTuRules::DayKey. */
    double NextDayUtc(double Utc){return (FMath::FloorToDouble((Utc+8*3600)/86400.0)+1)*86400.0-8*3600;}

    FText MajorName(int32 Major)
    {
        static const TCHAR* Names[]={TEXT("练气"),TEXT("筑基"),TEXT("结晶"),TEXT("金丹"),TEXT("元婴"),TEXT("化神"),TEXT("元神"),TEXT("炼虚"),TEXT("大乘"),TEXT("飞升"),TEXT("混元无极")};
        return FText::FromString(Names[FMath::Clamp(Major,0,10)]);
    }

    TArray<int32>& SlotsOf(FXianTuBreakPlan& P,ECategory C){return C==ECategory::Guard?P.Guard:C==ECategory::Essence?P.Essence:P.Treasure;}
    const TArray<int32>& SlotsOf(const FXianTuBreakPlan& P,ECategory C){return C==ECategory::Guard?P.Guard:C==ECategory::Essence?P.Essence:P.Treasure;}

    /** Graded counts the plan holds: [category][grade]. */
    void Tally(const FXianTuBreakPlan& P,int32 (&Out)[4][5])
    {
        FMemory::Memzero(Out);
        if(P.Core>=1&&P.Core<=4)++Out[0][P.Core];
        for(int32 C=1;C<4;++C)for(int32 G:SlotsOf(P,ECategory(C)))if(G>=1&&G<=4)++Out[C][G];
    }
}

// ------------------------------------------------------------------ wallet and shops

int64* UXianTuSubsystem::WalletSlot(ECurrency Currency)
{
    FXianTuWallet& W=State->Wallet;
    switch(Currency){case ECurrency::Jade:return &W.Jade;case ECurrency::Wuling:return &W.Wuling;case ECurrency::Soul:return &W.Soul;case ECurrency::Gold:return &W.Gold;default:return nullptr;}
}

int64 UXianTuSubsystem::Wallet(ECurrency Currency)const
{
    const FXianTuWallet& W=State->Wallet;
    switch(Currency){case ECurrency::Jade:return W.Jade;case ECurrency::Wuling:return W.Wuling;case ECurrency::Soul:return W.Soul;case ECurrency::Gold:return W.Gold;default:return 0;}
}

int32 UXianTuSubsystem::AnchorMajor()const{return LiveCombat::MajorRealm(AnchorRealm());}

void UXianTuSubsystem::RollShopDay()
{
    const FString Key=XianTuRules::DayKey(UCultivationSubsystem::UtcNow());
    if(State->ShopDay!=Key){State->ShopDay=Key;if(State->ShopBought.Num()){State->ShopBought.Reset();Touch();}}
}

int32 UXianTuSubsystem::BoughtToday(FName Sku)const
{
    return State->ShopDay==XianTuRules::DayKey(UCultivationSubsystem::UtcNow())?State->ShopBought.FindRef(Sku):0;
}

bool UXianTuSubsystem::BuySku(FName Id,int32 Quantity,FText& Out)
{
    const FSku* S=Sku(Id);if(!S){Out=LOCTEXT("NoSku","此商品已下架");return false;}
    Quantity=FMath::Clamp(Quantity,1,99);RollShopDay();
    if(AnchorMajor()<S->MinMajor){Out=FText::Format(LOCTEXT("SkuLocked","达到{0}后解锁"),MajorName(S->MinMajor));return false;}
    const int32 Bought=BoughtToday(Id);
    if(S->DailyLimit>0&&Bought+Quantity>S->DailyLimit){Out=FText::Format(LOCTEXT("SkuLimit","今日限购 {0}，已购 {1}"),N(S->DailyLimit),N(Bought));return false;}
    if(S->Item.ToString().StartsWith(TEXT("GUILD_MAT_"))&&!State->Guild.bCreated){Out=LOCTEXT("NeedGuildVault","创建武灵后，建设材料才能存入公库");return false;}
    const int64 Cost=int64(S->Price)*Quantity;int64* W=WalletSlot(S->Currency);const FCurrency& Cur=Currency(S->Currency);
    if(!W||*W<Cost){Out=FText::Format(LOCTEXT("NoMoney","{0}不足：需要 {1}，现有 {2}"),Cur.Name,N(Cost),N(W?*W:0));return false;}
    *W-=Cost;
    FXianTuReward R;R.Item=S->Item;R.Count=S->Count*Quantity;Grant({R});
    if(S->DailyLimit>0)State->ShopBought.FindOrAdd(Id)+=Quantity;
    static const TCHAR* Shops[]={TEXT("灵玉商城"),TEXT("武灵商城"),TEXT("金元商城")};
    Record(TEXT("商城"),FString::Printf(TEXT("%s · %s ×%d · %s %lld"),Shops[int32(S->Shop)],*XianTuRules::ItemName(S->Item).ToString(),R.Count,*Cur.Name.ToString(),Cost));
    Out=FText::Format(LOCTEXT("Bought","购得 {0} ×{1} · 花费 {2} {3}"),XianTuRules::ItemName(S->Item),N(R.Count),N(Cost),Cur.Name);
    Touch();return true;
}

bool UXianTuSubsystem::BuyCosmetic(FName Id,FText& Out)
{
    const FCosmetic* C=Cosmetic(Id);if(!C){Out=LOCTEXT("NoCos","此外观暂未开放");return false;}
    if(State->Cosmetics.Contains(Id)){Out=LOCTEXT("OwnedCos","已拥有此外观");return false;}
    int64* W=WalletSlot(ECurrency::Gold);
    if(!W||*W<C->Price){Out=FText::Format(LOCTEXT("NoGold","金元不足：需要 {0}，现有 {1}"),N(C->Price),N(W?*W:0));return false;}
    *W-=C->Price;State->Cosmetics.Add(Id);
    Record(TEXT("仙道殿"),FString::Printf(TEXT("兑换 %s（%s）· 金元 %d"),*C->Name.ToString(),*CosmeticKindName(C->Kind).ToString(),C->Price));
    Out=FText::Format(LOCTEXT("CosBought","兑换 {0} · 花费 {1} 金元"),C->Name,N(C->Price));Touch();return true;
}

bool UXianTuSubsystem::WearCosmetic(FName Id,FText& Out)
{
    const FCosmetic* C=Cosmetic(Id);if(!C||!State->Cosmetics.Contains(Id)){Out=LOCTEXT("NotOwnedCos","尚未拥有此外观");return false;}
    static const FName Kinds[]={TEXT("Sword"),TEXT("Frame"),TEXT("Title"),TEXT("Aura")};
    const FName Kind=Kinds[FMath::Clamp(int32(C->Kind),0,3)];
    if(State->Wearing.FindRef(Kind)==Id){State->Wearing.Remove(Kind);Out=FText::Format(LOCTEXT("CosOff","已卸下 {0}"),C->Name);}
    else{State->Wearing.Add(Kind,Id);Out=FText::Format(LOCTEXT("CosOn","已穿戴 {0} · 只改变外观，不改变任何属性"),C->Name);}
    Touch();return true;
}

// ------------------------------------------------------------------ 每日一签

EFortune UXianTuSubsystem::TodayFortune()const
{
    const FXianTuFortune& F=State->Fortune;
    if(F.Kind==0||F.DayKey!=XianTuRules::DayKey(UCultivationSubsystem::UtcNow()))return EFortune::None;
    return EFortune(FMath::Clamp<int32>(F.Kind,0,int32(EFortune::Count)-1));
}

float UXianTuSubsystem::LuckBonus()const{return Fortune(TodayFortune()).Luck;}
int32 UXianTuSubsystem::FortunePenalty()const{return Fortune(TodayFortune()).TribulationPenalty;}

double UXianTuSubsystem::SecondsToNextDay()const
{
    const double Now=UCultivationSubsystem::UtcNow();return FMath::Max(1.0,NextDayUtc(Now)-Now);
}

bool UXianTuSubsystem::DrawFortune(FText& Out)
{
    const FString Key=XianTuRules::DayKey(UCultivationSubsystem::UtcNow());
    FXianTuFortune& F=State->Fortune;
    if(TodayFortune()!=EFortune::None){Out=LOCTEXT("Drawn","今日已求签，明日子时（香港时间 0 点）可再求");return false;}
    const int32 Roll=FMath::RandRange(0,99);
    const EFortune Kind=ForcedFortune>0?EFortune(ForcedFortune):FortuneForRoll(Roll);
    const FFortune& Def=Fortune(Kind);
    F.DayKey=Key;F.Kind=uint8(Kind);F.Roll=ForcedFortune>0?-1:Roll;F.DrawnUtc=UCultivationSubsystem::UtcNow();++F.Draws;
    F.History.Insert(FString::Printf(TEXT("%s %s"),*Key,*Def.Name.ToString()),0);
    if(F.History.Num()>14)F.History.SetNum(14);
    Record(TEXT("签"),FString::Printf(TEXT("每日一签 · %s「%s」· %s"),*Def.Name.ToString(),*Def.Verse.ToString(),*Def.Effect.ToString()));
    SyncFortune();
    Out=FText::Format(LOCTEXT("DrawnNow","求得{0}「{1}」· {2}"),Def.Name,Def.Verse,Def.Effect);
    Touch();return true;
}

void UXianTuSubsystem::DebugResetFortune()
{
    State->Fortune.DayKey.Reset();State->Fortune.Kind=0;SyncFortune();Touch();
}

void UXianTuSubsystem::SyncFortune()
{
    const UCultivationSubsystem* C=Cultivation();const UCultivationHealthComponent* H=C?C->GetAnchorHealth():nullptr;
    ULiveAnchorBuffSubsystem* Buffs=H?ULiveAnchorBuffSubsystem::Get(H):nullptr;if(!Buffs)return;
    const EFortune Kind=TodayFortune();
    // 下下签 changes nothing, so it leaves no entry on the buff list.
    const bool bWant=Kind!=EFortune::None&&Kind!=EFortune::Poor&&!H->bDead;
    const bool bHas=Buffs->GetStacks(TEXT("Fortune"),TEXT("fortune"))>0;
    if(bHas&&(!bWant||AppliedFortune!=uint8(Kind))){Buffs->Remove(TEXT("Fortune"),TEXT("fortune"));AppliedFortune=0;}
    if(bWant&&(!bHas||AppliedFortune!=uint8(Kind)))
    {
        const FFortune& Def=Fortune(Kind);
        const TCHAR* Glyph=Kind==EFortune::Calamity?TEXT("灾"):Kind==EFortune::Ominous?TEXT("凶"):TEXT("吉");
        Buffs->Apply(ULiveAnchorBuffSubsystem::MakeFortune(Def.Name,Def.Effect,Glyph,Def.Color,Def.bOmen,Def.DamageTaken,float(SecondsToNextDay())));
        AppliedFortune=uint8(Kind);
    }
}

void UXianTuSubsystem::RollFortuneDrops(int32 Stages,TArray<FXianTuReward>& Rewards)
{
    const FStage* S=UpcomingStage();if(!S||Stages<=0)return;
    const float Chance=FortuneChanceBase*(1.f+LuckBonus());
    int32 Found=0;
    for(int32 I=0;I<Stages;++I)
    {
        if(FMath::FRand()>=Chance)continue;
        const int32 Pick=FMath::RandRange(0,9);
        const FName Item=MaterialKey(*S,Pick<4?ECategory::Guard:Pick<9?ECategory::Essence:ECategory::Treasure,1);
        if(FXianTuReward* E=Rewards.FindByPredicate([&](const FXianTuReward& X){return X.Item==Item;}))++E->Count;else{FXianTuReward R;R.Item=Item;R.Count=1;Rewards.Add(R);}
        ++Found;
    }
    if(Found)Record(TEXT("机缘"),FString::Printf(TEXT("历练机缘 %d 份 · 机缘率 %.0f%%（幸运 +%.0f%%）"),Found,Chance*100.f,LuckBonus()*100.f));
}

// ------------------------------------------------------------------ props

float UXianTuSubsystem::PropCooldown(FName Id)const
{
    const double* Ready=PropReady.Find(Id);return Ready?float(FMath::Max(0.0,*Ready-FPlatformTime::Seconds())):0.f;
}

bool UXianTuSubsystem::UseProp(FName Id,FText& Out)
{
    const FProp* P=Prop(Id);if(!P){Out=LOCTEXT("NoProp","此物无法直接使用");return false;}
    if(ItemCount(Id)<=0){Out=FText::Format(LOCTEXT("NoPropLeft","{0} 数量不足"),P->Name);return false;}
    if(AnchorMajor()<P->MinMajor){Out=FText::Format(LOCTEXT("PropLocked","{0} 需达到{1}方可使用"),P->Name,MajorName(P->MinMajor));return false;}
    if(const float Cd=PropCooldown(Id);Cd>0){Out=FText::Format(LOCTEXT("PropCd","{0} 冷却中，还需 {1} 秒"),P->Name,N(FMath::CeilToInt(Cd)));return false;}
    const UCultivationSubsystem* C=Cultivation();UCultivationHealthComponent* H=C?C->GetAnchorHealth():nullptr;
    ULiveAnchorBuffSubsystem* Buffs=H?ULiveAnchorBuffSubsystem::Get(H):nullptr;
    if(!H||!Buffs||H->bDead){Out=LOCTEXT("PropNoAnchor","进入游戏后可使用道具");return false;}
    const FName Key=P->EffectKey;FText Done;
    if(Key==TEXT("cleanse"))
    {
        int32 Count=0;FText Name;for(int32 I=0;I<FMath::RoundToInt(P->Magnitude);++I)if(Buffs->DispelOne(Name))++Count;
        if(!Count){Out=LOCTEXT("NothingToCleanse","身上没有可驱散的负面状态，未消耗");return false;}
        Done=FText::Format(LOCTEXT("Cleansed","驱散 {0} 层负面状态"),N(Count));
    }
    else if(Key==TEXT("cleanse_poison"))
    {
        if(!Buffs->Remove(TEXT("Poison"),TEXT("monster"))){Out=LOCTEXT("NoPoison","身上没有瘴毒，未消耗");return false;}
        Done=LOCTEXT("PoisonGone","瘴毒已清除");
    }
    else if(Key==TEXT("shield"))
    {
        const double Amount=H->MaxHealth*P->Magnitude;
        // Same kind keeps the strongest: a weaker charm never replaces a fuller barrier.
        for(const FLiveAnchorBuff& B:Buffs->GetBuffs())if(B.Id==TEXT("PropShield")&&B.Shield>=Amount){Out=LOCTEXT("StrongerShield","已有更强的符箓屏障，未消耗");return false;}
        Buffs->Apply(ULiveAnchorBuffSubsystem::MakePropShield(P->Name,Amount,P->Duration));
        Done=FText::Format(LOCTEXT("Shielded","屏障 {0}，持续 {1} 秒"),N(FMath::RoundToInt(Amount)),N(FMath::RoundToInt(P->Duration)));
    }
    else if(Key==TEXT("heal"))
    {
        if(H->Health>=H->MaxHealth){Out=LOCTEXT("FullHealth","生命已满，未消耗");return false;}
        const double Healed=H->Heal(H->MaxHealth*P->Magnitude);
        Done=FText::Format(LOCTEXT("Healed","恢复生命 {0}"),N(FMath::RoundToInt(Healed)));
    }
    else if(Key==TEXT("meditation_visual"))
    {
        Buffs->Apply(ULiveAnchorBuffSubsystem::MakePropVisual(TEXT("Incense"),P->Name,LOCTEXT("IncenseDesc","静修香雾：只有演出，无修为或属性加成。"),TEXT("香"),FLinearColor(.82f,.76f,1.f),P->Duration));
        Done=FText::Format(LOCTEXT("Incense","香雾缭绕 {0} 秒"),N(FMath::RoundToInt(P->Duration)));
    }
    else if(Key==TEXT("ceremony"))
    {
        State->CeremonyUntil=FMath::Max(State->CeremonyUntil,State->SimNow+P->Duration);
        Buffs->Apply(ULiveAnchorBuffSubsystem::MakePropVisual(TEXT("Ceremony"),P->Name,LOCTEXT("CeremonyDesc","突破与渡劫演出添一层天光灯影；无成功率加成。"),TEXT("灯"),FLinearColor(1.f,.82f,.4f),P->Duration/FMath::Clamp(State->TimeScale,.1f,60.f)));
        Done=FText::Format(LOCTEXT("Ceremony","{0} 秒内突破演出添灯影"),N(FMath::RoundToInt(P->Duration)));
    }
    else if(Key==TEXT("replay_marker"))
    {
        FXianTuBreakRecord* Latest=nullptr;
        for(FXianTuBreakRecord& R:State->BreakRecords)if(!Latest||R.Utc>Latest->Utc)Latest=&R;
        if(!Latest){Out=LOCTEXT("NoRecord","尚无大境界突破记录可标记，未消耗");return false;}
        ++Latest->Markers;
        Done=FText::Format(LOCTEXT("Marked","已标记 {0} 突破演出（本机记录）"),FText::FromName(Latest->Stage));
    }
    else{Out=LOCTEXT("PropUnknown","此道具效果尚未接入");return false;}
    Spend(Id,1);PropReady.Add(Id,FPlatformTime::Seconds()+P->Cooldown);
    Record(TEXT("道具"),FString::Printf(TEXT("使用 %s · %s"),*P->Name.ToString(),*Done.ToString()));
    Out=FText::Format(LOCTEXT("PropUsed","使用 {0} · {1}"),P->Name,Done);Touch();return true;
}

// ------------------------------------------------------------------ breakthrough

const FStage* UXianTuSubsystem::NextStage()const{return StageFrom(AnchorRealm());}

const FStage* UXianTuSubsystem::UpcomingStage()const
{
    const int32 Realm=AnchorRealm();
    for(const FStage& S:Stages())if(S.From>=Realm)return &S;
    return nullptr;
}

void UXianTuSubsystem::NormalizePlan()
{
    const UCultivationSubsystem* C=Cultivation();if(!C||C->GetParticipantSnapshot(C->GetAnchorId()).ParticipantId.IsEmpty())return;
    FXianTuBreakPlan& P=State->Plan;const FStage* S=NextStage();
    bool bChanged=false;
    if(!S){if(!P.Stage.IsNone()){P=FXianTuBreakPlan();bChanged=true;}}
    else
    {
        if(P.Stage!=S->Id){P=FXianTuBreakPlan();P.Stage=S->Id;bChanged=true;}
        for(int32 Cat=1;Cat<4;++Cat){TArray<int32>& Arr=SlotsOf(P,ECategory(Cat));if(Arr.Num()!=S->Caps[Cat]){Arr.SetNumZeroed(S->Caps[Cat]);bChanged=true;}}
    }
    if(bChanged)Touch();
}

int32 UXianTuSubsystem::FreeCount(const FStage& Stage,ECategory Category,int32 Grade)const
{
    const int32 Owned=ItemCount(MaterialKey(Stage,Category,Grade));
    if(State->Plan.Stage!=Stage.Id)return Owned;
    int32 Used[4][5];Tally(State->Plan,Used);
    return Owned-Used[FMath::Clamp(int32(Category),0,3)][FMath::Clamp(Grade,1,4)];
}

int32 UXianTuSubsystem::PlanQuality()const
{
    const FXianTuBreakPlan& P=State->Plan;const FStage* S=Stage(P.Stage);
    if(!S||P.Core<=0)return 0;
    return QualityScore(*S,P.Core,P.Guard,P.Essence,P.Treasure);
}

bool UXianTuSubsystem::PlanAvailable(FText* Reason)const
{
    auto Fail=[&](FText T){if(Reason)*Reason=T;return false;};
    const FXianTuBreakPlan& P=State->Plan;const FStage* S=Stage(P.Stage);
    if(!S||S!=NextStage())return Fail(LOCTEXT("PlanStale","材料方案与当前境界不符，请重新放入"));
    if(P.Core<=0)return Fail(FText::Format(LOCTEXT("PlanNoCore","请放入核心材料：{0}"),S->Species[0].Name));
    int32 Used[4][5];Tally(P,Used);
    for(int32 Cat=0;Cat<4;++Cat)for(int32 G=1;G<=4;++G)
        if(Used[Cat][G]>ItemCount(MaterialKey(*S,ECategory(Cat),G)))
            return Fail(FText::Format(LOCTEXT("PlanShort","{0}{1}数量不足"),GradeName(G),S->Species[Cat].Name));
    return true;
}

void UXianTuSubsystem::EnsurePlanCore()
{
    FXianTuBreakPlan& P=State->Plan;const FStage* S=NextStage();
    if(!S||P.Stage!=S->Id||P.Core>0)return;
    for(int32 G=4;G>=1;--G)if(ItemCount(MaterialKey(*S,ECategory::Core,G))>0){P.Core=G;Touch();return;}
}

void UXianTuSubsystem::ConsumePlan()
{
    FXianTuBreakPlan& P=State->Plan;const FStage* S=Stage(P.Stage);if(!S)return;
    if(P.Core>0)Spend(MaterialKey(*S,ECategory::Core,P.Core),1);
    for(int32 Cat=1;Cat<4;++Cat)for(int32& G:SlotsOf(P,ECategory(Cat))){if(G>0)Spend(MaterialKey(*S,ECategory(Cat),G),1);G=0;}
    P.Core=0;Touch();
}

bool UXianTuSubsystem::CanBreakthrough(FText* Reason)const
{
    auto Fail=[&](FText T){if(Reason)*Reason=T;return false;};
    const UCultivationSubsystem* C=Cultivation();if(!C)return Fail(LOCTEXT("BNoLedger","修为账本未就绪"));
    const FCultivationParticipantSnapshot A=C->GetParticipantSnapshot(C->GetAnchorId());
    if(A.ParticipantId.IsEmpty())return Fail(LOCTEXT("BNoAnchor","进入游戏后才能突破"));
    if(A.RealmIndex>=C->Rules.RealmNames.Num()-1)return Fail(LOCTEXT("BPeak","已至混元无极，大道圆满"));
    if(A.Cultivation<A.RequiredCultivation)return Fail(FText::Format(LOCTEXT("BQi","修为未满：{0}/{1}"),N(FMath::FloorToInt(A.Cultivation)),N(FMath::FloorToInt(A.RequiredCultivation))));
    FText Busy;if(IsBusy(&Busy))return Fail(Busy);
    const FStage* S=StageFrom(A.RealmIndex);
    if(!S)
    {
        if(A.BreakthroughMaterialCount<A.BreakthroughMaterialCost)return Fail(FText::Format(LOCTEXT("BStone","破境石不足：{0}/{1}"),N(A.BreakthroughMaterialCount),N(A.BreakthroughMaterialCost)));
        return true;
    }
    if(S->bTribulation)return Fail(LOCTEXT("BTrib","此关须渡天劫：放好材料后前往天劫"));
    if(State->Plan.Stage!=S->Id||State->Plan.Core<=0)return Fail(FText::Format(LOCTEXT("BCore","请放入核心材料：{0}"),S->Species[0].Name));
    return PlanAvailable(Reason);
}

bool UXianTuSubsystem::BreakNow(FText& Out)
{
    NormalizePlan();
    if(!CanBreakthrough(&Out))return false;
    UCultivationSubsystem* C=Cultivation();const int32 From=AnchorRealm();
    const FText To=C->GetRealmName(From+1);const FStage* S=StageFrom(From);
    if(!S)
    {
        if(!C->TryAnchorBreakthroughEx(true)){Out=LOCTEXT("BChanged","突破未成：条件已变化");return false;}
        Record(TEXT("突破"),FString::Printf(TEXT("小境界突破 %s → %s"),*C->GetRealmName(From).ToString(),*To.ToString()));
        Out=FText::Format(LOCTEXT("MinorDone","突破成功 · 晋升{0}"),To);Touch();return true;
    }
    // The ledger checked every slot above; the realm moves first so a refused breakthrough costs nothing.
    const int32 Quality=PlanQuality();
    if(!C->TryAnchorBreakthroughEx(false)){Out=LOCTEXT("BChanged2","突破未成：条件已变化");return false;}
    ConsumePlan();
    const int32 Soul=RecordBreak(*S,Quality,From);
    Out=FText::Format(LOCTEXT("MajorDone","晋升{0} · {1} Q{2} · 元魂 +{3}"),To,Bands()[BandIndex(Quality)].Name,N(Quality),N(Soul));
    Touch();return true;
}

int32 UXianTuSubsystem::RecordBreak(const FStage& S,int32 Quality,int32 FromRealm)
{
    const int32 Band=BandIndex(Quality);
    FXianTuBreakRecord* R=State->BreakRecords.FindByPredicate([&](const FXianTuBreakRecord& X){return X.Stage==S.Id;});
    const bool bBetter=!R||Quality>R->Quality;
    if(!R){R=&State->BreakRecords.AddDefaulted_GetRef();R->Stage=S.Id;}
    if(bBetter){R->Quality=Quality;R->Band=Band;R->FromRealm=FromRealm;R->Utc=UCultivationSubsystem::UtcNow();R->Receipt=NextId(TEXT("BQ"));}
    const int32 Soul=10*(S.Index+1)+5*Band;
    State->Wallet.Soul+=Soul;
    SyncQualityFactor();
    Record(TEXT("突破"),FString::Printf(TEXT("%s 大境界突破 · %s Q%d%s · 元魂 +%d · 品质因子 ×%.3f"),*S.Id.ToString(),*Bands()[Band].Name.ToString(),Quality,bBetter?TEXT(""):TEXT("（未超过旧快照）"),Soul,QualityFactor()));
    return Soul;
}

const FXianTuBreakRecord* UXianTuSubsystem::BreakRecord(FName StageId)const
{
    return State->BreakRecords.FindByPredicate([&](const FXianTuBreakRecord& X){return X.Stage==StageId;});
}

double UXianTuSubsystem::QualityFactor()const
{
    TArray<int32> Best;for(const FXianTuBreakRecord& R:State->BreakRecords)Best.Add(R.Band);
    return XianTuCatalog::QualityFactor(Best);
}

void UXianTuSubsystem::SyncQualityFactor()
{
    UCultivationSubsystem* C=Cultivation();if(!C)return;
    C->SetAnchorQualityFactor(QualityFactor());bQualitySynced=true;
}

bool UXianTuSubsystem::Refine(int32 Category,int32 Grade,FText& Out)
{
    const FStage* S=UpcomingStage();if(!S){Out=LOCTEXT("NoRefine","已无可淬炼的突破材料");return false;}
    if(Category<0||Category>3||Grade<2||Grade>4){Out=LOCTEXT("BadRefine","淬炼品阶无效");return false;}
    const ECategory Cat=ECategory(Category);const int32 Cost=RefineCost(*S,Grade);
    if(FreeCount(*S,Cat,Grade-1)<2){Out=FText::Format(LOCTEXT("RefineShort","需要 2 份{0}{1}（已放入突破方案的不计）"),GradeName(Grade-1),S->Species[Category].Name);return false;}
    if(State->Stones<Cost){Out=FText::Format(LOCTEXT("RefineStone","灵石不足：需要 {0}"),N(Cost));return false;}
    Spend(MaterialKey(*S,Cat,Grade-1),2);State->Stones-=Cost;State->Items.FindOrAdd(MaterialKey(*S,Cat,Grade))+=1;
    Record(TEXT("淬炼"),FString::Printf(TEXT("2×%s%s → 1×%s%s · 灵石 %d"),*GradeName(Grade-1).ToString(),*S->Species[Category].Name.ToString(),*GradeName(Grade).ToString(),*S->Species[Category].Name.ToString(),Cost));
    Out=FText::Format(LOCTEXT("Refined","淬炼成 {0}{1} ×1 · 花费 {2} 灵石"),GradeName(Grade),S->Species[Category].Name,N(Cost));Touch();return true;
}

bool UXianTuSubsystem::EditPlan(const FString& Verb,const TArray<FString>& Args,FText& Out)
{
    auto Int=[&](int32 I){return Args.IsValidIndex(I)?FCString::Atoi(*Args[I]):0;};
    if(Verb==TEXT("BreakNow"))return BreakNow(Out);
    if(Verb==TEXT("BreakRefine"))return Refine(Int(0),Int(1),Out);
    NormalizePlan();
    const FStage* S=NextStage();
    if(!S){Out=LOCTEXT("MinorNoPlan","小境界突破只需修为与破境石，无需放入材料");return false;}
    FXianTuBreakPlan& P=State->Plan;
    if(Verb==TEXT("BreakCore"))
    {
        // Tap cycles down through the grades in stock: best first, then lower, then empty.
        int32 Next=0;
        for(int32 G=P.Core>0?P.Core-1:4;G>=1;--G)if(ItemCount(MaterialKey(*S,ECategory::Core,G))>0){Next=G;break;}
        if(!Next&&P.Core==0){Out=FText::Format(LOCTEXT("NoCoreStock","没有可用的{0}"),S->Species[0].Name);return false;}
        P.Core=Next;
        Out=Next?FText::Format(LOCTEXT("CoreIn","放入 {0}{1}"),GradeName(Next),S->Species[0].Name):LOCTEXT("CoreOut","已取下核心材料");
        Touch();return true;
    }
    if(Verb==TEXT("BreakSlot"))
    {
        const ECategory Cat=ECategory(FMath::Clamp(Int(0),1,3));TArray<int32>& Arr=SlotsOf(P,Cat);const int32 Index=Int(1);
        if(!Arr.IsValidIndex(Index)){Out=LOCTEXT("BadSlot","槽位无效");return false;}
        if(Arr[Index]>0){Arr[Index]=0;Out=FText::Format(LOCTEXT("SlotOut","已取下{0}"),CategoryName(Cat));Touch();return true;}
        for(int32 G=4;G>=1;--G)if(FreeCount(*S,Cat,G)>0)
        {
            Arr[Index]=G;Out=FText::Format(LOCTEXT("SlotIn","放入 {0}{1}"),GradeName(G),S->Species[int32(Cat)].Name);Touch();return true;
        }
        Out=FText::Format(LOCTEXT("SlotEmpty","没有可放入的{0}：{1}"),CategoryName(Cat),S->Species[int32(Cat)].Name);return false;
    }
    if(Verb==TEXT("BreakAuto"))
    {
        if(P.Core==0)for(int32 G=4;G>=1;--G)if(ItemCount(MaterialKey(*S,ECategory::Core,G))>0){P.Core=G;break;}
        for(int32 Cat=1;Cat<4;++Cat)
        {
            TArray<int32>& Arr=SlotsOf(P,ECategory(Cat));
            for(int32& Slot:Arr)if(Slot==0)for(int32 G=4;G>=1;--G)if(FreeCount(*S,ECategory(Cat),G)>0){Slot=G;break;}
        }
        Out=FText::Format(LOCTEXT("AutoDone","已放入最佳材料 · 突破品质 Q{0}"),N(PlanQuality()));Touch();return true;
    }
    if(Verb==TEXT("BreakClear"))
    {
        P.Core=0;for(int32 Cat=1;Cat<4;++Cat)for(int32& Slot:SlotsOf(P,ECategory(Cat)))Slot=0;
        Out=LOCTEXT("Cleared","已清空材料方案");Touch();return true;
    }
    Out=LOCTEXT("UnknownBreak","未识别的突破指令");return false;
}

void UXianTuSubsystem::HandleRealmAdvanced(const FString& Id,int32 Previous,int32 Current)
{
    const UCultivationSubsystem* C=Cultivation();if(!C||Id!=C->GetAnchorId()||Current<=Previous)return;
    // Every path that moves the anchor up (breakthrough screen, tribulation, character page) gets the same show.
    XianTuBreakthroughFx::Play(AnchorPawn(),Previous,Current,CeremonyActive());
    Touch();
}

// ------------------------------------------------------------------ 武灵建设 and 装备法宝

bool UXianTuSubsystem::BuildGuild(FName Id,FText& Out)
{
    FXianTuGuild& G=State->Guild;if(!G.bCreated){Out=LOCTEXT("BuildNoGuild","创建武灵后才能建设");return false;}
    const FGuildBuilding* B=GuildBuildings().FindByPredicate([&](const FGuildBuilding& X){return X.Id==Id;});
    if(!B){Out=LOCTEXT("BuildUnknown","未知建筑");return false;}
    const int32 Level=G.BuildLevels.FindRef(Id);
    if(Level>=3){Out=FText::Format(LOCTEXT("BuildMax","{0}已建至最高档"),B->Name);return false;}
    const FBuildLevel& L=B->Levels[Level];
    if(G.Materials<L.Supply){Out=FText::Format(LOCTEXT("BuildSupply","公共资材不足：需要 {0}，现有 {1}"),N(L.Supply),N(G.Materials));return false;}
    for(const FName Mat:{L.MatA,L.MatB})if(G.Vault.FindRef(Mat)<L.MatCount)
    {Out=FText::Format(LOCTEXT("BuildMat","公库{0}不足：需要 {1}，现有 {2}"),XianTuRules::ItemName(Mat),N(L.MatCount),N(G.Vault.FindRef(Mat)));return false;}
    G.Materials-=L.Supply;G.Vault.FindOrAdd(L.MatA)-=L.MatCount;G.Vault.FindOrAdd(L.MatB)-=L.MatCount;G.BuildLevels.Add(Id,Level+1);
    FXianTuRecord Rec;Rec.Utc=UCultivationSubsystem::UtcNow();Rec.Kind=TEXT("建设");
    Rec.Text=FString::Printf(TEXT("%s 升至第 %d 档 · 资材 %d · %s/%s 各 %d"),*B->Name.ToString(),Level+1,L.Supply,*XianTuRules::ItemName(L.MatA).ToString(),*XianTuRules::ItemName(L.MatB).ToString(),L.MatCount);
    G.Records.Insert(Rec,0);
    Out=FText::Format(LOCTEXT("Built","{0} 升至第 {1} 档 · {2}"),B->Name,N(Level+1),L.Visual);Touch();return true;
}

bool UXianTuSubsystem::TryGear(FName Id,FText& Out)
{
    const FGear* G=GearItem(Id);if(!G){Out=LOCTEXT("GearUnknown","未知装备");return false;}
    if(AnchorMajor()<G->MinMajor){Out=FText::Format(LOCTEXT("GearLocked","{0} 需达到{1}"),G->Name,MajorName(G->MinMajor));return false;}
    const FName Slot(*FString::FromInt(G->Slot));
    if(State->Gear.FindRef(Slot)==Id){State->Gear.Remove(Slot);Out=FText::Format(LOCTEXT("GearOff","已卸下 {0}"),G->Name);}
    else{State->Gear.Add(Slot,Id);Out=FText::Format(LOCTEXT("GearOn","试穿 {0} · 本地验证，属性待装备制作接入后生效"),G->Name);}
    Touch();return true;
}

#undef LOCTEXT_NAMESPACE

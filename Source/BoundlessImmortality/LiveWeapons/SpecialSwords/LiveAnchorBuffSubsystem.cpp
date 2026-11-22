#include "LiveWeapons/SpecialSwords/LiveAnchorBuffSubsystem.h"
#include "Cultivation/CultivationSubsystem.h"
#include "Cultivation/CultivationHealthComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

#define LOCTEXT_NAMESPACE "LiveAnchorBuffs"

ULiveAnchorBuffSubsystem* ULiveAnchorBuffSubsystem::Get(const UObject* WorldContext)
{
    const UWorld* World=WorldContext?WorldContext->GetWorld():nullptr;
    return World?World->GetSubsystem<ULiveAnchorBuffSubsystem>():nullptr;
}

TStatId ULiveAnchorBuffSubsystem::GetStatId()const{RETURN_QUICK_DECLARE_CYCLE_STAT(ULiveAnchorBuffSubsystem,STATGROUP_Tickables);}

UCultivationHealthComponent* ULiveAnchorBuffSubsystem::AnchorHealth()const
{
    const UGameInstance* GI=GetWorld()?GetWorld()->GetGameInstance():nullptr;
    const UCultivationSubsystem* C=GI?GI->GetSubsystem<UCultivationSubsystem>():nullptr;
    UCultivationHealthComponent* H=C?C->GetAnchorHealth():nullptr;
    return H&&H->GetWorld()==GetWorld()?H:nullptr;
}

void ULiveAnchorBuffSubsystem::Apply(const FLiveAnchorBuff& Buff)
{
    if(Buff.Id.IsNone())return;
    FLiveAnchorBuff* Existing=Buffs.FindByPredicate([&](const FLiveAnchorBuff& B){return B.Id==Buff.Id&&B.Source==Buff.Source;});
    if(Existing)
    {
        const double Added=Existing->AddedAt;*Existing=Buff;Existing->AddedAt=Added;
        Existing->Remaining=Buff.Duration;
    }
    else{FLiveAnchorBuff& B=Buffs.Add_GetRef(Buff);B.Remaining=Buff.Duration;B.AddedAt=Clock;}
    ++Revision;
}

bool ULiveAnchorBuffSubsystem::Remove(FName Id,const FString& Source)
{
    const int32 Removed=Buffs.RemoveAll([&](const FLiveAnchorBuff& B){return B.Id==Id&&B.Source==Source;});
    if(Removed)++Revision;
    return Removed>0;
}

void ULiveAnchorBuffSubsystem::RemoveSource(const FString& Source)
{
    if(Buffs.RemoveAll([&](const FLiveAnchorBuff& B){return B.Source==Source;}))++Revision;
}

bool ULiveAnchorBuffSubsystem::DispelOne(FText& OutName)
{
    int32 Oldest=INDEX_NONE;
    for(int32 I=0;I<Buffs.Num();++I)if(Buffs[I].bDebuff&&Buffs[I].bDispellable&&(Oldest==INDEX_NONE||Buffs[I].AddedAt<Buffs[Oldest].AddedAt))Oldest=I;
    if(Oldest==INDEX_NONE)return false;
    OutName=Buffs[Oldest].Name;Buffs.RemoveAt(Oldest);++Revision;return true;
}

void ULiveAnchorBuffSubsystem::ApplyRandomMonsterDebuff(FRandomStream& Random)
{
    if(!AnchorHealth())return;
    Apply(MakeDebuff(Random.RandRange(0,2)));
}

double ULiveAnchorBuffSubsystem::AbsorbDamage(double Damage,bool bStarShields)
{
    if(!FMath::IsFinite(Damage)||Damage<=0)return 0;
    TArray<FString> Broken;
    for(FLiveAnchorBuff& B:Buffs)
    {
        if(Damage<=0)break;
        if(B.Shield<=0||(!bStarShields&&B.Id==TEXT("StarShield")))continue;
        const double Taken=FMath::Min(B.Shield,Damage);B.Shield-=Taken;Damage-=Taken;
        if(B.Shield<=KINDA_SMALL_NUMBER){B.Shield=0;Broken.Add(B.Source);}
    }
    if(Broken.Num())
    {
        Buffs.RemoveAll([](const FLiveAnchorBuff& B){return B.ShieldMax>0&&B.Shield<=0;});++Revision;
        for(const FString& Source:Broken)OnShieldBroken.Broadcast(Source);
    }
    return Damage;
}

double ULiveAnchorBuffSubsystem::GetDefenseMultiplier()const
{
    double Bonus=0;for(const FLiveAnchorBuff& B:Buffs)Bonus+=B.DefensePerStack*FMath::Max(1,B.Stacks);
    return FMath::Max(.2,1.0+Bonus);
}

double ULiveAnchorBuffSubsystem::GetDamageReduction()const
{
    double Reduction=0;for(const FLiveAnchorBuff& B:Buffs)Reduction+=B.DamageReduction;
    // Design cap: total damage reduction never exceeds 60%.
    return FMath::Clamp(Reduction,0.0,.6);
}

double ULiveAnchorBuffSubsystem::GetDamageTakenMultiplier()const
{
    double Extra=0;for(const FLiveAnchorBuff& B:Buffs)Extra+=B.DamageTaken;
    return FMath::Max(0.0,1.0+Extra);
}

void ULiveAnchorBuffSubsystem::GetShield(double& Current,double& Maximum)const
{
    Current=Maximum=0;for(const FLiveAnchorBuff& B:Buffs){Current+=B.Shield;Maximum+=B.ShieldMax;}
}

int32 ULiveAnchorBuffSubsystem::GetStacks(FName Id,const FString& Source)const
{
    int32 N=0;for(const FLiveAnchorBuff& B:Buffs)if(B.Id==Id&&(Source.IsEmpty()||B.Source==Source))N+=B.Stacks;return N;
}

int32 ULiveAnchorBuffSubsystem::CountDebuffs()const{int32 N=0;for(const FLiveAnchorBuff& B:Buffs)N+=B.bDebuff?1:0;return N;}

void ULiveAnchorBuffSubsystem::Tick(float DeltaTime)
{
    if(!FMath::IsFinite(DeltaTime)||DeltaTime<=0)return;
    Clock+=DeltaTime;
    UCultivationHealthComponent* Health=AnchorHealth();
    if(!Health||Health->bDead)
    {
        // A fallen anchor sheds every timed effect; sword-held auras are re-applied by their sources.
        if(Buffs.Num()&&Health&&Health->bDead){Buffs.Reset();++Revision;}
        return;
    }
    double Regen=0,Dot=0;
    for(const FLiveAnchorBuff& B:Buffs){Regen+=B.RegenCurrentPerSecond;Dot+=B.DotMaxPerSecond;}
    if(Regen>0)Health->Heal(Health->Health*Regen*DeltaTime);
    // Damage over time lands in half-second ticks so hit feedback reads as pulses, not a per-frame drain.
    DotPending+=Health->MaxHealth*Dot*DeltaTime;DotTimer+=DeltaTime;
    if(Dot<=0){DotPending=0;DotTimer=0;}
    else if(DotTimer>=.5f){Health->ApplyDirectDamage(DotPending);DotPending=0;DotTimer=0;}
    bool bExpired=false;
    for(FLiveAnchorBuff& B:Buffs)if(B.Duration>0){B.Remaining-=DeltaTime;bExpired|=B.Remaining<=0;}
    if(bExpired){Buffs.RemoveAll([](const FLiveAnchorBuff& B){return B.Duration>0&&B.Remaining<=0;});++Revision;}
}

FLiveAnchorBuff ULiveAnchorBuffSubsystem::MakeMechaSoul(const FString& Source,int32 Stacks)
{
    FLiveAnchorBuff B;B.Id=TEXT("MechaSoul");B.Source=Source;B.Name=LOCTEXT("MechaSoul","机魂");B.Glyph=TEXT("机");
    B.Description=LOCTEXT("MechaSoulDesc","无机态那每次攻击叠加一层，每层主播防御+5%；满5层化斧砸地后清空。");
    B.Color=FLinearColor(.1f,.85f,1.f);B.Stacks=FMath::Clamp(Stacks,0,5);B.MaxStacks=5;B.DefensePerStack=.05;return B;
}

FLiveAnchorBuff ULiveAnchorBuffSubsystem::MakeStarShield(const FString& Source,double Amount,float Duration)
{
    FLiveAnchorBuff B;B.Id=TEXT("StarShield");B.Source=Source;B.Name=LOCTEXT("StarShield","七星光盾");B.Glyph=TEXT("盾");
    B.Description=LOCTEXT("StarShieldDesc","七星光芒剑守护：吸收伤害，护盾破碎或时间结束后消散。");
    B.Color=FLinearColor(1.f,.9f,.45f);B.Duration=Duration;B.Shield=B.ShieldMax=FMath::Max(1.0,Amount);return B;
}

FLiveAnchorBuff ULiveAnchorBuffSubsystem::MakeStarRegen(const FString& Source,float Duration)
{
    FLiveAnchorBuff B;B.Id=TEXT("StarRegen");B.Source=Source;B.Name=LOCTEXT("StarRegen","星辉回春");B.Glyph=TEXT("愈");
    B.Description=LOCTEXT("StarRegenDesc","每秒恢复当前生命的1%。");
    B.Color=FLinearColor(.45f,1.f,.55f);B.Duration=Duration;B.RegenCurrentPerSecond=.01;return B;
}

FLiveAnchorBuff ULiveAnchorBuffSubsystem::MakeGuard(const FString& Source,double Reduction,float Duration)
{
    FLiveAnchorBuff B;B.Id=TEXT("MethodGuard");B.Source=Source;B.Name=LOCTEXT("Guard","玄罡护体");B.Glyph=TEXT("罡");
    B.Description=FText::Format(LOCTEXT("GuardDesc","玄罡体经：受到的伤害降低{0}%。"),FText::AsNumber(FMath::RoundToInt(Reduction*100)));
    B.Color=FLinearColor(.95f,.75f,.3f);B.Duration=Duration;B.DamageReduction=Reduction;return B;
}

FLiveAnchorBuff ULiveAnchorBuffSubsystem::MakeDebuff(int32 Kind)
{
    FLiveAnchorBuff B;B.bDebuff=true;B.Source=TEXT("monster");
    switch(FMath::Clamp(Kind,0,2))
    {
    case 0:B.Id=TEXT("Poison");B.Name=LOCTEXT("Poison","中毒");B.Glyph=TEXT("毒");B.Color=FLinearColor(.45f,.9f,.2f);B.Duration=8;B.DotMaxPerSecond=.008;
        B.Description=LOCTEXT("PoisonDesc","每秒损失0.8%最大生命，无视防御。");break;
    case 1:B.Id=TEXT("Burn");B.Name=LOCTEXT("Burn","灼烧");B.Glyph=TEXT("灼");B.Color=FLinearColor(1.f,.42f,.12f);B.Duration=5;B.DotMaxPerSecond=.012;
        B.Description=LOCTEXT("BurnDesc","每秒损失1.2%最大生命，无视防御。");break;
    default:B.Id=TEXT("ArmorBreak");B.Name=LOCTEXT("ArmorBreak","破甲");B.Glyph=TEXT("破");B.Color=FLinearColor(.85f,.2f,.2f);B.Duration=10;B.DefensePerStack=-.3;
        B.Description=LOCTEXT("ArmorBreakDesc","防御降低30%。");break;
    }
    return B;
}

FLiveAnchorBuff ULiveAnchorBuffSubsystem::MakeDaoHeart(double DefenseLoss,float Duration)
{
    FLiveAnchorBuff B;B.Id=TEXT("DaoHeart");B.Source=TEXT("tribulation");B.bDebuff=true;B.bDispellable=false;
    B.Name=LOCTEXT("DaoHeart","道心破碎");B.Glyph=TEXT("心");B.Color=FLinearColor(.62f,.16f,.2f);
    B.Description=FText::Format(LOCTEXT("DaoHeartDesc","渡劫失败后扛过天罚：防御降低{0}%，道心未复前无法再渡天劫；驱散无效，只能静养。"),
        FText::AsNumber(FMath::RoundToInt(DefenseLoss*100)));
    B.Duration=FMath::Max(1.f,Duration);B.DefensePerStack=-FMath::Clamp(DefenseLoss,0.0,.8);return B;
}

FLiveAnchorBuff ULiveAnchorBuffSubsystem::MakeFortune(const FText& Name,const FText& Effect,const FString& Glyph,FLinearColor Color,bool bOmen,double DamageTaken,float Duration)
{
    FLiveAnchorBuff B;B.Id=TEXT("Fortune");B.Source=TEXT("fortune");B.bDebuff=bOmen;B.bDispellable=false;
    B.Name=FText::Format(LOCTEXT("FortuneName","每日一签 · {0}"),Name);B.Glyph=Glyph;B.Color=Color;
    B.Description=FText::Format(LOCTEXT("FortuneDesc","{0}。签运持续到今日子时（香港时间 0 点），驱散无效。"),Effect);
    B.Duration=FMath::Max(1.f,Duration);B.DamageTaken=FMath::Max(0.0,DamageTaken);return B;
}

FLiveAnchorBuff ULiveAnchorBuffSubsystem::MakePropShield(const FText& Name,double Amount,float Duration)
{
    FLiveAnchorBuff B;B.Id=TEXT("PropShield");B.Source=TEXT("prop");B.Name=Name;B.Glyph=TEXT("符");
    B.Description=LOCTEXT("PropShieldDesc","符箓屏障：吸收伤害，同类屏障取最高，破碎或时间结束后消散。");
    B.Color=FLinearColor(.55f,.85f,1.f);B.Duration=Duration;B.Shield=B.ShieldMax=FMath::Max(1.0,Amount);return B;
}

FLiveAnchorBuff ULiveAnchorBuffSubsystem::MakePropVisual(FName Id,const FText& Name,const FText& Description,const FString& Glyph,FLinearColor Color,float Duration)
{
    FLiveAnchorBuff B;B.Id=Id;B.Source=TEXT("prop");B.Name=Name;B.Description=Description;B.Glyph=Glyph;B.Color=Color;B.Duration=FMath::Max(1.f,Duration);return B;
}

#undef LOCTEXT_NAMESPACE

#include "LiveWeapons/SpecialSwords/LiveSwordCodex.h"
#include "Cultivation/CultivationTypes.h"

#define LOCTEXT_NAMESPACE "LiveSwordCodex"

namespace LiveSwordCodex
{
    namespace
    {
        FString Percent(float Value,bool bFine){return bFine?FString::Printf(TEXT("%.2f%%"),Value*100.f):FString::Printf(TEXT("%.1f%%"),Value*100.f);}
        FString Growth(float Value,bool bFine){return Value>0?TEXT("+")+Percent(Value,bFine):FString(TEXT("—"));}
    }

    TArray<FStatLine> StatLines(ELiveSpecialSword Kind,int32 Realm)
    {
        using namespace LiveSpecialSwords;
        const FLiveSwordSpiritStats& S=Stats(Kind);
        TArray<FStatLine> Lines;
        Lines.Add({LOCTEXT("Attack","攻击"),FString::Printf(TEXT("%.0f"),S.Attack),S.AttackGrowth>0?FString::Printf(TEXT("+%.0f%%"),S.AttackGrowth*100.f):FString(TEXT("—")),
            FString::Printf(TEXT("%.1f"),AttackAt(Kind,Realm))});
        Lines.Add({LOCTEXT("Critical","会心率"),Percent(S.CriticalChance,false),Growth(S.CriticalGrowth,true),Percent(CriticalAt(Kind,Realm),false)});
        Lines.Add({LOCTEXT("CriticalDamage","会心伤害"),FString::Printf(TEXT("%.0f%%"),S.CriticalDamage*100.f),Growth(S.CriticalDamageGrowth,false),FString::Printf(TEXT("%.1f%%"),CriticalDamageAt(Kind,Realm)*100.f)});
        Lines.Add({LOCTEXT("Penetration","破甲"),Percent(S.ArmorPenetration,false),Growth(S.PenetrationGrowth,true),Percent(PenetrationAt(Kind,Realm),false)});
        Lines.Add({LOCTEXT("Interval","出手间隔"),FString::Printf(TEXT("%.2f秒"),S.AttackInterval),FString(TEXT("—")),FString::Printf(TEXT("%.2f秒"),S.AttackInterval)});
        return Lines;
    }

    FText Role(ELiveSpecialSword Kind)
    {
        switch(Kind)
        {
        case ELiveSpecialSword::SoulEater:return LOCTEXT("RoleSoulEater","成长型 · 吞噬让攻击永久成长，基础与成长为五剑最低");
        case ELiveSpecialSword::Inorganic:return LOCTEXT("RoleInorganic","机魂型 · 叠层护主化斧，砸地后伤害永久提升");
        case ELiveSpecialSword::DivineBlade:return LOCTEXT("RoleDivineBlade","护主型 · 驱散回血，双影蓄力释放毁天一击");
        case ELiveSpecialSword::SevenStar:return LOCTEXT("RoleSevenStar","守护型 · 百分比天雷，护盾守护，光芒剑气");
        case ELiveSpecialSword::Godslayer:return LOCTEXT("RoleGodslayer","纯被动 · 破甲、真伤、再斩，基础与成长为五剑最高");
        default:return LOCTEXT("RoleNormal","普通剑灵 · 攻击即观众剑灵攻击，随境界成长");
        }
    }

    FText BaseSummary(ELiveSpecialSword Kind)
    {
        const TArray<FStatLine> L=StatLines(Kind,0);
        return FText::Format(LOCTEXT("BaseSummary","攻击 {0} · 会心 {1} · 会心伤害 {2} · 破甲 {3} · 出手间隔 {4}"),
            FText::FromString(L[0].Base),FText::FromString(L[1].Base),FText::FromString(L[2].Base),FText::FromString(L[3].Base),FText::FromString(L[4].Base));
    }

    FText GrowthSummary(ELiveSpecialSword Kind)
    {
        const TArray<FStatLine> L=StatLines(Kind,0);
        const FString Ratio=FString::Printf(TEXT("×%.2f → ×%.2f"),LiveSpecialSwords::AttackScale(Kind,0),LiveSpecialSwords::AttackScale(Kind,33));
        return FText::Format(LOCTEXT("GrowthSummary","每境界 攻击 {0} · 会心 {1} · 会心伤害 {2} · 破甲 {3} · 同境界攻击为普通剑灵 {4}"),
            FText::FromString(L[0].Growth),FText::FromString(L[1].Growth),FText::FromString(L[2].Growth),FText::FromString(L[3].Growth),FText::FromString(Ratio));
    }

    const TArray<int32>& MajorStops(){static const TArray<int32> Stops={0,9,13,16,22,23,24,26,31,32,33};return Stops;}

    FText RealmName(int32 Realm)
    {
        static const FCultivationRuleConfig Rules;
        return Rules.RealmNames.IsValidIndex(Realm)?Rules.RealmNames[Realm]:FText::AsNumber(Realm);
    }

    FString Amount(double V)
    {
        return V>=1e8?FString::Printf(TEXT("%.2f亿"),V/1e8):V>=1e7?FString::Printf(TEXT("%.0f万"),V/1e4)
            :V>=1e4?FString::Printf(TEXT("%.1f万"),V/1e4):FString::Printf(TEXT("%.0f"),V);
    }
}

#undef LOCTEXT_NAMESPACE

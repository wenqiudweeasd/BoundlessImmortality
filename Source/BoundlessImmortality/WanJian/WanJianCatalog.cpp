#include "WanJian/WanJianCatalog.h"

namespace WanJianCatalog
{
    namespace
    {
        FSword MakeSword(const TCHAR* Id,const TCHAR* Name,const TCHAR* Role,EWanJianElement Element,EWanJianRarity Rarity,EAttack Attack,EProc Proc,int32 CooldownMs,double Coefficient)
        {
            FSword S;
            S.Id=Id;S.Name=Name;S.Role=Role;S.Element=Element;S.Rarity=Rarity;S.Attack=Attack;S.Proc=Proc;S.CooldownMs=CooldownMs;S.Coefficient=Coefficient;
            return S;
        }
    }

    const TArray<FSword>& Swords()
    {
        static const TArray<FSword> Table=[]
        {
            using E=EWanJianElement;using R=EWanJianRarity;
            TArray<FSword> T;
            FSword Starter=MakeSword(TEXT("Qingfeng"),TEXT("本命青锋"),TEXT("初始"),E::Metal,R::Common,EAttack::Homing,EProc::None,900,1.0);
            Starter.bInPool=false;
            T.Add(Starter);
            T.Add(MakeSword(TEXT("Chiyan"),TEXT("赤炎剑"),TEXT("输出"),E::Fire,R::Common,EAttack::Area,EProc::Burn,1200,0.85));
            T.Add(MakeSword(TEXT("Xuanbing"),TEXT("玄冰剑"),TEXT("控制"),E::Water,R::Common,EAttack::Area,EProc::Slow,1300,0.7));
            T.Add(MakeSword(TEXT("Jinglei"),TEXT("惊雷剑"),TEXT("连锁"),E::Wood,R::Common,EAttack::Chain,EProc::None,1400,0.75));
            T.Add(MakeSword(TEXT("Panyue"),TEXT("磐岳剑"),TEXT("防守"),E::Earth,R::Common,EAttack::Homing,EProc::ArmorBreak,1500,0.7));
            T.Add(MakeSword(TEXT("Pojia"),TEXT("破甲剑"),TEXT("精英杀手"),E::Metal,R::Fine,EAttack::Pierce,EProc::ArmorBreak,1300,1.1));
            T.Add(MakeSword(TEXT("Fenfeng"),TEXT("焚风剑"),TEXT("群伤"),E::Fire,R::Fine,EAttack::Area,EProc::Burn,1600,0.8));
            T.Add(MakeSword(TEXT("Guichao"),TEXT("归潮剑"),TEXT("回旋"),E::Water,R::Fine,EAttack::Boomerang,EProc::None,1500,0.6));
            T.Add(MakeSword(TEXT("Xuming"),TEXT("续命剑"),TEXT("辅助"),E::Wood,R::Rare,EAttack::Support,EProc::Heal,4000,0));
            T.Add(MakeSword(TEXT("Zhenyue"),TEXT("镇岳剑"),TEXT("控制"),E::Earth,R::Rare,EAttack::Homing,EProc::Knockback,1600,0.7));
            T.Add(MakeSword(TEXT("Xinghen"),TEXT("星痕剑"),TEXT("增益"),E::Metal,R::Legendary,EAttack::Homing,EProc::ElementFocus,1100,0.9));
            FSword Star=MakeSword(TEXT("QiXing"),TEXT("七星光芒剑"),TEXT("限定核心"),E::None,R::Legendary,EAttack::Support,EProc::StarGuard,3500,0);
            Star.MaxInLineup=1;
            T.Add(Star);
            return T;
        }();
        return Table;
    }

    const FSword* Sword(FName Id){return Swords().FindByPredicate([Id](const FSword& S){return S.Id==Id;});}
    FName StarterSword(){return TEXT("Qingfeng");}
    FName SevenStarSword(){return TEXT("QiXing");}

    const TArray<FWorld>& Worlds()
    {
        static const TArray<FWorld> Table=
        {
            {1,TEXT("青云山"),TEXT("入门 · 灵泉"),true},
            {2,TEXT("落霞谷"),TEXT("采药 · 木水"),true},
            {3,TEXT("玄铁岭"),TEXT("锻造 · 破甲"),false},
            {4,TEXT("赤炎荒域"),TEXT("火系 · 灼烧"),false},
            {5,TEXT("北冥寒渊"),TEXT("水系 · 冰封"),false},
            {6,TEXT("万木秘境"),TEXT("木系 · 复苏"),false},
            {7,TEXT("天雷绝境"),TEXT("雷劫 · 连锁"),false},
            {8,TEXT("太虚仙域"),TEXT("终局 · 五行"),false},
        };
        return Table;
    }

    const TArray<FMethod>& Methods()
    {
        static const TArray<FMethod> Table=[]
        {
            auto Make=[](const TCHAR* Id,const TCHAR* Name,const TCHAR* Effect,int32 Critical,int32 Fragments,bool bFirst)
            {
                FMethod M;M.Id=Id;M.Name=Name;M.Effect=Effect;M.Critical=Critical;M.Fragments=Fragments;M.bFirstRelease=bFirst;
                return M;
            };
            TArray<FMethod> T;
            T.Add(Make(TEXT("Jifeng"),TEXT("疾锋剑诀"),TEXT("剑灵攻速+4%，每10临界值再+1%（至多+10%）"),0,20,true));
            T.Add(Make(TEXT("Xuangang"),TEXT("玄罡体经"),TEXT("基础减伤+5%，每10临界值护盾+2%（至多20%）"),2,20,true));
            T.Add(Make(TEXT("Huixiang"),TEXT("回响诀"),TEXT("每第5次命中追加回响，临界20/50增加目标"),5,30,true));
            T.Add(Make(TEXT("Fentian"),TEXT("焚天剑诀"),TEXT("灼烧目标受到额外火伤"),12,40,false));
            T.Add(Make(TEXT("Wanjian"),TEXT("万剑归宗"),TEXT("周期性剑影爆发"),35,60,false));
            return T;
        }();
        return Table;
    }

    const FMethod* Method(FName Id){return Methods().FindByPredicate([Id](const FMethod& M){return M.Id==Id;});}
    FName MethodFragmentAsset(FName Method){return FName(*FString::Printf(TEXT("Frag_%s"),*Method.ToString()));}

    const TArray<FDungeon>& Dungeons()
    {
        static const TArray<FDungeon> Table=[]
        {
            auto Make=[](const TCHAR* Id,const TCHAR* Name,const TCHAR* Focus,double Chance,int32 FixedEvery,bool bOpen)
            {
                FDungeon D;D.Id=Id;D.Name=Name;D.Focus=Focus;D.BaseChance=Chance;D.FixedEvery=FixedEvery;D.bOpen=bOpen;
                return D;
            };
            TArray<FDungeon> T;
            T.Add(Make(TEXT("Breakthrough"),TEXT("破境试炼"),TEXT("破境石与突破专属材料"),0.35,8,true));
            T.Add(Make(TEXT("Scripture"),TEXT("剑诀残卷"),TEXT("功法残页"),0.30,8,true));
            T.Add(Make(TEXT("Abyss"),TEXT("五行深渊"),TEXT("灵根研习材料"),0.25,10,false));
            T.Add(Make(TEXT("Treasure"),TEXT("法宝秘境"),TEXT("法宝碎片"),0.20,10,false));
            return T;
        }();
        return Table;
    }

    const FDungeon* Dungeon(FName Id){return Dungeons().FindByPredicate([Id](const FDungeon& D){return D.Id==Id;});}

    const TArray<FBuilding>& Buildings()
    {
        static const TArray<FBuilding> Table=
        {
            {TEXT("Garden"),TEXT("灵圃"),TEXT("每10分钟产2灵草，仓储上限24")},
            {TEXT("Forge"),TEXT("炼器坊"),TEXT("10玄铁+40灵石，3小时产1剑魂晶")},
            {TEXT("Library"),TEXT("藏经阁"),TEXT("灵石+灵草推进五行研究")},
        };
        return Table;
    }

    const TArray<FSynergy>& Synergies()
    {
        using E=EWanJianElement;
        static const TArray<FSynergy> Table=
        {
            {E::Metal,E::Fire,TEXT("熔锋"),TEXT("命中灼烧敌人时附加10%破甲3秒")},
            {E::Water,E::Wood,TEXT("生机"),TEXT("控制敌人后恢复少量生命，8秒一次")},
            {E::Water,E::Earth,TEXT("凝障"),TEXT("水系命中后触发小护盾")},
            {E::Fire,E::Wood,TEXT("焚生"),TEXT("灼烧命中后小范围蔓延")},
            {E::Metal,E::Earth,TEXT("镇锋"),TEXT("破甲目标被土系命中时延长减速")},
        };
        return Table;
    }

    const TCHAR* ElementName(EWanJianElement Element)
    {
        switch(Element)
        {
        case EWanJianElement::Metal:return TEXT("金");
        case EWanJianElement::Wood:return TEXT("木");
        case EWanJianElement::Water:return TEXT("水");
        case EWanJianElement::Fire:return TEXT("火");
        case EWanJianElement::Earth:return TEXT("土");
        default:return TEXT("无");
        }
    }

    int32 MeltShards(EWanJianRarity Rarity)
    {
        switch(Rarity)
        {
        case EWanJianRarity::Fine:return 15;
        case EWanJianRarity::Rare:return 40;
        case EWanJianRarity::Legendary:return 120;
        default:return 5;
        }
    }
}

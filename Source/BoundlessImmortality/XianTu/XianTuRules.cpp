#include "XianTu/XianTuRules.h"
#include "XianTu/XianTuCatalog.h"
#include "Combat/LiveCombat.h"

#define LOCTEXT_NAMESPACE "XianTuRules"

namespace XianTuRules
{
    namespace{FText T(const TCHAR* S){return FText::FromString(S);}}

    const TArray<FRegion>& Regions()
    {
        auto R=[](const TCHAR* Id,const TCHAR* Name,const TCHAR* Theme,const TCHAR* Phase,bool bOpen,int32 Scene){FRegion X;X.Id=Id;X.Name=T(Name);X.Theme=T(Theme);X.Phase=T(Phase);X.bOpen=bOpen;X.Scene=Scene;return X;};
        static const TArray<FRegion> List={
            R(TEXT("qingyun"),TEXT("青云山"),TEXT("入门 · 护阵 · 灵泉"),TEXT("P1"),true,4),
            R(TEXT("luoxia"),TEXT("落霞谷"),TEXT("路线 · 采药"),TEXT("P2"),true,3),
            R(TEXT("xuantie"),TEXT("玄铁岭"),TEXT("破障 · 锻材"),TEXT("P2"),true,5),
            R(TEXT("xuanbing"),TEXT("玄冰原"),TEXT("环境 · 稳阵"),TEXT("P4"),false,2),
            R(TEXT("yaoshou"),TEXT("妖兽林"),TEXT("标记 · 协击"),TEXT("P4"),false,1),
            R(TEXT("chiyan"),TEXT("赤炎地"),TEXT("爆发 · 时机"),TEXT("P4"),false,6),
            R(TEXT("xingyun"),TEXT("星陨海"),TEXT("法域 · 回响"),TEXT("P4"),false,7),
            R(TEXT("taixu"),TEXT("太虚境"),TEXT("多阶段 · 问道"),TEXT("P4"),false,0)};
        return List;
    }

    const TArray<FPreset>& Presets()
    {
        auto P=[](const TCHAR* Id,const TCHAR* Name,int32 Reference,float Normal,float Obstacle,float Boss){FPreset X;X.Id=Id;X.Name=T(Name);X.Reference=Reference;X.Normal=Normal;X.Obstacle=Obstacle;X.Boss=Boss;return X;};
        static const TArray<FPreset> List={
            P(TEXT("ENC_SOLO"),TEXT("独行历练"),0,20,40,360),
            P(TEXT("ENC_SMALL"),TEXT("小型历练"),5,120,240,2160),
            P(TEXT("ENC_MEDIUM"),TEXT("中型历练"),20,420,840,7560),
            P(TEXT("ENC_LARGE"),TEXT("大型历练"),80,1620,3240,29160)};
        return List;
    }

    const FPreset& Preset(FName Id){for(const FPreset& P:Presets())if(P.Id==Id)return P;return Presets()[0];}

    const TArray<FItem>& Items()
    {
        auto I=[](const TCHAR* Id,const TCHAR* Name,const TCHAR* Icon,int32 Quality,const TCHAR* Desc,bool bTradable){FItem X;X.Id=Id;X.Name=T(Name);X.Icon=Icon;X.Quality=Quality;X.Description=T(Desc);X.bTradable=bTradable;return X;};
        static const TArray<FItem> List={
            I(TEXT("CUR_STONE"),TEXT("灵石"),TEXT("crystal"),0,TEXT("主播个人货币，用于突破、研习与坊市。"),false),
            I(TEXT("MAT_BREAK"),TEXT("破境石"),TEXT("crystal_gold"),2,TEXT("突破所需材料，与角色页突破共用同一存量。"),false),
            I(TEXT("MAT_HERB_B"),TEXT("灵草·绑定"),TEXT("lotus"),1,TEXT("世界历练产出的绑定灵草，用于配方，不可交易。"),false),
            I(TEXT("MAT_HERB_T"),TEXT("灵草"),TEXT("lotus"),2,TEXT("可交易灵草，可寄售、回收或绑定为配方材料。"),true),
            I(TEXT("MAT_IRON_B"),TEXT("玄铁·绑定"),TEXT("sword_emblem"),1,TEXT("世界历练产出的绑定玄铁，用于配方，不可交易。"),false),
            I(TEXT("MAT_IRON_T"),TEXT("玄铁"),TEXT("sword_emblem"),2,TEXT("可交易玄铁，可寄售、回收或绑定为配方材料。"),true),
            I(TEXT("MAT_PAGE"),TEXT("功法残页"),TEXT("scroll"),2,TEXT("研习功法所需残页，两页加五十灵石可制成一卷研习卷。"),false),
            I(TEXT("ITEM_STUDY"),TEXT("研习卷"),TEXT("golden_scroll"),3,TEXT("功法定向研习进度一份。"),false)};
        static const TArray<FItem> All=[]
        {
            // The v1.4 economy: four currencies, graded breakthrough materials, props and the guild vault's materials.
            using namespace XianTuCatalog;
            TArray<FItem> Out=List;
            for(const FCurrency& C:Currencies())
            {
                FItem X;X.Id=C.Key;X.Name=C.Name;X.Icon=C.Icon;X.Quality=C.Id==ECurrency::Gold?4:C.Id==ECurrency::Soul?3:2;X.Kind=TEXT("Currency");
                X.Description=FText::Format(LOCTEXT("CurrencyDesc","来源：{0}。用途：{1}。"),C.Source,C.Use);Out.Add(X);
            }
            for(const FStage& S:Stages())for(int32 C=0;C<int32(ECategory::Count);++C)for(int32 G=1;G<=4;++G)
            {
                const FSpecies& Sp=S.Species[C];FItem X;X.Id=MaterialKey(S,ECategory(C),G);
                X.Name=FText::Format(LOCTEXT("GradedName","{0}{1}"),GradeName(G),Sp.Name);X.Icon=Sp.Icon;X.Quality=G;
                X.Description=FText::Format(LOCTEXT("MaterialDesc","{0} · {1}（{2}·{3}）。{4}。单件评分 {5}。"),
                    CategoryName(ECategory(C)),FText::FromName(S.Id),S.Region,S.Subarea,CategoryRole(ECategory(C)),FText::AsNumber(GradeScore(ECategory(C),G)));
                Out.Add(X);
            }
            for(const FProp& P:Props())
            {
                FItem X;X.Id=P.Id;X.Name=P.Name;X.Icon=P.Icon;X.Quality=P.Quality;X.Kind=TEXT("Consumable");
                X.Description=FText::Format(LOCTEXT("PropDesc","{0}冷却 {1} 秒。"),P.Effect,FText::AsNumber(FMath::RoundToInt(P.Cooldown)));Out.Add(X);
            }
            for(const FGuildMaterial& G:GuildMaterials())
            {
                FItem X;X.Id=G.Id;X.Name=G.Name;X.Icon=G.Icon;X.Quality=2;
                X.Description=LOCTEXT("GuildMatDesc","武灵建设材料，直接存入本武灵公库，用于武灵建设；不可转为个人物品。");Out.Add(X);
            }
            return Out;
        }();
        return All;
    }

    const FItem* Item(FName Id){return Items().FindByPredicate([&](const FItem& I){return I.Id==Id;});}
    FText ItemName(FName Id){const FItem* I=Item(Id);return I?I->Name:FText::FromName(Id);}

    const TArray<FNode>& Nodes()
    {
        auto N=[](const TCHAR* Id,const TCHAR* Name,float Qi,const TCHAR* Item,int32 Count,int32 Interval,const TCHAR* Owner,int32 Scene){FNode X;X.Id=Id;X.Name=T(Name);X.QiBonus=Qi;X.Item=Item;X.Count=Count;X.Interval=Interval;X.Owner=T(Owner);X.Scene=Scene;return X;};
        static const TArray<FNode> List={
            N(TEXT("NODE_QI"),TEXT("灵泉"),.2f,TEXT(""),0,1,TEXT("主播与在场正式成员修炼 +20%"),4),
            N(TEXT("NODE_STONE"),TEXT("灵石矿"),0,TEXT("CUR_STONE"),20,600,TEXT("主播 · 每有效600秒 20灵石"),3),
            N(TEXT("NODE_HERB"),TEXT("灵草园"),0,TEXT("MAT_HERB_B"),2,600,TEXT("主播 · 每有效600秒 2灵草"),2),
            N(TEXT("NODE_IRON"),TEXT("玄铁脉"),0,TEXT("MAT_IRON_B"),2,600,TEXT("主播 · 每有效600秒 2玄铁"),5),
            N(TEXT("NODE_PAGE"),TEXT("残卷遗址"),0,TEXT("MAT_PAGE"),1,1200,TEXT("主播 · 每有效1200秒 1残页"),0)};
        return List;
    }

    const FNode* Node(FName Id){return Nodes().FindByPredicate([&](const FNode& N){return N.Id==Id;});}

    const TArray<FMethod>& Methods()
    {
        auto M=[](const TCHAR* Id,const TCHAR* Name,const TCHAR* Route,bool bOpen,const TCHAR* Main,const TCHAR* Support,const TCHAR* Live){FMethod X;X.Id=Id;X.Name=T(Name);X.Route=T(Route);X.bOpen=bOpen;X.Main=T(Main);X.Support=T(Support);X.Live=T(Live);return X;};
        static const TArray<FMethod> List={
            M(TEXT("METHOD_EDGE"),TEXT("疾锋剑诀"),TEXT("剑修"),true,TEXT("本命剑每第5次有效命中强化为1.8倍伤害"),TEXT("完整蓄力（本命剑静候3秒）后下一击增伤10%"),TEXT("剑意计数可见")),
            M(TEXT("METHOD_STAR"),TEXT("星罗法诀"),TEXT("法修"),false,TEXT("每20秒开6秒法域，域内伤害提高20%"),TEXT("有效范围目标额外5%伤害"),TEXT("主播移动影响站位")),
            M(TEXT("METHOD_GUARD"),TEXT("玄罡体经"),TEXT("护体"),true,TEXT("每30秒主动护体6秒，减伤20%"),TEXT("主播生命低于50%时防御提高10%"),TEXT("作用于主播，不给剑灵造生命值")),
            M(TEXT("METHOD_ECHO"),TEXT("回响诀"),TEXT("魂修"),true,TEXT("本命剑每第4次有效命中追加0.5倍延迟伤害"),TEXT("每20秒下一击追加0.1倍延迟伤害"),TEXT("延迟伤害不再次触发回响")),
            M(TEXT("METHOD_PACT"),TEXT("合契经"),TEXT("协契"),false,TEXT("主播标记20秒，正式成员命中标记目标增伤15%"),TEXT("每20秒第一次命中标记目标增伤5%"),TEXT("用剑灵协击适配御兽思路")),
            M(TEXT("METHOD_SURGE"),TEXT("燃脉诀"),TEXT("燃息"),false,TEXT("消耗当前生命10%，伤害提高25%持续6秒，间隔30秒"),TEXT("主播生命低于50%时本命剑增伤10%"),TEXT("生命不足时拒绝消耗")),
            M(TEXT("METHOD_ETHER"),TEXT("百蚀经"),TEXT("蚀灵"),false,TEXT("命中加蚀灵，每秒0.05倍基础伤害，最多5层"),TEXT("每20秒首次命中附加1层"),TEXT("每秒至多结算一次"))};
        return List;
    }

    const FMethod* Method(FName Id){return Methods().FindByPredicate([&](const FMethod& M){return M.Id==Id;});}

    const TArray<FBuilding>& Buildings()
    {
        auto B=[](const TCHAR* Id,const TCHAR* Name,const TCHAR* Desc){FBuilding X;X.Id=Id;X.Name=T(Name);X.Description=T(Desc);X.Cost=100;return X;};
        static const TArray<FBuilding> List={
            B(TEXT("BUILD_BANNER"),TEXT("剑阵旗台"),TEXT("山门竖起剑阵大旗，解锁旗台外观层级。")),
            B(TEXT("BUILD_STELE"),TEXT("历练碑"),TEXT("镌刻同道历练足迹，解锁石碑外观层级。")),
            B(TEXT("BUILD_COURT"),TEXT("共修庭"),TEXT("同修之所，解锁庭院外观层级；不叠加修炼倍率。"))};
        return List;
    }

    TArray<FXianTuReward> StageRewards(int32 Stage)
    {
        auto R=[](const TCHAR* Item,int32 Count){FXianTuReward X;X.Item=Item;X.Count=Count;return X;};
        switch(Stage)
        {
        case 1:return {R(TEXT("CUR_STONE"),16),R(TEXT("MAT_BREAK"),3),R(TEXT("MAT_HERB_B"),1),R(TEXT("MAT_IRON_B"),1)};
        case 2:return {R(TEXT("CUR_STONE"),24),R(TEXT("MAT_BREAK"),3),R(TEXT("MAT_HERB_B"),2),R(TEXT("MAT_IRON_B"),1),R(TEXT("MAT_PAGE"),1)};
        case 3:return {R(TEXT("CUR_STONE"),40),R(TEXT("MAT_BREAK"),6),R(TEXT("MAT_HERB_B"),2),R(TEXT("MAT_HERB_T"),1),R(TEXT("MAT_IRON_B"),1),R(TEXT("MAT_IRON_T"),1),R(TEXT("MAT_PAGE"),1)};
        default:return {};
        }
    }

    FText StageName(FName Route,int32 Stage)
    {
        const bool bGather=Route==TEXT("gather");
        switch(Stage)
        {
        case 1:return bGather?LOCTEXT("Gather1","采集 · 击破灵植守卫"):LOCTEXT("Guard1","护阵 · 击退来袭妖物");
        case 2:return bGather?LOCTEXT("Gather2","护阵 · 守住采集成果"):LOCTEXT("Guard2","破障 · 摧毁结界阵眼");
        case 3:return LOCTEXT("Boss","首领 · 斩落妖王");
        default:return LOCTEXT("Prepare","准备 · 迎接新人");
        }
    }

    FText LevelLabel(int32 Level)
    {
        static const TCHAR* Labels[]={TEXT("黄"),TEXT("玄"),TEXT("地"),TEXT("天"),TEXT("仙"),TEXT("神")};
        return T(Labels[FMath::Clamp(Level,1,6)-1]);
    }

    int32 StudyNeeded(int32 Level){static const int32 Needs[]={0,0,1,2,3,4,6};return Needs[FMath::Clamp(Level,0,6)];}
    float LevelScale(int32 Level){return Level>=6?1.2f:Level>=3?1.1f:1.f;}

    bool IsTribulation(int32 From){return From==21||From==22||From==23||From==25||From==30||From==31||From==32;}

    int32 StarWardChance(int32 GuardRealm,int32 FromRealm)
    {
        if(GuardRealm<0)return 0;
        const int32 Gap=LiveCombat::MajorRealm(GuardRealm)-LiveCombat::MajorRealm(FromRealm+1);
        return Gap<1?0:10000-FMath::RoundToInt(10000.0/double(1<<FMath::Min(Gap,13)));
    }

    int32 SupportSlots(int32 Realm)
    {
        if(Realm>=31)return 6;if(Realm>=23)return 5;if(Realm>=16)return 4;if(Realm>=9)return 3;return 2;
    }

    void MarketRange(FName Item,int32& Min,int32& Max,int32& Recycle)
    {
        if(Item==TEXT("MAT_IRON_T")){Min=6;Max=18;Recycle=6;}else{Min=4;Max=12;Recycle=4;}
    }

    FString DayKey(double Utc)
    {
        // Daily caps roll over at Asia/Hong_Kong 00:00.
        const FDateTime Local=FDateTime(1970,1,1)+FTimespan::FromSeconds(Utc+8*3600);
        return Local.ToString(TEXT("%Y-%m-%d"));
    }

    FText Clock(double Seconds)
    {
        const int32 S=FMath::Max(0,FMath::CeilToInt(Seconds));
        return FText::FromString(S>=3600?FString::Printf(TEXT("%d:%02d:%02d"),S/3600,(S/60)%60,S%60):FString::Printf(TEXT("%d:%02d"),S/60,S%60));
    }

    float NameLength(const FString& Name)
    {
        float Length=0;for(const TCHAR C:Name)Length+=C<128?.5f:1.f;return Length;
    }

    FText StateText(EXianTuWorldState S)
    {
        switch(S)
        {
        case EXianTuWorldState::Preparing:return LOCTEXT("WPrep","准备中");
        case EXianTuWorldState::Running:return LOCTEXT("WRun","历练中");
        case EXianTuWorldState::Settling:return LOCTEXT("WSettle","结算中");
        case EXianTuWorldState::Completed:return LOCTEXT("WDone","已完成");
        case EXianTuWorldState::Interrupted:return LOCTEXT("WInt","战败恢复");
        default:return LOCTEXT("WIdle","未开始");
        }
    }
    FText StateText(EXianTuRaceState S)
    {
        switch(S)
        {
        case EXianTuRaceState::Queue:return LOCTEXT("RQueue","匹配中");
        case EXianTuRaceState::Matched:return LOCTEXT("RMatch","已匹配");
        case EXianTuRaceState::Ready:return LOCTEXT("RReady","准备期");
        case EXianTuRaceState::Running:return LOCTEXT("RRun","挑战中");
        case EXianTuRaceState::Frozen:return LOCTEXT("RFrozen","计分冻结");
        case EXianTuRaceState::Settled:return LOCTEXT("RSettled","已结算");
        case EXianTuRaceState::Cancelled:return LOCTEXT("RCancel","已取消");
        default:return LOCTEXT("RNone","未报名");
        }
    }
    FText StateText(EXianTuLeaseState S)
    {
        switch(S)
        {
        case EXianTuLeaseState::Active:return LOCTEXT("LActive","生效中");
        case EXianTuLeaseState::Expired:return LOCTEXT("LExpired","已到期");
        case EXianTuLeaseState::Revoked:return LOCTEXT("LRevoked","已撤销");
        default:return LOCTEXT("LPending","待生效");
        }
    }
    FText StateText(EXianTuTribState S)
    {
        switch(S)
        {
        case EXianTuTribState::Prepare:return LOCTEXT("TPrep","准备");
        case EXianTuTribState::Trial:return LOCTEXT("TTrial","渡劫中");
        case EXianTuTribState::Frozen:return LOCTEXT("TFrozen","对账中");
        case EXianTuTribState::Backlash:return LOCTEXT("TBacklash","天罚");
        case EXianTuTribState::Succeeded:return LOCTEXT("TOk","渡劫成功");
        case EXianTuTribState::Failed:return LOCTEXT("TFail","渡劫失败");
        case EXianTuTribState::Cancelled:return LOCTEXT("TCancel","已取消");
        default:return LOCTEXT("TNone","未开始");
        }
    }
    FText StateText(EXianTuOrderState S)
    {
        switch(S)
        {
        case EXianTuOrderState::Sold:return LOCTEXT("OSold","已成交");
        case EXianTuOrderState::Cancelled:return LOCTEXT("OCancel","已撤单");
        case EXianTuOrderState::Expired:return LOCTEXT("OExpired","已过期");
        default:return LOCTEXT("OListed","挂牌中");
        }
    }
}

#undef LOCTEXT_NAMESPACE

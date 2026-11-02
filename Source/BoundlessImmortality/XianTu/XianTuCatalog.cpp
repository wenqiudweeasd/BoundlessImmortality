#include "XianTu/XianTuCatalog.h"

#define LOCTEXT_NAMESPACE "XianTuCatalog"

namespace XianTuCatalog
{
    namespace
    {
        FText T(const TCHAR* S){return FText::FromString(S);}
        FLinearColor Hex(const TCHAR* S){return FLinearColor::FromSRGBColor(FColor::FromHex(S));}
        const TCHAR* CategoryLetter[]={TEXT("C"),TEXT("G"),TEXT("E"),TEXT("T")};
    }

    // ------------------------------------------------------------------ currencies

    const TArray<FCurrency>& Currencies()
    {
        auto C=[](ECurrency Id,const TCHAR* Key,const TCHAR* Name,const TCHAR* Icon,const TCHAR* Source,const TCHAR* Use,const TCHAR* Tint)
        {FCurrency X;X.Id=Id;X.Key=Key;X.Name=T(Name);X.Icon=Icon;X.Source=T(Source);X.Use=T(Use);X.Tint=Hex(Tint);return X;};
        static const TArray<FCurrency> List={
            C(ECurrency::Jade,TEXT("CUR_JADE"),TEXT("灵玉"),TEXT("jade_orb"),TEXT("世界历练结算、每日一签与仙途活动"),TEXT("灵玉商城：突破材料、道具与杂货"),TEXT("5FC9A8")),
            C(ECurrency::Wuling,TEXT("CUR_WULING"),TEXT("武灵"),TEXT("swords"),TEXT("武灵正式成员共同完成的历练与建设"),TEXT("武灵商城：建设材料、修行与护持"),TEXT("D9733F")),
            C(ECurrency::Soul,TEXT("CUR_SOUL"),TEXT("元魂"),TEXT("vortex"),TEXT("大境界突破与渡劫成功凝结"),TEXT("魂炼与元魂商城（后续开放）"),TEXT("9A7BF0")),
            C(ECurrency::Gold,TEXT("CUR_GOLD"),TEXT("金元"),TEXT("orb_gold"),TEXT("本地验证发放（未接入充值）"),TEXT("仙道殿 · 金元商城：兑换外观"),TEXT("F2C14E"))};
        return List;
    }

    const FCurrency& Currency(ECurrency Id){return Currencies()[FMath::Clamp(int32(Id),0,int32(ECurrency::Count)-1)];}

    bool ParseCurrency(FName Key,ECurrency& Out)
    {
        for(const FCurrency& C:Currencies())if(C.Key==Key){Out=C.Id;return true;}
        return false;
    }

    // ------------------------------------------------------------------ breakthrough materials

    FText CategoryName(ECategory C)
    {
        switch(C)
        {
        case ECategory::Core:return LOCTEXT("Core","核心材料");
        case ECategory::Guard:return LOCTEXT("Guard","避凶吉兆");
        case ECategory::Essence:return LOCTEXT("Essence","滋润灵物");
        default:return LOCTEXT("Treasure","天材地宝");
        }
    }

    FText CategoryRole(ECategory C)
    {
        switch(C)
        {
        case ECategory::Core:return LOCTEXT("CoreRole","大境界破境资格，整次只消耗一份；品阶决定核心评分");
        case ECategory::Guard:return LOCTEXT("GuardRole","护道：提升评分、缩短突破后虚弱；不增加渡劫成功率");
        case ECategory::Essence:return LOCTEXT("EssenceRole","滋养经脉：按投入总权重计分，数量上限公开");
        default:return LOCTEXT("TreasureRole","天地共鸣：可选一件，直接计入奇珍评分");
        }
    }

    FText GradeName(int32 Grade)
    {
        static const TCHAR* Names[]={TEXT("下品"),TEXT("中品"),TEXT("上品"),TEXT("极品")};
        return T(Names[FMath::Clamp(Grade,1,4)-1]);
    }

    int32 GradeScore(ECategory Category,int32 Grade)
    {
        static const int32 Scores[4][4]={{10,18,26,35},{3,6,10,15},{6,14,22,30},{4,9,15,20}};
        if(Grade<1||Grade>4)return 0;
        return Scores[FMath::Clamp(int32(Category),0,3)][Grade-1];
    }

    int32 CategoryCap(ECategory Category){static const int32 Caps[]={35,15,30,20};return Caps[FMath::Clamp(int32(Category),0,3)];}

    const TArray<FStage>& Stages()
    {
        struct FRow{const TCHAR* Region;const TCHAR* Sub;int32 From,MinMajor,Guard,Essence;bool bTrib;const TCHAR* Names[4];const TCHAR* Icons[4];};
        static const FRow Rows[]={
            {TEXT("青云山"),TEXT("凝脉谷"),8,0,3,3,false,{TEXT("筑基丹"),TEXT("凝脉草"),TEXT("百年灵髓"),TEXT("天道筑基石")},{TEXT("pill"),TEXT("lotus"),TEXT("water_orb"),TEXT("crystal_gold")}},
            {TEXT("玄铁岭"),TEXT("凝晶地窟"),12,1,2,4,false,{TEXT("凝晶丹"),TEXT("固脉玉液"),TEXT("地脉晶髓"),TEXT("无垢灵晶")},{TEXT("crystal"),TEXT("water_orb"),TEXT("crystal"),TEXT("crystal_gold")}},
            {TEXT("赤炎地"),TEXT("五行丹窟"),15,2,2,5,false,{TEXT("结丹灵液"),TEXT("护脉丹"),TEXT("金灵精"),TEXT("天罡丹髓")},{TEXT("water_orb"),TEXT("heart"),TEXT("crystal_gold"),TEXT("orb_gold")}},
            {TEXT("妖兽林"),TEXT("古修育婴府"),21,3,3,4,true,{TEXT("化婴丹"),TEXT("镇魂香"),TEXT("千年魂髓"),TEXT("先天元胎")},{TEXT("lotus_orb"),TEXT("hourglass"),TEXT("cloud_orb"),TEXT("lotus_orb")}},
            {TEXT("星陨海"),TEXT("观神古台"),22,4,1,5,true,{TEXT("化神灵引"),TEXT("定魂珠"),TEXT("神识结晶"),TEXT("天魂玉")},{TEXT("cloud_orb"),TEXT("jade_orb"),TEXT("crystal"),TEXT("jade_orb")}},
            {TEXT("星陨海"),TEXT("沉魂古战场"),23,5,2,6,true,{TEXT("元神灵髓"),TEXT("固神丹"),TEXT("神魂精华"),TEXT("太初魂晶")},{TEXT("jade_orb"),TEXT("shield"),TEXT("lotus_orb"),TEXT("crystal_gold")}},
            {TEXT("太虚境"),TEXT("定界裂隙"),25,6,2,5,true,{TEXT("虚空晶核"),TEXT("定界石"),TEXT("空间灵髓"),TEXT("虚空本源")},{TEXT("vortex"),TEXT("crystal"),TEXT("vortex"),TEXT("star_compass")}},
            {TEXT("太虚境"),TEXT("问道天阙"),30,7,3,8,true,{TEXT("大乘道果"),TEXT("护道丹"),TEXT("天地精粹"),TEXT("道源之心")},{TEXT("orb_gold"),TEXT("ring_jade"),TEXT("gold_cloud"),TEXT("heart")}},
            {TEXT("太虚境"),TEXT("九天劫台"),31,8,3,10,true,{TEXT("飞升道印"),TEXT("渡劫丹"),TEXT("九天仙髓"),TEXT("天劫本源")},{TEXT("seal_scroll"),TEXT("heart"),TEXT("water_orb"),TEXT("star_compass")}},
            {TEXT("太虚境"),TEXT("混沌归一坛"),32,9,3,10,true,{TEXT("混元道种"),TEXT("无极灵液"),TEXT("混沌精粹"),TEXT("鸿蒙本源")},{TEXT("lotus"),TEXT("water_orb"),TEXT("vortex"),TEXT("treasure")}}};
        static const TCHAR* Themes[]={TEXT("资格"),TEXT("经脉"),TEXT("凝练"),TEXT("天地共鸣")};
        static const TArray<FStage> List=[]
        {
            TArray<FStage> Out;
            for(int32 I=0;I<UE_ARRAY_COUNT(Rows);++I)
            {
                const FRow& R=Rows[I];FStage S;
                S.Id=*FString::Printf(TEXT("BR%02d"),I+1);S.Index=I;S.From=R.From;S.To=R.From+1;S.MinMajor=R.MinMajor;
                S.Region=T(R.Region);S.Subarea=T(R.Sub);S.bTribulation=R.bTrib;
                S.Caps[0]=1;S.Caps[1]=R.Guard;S.Caps[2]=R.Essence;S.Caps[3]=1;
                for(int32 C=0;C<4;++C)
                {
                    FSpecies& Sp=S.Species[C];
                    Sp.Id=*FString::Printf(TEXT("MAT_BR%02d_%s01"),I+1,CategoryLetter[C]);Sp.Name=T(R.Names[C]);Sp.Icon=R.Icons[C];Sp.Theme=T(Themes[C]);
                }
                Out.Add(S);
            }
            return Out;
        }();
        return List;
    }

    const FStage* StageFrom(int32 FromRealm){return Stages().FindByPredicate([&](const FStage& S){return S.From==FromRealm;});}
    const FStage* Stage(FName Id){return Stages().FindByPredicate([&](const FStage& S){return S.Id==Id;});}

    FName MaterialKey(const FStage& Stage,ECategory Category,int32 Grade)
    {
        return *FString::Printf(TEXT("%s_G%d"),*Stage.Species[FMath::Clamp(int32(Category),0,3)].Id.ToString(),FMath::Clamp(Grade,1,4));
    }

    bool ParseMaterial(FName Key,const FStage*& OutStage,ECategory& OutCategory,int32& OutGrade)
    {
        // MAT_BR01_C01_G2
        const FString S=Key.ToString();
        if(S.Len()!=15||!S.StartsWith(TEXT("MAT_BR"))||S[12]!=TEXT('G'))return false;
        const int32 StageNo=FCString::Atoi(*S.Mid(6,2)),Grade=FCString::Atoi(*S.Mid(13,1));
        if(StageNo<1||StageNo>Stages().Num()||Grade<1||Grade>4)return false;
        for(int32 C=0;C<4;++C)if(S[9]==CategoryLetter[C][0])
        {
            OutStage=&Stages()[StageNo-1];OutCategory=ECategory(C);OutGrade=Grade;
            return OutStage->Species[C].Id==FName(*S.Left(12));
        }
        return false;
    }

    int32 RefineCost(const FStage& Stage,int32 Grade){return 10*(Stage.Index+1)*FMath::Clamp(Grade,2,4);}

    // ------------------------------------------------------------------ quality

    const TArray<FBand>& Bands()
    {
        auto B=[](const TCHAR* Name,int32 Min,int32 Max,double Factor,const TCHAR* Color){FBand X;X.Name=T(Name);X.Min=Min;X.Max=Max;X.Factor=Factor;X.Color=Hex(Color);return X;};
        static const TArray<FBand> List={
            B(TEXT("凡品"),0,39,1.0,TEXT("9A9184")),B(TEXT("良品"),40,59,1.02,TEXT("6FBF73")),B(TEXT("上品"),60,74,1.04,TEXT("4FA3E0")),
            B(TEXT("极品"),75,89,1.06,TEXT("B07CF2")),B(TEXT("天品"),90,97,1.08,TEXT("F2B84B")),B(TEXT("完美"),98,100,1.10,TEXT("FF6A3D"))};
        return List;
    }

    int32 BandIndex(int32 Quality)
    {
        const TArray<FBand>& List=Bands();
        for(int32 I=List.Num()-1;I>0;--I)if(Quality>=List[I].Min)return I;
        return 0;
    }

    int32 QualityScore(const FStage& Stage,int32 CoreGrade,const TArray<int32>& Guard,const TArray<int32>& Essence,const TArray<int32>& Treasure)
    {
        auto Sum=[](ECategory C,const TArray<int32>& Grades,int32 Cap){int32 W=0,N=0;for(int32 G:Grades)if(G>0&&N<Cap){W+=GradeScore(C,G);++N;}return W;};
        const int32 Core=FMath::Min(GradeScore(ECategory::Core,CoreGrade),CategoryCap(ECategory::Core));
        const int32 GCap=FMath::Max(1,Stage.Caps[1]),ECap=FMath::Max(1,Stage.Caps[2]);
        const int32 G=FMath::Min(Sum(ECategory::Guard,Guard,Stage.Caps[1])/GCap,CategoryCap(ECategory::Guard));
        const int32 E=FMath::Min(Sum(ECategory::Essence,Essence,Stage.Caps[2])/ECap,CategoryCap(ECategory::Essence));
        const int32 Tr=FMath::Min(Sum(ECategory::Treasure,Treasure,Stage.Caps[3]),CategoryCap(ECategory::Treasure));
        return FMath::Clamp(Core+G+E+Tr,0,100);
    }

    double QualityFactor(const TArray<int32>& BestBandPerStage)
    {
        double Budget=0;
        for(int32 Band:BestBandPerStage)if(Bands().IsValidIndex(Band))Budget+=(Bands()[Band].Factor-1.0)/10.0;
        return 1.0+FMath::Min(.10,Budget);
    }

    // ------------------------------------------------------------------ props, guild

    const TArray<FProp>& Props()
    {
        auto P=[](const TCHAR* Id,const TCHAR* Name,int32 MinMajor,const TCHAR* Icon,int32 Quality,const TCHAR* Effect,const TCHAR* Key,float Mag,float Dur,float Cd)
        {FProp X;X.Id=Id;X.Name=T(Name);X.MinMajor=MinMajor;X.Icon=Icon;X.Quality=Quality;X.Effect=T(Effect);X.EffectKey=Key;X.Magnitude=Mag;X.Duration=Dur;X.Cooldown=Cd;return X;};
        static const TArray<FProp> List={
            P(TEXT("PROP_01"),TEXT("清心散"),0,TEXT("pill"),1,TEXT("清除1层可驱散的负面状态；身上无可驱散状态时不消耗。"),TEXT("cleanse"),1,0,60),
            P(TEXT("PROP_03"),TEXT("凝神香"),1,TEXT("hourglass"),2,TEXT("静修香雾300秒，只有演出，无修为或属性加成。"),TEXT("meditation_visual"),1,300,300),
            P(TEXT("PROP_04"),TEXT("护心符"),1,TEXT("shield"),2,TEXT("获得最大生命5%的屏障，持续10秒；与护阵类屏障同类取最高。"),TEXT("shield"),.05f,10,90),
            P(TEXT("PROP_06"),TEXT("清瘴丸"),2,TEXT("pill"),1,TEXT("移除1层瘴毒（中毒），不免疫后续伤害；身上无毒时不消耗。"),TEXT("cleanse_poison"),1,0,60),
            P(TEXT("PROP_14"),TEXT("涤魂茶"),6,TEXT("water_orb"),3,TEXT("清除最多2层可驱散的负面状态。"),TEXT("cleanse"),2,0,60),
            P(TEXT("PROP_15"),TEXT("归真露"),7,TEXT("heart"),3,TEXT("恢复主播最大生命15%，与治疗道具共享90秒冷却。"),TEXT("heal"),.15f,0,90),
            P(TEXT("PROP_16"),TEXT("太清符"),7,TEXT("seal_scroll"),3,TEXT("获得最大生命8%的屏障，持续10秒；同类取最高。"),TEXT("shield"),.08f,10,90),
            P(TEXT("PROP_17"),TEXT("问道灯"),8,TEXT("gold_cloud"),4,TEXT("突破与渡劫演出添一层灯影，持续120秒；无成功率加成。"),TEXT("ceremony"),1,120,120),
            P(TEXT("PROP_19"),TEXT("无极香"),9,TEXT("cloud_orb"),4,TEXT("混元突破香雾，演出持续180秒；无成功率加成。"),TEXT("ceremony"),1,180,180),
            P(TEXT("PROP_20"),TEXT("回影玉简"),0,TEXT("golden_scroll"),2,TEXT("在本机突破记录中标记一条已发生的突破演出；不存他人平台身份。"),TEXT("replay_marker"),1,0,30)};
        return List;
    }

    const FProp* Prop(FName Id){return Props().FindByPredicate([&](const FProp& P){return P.Id==Id;});}

    const TArray<FGuildMaterial>& GuildMaterials()
    {
        auto G=[](const TCHAR* Id,const TCHAR* Name,const TCHAR* Icon){FGuildMaterial X;X.Id=Id;X.Name=T(Name);X.Icon=Icon;return X;};
        static const TArray<FGuildMaterial> List={G(TEXT("GUILD_MAT_01"),TEXT("灵纹木"),TEXT("chest_bronze")),G(TEXT("GUILD_MAT_02"),TEXT("青罡砖"),TEXT("shield")),
            G(TEXT("GUILD_MAT_03"),TEXT("云纹绢"),TEXT("seal_scroll")),G(TEXT("GUILD_MAT_04"),TEXT("阵纹砂"),TEXT("crystal_gold"))};
        return List;
    }

    const TArray<FGuildBuilding>& GuildBuildings()
    {
        struct FRow{const TCHAR* Id;const TCHAR* Name;const TCHAR* Theme;int32 Scene;int32 A,B;};
        static const FRow Rows[]={
            {TEXT("BUILD_01"),TEXT("武灵大殿"),TEXT("名称旗帜与组织总览"),2,1,2},{TEXT("BUILD_02"),TEXT("炼丹阁"),TEXT("打开已有炼丹配方入口，安全区同样可用"),6,2,3},
            {TEXT("BUILD_03"),TEXT("炼器坊"),TEXT("打开已有炼器入口，无制作速度和品质加成"),5,3,4},{TEXT("BUILD_04"),TEXT("藏经阁"),TEXT("展示功法收藏与公开说明，不给额外研习进度"),0,4,1},
            {TEXT("BUILD_05"),TEXT("演武场"),TEXT("统一属性演示构筑，不给训练经验"),1,1,2},{TEXT("BUILD_06"),TEXT("护山阵"),TEXT("公共阵法外显，不产生战斗护盾"),4,2,3},
            {TEXT("BUILD_07"),TEXT("灵圃"),TEXT("静态药圃外观，不离线生产材料"),3,3,4},{TEXT("BUILD_08"),TEXT("万灵碑"),TEXT("展示允许的成员荣誉，不兑换属性"),7,4,1}};
        static const int32 Supply[]={20,60,120},Mats[]={2,6,12};
        static const TCHAR* Visual[]={TEXT("第1档外观"),TEXT("第2档外观"),TEXT("第3档外观")};
        static const TArray<FGuildBuilding> List=[]
        {
            TArray<FGuildBuilding> Out;
            for(const FRow& R:Rows)
            {
                FGuildBuilding B;B.Id=R.Id;B.Name=T(R.Name);B.Theme=T(R.Theme);B.Scene=R.Scene;
                for(int32 L=0;L<3;++L)
                {
                    FBuildLevel& Lv=B.Levels[L];Lv.Supply=Supply[L];Lv.MatCount=Mats[L];Lv.Visual=T(Visual[L]);
                    Lv.MatA=*FString::Printf(TEXT("GUILD_MAT_%02d"),R.A);Lv.MatB=*FString::Printf(TEXT("GUILD_MAT_%02d"),R.B);
                }
                Out.Add(B);
            }
            return Out;
        }();
        return List;
    }

    // ------------------------------------------------------------------ equipment and 法宝

    const TArray<FGear>& Gear()
    {
        static const TArray<FGear> List=[]
        {
            TArray<FGear> Out;
            static const TCHAR* Series[]={TEXT("青云"),TEXT("玄罡"),TEXT("太清"),TEXT("无极")};
            static const TCHAR* Pieces[]={TEXT("法衣"),TEXT("道冠"),TEXT("云履"),TEXT("灵佩")};
            static const TCHAR* Keys[]={TEXT("ROBE"),TEXT("CROWN"),TEXT("BOOTS"),TEXT("PENDANT")};
            static const TCHAR* Stats[]={TEXT("生命"),TEXT("防御"),TEXT("移速"),TEXT("攻击")};
            static const TCHAR* Icons[]={TEXT("armor"),TEXT("ring_gold"),TEXT("gold_cloud"),TEXT("ring_jade")};
            static const float Values[4][4]={{.03f,.03f,.02f,.02f},{.05f,.04f,.03f,.03f},{.08f,.06f,.04f,.05f},{.12f,.09f,.05f,.08f}};
            for(int32 Tier=0;Tier<4;++Tier)for(int32 Slot=0;Slot<4;++Slot)
            {
                FGear G;G.Id=*FString::Printf(TEXT("EQ_%s_T%d"),Keys[Slot],Tier+1);G.Name=T(*(FString(Series[Tier])+Pieces[Slot]));G.Slot=Slot;G.Tier=Tier+1;G.MinMajor=Tier*3;
                G.Value=Values[Tier][Slot];G.Stat=T(Stats[Slot]);G.Icon=Icons[Slot];
                G.Effect=FText::Format(LOCTEXT("GearEffect","{0} +{1}%（加法，不随机词条、不强化）"),G.Stat,FText::AsNumber(FMath::RoundToInt(G.Value*100)));
                Out.Add(G);
            }
            struct FRow{const TCHAR* Name;int32 MinMajor;float Cd;const TCHAR* Effect;const TCHAR* Icon;};
            static const FRow Fabao[]={
                {TEXT("引灵葫芦"),0,90,TEXT("恢复主播最大生命10%；满生命不消耗冷却"),TEXT("water_orb")},{TEXT("镇山印"),1,60,TEXT("8秒内下一次本命剑命中增伤30%，最多一次"),TEXT("seal_scroll")},
                {TEXT("玄罡镜"),1,90,TEXT("10%最大生命护盾12秒；同类屏障取最高"),TEXT("shield")},{TEXT("听风铃"),2,60,TEXT("后续10秒机制提示提前1秒，不自动答题"),TEXT("hourglass")},
                {TEXT("锁云绫"),3,60,TEXT("普通世界目标减速20%持续6秒，霸主免控时明确无效"),TEXT("gold_cloud")},{TEXT("归元鼎"),4,90,TEXT("恢复主播最大生命12%，不复活"),TEXT("treasure")},
                {TEXT("星河盘"),4,60,TEXT("后续3次本命剑命中增伤10%，12秒到期"),TEXT("star_compass")},{TEXT("离火幡"),5,60,TEXT("每秒0.1倍本命剑基础攻击，共5次"),TEXT("golden_scroll")},
                {TEXT("定界珠"),6,90,TEXT("世界环境伤害减免20%持续6秒，受总减伤60%上限"),TEXT("jade_orb")},{TEXT("镇魂钟"),7,90,TEXT("10秒内新施加的可清除负面时长缩短10%"),TEXT("orb_gold")},
                {TEXT("太虚尺"),8,60,TEXT("下一次本命剑攻击破甲10%，受破甲总上限60%"),TEXT("swords")},{TEXT("乾坤图"),9,60,TEXT("显示已公开的两路线机制预览15秒"),TEXT("scroll")}};
            for(int32 I=0;I<UE_ARRAY_COUNT(Fabao);++I)
            {
                FGear G;G.Id=*FString::Printf(TEXT("FABAO_%02d"),I+1);G.Name=T(Fabao[I].Name);G.Slot=4;G.Tier=FMath::Clamp(Fabao[I].MinMajor/3+1,1,4);G.MinMajor=Fabao[I].MinMajor;
                G.Cooldown=Fabao[I].Cd;G.Effect=T(Fabao[I].Effect);G.Stat=LOCTEXT("FabaoStat","主动");G.Icon=Fabao[I].Icon;Out.Add(G);
            }
            return Out;
        }();
        return List;
    }

    const FGear* GearItem(FName Id){return Gear().FindByPredicate([&](const FGear& G){return G.Id==Id;});}

    FText GearSlotName(int32 Slot)
    {
        static const TCHAR* Names[]={TEXT("法衣"),TEXT("道冠"),TEXT("云履"),TEXT("灵佩"),TEXT("法宝")};
        return T(Names[FMath::Clamp(Slot,0,4)]);
    }

    // ------------------------------------------------------------------ shops

    const TArray<FSku>& Skus()
    {
        static const TArray<FSku> List=[]
        {
            TArray<FSku> Out;
            auto Add=[&](const FString& Id,EShop Shop,int32 Tab,FName Item,int32 Count,ECurrency Cur,int32 Price,int32 Limit,int32 MinMajor,const TCHAR* Tag=TEXT(""))
            {FSku S;S.Id=*Id;S.Shop=Shop;S.Tab=Tab;S.Item=Item;S.Count=Count;S.Currency=Cur;S.Price=Price;S.DailyLimit=Limit;S.MinMajor=MinMajor;S.Tag=Tag;Out.Add(S);};
            // 灵玉商城: graded-one materials for every stage, gated by the stage's major realm; props; sundries.
            for(const FStage& S:Stages())
            {
                const int32 N=S.Index+1;
                Add(FString::Printf(TEXT("JADE_%s_C"),*S.Id.ToString()),EShop::Jade,0,MaterialKey(S,ECategory::Core,1),1,ECurrency::Jade,60*N,1,S.MinMajor,N==1?TEXT("hot"):TEXT(""));
                Add(FString::Printf(TEXT("JADE_%s_G"),*S.Id.ToString()),EShop::Jade,0,MaterialKey(S,ECategory::Guard,1),1,ECurrency::Jade,20*N,3,S.MinMajor);
                Add(FString::Printf(TEXT("JADE_%s_E"),*S.Id.ToString()),EShop::Jade,0,MaterialKey(S,ECategory::Essence,1),1,ECurrency::Jade,15*N,5,S.MinMajor);
                Add(FString::Printf(TEXT("JADE_%s_T"),*S.Id.ToString()),EShop::Jade,0,MaterialKey(S,ECategory::Treasure,1),1,ECurrency::Jade,50*N,1,S.MinMajor,TEXT("limited"));
            }
            static const int32 PropPrice[]={0,20,40,60,90};
            for(const FProp& P:Props())Add(TEXT("JADE_")+P.Id.ToString(),EShop::Jade,1,P.Id,1,ECurrency::Jade,PropPrice[FMath::Clamp(P.Quality,1,4)],2,P.MinMajor,P.Id==TEXT("PROP_15")?TEXT("new"):TEXT(""));
            Add(TEXT("JADE_BREAK"),EShop::Jade,2,TEXT("MAT_BREAK"),5,ECurrency::Jade,30,3,0,TEXT("hot"));
            Add(TEXT("JADE_STUDY"),EShop::Jade,2,TEXT("ITEM_STUDY"),1,ECurrency::Jade,80,1,0);
            Add(TEXT("JADE_PAGE"),EShop::Jade,2,TEXT("MAT_PAGE"),2,ECurrency::Jade,40,2,0);
            Add(TEXT("JADE_HERB"),EShop::Jade,2,TEXT("MAT_HERB_B"),3,ECurrency::Jade,20,3,0);
            Add(TEXT("JADE_IRON"),EShop::Jade,2,TEXT("MAT_IRON_B"),3,ECurrency::Jade,20,3,0);
            // 武灵商城: construction materials go to the vault; study and protection for the anchor.
            for(const FGuildMaterial& G:GuildMaterials())Add(TEXT("WULING_")+G.Id.ToString(),EShop::Wuling,0,G.Id,2,ECurrency::Wuling,12,6,0);
            Add(TEXT("WULING_PAGE"),EShop::Wuling,1,TEXT("MAT_PAGE"),1,ECurrency::Wuling,15,3,0,TEXT("hot"));
            Add(TEXT("WULING_STUDY"),EShop::Wuling,1,TEXT("ITEM_STUDY"),1,ECurrency::Wuling,45,1,0);
            Add(TEXT("WULING_BREAK"),EShop::Wuling,1,TEXT("MAT_BREAK"),3,ECurrency::Wuling,18,2,0);
            Add(TEXT("WULING_PROP_01"),EShop::Wuling,2,TEXT("PROP_01"),1,ECurrency::Wuling,10,2,0);
            Add(TEXT("WULING_PROP_04"),EShop::Wuling,2,TEXT("PROP_04"),1,ECurrency::Wuling,20,2,1);
            Add(TEXT("WULING_PROP_16"),EShop::Wuling,2,TEXT("PROP_16"),1,ECurrency::Wuling,35,2,7,TEXT("new"));
            Add(TEXT("WULING_PROP_15"),EShop::Wuling,2,TEXT("PROP_15"),1,ECurrency::Wuling,35,2,7);
            return Out;
        }();
        return List;
    }

    const FSku* Sku(FName Id){return Skus().FindByPredicate([&](const FSku& S){return S.Id==Id;});}

    // ------------------------------------------------------------------ 仙道殿

    const TArray<FCosmetic>& Cosmetics()
    {
        auto C=[](const TCHAR* Id,ECosmetic Kind,const TCHAR* Name,const TCHAR* Desc,int32 Price,int32 Quality,const TCHAR* A,const TCHAR* B,bool bLimited)
        {FCosmetic X;X.Id=Id;X.Kind=Kind;X.Name=T(Name);X.Description=T(Desc);X.Price=Price;X.Quality=Quality;X.Primary=Hex(A);X.Secondary=Hex(B);X.bLimited=bLimited;return X;};
        static const TArray<FCosmetic> List={
            C(TEXT("COS_SWORD_01"),ECosmetic::Sword,TEXT("青霄流光"),TEXT("本命剑化作一道青霄流光，剑身带碧色云纹。"),680,3,TEXT("6FE3C8"),TEXT("1E6B5C"),false),
            C(TEXT("COS_SWORD_02"),ECosmetic::Sword,TEXT("赤霞焚天"),TEXT("赤霞缠绕剑脊，挥剑时拖曳一线霞光。"),880,4,TEXT("FF7A45"),TEXT("8C1D18"),false),
            C(TEXT("COS_SWORD_03"),ECosmetic::Sword,TEXT("紫电青霜"),TEXT("紫电在霜刃上游走，剑鸣如雷。限定外观。"),1080,4,TEXT("B58CFF"),TEXT("5AD1FF"),true),
            C(TEXT("COS_SWORD_04"),ECosmetic::Sword,TEXT("太虚星河"),TEXT("剑身映出一条流动星河，星屑随剑而落。限定外观。"),1280,5,TEXT("8FB6FF"),TEXT("F7F2D8"),true),
            C(TEXT("COS_FRAME_01"),ECosmetic::Frame,TEXT("云纹金框"),TEXT("鎏金云纹头像框，四角祥云回旋。"),380,2,TEXT("E8C46A"),TEXT("7A5A1E"),false),
            C(TEXT("COS_FRAME_02"),ECosmetic::Frame,TEXT("青莲玉框"),TEXT("青玉莲瓣层层绽开的头像框。"),480,3,TEXT("7FD6B8"),TEXT("2C6E5B"),false),
            C(TEXT("COS_FRAME_03"),ECosmetic::Frame,TEXT("九霄龙纹框"),TEXT("金龙盘绕九霄的头像框。限定外观。"),880,5,TEXT("FFD15C"),TEXT("C2361F"),true),
            C(TEXT("COS_TITLE_01"),ECosmetic::Title,TEXT("剑啸九州"),TEXT("名牌上方悬一行剑气称号。"),520,3,TEXT("9AD7FF"),TEXT("1F3E66"),false),
            C(TEXT("COS_TITLE_02"),ECosmetic::Title,TEXT("一念逍遥"),TEXT("墨色行书称号，随风轻摆。"),520,3,TEXT("F2E6C9"),TEXT("3B3127"),false),
            C(TEXT("COS_TITLE_03"),ECosmetic::Title,TEXT("万剑归宗"),TEXT("万剑环绕的金色称号。限定外观。"),980,5,TEXT("FFD66B"),TEXT("8A4B12"),true),
            C(TEXT("COS_AURA_01"),ECosmetic::Aura,TEXT("祥云踏月"),TEXT("足下生出祥云，夜色里托着一弯新月。"),760,4,TEXT("D9E8FF"),TEXT("5865A8"),false),
            C(TEXT("COS_AURA_02"),ECosmetic::Aura,TEXT("金莲步生"),TEXT("步步生金莲，入定时莲台缓缓旋转。"),980,5,TEXT("FFCF6A"),TEXT("B4532A"),false)};
        return List;
    }

    const FCosmetic* Cosmetic(FName Id){return Cosmetics().FindByPredicate([&](const FCosmetic& C){return C.Id==Id;});}

    FText CosmeticKindName(ECosmetic Kind)
    {
        switch(Kind)
        {
        case ECosmetic::Sword:return LOCTEXT("KSword","剑灵外观");
        case ECosmetic::Frame:return LOCTEXT("KFrame","头像框");
        case ECosmetic::Title:return LOCTEXT("KTitle","称号");
        default:return LOCTEXT("KAura","足下灵光");
        }
    }

    // ------------------------------------------------------------------ 每日一签

    const TArray<FFortune>& Fortunes()
    {
        auto F=[](EFortune Id,const TCHAR* Name,const TCHAR* Verse,const TCHAR* Poem,const TCHAR* Effect,int32 Weight,float Luck,int32 Trib,float Damage,const TCHAR* Color,bool bOmen)
        {FFortune X;X.Id=Id;X.Name=T(Name);X.Verse=T(Verse);X.Poem=T(Poem);X.Effect=T(Effect);X.Weight=Weight;X.Luck=Luck;X.TribulationPenalty=Trib;X.DamageTaken=Damage;X.Color=Hex(Color);X.bOmen=bOmen;return X;};
        static const TArray<FFortune> List={
            F(EFortune::None,TEXT("未求签"),TEXT(""),TEXT(""),TEXT("今日尚未求签"),0,0,0,0,TEXT("9A9184"),false),
            F(EFortune::Great,TEXT("上上签"),TEXT("紫气东来"),TEXT("紫气东来满庭芳，青云直上步天梯"),TEXT("今日幸运 +50%"),10,.5f,0,0,TEXT("F2B84B"),false),
            F(EFortune::Good,TEXT("上签"),TEXT("云开月明"),TEXT("云开月明照前程，清风徐来送好音"),TEXT("今日幸运 +10%"),35,.1f,0,0,TEXT("6FBF73"),false),
            F(EFortune::Poor,TEXT("下下签"),TEXT("守拙待时"),TEXT("潜龙勿用且藏锋，静待春雷第一声"),TEXT("今日不加幸运"),35,0,0,0,TEXT("8C8378"),false),
            F(EFortune::Calamity,TEXT("灾"),TEXT("天雷暗伏"),TEXT("乌云压顶雷将至，渡劫之事且缓行"),TEXT("今日渡劫成功率 −10%"),10,0,1000,0,TEXT("7A2E8C"),true),
            F(EFortune::Ominous,TEXT("凶"),TEXT("血光隐现"),TEXT("血光隐现刀兵见，今日出行宜避锋"),TEXT("今日受到伤害 +50%"),10,0,0,.5f,TEXT("C2361F"),true)};
        return List;
    }

    const FFortune& Fortune(EFortune Id){return Fortunes()[FMath::Clamp(int32(Id),0,int32(EFortune::Count)-1)];}

    EFortune FortuneForRoll(int32 Roll)
    {
        int32 Edge=0;
        for(const FFortune& F:Fortunes()){Edge+=F.Weight;if(F.Weight>0&&Roll<Edge)return F.Id;}
        return EFortune::Poor;
    }
}

#undef LOCTEXT_NAMESPACE

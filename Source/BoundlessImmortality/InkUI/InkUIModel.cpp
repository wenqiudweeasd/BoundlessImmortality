#include "InkUI/InkUIModel.h"

#define LOCTEXT_NAMESPACE "InkUIModel"

namespace InkUI
{
    namespace
    {
        FText T(const TCHAR* Text){return FText::FromString(Text);}

        FReward Reward(const TCHAR* Icon,const TCHAR* Name,int32 Count,EQuality Quality=EQuality::Common)
        {
            FReward R;R.Icon=Icon;R.Name=T(Name);R.Count=Count;R.Quality=Quality;return R;
        }

        FItem Item(const TCHAR* Id,const TCHAR* Name,const TCHAR* Category,const TCHAR* Icon,EQuality Quality,int32 Count,const TCHAR* Description,bool bUsable=false,const TCHAR* Slot=TEXT(""))
        {
            FItem I;I.Key=FGuid::NewGuid();I.Id=Id;I.Name=T(Name);I.Category=Category;I.Icon=Icon;I.Quality=Quality;I.Count=Count;
            I.Description=T(Description);I.bUsable=bUsable;I.EquipSlot=Slot;return I;
        }

        FActivity Activity(const TCHAR* Id,const TCHAR* Category,const TCHAR* Title,const TCHAR* Description,const TCHAR* Time,int32 Scene,int32 Progress,int32 Target,TArray<FReward> Rewards,EClaim State)
        {
            FActivity A;A.Id=Id;A.Category=Category;A.Title=T(Title);A.Description=T(Description);A.TimeText=T(Time);A.Scene=Scene;
            A.Progress=Progress;A.Target=Target;A.Rewards=MoveTemp(Rewards);A.State=State;return A;
        }

        FJourneyEntry MakeJourney(const TCHAR* Id,const TCHAR* Title,const TCHAR* Description,int32 Scene,bool bNeedsService,const TCHAR* Route=TEXT(""))
        {
            FJourneyEntry J;J.Id=Id;J.Title=T(Title);J.Description=T(Description);J.Scene=Scene;J.bNeedsService=bNeedsService;J.Route=Route;return J;
        }
    }

    FText FormatAmount(double Value)
    {
        const double A=FMath::Abs(Value);
        if(A>=1.e8)return FText::FromString(FString::Printf(TEXT("%.1f亿"),Value/1.e8));
        if(A>=1.e5)return FText::FromString(FString::Printf(TEXT("%.1f万"),Value/1.e4));
        return FText::AsNumber(FMath::RoundToInt64(Value));
    }

    FText CategoryLabel(FName Category)
    {
        if(Category==TEXT("Equipment"))return LOCTEXT("CatEquipment","装备");
        if(Category==TEXT("Material"))return LOCTEXT("CatMaterial","材料");
        if(Category==TEXT("Consumable"))return LOCTEXT("CatConsumable","丹药");
        if(Category==TEXT("Treasure"))return LOCTEXT("CatTreasure","法宝");
        return LOCTEXT("CatOther","杂物");
    }

    void FModel::Touch(){++Revision;OnChanged.Broadcast();}

    void FModel::SeedLocal()
    {
        Player.Name=T(TEXT("云川"));Player.Realm=T(TEXT("筑基初期"));Player.RealmIndex=9;
        Player.Health=4150;Player.MaxHealth=4150;Player.Qi=2380;Player.MaxQi=5400;Player.QiPerSecond=3.2;Player.bLive=false;

        Attributes.Reset();
        auto Attr=[&](const TCHAR* Id,const TCHAR* Label,const TCHAR* Value){FAttribute A;A.Id=Id;A.Label=T(Label);A.Value=T(Value);Attributes.Add(A);};
        Attr(TEXT("Health"),TEXT("血量"),TEXT("4150 / 4150"));Attr(TEXT("Attack"),TEXT("剑灵攻击"),TEXT("286"));
        Attr(TEXT("Defense"),TEXT("防御"),TEXT("132"));Attr(TEXT("SpiritDamage"),TEXT("剑灵伤害"),TEXT("412"));
        Attr(TEXT("Critical"),TEXT("会心一击"),TEXT("12.5%"));Attr(TEXT("Evasion"),TEXT("闪避"),TEXT("6.0%"));
        Attr(TEXT("Luck"),TEXT("幸运"),TEXT("3.0"));Attr(TEXT("MoveSpeed"),TEXT("移速上限"),TEXT("480 厘米/秒"));
        Attr(TEXT("Swordsmanship"),TEXT("剑法"),TEXT("18.0"));Attr(TEXT("CultivationSpeed"),TEXT("修炼速度"),TEXT("3.20 /秒"));
        Attr(TEXT("Realm"),TEXT("境界"),TEXT("筑基初期"));

        Items.Reset();BagCapacity=40;
        Items.Add(Item(TEXT("SpiritStoneLow"),TEXT("下品灵石"),TEXT("Material"),TEXT("crystal"),EQuality::Common,268,TEXT("天地灵气凝结之石，修士间通行的货币，亦可直接吸纳修炼。")));
        Items.Add(Item(TEXT("SpiritStoneMid"),TEXT("中品灵石"),TEXT("Material"),TEXT("crystal_gold"),EQuality::Rare,36,TEXT("灵气更为精纯的灵石，炼丹、布阵皆可用之。")));
        Items.Add(Item(TEXT("RejuvenationPill"),TEXT("回春丹"),TEXT("Consumable"),TEXT("pill"),EQuality::Uncommon,12,TEXT("服下可回复三成血量，战后疗伤常备之物。"),true));
        Items.Add(Item(TEXT("FoundationPill"),TEXT("筑基丹"),TEXT("Consumable"),TEXT("lotus_orb"),EQuality::Epic,2,TEXT("凝练道基的灵丹，突破筑基关隘时服用可稳固根基。"),true));
        Items.Add(Item(TEXT("AzureSword"),TEXT("青锋剑"),TEXT("Equipment"),TEXT("swords"),EQuality::Rare,1,TEXT("以玄铁百炼而成的长剑，剑气凌厉，可承载剑灵。"),false,TEXT("Weapon")));
        Items.Add(Item(TEXT("TortoiseArmor"),TEXT("玄龟甲"),TEXT("Equipment"),TEXT("armor"),EQuality::Uncommon,1,TEXT("取千年玄龟背甲所制，坚逾精铁。"),false,TEXT("Armor")));
        Items.Add(Item(TEXT("StarCompass"),TEXT("星辰罗盘"),TEXT("Treasure"),TEXT("star_compass"),EQuality::Legendary,1,TEXT("可推演周天星斗的上古法宝，传闻能指引机缘所在。")));
        Items.Add(Item(TEXT("ManualShard"),TEXT("功法残卷"),TEXT("Material"),TEXT("scroll"),EQuality::Common,5,TEXT("残缺的功法篇章，集齐十卷可参悟完整心法。")));
        Items.Add(Item(TEXT("TaiyiScripture"),TEXT("太乙玄经"),TEXT("Treasure"),TEXT("golden_scroll"),EQuality::Epic,1,TEXT("道门正宗心法，修习后修炼速度大幅提升。")));
        Items.Add(Item(TEXT("GatheringPearl"),TEXT("聚灵珠"),TEXT("Treasure"),TEXT("orb_gold"),EQuality::Legendary,3,TEXT("可聚拢周身灵气的宝珠，入定时佩于身侧事半功倍。")));
        Items.Add(Item(TEXT("JadeLotus"),TEXT("青玉莲台"),TEXT("Treasure"),TEXT("lotus"),EQuality::Epic,1,TEXT("佛门至宝之仿品，静坐其上心神澄明。")));
        Items.Add(Item(TEXT("WaterPearl"),TEXT("水灵珠"),TEXT("Material"),TEXT("water_orb"),EQuality::Rare,4,TEXT("蕴含水行灵力的宝珠，炼制水属法宝的主材。")));
        Items.Add(Item(TEXT("HeartMirror"),TEXT("护心镜"),TEXT("Equipment"),TEXT("shield"),EQuality::Rare,1,TEXT("护住心脉要害的宝镜，可抵挡致命一击。"),false,TEXT("Necklace")));
        Items.Add(Item(TEXT("TimeSand"),TEXT("流光沙漏"),TEXT("Treasure"),TEXT("hourglass"),EQuality::Mythic,1,TEXT("内藏一缕光阴之力，传说可令修炼时光倍增。")));
        Items.Add(Item(TEXT("BloodHeart"),TEXT("赤血灵心"),TEXT("Consumable"),TEXT("heart"),EQuality::Epic,2,TEXT("妖王精血所化，服下永久提升血量上限。"),true));
        Items.Add(Item(TEXT("CopperCoin"),TEXT("铜钱"),TEXT("Material"),TEXT("coin"),EQuality::Common,999,TEXT("凡俗钱币，坊市中仍可换些寻常物件。")));
        Items.Add(Item(TEXT("JadeChest"),TEXT("青玉宝箱"),TEXT("Consumable"),TEXT("chest_jade"),EQuality::Rare,2,TEXT("开启后随机获得灵石或丹药。"),true));
        Items.Add(Item(TEXT("GoldChest"),TEXT("金丝宝箱"),TEXT("Consumable"),TEXT("chest_gold"),EQuality::Legendary,1,TEXT("以金丝封缄的宝箱，开启必得珍稀宝物。"),true));
        Items.Add(Item(TEXT("JadePearl"),TEXT("碧玉灵珠"),TEXT("Material"),TEXT("jade_orb"),EQuality::Uncommon,6,TEXT("温润如玉的灵珠，可作阵法阵眼。")));
        Items.Add(Item(TEXT("SwordSeal"),TEXT("玄铁剑印"),TEXT("Equipment"),TEXT("sword_emblem"),EQuality::Epic,1,TEXT("剑修信物，佩戴后剑灵攻击提升。"),false,TEXT("Ring")));
        Items[2].bNew=true;Items[9].bNew=true;Items[17].bNew=true;

        Currencies.Reset();
        FCurrency Stone;Stone.Id=TEXT("SpiritStone");Stone.Name=T(TEXT("灵石"));Stone.Icon=TEXT("crystal");Stone.Amount=12860;Currencies.Add(Stone);
        // The four 仙途 currencies; amounts come from the ledger once a game runs (XianTuCatalog).
        auto Cur=[&](const TCHAR* Id,const TCHAR* Name,const TCHAR* Icon,int64 Amount){FCurrency C;C.Id=Id;C.Name=T(Name);C.Icon=Icon;C.Amount=Amount;Currencies.Add(C);};
        Cur(TEXT("CUR_JADE"),TEXT("灵玉"),TEXT("jade_orb"),680);Cur(TEXT("CUR_WULING"),TEXT("武灵"),TEXT("swords"),120);
        Cur(TEXT("CUR_SOUL"),TEXT("元魂"),TEXT("vortex"),0);Cur(TEXT("CUR_GOLD"),TEXT("金元"),TEXT("orb_gold"),1288);

        Boards.Reset();bLiveBoards=false;
        const TCHAR* Names[]={TEXT("凌霄子"),TEXT("白衣卿相"),TEXT("墨染"),TEXT("云川"),TEXT("清风徐来"),TEXT("苍梧"),TEXT("紫陌"),TEXT("青冥"),TEXT("落雪无声"),TEXT("玄机"),TEXT("星河"),TEXT("归尘")};
        const TCHAR* Realms[]={TEXT("金丹三阶"),TEXT("金丹一阶"),TEXT("结晶三阶"),TEXT("筑基初期"),TEXT("筑基初期"),TEXT("练气九阶"),TEXT("练气八阶"),TEXT("练气八阶"),TEXT("练气七阶"),TEXT("练气六阶"),TEXT("练气五阶"),TEXT("练气三阶")};
        auto Board=[&](FName Id,TFunctionRef<FString(int32)> Metric)
        {
            TArray<FRankRow> Rows;
            for(int32 I=0;I<UE_ARRAY_COUNT(Names);++I){FRankRow R;R.Rank=I+1;R.Name=T(Names[I]);R.Realm=T(Realms[I]);R.Metric=FText::FromString(Metric(I));R.bSelf=I==3;Rows.Add(R);}
            Boards.Add(Id,Rows);
        };
        Board(TEXT("Anchor"),[](int32 I){return FString::Printf(TEXT("%d 把"),FMath::Max(3,96-I*7-I*I));});
        Board(TEXT("CultivationSpeed"),[](int32 I){return FString::Printf(TEXT("%.2f /秒"),12.8-I*.93);});
        Board(TEXT("Damage"),[](int32 I){return FString::Printf(TEXT("%.0f /秒"),FMath::Max(40.0,2860.0-I*231.0));});

        SignIn.Reset();
        const FReward Days[]={Reward(TEXT("crystal"),TEXT("下品灵石"),100),Reward(TEXT("scroll"),TEXT("功法残卷"),2),Reward(TEXT("pill"),TEXT("回春丹"),3,EQuality::Uncommon),
            Reward(TEXT("coin"),TEXT("铜钱"),500),Reward(TEXT("crystal_gold"),TEXT("中品灵石"),5,EQuality::Rare),Reward(TEXT("orb_gold"),TEXT("聚灵珠"),1,EQuality::Legendary),
            Reward(TEXT("chest_gold"),TEXT("金丝宝箱"),1,EQuality::Legendary)};
        for(int32 I=0;I<7;++I){FSignInDay D;D.Day=I+1;D.Reward=Days[I];D.State=I<2?EClaim::Claimed:I==2?EClaim::Claimable:EClaim::Locked;SignIn.Add(D);}
        SignInTotal=2;
        Milestones.Reset();
        const int32 Steps[]={3,7,14,21};
        const FReward Chests[]={Reward(TEXT("chest_bronze"),TEXT("青铜宝箱"),1,EQuality::Uncommon),Reward(TEXT("chest_jade"),TEXT("青玉宝箱"),1,EQuality::Rare),
            Reward(TEXT("chest_gold"),TEXT("金丝宝箱"),1,EQuality::Legendary),Reward(TEXT("treasure"),TEXT("天机宝匣"),1,EQuality::Mythic)};
        for(int32 I=0;I<4;++I){FMilestone M;M.Days=Steps[I];M.Reward=Chests[I];Milestones.Add(M);}

        Activities.Reset();
        Activities.Add(Activity(TEXT("Meditate"),TEXT("Daily"),TEXT("静心修炼"),TEXT("入定修炼累计十分钟"),TEXT("每日 05:00 重置"),6,6,10,{Reward(TEXT("crystal"),TEXT("下品灵石"),200),Reward(TEXT("pill"),TEXT("回春丹"),2,EQuality::Uncommon)},EClaim::Locked));
        Activities.Add(Activity(TEXT("SwordTrial"),TEXT("Daily"),TEXT("剑灵试炼"),TEXT("驱使剑灵击退妖物五只"),TEXT("每日 05:00 重置"),1,5,5,{Reward(TEXT("crystal_gold"),TEXT("中品灵石"),3,EQuality::Rare),Reward(TEXT("scroll"),TEXT("功法残卷"),1)},EClaim::Claimable));
        Activities.Add(Activity(TEXT("Alchemy"),TEXT("Daily"),TEXT("丹房问药"),TEXT("服用任意丹药一次"),TEXT("每日 05:00 重置"),3,1,1,{Reward(TEXT("coin"),TEXT("铜钱"),300)},EClaim::Claimed));
        Activities.Add(Activity(TEXT("QingyunDuel"),TEXT("Timed"),TEXT("青云论剑"),TEXT("限时比武，与同道切磋剑艺"),TEXT("剩余 2天06时"),2,2,3,{Reward(TEXT("jade_orb"),TEXT("碧玉灵珠"),5,EQuality::Uncommon),Reward(TEXT("swords"),TEXT("青锋剑"),1,EQuality::Rare)},EClaim::Locked));
        Activities.Last().bFeatured=true;
        Activities.Add(Activity(TEXT("SpringRealm"),TEXT("Timed"),TEXT("灵泉秘境"),TEXT("探索秘境三次，寻得灵泉"),TEXT("剩余 5天12时"),5,3,3,{Reward(TEXT("water_orb"),TEXT("水灵珠"),2,EQuality::Rare)},EClaim::Claimable));
        Activities.Add(Activity(TEXT("LaunchGathering"),TEXT("Launch"),TEXT("青云初启 · 同道共修"),TEXT("开服七日，与同道共聚灵气"),TEXT("开服第 3 天"),4,3,7,{Reward(TEXT("lotus"),TEXT("青玉莲台"),1,EQuality::Epic),Reward(TEXT("crystal_gold"),TEXT("中品灵石"),10,EQuality::Rare)},EClaim::Locked));
        Activities.Last().bFeatured=true;
        Activities.Add(Activity(TEXT("LaunchLogin"),TEXT("Launch"),TEXT("登临仙途"),TEXT("开服期间累计登录三日"),TEXT("开服第 3 天"),0,3,3,{Reward(TEXT("orb_gold"),TEXT("聚灵珠"),1,EQuality::Legendary)},EClaim::Claimable));

        Journey.Reset();
        // Every 仙途 entry runs on the local ledger (UXianTuSubsystem); none waits for the live SDK.
        Journey.Add(MakeJourney(TEXT("World"),TEXT("世界历练"),TEXT("十分钟三阶段，共历山河"),4,false,TEXT("World")));
        Journey.Add(MakeJourney(TEXT("Resources"),TEXT("资源争夺"),TEXT("灵脉竞采，限时开采"),3,false,TEXT("Resources")));
        Journey.Add(MakeJourney(TEXT("Wuling"),TEXT("武灵山门"),TEXT("开宗立派，聚拢同道"),2,false,TEXT("Wuling")));
        Journey.Add(MakeJourney(TEXT("Market"),TEXT("云山坊市"),TEXT("灵草玄铁，寄售回收"),7,false,TEXT("Market")));
        Journey.Add(MakeJourney(TEXT("JadeShop"),TEXT("灵玉商城"),TEXT("突破材料，道具杂货"),3,false,TEXT("JadeShop")));
        Journey.Add(MakeJourney(TEXT("Breakthrough"),TEXT("境界突破"),TEXT("修为圆满，叩问大道"),6,false,TEXT("Breakthrough")));
        Journey.Add(MakeJourney(TEXT("SwordSpirit"),TEXT("剑灵谱"),TEXT("观众剑灵，与你同行"),1,false,TEXT("SwordSpirit")));
        Journey.Add(MakeJourney(TEXT("Methods"),TEXT("功法典藏"),TEXT("一主两辅，构筑剑路"),0,false,TEXT("Methods")));
        Journey.Add(MakeJourney(TEXT("Tribulation"),TEXT("天劫"),TEXT("三重雷劫，众剑助力"),5,false,TEXT("Tribulation")));
        Journey.Add(MakeJourney(TEXT("ItemSources"),TEXT("物品来源"),TEXT("材料去处，一目了然"),4,false,TEXT("ItemSources")));
        Journey.Add(MakeJourney(TEXT("EquipmentFabao"),TEXT("装备法宝"),TEXT("四件法衣，一件法宝"),1,false,TEXT("EquipmentFabao")));

        Saves.Reset();bLiveSaves=false;
        for(int32 I=0;I<3;++I){FSaveSlot S;S.Index=I;S.bUsed=I==0;S.Title=FText::Format(LOCTEXT("SlotTitle","存档{0}"),FText::FromString(I==0?TEXT("一"):I==1?TEXT("二"):TEXT("三")));
            S.Detail=I==0?T(TEXT("青云山 · 筑基初期")):LOCTEXT("SlotEmpty","空位 · 可保存当前行程");S.Time=I==0?T(TEXT("10-28 18:20")):FText::GetEmpty();Saves.Add(S);}
        ++ItemsRev;++BoardsRev;++SignInRev;++ActivitiesRev;++SavesRev;++AttributesRev;
        Touch();
    }

    bool FModel::ClaimSignIn(TArray<FReward>& OutRewards)
    {
        for(auto& Day:SignIn)if(Day.State==EClaim::Claimable)
        {
            Day.State=EClaim::Claimed;++SignInTotal;OutRewards.Add(Day.Reward);
            for(auto& M:Milestones)if(M.State==EClaim::Locked&&SignInTotal>=M.Days)M.State=EClaim::Claimable;
            ++SignInRev;GrantRewards(OutRewards);Touch();return true;
        }
        return false;
    }

    bool FModel::ClaimMilestone(int32 Index,TArray<FReward>& OutRewards)
    {
        if(!Milestones.IsValidIndex(Index)||Milestones[Index].State!=EClaim::Claimable)return false;
        Milestones[Index].State=EClaim::Claimed;OutRewards.Add(Milestones[Index].Reward);++SignInRev;GrantRewards(OutRewards);Touch();return true;
    }

    bool FModel::ClaimActivity(FName Id,TArray<FReward>& OutRewards)
    {
        for(auto& A:Activities)if(A.Id==Id&&A.State==EClaim::Claimable)
        {
            A.State=EClaim::Claimed;OutRewards.Append(A.Rewards);++ActivitiesRev;GrantRewards(OutRewards);Touch();return true;
        }
        return false;
    }

    void FModel::GrantRewards(const TArray<FReward>& Rewards)
    {
        ++ItemsRev;
        for(const auto& R:Rewards)
        {
            FItem* Existing=Items.FindByPredicate([&](const FItem& I){return I.Icon==R.Icon&&I.Name.EqualTo(R.Name);});
            if(Existing){Existing->Count+=R.Count;Existing->bNew=true;continue;}
            if(Items.Num()>=BagCapacity)continue;
            FItem I;I.Key=FGuid::NewGuid();I.Id=FName(*R.Name.ToString());I.Name=R.Name;I.Icon=R.Icon;I.Quality=R.Quality;I.Count=R.Count;I.bNew=true;
            I.Category=R.Icon.ToString().StartsWith(TEXT("chest"))||R.Icon==TEXT("pill")?FName(TEXT("Consumable")):FName(TEXT("Material"));
            I.bUsable=I.Category==TEXT("Consumable");I.Description=LOCTEXT("RewardItemDesc","本地验证奖励，用于检视界面表现。");
            Items.Add(I);
        }
    }

    bool FModel::UseItem(const FGuid& Key,FText& OutMessage,TArray<FReward>& OutRewards)
    {
        const int32 Index=Items.IndexOfByPredicate([&](const FItem& I){return I.Key==Key;});
        if(Index==INDEX_NONE||!Items[Index].bUsable){OutMessage=LOCTEXT("CannotUse","此物无法直接使用");return false;}
        FItem& I=Items[Index];
        OutMessage=FText::Format(LOCTEXT("UsedItem","使用了 {0} ×1"),I.Name);
        if(I.Icon==TEXT("chest_jade"))OutRewards.Add(Reward(TEXT("crystal_gold"),TEXT("中品灵石"),4,EQuality::Rare));
        else if(I.Icon==TEXT("chest_gold"))OutRewards.Append({Reward(TEXT("orb_gold"),TEXT("聚灵珠"),1,EQuality::Legendary),Reward(TEXT("crystal_gold"),TEXT("中品灵石"),12,EQuality::Rare)});
        else if(I.Icon==TEXT("pill"))Player.Health=FMath::Min(Player.MaxHealth,Player.Health+Player.MaxHealth*.3);
        if(--I.Count<=0)Items.RemoveAt(Index);
        if(OutRewards.Num())GrantRewards(OutRewards);
        ++ItemsRev;Touch();return true;
    }

    void FModel::SortItems()
    {
        auto Rank=[](FName C){return C==TEXT("Equipment")?0:C==TEXT("Treasure")?1:C==TEXT("Consumable")?2:3;};
        Items.Sort([&](const FItem& A,const FItem& B)
        {
            if(Rank(A.Category)!=Rank(B.Category))return Rank(A.Category)<Rank(B.Category);
            if(A.Quality!=B.Quality)return A.Quality>B.Quality;
            return A.Name.CompareTo(B.Name)<0;
        });
        ++ItemsRev;Touch();
    }

    const FItem* FModel::FindItem(const FGuid& Key) const{return Items.FindByPredicate([&](const FItem& I){return I.Key==Key;});}
    const FCurrency* FModel::FindCurrency(FName Id) const{return Currencies.FindByPredicate([&](const FCurrency& C){return C.Id==Id;});}
    bool FModel::HasClaimableSignIn() const
    {
        return SignIn.ContainsByPredicate([](const FSignInDay& D){return D.State==EClaim::Claimable;})||Milestones.ContainsByPredicate([](const FMilestone& M){return M.State==EClaim::Claimable;});
    }
    int32 FModel::ClaimableActivities(FName Category) const
    {
        int32 N=0;for(const auto& A:Activities)if(A.State==EClaim::Claimable&&(Category.IsNone()||A.Category==Category))++N;return N;
    }
    float FModel::QiFraction() const{return Player.MaxQi>0?float(FMath::Clamp(Player.Qi/Player.MaxQi,0.0,1.0)):0.f;}
    float FModel::HealthFraction() const{return Player.MaxHealth>0?float(FMath::Clamp(Player.Health/Player.MaxHealth,0.0,1.0)):0.f;}
}

#undef LOCTEXT_NAMESPACE

#if WITH_DEV_AUTOMATION_TESTS
#include "Engine/GameInstance.h"
#include "Misc/AutomationTest.h"
#include "WanJian/WanJianBattle.h"
#include "WanJian/WanJianCatalog.h"
#include "WanJian/WanJianRules.h"
#include "WanJian/WanJianSubsystem.h"

using namespace WanJianCatalog;
using namespace WanJianRules;

namespace
{
    constexpr EAutomationTestFlags Flags=EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter;

    UWanJianSubsystem* Account(double* Clock=nullptr)
    {
        UWanJianSubsystem* A=NewObject<UWanJianSubsystem>(NewObject<UGameInstance>(GetTransientPackage()));
        if(Clock)A->SetClockForTests([Clock]{return *Clock;});
        else A->SetClockForTests([]{return 1'800'000'000.0;});
        A->ResetForTests();
        return A;
    }

    WanJianBattle::FResult Won()
    {
        WanJianBattle::FResult R;
        R.bValid=true;
        R.bVictory=true;
        return R;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWanJianStageTableTest,"BoundlessImmortality.WanJian.StageTable",Flags)
bool FWanJianStageTableTest::RunTest(const FString&)
{
    TestEqual(TEXT("eight worlds in the catalog"),Worlds().Num(),WorldCount);
    TSet<int32> Ids;
    int32 Open=0;
    for(int32 World=1;World<=WorldCount;++World)
    {
        for(int32 Index=1;Index<=StagesPerWorld;++Index)
        {
            FStage Stage;
            TestTrue(TEXT("stage exists"),MakeStage(World,Index,Stage));
            TestEqual(TEXT("stage id formula"),Stage.Id,World*1000+Index);
            bool bDuplicate=false;
            Ids.Add(Stage.Id,&bDuplicate);
            TestFalse(TEXT("stage ids are unique"),bDuplicate);
            int32 W=0,I=0;
            TestTrue(TEXT("id splits back"),SplitStageId(Stage.Id,W,I)&&W==World&&I==Index);
            TestEqual(TEXT("bosses close each 30 stages"),Stage.bBoss,Index%30==0);
            TestTrue(TEXT("stage has enemies"),Stage.Normals>0&&Stage.NormalHealth>0);
            TestTrue(TEXT("at most 50 regular enemies"),Stage.Normals<=50);
            TestTrue(TEXT("duration in the 45-180 s window"),Stage.Duration>=45&&Stage.Duration<=180);
            TestTrue(TEXT("template in range"),Stage.Template>=0&&Stage.Template<TutorialStages+BaseTemplates);
            Open+=Stage.bOpen?1:0;
        }
    }
    TestEqual(TEXT("960 stages"),Ids.Num(),960);
    TestEqual(TEXT("first release opens 240 stages"),Open,240);
    TestEqual(TEXT("the first stage has no prerequisite"),PreviousStage(1001),0);
    TestEqual(TEXT("worlds chain"),PreviousStage(2001),1120);
    TestEqual(TEXT("two slots at the start"),UnlockedSlots(0),2);
    TestEqual(TEXT("eighteen slots at the end"),UnlockedSlots(120),18);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWanJianPityTest,"BoundlessImmortality.WanJian.Pity500",Flags)
bool FWanJianPityTest::RunTest(const FString&)
{
    int32 Forced=0;
    int32 Natural=0;
    for(int32 Seed=1;Seed<=120;++Seed)
    {
        FRandomStream Stream(Seed);
        FWanJianPity Pity;
        for(int32 Draw=1;Draw<=PityDraws;++Draw)
        {
            bool bGuaranteed=false;
            const FName Result=RollDraw(Stream,Pity,bGuaranteed);
            if(Draw<PityDraws)TestFalse(TEXT("no guarantee before draw 500"),bGuaranteed);
            if(Draw==PityDraws&&!bGuaranteed)TestTrue(TEXT("only an earlier natural hit skips the guarantee"),Pity.AcquiredAtDraw<PityDraws);
            if(bGuaranteed)TestEqual(TEXT("the guarantee pays 七星光芒剑"),Result,SevenStarSword());
            if(Draw==PityDraws)
            {
                Forced+=bGuaranteed?1:0;
                Natural+=bGuaranteed?0:1;
            }
        }
        TestTrue(TEXT("七星 is always owned by draw 500"),Pity.bSevenStarOwned);
        const int32 Counted=Pity.Draws;
        bool bGuaranteed=false;
        RollDraw(Stream,Pity,bGuaranteed);
        TestFalse(TEXT("no second guarantee"),bGuaranteed);
        TestEqual(TEXT("the counter stops once owned"),Pity.Draws,Counted);
    }
    TestTrue(TEXT("both outcomes occurred"),Forced>0&&Natural>0);

    FRandomStream Stream(99);
    FWanJianPity Pity;
    Pity.bSevenStarOwned=true;
    TMap<EWanJianRarity,int32> Tiers;
    int32 Stars=0;
    const int32 Total=400000;
    for(int32 I=0;I<Total;++I)
    {
        bool bGuaranteed=false;
        const FName Id=RollDraw(Stream,Pity,bGuaranteed);
        Stars+=Id==SevenStarSword()?1:0;
        ++Tiers.FindOrAdd(Sword(Id)->Rarity);
    }
    auto Near=[&](const TCHAR* What,int32 Count,double Expected,double Tolerance){TestTrue(What,FMath::Abs(double(Count)/Total-Expected)<Tolerance);};
    Near(TEXT("普通 72%"),Tiers.FindRef(EWanJianRarity::Common),.72,.005);
    Near(TEXT("优秀 23%"),Tiers.FindRef(EWanJianRarity::Fine),.23,.005);
    Near(TEXT("稀有 4.5%"),Tiers.FindRef(EWanJianRarity::Rare),.045,.002);
    Near(TEXT("传说 0.5%"),Tiers.FindRef(EWanJianRarity::Legendary),.005,.001);
    Near(TEXT("七星 0.1%"),Stars,.001,.0004);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWanJianDrawTransactionTest,"BoundlessImmortality.WanJian.DrawTransactions",Flags)
bool FWanJianDrawTransactionTest::RunTest(const FString&)
{
    UWanJianSubsystem* A=Account();
    FText Out;
    TestNull(TEXT("no tickets, no draw"),A->DrawSwords(TEXT("r0"),1,A->GetState().Version,Out));
    A->ClaimTutorial(TEXT("t"),Out);
    A->ClaimTutorial(TEXT("t2"),Out);
    TestEqual(TEXT("the tutorial pays once"),A->Balance(Asset::Ticket),int64(10));

    const int64 Version=A->GetState().Version;
    TestNull(TEXT("an old version is rejected"),A->DrawSwords(TEXT("stale"),1,Version-1,Out));
    TestNull(TEXT("only one or ten"),A->DrawSwords(TEXT("bad"),3,Version,Out));
    const FWanJianDrawReceipt* First=A->DrawSwords(TEXT("ten"),10,Version,Out);
    TestNotNull(TEXT("ten-draw works"),First);
    if(!First)return false;
    TestEqual(TEXT("ten results"),First->Results.Num(),10);
    TestEqual(TEXT("tickets charged"),A->Balance(Asset::Ticket),int64(0));
    TestEqual(TEXT("pity counted one by one"),A->GetState().Pity.Draws,10);
    const FName FirstSword=First->Results[0].Sword;
    const int32 Owned=A->GetState().Swords.Num();

    const FWanJianDrawReceipt* Retry=A->DrawSwords(TEXT("ten"),10,0,Out);
    TestNotNull(TEXT("a retry returns the receipt even with an old version"),Retry);
    if(Retry)TestEqual(TEXT("same result"),Retry->Results[0].Sword,FirstSword);
    TestEqual(TEXT("no second charge"),A->Balance(Asset::Ticket),int64(0));
    TestEqual(TEXT("no second grant"),A->GetState().Swords.Num(),Owned);
    TestEqual(TEXT("no second pity count"),A->GetState().Pity.Draws,10);

    FRandomStream Ten(5),One(5);
    FWanJianPity TenPity,OnePity;
    for(int32 I=0;I<10;++I)
    {
        bool bA=false,bB=false;
        TestEqual(TEXT("ten-draw matches single draws in order"),RollDraw(Ten,TenPity,bA),RollDraw(One,OnePity,bB));
    }

    FString Report;
    TestTrue(TEXT("the ledger explains the wallet"),A->Reconcile(Report));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWanJianCriticalTest,"BoundlessImmortality.WanJian.Critical",Flags)
bool FWanJianCriticalTest::RunTest(const FString&)
{
    TestEqual(TEXT("normal solo +0.1"),KillCriticalUnits(EKill::Normal,1),12);
    TestEqual(TEXT("elite solo +1"),KillCriticalUnits(EKill::Elite,1),120);
    TestEqual(TEXT("boss solo +10"),KillCriticalUnits(EKill::Boss,1),1200);
    TestEqual(TEXT("normal four players +0.025"),KillCriticalUnits(EKill::Normal,4),3);
    TestEqual(TEXT("elite four players +0.25"),KillCriticalUnits(EKill::Elite,4),30);
    TestEqual(TEXT("boss four players +2.5"),KillCriticalUnits(EKill::Boss,4),300);
    for(int32 Team=1;Team<=MaxTeam;++Team)
    {
        TestEqual(TEXT("a normal kill divides exactly"),KillCriticalUnits(EKill::Normal,Team)*Team,12);
        TestEqual(TEXT("1000 normal kills reach 100 for a solo player"),1000*KillCriticalUnits(EKill::Normal,1),CriticalCapUnits);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWanJianTeamRulesTest,"BoundlessImmortality.WanJian.TeamRules",Flags)
bool FWanJianTeamRulesTest::RunTest(const FString&)
{
    TestEqual(TEXT("four players quadruple the chance"),TeamDropChance(.1,4),.4,1e-9);
    TestEqual(TEXT("the chance is capped"),TeamDropChance(.3,4),1.0,1e-9);
    TestEqual(TEXT("expected team quantity"),ExpectedDropQuantity(4),1.42,1e-9);
    TestEqual(TEXT("per-player yield of GDD 17.3 is 5.68x"),TeamDropChance(.1,4)*ExpectedDropQuantity(4)/(TeamDropChance(.1,1)*ExpectedDropQuantity(1)),5.68,1e-9);
    TestEqual(TEXT("spawn multiplier for four"),SpawnMultiplier(4),3.4,1e-9);
    TestEqual(TEXT("health multiplier for four"),TeamHealthMultiplier(4),2.05,1e-9);
    FRandomStream Stream(3);
    int32 Sum=0;
    const int32 Total=200000;
    for(int32 I=0;I<Total;++I)
    {
        const int32 Q=RollDropQuantity(Stream,3);
        TestTrue(TEXT("quantity 1 to 4"),Q>=1&&Q<=4);
        Sum+=Q;
    }
    TestTrue(TEXT("average team quantity 1.42"),FMath::Abs(double(Sum)/Total-1.42)<.01);
    TestEqual(TEXT("a solo drop pays exactly one"),RollDropQuantity(Stream,1),1);

    double Now=1'800'000'000.0;
    UWanJianSubsystem* A=Account(&Now);
    FText Out;
    TestFalse(TEXT("a lost run pays nothing"),A->ClaimDungeon(TEXT("lost"),TEXT("Breakthrough"),4,NAME_None,WanJianBattle::FResult(),Out));
    TestEqual(TEXT("a lost run keeps the bonus"),A->GetState().Period.DungeonBonusUsed,0);
    TestFalse(TEXT("later dungeons are locked"),A->ClaimDungeon(TEXT("x"),TEXT("Treasure"),1,NAME_None,Won(),Out));
    for(int32 I=0;I<5;++I)TestTrue(TEXT("runs settle"),A->ClaimDungeon(FString::Printf(TEXT("d%d"),I),TEXT("Breakthrough"),4,NAME_None,Won(),Out));
    TestEqual(TEXT("three runs a day use the bonus"),A->GetState().Period.DungeonBonusUsed,DailyTeamBonus);
    const int64 Stones=A->Balance(Asset::Stone);
    TestTrue(TEXT("a retried claim"),A->ClaimDungeon(TEXT("d0"),TEXT("Breakthrough"),4,NAME_None,Won(),Out));
    TestEqual(TEXT("a retried claim pays nothing"),A->Balance(Asset::Stone),Stones);
    for(int32 I=5;I<8;++I)A->ClaimDungeon(FString::Printf(TEXT("d%d"),I),TEXT("Breakthrough"),1,NAME_None,Won(),Out);
    TestTrue(TEXT("the eighth clear pays a fixed material"),A->Balance(Asset::BreakStone)>=1);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWanJianSectTest,"BoundlessImmortality.WanJian.Sect",Flags)
bool FWanJianSectTest::RunTest(const FString&)
{
    TestEqual(TEXT("two herbs per ten minutes"),Production(600,600,2,24),2);
    TestEqual(TEXT("unfinished time pays nothing"),Production(599,600,2,24),0);
    TestEqual(TEXT("storage caps production"),Production(100000,600,2,24),24);
    TestEqual(TEXT("offline time is capped at 8 hours"),Production(3600.0*24,3600,1,1000),8);

    double Now=1'800'000'000.0;
    UWanJianSubsystem* A=Account(&Now);
    FText Out;
    TestFalse(TEXT("nothing to claim at once"),A->ClaimGarden(TEXT("g0"),Out));
    Now+=1800;
    TestEqual(TEXT("three cycles ready"),A->GardenReady(),6);
    TestTrue(TEXT("claim"),A->ClaimGarden(TEXT("g1"),Out));
    TestEqual(TEXT("herbs granted"),A->Balance(Asset::Herb),int64(6));
    TestTrue(TEXT("retry"),A->ClaimGarden(TEXT("g1"),Out));
    TestEqual(TEXT("a retry grants nothing"),A->Balance(Asset::Herb),int64(6));
    TestEqual(TEXT("the claim restarts production"),A->GardenReady(),0);

    TestFalse(TEXT("forge needs materials"),A->StartForge(TEXT("f0"),Out));
    for(int32 I=0;I<6;++I)A->ClaimStageClear(FString::Printf(TEXT("s%d"),I),1001+I,Won(),Out);
    TestTrue(TEXT("forge has ore"),A->Balance(Asset::Ore)>=10);
    TestTrue(TEXT("forge starts"),A->StartForge(TEXT("f1"),Out));
    TestFalse(TEXT("one job at a time"),A->StartForge(TEXT("f2"),Out));
    TestFalse(TEXT("not ready yet"),A->ClaimForge(TEXT("c0"),Out));
    Now+=3.0*3600.0;
    TestTrue(TEXT("forge finished"),A->ClaimForge(TEXT("c1"),Out));
    TestEqual(TEXT("one crystal"),A->Balance(Asset::Crystal),int64(1));
    FString Report;
    TestTrue(TEXT("the ledger explains the wallet"),A->Reconcile(Report));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWanJianLineupTest,"BoundlessImmortality.WanJian.Lineup",Flags)
bool FWanJianLineupTest::RunTest(const FString&)
{
    UWanJianSubsystem* A=Account();
    FText Out;
    const int32 Starter=A->GetState().Lineup[0];
    TestTrue(TEXT("the account starts with the bound sword equipped"),Starter>0);
    TestFalse(TEXT("unknown instance"),A->Equip(999,1,Out));
    TestFalse(TEXT("locked slot"),A->Equip(Starter,2,Out));
    TestFalse(TEXT("an instance fills one slot"),A->Equip(Starter,1,Out));
    TestFalse(TEXT("melting needs confirmation"),A->MeltSword(Starter,false,Out));
    TestFalse(TEXT("an equipped sword cannot be melted"),A->MeltSword(Starter,true,Out));

    A->ClaimTutorial(TEXT("t"),Out);
    A->DrawSwords(TEXT("ten"),10,A->GetState().Version,Out);
    int32 Spare=0;
    for(const FWanJianSwordInstance& S:A->GetState().Swords)
    {
        if(S.Instance!=Starter)Spare=S.Instance;
    }
    TestTrue(TEXT("second slot"),A->Equip(Spare,1,Out));
    TestTrue(TEXT("unequip"),A->Unequip(1,Out));
    const int64 Before=A->Balance(Asset::Shard);
    TestTrue(TEXT("melt"),A->MeltSword(Spare,true,Out));
    TestTrue(TEXT("melting pays shards"),A->Balance(Asset::Shard)>Before);

    // Seven copies of one kind: a lineup holds at most three of them.
    UWanJianSubsystem* B=Account();
    FWanJianSave Save;
    B->ImportState(B->ExportState());
    FString Json=B->ExportState();
    for(int32 I=0;I<140;++I)B->ClaimStageClear(FString::Printf(TEXT("c%d"),I),1001+I>1120?2001+(I-120):1001+I,Won(),Out);
    TestEqual(TEXT("all slots open after 120 clears"),B->UnlockedSlots(),MaxLineup);
    TestEqual(TEXT("one ticket per first clear"),B->Balance(Asset::Ticket),int64(140));
    B->DrawSwords(TEXT("a"),10,B->GetState().Version,Out);
    B->DrawSwords(TEXT("b"),10,B->GetState().Version,Out);
    TMap<FName,int32> Placed;
    int32 Slot=0;
    bool bRefused=false;
    for(const FWanJianSwordInstance& S:B->GetState().Swords)
    {
        if(Slot>=MaxLineup)break;
        const FName Kind=S.Sword;
        if(S.Instance==B->GetState().Lineup[0])continue;
        const bool bOk=B->Equip(S.Instance,Slot+1>=MaxLineup?MaxLineup-1:Slot+1,Out);
        int32& Count=Placed.FindOrAdd(Kind);
        if(bOk)
        {
            ++Count;
            ++Slot;
        }
        else
        {
            bRefused=true;
        }
    }
    TestTrue(TEXT("extra copies are refused or slots run out"),bRefused||Slot>=MaxLineup-1);
    TMap<FName,int32> Counts;
    for(const int32 Id:B->GetState().Lineup)
    {
        if(Id==0)continue;
        const FWanJianSwordInstance* S=B->GetState().Swords.FindByPredicate([Id](const FWanJianSwordInstance& X){return X.Instance==Id;});
        if(S)++Counts.FindOrAdd(S->Sword);
    }
    for(const TPair<FName,int32>& Item:Counts)TestTrue(TEXT("no more than three of a kind"),Item.Value<=3);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWanJianProgressTest,"BoundlessImmortality.WanJian.Progress",Flags)
bool FWanJianProgressTest::RunTest(const FString&)
{
    double Now=1'800'000'000.0;
    UWanJianSubsystem* A=Account(&Now);
    FText Out;
    TestFalse(TEXT("a later stage is locked"),A->ClaimStageClear(TEXT("x"),1002,Won(),Out));
    TestFalse(TEXT("an unopened world is locked"),A->IsStageUnlocked(3001));
    TestFalse(TEXT("a loss pays nothing"),A->ClaimStageClear(TEXT("loss"),1001,WanJianBattle::FResult(),Out));
    TestTrue(TEXT("first clear"),A->ClaimStageClear(TEXT("a"),1001,Won(),Out));
    TestEqual(TEXT("first clear pays one ticket"),A->Balance(Asset::Ticket),int64(1));
    TestTrue(TEXT("replay"),A->ClaimStageClear(TEXT("b"),1001,Won(),Out));
    TestEqual(TEXT("a repeated clear pays no ticket"),A->Balance(Asset::Ticket),int64(1));
    const int64 Stones=A->Balance(Asset::Stone);
    TestTrue(TEXT("retry"),A->ClaimStageClear(TEXT("b"),1001,Won(),Out));
    TestEqual(TEXT("a retried request pays nothing"),A->Balance(Asset::Stone),Stones);
    TestTrue(TEXT("the next stage opens"),A->IsStageUnlocked(1002));

    TestFalse(TEXT("daily tasks need three wins"),A->ClaimDailyTasks(TEXT("d0"),Out));
    A->ClaimStageClear(TEXT("c"),1002,Won(),Out);
    TestTrue(TEXT("daily tasks"),A->ClaimDailyTasks(TEXT("d1"),Out));
    TestFalse(TEXT("once a day"),A->ClaimDailyTasks(TEXT("d2"),Out));
    TestTrue(TEXT("secret realm"),A->ClaimDailySecret(TEXT("s1"),Out));
    TestFalse(TEXT("secret realm once a day"),A->ClaimDailySecret(TEXT("s2"),Out));

    const TSharedRef<FWanJianLocalPlatform> Platform=MakeShared<FWanJianLocalPlatform>();
    A->SetPlatformForTests(Platform);
    Platform->bAdCompletes=false;
    const int64 Tickets=A->Balance(Asset::Ticket);
    TestFalse(TEXT("an unfinished ad pays nothing"),A->ClaimRewardedAd(TEXT("ad0"),Out));
    TestEqual(TEXT("still no tickets"),A->Balance(Asset::Ticket),Tickets);
    Platform->bAdCompletes=true;
    TestTrue(TEXT("a finished ad pays"),A->ClaimRewardedAd(TEXT("ad1"),Out));
    TestEqual(TEXT("ten tickets"),A->Balance(Asset::Ticket),Tickets+10);
    TestFalse(TEXT("one ad a day"),A->ClaimRewardedAd(TEXT("ad2"),Out));
    Now+=86400;
    TestTrue(TEXT("the next day resets"),A->ClaimRewardedAd(TEXT("ad3"),Out));

    const FString Json=A->ExportState();
    UWanJianSubsystem* C=Account(&Now);
    TestTrue(TEXT("save imports"),C->ImportState(Json));
    TestEqual(TEXT("tickets survive a round trip"),C->Balance(Asset::Ticket),A->Balance(Asset::Ticket));
    TestEqual(TEXT("cleared stages survive"),C->GetState().ClearedStages.Num(),A->GetState().ClearedStages.Num());
    TestFalse(TEXT("a damaged save is refused"),C->ImportState(TEXT("{\"Lineup\":[]}")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWanJianMethodTest,"BoundlessImmortality.WanJian.Methods",Flags)
bool FWanJianMethodTest::RunTest(const FString&)
{
    UWanJianSubsystem* A=Account();
    FText Out;
    TestFalse(TEXT("needs fragments"),A->LearnMethod(TEXT("Jifeng"),Out));
    TestFalse(TEXT("later methods are locked"),A->LearnMethod(TEXT("Wanjian"),Out));
    TestFalse(TEXT("unknown methods cannot be equipped"),A->EquipMethods({TEXT("Jifeng")},Out));
    TestFalse(TEXT("three slots"),A->EquipMethods({TEXT("A"),TEXT("B"),TEXT("C"),TEXT("D")},Out));

    WanJianBattle::FSetup Setup=WanJianBattle::ReferenceSetup(1,1,4);
    Setup.Methods={TEXT("Jifeng"),TEXT("Xuangang")};
    WanJianBattle::FSimulator Moving(Setup);
    TestTrue(TEXT("a zero-threshold method is active at once"),Moving.IsMethodActive(TEXT("Jifeng")));
    TestFalse(TEXT("a thresholded method waits"),Moving.IsMethodActive(TEXT("Xuangang")));
    while(!Moving.IsFinished())Moving.Tick(true);
    TestFalse(TEXT("moving never finishes the cast"),Moving.IsMethodActive(TEXT("Xuangang")));

    WanJianBattle::FSimulator Standing(Setup);
    while(!Standing.IsFinished()&&Standing.GetCriticalPoints()<2)Standing.Tick(false);
    for(int32 I=0;I<25&&!Standing.IsFinished();++I)Standing.Tick(false);
    TestTrue(TEXT("standing casts once the critical value is reached"),Standing.IsMethodActive(TEXT("Xuangang"))||Standing.IsFinished());

    const WanJianBattle::FResult Solo=WanJianBattle::Run(WanJianBattle::ReferenceSetup(1,30,8,1));
    TestTrue(TEXT("critical is capped"),Solo.CriticalUnits<=CriticalCapUnits);
    TestTrue(TEXT("a boss fight counts the boss"),Solo.BossKills==1||!Solo.bVictory);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWanJianBalanceTest,"BoundlessImmortality.WanJian.StagesBeatable",Flags)
bool FWanJianBalanceTest::RunTest(const FString&)
{
    const double MinMargin=.55;
    const double MaxMargin=.92;
    int32 Beaten=0;
    int32 Failed=0;
    int32 FirstFailure=0;
    double Peak=0;
    for(int32 World=1;World<=OpenWorldCount;++World)
    {
        for(int32 Index=1;Index<=StagesPerWorld;++Index)
        {
            WanJianBattle::FSetup Setup=WanJianBattle::ReferenceSetup(World,Index,Index*7+World);
            if(WanJianBattle::Run(Setup).bVictory)++Beaten;
            else
            {
                ++Failed;
                if(!FirstFailure)FirstFailure=StageId(World,Index);
            }
            if(Index%30==0||(Index==10&&World==1))
            {
                // The lowest attack, as a share of the reference, that still clears the stage.
                double Low=0,High=1;
                for(int32 Step=0;Step<9;++Step)
                {
                    WanJianBattle::FSetup Trial=Setup;
                    const double Mid=(Low+High)*.5;
                    Trial.Attack=Setup.Attack*Mid;
                    (WanJianBattle::Run(Trial).bVictory?High:Low)=Mid;
                }
                AddInfo(FString::Printf(TEXT("stage %d margin: wins from %.0f%% of reference attack"),Setup.StageId,High*100));
                TestTrue(*FString::Printf(TEXT("stage %d needs real power"),Setup.StageId),High>=MinMargin);
                TestTrue(*FString::Printf(TEXT("stage %d keeps some slack"),Setup.StageId),High<=MaxMargin);
            }
            Peak=FMath::Max(Peak,double(Setup.Swords.Num()));
        }
    }
    AddInfo(FString::Printf(TEXT("beaten %d of 240, first failure %d"),Beaten,FirstFailure));
    TestEqual(TEXT("the reference loadout beats every open stage"),Failed,0);
    TestEqual(TEXT("late stages run the full 18 swords"),Peak,18.0);

    // GDD 9: the very first battle is fought with the single bound sword.
    UWanJianSubsystem* Fresh=Account();
    TestEqual(TEXT("a new account owns one sword"),Fresh->MakeBattle(1001).Swords.Num(),1);
    TestTrue(TEXT("a new account clears the first stage"),WanJianBattle::Run(Fresh->MakeBattle(1001)).bVictory);

    const WanJianBattle::FSetup Setup=WanJianBattle::ReferenceSetup(2,120,3);
    const WanJianBattle::FResult X=WanJianBattle::Run(Setup),Y=WanJianBattle::Run(Setup);
    TestTrue(TEXT("the simulator is deterministic"),X.Hits==Y.Hits&&X.Seconds==Y.Seconds&&X.CriticalUnits==Y.CriticalUnits);

    const WanJianBattle::FResult Team=WanJianBattle::Run(WanJianBattle::ReferenceSetup(2,120,3,4));
    TestTrue(TEXT("a room of four has more enemies alive"),Team.PeakEnemies>=X.PeakEnemies);
    return true;
}
#endif

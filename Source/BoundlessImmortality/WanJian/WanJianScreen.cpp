#include "WanJian/WanJianScreen.h"
#include "Cultivation/CultivationSubsystem.h"
#include "Cultivation/CultivationTypes.h"
#include "Engine/GameInstance.h"
#include "InkUI/InkUIXianTuKit.h"
#include "Misc/Guid.h"
#include "WanJian/WanJianCatalog.h"
#include "WanJian/WanJianRules.h"
#include "WanJian/WanJianSubsystem.h"
#include "Widgets/Layout/SScrollBox.h"

#define LOCTEXT_NAMESPACE "WanJianScreen"

using namespace InkUI;
using namespace InkUI::Kit;
using namespace WanJianCatalog;
using namespace WanJianRules;

namespace
{
    FString NewRequest(){return FGuid::NewGuid().ToString(EGuidFormats::Digits);}

    FLinearColor ElementColor(EWanJianElement Element)
    {
        switch(Element)
        {
        case EWanJianElement::Metal:return Hex(TEXT("E6C36A"));
        case EWanJianElement::Wood:return Hex(TEXT("6FBF73"));
        case EWanJianElement::Water:return Hex(TEXT("5AA9E6"));
        case EWanJianElement::Fire:return Hex(TEXT("E4572E"));
        case EWanJianElement::Earth:return Hex(TEXT("B08D57"));
        default:return Hex(TEXT("FFF3C4"));
        }
    }

    FLinearColor RarityColor(EWanJianRarity Rarity)
    {
        switch(Rarity)
        {
        case EWanJianRarity::Fine:return Hex(TEXT("5AA9E6"));
        case EWanJianRarity::Rare:return Hex(TEXT("B57BE8"));
        case EWanJianRarity::Legendary:return Hex(TEXT("FFD36B"));
        default:return Hex(TEXT("C9C4B6"));
        }
    }

    const TCHAR* RarityName(EWanJianRarity Rarity)
    {
        switch(Rarity)
        {
        case EWanJianRarity::Fine:return TEXT("优秀");
        case EWanJianRarity::Rare:return TEXT("稀有");
        case EWanJianRarity::Legendary:return TEXT("传说");
        default:return TEXT("普通");
        }
    }

    /** A sword as a disc: element wash, the emblem, a rarity ring. */
    TSharedRef<SWidget> SwordChip(FName Id,float Size=64.f)
    {
        return SNew(SInkPaint).DesiredSize(FVector2D(Size,Size)).OnPaint_Lambda([Id](FPainter& P,const FGeometry&)
        {
            const FSword* Def=Sword(Id);
            const FVector2f S=P.Size();
            const FVector2f Center=S*.5f;
            const float R=FMath::Min(S.X,S.Y)*.5f;
            const FLinearColor Element=Def?ElementColor(Def->Element):FLinearColor::Gray;
            P.Glow(Center,FVector2f(R,R),WithAlpha(Element,.55f));
            P.Next();
            P.Rounded(Center-FVector2f(R*.8f),FVector2f(R*1.6f),WithAlpha(Colors().InkBlack,.72f),R*.8f,Def?RarityColor(Def->Rarity):Colors().Gold,2.f);
            P.Next();
            P.BrushFit(Art::ItemIcon(TEXT("sword_emblem")),Center-FVector2f(R*.62f),FVector2f(R*1.24f),Element);
        });
    }

    FText RealmText(int32 Realm)
    {
        static const FCultivationRuleConfig Config;
        return Config.RealmNames.IsValidIndex(Realm)?Config.RealmNames[Realm]:Config.RealmNames.Last();
    }

    /** Lacquer strip of the two balances GDD chapter 9 keeps on top: 灵石 and 剑券. */
    TSharedRef<SWidget> TopWallet(TFunction<const UWanJianSubsystem*()> Get)
    {
        return SNew(SInkPaint).DesiredSize(FVector2D(320.f,48.f)).OnPaint_Lambda([Get](FPainter& P,const FGeometry&)
        {
            const auto& K=Colors();
            const FVector2f S=P.Size();
            const UWanJianSubsystem* A=Get();
            P.Skin(Art::ESkin::PlaqueLacquer,FVector2f(0.f,0.f),S,FLinearColor(1,1,1,.95f));
            P.Next();
            const struct{const TCHAR* Icon;const TCHAR* Asset;}Rows[]={{TEXT("crystal"),Asset::Stone},{TEXT("seal_scroll"),Asset::Ticket}};
            const float W=S.X*.5f;
            for(int32 I=0;I<2;++I)
            {
                P.BrushFit(Art::ItemIcon(Rows[I].Icon),FVector2f(I*W+14.f,S.Y*.5f-15.f),FVector2f(30.f,30.f));
                P.Text(FormatAmount(A?double(A->Balance(Rows[I].Asset)):0.0).ToString(),Font(EFont::Data,22.f),FVector2f((I+1)*W-14.f,S.Y*.5f),K.TextGold,FVector2f(1.f,.52f));
            }
            P.Line(FVector2f(W,S.Y*.24f),FVector2f(W,S.Y*.76f),WithAlpha(K.Gold,.35f),1.f);
        });
    }

    FText TicketSourceLabel(int32 Tickets,const FText& Name){return FText::Format(LOCTEXT("TicketSource","{0} +{1} 券"),Name,N(Tickets));}

    /** A stage on the chapter grid: the number stays legible in every state, which a skinned button cannot manage. */
    TSharedRef<SWidget> StageTile(int32 Index,bool bOpen,bool bCleared,bool bBoss,bool bPicked,FName Id,FSimpleDelegate Click)
    {
        const TSharedRef<SWidget> Tile=SNew(SInkPaint).DesiredSize(FVector2D(84.f,52.f)).OnPaint_Lambda([=](FPainter& P,const FGeometry&)
        {
            const auto& K=Colors();
            const FVector2f Inset(3.f,3.f);
            const FLinearColor Fill=!bOpen?Hex(TEXT("CFC9BA")):bPicked?K.GoldBright:bCleared?K.Jade:bBoss?K.Cinnabar:K.PaperLight;
            P.Rounded(Inset,P.Size()-Inset*2.f,Fill,10.f,bPicked?K.GoldDeep:WithAlpha(K.GoldDeep,.7f),bPicked?2.5f:1.5f);
            P.Next();
            const FLinearColor Text=!bOpen?WithAlpha(K.TextDark,.6f):(bBoss&&!bPicked)?Hex(TEXT("FFF0D8")):K.TextDark;
            P.Text(FString::FromInt(Index),Font(EFont::Data,bBoss?25.f:23.f),P.Size()*.5f,Text,FVector2f(.5f,.52f));
        });
        return SNew(SOverlay)
            +SOverlay::Slot()[Tile]
            +SOverlay::Slot()[SNew(SInkButton).Text(FText::GetEmpty()).Skin(EButtonSkin::Ghost).TestId(Id).OnClicked(Click).Enabled(bOpen).Height(52.f).MinWidth(84.f)];
    }
}

void SInkWanJianScreen::Construct(const FArguments& InArgs)
{
    InitScreen(InArgs._Root.ToSharedRef(),TEXT("WanJian"));
    const TWeakPtr<SInkWanJianScreen> Weak=StaticCastSharedRef<SInkWanJianScreen>(AsShared());
    BuildShell(LOCTEXT("Title","万剑仙宗"),LOCTEXT("Seal","剑"),
        SNew(SVerticalBox)
        +SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
        [
            TopWallet([Weak]()->const UWanJianSubsystem*{const TSharedPtr<SInkWanJianScreen> Self=Weak.Pin();return Self?Self->Account():nullptr;})
        ]);
}

UWanJianSubsystem* SInkWanJianScreen::Account()const
{
    const UXianTuSubsystem* Hub=M().XianTu.Get();
    UGameInstance* Game=Hub?Hub->GetGameInstance():nullptr;
    return Game?Game->GetSubsystem<UWanJianSubsystem>():nullptr;
}

int32 SInkWanJianScreen::Revision()const
{
    const UWanJianSubsystem* A=Account();
    return A?int32(A->GetState().Version):-1;
}

void SInkWanJianScreen::Report(bool bOk,const FText& Message)
{
    if(!Message.IsEmpty())
    {
        if(const TSharedPtr<SInkUIRoot> R=GetRoot())R->Toast(Message,bOk?1:2);
    }
    Refresh();
}

void SInkWanJianScreen::Tick(const FGeometry& AllottedGeometry,const double InCurrentTime,const float InDeltaTime)
{
    SInkXianTuPage::Tick(AllottedGeometry,InCurrentTime,InDeltaTime);
    if(!Sim||bSettled)return;
    Accumulator+=InDeltaTime*BattleSpeed;
    while(Accumulator>=WanJianBattle::StepSeconds&&!Sim->IsFinished())
    {
        Accumulator-=WanJianBattle::StepSeconds;
        Sim->Tick(false);
    }
    if(Sim->IsFinished())SettleBattle();
}

bool SInkWanJianScreen::HandleBack()
{
    if(!Sim)return false;
    CloseBattle();
    return true;
}

void SInkWanJianScreen::Rebuild()
{
    UWanJianSubsystem* A=Account();
    if(!A)
    {
        Body->SetContent(Paper(Hint(LOCTEXT("Offline","进入游戏后可打开万剑仙宗。"))));
        Footer->SetContent(SNullWidget::NullWidget);
        return;
    }
    if(Sim)
    {
        Body->SetContent(BuildBattleView());
        Footer->SetContent(SNullWidget::NullWidget);
        return;
    }
    switch(Tab)
    {
    case 1:Body->SetContent(BuildDungeonTab());break;
    case 2:Body->SetContent(BuildLineupTab());break;
    case 3:Body->SetContent(BuildSummonTab());break;
    case 4:Body->SetContent(BuildSectTab());break;
    case 5:Body->SetContent(BuildCultivationTab());break;
    default:Body->SetContent(BuildWorldTab());break;
    }
    Footer->SetContent(Tabs(TEXT("WanJian.Nav"),{LOCTEXT("Nav0","世界"),LOCTEXT("Nav1","副本"),LOCTEXT("Nav2","剑阵"),LOCTEXT("Nav3","召剑"),LOCTEXT("Nav4","宗门"),LOCTEXT("Nav5","修仙")}));
}

// ------------------------------------------------------------------ 世界

TSharedRef<SWidget> SInkWanJianScreen::BuildWorldTab()
{
    UWanJianSubsystem* A=Account();
    TArray<TSharedRef<SWidget>> WorldCards;
    for(const FWorld& Def:Worlds())
    {
        const int32 Id=Def.Id;
        WorldCards.Add(Btn(T(Def.Name),Id==WorldPick?EButtonSkin::Gold:EButtonSkin::Paper,*FString::Printf(TEXT("WanJian.World.%d"),Id),
            Guarded([this,Id]{WorldPick=Id;ChapterPick=1;StagePick=0;Refresh();}),Def.bOpen,46.f,120.f,18.f));
    }

    TArray<TSharedRef<SWidget>> ChapterCards;
    for(int32 Chapter=1;Chapter<=4;++Chapter)
    {
        const FText Label=FText::Format(LOCTEXT("Chapter","{0}-{1} 关"),N((Chapter-1)*StagesPerChapter+1),N(Chapter*StagesPerChapter));
        ChapterCards.Add(Btn(Label,Chapter==ChapterPick?EButtonSkin::Jade:EButtonSkin::Paper,*FString::Printf(TEXT("WanJian.Chapter.%d"),Chapter),
            Guarded([this,Chapter]{ChapterPick=Chapter;StagePick=0;Refresh();}),true,50.f,120.f,19.f));
    }

    TArray<TSharedRef<SWidget>> StageCells;
    for(int32 I=1;I<=StagesPerChapter;++I)
    {
        const int32 Index=(ChapterPick-1)*StagesPerChapter+I;
        const int32 Id=StageId(WorldPick,Index);
        const bool bCleared=A->IsStageCleared(Id);
        const bool bOpen=A->IsStageUnlocked(Id);
        const bool bBoss=Index%StagesPerChapter==0;
        StageCells.Add(StageTile(Index,bOpen||bCleared,bCleared,bBoss,Id==StagePick,*FString::Printf(TEXT("WanJian.Stage.%d"),Id),Guarded([this,Id]{StagePick=Id;Refresh();})));
    }

    TSharedRef<SWidget> Detail=Paper(Hint(LOCTEXT("PickStage","选择一关查看详情。")));
    FStage Stage;
    int32 W=0,Index=0;
    if(StagePick&&SplitStageId(StagePick,W,Index)&&MakeStage(W,Index,Stage))
    {
        const bool bCleared=A->IsStageCleared(StagePick);
        const int32 Id=StagePick;
        Detail=Paper(Rows({
            Head(FText::Format(LOCTEXT("StageHead","{0} · 第 {1} 关{2}"),T(Worlds()[W-1].Name),N(Index),Stage.bWorldBoss?LOCTEXT("WorldBoss"," · 世界首领"):Stage.bBoss?LOCTEXT("ChapterBoss"," · 阶段首领"):FText::GetEmpty())),
            Line(FText::Format(LOCTEXT("StageInfo","普通怪 {0} · 精英 {1} · 限时约 {2} 秒"),N(Stage.Normals),N(Stage.Elites),N(Stage.Duration))),
            Hint(bCleared?LOCTEXT("Replay","已通关：重复挑战有常规掉落，不再发放剑券。"):LOCTEXT("FirstClear","首次通关：+1 剑券，并按世界掉落灵石、修为与物资。")),
            Btn(LOCTEXT("Start","开始挑战"),EButtonSkin::Gold,TEXT("WanJian.Start"),Guarded([this,Id]{StartStage(Id);}),true,62.f,220.f,24.f)
        }));
    }

    return Rows({
        Appear(Paper(Rows({Grid(WorldCards,4,4.f),Hint(LOCTEXT("WorldsLater","灰色世界后续开放。"))},2.f),FMargin(20.f,14.f,20.f,12.f)),0),
        Appear(Paper(Rows({Grid(ChapterCards,4,6.f),Grid(StageCells,6,4.f)})),1),
        Appear(Detail,2)
    });
}

// ------------------------------------------------------------------ 副本

TSharedRef<SWidget> SInkWanJianScreen::BuildDungeonTab()
{
    UWanJianSubsystem* A=Account();
    const int32 BonusLeft=FMath::Max(0,DailyTeamBonus-A->GetState().Period.DungeonBonusUsed);

    TArray<TSharedRef<SWidget>> TeamButtons;
    for(int32 Team=1;Team<=MaxTeam;++Team)
    {
        const FText Label=Team==1?LOCTEXT("Solo","单人"):FText::Format(LOCTEXT("Team","{0} 人"),N(Team));
        TeamButtons.Add(Btn(Label,Team==TeamPick?EButtonSkin::Jade:EButtonSkin::Paper,*FString::Printf(TEXT("WanJian.Team.%d"),Team),Guarded([this,Team]{TeamPick=Team;Refresh();}),true,50.f,110.f,20.f));
    }

    TArray<TSharedRef<SWidget>> MethodButtons;
    for(const FMethod& Def:Methods())
    {
        if(!Def.bFirstRelease)continue;
        const FName Id=Def.Id;
        MethodButtons.Add(Btn(T(Def.Name),Id==MethodPick?EButtonSkin::Jade:EButtonSkin::Paper,*FString::Printf(TEXT("WanJian.Fragment.%s"),*Id.ToString()),Guarded([this,Id]{MethodPick=Id;Refresh();}),true,50.f,120.f,19.f));
    }

    const TSharedRef<SVerticalBox> List=SNew(SVerticalBox);
    int32 Order=2;
    for(const FDungeon& Def:Dungeons())
    {
        const FName Id=Def.Id;
        const double Chance=TeamDropChance(Def.BaseChance,TeamPick);
        const int32 Progress=A->GetState().DungeonProgress.FindRef(Id);
        const TSharedRef<SWidget> Action=Def.bOpen
            ?Btn(LOCTEXT("Enter","进入"),EButtonSkin::Gold,*FString::Printf(TEXT("WanJian.Dungeon.%s"),*Id.ToString()),Guarded([this,Id]{StartDungeon(Id);}),true,56.f,120.f,22.f)
            :Plaque(LOCTEXT("Later","后续开放"),1,120.f,40.f);
        List->AddSlot().AutoHeight().Padding(FMargin(0.f,5.f))[Appear(Paper(
            SNew(SHorizontalBox)
            +SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
            [
                Rows({
                    Head(T(Def.Name)),
                    Line(T(Def.Focus),21.f),
                    Hint(FText::Format(LOCTEXT("DungeonOdds","当前掉率 {0}% · 累计奖励 {1}/{2}"),N(FMath::RoundToInt(Chance*100)),N(Progress),N(Def.FixedEvery)))
                })
            ]
            +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Action]),Order++)];
    }

    return Rows({
        Appear(Paper(Rows({
            Head(LOCTEXT("TeamHead","队伍人数")),
            Grid(TeamButtons,4,6.f),
            Hint(FText::Format(LOCTEXT("TeamHint","今日剩余 {0} 次组队加成。队友由本地模拟，联机需要房间服务。"),N(BonusLeft)))
        })),0),
        Appear(Paper(Rows({Head(LOCTEXT("FragmentHead","剑诀残卷目标")),Grid(MethodButtons,3,6.f)})),1),
        List
    });
}

// ------------------------------------------------------------------ 剑阵

void SInkWanJianScreen::AutoFillLineup()
{
    UWanJianSubsystem* A=Account();
    TArray<FWanJianSwordInstance> Owned=A->GetState().Swords;
    Owned.Sort([](const FWanJianSwordInstance& X,const FWanJianSwordInstance& Y)
    {
        const FSword* DX=Sword(X.Sword);
        const FSword* DY=Sword(Y.Sword);
        return int32(DX->Rarity)!=int32(DY->Rarity)?int32(DX->Rarity)>int32(DY->Rarity):X.Level>Y.Level;
    });
    FText Ignored;
    for(int32 Slot=0;Slot<MaxLineup;++Slot)A->Unequip(Slot,Ignored);
    int32 Slot=0;
    for(const FWanJianSwordInstance& S:Owned)
    {
        if(Slot>=A->UnlockedSlots())break;
        if(A->Equip(S.Instance,Slot,Ignored))++Slot;
    }
    Report(true,LOCTEXT("AutoFilled","已按稀有度推荐上阵"));
}

TSharedRef<SWidget> SInkWanJianScreen::BuildLineupTab()
{
    UWanJianSubsystem* A=Account();
    const FWanJianSave& St=A->GetState();
    const int32 Unlocked=A->UnlockedSlots();

    TArray<TSharedRef<SWidget>> Slots;
    int32 Equipped=0;
    for(int32 Index=0;Index<MaxLineup;++Index)
    {
        const int32 Instance=St.Lineup[Index];
        const FWanJianSwordInstance* Owned=St.Swords.FindByPredicate([Instance](const FWanJianSwordInstance& S){return S.Instance==Instance;});
        Equipped+=Owned?1:0;
        const bool bLocked=Index>=Unlocked;
        TSharedRef<SWidget> SlotArt=SNew(SInkPaint).DesiredSize(FVector2D(76.f,76.f)).OnPaint_Lambda([bLocked](FPainter& P,const FGeometry&)
        {
            const auto& K=Colors();
            const FVector2f S=P.Size();
            P.Rounded(FVector2f(4.f,4.f),S-FVector2f(8.f,8.f),WithAlpha(K.InkBlack,bLocked?.25f:.5f),10.f,WithAlpha(K.Gold,bLocked?.2f:.75f),1.5f);
        });
        if(Owned)
        {
            SlotArt=SNew(SOverlay)+SOverlay::Slot()[SlotArt]+SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)[SwordChip(Owned->Sword,62.f)];
        }
        const FSimpleDelegate Clear=Guarded([this,Index]{FText Out;const bool bOk=Account()->Unequip(Index,Out);Report(bOk,Out);});
        Slots.Add(SNew(SOverlay)
            +SOverlay::Slot()[SlotArt]
            +SOverlay::Slot()
            [
                SNew(SInkButton).Text(FText::GetEmpty()).Skin(EButtonSkin::Ghost).TestId(*FString::Printf(TEXT("WanJian.Slot.%d"),Index)).OnClicked(Clear).Enabled(Owned!=nullptr).Height(76.f).MinWidth(76.f)
            ]);
    }

    TArray<FWanJianSwordInstance> Owned=St.Swords;
    Owned.RemoveAll([&St](const FWanJianSwordInstance& S){return St.Lineup.Contains(S.Instance);});
    Owned.Sort([](const FWanJianSwordInstance& X,const FWanJianSwordInstance& Y){return int32(Sword(X.Sword)->Rarity)>int32(Sword(Y.Sword)->Rarity);});
    const TSharedRef<SVerticalBox> List=SNew(SVerticalBox);
    const int32 Shown=FMath::Min(Owned.Num(),40);
    for(int32 I=0;I<Shown;++I)
    {
        const FWanJianSwordInstance S=Owned[I];
        const FSword* Def=Sword(S.Sword);
        const int32 Instance=S.Instance;
        const int32 FreeSlot=St.Lineup.IndexOfByPredicate([](int32 V){return V==0;});
        const bool bCanEquip=FreeSlot!=INDEX_NONE&&FreeSlot<Unlocked;
        List->AddSlot().AutoHeight().Padding(FMargin(0.f,4.f))[Paper(
            SNew(SHorizontalBox)
            +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(0.f,0.f,12.f,0.f))[SwordChip(S.Sword,60.f)]
            +SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
            [
                Rows({
                    Line(FText::Format(LOCTEXT("SwordRow","{0}  Lv.{1}"),T(Def->Name),N(S.Level)),23.f),
                    Hint(FText::Format(LOCTEXT("SwordInfo","{0} · {1}系 · {2}"),T(RarityName(Def->Rarity)),T(ElementName(Def->Element)),T(Def->Role)))
                },2.f)
            ]
            +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(6.f,0.f,0.f,0.f))
            [
                Btn(LOCTEXT("Equip","上阵"),EButtonSkin::Jade,*FString::Printf(TEXT("WanJian.Equip.%d"),Instance),Guarded([this,Instance,FreeSlot]{FText Out;const bool bOk=Account()->Equip(Instance,FreeSlot,Out);Report(bOk,Out);}),bCanEquip,50.f,84.f,20.f)
            ]
            +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(6.f,0.f,0.f,0.f))
            [
                Btn(LOCTEXT("Melt","熔炼"),EButtonSkin::Cinnabar,*FString::Printf(TEXT("WanJian.Melt.%d"),Instance),Guarded([this,Instance]
                {
                    if(const TSharedPtr<SInkUIRoot> R=GetRoot())
                    {
                        R->Confirm(LOCTEXT("MeltTitle","熔炼剑灵"),LOCTEXT("MeltBody","熔炼后获得剑魂碎片，此操作不可撤销。"),Guarded([this,Instance]{FText Out;const bool bOk=Account()->MeltSword(Instance,true,Out);Report(bOk,Out);}));
                    }
                }),true,50.f,84.f,20.f)
            ],FMargin(18.f,10.f,18.f,10.f))];
    }

    return Rows({
        Appear(Paper(Rows({
            SNew(SHorizontalBox)
            +SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)[Head(FText::Format(LOCTEXT("LineupHead","十八剑阵 · 上阵 {0}/{1}"),N(Equipped),N(Unlocked)))]
            +SHorizontalBox::Slot().AutoWidth()[Btn(LOCTEXT("AutoFill","一键推荐"),EButtonSkin::Gold,TEXT("WanJian.AutoFill"),Guarded([this]{AutoFillLineup();}),true,52.f,130.f,20.f)],
            Grid(Slots,6,4.f),
            Hint(LOCTEXT("LineupHint","同种普通剑最多上阵 3 柄，七星光芒剑最多 1 柄。点已上阵的剑可卸下。"))
        })),0),
        Appear(List,1)
    });
}

// ------------------------------------------------------------------ 召剑

TSharedRef<SWidget> SInkWanJianScreen::BuildSummonTab()
{
    UWanJianSubsystem* A=Account();
    const FWanJianSave& St=A->GetState();
    const int64 Tickets=A->Balance(Asset::Ticket);
    const bool bOwned=St.Pity.bSevenStarOwned;
    const float Fraction=bOwned?1.f:float(St.Pity.Draws)/PityDraws;
    const FText PityText=bOwned?LOCTEXT("PityDone","已获得七星光芒剑"):FText::Format(LOCTEXT("PityCount","{0} / {1}"),N(St.Pity.Draws),N(PityDraws));

    auto Draw=[this](int32 Count)
    {
        return Guarded([this,Count]
        {
            UWanJianSubsystem* Svc=Account();
            FText Out;
            const FWanJianDrawReceipt* Receipt=Svc->DrawSwords(NewRequest(),Count,Svc->GetState().Version,Out);
            if(Receipt)LastDraw=Receipt->Results;
            Report(Receipt!=nullptr,Out);
        });
    };
    auto Claim=[this](TFunction<bool(UWanJianSubsystem*,const FString&,FText&)> Call)
    {
        return Guarded([this,Call]{FText Out;const bool bOk=Call(Account(),NewRequest(),Out);Report(bOk,Out);});
    };

    TSharedRef<SWidget> Result=SNullWidget::NullWidget;
    if(LastDraw.Num())
    {
        const TSharedRef<SWrapBox> Box=SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(8.f,8.f));
        for(const FWanJianDrawResult& R:LastDraw)Box->AddSlot()[SwordChip(R.Sword,72.f)];
        Result=Paper(Rows({Head(LOCTEXT("ResultHead","本次所得")),Box}));
    }

    const FWanJianPeriod& Day=St.Period;
    return Rows({
        Appear(Paper(Rows({
            Scene(0,170.f,false),
            Head(LOCTEXT("PoolHead","七星光芒剑 · 首获保底")),
            Meter(Fraction,PityText,EBar::Gold),
            Hint(LOCTEXT("PoolRule","未获得前累计每抽 +1，第 500 抽必得；计数跨设备保存、永不清零；提前获得即完成保底。")),
            SNew(SHorizontalBox)
            +SHorizontalBox::Slot().FillWidth(1.f).Padding(FMargin(0.f,0.f,5.f,0.f))[Btn(LOCTEXT("DrawOne","抽一次 · 1 券"),EButtonSkin::Jade,TEXT("WanJian.DrawOne"),Draw(1),Tickets>=1,60.f,200.f,23.f)]
            +SHorizontalBox::Slot().FillWidth(1.f).Padding(FMargin(5.f,0.f,0.f,0.f))[Btn(LOCTEXT("DrawTen","十连 · 10 券"),EButtonSkin::Gold,TEXT("WanJian.DrawTen"),Draw(10),Tickets>=10,60.f,200.f,23.f)]
        })),0),
        Result,
        Appear(Paper(Rows({
            Head(LOCTEXT("OddsHead","奖池概率")),
            Pair(LOCTEXT("OddsCommon","普通 赤炎/玄冰/惊雷/磐岳"),FText::FromString(TEXT("72.0%")),20.f),
            Pair(LOCTEXT("OddsFine","优秀 破甲/焚风/归潮"),FText::FromString(TEXT("23.0%")),20.f),
            Pair(LOCTEXT("OddsRare","稀有 续命/镇岳"),FText::FromString(TEXT("4.5%")),20.f),
            Pair(LOCTEXT("OddsLegend","传说 星痕 0.4% / 七星 0.1%"),FText::FromString(TEXT("0.5%")),20.f),
            Hint(LOCTEXT("OddsNote","重复获得可保留或主动熔炼为剑魂碎片。本页概率为待测试的策划值。"))
        })),1),
        Appear(Paper(Rows({
            Head(LOCTEXT("TicketHead","获取剑券")),
            Btn(TicketSourceLabel(10,LOCTEXT("SrcTutorial","新手引导")),EButtonSkin::Paper,TEXT("WanJian.Claim.Tutorial"),Claim([](UWanJianSubsystem* S,const FString& R,FText& O){return S->ClaimTutorial(R,O);}),!St.bTutorialRewarded,52.f,400.f,21.f),
            Btn(TicketSourceLabel(8,FText::Format(LOCTEXT("SrcDaily","每日任务（胜利 {0}/3）"),N(FMath::Min(Day.WinsToday,3)))),EButtonSkin::Paper,TEXT("WanJian.Claim.Daily"),Claim([](UWanJianSubsystem* S,const FString& R,FText& O){return S->ClaimDailyTasks(R,O);}),!Day.bDailyTasksClaimed&&Day.WinsToday>=3,52.f,400.f,21.f),
            Btn(TicketSourceLabel(3,LOCTEXT("SrcSecret","每日秘境首通")),EButtonSkin::Paper,TEXT("WanJian.Claim.Secret"),Claim([](UWanJianSubsystem* S,const FString& R,FText& O){return S->ClaimDailySecret(R,O);}),!Day.bSecretClaimed&&Day.WinsToday>=1,52.f,400.f,21.f),
            Btn(TicketSourceLabel(14,FText::Format(LOCTEXT("SrcWeekly","每周修炼（胜利 {0}/15）"),N(FMath::Min(Day.WinsThisWeek,15)))),EButtonSkin::Paper,TEXT("WanJian.Claim.Weekly"),Claim([](UWanJianSubsystem* S,const FString& R,FText& O){return S->ClaimWeekly(R,O);}),!Day.bWeeklyClaimed&&Day.WinsThisWeek>=15,52.f,400.f,21.f),
            Btn(TicketSourceLabel(10,LOCTEXT("SrcAd","观看激励视频（自愿，每日 1 次）")),EButtonSkin::Paper,TEXT("WanJian.Claim.Ad"),Claim([](UWanJianSubsystem* S,const FString& R,FText& O){return S->ClaimRewardedAd(R,O);}),!Day.bAdClaimed,52.f,400.f,21.f),
            Hint(LOCTEXT("TicketNote","本地验证：广告为模拟结果，账本存在本机。公开版需服务端权威结算。"))
        })),2)
    });
}

// ------------------------------------------------------------------ 宗门

TSharedRef<SWidget> SInkWanJianScreen::BuildSectTab()
{
    UWanJianSubsystem* A=Account();
    const FWanJianSave& St=A->GetState();
    auto LevelOf=[&St](FName Id){const FWanJianBuilding* B=St.Buildings.FindByPredicate([Id](const FWanJianBuilding& X){return X.Id==Id;});return B?B->Level:1;};
    auto Upgrade=[this](FName Id){return Guarded([this,Id]{FText Out;const bool bOk=Account()->UpgradeBuilding(Id,Out);Report(bOk,Out);});};

    const int32 Ready=A->GardenReady();
    const double ForgeLeft=St.Forge.bActive?FMath::Max(0.0,St.Forge.ReadyUtc-UCultivationSubsystem::UtcNow()):0.0;

    const TSharedRef<SWrapBox> Elements=SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(6.f,6.f));
    for(const EWanJianElement Element:{EWanJianElement::Metal,EWanJianElement::Wood,EWanJianElement::Water,EWanJianElement::Fire,EWanJianElement::Earth})
    {
        const FName Key(ElementName(Element));
        Elements->AddSlot()[Btn(FText::Format(LOCTEXT("ResearchBtn","{0}系 {1}/5"),T(ElementName(Element)),N(St.Research.FindRef(Key))),EButtonSkin::Paper,*FString::Printf(TEXT("WanJian.Research.%d"),int32(Element)),
            Guarded([this,Element]{FText Out;const bool bOk=Account()->Research(Element,NewRequest(),Out);Report(bOk,Out);}),true,50.f,110.f,20.f)];
    }

    auto Card=[&](FName Id,const FText& Status,TSharedRef<SWidget> Actions,int32 Order)
    {
        const FBuilding* Def=Buildings().FindByPredicate([Id](const FBuilding& B){return B.Id==Id;});
        const int32 Level=LevelOf(Id);
        return Appear(Paper(Rows({
            SNew(SHorizontalBox)
            +SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)[Head(FText::Format(LOCTEXT("BuildingHead","{0} · {1} 级"),T(Def->Name),N(Level)))]
            +SHorizontalBox::Slot().AutoWidth()[Btn(LOCTEXT("Upgrade","升级"),EButtonSkin::Paper,*FString::Printf(TEXT("WanJian.Upgrade.%s"),*Id.ToString()),Upgrade(Id),Level<3,50.f,100.f,20.f)],
            Line(T(Def->Effect),21.f),
            Hint(Status),
            Actions
        })),Order);
    };

    return Rows({
        Card(TEXT("Garden"),FText::Format(LOCTEXT("GardenStatus","可收取 {0} 灵草，仓储上限 {1}，离线最多累计 8 小时"),N(Ready),N(GardenStorage(LevelOf(TEXT("Garden"))))),
            Btn(LOCTEXT("Collect","收取灵草"),EButtonSkin::Jade,TEXT("WanJian.Garden"),Guarded([this]{FText Out;const bool bOk=Account()->ClaimGarden(NewRequest(),Out);Report(bOk,Out);}),Ready>0,56.f,200.f,22.f),0),
        Card(TEXT("Forge"),St.Forge.bActive?FText::Format(LOCTEXT("ForgeBusy","炼制中，剩余 {0}"),XianTuRules::Clock(ForgeLeft)):LOCTEXT("ForgeIdle","炉火已熄，可投入新的炼制"),
            St.Forge.bActive
            ?Btn(LOCTEXT("ForgeClaim","取出剑魂晶"),EButtonSkin::Gold,TEXT("WanJian.ForgeClaim"),Guarded([this]{FText Out;const bool bOk=Account()->ClaimForge(NewRequest(),Out);Report(bOk,Out);}),ForgeLeft<=0,56.f,200.f,22.f)
            :Btn(LOCTEXT("ForgeStart","开炉炼制"),EButtonSkin::Jade,TEXT("WanJian.ForgeStart"),Guarded([this]{FText Out;const bool bOk=Account()->StartForge(NewRequest(),Out);Report(bOk,Out);}),true,56.f,200.f,22.f),1),
        Card(TEXT("Library"),LOCTEXT("LibraryStatus","每条研究有明确上限，消耗随等级增加"),Elements,2)
    });
}

// ------------------------------------------------------------------ 修仙

TSharedRef<SWidget> SInkWanJianScreen::BuildCultivationTab()
{
    UWanJianSubsystem* A=Account();
    const FWanJianSave& St=A->GetState();
    const int64 NeedQi=BreakthroughQi(St.Realm);
    const int32 NeedStones=BreakthroughStones(St.Realm);
    const int64 Qi=A->Balance(Asset::Qi);
    const int64 Stones=A->Balance(Asset::BreakStone);

    const TSharedRef<SWrapBox> Roots=SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(6.f,6.f));
    for(const EWanJianElement Element:{EWanJianElement::Metal,EWanJianElement::Wood,EWanJianElement::Water,EWanJianElement::Fire,EWanJianElement::Earth})
    {
        Roots->AddSlot()[Btn(T(ElementName(Element)),St.Root==Element?EButtonSkin::Gold:EButtonSkin::Paper,*FString::Printf(TEXT("WanJian.Root.%d"),int32(Element)),
            Guarded([this,Element]{FText Out;const bool bOk=Account()->ChooseRoot(Element,Out);Report(bOk,Out);}),true,56.f,84.f,24.f)];
    }

    const TSharedRef<SVerticalBox> MethodList=SNew(SVerticalBox);
    int32 Order=2;
    for(const FMethod& Def:Methods())
    {
        const FName Id=Def.Id;
        const bool bLearned=St.LearnedMethods.Contains(Id);
        const bool bEquipped=St.EquippedMethods.Contains(Id);
        const int64 Fragments=A->Balance(MethodFragmentAsset(Id));
        TSharedRef<SWidget> Action=Plaque(LOCTEXT("MethodLater","后续开放"),1,120.f,40.f);
        if(Def.bFirstRelease&&!bLearned)
        {
            Action=Btn(LOCTEXT("Learn","参悟"),EButtonSkin::Jade,*FString::Printf(TEXT("WanJian.Learn.%s"),*Id.ToString()),Guarded([this,Id]{FText Out;const bool bOk=Account()->LearnMethod(Id,Out);Report(bOk,Out);}),Fragments>=Def.Fragments,52.f,110.f,21.f);
        }
        else if(bLearned)
        {
            Action=Btn(bEquipped?LOCTEXT("Unequip","卸下"):LOCTEXT("EquipMethod","配置"),bEquipped?EButtonSkin::Paper:EButtonSkin::Gold,*FString::Printf(TEXT("WanJian.Method.%s"),*Id.ToString()),Guarded([this,Id,bEquipped]
            {
                TArray<FName> Next=Account()->GetState().EquippedMethods;
                if(bEquipped)Next.Remove(Id);
                else Next.Add(Id);
                FText Out;
                const bool bOk=Account()->EquipMethods(Next,Out);
                Report(bOk,Out);
            }),true,52.f,110.f,21.f);
        }
        MethodList->AddSlot().AutoHeight().Padding(FMargin(0.f,4.f))[Appear(Paper(
            SNew(SHorizontalBox)
            +SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
            [
                Rows({
                    Line(FText::Format(LOCTEXT("MethodName","{0} · 临界 {1}"),T(Def.Name),N(Def.Critical)),23.f),
                    Hint(T(Def.Effect)),
                    Hint(bLearned?LOCTEXT("MethodKnown","已参悟"):FText::Format(LOCTEXT("MethodFrag","残页 {0}/{1}"),N(Fragments),N(Def.Fragments)))
                },2.f)
            ]
            +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Action],FMargin(18.f,12.f,18.f,12.f)),Order++)];
    }

    return Rows({
        Appear(Paper(Rows({
            Head(RealmText(St.Realm),32.f),
            Meter(NeedQi>0?float(FMath::Min<double>(1.0,double(Qi)/NeedQi)):1.f,FText::Format(LOCTEXT("QiMeter","修为 {0}/{1}"),N(Qi),N(NeedQi)),EBar::Qi),
            Pair(LOCTEXT("StonesKey","破境石"),FText::Format(LOCTEXT("StonesValue","{0}/{1}"),N(Stones),N(NeedStones))),
            Btn(LOCTEXT("Break","境界突破"),EButtonSkin::Gold,TEXT("WanJian.Break"),Guarded([this]{FText Out;const bool bOk=Account()->Breakthrough(NewRequest(),Out);Report(bOk,Out);}),Qi>=NeedQi&&Stones>=NeedStones,62.f,240.f,24.f)
        })),0),
        Appear(Paper(Rows({
            Head(FText::Format(LOCTEXT("RootHead","五行灵根 · {0}"),St.Root==EWanJianElement::None?LOCTEXT("RootNone","未选择"):T(ElementName(St.Root)))),
            Roots,
            Hint(LOCTEXT("RootHint","主灵根使同系剑灵效果 +10%，不影响其他剑灵上阵。新手可免费改一次，之后每日一次。"))
        })),1),
        MethodList
    });
}

// ------------------------------------------------------------------ 战斗

void SInkWanJianScreen::StartBattle(const WanJianBattle::FSetup& Setup)
{
    if(Setup.Swords.IsEmpty())
    {
        Report(false,LOCTEXT("NoSwords","剑阵里还没有剑灵"));
        return;
    }
    Sim=MakeUnique<WanJianBattle::FSimulator>(Setup);
    BattleTeam=Setup.Team;
    BattleSwords=Setup.Swords.Num()*Setup.Team;
    Accumulator=0;
    bSettled=false;
    Outcome=WanJianBattle::FResult();
    OutcomeText=FText::GetEmpty();
    Refresh();
}

void SInkWanJianScreen::StartStage(int32 Id)
{
    BattleStage=Id;
    BattleDungeon=NAME_None;
    StartBattle(Account()->MakeBattle(Id,1));
}

void SInkWanJianScreen::StartDungeon(FName Dungeon)
{
    const UWanJianSubsystem* A=Account();
    // Dungeon battles use the player's current frontier so they stay relevant; a dedicated encounter table comes later.
    int32 Frontier=StageId(1,1);
    for(const int32 Id:A->GetState().ClearedStages)Frontier=FMath::Max(Frontier,Id);
    BattleStage=Frontier;
    BattleDungeon=Dungeon;
    StartBattle(A->MakeBattle(Frontier,TeamPick));
}

void SInkWanJianScreen::SettleBattle()
{
    bSettled=true;
    Outcome=Sim->GetResult();
    UWanJianSubsystem* A=Account();
    if(Outcome.bVictory)
    {
        FText Out;
        const bool bFirst=BattleDungeon.IsNone()&&!A->IsStageCleared(BattleStage);
        const bool bOk=BattleDungeon.IsNone()
            ?A->ClaimStageClear(NewRequest(),BattleStage,Outcome,Out)
            :A->ClaimDungeon(NewRequest(),BattleDungeon,BattleTeam,MethodPick,Outcome,Out);
        OutcomeText=!bOk?Out:bFirst?LOCTEXT("FirstReward","首次通关：获得 1 张剑券与关卡奖励"):LOCTEXT("Reward","已结算本次奖励");
    }
    else
    {
        OutcomeText=LOCTEXT("Lost","未能通关，本次没有奖励");
    }
    Refresh();
}

void SInkWanJianScreen::CloseBattle()
{
    Sim.Reset();
    bSettled=false;
    Refresh();
}

void SInkWanJianScreen::PaintArena(FPainter& P)const
{
    const auto& K=Colors();
    const FVector2f S=P.Size();
    const double Now=Motion::Now();
    P.VGradient(FVector2f(0.f,0.f),S,{{0.f,Hex(TEXT("141B33"))},{.7f,Hex(TEXT("1F2A3A"))},{1.f,Hex(TEXT("2B3A34"))}},10.f);
    P.Next();
    if(!Sim)return;

    const float Top=26.f;
    const float Goal=S.Y-92.f;
    const FVector2f Hero(S.X*.5f,S.Y-52.f);
    P.FadeLine(FVector2f(30.f,Goal),FVector2f(S.X-30.f,Goal),WithAlpha(K.Gold,.55f),2.f,.1f);
    P.Glow(Hero,FVector2f(120.f,70.f),WithAlpha(K.GoldBright,.3f));
    P.Next();

    for(const WanJianBattle::FEnemy& E:Sim->GetEnemies())
    {
        if(!E.IsAlive())continue;
        const float Lane=.1f+.8f*(FMath::Frac(E.Id*.61803f));
        const float Y=FMath::Lerp(Goal,Top,float(E.Distance/1000.0));
        const bool bBoss=E.Kind==EKill::Boss;
        const bool bElite=E.Kind==EKill::Elite;
        const float R=bBoss?34.f:bElite?20.f:12.f;
        const FVector2f Center(bBoss?S.X*.5f:S.X*Lane,Y);
        const FLinearColor Color=bBoss?K.CinnabarBright:bElite?Hex(TEXT("B57BE8")):Hex(TEXT("7A8AA8"));
        if(bBoss||bElite)P.Glow(Center,FVector2f(R*1.7f,R*1.7f),WithAlpha(Color,.4f));
        P.Rounded(Center-FVector2f(R),FVector2f(R*2.f),WithAlpha(Color,.92f),R,WithAlpha(K.InkBlack,.8f),1.5f);
        const float Width=R*2.2f;
        P.Rounded(Center+FVector2f(-Width*.5f,-R-9.f),FVector2f(Width,4.f),WithAlpha(K.InkBlack,.7f),2.f);
        P.Rounded(Center+FVector2f(-Width*.5f,-R-9.f),FVector2f(Width*float(E.Health/E.MaxHealth),4.f),K.Cinnabar,2.f);
    }
    P.Next();

    // Swords hold a ring around the player, biased to the enemy side so the formation reads at a glance.
    const int32 Count=FMath::Min(BattleSwords,72);
    for(int32 I=0;I<Count;++I)
    {
        const float Ring=46.f+(I/18)*18.f;
        const float A=float(Now)*.9f+I*(2.f*PI/FMath::Min(Count,18));
        const FVector2f Dir(FMath::Cos(A),FMath::Sin(A)*.5f-.25f);
        const FVector2f Pos=Hero+Dir*Ring;
        P.Line(Pos-Dir*9.f,Pos+Dir*9.f,WithAlpha(K.GoldBright,.95f),2.5f);
    }
    P.Next();
    P.BrushFit(Art::ItemIcon(TEXT("lotus_orb")),Hero-FVector2f(26.f),FVector2f(52.f),FLinearColor::White);
    P.Next();
    const int32 Active=Sim->GetResult().ActiveMethods.Num();
    P.Text(FString::Printf(TEXT("%.0f s"),Sim->GetTime()),Font(EFont::Data,24.f),FVector2f(18.f,22.f),K.TextGold,FVector2f(0.f,.5f));
    P.Text(FString::Printf(TEXT("功法 %d"),Active),Font(EFont::Small,20.f),FVector2f(S.X-18.f,22.f),K.TextLight,FVector2f(1.f,.5f));
}

TSharedRef<SWidget> SInkWanJianScreen::BuildSettlement()
{
    const int32 Stage=BattleStage;
    const FName Dungeon=BattleDungeon;
    const bool bWon=Outcome.bVictory;
    const FSimpleDelegate Retry=Guarded([this,Stage,Dungeon]{if(Dungeon.IsNone())StartStage(Stage);else StartDungeon(Dungeon);});

    // GDD 17.4: retry, adjust and invite are equally visible; sharing never revives or pays by itself.
    TArray<TSharedRef<SWidget>> Actions;
    Actions.Add(Btn(bWon?LOCTEXT("Again","再战一次"):LOCTEXT("TryAgain","再次挑战"),EButtonSkin::Gold,TEXT("WanJian.Retry"),Retry,true,60.f,150.f,22.f));
    Actions.Add(Btn(LOCTEXT("Adjust","调整剑阵"),EButtonSkin::Jade,TEXT("WanJian.Adjust"),Guarded([this]{Tab=2;CloseBattle();}),true,60.f,150.f,22.f));
    if(!bWon&&Dungeon.IsNone())
    {
        Actions.Add(Btn(LOCTEXT("Invite","邀请好友"),EButtonSkin::Paper,TEXT("WanJian.Invite"),Guarded([this,Stage]
        {
            const FString Token=Account()->GetPlatform().ShareStage(Stage);
            Report(true,FText::Format(LOCTEXT("InviteDone","本地验证：已生成求援房间 {0}，分享本身不发奖励"),FText::FromString(Token)));
        }),true,60.f,150.f,22.f));
    }
    Actions.Add(Btn(LOCTEXT("Leave","返回"),EButtonSkin::Paper,TEXT("WanJian.Leave"),Guarded([this]{CloseBattle();}),true,60.f,150.f,22.f));

    return Paper(Rows({
        Head(bWon?LOCTEXT("Victory","通关"):LOCTEXT("Defeat","未能通关"),32.f),
        Line(OutcomeText),
        Hint(FText::Format(LOCTEXT("BattleStats","用时 {0} 秒 · 击杀 普通 {1} / 精英 {2} / 首领 {3} · 临界值 {4}"),N(FMath::RoundToInt(Outcome.Seconds)),N(Outcome.NormalKills),N(Outcome.EliteKills),N(Outcome.BossKills),FText::AsNumber(FMath::RoundToInt(double(Outcome.CriticalUnits)/CriticalUnitsPerPoint)))),
        Grid(Actions,2,8.f)
    }));
}

TSharedRef<SWidget> SInkWanJianScreen::BuildBattleView()
{
    // The arena shrinks at the end so the result and its actions fit on a portrait screen without scrolling.
    const TSharedRef<SWidget> Arena=SNew(SBox).HeightOverride(bSettled?300.f:640.f)
    [
        SNew(SInkPaint).OnPaint_Lambda([this](FPainter& P,const FGeometry&){PaintArena(P);})
    ];
    if(bSettled)return Rows({Arena,BuildSettlement()});

    return Rows({
        Arena,
        Meter(TAttribute<float>::CreateLambda([this]{const WanJianBattle::FSimulator* B=Sim.Get();return B&&Account()?float(B->GetHealth()/FMath::Max(1.0,Account()->MakeBattle(BattleStage,BattleTeam).MaxHealth)):0.f;}),
            TAttribute<FText>::CreateLambda([this]{const WanJianBattle::FSimulator* B=Sim.Get();return FText::AsNumber(B?FMath::CeilToInt(B->GetHealth()):0);}),EBar::Health),
        Meter(TAttribute<float>::CreateLambda([this]{const WanJianBattle::FSimulator* B=Sim.Get();return B?float(B->GetCriticalPoints()/100.0):0.f;}),
            TAttribute<FText>::CreateLambda([this]{const WanJianBattle::FSimulator* B=Sim.Get();return FText::Format(LOCTEXT("CriticalMeter","临界值 {0}"),FText::AsNumber(B?FMath::FloorToInt(B->GetCriticalPoints()*10)*.1:0.0));}),EBar::Jade),
        SNew(SHorizontalBox)
        +SHorizontalBox::Slot().FillWidth(1.f).Padding(FMargin(0.f,0.f,5.f,0.f))
        [
            Btn(TAttribute<FText>::CreateLambda([this]{return FText::Format(LOCTEXT("Speed","倍速 ×{0}"),N(BattleSpeed));}),EButtonSkin::Paper,TEXT("WanJian.Speed"),Guarded([this]{BattleSpeed=BattleSpeed>=4?1:BattleSpeed*2;}),true,56.f,150.f,22.f)
        ]
        +SHorizontalBox::Slot().FillWidth(1.f).Padding(FMargin(5.f,0.f,0.f,0.f))
        [
            Btn(LOCTEXT("Retreat","撤退"),EButtonSkin::Cinnabar,TEXT("WanJian.Retreat"),Guarded([this]{CloseBattle();}),true,56.f,150.f,22.f)
        ]
    });
}

#undef LOCTEXT_NAMESPACE

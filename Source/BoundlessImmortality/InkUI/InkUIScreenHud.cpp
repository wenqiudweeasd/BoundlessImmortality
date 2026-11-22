#include "InkUI/InkUIScreens.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Layout/SBox.h"
#include "XianTu/XianTuSubsystem.h"

#define LOCTEXT_NAMESPACE "InkUIHud"

using namespace InkUI;

namespace
{
    FSimpleDelegate RunCommand(const TWeakPtr<SInkUIRoot>& Root,FName Command)
    {
        return FSimpleDelegate::CreateLambda([Root,Command]{if(const auto R=Root.Pin())R->RunCommand(Command);});
    }

    FSimpleDelegate OpenRoute(const TWeakPtr<SInkUIRoot>& Root,FName Route)
    {
        return FSimpleDelegate::CreateLambda([Root,Route]{if(const auto R=Root.Pin())R->OpenPage(Route);});
    }

    void PaintChip(FPainter& P,const FString& Text,FLinearColor TextColor)
    {
        P.Skin(Art::ESkin::PlaqueLacquer,FVector2f(0.f,0.f),P.Size(),FLinearColor(1,1,1,.92f));
        P.Next();
        P.Text(Text,Font(EFont::Tiny,16.f),P.Size()*.5f,TextColor,FVector2f(.5f,.52f));
    }
}

void SInkMainMenuScreen::Construct(const FArguments& InArgs)
{
    InitScreen(InArgs._Root.ToSharedRef(),TEXT("MainMenu"));
    const TWeakPtr<SInkUIRoot> WeakRoot=InArgs._Root;
    const bool bHasSave=M().Saves.ContainsByPredicate([](const FSaveSlot& S){return S.bUsed&&S.bValid;});
    const FSimpleDelegate Quit=FSimpleDelegate::CreateLambda([WeakRoot]
    {
        if(const auto R=WeakRoot.Pin())R->Confirm(LOCTEXT("QuitTitle","暂别仙途"),LOCTEXT("QuitBody","确定退出游戏？尚未保存的行程将不会保留。"),RunCommand(WeakRoot,TEXT("QuitGame")),true,LOCTEXT("QuitConfirm","退出"));
    });
    ChildSlot
    [
        SNew(SOverlay)
        +SOverlay::Slot()
        [
            SNew(SInkPaint).OnPaint_Lambda([](FPainter& P,const FGeometry&)
            {
                const auto& K=Colors();const FVector2f S=P.Size();
                P.VGradient(FVector2f(0.f,0.f),FVector2f(S.X,S.Y*.24f),{{0.f,WithAlpha(K.InkBlack,.5f)},{1.f,WithAlpha(K.InkBlack,0.f)}});
                P.VGradient(FVector2f(0.f,S.Y*.5f),FVector2f(S.X,S.Y*.5f),{{0.f,WithAlpha(K.InkBlack,0.f)},{.55f,WithAlpha(K.InkBlack,.55f)},{1.f,WithAlpha(K.InkBlack,.86f)}});
                P.Next();
                P.Motes(FVector2f(0.f,S.Y*.45f),FVector2f(S.X,S.Y*.55f),16,Motion::Now(),WithAlpha(K.GoldBright,.75f),21,1.2f,5.f);
            })
        ]
        +SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Top).Padding(FMargin(0.f,28.f,0.f,0.f))
        [
            SNew(SInkAnimBox).Enter(EEnter::Zoom).Duration(1.15f).Curve(EEase::OutCubic)
            [
                SNew(SBox).WidthOverride(660.f).HeightOverride(372.f)
                [
                    SNew(SInkPaint).OnPaint_Lambda([](FPainter& P,const FGeometry&)
                    {
                        const auto& K=Colors();const FVector2f S=P.Size();const double Now=Motion::Now();
                        P.Glow(S*FVector2f(.5f,.5f),FVector2f(S.X*.56f,S.Y*.5f),WithAlpha(FLinearColor::White,.16f+.06f*Motion::Pulse(Now,4.f)));
                        P.Next();
                        // The painting is cut flat on its left and bottom; dissolve those edges into the scene.
                        P.BrushFitFeathered(Art::Image(Art::Paths::Title),FVector2f(0.f,0.f),S,FVector4f(.16f,0.f,.02f,.3f));
                        P.Next();
                        P.Motes(FVector2f(S.X*.1f,S.Y*.15f),FVector2f(S.X*.8f,S.Y*.8f),10,Now,WithAlpha(K.GoldBright,.7f),5,.9f,4.f);
                    })
                ]
            ]
        ]
        +SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Bottom).Padding(FMargin(0.f,0.f,0.f,118.f))
        [
            SNew(SVerticalBox)
            +SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(FMargin(0.f,7.f))
            [
                SNew(SInkAnimBox).Delay(.45f).Distance(36.f)
                [
                    SNew(SInkButton).Skin(EButtonSkin::Gold).Text(LOCTEXT("Start","开始游戏")).Height(98.f).MinWidth(470.f).FontSize(36.f)
                    .Highlight(true).TestId(TEXT("Main.Start")).OnClicked(RunCommand(WeakRoot,TEXT("StartGame")))
                ]
            ]
            +SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(FMargin(0.f,7.f))
            [
                SNew(SInkAnimBox).Delay(.53f).Distance(36.f)
                [
                    SNew(SInkButton).Skin(EButtonSkin::Paper).Text(LOCTEXT("Continue","继续游戏")).Height(84.f).MinWidth(470.f).FontSize(31.f)
                    .Enabled(bHasSave).TestId(TEXT("Main.Continue")).OnClicked(RunCommand(WeakRoot,TEXT("ContinueGame")))
                ]
            ]
            +SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(FMargin(0.f,7.f))
            [
                SNew(SInkAnimBox).Delay(.61f).Distance(36.f)
                [
                    SNew(SHorizontalBox)
                    +SHorizontalBox::Slot().AutoWidth().Padding(FMargin(0.f,0.f,13.f,0.f))
                    [
                        SNew(SInkButton).Skin(EButtonSkin::Dark).Text(LOCTEXT("Saves","存档")).Height(76.f).MinWidth(228.f).TestId(TEXT("Main.Saves")).OnClicked(OpenRoute(WeakRoot,TEXT("Saves")))
                    ]
                    +SHorizontalBox::Slot().AutoWidth().Padding(FMargin(13.f,0.f,0.f,0.f))
                    [
                        SNew(SInkButton).Skin(EButtonSkin::Dark).Text(LOCTEXT("Settings","设置")).Height(76.f).MinWidth(228.f).TestId(TEXT("Main.Settings")).OnClicked(OpenRoute(WeakRoot,TEXT("Settings")))
                    ]
                ]
            ]
            +SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(FMargin(0.f,12.f,0.f,0.f))
            [
                SNew(SInkAnimBox).Enter(EEnter::Fade).Delay(.72f)
                [
                    SNew(SInkButton).Skin(EButtonSkin::Ghost).Text(LOCTEXT("Exit","退出游戏")).Height(58.f).MinWidth(220.f).FontSize(25.f).TestId(TEXT("Main.Exit")).OnClicked(Quit)
                ]
            ]
        ]
        // Fill, not Center: auto-wrapped text in a centred slot wraps against its own previous width.
        +SOverlay::Slot().HAlign(HAlign_Fill).VAlign(VAlign_Bottom).Padding(FMargin(20.f,0.f,20.f,18.f))
        [
            SNew(SInkAnimBox).Enter(EEnter::Fade).Delay(.95f)
            [
                MakeText(LOCTEXT("Advice","抵制不良游戏，拒绝盗版游戏。注意自我保护，谨防受骗上当。\n适度游戏益脑，沉迷游戏伤身。合理安排时间，享受健康生活。"),EFont::Tiny,WithAlpha(Colors().TextLightDim,.8f),17.f,1.f,true,ETextJustify::Center)
            ]
        ]
        +SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Top).Padding(FMargin(0.f,12.f,12.f,0.f))
        [
            SNew(SInkAnimBox).Enter(EEnter::Fade).Delay(1.1f)
            [
                SNew(SInkPaint).DesiredSize(FVector2D(256.f,42.f)).OnPaint_Lambda([](FPainter& P,const FGeometry&){PaintChip(P,TEXT("本地验证 · 未接入直播SDK"),Colors().TextGold);})
            ]
        ]
    ];
}

void SInkHudScreen::Construct(const FArguments& InArgs)
{
    InitScreen(InArgs._Root.ToSharedRef(),TEXT("Hud"));
    const TWeakPtr<SInkUIRoot> WeakRoot=InArgs._Root;
    auto Entry=[&](const TCHAR* Icon,const FText& Label,FName Target,FName Id,TFunction<bool()> Badge,float Delay)
    {
        return SNew(SInkAnimBox).Enter(EEnter::Down).Delay(Delay).Distance(30.f)
        [
            SNew(SInkIconButton).Icon(Art::Image(Icon)).Label(Label).Tile(true).Size(FVector2D(94.f,116.f)).IconSize(66.f).LabelSize(23.f)
            .Badge_Lambda([Badge]{return Badge?Badge():false;}).TestId(Id).OnClicked(OpenRoute(WeakRoot,Target))
        ];
    };
    // Second row: atlas art instead of a whole texture, smaller labels for the four-character names.
    auto EntryArt=[&](const FSlateBrush* Icon,const FText& Label,FName Target,FName Id,TFunction<bool()> Badge,float Delay)
    {
        return SNew(SInkAnimBox).Enter(EEnter::Down).Delay(Delay).Distance(30.f)
        [
            SNew(SInkIconButton).Icon(Icon).Label(Label).Tile(true).Size(FVector2D(94.f,116.f)).IconSize(62.f).LabelSize(20.f)
            .Badge_Lambda([Badge]{return Badge?Badge():false;}).TestId(Id).OnClicked(OpenRoute(WeakRoot,Target))
        ];
    };
    auto Nav=[&](const TCHAR* Icon,const FText& Label,FName Id,FSimpleDelegate Click)
    {
        return SNew(SInkIconButton).Icon(Art::Image(Icon)).Label(Label).Size(FVector2D(120.f,136.f)).IconSize(84.f).LabelSize(25.f).TestId(Id).OnClicked(Click);
    };
    auto GuideRow=[&](FName Id,FSimpleDelegate Click,TFunction<FText()> Text,TFunction<bool()> bHot)
    {
        return SNew(SInkClickable).TestId(Id).OnClicked(Click).Radius(6.f)
        [
            SNew(SHorizontalBox)
            +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(2.f,6.f,8.f,6.f))
            [
                SNew(SInkPaint).DesiredSize(FVector2D(16.f,16.f)).OnPaint_Lambda([bHot](FPainter& P,const FGeometry&)
                {
                    const bool bOn=bHot&&bHot();
                    P.Diamond(P.Size()*.5f,FVector2f(6.f),bOn?Colors().GoldBright:WithAlpha(Colors().TextLightDim,.8f));
                    if(bOn)P.Glow(P.Size()*.5f,FVector2f(12.f),WithAlpha(Colors().GoldBright,.4f+.3f*Motion::Pulse(Motion::Now(),1.4f)));
                })
            ]
            +SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
            [
                MakeText(TAttribute<FText>::CreateLambda([Text]{return Text();}),EFont::Small,Colors().TextLight,22.f,1.f)
            ]
        ];
    };
    ChildSlot
    [
        SNew(SOverlay)
        +SOverlay::Slot()[SNew(SInkPaint).OnPaint(this,&SInkHudScreen::PaintBackdrop)]
        +SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(FMargin(4.f,4.f,0.f,0.f))
        [
            SNew(SInkAnimBox).Enter(EEnter::Left).Duration(.55f).Distance(46.f)
            [
                SNew(SBox).WidthOverride(444.f).HeightOverride(190.f)
                [
                    SNew(SOverlay)
                    +SOverlay::Slot()[SNew(SInkPaint).OnPaint(this,&SInkHudScreen::PaintIdentity)]
                    +SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(FMargin(136.f,56.f,0.f,0.f))
                    [
                        SNew(SBox).WidthOverride(298.f).HeightOverride(40.f)
                        [
                            SNew(SInkProgressBar).Style(EBar::Health).Height(40.f).Width(298.f).Label(LOCTEXT("Health","血量"))
                            .Fraction_Lambda([this]{return M().HealthFraction();})
                            .Value_Lambda([this]{return FText::Format(LOCTEXT("Pair","{0}/{1}"),FormatAmount(M().Player.Health),FormatAmount(M().Player.MaxHealth));})
                        ]
                    ]
                    +SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(FMargin(136.f,98.f,0.f,0.f))
                    [
                        SNew(SBox).WidthOverride(298.f).HeightOverride(40.f)
                        [
                            SNew(SInkProgressBar).Style(EBar::Qi).Height(40.f).Width(298.f).Label(LOCTEXT("Qi","修为"))
                            .Fraction_Lambda([this]{return M().QiFraction();})
                            .Value_Lambda([this]
                            {
                                const FPlayerState& Pl=M().Player;
                                if(Pl.MaxQi<=0)return LOCTEXT("Peak","已达巅峰");
                                // Qi keeps accruing past the realm cap while a breakthrough waits; name that state instead of showing an overflow.
                                if(Pl.Qi>=Pl.MaxQi)return LOCTEXT("QiFull","圆满 · 可突破");
                                return FText::Format(LOCTEXT("Pair","{0}/{1}"),FormatAmount(Pl.Qi),FormatAmount(Pl.MaxQi));
                            })
                        ]
                    ]
                    +SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(FMargin(142.f,142.f,0.f,0.f))
                    [
                        SNew(SBox).WidthOverride(300.f).HeightOverride(44.f).Visibility(EVisibility::HitTestInvisible)
                        [
                            SNew(SInkPaint).OnPaint_Lambda([this](FPainter& P,const FGeometry&){PaintBuffs(P);})
                        ]
                    ]
                    +SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top)
                    [
                        SNew(SInkClickable).TestId(TEXT("Hud.Identity")).Radius(64.f).OnClicked(OpenRoute(WeakRoot,TEXT("Character")))
                        [
                            SNew(SBox).WidthOverride(140.f).HeightOverride(140.f)
                        ]
                    ]
                ]
            ]
        ]
        +SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Top).Padding(FMargin(0.f,8.f,6.f,0.f))
        [
            SNew(SHorizontalBox)
            +SHorizontalBox::Slot().AutoWidth().Padding(FMargin(3.f,0.f))[Entry(Art::Paths::IconActivity,LOCTEXT("Activities","活动"),TEXT("Activities"),TEXT("Hud.Activities"),[this]{return M().ClaimableActivities()>0;},.1f)]
            +SHorizontalBox::Slot().AutoWidth().Padding(FMargin(3.f,0.f))[Entry(Art::Paths::IconSignIn,LOCTEXT("SignIn","签到"),TEXT("SignIn"),TEXT("Hud.SignIn"),[this]{return M().HasClaimableSignIn();},.17f)]
            +SHorizontalBox::Slot().AutoWidth().Padding(FMargin(3.f,0.f))[Entry(Art::Paths::IconJourney,LOCTEXT("Journey","仙途"),TEXT("Journey"),TEXT("Hud.Journey"),nullptr,.24f)]
        ]
        +SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(FMargin(10.f,204.f,0.f,0.f))
        [
            SNew(SInkAnimBox).Enter(EEnter::Left).Delay(.22f).Distance(36.f)
            [
                SNew(SBox).WidthOverride(318.f)
                [
                    SNew(SOverlay)
                    +SOverlay::Slot()
                    [
                        SNew(SInkPaint).OnPaint_Lambda([](FPainter& P,const FGeometry&)
                        {
                            const auto& K=Colors();
                            P.HGradient(FVector2f(0.f,0.f),P.Size(),{{0.f,WithAlpha(K.InkBlack,.72f)},{.75f,WithAlpha(K.InkBlack,.45f)},{1.f,WithAlpha(K.InkBlack,0.f)}},8.f);
                            P.Next();
                            P.FadeLine(FVector2f(0.f,1.f),FVector2f(P.Size().X*.85f,1.f),WithAlpha(K.Gold,.8f),1.5f,.05f);
                            P.Rounded(FVector2f(0.f,0.f),FVector2f(4.f,P.Size().Y),WithAlpha(K.Gold,.85f),2.f);
                        })
                    ]
                    +SOverlay::Slot().Padding(FMargin(16.f,10.f,14.f,12.f))
                    [
                        SNew(SVerticalBox)
                        +SVerticalBox::Slot().AutoHeight()[MakeText(LOCTEXT("Guide","仙途指引"),EFont::Section,Colors().TextGold,25.f,1.f)]
                        +SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f,4.f,0.f,0.f))
                        [
                            GuideRow(TEXT("Hud.Guide.SignIn"),OpenRoute(WeakRoot,TEXT("SignIn")),[this]{return M().HasClaimableSignIn()?LOCTEXT("GuideSign","今日签到 · 奖励可领"):LOCTEXT("GuideSigned","今日已签到 · 明日再来");},[this]{return M().HasClaimableSignIn();})
                        ]
                        +SVerticalBox::Slot().AutoHeight()
                        [
                            GuideRow(TEXT("Hud.Guide.Activity"),OpenRoute(WeakRoot,TEXT("Activities")),[this]
                            {
                                const FActivity* A=M().Activities.FindByPredicate([](const FActivity& X){return X.State!=EClaim::Claimed;});
                                return A?FText::Format(LOCTEXT("GuideAct","{0} {1}/{2}"),A->Title,FText::AsNumber(A->Progress),FText::AsNumber(A->Target)):LOCTEXT("GuideDone","今日历练已圆满");
                            },[this]{return M().ClaimableActivities()>0;})
                        ]
                        +SVerticalBox::Slot().AutoHeight()
                        [
                            GuideRow(TEXT("Hud.Guide.Realm"),OpenRoute(WeakRoot,TEXT("Breakthrough")),[this]
                            {
                                return M().QiFraction()>=1.f?LOCTEXT("GuideBreak","修为圆满 · 可尝试突破"):FText::Format(LOCTEXT("GuideQi","修为 {0}% · 入定精进"),FText::AsNumber(FMath::FloorToInt(M().QiFraction()*100.f)));
                            },[this]{return M().QiFraction()>=1.f;})
                        ]
                    ]
                ]
            ]
        ]
        +SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Top).Padding(FMargin(0.f,138.f,8.f,0.f))
        [
            SNew(SInkAnimBox).Enter(EEnter::Right).Delay(.3f).Distance(36.f)
            [
                SNew(SInkClickable).TestId(TEXT("Hud.RankChip")).OnClicked(OpenRoute(WeakRoot,TEXT("Leaderboard"))).Radius(20.f)
                [
                    SNew(SInkPaint).DesiredSize(FVector2D(204.f,58.f)).OnPaint_Lambda([](FPainter& P,const FGeometry&)
                    {
                        P.BrushFit(Art::Image(Art::Paths::RankPaper),FVector2f(0.f,0.f),P.Size());
                        P.Next();
                        P.Text(TEXT("查看排行"),Font(EFont::Button,25.f),FVector2f(P.Size().X*.56f,P.Size().Y*.5f),Colors().TextDark,FVector2f(.5f,.52f));
                    })
                ]
            ]
        ]
        +SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Top).Padding(FMargin(0.f,204.f,6.f,0.f))
        [
            SNew(SHorizontalBox)
            +SHorizontalBox::Slot().AutoWidth().Padding(FMargin(3.f,0.f))
            [EntryArt(Art::ItemIcon(TEXT("golden_scroll")),LOCTEXT("Fortune","每日一签"),TEXT("Fortune"),TEXT("Hud.Fortune"),[this]
            {
                const UXianTuSubsystem* X=M().XianTu.Get();return X&&X->TodayFortune()==XianTuCatalog::EFortune::None;
            },.31f)]
            +SHorizontalBox::Slot().AutoWidth().Padding(FMargin(3.f,0.f))[EntryArt(Art::ItemIcon(TEXT("gold_cloud")),LOCTEXT("XianDao","仙道殿"),TEXT("XianDao"),TEXT("Hud.XianDao"),nullptr,.38f)]
            +SHorizontalBox::Slot().AutoWidth().Padding(FMargin(3.f,0.f))[EntryArt(Art::ItemIcon(TEXT("swords")),LOCTEXT("WanJian","万剑仙宗"),TEXT("WanJian"),TEXT("Hud.WanJian"),nullptr,.45f)]
        ]
        +SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Bottom).Padding(FMargin(0.f,0.f,0.f,214.f))
        [
            SNew(SBox).Visibility_Lambda([this]{return M().Player.bCultivating?EVisibility::HitTestInvisible:EVisibility::Collapsed;})
            [
                SNew(SInkPaint).DesiredSize(FVector2D(440.f,58.f)).OnPaint_Lambda([this](FPainter& P,const FGeometry&)
                {
                    const auto& K=Colors();const float Pulse=Motion::Pulse(Motion::Now(),Durations::BreathPeriod);
                    P.Glow(P.Size()*.5f,FVector2f(P.Size().X*.6f,P.Size().Y*1.2f),WithAlpha(K.GoldBright,.12f+.1f*Pulse));
                    P.Next();
                    P.Skin(Art::ESkin::PlaqueLacquer,FVector2f(0.f,0.f),P.Size(),FLinearColor(1,1,1,.94f));
                    P.Next();
                    const FString Text=FString::Printf(TEXT("入定修炼中 · 修为 +%.2f/秒"),M().Player.QiPerSecond);
                    P.Text(Text,Font(EFont::Body,25.f),P.Size()*.5f,Lerp(K.TextGold,K.GoldBright,Pulse),FVector2f(.5f,.52f));
                })
            ]
        ]
        +SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Bottom).Padding(FMargin(0.f,0.f,0.f,8.f))
        [
            SNew(SInkAnimBox).Enter(EEnter::Up).Delay(.04f).Distance(70.f).Duration(.6f).Curve(EEase::OutBackSoft)
            [
                SNew(SHorizontalBox)
                +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Bottom)[Nav(Art::Paths::AvatarPortrait,LOCTEXT("Role","角色"),TEXT("Hud.Character"),OpenRoute(WeakRoot,TEXT("Character")))]
                +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Bottom)[Nav(Art::Paths::IconBag,LOCTEXT("Bag","背包"),TEXT("Hud.Inventory"),OpenRoute(WeakRoot,TEXT("Inventory")))]
                +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Bottom)
                [
                    SNew(SInkIconButton).Icon(Art::Image(Art::Paths::IconCultivate)).Size(FVector2D(176.f,184.f)).IconSize(128.f).LabelSize(30.f).Glow(true)
                    .Active_Lambda([this]{return M().Player.bCultivating;})
                    .Label_Lambda([this]{return M().Player.bCultivating?LOCTEXT("StopCultivate","收功"):LOCTEXT("Cultivate","修炼");})
                    .TestId(TEXT("Hud.Cultivate")).OnClicked(RunCommand(WeakRoot,TEXT("ToggleCultivation")))
                ]
                +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Bottom)[Nav(Art::Paths::IconRank,LOCTEXT("Rank","排行"),TEXT("Hud.Leaderboard"),OpenRoute(WeakRoot,TEXT("Leaderboard")))]
                +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Bottom)[Nav(Art::Paths::IconMenu,LOCTEXT("Menu","菜单"),TEXT("Hud.Menu"),FSimpleDelegate::CreateLambda([this]{ToggleMenu(!bMenuOpen);}))]
            ]
        ]
        +SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Bottom).Padding(FMargin(0.f,0.f,14.f,176.f))
        [
            SAssignNew(MenuPopover,SInkAnimBox).Enter(EEnter::Pop).AutoPlay(false).Duration(.28f)
            [
                SNew(SBox).WidthOverride(290.f)
                [
                    SNew(SOverlay)
                    +SOverlay::Slot()
                    [
                        SNew(SInkPaint).OnPaint_Lambda([](FPainter& P,const FGeometry&)
                        {
                            P.Halo(FVector2f(6.f,6.f),P.Size()-FVector2f(12.f,12.f),12.f,22.f,WithAlpha(Colors().InkBlack,.55f));
                            P.Next();
                            P.Skin(Art::ESkin::CardPaper,FVector2f(0.f,0.f),P.Size(),FLinearColor::White,.5f);
                        })
                    ]
                    +SOverlay::Slot().Padding(FMargin(24.f,26.f,24.f,26.f))
                    [
                        SNew(SVerticalBox)
                        +SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f,5.f))
                        [
                            SNew(SInkButton).Skin(EButtonSkin::Paper).Text(LOCTEXT("MenuSaves","存档")).Height(68.f).MinWidth(242.f).TestId(TEXT("Hud.Menu.Saves"))
                            .OnClicked_Lambda([this,WeakRoot]{ToggleMenu(false);if(const auto R=WeakRoot.Pin())R->OpenPage(TEXT("Saves"));})
                        ]
                        +SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f,5.f))
                        [
                            SNew(SInkButton).Skin(EButtonSkin::Paper).Text(LOCTEXT("MenuSettings","设置")).Height(68.f).MinWidth(242.f).TestId(TEXT("Hud.Menu.Settings"))
                            .OnClicked_Lambda([this,WeakRoot]{ToggleMenu(false);if(const auto R=WeakRoot.Pin())R->OpenPage(TEXT("Settings"));})
                        ]
                        +SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f,5.f))
                        [
                            SNew(SInkButton).Skin(EButtonSkin::Dark).Text(LOCTEXT("MenuMain","返回主菜单")).Height(68.f).MinWidth(242.f).TestId(TEXT("Hud.Menu.Main"))
                            .OnClicked_Lambda([this,WeakRoot]
                            {
                                ToggleMenu(false);
                                if(const auto R=WeakRoot.Pin())R->Confirm(LOCTEXT("BackTitle","返回主菜单"),LOCTEXT("BackBody","将结束本次行程并回到主菜单，未保存的进度不会保留。"),RunCommand(WeakRoot,TEXT("ReturnToMainMenu")));
                            })
                        ]
                    ]
                ]
            ]
        ]
    ];
    MenuPopover->Snap(false);
}

void SInkHudScreen::ToggleMenu(bool bOpen)
{
    if(bOpen==bMenuOpen)return;
    bMenuOpen=bOpen;
    if(bOpen)MenuPopover->PlayIn();else MenuPopover->PlayOut(0.f,.14f);
    if(const auto R=GetRoot())R->SetOverlayBlocking(bOpen);
}

bool SInkHudScreen::HandleBack()
{
    if(bMenuOpen){ToggleMenu(false);return true;}
    return false;
}

FString SInkHudScreen::Describe() const{return bMenuOpen?TEXT("Hud+Menu"):TEXT("Hud");}

void SInkHudScreen::Tick(const FGeometry& AllottedGeometry,const double InCurrentTime,const float InDeltaTime)
{
    SInkScreen::Tick(AllottedGeometry,InCurrentTime,InDeltaTime);
    for(auto& F:Floaters)F.Age+=InDeltaTime;
    Floaters.RemoveAll([](const FFloater& F){return F.Age>1.7f;});
    if(!M().Player.bCultivating||Motion::IsReduced()){QiGainAccum=0.f;return;}
    QiGainAccum+=InDeltaTime;
    if(QiGainAccum>=1.f)
    {
        QiGainAccum-=1.f;
        FFloater F;F.X=FMath::FRandRange(-90.f,90.f);F.Text=FString::Printf(TEXT("+%.1f 修为"),M().Player.QiPerSecond);
        Floaters.Add(F);
    }
}

void SInkHudScreen::PaintBackdrop(FPainter& P,const FGeometry& G) const
{
    const auto& K=Colors();
    const FVector2f S=P.Size();
    const double Now=Motion::Now();
    P.VGradient(FVector2f(0.f,0.f),FVector2f(S.X,250.f),{{0.f,WithAlpha(K.InkBlack,.55f)},{1.f,WithAlpha(K.InkBlack,0.f)}});
    P.VGradient(FVector2f(0.f,S.Y-340.f),FVector2f(S.X,340.f),{{0.f,WithAlpha(K.InkBlack,0.f)},{.42f,WithAlpha(K.InkBlack,.5f)},{1.f,WithAlpha(K.InkBlack,.92f)}});
    P.Next();
    const float CloudH=FMath::Min(64.f,S.X/14.4f);
    P.Brush(Art::Image(Art::Paths::BottomCloud),FVector2f(0.f,S.Y-CloudH-6.f),FVector2f(S.X,CloudH),FLinearColor(1,1,1,.55f));
    P.FadeLine(FVector2f(S.X*.06f,S.Y-196.f),FVector2f(S.X*.94f,S.Y-196.f),WithAlpha(K.Gold,.5f),1.5f,.25f);
    P.Diamond(FVector2f(S.X*.5f,S.Y-196.f),FVector2f(6.f,4.f),WithAlpha(K.GoldBright,.85f));
    if(M().Player.bCultivating)
    {
        P.Next();
        const float Pulse=Motion::Pulse(Now,Durations::BreathPeriod);
        P.Glow(FVector2f(S.X*.5f,S.Y-96.f),FVector2f(S.X*.62f,280.f),WithAlpha(K.GoldBright,.16f+.08f*Pulse));
        P.Motes(FVector2f(S.X*.2f,S.Y-520.f),FVector2f(S.X*.6f,420.f),18,Now,WithAlpha(K.GoldBright,.85f),31,2.f,5.f);
        P.Next();
        for(const auto& F:Floaters)
        {
            const float A=FMath::Clamp(F.Age/.25f,0.f,1.f)*FMath::Clamp((1.7f-F.Age)/.5f,0.f,1.f);
            P.Text(F.Text,Font(EFont::Data,26.f,1.5f,K.InkBlack),FVector2f(S.X*.5f+F.X,S.Y-250.f-F.Age*70.f),WithAlpha(K.GoldBright,A),FVector2f(.5f,.5f));
        }
    }
}

void SInkHudScreen::PaintBuffs(FPainter& P) const
{
    const auto& K=Colors();
    const float D=36.f;float X=2.f;
    for(const FBuffView& B:M().Buffs)
    {
        if(X+D>P.Size().X)break;
        const FVector2f C(X+D*.5f,D*.5f+3.f);
        const FLinearColor Edge=B.bDebuff?K.CinnabarBright:B.Color;
        P.Glow(C,FVector2f(D*.66f),WithAlpha(Edge,.32f));
        P.Next();
        P.Rounded(C-FVector2f(D*.5f),FVector2f(D),WithAlpha(K.InkBlack,.84f),D*.5f,WithAlpha(Edge,.95f),2.f);
        P.Next();
        if(B.Duration>0&&B.Remaining>0)P.Ring(C,D*.5f-1.5f,3.f,WithAlpha(Edge,.95f),WithAlpha(Edge,.95f),-PI*.5f,2*PI*FMath::Clamp(B.Remaining/B.Duration,0.f,1.f),40);
        P.Next();
        P.Text(B.Glyph,Font(EFont::Title,21.f,1.f,K.InkBlack),C+FVector2f(0.f,-1.f),Lerp(Edge,FLinearColor::White,.35f),FVector2f(.5f,.55f));
        if(B.Stacks>1)P.Text(FString::FromInt(B.Stacks),Font(EFont::Data,15.f,1.5f,K.InkBlack),C+FVector2f(D*.36f,D*.34f),K.TextLight,FVector2f(.5f,.5f));
        P.Next();
        X+=D+5.f;
    }
}

void SInkHudScreen::PaintIdentity(FPainter& P,const FGeometry& G) const
{
    const auto& K=Colors();
    const FPlayerState& Pl=M().Player;
    const double Now=Motion::Now();
    P.BrushFit(Art::Image(Art::Paths::IdentityInk),FVector2f(66.f,6.f),FVector2f(378.f,128.f),FLinearColor(1,1,1,.96f));
    P.Next();
    if(Pl.bCultivating)P.Glow(FVector2f(70.f,70.f),FVector2f(84.f),WithAlpha(K.GoldBright,.32f+.2f*Motion::Pulse(Now,Durations::BreathPeriod)));
    P.Next();
    SInkAvatar::PaintAvatar(P,FVector2f(70.f,70.f),130.f,Art::Image(Art::Paths::AvatarPortrait),Pl.Name,0);
    P.Next();P.Next();
    if(M().ShieldMax>0&&M().Shield>0)
    {
        // Seven-star radiance shield: rotating rays and a ring that empties as the shield absorbs damage.
        const float F=float(FMath::Clamp(M().Shield/M().ShieldMax,0.0,1.0));
        const FLinearColor Gold=Hex(TEXT("FFE9A0")),White(1.f,1.f,.95f,1.f);
        P.Glow(FVector2f(70.f,70.f),FVector2f(96.f),WithAlpha(Gold,.32f+.14f*Motion::Pulse(Now,1.3f)));
        P.Rays(FVector2f(70.f,70.f),60.f,100.f,14,float(Now*.55),.055f,WithAlpha(Gold,.38f));
        P.Next();
        P.Ring(FVector2f(70.f,70.f),75.f,4.f,WithAlpha(Gold,.35f),WithAlpha(Gold,.35f));
        P.Next();
        P.Ring(FVector2f(70.f,70.f),75.f,5.5f,WithAlpha(White,.95f),WithAlpha(Gold,.95f),-PI*.5f,2*PI*F,72);
        P.Next();
        P.Text(FString::Printf(TEXT("护盾 %s"),*FormatAmount(M().Shield).ToString()),Font(EFont::Tiny,16.f,1.5f,K.InkBlack),FVector2f(70.f,128.f),Gold,FVector2f(.5f,.5f));
        P.Next();
    }
    const FSlateFontInfo NameFont=Font(EFont::Body,30.f,2.f,WithAlpha(K.InkBlack,.95f));
    const FVector2f NameSize=P.Text(Pl.Name.ToString(),NameFont,FVector2f(144.f,30.f),K.TextLight,FVector2f(0.f,.5f));
    if(!Pl.bLive)
    {
        const FVector2f TagPos(144.f+NameSize.X+10.f,18.f);
        P.Rounded(TagPos,FVector2f(52.f,24.f),WithAlpha(K.Jade,.85f),4.f,WithAlpha(K.GoldBright,.6f),1.f);
        P.Next();
        P.Text(TEXT("本地"),Font(EFont::Tiny,16.f),TagPos+FVector2f(26.f,12.f),K.TextLight,FVector2f(.5f,.52f));
    }
    P.Next();
    P.Skin(Art::ESkin::RealmPlaque,FVector2f(4.f,136.f),FVector2f(136.f,42.f));
    P.Next();
    P.Text(Pl.Realm.ToString(),Font(EFont::Small,22.f),FVector2f(72.f,157.f),K.TextGold,FVector2f(.5f,.52f));
}

#undef LOCTEXT_NAMESPACE

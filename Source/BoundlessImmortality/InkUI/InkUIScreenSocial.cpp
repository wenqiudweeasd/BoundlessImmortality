#include "InkUI/InkUIScreens.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"

#define LOCTEXT_NAMESPACE "InkUISocial"

using namespace InkUI;

namespace
{
    /** Cinnabar seal stamped onto finished entries. */
    TSharedRef<SWidget> Stamp(const FText& Text,float Size,bool bAnimate)
    {
        const TSharedRef<SWidget> Seal=SNew(SBox).WidthOverride(Size).HeightOverride(Size)
        [
            SNew(SInkPaint).OnPaint_Lambda([Text](FPainter& P,const FGeometry&)
            {
                const auto& K=Colors();const FVector2f S=P.Size();const FVector2f C=S*.5f;
                // Upright seal on a paper disc: the glyphs vanished under a rotated transform, and cinnabar alone is lost on dark item art.
                const float R=S.X*.44f;
                P.Rounded(C-FVector2f(R),FVector2f(R*2.f),WithAlpha(K.PaperLight,.72f),R);
                P.Next();
                P.Ring(C,R,S.X*.06f,WithAlpha(K.Cinnabar,.92f),WithAlpha(K.Cinnabar,.92f),0.f,2.f*PI,40);
                P.Ring(C,S.X*.36f,1.5f,WithAlpha(K.Cinnabar,.8f),WithAlpha(K.Cinnabar,.8f),0.f,2.f*PI,40);
                P.Next();
                P.Text(Text.ToString(),Font(EFont::Title,S.X*.3f),C,WithAlpha(K.Cinnabar,.95f),FVector2f(.5f,.55f));
            })
        ];
        if(!bAnimate)return Seal;
        return SNew(SInkAnimBox).Enter(EEnter::Zoom).Duration(.32f).Delay(.12f).Curve(EEase::OutBack)[Seal];
    }

    TSharedRef<SWidget> Divider(float Width)
    {
        return SNew(SInkPaint).DesiredSize(FVector2D(Width,16.f)).OnPaint_Lambda([](FPainter& P,const FGeometry&)
        {
            P.Divider(P.Size()*.5f,P.Size().X*.92f,WithAlpha(Colors().GoldDeep,.85f));
        });
    }

    void PaintRankNumber(FPainter& P,int32 Rank,FVector2f Center)
    {
        const auto& K=Colors();
        P.Diamond(Center,FVector2f(24.f,24.f),WithAlpha(K.PaperShade,.7f));
        P.Next();
        P.Text(FString::FromInt(Rank),Font(EFont::Data,28.f),Center,K.TextDark,FVector2f(.5f,.52f));
    }
}

void SInkRankScreen::Construct(const FArguments& InArgs)
{
    InitScreen(InArgs._Root.ToSharedRef(),TEXT("Leaderboard"));
    BuildPage(LOCTEXT("RankTitle","排行"),LOCTEXT("RankSeal","榜"),
        SNew(SInkTabBar).Tabs(TArray<FText>{LOCTEXT("Sword","剑灵榜"),LOCTEXT("Speed","修炼榜"),LOCTEXT("Damage","伤害榜")}).TestPrefix(TEXT("Rank.Tabs"))
            .OnSelected_Lambda([this](int32 Index){Board=Index;Rebuild();}),
        SNew(SScrollBox).ScrollBarStyle(&ScrollStyle())+SScrollBox::Slot()[SAssignNew(Content,SBox)],
        SAssignNew(SelfRow,SBox),
        SNullWidget::NullWidget);
    Rebuild();
}

void SInkRankScreen::OnModelChanged()
{
    if(SeenBoards==M().BoardsRev)return;
    Rebuild();
}

void SInkRankScreen::Rebuild()
{
    SeenBoards=M().BoardsRev;
    static const FName Keys[]={TEXT("Anchor"),TEXT("CultivationSpeed"),TEXT("Damage")};
    static const TCHAR* const* Headers[]={&Art::Paths::RankSwordHeader,&Art::Paths::RankSpeedHeader,&Art::Paths::RankDamageHeader};
    const int32 B=FMath::Clamp(Board,0,2);
    const TArray<FRankRow> Rows=M().Boards.FindRef(Keys[B]);
    const TCHAR* Header=*Headers[B];
    const bool bLive=M().bLiveBoards;
    const TSharedRef<SVerticalBox> List=SNew(SVerticalBox);
    List->AddSlot().AutoHeight()
    [
        SNew(SInkAnimBox).Enter(EEnter::Fade).Duration(.4f)
        [
            SNew(SInkPaint).DesiredSize(FVector2D(620.f,190.f)).OnPaint_Lambda([Header,bLive](FPainter& P,const FGeometry&)
            {
                const FVector2f S=P.Size();
                P.Glow(S*FVector2f(.5f,.55f),FVector2f(S.X*.5f,S.Y*.45f),WithAlpha(Colors().GoldBright,.18f));
                P.Next();
                P.BrushFit(Art::Image(Header),FVector2f(0.f,0.f),FVector2f(S.X,S.Y-26.f));
                P.Next();
                P.Text(bLive?TEXT("本局实时数据"):TEXT("本地验证数据 · 非全服榜"),Font(EFont::Tiny,19.f),FVector2f(S.X*.5f,S.Y-12.f),Colors().TextDarkDim,FVector2f(.5f,.5f));
            })
        ]
    ];
    if(!Rows.Num())
    {
        List->AddSlot().AutoHeight().HAlign(HAlign_Center).Padding(FMargin(0.f,60.f))[MakeText(LOCTEXT("NoRows","榜单暂无修士"),EFont::Body,Colors().TextDarkDim,27.f)];
        Content->SetContent(List);SelfRow->SetContent(SNullWidget::NullWidget);
        return;
    }
    List->AddSlot().AutoHeight().Padding(FMargin(0.f,6.f,0.f,10.f))
    [
        SNew(SInkAnimBox).Enter(EEnter::Up).Delay(.08f).Distance(40.f)
        [
            SNew(SInkPaint).DesiredSize(FVector2D(620.f,300.f)).OnPaint_Lambda([Rows](FPainter& P,const FGeometry&)
            {
                const auto& K=Colors();const FVector2f S=P.Size();const double Now=Motion::Now();
                const float Xs[]={.5f,.2f,.8f},Lift[]={0.f,38.f,52.f},Diam[]={136.f,112.f,104.f};
                P.Rays(FVector2f(S.X*.5f,96.f),30.f,190.f,16,float(FMath::Fmod(Now*.2,2.0*PI)),.06f,WithAlpha(K.GoldBright,.2f));
                P.Next();
                for(int32 I=0;I<3&&I<Rows.Num();++I)
                {
                    const FVector2f C(S.X*Xs[I],92.f+Lift[I]);
                    const FVector2f Base(C.X-88.f,C.Y+Diam[I]*.5f+52.f);
                    P.Skin(I==0?Art::ESkin::PlaqueGold:Art::ESkin::PlaquePaperScenic,Base,FVector2f(176.f,S.Y-Base.Y-6.f),FLinearColor::White,.42f);
                    P.Next();
                    SInkAvatar::PaintAvatar(P,C,Diam[I],nullptr,Rows[I].Name,I+1);
                    P.Next();P.Next();
                    P.Rounded(FVector2f(C.X-19.f,C.Y+Diam[I]*.5f-24.f),FVector2f(38.f,38.f),I==0?Hex(TEXT("D9A441")):I==1?Hex(TEXT("A8B3BE")):Hex(TEXT("B5713F")),19.f,WithAlpha(FLinearColor::White,.8f),1.5f);
                    P.Next();
                    P.Text(FString::FromInt(I+1),Font(EFont::Data,24.f),FVector2f(C.X,C.Y+Diam[I]*.5f-5.f),FLinearColor::White,FVector2f(.5f,.52f));
                    P.Text(Rows[I].Name.ToString(),Font(EFont::Body,25.f),FVector2f(C.X,Base.Y+18.f),I==0?K.TextDark:K.TextDark,FVector2f(.5f,.5f));
                    P.Text(Rows[I].Metric.ToString(),Font(EFont::Data,22.f),FVector2f(C.X,Base.Y+46.f),I==0?Hex(TEXT("7A4E12")):K.TextDarkDim,FVector2f(.5f,.5f));
                    P.Next();
                }
            })
        ]
    ];
    if(Rows.Num()<4)
    {
        // A live round often has only the host on the board; say so instead of leaving an empty scroll.
        List->AddSlot().AutoHeight().HAlign(HAlign_Center).Padding(FMargin(0.f,18.f,0.f,0.f))
        [
            SNew(SInkAnimBox).Enter(EEnter::Fade).Delay(.3f)
            [
                SNew(SVerticalBox)
                +SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)[Divider(420.f)]
                +SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(FMargin(0.f,10.f,0.f,0.f))
                [
                    MakeText(FText::Format(LOCTEXT("FewRows","本局暂有 {0} 位修士上榜\n更多仙友参与后将实时刷新"),FText::AsNumber(Rows.Num())),EFont::Body,Colors().TextDarkDim,25.f,0.f,false,ETextJustify::Center)
                ]
            ]
        ];
    }
    for(int32 I=3;I<Rows.Num();++I)
    {
        const FRankRow Row=Rows[I];
        List->AddSlot().AutoHeight().Padding(FMargin(0.f,3.f))
        [
            SNew(SInkAnimBox).Delay(.14f+Motion::Stagger(I-3,.03f,.21f)).Distance(22.f)
            [
                SNew(SInkPaint).DesiredSize(FVector2D(620.f,80.f)).OnPaint_Lambda([Row](FPainter& P,const FGeometry&)
                {
                    const auto& K=Colors();const FVector2f S=P.Size();
                    P.Skin(Art::ESkin::RowScenic,FVector2f(0.f,0.f),S,FLinearColor(1,1,1,Row.bSelf?1.f:.88f));
                    if(Row.bSelf){P.Next();P.Rounded(FVector2f(2.f,2.f),S-FVector2f(4.f,4.f),WithAlpha(K.Jade,.14f),8.f,WithAlpha(K.Jade,.8f),2.f);}
                    P.Next();
                    PaintRankNumber(P,Row.Rank,FVector2f(52.f,S.Y*.5f));
                    P.Next();P.Next();
                    SInkAvatar::PaintAvatar(P,FVector2f(122.f,S.Y*.5f),58.f,nullptr,Row.Name,4);
                    P.Next();P.Next();
                    P.Text(Row.Name.ToString(),Font(EFont::Body,27.f),FVector2f(166.f,S.Y*.36f),K.TextDark,FVector2f(0.f,.5f));
                    P.Text(Row.Realm.ToString(),Font(EFont::Small,21.f),FVector2f(166.f,S.Y*.72f),K.TextJade,FVector2f(0.f,.5f));
                    P.Text(Row.Metric.ToString(),Font(EFont::Data,27.f),FVector2f(S.X-28.f,S.Y*.5f),K.TextDark,FVector2f(1.f,.52f));
                })
            ]
        ];
    }
    Content->SetContent(List);
    const FRankRow* Self=Rows.FindByPredicate([](const FRankRow& R){return R.bSelf;});
    if(!Self){SelfRow->SetContent(SNullWidget::NullWidget);return;}
    const FRankRow Me=*Self;
    SelfRow->SetContent(
        SNew(SInkPaint).DesiredSize(FVector2D(620.f,76.f)).OnPaint_Lambda([Me](FPainter& P,const FGeometry&)
        {
            const auto& K=Colors();const FVector2f S=P.Size();
            P.Skin(Art::ESkin::PlaqueDarkGold,FVector2f(0.f,0.f),S);
            P.Next();
            P.Text(FString::Printf(TEXT("我的排名  第%d名"),Me.Rank),Font(EFont::Button,27.f),FVector2f(48.f,S.Y*.5f),K.GoldBright,FVector2f(0.f,.52f));
            P.Text(Me.Metric.ToString(),Font(EFont::Data,27.f),FVector2f(S.X-48.f,S.Y*.5f),K.TextLight,FVector2f(1.f,.52f));
        }));
}

void SInkSignInScreen::Construct(const FArguments& InArgs)
{
    InitScreen(InArgs._Root.ToSharedRef(),TEXT("SignIn"));
    BuildPage(LOCTEXT("SignTitle","签到"),LOCTEXT("SignSeal","签"),SNullWidget::NullWidget,
        SNew(SScrollBox).ScrollBarStyle(&ScrollStyle())+SScrollBox::Slot()[SAssignNew(Content,SBox)],
        SNew(SBox).HAlign(HAlign_Center)
        [
            SNew(SInkButton).Skin(EButtonSkin::Gold).Height(88.f).MinWidth(420.f).FontSize(32.f).TestId(TEXT("SignIn.Claim"))
            .Text_Lambda([this]{return M().SignIn.ContainsByPredicate([](const FSignInDay& D){return D.State==EClaim::Claimable;})?LOCTEXT("Claim","领取今日奖励"):LOCTEXT("Claimed","今日已领取");})
            .Enabled_Lambda([this]{return M().SignIn.ContainsByPredicate([](const FSignInDay& D){return D.State==EClaim::Claimable;});})
            .Highlight_Lambda([this]{return M().SignIn.ContainsByPredicate([](const FSignInDay& D){return D.State==EClaim::Claimable;});})
            .OnClicked_Lambda([this]{Claim();})
        ],
        SNullWidget::NullWidget);
    Rebuild();
}

void SInkSignInScreen::OnModelChanged()
{
    if(SeenSignIn==M().SignInRev)return;
    Rebuild();
}

void SInkSignInScreen::Claim()
{
    const int32 Day=M().SignIn.IndexOfByPredicate([](const FSignInDay& D){return D.State==EClaim::Claimable;});
    TArray<FReward> Rewards;
    if(!M().ClaimSignIn(Rewards))return;
    if(const auto R=GetRoot())
    {
        const TWeakPtr<SInkSignInScreen> Weak=StaticCastSharedRef<SInkSignInScreen>(AsShared());
        R->ShowRewards(Rewards,LOCTEXT("SignReward","签到奖励"),FSimpleDelegate::CreateLambda([Weak,Day]
        {
            if(const auto S=Weak.Pin()){S->StampDay=Day;S->Rebuild();}
        }));
    }
}

void SInkSignInScreen::OnLayoutChanged()
{
    SInkPageScreen::OnLayoutChanged();
    if(Content&&bEntered&&bBuiltLandscape!=IsLandscape())Rebuild();
}

void SInkSignInScreen::Rebuild()
{
    SeenSignIn=M().SignInRev;
    const int32 Stamped=StampDay;StampDay=INDEX_NONE;
    const bool bEnter=!bEntered;bEntered=true;
    bBuiltLandscape=IsLandscape();
    const TWeakPtr<SInkUIRoot> WeakRoot=GetRoot();
    const int32 Total=M().SignInTotal;
    auto DayCell=[&](const FSignInDay& D,bool bWide)
    {
        const bool bToday=D.State==EClaim::Claimable,bDone=D.State==EClaim::Claimed;
        const FLinearColor Label=bToday?Colors().TextRed:Colors().TextDarkDim;
        return SNew(SInkAnimBox).Enter(EEnter::Pop).AutoPlay(bEnter).Delay(.06f+Motion::Stagger(D.Day-1,.035f,.24f))
        [
            SNew(SBox).HeightOverride(188.f)
            [
                SNew(SOverlay)
                +SOverlay::Slot()
                [
                    SNew(SInkPaint).OnPaint_Lambda([bToday,bDone,bWide](FPainter& P,const FGeometry&)
                    {
                        const auto& K=Colors();const FVector2f S=P.Size();
                        if(bToday)
                        {
                            const float Pulse=Motion::Pulse(Motion::Now(),1.8f);
                            P.Halo(FVector2f(4.f,4.f),S-FVector2f(8.f,8.f),12.f,18.f,WithAlpha(K.GoldBright,.45f+.35f*Pulse));
                            P.Next();
                        }
                        P.Skin(bWide?Art::ESkin::PanelFramed:Art::ESkin::CardPaper,FVector2f(0.f,0.f),S,bDone?FLinearColor(.86f,.84f,.8f,1.f):FLinearColor::White,.34f);
                        if(bWide){P.Next();P.Glow(S*FVector2f(.5f,.55f),FVector2f(S.X*.42f,S.Y*.42f),WithAlpha(K.GoldBright,.3f));}
                        if(bToday){P.Next();P.Rounded(FVector2f(3.f,3.f),S-FVector2f(6.f,6.f),FLinearColor::Transparent,10.f,K.Gold,2.f);}
                    })
                ]
                +SOverlay::Slot().Padding(FMargin(6.f,14.f,6.f,10.f))
                [
                    SNew(SVerticalBox)
                    +SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
                    [
                        MakeText(FText::Format(LOCTEXT("DayN","第{0}天"),FText::AsNumber(D.Day)),EFont::Small,Label,23.f)
                    ]
                    +SVerticalBox::Slot().FillHeight(1.f).HAlign(HAlign_Center).VAlign(VAlign_Center)
                    [
                        SNew(SInkItemSlot).SlotSize(bWide?104.f:88.f).Icon(Art::ItemIcon(D.Reward.Icon)).Quality(D.Reward.Quality).Count(D.Reward.Count)
                    ]
                    +SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
                    [
                        MakeText(bToday?LOCTEXT("Ready","可领取"):D.Reward.Name,EFont::Tiny,bToday?Colors().TextRed:Colors().TextDark,20.f)
                    ]
                ]
                +SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
                [
                    bDone?Stamp(LOCTEXT("Got","已领"),bWide?110.f:96.f,Stamped==D.Day-1):SNullWidget::NullWidget
                ]
            ]
        ];
    };
    const auto& Days=M().SignIn;
    const TSharedRef<SHorizontalBox> RowA=SNew(SHorizontalBox),RowB=SNew(SHorizontalBox);
    // Portrait stacks 4 + 3 with a double-width final day; landscape has the width for the whole week in one row.
    for(int32 I=0;I<Days.Num()&&I<7;++I)
    {
        const bool bWide=I==6;
        (bBuiltLandscape||I<4?RowA:RowB)->AddSlot().FillWidth(bWide?(bBuiltLandscape?1.6f:2.f):1.f).Padding(FMargin(5.f))[DayCell(Days[I],bWide)];
    }
    const TSharedRef<SHorizontalBox> Track=SNew(SHorizontalBox);
    for(int32 I=0;I<M().Milestones.Num();++I)
    {
        const FMilestone Ms=M().Milestones[I];
        Track->AddSlot().FillWidth(1.f).HAlign(HAlign_Center)
        [
            SNew(SVerticalBox)
            +SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
            [
                SNew(SInkClickable).TestId(FName(*FString::Printf(TEXT("SignIn.Chest%d"),I))).Enabled(Ms.State==EClaim::Claimable).Radius(40.f)
                .OnClicked_Lambda([this,I,WeakRoot]
                {
                    TArray<FReward> Rewards;
                    if(M().ClaimMilestone(I,Rewards))if(const auto R=WeakRoot.Pin())R->ShowRewards(Rewards,LOCTEXT("ChestReward","累计签到奖励"));
                })
                [
                    SNew(SInkPaint).DesiredSize(FVector2D(100.f,84.f)).OnPaint_Lambda([Ms](FPainter& P,const FGeometry&)
                    {
                        const auto& K=Colors();const FVector2f S=P.Size();const double Now=Motion::Now();
                        const bool bReady=Ms.State==EClaim::Claimable;
                        const float Bob=bReady&&!Motion::IsReduced()?FMath::Sin(float(Now)*4.f)*4.f:0.f;
                        if(bReady){P.Glow(S*.5f,FVector2f(60.f),WithAlpha(K.GoldBright,.45f+.3f*Motion::Pulse(Now,1.3f)));P.Next();}
                        P.BrushFit(Art::Icon(Art::EIconSet::Chests,Ms.State==EClaim::Claimed?1:0),FVector2f(6.f,4.f+Bob),S-FVector2f(12.f,8.f),Ms.State==EClaim::Locked?FLinearColor(.75f,.75f,.75f,1.f):FLinearColor::White);
                        if(bReady){P.Next();P.Next();P.RedDot(FVector2f(S.X-16.f,14.f),8.f,Motion::Pulse(Now,1.4f));}
                    })
                ]
            ]
            +SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
            [
                MakeText(FText::Format(LOCTEXT("MsDays","{0}天"),FText::AsNumber(Ms.Days)),EFont::Small,Ms.State==EClaim::Claimable?Colors().TextRed:Colors().TextDark,23.f)
            ]
        ];
    }
    TArray<int32> Steps;for(const auto& Ms:M().Milestones)Steps.Add(Ms.Days);
    Content->SetContent(
        SNew(SVerticalBox)
        +SVerticalBox::Slot().AutoHeight()
        [
            SNew(SInkAnimBox).Enter(EEnter::Fade).AutoPlay(bEnter).Duration(.45f)
            [
                // Sized with the day grid and chest track so a portrait week fits between the scroll's painted cap and foot.
                SNew(SInkPaint).DesiredSize(FVector2D(620.f,176.f)).OnPaint_Lambda([Total](FPainter& P,const FGeometry&)
                {
                    const auto& K=Colors();const FVector2f S=P.Size();
                    P.Halo(FVector2f(0.f,0.f),S,8.f,10.f,WithAlpha(K.InkBlack,.3f));
                    P.Next();
                    P.BrushFit(Art::Icon(Art::EIconSet::FeatureScenes,0),FVector2f(0.f,0.f),S);
                    P.Next();
                    P.HGradient(FVector2f(S.X*.38f,0.f),FVector2f(S.X*.62f,S.Y),{{0.f,WithAlpha(K.PaperLight,0.f)},{.35f,WithAlpha(K.PaperLight,.82f)},{1.f,WithAlpha(K.PaperLight,.9f)}});
                    P.GoldFrame(FVector2f(0.f,0.f),S,1.f,WithAlpha(K.GoldDeep,.9f),.7f,true);
                    P.Next();
                    P.Text(TEXT("每日签到"),Font(EFont::Title,54.f),FVector2f(S.X*.73f,S.Y*.38f),K.TextDark,FVector2f(.5f,.5f));
                    P.Divider(FVector2f(S.X*.73f,S.Y*.6f),220.f,WithAlpha(K.GoldDeep,.9f));
                    P.Text(FString::Printf(TEXT("仙途有礼 · 已累计签到 %d 天"),Total),Font(EFont::Small,22.f),FVector2f(S.X*.73f,S.Y*.76f),K.TextDarkDim,FVector2f(.5f,.5f));
                })
            ]
        ]
        +SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f,12.f,0.f,0.f))[RowA]
        +SVerticalBox::Slot().AutoHeight()[RowB]
        +SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(FMargin(0.f,6.f,0.f,2.f))[Divider(520.f)]
        +SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)[MakeText(LOCTEXT("Cumulative","累计签到奖励"),EFont::Section,Colors().TextDark,28.f)]
        +SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f,8.f,0.f,6.f))
        [
            SNew(SOverlay)
            +SOverlay::Slot().VAlign(VAlign_Top).Padding(FMargin(0.f,40.f,0.f,0.f))
            [
                SNew(SInkPaint).DesiredSize(FVector2D(620.f,20.f)).OnPaint_Lambda([Total,Steps](FPainter& P,const FGeometry&)
                {
                    const auto& K=Colors();const FVector2f S=P.Size();
                    // Nodes sit evenly at cell centres; the fill interpolates within the reached segment.
                    const int32 N=FMath::Max(1,Steps.Num());const float Cell=S.X/N,Start=8.f;
                    float Fill=Start;
                    for(int32 I=0;I<Steps.Num();++I)
                    {
                        const float Prev=I?float(Steps[I-1]):0.f,PrevX=I?Cell*(I-.5f):Start,NodeX=Cell*(I+.5f);
                        if(Total>=Steps[I]){Fill=NodeX;continue;}
                        Fill=PrevX+(NodeX-PrevX)*FMath::Clamp((Total-Prev)/FMath::Max(1.f,Steps[I]-Prev),0.f,1.f);
                        break;
                    }
                    P.Rounded(FVector2f(Start,4.f),FVector2f(S.X-Start*2.f,12.f),WithAlpha(K.InkBlack,.35f),6.f,WithAlpha(K.GoldDeep,.6f),1.f);
                    P.Next();
                    if(Fill>Start+1.f)P.HGradient(FVector2f(Start+1.f,5.f),FVector2f(Fill-Start-1.f,10.f),{{0.f,K.GoldDeep},{1.f,K.GoldBright}},5.f);
                })
            ]
            +SOverlay::Slot()[Track]
        ]);
}

void SInkActivityScreen::Construct(const FArguments& InArgs)
{
    InitScreen(InArgs._Root.ToSharedRef(),TEXT("Activities"));
    static const FName Cats[]={TEXT("Daily"),TEXT("Timed"),TEXT("Launch")};
    BuildPage(LOCTEXT("ActTitle","活动"),LOCTEXT("ActSeal","庆"),
        SNew(SInkTabBar).Tabs(TArray<FText>{LOCTEXT("Daily","日常"),LOCTEXT("Timed","限时"),LOCTEXT("Launch","开服")}).TestPrefix(TEXT("Act.Tabs"))
            .Badges_Lambda([this]{TArray<bool> B;for(const FName C:Cats)B.Add(M().ClaimableActivities(C)>0);return B;})
            .OnSelected_Lambda([this](int32 Index){Category=Index;Rebuild();}),
        SNew(SScrollBox).ScrollBarStyle(&ScrollStyle())+SScrollBox::Slot()[SAssignNew(Content,SBox)],
        SNullWidget::NullWidget,SNullWidget::NullWidget);
    Rebuild();
}

void SInkActivityScreen::OnModelChanged()
{
    if(SeenActs==M().ActivitiesRev)return;
    Rebuild();
}

void SInkActivityScreen::Rebuild()
{
    SeenActs=M().ActivitiesRev;
    static const FName Cats[]={TEXT("Daily"),TEXT("Timed"),TEXT("Launch")};
    const FName Cat=Cats[FMath::Clamp(Category,0,2)];
    const TWeakPtr<SInkUIRoot> WeakRoot=GetRoot();
    const TSharedRef<SVerticalBox> List=SNew(SVerticalBox);
    if(const FActivity* Featured=M().Activities.FindByPredicate([&](const FActivity& A){return A.Category==Cat&&A.bFeatured;}))
    {
        const FActivity F=*Featured;
        List->AddSlot().AutoHeight().Padding(FMargin(0.f,0.f,0.f,8.f))
        [
            SNew(SInkAnimBox).Enter(EEnter::Fade).Duration(.42f)
            [
                SNew(SInkPaint).DesiredSize(FVector2D(620.f,226.f)).OnPaint_Lambda([F](FPainter& P,const FGeometry&)
                {
                    const auto& K=Colors();const FVector2f S=P.Size();
                    P.Halo(FVector2f(0.f,0.f),S,8.f,10.f,WithAlpha(K.InkBlack,.35f));
                    P.Next();
                    P.BrushFit(Art::Icon(Art::EIconSet::FeatureScenes,F.Scene),FVector2f(0.f,0.f),S);
                    P.Next();
                    P.VGradient(FVector2f(0.f,S.Y*.45f),FVector2f(S.X,S.Y*.55f),{{0.f,WithAlpha(K.InkBlack,0.f)},{1.f,WithAlpha(K.InkBlack,.82f)}});
                    P.GoldFrame(FVector2f(0.f,0.f),S,1.f,WithAlpha(K.Gold,.95f),.75f,true);
                    P.Next();
                    P.Text(F.Title.ToString(),Font(EFont::Title,42.f,2.f,K.InkBlack),FVector2f(28.f,S.Y-62.f),K.GoldBright,FVector2f(0.f,.5f),WithAlpha(K.InkBlack,.7f));
                    P.Text(F.Description.ToString(),Font(EFont::Small,22.f,1.f,K.InkBlack),FVector2f(30.f,S.Y-24.f),K.TextLight,FVector2f(0.f,.5f));
                    const FVector2f Chip(S.X-200.f,16.f);
                    P.Skin(Art::ESkin::PlaqueCinnabar,Chip,FVector2f(184.f,46.f));
                    P.Next();P.Next();
                    P.Text(F.TimeText.ToString(),Font(EFont::Small,21.f),Chip+FVector2f(92.f,23.f),Hex(TEXT("FFF0D8")),FVector2f(.5f,.52f));
                })
            ]
        ];
    }
    int32 N=0;
    for(const FActivity& A:M().Activities)
    {
        if(A.Category!=Cat)continue;
        const FActivity Act=A;
        const FName Id=A.Id;
        TSharedRef<SWidget> Action=SNullWidget::NullWidget;
        if(A.State==EClaim::Claimable)
        {
            Action=SNew(SInkButton).Skin(EButtonSkin::Gold).Text(LOCTEXT("Get","领取")).Height(64.f).MinWidth(146.f).Highlight(true)
                .TestId(FName(*(TEXT("Act.Claim.")+Id.ToString())))
                .OnClicked_Lambda([this,Id,WeakRoot]
                {
                    TArray<FReward> Rewards;
                    if(M().ClaimActivity(Id,Rewards))if(const auto R=WeakRoot.Pin())R->ShowRewards(Rewards,LOCTEXT("ActReward","活动奖励"));
                });
        }
        else if(A.State==EClaim::Claimed)Action=Stamp(LOCTEXT("Done","已完成"),92.f,false);
        else
        {
            Action=SNew(SInkButton).Skin(EButtonSkin::Paper).Text(LOCTEXT("Go","前往")).Height(64.f).MinWidth(146.f)
                .TestId(FName(*(TEXT("Act.Go.")+Id.ToString())))
                .OnClicked_Lambda([Act,WeakRoot]
                {
                    if(const auto R=WeakRoot.Pin())
                    {
                        R->CloseAllPages();
                        R->Toast(FText::Format(LOCTEXT("GoTo","前往历练 · {0}"),Act.Title));
                        if(Act.Id==TEXT("Meditate")&&!R->Model().Player.bCultivating)R->RunCommand(TEXT("ToggleCultivation"));
                    }
                });
        }
        const TSharedRef<SHorizontalBox> RewardRow=SNew(SHorizontalBox);
        for(const FReward& R:A.Rewards)
        {
            RewardRow->AddSlot().AutoWidth().Padding(FMargin(0.f,0.f,8.f,0.f))
            [
                SNew(SInkItemSlot).SlotSize(62.f).Icon(Art::ItemIcon(R.Icon)).Quality(R.Quality).Count(R.Count)
            ];
        }
        List->AddSlot().AutoHeight().Padding(FMargin(0.f,5.f))
        [
            SNew(SInkAnimBox).Delay(.1f+Motion::Stagger(N++,.04f,.2f)).Distance(24.f)
            [
                SNew(SBox).HeightOverride(198.f)
                [
                    SNew(SOverlay)
                    +SOverlay::Slot()
                    [
                        SNew(SInkPaint).OnPaint_Lambda([](FPainter& P,const FGeometry&)
                        {
                            // Caps at 1:1 so the dark ink wash on the left stays narrow on these tall cards.
                            P.Skin(Art::ESkin::RowScenic,FVector2f(0.f,0.f),P.Size(),FLinearColor::White,1.f);
                        })
                    ]
                    +SOverlay::Slot().Padding(FMargin(58.f,18.f,24.f,16.f))
                    [
                        SNew(SHorizontalBox)
                        +SHorizontalBox::Slot().FillWidth(1.f)
                        [
                            SNew(SVerticalBox)
                            +SVerticalBox::Slot().AutoHeight()[MakeText(A.Title,EFont::Section,Colors().TextDark,30.f)]
                            +SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f,2.f,0.f,6.f))[MakeText(FText::Format(LOCTEXT("DescTime","{0} · {1}"),A.Description,A.TimeText),EFont::Small,Colors().TextDarkDim,21.f)]
                            +SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f,0.f,0.f,8.f))
                            [
                                SNew(SInkProgressBar).Style(A.State==EClaim::Locked?EBar::Gold:EBar::Jade).Width(300.f).Height(24.f)
                                .Fraction(float(A.Progress)/FMath::Max(1,A.Target)).Value(FText::Format(LOCTEXT("Prog","{0}/{1}"),FText::AsNumber(A.Progress),FText::AsNumber(A.Target)))
                            ]
                            +SVerticalBox::Slot().AutoHeight()[RewardRow]
                        ]
                        +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(10.f,0.f,0.f,0.f))[Action]
                    ]
                ]
            ]
        ];
    }
    if(!N)List->AddSlot().AutoHeight().HAlign(HAlign_Center).Padding(FMargin(0.f,50.f))[MakeText(LOCTEXT("NoAct","此类活动暂未开放"),EFont::Body,Colors().TextDarkDim,27.f)];
    Content->SetContent(List);
}

#undef LOCTEXT_NAMESPACE

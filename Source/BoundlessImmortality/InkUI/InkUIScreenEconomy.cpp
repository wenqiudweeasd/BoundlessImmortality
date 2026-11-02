// 灵玉商城, 武灵商城, 仙道殿 (金元商城) and 每日一签: pages over the 仙途 ledger's item economy (XianTuCatalog).

#include "InkUI/InkUIScreensXianTu.h"
#include "InkUI/InkUIXianTuKit.h"
#include "XianTu/XianTuSubsystem.h"
#include "XianTu/XianTuCatalog.h"
#include "XianTu/XianTuBreakthroughFx.h"
#include "Combat/LiveCombat.h"
#include "Widgets/Layout/SScrollBox.h"

#define LOCTEXT_NAMESPACE "InkUIEconomy"

using namespace InkUI;
using namespace InkUI::Kit;
using namespace XianTuCatalog;

namespace
{
    const TCHAR* ShopRoutes[]={TEXT("JadeShop"),TEXT("WulingShop"),TEXT("XianDao")};

    FText MajorShort(int32 Major)
    {
        static const TCHAR* Names[]={TEXT("练气"),TEXT("筑基"),TEXT("结晶"),TEXT("金丹"),TEXT("元婴"),TEXT("化神"),TEXT("元神"),TEXT("炼虚"),TEXT("大乘"),TEXT("飞升"),TEXT("混元")};
        return FText::FromString(Names[FMath::Clamp(Major,0,10)]);
    }

    /** Feature-scene art that reads as each stage's region. */
    int32 RegionScene(const FStage& S){static const int32 Scenes[]={4,5,6,1,7,7,0,0,0,0};return Scenes[FMath::Clamp(S.Index,0,9)];}

    FText TagLabel(FName Tag)
    {
        if(Tag==TEXT("hot"))return LOCTEXT("TagHot","热卖");
        if(Tag==TEXT("new"))return LOCTEXT("TagNew","新品");
        if(Tag==TEXT("limited"))return LOCTEXT("TagLimited","限量");
        return FText::GetEmpty();
    }

    /** Price chip: currency icon and amount on a lacquer plaque. */
    TSharedRef<SWidget> PriceTag(ECurrency Currency,int64 Price,float W=150.f)
    {
        const XianTuCatalog::FCurrency& C=XianTuCatalog::Currency(Currency);
        return SNew(SInkPaint).DesiredSize(FVector2D(W,40.f)).OnPaint_Lambda([Icon=C.Icon,Price](FPainter& P,const FGeometry&)
        {
            const auto& K=Colors();const FVector2f S=P.Size();
            P.Skin(Art::ESkin::PlaqueLacquer,FVector2f(0.f,0.f),S,FLinearColor(1,1,1,.92f));P.Next();
            P.BrushFit(Art::ItemIcon(Icon),FVector2f(10.f,S.Y*.5f-15.f),FVector2f(30.f,30.f));
            P.Text(FormatAmount(double(Price)).ToString(),Font(EFont::Data,22.f),FVector2f(S.X-14.f,S.Y*.5f),K.TextGold,FVector2f(1.f,.52f));
        });
    }

    /** Cosmetic art drawn procedurally from its two colours: a blade, a portrait frame, a title plaque or a ground aura. */
    void PaintCosmetic(FPainter& P,const FCosmetic& C,FVector2f Center,float Scale,double Time,bool bLarge)
    {
        const auto& K=Colors();const float Pulse=Motion::Pulse(Time,2.6f);
        const FLinearColor A=C.Primary,B=C.Secondary;
        switch(C.Kind)
        {
        case ECosmetic::Sword:
        {
            const float L=150.f*Scale,W=16.f*Scale;
            P.Glow(Center,FVector2f(W*4.f,L*.75f),WithAlpha(A,.28f+.12f*Pulse));P.Next();
            // Blade: a tapering quad with a bright edge, guard and hilt.
            const FVector2f Tip=Center+FVector2f(0.f,-L*.62f),Base=Center+FVector2f(0.f,L*.2f);
            P.Quad(Tip+FVector2f(-1.f,0.f),Tip+FVector2f(1.f,0.f),Base+FVector2f(W*.5f,0.f),Base+FVector2f(-W*.5f,0.f),WithAlpha(FLinearColor::White,.95f),WithAlpha(A,.95f),A,Lerp(A,B,.5f));
            P.Line(Tip,Base,WithAlpha(FLinearColor::White,.75f),1.5f*Scale);
            P.Rounded(Base+FVector2f(-W*1.6f,0.f),FVector2f(W*3.2f,W*.55f),Lerp(K.Gold,B,.3f),3.f*Scale);
            P.Rounded(Base+FVector2f(-W*.32f,W*.55f),FVector2f(W*.64f,L*.22f),Lerp(B,K.InkBlack,.4f),3.f*Scale);
            P.Diamond(Base+FVector2f(0.f,W*.55f+L*.24f),FVector2f(W*.45f),K.Gold);
            P.Next();
            P.Motes(Center-FVector2f(L*.4f,L*.7f),FVector2f(L*.8f,L*1.1f),bLarge?18:8,Time,WithAlpha(Lerp(A,FLinearColor::White,.4f),.85f),uint32(C.Price),1.1f,bLarge?5.f:3.f);
            break;
        }
        case ECosmetic::Frame:
        {
            const float R=58.f*Scale;
            P.Glow(Center,FVector2f(R*1.7f),WithAlpha(A,.22f+.1f*Pulse));P.Next();
            P.BrushFit(Art::Image(Art::Paths::AvatarPortrait),Center-FVector2f(R*.8f),FVector2f(R*1.6f));
            P.Next();
            P.Ring(Center,R,6.f*Scale,A,Lerp(A,B,.6f));
            P.Ring(Center,R+7.f*Scale,2.f*Scale,WithAlpha(K.GoldBright,.9f),WithAlpha(K.Gold,.6f));
            for(int32 I=0;I<4;++I)
            {
                const float Ang=I*HALF_PI+PI*.25f;const FVector2f D(FMath::Cos(Ang),FMath::Sin(Ang));
                P.Cloud(Center+D*(R+10.f*Scale),.34f*Scale,I%2==0,WithAlpha(A,.95f),2.f*Scale);
            }
            P.Diamond(Center-FVector2f(0.f,R+9.f*Scale),FVector2f(7.f*Scale,10.f*Scale),B);
            break;
        }
        case ECosmetic::Title:
        {
            const FVector2f Ext(220.f*Scale,58.f*Scale);
            P.Glow(Center,Ext*FVector2f(.75f,1.1f),WithAlpha(A,.25f+.12f*Pulse));P.Next();
            P.Rounded(Center-Ext*.5f,Ext,WithAlpha(Lerp(B,K.InkBlack,.35f),.92f),10.f*Scale,WithAlpha(A,.95f),2.f*Scale);
            P.CornerOrnament(Center-Ext*.5f+FVector2f(6.f,6.f)*Scale,FVector2f(1.f,1.f),.5f*Scale,WithAlpha(K.GoldBright,.9f));
            P.CornerOrnament(Center+Ext*.5f-FVector2f(6.f,6.f)*Scale,FVector2f(-1.f,-1.f),.5f*Scale,WithAlpha(K.GoldBright,.9f));
            P.Next();
            P.Text(C.Name.ToString(),Font(EFont::Title,30.f*Scale),Center,Lerp(A,FLinearColor::White,.25f+.2f*Pulse),FVector2f(.5f,.52f),WithAlpha(K.InkBlack,.8f));
            P.Shine(Center-Ext*.5f,Ext,float(FMath::Fmod(Time*.35,1.0)),.18f,WithAlpha(FLinearColor::White,.18f));
            break;
        }
        default:
        {
            const float R=92.f*Scale;
            P.Glow(Center,FVector2f(R*1.2f,R*.5f),WithAlpha(A,.3f+.12f*Pulse));P.Next();
            for(int32 I=0;I<3;++I)
            {
                const float Rr=R*(.45f+.28f*I)*(1.f+.04f*FMath::Sin(float(Time)*1.6f+I));
                P.Ring(Center,Rr,3.f*Scale,WithAlpha(A,.8f-.2f*I),WithAlpha(B,.5f-.12f*I),float(Time)*(.4f+.2f*I),2.f*PI,48);
            }
            P.Rays(Center,R*.2f,R*.95f,12,float(Time)*.25f,.06f,WithAlpha(A,.22f));
            P.Next();
            P.Cloud(Center+FVector2f(-R*.55f,R*.12f),R*.007f,false,WithAlpha(FLinearColor::White,.85f),2.f*Scale);
            P.Cloud(Center+FVector2f(R*.55f,R*.12f),R*.007f,true,WithAlpha(FLinearColor::White,.85f),2.f*Scale);
            P.Motes(Center-FVector2f(R,R*.6f),FVector2f(R*2.f,R*.9f),bLarge?16:6,Time,WithAlpha(Lerp(A,FLinearColor::White,.5f),.85f),uint32(C.Price),.8f,bLarge?4.f:2.5f);
            break;
        }
        }
    }

    /** Rotated rectangle with a top-to-bottom gradient. */
    void RotRect(FPainter& P,FVector2f Center,FVector2f Half,float Angle,FLinearColor Top,FLinearColor Bottom)
    {
        const float C=FMath::Cos(Angle),S=FMath::Sin(Angle);
        auto R=[&](float X,float Y){return Center+FVector2f(X*C-Y*S,X*S+Y*C);};
        P.Quad(R(-Half.X,-Half.Y),R(Half.X,-Half.Y),R(Half.X,Half.Y),R(-Half.X,Half.Y),Top,Top,Bottom,Bottom);
    }

    /** Rotated rectangle shaded across its width (left, middle, right), so a body reads as round. */
    void RotRectH(FPainter& P,FVector2f Center,FVector2f Half,float Angle,FLinearColor Left,FLinearColor Mid,FLinearColor Right)
    {
        const float C=FMath::Cos(Angle),S=FMath::Sin(Angle);
        auto R=[&](float X,float Y){return Center+FVector2f(X*C-Y*S,X*S+Y*C);};
        P.Quad(R(-Half.X,-Half.Y),R(0.f,-Half.Y),R(0.f,Half.Y),R(-Half.X,Half.Y),Left,Mid,Mid,Left);
        P.Quad(R(0.f,-Half.Y),R(Half.X,-Half.Y),R(Half.X,Half.Y),R(0.f,Half.Y),Mid,Right,Right,Mid);
    }

    constexpr double ShakeTime=1.35,FlyTime=.7;
}

// ------------------------------------------------------------------ 灵玉商城 / 武灵商城

void SInkShopScreen::Construct(const FArguments& InArgs)
{
    Shop=FMath::Clamp<uint8>(InArgs._Shop,0,1);
    InitScreen(InArgs._Root.ToSharedRef(),ShopRoutes[Shop]);
    const bool bJade=Shop==0;
    const TWeakObjectPtr<UXianTuSubsystem> Ledger=M().XianTu;
    BuildShell(bJade?LOCTEXT("JadeShop","灵玉商城"):LOCTEXT("WulingShop","武灵商城"),bJade?LOCTEXT("JadeSeal","玉"):LOCTEXT("WulingSeal","武"),
        SNew(SVerticalBox)
        +SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(FMargin(0.f,0.f,0.f,8.f))
        [WalletBar(Ledger,bJade?TArray<FName>{TEXT("CUR_JADE"),TEXT("CUR_STONE")}:TArray<FName>{TEXT("CUR_WULING"),TEXT("CUR_STONE")},48.f)]
        +SVerticalBox::Slot().AutoHeight()
        [Tabs(bJade?FName(TEXT("JadeShop.Tabs")):FName(TEXT("WulingShop.Tabs")),bJade?TArray<FText>{LOCTEXT("JTab0","突破材料"),LOCTEXT("JTab1","道具"),LOCTEXT("JTab2","杂货")}
            :TArray<FText>{LOCTEXT("WTab0","建设"),LOCTEXT("WTab1","修行"),LOCTEXT("WTab2","护持")})]);
}

int32 SInkShopScreen::Revision() const{return M().XianTuRev;}

void SInkShopScreen::Buy(FName Id)
{
    const FSku* S=Sku(Id);const auto R=GetRoot();if(!S||!R)return;
    const XianTuCatalog::FCurrency& Cur=XianTuCatalog::Currency(S->Currency);
    R->Confirm(LOCTEXT("BuyTitle","确认购买"),FText::Format(LOCTEXT("BuyBody","以 {0} {1} 购买 {2} ×{3}？\n本地验证商城：只记入本地账本，未接入支付。"),N(S->Price),Cur.Name,XianTuRules::ItemName(S->Item),N(S->Count)),
        Guarded([this,Id]
        {
            const FSku* Def=Sku(Id);
            if(!Def||!Exec(TEXT("ShopBuy"),{Id.ToString(),TEXT("1")}))return;
            const XianTuRules::FItem* Item=XianTuRules::Item(Def->Item);
            FReward W;W.Icon=Item?Item->Icon:FName(TEXT("crystal"));W.Name=XianTuRules::ItemName(Def->Item);W.Count=Def->Count;W.Quality=EQuality(Item?FMath::Clamp(Item->Quality,0,5):0);
            if(const auto Root2=GetRoot())Root2->ShowRewards({W},LOCTEXT("BoughtTitle","购入"));
        }),false,LOCTEXT("BuyConfirm","购买"));
}

void SInkShopScreen::Rebuild()
{
    UXianTuSubsystem* Svc=X();Footer->SetContent(Hint(Shop==0?LOCTEXT("JadeFoot","本地验证商城 · 灵玉来自世界历练结算；购买只记入本地账本，未接入支付与服务端。")
        :LOCTEXT("WulingFoot","武灵商城 · 武灵来自武灵共同历练；建设材料直接存入武灵公库，不能转为个人物品。本地验证数据。")));
    if(!Svc){Body->SetContent(Paper(Hint(LOCTEXT("ShopOffline","进入游戏后可使用商城。"))));return;}
    const TSharedRef<SVerticalBox> List=SNew(SVerticalBox);int32 Order=0;
    auto Add=[&](TSharedRef<SWidget> Widget){List->AddSlot().AutoHeight().Padding(FMargin(0.f,5.f))[Appear(Widget,Order++)];};
    const int32 Major=Svc->AnchorMajor();
    auto Card=[&](const FSku& S)->TSharedRef<SWidget>
    {
        const FName Id=S.Id;const XianTuRules::FItem* Item=XianTuRules::Item(S.Item);
        const bool bLocked=Major<S.MinMajor;const int32 Bought=Svc->BoughtToday(Id);const bool bSoldOut=S.DailyLimit>0&&Bought>=S.DailyLimit;
        const bool bAfford=Svc->Wallet(S.Currency)>=S.Price;
        const FText Limit=S.DailyLimit>0?FText::Format(LOCTEXT("Limit","今日 {0}/{1}"),N(Bought),N(S.DailyLimit)):LOCTEXT("NoLimit","不限购");
        const FName Tag=S.Tag;const int32 Quality=Item?FMath::Clamp(Item->Quality,0,5):0;
        return SNew(SOverlay)
            +SOverlay::Slot()
            [
                Paper(SNew(SVerticalBox)
                    +SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(FMargin(0.f,4.f,0.f,6.f))
                    [
                        SNew(SOverlay)
                        +SOverlay::Slot()[SNew(SInkPaint).DesiredSize(FVector2D(120.f,104.f)).OnPaint_Lambda([Quality](FPainter& P,const FGeometry&)
                        {
                            const float Pulse=Motion::Pulse(Motion::Now(),3.f);
                            P.Glow(P.Size()*.5f,FVector2f(60.f,52.f),WithAlpha(QualityColor(EQuality(Quality)),.18f+.1f*Pulse*(Quality>=3?1.f:.4f)));
                        })]
                        +SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)[Cell(S.Item,S.Count,92.f)]
                    ]
                    +SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)[MakeText(XianTuRules::ItemName(S.Item),EFont::Section,Colors().TextDark,24.f)]
                    +SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(FMargin(0.f,2.f,0.f,6.f))
                    [MakeText(bLocked?FText::Format(LOCTEXT("Locked","达到{0}解锁"),MajorShort(S.MinMajor)):Limit,EFont::Small,bLocked?Colors().TextRed:Colors().TextDarkDim,19.f)]
                    +SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(FMargin(0.f,0.f,0.f,8.f))[PriceTag(S.Currency,S.Price)]
                    +SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
                    [Btn(bSoldOut?LOCTEXT("SoldOut","今日售罄"):bLocked?LOCTEXT("LockedBtn","未解锁"):LOCTEXT("BuyBtn","购买"),bSoldOut||bLocked?EButtonSkin::Paper:EButtonSkin::Gold,
                        FName(*(TEXT("Shop.Buy.")+Id.ToString())),FSimpleDelegate::CreateLambda([this,Id]{Buy(Id);}),!bLocked&&!bSoldOut&&bAfford,54.f,170.f,22.f)],
                    FMargin(18.f,16.f,18.f,18.f))
            ]
            +SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(FMargin(10.f,10.f,0.f,0.f))
            [Tag.IsNone()?StaticCastSharedRef<SWidget>(SNullWidget::NullWidget):Plaque(TagLabel(Tag),Tag==TEXT("limited")?3:2,70.f,32.f)];
    };
    TArray<TSharedRef<SWidget>> Cards;
    if(Shop==0&&Tab==0)
    {
        const FStage* Upcoming=Svc->UpcomingStage();
        const FStage* Shown=XianTuCatalog::Stage(StagePick);if(!Shown)Shown=Upcoming?Upcoming:&Stages().Last();
        // Stage picker: the target realm of each major stage; locked ones stay visible so the road ahead reads.
        const TSharedRef<SWrapBox> Picker=SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(6.f,6.f));
        for(const FStage& S:Stages())
        {
            const FName Id=S.Id;const bool bOn=&S==Shown;const bool bOpen=Major>=S.MinMajor;
            Picker->AddSlot()
            [
                SNew(SInkClickable).TestId(FName(*(TEXT("JadeShop.Stage.")+Id.ToString()))).Radius(8.f).OnClicked_Lambda([this,Id]{StagePick=Id;Refresh();})
                [
                    SNew(SInkPaint).DesiredSize(FVector2D(96.f,40.f)).OnPaint_Lambda([bOn,bOpen,Label=MajorShort(LiveCombat::MajorRealm(S.To))](FPainter& P,const FGeometry&)
                    {
                        const auto& K=Colors();
                        P.Skin(bOn?Art::ESkin::PlaqueGold:bOpen?Art::ESkin::PlaqueJade:Art::ESkin::PlaqueLacquer,FVector2f(0.f,0.f),P.Size(),FLinearColor(1,1,1,bOpen||bOn?1.f:.7f));
                        P.Next();P.Next();
                        P.Text(Label.ToString(),Font(EFont::Tiny,18.f),P.Size()*.5f,bOn||bOpen?K.TextDark:K.TextGold,FVector2f(.5f,.52f));
                    })
                ]
            ];
        }
        Add(Paper(Rows({Head(LOCTEXT("StageHead","突破阶段"),24.f),Picker}),FMargin(22.f,14.f,22.f,16.f)));
        const FStage& S=*Shown;
        Add(SNew(SOverlay)
            +SOverlay::Slot()[Scene(RegionScene(S),150.f,Major<S.MinMajor)]
            +SOverlay::Slot().VAlign(VAlign_Bottom).Padding(FMargin(20.f,0.f,20.f,12.f))
            [
                SNew(SVerticalBox)
                +SVerticalBox::Slot().AutoHeight()[MakeText(FText::Format(LOCTEXT("StageTitle","{0} · {1} → {2}"),FText::FromName(S.Id),MajorShort(LiveCombat::MajorRealm(S.From)),MajorShort(LiveCombat::MajorRealm(S.To))),EFont::Section,Colors().TextLight,27.f,1.f)]
                +SVerticalBox::Slot().AutoHeight()[MakeText(FText::Format(LOCTEXT("StageRegion","{0} · {1}{2}"),S.Region,S.Subarea,S.bTribulation?LOCTEXT("TribTag"," · 须渡天劫"):FText::GetEmpty()),EFont::Small,Colors().TextGold,20.f,1.f)]
            ]);
        for(const FSku& Sku:Skus())if(Sku.Shop==EShop::Jade&&Sku.Tab==0&&Sku.Id.ToString().Contains(S.Id.ToString()))Cards.Add(Card(Sku));
    }
    else
    {
        const EShop Want=Shop==0?EShop::Jade:EShop::Wuling;
        if(Shop==1&&Tab==0&&!(S()&&S()->Guild.bCreated))
            Add(Paper(Rows({Head(LOCTEXT("NoGuildHead","尚未创建武灵"),26.f),Hint(LOCTEXT("NoGuildBody","建设材料存入武灵公库，创建武灵后开放；修行与护持两栏可直接购买。"))}),FMargin(24.f,16.f,24.f,18.f),Art::ESkin::CardOrnate));
        for(const FSku& Sku:Skus())if(Sku.Shop==Want&&Sku.Tab==Tab)Cards.Add(Card(Sku));
    }
    Add(Grid(Cards,IsLandscape()?3:2,10.f));
    Body->SetContent(List);
}

// ------------------------------------------------------------------ 仙道殿

void SInkHallScreen::Construct(const FArguments& InArgs)
{
    InitScreen(InArgs._Root.ToSharedRef(),TEXT("XianDao"));
    const TWeakObjectPtr<UXianTuSubsystem> Ledger=M().XianTu;
    Preview=Cosmetics()[0].Id;PreviewSince=Motion::Now();
    BuildShell(LOCTEXT("HallTitle","仙道殿"),LOCTEXT("HallSeal","殿"),
        SNew(SVerticalBox)
        +SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(FMargin(0.f,0.f,0.f,8.f))[WalletBar(Ledger,{TEXT("CUR_GOLD")},48.f)]
        +SVerticalBox::Slot().AutoHeight()[Tabs(TEXT("Hall.Tabs"),{LOCTEXT("HTab0","金元商城"),LOCTEXT("HTab1","我的外观")})]);
}

int32 SInkHallScreen::Revision() const{return M().XianTuRev;}

void SInkHallScreen::PaintPreview(FPainter& P) const
{
    const auto& K=Colors();const FVector2f S=P.Size();const double Now=Motion::Now();
    // A night hall: deep indigo wash, gilded beams, a lotus dais under the look on show.
    P.VGradient(FVector2f(0.f,0.f),S,{{0.f,Hex(TEXT("141B33"))},{.62f,Hex(TEXT("2A2440"))},{1.f,Hex(TEXT("3B2A22"))}},10.f);
    P.Next();
    P.Rays(FVector2f(S.X*.5f,S.Y*.05f),20.f,S.Y*1.1f,14,float(Now)*.03f,.05f,WithAlpha(K.GoldBright,.07f));
    P.Glow(FVector2f(S.X*.5f,S.Y*.58f),FVector2f(S.X*.42f,S.Y*.42f),WithAlpha(K.GoldBright,.16f+.06f*Motion::Pulse(Now,3.2f)));
    P.Next();
    for(int32 I=0;I<2;++I)
    {
        const float X=I?S.X-34.f:34.f;
        P.VGradient(FVector2f(X-9.f,0.f),FVector2f(18.f,S.Y),{{0.f,WithAlpha(K.Cinnabar,.0f)},{.2f,WithAlpha(K.Cinnabar,.8f)},{1.f,WithAlpha(Hex(TEXT("5C1712")),.9f)}});
        P.Rounded(FVector2f(X-12.f,S.Y*.18f),FVector2f(24.f,8.f),K.Gold,3.f);
    }
    P.FadeLine(FVector2f(40.f,26.f),FVector2f(S.X-40.f,26.f),WithAlpha(K.Gold,.9f),2.f,.08f);
    // Dais.
    const FVector2f Dais(S.X*.5f,S.Y*.84f);
    P.Glow(Dais,FVector2f(170.f,30.f),WithAlpha(K.GoldBright,.35f));
    P.Rounded(Dais-FVector2f(150.f,12.f),FVector2f(300.f,24.f),WithAlpha(Hex(TEXT("6B4A22")),.95f),12.f,WithAlpha(K.GoldBright,.85f),1.5f);
    P.Next();
    for(int32 I=0;I<7;++I)
    {
        const float A=-PI*.9f+I*(PI*.8f/6.f)-PI*.0f;const FVector2f D(FMath::Cos(A),FMath::Sin(A)*.45f);
        P.Cloud(Dais+D*120.f+FVector2f(0.f,-6.f),.55f,I>=3,WithAlpha(K.GoldBright,.55f),1.6f);
    }
    P.Next();
    if(const FCosmetic* C=Cosmetic(Preview))
    {
        const float Rise=Ease(EEase::OutBack,FMath::Clamp(float(Now-PreviewSince)/.45f,0.f,1.f));
        const FVector2f Center(S.X*.5f,S.Y*.48f+(1.f-Rise)*30.f);
        PaintCosmetic(P,*C,Center,1.25f+.05f*Rise,Now,true);
        P.Next();
        P.Text(C->Name.ToString(),Font(EFont::Title,40.f),FVector2f(S.X*.5f,S.Y*.11f+10.f),K.TextGold,FVector2f(.5f,.5f),WithAlpha(K.InkBlack,.85f));
        P.Text(CosmeticKindName(C->Kind).ToString(),Font(EFont::Small,19.f),FVector2f(S.X*.5f,S.Y*.11f+44.f),WithAlpha(K.TextLight,.8f),FVector2f(.5f,.5f));
    }
    P.GoldFrame(FVector2f(0.f,0.f),S,1.f,WithAlpha(K.Gold,.9f),.8f,true);
}

void SInkHallScreen::Rebuild()
{
    const FXianTuSave* St=S();UXianTuSubsystem* Svc=X();
    Footer->SetContent(Hint(LOCTEXT("HallFoot","仙道殿只兑换外观：剑灵外观、头像框、称号与足下灵光，不改变任何属性。金元为本地验证发放，未接入充值。")));
    if(!St||!Svc){Body->SetContent(Paper(Hint(LOCTEXT("HallOffline","进入游戏后可进入仙道殿。"))));return;}
    const TSharedRef<SVerticalBox> List=SNew(SVerticalBox);int32 Order=0;
    auto Add=[&](TSharedRef<SWidget> Widget){List->AddSlot().AutoHeight().Padding(FMargin(0.f,5.f))[Appear(Widget,Order++)];};
    Add(SNew(SBox).HeightOverride(IsLandscape()?300.f:360.f)[SNew(SInkPaint).OnPaint_Lambda([this](FPainter& P,const FGeometry&){PaintPreview(P);})]);
    static const FName Kinds[]={TEXT("Sword"),TEXT("Frame"),TEXT("Title"),TEXT("Aura")};
    TArray<TSharedRef<SWidget>> Cards;
    for(const FCosmetic& C:Cosmetics())
    {
        const bool bOwned=St->Cosmetics.Contains(C.Id);
        if(Tab==1&&!bOwned)continue;
        const FName Id=C.Id;const bool bWorn=St->Wearing.FindRef(Kinds[int32(C.Kind)])==Id;const bool bShown=Preview==Id;
        const FCosmetic Copy=C;
        const TSharedRef<SWidget> Action=bOwned
            ?Btn(bWorn?LOCTEXT("TakeOff","卸下"):LOCTEXT("Wear","穿戴"),bWorn?EButtonSkin::Paper:EButtonSkin::Jade,FName(*(TEXT("Hall.Wear.")+Id.ToString())),
                FSimpleDelegate::CreateLambda([this,Id]{Exec(TEXT("CosmeticWear"),{Id.ToString()});}),true,50.f,150.f,21.f)
            :Btn(LOCTEXT("Exchange","兑换"),EButtonSkin::Gold,FName(*(TEXT("Hall.Buy.")+Id.ToString())),FSimpleDelegate::CreateLambda([this,Id]
            {
                const FCosmetic* Def=Cosmetic(Id);const auto R=GetRoot();if(!Def||!R)return;
                R->Confirm(LOCTEXT("ExTitle","兑换外观"),FText::Format(LOCTEXT("ExBody","以 {0} 金元兑换{1}「{2}」？\n外观不改变任何属性；金元为本地验证发放。"),N(Def->Price),CosmeticKindName(Def->Kind),Def->Name),
                    Guarded([this,Id]
                    {
                        const FCosmetic* D2=Cosmetic(Id);if(!D2||!Exec(TEXT("CosmeticBuy"),{Id.ToString()}))return;
                        static const TCHAR* Icons[]={TEXT("swords"),TEXT("ring_gold"),TEXT("golden_scroll"),TEXT("gold_cloud")};
                        FReward W;W.Icon=Icons[int32(D2->Kind)];W.Name=D2->Name;W.Quality=EQuality(FMath::Clamp(D2->Quality,0,5));
                        Preview=Id;PreviewSince=Motion::Now();
                        if(const auto R2=GetRoot())R2->ShowRewards({W},LOCTEXT("ExDone","兑换外观"));
                    }),false,LOCTEXT("ExConfirm","兑换"));
            }),Svc->Wallet(ECurrency::Gold)>=C.Price,50.f,150.f,21.f);
        Cards.Add(SNew(SInkClickable).TestId(FName(*(TEXT("Hall.Card.")+Id.ToString()))).Radius(12.f).OnClicked_Lambda([this,Id]{Preview=Id;PreviewSince=Motion::Now();Refresh();})
        [
            Paper(SNew(SVerticalBox)
                +SVerticalBox::Slot().AutoHeight()
                [
                    SNew(SBox).HeightOverride(128.f)
                    [
                        SNew(SInkPaint).OnPaint_Lambda([Copy,bShown,bOwned,bWorn](FPainter& P,const FGeometry&)
                        {
                            const auto& K=Colors();const FVector2f S=P.Size();
                            P.VGradient(FVector2f(0.f,0.f),S,{{0.f,Hex(TEXT("1A2140"))},{1.f,Hex(TEXT("33263A"))}},8.f);
                            P.Next();
                            PaintCosmetic(P,Copy,FVector2f(S.X*.5f,S.Y*.55f),.62f,Motion::Now(),false);
                            P.Next();
                            if(Copy.bLimited)P.Text(TEXT("限定"),Font(EFont::Tiny,16.f),FVector2f(10.f,8.f),K.GoldBright,FVector2f(0.f,0.f));
                            if(bWorn)P.Text(TEXT("穿戴中"),Font(EFont::Tiny,16.f),FVector2f(S.X-10.f,8.f),K.JadeBright,FVector2f(1.f,0.f));
                            else if(bOwned)P.Text(TEXT("已拥有"),Font(EFont::Tiny,16.f),FVector2f(S.X-10.f,8.f),K.TextLight,FVector2f(1.f,0.f));
                            P.GoldFrame(FVector2f(0.f,0.f),S,1.f,WithAlpha(bShown?K.GoldBright:K.GoldDeep,bShown?1.f:.8f),.5f,bShown);
                        })
                    ]
                ]
                +SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(FMargin(0.f,8.f,0.f,0.f))[MakeText(C.Name,EFont::Section,QualityColor(EQuality(FMath::Clamp(C.Quality,0,5)))*FLinearColor(.75f,.75f,.75f,1.f),24.f)]
                +SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(FMargin(0.f,2.f,0.f,6.f))[MakeText(CosmeticKindName(C.Kind),EFont::Small,Colors().TextDarkDim,18.f)]
                +SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(FMargin(0.f,0.f,0.f,8.f))[bOwned?StaticCastSharedRef<SWidget>(Plaque(LOCTEXT("OwnedP","已拥有"),0,110.f,34.f)):PriceTag(ECurrency::Gold,C.Price,140.f)]
                +SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)[Action],
                FMargin(14.f,14.f,14.f,16.f))
        ]);
    }
    if(!Cards.Num())Add(Paper(Hint(LOCTEXT("NoLooks","尚未兑换任何外观。在金元商城兑换后，可在这里穿戴与卸下。"))));
    else Add(Grid(Cards,IsLandscape()?3:2,10.f));
    if(const FCosmetic* C=Cosmetic(Preview))Add(Paper(Rows({Head(C->Name,26.f),Line(C->Description,21.f),Hint(LOCTEXT("LookRule","外观同类只能穿戴一件；穿戴、卸下即时生效，只影响显示。"))}),FMargin(24.f,14.f,24.f,16.f)));
    Body->SetContent(List);
}

// ------------------------------------------------------------------ 每日一签

void SInkFortuneScreen::Construct(const FArguments& InArgs)
{
    InitScreen(InArgs._Root.ToSharedRef(),TEXT("Fortune"));
    BuildShell(LOCTEXT("FortuneTitle","每日一签"),LOCTEXT("FortuneSeal","签"),
        SNew(SBox).HAlign(HAlign_Center)
        [
            MakeText(TAttribute<FText>::CreateLambda([this]
            {
                const UXianTuSubsystem* Q=X();if(!Q)return LOCTEXT("FortuneOff","进入游戏后可求签");
                const EFortune Today=Q->TodayFortune();
                if(Today==EFortune::None||ShakeStart>=0)return LOCTEXT("FortuneIdle","诚心求签 · 每日一次 · 子时更新");
                return FText::Format(LOCTEXT("FortuneLeft","今日签运 · {0} 后可再求"),XianTuRules::Clock(Q->SecondsToNextDay()));
            }),EFont::Section,Colors().TextDark,24.f)
        ]);
}

int32 SInkFortuneScreen::Revision() const{return M().XianTuRev;}

void SInkFortuneScreen::Tick(const FGeometry& AllottedGeometry,const double InCurrentTime,const float InDeltaTime)
{
    SInkXianTuPage::Tick(AllottedGeometry,InCurrentTime,InDeltaTime);
    // The ledger has the result at once; the page keeps it hidden until the stick has flown out.
    if(ShakeStart>=0&&Motion::Now()-ShakeStart>=(Motion::IsReduced()?.2:ShakeTime+FlyTime)){ShakeStart=-1;RevealStart=Motion::Now();Pending=0;Refresh();}
}

void SInkFortuneScreen::Draw()
{
    UXianTuSubsystem* Svc=X();const auto R=GetRoot();if(!Svc||!R||ShakeStart>=0)return;
    // The ledger draws at once; no toast, so the result waits for the stick to leap out.
    FText Message;
    if(!Svc->Execute(TEXT("FortuneDraw"),{},Message)){R->Toast(Message,2);return;}
    Pending=uint8(Svc->TodayFortune());ShakeStart=Motion::Now();RevealStart=-1;
    R->Model().XianTuRev=Svc->GetRevision();R->Model().Touch();
    Refresh();
}

void SInkFortuneScreen::PaintAltar(FPainter& P) const
{
    const auto& K=Colors();const FVector2f S=P.Size();const double Now=Motion::Now();
    const UXianTuSubsystem* Q=X();
    const EFortune Today=Q?Q->TodayFortune():EFortune::None;
    const bool bShaking=ShakeStart>=0;const double T=bShaking?Now-ShakeStart:0.0;
    const bool bRevealed=!bShaking&&Today!=EFortune::None;
    const FFortune& Def=Fortune(bShaking?EFortune(Pending):Today);
    // Temple dusk: lacquer wall, a warm halo, incense threads drifting up.
    P.VGradient(FVector2f(0.f,0.f),S,{{0.f,Hex(TEXT("2A1410"))},{.55f,Hex(TEXT("4A1F16"))},{1.f,Hex(TEXT("1B0E0B"))}},10.f);
    P.Next();
    const FLinearColor Halo=bRevealed?Def.Color:K.GoldBright;
    P.Glow(FVector2f(S.X*.5f,S.Y*.5f),FVector2f(S.X*.5f,S.Y*.48f),WithAlpha(Halo,(bRevealed?.28f:.16f)+.06f*Motion::Pulse(Now,3.4f)));
    for(int32 I=0;I<3;++I)
    {
        const float X0=S.X*(.16f+.34f*I),Sway=FMath::Sin(float(Now)*.7f+I*1.7f)*18.f;
        P.Curve(FVector2f(X0,S.Y*.98f),FVector2f(X0+Sway,S.Y*.72f),FVector2f(X0-Sway,S.Y*.45f),FVector2f(X0+Sway*.5f,S.Y*.12f),WithAlpha(FLinearColor(.85f,.8f,.75f,1.f),.14f),2.f);
    }
    P.Next();
    P.Motes(FVector2f(0.f,S.Y*.2f),FVector2f(S.X,S.Y*.8f),14,Now,WithAlpha(K.GoldBright,.6f),31,.8f,4.f);
    P.Next();
    if(bRevealed)
    {
        // The drawn stick stands tall; good fortune throws golden rays, omens smoulder in smoke and cracks.
        const float Since=RevealStart>=0?float(Now-RevealStart):10.f;
        const float Pop=Motion::IsReduced()?1.f:Ease(EEase::OutBack,FMath::Clamp(Since/.55f,0.f,1.f));
        const FVector2f C(S.X*.5f,S.Y*.5f);
        if(!Def.bOmen&&Def.Luck>0)
        {
            P.Rays(C,40.f,S.Y*.62f,Def.Luck>=.5f?20:12,float(Now)*.12f,.05f,WithAlpha(Def.Color,(Def.Luck>=.5f?.32f:.2f)*FMath::Min(1.f,Since*2.f)));
            if(Since<1.2f)P.Ring(C,40.f+Since*360.f,10.f*(1.f-Since/1.2f),WithAlpha(Def.Color,.0f),WithAlpha(FLinearColor::White,.7f*(1.f-Since/1.2f)));
        }
        else if(Def.bOmen)
        {
            for(int32 I=0;I<6;++I)
            {
                const float A=I*1.047f+float(Now)*.2f;const FVector2f D(FMath::Cos(A),FMath::Sin(A));
                P.Glow(C+D*(120.f+12.f*FMath::Sin(float(Now)*1.3f+I)),FVector2f(130.f,100.f),WithAlpha(Hex(TEXT("0B0608")),.8f));
            }
            P.Next();
            for(int32 I=0;I<5;++I)
            {
                const float A=-1.2f+I*.6f;FVector2f From=C+FVector2f(FMath::Cos(A),FMath::Sin(A))*80.f;
                for(int32 J=0;J<4;++J){const FVector2f To=From+FVector2f(FMath::Cos(A+.4f*FMath::Sin(J*2.1f+I)),FMath::Sin(A+.4f*FMath::Sin(J*2.1f+I)))*28.f;P.Line(From,To,WithAlpha(Def.Color,.75f),2.f);From=To;}
            }
        }
        P.Next();
        const FVector2f Half(62.f*Pop,214.f*Pop);
        P.Halo(C-Half,Half*2.f,10.f,16.f,WithAlpha(K.InkBlack,.5f));
        P.VGradient(C-Half,Half*2.f,{{0.f,Hex(TEXT("F3DDAA"))},{.5f,Hex(TEXT("E2C182"))},{1.f,Hex(TEXT("C99A55"))}},8.f);
        P.Rounded(C-Half+FVector2f(6.f,6.f),Half*2.f-FVector2f(12.f,12.f),FLinearColor::Transparent,6.f,WithAlpha(Hex(TEXT("8A5A22")),.8f),1.5f);
        P.Rounded(FVector2f(C.X-Half.X,C.Y-Half.Y),FVector2f(Half.X*2.f,22.f*Pop),Def.Color,8.f);
        P.Next();
        // Vertical calligraphy: the fortune's name, then its verse in smaller ink.
        const FString Name=Def.Name.ToString();const float Big=58.f*Pop;
        float Y=C.Y-Half.Y+50.f*Pop;
        for(int32 I=0;I<Name.Len();++I){P.Text(Name.Mid(I,1),Font(EFont::Title,Big),FVector2f(C.X,Y),Def.bOmen?Lerp(Def.Color,K.InkBlack,.15f):Lerp(Def.Color,K.Cinnabar,.35f),FVector2f(.5f,0.f));Y+=Big*1.08f;}
        Y+=10.f*Pop;
        const FString Verse=Def.Verse.ToString();
        for(int32 I=0;I<Verse.Len();++I){P.Text(Verse.Mid(I,1),Font(EFont::Body,26.f*Pop),FVector2f(C.X,Y),Hex(TEXT("3B2414")),FVector2f(.5f,0.f));Y+=30.f*Pop;}
        P.Next();
        P.Text(TEXT("签"),Font(EFont::Title,26.f*Pop),FVector2f(C.X,C.Y+Half.Y-34.f*Pop),WithAlpha(K.Cinnabar,.9f),FVector2f(.5f,.5f));
        if(Since<.9f&&!Motion::IsReduced())P.Shine(C-Half,Half*2.f,Since/.9f,.3f,WithAlpha(FLinearColor::White,.35f));
        P.GoldFrame(FVector2f(0.f,0.f),S,1.f,WithAlpha(K.Gold,.9f),.8f,true);
        return;
    }
    // The cylinder on a lacquer altar: rocking while it shakes, sticks rattling, one leaping out at the end.
    const FVector2f Base(S.X*.5f,S.Y*.78f);
    P.Rounded(FVector2f(S.X*.5f-180.f,Base.Y+6.f),FVector2f(360.f,30.f),Hex(TEXT("3A140E")),8.f,WithAlpha(K.Gold,.8f),1.5f);
    P.Rounded(FVector2f(S.X*.5f-160.f,Base.Y+36.f),FVector2f(320.f,60.f),Hex(TEXT("2A0F0B")),6.f,WithAlpha(K.GoldDeep,.6f),1.f);
    P.Glow(Base+FVector2f(0.f,8.f),FVector2f(120.f,18.f),WithAlpha(K.InkBlack,.6f));
    for(const float Px:{-130.f,130.f})
    {
        // Incense sticks with glowing tips on the altar.
        P.Rounded(FVector2f(S.X*.5f+Px-2.f,Base.Y-60.f),FVector2f(4.f,66.f),Hex(TEXT("5C3A1E")),2.f);
        P.Glow(FVector2f(S.X*.5f+Px,Base.Y-62.f),FVector2f(9.f),WithAlpha(Hex(TEXT("FF7A30")),.9f*(.7f+.3f*Motion::Pulse(Now,1.1f,Px))));
    }
    P.Next();
    float Angle=0.f;
    if(bShaking&&T<ShakeTime)
    {
        const float Env=FMath::Sin(float(T/ShakeTime)*PI);
        Angle=Env*.22f*FMath::Sin(float(T)*26.f);
    }
    else if(!bShaking)Angle=.03f*FMath::Sin(float(Now)*1.4f);
    const float Up=-FMath::Cos(Angle),Side=FMath::Sin(Angle);
    const FVector2f Axis(Side,Up);const FVector2f Across(-Up,-Side);
    const float BodyH=250.f,BodyW=78.f;
    const FVector2f Mid=Base+Axis*(BodyH*.5f);
    // Sticks first (behind the rim).
    for(int32 I=0;I<11;++I)
    {
        const float Off=(I-5)*11.f,Rattle=bShaking&&T<ShakeTime?FMath::Sin(float(T)*40.f+I*1.9f)*6.f:0.f;
        const float Len=70.f+((I*37)%5)*9.f+Rattle;
        const FVector2f StickRoot=Base+Axis*BodyH+Across*Off;
        const float Tilt=Angle+(I-5)*.025f;
        RotRect(P,StickRoot+FVector2f(FMath::Sin(Tilt),-FMath::Cos(Tilt))*(Len*.5f),FVector2f(4.5f,Len*.5f),Tilt,Hex(TEXT("F0D69C")),Hex(TEXT("B8894A")));
        RotRect(P,StickRoot+FVector2f(FMath::Sin(Tilt),-FMath::Cos(Tilt))*(Len-5.f),FVector2f(4.5f,5.f),Tilt,Hex(TEXT("C2361F")),Hex(TEXT("8C1D18")));
    }
    P.Next();
    RotRectH(P,Mid,FVector2f(BodyW,BodyH*.5f),Angle,Hex(TEXT("4E0E0B")),Hex(TEXT("B02A1E")),Hex(TEXT("3A0A08")));
    P.Next();
    RotRect(P,Mid+Across*(-BodyW*.32f),FVector2f(7.f,BodyH*.47f),Angle,WithAlpha(FLinearColor::White,.16f),WithAlpha(FLinearColor::White,.04f));
    P.Next();
    for(const float F:{.08f,.5f,.92f})RotRect(P,Base+Axis*(BodyH*F),FVector2f(BodyW+4.f,7.f),Angle,K.GoldBright,K.GoldDeep);
    P.Next();
    P.Text(TEXT("签"),Font(EFont::Title,62.f),Base+Axis*(BodyH*.3f),K.GoldBright,FVector2f(.5f,.5f),WithAlpha(K.InkBlack,.6f));
    P.Next();
    if(bShaking&&T>=ShakeTime*.85)
    {
        // One stick leaps out, spins and grows toward the viewer.
        const float F=FMath::Clamp(float((T-ShakeTime*.85)/(FlyTime+ShakeTime*.15)),0.f,1.f);const float E=Ease(EEase::OutCubic,F);
        const FVector2f From=Base+Axis*(BodyH+40.f),To(S.X*.5f,S.Y*.3f);
        const FVector2f Pos=From+(To-From)*E+FVector2f(0.f,-60.f*FMath::Sin(E*PI));
        const float Spin=E*PI*2.f,Scale=1.f+E*1.2f;
        P.Glow(Pos,FVector2f(60.f,90.f)*Scale,WithAlpha(K.GoldBright,.4f*E));
        RotRect(P,Pos,FVector2f(6.f,48.f)*Scale,Spin,Hex(TEXT("F6E0A8")),Hex(TEXT("C99A55")));
    }
    P.GoldFrame(FVector2f(0.f,0.f),S,1.f,WithAlpha(K.Gold,.9f),.8f,true);
}

void SInkFortuneScreen::Rebuild()
{
    const FXianTuSave* St=S();UXianTuSubsystem* Svc=X();
    if(!St||!Svc){Body->SetContent(Paper(Hint(LOCTEXT("FortuneOffline","进入游戏后可求签。"))));Footer->SetContent(SNullWidget::NullWidget);return;}
    const EFortune Today=Svc->TodayFortune();const bool bShaking=ShakeStart>=0;const bool bRevealed=!bShaking&&Today!=EFortune::None;
    const TSharedRef<SVerticalBox> List=SNew(SVerticalBox);int32 Order=0;
    auto Add=[&](TSharedRef<SWidget> Widget){List->AddSlot().AutoHeight().Padding(FMargin(0.f,5.f))[Appear(Widget,Order++)];};
    Add(SNew(SBox).HeightOverride(IsLandscape()?440.f:560.f)[SNew(SInkPaint).OnPaint_Lambda([this](FPainter& P,const FGeometry&){PaintAltar(P);})]);
    if(bRevealed)
    {
        const FFortune& Def=Fortune(Today);
        const bool bGood=!Def.bOmen&&Def.Luck>0;
        Add(Paper(Rows({
            SNew(SHorizontalBox)
            +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(0.f,0.f,12.f,0.f))[Plaque(Def.Name,Def.bOmen?2:bGood?3:1,104.f,42.f)]
            +SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)[Head(FText::Format(LOCTEXT("VerseHead","「{0}」"),Def.Verse),30.f)],
            Line(Def.Poem,23.f),
            Line(Def.Effect,24.f,Def.bOmen?Colors().TextRed:bGood?Colors().TextJade:Colors().TextDarkDim),
            Hint(Def.bOmen?(Def.TribulationPenalty>0?LOCTEXT("OmenTrib","灾签：今日渡劫成功率减 10 个百分点（与助力、保底一起计算）。"):LOCTEXT("OmenHit","凶签：今日主播受到的伤害提高 50%（含天罚雷击）。"))
                :bGood?LOCTEXT("LuckHint","幸运：世界历练每完成一阶段，有机缘额外获得突破辅材（基础 20%，按幸运提高）。"):LOCTEXT("PoorHint","下下签无加成，也无惩罚；静待明日。")),
            Hint(TAttribute<FText>::CreateLambda([this]{const UXianTuSubsystem* Q=X();return Q?FText::Format(LOCTEXT("Until","签运持续到今日子时（香港时间 0 点），还剩 {0}"),XianTuRules::Clock(Q->SecondsToNextDay())):FText();}))
        }),FMargin(26.f,18.f,26.f,20.f),Art::ESkin::CardOrnate));
    }
    else
    {
        Add(Paper(Rows({
            Head(LOCTEXT("RulesHead","签运"),26.f),
            Line(LOCTEXT("Rules1","上上签 幸运 +50% · 上签 幸运 +10% · 下下签 不加幸运"),21.f),
            Line(LOCTEXT("Rules2","灾 渡劫成功率 −10% · 凶 受到伤害 +50%"),21.f,Colors().TextRed),
            Hint(LOCTEXT("Rules3","签运当日有效，子时（香港时间 0 点）更新；每日一次，不可付费改签。概率：上上签 10% · 上签 35% · 下下签 35% · 灾 10% · 凶 10%。"))
        }),FMargin(26.f,16.f,26.f,18.f)));
    }
    if(St->Fortune.History.Num())
    {
        const TSharedRef<SWrapBox> Chips=SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(8.f,6.f));
        for(int32 I=0;I<FMath::Min(7,St->Fortune.History.Num());++I)Chips->AddSlot()[Plaque(FText::FromString(St->Fortune.History[I].Mid(5)),1,170.f,34.f)];
        Add(Paper(Rows({Head(LOCTEXT("History","近日签运"),24.f),Chips}),FMargin(24.f,14.f,24.f,16.f)));
    }
    Footer->SetContent(SNew(SBox).HAlign(HAlign_Center)
    [
        Btn(bShaking?LOCTEXT("Shaking","摇签中…"):Today!=EFortune::None?LOCTEXT("DrawnBtn","今日已求签"):LOCTEXT("DrawBtn","摇 签"),EButtonSkin::Gold,TEXT("Fortune.Draw"),
            FSimpleDelegate::CreateLambda([this]{Draw();}),!bShaking&&Today==EFortune::None,76.f,300.f,30.f)
    ]);
    Body->SetContent(List);
}

#undef LOCTEXT_NAMESPACE

#include "InkUI/InkUIScreens.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/Layout/SWrapBox.h"
#include "XianTu/XianTuSubsystem.h"

#define LOCTEXT_NAMESPACE "InkUIRole"

using namespace InkUI;

namespace
{
    /** Light inset row used for stat cells on the parchment. */
    TSharedRef<SWidget> StatCell(const FSlateBrush* Icon,const FText& Label,const FText& Value)
    {
        return SNew(SBox).HeightOverride(92.f)
        [
            SNew(SOverlay)
            +SOverlay::Slot()
            [
                SNew(SInkPaint).OnPaint_Lambda([](FPainter& P,const FGeometry&)
                {
                    const auto& K=Colors();
                    P.HGradient(FVector2f(0.f,0.f),P.Size(),{{0.f,WithAlpha(K.PaperShade,.55f)},{1.f,WithAlpha(K.PaperShade,.12f)}},8.f);
                    P.Next();
                    P.Rounded(FVector2f(0.f,10.f),FVector2f(4.f,P.Size().Y-20.f),WithAlpha(K.GoldDeep,.8f),2.f);
                    P.FadeLine(FVector2f(8.f,P.Size().Y-1.f),FVector2f(P.Size().X,P.Size().Y-1.f),WithAlpha(K.PaperEdge,.6f),1.f,.1f);
                })
            ]
            +SOverlay::Slot().Padding(FMargin(12.f,6.f,10.f,6.f))
            [
                SNew(SHorizontalBox)
                +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                [
                    SNew(SInkPaint).DesiredSize(FVector2D(62.f,62.f)).OnPaint_Lambda([Icon](FPainter& P,const FGeometry&)
                    {
                        P.Glow(P.Size()*.5f,FVector2f(30.f),WithAlpha(Colors().GoldBright,.25f));
                        P.Next();
                        P.BrushFit(Icon,FVector2f(4.f,4.f),P.Size()-FVector2f(8.f,8.f));
                    })
                ]
                +SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center).Padding(FMargin(10.f,0.f,0.f,0.f))
                [
                    SNew(SVerticalBox)
                    +SVerticalBox::Slot().AutoHeight()[MakeText(Label,EFont::Small,Colors().TextDarkDim,22.f)]
                    +SVerticalBox::Slot().AutoHeight()[MakeText(Value,EFont::Data,Colors().TextDark,29.f)]
                ]
            ]
        ];
    }

    FText RealmLabel(int32 Index,const FText& Fallback)
    {
        static const TCHAR* Names[]={TEXT("练气一阶"),TEXT("练气二阶"),TEXT("练气三阶"),TEXT("练气四阶"),TEXT("练气五阶"),TEXT("练气六阶"),TEXT("练气七阶"),TEXT("练气八阶"),TEXT("练气九阶"),
            TEXT("筑基初期"),TEXT("筑基中期"),TEXT("筑基后期"),TEXT("筑基大圆满"),TEXT("结晶一阶"),TEXT("结晶二阶"),TEXT("结晶三阶"),TEXT("金丹一阶"),TEXT("金丹二阶"),TEXT("金丹三阶"),
            TEXT("金丹四阶"),TEXT("金丹五阶"),TEXT("金丹六阶"),TEXT("元婴"),TEXT("化神"),TEXT("元神一阶"),TEXT("元神二阶"),TEXT("炼虚一阶"),TEXT("炼虚二阶"),TEXT("炼虚三阶"),
            TEXT("炼虚四阶"),TEXT("大乘"),TEXT("飞升"),TEXT("混元无极"),TEXT("混元无极")};
        return Index>=0&&Index<UE_ARRAY_COUNT(Names)?FText::FromString(Names[Index]):Fallback;
    }

    void PaintRealmBanner(FPainter& P,int32 Index,const FText& Fallback,FVector2f Pos,FVector2f Extent)
    {
        if(Index>=0&&Index<34&&Art::IsValid(Art::RealmBanner(Index))){P.BrushFit(Art::RealmBanner(Index),Pos,Extent);return;}
        P.Skin(Art::ESkin::RealmPlaque,Pos,Extent);
        P.Next();
        P.Text(Fallback.ToString(),Font(EFont::Small,Extent.Y*.55f),Pos+Extent*.5f,Colors().TextGold,FVector2f(.5f,.52f));
    }

    const FItem* EquippedIn(const FModel& Model,FName Slot)
    {
        if(Slot==TEXT("Treasure"))return Model.Items.FindByPredicate([](const FItem& I){return I.Category==TEXT("Treasure");});
        return Model.Items.FindByPredicate([&](const FItem& I){return I.Category==TEXT("Equipment")&&I.EquipSlot==Slot;});
    }
}

void SInkCharacterScreen::Construct(const FArguments& InArgs)
{
    InitScreen(InArgs._Root.ToSharedRef(),TEXT("Character"));
    const TSharedRef<SWidget> Hero=SNew(SInkAnimBox).Enter(EEnter::Fade).Delay(.08f)
    [
        SNew(SBox).HeightOverride(268.f)
        [
            SNew(SOverlay)
            +SOverlay::Slot()
            [
                SNew(SInkPaint).OnPaint_Lambda([this](FPainter& P,const FGeometry&)
                {
                    const auto& K=Colors();
                    P.Skin(Art::ESkin::PanelFramed,FVector2f(0.f,0.f),P.Size(),FLinearColor::White,.5f);
                    P.Next();
                    const FVector2f C(118.f,126.f);
                    P.Glow(C,FVector2f(132.f),WithAlpha(K.Jade,.22f+.08f*Motion::Pulse(Motion::Now(),4.f)));
                    P.Next();
                    SInkAvatar::PaintAvatar(P,C,198.f,Art::Image(Art::Paths::AvatarPortrait),M().Player.Name,0);
                })
            ]
            +SOverlay::Slot().Padding(FMargin(236.f,24.f,22.f,18.f))
            [
                SNew(SVerticalBox)
                +SVerticalBox::Slot().AutoHeight()[MakeText(TAttribute<FText>::CreateLambda([this]{return M().Player.Name;}),EFont::Title,Colors().TextDark,46.f)]
                +SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f,2.f,0.f,10.f))
                [
                    SNew(SInkPaint).DesiredSize(FVector2D(270.f,40.f)).OnPaint_Lambda([this](FPainter& P,const FGeometry&)
                    {
                        PaintRealmBanner(P,M().Player.RealmIndex,M().Player.Realm,FVector2f(0.f,0.f),P.Size());
                    })
                ]
                +SVerticalBox::Slot().AutoHeight()
                [
                    SNew(SInkProgressBar).Style(EBar::Jade).Width(330.f).Height(28.f)
                    .Fraction_Lambda([this]{return M().QiFraction();})
                    .Value_Lambda([this]
                    {
                        const FPlayerState& Pl=M().Player;
                        if(Pl.MaxQi>0&&Pl.Qi>=Pl.MaxQi)return LOCTEXT("QiFullCard","修为圆满 · 可突破");
                        return FText::Format(LOCTEXT("QiPair","修为 {0} / {1}"),FormatAmount(Pl.Qi),FormatAmount(Pl.MaxQi));
                    })
                ]
                +SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f,12.f,0.f,0.f))
                [
                    MakeText(TAttribute<FText>::CreateLambda([this]{return FText::Format(LOCTEXT("QiRate","修炼速度 {0}/秒"),FText::AsNumber(M().Player.QiPerSecond,&FNumberFormattingOptions::DefaultNoGrouping()));}),EFont::Small,Colors().TextJade,23.f)
                ]
            ]
        ]
    ];
    BuildPage(LOCTEXT("Title","角色"),LOCTEXT("Seal","道"),
        SNew(SInkTabBar).Tabs(TArray<FText>{LOCTEXT("TabAttr","属性"),LOCTEXT("TabGear","装备"),LOCTEXT("TabRealm","境界")}).TestPrefix(TEXT("Character.Tabs"))
            .OnSelected_Lambda([this](int32 Index){ShowTab(Index);}),
        SNew(SScrollBox).ScrollBarStyle(&ScrollStyle())
        +SScrollBox::Slot()
        [
            SNew(SVerticalBox)
            +SVerticalBox::Slot().AutoHeight()[Hero]
            +SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f,14.f,0.f,10.f))[SAssignNew(Content,SBox)]
        ],
        SNullWidget::NullWidget,SNullWidget::NullWidget);
    ShowTab(0);
}

void SInkCharacterScreen::OnModelChanged()
{
    if(SeenAttributes==M().AttributesRev&&SeenItems==M().ItemsRev)return;
    ShowTab(Tab);
}

void SInkCharacterScreen::ShowTab(int32 Index)
{
    Tab=Index;
    SeenAttributes=M().AttributesRev;SeenItems=M().ItemsRev;
    Content->SetContent(Index==1?BuildEquipment():Index==2?BuildRealm():BuildAttributes());
}

TSharedRef<SWidget> SInkCharacterScreen::BuildAttributes()
{
    const TSharedRef<SUniformGridPanel> Grid=SNew(SUniformGridPanel).SlotPadding(FMargin(6.f));
    int32 N=0;
    for(const FAttribute& A:M().Attributes)
    {
        if(A.Id==TEXT("Realm")||A.Id==TEXT("Cultivation"))continue;
        Grid->AddSlot(N%2,N/2)
        [
            SNew(SInkAnimBox).Delay(Motion::Stagger(N,.025f,.2f)).Distance(18.f)[StatCell(Art::AttributeIcon(A.Id),A.Label,A.Value)]
        ];
        ++N;
    }
    if(!N)return MakeText(LOCTEXT("NoAttr","属性尚未同步"),EFont::Body,Colors().TextDarkDim,26.f);
    return Grid;
}

TSharedRef<SWidget> SInkCharacterScreen::BuildEquipment()
{
    const TWeakPtr<SInkUIRoot> WeakRoot=GetRoot();
    auto Gear=[&](FName Slot,const FText& Label,int32 Order,bool bLocked=false)
    {
        const FItem* Item=bLocked?nullptr:EquippedIn(M(),Slot);
        return SNew(SInkAnimBox).Enter(EEnter::Pop).Delay(.05f+Order*.04f)
        [
            SNew(SVerticalBox)
            +SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
            [
                SNew(SInkItemSlot).SlotSize(108.f).Icon(Item?Art::ItemIcon(Item->Icon):nullptr).Quality(Item?Item->Quality:EQuality::Common)
                .Empty(!Item).Locked(bLocked).TestId(FName(*FString::Printf(TEXT("Character.Gear%d"),Order)))
                .OnClicked_Lambda([WeakRoot]{if(const auto R=WeakRoot.Pin())R->OpenPage(TEXT("Inventory"));})
            ]
            +SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(FMargin(0.f,6.f,0.f,10.f))
            [
                MakeText(Item?Item->Name:Label,EFont::Small,Item?Lerp(QualityColor(Item->Quality),Colors().TextDark,.4f):Colors().TextDarkDim,22.f)
            ]
        ];
    };
    return SNew(SVerticalBox)
    +SVerticalBox::Slot().AutoHeight()
    [
        SNew(SHorizontalBox)
        +SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
        [
            SNew(SVerticalBox)
            +SVerticalBox::Slot().AutoHeight()[Gear(TEXT("Weapon"),LOCTEXT("Weapon","武器"),0)]
            +SVerticalBox::Slot().AutoHeight()[Gear(TEXT("Armor"),LOCTEXT("Armor","护甲"),1)]
            +SVerticalBox::Slot().AutoHeight()[Gear(TEXT("Ring"),LOCTEXT("Ring","戒指"),2)]
        ]
        +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
        [
            SNew(SInkAnimBox).Enter(EEnter::Fade).Duration(.6f)
            [
                SNew(SInkPaint).DesiredSize(FVector2D(250.f,420.f)).OnPaint_Lambda([](FPainter& P,const FGeometry&)
                {
                    const auto& K=Colors();const FVector2f S=P.Size();const double Now=Motion::Now();
                    P.Glow(S*FVector2f(.5f,.52f),FVector2f(S.X*.62f,S.Y*.46f),WithAlpha(K.Jade,.2f+.1f*Motion::Pulse(Now,3.6f)));
                    P.Next();
                    P.BrushFit(Art::Image(Art::Paths::Meditation),FVector2f(0.f,S.Y*.12f),FVector2f(S.X,S.Y*.76f),FLinearColor(1,1,1,.92f));
                    P.Next();
                    P.Ring(S*FVector2f(.5f,.5f),S.X*.46f,1.5f,WithAlpha(K.GoldDeep,.55f),WithAlpha(K.GoldDeep,.55f),float(Now*.2),PI*1.6f,64);
                    P.Motes(FVector2f(S.X*.15f,S.Y*.2f),FVector2f(S.X*.7f,S.Y*.6f),8,Now,WithAlpha(K.Jade,.8f),17,1.f,4.f);
                })
            ]
        ]
        +SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
        [
            SNew(SVerticalBox)
            +SVerticalBox::Slot().AutoHeight()[Gear(TEXT("Necklace"),LOCTEXT("Necklace","项链"),3)]
            +SVerticalBox::Slot().AutoHeight()[Gear(TEXT("Treasure"),LOCTEXT("Treasure","法宝"),4)]
            +SVerticalBox::Slot().AutoHeight()[Gear(TEXT("Mount"),LOCTEXT("Mount","坐骑"),5,true)]
        ]
    ]
    +SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(FMargin(0.f,4.f,0.f,0.f))
    [
        MakeText(LOCTEXT("GearHint","点击装备格前往背包 · 坐骑玩法尚未开放"),EFont::Small,Colors().TextDarkDim,22.f)
    ];
}

TSharedRef<SWidget> SInkCharacterScreen::BuildRealm()
{
    const TWeakPtr<SInkUIRoot> WeakRoot=GetRoot();
    const FPlayerState& Pl=M().Player;
    const bool bReady=M().QiFraction()>=1.f;
    const FItem* Pill=M().Items.FindByPredicate([](const FItem& I){return I.Id==TEXT("FoundationPill");});
    const int32 PillCount=Pill?Pill->Count:0;
    const FText Next=RealmLabel(Pl.RealmIndex+1,LOCTEXT("NextUnknown","下一境界"));
    return SNew(SVerticalBox)
    +SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
    [
        SNew(SInkAnimBox).Enter(EEnter::Fade)
        [
            SNew(SInkPaint).DesiredSize(FVector2D(620.f,96.f)).OnPaint_Lambda([this,Next](FPainter& P,const FGeometry&)
            {
                const auto& K=Colors();const FVector2f S=P.Size();
                const FPlayerState& Me=M().Player;
                PaintRealmBanner(P,Me.RealmIndex,Me.Realm,FVector2f(0.f,24.f),FVector2f(250.f,48.f));
                P.Next();
                const float Flow=float(FMath::Fmod(Motion::Now()*.8,1.0));
                for(int32 I=0;I<3;++I)P.Diamond(FVector2f(S.X*.5f-24.f+I*24.f,48.f),FVector2f(7.f,9.f),WithAlpha(K.GoldDeep,.35f+.65f*FMath::Clamp(1.f-FMath::Abs(Flow*3.f-I),0.f,1.f)));
                PaintRealmBanner(P,Me.RealmIndex>=0?Me.RealmIndex+1:-1,Next,FVector2f(S.X-250.f,24.f),FVector2f(250.f,48.f));
            })
        ]
    ]
    +SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f,8.f))
    [
        SNew(SInkAnimBox).Delay(.06f)
        [
            SNew(SOverlay)
            +SOverlay::Slot()[SNew(SInkPaint).OnPaint_Lambda([](FPainter& P,const FGeometry&){P.Skin(Art::ESkin::CardPaper,FVector2f(0.f,0.f),P.Size(),FLinearColor::White,.55f);})]
            +SOverlay::Slot().Padding(FMargin(34.f,26.f,34.f,26.f))
            [
                SNew(SVerticalBox)
                +SVerticalBox::Slot().AutoHeight()[MakeText(LOCTEXT("Need","突破条件"),EFont::Section,Colors().TextDark,30.f)]
                +SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f,12.f,0.f,6.f))
                [
                    SNew(SInkProgressBar).Style(EBar::Gold).Width(520.f).Height(30.f)
                    .Fraction_Lambda([this]{return M().QiFraction();})
                    .Value_Lambda([this]{return FText::Format(LOCTEXT("NeedQi","修为 {0} / {1}"),FormatAmount(M().Player.Qi),FormatAmount(M().Player.MaxQi));})
                ]
                +SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f,10.f,0.f,0.f))
                [
                    SNew(SHorizontalBox)
                    +SHorizontalBox::Slot().AutoWidth()
                    [
                        SNew(SInkItemSlot).SlotSize(84.f).Icon(Art::ItemIcon(TEXT("lotus_orb"))).Quality(EQuality::Epic).Count(PillCount).Empty(false)
                    ]
                    +SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center).Padding(FMargin(16.f,0.f,0.f,0.f))
                    [
                        SNew(SVerticalBox)
                        +SVerticalBox::Slot().AutoHeight()[MakeText(LOCTEXT("PillName","筑基丹 ×1"),EFont::Body,Lerp(QualityColor(EQuality::Epic),Colors().TextDark,.4f),27.f)]
                        +SVerticalBox::Slot().AutoHeight()[MakeText(FText::Format(LOCTEXT("PillOwn","持有 {0} · 突破时消耗"),FText::AsNumber(PillCount)),EFont::Small,Colors().TextDarkDim,22.f)]
                    ]
                ]
                +SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f,14.f,0.f,0.f))
                [
                    MakeText(LOCTEXT("RealmRule","修为圆满后可叩问大道；成败与消耗以游戏规则结算为准。"),EFont::Small,Colors().TextDarkDim,22.f,0.f,true)
                ]
            ]
        ]
    ]
    +SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(FMargin(0.f,12.f,0.f,4.f))
    [
        SNew(SInkAnimBox).Delay(.12f)
        [
            SNew(SInkButton).Skin(EButtonSkin::Gold).Text(LOCTEXT("Break","突破境界")).Height(86.f).MinWidth(380.f).Highlight(bReady).Enabled(bReady)
            .TestId(TEXT("Character.Breakthrough")).OnClicked_Lambda([WeakRoot]{if(const auto R=WeakRoot.Pin())R->RunCommand(TEXT("Breakthrough"));})
        ]
    ]
    +SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
    [
        MakeText(bReady?LOCTEXT("Ready","修为已满，可尝试突破"):FText::Format(LOCTEXT("NotReady","修为未满，尚需 {0}"),FormatAmount(FMath::Max(0.0,Pl.MaxQi-Pl.Qi))),EFont::Small,bReady?Colors().TextJade:Colors().TextRed,23.f)
    ];
}

void SInkInventoryScreen::Construct(const FArguments& InArgs)
{
    InitScreen(InArgs._Root.ToSharedRef(),TEXT("Inventory"));
    auto Currency=[this](FName Id)
    {
        return SNew(SInkPaint).DesiredSize(FVector2D(122.f,46.f)).OnPaint_Lambda([this,Id](FPainter& P,const FGeometry&)
        {
            const FCurrency* C=M().FindCurrency(Id);
            P.Skin(Art::ESkin::PlaqueLacquer,FVector2f(0.f,0.f),P.Size(),FLinearColor(1,1,1,.95f));
            P.Next();
            P.BrushFit(Art::ItemIcon(C?C->Icon:FName(TEXT("crystal"))),FVector2f(8.f,6.f),FVector2f(34.f,34.f));
            P.Text(C?FormatAmount(double(C->Amount)).ToString():TEXT("—"),Font(EFont::Data,20.f),FVector2f(P.Size().X-12.f,P.Size().Y*.5f),Colors().TextGold,FVector2f(1.f,.52f));
        });
    };
    const TSharedRef<SWidget> Header=SNew(SVerticalBox)
        +SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f,0.f,0.f,10.f))
        [
            SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(6.f,6.f))
            +SWrapBox::Slot()[Currency(TEXT("SpiritStone"))]
            +SWrapBox::Slot()[Currency(TEXT("CUR_JADE"))]
            +SWrapBox::Slot()[Currency(TEXT("CUR_WULING"))]
            +SWrapBox::Slot()[Currency(TEXT("CUR_SOUL"))]
            +SWrapBox::Slot()[Currency(TEXT("CUR_GOLD"))]
        ]
        +SVerticalBox::Slot().AutoHeight()
        [
            SNew(SInkTabBar).Tabs(TArray<FText>{LOCTEXT("All","全部"),LOCTEXT("Gear","装备"),LOCTEXT("Mat","材料"),LOCTEXT("Pill","丹药"),LOCTEXT("Treasure","法宝")}).Height(64.f)
            .TestPrefix(TEXT("Bag.Tabs")).OnSelected_Lambda([this](int32 Index){Category=Index;Select(FGuid());RebuildGrid();})
        ];
    const TWeakPtr<SInkUIRoot> WeakRoot=GetRoot();
    BuildPage(LOCTEXT("BagTitle","背包"),LOCTEXT("BagSeal","藏"),Header,
        SNew(SScrollBox).ScrollBarStyle(&ScrollStyle())+SScrollBox::Slot().Padding(FMargin(0.f,6.f,0.f,180.f))[SAssignNew(GridBox,SBox)],
        SNew(SHorizontalBox)
        +SHorizontalBox::Slot().AutoWidth()
        [
            SNew(SInkButton).Skin(EButtonSkin::Paper).Text(LOCTEXT("Sort","整理")).Height(66.f).MinWidth(170.f).TestId(TEXT("Bag.Sort"))
            .OnClicked_Lambda([this,WeakRoot]{M().SortItems();if(const auto R=WeakRoot.Pin())R->Toast(LOCTEXT("Sorted","已按品类与品阶整理"));})
        ]
        +SHorizontalBox::Slot().FillWidth(1.f).HAlign(HAlign_Right).VAlign(VAlign_Center)
        [
            MakeText(TAttribute<FText>::CreateLambda([this]{return FText::Format(LOCTEXT("Capacity","容量 {0}/{1} · 点选查看详情"),FText::AsNumber(M().Items.Num()),FText::AsNumber(M().BagCapacity));}),EFont::Small,Colors().TextDarkDim,22.f)
        ],
        SNew(SBox).VAlign(VAlign_Bottom).Padding(FMargin(30.f,0.f,30.f,128.f))
        [
            SAssignNew(DetailSheet,SInkAnimBox).Enter(EEnter::Up).AutoPlay(false).Distance(90.f).Duration(.3f)
            [
                SAssignNew(DetailBox,SBox)
            ]
        ]);
    DetailSheet->Snap(false);
    RebuildGrid();
}

void SInkInventoryScreen::OnModelChanged()
{
    if(SeenItems==M().ItemsRev)return;
    RebuildGrid();
    if(SelectedKey.IsValid()&&!M().FindItem(SelectedKey))Select(FGuid());else RebuildDetail();
}

void SInkInventoryScreen::OnLayoutChanged()
{
    SInkPageScreen::OnLayoutChanged();
    if(GridBox)RebuildGrid();
}

bool SInkInventoryScreen::HandleBack()
{
    if(SelectedKey.IsValid()){Select(FGuid());return true;}
    return false;
}

FString SInkInventoryScreen::Describe() const
{
    const FItem* I=SelectedKey.IsValid()?M().FindItem(SelectedKey):nullptr;
    return I?FString::Printf(TEXT("Inventory:%s"),*I->Name.ToString()):TEXT("Inventory");
}

void SInkInventoryScreen::RebuildGrid()
{
    if(!GridBox)return;
    SeenItems=M().ItemsRev;
    const auto R=GetRoot();
    const int32 Columns=IsLandscape()?8:4;
    const float Avail=R?FMath::Min(R->GetLogicalSize().X-(IsLandscape()?300.f:100.f),IsLandscape()?980.f:630.f):620.f;
    const float SlotSize=FMath::Clamp(Avail/Columns-16.f,84.f,134.f);
    static const FName Filters[]={NAME_None,TEXT("Equipment"),TEXT("Material"),TEXT("Consumable"),TEXT("Treasure")};
    const FName Filter=Filters[FMath::Clamp(Category,0,4)];
    const TSharedRef<SUniformGridPanel> Grid=SNew(SUniformGridPanel).SlotPadding(FMargin(7.f));
    int32 N=0;
    for(const FItem& Item:M().Items)
    {
        if(!Filter.IsNone()&&Item.Category!=Filter)continue;
        const FGuid Key=Item.Key;
        Grid->AddSlot(N%Columns,N/Columns)
        [
            SNew(SInkItemSlot).SlotSize(SlotSize).Icon(Art::ItemIcon(Item.Icon)).Quality(Item.Quality).Count(Item.Count).IsNew(Item.bNew)
            .Selected_Lambda([this,Key]{return SelectedKey==Key;}).TestId(N<12?FName(*FString::Printf(TEXT("Bag.Slot%d"),N)):FName())
            .OnClicked_Lambda([this,Key]{Select(SelectedKey==Key?FGuid():Key);})
        ];
        ++N;
    }
    if(Filter.IsNone())
    {
        for(;N<M().BagCapacity;++N)Grid->AddSlot(N%Columns,N/Columns)[SNew(SInkItemSlot).SlotSize(SlotSize).Empty(true)];
        for(int32 L=0;L<Columns;++L,++N)Grid->AddSlot(N%Columns,N/Columns)[SNew(SInkItemSlot).SlotSize(SlotSize).Empty(true).Locked(true)];
    }
    else if(!N)
    {
        GridBox->SetContent(SNew(SBox).HAlign(HAlign_Center).Padding(FMargin(0.f,80.f))[MakeText(LOCTEXT("EmptyCat","此类暂无物品"),EFont::Body,Colors().TextDarkDim,27.f)]);
        return;
    }
    GridBox->SetContent(Grid);
}

void SInkInventoryScreen::Select(const FGuid& Key)
{
    const bool bWasOpen=SelectedKey.IsValid();
    SelectedKey=Key;
    if(!Key.IsValid()){if(bWasOpen)DetailSheet->PlayOut(0.f,.16f);return;}
    if(FItem* Item=M().Items.FindByPredicate([&](const FItem& I){return I.Key==Key;}))Item->bNew=false;
    RebuildDetail();
    DetailSheet->PlayIn();
}

void SInkInventoryScreen::RebuildDetail()
{
    const FItem* Item=SelectedKey.IsValid()?M().FindItem(SelectedKey):nullptr;
    if(!Item||!DetailBox)return;
    const TWeakPtr<SInkUIRoot> WeakRoot=GetRoot();
    const FLinearColor NameColor=Lerp(QualityColor(Item->Quality),Colors().TextDark,.38f);
    auto NotHere=[WeakRoot]{if(const auto R=WeakRoot.Pin())R->Toast(LOCTEXT("NoMarket","坊市服务未接入，本地验证暂不可交易"),2);};
    DetailBox->SetContent(
        SNew(SOverlay)
        +SOverlay::Slot()
        [
            SNew(SInkPaint).OnPaint_Lambda([](FPainter& P,const FGeometry&)
            {
                P.Halo(FVector2f(8.f,8.f),P.Size()-FVector2f(16.f,16.f),12.f,30.f,WithAlpha(Colors().InkBlack,.55f));
                P.Next();
                P.Skin(Art::ESkin::CardOrnate,FVector2f(0.f,0.f),P.Size(),FLinearColor::White,.55f);
            })
        ]
        +SOverlay::Slot().Padding(FMargin(34.f,30.f,30.f,28.f))
        [
            SNew(SVerticalBox)
            +SVerticalBox::Slot().AutoHeight()
            [
                SNew(SHorizontalBox)
                +SHorizontalBox::Slot().AutoWidth()
                [
                    SNew(SInkItemSlot).SlotSize(112.f).Icon(Art::ItemIcon(Item->Icon)).Quality(Item->Quality).Count(Item->Count)
                ]
                +SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center).Padding(FMargin(18.f,0.f,0.f,0.f))
                [
                    SNew(SVerticalBox)
                    +SVerticalBox::Slot().AutoHeight()[MakeText(Item->Name,EFont::Section,NameColor,34.f)]
                    +SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f,4.f))
                    [
                        MakeText(FText::Format(LOCTEXT("Kind","{0} · {1}"),QualityName(Item->Quality),CategoryLabel(Item->Category)),EFont::Small,Colors().TextDarkDim,23.f)
                    ]
                    +SVerticalBox::Slot().AutoHeight()[MakeText(FText::Format(LOCTEXT("Count","持有 {0}"),FText::AsNumber(Item->Count)),EFont::Data,Colors().TextDark,24.f)]
                ]
                +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top)
                [
                    SNew(SInkCloseButton).TestId(TEXT("Bag.DetailClose")).OnClicked_Lambda([this]{Select(FGuid());})
                ]
            ]
            +SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f,10.f))
            [
                SNew(SInkPaint).DesiredSize(FVector2D(500.f,16.f)).OnPaint_Lambda([](FPainter& P,const FGeometry&){P.Divider(P.Size()*.5f,P.Size().X*.94f,WithAlpha(Colors().GoldDeep,.9f));})
            ]
            +SVerticalBox::Slot().AutoHeight()[MakeText(Item->Description,EFont::Body,Colors().TextDark,25.f,0.f,true)]
            +SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right).Padding(FMargin(0.f,18.f,0.f,0.f))
            [
                SNew(SHorizontalBox)
                +SHorizontalBox::Slot().AutoWidth().Padding(FMargin(6.f,0.f))
                [
                    SNew(SInkButton).Skin(EButtonSkin::Paper).Text(LOCTEXT("Sell","出售")).Height(64.f).MinWidth(140.f).TestId(TEXT("Bag.Sell")).OnClicked_Lambda(NotHere)
                ]
                +SHorizontalBox::Slot().AutoWidth().Padding(FMargin(6.f,0.f))
                [
                    SNew(SInkButton).Skin(EButtonSkin::Paper).Text(LOCTEXT("Break","分解")).Height(64.f).MinWidth(140.f).TestId(TEXT("Bag.Decompose")).OnClicked_Lambda(NotHere)
                ]
                +SHorizontalBox::Slot().AutoWidth().Padding(FMargin(6.f,0.f))
                [
                    SNew(SInkButton).Skin(EButtonSkin::Gold).Text(LOCTEXT("Use","使用")).Height(64.f).MinWidth(160.f).Enabled(Item->bUsable).Highlight(Item->bUsable)
                    .TestId(TEXT("Bag.Use")).OnClicked_Lambda([this]{UseSelected();})
                ]
            ]
        ]);
}

void SInkInventoryScreen::UseSelected()
{
    // Ledger props (仙途 items, "XT_" in the bag) are used by the ledger, which applies their effect on the anchor.
    if(const FItem* Item=M().FindItem(SelectedKey);Item&&Item->Id.ToString().StartsWith(TEXT("XT_")))
    {
        const auto R=GetRoot();UXianTuSubsystem* Ledger=M().XianTu.Get();if(!R)return;
        if(!Ledger){R->Toast(LOCTEXT("NoLedgerUse","进入游戏后可使用道具"),2);return;}
        FText Result;const bool bOk=Ledger->Execute(TEXT("PropUse"),{Item->Id.ToString().Mid(3)},Result);
        R->Toast(Result,bOk?1:2);M().XianTuRev=Ledger->GetRevision();M().Touch();
        return;
    }
    FText Message;TArray<FReward> Rewards;
    const bool bUsed=M().UseItem(SelectedKey,Message,Rewards);
    if(const auto R=GetRoot())
    {
        R->Toast(Message,bUsed?1:2);
        if(Rewards.Num())R->ShowRewards(Rewards,LOCTEXT("Opened","开启获得"));
    }
}

#undef LOCTEXT_NAMESPACE

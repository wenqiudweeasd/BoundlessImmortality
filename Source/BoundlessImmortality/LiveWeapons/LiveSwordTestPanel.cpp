#include "LiveSwordTestPanel.h"
#include "LiveSwordTestGameMode.h"
#include "LiveScarecrow.h"
#include "LiveWeapons/SpecialSwords/LiveSpecialSwordTypes.h"
#include "LiveWeapons/SpecialSwords/LiveSpecialSword.h"
#include "LiveWeapons/SpecialSwords/LiveSwordCodex.h"
#include "XianTu/XianTuSubsystem.h"
#include "Combat/LiveCombat.h"
#include "InkUI/InkUIWidgets.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"

using namespace InkUI;

namespace
{
    constexpr float SwordChipWidth=124.f,SwordChipHeight=92.f,SwordDockWidth=696.f;
    /** Small amounts keep a decimal (a dummy's 1% defense), large ones read in 万/亿. */
    FString SwordRangeStat(double Value){return Value<100.0?FString::Printf(TEXT("%.1f"),Value):LiveSwordCodex::Amount(Value);}
    FString ScarecrowTitle(const ALiveScarecrow* S){return S->RealmPreset>=0?LiveSwordCodex::RealmName(S->RealmPreset).ToString()+TEXT(" 稻草人"):FString(TEXT("木人桩"));}
    constexpr float CodexWidth=1180.f,CodexHeight=660.f,CodexListWidth=214.f;
    FString SwordRangeDamage(double Damage){return LiveSwordCodex::Amount(Damage);}
    /** The five forms, then the normal spirit they are measured against. */
    TArray<ELiveSpecialSword> CodexKinds(){TArray<ELiveSpecialSword> Kinds=LiveSpecialSwords::All();Kinds.Add(ELiveSpecialSword::None);return Kinds;}
    FString CodexKey(ELiveSpecialSword Kind){return Kind==ELiveSpecialSword::None?FString(TEXT("Normal")):StaticEnum<ELiveSpecialSword>()->GetNameStringByValue(int64(Kind));}
    FText CodexName(ELiveSpecialSword Kind){return Kind==ELiveSpecialSword::None?FText::FromString(TEXT("普通剑灵")):LiveSpecialSwords::Name(Kind);}
    FLinearColor CodexTint(ELiveSpecialSword Kind){return Kind==ELiveSpecialSword::None?Colors().GoldBright:LiveSpecialSwords::Color(Kind);}
}

/** One special sword: a dim ink tablet while it rests, a breathing lacquer-and-gold plate in its colour while it fights. */
class SLiveSwordChip : public SInkPressable
{
public:
    SLATE_BEGIN_ARGS(SLiveSwordChip):_Kind(ELiveSpecialSword::None){}
        SLATE_ARGUMENT(ELiveSpecialSword,Kind)
        /** The sword's viewer is in the swarm. */
        SLATE_ATTRIBUTE(bool,Active)
        /** The sword itself is flying (the manager picks the viewer up within 0.3 s). */
        SLATE_ATTRIBUTE(bool,Arrived)
        SLATE_ATTRIBUTE(double,Damage)
        SLATE_EVENT(FSimpleDelegate,OnClicked)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs)
    {
        Kind=InArgs._Kind;Active=InArgs._Active;Arrived=InArgs._Arrived;Damage=InArgs._Damage;
        Name=LiveSpecialSwords::Name(Kind).ToString();Tint=LiveSpecialSwords::Color(Kind);
        const FString Id=TEXT("SwordRange.")+StaticEnum<ELiveSpecialSword>()->GetNameStringByValue(int64(Kind));
        InitPressable(FName(*Id),InArgs._OnClicked,true);
    }
    virtual FVector2D ComputeDesiredSize(float)const override{return FVector2D(SwordChipWidth,SwordChipHeight);}
    virtual int32 OnPaint(const FPaintArgs& Args,const FGeometry& Geometry,const FSlateRect& Culling,FSlateWindowElementList& Out,int32 Layer,const FWidgetStyle& Style,bool bParentEnabled)const override
    {
        FPainter P(Geometry,Out,Layer,Style);
        const auto& K=Colors();
        const double Now=Motion::Now();
        const float Pr=PressAmount(),Hv=HoverAmount(),Fl=FlashAmount(),Breath=Motion::Pulse(Now,Durations::BreathPeriod,Phase);
        const bool bOn=Active.Get(false),bHere=Arrived.Get(false);
        const FVector2f Ext=P.Size()*(1.f-.04f*Pr),Pos=(P.Size()-Ext)*.5f;
        if(bOn)
        {
            P.Halo(Pos,Ext,12.f,8.f+6.f*Breath,WithAlpha(Tint,.28f+.22f*Breath));P.Next();
            P.Panel(Pos,Ext,EPanel::Lacquer,12.f,true);
        }
        else P.Rounded(Pos,Ext,WithAlpha(K.InkBlack,.58f+.1f*Hv),12.f,WithAlpha(K.GoldLine,.3f+.45f*Hv),1.5f);
        P.Next();
        // The sword's jewel, lit in its own colour while it fights.
        const FVector2f Gem(Pos.X+Ext.X*.5f,Pos.Y+25.f);
        const FLinearColor Jewel=bOn?Tint:Lerp(Tint,K.TextLightDim,.55f);
        P.Glow(Gem,FVector2f(23.f,23.f)*(bOn?1.f+.15f*Breath:.8f),WithAlpha(Jewel,bOn?.55f:.16f));P.Next();
        P.Ring(Gem,12.f,2.5f,WithAlpha(K.GoldBright,bOn?.95f:.38f),WithAlpha(K.Gold,bOn?.95f:.38f));
        P.Glow(Gem,FVector2f(8.f,8.f),WithAlpha(FLinearColor::White,bOn?.9f:.22f),WithAlpha(Jewel,bOn?.9f:.3f));
        P.Next();
        P.Text(Name,Font(EFont::Body,19.f),FVector2f(Pos.X+Ext.X*.5f,Pos.Y+45.f),bOn?K.TextLight:K.TextLightDim,FVector2f(.5f,0.f),WithAlpha(K.InkBlack,.6f));
        FString State;FLinearColor StateColor=K.TextGold;
        if(!bOn){State=TEXT("点击召出");StateColor=WithAlpha(K.TextLightDim,.75f+.25f*Hv);}
        else if(!bHere)State=FString(TEXT("召唤中"))+FString::ChrN(1+int32(Now*3.0)%3,TEXT('·'));
        else{const double Dealt=Damage.Get(0.0);State=Dealt>0?TEXT("伤害 ")+SwordRangeDamage(Dealt):TEXT("出战中");}
        P.Text(State,Font(EFont::Small,15.f),FVector2f(Pos.X+Ext.X*.5f,Pos.Y+69.f),StateColor,FVector2f(.5f,0.f));
        if(Hv>.01f){P.Next();P.Rounded(Pos+FVector2f(2.f,2.f),Ext-FVector2f(4.f,4.f),WithAlpha(FLinearColor::White,.06f*Hv),10.f);}
        if(Fl>.01f){P.Next();P.Halo(Pos,Ext,12.f,4.f+14.f*(1.f-Fl),WithAlpha(Tint,.6f*Fl));}
        return P.Layer+2;
    }

private:
    ELiveSpecialSword Kind=ELiveSpecialSword::None;
    TAttribute<bool> Active,Arrived;
    TAttribute<double> Damage;
    FString Name;
    FLinearColor Tint=FLinearColor::White;
};


/** A codex entry in the left column: jewel, name and how it is unlocked; the open one sits on a lacquer plate in its colour. */
class SLiveCodexEntry : public SInkPressable
{
public:
    SLATE_BEGIN_ARGS(SLiveCodexEntry):_Kind(ELiveSpecialSword::None){}
        SLATE_ARGUMENT(ELiveSpecialSword,Kind)
        SLATE_ATTRIBUTE(bool,Selected)
        SLATE_EVENT(FSimpleDelegate,OnClicked)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs)
    {
        Kind=InArgs._Kind;Selected=InArgs._Selected;Name=CodexName(Kind).ToString();Tint=CodexTint(Kind);
        Sub=Kind==ELiveSpecialSword::None?FString(TEXT("参照 · 基础剑灵")):FString::Printf(TEXT("赠送「%s」"),*LiveSpecialSwords::GiftName(Kind));
        InitPressable(FName(*(TEXT("SwordCodex.")+CodexKey(Kind))),InArgs._OnClicked,true);
    }
    virtual FVector2D ComputeDesiredSize(float)const override{return FVector2D(CodexListWidth,74.f);}
    virtual int32 OnPaint(const FPaintArgs& Args,const FGeometry& Geometry,const FSlateRect& Culling,FSlateWindowElementList& Out,int32 Layer,const FWidgetStyle& Style,bool bParentEnabled)const override
    {
        FPainter P(Geometry,Out,Layer,Style);
        const auto& K=Colors();
        const float Pr=PressAmount(),Hv=HoverAmount();const bool bOn=Selected.Get(false);
        const FVector2f Ext=P.Size()*(1.f-.03f*Pr),Pos=(P.Size()-Ext)*.5f;
        if(bOn){P.Halo(Pos,Ext,10.f,8.f,WithAlpha(Tint,.32f));P.Next();P.Panel(Pos,Ext,EPanel::Lacquer,10.f,true);}
        else P.Rounded(Pos,Ext,WithAlpha(K.InkBlack,.34f+.16f*Hv),10.f,WithAlpha(K.GoldLine,.22f+.4f*Hv),1.2f);
        P.Next();
        const FVector2f Gem(Pos.X+31.f,Pos.Y+Ext.Y*.5f);
        P.Glow(Gem,FVector2f(20.f,20.f),WithAlpha(Tint,bOn?.6f:.22f));P.Next();
        P.Ring(Gem,11.f,2.5f,WithAlpha(K.GoldBright,bOn?.95f:.45f),WithAlpha(K.Gold,bOn?.95f:.45f));
        P.Glow(Gem,FVector2f(7.f,7.f),WithAlpha(FLinearColor::White,bOn?.9f:.35f),WithAlpha(Tint,bOn?.9f:.4f));P.Next();
        P.Text(Name,Font(EFont::Body,21.f),FVector2f(Pos.X+56.f,Pos.Y+13.f),bOn?K.TextLight:Lerp(K.TextLightDim,K.TextLight,Hv),FVector2f(0.f,0.f),WithAlpha(K.InkBlack,.6f));
        P.Text(Sub,Font(EFont::Small,15.f),FVector2f(Pos.X+56.f,Pos.Y+42.f),bOn?K.TextGold:WithAlpha(K.TextLightDim,.8f),FVector2f(0.f,0.f));
        return P.Layer+1;
    }

private:
    ELiveSpecialSword Kind=ELiveSpecialSword::None;
    TAttribute<bool> Selected;
    FString Name,Sub;
    FLinearColor Tint=FLinearColor::White;
};

/** Round chevron button stepping the codex realm. */
class SLiveCodexArrow : public SInkPressable
{
public:
    SLATE_BEGIN_ARGS(SLiveCodexArrow):_Direction(1),_TestId(NAME_None),_Enabled(true){}
        SLATE_ARGUMENT(int32,Direction)
        SLATE_ARGUMENT(FName,TestId)
        SLATE_ATTRIBUTE(bool,Enabled)
        SLATE_EVENT(FSimpleDelegate,OnClicked)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs){Direction=InArgs._Direction>=0?1:-1;InitPressable(InArgs._TestId,InArgs._OnClicked,InArgs._Enabled);}
    virtual FVector2D ComputeDesiredSize(float)const override{return FVector2D(44.f,44.f);}
    virtual int32 OnPaint(const FPaintArgs& Args,const FGeometry& Geometry,const FSlateRect& Culling,FSlateWindowElementList& Out,int32 Layer,const FWidgetStyle& Style,bool bParentEnabled)const override
    {
        FPainter P(Geometry,Out,Layer,Style);
        const auto& K=Colors();const bool bOn=IsInteractive();const float Hv=HoverAmount(),Pr=PressAmount();
        const FVector2f C=P.Size()*.5f;const float R=P.Size().GetMin()*.5f*(1.f-.06f*Pr);
        P.Glow(C,FVector2f(R,R),WithAlpha(K.InkBlack,.75f),WithAlpha(K.InkBlack,.75f));P.Next();
        P.Ring(C,R-1.5f,1.5f,WithAlpha(K.GoldBright,bOn?.55f+.4f*Hv:.2f),WithAlpha(K.Gold,bOn?.55f+.4f*Hv:.2f));P.Next();
        const float D=float(Direction);const FLinearColor Ink=WithAlpha(K.GoldBright,bOn?.9f:.25f);
        P.Line(C+FVector2f(-4.f*D,-8.f),C+FVector2f(5.f*D,0.f),Ink,3.f);P.Line(C+FVector2f(5.f*D,0.f),C+FVector2f(-4.f*D,8.f),Ink,3.f);
        return P.Layer+1;
    }

private:
    int32 Direction=1;
};

/** Dark veil behind the codex; clicking it closes the codex. */
class SLiveCodexVeil : public SInkPressable
{
public:
    SLATE_BEGIN_ARGS(SLiveCodexVeil){}
        SLATE_EVENT(FSimpleDelegate,OnClicked)
    SLATE_END_ARGS()
    void Construct(const FArguments& InArgs){InitPressable(TEXT("SwordCodex.Veil"),InArgs._OnClicked,true);}
    virtual FVector2D ComputeDesiredSize(float)const override{return FVector2D(8.f,8.f);}
    virtual FCursorReply OnCursorQuery(const FGeometry&,const FPointerEvent&)const override{return FCursorReply::Cursor(EMouseCursor::Default);}
    virtual int32 OnPaint(const FPaintArgs& Args,const FGeometry& Geometry,const FSlateRect& Culling,FSlateWindowElementList& Out,int32 Layer,const FWidgetStyle& Style,bool bParentEnabled)const override
    {
        FPainter P(Geometry,Out,Layer,Style);
        P.Rounded(FVector2f::ZeroVector,P.Size(),WithAlpha(Colors().InkBlack,.58f),0.f);
        return P.Layer+1;
    }
};

/**
 * 剑谱: the five special sword spirits (and the normal spirit they are measured against) with their attribute sheet at
 * the sword's current realm, which the header arrows change (the sword then fights at that realm; the normal spirit page
 * only views), an attack comparison across the forms, the live values of a sword on the field and every skill.
 * Side by side when the screen is wide, stacked and scrolling in portrait.
 * Control ids: SwordCodex.<Kind|Normal>, SwordCodex.RealmPrev, SwordCodex.RealmNext, SwordCodex.Close, SwordCodex.Veil.
 */
class SLiveSwordCodex : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SLiveSwordCodex){}
        SLATE_ARGUMENT(TWeakObjectPtr<ALiveSwordTestGameMode>,Mode)
        SLATE_EVENT(FSimpleDelegate,OnClose)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs)
    {
        Mode=InArgs._Mode;OnClose=InArgs._OnClose;
        ChildSlot
        [
            SNew(SOverlay)
            +SOverlay::Slot()[SNew(SLiveCodexVeil).OnClicked_Lambda([this]{OnClose.ExecuteIfBound();})]
            +SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center).Padding(12.f)[SAssignNew(Frame,SBox)]
        ];
    }

    virtual void Tick(const FGeometry& AllottedGeometry,const double InCurrentTime,const float InDeltaTime)override
    {
        SCompoundWidget::Tick(AllottedGeometry,InCurrentTime,InDeltaTime);
        // Lay out for the space the screen gives: side by side when the card fits, otherwise stacked.
        const FVector2f Space=FVector2f(AllottedGeometry.GetLocalSize());
        const bool bWantPortrait=Space.X<CodexWidth+40.f;
        const FVector2f Card=bWantPortrait?FVector2f(FMath::Min(696.f,Space.X-24.f),FMath::Min(1180.f,Space.Y-40.f)):FVector2f(CodexWidth,FMath::Min(CodexHeight,Space.Y-24.f));
        if(bBuilt&&bWantPortrait==bPortrait&&Card.Equals(CardSize,1.f))
        {
            // The dock's realm arrows move the swords too: the open page follows.
            if(Realm()!=BuiltRealm)Rebuild();
            return;
        }
        bBuilt=true;bPortrait=bWantPortrait;CardSize=Card;BuildFrame();
    }

    void Select(ELiveSpecialSword Kind){if(Selected!=Kind){Selected=Kind;Rebuild();}}
    ELiveSpecialSword GetSelected()const{return Selected;}
    /** The realm the page shows: the selected sword's fighting realm, or the viewed realm on the normal spirit page. */
    int32 Realm()const
    {
        const ALiveSwordTestGameMode* M=Mode.Get();
        return Selected!=ELiveSpecialSword::None&&M?M->GetSwordRealm(Selected):ViewRealm;
    }

private:
    TWeakObjectPtr<ALiveSwordTestGameMode> Mode;
    FSimpleDelegate OnClose;
    ELiveSpecialSword Selected=ELiveSpecialSword::SoulEater;
    int32 ViewRealm=0,BuiltRealm=-1;
    bool bBuilt=false,bPortrait=false;
    FVector2f CardSize=FVector2f::ZeroVector;
    /** Where the painted divider under the header and the list's right edge (landscape) sit, in card space. */
    float DividerY=78.f;
    TSharedPtr<SBox> Frame,Detail;

    void StepRealm(int32 Direction)
    {
        const TArray<int32>& Stops=LiveSwordCodex::MajorStops();
        const int32 From=Realm();int32 Next=From;
        if(Direction>0){for(const int32 S:Stops)if(S>From){Next=S;break;}}
        else for(int32 I=Stops.Num()-1;I>=0;--I)if(Stops[I]<From){Next=Stops[I];break;}
        if(Next==From)return;
        if(ALiveSwordTestGameMode* M=Mode.Get();M&&Selected!=ELiveSpecialSword::None)M->SetSwordRealm(Selected,Next);else ViewRealm=Next;
        Rebuild();
    }
    void Rebuild(){BuiltRealm=Realm();if(Detail)Detail->SetContent(BuildDetail());}

    TSharedRef<SWidget> RealmStepper()
    {
        const auto& K=Colors();
        return SNew(SHorizontalBox)
            +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f,0.f,10.f,0.f)
            [MakeText(TAttribute<FText>::CreateLambda([this]{return FText::FromString(Selected==ELiveSpecialSword::None?TEXT("查看境界"):TEXT("剑灵境界"));}),EFont::Small,K.TextLightDim,18.f)]
            +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
            [SNew(SLiveCodexArrow).Direction(-1).TestId(TEXT("SwordCodex.RealmPrev")).Enabled_Lambda([this]{return Realm()>0;}).OnClicked_Lambda([this]{StepRealm(-1);})]
            +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
            [
                SNew(SBox).WidthOverride(132.f).HAlign(HAlign_Center)
                [MakeText(TAttribute<FText>::CreateLambda([this]{return LiveSwordCodex::RealmName(Realm());}),EFont::Body,K.TextGold,22.f,0.f,false,ETextJustify::Center)]
            ]
            +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
            [SNew(SLiveCodexArrow).Direction(1).TestId(TEXT("SwordCodex.RealmNext")).Enabled_Lambda([this]{return Realm()<33;}).OnClicked_Lambda([this]{StepRealm(1);})];
    }

    void BuildFrame()
    {
        const auto& K=Colors();
        const TSharedRef<SWidget> Title=SNew(SHorizontalBox)
            +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[MakeText(FText::FromString(TEXT("剑谱")),EFont::Section,K.GoldBright,34.f,1.f)]
            +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(16.f,6.f,0.f,0.f)[MakeText(FText::FromString(TEXT("特殊剑灵 · 技能与属性")),EFont::Small,K.TextLightDim,19.f)];
        const TSharedRef<SWidget> Close=SNew(SBox).WidthOverride(54.f).HeightOverride(54.f)[SNew(SInkCloseButton).TestId(TEXT("SwordCodex.Close")).OnClicked_Lambda([this]{OnClose.ExecuteIfBound();})];
        const auto Entry=[this](ELiveSpecialSword Kind)
        {
            return SNew(SLiveCodexEntry).Kind(Kind).Selected_Lambda([this,Kind]{return Selected==Kind;}).OnClicked_Lambda([this,Kind]{Select(Kind);});
        };
        TSharedRef<SVerticalBox> Column=SNew(SVerticalBox);
        if(bPortrait)
        {
            // Title and close, the realm stepper, the six entries three to a row, then the page scrolling below.
            DividerY=186.f;
            Column->AddSlot().AutoHeight()
            [
                SNew(SHorizontalBox)
                +SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)[Title]
                +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Close]
            ];
            Column->AddSlot().AutoHeight().HAlign(HAlign_Center).Padding(0.f,2.f,0.f,0.f)[RealmStepper()];
            const TArray<ELiveSpecialSword> Kinds=CodexKinds();
            for(int32 Row=0;Row*3<Kinds.Num();++Row)
            {
                const TSharedRef<SHorizontalBox> Line=SNew(SHorizontalBox);
                for(int32 I=Row*3;I<FMath::Min(Kinds.Num(),Row*3+3);++I)Line->AddSlot().AutoWidth().Padding(I%3?8.f:0.f,0.f,0.f,0.f)[Entry(Kinds[I])];
                Column->AddSlot().AutoHeight().HAlign(HAlign_Center).Padding(0.f,Row?8.f:26.f,0.f,0.f)[Line];
            }
            Column->AddSlot().FillHeight(1.f).Padding(0.f,18.f,0.f,0.f)
            [
                SNew(SScrollBox).ScrollBarThickness(FVector2D(4.f,4.f))
                +SScrollBox::Slot()[SAssignNew(Detail,SBox)]
            ];
        }
        else
        {
            DividerY=78.f;
            const TSharedRef<SVerticalBox> List=SNew(SVerticalBox);
            for(const ELiveSpecialSword Kind:CodexKinds())List->AddSlot().AutoHeight().Padding(0.f,0.f,0.f,8.f)[Entry(Kind)];
            Column->AddSlot().AutoHeight()
            [
                SNew(SHorizontalBox)
                +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Title]
                +SHorizontalBox::Slot().FillWidth(1.f)[SNullWidget::NullWidget]
                +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[RealmStepper()]
                +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(14.f,0.f,0.f,0.f)[Close]
            ];
            Column->AddSlot().FillHeight(1.f).Padding(0.f,14.f,0.f,0.f)
            [
                SNew(SHorizontalBox)
                +SHorizontalBox::Slot().AutoWidth()[List]
                +SHorizontalBox::Slot().FillWidth(1.f).Padding(26.f,0.f,10.f,0.f)[SAssignNew(Detail,SBox)]
            ];
        }
        Frame->SetWidthOverride(CardSize.X);Frame->SetHeightOverride(CardSize.Y);
        Frame->SetContent(SNew(SOverlay)
            +SOverlay::Slot()[SNew(SInkPaint).HitTest(true).OnPaint_Lambda([this](FPainter& P,const FGeometry&){PaintCard(P);})]
            +SOverlay::Slot().Padding(bPortrait?FMargin(18.f,14.f,14.f,18.f):FMargin(28.f,16.f,18.f,22.f))[Column]);
        Rebuild();
    }

    void PaintCard(FPainter& P)const
    {
        const auto& K=Colors();const FVector2f Size=P.Size();
        P.Halo(FVector2f::ZeroVector,Size,16.f,22.f,WithAlpha(K.InkBlack,.55f));P.Next();
        P.Panel(FVector2f::ZeroVector,Size,EPanel::Lacquer,16.f,true);P.Next();
        P.Glow(FVector2f(Size.X*.62f,DividerY+50.f),FVector2f(Size.X*.36f,150.f),WithAlpha(CodexTint(Selected),.16f));P.Next();
        P.Divider(FVector2f(Size.X*.5f,DividerY),Size.X-80.f,WithAlpha(K.Gold,.5f));
        if(bPortrait)P.Divider(FVector2f(Size.X*.5f,DividerY+176.f),Size.X-80.f,WithAlpha(K.Gold,.35f));
        else P.Line(FVector2f(28.f+CodexListWidth+13.f,98.f),FVector2f(28.f+CodexListWidth+13.f,Size.Y-34.f),WithAlpha(K.GoldLine,.35f),1.f);
        P.Cloud(FVector2f(Size.X-44.f,Size.Y-30.f),.7f,true,WithAlpha(K.Gold,.28f));
    }

    static TSharedRef<SWidget> Cell(const FString& Text,float Width,FLinearColor Color,EFont Role=EFont::Data,float Size=19.f,ETextJustify::Type Justify=ETextJustify::Center)
    {
        return SNew(SBox).WidthOverride(Width).HAlign(Justify==ETextJustify::Left?HAlign_Left:HAlign_Center)[MakeText(FText::FromString(Text),Role,Color,Size,0.f,false,Justify)];
    }
    static TSharedRef<SWidget> Label(const TCHAR* Text,FLinearColor Color)
    {
        return MakeText(FText::FromString(Text),EFont::Section,Color,23.f);
    }
    static TSharedRef<SWidget> Plaque(const FString& Text,bool bPassive)
    {
        return SNew(SInkPaint).DesiredSize(FVector2D(92.f,30.f)).OnPaint_Lambda([Text,bPassive](FPainter& P,const FGeometry&)
        {
            const auto& K=Colors();
            P.Skin(bPassive?Art::ESkin::PlaqueJade:Art::ESkin::PlaqueGold,FVector2f(0.f,0.f),P.Size());P.Next();P.Next();
            P.Text(Text,Font(EFont::Tiny,16.f),P.Size()*.5f,K.TextDark,FVector2f(.5f,.52f));
        });
    }

    TSharedRef<SWidget> BuildDetail()const
    {
        const auto& K=Colors();
        const ELiveSpecialSword Kind=Selected;const FLinearColor Accent=CodexTint(Kind);
        const int32 At=Realm();
        const TSharedRef<SVerticalBox> Box=SNew(SVerticalBox);
        // Name, motto and how the form is unlocked.
        const TSharedRef<SWidget> Unlock=MakeText(FText::FromString(Kind==ELiveSpecialSword::None?FString(TEXT("观众加入即拥有")):FString::Printf(TEXT("解锁：赠送「%s」"),*LiveSpecialSwords::GiftName(Kind))),EFont::Small,K.TextGold,18.f);
        Box->AddSlot().AutoHeight()
        [
            SNew(SHorizontalBox)
            +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Bottom)[MakeText(CodexName(Kind),EFont::Title,Lerp(Accent,K.TextLight,.2f),46.f,1.f)]
            +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Bottom).Padding(16.f,0.f,0.f,9.f)[MakeText(LiveSpecialSwords::Motto(Kind),EFont::Body,K.TextLightDim,20.f)]
            +SHorizontalBox::Slot().FillWidth(1.f)[SNullWidget::NullWidget]
            +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[bPortrait?SNullWidget::NullWidget:Unlock]
        ];
        if(bPortrait)Box->AddSlot().AutoHeight().Padding(0.f,2.f,0.f,0.f)[Unlock];
        Box->AddSlot().AutoHeight().Padding(0.f,2.f,0.f,12.f)[MakeText(LiveSwordCodex::Role(Kind),EFont::Small,Lerp(Accent,K.TextLight,.5f),18.f,0.f,true)];

        // Attribute sheet: 基础 at 练气一阶, 成长 per realm, and the value at the sword's realm (picked in the header).
        const TSharedRef<SVerticalBox> Sheet=SNew(SVerticalBox);
        Sheet->AddSlot().AutoHeight().Padding(0.f,0.f,0.f,6.f)
        [
            SNew(SHorizontalBox)
            +SHorizontalBox::Slot().AutoWidth()[Label(TEXT("属性"),K.GoldBright)]
            +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Bottom).Padding(10.f,0.f,0.f,2.f)
            [MakeText(FText::FromString(Kind==ELiveSpecialSword::None?TEXT("基础为练气一阶，成长为每升一个境界")
                :TEXT("末列为当前境界；顶部箭头调整这把剑的境界")),EFont::Small,K.TextLightDim,15.f)]
        ];
        const auto Row=[](const FString& A,const FString& B,const FString& C,const FString& D,FLinearColor Name,FLinearColor Value,EFont Role)
        {
            return SNew(SHorizontalBox)
                +SHorizontalBox::Slot().AutoWidth()[Cell(A,96.f,Name,EFont::Body,19.f,ETextJustify::Left)]
                +SHorizontalBox::Slot().AutoWidth()[Cell(B,96.f,Value,Role)]
                +SHorizontalBox::Slot().AutoWidth()[Cell(C,112.f,Value,Role)]
                +SHorizontalBox::Slot().AutoWidth()[Cell(D,124.f,Value,Role)];
        };
        Sheet->AddSlot().AutoHeight().Padding(0.f,2.f)[Row(TEXT("属性"),TEXT("基础"),TEXT("成长/境界"),LiveSwordCodex::RealmName(At).ToString(),K.TextLightDim,K.TextLightDim,EFont::Small)];
        for(const LiveSwordCodex::FStatLine& L:LiveSwordCodex::StatLines(Kind,At))
            Sheet->AddSlot().AutoHeight().Padding(0.f,3.f)[Row(L.Label.ToString(),L.Base,L.Growth,L.At,K.TextLight,K.TextGold,EFont::Data)];
        Sheet->AddSlot().AutoHeight().Padding(0.f,12.f,0.f,4.f)
        [
            SNew(SHorizontalBox)
            +SHorizontalBox::Slot().AutoWidth()[Label(TEXT("同境界攻击"),K.GoldBright)]
            +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Bottom).Padding(10.f,0.f,0.f,2.f)[MakeText(FText::FromString(TEXT("观众剑灵攻击 × 本剑攻击 / 普通剑灵攻击")),EFont::Small,K.TextLightDim,15.f)]
        ];
        Sheet->AddSlot().AutoHeight()[SNew(SInkPaint).DesiredSize(FVector2D(428.f,6*25.f+4.f)).OnPaint_Lambda([this](FPainter& P,const FGeometry&){PaintCompare(P);})];
        Sheet->AddSlot().AutoHeight().Padding(0.f,10.f,0.f,0.f)
        [
            MakeText(TAttribute<FText>::CreateLambda([this]{return LiveLine();}),EFont::Small,Lerp(Accent,K.TextLight,.45f),16.f,0.f,true)
        ];

        // Skills, in the order the form lists them.
        const TSharedRef<SVerticalBox> Skills=SNew(SVerticalBox);
        Skills->AddSlot().AutoHeight().Padding(0.f,0.f,0.f,8.f)[Label(TEXT("技能"),K.GoldBright)];
        for(const auto& Skill:LiveSpecialSwords::Skills(Kind))
            Skills->AddSlot().AutoHeight().Padding(0.f,0.f,0.f,12.f)
            [
                SNew(SHorizontalBox)
                +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top).Padding(0.f,3.f,12.f,0.f)
                [Plaque(Skill.bPassive?FString(TEXT("被动")):FString::Printf(TEXT("冷却 %.0f秒"),Skill.Cooldown),Skill.bPassive)]
                +SHorizontalBox::Slot().FillWidth(1.f)
                [
                    SNew(SVerticalBox)
                    +SVerticalBox::Slot().AutoHeight()[MakeText(Skill.Name,EFont::Body,K.TextLight,21.f)]
                    +SVerticalBox::Slot().AutoHeight().Padding(0.f,2.f,0.f,0.f)[MakeText(Skill.Description,EFont::Small,K.TextLightDim,16.f,0.f,true)]
                ]
            ];
        Skills->AddSlot().AutoHeight().Padding(0.f,2.f,0.f,0.f)
        [
            MakeText(FText::FromString(Kind==ELiveSpecialSword::None
                ?TEXT("普通剑灵没有特殊技能：按武器（剑/锤/弓/盾）出手，连击与观众技能照常结算。")
                :TEXT("技能就绪时随机释放其一；释放后 50% 普通攻击、50% 继续释放冷却已好的技能。")),EFont::Small,WithAlpha(K.TextLightDim,.85f),15.f,0.f,true)
        ];
        const TSharedRef<SWidget> Formula=MakeText(FText::FromString(TEXT("伤害 = A²/(A+D) × 招式系数 × (1+增伤) × 会心 × 境界系数；D 计入破甲（上限 60%），总减伤上限 60%，真实伤害不受防御影响。")),
            EFont::Small,WithAlpha(K.TextGold,.8f),15.f,0.f,true);

        if(bPortrait)
        {
            Box->AddSlot().AutoHeight()[Sheet];
            Box->AddSlot().AutoHeight().Padding(0.f,18.f,0.f,0.f)[Skills];
            Box->AddSlot().AutoHeight().Padding(0.f,10.f,0.f,8.f)[Formula];
            return Box;
        }
        Skills->AddSlot().FillHeight(1.f)[SNullWidget::NullWidget];
        Skills->AddSlot().AutoHeight()[Formula];
        Box->AddSlot().FillHeight(1.f)
        [
            SNew(SHorizontalBox)
            +SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(436.f)[Sheet]]
            +SHorizontalBox::Slot().FillWidth(1.f).Padding(26.f,0.f,0.f,0.f)[Skills]
        ];
        return Box;
    }

    /** Bars of every form's attack at the viewed realm, the open one lit; the five rank 噬魂 lowest to 弑神 highest. */
    void PaintCompare(FPainter& P)const
    {
        const auto& K=Colors();const int32 At=Realm();
        const TArray<ELiveSpecialSword> Kinds=CodexKinds();
        float Max=1.f;for(const ELiveSpecialSword Kind:Kinds)Max=FMath::Max(Max,LiveSpecialSwords::AttackAt(Kind,At));
        const float LabelW=104.f,ValueW=98.f,BarW=P.Size().X-LabelW-ValueW-8.f;
        for(int32 I=0;I<Kinds.Num();++I)
        {
            const ELiveSpecialSword Kind=Kinds[I];const bool bOn=Kind==Selected;
            const float Y=2.f+I*25.f,Attack=LiveSpecialSwords::AttackAt(Kind,At);
            const FLinearColor Tint=CodexTint(Kind);
            P.Text(CodexName(Kind).ToString(),Font(EFont::Small,16.f),FVector2f(0.f,Y+2.f),bOn?K.TextLight:K.TextLightDim);
            P.Rounded(FVector2f(LabelW,Y+6.f),FVector2f(BarW,11.f),WithAlpha(K.InkBlack,.55f),5.5f);P.Next();
            const float W=FMath::Max(11.f,BarW*Attack/Max);
            P.HGradient(FVector2f(LabelW,Y+6.f),FVector2f(W,11.f),{{0.f,WithAlpha(Tint,bOn?.55f:.28f)},{1.f,WithAlpha(Tint,bOn?1.f:.5f)}},5.5f);P.Next();
            if(bOn)P.Halo(FVector2f(LabelW,Y+6.f),FVector2f(W,11.f),5.5f,6.f,WithAlpha(Tint,.35f));
            P.Text(FString::Printf(TEXT("%.1f  ×%.2f"),Attack,LiveSpecialSwords::AttackScale(Kind,At)),Font(EFont::Data,15.f),FVector2f(P.Size().X,Y+3.f),bOn?K.TextGold:K.TextLightDim,FVector2f(1.f,0.f));
            P.Next();
        }
    }

    /** The open form's sword on the field right now, or how to bring one out. */
    FText LiveLine()const
    {
        const ALiveSwordTestGameMode* M=Mode.Get();
        if(Selected==ELiveSpecialSword::None)return FText::FromString(TEXT("观众剑灵的实际攻击 = 观众修为攻击（含礼物、功法与主播境界加成）。"));
        const ALiveSpecialSword* Sword=M?M->FindSpecialSword(Selected):nullptr;
        if(!Sword)return FText::FromString(TEXT("未出战 · 在试剑台召出后，这里显示它此刻的实际属性与伤害。"));
        const FString Bonus=Sword->GetDamageBonus()>0?FString::Printf(TEXT(" · 增伤 %.0f%%"),Sword->GetDamageBonus()*100.f):FString();
        // 噬魂 shows what its devours have taken so far: attack (with the health turned into attack), defense and health.
        const FLiveSpecialSwordGrowth Growth=M->GetSwordGrowth(Selected);
        const FString Devoured=Selected==ELiveSpecialSword::SoulEater?FString::Printf(TEXT(" · 吞噬 %d 次：攻 +%s 防 +%s 血 +%s"),Growth.Devoured,
            *SwordRangeStat(Growth.DevouredAttack),*SwordRangeStat(Growth.DevouredDefense),*SwordRangeStat(Growth.DevouredHealth)):FString();
        return FText::FromString(FString::Printf(TEXT("出战中 · %s · 攻击 %s · 会心 %.1f%% · 会心伤害 %.0f%% · 破甲 %.1f%% · 出手 %.2f秒%s%s · 累计伤害 %s"),
            *LiveSwordCodex::RealmName(Sword->GetRealm()).ToString(),*SwordRangeStat(Sword->GetAttackPower()),Sword->GetCriticalChance()*100.f,
            Sword->GetCriticalDamage()*100.f,Sword->GetArmorPenetration()*100.f,Sword->GetAttackInterval(),*Bonus,*Devoured,*LiveSwordCodex::Amount(M->GetSpecialSwordDamage(Selected))));
    }
};

/**
 * Bottom dock: header (title, swords out, call all, dismiss all, fold) over the five chips; the realm all five swords fight
 * at (剑谱 beside it); the nearest scarecrow with 残血 and 复原; the 渡劫 row with its outcome switch and status. Ink plates
 * over the scarecrows show their realm attributes; the codex opens over everything.
 */
class SLiveSwordTestPanel : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SLiveSwordTestPanel){}
        SLATE_ARGUMENT(TWeakObjectPtr<ALiveSwordTestGameMode>,Mode)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs)
    {
        Mode=InArgs._Mode;
        const auto& K=Colors();
        const TWeakObjectPtr<ALiveSwordTestGameMode> Weak=Mode;
        const TSharedRef<SHorizontalBox> Chips=SNew(SHorizontalBox);
        for(const ELiveSpecialSword Kind:LiveSpecialSwords::All())
            Chips->AddSlot().AutoWidth().Padding(4.f,0.f)
            [
                SNew(SLiveSwordChip).Kind(Kind)
                    .Active_Lambda([Weak,Kind]{const auto* M=Weak.Get();return M&&M->HasSpecialSword(Kind);})
                    .Arrived_Lambda([Weak,Kind]{const auto* M=Weak.Get();return M&&M->FindSpecialSword(Kind)!=nullptr;})
                    .Damage_Lambda([Weak,Kind]{const auto* M=Weak.Get();return M?M->GetSpecialSwordDamage(Kind):0.0;})
                    .OnClicked_Lambda([Weak,Kind]{if(auto* M=Weak.Get())M->SetSpecialSword(Kind,!M->HasSpecialSword(Kind));})
            ];
        const auto Button=[](const TCHAR* Label,EButtonSkin Skin,const TCHAR* Id,float Width,FSimpleDelegate OnClicked)
        {
            return SNew(SInkButton).Text(FText::FromString(Label)).Skin(Skin).Height(42.f).MinWidth(Width).FontSize(18.f).TestId(Id).OnClicked(OnClicked);
        };
        const auto Unfolded=[this]{return bFolded?EVisibility::Collapsed:EVisibility::Visible;};
        ChildSlot
        [
            SNew(SOverlay).Visibility(EVisibility::SelfHitTestInvisible)
            // Scarecrow plates under everything else; they never take clicks.
            +SOverlay::Slot()[SNew(SInkPaint).OnPaint_Lambda([this](FPainter& P,const FGeometry& G){PaintPlates(P,G);})]
            // Sits just above the HUD's key-hint strip.
            +SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Bottom).Padding(FMargin(12.f,0.f,12.f,66.f))
            [
                SNew(SBox).WidthOverride(SwordDockWidth)
                [
                    SNew(SOverlay)
                    +SOverlay::Slot()[SNew(SInkPaint).HitTest(true).OnPaint_Lambda([this](FPainter& P,const FGeometry& G){DockTop=G.GetAbsolutePosition().Y;PaintDock(P);})]
                    +SOverlay::Slot().Padding(FMargin(18.f,10.f,12.f,12.f))
                    [
                        SNew(SVerticalBox)
                        +SVerticalBox::Slot().AutoHeight()
                        [
                            SNew(SHorizontalBox)
                            +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[MakeText(FText::FromString(TEXT("试剑台")),EFont::Section,K.GoldBright,30.f,1.f)]
                            +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(14.f,4.f,0.f,0.f)
                            [MakeText(TAttribute<FText>::CreateLambda([this]{return FText::FromString(FString::Printf(TEXT("出战 %d / 5"),CountOut()));}),EFont::Small,K.TextLightDim,18.f)]
                            +SHorizontalBox::Slot().FillWidth(1.f)[SNullWidget::NullWidget]
                            +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(4.f,0.f)
                            [Button(TEXT("全部召出"),EButtonSkin::Jade,TEXT("SwordRange.All"),112.f,FSimpleDelegate::CreateLambda([Weak]{if(auto* M=Weak.Get())M->SetAllSpecialSwords(true);}))]
                            +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(4.f,0.f)
                            [Button(TEXT("全部收回"),EButtonSkin::Lacquer,TEXT("SwordRange.None"),112.f,FSimpleDelegate::CreateLambda([Weak]{if(auto* M=Weak.Get())M->SetAllSpecialSwords(false);}))]
                            +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(4.f,0.f,0.f,0.f)
                            [
                                SNew(SInkButton).Text_Lambda([this]{return FText::FromString(bFolded?TEXT("展开"):TEXT("收起"));}).Skin(EButtonSkin::Ghost)
                                    .Height(42.f).MinWidth(72.f).FontSize(18.f).TestId(TEXT("SwordRange.Fold")).OnClicked_Lambda([this]{bFolded=!bFolded;})
                            ]
                        ]
                        +SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.f,12.f,0.f,0.f)
                        [
                            SNew(SBox).Visibility_Lambda(Unfolded)[Chips]
                        ]
                        // The realm all five swords fight at: their attack, 会心, 破甲 and the suppression against a scarecrow follow it.
                        +SVerticalBox::Slot().AutoHeight().Padding(0.f,10.f,0.f,0.f)
                        [
                            SNew(SBox).Visibility_Lambda(Unfolded)
                            [
                                SNew(SHorizontalBox)
                                +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f,0.f,8.f,0.f)[MakeText(FText::FromString(TEXT("剑灵境界")),EFont::Small,K.TextLightDim,18.f)]
                                +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                                [SNew(SLiveCodexArrow).Direction(-1).TestId(TEXT("SwordRange.RealmPrev")).OnClicked_Lambda([Weak]{if(auto* M=Weak.Get())M->StepSwordRealms(-1);})]
                                +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                                [
                                    SNew(SBox).WidthOverride(124.f).HAlign(HAlign_Center)
                                    [
                                        MakeText(TAttribute<FText>::CreateLambda([Weak]
                                        {
                                            const auto* M=Weak.Get();const int32 R=M?M->GetCommonSwordRealm():0;
                                            return R<0?FText::FromString(TEXT("各自境界")):LiveSwordCodex::RealmName(R);
                                        }),EFont::Body,K.TextGold,21.f,0.f,false,ETextJustify::Center)
                                    ]
                                ]
                                +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                                [SNew(SLiveCodexArrow).Direction(1).TestId(TEXT("SwordRange.RealmNext")).OnClicked_Lambda([Weak]{if(auto* M=Weak.Get())M->StepSwordRealms(1);})]
                                +SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center).Padding(10.f,0.f,8.f,0.f)
                                [MakeText(TAttribute<FText>::CreateLambda([this]{return RealmHint();}),EFont::Small,WithAlpha(K.TextLightDim,.95f),15.f,0.f,true)]
                                +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                                [Button(TEXT("剑谱"),EButtonSkin::Gold,TEXT("SwordRange.Codex"),84.f,FSimpleDelegate::CreateLambda([this]{SetCodexOpen(!bCodexOpen);}))]
                            ]
                        ]
                        // The scarecrow nearest the cultivator: 残血 leaves it at 14% so 噬魂 can devour it, 复原 stands it up whole.
                        +SVerticalBox::Slot().AutoHeight().Padding(0.f,8.f,0.f,0.f)
                        [
                            SNew(SBox).Visibility_Lambda(Unfolded)
                            [
                                SNew(SHorizontalBox)
                                +SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
                                [MakeText(TAttribute<FText>::CreateLambda([this]{return TargetLine();}),EFont::Small,Hex(TEXT("EAD9B0")),16.f,0.f,true)]
                                +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8.f,0.f,0.f,0.f)
                                [Button(TEXT("残血"),EButtonSkin::Lacquer,TEXT("SwordRange.Low"),76.f,FSimpleDelegate::CreateLambda([Weak]{if(auto* M=Weak.Get())if(ALiveScarecrow* S=M->GetNearestScarecrow())S->SetHealthFraction(.14f);}))]
                                +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(6.f,0.f,0.f,0.f)
                                [Button(TEXT("复原"),EButtonSkin::Jade,TEXT("SwordRange.Heal"),76.f,FSimpleDelegate::CreateLambda([Weak]{if(auto* M=Weak.Get())if(ALiveScarecrow* S=M->GetNearestScarecrow())S->Restore();}))]
                            ]
                        ]
                        +SVerticalBox::Slot().AutoHeight().Padding(0.f,10.f,0.f,0.f)
                        [
                            SNew(SHorizontalBox)
                            +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top)
                            [
                                SNew(SInkButton).Skin(EButtonSkin::Cinnabar).Height(42.f).MinWidth(104.f).FontSize(18.f).TestId(TEXT("SwordRange.Trib"))
                                    .Text_Lambda([this]{return FText::FromString(TribLabel());})
                                    .Enabled_Lambda([this]{return CanPressTrib();})
                                    .OnClicked_Lambda([Weak]{if(auto* M=Weak.Get()){if(M->IsTribulationActive())M->AssistTribulation();else M->StartTribulationTest();}})
                            ]
                            // Random, always fail (to watch the backlash) or always succeed.
                            +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top).Padding(6.f,0.f,0.f,0.f)
                            [
                                SNew(SInkButton).Skin(EButtonSkin::Ghost).Height(42.f).MinWidth(100.f).FontSize(17.f).TestId(TEXT("SwordRange.TribRoll"))
                                    .Text_Lambda([Weak]
                                    {
                                        static const TCHAR* Labels[]={TEXT("结果·随机"),TEXT("结果·必败"),TEXT("结果·必成")};
                                        const auto* M=Weak.Get();return FText::FromString(Labels[M?FMath::Clamp(M->GetTribulationOutcome(),0,2):0]);
                                    })
                                    .OnClicked_Lambda([Weak]{if(auto* M=Weak.Get())M->SetTribulationOutcome((M->GetTribulationOutcome()+1)%3);})
                            ]
                            +SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center).Padding(10.f,0.f,4.f,0.f)
                            [
                                MakeText(TAttribute<FText>::CreateLambda([Weak]
                                {
                                    const auto* M=Weak.Get();const FText Status=M?M->GetTribulationStatus():FText();
                                    return Status.IsEmpty()?FText::FromString(TEXT("渡劫：天雷随成功率加剧，失败后天罚十息；七星剑须高出所渡天劫一个大境界才有五成挡劫 · 滚轮拉近镜头")):Status;
                                }),EFont::Small,Hex(TEXT("BFD4FF")),16.f,0.f,true)
                            ]
                        ]
                    ]
                ]
            ]
            +SOverlay::Slot()
            [
                SAssignNew(Codex,SLiveSwordCodex).Mode(Mode).OnClose_Lambda([this]{SetCodexOpen(false);})
                    .Visibility_Lambda([this]{return bCodexOpen?EVisibility::Visible:EVisibility::Collapsed;})
            ]
        ];
    }

    void SetCodexOpen(bool bOpen){bCodexOpen=bOpen;}
    bool IsCodexOpen()const{return bCodexOpen;}
    ELiveSpecialSword GetCodexSelection()const{return Codex?Codex->GetSelected():ELiveSpecialSword::None;}
    int32 GetCodexRealm()const{return Codex?Codex->Realm():0;}

private:
    TWeakObjectPtr<ALiveSwordTestGameMode> Mode;
    TSharedPtr<SLiveSwordCodex> Codex;
    bool bFolded=false,bCodexOpen=false;
    // Absolute top of the dock from its last paint: plates stop above it.
    mutable float DockTop=0.f;

    int32 CountOut()const
    {
        int32 Count=0;
        if(const auto* M=Mode.Get())for(const ELiveSpecialSword Kind:LiveSpecialSwords::All())Count+=M->HasSpecialSword(Kind)?1:0;
        return Count;
    }
    EXianTuTribState TribState()const
    {
        const ALiveSwordTestGameMode* M=Mode.Get();const UGameInstance* GI=M?M->GetGameInstance():nullptr;
        const UXianTuSubsystem* X=GI?GI->GetSubsystem<UXianTuSubsystem>():nullptr;
        return X?X->GetState()->Trib.State:EXianTuTribState::None;
    }
    static bool InTrial(EXianTuTribState S){return S==EXianTuTribState::Prepare||S==EXianTuTribState::Trial||S==EXianTuTribState::Frozen;}
    FString TribLabel()const
    {
        const ALiveSwordTestGameMode* M=Mode.Get();const EXianTuTribState S=TribState();
        return S==EXianTuTribState::Backlash?TEXT("天罚中"):InTrial(S)?TEXT("助力 +10"):M&&M->IsTribulationActive()?TEXT("劫云散去"):TEXT("渡劫");
    }
    // Starts a rehearsal when idle and sends helpers during the trial; waits out the backlash and a settled storm's fade.
    bool CanPressTrib()const
    {
        const ALiveSwordTestGameMode* M=Mode.Get();const EXianTuTribState S=TribState();
        return InTrial(S)||(M&&!M->IsTribulationActive());
    }
    /** How hard the nearest scarecrow's realm presses the swords (the core formula's realm factor). */
    FText RealmHint()const
    {
        const ALiveSwordTestGameMode* M=Mode.Get();if(!M)return FText::GetEmpty();
        const ALiveScarecrow* S=M->GetNearestScarecrow();const int32 R=M->GetCommonSwordRealm();
        if(S&&S->Realm>=0&&R>=0)
            return FText::FromString(FString::Printf(TEXT("对%s 压制 ×%.2f"),*LiveSwordCodex::RealmName(S->Realm).ToString(),LiveCombat::RealmFactor(R,S->Realm)));
        return FText::FromString(R<0?TEXT("剑谱中已分别调整"):TEXT("五剑同境 · 剑谱可单独调整"));
    }
    FText TargetLine()const
    {
        const ALiveSwordTestGameMode* M=Mode.Get();const ALiveScarecrow* S=M?M->GetNearestScarecrow():nullptr;
        if(!S)return FText::FromString(TEXT("附近没有稻草人"));
        const APlayerController* PC=M->GetWorld()->GetFirstPlayerController();const APawn* Pawn=PC?PC->GetPawn():nullptr;
        const float Meters=Pawn?float(FVector::Dist2D(Pawn->GetActorLocation(),S->GetActorLocation()))/100.f:0.f;
        return FText::FromString(FString::Printf(TEXT("目标 · %s · 生命 %.0f%% · 攻 %s · 防 %s · 相距 %.1f 米"),*ScarecrowTitle(S),S->GetHealthFraction()*100.f,
            *SwordRangeStat(S->GetCurrentAttack()),*SwordRangeStat(S->GetCurrentDefense()),Meters));
    }
    void PaintDock(FPainter& P)const
    {
        const auto& K=Colors();const FVector2f Size=P.Size();
        P.Panel(FVector2f::ZeroVector,Size,EPanel::Lacquer,14.f,true);P.Next();
        P.Cloud(FVector2f(Size.X-30.f,Size.Y-22.f),.55f,true,WithAlpha(K.Gold,.3f));
        if(!bFolded)P.Divider(FVector2f(Size.X*.5f,62.f),Size.X-90.f,WithAlpha(K.Gold,.55f));
    }
    /**
     * An ink plate over each scarecrow on screen: its realm, a health bar, attack and defense, the suppression the swords
     * suffer at their realm, and DPS with the running total.
     */
    void PaintPlates(FPainter& P,const FGeometry& G)const
    {
        const ALiveSwordTestGameMode* M=Mode.Get();if(!M||bCodexOpen)return;
        APlayerController* PC=M->GetWorld()?M->GetWorld()->GetFirstPlayerController():nullptr;if(!PC)return;
        const float Scale=FMath::Max(.01f,UWidgetLayoutLibrary::GetViewportScale(PC));
        const auto& K=Colors();const FVector2f Space=P.Size();const int32 SwordRealm=M->GetCommonSwordRealm();
        // A scarecrow down the screen (a room behind) would show under the dock and over the HUD's hint strip.
        const float Floor=DockTop>0.f?FMath::Min(Space.Y,float(G.AbsoluteToLocal(FVector2f(0.f,DockTop)).Y)):Space.Y;
        for(const ALiveScarecrow* S:M->GetScarecrows())
        {
            FVector2D Screen;
            if(!PC->ProjectWorldLocationToScreen(S->GetHealthBarLocation()+FVector(0,0,30),Screen,true))continue;
            const FVector2f Anchor=FVector2f(Screen/Scale),Ext(188.f,92.f),Pos(Anchor.X-Ext.X*.5f,Anchor.Y-Ext.Y);
            if(Pos.X>Space.X||Pos.Y+Ext.Y>Floor||Pos.X+Ext.X<0.f||Pos.Y+Ext.Y<0.f)continue;
            P.Rounded(Pos,Ext,WithAlpha(K.InkBlack,.74f),9.f,WithAlpha(K.GoldLine,.6f),1.2f);P.Next();
            P.Text(ScarecrowTitle(S),Font(EFont::Body,17.f),FVector2f(Anchor.X,Pos.Y+6.f),K.GoldBright,FVector2f(.5f,0.f),WithAlpha(K.InkBlack,.7f));
            const FVector2f BarPos(Pos.X+12.f,Pos.Y+31.f),BarExt(Ext.X-24.f,10.f);
            P.Rounded(BarPos,BarExt,WithAlpha(K.InkBlack,.85f),5.f,WithAlpha(K.GoldLine,.35f),1.f);P.Next();
            const float Fill=BarExt.X*S->GetHealthFraction();
            if(Fill>1.f){P.HGradient(BarPos,FVector2f(Fill,BarExt.Y),{{0.f,Hex(TEXT("7A1C14"))},{1.f,Hex(TEXT("D8492F"))}},5.f);P.Next();}
            P.Text(S->IsKnockedDown()?FString::Printf(TEXT("倒地 · 第 %d 次"),S->Knockdowns)
                :FString::Printf(TEXT("生命 %s / %s"),*SwordRangeStat(S->Health),*SwordRangeStat(S->GetCurrentMaxHealth())),
                Font(EFont::Data,13.f),FVector2f(Anchor.X,Pos.Y+44.f),K.TextLight,FVector2f(.5f,0.f));
            const float Factor=SwordRealm>=0?LiveCombat::RealmFactor(SwordRealm,S->Realm):1.f;
            P.Text(FString::Printf(TEXT("攻 %s · 防 %s · 压制 ×%.2f"),*SwordRangeStat(S->GetCurrentAttack()),*SwordRangeStat(S->GetCurrentDefense()),Factor),
                Font(EFont::Small,13.f),FVector2f(Anchor.X,Pos.Y+60.f),K.TextGold,FVector2f(.5f,0.f));
            P.Text(FString::Printf(TEXT("DPS %s · 总伤 %s"),*SwordRangeStat(S->GetDamagePerSecond()),*LiveSwordCodex::Amount(S->TotalDamage)),
                Font(EFont::Small,13.f),FVector2f(Anchor.X,Pos.Y+75.f),Hex(TEXT("BFD4FF")),FVector2f(.5f,0.f));
            P.Next();
        }
    }
};

TSharedRef<SWidget> ULiveSwordTestPanel::RebuildWidget()
{
    // The range has no game UI to load the painted skins first.
    Art::Preload();
    SAssignNew(Panel,SLiveSwordTestPanel).Mode(Mode);
    return Panel.ToSharedRef();
}

void ULiveSwordTestPanel::ReleaseSlateResources(bool bReleaseChildren)
{
    Super::ReleaseSlateResources(bReleaseChildren);
    Panel.Reset();
}

bool ULiveSwordTestPanel::ClickTestTarget(FName Id){return Testing::Click(Id);}
bool ULiveSwordTestPanel::IsCodexOpen()const{return Panel&&Panel->IsCodexOpen();}
ELiveSpecialSword ULiveSwordTestPanel::GetCodexSelection()const{return Panel?Panel->GetCodexSelection():ELiveSpecialSword::None;}
int32 ULiveSwordTestPanel::GetCodexRealm()const{return Panel?Panel->GetCodexRealm():0;}

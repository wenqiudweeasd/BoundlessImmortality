#include "InkUI/InkUIWidgets.h"
#include "Framework/Application/SlateApplication.h"

using namespace InkUI;

void SInkProgressBar::Construct(const FArguments& InArgs)
{
    Fraction=InArgs._Fraction;Label=InArgs._Label;Value=InArgs._Value;Style=InArgs._Style;
    Height=InArgs._Height;Width=InArgs._Width;FontSize=InArgs._FontSize;
    LastTarget=FMath::Clamp(Fraction.Get(0.f),0.f,1.f);
    Shown.Snap(LastTarget);Trail.Snap(LastTarget);
    SetCanTick(true);
    ForceVolatile(true);
}

void SInkProgressBar::Tick(const FGeometry& AllottedGeometry,const double InCurrentTime,const float InDeltaTime)
{
    const float Target=FMath::Clamp(Fraction.Get(0.f),0.f,1.f);
    if(!FMath::IsNearlyEqual(Target,LastTarget,.0005f))
    {
        // Losses hold a pale trail briefly, so damage reads as a chunk instead of a slide.
        if(Target<Shown.Value()){TrailHold=.3f;Trail.Snap(FMath::Max(Trail.Value(),Shown.Value()));}
        Shown.Retarget(Target,Durations::Number,EEase::OutCubic);
        LastTarget=Target;
    }
    Shown.Tick(InDeltaTime);
    if(TrailHold>0.f){TrailHold-=InDeltaTime;if(TrailHold<=0.f)Trail.Retarget(LastTarget,.5f,EEase::InOutCubic);}
    Trail.Tick(InDeltaTime);
    if(Trail.Value()<Shown.Value())Trail.Snap(Shown.Value());
}

int32 SInkProgressBar::OnPaint(const FPaintArgs& Args,const FGeometry& Geometry,const FSlateRect& Culling,FSlateWindowElementList& Out,int32 Layer,const FWidgetStyle& InStyle,bool bParentEnabled) const
{
    FPainter P(Geometry,Out,Layer,InStyle);
    const auto& K=Colors();
    const FVector2f Size=P.Size();
    const float F=FMath::Clamp(Shown.Value(),0.f,1.f),Tr=FMath::Clamp(Trail.Value(),0.f,1.f);
    const double Now=Motion::Now();
    FVector2f ChPos,ChExt;
    const bool bHud=Style==EBar::Health||Style==EBar::Qi;
    if(bHud)
    {
        P.Skin(Art::ESkin::HudBarTrack,FVector2f(0,0),Size);
        const float S=Size.Y/226.f;
        ChPos=FVector2f(168.f*S,54.f*S);ChExt=FVector2f(Size.X-334.f*S,Size.Y-106.f*S);
        P.Next();
        if(Tr>F)P.Rounded(ChPos+FVector2f(ChExt.X*F,0),FVector2f(ChExt.X*(Tr-F),ChExt.Y),WithAlpha(Hex(TEXT("FFF1D6")),.62f),1.f);
        const TCHAR* FillPath=Style==EBar::Health?Art::Paths::HealthFill:Art::Paths::QiFill;
        const FVector2f Src=Style==EBar::Health?FVector2f(1429,135):FVector2f(1436,143);
        if(const FSlateBrush* Fill=Art::Image(FillPath);Art::IsValid(Fill)&&F>.002f)
        {
            FSlateBrush Crop=*Fill;
            const float U0=10.f/Src.X,U1=(10.f+F*(Src.X-20.f))/Src.X;
            Crop.SetUVRegion(FBox2f(FVector2f(U0,10.f/Src.Y),FVector2f(U1,(Src.Y-10.f)/Src.Y)));
            P.Next();
            P.Brush(&Crop,ChPos,FVector2f(ChExt.X*F,ChExt.Y));
        }
    }
    else
    {
        P.Rounded(FVector2f(0,0),Size,WithAlpha(K.InkBlack,.6f),Size.Y*.5f,WithAlpha(K.GoldDeep,.75f),1.f);
        ChPos=FVector2f(2.f,2.f);ChExt=Size-FVector2f(4.f,4.f);
        P.Next();
        if(Tr>F)P.Rounded(ChPos+FVector2f(ChExt.X*F,0),FVector2f(ChExt.X*(Tr-F),ChExt.Y),WithAlpha(FLinearColor::White,.45f),ChExt.Y*.5f);
        if(F>.002f)
        {
            TArray<TPair<float,FLinearColor>> Stops;
            if(Style==EBar::Jade)Stops={{0.f,K.JadeBright},{.5f,K.Jade},{1.f,K.JadeDeep}};
            else if(Style==EBar::Red)Stops={{0.f,Hex(TEXT("F28A64"))},{.5f,K.CinnabarBright},{1.f,K.Cinnabar}};
            else Stops={{0.f,K.GoldBright},{.5f,K.Gold},{1.f,K.GoldDeep}};
            P.Next();
            P.VGradient(ChPos,FVector2f(FMath::Max(ChExt.Y,ChExt.X*F),ChExt.Y),Stops,ChExt.Y*.5f);
        }
    }
    if(F>.02f&&!Motion::IsReduced())
    {
        P.Next();
        const float Cycle=float(FMath::Fmod(Now*.5,1.4));
        P.Shine(ChPos,FVector2f(ChExt.X*F,ChExt.Y),Cycle,ChExt.Y*2.2f,WithAlpha(FLinearColor::White,.32f));
        P.Glow(ChPos+FVector2f(ChExt.X*F,ChExt.Y*.5f),FVector2f(ChExt.Y*.9f,ChExt.Y*.75f),WithAlpha(FLinearColor::White,.5f),FLinearColor::Transparent,16);
    }
    P.Next();
    const float TextSize=FontSize>0.f?FontSize:FMath::Clamp(Size.Y*(bHud?.62f:.78f),14.f,26.f);
    const FSlateFontInfo TextFont=Font(EFont::Data,TextSize,1.5f,WithAlpha(K.InkBlack,.95f));
    const FString L=Label.Get().ToString(),V=Value.Get().ToString();
    const float MidY=ChPos.Y+ChExt.Y*.5f;
    if(!L.IsEmpty())
    {
        P.Text(L,TextFont,FVector2f(ChPos.X+8.f,MidY),K.TextLight,FVector2f(0.f,.52f));
        if(!V.IsEmpty())P.Text(V,TextFont,FVector2f(ChPos.X+ChExt.X-8.f,MidY),K.TextLight,FVector2f(1.f,.52f));
    }
    else if(!V.IsEmpty())P.Text(V,TextFont,FVector2f(ChPos.X+ChExt.X*.5f,MidY),K.TextLight,FVector2f(.5f,.52f));
    return P.Layer+2;
}

void SInkItemSlot::Construct(const FArguments& InArgs)
{
    Icon=InArgs._Icon;Quality=InArgs._Quality;Count=InArgs._Count;SlotSize=InArgs._SlotSize;Selected=InArgs._Selected;
    bNew=InArgs._IsNew;bEmpty=InArgs._Empty;bLocked=InArgs._Locked;bShowName=InArgs._ShowName;Name=InArgs._Name;
    InitPressable(InArgs._TestId,InArgs._OnClicked,!bEmpty&&!bLocked);
}

void SInkItemSlot::PaintSlot(FPainter& P,FVector2f Pos,float Size,const FSlateBrush* InIcon,EQuality InQuality,int32 InCount,bool bSelected,bool bIsNew,bool bIsEmpty,bool bIsLocked,float HoverAmount,float InPhase)
{
    const auto& K=Colors();
    const FLinearColor Q=QualityColor(InQuality);
    const float R=Size*.12f;
    const FVector2f Ext(Size,Size);
    const double Now=Motion::Now();
    P.Halo(Pos,Ext,R,Size*.08f,WithAlpha(K.InkBlack,.32f));
    P.Next();
    if(bIsEmpty)
    {
        P.Rounded(Pos,Ext,WithAlpha(K.PaperShade,.32f),R,WithAlpha(K.PaperEdge,.55f),1.f);
        P.Next();
        P.Diamond(Pos+Ext*.5f,FVector2f(Size*.05f),WithAlpha(K.PaperEdge,.45f));
        if(bIsLocked)
        {
            const FVector2f C=Pos+Ext*.5f;
            P.Rounded(C-FVector2f(10.f,4.f),FVector2f(20.f,16.f),WithAlpha(K.TextDarkDim,.6f),3.f);
            P.Ring(C+FVector2f(0.f,-5.f),7.f,2.5f,WithAlpha(K.TextDarkDim,.6f),WithAlpha(K.TextDarkDim,.6f),PI,PI,12);
        }
        return;
    }
    P.VGradient(Pos,Ext,{{0.f,Hex(TEXT("2E3432"))},{1.f,Hex(TEXT("121615"))}},R);
    P.Next();
    P.Glow(Pos+FVector2f(Size*.5f,Size*.74f),FVector2f(Size*.6f,Size*.48f),WithAlpha(Q,.5f),FLinearColor::Transparent,28);
    P.Next();
    P.Rounded(Pos,Ext,FLinearColor::Transparent,R,WithAlpha(Q,.95f),2.f);
    P.Rounded(Pos+FVector2f(4.f,4.f),Ext-FVector2f(8.f,8.f),FLinearColor::Transparent,R*.7f,WithAlpha(K.GoldBright,.16f),1.f);
    if(InQuality>=EQuality::Epic)
    {
        const float D=Size*.035f+1.f,In=7.f;
        for(const FVector2f Corner:{FVector2f(In,In),FVector2f(Size-In,In),FVector2f(Size-In,Size-In),FVector2f(In,Size-In)})P.Diamond(Pos+Corner,FVector2f(D),Lerp(Q,K.GoldBright,.35f));
    }
    P.Next();
    P.BrushFit(InIcon,Pos+FVector2f(Size*.12f),FVector2f(Size*.76f));
    if(InQuality>=EQuality::Legendary&&!Motion::IsReduced())
    {
        P.Next();
        const float Cycle=float(FMath::Fmod(Now+InPhase,3.8))/1.3f;
        P.Shine(Pos+FVector2f(3.f),Ext-FVector2f(6.f),Cycle,Size*.32f,WithAlpha(FLinearColor::White,.26f));
    }
    P.Next();
    if(InCount>1)P.Text(FormatAmount(InCount).ToString(),Font(EFont::Data,FMath::Max(15.f,Size*.19f),1.5f,K.InkBlack),Pos+FVector2f(Size-7.f,Size-3.f),K.TextLight,FVector2f(1.f,1.f));
    if(bIsNew)
    {
        P.Rounded(Pos+FVector2f(3.f,3.f),FVector2f(30.f,22.f),K.Cinnabar,5.f,Hex(TEXT("FFD9A8"),.8f),1.f);
        P.Next();
        P.Text(TEXT("新"),Font(EFont::Tiny,16.f),Pos+FVector2f(18.f,14.f),Hex(TEXT("FFF3DE")),FVector2f(.5f,.5f));
    }
    if(HoverAmount>.01f)P.Rounded(Pos,Ext,WithAlpha(FLinearColor::White,.07f*HoverAmount),R);
    if(bSelected)
    {
        P.Next();
        const float Pulse=Motion::Pulse(Now,1.6f,InPhase);
        P.Halo(Pos,Ext,R,Size*.12f,WithAlpha(K.GoldBright,.5f+.3f*Pulse));
        P.Next();
        P.Rounded(Pos-FVector2f(2.f),Ext+FVector2f(4.f),FLinearColor::Transparent,R+2.f,K.GoldBright,2.5f);
    }
    if(bIsLocked)
    {
        P.Next();
        P.Rounded(Pos,Ext,WithAlpha(K.InkBlack,.62f),R);
    }
}

int32 SInkItemSlot::OnPaint(const FPaintArgs& Args,const FGeometry& Geometry,const FSlateRect& Culling,FSlateWindowElementList& Out,int32 Layer,const FWidgetStyle& Style,bool bParentEnabled) const
{
    FPainter P(Geometry,Out,Layer,Style);
    const float S=SlotSize*(1.f-.05f*PressAmount());
    const FVector2f Pos((P.Size().X-S)*.5f,(SlotSize-S)*.5f);
    PaintSlot(P,Pos,S,Icon,Quality,Count,Selected.Get(false),bNew,bEmpty,bLocked,HoverAmount(),Phase);
    if(bShowName&&!Name.IsEmpty())
    {
        P.Next();P.Next();
        P.Text(Name.ToString(),Font(EFont::Small,21.f),FVector2f(P.Size().X*.5f,SlotSize+6.f),Lerp(QualityColor(Quality),Colors().TextDark,.45f),FVector2f(.5f,0.f));
    }
    return P.Layer+3;
}

void SInkAvatar::Construct(const FArguments& InArgs)
{
    Portrait=InArgs._Portrait;Initial=InArgs._Initial;Diameter=InArgs._Diameter;Ring=InArgs._Ring;
}

void SInkAvatar::PaintAvatar(FPainter& P,FVector2f Center,float Diameter,const FSlateBrush* InPortrait,const FText& InInitial,int32 InRing)
{
    const auto& K=Colors();
    const float R=Diameter*.5f;
    P.Glow(Center,FVector2f(R*1.08f),WithAlpha(K.InkBlack,.5f));
    P.Next();
    P.Rounded(Center-FVector2f(R*.8f),FVector2f(R*1.6f),Hex(TEXT("1E2D2B")),R*.8f);
    P.Next();
    if(Art::IsValid(InPortrait))P.BrushFit(InPortrait,Center-FVector2f(R*.86f),FVector2f(R*1.72f));
    else
    {
        P.Glow(Center,FVector2f(R*.7f),WithAlpha(K.Jade,.45f));
        P.Next();
        P.Text(InInitial.ToString().Left(1),Font(EFont::Title,R*.95f),Center,K.TextGold,FVector2f(.5f,.56f),WithAlpha(K.InkBlack,.7f));
    }
    P.Next();P.Next();
    if(InRing==0&&Art::IsValid(Art::Image(Art::Paths::AvatarFrame)))
    {
        P.BrushFit(Art::Image(Art::Paths::AvatarFrame),Center-FVector2f(R),FVector2f(Diameter));
        return;
    }
    static const TCHAR* Metals[][2]={{TEXT("F6D67A"),TEXT("9A6A22")},{TEXT("F6D67A"),TEXT("9A6A22")},{TEXT("E7EDF2"),TEXT("7D8994")},{TEXT("E3A06A"),TEXT("7B4524")},{TEXT("D4AA62"),TEXT("6E5126")}};
    const int32 M=FMath::Clamp(InRing,0,4);
    P.Ring(Center,R*.86f,R*.14f,Hex(Metals[M][0]),Hex(Metals[M][1]),0.f,2.f*PI,48);
    P.Next();
    P.Ring(Center,R*.94f,1.5f,WithAlpha(Hex(Metals[M][0]),.8f),WithAlpha(Hex(Metals[M][0]),.8f),0.f,2.f*PI,48);
    for(int32 I=0;I<4;++I)P.Diamond(Center+FVector2f(FMath::Cos(I*HALF_PI),FMath::Sin(I*HALF_PI))*R*.86f,FVector2f(R*.09f),Hex(Metals[M][0]));
}

int32 SInkAvatar::OnPaint(const FPaintArgs& Args,const FGeometry& Geometry,const FSlateRect& Culling,FSlateWindowElementList& Out,int32 Layer,const FWidgetStyle& Style,bool bParentEnabled) const
{
    FPainter P(Geometry,Out,Layer,Style);
    PaintAvatar(P,P.Size()*.5f,FMath::Min(P.Size().X,P.Size().Y),Portrait,Initial,Ring);
    return P.Layer+2;
}

void SInkTabBar::Construct(const FArguments& InArgs)
{
    Tabs=InArgs._Tabs;Selected=FMath::Clamp(InArgs._Selected,0,FMath::Max(0,Tabs.Num()-1));Height=InArgs._Height;bOnDark=InArgs._OnDark;
    TestPrefix=InArgs._TestPrefix;Badges=InArgs._Badges;OnSelected=InArgs._OnSelected;
    Slide.Snap(float(Selected));
    SetCanTick(true);
    ForceVolatile(true);
    if(!TestPrefix.IsNone())Testing::Register(TestPrefix,SharedThis(this));
}

void SInkTabBar::Tick(const FGeometry& AllottedGeometry,const double InCurrentTime,const float InDeltaTime){Slide.Tick(InDeltaTime);}

int32 SInkTabBar::IndexAt(const FGeometry& Geometry,FVector2f Local) const
{
    const float W=Geometry.GetLocalSize().X/FMath::Max(1,Tabs.Num());
    return Local.Y<0.f||Local.Y>Geometry.GetLocalSize().Y?INDEX_NONE:FMath::Clamp(int32(Local.X/FMath::Max(1.f,W)),0,Tabs.Num()-1);
}

void SInkTabBar::Select(int32 Index,bool bNotify)
{
    if(!Tabs.IsValidIndex(Index)||Index==Selected)return;
    Selected=Index;
    Slide.Retarget(float(Index),Durations::Tab,EEase::OutCubic);
    if(bNotify)OnSelected.ExecuteIfBound(Index);
}

bool SInkTabBar::ClickTab(FName InTestPrefix,int32 Index)
{
    const TSharedPtr<SWidget> W=Testing::Find(InTestPrefix);
    if(!W||W->GetType()!=FName(TEXT("SInkTabBar")))return false;
    const TSharedPtr<SInkTabBar> Bar=StaticCastSharedPtr<SInkTabBar>(W);
    if(!Bar->Tabs.IsValidIndex(Index))return false;
    Bar->Select(Index,true);return true;
}

FReply SInkTabBar::OnMouseButtonDown(const FGeometry& Geometry,const FPointerEvent& Event)
{
    PressIndex=IndexAt(Geometry,FVector2f(Geometry.AbsoluteToLocal(Event.GetScreenSpacePosition())));
    return PressIndex==INDEX_NONE?FReply::Unhandled():FReply::Handled();
}

FReply SInkTabBar::OnMouseButtonUp(const FGeometry& Geometry,const FPointerEvent& Event)
{
    const int32 Index=IndexAt(Geometry,FVector2f(Geometry.AbsoluteToLocal(Event.GetScreenSpacePosition())));
    if(Index!=INDEX_NONE&&Index==PressIndex)Select(Index,true);
    PressIndex=INDEX_NONE;
    return FReply::Handled();
}

FReply SInkTabBar::OnMouseMove(const FGeometry& Geometry,const FPointerEvent& Event)
{
    HoverIndex=IndexAt(Geometry,FVector2f(Geometry.AbsoluteToLocal(Event.GetScreenSpacePosition())));
    return FReply::Unhandled();
}

FCursorReply SInkTabBar::OnCursorQuery(const FGeometry& Geometry,const FPointerEvent& Event) const{return FCursorReply::Cursor(EMouseCursor::Hand);}

int32 SInkTabBar::OnPaint(const FPaintArgs& Args,const FGeometry& Geometry,const FSlateRect& Culling,FSlateWindowElementList& Out,int32 Layer,const FWidgetStyle& Style,bool bParentEnabled) const
{
    FPainter P(Geometry,Out,Layer,Style);
    const auto& K=Colors();
    const FVector2f Size=P.Size();
    const int32 N=FMath::Max(1,Tabs.Num());
    const float W=Size.X/N;
    P.Skin(bOnDark?Art::ESkin::BarDark:Art::ESkin::PlaquePaper,FVector2f(0.f,Size.Y*.08f),FVector2f(Size.X,Size.Y*.84f),FLinearColor(1,1,1,bOnDark?.92f:.95f));
    P.Next();
    for(int32 I=1;I<N;++I)
    {
        const float X=W*I;
        P.FadeLine(FVector2f(X,Size.Y*.28f),FVector2f(X,Size.Y*.72f),WithAlpha(bOnDark?K.Gold:K.PaperEdge,.55f),1.f,.35f);
    }
    P.Next();
    const float SelX=Slide.Value()*W;
    P.Halo(FVector2f(SelX+8.f,4.f),FVector2f(W-16.f,Size.Y-8.f),10.f,12.f,WithAlpha(K.GoldBright,.25f));
    P.Next();
    P.Skin(Art::ESkin::ButtonDark,FVector2f(SelX+2.f,0.f),FVector2f(W-4.f,Size.Y));
    P.Next();
    const TArray<bool> BadgeList=Badges.Get(TArray<bool>());
    const double Now=Motion::Now();
    for(int32 I=0;I<Tabs.Num();++I)
    {
        const float Weight=FMath::Clamp(1.f-FMath::Abs(Slide.Value()-I),0.f,1.f);
        const FLinearColor Idle=bOnDark?K.TextLightDim:K.TextDarkDim;
        const FLinearColor Color=Lerp(I==HoverIndex?(bOnDark?K.TextLight:K.TextDark):Idle,K.TextGold,Weight);
        P.Text(Tabs[I].ToString(),Font(EFont::Button,FMath::Min(28.f,Size.Y*.42f)),FVector2f(W*(I+.5f),Size.Y*.5f),Color,FVector2f(.5f,.52f),Weight>.5f?WithAlpha(K.InkBlack,.6f*Weight):FLinearColor::Transparent);
        if(BadgeList.IsValidIndex(I)&&BadgeList[I])P.RedDot(FVector2f(W*(I+1)-W*.16f,Size.Y*.24f),7.f,Motion::Pulse(Now,1.4f,I*.7f));
    }
    return P.Layer+3;
}

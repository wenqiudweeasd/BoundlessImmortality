#include "InkUI/InkUIWidgets.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBox.h"

using namespace InkUI;

namespace
{
    /** Point on the half frame from the top-centre stud down one side to the bottom centre. */
    FVector2f FramePoint(FVector2f Size,float Inset,float U,bool bRight)
    {
        const float Half=Size.X*.5f-Inset,Side=Size.Y-Inset*2.f,Total=Half*2.f+Side;
        float D=FMath::Clamp(U,0.f,1.f)*Total;
        FVector2f P;
        if(D<Half)P=FVector2f(Size.X*.5f-D,Inset);
        else if(D<Half+Side)P=FVector2f(Inset,Inset+(D-Half));
        else P=FVector2f(Inset+(D-Half-Side),Size.Y-Inset);
        if(bRight)P.X=Size.X-P.X;
        return P;
    }
}

void SInkWindow::Construct(const FArguments& InArgs)
{
    Title=InArgs._Title;Seal=InArgs._Seal;
    SetCanTick(true);
    SetRenderTransformPivot(FVector2D(.5f,.42f));
    const FName CloseId=InArgs._TestId.IsNone()?FName(TEXT("Window.Close")):FName(*(InArgs._TestId.ToString()+TEXT(".Close")));
    ChildSlot
    [
        SNew(SOverlay)
        +SOverlay::Slot()
        [
            SAssignNew(Column,SVerticalBox)
            +SVerticalBox::Slot().AutoHeight().Padding(FMargin(42.f,82.f,42.f,6.f))[InArgs._Header.Widget]
            +SVerticalBox::Slot().FillHeight(1.f).Padding(FMargin(38.f,4.f,38.f,4.f))[InArgs._Body.Widget]
            +SVerticalBox::Slot().AutoHeight().Padding(FMargin(42.f,6.f,42.f,42.f))[InArgs._Footer.Widget]
        ]
        +SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Top).Padding(FMargin(0.f,2.f,4.f,0.f))
        [
            SNew(SInkCloseButton).TestId(CloseId).OnClicked(InArgs._OnClose)
        ]
        +SOverlay::Slot()[InArgs._Overlay.Widget]
    ];
    PlayOpen();
}

void SInkWindow::PlayOpen()
{
    bClosing=false;
    SetVisibility(EVisibility::SelfHitTestInvisible);
    Open.Start(0.f,1.f,Durations::PageOpen,EEase::OutBackSoft);
    Trace.Start(0.f,1.f,.95f,EEase::InOutCubic,.06f);
}

void SInkWindow::PlayClose()
{
    if(bClosing)return;
    bClosing=true;
    SetVisibility(EVisibility::HitTestInvisible);
    Open.Start(Open.Value(),0.f,Durations::PageClose,EEase::InCubic);
}

void SInkWindow::Tick(const FGeometry& AllottedGeometry,const double InCurrentTime,const float InDeltaTime)
{
    const FVector2f Size(AllottedGeometry.GetLocalSize());
    if(Column&&Size.X>1.f&&!Size.Equals(LaidOutFor,.5f))
    {
        // The peaks-and-cranes cap (source rows 44-270) and the pine-and-mountain foot (rows 1190-1476) scale with the frame;
        // header and footer sit on the paper between them instead of over the painting.
        LaidOutFor=Size;
        const Art::FSkin& Frame=Art::Skin(Art::ESkin::Window);
        const float Scale=FMath::Min3(1.f,Size.X/Frame.SourceWidth(),Size.Y/Frame.SourceHeight());
        Column->GetSlot(0).SetPadding(FMargin(42.f,FMath::Max(82.f,226.f*Scale*.7f),42.f,6.f));
        Column->GetSlot(2).SetPadding(FMargin(42.f,6.f,42.f,FMath::Max(42.f,286.f*Scale*.78f)));
    }
    Open.Tick(InDeltaTime);Trace.Tick(InDeltaTime);
    const float V=Open.Value();
    SetRenderOpacity(FMath::Clamp(V*1.5f,0.f,1.f));
    const float Scale=Motion::IsReduced()?1.f:FMath::Lerp(bClosing?.96f:.9f,1.f,V);
    const float Drop=Motion::IsReduced()?0.f:(1.f-FMath::Min(V,1.f))*(bClosing?12.f:28.f);
    SetRenderTransform(FSlateRenderTransform(FScale2f(Scale),FVector2f(0.f,Drop)));
}

int32 SInkWindow::OnPaint(const FPaintArgs& Args,const FGeometry& Geometry,const FSlateRect& Culling,FSlateWindowElementList& Out,int32 Layer,const FWidgetStyle& Style,bool bParentEnabled) const
{
    FPainter P(Geometry,Out,Layer,Style);
    const auto& K=Colors();
    const FVector2f Size=P.Size();
    P.Halo(FVector2f(14.f,14.f),Size-FVector2f(28.f,28.f),18.f,40.f,WithAlpha(K.InkBlack,.6f));
    P.Next();
    P.Skin(Art::ESkin::Window,FVector2f(0.f,0.f),Size);
    const float T=Trace.Value();
    if(T>0.f&&T<1.f&&!Motion::IsReduced())
    {
        // Gilding sweep: two sparks run the frame from the top stud to the bottom one.
        P.Next();
        const float Fade=FMath::Sin(T*PI);
        for(const bool bRight:{false,true})
        {
            const FVector2f Head=FramePoint(Size,9.f,T,bRight),Tail=FramePoint(Size,9.f,FMath::Max(0.f,T-.08f),bRight);
            P.FadeLine(Tail,Head,WithAlpha(K.GoldBright,.9f*Fade),3.f,.1f);
            P.Glow(Head,FVector2f(20.f),WithAlpha(K.GoldBright,.85f*Fade),FLinearColor::Transparent,18);
        }
    }
    P.Next();
    const FString TitleText=Title.ToString();
    const FSlateFontInfo TitleFont=Font(EFont::Title,44.f,2.f,WithAlpha(K.InkBlack,.9f));
    const float TitleWidth=FPainter::Measure(TitleText,TitleFont).X;
    const float PlaqueW=FMath::Clamp(TitleWidth+190.f,300.f,FMath::Max(300.f,Size.X-200.f)),PlaqueH=84.f,PlaqueY=-28.f;
    const FVector2f PlaquePos((Size.X-PlaqueW)*.5f,PlaqueY);
    P.Glow(PlaquePos+FVector2f(PlaqueW*.5f,PlaqueH*.55f),FVector2f(PlaqueW*.62f,PlaqueH*.95f),WithAlpha(K.InkBlack,.5f));
    P.Next();
    P.Skin(Art::ESkin::PlaqueDarkGold,PlaquePos,FVector2f(PlaqueW,PlaqueH));
    P.Next();
    P.Cloud(FVector2f(PlaquePos.X-30.f,PlaqueY+PlaqueH*.6f),.95f,false,WithAlpha(K.Gold,.92f),2.2f);
    P.Cloud(FVector2f(PlaquePos.X+PlaqueW+30.f,PlaqueY+PlaqueH*.6f),.95f,true,WithAlpha(K.Gold,.92f),2.2f);
    P.Text(TitleText,TitleFont,FVector2f(Size.X*.5f,PlaqueY+PlaqueH*.5f),K.GoldBright,FVector2f(.5f,.54f),WithAlpha(K.InkBlack,.75f));
    if(!Seal.IsEmpty())
    {
        P.Next();P.Next();
        const FVector2f SealPos(Size.X*.5f+TitleWidth*.5f+14.f,PlaqueY+PlaqueH*.5f-17.f);
        P.Rounded(SealPos,FVector2f(30.f,34.f),K.Cinnabar,4.f,Hex(TEXT("6B1A10")),1.2f);
        P.Next();
        P.Text(Seal.ToString().Left(1),Font(EFont::Title,22.f),SealPos+FVector2f(15.f,17.f),Hex(TEXT("FFE9D0")),FVector2f(.5f,.55f));
    }
    return SCompoundWidget::OnPaint(Args,Geometry,Culling,Out,P.Layer+2,Style,bParentEnabled);
}

void SInkToastHost::Construct(const FArguments& InArgs)
{
    SetCanTick(true);
    ForceVolatile(true);
    SetVisibility(EVisibility::HitTestInvisible);
}

void SInkToastHost::Push(const FText& Message,int32 Kind)
{
    if(Message.IsEmpty())return;
    Toasts.Add({Message,Kind,0.f});
    while(Toasts.Num()>3)Toasts.RemoveAt(0);
}

void SInkToastHost::Tick(const FGeometry& AllottedGeometry,const double InCurrentTime,const float InDeltaTime)
{
    for(auto& T:Toasts)T.Age+=InDeltaTime;
    Toasts.RemoveAll([](const FToast& T){return T.Age>Durations::Toast;});
}

int32 SInkToastHost::OnPaint(const FPaintArgs& Args,const FGeometry& Geometry,const FSlateRect& Culling,FSlateWindowElementList& Out,int32 Layer,const FWidgetStyle& Style,bool bParentEnabled) const
{
    FPainter P(Geometry,Out,Layer,Style);
    const auto& K=Colors();
    const FVector2f Size=P.Size();
    const FSlateFontInfo TextFont=Font(EFont::Body,26.f);
    for(int32 I=0;I<Toasts.Num();++I)
    {
        const FToast& T=Toasts[I];
        const float In=FMath::Clamp(T.Age/.25f,0.f,1.f),OutA=FMath::Clamp((Durations::Toast-T.Age)/.35f,0.f,1.f),A=FMath::Min(In,OutA);
        const float Slide=Motion::IsReduced()?0.f:(1.f-Ease(EEase::OutCubic,In))*-18.f;
        const FString Text=T.Message.ToString();
        const float W=FMath::Min(Size.X-48.f,FPainter::Measure(Text,TextFont).X+128.f),H=62.f;
        const FVector2f Pos((Size.X-W)*.5f,Size.Y*.17f+I*72.f+Slide);
        P.Glow(Pos+FVector2f(W*.5f,H*.5f),FVector2f(W*.62f,H*1.1f),WithAlpha(K.InkBlack,.45f*A));
        P.Next();
        P.Skin(Art::ESkin::PlaqueLacquer,Pos,FVector2f(W,H),FLinearColor(1,1,1,A));
        P.Next();
        const FLinearColor Accent=T.Kind==1?K.GoldBright:T.Kind==2?K.CinnabarBright:K.Gold;
        P.Diamond(Pos+FVector2f(40.f,H*.5f),FVector2f(7.f),WithAlpha(Accent,A));
        P.Text(Text,TextFont,Pos+FVector2f(58.f,H*.5f),WithAlpha(T.Kind==2?Hex(TEXT("FFD2BF")):K.TextLight,A),FVector2f(0.f,.52f));
        P.Next();P.Next();
    }
    return P.Layer+1;
}

void SInkModal::Construct(const FArguments& InArgs)
{
    OnConfirm=InArgs._OnConfirm;OnCancel=InArgs._OnCancel;
    SetCanTick(true);
    Fade.Start(0.f,1.f,.2f);
    const TWeakPtr<SInkModal> Weak=SharedThis(this);
    const FText ConfirmText=InArgs._ConfirmText.IsEmpty()?FText::FromString(TEXT("确定")):InArgs._ConfirmText;
    const FText CancelText=InArgs._CancelText.IsEmpty()?FText::FromString(TEXT("取消")):InArgs._CancelText;
    ChildSlot
    [
        SNew(SBox).HAlign(HAlign_Center).VAlign(VAlign_Center).Padding(FMargin(24.f))
        [
            SAssignNew(Card,SInkAnimBox).Enter(EEnter::Pop).Duration(.32f)
            [
                SNew(SBox).WidthOverride(600.f)
                [
                    SNew(SOverlay)
                    +SOverlay::Slot()
                    [
                        SNew(SInkPaint).OnPaint_Lambda([](FPainter& P,const FGeometry&)
                        {
                            P.Halo(FVector2f(10.f,10.f),P.Size()-FVector2f(20.f,20.f),14.f,30.f,WithAlpha(Colors().InkBlack,.6f));
                            P.Next();
                            P.Skin(Art::ESkin::CardOrnate,FVector2f(0.f,0.f),P.Size(),FLinearColor::White,.62f);
                        })
                    ]
                    +SOverlay::Slot().Padding(FMargin(56.f,46.f,56.f,42.f))
                    [
                        SNew(SVerticalBox)
                        +SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)[MakeText(InArgs._Title,EFont::Title,Colors().TextDark,42.f)]
                        +SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f,6.f))
                        [
                            SNew(SInkPaint).DesiredSize(FVector2D(420.f,18.f)).OnPaint_Lambda([](FPainter& P,const FGeometry&)
                            {
                                P.Divider(P.Size()*.5f,P.Size().X*.9f,WithAlpha(Colors().GoldDeep,.95f));
                            })
                        ]
                        +SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f,6.f,0.f,30.f))[MakeText(InArgs._Message,EFont::Body,Colors().TextDark,27.f,0.f,true,ETextJustify::Center)]
                        +SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
                        [
                            SNew(SHorizontalBox)
                            +SHorizontalBox::Slot().AutoWidth().Padding(FMargin(10.f,0.f))
                            [
                                SNew(SInkButton).Skin(EButtonSkin::Paper).Text(CancelText).Height(72.f).MinWidth(200.f).TestId(TEXT("Modal.Cancel"))
                                .OnClicked_Lambda([Weak]{if(const auto S=Weak.Pin())S->Dismiss(false);})
                            ]
                            +SHorizontalBox::Slot().AutoWidth().Padding(FMargin(10.f,0.f))
                            [
                                SNew(SInkButton).Skin(InArgs._Danger?EButtonSkin::Cinnabar:EButtonSkin::Gold).Text(ConfirmText).Height(72.f).MinWidth(200.f).TestId(TEXT("Modal.Confirm"))
                                .OnClicked_Lambda([Weak]{if(const auto S=Weak.Pin())S->Dismiss(true);})
                            ]
                        ]
                    ]
                ]
            ]
        ]
    ];
}

void SInkModal::Dismiss(bool bConfirmed)
{
    if(bDismissing)return;
    bDismissing=true;
    SetVisibility(EVisibility::HitTestInvisible);
    Fade.Start(Fade.Value(),0.f,.18f,EEase::InCubic);
    if(Card)Card->PlayOut(0.f,.15f);
    (bConfirmed?OnConfirm:OnCancel).ExecuteIfBound();
}

void SInkModal::Tick(const FGeometry& AllottedGeometry,const double InCurrentTime,const float InDeltaTime){Fade.Tick(InDeltaTime);}

int32 SInkModal::OnPaint(const FPaintArgs& Args,const FGeometry& Geometry,const FSlateRect& Culling,FSlateWindowElementList& Out,int32 Layer,const FWidgetStyle& Style,bool bParentEnabled) const
{
    FPainter P(Geometry,Out,Layer,Style);
    P.Rounded(FVector2f(0.f,0.f),P.Size(),WithAlpha(Colors().InkBlack,.6f*Fade.Value()),0.f);
    return SCompoundWidget::OnPaint(Args,Geometry,Culling,Out,P.Layer+1,Style,bParentEnabled);
}

void SInkRewardOverlay::Construct(const FArguments& InArgs)
{
    Rewards=InArgs._Rewards;Title=InArgs._Title.IsEmpty()?FText::FromString(TEXT("恭喜获得")):InArgs._Title;OnClosed=InArgs._OnClosed;
    SetCanTick(true);
    ForceVolatile(true);
    Fade.Start(0.f,1.f,.25f);
}

void SInkRewardOverlay::Tick(const FGeometry& AllottedGeometry,const double InCurrentTime,const float InDeltaTime)
{
    Age+=InDeltaTime;Fade.Tick(InDeltaTime);
}

FReply SInkRewardOverlay::OnMouseButtonDown(const FGeometry& Geometry,const FPointerEvent& Event)
{
    if(Age>.5f)Close();
    return FReply::Handled();
}

void SInkRewardOverlay::Close()
{
    if(bClosing)return;
    bClosing=true;
    SetVisibility(EVisibility::HitTestInvisible);
    Fade.Start(Fade.Value(),0.f,.24f,EEase::InCubic);
    OnClosed.ExecuteIfBound();
}

int32 SInkRewardOverlay::OnPaint(const FPaintArgs& Args,const FGeometry& Geometry,const FSlateRect& Culling,FSlateWindowElementList& Out,int32 Layer,const FWidgetStyle& Style,bool bParentEnabled) const
{
    FPainter P(Geometry,Out,Layer,Style);
    const auto& K=Colors();
    const FVector2f Size=P.Size();
    const float A=Fade.Value();
    const double Now=Motion::Now();
    const FVector2f Center(Size.X*.5f,Size.Y*.46f);
    auto Fa=[&](FLinearColor C,float Mul=1.f){return WithAlpha(C,C.A*A*Mul);};
    P.Rounded(FVector2f(0.f,0.f),Size,Fa(K.InkBlack,.74f),0.f);
    P.Next();
    if(!Motion::IsReduced())
    {
        P.Rays(Center,36.f,Size.X*.7f,18,float(FMath::Fmod(Now*.22,2.0*PI)),.05f,Fa(K.GoldBright,.2f));
        P.Next();
        P.Rays(Center,24.f,Size.X*.5f,12,-float(FMath::Fmod(Now*.15,2.0*PI)),.035f,Fa(Hex(TEXT("FFFFFF")),.12f));
        P.Next();
    }
    P.Glow(Center,FVector2f(Size.X*.46f,Size.X*.4f),Fa(K.GoldBright,.24f));
    P.Next();
    const float TitleIn=Ease(EEase::OutBack,FMath::Clamp(Age/.42f,0.f,1.f));
    const float TitleY=Center.Y-210.f+(1.f-TitleIn)*-30.f;
    const FVector2f PlaqueExt(440.f,100.f);
    P.Skin(Art::ESkin::PlaqueDarkGold,FVector2f(Center.X-PlaqueExt.X*.5f,TitleY-PlaqueExt.Y*.5f),PlaqueExt,FLinearColor(1,1,1,A*FMath::Min(1.f,TitleIn)));
    P.Next();
    P.Cloud(FVector2f(Center.X-PlaqueExt.X*.5f-30.f,TitleY+8.f),1.05f,false,Fa(K.Gold,.95f),2.4f);
    P.Cloud(FVector2f(Center.X+PlaqueExt.X*.5f+30.f,TitleY+8.f),1.05f,true,Fa(K.Gold,.95f),2.4f);
    P.Text(Title.ToString(),Font(EFont::Title,54.f,2.f,K.InkBlack),FVector2f(Center.X,TitleY),Fa(K.GoldBright,FMath::Min(1.f,TitleIn)),FVector2f(.5f,.54f),Fa(K.InkBlack,.7f));
    P.Next();P.Next();
    const int32 N=Rewards.Num(),PerRow=FMath::Min(4,FMath::Max(1,N));
    const float SlotSize=FMath::Min(132.f,(Size.X-96.f-(PerRow-1)*26.f)/PerRow),Gap=26.f;
    for(int32 I=0;I<N;++I)
    {
        const int32 Row=I/PerRow,Col=I%PerRow,InRow=FMath::Min(PerRow,N-Row*PerRow);
        const float RowW=InRow*SlotSize+(InRow-1)*Gap;
        const FVector2f Cell(Center.X-RowW*.5f+Col*(SlotSize+Gap)+SlotSize*.5f,Center.Y-30.f+Row*(SlotSize+78.f));
        const float T=FMath::Clamp((Age-.28f-I*.09f)/.36f,0.f,1.f);
        if(T<=0.f)continue;
        const float S=Motion::IsReduced()?1.f:Ease(EEase::OutBack,T);
        const auto& R=Rewards[I];
        if(R.Quality>=EQuality::Epic&&T<1.f)P.Ring(Cell,SlotSize*(.5f+.5f*T),4.f*(1.f-T),Fa(QualityColor(R.Quality),1.f-T),Fa(QualityColor(R.Quality),0.f));
        P.Glow(Cell,FVector2f(SlotSize*.85f),Fa(QualityColor(R.Quality),.35f*T));
        P.Next();
        FPainter Sub(Geometry,Out,P.Layer,FWidgetStyle().BlendOpacity(A*FMath::Min(1.f,T*1.5f)));
        SInkItemSlot::PaintSlot(Sub,Cell-FVector2f(SlotSize*S*.5f),SlotSize*S,Art::ItemIcon(R.Icon),R.Quality,R.Count,false,false,false,false,0.f,I*.7f);
        P.Layer=Sub.Layer+1;
        P.Text(R.Name.ToString(),Font(EFont::Body,25.f,1.5f,K.InkBlack),FVector2f(Cell.X,Cell.Y+SlotSize*.5f+14.f),Fa(Lerp(QualityColor(R.Quality),K.TextLight,.35f),T),FVector2f(.5f,0.f));
        P.Next();
    }
    P.Motes(FVector2f(0.f,Size.Y*.18f),FVector2f(Size.X,Size.Y*.62f),22,Now,Fa(K.GoldBright,.85f),3,1.6f,6.f);
    if(Age>.9f)
    {
        P.Next();
        P.Text(TEXT("点击任意处继续"),Font(EFont::Small,25.f),FVector2f(Center.X,Size.Y*.82f),Fa(K.TextLightDim,.55f+.45f*Motion::Pulse(Now,1.6f)),FVector2f(.5f,.5f));
    }
    return P.Layer+2;
}

#include "InkUI/InkUIWidgets.h"
#include "Brushes/SlateNoResource.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Framework/Application/SlateApplication.h"
#include "Input/HittestGrid.h"
#include "Widgets/SWindow.h"
#include "Styling/SlateTypes.h"
#include "Widgets/Text/STextBlock.h"

using namespace InkUI;

namespace InkUI
{
    namespace Testing
    {
        namespace
        {
            TMap<FName,TWeakPtr<SWidget>>& Registry(){static TMap<FName,TWeakPtr<SWidget>> Map;return Map;}
        }

        void Register(FName Id,const TSharedRef<SWidget>& Widget){if(!Id.IsNone())Registry().Add(Id,Widget);}

        TSharedPtr<SWidget> Find(FName Id){if(const auto* Found=Registry().Find(Id))return Found->Pin();return nullptr;}

        TArray<FName> VisibleIds()
        {
            TArray<FName> Out;
            for(auto It=Registry().CreateIterator();It;++It)
            {
                const TSharedPtr<SWidget> W=It.Value().Pin();
                if(!W){It.RemoveCurrent();continue;}
                if(W->GetVisibility().IsVisible()&&!W->GetTickSpaceGeometry().GetLocalSize().IsNearlyZero())Out.Add(It.Key());
            }
            return Out;
        }

        bool Click(FName Id)
        {
            const TSharedPtr<SWidget> Target=Find(Id);
            if(!Target||!FSlateApplication::IsInitialized())return false;
            // Tick-space geometry is in desktop space like pointer events; paint-space geometry is window-local while a window paints.
            const FGeometry& Geometry=Target->GetTickSpaceGeometry();
            if(Geometry.GetLocalSize().IsNearlyZero())return false;
            FSlateApplication& Slate=FSlateApplication::Get();
            const TSharedPtr<SWindow> Window=Slate.FindWidgetWindow(Target.ToSharedRef());
            if(!Window)return false;
            const FVector2D Position=Geometry.LocalToAbsolute(Geometry.GetLocalSize()*.5f);
            // Hit-test inside the game window itself: an unattended PIE window may sit under the editor or another app at OS level,
            // but anything of ours covering the target inside the game window must still fail the click.
            TArray<FWidgetAndPointer> Bubble=Window->GetHittestGrid().GetBubblePath(Position,0.f,false,INDEX_NONE);
            const FWidgetPath Hit(Bubble);
            if(!Hit.ContainsWidget(Target.Get()))
            {
                UE_LOG(LogTemp,Warning,TEXT("InkUI click %s blocked by %s"),*Id.ToString(),Hit.IsValid()?*Hit.GetLastWidget()->ToString():TEXT("nothing"));
                return false;
            }
            const auto Pointer=[&Position](const TSet<FKey>& Pressed,FKey Effecting){return FPointerEvent(FSlateApplication::CursorPointerIndex,Position,Position,Pressed,Effecting,0,FModifierKeysState());};
            Slate.RoutePointerMoveEvent(Hit,Pointer({},EKeys::Invalid),false);
            Slate.RoutePointerDownEvent(Hit,Pointer({EKeys::LeftMouseButton},EKeys::LeftMouseButton));
            Slate.RoutePointerUpEvent(Hit,Pointer({},EKeys::LeftMouseButton));
            return true;
        }
    }

    const FScrollBarStyle& ScrollStyle()
    {
        static const FScrollBarStyle Style=[]
        {
            const FSlateNoResource None;
            const FSlateRoundedBoxBrush Thumb(WithAlpha(Colors().GoldDeep,.7f),3.f);
            const FSlateRoundedBoxBrush Active(WithAlpha(Colors().Gold,.95f),3.f);
            FScrollBarStyle S;
            S.SetVerticalBackgroundImage(None).SetHorizontalBackgroundImage(None).SetVerticalTopSlotImage(None).SetVerticalBottomSlotImage(None)
             .SetHorizontalTopSlotImage(None).SetHorizontalBottomSlotImage(None).SetNormalThumbImage(Thumb).SetHoveredThumbImage(Active)
             .SetDraggedThumbImage(Active).SetThickness(5.f);
            return S;
        }();
        return Style;
    }

    TSharedRef<SWidget> MakeText(const TAttribute<FText>& Text,EFont Role,FLinearColor Color,float Size,float Outline,bool bWrap,ETextJustify::Type Justify)
    {
        return SNew(STextBlock).Text(Text).Font(Font(Role,Size,Outline,WithAlpha(Colors().InkBlack,.9f))).ColorAndOpacity(Color)
            .AutoWrapText(bWrap).Justification(Justify).LineHeightPercentage(bWrap?1.18f:1.f);
    }
}

void SInkAnimBox::Construct(const FArguments& InArgs)
{
    Enter=InArgs._Enter;Curve=InArgs._Curve;Delay=InArgs._Delay;Duration=InArgs._Duration;Distance=InArgs._Distance;
    SetCanTick(true);
    SetRenderTransformPivot(FVector2D(.5f,.5f));
    ChildSlot[InArgs._Content.Widget];
    if(InArgs._AutoPlay){Snap(false);PlayIn();}else Snap(true);
}

void SInkAnimBox::PlayIn(float ExtraDelay)
{
    bLeaving=false;
    const EEase Use=Enter==EEnter::Pop?EEase::OutBack:Curve;
    Progress.Start(Progress.Value(),1.f,Duration,Use,Delay+ExtraDelay);
    Apply();
}

void SInkAnimBox::PlayOut(float InDelay,float InDuration)
{
    bLeaving=true;
    Progress.Start(Progress.Value(),0.f,InDuration,EEase::InCubic,InDelay);
}

void SInkAnimBox::Snap(bool bShown){Progress.Snap(bShown?1.f:0.f);bLeaving=!bShown;Apply();}

void SInkAnimBox::Tick(const FGeometry& AllottedGeometry,const double InCurrentTime,const float InDeltaTime)
{
    if(Progress.Tick(InDeltaTime))Apply();
}

void SInkAnimBox::Apply()
{
    const float V=Progress.Value();
    const float Opacity=FMath::Clamp(Enter==EEnter::Pop?V*1.6f:V,0.f,1.f);
    const float Travel=(Motion::IsReduced()?0.f:1.f-V)*Distance*(bLeaving?.45f:1.f);
    FVector2D Offset(0,0);float Scale=1.f;
    switch(Enter)
    {
    case EEnter::Up:Offset.Y=Travel;break;
    case EEnter::Down:Offset.Y=-Travel;break;
    case EEnter::Left:Offset.X=-Travel;break;
    case EEnter::Right:Offset.X=Travel;break;
    case EEnter::Pop:Scale=Motion::IsReduced()?1.f:FMath::Lerp(.78f,1.f,V);break;
    case EEnter::Zoom:Scale=Motion::IsReduced()?1.f:FMath::Lerp(1.14f,1.f,V);break;
    default:break;
    }
    SetRenderOpacity(Opacity);
    SetRenderTransform(FSlateRenderTransform(FScale2f(Scale),FVector2f(Offset)));
    const EVisibility Wanted=Opacity<=.02f?EVisibility::HitTestInvisible:EVisibility::SelfHitTestInvisible;
    if(GetVisibility()!=Wanted)SetVisibility(Wanted);
}

void SInkPaint::Construct(const FArguments& InArgs)
{
    DesiredSize=InArgs._DesiredSize;PaintHandler=InArgs._OnPaint;
    if(!InArgs._HitTest)SetVisibility(EVisibility::HitTestInvisible);
    SetCanTick(false);
    ForceVolatile(true);
}

int32 SInkPaint::OnPaint(const FPaintArgs& Args,const FGeometry& Geometry,const FSlateRect& Culling,FSlateWindowElementList& Out,int32 Layer,const FWidgetStyle& Style,bool bParentEnabled) const
{
    FPainter P(Geometry,Out,Layer,Style);
    PaintHandler.ExecuteIfBound(P,Geometry);
    return P.Layer+2;
}

void SInkPressable::InitPressable(FName InTestId,const FSimpleDelegate& InOnClicked,const TAttribute<bool>& InEnabled)
{
    OnClicked=InOnClicked;Enabled=InEnabled;Phase=FMath::FRand()*6.2832f;
    SetCanTick(true);
    ForceVolatile(true);
    Hover.Snap(0.f);Press.Snap(0.f);Flash.Snap(0.f);
    if(!InTestId.IsNone())Testing::Register(InTestId,SharedThis(this));
}

bool SInkPressable::IsInteractive() const{return Enabled.Get(true)&&IsEnabled();}

void SInkPressable::Tick(const FGeometry& AllottedGeometry,const double InCurrentTime,const float InDeltaTime)
{
    Hover.Tick(InDeltaTime);Press.Tick(InDeltaTime);Flash.Tick(InDeltaTime);
}

FReply SInkPressable::OnMouseButtonDown(const FGeometry& Geometry,const FPointerEvent& Event)
{
    const bool bTouch=Event.IsTouchEvent();
    if(!IsInteractive()||(!bTouch&&Event.GetEffectingButton()!=EKeys::LeftMouseButton))return FReply::Unhandled();
    bPressed=true;bTouchPress=bTouch;PressPoint=FVector2f(Event.GetScreenSpacePosition());
    Press.Retarget(1.f,Durations::Press,EEase::OutQuad);
    // Touch leaves capture to a parent scroll box so a drag scrolls instead of clicking.
    return bTouch?FReply::Handled():FReply::Handled().CaptureMouse(SharedThis(this));
}

FReply SInkPressable::OnMouseMove(const FGeometry& Geometry,const FPointerEvent& Event)
{
    if(bPressed&&bTouchPress&&(FVector2f(Event.GetScreenSpacePosition())-PressPoint).Size()>16.f)
    {
        bPressed=false;Press.Retarget(0.f,Durations::Release);
    }
    return FReply::Unhandled();
}

FReply SInkPressable::OnMouseButtonUp(const FGeometry& Geometry,const FPointerEvent& Event)
{
    if(!bPressed)return FReply::Unhandled();
    bPressed=false;
    Press.Retarget(0.f,Durations::Release,EEase::OutBack);
    const bool bInside=Geometry.IsUnderLocation(Event.GetScreenSpacePosition());
    FReply Reply=FReply::Handled();
    if(HasMouseCapture())Reply.ReleaseMouseCapture();
    if(bInside&&IsInteractive())HandleClick();
    return Reply;
}

void SInkPressable::OnMouseEnter(const FGeometry& Geometry,const FPointerEvent& Event){Hover.Retarget(1.f,.12f);}

void SInkPressable::OnMouseLeave(const FPointerEvent& Event)
{
    Hover.Retarget(0.f,.2f);
    if(bPressed&&bTouchPress){bPressed=false;Press.Retarget(0.f,Durations::Release);}
}

void SInkPressable::OnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent)
{
    bPressed=false;Press.Retarget(0.f,Durations::Release);
}

FReply SInkPressable::OnKeyDown(const FGeometry& Geometry,const FKeyEvent& Event)
{
    const FKey Key=Event.GetKey();
    if(IsInteractive()&&(Key==EKeys::Enter||Key==EKeys::SpaceBar||Key==EKeys::Gamepad_FaceButton_Bottom)){HandleClick();return FReply::Handled();}
    return FReply::Unhandled();
}

FCursorReply SInkPressable::OnCursorQuery(const FGeometry& Geometry,const FPointerEvent& Event) const
{
    return IsInteractive()?FCursorReply::Cursor(EMouseCursor::Hand):FCursorReply::Unhandled();
}

void SInkPressable::HandleClick()
{
    Flash.Start(1.f,0.f,.5f,EEase::OutCubic);
    OnClicked.ExecuteIfBound();
}

void SInkClickable::Construct(const FArguments& InArgs)
{
    OnClicked=InArgs._OnClicked;Enabled=InArgs._Enabled;Radius=InArgs._Radius;
    SetCanTick(true);
    SetRenderTransformPivot(FVector2D(.5f,.5f));
    ChildSlot[InArgs._Content.Widget];
    if(!InArgs._TestId.IsNone())Testing::Register(InArgs._TestId,SharedThis(this));
}

void SInkClickable::Tick(const FGeometry& AllottedGeometry,const double InCurrentTime,const float InDeltaTime)
{
    if(Hover.Tick(InDeltaTime)|Press.Tick(InDeltaTime))
        SetRenderTransform(FSlateRenderTransform(FScale2f(1.f-.025f*Press.Value())));
}

int32 SInkClickable::OnPaint(const FPaintArgs& Args,const FGeometry& Geometry,const FSlateRect& Culling,FSlateWindowElementList& Out,int32 Layer,const FWidgetStyle& Style,bool bParentEnabled) const
{
    const int32 Top=SCompoundWidget::OnPaint(Args,Geometry,Culling,Out,Layer,Style,bParentEnabled);
    if(Hover.Value()<.01f&&Press.Value()<.01f)return Top;
    FPainter P(Geometry,Out,Top+1,Style);
    P.Rounded(FVector2f(2.f,2.f),P.Size()-FVector2f(4.f,4.f),WithAlpha(Colors().GoldBright,.07f*Hover.Value()+.06f*Press.Value()),Radius,WithAlpha(Colors().GoldBright,.35f*Hover.Value()),1.f);
    return P.Layer+1;
}

FReply SInkClickable::OnMouseButtonDown(const FGeometry& Geometry,const FPointerEvent& Event)
{
    bTouch=Event.IsTouchEvent();
    if(!Enabled.Get(true)||(!bTouch&&Event.GetEffectingButton()!=EKeys::LeftMouseButton))return FReply::Unhandled();
    bPressed=true;PressPoint=FVector2f(Event.GetScreenSpacePosition());
    Press.Retarget(1.f,Durations::Press,EEase::OutQuad);
    return bTouch?FReply::Handled():FReply::Handled().CaptureMouse(SharedThis(this));
}

FReply SInkClickable::OnMouseMove(const FGeometry& Geometry,const FPointerEvent& Event)
{
    if(bPressed&&bTouch&&(FVector2f(Event.GetScreenSpacePosition())-PressPoint).Size()>16.f){bPressed=false;Press.Retarget(0.f,Durations::Release);}
    return FReply::Unhandled();
}

FReply SInkClickable::OnMouseButtonUp(const FGeometry& Geometry,const FPointerEvent& Event)
{
    if(!bPressed)return FReply::Unhandled();
    bPressed=false;Press.Retarget(0.f,Durations::Release,EEase::OutBack);
    FReply Reply=FReply::Handled();
    if(HasMouseCapture())Reply.ReleaseMouseCapture();
    if(Geometry.IsUnderLocation(Event.GetScreenSpacePosition())&&Enabled.Get(true))OnClicked.ExecuteIfBound();
    return Reply;
}

void SInkClickable::OnMouseLeave(const FPointerEvent& Event)
{
    Hover.Retarget(0.f,.2f);
    if(bPressed&&bTouch){bPressed=false;Press.Retarget(0.f,Durations::Release);}
}

FCursorReply SInkClickable::OnCursorQuery(const FGeometry& Geometry,const FPointerEvent& Event) const
{
    return Enabled.Get(true)?FCursorReply::Cursor(EMouseCursor::Hand):FCursorReply::Unhandled();
}

namespace
{
    Art::ESkin ButtonSkin(EButtonSkin Skin)
    {
        switch(Skin)
        {
        case EButtonSkin::Paper:return Art::ESkin::ButtonPaper;
        case EButtonSkin::Dark:return Art::ESkin::ButtonDark;
        case EButtonSkin::Blue:return Art::ESkin::ButtonBlue;
        case EButtonSkin::Lacquer:return Art::ESkin::PlaqueLacquer;
        case EButtonSkin::Cinnabar:return Art::ESkin::PlaqueCinnabar;
        case EButtonSkin::Jade:return Art::ESkin::PlaqueJade;
        default:return Art::ESkin::ButtonGold;
        }
    }

    void ButtonTextColors(EButtonSkin Skin,FLinearColor& Text,FLinearColor& Shadow)
    {
        const auto& K=Colors();
        switch(Skin)
        {
        case EButtonSkin::Gold:Text=Hex(TEXT("3A2306"));Shadow=Hex(TEXT("FFF2C8"),.45f);break;
        case EButtonSkin::Dark:
        case EButtonSkin::Lacquer:Text=K.TextGold;Shadow=WithAlpha(K.InkBlack,.75f);break;
        case EButtonSkin::Blue:Text=K.TextLight;Shadow=WithAlpha(K.InkBlack,.6f);break;
        case EButtonSkin::Cinnabar:Text=Hex(TEXT("FFF0D8"));Shadow=Hex(TEXT("5A1208"),.7f);break;
        case EButtonSkin::Ghost:Text=WithAlpha(K.TextLight,.9f);Shadow=WithAlpha(K.InkBlack,.85f);break;
        default:Text=K.TextDark;Shadow=FLinearColor::Transparent;break;
        }
    }
}

void SInkButton::Construct(const FArguments& InArgs)
{
    Text=InArgs._Text;Skin=InArgs._Skin;Height=InArgs._Height;MinWidth=InArgs._MinWidth;FontSize=InArgs._FontSize;
    Highlight=InArgs._Highlight;Pending=InArgs._Pending;Badge=InArgs._Badge;Icon=InArgs._Icon;
    InitPressable(InArgs._TestId,InArgs._OnClicked,InArgs._Enabled);
}

FSlateFontInfo SInkButton::GetFont() const
{
    return Font(EFont::Button,FontSize>0.f?FontSize:FMath::Clamp(Height*.38f,20.f,32.f));
}

FVector2D SInkButton::ComputeDesiredSize(float) const
{
    const FVector2f Label=FPainter::Measure(Text.Get().ToString(),GetFont());
    return FVector2D(FMath::Max(MinWidth,Label.X+Height*1.5f+(Icon?Height*.6f:0.f)),Height);
}

int32 SInkButton::OnPaint(const FPaintArgs& Args,const FGeometry& Geometry,const FSlateRect& Culling,FSlateWindowElementList& Out,int32 Layer,const FWidgetStyle& Style,bool bParentEnabled) const
{
    FPainter P(Geometry,Out,Layer,Style);
    const auto& K=Colors();
    const FVector2f Size=P.Size();
    const bool bOn=IsInteractive(),bBusy=Pending.Get(false);
    const double Now=Motion::Now();
    const float Pr=PressAmount(),Hv=bOn?HoverAmount():0.f;
    const FVector2f Ext=Size*(1.f-.035f*Pr),Pos=(Size-Ext)*.5f;
    const bool bHi=Highlight.Get(false)&&bOn&&!bBusy;
    if(bHi)
    {
        const float Pulse=Motion::Pulse(Now,Durations::BreathPeriod,Phase);
        P.Halo(Pos+FVector2f(Ext.Y*.22f,Ext.Y*.14f),Ext-FVector2f(Ext.Y*.44f,Ext.Y*.28f),Ext.Y*.3f,Ext.Y*.5f,WithAlpha(K.GoldBright,.16f+.24f*Pulse));
        P.Next();
    }
    if(Skin!=EButtonSkin::Ghost)P.Skin(ButtonSkin(Skin),Pos,Ext,bOn?FLinearColor::White:FLinearColor(.6f,.6f,.58f,1.f));
    P.Next();
    const FVector2f InPos=Pos+FVector2f(Ext.Y*.34f,Ext.Y*.2f),InExt=Ext-FVector2f(Ext.Y*.68f,Ext.Y*.4f);
    if(Hv>.01f)P.Rounded(InPos,InExt,WithAlpha(FLinearColor::White,.1f*Hv),InExt.Y*.3f);
    if(Pr>.01f)P.Rounded(InPos,InExt,WithAlpha(K.InkBlack,.16f*Pr),InExt.Y*.3f);
    if(Skin==EButtonSkin::Ghost)
    {
        // Tertiary action over the scene: a gilt rule instead of a plate, brightening on hover.
        const float RuleY=Pos.Y+Ext.Y-8.f;
        P.FadeLine(FVector2f(Pos.X+Ext.X*.14f,RuleY),FVector2f(Pos.X+Ext.X*.86f,RuleY),WithAlpha(K.Gold,.45f+.5f*Hv),1.5f);
        P.Diamond(FVector2f(Pos.X+Ext.X*.5f,RuleY),FVector2f(4.f,3.f),WithAlpha(K.GoldBright,.6f+.4f*Hv));
    }
    if(bHi)
    {
        P.Next();
        const float Cycle=float(FMath::Fmod(Now+Phase,double(Durations::ShinePeriod)));
        P.Shine(InPos,InExt,Cycle/1.15f,InExt.Y*.9f,WithAlpha(FLinearColor::White,.42f));
    }
    if(FlashAmount()>.01f)
    {
        P.Next();
        P.Halo(InPos,InExt,InExt.Y*.3f,Ext.Y*(.55f-.35f*FlashAmount()),WithAlpha(K.GoldBright,.55f*FlashAmount()));
    }
    P.Next();
    FLinearColor TextColor,Shadow;ButtonTextColors(Skin,TextColor,Shadow);
    if(!bOn){TextColor=WithAlpha(TextColor,.62f);Shadow=FLinearColor::Transparent;}
    FString Label=Text.Get().ToString();
    if(bBusy)Label=FString(TEXT("处理中"))+FString::ChrN(1+int32(Now*3.0)%3,TEXT('·'));
    const FSlateFontInfo LabelFont=GetFont();
    float IconSpace=0.f;
    if(Icon)
    {
        const float S=Ext.Y*.58f;IconSpace=S+6.f;
        const float W=FPainter::Measure(Label,LabelFont).X+IconSpace;
        P.BrushFit(Icon,FVector2f(Pos.X+(Ext.X-W)*.5f,Pos.Y+(Ext.Y-S)*.5f),FVector2f(S,S));
    }
    P.Text(Label,LabelFont,Pos+Ext*.5f+FVector2f(IconSpace*.5f,0.f),TextColor,FVector2f(.5f,.52f),Shadow);
    if(Badge.Get(false))
    {
        P.Next();P.Next();
        P.RedDot(Pos+FVector2f(Ext.X-Ext.Y*.42f,Ext.Y*.24f),9.f,Motion::Pulse(Now,1.4f,Phase));
    }
    return P.Layer+3;
}

void SInkIconButton::Construct(const FArguments& InArgs)
{
    Icon=InArgs._Icon;Label=InArgs._Label;IconSize=InArgs._IconSize;Size=InArgs._Size;bTile=InArgs._Tile;bGlow=InArgs._Glow;
    Active=InArgs._Active;Badge=InArgs._Badge;LabelSize=InArgs._LabelSize;
    InitPressable(InArgs._TestId,InArgs._OnClicked,true);
}

int32 SInkIconButton::OnPaint(const FPaintArgs& Args,const FGeometry& Geometry,const FSlateRect& Culling,FSlateWindowElementList& Out,int32 Layer,const FWidgetStyle& Style,bool bParentEnabled) const
{
    FPainter P(Geometry,Out,Layer,Style);
    const auto& K=Colors();
    const FVector2f Box=P.Size();
    const double Now=Motion::Now();
    const float Hv=HoverAmount(),Pr=PressAmount();
    const bool bOn=Active.Get(false);
    const float Lift=Motion::IsReduced()?0.f:-4.f*Hv;
    const float IconExtent=IconSize*(1.f-.07f*Pr);
    const FVector2f Center(Box.X*.5f,(bTile?10.f:2.f)+IconSize*.5f+Lift);
    if(bTile)
    {
        P.Skin(Art::ESkin::TopTile,FVector2f(0,0),Box,FLinearColor::White,Box.X/880.f);
        P.Next();
        if(Hv>.01f){P.Rounded(FVector2f(6,6),Box-FVector2f(12,12),WithAlpha(K.GoldBright,.08f*Hv),8.f);P.Next();}
    }
    if(bGlow)
    {
        const float Pulse=Motion::Pulse(Now,Durations::BreathPeriod,Phase);
        P.Glow(Center,FVector2f(IconSize*.82f),WithAlpha(K.GoldBright,(bOn?.42f:.24f)+.18f*Pulse));
        P.Next();
        P.Ring(Center,IconSize*.56f,3.f,WithAlpha(K.Gold,.9f),WithAlpha(K.GoldDeep,.25f));
        if(bOn&&!Motion::IsReduced())
        {
            const float Spin=float(FMath::Fmod(Now,100.0));
            P.Next();
            P.Rays(Center,IconSize*.34f,IconSize*1.02f,14,Spin*.45f,.06f,WithAlpha(K.GoldBright,.32f));
            P.Next();
            P.Ring(Center,IconSize*.64f,4.f,WithAlpha(K.GoldBright,.95f),WithAlpha(K.GoldBright,0.f),Spin*2.2f,PI*.55f,32);
            P.Ring(Center,IconSize*.64f,4.f,WithAlpha(K.GoldBright,.95f),WithAlpha(K.GoldBright,0.f),Spin*2.2f+PI,PI*.55f,32);
            P.Motes(Center-FVector2f(IconSize*.7f,IconSize*.9f),FVector2f(IconSize*1.4f,IconSize*1.2f),9,Now,WithAlpha(K.GoldBright,.9f),11,2.2f,5.f);
        }
        P.Next();
    }
    P.BrushFit(Icon,Center-FVector2f(IconExtent*.5f),FVector2f(IconExtent),IsInteractive()?FLinearColor::White:FLinearColor(.5f,.5f,.5f,1.f));
    if(FlashAmount()>.01f){P.Next();P.Glow(Center,FVector2f(IconSize*.7f),WithAlpha(K.GoldBright,.45f*FlashAmount()));}
    P.Next();
    const FText Caption=Label.Get();
    if(!Caption.IsEmpty())
    {
        const float LabelY=bTile?Box.Y-LabelSize-12.f:Center.Y+IconSize*.5f-2.f-Lift;
        P.Text(Caption.ToString(),Font(EFont::Hud,LabelSize,2.f,WithAlpha(K.InkBlack,.95f)),FVector2f(Box.X*.5f,LabelY),bOn?K.GoldBright:K.TextLight,FVector2f(.5f,0.f));
    }
    if(Badge.Get(false))
    {
        P.Next();P.Next();
        P.RedDot(Center+FVector2f(IconSize*.4f,-IconSize*.38f),9.f,Motion::Pulse(Now,1.4f,Phase));
    }
    return P.Layer+3;
}

void SInkCloseButton::Construct(const FArguments& InArgs)
{
    InitPressable(InArgs._TestId,InArgs._OnClicked,true);
}

int32 SInkCloseButton::OnPaint(const FPaintArgs& Args,const FGeometry& Geometry,const FSlateRect& Culling,FSlateWindowElementList& Out,int32 Layer,const FWidgetStyle& Style,bool bParentEnabled) const
{
    FPainter P(Geometry,Out,Layer,Style);
    const auto& K=Colors();
    const FVector2f C=P.Size()*.5f;
    const float R=25.f*(1.f-.08f*PressAmount());
    P.Glow(C,FVector2f(R*1.6f),WithAlpha(K.InkBlack,.55f));
    P.Next();
    P.Rounded(C-FVector2f(R),FVector2f(R*2.f),K.Lacquer,R,WithAlpha(K.Gold,.95f),2.f);
    P.Next();
    P.Ring(C,R-5.f,1.2f,WithAlpha(K.Gold,.5f),WithAlpha(K.Gold,.5f));
    P.Next();
    const float Spin=(Motion::IsReduced()?0.f:HoverAmount()*HALF_PI)+PI*.25f;
    const FLinearColor Mark=Lerp(K.Gold,K.GoldBright,HoverAmount());
    for(int32 I=0;I<2;++I)
    {
        const float A=Spin+I*HALF_PI;
        const FVector2f D(FMath::Cos(A)*R*.46f,FMath::Sin(A)*R*.46f);
        P.Line(C-D,C+D,Mark,3.f);
    }
    return P.Layer+1;
}

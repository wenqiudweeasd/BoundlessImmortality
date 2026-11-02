#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SLeafWidget.h"
#include "Widgets/Layout/SBox.h"
#include "Framework/Text/TextLayout.h"
#include "InkUI/InkUIArt.h"
#include "InkUI/InkUIDraw.h"
#include "InkUI/InkUIModel.h"
#include "InkUI/InkUIMotion.h"
#include "InkUI/InkUITokens.h"

class SOverlay;
class SVerticalBox;
class SScrollBox;

namespace InkUI
{
    /** Test hooks: interactive widgets register a stable id so automation clicks real geometry. */
    namespace Testing
    {
        BOUNDLESSIMMORTALITY_API void Register(FName Id,const TSharedRef<SWidget>& Widget);
        BOUNDLESSIMMORTALITY_API TSharedPtr<SWidget> Find(FName Id);
        BOUNDLESSIMMORTALITY_API TArray<FName> VisibleIds();
        /** Clicks a registered control through real Slate hit testing; false if missing, hidden or covered. */
        BOUNDLESSIMMORTALITY_API bool Click(FName Id);
    }

    enum class EEnter : uint8 { None, Fade, Up, Down, Left, Right, Pop, Zoom };
    enum class EButtonSkin : uint8 { Gold, Paper, Dark, Blue, Lacquer, Cinnabar, Jade, Ghost };
    enum class EBar : uint8 { Health, Qi, Gold, Jade, Red };

    BOUNDLESSIMMORTALITY_API const struct FScrollBarStyle& ScrollStyle();
    /** Builds a styled text block; text roles map to the calligraphy or kaishu font. */
    BOUNDLESSIMMORTALITY_API TSharedRef<SWidget> MakeText(const TAttribute<FText>& Text,EFont Role,FLinearColor Color,float Size=0.f,float Outline=0.f,bool bWrap=false,ETextJustify::Type Justify=ETextJustify::Left);
}

/** Wraps content with enter/exit motion driven by render transform and opacity only. */
class BOUNDLESSIMMORTALITY_API SInkAnimBox : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SInkAnimBox):_Enter(InkUI::EEnter::Up),_Delay(0.f),_Duration(.34f),_Distance(28.f),_Curve(InkUI::EEase::OutCubic),_AutoPlay(true){}
        SLATE_DEFAULT_SLOT(FArguments,Content)
        SLATE_ARGUMENT(InkUI::EEnter,Enter)
        SLATE_ARGUMENT(float,Delay)
        SLATE_ARGUMENT(float,Duration)
        SLATE_ARGUMENT(float,Distance)
        SLATE_ARGUMENT(InkUI::EEase,Curve)
        SLATE_ARGUMENT(bool,AutoPlay)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);
    void PlayIn(float ExtraDelay=0.f);
    void PlayOut(float Delay=0.f,float Duration=.16f);
    void Snap(bool bShown);
    bool IsAnimating() const{return Progress.bRunning;}
    virtual void Tick(const FGeometry& AllottedGeometry,const double InCurrentTime,const float InDeltaTime) override;

private:
    InkUI::FTween Progress;
    InkUI::EEnter Enter=InkUI::EEnter::Up;
    InkUI::EEase Curve=InkUI::EEase::OutCubic;
    float Delay=0.f,Duration=.34f,Distance=28.f;
    bool bLeaving=false;
    void Apply();
};

/** Ad-hoc ornament painter; repaints every frame so it can host looping motion. */
class BOUNDLESSIMMORTALITY_API SInkPaint : public SLeafWidget
{
public:
    DECLARE_DELEGATE_TwoParams(FOnInkPaint,InkUI::FPainter&,const FGeometry&);
    SLATE_BEGIN_ARGS(SInkPaint):_DesiredSize(FVector2D(16,16)),_HitTest(false){}
        SLATE_ARGUMENT(FVector2D,DesiredSize)
        /** Ornament never takes clicks unless asked; the veil opts in to block the world. */
        SLATE_ARGUMENT(bool,HitTest)
        SLATE_EVENT(FOnInkPaint,OnPaint)
    SLATE_END_ARGS()
    void Construct(const FArguments& InArgs);
    virtual FVector2D ComputeDesiredSize(float) const override{return DesiredSize;}
    virtual int32 OnPaint(const FPaintArgs& Args,const FGeometry& Geometry,const FSlateRect& Culling,FSlateWindowElementList& Out,int32 Layer,const FWidgetStyle& Style,bool bParentEnabled) const override;
private:
    FVector2D DesiredSize;
    FOnInkPaint PaintHandler;
};

/** Shared hover/press/click behaviour for painted controls; touch presses cancel when dragged. */
class BOUNDLESSIMMORTALITY_API SInkPressable : public SLeafWidget
{
public:
    virtual void Tick(const FGeometry& AllottedGeometry,const double InCurrentTime,const float InDeltaTime) override;
    virtual FReply OnMouseButtonDown(const FGeometry& Geometry,const FPointerEvent& Event) override;
    virtual FReply OnMouseButtonUp(const FGeometry& Geometry,const FPointerEvent& Event) override;
    virtual FReply OnMouseMove(const FGeometry& Geometry,const FPointerEvent& Event) override;
    virtual void OnMouseEnter(const FGeometry& Geometry,const FPointerEvent& Event) override;
    virtual void OnMouseLeave(const FPointerEvent& Event) override;
    virtual void OnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent) override;
    virtual FReply OnKeyDown(const FGeometry& Geometry,const FKeyEvent& Event) override;
    virtual bool SupportsKeyboardFocus() const override{return IsInteractive();}
    virtual FCursorReply OnCursorQuery(const FGeometry& Geometry,const FPointerEvent& Event) const override;
    bool IsInteractive() const;
    void SetOnClicked(FSimpleDelegate InOnClicked){OnClicked=InOnClicked;}

protected:
    void InitPressable(FName InTestId,const FSimpleDelegate& InOnClicked,const TAttribute<bool>& InEnabled);
    virtual void HandleClick();
    float HoverAmount() const{return Hover.Value();}
    float PressAmount() const{return Press.Value();}
    float FlashAmount() const{return Flash.Value();}
    InkUI::FTween Hover,Press,Flash;
    FSimpleDelegate OnClicked;
    TAttribute<bool> Enabled;
    float Phase=0.f;

private:
    bool bPressed=false;
    bool bTouchPress=false;
    FVector2f PressPoint=FVector2f::ZeroVector;
};

/** Clickable wrapper for composite cards; adds a soft hover sheen and a press squash. */
class BOUNDLESSIMMORTALITY_API SInkClickable : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SInkClickable):_TestId(NAME_None),_Radius(10.f),_Enabled(true){}
        SLATE_DEFAULT_SLOT(FArguments,Content)
        SLATE_ARGUMENT(FName,TestId)
        SLATE_ARGUMENT(float,Radius)
        SLATE_ATTRIBUTE(bool,Enabled)
        SLATE_EVENT(FSimpleDelegate,OnClicked)
    SLATE_END_ARGS()
    void Construct(const FArguments& InArgs);
    virtual void Tick(const FGeometry& AllottedGeometry,const double InCurrentTime,const float InDeltaTime) override;
    virtual int32 OnPaint(const FPaintArgs& Args,const FGeometry& Geometry,const FSlateRect& Culling,FSlateWindowElementList& Out,int32 Layer,const FWidgetStyle& Style,bool bParentEnabled) const override;
    virtual FReply OnMouseButtonDown(const FGeometry& Geometry,const FPointerEvent& Event) override;
    virtual FReply OnMouseButtonUp(const FGeometry& Geometry,const FPointerEvent& Event) override;
    virtual FReply OnMouseMove(const FGeometry& Geometry,const FPointerEvent& Event) override;
    virtual void OnMouseEnter(const FGeometry& Geometry,const FPointerEvent& Event) override{Hover.Retarget(1.f,.12f);}
    virtual void OnMouseLeave(const FPointerEvent& Event) override;
    virtual void OnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent) override{bPressed=false;Press.Retarget(0.f,InkUI::Durations::Release);}
    virtual FCursorReply OnCursorQuery(const FGeometry& Geometry,const FPointerEvent& Event) const override;
private:
    FSimpleDelegate OnClicked;
    TAttribute<bool> Enabled;
    InkUI::FTween Hover,Press;
    float Radius=10.f;
    bool bPressed=false,bTouch=false;
    FVector2f PressPoint=FVector2f::ZeroVector;
};

class BOUNDLESSIMMORTALITY_API SInkButton : public SInkPressable
{
public:
    SLATE_BEGIN_ARGS(SInkButton):_Skin(InkUI::EButtonSkin::Gold),_Height(76.f),_MinWidth(176.f),_FontSize(0.f),_Highlight(false),_TestId(NAME_None),_Enabled(true),_Pending(false),_Badge(false),_Icon(nullptr){}
        SLATE_ATTRIBUTE(FText,Text)
        SLATE_ARGUMENT(InkUI::EButtonSkin,Skin)
        SLATE_ARGUMENT(float,Height)
        SLATE_ARGUMENT(float,MinWidth)
        SLATE_ARGUMENT(float,FontSize)
        SLATE_ATTRIBUTE(bool,Highlight)
        SLATE_ARGUMENT(FName,TestId)
        SLATE_ATTRIBUTE(bool,Enabled)
        SLATE_ATTRIBUTE(bool,Pending)
        SLATE_ATTRIBUTE(bool,Badge)
        SLATE_ARGUMENT(const FSlateBrush*,Icon)
        SLATE_EVENT(FSimpleDelegate,OnClicked)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);
    virtual FVector2D ComputeDesiredSize(float) const override;
    virtual int32 OnPaint(const FPaintArgs& Args,const FGeometry& Geometry,const FSlateRect& Culling,FSlateWindowElementList& Out,int32 Layer,const FWidgetStyle& Style,bool bParentEnabled) const override;

private:
    TAttribute<FText> Text;
    TAttribute<bool> Highlight,Pending,Badge;
    InkUI::EButtonSkin Skin=InkUI::EButtonSkin::Gold;
    float Height=76.f,MinWidth=176.f,FontSize=0.f;
    const FSlateBrush* Icon=nullptr;
    FSlateFontInfo GetFont() const;
};

/** HUD entry: painted icon with outlined caption, optional tile, red dot and focus glow. */
class BOUNDLESSIMMORTALITY_API SInkIconButton : public SInkPressable
{
public:
    SLATE_BEGIN_ARGS(SInkIconButton):_Icon(nullptr),_IconSize(64.f),_Size(FVector2D(96,112)),_Tile(false),_Glow(false),_Active(false),_Badge(false),_LabelSize(24.f),_TestId(NAME_None){}
        SLATE_ARGUMENT(const FSlateBrush*,Icon)
        SLATE_ATTRIBUTE(FText,Label)
        SLATE_ARGUMENT(float,IconSize)
        SLATE_ARGUMENT(FVector2D,Size)
        SLATE_ARGUMENT(bool,Tile)
        SLATE_ARGUMENT(bool,Glow)
        SLATE_ATTRIBUTE(bool,Active)
        SLATE_ATTRIBUTE(bool,Badge)
        SLATE_ARGUMENT(float,LabelSize)
        SLATE_ARGUMENT(FName,TestId)
        SLATE_EVENT(FSimpleDelegate,OnClicked)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);
    virtual FVector2D ComputeDesiredSize(float) const override{return Size;}
    virtual int32 OnPaint(const FPaintArgs& Args,const FGeometry& Geometry,const FSlateRect& Culling,FSlateWindowElementList& Out,int32 Layer,const FWidgetStyle& Style,bool bParentEnabled) const override;
    void SetSize(FVector2D InSize,float InIconSize){Size=InSize;IconSize=InIconSize;Invalidate(EInvalidateWidgetReason::Layout);}

private:
    const FSlateBrush* Icon=nullptr;
    TAttribute<FText> Label;
    TAttribute<bool> Active,Badge;
    float IconSize=64.f,LabelSize=24.f;
    FVector2D Size;
    bool bTile=false,bGlow=false;
};

class BOUNDLESSIMMORTALITY_API SInkCloseButton : public SInkPressable
{
public:
    SLATE_BEGIN_ARGS(SInkCloseButton):_TestId(NAME_None){}
        SLATE_ARGUMENT(FName,TestId)
        SLATE_EVENT(FSimpleDelegate,OnClicked)
    SLATE_END_ARGS()
    void Construct(const FArguments& InArgs);
    virtual FVector2D ComputeDesiredSize(float) const override{return FVector2D(72,72);}
    virtual int32 OnPaint(const FPaintArgs& Args,const FGeometry& Geometry,const FSlateRect& Culling,FSlateWindowElementList& Out,int32 Layer,const FWidgetStyle& Style,bool bParentEnabled) const override;
};

/** Progress bar; displayed value eases toward the target, and losses leave a lagging trail. */
class BOUNDLESSIMMORTALITY_API SInkProgressBar : public SLeafWidget
{
public:
    SLATE_BEGIN_ARGS(SInkProgressBar):_Style(InkUI::EBar::Gold),_Height(22.f),_Width(200.f),_FontSize(0.f){}
        SLATE_ATTRIBUTE(float,Fraction)
        SLATE_ATTRIBUTE(FText,Label)
        SLATE_ATTRIBUTE(FText,Value)
        SLATE_ARGUMENT(InkUI::EBar,Style)
        SLATE_ARGUMENT(float,Height)
        SLATE_ARGUMENT(float,Width)
        SLATE_ARGUMENT(float,FontSize)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);
    virtual void Tick(const FGeometry& AllottedGeometry,const double InCurrentTime,const float InDeltaTime) override;
    virtual FVector2D ComputeDesiredSize(float) const override{return FVector2D(Width,Height);}
    virtual int32 OnPaint(const FPaintArgs& Args,const FGeometry& Geometry,const FSlateRect& Culling,FSlateWindowElementList& Out,int32 Layer,const FWidgetStyle& Style,bool bParentEnabled) const override;

private:
    TAttribute<float> Fraction;
    TAttribute<FText> Label,Value;
    InkUI::EBar Style=InkUI::EBar::Gold;
    float Height=22.f,Width=200.f,FontSize=0.f;
    InkUI::FTween Shown,Trail;
    float LastTarget=-1.f,TrailHold=0.f;
};

/** Quality-framed item cell; legendary and above carry a slow sweep, selection a gold halo. */
class BOUNDLESSIMMORTALITY_API SInkItemSlot : public SInkPressable
{
public:
    SLATE_BEGIN_ARGS(SInkItemSlot):_Icon(nullptr),_Quality(InkUI::EQuality::Common),_Count(0),_SlotSize(112.f),_Selected(false),_IsNew(false),_Empty(false),_Locked(false),_TestId(NAME_None),_ShowName(false){}
        SLATE_ARGUMENT(const FSlateBrush*,Icon)
        SLATE_ARGUMENT(InkUI::EQuality,Quality)
        SLATE_ARGUMENT(int32,Count)
        SLATE_ARGUMENT(float,SlotSize)
        SLATE_ATTRIBUTE(bool,Selected)
        SLATE_ARGUMENT(bool,IsNew)
        SLATE_ARGUMENT(bool,Empty)
        SLATE_ARGUMENT(bool,Locked)
        SLATE_ARGUMENT(FName,TestId)
        SLATE_ARGUMENT(bool,ShowName)
        SLATE_ARGUMENT(FText,Name)
        SLATE_EVENT(FSimpleDelegate,OnClicked)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);
    virtual FVector2D ComputeDesiredSize(float) const override{return FVector2D(SlotSize,SlotSize+(bShowName?34.f:0.f));}
    virtual int32 OnPaint(const FPaintArgs& Args,const FGeometry& Geometry,const FSlateRect& Culling,FSlateWindowElementList& Out,int32 Layer,const FWidgetStyle& Style,bool bParentEnabled) const override;
    static void PaintSlot(InkUI::FPainter& P,FVector2f Pos,float Size,const FSlateBrush* Icon,InkUI::EQuality Quality,int32 Count,bool bSelected,bool bNew,bool bEmpty,bool bLocked,float Hover,float Phase);

private:
    const FSlateBrush* Icon=nullptr;
    InkUI::EQuality Quality=InkUI::EQuality::Common;
    int32 Count=0;
    float SlotSize=112.f;
    TAttribute<bool> Selected;
    bool bNew=false,bEmpty=false,bLocked=false,bShowName=false;
    FText Name;
};

/** Portrait inside the gilded ring; names without art fall back to a calligraphy initial. */
class BOUNDLESSIMMORTALITY_API SInkAvatar : public SLeafWidget
{
public:
    SLATE_BEGIN_ARGS(SInkAvatar):_Portrait(nullptr),_Diameter(120.f),_Ring(0){}
        SLATE_ARGUMENT(const FSlateBrush*,Portrait)
        SLATE_ARGUMENT(FText,Initial)
        SLATE_ARGUMENT(float,Diameter)
        /** 0 gilded frame, 1 gold rank, 2 silver rank, 3 bronze rank, 4 plain. */
        SLATE_ARGUMENT(int32,Ring)
    SLATE_END_ARGS()
    void Construct(const FArguments& InArgs);
    virtual FVector2D ComputeDesiredSize(float) const override{return FVector2D(Diameter);}
    virtual int32 OnPaint(const FPaintArgs& Args,const FGeometry& Geometry,const FSlateRect& Culling,FSlateWindowElementList& Out,int32 Layer,const FWidgetStyle& Style,bool bParentEnabled) const override;
    static void PaintAvatar(InkUI::FPainter& P,FVector2f Center,float Diameter,const FSlateBrush* Portrait,const FText& Initial,int32 Ring);
private:
    const FSlateBrush* Portrait=nullptr;
    FText Initial;
    float Diameter=120.f;
    int32 Ring=0;
};

/** Plaque tab strip; the selected lacquer plaque glides between tabs. */
class BOUNDLESSIMMORTALITY_API SInkTabBar : public SLeafWidget
{
public:
    DECLARE_DELEGATE_OneParam(FOnTab,int32);
    SLATE_BEGIN_ARGS(SInkTabBar):_Selected(0),_Height(68.f),_OnDark(false),_TestPrefix(NAME_None){}
        SLATE_ARGUMENT(TArray<FText>,Tabs)
        SLATE_ARGUMENT(int32,Selected)
        SLATE_ARGUMENT(float,Height)
        SLATE_ARGUMENT(bool,OnDark)
        SLATE_ARGUMENT(FName,TestPrefix)
        SLATE_ATTRIBUTE(TArray<bool>,Badges)
        SLATE_EVENT(FOnTab,OnSelected)
    SLATE_END_ARGS()
    void Construct(const FArguments& InArgs);
    virtual void Tick(const FGeometry& AllottedGeometry,const double InCurrentTime,const float InDeltaTime) override;
    virtual FVector2D ComputeDesiredSize(float) const override{return FVector2D(120.f*FMath::Max(1,Tabs.Num()),Height);}
    virtual int32 OnPaint(const FPaintArgs& Args,const FGeometry& Geometry,const FSlateRect& Culling,FSlateWindowElementList& Out,int32 Layer,const FWidgetStyle& Style,bool bParentEnabled) const override;
    virtual FReply OnMouseButtonDown(const FGeometry& Geometry,const FPointerEvent& Event) override;
    virtual FReply OnMouseButtonUp(const FGeometry& Geometry,const FPointerEvent& Event) override;
    virtual FReply OnMouseMove(const FGeometry& Geometry,const FPointerEvent& Event) override;
    virtual void OnMouseLeave(const FPointerEvent& Event) override{HoverIndex=INDEX_NONE;}
    virtual FCursorReply OnCursorQuery(const FGeometry& Geometry,const FPointerEvent& Event) const override;
    void Select(int32 Index,bool bNotify);
    int32 GetSelected() const{return Selected;}
    /** Test hook: simulates choosing a tab through the same path as a click. */
    static bool ClickTab(FName TestPrefix,int32 Index);

private:
    TArray<FText> Tabs;
    TAttribute<TArray<bool>> Badges;
    int32 Selected=0,HoverIndex=INDEX_NONE,PressIndex=INDEX_NONE;
    float Height=68.f;
    bool bOnDark=false;
    FName TestPrefix;
    InkUI::FTween Slide;
    FOnTab OnSelected;
    int32 IndexAt(const FGeometry& Geometry,FVector2f Local) const;
};

/** Feature page shell: crane scroll frame, calligraphy title with seal, close button, and slots. */
class BOUNDLESSIMMORTALITY_API SInkWindow : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SInkWindow):_TestId(NAME_None){}
        SLATE_ARGUMENT(FText,Title)
        SLATE_ARGUMENT(FText,Seal)
        SLATE_ARGUMENT(FName,TestId)
        SLATE_EVENT(FSimpleDelegate,OnClose)
        SLATE_NAMED_SLOT(FArguments,Header)
        SLATE_NAMED_SLOT(FArguments,Body)
        SLATE_NAMED_SLOT(FArguments,Footer)
        SLATE_NAMED_SLOT(FArguments,Overlay)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);
    virtual void Tick(const FGeometry& AllottedGeometry,const double InCurrentTime,const float InDeltaTime) override;
    virtual int32 OnPaint(const FPaintArgs& Args,const FGeometry& Geometry,const FSlateRect& Culling,FSlateWindowElementList& Out,int32 Layer,const FWidgetStyle& Style,bool bParentEnabled) const override;
    void PlayOpen();
    /** The owning root removes the page once IsClosed(); removal never happens inside this tick. */
    void PlayClose();
    bool IsClosing() const{return bClosing;}
    bool IsClosed() const{return bClosing&&!Open.bRunning;}

private:
    FText Title,Seal;
    InkUI::FTween Open,Trace;
    bool bClosing=false;
    /** Header, body, footer; header and footer insets follow the frame scale so content stays on the paper. */
    TSharedPtr<SVerticalBox> Column;
    FVector2f LaidOutFor=FVector2f::ZeroVector;
};

class BOUNDLESSIMMORTALITY_API SInkToastHost : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SInkToastHost){}
    SLATE_END_ARGS()
    void Construct(const FArguments& InArgs);
    void Push(const FText& Message,int32 Kind=0);
    virtual void Tick(const FGeometry& AllottedGeometry,const double InCurrentTime,const float InDeltaTime) override;
    virtual int32 OnPaint(const FPaintArgs& Args,const FGeometry& Geometry,const FSlateRect& Culling,FSlateWindowElementList& Out,int32 Layer,const FWidgetStyle& Style,bool bParentEnabled) const override;
    virtual FVector2D ComputeDesiredSize(float) const override{return FVector2D(10,10);}
    FText GetLatest() const{return Toasts.Num()?Toasts.Last().Message:FText::GetEmpty();}
private:
    struct FToast{FText Message;int32 Kind=0;float Age=0.f;};
    TArray<FToast> Toasts;
};

/** Centered confirm card on its own veil; Escape and the veil cancel. */
class BOUNDLESSIMMORTALITY_API SInkModal : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SInkModal):_Danger(false){}
        SLATE_ARGUMENT(FText,Title)
        SLATE_ARGUMENT(FText,Message)
        SLATE_ARGUMENT(FText,ConfirmText)
        SLATE_ARGUMENT(FText,CancelText)
        SLATE_ARGUMENT(bool,Danger)
        SLATE_EVENT(FSimpleDelegate,OnConfirm)
        SLATE_EVENT(FSimpleDelegate,OnCancel)
    SLATE_END_ARGS()
    void Construct(const FArguments& InArgs);
    virtual void Tick(const FGeometry& AllottedGeometry,const double InCurrentTime,const float InDeltaTime) override;
    virtual int32 OnPaint(const FPaintArgs& Args,const FGeometry& Geometry,const FSlateRect& Culling,FSlateWindowElementList& Out,int32 Layer,const FWidgetStyle& Style,bool bParentEnabled) const override;
    virtual FReply OnMouseButtonDown(const FGeometry& Geometry,const FPointerEvent& Event) override{return FReply::Handled();}
    void Dismiss(bool bConfirmed);
    bool IsDismissing() const{return bDismissing;}
    bool IsFinished() const{return bDismissing&&!Fade.bRunning;}
private:
    FSimpleDelegate OnConfirm,OnCancel;
    InkUI::FTween Fade;
    bool bDismissing=false;
    TSharedPtr<SInkAnimBox> Card;
};

/** Reward ceremony: rotating halo, calligraphy banner and items popping in sequence. */
class BOUNDLESSIMMORTALITY_API SInkRewardOverlay : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SInkRewardOverlay){}
        SLATE_ARGUMENT(TArray<InkUI::FReward>,Rewards)
        SLATE_ARGUMENT(FText,Title)
        SLATE_EVENT(FSimpleDelegate,OnClosed)
    SLATE_END_ARGS()
    void Construct(const FArguments& InArgs);
    virtual void Tick(const FGeometry& AllottedGeometry,const double InCurrentTime,const float InDeltaTime) override;
    virtual int32 OnPaint(const FPaintArgs& Args,const FGeometry& Geometry,const FSlateRect& Culling,FSlateWindowElementList& Out,int32 Layer,const FWidgetStyle& Style,bool bParentEnabled) const override;
    virtual FReply OnMouseButtonDown(const FGeometry& Geometry,const FPointerEvent& Event) override;
    void Close();
    bool IsClosing() const{return bClosing;}
    bool IsFinished() const{return bClosing&&!Fade.bRunning;}
private:
    TArray<InkUI::FReward> Rewards;
    FText Title;
    FSimpleDelegate OnClosed;
    InkUI::FTween Fade;
    float Age=0.f;
    bool bClosing=false;
};

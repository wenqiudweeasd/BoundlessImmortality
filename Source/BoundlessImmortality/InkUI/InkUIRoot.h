#pragma once

#include "CoreMinimal.h"
#include "InkUI/InkUIWidgets.h"

class SInkUIRoot;
class SOverlay;

namespace InkUI
{
    enum class EBase : uint8 { None, MainMenu, Hud };
    DECLARE_DELEGATE_OneParam(FOnUICommand,FName);
}

/** One screen: a base (main menu, HUD) or a page on the stack. Exit motion finishes before removal. */
class BOUNDLESSIMMORTALITY_API SInkScreen : public SCompoundWidget
{
public:
    void InitScreen(const TSharedRef<SInkUIRoot>& InRoot,FName InRoute);
    FName GetRoute() const{return Route;}
    virtual void PlayExit();
    virtual bool IsExitFinished() const;
    /** Consumes Back for inner layers (detail sheets, popovers); false lets the root pop the page. */
    virtual bool HandleBack(){return false;}
    virtual void OnModelChanged(){}
    virtual void OnLayoutChanged(){}
    virtual bool IsPage() const{return true;}
    /** Pages that block world input; the HUD does not. */
    virtual bool BlocksGameInput() const{return IsPage();}
    virtual FString Describe() const{return Route.ToString();}
    virtual void Tick(const FGeometry& AllottedGeometry,const double InCurrentTime,const float InDeltaTime) override;
    int32 SeenRevision=-1;
    FVector2f SeenSize=FVector2f::ZeroVector;

protected:
    TWeakPtr<SInkUIRoot> Root;
    FName Route;
    InkUI::FTween ExitFade;
    bool bExiting=false;
    TSharedPtr<SInkUIRoot> GetRoot() const{return Root.Pin();}
    InkUI::FModel& M() const;
    bool IsLandscape() const;
};

/** Page screens share the scroll window shell; subclasses provide header, body and footer. */
class BOUNDLESSIMMORTALITY_API SInkPageScreen : public SInkScreen
{
public:
    virtual void PlayExit() override;
    virtual bool IsExitFinished() const override;
    virtual void OnLayoutChanged() override;

protected:
    TSharedPtr<SInkWindow> Window;
    TSharedPtr<SBox> Frame;
    /** Wraps the window in the responsive frame: near full-screen portrait, centred landscape. */
    void BuildPage(const FText& Title,const FText& Seal,TSharedRef<SWidget> Header,TSharedRef<SWidget> Body,TSharedRef<SWidget> Footer,TSharedRef<SWidget> Overlay);
    void RequestClose();
};

class BOUNDLESSIMMORTALITY_API SInkUIRoot : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SInkUIRoot){}
        SLATE_ARGUMENT(TSharedPtr<InkUI::FModel>,Model)
        SLATE_EVENT(InkUI::FOnUICommand,OnCommand)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);
    virtual void Tick(const FGeometry& AllottedGeometry,const double InCurrentTime,const float InDeltaTime) override;
    virtual FReply OnKeyDown(const FGeometry& Geometry,const FKeyEvent& Event) override;
    virtual bool SupportsKeyboardFocus() const override{return true;}

    void ShowBase(InkUI::EBase InBase);
    InkUI::EBase GetBase() const{return Base;}
    bool OpenPage(FName Route);
    void CloseTopPage();
    void CloseAllPages();
    FName GetTopRoute() const;
    bool HasPages() const;
    void Toast(const FText& Text,int32 Kind=0);
    void Confirm(const FText& Title,const FText& Message,FSimpleDelegate OnConfirm,bool bDanger=false,const FText& ConfirmText=FText());
    void ShowRewards(const TArray<InkUI::FReward>& Rewards,const FText& Title=FText(),FSimpleDelegate OnClosed=FSimpleDelegate());
    void RunCommand(FName Command);
    bool HandleBack();
    bool HasModal() const;
    InkUI::FModel& Model() const{return *ModelData;}
    bool IsLandscape() const{return bLandscape;}
    FVector2f GetLogicalSize() const{return LogicalSize;}
    FString Describe() const;
    FText LatestToast() const;
    /** Called by the HUD while its menu popover is open, so the host keeps world input blocked. */
    void SetOverlayBlocking(bool bBlocking){bOverlayBlocking=bBlocking;}
    bool WantsGameInputBlocked() const;
    void FocusRoot();
    /** Runs first in the root tick, so the host can refresh live data before screens read it. */
    TFunction<void(float)> HostTick;

private:
    TSharedPtr<InkUI::FModel> ModelData;
    InkUI::FOnUICommand OnCommand;
    TSharedPtr<SOverlay> BaseLayer,PageLayer,ModalLayer;
    TSharedPtr<SInkToastHost> Toasts;
    TSharedPtr<SInkScreen> BaseScreen;
    TArray<TSharedPtr<SInkScreen>> LeavingBases;
    TArray<TSharedPtr<SInkScreen>> Pages;
    TArray<TSharedPtr<SInkScreen>> LeavingPages;
    TArray<TSharedPtr<SWidget>> Modals;
    InkUI::EBase Base=InkUI::EBase::None;
    InkUI::FTween Veil;
    FVector2f LogicalSize=FVector2f(720.f,1280.f);
    bool bLandscape=false,bOverlayBlocking=false;
    TSharedPtr<SInkScreen> CreateScreen(FName Route);
    void SyncScreen(const TSharedPtr<SInkScreen>& Screen);
};

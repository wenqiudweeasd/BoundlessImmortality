#include "InkUI/InkUIRoot.h"
#include "InkUI/InkUIScreens.h"
#include "InkUI/InkUIScreensXianTu.h"
#include "WanJian/WanJianScreen.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Layout/SSafeZone.h"

using namespace InkUI;

void SInkScreen::InitScreen(const TSharedRef<SInkUIRoot>& InRoot,FName InRoute)
{
    Root=InRoot;Route=InRoute;
    SeenRevision=InRoot->Model().Revision;
    SetCanTick(true);
    SetVisibility(EVisibility::SelfHitTestInvisible);
}

FModel& SInkScreen::M() const{return Root.Pin()->Model();}

bool SInkScreen::IsLandscape() const{const auto R=Root.Pin();return R&&R->IsLandscape();}

void SInkScreen::PlayExit()
{
    if(bExiting)return;
    bExiting=true;
    SetVisibility(EVisibility::HitTestInvisible);
    ExitFade.Start(1.f,0.f,Durations::PageClose+.06f,EEase::InCubic);
}

bool SInkScreen::IsExitFinished() const{return bExiting&&!ExitFade.bRunning;}

void SInkScreen::Tick(const FGeometry& AllottedGeometry,const double InCurrentTime,const float InDeltaTime)
{
    if(bExiting&&ExitFade.Tick(InDeltaTime))SetRenderOpacity(ExitFade.Value());
}

void SInkPageScreen::BuildPage(const FText& Title,const FText& Seal,TSharedRef<SWidget> Header,TSharedRef<SWidget> Body,TSharedRef<SWidget> Footer,TSharedRef<SWidget> Overlay)
{
    ChildSlot
    [
        SAssignNew(Frame,SBox)
        [
            SAssignNew(Window,SInkWindow).Title(Title).Seal(Seal).TestId(Route).OnClose(this,&SInkPageScreen::RequestClose)
            .Header()[Header].Body()[Body].Footer()[Footer].Overlay()[Overlay]
        ]
    ];
    OnLayoutChanged();
}

void SInkPageScreen::RequestClose(){if(const auto R=GetRoot())R->CloseTopPage();}

void SInkPageScreen::PlayExit()
{
    if(bExiting)return;
    bExiting=true;
    if(Window)Window->PlayClose();else SInkScreen::PlayExit();
}

bool SInkPageScreen::IsExitFinished() const{return Window?Window->IsClosed():SInkScreen::IsExitFinished();}

void SInkPageScreen::OnLayoutChanged()
{
    if(!Frame)return;
    const auto R=GetRoot();
    const FVector2f S=R?R->GetLogicalSize():FVector2f(720.f,1280.f);
    if(R&&R->IsLandscape())
    {
        // Landscape keeps the scroll centred and bounded instead of stretching a portrait page.
        const float W=FMath::Min(S.X-120.f,1080.f);
        Frame->SetPadding(FMargin(FMath::Max(24.f,(S.X-W)*.5f),46.f,FMath::Max(24.f,(S.X-W)*.5f),10.f));
    }
    else Frame->SetPadding(FMargin(8.f,58.f,8.f,8.f));
}

void SInkUIRoot::Construct(const FArguments& InArgs)
{
    ModelData=InArgs._Model;
    if(!ModelData){ModelData=MakeShared<FModel>();ModelData->SeedLocal();}
    OnCommand=InArgs._OnCommand;
    SetCanTick(true);
    // Layers must not swallow hits themselves: empty HUD space falls through to the game viewport.
    SetVisibility(EVisibility::SelfHitTestInvisible);
    Veil.Snap(0.f);
    TSharedPtr<SInkPaint> VeilWidget;
    // SSafeZone has no visibility default of its own, so it would be a full-screen Visible hit target.
    const EVisibility Pass=EVisibility::SelfHitTestInvisible;
    ChildSlot
    [
        SNew(SOverlay)
        +SOverlay::Slot()[SNew(SSafeZone).Visibility(Pass)[SAssignNew(BaseLayer,SOverlay)]]
        +SOverlay::Slot()
        [
            SAssignNew(VeilWidget,SInkPaint).HitTest(true).Visibility_Lambda([this]{return Pages.Num()>0&&Veil.Value()>.01f?EVisibility::Visible:EVisibility::HitTestInvisible;})
            .OnPaint_Lambda([this](FPainter& P,const FGeometry&)
            {
                const float A=Veil.Value();
                if(A<=.002f)return;
                const auto& K=Colors();
                P.VGradient(FVector2f(0.f,0.f),P.Size(),{{0.f,WithAlpha(K.InkBlack,.62f*A)},{.5f,WithAlpha(K.InkBlack,.5f*A)},{1.f,WithAlpha(K.InkBlack,.78f*A)}});
            })
        ]
        +SOverlay::Slot()[SNew(SSafeZone).Visibility(Pass)[SAssignNew(PageLayer,SOverlay)]]
        +SOverlay::Slot()[SAssignNew(ModalLayer,SOverlay)]
        +SOverlay::Slot()[SNew(SSafeZone).Visibility(Pass)[SAssignNew(Toasts,SInkToastHost)]]
    ];
}

TSharedPtr<SInkScreen> SInkUIRoot::CreateScreen(FName Route)
{
    const TSharedRef<SInkUIRoot> Self=SharedThis(this);
    if(Route==TEXT("MainMenu"))return SNew(SInkMainMenuScreen).Root(Self);
    if(Route==TEXT("Hud"))return SNew(SInkHudScreen).Root(Self);
    if(Route==TEXT("Character"))return SNew(SInkCharacterScreen).Root(Self);
    if(Route==TEXT("Inventory"))return SNew(SInkInventoryScreen).Root(Self);
    if(Route==TEXT("Leaderboard"))return SNew(SInkRankScreen).Root(Self);
    if(Route==TEXT("SignIn"))return SNew(SInkSignInScreen).Root(Self);
    if(Route==TEXT("Activities"))return SNew(SInkActivityScreen).Root(Self);
    if(Route==TEXT("Journey"))return SNew(SInkJourneyScreen).Root(Self);
    if(Route==TEXT("Settings"))return SNew(SInkSettingsScreen).Root(Self);
    if(Route==TEXT("Saves"))return SNew(SInkSavesScreen).Root(Self);
    if(Route==TEXT("World"))return SNew(SInkWorldScreen).Root(Self);
    if(Route==TEXT("Resources"))return SNew(SInkRaceScreen).Root(Self);
    if(Route==TEXT("Wuling"))return SNew(SInkGuildScreen).Root(Self);
    if(Route==TEXT("Market"))return SNew(SInkMarketScreen).Root(Self);
    if(Route==TEXT("SwordSpirit"))return SNew(SInkSwordBookScreen).Root(Self);
    if(Route==TEXT("Methods"))return SNew(SInkMethodsScreen).Root(Self);
    if(Route==TEXT("Tribulation"))return SNew(SInkTribulationScreen).Root(Self);
    if(Route==TEXT("JadeShop"))return SNew(SInkShopScreen).Root(Self).Shop(0);
    if(Route==TEXT("WulingShop"))return SNew(SInkShopScreen).Root(Self).Shop(1);
    if(Route==TEXT("XianDao"))return SNew(SInkHallScreen).Root(Self);
    if(Route==TEXT("Fortune"))return SNew(SInkFortuneScreen).Root(Self);
    if(Route==TEXT("Breakthrough"))return SNew(SInkBreakthroughScreen).Root(Self);
    if(Route==TEXT("ItemSources"))return SNew(SInkItemSourcesScreen).Root(Self);
    if(Route==TEXT("GuildConstruction"))return SNew(SInkGuildBuildScreen).Root(Self);
    if(Route==TEXT("EquipmentFabao"))return SNew(SInkGearScreen).Root(Self);
    if(Route==TEXT("WanJian"))return SNew(SInkWanJianScreen).Root(Self);
    return nullptr;
}

void SInkUIRoot::SyncScreen(const TSharedPtr<SInkScreen>& Screen)
{
    if(!Screen)return;
    if(!Screen->SeenSize.Equals(LogicalSize,1.f)){Screen->SeenSize=LogicalSize;Screen->OnLayoutChanged();}
    if(Screen->SeenRevision!=ModelData->Revision){Screen->SeenRevision=ModelData->Revision;Screen->OnModelChanged();}
}

void SInkUIRoot::Tick(const FGeometry& AllottedGeometry,const double InCurrentTime,const float InDeltaTime)
{
    if(HostTick)HostTick(InDeltaTime);
    const FVector2f Size(AllottedGeometry.GetLocalSize());
    if(Size.X>1.f&&Size.Y>1.f)
    {
        LogicalSize=Size;
        // Hysteresis keeps a dragged window near 5:4 from flipping layouts every frame.
        const float Ratio=Size.X/Size.Y,Edge=Metrics::LandscapeRatio+(bLandscape?-Metrics::OrientationHysteresis:Metrics::OrientationHysteresis);
        bLandscape=Ratio>=Edge;
    }
    SyncScreen(BaseScreen);
    for(const auto& Page:Pages)SyncScreen(Page);
    // Removal happens here, before children tick, never inside a child's own tick.
    for(int32 I=LeavingPages.Num()-1;I>=0;--I)if(LeavingPages[I]->IsExitFinished()){PageLayer->RemoveSlot(LeavingPages[I].ToSharedRef());LeavingPages.RemoveAt(I);}
    for(int32 I=LeavingBases.Num()-1;I>=0;--I)if(LeavingBases[I]->IsExitFinished()){BaseLayer->RemoveSlot(LeavingBases[I].ToSharedRef());LeavingBases.RemoveAt(I);}
    for(int32 I=Modals.Num()-1;I>=0;--I)
    {
        const TSharedPtr<SWidget>& W=Modals[I];
        bool bDone=false;
        if(W->GetType()==TEXT("SInkModal"))bDone=StaticCastSharedPtr<SInkModal>(W)->IsFinished();
        else if(W->GetType()==TEXT("SInkRewardOverlay"))bDone=StaticCastSharedPtr<SInkRewardOverlay>(W)->IsFinished();
        if(bDone){ModalLayer->RemoveSlot(W.ToSharedRef());Modals.RemoveAt(I);}
    }
    const float Wanted=Pages.Num()>0?1.f:0.f;
    if(!FMath::IsNearlyEqual(Veil.To,Wanted))Veil.Retarget(Wanted,Durations::VeilFade);
    Veil.Tick(InDeltaTime);
}

FReply SInkUIRoot::OnKeyDown(const FGeometry& Geometry,const FKeyEvent& Event)
{
    const FKey Key=Event.GetKey();
    if(Key==EKeys::Escape||Key==EKeys::Gamepad_FaceButton_Right||Key==EKeys::Android_Back)
        return HandleBack()?FReply::Handled():FReply::Unhandled();
    return FReply::Unhandled();
}

void SInkUIRoot::FocusRoot()
{
    if(FSlateApplication::IsInitialized())FSlateApplication::Get().SetKeyboardFocus(SharedThis(this),EFocusCause::SetDirectly);
}

void SInkUIRoot::ShowBase(EBase InBase)
{
    if(InBase==Base&&BaseScreen)return;
    Base=InBase;
    CloseAllPages();
    if(BaseScreen){BaseScreen->PlayExit();LeavingBases.Add(BaseScreen);BaseScreen.Reset();}
    if(InBase==EBase::None)return;
    BaseScreen=CreateScreen(InBase==EBase::MainMenu?FName(TEXT("MainMenu")):FName(TEXT("Hud")));
    if(BaseScreen)BaseLayer->AddSlot()[BaseScreen.ToSharedRef()];
    // In play the viewport keeps keyboard focus so movement keys reach the pawn directly.
    if(InBase==EBase::MainMenu)FocusRoot();
}

bool SInkUIRoot::OpenPage(FName Route)
{
    if(GetTopRoute()==Route)return true;
    const int32 Existing=Pages.IndexOfByPredicate([&](const TSharedPtr<SInkScreen>& P){return P->GetRoute()==Route;});
    if(Existing!=INDEX_NONE){while(Pages.Num()>Existing+1)CloseTopPage();return true;}
    const TSharedPtr<SInkScreen> Screen=CreateScreen(Route);
    if(!Screen)return false;
    if(Pages.Num())Pages.Last()->SetVisibility(EVisibility::HitTestInvisible);
    Pages.Add(Screen);
    PageLayer->AddSlot()[Screen.ToSharedRef()];
    FocusRoot();
    return true;
}

void SInkUIRoot::CloseTopPage()
{
    if(!Pages.Num())return;
    const TSharedPtr<SInkScreen> Top=Pages.Pop();
    Top->PlayExit();LeavingPages.Add(Top);
    if(Pages.Num())Pages.Last()->SetVisibility(EVisibility::SelfHitTestInvisible);
    FocusRoot();
}

void SInkUIRoot::CloseAllPages(){while(Pages.Num())CloseTopPage();}

FName SInkUIRoot::GetTopRoute() const{return Pages.Num()?Pages.Last()->GetRoute():(BaseScreen?BaseScreen->GetRoute():NAME_None);}

bool SInkUIRoot::HasPages() const{return Pages.Num()>0;}

bool SInkUIRoot::HasModal() const
{
    for(const auto& W:Modals)
    {
        if(W->GetType()==TEXT("SInkModal")&&!StaticCastSharedPtr<SInkModal>(W)->IsDismissing())return true;
        if(W->GetType()==TEXT("SInkRewardOverlay")&&!StaticCastSharedPtr<SInkRewardOverlay>(W)->IsClosing())return true;
    }
    return false;
}

void SInkUIRoot::Toast(const FText& Text,int32 Kind){if(Toasts)Toasts->Push(Text,Kind);}

FText SInkUIRoot::LatestToast() const{return Toasts?Toasts->GetLatest():FText::GetEmpty();}

void SInkUIRoot::Confirm(const FText& Title,const FText& Message,FSimpleDelegate OnConfirm,bool bDanger,const FText& ConfirmText)
{
    const TSharedRef<SInkModal> Modal=SNew(SInkModal).Title(Title).Message(Message).ConfirmText(ConfirmText).Danger(bDanger).OnConfirm(OnConfirm);
    ModalLayer->AddSlot()[Modal];
    Modals.Add(Modal);
    FocusRoot();
}

void SInkUIRoot::ShowRewards(const TArray<FReward>& Rewards,const FText& Title,FSimpleDelegate OnClosed)
{
    if(!Rewards.Num()){OnClosed.ExecuteIfBound();return;}
    const TSharedRef<SInkRewardOverlay> Overlay=SNew(SInkRewardOverlay).Rewards(Rewards).Title(Title).OnClosed(OnClosed);
    ModalLayer->AddSlot()[Overlay];
    Modals.Add(Overlay);
    FocusRoot();
}

void SInkUIRoot::RunCommand(FName Command){OnCommand.ExecuteIfBound(Command);}

bool SInkUIRoot::HandleBack()
{
    for(int32 I=Modals.Num()-1;I>=0;--I)
    {
        const TSharedPtr<SWidget>& W=Modals[I];
        if(W->GetType()==TEXT("SInkModal")){const auto M=StaticCastSharedPtr<SInkModal>(W);if(!M->IsDismissing()){M->Dismiss(false);return true;}}
        else if(W->GetType()==TEXT("SInkRewardOverlay")){const auto R=StaticCastSharedPtr<SInkRewardOverlay>(W);if(!R->IsClosing()){R->Close();return true;}}
    }
    if(Pages.Num()){if(!Pages.Last()->HandleBack())CloseTopPage();return true;}
    return BaseScreen&&BaseScreen->HandleBack();
}

bool SInkUIRoot::WantsGameInputBlocked() const
{
    return Pages.Num()>0||HasModal()||bOverlayBlocking||(BaseScreen&&BaseScreen->BlocksGameInput());
}

FString SInkUIRoot::Describe() const
{
    auto Quote=[](const FString& S){return TEXT("\"")+S.Replace(TEXT("\\"),TEXT("/")).Replace(TEXT("\""),TEXT("'"))+TEXT("\"");};
    TArray<FString> PageNames,Ids;
    for(const auto& P:Pages)PageNames.Add(Quote(P->Describe()));
    for(const FName Id:Testing::VisibleIds())Ids.Add(Quote(Id.ToString()));
    return FString::Printf(TEXT("{\"base\":%s,\"top\":%s,\"pages\":[%s],\"modal\":%s,\"landscape\":%s,\"size\":[%.0f,%.0f],\"toast\":%s,\"revision\":%d,\"blocking\":%s,\"ids\":[%s]}"),
        *Quote(BaseScreen?BaseScreen->Describe():TEXT("")),*Quote(GetTopRoute().ToString()),*FString::Join(PageNames,TEXT(",")),HasModal()?TEXT("true"):TEXT("false"),
        bLandscape?TEXT("true"):TEXT("false"),LogicalSize.X,LogicalSize.Y,*Quote(LatestToast().ToString()),ModelData->Revision,WantsGameInputBlocked()?TEXT("true"):TEXT("false"),*FString::Join(Ids,TEXT(",")));
}

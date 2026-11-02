#include "InkFrontendEditorLibrary.h"
#include "LiveWeapons/LiveAnchorController.h"
#include "LiveWeapons/LiveWeaponGameMode.h"
#include "LiveWeapons/LiveWeaponSwarm.h"
#include "LiveWeapons/LiveAudienceAdapterComponent.h"
#include "LiveWeapons/LiveMonster.h"
#include "LiveWeapons/LiveGMOverlay.h"
#include "LiveWeapons/LiveGMControlPanel.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "Misc/CommandLine.h"
#include "Misc/ScopeExit.h"
#include "Components/WidgetSwitcher.h"
#include "Blueprint/UserWidget.h"
#include "Framework/Application/SlateApplication.h"
#include "Layout/WidgetPath.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"
#include "Engine/GameViewportClient.h"
#include "Slate/SceneViewport.h"
#include "Editor.h"
#include "PlayInEditorDataTypes.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "Widgets/SWindow.h"
#include "Widgets/SViewport.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"

namespace InkEditorPIE
{
    bool IsEffectivelyVisible(const UWidget* Widget)
    {
        if(!Widget||!Widget->IsVisible())return false;
        const UWidget* Ancestor=Widget;TSet<const UWidget*> Seen;
        while(Ancestor&&!Seen.Contains(Ancestor))
        {
            Seen.Add(Ancestor);const UWidget* Parent=Ancestor->GetParent();
            if(!Parent){const auto* Owner=Ancestor->GetTypedOuter<UUserWidget>();if(Owner&&Owner!=Ancestor){Ancestor=Owner;if(!Owner->IsVisible())return false;continue;}break;}
            if(!Parent->IsVisible())return false;
            if(const auto* Switcher=Cast<UWidgetSwitcher>(Parent))if(Switcher->GetActiveWidget()!=Ancestor)return false;
            Ancestor=Parent;
        }
        return true;
    }

    bool MoveTo(UWidget* Widget)
    {
        if(!IsEffectivelyVisible(Widget)||!FSlateApplication::IsInitialized()||!Widget->GetCachedWidget())return false;
        const auto& Geometry=Widget->GetCachedGeometry();if(Geometry.GetLocalSize().IsNearlyZero())return false;
        const FVector2D Position=Geometry.LocalToAbsolute(Geometry.GetLocalSize()*.5f);
        auto& Slate=FSlateApplication::Get();const FVector2D Previous=Slate.GetCursorPos();Slate.SetCursorPos(Position);
        Slate.ProcessMouseMoveEvent(FPointerEvent(FSlateApplication::CursorPointerIndex,Position,Previous,TSet<FKey>{},EKeys::Invalid,0,FModifierKeysState()));
        return Slate.LocateWindowUnderMouse(Position,Slate.GetInteractiveTopLevelWindows()).ContainsWidget(Widget->GetCachedWidget().Get());
    }
}

bool UInkFrontendEditorLibrary::ClickGMAction(ULiveGMOverlayWidget* Widget,FName Action)
{
    if(!Widget||!Widget->Controls||!Widget->bPanelOpen||!FSlateApplication::IsInitialized())return false;
    for(auto Button:Widget->Controls->Buttons)if(Button&&Button->Action==Action&&Button->GetIsEnabled())
    {
        if(!InkEditorPIE::MoveTo(Button))continue;const auto& G=Button->GetCachedGeometry();const FVector2D P=G.LocalToAbsolute(G.GetLocalSize()*.5f);auto& Slate=FSlateApplication::Get();
        const auto Hit=Slate.LocateWindowUnderMouse(P,Slate.GetInteractiveTopLevelWindows());if(!Hit.ContainsWidget(Button->GetCachedWidget().Get()))continue;
        const bool Down=Slate.ProcessMouseButtonDownEvent(nullptr,FPointerEvent(FSlateApplication::CursorPointerIndex,P,P,TSet<FKey>{EKeys::LeftMouseButton},EKeys::LeftMouseButton,0,FModifierKeysState()));Slate.ProcessMouseButtonUpEvent(FPointerEvent(FSlateApplication::CursorPointerIndex,P,P,TSet<FKey>{},EKeys::LeftMouseButton,0,FModifierKeysState()));return Down;
    }
    return false;
}

bool UInkFrontendEditorLibrary::PressFocusedFrontendKey(FKey Key)
{
    if(!FSlateApplication::IsInitialized())return false;
    auto& Slate=FSlateApplication::Get();const FKeyEvent Event(Key,FModifierKeysState(),0,false,0,0);
    const bool Handled=Slate.ProcessKeyDownEvent(Event);Slate.ProcessKeyUpEvent(Event);return Handled;
}

bool UInkFrontendEditorLibrary::SetPIEViewportSize(int32 Width,int32 Height)
{
    if(!GEditor||!GEditor->GameViewport||Width<320||Height<320)return false;
    auto* Viewport=GEditor->GameViewport->GetGameViewport();if(!Viewport)return false;
    const FIntPoint Current=Viewport->GetSizeXY();
    auto Window=Viewport->FindWindow();if(!Window)return false;
    // PIE windows inherit desktop-sized limits. QA must also exercise phone
    // resolutions taller than this monitor without changing editor preferences.
    auto Limits=Window->GetSizeLimits();
    Limits.SetMaxWidth(FMath::Max(Limits.GetMaxWidth().Get(0.f),float(Width+128)));
    Limits.SetMaxHeight(FMath::Max(Limits.GetMaxHeight().Get(0.f),float(Height+128)));
    Window->SetSizeLimits(Limits);
    // PIE adds a small Slate border; resize by the measured viewport delta instead of
    // treating the outer client area as the rendered game area.
    Window->Resize(Window->GetClientSizeInScreen()+FVector2D(Width-Current.X,Height-Current.Y));return true;
}

bool UInkFrontendEditorLibrary::BeginFrontendPIE(int32 Width,int32 Height)
{
    if(!GEditor||GEditor->PlayWorld||Width<320||Height<320)return false;
    auto* Settings=DuplicateObject<ULevelEditorPlaySettings>(GetDefault<ULevelEditorPlaySettings>(),GetTransientPackage());
    Settings->NewWindowWidth=Width;Settings->NewWindowHeight=Height;Settings->CenterNewWindow=true;Settings->GameGetsMouseControl=true;
    FRequestPlaySessionParams Params;Params.WorldType=EPlaySessionWorldType::PlayInEditor;Params.EditorPlaySettings=Settings;Params.bAllowOnlineSubsystem=false;
    GEditor->RequestPlaySession(Params);return true;
}

bool UInkFrontendEditorLibrary::CaptureFrontendViewport(const FString& Filename)
{
    if(!GEditor||!GEditor->GameViewport||!FSlateApplication::IsInitialized())return false;
    const auto ViewportWidget=GEditor->GameViewport->GetGameViewportWidget();if(!ViewportWidget)return false;
    TArray<FColor> Pixels;FIntVector Size;
    const auto Window=FSlateApplication::Get().FindWidgetWindow(ViewportWidget.ToSharedRef());if(!Window)return false;
    // Repaint from the Slate window root. Subtree screenshots reuse cached child
    // layers without their parent clipping state and can omit parts of glyphs.
    if(!FSlateApplication::Get().TakeScreenshot(Window.ToSharedRef(),Pixels,Size)||Size.X<=0||Size.Y<=0)return false;
    TArray64<uint8> WindowPng;FImageUtils::PNGCompressImageArray(Size.X,Size.Y,Pixels,WindowPng);
    FFileHelper::SaveArrayToFile(WindowPng,*Filename.Replace(TEXT(".png"),TEXT("-window.png")));
    const auto& Geometry=ViewportWidget->GetCachedGeometry();
    const auto Origin=Geometry.GetAbsolutePosition()-Window->GetCachedGeometry().GetAbsolutePosition();
    const auto Extent=Geometry.GetAbsoluteSize();
    const int32 X=FMath::Clamp(FMath::RoundToInt(Origin.X),0,Size.X-1),Y=FMath::Clamp(FMath::RoundToInt(Origin.Y),0,Size.Y-1);
    const int32 W=FMath::Clamp(FMath::RoundToInt(Extent.X),1,Size.X-X),H=FMath::Clamp(FMath::RoundToInt(Extent.Y),1,Size.Y-Y);
    TArray<FColor> Cropped;Cropped.SetNumUninitialized(W*H);
    for(int32 Row=0;Row<H;++Row)FMemory::Memcpy(Cropped.GetData()+Row*W,Pixels.GetData()+(Y+Row)*Size.X+X,W*sizeof(FColor));
    Pixels=MoveTemp(Cropped);Size=FIntVector(W,H,0);
    TArray64<uint8> Png;FImageUtils::PNGCompressImageArray(Size.X,Size.Y,Pixels,Png);
    return FFileHelper::SaveArrayToFile(Png,*Filename);
}

FString UInkFrontendEditorLibrary::VerifyDevelopmentMutationRefusal(UObject* Context)
{
    UWorld* World=GEngine->GetWorldFromContextObject(Context,EGetWorldErrorMode::ReturnNull);auto* Mode=World?World->GetAuthGameMode<ALiveWeaponGameMode>():nullptr;
    if(!Mode||!Mode->Audience||!Mode->Swarm)return TEXT("{}");
    auto* PC=Cast<ALiveAnchorController>(World->GetFirstPlayerController());if(!PC)return TEXT("{}");
    int32 BeforeMonsters=0;for(TActorIterator<ALiveMonster> It(World);It;++It)++BeforeMonsters;
    const int32 BeforeAudience=Mode->Swarm->GetParticipantCount();
    const FString SavedLine=FCommandLine::Get();ON_SCOPE_EXIT{FCommandLine::Set(*SavedLine);};FCommandLine::Set(TEXT("-GWLocalTest"));
    const bool ClearAccepted=Mode->Audience->ClearSimulatedAudience();PC->LiveSpawnMonsters(7);PC->LiveSkill(TEXT("viewer_0000"),TEXT("QA_Unauthorized"));PC->LiveAudience(10);PC->LiveComment(TEXT("bad_id"),TEXT("加入"));
    int32 AfterMonsters=0;for(TActorIterator<ALiveMonster> It(World);It;++It)++AfterMonsters;
    auto Root=MakeShared<FJsonObject>();Root->SetBoolField(TEXT("clear_refused"),!ClearAccepted);Root->SetBoolField(TEXT("f4_refused"),BeforeMonsters==AfterMonsters);Root->SetBoolField(TEXT("audience_refused"),BeforeAudience==Mode->Swarm->GetParticipantCount());
    FString Result;FJsonSerializer::Serialize(Root,TJsonWriterFactory<>::Create(&Result));return Result;
}

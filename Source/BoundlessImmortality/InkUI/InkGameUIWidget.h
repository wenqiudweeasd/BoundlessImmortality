#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Frontend/InkFrontendTypes.h"
#include "InkGameUIWidget.generated.h"

class AInkMenuDirector;
class SInkUIRoot;
namespace InkUI{class FModel;}

/**
 * Viewport host for the Slate xianxia UI. Owns the presentation model, relays UI commands to the
 * menu director and gameplay components, and pulls live identity/vitals while in play.
 * Live-service features stay on local verification data until the SDK is integrated.
 */
UCLASS()
class BOUNDLESSIMMORTALITY_API UInkGameUIWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    UInkGameUIWidget(const FObjectInitializer& ObjectInitializer);

    void Configure(AInkMenuDirector* InDirector);
    UFUNCTION(BlueprintCallable,Category="Ink UI") void ShowMainMenu();
    UFUNCTION(BlueprintCallable,Category="Ink UI") void ShowHud();
    UFUNCTION(BlueprintCallable,Category="Ink UI") bool OpenRoute(FName Route);
    UFUNCTION(BlueprintCallable,Category="Ink UI") void CloseTopRoute();
    UFUNCTION(BlueprintCallable,Category="Ink UI") void ShowToast(const FText& Message,int32 Kind=0);
    UFUNCTION(BlueprintCallable,Category="Ink UI") void SetReducedMotion(bool bReduced);
    UFUNCTION(BlueprintPure,Category="Ink UI") FName GetTopRoute() const;
    UFUNCTION(BlueprintPure,Category="Ink UI") bool WantsGameInputBlocked() const;
    /** The HUD is on screen with no page over it, so its identity card already shows the anchor. */
    UFUNCTION(BlueprintPure,Category="Ink UI") bool IsHudVisible() const;
    UFUNCTION(BlueprintPure,Category="Ink UI") bool IsReducedMotion() const;

    /** Live data from the director's publish loop; unchanged values do not rebuild pages. */
    UFUNCTION(BlueprintCallable,Category="Ink UI|Data") void SetCharacterData(const FText& Name,const TArray<FInkCharacterAttribute>& Attributes);
    UFUNCTION(BlueprintCallable,Category="Ink UI|Data") void SetLeaderboardEntries(const TArray<FInkLeaderboardEntry>& Entries);

    /** Test hooks: clicks travel through real Slate hit testing on registered controls. */
    UFUNCTION(BlueprintCallable,Category="Ink UI|Test") bool ClickTestTarget(FName Id);
    UFUNCTION(BlueprintCallable,Category="Ink UI|Test") bool SelectTab(FName TabBar,int32 Index);
    UFUNCTION(BlueprintCallable,Category="Ink UI|Test") bool PressBack();
    UFUNCTION(BlueprintCallable,Category="Ink UI|Test") FString DescribeUI() const;
    /** Scrolls the nearest scroll box so a registered control is in view (it is clickable a frame later). */
    UFUNCTION(BlueprintCallable,Category="Ink UI|Test") bool RevealTestTarget(FName Id);
    /** Realm index the HUD shows (pulled from the ledger five times a second); -1 before a game runs. */
    UFUNCTION(BlueprintPure,Category="Ink UI|Test") int32 GetPlayerRealmIndex() const;
    /** Presentation-only override for screenshots of cultivation and damage states. */
    UFUNCTION(BlueprintCallable,Category="Ink UI|Test") void SetPreviewVitals(float HealthFraction,float QiFraction,bool bCultivating);

protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void ReleaseSlateResources(bool bReleaseChildren) override;

private:
    UPROPERTY(Transient) TObjectPtr<AInkMenuDirector> Director;
    TSharedPtr<SInkUIRoot> Root;
    TSharedPtr<InkUI::FModel> Model;
    int32 PendingBase=0;
    bool bInputBlocked=false;
    bool bPreviewVitals=false;
    float PullElapsed=0.f;
    void HostTick(float DeltaSeconds);
    void HandleCommand(FName Command);
    void PullLiveState();
    void PullSaves();
    void PullSettings();
    void ApplySettings();
    /** 仙途 ledger: notices to toasts/rewards, wallet projected into the bag. */
    void PullXianTu();
    void ProjectWallet();
    /** Anchor buffs, debuffs and the seven-star shield for the HUD identity card. */
    void PullBuffs();
    /** Audience spirits (mesh, special form, skills) for the 剑灵谱 page. */
    void PullSpirits();
    float SpiritElapsed=0.f;
    int32 WalletRev=-1;
};

#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LiveWeapons/SpecialSwords/LiveSpecialSwordTypes.h"
#include "LiveSwordTestPanel.generated.h"
class ALiveSwordTestGameMode;
class SLiveSwordTestPanel;
/**
 * 试剑台: the sword test range's dock. One chip per special sword calls it into the fight or dismisses it (showing its
 * damage while out); the header counts the swords out, calls or dismisses all five and folds the rest away. Below: the
 * realm the swords fight at (arrows step all five by major realm) beside the 剑谱 codex (skills and attribute sheets at
 * the sword's realm, which it can also change per sword); the nearest scarecrow with 残血 (14%, for devours) and 复原;
 * the 渡劫 button, which turns into 助力 +10 while a tribulation lasts, its outcome switch (random, always fail to watch
 * the backlash, always succeed) and a status line. Ink plates over the scarecrows show their realm attributes.
 * Painted with the Ink UI kit so it matches the game.
 * Control ids: SwordRange.<Kind> (SoulEater, Inorganic, DivineBlade, SevenStar, Godslayer), SwordRange.RealmPrev/RealmNext,
 * SwordRange.Codex, SwordRange.Low, SwordRange.Heal, SwordRange.Trib, SwordRange.TribRoll, SwordRange.All, SwordRange.None,
 * SwordRange.Fold; codex ids SwordCodex.<Kind|Normal>, SwordCodex.RealmPrev/RealmNext, SwordCodex.Close, SwordCodex.Veil.
 */
UCLASS()
class BOUNDLESSIMMORTALITY_API ULiveSwordTestPanel : public UUserWidget
{
    GENERATED_BODY()
public:
    void Configure(ALiveSwordTestGameMode* InMode){Mode=InMode;}
    /** Test hook: clicks a panel control through real Slate hit testing. */
    UFUNCTION(BlueprintCallable,Category="Test") bool ClickTestTarget(FName Id);
    UFUNCTION(BlueprintPure,Category="Test") bool IsCodexOpen()const;
    /** The form the codex shows; None is the normal spirit page. */
    UFUNCTION(BlueprintPure,Category="Test") ELiveSpecialSword GetCodexSelection()const;
    /** Realm index the codex sheet is evaluated at. */
    UFUNCTION(BlueprintPure,Category="Test") int32 GetCodexRealm()const;
protected:
    virtual TSharedRef<SWidget> RebuildWidget()override;
    virtual void ReleaseSlateResources(bool bReleaseChildren)override;
private:
    TWeakObjectPtr<ALiveSwordTestGameMode> Mode;
    TSharedPtr<SLiveSwordTestPanel> Panel;
};

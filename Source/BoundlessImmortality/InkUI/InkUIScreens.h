#pragma once

#include "CoreMinimal.h"
#include "InkUI/InkUIRoot.h"

class SScrollBox;
class SUniformGridPanel;
class SVerticalBox;

#define INK_SCREEN_ARGS(ClassName) \
    SLATE_BEGIN_ARGS(ClassName){} \
        SLATE_ARGUMENT(TSharedPtr<SInkUIRoot>,Root) \
    SLATE_END_ARGS() \
    void Construct(const FArguments& InArgs);

/** Title calligraphy over the live meditation scene; buttons rise in sequence. */
class BOUNDLESSIMMORTALITY_API SInkMainMenuScreen : public SInkScreen
{
public:
    INK_SCREEN_ARGS(SInkMainMenuScreen)
    virtual bool IsPage() const override{return false;}
    virtual bool BlocksGameInput() const override{return true;}
};

/** Edge HUD: identity top-left, three entries top-right, guide card, five-entry bottom bar. */
class BOUNDLESSIMMORTALITY_API SInkHudScreen : public SInkScreen
{
public:
    INK_SCREEN_ARGS(SInkHudScreen)
    virtual bool IsPage() const override{return false;}
    virtual bool BlocksGameInput() const override{return bMenuOpen;}
    virtual bool HandleBack() override;
    virtual void Tick(const FGeometry& AllottedGeometry,const double InCurrentTime,const float InDeltaTime) override;
    virtual FString Describe() const override;
private:
    TSharedPtr<SInkAnimBox> MenuPopover;
    bool bMenuOpen=false;
    bool bGuideCollapsed=false;
    double LastQiPulse=0;
    float QiGainAccum=0.f;
    struct FFloater{float Age=0.f;float X=0.f;FString Text;};
    TArray<FFloater> Floaters;
    void ToggleMenu(bool bOpen);
    void PaintBackdrop(InkUI::FPainter& P,const FGeometry& G) const;
    void PaintIdentity(InkUI::FPainter& P,const FGeometry& G) const;
    /** Buff/debuff glyphs under the vitals: machine-soul layers, shields, regen, method guard, poisons. */
    void PaintBuffs(InkUI::FPainter& P) const;
};

class BOUNDLESSIMMORTALITY_API SInkCharacterScreen : public SInkPageScreen
{
public:
    INK_SCREEN_ARGS(SInkCharacterScreen)
    virtual void OnModelChanged() override;
private:
    int32 Tab=0;
    int32 SeenAttributes=-1,SeenItems=-1;
    TSharedPtr<SBox> Content;
    void ShowTab(int32 Index);
    TSharedRef<SWidget> BuildAttributes();
    TSharedRef<SWidget> BuildEquipment();
    TSharedRef<SWidget> BuildRealm();
};

class BOUNDLESSIMMORTALITY_API SInkInventoryScreen : public SInkPageScreen
{
public:
    INK_SCREEN_ARGS(SInkInventoryScreen)
    virtual void OnModelChanged() override;
    virtual void OnLayoutChanged() override;
    virtual bool HandleBack() override;
    virtual FString Describe() const override;
private:
    int32 Category=0;
    int32 SeenItems=-1;
    FGuid SelectedKey;
    TSharedPtr<SBox> GridBox;
    TSharedPtr<SBox> DetailBox;
    TSharedPtr<SInkAnimBox> DetailSheet;
    void RebuildGrid();
    void Select(const FGuid& Key);
    void RebuildDetail();
    void UseSelected();
};

class BOUNDLESSIMMORTALITY_API SInkRankScreen : public SInkPageScreen
{
public:
    INK_SCREEN_ARGS(SInkRankScreen)
    virtual void OnModelChanged() override;
private:
    int32 Board=0;
    int32 SeenBoards=-1;
    TSharedPtr<SBox> Content;
    TSharedPtr<SBox> SelfRow;
    void Rebuild();
};

class BOUNDLESSIMMORTALITY_API SInkSignInScreen : public SInkPageScreen
{
public:
    INK_SCREEN_ARGS(SInkSignInScreen)
    virtual void OnModelChanged() override;
    virtual void OnLayoutChanged() override;
private:
    TSharedPtr<SBox> Content;
    int32 StampDay=INDEX_NONE;
    int32 SeenSignIn=-1;
    /** The entrance plays once; later rebuilds (claims, rotation) only animate what changed. */
    bool bEntered=false;
    bool bBuiltLandscape=false;
    void Rebuild();
    void Claim();
};

class BOUNDLESSIMMORTALITY_API SInkActivityScreen : public SInkPageScreen
{
public:
    INK_SCREEN_ARGS(SInkActivityScreen)
    virtual void OnModelChanged() override;
private:
    int32 Category=0;
    int32 SeenActs=-1;
    TSharedPtr<SBox> Content;
    void Rebuild();
};

class BOUNDLESSIMMORTALITY_API SInkJourneyScreen : public SInkPageScreen
{
public:
    INK_SCREEN_ARGS(SInkJourneyScreen)
};

class BOUNDLESSIMMORTALITY_API SInkSettingsScreen : public SInkPageScreen
{
public:
    INK_SCREEN_ARGS(SInkSettingsScreen)
private:
    InkUI::FSettings Draft;
    TSharedPtr<SBox> Content;
    void Rebuild();
};

class BOUNDLESSIMMORTALITY_API SInkSavesScreen : public SInkPageScreen
{
public:
    INK_SCREEN_ARGS(SInkSavesScreen)
    virtual void OnModelChanged() override;
private:
    TSharedPtr<SBox> Content;
    int32 SeenSaves=-1;
    void Rebuild();
};

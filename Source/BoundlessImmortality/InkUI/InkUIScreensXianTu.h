#pragma once

#include "CoreMinimal.h"
#include "InkUI/InkUIRoot.h"
#include "InkUI/InkUIScreens.h"

class UXianTuSubsystem;
struct FXianTuSave;
class SEditableTextBox;

/**
 * Shared shell for the 仙途 pages: the body rebuilds only when the ledger's structure changes, the first
 * build plays the staggered entrance and later rebuilds swap in place; timers read the live ledger.
 */
class BOUNDLESSIMMORTALITY_API SInkXianTuPage : public SInkPageScreen
{
public:
    virtual void OnModelChanged() override;
    virtual void OnLayoutChanged() override;
protected:
    TSharedPtr<SBox> Body,Footer;
    int32 SeenRev=-1;
    bool bEntered=false,bBuiltLandscape=false;
    int32 Tab=0,Page=0,Pick0=0,Pick1=0,Pick2=0;
    TSharedPtr<SEditableTextBox> Input;
    UXianTuSubsystem* X() const;
    const FXianTuSave* S() const;
    virtual int32 Revision() const;
    virtual void Rebuild(){}
    /** Overlay covers the whole window above the body (celebrations); it should not take clicks. */
    void BuildShell(const FText& Title,const FText& Seal,TSharedRef<SWidget> Header,TSharedRef<SWidget> Overlay=SNullWidget::NullWidget);
    bool Exec(const FString& Verb,const TArray<FString>& Args=TArray<FString>());
    TSharedRef<SWidget> Appear(TSharedRef<SWidget> Widget,int32 Order) const;
    TSharedRef<SWidget> Tabs(FName Prefix,const TArray<FText>& Labels);
    void Refresh();
    FSimpleDelegate Guarded(TFunction<void()> Fn);
};

#define INK_XT_SCREEN(ClassName) \
class BOUNDLESSIMMORTALITY_API ClassName : public SInkXianTuPage \
{ \
public: \
    INK_SCREEN_ARGS(ClassName) \
protected: \
    virtual void Rebuild() override; \
    virtual int32 Revision() const override; \
};

INK_XT_SCREEN(SInkWorldScreen)
INK_XT_SCREEN(SInkRaceScreen)
INK_XT_SCREEN(SInkGuildScreen)
INK_XT_SCREEN(SInkMarketScreen)
INK_XT_SCREEN(SInkSwordBookScreen)
INK_XT_SCREEN(SInkMethodsScreen)
INK_XT_SCREEN(SInkTribulationScreen)
INK_XT_SCREEN(SInkItemSourcesScreen)
INK_XT_SCREEN(SInkGuildBuildScreen)
INK_XT_SCREEN(SInkGearScreen)

/** 灵玉商城 (in 仙途) and 武灵商城 (in 武灵山门): one page, the shop chosen at construction (XianTuCatalog::EShop). */
class BOUNDLESSIMMORTALITY_API SInkShopScreen : public SInkXianTuPage
{
public:
    SLATE_BEGIN_ARGS(SInkShopScreen):_Shop(0){}
        SLATE_ARGUMENT(TSharedPtr<SInkUIRoot>,Root)
        SLATE_ARGUMENT(uint8,Shop)
    SLATE_END_ARGS()
    void Construct(const FArguments& InArgs);
protected:
    virtual void Rebuild() override;
    virtual int32 Revision() const override;
    uint8 Shop=0;
    /** Stage shown on the 突破材料 tab; None follows the anchor's next major stage. */
    FName StagePick;
    void Buy(FName Sku);
};

/** 仙道殿: the 金元商城 of looks and the wardrobe. Looks never change a stat. */
class BOUNDLESSIMMORTALITY_API SInkHallScreen : public SInkXianTuPage
{
public:
    INK_SCREEN_ARGS(SInkHallScreen)
protected:
    virtual void Rebuild() override;
    virtual int32 Revision() const override;
    FName Preview;
    double PreviewSince=0;
    void PaintPreview(InkUI::FPainter& P) const;
};

/** 每日一签: shake the bamboo cylinder until one stick leaps out and reads the day's fortune. */
class BOUNDLESSIMMORTALITY_API SInkFortuneScreen : public SInkXianTuPage
{
public:
    INK_SCREEN_ARGS(SInkFortuneScreen)
    virtual void Tick(const FGeometry& AllottedGeometry,const double InCurrentTime,const float InDeltaTime) override;
protected:
    virtual void Rebuild() override;
    virtual int32 Revision() const override;
    /** Motion::Now() when the shaking began; negative when idle. */
    double ShakeStart=-1,RevealStart=-1;
    uint8 Pending=0;
    void Draw();
    void PaintAltar(InkUI::FPainter& P) const;
};

/** 境界突破: minor steps pay 修为 and 破境石; major stages lay out a core and optional 避凶吉兆, 滋润灵物 and 天材地宝. */
class BOUNDLESSIMMORTALITY_API SInkBreakthroughScreen : public SInkXianTuPage
{
public:
    INK_SCREEN_ARGS(SInkBreakthroughScreen)
protected:
    virtual void Rebuild() override;
    virtual int32 Revision() const override;
    double CelebrateStart=-1;
    int32 CelebrateTo=0,CelebrateQuality=-1;
    bool bCelebrateMajor=false;
    void Commit();
    void PaintCelebration(InkUI::FPainter& P) const;
};

#undef INK_XT_SCREEN

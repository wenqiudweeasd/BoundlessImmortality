#pragma once
#include "CoreMinimal.h"
#include "InkUI/InkUIScreensXianTu.h"
#include "WanJian/WanJianBattle.h"
#include "WanJian/WanJianTypes.h"

class UWanJianSubsystem;

/** The 万剑仙宗 hub of GDD chapter 9: six bottom tabs and the battle view over the branch's own account. */
class BOUNDLESSIMMORTALITY_API SInkWanJianScreen:public SInkXianTuPage
{
public:
    INK_SCREEN_ARGS(SInkWanJianScreen)
    virtual void Tick(const FGeometry& AllottedGeometry,const double InCurrentTime,const float InDeltaTime)override;
    virtual bool HandleBack()override;

protected:
    virtual void Rebuild()override;
    virtual int32 Revision()const override;

private:
    UWanJianSubsystem* Account()const;
    void Report(bool bOk,const FText& Message);

    TSharedRef<SWidget> BuildWorldTab();
    TSharedRef<SWidget> BuildDungeonTab();
    TSharedRef<SWidget> BuildLineupTab();
    TSharedRef<SWidget> BuildSummonTab();
    TSharedRef<SWidget> BuildSectTab();
    TSharedRef<SWidget> BuildCultivationTab();
    TSharedRef<SWidget> BuildBattleView();
    TSharedRef<SWidget> BuildSettlement();
    void PaintArena(InkUI::FPainter& P)const;

    void StartStage(int32 StageId);
    void StartDungeon(FName Dungeon);
    void StartBattle(const WanJianBattle::FSetup& Setup);
    void SettleBattle();
    void CloseBattle();
    void AutoFillLineup();

    int32 WorldPick=1;
    int32 ChapterPick=1;
    int32 StagePick=0;
    int32 TeamPick=1;
    FName MethodPick=TEXT("Jifeng");

    TUniquePtr<WanJianBattle::FSimulator> Sim;
    int32 BattleStage=0;
    FName BattleDungeon;
    int32 BattleTeam=1;
    int32 BattleSwords=0;
    int32 BattleSpeed=1;
    double Accumulator=0;
    bool bSettled=false;
    WanJianBattle::FResult Outcome;
    FText OutcomeText;
    TArray<FWanJianDrawResult> LastDraw;
};

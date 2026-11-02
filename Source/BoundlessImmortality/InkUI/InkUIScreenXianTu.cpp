#include "InkUI/InkUIScreensXianTu.h"
#include "InkUI/InkUIXianTuKit.h"
#include "XianTu/XianTuSubsystem.h"
#include "XianTu/XianTuRules.h"
#include "XianTu/XianTuCatalog.h"
#include "LiveWeapons/SpecialSwords/LiveSpecialSwordTypes.h"
#include "LiveWeapons/SpecialSwords/LiveSwordCodex.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Styling/CoreStyle.h"

#define LOCTEXT_NAMESPACE "InkUIXianTu"

using namespace InkUI;
using namespace InkUI::Kit;
using namespace XianTuRules;

namespace
{
    /** Horizontal schedule with labelled segments and a moving cursor (0..1). */
    TSharedRef<SWidget> Timeline(TArray<float> Stops,TArray<FText> Labels,TAttribute<float> Cursor,float Height=56.f)
    {
        return SNew(SBox).HeightOverride(Height)
        [
            SNew(SInkPaint).OnPaint_Lambda([Stops,Labels,Cursor](FPainter& P,const FGeometry&)
            {
                const auto& K=Colors();const FVector2f S=P.Size();const float Y=S.Y*.35f,Total=Stops.Last();
                const float C=FMath::Clamp(Cursor.Get(),0.f,1.f);
                for(int32 I=0;I+1<Stops.Num();++I)
                {
                    const float A=Stops[I]/Total*S.X,B=Stops[I+1]/Total*S.X;
                    const bool bPast=C*Total>=Stops[I+1],bNow=C*Total>=Stops[I]&&!bPast;
                    P.Rounded(FVector2f(A+2.f,Y-7.f),FVector2f(B-A-4.f,14.f),bPast?WithAlpha(K.Jade,.95f):bNow?WithAlpha(K.Gold,.95f):WithAlpha(K.PaperShade,.9f),5.f,WithAlpha(K.GoldDeep,.7f),1.f);
                    if(Labels.IsValidIndex(I))P.Text(Labels[I].ToString(),Font(EFont::Tiny,16.f),FVector2f((A+B)*.5f,Y+22.f),bNow?K.TextDark:K.TextDarkDim,FVector2f(.5f,.5f));
                }
                P.Next();
                const float X=C*S.X;
                P.Glow(FVector2f(X,Y),FVector2f(18.f),WithAlpha(K.GoldBright,.6f));
                P.Diamond(FVector2f(X,Y),FVector2f(7.f,10.f),K.Cinnabar);
            })
        ];
    }

    /** Paper input well with a gilt edge; the core editable style is dark and swallows dark ink text. */
    const FEditableTextBoxStyle& InputStyle()
    {
        static const FEditableTextBoxStyle Style=[]
        {
            const auto& K=Colors();
            FEditableTextBoxStyle S=FCoreStyle::Get().GetWidgetStyle<FEditableTextBoxStyle>("NormalEditableTextBox");
            const FSlateRoundedBoxBrush Idle(K.PaperLight,6.f,WithAlpha(K.GoldDeep,.85f),1.5f);
            const FSlateRoundedBoxBrush Focus(K.PaperLight,6.f,K.Gold,2.5f);
            S.SetBackgroundImageNormal(Idle).SetBackgroundImageHovered(Focus).SetBackgroundImageFocused(Focus).SetBackgroundImageReadOnly(Idle);
            S.SetForegroundColor(FSlateColor(K.TextDark)).SetFocusedForegroundColor(FSlateColor(K.TextDark)).SetPadding(FMargin(14.f,10.f));
            return S;
        }();
        return Style;
    }

    FString PrettyMesh(const FString& Mesh)
    {
        if(Mesh.IsEmpty())return TEXT("本命默认剑");
        FString Kind=TEXT("兵刃");
        if(Mesh.Contains(TEXT("GreatSword")))Kind=TEXT("巨剑");else if(Mesh.Contains(TEXT("GreatBow")))Kind=TEXT("长弓");
        else if(Mesh.Contains(TEXT("Hammer")))Kind=TEXT("战锤");else if(Mesh.Contains(TEXT("Shield")))Kind=TEXT("灵盾");else if(Mesh.Contains(TEXT("Sword")))Kind=TEXT("长剑");
        static const TCHAR* Nums[]={TEXT("一"),TEXT("二"),TEXT("三")};
        int32 Type=0,Color=0;
        for(int32 I=1;I<=3;++I){if(Mesh.Contains(FString::Printf(TEXT("Type%d"),I)))Type=I;if(Mesh.Contains(FString::Printf(TEXT("Color_%d"),I)))Color=I;}
        return FString::Printf(TEXT("%s · %s型 · %s色"),*Kind,Type?Nums[Type-1]:TEXT("一"),Color?Nums[Color-1]:TEXT("一"));
    }
}

// ------------------------------------------------------------------ shared shell

UXianTuSubsystem* SInkXianTuPage::X() const{return M().XianTu.Get();}
const FXianTuSave* SInkXianTuPage::S() const{const UXianTuSubsystem* Svc=X();return Svc?&Svc->GetState().Get():nullptr;}
int32 SInkXianTuPage::Revision() const{return M().XianTuRev;}

void SInkXianTuPage::BuildShell(const FText& Title,const FText& Seal,TSharedRef<SWidget> Header,TSharedRef<SWidget> Overlay)
{
    BuildPage(Title,Seal,Header,SNew(SScrollBox).ScrollBarStyle(&ScrollStyle())+SScrollBox::Slot()[SAssignNew(Body,SBox)],SAssignNew(Footer,SBox),Overlay);
    Refresh();
}

void SInkXianTuPage::Refresh()
{
    SeenRev=Revision();
    bBuiltLandscape=IsLandscape();
    if(Body&&Footer)Rebuild();
    bEntered=true;
}

void SInkXianTuPage::OnModelChanged(){if(Revision()!=SeenRev)Refresh();}

void SInkXianTuPage::OnLayoutChanged()
{
    SInkPageScreen::OnLayoutChanged();
    if(bEntered&&IsLandscape()!=bBuiltLandscape)Refresh();
}

FSimpleDelegate SInkXianTuPage::Guarded(TFunction<void()> Fn)
{
    // Confirm cards outlive a page that closes underneath them; the action then does nothing.
    const TWeakPtr<SWidget> Weak=AsShared();
    return FSimpleDelegate::CreateLambda([Weak,Fn=MoveTemp(Fn)]{if(Weak.IsValid())Fn();});
}

bool SInkXianTuPage::Exec(const FString& Verb,const TArray<FString>& Args)
{
    const auto R=GetRoot();if(!R)return false;
    UXianTuSubsystem* Svc=X();
    if(!Svc){R->Toast(LOCTEXT("NoService","进入游戏后可使用仙途功能"),2);return false;}
    FText Message;const bool bOk=Svc->Execute(Verb,Args,Message);
    if(!Message.IsEmpty())R->Toast(Message,bOk?1:2);
    R->Model().XianTuRev=Svc->GetRevision();R->Model().Touch();
    return bOk;
}

TSharedRef<SWidget> SInkXianTuPage::Appear(TSharedRef<SWidget> Widget,int32 Order) const
{
    if(bEntered)return Widget;
    return SNew(SInkAnimBox).Delay(Motion::Stagger(Order,.05f,.25f)).Distance(22.f)[Widget];
}

TSharedRef<SWidget> SInkXianTuPage::Tabs(FName Prefix,const TArray<FText>& Labels)
{
    return SNew(SInkTabBar).Tabs(Labels).Selected(Tab).TestPrefix(Prefix).Height(62.f).OnSelected_Lambda([this](int32 I){Tab=I;Page=0;Refresh();});
}

// ------------------------------------------------------------------ 世界历练

void SInkWorldScreen::Construct(const FArguments& InArgs)
{
    InitScreen(InArgs._Root.ToSharedRef(),TEXT("World"));
    Pick1=0;Pick2=0;
    BuildShell(LOCTEXT("WorldTitle","世界历练"),LOCTEXT("WorldSeal","历"),
        SNew(SBox).HAlign(HAlign_Center)
        [
            MakeText(TAttribute<FText>::CreateLambda([this]
            {
                const FXianTuSave* St=S();if(!St)return LOCTEXT("WorldHeadOff","进入游戏后开启历练");
                return FText::Format(LOCTEXT("WorldHead","今日完整历练 {0}/6 · 累计 {1} 轮 · 本地时间流速 ×{2}"),N(St->World.RoundsToday),N(St->World.CompletedTotal),FText::AsNumber(St->TimeScale));
            }),EFont::Section,Colors().TextDark,24.f)
        ]);
}

int32 SInkWorldScreen::Revision() const{return M().XianTuRev;}

void SInkWorldScreen::Rebuild()
{
    const FXianTuSave* St=S();
    if(!St){Body->SetContent(Paper(Hint(LOCTEXT("WorldOffline","进入游戏后可开启世界历练：主播在场景中与剑灵共同完成三个阶段。"))));Footer->SetContent(SNullWidget::NullWidget);return;}
    const FXianTuWorld& W=St->World;
    const TArray<FRegion>& AllRegions=XianTuRules::Regions();
    const bool bLive=W.State==EXianTuWorldState::Preparing||W.State==EXianTuWorldState::Running||W.State==EXianTuWorldState::Interrupted||W.State==EXianTuWorldState::Settling;
    const TSharedRef<SVerticalBox> List=SNew(SVerticalBox);
    int32 Order=0;
    auto Add=[&](TSharedRef<SWidget> Widget){List->AddSlot().AutoHeight().Padding(FMargin(0.f,6.f))[Appear(Widget,Order++)];};
    const FRegion* Region=AllRegions.FindByPredicate([&](const FRegion& R){return R.Id==W.Region;});
    if(bLive)
    {
        Add(Paper(Rows({
            Scene(Region?Region->Scene:4,170.f,false),
            Head(TAttribute<FText>::CreateLambda([this]{const FXianTuSave* X2=S();if(!X2)return FText();const FRegion* R=XianTuRules::Regions().FindByPredicate([&](const FRegion& Q){return Q.Id==X2->World.Region;});
                return FText::Format(LOCTEXT("LiveHead","{0} · {1} · {2}"),R?R->Name:FText(),Preset(X2->World.Preset).Name,StateText(X2->World.State));}),30.f),
            Line(TAttribute<FText>::CreateLambda([this]{const FXianTuSave* X2=S();return X2?FText::Format(LOCTEXT("LiveClock","历练时钟 {0} / 10:00"),Clock(X2->World.Elapsed)):FText();}),24.f),
            Timeline({0,60,240,420,540,600},{LOCTEXT("TPrep","准备"),FText::FromString(StageName(W.Route,1).ToString().Left(2)),FText::FromString(StageName(W.Route,2).ToString().Left(2)),LOCTEXT("TBoss","首领"),LOCTEXT("TSettle","结算")},
                TAttribute<float>::CreateLambda([this]{const FXianTuSave* X2=S();return X2?float(X2->World.Elapsed/600.0):0.f;}))
        })));
        if(W.State==EXianTuWorldState::Interrupted)
        {
            Add(Paper(Rows({Head(LOCTEXT("Down","主播战败 · 恢复中"),28.f),
                Line(TAttribute<FText>::CreateLambda([this]{const FXianTuSave* X2=S();return X2?FText::Format(LOCTEXT("Recover","{0} 后重返当前阶段；已完成阶段保留，不掉境界、不剥夺礼物。"),Clock(X2->World.RecoverRemaining)):FText();}))})));
        }
        else if(W.State==EXianTuWorldState::Preparing)
        {
            Add(Paper(Rows({Head(LOCTEXT("Prep","准备 · 迎接新人入阵"),28.f),
                Line(TAttribute<FText>::CreateLambda([this]{const FXianTuSave* X2=S();return X2?FText::Format(LOCTEXT("PrepLeft","{0} 后进入第一阶段。观众发送“加入”即可让剑灵随主播出战。"),Clock(60-X2->World.Elapsed)):FText();}))})));
        }
        else if(W.State==EXianTuWorldState::Running)
        {
            const FPreset& P=Preset(W.Preset);
            Add(Paper(Rows({
                Head(FText::Format(LOCTEXT("StageHead","第{0}阶段 · {1}"),N(W.Stage),StageName(W.Route,W.Stage)),28.f),
                Meter(TAttribute<float>::CreateLambda([this]{const FXianTuSave* X2=S();return X2&&X2->World.Target>0?float(X2->World.Progress)/X2->World.Target:0.f;}),
                    TAttribute<FText>::CreateLambda([this]{const FXianTuSave* X2=S();return X2?FText::Format(LOCTEXT("Obj","目标 {0}/{1}"),N(X2->World.Progress),N(X2->World.Target)):FText();}),EBar::Jade),
                Line(TAttribute<FText>::CreateLambda([this,P]
                {
                    const FXianTuSave* X2=S();if(!X2)return FText();
                    const double End=X2->World.Stage<=1?240:X2->World.Stage==2?420:540;
                    return FText::Format(LOCTEXT("StageLeft","本阶段剩余 {0} · 第 {1}/{2} 波 · 怪物来自场景实时生成"),Clock(End-X2->World.Elapsed),N(FMath::Max(1,X2->World.Wave)),N(P.Waves));
                }),22.f)})));
        }
        else Add(Paper(Head(LOCTEXT("Settling","结算演出中…"),28.f)));
        const TSharedRef<SVerticalBox> Stages=SNew(SVerticalBox);
        for(int32 I=1;I<=3;++I)
        {
            const int32 R=W.StageResults.IsValidIndex(I-1)?W.StageResults[I-1]:-1;
            const bool bNow=W.Stage==I&&W.State==EXianTuWorldState::Running;
            Stages->AddSlot().AutoHeight().Padding(FMargin(0.f,4.f))
            [
                SNew(SHorizontalBox)
                +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Plaque(R==1?LOCTEXT("SDone","完成"):R==0?LOCTEXT("SMiss","未完成"):bNow?LOCTEXT("SNow","进行中"):LOCTEXT("SWait","待开始"),R==1?0:R==0?2:bNow?3:1,100.f,36.f)]
                +SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center).Padding(FMargin(12.f,0.f))[Line(FText::Format(LOCTEXT("SRow","第{0}阶段 · {1}"),N(I),StageName(W.Route,I)),22.f)]
                +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Strip(StageRewards(I),46.f)]
            ];
        }
        Add(Paper(Rows({Head(LOCTEXT("StagesHead","阶段与固定奖励"),26.f),Stages})));
        Footer->SetContent(SNew(SHorizontalBox)
            +SHorizontalBox::Slot().FillWidth(1.f).HAlign(HAlign_Center)
            [
                Btn(LOCTEXT("Abandon","提前结束"),EButtonSkin::Cinnabar,TEXT("World.Abandon"),FSimpleDelegate::CreateLambda([this]
                {
                    if(const auto R=GetRoot())R->Confirm(LOCTEXT("AbandonT","提前结束历练"),LOCTEXT("AbandonB","将按已完成的阶段结算奖励，未完成阶段不发放。"),Guarded([this]{Exec(TEXT("WorldAbandon"));}),true,LOCTEXT("AbandonC","结束"));
                }),true,64.f,240.f)
            ]);
        Body->SetContent(List);
        return;
    }
    if(W.State==EXianTuWorldState::Completed)
    {
        int32 Done=0;for(int32 R:W.StageResults)Done+=R==1?1:0;
        Add(Paper(Rows({
            Head(FText::Format(LOCTEXT("ResultHead","历练结算 · 完成 {0}/3 阶段"),N(Done)),30.f),
            Strip(W.LastRewards,62.f),
            Line(FText::Format(LOCTEXT("ResultQi","事件修为 +{0}（每日前6个完整轮次）· 公共资材 +{1} · 声望 +{2}"),N(FMath::RoundToInt(W.LastQi)),N(W.LastGuildMaterials),N(W.LastReputation)),22.f),
            Hint(FText::Format(LOCTEXT("Receipt","本地收据 {0} · 破境石计入角色突破材料"),FText::FromString(W.LastReceipt))),
            SNew(SBox).HAlign(HAlign_Right)[Btn(LOCTEXT("Ack","收下"),EButtonSkin::Gold,TEXT("World.Ack"),FSimpleDelegate::CreateLambda([this]{Exec(TEXT("WorldAck"));}))]
        }),FMargin(28.f,20.f,28.f,22.f),Art::ESkin::CardOrnate));
    }
    // Region choice: the first three regions are open; the rest are listed with their release phase.
    const int32 Columns=IsLandscape()?4:2;
    const TSharedRef<SUniformGridPanel> Grid=SNew(SUniformGridPanel).SlotPadding(FMargin(6.f));
    for(int32 I=0;I<AllRegions.Num();++I)
    {
        const FRegion Rg=AllRegions[I];
        Grid->AddSlot(I%Columns,I/Columns)
        [
            SNew(SInkClickable).TestId(FName(*(TEXT("World.Region.")+Rg.Id.ToString()))).Radius(8.f)
            .OnClicked_Lambda([this,I,Rg]{if(!Rg.bOpen){if(const auto R=GetRoot())R->Toast(FText::Format(LOCTEXT("Locked","{0} 将在 {1} 阶段开放"),Rg.Name,Rg.Phase),2);return;}Pick0=I;Refresh();})
            [
                SNew(SBox).HeightOverride(150.f)
                [
                    SNew(SInkPaint).OnPaint_Lambda([this,I,Rg](FPainter& P,const FGeometry&)
                    {
                        const auto& K=Colors();const FVector2f Sz=P.Size();const bool bSel=Pick0==I;
                        Cover(P,Art::Icon(Art::EIconSet::FeatureScenes,Rg.Scene),FVector2f(0.f,0.f),Sz,Rg.bOpen?FLinearColor::White:FLinearColor(.45f,.45f,.45f,1.f));P.Next();
                        P.VGradient(FVector2f(0.f,Sz.Y*.4f),FVector2f(Sz.X,Sz.Y*.6f),{{0.f,WithAlpha(K.InkBlack,0.f)},{1.f,WithAlpha(K.InkBlack,.85f)}});
                        P.GoldFrame(FVector2f(0.f,0.f),Sz,1.f,bSel?K.GoldBright:WithAlpha(K.GoldDeep,.7f),.5f,bSel);
                        if(bSel)P.Halo(FVector2f(2.f,2.f),Sz-FVector2f(4.f,4.f),8.f,6.f,WithAlpha(K.GoldBright,.6f));
                        P.Next();
                        P.Text(Rg.Name.ToString(),Font(EFont::Title,30.f,1.5f,K.InkBlack),FVector2f(14.f,Sz.Y-50.f),K.GoldBright,FVector2f(0.f,.5f));
                        P.Text(Rg.Theme.ToString(),Font(EFont::Small,18.f,1.f,K.InkBlack),FVector2f(14.f,Sz.Y-20.f),K.TextLight,FVector2f(0.f,.5f));
                        P.Skin(Rg.bOpen?Art::ESkin::PlaqueJade:Art::ESkin::PlaqueLacquer,FVector2f(Sz.X-74.f,8.f),FVector2f(66.f,30.f));P.Next();P.Next();
                        P.Text(Rg.bOpen?TEXT("开放"):Rg.Phase.ToString(),Font(EFont::Tiny,15.f),FVector2f(Sz.X-41.f,23.f),Rg.bOpen?K.TextDark:K.TextGold,FVector2f(.5f,.52f));
                    })
                ]
            ]
        ];
    }
    Add(Paper(Rows({Head(LOCTEXT("RegionHead","选择区域"),26.f),Grid})));
    TArray<FText> PresetNames;for(const FPreset& P:Presets())PresetNames.Add(P.Name);
    Add(Paper(Rows({
        Head(LOCTEXT("DiffHead","难度（开局固定，不随送礼抬高）"),26.f),
        SNew(SInkTabBar).Tabs(PresetNames).Selected(Pick1).Height(58.f).TestPrefix(TEXT("World.Preset")).OnSelected_Lambda([this](int32 I){Pick1=I;}),
        Hint(TAttribute<FText>::CreateLambda([this]
        {
            const FPreset& P=Presets()[FMath::Clamp(Pick1,0,Presets().Num()-1)];
            return FText::Format(LOCTEXT("PresetInfo","参考观众 {0} 人 · 妖物生命 {1} · 阵眼生命 {2} · 首领生命 {3} · 每波 {4} 只共 {5} 波"),N(P.Reference),N(FMath::RoundToInt(P.Normal)),N(FMath::RoundToInt(P.Obstacle)),N(FMath::RoundToInt(P.Boss)),N(P.PerWave),N(P.Waves));
        })),
        Gap(4.f),
        Head(LOCTEXT("RouteHead","路线"),26.f),
        SNew(SInkTabBar).Tabs(TArray<FText>{LOCTEXT("RouteGuard","先护阵"),LOCTEXT("RouteGather","先采集")}).Selected(Pick2).Height(58.f).TestPrefix(TEXT("World.Route")).OnSelected_Lambda([this](int32 I){Pick2=I;}),
        Hint(LOCTEXT("RouteHint","路线改变敌方行为与演出顺序，不改变固定产出：先护阵为 护阵→破障→首领，先采集为 采集→护阵→首领。"))
    })));
    const TSharedRef<SVerticalBox> Rewards=SNew(SVerticalBox);
    for(int32 I=1;I<=3;++I)Rewards->AddSlot().AutoHeight().Padding(FMargin(0.f,4.f))
    [
        SNew(SHorizontalBox)
        +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(0.f,0.f,12.f,0.f))[Plaque(FText::Format(LOCTEXT("StageN","阶段{0}"),N(I)),1,90.f,36.f)]
        +SHorizontalBox::Slot().FillWidth(1.f)[Strip(StageRewards(I),50.f)]
    ];
    Add(Paper(Rows({Head(LOCTEXT("RewardHead","固定阶段奖励"),26.f),Rewards,
        Hint(LOCTEXT("WorldRules","十分钟三阶段：0–60秒准备，60–240阶段一，240–420阶段二，420–540阶段三，540–600结算；提前完成即进入下一阶段。主播战败进入30秒恢复，已完成阶段保留。完整历练另得60修为（每日前6轮），在场武灵正式成员每人+1公共资材（个人每日6、武灵每日100）。"))})));
    Footer->SetContent(SNew(SHorizontalBox)
        +SHorizontalBox::Slot().FillWidth(1.f).HAlign(HAlign_Center)
        [
            Btn(LOCTEXT("Start","开始历练"),EButtonSkin::Gold,TEXT("World.Start"),FSimpleDelegate::CreateLambda([this]
            {
                const TArray<FRegion>& Rs=XianTuRules::Regions();const FRegion& R=Rs[FMath::Clamp(Pick0,0,Rs.Num()-1)];
                Exec(TEXT("WorldStart"),{R.Id.ToString(),Presets()[FMath::Clamp(Pick1,0,Presets().Num()-1)].Id.ToString(),Pick2?TEXT("gather"):TEXT("guard")});
            }),true,72.f,300.f,28.f)
        ]);
    Body->SetContent(List);
}

// ------------------------------------------------------------------ 资源争夺

void SInkRaceScreen::Construct(const FArguments& InArgs)
{
    InitScreen(InArgs._Root.ToSharedRef(),TEXT("Resources"));
    BuildShell(LOCTEXT("RaceTitle","资源争夺"),LOCTEXT("RaceSeal","脉"),
        Tabs(TEXT("Race.Tabs"),{LOCTEXT("RaceTab0","灵脉竞采"),LOCTEXT("RaceTab1","开采权"),LOCTEXT("RaceTab2","竞采记录")}));
}

int32 SInkRaceScreen::Revision() const{return M().XianTuRev;}

void SInkRaceScreen::Rebuild()
{
    const FXianTuSave* St=S();UXianTuSubsystem* Svc=X();
    if(!St||!Svc){Body->SetContent(Paper(Hint(LOCTEXT("RaceOffline","进入游戏后可报名灵脉竞采。"))));Footer->SetContent(SNullWidget::NullWidget);return;}
    const TSharedRef<SVerticalBox> List=SNew(SVerticalBox);int32 Order=0;
    auto Add=[&](TSharedRef<SWidget> Widget){List->AddSlot().AutoHeight().Padding(FMargin(0.f,6.f))[Appear(Widget,Order++)];};
    Footer->SetContent(SNullWidget::NullWidget);
    const FXianTuRace& R=St->Race;
    if(Tab==1)
    {
        if(const FXianTuLease* L=Svc->ActiveLease())
        {
            const FNode* Nd=Node(L->Node);const FString Id=L->Id;
            Add(Paper(Rows({
                Scene(Nd?Nd->Scene:3,140.f,false),
                Head(FText::Format(LOCTEXT("LeaseHead","{0} 开采权 · {1}"),Nd?Nd->Name:FText(),StateText(L->State)),30.f),
                Line(Nd?Nd->Owner:FText(),22.f),
                Line(TAttribute<FText>::CreateLambda([this,Id]
                {
                    const FXianTuSave* X2=S();if(!X2)return FText();
                    const FXianTuLease* Q=X2->Leases.FindByPredicate([&](const FXianTuLease& A){return A.Id==Id;});if(!Q)return FText();
                    return FText::Format(LOCTEXT("LeaseLive","剩余 {0} · 有效在线 {1} · 已产出 修为 {2} / 物品 {3}"),Clock(Q->End-X2->SimNow),Clock(Q->OnlineSeconds),N(FMath::RoundToInt(Q->QiGranted)),N(Q->ItemsGranted));
                }),22.f),
                Hint(FText::Format(LOCTEXT("LeaseNote","竞采第{0}名 {1} 分获得 {2} 分钟 · 收据 {3}。到期按时钟计算不因关播暂停；只有主播在游戏中的时间产生收益；不可转让、不自动续期。"),N(L->Rank),N(L->Score),N(L->Minutes),FText::FromString(L->Receipt)))
            }),FMargin(28.f,20.f,28.f,22.f),Art::ESkin::CardOrnate));
        }
        else Add(Paper(Hint(LOCTEXT("NoLease","暂无生效中的开采权。在灵脉竞采中取得 600 分以上即按名次获得 30/20/10 分钟开采权。"))));
        for(const FXianTuLease& L:St->Leases)
        {
            if(L.State==EXianTuLeaseState::Active)continue;
            const FNode* Nd=Node(L.Node);
            Add(Paper(SNew(SHorizontalBox)
                +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(0.f,0.f,12.f,0.f))[Plaque(StateText(L.State),1,96.f,36.f)]
                +SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)[Line(FText::Format(LOCTEXT("LeaseRow","{0} · {1} 分钟 · 在线 {2} · 修为 {3} · 物品 {4}"),Nd?Nd->Name:FText(),N(L.Minutes),Clock(L.OnlineSeconds),N(FMath::RoundToInt(L.QiGranted)),N(L.ItemsGranted)),21.f)]));
        }
        Body->SetContent(List);return;
    }
    if(Tab==2)
    {
        int32 Count=0;
        for(const FXianTuRecord& Rec:St->Receipts)if(Rec.Kind==TEXT("竞采"))
        {
            Add(Paper(SNew(SHorizontalBox)
                +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(0.f,0.f,12.f,0.f))[Plaque(LOCTEXT("RecKind","竞采"),3,80.f,34.f)]
                +SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)[Line(FText::FromString(Rec.Text),21.f)]));
            if(++Count>=20)break;
        }
        if(!Count)Add(Paper(Hint(LOCTEXT("NoRec","暂无竞采记录。"))));
        Body->SetContent(List);return;
    }
    const FNode* Nd=Node(R.Node);
    switch(R.State)
    {
    case EXianTuRaceState::Queue:case EXianTuRaceState::Matched:case EXianTuRaceState::Ready:
    {
        TArray<TSharedRef<SWidget>> Parts;
        Parts.Add(Head(FText::Format(LOCTEXT("QueueHead","{0} · {1}"),Nd?Nd->Name:FText(),StateText(R.State)),30.f));
        Parts.Add(Line(TAttribute<FText>::CreateLambda([this]
        {
            const FXianTuSave* X2=S();if(!X2)return FText();const FXianTuRace& Q=X2->Race;
            static const TCHAR* Tiers[]={TEXT("1–9人档"),TEXT("10–49人档"),TEXT("50人以上档")};
            if(Q.State==EXianTuRaceState::Ready)return FText::Format(LOCTEXT("ReadyLeft","准备期剩余 {0} · {1} · 名单已冻结"),Clock(120-Q.StateElapsed),FText::FromString(Tiers[FMath::Clamp(Q.Tier,0,2)]));
            return FText::Format(LOCTEXT("QueueLeft","匹配中 {0} · {1}（按最近参与人数分档，不混入高档对手）"),Clock(Q.StateElapsed),FText::FromString(Tiers[FMath::Clamp(Q.Tier,0,2)]));
        }),22.f));
        for(const FXianTuRaceTeam& Team:R.Teams)Parts.Add(Pair(Team.bSelf?LOCTEXT("SelfTeam","本武灵"):LOCTEXT("Rival","对手"),FText::Format(LOCTEXT("TeamRow","{0} · 参与 {1} 人"),FText::FromString(Team.Name),N(Team.Members))));
        if(R.Representatives.Num())Parts.Add(Hint(FText::Format(LOCTEXT("Reps","协作代表（{0}）：{1}"),N(R.Representatives.Num()),FText::FromString(FString::Join(R.RepresentativeNames,TEXT("、"))))));
        Add(Paper(Rows(Parts)));
        Footer->SetContent(SNew(SBox).HAlign(HAlign_Center)[Btn(LOCTEXT("CancelRace","取消报名"),EButtonSkin::Paper,TEXT("Race.Cancel"),FSimpleDelegate::CreateLambda([this]{Exec(TEXT("RaceCancel"));}),true,64.f,240.f)]);
        break;
    }
    case EXianTuRaceState::Running:case EXianTuRaceState::Frozen:
    {
        Add(Paper(Rows({
            Head(TAttribute<FText>::CreateLambda([this]
            {
                const FXianTuSave* X2=S();if(!X2)return FText();const FNode* Q=Node(X2->Race.Node);
                if(X2->Race.State==EXianTuRaceState::Frozen)return LOCTEXT("Frozen","计分冻结 · 结算中…");
                return FText::Format(LOCTEXT("RunHead","{0} · 挑战剩余 {1}"),Q?Q->Name:FText(),Clock(360-X2->Race.StateElapsed));
            }),30.f),
            Timeline({0,60,120,180,240,300,360},{LOCTEXT("T1","任务1"),LOCTEXT("T2","任务2"),LOCTEXT("T3","任务3"),LOCTEXT("T4","任务4"),LOCTEXT("T5","任务5"),LOCTEXT("T6","任务6")},
                TAttribute<float>::CreateLambda([this]{const FXianTuSave* X2=S();return X2?float(X2->Race.StateElapsed/360.0):0.f;})),
            Meter(TAttribute<float>::CreateLambda([this]{const FXianTuSave* X2=S();return X2&&X2->Race.Target>0?float(X2->Race.Progress)/X2->Race.Target:0.f;}),
                TAttribute<FText>::CreateLambda([this]
                {
                    const FXianTuSave* X2=S();if(!X2)return FText();int32 Done=0;for(int32 V:X2->Race.TaskResults)Done+=V==1?1:0;
                    return FText::Format(LOCTEXT("TaskMeter","当前任务 {0}/{1} · 已完成 {2}/6"),N(X2->Race.Progress),N(X2->Race.Target),N(Done));
                }),EBar::Jade)
        })));
        Add(Paper(Rows({
            Head(LOCTEXT("OpHead","主播时机操作（每次 100 分）"),26.f),
            Line(TAttribute<FText>::CreateLambda([this]
            {
                const UXianTuSubsystem* Svc2=X();const FXianTuSave* X2=S();if(!Svc2||!X2)return FText();int32 K=0;double Left=0;
                if(Svc2->OpWindow(K,Left))return FText::Format(LOCTEXT("OpNow","第{0}次时机已至！剩余 {1} 秒"),N(K+1),FText::AsNumber(FMath::CeilToInt(Left)));
                const double Next=X2->Race.StateElapsed<90?90:X2->Race.StateElapsed<270?270:-1;
                return Next<0?LOCTEXT("OpDone","两次时机均已结束"):FText::Format(LOCTEXT("OpNext","下一次时机预告：{0} 后"),Clock(Next-X2->Race.StateElapsed));
            }),23.f),
            SNew(SBox).HAlign(HAlign_Center)
            [
                Btn(LOCTEXT("OpBtn","把握时机"),EButtonSkin::Gold,TEXT("Race.Op"),FSimpleDelegate::CreateLambda([this]{Exec(TEXT("RaceOp"));}),
                    TAttribute<bool>::CreateLambda([this]{const UXianTuSubsystem* Svc2=X();int32 K=0;double Left=0;return Svc2&&Svc2->OpWindow(K,Left);}),70.f,280.f,27.f)
            ]
        })));
        TArray<TSharedRef<SWidget>> Reps;
        Reps.Add(Head(LOCTEXT("RepHead","成员协作（代表响应，满分 200）"),26.f));
        Reps.Add(Line(TAttribute<FText>::CreateLambda([this]
        {
            const UXianTuSubsystem* Svc2=X();const FXianTuSave* X2=S();if(!Svc2||!X2)return FText();int32 K=0;double Left=0;
            if(Svc2->ResponseWindow(K,Left))return FText::Format(LOCTEXT("RespNow","响应窗口 {0} 开启 · 剩余 {1} 秒 · 代表发送“护阵”即计 1 次"),N(K+1),FText::AsNumber(FMath::CeilToInt(Left)));
            return LOCTEXT("RespWait","响应窗口：第45–70秒、第225–250秒；名单在准备期冻结，离场未响应记0。");
        }),22.f));
        const TSharedRef<SWrapBox> Names=SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(8.f,6.f));
        for(int32 I=0;I<R.Representatives.Num();++I)
        {
            const int32 Mask=R.Responses.FindRef(R.Representatives[I]);
            Names->AddSlot()[Plaque(FText::Format(LOCTEXT("RepChip","{0} {1}{2}"),FText::FromString(R.RepresentativeNames.IsValidIndex(I)?R.RepresentativeNames[I].Left(6):TEXT("?")),(Mask&1)?T(TEXT("✓")):T(TEXT("·")),(Mask&2)?T(TEXT("✓")):T(TEXT("·"))),(Mask==3)?0:1,150.f,36.f)];
        }
        Reps.Add(Names);
        Add(Paper(Rows(Reps)));
        Footer->SetContent(SNew(SBox).HAlign(HAlign_Center)[Btn(LOCTEXT("QuitRace","退出竞采"),EButtonSkin::Cinnabar,TEXT("Race.Quit"),FSimpleDelegate::CreateLambda([this]
        {
            if(const auto Rt=GetRoot())Rt->Confirm(LOCTEXT("QuitT","退出竞采"),LOCTEXT("QuitB","将按当前得分立即结算，未完成任务记 0 分。竞采失败无惩罚。"),Guarded([this]{Exec(TEXT("RaceCancel"));}),true,LOCTEXT("QuitC","退出"));
        }),true,62.f,220.f)]);
        break;
    }
    case EXianTuRaceState::Settled:
    {
        const TSharedRef<SVerticalBox> Table=SNew(SVerticalBox);
        TArray<FXianTuRaceTeam> Teams=R.Teams;Teams.Sort([](const FXianTuRaceTeam& A,const FXianTuRaceTeam& B){return A.Total>B.Total;});
        for(const FXianTuRaceTeam& Team:Teams)
        {
            Table->AddSlot().AutoHeight().Padding(FMargin(0.f,4.f))
            [
                SNew(SHorizontalBox)
                +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(0.f,0.f,10.f,0.f))[Plaque(FText::Format(LOCTEXT("RankN","第{0}"),N(Team.Rank)),Team.bSelf?3:1,76.f,36.f)]
                +SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)[Line(FText::Format(LOCTEXT("ScoreRow","{0} · 任务{1} 操作{2} 协作{3} = {4} 分 · {5}"),FText::FromString(Team.Name),N(Team.TaskScore),N(Team.AnchorScore),N(Team.MemberScore),N(Team.Total),
                    Team.LeaseMinutes>0?FText::Format(LOCTEXT("LeaseMin","开采 {0} 分钟"),N(Team.LeaseMinutes)):LOCTEXT("NoQualify","未达资格")),21.f,Team.bSelf?Colors().TextRed:Colors().TextDark)]
            ];
        }
        Add(Paper(Rows({Head(FText::Format(LOCTEXT("SettleHead","{0} 竞采结算"),Nd?Nd->Name:FText()),30.f),Table,
            Hint(FText::Format(LOCTEXT("SettleNote","总分1000=共同任务600+主播操作200+成员协作200；600分以上按不同分数组排名得30/20/10分钟，并列同档。礼物与付费成长不计分。对手为本地模拟。收据 {0}"),FText::FromString(R.Receipt)))}),FMargin(28.f,20.f,28.f,22.f),Art::ESkin::CardOrnate));
        Footer->SetContent(SNew(SBox).HAlign(HAlign_Center)[Btn(LOCTEXT("RaceAck","确认"),EButtonSkin::Gold,TEXT("Race.Ack"),FSimpleDelegate::CreateLambda([this]{Exec(TEXT("RaceAck"));}),true,64.f,220.f)]);
        break;
    }
    default:
    {
        Add(Paper(Rows({
            Head(LOCTEXT("ReqHead","报名条件"),26.f),
            Pair(LOCTEXT("ReqGuild","武灵"),TAttribute<FText>::CreateLambda([this]{const FXianTuSave* X2=S();return !X2||!X2->Guild.bCreated?LOCTEXT("NoGuildYet","未创建（前往武灵山门免费创建）"):X2->Guild.Name.IsEmpty()?LOCTEXT("GuildReview","名称审核中"):FText::FromString(X2->Guild.Name);})),
            Pair(LOCTEXT("ReqMembers","在场正式成员"),TAttribute<FText>::CreateLambda([this]{const UXianTuSubsystem* Svc2=X();return Svc2?FText::Format(LOCTEXT("MembersN","{0} 人（至少1人；临时助战不计）"),N(Svc2->FormalMembers(true))):FText();})),
            Pair(LOCTEXT("ReqLease","开采权"),TAttribute<FText>::CreateLambda([this]{const UXianTuSubsystem* Svc2=X();return Svc2&&Svc2->ActiveLease()?LOCTEXT("HasLease","生效中，到期后再报名"):LOCTEXT("NoLeaseOk","无（可报名）");})),
            Hint(LOCTEXT("RaceRules","报名免费。2–4个武灵同场采集试炼：6个共同任务（各100）+ 主播两次时机操作（各100）+ 代表响应（满200）。达到600分按名次获得限时开采权；失败无惩罚，可立即回到世界历练。对手为本地模拟。"))
        })));
        for(const FNode& Vein:Nodes())
        {
            const FName Id=Vein.Id;
            Add(Paper(SNew(SHorizontalBox)
                +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(0.f,0.f,14.f,0.f))[SNew(SBox).WidthOverride(150.f)[Scene(Vein.Scene,96.f,false)]]
                +SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
                [
                    Rows({Head(Vein.Name,28.f),Line(Vein.Owner,21.f),Hint(Vein.Interval>1?FText::Format(LOCTEXT("NodeInterval","收集类按累计有效在线 {0} 秒一档 · 开采权最长 30 分钟"),N(Vein.Interval)):LOCTEXT("NodeQi","灵泉逐秒生效 · 开采权最长 30 分钟"))},2.f)
                ]
                +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(10.f,0.f,0.f,0.f))
                [Btn(LOCTEXT("Queue","报名"),EButtonSkin::Gold,FName(*(TEXT("Race.Queue.")+Id.ToString())),FSimpleDelegate::CreateLambda([this,Id]{Exec(TEXT("RaceQueue"),{Id.ToString()});}),true,58.f,120.f)]));
        }
        break;
    }
    }
    Body->SetContent(List);
}

// ------------------------------------------------------------------ 武灵山门

void SInkGuildScreen::Construct(const FArguments& InArgs)
{
    InitScreen(InArgs._Root.ToSharedRef(),TEXT("Wuling"));
    BuildShell(LOCTEXT("GuildTitle","武灵山门"),LOCTEXT("GuildSeal","宗"),
        Tabs(TEXT("Guild.Tabs"),{LOCTEXT("GTab0","总览"),LOCTEXT("GTab1","成员"),LOCTEXT("GTab2","建设"),LOCTEXT("GTab3","记录")}));
}

int32 SInkGuildScreen::Revision() const{return M().XianTuRev;}

void SInkGuildScreen::Rebuild()
{
    const FXianTuSave* St=S();UXianTuSubsystem* Svc=X();
    Footer->SetContent(SNullWidget::NullWidget);
    if(!St||!Svc){Body->SetContent(Paper(Hint(LOCTEXT("GuildOffline","进入游戏后可创建与管理武灵。"))));return;}
    const FXianTuGuild& G=St->Guild;
    const TSharedRef<SVerticalBox> List=SNew(SVerticalBox);int32 Order=0;
    auto Add=[&](TSharedRef<SWidget> Widget){List->AddSlot().AutoHeight().Padding(FMargin(0.f,6.f))[Appear(Widget,Order++)];};
    auto NameBox=[&](const FText& HintText)
    {
        return SAssignNew(Input,SEditableTextBox).Style(&InputStyle()).Font(Font(EFont::Body,26.f)).HintText(HintText).ForegroundColor(FSlateColor(Colors().TextDark));
    };
    if(!G.bCreated)
    {
        Add(Paper(Rows({
            Head(LOCTEXT("CreateHead","开宗立派 · 免费创建武灵"),30.f),
            Line(LOCTEXT("CreateBody","一名主播最多创建一个武灵。观众发送“加入”后以剑灵身份成为正式成员；成员数量本身不提供数值加成。"),22.f),
            NameBox(LOCTEXT("NameHint","武灵名称（2–12个汉字等效长度）")),
            Hint(LOCTEXT("CreateRules","名称需经审核后展示（本地审核约5秒，含“官方/管理/客服”等词会被驳回，可修改后重新提交）；每7日可免费改名一次，审核通过前保留旧名。")),
            SNew(SBox).HAlign(HAlign_Right)[Btn(LOCTEXT("Create","免费创建"),EButtonSkin::Gold,TEXT("Guild.Create"),FSimpleDelegate::CreateLambda([this]{Exec(TEXT("GuildCreate"),{Input?Input->GetText().ToString():FString()});}),true,64.f,220.f)]
        }),FMargin(28.f,20.f,28.f,22.f),Art::ESkin::CardOrnate));
        Body->SetContent(List);return;
    }
    if(Tab==0)
    {
        Add(Paper(Rows({
            Head(G.Name.IsEmpty()?FText::Format(LOCTEXT("PendingName","名称审核中：{0}"),FText::FromString(G.PendingName)):FText::FromString(G.Name),36.f),
            G.RejectReason.IsEmpty()?(G.PendingName.IsEmpty()||G.Name.IsEmpty()?Gap(2.f):Hint(FText::Format(LOCTEXT("Renaming","改名审核中：{0}（通过前保留原名）"),FText::FromString(G.PendingName)))):Line(FText::FromString(G.RejectReason),21.f,Colors().TextRed),
            Pair(LOCTEXT("Total","总成员"),TAttribute<FText>::CreateLambda([this]{const UXianTuSubsystem* Q=X();return Q?N(Q->FormalMembers(false)):FText();})),
            Pair(LOCTEXT("Present","当前参与成员"),TAttribute<FText>::CreateLambda([this]{const UXianTuSubsystem* Q=X();return Q?N(Q->FormalMembers(true)):FText();})),
            Pair(LOCTEXT("Helpers","临时助战"),TAttribute<FText>::CreateLambda([this]{const UXianTuSubsystem* Q=X();return Q?N(Q->TemporaryHelpers()):FText();})),
            Pair(LOCTEXT("Materials","公共资材"),TAttribute<FText>::CreateLambda([this]{const FXianTuSave* X2=S();return X2?FText::Format(LOCTEXT("MatN","{0}（今日 {1}/100）"),N(X2->Guild.Materials),N(X2->Guild.DailyMaterials)):FText();})),
            Pair(LOCTEXT("Rep","声望"),TAttribute<FText>::CreateLambda([this]{const FXianTuSave* X2=S();return X2?FText::Format(LOCTEXT("RepN","{0}（今日 {1}/60）"),N(X2->Guild.Reputation),N(X2->Guild.DailyReputation)):FText();})),
            Pair(LOCTEXT("LeaseNow","开采权"),TAttribute<FText>::CreateLambda([this]{const UXianTuSubsystem* Q=X();const FXianTuLease* L=Q?Q->ActiveLease():nullptr;const FNode* Nd=L?Node(L->Node):nullptr;return Nd?Nd->Name:LOCTEXT("None","无");})),
            Hint(LOCTEXT("GuildNote","总成员、当前参与成员与临时助战分开统计；公共资材只由正式成员完成的世界历练产生，不能换灵石或个人物品。"))
        }),FMargin(28.f,20.f,28.f,22.f),Art::ESkin::CardOrnate));
        Add(Paper(SNew(SHorizontalBox)
            +SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)[Rows({Head(LOCTEXT("GuildShopHead","商城与建设"),26.f),
                Pair(LOCTEXT("WulingCoin","武灵"),TAttribute<FText>::CreateLambda([this]{const UXianTuSubsystem* Q=X();return Q?FText::AsNumber(Q->Wallet(XianTuCatalog::ECurrency::Wuling)):FText();}))},2.f)]
            +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(8.f,0.f,8.f,0.f))[Btn(LOCTEXT("GuildShop","武灵商城"),EButtonSkin::Gold,TEXT("Guild.Shop"),FSimpleDelegate::CreateLambda([this]{if(const auto R=GetRoot())R->OpenPage(TEXT("WulingShop"));}),true,56.f,150.f,22.f)]
            +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Btn(LOCTEXT("GuildBuildPage","武灵建设"),EButtonSkin::Jade,TEXT("Guild.Construction"),FSimpleDelegate::CreateLambda([this]{if(const auto R=GetRoot())R->OpenPage(TEXT("GuildConstruction"));}),true,56.f,150.f,22.f)],
            FMargin(24.f,14.f,24.f,16.f)));
        Add(Paper(Rows({
            Head(LOCTEXT("RenameHead","改名"),26.f),NameBox(LOCTEXT("RenameHint","新名称（每7日一次）")),
            SNew(SBox).HAlign(HAlign_Right)[Btn(LOCTEXT("Rename","提交审核"),EButtonSkin::Paper,TEXT("Guild.Rename"),FSimpleDelegate::CreateLambda([this]{Exec(TEXT("GuildCreate"),{Input?Input->GetText().ToString():FString()});}),true,58.f,180.f)]
        })));
        Footer->SetContent(SNew(SBox).HAlign(HAlign_Center)[Btn(LOCTEXT("Disband","解散武灵"),EButtonSkin::Cinnabar,TEXT("Guild.Disband"),FSimpleDelegate::CreateLambda([this]
        {
            if(const auto R=GetRoot())R->Confirm(LOCTEXT("DisbandT","解散武灵"),LOCTEXT("DisbandB","竞采进行中不能解散。解散后公共资材、建设与名称清空；成员的个人境界、剑灵与礼物权益不受影响。"),Guarded([this]{Exec(TEXT("GuildDisband"));}),true,LOCTEXT("DisbandC","解散"));
        }),true,58.f,200.f)]);
    }
    else if(Tab==1)
    {
        TArray<const FXianTuMember*> Members;for(const FXianTuMember& M2:G.Members)Members.Add(&M2);
        Members.Sort([](const FXianTuMember& A,const FXianTuMember& B){if(A.bFormal!=B.bFormal)return A.bFormal;if(A.bPresent!=B.bPresent)return A.bPresent;return A.Contribution>B.Contribution;});
        const int32 Pages=FMath::Max(1,FMath::DivideAndRoundUp(Members.Num(),20));Page=FMath::Clamp(Page,0,Pages-1);
        for(int32 I=Page*20;I<FMath::Min(Members.Num(),Page*20+20);++I)
        {
            const FXianTuMember& M2=*Members[I];const FString Id=M2.Id,Name=M2.Name;
            TSharedRef<SWidget> Action=M2.bFormal?Btn(LOCTEXT("Remove","移出"),EButtonSkin::Paper,FName(*FString::Printf(TEXT("Guild.Remove.%d"),I)),FSimpleDelegate::CreateLambda([this,Id,Name]
            {
                if(const auto R=GetRoot())R->Confirm(LOCTEXT("RemoveT","移出成员"),FText::Format(LOCTEXT("RemoveB","确定将 {0} 移出武灵？其个人境界与剑灵权益保留，24小时内不能加入新武灵，仍可临时助战。"),FText::FromString(Name)),Guarded([this,Id]{Exec(TEXT("GuildRemove"),{Id});}),true,LOCTEXT("RemoveC","移出"));
            }),true,50.f,100.f,21.f):StaticCastSharedRef<SWidget>(Plaque(LOCTEXT("Temp","临时助战"),1,110.f,34.f));
            Add(Paper(SNew(SHorizontalBox)
                +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(0.f,0.f,12.f,0.f))[SNew(SInkAvatar).Diameter(54.f).Initial(FText::FromString(Name.Left(1))).Ring(4)]
                +SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
                [
                    Rows({Line(FText::FromString(Name),23.f),Hint(FText::Format(LOCTEXT("MemberRow","{0} · {1} · 今日资材 {2}/6 · 累计贡献 {3}"),M2.bFormal?LOCTEXT("Formal","正式成员"):LOCTEXT("TempM","临时助战"),M2.bPresent?LOCTEXT("Here","在场"):LOCTEXT("Away","离场"),N(M2.DailyMaterials),N(M2.Contribution)),19.f)},1.f)
                ]
                +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Action],FMargin(22.f,12.f,22.f,12.f)));
        }
        if(!Members.Num())Add(Paper(Hint(LOCTEXT("NoMembers","暂无成员。观众发送“加入”后会以剑灵身份加入武灵。"))));
        Footer->SetContent(SNew(SHorizontalBox)
            +SHorizontalBox::Slot().FillWidth(1.f).HAlign(HAlign_Left)[Btn(LOCTEXT("Prev","上一页"),EButtonSkin::Paper,TEXT("Guild.Prev"),FSimpleDelegate::CreateLambda([this]{Page=FMath::Max(0,Page-1);Refresh();}),Page>0,56.f,150.f)]
            +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[MakeText(FText::Format(LOCTEXT("PageN","第 {0}/{1} 页 · 每页20"),N(Page+1),N(Pages)),EFont::Small,Colors().TextDark,21.f)]
            +SHorizontalBox::Slot().FillWidth(1.f).HAlign(HAlign_Right)[Btn(LOCTEXT("Next","下一页"),EButtonSkin::Paper,TEXT("Guild.Next"),FSimpleDelegate::CreateLambda([this]{++Page;Refresh();}),Page+1<Pages,56.f,150.f)]);
    }
    else if(Tab==2)
    {
        // v1.4 武灵建设: eight buildings in three looks each; the full page carries costs and the vault.
        const TSharedRef<SWrapBox> Levels=SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(8.f,8.f));
        for(const XianTuCatalog::FGuildBuilding& B:XianTuCatalog::GuildBuildings())
            Levels->AddSlot()[Plaque(FText::Format(LOCTEXT("BuildLevel","{0} {1}/3"),B.Name,N(G.BuildLevels.FindRef(B.Id))),G.BuildLevels.FindRef(B.Id)>0?3:1,150.f,36.f)];
        Add(Paper(Rows({Head(LOCTEXT("BuildHead","武灵建设"),28.f),
            Pair(LOCTEXT("SupplyNow","公共资材"),TAttribute<FText>::CreateLambda([this]{const FXianTuSave* X2=S();return X2?N(X2->Guild.Materials):FText();})),
            Levels,
            SNew(SHorizontalBox)
            +SHorizontalBox::Slot().AutoWidth().Padding(FMargin(0.f,0.f,10.f,0.f))[Btn(LOCTEXT("OpenBuild","进入武灵建设"),EButtonSkin::Gold,TEXT("Guild.OpenConstruction"),FSimpleDelegate::CreateLambda([this]{if(const auto R=GetRoot())R->OpenPage(TEXT("GuildConstruction"));}),true,58.f,220.f)]
            +SHorizontalBox::Slot().AutoWidth()[Btn(LOCTEXT("OpenWulingShop","武灵商城"),EButtonSkin::Jade,TEXT("Guild.OpenShop"),FSimpleDelegate::CreateLambda([this]{if(const auto R=GetRoot())R->OpenPage(TEXT("WulingShop"));}),true,58.f,170.f)]},8.f),
            FMargin(26.f,18.f,26.f,20.f),Art::ESkin::CardOrnate));
        Add(Paper(Hint(LOCTEXT("BuildNote","建设只解锁公开外观档位，不叠加全体修炼倍率；仅主播可按公示用途支出公共资材与公库建设材料。"))));
    }
    else
    {
        int32 Count=0;
        for(const FXianTuRecord& Rec:G.Records)
        {
            Add(Paper(SNew(SHorizontalBox)
                +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(0.f,0.f,12.f,0.f))[Plaque(FText::FromString(Rec.Kind),3,80.f,34.f)]
                +SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)[Line(FText::FromString(Rec.Text),20.f)],FMargin(22.f,12.f,22.f,12.f)));
            if(++Count>=30)break;
        }
        if(!Count)Add(Paper(Hint(LOCTEXT("NoGuildRec","暂无记录。"))));
    }
    Body->SetContent(List);
}

// ------------------------------------------------------------------ 云山坊市

void SInkMarketScreen::Construct(const FArguments& InArgs)
{
    InitScreen(InArgs._Root.ToSharedRef(),TEXT("Market"));
    Pick1=10;Pick2=8;
    BuildShell(LOCTEXT("MarketTitle","云山坊市"),LOCTEXT("MarketSeal","市"),
        SNew(SVerticalBox)
        +SVerticalBox::Slot().AutoHeight()[Tabs(TEXT("Market.Tabs"),{LOCTEXT("MTab0","坊市"),LOCTEXT("MTab1","寄售"),LOCTEXT("MTab2","仓库"),LOCTEXT("MTab3","收据")})]
        +SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(FMargin(0.f,6.f,0.f,0.f))
        [MakeText(TAttribute<FText>::CreateLambda([this]{const UXianTuSubsystem* Q=X();return Q?FText::Format(LOCTEXT("Wallet","灵石 {0} · 灵草 {1} · 玄铁 {2}（可交易）"),N(Q->Stones()),N(Q->ItemCount(TEXT("MAT_HERB_T"))),N(Q->ItemCount(TEXT("MAT_IRON_T")))):FText();}),EFont::Data,Colors().TextDark,22.f)]);
}

int32 SInkMarketScreen::Revision() const{return M().XianTuRev;}

void SInkMarketScreen::Rebuild()
{
    const FXianTuSave* St=S();UXianTuSubsystem* Svc=X();
    Footer->SetContent(SNullWidget::NullWidget);
    if(!St||!Svc){Body->SetContent(Paper(Hint(LOCTEXT("MarketOffline","进入游戏后可使用坊市。"))));return;}
    const TSharedRef<SVerticalBox> List=SNew(SVerticalBox);int32 Order=0;
    auto Add=[&](TSharedRef<SWidget> Widget){List->AddSlot().AutoHeight().Padding(FMargin(0.f,5.f))[Appear(Widget,Order++)];};
    const FText Rule=LOCTEXT("MarketRule","原型模块：仅主播之间以灵石寄售世界历练免费产出的可交易灵草、玄铁；观众无交易权限，绑定物、开采权与礼物权益均不可交易。买卖对手为本地模拟坊市。");
    if(Tab==0)
    {
        Add(Paper(Hint(Rule)));
        for(const FXianTuOrder& O:St->Orders)
        {
            if(O.bMine||O.State!=EXianTuOrderState::Listed)continue;
            const FString Id=O.Id;const int32 Total=O.Count*O.UnitPrice;const FText Name=ItemName(O.Item);
            Add(Paper(SNew(SHorizontalBox)
                +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(0.f,0.f,12.f,0.f))[Cell(O.Item,O.Count,64.f)]
                +SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
                [Rows({Line(FText::Format(LOCTEXT("OrderLine","{0} ×{1} · 单价 {2} · 总价 {3}"),Name,N(O.Count),N(O.UnitPrice),N(Total)),22.f),
                    Hint(FText::Format(LOCTEXT("OrderSeller","{0}（本地模拟）· 剩余 {1}"),FText::FromString(O.Seller),Clock(O.Expires-St->SimNow)),18.f)},1.f)]
                +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                [Btn(LOCTEXT("Buy","买入"),EButtonSkin::Gold,FName(*(TEXT("Market.Buy.")+Id)),FSimpleDelegate::CreateLambda([this,Id,Name,Total]
                {
                    if(const auto R=GetRoot())R->Confirm(LOCTEXT("BuyT","整单买入"),FText::Format(LOCTEXT("BuyB","以 {0} 灵石整单买入 {1}？到手后可绑定用于配方或再次寄售。"),N(Total),Name),Guarded([this,Id]{Exec(TEXT("MarketBuy"),{Id});}),false,LOCTEXT("BuyC","买入"));
                }),Svc->Stones()>=Total,54.f,110.f,21.f)],FMargin(22.f,12.f,22.f,12.f)));
        }
    }
    else if(Tab==1)
    {
        const FName Item=Pick0?FName(TEXT("MAT_IRON_T")):FName(TEXT("MAT_HERB_T"));
        int32 Min,Max,Recycle;MarketRange(Item,Min,Max,Recycle);Pick2=FMath::Clamp(Pick2,Min,Max);Pick1=FMath::Clamp(Pick1,1,100);
        auto Step=[this](FName Id,int32* Value,int32 Delta,int32 Lo,int32 Hi,const TCHAR* Label)
        {
            return Btn(FText::FromString(Label),EButtonSkin::Paper,Id,FSimpleDelegate::CreateLambda([this,Value,Delta,Lo,Hi]{*Value=FMath::Clamp(*Value+Delta,Lo,Hi);Refresh();}),true,48.f,72.f,20.f);
        };
        Add(Paper(Rows({
            Head(LOCTEXT("ListHead","新寄售"),28.f),
            SNew(SInkTabBar).Tabs(TArray<FText>{LOCTEXT("Herb","灵草"),LOCTEXT("Iron","玄铁")}).Selected(Pick0).Height(56.f).TestPrefix(TEXT("Market.Item")).OnSelected_Lambda([this](int32 I){Pick0=I;Refresh();}),
            SNew(SHorizontalBox)
            +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[MakeText(FText::Format(LOCTEXT("CountN","数量 {0}"),N(Pick1)),EFont::Data,Colors().TextDark,24.f)]
            +SHorizontalBox::Slot().FillWidth(1.f)[SNullWidget::NullWidget]
            +SHorizontalBox::Slot().AutoWidth().Padding(FMargin(3.f,0.f))[Step(TEXT("Market.Count.M10"),&Pick1,-10,1,100,TEXT("-10"))]
            +SHorizontalBox::Slot().AutoWidth().Padding(FMargin(3.f,0.f))[Step(TEXT("Market.Count.M1"),&Pick1,-1,1,100,TEXT("-1"))]
            +SHorizontalBox::Slot().AutoWidth().Padding(FMargin(3.f,0.f))[Step(TEXT("Market.Count.P1"),&Pick1,1,1,100,TEXT("+1"))]
            +SHorizontalBox::Slot().AutoWidth().Padding(FMargin(3.f,0.f))[Step(TEXT("Market.Count.P10"),&Pick1,10,1,100,TEXT("+10"))],
            SNew(SHorizontalBox)
            +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[MakeText(FText::Format(LOCTEXT("PriceN","单价 {0}（{1}–{2}）"),N(Pick2),N(Min),N(Max)),EFont::Data,Colors().TextDark,24.f)]
            +SHorizontalBox::Slot().FillWidth(1.f)[SNullWidget::NullWidget]
            +SHorizontalBox::Slot().AutoWidth().Padding(FMargin(3.f,0.f))[Step(TEXT("Market.Price.M1"),&Pick2,-1,Min,Max,TEXT("-1"))]
            +SHorizontalBox::Slot().AutoWidth().Padding(FMargin(3.f,0.f))[Step(TEXT("Market.Price.P1"),&Pick2,1,Min,Max,TEXT("+1"))],
            Line(FText::Format(LOCTEXT("ListPreview","总价 {0} · 挂牌费 1（不退）· 成交税 {1}（向上取整5%）· 成交到手 {2} · 现有 {3}"),N(Pick1*Pick2),N(FMath::CeilToInt(Pick1*Pick2*.05f)),N(Pick1*Pick2-FMath::CeilToInt(Pick1*Pick2*.05f)),N(Svc->ItemCount(Item))),21.f),
            Hint(LOCTEXT("ListRule","同时挂牌最多5笔，单笔1–100份，整单成交，最长24小时；先托管商品与挂牌费再发布，撤单或过期原物退回。")),
            SNew(SBox).HAlign(HAlign_Right)[Btn(LOCTEXT("List","托管挂牌"),EButtonSkin::Gold,TEXT("Market.List"),FSimpleDelegate::CreateLambda([this,Item]{Exec(TEXT("MarketList"),{Item.ToString(),FString::FromInt(Pick1),FString::FromInt(Pick2)});}),Svc->ItemCount(Item)>=Pick1,60.f,200.f)]
        })));
        for(const FXianTuOrder& O:St->Orders)
        {
            if(!O.bMine)continue;
            const FString Id=O.Id;
            Add(Paper(SNew(SHorizontalBox)
                +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(0.f,0.f,12.f,0.f))[Cell(O.Item,O.Count,58.f)]
                +SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
                [Rows({Line(FText::Format(LOCTEXT("MyOrder","{0} ×{1} · 单价 {2} · {3}"),ItemName(O.Item),N(O.Count),N(O.UnitPrice),StateText(O.State)),22.f),
                    Hint(O.State==EXianTuOrderState::Sold?FText::Format(LOCTEXT("SoldInfo","成交税 {0} · 到手 {1} 灵石"),N(O.Tax),N(O.Income)):O.State==EXianTuOrderState::Listed?FText::Format(LOCTEXT("ListedInfo","剩余 {0}"),Clock(O.Expires-St->SimNow)):LOCTEXT("Returned","货物已退回库存"),18.f)},1.f)]
                +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                [O.State==EXianTuOrderState::Listed?Btn(LOCTEXT("CancelOrder","撤单"),EButtonSkin::Paper,FName(*(TEXT("Market.Cancel.")+Id)),FSimpleDelegate::CreateLambda([this,Id]{Exec(TEXT("MarketCancel"),{Id});}),true,52.f,100.f,21.f):StaticCastSharedRef<SWidget>(SNullWidget::NullWidget)],FMargin(22.f,10.f,22.f,10.f)));
        }
    }
    else if(Tab==2)
    {
        for(const TCHAR* Id:{TEXT("MAT_HERB_T"),TEXT("MAT_IRON_T")})
        {
            const FName Item(Id);int32 Min,Max,Recycle;MarketRange(Item,Min,Max,Recycle);const int32 Have=Svc->ItemCount(Item);
            auto Do=[this,Item](const TCHAR* Verb,int32 Count){return FSimpleDelegate::CreateLambda([this,Item,Verb,Count]{Exec(Verb,{Item.ToString(),FString::FromInt(Count)});});};
            Add(Paper(Rows({
                SNew(SHorizontalBox)
                +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(0.f,0.f,12.f,0.f))[Cell(Item,Have,64.f)]
                +SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)[Rows({Head(ItemName(Item),26.f),Hint(FText::Format(LOCTEXT("RecycleInfo","坊市回收 {0} 灵石/份 · 绑定后用于配方，不改变原期限"),N(Recycle)))},1.f)],
                SNew(SHorizontalBox)
                +SHorizontalBox::Slot().AutoWidth().Padding(FMargin(0.f,0.f,6.f,0.f))[Btn(LOCTEXT("Rec1","回收×1"),EButtonSkin::Paper,FName(*(FString(TEXT("Market.Recycle1."))+Id)),Do(TEXT("MarketRecycle"),1),Have>=1,50.f,120.f,20.f)]
                +SHorizontalBox::Slot().AutoWidth().Padding(FMargin(0.f,0.f,6.f,0.f))[Btn(LOCTEXT("Rec10","回收×10"),EButtonSkin::Paper,FName(*(FString(TEXT("Market.Recycle10."))+Id)),Do(TEXT("MarketRecycle"),10),Have>=10,50.f,130.f,20.f)]
                +SHorizontalBox::Slot().AutoWidth().Padding(FMargin(0.f,0.f,6.f,0.f))[Btn(LOCTEXT("Bind1","绑定×1"),EButtonSkin::Jade,FName(*(FString(TEXT("Market.Bind1."))+Id)),Do(TEXT("MarketBind"),1),Have>=1,50.f,120.f,20.f)]
                +SHorizontalBox::Slot().AutoWidth()[Btn(LOCTEXT("Bind10","绑定×10"),EButtonSkin::Jade,FName(*(FString(TEXT("Market.Bind10."))+Id)),Do(TEXT("MarketBind"),10),Have>=10,50.f,130.f,20.f)]
            })));
        }
        TArray<FXianTuReward> Bound;
        for(const TCHAR* Id:{TEXT("MAT_HERB_B"),TEXT("MAT_IRON_B"),TEXT("MAT_PAGE"),TEXT("MAT_BREAK")}){FXianTuReward R;R.Item=Id;R.Count=Svc->ItemCount(Id);Bound.Add(R);}
        Add(Paper(Rows({Head(LOCTEXT("BoundHead","绑定材料（不可交易）"),26.f),Strip(Bound,60.f),Hint(Rule)})));
    }
    else
    {
        int32 Count=0;
        for(const FXianTuRecord& Rec:St->Receipts)if(Rec.Kind==TEXT("坊市"))
        {
            Add(Paper(Line(FText::FromString(Rec.Text),20.f),FMargin(22.f,12.f,22.f,12.f)));
            if(++Count>=30)break;
        }
        if(!Count)Add(Paper(Hint(LOCTEXT("NoMarketRec","暂无坊市收据。"))));
    }
    Body->SetContent(List);
}

// ------------------------------------------------------------------ 剑灵谱

void SInkSwordBookScreen::Construct(const FArguments& InArgs)
{
    InitScreen(InArgs._Root.ToSharedRef(),TEXT("SwordSpirit"));
    BuildShell(LOCTEXT("BookTitle","剑灵谱"),LOCTEXT("BookSeal","剑"),
        Tabs(TEXT("Book.Tabs"),{LOCTEXT("BTab0","剑灵名录"),LOCTEXT("BTab1","特殊剑"),LOCTEXT("BTab2","礼物权益")}));
}

int32 SInkSwordBookScreen::Revision() const{return M().SpiritsRev*7+Tab;}

void SInkSwordBookScreen::Rebuild()
{
    const TSharedRef<SVerticalBox> List=SNew(SVerticalBox);int32 Order=0;
    auto Add=[&](TSharedRef<SWidget> Widget){List->AddSlot().AutoHeight().Padding(FMargin(0.f,5.f))[Appear(Widget,Order++)];};
    Footer->SetContent(SNullWidget::NullWidget);
    const TArray<FSpiritView>& Spirits=M().Spirits;
    if(Tab==0)
    {
        Add(Paper(Rows({
            Head(FText::Format(LOCTEXT("RosterHead","在场剑灵 {0} 把"),N(Spirits.Num())),28.f),
            Hint(FText::Format(LOCTEXT("RosterHint","观众剑灵的模型从 GanzSe 武器包 {0} 件静态网格中随机取一件（同一观众固定）；赠送对应礼物可化为特殊剑。"),N(M().AudienceMeshes)))
        })));
        const int32 Pages=FMath::Max(1,FMath::DivideAndRoundUp(Spirits.Num(),20));Page=FMath::Clamp(Page,0,Pages-1);
        for(int32 I=Page*20;I<FMath::Min(Spirits.Num(),Page*20+20);++I)
        {
            const FSpiritView& V=Spirits[I];const ELiveSpecialSword Kind=ELiveSpecialSword(V.Special);const FString Id=V.Id;
            const FLinearColor Accent=V.Special?LiveSpecialSwords::Color(Kind):Colors().GoldDeep;
            Add(SNew(SOverlay)
                +SOverlay::Slot()[SNew(SInkPaint).OnPaint_Lambda([Accent](FPainter& P,const FGeometry&)
                {
                    P.Skin(Art::ESkin::CardPaper,FVector2f(0.f,0.f),P.Size(),FLinearColor::White,.45f);P.Next();
                    P.Rounded(FVector2f(8.f,10.f),FVector2f(6.f,P.Size().Y-20.f),WithAlpha(Accent,.95f),3.f);
                })]
                +SOverlay::Slot().Padding(FMargin(26.f,12.f,22.f,12.f))
                [
                    SNew(SHorizontalBox)
                    +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(0.f,0.f,12.f,0.f))[SNew(SInkAvatar).Diameter(56.f).Initial(FText::FromString(V.Name.ToString().Left(1))).Ring(V.Special?1:4)]
                    +SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
                    [
                        Rows({
                            Line(FText::Format(LOCTEXT("SpiritName","{0} · {1}"),V.Name,V.Realm),23.f),
                            Hint(V.Special?FText::Format(LOCTEXT("SpecialLine","{0} · 攻击 {1} · 累计伤害 {2}"),LiveSpecialSwords::Name(Kind),FormatAmount(V.Attack),FormatAmount(V.Damage)):FText::Format(LOCTEXT("PlainLine","{0} · 攻击 {1}"),FText::FromString(PrettyMesh(V.Mesh)),FormatAmount(V.Attack)),19.f),
                            V.Special?StaticCastSharedRef<SWidget>(MakeText(TAttribute<FText>::CreateLambda([this,Id]
                            {
                                const FSpiritView* Live=M().Spirits.FindByPredicate([&](const FSpiritView& Q){return Q.Id==Id;});if(!Live)return FText();
                                FString Out=Live->Status.ToString();
                                for(const FSpiritSkillView& K:Live->Skills)if(!K.bPassive)
                                {
                                    // A skill in progress (twin clones) parks its cooldown at infinity until it resolves.
                                    const FString State=K.bActive||K.Remaining>9999.f?FString(TEXT("施放中")):K.Remaining>0?FString::Printf(TEXT("%.0f秒"),K.Remaining):FString(TEXT("就绪"));
                                    Out+=FString::Printf(TEXT(" · %s %s"),*K.Name.ToString(),*State);
                                }
                                return FText::FromString(Out);
                            }),EFont::Small,Lerp(Accent,Colors().TextDark,.55f),19.f)):Gap(1.f)
                        },1.f)
                    ]
                ]);
        }
        if(!Spirits.Num())Add(Paper(Hint(LOCTEXT("NoSpirits","暂无观众剑灵。在 GM 面板“观众”页加入模拟观众，或等待观众发送“加入”。"))));
        Footer->SetContent(SNew(SHorizontalBox)
            +SHorizontalBox::Slot().FillWidth(1.f).HAlign(HAlign_Left)[Btn(LOCTEXT("BPrev","上一页"),EButtonSkin::Paper,TEXT("Book.Prev"),FSimpleDelegate::CreateLambda([this]{Page=FMath::Max(0,Page-1);Refresh();}),Page>0,56.f,150.f)]
            +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[MakeText(FText::Format(LOCTEXT("BPage","第 {0}/{1} 页"),N(Page+1),N(Pages)),EFont::Small,Colors().TextDark,21.f)]
            +SHorizontalBox::Slot().FillWidth(1.f).HAlign(HAlign_Right)[Btn(LOCTEXT("BNext","下一页"),EButtonSkin::Paper,TEXT("Book.Next"),FSimpleDelegate::CreateLambda([this]{++Page;Refresh();}),Page+1<Pages,56.f,150.f)]);
    }
    else if(Tab==1)
    {
        for(const ELiveSpecialSword Kind:LiveSpecialSwords::All())
        {
            int32 Holders=0;for(const FSpiritView& V:Spirits)Holders+=V.Special==uint8(Kind)?1:0;
            const FLinearColor Accent=LiveSpecialSwords::Color(Kind);
            TArray<TSharedRef<SWidget>> Parts;
            Parts.Add(SNew(SHorizontalBox)
                +SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)[MakeText(LiveSpecialSwords::Name(Kind),EFont::Title,Lerp(Accent,Colors().TextDark,.45f),40.f)]
                +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Plaque(FText::Format(LOCTEXT("Holders","在场 {0} 把"),N(Holders)),Holders?0:1,120.f,38.f)]);
            Parts.Add(Hint(FText::Format(LOCTEXT("Unlock","{0} · 解锁：赠送「{1}」（等同装备其称号；同一剑灵同时只呈现一种特殊剑形态）"),LiveSpecialSwords::Motto(Kind),FText::FromString(LiveSpecialSwords::GiftName(Kind)))));
            // Attribute sheet: 基础 at 练气一阶 and 成长 per realm; the full table with any realm lives in the 试剑台 剑谱.
            Parts.Add(Line(LiveSwordCodex::Role(Kind),21.f,Lerp(Accent,Colors().TextDark,.55f)));
            Parts.Add(SNew(SHorizontalBox)
                +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top).Padding(FMargin(0.f,0.f,12.f,0.f))[Plaque(LOCTEXT("SheetBase","基础属性"),3,104.f,34.f)]
                +SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)[Hint(LiveSwordCodex::BaseSummary(Kind),19.f)]);
            Parts.Add(SNew(SHorizontalBox)
                +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top).Padding(FMargin(0.f,0.f,12.f,0.f))[Plaque(LOCTEXT("SheetGrowth","成长属性"),0,104.f,34.f)]
                +SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)[Hint(LiveSwordCodex::GrowthSummary(Kind),19.f)]);
            for(const auto& Skill:LiveSpecialSwords::Skills(Kind))
            {
                Parts.Add(SNew(SHorizontalBox)
                    +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top).Padding(FMargin(0.f,2.f,12.f,0.f))[Plaque(Skill.bPassive?LOCTEXT("Passive","被动"):FText::Format(LOCTEXT("CdN","冷却{0}秒"),N(FMath::RoundToInt(Skill.Cooldown))),Skill.bPassive?1:3,104.f,34.f)]
                    +SHorizontalBox::Slot().FillWidth(1.f)[Rows({Line(Skill.Name,23.f),Hint(Skill.Description,19.f)},1.f)]);
            }
            Parts.Add(Hint(LOCTEXT("SkillRule","技能就绪时随机释放其一；释放后 50% 普通攻击、50% 继续释放冷却已好的技能。普攻与技能均带剑气拖尾与 Niagara 特效。")));
            Add(SNew(SOverlay)
                +SOverlay::Slot()[SNew(SInkPaint).OnPaint_Lambda([Accent](FPainter& P,const FGeometry&)
                {
                    P.Skin(Art::ESkin::CardOrnate,FVector2f(0.f,0.f),P.Size(),FLinearColor::White,.42f);P.Next();
                    P.Glow(FVector2f(P.Size().X*.85f,40.f),FVector2f(160.f,60.f),WithAlpha(Accent,.25f));
                })]
                +SOverlay::Slot().Padding(FMargin(30.f,22.f,30.f,24.f))[Rows(Parts,4.f)]);
        }
    }
    else
    {
        struct FGiftLine{const TCHAR* Name;const TCHAR* Effect;const TCHAR* Sword;};
        static const FGiftLine Gifts[]={
            {TEXT("仙女棒"),TEXT("送礼观众修炼速度 ×1.15 起，连送叠加倍率与时长（30秒/份）"),TEXT("")},
            {TEXT("爱的爆炸"),TEXT("送礼观众修为 +19999，立即生效"),TEXT("")},
            {TEXT("稀有宝箱"),TEXT("永久自身属性 ×2.8，周期共享给全场"),TEXT("噬魂剑")},
            {TEXT("超能喷射"),TEXT("全场攻击/灵伤增益，主播另加生命与防御"),TEXT("无机态那")},
            {TEXT("神秘空投"),TEXT("持有者在场时场内怪物属性 -20%"),TEXT("驱煞神刃")},
            {TEXT("草莓甜点"),TEXT("主播低血治疗储备 7 次，独立冷却"),TEXT("七星光芒剑")},
            {TEXT("超级空投"),TEXT("持有者在场时主播 20% 概率免伤"),TEXT("弑神剑")},
            {TEXT("生命药水"),TEXT("长时修炼倍率，同房观众与主播共享"),TEXT("")}};
        Add(Paper(Hint(LOCTEXT("GiftNote","礼物原有权益保持基线不变；五件礼物额外让送礼观众的剑灵化为对应特殊剑（本地演示，正式价格以平台配置为准，点击说明不触发购买）。"))));
        for(const FGiftLine& G:Gifts)
        {
            Add(Paper(SNew(SHorizontalBox)
                +SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)[Rows({Head(FText::FromString(G.Name),26.f),Hint(FText::FromString(G.Effect),19.f)},1.f)]
                +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[*G.Sword?StaticCastSharedRef<SWidget>(Plaque(FText::FromString(FString(TEXT("化剑·"))+G.Sword),3,170.f,40.f)):StaticCastSharedRef<SWidget>(SNullWidget::NullWidget)],FMargin(24.f,14.f,24.f,14.f)));
        }
    }
    Body->SetContent(List);
}

// ------------------------------------------------------------------ 功法典藏

void SInkMethodsScreen::Construct(const FArguments& InArgs)
{
    InitScreen(InArgs._Root.ToSharedRef(),TEXT("Methods"));
    BuildShell(LOCTEXT("MethodsTitle","功法典藏"),LOCTEXT("MethodsSeal","法"),
        SNew(SBox).HAlign(HAlign_Center)
        [
            MakeText(TAttribute<FText>::CreateLambda([this]
            {
                const UXianTuSubsystem* Q=X();const FXianTuSave* St=S();if(!Q||!St)return LOCTEXT("MethodsOff","进入游戏后配置功法");
                return FText::Format(LOCTEXT("MethodsHead","主修 {0}/1 · 辅修 {1}/{2} · 残页 {3} · 灵石 {4}"),N(St->Methods.Main.IsNone()?0:1),N(St->Methods.Support.Num()),N(Q->SupportSlots()),N(Q->ItemCount(TEXT("MAT_PAGE"))),N(Q->Stones()));
            }),EFont::Section,Colors().TextDark,23.f)
        ]);
}

int32 SInkMethodsScreen::Revision() const{return M().XianTuRev;}

void SInkMethodsScreen::Rebuild()
{
    const FXianTuSave* St=S();UXianTuSubsystem* Svc=X();
    Footer->SetContent(SNullWidget::NullWidget);
    if(!St||!Svc){Body->SetContent(Paper(Hint(LOCTEXT("MethodsOffline","进入游戏后可配置功法。"))));return;}
    const FXianTuMethods& Ms=St->Methods;
    const TSharedRef<SVerticalBox> List=SNew(SVerticalBox);int32 Order=0;
    auto Add=[&](TSharedRef<SWidget> Widget){List->AddSlot().AutoHeight().Padding(FMargin(0.f,5.f))[Appear(Widget,Order++)];};
    auto SlotName=[&](FName Id){const FMethod* Md=Method(Id);return Md?Md->Name:LOCTEXT("Empty","空");};
    Add(Paper(Rows({
        Head(LOCTEXT("SlotsHead","构筑槽位"),27.f),
        SNew(SHorizontalBox)
        +SHorizontalBox::Slot().FillWidth(1.f).Padding(FMargin(0.f,0.f,6.f,0.f))[Plaque(FText::Format(LOCTEXT("MainSlot","主修 · {0}"),SlotName(Ms.Main)),3,200.f,44.f)]
        +SHorizontalBox::Slot().FillWidth(1.f).Padding(FMargin(0.f,0.f,6.f,0.f))[Plaque(FText::Format(LOCTEXT("Sup1","辅修一 · {0}"),SlotName(Ms.Support.IsValidIndex(0)?Ms.Support[0]:NAME_None)),1,200.f,44.f)]
        +SHorizontalBox::Slot().FillWidth(1.f)[Plaque(FText::Format(LOCTEXT("Sup2","辅修二 · {0}"),SlotName(Ms.Support.IsValidIndex(1)?Ms.Support[1]:NAME_None)),1,200.f,44.f)],
        Hint(LOCTEXT("MethodRules","1主修+2辅修（首版练气构筑；筑基后槽位按境界开放）。换装只在准备期；同一部功法不能占两槽。等级 黄→玄→地（首版上限）：研习需 2 残页+50 灵石/卷，升玄需1卷、升地需2卷；第3级强化主系数10%。主修作用于主播本命剑与主播本人，观众剑灵不受影响。"))
    })));
    for(const FMethod& Md:Methods())
    {
        const FName Id=Md.Id;const int32 Level=FMath::Max(1,Ms.Levels.FindRef(Id));const bool bMain=Ms.Main==Id,bSup=Ms.Support.Contains(Id);
        TArray<TSharedRef<SWidget>> Parts;
        Parts.Add(SNew(SHorizontalBox)
            +SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)[MakeText(FText::Format(LOCTEXT("MethodName","{0} · {1}"),Md.Name,Md.Route),EFont::Section,Colors().TextDark,28.f)]
            +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(6.f,0.f))[Plaque(FText::Format(LOCTEXT("LevelP","{0}阶"),LevelLabel(Level)),3,70.f,36.f)]
            +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Plaque(!Md.bOpen?LOCTEXT("NotOpen","后续开放"):bMain?LOCTEXT("IsMain","主修中"):bSup?LOCTEXT("IsSup","辅修中"):LOCTEXT("Idle","未装配"),!Md.bOpen?1:(bMain||bSup)?0:1,106.f,36.f)]);
        Parts.Add(Line(FText::Format(LOCTEXT("MainMech","主修：{0}"),Md.Main),21.f));
        Parts.Add(Line(FText::Format(LOCTEXT("SupMech","辅修：{0}"),Md.Support),21.f));
        Parts.Add(Hint(FText::Format(LOCTEXT("LiveNote","{0}{1}"),Md.Live,Md.bOpen&&Level<3?FText::Format(LOCTEXT("StudyProg","  · 研习 {0}/{1}"),N(Ms.Progress.FindRef(Id)),N(StudyNeeded(Level+1))):FText::GetEmpty())));
        if(Md.bOpen)
        {
            Parts.Add(SNew(SHorizontalBox)
                +SHorizontalBox::Slot().AutoWidth().Padding(FMargin(0.f,0.f,8.f,0.f))[Btn(bMain?LOCTEXT("UnMain","卸下主修"):LOCTEXT("SetMain","设为主修"),bMain?EButtonSkin::Paper:EButtonSkin::Gold,FName(*(TEXT("Methods.Main.")+Id.ToString())),FSimpleDelegate::CreateLambda([this,Id,bMain]{Exec(bMain?TEXT("MethodClear"):TEXT("MethodMain"),{Id.ToString()});}),true,54.f,150.f,21.f)]
                +SHorizontalBox::Slot().AutoWidth().Padding(FMargin(0.f,0.f,8.f,0.f))[Btn(bSup?LOCTEXT("UnSup","卸下辅修"):LOCTEXT("SetSup","辅修"),EButtonSkin::Paper,FName(*(TEXT("Methods.Sup.")+Id.ToString())),FSimpleDelegate::CreateLambda([this,Id]{Exec(TEXT("MethodSupport"),{Id.ToString()});}),!bMain,54.f,130.f,21.f)]
                +SHorizontalBox::Slot().AutoWidth()[Btn(LOCTEXT("Study","研习"),EButtonSkin::Jade,FName(*(TEXT("Methods.Study.")+Id.ToString())),FSimpleDelegate::CreateLambda([this,Id]{Exec(TEXT("MethodStudy"),{Id.ToString()});}),Level<3,54.f,120.f,21.f)]);
        }
        Add(Paper(Rows(Parts,3.f)));
    }
    Body->SetContent(List);
}

// ------------------------------------------------------------------ 天劫

void SInkTribulationScreen::Construct(const FArguments& InArgs)
{
    InitScreen(InArgs._Root.ToSharedRef(),TEXT("Tribulation"));
    BuildShell(LOCTEXT("TribTitle","天劫"),LOCTEXT("TribSeal","劫"),
        SNew(SBox).HAlign(HAlign_Center)
        [
            MakeText(TAttribute<FText>::CreateLambda([this]
            {
                const UXianTuSubsystem* Q=X();if(!Q)return LOCTEXT("TribOff","进入游戏后可渡劫");
                return FText::Format(LOCTEXT("TribHead","{0} → {1} · {2}"),Q->AnchorRealmName(),Q->AnchorRealmName(1),IsTribulation(Q->AnchorRealm())?LOCTEXT("NeedTrib","需渡天劫"):LOCTEXT("NoTribHere","此关无天劫"));
            }),EFont::Section,Colors().TextDark,25.f)
        ]);
}

int32 SInkTribulationScreen::Revision() const{return M().XianTuRev;}

void SInkTribulationScreen::Rebuild()
{
    const FXianTuSave* St=S();UXianTuSubsystem* Svc=X();
    Footer->SetContent(SNullWidget::NullWidget);
    if(!St||!Svc){Body->SetContent(Paper(Hint(LOCTEXT("TribOffline","进入游戏后可渡劫。"))));return;}
    const FXianTuTribulation& Tb=St->Trib;
    const TSharedRef<SVerticalBox> List=SNew(SVerticalBox);int32 Order=0;
    auto Add=[&](TSharedRef<SWidget> Widget){List->AddSlot().AutoHeight().Padding(FMargin(0.f,6.f))[Appear(Widget,Order++)];};
    auto Probability=[this](bool bLive)
    {
        return Line(TAttribute<FText>::CreateLambda([this,bLive]
        {
            const UXianTuSubsystem* Q=X();const FXianTuSave* X2=S();if(!Q||!X2)return FText();
            const int32 Assists=bLive?X2->Trib.Assists.Num():0;const int32 Fails=bLive?X2->Trib.Failures.FindRef(FString::FromInt(X2->Trib.FromRealm)):Q->TribulationFailures();
            return FText::Format(LOCTEXT("ProbLine","成功率 {0}% = 基础 50% + 助力 {1}%（{2} 人，上限50）+ 失败保底 {3}%（连败 {4} 次）{5}"),
                N(Q->TribulationProbability(Assists)/100),N(FMath::Min(Assists,50)),N(Assists),N(FMath::Min(Fails*10,50)),N(Fails),
                Q->FortunePenalty()>0?FText::Format(LOCTEXT("OmenPart"," − 灾签 {0}%"),N(Q->FortunePenalty()/100)):FText::GetEmpty());
        }),23.f);
    };
    if(Tb.State==EXianTuTribState::Prepare||Tb.State==EXianTuTribState::Trial||Tb.State==EXianTuTribState::Frozen)
    {
        const bool bTrial=Tb.State==EXianTuTribState::Trial;
        Add(Paper(Rows({
            Head(TAttribute<FText>::CreateLambda([this]
            {
                const FXianTuSave* X2=S();if(!X2)return FText();const FXianTuTribulation& Q=X2->Trib;
                if(Q.State==EXianTuTribState::Prepare)return FText::Format(LOCTEXT("PrepHead","准备 · {0} 后天劫降临"),Clock(15-Q.Elapsed));
                if(Q.State==EXianTuTribState::Frozen)return LOCTEXT("FrozenHead","冻结助力 · 唯一随机对账中");
                static const TCHAR* Phases[]={TEXT("肉身劫"),TEXT("神魂劫"),TEXT("心魔劫")};
                return FText::Format(LOCTEXT("TrialHead","{0} · 剩余 {1}"),FText::FromString(Phases[FMath::Clamp(int32(Q.Elapsed/20),0,2)]),Clock(60-Q.Elapsed));
            }),32.f),
            Timeline({0,20,40,60},{LOCTEXT("P1","肉身"),LOCTEXT("P2","神魂"),LOCTEXT("P3","心魔")},TAttribute<float>::CreateLambda([this]{const FXianTuSave* X2=S();return X2&&X2->Trib.State==EXianTuTribState::Trial?float(X2->Trib.Elapsed/60.0):X2&&X2->Trib.State==EXianTuTribState::Frozen?1.f:0.f;})),
            Probability(true),
            Meter(TAttribute<float>::CreateLambda([this]{const UXianTuSubsystem* Q=X();const FXianTuSave* X2=S();return Q&&X2?Q->TribulationProbability(X2->Trib.Assists.Num())/10000.f:0.f;}),
                TAttribute<FText>::CreateLambda([this]{const FXianTuSave* X2=S();return X2?FText::Format(LOCTEXT("AssistMeter","助力 {0} 人 · 每人+1%，50人封顶；同一观众同次只计一次"),N(X2->Trib.Assists.Num())):FText();}),EBar::Gold),
            Hint(bTrial?LOCTEXT("TrialHint","观众发送“助力”即可提升成功率，其剑灵转入预警护阵；三段只结算一次成功率，不逐段判定。离开界面不能取消已开始的风险。"):LOCTEXT("PrepHint","准备期只说明规则：修为与材料已预留，此时可无损取消；开始后风险不可撤回。"))
        }),FMargin(28.f,20.f,28.f,22.f),Art::ESkin::CardOrnate));
        if(Tb.AssistNames.Num())
        {
            const TSharedRef<SWrapBox> Names=SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(8.f,6.f));
            for(int32 I=0;I<Tb.AssistNames.Num()&&I<60;++I)Names->AddSlot()[Plaque(FText::FromString(Tb.AssistNames[I].Left(6)),0,120.f,34.f)];
            Add(Paper(Rows({Head(LOCTEXT("AssistHead","助力剑灵"),26.f),Names})));
        }
        if(Tb.State==EXianTuTribState::Prepare)
            Footer->SetContent(SNew(SBox).HAlign(HAlign_Center)[Btn(LOCTEXT("TribCancel","无损取消"),EButtonSkin::Paper,TEXT("Trib.Cancel"),FSimpleDelegate::CreateLambda([this]{Exec(TEXT("TribCancel"));}),true,62.f,220.f)]);
        Body->SetContent(List);return;
    }
    if(Tb.State==EXianTuTribState::Backlash)
    {
        Add(Paper(Rows({
            Head(TAttribute<FText>::CreateLambda([this]
            {
                const FXianTuSave* X2=S();return X2?FText::Format(LOCTEXT("BacklashHead","天罚 · 剩余 {0}"),Clock(TribBacklash-X2->Trib.Elapsed)):FText();
            }),32.f),
            Meter(TAttribute<float>::CreateLambda([this]{const FXianTuSave* X2=S();return X2?float(X2->Trib.Elapsed/TribBacklash):0.f;}),
                TAttribute<FText>::CreateLambda([this]
                {
                    const FXianTuSave* X2=S();if(!X2)return FText();
                    const FText Ward=X2->Trib.Ward==EXianTuWard::Held?LOCTEXT("BacklashHeld"," · 七星挡劫，天雷不致陨落")
                        :X2->Trib.Ward==EXianTuWard::Broken?LOCTEXT("BacklashBroken"," · 天雷破盾，七星护持无效"):FText::GetEmpty();
                    return FText::Format(LOCTEXT("BacklashMeter","第 {0}/{1} 道天雷 · 已伤生命 {2}{3}"),N(X2->Trib.BacklashStrikes),N(TribBacklashStrikes),FormatAmount(X2->Trib.BacklashDamage),Ward);
                }),EBar::Red),
            Hint(FText::Format(LOCTEXT("BacklashHint","渡劫失败，天雷反噬：每 0.2 秒落雷一道，一道重过一道，持续 10 秒，合计为生命上限的 100%～200%（成功率越低越重），单凭生命与回复难以扛过。主播倒下则损失修为；扛住则修为保留，但道心破碎（防御 −{0}%，{1} 分钟内不可再渡劫）。七星光芒剑的境界须高出所渡天劫一个大境界，才有 50% 概率挡劫（每再高一个大境界，被破的概率减半）。"),
                N(FMath::RoundToInt(DaoHeartDefense*100)),N(int32(DaoHeartDuration/60))))
        }),FMargin(28.f,20.f,28.f,22.f),Art::ESkin::CardOrnate));
        Body->SetContent(List);return;
    }
    if(Tb.State==EXianTuTribState::Succeeded||Tb.State==EXianTuTribState::Failed||Tb.State==EXianTuTribState::Cancelled)
    {
        const bool bOk=Tb.State==EXianTuTribState::Succeeded;
        const int32 Pity=FMath::Min(Tb.Failures.FindRef(FString::FromInt(Tb.FromRealm))*10,50);
        FText Outcome=FText::GetEmpty();
        if(bOk)Outcome=LOCTEXT("OkLine","境界已提升，失败保底清零。");
        else if(Tb.State==EXianTuTribState::Failed&&Tb.bDaoHeartBroken)
            Outcome=FText::Format(LOCTEXT("HeartLine","扛过 {0} 道天罚 · 修为与材料保留 · 道心破碎：防御 −{1}%，{2} 分钟内不可再渡劫 · 下次保底 +{3}%"),
                N(Tb.BacklashStrikes),N(FMath::RoundToInt(DaoHeartDefense*100)),N(int32(DaoHeartDuration/60)),N(Pity));
        else if(Tb.State==EXianTuTribState::Failed)
            Outcome=FText::Format(LOCTEXT("FailLine","陨落于第 {0} 道天罚 · 损失修为 {1}（上限1500）· 核心与材料保留 · 不掉境界 · 下次保底 +{2}%"),
                N(Tb.BacklashStrikes),N(FMath::RoundToInt(Tb.Loss)),N(Pity));
        if(Tb.State==EXianTuTribState::Failed&&Tb.Ward!=EXianTuWard::None)
            Outcome=FText::Format(LOCTEXT("WardOutcome","{0} · {1}（挡劫 {2}%）"),Outcome,
                Tb.Ward==EXianTuWard::Held?LOCTEXT("WardHeldShort","七星挡劫"):LOCTEXT("WardBrokenShort","天雷破盾"),FText::AsNumber(Tb.WardChance/100.0));
        Add(Paper(Rows({
            Head(Tb.State==EXianTuTribState::Failed?(Tb.bDaoHeartBroken?LOCTEXT("HeartHead","渡劫失败 · 道心破碎"):LOCTEXT("DiedHead","渡劫失败 · 陨落")):StateText(Tb.State),38.f),
            Line(Tb.State==EXianTuTribState::Cancelled?LOCTEXT("CancelledLine","本次渡劫已取消或作废，未扣任何修为与材料。"):
                FText::Format(LOCTEXT("RollLine","随机值 {0} / 成功区间 0–{1}（成功率 {2}%）· 助力 {3} 人"),N(Tb.Roll),N(Tb.Probability-1),N(Tb.Probability/100),N(Tb.Assists.Num())),22.f),
            Line(Outcome,22.f,bOk?Colors().TextJade:Colors().TextRed),
            Hint(FText::Format(LOCTEXT("TribReceipt","收据 {0}"),FText::FromString(Tb.Receipt)))
        }),FMargin(28.f,20.f,28.f,22.f),Art::ESkin::CardOrnate));
        Footer->SetContent(SNew(SBox).HAlign(HAlign_Center)[Btn(LOCTEXT("TribAck","确认"),EButtonSkin::Gold,TEXT("Trib.Ack"),FSimpleDelegate::CreateLambda([this]{Exec(TEXT("TribAck"));}),true,62.f,220.f)]);
        Body->SetContent(List);return;
    }
    auto Check=[this](const FText& Label,TFunction<bool(const UXianTuSubsystem&)> Ok,TFunction<FText(const UXianTuSubsystem&)> Value)
    {
        return SNew(SHorizontalBox)
            +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(0.f,0.f,10.f,0.f))
            [SNew(SInkPaint).DesiredSize(FVector2D(26.f,26.f)).OnPaint_Lambda([this,Ok](FPainter& P,const FGeometry&){const UXianTuSubsystem* Q=X();const bool b=Q&&Ok(*Q);P.Diamond(P.Size()*.5f,FVector2f(9.f),b?Colors().Jade:Colors().Cinnabar);})]
            +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(0.f,0.f,10.f,0.f))[MakeText(Label,EFont::Small,Colors().TextDarkDim,22.f)]
            +SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)[MakeText(TAttribute<FText>::CreateLambda([this,Value]{const UXianTuSubsystem* Q=X();return Q?Value(*Q):FText();}),EFont::Data,Colors().TextDark,22.f)];
    };
    Add(Paper(Rows({
        Head(LOCTEXT("PrepTitle","渡劫准备"),30.f),
        Probability(false),
        Check(LOCTEXT("CQi","修为"),[](const UXianTuSubsystem& Q){return Q.AnchorQi()>=Q.AnchorQiRequired();},[](const UXianTuSubsystem& Q){return FText::Format(LOCTEXT("QiV","{0} / {1}"),FormatAmount(Q.AnchorQi()),FormatAmount(Q.AnchorQiRequired()));}),
        Check(LOCTEXT("CCore","核心材料"),[](const UXianTuSubsystem& Q)
        {
            const XianTuCatalog::FStage* St2=Q.NextStage();if(!St2)return Q.ItemCount(TEXT("MAT_BREAK"))>0;
            int32 Cores=0;for(int32 G=1;G<=4;++G)Cores+=Q.ItemCount(XianTuCatalog::MaterialKey(*St2,XianTuCatalog::ECategory::Core,G));return Cores>0;
        },[](const UXianTuSubsystem& Q)
        {
            const XianTuCatalog::FStage* St2=Q.NextStage();
            if(!St2)return FText::Format(LOCTEXT("StoneV","破境石 {0}（不足时前往世界历练获取）"),N(Q.ItemCount(TEXT("MAT_BREAK"))));
            int32 Cores=0;for(int32 G=1;G<=4;++G)Cores+=Q.ItemCount(XianTuCatalog::MaterialKey(*St2,XianTuCatalog::ECategory::Core,G));
            const FXianTuBreakPlan& Plan=Q.GetPlan();
            if(Plan.Stage==St2->Id&&Plan.Core>0)
                return FText::Format(LOCTEXT("CoreSet","{0}{1} · 突破品质 {2} Q{3}"),XianTuCatalog::GradeName(Plan.Core),St2->Species[0].Name,XianTuCatalog::Bands()[XianTuCatalog::BandIndex(Q.PlanQuality())].Name,N(Q.PlanQuality()));
            return FText::Format(LOCTEXT("CoreHave","{0} 共 {1} 份（未放入时自动取最高品）"),St2->Species[0].Name,N(Cores));
        }),
        Check(LOCTEXT("CReady","条件"),[](const UXianTuSubsystem& Q){return Q.CanStartTribulation(nullptr);},[](const UXianTuSubsystem& Q){FText Why;return Q.CanStartTribulation(&Why)?LOCTEXT("ReadyNow","可以开始渡劫"):Why;}),
        Hint(LOCTEXT("TribRules","金丹六阶冲击元婴起每次大境界突破降下三重天劫（共60秒）。成功才消耗核心与所放辅材并记录突破品质；失败后天雷反噬十息：倒下则损失 min(1500, 未花费修为)；扛住则修为保留，但道心破碎（防御 −30%，10 分钟内不可再渡劫）。材料保留、不掉境界；每次失败下次 +10%，连败五次后第六次必成。礼物、材料品质与付费道具不进入成功率；每日一签抽到「灾」当日 −10%。")),
        SNew(SBox).HAlign(HAlign_Right)[Btn(LOCTEXT("TribPlan","调整突破材料"),EButtonSkin::Paper,TEXT("Trib.Plan"),FSimpleDelegate::CreateLambda([this]{if(const auto R=GetRoot())R->OpenPage(TEXT("Breakthrough"));}),true,54.f,200.f,22.f)]
    }),FMargin(28.f,20.f,28.f,22.f),Art::ESkin::CardOrnate));
    Footer->SetContent(SNew(SBox).HAlign(HAlign_Center)
    [
        Btn(LOCTEXT("TribStart","开始渡劫"),EButtonSkin::Gold,TEXT("Trib.Start"),FSimpleDelegate::CreateLambda([this]
        {
            if(const auto R=GetRoot())R->Confirm(LOCTEXT("TribConfirmT","开始渡劫"),LOCTEXT("TribConfirmB","准备15秒后天劫降临，开始后不能取消。失败后天雷反噬十息：倒下损失至多1500修为，扛住则道心破碎；材料保留、不掉境界。"),Guarded([this]{Exec(TEXT("TribStart"));}),false,LOCTEXT("TribConfirmC","渡劫"));
        }),TAttribute<bool>::CreateLambda([this]{const UXianTuSubsystem* Q=X();return Q&&Q->CanStartTribulation(nullptr);}),72.f,300.f,28.f)
    ]);
    Body->SetContent(List);
}

#undef LOCTEXT_NAMESPACE

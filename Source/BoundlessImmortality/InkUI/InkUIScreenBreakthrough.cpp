// 境界突破 with its material slots and celebration, plus the design pages prepared for later systems: 物品来源,
// 武灵建设 and 装备法宝. All read the 仙途 ledger (XianTuCatalog data); nothing here talks to a service.

#include "InkUI/InkUIScreensXianTu.h"
#include "InkUI/InkUIXianTuKit.h"
#include "XianTu/XianTuSubsystem.h"
#include "XianTu/XianTuCatalog.h"
#include "XianTu/XianTuBreakthroughFx.h"
#include "Combat/LiveCombat.h"
#include "Widgets/Layout/SScrollBox.h"

#define LOCTEXT_NAMESPACE "InkUIBreakthrough"

using namespace InkUI;
using namespace InkUI::Kit;
using namespace XianTuCatalog;

namespace
{
    FText MajorName(int32 Major)
    {
        static const TCHAR* Names[]={TEXT("练气"),TEXT("筑基"),TEXT("结晶"),TEXT("金丹"),TEXT("元婴"),TEXT("化神"),TEXT("元神"),TEXT("炼虚"),TEXT("大乘"),TEXT("飞升"),TEXT("混元无极")};
        return FText::FromString(Names[FMath::Clamp(Major,0,10)]);
    }

    FText Owned(const UXianTuSubsystem& Svc,const FStage& S,ECategory C)
    {
        return FText::Format(LOCTEXT("OwnedGrades","拥有 下品 {0} · 中品 {1} · 上品 {2} · 极品 {3}"),
            FText::AsNumber(Svc.ItemCount(MaterialKey(S,C,1))),FText::AsNumber(Svc.ItemCount(MaterialKey(S,C,2))),
            FText::AsNumber(Svc.ItemCount(MaterialKey(S,C,3))),FText::AsNumber(Svc.ItemCount(MaterialKey(S,C,4))));
    }

    /** One plan slot: the graded material, or an empty slot inviting a tap. */
    TSharedRef<SWidget> PlanSlot(const FSpecies& Sp,int32 Grade,float Size,FName Id,FSimpleDelegate Click)
    {
        if(Grade<=0)return SNew(SInkItemSlot).SlotSize(Size).Empty(true).TestId(Id).OnClicked(Click);
        return SNew(SOverlay)
            +SOverlay::Slot()[SNew(SInkItemSlot).SlotSize(Size).Icon(Art::ItemIcon(Sp.Icon)).Quality(EQuality(FMath::Clamp(Grade,1,4))).TestId(Id).OnClicked(Click)]
            +SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Bottom).Padding(FMargin(0.f,0.f,0.f,4.f))
            [
                SNew(SInkPaint).DesiredSize(FVector2D(Size*.62f,22.f)).OnPaint_Lambda([Grade](FPainter& P,const FGeometry&)
                {
                    P.Rounded(FVector2f(0.f,0.f),P.Size(),WithAlpha(Colors().InkBlack,.72f),8.f);P.Next();
                    P.Text(GradeName(Grade).ToString(),Font(EFont::Tiny,15.f),P.Size()*.5f,QualityColor(EQuality(FMath::Clamp(Grade,1,4))),FVector2f(.5f,.52f));
                })
            ];
    }

    /** Graded quality bar: the six bands across, the score's band lit and a cursor at the score. */
    void PaintQuality(FPainter& P,int32 Quality,bool bHasCore)
    {
        const auto& K=Colors();const FVector2f S=P.Size();const double Now=Motion::Now();
        const TArray<FBand>& List=Bands();const int32 Band=BandIndex(Quality);const FLinearColor C=List[Band].Color;
        P.Rounded(FVector2f(0.f,0.f),S,WithAlpha(K.InkBlack,.82f),10.f,WithAlpha(K.Gold,.8f),1.5f);
        P.Next();
        const FVector2f Num(92.f,S.Y*.5f);
        if(bHasCore)P.Glow(Num,FVector2f(80.f,60.f),WithAlpha(C,.25f+.1f*Motion::Pulse(Now,2.4f)));
        P.Next();
        P.Text(bHasCore?FString::FromInt(Quality):TEXT("—"),Font(EFont::Title,64.f),Num+FVector2f(0.f,-10.f),bHasCore?C:K.TextLightDim,FVector2f(.5f,.5f),WithAlpha(K.InkBlack,.8f));
        P.Text(bHasCore?List[Band].Name.ToString():TEXT("未放核心"),Font(EFont::Section,22.f),Num+FVector2f(0.f,40.f),bHasCore?C:K.TextLightDim,FVector2f(.5f,.5f));
        const float X0=190.f,X1=S.X-24.f,Y=S.Y*.42f,H=18.f;
        for(int32 I=0;I<List.Num();++I)
        {
            const float A=X0+(X1-X0)*List[I].Min/101.f,B=X0+(X1-X0)*(List[I].Max+1)/101.f;
            const bool bOn=bHasCore&&I==Band;
            P.Rounded(FVector2f(A+1.5f,Y-H*.5f),FVector2f(B-A-3.f,H),WithAlpha(List[I].Color,bOn?1.f:.32f),4.f);
            P.Text(List[I].Name.ToString().Left(1),Font(EFont::Tiny,16.f),FVector2f((A+B)*.5f,Y+H+8.f),bOn?List[I].Color:K.TextLightDim,FVector2f(.5f,.5f));
        }
        P.Next();
        if(bHasCore)
        {
            const float X=X0+(X1-X0)*(Quality+.5f)/101.f;
            P.Glow(FVector2f(X,Y),FVector2f(16.f),WithAlpha(FLinearColor::White,.6f));
            P.Diamond(FVector2f(X,Y),FVector2f(7.f,12.f),K.GoldBright);
        }
        P.Text(FString::Printf(TEXT("品质因子 +%.1f%%（本阶）"),(List[Band].Factor-1.0)*10.0),Font(EFont::Small,18.f),FVector2f(X0,S.Y-18.f),K.TextGold,FVector2f(0.f,.5f));
    }
}

// ------------------------------------------------------------------ 境界突破

void SInkBreakthroughScreen::Construct(const FArguments& InArgs)
{
    InitScreen(InArgs._Root.ToSharedRef(),TEXT("Breakthrough"));
    BuildShell(LOCTEXT("BreakTitle","境界突破"),LOCTEXT("BreakSeal","破"),
        SNew(SVerticalBox)
        +SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f,0.f,0.f,8.f))
        [
            SNew(SBox).HeightOverride(96.f)
            [
                SNew(SInkPaint).OnPaint_Lambda([this](FPainter& P,const FGeometry&)
                {
                    // Two painted plaques, the realm now and the realm ahead, joined by a glowing path in the next realm's hue.
                    const auto& K=Colors();const FVector2f S=P.Size();const UXianTuSubsystem* Q=X();
                    const int32 Realm=Q?Q->AnchorRealm():0;const FStage* Stage=Q?Q->NextStage():nullptr;
                    const FLinearColor Hue=XianTuBreakthroughFx::RealmColor(LiveCombat::MajorRealm(Realm+1));
                    const FVector2f Plaque(FMath::Min(250.f,S.X*.38f),58.f);
                    const FVector2f L(0.f,4.f),R(S.X-Plaque.X,4.f);
                    P.Glow(R+Plaque*.5f,Plaque*FVector2f(.62f,.9f),WithAlpha(Hue,.22f+.14f*Motion::Pulse(Motion::Now(),2.2f)));
                    P.Next();
                    P.Skin(Art::ESkin::PlaqueLacquer,L,Plaque);
                    P.Skin(Art::ESkin::PlaqueGold,R,Plaque);
                    P.Next();P.Next();
                    P.Text(Q?Q->AnchorRealmName().ToString():TEXT("—"),Font(EFont::Title,26.f),L+Plaque*.5f,K.TextGold,FVector2f(.5f,.52f));
                    P.Text(Q?Q->AnchorRealmName(1).ToString():TEXT("—"),Font(EFont::Title,26.f),R+Plaque*.5f,K.TextDark,FVector2f(.5f,.52f));
                    const FVector2f C(S.X*.5f,4.f+Plaque.Y*.5f);
                    P.Glow(C,FVector2f(70.f,30.f),WithAlpha(Hue,.3f+.15f*Motion::Pulse(Motion::Now(),1.8f)));
                    const float Flow=float(FMath::Fmod(Motion::Now()*.8,1.0));
                    for(int32 I=0;I<3;++I)
                    {
                        const float F=FMath::Fmod(I/3.f+Flow,1.f);
                        P.Diamond(C+FVector2f(-36.f+72.f*F,0.f),FVector2f(6.f,9.f),WithAlpha(Lerp(K.GoldBright,Hue,.4f),FMath::Sin(F*PI)));
                    }
                    P.Text(Stage?(Stage->bTribulation?TEXT("大境界 · 须渡天劫"):TEXT("大境界突破")):TEXT("小境界突破"),Font(EFont::Small,19.f),FVector2f(C.X,S.Y-14.f),Stage?Lerp(K.TextDark,K.Cinnabar,Stage->bTribulation?.8f:0.f):K.TextDarkDim,FVector2f(.5f,.5f));
                })
            ]
        ]
        +SVerticalBox::Slot().AutoHeight()[Tabs(TEXT("Break.Tabs"),{LOCTEXT("BTab0","突破"),LOCTEXT("BTab1","淬炼"),LOCTEXT("BTab2","品质")})],
        SNew(SBox).Visibility_Lambda([this]{return CelebrateStart>=0&&Motion::Now()-CelebrateStart<3.4?EVisibility::HitTestInvisible:EVisibility::Collapsed;})
        [SNew(SInkPaint).OnPaint_Lambda([this](FPainter& P,const FGeometry&){PaintCelebration(P);})]);
}

int32 SInkBreakthroughScreen::Revision() const{return M().XianTuRev;}

void SInkBreakthroughScreen::Commit()
{
    UXianTuSubsystem* Svc=X();const auto R=GetRoot();if(!Svc||!R)return;
    const FStage* Stage=Svc->NextStage();
    if(Stage&&Stage->bTribulation){R->OpenPage(TEXT("Tribulation"));return;}
    const int32 From=Svc->AnchorRealm();const int32 Quality=Stage?Svc->PlanQuality():-1;
    if(!Exec(TEXT("BreakNow")))return;
    CelebrateStart=Motion::IsReduced()?-1:Motion::Now();CelebrateTo=From+1;CelebrateQuality=Quality;bCelebrateMajor=Stage!=nullptr;
}

void SInkBreakthroughScreen::PaintCelebration(FPainter& P) const
{
    if(CelebrateStart<0)return;
    const auto& K=Colors();const FVector2f S=P.Size();const float T=float(Motion::Now()-CelebrateStart);
    if(T>3.4f)return;
    const float A=FMath::Clamp(T/.25f,0.f,1.f)*FMath::Clamp((3.4f-T)/.55f,0.f,1.f);
    const int32 Major=LiveCombat::MajorRealm(CelebrateTo);const FLinearColor Hue=XianTuBreakthroughFx::RealmColor(Major);
    const FVector2f C(S.X*.5f,S.Y*.4f);
    P.Rounded(FVector2f(0.f,0.f),S,WithAlpha(K.InkBlack,.72f*A),12.f);
    P.Next();
    P.Rays(C,40.f,S.Y*.7f,bCelebrateMajor?20:12,T*.3f,.028f,WithAlpha(Lerp(Hue,FLinearColor::White,.35f),(bCelebrateMajor?.22f:.15f)*A));
    P.Glow(C,FVector2f(S.X*.42f,S.X*.42f),WithAlpha(Hue,.35f*A));
    if(T>.5f&&T<1.6f){const float F=(T-.5f)/1.1f;P.Ring(C,80.f+F*S.X*.6f,14.f*(1.f-F),WithAlpha(Hue,0.f),WithAlpha(FLinearColor::White,.75f*(1.f-F)*A));}
    P.Next();
    // The 破 seal stamps down from above, then the new realm rises under it.
    const float Stamp=Ease(EEase::OutBack,FMath::Clamp((T-.1f)/.45f,0.f,1.f));
    const float Size=150.f*(2.1f-1.1f*Stamp);
    const FVector2f Seal(Size,Size);
    P.Halo(C-Seal*.5f,Seal,14.f,18.f,WithAlpha(K.InkBlack,.5f*A*Stamp));
    P.Rounded(C-Seal*.5f,Seal,WithAlpha(K.Cinnabar,.95f*A*FMath::Min(1.f,Stamp*1.5f)),14.f,WithAlpha(K.GoldBright,A),3.f);
    P.Next();
    P.Text(TEXT("破"),Font(EFont::Title,Size*.72f),C,WithAlpha(Hex(TEXT("FFF2D6")),A*FMath::Min(1.f,Stamp*1.5f)),FVector2f(.5f,.53f));
    P.Next();
    const float Rise=Ease(EEase::OutCubic,FMath::Clamp((T-.7f)/.5f,0.f,1.f));
    if(Rise>0.f)
    {
        // The new realm on a gilded plaque rising under the seal.
        const FVector2f Ext(FMath::Min(S.X*.7f,460.f),92.f);
        const FVector2f Pos(S.X*.5f-Ext.X*.5f,S.Y*.6f+30.f*(1.f-Rise));
        const UXianTuSubsystem* Q=X();
        P.Glow(Pos+Ext*.5f,Ext*FVector2f(.7f,1.1f),WithAlpha(Hue,.35f*A*Rise));
        P.Next();
        P.Skin(Art::ESkin::PlaqueGold,Pos,Ext,WithAlpha(FLinearColor::White,A*Rise));
        P.Next();P.Next();
        P.Text(Q?Q->AnchorRealmName().ToString():TEXT(""),Font(EFont::Title,44.f),Pos+Ext*.5f,WithAlpha(K.TextDark,A*Rise),FVector2f(.5f,.52f));
        P.Text(TEXT("晋升"),Font(EFont::Section,24.f),FVector2f(S.X*.5f,Pos.Y-22.f),WithAlpha(K.GoldBright,A*Rise),FVector2f(.5f,.5f),WithAlpha(K.InkBlack,.8f*A));
        if(bCelebrateMajor&&CelebrateQuality>=0)
        {
            const FBand& B=Bands()[BandIndex(CelebrateQuality)];
            P.Text(FString::Printf(TEXT("突破品质 · %s Q%d"),*B.Name.ToString(),CelebrateQuality),Font(EFont::Section,28.f),FVector2f(S.X*.5f,Pos.Y+Ext.Y+34.f),WithAlpha(B.Color,A*Rise),FVector2f(.5f,.5f),WithAlpha(K.InkBlack,.85f*A));
        }
    }
    P.Next();
    P.Motes(FVector2f(0.f,S.Y*.1f),FVector2f(S.X,S.Y*.8f),bCelebrateMajor?36:18,Motion::Now(),WithAlpha(Lerp(Hue,FLinearColor::White,.4f),.9f*A),91,1.6f,6.f);
}

void SInkBreakthroughScreen::Rebuild()
{
    const FXianTuSave* St=S();UXianTuSubsystem* Svc=X();
    if(!St||!Svc){Body->SetContent(Paper(Hint(LOCTEXT("BreakOffline","进入游戏后可突破境界。"))));Footer->SetContent(SNullWidget::NullWidget);return;}
    const TSharedRef<SVerticalBox> List=SNew(SVerticalBox);int32 Order=0;
    auto Add=[&](TSharedRef<SWidget> Widget){List->AddSlot().AutoHeight().Padding(FMargin(0.f,5.f))[Appear(Widget,Order++)];};
    const FStage* Stage=Svc->NextStage();const int32 Realm=Svc->AnchorRealm();
    if(Tab==1)
    {
        Footer->SetContent(Hint(LOCTEXT("RefineFoot","淬炼为确定性制作：两份同阶同品材料加灵石合成一份高一品，100% 成功；已放入突破方案的材料不参与。")));
        const FStage* Up=Svc->UpcomingStage();
        if(!Up){Add(Paper(Hint(LOCTEXT("NoUp","已无后续大境界材料可淬炼。"))));Body->SetContent(List);return;}
        Add(Paper(Rows({Head(FText::Format(LOCTEXT("RefineHead","{0} 材料淬炼 · {1}·{2}"),FText::FromName(Up->Id),Up->Region,Up->Subarea),26.f),
            Pair(LOCTEXT("Stones","灵石"),TAttribute<FText>::CreateLambda([this]{const UXianTuSubsystem* Q=X();return Q?FText::AsNumber(Q->Stones()):FText();}))}),FMargin(24.f,14.f,24.f,16.f)));
        for(int32 Cat=0;Cat<4;++Cat)
        {
            const FSpecies& Sp=Up->Species[Cat];
            TArray<TSharedRef<SWidget>> RowsOut;
            RowsOut.Add(SNew(SHorizontalBox)
                +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(0.f,0.f,10.f,0.f))[Plaque(CategoryName(ECategory(Cat)),Cat==0?3:Cat==1?0:1,110.f,34.f)]
                +SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)[Head(Sp.Name,25.f)]);
            for(int32 G=2;G<=4;++G)
            {
                const int32 Free=Svc->FreeCount(*Up,ECategory(Cat),G-1),Cost=RefineCost(*Up,G);
                const bool bCan=Free>=2&&Svc->Stones()>=Cost;
                RowsOut.Add(SNew(SHorizontalBox)
                    +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Cell(MaterialKey(*Up,ECategory(Cat),G-1),FMath::Max(0,Free),58.f)]
                    +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(8.f,0.f))[MakeText(LOCTEXT("TwoTo","×2 →"),EFont::Data,Colors().TextDark,22.f)]
                    +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Cell(MaterialKey(*Up,ECategory(Cat),G),Svc->ItemCount(MaterialKey(*Up,ECategory(Cat),G)),58.f)]
                    +SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center).Padding(FMargin(12.f,0.f,8.f,0.f))
                    [Rows({Line(FText::Format(LOCTEXT("RefineTo","{0}{1}"),GradeName(G),Sp.Name),21.f),Hint(FText::Format(LOCTEXT("RefineCost","灵石 {0} · 可用{1} {2}"),N(Cost),GradeName(G-1),N(FMath::Max(0,Free))),18.f)},1.f)]
                    +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                    [Btn(LOCTEXT("RefineBtn","淬炼"),EButtonSkin::Jade,FName(*FString::Printf(TEXT("Break.Refine.%d.%d"),Cat,G)),FSimpleDelegate::CreateLambda([this,Cat,G]{Exec(TEXT("BreakRefine"),{FString::FromInt(Cat),FString::FromInt(G)});}),bCan,50.f,110.f,21.f)]);
            }
            Add(Paper(Rows(RowsOut,6.f),FMargin(22.f,14.f,22.f,16.f)));
        }
        Body->SetContent(List);return;
    }
    if(Tab==2)
    {
        Footer->SetContent(Hint(LOCTEXT("QualityFoot","每个大境界取最高一次突破品质快照：因子 = 1 + min(10%, Σ(档位系数−1)/10)，作用于主播攻击、生命、防御；不影响修为速度与渡劫成功率。")));
        Add(Paper(Rows({
            Head(LOCTEXT("FactorHead","突破品质总览"),28.f),
            Pair(LOCTEXT("Factor","攻击 · 生命 · 防御"),TAttribute<FText>::CreateLambda([this]{const UXianTuSubsystem* Q=X();return Q?FText::FromString(FString::Printf(TEXT("×%.3f"),Q->QualityFactor())):FText();}),24.f)
        }),FMargin(26.f,16.f,26.f,18.f),Art::ESkin::CardOrnate));
        for(const FStage& Sx:Stages())
        {
            const FXianTuBreakRecord* Rec=Svc->BreakRecord(Sx.Id);
            const FBand* B=Rec?&Bands()[FMath::Clamp(Rec->Band,0,5)]:nullptr;
            Add(Paper(SNew(SHorizontalBox)
                +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(0.f,0.f,12.f,0.f))[Plaque(FText::FromName(Sx.Id),Rec?3:1,76.f,34.f)]
                +SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
                [Rows({Line(FText::Format(LOCTEXT("StageRow","{0} → {1}{2}"),MajorName(LiveCombat::MajorRealm(Sx.From)),MajorName(LiveCombat::MajorRealm(Sx.To)),Sx.bTribulation?LOCTEXT("TribMark"," · 天劫"):FText::GetEmpty()),22.f),
                    Hint(Rec?FText::Format(LOCTEXT("RecRow","最高 Q{0} · 回影标记 {1} · 收据 {2}"),N(Rec->Quality),N(Rec->Markers),FText::FromString(Rec->Receipt)):FText::Format(LOCTEXT("NoRec","未突破 · {0}·{1}"),Sx.Region,Sx.Subarea),18.f)},1.f)]
                +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                [SNew(SInkPaint).DesiredSize(FVector2D(96.f,40.f)).OnPaint_Lambda([B](FPainter& P,const FGeometry&)
                {
                    const auto& K=Colors();
                    P.Rounded(FVector2f(0.f,0.f),P.Size(),B?WithAlpha(B->Color,.9f):WithAlpha(K.PaperShade,.9f),8.f,WithAlpha(K.GoldDeep,.8f),1.f);P.Next();
                    P.Text(B?B->Name.ToString():TEXT("—"),Font(EFont::Section,20.f),P.Size()*.5f,B?K.InkBlack:K.TextDarkDim,FVector2f(.5f,.52f));
                })],FMargin(22.f,10.f,22.f,10.f)));
        }
        Body->SetContent(List);return;
    }
    // 突破
    Add(Paper(Rows({
        Head(Stage?FText::Format(LOCTEXT("MajorHead","大境界突破 · {0} · {1} → {2}"),FText::FromName(Stage->Id),Svc->AnchorRealmName(),Svc->AnchorRealmName(1))
            :FText::Format(LOCTEXT("MinorHead","小境界突破 · {0} → {1}"),Svc->AnchorRealmName(),Svc->AnchorRealmName(1)),27.f),
        Meter(TAttribute<float>::CreateLambda([this]{const UXianTuSubsystem* Q=X();return Q&&Q->AnchorQiRequired()>0?float(FMath::Min(1.0,Q->AnchorQi()/Q->AnchorQiRequired())):0.f;}),
            TAttribute<FText>::CreateLambda([this]{const UXianTuSubsystem* Q=X();return Q?FText::Format(LOCTEXT("QiMeter","修为 {0} / {1}"),FormatAmount(Q->AnchorQi()),FormatAmount(Q->AnchorQiRequired())):FText();}),EBar::Jade,600.f,30.f),
        Stage?Hint(FText::Format(LOCTEXT("StageWhere","{0}·{1}{2} · 大境界只消耗本阶核心与所放辅材，不再扣破境石"),Stage->Region,Stage->Subarea,Stage->bTribulation?LOCTEXT("TribWhere"," · 须渡三重天劫"):FText::GetEmpty()))
            :StaticCastSharedRef<SWidget>(SNew(SHorizontalBox)
                +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(0.f,0.f,12.f,0.f))[Cell(TEXT("MAT_BREAK"),Svc->ItemCount(TEXT("MAT_BREAK")),62.f)]
                +SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)[Rows({Line(LOCTEXT("MinorNeed","核心材料：破境石"),23.f),Hint(LOCTEXT("MinorRule","小境界突破：修为圆满并消耗本阶所需破境石；世界历练与灵玉商城均可获得。"))},2.f)])
    }),FMargin(26.f,18.f,26.f,20.f),Art::ESkin::CardOrnate));
    if(Stage)
    {
        const FXianTuBreakPlan& Plan=St->Plan;const bool bMine=Plan.Stage==Stage->Id;
        const FSpecies& Core=Stage->Species[0];
        // Score and one-tap actions first, so they stay in view while the slots below are filled.
        const int32 Quality=Svc->PlanQuality();const bool bCore=bMine&&Plan.Core>0;
        int32 PartCore=0,PartGuard=0,PartEssence=0,PartTreasure=0;
        if(bCore)
        {
            auto Sum=[](ECategory C,const TArray<int32>& Arr){int32 W=0;for(int32 G:Arr)W+=GradeScore(C,G);return W;};
            PartCore=GradeScore(ECategory::Core,Plan.Core);
            PartGuard=FMath::Min(Sum(ECategory::Guard,Plan.Guard)/FMath::Max(1,Stage->Caps[1]),CategoryCap(ECategory::Guard));
            PartEssence=FMath::Min(Sum(ECategory::Essence,Plan.Essence)/FMath::Max(1,Stage->Caps[2]),CategoryCap(ECategory::Essence));
            PartTreasure=FMath::Min(Sum(ECategory::Treasure,Plan.Treasure),CategoryCap(ECategory::Treasure));
        }
        Add(Paper(Rows({
            SNew(SBox).HeightOverride(136.f)[SNew(SInkPaint).OnPaint_Lambda([Quality,bCore](FPainter& P,const FGeometry&){PaintQuality(P,Quality,bCore);})],
            Hint(FText::Format(LOCTEXT("QBreak","Q = 核心 {0} + 护道 {1} + 精华 {2} + 奇珍 {3}（护道、精华按投入总权重 ÷ 本阶上限取整）"),N(PartCore),N(PartGuard),N(PartEssence),N(PartTreasure)),18.f),
            SNew(SHorizontalBox)
            +SHorizontalBox::Slot().AutoWidth().Padding(FMargin(0.f,0.f,10.f,0.f))[Btn(LOCTEXT("Auto","一键放入"),EButtonSkin::Jade,TEXT("Break.Auto"),FSimpleDelegate::CreateLambda([this]{Exec(TEXT("BreakAuto"));}),true,54.f,170.f,22.f)]
            +SHorizontalBox::Slot().AutoWidth().Padding(FMargin(0.f,0.f,10.f,0.f))[Btn(LOCTEXT("Clear","清空"),EButtonSkin::Paper,TEXT("Break.Clear"),FSimpleDelegate::CreateLambda([this]{Exec(TEXT("BreakClear"));}),bMine,54.f,120.f,22.f)]
            +SHorizontalBox::Slot().AutoWidth()[Btn(LOCTEXT("Sources","材料来源"),EButtonSkin::Paper,TEXT("Break.Sources"),FSimpleDelegate::CreateLambda([this]{if(const auto R=GetRoot())R->OpenPage(TEXT("ItemSources"));}),true,54.f,150.f,22.f)]
        },8.f),FMargin(22.f,16.f,22.f,18.f),Art::ESkin::CardOrnate));
        Add(Paper(SNew(SHorizontalBox)
            +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(0.f,0.f,16.f,0.f))
            [PlanSlot(Core,bMine?Plan.Core:0,104.f,TEXT("Break.Core"),FSimpleDelegate::CreateLambda([this]{Exec(TEXT("BreakCore"));}))]
            +SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
            [Rows({
                SNew(SHorizontalBox)
                +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(0.f,0.f,10.f,0.f))[Plaque(CategoryName(ECategory::Core),3,104.f,34.f)]
                +SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)[Head(Core.Name,26.f)],
                Hint(Owned(*Svc,*Stage,ECategory::Core),18.f),
                Hint(LOCTEXT("CoreTap","必需 · 点选切换品阶（极品→下品→取下），品阶决定核心评分 10/18/26/35"),18.f)},3.f)],
            FMargin(22.f,14.f,22.f,16.f)));
        for(int32 Cat=1;Cat<4;++Cat)
        {
            const FSpecies& Sp=Stage->Species[Cat];const TArray<int32>* Slots=bMine?(Cat==1?&Plan.Guard:Cat==2?&Plan.Essence:&Plan.Treasure):nullptr;
            int32 Filled=0;if(Slots)for(int32 G:*Slots)Filled+=G>0?1:0;
            const TSharedRef<SWrapBox> Box=SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(8.f,8.f));
            for(int32 I=0;I<Stage->Caps[Cat];++I)
            {
                const int32 Grade=Slots&&Slots->IsValidIndex(I)?(*Slots)[I]:0;
                Box->AddSlot()[PlanSlot(Sp,Grade,78.f,FName(*FString::Printf(TEXT("Break.Slot.%d.%d"),Cat,I)),FSimpleDelegate::CreateLambda([this,Cat,I]{Exec(TEXT("BreakSlot"),{FString::FromInt(Cat),FString::FromInt(I)});}))];
            }
            Add(Paper(Rows({
                SNew(SHorizontalBox)
                +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(0.f,0.f,10.f,0.f))[Plaque(CategoryName(ECategory(Cat)),Cat==1?0:Cat==2?1:2,110.f,34.f)]
                +SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)[Head(FText::Format(LOCTEXT("AuxHead","{0} · {1}/{2}"),Sp.Name,N(Filled),N(Stage->Caps[Cat])),24.f)],
                Box,
                Hint(Owned(*Svc,*Stage,ECategory(Cat)),18.f),
                Hint(CategoryRole(ECategory(Cat)),18.f)},5.f),FMargin(22.f,14.f,22.f,16.f)));
        }
    }
    const bool bTrib=Stage&&Stage->bTribulation;
    Footer->SetContent(SNew(SVerticalBox)
        +SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(FMargin(0.f,0.f,0.f,6.f))
        [MakeText(TAttribute<FText>::CreateLambda([this,bTrib]
        {
            const UXianTuSubsystem* Q=X();if(!Q)return FText();FText Why;
            if(bTrib)return Q->CanStartTribulation(&Why)?LOCTEXT("TribReady","材料已备，可前往天劫"):Why;
            return Q->CanBreakthrough(&Why)?LOCTEXT("Ready","条件已满足，可以突破"):Why;
        }),EFont::Small,Colors().TextDarkDim,20.f)]
        +SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
        [Btn(bTrib?LOCTEXT("GoTrib","前往渡劫"):LOCTEXT("BreakBtn","突 破"),bTrib?EButtonSkin::Cinnabar:EButtonSkin::Gold,TEXT("Break.Commit"),FSimpleDelegate::CreateLambda([this]{Commit();}),
            TAttribute<bool>::CreateLambda([this,bTrib]{const UXianTuSubsystem* Q=X();return Q&&(bTrib?Q->CanStartTribulation(nullptr):Q->CanBreakthrough(nullptr));}),74.f,300.f,30.f)]);
    (void)Realm;
    Body->SetContent(List);
}

// ------------------------------------------------------------------ 物品来源

void SInkItemSourcesScreen::Construct(const FArguments& InArgs)
{
    InitScreen(InArgs._Root.ToSharedRef(),TEXT("ItemSources"));
    if(const UXianTuSubsystem* Svc=M().XianTu.Get())if(const FStage* Up=Svc->UpcomingStage())Pick0=Up->Index;
    BuildShell(LOCTEXT("SourcesTitle","物品来源"),LOCTEXT("SourcesSeal","源"),
        Tabs(TEXT("Sources.Tabs"),{LOCTEXT("STab0","突破材料"),LOCTEXT("STab1","道具"),LOCTEXT("STab2","货币")}));
}

int32 SInkItemSourcesScreen::Revision() const{return M().XianTuRev;}

void SInkItemSourcesScreen::Rebuild()
{
    UXianTuSubsystem* Svc=X();
    Footer->SetContent(Hint(LOCTEXT("SourcesFoot","来源按设计表（items-v1.4）列出；区域副本与确定性炼制台为后续接入，当前可由世界历练机缘、商城与淬炼获得。")));
    if(!Svc){Body->SetContent(Paper(Hint(LOCTEXT("SourcesOffline","进入游戏后查看物品来源。"))));return;}
    const TSharedRef<SVerticalBox> List=SNew(SVerticalBox);int32 Order=0;
    auto Add=[&](TSharedRef<SWidget> Widget){List->AddSlot().AutoHeight().Padding(FMargin(0.f,5.f))[Appear(Widget,Order++)];};
    auto Go=[this](const TCHAR* Label,FName Target,FName Id){return Btn(FText::FromString(Label),EButtonSkin::Paper,Id,FSimpleDelegate::CreateLambda([this,Target]{if(const auto R=GetRoot())R->OpenPage(Target);}),true,48.f,128.f,20.f);};
    if(Tab==0)
    {
        Pick0=FMath::Clamp(Pick0,0,Stages().Num()-1);
        const TSharedRef<SWrapBox> Picker=SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(6.f,6.f));
        for(const FStage& Sx:Stages())
        {
            const int32 Index=Sx.Index;
            Picker->AddSlot()[SNew(SInkClickable).TestId(FName(*(TEXT("Sources.Stage.")+Sx.Id.ToString()))).Radius(8.f).OnClicked_Lambda([this,Index]{Pick0=Index;Refresh();})
            [Plaque(FText::FromName(Sx.Id),Index==Pick0?3:Svc->AnchorMajor()>=Sx.MinMajor?0:1,82.f,36.f)]];
        }
        Add(Paper(Picker,FMargin(22.f,14.f,22.f,16.f)));
        const FStage& St=Stages()[Pick0];
        static const int32 Cycles[]={1,1,2,3};
        for(int32 Cat=0;Cat<4;++Cat)
        {
            const FSpecies& Sp=St.Species[Cat];
            Add(Paper(Rows({
                SNew(SHorizontalBox)
                +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(0.f,0.f,14.f,0.f))[Cell(MaterialKey(St,ECategory(Cat),1),Svc->ItemCount(MaterialKey(St,ECategory(Cat),1)),72.f)]
                +SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
                [Rows({Head(Sp.Name,26.f),Hint(FText::Format(LOCTEXT("SpLine","{0} · {1} · {2}"),CategoryName(ECategory(Cat)),Sp.Theme,FText::FromName(St.Id)),18.f),Hint(Owned(*Svc,St,ECategory(Cat)),18.f)},2.f)],
                Line(FText::Format(LOCTEXT("SrcBase","区域副本：{0}·{1}，每 {2} 轮基础产出下品（后续开放）"),St.Region,St.Subarea,N(Cycles[Cat])),20.f),
                Line(LOCTEXT("SrcRefine","确定性淬炼：两份同品 + 灵石 → 一份高一品，100% 成功"),20.f),
                Line(Cat==0?LOCTEXT("SrcShopCore","灵玉商城：下品每日限购 1"):LOCTEXT("SrcShopAux","灵玉商城：下品每日限购；世界历练机缘随幸运提高"),20.f),
                SNew(SHorizontalBox)
                +SHorizontalBox::Slot().AutoWidth().Padding(FMargin(0.f,0.f,8.f,0.f))[Go(TEXT("去购买"),TEXT("JadeShop"),FName(*FString::Printf(TEXT("Sources.Buy.%d"),Cat)))]
                +SHorizontalBox::Slot().AutoWidth().Padding(FMargin(0.f,0.f,8.f,0.f))[Go(TEXT("去淬炼"),TEXT("Breakthrough"),FName(*FString::Printf(TEXT("Sources.Refine.%d"),Cat)))]
                +SHorizontalBox::Slot().AutoWidth()[Go(TEXT("去历练"),TEXT("World"),FName(*FString::Printf(TEXT("Sources.World.%d"),Cat)))]
            },4.f),FMargin(22.f,14.f,22.f,16.f)));
        }
    }
    else if(Tab==1)
    {
        for(const FProp& Pr:Props())
        {
            const bool bWuling=Sku(FName(*(TEXT("WULING_")+Pr.Id.ToString())))!=nullptr;
            Add(Paper(SNew(SHorizontalBox)
                +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(0.f,0.f,14.f,0.f))[Cell(Pr.Id,Svc->ItemCount(Pr.Id),68.f)]
                +SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
                [Rows({Head(FText::Format(LOCTEXT("PropHead","{0} · {1}起"),Pr.Name,MajorName(Pr.MinMajor)),24.f),Hint(Pr.Effect,18.f),
                    Hint(bWuling?LOCTEXT("PropSrc2","来源：灵玉商城 · 武灵商城 · 炼制台（后续开放）"):LOCTEXT("PropSrc1","来源：灵玉商城 · 炼制台（后续开放）"),18.f)},2.f)]
                +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Go(TEXT("去购买"),TEXT("JadeShop"),FName(*(TEXT("Sources.Prop.")+Pr.Id.ToString())))],FMargin(22.f,12.f,22.f,14.f)));
        }
    }
    else
    {
        auto Row=[&](FName Key,const FText& Name,const FText& Source,const FText& Use)
        {
            Add(Paper(SNew(SHorizontalBox)
                +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(0.f,0.f,14.f,0.f))[Cell(Key,Svc->ItemCount(Key),68.f)]
                +SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
                [Rows({Head(FText::Format(LOCTEXT("CurHead","{0} · {1}"),Name,FormatAmount(double(Svc->ItemCount(Key)))),25.f),Hint(FText::Format(LOCTEXT("CurSrc","来源：{0}"),Source),18.f),Hint(FText::Format(LOCTEXT("CurUse","用途：{0}"),Use),18.f)},2.f)],
                FMargin(22.f,12.f,22.f,14.f)));
        };
        Row(TEXT("CUR_STONE"),LOCTEXT("Stone","灵石"),LOCTEXT("StoneSrc","世界历练、灵石矿开采、坊市回收"),LOCTEXT("StoneUse","研习、淬炼、坊市寄售"));
        for(const XianTuCatalog::FCurrency& C:Currencies())Row(C.Key,C.Name,C.Source,C.Use);
    }
    Body->SetContent(List);
}

// ------------------------------------------------------------------ 武灵建设

void SInkGuildBuildScreen::Construct(const FArguments& InArgs)
{
    InitScreen(InArgs._Root.ToSharedRef(),TEXT("GuildConstruction"));
    const TWeakObjectPtr<UXianTuSubsystem> Ledger=M().XianTu;
    BuildShell(LOCTEXT("BuildTitle","武灵建设"),LOCTEXT("BuildSeal","建"),
        SNew(SVerticalBox)
        +SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(FMargin(0.f,0.f,0.f,4.f))
        [MakeText(TAttribute<FText>::CreateLambda([this]{const FXianTuSave* St=S();return St?FText::Format(LOCTEXT("Supply","公共资材 {0} · 公库建设材料"),FText::AsNumber(St->Guild.Materials)):FText();}),EFont::Section,Colors().TextDark,23.f)]
        +SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)[WalletBar(Ledger,{TEXT("GUILD_MAT_01"),TEXT("GUILD_MAT_02"),TEXT("GUILD_MAT_03"),TEXT("GUILD_MAT_04")},46.f)]);
}

int32 SInkGuildBuildScreen::Revision() const{return M().XianTuRev;}

void SInkGuildBuildScreen::Rebuild()
{
    const FXianTuSave* St=S();UXianTuSubsystem* Svc=X();
    Footer->SetContent(SNew(SHorizontalBox)
        +SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center).Padding(FMargin(0.f,0.f,12.f,0.f))[Hint(LOCTEXT("BuildFoot","建设只解锁公开外观档位，不提供数值加成；仅主播按公示用途支出公共资材与公库材料。"),18.f)]
        +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Btn(LOCTEXT("ToWulingShop","武灵商城"),EButtonSkin::Jade,TEXT("Build.Shop"),FSimpleDelegate::CreateLambda([this]{if(const auto R=GetRoot())R->OpenPage(TEXT("WulingShop"));}),true,56.f,170.f,22.f)]);
    if(!St||!Svc){Body->SetContent(Paper(Hint(LOCTEXT("BuildOffline","进入游戏后可建设武灵。"))));return;}
    const TSharedRef<SVerticalBox> List=SNew(SVerticalBox);int32 Order=0;
    auto Add=[&](TSharedRef<SWidget> Widget){List->AddSlot().AutoHeight().Padding(FMargin(0.f,5.f))[Appear(Widget,Order++)];};
    const FXianTuGuild& G=St->Guild;
    if(!G.bCreated)
    {
        Add(Paper(Rows({Head(LOCTEXT("NoGuild","尚未创建武灵"),28.f),Line(LOCTEXT("NoGuildBody","武灵建设使用公共资材与公库建设材料；先在武灵山门免费创建武灵。"),22.f),
            SNew(SBox).HAlign(HAlign_Right)[Btn(LOCTEXT("ToGuild","前往武灵山门"),EButtonSkin::Gold,TEXT("Build.ToGuild"),FSimpleDelegate::CreateLambda([this]{if(const auto R=GetRoot())R->OpenPage(TEXT("Wuling"));}),true,58.f,220.f)]}),
            FMargin(26.f,18.f,26.f,20.f),Art::ESkin::CardOrnate));
        Body->SetContent(List);return;
    }
    for(const FGuildBuilding& B:GuildBuildings())
    {
        const FName Id=B.Id;const int32 Level=G.BuildLevels.FindRef(Id);
        TSharedRef<SWidget> Action=StaticCastSharedRef<SWidget>(Plaque(LOCTEXT("MaxLevel","已满档"),3,110.f,40.f));
        FText Cost=LOCTEXT("MaxCost","三档外观均已建成");
        if(Level<3)
        {
            const FBuildLevel& L=B.Levels[Level];
            const int32 HaveA=G.Vault.FindRef(L.MatA),HaveB=G.Vault.FindRef(L.MatB);
            const bool bCan=G.Materials>=L.Supply&&HaveA>=L.MatCount&&HaveB>=L.MatCount;
            Cost=FText::Format(LOCTEXT("LevelCost","第 {0} 档：资材 {1}/{2} · {3} {4}/{5} · {6} {7}/{8}"),N(Level+1),N(G.Materials),N(L.Supply),
                XianTuRules::ItemName(L.MatA),N(HaveA),N(L.MatCount),XianTuRules::ItemName(L.MatB),N(HaveB),N(L.MatCount));
            Action=Btn(FText::Format(LOCTEXT("BuildUp","建至{0}档"),N(Level+1)),EButtonSkin::Gold,FName(*(TEXT("Build.Up.")+Id.ToString())),FSimpleDelegate::CreateLambda([this,Id]{Exec(TEXT("GuildBuildUp"),{Id.ToString()});}),bCan,56.f,150.f,22.f);
        }
        Add(Paper(Rows({
            SNew(SOverlay)
            +SOverlay::Slot()[Scene(B.Scene,108.f,Level==0)]
            +SOverlay::Slot().VAlign(VAlign_Bottom).Padding(FMargin(16.f,0.f,16.f,10.f))
            [
                SNew(SHorizontalBox)
                +SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)[MakeText(B.Name,EFont::Section,Colors().TextLight,28.f,1.f)]
                +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                [SNew(SInkPaint).DesiredSize(FVector2D(96.f,28.f)).OnPaint_Lambda([Level](FPainter& P,const FGeometry&)
                {
                    for(int32 I=0;I<3;++I)
                    {
                        const FVector2f C(16.f+I*32.f,14.f);
                        if(I<Level)P.Glow(C,FVector2f(16.f),WithAlpha(Colors().GoldBright,.5f));
                        P.Diamond(C,FVector2f(9.f,12.f),I<Level?Colors().GoldBright:WithAlpha(Colors().TextLightDim,.6f));
                    }
                })]
            ],
            Hint(B.Theme,19.f),
            SNew(SHorizontalBox)
            +SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center).Padding(FMargin(0.f,0.f,10.f,0.f))[Line(Cost,19.f)]
            +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Action]
        },6.f),FMargin(18.f,14.f,18.f,16.f)));
    }
    Body->SetContent(List);
}

// ------------------------------------------------------------------ 装备法宝

void SInkGearScreen::Construct(const FArguments& InArgs)
{
    InitScreen(InArgs._Root.ToSharedRef(),TEXT("EquipmentFabao"));
    BuildShell(LOCTEXT("GearTitle","装备法宝"),LOCTEXT("GearSeal","器"),
        Tabs(TEXT("Gear.Tabs"),{GearSlotName(0),GearSlotName(1),GearSlotName(2),GearSlotName(3),GearSlotName(4)}));
}

int32 SInkGearScreen::Revision() const{return M().XianTuRev;}

void SInkGearScreen::Rebuild()
{
    const FXianTuSave* St=S();UXianTuSubsystem* Svc=X();
    Footer->SetContent(Hint(LOCTEXT("GearFoot","主播四个非武器槽与一个法宝槽；属性为加法、无随机词条与强化。当前为本地试穿，装备制作接入后按表生效。"),18.f));
    if(!St||!Svc){Body->SetContent(Paper(Hint(LOCTEXT("GearOffline","进入游戏后查看装备法宝。"))));return;}
    const TSharedRef<SVerticalBox> List=SNew(SVerticalBox);int32 Order=0;
    auto Add=[&](TSharedRef<SWidget> Widget){List->AddSlot().AutoHeight().Padding(FMargin(0.f,5.f))[Appear(Widget,Order++)];};
    TArray<const FGear*> Worn;Worn.SetNumZeroed(GearSlots);
    float Stat[4]={0,0,0,0};
    for(int32 I=0;I<GearSlots;++I)if(const FName* Id=St->Gear.Find(FName(*FString::FromInt(I))))if(const FGear* G=GearItem(*Id)){Worn[I]=G;if(I<4)Stat[I]+=G->Value;}
    // Paper doll: four slots round the portrait, the 法宝 below.
    auto DollSlot=[&](int32 I)->TSharedRef<SWidget>
    {
        const FGear* G=Worn[I];const int32 Index=I;
        return SNew(SVerticalBox)
            +SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
            [G?StaticCastSharedRef<SWidget>(SNew(SInkItemSlot).SlotSize(86.f).Icon(Art::ItemIcon(G->Icon)).Quality(EQuality(FMath::Clamp(G->Tier,1,4))).TestId(FName(*FString::Printf(TEXT("Gear.Doll.%d"),I))).OnClicked_Lambda([this,Index]{Tab=Index;Refresh();}))
                :StaticCastSharedRef<SWidget>(SNew(SInkItemSlot).SlotSize(86.f).Empty(true).TestId(FName(*FString::Printf(TEXT("Gear.Doll.%d"),I))).OnClicked_Lambda([this,Index]{Tab=Index;Refresh();}))]
            +SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(FMargin(0.f,4.f,0.f,0.f))[MakeText(G?G->Name:GearSlotName(I),EFont::Small,G?Colors().TextDark:Colors().TextDarkDim,18.f)];
    };
    Add(Paper(SNew(SHorizontalBox)
        +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Rows({DollSlot(0),DollSlot(2)},14.f)]
        +SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center).HAlign(HAlign_Center)
        [
            SNew(SVerticalBox)
            +SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
            [SNew(SInkPaint).DesiredSize(FVector2D(190.f,190.f)).OnPaint_Lambda([](FPainter& P,const FGeometry&)
            {
                const auto& K=Colors();const FVector2f S=P.Size();
                P.Glow(S*.5f,S*.55f,WithAlpha(K.GoldBright,.22f+.08f*Motion::Pulse(Motion::Now(),3.f)));P.Next();
                P.BrushFit(Art::Image(Art::Paths::AvatarPortrait),FVector2f(14.f,14.f),S-FVector2f(28.f,28.f));P.Next();
                P.Ring(S*.5f,S.X*.46f,3.f,WithAlpha(K.GoldBright,.9f),WithAlpha(K.Gold,.6f));
            })]
            +SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(FMargin(0.f,8.f,0.f,0.f))[DollSlot(4)]
        ]
        +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Rows({DollSlot(1),DollSlot(3)},14.f)],
        FMargin(22.f,16.f,22.f,16.f),Art::ESkin::CardOrnate));
    Add(Paper(Rows({
        Head(LOCTEXT("GearStats","试穿加成（加法）"),24.f),
        Line(FText::Format(LOCTEXT("GearStatLine","生命 +{0}% · 防御 +{1}% · 移速 +{2}% · 攻击 +{3}%"),N(FMath::RoundToInt(Stat[0]*100)),N(FMath::RoundToInt(Stat[1]*100)),N(FMath::RoundToInt(Stat[2]*100)),N(FMath::RoundToInt(Stat[3]*100))),22.f),
        Hint(Worn[4]?FText::Format(LOCTEXT("FabaoLine","法宝 · {0}：{1}（冷却 {2} 秒）"),Worn[4]->Name,Worn[4]->Effect,N(FMath::RoundToInt(Worn[4]->Cooldown))):LOCTEXT("NoFabao","法宝槽空置"),18.f)
    }),FMargin(24.f,14.f,24.f,16.f)));
    const int32 Major=Svc->AnchorMajor();
    for(const FGear& G:Gear())
    {
        if(G.Slot!=Tab)continue;
        const FName Id=G.Id;const bool bLocked=Major<G.MinMajor;const bool bWorn=Worn[G.Slot]==&G;
        Add(Paper(SNew(SHorizontalBox)
            +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(0.f,0.f,14.f,0.f))
            [SNew(SInkItemSlot).SlotSize(70.f).Icon(Art::ItemIcon(G.Icon)).Quality(EQuality(FMath::Clamp(G.Tier,1,4))).Locked(bLocked)]
            +SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
            [Rows({Head(FText::Format(LOCTEXT("GearHead","{0} · {1}起"),G.Name,MajorName(G.MinMajor)),24.f),Hint(G.Effect,18.f)},2.f)]
            +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
            [Btn(bWorn?LOCTEXT("GearOff","卸下"):bLocked?LOCTEXT("GearLocked","未解锁"):LOCTEXT("GearOn","试穿"),bWorn?EButtonSkin::Paper:EButtonSkin::Jade,FName(*(TEXT("Gear.Try.")+Id.ToString())),
                FSimpleDelegate::CreateLambda([this,Id]{Exec(TEXT("GearTry"),{Id.ToString()});}),!bLocked||bWorn,50.f,120.f,21.f)],FMargin(22.f,12.f,22.f,14.f)));
    }
    Body->SetContent(List);
}

#undef LOCTEXT_NAMESPACE

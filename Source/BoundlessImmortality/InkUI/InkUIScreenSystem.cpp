#include "InkUI/InkUIScreens.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SUniformGridPanel.h"

#define LOCTEXT_NAMESPACE "InkUISystem"

using namespace InkUI;

namespace
{
    TSharedRef<SWidget> SectionCard(const FText& Title,const FText& Hint,TSharedRef<SWidget> Control,int32 Order)
    {
        return SNew(SInkAnimBox).Delay(Motion::Stagger(Order,.05f,.2f)).Distance(22.f)
        [
            SNew(SOverlay)
            +SOverlay::Slot()[SNew(SInkPaint).OnPaint_Lambda([](FPainter& P,const FGeometry&){P.Skin(Art::ESkin::CardPaper,FVector2f(0.f,0.f),P.Size(),FLinearColor::White,.45f);})]
            +SOverlay::Slot().Padding(FMargin(30.f,22.f,30.f,24.f))
            [
                SNew(SVerticalBox)
                +SVerticalBox::Slot().AutoHeight()[MakeText(Title,EFont::Section,Colors().TextDark,29.f)]
                +SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f,2.f,0.f,12.f))[MakeText(Hint,EFont::Small,Colors().TextDarkDim,21.f,0.f,true)]
                +SVerticalBox::Slot().AutoHeight()[Control]
            ]
        ];
    }
}

void SInkJourneyScreen::Construct(const FArguments& InArgs)
{
    InitScreen(InArgs._Root.ToSharedRef(),TEXT("Journey"));
    const TWeakPtr<SInkUIRoot> WeakRoot=GetRoot();
    const int32 Columns=IsLandscape()?3:2;
    const TSharedRef<SUniformGridPanel> Grid=SNew(SUniformGridPanel).SlotPadding(FMargin(8.f));
    int32 N=0;
    for(const FJourneyEntry& Entry:M().Journey)
    {
        const FJourneyEntry E=Entry;
        Grid->AddSlot(N%Columns,N/Columns)
        [
            SNew(SInkAnimBox).Enter(EEnter::Pop).Delay(.06f+Motion::Stagger(N,.03f,.24f)).Duration(.3f)
            [
                SNew(SInkClickable).TestId(FName(*(TEXT("Journey.")+E.Id.ToString()))).Radius(12.f)
                .OnClicked_Lambda([E,WeakRoot]
                {
                    const auto R=WeakRoot.Pin();if(!R)return;
                    if(!E.Route.IsNone()){R->OpenPage(E.Route);return;}
                    R->Toast(FText::Format(LOCTEXT("NeedService","{0}需连接直播服务，本地验证暂不开放"),E.Title),2);
                })
                [
                    SNew(SBox).HeightOverride(248.f)
                    [
                        SNew(SInkPaint).OnPaint_Lambda([E](FPainter& P,const FGeometry&)
                        {
                            const auto& K=Colors();const FVector2f S=P.Size();
                            P.Halo(FVector2f(4.f,4.f),S-FVector2f(8.f,8.f),10.f,12.f,WithAlpha(K.InkBlack,.3f));
                            P.Next();
                            P.Skin(Art::ESkin::CardPaper,FVector2f(0.f,0.f),S,FLinearColor::White,.4f);
                            P.Next();
                            const FVector2f ImgPos(14.f,14.f),ImgExt(S.X-28.f,(S.X-28.f)/3.f);
                            P.PushClip(ImgPos,ImgExt);
                            P.Brush(Art::Icon(Art::EIconSet::FeatureScenes,E.Scene),ImgPos,ImgExt,E.bNeedsService?FLinearColor(.78f,.78f,.76f,1.f):FLinearColor::White);
                            P.PopClip();
                            P.GoldFrame(ImgPos,ImgExt,0.f,WithAlpha(K.GoldDeep,.85f),0.f,false);
                            P.Next();
                            P.Text(E.Title.ToString(),Font(EFont::Title,36.f),FVector2f(S.X*.5f,ImgPos.Y+ImgExt.Y+38.f),K.TextDark,FVector2f(.5f,.5f));
                            P.Text(E.Description.ToString(),Font(EFont::Small,21.f),FVector2f(S.X*.5f,ImgPos.Y+ImgExt.Y+80.f),K.TextDarkDim,FVector2f(.5f,.5f));
                            const FVector2f Chip(S.X-114.f,20.f);
                            P.Skin(E.bNeedsService?Art::ESkin::PlaqueLacquer:Art::ESkin::PlaqueJade,Chip,FVector2f(96.f,34.f));
                            P.Next();P.Next();
                            P.Text(E.bNeedsService?TEXT("需联网"):TEXT("可前往"),Font(EFont::Tiny,18.f),Chip+FVector2f(48.f,17.f),E.bNeedsService?K.TextGold:K.TextDark,FVector2f(.5f,.52f));
                        })
                    ]
                ]
            ]
        ];
        ++N;
    }
    BuildPage(LOCTEXT("JourneyTitle","仙途"),LOCTEXT("JourneySeal","途"),
        SNew(SBox).HAlign(HAlign_Center)[MakeText(LOCTEXT("JourneySub","仙途百事 · 九州玩法一览"),EFont::Section,Colors().TextDark,28.f)],
        SNew(SScrollBox).ScrollBarStyle(&ScrollStyle())+SScrollBox::Slot()[Grid],
        MakeText(LOCTEXT("JourneyFoot","仙途玩法为本地实现：竞采对手与坊市买家为本地模拟，收据仅作本地记录；接入直播服务后切换为正式数据。"),EFont::Small,Colors().TextDarkDim,21.f,0.f,true,ETextJustify::Center),
        SNullWidget::NullWidget);
}

void SInkSettingsScreen::Construct(const FArguments& InArgs)
{
    InitScreen(InArgs._Root.ToSharedRef(),TEXT("Settings"));
    Draft=M().Settings;
    const TWeakPtr<SInkUIRoot> WeakRoot=GetRoot();
    BuildPage(LOCTEXT("SettingsTitle","设置"),LOCTEXT("SettingsSeal","静"),SNullWidget::NullWidget,
        SNew(SScrollBox).ScrollBarStyle(&ScrollStyle())+SScrollBox::Slot()[SAssignNew(Content,SBox)],
        SNew(SHorizontalBox)
        +SHorizontalBox::Slot().FillWidth(1.f).HAlign(HAlign_Left)
        [
            SNew(SInkButton).Skin(EButtonSkin::Paper).Text(LOCTEXT("Reset","恢复默认")).Height(74.f).MinWidth(220.f).TestId(TEXT("Settings.Reset"))
            .OnClicked_Lambda([this]{Draft=FSettings();Rebuild();})
        ]
        +SHorizontalBox::Slot().FillWidth(1.f).HAlign(HAlign_Right)
        [
            SNew(SInkButton).Skin(EButtonSkin::Gold).Text(LOCTEXT("Apply","应用设置")).Height(74.f).MinWidth(240.f).Highlight(true).TestId(TEXT("Settings.Apply"))
            .OnClicked_Lambda([this,WeakRoot]
            {
                M().Settings=Draft;
                Motion::SetReduced(Draft.bReducedMotion);
                if(const auto R=WeakRoot.Pin()){R->RunCommand(TEXT("ApplySettings"));R->Toast(LOCTEXT("Applied","设置已应用并保存"),1);}
            })
        ],
        SNullWidget::NullWidget);
    Rebuild();
}

void SInkSettingsScreen::Rebuild()
{
    Content->SetContent(
        SNew(SVerticalBox)
        +SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f,4.f))
        [
            SectionCard(LOCTEXT("Quality","画面品质"),LOCTEXT("QualityHint","流畅省电，精致呈现更完整的光影与粒子。"),
                SNew(SInkTabBar).Tabs(TArray<FText>{LOCTEXT("Low","流畅"),LOCTEXT("Mid","均衡"),LOCTEXT("High","精致")}).Selected(Draft.Quality).Height(62.f)
                .TestPrefix(TEXT("Settings.Quality")).OnSelected_Lambda([this](int32 I){Draft.Quality=I;}),0)
        ]
        +SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f,4.f))
        [
            SectionCard(LOCTEXT("Fps","帧率上限"),LOCTEXT("FpsHint","直播推流建议 60 帧；设备发热时可降至 30 帧。"),
                SNew(SInkTabBar).Tabs(TArray<FText>{LOCTEXT("Fps30","30 帧"),LOCTEXT("Fps60","60 帧")}).Selected(Draft.FrameRate>=60?1:0).Height(62.f)
                .TestPrefix(TEXT("Settings.Fps")).OnSelected_Lambda([this](int32 I){Draft.FrameRate=I?60:30;}),1)
        ]
        +SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f,4.f))
        [
            SectionCard(LOCTEXT("Motion","界面动效"),LOCTEXT("MotionHint","减少动效会关闭位移、呼吸与粒子，仅保留短淡入。"),
                SNew(SInkTabBar).Tabs(TArray<FText>{LOCTEXT("MotionStd","标准"),LOCTEXT("MotionLess","减少")}).Selected(Draft.bReducedMotion?1:0).Height(62.f)
                .TestPrefix(TEXT("Settings.Motion")).OnSelected_Lambda([this](int32 I){Draft.bReducedMotion=I==1;}),2)
        ]
        +SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f,4.f))
        [
            SectionCard(LOCTEXT("About","关于"),LOCTEXT("AboutHint","本地验证构建：直播 SDK 与联网服务尚未接入，排行、活动、签到使用本地数据演示界面。"),SNullWidget::NullWidget,3)
        ]);
}

void SInkSavesScreen::Construct(const FArguments& InArgs)
{
    InitScreen(InArgs._Root.ToSharedRef(),TEXT("Saves"));
    BuildPage(LOCTEXT("SavesTitle","存档"),LOCTEXT("SavesSeal","录"),SNullWidget::NullWidget,
        SNew(SScrollBox).ScrollBarStyle(&ScrollStyle())+SScrollBox::Slot()[SAssignNew(Content,SBox)],
        MakeText(LOCTEXT("SavesFoot","存档记录当前场景与角色位置，保存在本机。"),EFont::Small,Colors().TextDarkDim,21.f,0.f,true,ETextJustify::Center),
        SNullWidget::NullWidget);
    Rebuild();
}

void SInkSavesScreen::OnModelChanged()
{
    if(SeenSaves==M().SavesRev)return;
    Rebuild();
}

void SInkSavesScreen::Rebuild()
{
    SeenSaves=M().SavesRev;
    const TWeakPtr<SInkUIRoot> WeakRoot=GetRoot();
    const auto R=GetRoot();
    const bool bInGame=R&&R->GetBase()==EBase::Hud;
    const TSharedRef<SVerticalBox> List=SNew(SVerticalBox);
    int32 N=0;
    for(const FSaveSlot& Slot:M().Saves)
    {
        const FSaveSlot S=Slot;
        auto Command=[WeakRoot](const TCHAR* Verb,int32 Index){return FSimpleDelegate::CreateLambda([WeakRoot,Verb,Index]{if(const auto Root=WeakRoot.Pin())Root->RunCommand(FName(*FString::Printf(TEXT("%s_%d"),Verb,Index)));});};
        const FSimpleDelegate Delete=FSimpleDelegate::CreateLambda([WeakRoot,S,Command]
        {
            if(const auto Root=WeakRoot.Pin())Root->Confirm(LOCTEXT("DelTitle","删除存档"),FText::Format(LOCTEXT("DelBody","确定删除「{0}」？删除后无法恢复。"),S.Title),Command(TEXT("DeleteSlot"),S.Index),true,LOCTEXT("DelConfirm","删除"));
        });
        // Only the actions that apply: an empty slot in the main menu has none, saving needs a running game.
        const TSharedRef<SVerticalBox> Actions=SNew(SVerticalBox);
        if(S.bUsed)
        {
            Actions->AddSlot().AutoHeight().Padding(FMargin(0.f,3.f))
            [
                SNew(SInkButton).Skin(EButtonSkin::Gold).Text(LOCTEXT("Load","读取")).Height(58.f).MinWidth(140.f).FontSize(24.f).Enabled(S.bValid)
                .TestId(FName(*FString::Printf(TEXT("Saves.Load%d"),S.Index))).OnClicked(Command(TEXT("LoadSlot"),S.Index))
            ];
        }
        if(bInGame)
        {
            Actions->AddSlot().AutoHeight().Padding(FMargin(0.f,3.f))
            [
                SNew(SInkButton).Skin(S.bUsed?EButtonSkin::Paper:EButtonSkin::Gold).Text(LOCTEXT("Save","保存")).Height(58.f).MinWidth(140.f).FontSize(24.f)
                .TestId(FName(*FString::Printf(TEXT("Saves.Save%d"),S.Index))).OnClicked(Command(TEXT("SaveSlot"),S.Index))
            ];
        }
        if(S.bUsed)
        {
            Actions->AddSlot().AutoHeight().Padding(FMargin(0.f,3.f))
            [
                SNew(SInkButton).Skin(EButtonSkin::Cinnabar).Text(LOCTEXT("Delete","删除")).Height(46.f).MinWidth(140.f).FontSize(22.f)
                .TestId(FName(*FString::Printf(TEXT("Saves.Delete%d"),S.Index))).OnClicked(Delete)
            ];
        }
        List->AddSlot().AutoHeight().Padding(FMargin(0.f,6.f))
        [
            SNew(SInkAnimBox).Delay(Motion::Stagger(N++,.05f,.2f)).Distance(24.f)
            [
                SNew(SBox).HeightOverride(186.f)
                [
                    SNew(SOverlay)
                    +SOverlay::Slot()
                    [
                        SNew(SInkPaint).OnPaint_Lambda([S](FPainter& P,const FGeometry&)
                        {
                            const auto& K=Colors();const FVector2f Sz=P.Size();
                            P.Skin(Art::ESkin::CardOrnate,FVector2f(0.f,0.f),Sz,FLinearColor::White,.42f);
                            P.Next();
                            const FVector2f ImgPos(28.f,30.f),ImgExt(150.f,Sz.Y-60.f);
                            if(S.bUsed)
                            {
                                P.PushClip(ImgPos,ImgExt);
                                P.BrushFit(Art::Icon(Art::EIconSet::CohesionScenes,4+S.Index),ImgPos-FVector2f(80.f,0.f),FVector2f(ImgExt.X+160.f,ImgExt.Y));
                                P.PopClip();
                                P.GoldFrame(ImgPos,ImgExt,0.f,WithAlpha(K.GoldDeep,.8f),0.f,false);
                            }
                            else
                            {
                                P.Rounded(ImgPos,ImgExt,WithAlpha(K.PaperShade,.4f),8.f,WithAlpha(K.PaperEdge,.6f),1.f);
                                P.Next();
                                P.Text(TEXT("空"),Font(EFont::Title,48.f),ImgPos+ImgExt*.5f,WithAlpha(K.TextDarkDim,.5f),FVector2f(.5f,.55f));
                            }
                        })
                    ]
                    +SOverlay::Slot().Padding(FMargin(198.f,26.f,24.f,22.f))
                    [
                        SNew(SHorizontalBox)
                        +SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
                        [
                            SNew(SVerticalBox)
                            +SVerticalBox::Slot().AutoHeight()[MakeText(S.Title,EFont::Section,Colors().TextDark,30.f)]
                            +SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f,4.f))[MakeText(S.Detail,EFont::Small,S.bValid?Colors().TextDarkDim:Colors().TextRed,22.f,0.f,true)]
                            +SVerticalBox::Slot().AutoHeight()[MakeText(S.Time,EFont::Data,Colors().TextJade,22.f)]
                        ]
                        +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Actions]
                    ]
                ]
            ]
        ];
    }
    if(!bInGame)
    {
        List->AddSlot().AutoHeight().HAlign(HAlign_Center).Padding(FMargin(0.f,10.f))
        [
            MakeText(LOCTEXT("MenuSaves","进入游戏后可保存当前行程"),EFont::Small,Colors().TextDarkDim,22.f)
        ];
    }
    Content->SetContent(List);
}

#undef LOCTEXT_NAMESPACE

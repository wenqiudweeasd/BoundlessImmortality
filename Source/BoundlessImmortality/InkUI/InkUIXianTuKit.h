#pragma once

#include "CoreMinimal.h"
#include "InkUI/InkUIWidgets.h"
#include "XianTu/XianTuRules.h"
#include "XianTu/XianTuSubsystem.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SWrapBox.h"

/**
 * Building blocks shared by the 仙途 pages: paper cards, plaques, buttons, item cells and meters drawn with the Ink
 * skins. Header-only so each page file stays self-contained.
 */
namespace InkUI::Kit
{
    inline FText T(const TCHAR* S){return FText::FromString(S);}
    inline FText N(int64 V){return FText::AsNumber(V);}

    inline TSharedRef<SWidget> Paper(TSharedRef<SWidget> Content,FMargin Pad=FMargin(28.f,20.f,28.f,22.f),Art::ESkin Skin=Art::ESkin::CardPaper)
    {
        return SNew(SOverlay)
            +SOverlay::Slot()[SNew(SInkPaint).OnPaint_Lambda([Skin](FPainter& P,const FGeometry&){P.Skin(Skin,FVector2f(0.f,0.f),P.Size(),FLinearColor::White,.45f);})]
            +SOverlay::Slot().Padding(Pad)[Content];
    }
    inline TSharedRef<SWidget> Head(const TAttribute<FText>& Text,float Size=29.f){return MakeText(Text,EFont::Section,Colors().TextDark,Size);}
    inline TSharedRef<SWidget> Line(const TAttribute<FText>& Text,float Size=23.f,FLinearColor Color=Colors().TextDark){return MakeText(Text,EFont::Body,Color,Size,0.f,true);}
    inline TSharedRef<SWidget> Hint(const TAttribute<FText>& Text,float Size=20.f){return MakeText(Text,EFont::Small,Colors().TextDarkDim,Size,0.f,true);}

    /** 0 jade, 1 lacquer (gold text), 2 cinnabar, 3 gold. */
    inline TSharedRef<SWidget> Plaque(const TAttribute<FText>& Text,int32 Tone,float W=112.f,float H=38.f)
    {
        return SNew(SInkPaint).DesiredSize(FVector2D(W,H)).OnPaint_Lambda([Text,Tone](FPainter& P,const FGeometry&)
        {
            const auto& K=Colors();
            const Art::ESkin Skin=Tone==0?Art::ESkin::PlaqueJade:Tone==2?Art::ESkin::PlaqueCinnabar:Tone==3?Art::ESkin::PlaqueGold:Art::ESkin::PlaqueLacquer;
            P.Skin(Skin,FVector2f(0.f,0.f),P.Size());P.Next();P.Next();
            const FLinearColor C=Tone==0?K.TextDark:Tone==2?Hex(TEXT("FFF0D8")):Tone==3?K.TextDark:K.TextGold;
            P.Text(Text.Get().ToString(),Font(EFont::Tiny,17.f),P.Size()*.5f,C,FVector2f(.5f,.52f));
        });
    }

    inline TSharedRef<SWidget> Btn(const TAttribute<FText>& Text,EButtonSkin Skin,FName Id,FSimpleDelegate Click,const TAttribute<bool>& Enabled=true,float H=60.f,float W=160.f,float Size=23.f)
    {
        return SNew(SInkButton).Text(Text).Skin(Skin).TestId(Id).OnClicked(Click).Enabled(Enabled).Height(H).MinWidth(W).FontSize(Size);
    }

    inline TSharedRef<SWidget> Cell(FName Item,int32 Count,float Size=62.f)
    {
        const XianTuRules::FItem* Def=XianTuRules::Item(Item);
        return SNew(SInkItemSlot).SlotSize(Size).Icon(Art::ItemIcon(Def?Def->Icon:FName(TEXT("crystal")))).Quality(EQuality(Def?FMath::Clamp(Def->Quality,0,5):0)).Count(Count);
    }

    inline TSharedRef<SWidget> Strip(const TArray<FXianTuReward>& Rewards,float Size=58.f)
    {
        const TSharedRef<SWrapBox> Box=SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(6.f,6.f));
        for(const FXianTuReward& R:Rewards)Box->AddSlot()[Cell(R.Item,R.Count,Size)];
        return Box;
    }

    inline TSharedRef<SWidget> Pair(const FText& Key,const TAttribute<FText>& Value,float Size=22.f)
    {
        return SNew(SHorizontalBox)
            +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(0.f,0.f,10.f,0.f))[MakeText(Key,EFont::Small,Colors().TextDarkDim,Size)]
            +SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)[MakeText(Value,EFont::Data,Colors().TextDark,Size)];
    }

    inline TSharedRef<SWidget> Meter(const TAttribute<float>& Fraction,const TAttribute<FText>& Value,EBar Style,float W=560.f,float H=26.f)
    {
        return SNew(SInkProgressBar).Style(Style).Width(W).Height(H).Fraction(Fraction).Value(Value);
    }

    inline TSharedRef<SWidget> Gap(float H=10.f){return SNew(SBox).HeightOverride(H);}

    inline void Cover(FPainter& P,const FSlateBrush* Brush,FVector2f Pos,FVector2f Ext,FLinearColor Tint)
    {
        const float Aspect=FMath::Max(.1f,Art::Aspect(Brush));
        FVector2f Draw(Ext.X,Ext.X/Aspect);
        if(Draw.Y<Ext.Y)Draw=FVector2f(Ext.Y*Aspect,Ext.Y);
        P.PushClip(Pos,Ext);
        P.Brush(Brush,Pos+(Ext-Draw)*.5f,Draw,Tint);
        P.PopClip();
    }

    inline TSharedRef<SWidget> Scene(int32 Index,float Height,bool bDim)
    {
        return SNew(SBox).HeightOverride(Height)
        [
            SNew(SInkPaint).OnPaint_Lambda([Index,bDim](FPainter& P,const FGeometry&)
            {
                const auto& K=Colors();const FVector2f S=P.Size();
                Cover(P,Art::Icon(Art::EIconSet::FeatureScenes,Index),FVector2f(0.f,0.f),S,bDim?FLinearColor(.55f,.55f,.55f,1.f):FLinearColor::White);
                P.Next();
                P.VGradient(FVector2f(0.f,S.Y*.45f),FVector2f(S.X,S.Y*.55f),{{0.f,WithAlpha(K.InkBlack,0.f)},{1.f,WithAlpha(K.InkBlack,.7f)}});
                P.GoldFrame(FVector2f(0.f,0.f),S,1.f,WithAlpha(K.Gold,.9f),.6f,true);
            })
        ];
    }

    inline TSharedRef<SWidget> Rows(const TArray<TSharedRef<SWidget>>& Widgets,float Spacing=6.f)
    {
        const TSharedRef<SVerticalBox> Box=SNew(SVerticalBox);
        for(const auto& W:Widgets)Box->AddSlot().AutoHeight().Padding(FMargin(0.f,Spacing*.5f))[W];
        return Box;
    }

    /** Two-column grid of equal cards (one column when Columns is 1). */
    inline TSharedRef<SWidget> Grid(const TArray<TSharedRef<SWidget>>& Cards,int32 Columns=2,float Spacing=10.f)
    {
        const TSharedRef<SVerticalBox> Box=SNew(SVerticalBox);
        for(int32 I=0;I<Cards.Num();I+=Columns)
        {
            const TSharedRef<SHorizontalBox> Row=SNew(SHorizontalBox);
            for(int32 C=0;C<Columns;++C)
            {
                Row->AddSlot().FillWidth(1.f).Padding(FMargin(C?Spacing*.5f:0.f,0.f,C+1<Columns?Spacing*.5f:0.f,0.f))
                [Cards.IsValidIndex(I+C)?Cards[I+C]:SNullWidget::NullWidget];
            }
            Box->AddSlot().AutoHeight().Padding(FMargin(0.f,Spacing*.5f))[Row];
        }
        return Box;
    }

    /** Lacquer bar of wallet entries (ledger keys such as CUR_STONE, CUR_JADE), read live from the ledger. */
    inline TSharedRef<SWidget> WalletBar(TWeakObjectPtr<UXianTuSubsystem> Ledger,TArray<FName> Keys,float Height=50.f)
    {
        return SNew(SInkPaint).DesiredSize(FVector2D(150.f*Keys.Num(),Height)).OnPaint_Lambda([Ledger,Keys](FPainter& P,const FGeometry&)
        {
            const auto& K=Colors();const FVector2f S=P.Size();
            P.Skin(Art::ESkin::PlaqueLacquer,FVector2f(0.f,0.f),S,FLinearColor(1,1,1,.95f));
            P.Next();
            const float W=S.X/FMath::Max(1,Keys.Num());
            for(int32 I=0;I<Keys.Num();++I)
            {
                const XianTuRules::FItem* Def=XianTuRules::Item(Keys[I]);
                const UXianTuSubsystem* L=Ledger.Get();
                const int64 Amount=L?L->ItemCount(Keys[I]):0;
                const float X=I*W;
                if(I)P.Line(FVector2f(X,S.Y*.24f),FVector2f(X,S.Y*.76f),WithAlpha(K.Gold,.35f),1.f);
                P.BrushFit(Art::ItemIcon(Def?Def->Icon:FName(TEXT("crystal"))),FVector2f(X+12.f,S.Y*.5f-17.f),FVector2f(34.f,34.f));
                P.Text(L?FormatAmount(double(Amount)).ToString():TEXT("—"),Font(EFont::Data,21.f),FVector2f(X+W-12.f,S.Y*.5f),K.TextGold,FVector2f(1.f,.52f));
            }
        });
    }
}

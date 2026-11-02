#include "InkUI/InkUITokens.h"
#include "InkUI/InkUIArt.h"
#include "Engine/Font.h"
#include "Styling/CoreStyle.h"

#define LOCTEXT_NAMESPACE "InkUI"

namespace InkUI
{
    FLinearColor Hex(const TCHAR* Value,float Alpha)
    {
        FLinearColor C=FLinearColor::FromSRGBColor(FColor::FromHex(Value));C.A=Alpha;return C;
    }

    FLinearColor WithAlpha(FLinearColor Color,float Alpha){Color.A=Alpha;return Color;}

    FLinearColor Lerp(const FLinearColor& A,const FLinearColor& B,float T){return A+(B-A)*FMath::Clamp(T,0.f,1.f);}

    const FPalette& Colors()
    {
        static const FPalette P=[]
        {
            FPalette C;
            C.InkBlack=Hex(TEXT("0A0E0F"));C.Lacquer=Hex(TEXT("151C1D"));C.LacquerHi=Hex(TEXT("243231"));C.LacquerLo=Hex(TEXT("0C1112"));
            C.JadeDeep=Hex(TEXT("0F3C36"));C.Jade=Hex(TEXT("2F8F7F"));C.JadeBright=Hex(TEXT("86E3CC"));
            C.Gold=Hex(TEXT("D4AA62"));C.GoldBright=Hex(TEXT("F8E0A0"));C.GoldDeep=Hex(TEXT("8A6430"));C.GoldLine=Hex(TEXT("C99B55"));
            C.Paper=Hex(TEXT("EEE0C2"));C.PaperLight=Hex(TEXT("F9F1DE"));C.PaperShade=Hex(TEXT("D9C49A"));C.PaperEdge=Hex(TEXT("B39463"));
            C.Cinnabar=Hex(TEXT("B5402E"));C.CinnabarBright=Hex(TEXT("E35B3D"));C.RedDot=Hex(TEXT("EE3F2A"));
            C.TextLight=Hex(TEXT("F6ECD4"));C.TextLightDim=Hex(TEXT("C2B497"));C.TextDark=Hex(TEXT("2A1F15"));C.TextDarkDim=Hex(TEXT("6E5B43"));
            C.TextGold=Hex(TEXT("F3D38C"));C.TextJade=Hex(TEXT("1F7A6C"));C.TextRed=Hex(TEXT("A93A27"));
            C.Veil=Hex(TEXT("05090A"),.62f);
            return C;
        }();
        return P;
    }

    FLinearColor QualityColor(EQuality Quality)
    {
        switch(Quality)
        {
        case EQuality::Uncommon:return Hex(TEXT("6CC46E"));
        case EQuality::Rare:return Hex(TEXT("4FA3F0"));
        case EQuality::Epic:return Hex(TEXT("B470F2"));
        case EQuality::Legendary:return Hex(TEXT("F2A33A"));
        case EQuality::Mythic:return Hex(TEXT("F05454"));
        default:return Hex(TEXT("B9B2A1"));
        }
    }

    FText QualityName(EQuality Quality)
    {
        switch(Quality)
        {
        case EQuality::Uncommon:return LOCTEXT("QualityUncommon","灵品");
        case EQuality::Rare:return LOCTEXT("QualityRare","宝品");
        case EQuality::Epic:return LOCTEXT("QualityEpic","仙品");
        case EQuality::Legendary:return LOCTEXT("QualityLegendary","神品");
        case EQuality::Mythic:return LOCTEXT("QualityMythic","圣品");
        default:return LOCTEXT("QualityCommon","凡品");
        }
    }

    float FontSize(EFont Role)
    {
        switch(Role)
        {
        case EFont::Title:return 50;
        case EFont::Display:return 64;
        case EFont::Section:return 32;
        case EFont::Button:return 29;
        case EFont::Data:return 26;
        case EFont::Small:return 23;
        case EFont::Tiny:return 20;
        case EFont::Hud:return 24;
        default:return 27;
        }
    }

    FSlateFontInfo Font(EFont Role,float SizeOverride,float Outline,FLinearColor OutlineColor)
    {
        const float Logical=SizeOverride>0?SizeOverride:FontSize(Role);
        const bool bCalligraphy=Role==EFont::Title||Role==EFont::Display;
        FSlateFontInfo Info=FCoreStyle::GetDefaultFontStyle("Regular",FMath::Max(1,FMath::RoundToInt(Logical*.75f)));
        if(const UObject* Asset=bCalligraphy?Art::DisplayFont():Art::BodyFont())Info.FontObject=Asset;
        if(Outline>0)
        {
            Info.OutlineSettings.OutlineSize=FMath::RoundToInt(Outline);
            Info.OutlineSettings.OutlineColor=OutlineColor;
        }
        return Info;
    }
}

#undef LOCTEXT_NAMESPACE

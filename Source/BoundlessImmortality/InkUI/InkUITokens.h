#pragma once

#include "CoreMinimal.h"
#include "Fonts/SlateFontInfo.h"

/**
 * Xianxia legend UI tokens: lacquer-black frames, gilded trims, jade and cinnabar accents,
 * parchment reading surfaces. Values are authored in sRGB and converted once here.
 * Layout units are logical pixels on the 720 short-side reference (ShortestSide DPI curve).
 */
namespace InkUI
{
    enum class EFont : uint8 { Title, Display, Section, Body, Button, Data, Small, Tiny, Hud };
    enum class EQuality : uint8 { Common, Uncommon, Rare, Epic, Legendary, Mythic };

    struct FPalette
    {
        FLinearColor InkBlack,Lacquer,LacquerHi,LacquerLo;
        FLinearColor JadeDeep,Jade,JadeBright;
        FLinearColor Gold,GoldBright,GoldDeep,GoldLine;
        FLinearColor Paper,PaperLight,PaperShade,PaperEdge;
        FLinearColor Cinnabar,CinnabarBright,RedDot;
        FLinearColor TextLight,TextLightDim,TextDark,TextDarkDim,TextGold,TextJade,TextRed;
        FLinearColor Veil;
    };

    BOUNDLESSIMMORTALITY_API const FPalette& Colors();
    BOUNDLESSIMMORTALITY_API FLinearColor Hex(const TCHAR* Hex,float Alpha=1.f);
    BOUNDLESSIMMORTALITY_API FLinearColor WithAlpha(FLinearColor Color,float Alpha);
    BOUNDLESSIMMORTALITY_API FLinearColor Lerp(const FLinearColor& A,const FLinearColor& B,float T);
    BOUNDLESSIMMORTALITY_API FLinearColor QualityColor(EQuality Quality);
    BOUNDLESSIMMORTALITY_API FText QualityName(EQuality Quality);

    /** Size is in logical pixels; Slate points render at 96 DPI so the font tokens scale by 0.75. */
    BOUNDLESSIMMORTALITY_API FSlateFontInfo Font(EFont Role,float SizeOverride=0,float Outline=0,FLinearColor OutlineColor=FLinearColor::Black);
    BOUNDLESSIMMORTALITY_API float FontSize(EFont Role);

    namespace Metrics
    {
        constexpr float ReferenceShort=720.f;
        constexpr float Outer=24.f;
        constexpr float Gap=16.f;
        constexpr float MinHit=88.f;
        constexpr float ButtonHeight=76.f;
        constexpr float ButtonHeightSmall=60.f;
        constexpr float HeaderHeight=96.f;
        constexpr float FooterHeight=112.f;
        constexpr float Radius=10.f;
        constexpr float LandscapeRatio=1.25f;
        constexpr float OrientationHysteresis=.03f;
        constexpr float DenseMaxWidth=1180.f;
        constexpr float DrawerMaxWidth=620.f;
    }

    namespace Durations
    {
        constexpr float PageOpen=.34f;
        constexpr float PageClose=.18f;
        constexpr float VeilFade=.22f;
        constexpr float Press=.07f;
        constexpr float Release=.12f;
        constexpr float Stagger=.03f;
        constexpr float MaxStagger=.15f;
        constexpr float Tab=.18f;
        constexpr float Select=.12f;
        constexpr float Number=.32f;
        constexpr float Toast=2.6f;
        constexpr float Reward=.42f;
        constexpr float BreathPeriod=3.2f;
        constexpr float ShinePeriod=3.4f;
        constexpr float ReducedFade=.12f;
    }
}

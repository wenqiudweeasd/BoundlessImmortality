#pragma once

#include "CoreMinimal.h"
#include "Layout/Geometry.h"
#include "Styling/WidgetStyle.h"
#include "Fonts/SlateFontInfo.h"
#include "InkUI/InkUIArt.h"

class FSlateWindowElementList;
struct FSlateBrush;

namespace InkUI
{
    enum class EPanel : uint8 { Lacquer, Paper, Jade, Glass, Inset };

    /**
     * Immediate-mode painter for procedural ornament: gilded frames, cloud curls, glows, rays.
     * Every color is multiplied by the inherited widget opacity so render-opacity animation works.
     * Layer() advances for overlapping translucent elements, because batching may reorder within a layer.
     */
    class BOUNDLESSIMMORTALITY_API FPainter
    {
    public:
        FPainter(const FGeometry& InGeometry,FSlateWindowElementList& InOut,int32 InLayer,const FWidgetStyle& Style);

        const FGeometry& Geometry;
        FSlateWindowElementList& Out;
        int32 Layer;
        float Alpha;

        int32 Next(){return ++Layer;}
        FLinearColor C(FLinearColor Color) const{Color.A*=Alpha;return Color;}
        FVector2f Size() const{return FVector2f(Geometry.GetLocalSize());}

        void Brush(const FSlateBrush* Brush,FVector2f Pos,FVector2f Extent,FLinearColor Tint=FLinearColor::White);
        /** Painted skin sliced per its segment table; CapScale<=0 picks a uniform ornament scale. */
        void Skin(Art::ESkin Id,FVector2f Pos,FVector2f Extent,FLinearColor Tint=FLinearColor::White,float CapScale=-1.f);
        /** Returns the measured text size; Align is the anchor inside the text box (0..1). */
        FVector2f Text(const FString& Value,const FSlateFontInfo& Font,FVector2f Anchor,FLinearColor Color,FVector2f Align=FVector2f(0.f,0.f),FLinearColor Shadow=FLinearColor::Transparent);
        static FVector2f Measure(const FString& Value,const FSlateFontInfo& Font);
        /** Draws a texture brush fitted inside the box, preserving the sampled aspect. */
        void BrushFit(const FSlateBrush* Brush,FVector2f Pos,FVector2f Extent,FLinearColor Tint=FLinearColor::White);
        /** BrushFit with eased alpha falloff on each edge (fractions of the fitted size: left, top, right, bottom), so art cut by its canvas dissolves into the scene. */
        void BrushFitFeathered(const FSlateBrush* Brush,FVector2f Pos,FVector2f Extent,const FVector4f& Feather,FLinearColor Tint=FLinearColor::White);
        void Rounded(FVector2f Pos,FVector2f Extent,FLinearColor Fill,float Radius,FLinearColor Outline=FLinearColor::Transparent,float OutlineWidth=0.f);
        void VGradient(FVector2f Pos,FVector2f Extent,const TArray<TPair<float,FLinearColor>>& Stops,float Radius=0.f);
        void HGradient(FVector2f Pos,FVector2f Extent,const TArray<TPair<float,FLinearColor>>& Stops,float Radius=0.f);
        /** Vertex-colored quad; corners are TL, TR, BR, BL. */
        void Quad(FVector2f A,FVector2f B,FVector2f C2,FVector2f D,FLinearColor CA,FLinearColor CB,FLinearColor CC,FLinearColor CD);
        void Line(FVector2f A,FVector2f B,FLinearColor Color,float Thickness=1.f);
        /** Line that fades in and out at both ends; used for gilded dividers. */
        void FadeLine(FVector2f A,FVector2f B,FLinearColor Color,float Thickness=1.f,float FadeFraction=.25f);
        void Curve(FVector2f P0,FVector2f P1,FVector2f P2,FVector2f P3,FLinearColor Color,float Thickness=1.5f);
        void Diamond(FVector2f Center,FVector2f HalfExtent,FLinearColor Color);
        /** Radial falloff; Inner at the center, Outer (usually transparent) at the rim. */
        void Glow(FVector2f Center,FVector2f Radii,FLinearColor Inner,FLinearColor Outer=FLinearColor::Transparent,int32 Segments=40);
        /** Soft halo hugging a rounded rectangle, for shadows and selection glows. */
        void Halo(FVector2f Pos,FVector2f Extent,float Radius,float Spread,FLinearColor Color,bool bFillInside=false);
        void Ring(FVector2f Center,float Radius,float Thickness,FLinearColor Inner,FLinearColor Outer,float StartRadians=0.f,float SweepRadians=2.f*PI,int32 Segments=64);
        void Rays(FVector2f Center,float InnerRadius,float OuterRadius,int32 Count,float Rotation,float HalfWidthRadians,FLinearColor Color);
        /** Diagonal light band sweeping across the rect; T in 0..1. */
        void Shine(FVector2f Pos,FVector2f Extent,float T,float BandWidth,FLinearColor Color);
        void PushClip(FVector2f Pos,FVector2f Extent);
        void PopClip();

        void CornerOrnament(FVector2f Corner,FVector2f Direction,float Scale,FLinearColor Color);
        /** Auspicious cloud (xiangyun) curl, drawn from cubic spirals. */
        void Cloud(FVector2f Center,float Scale,bool bMirror,FLinearColor Color,float Thickness=2.f);
        void Divider(FVector2f Center,float Width,FLinearColor Color);
        void GoldFrame(FVector2f Pos,FVector2f Extent,float Inset,FLinearColor Color,float CornerScale=1.f,bool bDouble=true);
        void Panel(FVector2f Pos,FVector2f Extent,EPanel Kind,float Radius=10.f,bool bFrame=true);
        void RedDot(FVector2f Center,float Radius,float Pulse=0.f);
        void Motes(FVector2f Pos,FVector2f Extent,int32 Count,double Time,FLinearColor Color,uint32 Seed=7,float Speed=1.f,float MaxSize=5.f);

    private:
        void Submit(const TArray<FVector2f>& Points,const TArray<FLinearColor>& Colors,const TArray<uint32>& Indices);
    };

    /** Rounded-rect outline sampled clockwise from the top-left corner. */
    BOUNDLESSIMMORTALITY_API void RoundedOutline(FVector2f Pos,FVector2f Extent,float Radius,int32 SegmentsPerCorner,TArray<FVector2f>& OutPoints,TArray<FVector2f>& OutNormals);
}

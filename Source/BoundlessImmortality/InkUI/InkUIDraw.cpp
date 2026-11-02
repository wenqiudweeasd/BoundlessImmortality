#include "InkUI/InkUIDraw.h"
#include "InkUI/InkUIMotion.h"
#include "InkUI/InkUITokens.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Rendering/RenderingCommon.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"

namespace InkUI
{
    namespace
    {
        // Slate names gradients by their band direction: horizontal bands change colour top to bottom.
        constexpr EOrientation TopToBottom=Orient_Horizontal;
        constexpr EOrientation LeftToRight=Orient_Vertical;

        void LayoutSegments(const TArray<Art::FSliceSegment>& Segments,float Extent,float Scale,TArray<float>& Out)
        {
            float Fixed=0.f,Stretch=0.f;
            for(const auto& S:Segments)(S.bStretch?Stretch:Fixed)+=S.B-S.A;
            float Remain=Extent-Fixed*Scale;
            if(Stretch<=0.f||Remain<0.f){Scale=Extent/FMath::Max(1.f,Fixed);Remain=0.f;}
            for(const auto& S:Segments)Out.Add(S.bStretch?Remain*(S.B-S.A)/Stretch:(S.B-S.A)*Scale);
        }

        FVector2f Dir(float Radians){return FVector2f(FMath::Cos(Radians),FMath::Sin(Radians));}
    }

    void RoundedOutline(FVector2f Pos,FVector2f Extent,float Radius,int32 SegmentsPerCorner,TArray<FVector2f>& OutPoints,TArray<FVector2f>& OutNormals)
    {
        Radius=FMath::Clamp(Radius,0.f,FMath::Min(Extent.X,Extent.Y)*.5f);
        SegmentsPerCorner=FMath::Max(1,SegmentsPerCorner);
        const FVector2f Centers[]={Pos+FVector2f(Radius,Radius),Pos+FVector2f(Extent.X-Radius,Radius),Pos+Extent-FVector2f(Radius,Radius),Pos+FVector2f(Radius,Extent.Y-Radius)};
        for(int32 Corner=0;Corner<4;++Corner)
        {
            const float Start=PI+Corner*HALF_PI;
            for(int32 S=0;S<=SegmentsPerCorner;++S)
            {
                const FVector2f N=Dir(Start+HALF_PI*S/SegmentsPerCorner);
                OutPoints.Add(Centers[Corner]+N*Radius);OutNormals.Add(N);
            }
        }
    }

    FPainter::FPainter(const FGeometry& InGeometry,FSlateWindowElementList& InOut,int32 InLayer,const FWidgetStyle& Style)
        :Geometry(InGeometry),Out(InOut),Layer(InLayer),Alpha(Style.GetColorAndOpacityTint().A)
    {
    }

    void FPainter::Brush(const FSlateBrush* InBrush,FVector2f Pos,FVector2f Extent,FLinearColor Tint)
    {
        if(!InBrush||InBrush->DrawAs==ESlateBrushDrawType::NoDrawType||Extent.X<=0.f||Extent.Y<=0.f)return;
        FSlateDrawElement::MakeBox(Out,Layer,Geometry.ToPaintGeometry(Extent,FSlateLayoutTransform(Pos)),InBrush,ESlateDrawEffect::None,C(Tint));
    }

    void FPainter::BrushFit(const FSlateBrush* InBrush,FVector2f Pos,FVector2f Extent,FLinearColor Tint)
    {
        if(!InBrush)return;
        const float Aspect=Art::Aspect(InBrush);
        FVector2f Size=Extent;
        if(Extent.X/FMath::Max(1.f,Extent.Y)>Aspect)Size.X=Extent.Y*Aspect;else Size.Y=Extent.X/FMath::Max(.01f,Aspect);
        Brush(InBrush,Pos+(Extent-Size)*.5f,Size,Tint);
    }

    void FPainter::BrushFitFeathered(const FSlateBrush* InBrush,FVector2f Pos,FVector2f Extent,const FVector4f& Feather,FLinearColor Tint)
    {
        if(!Art::IsValid(InBrush)||Extent.X<=0.f||Extent.Y<=0.f||!FSlateApplication::IsInitialized())return;
        const FSlateResourceHandle Handle=FSlateApplication::Get().GetRenderer()->GetResourceHandle(*InBrush);
        if(!Handle.IsValid())return;
        const float Aspect=Art::Aspect(InBrush);
        FVector2f Size=Extent;
        if(Extent.X/FMath::Max(1.f,Extent.Y)>Aspect)Size.X=Extent.Y*Aspect;else Size.Y=Extent.X/FMath::Max(.01f,Aspect);
        const FVector2f Origin=Pos+(Extent-Size)*.5f;
        // Each feathered band gets smoothstep samples at 0, 1/3, 2/3 and 1; an unfeathered edge is a single opaque stop.
        auto Axis=[](float Lead,float Trail,TArray<float>& Stops,TArray<float>& Alphas)
        {
            static constexpr float Ease[]={0.f,.26f,.74f,1.f};
            if(Lead>0.f)for(int32 I=0;I<4;++I){Stops.Add(Lead*I/3.f);Alphas.Add(Ease[I]);}
            else{Stops.Add(0.f);Alphas.Add(1.f);}
            if(Trail>0.f)for(int32 I=3;I>=0;--I){Stops.Add(1.f-Trail*I/3.f);Alphas.Add(Ease[I]);}
            else{Stops.Add(1.f);Alphas.Add(1.f);}
        };
        TArray<float> Xs,Ax,Ys,Ay;
        Axis(Feather.X,Feather.Z,Xs,Ax);Axis(Feather.Y,Feather.W,Ys,Ay);
        const FBox2f& Region=InBrush->GetUVRegion();
        const FBox2f UV=Region.bIsValid?Region:FBox2f(FVector2f(0.f,0.f),FVector2f(1.f,1.f));
        const FSlateRenderTransform& Transform=Geometry.GetAccumulatedRenderTransform();
        const FLinearColor Base=C(Tint);
        TArray<FSlateVertex> Vertices;Vertices.Reserve(Xs.Num()*Ys.Num());
        for(int32 J=0;J<Ys.Num();++J)for(int32 I=0;I<Xs.Num();++I)
        {
            const FVector2f T(Xs[I],Ys[J]);
            FLinearColor Color=Base;Color.A*=Ax[I]*Ay[J];
            Vertices.Add(FSlateVertex::Make(Transform,Origin+Size*T,UV.Min+UV.GetSize()*T,Color.ToFColor(true)));
        }
        TArray<SlateIndex> Indices;
        const int32 Row=Xs.Num();
        for(int32 J=0;J+1<Ys.Num();++J)for(int32 I=0;I+1<Row;++I)
        {
            const SlateIndex A=SlateIndex(J*Row+I),B=SlateIndex(A+1),D=SlateIndex(A+Row),E=SlateIndex(D+1);
            Indices.Append({A,B,E,A,E,D});
        }
        FSlateDrawElement::MakeCustomVerts(Out,Layer,Handle,Vertices,Indices,nullptr,0,0);
    }

    void FPainter::Skin(Art::ESkin Id,FVector2f Pos,FVector2f Extent,FLinearColor Tint,float CapScale)
    {
        const Art::FSkin& S=Art::Skin(Id);
        const FSlateBrush* Texture=S.Path?Art::Image(S.Path):nullptr;
        if(!Art::IsValid(Texture)||Extent.X<1.f||Extent.Y<1.f)return;
        const FVector2f TexSize=S.Sheet.X>0.f&&S.Sheet.Y>0.f?S.Sheet:FVector2f(FMath::Max(1.f,float(Texture->ImageSize.X)),FMath::Max(1.f,float(Texture->ImageSize.Y)));
        bool bVerticalCaps=false;
        for(const auto& V:S.Vertical)bVerticalCaps|=!V.bStretch;
        const float Scale=CapScale>0.f?CapScale:(bVerticalCaps?FMath::Min3(1.f,Extent.X/S.SourceWidth(),Extent.Y/S.SourceHeight()):Extent.Y/FMath::Max(1.f,S.SourceHeight()));
        TArray<float> Widths,Heights;
        LayoutSegments(S.Horizontal,Extent.X,Scale,Widths);
        LayoutSegments(S.Vertical,Extent.Y,Scale,Heights);
        FSlateBrush Slice=*Texture;
        Slice.DrawAs=ESlateBrushDrawType::Image;Slice.Margin=FMargin(0);
        const FLinearColor Final=C(Tint);
        float Y=Pos.Y;
        for(int32 J=0;J<Heights.Num();++J)
        {
            float X=Pos.X;
            for(int32 I=0;I<Widths.Num();++I)
            {
                if(Widths[I]>.01f&&Heights[J]>.01f)
                {
                    const auto& H=S.Horizontal[I];const auto& V=S.Vertical[J];
                    Slice.SetUVRegion(FBox2f(FVector2f(H.A/TexSize.X,V.A/TexSize.Y),FVector2f(H.B/TexSize.X,V.B/TexSize.Y)));
                    // Half-pixel overlap on interior seams hides bilinear gaps between slices.
                    const FVector2f Grow(I+1<Widths.Num()?.5f:0.f,J+1<Heights.Num()?.5f:0.f);
                    FSlateDrawElement::MakeBox(Out,Layer,Geometry.ToPaintGeometry(FVector2f(Widths[I],Heights[J])+Grow,FSlateLayoutTransform(FVector2f(X,Y))),&Slice,ESlateDrawEffect::None,Final);
                }
                X+=Widths[I];
            }
            Y+=Heights[J];
        }
    }

    FVector2f FPainter::Measure(const FString& Value,const FSlateFontInfo& Font)
    {
        if(!FSlateApplication::IsInitialized()||Value.IsEmpty())return FVector2f::ZeroVector;
        return FVector2f(FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(Value,Font));
    }

    FVector2f FPainter::Text(const FString& Value,const FSlateFontInfo& Font,FVector2f Anchor,FLinearColor Color,FVector2f Align,FLinearColor Shadow)
    {
        const FVector2f Size=Measure(Value,Font);
        if(Size.X<=0.f)return Size;
        const FVector2f P=Anchor-Size*Align;
        if(Shadow.A>0.f)FSlateDrawElement::MakeText(Out,Layer,Geometry.ToPaintGeometry(Size,FSlateLayoutTransform(P+FVector2f(0.f,2.f))),Value,Font,ESlateDrawEffect::None,C(Shadow));
        FSlateDrawElement::MakeText(Out,Layer+1,Geometry.ToPaintGeometry(Size,FSlateLayoutTransform(P)),Value,Font,ESlateDrawEffect::None,C(Color));
        return Size;
    }

    void FPainter::Rounded(FVector2f Pos,FVector2f Extent,FLinearColor Fill,float Radius,FLinearColor Outline,float OutlineWidth)
    {
        if(Extent.X<=0.f||Extent.Y<=0.f)return;
        const FSlateRoundedBoxBrush Box(FLinearColor::White,Radius,C(Outline),OutlineWidth);
        FSlateDrawElement::MakeBox(Out,Layer,Geometry.ToPaintGeometry(Extent,FSlateLayoutTransform(Pos)),&Box,ESlateDrawEffect::None,C(Fill));
    }

    void FPainter::VGradient(FVector2f Pos,FVector2f Extent,const TArray<TPair<float,FLinearColor>>& Stops,float Radius)
    {
        TArray<FSlateGradientStop> S;
        for(const auto& P:Stops)S.Add(FSlateGradientStop(FVector2D(Extent.X*P.Key,Extent.Y*P.Key),C(P.Value)));
        FSlateDrawElement::MakeGradient(Out,Layer,Geometry.ToPaintGeometry(Extent,FSlateLayoutTransform(Pos)),S,TopToBottom,ESlateDrawEffect::None,FVector4f(Radius));
    }

    void FPainter::HGradient(FVector2f Pos,FVector2f Extent,const TArray<TPair<float,FLinearColor>>& Stops,float Radius)
    {
        TArray<FSlateGradientStop> S;
        for(const auto& P:Stops)S.Add(FSlateGradientStop(FVector2D(Extent.X*P.Key,Extent.Y*P.Key),C(P.Value)));
        FSlateDrawElement::MakeGradient(Out,Layer,Geometry.ToPaintGeometry(Extent,FSlateLayoutTransform(Pos)),S,LeftToRight,ESlateDrawEffect::None,FVector4f(Radius));
    }

    void FPainter::Submit(const TArray<FVector2f>& Points,const TArray<FLinearColor>& Colors,const TArray<uint32>& Indices)
    {
        const FSlateBrush* WhiteBrush=Art::White();
        if(Points.Num()<3||Indices.Num()<3||!Art::IsValid(WhiteBrush)||!FSlateApplication::IsInitialized())return;
        const FSlateResourceHandle Handle=FSlateApplication::Get().GetRenderer()->GetResourceHandle(*WhiteBrush);
        if(!Handle.IsValid())return;
        const FSlateRenderTransform& Transform=Geometry.GetAccumulatedRenderTransform();
        TArray<FSlateVertex> Vertices;Vertices.Reserve(Points.Num());
        for(int32 I=0;I<Points.Num();++I)Vertices.Add(FSlateVertex::Make(Transform,Points[I],FVector2f(.5f,.5f),C(Colors[I]).ToFColor(true)));
        TArray<SlateIndex> SlateIndices;SlateIndices.Reserve(Indices.Num());
        for(const uint32 I:Indices)SlateIndices.Add(SlateIndex(I));
        FSlateDrawElement::MakeCustomVerts(Out,Layer,Handle,Vertices,SlateIndices,nullptr,0,0);
    }

    void FPainter::Quad(FVector2f A,FVector2f B,FVector2f C2,FVector2f D,FLinearColor CA,FLinearColor CB,FLinearColor CC,FLinearColor CD)
    {
        Submit({A,B,C2,D},{CA,CB,CC,CD},{0,1,2,0,2,3});
    }

    void FPainter::Line(FVector2f A,FVector2f B,FLinearColor Color,float Thickness)
    {
        FSlateDrawElement::MakeLines(Out,Layer,Geometry.ToPaintGeometry(),TArray<FVector2f>{A,B},ESlateDrawEffect::None,C(Color),true,Thickness);
    }

    void FPainter::FadeLine(FVector2f A,FVector2f B,FLinearColor Color,float Thickness,float FadeFraction)
    {
        const FVector2f Along=B-A;const float Length=Along.Size();if(Length<=.5f)return;
        const FVector2f N=FVector2f(-Along.Y,Along.X)/Length*(Thickness*.5f);
        const float T[]={0.f,FadeFraction,1.f-FadeFraction,1.f};const float K[]={0.f,1.f,1.f,0.f};
        TArray<FVector2f> P;TArray<FLinearColor> Col;TArray<uint32> I;
        for(int32 S=0;S<4;++S)
        {
            const FVector2f M=A+Along*T[S];const FLinearColor Cl=WithAlpha(Color,Color.A*K[S]);
            P.Add(M-N);P.Add(M+N);Col.Add(Cl);Col.Add(Cl);
        }
        for(uint32 S=0;S<3;++S){const uint32 B0=S*2;I.Append({B0,B0+1,B0+3,B0,B0+3,B0+2});}
        Submit(P,Col,I);
    }

    void FPainter::Curve(FVector2f P0,FVector2f P1,FVector2f P2,FVector2f P3,FLinearColor Color,float Thickness)
    {
        FSlateDrawElement::MakeCubicBezierSpline(Out,Layer,Geometry.ToPaintGeometry(),P0,P1,P2,P3,Thickness,ESlateDrawEffect::None,C(Color));
    }

    void FPainter::Diamond(FVector2f Center,FVector2f HalfExtent,FLinearColor Color)
    {
        Submit({Center+FVector2f(0,-HalfExtent.Y),Center+FVector2f(HalfExtent.X,0),Center+FVector2f(0,HalfExtent.Y),Center+FVector2f(-HalfExtent.X,0)},{Color,Color,Color,Color},{0,1,2,0,2,3});
    }

    void FPainter::Glow(FVector2f Center,FVector2f Radii,FLinearColor Inner,FLinearColor Outer,int32 Segments)
    {
        // Three rings approximate a gaussian falloff instead of a hard linear cone.
        static const float Rings[]={0.f,.34f,.68f,1.f};
        static const float Weights[]={1.f,.6f,.2f,0.f};
        Segments=FMath::Clamp(Segments,8,96);
        TArray<FVector2f> P;TArray<FLinearColor> Col;TArray<uint32> I;
        P.Add(Center);Col.Add(Inner);
        for(int32 R=1;R<4;++R)for(int32 S=0;S<Segments;++S)
        {
            const FVector2f D=Dir(2.f*PI*S/Segments);
            P.Add(Center+FVector2f(D.X*Radii.X,D.Y*Radii.Y)*Rings[R]);
            Col.Add(Lerp(Outer,Inner,Weights[R]));
        }
        for(int32 S=0;S<Segments;++S)I.Append({0u,uint32(1+S),uint32(1+(S+1)%Segments)});
        for(int32 R=1;R<3;++R)for(int32 S=0;S<Segments;++S)
        {
            const uint32 A=1+(R-1)*Segments+S,B=1+(R-1)*Segments+(S+1)%Segments,A2=A+Segments,B2=B+Segments;
            I.Append({A,A2,B2,A,B2,B});
        }
        Submit(P,Col,I);
    }

    void FPainter::Halo(FVector2f Pos,FVector2f Extent,float Radius,float Spread,FLinearColor Color,bool bFillInside)
    {
        TArray<FVector2f> Edge,Normals;RoundedOutline(Pos,Extent,Radius,6,Edge,Normals);
        const int32 N=Edge.Num();
        TArray<FVector2f> P;TArray<FLinearColor> Col;TArray<uint32> I;
        const FLinearColor Mid=WithAlpha(Color,Color.A*.32f),Clear=WithAlpha(Color,0.f);
        for(int32 K=0;K<N;++K)
        {
            P.Add(Edge[K]);P.Add(Edge[K]+Normals[K]*Spread*.4f);P.Add(Edge[K]+Normals[K]*Spread);
            Col.Add(Color);Col.Add(Mid);Col.Add(Clear);
        }
        for(int32 K=0;K<N;++K)
        {
            const uint32 A=K*3,B=((K+1)%N)*3;
            I.Append({A,A+1,B+1,A,B+1,B,A+1,A+2,B+2,A+1,B+2,B+1});
        }
        if(bFillInside)
        {
            const uint32 Center=P.Num();P.Add(Pos+Extent*.5f);Col.Add(Color);
            for(int32 K=0;K<N;++K)I.Append({Center,uint32(K*3),uint32(((K+1)%N)*3)});
        }
        Submit(P,Col,I);
    }

    void FPainter::Ring(FVector2f Center,float Radius,float Thickness,FLinearColor Inner,FLinearColor OuterColor,float StartRadians,float SweepRadians,int32 Segments)
    {
        Segments=FMath::Clamp(Segments,6,180);
        TArray<FVector2f> P;TArray<FLinearColor> Col;TArray<uint32> I;
        for(int32 S=0;S<=Segments;++S)
        {
            const FVector2f D=Dir(StartRadians+SweepRadians*S/Segments);
            P.Add(Center+D*(Radius-Thickness*.5f));P.Add(Center+D*(Radius+Thickness*.5f));
            Col.Add(Inner);Col.Add(OuterColor);
        }
        for(uint32 S=0;S<uint32(Segments);++S){const uint32 A=S*2;I.Append({A,A+1,A+3,A,A+3,A+2});}
        Submit(P,Col,I);
    }

    void FPainter::Rays(FVector2f Center,float InnerRadius,float OuterRadius,int32 Count,float Rotation,float HalfWidthRadians,FLinearColor Color)
    {
        TArray<FVector2f> P;TArray<FLinearColor> Col;TArray<uint32> I;
        const FLinearColor Clear=WithAlpha(Color,0.f);
        for(int32 K=0;K<Count;++K)
        {
            const float A=Rotation+2.f*PI*K/FMath::Max(1,Count);
            const uint32 B=P.Num();
            P.Add(Center+Dir(A-HalfWidthRadians)*InnerRadius);P.Add(Center+Dir(A+HalfWidthRadians)*InnerRadius);
            P.Add(Center+Dir(A+HalfWidthRadians*1.7f)*OuterRadius);P.Add(Center+Dir(A-HalfWidthRadians*1.7f)*OuterRadius);
            Col.Append({Color,Color,Clear,Clear});
            I.Append({B,B+1,B+2,B,B+2,B+3});
        }
        Submit(P,Col,I);
    }

    void FPainter::PushClip(FVector2f Pos,FVector2f Extent)
    {
        Out.PushClip(FSlateClippingZone(Geometry.MakeChild(Extent,FSlateLayoutTransform(Pos))));
    }

    void FPainter::PopClip(){Out.PopClip();}

    void FPainter::Shine(FVector2f Pos,FVector2f Extent,float T,float BandWidth,FLinearColor Color)
    {
        if(T<=0.f||T>=1.f)return;
        PushClip(Pos,Extent);
        const float Skew=Extent.Y*.45f,Travel=Extent.X+BandWidth*2.f+Skew;
        const float X=Pos.X-BandWidth-Skew+Travel*T;
        const FLinearColor Clear=WithAlpha(Color,0.f);
        const FVector2f TL(X+Skew,Pos.Y),TM(X+Skew+BandWidth*.5f,Pos.Y),TR(X+Skew+BandWidth,Pos.Y);
        const FVector2f BL(X,Pos.Y+Extent.Y),BM(X+BandWidth*.5f,Pos.Y+Extent.Y),BR(X+BandWidth,Pos.Y+Extent.Y);
        Quad(TL,TM,BM,BL,Clear,Color,Color,Clear);
        Quad(TM,TR,BR,BM,Color,Clear,Clear,Color);
        PopClip();
    }

    void FPainter::CornerOrnament(FVector2f Corner,FVector2f D,float Scale,FLinearColor Color)
    {
        // Fret ("hui wen") hook: an outer bracket with a squared spiral and a stud, mirrored per corner.
        auto P=[&](float X,float Y){return Corner+FVector2f(D.X*X,D.Y*Y)*Scale;};
        const float T=FMath::Max(1.f,1.6f*Scale);
        FSlateDrawElement::MakeLines(Out,Layer,Geometry.ToPaintGeometry(),TArray<FVector2f>{P(0,26),P(0,0),P(26,0)},ESlateDrawEffect::None,C(Color),true,T);
        FSlateDrawElement::MakeLines(Out,Layer,Geometry.ToPaintGeometry(),TArray<FVector2f>{P(6,20),P(6,6),P(18,6),P(18,14),P(12,14)},ESlateDrawEffect::None,C(WithAlpha(Color,Color.A*.85f)),true,FMath::Max(1.f,1.2f*Scale));
        Diamond(P(32,0),FVector2f(3.2f,3.2f)*Scale,Color);
        Diamond(P(0,32),FVector2f(3.2f,3.2f)*Scale,Color);
    }

    void FPainter::Cloud(FVector2f Center,float Scale,bool bMirror,FLinearColor Color,float Thickness)
    {
        const float M=bMirror?-1.f:1.f;
        auto P=[&](float X,float Y){return Center+FVector2f(X*M,Y)*Scale;};
        // Inward spiral built from quarter arcs of shrinking radius.
        auto Spiral=[&](FVector2f C,float R,float Turns,bool bClockwise)
        {
            float A=PI,Radius=R;const int32 Quarters=FMath::RoundToInt(Turns*4.f);
            const float Step=bClockwise?HALF_PI:-HALF_PI;
            for(int32 Q=0;Q<Quarters;++Q)
            {
                const float NextR=Radius*.78f,A1=A+Step;
                const FVector2f S0=C+Dir(A)*Radius,S3=C+Dir(A1)*NextR;
                const FVector2f T0=FVector2f(-FMath::Sin(A),FMath::Cos(A))*(bClockwise?1.f:-1.f),T1=FVector2f(-FMath::Sin(A1),FMath::Cos(A1))*(bClockwise?1.f:-1.f);
                Curve(P(S0.X,S0.Y),P(S0.X+T0.X*Radius*.55f,S0.Y+T0.Y*Radius*.55f),P(S3.X-T1.X*NextR*.55f,S3.Y-T1.Y*NextR*.55f),P(S3.X,S3.Y),Color,Thickness);
                A=A1;Radius=NextR;
            }
        };
        Spiral(FVector2f(0,0),11.f,1.25f,true);
        Spiral(FVector2f(19,5),7.f,1.f,false);
        Curve(P(-11,0),P(-11,-17),P(26,-15),P(26,5),Color,Thickness);
        Curve(P(-11,0),P(-19,9),P(-28,-2),P(-40,7),Color,Thickness*.85f);
    }

    void FPainter::Divider(FVector2f Center,float Width,FLinearColor Color)
    {
        FadeLine(Center-FVector2f(Width*.5f,0),Center+FVector2f(Width*.5f,0),Color,1.4f,.3f);
        Diamond(Center,FVector2f(6.f,4.f),Color);
        Diamond(Center-FVector2f(14.f,0),FVector2f(2.5f,2.5f),WithAlpha(Color,Color.A*.8f));
        Diamond(Center+FVector2f(14.f,0),FVector2f(2.5f,2.5f),WithAlpha(Color,Color.A*.8f));
    }

    void FPainter::GoldFrame(FVector2f Pos,FVector2f Extent,float Inset,FLinearColor Color,float CornerScale,bool bDouble)
    {
        auto Rect=[&](float In,FLinearColor Cl,float Thick)
        {
            const FVector2f A=Pos+FVector2f(In,In),B=Pos+Extent-FVector2f(In,In);
            FSlateDrawElement::MakeLines(Out,Layer,Geometry.ToPaintGeometry(),TArray<FVector2f>{A,FVector2f(B.X,A.Y),B,FVector2f(A.X,B.Y),A},ESlateDrawEffect::None,C(Cl),true,Thick);
        };
        Rect(Inset,Color,1.6f);
        if(bDouble)Rect(Inset+5.f,WithAlpha(Color,Color.A*.55f),1.f);
        if(CornerScale>0.f)
        {
            const float In=Inset+2.f;
            CornerOrnament(Pos+FVector2f(In,In),FVector2f(1,1),CornerScale,Color);
            CornerOrnament(Pos+FVector2f(Extent.X-In,In),FVector2f(-1,1),CornerScale,Color);
            CornerOrnament(Pos+FVector2f(Extent.X-In,Extent.Y-In),FVector2f(-1,-1),CornerScale,Color);
            CornerOrnament(Pos+FVector2f(In,Extent.Y-In),FVector2f(1,-1),CornerScale,Color);
        }
    }

    void FPainter::Panel(FVector2f Pos,FVector2f Extent,EPanel Kind,float Radius,bool bFrame)
    {
        const auto& K=Colors();
        switch(Kind)
        {
        case EPanel::Lacquer:
            Halo(Pos,Extent,Radius,16.f,WithAlpha(K.InkBlack,.45f));
            Next();
            VGradient(Pos,Extent,{{0.f,K.LacquerHi},{.45f,K.Lacquer},{1.f,K.LacquerLo}},Radius);
            Next();
            FadeLine(Pos+FVector2f(Radius,1.5f),Pos+FVector2f(Extent.X-Radius,1.5f),WithAlpha(K.GoldBright,.35f),1.f,.2f);
            if(bFrame){Next();GoldFrame(Pos,Extent,3.f,WithAlpha(K.Gold,.9f),.9f,true);}
            break;
        case EPanel::Paper:
            Halo(Pos,Extent,Radius,12.f,WithAlpha(K.InkBlack,.3f));
            Next();
            VGradient(Pos,Extent,{{0.f,K.PaperLight},{.6f,K.Paper},{1.f,Lerp(K.Paper,K.PaperShade,.5f)}},Radius);
            if(bFrame){Next();GoldFrame(Pos,Extent,4.f,WithAlpha(K.GoldDeep,.75f),.8f,true);}
            break;
        case EPanel::Jade:
            Halo(Pos,Extent,Radius,14.f,WithAlpha(K.InkBlack,.4f));
            Next();
            VGradient(Pos,Extent,{{0.f,Hex(TEXT("1E5F55"))},{1.f,K.JadeDeep}},Radius);
            if(bFrame){Next();GoldFrame(Pos,Extent,3.f,WithAlpha(K.Gold,.85f),.8f,true);}
            break;
        case EPanel::Glass:
            Rounded(Pos,Extent,WithAlpha(K.InkBlack,.55f),Radius,WithAlpha(K.Gold,.45f),1.f);
            break;
        case EPanel::Inset:
            Rounded(Pos,Extent,WithAlpha(K.PaperShade,.38f),Radius,WithAlpha(K.PaperEdge,.45f),1.f);
            break;
        }
    }

    void FPainter::RedDot(FVector2f Center,float Radius,float Pulse)
    {
        const auto& K=Colors();
        Glow(Center,FVector2f(Radius*2.4f),WithAlpha(K.RedDot,.35f+.35f*Pulse),FLinearColor::Transparent,20);
        Next();
        Rounded(Center-FVector2f(Radius),FVector2f(Radius*2.f),K.RedDot,Radius,Hex(TEXT("FFE3B8")),1.5f);
        Next();
        Glow(Center-FVector2f(Radius*.3f,Radius*.35f),FVector2f(Radius*.45f,Radius*.3f),WithAlpha(FLinearColor::White,.55f),FLinearColor::Transparent,12);
    }

    void FPainter::Motes(FVector2f Pos,FVector2f Extent,int32 Count,double Time,FLinearColor Color,uint32 Seed,float Speed,float MaxSize)
    {
        if(Motion::IsReduced()||Count<=0)return;
        FRandomStream Random(static_cast<int32>(Seed));
        for(int32 I=0;I<Count;++I)
        {
            const float X0=Random.FRand(),Phase=Random.FRand(),Rate=FMath::Lerp(.45f,1.f,Random.FRand()),Size=MaxSize*FMath::Lerp(.35f,1.f,Random.FRand());
            const float Travel=float(FMath::Fmod(Time*Speed*Rate*.08+Phase,1.0));
            const float Life=FMath::Sin(Travel*PI);
            const FVector2f P(Pos.X+X0*Extent.X+FMath::Sin(float(Time)*.9f+Phase*6.28f)*10.f,Pos.Y+Extent.Y*(1.f-Travel));
            Glow(P,FVector2f(Size*2.2f),WithAlpha(Color,Color.A*Life*.55f),FLinearColor::Transparent,12);
            Diamond(P,FVector2f(Size*.35f),WithAlpha(FLinearColor::White,Color.A*Life*.7f));
        }
    }
}

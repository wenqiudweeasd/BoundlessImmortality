#pragma once

#include "CoreMinimal.h"
#include "InkUI/InkUITokens.h"

namespace InkUI
{
    enum class EEase : uint8 { Linear, OutQuad, OutCubic, InCubic, InOutCubic, OutQuint, OutExpo, OutBack, OutBackSoft, OutElastic, InOutSine, Smooth5 };

    BOUNDLESSIMMORTALITY_API float Ease(EEase Curve,float T);

    namespace Motion
    {
        /** Presentation-only switch: removes travel, loops and bursts but keeps short fades. */
        BOUNDLESSIMMORTALITY_API bool IsReduced();
        BOUNDLESSIMMORTALITY_API void SetReduced(bool bReduced);
        BOUNDLESSIMMORTALITY_API float Duration(float Nominal);
        BOUNDLESSIMMORTALITY_API double Now();
        /** 0..1 sine pulse for idle loops; flat when reduced motion is on. */
        BOUNDLESSIMMORTALITY_API float Pulse(double Time,float Period,float Phase=0.f);
        BOUNDLESSIMMORTALITY_API float Stagger(int32 Index,float Step=Durations::Stagger,float Max=Durations::MaxStagger);
    }

    struct BOUNDLESSIMMORTALITY_API FTween
    {
        float From=0.f,To=0.f,Duration=.3f,Delay=0.f,Elapsed=0.f;
        EEase Curve=EEase::OutCubic;
        bool bRunning=false;

        void Start(float InFrom,float InTo,float InDuration,EEase InCurve=EEase::OutCubic,float InDelay=0.f);
        /** Continues from the currently shown value, so interrupted motion never jumps. */
        void Retarget(float InTo,float InDuration,EEase InCurve=EEase::OutCubic,float InDelay=0.f);
        void Snap(float Value);
        bool Tick(float DeltaSeconds);
        float Value() const;
        float Progress() const;
    };
}

#include "InkUI/InkUIMotion.h"
#include "HAL/PlatformTime.h"

namespace InkUI
{
    float Ease(EEase Curve,float T)
    {
        T=FMath::Clamp(T,0.f,1.f);
        switch(Curve)
        {
        case EEase::OutQuad:return 1.f-(1.f-T)*(1.f-T);
        case EEase::OutCubic:return 1.f-FMath::Pow(1.f-T,3.f);
        case EEase::InCubic:return T*T*T;
        case EEase::InOutCubic:return T<.5f?4.f*T*T*T:1.f-FMath::Pow(-2.f*T+2.f,3.f)*.5f;
        case EEase::OutQuint:return 1.f-FMath::Pow(1.f-T,5.f);
        case EEase::OutExpo:return T>=1.f?1.f:1.f-FMath::Pow(2.f,-10.f*T);
        case EEase::OutBack:
        case EEase::OutBackSoft:
        {
            const float C1=Curve==EEase::OutBack?1.70158f:1.1f,C3=C1+1.f,U=T-1.f;
            return 1.f+C3*U*U*U+C1*U*U;
        }
        case EEase::OutElastic:
        {
            if(T<=0.f||T>=1.f)return T;
            return FMath::Pow(2.f,-10.f*T)*FMath::Sin((T*10.f-.75f)*(2.f*PI/3.f))+1.f;
        }
        case EEase::InOutSine:return -(FMath::Cos(PI*T)-1.f)*.5f;
        case EEase::Smooth5:return T*T*T*(T*(T*6.f-15.f)+10.f);
        default:return T;
        }
    }

    namespace Motion
    {
        namespace {bool GReduced=false;}

        bool IsReduced(){return GReduced;}
        void SetReduced(bool bReduced){GReduced=bReduced;}
        float Duration(float Nominal){return GReduced?FMath::Min(Nominal,Durations::ReducedFade):Nominal;}
        double Now(){return FPlatformTime::Seconds();}

        float Pulse(double Time,float Period,float Phase)
        {
            if(GReduced||Period<=0.f)return .5f;
            return .5f+.5f*FMath::Sin(float(FMath::Fmod(Time,double(Period))/Period)*2.f*PI+Phase);
        }

        float Stagger(int32 Index,float Step,float Max)
        {
            return GReduced?0.f:FMath::Min(FMath::Max(0,Index)*Step,Max);
        }
    }

    void FTween::Start(float InFrom,float InTo,float InDuration,EEase InCurve,float InDelay)
    {
        From=InFrom;To=InTo;Duration=FMath::Max(.001f,Motion::Duration(InDuration));Curve=InCurve;
        Delay=Motion::IsReduced()?0.f:FMath::Max(0.f,InDelay);Elapsed=0.f;bRunning=true;
    }

    void FTween::Retarget(float InTo,float InDuration,EEase InCurve,float InDelay)
    {
        if(!bRunning&&FMath::IsNearlyEqual(Value(),InTo))return;
        Start(Value(),InTo,InDuration,InCurve,InDelay);
    }

    void FTween::Snap(float InValue){From=To=InValue;Elapsed=Duration;Delay=0.f;bRunning=false;}

    bool FTween::Tick(float DeltaSeconds)
    {
        if(!bRunning)return false;
        Elapsed+=FMath::Max(0.f,DeltaSeconds);
        if(Elapsed>=Delay+Duration){Elapsed=Delay+Duration;bRunning=false;}
        return true;
    }

    float FTween::Progress() const{return FMath::Clamp((Elapsed-Delay)/FMath::Max(.001f,Duration),0.f,1.f);}

    float FTween::Value() const{return From+(To-From)*Ease(Curve,Progress());}
}

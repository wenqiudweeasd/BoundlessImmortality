#include "XianTu/XianTuBreakthroughFx.h"
#include "Combat/LiveCombat.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "GameFramework/Character.h"
#include "Components/CapsuleComponent.h"

namespace XianTuBreakthroughFx
{
    namespace
    {
        const TCHAR* MinorPath=TEXT("/Game/LiveWeapons/SpecialSwords/VFX/NS_SS_BreakMinor.NS_SS_BreakMinor");
        const TCHAR* MajorPath=TEXT("/Game/LiveWeapons/SpecialSwords/VFX/NS_SS_BreakMajor.NS_SS_BreakMajor");
        const TCHAR* AscendPath=TEXT("/Game/LiveWeapons/SpecialSwords/VFX/NS_SS_BreakAscend.NS_SS_BreakAscend");

        /** Attached at the feet and kept level, so the formation lies on the ground while the anchor sits or turns. */
        void Spawn(APawn* Anchor,const TCHAR* Path,const FVector& Feet,const FLinearColor& Color,float Radius,float Height,float Tier)
        {
            UNiagaraSystem* System=LoadObject<UNiagaraSystem>(nullptr,Path,nullptr,LOAD_NoWarn|LOAD_Quiet);
            USceneComponent* Root=Anchor?Anchor->GetRootComponent():nullptr;if(!System||!Root)return;
            UNiagaraComponent* C=UNiagaraFunctionLibrary::SpawnSystemAttached(System,Root,NAME_None,Feet,FRotator::ZeroRotator,EAttachLocation::KeepRelativeOffset,true,false,ENCPoolMethod::None,false);
            if(!C)return;
            C->SetUsingAbsoluteRotation(true);C->SetWorldRotation(FRotator::ZeroRotator);
            C->SetVariableLinearColor(TEXT("User.Color"),Color);
            C->SetVariableFloat(TEXT("User.Radius"),Radius);C->SetVariableFloat(TEXT("User.Height"),Height);C->SetVariableFloat(TEXT("User.Storm"),Tier);
            C->Activate(true);
        }
    }

    FLinearColor RealmColor(int32 Major)
    {
        static const FLinearColor Colors[]={
            FLinearColor(.42f,1.f,.72f),   // 练气 jade
            FLinearColor(.35f,.92f,1.f),   // 筑基 sky
            FLinearColor(.55f,.72f,1.f),   // 结晶 ice
            FLinearColor(1.f,.74f,.22f),   // 金丹 gold
            FLinearColor(1.f,.62f,.34f),   // 元婴 dawn
            FLinearColor(.68f,.6f,1.f),    // 化神 azure violet
            FLinearColor(.88f,.7f,1.f),    // 元神 pale violet
            FLinearColor(.45f,.6f,1.f),    // 炼虚 void blue
            FLinearColor(1.f,.84f,.42f),   // 大乘 radiant gold
            FLinearColor(1.f,.93f,.72f),   // 飞升 white gold
            FLinearColor(1.f,.97f,.9f)};   // 混元无极 white
        return Colors[FMath::Clamp(Major,0,10)];
    }

    void Play(APawn* Anchor,int32 FromRealm,int32 ToRealm,bool bCeremony)
    {
        if(!Anchor||ToRealm<=FromRealm)return;
        const int32 Major=LiveCombat::MajorRealm(ToRealm);
        const bool bMajor=Major>LiveCombat::MajorRealm(FromRealm);
        float Half=88.f;
        if(const ACharacter* Character=Cast<ACharacter>(Anchor))if(const UCapsuleComponent* Capsule=Character->GetCapsuleComponent())Half=Capsule->GetScaledCapsuleHalfHeight();
        const FVector Feet(0.f,0.f,-Half);
        const float Tier=Major/10.f;
        // Brighter with every major realm: the ink-wash materials read intensity above 1 as glow.
        const FLinearColor Color=RealmColor(Major)*(1.15f+.9f*Tier);
        if(!bMajor)
        {
            Spawn(Anchor,MinorPath,Feet,Color,95.f+9.f*Major,Half*2.f,Tier);
            if(bCeremony)Spawn(Anchor,MajorPath,Feet,Color,120.f+12.f*Major,500.f+120.f*Major,Tier*.6f);
            return;
        }
        Spawn(Anchor,MajorPath,Feet,Color,170.f+24.f*Major,800.f+260.f*Major,Tier);
        if(Major>=4||bCeremony)Spawn(Anchor,AscendPath,Feet,Color,240.f+28.f*Major,1600.f+320.f*Major,Tier);
    }
}

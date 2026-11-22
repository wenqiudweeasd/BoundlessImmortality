#include "LiveWeapons/SpecialSwords/LiveSwordVfx.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Components/SceneComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "TimerManager.h"

namespace LiveSwordVfx
{
    namespace
    {
        template<typename T> T* Load(const TCHAR* Path){return Path?LoadObject<T>(nullptr,Path,nullptr,LOAD_NoWarn|LOAD_Quiet):nullptr;}
        const TCHAR* SizeParameter(EFx Fx)
        {
            switch(Fx)
            {
            case EFx::Trail:case EFx::Beam:return TEXT("User.Width");
            case EFx::Slash:case EFx::Shatter:return TEXT("User.Size");
            case EFx::Qi:return TEXT("User.Length");
            default:return TEXT("User.Radius");
            }
        }
        bool IsLooping(EFx Fx){return Fx==EFx::Trail||Fx==EFx::Charge||Fx==EFx::Qi||Fx==EFx::Shield||Fx==EFx::Aura||Fx==EFx::Spin||Fx==EFx::Burn;}
        /** One-shot systems complete on their own; this only reclaims a component whose emitters never finish. */
        void Expire(UNiagaraComponent* C,float After)
        {
            UWorld* World=C?C->GetWorld():nullptr;if(!World)return;
            const TWeakObjectPtr<UNiagaraComponent> Weak(C);FTimerHandle Handle;
            World->GetTimerManager().SetTimer(Handle,FTimerDelegate::CreateLambda([Weak]{if(UNiagaraComponent* Alive=Weak.Get())Alive->DestroyComponent();}),After,false);
        }
    }

    FString SystemPath(EFx Fx,EStyle Style)
    {
        static const TCHAR* Names[]={TEXT("Trail"),TEXT("Slash"),TEXT("Burst"),TEXT("Ring"),TEXT("Charge"),TEXT("Qi"),TEXT("Beam"),
            TEXT("Shield"),TEXT("Aura"),TEXT("Devour"),TEXT("Spin"),TEXT("Glyph"),TEXT("Shatter"),TEXT("Pillar"),TEXT("Burn")};
        // Only what the Inorganic form plays has a futuristic twin; Spin and Glyph are its own and futuristic already.
        static const bool bTechTwin[]={true,true,true,true,false,true,false,false,true,false,false,false,true,false,false};
        static_assert(UE_ARRAY_COUNT(Names)==int32(EFx::Count)&&UE_ARRAY_COUNT(bTechTwin)==int32(EFx::Count),"Every effect needs an asset name");
        const int32 Index=FMath::Clamp(int32(Fx),0,int32(EFx::Count)-1);
        const FString Name=FString(TEXT("NS_SS_"))+Names[Index]+(Style==EStyle::Tech&&bTechTwin[Index]?TEXT("_Tech"):TEXT(""));
        return FString::Printf(TEXT("/Game/LiveWeapons/SpecialSwords/VFX/%s.%s"),*Name,*Name);
    }

    UNiagaraSystem* System(EFx Fx,EStyle Style)
    {
        UNiagaraSystem* S=Load<UNiagaraSystem>(*SystemPath(Fx,Style));
        return S||Style==EStyle::Ink?S:Load<UNiagaraSystem>(*SystemPath(Fx,EStyle::Ink));
    }

    UNiagaraSystem* ZapSystem(int32 Index)
    {
        static const TCHAR* Paths[]={
            TEXT("/Game/Vefects/Zap_VFX/VFX/Zap/Particles/NS_Zap_01_Red.NS_Zap_01_Red"),
            TEXT("/Game/Vefects/Zap_VFX/VFX/Zap/Particles/NS_Zap_02_Blue.NS_Zap_02_Blue"),
            TEXT("/Game/Vefects/Zap_VFX/VFX/Zap/Particles/NS_Zap_03_Yellow.NS_Zap_03_Yellow"),
            TEXT("/Game/Vefects/Zap_VFX/VFX/Zap/Particles/NS_Zap_04_Green.NS_Zap_04_Green"),
            TEXT("/Game/Vefects/Zap_VFX/VFX/Zap/Particles/NS_Zap_05_Purple.NS_Zap_05_Purple"),
            TEXT("/Game/Vefects/Zap_VFX/VFX/Zap/Particles/NS_Zap_06_White.NS_Zap_06_White"),
            TEXT("/Game/Vefects/Zap_VFX/VFX/Zap/Particles/NS_Zap_07_Black.NS_Zap_07_Black")};
        return Load<UNiagaraSystem>(Paths[((Index%7)+7)%7]);
    }

    FLinearColor ZapColor(int32 Index)
    {
        static const FLinearColor Colors[]={FLinearColor(1.f,.12f,.08f),FLinearColor(.15f,.45f,1.f),FLinearColor(1.f,.86f,.12f),FLinearColor(.2f,1.f,.3f),
            FLinearColor(.72f,.2f,1.f),FLinearColor(1.f,1.f,1.f),FLinearColor(.3f,.08f,.45f)};
        return Colors[((Index%7)+7)%7];
    }

    void Preload(TArray<TObjectPtr<UObject>>& Keep)
    {
        for(int32 I=0;I<int32(EFx::Count);++I)for(EStyle Style:{EStyle::Ink,EStyle::Tech})if(UObject* S=System(EFx(I),Style))Keep.AddUnique(S);
        for(int32 I=0;I<7;++I)if(UObject* S=ZapSystem(I))Keep.AddUnique(S);
        for(uint8 K=1;K<=5;++K)if(UObject* M=SwordMesh(K))Keep.AddUnique(M);
        if(UObject* Axe=GiantAxeMesh())Keep.AddUnique(Axe);
        for(EStyle Style:{EStyle::Ink,EStyle::Tech})if(UObject* Ghost=GhostMaterial(Style))Keep.AddUnique(Ghost);
    }

    void SetSize(UNiagaraComponent* C,EFx Fx,float Size)
    {
        if(!C)return;
        C->SetVariableFloat(SizeParameter(Fx),Size);
        if(Fx==EFx::Qi)C->SetVariableFloat(TEXT("User.Width"),Size*.55f);
        if(Fx==EFx::Trail)C->SetVariableFloat(TEXT("User.Life"),.45f);
    }

    UNiagaraComponent* SpawnAt(UObject* WorldContext,EFx Fx,const FVector& Location,const FLinearColor& Color,float Size,const FRotator& Rotation,EStyle Style)
    {
        UNiagaraSystem* S=System(Fx,Style);if(!S||!WorldContext)return nullptr;
        UNiagaraComponent* C=UNiagaraFunctionLibrary::SpawnSystemAtLocation(WorldContext,S,Location,Rotation,FVector(1.f),true,false,ENCPoolMethod::None,false);
        if(!C)return nullptr;
        C->SetVariableLinearColor(TEXT("User.Color"),Color);SetSize(C,Fx,Size);C->Activate(true);
        if(!IsLooping(Fx))Expire(C,4.f);
        return C;
    }

    UNiagaraComponent* SpawnAttached(USceneComponent* Parent,EFx Fx,const FLinearColor& Color,float Size,const FVector& Offset,bool bAutoDestroy,EStyle Style)
    {
        UNiagaraSystem* S=System(Fx,Style);if(!S||!Parent)return nullptr;
        UNiagaraComponent* C=UNiagaraFunctionLibrary::SpawnSystemAttached(S,Parent,NAME_None,Offset,FRotator::ZeroRotator,EAttachLocation::KeepRelativeOffset,bAutoDestroy,false,ENCPoolMethod::None,false);
        if(!C)return nullptr;
        C->SetVariableLinearColor(TEXT("User.Color"),Color);SetSize(C,Fx,Size);C->Activate(true);
        if(!IsLooping(Fx))Expire(C,4.f);
        return C;
    }

    void Beam(UObject* WorldContext,const FVector& From,const FVector& To,const FLinearColor& Color,float Width)
    {
        UNiagaraSystem* S=System(EFx::Beam);if(!S||!WorldContext)return;
        UNiagaraComponent* C=UNiagaraFunctionLibrary::SpawnSystemAtLocation(WorldContext,S,From,FRotator::ZeroRotator,FVector(1.f),true,false,ENCPoolMethod::None,false);
        if(!C)return;
        C->SetVariableLinearColor(TEXT("User.Color"),Color);C->SetVariableFloat(TEXT("User.Width"),Width);C->SetVariableVec3(TEXT("User.Target"),To-From);C->Activate(true);Expire(C,2.f);
    }

    void Devour(UObject* WorldContext,const FVector& Victim,const FVector& Sink,const FLinearColor& Color)
    {
        UNiagaraSystem* S=System(EFx::Devour);if(!S||!WorldContext)return;
        UNiagaraComponent* C=UNiagaraFunctionLibrary::SpawnSystemAtLocation(WorldContext,S,Sink,FRotator::ZeroRotator,FVector(1.f),true,false,ENCPoolMethod::None,false);
        if(!C)return;
        C->SetVariableLinearColor(TEXT("User.Color"),Color);C->SetVariableFloat(TEXT("User.Radius"),70.f);C->SetVariableVec3(TEXT("User.Source"),Victim-Sink);C->Activate(true);Expire(C,3.f);
    }

    UNiagaraComponent* DevourAttached(USceneComponent* Sink,const FVector& Victim,const FLinearColor& Color)
    {
        UNiagaraSystem* S=System(EFx::Devour);if(!S||!Sink)return nullptr;
        UNiagaraComponent* C=UNiagaraFunctionLibrary::SpawnSystemAttached(S,Sink,NAME_None,FVector::ZeroVector,FRotator::ZeroRotator,EAttachLocation::KeepRelativeOffset,true,false,ENCPoolMethod::None,false);
        if(!C)return nullptr;
        // Follows the blade's position only: the serpent and the essence stay level however the sword is turned.
        C->SetUsingAbsoluteRotation(true);C->SetWorldRotation(FRotator::ZeroRotator);
        // The head sprite's crown side runs along strike × view direction, which points down for strikes toward screen
        // right: those are mirrored so the serpent never strikes upside down.
        float Flip=1.f;
        if(const UWorld* World=Sink->GetWorld())if(const APlayerController* PC=World->GetFirstPlayerController();PC&&PC->PlayerCameraManager)
        {
            const FVector Right=FRotationMatrix(PC->PlayerCameraManager->GetCameraRotation()).GetUnitAxis(EAxis::Y);
            Flip=FVector::DotProduct(Victim-C->GetComponentLocation(),Right)>0.0?-1.f:1.f;
        }
        C->SetVariableFloat(TEXT("User.Flip"),Flip);
        C->SetVariableLinearColor(TEXT("User.Color"),Color);C->SetVariableFloat(TEXT("User.Radius"),70.f);
        C->SetVariableVec3(TEXT("User.Source"),Victim-C->GetComponentLocation());C->Activate(true);Expire(C,3.f);
        return C;
    }

    void Zap(UObject* WorldContext,int32 Index,const FVector& Location,float Scale)
    {
        UNiagaraSystem* S=ZapSystem(Index);if(!S||!WorldContext)return;
        UNiagaraFunctionLibrary::SpawnSystemAtLocation(WorldContext,S,Location,FRotator::ZeroRotator,FVector(Scale),true,true,ENCPoolMethod::None,false);
    }

    UNiagaraComponent* BurnAttached(USceneComponent* Parent,const FVector& Offset,const FLinearColor& Color,float Radius,float Height)
    {
        UNiagaraSystem* S=System(EFx::Burn);if(!S||!Parent)return nullptr;
        UNiagaraComponent* C=UNiagaraFunctionLibrary::SpawnSystemAttached(S,Parent,NAME_None,Offset,FRotator::ZeroRotator,EAttachLocation::KeepRelativeOffset,true,false,ENCPoolMethod::None,false);
        if(!C)return nullptr;
        // Flames rise along world up however the enemy turns or is scaled.
        C->SetUsingAbsoluteRotation(true);C->SetUsingAbsoluteScale(true);C->SetWorldRotation(FRotator::ZeroRotator);C->SetWorldScale3D(FVector(1.f));
        C->SetVariableLinearColor(TEXT("User.Color"),Color);C->SetVariableFloat(TEXT("User.Radius"),Radius);C->SetVariableFloat(TEXT("User.Height"),Height);
        C->Activate(true);
        return C;
    }

    UNiagaraComponent* ZapAttached(USceneComponent* Parent,int32 Index,const FVector& Offset,float Scale)
    {
        UNiagaraSystem* S=ZapSystem(Index);if(!S||!Parent)return nullptr;
        UNiagaraComponent* C=UNiagaraFunctionLibrary::SpawnSystemAttached(S,Parent,NAME_None,Offset,FRotator::ZeroRotator,EAttachLocation::KeepRelativeOffset,true,false,ENCPoolMethod::None,false);
        if(!C)return nullptr;
        C->SetUsingAbsoluteRotation(true);C->SetUsingAbsoluteScale(true);C->SetWorldRotation(FRotator::ZeroRotator);C->SetWorldScale3D(FVector(Scale));
        C->Activate(true);
        return C;
    }

    UStaticMesh* SwordMesh(uint8 Kind)
    {
        // Indexed by ELiveSpecialSword. The Inorganic form uses the Fab skeletal mesh instead.
        static const TCHAR* Paths[]={nullptr,
            TEXT("/Game/LiveWeapons/SpecialSwords/Meshes/SM_SoulEater.SM_SoulEater"),
            nullptr,
            TEXT("/Game/LiveWeapons/SpecialSwords/Meshes/SM_DivineBlade.SM_DivineBlade"),
            TEXT("/Game/LiveWeapons/SpecialSwords/Meshes/SM_SevenStar.SM_SevenStar"),
            TEXT("/Game/LiveWeapons/SpecialSwords/Meshes/SM_Godslayer.SM_Godslayer")};
        return Kind<UE_ARRAY_COUNT(Paths)?Load<UStaticMesh>(Paths[Kind]):nullptr;
    }

    UStaticMesh* GiantAxeMesh(){return Load<UStaticMesh>(TEXT("/Game/LiveWeapons/SpecialSwords/Meshes/SM_GiantAxe.SM_GiantAxe"));}
    UMaterialInterface* GhostMaterial(EStyle Style)
    {
        return Load<UMaterialInterface>(Style==EStyle::Ink?TEXT("/Game/LiveWeapons/SpecialSwords/Materials/M_SS_GhostInk.M_SS_GhostInk")
            :TEXT("/Game/LiveWeapons/SpecialSwords/Materials/M_SS_Ghost.M_SS_Ghost"));
    }
}

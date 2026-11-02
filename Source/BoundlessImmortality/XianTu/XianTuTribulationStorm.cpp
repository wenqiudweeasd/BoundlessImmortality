#include "XianTu/XianTuTribulationStorm.h"
#include "XianTu/XianTuSubsystem.h"
#include "XianTu/XianTuRules.h"
#include "Cultivation/CultivationSubsystem.h"
#include "Cultivation/CultivationHealthComponent.h"
#include "LiveWeapons/SpecialSwords/LiveSwordVfx.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Components/PointLightComponent.h"
#include "Components/CapsuleComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Camera/CameraShakeBase.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Engine/DirectionalLight.h"
#include "Components/LightComponent.h"
#include "EngineUtils.h"

namespace
{
    const TCHAR* StormCloudPath=TEXT("/Game/LiveWeapons/SpecialSwords/VFX/NS_SS_TribCloud.NS_SS_TribCloud");
    const TCHAR* StormBoltPath=TEXT("/Game/LiveWeapons/SpecialSwords/VFX/NS_SS_TribBolt.NS_SS_TribBolt");
    const TCHAR* StormHitPath=TEXT("/Game/LiveWeapons/SpecialSwords/VFX/NS_SS_TribHit.NS_SS_TribHit");
    // Vefects zap pack: a slight camera shake and two big crack sounds for the strikes.
    const TCHAR* StormShakePath=TEXT("/Game/Vefects/Zap_VFX/Demo/Shared/Cam/Cam_Shake_Slight.Cam_Shake_Slight_C");
    const TCHAR* StormCrackPaths[]={TEXT("/Game/Vefects/Zap_VFX/Audio/SFX_Vefects_Zap_Big_01_Cue.SFX_Vefects_Zap_Big_01_Cue"),
        TEXT("/Game/Vefects/Zap_VFX/Audio/SFX_Vefects_Zap_Big_02_Cue.SFX_Vefects_Zap_Big_02_Cue")};
    template<typename T> T* LoadQuiet(const TCHAR* Path){return LoadObject<T>(nullptr,Path,nullptr,LOAD_NoWarn|LOAD_Quiet);}
    /** Cloud height above the strike point: the telephoto range camera sees little sky, the orthographic one more. */
    constexpr float StormHeightPerspective=620.f,StormHeightOrtho=900.f;
    /** Where the bolt meets the anchor above its feet; NS_SS_TribBolt draws the ring and scorch this far below. */
    constexpr float StormStrikeHeight=58.f;
}

AXianTuTribulationStorm::AXianTuTribulationStorm()
{
    PrimaryActorTick.bCanEverTick=true;PrimaryActorTick.TickGroup=TG_PostUpdateWork;
    Root=CreateDefaultSubobject<USceneComponent>(TEXT("Root"));SetRootComponent(Root);
    Cloud=CreateDefaultSubobject<UNiagaraComponent>(TEXT("Cloud"));Cloud->SetupAttachment(Root);Cloud->bAutoActivate=false;
    auto MakeLight=[this](const TCHAR* Name,float Radius)
    {
        UPointLightComponent* L=CreateDefaultSubobject<UPointLightComponent>(Name);L->SetupAttachment(Root);L->SetUsingAbsoluteLocation(true);
        L->SetIntensityUnits(ELightUnits::Candelas);L->SetIntensity(0.f);L->SetAttenuationRadius(Radius);L->SetCastShadows(false);
        L->SetLightColor(FLinearColor(.62f,.72f,1.f));return L;
    };
    ImpactLight=MakeLight(TEXT("ImpactLight"),2600.f);
    CloudLight=MakeLight(TEXT("CloudLight"),3600.f);
    SetActorEnableCollision(false);
}

void AXianTuTribulationStorm::BeginPlay()
{
    Super::BeginPlay();
    Random.Initialize(int32(GetUniqueID()*2654435761u));
    for(TActorIterator<ADirectionalLight> It(GetWorld());It;++It)if(const ULightComponent* Sun=It->GetLightComponent())SunLux=FMath::Max(SunLux,Sun->Intensity);
    if(UNiagaraSystem* System=LoadQuiet<UNiagaraSystem>(StormCloudPath))
    {
        Cloud->SetAsset(System);
        Cloud->SetVariableLinearColor(TEXT("User.Color"),FLinearColor::White);Cloud->SetVariableFloat(TEXT("User.Density"),0.f);
        Cloud->Activate(true);
    }
}

FXianTuTribulationPressure AXianTuTribulationStorm::Pressure(int32 Probability,int32 Phase,float BaseDamage)
{
    FXianTuTribulationPressure P;
    P.Intensity=FMath::Clamp((10000-Probability)/5000.f,0.f,1.f);
    // Later trials strike faster: 肉身 ×1.15, 神魂 ×1, 心魔 ×0.85.
    static constexpr float PhaseFactor[]={1.15f,1.f,.85f};
    P.Interval=FMath::Lerp(15.f,7.5f,P.Intensity)*PhaseFactor[FMath::Clamp(Phase,0,2)];
    P.Bolts=FMath::Clamp(1+FMath::FloorToInt(P.Intensity*2.f+.25f),1,3);
    P.DamageFraction=FMath::Max(0.f,BaseDamage)*(1.f+.5f*P.Intensity);
    return P;
}

float AXianTuTribulationStorm::BacklashFraction(int32 Strike,float Intensity)
{
    constexpr float Growth=1.06f;
    const int32 N=XianTuRules::TribBacklashStrikes;
    const float Sum=(FMath::Pow(Growth,float(N))-1.f)/(Growth-1.f);
    const float Total=FMath::Lerp(1.f,2.f,FMath::Clamp(Intensity,0.f,1.f));
    return Total/Sum*FMath::Pow(Growth,float(FMath::Clamp(Strike,0,N-1)));
}

float AXianTuTribulationStorm::GetNextStrikeIn()const
{
    const UXianTuSubsystem* X=XianTu.Get();if(!X)return -1.f;
    const FXianTuTribulation& T=X->GetState()->Trib;
    if(T.State==EXianTuTribState::Backlash)return float(FMath::Max(0.0,BacklashFired*XianTuRules::TribBacklashInterval-T.Elapsed));
    return T.State==EXianTuTribState::Trial?float(FMath::Max(0.0,NextStrikeAt-T.Elapsed)):-1.f;
}

UCultivationHealthComponent* AXianTuTribulationStorm::AnchorHealth()const
{
    const UGameInstance* GI=GetGameInstance();const UCultivationSubsystem* C=GI?GI->GetSubsystem<UCultivationSubsystem>():nullptr;
    UCultivationHealthComponent* H=C?C->GetAnchorHealth():nullptr;
    return H&&H->GetWorld()==GetWorld()?H:nullptr;
}

APawn* AXianTuTribulationStorm::AnchorPawn()const
{
    const UCultivationHealthComponent* H=AnchorHealth();return H?Cast<APawn>(H->GetOwner()):nullptr;
}

FVector AXianTuTribulationStorm::StrikePoint(const APawn* Pawn)const
{
    const ACharacter* Character=Cast<ACharacter>(Pawn);
    const float Capsule=Character&&Character->GetCapsuleComponent()?Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight():88.f;
    return Pawn->GetActorLocation()-FVector(0,0,Capsule-StormStrikeHeight);
}

void AXianTuTribulationStorm::UpdateFrame(const APawn* Pawn,float DeltaSeconds)
{
    const APlayerController* PC=GetWorld()->GetFirstPlayerController();
    const APlayerCameraManager* Camera=PC?PC->PlayerCameraManager.Get():nullptr;
    FMinimalViewInfo View;
    if(Camera)View=Camera->GetCameraCacheView();
    else{View.Location=Pawn->GetActorLocation()+FVector(-1200,0,1800);View.Rotation=FRotator(-56.3f,0,0);View.FOV=60.f;}
    const bool bOrtho=View.ProjectionMode==ECameraProjectionMode::Orthographic;
    ViewYaw=View.Rotation.Yaw;ViewPitch=FMath::Clamp(-View.Rotation.Pitch,15.f,85.f);
    // The eye sits on the sight line to the strike point, so the anchor is seen through it.
    const FVector Forward=FRotator(0,ViewYaw,0).Vector();
    const float Height=bOrtho?StormHeightOrtho:StormHeightPerspective;
    const FVector Want=StrikePoint(Pawn)+FVector(0,0,Height)-Forward*(Height/FMath::Tan(FMath::DegreesToRadians(ViewPitch)));
    const bool bSnap=EyeCentre.IsZero();
    EyeCentre=bSnap?Want:FMath::VInterpTo(EyeCentre,Want,DeltaSeconds,4.f);
    int32 SizeX=16,SizeY=9;if(PC)PC->GetViewportSize(SizeX,SizeY);
    const float Aspect=SizeY>0&&SizeX>0?float(SizeX)/float(SizeY):16.f/9.f;
    float Half=0.f;
    if(bOrtho)Half=View.OrthoWidth*.5f;
    else
    {
        const float Tan=FMath::Tan(FMath::DegreesToRadians(FMath::Clamp(View.FOV,5.f,170.f)*.5f));
        const float Extent=FVector::Dist(View.Location,EyeCentre)*Tan;
        // A camera that keeps its vertical field of view shows Extent up and down instead of sideways.
        Half=View.AspectRatioAxisConstraint.Get(AspectRatio_MaintainXFOV)==AspectRatio_MaintainYFOV?Extent*Aspect:Extent;
    }
    Half=FMath::Clamp(Half,200.f,8000.f);
    HalfWidth=bSnap?Half:FMath::FInterpTo(HalfWidth,Half,DeltaSeconds,3.f);HalfHeight=HalfWidth/Aspect;
    // Fit the vortex to the screen in portrait and landscape alike: the eye clears the middle (half the width, a third
    // of the height above and below the anchor), the far side reaches past the top edge and the flanks past the sides.
    const float SinPitch=FMath::Max(.2f,FMath::Sin(FMath::DegreesToRadians(ViewPitch)));
    const float EyeWorld=FMath::Max(.5f*HalfWidth,.3f*HalfHeight/SinPitch);
    CloudRadius=FMath::Max(1.15f*HalfHeight/SinPitch,1.6f*HalfWidth)/1.7f;EyeFit=FMath::Clamp(EyeWorld/CloudRadius,.2f,.95f);
    SetActorLocationAndRotation(EyeCentre,FRotator(0,ViewYaw,0));
}

void AXianTuTribulationStorm::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    const UXianTuSubsystem* X=XianTu.Get();
    const FXianTuTribulation* T=X?&X->GetState()->Trib:nullptr;
    const EXianTuTribState S=T?T->State:EXianTuTribState::None;
    APawn* Pawn=AnchorPawn();
    if(T&&(S==EXianTuTribState::Prepare||S==EXianTuTribState::Trial)&&T->AttemptId!=AttemptId)
    {
        AttemptId=T->AttemptId;Strikes=BoltsFired=BoltsLanded=BacklashFired=0;DamageDealt=LastBoltDamage=BacklashDamage=0;bFinale=false;NextStrikeAt=2.0;Volley.Reset();
    }
    const int32 Phase=T?FMath::Clamp(int32(T->Elapsed/(XianTuRules::TribTrial/3.0)),0,2):1;
    Current=Pressure(T?T->Probability:10000,Phase,DamageFraction);

    // Gather through the preparation, rage through the trials, part (success) or fade (failure, cancel) afterwards.
    // The eye opens wide while the clouds gather, narrows a little more under pressure and parts on success.
    float WantDensity=0.f,EyeScale=2.4f,WantStorm=0.f,FadeRate=.8f;
    switch(S)
    {
    case EXianTuTribState::Prepare:
        WantDensity=FMath::Clamp(float(T->Elapsed)/12.f,0.f,1.f);EyeScale=FMath::Lerp(1.9f,1.f,WantDensity);WantStorm=.4f*Current.Intensity;FadeRate=.35f;break;
    case EXianTuTribState::Trial:case EXianTuTribState::Frozen:
        WantDensity=1.f;EyeScale=1.f-.1f*Current.Intensity;WantStorm=Current.Intensity;FadeRate=.5f;break;
    // The eye closes in over the anchor and the clouds churn at full fury while the backlash lasts.
    case EXianTuTribState::Backlash:WantDensity=1.f;EyeScale=.7f;WantStorm=1.f;FadeRate=.8f;break;
    case EXianTuTribState::Succeeded:EyeScale=4.5f;FadeRate=.25f;if(!bFinale)Finale(true);break;
    case EXianTuTribState::Failed:EyeScale=2.f;FadeRate=.3f;if(!bFinale)Finale(false);break;
    default:break;
    }
    const float WantEye=FMath::Min(3.f,EyeFit*EyeScale);
    Density=FMath::FInterpConstantTo(Density,WantDensity,DeltaSeconds,FadeRate);
    Eye=FMath::FInterpTo(Eye,WantEye,DeltaSeconds,1.2f);StormLevel=FMath::FInterpTo(StormLevel,WantStorm,DeltaSeconds,2.f);

    // Strikes are timed on the trial's own clock, so a faster local time scale keeps the same count per trial.
    if(S==EXianTuTribState::Trial&&Pawn&&Volley.Num()==0&&T->Elapsed>=NextStrikeAt)
    {
        BeginVolley(Current.Bolts,true,1.f);
        NextStrikeAt=T->Elapsed+Current.Interval*Random.FRandRange(.85f,1.15f);
    }
    if(S==EXianTuTribState::Backlash&&Pawn)
    {
        // A trial volley still charging gives way to the backlash: a bolt every interval on its clock, the first at once,
        // each harder and wider than the last, and allowed to kill.
        if(LastState!=EXianTuTribState::Backlash){Volley.Reset();Charge=1.f;}
        const int32 Last=XianTuRules::TribBacklashStrikes;
        const int32 Due=FMath::Min(Last,FMath::FloorToInt(float(T->Elapsed/XianTuRules::TribBacklashInterval))+1);
        while(BacklashFired<Due)
        {
            const UCultivationHealthComponent* H=AnchorHealth();if(!H||H->bDead)break;
            FBolt B;B.bBacklash=true;B.Floor=0.f;B.Fraction=BacklashFraction(BacklashFired,Current.Intensity);
            B.Scale=1.f+1.4f*float(BacklashFired)/float(Last-1);
            ++BacklashFired;FireBolt(B);
        }
    }
    LastState=S;
    // The cloud brightens while a volley charges; each bolt fires after its delay.
    Charge=Volley.Num()?FMath::Min(1.f,Charge+DeltaSeconds*2.5f):FMath::Max(0.f,Charge-DeltaSeconds*3.f);
    for(int32 I=0;I<Volley.Num();)
    {
        if((Volley[I].Delay-=DeltaSeconds)>0.f){++I;continue;}
        const FBolt Bolt=Volley[I];Volley.RemoveAt(I);FireBolt(Bolt);
    }
    Flash=FMath::Max(Charge*.25f,Flash*FMath::Exp(-DeltaSeconds*7.f));
    ImpactGlow*=FMath::Exp(-DeltaSeconds*13.f);CloudGlow*=FMath::Exp(-DeltaSeconds*8.f);
    const float Flicker=.65f+.35f*FMath::Sin(GetWorld()->GetTimeSeconds()*97.f);
    // About eight suns on the ground right under the strike, fading to one at three metres; the cloud a faint blue fill.
    ImpactLight->SetIntensity(ImpactGlow*Flicker*SunLux*8.f);
    CloudLight->SetIntensity((CloudGlow*Flicker+Charge*.12f)*SunLux*20.f*Density);

    if(Pawn)UpdateFrame(Pawn,DeltaSeconds);
    if(Cloud->GetAsset())
    {
        Cloud->SetVariableFloat(TEXT("User.Radius"),CloudRadius);Cloud->SetVariableFloat(TEXT("User.Eye"),Eye);
        Cloud->SetVariableFloat(TEXT("User.Density"),Density);Cloud->SetVariableFloat(TEXT("User.Flash"),FMath::Min(1.f,Flash));
        Cloud->SetVariableFloat(TEXT("User.Storm"),StormLevel);
    }
    const bool bLive=S==EXianTuTribState::Prepare||S==EXianTuTribState::Trial||S==EXianTuTribState::Frozen||S==EXianTuTribState::Backlash;
    if(!bLive&&!Volley.Num()&&Density<=.005f&&ImpactGlow<.01f)Destroy();
}

void AXianTuTribulationStorm::StrikeNow(){if(AnchorPawn()&&!Volley.Num())BeginVolley(Current.Bolts,true,1.f);}

void AXianTuTribulationStorm::BeginVolley(int32 Bolts,bool bDamage,float Scale)
{
    ++Strikes;
    // A short charge in the cloud, then the bolts a beat apart.
    for(int32 I=0;I<FMath::Max(1,Bolts);++I){FBolt B;B.Delay=.45f+.18f*I;B.bDamage=bDamage;B.Scale=Scale;Volley.Add(B);}
}

void AXianTuTribulationStorm::FireBolt(const FBolt& Bolt)
{
    APawn* Pawn=AnchorPawn();if(!Pawn)return;
    const FVector Target=StrikePoint(Pawn);
    // Born high in the far side of the cloud, so on screen the bolt runs from near the top edge down to the anchor.
    const float Theta=FMath::DegreesToRadians(Random.FRandRange(-55.f,55.f));
    const float SinPitch=FMath::Max(.2f,FMath::Sin(FMath::DegreesToRadians(ViewPitch)));
    float Rho=Random.FRandRange(.62f,.9f)*HalfHeight/(FMath::Max(.35f,FMath::Cos(Theta))*SinPitch);
    Rho=FMath::Min3(Rho,.92f*HalfWidth/FMath::Max(.05f,FMath::Abs(FMath::Sin(Theta))),1.6f*CloudRadius);
    // Out of the eye and into the cloud mass.
    Rho=FMath::Max(Rho,1.15f*Eye*CloudRadius);
    const FVector Origin=EyeCentre+FRotator(0,ViewYaw,0).RotateVector(FVector(FMath::Cos(Theta),FMath::Sin(Theta),0.f)*Rho)+FVector(0,0,Random.FRandRange(-30.f,40.f));
    const float Width=9.f*Bolt.Scale*(1.f+.5f*Current.Intensity);
    if(UNiagaraSystem* System=LoadQuiet<UNiagaraSystem>(StormBoltPath))
        if(UNiagaraComponent* C=UNiagaraFunctionLibrary::SpawnSystemAtLocation(this,System,Origin,FRotator::ZeroRotator,FVector(1.f),true,false,ENCPoolMethod::None,false))
        {
            C->SetVariableLinearColor(TEXT("User.Color"),FLinearColor::White);C->SetVariableFloat(TEXT("User.Width"),Width);
            C->SetVariableVec3(TEXT("User.Target"),Target-Origin);
            // Translucency sorts whole objects: born deep in the far cloud, the bolt would draw under the nearer clouds.
            C->SetTranslucentSortPriority(10);C->Activate(true);
        }
    ++BoltsFired;Flash=1.f;ImpactGlow=1.f;CloudGlow=1.f;
    ImpactLight->SetWorldLocation(Target+FVector(0,0,80));CloudLight->SetWorldLocation(Origin);
    // Backlash bolts come every 0.2 s: the shake and crack grow with them, and only every other early bolt cracks aloud.
    const float Ramp=Bolt.bBacklash?float(BacklashFired)/float(XianTuRules::TribBacklashStrikes):0.f;
    // What the strike leaves (flash, scorch, ground arcs, sparks, the Vefects zap) rides on the anchor's feet, so it stays
    // under the anchor. The zap was spawned at waist height, and its floor scorch hung in the air beside the anchor.
    if(USceneComponent* AnchorRoot=Pawn->GetRootComponent())
    {
        const FVector Ground=Target-FVector(0,0,StormStrikeHeight);
        const FVector Feet=AnchorRoot->GetComponentTransform().InverseTransformPosition(Ground);
        // Body arcs climb as high as the anchor stands: the capsule's top, or twice the actor's height above its feet.
        const ACharacter* Character=Cast<ACharacter>(Pawn);
        const float Height=Character&&Character->GetCapsuleComponent()?2.f*Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight():2.f*float(Pawn->GetActorLocation().Z-Ground.Z);
        if(UNiagaraSystem* System=LoadQuiet<UNiagaraSystem>(StormHitPath))
            if(UNiagaraComponent* C=UNiagaraFunctionLibrary::SpawnSystemAttached(System,AnchorRoot,NAME_None,Feet,FRotator::ZeroRotator,EAttachLocation::KeepRelativeOffset,true,false,ENCPoolMethod::None,false))
            {
                C->SetUsingAbsoluteRotation(true);C->SetUsingAbsoluteScale(true);C->SetWorldRotation(FRotator::ZeroRotator);C->SetWorldScale3D(FVector(1.f));
                C->SetVariableLinearColor(TEXT("User.Color"),FLinearColor::White);C->SetVariableFloat(TEXT("User.Width"),Width);
                C->SetVariableFloat(TEXT("User.Height"),FMath::Clamp(Height,80.f,400.f));
                C->SetTranslucentSortPriority(9);C->Activate(true);
            }
        LiveSwordVfx::ZapAttached(AnchorRoot,1,Feet,Bolt.bBacklash?.7f+.7f*Ramp:.8f+.4f*Current.Intensity*Bolt.Scale);
    }
    if(APlayerController* PC=GetWorld()->GetFirstPlayerController())
        if(UClass* Shake=LoadQuiet<UClass>(StormShakePath);Shake&&Shake->IsChildOf(UCameraShakeBase::StaticClass()))
            PC->ClientStartCameraShake(Shake,Bolt.bBacklash?.35f+.75f*Ramp:.6f+.6f*Current.Intensity*Bolt.Scale);
    if(!Bolt.bBacklash||BacklashFired%2==1||Ramp>.7f)
        if(USoundBase* Crack=LoadQuiet<USoundBase>(StormCrackPaths[Random.RandRange(0,1)]))UGameplayStatics::PlaySound2D(this,Crack,Bolt.bBacklash?.4f+.4f*Ramp:.55f+.25f*Current.Intensity);
    if(!Bolt.bDamage)return;
    if(UCultivationHealthComponent* Health=AnchorHealth();Health&&!Health->bDead)
    {
        LastBoltDamage=Health->MaxHealth*(Bolt.bBacklash?Bolt.Fraction:Current.DamageFraction);
        // Trial bolts never fell the anchor. In the backlash only a held 七星挡劫 stands between it and death: the guard keeps
        // its shield and star healing and the bolts stop at the trial's floor; broken or absent, the heavens pass over both.
        const bool bWarded=!Bolt.bBacklash||(XianTu.IsValid()&&XianTu->GetStarWard()==EXianTuWard::Held);
        const double Applied=Health->ApplyUnavoidableDamage(LastBoltDamage,bWarded?MinHealthFraction:Bolt.Floor,bWarded);
        DamageDealt+=Applied;++BoltsLanded;
        if(Bolt.bBacklash){BacklashDamage+=Applied;if(UXianTuSubsystem* X=XianTu.Get())X->NoteBacklashStrike(Applied);}
    }
}

void AXianTuTribulationStorm::Finale(bool bSucceeded)
{
    bFinale=true;Volley.Reset();
    // After a backlash the failure has already been written in lightning; the clouds only fade.
    if(!bSucceeded&&BacklashFired>0)return;
    const APawn* Pawn=AnchorPawn();if(!Pawn)return;
    const FVector Feet=StrikePoint(Pawn)-FVector(0,0,StormStrikeHeight);
    if(bSucceeded)
    {
        // The clouds part over a pillar of golden light.
        const FLinearColor Gold(1.f,.78f,.32f);
        LiveSwordVfx::SpawnAt(this,LiveSwordVfx::EFx::Pillar,Feet,Gold*2.f,170.f);
        CloudLight->SetLightColor(Gold);CloudLight->SetWorldLocation(Feet+FVector(0,0,400));CloudGlow=1.5f;
    }
    else
    {
        // The heavens close with one last bolt that only marks the failure.
        FBolt Last;Last.Delay=.3f;Last.bDamage=false;Last.Scale=1.6f;Volley.Add(Last);
    }
}

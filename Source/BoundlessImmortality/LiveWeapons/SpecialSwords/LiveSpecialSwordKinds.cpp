#include "LiveWeapons/SpecialSwords/LiveSpecialSwordKinds.h"
#include "LiveWeapons/SpecialSwords/LiveSpecialSwordComponent.h"
#include "LiveWeapons/SpecialSwords/LiveAnchorBuffSubsystem.h"
#include "LiveWeapons/SpecialSwords/LiveSwordProjectile.h"
#include "LiveWeapons/SpecialSwords/LiveSwordCodex.h"
#include "LiveWeapons/LiveMonster.h"
#include "XianTu/XianTuSubsystem.h"
#include "Cultivation/CultivationSubsystem.h"
#include "Cultivation/CultivationHealthComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SphereComponent.h"
#include "NiagaraComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/AnimSequence.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"

#define LOCTEXT_NAMESPACE "LiveSpecialSwordKinds"

using namespace LiveSwordVfx;

namespace
{
    UCultivationHealthComponent* AnchorHealthOf(const AActor* Actor)
    {
        const UGameInstance* GI=Actor?Actor->GetGameInstance():nullptr;
        const UCultivationSubsystem* C=GI?GI->GetSubsystem<UCultivationSubsystem>():nullptr;
        return C?C->GetAnchorHealth():nullptr;
    }
    bool Crossed(const FVector& Goal,const FVector& Before,const FVector& After,float Radius){return FMath::PointDistToSegment(Goal,Before,After)<Radius;}
}

// ---------------------------------------------------------------- 噬魂剑

void ALiveSwordSoulEater::SetupForm(){SetupBlade(SwordMesh(uint8(Kind)),130.f);}

float ALiveSwordSoulEater::GrowthAttack()const
{
    const ULiveSpecialSwordComponent* M=Manager.Get();return M?float(M->GetGrowth(ParticipantId).DevouredAttack):0.f;
}

int32 ALiveSwordSoulEater::CloneCount()const
{
    const float Unit=FMath::Max(1.f,BaseAttackPower()*.5f);
    return FMath::Clamp(FMath::FloorToInt(AttackPower()/Unit),5,20);
}

bool ALiveSwordSoulEater::CanCast(FName Skill)const
{
    return NearestEnemy(AnchorCenter(),Skill==TEXT("GroundWave")?950.f:EngageRange())!=nullptr;
}

void ALiveSwordSoulEater::BeginCast(FName Skill)
{
    Phase=0;
    if(Skill==TEXT("Clones"))
    {
        TArray<ALiveMonster*> Targets;Enemies(AnchorCenter(),1600.f,Targets);
        const int32 Count=CloneCount();
        PlayFx(EFx::Burst,GetActorLocation(),Tint(),170.f);
        for(int32 I=0;I<Count;++I)
        {
            const float A=2*PI*I/Count;
            const FVector Out(FMath::Cos(A),FMath::Sin(A),.35f*FMath::Sin(3*A));
            const int32 G=SpawnGhost(GetActorLocation()+Out*Random.FRandRange(50.f,90.f),.62f);
            if(G==INDEX_NONE)continue;
            Ghosts[G].Velocity=Out*Random.FRandRange(700.f,1100.f);Ghosts[G].Timer=Random.FRandRange(.12f,.3f);
            Ghosts[G].Target=Targets.Num()?Targets[I%Targets.Num()]:nullptr;
        }
        DashTarget=nullptr;
        PushFloater(FString::Printf(TEXT("万影分身 ×%d"),Count),Tint(),GetActorLocation()+FVector(0,0,90));
        return;
    }
    const ALiveMonster* T=NearestEnemy(AnchorCenter(),950.f);
    const FVector Dir=T?(T->GetActorLocation()-AnchorCenter()).GetSafeNormal2D():FVector::ForwardVector;
    Spot=AnchorCenter()+Dir*FMath::Clamp(T?float(FVector::Dist2D(T->GetActorLocation(),AnchorCenter()))*.5f:250.f,180.f,420.f);
    Spot.Z=FloorZ();
}

void ALiveSwordSoulEater::TickCast(float DeltaSeconds)
{
    if(CastingSkill==TEXT("Clones"))TickClones(DeltaSeconds);else TickGroundWave(DeltaSeconds);
}

void ALiveSwordSoulEater::TickClones(float DeltaSeconds)
{
    // The body keeps hunting with no attack interval and never returns while any clone is alive.
    ALiveMonster* T=DashTarget.Get();
    if(!T||!T->IsAlive()){T=NearestEnemy(GetActorLocation(),1700.f);DashTarget=T;}
    if(T)
    {
        const FVector Goal=HitPoint(T),Before=GetActorLocation();
        Steer(Goal,DeltaSeconds,3100.f,15.f);Face(Velocity,DeltaSeconds,24.f);
        if(Crossed(Goal,Before,GetActorLocation(),75.f))
        {
            DealDamage(T,AttackPower(),TEXT("Attack"),Goal);
            PlayFx(EFx::Slash,Goal,Tint(),120.f,Velocity.Rotation());
            DashTarget=NearestEnemy(GetActorLocation(),1700.f,T);Velocity*=.3f;
        }
    }
    else{Steer(IdlePoint(),DeltaSeconds,1600.f,5.f);Face(FVector::UpVector,DeltaSeconds,6.f);}
    for(int32 I=Ghosts.Num()-1;I>=0;--I)
    {
        FGhost& G=Ghosts[I];G.Age+=DeltaSeconds;
        if(G.Timer>0)
        {
            G.Timer-=DeltaSeconds;G.Position+=G.Velocity*DeltaSeconds;G.Velocity*=FMath::Exp(-5.f*DeltaSeconds);
            G.Facing=FMath::QInterpTo(G.Facing,FRotationMatrix::MakeFromZ((G.Velocity+FVector(0,0,200)).GetSafeNormal()).ToQuat(),DeltaSeconds,10.f);
            continue;
        }
        ALiveMonster* GT=G.Target.Get();
        if(!GT||!GT->IsAlive()){GT=NearestEnemy(G.Position,1700.f);G.Target=GT;}
        if(!GT||G.Age>4.5f){ReleaseGhost(I,true);continue;}
        const FVector Goal=HitPoint(GT),Before=G.Position;
        SteerGhost(G,Goal,DeltaSeconds,2700.f,12.f);
        if(Crossed(Goal,Before,G.Position,70.f))
        {
            // Clones inherit half the body's attack and shatter on their single strike.
            DealDamage(GT,AttackPower()*.5f,TEXT("Clones"),Goal);
            PlayFx(EFx::Slash,Goal,Tint(),80.f,G.Velocity.Rotation());
            ReleaseGhost(I,true);
        }
    }
    if(!Ghosts.Num())FinishCast();
}

void ALiveSwordSoulEater::EndPlay(const EEndPlayReason::Type Reason)
{
    // The ground-wave charge plays at the planted spot rather than on the blade, so a sword dismissed mid-charge would
    // leave it looping in the world. Fires on enemies burn down the same way.
    if(ChargeFx){ChargeFx->Deactivate();ChargeFx->SetAutoDestroy(true);ChargeFx=nullptr;}
    for(const FBurn& B:Burns)if(UNiagaraComponent* Fx=B.Fx.Get())Fx->Deactivate();
    Burns.Reset();
    Super::EndPlay(Reason);
}

void ALiveSwordSoulEater::Ignite(ALiveMonster* Target)
{
    for(FBurn& B:Burns)if(B.Target.Get()==Target&&B.Fx.IsValid()){B.Remaining=BurnSeconds;return;}
    USceneComponent* Root=Target->GetRootComponent();if(!Root)return;
    // From the ground it stands on up to its head, as wide as its body.
    const FVector Feet=Target->GetFootLocation();
    const float Height=FMath::Max(60.f,float(Target->GetHealthBarLocation().Z-Feet.Z)-25.f);
    const float Radius=Target->Collision?Target->Collision->GetScaledSphereRadius():40.f;
    FBurn B;B.Target=Target;B.Remaining=BurnSeconds;
    B.Fx=LiveSwordVfx::BurnAttached(Root,Root->GetComponentTransform().InverseTransformPosition(Feet),Tint()*2.f,Radius,Height);
    if(B.Fx.IsValid())Burns.Add(B);
}

void ALiveSwordSoulEater::TickGroundWave(float DeltaSeconds)
{
    const FVector Above=Spot+FVector(0,0,BladeLength*.5f+170.f),Planted=Spot+FVector(0,0,BladeLength*.16f);
    switch(Phase)
    {
    case 0:
        Steer(Above,DeltaSeconds,2500.f,9.f);FaceDown(DeltaSeconds,10.f);
        if(ModeTime>.9f||FVector::DistSquared(GetActorLocation(),Above)<1600.f){Phase=1;ModeTime=0;}
        break;
    case 1:
        Steer(Planted,DeltaSeconds,4200.f,22.f);FaceDown(DeltaSeconds,30.f);
        if(ModeTime>.2f)
        {
            SetActorLocation(Planted);Velocity=FVector::ZeroVector;Phase=2;ModeTime=0;
            PlayFx(EFx::Ring,Spot,Tint(),240.f);PlayFx(EFx::Shatter,Spot,Tint(),90.f);
            ChargeFx=PlayFx(EFx::Charge,Spot+FVector(0,0,40),Tint(),230.f);
        }
        break;
    case 2:
    {
        // Two seconds planted: the blade trembles harder as the charge gathers.
        const float Shake=2.f+ModeTime*3.f;
        SetActorLocation(Planted+FVector(Random.FRandRange(-Shake,Shake),Random.FRandRange(-Shake,Shake),0));FaceDown(DeltaSeconds,30.f);
        if(ModeTime>=2.f)
        {
            if(ChargeFx){ChargeFx->Deactivate();ChargeFx=nullptr;}
            for(int32 I=0;I<8;++I)
            {
                const FVector D(FMath::Cos(I*PI/4),FMath::Sin(I*PI/4),0);
                FLiveSwordShot S;S.Velocity=D*1500.f;S.Range=1300.f;S.Radius=80.f;S.Damage=AttackPower()*1.2f;S.bPierce=true;S.bGround=true;
                S.Color=Tint();S.Size=170.f;S.HitFx=EFx::Burst;S.HitSize=70.f;S.SkillId=TEXT("GroundWave");
                Launch(Spot+D*70.f+FVector(0,0,28),S);
            }
            PlayFx(EFx::Ring,Spot,Tint(),560.f);PlayFx(EFx::Shatter,Spot,Tint(),140.f);
            Phase=3;ModeTime=0;
        }
        break;
    }
    default:
        Steer(Above+FVector(0,0,40),DeltaSeconds,2400.f,10.f);Face(FVector::UpVector,DeltaSeconds,12.f);
        if(ModeTime>.3f)FinishCast();
        break;
    }
}

void ALiveSwordSoulEater::AfterHit(ALiveMonster* Target,float Applied,bool bKilled,FName SkillId,bool bAllowFollowUp)
{
    if(!IsValid(Target)||bKilled||!Target->IsAlive())return;
    if(Applied>0)Ignite(Target);
    if(Target->GetHealthFraction()>=LiveSpecialSwords::DevourHealthBelow)return;
    if(Devours.ContainsByPredicate([Target](const FDevour& D){return D.Target.Get()==Target;}))return;
    if(Random.FRand()<LiveSpecialSwords::DevourChance)BeginDevour(Target);
}

bool ALiveSwordSoulEater::DevourNow(ALiveMonster* Target)
{
    if(!IsValid(Target)||!Target->IsAlive()||Devours.ContainsByPredicate([Target](const FDevour& D){return D.Target.Get()==Target;}))return false;
    BeginDevour(Target);return true;
}

void ALiveSwordSoulEater::BeginDevour(ALiveMonster* Target)
{
    // A violet serpent leaves the blade for the target; the bite and the absorption follow the effect's own timeline.
    FDevour D;D.Target=Target;D.At=HitPoint(Target);
    D.Fx=LiveSwordVfx::DevourAttached(Pivot,D.At,Tint()*2.f);
    Devours.Add(D);
}

void ALiveSwordSoulEater::TickKind(float DeltaSeconds)
{
    // Fires burn down after their last hit, or once the enemy falls (slain, devoured, a scarecrow knocked down).
    for(int32 I=Burns.Num()-1;I>=0;--I)
    {
        FBurn& B=Burns[I];B.Remaining-=DeltaSeconds;
        const ALiveMonster* T=B.Target.Get();
        if(B.Remaining>0.f&&T&&T->IsAlive()&&B.Fx.IsValid())continue;
        if(UNiagaraComponent* Fx=B.Fx.Get())Fx->Deactivate();
        Burns.RemoveAt(I);
    }
    const auto Short=[](double V){return V<10.0?FString::Printf(TEXT("%.1f"),V):LiveSwordCodex::Amount(V);};
    for(int32 I=Devours.Num()-1;I>=0;--I)
    {
        FDevour& D=Devours[I];D.Time+=DeltaSeconds;
        ALiveMonster* T=D.Target.Get();
        // The jaws follow the target until they close; the essence then flows from where it was swallowed.
        if(!D.bBitten&&T&&T->IsAlive())D.At=HitPoint(T);
        if(UNiagaraComponent* Fx=D.Fx.Get())Fx->SetVariableVec3(TEXT("User.Source"),D.At-Fx->GetComponentLocation());
        if(!D.bBitten&&D.Time>=LiveSwordVfx::DevourBite)
        {
            D.bBitten=true;
            // Another blow finished it first: the serpent closes on nothing and no essence rises.
            if(!T||!T->IsAlive()){if(UNiagaraComponent* Fx=D.Fx.Get())Fx->Deactivate();Devours.RemoveAt(I);continue;}
            D.bGrows=T->GrantsGrowth();
            D.Gain=LiveSpecialSwords::DevourGain(T->GetCurrentAttack(),T->GetCurrentDefense(),T->GetCurrentMaxHealth());
            T->Devour(ParticipantId);
            PushFloater(TEXT("噬魂"),Tint(),D.At+FVector(0,0,70));
        }
        if(D.bBitten&&D.Time>=LiveSwordVfx::DevourAbsorb)
        {
            if(D.bGrows)
            {
                if(ULiveSpecialSwordComponent* M=Manager.Get())M->AddDevour(ParticipantId,D.Gain);
                PushFloater(FString::Printf(TEXT("吞噬 攻+%s 防+%s 血+%s"),*Short(D.Gain.Attack),*Short(D.Gain.Defense),*Short(D.Gain.Health)),
                    FLinearColor(1.f,.45f,1.f),GetActorLocation()+FVector(0,0,110));
            }
            else PushFloater(TEXT("精元入剑"),FLinearColor(1.f,.45f,1.f),GetActorLocation()+FVector(0,0,110));
            PlayFx(EFx::Burst,GetActorLocation(),Tint(),90.f);
            Devours.RemoveAt(I);
        }
    }
}

float ALiveSwordSoulEater::GetMeterFraction()const
{
    if(Mode==EMode::Cast&&CastingSkill==TEXT("GroundWave")&&Phase==2)return FMath::Clamp(ModeTime/2.f,0.f,1.f);
    return -1.f;
}

FText ALiveSwordSoulEater::GetMeterLabel()const{return LOCTEXT("SoulCharge","插地蓄力");}

// ---------------------------------------------------------------- 无机态那

ALiveSwordInorganic::ALiveSwordInorganic()
{
    Kind=ELiveSpecialSword::Inorganic;
    AxeForm=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("AxeForm"));AxeForm->SetupAttachment(Pivot);
    AxeForm->SetCollisionEnabled(ECollisionEnabled::NoCollision);AxeForm->SetGenerateOverlapEvents(false);AxeForm->SetVisibility(false);AxeForm->SetCastShadow(false);
}

void ALiveSwordInorganic::SetupForm()
{
    USkeletalMesh* Mesh=LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/Fab/Inorganicstate/Solitude_Ember.Solitude_Ember"),nullptr,LOAD_NoWarn|LOAD_Quiet);
    UAnimSequence* Anim=LoadObject<UAnimSequence>(nullptr,TEXT("/Game/Fab/Inorganicstate/Solitude_Ember_Anim.Solitude_Ember_Anim"),nullptr,LOAD_NoWarn|LOAD_Quiet);
    SetupSkeletal(Mesh,Anim,140.f);
    if(UStaticMesh* Axe=GiantAxeMesh())
    {
        AxeForm->SetStaticMesh(Axe);
        if(UMaterialInterface* Ghost=GhostMaterial(EStyle::Tech))
        {
            UMaterialInstanceDynamic* MID=UMaterialInstanceDynamic::Create(Ghost,AxeForm);
            MID->SetVectorParameterValue(TEXT("Color"),Tint()*3.f);MID->SetScalarParameterValue(TEXT("Opacity"),.9f);
            for(int32 I=0;I<FMath::Max(1,AxeForm->GetNumMaterials());++I)AxeForm->SetMaterial(I,MID);
        }
        // The axe is authored with the grip end at its origin and the head toward +Z.
        AxeForm->SetRelativeLocation(FVector(0,0,-BladeLength*.45f));
    }
}

void ALiveSwordInorganic::EndPlay(const EEndPlayReason::Type Reason)
{
    if(auto* B=Buffs())B->Remove(TEXT("MechaSoul"),ParticipantId);
    Super::EndPlay(Reason);
}

float ALiveSwordInorganic::DamageBonus()const
{
    const ULiveSpecialSwordComponent* M=Manager.Get();return M?float(M->GetGrowth(ParticipantId).MechaDamageBonus):0.f;
}

FText ALiveSwordInorganic::GetMeterLabel()const{return FText::Format(LOCTEXT("MechaMeter","机魂 {0}/5"),FText::AsNumber(Stacks));}

void ALiveSwordInorganic::OnNormalHit(ALiveMonster* Target)
{
    if(Stacks>=5)return;
    Stacks=FMath::Min(5,Stacks+1);
    if(auto* B=Buffs())B->Apply(ULiveAnchorBuffSubsystem::MakeMechaSoul(ParticipantId,Stacks));
    PlayFx(EFx::Glyph,GetActorLocation(),Tint(),45.f);
    PushFloater(FString::Printf(TEXT("机魂 %d/5"),Stacks),Tint(),GetActorLocation()+FVector(0,0,80));
}

void ALiveSwordInorganic::ShowAxe(bool bShow)
{
    AxeForm->SetVisibility(bShow&&AxeForm->GetStaticMesh()!=nullptr);
    SkeletalBlade->SetVisibility(!bShow||!AxeForm->GetStaticMesh());
    if(Aura)Aura->SetVisibility(SkeletalBlade->IsVisible());
}

void ALiveSwordInorganic::BeginCast(FName Skill)
{
    Phase=0;ShotTimer=0;
    if(Skill==TEXT("AxeSlam"))
    {
        const ALiveMonster* T=NearestEnemy(AnchorCenter(),EngageRange());
        Spot=T?T->GetActorLocation():AnchorCenter()+FVector(320,0,0);Spot.Z=FloorZ();
        PlayFx(EFx::Glyph,GetActorLocation(),Tint(),90.f);
    }
    else if(Skill==TEXT("Spin"))
    {
        SpinHits.Reset();SpinFx=AttachFx(Pivot,EFx::Spin,Tint(),BladeLength*.85f);
        // The blade itself whirls; the vortex follows its position but stays level.
        if(SpinFx){SpinFx->SetUsingAbsoluteRotation(true);SpinFx->SetWorldRotation(FRotator::ZeroRotator);}
        if(Trail)Trail->SetVariableFloat(TEXT("User.Width"),BladeLength*.3f);
    }
    else BarrageIndex=0;
}

void ALiveSwordInorganic::TickCast(float DeltaSeconds)
{
    if(CastingSkill==TEXT("AxeSlam"))TickAxe(DeltaSeconds);
    else if(CastingSkill==TEXT("Spin"))TickSpin(DeltaSeconds);
    else TickBarrage(DeltaSeconds);
}

void ALiveSwordInorganic::TickAxe(float DeltaSeconds)
{
    const FVector High=Spot+FVector(0,0,430.f);
    FVector Toward=(Spot-AnchorCenter()).GetSafeNormal2D();if(Toward.IsNearlyZero())Toward=FVector::ForwardVector;
    switch(Phase)
    {
    case 0:
        Steer(High,DeltaSeconds,2600.f,8.f);Face(FVector::UpVector,DeltaSeconds,8.f);
        if(ModeTime>.5f)
        {
            Phase=1;ModeTime=0;ShowAxe(true);
            PlayFx(EFx::Glyph,GetActorLocation(),Tint(),220.f);PlayFx(EFx::Burst,GetActorLocation(),Tint(),200.f);
            PushFloater(TEXT("机魂化斧"),Tint(),GetActorLocation()+FVector(0,0,160));
        }
        break;
    case 1:
        // Wind-up: the giant axe leans back over the target.
        Steer(High+FVector(0,0,60)-Toward*120.f,DeltaSeconds,1200.f,6.f);Face(FVector::UpVector*.55f-Toward,DeltaSeconds,7.f);
        if(ModeTime>.38f){Phase=2;ModeTime=0;}
        break;
    case 2:
        Steer(Spot-Toward*140.f+FVector(0,0,120.f),DeltaSeconds,6500.f,26.f);Face(Toward+FVector(0,0,-.35f),DeltaSeconds,40.f);
        if(ModeTime>.18f)
        {
            TArray<ALiveMonster*> Hit;Enemies(Spot,650.f,Hit);
            for(ALiveMonster* M:Hit){DealDamage(M,AttackPower()*3.f,TEXT("AxeSlam"),HitPoint(M));if(M->IsAlive())M->ApplyPush(Spot,450.f,.35f);}
            PlayFx(EFx::Ring,Spot,Tint(),650.f);PlayFx(EFx::Glyph,Spot,Tint(),320.f);PlayFx(EFx::Shatter,Spot,Tint(),180.f);
            const int32 Consumed=Stacks;Stacks=0;
            if(auto* B=Buffs())B->Remove(TEXT("MechaSoul"),ParticipantId);
            const double Total=Manager.IsValid()?Manager->AddMechaBonus(ParticipantId,.05*Consumed):0.0;
            PushFloater(FString::Printf(TEXT("机魂淬炼 · 剑灵伤害 +%.0f%%"),Total*100),FLinearColor(.4f,1.f,1.f),Spot+FVector(0,0,200));
            Phase=3;ModeTime=0;
        }
        break;
    default:
        Steer(Spot+FVector(0,0,260.f),DeltaSeconds,900.f,4.f);
        if(ModeTime>.45f){ShowAxe(false);PlayFx(EFx::Glyph,GetActorLocation(),Tint(),120.f);FinishCast();}
        break;
    }
}

void ALiveSwordInorganic::TickSpin(float DeltaSeconds)
{
    // Rosette path: each petal flies out through the crowd and back like a boomerang.
    SpinAngle=FMath::Fmod(SpinAngle+DeltaSeconds*26.f,2*PI);
    const float Phi=ModeTime*2.5f+OrbitAngle;
    const float Rho=150.f+540.f*FMath::Abs(FMath::Sin(1.5f*Phi));
    Steer(AnchorCenter()+FVector(FMath::Cos(Phi)*Rho,FMath::Sin(Phi)*Rho,40.f+25.f*FMath::Sin(ModeTime*5.f)),DeltaSeconds,3400.f,12.f);
    Facing=FRotationMatrix::MakeFromZ(FVector(FMath::Cos(SpinAngle),FMath::Sin(SpinAngle),.22f).GetSafeNormal()).ToQuat();
    TArray<ALiveMonster*> Near;Enemies(GetActorLocation(),180.f+BladeLength*.45f,Near);
    for(ALiveMonster* M:Near)
    {
        float& Next=SpinHits.FindOrAdd(M);
        if(ModeTime<Next)continue;
        // Twice the normal hit frequency on every enemy the blade passes.
        Next=ModeTime+AttackInterval*.5f;
        DealDamage(M,AttackPower(),TEXT("Spin"),HitPoint(M));
        PlayFx(EFx::Slash,HitPoint(M),Tint(),90.f,FRotator(0,SpinAngle*57.3f,0));
    }
    if(ModeTime>=5.f)
    {
        if(SpinFx){SpinFx->Deactivate();SpinFx=nullptr;}
        if(Trail)Trail->SetVariableFloat(TEXT("User.Width"),BladeLength*.16f);
        FinishCast();
    }
}

void ALiveSwordInorganic::TickBarrage(float DeltaSeconds)
{
    TArray<ALiveMonster*> List;Enemies(AnchorCenter(),1700.f,List);
    const FVector Toward=List.Num()?(List[0]->GetActorLocation()-AnchorCenter()).GetSafeNormal2D():FVector::ZeroVector;
    Steer(AnchorCenter()+FVector(0,0,270.f)+Toward*90.f,DeltaSeconds,1800.f,8.f);
    if(List.Num())Face(HitPoint(List[BarrageIndex%FMath::Min(List.Num(),6)])-GetActorLocation(),DeltaSeconds,16.f);
    ShotTimer-=DeltaSeconds;
    while(ShotTimer<=0.f&&ModeTime<5.f&&List.Num())
    {
        ShotTimer+=.2f;
        ALiveMonster* T=List[BarrageIndex%FMath::Min(List.Num(),6)];++BarrageIndex;
        const FVector From=GetActorLocation()+Facing.GetUpVector()*BladeLength*.45f;
        FLiveSwordShot S;S.Velocity=(HitPoint(T)-From).GetSafeNormal()*2700.f;S.Range=1800.f;S.Radius=55.f;S.Damage=AttackPower()*.6f;S.bPierce=true;
        S.Color=BarrageIndex%2?Tint()*1.6f:FLinearColor(1.f,.2f,.9f)*1.6f;S.Size=110.f;S.HitFx=EFx::Glyph;S.HitSize=60.f;S.SkillId=TEXT("Barrage");
        Launch(From,S);
        PlayFx(EFx::Glyph,From,S.Color,28.f);
    }
    if(ShotTimer<=0.f)ShotTimer=0.f;
    if(ModeTime>=5.f)FinishCast();
}

// ---------------------------------------------------------------- 驱煞神刃

void ALiveSwordDivineBlade::SetupForm(){SetupBlade(SwordMesh(uint8(Kind)),135.f);}

float ALiveSwordDivineBlade::GetMeterFraction()const{return bTwinActive?FMath::Clamp(Store/FMath::Max(1.f,GetStoreCapacity()),0.f,1.f):-1.f;}
FText ALiveSwordDivineBlade::GetMeterLabel()const
{
    return FText::Format(LOCTEXT("TwinStore","蓄势 {0}%"),FText::AsNumber(FMath::FloorToInt(GetMeterFraction()*100)));
}

void ALiveSwordDivineBlade::Purify(const FVector& From,float& Dispel,float& Bless,float DeltaSeconds)
{
    Dispel-=DeltaSeconds;Bless-=DeltaSeconds;
    APawn* A=Anchor();UCultivationHealthComponent* H=AnchorHealthOf(this);
    if(!A||!H||H->bDead)return;
    const FVector At=A->GetActorLocation();
    if(Dispel<=0.f)
    {
        Dispel+=10.f;FText Removed;
        if(auto* B=Buffs();B&&B->DispelOne(Removed))
        {
            LiveSwordVfx::Beam(this,From,At,Tint()*2.f,10.f);PlayFx(EFx::Burst,At,Tint(),150.f);
            PushFloater(TEXT("驱散·")+Removed.ToString(),Tint(),At+FVector(0,0,120));
        }
    }
    if(Bless<=0.f)
    {
        Bless+=10.f;
        // Restores a quarter of the anchor's current (not maximum) health.
        const double Healed=H->Heal(H->Health*.25);
        if(Healed>0)
        {
            LiveSwordVfx::Beam(this,From,At,FLinearColor(.5f,1.f,.55f)*2.f,8.f);PlayFx(EFx::Pillar,At,FLinearColor(.55f,1.f,.6f),90.f);
            PushFloater(FString::Printf(TEXT("+%.0f 生命"),Healed),FLinearColor(.5f,1.f,.55f),At+FVector(0,0,150));
        }
    }
}

void ALiveSwordDivineBlade::TickKind(float DeltaSeconds)
{
    Purify(GetActorLocation(),DispelTimer,BlessTimer,DeltaSeconds);
    // Clones inherit the passives: each keeps its own dispel and blessing clock.
    for(FGhost& G:Ghosts)Purify(G.Position,G.DispelTimer,G.BlessTimer,DeltaSeconds);
    if(bTwinActive&&!bMerging)TickTwins(DeltaSeconds);
}

void ALiveSwordDivineBlade::BeginCast(FName Skill)
{
    bTwinActive=true;bMerging=false;Store=0;Phase=0;
    SkillReadyAt.Add(TEXT("Twin"),MAX_flt);
    for(int32 I=0;I<2;++I)
    {
        const FVector Side=FVector(0,I?1:-1,0).RotateAngleAxis(Random.FRandRange(-20.f,20.f),FVector::UpVector);
        const int32 G=SpawnGhost(GetActorLocation()+Side*90.f,.7f);if(G==INDEX_NONE)continue;
        Ghosts[G].Angle=OrbitAngle+(I?2.1f:-2.1f);Ghosts[G].Timer=.3f+I*.25f;
        Ghosts[G].DispelTimer=FMath::Fmod(DispelTimer+3.3f*(I+1),10.f);Ghosts[G].BlessTimer=FMath::Fmod(BlessTimer+3.3f*(I+1),10.f);
    }
    PlayFx(EFx::Burst,GetActorLocation(),Tint(),160.f);
    PushFloater(TEXT("双影出鞘"),Tint(),GetActorLocation()+FVector(0,0,100));
    // The body keeps fighting while its clones gather force.
    CastingSkill=NAME_None;Mode=EMode::Recover;ModeTime=0;
}

void ALiveSwordDivineBlade::TickTwins(float DeltaSeconds)
{
    for(FGhost& G:Ghosts)
    {
        G.Age+=DeltaSeconds;G.Timer-=DeltaSeconds;
        if(G.Phase==0)
        {
            G.Angle+=DeltaSeconds*.8f;
            SteerGhost(G,AnchorCenter()+FVector(FMath::Cos(G.Angle)*(OrbitRadius+60.f),FMath::Sin(G.Angle)*(OrbitRadius+60.f),OrbitHeight-20.f),DeltaSeconds,1400.f,4.f);
            if(G.Velocity.SizeSquared()<10000.f)G.Facing=FMath::QInterpTo(G.Facing,FQuat::Identity,DeltaSeconds,4.f);
            if(G.Timer<=0.f)if(ALiveMonster* T=NearestEnemy(AnchorCenter(),EngageRange())){G.Target=T;G.Phase=1;G.Age=0;}
        }
        else if(G.Phase==1)
        {
            ALiveMonster* T=G.Target.Get();
            if(!T||!T->IsAlive()){G.Phase=0;G.Timer=.2f;continue;}
            const FVector Goal=HitPoint(T),Before=G.Position;
            SteerGhost(G,Goal,DeltaSeconds,2800.f,13.f);
            if(Crossed(Goal,Before,G.Position,70.f))
            {
                // Each clone strikes with the body's full attack; its damage feeds the shared store.
                Store+=DealDamage(T,AttackPower(),TEXT("Twin"),Goal);
                PlayFx(EFx::Slash,Goal,Tint(),100.f,G.Velocity.Rotation());
                G.Phase=2;G.Age=0;G.Timer=AttackInterval*Random.FRandRange(.85f,1.15f);G.Velocity*=-.3f;
            }
            else if(G.Age>2.5f){G.Phase=2;G.Age=0;}
        }
        else
        {
            SteerGhost(G,AnchorCenter()+FVector(FMath::Cos(G.Angle)*OrbitRadius,FMath::Sin(G.Angle)*OrbitRadius,OrbitHeight),DeltaSeconds,1800.f,6.f);
            if(G.Age>.35f)G.Phase=0;
        }
    }
    if(Store>=GetStoreCapacity()&&Mode!=EMode::Cast)
    {
        bMerging=true;Phase=0;Mode=EMode::Cast;ModeTime=0;CastingSkill=TEXT("Twin");
        PushFloater(TEXT("双影归一"),Tint(),GetActorLocation()+FVector(0,0,110));
    }
}

void ALiveSwordDivineBlade::TickCast(float DeltaSeconds)
{
    if(!bMerging){FinishCast();return;}
    const FVector Above=AnchorCenter()+FVector(0,0,280.f);
    if(Phase==0)
    {
        Steer(Above,DeltaSeconds,1800.f,7.f);Face(FVector::UpVector,DeltaSeconds,8.f);
        for(FGhost& G:Ghosts)SteerGhost(G,GetActorLocation(),DeltaSeconds,3000.f,10.f);
        if(ModeTime>.55f)
        {
            while(Ghosts.Num())ReleaseGhost(Ghosts.Num()-1,false);
            PlayFx(EFx::Burst,GetActorLocation(),Tint(),220.f);
            ChargeFx=AttachFx(Pivot,EFx::Charge,Tint(),260.f);
            Phase=1;ModeTime=0;
        }
    }
    else if(Phase==1)
    {
        // One-second charge, then a strike on every enemy in a very large radius for 128% of the store.
        Steer(Above+FVector(Random.FRandRange(-4.f,4.f),Random.FRandRange(-4.f,4.f),0),DeltaSeconds,600.f,6.f);Face(FVector::UpVector,DeltaSeconds,8.f);
        if(ModeTime>=1.f)
        {
            if(ChargeFx){ChargeFx->Deactivate();ChargeFx=nullptr;}
            const float Damage=1.28f*Store;
            TArray<ALiveMonster*> All;Enemies(AnchorCenter(),1800.f,All);
            for(ALiveMonster* M:All)DealDamage(M,Damage,TEXT("Devastate"),HitPoint(M));
            FVector Ground=AnchorCenter();Ground.Z=FloorZ();
            PlayFx(EFx::Pillar,Ground,Tint(),320.f);PlayFx(EFx::Ring,Ground,Tint(),1800.f);PlayFx(EFx::Burst,GetActorLocation(),Tint(),420.f);
            PushFloater(FString::Printf(TEXT("毁天灭地 %.0f"),Damage),FLinearColor(1.f,.85f,.3f),GetActorLocation()+FVector(0,0,160));
            Store=0;bTwinActive=false;bMerging=false;StartCooldown(TEXT("Twin"));
            Phase=2;ModeTime=0;
        }
    }
    else if(ModeTime>.4f)FinishCast();
}

void ALiveSwordDivineBlade::AfterHit(ALiveMonster* Target,float Applied,bool bKilled,FName SkillId,bool bAllowFollowUp){}

// ---------------------------------------------------------------- 七星光芒剑

void ALiveSwordSevenStar::SetupForm(){SetupBlade(SwordMesh(uint8(Kind)),140.f);}

void ALiveSwordSevenStar::BeginPlay()
{
    Super::BeginPlay();
    // Holding off a backlash, the guard stands for all of it even once its light shield is spent.
    if(auto* B=Buffs())ShieldHandle=B->OnShieldBroken.AddWeakLambda(this,[this](const FString& Source)
    {if(Source==ParticipantId&&Mode==EMode::Guard&&LastWard!=uint8(EXianTuWard::Held))EndGuard(true);});
}

void ALiveSwordSevenStar::EndPlay(const EEndPlayReason::Type Reason)
{
    if(auto* B=Buffs())B->OnShieldBroken.Remove(ShieldHandle);
    if(ShieldFx){ShieldFx->DestroyComponent();ShieldFx=nullptr;}
    Super::EndPlay(Reason);
}

bool ALiveSwordSevenStar::CanCast(FName Skill)const{return NearestEnemy(AnchorCenter(),Skill==TEXT("Lightning")?1400.f:1600.f)!=nullptr;}

void ALiveSwordSevenStar::TickKind(float DeltaSeconds)
{
    TickWard();
    // Strikes called down by 光芒剑气, a beat apart: strike k falls on the k-th enemy its sword qi pierced (cycling), and the
    // fallen give way to the nearest enemy still standing.
    for(int32 I=0;I<PendingStrikes.Num();)
    {
        if((PendingStrikes[I].Delay-=DeltaSeconds)>0.f){++I;continue;}
        const int32 Index=PendingStrikes[I].Index;PendingStrikes.RemoveAt(I);
        TArray<ALiveMonster*> Struck;for(const auto& W:RadianceHits)if(ALiveMonster* M=W.Get();M&&M->IsAlive())Struck.Add(M);
        ALiveMonster* Target=Struck.Num()?Struck[Index%Struck.Num()]:NearestEnemy(AnchorCenter(),1700.f);
        if(Target)CallStrike(Target,Index);
    }
    const UCultivationHealthComponent* H=AnchorHealthOf(this);
    if(!H||H->bDead||H->MaxHealth<=0){LastSeventh=-1;return;}
    const float F=float(H->GetHealthFraction());
    const int32 Now=FMath::Clamp(FMath::FloorToInt(F*7.f+1.e-3f),0,7);
    if(LastSeventh<0){LastSeventh=Now;return;}
    if(Now<LastSeventh)
    {
        LastSeventh=Now;
        // Each lost seventh of health calls the guardian, except while a skill is being cast or the heavens broke its light.
        if(Mode!=EMode::Cast&&Mode!=EMode::Guard&&LastWard!=uint8(EXianTuWard::Broken))TriggerGuard();
    }
    else if(Now>LastSeventh&&F>=Now/7.f+.02f)LastSeventh=Now;
}

void ALiveSwordSevenStar::TickWard()
{
    const UGameInstance* GI=GetGameInstance();const UXianTuSubsystem* X=GI?GI->GetSubsystem<UXianTuSubsystem>():nullptr;
    const uint8 Ward=X?uint8(X->GetStarWard()):0;
    if(Ward==LastWard)
    {
        // The held guard stands over the anchor until the backlash ends.
        if(Ward==uint8(EXianTuWard::Held)&&Mode!=EMode::Guard)TriggerGuard(false);
        return;
    }
    LastWard=Ward;
    const FVector Above=AnchorCenter()+FVector(0,0,200);
    if(Ward==uint8(EXianTuWard::Held))
    {
        // 七星挡劫: whatever it was casting, the guardian rises over the anchor against the heavens.
        if(ChargeFx){ChargeFx->Deactivate();ChargeFx=nullptr;}
        TriggerGuard(false);
        PushFloater(TEXT("七星挡劫"),Tint(),Above);
    }
    else if(Ward==uint8(EXianTuWard::Broken))
    {
        // Too low a realm for these heavens: the light shatters and the guard cannot rise again while the backlash lasts.
        if(Mode==EMode::Guard)EndGuard(false);
        PlayFx(EFx::Shatter,AnchorCenter(),Tint(),220.f);
        PushFloater(TEXT("天雷破盾"),FLinearColor(.62f,.74f,1.f),Above);
    }
}

void ALiveSwordSevenStar::TriggerGuard(bool bAnnounce)
{
    APawn* A=Anchor();if(!A)return;
    Mode=EMode::Guard;ModeTime=0;GuardRemaining=15.f;CastingSkill=NAME_None;DashTarget=nullptr;
    if(auto* B=Buffs())
    {
        B->Apply(ULiveAnchorBuffSubsystem::MakeStarShield(ParticipantId,AttackPower()*.7f,15.f));
        B->Apply(ULiveAnchorBuffSubsystem::MakeStarRegen(ParticipantId,15.f));
    }
    if(!ShieldFx)ShieldFx=AttachFx(A->GetRootComponent(),EFx::Shield,Tint()*1.5f,150.f);
    PlayFx(EFx::Burst,A->GetActorLocation(),Tint(),200.f);
    if(bAnnounce)PushFloater(TEXT("七星护道"),Tint(),A->GetActorLocation()+FVector(0,0,170));
}

void ALiveSwordSevenStar::EndGuard(bool bBroken)
{
    if(auto* B=Buffs())B->Remove(TEXT("StarShield"),ParticipantId);
    if(ShieldFx){ShieldFx->Deactivate();ShieldFx->SetAutoDestroy(true);ShieldFx=nullptr;}
    if(bBroken)
    {
        PlayFx(EFx::Shatter,AnchorCenter(),Tint(),160.f);
        PushFloater(TEXT("光盾破碎"),Tint(),AnchorCenter()+FVector(0,0,150));
    }
    GuardRemaining=0;if(Mode==EMode::Guard){Mode=EMode::Recover;ModeTime=0;}
}

void ALiveSwordSevenStar::TickGuard(float DeltaSeconds)
{
    GuardRemaining-=DeltaSeconds;
    // Hovers before the anchor in a slow circle and does not attack while guarding.
    Steer(AnchorCenter()+FVector(70.f*FMath::Cos(Clock*2.f),70.f*FMath::Sin(Clock*2.f),200.f),DeltaSeconds,1600.f,6.f);
    Face(FVector(.15f*FMath::Cos(Clock*3.f),.15f*FMath::Sin(Clock*3.f),1.f),DeltaSeconds,6.f);
    if(GuardRemaining<=0.f)EndGuard(false);
}

void ALiveSwordSevenStar::BeginCast(FName Skill)
{
    Phase=0;Fired=0;BoltTimer=0;
    ChargeFx=AttachFx(Pivot,EFx::Charge,Tint(),Skill==TEXT("Lightning")?150.f:120.f);
}

void ALiveSwordSevenStar::TickCast(float DeltaSeconds){if(CastingSkill==TEXT("Lightning"))TickLightning(DeltaSeconds);else TickRadiance(DeltaSeconds);}

void ALiveSwordSevenStar::TickLightning(float DeltaSeconds)
{
    Steer(AnchorCenter()+FVector(0,0,330.f),DeltaSeconds,1800.f,7.f);Face(FVector::UpVector,DeltaSeconds,6.f);
    if(Phase==0)
    {
        if(ModeTime<.6f)return;
        if(ChargeFx){ChargeFx->Deactivate();ChargeFx=nullptr;}
        TArray<ALiveMonster*> List;Enemies(AnchorCenter(),1400.f,List);
        BoltTargets.Reset();
        for(int32 I=0;I<List.Num()&&BoltTargets.Num()<7;++I)BoltTargets.Add(List[I]);
        const int32 Distinct=BoltTargets.Num();
        if(!Distinct){FinishCast();return;}
        // Fewer than seven targets: the spare bolts fall on random targets already chosen. Copy the pick first:
        // adding an element of the array to itself asserts, since the add may reallocate it.
        while(BoltTargets.Num()<7){const TWeakObjectPtr<ALiveMonster> Spare=BoltTargets[Random.RandRange(0,Distinct-1)];BoltTargets.Add(Spare);}
        PlayFx(EFx::Burst,GetActorLocation(),Tint(),180.f);
        Phase=1;ModeTime=0;
        return;
    }
    BoltTimer-=DeltaSeconds;
    while(BoltTimer<=0.f&&Fired<7)
    {
        BoltTimer+=.09f;const int32 I=Fired++;
        ALiveMonster* M=BoltTargets[I].Get();if(!M||!M->IsAlive())continue;
        const FVector At=HitPoint(M);
        LiveSwordVfx::Beam(this,GetActorLocation()+Facing.GetUpVector()*BladeLength*.45f,At,ZapColor(I)*3.f,16.f);
        LiveSwordVfx::Zap(this,I,M->GetFootLocation(),.55f);
        // 80% of the attack, plus 5% of the target's current health added in ModifyHit.
        DealDamage(M,AttackPower()*LiveSpecialSwords::BoltAttack,TEXT("Lightning"),At);
    }
    if(Fired>=7&&ModeTime>.9f)FinishCast();
}

void ALiveSwordSevenStar::CallStrike(ALiveMonster* Target,int32 Index)
{
    // Down from the sky over the target in the seven-star gold, onto its feet.
    const FVector At=HitPoint(Target);
    const FVector Sky=At+FVector(Random.FRandRange(-110.f,110.f),Random.FRandRange(-110.f,110.f),760.f);
    LiveSwordVfx::Beam(this,Sky,At,FLinearColor(1.f,.95f,.72f)*3.2f,22.f);
    LiveSwordVfx::Zap(this,Index%2?5:2,Target->GetFootLocation(),.75f);
    PlayFx(EFx::Burst,At,Tint(),110.f);
    DealDamage(Target,AttackPower()*LiveSpecialSwords::BoltAttack,TEXT("Strike"),At);
    ++StrikesCalled;
}

void ALiveSwordSevenStar::TickRadiance(float DeltaSeconds)
{
    const ALiveMonster* T=NearestEnemy(AnchorCenter(),1700.f);
    FVector Dir=T?(T->GetActorLocation()-AnchorCenter()).GetSafeNormal2D():FVector::ZeroVector;
    if(Dir.IsNearlyZero())Dir=FVector::ForwardVector;
    if(Phase==0)
    {
        Steer(AnchorCenter()+FVector(0,0,300.f)-Dir*60.f,DeltaSeconds,1500.f,6.f);Face(FVector::UpVector*.5f+Dir,DeltaSeconds,6.f);
        if(ChargeFx)ChargeFx->SetVariableFloat(TEXT("User.Radius"),120.f+ModeTime*45.f);
        if(ModeTime>=5.f)
        {
            if(ChargeFx){ChargeFx->Deactivate();ChargeFx=nullptr;}
            // Dives from the hover point at the target's chest, then sweeps level through the line behind it. Fired flat
            // from up here, the shot passed three metres over every enemy and never hit.
            const FVector From=GetActorLocation()+Dir*80.f;
            const FVector Aim=T?HitPoint(T):FVector(From.X,From.Y,AnchorCenter().Z)+Dir*700.f;
            FLiveSwordShot S;S.Velocity=(Aim-From).GetSafeNormal()*1700.f;S.bLevelOut=true;S.LevelZ=Aim.Z;
            S.Range=1900.f;S.Radius=190.f;S.Damage=AttackPower()*LiveSpecialSwords::RadianceCoefficient;S.bPierce=true;S.Knockback=500.f;
            S.Color=Tint()*1.6f;S.Size=430.f;S.HitFx=EFx::Burst;S.HitSize=140.f;S.SkillId=TEXT("Radiance");
            Launch(From,S);
            bRadianceRolled=false;RadianceHits.Reset();
            PlayFx(EFx::Ring,GetActorLocation(),Tint(),300.f);PlayFx(EFx::Burst,GetActorLocation(),Tint(),220.f);
            PushFloater(TEXT("光芒剑气"),Tint(),GetActorLocation()+FVector(0,0,120));
            Phase=1;ModeTime=0;
        }
        return;
    }
    if(ModeTime>.35f)FinishCast();
}

void ALiveSwordSevenStar::ModifyHit(ALiveMonster* Target,FLiveHit& Hit,FName SkillId)
{
    // Every bolt, the seven and the strikes 光芒剑气 calls down: 80% of the attack through the formula, plus 5% of the
    // target's current health as true damage, untouched by defense, realm or 会心.
    if(SkillId==TEXT("Lightning")||SkillId==TEXT("Strike"))Hit.TrueDamage+=LiveSpecialSwords::BoltHealthShare*FMath::Max(0.f,Target->Health);
}

void ALiveSwordSevenStar::AfterHit(ALiveMonster* Target,float Applied,bool bKilled,FName SkillId,bool bAllowFollowUp)
{
    if(SkillId!=TEXT("Radiance")||!IsValid(Target))return;
    RadianceHits.AddUnique(Target);
    if(bRadianceRolled)return;
    // One roll per release, on the sword qi's first hit: 45% to call three strikes down on what it pierces.
    bRadianceRolled=true;
    const bool bCall=ForcedStrikeRoll>=0?ForcedStrikeRoll>0:Random.FRand()<LiveSpecialSwords::RadianceStrikeChance;
    ForcedStrikeRoll=-1;
    if(!bCall)return;
    for(int32 I=0;I<LiveSpecialSwords::RadianceStrikes;++I){FStrike S;S.Delay=.12f+.18f*I;S.Index=I;PendingStrikes.Add(S);}
    PushFloater(TEXT("引雷三击"),FLinearColor(1.f,.92f,.62f),HitPoint(Target)+FVector(0,0,140));
}

float ALiveSwordSevenStar::GetMeterFraction()const
{
    if(Mode==EMode::Guard)return FMath::Clamp(GuardRemaining/15.f,0.f,1.f);
    if(Mode==EMode::Cast&&CastingSkill==TEXT("Radiance")&&Phase==0)return FMath::Clamp(ModeTime/5.f,0.f,1.f);
    return -1.f;
}

FText ALiveSwordSevenStar::GetMeterLabel()const{return Mode==EMode::Guard?LOCTEXT("Guarding","七星护道"):LOCTEXT("Charging","光芒蓄力");}

// ---------------------------------------------------------------- 弑神剑

void ALiveSwordGodslayer::SetupForm(){SetupBlade(SwordMesh(uint8(Kind)),135.f);}

void ALiveSwordGodslayer::ModifyHit(ALiveMonster* Target,FLiveHit& Hit,FName SkillId)
{
    LastFraction=Target->GetHealthFraction();LastRaw=Hit.Attack*Hit.SkillCoefficient;
    // Ignore half the defense, three quarters once the target is below half health; 5% of the dealt damage lands again as true damage.
    Hit.DefenseIgnore=FMath::Max(Hit.DefenseIgnore,LastFraction<.5f?.75f:.5f);
    Hit.TrueDamageRatio+=.05f;
}

void ALiveSwordGodslayer::AfterHit(ALiveMonster* Target,float Applied,bool bKilled,FName SkillId,bool bAllowFollowUp)
{
    if(Applied<=0||!IsValid(Target))return;
    PlayFx(EFx::Burst,HitPoint(Target),Tint(),40.f);
    if(!bAllowFollowUp||!Target->IsAlive())return;
    // Follow-up chance is decided by the health before this hit; a follow-up never chains another.
    if(Random.FRand()<(LastFraction>.5f?.75f:.5f))
    {
        FPending P;P.Target=Target;P.Amount=LastRaw;P.Delay=.14f;Pending.Add(P);
    }
}

void ALiveSwordGodslayer::TickKind(float DeltaSeconds)
{
    for(int32 I=Pending.Num()-1;I>=0;--I)
    {
        FPending& P=Pending[I];P.Delay-=DeltaSeconds;
        if(P.Delay>0)continue;
        ALiveMonster* T=P.Target.Get();const float Amount=P.Amount;Pending.RemoveAt(I);
        if(!T||!T->IsAlive())continue;
        const FVector At=HitPoint(T);
        DealDamage(T,Amount,TEXT("Again"),At,false);++FollowUps;
        PlayFx(EFx::Slash,At,Tint()*1.5f,150.f,FRotator(0,Random.FRandRange(0.f,360.f),90.f));
        if(FollowUps%3==1)PushFloater(TEXT("再斩"),Tint(),At+FVector(0,0,90));
    }
}

#undef LOCTEXT_NAMESPACE

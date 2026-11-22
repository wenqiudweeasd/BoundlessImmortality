#include "LiveWeapons/SpecialSwords/LiveSwordProjectile.h"
#include "LiveWeapons/SpecialSwords/LiveSpecialSword.h"
#include "LiveWeapons/LiveMonster.h"
#include "LiveWeapons/LiveMonsterRegistrySubsystem.h"
#include "NiagaraComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/World.h"

ALiveSwordProjectile::ALiveSwordProjectile()
{
    PrimaryActorTick.bCanEverTick=true;
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
    SetActorEnableCollision(false);
}

void ALiveSwordProjectile::Launch(ALiveSpecialSword* InOwner,const FLiveSwordShot& InShot)
{
    OwnerSword=InOwner;Shot=InShot;FloorZ=GetActorLocation().Z;
    if(Shot.Velocity.IsNearlyZero())Shot.Velocity=FVector::ForwardVector*1800;
    SetActorRotation(Shot.Velocity.Rotation());
    Body=LiveSwordVfx::SpawnAttached(GetRootComponent(),LiveSwordVfx::EFx::Qi,Shot.Color,Shot.Size,FVector::ZeroVector,false,Shot.Style);
    if(Body)Body->SetVariableFloat(TEXT("User.Width"),Shot.bGround?Shot.Size*.35f:Shot.Size*.55f);
}

void ALiveSwordProjectile::Finish()
{
    if(bFinished)return;bFinished=true;
    if(Body)Body->Deactivate();
    SetActorTickEnabled(false);SetLifeSpan(.7f);
}

void ALiveSwordProjectile::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if(bFinished||!FMath::IsFinite(DeltaSeconds)||DeltaSeconds<=0)return;
    const FVector From=GetActorLocation();
    FVector To=From+Shot.Velocity*DeltaSeconds;
    if(Shot.bGround)To.Z=FloorZ;
    else if(Shot.bLevelOut&&Shot.Velocity.Z<0.f&&To.Z<=Shot.LevelZ)
    {
        // Down at the target's height: keep the speed, drop the dive and turn the crescent level with it.
        const FVector Flat=Shot.Velocity.GetSafeNormal2D();
        To.Z=Shot.LevelZ;Shot.Velocity=(Flat.IsNearlyZero()?GetActorForwardVector().GetSafeNormal2D():Flat)*Shot.Velocity.Size();
        SetActorRotation(Shot.Velocity.Rotation());
    }
    Travelled+=FVector::Dist(From,To);
    SetActorLocation(To);
    if(auto* Registry=GetWorld()->GetSubsystem<ULiveMonsterRegistrySubsystem>())
    {
        TArray<TWeakObjectPtr<ALiveMonster>> Near;
        Registry->CollectInRange((From+To)*.5f,FVector::Dist(From,To)*.5f+Shot.Radius+120.f,Near);
        for(const auto& Weak:Near)
        {
            ALiveMonster* M=Weak.Get();
            if(!M||!M->IsAlive()||AlreadyHit.Contains(Weak))continue;
            const float MonsterRadius=M->Collision?M->Collision->GetScaledSphereRadius():40.f;
            FVector Point=M->GetActorLocation();if(Shot.bGround)Point.Z=FloorZ;
            const FVector Closest=FMath::ClosestPointOnSegment(Point,From,To);
            if(FVector::DistSquared(Closest,Point)>FMath::Square(Shot.Radius+MonsterRadius))continue;
            AlreadyHit.Add(Weak);++HitCount;
            if(ALiveSpecialSword* Sword=OwnerSword.Get())Sword->DealDamage(M,Shot.Damage,Shot.SkillId,M->GetActorLocation());
            LiveSwordVfx::SpawnAt(this,Shot.HitFx,M->GetActorLocation(),Shot.Color,Shot.HitSize,FRotator::ZeroRotator,Shot.Style);
            if(Shot.Knockback>0&&M->IsAlive())M->ApplyPush(From-Shot.Velocity.GetSafeNormal()*50,Shot.Knockback,.35f);
            if(!Shot.bPierce){Finish();return;}
        }
    }
    if(Travelled>=Shot.Range)Finish();
}

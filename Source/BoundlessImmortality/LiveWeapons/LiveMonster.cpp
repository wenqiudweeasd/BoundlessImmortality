#include "LiveMonster.h"
#include "LiveMonsterRegistrySubsystem.h"
#include "Cultivation/CultivationSubsystem.h"
#include "Cultivation/CultivationHealthComponent.h"
#include "LiveWeapons/SpecialSwords/LiveAnchorBuffSubsystem.h"
#include "Combat/LiveCombat.h"
#include "Engine/GameInstance.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"
#include "UObject/ConstructorHelpers.h"
ALiveMonster::ALiveMonster()
{
    PrimaryActorTick.bCanEverTick=true;PrimaryActorTick.TickInterval=.05f;
    Collision=CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));SetRootComponent(Collision);
    Collision->InitSphereRadius(40);Collision->SetCollisionProfileName(TEXT("Pawn"));
    Body=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));Body->SetupAttachment(Collision);Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));Body->SetStaticMesh(Cube.Object);Body->SetRelativeScale3D(FVector(.7,.7,.85));
}
void ALiveMonster::BeginPlay(){Super::BeginPlay();DebuffRandom.Initialize(int32(GetUniqueID()*2654435761u));Health=FMath::Max(1.f,MaxHealth);RefreshEnemyModifier();GetWorld()->GetSubsystem<ULiveMonsterRegistrySubsystem>()->Register(this);}
void ALiveMonster::RefreshEnemyModifier()
{
    const float Desired=GetWorld()->GetGameInstance()?float(GetWorld()->GetGameInstance()->GetSubsystem<UCultivationSubsystem>()->GetEnemyAttributeMultiplier()):1.f;
    if(!FMath::IsNearlyEqual(Desired,EnemyAttributeMultiplier)){const float Fraction=GetHealthFraction();EnemyAttributeMultiplier=FMath::Max(.01f,Desired);Health=MaxHealth*EnemyAttributeMultiplier*Fraction;}
}
void ALiveMonster::EndPlay(const EEndPlayReason::Type Reason){if(auto* Registry=GetWorld()->GetSubsystem<ULiveMonsterRegistrySubsystem>())Registry->Unregister(this);Super::EndPlay(Reason);}
bool ALiveMonster::ApplyWeaponDamage(float Amount,const FString& Id)
{
    if(!IsAlive()||!FMath::IsFinite(Amount)||Amount<=0)return false;
    FLiveHit Hit;Hit.Attack=Amount;return ApplyWeaponHit(Hit,Id);
}
bool ALiveMonster::ApplyWeaponHit(const FLiveHit& Hit,const FString& Id)
{
    if(!IsAlive()||!FMath::IsFinite(Hit.Attack)||!FMath::IsFinite(Hit.TrueDamage)||(Hit.Attack<=0&&Hit.TrueDamage<=0))return false;
    ApplyHit(Hit,Id);return true;
}
float ALiveMonster::ApplyHit(const FLiveHit& InHit,const FString& Id,FRandomStream* Random,FLiveHitResult* OutResult)
{
    if(!IsAlive()||!FMath::IsFinite(InHit.Attack)||!FMath::IsFinite(InHit.TrueDamage)||(InHit.Attack<=0&&InHit.TrueDamage<=0))return 0;
    RefreshEnemyModifier();
    FLiveHit Hit=InHit;
    if(Realm>=0&&Hit.AttackerRealm<0&&!Id.IsEmpty())
        if(const UGameInstance* GI=GetWorld()->GetGameInstance())
            if(const auto* Cultivation=GI->GetSubsystem<UCultivationSubsystem>())
            {const FCultivationParticipantSnapshot S=Cultivation->GetParticipantSnapshot(Id);if(!S.ParticipantId.IsEmpty())Hit.AttackerRealm=S.RealmIndex;}
    const FLiveHitResult Result=LiveCombat::Resolve(Hit,GetCurrentDefense(),Realm,0,Random);
    if(OutResult)*OutResult=Result;
    if(Result.Damage<=0)return 0;
    const float Applied=FMath::Min(Health,Result.Damage);Health=FMath::Max(0.f,Health-Result.Damage);
    return FinishDamage(Applied,Id);
}
FVector ALiveMonster::GetFootLocation()const{return GetActorLocation()-FVector(0,0,Collision?Collision->GetScaledSphereRadius():40.f);}
bool ALiveMonster::Devour(const FString& Id)
{
    if(!IsAlive())return false;
    const float Applied=Health;Health=0;FinishDamage(Applied,Id);return true;
}
float ALiveMonster::FinishDamage(float Applied,const FString& Id)
{
    const bool Lethal=Health<=0;
    if(GetWorld()->GetGameInstance())GetWorld()->GetGameInstance()->GetSubsystem<UCultivationSubsystem>()->RecordActualDamage(Id,Applied,Lethal);
    OnDamaged(Applied,Id);
    if(Lethal){Health=0;Die(Id);}
    return Applied;
}
void ALiveMonster::Die(const FString& Id)
{
    GetWorld()->GetSubsystem<ULiveMonsterRegistrySubsystem>()->Unregister(this);OnDeath(Id);SetActorEnableCollision(false);SetActorHiddenInGame(true);SetActorTickEnabled(false);SetLifeSpan(.1f);
}
void ALiveMonster::ApplyPush(const FVector& Origin,float Distance,float Duration)
{
    if(!IsAlive()||!FMath::IsFinite(Distance)||!FMath::IsFinite(Duration)||Distance<=0||Duration<=0)return;
    FVector Direction=(GetActorLocation()-Origin).GetSafeNormal2D();if(Direction.IsNearlyZero())Direction=FVector::ForwardVector;
    PushVelocity=Direction*(Distance/FMath::Max(.05f,Duration));PushRemaining=FMath::Max(.05f,Duration);
}
void ALiveMonster::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);if(!IsAlive()||!HasAuthority()||!FMath::IsFinite(DeltaSeconds)||DeltaSeconds<=0)return;RefreshEnemyModifier();AttackCooldown=FMath::Max(0.f,AttackCooldown-DeltaSeconds);
    if(PushRemaining>0){const float Step=FMath::Min(DeltaSeconds,PushRemaining);AddActorWorldOffset(PushVelocity*Step,true);PushRemaining-=Step;return;}
    if(IsValid(FollowTarget)&&!bStationary)
    {
        const FVector Delta=FollowTarget->GetActorLocation()-GetActorLocation();
        if(Delta.SizeSquared2D()>FMath::Square(FMath::Max(1.f,AttackRange)))AddActorWorldOffset(Delta.GetSafeNormal2D()*FMath::Max(0.f,MoveSpeed)*EnemyAttributeMultiplier*DeltaSeconds,true);
        else if(AttackCooldown<=0)
        {
            AttackCooldown=FMath::Max(.1f,AttackInterval);
            if(auto* HealthComponent=FollowTarget->FindComponentByClass<UCultivationHealthComponent>())
            {
                FLiveHit Hit;Hit.Attack=GetCurrentAttack();Hit.AttackerRealm=Realm;
                const double Taken=HealthComponent->ApplyIncomingHit(Hit);
                if(Taken>0&&DebuffRandom.FRand()<DebuffChance)if(auto* Buffs=ULiveAnchorBuffSubsystem::Get(this))Buffs->ApplyRandomMonsterDebuff(DebuffRandom);
            }
        }
    }
}

#include "LiveWeaponGameMode.h"
#include "LiveWeaponSwarm.h"
#include "LiveMonster.h"
#include "LiveAnchorController.h"
#include "Cultivation/CultivationSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "EngineUtils.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
ALiveWeaponGameMode::ALiveWeaponGameMode()
{
    PrimaryActorTick.bCanEverTick=true;
    DefaultPawnClass=ACharacter::StaticClass();
    PlayerControllerClass=ALiveAnchorController::StaticClass();
    SwarmClass=ALiveWeaponSwarm::StaticClass();
    MonsterClass=ALiveMonster::StaticClass();
}
void ALiveWeaponGameMode::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if(bStarted)return;
    auto* PC=GetWorld()->GetFirstPlayerController();
    if(!PC||!PC->GetPawn())return;
    if(auto* Cultivation=GetGameInstance()?GetGameInstance()->GetSubsystem<UCultivationSubsystem>():nullptr)
    {
        Cultivation->SetAnchor(PC->GetPawn());
    }
    Swarm=GetWorld()->SpawnActor<ALiveWeaponSwarm>(SwarmClass);
    if(!Swarm)return;
    Swarm->bAnchorPersonalSword=bAnchorPersonalSword;
    Swarm->SetAnchor(PC->GetPawn());
    bStarted=true;
    for(TActorIterator<ALiveMonster> It(GetWorld());It;++It)It->FollowTarget=PC->GetPawn();
    int32 MonsterCount=InitialMonsters;
#if !UE_BUILD_SHIPPING
    FParse::Value(FCommandLine::Get(),TEXT("LiveMonsters="),MonsterCount);
#endif
    SpawnMonsters(MonsterCount,FParse::Param(FCommandLine::Get(),TEXT("LiveStress")));
    SetActorTickEnabled(false);
}
void ALiveWeaponGameMode::SpawnMonsters(int32 Count,bool bHighHealth)
{
    APawn* Pawn=GetWorld()->GetFirstPlayerController()?GetWorld()->GetFirstPlayerController()->GetPawn():nullptr;
    if(!Pawn)return;
    Count=FMath::Clamp(Count,0,100);
    for(int I=0;I<Count;++I)
    {
        const float Angle=I*2.399963f;const float Radius=900+I%5*110;
        FVector Position=Pawn->GetActorLocation()+FVector(FMath::Cos(Angle)*Radius,FMath::Sin(Angle)*Radius,0);
        FHitResult Floor;FCollisionQueryParams Params;Params.AddIgnoredActor(Pawn);
        if(GetWorld()->LineTraceSingleByObjectType(Floor,Position+FVector(0,0,200),Position-FVector(0,0,2000),FCollisionObjectQueryParams(ECC_WorldStatic),Params))Position.Z=Floor.ImpactPoint.Z+45;
        const FTransform Transform(FRotator::ZeroRotator,Position);
        auto* M=GetWorld()->SpawnActorDeferred<ALiveMonster>(MonsterClass,Transform,nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
        if(M){M->MaxHealth=bHighHealth?1000000.f:2000;M->FollowTarget=Pawn;M->FinishSpawning(Transform);}
    }
}

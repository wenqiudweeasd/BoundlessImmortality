#include "LiveAnchorController.h"
#include "LiveAnchorAbilityComponent.h"
#include "LiveWeaponGameMode.h"
#include "LiveWeaponSwarm.h"
#include "LiveMonster.h"
#include "CultivationGameplayComponent.h"
#include "QiCultivationComponent.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerInput.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputCoreTypes.h"
ALiveAnchorController::ALiveAnchorController(){bAutoManageActiveCameraTarget=false;bShowMouseCursor=true;bEnableClickEvents=true;}
void ALiveAnchorController::OnPossess(APawn* ControlledPawn)
{
    Super::OnPossess(ControlledPawn);
    if(auto* Old=ControlledPawn->FindComponentByClass<UCultivationGameplayComponent>()){Old->SetComponentTickEnabled(false);Old->Deactivate();}
    if(auto* Qi=ControlledPawn->FindComponentByClass<UQiCultivationComponent>()){Qi->bEnableKeyboard=false;Qi->StopCultivation();}
    Ability=ControlledPawn->FindComponentByClass<ULiveAnchorAbilityComponent>();if(!Ability){Ability=NewObject<ULiveAnchorAbilityComponent>(ControlledPawn);Ability->RegisterComponent();}
    const auto* LiveMode=GetWorld()->GetAuthGameMode<ALiveWeaponGameMode>();
    if(LiveMode&&!LiveMode->bOrthographicCamera)
    {
        bAutoManageActiveCameraTarget=true;SetViewTarget(ControlledPawn);
        if(USpringArmComponent* Arm=ControlledPawn->FindComponentByClass<USpringArmComponent>();Arm&&Arm!=Boom.Get()){Boom=Arm;BoomLength=Arm->TargetArmLength;}
    }
    else
    {
        LiveCamera=GetWorld()->SpawnActor<ACameraActor>();LiveCamera->GetCameraComponent()->ProjectionMode=ECameraProjectionMode::Orthographic;
        LiveCamera->GetCameraComponent()->OrthoWidth=OrthoWidth;LiveCamera->GetCameraComponent()->bConstrainAspectRatio=false;SetViewTarget(LiveCamera);
    }
    SetCameraZoom(ZoomGoal,true);
    FInputModeGameAndUI Mode;Mode.SetHideCursorDuringCapture(false);Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);SetInputMode(Mode);
}
void ALiveAnchorController::PlayerTick(float DeltaSeconds)
{
    Super::PlayerTick(DeltaSeconds);APawn* ControlledPawn=GetPawn();if(!ControlledPawn)return;
    const float X=float(IsInputKeyDown(EKeys::W))-float(IsInputKeyDown(EKeys::S));
    const float Y=float(IsInputKeyDown(EKeys::D))-float(IsInputKeyDown(EKeys::A));
    const float Yaw=LiveCamera?LiveCamera->GetActorRotation().Yaw:PlayerCameraManager?PlayerCameraManager->GetCameraRotation().Yaw:0;const FRotationMatrix Basis(FRotator(0,Yaw,0));
    ControlledPawn->AddMovementInput((Basis.GetUnitAxis(EAxis::X)*X+Basis.GetUnitAxis(EAxis::Y)*Y).GetSafeNormal());
    if(WasInputKeyJustPressed(EKeys::SpaceBar)&&Ability)Ability->TryPush();
    if(LiveCamera){const FVector Location=ControlledPawn->GetActorLocation()+FVector(-1200,0,1800);LiveCamera->SetActorLocationAndRotation(Location,(ControlledPawn->GetActorLocation()-Location).Rotation());}
    TickZoom(DeltaSeconds);
}
void ALiveAnchorController::LiveSpawnMonsters(int32 Count)
{
#if !UE_BUILD_SHIPPING
    if(auto* Mode=GetWorld()->GetAuthGameMode<ALiveWeaponGameMode>())Mode->SpawnMonsters(Count);
#endif
}
bool ALiveAnchorController::IsUsingGameplayCamera()const
{return GetPawn()&&LiveCamera&&GetViewTarget()==LiveCamera;}
void ALiveAnchorController::SetCameraZoom(float NewZoom,bool bInstant)
{
    ZoomGoal=FMath::Clamp(FMath::IsFinite(NewZoom)?NewZoom:1.f,FMath::Min(MinZoom,1.f),FMath::Max(MaxZoom,1.f));
    if(bInstant){Zoom=ZoomGoal;TickZoom(0.f);}
}
void ALiveAnchorController::TickZoom(float DeltaSeconds)
{
    int32 Notches=0;
    if(PlayerInput)
    {
        if(const FKeyState* Up=PlayerInput->GetKeyState(EKeys::MouseScrollUp))Notches+=Up->EventCounts[IE_Pressed].Num();
        if(const FKeyState* Down=PlayerInput->GetKeyState(EKeys::MouseScrollDown))Notches-=Down->EventCounts[IE_Pressed].Num();
    }
    if(Notches)SetCameraZoom(ZoomGoal*FMath::Pow(.85f,float(Notches)));
    Zoom=DeltaSeconds>0.f?FMath::FInterpTo(Zoom,ZoomGoal,DeltaSeconds,10.f):ZoomGoal;
    if(USpringArmComponent* Arm=Boom.Get();Arm&&BoomLength>0.f)Arm->TargetArmLength=BoomLength*Zoom;
    if(LiveCamera)LiveCamera->GetCameraComponent()->OrthoWidth=OrthoWidth*Zoom;
}

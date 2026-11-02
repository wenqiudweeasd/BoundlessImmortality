#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LiveWeapons/SpecialSwords/LiveSwordVfx.h"
#include "LiveSwordProjectile.generated.h"
class UNiagaraComponent;
class ALiveMonster;
class ALiveSpecialSword;

struct FLiveSwordShot
{
    FVector Velocity=FVector::ForwardVector*1800;
    float Range=1200;
    /** Hit radius around the flight segment, in cm. */
    float Radius=60;
    float Damage=1;
    bool bPierce=true;
    /** Skims the floor instead of flying straight (ground waves). */
    bool bGround=false;
    /** A diving shot flattens out once it comes down to LevelZ and sweeps on level at that height. */
    bool bLevelOut=false;
    float LevelZ=0;
    float Knockback=0;
    FLinearColor Color=FLinearColor::White;
    float Size=120;
    LiveSwordVfx::EFx HitFx=LiveSwordVfx::EFx::Burst;
    float HitSize=60;
    FName SkillId;
    /** Set by the launching sword so the sword-qi and its hits match the form's art style. */
    LiveSwordVfx::EStyle Style=LiveSwordVfx::EStyle::Ink;
};

/** Sword-qi projectile: a Niagara crescent that sweeps its path every frame and damages through its owner. */
UCLASS(NotBlueprintable)
class BOUNDLESSIMMORTALITY_API ALiveSwordProjectile : public AActor
{
    GENERATED_BODY()
public:
    ALiveSwordProjectile();
    void Launch(ALiveSpecialSword* InOwner,const FLiveSwordShot& InShot);
    virtual void Tick(float DeltaSeconds)override;
    int32 GetHitCount()const{return HitCount;}
private:
    UPROPERTY(Transient) TObjectPtr<UNiagaraComponent> Body;
    TWeakObjectPtr<ALiveSpecialSword> OwnerSword;
    TArray<TWeakObjectPtr<ALiveMonster>> AlreadyHit;
    FLiveSwordShot Shot;
    float Travelled=0,FloorZ=0;
    int32 HitCount=0;
    bool bFinished=false;
    void Finish();
};

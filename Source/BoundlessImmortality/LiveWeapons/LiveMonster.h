#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LiveMonster.generated.h"
class USphereComponent;
class UStaticMeshComponent;
class APawn;
struct FLiveHit;
struct FLiveHitResult;
UCLASS(Blueprintable)
class BOUNDLESSIMMORTALITY_API ALiveMonster : public AActor
{
    GENERATED_BODY()
public:
    ALiveMonster();
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TObjectPtr<USphereComponent> Collision;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> Body;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,meta=(ClampMin="1")) float MaxHealth=100;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly) float Health=100;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,meta=(ClampMin="0")) float MoveSpeed=85;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Combat",meta=(ClampMin="0")) float AttackDamage=8;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Combat",meta=(ClampMin="0")) float Defense=0;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Combat",meta=(ClampMin="0.1")) float AttackInterval=1.5f;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Combat",meta=(ClampMin="1")) float AttackRange=115;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Combat") float EnemyAttributeMultiplier=1;
    /** 境界序号 for the world-trial suppression of lower-realm attackers; -1 (default) means no realm and no suppression. */
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Combat",meta=(ClampMin="-1",ClampMax="33")) int32 Realm=-1;
    UPROPERTY(EditAnywhere,BlueprintReadWrite) TObjectPtr<APawn> FollowTarget;
    UFUNCTION(BlueprintPure) bool IsAlive()const{return Health>0 && !IsActorBeingDestroyed();}
    UFUNCTION(BlueprintPure) float GetHealthFraction()const{return MaxHealth>0?FMath::Clamp(Health/(MaxHealth*FMath::Max(.01f,EnemyAttributeMultiplier)),0.f,1.f):0.f;}
    /** A plain strike through the core formula with Amount as the attack stat (招式系数 1). */
    UFUNCTION(BlueprintCallable) bool ApplyWeaponDamage(float Amount,const FString& ParticipantId);
    /** A sword strike through the core formula; false when the target ignores it. Authority mirrors submit it instead. */
    virtual bool ApplyWeaponHit(const FLiveHit& Hit,const FString& ParticipantId);
    /**
     * Resolves a hit with the core formula (LiveCombat) against this monster's defense and realm and removes the
     * health; returns the health removed. A missing attacker realm is read from the participant's cultivation.
     */
    float ApplyHit(const FLiveHit& Hit,const FString& ParticipantId,FRandomStream* Random=nullptr,FLiveHitResult* OutResult=nullptr);
    /** Instant consumption by a soul-devouring sword: credited as a kill to the participant. */
    bool Devour(const FString& ParticipantId);
    float GetCurrentMaxHealth()const{return MaxHealth*FMath::Max(.01f,EnemyAttributeMultiplier);}
    float GetCurrentAttack()const{return AttackDamage*FMath::Max(.01f,EnemyAttributeMultiplier);}
    float GetCurrentDefense()const{return Defense*FMath::Max(.01f,EnemyAttributeMultiplier);}
    /** Chance for each strike on the anchor to inflict poison, burn or armour break. */
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Combat",meta=(ClampMin="0",ClampMax="1")) float DebuffChance=.15f;
    /** Stationary targets (world obstacles, race markers) never walk toward the anchor. */
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Combat") bool bStationary=false;
    UFUNCTION(BlueprintCallable) void ApplyPush(const FVector& Origin,float Distance,float Duration);
    /** Whether devouring this target can grant permanent growth (噬魂剑); training dummies opt out. */
    virtual bool GrantsGrowth()const{return true;}
    /** Where health bars hang over this monster. */
    virtual FVector GetHealthBarLocation()const{return GetActorLocation()+FVector(0,0,65);}
    /** The ground under it, where strike scorches and burning flames belong (the bottom of the collision sphere). */
    virtual FVector GetFootLocation()const;
    UFUNCTION(BlueprintImplementableEvent) void OnDamaged(float Amount,const FString& ParticipantId);
    UFUNCTION(BlueprintImplementableEvent) void OnDeath(const FString& ParticipantId);
    virtual void Tick(float DeltaSeconds)override;
protected:
    virtual void BeginPlay()override;
    virtual void EndPlay(const EEndPlayReason::Type Reason)override;
    /** Lethal damage: the monster leaves the world. Training dummies override this to stand back up. */
    virtual void Die(const FString& ParticipantId);
private:
    FVector PushVelocity=FVector::ZeroVector;
    float PushRemaining=0;
    float AttackCooldown=0;
    void RefreshEnemyModifier();
    float FinishDamage(float Applied,const FString& Id);
    FRandomStream DebuffRandom{4417};
};

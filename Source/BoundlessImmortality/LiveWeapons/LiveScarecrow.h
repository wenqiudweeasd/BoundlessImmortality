#pragma once
#include "CoreMinimal.h"
#include "LiveMonster.h"
#include "LiveScarecrow.generated.h"
class UTextRenderComponent;
class UMaterialInterface;
/**
 * Training dummy for the sword test range. It never moves or attacks, springs back to its post after pushes and stands
 * up again RespawnDelay seconds after a kill or devour, so every hit, death and devour effect can be replayed.
 * The actor sits at chest height, where sword hits land; the figure stands on the ground FootDepth below.
 * With a RealmPreset it is a same-realm cultivator of that realm (design v1.4 §3: 生命 1000×(1+0.15r), 攻击 10×(1+0.12r),
 * 防御 20×(1+0.10r)) and carries the realm, so lower-realm swords are suppressed by the core damage formula; such a dummy
 * also feeds 噬魂 devour growth. Without one it keeps its own (100M health, no defense) and grants nothing.
 */
UCLASS(Blueprintable)
class BOUNDLESSIMMORTALITY_API ALiveScarecrow : public ALiveMonster
{
    GENERATED_BODY()
public:
    ALiveScarecrow();
    static constexpr float FootDepth=95.f;
    /** Realm index whose cultivator attributes the dummy takes at BeginPlay; -1 keeps MaxHealth, Defense and Realm as set. */
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Scarecrow",meta=(ClampMin="-1",ClampMax="33")) int32 RealmPreset=-1;
    /** Same-realm cultivator attributes at a realm index. */
    static float RealmHealth(int32 Realm){return 1000.f*(1.f+.15f*FMath::Clamp(Realm,0,33));}
    static float RealmAttack(int32 Realm){return 10.f*(1.f+.12f*FMath::Clamp(Realm,0,33));}
    static float RealmDefense(int32 Realm){return 20.f*(1.f+.10f*FMath::Clamp(Realm,0,33));}
    /** Test hook: sets the health to this share of the maximum (the 残血 button: 0.14 lets 噬魂 devour it). */
    UFUNCTION(BlueprintCallable,Category="Scarecrow") void SetHealthFraction(float Fraction);
    /** Full health and cleared readouts. */
    UFUNCTION(BlueprintCallable,Category="Scarecrow") void Restore();
    /** Pivot at the foot of the post; tips over while knocked down and shakes on hits. */
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Scarecrow") TObjectPtr<USceneComponent> Figure;
    /** Damage per second and running total, turned toward the camera. */
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Scarecrow") TObjectPtr<UTextRenderComponent> Readout;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Scarecrow",meta=(ClampMin="0.3")) float RespawnDelay=2.f;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Scarecrow",meta=(ClampMin="0.5")) float DpsWindow=3.f;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Scarecrow") bool bShowReadout=true;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Scarecrow") float TotalDamage=0;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Scarecrow") int32 Knockdowns=0;
    UFUNCTION(BlueprintPure,Category="Scarecrow") float GetDamagePerSecond()const;
    UFUNCTION(BlueprintPure,Category="Scarecrow") bool IsKnockedDown()const{return DownTime>0;}
    UFUNCTION(BlueprintCallable,Category="Scarecrow") void ResetStats();
    /** Just above the hat, under the readout. */
    virtual FVector GetHealthBarLocation()const override{return GetActorLocation()+FVector(0,0,214-FootDepth);}
    virtual FVector GetFootLocation()const override{return GetActorLocation()-FVector(0,0,FootDepth);}
    /** A plain dummy's 100M health would turn one devour into millions of permanent attack; a realm dummy has realm stats. */
    virtual bool GrantsGrowth()const override{return RealmPreset>=0;}
    virtual void Tick(float DeltaSeconds)override;
protected:
    virtual void BeginPlay()override;
    virtual void Die(const FString& ParticipantId)override;
private:
    /** Every visible part; tinted through custom primitive data slot 0 by M_Scarecrow. */
    UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Parts;
    FVector Home=FVector::ZeroVector,LastPlaced=FVector::ZeroVector;
    float LastHealth=0,DownTime=0,Settle=0,Wobble=0,WobblePhase=0;
    TArray<TPair<double,float>> Hits;
    void StandUp();
    void UpdateReadout();
};

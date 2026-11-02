#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CultivationHealthComponent.generated.h"
class UCultivationSubsystem;
struct FLiveHit;
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FCultivationHealthChanged,double,Health,double,MaxHealth);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FCultivationAnchorDied);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FCultivationDamageApplied,double,Applied,double,Maximum);
UCLASS(ClassGroup=(Cultivation),meta=(BlueprintSpawnableComponent))
class BOUNDLESSIMMORTALITY_API UCultivationHealthComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UCultivationHealthComponent();
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly) double Health=100;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly) double MaxHealth=100;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly) bool bDead=false;
    UPROPERTY(BlueprintAssignable) FCultivationHealthChanged OnHealthChanged;
    UPROPERTY(BlueprintAssignable) FCultivationAnchorDied OnDied;
    UPROPERTY(BlueprintAssignable) FCultivationDamageApplied OnDamageApplied;
    /** A hit with Amount as the attacker's attack stat; see ApplyIncomingHit. */
    UFUNCTION(BlueprintCallable) double ApplyIncomingDamage(double Amount);
    /**
     * A monster hit through the core formula: immunity and evasion first, then A²/(A+D_eff) against the anchor's defense
     * (machine-soul stacks raise it) with the guard reduction, together capped at 60% removed, then shields absorb.
     */
    double ApplyIncomingHit(const FLiveHit& Hit);
    UFUNCTION(BlueprintCallable) double Heal(double Amount);
    /** Health loss that bypasses defense, evasion and shields (damage over time). */
    UFUNCTION(BlueprintCallable) double ApplyDirectDamage(double Amount);
    /**
     * Heavenly damage (天劫落雷): no immunity, evasion, defense or reduction, but shields still absorb it. It never takes
     * health below MinHealthFraction of the maximum (0 lets it kill). Without bStarWard the 七星 guard does nothing against
     * it: its light shield is passed over and no 草莓甜点 star healing answers the bolt (a broken 七星挡劫). Returns the
     * health removed.
     */
    UFUNCTION(BlueprintCallable) double ApplyUnavoidableDamage(double Amount,double MinHealthFraction,bool bStarWard=true);
    UFUNCTION(BlueprintCallable) void Revive();
    void ApplyWulingHealth(double Current,double Maximum);
    UFUNCTION(BlueprintPure) double GetHealthFraction()const{return MaxHealth>0?FMath::Clamp(Health/MaxHealth,0.0,1.0):0;}
    virtual void TickComponent(float Delta,ELevelTick Type,FActorComponentTickFunction* Function)override;
private:
    UCultivationSubsystem* Cultivation()const;
    FRandomStream Random{68147};
};

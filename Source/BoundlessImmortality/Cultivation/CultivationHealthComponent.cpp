#include "CultivationHealthComponent.h"
#include "CultivationSubsystem.h"
#include "LiveWeapons/SpecialSwords/LiveAnchorBuffSubsystem.h"
#include "Combat/LiveCombat.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

UCultivationHealthComponent::UCultivationHealthComponent(){PrimaryComponentTick.bCanEverTick=true;PrimaryComponentTick.TickInterval=.1f;}
UCultivationSubsystem* UCultivationHealthComponent::Cultivation()const{return GetWorld()&&GetWorld()->GetGameInstance()?GetWorld()->GetGameInstance()->GetSubsystem<UCultivationSubsystem>():nullptr;}
double UCultivationHealthComponent::ApplyIncomingDamage(double Amount)
{
    FLiveHit Hit;Hit.Attack=FMath::IsFinite(Amount)?float(FMath::Min(Amount,1.e30)):0.f;return ApplyIncomingHit(Hit);
}
double UCultivationHealthComponent::ApplyIncomingHit(const FLiveHit& Hit)
{
    if(bDead||!FMath::IsFinite(Hit.Attack)||Hit.Attack<=0)return 0;
    auto* C=Cultivation();if(C&&C->IsWulingAuthorityEnabled())return 0;double Defense=0;int32 Realm=-1;
    if(C)
    {
        const auto Snapshot=C->GetParticipantSnapshot(C->GetAnchorId());if(!Snapshot.bPresent||C->GetAnchorHealth()!=this)return 0;
        const auto A=Snapshot.Attributes;
        const double DesiredMax=FMath::Max(1.0,A.MaxHealth);
        if(!FMath::IsNearlyEqual(DesiredMax,MaxHealth)){const double Fraction=GetHealthFraction();MaxHealth=DesiredMax;Health=MaxHealth*Fraction;}
        if(Random.FRand()<C->GetAnchorImmunityChance()||Random.FRand()<A.EvasionChance)return 0;
        Defense=FMath::Max(0.0,A.Defense);Realm=Snapshot.RealmIndex;
    }
    // Sword-spirit buffs: machine-soul defense and the method guard reduction share the 60% cap; the seven-star shield absorbs after.
    auto* Buffs=ULiveAnchorBuffSubsystem::Get(this);
    if(Buffs)Defense*=Buffs->GetDefenseMultiplier();
    double Damage=LiveCombat::Resolve(Hit,float(FMath::Min(Defense,1.e30)),Realm,Buffs?Buffs->GetDamageReduction():0.f).Damage;
    // 每日一签 凶: the day's omen raises what lands, before shields take their share.
    if(Buffs)Damage*=Buffs->GetDamageTakenMultiplier();
    if(Buffs){Damage=Buffs->AbsorbDamage(Damage);if(Damage<=0){OnHealthChanged.Broadcast(Health,MaxHealth);return 0;}}
    const double Applied=FMath::Min(Health,Damage);Health=FMath::Max(0.0,Health-Damage);
    if(Applied>0)OnDamageApplied.Broadcast(Applied,MaxHealth);
    if(Health<=0){bDead=true;OnDied.Broadcast();}
    else if(C&&Applied>0){const double Fraction=C->ConsumeStarHealing(GetHealthFraction());if(Fraction>0)Heal(MaxHealth*Fraction);}
    OnHealthChanged.Broadcast(Health,MaxHealth);return Applied;
}
double UCultivationHealthComponent::ApplyUnavoidableDamage(double Amount,double MinHealthFraction,bool bStarWard)
{
    auto* C=Cultivation();if(C&&C->IsWulingAuthorityEnabled())return 0;
    if(bDead||!FMath::IsFinite(Amount)||Amount<=0)return 0;
    double Damage=Amount;
    if(auto* Buffs=ULiveAnchorBuffSubsystem::Get(this)){Damage*=Buffs->GetDamageTakenMultiplier();Damage=Buffs->AbsorbDamage(Damage,bStarWard);if(Damage<=0){OnHealthChanged.Broadcast(Health,MaxHealth);return 0;}}
    const double Floor=MaxHealth*(FMath::IsFinite(MinHealthFraction)?FMath::Clamp(MinHealthFraction,0.0,1.0):0.0);
    const double Applied=FMath::Clamp(FMath::Min(Damage,Health-Floor),0.0,Health);
    if(Applied<=0)return 0;
    Health-=Applied;OnDamageApplied.Broadcast(Applied,MaxHealth);
    if(Health<=0){Health=0;bDead=true;OnDied.Broadcast();}
    else if(C&&bStarWard){const double Fraction=C->ConsumeStarHealing(GetHealthFraction());if(Fraction>0)Heal(MaxHealth*Fraction);}
    OnHealthChanged.Broadcast(Health,MaxHealth);return Applied;
}
double UCultivationHealthComponent::Heal(double Amount)
{
    if(auto* C=Cultivation();C&&C->IsWulingAuthorityEnabled())return 0;
    if(bDead||!FMath::IsFinite(Amount)||Amount<=0)return 0;const double Before=Health;Health=FMath::Min(MaxHealth,Health+Amount);if(Health!=Before)OnHealthChanged.Broadcast(Health,MaxHealth);return Health-Before;
}
double UCultivationHealthComponent::ApplyDirectDamage(double Amount)
{
    if(auto* C=Cultivation();C&&C->IsWulingAuthorityEnabled())return 0;
    if(bDead||!FMath::IsFinite(Amount)||Amount<=0)return 0;
    const double Applied=FMath::Min(Health,Amount);Health=FMath::Max(0.0,Health-Amount);
    if(Applied>0)OnDamageApplied.Broadcast(Applied,MaxHealth);
    if(Health<=0){bDead=true;OnDied.Broadcast();}
    OnHealthChanged.Broadcast(Health,MaxHealth);return Applied;
}
void UCultivationHealthComponent::Revive(){bDead=false;Health=MaxHealth;OnHealthChanged.Broadcast(Health,MaxHealth);}
void UCultivationHealthComponent::TickComponent(float Delta,ELevelTick Type,FActorComponentTickFunction* Function)
{
    Super::TickComponent(Delta,Type,Function);auto* C=Cultivation();if(!C||Delta<=0||C->IsWulingAuthorityEnabled())return;
    const auto S=C->GetParticipantSnapshot(C->GetAnchorId());const double NewMax=FMath::Max(1.0,S.Attributes.MaxHealth);
    if(!FMath::IsNearlyEqual(NewMax,MaxHealth)){const double Fraction=GetHealthFraction();MaxHealth=NewMax;Health=bDead?0:MaxHealth*Fraction;OnHealthChanged.Broadcast(Health,MaxHealth);}
    if(!bDead&&S.bPresent)Heal(FMath::Max(0.0,S.Attributes.HealthRegeneration)*Delta);
}
void UCultivationHealthComponent::ApplyWulingHealth(double Current,double Maximum)
{
    auto* C=Cultivation();if(!C||!C->IsWulingAuthorityEnabled()||!FMath::IsFinite(Current)||!FMath::IsFinite(Maximum)||Maximum<=0)return;
    const double Before=Health;MaxHealth=Maximum;Health=FMath::Clamp(Current,0.0,MaxHealth);bDead=Health<=0;
    if(Before>Health)OnDamageApplied.Broadcast(Before-Health,MaxHealth);OnHealthChanged.Broadcast(Health,MaxHealth);
}

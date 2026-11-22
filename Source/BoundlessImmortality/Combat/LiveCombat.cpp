#include "Combat/LiveCombat.h"

namespace LiveCombat
{
    int32 MajorRealm(int32 RealmIndex)
    {
        static constexpr int32 Starts[]={0,9,13,16,22,23,24,26,31,32,33};
        RealmIndex=FMath::Clamp(RealmIndex,0,33);
        for(int32 I=UE_ARRAY_COUNT(Starts)-1;I>0;--I)if(RealmIndex>=Starts[I])return I;
        return 0;
    }
    float EffectiveDefense(float Defense,float ArmorPenetration,float DefenseIgnore)
    {
        if(!FMath::IsFinite(Defense)||Defense<=0)return 0;
        const float Pen=FMath::IsFinite(ArmorPenetration)?FMath::Clamp(ArmorPenetration,0.f,MaxArmorPenetration):0.f;
        const float Ignore=FMath::IsFinite(DefenseIgnore)?FMath::Clamp(DefenseIgnore,0.f,1.f):0.f;
        return FMath::Max(0.f,Defense*(1-Ignore)*(1-Pen));
    }
    float Mitigation(float Attack,float Defense,float Reduction)
    {
        const float Kept=Attack>0&&Defense>0?Attack/(Attack+Defense):1.f;
        const float Buff=FMath::IsFinite(Reduction)?FMath::Clamp(Reduction,0.f,1.f):0.f;
        return FMath::Max(1-MaxDamageReduction,Kept*(1-Buff));
    }
    float RealmFactor(int32 AttackerRealm,int32 DefenderRealm)
    {
        if(AttackerRealm<0||DefenderRealm<0||AttackerRealm>=DefenderRealm)return 1;
        const int32 MajorGap=MajorRealm(DefenderRealm)-MajorRealm(AttackerRealm);
        if(MajorGap<=0)return FMath::Max(.70f,1-.03f*(DefenderRealm-AttackerRealm));
        return FMath::Max(.25f,FMath::Pow(.8f,float(MajorGap)));
    }
    FLiveHitResult Resolve(const FLiveHit& Hit,float Defense,int32 DefenderRealm,float Reduction,FRandomStream* Random)
    {
        FLiveHitResult R;
        const auto Finite=[](float V){return FMath::IsFinite(V)?FMath::Max(0.f,V):0.f;};
        const float A=Finite(Hit.Attack),Coefficient=Finite(Hit.SkillCoefficient);
        R.EffectiveDefense=EffectiveDefense(Defense,Hit.ArmorPenetration,Hit.DefenseIgnore);
        R.Mitigation=Mitigation(A,R.EffectiveDefense,Reduction);
        R.RealmFactor=RealmFactor(Hit.AttackerRealm,DefenderRealm);
        const float Chance=FMath::IsFinite(Hit.CriticalChance)?FMath::Clamp(Hit.CriticalChance,0.f,1.f):0.f;
        R.bCritical=Random&&Chance>0&&A*Coefficient>0&&Random->FRand()<Chance;
        const float Critical=R.bCritical?FMath::Max(1.f,Finite(Hit.CriticalDamage)):1.f;
        const float Bonus=FMath::IsFinite(Hit.DamageBonus)?FMath::Max(-1.f,Hit.DamageBonus):0.f;
        // A²/(A+D_eff) is A × A/(A+D_eff): the defense share depends on the attack stat, not on the skill multiple.
        const float Formula=FMath::Min(1.e30f,A*R.Mitigation*Coefficient*(1+Bonus)*Critical*R.RealmFactor);
        R.TrueDamage=FMath::Min(1.e30f,Finite(Hit.TrueDamage)+Finite(Hit.TrueDamageRatio)*Formula);
        R.Damage=Formula+R.TrueDamage;
        return R;
    }
}

FLiveHitResult ULiveCombatLibrary::ResolveHit(const FLiveHit& Hit,float Defense,int32 DefenderRealm,float Reduction,bool bForceCritical)
{
    FLiveHit Copy=Hit;Copy.CriticalChance=bForceCritical?1.f:0.f;
    FRandomStream Random(1);
    return LiveCombat::Resolve(Copy,Defense,DefenderRealm,Reduction,&Random);
}

float ULiveCombatLibrary::RealmFactor(int32 AttackerRealm,int32 DefenderRealm){return LiveCombat::RealmFactor(AttackerRealm,DefenderRealm);}

#pragma once
#include "CoreMinimal.h"
#include "LiveWeapons/SpecialSwords/LiveSpecialSword.h"
#include "LiveSpecialSwordKinds.generated.h"

/**
 * 噬魂剑: devours low-health enemies (a violet serpent swallows them and the blade drinks their essence, taking 1% of their
 * attributes and 5% of their health as attack), splits into chasing clones, plunges for eight ground waves. Whatever it
 * strikes burns with violet fire for a while (a look only, no damage over time).
 */
UCLASS()
class BOUNDLESSIMMORTALITY_API ALiveSwordSoulEater : public ALiveSpecialSword
{
    GENERATED_BODY()
public:
    ALiveSwordSoulEater(){Kind=ELiveSpecialSword::SoulEater;}
    virtual void EndPlay(const EEndPlayReason::Type Reason)override;
    virtual float GetMeterFraction()const override;
    virtual FText GetMeterLabel()const override;
    /** Clone count rule: attack / (base attack × 0.5), clamped to 5..20. */
    int32 CloneCount()const;
    /** Test hook: sends the serpent at a living target now, whatever its health; false when it is already being devoured. */
    UFUNCTION(BlueprintCallable,Category="特殊剑灵") bool DevourNow(ALiveMonster* Target);
    /** Devours whose essence has not reached the blade yet. */
    UFUNCTION(BlueprintPure,Category="特殊剑灵") int32 GetDevoursInFlight()const{return Devours.Num();}
    /** Enemies burning with its violet fire right now. */
    UFUNCTION(BlueprintPure,Category="特殊剑灵") int32 GetBurning()const{return Burns.Num();}
    /** Seconds a struck enemy keeps burning after its last hit. */
    static constexpr float BurnSeconds=2.5f;
protected:
    virtual void SetupForm()override;
    virtual TArray<FName> ActiveSkills()const override{return {TEXT("Clones"),TEXT("GroundWave")};}
    virtual bool CanCast(FName Skill)const override;
    virtual void BeginCast(FName Skill)override;
    virtual void TickCast(float DeltaSeconds)override;
    virtual void TickKind(float DeltaSeconds)override;
    virtual void AfterHit(ALiveMonster* Target,float Applied,bool bKilled,FName SkillId,bool bAllowFollowUp)override;
    virtual float GrowthAttack()const override;
private:
    int32 Phase=0;
    FVector Spot=FVector::ZeroVector;
    UPROPERTY(Transient) TObjectPtr<class UNiagaraComponent> ChargeFx;
    /** A devour in flight: the serpent bites at LiveSwordVfx::DevourBite, the essence reaches the blade at DevourAbsorb. */
    struct FDevour
    {
        TWeakObjectPtr<ALiveMonster> Target;
        TWeakObjectPtr<class UNiagaraComponent> Fx;
        FVector At=FVector::ZeroVector;
        float Time=0;
        bool bBitten=false,bGrows=false;
        LiveSpecialSwords::FDevourGain Gain;
    };
    TArray<FDevour> Devours;
    /** One fire per struck enemy, fed by every hit; it burns down BurnSeconds after the last. */
    struct FBurn{TWeakObjectPtr<ALiveMonster> Target;TWeakObjectPtr<class UNiagaraComponent> Fx;float Remaining=0;};
    TArray<FBurn> Burns;
    void Ignite(ALiveMonster* Target);
    void BeginDevour(ALiveMonster* Target);
    void TickClones(float DeltaSeconds);
    void TickGroundWave(float DeltaSeconds);
};

/** 无机态那: machine-soul stacks harden the anchor and become a giant axe; boomerang spin; sci-fi qi barrage. */
UCLASS(NotBlueprintable)
class BOUNDLESSIMMORTALITY_API ALiveSwordInorganic : public ALiveSpecialSword
{
    GENERATED_BODY()
public:
    ALiveSwordInorganic();
    virtual float GetMeterFraction()const override{return Stacks/5.f;}
    virtual FText GetMeterLabel()const override;
    int32 GetStacks()const{return Stacks;}
protected:
    virtual void SetupForm()override;
    virtual TArray<FName> ActiveSkills()const override{return {TEXT("Spin"),TEXT("Barrage")};}
    virtual FName PriorityCast()const override{return Stacks>=5?FName(TEXT("AxeSlam")):NAME_None;}
    virtual void BeginCast(FName Skill)override;
    virtual void TickCast(float DeltaSeconds)override;
    virtual void OnNormalHit(ALiveMonster* Target)override;
    virtual float DamageBonus()const override;
    virtual void EndPlay(const EEndPlayReason::Type Reason)override;
public:
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> AxeForm;
private:
    int32 Stacks=0,Phase=0,BarrageIndex=0;
    float ShotTimer=0,SpinAngle=0;
    FVector Spot=FVector::ZeroVector;
    TMap<TWeakObjectPtr<ALiveMonster>,float> SpinHits;
    UPROPERTY(Transient) TObjectPtr<class UNiagaraComponent> SpinFx;
    void TickAxe(float DeltaSeconds);
    void TickSpin(float DeltaSeconds);
    void TickBarrage(float DeltaSeconds);
    void ShowAxe(bool bShow);
};

/** 驱煞神刃: dispels and heals the anchor every 10 s; two clones fill a store released as one huge strike. */
UCLASS(NotBlueprintable)
class BOUNDLESSIMMORTALITY_API ALiveSwordDivineBlade : public ALiveSpecialSword
{
    GENERATED_BODY()
public:
    ALiveSwordDivineBlade(){Kind=ELiveSpecialSword::DivineBlade;}
    virtual float GetMeterFraction()const override;
    virtual FText GetMeterLabel()const override;
    float GetStore()const{return Store;}
    float GetStoreCapacity()const{return AttackPower()*12.f;}
protected:
    virtual void SetupForm()override;
    virtual TArray<FName> ActiveSkills()const override{return {TEXT("Twin")};}
    virtual bool CanCast(FName Skill)const override{return !bTwinActive;}
    virtual void BeginCast(FName Skill)override;
    virtual void TickCast(float DeltaSeconds)override;
    virtual void TickKind(float DeltaSeconds)override;
    virtual void AfterHit(ALiveMonster* Target,float Applied,bool bKilled,FName SkillId,bool bAllowFollowUp)override;
private:
    bool bTwinActive=false,bMerging=false;
    float Store=0,DispelTimer=4,BlessTimer=7;
    int32 Phase=0;
    UPROPERTY(Transient) TObjectPtr<class UNiagaraComponent> ChargeFx;
    void Purify(const FVector& From,float& Dispel,float& Bless,float DeltaSeconds);
    void TickTwins(float DeltaSeconds);
};

/**
 * 七星光芒剑: seven-colour lightning, guards the anchor at every lost seventh of health (and against a tribulation's backlash
 * when its realm is high enough, 七星挡劫), five-second radiant qi that may call down three lightning strikes.
 */
UCLASS(NotBlueprintable)
class BOUNDLESSIMMORTALITY_API ALiveSwordSevenStar : public ALiveSpecialSword
{
    GENERATED_BODY()
public:
    ALiveSwordSevenStar(){Kind=ELiveSpecialSword::SevenStar;}
    virtual float GetMeterFraction()const override;
    virtual FText GetMeterLabel()const override;
    bool IsGuarding()const{return Mode==EMode::Guard;}
    /** Test hook: what the guardian passive would do right now. */
    void ForceGuard(){TriggerGuard();}
    /** Lightning strikes 光芒剑气 has called down since the sword appeared. */
    UFUNCTION(BlueprintPure,Category="特殊剑灵") int32 GetRadianceStrikes()const{return StrikesCalled;}
    /** Test hook: the next 光芒剑气 calls its strikes down (1), does not (0), or rolls the 45% (-1). */
    UFUNCTION(BlueprintCallable,Category="特殊剑灵") void DebugForceStrikeRoll(int32 Outcome){ForcedStrikeRoll=Outcome;}
protected:
    virtual void SetupForm()override;
    virtual TArray<FName> ActiveSkills()const override{return {TEXT("Lightning"),TEXT("Radiance")};}
    virtual bool CanCast(FName Skill)const override;
    virtual void BeginCast(FName Skill)override;
    virtual void TickCast(float DeltaSeconds)override;
    virtual void TickKind(float DeltaSeconds)override;
    virtual void TickGuard(float DeltaSeconds)override;
    virtual bool SuppressAttacks()const override{return Mode==EMode::Guard;}
    virtual void ModifyHit(ALiveMonster* Target,FLiveHit& Hit,FName SkillId)override;
    virtual void AfterHit(ALiveMonster* Target,float Applied,bool bKilled,FName SkillId,bool bAllowFollowUp)override;
    virtual void BeginPlay()override;
    virtual void EndPlay(const EEndPlayReason::Type Reason)override;
private:
    int32 Phase=0,Fired=0,LastSeventh=-1,StrikesCalled=0;
    float GuardRemaining=0,BoltTimer=0;
    TArray<TWeakObjectPtr<ALiveMonster>> BoltTargets;
    /** The sword qi of the last 光芒剑气 rolls once, on its first hit, and its strikes fall on what it has hit. */
    bool bRadianceRolled=true;
    int32 ForcedStrikeRoll=-1;
    TArray<TWeakObjectPtr<ALiveMonster>> RadianceHits;
    struct FStrike{float Delay=0;int32 Index=0;};
    TArray<FStrike> PendingStrikes;
    /** EXianTuWard of the backlash last seen (0 none): the guard answers when the heavens test it. */
    uint8 LastWard=0;
    FDelegateHandle ShieldHandle;
    UPROPERTY(Transient) TObjectPtr<class UNiagaraComponent> ChargeFx;
    UPROPERTY(Transient) TObjectPtr<class UNiagaraComponent> ShieldFx;
    void TriggerGuard(bool bAnnounce=true);
    void EndGuard(bool bBroken);
    void TickLightning(float DeltaSeconds);
    void TickRadiance(float DeltaSeconds);
    void TickWard();
    /** One sky strike on the target: 80% attack plus 5% of its current health, like each of the seven bolts. */
    void CallStrike(ALiveMonster* Target,int32 Index);
};

/** 弑神剑: ignores half (or three quarters) of defense, adds 5% true damage, often strikes twice. */
UCLASS(NotBlueprintable)
class BOUNDLESSIMMORTALITY_API ALiveSwordGodslayer : public ALiveSpecialSword
{
    GENERATED_BODY()
public:
    ALiveSwordGodslayer(){Kind=ELiveSpecialSword::Godslayer;}
    int32 GetFollowUps()const{return FollowUps;}
protected:
    virtual void SetupForm()override;
    virtual void TickKind(float DeltaSeconds)override;
    virtual void ModifyHit(ALiveMonster* Target,FLiveHit& Hit,FName SkillId)override;
    virtual void AfterHit(ALiveMonster* Target,float Applied,bool bKilled,FName SkillId,bool bAllowFollowUp)override;
private:
    struct FPending{TWeakObjectPtr<ALiveMonster> Target;float Amount=0;float Delay=0;};
    TArray<FPending> Pending;
    float LastFraction=1.f,LastRaw=0;
    int32 FollowUps=0;
};

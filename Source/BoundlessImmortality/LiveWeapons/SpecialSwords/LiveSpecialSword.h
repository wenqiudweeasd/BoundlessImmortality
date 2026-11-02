#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LiveWeapons/SpecialSwords/LiveSpecialSwordTypes.h"
#include "LiveWeapons/SpecialSwords/LiveSwordVfx.h"
#include "Combat/LiveCombat.h"
#include "LiveSpecialSword.generated.h"
class UStaticMeshComponent;
class USkeletalMeshComponent;
class UNiagaraComponent;
class UStaticMesh;
class USkeletalMesh;
class UAnimSequence;
class UMaterialInterface;
class ALiveMonster;
class ALiveWeaponSwarm;
class ULiveSpecialSwordComponent;
class ULiveAnchorBuffSubsystem;
class APawn;
struct FLiveSwordShot;

/**
 * A gift-unlocked sword spirit that flies and fights on its own instead of inside the instanced swarm.
 * Skill rule shared by every form: whenever a skill is off cooldown it casts a random ready one; after a
 * cast it has a 50% chance to strike normally and a 50% chance to chain straight into another ready skill.
 */
UCLASS(Abstract,NotBlueprintable)
class BOUNDLESSIMMORTALITY_API ALiveSpecialSword : public AActor
{
    GENERATED_BODY()
public:
    ALiveSpecialSword();
    static TSubclassOf<ALiveSpecialSword> ClassFor(ELiveSpecialSword Kind);
    void InitializeSword(ULiveSpecialSwordComponent* InManager,ALiveWeaponSwarm* InSwarm,const FString& InParticipantId,const FString& InDisplayName);

    ELiveSpecialSword GetKind()const{return Kind;}
    const FString& GetParticipantId()const{return ParticipantId;}
    const FString& GetDisplayName()const{return DisplayName;}
    /**
     * Current attack (A of the damage formula): the viewer's sword attack (attack + spirit damage) scaled by this form's
     * attribute sheet at the viewer's realm, plus the form's permanent growth (devoured attack).
     */
    float AttackPower()const;
    /** Attack before devoured growth; the clone formula divides by it. */
    float BaseAttackPower()const;
    /**
     * Routes every hit of this sword, its clones and its projectiles through the form's passives and the core damage
     * formula. Amount is the skill's damage as a multiple of AttackPower(): A stays the attack, Amount/A the 招式系数.
     */
    float DealDamage(ALiveMonster* Target,float Amount,FName SkillId,const FVector& HitPoint,bool bAllowFollowUp=true);

    virtual void GetSkillStates(TArray<FLiveSpecialSkillState>& Out)const;
    /** Overhead meter 0..1 (charge, store, stacks); negative hides it. */
    virtual float GetMeterFraction()const{return -1.f;}
    virtual FText GetMeterLabel()const{return FText::GetEmpty();}
    FText GetStatusText()const;
    FVector GetOverheadLocation()const;

    struct FFloater{FString Text;FLinearColor Color=FLinearColor::White;FVector Location=FVector::ZeroVector;float Age=0;};
    void PushFloater(const FString& Text,const FLinearColor& Color,const FVector& At);
    const TArray<FFloater>& GetFloaters()const{return Floaters;}
    /**
     * Damage rising from above the target's health bar; the special-sword overlay paints it. Rapid non-critical hits on
     * one target add into the open number (it re-pops instead of stacking); Drift spreads numbers sideways (-1..1).
     */
    struct FDamageNumber{TWeakObjectPtr<const ALiveMonster> Target;float Amount=0;FVector Location=FVector::ZeroVector;float Age=0,Drift=0;bool bCritical=false,bSkill=false;};
    static constexpr float DamageNumberLife=1.1f;
    const TArray<FDamageNumber>& GetDamageNumbers()const{return DamageNumbers;}
    /** Damage numbers currently on screen; for tests. */
    UFUNCTION(BlueprintPure,Category="特殊剑灵") int32 GetLiveDamageNumbers()const{return DamageNumbers.Num();}
    /** Damage this sword has dealt with one skill (Attack for plain strikes) since it appeared. */
    UFUNCTION(BlueprintPure,Category="特殊剑灵") double GetSkillDamage(FName SkillId)const{return SkillDamage.FindRef(SkillId);}
    UFUNCTION(BlueprintPure,Category="特殊剑灵") float GetAttackPower()const{return AttackPower();}
    UFUNCTION(BlueprintPure,Category="特殊剑灵") float GetCriticalChance()const{return float(CachedCritical);}
    UFUNCTION(BlueprintPure,Category="特殊剑灵") float GetCriticalDamage()const{return float(CachedCriticalDamage);}
    UFUNCTION(BlueprintPure,Category="特殊剑灵") float GetArmorPenetration()const{return float(CachedPenetration);}
    /** 增伤 from the form's own growth (machine-soul tempering). */
    UFUNCTION(BlueprintPure,Category="特殊剑灵") float GetDamageBonus()const{return DamageBonus();}
    UFUNCTION(BlueprintPure,Category="特殊剑灵") int32 GetRealm()const{return Realm;}
    UFUNCTION(BlueprintPure,Category="特殊剑灵") float GetAttackInterval()const{return AttackInterval;}
    /** Test hook: casts one of the form's active skills now if the sword is between actions and the skill has a target. */
    UFUNCTION(BlueprintCallable,Category="特殊剑灵") bool CastSkillNow(FName SkillId);

    virtual void Tick(float DeltaSeconds)override;
    virtual void EndPlay(const EEndPlayReason::Type Reason)override;

    UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> Pivot;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Blade;
    UPROPERTY(VisibleAnywhere) TObjectPtr<USkeletalMeshComponent> SkeletalBlade;
    UPROPERTY(Transient) TObjectPtr<UNiagaraComponent> Trail;
    UPROPERTY(Transient) TObjectPtr<UNiagaraComponent> Aura;

protected:
    enum class EMode : uint8 { Idle, Dash, Recover, Cast, Guard };
    ELiveSpecialSword Kind=ELiveSpecialSword::None;
    EMode Mode=EMode::Idle;
    FString ParticipantId,DisplayName;
    TWeakObjectPtr<ULiveSpecialSwordComponent> Manager;
    TWeakObjectPtr<ALiveWeaponSwarm> Swarm;
    FVector Velocity=FVector::ZeroVector;
    FQuat Facing=FQuat::Identity;
    float Clock=0,ModeTime=0,StatsTimer=0;
    float OrbitAngle=0,OrbitRadius=250,OrbitHeight=150;
    float AttackCooldown=.6f,AttackInterval=1.2f;
    float BladeLength=120;
    bool bChainPending=false;
    FName CastingSkill;
    TMap<FName,float> SkillReadyAt;
    TWeakObjectPtr<ALiveMonster> DashTarget;
    FRandomStream Random;
    TArray<FFloater> Floaters;
    TArray<FDamageNumber> DamageNumbers;
    TMap<FName,double> SkillDamage;
    double CachedAttack=10,CachedCritical=0,CachedCriticalDamage=2,CachedPenetration=0;
    /** The viewer's realm index, refreshed with the stats. */
    int32 Realm=0;

    APawn* Anchor()const;
    ULiveAnchorBuffSubsystem* Buffs()const;
    bool IsAnchorDown()const;
    FVector AnchorCenter()const;
    float FloorZ()const;
    FVector IdlePoint()const;
    FLinearColor Tint()const{return LiveSpecialSwords::Color(Kind);}
    /** The grip in Pivot space: blades fly tip-first along +Z, so sword-qi trails stream from here. */
    FVector HiltOffset()const{return FVector(0,0,-BladeLength*.4f);}
    /** Effects follow the form: futuristic for the mechanical Inorganic form, ink wash for the cultivator swords. */
    LiveSwordVfx::EStyle Style()const{return Kind==ELiveSpecialSword::Inorganic?LiveSwordVfx::EStyle::Tech:LiveSwordVfx::EStyle::Ink;}
    UNiagaraComponent* PlayFx(LiveSwordVfx::EFx Fx,const FVector& At,const FLinearColor& Color,float Size,const FRotator& Rotation=FRotator::ZeroRotator)
    {return LiveSwordVfx::SpawnAt(this,Fx,At,Color,Size,Rotation,Style());}
    UNiagaraComponent* AttachFx(USceneComponent* Parent,LiveSwordVfx::EFx Fx,const FLinearColor& Color,float Size,const FVector& Offset=FVector::ZeroVector)
    {return LiveSwordVfx::SpawnAttached(Parent,Fx,Color,Size,Offset,false,Style());}

    void Steer(const FVector& Desired,float DeltaSeconds,float MaxSpeed,float Response=8.f);
    void Face(const FVector& Direction,float DeltaSeconds,float Speed=12.f);
    void FaceDown(float DeltaSeconds,float Speed=14.f){Face(FVector::DownVector,DeltaSeconds,Speed);}
    ALiveMonster* NearestEnemy(const FVector& From,float Range,const ALiveMonster* Exclude=nullptr)const;
    void Enemies(const FVector& Center,float Radius,TArray<ALiveMonster*>& Out)const;
    FVector HitPoint(const ALiveMonster* M)const;
    float EngageRange()const{return 1200.f;}
    bool IsSkillReady(FName Id)const{return Clock>=SkillReadyAt.FindRef(Id);}
    void StartCooldown(FName Id);
    float CooldownOf(FName Id)const;
    void BeginDash(ALiveMonster* Target);
    void FinishCast();
    void Launch(const FVector& From,const FLiveSwordShot& Shot);

    void SetupBlade(UStaticMesh* Mesh,float Length);
    void SetupSkeletal(USkeletalMesh* Mesh,UAnimSequence* Anim,float Length);

    /** Kind hooks. SetupForm loads the blade before effects attach. */
    virtual void SetupForm(){SetupBlade(nullptr,120.f);}
    virtual TArray<FName> ActiveSkills()const{return {};}
    virtual bool CanCast(FName Skill)const{return true;}
    virtual void BeginCast(FName Skill){}
    virtual void TickCast(float DeltaSeconds){FinishCast();}
    virtual void TickKind(float DeltaSeconds){}
    virtual void TickGuard(float DeltaSeconds){Mode=EMode::Idle;}
    virtual FName PriorityCast()const{return NAME_None;}
    virtual bool SuppressAttacks()const{return false;}
    virtual void OnNormalHit(ALiveMonster* Target){}
    virtual void ModifyHit(ALiveMonster* Target,FLiveHit& Hit,FName SkillId){}
    virtual void AfterHit(ALiveMonster* Target,float Applied,bool bKilled,FName SkillId,bool bAllowFollowUp){}
    /** Permanent attack the form has grown (噬魂 devoured attack). */
    virtual float GrowthAttack()const{return 0;}
    /** Permanent 增伤 the form has grown (无机态那 tempering). */
    virtual float DamageBonus()const{return 0;}

    /** Translucent copy of the blade for clones; owned and cleaned up by this actor. */
    struct FGhost
    {
        TObjectPtr<UStaticMeshComponent> Mesh;
        TObjectPtr<UNiagaraComponent> Trail;
        FVector Position=FVector::ZeroVector,Velocity=FVector::ZeroVector;
        FQuat Facing=FQuat::Identity;
        TWeakObjectPtr<ALiveMonster> Target;
        float Age=0,Timer=0,Angle=0;
        float DispelTimer=0,BlessTimer=0;
        int32 Phase=0;
    };
    TArray<FGhost> Ghosts;
    int32 SpawnGhost(const FVector& At,float Opacity=.7f);
    void ReleaseGhost(int32 Index,bool bShatter);
    void SteerGhost(FGhost& G,const FVector& Desired,float DeltaSeconds,float MaxSpeed,float Response=9.f);
    void PlaceGhosts(float DeltaSeconds);

private:
    void RefreshStats();
    void Decide();
    void TickIdle(float DeltaSeconds);
    void TickDash(float DeltaSeconds);
    void TickRecover(float DeltaSeconds);
    void ApplyTransform(float DeltaSeconds);
    FVector MeshCenterOffset=FVector::ZeroVector;
};

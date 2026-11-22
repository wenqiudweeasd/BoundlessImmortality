#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "XianTu/XianTuTypes.h"
#include "XianTuTribulationStorm.generated.h"
class UNiagaraComponent;
class UPointLightComponent;
class UXianTuSubsystem;
class UCultivationHealthComponent;
class APawn;

/** How hard the heavens strike at a success probability: the lower the chance, the more, harder and faster the bolts. */
USTRUCT(BlueprintType)
struct BOUNDLESSIMMORTALITY_API FXianTuTribulationPressure
{
    GENERATED_BODY()
    /** 劫威 0..1: 0 at a certain success, 1 at the 50% floor of the success formula. */
    UPROPERTY(BlueprintReadOnly,Category="天劫") float Intensity=0;
    /** Seconds between strikes, phase factor included, before the ±15% jitter. */
    UPROPERTY(BlueprintReadOnly,Category="天劫") float Interval=15;
    /** Bolts in one strike, all aimed at the anchor. */
    UPROPERTY(BlueprintReadOnly,Category="天劫") int32 Bolts=1;
    /** Share of the anchor's maximum health one bolt removes. */
    UPROPERTY(BlueprintReadOnly,Category="天劫") float DamageFraction=.05f;
};

/**
 * 天劫雷云: the storm of a running tribulation. A vortex of storm clouds sits on the camera's sight line to the anchor at
 * cloud height with a clear eye over the anchor, so the anchor stays in view while the clouds fill the top of the screen.
 * It gathers through the preparation, rages through the three trials (肉身/神魂/心魔) striking the anchor with lightning,
 * then parts in golden light on success. A failed roll does not let the clouds go: the eye closes and the backlash (天罚)
 * strikes the anchor every 0.2 s for 10 s, each bolt harder than the last (BacklashFraction); these bolts can kill, and
 * only a held 七星挡劫 (UXianTuSubsystem::GetStarWard) keeps them from it. What a strike leaves on the ground (NS_SS_TribHit
 * and the Vefects zap) rides on the anchor's feet.
 * Trial strikes follow the live success probability, which rises with audience assists (see Pressure): from one bolt of
 * 5% of the anchor's maximum health every ~16 s at a certain success up to three bolts of 7.5% every ~7 s at the 50%
 * floor, later trials striking faster. Bolts ignore defense, evasion and immunity, shields still absorb them, and trial
 * bolts never take the anchor below MinHealthFraction: success is only the one roll at the end.
 * Spawned by UXianTuSubsystem while a tribulation prepares, runs or punishes; it removes itself once it has faded.
 */
UCLASS(NotBlueprintable)
class BOUNDLESSIMMORTALITY_API AXianTuTribulationStorm : public AActor
{
    GENERATED_BODY()
public:
    AXianTuTribulationStorm();
    void Bind(UXianTuSubsystem* InXianTu){XianTu=InXianTu;}
    virtual void Tick(float DeltaSeconds)override;

    /** Pressure for a success probability in basis points (5000 = 50%) in trial phase 0 (肉身), 1 (神魂) or 2 (心魔). */
    UFUNCTION(BlueprintPure,Category="天劫") static FXianTuTribulationPressure Pressure(int32 ProbabilityBasisPoints,int32 Phase=1,float BaseDamage=.05f);
    /**
     * Share of the anchor's maximum health backlash bolt Strike (0..49) removes: each bolt 6% harder than the one before,
     * all fifty together lerp(100%, 200%, 劫威) of the maximum, so a failure against long odds is the deadliest and health
     * and healing alone hardly ever outlast it.
     */
    UFUNCTION(BlueprintPure,Category="天劫") static float BacklashFraction(int32 Strike,float Intensity);
    /** Backlash bolts fired this attempt, and the health they removed. */
    UFUNCTION(BlueprintPure,Category="天劫") int32 GetBacklashStrikes()const{return BacklashFired;}
    UFUNCTION(BlueprintPure,Category="天劫") double GetBacklashDamage()const{return BacklashDamage;}
    UFUNCTION(BlueprintPure,Category="天劫") FXianTuTribulationPressure GetPressure()const{return Current;}
    UFUNCTION(BlueprintPure,Category="天劫") int32 GetStrikes()const{return Strikes;}
    UFUNCTION(BlueprintPure,Category="天劫") int32 GetBoltsFired()const{return BoltsFired;}
    UFUNCTION(BlueprintPure,Category="天劫") int32 GetBoltsLanded()const{return BoltsLanded;}
    /** Health the bolts have removed this attempt. */
    UFUNCTION(BlueprintPure,Category="天劫") double GetDamageDealt()const{return DamageDealt;}
    /** Health the last landed bolt asked for (before shields and the health floor). */
    UFUNCTION(BlueprintPure,Category="天劫") double GetLastBoltDamage()const{return LastBoltDamage;}
    /** How far the clouds have gathered, 0..1. */
    UFUNCTION(BlueprintPure,Category="天劫") float GetGathered()const{return Density;}
    /** Sim seconds until the next strike while a trial runs, otherwise -1. */
    UFUNCTION(BlueprintPure,Category="天劫") float GetNextStrikeIn()const;
    /** Test hook: begins the volley the current pressure calls for, now. */
    UFUNCTION(BlueprintCallable,Category="天劫") void StrikeNow();

    UPROPERTY(EditAnywhere,Category="天劫",meta=(ClampMin="0",ClampMax="1")) float DamageFraction=.05f;
    UPROPERTY(EditAnywhere,Category="天劫",meta=(ClampMin="0",ClampMax="1")) float MinHealthFraction=.1f;
    UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> Root;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UNiagaraComponent> Cloud;
    /** Lightning light where a bolt lands, and in the cloud it leaves. */
    UPROPERTY(VisibleAnywhere) TObjectPtr<UPointLightComponent> ImpactLight;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UPointLightComponent> CloudLight;

protected:
    virtual void BeginPlay()override;

private:
    /** Fraction: share of maximum health the bolt removes; Floor: health share it never goes below (0 lets it kill). */
    struct FBolt{float Delay=0;bool bDamage=true;float Scale=1;float Fraction=0;float Floor=.1f;bool bBacklash=false;};
    TWeakObjectPtr<UXianTuSubsystem> XianTu;
    FXianTuTribulationPressure Current;
    TArray<FBolt> Volley;
    FString AttemptId;
    EXianTuTribState LastState=EXianTuTribState::None;
    float Density=0,Eye=1.f,StormLevel=0,Flash=0,ImpactGlow=0,CloudGlow=0,Charge=0;
    double NextStrikeAt=2.0;
    int32 Strikes=0,BoltsFired=0,BoltsLanded=0,BacklashFired=0;
    double DamageDealt=0,LastBoltDamage=0,BacklashDamage=0;
    bool bFinale=false;
    FRandomStream Random;
    /** Camera frame: the eye centre on the sight line, the world half-extents shown there, the view yaw and pitch. */
    FVector EyeCentre=FVector::ZeroVector;
    float HalfWidth=800.f,HalfHeight=450.f,ViewYaw=0.f,ViewPitch=45.f;
    /** The vortex fitted to that frame: its User.Radius and the eye radius as a share of it. */
    float CloudRadius=800.f,EyeFit=.5f;
    /** Brightest directional light in the world (lux), the scale for the strike lights. */
    float SunLux=10.f;

    UCultivationHealthComponent* AnchorHealth()const;
    APawn* AnchorPawn()const;
    void UpdateFrame(const APawn* Pawn,float DeltaSeconds);
    void BeginVolley(int32 Bolts,bool bDamage,float Scale);
    void FireBolt(const FBolt& Bolt);
    void Finale(bool bSucceeded);
    FVector StrikePoint(const APawn* Pawn)const;
};

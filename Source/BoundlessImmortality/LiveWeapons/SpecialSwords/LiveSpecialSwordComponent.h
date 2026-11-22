#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Blueprint/UserWidget.h"
#include "LiveWeapons/SpecialSwords/LiveSpecialSwordTypes.h"
#include "LiveSpecialSwordComponent.generated.h"
class ALiveSpecialSword;
class ALiveWeaponSwarm;
class UFont;

/**
 * Lives on the gameplay swarm. A viewer whose equipped title is one of the five special swords flies as a
 * dedicated ALiveSpecialSword; the swarm keeps the roster slot (nameplate, leases) but stops drawing it.
 */
UCLASS(ClassGroup=(LiveWeapons),meta=(BlueprintSpawnableComponent))
class BOUNDLESSIMMORTALITY_API ULiveSpecialSwordComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    ULiveSpecialSwordComponent();
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="特殊剑灵") bool bEnabled=true;
    /** Upper bound for the machine-soul tempering bonus (+5% per consumed layer). 1 = +100%. */
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="特殊剑灵",meta=(ClampMin="0")) float MechaDamageBonusCap=1.f;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="特殊剑灵") int32 ActiveSwords=0;

    static ULiveSpecialSwordComponent* Find(const UObject* WorldContext);
    ALiveSpecialSword* FindSword(const FString& ParticipantId)const;
    void GetSwords(TArray<ALiveSpecialSword*>& Out)const;
    ELiveSpecialSword GetKind(const FString& ParticipantId)const;
    const FLiveSpecialSwordGrowth& GetGrowth(const FString& ParticipantId)const;
    void AddDevour(const FString& ParticipantId,const LiveSpecialSwords::FDevourGain& Gain);
    /** Forgets a viewer's devoured attack and tempering (test viewers start each session from nothing). */
    void ResetGrowth(const FString& ParticipantId);
    /** Returns the total bonus after the cap. */
    double AddMechaBonus(const FString& ParticipantId,double Amount);
    void NotifyDamage(const FString& ParticipantId,float Applied);
    double GetDamageDealt(const FString& ParticipantId)const{return DamageDealt.FindRef(ParticipantId);}
    ALiveWeaponSwarm* GetSwarm()const;

    virtual void TickComponent(float DeltaTime,ELevelTick TickType,FActorComponentTickFunction* ThisTickFunction)override;
protected:
    virtual void BeginPlay()override;
    virtual void EndPlay(const EEndPlayReason::Type Reason)override;
private:
    UPROPERTY(Transient) TMap<FString,TObjectPtr<ALiveSpecialSword>> Swords;
    UPROPERTY(Transient) TArray<TObjectPtr<UObject>> Preloaded;
    UPROPERTY(Transient) TObjectPtr<class ULiveSpecialSwordOverlay> Overlay;
    FLiveSpecialSwordGrowthFile Growth;
    TMap<FString,double> DamageDealt;
    float ScanTimer=0,SaveTimer=0;
    bool bGrowthDirty=false;
    void Scan();
    void Release(const FString& Id,bool bReturnToSwarm);
    void ReleaseAll();
    FString GrowthPath()const;
    void LoadGrowth();
    void SaveGrowth();
};

/** World-anchored meters and floating combat text for special swords; drawn under the page UI. */
UCLASS()
class BOUNDLESSIMMORTALITY_API ULiveSpecialSwordOverlay : public UUserWidget
{
    GENERATED_BODY()
public:
    void Configure(ULiveSpecialSwordComponent* InOwner);
protected:
    virtual int32 NativePaint(const FPaintArgs& Args,const FGeometry& Geometry,const FSlateRect& CullingRect,FSlateWindowElementList& OutDrawElements,int32 LayerId,const FWidgetStyle& InWidgetStyle,bool bParentEnabled)const override;
private:
    TWeakObjectPtr<ULiveSpecialSwordComponent> Owner;
    UPROPERTY(Transient) TObjectPtr<UFont> Font;
};

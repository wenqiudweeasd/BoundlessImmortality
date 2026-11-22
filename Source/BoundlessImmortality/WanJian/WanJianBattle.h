#pragma once
#include "CoreMinimal.h"
#include "WanJian/WanJianCatalog.h"
#include "WanJian/WanJianRules.h"

/**
 * The shared combat simulator of GDD 3.1: fixed 10 Hz steps, every lineup sword with its own cooldown and target, and
 * hits resolved centrally so rendering stays free to interpolate. Deterministic for a given setup and seed.
 */
namespace WanJianBattle
{
    inline constexpr double StepSeconds=0.1;

    struct FSetup
    {
        int32 StageId=0;
        TArray<FName> Swords;
        TArray<FName> Methods;
        EWanJianElement Root=EWanJianElement::None;
        double Attack=20;
        double MaxHealth=200;
        int32 Seed=1;
        /** Players sharing the room. Each brings a copy of the lineup, so a team of 4 runs up to 72 logical swords. */
        int32 Team=1;
    };

    struct FResult
    {
        bool bValid=false;
        bool bVictory=false;
        double Seconds=0;
        int32 NormalKills=0;
        int32 EliteKills=0;
        int32 BossKills=0;
        int32 CriticalUnits=0;
        double HealthLeft=0;
        int64 Hits=0;
        int32 PeakEnemies=0;
        TArray<FName> ActiveMethods;
    };

    struct FEnemy
    {
        int32 Id=0;
        WanJianRules::EKill Kind=WanJianRules::EKill::Normal;
        double Health=0;
        double MaxHealth=0;
        double Defense=0;
        double Attack=0;
        /** Distance left to the goal; at zero the enemy attacks the player. */
        double Distance=1000;
        double Speed=0;
        double AttackTimer=0;
        double BurnSeconds=0;
        double BurnDps=0;
        double SlowSeconds=0;
        double BrokenSeconds=0;
        double SpreadCooldown=0;
        bool IsAlive()const{return Health>0;}
    };

    class BOUNDLESSIMMORTALITY_API FSimulator
    {
    public:
        explicit FSimulator(const FSetup& InSetup);
        /** Advances one step. A moving player interrupts standing and casting; fixed-position play passes false. */
        void Tick(bool bPlayerMoving=false);
        bool IsFinished()const{return bFinished||!Result.bValid;}
        const FResult& GetResult()const{return Result;}
        const TArray<FEnemy>& GetEnemies()const{return Enemies;}
        double GetHealth()const{return Health;}
        double GetShield()const{return Shield;}
        double GetTime()const{return Time;}
        double GetCriticalPoints()const{return double(Result.CriticalUnits)/WanJianRules::CriticalUnitsPerPoint;}
        bool IsMethodActive(FName Method)const{return Result.ActiveMethods.Contains(Method);}

    private:
        struct FSpawn { double At=0; WanJianRules::EKill Kind=WanJianRules::EKill::Normal; };
        struct FSwordState { const WanJianCatalog::FSword* Def=nullptr; double Cooldown=0; int32 Target=0; double Lock=0; };

        void BuildSpawns();
        void SpawnDue();
        void UpdateMethods(bool bPlayerMoving);
        void UpdateEnemies();
        void FireSword(FSwordState& Sword);
        FEnemy* PickTarget(FSwordState& Sword);
        FEnemy* FindEnemy(int32 Id);
        void Hit(const WanJianCatalog::FSword& Sword,FEnemy& Enemy,double Skill,bool bPrimary);
        void Damage(FEnemy& Enemy,double Amount);
        void Kill(FEnemy& Enemy);
        void ApplyProc(const WanJianCatalog::FSword& Sword,FEnemy& Enemy,double Dealt);
        void Heal(double Fraction);
        void Echo(const WanJianCatalog::FSword& Sword);
        void HurtPlayer(double Amount);
        double AttackSpeedBonus()const;
        void Finish(bool bVictory);
        bool HasPair(EWanJianElement A,EWanJianElement B)const;

        FSetup Setup;
        WanJianRules::FStage Stage;
        FRandomStream Random;
        TArray<FSwordState> Swords;
        TArray<FEnemy> Enemies;
        TArray<FSpawn> Spawns;
        TArray<FName> PendingMethods;
        TSet<EWanJianElement> Elements;
        FResult Result;
        double Time=0;
        double Health=0;
        double Shield=0;
        double StandSeconds=0;
        double CastSeconds=0;
        double FocusSeconds=0;
        double VitalityCooldown=0;
        double WardCooldown=0;
        int32 NextSpawn=0;
        int32 NextEnemyId=1;
        int32 HitCounter=0;
        int32 ShieldTier=0;
        bool bFinished=false;
    };

    /** Runs a whole battle without any input. */
    BOUNDLESSIMMORTALITY_API FResult Run(const FSetup& Setup);
    /** The loadout a stage's difficulty assumes: its slots filled from the catalog at the reference attack. */
    BOUNDLESSIMMORTALITY_API FSetup ReferenceSetup(int32 World,int32 Index,int32 Seed,int32 Team=1);
}

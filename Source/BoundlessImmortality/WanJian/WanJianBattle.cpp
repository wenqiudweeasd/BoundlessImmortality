#include "WanJian/WanJianBattle.h"

namespace WanJianBattle
{
    using namespace WanJianCatalog;
    using namespace WanJianRules;

    namespace
    {
        constexpr double PathLength=1000.0;
        /** Enemies this close to the goal outrank every other target. */
        constexpr double ThreatDistance=180.0;
        constexpr double TargetLockSeconds=0.5;
        constexpr double StandBeforeCast=1.2;
        constexpr double CastSeconds_=0.8;
        constexpr double AreaRadius=160.0;
        constexpr double OvertimeSeconds=30.0;
        constexpr double CriticalChance=0.10;
        constexpr double CriticalMultiplier=1.5;
        const TCHAR* const Jifeng=TEXT("Jifeng");
        const TCHAR* const Xuangang=TEXT("Xuangang");
        const TCHAR* const Huixiang=TEXT("Huixiang");

        double EnemyHealthScale(EKill Kind,bool bWorldBoss){return Kind==EKill::Boss?(bWorldBoss?45.0:30.0):Kind==EKill::Elite?5.0:1.0;}
        double EnemyAttackScale(EKill Kind){return Kind==EKill::Boss?7.0:Kind==EKill::Elite?3.0:1.0;}
    }

    FSimulator::FSimulator(const FSetup& InSetup):Setup(InSetup),Random(InSetup.Seed)
    {
        Setup.Team=FMath::Clamp(Setup.Team,1,MaxTeam);
        int32 World=0,Index=0;
        Result.bValid=SplitStageId(Setup.StageId,World,Index)&&MakeStage(World,Index,Stage);
        if(!Result.bValid)return;

        Health=Setup.MaxHealth;
        Result.HealthLeft=Health;
        int32 Slot=0;
        for(int32 Copy=0;Copy<Setup.Team;++Copy)
        {
            for(const FName Id:Setup.Swords)
            {
                const FSword* Def=Sword(Id);
                if(!Def)continue;
                FSwordState State;
                State.Def=Def;
                // Staggered starts: a full lineup should not fire in the same step.
                State.Cooldown=Def->CooldownMs*0.001*((Slot++%5)*0.2);
                Swords.Add(State);
                if(Def->Element!=EWanJianElement::None)Elements.Add(Def->Element);
            }
        }
        for(const FName Id:Setup.Methods)
        {
            const FMethod* Def=Method(Id);
            if(!Def)continue;
            // GDD 18.3: a method without a threshold is active from the start and only reads the critical value.
            if(Def->Critical<=0)Result.ActiveMethods.Add(Id);
            else PendingMethods.Add(Id);
        }
        BuildSpawns();
    }

    void FSimulator::BuildSpawns()
    {
        const double Multiplier=SpawnMultiplier(Setup.Team);
        const int32 Normals=FMath::RoundToInt(Stage.Normals*Multiplier);
        const int32 Elites=FMath::RoundToInt(Stage.Elites*Multiplier);
        const double Window=Stage.Duration*0.7;
        for(int32 I=0;I<Normals;++I)Spawns.Add({1.0+Window*I/FMath::Max(1,Normals),EKill::Normal});
        for(int32 I=0;I<Elites;++I)Spawns.Add({Window*(I+1)/double(Elites+1),EKill::Elite});
        if(Stage.bBoss)Spawns.Add({Stage.Duration*0.5,EKill::Boss});
        Spawns.StableSort([](const FSpawn& A,const FSpawn& B){return A.At<B.At;});
    }

    void FSimulator::SpawnDue()
    {
        while(NextSpawn<Spawns.Num()&&Spawns[NextSpawn].At<=Time)
        {
            const EKill Kind=Spawns[NextSpawn++].Kind;
            FEnemy E;
            E.Id=NextEnemyId++;
            E.Kind=Kind;
            E.MaxHealth=Stage.NormalHealth*EnemyHealthScale(Kind,Stage.bWorldBoss)*TeamHealthMultiplier(Setup.Team);
            E.Health=E.MaxHealth;
            E.Defense=Stage.Defense;
            E.Attack=Stage.Attack*EnemyAttackScale(Kind);
            E.Distance=PathLength;
            E.Speed=PathLength/Stage.PathSeconds*(Kind==EKill::Boss?0.6:Kind==EKill::Elite?0.8:1.0);
            Enemies.Add(E);
        }
        int32 Alive=0;
        for(const FEnemy& E:Enemies)Alive+=E.IsAlive()?1:0;
        Result.PeakEnemies=FMath::Max(Result.PeakEnemies,Alive);
    }

    void FSimulator::UpdateMethods(bool bPlayerMoving)
    {
        if(PendingMethods.IsEmpty())return;
        if(bPlayerMoving)
        {
            StandSeconds=0;
            CastSeconds=0;
            return;
        }
        StandSeconds+=StepSeconds;
        if(StandSeconds<StandBeforeCast)return;

        const int32 Ready=PendingMethods.IndexOfByPredicate([this](FName Id){return GetCriticalPoints()>=Method(Id)->Critical;});
        if(Ready==INDEX_NONE)
        {
            CastSeconds=0;
            return;
        }
        CastSeconds+=StepSeconds;
        if(CastSeconds+1e-9>=CastSeconds_)
        {
            Result.ActiveMethods.Add(PendingMethods[Ready]);
            PendingMethods.RemoveAt(Ready);
            CastSeconds=0;
        }
    }

    double FSimulator::AttackSpeedBonus()const
    {
        if(!IsMethodActive(Jifeng))return 0;
        const double Extra=FMath::Min(0.10,0.01*FMath::FloorToInt(GetCriticalPoints()/10.0));
        return FMath::Min(0.30,0.04+Extra);
    }

    FEnemy* FSimulator::FindEnemy(int32 Id)
    {
        return Enemies.FindByPredicate([Id](const FEnemy& E){return E.Id==Id&&E.IsAlive();});
    }

    FEnemy* FSimulator::PickTarget(FSwordState& Sword)
    {
        if(Sword.Lock>0)
        {
            if(FEnemy* Locked=FindEnemy(Sword.Target))return Locked;
        }
        FEnemy* Threat=nullptr;
        FEnemy* Priority=nullptr;
        FEnemy* Nearest=nullptr;
        for(FEnemy& E:Enemies)
        {
            if(!E.IsAlive())continue;
            if(E.Distance<=ThreatDistance&&(!Threat||E.Distance<Threat->Distance))Threat=&E;
            if(E.Kind!=EKill::Normal&&(!Priority||int32(E.Kind)>int32(Priority->Kind)||(E.Kind==Priority->Kind&&E.Distance<Priority->Distance)))Priority=&E;
            if(!Nearest||E.Distance<Nearest->Distance)Nearest=&E;
        }
        FEnemy* Choice=Threat?Threat:Priority?Priority:Nearest;
        if(Choice)
        {
            Sword.Target=Choice->Id;
            Sword.Lock=TargetLockSeconds;
        }
        return Choice;
    }

    void FSimulator::FireSword(FSwordState& State)
    {
        const FSword& Def=*State.Def;
        if(Def.Attack==EAttack::Support)
        {
            if(Def.Proc==EProc::Heal)
            {
                Heal(0.02);
            }
            else if(Def.Proc==EProc::StarGuard)
            {
                // 七星护持: a small steady heal, and a real save when the player is close to falling.
                const bool bCritical=Health<Setup.MaxHealth*0.3;
                Heal(bCritical?0.08:0.015);
                if(bCritical)Shield=FMath::Max(Shield,Setup.MaxHealth*0.05);
            }
            return;
        }
        FEnemy* Target=PickTarget(State);
        if(!Target)return;

        const double PrimaryDistance=Target->Distance;
        const int32 PrimaryId=Target->Id;
        Hit(Def,*Target,1.0,true);
        if(Def.Attack==EAttack::Boomerang)
        {
            if(FEnemy* Again=FindEnemy(PrimaryId))Hit(Def,*Again,1.0,false);
            return;
        }
        if(Def.Attack==EAttack::Homing)return;

        const int32 MaxOthers=Def.Attack==EAttack::Pierce?1:Def.Attack==EAttack::Chain?2:3;
        const double Falloff=Def.Attack==EAttack::Area?0.5:0.6;
        int32 Others=0;
        for(FEnemy& E:Enemies)
        {
            if(Others>=MaxOthers)break;
            if(!E.IsAlive()||E.Id==PrimaryId||FMath::Abs(E.Distance-PrimaryDistance)>AreaRadius)continue;
            Hit(Def,E,Falloff,false);
            ++Others;
        }
    }

    void FSimulator::Hit(const FSword& Def,FEnemy& Enemy,double Skill,bool bPrimary)
    {
        double Bonus=1.0;
        if(Setup.Root!=EWanJianElement::None&&Def.Element==Setup.Root)Bonus*=1.10;
        if(FocusSeconds>0&&Def.Element==EWanJianElement::Metal)Bonus*=1.10;
        if(Random.FRand()<CriticalChance)Bonus*=CriticalMultiplier;
        const double Defense=Enemy.BrokenSeconds>0?Enemy.Defense*0.9:Enemy.Defense;
        const double Amount=FMath::Max(1.0,FMath::FloorToDouble(Setup.Attack*Def.Coefficient*Skill*Bonus*DefenseFactor(Defense)));
        const bool bWasBurning=Enemy.BurnSeconds>0;
        const bool bWasBroken=Enemy.BrokenSeconds>0;
        ++Result.Hits;
        Damage(Enemy,Amount);
        if(!bPrimary)return;

        ApplyProc(Def,Enemy,Amount);
        // Pairings of GDD 4.2. Each needs both elements in the lineup and has its own cooldown or no-stack rule.
        if(Def.Element==EWanJianElement::Metal&&bWasBurning&&HasPair(EWanJianElement::Metal,EWanJianElement::Fire))Enemy.BrokenSeconds=FMath::Max(Enemy.BrokenSeconds,3.0);
        if(Def.Element==EWanJianElement::Earth&&bWasBroken&&HasPair(EWanJianElement::Metal,EWanJianElement::Earth))Enemy.SlowSeconds+=Enemy.Kind==EKill::Boss?0.5:1.0;
        if(Def.Proc==EProc::Slow&&VitalityCooldown<=0&&HasPair(EWanJianElement::Water,EWanJianElement::Wood))
        {
            Heal(0.01);
            VitalityCooldown=8.0;
        }
        if(Def.Element==EWanJianElement::Water&&WardCooldown<=0&&HasPair(EWanJianElement::Water,EWanJianElement::Earth))
        {
            Shield=FMath::Max(Shield,Setup.MaxHealth*0.02);
            WardCooldown=6.0;
        }
        if(Def.Proc==EProc::Burn&&Enemy.SpreadCooldown<=0&&HasPair(EWanJianElement::Fire,EWanJianElement::Wood))
        {
            Enemy.SpreadCooldown=4.0;
            for(FEnemy& Other:Enemies)
            {
                if(Other.IsAlive()&&Other.Id!=Enemy.Id&&FMath::Abs(Other.Distance-Enemy.Distance)<=AreaRadius)
                {
                    Other.BurnSeconds=FMath::Max(Other.BurnSeconds,2.0);
                    Other.BurnDps=FMath::Max(Other.BurnDps,Amount*0.1);
                    break;
                }
            }
        }
        if(IsMethodActive(Huixiang)&&++HitCounter%5==0)Echo(Def);
    }

    void FSimulator::Echo(const FSword& Def)
    {
        const double Critical=GetCriticalPoints();
        int32 Remaining=1+(Critical>=20?1:0)+(Critical>=50?1:0);
        for(FEnemy& E:Enemies)
        {
            if(Remaining<=0)break;
            if(!E.IsAlive())continue;
            const double Amount=FMath::Max(1.0,FMath::FloorToDouble(Setup.Attack*Def.Coefficient*0.5*DefenseFactor(E.Defense)));
            ++Result.Hits;
            Damage(E,Amount);
            --Remaining;
        }
    }

    void FSimulator::ApplyProc(const FSword& Def,FEnemy& Enemy,double Dealt)
    {
        const bool bBoss=Enemy.Kind==EKill::Boss;
        switch(Def.Proc)
        {
        case EProc::Burn:
            Enemy.BurnSeconds=FMath::Max(Enemy.BurnSeconds,3.0);
            Enemy.BurnDps=FMath::Max(Enemy.BurnDps,Dealt*0.2);
            break;
        case EProc::Slow:
            Enemy.SlowSeconds=FMath::Max(Enemy.SlowSeconds,bBoss?0.75:1.5);
            break;
        case EProc::ArmorBreak:
            Enemy.BrokenSeconds=FMath::Max(Enemy.BrokenSeconds,3.0);
            break;
        case EProc::Knockback:
            if(Enemy.IsAlive())Enemy.Distance=FMath::Min(PathLength,Enemy.Distance+(bBoss?20.0:40.0));
            break;
        case EProc::ElementFocus:
            FocusSeconds=3.0;
            break;
        default:
            break;
        }
    }

    void FSimulator::Damage(FEnemy& Enemy,double Amount)
    {
        if(!Enemy.IsAlive())return;
        Enemy.Health-=Amount;
        if(!Enemy.IsAlive())Kill(Enemy);
    }

    void FSimulator::Kill(FEnemy& Enemy)
    {
        int32& Count=Enemy.Kind==EKill::Boss?Result.BossKills:Enemy.Kind==EKill::Elite?Result.EliteKills:Result.NormalKills;
        ++Count;
        Result.CriticalUnits=FMath::Min(CriticalCapUnits,Result.CriticalUnits+KillCriticalUnits(Enemy.Kind,Setup.Team));

        if(IsMethodActive(Xuangang))
        {
            const int32 Tier=FMath::FloorToInt(GetCriticalPoints()/10.0);
            if(Tier>ShieldTier)
            {
                ShieldTier=Tier;
                Shield=FMath::Max(Shield,Setup.MaxHealth*FMath::Min(0.2,0.02*Tier));
            }
        }
    }

    void FSimulator::Heal(double Fraction)
    {
        Health=FMath::Min(Setup.MaxHealth,Health+Setup.MaxHealth*Fraction);
    }

    void FSimulator::HurtPlayer(double Amount)
    {
        double Left=Amount*(IsMethodActive(Xuangang)?0.95:1.0);
        const double Absorbed=FMath::Min(Shield,Left);
        Shield-=Absorbed;
        Left-=Absorbed;
        Health-=Left;
    }

    void FSimulator::UpdateEnemies()
    {
        for(FEnemy& E:Enemies)
        {
            if(!E.IsAlive())continue;
            E.SpreadCooldown=FMath::Max(0.0,E.SpreadCooldown-StepSeconds);
            E.BrokenSeconds=FMath::Max(0.0,E.BrokenSeconds-StepSeconds);
            if(E.BurnSeconds>0)
            {
                E.BurnSeconds-=StepSeconds;
                Damage(E,E.BurnDps*StepSeconds);
                if(!E.IsAlive())continue;
            }
            const double Pace=E.SlowSeconds>0?0.6:1.0;
            E.SlowSeconds=FMath::Max(0.0,E.SlowSeconds-StepSeconds);
            if(E.Distance>0)
            {
                E.Distance=FMath::Max(0.0,E.Distance-E.Speed*Pace*StepSeconds);
                continue;
            }
            E.AttackTimer-=StepSeconds;
            if(E.AttackTimer<=0)
            {
                E.AttackTimer=1.2;
                HurtPlayer(E.Attack);
            }
        }
    }

    bool FSimulator::HasPair(EWanJianElement A,EWanJianElement B)const
    {
        return Elements.Contains(A)&&Elements.Contains(B);
    }

    void FSimulator::Finish(bool bVictory)
    {
        bFinished=true;
        Result.bVictory=bVictory;
        Result.Seconds=Time;
        Result.HealthLeft=FMath::Max(0.0,Health);
    }

    void FSimulator::Tick(bool bPlayerMoving)
    {
        if(IsFinished())return;
        Time+=StepSeconds;
        FocusSeconds=FMath::Max(0.0,FocusSeconds-StepSeconds);
        VitalityCooldown=FMath::Max(0.0,VitalityCooldown-StepSeconds);
        WardCooldown=FMath::Max(0.0,WardCooldown-StepSeconds);

        SpawnDue();
        UpdateMethods(bPlayerMoving);

        const double Speed=StepSeconds*(1.0+AttackSpeedBonus());
        for(FSwordState& State:Swords)
        {
            State.Lock=FMath::Max(0.0,State.Lock-StepSeconds);
            State.Cooldown-=Speed;
            if(State.Cooldown<=0)
            {
                FireSword(State);
                State.Cooldown+=State.Def->CooldownMs*0.001;
            }
        }
        UpdateEnemies();
        Result.HealthLeft=FMath::Max(0.0,Health);

        if(Health<=0)
        {
            Finish(false);
            return;
        }
        const bool bAllSpawned=NextSpawn>=Spawns.Num();
        const bool bAnyAlive=Enemies.ContainsByPredicate([](const FEnemy& E){return E.IsAlive();});
        if(bAllSpawned&&!bAnyAlive)Finish(true);
        else if(Time>=Stage.Duration+OvertimeSeconds)Finish(false);
    }

    FResult Run(const FSetup& Setup)
    {
        FSimulator Simulator(Setup);
        while(!Simulator.IsFinished())Simulator.Tick();
        return Simulator.GetResult();
    }

    FSetup ReferenceSetup(int32 World,int32 Index,int32 Seed,int32 Team)
    {
        FStage Stage;
        MakeStage(World,Index,Stage);
        FSetup Setup;
        Setup.StageId=Stage.Id;
        Setup.Seed=Seed;
        Setup.Team=Team;
        Setup.Attack=ReferenceAttack(Stage.Ordinal);
        Setup.MaxHealth=ReferenceHealth(Stage.Ordinal);
        Setup.Root=EWanJianElement::Metal;
        const int32 Slots=UnlockedSlots(Stage.Ordinal-1);
        // Damage dealers first, so an early two-slot loadout is not two healers.
        TArray<FName,TInlineAllocator<12>> Pool;
        for(const FSword& S:Swords())
        {
            if(S.Attack!=EAttack::Support)Pool.Add(S.Id);
        }
        for(int32 Slot=0;Slot<Slots;++Slot)
        {
            const int32 Copy=Slot/Pool.Num();
            Setup.Swords.Add(Copy<3?Pool[Slot%Pool.Num()]:StarterSword());
        }
        return Setup;
    }
}

#include "WanJian/WanJianRules.h"
#include "WanJian/WanJianCatalog.h"

namespace WanJianRules
{
    namespace
    {
        constexpr double ReferenceCoefficient=0.82;
        constexpr double ReferenceCooldown=1.3;
        /** Share of the reference loadout's total damage that the enemies of a stage are given. */
        constexpr double HealthBudget=1.4;
        constexpr double TutorialStartBudget=0.45;
        constexpr int32 EliteWeight=5;
        constexpr int32 BossWeight=30;
        constexpr int32 WorldBossWeight=45;

        double ReferenceDefense(int32 Ordinal){return 10.0+0.25*Ordinal;}

        /** Damage the reference loadout can deal over a whole stage. */
        double ReferenceDamage(int32 Ordinal,int32 Duration)
        {
            const int32 Slots=UnlockedSlots(Ordinal-1);
            const double Dps=Slots*ReferenceAttack(Ordinal)*ReferenceCoefficient/ReferenceCooldown*DefenseFactor(ReferenceDefense(Ordinal));
            return Dps*Duration;
        }
    }

    int32 StageId(int32 World,int32 Index){return World*1000+Index;}

    bool SplitStageId(int32 Id,int32& OutWorld,int32& OutIndex)
    {
        OutWorld=Id/1000;
        OutIndex=Id%1000;
        return OutWorld>=1&&OutWorld<=WorldCount&&OutIndex>=1&&OutIndex<=StagesPerWorld;
    }

    bool MakeStage(int32 World,int32 Index,FStage& Out)
    {
        if(World<1||World>WorldCount||Index<1||Index>StagesPerWorld)return false;
        Out=FStage();
        Out.Id=StageId(World,Index);
        Out.World=World;
        Out.Index=Index;
        Out.Ordinal=(World-1)*StagesPerWorld+Index;
        Out.Chapter=(Index-1)/StagesPerChapter+1;
        Out.bOpen=World<=OpenWorldCount;
        Out.bTutorial=World==1&&Index<=TutorialStages;
        Out.bBoss=Index%StagesPerChapter==0;
        Out.bWorldBoss=Index==StagesPerWorld;
        Out.Template=Out.bTutorial?Index-1:TutorialStages+((Index+World)%BaseTemplates);

        const int32 InChapter=(Index-1)%StagesPerChapter;
        Out.Duration=Out.bTutorial?45+Index*3:Out.bBoss?180:60+InChapter*4;
        Out.Normals=FMath::Clamp(14+Out.Ordinal/5+(Out.Template%4)*3,14,50);
        Out.Elites=Out.bTutorial?0:FMath::Clamp(Out.Chapter+(Out.Template%3),1,8);
        Out.Defense=ReferenceDefense(Out.Ordinal);
        Out.PathSeconds=FMath::Max(16.0,Out.Duration*0.35);

        const int32 Weight=Out.Normals+Out.Elites*EliteWeight+(Out.bBoss?(Out.bWorldBoss?WorldBossWeight:BossWeight):0);
        // The guided opening starts soft enough for the single bound sword a new player owns, then ramps to the curve.
        const double Ramp=Out.bTutorial?FMath::Lerp(TutorialStartBudget,1.0,(Index-1)/double(TutorialStages-1)):1.0;
        Out.NormalHealth=FMath::Max(1.0,ReferenceDamage(Out.Ordinal,Out.Duration)*HealthBudget*Ramp/Weight);
        Out.Attack=ReferenceHealth(Out.Ordinal)*0.02;
        return true;
    }

    int32 PreviousStage(int32 Id)
    {
        int32 World=0,Index=0;
        if(!SplitStageId(Id,World,Index))return 0;
        if(Index>1)return StageId(World,Index-1);
        return World>1?StageId(World-1,StagesPerWorld):0;
    }

    int32 UnlockedSlots(int32 ClearedStages)
    {
        return ClearedStages>=120?MaxLineup:ClearedStages>=60?12:ClearedStages>=20?6:2;
    }

    double ReferenceAttack(int32 Ordinal){return 20.0+3.0*Ordinal;}
    double ReferenceHealth(int32 Ordinal){return 200.0+20.0*Ordinal;}
    double PlayerAttack(int32 Realm,double AverageSwordLevel){return 20.0+12.0*Realm+6.0*AverageSwordLevel;}
    double PlayerHealth(int32 Realm){return 240.0+80.0*Realm;}
    double DefenseFactor(double Defense){return 100.0/(100.0+FMath::Max(0.0,Defense));}
    int64 BreakthroughQi(int32 Realm){return int64(FMath::RoundToDouble(120.0*FMath::Pow(1.35,double(Realm))));}
    int32 BreakthroughStones(int32 Realm){return 1+Realm/3;}

    FName RollDraw(FRandomStream& Stream,FWanJianPity& Pity,bool& bOutGuaranteed)
    {
        using namespace WanJianCatalog;
        bOutGuaranteed=false;
        ++Pity.TotalDraws;
        if(!Pity.bSevenStarOwned)++Pity.Draws;

        FName Result;
        if(!Pity.bSevenStarOwned&&Pity.Draws>=PityDraws)
        {
            Result=SevenStarSword();
            bOutGuaranteed=true;
        }
        else
        {
            // Per ten thousand: 普通 72.0%, 优秀 23.0%, 稀有 4.5%, 传说 0.5% of which 七星 0.1%.
            const int32 Roll=Stream.RandRange(0,9999);
            if(Roll<10)
            {
                Result=SevenStarSword();
            }
            else if(Roll<50)
            {
                Result=TEXT("Xinghen");
            }
            else
            {
                const EWanJianRarity Tier=Roll<500?EWanJianRarity::Rare:Roll<2800?EWanJianRarity::Fine:EWanJianRarity::Common;
                TArray<FName,TInlineAllocator<8>> Pool;
                for(const FSword& S:Swords())
                {
                    if(S.bInPool&&S.Rarity==Tier)Pool.Add(S.Id);
                }
                Result=Pool[Stream.RandRange(0,Pool.Num()-1)];
            }
        }
        if(Result==SevenStarSword()&&!Pity.bSevenStarOwned)
        {
            Pity.bSevenStarOwned=true;
            Pity.AcquiredAtDraw=Pity.Draws;
        }
        return Result;
    }

    int32 KillCriticalUnits(EKill Kill,int32 Team)
    {
        const int32 Units=Kill==EKill::Boss?1200:Kill==EKill::Elite?120:12;
        return Units/FMath::Clamp(Team,1,MaxTeam);
    }

    double TeamDropChance(double BaseChance,int32 Team)
    {
        return FMath::Min(1.0,FMath::Clamp(Team,1,MaxTeam)*BaseChance);
    }

    int32 RollDropQuantity(FRandomStream& Stream,int32 Team)
    {
        if(Team<=1)return 1;
        const int32 Roll=Stream.RandRange(0,99);
        return Roll<70?1:Roll<90?2:Roll<98?3:4;
    }

    double ExpectedDropQuantity(int32 Team){return Team<=1?1.0:0.70+0.20*2+0.08*3+0.02*4;}
    double SpawnMultiplier(int32 Team){return 1.0+0.8*(FMath::Clamp(Team,1,MaxTeam)-1);}
    double TeamHealthMultiplier(int32 Team){return 1.0+0.35*(FMath::Clamp(Team,1,MaxTeam)-1);}

    int32 Production(double ElapsedSeconds,double IntervalSeconds,int32 PerInterval,int32 Storage)
    {
        if(ElapsedSeconds<=0||IntervalSeconds<=0)return 0;
        const double Counted=FMath::Min(ElapsedSeconds,MaxOfflineSeconds);
        return FMath::Min(FMath::FloorToInt(Counted/IntervalSeconds)*PerInterval,Storage);
    }

    double GardenIntervalSeconds(int32 Level){return Level>=3?420.0:Level==2?510.0:600.0;}
    int32 GardenStorage(int32 Level){return 24+(FMath::Max(Level,1)-1)*4;}

    FString WeekKey(double Utc)
    {
        // 1970-01-05 was a Monday, four days after the epoch.
        const int64 Days=FMath::FloorToInt64((Utc+8.0*3600.0)/86400.0);
        return FString::Printf(TEXT("W%lld"),FMath::FloorToInt64((Days-4)/7.0));
    }
}

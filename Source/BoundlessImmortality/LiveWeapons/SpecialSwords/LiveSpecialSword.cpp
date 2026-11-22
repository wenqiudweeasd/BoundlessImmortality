#include "LiveWeapons/SpecialSwords/LiveSpecialSword.h"
#include "LiveWeapons/SpecialSwords/LiveSpecialSwordKinds.h"
#include "LiveWeapons/SpecialSwords/LiveSpecialSwordComponent.h"
#include "LiveWeapons/SpecialSwords/LiveAnchorBuffSubsystem.h"
#include "LiveWeapons/SpecialSwords/LiveSwordProjectile.h"
#include "LiveWeapons/LiveMonster.h"
#include "LiveWeapons/LiveMonsterRegistrySubsystem.h"
#include "LiveWeapons/LiveWeaponSwarm.h"
#include "Cultivation/CultivationSubsystem.h"
#include "Cultivation/CultivationHealthComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "NiagaraComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/AnimSequence.h"
#include "GameFramework/Character.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

#define LOCTEXT_NAMESPACE "LiveSpecialSword"

TSubclassOf<ALiveSpecialSword> ALiveSpecialSword::ClassFor(ELiveSpecialSword K)
{
    switch(K)
    {
    case ELiveSpecialSword::SoulEater:return ALiveSwordSoulEater::StaticClass();
    case ELiveSpecialSword::Inorganic:return ALiveSwordInorganic::StaticClass();
    case ELiveSpecialSword::DivineBlade:return ALiveSwordDivineBlade::StaticClass();
    case ELiveSpecialSword::SevenStar:return ALiveSwordSevenStar::StaticClass();
    case ELiveSpecialSword::Godslayer:return ALiveSwordGodslayer::StaticClass();
    default:return nullptr;
    }
}

ALiveSpecialSword::ALiveSpecialSword()
{
    PrimaryActorTick.bCanEverTick=true;PrimaryActorTick.TickGroup=TG_PostPhysics;
    Pivot=CreateDefaultSubobject<USceneComponent>(TEXT("Pivot"));SetRootComponent(Pivot);
    Blade=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Blade"));Blade->SetupAttachment(Pivot);
    Blade->SetCollisionEnabled(ECollisionEnabled::NoCollision);Blade->SetGenerateOverlapEvents(false);Blade->SetCanEverAffectNavigation(false);
    SkeletalBlade=CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("SkeletalBlade"));SkeletalBlade->SetupAttachment(Pivot);
    SkeletalBlade->SetCollisionEnabled(ECollisionEnabled::NoCollision);SkeletalBlade->SetGenerateOverlapEvents(false);SkeletalBlade->SetVisibility(false);
    SetActorEnableCollision(false);
}

void ALiveSpecialSword::InitializeSword(ULiveSpecialSwordComponent* InManager,ALiveWeaponSwarm* InSwarm,const FString& InParticipantId,const FString& InDisplayName)
{
    Manager=InManager;Swarm=InSwarm;ParticipantId=InParticipantId;DisplayName=InDisplayName;
    Random.Initialize(int32(GetTypeHash(InParticipantId)^0x5eed));
    OrbitAngle=Random.FRandRange(0.f,2*PI);OrbitRadius=Random.FRandRange(210.f,300.f);OrbitHeight=Random.FRandRange(120.f,190.f);
    AttackInterval=LiveSpecialSwords::Stats(Kind).AttackInterval;
    SetupForm();
    RefreshStats();
    Trail=AttachFx(Pivot,LiveSwordVfx::EFx::Trail,Tint(),BladeLength*.16f,HiltOffset());
    Aura=AttachFx(Pivot,LiveSwordVfx::EFx::Aura,Tint(),BladeLength);
    // Skills start ready, staggered so a crowd of the same form does not cast in lockstep.
    for(const auto& S:LiveSpecialSwords::Skills(Kind))if(!S.bPassive)SkillReadyAt.Add(S.Id,Random.FRandRange(1.5f,4.f));
    PlayFx(LiveSwordVfx::EFx::Burst,GetActorLocation(),Tint(),110.f);
    PushFloater(LiveSpecialSwords::Name(Kind).ToString()+TEXT(" · 觉醒"),Tint(),GetActorLocation()+FVector(0,0,80));
}

void ALiveSpecialSword::EndPlay(const EEndPlayReason::Type Reason)
{
    while(Ghosts.Num())ReleaseGhost(Ghosts.Num()-1,false);
    if(auto* B=Buffs())B->RemoveSource(ParticipantId);
    Super::EndPlay(Reason);
}

void ALiveSpecialSword::SetupBlade(UStaticMesh* Mesh,float Length)
{
    if(!Mesh)Mesh=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube"));
    if(!Mesh)return;
    Blade->SetStaticMesh(Mesh);Blade->SetVisibility(true);SkeletalBlade->SetVisibility(false);
    const FBox Box=Mesh->GetBoundingBox();const FVector Size=Box.GetSize();
    // Longest axis becomes the flight axis; the far end from the grip pivot is the tip.
    int32 Axis=2;if(Size.X>Size[Axis])Axis=0;if(Size.Y>Size[Axis])Axis=1;
    FVector Tip=FVector::ZeroVector;Tip[Axis]=FMath::Abs(Box.Min[Axis])>FMath::Abs(Box.Max[Axis])*1.15?-1:1;
    const FQuat Fix=FQuat::FindBetweenNormals(Tip,FVector::UpVector);
    const float Scale=Length/FMath::Max(1.f,float(Size[Axis]));
    Blade->SetRelativeRotation(Fix);Blade->SetRelativeScale3D(FVector(Scale));
    Blade->SetRelativeLocation(-Fix.RotateVector(Box.GetCenter()*Scale));
    BladeLength=Length;
}

void ALiveSpecialSword::SetupSkeletal(USkeletalMesh* Mesh,UAnimSequence* Anim,float Length)
{
    if(!Mesh){SetupBlade(nullptr,Length);return;}
    SkeletalBlade->SetSkeletalMesh(Mesh);SkeletalBlade->SetVisibility(true);Blade->SetVisibility(false);
    const FBoxSphereBounds Bounds=Mesh->GetBounds();const FBox Box=Bounds.GetBox();const FVector Size=Box.GetSize();
    int32 Axis=2;if(Size.X>Size[Axis])Axis=0;if(Size.Y>Size[Axis])Axis=1;
    FVector Tip=FVector::ZeroVector;Tip[Axis]=FMath::Abs(Box.Min[Axis])>FMath::Abs(Box.Max[Axis])*1.15?-1:1;
    const FQuat Fix=FQuat::FindBetweenNormals(Tip,FVector::UpVector);
    const float Scale=Length/FMath::Max(1.f,float(Size[Axis]));
    SkeletalBlade->SetRelativeRotation(Fix);SkeletalBlade->SetRelativeScale3D(FVector(Scale));
    SkeletalBlade->SetRelativeLocation(-Fix.RotateVector(Box.GetCenter()*Scale));
    if(Anim)SkeletalBlade->PlayAnimation(Anim,true);
    BladeLength=Length;
}

APawn* ALiveSpecialSword::Anchor()const{const ALiveWeaponSwarm* S=Swarm.Get();return S?S->GetAnchor():nullptr;}
ULiveAnchorBuffSubsystem* ALiveSpecialSword::Buffs()const{return ULiveAnchorBuffSubsystem::Get(this);}

bool ALiveSpecialSword::IsAnchorDown()const
{
    const UGameInstance* GI=GetGameInstance();const UCultivationSubsystem* C=GI?GI->GetSubsystem<UCultivationSubsystem>():nullptr;
    const UCultivationHealthComponent* H=C?C->GetAnchorHealth():nullptr;
    return !Anchor()||(H&&H->bDead);
}

FVector ALiveSpecialSword::AnchorCenter()const{const APawn* A=Anchor();return A?A->GetActorLocation():GetActorLocation();}

float ALiveSpecialSword::FloorZ()const
{
    const APawn* A=Anchor();if(!A)return GetActorLocation().Z-150;
    float Z=A->GetActorLocation().Z;
    if(const ACharacter* C=Cast<ACharacter>(A))Z-=C->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    return Z;
}

FVector ALiveSpecialSword::IdlePoint()const
{
    return AnchorCenter()+FVector(FMath::Cos(OrbitAngle)*OrbitRadius,FMath::Sin(OrbitAngle)*OrbitRadius,OrbitHeight+14*FMath::Sin(Clock*1.7f));
}

void ALiveSpecialSword::RefreshStats()
{
    const UGameInstance* GI=GetGameInstance();const UCultivationSubsystem* C=GI?GI->GetSubsystem<UCultivationSubsystem>():nullptr;
    if(!C)return;
    const FCultivationParticipantSnapshot S=C->GetParticipantSnapshot(ParticipantId);
    if(S.ParticipantId.IsEmpty())return;
    // The viewer's sword attack already carries the realm curve, gifts and methods; the form's sheet scales it by its own
    // curve over the normal spirit's, and adds its critical, critical damage and 破甲 at that realm.
    Realm=FMath::Clamp(S.RealmIndex,0,33);
    CachedAttack=FMath::Clamp((S.Attributes.Attack+S.Attributes.SpiritDamage)*LiveSpecialSwords::AttackScale(Kind,Realm),1.0,1.e12);
    CachedCritical=FMath::Clamp(S.Attributes.CriticalChance+LiveSpecialSwords::CriticalAt(Kind,Realm),0.0,1.0);
    CachedCriticalDamage=LiveSpecialSwords::CriticalDamageAt(Kind,Realm);
    CachedPenetration=LiveSpecialSwords::PenetrationAt(Kind,Realm);
}

float ALiveSpecialSword::BaseAttackPower()const{return float(CachedAttack);}
float ALiveSpecialSword::AttackPower()const{return float(FMath::Min(1.e12,CachedAttack+FMath::Max(0.f,GrowthAttack())));}

void ALiveSpecialSword::Steer(const FVector& Desired,float DeltaSeconds,float MaxSpeed,float Response)
{
    const FVector Want=((Desired-GetActorLocation())*Response).GetClampedToMaxSize(MaxSpeed);
    Velocity=FMath::VInterpTo(Velocity,Want,DeltaSeconds,9.f);
    SetActorLocation(GetActorLocation()+Velocity*DeltaSeconds);
}

void ALiveSpecialSword::Face(const FVector& Direction,float DeltaSeconds,float Speed)
{
    if(Direction.IsNearlyZero(1.e-3))return;
    const FQuat Target=FRotationMatrix::MakeFromZ(Direction.GetSafeNormal()).ToQuat();
    Facing=FMath::QInterpTo(Facing,Target,DeltaSeconds,Speed);
}

ALiveMonster* ALiveSpecialSword::NearestEnemy(const FVector& From,float Range,const ALiveMonster* Exclude)const
{
    auto* Registry=GetWorld()?GetWorld()->GetSubsystem<ULiveMonsterRegistrySubsystem>():nullptr;if(!Registry)return nullptr;
    TArray<TWeakObjectPtr<ALiveMonster>> Near;Registry->CollectInRange(From,Range,Near);
    ALiveMonster* Best=nullptr;double BestDistance=MAX_dbl;
    for(const auto& W:Near)if(ALiveMonster* M=W.Get();M&&M!=Exclude&&M->IsAlive())
    {const double D=FVector::DistSquared(From,M->GetActorLocation());if(D<BestDistance){BestDistance=D;Best=M;}}
    return Best;
}

void ALiveSpecialSword::Enemies(const FVector& Center,float Radius,TArray<ALiveMonster*>& Out)const
{
    Out.Reset();auto* Registry=GetWorld()?GetWorld()->GetSubsystem<ULiveMonsterRegistrySubsystem>():nullptr;if(!Registry)return;
    TArray<TWeakObjectPtr<ALiveMonster>> Near;Registry->CollectInRange(Center,Radius,Near);
    for(const auto& W:Near)if(ALiveMonster* M=W.Get();M&&M->IsAlive())Out.Add(M);
    Out.Sort([&](const ALiveMonster& A,const ALiveMonster& B){return FVector::DistSquared(Center,A.GetActorLocation())<FVector::DistSquared(Center,B.GetActorLocation());});
}

FVector ALiveSpecialSword::HitPoint(const ALiveMonster* M)const{return M?M->GetActorLocation()+FVector(0,0,20):GetActorLocation();}

float ALiveSpecialSword::CooldownOf(FName Id)const
{
    for(const auto& S:LiveSpecialSwords::Skills(Kind))if(S.Id==Id)return S.Cooldown;
    return 5.f;
}

void ALiveSpecialSword::StartCooldown(FName Id){SkillReadyAt.Add(Id,Clock+CooldownOf(Id));}

float ALiveSpecialSword::DealDamage(ALiveMonster* Target,float Amount,FName SkillId,const FVector& At,bool bAllowFollowUp)
{
    if(!IsValid(Target)||!Target->IsAlive()||!FMath::IsFinite(Amount)||Amount<0)return 0;
    FLiveHit Hit;Hit.Attack=AttackPower();Hit.SkillCoefficient=Amount/FMath::Max(1.f,Hit.Attack);
    Hit.DamageBonus=DamageBonus();Hit.ArmorPenetration=float(CachedPenetration);Hit.AttackerRealm=Realm;
    // The stored release is a fixed amount and never crits. 七星 bolts crit on their attack share only: their share of the
    // target's health is true damage outside the formula.
    if(SkillId!=TEXT("Devastate")){Hit.CriticalChance=float(CachedCritical);Hit.CriticalDamage=float(CachedCriticalDamage);}
    ModifyHit(Target,Hit,SkillId);
    FLiveHitResult Result;
    const float Applied=Target->ApplyHit(Hit,ParticipantId,&Random,&Result);
    const bool bCritical=Result.bCritical;
    if(Applied>0)
    {
        if(auto* M=Manager.Get())M->NotifyDamage(ParticipantId,Applied);
        SkillDamage.FindOrAdd(SkillId)+=Applied;
        // Clones, spins and bolts hit one target many times a second: those add into its open number, which re-pops,
        // instead of piling up unreadable copies. Crits always stand alone. The drift is cosmetic, so it stays off the
        // sword's own random stream and leaves crit and devour rolls untouched.
        FDamageNumber* Open=nullptr;
        if(!bCritical)for(FDamageNumber& N:DamageNumbers)if(!N.bCritical&&N.Age<.3f&&N.Target.Get()==Target)Open=&N;
        if(Open){Open->Amount+=Applied;Open->Age=FMath::Min(Open->Age,.05f);Open->bSkill|=SkillId!=TEXT("Attack");}
        else
        {
            if(DamageNumbers.Num()>=24)DamageNumbers.RemoveAt(0);
            FDamageNumber Number;Number.Target=Target;Number.Amount=Applied;Number.Location=Target->GetHealthBarLocation()+FVector(0,0,24);
            Number.Drift=FMath::FRandRange(-1.f,1.f);Number.bCritical=bCritical;Number.bSkill=SkillId!=TEXT("Attack");DamageNumbers.Add(Number);
        }
    }
    AfterHit(Target,Applied,!Target->IsAlive(),SkillId,bAllowFollowUp);
    return Applied;
}

void ALiveSpecialSword::Launch(const FVector& From,const FLiveSwordShot& Shot)
{
    FActorSpawnParameters Params;Params.Owner=this;Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    FLiveSwordShot Styled=Shot;Styled.Style=Style();
    if(ALiveSwordProjectile* P=GetWorld()->SpawnActor<ALiveSwordProjectile>(ALiveSwordProjectile::StaticClass(),From,Shot.Velocity.Rotation(),Params))P->Launch(this,Styled);
}

void ALiveSpecialSword::BeginDash(ALiveMonster* Target)
{
    DashTarget=Target;Mode=EMode::Dash;ModeTime=0;
}

bool ALiveSpecialSword::CastSkillNow(FName SkillId)
{
    if((Mode!=EMode::Idle&&Mode!=EMode::Recover)||!Anchor()||!ActiveSkills().Contains(SkillId)||!CanCast(SkillId))return false;
    CastingSkill=SkillId;Mode=EMode::Cast;ModeTime=0;StartCooldown(SkillId);BeginCast(SkillId);
    return true;
}

void ALiveSpecialSword::FinishCast()
{
    CastingSkill=NAME_None;Mode=EMode::Recover;ModeTime=0;bChainPending=true;
}

void ALiveSpecialSword::Decide()
{
    if(SuppressAttacks()||IsAnchorDown())return;
    if(const FName Priority=PriorityCast();!Priority.IsNone())
    {
        CastingSkill=Priority;Mode=EMode::Cast;ModeTime=0;BeginCast(Priority);return;
    }
    ALiveMonster* Target=NearestEnemy(AnchorCenter(),EngageRange());
    if(!Target){bChainPending=false;return;}
    TArray<FName> Ready;for(const FName S:ActiveSkills())if(IsSkillReady(S)&&CanCast(S))Ready.Add(S);
    const bool bChain=bChainPending;bChainPending=false;
    auto Cast=[&]
    {
        const FName S=Ready[Random.RandRange(0,Ready.Num()-1)];
        CastingSkill=S;Mode=EMode::Cast;ModeTime=0;StartCooldown(S);BeginCast(S);
    };
    if(bChain)
    {
        // After a cast: half the time chain into another ready skill, otherwise strike at once.
        if(Ready.Num()&&Random.FRand()<.5f)Cast();else BeginDash(Target);
        return;
    }
    if(Ready.Num()){Cast();return;}
    if(AttackCooldown<=0)BeginDash(Target);
}

void ALiveSpecialSword::TickIdle(float DeltaSeconds)
{
    OrbitAngle=FMath::Fmod(OrbitAngle+DeltaSeconds*.55f,2*PI);
    Steer(IdlePoint(),DeltaSeconds,1400.f,3.5f);
    const FVector Tangent(-FMath::Sin(OrbitAngle),FMath::Cos(OrbitAngle),0);
    Face(FVector::UpVector*.9f+Tangent*.35f+FVector(0,0,.1f)*FMath::Sin(Clock),DeltaSeconds,4.f);
    Decide();
}

void ALiveSpecialSword::TickDash(float DeltaSeconds)
{
    ALiveMonster* T=DashTarget.Get();
    if(!T||!T->IsAlive())
    {
        T=NearestEnemy(GetActorLocation(),700.f);
        if(!T){Mode=EMode::Recover;ModeTime=0;return;}
        DashTarget=T;
    }
    const FVector Goal=HitPoint(T),Before=GetActorLocation();
    Steer(Goal,DeltaSeconds,2900.f,13.f);
    Face(Velocity,DeltaSeconds,22.f);
    if(FMath::PointDistToSegment(Goal,Before,GetActorLocation())<75.f)
    {
        DealDamage(T,AttackPower(),TEXT("Attack"),Goal);
        PlayFx(LiveSwordVfx::EFx::Slash,Goal,Tint(),120.f,Velocity.Rotation());
        OnNormalHit(T);
        AttackCooldown=AttackInterval*Random.FRandRange(.85f,1.15f);
        Velocity*=-.35f;Mode=EMode::Recover;ModeTime=0;
        return;
    }
    if(ModeTime>2.5f){Mode=EMode::Recover;ModeTime=0;}
}

void ALiveSpecialSword::TickRecover(float DeltaSeconds)
{
    Steer(IdlePoint(),DeltaSeconds,1800.f,6.f);
    Face(Velocity.SizeSquared()>2500?Velocity:FVector::UpVector,DeltaSeconds,10.f);
    if(ModeTime>.35f){Mode=EMode::Idle;ModeTime=0;Decide();}
}

void ALiveSpecialSword::ApplyTransform(float DeltaSeconds){SetActorRotation(Facing);}

void ALiveSpecialSword::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if(!FMath::IsFinite(DeltaSeconds)||DeltaSeconds<=0)return;
    DeltaSeconds=FMath::Min(DeltaSeconds,.1f);
    Clock+=DeltaSeconds;ModeTime+=DeltaSeconds;AttackCooldown-=DeltaSeconds;
    for(FFloater& F:Floaters)F.Age+=DeltaSeconds;
    Floaters.RemoveAll([](const FFloater& F){return F.Age>1.8f;});
    for(FDamageNumber& N:DamageNumbers)N.Age+=DeltaSeconds;
    DamageNumbers.RemoveAll([](const FDamageNumber& N){return N.Age>DamageNumberLife;});
    if((StatsTimer-=DeltaSeconds)<=0){StatsTimer=.5f;RefreshStats();}
    if(!Anchor())return;
    TickKind(DeltaSeconds);
    switch(Mode)
    {
    case EMode::Idle:TickIdle(DeltaSeconds);break;
    case EMode::Dash:TickDash(DeltaSeconds);break;
    case EMode::Recover:TickRecover(DeltaSeconds);break;
    case EMode::Cast:TickCast(DeltaSeconds);break;
    case EMode::Guard:TickGuard(DeltaSeconds);break;
    }
    // A sword that drifts far (teleporting owner, long dash) catches up instead of crossing the map.
    if(FVector::DistSquared(GetActorLocation(),AnchorCenter())>FMath::Square(4000.f))SetActorLocation(IdlePoint());
    ApplyTransform(DeltaSeconds);
    PlaceGhosts(DeltaSeconds);
}

void ALiveSpecialSword::PushFloater(const FString& Text,const FLinearColor& Color,const FVector& At)
{
    if(Floaters.Num()>=8)Floaters.RemoveAt(0);
    FFloater F;F.Text=Text;F.Color=Color;F.Location=At;Floaters.Add(F);
}

FVector ALiveSpecialSword::GetOverheadLocation()const{return GetActorLocation()+FVector(0,0,BladeLength*.55f+36.f);}

FText ALiveSpecialSword::GetStatusText()const
{
    switch(Mode)
    {
    case EMode::Dash:return LOCTEXT("Dash","出剑");
    case EMode::Cast:
        for(const auto& S:LiveSpecialSwords::Skills(Kind))if(S.Id==CastingSkill)return FText::Format(LOCTEXT("Casting","施放 · {0}"),S.Name);
        return LOCTEXT("CastingAxe","机魂化斧");
    case EMode::Guard:return LOCTEXT("Guard","守护主播");
    default:return LOCTEXT("Ready","待命");
    }
}

void ALiveSpecialSword::GetSkillStates(TArray<FLiveSpecialSkillState>& Out)const
{
    Out.Reset();
    for(const auto& S:LiveSpecialSwords::Skills(Kind))
    {
        FLiveSpecialSkillState State;State.SkillId=S.Id;State.Name=S.Name;State.bPassive=S.bPassive;State.Cooldown=S.Cooldown;
        State.Remaining=S.bPassive?0.f:FMath::Max(0.f,SkillReadyAt.FindRef(S.Id)-Clock);
        State.bActive=Mode==EMode::Cast&&CastingSkill==S.Id;Out.Add(State);
    }
}

int32 ALiveSpecialSword::SpawnGhost(const FVector& At,float Opacity)
{
    UStaticMesh* Mesh=Blade->GetStaticMesh();if(!Mesh)return INDEX_NONE;
    UStaticMeshComponent* M=NewObject<UStaticMeshComponent>(this);
    M->SetupAttachment(Pivot);M->SetUsingAbsoluteLocation(true);M->SetUsingAbsoluteRotation(true);M->SetUsingAbsoluteScale(true);
    M->SetStaticMesh(Mesh);M->SetCollisionEnabled(ECollisionEnabled::NoCollision);M->SetGenerateOverlapEvents(false);M->SetCastShadow(false);
    if(UMaterialInterface* Ghost=LiveSwordVfx::GhostMaterial(Style()))
    {
        UMaterialInstanceDynamic* MID=UMaterialInstanceDynamic::Create(Ghost,M);
        MID->SetVectorParameterValue(TEXT("Color"),Tint()*2.5f);MID->SetScalarParameterValue(TEXT("Opacity"),Opacity);
        for(int32 I=0;I<FMath::Max(1,M->GetNumMaterials());++I)M->SetMaterial(I,MID);
    }
    M->RegisterComponent();
    FGhost G;G.Mesh=M;G.Position=At;G.Facing=Facing;G.Angle=Random.FRandRange(0.f,2*PI);
    // The clone mesh carries the blade's fix-up transform, so the hilt is mapped back into its mesh space.
    G.Trail=AttachFx(M,LiveSwordVfx::EFx::Trail,Tint(),BladeLength*.1f,Blade->GetRelativeTransform().InverseTransformPosition(HiltOffset()));
    const int32 Index=Ghosts.Add(G);
    PlaceGhosts(0.f);
    return Index;
}

void ALiveSpecialSword::ReleaseGhost(int32 Index,bool bShatter)
{
    if(!Ghosts.IsValidIndex(Index))return;
    FGhost& G=Ghosts[Index];
    if(bShatter){PlayFx(LiveSwordVfx::EFx::Shatter,G.Position,Tint(),BladeLength*.6f);}
    if(G.Trail){G.Trail->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);G.Trail->SetAutoDestroy(true);G.Trail->Deactivate();}
    if(G.Mesh)G.Mesh->DestroyComponent();
    Ghosts.RemoveAt(Index);
}

void ALiveSpecialSword::SteerGhost(FGhost& G,const FVector& Desired,float DeltaSeconds,float MaxSpeed,float Response)
{
    const FVector Want=((Desired-G.Position)*Response).GetClampedToMaxSize(MaxSpeed);
    G.Velocity=FMath::VInterpTo(G.Velocity,Want,DeltaSeconds,9.f);G.Position+=G.Velocity*DeltaSeconds;
    if(!G.Velocity.IsNearlyZero(5.f))G.Facing=FMath::QInterpTo(G.Facing,FRotationMatrix::MakeFromZ(G.Velocity.GetSafeNormal()).ToQuat(),DeltaSeconds,16.f);
}

void ALiveSpecialSword::PlaceGhosts(float DeltaSeconds)
{
    const FTransform Fix=Blade->GetRelativeTransform();
    for(FGhost& G:Ghosts)if(G.Mesh)G.Mesh->SetWorldTransform(Fix*FTransform(G.Facing,G.Position));
}

#undef LOCTEXT_NAMESPACE

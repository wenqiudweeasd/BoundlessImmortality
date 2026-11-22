#include "LiveScarecrow.h"
#include "LiveMonsterRegistrySubsystem.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/PlayerController.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"
namespace
{
    FString ScarecrowNumber(float Value)
    {
        return Value>=1e6f?FString::Printf(TEXT("%.2fM"),Value/1e6f):Value>=1e4f?FString::Printf(TEXT("%.1fk"),Value/1e3f):FString::Printf(TEXT("%.0f"),Value);
    }
}
ALiveScarecrow::ALiveScarecrow()
{
    PrimaryActorTick.TickInterval=0;
    // With the local anchor's realm a special-sword hit lands for hundreds of thousands; a scarecrow should outlast
    // many skill rounds and fall only when a test asks for it (GM 怪物残血 for devours).
    MaxHealth=1e8f;Health=MaxHealth;MoveSpeed=0;AttackDamage=0;DebuffChance=0;bStationary=true;
    Collision->InitSphereRadius(55);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cone(TEXT("/Engine/BasicShapes/Cone.Cone"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
    // Built with the test range by Tools/SpecialSwords/build_sword_test_range.py.
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> Paint(TEXT("/Game/LiveWeapons/TestRange/M_Scarecrow.M_Scarecrow"));
    Figure=CreateDefaultSubobject<USceneComponent>(TEXT("Figure"));Figure->SetupAttachment(Collision);Figure->SetRelativeLocation(FVector(0,0,-FootDepth));
    auto Part=[this](UStaticMeshComponent* C,UStaticMesh* Mesh,const FVector& At,const FRotator& Turn,const FVector& Scale,const FLinearColor& Color)
    {
        C->SetupAttachment(Figure);C->SetStaticMesh(Mesh);C->SetRelativeLocationAndRotation(At,Turn);C->SetRelativeScale3D(Scale);
        C->SetCollisionEnabled(ECollisionEnabled::NoCollision);C->SetDefaultCustomPrimitiveDataVector4(0,FVector4(Color.R,Color.G,Color.B,1));Parts.Add(C);
    };
    auto New=[this](const TCHAR* Name){return CreateDefaultSubobject<UStaticMeshComponent>(Name);};
    const FLinearColor Wood(.16f,.08f,.035f),Straw(.62f,.45f,.16f),Cloth(.1f,.14f,.22f),Sash(.45f,.04f,.03f),Burlap(.48f,.36f,.22f);
    const FLinearColor Bamboo(.42f,.3f,.12f),Paper(.85f,.68f,.25f),Cinnabar(.6f,.03f,.02f),Stone(.18f,.18f,.17f);
    // The inherited cube becomes the post; a straw body in a faded robe hangs on the crossbar, under a bamboo hat.
    Part(Body,Cylinder.Object,FVector(0,0,90),FRotator::ZeroRotator,FVector(.08f,.08f,1.8f),Wood);
    Part(New(TEXT("Base")),Cylinder.Object,FVector(0,0,4),FRotator::ZeroRotator,FVector(.6f,.6f,.08f),Stone);
    Part(New(TEXT("Robe")),Cone.Object,FVector(0,0,96),FRotator::ZeroRotator,FVector(.6f,.6f,.62f),Cloth);
    Part(New(TEXT("Torso")),Cylinder.Object,FVector(0,0,128),FRotator::ZeroRotator,FVector(.34f,.34f,.5f),Straw);
    Part(New(TEXT("Sash")),Cylinder.Object,FVector(0,0,118),FRotator::ZeroRotator,FVector(.37f,.37f,.07f),Sash);
    Part(New(TEXT("Arms")),Cylinder.Object,FVector(0,0,146),FRotator(0,0,90),FVector(.06f,.06f,1.4f),Wood);
    for(int32 Side=0;Side<2;++Side)
    {
        const float S=Side?-1.f:1.f;
        Part(New(Side?TEXT("SleeveL"):TEXT("SleeveR")),Cylinder.Object,FVector(0,40*S,146),FRotator(0,0,90),FVector(.14f,.14f,.5f),Cloth);
        Part(New(Side?TEXT("HandL"):TEXT("HandR")),Cone.Object,FVector(0,76*S,146),FRotator(0,0,90*S),FVector(.18f,.18f,.24f),Straw);
    }
    Part(New(TEXT("Neck")),Cylinder.Object,FVector(0,0,158),FRotator::ZeroRotator,FVector(.12f,.12f,.12f),Straw);
    Part(New(TEXT("Head")),Sphere.Object,FVector(0,0,178),FRotator::ZeroRotator,FVector(.3f,.3f,.32f),Burlap);
    Part(New(TEXT("Hat")),Cone.Object,FVector(0,0,196),FRotator::ZeroRotator,FVector(.78f,.78f,.2f),Bamboo);
    // A paper talisman marks the front (+X).
    Part(New(TEXT("Talisman")),Cube.Object,FVector(18.5f,0,132),FRotator::ZeroRotator,FVector(.01f,.12f,.2f),Paper);
    Part(New(TEXT("TalismanInk")),Cube.Object,FVector(19.2f,0,132),FRotator::ZeroRotator,FVector(.004f,.03f,.15f),Cinnabar);
    if(Paint.Succeeded())for(UStaticMeshComponent* C:Parts)C->SetMaterial(0,Paint.Object);
    Readout=CreateDefaultSubobject<UTextRenderComponent>(TEXT("Readout"));Readout->SetupAttachment(Collision);Readout->SetRelativeLocation(FVector(0,0,236-FootDepth));
    Readout->SetHorizontalAlignment(EHTA_Center);Readout->SetVerticalAlignment(EVRTA_TextBottom);Readout->SetWorldSize(20);Readout->SetTextRenderColor(FColor(255,224,150));
    Readout->SetText(FText::FromString(TEXT("DPS 0")));Readout->SetCollisionEnabled(ECollisionEnabled::NoCollision);Readout->SetCastShadow(false);
}
void ALiveScarecrow::BeginPlay()
{
    if(RealmPreset>=0)
    {
        // Set before the monster's BeginPlay fills health from MaxHealth. It never attacks (stationary): the attack is
        // what the dummy is worth to a devour.
        Realm=FMath::Clamp(RealmPreset,0,33);MaxHealth=RealmHealth(Realm);AttackDamage=RealmAttack(Realm);Defense=RealmDefense(Realm);
    }
    Super::BeginPlay();
    Home=LastPlaced=GetActorLocation();LastHealth=Health;Readout->SetVisibility(bShowReadout);
}
void ALiveScarecrow::SetHealthFraction(float Fraction)
{
    if(!IsAlive())return;
    Health=FMath::Max(1.f,GetCurrentMaxHealth()*FMath::Clamp(Fraction,0.f,1.f));LastHealth=Health;
}
void ALiveScarecrow::Restore()
{
    if(IsKnockedDown())StandUp();
    Health=GetCurrentMaxHealth();LastHealth=Health;ResetStats();
}
void ALiveScarecrow::Die(const FString& Id)
{
    // Leaves the registry like a dead monster, so swords retarget and devours complete, but stays in the world.
    GetWorld()->GetSubsystem<ULiveMonsterRegistrySubsystem>()->Unregister(this);OnDeath(Id);SetActorEnableCollision(false);
    DownTime=RespawnDelay;++Knockdowns;
}
void ALiveScarecrow::StandUp()
{
    DownTime=0;Health=GetCurrentMaxHealth();LastHealth=Health;SetActorEnableCollision(true);SetActorLocation(Home);LastPlaced=Home;
    GetWorld()->GetSubsystem<ULiveMonsterRegistrySubsystem>()->Register(this);
}
float ALiveScarecrow::GetDamagePerSecond()const
{
    float Sum=0;for(const auto& Hit:Hits)Sum+=Hit.Value;return Sum/FMath::Max(.5f,DpsWindow);
}
void ALiveScarecrow::ResetStats(){TotalDamage=0;Knockdowns=0;Hits.Reset();}
void ALiveScarecrow::Tick(float DeltaSeconds)
{
    if(!FMath::IsFinite(DeltaSeconds)||DeltaSeconds<=0)return;
    // Damage arrives between ticks; health lost since the last tick is the damage taken, read before the enemy
    // multiplier refresh in the monster tick can rescale it.
    const double Now=GetWorld()->GetTimeSeconds();
    if(const float Lost=LastHealth-Health;Lost>0){TotalDamage+=Lost;Hits.Emplace(Now,Lost);Wobble=1;}
    Hits.RemoveAll([Now,this](const TPair<double,float>& Hit){return Now-Hit.Key>DpsWindow;});
    Super::Tick(DeltaSeconds);
    LastHealth=Health;
    float Tip=0;
    if(DownTime>0)
    {
        // Tips over backwards, lies still, then rises during the last half second.
        DownTime-=DeltaSeconds;
        if(DownTime<=0)StandUp();
        else Tip=80.f*FMath::Min(1.f,(RespawnDelay-DownTime)/.35f)*FMath::Min(1.f,DownTime/.5f);
    }
    else
    {
        // Pushes move the dummy like any monster; once nothing has moved it for a moment it slides back to its post.
        const FVector At=GetActorLocation();
        if(!At.Equals(LastPlaced,.1f))Settle=.45f;else Settle-=DeltaSeconds;
        if(Settle<=0&&!At.Equals(Home,.5f))SetActorLocation(FMath::VInterpTo(At,Home,DeltaSeconds,3.f));
        LastPlaced=GetActorLocation();
    }
    Wobble=FMath::Max(0.f,Wobble-DeltaSeconds*4.f);WobblePhase+=DeltaSeconds*38.f;
    Figure->SetRelativeRotation(FRotator(Tip,0,FMath::Sin(WobblePhase)*7.f*Wobble));
    UpdateReadout();
}
void ALiveScarecrow::UpdateReadout()
{
    if(!bShowReadout)return;
    const FString Top=IsKnockedDown()?FString::Printf(TEXT("KO x%d"),Knockdowns):TEXT("DPS ")+ScarecrowNumber(GetDamagePerSecond());
    Readout->SetText(FText::FromString(Top+TEXT("\nTOTAL ")+ScarecrowNumber(TotalDamage)));
    if(const auto* PC=GetWorld()->GetFirstPlayerController();PC&&PC->PlayerCameraManager)Readout->SetWorldRotation((-PC->PlayerCameraManager->GetCameraRotation().Vector()).Rotation());
}

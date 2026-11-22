#include "LiveWeapons/SpecialSwords/LiveSpecialSwordComponent.h"
#include "LiveWeapons/SpecialSwords/LiveSpecialSword.h"
#include "LiveWeapons/SpecialSwords/LiveSwordVfx.h"
#include "LiveWeapons/SpecialSwords/LiveAnchorBuffSubsystem.h"
#include "LiveWeapons/LiveWeaponSwarm.h"
#include "Cultivation/CultivationSubsystem.h"
#include "GameFramework/Pawn.h"
#include "JsonObjectConverter.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "EngineUtils.h"
#include "Engine/Font.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/SlateRenderer.h"
#include "Fonts/FontMeasure.h"

ULiveSpecialSwordComponent::ULiveSpecialSwordComponent()
{
    PrimaryComponentTick.bCanEverTick=true;PrimaryComponentTick.TickGroup=TG_PostPhysics;
}

ALiveWeaponSwarm* ULiveSpecialSwordComponent::GetSwarm()const{return Cast<ALiveWeaponSwarm>(GetOwner());}

ULiveSpecialSwordComponent* ULiveSpecialSwordComponent::Find(const UObject* WorldContext)
{
    UWorld* World=WorldContext?WorldContext->GetWorld():nullptr;if(!World)return nullptr;
    ULiveSpecialSwordComponent* Fallback=nullptr;
    for(TActorIterator<ALiveWeaponSwarm> It(World);It;++It)
    {
        if(It->GetPresentationMode()!=ELiveWeaponPresentationMode::Gameplay)continue;
        if(auto* C=It->FindComponentByClass<ULiveSpecialSwordComponent>()){if(It->GetAnchor())return C;Fallback=C;}
    }
    return Fallback;
}

void ULiveSpecialSwordComponent::BeginPlay()
{
    Super::BeginPlay();
    LoadGrowth();
    LiveSwordVfx::Preload(Preloaded);
}

void ULiveSpecialSwordComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    ReleaseAll();
    if(bGrowthDirty)SaveGrowth();
    if(Overlay){Overlay->RemoveFromParent();Overlay=nullptr;}
    Super::EndPlay(Reason);
}

ALiveSpecialSword* ULiveSpecialSwordComponent::FindSword(const FString& Id)const{return Swords.FindRef(Id);}

void ULiveSpecialSwordComponent::GetSwords(TArray<ALiveSpecialSword*>& Out)const
{
    Out.Reset();for(const auto& Pair:Swords)if(IsValid(Pair.Value))Out.Add(Pair.Value);
}

ELiveSpecialSword ULiveSpecialSwordComponent::GetKind(const FString& Id)const
{
    const ALiveSpecialSword* S=FindSword(Id);return S?S->GetKind():ELiveSpecialSword::None;
}

const FLiveSpecialSwordGrowth& ULiveSpecialSwordComponent::GetGrowth(const FString& Id)const
{
    static const FLiveSpecialSwordGrowth Empty;
    const FLiveSpecialSwordGrowth* G=Growth.Participants.Find(Id);return G?*G:Empty;
}

void ULiveSpecialSwordComponent::AddDevour(const FString& Id,const LiveSpecialSwords::FDevourGain& Gain)
{
    FLiveSpecialSwordGrowth& G=Growth.Participants.FindOrAdd(Id);
    const auto Add=[](double& Total,double Amount){if(FMath::IsFinite(Amount)&&Amount>0)Total=FMath::Min(1.e12,Total+Amount);};
    Add(G.DevouredAttack,Gain.Attack);Add(G.DevouredDefense,Gain.Defense);Add(G.DevouredHealth,Gain.Health);
    ++G.Devoured;bGrowthDirty=true;
}

void ULiveSpecialSwordComponent::ResetGrowth(const FString& Id){if(Growth.Participants.Remove(Id)>0)bGrowthDirty=true;}

double ULiveSpecialSwordComponent::AddMechaBonus(const FString& Id,double Amount)
{
    FLiveSpecialSwordGrowth& G=Growth.Participants.FindOrAdd(Id);
    if(FMath::IsFinite(Amount)&&Amount>0)G.MechaDamageBonus=FMath::Min(double(FMath::Max(0.f,MechaDamageBonusCap)),G.MechaDamageBonus+Amount);
    ++G.AxeSlams;bGrowthDirty=true;return G.MechaDamageBonus;
}

void ULiveSpecialSwordComponent::NotifyDamage(const FString& Id,float Applied){if(Applied>0)DamageDealt.FindOrAdd(Id)+=Applied;}

void ULiveSpecialSwordComponent::TickComponent(float DeltaTime,ELevelTick TickType,FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime,TickType,ThisTickFunction);
    ALiveWeaponSwarm* S=GetSwarm();
    if(!bEnabled||!S||S->GetPresentationMode()!=ELiveWeaponPresentationMode::Gameplay||!S->GetAnchor()){ReleaseAll();return;}
    if((ScanTimer-=DeltaTime)<=0.f){ScanTimer=.3f;Scan();}
    for(const auto& Pair:Swords)if(IsValid(Pair.Value))S->SetDelegatedPosition(Pair.Key,Pair.Value->GetActorLocation());
    ActiveSwords=Swords.Num();
    if(Swords.Num()&&!Overlay)
        if(APlayerController* PC=GetWorld()->GetFirstPlayerController();PC&&PC->IsLocalController())
        {
            Overlay=CreateWidget<ULiveSpecialSwordOverlay>(PC);
            if(Overlay){Overlay->Configure(this);Overlay->AddToPlayerScreen(5);}
        }
    if(bGrowthDirty&&(SaveTimer-=DeltaTime)<=0.f){SaveTimer=5.f;SaveGrowth();}
}

void ULiveSpecialSwordComponent::Scan()
{
    ALiveWeaponSwarm* S=GetSwarm();
    const UGameInstance* GI=GetWorld()?GetWorld()->GetGameInstance():nullptr;
    const UCultivationSubsystem* C=GI?GI->GetSubsystem<UCultivationSubsystem>():nullptr;
    if(!S||!C)return;
    TSet<FString> Seen;
    for(const FLiveWeaponState& State:S->GetRoster().GetStates())
    {
        if(!State.bParticipating||State.bPersonalSword)continue;
        const FString Id=State.Profile.ParticipantId;
        const FCultivationParticipantSnapshot Snapshot=C->GetParticipantSnapshot(S->GetProgressionOwnerId(Id));
        const ELiveSpecialSword Kind=LiveSpecialSwords::FromTitle(Snapshot.TitleId);
        if(Kind==ELiveSpecialSword::None)continue;
        Seen.Add(Id);
        ALiveSpecialSword* Existing=Swords.FindRef(Id);
        if(IsValid(Existing)&&Existing->GetKind()==Kind)continue;
        FVector Start=S->GetWeaponPosition(Id);
        if(IsValid(Existing)){Start=Existing->GetActorLocation();Release(Id,false);}
        if(Start.IsNearlyZero())Start=S->GetAnchor()->GetActorLocation()+FVector(0,0,150);
        FActorSpawnParameters Params;Params.Owner=S;Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        ALiveSpecialSword* Sword=GetWorld()->SpawnActor<ALiveSpecialSword>(ALiveSpecialSword::ClassFor(Kind),Start,FRotator::ZeroRotator,Params);
        if(!Sword)continue;
        Sword->InitializeSword(this,S,Id,State.Profile.DisplayName);
        S->SetParticipantDelegated(Id,true);
        Swords.Add(Id,Sword);
    }
    TArray<FString> Gone;
    for(const auto& Pair:Swords)if(!Seen.Contains(Pair.Key)||!IsValid(Pair.Value))Gone.Add(Pair.Key);
    for(const FString& Id:Gone)Release(Id,true);
}

void ULiveSpecialSwordComponent::Release(const FString& Id,bool bReturnToSwarm)
{
    ALiveSpecialSword* Sword=Swords.FindRef(Id);Swords.Remove(Id);
    if(ALiveWeaponSwarm* S=GetSwarm();S&&bReturnToSwarm)
    {
        if(IsValid(Sword))S->SetDelegatedPosition(Id,Sword->GetActorLocation());
        S->SetParticipantDelegated(Id,false);
    }
    if(auto* Buffs=ULiveAnchorBuffSubsystem::Get(this))Buffs->RemoveSource(Id);
    if(IsValid(Sword))Sword->Destroy();
}

void ULiveSpecialSwordComponent::ReleaseAll()
{
    TArray<FString> Ids;Swords.GetKeys(Ids);
    for(const FString& Id:Ids)Release(Id,true);
    ActiveSwords=0;
}

FString ULiveSpecialSwordComponent::GrowthPath()const{return FPaths::ProjectSavedDir()/TEXT("LiveSpecialSwords/growth.json");}

void ULiveSpecialSwordComponent::LoadGrowth()
{
    FString Json;
    if(FFileHelper::LoadFileToString(Json,*GrowthPath()))
    {
        FLiveSpecialSwordGrowthFile Loaded;
        if(FJsonObjectConverter::JsonObjectStringToUStruct(Json,&Loaded))Growth=MoveTemp(Loaded);
    }
}

void ULiveSpecialSwordComponent::SaveGrowth()
{
    FString Json;if(!FJsonObjectConverter::UStructToJsonObjectString(Growth,Json))return;
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(GrowthPath()),true);
    if(FFileHelper::SaveStringToFile(Json,*GrowthPath(),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))bGrowthDirty=false;
}

void ULiveSpecialSwordOverlay::Configure(ULiveSpecialSwordComponent* InOwner)
{
    Owner=InOwner;SetVisibility(ESlateVisibility::HitTestInvisible);SetIsFocusable(false);
    Font=LoadObject<UFont>(nullptr,TEXT("/Game/LiveWeapons/Fonts/F_LiveNames.F_LiveNames"),nullptr,LOAD_NoWarn|LOAD_Quiet);
}

namespace
{
    FString SpecialSwordDamageText(float Damage)
    {
        return Damage>=1e8f?FString::Printf(TEXT("%.2f亿"),Damage/1e8f):Damage>=1e7f?FString::Printf(TEXT("%.0f万"),Damage/1e4f)
            :Damage>=1e4f?FString::Printf(TEXT("%.1f万"),Damage/1e4f):FString::Printf(TEXT("%.0f"),Damage);
    }
}

int32 ULiveSpecialSwordOverlay::NativePaint(const FPaintArgs& Args,const FGeometry& Geometry,const FSlateRect& CullingRect,FSlateWindowElementList& Out,int32 Layer,const FWidgetStyle& Style,bool bParentEnabled)const
{
    const ULiveSpecialSwordComponent* O=Owner.Get();APlayerController* PC=GetOwningPlayer();
    if(!O||!PC)return Layer;
    const float Scale=FMath::Max(.01f,UWidgetLayoutLibrary::GetViewportScale(this));
    const FSlateBrush* White=FCoreStyle::Get().GetBrush("WhiteBrush");
    auto Box=[&](int32 L,FVector2D P,FVector2D S,FLinearColor C)
    {FSlateDrawElement::MakeBox(Out,L,Geometry.ToPaintGeometry(FVector2f(S),FSlateLayoutTransform(FVector2f(P))),White,ESlateDrawEffect::None,C);};
    FSlateFontInfo Info=FCoreStyle::GetDefaultFontStyle("Bold",13);if(Font)Info.FontObject=Font;
    Info.OutlineSettings.OutlineSize=2;Info.OutlineSettings.OutlineColor=FLinearColor(0,0,0,.85f);
    auto Text=[&](int32 L,const FString& Value,FVector2D Center,FLinearColor C,int32 Size)
    {
        FSlateFontInfo F=Info;F.Size=Size;
        const FVector2D Measure=FSlateApplication::IsInitialized()?FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(Value,F):FVector2D(Value.Len()*Size,Size*1.4f);
        FSlateDrawElement::MakeText(Out,L,Geometry.ToPaintGeometry(FVector2f(Measure),FSlateLayoutTransform(FVector2f(Center-Measure*.5))),Value,F,ESlateDrawEffect::None,C);
    };
    TArray<ALiveSpecialSword*> Swords;O->GetSwords(Swords);
    for(const ALiveSpecialSword* S:Swords)
    {
        const FLinearColor Tint=LiveSpecialSwords::Color(S->GetKind());
        FVector2D P;
        const float Fraction=S->GetMeterFraction();
        if(Fraction>=0.f&&PC->ProjectWorldLocationToScreen(S->GetOverheadLocation(),P,true))
        {
            P/=Scale;const FVector2D Size(96,9),Origin=P-FVector2D(Size.X*.5,0);
            Box(Layer+1,Origin-FVector2D(2,2),Size+FVector2D(4,4),FLinearColor(.02f,.02f,.03f,.85f));
            Box(Layer+2,Origin,FVector2D(Size.X*Fraction,Size.Y),FLinearColor(Tint.R,Tint.G,Tint.B,.95f));
            Box(Layer+3,Origin+FVector2D(Size.X*Fraction-1,0),FVector2D(2,Size.Y),FLinearColor(1,1,1,.9f));
            Text(Layer+4,S->GetMeterLabel().ToString(),P-FVector2D(0,12),FLinearColor(1.f,.95f,.8f),12);
        }
        for(const ALiveSpecialSword::FFloater& F:S->GetFloaters())
        {
            if(!PC->ProjectWorldLocationToScreen(F.Location+FVector(0,0,F.Age*70.f),P,true))continue;
            const float Alpha=FMath::Clamp(F.Age/.15f,0.f,1.f)*FMath::Clamp((1.8f-F.Age)/.5f,0.f,1.f);
            Text(Layer+5,F.Text,P/Scale,FLinearColor(F.Color.R,F.Color.G,F.Color.B,Alpha),15);
        }
        // Damage numbers pop in large, settle, rise while drifting aside, and fade over the last third: sword colour
        // for normal hits, a size up for skills, bright gold and a 暴击 tag for crits.
        const FLinearColor Light=FMath::Lerp(Tint,FLinearColor::White,.45f);
        for(const ALiveSpecialSword::FDamageNumber& N:S->GetDamageNumbers())
        {
            if(!PC->ProjectWorldLocationToScreen(N.Location,P,true))continue;
            const float T=FMath::Clamp(N.Age/ALiveSpecialSword::DamageNumberLife,0.f,1.f);
            const float Pop=N.Age<.1f?FMath::Lerp(1.8f,1.f,N.Age/.1f):1.f;
            const float Alpha=FMath::Clamp((1.f-T)/.35f,0.f,1.f);
            const FVector2D At=P/Scale+FVector2D(N.Drift*(24.f+26.f*T),-58.f*(1.f-FMath::Square(1.f-T)));
            const float Size=(N.bCritical?31.f:N.bSkill?25.f:22.f)*Pop;
            FSlateFontInfo NumberInfo=Info;NumberInfo.Size=FMath::Max(1,FMath::RoundToInt(Size));
            // The outline fades with the digits, or it would linger as a dark ghost.
            NumberInfo.OutlineSettings.OutlineSize=3;NumberInfo.OutlineSettings.OutlineColor=FLinearColor(.05f,.02f,.01f,.92f*Alpha);
            const FString Value=N.bCritical?TEXT("暴击 ")+SpecialSwordDamageText(N.Amount):SpecialSwordDamageText(N.Amount);
            const FVector2D Measure=FSlateApplication::IsInitialized()?FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(Value,NumberInfo):FVector2D(Value.Len()*Size,Size*1.4f);
            const FLinearColor Color=N.bCritical?FLinearColor(1.f,.86f,.35f,Alpha):FLinearColor(Light.R,Light.G,Light.B,Alpha);
            FSlateDrawElement::MakeText(Out,Layer+6,Geometry.ToPaintGeometry(FVector2f(Measure),FSlateLayoutTransform(FVector2f(At-Measure*.5))),Value,NumberInfo,ESlateDrawEffect::None,Color);
        }
    }
    return Layer+7;
}

#pragma once
#include "CoreMinimal.h"

class UNiagaraSystem;
class UNiagaraComponent;
class USceneComponent;
class UStaticMesh;
class UMaterialInterface;

/**
 * Sword-spirit effect library. Every system is a procedural Niagara asset authored by
 * Tools/SpecialSwords/special_sword_vfx.py with a shared user-parameter contract (User.Color plus a size).
 */
namespace LiveSwordVfx
{
    enum class EFx : uint8
    {
        Trail,   // sword-qi ribbons, mist and sparkles left by the hilt (loop): User.Color, User.Width, User.Life
        Slash,   // sweeping crescent, sword shade and sparks along +X: User.Color, User.Size
        Burst,   // star flare, shock rings, sparks and shards: User.Color, User.Radius
        Ring,    // ground formation, shockwave, light curtain and erupting swords: User.Color, User.Radius
        Charge,  // spinning formations, converging streaks and swords (loop): User.Color, User.Radius
        Qi,      // flying crescent with its wake (loop, local +X): User.Color, User.Length, User.Width
        Beam,    // forked bolt to User.Target (relative): User.Color, User.Width
        Shield,  // shell, ground formation and seven orbiting stars (loop): User.Color, User.Radius
        Aura,    // glow and light flowing along a blade on local Z (loop): User.Color, User.Radius = blade length
        Devour,  // violet serpent biting User.Source (relative), essence streaming back into the sword: User.Color, User.Radius
        Spin,    // level vortex of crescents and blade streaks (loop): User.Color, User.Radius
        Glyph,   // sci-fi seal, data bits and circuit traces: User.Color, User.Radius
        Shatter, // spinning crystal shards and glints: User.Color, User.Size
        Pillar,  // flowing light pillar over a formation and lotus: User.Color, User.Radius
        Burn,    // fire clinging to a struck enemy, base on the ground (loop): User.Color, User.Radius, User.Height
        Count
    };

    /** Art style: ink wash for the cultivator swords, futuristic for the mechanical Inorganic form. */
    enum class EStyle : uint8 { Ink, Tech };

    /** NS_SS_<Name>, or NS_SS_<Name>_Tech for the effects that have a futuristic twin. */
    BOUNDLESSIMMORTALITY_API FString SystemPath(EFx Fx,EStyle Style=EStyle::Ink);
    /** Falls back to the ink system when a futuristic twin is missing. */
    BOUNDLESSIMMORTALITY_API UNiagaraSystem* System(EFx Fx,EStyle Style=EStyle::Ink);
    /** Seven Vefects zap colours: red, blue, yellow, green, purple, white, black. */
    BOUNDLESSIMMORTALITY_API UNiagaraSystem* ZapSystem(int32 Index);
    BOUNDLESSIMMORTALITY_API FLinearColor ZapColor(int32 Index);
    BOUNDLESSIMMORTALITY_API void Preload(TArray<TObjectPtr<UObject>>& Keep);

    BOUNDLESSIMMORTALITY_API UNiagaraComponent* SpawnAt(UObject* WorldContext,EFx Fx,const FVector& Location,const FLinearColor& Color,float Size,const FRotator& Rotation=FRotator::ZeroRotator,EStyle Style=EStyle::Ink);
    BOUNDLESSIMMORTALITY_API UNiagaraComponent* SpawnAttached(USceneComponent* Parent,EFx Fx,const FLinearColor& Color,float Size,const FVector& Offset=FVector::ZeroVector,bool bAutoDestroy=false,EStyle Style=EStyle::Ink);
    BOUNDLESSIMMORTALITY_API void SetSize(UNiagaraComponent* Component,EFx Fx,float Size);
    BOUNDLESSIMMORTALITY_API void Beam(UObject* WorldContext,const FVector& From,const FVector& To,const FLinearColor& Color,float Width);
    BOUNDLESSIMMORTALITY_API void Devour(UObject* WorldContext,const FVector& Victim,const FVector& Sink,const FLinearColor& Color);
    /**
     * Timeline of NS_SS_Devour: the serpent leaves the sword, its jaws close on the victim at DevourBite seconds, it pulls
     * back while violet essence bursts from the bite and streams into the sword, absorbed at DevourAbsorb.
     */
    constexpr float DevourBite=.42f,DevourAbsorb=1.3f;
    /**
     * The devour following its sink (the sword) in world orientation. Callers keep User.Source (victim minus the component
     * location) current while it plays, so the serpent strikes where the victim is and the essence ends in the moving blade.
     */
    BOUNDLESSIMMORTALITY_API UNiagaraComponent* DevourAttached(USceneComponent* Sink,const FVector& Victim,const FLinearColor& Color);
    /**
     * 噬魂's violet fire on a struck enemy (loops until deactivated, then burns down): attached at Offset (its feet, in
     * Parent's space) and kept level; Radius is the body's radius and Height how tall the enemy stands.
     */
    BOUNDLESSIMMORTALITY_API UNiagaraComponent* BurnAttached(USceneComponent* Parent,const FVector& Offset,const FLinearColor& Color,float Radius,float Height);
    /** A Vefects zap at Location. Its floor scorch lies at the spawn point, so Location belongs on the ground. */
    BOUNDLESSIMMORTALITY_API void Zap(UObject* WorldContext,int32 Index,const FVector& Location,float Scale);
    /** The zap riding on Parent (Offset in its space, kept level), so its floor scorch stays under a moving target. */
    BOUNDLESSIMMORTALITY_API UNiagaraComponent* ZapAttached(USceneComponent* Parent,int32 Index,const FVector& Offset,float Scale);

    BOUNDLESSIMMORTALITY_API UStaticMesh* SwordMesh(uint8 Kind);
    BOUNDLESSIMMORTALITY_API UStaticMesh* GiantAxeMesh();
    /** Clone and axe material: translucent ink for the ink style, an additive hologram for the futuristic one. */
    BOUNDLESSIMMORTALITY_API UMaterialInterface* GhostMaterial(EStyle Style);
}

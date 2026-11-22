#pragma once
#include "CoreMinimal.h"

class APawn;

/**
 * Breakthrough effects on the anchor (Niagara systems NS_SS_BreakMinor / NS_SS_BreakMajor / NS_SS_BreakAscend authored by
 * Tools/SpecialSwords/special_sword_vfx.py). A minor step inside a major realm draws a qi spiral and a ripple; a new major
 * realm raises a light pillar over a formation; from 元婴 up (or under 问道灯 / 无极香 lantern light) the sky opens over
 * it as well. Size, colour and the layers that switch on grow with the major realm reached.
 */
namespace XianTuBreakthroughFx
{
    /** Hue of a major realm (0 练气 .. 10 混元无极), shared with the breakthrough screen. */
    BOUNDLESSIMMORTALITY_API FLinearColor RealmColor(int32 Major);
    BOUNDLESSIMMORTALITY_API void Play(APawn* Anchor,int32 FromRealm,int32 ToRealm,bool bCeremony);
}

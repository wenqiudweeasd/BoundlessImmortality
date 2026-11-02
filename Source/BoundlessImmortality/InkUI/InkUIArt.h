#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateBrush.h"

/**
 * Texture and font registry for the Slate UI. Brushes are cached by key and their resource
 * objects are kept alive by a GC referencer, so widgets can hold raw brush pointers.
 */
namespace InkUI::Art
{
    BOUNDLESSIMMORTALITY_API const UObject* DisplayFont();
    BOUNDLESSIMMORTALITY_API const UObject* BodyFont();

    /** Whole texture by package path, e.g. "/Game/UI/Ink/Textures/HUD/T_HomeAvatarFrame". */
    BOUNDLESSIMMORTALITY_API const FSlateBrush* Image(const TCHAR* Path);
    /** UV sub-rectangle in 0..1 texture space. */
    BOUNDLESSIMMORTALITY_API const FSlateBrush* Region(const TCHAR* Path,const FBox2f& UV);
    /** One cell of a uniform atlas grid; Inset trims bleeding pixels in UV units of a cell. */
    BOUNDLESSIMMORTALITY_API const FSlateBrush* Cell(const TCHAR* Path,int32 Columns,int32 Rows,int32 Index,float Inset=0.f);
    /** Nine-slice box; Margin is normalized to the texture. */
    BOUNDLESSIMMORTALITY_API const FSlateBrush* Box(const TCHAR* Path,const FMargin& Margin);
    /** Horizontal three-slice that keeps ornate end caps undistorted. */
    BOUNDLESSIMMORTALITY_API const FSlateBrush* HorizontalBox(const TCHAR* Path,float LeftRight);
    /** Texture-backed white brush; custom vertex geometry needs a real render resource. */
    BOUNDLESSIMMORTALITY_API const FSlateBrush* White();
    BOUNDLESSIMMORTALITY_API bool IsValid(const FSlateBrush* Brush);
    /** Pixel aspect (width/height) of the brush's sampled region; 1 when unknown. */
    BOUNDLESSIMMORTALITY_API float Aspect(const FSlateBrush* Brush);

    namespace Paths
    {
        inline const TCHAR* AvatarFrame=TEXT("/Game/UI/Ink/Textures/HUD/T_HomeAvatarFrame");
        inline const TCHAR* AvatarPortrait=TEXT("/Game/UI/Ink/Textures/HUD/T_HomeAvatarPortrait");
        inline const TCHAR* BarTrack=TEXT("/Game/UI/Ink/Textures/HUD/T_HomeBarTrack");
        inline const TCHAR* HealthFill=TEXT("/Game/UI/Ink/Textures/HUD/T_HomeHealthFill");
        inline const TCHAR* QiFill=TEXT("/Game/UI/Ink/Textures/HUD/T_HomeQiFill");
        inline const TCHAR* IdentityInk=TEXT("/Game/UI/Ink/Textures/HUD/T_HomeIdentityInk");
        inline const TCHAR* RealmPlaque=TEXT("/Game/UI/Ink/Textures/HUD/T_HomeRealmPlaque");
        inline const TCHAR* TopTile=TEXT("/Game/UI/Ink/Textures/HUD/T_HomeTopTile");
        inline const TCHAR* PreviewPaper=TEXT("/Game/UI/Ink/Textures/HUD/T_HomePreviewPaper");
        inline const TCHAR* BottomCloud=TEXT("/Game/UI/Ink/Textures/HUD/T_HomeBottomCloud");
        inline const TCHAR* Namecard=TEXT("/Game/UI/Ink/Textures/HUD/T_InkGoldNamecard");
        inline const TCHAR* NavigationAtlas=TEXT("/Game/UI/Ink/Textures/HUD/T_NavigationAtlas");
        inline const TCHAR* Meters=TEXT("/Game/UI/Ink/Textures/HUD/T_XianxiaMeters");
        inline const TCHAR* IconActivity=TEXT("/Game/UI/Ink/Textures/HUD/T_HomeIconActivity");
        inline const TCHAR* IconJourney=TEXT("/Game/UI/Ink/Textures/HUD/T_HomeIconJourney");
        inline const TCHAR* IconCultivate=TEXT("/Game/UI/Ink/Textures/HUD/T_HomeIconCultivate");
        inline const TCHAR* IconMenu=TEXT("/Game/UI/Ink/Textures/HUD/T_HomeIconMenu");
        inline const TCHAR* IconBag=TEXT("/Game/UI/Ink/Textures/Inventory/T_HomeIconBag");
        inline const TCHAR* IconRank=TEXT("/Game/UI/Ink/Textures/Leaderboard/T_HomeIconRank");
        inline const TCHAR* IconSignIn=TEXT("/Game/UI/Ink/Textures/Rewards/T_HomeIconSignIn");
        inline const TCHAR* RankPaper=TEXT("/Game/UI/Ink/Textures/Leaderboard/T_HomeRankPaper");
        inline const TCHAR* RankPortraitFrame=TEXT("/Game/UI/Ink/Textures/Leaderboard/T_InkGoldLeaderboardPortrait");
        inline const TCHAR* RankSwordHeader=TEXT("/Game/UI/Ink/Textures/Leaderboard/T_RankSwordHeader");
        inline const TCHAR* RankSpeedHeader=TEXT("/Game/UI/Ink/Textures/Leaderboard/T_RankSpeedHeader");
        inline const TCHAR* RankDamageHeader=TEXT("/Game/UI/Ink/Textures/Leaderboard/T_RankDamageHeader");
        inline const TCHAR* Chests=TEXT("/Game/UI/Ink/Textures/Rewards/T_ChestStates");
        inline const TCHAR* RareChest=TEXT("/Game/UI/Ink/Textures/Rewards/T_Gift_RareChest");
        inline const TCHAR* Title=TEXT("/Game/UI/Ink/Textures/Shared/T_InkTitle");
        inline const TCHAR* Backing=TEXT("/Game/UI/Ink/Textures/Shared/T_XianxiaBacking");
        inline const TCHAR* CohesionIcons=TEXT("/Game/UI/Ink/Textures/Shared/T_CohesionIcons");
        inline const TCHAR* FeatureScenes=TEXT("/Game/UI/Ink/Textures/Shared/T_InkFeatureScenes");
        inline const TCHAR* CohesionScenes=TEXT("/Game/UI/Ink/Textures/Shared/T_CohesionScenes");
        inline const TCHAR* AttributeIcons=TEXT("/Game/UI/Ink/Textures/Shared/T_InkAttributeIcons");
        inline const TCHAR* RealmBanners=TEXT("/Game/UI/Ink/Textures/Shared/T_RealmBanners");
        inline const TCHAR* CultivationLotus=TEXT("/Game/UI/Ink/Textures/Cultivation/T_CultivationPadded");
        inline const TCHAR* RatingS=TEXT("/Game/UI/Ink/Textures/Character/T_Rating_S");
        inline const TCHAR* RatingA=TEXT("/Game/UI/Ink/Textures/Character/T_Rating_A");
        inline const TCHAR* RatingB=TEXT("/Game/UI/Ink/Textures/Character/T_Rating_B");
        inline const TCHAR* GiftPotion=TEXT("/Game/UI/Ink/Textures/Gifts/T_Gift_LifePotion");
        inline const TCHAR* GiftWand=TEXT("/Game/UI/Ink/Textures/Gifts/T_Gift_Wand");
        inline const TCHAR* GiftAirdrop=TEXT("/Game/UI/Ink/Textures/Gifts/T_Gift_MysteryAirdrop");
        inline const TCHAR* GiftDessert=TEXT("/Game/UI/Ink/Textures/Gifts/T_Gift_StrawberryDessert");
        inline const TCHAR* WorldAtlas=TEXT("/Game/UI/Ink/Textures/Journey/T_WorldAtlas_v1");
        inline const TCHAR* ResourceAtlas=TEXT("/Game/UI/Ink/Textures/Journey/T_ResourceAtlas_v1");
        inline const TCHAR* PortraitPanel=TEXT("/Game/UI/Ink/Textures/Shared/T_PortraitPanel");
        inline const TCHAR* PolishControls=TEXT("/Game/UI/Ink/Textures/Shared/T_PolishControls");
        inline const TCHAR* PortraitControls=TEXT("/Game/UI/Ink/Textures/Shared/T_PortraitControls");
        inline const TCHAR* CohesionControls=TEXT("/Game/UI/Ink/Textures/Shared/T_CohesionControls");
        inline const TCHAR* PolishOrnaments=TEXT("/Game/UI/Ink/Textures/Shared/T_PolishOrnaments");
        inline const TCHAR* LeaderboardTitles=TEXT("/Game/UI/Ink/Textures/Leaderboard/T_LeaderboardTitles");
        inline const TCHAR* Meditation=TEXT("/Game/UI/Ink/Textures/Cultivation/T_InkMeditation");
    }

    /** Painted skins sliced in source pixels; fixed segments keep ornament aspect, stretch segments fill. */
    struct FSliceSegment
    {
        float A=0.f,B=0.f;
        bool bStretch=false;
    };

    struct FSkin
    {
        const TCHAR* Path=nullptr;
        /** Source sheet size the segment pixels were measured on; UVs use it instead of the imported surface size. */
        FVector2f Sheet=FVector2f::ZeroVector;
        TArray<FSliceSegment> Horizontal;
        TArray<FSliceSegment> Vertical;
        float SourceHeight() const;
        float SourceWidth() const;
    };

    enum class ESkin : uint8
    {
        Window,
        ButtonGold,ButtonPaper,ButtonBlue,ButtonDark,
        PlaqueLacquer,PlaqueCinnabar,PlaquePaper,PlaqueJade,
        CardPaper,RowScenic,BarDark,BarLight,
        PlaqueGold,PlaqueDarkGold,PlaquePaperScenic,PlaqueBlue,
        PanelFramed,CardOrnate,BarGold,BarDarkGold,BarBlue,
        HudBarTrack,TopTile,RealmPlaque,
        Count
    };
    BOUNDLESSIMMORTALITY_API const FSkin& Skin(ESkin Id);

    enum class EIconSet : uint8 { Items, Ornaments, Attributes, Navigation, Chests, FeatureScenes, CohesionScenes, Titles };
    BOUNDLESSIMMORTALITY_API const FSlateBrush* Icon(EIconSet Set,int32 Index);
    /** 34 realm plaques, column-major on a 2x17 sheet. */
    BOUNDLESSIMMORTALITY_API const FSlateBrush* RealmBanner(int32 RealmIndex);
    /** Item icon by semantic key ("crystal", "scroll", "pill"...); falls back to a jade orb. */
    BOUNDLESSIMMORTALITY_API const FSlateBrush* ItemIcon(FName Key);
    BOUNDLESSIMMORTALITY_API const FSlateBrush* AttributeIcon(FName AttributeId);
    /** Touches every asset once so the first page open does not hitch on synchronous loads. */
    BOUNDLESSIMMORTALITY_API void Preload();
}

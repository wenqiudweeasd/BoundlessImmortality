#pragma once

#include "CoreMinimal.h"
#include "InkUI/InkUITokens.h"

class UXianTuSubsystem;

/**
 * Presentation state for the Slate UI. Live game values (identity, vitals, attributes, local
 * leaderboards, saves) overwrite the seed; features that need the live-service SDK keep
 * clearly labelled local-verification data and never claim a server receipt.
 */
namespace InkUI
{
    struct FReward
    {
        FName Icon;
        FText Name;
        int32 Count=1;
        EQuality Quality=EQuality::Common;
    };

    struct FPlayerState
    {
        FText Name;
        FText Realm;
        int32 RealmIndex=INDEX_NONE;
        double Health=0,MaxHealth=0,Qi=0,MaxQi=0,QiPerSecond=0;
        bool bCultivating=false;
        bool bLive=false;
        FText ActionLabel;
    };

    struct FAttribute
    {
        FName Id;
        FText Label;
        FText Value;
    };

    struct FItem
    {
        FGuid Key;
        FName Id;
        FText Name;
        FText Description;
        /** Equipment / Material / Consumable / Treasure. */
        FName Category;
        FName Icon;
        EQuality Quality=EQuality::Common;
        int32 Count=1;
        bool bNew=false;
        bool bUsable=false;
        FName EquipSlot;
    };

    struct FCurrency
    {
        FName Id;
        FText Name;
        FName Icon;
        int64 Amount=0;
    };

    struct FRankRow
    {
        int32 Rank=0;
        FText Name;
        FText Realm;
        FText Metric;
        bool bSelf=false;
    };

    enum class EClaim : uint8 { Locked, Claimable, Claimed };

    struct FSignInDay
    {
        int32 Day=1;
        FReward Reward;
        EClaim State=EClaim::Locked;
    };

    struct FMilestone
    {
        int32 Days=0;
        FReward Reward;
        EClaim State=EClaim::Locked;
    };

    struct FActivity
    {
        FName Id;
        /** Daily / Timed / Launch. */
        FName Category;
        FText Title;
        FText Description;
        FText TimeText;
        int32 Scene=0;
        int32 Progress=0,Target=1;
        TArray<FReward> Rewards;
        EClaim State=EClaim::Locked;
        bool bFeatured=false;
    };

    struct FJourneyEntry
    {
        FName Id;
        FText Title;
        FText Description;
        int32 Scene=0;
        bool bNeedsService=false;
        FName Route;
    };

    struct FSaveSlot
    {
        int32 Index=0;
        bool bUsed=false;
        bool bValid=true;
        FText Title;
        FText Detail;
        FText Time;
    };

    /** Anchor buff or debuff as the HUD draws it: a calligraphy glyph in a coloured ring. */
    struct FBuffView
    {
        FName Id;
        FString Glyph;
        FText Name;
        FText Description;
        FLinearColor Color=FLinearColor::White;
        int32 Stacks=1;
        float Remaining=0,Duration=0;
        bool bDebuff=false;
    };

    struct FSpiritSkillView
    {
        FText Name;
        float Remaining=0,Cooldown=0;
        bool bPassive=false,bActive=false;
    };

    /** One audience sword spirit for the 剑灵谱: mesh drawn from the audience pool, optional special form. */
    struct FSpiritView
    {
        FString Id;
        FText Name;
        FText Realm;
        int32 RealmIndex=0;
        FString Mesh;
        /** ELiveSpecialSword as a byte; 0 for an ordinary spirit. */
        uint8 Special=0;
        FText Status;
        float Meter=-1;
        FText MeterLabel;
        double Attack=0,Damage=0;
        TArray<FSpiritSkillView> Skills;
    };

    struct FSettings
    {
        int32 Quality=1;
        int32 FrameRate=60;
        bool bReducedMotion=false;
    };

    class BOUNDLESSIMMORTALITY_API FModel
    {
    public:
        FPlayerState Player;
        TArray<FAttribute> Attributes;
        TArray<FItem> Items;
        int32 BagCapacity=40;
        TArray<FCurrency> Currencies;
        TMap<FName,TArray<FRankRow>> Boards;
        bool bLiveBoards=false;
        TArray<FSignInDay> SignIn;
        TArray<FMilestone> Milestones;
        int32 SignInTotal=0;
        TArray<FActivity> Activities;
        TArray<FJourneyEntry> Journey;
        TArray<FSaveSlot> Saves;
        bool bLiveSaves=false;
        FSettings Settings;
        TArray<FBuffView> Buffs;
        double Shield=0,ShieldMax=0;
        TArray<FSpiritView> Spirits;
        int32 AudienceMeshes=0;
        /** Local 仙途 ledger; null in the main menu before a game instance exists. */
        TWeakObjectPtr<UXianTuSubsystem> XianTu;
        /** Global revision wakes screens; section revisions decide what actually rebuilds. */
        int32 Revision=0;
        int32 ItemsRev=0,BoardsRev=0,SignInRev=0,ActivitiesRev=0,SavesRev=0,AttributesRev=0,BuffsRev=0,SpiritsRev=0,XianTuRev=0;
        FSimpleMulticastDelegate OnChanged;

        void SeedLocal();
        void Touch();

        bool ClaimSignIn(TArray<FReward>& OutRewards);
        bool ClaimMilestone(int32 Index,TArray<FReward>& OutRewards);
        bool ClaimActivity(FName Id,TArray<FReward>& OutRewards);
        void GrantRewards(const TArray<FReward>& Rewards);
        bool UseItem(const FGuid& Key,FText& OutMessage,TArray<FReward>& OutRewards);
        void SortItems();

        const FItem* FindItem(const FGuid& Key) const;
        const FCurrency* FindCurrency(FName Id) const;
        bool HasClaimableSignIn() const;
        int32 ClaimableActivities(FName Category=NAME_None) const;
        float QiFraction() const;
        float HealthFraction() const;
    };

    BOUNDLESSIMMORTALITY_API FText FormatAmount(double Value);
    BOUNDLESSIMMORTALITY_API FText CategoryLabel(FName Category);
}

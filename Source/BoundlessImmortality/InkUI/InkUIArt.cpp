#include "InkUI/InkUIArt.h"
#include "Engine/Font.h"
#include "Engine/Texture2D.h"
#include "Misc/PackageName.h"
#include "UObject/GCObject.h"

namespace InkUI::Art
{
    namespace
    {
        /** Lives for the process; a static FGCObject would unregister after the GC is gone. */
        class FRegistry final : public FGCObject
        {
        public:
            TMap<FString,TObjectPtr<UObject>> Assets;
            TMap<FString,TUniquePtr<FSlateBrush>> Brushes;

            virtual void AddReferencedObjects(FReferenceCollector& Collector) override
            {
                for(auto& Pair:Assets)if(Pair.Value)Collector.AddReferencedObject(Pair.Value);
            }
            virtual FString GetReferencerName() const override{return TEXT("InkUI::Art");}

            UObject* Load(const TCHAR* Path,UClass* Class)
            {
                const FString Key(Path);
                if(const auto* Found=Assets.Find(Key))return Found->Get();
                FString Full=Key;
                if(!Full.Contains(TEXT(".")))Full+=TEXT(".")+FPackageName::GetShortName(Full);
                UObject* Object=StaticLoadObject(Class,nullptr,*Full,nullptr,LOAD_NoWarn|LOAD_Quiet);
                Assets.Add(Key,Object);
                return Object;
            }

            const FSlateBrush* Make(const FString& Key,TFunctionRef<void(FSlateBrush&)> Setup)
            {
                if(const auto* Found=Brushes.Find(Key))return Found->Get();
                auto Brush=MakeUnique<FSlateBrush>();
                Setup(*Brush);
                const FSlateBrush* Result=Brush.Get();
                Brushes.Add(Key,MoveTemp(Brush));
                return Result;
            }
        };

        FRegistry& Registry()
        {
            static FRegistry* Instance=new FRegistry();
            return *Instance;
        }

        UTexture2D* Texture(const TCHAR* Path){return Cast<UTexture2D>(Registry().Load(Path,UTexture2D::StaticClass()));}

        void InitImage(FSlateBrush& Brush,UTexture2D* Source,const FBox2f& UV)
        {
            Brush.DrawAs=Source?ESlateBrushDrawType::Image:ESlateBrushDrawType::NoDrawType;
            if(!Source){Brush.ImageSize=FVector2D(32);return;}
            Brush.SetResourceObject(Source);
            FVector2D Full(FMath::Max(1.f,Source->GetSurfaceWidth()),FMath::Max(1.f,Source->GetSurfaceHeight()));
#if WITH_EDITORONLY_DATA
            // A texture still compiling in the editor reports a 32x32 placeholder surface, and the brush caches it; the imported source size is authoritative.
            if(Source->Source.GetSizeX()>0&&Source->Source.GetSizeY()>0)Full=FVector2D(double(Source->Source.GetSizeX()),double(Source->Source.GetSizeY()));
#endif
            const FVector2f Extent=UV.GetSize();
            Brush.ImageSize=FVector2D(Full.X*Extent.X,Full.Y*Extent.Y);
            if(UV.Min!=FVector2f::ZeroVector||UV.Max!=FVector2f(1,1))Brush.SetUVRegion(UV);
        }
    }

    const UObject* DisplayFont(){return Registry().Load(TEXT("/Game/UI/Ink/Fonts/Shared/F_InkDisplay"),UFont::StaticClass());}
    const UObject* BodyFont(){return Registry().Load(TEXT("/Game/UI/Ink/Fonts/Shared/F_InkBody"),UFont::StaticClass());}

    const FSlateBrush* Image(const TCHAR* Path)
    {
        return Registry().Make(FString(Path),[&](FSlateBrush& B){InitImage(B,Texture(Path),FBox2f(FVector2f(0,0),FVector2f(1,1)));});
    }

    const FSlateBrush* Region(const TCHAR* Path,const FBox2f& UV)
    {
        const FString Key=FString::Printf(TEXT("%s@%.4f,%.4f,%.4f,%.4f"),Path,UV.Min.X,UV.Min.Y,UV.Max.X,UV.Max.Y);
        return Registry().Make(Key,[&](FSlateBrush& B){InitImage(B,Texture(Path),UV);});
    }

    const FSlateBrush* Cell(const TCHAR* Path,int32 Columns,int32 Rows,int32 Index,float Inset)
    {
        Columns=FMath::Max(1,Columns);Rows=FMath::Max(1,Rows);Index=FMath::Clamp(Index,0,Columns*Rows-1);
        const float W=1.f/Columns,H=1.f/Rows,X=(Index%Columns)*W,Y=(Index/Columns)*H;
        return Region(Path,FBox2f(FVector2f(X+Inset*W,Y+Inset*H),FVector2f(X+W-Inset*W,Y+H-Inset*H)));
    }

    const FSlateBrush* Box(const TCHAR* Path,const FMargin& Margin)
    {
        const FString Key=FString::Printf(TEXT("%s#box%.3f,%.3f,%.3f,%.3f"),Path,Margin.Left,Margin.Top,Margin.Right,Margin.Bottom);
        return Registry().Make(Key,[&](FSlateBrush& B)
        {
            InitImage(B,Texture(Path),FBox2f(FVector2f(0,0),FVector2f(1,1)));
            if(B.GetResourceObject()){B.DrawAs=ESlateBrushDrawType::Box;B.Margin=Margin;}
        });
    }

    const FSlateBrush* HorizontalBox(const TCHAR* Path,float LeftRight){return Box(Path,FMargin(LeftRight,0,LeftRight,0));}

    const FSlateBrush* White()
    {
        return Registry().Make(TEXT("#white"),[](FSlateBrush& B)
        {
            InitImage(B,Texture(TEXT("/Engine/EngineResources/WhiteSquareTexture")),FBox2f(FVector2f(0,0),FVector2f(1,1)));
        });
    }

    bool IsValid(const FSlateBrush* Brush){return Brush&&Brush->GetResourceObject()!=nullptr;}

    float Aspect(const FSlateBrush* Brush)
    {
        return Brush&&Brush->ImageSize.Y>0?float(Brush->ImageSize.X/Brush->ImageSize.Y):1.f;
    }

    float FSkin::SourceHeight() const{float H=0;for(const auto& S:Vertical)H+=S.B-S.A;return H;}
    float FSkin::SourceWidth() const{float W=0;for(const auto& S:Horizontal)W+=S.B-S.A;return W;}

    namespace
    {
        FVector2f SourceSheet(const TCHAR* Path)
        {
            using namespace Paths;
            if(Path==PortraitPanel)return FVector2f(1024.f,1536.f);
            if(Path==PolishControls||Path==PortraitControls||Path==CohesionControls)return FVector2f(1536.f,1024.f);
            if(Path==BarTrack)return FVector2f(2046.f,226.f);
            if(Path==TopTile)return FVector2f(889.f,1097.f);
            if(Path==RealmPlaque)return FVector2f(1988.f,474.f);
            return FVector2f::ZeroVector;
        }

        FSkin MakeSkin(const TCHAR* Path,std::initializer_list<FSliceSegment> H,std::initializer_list<FSliceSegment> V)
        {
            FSkin S;S.Path=Path;S.Sheet=SourceSheet(Path);S.Horizontal=H;S.Vertical=V;return S;
        }
        /** Fixed caps on both ends with one stretched middle; the height scales uniformly. */
        FSkin ThreeSlice(const TCHAR* Path,float X0,float X1,float Cap,float Y0,float Y1)
        {
            return MakeSkin(Path,{{X0,X0+Cap,false},{X0+Cap,X1-Cap,true},{X1-Cap,X1,false}},{{Y0,Y1,true}});
        }
        FSkin NineSlice(const TCHAR* Path,float X0,float X1,float CapX,float Y0,float Y1,float CapY)
        {
            return MakeSkin(Path,{{X0,X0+CapX,false},{X0+CapX,X1-CapX,true},{X1-CapX,X1,false}},{{Y0,Y0+CapY,false},{Y0+CapY,Y1-CapY,true},{Y1-CapY,Y1,false}});
        }
    }

    const FSkin& Skin(ESkin Id)
    {
        static const TArray<FSkin> Skins=[]
        {
            using namespace Paths;
            TArray<FSkin> S;S.SetNum(int32(ESkin::Count));
            // Crane-and-mountain scroll. The mid-edge diamonds and top-centre stud stay unstretched.
            S[int32(ESkin::Window)]=MakeSkin(PortraitPanel,
                {{30,300,false},{300,440,true},{440,584,false},{584,724,true},{724,992,false}},
                {{44,300,false},{300,700,true},{700,800,false},{800,1170,true},{1170,1476,false}});
            S[int32(ESkin::ButtonGold)]=ThreeSlice(PolishControls,784,1504,118,94,236);
            S[int32(ESkin::ButtonPaper)]=ThreeSlice(PolishControls,32,752,118,94,234);
            S[int32(ESkin::ButtonBlue)]=ThreeSlice(PolishControls,32,752,118,332,476);
            S[int32(ESkin::ButtonDark)]=ThreeSlice(PolishControls,786,1502,118,330,478);
            S[int32(ESkin::PlaqueLacquer)]=ThreeSlice(CohesionControls,810,1484,72,102,240);
            S[int32(ESkin::PlaqueCinnabar)]=ThreeSlice(CohesionControls,810,1484,72,314,450);
            S[int32(ESkin::PlaquePaper)]=ThreeSlice(CohesionControls,54,724,72,100,240);
            S[int32(ESkin::PlaqueJade)]=ThreeSlice(CohesionControls,54,724,72,314,450);
            S[int32(ESkin::CardPaper)]=NineSlice(CohesionControls,54,724,72,518,777,74);
            S[int32(ESkin::RowScenic)]=MakeSkin(CohesionControls,{{810,900,false},{900,1240,true},{1240,1484,false}},{{580,722,true}});
            S[int32(ESkin::BarDark)]=ThreeSlice(CohesionControls,62,720,40,853,900);
            S[int32(ESkin::BarLight)]=ThreeSlice(CohesionControls,816,1474,36,854,900);
            S[int32(ESkin::PlaqueGold)]=ThreeSlice(PortraitControls,792,1484,92,88,250);
            S[int32(ESkin::PlaqueDarkGold)]=ThreeSlice(PortraitControls,792,1484,92,330,490);
            S[int32(ESkin::PlaquePaperScenic)]=ThreeSlice(PortraitControls,54,744,96,88,248);
            S[int32(ESkin::PlaqueBlue)]=ThreeSlice(PortraitControls,54,744,96,330,492);
            S[int32(ESkin::PanelFramed)]=NineSlice(PortraitControls,54,740,64,548,806,64);
            S[int32(ESkin::CardOrnate)]=NineSlice(PolishControls,40,746,76,550,734,64);
            S[int32(ESkin::BarGold)]=ThreeSlice(PortraitControls,794,1482,60,881,937);
            S[int32(ESkin::BarDarkGold)]=ThreeSlice(PortraitControls,54,742,60,881,937);
            S[int32(ESkin::BarBlue)]=ThreeSlice(PolishControls,40,746,72,836,908);
            // Track channel is (168,54)-(1880,174); SInkProgressBar insets the fill by the same ratio.
            S[int32(ESkin::HudBarTrack)]=ThreeSlice(BarTrack,0,2046,260,0,226);
            S[int32(ESkin::TopTile)]=NineSlice(TopTile,10,876,120,14,1086,150);
            S[int32(ESkin::RealmPlaque)]=ThreeSlice(RealmPlaque,12,1976,300,10,462);
            return S;
        }();
        return Skins[FMath::Clamp(int32(Id),0,Skins.Num()-1)];
    }

    const FSlateBrush* Icon(EIconSet Set,int32 Index)
    {
        using namespace Paths;
        switch(Set)
        {
        case EIconSet::Items:return Cell(CohesionIcons,4,4,Index,.02f);
        case EIconSet::Ornaments:return Cell(PolishOrnaments,4,4,Index,.02f);
        case EIconSet::Attributes:return Cell(AttributeIcons,4,3,Index,.03f);
        case EIconSet::Navigation:return Cell(NavigationAtlas,4,2,Index,.02f);
        case EIconSet::Chests:return Cell(Chests,2,1,Index,.02f);
        case EIconSet::FeatureScenes:return Cell(FeatureScenes,2,4,Index,.01f);
        case EIconSet::CohesionScenes:return Cell(CohesionScenes,2,4,Index,.01f);
        case EIconSet::Titles:return Cell(LeaderboardTitles,2,8,Index,.01f);
        default:return Image(CohesionIcons);
        }
    }

    const FSlateBrush* RealmBanner(int32 RealmIndex)
    {
        RealmIndex=FMath::Clamp(RealmIndex,0,33);
        const int32 Column=RealmIndex/17,Row=RealmIndex%17;
        return Cell(Paths::RealmBanners,2,17,Row*2+Column,.0f);
    }

    const FSlateBrush* ItemIcon(FName Key)
    {
        struct FEntry{const TCHAR* Key;EIconSet Set;int32 Index;};
        static const FEntry Table[]={
            {TEXT("crystal"),EIconSet::Items,8},{TEXT("scroll"),EIconSet::Items,9},{TEXT("pill"),EIconSet::Items,10},
            {TEXT("coin"),EIconSet::Items,11},{TEXT("chest_bronze"),EIconSet::Items,12},{TEXT("chest_jade"),EIconSet::Items,13},
            {TEXT("chest_gold"),EIconSet::Items,14},{TEXT("orb_gold"),EIconSet::Items,15},{TEXT("lotus"),EIconSet::Items,5},
            {TEXT("sword_emblem"),EIconSet::Items,6},{TEXT("vortex"),EIconSet::Items,4},{TEXT("cloud_orb"),EIconSet::Items,7},
            {TEXT("ring_jade"),EIconSet::Items,1},{TEXT("ring_gold"),EIconSet::Items,2},
            {TEXT("seal_scroll"),EIconSet::Ornaments,2},{TEXT("lotus_orb"),EIconSet::Ornaments,3},{TEXT("heart"),EIconSet::Ornaments,4},
            {TEXT("shield"),EIconSet::Ornaments,5},{TEXT("water_orb"),EIconSet::Ornaments,6},{TEXT("hourglass"),EIconSet::Ornaments,7},
            {TEXT("crystal_gold"),EIconSet::Ornaments,8},{TEXT("jade_orb"),EIconSet::Ornaments,9},{TEXT("treasure"),EIconSet::Ornaments,10},
            {TEXT("golden_scroll"),EIconSet::Ornaments,11},{TEXT("swords"),EIconSet::Ornaments,12},{TEXT("armor"),EIconSet::Ornaments,13},
            {TEXT("star_compass"),EIconSet::Ornaments,14},{TEXT("gold_cloud"),EIconSet::Ornaments,15},
        };
        for(const auto& E:Table)if(Key==FName(E.Key))return Icon(E.Set,E.Index);
        return Icon(EIconSet::Ornaments,9);
    }

    const FSlateBrush* AttributeIcon(FName AttributeId)
    {
        static const FName Order[]={TEXT("Health"),TEXT("Attack"),TEXT("Defense"),TEXT("SpiritDamage"),TEXT("Critical"),TEXT("Evasion"),TEXT("Luck"),TEXT("MoveSpeed"),TEXT("Swordsmanship"),TEXT("CultivationSpeed"),TEXT("Realm")};
        for(int32 I=0;I<UE_ARRAY_COUNT(Order);++I)if(Order[I]==AttributeId)return Icon(EIconSet::Attributes,I);
        if(AttributeId==TEXT("Cultivation"))return Icon(EIconSet::Attributes,9);
        return Icon(EIconSet::Attributes,10);
    }

    void Preload()
    {
        using namespace Paths;
        const TCHAR* All[]={AvatarFrame,AvatarPortrait,BarTrack,HealthFill,QiFill,IdentityInk,RealmPlaque,TopTile,BottomCloud,NavigationAtlas,
            IconActivity,IconJourney,IconCultivate,IconMenu,IconBag,IconRank,IconSignIn,RankPaper,RankSwordHeader,RankSpeedHeader,RankDamageHeader,
            Chests,Title,CohesionIcons,FeatureScenes,CohesionScenes,AttributeIcons,RealmBanners,CultivationLotus,RatingS,RatingA,RatingB,
            PortraitPanel,PolishControls,PortraitControls,CohesionControls,PolishOrnaments,LeaderboardTitles,Meditation};
        for(const TCHAR* Path:All)Image(Path);
        White();DisplayFont();BodyFont();
    }
}

#include "LiveWeapons/SpecialSwords/LiveSpecialSwordTypes.h"

#define LOCTEXT_NAMESPACE "LiveSpecialSwords"

namespace LiveSpecialSwords
{
    ELiveSpecialSword FromTitle(FName T)
    {
        if(T==TEXT("SoulEater"))return ELiveSpecialSword::SoulEater;
        if(T==TEXT("Cyberspace"))return ELiveSpecialSword::Inorganic;
        if(T==TEXT("DivineBlade"))return ELiveSpecialSword::DivineBlade;
        if(T==TEXT("SevenStar"))return ELiveSpecialSword::SevenStar;
        if(T==TEXT("Godslayer"))return ELiveSpecialSword::Godslayer;
        return ELiveSpecialSword::None;
    }
    FName TitleOf(ELiveSpecialSword K)
    {
        switch(K)
        {
        case ELiveSpecialSword::SoulEater:return TEXT("SoulEater");
        case ELiveSpecialSword::Inorganic:return TEXT("Cyberspace");
        case ELiveSpecialSword::DivineBlade:return TEXT("DivineBlade");
        case ELiveSpecialSword::SevenStar:return TEXT("SevenStar");
        case ELiveSpecialSword::Godslayer:return TEXT("Godslayer");
        default:return NAME_None;
        }
    }
    ECultivationGift GiftOf(ELiveSpecialSword K)
    {
        switch(K)
        {
        case ELiveSpecialSword::SoulEater:return ECultivationGift::RareChest;
        case ELiveSpecialSword::Inorganic:return ECultivationGift::SuperJet;
        case ELiveSpecialSword::DivineBlade:return ECultivationGift::MysteryAirdrop;
        case ELiveSpecialSword::SevenStar:return ECultivationGift::Strawberry;
        default:return ECultivationGift::SuperAirdrop;
        }
    }
    FString GiftName(ELiveSpecialSword K)
    {
        switch(K)
        {
        case ELiveSpecialSword::SoulEater:return TEXT("稀有宝箱");
        case ELiveSpecialSword::Inorganic:return TEXT("超能喷射");
        case ELiveSpecialSword::DivineBlade:return TEXT("神秘空投");
        case ELiveSpecialSword::SevenStar:return TEXT("草莓甜点");
        case ELiveSpecialSword::Godslayer:return TEXT("超级空投");
        default:return FString();
        }
    }
    FText Name(ELiveSpecialSword K)
    {
        switch(K)
        {
        case ELiveSpecialSword::SoulEater:return LOCTEXT("SoulEater","噬魂剑");
        case ELiveSpecialSword::Inorganic:return LOCTEXT("Inorganic","无机态那");
        case ELiveSpecialSword::DivineBlade:return LOCTEXT("DivineBlade","驱煞神刃");
        case ELiveSpecialSword::SevenStar:return LOCTEXT("SevenStar","七星光芒剑");
        case ELiveSpecialSword::Godslayer:return LOCTEXT("Godslayer","弑神剑");
        default:return LOCTEXT("Plain","剑灵");
        }
    }
    FText Motto(ELiveSpecialSword K)
    {
        switch(K)
        {
        case ELiveSpecialSword::SoulEater:return LOCTEXT("SoulEaterMotto","吞魂炼魄，分影万千");
        case ELiveSpecialSword::Inorganic:return LOCTEXT("InorganicMotto","机魂觉醒，化斧开天");
        case ELiveSpecialSword::DivineBlade:return LOCTEXT("DivineBladeMotto","驱邪护主，双影归一");
        case ELiveSpecialSword::SevenStar:return LOCTEXT("SevenStarMotto","七星垂光，护道不灭");
        case ELiveSpecialSword::Godslayer:return LOCTEXT("GodslayerMotto","破甲诛神，一斩再斩");
        default:return FText::GetEmpty();
        }
    }
    FLinearColor Color(ELiveSpecialSword K)
    {
        switch(K)
        {
        case ELiveSpecialSword::SoulEater:return FLinearColor(.62f,.12f,1.f,1.f);
        case ELiveSpecialSword::Inorganic:return FLinearColor(.08f,.85f,1.f,1.f);
        case ELiveSpecialSword::DivineBlade:return FLinearColor(1.f,.72f,.18f,1.f);
        case ELiveSpecialSword::SevenStar:return FLinearColor(1.f,.93f,.55f,1.f);
        case ELiveSpecialSword::Godslayer:return FLinearColor(1.f,.12f,.08f,1.f);
        default:return FLinearColor(.75f,.9f,1.f,1.f);
        }
    }
    const TArray<FSkillInfo>& Skills(ELiveSpecialSword K)
    {
        auto Make=[](const TCHAR* Id,FText Name,FText Description,float Cooldown,bool bPassive){FSkillInfo S;S.Id=Id;S.Name=Name;S.Description=Description;S.Cooldown=Cooldown;S.bPassive=bPassive;return S;};
        static const TArray<FSkillInfo> SoulEater={
            Make(TEXT("Devour"),LOCTEXT("Devour","噬魂"),LOCTEXT("DevourDesc","被动：对生命低于15%的敌人造成伤害时有30%概率直接吞噬——紫色蛇首噬敌，散出紫色精元由噬魂剑吸取。每次吞噬永久获得敌人1%的属性（攻击、防御、生命），并将敌人生命值的5%转化为攻击力。"),0,true),
            Make(TEXT("Clones"),LOCTEXT("Clones","万影分身"),LOCTEXT("ClonesDesc","分出 攻击力/(基础攻击×0.5) 把分身（最少5，最多20），各自追击目标，命中即碎；分身继承50%攻击力。分身未散尽前本体连续追击、不回主播身边。冷却25秒。"),25,false),
            Make(TEXT("GroundWave"),LOCTEXT("GroundWave","插地蓄力"),LOCTEXT("GroundWaveDesc","插入地面蓄力2秒后拔出，向八方发射贯穿的地波剑气。冷却5秒。"),5,false)};
        static const TArray<FSkillInfo> Inorganic={
            Make(TEXT("MechaSoul"),LOCTEXT("MechaSoul","机魂"),LOCTEXT("MechaSoulDesc","被动：每次攻击获得1层机魂，每层提升主播5%防御；叠满5层化作巨斧砸地，范围伤害并击退，消耗全部层数，每层令本剑灵伤害永久+5%。"),0,true),
            Make(TEXT("Spin"),LOCTEXT("Spin","回旋疾斩"),LOCTEXT("SpinDesc","疯狂旋转，如回旋镖般往复切割周围敌人，伤害频率提升2倍，持续5秒。冷却15秒。"),15,false),
            Make(TEXT("Barrage"),LOCTEXT("Barrage","剑气风暴"),LOCTEXT("BarrageDesc","约每0.2秒释放一道贯穿剑气（未来科技风格），持续5秒。冷却12秒。"),12,false)};
        static const TArray<FSkillInfo> DivineBlade={
            Make(TEXT("Dispel"),LOCTEXT("Dispel","驱煞"),LOCTEXT("DispelDesc","被动：每10秒驱散主播身上一个减益。"),10,true),
            Make(TEXT("Bless"),LOCTEXT("Bless","回元"),LOCTEXT("BlessDesc","被动：每10秒为主播恢复当前生命的25%。"),10,true),
            Make(TEXT("Twin"),LOCTEXT("Twin","双影归一"),LOCTEXT("TwinDesc","释放2个继承被动与100%攻击力的分身，分身伤害蓄入本体头顶进度（上限为攻击力1200%）；蓄满后分身合体，蓄力1秒释放毁天灭地一击，对大范围敌人造成128%蓄积值伤害。冷却8秒。"),8,false)};
        static const TArray<FSkillInfo> SevenStar={
            Make(TEXT("Lightning"),LOCTEXT("Lightning","七星引雷"),LOCTEXT("LightningDesc","向四周7个目标发射七色闪电，每道造成攻击力80%＋目标当前生命5%的伤害（生命部分为真实伤害）；目标不足7个时多余闪电随机落在已选目标上。冷却45秒。"),45,false),
            Make(TEXT("Guardian"),LOCTEXT("Guardian","七星护道"),LOCTEXT("GuardianDesc","被动：主播生命每损失七分之一时守护主播，期间不攻击；主播头像生成光芒护盾（吸收七星剑攻击力70%的伤害），并获得每秒恢复当前生命1%的增益，持续15秒。施放技能时不触发。七星挡劫：渡劫失败的天罚中，剑灵境界高出所渡天劫一个大境界才有50%概率挡下（再高一个大境界，被破的概率减半），挡下则天雷不致陨落；挡不住则光盾破碎，护盾与草莓甜点的星辉治疗对天雷无效。"),0,true),
            Make(TEXT("Radiance"),LOCTEXT("Radiance","光芒剑气"),LOCTEXT("RadianceDesc","蓄力5秒，释放巨大剑气，击退敌人并造成攻击力488%的伤害；45%概率引来3次雷击，落在剑气贯穿的敌人身上，每次造成攻击力80%＋目标当前生命5%的伤害。冷却15秒。"),15,false)};
        static const TArray<FSkillInfo> Godslayer={
            Make(TEXT("Pierce"),LOCTEXT("Pierce","诛神破甲"),LOCTEXT("PierceDesc","被动：伤害无视50%防御；目标生命低于50%时提升至75%。"),0,true),
            Make(TEXT("Truth"),LOCTEXT("Truth","真意"),LOCTEXT("TruthDesc","被动：造成伤害时附加该伤害5%的真实伤害。"),0,true),
            Make(TEXT("Again"),LOCTEXT("Again","再斩"),LOCTEXT("AgainDesc","被动：每次造成伤害有50%概率追加一次伤害；目标生命高于50%时概率提升至75%（追加伤害不再连锁追加）。"),0,true)};
        static const TArray<FSkillInfo> None;
        switch(K)
        {
        case ELiveSpecialSword::SoulEater:return SoulEater;
        case ELiveSpecialSword::Inorganic:return Inorganic;
        case ELiveSpecialSword::DivineBlade:return DivineBlade;
        case ELiveSpecialSword::SevenStar:return SevenStar;
        case ELiveSpecialSword::Godslayer:return Godslayer;
        default:return None;
        }
    }
    const FLiveSwordSpiritStats& Stats(ELiveSpecialSword K)
    {
        auto Make=[](float Attack,float Growth,float Crit,float CritGrowth,float CritDamage,float CritDamageGrowth,float Pen,float PenGrowth,float Interval)
        {
            FLiveSwordSpiritStats S;S.Attack=Attack;S.AttackGrowth=Growth;S.CriticalChance=Crit;S.CriticalGrowth=CritGrowth;
            S.CriticalDamage=CritDamage;S.CriticalDamageGrowth=CritDamageGrowth;S.ArmorPenetration=Pen;S.PenetrationGrowth=PenGrowth;S.AttackInterval=Interval;return S;
        };
        // Ranked by how much the form's skills already grow it: 噬魂 devours attack for good, so its sheet is the lowest;
        // 无机态那 tempers damage permanently; 驱煞 and 七星 carry support and burst; 弑神 has passives only, so its sheet leads.
        //                                          攻击  成长    会心   会心成长 会心伤害 伤害成长 破甲   破甲成长 间隔
        static const FLiveSwordSpiritStats Normal=Make(10.f,.12f,  0.f,  0.f,     2.0f,   0.f,    0.f,  0.f,     1.00f);
        static const FLiveSwordSpiritStats SoulEater=Make(12.f,.13f,.03f,.001f,   2.0f,   0.f,    0.f,  .002f,   1.15f);
        static const FLiveSwordSpiritStats Inorganic=Make(13.f,.14f,.04f,.0015f,  2.0f,   .01f,   .05f, .003f,   1.20f);
        static const FLiveSwordSpiritStats DivineBlade=Make(14.f,.15f,.05f,.0015f,2.1f,   .01f,   .05f, .003f,   1.10f);
        static const FLiveSwordSpiritStats SevenStar=Make(15.f,.16f,.06f,.002f,   2.2f,   .015f,  .08f, .004f,   1.05f);
        static const FLiveSwordSpiritStats Godslayer=Make(18.f,.18f,.08f,.0025f,  2.5f,   .02f,   .12f, .005f,   1.00f);
        switch(K)
        {
        case ELiveSpecialSword::SoulEater:return SoulEater;
        case ELiveSpecialSword::Inorganic:return Inorganic;
        case ELiveSpecialSword::DivineBlade:return DivineBlade;
        case ELiveSpecialSword::SevenStar:return SevenStar;
        case ELiveSpecialSword::Godslayer:return Godslayer;
        default:return Normal;
        }
    }
    namespace{int32 RealmOf(int32 Realm){return FMath::Clamp(Realm,0,33);}}
    float AttackAt(ELiveSpecialSword K,int32 Realm){const auto& S=Stats(K);return S.Attack*(1+S.AttackGrowth*RealmOf(Realm));}
    float CriticalAt(ELiveSpecialSword K,int32 Realm){const auto& S=Stats(K);return FMath::Clamp(S.CriticalChance+S.CriticalGrowth*RealmOf(Realm),0.f,1.f);}
    float CriticalDamageAt(ELiveSpecialSword K,int32 Realm){const auto& S=Stats(K);return FMath::Max(1.f,S.CriticalDamage+S.CriticalDamageGrowth*RealmOf(Realm));}
    float PenetrationAt(ELiveSpecialSword K,int32 Realm){const auto& S=Stats(K);return FMath::Clamp(S.ArmorPenetration+S.PenetrationGrowth*RealmOf(Realm),0.f,.6f);}
    float AttackScale(ELiveSpecialSword K,int32 Realm){return AttackAt(K,Realm)/FMath::Max(1.e-3f,AttackAt(ELiveSpecialSword::None,Realm));}
    TArray<ELiveSpecialSword> All(){return {ELiveSpecialSword::SoulEater,ELiveSpecialSword::Inorganic,ELiveSpecialSword::DivineBlade,ELiveSpecialSword::SevenStar,ELiveSpecialSword::Godslayer};}
    FDevourGain DevourGain(double EnemyAttack,double EnemyDefense,double EnemyMaxHealth)
    {
        const auto Clean=[](double V){return FMath::IsFinite(V)?FMath::Clamp(V,0.0,1.e15):0.0;};
        FDevourGain G;G.Defense=Clean(EnemyDefense)*DevourShare;G.Health=Clean(EnemyMaxHealth)*DevourShare;
        G.HealthAttack=Clean(EnemyMaxHealth)*DevourHealthToAttack;G.Attack=Clean(EnemyAttack)*DevourShare+G.HealthAttack;
        return G;
    }
}

#undef LOCTEXT_NAMESPACE

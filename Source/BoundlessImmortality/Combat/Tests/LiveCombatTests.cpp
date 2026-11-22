#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Combat/LiveCombat.h"
#include "LiveWeapons/SpecialSwords/LiveSpecialSwordTypes.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLiveCombatFormulaTest,"BoundlessImmortality.Combat.DamageFormula",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FLiveCombatFormulaTest::RunTest(const FString&)
{
    auto Damage=[](float Attack,float Defense,TFunction<void(FLiveHit&)> Edit=nullptr,int32 DefenderRealm=-1,float Reduction=0,bool bCritical=false)
    {FLiveHit Hit;Hit.Attack=Attack;if(Edit)Edit(Hit);return ULiveCombatLibrary::ResolveHit(Hit,Defense,DefenderRealm,Reduction,bCritical).Damage;};
    // Design check: a 10-attack sword against the world trial's 10 defense deals 5 (360 health / 4 DPS ≈ 90 s solo).
    TestEqual(TEXT("A²/(A+D) at A=D"),Damage(10,10),5.f,1.e-4f);
    TestEqual(TEXT("no defense keeps A"),Damage(10,0),10.f,1.e-4f);
    TestEqual(TEXT("defense removes at most 60%"),Damage(10,100),4.f,1.e-4f);
    TestEqual(TEXT("破甲 caps at 60%"),Damage(10,20,[](FLiveHit& H){H.ArmorPenetration=.9f;}),10.f*10.f/18.f,1.e-3f);
    TestEqual(TEXT("无视防御 applies before 破甲"),Damage(10,40,[](FLiveHit& H){H.DefenseIgnore=.5f;H.ArmorPenetration=.6f;}),10.f*10.f/18.f,1.e-3f);
    TestEqual(TEXT("招式系数 scales after the defense share"),Damage(10,10,[](FLiveHit& H){H.SkillCoefficient=3.5f;}),17.5f,1.e-3f);
    TestEqual(TEXT("增伤 is additive"),Damage(10,0,[](FLiveHit& H){H.DamageBonus=.25f;}),12.5f,1.e-3f);
    TestEqual(TEXT("会心伤害 multiplies a critical"),Damage(10,0,[](FLiveHit& H){H.CriticalDamage=2.5f;},-1,0,true),25.f,1.e-3f);
    TestEqual(TEXT("true damage ratio adds on the formula"),Damage(10,10,[](FLiveHit& H){H.TrueDamageRatio=.05f;}),5.25f,1.e-3f);
    TestEqual(TEXT("flat true damage skips defense"),Damage(0,1000,[](FLiveHit& H){H.TrueDamage=7;}),7.f,1.e-3f);
    TestEqual(TEXT("reduction stacks with defense"),Damage(10,0,nullptr,-1,.3f),7.f,1.e-3f);
    TestEqual(TEXT("defense and reduction share the 60% cap"),Damage(10,10,nullptr,-1,.5f),4.f,1.e-3f);
    // Realm suppression: only a lower attacker, same major realm 3% per tier down to 70%, across major realms 0.8^gap down to 25%.
    TestEqual(TEXT("same major realm"),ULiveCombatLibrary::RealmFactor(0,5),.85f,1.e-4f);
    TestEqual(TEXT("same major realm five tiers apart"),ULiveCombatLibrary::RealmFactor(16,21),.85f,1.e-4f);
    TestEqual(TEXT("one major realm apart"),ULiveCombatLibrary::RealmFactor(8,9),.8f,1.e-4f);
    TestEqual(TEXT("cross realm floor"),ULiveCombatLibrary::RealmFactor(0,33),.25f,1.e-4f);
    TestEqual(TEXT("higher attacker unsuppressed"),ULiveCombatLibrary::RealmFactor(20,3),1.f);
    TestEqual(TEXT("unknown realm unsuppressed"),ULiveCombatLibrary::RealmFactor(-1,3),1.f);
    TestEqual(TEXT("suppression applies to the formula"),Damage(10,10,[](FLiveHit& H){H.AttackerRealm=8;},9),4.f,1.e-3f);
    TestEqual(TEXT("true damage ignores suppression"),Damage(0,0,[](FLiveHit& H){H.TrueDamage=9;H.AttackerRealm=0;},33),9.f,1.e-3f);
    // Attribute sheets: 噬魂 lowest of the five in base and growth, 弑神 highest, every special above the normal spirit.
    using namespace LiveSpecialSwords;
    const auto& Normal=Stats(ELiveSpecialSword::None);
    TestEqual(TEXT("normal spirit base attack"),Normal.Attack,10.f);TestEqual(TEXT("normal spirit growth"),Normal.AttackGrowth,.12f);
    TestEqual(TEXT("normal spirit scale"),AttackScale(ELiveSpecialSword::None,20),1.f,1.e-4f);
    for(const ELiveSpecialSword K:All())
    {
        TestTrue(TEXT("special above normal"),Stats(K).Attack>Normal.Attack&&Stats(K).AttackGrowth>Normal.AttackGrowth);
        if(K!=ELiveSpecialSword::SoulEater)TestTrue(TEXT("噬魂 lowest"),Stats(ELiveSpecialSword::SoulEater).Attack<Stats(K).Attack&&Stats(ELiveSpecialSword::SoulEater).AttackGrowth<Stats(K).AttackGrowth);
        if(K!=ELiveSpecialSword::Godslayer)TestTrue(TEXT("弑神 highest"),Stats(ELiveSpecialSword::Godslayer).Attack>Stats(K).Attack&&Stats(ELiveSpecialSword::Godslayer).AttackGrowth>Stats(K).AttackGrowth);
        TestTrue(TEXT("special scale grows with realm"),AttackScale(K,33)>AttackScale(K,0));
        TestTrue(TEXT("破甲 sheet within the cap"),PenetrationAt(K,33)<=LiveCombat::MaxArmorPenetration);
    }
    TestEqual(TEXT("弑神 realm-0 scale"),AttackScale(ELiveSpecialSword::Godslayer,0),1.8f,1.e-4f);
    TestEqual(TEXT("realm index clamps"),AttackAt(ELiveSpecialSword::SoulEater,99),AttackAt(ELiveSpecialSword::SoulEater,33));
    return true;
}
#endif

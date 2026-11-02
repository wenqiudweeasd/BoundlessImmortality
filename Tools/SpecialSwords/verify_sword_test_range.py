"""Rendered PIE check of the sword test range: the 试剑台 panel adding and removing special swords against the scarecrows.

Run inside the editor after build_sword_test_range.py:
  UnrealEditor.exe BoundlessImmortality.uproject -unattended -noxgecontroller -ExecutePythonScript=Tools/SpecialSwords/verify_sword_test_range.py
Every panel action is a real Slate click on its control. Captures (portrait 720x1280, the game camera) and report.json
land in Saved/SwordTestRange/Verification/<time>/.
"""
import json
import time
import traceback
from datetime import datetime
from pathlib import Path

import unreal

ROOT = Path(unreal.Paths.project_dir())
OUT = ROOT / "Saved/SwordTestRange/Verification" / datetime.now().strftime("%Y%m%dT%H%M%S")
OUT.mkdir(parents=True, exist_ok=True)
LEVEL = "/Game/LiveWeapons/Maps/L_SwordTestRange"
SWORD_CLASS = "/Script/BoundlessImmortality.LiveSpecialSword"
SCARECROW_CLASS = "/Script/BoundlessImmortality.LiveScarecrow"
KIND = unreal.LiveSpecialSwordKind
SWORDS = (("SoulEater", KIND.SOUL_EATER), ("Inorganic", KIND.INORGANIC), ("DivineBlade", KIND.DIVINE_BLADE),
          ("SevenStar", KIND.SEVEN_STAR), ("Godslayer", KIND.GODSLAYER))
LIB = unreal.InkFrontendEditorLibrary
REPORT = dict(checks=[], captures=[], scarecrows=[], damage={}, damage_numbers=0, finished=False,
              boundary="Windows editor PIE, local simulation, no SDK, no device run")
STATE = dict(step=-2, next=0.0, world=None, mode=None)
START = time.monotonic()
unreal.EditorPythonScripting.set_keep_python_script_alive(True)


def save():
    REPORT["passed"] = REPORT["finished"] and not REPORT.get("error") and all(c["passed"] for c in REPORT["checks"])
    (OUT / "report.json").write_text(json.dumps(REPORT, ensure_ascii=False, indent=2), "utf-8")


def check(name, ok, actual=None):
    REPORT["checks"].append(dict(name=name, passed=bool(ok), actual=actual))
    save()


def actors(path):
    cls = unreal.load_class(None, path)
    return list(unreal.GameplayStatics.get_all_actors_of_class(STATE["world"], cls)) if cls else []


def capture(tag):
    path = OUT / (tag + ".png")
    check("capture " + tag, LIB.capture_frontend_viewport(str(path)))
    REPORT["captures"].append(tag)


def mode():
    return STATE["mode"]


def swarm():
    return mode().get_editor_property("swarm")


def click(control):
    panel = mode().get_editor_property("sword_panel")
    check("click " + control, panel is not None and panel.click_test_target(control))


def flying():
    return len(actors(SWORD_CLASS))


def expect_only(name, kind):
    present = [n for n, k in SWORDS if mode().find_special_sword(k)]
    check(name + " flies alone", present == [name] and flying() == 1, present)


def expect_none(tag):
    # Removing a sword takes its viewer out too: nothing special and no ordinary audience sword stays behind.
    check(tag + ": no special swords", flying() == 0, flying())
    check(tag + ": swarm empty", swarm().get_rendered_count() == 0, swarm().get_rendered_count())


def gm(action):
    overlay = mode().get_editor_property("overlay")
    check("gm " + action, overlay is not None and overlay.execute_gm_action(action))


def scarecrow_stats(tag):
    rows = []
    for s in actors(SCARECROW_CLASS):
        rows.append(dict(label=s.get_actor_label(), total=round(s.get_editor_property("total_damage")), dps=round(s.get_damage_per_second()),
                         knockdowns=s.get_editor_property("knockdowns"), down=s.is_knocked_down()))
    REPORT["scarecrows"].append(dict(at=tag, rows=rows))
    return rows


STEPS = []


def step(name, fn, delay=0.8):
    STEPS.append((name, fn, delay))


def walk_in():
    """The range is a row of rooms with a realm scarecrow in every other one; stand in front of the first (练气一阶)."""
    first = sorted(actors(SCARECROW_CLASS), key=lambda s: s.get_editor_property("realm_preset"))[0].get_actor_location()
    pawn = unreal.GameplayStatics.get_player_pawn(STATE["world"], 0)
    at = pawn.get_actor_location()
    dx, dy = first.x - at.x, first.y - at.y
    n = max((dx * dx + dy * dy) ** 0.5, 1.0)
    pawn.set_actor_location(unreal.Vector(first.x - dx / n * 330.0, first.y - dy / n * 330.0, at.z), False, True)


step("targets", lambda: check("four realm scarecrows", len(actors(SCARECROW_CLASS)) == 4, len(actors(SCARECROW_CLASS))), 0.2)
step("walk in", walk_in, 1.5)
step("no personal sword", lambda: check("anchor has no personal sword", swarm().get_personal_sword_id() == "" and swarm().get_rendered_count() == 0,
                                        dict(id=swarm().get_personal_sword_id(), rendered=swarm().get_rendered_count())), 0.2)
step("empty start", lambda: check("no special swords at start", flying() == 0, flying()), 0.2)
step("start", lambda: capture("01-start-panel"), 0.3)
# Each chip on its own: call the sword, watch it fight alone, dismiss it.
for index, (name, kind) in enumerate(SWORDS):
    step("add " + name, lambda name=name: click("SwordRange." + name), 1.5)
    step(name + " alone", lambda name=name, kind=kind: expect_only(name, kind), 0.2)
    step(name + " fights", lambda index=index, name=name: capture("%02d-%s-a" % (index + 2, name)), 3.0)
    step(name + " later", lambda index=index, name=name: capture("%02d-%s-b" % (index + 2, name)), 2.5)
    step(name + " damage", lambda name=name, kind=kind: (REPORT["damage"].__setitem__(name, round(mode().get_special_sword_damage(kind))),
                                                           check(name + " dealt damage", mode().get_special_sword_damage(kind) > 0, REPORT["damage"][name])), 0.2)
    step("remove " + name, lambda name=name: click("SwordRange." + name), 1.2)
    step(name + " gone", lambda name=name: expect_none("after removing " + name), 0.2)
def seven():
    return mode().find_special_sword(KIND.SEVEN_STAR)


def cast_radiance():
    # The sword casts once it is between actions; until then ask again shortly.
    sword = seven()
    if sword and sword.cast_skill_now("Radiance"):
        STATE["radiance_at"] = time.monotonic()
        return check("cast 光芒剑气", True)
    if time.monotonic() - STATE.setdefault("radiance_try", time.monotonic()) > 12:
        STATE["radiance_at"] = time.monotonic()
        return check("cast 光芒剑气", False, "sword never free to cast")
    return 0.25


def radiance_launch():
    # The charge lasts five seconds; catch the shot as it leaves the sword.
    left = STATE["radiance_at"] + 5.15 - time.monotonic()
    if left > 0:
        return left
    capture("11-radiance-launch")


# 七星光芒剑's big sword qi (光芒剑气) must land: it used to fly level at the sword's hover height, over every target.
step("add SevenStar", lambda: click("SwordRange.SevenStar"), 1.5)
step("cast radiance", cast_radiance, 0.2)
step("radiance launch", radiance_launch, 0.3)
step("radiance flight", lambda: capture("11-radiance-flight"), 1.0)
step("radiance hits", lambda: check("光芒剑气 hits the scarecrows", seven() is not None and seven().get_skill_damage("Radiance") > 0,
                                    round(seven().get_skill_damage("Radiance")) if seven() else None), 0.2)
step("remove SevenStar", lambda: click("SwordRange.SevenStar"), 1.2)


def cast_ground_wave():
    sword = mode().find_special_sword(KIND.SOUL_EATER)
    if sword and sword.cast_skill_now("GroundWave"):
        STATE["wave_at"] = time.monotonic()
        return check("cast 噬魂剑 GroundWave", True)
    if time.monotonic() - STATE.setdefault("wave_try", time.monotonic()) > 12:
        STATE["wave_at"] = time.monotonic()
        return check("cast 噬魂剑 GroundWave", False, "sword never free to cast")
    return 0.25


def dismiss_mid_charge():
    # The blade plants about 1.1 s in and charges for 2 s at the spot; dismiss it in the middle of that.
    left = STATE["wave_at"] + 1.9 - time.monotonic()
    if left > 0:
        return left
    click("SwordRange.SoulEater")


def live_charges():
    return [c.get_asset().get_name() for c in unreal.ObjectIterator(unreal.NiagaraComponent)
            if c.get_world() == STATE["world"] and c.is_active() and c.get_asset() and c.get_asset().get_name().startswith("NS_SS_Charge")]


# A sword dismissed mid-skill must take its world-space charge effect with it.
step("add SoulEater", lambda: click("SwordRange.SoulEater"), 1.5)
step("cast ground wave", cast_ground_wave, 0.2)
def charge_cleared():
    # A deactivated system lets its particles finish first; an orphaned loop never ends.
    waited = time.monotonic() - STATE.setdefault("dismissed_at", time.monotonic())
    if live_charges() and waited < 6.0:
        return 0.25
    check("charge effect ends after dismissal", not live_charges(), dict(left=live_charges(), seconds=round(waited, 2)))


step("dismiss mid charge", dismiss_mid_charge, 0.1)
step("no orphan charge", charge_cleared, 0.2)
step("all", lambda: click("SwordRange.All"), 1.5)
step("all five", lambda: check("call all: five swords", flying() == 5, flying()), 0.2)
step("all capture", lambda: capture("07-all"), 2.0)
# Slowed down so the damage numbers can be read mid-rise.
step("slow", lambda: unreal.GameplayStatics.set_global_time_dilation(STATE["world"], 0.3), 0.4)
for i in range(4):
    step("numbers %d" % i, lambda i=i: capture("07-numbers-%d" % i), 0.5)
step("normal speed", lambda: unreal.GameplayStatics.set_global_time_dilation(STATE["world"], 1.0), 0.3)
step("numbers shown", lambda: check("damage numbers shown on hits", REPORT["damage_numbers"] > 0, REPORT["damage_numbers"]), 0.2)
step("none", lambda: click("SwordRange.None"), 1.2)
step("none left", lambda: expect_none("after dismissing all"), 0.2)


def anchor_parts():
    pawn = unreal.GameplayStatics.get_player_pawn(STATE["world"], 0)
    return pawn.get_component_by_class(unreal.CultivationHealthComponent), pawn.get_component_by_class(unreal.InkDamageFeedbackComponent)


def hit_seventh():
    health, _ = anchor_parts()
    before, top = health.get_editor_property("health"), health.get_editor_property("max_health")
    ok = mode().get_editor_property("overlay").execute_gm_action("SS_HitSeventh")
    after = health.get_editor_property("health")
    check("GM 主播受击 1/7 lands", ok and before - after >= top / 7.0 * 0.95, dict(before=round(before, 1), after=round(after, 1), max=round(top, 1)))


def damage_sample():
    # The 精确伤害比例样本 buttons preview hit feedback without touching health; 42% is the third tier.
    _, feedback = anchor_parts()
    events = feedback.get_editor_property("damage_event_count")
    ok = mode().get_editor_property("overlay").get_editor_property("test_session").run_action("Damage_42")
    tier, after = feedback.get_editor_property("last_damage_tier"), feedback.get_editor_property("damage_event_count")
    check("GM 伤害样本 42% plays tier 3", ok and tier == 3 and after == events + 1, dict(tier=tier, events=after - events))


def knock_out():
    health, _ = anchor_parts()
    health.apply_direct_damage(health.get_editor_property("max_health") * 2)
    check("anchor knocked out", health.get_editor_property("dead"), round(health.get_editor_property("health"), 1))


def revived():
    health, _ = anchor_parts()
    full = health.get_editor_property("health") >= health.get_editor_property("max_health") * 0.99
    check("range revives the anchor", not health.get_editor_property("dead") and full, round(health.get_editor_property("health"), 1))


# GM anchor-damage tests, with no swords out so no shield or heal masks them.
step("anchor hit seventh", hit_seventh, 0.5)
step("anchor damage sample", damage_sample, 0.5)
step("anchor knocked out", knock_out, 3.7)
step("anchor revived", revived, 0.3)
step("fold", lambda: click("SwordRange.Fold"), 0.6)
step("folded", lambda: capture("08-folded"), 0.3)
step("unfold", lambda: click("SwordRange.Fold"), 0.6)
# A returning viewer keeps its sword title: re-adding brings the same sword back.
step("re-add", lambda: click("SwordRange.SoulEater"), 1.5)
step("re-added", lambda: expect_only("SoulEater", KIND.SOUL_EATER), 0.2)
step("damage dealt", lambda: (lambda rows: check("scarecrows took damage", sum(r["total"] for r in rows) > 0, rows))(scarecrow_stats("fight")), 0.2)
# Low health lets 噬魂剑 devour; devoured or killed scarecrows stand back up after their respawn delay.
step("low health", lambda: gm("SS_LowHealth"), 0.8)
for i in range(3):
    step("devour %d" % i, lambda i=i: capture("09-devour-%d" % i), 1.0)
step("knockdowns", lambda: STATE.update(down=[r for r in scarecrow_stats("after low health") if r["down"]]), 3.0)


def stood_up():
    # Scarecrows at 12% keep falling under fire, so check the ones that were down: standing again, or felled anew.
    later = {r["label"]: r for r in scarecrow_stats("recovered")}
    ok = all(not later[r["label"]]["down"] or later[r["label"]]["knockdowns"] > r["knockdowns"] for r in STATE["down"])
    check("knocked-down scarecrows stand back up", ok, dict(down=[r["label"] for r in STATE["down"]], later=list(later.values())))


step("stood up", stood_up, 0.2)
step("recovered", lambda: capture("10-recovered"), 0.3)


def finish():
    unreal.unregister_slate_post_tick_callback(HANDLE[0])
    unreal.EditorLevelLibrary.editor_end_play()

    def quit_tick(_):
        if unreal.EditorLevelLibrary.get_pie_worlds(False):
            return
        unreal.unregister_slate_post_tick_callback(QUIT[0])
        unreal.EditorPythonScripting.set_keep_python_script_alive(False)
        unreal.SystemLibrary.quit_editor()

    QUIT[0] = unreal.register_slate_post_tick_callback(quit_tick)


def tick(_):
    if STATE["mode"] and "sample_error" not in REPORT:
        # Numbers live about a second; sample every frame for the most seen at once.
        try:
            live = sum(s.get_live_damage_numbers() for s in actors(SWORD_CLASS))
            REPORT["damage_numbers"] = max(REPORT["damage_numbers"], live)
        except Exception:
            REPORT["sample_error"] = traceback.format_exc()
    if time.monotonic() < STATE["next"]:
        return
    try:
        if time.monotonic() - START > 300:
            raise TimeoutError("verification exceeded 300s")
        if STATE["step"] == -2:
            check("begin PIE", LIB.begin_frontend_pie(720, 1280))
            STATE["step"] = -1
            STATE["next"] = time.monotonic() + 8
            return
        if STATE["step"] == -1:
            # Ready once the test mode switched on local simulation and put up the panel.
            worlds = unreal.EditorLevelLibrary.get_pie_worlds(False)
            STATE["world"] = worlds[0] if worlds else None
            found = unreal.GameplayStatics.get_game_mode(STATE["world"]) if STATE["world"] else None
            if not found or not found.is_test_ready() or not found.get_editor_property("sword_panel"):
                if time.monotonic() - START > 90:
                    raise TimeoutError("test range never became ready")
                STATE["next"] = time.monotonic() + 1
                return
            check("test game mode", found.get_class().get_name() == "LiveSwordTestGameMode", found.get_class().get_name())
            STATE["mode"] = found
            STATE["step"] = 0
            STATE["next"] = time.monotonic() + 1.5
            return
        if STATE["step"] >= len(STEPS):
            REPORT["finished"] = True
            save()
            finish()
            return
        name, fn, delay = STEPS[STATE["step"]]
        REPORT["current_step"] = name
        again = fn()
        # A step that returns seconds is not done yet: run it again after that wait.
        if isinstance(again, float):
            STATE["next"] = time.monotonic() + again
            return
        STATE["step"] += 1
        STATE["next"] = time.monotonic() + delay
        save()
    except Exception:
        REPORT["error"] = traceback.format_exc()
        save()
        finish()


assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level(LEVEL), "could not open " + LEVEL
HANDLE = [unreal.register_slate_post_tick_callback(tick)]
QUIT = [None]
unreal.log("Sword test range verification writing to " + str(OUT))

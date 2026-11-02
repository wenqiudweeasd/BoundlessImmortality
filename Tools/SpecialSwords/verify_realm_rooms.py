"""Rendered PIE check of the room range: realm scarecrows and their plates, the sword realm stepper (dock and 剑谱), the
噬魂 fire on what it strikes and its serpent devour and growth, 七星光芒剑's lightning (80% attack + 5% of current health)
and 488% sword qi, the mouse-wheel zoom, and the tribulation backlash (天罚): it fells an anchor without a 七星光芒剑, breaks
the guard of a 练气 sword against the 元婴 tribulation (天雷破盾), and a 化神 sword's guard that holds (七星挡劫, 50%, forced
here) brings the anchor through with a broken Dao heart.

Run inside the editor (no other editor open), after build_sword_test_range.py and the effect rebuild:
  UnrealEditor.exe BoundlessImmortality.uproject -unattended -noxgecontroller -nosplash -NoLiveCoding
      -ExecutePythonScript=Tools/SpecialSwords/verify_realm_rooms.py
Panel actions are real Slate clicks. Portrait 720x1280 captures and report.json land in
Saved/SwordTestRange/Rooms/<time>/.
"""
import json
import math
import re
import time
import traceback
from datetime import datetime
from pathlib import Path

import unreal

ROOT = Path(unreal.Paths.project_dir())
OUT = ROOT / "Saved/SwordTestRange/Rooms" / datetime.now().strftime("%Y%m%dT%H%M%S")
OUT.mkdir(parents=True, exist_ok=True)
LEVEL = "/Game/LiveWeapons/Maps/L_SwordTestRange"
STORM_CLASS = "/Script/BoundlessImmortality.XianTuTribulationStorm"
KIND = unreal.LiveSpecialSwordKind
LIB = unreal.InkFrontendEditorLibrary
REPORT = dict(checks=[], captures=[], backlash=[], finished=False, boundary="Windows editor PIE, local simulation, no SDK, no device run")
STATE = dict(step=-2, next=0.0, world=None, mode=None, min_backlash=None)
START = time.monotonic()
TIMEOUT = 900
# Same-realm cultivator attributes (ALiveScarecrow::Realm*): 生命 1000(1+0.15r), 攻击 10(1+0.12r), 防御 20(1+0.10r).
REALMS = {0: (1000.0, 10.0, 20.0), 9: (2350.0, 20.8, 38.0), 22: (4300.0, 36.4, 64.0), 23: (4450.0, 37.6, 66.0)}
unreal.EditorPythonScripting.set_keep_python_script_alive(True)


def save():
    REPORT["passed"] = REPORT["finished"] and not REPORT.get("error") and all(c["passed"] for c in REPORT["checks"])
    (OUT / "report.json").write_text(json.dumps(REPORT, ensure_ascii=False, indent=2), "utf-8")


def check(name, ok, actual=None):
    REPORT["checks"].append(dict(name=name, passed=bool(ok), actual=actual))
    save()
    return bool(ok)


def capture(tag):
    path = OUT / (tag + ".png")
    check("capture " + tag, LIB.capture_frontend_viewport(str(path)))
    REPORT["captures"].append(tag)


def mode():
    return STATE["mode"]


def panel():
    return mode().get_editor_property("sword_panel")


def click(control):
    check("click " + control, panel() is not None and panel().click_test_target(control))


def pawn():
    return unreal.GameplayStatics.get_player_pawn(STATE["world"], 0)


def controller():
    return unreal.GameplayStatics.get_player_controller(STATE["world"], 0)


def health():
    return pawn().get_component_by_class(unreal.CultivationHealthComponent)


def storm():
    cls = unreal.load_class(None, STORM_CLASS)
    found = list(unreal.GameplayStatics.get_all_actors_of_class(STATE["world"], cls)) if cls else []
    return found[0] if found else None


def status():
    return str(mode().get_tribulation_status())


def scarecrows():
    return list(mode().get_scarecrows())


def dilate(rate):
    unreal.GameplayStatics.set_global_time_dilation(STATE["world"], rate)


STEPS = []


def step(name, fn, delay=0.8):
    STEPS.append((name, fn, delay))


def wait_until(key, condition, limit, what):
    """A step that polls: returns a short wait until condition() holds or the limit passes (then the check fails)."""
    def run():
        started = STATE.setdefault("since_" + key, time.monotonic())
        if condition():
            return check(what, True, round(time.monotonic() - started, 2))
        if time.monotonic() - started > limit:
            return check(what, False, "not within %ss" % limit)
        return 0.2
    return run


# ---------------------------------------------------------------- rooms and scarecrows


def layout():
    found = scarecrows()
    realms = [s.get_editor_property("realm_preset") for s in found]
    check("four realm scarecrows: 练气一阶, 筑基初期, 元婴, 化神", realms == [0, 9, 22, 23], realms)
    rows = []
    for s in found:
        r = s.get_editor_property("realm_preset")
        hp, atk, df = REALMS.get(r, (0, 0, 0))
        # The range keeps gift power out (no 神秘空投 enemy reduction), so the current values are the set ones.
        row = dict(realm=r, realm_index=s.get_editor_property("realm"), max_health=round(s.get_editor_property("max_health"), 2),
                   attack=round(s.get_editor_property("attack_damage"), 2), defense=round(s.get_editor_property("defense"), 2),
                   multiplier=s.get_editor_property("enemy_attribute_multiplier"))
        rows.append(row)
        check("scarecrow %d has its realm attributes" % r, abs(row["max_health"] - hp) < 0.5 and abs(row["attack"] - atk) < 0.05
              and abs(row["defense"] - df) < 0.05 and row["realm_index"] == r, row)
    REPORT["scarecrows"] = rows
    start = pawn().get_actor_location()
    gaps = [round(math.dist((found[i].get_actor_location().x, found[i].get_actor_location().y),
                            (found[i + 1].get_actor_location().x, found[i + 1].get_actor_location().y))) for i in range(len(found) - 1)]
    check("one scarecrow every other room (22 m apart)", all(abs(g - 2200) < 5 for g in gaps), gaps)
    STATE["start"] = (start.x, start.y, start.z)


def go_to(index):
    """Stands the cultivator 3.3 m in front of scarecrow `index`, on the side the player comes from."""
    s = scarecrows()[index].get_actor_location()
    sx, sy, sz = STATE["start"]
    dx, dy = s.x - sx, s.y - sy
    n = max(math.hypot(dx, dy), 1.0)
    where = unreal.Vector(s.x - dx / n * 330.0, s.y - dy / n * 330.0, sz)
    pawn().set_actor_location(where, False, True)
    REPORT.setdefault("teleports", []).append(dict(index=index, x=where.x, y=where.y))


def sword_realms(expected):
    swords = [mode().find_special_sword(k) for k in (KIND.SOUL_EATER, KIND.INORGANIC, KIND.DIVINE_BLADE, KIND.SEVEN_STAR, KIND.GODSLAYER)]
    realms = [s.get_realm() if s else None for s in swords]
    attacks = [round(s.get_attack_power(), 2) if s else None for s in swords]
    REPORT.setdefault("sword_realms", []).append(dict(expected=expected, realms=realms, attacks=attacks))
    return check("all five swords fight at realm %d" % expected, realms == [expected] * 5 and mode().get_common_sword_realm() == expected,
                 dict(realms=realms, attacks=attacks))


step("layout", layout, 0.3)
step("start room", lambda: capture("00-start-room"), 0.3)
step("go lianqi", lambda: go_to(0), 1.2)
step("all swords", lambda: click("SwordRange.All"), 3.5)
step("lianqi room", lambda: capture("01-lianqi-room"), 0.2)
step("realm 0", lambda: sword_realms(0), 0.2)
step("damage lands", lambda: check("swords damage the 练气 scarecrow", scarecrows()[0].get_editor_property("total_damage") > 0,
                                   round(scarecrows()[0].get_editor_property("total_damage"), 1)), 0.2)
step("realm next", lambda: click("SwordRange.RealmNext"), 1.2)
step("realm 9", lambda: sword_realms(9), 0.2)
step("zhuji dock", lambda: capture("02-realm-zhuji"), 0.2)
step("go huashen", lambda: go_to(3), 2.0)
step("huashen room", lambda: capture("03-huashen-room"), 3.0)
step("huashen damage", lambda: check("swords reach the 化神 scarecrow", scarecrows()[3].get_editor_property("total_damage") > 0,
                                     round(scarecrows()[3].get_editor_property("total_damage"), 1)), 0.2)
# The player stands 3.3 m in front of 化神, toward 元婴 two rooms back (18.7 m away): no sword skill reaches that far.
step("rooms isolate", lambda: check("the 元婴 scarecrow in the previous scarecrow room takes no hits", scarecrows()[2].get_editor_property("total_damage") == 0,
                                    round(scarecrows()[2].get_editor_property("total_damage"), 1)), 0.2)

# ---------------------------------------------------------------- 剑谱 sets a sword's realm


step("open codex", lambda: click("SwordRange.Codex"), 0.8)
step("codex souleater", lambda: check("codex opens on 噬魂剑", panel().get_codex_selection() == KIND.SOUL_EATER), 0.2)
step("codex realm next", lambda: click("SwordCodex.RealmNext"), 1.0)
step("codex realm", lambda: check("剑谱 arrows move 噬魂剑 alone to 结晶一阶", mode().get_sword_realm(KIND.SOUL_EATER) == 13
                                   and mode().get_sword_realm(KIND.GODSLAYER) == 9 and mode().get_common_sword_realm() == -1,
                                   dict(soul=mode().get_sword_realm(KIND.SOUL_EATER), godslayer=mode().get_sword_realm(KIND.GODSLAYER))), 0.6)
step("codex sword", lambda: check("噬魂剑 now fights at 结晶一阶", mode().find_special_sword(KIND.SOUL_EATER).get_realm() == 13,
                                  mode().find_special_sword(KIND.SOUL_EATER).get_realm()), 0.2)
step("codex capture", lambda: capture("04-codex-realm"), 0.3)
step("close codex", lambda: click("SwordCodex.Close"), 0.6)
step("dock mixed", lambda: capture("05-dock-mixed"), 0.2)
step("realm back", lambda: (mode().set_all_sword_realms(0), click("SwordRange.None")), 1.5)

# ---------------------------------------------------------------- 噬魂 serpent devour


def devour_setup():
    click("SwordRange.SoulEater")
    go_to(0)


def devour_start():
    target = scarecrows()[0]
    sword = mode().find_special_sword(KIND.SOUL_EATER)
    # Two and a half seconds of 噬魂 hitting the scarecrow: it burns with violet fire.
    check("噬魂 sets what it strikes on fire", sword is not None and sword.get_burning() >= 1, sword.get_burning() if sword else None)
    capture("06-burning")
    target.restore()
    target.set_health_fraction(0.14)
    before = mode().get_sword_growth(KIND.SOUL_EATER)
    STATE["growth_before"] = (before.devoured, before.devoured_attack, before.devoured_defense, before.devoured_health)
    STATE["knockdowns"] = target.get_editor_property("knockdowns")
    dilate(0.2)
    check("serpent sent at the 14% scarecrow", sword is not None and sword.devour_now(target))
    STATE["devour_t0"] = time.monotonic()


# Captures on the effect clock: real time is five times slower under 0.2 time dilation.
DEVOUR_FRAMES = (0.08, 0.22, 0.36, 0.44, 0.6, 0.85, 1.1, 1.3)


def devour_frames():
    done = STATE.setdefault("devour_frames", 0)
    if done >= len(DEVOUR_FRAMES):
        return check("devour frames captured", True, done)
    due = STATE["devour_t0"] + DEVOUR_FRAMES[done] / 0.2
    if time.monotonic() < due:
        return 0.02
    capture("06-devour-%.2f" % DEVOUR_FRAMES[done])
    STATE["devour_frames"] = done + 1
    return 0.01


def devour_result():
    # Slow screenshots hold the game clock back (frame time is clamped), so wait for the effect's own absorb.
    dilate(1.0)
    sword = mode().find_special_sword(KIND.SOUL_EATER)
    started = STATE.setdefault("absorb_wait", time.monotonic())
    if sword is not None and sword.get_devours_in_flight() > 0 and time.monotonic() - started < 5.0:
        return 0.1
    g = mode().get_sword_growth(KIND.SOUL_EATER)
    before = STATE["growth_before"]
    gained = dict(devoured=g.devoured - before[0], attack=round(g.devoured_attack - before[1], 3), defense=round(g.devoured_defense - before[2], 3),
                  health=round(g.devoured_health - before[3], 3))
    # 练气一阶 scarecrow: 1% of attack 10 plus 5% of health 1000 as attack, 1% of defense 20 and of health 1000.
    check("devour takes 1% of the attributes and 5% of health as attack", gained["devoured"] == 1 and abs(gained["attack"] - 50.1) < 0.01
          and abs(gained["defense"] - 0.2) < 0.001 and abs(gained["health"] - 10.0) < 0.001, gained)
    target = scarecrows()[0]
    check("the swallowed scarecrow goes down", target.get_editor_property("knockdowns") == STATE["knockdowns"] + 1,
          target.get_editor_property("knockdowns"))


step("devour setup", devour_setup, 2.5)
step("devour start", devour_start, 0.0)
step("devour frames", devour_frames, 0.2)
step("devour wait", lambda: None, 0.6)
step("devour result", devour_result, 0.5)

# ---------------------------------------------------------------- zoom


def zoom(value, tag):
    pc = controller()
    pc.set_camera_zoom(value, True)
    arm = pawn().get_component_by_class(unreal.SpringArmComponent)
    REPORT.setdefault("zoom", []).append(dict(zoom=round(pc.get_camera_zoom(), 3), arm=round(arm.get_editor_property("target_arm_length"), 1) if arm else None))
    if tag:
        capture(tag)


def zoom_check():
    rows = REPORT["zoom"]
    check("zoom pulls the boom in", len(rows) >= 2 and rows[1]["arm"] is not None and abs(rows[1]["arm"] - rows[0]["arm"] * 0.25) < 1.0, rows)


step("zoom base", lambda: zoom(1.0, None), 0.6)
step("zoom in", lambda: zoom(0.25, None), 1.2)
step("zoom capture", lambda: capture("07-zoom-in"), 0.3)
step("zoom check", zoom_check, 0.2)
step("zoom out", lambda: zoom(1.0, None), 0.8)

# ---------------------------------------------------------------- 七星光芒剑 numbers


def seven():
    return mode().find_special_sword(KIND.SEVEN_STAR)


def seven_setup():
    """At the back of the first room, more than 14 m from every scarecrow, with a dummy (1M health, no defense, no realm)
    3 m to the side: the only enemy the seven bolts and the sword qi can reach."""
    click("SwordRange.None")
    mode().set_all_sword_realms(0)
    sx, sy, sz = STATE["start"]
    s = scarecrows()[0].get_actor_location()
    dx, dy = s.x - sx, s.y - sy
    n = max(math.hypot(dx, dy), 1.0)
    dx, dy = dx / n, dy / n
    spot = unreal.Vector(sx - dx * 380.0, sy - dy * 380.0, sz)
    pawn().set_actor_location(spot, False, True)
    STATE["dummy"] = mode().spawn_test_target(1.0e6, 0.0, -1, unreal.Vector(spot.x - dy * 300.0, spot.y + dx * 300.0, sz))
    REPORT["seven_setup"] = dict(nearest_scarecrow=round(math.dist((spot.x, spot.y), (s.x, s.y))))
    click("SwordRange.SevenStar")


def seven_cast(skill):
    def run():
        sword, dummy = seven(), STATE.get("dummy")
        # Skill damage so far: the sword may have cast this skill on its own before. Strikes from such a cast land up to
        # half a second after its sword qi hits, so wait until their count has held still for 0.7 s.
        before = (sword.get_skill_damage(skill), sword.get_skill_damage("Strike"), sword.get_radiance_strikes()) if sword else None
        settled = STATE.setdefault("strikes_" + skill, [None, time.monotonic()])
        if before and before[2] != settled[0]:
            settled[0], settled[1] = before[2], time.monotonic()
        quiet = time.monotonic() - settled[1] >= 0.7
        if sword and dummy and quiet and skill == "Radiance":
            # 45% per release: make this one call its strikes down, so the strike check always runs.
            sword.debug_force_strike_roll(1)
        if sword and dummy and quiet and sword.cast_skill_now(skill):
            STATE[skill + "_h0"] = dummy.get_editor_property("health")
            STATE[skill + "_a"] = sword.get_attack_power()
            STATE[skill + "_d0"] = before
            return check("七星 casts " + skill, True)
        started = STATE.setdefault("since_cast_" + skill, time.monotonic())
        return check("七星 casts " + skill, False, "never free to cast") if time.monotonic() - started > 12 else 0.1
    return run


def lightning_check():
    sword = seven()
    dealt = sword.get_skill_damage("Lightning") - STATE["Lightning_d0"][0]
    h0, a, crit = STATE["Lightning_h0"], STATE["Lightning_a"], sword.get_critical_damage()
    # Seven bolts on the one dummy, each 80% of the attack plus 5% of the health it has left.
    low = h0 * (1.0 - 0.95 ** 7)
    high = low + 7 * 0.8 * a * crit
    REPORT["lightning"] = dict(dealt=round(dealt, 1), health_before=round(h0, 1), attack=round(a, 2), expected=[round(low, 1), round(high, 1)],
                               old_rule_35pct=round(0.35 * h0, 1))
    check("七星引雷: seven bolts of 80% attack + 5% of current health", low - 1.0 <= dealt <= high + 1.0, REPORT["lightning"])


def radiance_check():
    sword = seven()
    dealt = sword.get_skill_damage("Radiance") - STATE["Radiance_d0"][0]
    if dealt <= 0:
        started = STATE.setdefault("since_radiance", time.monotonic())
        return check("光芒剑气 lands", False, "no hit") if time.monotonic() - started > 9 else 0.2
    capture("07-radiance-hit")
    a, crit = STATE["Radiance_a"], sword.get_critical_damage()
    plain = 4.88 * a
    REPORT["radiance"] = dict(dealt=round(dealt, 2), attack=round(a, 2), plain=round(plain, 2), critical=round(plain * crit, 2))
    check("光芒剑气 deals 488% of attack", abs(dealt - plain) < 0.02 * plain or abs(dealt - plain * crit) < 0.02 * plain * crit, REPORT["radiance"])


def radiance_strikes():
    sword = seven()
    _, strike0, called0 = STATE["Radiance_d0"]
    called, strike = sword.get_radiance_strikes() - called0, sword.get_skill_damage("Strike") - strike0
    REPORT["radiance"]["strikes_called"] = called
    REPORT["radiance"]["strike_damage"] = round(strike, 1)
    # This release was made to call them (debug_force_strike_roll): all three land.
    check("光芒剑气 called down three strikes", called == 3 and strike > 0, REPORT["radiance"])


step("seven setup", seven_setup, 2.5)
step("seven lightning", seven_cast("Lightning"), 0.75)
step("lightning lands", lambda: capture("07-seven-lightning"), 1.0)
step("lightning check", lightning_check, 0.2)
step("seven radiance", seven_cast("Radiance"), 0.1)
step("radiance wait", lambda: None, 5.6)
step("radiance check", radiance_check, 0.2)
step("radiance strike frame", lambda: capture("07-radiance-strikes"), 1.0)
step("radiance strikes", radiance_strikes, 0.2)
step("seven done", lambda: (STATE["dummy"].destroy_actor() if STATE.get("dummy") else None, click("SwordRange.None"), go_to(0)), 1.0)

# ---------------------------------------------------------------- tribulation backlash


def remaining():
    m = re.search(r"剩余 ([\d.]+)秒", status())
    return float(m.group(1)) if m else None


def trial_late():
    """The trial at x10 until its last 12 s, then real time so the roll and the backlash play at their own pace."""
    text = status()
    left = remaining()
    if ("劫" in text and "剩余" in text and left is not None and left <= 12.0 and "天罚" not in text) or "天罚" in text:
        mode().get_editor_property("overlay").execute_gm_action("XT_Scale1")
        return True
    return False


def backlash_frames(tag):
    def run():
        s = storm()
        text = status()
        rows = STATE.setdefault("frames_" + tag, [])
        h = health()
        # The result line names the 天罚 too ("陨落于第 n 道天罚"), so it is checked first.
        if h and "天罚" in text and "渡劫失败" not in text:
            frac = h.get_editor_property("health") / max(h.get_editor_property("max_health"), 1.0)
            rows.append(dict(strikes=s.get_backlash_strikes() if s else None, health=round(frac, 4)))
            if len(rows) in (3, 18, 40):
                capture("08-%s-backlash-%02d" % (tag, len(rows)))
            return 0.15
        if "渡劫失败" in text:
            REPORT["backlash"].append(dict(tag=tag, status=text, samples=rows[-6:], strikes=s.get_backlash_strikes() if s else None,
                                           damage=round(s.get_backlash_damage(), 1) if s else None))
            capture("09-%s-result" % tag)
            return True
        started = STATE.setdefault("since_frames_" + tag, time.monotonic())
        if time.monotonic() - started > 60:
            return check(tag + " backlash reached", False, text)
        return 0.15
    return lambda: (lambda r: check(tag + " backlash played", True) if r is True else r)(run())


def meditation():
    return pawn().get_component_by_class(unreal.CultivationGameplayComponent)


def push(tag, expect_moved):
    """Feeds movement input for half a second, then checks whether the anchor moved (渡劫 keeps it seated)."""
    def run():
        p = pawn()
        rows = STATE.setdefault("push_" + tag, [])
        if not rows:
            rows.append(p.get_actor_location())
        if len(rows) < 12:
            p.add_movement_input(unreal.Vector(0.7, 0.7, 0.0), 1.0, True)
            rows.append(None)
            return 0.05
        moved = round((p.get_actor_location() - rows[0]).length(), 1)
        REPORT.setdefault("push", {})[tag] = moved
        if expect_moved:
            return check("the anchor walks when no tribulation holds it (%s)" % tag, moved > 0.5, moved)
        return check("渡劫 keeps the anchor from moving (%s)" % tag, moved < 1.0, moved)
    return run


def seated(expect, what):
    def run():
        m = meditation()
        state = str(m.get_editor_property("state")) if m else None
        locked = bool(m and m.is_tribulation_meditation())
        REPORT.setdefault("meditation", []).append(dict(what=what, locked=locked, state=state))
        ok = locked and "MEDITATION" in state.upper() if expect else (not locked and state is not None and "IDLE" in state.upper())
        return check(what, ok, dict(locked=locked, state=state))
    return run


def start_trib(assists):
    def run():
        check("rehearsal starts", mode().start_tribulation_test())
        mode().get_editor_property("overlay").execute_gm_action("XT_Scale10")
        STATE["assists_" + str(assists)] = 0
    return run


def assist(assists):
    def run():
        if "劫" in status() and "剩余" in status():
            for _ in range(assists):
                click("SwordRange.Trib")
            return True
        return 0.2
    return lambda: (lambda r: check("helpers sent %d" % assists, True) if r is True else r)(run())


def ward_odds(expected, what):
    """The dock names the 七星挡劫 chance while the trial runs."""
    def run():
        text = status()
        if "七星挡劫" in text:
            return check(what, ("七星挡劫 %s%%" % expected) in text, text)
        started = STATE.setdefault("since_odds_" + what, time.monotonic())
        return check(what, False, text) if time.monotonic() - started > 10 else 0.2
    return run


step("dismiss", lambda: click("SwordRange.None"), 1.0)
step("outcome fail", lambda: click("SwordRange.TribRoll"), 0.3)
step("outcome is fail", lambda: check("rehearsal outcome set to 必败", mode().get_tribulation_outcome() == 1), 0.2)
# No 七星光芒剑: 50%, full 劫威, and fifty bolts worth twice the anchor's health — it falls.
step("push free", push("before", True), 0.3)
step("trib fall", start_trib(0), 2.0)
step("seated fall", seated(True, "渡劫: the anchor sits in meditation from the preparation on"), 0.1)
step("push seated", push("trial", False), 0.2)
step("seated frame", lambda: capture("08-meditating"), 0.2)
step("trial fall", wait_until("trial_fall", trial_late, 40, "trial nears its end (fall run)"), 0.2)
step("backlash fall", backlash_frames("fall"), 0.5)
step("storm gone fall", wait_until("gone_fall", lambda: not mode().is_tribulation_active(), 40, "storm clears after the fall run"), 2.5)
step("ward none", lambda: check("no 七星挡劫 roll without a 七星光芒剑", mode().get_star_ward() == 0, mode().get_star_ward()), 0.2)
# A 练气一阶 七星光芒剑 against the 元婴 tribulation: no chance to hold. The heavens break its light, its shield and the
# star healing do nothing against the bolts, and the anchor falls.
step("seven lianqi", lambda: (mode().set_sword_realm(KIND.SEVEN_STAR, 0), mode().set_star_ward_outcome(0), click("SwordRange.SevenStar")), 2.5)
step("trib broken", start_trib(0), 0.5)
step("odds broken", ward_odds("0", "练气 七星 has no chance against the 元婴 tribulation"), 0.2)
step("trial broken", wait_until("trial_broken", trial_late, 40, "trial nears its end (broken run)"), 0.2)
step("backlash broken", backlash_frames("broken"), 0.5)
step("storm gone broken", wait_until("gone_broken", lambda: not mode().is_tribulation_active(), 40, "storm clears after the broken run"), 2.5)
step("ward broken", lambda: check("天雷破盾: the 练气 guard breaks", mode().get_star_ward() == 2 and mode().get_star_ward_chance() == 0,
                                   (mode().get_star_ward(), mode().get_star_ward_chance())), 0.2)
# The same sword at 化神, one major realm above 元婴: 50% to hold. Forced to hold here, it keeps the backlash from felling
# the anchor, which comes through with a broken Dao heart.
step("seven huashen", lambda: (mode().set_sword_realm(KIND.SEVEN_STAR, 23), mode().set_star_ward_outcome(2)), 1.5)
step("trib held", start_trib(0), 0.5)
step("odds held", ward_odds("50", "化神 七星 holds the 元婴 tribulation half the time"), 0.2)
step("trial held", wait_until("trial_held", trial_late, 40, "trial nears its end (held run)"), 0.2)
step("backlash held", backlash_frames("held"), 0.5)


def outcomes():
    rows = {r["tag"]: r for r in REPORT["backlash"]}
    texts = {tag: r["status"] for tag, r in rows.items()}
    check("all three runs fail through the backlash", len(rows) == 3 and all(r["strikes"] and r["strikes"] > 0 for r in rows.values()), texts)
    check("without 七星 the backlash fells the anchor", "陨落" in texts.get("fall", ""), texts.get("fall"))
    check("a broken guard does not save it: 天雷破盾, 陨落", "天雷破盾" in texts.get("broken", "") and "陨落" in texts.get("broken", ""), texts.get("broken"))
    held = rows.get("held", {})
    lowest = min((s["health"] for s in STATE.get("frames_held", [])), default=None)
    check("a held guard brings it through: 七星挡劫, 道心破碎", "七星挡劫" in texts.get("held", "") and "道心破碎" in texts.get("held", ""), texts.get("held"))
    check("held, the bolts stop at the 10% floor", lowest is not None and lowest >= 0.095, lowest)
    check("held ward recorded at 50%", mode().get_star_ward() == 1 and mode().get_star_ward_chance() == 5000,
          (mode().get_star_ward(), mode().get_star_ward_chance()))
    REPORT["held_lowest_health"] = lowest


step("outcomes", outcomes, 0.2)
step("storm gone held", wait_until("gone_held", lambda: not mode().is_tribulation_active(), 40, "storm clears after the held run"), 2.0)
step("risen held", seated(False, "after the tribulation the anchor rises (收功) and is free"), 0.1)
step("push after", push("after", True), 0.3)
step("ward random", lambda: (mode().set_star_ward_outcome(0), click("SwordRange.None")), 0.5)
step("after", lambda: capture("10-after"), 0.2)


def finish():
    dilate(1.0) if STATE["world"] else None
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
    if time.monotonic() < STATE["next"]:
        return
    try:
        if time.monotonic() - START > TIMEOUT:
            raise TimeoutError("verification exceeded %ss" % TIMEOUT)
        if STATE["step"] == -2:
            check("begin PIE", LIB.begin_frontend_pie(720, 1280))
            STATE["step"] = -1
            STATE["next"] = time.monotonic() + 8
            return
        if STATE["step"] == -1:
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
unreal.log("Realm room verification writing to " + str(OUT))

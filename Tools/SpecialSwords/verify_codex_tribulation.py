"""Rendered PIE check of the 剑谱 codex, the sword attribute sheets, the core damage formula and the tribulation storm,
in the sword test range.

Run inside the editor (no other editor open):
  UnrealEditor.exe BoundlessImmortality.uproject -unattended -noxgecontroller -nosplash -NoLiveCoding
      -ExecutePythonScript=Tools/SpecialSwords/verify_codex_tribulation.py
Every panel action is a real Slate click on its control. Captures (portrait 720x1280 unless named landscape) and
report.json land in Saved/SwordTestRange/Tribulation/<time>/.
With -SwordRangeOrtho on the editor command line the range plays through the main game's orthographic camera and
only the tribulation runs; output goes to <time>-ortho.
"""
import json
import time
import traceback
from datetime import datetime
from pathlib import Path

import unreal

ROOT = Path(unreal.Paths.project_dir())
ORTHO = "-swordrangeortho" in unreal.SystemLibrary.get_command_line().lower()
OUT = ROOT / "Saved/SwordTestRange/Tribulation" / (datetime.now().strftime("%Y%m%dT%H%M%S") + ("-ortho" if ORTHO else ""))
OUT.mkdir(parents=True, exist_ok=True)
LEVEL = "/Game/LiveWeapons/Maps/L_SwordTestRange"
STORM_CLASS = "/Script/BoundlessImmortality.XianTuTribulationStorm"
KIND = unreal.LiveSpecialSwordKind
LIB = unreal.InkFrontendEditorLibrary
REPORT = dict(checks=[], captures=[], strikes=[], health=[], finished=False,
              boundary="Windows editor PIE, local simulation, no SDK, no device run")
STATE = dict(step=-2, next=0.0, world=None, mode=None, watch=None, bolt_captures=0, min_health=1.0, died=False)
START = time.monotonic()
TIMEOUT = 420
# Sheet values (LiveSpecialSwords::Stats): base and per-realm growth of 会心率, 会心伤害, 破甲 and the strike interval.
SHEET = {"SoulEater": (0.03, 0.001, 2.0, 0.0, 0.0, 0.002, 1.15), "Godslayer": (0.08, 0.0025, 2.5, 0.02, 0.12, 0.005, 1.0)}
unreal.EditorPythonScripting.set_keep_python_script_alive(True)


def save():
    REPORT["passed"] = REPORT["finished"] and not REPORT.get("error") and all(c["passed"] for c in REPORT["checks"])
    (OUT / "report.json").write_text(json.dumps(REPORT, ensure_ascii=False, indent=2), "utf-8")


def check(name, ok, actual=None):
    REPORT["checks"].append(dict(name=name, passed=bool(ok), actual=actual))
    save()
    return bool(ok)


def actors(path):
    cls = unreal.load_class(None, path)
    return list(unreal.GameplayStatics.get_all_actors_of_class(STATE["world"], cls)) if cls else []


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


def gm(action):
    overlay = mode().get_editor_property("overlay")
    check("gm " + action, overlay is not None and overlay.execute_gm_action(action))


def health():
    pawn = unreal.GameplayStatics.get_player_pawn(STATE["world"], 0)
    return pawn.get_component_by_class(unreal.CultivationHealthComponent)


def storm():
    found = actors(STORM_CLASS)
    return found[0] if found else None


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


# ---------------------------------------------------------------- core damage formula on a fresh target


def formula():
    # Far off the platform so no sword picks it as a target.
    target = mode().spawn_test_target(100.0, 10.0, -1, unreal.Vector(0.0, 6000.0, -3000.0))
    if not check("formula target spawned", target is not None):
        return
    rows = []
    for defense, expect in ((10.0, 5.0), (100.0, 4.0), (0.0, 10.0)):
        target.set_editor_property("defense", defense)
        before = target.get_editor_property("health")
        target.apply_weapon_damage(10.0, "formula_test")
        lost = before - target.get_editor_property("health")
        rows.append(dict(attack=10, defense=defense, lost=round(lost, 3), expect=expect))
        check("A=10 vs D=%g removes %g" % (defense, expect), abs(lost - expect) < 1e-3, rows[-1])
    hit = unreal.LiveHit(attack=10.0, skill_coefficient=3.5, armor_penetration=0.9, attacker_realm=8)
    result = unreal.LiveCombatLibrary.resolve_hit(hit, 20.0, 9, 0.0, False)
    # D_eff = 20 × (1 − 0.6) = 8, kept 10/18, ×3.5, realm 练气九阶 → 筑基初期 one major realm apart ×0.8.
    check("library: 破甲 cap, 招式系数 and realm suppression", abs(result.damage - 10.0 * 10.0 / 18.0 * 3.5 * 0.8) < 1e-3,
          dict(damage=round(result.damage, 4), realm=result.realm_factor, defense=round(result.effective_defense, 3)))
    target.destroy_actor()


def sheet(name, kind):
    sword = mode().find_special_sword(kind)
    if not sword:
        return check(name + " sheet", False, "sword missing")
    crit, crit_growth, crit_damage, crit_damage_growth, pen, pen_growth, interval = SHEET[name]
    realm = sword.get_realm()
    actual = dict(realm=realm, critical=round(sword.get_critical_chance(), 4), critical_damage=round(sword.get_critical_damage(), 4),
                  penetration=round(sword.get_armor_penetration(), 4), interval=round(sword.get_attack_interval(), 3),
                  attack=round(sword.get_attack_power(), 2))
    # The viewer's own 会心 adds on top; the test viewers carry none.
    ok = (abs(actual["critical"] - (crit + crit_growth * realm)) < 1e-3 and abs(actual["critical_damage"] - (crit_damage + crit_damage_growth * realm)) < 1e-3
          and abs(actual["penetration"] - min(0.6, pen + pen_growth * realm)) < 1e-3 and abs(actual["interval"] - interval) < 1e-3)
    REPORT.setdefault("sheets", {})[name] = actual
    check(name + " flies with its sheet", ok, actual)


if not ORTHO:
    step("formula", formula, 0.3)
    step("add SoulEater", lambda: click("SwordRange.SoulEater"), 1.4)
    step("add Godslayer", lambda: click("SwordRange.Godslayer"), 1.6)
    step("SoulEater sheet", lambda: sheet("SoulEater", KIND.SOUL_EATER), 0.2)
    step("Godslayer sheet", lambda: sheet("Godslayer", KIND.GODSLAYER), 0.2)
    step("godslayer stronger", lambda: check("弑神剑 strikes harder than 噬魂剑 at the same realm",
                                             REPORT["sheets"]["Godslayer"]["attack"] > REPORT["sheets"]["SoulEater"]["attack"] * 1.2
                                             or REPORT["sheets"]["Godslayer"]["realm"] != REPORT["sheets"]["SoulEater"]["realm"],
                                             {k: v["attack"] for k, v in REPORT["sheets"].items()}), 2.0)

# ---------------------------------------------------------------- 剑谱


def selection(kind, label):
    return lambda: check("codex shows " + label, panel().get_codex_selection() == kind, str(panel().get_codex_selection()))


if not ORTHO:
    step("open codex", lambda: click("SwordRange.Codex"), 0.8)
    step("codex open", lambda: check("codex open", panel().is_codex_open()), 0.2)
    step("codex default", selection(KIND.SOUL_EATER, "噬魂剑"), 0.2)
    step("codex souleater", lambda: capture("01-codex-souleater"), 0.3)
    step("pick godslayer", lambda: click("SwordCodex.Godslayer"), 0.5)
    step("codex godslayer", selection(KIND.GODSLAYER, "弑神剑"), 0.2)
    step("godslayer capture", lambda: capture("02-codex-godslayer-live"), 0.3)
    for i in range(3):
        step("realm next %d" % i, lambda: click("SwordCodex.RealmNext"), 0.3)
    step("realm 16", lambda: check("realm stepper reaches 金丹一阶", panel().get_codex_realm() == 16, panel().get_codex_realm()), 0.2)
    step("godslayer jindan", lambda: capture("03-codex-godslayer-jindan"), 0.3)
    step("pick normal", lambda: click("SwordCodex.Normal"), 0.5)
    step("codex normal", selection(KIND.NONE, "普通剑灵"), 0.2)
    step("normal capture", lambda: capture("04-codex-normal"), 0.3)
    step("pick sevenstar", lambda: click("SwordCodex.SevenStar"), 0.5)
    step("sevenstar capture", lambda: capture("05-codex-sevenstar"), 0.3)
    step("close codex", lambda: click("SwordCodex.Close"), 0.6)
    step("codex closed", lambda: check("codex closes", not panel().is_codex_open()), 0.2)
    step("landscape", lambda: check("landscape viewport", LIB.set_pie_viewport_size(1280, 720)), 1.5)
    step("open landscape", lambda: click("SwordRange.Codex"), 0.9)
    step("landscape capture", lambda: capture("06-codex-landscape"), 0.3)
    step("close landscape", lambda: click("SwordCodex.Close"), 0.6)
    step("landscape closed", lambda: check("codex closes in landscape", not panel().is_codex_open()), 0.2)
    step("portrait", lambda: check("portrait viewport", LIB.set_pie_viewport_size(720, 1280)), 1.5)
step("dock", lambda: capture("07-dock"), 0.3)

# ---------------------------------------------------------------- tribulation


def begin_tribulation():
    STATE["realm_before"] = mode().get_anchor_realm()
    click("SwordRange.Trib")
    check("tribulation started", mode().is_tribulation_active())


def first_pressure():
    s = storm()
    p = s.get_pressure() if s else None
    check("storm present", s is not None)
    # No assists and no earlier failure: 50%, the full storm: three bolts of 7.5% every 7.5 s × 1.15 in 肉身劫.
    ok = p is not None and abs(p.intensity - 1.0) < 1e-3 and p.bolts == 3 and abs(p.damage_fraction - 0.075) < 1e-4 and abs(p.interval - 8.625) < 1e-3
    check("50% success: 3 bolts, 7.5% each, every 8.6 s", ok, dict(intensity=p.intensity, bolts=p.bolts, damage=p.damage_fraction, interval=p.interval) if p else None)


def watch(tag, volleys):
    """Starts watching bolts: every landed bolt is captured (up to a few) and its health loss recorded."""
    s = storm()
    if not check("storm ready for " + tag, s is not None):
        raise RuntimeError("no storm to watch")
    STATE["watch"] = dict(tag=tag, start=s.get_bolts_fired(), strikes=s.get_strikes(), volleys=volleys, landed=s.get_bolts_landed(),
                          dealt=s.get_damage_dealt(), health=health().get_editor_property("health"))


def volley_done(key, expected_bolts, limit):
    def run():
        s = storm()
        w = STATE["watch"]
        started = STATE.setdefault("since_" + key, time.monotonic())
        if s and s.get_strikes() >= w["strikes"] + w["volleys"] and s.get_bolts_fired() >= w["start"] + expected_bolts * w["volleys"]:
            fired = s.get_bolts_fired() - w["start"]
            landed = s.get_bolts_landed() - w["landed"]
            dealt = s.get_damage_dealt() - w["dealt"]
            top = health().get_editor_property("max_health")
            row = dict(tag=w["tag"], fired=fired, landed=landed, dealt=round(dealt, 2), max_health=round(top, 1),
                       per_bolt=round(dealt / max(landed, 1) / top, 4), pressure=dict(bolts=s.get_pressure().bolts, damage=s.get_pressure().damage_fraction))
            REPORT["strikes"].append(row)
            STATE["watch"] = None
            return check(key, fired == expected_bolts * w["volleys"] and landed == fired, row)
        if time.monotonic() - started > limit:
            STATE["watch"] = None
            return check(key, False, dict(fired=(s.get_bolts_fired() - w["start"]) if s else None))
        return 0.1
    return run


def assist_five():
    for _ in range(5):
        panel().click_test_target("SwordRange.Trib")
    s = storm()
    p = s.get_pressure() if s else None
    REPORT["assisted"] = dict(intensity=p.intensity, bolts=p.bolts, damage=p.damage_fraction, interval=p.interval) if p else None


def calm_pressure():
    s = storm()
    p = s.get_pressure() if s else None
    check("100% success after 50 assists: 1 bolt of 5%, calm storm", p is not None and p.intensity < 1e-3 and p.bolts == 1 and abs(p.damage_fraction - 0.05) < 1e-4,
          dict(intensity=p.intensity, bolts=p.bolts, damage=p.damage_fraction, interval=p.interval) if p else None)


def succeeded():
    return "渡劫成功" in str(mode().get_tribulation_status())


step("start tribulation", begin_tribulation, 0.5)
step("prepared realm", lambda: check("rehearsal puts the anchor at 金丹六阶", mode().get_anchor_realm() == 21, mode().get_anchor_realm()), 3.5)
step("gathering", lambda: capture("08-trib-gather-early"), 7.5)
step("gathered", lambda: capture("09-trib-gather-late"), 0.2)
step("trial begins", wait_until("trial", lambda: storm() is not None and storm().get_next_strike_in() >= 0.0, 12, "trial begins after 15 s"), 0.1)
step("first pressure", first_pressure, 0.1)
step("watch first", lambda: watch("50%", 1), 0.0)
step("first volley", volley_done("first strike: 3 bolts land", 3, 6.0), 0.4)
step("after strike", lambda: capture("11-trib-after-strike"), 0.2)
step("watch second", lambda: watch("50% again", 1), 0.0)
step("second volley", volley_done("second strike: 3 bolts land", 3, 14.0), 0.6)
step("status", lambda: (REPORT.__setitem__("status_full", str(mode().get_tribulation_status())), capture("13-trib-status")), 0.2)
step("assist", assist_five, 0.6)
step("calm", calm_pressure, 0.2)
step("watch calm", lambda: watch("100%", 1), 0.0)
step("calm volley", volley_done("calm strike: 1 bolt lands", 1, 22.0), 0.5)
step("calm capture", lambda: capture("15-trib-calm"), 0.2)
step("speed up", lambda: gm("XT_Scale5"), 0.2)
step("success", wait_until("success", succeeded, 30, "100% tribulation succeeds"), 0.8)
step("success capture", lambda: capture("16-trib-success"), 1.2)
step("success later", lambda: capture("17-trib-parting"), 0.2)
step("normal time", lambda: gm("XT_Scale1"), 0.2)
step("storm gone", wait_until("gone", lambda: storm() is None, 20, "storm fades and removes itself"), 1.5)
step("floor", lambda: check("bolts never take the anchor below 10% or down", STATE["min_health"] >= 0.1 - 1e-3 and not STATE["died"],
                            dict(min_fraction=round(STATE["min_health"], 4), died=STATE["died"])), 0.2)
step("realm restored", lambda: check("rehearsal returns the anchor to its realm", mode().get_anchor_realm() == STATE["realm_before"],
                                     dict(before=STATE["realm_before"], after=mode().get_anchor_realm())), 0.2)
step("after", lambda: capture("18-after"), 0.2)


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


def sample():
    """Every frame: the anchor's lowest health during the tribulation, and a capture of the first bolts as they land."""
    h = health()
    if h and storm():
        top = max(h.get_editor_property("max_health"), 1.0)
        STATE["min_health"] = min(STATE["min_health"], h.get_editor_property("health") / top)
        STATE["died"] |= bool(h.get_editor_property("dead"))
    w = STATE["watch"]
    s = storm()
    if w and s and STATE["bolt_captures"] < 6 and s.get_bolts_fired() > w["start"] + w.get("seen", 0):
        w["seen"] = s.get_bolts_fired() - w["start"]
        STATE["bolt_captures"] += 1
        capture("10-trib-bolt-%d" % STATE["bolt_captures"])


def tick(_):
    if STATE["mode"]:
        try:
            sample()
        except Exception:
            REPORT.setdefault("sample_error", traceback.format_exc())
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
unreal.log("Codex and tribulation verification writing to " + str(OUT))

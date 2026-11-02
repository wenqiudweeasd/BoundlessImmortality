"""Rendered PIE check of the audience swords, the five special swords and the 仙途 pages, driven by GM actions and Slate clicks.

Run inside the editor after build_special_sword_assets.py:
  UnrealEditor.exe BoundlessImmortality.uproject -unattended -noxgecontroller -ExecutePythonScript=Tools/SpecialSwords/verify_special_swords_pie.py
Captures and report.json land in Saved/SpecialSwords/Verification/<time>/. Local simulation only; no SDK.
"""
import json
import time
import traceback
from datetime import datetime
from pathlib import Path

import unreal

ROOT = Path(unreal.Paths.project_dir())
OUT = ROOT / "Saved/SpecialSwords/Verification" / datetime.now().strftime("%Y%m%dT%H%M%S")
OUT.mkdir(parents=True, exist_ok=True)
LIB = unreal.InkFrontendEditorLibrary
REPORT = dict(checks=[], captures=[], finished=False, boundary="Windows editor PIE, local simulation, no SDK, no device run")
STATE = dict(step=-2, next=0.0, director=None, world=None, gm=None)
START = time.monotonic()
unreal.EditorPythonScripting.set_keep_python_script_alive(True)


def save():
    REPORT["passed"] = REPORT["finished"] and not REPORT.get("error") and all(c["passed"] for c in REPORT["checks"])
    (OUT / "report.json").write_text(json.dumps(REPORT, ensure_ascii=False, indent=2), "utf-8")


def check(name, ok, actual=None):
    REPORT["checks"].append(dict(name=name, passed=bool(ok), actual=actual))
    save()


def ui():
    return STATE["director"].get_editor_property("game_ui")


def describe():
    return json.loads(ui().describe_ui())


def capture(tag):
    path = OUT / (tag + ".png")
    check("capture " + tag, LIB.capture_frontend_viewport(str(path)))
    (OUT / (tag + ".json")).write_text(json.dumps(describe(), ensure_ascii=False, indent=2), "utf-8")
    REPORT["captures"].append(tag)


def click(target):
    check("slate click " + target, ui().click_test_target(target), describe().get("top"))


def expect_top(route):
    top = describe().get("top")
    check("top is " + route, top == route, top)


def tab(bar, index):
    check("tab %s/%d" % (bar, index), ui().select_tab(bar, index))


def back():
    check("back", ui().press_back(), describe().get("top"))


def gm(action):
    if not STATE["gm"]:
        overlays = unreal.GameplayStatics.get_all_actors_of_class(STATE["world"], unreal.LiveGMOverlay)
        STATE["gm"] = overlays[0] if overlays else None
    check("gm " + action, STATE["gm"] is not None and STATE["gm"].execute_gm_action(action))


def count(path):
    # By path: the ELiveSpecialSword enum and the ALiveSpecialSword class share one Python name.
    cls = unreal.load_class(None, path)
    return len(unreal.GameplayStatics.get_all_actors_of_class(STATE["world"], cls)) if cls else -1


STEPS = []


def step(name, fn, delay=0.8):
    STEPS.append((name, fn, delay))


step("portrait size", lambda: LIB.set_pie_viewport_size(720, 1280), 2.5)
step("start game", lambda: click("Main.Start"), 6.0)
step("hud", lambda: (capture("01-hud"), expect_top("Hud")), 0.3)
# Audience swords draw a random GanzSe mesh per viewer.
step("local audience", lambda: gm("EnableTest"), 1.0)
step("ten viewers", lambda: gm("Add10"), 4.0)
step("audience swords", lambda: capture("02-audience-random-meshes"), 0.3)
# Five gifts, five special forms; monsters come from the ambient encounters and the world trial.
step("five special swords", lambda: gm("SS_All"), 2.0)
SWORD_CLASS = "/Script/BoundlessImmortality.LiveSpecialSword"
MONSTER_CLASS = "/Script/BoundlessImmortality.LiveMonster"
step("special sword actors", lambda: check("five special swords spawned", count(SWORD_CLASS) >= 5, count(SWORD_CLASS)), 1.0)
step("special swords idle", lambda: capture("03-special-swords"), 0.3)
step("monsters", lambda: gm("SpawnMonsters"), 1.0)
step("monsters present", lambda: check("monsters spawned", count(MONSTER_CLASS) >= 12, count(MONSTER_CLASS)), 2.0)
# Skills come off cooldown at staggered times; a burst of captures catches casts, trails and hits.
for i in range(8):
    step("combat %d" % i, lambda i=i: capture("04-combat-%d" % i), 2.2)
step("debuff", lambda: gm("SS_Debuff"), 0.3)
step("second debuff", lambda: gm("SS_Debuff"), 0.3)
step("lose a seventh", lambda: gm("SS_HitSeventh"), 1.5)
step("hud buffs and shield", lambda: capture("06-hud-buffs-shield"), 0.3)
step("low health monsters", lambda: gm("SS_LowHealth"), 1.2)
step("devour", lambda: capture("07-devour"), 2.0)
step("devour later", lambda: (capture("07b-devour-later"), check("monsters after devour", True, count(MONSTER_CLASS))), 0.3)
# 仙途 pages.
step("grant bundle", lambda: gm("XT_Bundle"), 0.3)
step("open journey", lambda: click("Hud.Journey"), 1.0)
step("journey", lambda: (capture("08-journey"), expect_top("Journey")), 0.3)
step("open sword book", lambda: click("Journey.SwordSpirit"), 2.2)
step("sword book", lambda: (capture("09-sword-book"), expect_top("SwordSpirit")), 0.3)
step("special tab", lambda: tab("Book.Tabs", 1), 0.8)
step("sword book special", lambda: capture("09b-sword-book-special"), 0.3)
step("gift tab", lambda: tab("Book.Tabs", 2), 0.8)
step("sword book gifts", lambda: capture("09c-sword-book-gifts"), 0.3)
step("close sword book", back, 0.8)
step("open world", lambda: click("Journey.World"), 1.0)
step("world", lambda: (capture("10-world"), expect_top("World")), 0.3)
step("fast clock", lambda: gm("XT_Scale10"), 0.3)
step("start trial", lambda: click("World.Start"), 7.5)
step("world live", lambda: capture("10b-world-live"), 0.3)
step("close world", back, 0.8)
step("open resources", lambda: click("Journey.Resources"), 1.0)
step("resources", lambda: (capture("11-resources"), expect_top("Resources")), 0.3)
step("lease tab", lambda: tab("Race.Tabs", 1), 0.8)
step("resources lease", lambda: capture("11b-resources-lease"), 0.3)
step("close resources", back, 0.8)
step("open wuling", lambda: click("Journey.Wuling"), 1.0)
step("wuling", lambda: (capture("12-wuling"), expect_top("Wuling")), 0.3)
step("close wuling", back, 0.8)
step("open market", lambda: click("Journey.Market"), 1.0)
step("market", lambda: (capture("13-market"), expect_top("Market")), 0.3)
step("listing tab", lambda: tab("Market.Tabs", 1), 0.8)
step("market listing", lambda: capture("13b-market-listing"), 0.3)
step("warehouse tab", lambda: tab("Market.Tabs", 2), 0.8)
step("market warehouse", lambda: capture("13c-market-warehouse"), 0.3)
step("close market", back, 0.8)
# The last two journey cards sit below the fold of the portrait grid; open their routes directly.
step("open methods", lambda: check("open Methods", ui().open_route("Methods")), 1.0)
step("methods", lambda: (capture("14-methods"), expect_top("Methods")), 0.3)
step("main method", lambda: click("Methods.Main.METHOD_EDGE"), 0.8)
step("methods equipped", lambda: capture("14b-methods-equipped"), 0.3)
step("close methods", back, 0.8)
step("prepare tribulation", lambda: gm("XT_Trib"), 0.5)
step("open tribulation", lambda: check("open Tribulation", ui().open_route("Tribulation")), 1.0)
step("tribulation", lambda: (capture("15-tribulation"), expect_top("Tribulation")), 0.3)
step("start tribulation", lambda: click("Trib.Start"), 0.6)
step("confirm tribulation", lambda: click("Modal.Confirm"), 1.5)
step("assists", lambda: gm("XT_Assist"), 1.5)
step("tribulation live", lambda: capture("15b-tribulation-live"), 0.3)
step("close tribulation", back, 0.8)
step("close journey", back, 1.0)
step("world trial in scene", lambda: (capture("16-world-trial-scene"), check("trial monsters", True, count(MONSTER_CLASS))), 0.3)
step("normal clock", lambda: gm("XT_Scale1"), 0.3)


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
    if time.monotonic() < STATE["next"]:
        return
    try:
        if time.monotonic() - START > 360:
            raise TimeoutError("verification exceeded 360s")
        if STATE["step"] == -2:
            check("begin PIE", LIB.begin_frontend_pie(720, 1280))
            STATE["step"] = -1
            STATE["next"] = time.monotonic() + 10
            return
        if STATE["step"] == -1:
            worlds = unreal.EditorLevelLibrary.get_pie_worlds(False)
            directors = unreal.GameplayStatics.get_all_actors_of_class(worlds[0], unreal.InkMenuDirector) if worlds else []
            if not directors or not directors[0].get_editor_property("game_ui"):
                STATE["next"] = time.monotonic() + 1
                return
            STATE["world"], STATE["director"] = worlds[0], directors[0]
            STATE["step"] = 0
            STATE["next"] = time.monotonic() + 3.0
            return
        if STATE["step"] >= len(STEPS):
            REPORT["finished"] = True
            save()
            finish()
            return
        name, fn, delay = STEPS[STATE["step"]]
        REPORT["current_step"] = name
        fn()
        STATE["step"] += 1
        STATE["next"] = time.monotonic() + delay
        save()
    except Exception:
        REPORT["error"] = traceback.format_exc()
        save()
        finish()


HANDLE = [unreal.register_slate_post_tick_callback(tick)]
QUIT = [None]
unreal.log("Special sword verification writing to " + str(OUT))

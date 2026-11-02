"""Local verification of the Slate xianxia UI: rendered PIE captures driven by real Slate clicks.

Run inside the editor:
  UnrealEditor.exe BoundlessImmortality.uproject -unattended -ExecutePythonScript=Tools/InkFrontend/verify_ink_slate_ui_pie.py
No live-service SDK is involved; all service-backed panels show local verification data.
"""
import json
import time
import traceback
from datetime import datetime
from pathlib import Path

import unreal

ROOT = Path(unreal.Paths.project_dir())
OUT = ROOT / "Saved/InkUI/Verification" / datetime.now().strftime("%Y%m%dT%H%M%S")
OUT.mkdir(parents=True, exist_ok=True)
LIB = unreal.InkFrontendEditorLibrary
REPORT = dict(checks=[], captures=[], finished=False, boundary="Windows editor PIE, local data only, no SDK, no device run")
STATE = dict(step=-2, next=0.0, director=None, world=None)
START = time.monotonic()
# -ExecutePythonScript closes the editor on the tick after this file returns unless the script stays alive.
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
    state = describe()
    (OUT / (tag + ".json")).write_text(json.dumps(state, ensure_ascii=False, indent=2), "utf-8")
    REPORT["captures"].append(tag)
    return state


def click(target):
    ok = ui().click_test_target(target)
    check("slate click " + target, ok, describe().get("top"))


def expect_top(route):
    top = describe().get("top")
    check("top is " + route, top == route, top)


def tab(bar, index):
    check("tab %s/%d" % (bar, index), ui().select_tab(bar, index))


def back():
    check("back", ui().press_back(), describe().get("top"))


def resize(width, height):
    check("resize %dx%d" % (width, height), LIB.set_pie_viewport_size(width, height))


STEPS = []


def step(name, fn, delay=0.8):
    STEPS.append((name, fn, delay))


# Portrait main menu and its two system pages.
step("portrait size", lambda: resize(720, 1280), 2.5)
step("main menu settled", lambda: capture("01-main-menu"), 0.2)
step("open settings", lambda: click("Main.Settings"), 0.14)
step("settings opening frame", lambda: capture("02a-settings-opening"), 0.9)
step("settings", lambda: (capture("02-settings"), expect_top("Settings")), 0.3)
step("close settings", back, 0.5)
step("open saves", lambda: click("Main.Saves"), 0.9)
step("saves", lambda: capture("03-saves"), 0.3)
step("close saves", back, 0.6)
step("start game", lambda: click("Main.Start"), 5.0)
step("hud", lambda: (capture("04-hud"), expect_top("Hud")), 0.3)
# Sign-in: open, claim, ceremony, stamp.
step("open sign-in", lambda: click("Hud.SignIn"), 0.12)
step("sign-in opening frame", lambda: capture("05a-signin-opening"), 0.9)
step("sign-in", lambda: (capture("05-signin"), expect_top("SignIn")), 0.3)
step("claim", lambda: click("SignIn.Claim"), 0.35)
step("reward early", lambda: capture("05b-reward-early"), 0.9)
step("reward", lambda: capture("05c-reward"), 0.3)
step("dismiss reward", back, 0.25)
step("stamp", lambda: capture("05d-signin-stamped"), 0.6)
step("close sign-in", back, 0.6)
# Activities with a claim.
step("open activities", lambda: click("Hud.Activities"), 0.9)
step("activities", lambda: (capture("06-activities"), expect_top("Activities")), 0.3)
step("timed tab", lambda: tab("Act.Tabs", 1), 0.7)
step("activities timed", lambda: capture("06b-activities-timed"), 0.3)
step("close activities", back, 0.6)
step("open journey", lambda: click("Hud.Journey"), 0.9)
step("journey", lambda: (capture("07-journey"), expect_top("Journey")), 0.3)
step("close journey", back, 0.6)
# Character tabs.
step("open character", lambda: click("Hud.Character"), 0.9)
step("character", lambda: (capture("08-character"), expect_top("Character")), 0.3)
step("gear tab", lambda: tab("Character.Tabs", 1), 0.8)
step("character gear", lambda: capture("08b-character-gear"), 0.3)
step("realm tab", lambda: tab("Character.Tabs", 2), 0.8)
step("character realm", lambda: capture("08c-character-realm"), 0.3)
step("close character", back, 0.6)
# Inventory and the detail sheet.
step("open inventory", lambda: click("Hud.Inventory"), 0.9)
step("inventory", lambda: (capture("09-inventory"), expect_top("Inventory")), 0.3)
step("select item", lambda: click("Bag.Slot2"), 0.6)
step("inventory detail", lambda: capture("09b-inventory-detail"), 0.3)
step("close detail", back, 0.5)
step("close inventory", back, 0.6)
step("open leaderboard", lambda: click("Hud.Leaderboard"), 1.0)
step("leaderboard", lambda: (capture("10-leaderboard"), expect_top("Leaderboard")), 0.3)
step("close leaderboard", back, 0.6)
# HUD states: menu popover and cultivation.
step("menu", lambda: click("Hud.Menu"), 0.6)
step("menu popover", lambda: capture("11-hud-menu"), 0.3)
step("close menu", back, 0.5)
step("cultivate", lambda: click("Hud.Cultivate"), 2.2)
step("cultivating", lambda: capture("12-hud-cultivating"), 0.3)
step("stop cultivating", lambda: click("Hud.Cultivate"), 1.0)
# Landscape reflow.
step("landscape size", lambda: resize(1280, 720), 2.5)
step("hud landscape", lambda: capture("13-hud-landscape"), 0.3)
step("inventory landscape open", lambda: click("Hud.Inventory"), 1.0)
step("inventory landscape", lambda: capture("14-inventory-landscape"), 0.3)
step("close landscape inventory", back, 0.6)
step("signin landscape open", lambda: click("Hud.SignIn"), 1.0)
step("signin landscape", lambda: capture("15-signin-landscape"), 0.3)
step("close signin landscape", back, 0.6)
step("portrait restore", lambda: resize(720, 1280), 2.0)
step("return to menu", lambda: click("Hud.Menu"), 0.6)
step("menu main", lambda: click("Hud.Menu.Main"), 0.6)
step("confirm modal", lambda: capture("16-confirm-modal"), 0.3)
step("confirm", lambda: click("Modal.Confirm"), 3.0)
step("main menu again", lambda: (capture("17-main-menu-return"), expect_top("MainMenu")), 0.3)


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
        if time.monotonic() - START > 420:
            raise TimeoutError("verification exceeded 420s")
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
unreal.log("InkUI verification writing to " + str(OUT))

"""Local verification of the 万剑仙宗 hub, driven by real Slate clicks in a 720x1280 PIE session.

Run inside the editor. The run plays on the account in Saved/WanJian, so move that file away first for a clean start:
  UnrealEditor.exe BoundlessImmortality.uproject -unattended -noxgecontroller -ExecutePythonScript=<absolute path of this file>
Captures and report.json land in Saved/InkUI/WanJian/<time>/. No WeChat host, server, ad network or device is involved.
"""
import json
import time
import traceback
from datetime import datetime
from pathlib import Path

import unreal

ROOT = Path(unreal.Paths.project_dir())
OUT = ROOT / "Saved/InkUI/WanJian" / datetime.now().strftime("%Y%m%dT%H%M%S")
OUT.mkdir(parents=True, exist_ok=True)
ACCOUNT = ROOT / "Saved/WanJian/wanjian-local.json"
LIB = unreal.InkFrontendEditorLibrary
REPORT = dict(checks=[], captures=[], finished=False, boundary="Windows editor PIE, local account only, no WeChat host, no server, no device")
STATE = dict(step=-2, next=0.0, director=None, world=None)
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
    REPORT["captures"].append(tag)


def click(target):
    check("click " + target, ui().click_test_target(target), describe().get("top"))


def reveal(target):
    # The scroll takes effect on a later frame, so the click goes in a following step.
    check("reveal " + target, ui().reveal_test_target(target))


def nav(index):
    check("nav %d" % index, ui().select_tab("WanJian.Nav", index))


def expect_top(route):
    top = describe().get("top")
    check("top is " + route, top == route, top)


def account():
    return json.loads(ACCOUNT.read_text("utf-8"))


def balance(name):
    wallet = account().get("wallet", {})
    return int(wallet.get(name, wallet.get(name[0].lower() + name[1:], 0)))


def check_balance(name, asset, want):
    got = balance(asset)
    check(name, got == want, got)


STEPS = []


def step(name, fn, delay=0.8):
    STEPS.append((name, fn, delay))


step("portrait", lambda: check("resize", LIB.set_pie_viewport_size(720, 1280)), 2.5)
step("start game", lambda: click("Main.Start"), 6.0)
step("hud", lambda: (capture("w01-hud"), expect_top("Hud")), 0.3)
step("open hub", lambda: click("Hud.WanJian"), 1.2)
step("world tab", lambda: (capture("w02-world"), expect_top("WanJian")), 0.3)
step("pick stage", lambda: click("WanJian.Stage.1001"), 0.6)
step("stage detail", lambda: capture("w03-stage-detail"), 0.3)
step("reveal start battle", lambda: reveal("WanJian.Start"), 0.5)
step("start battle", lambda: click("WanJian.Start"), 3.5)
step("battle early", lambda: capture("w04-battle-early"), 0.3)
step("reveal speed 2", lambda: reveal("WanJian.Speed"), 0.5)
step("speed 2", lambda: click("WanJian.Speed"), 0.2)
step("speed 4", lambda: click("WanJian.Speed"), 7.0)
step("battle middle", lambda: capture("w05-battle-mid"), 9.0)
step("settlement", lambda: (capture("w06-settlement"), check_balance("first clear pays one ticket", "SwordTicket", 1)), 0.3)
step("reveal leave battle", lambda: reveal("WanJian.Leave"), 0.5)
step("leave battle", lambda: click("WanJian.Leave"), 0.7)
step("summon tab", lambda: nav(3), 0.8)
step("summon", lambda: capture("w07-summon"), 0.3)
step("reveal tutorial tickets", lambda: reveal("WanJian.Claim.Tutorial"), 0.5)
step("tutorial tickets", lambda: click("WanJian.Claim.Tutorial"), 0.6)
step("tickets granted", lambda: check_balance("tutorial pays ten tickets", "SwordTicket", 11), 0.2)
step("scroll to draw", lambda: reveal("WanJian.DrawTen"), 0.5)
step("draw ten", lambda: click("WanJian.DrawTen"), 1.0)
step("draw result", lambda: (capture("w08-draw-result"), check_balance("ten tickets charged", "SwordTicket", 1)), 0.3)
step("pity counted", lambda: check("pity counts ten draws", account()["pity"]["draws"] == 10, account()["pity"]), 0.2)
step("lineup tab", lambda: nav(2), 0.8)
step("lineup", lambda: capture("w09-lineup"), 0.3)
step("auto fill", lambda: click("WanJian.AutoFill"), 0.8)
step("lineup filled", lambda: (capture("w10-lineup-filled"), check("two swords equipped", sum(1 for s in account()["lineup"] if s) == 2, account()["lineup"])), 0.3)
step("sect tab", lambda: nav(4), 0.8)
step("sect", lambda: capture("w11-sect"), 0.3)
step("cultivation tab", lambda: nav(5), 0.8)
step("choose root", lambda: click("WanJian.Root.1"), 0.6)
step("cultivation", lambda: (capture("w12-cultivation"), check("metal root stored", account()["root"] == "Metal", account()["root"])), 0.3)
step("dungeon tab", lambda: nav(1), 0.8)
step("dungeons", lambda: capture("w13-dungeons"), 0.3)
step("world tab again", lambda: nav(0), 0.8)
step("next stage open", lambda: click("WanJian.Stage.1002"), 0.5)
step("reveal second battle", lambda: reveal("WanJian.Start"), 0.5)
step("second battle", lambda: click("WanJian.Start"), 2.5)
step("two sword battle", lambda: capture("w14-two-swords"), 0.3)
step("reveal retreat", lambda: reveal("WanJian.Retreat"), 0.5)
step("retreat", lambda: click("WanJian.Retreat"), 0.8)
step("still on hub", lambda: expect_top("WanJian"), 0.3)
step("landscape", lambda: check("resize landscape", LIB.set_pie_viewport_size(1280, 720)), 2.5)
step("landscape world", lambda: capture("w15-landscape"), 0.3)


def finish():
    unreal.unregister_slate_post_tick_callback(HANDLE[0])
    unreal.EditorLevelLibrary.editor_end_play()

    def quit_tick(_):
        unreal.unregister_slate_post_tick_callback(QUIT[0])
        unreal.SystemLibrary.quit_editor()

    QUIT[0] = unreal.register_slate_post_tick_callback(quit_tick)


def tick(_):
    try:
        if time.monotonic() < STATE["next"]:
            return
        if time.monotonic() - START > 300:
            raise RuntimeError("timed out at step %s" % STATE["step"])
        if STATE["step"] == -2:
            check("begin PIE", LIB.begin_frontend_pie(720, 1280))
            STATE["step"] = -1
            STATE["next"] = time.monotonic() + 8
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
unreal.log("WanJian verification writing to " + str(OUT))

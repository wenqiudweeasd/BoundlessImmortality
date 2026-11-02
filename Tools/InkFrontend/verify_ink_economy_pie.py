"""Local verification of the 仙途 item economy UI: 每日一签, 灵玉商城, 境界突破 (with its Niagara breakthroughs),
仙道殿, 武灵建设 and 武灵商城, 物品来源, 装备法宝, the 天劫 hand-off and the bag wallet, driven by real Slate clicks.

Run inside the editor (back up Saved/XianTu and Saved/CultivationPrototype first: the run spends and grants local data):
  UnrealEditor.exe BoundlessImmortality.uproject -unattended -noxgecontroller -ExecutePythonScript=Tools/InkFrontend/verify_ink_economy_pie.py
GM rehearsal actions (ALiveGMOverlay) place the anchor at a realm, fill the ledger and force the day's fortune.
Captures and report.json land in Saved/InkUI/Economy/<time>/. No live-service SDK is involved.
"""
import json
import time
import traceback
from datetime import datetime
from pathlib import Path

import unreal

ROOT = Path(unreal.Paths.project_dir())
OUT = ROOT / "Saved/InkUI/Economy" / datetime.now().strftime("%Y%m%dT%H%M%S")
OUT.mkdir(parents=True, exist_ok=True)
LIB = unreal.InkFrontendEditorLibrary
REPORT = dict(checks=[], captures=[], finished=False, boundary="Windows editor PIE, local ledger only, no SDK, no payment, no device run")
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


def reveal(target):
    check("reveal " + target, ui().reveal_test_target(target))


def back():
    check("back", ui().press_back(), describe().get("top"))


def resize(width, height):
    check("resize %dx%d" % (width, height), LIB.set_pie_viewport_size(width, height))


def gm(action):
    if not STATE["gm"]:
        overlays = unreal.GameplayStatics.get_all_actors_of_class(STATE["world"], unreal.LiveGMOverlay)
        STATE["gm"] = overlays[0] if overlays else None
    check("gm " + action, STATE["gm"] is not None and STATE["gm"].execute_gm_action(action))


def realm():
    return ui().get_player_realm_index()


def realm_check(name, want):
    got = realm()
    check("realm " + name, got == want, got)


STEPS = []


def step(name, fn, delay=0.8):
    STEPS.append((name, fn, delay))


step("portrait size", lambda: resize(720, 1280), 2.5)
step("start game", lambda: click("Main.Start"), 6.0)
step("realm 8 (练气九阶, BR01 ahead)", lambda: gm("XT_Realm8"), 0.5)
step("fortune reset", lambda: gm("XT_FortuneReset"), 0.2)
step("ledger bundle", lambda: gm("XT_Bundle"), 1.5)
step("hud", lambda: (capture("e01-hud"), expect_top("Hud")), 0.3)
# 每日一签: forced 上上签, the shake and the reveal, the HUD glyph; then a forced 凶.
step("force 上上签", lambda: gm("XT_Fortune1"), 0.2)
step("open fortune", lambda: click("Hud.Fortune"), 1.0)
step("fortune idle", lambda: (capture("e02-fortune-idle"), expect_top("Fortune")), 0.3)
step("draw", lambda: click("Fortune.Draw"), 0.55)
step("fortune shaking", lambda: capture("e02b-fortune-shaking"), 1.9)
step("fortune great", lambda: capture("e02c-fortune-great"), 0.3)
step("close fortune", back, 0.8)
step("hud fortune glyph", lambda: capture("e03-hud-fortune"), 0.3)
step("fortune reset 2", lambda: gm("XT_FortuneReset"), 0.2)
step("force 凶", lambda: gm("XT_Fortune5"), 0.2)
step("open fortune 2", lambda: click("Hud.Fortune"), 1.0)
step("draw 2", lambda: click("Fortune.Draw"), 2.7)
step("fortune ominous", lambda: capture("e02d-fortune-ominous"), 0.3)
step("close fortune 2", back, 0.8)
# 灵玉商城 in 仙途.
step("open journey", lambda: click("Hud.Journey"), 1.0)
step("journey", lambda: capture("e04-journey"), 0.3)
step("open jade shop", lambda: click("Journey.JadeShop"), 1.0)
step("jade shop", lambda: (capture("e05-jadeshop"), expect_top("JadeShop")), 0.3)
step("buy core", lambda: click("Shop.Buy.JADE_BR01_C"), 0.6)
step("buy confirm", lambda: capture("e05b-jadeshop-confirm"), 0.3)
step("confirm buy", lambda: click("Modal.Confirm"), 1.3)
step("bought", lambda: capture("e05c-jadeshop-bought"), 0.3)
step("dismiss reward", back, 0.7)
step("props tab", lambda: tab("JadeShop.Tabs", 1), 0.8)
step("jade props", lambda: capture("e05d-jadeshop-props"), 0.3)
step("close jade shop", back, 0.7)
# 境界突破: BR01 major with laid-out materials, the celebration, the quality record.
step("open breakthrough", lambda: click("Journey.Breakthrough"), 1.0)
step("breakthrough", lambda: (capture("e06-breakthrough"), expect_top("Breakthrough")), 0.3)
step("auto fill", lambda: click("Break.Auto"), 0.8)
step("breakthrough plan", lambda: capture("e06b-breakthrough-plan"), 0.3)
step("refine tab", lambda: tab("Break.Tabs", 1), 0.8)
step("refine", lambda: capture("e06c-breakthrough-refine"), 0.3)
step("break tab", lambda: tab("Break.Tabs", 0), 0.8)
step("commit", lambda: (realm_check("before commit", 8), click("Break.Commit")), 0.7)
step("celebration", lambda: capture("e06d-celebration"), 1.1)
step("celebration late", lambda: (capture("e06e-celebration-late"), realm_check("after major", 9)), 2.2)
step("quality tab", lambda: tab("Break.Tabs", 2), 0.8)
step("quality", lambda: capture("e06f-quality"), 0.3)
step("close breakthrough", back, 0.7)
step("close journey", back, 0.9)
# Niagara breakthroughs on the anchor, minor to 混元无极 (the realm itself does not move).
for tag, action, first, second in (("minor", "XT_Fx.3.4", 0.5, 0.6), ("major-zhuji", "XT_Fx.8.9", 0.6, 0.8),
                                   ("major-jindan", "XT_Fx.15.16", 0.6, 0.9), ("ascend-yuanying", "XT_Fx.21.22", 0.7, 1.0),
                                   ("ascend-dacheng", "XT_Fx.30.31", 0.8, 1.0), ("ascend-hunyuan", "XT_Fx.32.33", 0.8, 1.0)):
    step("fx " + tag, lambda a=action: gm(a), first)
    step("fx %s early" % tag, lambda t=tag: capture("e07-fx-%s-a" % t), second)
    step("fx %s late" % tag, lambda t=tag: capture("e07-fx-%s-b" % t), 2.6)
# 仙道殿 and the 金元商城.
step("open hall", lambda: click("Hud.XianDao"), 1.0)
step("hall", lambda: (capture("e08-hall"), expect_top("XianDao")), 0.3)
step("preview sword", lambda: click("Hall.Card.COS_SWORD_02"), 0.8)
step("hall preview", lambda: capture("e08b-hall-preview"), 0.3)
step("reveal frame", lambda: reveal("Hall.Buy.COS_FRAME_01"), 0.5)
step("buy frame", lambda: click("Hall.Buy.COS_FRAME_01"), 0.6)
step("confirm frame", lambda: click("Modal.Confirm"), 1.3)
step("hall bought", lambda: capture("e08c-hall-bought"), 0.3)
step("dismiss frame reward", back, 0.7)
step("wardrobe", lambda: tab("Hall.Tabs", 1), 0.8)
step("reveal wear", lambda: reveal("Hall.Wear.COS_FRAME_01"), 0.5)
step("wear frame", lambda: click("Hall.Wear.COS_FRAME_01"), 0.8)
step("hall wardrobe", lambda: capture("e08d-hall-wardrobe"), 0.3)
step("close hall", back, 0.7)
# 武灵山门 → 武灵建设 → 武灵商城.
step("guild", lambda: gm("XT_Guild"), 0.5)
step("vault bundle", lambda: gm("XT_Bundle"), 1.2)
step("open journey 2", lambda: click("Hud.Journey"), 1.0)
step("open guild", lambda: click("Journey.Wuling"), 1.0)
step("guild page", lambda: capture("e09-guild"), 0.3)
step("open construction", lambda: click("Guild.Construction"), 1.0)
step("construction", lambda: (capture("e09b-construction"), expect_top("GuildConstruction")), 0.3)
step("build up", lambda: click("Build.Up.BUILD_01"), 0.8)
step("construction built", lambda: capture("e09c-construction-built"), 0.3)
step("open wuling shop", lambda: click("Build.Shop"), 1.0)
step("wuling shop", lambda: (capture("e09d-wulingshop"), expect_top("WulingShop")), 0.3)
step("close wuling shop", back, 0.7)
step("close construction", back, 0.7)
step("close guild", back, 0.7)
# 物品来源 and 装备法宝.
step("reveal sources", lambda: reveal("Journey.ItemSources"), 0.5)
step("open sources", lambda: click("Journey.ItemSources"), 1.0)
step("sources", lambda: (capture("e10-sources"), expect_top("ItemSources")), 0.3)
step("sources props", lambda: tab("Sources.Tabs", 1), 0.8)
step("sources props capture", lambda: capture("e10b-sources-props"), 0.3)
step("sources currency", lambda: tab("Sources.Tabs", 2), 0.8)
step("sources currency capture", lambda: capture("e10c-sources-currency"), 0.3)
step("close sources", back, 0.7)
step("reveal gear", lambda: reveal("Journey.EquipmentFabao"), 0.5)
step("open gear", lambda: click("Journey.EquipmentFabao"), 1.0)
step("gear", lambda: (capture("e11-gear"), expect_top("EquipmentFabao")), 0.3)
step("reveal robe", lambda: reveal("Gear.Try.EQ_ROBE_T1"), 0.5)
step("try robe", lambda: click("Gear.Try.EQ_ROBE_T1"), 0.8)
step("gear robe", lambda: capture("e11b-gear-robe"), 0.3)
step("fabao tab", lambda: tab("Gear.Tabs", 4), 0.8)
step("gear fabao", lambda: capture("e11c-gear-fabao"), 0.3)
step("close gear", back, 0.7)
# 天劫 hand-off: realm 21 with a BR04 core.
step("tribulation rehearsal", lambda: gm("XT_Trib"), 0.7)
step("reveal tribulation", lambda: reveal("Journey.Tribulation"), 0.5)
step("open tribulation", lambda: click("Journey.Tribulation"), 1.0)
step("acknowledge an old result", lambda: click("Trib.Ack") if "Trib.Ack" in describe().get("ids", []) else None, 0.8)
step("tribulation", lambda: (capture("e12-tribulation"), expect_top("Tribulation")), 0.3)
step("close tribulation", back, 0.7)
step("close journey 2", back, 0.9)
# A ledger prop on the anchor and the bag wallet.
step("use 护心符", lambda: gm("XT_Do_PropUse.PROP_04"), 0.8)
step("hud shield glyph", lambda: capture("e13-hud-prop"), 0.3)
step("open inventory", lambda: click("Hud.Inventory"), 1.0)
step("inventory", lambda: (capture("e14-inventory"), expect_top("Inventory")), 0.3)
step("pills tab", lambda: tab("Bag.Tabs", 3), 0.8)
step("inventory pills", lambda: capture("e14b-inventory-pills"), 0.3)
step("close inventory", back, 0.7)
# Landscape reflow of the new pages.
step("landscape", lambda: resize(1280, 720), 2.5)
step("open hall landscape", lambda: click("Hud.XianDao"), 1.0)
step("hall landscape", lambda: capture("e15-hall-landscape"), 0.3)
step("close hall landscape", back, 0.7)
step("open fortune landscape", lambda: click("Hud.Fortune"), 1.0)
step("fortune landscape", lambda: capture("e16-fortune-landscape"), 0.3)
step("close fortune landscape", back, 0.7)
step("portrait", lambda: resize(720, 1280), 2.0)
step("release fortune", lambda: (gm("XT_Fortune0"), gm("XT_FortuneReset")), 0.3)


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
        if time.monotonic() - START > 600:
            raise TimeoutError("verification exceeded 600s")
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
unreal.log("InkUI economy verification writing to " + str(OUT))

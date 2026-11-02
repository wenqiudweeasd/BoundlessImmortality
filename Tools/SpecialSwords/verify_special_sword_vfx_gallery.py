"""Rendered gallery of the NS_SS_* effects in the PIE scene: the ink-wash systems in the colours of the four
cultivator swords, and the futuristic _Tech systems, Spin and Glyph in the colour of 无机态那.

Run after build_special_sword_assets.py, with the editor closed:
  UnrealEditor.exe BoundlessImmortality.uproject -unattended -noxgecontroller -ExecutePythonScript=Tools/SpecialSwords/verify_special_sword_vfx_gallery.py
One-shot effects play under global time dilation and are captured at fixed effect ages; loops and moving effects
(trail, sword-qi) run at normal speed. Captures and report.json land in Saved/SpecialSwords/VfxGallery/<time>/.
SPECIAL_SWORD_GALLERY_ONLY=01-slash,14-pillar limits the run to scenes with those prefixes.
"""
import json
import math
import os
import time
import traceback
from datetime import datetime
from pathlib import Path

import unreal

ROOT = Path(unreal.Paths.project_dir())
OUT = ROOT / 'Saved/SpecialSwords/VfxGallery' / datetime.now().strftime('%Y%m%dT%H%M%S')
OUT.mkdir(parents=True, exist_ok=True)
LIB = unreal.InkFrontendEditorLibrary
VFX = '/Game/LiveWeapons/SpecialSwords/VFX/'
SLOW = 0.2
ONLY = tuple(p for p in os.environ.get('SPECIAL_SWORD_GALLERY_ONLY', '').split(',') if p)
REPORT = dict(checks=[], captures=[], finished=False, boundary='Windows editor PIE, local scene, no device run')
STATE = dict(step=-2, next=0.0, world=None, director=None, origin=None, right=None, up=None, floor=0.0, t0=0.0)
LIVE = []    # spawned components of the current scene
MOVERS = []  # (component, path(t) -> world location) driven every tick
START = time.monotonic()
unreal.EditorPythonScripting.set_keep_python_script_alive(True)


def colour(r, g, b, scale=1.0):
    return unreal.LinearColor(r * scale, g * scale, b * scale, 1.0)


# LiveSpecialSwords::Color
SOUL, MECHA, DIVINE, STAR, GOD = (colour(0.62, 0.12, 1.0), colour(0.08, 0.85, 1.0), colour(1.0, 0.72, 0.18),
                                  colour(1.0, 0.93, 0.55), colour(1.0, 0.12, 0.08))
HEAL = colour(0.55, 1.0, 0.6)


def save():
    REPORT['passed'] = REPORT['finished'] and not REPORT.get('error') and all(c['passed'] for c in REPORT['checks'])
    (OUT / 'report.json').write_text(json.dumps(REPORT, ensure_ascii=False, indent=2), 'utf-8')


def check(name, ok, actual=None):
    REPORT['checks'].append(dict(name=name, passed=bool(ok), actual=actual))
    save()


def world_time():
    return unreal.GameplayStatics.get_time_seconds(STATE['world'])


def at(right, up, ground=False):
    """Point in the camera plane through the anchor; ground points sit on the floor under it."""
    p = STATE['origin'] + STATE['right'] * right + STATE['up'] * up
    if ground:
        p.z = STATE['floor']
    return p


def toward_screen_right():
    return unreal.MathLibrary.make_rot_from_x(STATE['right'])


def spawn(effect, where, tint, rotation=None, **params):
    system = unreal.load_asset(VFX + effect)
    check('loaded ' + effect, system is not None)
    comp = unreal.NiagaraFunctionLibrary.spawn_system_at_location(
        STATE['world'], system, where, rotation or unreal.Rotator(0.0, 0.0, 0.0), unreal.Vector(1.0, 1.0, 1.0),
        True, False, unreal.NCPoolMethod.NONE, False)
    comp.set_variable_linear_color('User.Color', tint)
    for name, value in params.items():
        if isinstance(value, unreal.Vector):
            comp.set_variable_vec3('User.' + name, value)
        else:
            comp.set_variable_float('User.' + name, float(value))
    comp.activate(True)
    LIVE.append(comp)
    return comp


def clear():
    """Hides and destroys the scene's components so looping effects do not leak into the next scene."""
    for comp in LIVE:
        for name, kill in (('hide', lambda c: c.set_visibility(False, True)), ('deactivate', lambda c: c.deactivate()),
                           ('destroy', lambda c: c.destroy_component(c))):
            try:
                kill(comp)
            except Exception as error:
                REPORT.setdefault('clear_errors', []).append('%s: %s' % (name, str(error)[:160]))
    LIVE.clear()
    MOVERS.clear()


def capture(tag):
    path = OUT / (tag + '.png')
    check('capture ' + tag, LIB.capture_frontend_viewport(str(path)))
    REPORT['captures'].append(tag)


STEPS = []


def step(name, fn, delay=0.0, age=None):
    """Runs fn after `delay` real seconds, or once the current scene has played `age` seconds of world time."""
    STEPS.append((name, fn, delay, age))


def scene(name, setup, ages, slow=True, settle=0.6):
    if ONLY and not name.startswith(ONLY):
        return

    def begin():
        clear()
        unreal.GameplayStatics.set_global_time_dilation(STATE['world'], SLOW if slow else 1.0)
        setup()
        STATE['t0'] = world_time()
    step(name + ' setup', clear, settle)
    step(name + ' spawn', begin, 0.0)
    for age in ages:
        step('%s @%.2f' % (name, age), lambda age=age: capture('%s-%03d' % (name, int(round(age * 100)))), 0.0, age)


# ---------------------------------------------------------------- scenes

# The four ink swords placed in the four quadrants of the portrait frame.
QUADRANTS = ((SOUL, -160, 260), (DIVINE, 150, 270), (STAR, -150, 50), (GOD, 150, 40))


def slash():
    rot = toward_screen_right()
    for tint, right, up in QUADRANTS:
        spawn('NS_SS_Slash', at(right, up), tint, rot, Size=120)


def burst():
    for tint, right, up in QUADRANTS:
        spawn('NS_SS_Burst', at(right, up), tint, Radius=100)


def ring(tint, effect='NS_SS_Ring'):
    return lambda: spawn(effect, at(0, 0, ground=True), tint, Radius=280)


def charge():
    spawn('NS_SS_Charge', at(-150, 280), STAR, Radius=120)
    spawn('NS_SS_Charge', at(150, 260), DIVINE, Radius=120)
    spawn('NS_SS_Charge', at(0, 30), SOUL, Radius=150)


def fly(effect, shots):
    span = 520.0
    for tint, length, width, up, speed in shots:
        comp = spawn(effect, at(-span, up), tint, toward_screen_right(), Length=length, Width=width)
        t0 = world_time()
        MOVERS.append((comp, lambda t, t0=t0, up=up, speed=speed: at(-span + min((t - t0) * speed, 2 * span), up)))


def qi():
    fly('NS_SS_Qi', ((SOUL, 170, 60, 250, 900.0), (STAR, 300, 165, 40, 1100.0)))


def qi_tech():
    fly('NS_SS_Qi_Tech', ((MECHA, 170, 60, 250, 900.0), (MECHA, 300, 165, 40, 1100.0)))


def beam():
    source = at(0, 300)
    for i, (tint, width, right, up) in enumerate(((colour(1.0, 0.12, 0.08, 3.0), 16, -260, 40), (colour(0.15, 0.45, 1.0, 3.0), 16, -60, -40),
                                                  (colour(1.0, 0.86, 0.12, 3.0), 16, 200, 60), (colour(0.5, 1.0, 0.55, 2.0), 8, 260, 260))):
        spawn('NS_SS_Beam', source, tint, Width=width, Target=at(right, up) - source)


def shield():
    spawn('NS_SS_Shield', STATE['origin'], colour(1.0, 0.93, 0.55, 1.5), Radius=150)


def aura():
    for tint, right in ((SOUL, -200), (DIVINE, -70), (STAR, 60), (GOD, 190)):
        spawn('NS_SS_Aura', at(right, 190), tint, unreal.Rotator(0.0, 0.0, 20.0), Radius=135)


def aura_tech():
    for right in (-120, 120):
        spawn('NS_SS_Aura_Tech', at(right, 190), MECHA, unreal.Rotator(0.0, 0.0, 20.0), Radius=135)


def devour():
    # The serpent strikes toward screen left: no mirror (User.Flip keeps its positive default).
    sink = at(150, 250)
    spawn('NS_SS_Devour', sink, colour(0.62, 0.12, 1.0, 2.0), Radius=70, Source=at(-150, 30) - sink)


def devour_right():
    # Toward screen right the game passes User.Flip -1 so the serpent's crown stays up.
    sink = at(-150, 250)
    spawn('NS_SS_Devour', sink, colour(0.62, 0.12, 1.0, 2.0), Radius=70, Source=at(150, 30) - sink, Flip=-1.0)


def burn():
    # 噬魂's fire on a struck scarecrow-sized body (left) and a small monster (right), both standing on the floor.
    spawn('NS_SS_Burn', at(-130, 0, ground=True), colour(0.62, 0.12, 1.0, 2.0), Radius=55, Height=189)
    spawn('NS_SS_Burn', at(150, 0, ground=True), colour(0.62, 0.12, 1.0, 2.0), Radius=40, Height=80)


def trib_strike():
    # A tribulation bolt from high up the frame onto the anchor, and what it leaves on the anchor's feet.
    feet = at(0, 0, ground=True)
    strike = feet + unreal.Vector(0.0, 0.0, 58.0)
    sky = strike + STATE['up'] * 1150.0 + STATE['right'] * 160.0
    spawn('NS_SS_TribBolt', sky, colour(1.0, 1.0, 1.0), Width=13.5, Target=strike - sky)
    spawn('NS_SS_TribHit', feet, colour(1.0, 1.0, 1.0), Width=13.5, Height=192)


def spin():
    spawn('NS_SS_Spin', at(0, 170), MECHA, Radius=119)


def glyph():
    spawn('NS_SS_Glyph', at(-130, 230), MECHA, Radius=90)
    spawn('NS_SS_Glyph', at(110, 50), MECHA, Radius=200)


def shatter():
    for tint, right, up in QUADRANTS:
        spawn('NS_SS_Shatter', at(right, up), tint, Size=110)


def pillar():
    spawn('NS_SS_Pillar', at(-230, 0, ground=True), HEAL, Radius=90)
    spawn('NS_SS_Pillar', at(120, 0, ground=True), DIVINE, Radius=200)


def orbit(effect, tints):
    for i, tint in enumerate(tints):
        comp = spawn(effect, at(0, 180), tint, Width=21, Life=0.45)
        phase = i * 2.0 * math.pi / len(tints)
        MOVERS.append((comp, lambda t, phase=phase, i=i: at(math.cos(t * 2.4 + phase) * (170 + 25 * i), 180 + math.sin(t * 2.4 + phase) * 140)))


def slash_tech():
    rot = toward_screen_right()
    spawn('NS_SS_Slash_Tech', at(-120, 230), MECHA, rot, Size=120)
    spawn('NS_SS_Slash_Tech', at(100, 40), MECHA, rot, Size=150)


def burst_tech():
    spawn('NS_SS_Burst_Tech', at(-130, 230), MECHA, Radius=110)
    spawn('NS_SS_Burst_Tech', at(120, 40), MECHA, Radius=150)


def shatter_tech():
    spawn('NS_SS_Shatter_Tech', at(-140, 220), MECHA, Size=140)
    spawn('NS_SS_Shatter_Tech', at(140, 60), MECHA, Size=180)


# Ink wash in the four cultivator sword colours.
scene('01-slash', slash, (0.05, 0.12, 0.22, 0.32))
scene('02-burst', burst, (0.04, 0.12, 0.25, 0.4))
scene('03-ring-soul', ring(SOUL), (0.1, 0.3, 0.6))
scene('04-ring-gold', ring(DIVINE), (0.15, 0.4))
scene('05-ring-star', ring(STAR), (0.2, 0.5))
scene('06-charge', charge, (1.0, 2.2), slow=False)
scene('07-qi', qi, (0.35, 0.55), slow=False)
scene('08-beam', beam, (0.03, 0.08, 0.16))
scene('09-shield', shield, (1.0, 2.5), slow=False)
scene('10-aura', aura, (1.0,), slow=False)
scene('11-devour', devour, (0.12, 0.3, 0.4, 0.5, 0.75, 1.0, 1.25))
scene('15-devour-right', devour_right, (0.3, 0.4, 0.6))
scene('16-burn', burn, (0.8, 1.6), slow=False)
scene('17-trib', trib_strike, (0.03, 0.075, 0.12, 0.18, 0.3, 0.6, 1.5))
scene('12-shatter', shatter, (0.05, 0.15, 0.35))
scene('13-pillar', pillar, (0.1, 0.3, 0.6))
scene('14-trail', lambda: orbit('NS_SS_Trail', (SOUL, DIVINE, STAR, GOD)), (1.2, 2.0), slow=False)
# Futuristic twins and the mechanical form's own effects.
scene('21-slash-tech', slash_tech, (0.05, 0.12, 0.22))
scene('22-burst-tech', burst_tech, (0.04, 0.12, 0.25))
scene('23-ring-tech', ring(MECHA, 'NS_SS_Ring_Tech'), (0.1, 0.3, 0.6))
scene('24-qi-tech', qi_tech, (0.35, 0.55), slow=False)
scene('25-aura-tech', aura_tech, (1.0,), slow=False)
scene('26-trail-tech', lambda: orbit('NS_SS_Trail_Tech', (MECHA, MECHA)), (1.2,), slow=False)
scene('27-spin', spin, (0.8, 1.6), slow=False)
scene('28-glyph', glyph, (0.05, 0.15, 0.3))
scene('29-shatter-tech', shatter_tech, (0.05, 0.15, 0.35))
step('restore time', lambda: (clear(), unreal.GameplayStatics.set_global_time_dilation(STATE['world'], 1.0)), 0.3)


# ---------------------------------------------------------------- driver

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


def frame_scene():
    world = STATE['world']
    pawn = unreal.GameplayStatics.get_player_pawn(world, 0)
    camera = unreal.GameplayStatics.get_player_camera_manager(world, 0)
    rotation = camera.get_camera_rotation()
    STATE['right'], STATE['up'] = rotation.get_right_vector(), rotation.get_up_vector()
    STATE['origin'] = pawn.get_actor_location() if pawn else camera.get_camera_location() + rotation.get_forward_vector() * 1500.0
    STATE['floor'] = STATE['origin'].z - 88.0
    REPORT['camera'] = dict(location=str(camera.get_camera_location()), rotation=str(rotation), anchor=str(STATE['origin']))


def tick(_):
    try:
        if STATE['world']:
            now = world_time()
            for comp, path in MOVERS:
                comp.set_world_location(path(now), False, False)
        if time.monotonic() < STATE['next']:
            return
        if time.monotonic() - START > 720:
            raise TimeoutError('gallery exceeded 720s')
        if STATE['step'] == -2:
            check('begin PIE', LIB.begin_frontend_pie(720, 1280))
            STATE['step'], STATE['next'] = -1, time.monotonic() + 10
            return
        if STATE['step'] == -1:
            worlds = unreal.EditorLevelLibrary.get_pie_worlds(False)
            directors = unreal.GameplayStatics.get_all_actors_of_class(worlds[0], unreal.InkMenuDirector) if worlds else []
            if not directors or not directors[0].get_editor_property('game_ui'):
                STATE['next'] = time.monotonic() + 1
                return
            STATE['world'], STATE['director'] = worlds[0], directors[0]
            check('slate click Main.Start', directors[0].get_editor_property('game_ui').click_test_target('Main.Start'))
            STATE['step'], STATE['next'] = 0, time.monotonic() + 7.0
            return
        if STATE['step'] == 0 and STATE['origin'] is None:
            frame_scene()
        if STATE['step'] >= len(STEPS):
            REPORT['finished'] = True
            save()
            finish()
            return
        name, fn, delay, age = STEPS[STATE['step']]
        if age is not None and world_time() - STATE['t0'] < age:
            return
        REPORT['current_step'] = name
        fn()
        STATE['step'] += 1
        STATE['next'] = time.monotonic() + delay
        save()
    except Exception:
        REPORT['error'] = traceback.format_exc()
        save()
        finish()


HANDLE = [unreal.register_slate_post_tick_callback(tick)]
QUIT = [None]
unreal.log('Special sword VFX gallery writing to ' + str(OUT))

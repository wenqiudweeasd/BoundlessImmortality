"""Build the sword test range: /Game/LiveWeapons/Maps/L_SwordTestRange, a walkable row of rooms with realm scarecrows.

The level copies the main map's sky, sun, sky light, fog, post process and clouds and drops the island, so the effects
are still seen against the open sky they were tuned on. In its place a floating stone terrace runs along the game
camera's heading (up the portrait screen): eight rooms behind low walls, joined by doorways framed with pillars. Every
other room holds one ALiveScarecrow with a realm preset (练气一阶, 筑基初期, 元婴, 化神), whose attributes follow that
realm; the empty rooms between keep the next scarecrow out of the swords' reach. The player starts in the first room.
ALiveSwordTestGameMode spawns the gameplay cultivator (WASD to walk, its isometric camera, mouse wheel to zoom), switches
on local audience simulation and opens the 试剑台 dock; F10 opens the GM panel.

Run after building BoundlessImmortalityEditor, with the editor closed (the level is regenerated from scratch, so
hand edits to it are lost):
  UnrealEditor.exe BoundlessImmortality.uproject -unattended -noxgecontroller
      -ExecutePythonScript=Tools/SpecialSwords/build_sword_test_range.py
M_Scarecrow is created only when missing (the scarecrow class loads it at startup); the tile material is rebuilt.
The report lands in Saved/SwordTestRange/report.json.
"""
import json
import math
import traceback
from pathlib import Path

import unreal

PROJECT = Path(unreal.Paths.project_dir()).resolve()
FOLDER = '/Game/LiveWeapons/TestRange'
LEVEL = '/Game/LiveWeapons/Maps/L_SwordTestRange'
TEMPLATE = '/Game/Isometric_World/Sky_Temple/Maps/Level_Sky_Temple_Demo_01'
# The gameplay cultivator (ALiveSwordTestGameMode's pawn, whose camera the game uses) and the menu's presentation copy.
# The main map's own PlayerStart lies off the island; the game puts the player where the menu pawn stands.
CULTIVATOR = '/Game/Isometric_World/Demo/Blueprints/BP_TopDownCharacter.BP_TopDownCharacter_C'
MENU_PAWN = '/Game/1/BP_TopDown.BP_TopDown_C'
GAME_MODE = '/Script/BoundlessImmortality.LiveSwordTestGameMode'
SCARECROW = '/Script/BoundlessImmortality.LiveScarecrow'
FOOT_DEPTH = 95.0  # ALiveScarecrow::FootDepth: the actor sits at chest height above its post
# Rooms along the camera heading. Sword engage range is 12 m and the widest skills reach 18 m from the anchor, so with
# 11 m rooms the scarecrows (two rooms apart) stay out of each other's fights while the player stands by one.
ROOM_DEPTH, ROOM_WIDTH, ROOMS = 1100.0, 900.0, 8
FLOOR_THICKNESS, WALL_HEIGHT, WALL_THICKNESS, DOOR_WIDTH = 60.0, 70.0, 40.0, 320.0
PILLAR_RADIUS, PILLAR_HEIGHT = 34.0, 190.0
# Room index -> scarecrow realm index: one in every other room (练气一阶, 筑基初期, 元婴, 化神).
REALM_ROOMS = {1: (0, '练气'), 3: (9, '筑基'), 5: (22, '元婴'), 7: (23, '化神')}
# Scarecrows stand a little past the room centre, facing the doorway the player comes through.
SCARECROW_AHEAD = 180.0
STONE = (0.2, 0.19, 0.175)
WALL = (0.26, 0.245, 0.22)
CAP = (0.33, 0.27, 0.16)
ROCK = (0.12, 0.115, 0.11)
# Sky and lighting kept from the template; everything else in it is removed.
KEEP_CLASSES = {'SkyAtmosphere', 'DirectionalLight', 'SkyLight', 'ExponentialHeightFog', 'PostProcessVolume', 'VolumetricCloud',
                'WorldSettings', 'Brush'}
OUT = PROJECT / 'Saved/SwordTestRange'
OUT.mkdir(parents=True, exist_ok=True)
REPORT = dict(level=LEVEL, kept=[], removed=0, rooms=[], scarecrows=[], errors=[])

tools = unreal.AssetToolsHelpers.get_asset_tools()
mel = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)

# Running-bond flagstones in the room's own axes (custom primitive data 1 and 2 hold the cosine and sine of the room
# heading): 120 x 80 cm stones, every other row shifted half a stone, dark mortar, per-stone tone and a fine grain.
# Custom primitive data 0 brightens or darkens a room.
TILE_CODE = r'''
float2 p = float2(dot(Pos.xy, float2(C, S)), dot(Pos.xy, float2(-S, C)));
float2 size = float2(120.0, 80.0);
float row = floor(p.y / size.y);
float x = p.x / size.x + fmod(abs(row), 2.0) * 0.5;
float2 cell = float2(floor(x), row);
float2 f = float2(frac(x) * size.x, frac(p.y / size.y) * size.y);
float edge = min(min(f.x, size.x - f.x), min(f.y, size.y - f.y));
float mortar = 1.0 - smoothstep(1.5, 4.0, edge);
float h = frac(sin(dot(cell, float2(12.9898, 78.233))) * 43758.5453);
float g = frac(sin(dot(floor(p / 5.0), float2(127.1, 311.7))) * 43758.5453);
float3 stone = lerp(float3(0.30, 0.285, 0.255), float3(0.44, 0.415, 0.36), h) * (0.9 + 0.14 * g);
return lerp(stone, float3(0.07, 0.065, 0.06), mortar) * Tone;
'''
# Breaks up the flat part colours with a fine fibre speckle.
SCARECROW_CODE = r'''
float n = frac(sin(dot(floor(Pos.xy / 2.5) + floor(Pos.z / 1.5) * 7.13, float2(12.9898, 78.233))) * 43758.5453);
return Color.rgb * (0.82 + 0.3 * n);
'''


def unlink(package):
    file = PROJECT / 'Content' / (package[len('/Game/'):] + ('.umap' if '/Maps/' in package else '.uasset'))
    if file.is_file():
        file.unlink()


def custom(material, code, inputs, x, y):
    node = mel.create_material_expression(material, unreal.MaterialExpressionCustom, x, y)
    node.set_editor_property('code', code)
    node.set_editor_property('output_type', unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    items = []
    for name in inputs:
        item = unreal.CustomInput()
        item.set_editor_property('input_name', name)
        items.append(item)
    node.set_editor_property('inputs', items)
    return node


def lit_material(name):
    material = tools.create_asset(name, FOLDER, unreal.Material, unreal.MaterialFactoryNew())
    assert material, name
    roughness = mel.create_material_expression(material, unreal.MaterialExpressionConstant, -300, 260)
    roughness.set_editor_property('r', 0.85)
    assert mel.connect_material_property(roughness, '', unreal.MaterialProperty.MP_ROUGHNESS)
    return material


def primitive_scalar(material, name, slot, value, x, y):
    node = mel.create_material_expression(material, unreal.MaterialExpressionScalarParameter, x, y)
    node.set_editor_property('parameter_name', name)
    node.set_editor_property('default_value', value)
    node.set_editor_property('use_custom_primitive_data', True)
    node.set_editor_property('primitive_data_index', slot)
    return node


def scarecrow_material():
    """Part colours come from custom primitive data slot 0, set per part by ALiveScarecrow (and per wall here)."""
    material = lit_material('M_Scarecrow')
    color = mel.create_material_expression(material, unreal.MaterialExpressionVectorParameter, -800, 0)
    color.set_editor_property('parameter_name', 'Color')
    color.set_editor_property('default_value', unreal.LinearColor(0.5, 0.4, 0.25, 1.0))
    color.set_editor_property('use_custom_primitive_data', True)
    color.set_editor_property('primitive_data_index', 0)
    position = mel.create_material_expression(material, unreal.MaterialExpressionWorldPosition, -800, 200)
    node = custom(material, SCARECROW_CODE, ('Color', 'Pos'), -450, 60)
    assert mel.connect_material_expressions(color, '', node, 'Color')
    assert mel.connect_material_expressions(position, '', node, 'Pos')
    assert mel.connect_material_property(node, '', unreal.MaterialProperty.MP_BASE_COLOR)
    mel.recompile_material(material)
    return material


def tile_material():
    material = lit_material('M_TestRangeTiles')
    position = mel.create_material_expression(material, unreal.MaterialExpressionWorldPosition, -800, -100)
    tone = primitive_scalar(material, 'Tone', 0, 1.0, -800, 40)
    cosine = primitive_scalar(material, 'C', 1, 1.0, -800, 160)
    sine = primitive_scalar(material, 'S', 2, 0.0, -800, 280)
    node = custom(material, TILE_CODE, ('Pos', 'Tone', 'C', 'S'), -450, 40)
    for source, pin in ((position, 'Pos'), (tone, 'Tone'), (cosine, 'C'), (sine, 'S')):
        assert mel.connect_material_expressions(source, '', node, pin)
    assert mel.connect_material_property(node, '', unreal.MaterialProperty.MP_BASE_COLOR)
    mel.recompile_material(material)
    return material


def keep(actor):
    name = actor.get_class().get_name()
    if name in KEEP_CLASSES:
        return True
    if isinstance(actor, unreal.StaticMeshActor):
        mesh = actor.static_mesh_component.get_editor_property('static_mesh')
        return bool(mesh) and 'SkySphere' in mesh.get_name()
    # The soft clouds drifting around the island.
    for kind in (unreal.ParticleSystemComponent, unreal.NiagaraComponent):
        component = actor.get_component_by_class(kind)
        if component:
            asset = component.get_editor_property('template' if kind is unreal.ParticleSystemComponent else 'asset')
            return bool(asset) and 'Cloud' in asset.get_name()
    return False


class Terrace:
    """Places meshes in the room frame: a runs along the camera heading from the start room's centre, b to its right."""

    def __init__(self, center, ground, heading, paint, tiles):
        self.center, self.ground, self.heading, self.paint, self.tiles = center, ground, heading, paint, tiles
        self.forward = (math.cos(math.radians(heading)), math.sin(math.radians(heading)))
        self.cube = unreal.load_asset('/Engine/BasicShapes/Cube')
        self.cylinder = unreal.load_asset('/Engine/BasicShapes/Cylinder')

    def at(self, a, b, z):
        f = self.forward
        return unreal.Vector(self.center.x + f[0] * a - f[1] * b, self.center.y + f[1] * a + f[0] * b, z)

    def mesh(self, mesh, a, b, z, scale, material, label, folder):
        actor = eas.spawn_actor_from_object(mesh, self.at(a, b, z), unreal.Rotator(0, 0, self.heading))
        actor.set_actor_scale3d(scale)
        actor.static_mesh_component.set_material(0, material)
        actor.set_actor_label(label)
        actor.set_folder_path(folder)
        return actor

    def painted(self, mesh, a, b, z, scale, color, label, folder):
        actor = self.mesh(mesh, a, b, z, scale, self.paint, label, folder)
        actor.static_mesh_component.set_default_custom_primitive_data_vector4(0, unreal.Vector4(color[0], color[1], color[2], 1.0))
        return actor

    def box(self, a, b, z, along, across, height, color, label, folder):
        """A stone block centred at (a, b) whose base sits at z."""
        return self.painted(self.cube, a, b, z + height / 2, unreal.Vector(along / 100, across / 100, height / 100), color, label, folder)

    def floor(self, room, tone):
        top = self.ground
        slab = self.mesh(self.cube, room * ROOM_DEPTH, 0.0, top - FLOOR_THICKNESS / 2,
                         unreal.Vector(ROOM_DEPTH / 100, ROOM_WIDTH / 100, FLOOR_THICKNESS / 100), self.tiles, 'Floor_%d' % room, 'TestRange/Floor')
        component = slab.static_mesh_component
        component.set_default_custom_primitive_data_float(0, tone)
        component.set_default_custom_primitive_data_float(1, self.forward[0])
        component.set_default_custom_primitive_data_float(2, self.forward[1])
        return slab

    def pillar(self, a, b, label):
        base = self.ground
        self.painted(self.cylinder, a, b, base + PILLAR_HEIGHT / 2,
                     unreal.Vector(PILLAR_RADIUS / 50, PILLAR_RADIUS / 50, PILLAR_HEIGHT / 100), WALL, label, 'TestRange/Walls')
        self.box(a, b, base + PILLAR_HEIGHT, PILLAR_RADIUS * 2.6, PILLAR_RADIUS * 2.6, 18.0, CAP, label + 'Cap', 'TestRange/Walls')


def build_terrace(t):
    half_w = ROOM_WIDTH / 2
    start, end = -ROOM_DEPTH / 2, (ROOMS - 1) * ROOM_DEPTH + ROOM_DEPTH / 2
    length = end - start
    for room in range(ROOMS):
        # Scarecrow rooms read a touch warmer than the passages between them.
        t.floor(room, 1.08 if room in REALM_ROOMS else 0.9)
        REPORT['rooms'].append(dict(index=room, a=room * ROOM_DEPTH, realm=REALM_ROOMS.get(room, (None,))[0]))
    # The terrace floats: a rim under the slabs and a heavy rock keel below them.
    t.box((start + end) / 2, 0.0, t.ground - FLOOR_THICKNESS - 40.0, length + 60.0, ROOM_WIDTH + 60.0, 40.0, STONE, 'Rim', 'TestRange/Floor')
    t.box((start + end) / 2, 0.0, t.ground - FLOOR_THICKNESS - 40.0 - 260.0, length - 200.0, ROOM_WIDTH - 220.0, 260.0, ROCK, 'Keel', 'TestRange/Floor')
    # Side walls along the whole terrace, end walls at both ends.
    for side, sign in (('L', -1.0), ('R', 1.0)):
        t.box((start + end) / 2, sign * (half_w - WALL_THICKNESS / 2), t.ground, length, WALL_THICKNESS, WALL_HEIGHT, WALL, 'Wall' + side, 'TestRange/Walls')
    for label, a in (('WallBack', start + WALL_THICKNESS / 2), ('WallFront', end - WALL_THICKNESS / 2)):
        t.box(a, 0.0, t.ground, WALL_THICKNESS, ROOM_WIDTH, WALL_HEIGHT, WALL, label, 'TestRange/Walls')
    # Partitions with a doorway between neighbouring rooms, pillars framing each door, pillars at the outer corners.
    segment = (ROOM_WIDTH - DOOR_WIDTH) / 2
    for room in range(ROOMS - 1):
        a = room * ROOM_DEPTH + ROOM_DEPTH / 2
        for side, sign in (('L', -1.0), ('R', 1.0)):
            t.box(a, sign * (DOOR_WIDTH / 2 + segment / 2), t.ground, WALL_THICKNESS, segment, WALL_HEIGHT, WALL,
                  'Partition_%d%s' % (room, side), 'TestRange/Walls')
            t.pillar(a, sign * (DOOR_WIDTH / 2 + PILLAR_RADIUS), 'Door_%d%s' % (room, side))
    for label, a in (('Back', start + PILLAR_RADIUS), ('Front', end - PILLAR_RADIUS)):
        for side, sign in (('L', -1.0), ('R', 1.0)):
            t.pillar(a, sign * (half_w - PILLAR_RADIUS), 'Corner' + label + side)


def main():
    unlink(LEVEL)
    # M_TestRangeFloor belonged to the floating-stone range; the rooms use M_TestRangeTiles.
    unlink(FOLDER + '/M_TestRangeFloor')
    unlink(FOLDER + '/M_TestRangeTiles')
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    registry.scan_paths_synchronous([FOLDER, '/Game/LiveWeapons/Maps'], True)
    if not eal.does_asset_exist(FOLDER + '/M_Scarecrow'):
        scarecrow_material()
    tiles = tile_material()
    paint = unreal.load_asset(FOLDER + '/M_Scarecrow')
    for path in (FOLDER + '/M_Scarecrow', FOLDER + '/M_TestRangeTiles'):
        eal.save_asset(path, only_if_is_dirty=False)

    assert les.new_level_from_template(LEVEL, TEMPLATE), 'new_level_from_template failed'
    center, yaw, ground = None, 0.0, 0.0
    for actor in eas.get_all_level_actors():
        if actor.get_class().get_path_name() == MENU_PAWN:
            capsule = actor.get_component_by_class(unreal.CapsuleComponent)
            center, yaw = actor.get_actor_location(), actor.get_actor_rotation().yaw
            ground = center.z - (capsule.get_scaled_capsule_half_height() if capsule else 90.0)
            REPORT['template_menu_pawn'] = dict(x=center.x, y=center.y, z=center.z, yaw=yaw, ground=ground)
            break
    assert center, 'menu pawn not found in ' + TEMPLATE
    doomed = []
    for actor in eas.get_all_level_actors():
        if keep(actor):
            REPORT['kept'].append(actor.get_class().get_name() + ':' + actor.get_actor_label())
        else:
            doomed.append(actor)
    REPORT['removed'] = len(doomed)
    eas.destroy_actors(doomed)

    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    settings = next((getattr(world, name)() for name in ('get_world_settings', 'k2_get_world_settings') if hasattr(world, name)), None)
    if not settings:
        found = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.WorldSettings)
        settings = found[0] if found else None
    assert settings, 'no world settings'
    settings.set_editor_property('default_game_mode', unreal.load_class(None, GAME_MODE))

    # The game camera keeps a fixed heading; read it from a cultivator standing at the start, then remove it.
    heading = yaw
    probe = eas.spawn_actor_from_class(unreal.load_class(None, CULTIVATOR), unreal.Vector(center.x, center.y, ground + 100), unreal.Rotator(0, 0, yaw))
    camera = probe.get_component_by_class(unreal.CameraComponent) if probe else None
    if camera:
        heading = camera.get_world_rotation().yaw
        where = camera.get_world_location()
        REPORT['camera'] = dict(x=where.x, y=where.y, z=where.z, pitch=camera.get_world_rotation().pitch, yaw=heading,
                                fov=camera.get_editor_property('field_of_view'))
    if probe:
        eas.destroy_actor(probe)
    REPORT['heading'] = heading

    terrace = Terrace(center, ground, heading, paint, tiles)
    build_terrace(terrace)
    start = eas.spawn_actor_from_class(unreal.PlayerStart, terrace.at(0.0, 0.0, ground + 100), unreal.Rotator(0, 0, heading))
    start.set_folder_path('TestRange')

    scarecrow_class = unreal.load_class(None, SCARECROW)
    for room, (realm, label) in sorted(REALM_ROOMS.items()):
        a = room * ROOM_DEPTH + SCARECROW_AHEAD
        where = terrace.at(a, 0.0, ground + FOOT_DEPTH)
        scarecrow = eas.spawn_actor_from_class(scarecrow_class, where, unreal.Rotator(0, 0, heading + 180.0))
        scarecrow.set_actor_label('Scarecrow_%s' % label)
        scarecrow.set_folder_path('TestRange/Scarecrows')
        scarecrow.set_editor_property('realm_preset', realm)
        # The 试剑台 paints an ink plate over each realm scarecrow; the old text readout would sit under it.
        scarecrow.set_editor_property('show_readout', False)
        # The class found no M_Scarecrow at startup on the first run; later runs inherit it from the class defaults.
        for part in scarecrow.get_components_by_class(unreal.StaticMeshComponent):
            part.set_material(0, paint)
        REPORT['scarecrows'].append(dict(room=room, realm=realm, label=label, x=where.x, y=where.y, z=where.z))

    assert les.save_current_level(), 'save_current_level failed'
    REPORT['saved'] = True


try:
    main()
except Exception:
    REPORT['errors'].append(traceback.format_exc())
(OUT / 'report.json').write_text(json.dumps(REPORT, ensure_ascii=False, indent=2), 'utf-8')
print('SWORD_TEST_RANGE_REPORT', json.dumps(REPORT, ensure_ascii=False))

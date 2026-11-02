"""Author the special sword-spirit assets: effect materials, the four Meshy swords, the giant axe and NS_SS_* Niagara.

Run after building BoundlessImmortalityEditor, with the editor closed:
  UnrealEditor-Cmd.exe BoundlessImmortality.uproject -run=pythonscript
      -script=Tools/SpecialSwords/build_special_sword_assets.py -unattended -nosplash -nop4 -NoLiveCoding -NoAutoSave -noxgecontroller
SPECIAL_SWORD_SOURCE may point at the folder holding the four download folders (default: ~/Downloads).
SPECIAL_SWORD_EFFECTS_ONLY=1 rebuilds just the effect and ghost materials and the Niagara systems (no download needed).
Re-running deletes and rebuilds only the assets listed here; the report lands in Saved/SpecialSwordImport.
The look of the effects lives in special_sword_vfx.py next to this script.
"""
import json
import os
import shutil
import sys
from pathlib import Path

import unreal

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import special_sword_vfx as vfx  # noqa: E402

ROOT = '/Game/LiveWeapons/SpecialSwords'
MESHES = ROOT + '/Meshes'
MATERIALS = ROOT + '/Materials'
TEXTURES = ROOT + '/Textures'
VFX = ROOT + '/VFX'
IMPORT = ROOT + '/Import'
PROJECT = Path(unreal.Paths.project_dir()).resolve()
SOURCE = Path(os.environ.get('SPECIAL_SWORD_SOURCE', str(Path.home() / 'Downloads')))
STAGING = PROJECT / 'Saved/SpecialSwordImport'

# Asset name, download folder, file stem, blade length used by the runtime (cm), glow tint (LiveSpecialSwords::Color).
SWORDS = (
    ('SoulEater', '噬魂剑', 'Meshy_AI_Soul_Devouring_Sword__1020100625_texture', 130, (0.62, 0.12, 1.0)),
    ('DivineBlade', '驱煞神刃', 'Meshy_AI_Evil_Warding_Immortal_1020100322_texture', 135, (1.0, 0.72, 0.18)),
    ('SevenStar', '七星光茫剑', 'Meshy_AI_Seven_Star_Radiant_No_1020100350_image-to-3d-texture', 140, (1.0, 0.93, 0.55)),
    ('Godslayer', '弑神剑', 'Meshy_AI_Godslayer_Sword_Game_1020100649_texture', 135, (1.0, 0.12, 0.08)),
)
EFFECTS_ONLY = os.environ.get('SPECIAL_SWORD_EFFECTS_ONLY') == '1'

tools = unreal.AssetToolsHelpers.get_asset_tools()
mel = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary
lib = unreal.SwordSpiritVfxEditorLibrary
FLOAT3 = unreal.CustomMaterialOutputType.CMOT_FLOAT3
FLOAT4 = unreal.CustomMaterialOutputType.CMOT_FLOAT4


def asset_path(folder, name):
    return folder + '/' + name


def owned_packages():
    # Retired masks are only deleted; the ghost materials belong to the effects and are rebuilt with them.
    packages = [asset_path(VFX, name) for name in vfx.EFFECTS] + [asset_path(MATERIALS, name) for name in vfx.MASKS]
    packages += [asset_path(MATERIALS, name) for name in vfx.RETIRED + ('M_SS_Ghost', 'M_SS_GhostInk')]
    if not EFFECTS_ONLY:
        for name, *_ in SWORDS:
            packages += [asset_path(MESHES, 'SM_' + name), asset_path(MATERIALS, 'MI_SS_' + name), asset_path(TEXTURES, 'T_SS_' + name)]
        packages += [asset_path(MESHES, 'SM_GiantAxe'), asset_path(MATERIALS, 'M_SS_Blade')]
    return packages


def remove_owned():
    """Deletes the owned packages' files before anything loads them. Deleting loaded packages through the editor and
    saving new assets under the same names in the same session crashes the save (null access in Engine)."""
    for package in owned_packages():
        file = PROJECT / 'Content' / (package[len('/Game/'):] + '.uasset')
        if file.is_file():
            file.unlink()
    unreal.AssetRegistryHelpers.get_asset_registry().scan_paths_synchronous([ROOT], True)
    if eal.does_directory_exist(IMPORT):
        eal.delete_directory(IMPORT)


def new_material(name, blend, two_sided=True, lit=False):
    material = tools.create_asset(name, MATERIALS, unreal.Material, unreal.MaterialFactoryNew())
    assert material, name
    material.set_editor_property('blend_mode', blend)
    material.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_DEFAULT_LIT if lit else unreal.MaterialShadingModel.MSM_UNLIT)
    material.set_editor_property('two_sided', two_sided)
    return material


def custom(material, code, inputs, x, y, output=FLOAT3):
    node = mel.create_material_expression(material, unreal.MaterialExpressionCustom, x, y)
    node.set_editor_property('code', code)
    node.set_editor_property('output_type', output)
    items = []
    for name in inputs:
        item = unreal.CustomInput()
        item.set_editor_property('input_name', name)
        items.append(item)
    node.set_editor_property('inputs', items)
    return node


def parameter(material, kind, name, value, x, y):
    node = mel.create_material_expression(material, kind, x, y)
    node.set_editor_property('parameter_name', name)
    node.set_editor_property('default_value', value)
    return node


def component_mask(material, source, channels, x, y):
    mask = mel.create_material_expression(material, unreal.MaterialExpressionComponentMask, x, y)
    for channel in 'rgba':
        mask.set_editor_property(channel, channel in channels)
    assert mel.connect_material_expressions(source, '', mask, '')
    return mask


def effect_material(name):
    """Premultiplied particle mask: the custom node returns (emissive, opacity) for AlphaComposite blending."""
    material = new_material(name, unreal.BlendMode.BLEND_ALPHA_COMPOSITE)
    for flag in ('used_with_niagara_sprites', 'used_with_niagara_ribbons', 'used_with_niagara_mesh_particles'):
        material.set_editor_property(flag, True)
    uv = mel.create_material_expression(material, unreal.MaterialExpressionTextureCoordinate, -900, -100)
    particle = mel.create_material_expression(material, unreal.MaterialExpressionParticleColor, -900, 100)
    dynamic = mel.create_material_expression(material, unreal.MaterialExpressionDynamicParameter, -900, 300)
    dynamic.set_editor_property('default_value', unreal.LinearColor(0.15, 0.5, 1.0, 0.0))
    clock = mel.create_material_expression(material, unreal.MaterialExpressionTime, -900, 520)
    node = custom(material, vfx.material_code(name), ('UV', 'Tint', 'Alpha', 'Dyn', 'Clock'), -520, 100, FLOAT4)
    assert mel.connect_material_expressions(uv, '', node, 'UV')
    assert mel.connect_material_expressions(particle, '', node, 'Tint')
    assert mel.connect_material_expressions(particle, 'A', node, 'Alpha')
    assert mel.connect_material_expressions(dynamic, 'RGBA', node, 'Dyn')
    assert mel.connect_material_expressions(clock, '', node, 'Clock')
    assert mel.connect_material_property(component_mask(material, node, 'rgb', -240, 40), '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    assert mel.connect_material_property(component_mask(material, node, 'a', -240, 200), '', unreal.MaterialProperty.MP_OPACITY)
    mel.recompile_material(material)
    return material


def ghost_inputs(material, exponent):
    """Color/Opacity parameters, a fresnel rim, world position and time for the ghost materials."""
    material.set_editor_property('used_with_skeletal_mesh', True)
    color = parameter(material, unreal.MaterialExpressionVectorParameter, 'Color', unreal.LinearColor(0.4, 0.9, 1.0, 1.0), -700, 0)
    opacity = parameter(material, unreal.MaterialExpressionScalarParameter, 'Opacity', 0.85, -700, 220)
    rim = mel.create_material_expression(material, unreal.MaterialExpressionFresnel, -700, 380)
    rim.set_editor_property('exponent', exponent)
    rim.set_editor_property('base_reflect_fraction', 0.04)
    position = mel.create_material_expression(material, unreal.MaterialExpressionWorldPosition, -700, 540)
    clock = mel.create_material_expression(material, unreal.MaterialExpressionTime, -700, 680)
    return (color, opacity, rim, position, clock)


def ghost_node(material, code, inputs, output=FLOAT3):
    node = custom(material, code, ('Tint', 'Opacity', 'Rim', 'Pos', 'Clock'), -350, 120, output)
    for source, pin in zip(inputs, ('Tint', 'Opacity', 'Rim', 'Pos', 'Clock')):
        assert mel.connect_material_expressions(source, '', node, pin)
    return node


def ghost_material():
    """The giant axe of 无机态那: an additive hologram, rim-lit with scan bands running up it."""
    material = new_material('M_SS_Ghost', unreal.BlendMode.BLEND_ADDITIVE)
    node = ghost_node(material, 'float scan = 0.7 + 0.5 * step(0.5, frac(Pos.z * 0.12 - Clock * 0.6));\n'
                      'return Tint * Opacity * (Rim * 1.8 + 0.22) * scan;', ghost_inputs(material, 2.6))
    assert mel.connect_material_property(node, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.recompile_material(material)
    return material


def ghost_ink_material():
    """Ink clones of the cultivator swords: a translucent ink body with ink flowing along it and a rim of the sword's
    colour (Color is normalised, so its brightness does not matter)."""
    material = new_material('M_SS_GhostInk', unreal.BlendMode.BLEND_TRANSLUCENT)
    node = ghost_node(material, '''
float mx = max(max(Tint.r, Tint.g), max(Tint.b, 0.0001));
float3 n = Tint / mx;
float3 ink = lerp(float3(0.014, 0.014, 0.018), n * 0.045, 0.4);
float flow = 0.8 + 0.2 * sin((dot(Pos, float3(0.011, 0.007, 0.013)) - Clock * 0.6) * 6.2831853);
float edge = saturate(Rim * 1.5);
return float4(lerp(ink, n * 1.3, edge * edge), saturate(Opacity * (0.62 * flow + edge * 0.6)));
''', ghost_inputs(material, 2.2), FLOAT4)
    assert mel.connect_material_property(component_mask(material, node, 'rgb', -120, 60), '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    assert mel.connect_material_property(component_mask(material, node, 'a', -120, 220), '', unreal.MaterialProperty.MP_OPACITY)
    mel.recompile_material(material)
    return material


def blade_material():
    """Lit sword surface from the Meshy albedo with a faint self-glow and a coloured rim."""
    material = new_material('M_SS_Blade', unreal.BlendMode.BLEND_OPAQUE, two_sided=False, lit=True)
    albedo = mel.create_material_expression(material, unreal.MaterialExpressionTextureSampleParameter2D, -800, 0)
    albedo.set_editor_property('parameter_name', 'Albedo')
    albedo.set_editor_property('texture', unreal.load_asset('/Engine/EngineResources/DefaultTexture'))
    glow = parameter(material, unreal.MaterialExpressionVectorParameter, 'Glow', unreal.LinearColor(1.0, 1.0, 1.0, 1.0), -800, 300)
    strength = parameter(material, unreal.MaterialExpressionScalarParameter, 'GlowStrength', 0.35, -800, 450)
    rim = mel.create_material_expression(material, unreal.MaterialExpressionFresnel, -800, 580)
    rim.set_editor_property('exponent', 3.0)
    emissive = custom(material, 'return Base * Glow * Strength + Glow * Rim * 0.9;', ('Base', 'Glow', 'Strength', 'Rim'), -400, 300)
    assert mel.connect_material_expressions(albedo, '', emissive, 'Base')
    assert mel.connect_material_expressions(glow, '', emissive, 'Glow')
    assert mel.connect_material_expressions(strength, '', emissive, 'Strength')
    assert mel.connect_material_expressions(rim, '', emissive, 'Rim')
    metal = parameter(material, unreal.MaterialExpressionScalarParameter, 'Metallic', 0.55, -400, 520)
    rough = parameter(material, unreal.MaterialExpressionScalarParameter, 'Roughness', 0.32, -400, 620)
    assert mel.connect_material_property(albedo, '', unreal.MaterialProperty.MP_BASE_COLOR)
    assert mel.connect_material_property(emissive, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    assert mel.connect_material_property(metal, '', unreal.MaterialProperty.MP_METALLIC)
    assert mel.connect_material_property(rough, '', unreal.MaterialProperty.MP_ROUGHNESS)
    mel.recompile_material(material)
    return material


def import_file(source, folder, name):
    task = unreal.AssetImportTask()
    task.set_editor_property('filename', str(source))
    task.set_editor_property('destination_path', folder)
    task.set_editor_property('destination_name', name)
    task.set_editor_property('automated', True)
    task.set_editor_property('replace_existing', True)
    task.set_editor_property('save', False)
    tools.import_asset_tasks([task])
    return [unreal.load_asset(path) for path in task.get_editor_property('imported_object_paths')]


def import_sword(name, folder, stem, length, glow, blade):
    source = SOURCE / folder
    obj, png = source / (stem + '.obj'), source / (stem + '.png')
    assert obj.is_file() and png.is_file(), 'missing download for ' + name + ': ' + str(source)
    # The OBJ is copied alone into an empty staging folder, so the importer only builds the mesh.
    stage = STAGING / name
    if stage.exists():
        shutil.rmtree(stage)
    stage.mkdir(parents=True)
    staged = stage / ('SM_' + name + '.obj')
    shutil.copyfile(obj, staged)

    texture = next((a for a in import_file(png, TEXTURES, 'T_SS_' + name) if isinstance(a, unreal.Texture2D)), None)
    assert texture, 'texture import failed for ' + name
    texture.set_editor_property('max_texture_size', 2048)
    texture.set_editor_property('srgb', True)

    imported = import_file(staged, IMPORT + '/' + name, 'SM_' + name)
    meshes = [a for a in imported if isinstance(a, unreal.StaticMesh)]
    assert meshes, 'mesh import failed for ' + name + ': ' + repr(imported)
    mesh_path = asset_path(MESHES, 'SM_' + name)
    assert eal.rename_asset(meshes[0].get_path_name(), mesh_path), 'could not move mesh for ' + name
    mesh = unreal.load_asset(mesh_path)

    instance = tools.create_asset('MI_SS_' + name, MATERIALS, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(instance, blade)
    mel.set_material_instance_texture_parameter_value(instance, 'Albedo', texture)
    mel.set_material_instance_vector_parameter_value(instance, 'Glow', unreal.LinearColor(glow[0], glow[1], glow[2], 1.0))
    mel.update_material_instance(instance)

    report = lib.normalize_sword_mesh(mesh, float(length), 40000)
    for index in range(len(mesh.get_editor_property('static_materials'))):
        mesh.set_material(index, instance)
    return {'mesh': mesh_path, 'texture': texture.get_path_name(), 'material': instance.get_path_name(), 'normalize': report,
            'extra_import_assets': [a.get_path_name() for a in imported if a and not isinstance(a, unreal.StaticMesh)]}


def build():
    report = {'source': None if EFFECTS_ONLY else str(SOURCE), 'swords': [], 'effects': []}
    remove_owned()
    for folder in (MESHES, MATERIALS, TEXTURES, VFX):
        eal.make_directory(folder)
    # Material keys are the mask names without the M_SS_ prefix (special_sword_vfx.OCCLUSION).
    effect = {name[len('M_SS_'):]: effect_material(name) for name in vfx.MASKS}
    ghost = ghost_material()
    ghost_ink_material()
    if not EFFECTS_ONLY:
        blade = blade_material()
        for sword in SWORDS:
            report['swords'].append(import_sword(*sword, blade))
        if eal.does_directory_exist(IMPORT):
            eal.delete_directory(IMPORT)
        axe = lib.create_giant_axe_mesh(asset_path(MESHES, 'SM_GiantAxe'), ghost)
        assert axe, 'giant axe failed'
        report['axe'] = axe.get_path_name()
    lib.finish_pending_compilation()
    report['effects'] = list(lib.build_sword_spirit_effects(VFX, effect, vfx.spec_json()))
    for line in report['effects']:
        unreal.log('[SpecialSwords] ' + line)
    lib.finish_pending_compilation()
    assert eal.save_directory(ROOT, True, True), 'save failed'
    STAGING.mkdir(parents=True, exist_ok=True)
    path = STAGING / ('effects-report.json' if EFFECTS_ONLY else 'report.json')
    path.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
    unreal.log('[SpecialSwords] report: ' + str(path))


build()

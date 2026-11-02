"""Look of the special sword-spirit effects: procedural particle masks and the NS_SS_* Niagara systems.

Two art styles share one effect contract (LiveSwordVfx::EFx). 噬魂剑, 驱煞神刃, 七星光芒剑 and 弑神剑 play the ink-wash
systems (NS_SS_<Name>): brush strokes, splashes, washes, ensō circles, trigram formations, calligraphy, talismans and
seals in dark ink, with the sword's colour kept for thin accents. The mechanical 无机态那 plays futuristic hologram
twins (NS_SS_<Name>_Tech) plus its own NS_SS_Spin and NS_SS_Glyph. Inside the ink style every sword also shows its own
signature elements, switched on by the hue of User.Color: violet soul fire for 噬魂, talismans and gold leaf for 驱煞,
the Big Dipper and auspicious clouds for 七星, cinnabar seals, blood ink and red plum petals for 弑神.

Pure data, no `unreal` import: build_special_sword_assets.py turns MASKS into M_SS_* materials and passes
spec_json() to USwordSpiritVfxEditorLibrary.BuildSwordSpiritEffects, which writes every particle attribute
analytically from age, id and the system's user parameters (User.Color plus the size parameters listed per system).

Materials are premultiplied-alpha (AlphaComposite): emissive adds light, and Dyn.x ("occlusion") lets a layer also
cover the background. Ink layers are dark colours at full occlusion, so the ink paints over the scene; glow accents
add light with little occlusion. Particles.DynamicMaterialParameter = (occlusion, per-particle seed, progress, variant).
Systems that move or stay attached (trails, aura, sword-qi, charge, shield, spin) never use {SEED}: it hashes the
owner position, so it would change every frame there.
"""
import json

# ---------------------------------------------------------------- materials

PRELUDE = '''
float2 d = UV * 2.0 - 1.0;
d.y = -d.y;
float occ = saturate(Dyn.x);
float mx = max(max(Tint.r, Tint.g), max(Tint.b, 0.0001));
float3 white = float3(mx, mx, mx);
float3 col = float3(0.0, 0.0, 0.0);
float cover = 0.0;
'''

EPILOGUE = '''
return float4(max(col, 0.0) * Alpha, saturate(cover * Alpha * occ));
'''


def _hash(out, cell):
    """Hoskins hash12 of a float2 cell, without sin() so it stays stable for large cells."""
    return ('float3 {o}_p = frac(float3(({c}).xyx) * 0.1031);\n'
            '{o}_p += dot({o}_p, {o}_p.yzx + 33.33);\n'
            'float {o} = frac(({o}_p.x + {o}_p.y) * {o}_p.z);\n').format(o=out, c=cell)


def _fbm(out, point, octaves):
    """Value-noise fbm in [0, ~1) written to a new float `out`."""
    corners = ''.join(_hash('h' + name, 'i + float2(%s)' % offset)
                      for name, offset in (('00', '0.0, 0.0'), ('10', '1.0, 0.0'), ('01', '0.0, 1.0'), ('11', '1.0, 1.0')))
    return '''
float {o} = 0.0;
{{
    float2 q = {p};
    float amp = 0.5;
    [unroll] for (int oc = 0; oc < {n}; oc++)
    {{
        float2 i = floor(q);
        float2 f = frac(q);
        f = f * f * (3.0 - 2.0 * f);
{corners}
        {o} += amp * lerp(lerp(h00, h10, f.x), lerp(h01, h11, f.x), f.y);
        q = q * 2.03 + float2(17.13, 9.71);
        amp *= 0.5;
    }}
}}
'''.format(o=out, p=point, n=octaves, corners=corners)


def _strokes(cell, u, v):
    """Pseudo-script glyph in a cell: three vertical and three horizontal strokes switched by a per-cell hash."""
    return _hash('gh', 'float2(%s, Dyn.y * 17.0)' % cell) + '''
float g = 0.0;
g += (1.0 - smoothstep(0.06, 0.12, abs({u} - 0.5))) * (gh > 0.2 ? 1.0 : 0.0) * step(0.1, {v}) * step({v}, 0.9);
g += (1.0 - smoothstep(0.05, 0.10, abs({u} - 0.25))) * step(0.5, frac(gh * 7.0)) * step(0.1, {v}) * step({v}, 0.6);
g += (1.0 - smoothstep(0.05, 0.10, abs({u} - 0.75))) * step(0.5, frac(gh * 13.0)) * step(0.4, {v}) * step({v}, 0.9);
g += (1.0 - smoothstep(0.06, 0.12, abs({v} - 0.2))) * step(0.5, frac(gh * 3.0)) * step(0.2, {u}) * step({u}, 0.8);
g += (1.0 - smoothstep(0.06, 0.12, abs({v} - 0.5))) * step(0.5, frac(gh * 5.0)) * step(0.15, {u}) * step({u}, 0.85);
g += (1.0 - smoothstep(0.06, 0.12, abs({v} - 0.8))) * step(0.5, frac(gh * 11.0)) * step(0.2, {u}) * step({u}, 0.8);
'''.format(u=u, v=v)


def _line(x, w):
    return '(1.0 - smoothstep({w}, {w} + aa, abs({x})))'.format(x=x, w=w)


def _capsule(out, px, py, r1, r2, h):
    """Signed distance to a capsule along +y from (0,0) with radius r1 to (0,h) with radius r2 (an uneven capsule), as a
    new float `out`. The masks are single Custom-node bodies, so this is inlined rather than a function."""
    return ('float2 {o}_p = float2(abs({x}), {y});\n'
            'float {o}_b = ({r1} - {r2}) / {h};\n'
            'float {o}_a = sqrt(1.0 - {o}_b * {o}_b);\n'
            'float {o}_k = dot({o}_p, float2(-{o}_b, {o}_a));\n'
            'float {o} = {o}_k < 0.0 ? length({o}_p) - {r1} : ({o}_k > {o}_a * {h} ? length({o}_p - float2(0.0, {h})) - {r2}'
            ' : dot({o}_p, float2({o}_a, {o}_b)) - {r1});\n').format(o=out, x=px, y=py, r1=r1, r2=r2, h=h)


# Brush characters as (x0, y0, x1, y1, start width, end width) strokes in writing order, y up in [-1, 1].
CHARACTERS = (
    ('斩', ((-0.80, 0.62, -0.14, 0.70, 0.060, 0.055), (-0.40, 0.62, -0.70, 0.12, 0.055, 0.050),
           (-0.70, 0.12, -0.12, 0.20, 0.050, 0.055), (-0.42, 0.45, -0.42, -0.95, 0.065, 0.045),
           (-0.86, -0.30, -0.06, -0.12, 0.060, 0.030), (0.74, 0.90, 0.22, 0.64, 0.065, 0.030),
           (0.24, 0.64, 0.21, -0.06, 0.060, 0.055), (0.21, -0.06, 0.00, -0.82, 0.055, 0.015),
           (0.24, 0.14, 0.88, 0.20, 0.055, 0.060), (0.58, 0.16, 0.58, -0.95, 0.070, 0.050))),
    ('封', ((-0.72, 0.62, -0.22, 0.66, 0.050, 0.050), (-0.47, 0.90, -0.47, 0.20, 0.060, 0.050),
           (-0.80, 0.24, -0.14, 0.30, 0.055, 0.055), (-0.72, -0.14, -0.20, -0.10, 0.050, 0.050),
           (-0.47, 0.14, -0.47, -0.64, 0.060, 0.050), (-0.86, -0.64, -0.10, -0.48, 0.060, 0.030),
           (0.06, 0.40, 0.90, 0.46, 0.055, 0.060), (0.62, 0.88, 0.62, -0.80, 0.070, 0.055),
           (0.62, -0.80, 0.44, -0.66, 0.055, 0.020), (0.26, 0.10, 0.40, -0.12, 0.045, 0.070))),
    ('星', ((-0.40, 0.90, -0.40, 0.30, 0.055, 0.050), (-0.40, 0.90, 0.40, 0.92, 0.050, 0.055),
           (0.40, 0.92, 0.38, 0.30, 0.060, 0.050), (-0.38, 0.61, 0.38, 0.62, 0.045, 0.045),
           (-0.38, 0.32, 0.38, 0.33, 0.050, 0.050), (-0.36, 0.20, -0.62, -0.26, 0.055, 0.025),
           (-0.46, -0.04, 0.56, 0.00, 0.050, 0.055), (-0.36, -0.42, 0.48, -0.38, 0.050, 0.050),
           (0.05, 0.22, 0.05, -0.86, 0.065, 0.055), (-0.86, -0.86, 0.88, -0.82, 0.060, 0.065))),
    ('灭', ((-0.80, 0.70, 0.82, 0.76, 0.060, 0.065), (-0.56, 0.30, -0.38, 0.04, 0.045, 0.070),
           (0.58, 0.40, 0.40, 0.12, 0.060, 0.035), (0.06, 0.62, 0.02, -0.06, 0.065, 0.060),
           (0.02, -0.06, -0.74, -0.88, 0.060, 0.015), (0.04, -0.14, 0.86, -0.86, 0.040, 0.090))),
)


def _character(strokes):
    """Brush-written character: Dyn.z writes the strokes in order, each dry-brushed toward its end."""
    n1 = len(strokes) + 1
    code = ''
    for index, (x0, y0, x1, y1, w0, w1) in enumerate(strokes):
        bx, by = x1 - x0, y1 - y0
        code += '''    {{
        float2 pa = d - float2({x0:.3f}, {y0:.3f});
        float h = saturate(dot(pa, float2({bx:.3f}, {by:.3f})) * {inv:.4f});
        float wd = lerp({w0:.3f}, {w1:.3f}, h) * (0.85 + 0.3 * nb);
        float pr = saturate(Dyn.z * {n1:.1f} - {i:.1f});
        float shown = pr >= 1.0 ? 1.0 : 1.0 - smoothstep(-0.02, 0.0, h - pr);
        float dry = 0.1 + 0.45 * h * h;
        m = max(m, (1.0 - smoothstep(wd - aa, wd, length(pa - float2({bx:.3f}, {by:.3f}) * h))) * shown * smoothstep(dry - 0.1, dry + 0.1, nb + 0.15));
    }}
'''.format(x0=x0, y0=y0, bx=bx, by=by, inv=1.0 / (bx * bx + by * by), w0=w0, w1=w1, n1=n1, i=index)
    return code


MASKS = {
    # Soft orb with a white-hot centre.
    'M_SS_Glow': '''
float r2 = dot(d, d);
float soft = saturate(1.0 - r2);
soft *= soft;
float core = exp2(-r2 * 16.0);
col = lerp(Tint, white, core * 0.45) * (soft * 0.8 + core * 0.7);
cover = soft;
''',
    # Shock ring; Dyn.w thickens it.
    'M_SS_Ring': '''
float r = length(d);
float w = lerp(0.035, 0.22, saturate(Dyn.w));
float rc = 0.95 - w;
float x = (r - rc) / w;
float band = exp2(-x * x * 2.5);
float edge = exp2(-x * x * 20.0);
float inner = saturate(r / rc);
inner = inner * inner * inner * inner * (r < rc ? 0.14 : 0.0);
float fade = saturate((1.0 - r) * 14.0);
col = (lerp(Tint, white, edge * 0.35) * (band * 0.8 + edge * 0.6) + Tint * inner) * fade;
cover = (band * 0.9 + inner) * fade;
''',
    # Sci-fi seal for the mechanical form; Dyn.w > 0.5 draws a single data bit instead.
    'M_SS_Glyph': '''
float r = length(d);
float a = atan2(d.y, d.x);
float aa = fwidth(r) * 1.5 + 0.003;
float m = 0.0;
if (Dyn.w > 0.5)
{
    float sq = max(abs(d.x), abs(d.y));
    float af = fwidth(sq) * 1.5 + 0.003;
    m = (sq < 0.62 ? 0.35 : 0.0) + (1.0 - smoothstep(0.07, 0.07 + af, abs(sq - 0.62))) + exp2(-dot(d, d) * 30.0) * 0.6;
}
else
{
    float spin = Clock * (frac(Dyn.y * 5.3) > 0.5 ? 0.8 : -0.8);
    float dash = frac((a + spin) * 3.1830989) < 0.62 ? 1.0 : 0.0;
    float sq = max(abs(d.x), abs(d.y));
    m += ''' + _line('r - 0.88', '0.02') + ''' * dash;
    m += ''' + _line('r - 0.97', '0.007') + ''' * 0.8;
    m += ''' + _line('sq - 0.64', '0.012') + ''' * (min(abs(d.x), abs(d.y)) > 0.38 ? 1.0 : 0.0);
    m += ''' + _line('r - 0.42', '0.01') + ''' * 0.7;
    m += ''' + _line('min(abs(d.x), abs(d.y))', '0.012') + ''' * (r > 0.47 && r < 0.6 ? 1.0 : 0.0);
    m += (1.0 - smoothstep(0.12, 0.12 + aa, abs(d.x) + abs(d.y))) * 0.9;
    float scanA = frac((a - Clock * 4.0) * 0.15915494);
    float scan = saturate(1.0 - scanA * 4.0);
    m += scan * scan * (r > 0.44 && r < 0.86 ? 0.5 : 0.0);
}
float halo = exp2(-r * r * 3.0) * (r < 1.0 ? 0.12 : 0.0);
col = lerp(Tint, white, saturate(m - 0.6) * 0.3) * (m + halo);
cover = saturate(m) * 0.8 + halo;
''',
    # Four-point star flare with faint diagonal rays.
    'M_SS_Star': '''
float r2 = dot(d, d);
float ax = abs(d.x);
float ay = abs(d.y);
float rays = exp2(-ay * ay * 700.0) * pow(saturate(1.0 - ax), 2.2) + exp2(-ax * ax * 700.0) * pow(saturate(1.0 - ay), 2.2);
float2 e = float2(d.x + d.y, d.x - d.y) * 0.70710678;
float diag = (exp2(-e.y * e.y * 1400.0) * pow(saturate(1.0 - abs(e.x) * 1.7), 2.0) + exp2(-e.x * e.x * 1400.0) * pow(saturate(1.0 - abs(e.y) * 1.7), 2.0)) * 0.5;
float core = exp2(-r2 * 35.0);
float halo = pow(saturate(1.0 - sqrt(r2)), 3.0) * 0.4;
col = lerp(Tint, white, saturate(core * 0.75 + rays * 0.25)) * (rays + diag + core * 1.4 + halo);
cover = saturate(halo * 1.5 + core);
''',
    # Crescent sword-light bulging along +y (the sprite alignment). Dyn.z sweeps it in from one tip;
    # |Dyn.w| is the thickness and a negative Dyn.w shows the whole arc at once.
    'M_SS_Crescent': '''
float yA = 0.85 - lerp(0.12, 0.5, saturate(abs(Dyn.w)));
float c2 = (yA * yA - 1.105) / (2.0 * yA + 0.9);
float R2 = yA - c2;
float so = 0.997 - length(d - float2(0.0, -0.147));
float si = length(d - float2(0.0, c2)) - R2;
float k = saturate(si / max(so + si, 0.001));
float phi = atan2(d.x, d.y + 0.147);
float s = saturate(phi / 3.76 + 0.5);
if (frac(Dyn.y * 7.13) > 0.5) s = 1.0 - s;
float sweep = 1.0;
if (Dyn.w >= 0.0)
{
    float h = Dyn.z * 1.7;
    sweep = (1.0 - smoothstep(h - 0.08, h, s)) * saturate(1.0 - (h - s) * 0.7);
}
float tips = saturate((1.0 - abs(s * 2.0 - 1.0)) * 5.0);
float aa = fwidth(so) * 1.5 + 0.001;
float body = smoothstep(0.0, aa, so) * smoothstep(0.0, aa, si);
float outer = si > 0.0 ? 1.0 : 0.0;
float edge = exp2(-so * so * 900.0) * outer;
float lines = 0.8 + 0.2 * sin(k * 38.0 + Dyn.y * 17.0);
float halo = exp2(-max(-so, 0.0) * 22.0) * outer * (1.0 - body) * 0.35;
float m = (body * (0.5 + 0.7 * pow(k, 1.3)) * lines + edge * 1.1 + halo) * tips * sweep;
col = lerp(Tint, white, saturate(edge * 0.4 + k * k * 0.06)) * m;
cover = body * (0.75 + 0.25 * k) * tips * sweep;
''',
    # Teardrop pointing along +y: a clean lotus petal at Dyn.w 0, a flickering flame or soul wisp toward 1.
    'M_SS_Wisp': '''
float t = (d.y + 0.9) / 1.85;
float tt = saturate(t);
float turb = saturate(Dyn.w);
''' + _fbm('nz', 'float2(d.y * 2.5 - Clock * 3.0 + Dyn.y * 11.0, d.x * 1.5 + Dyn.y * 5.0)', 2) + '''
float x = d.x + (nz - 0.5) * turb * 0.7 * tt;
float w = 0.58 * pow(max(sin(3.14159265 * pow(tt, 0.62)), 0.0), 0.85) + 0.001;
float inside = (t > 0.0 && t < 1.0) ? 1.0 : 0.0;
float body = (1.0 - smoothstep(w * 0.55, w, abs(x))) * inside;
float core = exp2(-(x * x) / (w * w * 0.18)) * pow(1.0 - tt, 0.6) * inside;
float vein = (1.0 - smoothstep(0.012, 0.03, abs(x))) * inside * (1.0 - turb) * 0.4;
col = lerp(Tint, white, saturate(core * 0.5)) * (body * 0.8 + core * 0.7 + vein);
cover = body;
''',
    # Faceted flake, long along y (gold leaf).
    'M_SS_Shard': '''
float e = abs(d.x) / 0.38 + abs(d.y) / 0.95;
float aa = fwidth(e) * 1.5 + 0.001;
float fill = 1.0 - smoothstep(1.0 - aa, 1.0, e);
float z = (1.0 - e) * 9.0;
float rim = exp2(-z * z) * fill;
float facet = d.x > 0.0 ? 0.9 : 0.45;
float ridge = (1.0 - smoothstep(0.01, 0.04, abs(d.x))) * fill * 0.6;
float halo = exp2(-max(e - 1.0, 0.0) * 6.0) * (1.0 - fill) * 0.25;
col = lerp(Tint, white, saturate(rim * 0.45 + ridge * 0.6)) * (fill * facet * 0.6 + rim * 0.8 + ridge + halo);
cover = fill * 0.85 + halo;
''',
    # Light streak, head toward +y; Dyn.w > 0 adds energy flowing toward the head at that speed.
    'M_SS_Streak': '''
float ax = abs(d.x);
float taper = sqrt(saturate(1.0 - abs(d.y)));
float head = saturate(0.6 + 0.4 * d.y);
float soft = exp2(-ax * ax * 5.0);
float core = exp2(-ax * ax * 70.0);
float flow = 1.0;
if (Dyn.w > 0.0)
{
''' + _fbm('n', 'float2(d.x * 2.5 + Dyn.y * 7.0, d.y * 3.0 - Clock * Dyn.w)', 2) + '''
    flow = 0.4 + 1.1 * n;
}
col = lerp(Tint, white, core * 0.5) * (soft * 0.65 * flow + core * 0.9) * taper * head;
cover = soft * taper * flow * 0.8;
''',
    # Flying-sword silhouette, tip toward +y, with a spine and edge.
    'M_SS_Sword': '''
float x = abs(d.x);
float y = d.y;
float wy = y < 0.72 ? 0.085 : 0.085 * saturate((0.98 - y) / 0.26);
float dBlade = max(x - wy, max(-0.42 - y, y - 0.98));
float2 qg = abs(d - float2(0.0, -0.47)) - float2(0.30, 0.05);
float dGuard = length(max(qg, 0.0)) + min(max(qg.x, qg.y), 0.0);
float2 qh = abs(d - float2(0.0, -0.69)) - float2(0.04, 0.17);
float dGrip = length(max(qh, 0.0)) + min(max(qh.x, qh.y), 0.0);
float dPommel = length(d - float2(0.0, -0.9)) - 0.065;
float dist = min(min(dBlade, dGuard), min(dGrip, dPommel));
float aa = fwidth(dist) * 1.2 + 0.001;
float fill = 1.0 - smoothstep(0.0, aa, dist);
float edge = exp2(-dist * dist * 6000.0);
float spine = (1.0 - smoothstep(0.008, 0.02, x)) * (y > -0.42 && y < 0.9 ? 1.0 : 0.0);
float halo = exp2(-max(dist, 0.0) * 14.0) * 0.45;
col = lerp(Tint, white, saturate(edge * 0.4 + spine * 0.5)) * (fill * 0.65 + edge * 0.8 + spine * 0.7 * fill + halo);
cover = fill * 0.8 + halo * 0.5;
''',

    # ------------------------------------------------------------ ink wash
    # Brush crescent: bristles dry out toward the lifted end, the inner edge is ragged and drops fly off the outer
    # edge. Geometry and Dyn.y/|Dyn.w| as Crescent; Dyn.z draws the stroke in (1 = whole), a negative Dyn.w shows it all.
    'M_SS_InkArc': '''
float th = saturate(frac(abs(Dyn.w)));
float streaky = abs(Dyn.w) >= 1.0 ? 1.0 : 0.0;
float yA = 0.85 - lerp(0.12, 0.5, th);
float c2 = (yA * yA - 1.105) / (2.0 * yA + 0.9);
float R2 = yA - c2;
''' + _fbm('wb', 'd * 1.3 + Dyn.y * 9.0', 2) + '''
float2 q = d + float2(wb - 0.5, 0.5 - wb) * 0.09;
float phi = atan2(q.x, q.y + 0.147);
float s0 = saturate(phi / 3.76 + 0.5);
float s = frac(Dyn.y * 7.13) > 0.5 ? 1.0 - s0 : s0;
''' + _fbm('ne', 'float2(s0 * 9.0 + Dyn.y * 11.0, Dyn.y * 5.0)', 2) + _fbm('np', 'float2(s0 * 3.0 + Dyn.y * 4.0, 2.7)', 2) + '''
float so = 0.997 - length(q - float2(0.0, -0.147)) + (ne - 0.5) * 0.04;
float si = length(q - float2(0.0, c2)) - R2 + (ne - 0.5) * 0.12 + (np - 0.5) * 0.16 - s * s * 0.08;
float k = saturate(si / max(so + si, 0.001));
float aa = fwidth(so) * 1.5 + 0.001;
float body = smoothstep(0.0, aa, so) * smoothstep(0.0, aa * 2.0, si);
''' + _fbm('nb', 'float2(k * 4.5 + Dyn.y * 3.0, s0 * 1.6 + Dyn.y * 13.0)', 3) + '''
float dry = lerp(0.0, 0.62, pow(s, 1.4)) + (1.0 - k) * 0.15 + streaky * 0.3;
// Bristle tracks run along the stroke; the drier the brush the more they part into flying white (飞白).
float bristle = sin(k * 52.0 + (nb - 0.5) * 6.0 + Dyn.y * 17.0) * 0.5 + 0.5;
float ink = smoothstep(dry - 0.06, dry + 0.06, nb + 0.1 + (bristle - 0.5) * (0.12 + 0.4 * s));
float pool = exp2(-s * s * 90.0) * 0.3 * body;
float bleed = exp2(-max(-so, 0.0) * 30.0) * (1.0 - body) * (si > 0.0 ? 1.0 : 0.0) * (1.0 - s) * 0.3 * (1.0 - streaky);
float sweep = 1.0;
if (Dyn.w >= 0.0)
{
    float h = Dyn.z * 1.1;
    sweep = 1.0 - smoothstep(h - 0.06, h, s);
}
float tips = saturate(s * 14.0) * saturate((1.0 - s) * 3.0 + 0.15);
float cell = floor(s0 * 16.0);
''' + _hash('hd', 'float2(cell, Dyn.y * 23.0)') + _hash('hr', 'float2(cell * 1.7, Dyn.y * 41.0 + 3.0)') + '''
float phc = ((cell + 0.5 + (hr - 0.5) * 0.5) / 16.0 - 0.5) * 3.76;
float2 pc = float2(sin(phc), cos(phc)) * (1.03 + hr * 0.1) - float2(0.0, 0.147);
float drop = (hd > 0.5 ? 1.0 : 0.0) * (1.0 - smoothstep(0.0, aa, length(d - pc) - (0.014 + 0.03 * hd))) * (1.0 - streaky);
float m = saturate(body * ink * tips * 1.2 + pool + drop + bleed) * sweep;
col = Tint * m;
cover = m;
''',
    # Straight brush stroke along +y: a pressed head at +y tapering to a dry tail at -y. |Dyn.w| is the dryness and a
    # negative Dyn.w gives a round-ended segment (ink bolts); Dyn.z reveals it from the head back to the tail.
    'M_SS_InkStroke': '''
float t = d.y * 0.5 + 0.5;
float dry = saturate(abs(Dyn.w));
''' + _fbm('ne', 'float2(t * 5.0 + Dyn.y * 9.0, Dyn.y * 3.0)', 2) + '''
float hw = Dyn.w < 0.0
    ? 0.42 * sqrt(saturate(t * 8.0)) * sqrt(saturate((1.0 - t) * 8.0))
    : 0.46 * pow(saturate(t * 1.2), 0.7) * sqrt(saturate((1.0 - t) * 6.0));
hw *= 0.75 + 0.5 * ne;
float aa = fwidth(d.x) * 1.5 + 0.002;
float inside = 1.0 - smoothstep(hw - aa, hw, abs(d.x));
''' + _fbm('nb', 'float2(d.x / max(hw, 0.05) * 2.2 + Dyn.y * 17.0, t * 1.4 + Dyn.y * 5.0)', 3) + '''
float thr = dry * lerp(0.8, 0.2, t);
float bristle = sin(d.x / max(hw, 0.02) * 16.0 + (nb - 0.5) * 5.0 + Dyn.y * 11.0) * 0.5 + 0.5;
float ink = smoothstep(thr - 0.06, thr + 0.06, nb + 0.12 - abs(d.x) / max(hw, 0.01) * 0.12 + (bristle - 0.5) * (0.1 + 0.45 * dry * (1.0 - t)));
float rv = 1.0 - saturate(Dyn.z) * 1.1;
float m = saturate(inside * ink * 1.15) * smoothstep(rv - 0.06, rv, t);
col = Tint * m;
cover = m;
''',
    # Brush ribbon (U along the trail, V across): ragged edges and bristles that dry out with particle age (Dyn.z).
    'M_SS_InkRibbon': '''
float v = abs(UV.y * 2.0 - 1.0);
float age = saturate(Dyn.z);
''' + _fbm('ne', 'float2(UV.x * 14.0 + Dyn.y * 7.0, UV.y > 0.5 ? 1.7 : 5.3)', 2) + '''
float hw = 0.97 - ne * 0.35 - age * 0.3;
float inside = 1.0 - smoothstep(hw - 0.05, hw, v);
''' + _fbm('nb', 'float2(UV.x * 2.0 + Dyn.y * 3.0, UV.y * 4.5)', 3) + '''
float dry = age * 0.75 + v * 0.2 - 0.05;
float bristle = sin(UV.y * 44.0 + (nb - 0.5) * 6.0) * 0.5 + 0.5;
float ink = smoothstep(dry - 0.06, dry + 0.06, nb + 0.12 + (bristle - 0.5) * (0.1 + 0.5 * age));
float caps = saturate(UV.x * 12.0) * saturate((1.0 - UV.x) * 12.0);
float m = saturate(inside * ink * 1.15) * caps;
col = Tint * m;
cover = m;
''',
    # Ink splash. Dyn.w < 0.5: a lobed blot with spikes, end drops and satellite drops; otherwise one drop with a
    # short tail toward -y. Dyn.z (drying) pales the inside so the darker rim shows.
    'M_SS_InkSplat': '''
float r = length(d);
float aa = fwidth(r) * 1.5 + 0.002;
float m = 0.0;
if (Dyn.w < 0.5)
{
    float a = atan2(d.y, d.x);
    float an = a * 0.15915494 + 0.5;
''' + _fbm('nb', 'float2(cos(a), sin(a)) * 1.8 + Dyn.y * 17.0', 3) + _fbm('nf', 'd * 7.0 + Dyn.y * 5.0', 2) + '''
    float R0 = 0.28 + 0.26 * nb + (nf - 0.5) * 0.06;
    float blob = 1.0 - smoothstep(R0 - aa, R0, r);
    float sec = floor(an * 13.0);
''' + _hash('hs', 'float2(sec, Dyn.y * 19.0)') + _hash('hj', 'float2(sec * 3.1, Dyn.y * 7.0 + 1.0)') + '''
    float sa = ((sec + 0.5 + (hj - 0.5) * 0.3) / 13.0 - 0.5) * 6.2831853;
    float2 dir = float2(cos(sa), sin(sa));
    float along = dot(d, dir);
    float across = abs(dot(d, float2(-dir.y, dir.x)));
    float L = hs > 0.35 ? 0.08 + 0.34 * hs : 0.0;
    float sw = 0.085 * pow(saturate(1.0 - (along - R0 * 0.7) / max(L + 0.06, 0.01)), 0.7) * (0.6 + 0.6 * hj);
    float spike = (along > 0.0 && along < R0 + L) ? 1.0 - smoothstep(sw - aa, sw, across) : 0.0;
    float tipDrop = L > 0.0 ? 1.0 - smoothstep(-aa, 0.0, length(d - dir * (R0 + L + 0.04 + 0.03 * hj)) - (0.03 + 0.04 * hs)) : 0.0;
    float sec2 = floor(an * 21.0);
''' + _hash('h2', 'float2(sec2 * 1.3, Dyn.y * 29.0 + 5.0)') + _hash('h3', 'float2(sec2 * 0.7, Dyn.y * 13.0 + 9.0)') + '''
    float sa2 = ((sec2 + 0.5) / 21.0 - 0.5) * 6.2831853;
    float2 p2 = float2(cos(sa2), sin(sa2)) * (0.55 + 0.38 * h3);
    float sat = h2 > 0.55 ? 1.0 - smoothstep(-aa, 0.0, length(d - p2) - (0.012 + 0.036 * h2 * (1.0 - h3 * 0.5))) : 0.0;
    float rx = (r - R0) * 30.0;
    float inner = blob * (0.84 + 0.16 * nf - 0.3 * saturate(Dyn.z) * (1.0 - saturate(r / R0)) + exp2(-rx * rx) * 0.3);
    m = saturate(max(inner, max(spike, max(tipDrop, sat))));
}
else
{
''' + _fbm('nd', 'd * 3.0 + Dyn.y * 9.0', 2) + '''
    float e = length(float2(d.x, d.y * 0.85 + 0.08)) - (0.6 + (nd - 0.5) * 0.12);
    float tail = (d.y < -0.2 && d.y > -0.95) ? 1.0 - smoothstep(0.0, aa, abs(d.x) - 0.12 * saturate((d.y + 0.95) / 0.75)) : 0.0;
    m = max(1.0 - smoothstep(-aa, 0.0, e), tail) * (0.88 + 0.12 * nd);
}
col = Tint * m;
cover = m;
''',
    # Ink diffusing on wet paper: domain-warped fbm whose pigment gathers in darker filaments (墨丝), a darker tide line
    # where the wash dries (Dyn.w 1 leaves it out), capillary feathering (洇) along the paper fibres at the thin edge and
    # granulation; Dyn.z erodes it from the thin parts so it pales and breaks up as it dries.
    'M_SS_InkWash': '''
float r = length(d);
float2 sd = float2(Dyn.y * 17.0, Dyn.y * 9.0);
''' + _fbm('w1', 'd * 1.3 + sd + float2(Clock * 0.05, 0.0)', 2) + _fbm('w2', 'd * 1.3 + sd.yx + float2(0.0, -Clock * 0.04)', 2)
    + _fbm('n', 'd * 1.7 + float2(w1, w2) * 1.6 + sd', 5)
    + _fbm('fl', 'd * 3.4 + float2(w2, w1) * 2.4 + sd.yx', 3) + '''
float shape = saturate(1.0 - r * r * 1.15);
float dens = saturate(shape * 1.3 + (n - 0.5) * 1.5 - Dyn.z * 0.95);
float body = smoothstep(0.05, 0.62, dens);
float vein = 1.0 - abs(fl * 2.0 - 1.0);
vein = vein * vein * vein * vein * smoothstep(0.08, 0.4, dens);
float2 radial = d / max(r, 0.001);
''' + _fbm('fb', 'float2(dot(d, float2(-radial.y, radial.x)) * 22.0, r * 2.6) + sd', 2) + '''
float feather = smoothstep(0.58, 0.92, fb) * smoothstep(0.0, 0.2, dens) * (1.0 - smoothstep(0.22, 0.5, dens));
float tx = (dens - 0.1) * 12.0;
float tide = exp2(-tx * tx) * (1.0 - saturate(Dyn.w));
''' + _hash('gr', 'floor(d * 48.0) + sd') + '''
float m = saturate(body * (0.52 + 0.12 * gr) + vein * 0.38 + tide * 0.35 + feather * 0.42) * saturate((1.0 - r) * 4.0);
col = Tint * (1.0 - 0.6 * saturate(tide * 2.0)) * (1.0 - 0.45 * vein) * m;
cover = m;
''',
    # Ink billowing in water (墨云): a domain-warped volume with dense dark cores, thinner translucent lobes and curling
    # wisps at the fringe. Light from above catches the upper surfaces as a cool grey sheen (Dyn.w is its strength), so the
    # cloud reads as volume instead of a flat blot. Dyn.z thins and spreads it with age.
    'M_SS_InkPlume': '''
float r = length(d);
float2 sd = float2(Dyn.y * 23.0, Dyn.y * 11.0);
float grow = saturate(Dyn.z);
''' + _fbm('w1', 'd * 1.2 + sd + float2(Clock * 0.06, grow * 0.6)', 2) + _fbm('w2', 'd * 1.2 + sd.yx + float2(-grow * 0.5, -Clock * 0.05)', 2)
    + _fbm('n', 'd * 1.5 + float2(w1, w2) * 1.8 + sd', 5) + '''
float shape = saturate(1.0 - r * r * 1.05);
float dens = saturate(shape * 1.45 + (n - 0.52) * 1.8 - grow * 0.75);
float body = smoothstep(0.06, 0.66, dens) * saturate((1.0 - r) * 3.0);
float a = atan2(d.y, d.x);
float curl = sin(a * 5.0 + r * 13.0 + (n - 0.5) * 7.0 - Clock * 1.1 + Dyn.y * 9.0) * 0.5 + 0.5;
float wisp = smoothstep(0.8, 0.98, curl) * smoothstep(0.02, 0.16, dens) * (1.0 - smoothstep(0.16, 0.42, dens)) * saturate((1.0 - r) * 3.0);
float core = pow(saturate(dens), 2.2);
float top = saturate(0.45 + d.y * 0.65 + (n - 0.5) * 0.9);
float3 sheen = float3(0.3, 0.31, 0.34) * top * top * (1.0 - core) * saturate(Dyn.w);
col = (Tint * (0.6 + 0.8 * (1.0 - core)) + sheen) * saturate(body + wisp * 0.6);
cover = saturate(body * (0.72 + 0.28 * core) + wisp * 0.5);
''',
    # Ensō (圆相): a brush circle with a gap, pressed where Dyn.y says and thinning to a dry tail. Dyn.w is the brush
    # width and Dyn.z draws it round.
    'M_SS_Enso': '''
float r = length(d);
float a = frac(atan2(d.y, d.x) * 0.15915494 + 0.5 + Dyn.y);
float u = a / 0.9;
float live = u < 1.0 ? 1.0 : 0.0;
''' + _fbm('nr', 'float2(a * 6.0, Dyn.y * 7.0)', 2) + '''
float w = lerp(0.035, 0.16, saturate(Dyn.w)) * lerp(1.0, 0.3, pow(saturate(u), 1.4)) * (0.55 + 0.45 * saturate(u * 25.0));
float rc = 0.8 + (nr - 0.5) * 0.07 + u * 0.035;
float x = (r - rc) / max(w, 0.002);
float aa = fwidth(r) * 1.5 / max(w, 0.002) + 0.02;
float band = 1.0 - smoothstep(1.0 - aa, 1.0, abs(x));
''' + _fbm('nb', 'float2(a * 10.0 + Dyn.y * 11.0, x * 2.2 + Dyn.y * 5.0)', 3) + '''
float dry = lerp(0.04, 0.6, pow(saturate(u), 1.3));
float ink = smoothstep(dry - 0.1, dry + 0.1, nb + 0.12);
float rv = saturate(Dyn.z) * 1.08;
float m = band * ink * live * (1.0 - smoothstep(rv - 0.04, rv, u)) * (0.85 + 0.15 * nb);
col = Tint * m;
cover = m;
''',
    # Ink formation. Dyn.w < 0.5: eight trigrams between two brush rings around a taiji whose yin half is ink and
    # yang half bare; otherwise an ink vortex. Dyn.z < 1 draws it round.
    'M_SS_Bagua': '''
float r = length(d);
float a = atan2(d.y, d.x) * 0.15915494 + 0.5;
float aa = fwidth(r) * 1.5 + 0.002;
''' + _fbm('nb', 'float2(a * 24.0 + Dyn.y * 7.0, r * 9.0)', 3) + '''
float m = 0.0;
float wash = 0.0;
if (Dyn.w < 0.5)
{
    float wo = 0.045 * (0.7 + 0.6 * nb);
    m += 1.0 - smoothstep(wo - aa, wo, abs(r - 0.9));
    m += (1.0 - smoothstep(0.02, 0.02 + aa, abs(r - 0.47))) * 0.9;
    float sec = floor(a * 8.0);
    float su = (frac(a * 8.0) - 0.5) * 0.78539816;
    [unroll] for (int b = 0; b < 3; b++)
    {
        float rb = 0.58 + 0.085 * b;
        float tu = su * rb;
        float bit = fmod(floor(sec / exp2((float)b)), 2.0);
        float bar = (1.0 - smoothstep(0.032 - aa, 0.032, abs(r - rb))) * (1.0 - smoothstep(0.12, 0.14, abs(tu)));
        m += bar * (bit > 0.5 ? 1.0 : smoothstep(0.018, 0.032, abs(tu)));
    }
    float tc = cos(Clock * 0.8);
    float ts = sin(Clock * 0.8);
    float2 t = float2(d.x * tc - d.y * ts, d.x * ts + d.y * tc) / 0.3;
    float yin = t.x < 0.0 ? 1.0 : 0.0;
    if (length(t - float2(0.0, 0.5)) < 0.5) yin = 1.0;
    if (length(t - float2(0.0, -0.5)) < 0.5) yin = 0.0;
    if (length(t - float2(0.0, 0.5)) < 0.15) yin = 0.0;
    if (length(t - float2(0.0, -0.5)) < 0.15) yin = 1.0;
    float lt = length(t);
    m += (lt < 1.0 ? yin : 0.0) + (1.0 - smoothstep(0.035, 0.07, abs(lt - 1.0))) * 0.9;
    float wx = (r - 0.66) * 5.0;
    wash = exp2(-wx * wx) * 0.2;
}
else
{
    float arms = cos(a * 18.849556 + log(max(r, 0.02)) * 6.0 - Clock * 5.0);
    m += smoothstep(0.55, 0.9, arms * 0.5 + 0.5) * smoothstep(0.03, 0.3, r) * (1.0 - smoothstep(0.7, 0.98, r));
    m += exp2(-r * r * 40.0);
    wash = (1.0 - smoothstep(0.3, 0.98, r)) * 0.25;
}
m = saturate(m) * smoothstep(0.14, 0.3, nb + 0.12);
if (Dyn.z < 0.999)
{
    float h = Dyn.z * 1.1;
    float rv = 1.0 - smoothstep(h - 0.05, h, a);
    m *= rv;
    wash *= rv;
}
float edgeFade = 1.0 - smoothstep(0.96, 1.0, r);
col = (Tint * m + lerp(Tint, float3(0.13, 0.13, 0.14), 0.7) * wash) * edgeFade;
cover = (m + wash) * edgeFade;
''',
    # Brush calligraphy written stroke by stroke with Dyn.z. Dyn.w picks 斩 (0), 封 (1), 星 (2) or 灭 (3).
    'M_SS_Calli': '''
float aa = fwidth(d.x) * 1.5 + 0.003;
''' + _fbm('nb', 'd * 5.0 + Dyn.y * 3.0', 3) + '''
float m = 0.0;
if (Dyn.w < 0.5)
{
''' + _character(CHARACTERS[0][1]) + '''}
else if (Dyn.w < 1.5)
{
''' + _character(CHARACTERS[1][1]) + '''}
else if (Dyn.w < 2.5)
{
''' + _character(CHARACTERS[2][1]) + '''}
else
{
''' + _character(CHARACTERS[3][1]) + '''}
col = Tint * m;
cover = m;
''',
    # Talisman (符箓): yellow paper with a cinnabar border and script; Dyn.z burns it from the bottom up behind a
    # charred band and an ember edge in the particle colour.
    'M_SS_Talisman': '''
float aa = fwidth(d.y) * 1.5 + 0.003;
float paper = (1.0 - smoothstep(0.9 - aa, 0.9, abs(d.x))) * (1.0 - smoothstep(0.95 - aa, 0.95, abs(d.y)));
''' + _fbm('nb', 'd * float2(2.5, 3.5) + Dyn.y * 7.0', 3) + '''
float front = lerp(-1.25, 1.25, saturate(Dyn.z)) + (nb - 0.5) * 0.45;
float intact = smoothstep(front - 0.015, front + 0.015, d.y);
float cx = (d.y - front) / 0.14;
float charred = exp2(-cx * cx) * intact;
float ex = (d.y - front) / 0.035;
float ember = exp2(-ex * ex) * paper * (Dyn.z > 0.001 ? 1.0 : 0.0);
float bx = max(abs(d.x) / 0.74, abs(d.y) / 0.86);
float border = 1.0 - smoothstep(0.012, 0.012 + aa, abs(bx - 1.0) * 0.8);
float glyph = 0.0;
float cv0 = (d.y + 0.72) / 0.48;
float cell = floor(cv0);
float cu = (d.x + 0.42) / 0.84;
float cv = frac(cv0);
if (cell >= 0.0 && cell < 3.0 && cu > 0.0 && cu < 1.0)
{
''' + _strokes('cell', 'cu', 'cv') + '''
    glyph = saturate(g);
}
float head = 1.0 - smoothstep(0.02, 0.02 + aa, abs(length(float2(d.x * 0.4, d.y - 0.79)) - 0.05));
float script = saturate(border * 0.8 + glyph + head) * intact * paper;
float body = paper * intact;
float3 paperCol = float3(0.78, 0.6, 0.2) * (0.92 + 0.08 * nb);
col = lerp(lerp(paperCol, float3(0.6, 0.03, 0.02), script), float3(0.05, 0.03, 0.02), saturate(charred * 0.9)) * body + Tint * ember * 2.0;
cover = body + ember * 0.3;
''',
    # Seal (印章): a worn square in the particle colour with four carved pseudo-characters, cut into the field (白文)
    # for Dyn.w < 0.5, otherwise standing inside a frame (朱文).
    'M_SS_Seal': '''
float2 p = d / 0.92;
float aa = fwidth(p.x) * 1.5 + 0.003;
''' + _fbm('ne', 'p * 4.0 + Dyn.y * 11.0', 2) + _fbm('nw', 'p * 5.0 + Dyn.y * 5.0', 2) + '''
float sq = max(abs(p.x), abs(p.y));
float edge = 0.93 + (ne - 0.5) * 0.07;
float field = 1.0 - smoothstep(edge - aa, edge, sq);
float2 gp = (p + 0.72) / 0.72;
float2 cell = floor(gp);
float2 cuv = frac(gp);
float carve = 0.0;
if (cell.x >= 0.0 && cell.x < 2.0 && cell.y >= 0.0 && cell.y < 2.0)
{
''' + _strokes('cell.x * 2.0 + cell.y + Dyn.w * 4.0', 'cuv.x', '1.0 - cuv.y') + '''
    carve = saturate(g);
}
float wear = smoothstep(0.16, 0.26, nw);
float m = 0.0;
if (Dyn.w < 0.5)
    m = field * (1.0 - carve) * wear;
else
    m = saturate((1.0 - smoothstep(0.055 - aa, 0.055, abs(sq - 0.85 - (ne - 0.5) * 0.04))) + carve * (sq < 0.78 ? 1.0 : 0.0)) * wear;
col = Tint * m;
cover = m;
''',
    # Plum blossom (梅花) in the particle colour with ink stamens; Dyn.w >= 0.5 draws a single falling petal.
    'M_SS_Blossom': '''
float r = length(d);
float aa = fwidth(r) * 1.5 + 0.002;
float a = atan2(d.y, d.x);
float m = 0.0;
float st = 0.0;
if (Dyn.w < 0.5)
{
    float re = 0.48 + 0.44 * sqrt(abs(cos(a * 2.5 + Dyn.y * 6.2831853)));
    m = (1.0 - smoothstep(re - aa, re, r)) * (0.72 + 0.28 * smoothstep(0.2, 0.95, r / re));
    float cs = (frac((a * 0.15915494 + 0.5) * 8.0 + Dyn.y) - 0.5) * 0.78539816 * r;
    float dots = 1.0 - smoothstep(0.035, 0.035 + aa, length(float2(cs, r - 0.34)));
    float stalks = (1.0 - smoothstep(0.006, 0.006 + aa, abs(cs))) * step(0.08, r) * step(r, 0.32);
    st = saturate(dots + stalks * 0.8 + (1.0 - smoothstep(0.07, 0.07 + aa, r)));
}
else
{
    float e = length(float2(d.x / 0.72, (d.y + 0.08) / 0.92));
    float notch = 1.0 - smoothstep(0.16, 0.16 + aa, length(d - float2(0.0, 0.9)));
    m = (1.0 - smoothstep(1.0 - aa * 2.0, 1.0, e)) * (1.0 - notch) * (0.7 + 0.3 * smoothstep(0.1, 1.0, e));
}
col = Tint * m * (1.0 - st) + float3(0.03, 0.02, 0.015) * st;
cover = saturate(m * (1.0 - st) + st);
''',
    # Auspicious cloud (祥云): four lobes with spiral curls, lined in the particle colour over a pale fill.
    'M_SS_Cloud': '''
float aa = fwidth(d.x) * 1.5 + 0.003;
float2 c0 = float2(0.0, 0.12);
float2 c1 = float2(-0.46, -0.06);
float2 c2 = float2(0.47, -0.02);
float2 c3 = float2(0.1, -0.3);
float dist = min(min(length(d - c0) - 0.4, length(d - c1) - 0.29), min(length(d - c2) - 0.31, length(d - c3) - 0.24));
dist = max(dist, -0.5 - d.y);
float fill = 1.0 - smoothstep(-aa, 0.0, dist);
float outline = 1.0 - smoothstep(0.022, 0.022 + aa, abs(dist + 0.022));
float curl = 0.0;
[unroll] for (int i = 0; i < 3; i++)
{
    float2 c = i == 0 ? c0 : (i == 1 ? c1 : c2);
    float rl = i == 0 ? 0.4 : (i == 1 ? 0.29 : 0.31);
    float2 q = (d - c) / rl;
    float rq = length(q);
    float th = atan2(q.y, q.x) * 0.15915494 + 0.5;
    th = i == 1 ? 1.0 - th : th;
    float sl = abs(frac(rq / 0.3 - th + 0.5) - 0.5) * 0.3 * rl;
    curl += (1.0 - smoothstep(0.014, 0.014 + aa, sl)) * step(0.12, rq) * step(rq, 0.7);
}
float lines = saturate(outline + curl * fill);
col = Tint * (lines + fill * 0.3 * (1.0 - lines));
cover = saturate(lines + fill * 0.32);
''',
    # Big Dipper star chart (北斗七星): ink constellation lines and rings with stars in the particle colour; Dyn.z
    # lights the stars in order from Dubhe to Alkaid and draws the lines after them.
    'M_SS_Dipper': '''
float r = length(d);
float aa = fwidth(r) * 1.5 + 0.002;
const float2 STAR[7] = { float2(0.806, 0.4375), float2(0.825, 0.062), float2(0.301, -0.127), float2(0.084, 0.107),
                         float2(-0.301, 0.032), float2(-0.602, -0.04), float2(-0.837, -0.433) };
const int LA[7] = { 0, 1, 2, 3, 3, 4, 5 };
const int LB[7] = { 1, 2, 3, 0, 4, 5, 6 };
float show = saturate(Dyn.z) * 8.0;
float stars = 0.0;
float core = 0.0;
float lines = 0.0;
[unroll] for (int i = 0; i < 7; i++)
{
    float2 q = d - STAR[i];
    float q2 = dot(q, q);
    float on = saturate(show - i) * (0.8 + 0.2 * sin(Clock * 3.0 + i * 1.7));
    core += exp2(-q2 * 2500.0) * on;
    stars += (exp2(-q2 * 600.0) + exp2(-q2 * 60.0) * 0.25 + exp2(-q.y * q.y * 5000.0 - q.x * q.x * 150.0) * 0.5
              + exp2(-q.x * q.x * 5000.0 - q.y * q.y * 150.0) * 0.5) * on;
}
[unroll] for (int j = 0; j < 7; j++)
{
    float2 pa = d - STAR[LA[j]];
    float2 ba = STAR[LB[j]] - STAR[LA[j]];
    float h = saturate(dot(pa, ba) / dot(ba, ba));
    float pr = saturate(show - max(LA[j], LB[j]));
    lines += (1.0 - smoothstep(0.006, 0.006 + aa, length(pa - ba * h))) * (h <= pr ? 1.0 : 0.0);
}
float ring = (1.0 - smoothstep(0.006, 0.006 + aa, abs(r - 0.95))) + (1.0 - smoothstep(0.003, 0.003 + aa, abs(r - 0.9))) * 0.6;
float ink = saturate(lines * 0.9 + ring * 0.8) * (1.0 - saturate(stars));
col = lerp(Tint, white, saturate(core)) * stars + float3(0.02, 0.02, 0.03) * ink;
cover = saturate(stars * 0.6 + ink);
''',
    # Violet serpent head (噬魂's devour) in side profile: snout toward +y, crown toward +x, mirrored when Dyn.y >= 0.5 so
    # the crown stays up on screen whichever way it strikes. A domed skull over a bowed upper lip with jaw muscles
    # bulging behind, a slimmer lower jaw and a neck narrower than the head; Dyn.z opens the jaws about the hinge. The
    # particle colour is the glow. Painted in violet ink: dense along the back, around the eye and where the pigment pools
    # at the wet edge, a paler violet wash over cheek and lip, faint scale rows, brush-hair streaks, a brushed silhouette
    # that bleeds into the paper and runs dry toward the neck. The open maw is a dark throat lit from deep inside, with
    # teeth along both lips, a fang and a flickering forked tongue. Over the ink, violet fire burns along the whole outline
    # and in patches over the skull, and a blood-red slit-pupil eye glows through it.
    'M_SS_Serpent': '''
float2 p = d;
if (Dyn.y >= 0.5) p.x = -p.x;
float open = saturate(Dyn.z);
float aa = fwidth(p.y) * 1.5 + 0.003;
float2 q = p - float2(0.0, -0.32);
float tu = 0.6 * open;
float tl = 0.55 * open;
float2 pu = float2(q.x * cos(tu) - q.y * sin(tu), q.x * sin(tu) + q.y * cos(tu));
float2 pl = float2(q.x * cos(tl) + q.y * sin(tl), -q.x * sin(tl) + q.y * cos(tl));
''' + _capsule('dMain', 'pu.x - 0.13 + 0.07 * saturate((pu.y + 0.12) / 0.8)', 'pu.y + 0.12', '0.27', '0.125', '0.8')
    + _capsule('dJawBar', 'pl.x + 0.05 - 0.025 * saturate((pl.y + 0.08) / 0.62)', 'pl.y + 0.08', '0.15', '0.055', '0.62')
    + _capsule('dNeck', 'q.x - 0.04 + 0.03 * q.y', '-(q.y + 0.1)', '0.205', '0.18', '0.95')
    + _fbm('nr', 'p * 5.0 + 3.7', 2) + _fbm('nk', 'float2(pu.x * 3.0, pu.y * 0.8) * 4.0 + 1.3', 2)
    + _fbm('ns', 'float2(p.x * 26.0, p.y * 2.2) + 7.1', 2) + _hash('ng', 'floor(p * 160.0)') + '''
float lipU = -0.045 - 0.035 * sin(saturate(pu.y / 0.75) * 3.14159265);
float lipL = 0.04 + 0.025 * sin(saturate(pl.y / 0.6) * 3.14159265);
float dSkull = max(min(dMain, length(pu - float2(0.09, -0.12)) - 0.29), lipU - pu.x);
float dJaw = max(dJawBar, pl.x - lipL);
float dHead = min(min(dSkull, dJaw), dNeck) + (nr - 0.5) * 0.045 + (ns - 0.5) * 0.02;
float body = 1.0 - smoothstep(-aa, aa, dHead);
float dryB = saturate((-0.62 - p.y) / 0.36);
body *= smoothstep(dryB * 0.85 - 0.08, dryB * 0.85 + 0.08, ns);
float bleed = exp2(-max(dHead, 0.0) * 34.0) * 0.3 * (1.0 - body) * (1.0 - dryB);
float ang = atan2(q.x, q.y);
float rq = length(q);
float wedge = smoothstep(-tl * 0.92 - 0.03, -tl * 0.92 + 0.03, ang) * (1.0 - smoothstep(tu * 0.92 - 0.03, tu * 0.92 + 0.03, ang));
float mouth = wedge * (1.0 - smoothstep(0.66, 0.8, rq)) * smoothstep(0.03, 0.1, rq) * smoothstep(0.03, 0.12, open);
float fire = exp2(-rq * rq * 16.0) * (0.85 + 0.15 * sin(Clock * 13.0));
float triU = 1.0 - abs(frac(pu.y * 11.0) - 0.5) * 2.0;
float hU = 0.05 * triU * triU * step(0.12, pu.y) * step(pu.y, 0.6);
float teethU = smoothstep(-aa, aa, pu.x - (lipU - hU)) * (1.0 - smoothstep(-aa, aa, pu.x - lipU - 0.005)) * step(0.004, hU);
float triL = 1.0 - abs(frac(pl.y * 11.0 + 0.5) - 0.5) * 2.0;
float hL = 0.04 * triL * triL * step(0.14, pl.y) * step(pl.y, 0.5);
float teethL = (1.0 - smoothstep(-aa, aa, pl.x - (lipL + hL))) * smoothstep(-aa, aa, pl.x - lipL + 0.005) * step(0.004, hL);
float teeth = saturate(teethU + teethL) * (1.0 - body) * smoothstep(0.15, 0.35, open);
float pool = exp2(-max(-dHead, 0.0) * 50.0);
float2 e = pu - float2(0.17, 0.24);
float le = length(e);
float socket = 1.0 - smoothstep(0.075, 0.12, le);
float dense = 0.42 + 0.5 * smoothstep(-0.28, 0.3, p.x) + 0.35 * pool + 0.4 * socket - 0.3 * smoothstep(0.45, 0.8, nk);
float2 sc = float2(pu.x * 10.0, pu.y * 7.0 + 0.5 * floor(pu.x * 10.0));
float2 cs = frac(sc) - 0.5;
float scaleEdge = 1.0 - smoothstep(0.05, 0.12, abs(length(cs - float2(0.0, 0.3)) - 0.42));
dense += 0.14 * scaleEdge * smoothstep(0.02, 0.16, pu.x) * smoothstep(0.25, 0.6, nk + ns * 0.3) * (1.0 - pool);
dense = saturate(dense * (0.8 + 0.4 * ns) + (ng - 0.5) * 0.12);
float sheen = smoothstep(-0.2, -0.05, dSkull) * (1.0 - smoothstep(-0.05, -0.005, dSkull)) * smoothstep(0.02, 0.16, pu.x) * smoothstep(0.3, 0.7, nk);
float2 fb = pu - float2(-0.03, 0.5);
float2 fdir = normalize(float2(-1.0, -0.3));
''' + _capsule('dFang', 'dot(fb, float2(-fdir.y, fdir.x))', 'dot(fb, fdir)', '0.03', '0.004', '0.17') + '''
float fang = (1.0 - smoothstep(-aa, aa, dFang)) * smoothstep(0.12, 0.3, open);
float eye = 1.0 - smoothstep(-aa, aa, le - 0.066);
float pupil = 1.0 - smoothstep(0.85, 1.15, length(e / float2(0.018, 0.055)));
float2 eg = e - float2(0.03, 0.03);
float glint = exp2(-dot(eg, eg) * 9000.0);
float mid = (tu - tl) * 0.5;
float2 tq = float2(q.x * cos(mid) - q.y * sin(mid), q.x * sin(mid) + q.y * cos(mid));
float wig = 0.03 * sin(tq.y * 12.0 - Clock * 28.0) * saturate(tq.y);
float split = tq.y > 0.88 ? (tq.y - 0.88) * 0.9 : 0.0;
float tongue = (1.0 - smoothstep(0.01, 0.018, min(abs(tq.x - wig - split), abs(tq.x - wig + split)))) * step(0.1, tq.y) * step(tq.y, 1.05) * smoothstep(0.45, 0.8, open);
tongue *= 1.0 - body;
float3 deep = float3(0.016, 0.01, 0.024) + Tint * 0.008;
float3 wash = Tint * 0.095 + float3(0.03, 0.025, 0.04);
float3 inkCol = lerp(wash, deep, dense) + Tint * 0.12 * sheen;
float3 mouthCol = lerp(float3(0.02, 0.006, 0.03), Tint * 0.45, fire);
col = mouthCol * mouth * (1.0 - body) + inkCol * body + wash * 0.5 * bleed;
cover = saturate(mouth * 0.95 * (1.0 - body) + body + bleed);
col = lerp(col, float3(0.86, 0.82, 0.78) * 0.85, teeth);
cover = max(cover, teeth);
// 噬魂's fire wreathes the head: the outline burns and short tongues lick off it, longest off the crown (+x, up on
// screen), rising and drifting back past the neck (-y), kept out of the open mouth and fading toward their tips; fire
// plays in patches over the skull, never over the eye. The tall flames are particles behind the head (HeadFire).
''' + _fbm('nfa', 'float2(p.x * 1.6 - Clock * 2.6, p.y * 4.2 + Clock * 1.1)', 3) + _fbm('nfb', 'float2(p.x * 3.4 - Clock * 4.2, p.y * 8.5 + Clock * 1.9)', 2) + '''
float turF = nfa * 0.7 + nfb * 0.3;
float outF = max(dHead, 0.0) / (0.05 + 0.24 * saturate(0.3 + p.x * 1.5));
float fiF = 1.0 - outF + (turF - 0.42) * (0.9 + 2.4 * outF);
float tongF = smoothstep(0.0, 0.1, fiF) * smoothstep(-0.01, 0.015, dHead) * (1.0 - mouth) * (1.0 - teeth);
float edgeF = body * (1.0 - smoothstep(0.0, 0.06, -dHead));
float skinF = smoothstep(0.5, 0.64, turF) * body * smoothstep(-0.2, 0.15, p.x) * (1.0 - socket);
float heatT = saturate(fiF * 1.2) * (0.55 + 0.6 * nfb);
float heatF = saturate(max(tongF * heatT, max(edgeF, skinF * 0.8) * (0.6 + 0.5 * nfb)));
float3 fireCol = Tint * (0.12 + 0.85 * pow(heatF, 1.4)) + white * 0.5 * pow(smoothstep(0.75, 1.0, heatF), 2.0);
col = lerp(col, fireCol, max(edgeF, skinF) * 0.9 * (0.5 + 0.5 * heatF));
float outside = tongF * (1.0 - body);
col = col * (1.0 - outside) + fireCol * outside * (0.45 + 0.55 * heatT);
cover = max(cover, tongF * (0.3 + 0.6 * heatT));
// A blood-red eye smoulders through the fire: a red glow round it, a red iris lit from within, a black slit and a small
// glint. Absolute colours, so bloom keeps it red instead of white.
float eyeGlow = exp2(-le * le * 90.0) * (1.0 - eye);
col = col * (1.0 - eyeGlow * 0.6) + float3(0.85, 0.03, 0.02) * eyeGlow;
cover = max(cover, eyeGlow * 0.6);
float3 iris = lerp(float3(0.5, 0.01, 0.01), float3(1.0, 0.1, 0.04) * 1.6, 0.25 + 0.75 * saturate(1.0 - le / 0.066));
col = lerp(col, iris * (1.0 - pupil) + float3(0.01, 0.0, 0.0) * pupil + float3(1.0, 0.85, 0.8) * glint * 1.2, eye);
cover = max(cover, eye);
col = lerp(col, float3(0.92, 0.88, 0.82) * 0.9, fang);
cover = max(cover, fang);
col = lerp(col, float3(0.62, 0.06, 0.26) + Tint * 0.15, tongue);
cover = max(cover, tongue);
''',
    # Realistic flame (噬魂's violet fire), base at -y, tip toward +y: a teardrop that sways more the higher it reaches,
    # eroded by fine turbulence stretched upward (three or four streaks across it), so above a whole base it tears into
    # separate licking tongues. Heat runs from a small hot core low in the body through the particle colour to a soft,
    # saturated edge with a faint glow round the tongues.
    # Dyn.z is the flame's age (it shortens and narrows as it dies), Dyn.w its breadth (0 a narrow tongue, 1 a broad
    # fire); Dyn.y seeds it. Fire occludes most of what is behind it (FIRE_OCCLUSION), so the violet stays deep over a
    # bright sky or floor instead of washing out to lilac.
    'M_SS_Flame': '''
float seed = Dyn.y * 23.0;
float age = saturate(Dyn.z);
float broad = saturate(Dyn.w);
float y = d.y * 0.5 + 0.5;
''' + _fbm('sw', 'float2(y * 1.2 - Clock * 1.5 + seed, seed * 0.31)', 2) + '''
float fx = d.x + (sw - 0.5) * 0.8 * y * y;
''' + _fbm('n1', 'float2(fx * 6.0 + seed * 1.7, y * 1.4 - Clock * 3.0)', 3) + _fbm('n2', 'float2(fx * 12.0 - seed, y * 2.6 - Clock * 5.2)', 2) + '''
float tur = n1 * 0.7 + n2 * 0.3;
float hh = max(y, 0.0) / (1.0 - 0.4 * age);
float wd = lerp(0.5, 0.8, broad) * (1.0 - 0.3 * age);
float body = ((1.0 - pow(hh, 1.25)) - abs(fx) / (wd * (1.0 - 0.5 * saturate(hh)) + 0.04)) * saturate(y * 6.0);
float fi = body + (tur - 0.5) * (0.7 + 3.2 * hh);
float fa = smoothstep(0.0, 0.12, fi);
float heat = saturate(fi * 1.5) * (0.72 + 0.56 * n2);
float core = saturate(1.0 - hh * 3.2) * smoothstep(0.62, 0.95, heat);
float halo = smoothstep(-0.3, 0.0, fi) * (1.0 - fa) * saturate(1.3 - abs(fx) / (wd + 0.05)) * saturate(y * 4.0);
col = (Tint / mx * (0.08 + 0.95 * pow(heat, 1.3)) + 0.75 * core * core) * mx * fa + Tint * halo * 0.22;
cover = fa * (0.7 + 0.25 * heat);
''',

    # ------------------------------------------------------------ futuristic
    # Hologram hexagon. Dyn.w < 0.5: frame with corner ticks over a dark panel; < 1.5: hex grid with a pulse ring
    # travelling out with Dyn.z and blinking cells; otherwise a lit tile.
    'M_SS_Hex': '''
float aa = fwidth(d.x) * 1.5 + 0.003;
float2 p = abs(d);
float hx = max(p.x * 0.8660254 + p.y * 0.5, p.y);
float m = 0.0;
float fill = 0.0;
if (Dyn.w < 0.5)
{
    float ang = atan2(d.y, d.x) * 0.15915494 + 0.5;
    m += 1.0 - smoothstep(0.018, 0.018 + aa, abs(hx - 0.78));
    m += (1.0 - smoothstep(0.008, 0.008 + aa, abs(hx - 0.66))) * (frac(ang * 24.0 - Clock * 0.5) < 0.6 ? 0.8 : 0.0);
    m += (1.0 - smoothstep(0.012, 0.012 + aa, abs(hx - 0.86))) * (abs(frac(ang * 6.0 + 0.5) - 0.5) < 0.06 ? 1.0 : 0.0);
    fill = (1.0 - smoothstep(0.78 - aa, 0.78, hx)) * 0.35;
}
else if (Dyn.w < 1.5)
{
    float2 h = d * 5.0;
    float2 s = float2(1.0, 1.7320508);
    float2 ha = h - s * floor(h / s) - s * 0.5;
    float2 h2 = h - s * 0.5;
    float2 hb = h2 - s * floor(h2 / s) - s * 0.5;
    float2 g = dot(ha, ha) < dot(hb, hb) ? ha : hb;
    float2 ga = abs(g);
    float hexd = 0.5 - max(dot(ga, float2(0.5, 0.8660254)), ga.x);
    float r = length(d);
    float wave = (r - Dyn.z * 1.15) / 0.12;
    float pulse = exp2(-wave * wave);
''' + _hash('hc', 'floor((h - g) * 2.0 + 0.5) + Dyn.y * 11.0') + '''
    float blink = hc > 0.82 ? 0.5 + 0.5 * sin(Clock * 9.0 + hc * 40.0) : 0.0;
    float edgeL = 1.0 - smoothstep(0.03, 0.06, hexd);
    float lit = (1.0 - edgeL) * (blink * 0.5 + pulse * 0.4 * step(0.6, hc));
    m = (edgeL * saturate(0.25 + pulse * 1.5) + lit) * (1.0 - smoothstep(0.7, 1.0, r));
    fill = (1.0 - smoothstep(0.7, 1.0, r)) * 0.1;
}
else
{
    m = (1.0 - smoothstep(0.05, 0.05 + aa, abs(hx - 0.74))) + (1.0 - smoothstep(0.012, 0.012 + aa, abs(hx - 0.56))) * 0.6;
    fill = (1.0 - smoothstep(0.8 - aa, 0.8, hx)) * 0.45;
}
col = lerp(Tint, white, saturate(m - 0.8) * 0.4) * m + Tint * 0.05 * fill;
cover = saturate(m * 0.65 + fill);
''',
    # Data bit "0" or "1" (chosen by Dyn.y) on a faint panel; Dyn.w >= 0.5 draws a pixel block instead.
    'M_SS_Digit': '''
float aa = fwidth(d.x) * 1.5 + 0.003;
float m = 0.0;
float fill = 0.0;
if (Dyn.w < 0.5)
{
    float2 p = d / float2(0.62, 0.86);
    if (frac(Dyn.y * 13.7) > 0.5)
    {
        float2 q = abs(p) - float2(0.5, 0.72);
        float rr = length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - 0.22;
        m = 1.0 - smoothstep(0.1, 0.1 + aa, abs(rr + 0.1));
        m += (1.0 - smoothstep(0.08, 0.08 + aa, abs(p.x - p.y * 0.5))) * step(abs(p.y), 0.5);
    }
    else
    {
        m = (1.0 - smoothstep(0.12, 0.12 + aa, abs(p.x - 0.05))) * step(abs(p.y), 0.95);
        m += (1.0 - smoothstep(0.1, 0.1 + aa, abs(p.y - 0.95 - (p.x - 0.05) * 0.8))) * step(-0.45, p.x) * step(p.x, 0.05);
        m += (1.0 - smoothstep(0.1, 0.1 + aa, abs(p.y + 0.95))) * step(abs(p.x - 0.05), 0.5);
    }
    m = saturate(m);
    fill = max(abs(d.x) / 0.9, abs(d.y) / 0.98) < 1.0 ? 0.25 : 0.0;
}
else
{
    float sq = max(abs(d.x), abs(d.y));
    m = (1.0 - smoothstep(0.1, 0.1 + aa, abs(sq - 0.76))) + (sq < 0.42 ? 0.9 : 0.0);
    fill = sq < 0.84 ? 0.35 : 0.0;
}
col = lerp(Tint, white, saturate(m) * 0.25) * m + Tint * 0.05 * fill;
cover = saturate(m * 0.6 + fill);
''',
    # HUD reticle. Dyn.w < 0.5: corner brackets, gapped crosshair and ring; < 1.5: dashed ring with degree ticks;
    # otherwise a dial whose blocks light up to Dyn.z under a sweeping hand.
    'M_SS_Reticle': '''
float r = length(d);
float a = atan2(d.y, d.x) * 0.15915494 + 0.5;
float aa = fwidth(r) * 1.5 + 0.003;
float m = 0.0;
if (Dyn.w < 0.5)
{
    float2 p = abs(d);
    m += (1.0 - smoothstep(0.02, 0.02 + aa, abs(p.y - 0.86))) * step(0.56, p.x) * step(p.x, 0.88);
    m += (1.0 - smoothstep(0.02, 0.02 + aa, abs(p.x - 0.86))) * step(0.56, p.y) * step(p.y, 0.88);
    m += (1.0 - smoothstep(0.01, 0.01 + aa, min(p.x, p.y))) * step(0.12, max(p.x, p.y)) * step(max(p.x, p.y), 0.42) * 0.9;
    m += (1.0 - smoothstep(0.012, 0.012 + aa, abs(r - 0.3))) * 0.8 + exp2(-r * r * 400.0);
}
else if (Dyn.w < 1.5)
{
    m += (1.0 - smoothstep(0.03, 0.03 + aa, abs(r - 0.9))) * (frac(a * 36.0 + Clock * 0.6) < 0.72 ? 1.0 : 0.0);
    float major = frac(a * 12.0 + 0.04) < 0.08 ? 1.0 : 0.0;
    float tick = abs(frac(a * 72.0 + 0.5) - 0.5) * 6.2831853 * r / 72.0;
    m += (1.0 - smoothstep(0.004, 0.004 + aa, tick)) * step(lerp(0.78, 0.7, major), r) * step(r, 0.84);
    m += (1.0 - smoothstep(0.006, 0.006 + aa, abs(r - 0.66))) * 0.5;
}
else
{
    m += (frac(a * 40.0) < 0.6 ? 1.0 : 0.0) * step(0.62, r) * step(r, 0.84) * (a < Dyn.z ? 1.0 : 0.22);
    m += saturate(1.0 - frac(a - Clock * 0.8) * 6.0) * step(0.3, r) * step(r, 0.92) * 0.5;
    m += 1.0 - smoothstep(0.01, 0.01 + aa, abs(r - 0.93));
}
float fade = 1.0 - smoothstep(0.96, 1.0, Dyn.w < 0.5 ? max(abs(d.x), abs(d.y)) : r);
col = lerp(Tint, white, saturate(m - 0.8) * 0.4) * m * fade;
cover = saturate(m * 0.6) * fade;
''',
    # Hologram crescent slash: segmented along the arc, scan bands across it, a hard bright rim, and rows that
    # glitch sideways for a frame. Geometry and Dyn as Crescent.
    'M_SS_TechArc': '''
float row = floor(d.y * 18.0);
''' + _hash('hg', 'float2(row, floor(Clock * 14.0) + Dyn.y * 31.0)') + '''
float2 q = d + float2(hg > 0.86 ? (hg - 0.93) * 0.9 : 0.0, 0.0);
float yA = 0.85 - lerp(0.12, 0.5, saturate(abs(Dyn.w)));
float c2 = (yA * yA - 1.105) / (2.0 * yA + 0.9);
float R2 = yA - c2;
float so = 0.997 - length(q - float2(0.0, -0.147));
float si = length(q - float2(0.0, c2)) - R2;
float k = saturate(si / max(so + si, 0.001));
float phi = atan2(q.x, q.y + 0.147);
float s = saturate(phi / 3.76 + 0.5);
if (frac(Dyn.y * 7.13) > 0.5) s = 1.0 - s;
float sweep = 1.0;
if (Dyn.w >= 0.0)
{
    float h = Dyn.z * 1.7;
    sweep = (1.0 - smoothstep(h - 0.04, h, s)) * saturate(1.0 - (h - s) * 0.9);
}
float aa = fwidth(so) * 1.5 + 0.001;
float body = smoothstep(0.0, aa, so) * smoothstep(0.0, aa, si);
float tips = saturate((1.0 - abs(s * 2.0 - 1.0)) * 6.0);
float seg = frac(s * 16.0) < 0.82 ? 1.0 : 0.25;
float scan = 0.55 + 0.45 * step(0.5, frac(k * 7.0 - Clock * 3.0));
float edge = ((1.0 - smoothstep(0.0, aa * 3.0, so)) + (1.0 - smoothstep(0.0, aa * 2.5, si)) * 0.6) * body;
float m = (body * seg * scan * 0.7 + edge * 1.3) * tips * sweep;
col = lerp(Tint, white, saturate(edge * 0.6)) * m;
cover = saturate(body * 0.6 * seg + edge * 0.5) * tips * sweep;
''',
    # Hologram ribbon: bright core, twin rails, dashes streaming along it and faint scan lines.
    'M_SS_TechRibbon': '''
float v = abs(UV.y * 2.0 - 1.0);
float aa = fwidth(v) * 1.5 + 0.01;
float core = 1.0 - smoothstep(0.08, 0.08 + aa, v);
float rails = 1.0 - smoothstep(0.035, 0.035 + aa, abs(v - 0.82));
float dash = (frac(UV.x * 24.0 - Clock * 3.0) < 0.5 ? 1.0 : 0.0) * (1.0 - smoothstep(0.03, 0.03 + aa, abs(v - 0.45)));
float scan = (v < 0.82 ? 0.18 : 0.0) * (0.6 + 0.4 * step(0.5, frac(UV.x * 60.0)));
float caps = saturate(UV.x * 10.0) * saturate((1.0 - UV.x) * 10.0);
float m = (core * 1.2 + rails * 0.8 + dash * 0.7 + scan) * caps;
col = lerp(Tint, white, core * 0.5) * m;
cover = saturate(core * 0.6 + rails * 0.4 + dash * 0.3 + scan) * caps;
''',
    # Low-poly triangle shard, apex toward +y: bright wire edge and inner line; Dyn.w < 0.5 also fills it.
    'M_SS_Tri': '''
float aa = fwidth(d.x) * 1.5 + 0.003;
float dist = max(-d.y - 0.5, max(dot(d, float2(0.8660254, 0.5)) - 0.5, dot(d, float2(-0.8660254, 0.5)) - 0.5));
float fill = 1.0 - smoothstep(-aa, 0.0, dist);
float edge = 1.0 - smoothstep(0.03, 0.03 + aa, abs(dist + 0.03));
float inner = 1.0 - smoothstep(0.012, 0.012 + aa, abs(dist + 0.2));
float solid = Dyn.w < 0.5 ? 1.0 : 0.0;
float m = edge + inner * 0.5 + fill * 0.3 * solid;
col = lerp(Tint, white, edge * 0.4) * m + Tint * 0.04 * fill;
cover = saturate(edge * 0.6 + fill * 0.4 * solid);
''',
    # Laser line along y: crisp core with a soft glow; Dyn.w > 0.5 cuts it into dashes streaming toward +y.
    'M_SS_Laser': '''
float ax = abs(d.x);
float aa = fwidth(d.x) * 1.5 + 0.01;
float core = 1.0 - smoothstep(0.1, 0.1 + aa, ax);
float glow = exp2(-ax * ax * 12.0) * 0.45;
float ends = saturate((1.0 - abs(d.y)) * 10.0);
float dash = Dyn.w > 0.5 ? (frac(d.y * 5.0 - Clock * 6.0 + Dyn.y * 3.0) < 0.55 ? 1.0 : 0.15) : 1.0;
float m = (core * 1.2 + glow) * ends * dash;
col = lerp(Tint, white, core * 0.55) * m;
cover = saturate(core * 0.5 + glow * 0.5) * ends * dash;
''',
    # Storm-cloud billow (天劫雷云): domain-warped puffs, lit from above, with a darker underside. Dyn.z is the light of
    # lightning inside the cloud (strongest in the dense core), Dyn.w a silver rim on the thin edges.
    'M_SS_Storm': '''
float r = length(d);
float2 sd = float2(Dyn.y * 23.0, Dyn.y * 11.0);
''' + _fbm('w1', 'd * 1.1 + sd + float2(Clock * 0.035, 0.0)', 2) + _fbm('w2', 'd * 1.1 + sd.yx + float2(0.0, -Clock * 0.03)', 2)
    + _fbm('n', 'd * 1.5 + float2(w1, w2) * 1.5 + sd', 4) + '''
float shape = saturate(1.0 - r * r);
float dens = saturate(shape * 1.35 + (n - 0.52) * 1.6);
float body = smoothstep(0.1, 0.75, dens) * saturate((1.0 - r) * 3.0);
float top = saturate(0.5 + d.y * 0.55 + (n - 0.5) * 0.8);
float core = pow(saturate(dens), 2.5);
float3 lit = Tint * (0.45 + 0.85 * top);
float3 flash = float3(0.55, 0.62, 1.0) * Dyn.z * (0.15 + 1.0 * core);
float rim = Dyn.w * saturate(1.0 - abs(dens - 0.22) * 6.0) * saturate(d.y + 0.6);
col = (lit + flash + float3(0.5, 0.56, 0.68) * rim * 0.35) * body;
cover = body;
''',
}

# Materials of the first version that no effect uses any more: the build deletes them.
RETIRED = ('M_SS_Ribbon', 'M_SS_Rune', 'M_SS_Smoke', 'M_SS_Bubble')

# Default occlusion per material when an emitter does not set Dyn.x itself. Ink covers what is behind it.
OCCLUSION = {'Glow': '0.2', 'Ring': '0.45', 'Glyph': '0.45', 'Star': '0.05', 'Crescent': '0.7', 'Wisp': '0.5',
             'Shard': '0.6', 'Streak': '0.3', 'Sword': '0.6',
             'InkArc': '0.97', 'InkStroke': '0.97', 'InkRibbon': '0.95', 'InkSplat': '0.97', 'InkWash': '0.9',
             'InkPlume': '0.92', 'Serpent': '1.0', 'Flame': '0.8',
             'Enso': '0.95', 'Bagua': '0.9', 'Calli': '0.97', 'Talisman': '1.0', 'Seal': '1.0', 'Blossom': '0.95',
             'Cloud': '0.9', 'Dipper': '0.85',
             'Hex': '0.5', 'Digit': '0.5', 'Reticle': '0.35', 'TechArc': '0.45', 'TechRibbon': '0.45', 'Tri': '0.5',
             'Laser': '0.3', 'Storm': '0.92'}

# Default progress (Dyn.z) per material when an emitter does not set it: fully drawn shapes, intact talismans.
PROGRESS = {'InkArc': '1.0', 'InkStroke': '1.0', 'Enso': '1.0', 'Bagua': '1.0', 'Calli': '1.0', 'Dipper': '1.0',
            'Talisman': '0.0', 'Reticle': '1.0'}


def material_code(name):
    return PRELUDE + MASKS[name] + EPILOGUE


# ---------------------------------------------------------------- effect language

# Expanded until stable. {C} is the caller's colour (may be scaled above 1); {N} its hue at full intensity {I}.
# Ink colours: {INK} near-black tinted by the hue, {CINK} the hue as coloured ink, {HUE} the accent each sword keeps
# inside its ink (coloured ink for violet and crimson, light for gold, pale gold and green), {WASH} diluted grey ink.
# {GLOW}/{HOTG} are glows
# capped in intensity, so callers that pass a boosted colour do not blow the ink style out.
# Signature gates by hue: {GV} violet 噬魂, {GR} crimson 弑神, {GG} gold 驱煞, {GS} pale gold 七星.
TOKENS = (
    ('{HUE}', 'lerp({GLOW}*0.75,{CINK}*1.5,{DK})'),
    ('{WASH}', 'lerp({INK},float3(0.16,0.16,0.17),0.55)'),
    ('{GLOW}', '({BODYN}*min({I},1.4))'),
    ('{HOTG}', '({HOTN}*min({I},1.4))'),
    ('{CINK}', '({N}*{N}*0.24+{INK}*0.5)'),
    ('{INK}', 'lerp(float3(0.014,0.014,0.018),{N}*0.045,0.4)'),
    ('{CIN}', 'float3(0.72,0.045,0.03)'),
    ('{GOLD}', 'float3(1.0,0.62,0.12)'),
    ('{PLUM}', 'float3(0.85,0.07,0.09)'),
    ('{MAG}', 'float3(1.0,0.12,0.75)'),
    ('{GV}', 'saturate(({N}.b+{N}.r*0.5-{N}.g*1.5-0.6)*2.0)'),
    ('{GR}', 'saturate(({N}.r-({N}.g+{N}.b)*1.2-0.2)*3.0)'),
    ('{GG}', 'saturate((0.45-{N}.b)*6.0)*saturate(({N}.g-0.45)*5.0)*saturate(({N}.r-0.8)*5.0)'),
    ('{GS}', 'saturate(({N}.b-0.35)*8.0)*saturate(({N}.g-0.8)*10.0)*saturate(({N}.r-0.8)*5.0)'),
    ('{BODY}', '({BODYN}*{I})'),
    ('{HOT}', '({HOTN}*{I})'),
    ('{BODYN}', 'lerp({N}*{N},{N},0.35)'),
    ('{HOTN}', 'lerp({N},float3(1.0,1.0,1.0),0.35)'),
    ('{DK}', 'saturate(({N}.r+{N}.b-2.0*{N}.g)*0.8)'),
    ('{N}', '({C}.rgb/{I})'),
    ('{I}', 'max(max({C}.r,{C}.g),max({C}.b,0.0001))'),
    ('{EASE}', '(1.0-pow(1.0-{A},3.0))'),
    ('{FADE}', 'pow(1.0-{A},1.5)'),
    ('{BELL}', 'sin({A}*{PI})'),
    ('{SEED}', 'frac(sin(dot(Map.Transient.SSOwner,float3(0.1297,0.0783,0.0377)))*4375.85)'),
    ('{H1}', 'frac(sin({HID}*12.9898+0.31)*43758.5453)'),
    ('{H2}', 'frac(sin({HID}*78.2330+1.73)*43758.5453)'),
    ('{H3}', 'frac(sin({HID}*37.7190+4.11)*43758.5453)'),
    ('{H4}', 'frac(sin({HID}*53.1170+2.29)*43758.5453)'),
    ('{HID}', 'fmod(float(Map.Particles.UniqueID),4096.0)'),
    ('{A}', 'saturate(Map.Particles.NormalizedAge)'),
    ('{PAGE}', 'Map.Particles.Age'),
    ('{ID}', 'float(Map.Particles.UniqueID)'),
    ('{ORG}', 'Map.Particles.SSOrigin'),
    ('{C}', 'Map.Transient.SSColor'),
    ('{AGE}', 'Map.Transient.SSAge'),
    ('{R}', 'Map.Transient.SSRadius'),
    ('{SZ}', 'Map.Transient.SSSize'),
    ('{W}', 'Map.Transient.SSWidth'),
    ('{L}', 'Map.Transient.SSLength'),
    ('{LIFE}', 'Map.Transient.SSLife'),
    ('{T}', 'Map.Transient.SSTarget'),
    ('{SRC}', 'Map.Transient.SSSource'),
    ('{E}', 'Map.Transient.SSE'),
    ('{K}', 'Map.Transient.SSK'),
    ('{TH}', 'Map.Transient.SSTheta'),
    ('{PR}', 'Map.Transient.SSProgress'),
    ('{LEN}', 'Map.Transient.SSLen'),
    ('{DIR}', 'Map.Transient.SSDir'),
    ('{P1}', 'Map.Transient.SSP1'),
    ('{P2}', 'Map.Transient.SSP2'),
    ('{QA}', 'Map.Transient.SSPa'),
    ('{QB}', 'Map.Transient.SSPb'),
    ('{X1}', 'Map.Transient.SSX1'),
    ('{X2}', 'Map.Transient.SSX2'),
    ('{V1}', 'Map.Transient.SSV1'),
    ('{V2}', 'Map.Transient.SSV2'),
    ('{PI}', '3.14159265'),
    ('{TAU}', '6.28318531'),
    ('{EYE}', 'Map.Transient.SSEye'),
    ('{DEN}', 'Map.Transient.SSDensity'),
    ('{FL}', 'Map.Transient.SSFlash'),
    ('{ST}', 'Map.Transient.SSStorm'),
    ('{RHO}', 'Map.Transient.SSRho'),
    ('{WT}', 'Map.Transient.SSWeight'),
    ('{FLIP}', 'Map.Transient.SSFlip'),
    ('{HT}', 'Map.Transient.SSHeight'),
)


def expand(code):
    for _ in range(10):
        before = code
        for token, value in TOKENS:
            code = code.replace(token, value)
        if code == before:
            break
    assert '{' not in code, 'unknown token in ' + code
    return code


def sub(template, **values):
    """Fills $name placeholders, leaving {TOKENS} alone."""
    for name, value in values.items():
        template = template.replace('$' + name, value)
    return template


def sphere(u, v):
    """Unit vector from two hashes (z = 2v-1)."""
    return sub('float3(cos($u*{TAU})*sqrt(saturate(1.0-($v*2.0-1.0)*($v*2.0-1.0))),'
               'sin($u*{TAU})*sqrt(saturate(1.0-($v*2.0-1.0)*($v*2.0-1.0))),$v*2.0-1.0)', u=u, v=v)


# Orthonormal basis around {DIR}.
PERP1 = 'normalize(cross({DIR},abs({DIR}.z)>0.95?float3(1.0,0.0,0.0):float3(0.0,0.0,1.0)))'
PERP2 = 'cross({DIR},{P1})'
ZERO = 'float3(0.0,0.0,0.0)'

# Signature switches: size multipliers that collapse a sword's own layer to nothing for the other swords.
ON_V, ON_R, ON_G, ON_S = ('step(0.01,{GV})', 'step(0.01,{GR})', 'step(0.01,{GG})', 'step(0.01,{GS})')
# Ink drops of the crimson sword are partly cinnabar ("blood ink").
BLOOD = 'lerp({INK},{CIN},{GR}*step(0.4,{H4}))'
# Petals drift on a breeze.
SWAY = 'float3(sin({PAGE}*3.0+{H3}*6.0),cos({PAGE}*2.6+{H1}*6.0),0.0)'
# Calligraphy per sword: 封 for 驱煞, 星 for 七星, 灭 for 噬魂, otherwise 斩.
CHARACTER = '({GG}>0.5?1.0:({GS}>0.5?2.0:({GV}>0.5?3.0:0.0)))'


class Emitter:
    def __init__(self, name, burst, count, life, mat):
        assert mat in OCCLUSION, mat
        self.d = dict(name=name, burst=burst, count=float(count), life=life, mat=mat, ribbon=False, world=False,
                      facing='camera', align='none', duration=0.0, delay=0.0, spawn=[], update=[])

    def w(self, kind, target, code):
        self.d['update'].append([kind, target, code])
        return self

    def f(self, target, code):
        return self.w('float', target, code)

    def v(self, target, code):
        return self.w('vec3', target, code)

    def pos(self, code):
        return self.w('pos', 'Particles.Position', code)

    def size(self, code):
        return self.w('vec2', 'Particles.SpriteSize', code)

    def tint(self, code):
        return self.w('color', 'Particles.Color', code)

    def turn(self, code):
        return self.w('float', 'Particles.SpriteRotation', code)

    def along(self, code):
        """Custom alignment: the material's +y (crescent bulge, stroke head, sword tip) points along this vector."""
        self.d['align'] = 'custom'
        return self.w('vec3', 'Particles.SpriteAlignment', code)

    def face(self, code):
        self.d['facing'] = 'custom'
        return self.w('vec3', 'Particles.SpriteFacing', code)

    def flat(self):
        return self.face('float3(0.0,0.0,1.0)')

    def width(self, code):
        return self.w('float', 'Particles.RibbonWidth', code)

    def dyn(self, x=None, y=None, z=None, w='0.0'):
        """Dynamic material parameter: occlusion, seed, progress, variant/shape (see the masks). Ribbons default to
        one seed for every particle so the brush pattern does not jump between ribbon segments."""
        mat = self.d['mat']
        x = OCCLUSION[mat] if x is None else x
        y = ('0.37' if self.d['ribbon'] else '{H1}') if y is None else y
        z = PROGRESS.get(mat, '{A}') if z is None else z
        return self.w('vec4', 'Particles.DynamicMaterialParameter', 'float4(%s,%s,%s,%s)' % (x, y, z, w))

    def ribbon(self):
        self.d['ribbon'] = True
        return self.f('Particles.RibbonLinkOrder', '{ID}')

    def world(self):
        self.d['world'] = True
        return self

    def origin(self):
        """World-space particles remember where they were born, so update can drift them from there."""
        self.d['spawn'].append(['pos', 'Particles.SSOrigin', 'Map.Particles.Position'])
        return self

    def after(self, seconds):
        self.d['delay'] = seconds
        return self

    def lasting(self, seconds):
        self.d['duration'] = seconds
        return self

    def data(self):
        if not any(row[1] == 'Particles.DynamicMaterialParameter' for row in self.d['update']):
            self.dyn()
        # The Fountain template randomises the sprite rotation, which would tilt aligned sprites.
        if not self.d['ribbon'] and not any(row[1] == 'Particles.SpriteRotation' for row in self.d['update']):
            self.turn('0.0')
        d = dict(self.d)
        d['life'] = expand(d['life'])
        d['spawn'] = [[k, t, expand(c)] for k, t, c in self.d['spawn']]
        d['update'] = [[k, t, expand(c)] for k, t, c in self.d['update']]
        return d


def burst(name, count, life, mat='Glow'):
    return Emitter(name, True, count, life, mat)


def loop(name, rate, life, mat='Glow'):
    return Emitter(name, False, rate, life, mat)


SYSTEMS = []


def system(name, params, emitters, warmup=0.0):
    """warmup pre-simulates looping systems so their formations are up the moment they appear."""
    SYSTEMS.append((name, params, emitters, warmup))


# ================================================================ ink wash: 噬魂剑, 驱煞神刃, 七星光芒剑, 弑神剑

TRAIL_LIFE = 'clamp({LIFE},0.05,2.0)'

# 噬魂's violet fire. Since 2026-11-01 realistic flames (M_SS_Flame) replace the soul-fire wisps of the earlier versions,
# which read as scattered points of light: each flame sprite stands along world up (local Z where the system is level),
# carries its own seed and age, and rises over dark violet smoke. Only the violet sword shows them.
FIRE_TINT = '{GLOW}*1.2+{HOTG}*0.1'
FIRE_OCCLUSION = '0.8'
FIRE_SMOKE = 'lerp({INK},{CINK},0.55)'
UP = 'float3(0.0,0.0,1.0)'
OFF_V = '(1.0-' + ON_V + ')'


def fire(e, pos, width, height, alpha, broad='0.2+0.6*{H2}', occlusion=FIRE_OCCLUSION, age='{A}', gate=True):
    """Flame sprites at pos, width × height (base to tip), fading by alpha; gated to 噬魂 unless gate is False."""
    on = '*' + ON_V if gate else ''
    hue = '*{GV}' if gate else ''
    return (e.pos(pos).along(UP)
            .size('float2(%s,%s)%s' % (width, height, on))
            .tint('float4(' + FIRE_TINT + ',{C}.a*(' + alpha + ')' + hue + ')')
            .dyn(x=occlusion, z=age, w=broad))


def fire_smoke(e, pos, size, alpha, gate=True):
    """Dark violet smoke billowing off the fire."""
    on = '*' + ON_V if gate else ''
    hue = '*{GV}' if gate else ''
    return (e.pos(pos).size('float2(1.0,1.0)*(%s)%s' % (size, on))
            .turn('{H1}*360.0+{A}*70.0')
            .tint('float4(' + FIRE_SMOKE + ',{C}.a*(' + alpha + ')' + hue + ')')
            .dyn(x='0.8', z='0.25+{A}*0.65', w='0.35'))

# Sword-qi trail from the hilt: a dry-brush ink ribbon with the sword's colour running inside it, ink mist spreading
# behind and drops flicked off, plus the sword's own signature: soul fire, gold leaf, star glints or plum petals.
system('NS_SS_Trail', ['Width', 'Life'], [
    loop('Wash', 16, TRAIL_LIFE + '*2.2', 'InkWash').world().origin()
        .pos('{ORG}+float3({H1}-0.5,{H2}-0.5,{H3}-0.35)*{W}*1.4*{A}')
        .size('float2(1.0,1.0)*{W}*(1.6+2.6*{A})')
        .turn('{H1}*360.0+{A}*50.0')
        .tint('float4({WASH},{C}.a*0.55*{BELL})')
        .dyn(z='{A}*0.8'),
    loop('Ink', 110, TRAIL_LIFE + '*1.15', 'InkRibbon').ribbon().world()
        .width('{W}*2.3*pow(1.0-{A},0.6)')
        .tint('float4({INK},{C}.a*pow(1.0-{A},0.7))'),
    loop('Hue', 110, TRAIL_LIFE + '*0.65', 'InkRibbon').ribbon().world()
        .width('{W}*0.9*pow(1.0-{A},0.9)')
        .tint('float4({HUE},{C}.a*0.9*pow(1.0-{A},1.1))')
        .dyn(x='0.85'),
    loop('Drops', 12, '0.55+0.3*{H3}', 'InkSplat').world().origin()
        .v('Transient.SSDir', sphere('{H1}', '{H2}'))
        .pos('{ORG}+{DIR}*{W}*(0.6+1.4*{H3})*{EASE}-float3(0.0,0.0,{W}*2.5*{A}*{A})')
        .size('float2(1.0,1.0)*{W}*(0.22+0.3*{H4})*(1.0-0.4*{A})')
        .turn('{H1}*360.0')
        .tint('float4(' + BLOOD + ',{C}.a*(1.0-{A}*{A}))')
        .dyn(w='1.0'),
    fire(loop('Fire', 18, '0.45+0.25*{H3}', 'Flame').world().origin(),
         '{ORG}+float3(({H1}-0.5)*{W}*0.7,({H2}-0.5)*{W}*0.7,{W}*(0.15+1.7*{EASE}))',
         '{W}*(1.1+0.6*{H4})*(1.0-0.25*{A})', '{W}*(2.0+1.2*{H4})*(0.75+0.25*{BELL})', 'saturate({A}*9.0)*(1.0-{A}*{A})'),
    fire_smoke(loop('FireSmoke', 5, '1.0', 'InkPlume').world().origin(),
               '{ORG}+float3(({H1}-0.5)*{W},({H2}-0.5)*{W},{W}*(1.4+2.6*{EASE}))',
               '{W}*(1.3+1.8*{A})', '0.32*{BELL}'),
    loop('Leaf', 10, '0.9', 'Shard').world().origin()
        .pos('{ORG}+float3(({H1}-0.5)*{W}*1.6,({H2}-0.5)*{W}*1.6,{W}*(0.6*{A}-1.2*{A}*{A}))+' + SWAY + '*{W}*0.3*{A}')
        .size('float2({W}*0.16,{W}*0.3)*(1.0-0.3*{A})*' + ON_G)
        .turn('{H1}*360.0+{PAGE}*({H2}-0.5)*600.0')
        .tint('float4({GOLD}*0.85,{C}.a*{GG}*(1.0-{A}))')
        .dyn(x='0.75'),
    loop('Glints', 10, '0.5', 'Star').world().origin()
        .pos('{ORG}+' + sphere('{H1}', '{H2}') + '*{W}*(0.4+0.9*{H3})')
        .size('float2(1.0,1.0)*{W}*(0.9+0.9*{H2})*{BELL}*(0.7+0.3*sin({PAGE}*30.0+{H1}*20.0))*' + ON_S)
        .turn('{H1}*90.0')
        .tint('float4({HOTG}*1.6,{C}.a*{GS}*{BELL})'),
    loop('Petals', 6, '1.1', 'Blossom').world().origin()
        .pos('{ORG}+float3(({H1}-0.5)*{W}*2.0,({H2}-0.5)*{W}*2.0,-{W}*1.2*{A})+' + SWAY + '*{W}*0.6*{A}')
        .size('float2(1.0,1.0)*{W}*(0.35+0.2*{H3})*' + ON_R)
        .turn('{H1}*360.0+{PAGE}*({H2}-0.5)*300.0')
        .tint('float4({PLUM},{C}.a*{GR}*(1.0-{A}*{A}))')
        .dyn(w='1.0'),
])

# Blade aura (loops, attached with the blade on local Z, User.Radius = blade length): ink mist clinging to the blade,
# a faint coloured sheath, ink wisps peeling off toward the grip and the sword's signature around it.
system('NS_SS_Aura', ['Radius'], [
    loop('Mist', 8, '1.1', 'InkWash')
        .pos('float3(({H2}-0.5)*{R}*0.08,({H3}-0.5)*{R}*0.08,({H1}-0.5)*{R}*0.75)')
        .size('float2(1.0,1.0)*{R}*0.24*(0.7+0.6*{A})')
        .turn('{H1}*360.0+{A}*40.0')
        .tint('float4({WASH},{C}.a*0.45*{BELL})')
        .dyn(z='0.25+{A}*0.6', w='1.0'),
    loop('Sheath', 5, '0.8')
        .pos('float3(0.0,0.0,({H1}-0.5)*{R}*0.4)')
        .along('float3(0.0,0.0,1.0)')
        .size('float2({R}*0.12,{R}*0.7)*(0.9+0.1*{BELL})')
        .tint('float4({GLOW},{C}.a*0.4*{BELL})')
        .dyn(x='0.35'),
    loop('Strokes', 6, '0.7', 'InkStroke')
        .f('Transient.SSTheta', '{H1}*{TAU}')
        .pos('float3(cos({TH}),sin({TH}),0.0)*{R}*(0.04+0.07*{A})+float3(0.0,0.0,{R}*(0.35-0.7*{A}))')
        .along('float3(0.0,0.0,1.0)')
        .size('float2({R}*0.035,{R}*0.24)*{BELL}')
        .tint('float4({INK},{C}.a*0.8*{BELL})')
        .dyn(w='0.6'),
    # 噬魂: the blade burns, flames licking along it toward the tip (its local Z), which points up while it hovers.
    fire(loop('Fire', 16, '0.4+0.2*{H3}', 'Flame'),
         'float3(({H2}-0.5)*{R}*0.07,({H3}-0.5)*{R}*0.07,({H1}-0.45)*{R}*0.8+{R}*0.14*{EASE})',
         '{R}*(0.11+0.07*{H4})', '{R}*(0.22+0.14*{H4})*(0.8+0.2*{BELL})', 'saturate({A}*8.0)*(1.0-{A}*{A})'),
    loop('Blood', 4, '0.8', 'InkSplat')
        .f('Transient.SSTheta', '{H1}*{TAU}')
        .pos('float3(cos({TH}),sin({TH}),0.0)*{R}*0.04+float3(0.0,0.0,{R}*(-0.3+0.8*{A}))')
        .size('float2(1.0,1.0)*{R}*0.035*(0.7+0.6*{H2})*' + ON_R)
        .tint('float4({CIN},{C}.a*{GR}*{BELL})')
        .dyn(w='1.0'),
    loop('Leaf', 6, '0.9', 'Shard')
        .f('Transient.SSTheta', '{H1}*{TAU}+{A}*4.0')
        .pos('float3(cos({TH}),sin({TH}),0.0)*{R}*0.12+float3(0.0,0.0,({H2}-0.5)*{R}*0.8)')
        .size('float2({R}*0.025,{R}*0.05)*{BELL}*' + ON_G)
        .turn('{H1}*360.0+{PAGE}*300.0')
        .tint('float4({GOLD}*0.85,{C}.a*{GG}*{BELL})')
        .dyn(x='0.75'),
    loop('Stars', 16, '0.55', 'Star')
        .f('Transient.SSTheta', '{H1}*{TAU}')
        .pos('float3(cos({TH}),sin({TH}),0.0)*{R}*0.05+float3(0.0,0.0,{R}*0.42-{A}*{R}*0.85)')
        .size('float2(1.0,1.0)*{R}*0.09*{BELL}*(0.6+0.4*{H2})*' + ON_S)
        .turn('{H1}*90.0')
        .tint('float4({HOTG}*1.5,{C}.a*{GS}*{BELL})'),
], warmup=0.8)

# Normal strike along local +X: a brush crescent drawn in one sweep with the sword's colour inside it and a thin
# sword-light rim, ink flicks and drops thrown forward, a small splash and a wash; the sweep side follows the component
# position, so consecutive strikes vary.
SEEDED = '{SEED}'
system('NS_SS_Slash', ['Size'], [
    burst('Wash', 3, '0.8', 'InkPlume')
        .pos('float3({SZ}*0.3*{H1},({H2}-0.5)*{SZ}*0.8,({H3}-0.5)*{SZ}*0.3)+float3({SZ}*0.25,0.0,{SZ}*0.12)*{EASE}')
        .size('float2(1.0,1.0)*{SZ}*(0.5+0.65*{EASE})')
        .turn('{H1}*360.0+{A}*35.0')
        .tint('float4({WASH},{C}.a*0.62*(1.0-{A}))')
        .dyn(z='{A}*0.9', w='0.7'),
    burst('Splash', 1, '0.55', 'InkSplat')
        .pos('float3({SZ}*0.45,0.0,0.0)')
        .size('float2(1.0,1.0)*{SZ}*0.5*(0.6+0.4*{EASE})')
        .turn('{SEED}*360.0')
        .tint('float4({INK},{C}.a*0.9*(1.0-{A}*{A}))')
        .dyn(z='{A}', w='0.0'),
    burst('Stroke', 1, '0.42', 'InkArc')
        .pos('float3({SZ}*0.08,0.0,0.0)').along('float3(1.0,0.0,0.0)')
        .size('float2({SZ}*1.6,{SZ}*1.0)*(0.82+0.25*{EASE})')
        .tint('float4({INK},{C}.a*(1.0-{A}*{A}*{A}))')
        .dyn(y=SEEDED, z='saturate({A}*3.0)', w='0.8'),
    burst('Hue', 1, '0.38', 'InkArc')
        .pos('float3({SZ}*0.1,0.0,0.0)').along('float3(1.0,0.0,0.0)')
        .size('float2({SZ}*1.56,{SZ}*0.98)*(0.82+0.25*{EASE})')
        .tint('float4({HUE},{C}.a*(1.0-{A}*{A}))')
        .dyn(x='0.95', y=SEEDED, z='saturate({A}*3.2)', w='1.45'),
    burst('Flicks', 14, '0.34+0.12*{H3}', 'InkStroke')
        .v('Transient.SSDir', 'normalize(float3(0.55+0.45*{H1},({H2}-0.5)*2.0,({H3}-0.5)*0.8))')
        .pos('{DIR}*{SZ}*(0.2+0.8*{EASE})*(0.6+0.4*{H4})')
        .along('{DIR}')
        .size('float2({SZ}*0.14,{SZ}*0.42*(1.0-0.4*{A}))')
        .tint('float4({INK},{C}.a*(1.0-{A}))')
        .dyn(w='0.55'),
    burst('Drops', 12, '0.5+0.2*{H3}', 'InkSplat')
        .v('Transient.SSDir', 'normalize(float3(0.4+0.6*{H1},({H2}-0.5)*2.0,0.3+0.5*{H3}))')
        .pos('{DIR}*{SZ}*(0.25+0.7*{EASE})-float3(0.0,0.0,{SZ}*0.5*{A}*{A})')
        .size('float2(1.0,1.0)*{SZ}*(0.035+0.05*{H4})')
        .turn('{H1}*360.0')
        .tint('float4(' + BLOOD + ',{C}.a*(1.0-{A}*{A}))')
        .dyn(w='1.0'),
    fire(burst('Fire', 9, '0.45+0.2*{H3}', 'Flame').f('Transient.SSTheta', '({H1}-0.5)*2.4'),
         'float3(cos({TH})*{SZ}*0.5,sin({TH})*{SZ}*0.75,{SZ}*(0.05+0.35*{EASE}))',
         '{SZ}*(0.17+0.1*{H4})', '{SZ}*(0.38+0.22*{H4})', 'saturate({A}*10.0)*(1.0-{A})'),
    burst('Petals', 7, '0.9', 'Blossom')
        .v('Transient.SSDir', 'normalize(float3(0.3+0.7*{H1},({H2}-0.5)*2.0,0.2+0.6*{H3}))')
        .pos('{DIR}*{SZ}*(0.2+0.6*{EASE})-float3(0.0,0.0,{SZ}*0.3*{A}*{A})+' + SWAY + '*{SZ}*0.06')
        .size('float2(1.0,1.0)*{SZ}*(0.09+0.05*{H4})*' + ON_R)
        .turn('{H1}*360.0+{PAGE}*({H2}-0.5)*500.0')
        .tint('float4({PLUM},{C}.a*{GR}*(1.0-{A}*{A}))')
        .dyn(w='step({H4},0.75)'),
    burst('Leaf', 8, '0.7', 'Shard')
        .v('Transient.SSDir', 'normalize(float3(0.3+0.7*{H1},({H2}-0.5)*2.0,0.3+0.6*{H3}))')
        .pos('{DIR}*{SZ}*(0.2+0.7*{EASE})-float3(0.0,0.0,{SZ}*0.25*{A}*{A})')
        .size('float2({SZ}*0.035,{SZ}*0.07)*(1.0-0.3*{A})*' + ON_G)
        .turn('{H1}*360.0+{PAGE}*({H2}-0.5)*700.0')
        .tint('float4({GOLD}*0.85,{C}.a*{GG}*(1.0-{A}))')
        .dyn(x='0.75'),
    burst('Glints', 5, '0.4', 'Star')
        .f('Transient.SSTheta', '({H1}-0.5)*2.8')
        .pos('float3(cos({TH})*{SZ}*0.62+{SZ}*0.05,sin({TH})*{SZ}*0.8,0.0)')
        .size('float2(1.0,1.0)*{SZ}*(0.3+0.2*{H2})*{BELL}*' + ON_S)
        .turn('{H1}*90.0')
        .tint('float4({HOTG}*1.6,{C}.a*{GS}*{BELL})'),
])

# Hit burst: an ink splash with the sword's colour splashed inside it, an ensō drawn round it, a coloured flash, ink
# flicks and falling drops, plus the signature: a cinnabar seal stamped on the splash, burning talismans and gold
# leaf, stars, or soul fire.
system('NS_SS_Burst', ['Radius'], [
    burst('Wash', 5, '1.0', 'InkPlume')
        .v('Transient.SSDir', 'normalize(' + sphere('{H1}', '{H2}') + '*float3(1.0,1.0,0.6)+float3(0.0,0.0,0.35))')
        .pos('{DIR}*{R}*(0.1+0.6*{EASE})+float3(0.0,0.0,{R}*0.25*{A})')
        .size('float2(1.0,1.0)*{R}*(0.7+1.0*{EASE})')
        .turn('{H1}*360.0+{A}*30.0')
        .tint('float4({WASH},{C}.a*0.6*(1.0-{A}))')
        .dyn(z='{A}*0.9', w='0.7'),
    burst('Splat', 1, '0.7', 'InkSplat')
        .pos(ZERO)
        .size('float2(1.0,1.0)*{R}*1.5*(0.55+0.45*{EASE})')
        .turn('{SEED}*360.0')
        .tint('float4({INK},{C}.a*(1.0-pow({A},2.5)))')
        .dyn(z='{A}', w='0.0'),
    burst('Hue', 1, '0.6', 'InkSplat')
        .pos(ZERO)
        .size('float2(1.0,1.0)*{R}*0.7*(0.5+0.5*{EASE})')
        .turn('{SEED}*360.0+137.0')
        .tint('float4({HUE},{C}.a*0.9*(1.0-{A}*{A}))')
        .dyn(x='0.9', y='{SEED}*0.7+0.1', z='{A}', w='0.0'),
    burst('Enso', 1, '0.55', 'Enso')
        .pos(ZERO)
        .size('float2(1.0,1.0)*{R}*2.3*(0.35+0.75*{EASE})')
        .tint('float4({INK},{C}.a*0.65*(1.0-{A}*{A}))')
        .dyn(y='{SEED}', z='saturate({A}*3.5)', w='0.5'),
    burst('Flash', 1, '0.22')
        .pos(ZERO)
        .size('float2(1.0,1.0)*{R}*0.45*(0.5+0.5*{A})')
        .tint('float4({GLOW},{C}.a*0.8*pow(1.0-{A},2.0))')
        .dyn(x='0.35'),
    burst('Flicks', 16, '0.4+0.15*{H3}', 'InkStroke')
        .v('Transient.SSDir', 'normalize(' + sphere('{H1}', '{H2}') + '+float3(0.0,0.0,0.3))')
        .pos('{DIR}*{R}*(0.25+1.0*{EASE})*(0.6+0.4*{H3})')
        .along('{DIR}')
        .size('float2({R}*0.16,{R}*0.55*(1.0-0.4*{A}))')
        .tint('float4({INK},{C}.a*(1.0-{A}))')
        .dyn(w='0.5'),
    burst('Drops', 14, '0.6+0.2*{H3}', 'InkSplat')
        .v('Transient.SSDir', 'normalize(' + sphere('{H3}', '{H1}') + '+float3(0.0,0.0,0.5))')
        .pos('{DIR}*{R}*(0.2+1.1*{EASE})*(0.5+0.5*{H2})-float3(0.0,0.0,{R}*0.7*{A}*{A})')
        .size('float2(1.0,1.0)*{R}*(0.05+0.07*{H4})*(1.0-0.3*{A})')
        .turn('{H1}*360.0')
        .tint('float4(' + BLOOD + ',{C}.a*(1.0-{A}*{A}))')
        .dyn(w='1.0'),
    burst('Seal', 1, '0.75', 'Seal')
        .pos(ZERO)
        .size('float2(1.0,1.0)*{R}*0.75*(1.0+0.6*saturate(1.0-{A}*8.0))*' + ON_R)
        .turn('{SEED}*30.0-15.0')
        .tint('float4({CIN},{C}.a*{GR}*saturate({A}*14.0)*(1.0-pow({A},4.0)))')
        .dyn(x='1.0', w='step(0.75,{SEED})'),
    burst('Petals', 8, '1.0', 'Blossom')
        .v('Transient.SSDir', 'normalize(' + sphere('{H2}', '{H3}') + '+float3(0.0,0.0,0.4))')
        .pos('{DIR}*{R}*(0.3+0.8*{EASE})-float3(0.0,0.0,{R}*0.4*{A}*{A})+' + SWAY + '*{R}*0.08')
        .size('float2(1.0,1.0)*{R}*(0.12+0.06*{H4})*' + ON_R)
        .turn('{H1}*360.0+{PAGE}*({H2}-0.5)*500.0')
        .tint('float4({PLUM},{C}.a*{GR}*(1.0-{A}*{A}))')
        .dyn(w='step({H4},0.7)'),
    burst('Talismans', 3, '0.8', 'Talisman')
        .f('Transient.SSTheta', '{ID}*2.0943951+{SEED}*{TAU}')
        .v('Transient.SSDir', 'normalize(float3(cos({TH}),sin({TH}),0.6))')
        .pos('{DIR}*{R}*(0.3+0.8*{EASE})')
        .size('float2(min({R}*0.2,48.0),min({R}*0.5,120.0))*' + ON_G)
        .turn('({H1}-0.5)*60.0+{A}*({H2}-0.5)*200.0')
        .tint('float4({GLOW}*1.2,{C}.a*{GG}*saturate((1.0-{A})*5.0))')
        .dyn(x='1.0', z='saturate({A}*1.3-0.1)'),
    burst('Leaf', 10, '0.8', 'Shard')
        .v('Transient.SSDir', sphere('{H1}', '{H3}'))
        .pos('{DIR}*{R}*(0.3+0.9*{EASE})-float3(0.0,0.0,{R}*0.3*{A}*{A})')
        .size('float2({R}*0.05,{R}*0.1)*(1.0-0.3*{A})*' + ON_G)
        .turn('{H1}*360.0+{PAGE}*({H2}-0.5)*700.0')
        .tint('float4({GOLD}*0.85,{C}.a*{GG}*(1.0-{A}))')
        .dyn(x='0.75'),
    burst('Stars', 7, '0.6', 'Star')
        .v('Transient.SSDir', sphere('{H2}', '{H1}'))
        .pos('{DIR}*{R}*(0.3+0.8*{EASE})')
        .size('float2(1.0,1.0)*{R}*(0.35+0.25*{H3})*{BELL}*' + ON_S)
        .turn('{H1}*90.0')
        .tint('float4({HOTG}*1.7,{C}.a*{GS}*{BELL})'),
    # 噬魂: violet fire bursting up from the hit, then smoke.
    fire(burst('Fire', 12, '0.5+0.25*{H3}', 'Flame').f('Transient.SSTheta', '{H1}*{TAU}'),
         'float3(cos({TH}),sin({TH}),0.0)*{R}*0.4*(0.25+0.75*{H2})*(0.5+0.5*{EASE})+float3(0.0,0.0,{R}*(0.0+0.55*{EASE}))',
         '{R}*(0.3+0.2*{H4})', '{R}*(0.62+0.4*{H4})', 'saturate({A}*10.0)*(1.0-{A}*{A})'),
    fire_smoke(burst('FireSmoke', 4, '1.1', 'InkPlume'),
               'float3(({H1}-0.5)*{R}*0.6,({H2}-0.5)*{R}*0.6,{R}*(0.5+1.2*{EASE}))', '{R}*(0.6+0.9*{A})', '0.35*{BELL}'),
])

# Ground slam (local Z up): a trigram formation brushed on the ground, an ensō with the sword's colour inside it, ink
# mist rolling out, an ink crest riding the wave, ink swords bursting from the ground and drops kicked up, plus
# the signature: soul fire, standing talismans, the Big Dipper chart or a cinnabar seal.
system('NS_SS_Ring', ['Radius'], [
    burst('Formation', 1, '1.0', 'Bagua').flat()
        .pos('float3(0.0,0.0,3.0)')
        .size('float2(1.0,1.0)*{R}*2.1*(0.6+0.45*{EASE})')
        .turn('{A}*30.0+{SEED}*360.0')
        .tint('float4({INK},{C}.a*0.5*pow(1.0-{A},1.2)*saturate({A}*8.0+0.1))')
        .dyn(z='saturate({A}*2.2)', w='0.0'),
    burst('Enso', 1, '0.7', 'Enso').flat()
        .pos('float3(0.0,0.0,5.0)')
        .size('float2(1.0,1.0)*{R}*2.0*(0.15+0.95*{EASE})')
        .tint('float4({INK},{C}.a*(1.0-{A}*{A}))')
        .dyn(y='{SEED}', z='saturate({A}*4.0)', w='0.35'),
    burst('Hue', 1, '0.6', 'Enso').flat()
        .pos('float3(0.0,0.0,6.0)')
        .size('float2(1.0,1.0)*{R}*1.9*(0.12+0.95*{EASE})')
        .tint('float4({HUE},{C}.a*0.85*pow(1.0-{A},1.3))')
        .dyn(x='0.85', y='{SEED}+0.37', z='saturate({A}*4.0)', w='0.1'),
    burst('Wave', 18, '1.0', 'InkPlume')
        .f('Transient.SSTheta', '{ID}/18.0*{TAU}+{H1}*0.3')
        .pos('float3(cos({TH}),sin({TH}),0.0)*{R}*(0.1+0.85*{EASE})*(0.85+0.3*{H2})+float3(0.0,0.0,min({R}*0.08,50.0)+min({R}*0.14,90.0)*{A}*{H3})')
        .size('float2(1.0,1.0)*min({R}*(0.22+0.3*{EASE}),260.0)*(0.7+0.6*{H3})')
        .turn('{H1}*360.0+{A}*40.0')
        .tint('float4({WASH},{C}.a*0.62*(1.0-{A}))')
        .dyn(z='{A}*0.9', w='0.75'),
    burst('Crest', 24, '0.6', 'InkSplat')
        .f('Transient.SSTheta', '{ID}/24.0*{TAU}+{H1}*0.2')
        .f('Transient.SSE', 'min({R}*0.24,120.0)*(0.6+0.6*{H2})')
        .pos('float3(cos({TH}),sin({TH}),0.0)*{R}*(0.12+0.9*{EASE})+float3(0.0,0.0,{E}*(0.3+0.5*{EASE}))')
        .size('float2(1.0,1.0)*{E}*(0.7+0.5*{EASE})')
        .turn('{H1}*360.0')
        .tint('float4({INK},{C}.a*0.95*pow(1.0-{A},1.3)*saturate({A}*8.0+0.1))')
        .dyn(z='{A}', w='0.0'),
    burst('Swords', 8, '0.9', 'Sword')
        .f('Transient.SSTheta', '{ID}/8.0*{TAU}+0.39')
        .f('Transient.SSE', 'min({R}*0.5,220.0)')
        .pos('float3(cos({TH}),sin({TH}),0.0)*{R}*0.62+float3(0.0,0.0,{E}*(-0.5+0.95*saturate({A}*3.0)))')
        .along('float3(0.0,0.0,1.0)')
        .size('float2({E}*0.32,{E})')
        .tint('float4({INK},{C}.a*0.75*saturate({A}*6.0)*pow(1.0-{A},1.2))')
        .dyn(x='0.95'),
    burst('Drops', 22, '0.7', 'InkSplat')
        .f('Transient.SSTheta', '{H1}*{TAU}')
        .v('Transient.SSDir', 'float3(cos({TH}),sin({TH}),0.0)')
        .f('Transient.SSE', 'min({R}*0.45,240.0)*(0.5+0.5*{H2})')
        .pos('{DIR}*{R}*(0.15+0.65*{EASE})+float3(0.0,0.0,{E}*({A}-{A}*{A})*4.0)')
        .size('float2(1.0,1.0)*min({R}*0.06,36.0)*(0.6+0.8*{H3})')
        .turn('{H1}*360.0')
        .tint('float4(' + BLOOD + ',{C}.a*(1.0-{A}*{A}))')
        .dyn(w='1.0'),
    # 噬魂: a ring of violet fire riding the wave out.
    fire(burst('Fire', 22, '0.6+0.3*{H3}', 'Flame').f('Transient.SSTheta', '{ID}/22.0*{TAU}+{H1}*0.3'),
         'float3(cos({TH}),sin({TH}),0.0)*{R}*(0.15+0.8*{EASE})*(0.85+0.3*{H2})+float3(0.0,0.0,min({R}*0.17,85.0)+min({R}*0.15,75.0)*{EASE}*{H2})',
         'min({R}*0.17,85.0)*(0.7+0.5*{H4})', 'min({R}*0.4,200.0)*(0.7+0.5*{H4})', 'saturate({A}*8.0)*(1.0-{A}*{A})'),
    burst('Talismans', 8, '1.1', 'Talisman')
        .f('Transient.SSTheta', '{ID}/8.0*{TAU}+{SEED}*{TAU}')
        .pos('float3(cos({TH}),sin({TH}),0.0)*{R}*0.75+float3(0.0,0.0,min({R}*0.18,90.0)*(0.6+0.4*{EASE}))')
        .size('float2(min({R}*0.12,60.0),min({R}*0.32,160.0))*' + ON_G)
        .tint('float4({GLOW}*1.2,{C}.a*{GG}*saturate({A}*6.0)*saturate((1.0-{A})*6.0))')
        .dyn(x='1.0', z='saturate({A}*1.2-0.15)'),
    burst('Dipper', 1, '1.1', 'Dipper').flat()
        .pos('float3(0.0,0.0,8.0)')
        .size('float2(1.0,1.0)*{R}*1.4*' + ON_S)
        .turn('{SEED}*360.0')
        .tint('float4({HOTG}*1.5,{C}.a*{GS}*pow(1.0-{A},1.2))')
        .dyn(z='saturate({A}*2.5)'),
    burst('Seal', 1, '1.0', 'Seal').flat()
        .pos('float3(0.0,0.0,7.0)')
        .size('float2(1.0,1.0)*min({R}*0.5,260.0)*(1.0+0.5*saturate(1.0-{A}*8.0))*' + ON_R)
        .turn('{SEED}*40.0-20.0')
        .tint('float4({CIN},{C}.a*{GR}*saturate({A}*12.0)*(1.0-pow({A},3.0)))')
        .dyn(x='1.0', w='0.0'),
])

# Charging (loops until deactivated, may follow the sword): a trigram formation and a counter-turning ensō, ink mist
# swirling in, brush strokes converging, a coloured core and the sword's character written over it stroke by stroke
# (斩, 封, 星 or 灭), plus orbiting talismans or seven stars, soul fire or cinnabar drops.
system('NS_SS_Charge', ['Radius'], [
    loop('Formation', 0.9, '2.2', 'Bagua').flat()
        .pos(ZERO)
        .size('float2(1.0,1.0)*min({R}*2.4,420.0)*(1.05-0.15*{A})')
        .turn('{AGE}*40.0+{H1}*360.0')
        .tint('float4({INK},{C}.a*0.8*{BELL})')
        .dyn(z='saturate({A}*3.0)', w='0.0'),
    loop('Inner', 0.9, '2.2', 'Enso').flat()
        .pos(ZERO)
        .size('float2(1.0,1.0)*min({R}*1.3,230.0)')
        .turn('-{AGE}*90.0')
        .tint('float4({HUE},{C}.a*0.7*{BELL})')
        .dyn(x='0.85', z='saturate({A}*3.0)', w='0.3'),
    loop('Swirl', 14, '1.0', 'InkPlume')
        .f('Transient.SSTheta', '{H1}*{TAU}+{A}*5.0')
        .f('Transient.SSE', '{R}*pow(1.0-{A},1.2)*(0.7+0.3*{H2})')
        .pos('float3(cos({TH})*{E},sin({TH})*{E},({H3}-0.5)*{R}*0.4*(1.0-{A}))')
        .size('float2(1.0,1.0)*{R}*0.4*(1.0-0.5*{A})')
        .turn('{H1}*360.0+{A}*120.0')
        .tint('float4({WASH},{C}.a*0.5*{BELL})')
        .dyn(z='0.2+{A}*0.5', w='0.6'),
    loop('Inflow', 40, '0.6', 'InkStroke')
        .v('Transient.SSDir', sphere('{H1}', '{H2}'))
        .pos('{DIR}*{R}*(0.15+0.95*pow(1.0-{A},1.5))')
        .along('-{DIR}')
        .size('float2({R}*0.05,{R}*0.3*(0.4+0.6*{A}))')
        .tint('float4({INK},{C}.a*0.85*{BELL})')
        .dyn(w='0.5'),
    # The glowing core is not 噬魂's: its charge burns instead (below).
    loop('Core', 8, '0.6')
        .pos(ZERO)
        .size('float2(1.0,1.0)*{R}*(0.42+0.06*sin({AGE}*9.0))*(0.6+0.4*{BELL})*' + OFF_V)
        .tint('float4({GLOW},{C}.a*0.5*{BELL})')
        .dyn(x='0.3'),
    loop('Word', 0.9, '2.2', 'Calli')
        .pos(ZERO)
        .size('float2(1.0,1.0)*min({R}*0.75,200.0)')
        .tint('float4({INK},{C}.a*{BELL})')
        .dyn(y='0.5', z='saturate({A}*2.2)', w=CHARACTER),
    # 噬魂's charge (插地蓄力, played 40 cm over the ground at the planted blade): a ring of violet fire roaring up round
    # the blade, a column of flame climbing it, and dark violet smoke rolling off the top.
    loop('FireGlow', 4, '0.7').flat()
        .pos('float3(0.0,0.0,-{R}*0.16)')
        .size('float2(1.0,1.0)*{R}*(2.4+0.4*{H1})*' + ON_V)
        .tint('float4({GLOW}*0.9,{C}.a*0.3*{BELL}*{GV})')
        .dyn(x='0.05'),
    fire(loop('FireRing', 40, '0.6+0.25*{H3}', 'Flame').f('Transient.SSTheta', '{H1}*{TAU}+{AGE}*0.7'),
         'float3(cos({TH}),sin({TH}),0.0)*{R}*(0.48+0.3*{H2})+float3(0.0,0.0,{R}*(0.12+0.3*{EASE}))',
         '{R}*(0.32+0.16*{H4})', '{R}*(0.62+0.3*{H4})', 'saturate({A}*7.0)*(1.0-{A}*{A})', broad='0.4+0.6*{H2}'),
    fire(loop('FireCore', 22, '0.5+0.2*{H3}', 'Flame'),
         'float3(({H1}-0.5)*{R}*0.24,({H2}-0.5)*{R}*0.24,{R}*(0.3+0.5*{EASE}))',
         '{R}*(0.36+0.14*{H4})', '{R}*(0.85+0.35*{H4})', 'saturate({A}*7.0)*(1.0-{A}*{A})', broad='0.5+0.5*{H2}'),
    fire_smoke(loop('FireSmoke', 6, '1.4', 'InkPlume'),
               'float3(({H1}-0.5)*{R}*0.7,({H2}-0.5)*{R}*0.7,{R}*(0.4+1.1*{EASE}))', '{R}*(0.45+0.6*{A})', '0.32*{BELL}'),
    loop('Talismans', 1.2, '2.5', 'Talisman')
        .f('Transient.SSTheta', '{H1}*{TAU}+{AGE}*1.2')
        .pos('float3(cos({TH}),sin({TH}),0.0)*{R}*0.9+float3(0.0,0.0,sin({AGE}*2.0+{H1}*6.0)*{R}*0.1)')
        .size('float2(min({R}*0.16,42.0),min({R}*0.42,110.0))*{BELL}*' + ON_G)
        .tint('float4({GLOW}*1.2,{C}.a*{GG}*{BELL})')
        .dyn(x='1.0', z='0.0'),
    loop('Stars', 7, '1.0', 'Star')
        .f('Transient.SSK', 'fmod({ID},7.0)')
        .f('Transient.SSTheta', '{K}*0.8975979+{AGE}*1.6')
        .pos('float3(cos({TH}),sin({TH}),0.0)*{R}*0.95+float3(0.0,0.0,sin({TH}*2.0+{AGE})*{R}*0.15)')
        .size('float2(1.0,1.0)*{R}*0.3*{BELL}*' + ON_S)
        .turn('{AGE}*80.0')
        .tint('float4({HOTG}*1.7,{C}.a*{GS}*{BELL})'),
    loop('Drops', 10, '0.7', 'InkSplat')
        .v('Transient.SSDir', sphere('{H1}', '{H2}'))
        .pos('{DIR}*{R}*1.1*pow(1.0-{A},1.4)')
        .size('float2(1.0,1.0)*{R}*0.06*(0.6+0.8*{H3})*' + ON_R)
        .turn('{H1}*360.0')
        .tint('float4({CIN},{C}.a*{GR}*{BELL})')
        .dyn(w='1.0'),
], warmup=1.2)

# Flying sword-qi (local +X is the flight direction): an ink crescent with the sword's colour inside it and a thin
# sword-light rim, an ink ribbon, mist and drops left in the world behind it, and the signature in its wake.
QI_SEED = '0.37'
system('NS_SS_Qi', ['Length', 'Width'], [
    loop('Mist', 22, '0.7', 'InkPlume').world().origin()
        .pos('{ORG}+' + sphere('{H1}', '{H2}') + '*{W}*0.4*{A}+float3(0.0,0.0,{W}*0.15*{A})')
        .size('float2(1.0,1.0)*{W}*(0.9+1.2*{A})')
        .turn('{H1}*360.0')
        .tint('float4({WASH},{C}.a*0.5*(1.0-{A}))')
        .dyn(z='{A}*0.8', w='0.6'),
    loop('Trail', 60, '0.3', 'InkRibbon').ribbon().world()
        .width('{W}*0.45*pow(1.0-{A},1.4)')
        .tint('float4({INK},{C}.a*(1.0-{A}*{A}))'),
    loop('Body', 7, '0.42', 'InkArc')
        .pos('float3({L}*0.02,0.0,0.0)').along('float3(1.0,0.0,0.0)')
        .size('float2({L}*1.05,{L}*0.62)')
        .tint('float4({INK},{C}.a*0.75*{BELL})')
        .dyn(y=QI_SEED, z='1.0', w='-0.75'),
    loop('Hue', 7, '0.42', 'InkArc')
        .pos('float3({L}*0.04,0.0,0.0)').along('float3(1.0,0.0,0.0)')
        .size('float2({L}*1.03,{L}*0.61)')
        .tint('float4({HUE},{C}.a*0.85*{BELL})')
        .dyn(x='0.95', y=QI_SEED, z='1.0', w='-1.45'),
    loop('Drops', 18, '0.5', 'InkSplat').world().origin()
        .pos('{ORG}+' + sphere('{H3}', '{H1}') + '*{L}*0.15*{EASE}-float3(0.0,0.0,{L}*0.3*{A}*{A})')
        .size('float2(1.0,1.0)*{L}*(0.025+0.035*{H2})')
        .turn('{H1}*360.0')
        .tint('float4(' + BLOOD + ',{C}.a*(1.0-{A}*{A}))')
        .dyn(w='1.0'),
    # 噬魂: a wake of violet fire left burning along the path.
    fire(loop('Fire', 20, '0.45+0.2*{H3}', 'Flame').world().origin(),
         '{ORG}+' + sphere('{H2}', '{H3}') + '*{L}*0.08+float3(0.0,0.0,{L}*(0.14+0.3*{EASE}))',
         '{L}*(0.15+0.08*{H4})', '{L}*(0.32+0.16*{H4})', 'saturate({A}*9.0)*(1.0-{A}*{A})'),
    fire_smoke(loop('FireSmoke', 5, '0.9', 'InkPlume').world().origin(),
               '{ORG}+float3(({H1}-0.5)*{L}*0.2,({H2}-0.5)*{L}*0.2,{L}*(0.3+0.6*{EASE}))', '{L}*(0.3+0.4*{A})', '0.3*{BELL}'),
    loop('Glints', 30, '0.45', 'Star').world().origin()
        .pos('{ORG}+' + sphere('{H3}', '{H1}') + '*{L}*0.1*{A}')
        .size('float2(1.0,1.0)*{L}*0.08*(1.0-{A})*(0.6+0.8*{H2})*' + ON_S)
        .turn('{H1}*90.0')
        .tint('float4({HOTG}*1.6,{C}.a*{GS}*(1.0-{A}))'),
    loop('Leaf', 10, '0.7', 'Shard').world().origin()
        .pos('{ORG}+' + sphere('{H1}', '{H3}') + '*{L}*0.12*{EASE}-float3(0.0,0.0,{L}*0.2*{A}*{A})')
        .size('float2({L}*0.03,{L}*0.06)*(1.0-0.3*{A})*' + ON_G)
        .turn('{H1}*360.0+{PAGE}*({H2}-0.5)*700.0')
        .tint('float4({GOLD}*0.85,{C}.a*{GG}*(1.0-{A}))')
        .dyn(x='0.75'),
])

# Ink lightning toward User.Target (relative): a jagged bolt of round-ended ink segments with a hot core in the
# sword's colour, ink forks, and an ink splash, drops and mist where it lands. Jitter depends only on the position
# along the bolt and time, so neighbouring segments share their joints.
SEGMENTS = 20
JITTER = ('({P1}*sin($t*21.0+{AGE}*55.0)+{P2}*cos($t*15.0+{AGE}*43.0)+{P1}*0.35*sin($t*57.0+{AGE}*91.0))'
          '*min({LEN}*0.07,55.0)*sin($t*{PI})')


def bolt_point(t):
    return '{T}*' + t + '+' + sub(JITTER, t=t)


def bolt(name, width, colour, life, occlusion, mat='Streak', w='0.0', overlap=1.25):
    n = '%.1f' % SEGMENTS
    return (burst(name, SEGMENTS, life, mat)
            .f('Transient.SSLen', 'max(length({T}),1.0)')
            .v('Transient.SSDir', '{T}/{LEN}')
            .v('Transient.SSP1', PERP1)
            .v('Transient.SSP2', PERP2)
            .f('Transient.SSK', 'fmod({ID},' + n + ')')
            .v('Transient.SSPa', bolt_point('({K}/' + n + ')'))
            .v('Transient.SSPb', bolt_point('(({K}+1.0)/' + n + ')'))
            .pos('({QA}+{QB})*0.5')
            .along('normalize({QB}-{QA}+float3(0.0,0.0,0.001))')
            .size('float2(' + width + ',length({QB}-{QA})*' + '%.2f' % overlap + ')')
            .tint(colour)
            .dyn(x=occlusion, z='1.0', w=w))


FORK = ('{V1}+{V2}*{E}*$k+({P1}*sin($k*11.4+{AGE}*60.0+{X1})+{P2}*cos($k*16.2+{AGE}*47.0))'
        '*min({E}*0.12,22.0)*sin($k*{PI})')
system('NS_SS_Beam', ['Width', 'Target'], [
    bolt('Ink', '{W}*2.6*(1.0-0.4*{A})+3.0', 'float4({INK},{C}.a*(1.0-{A}*{A}))', '0.34', '0.97', 'InkStroke', '-0.4', 1.5),
    bolt('Core', '{W}*0.35*(1.0-0.6*{A})+1.0', 'float4({HOTG}*1.5,{C}.a*(1.0-{A})*(0.65+0.35*sin({AGE}*80.0+{K})))', '0.24', '0.12', 'Laser', '0.0', 1.15),
    burst('Fork', 18, '0.24', 'InkStroke')
        .f('Transient.SSLen', 'max(length({T}),1.0)')
        .v('Transient.SSDir', '{T}/{LEN}')
        .v('Transient.SSP1', PERP1)
        .v('Transient.SSP2', PERP2)
        .f('Transient.SSX1', 'floor({ID}/6.0)')
        .f('Transient.SSK', 'fmod({ID},6.0)')
        .f('Transient.SSX2', '0.22+0.56*frac(sin({X1}*91.7+3.1)*43758.5453)')
        .f('Transient.SSTheta', 'frac(sin({X1}*47.3+1.7)*43758.5453)*{TAU}')
        .v('Transient.SSV1', bolt_point('{X2}'))
        .v('Transient.SSV2', 'normalize({DIR}*0.55+{P1}*cos({TH})+{P2}*sin({TH}))')
        .f('Transient.SSE', '{LEN}*(0.16+0.12*frac(sin({X1}*13.1+0.7)*43758.5453))')
        .v('Transient.SSPa', sub(FORK, k='({K}/6.0)'))
        .v('Transient.SSPb', sub(FORK, k='(({K}+1.0)/6.0)'))
        .pos('({QA}+{QB})*0.5')
        .along('normalize({QB}-{QA}+float3(0.0,0.0,0.001))')
        .size('float2({W}*1.4*(1.0-{K}/6.0)+1.5,length({QB}-{QA})*1.2)')
        .tint('float4({INK},{C}.a*(1.0-{A}))')
        .dyn(z='1.0', w='-0.5'),
    burst('Splat', 1, '0.5', 'InkSplat')
        .pos('{T}')
        .size('float2(1.0,1.0)*{W}*10.0*(0.55+0.45*{EASE})')
        .turn('{H1}*360.0')
        .tint('float4({INK},{C}.a*(1.0-{A}*{A}))')
        .dyn(z='{A}', w='0.0'),
    burst('Glow', 1, '0.3')
        .pos('{T}')
        .size('float2(1.0,1.0)*{W}*6.0*(0.6+0.4*{A})')
        .tint('float4({GLOW}*1.2,{C}.a*pow(1.0-{A},1.5))')
        .dyn(x='0.3'),
    burst('Drops', 10, '0.45', 'InkSplat')
        .v('Transient.SSDir', 'normalize(' + sphere('{H1}', '{H2}') + '+float3(0.0,0.0,0.4))')
        .pos('{T}+{DIR}*{W}*7.0*{EASE}-float3(0.0,0.0,{W}*4.0*{A}*{A})')
        .size('float2(1.0,1.0)*{W}*(0.5+0.6*{H3})')
        .turn('{H1}*360.0')
        .tint('float4({INK},{C}.a*(1.0-{A}*{A}))')
        .dyn(w='1.0'),
    burst('Wash', 2, '0.7', 'InkPlume')
        .pos('{T}+float3(({H1}-0.5)*{W}*3.0,({H2}-0.5)*{W}*3.0,{W}*2.0*{A})')
        .size('float2(1.0,1.0)*{W}*(5.0+5.0*{EASE})')
        .turn('{H1}*360.0')
        .tint('float4({WASH},{C}.a*0.5*(1.0-{A}))')
        .dyn(z='{A}*0.8'),
])

# Guardian shield of 七星 (loops, follows the anchor): a trigram formation and the Big Dipper chart under the feet, an
# ensō brushed round the anchor with a pale-gold circle inside it, seven stars orbiting, auspicious clouds drifting
# round the base and glints on the shell.
system('NS_SS_Shield', ['Radius'], [
    loop('Formation', 0.5, '4.0', 'Bagua').flat()
        .pos('float3(0.0,0.0,-{R}*0.58)')
        .size('float2(1.0,1.0)*{R}*2.5')
        .turn('{AGE}*15.0')
        .tint('float4({INK},{C}.a*0.55*{BELL})')
        .dyn(z='1.0', w='0.0'),
    loop('Chart', 0.5, '4.0', 'Dipper').flat()
        .pos('float3(0.0,0.0,-{R}*0.55)')
        .size('float2(1.0,1.0)*{R}*1.7')
        .turn('-{AGE}*10.0')
        .tint('float4({HOTG}*1.4,{C}.a*{BELL})')
        .dyn(z='saturate({A}*4.0)'),
    loop('Shell', 1.0, '2.0', 'Enso')
        .pos(ZERO)
        .size('float2(1.0,1.0)*{R}*2.2')
        .tint('float4({INK},{C}.a*0.75*{BELL})')
        .dyn(y='{H1}', z='saturate({A}*3.0)', w='0.25'),
    loop('Halo', 1.0, '2.0', 'Enso')
        .pos(ZERO)
        .size('float2(1.0,1.0)*{R}*2.05')
        .tint('float4({HUE},{C}.a*0.8*{BELL})')
        .dyn(x='0.8', y='{H1}+0.5', z='saturate({A}*3.0)', w='0.08'),
    loop('Stars', 7, '2.0', 'Star')
        .f('Transient.SSK', 'fmod({ID},7.0)')
        .f('Transient.SSTheta', '{K}*0.8975979+{AGE}*0.9')
        .pos('float3(cos({TH})*{R}*1.08,sin({TH})*{R}*1.08,sin({TH})*{R}*0.25+{R}*0.15)')
        .size('float2(1.0,1.0)*{R}*0.38*(0.8+0.2*sin({PAGE}*9.0+{K}))*{BELL}')
        .turn('{AGE}*60.0')
        .tint('float4({HOTG}*1.8,{C}.a*0.9*{BELL})'),
    loop('Clouds', 1.6, '2.5', 'Cloud')
        .f('Transient.SSTheta', '{H1}*{TAU}+{A}*0.6')
        .pos('float3(cos({TH}),sin({TH}),0.0)*{R}*0.95+float3(0.0,0.0,-{R}*0.42+{A}*{R}*0.2)')
        .size('float2({R}*0.75,{R}*0.45)*(0.8+0.3*{H2})')
        .tint('float4(lerp({INK},{GLOW}*1.1,step(0.5,{H3})),{C}.a*0.85*{BELL})')
        .dyn(x='0.9'),
    loop('Glints', 30, '1.0')
        .v('Transient.SSDir', sphere('{H1}', '{H2}'))
        .pos('{DIR}*{R}*0.98+float3(0.0,0.0,{A}*{R}*0.2)')
        .size('float2(1.0,1.0)*({R}*0.03*{BELL}+0.5)')
        .tint('float4({HOTG}*1.4,{C}.a*0.8*{BELL})'),
], warmup=2.0)

# Devour of 噬魂 (LiveSwordVfx::DevourAttached: it follows the blade, world-aligned; User.Source is the victim relative to
# it and is kept current). A violet serpent of black ink, its head and neck wreathed in violet fire and its eye blood red,
# shoots from the blade on an arching body, jaws gaping, and snaps shut on the victim at DEVOUR_BITE; ink, fire and violet
# light burst from the bite while the head pulls back with its prey; violet essence (精元) flames up where the victim was
# and spirals into the blade, which drinks it at DEVOUR_ABSORB under a closing violet ring. User.Flip (±1) mirrors the
# head so its crown stays up on screen. Timings match LiveSwordVfx.h.
DEVOUR_BITE, DEVOUR_ABSORB = 0.42, 1.3
DEVOUR_PATH = '{SRC}*(1.0-$s)+({P1}*cos($p*9.0+{TH})+{P2}*sin($p*9.0+{TH}))*{R}*(0.5+0.5*{H3})*(1.0-$s)'
SERPENT_SIZE = '{R}*5.0'


def ss(a, b, x):
    """smoothstep(a, b, x) spelt out: the CPU particle scripts compile to the vector VM, which has no smoothstep."""
    t = 'saturate((%s-(%s))/((%s)-(%s)))' % (x, a, b, a)
    return '(%s*%s*(3.0-2.0*%s))' % (t, t, t)


def devour_point(p):
    return sub(DEVOUR_PATH, p=p, s='(%s*%s*(3.0-2.0*%s))' % (p, p, p))


def devour_base(e):
    return (e.f('Transient.SSLen', 'max(length({SRC}),1.0)')
            .v('Transient.SSDir', '{SRC}/{LEN}')
            .v('Transient.SSP1', PERP1)
            .v('Transient.SSP2', PERP2))


def serpent(e):
    """Head motion on the effect clock ({PAGE} of a burst particle born with the system): {V1} the blade-side start, {V2}
    where the head centre sits when its open mouth closes on the victim, {E} strike progress (fast out of the blade),
    {K} retreat progress after the bite, {QB} the head centre and {RHO} its sprite size."""
    return (devour_base(e)
            .v('Transient.SSV1', 'float3(0.0,0.0,{R}*0.7)')
            .v('Transient.SSV2', '{SRC}-{DIR}*' + SERPENT_SIZE + '*0.05+float3(0.0,0.0,' + SERPENT_SIZE + '*0.04)')
            .f('Transient.SSE', '(1.0-pow(1.0-saturate({PAGE}/%.2f),2.4))' % DEVOUR_BITE)
            .f('Transient.SSK', 'saturate(({PAGE}-0.5)/0.45)')
            .v('Transient.SSPa', 'lerp({V1},{V2},{E})+float3(0.0,0.0,1.0)*sin({E}*{PI})*{LEN}*0.22')
            .v('Transient.SSPb', 'lerp({QA},{V1}+float3(0.0,0.0,{R}*0.4),{K}*{K})')
            .f('Transient.SSRho', SERPENT_SIZE + '*(0.55+0.45*saturate({PAGE}/0.25))*(1.0-0.55*{K})'))


# Shown from the first frame, gone once the head has pulled back into the blade.
SERPENT_ALPHA = 'saturate({PAGE}/0.06)*saturate((1.02-{PAGE})/0.22)'
# Jaws: barely parted leaving the blade, gaping on the approach, snapping shut at the bite, then a swallowing twitch.
SERPENT_JAW = ('saturate(0.06+0.94*' + ss('0.05', '0.3', '{PAGE}') + '*(1.0-' + ss('%.2f' % (DEVOUR_BITE - 0.03), '%.2f' % (DEVOUR_BITE + 0.05), '{PAGE}')
               + ')+0.08*step(%.2f,{PAGE})*sin(saturate(({PAGE}-0.5)*2.5)*{PI}*3.0))' % (DEVOUR_BITE + 0.05))
SERPENT_FLIP = 'step(0.0,-{FLIP})'
# Its flames lean back against the strike, standing up again once it has bitten.
SERPENT_LEAN = 'normalize(float3(0.0,0.0,1.0)-normalize({V2}-{V1}+float3(0.0,0.0,0.001))*0.45*(1.0-{K}))'
# Body point k (0 at the blade, 1 at the neck): an arching S that ripples toward the head.
SERPENT_NECK = '({QB}-normalize({V2}-{V1}+float3(0.0,0.0,0.001))*{RHO}*0.46)'
SERPENT_BODY = ('lerp(float3(0.0,0.0,0.0),' + SERPENT_NECK + ',{X1})+float3(0.0,0.0,1.0)*sin({X1}*{PI})*{LEN}*0.1'
                '+{P1}*sin({X1}*9.4-{PAGE}*16.0)*{R}*0.3*sin({X1}*{PI})')
ESSENCE = '{GLOW}*1.25+{HOTG}*0.12'

system('NS_SS_Devour', ['Radius', 'Source', 'Flip'], [
    # Ink smoke left along the strike as the head passes, billowing and thinning.
    devour_base(burst('Wake', 8, '1.3', 'InkPlume'))
        .f('Transient.SSX1', '(fmod({ID},8.0)+0.5)/8.0')
        .f('Transient.SSX2', 'max({PAGE}-{X1}*%.2f,0.0)' % DEVOUR_BITE)
        .pos('lerp(float3(0.0,0.0,{R}*0.7),{SRC},{X1}*0.92)+float3(0.0,0.0,1.0)*sin({X1}*{PI})*{LEN}*0.2'
             '+' + sphere('{H1}', '{H2}') + '*{R}*0.5*saturate({X2})')
        .size('float2(1.0,1.0)*{R}*(1.0+1.6*saturate({X2}*1.2))*step({X1}*%.2f,{PAGE})' % DEVOUR_BITE)
        .turn('{H1}*360.0+{X2}*40.0')
        .tint('float4({INK},{C}.a*0.55*saturate({X2}*8.0)*saturate(1.0-{X2}/0.85))')
        .dyn(z='saturate({X2}*1.1)', w='0.5'),
    serpent(burst('Body', 40, '1.05', 'InkRibbon').ribbon())
        .f('Transient.SSX1', 'fmod({ID},40.0)/39.0')
        .pos(SERPENT_BODY)
        .width('{RHO}*0.3*(0.3+0.7*{X1})')
        .tint('float4(lerp({INK},{CINK},0.55),{C}.a*' + SERPENT_ALPHA + ')')
        .dyn(z='0.22'),
    serpent(burst('Core', 40, '1.05', 'InkRibbon').ribbon())
        .f('Transient.SSX1', 'fmod({ID},40.0)/39.0')
        .pos(SERPENT_BODY)
        .width('{RHO}*0.07*(0.3+0.7*{X1})')
        .tint('float4({GLOW}*0.7,{C}.a*0.55*' + SERPENT_ALPHA + ')')
        .dyn(x='0.1', y='0.61', z='0.55'),
    serpent(burst('HeadGlow', 1, '1.05'))
        .pos('{QB}+float3(0.0,0.0,{RHO}*0.08)')
        .size('float2(1.0,1.0)*{RHO}*1.5*' + ON_V)
        .tint('float4({GLOW}*0.9,{C}.a*0.3*' + SERPENT_ALPHA + '*{GV})')
        .dyn(x='0.05'),
    # The serpent burns: a mane of fire along the neck, and tall flames rising off the crown and the back of the skull,
    # licking up and drifting back, leaning back while it strikes. They are drawn behind the head, so they rise from its
    # outline and never hide the eye; the fire on the head itself is in M_SS_Serpent. Each flame cycles on its own clock
    # ({X1} or {X2}) for as long as the serpent is out.
    serpent(burst('NeckFire', 36, '1.05', 'Flame'))
        .f('Transient.SSX1', '0.45+0.55*{H1}')
        .f('Transient.SSX2', 'frac({PAGE}*(2.0+1.0*{H2})+{H3})')
        .pos(SERPENT_BODY + '+float3(0.0,0.0,{RHO}*(0.05+0.08*{X2}+(0.1+0.05*{H4})*sin({X2}*{PI})))')
        .along(SERPENT_LEAN)
        .size('float2({RHO}*(0.14+0.06*{H4}),{RHO}*(0.28+0.14*{H4}))*sin({X2}*{PI})*' + ON_V)
        .tint('float4(' + FIRE_TINT + ',{C}.a*' + SERPENT_ALPHA + '*{GV})')
        .dyn(x=FIRE_OCCLUSION, z='{X2}', w='0.4+0.4*{H4}'),
    serpent(burst('HeadFire', 36, '1.05', 'Flame'))
        .f('Transient.SSX1', 'frac({PAGE}*(2.4+1.2*{H2})+{H1})')
        .pos('{QB}+normalize({V2}-{V1}+float3(0.0,0.0,0.001))*{RHO}*(0.12-0.48*{H3}-0.2*{X1})'
             '+float3(0.0,0.0,{RHO}*(0.02+0.1*sin({H3}*{PI})+0.08*{X1}+(0.19+0.1*{H4})*sin({X1}*{PI})))+{P1}*({H4}-0.5)*{RHO}*0.2')
        .along(SERPENT_LEAN)
        .size('float2({RHO}*(0.24+0.1*{H4}),{RHO}*(0.45+0.25*{H4}))*sin({X1}*{PI})*' + ON_V)
        .tint('float4(' + FIRE_TINT + ',{C}.a*' + SERPENT_ALPHA + '*{GV})')
        .dyn(x=FIRE_OCCLUSION, z='{X1}', w='0.5+0.5*{H2}'),
    serpent(burst('Head', 1, '1.05', 'Serpent'))
        .pos('{QB}')
        .along('normalize({V2}-{V1}+float3(0.0,0.0,0.001))')
        .size('float2(1.0,1.0)*{RHO}')
        .tint('float4({GLOW}*1.5,{C}.a*' + SERPENT_ALPHA + ')')
        .dyn(x='1.0', y=SERPENT_FLIP, z=SERPENT_JAW, w='0.0'),
    # The bite: an ink splash and violet light where the jaws closed, a violet shock ring and a puff of ink smoke.
    burst('Splash', 1, '0.6', 'InkSplat').after(DEVOUR_BITE)
        .pos('{SRC}')
        .size('float2(1.0,1.0)*{R}*2.6*(0.6+0.4*{EASE})')
        .turn('{H1}*360.0')
        .tint('float4({INK},{C}.a*(1.0-{A}*{A}))')
        .dyn(z='{A}', w='0.0'),
    burst('Bite', 1, '0.32').after(DEVOUR_BITE)
        .pos('{SRC}')
        .size('float2(1.0,1.0)*{R}*2.4*(0.6+0.4*{A})')
        .tint('float4({GLOW}*1.6,{C}.a*pow(1.0-{A},1.6))')
        .dyn(x='0.3'),
    burst('Shock', 1, '0.45', 'Ring').after(DEVOUR_BITE)
        .pos('{SRC}')
        .size('float2(1.0,1.0)*{R}*(0.8+2.6*{EASE})')
        .tint('float4({GLOW}*1.4,{C}.a*(1.0-{A}))')
        .dyn(x='0.3', w='0.15'),
    burst('Gulp', 3, '0.9', 'InkPlume').after(DEVOUR_BITE)
        .pos('{SRC}+' + sphere('{H1}', '{H2}') + '*{R}*0.6*{EASE}+float3(0.0,0.0,{R}*0.5*{A})')
        .size('float2(1.0,1.0)*{R}*(1.1+1.3*{EASE})')
        .turn('{H3}*360.0')
        .tint('float4({INK},{C}.a*0.7*(1.0-{A}))')
        .dyn(z='{A}', w='0.6'),
    fire(burst('BiteFire', 12, '0.55+0.3*{H3}', 'Flame').after(DEVOUR_BITE).f('Transient.SSTheta', '{H1}*{TAU}'),
         '{SRC}+float3(cos({TH}),sin({TH}),0.0)*{R}*0.7*(0.3+0.7*{H2})+float3(0.0,0.0,{R}*(-0.3+1.1*{EASE}))',
         '{R}*(0.5+0.3*{H4})', '{R}*(1.1+0.6*{H4})', 'saturate({A}*10.0)*(1.0-{A}*{A})'),
    # 精元: violet essence flames out of the swallowed victim, then spirals into the blade, arriving at DEVOUR_ABSORB.
    devour_base(burst('Essence', 22, '%.2f' % (DEVOUR_ABSORB - DEVOUR_BITE - 0.04), 'Flame')).after(DEVOUR_BITE + 0.04)
        .f('Transient.SSProgress', ss('0.16', '1.0', '{A}+({H1}-0.5)*0.12'))
        .f('Transient.SSTheta', '{H2}*{TAU}')
        .v('Transient.SSV1', 'normalize(' + sphere('{H3}', '{H4}') + '+float3(0.0,0.0,0.8))')
        .v('Transient.SSPa', devour_point('{PR}'))
        .v('Transient.SSPb', devour_point('min({PR}+0.03,1.0)'))
        .pos('{QA}+{V1}*{R}*1.2*sin(saturate({A}*3.0)*{PI})*(1.0-{PR})')
        # Flames trail their tips behind them as they fly into the blade.
        .along('-normalize({QB}-{QA}+{V1}*0.02+float3(0.0,0.0,0.001))')
        .size('float2({R}*0.5,{R}*1.1)*(0.6+0.4*{H3})*(1.0-0.45*{PR})')
        .tint('float4(' + ESSENCE + ',{C}.a*saturate({A}*10.0)*saturate((1.0-{PR})*5.0+0.15))')
        .dyn(x='0.7', w='0.85'),
    devour_base(burst('EssenceInk', 10, '%.2f' % (DEVOUR_ABSORB - DEVOUR_BITE - 0.04), 'InkPlume')).after(DEVOUR_BITE + 0.04)
        .f('Transient.SSProgress', ss('0.1', '1.0', '{A}+({H1}-0.5)*0.15'))
        .f('Transient.SSTheta', '{H2}*{TAU}')
        .v('Transient.SSV1', 'normalize(' + sphere('{H3}', '{H4}') + '+float3(0.0,0.0,0.8))')
        .pos(devour_point('{PR}') + '+{V1}*{R}*0.9*sin(saturate({A}*3.0)*{PI})*(1.0-{PR})')
        .size('float2(1.0,1.0)*{R}*(0.9+0.5*{H3})*(1.0-0.5*{PR})')
        .turn('{H1}*360.0+{A}*90.0')
        .tint('float4(lerp({CINK},{GLOW}*0.35,0.35),{C}.a*0.6*saturate({A}*8.0)*saturate((1.0-{PR})*4.0))')
        .dyn(z='{A}', w='0.35'),
    devour_base(burst('Flow', 12, '%.2f' % (DEVOUR_ABSORB - DEVOUR_BITE - 0.1), 'Streak')).after(DEVOUR_BITE + 0.1)
        .f('Transient.SSProgress', ss('0.0', '1.0', '{A}+({H1}-0.5)*0.15'))
        .f('Transient.SSTheta', '{H2}*{TAU}')
        .v('Transient.SSPa', devour_point('{PR}'))
        .v('Transient.SSPb', devour_point('min({PR}+0.05,1.0)'))
        .pos('{QA}')
        .along('normalize({QB}-{QA}+float3(0.0,0.0,0.001))')
        .size('float2({R}*0.06,{R}*0.9)*(1.0-0.5*{PR})')
        .tint('float4({GLOW}*0.9+{HOTG}*0.1,{C}.a*0.8*saturate({A}*8.0)*saturate((1.0-{PR})*4.0))')
        .dyn(x='0.55', w='4.0'),
    # The blade drinks it: a violet ring closing onto the sword, a swelling glow and a flare.
    burst('Drink', 1, '0.4', 'Ring').after(DEVOUR_ABSORB - 0.18)
        .pos(ZERO)
        .size('float2(1.0,1.0)*{R}*(3.2-2.8*{EASE})')
        .tint('float4({GLOW}*1.5,{C}.a*' + ss('0.0', '0.25', '{A}') + '*(1.0-{A}*{A}))')
        .dyn(x='0.3', w='0.4'),
    burst('Swell', 1, '0.5').after(DEVOUR_ABSORB - 0.1)
        .pos(ZERO)
        .size('float2(1.0,1.0)*{R}*(1.0+1.2*{BELL})')
        .tint('float4({GLOW}*1.3,{C}.a*0.85*{BELL})')
        .dyn(x='0.3'),
    burst('Flare', 1, '0.35', 'Star').after(DEVOUR_ABSORB)
        .pos(ZERO)
        .size('float2(1.0,1.0)*{R}*2.6*(1.0-0.4*{A})')
        .turn('{H1}*90.0')
        .tint('float4({HOTG}*1.7,{C}.a*(1.0-{A}))'),
])

# Shatter: an ink splash and wash, a coloured flash, ink fragments spinning up and falling, drops and glints, plus the
# signature: soul fire, plum petals, gold leaf or stars.
SHARD_DIR = ('float3(cos({H1}*{TAU})*sqrt(saturate(1.0-(0.2+0.8*{H2})*(0.2+0.8*{H2}))),'
             'sin({H1}*{TAU})*sqrt(saturate(1.0-(0.2+0.8*{H2})*(0.2+0.8*{H2}))),0.2+0.8*{H2})')
system('NS_SS_Shatter', ['Size'], [
    burst('Wash', 4, '0.9', 'InkPlume')
        .v('Transient.SSDir', sphere('{H1}', '{H2}'))
        .pos('{DIR}*{SZ}*0.4*{EASE}+float3(0.0,0.0,{SZ}*0.2*{A})')
        .size('float2(1.0,1.0)*{SZ}*(0.45+0.5*{EASE})')
        .turn('{H3}*360.0')
        .tint('float4({WASH},{C}.a*0.55*(1.0-{A}))')
        .dyn(z='{A}*0.8'),
    burst('Splat', 1, '0.55', 'InkSplat')
        .pos(ZERO)
        .size('float2(1.0,1.0)*{SZ}*(0.55+0.45*{EASE})')
        .turn('{SEED}*360.0')
        .tint('float4({INK},{C}.a*(1.0-{A}*{A}))')
        .dyn(z='{A}', w='0.0'),
    burst('Flash', 1, '0.2')
        .pos(ZERO)
        .size('float2(1.0,1.0)*{SZ}*(0.5+0.4*{A})')
        .tint('float4({GLOW}*1.2,{C}.a*pow(1.0-{A},2.0))')
        .dyn(x='0.35'),
    burst('Shards', 18, '0.7', 'InkStroke')
        .v('Transient.SSDir', SHARD_DIR)
        .f('Transient.SSE', '{SZ}*(0.7+0.8*{H3})')
        .pos('{DIR}*{E}*{A}*1.6+float3(0.0,0.0,-{SZ}*2.2*{A}*{A})')
        .along('normalize({DIR}*{E}*1.6-float3(0.0,0.0,{SZ}*4.4*{A})+float3(0.0,0.0,0.001))')
        .size('float2({SZ}*0.12,{SZ}*0.3)*(0.6+0.6*{H3})*(1.0-0.4*{A})')
        .tint('float4({INK},{C}.a*pow(1.0-{A},1.2))')
        .dyn(w='0.45'),
    burst('Drops', 14, '0.8', 'InkSplat')
        .v('Transient.SSDir', sphere('{H2}', '{H1}'))
        .pos('{DIR}*{SZ}*(0.2+0.6*{EASE})-float3(0.0,0.0,{SZ}*0.6*{A}*{A})')
        .size('float2(1.0,1.0)*{SZ}*(0.035+0.05*{H3})')
        .turn('{H1}*360.0')
        .tint('float4(' + BLOOD + ',{C}.a*(1.0-{A}*{A}))')
        .dyn(w='1.0'),
    burst('Glints', 8, '0.45', 'Star')
        .v('Transient.SSDir', sphere('{H1}', '{H3}'))
        .pos('{DIR}*{SZ}*0.6*{EASE}')
        .size('float2(1.0,1.0)*{SZ}*0.16*(1.0-{A})*(0.5+0.5*sin({PAGE}*30.0+{ID}))')
        .turn('{H2}*90.0')
        .tint('float4({HOTG}*1.5,{C}.a*(1.0-{A}))'),
    fire(burst('Fire', 8, '0.5+0.2*{H3}', 'Flame').v('Transient.SSDir', sphere('{H3}', '{H2}')),
         '{DIR}*{SZ}*0.3*{EASE}+float3(0.0,0.0,{SZ}*0.45*{EASE})',
         '{SZ}*(0.22+0.12*{H4})', '{SZ}*(0.48+0.25*{H4})', 'saturate({A}*10.0)*(1.0-{A}*{A})'),
    burst('Petals', 8, '1.0', 'Blossom')
        .v('Transient.SSDir', sphere('{H2}', '{H3}'))
        .pos('{DIR}*{SZ}*(0.2+0.6*{EASE})-float3(0.0,0.0,{SZ}*0.4*{A}*{A})+' + SWAY + '*{SZ}*0.06')
        .size('float2(1.0,1.0)*{SZ}*(0.09+0.05*{H4})*' + ON_R)
        .turn('{H1}*360.0+{PAGE}*({H2}-0.5)*500.0')
        .tint('float4({PLUM},{C}.a*{GR}*(1.0-{A}*{A}))')
        .dyn(w='step({H4},0.7)'),
    burst('Leaf', 10, '0.8', 'Shard')
        .v('Transient.SSDir', sphere('{H1}', '{H2}'))
        .pos('{DIR}*{SZ}*(0.2+0.7*{EASE})-float3(0.0,0.0,{SZ}*0.3*{A}*{A})')
        .size('float2({SZ}*0.04,{SZ}*0.08)*(1.0-0.3*{A})*' + ON_G)
        .turn('{H1}*360.0+{PAGE}*({H2}-0.5)*700.0')
        .tint('float4({GOLD}*0.85,{C}.a*{GG}*(1.0-{A}))')
        .dyn(x='0.75'),
    burst('Stars', 6, '0.6', 'Star')
        .v('Transient.SSDir', sphere('{H3}', '{H1}'))
        .pos('{DIR}*{SZ}*(0.2+0.6*{EASE})')
        .size('float2(1.0,1.0)*{SZ}*(0.3+0.2*{H2})*{BELL}*' + ON_S)
        .turn('{H1}*90.0')
        .tint('float4({HOTG}*1.7,{C}.a*{GS}*{BELL})'),
])

# Pillar of light from the ground (dispel, blessing, devastating strike): a trigram formation and ensō on the ground,
# a lotus opening in the sword's colour, a light pillar with ink strokes spiralling up round it, ink mist rolling
# out, auspicious clouds gathering at its foot, rising motes, a star, and burning talismans for 驱煞.
system('NS_SS_Pillar', ['Radius'], [
    burst('Formation', 1, '1.2', 'Bagua').flat()
        .pos('float3(0.0,0.0,4.0)')
        .size('float2(1.0,1.0)*{R}*2.6*(0.5+0.6*{EASE})')
        .turn('-{A}*40.0+{SEED}*360.0')
        .tint('float4({INK},{C}.a*0.85*pow(1.0-{A},1.1)*saturate({A}*8.0+0.1))')
        .dyn(z='saturate({A}*2.5)', w='0.0'),
    burst('Ring', 1, '0.7', 'Enso').flat()
        .pos('float3(0.0,0.0,6.0)')
        .size('float2(1.0,1.0)*{R}*2.0*(0.3+1.1*{EASE})')
        .tint('float4({INK},{C}.a*pow(1.0-{A},1.2))')
        .dyn(y='{SEED}', z='saturate({A}*4.0)', w='0.3'),
    burst('Wash', 10, '1.2', 'InkPlume')
        .f('Transient.SSTheta', '{ID}/10.0*{TAU}+{H1}*0.4')
        .pos('float3(cos({TH}),sin({TH}),0.0)*{R}*(0.3+0.7*{EASE})+float3(0.0,0.0,{R}*0.15+{R}*0.3*{A}*{H2})')
        .size('float2(1.0,1.0)*{R}*(0.5+0.4*{EASE})*(0.7+0.6*{H3})')
        .turn('{H1}*360.0')
        .tint('float4({WASH},{C}.a*0.5*(1.0-{A}))')
        .dyn(z='{A}*0.9'),
    burst('Lotus', 16, '1.1', 'Wisp').flat()
        .f('Transient.SSK', 'floor({ID}/8.0)')
        .f('Transient.SSTheta', 'fmod({ID},8.0)*0.7853982+{K}*0.3926991')
        .v('Transient.SSDir', 'float3(cos({TH}),sin({TH}),0.0)')
        .pos('{DIR}*{R}*(0.35+0.35*{K})*(0.6+0.4*{EASE})+float3(0.0,0.0,6.0+{K}*2.0)')
        .along('{DIR}')
        .size('float2({R}*(0.32+0.12*{K}),{R}*(0.6+0.25*{K}))*(0.4+0.6*{EASE})')
        .tint('float4({HUE},{C}.a*0.85*pow(1.0-{A},1.2)*saturate({A}*6.0+0.1))')
        .dyn(x='0.85', w='0.0'),
    burst('Beam', 1, '1.0', 'Streak')
        .pos('float3(0.0,0.0,{R}*3.2)')
        .along('float3(0.0,0.0,1.0)')
        .size('float2({R}*0.9*(1.0-{A}*{A})*saturate({A}*12.0+0.05),{R}*6.8)')
        .tint('float4({GLOW}*1.05,{C}.a*pow(1.0-{A},1.2))')
        .dyn(x='0.6', w='3.0'),
    burst('Core', 1, '1.0', 'Streak')
        .pos('float3(0.0,0.0,{R}*3.2)')
        .along('float3(0.0,0.0,1.0)')
        .size('float2({R}*0.22*(1.0-{A}*{A})*saturate({A}*12.0+0.05),{R}*6.4)')
        .tint('float4({HOTG}*1.6,{C}.a*{FADE})')
        .dyn(x='0.1', w='4.0'),
    burst('Spiral', 30, '1.0', 'InkStroke')
        .f('Transient.SSK', 'floor({ID}/2.0)/15.0')
        .f('Transient.SSTheta', '{K}*{TAU}*2.0+fmod({ID},2.0)*{PI}+{A}*4.0')
        .pos('float3(cos({TH}),sin({TH}),0.0)*{R}*0.62*(1.0-0.3*{K})+float3(0.0,0.0,{K}*{R}*5.5*(0.3+0.7*{EASE}))')
        .along('normalize(float3(-sin({TH}),cos({TH}),0.7))')
        .size('float2({R}*0.12,{R}*0.55)*(1.0-0.4*{A})')
        .tint('float4({INK},{C}.a*0.9*pow(1.0-{A},1.2)*saturate({A}*6.0+0.1))')
        .dyn(w='0.55'),
    burst('Clouds', 6, '1.3', 'Cloud')
        .f('Transient.SSTheta', '{ID}/6.0*{TAU}+{SEED}*{TAU}')
        .pos('float3(cos({TH}),sin({TH}),0.0)*{R}*(1.2-0.35*{EASE})+float3(0.0,0.0,{R}*(0.35+0.25*{H1}))')
        .size('float2({R}*1.1,{R}*0.66)*(0.8+0.3*{H2})')
        .tint('float4(lerp({INK},{GLOW}*1.1,step(0.5,{H3})),{C}.a*0.9*saturate({A}*5.0)*pow(1.0-{A},1.3))')
        .dyn(x='0.9'),
    burst('Motes', 24, '0.9')
        .f('Transient.SSTheta', '{H1}*{TAU}')
        .pos('float3(cos({TH}),sin({TH}),0.0)*{R}*0.8*{H2}+float3(0.0,0.0,{A}*{R}*5.0*(0.4+0.6*{H3}))')
        .size('float2(1.0,1.0)*({R}*0.06*{BELL}+0.5)')
        .tint('float4({GLOW}*1.1,{C}.a*{BELL})'),
    # Capped size and climb: the game camera looks down at 35 degrees from 47 m, so tall rising papers sweep toward
    # the lens and fill the portrait frame (the 驱煞 ultimate plays this at R=320).
    burst('Talismans', 6, '1.3', 'Talisman')
        .f('Transient.SSTheta', '{ID}/6.0*{TAU}+{A}*3.0')
        .pos('float3(cos({TH}),sin({TH}),0.0)*{R}*0.9+float3(0.0,0.0,{R}*0.4+min({R},180.0)*2.8*{EASE})')
        .size('float2(min({R}*0.2,48.0),min({R}*0.5,120.0))*' + ON_G)
        .turn('({H1}-0.5)*40.0')
        .tint('float4({GLOW}*1.2,{C}.a*{GG}*saturate({A}*8.0)*saturate((1.0-{A})*5.0))')
        .dyn(x='1.0', z='saturate({A}*1.4-0.3)'),
    burst('Flare', 1, '0.6', 'Star')
        .pos('float3(0.0,0.0,{R}*0.3)')
        .size('float2(1.0,1.0)*{R}*1.5*(1.0-0.4*{A})')
        .turn('{SEED}*90.0')
        .tint('float4({HOTG}*1.4,{C}.a*pow(1.0-{A},1.5))'),
])

# Burning victim of 噬魂 (loops until deactivated, attached level at the enemy's feet; User.Radius its body radius,
# User.Height how tall it stands): broad flames roaring up from the ground round its feet, flames hugging its body all
# the way up and licking over its head, so it stands in one fire; a violet glow on the ground under it and on its body,
# and dark violet smoke rolling up above. No sparks or embers: fire only. Not gated by hue, so any colour burns in its own
# fire.
system('NS_SS_Burn', ['Radius', 'Height'], [
    loop('Glow', 5, '0.6').flat()
        .pos('float3(0.0,0.0,2.0)')
        .size('float2(1.0,1.0)*{R}*(3.0+0.6*{H1})')
        .tint('float4({GLOW}*0.9,{C}.a*0.28*{BELL})')
        .dyn(x='0.05'),
    loop('BodyGlow', 4, '0.5')
        .pos('float3(0.0,0.0,{HT}*0.5)')
        .size('float2(1.0,1.0)*max({R}*2.4,{HT}*0.9)*(0.9+0.2*{H1})')
        .tint('float4({GLOW}*0.7,{C}.a*0.22*{BELL})')
        .dyn(x='0.05'),
    fire(loop('Base', 42, '0.5+0.3*{H3}', 'Flame').f('Transient.SSTheta', '{H1}*{TAU}'),
         'float3(cos({TH}),sin({TH}),0.0)*{R}*(0.35+0.5*{H2})+float3(0.0,0.0,{HT}*(0.2+0.1*{EASE}))',
         '{R}*(1.1+0.5*{H4})', '{HT}*(0.45+0.2*{H4})', 'saturate({A}*8.0)*(1.0-{A}*{A})', broad='0.6+0.4*{H2}', gate=False),
    fire(loop('Body', 38, '0.45+0.25*{H3}', 'Flame').f('Transient.SSTheta', '{H1}*{TAU}'),
         'float3(cos({TH}),sin({TH}),0.0)*{R}*(0.45+0.4*{H2})+float3(0.0,0.0,{HT}*(0.35+0.42*{H4}+0.12*{EASE}))',
         '{R}*(0.85+0.4*{H2})', '{HT}*(0.32+0.16*{H2})', 'saturate({A}*8.0)*(1.0-{A}*{A})', broad='0.35+0.5*{H2}', gate=False),
    fire(loop('Tips', 16, '0.35+0.2*{H3}', 'Flame').f('Transient.SSTheta', '{H1}*{TAU}'),
         'float3(cos({TH}),sin({TH}),0.0)*{R}*0.4*sqrt({H2})+float3(0.0,0.0,{HT}*(0.8+0.15*{H4}+0.12*{EASE}))',
         '{R}*(0.45+0.25*{H2})', '{HT}*(0.24+0.12*{H2})', 'saturate({A}*9.0)*(1.0-{A})', broad='0.2+0.4*{H2}', gate=False),
    fire_smoke(loop('Smoke', 5, '1.5', 'InkPlume'),
               'float3(({H1}-0.5)*{R}*0.8,({H2}-0.5)*{R}*0.8,{HT}*(0.85+0.9*{EASE}))+' + SWAY + '*{R}*0.3*{A}',
               '{R}*(1.1+1.6*{A})', '0.18*{BELL}', gate=False),
], warmup=0.3)

# ================================================================ futuristic: 无机态那

# Data trail from the hilt: a hologram ribbon with a magenta chromatic twin, blinking pixels, rising 0/1 bits, hex
# tiles left behind and a HUD ring turning at the grip.
system('NS_SS_Trail_Tech', ['Width', 'Life'], [
    loop('Ink', 110, TRAIL_LIFE + '*0.9', 'InkRibbon').ribbon().world()
        .width('{W}*2.0*pow(1.0-{A},0.6)')
        .tint('float4({INK},{C}.a*0.7*pow(1.0-{A},0.8))'),
    loop('Ribbon', 110, TRAIL_LIFE, 'TechRibbon').ribbon().world()
        .width('{W}*1.6*pow(1.0-{A},0.5)')
        .tint('float4({BODY}*1.1,{C}.a*pow(1.0-{A},0.9))')
        .dyn(x='0.45'),
    loop('Split', 110, TRAIL_LIFE + '*0.7', 'TechRibbon').ribbon().world().origin()
        .pos('{ORG}+float3(0.0,0.0,{W}*0.35)')
        .width('{W}*1.2*pow(1.0-{A},0.6)')
        .tint('float4({MAG}*0.9,{C}.a*0.35*pow(1.0-{A},1.2))')
        .dyn(x='0.2'),
    loop('Pixels', 30, '0.5+0.3*{H3}', 'Digit').world().origin()
        .pos('{ORG}+' + sphere('{H1}', '{H2}') + '*{W}*(0.5+1.5*{H3})*{EASE}')
        .size('float2(1.0,1.0)*{W}*(0.18+0.22*{H4})*(1.0-0.3*{A})')
        .tint('float4(lerp({BODY},{MAG},step(0.85,{H2}))*1.2,{C}.a*(1.0-{A})*step(0.35,frac({PAGE}*9.0+{H1})))')
        .dyn(w='1.0'),
    loop('Bits', 8, '0.8', 'Digit').world().origin()
        .pos('{ORG}+float3(({H1}-0.5)*{W}*1.5,({H2}-0.5)*{W}*1.5,{W}*2.0*{EASE})')
        .size('float2(0.7,1.0)*{W}*0.55*(1.0-0.3*{A})')
        .tint('float4({BODY}*1.2,{C}.a*{BELL})')
        .dyn(w='0.0'),
    loop('Hex', 5, '0.6', 'Hex').world().origin()
        .pos('{ORG}')
        .size('float2(1.0,1.0)*{W}*(1.2+1.2*{EASE})')
        .turn('{H1}*60.0')
        .tint('float4({BODY}*1.1,{C}.a*0.8*(1.0-{A}))')
        .dyn(w='2.0'),
    loop('Hilt', 1.5, '0.9', 'Reticle')
        .pos(ZERO)
        .size('float2(1.0,1.0)*{W}*3.0')
        .turn('{AGE}*120.0')
        .tint('float4({BODY}*1.1,{C}.a*0.7*{BELL})')
        .dyn(w='1.0'),
])

# Blade field (loops, attached with the blade on local Z): hex cells blinking along the blade, a scan line running from
# grip to tip, data bits and a faint cyan sheath.
system('NS_SS_Aura_Tech', ['Radius'], [
    loop('Sheath', 5, '0.7')
        .pos('float3(0.0,0.0,({H1}-0.5)*{R}*0.4)')
        .along('float3(0.0,0.0,1.0)')
        .size('float2({R}*0.1,{R}*0.7)*(0.9+0.1*{BELL})')
        .tint('float4({BODY}*0.9,{C}.a*0.35*{BELL})')
        .dyn(x='0.3'),
    loop('Cells', 14, '0.5', 'Hex')
        .pos('float3(({H2}-0.5)*{R}*0.1,({H3}-0.5)*{R}*0.1,({H1}-0.5)*{R}*0.85)')
        .size('float2(1.0,1.0)*{R}*(0.05+0.04*{H4})*{BELL}')
        .tint('float4({BODY}*1.2,{C}.a*{BELL})')
        .dyn(w='2.0'),
    loop('Scan', 2.0, '0.5', 'Laser')
        .pos('float3(0.0,0.0,{R}*(-0.45+0.9*{A}))')
        .along('float3(1.0,0.0,0.0)')
        .size('float2({R}*0.02,{R}*0.2)')
        .tint('float4({HOT}*1.3,{C}.a*{BELL})'),
    loop('Bits', 6, '0.8', 'Digit')
        .pos('float3(cos({H1}*{TAU}),sin({H1}*{TAU}),0.0)*{R}*0.12+float3(0.0,0.0,({H2}-0.5)*{R}*0.8+{A}*{R}*0.1)')
        .size('float2(0.7,1.0)*{R}*0.06')
        .tint('float4({BODY}*1.2,{C}.a*{BELL}*step(0.3,frac({PAGE}*7.0+{H3})))')
        .dyn(w='0.0'),
], warmup=0.8)

# Hologram strike along local +X: a segmented scan-line crescent with a magenta chromatic twin and a hot rim, wire
# triangles, pixels and laser sparks thrown forward and a hex frame at the hit.
system('NS_SS_Slash_Tech', ['Size'], [
    burst('Smoke', 3, '0.8', 'InkPlume')
        .pos('float3({SZ}*0.3*{H1},({H2}-0.5)*{SZ}*0.8,({H3}-0.5)*{SZ}*0.3)+float3({SZ}*0.25,0.0,{SZ}*0.1)*{EASE}')
        .size('float2(1.0,1.0)*{SZ}*(0.45+0.6*{EASE})')
        .turn('{H1}*360.0+{A}*35.0')
        .tint('float4({INK},{C}.a*0.55*(1.0-{A}))')
        .dyn(z='{A}*0.9', w='0.5'),
    burst('Stroke', 1, '0.36', 'InkArc')
        .pos('float3({SZ}*0.06,0.0,0.0)').along('float3(1.0,0.0,0.0)')
        .size('float2({SZ}*1.62,{SZ}*1.02)*(0.8+0.3*{EASE})')
        .tint('float4({INK},{C}.a*0.85*(1.0-{A}*{A}*{A}))')
        .dyn(y=SEEDED, z='saturate({A}*3.0)', w='0.8'),
    burst('Arc', 1, '0.32', 'TechArc')
        .pos('float3({SZ}*0.08,0.0,0.0)').along('float3(1.0,0.0,0.0)')
        .size('float2({SZ}*1.5,{SZ}*0.95)*(0.8+0.3*{EASE})')
        .tint('float4({BODY}*1.15,{C}.a*(1.0-{A}*{A}*{A}))')
        .dyn(y=SEEDED, z='{A}', w='0.8'),
    burst('Split', 1, '0.3', 'TechArc')
        .pos('float3({SZ}*0.02,{SZ}*0.05,0.0)').along('float3(1.0,0.0,0.0)')
        .size('float2({SZ}*1.5,{SZ}*0.95)*(0.8+0.3*{EASE})')
        .tint('float4({MAG}*0.9,{C}.a*0.4*(1.0-{A}*{A}))')
        .dyn(x='0.15', y=SEEDED, z='{A}*0.95', w='0.8'),
    burst('Edge', 1, '0.24', 'TechArc')
        .pos('float3({SZ}*0.1,0.0,0.0)').along('float3(1.0,0.0,0.0)')
        .size('float2({SZ}*1.58,{SZ}*1.0)*(0.8+0.3*{EASE})')
        .tint('float4({HOT}*1.6,{C}.a*pow(1.0-{A},1.2))')
        .dyn(x='0.1', y=SEEDED, z='{A}*1.15', w='0.06'),
    burst('Tris', 10, '0.45', 'Tri')
        .v('Transient.SSDir', 'normalize(float3(0.5+0.5*{H1},({H2}-0.5)*2.0,({H3}-0.5)*0.8))')
        .pos('{DIR}*{SZ}*(0.2+0.8*{EASE})*(0.6+0.4*{H4})')
        .turn('{H1}*360.0+{A}*({H2}-0.5)*900.0')
        .size('float2(1.0,1.0)*{SZ}*(0.08+0.08*{H3})*(1.0-0.4*{A})')
        .tint('float4({BODY}*1.2,{C}.a*(1.0-{A}))')
        .dyn(w='step(0.5,{H4})'),
    burst('Pixels', 16, '0.4', 'Digit')
        .v('Transient.SSDir', 'normalize(float3(0.4+0.6*{H1},({H2}-0.5)*2.0,({H3}-0.5)*1.2))')
        .pos('{DIR}*{SZ}*(0.2+0.9*{EASE})*(0.5+0.5*{H4})')
        .size('float2(1.0,1.0)*{SZ}*(0.03+0.04*{H3})')
        .tint('float4(lerp({BODY},{MAG},step(0.8,{H2}))*1.2,{C}.a*(1.0-{A})*step(0.3,frac({PAGE}*11.0+{H1})))')
        .dyn(w='1.0'),
    burst('Lasers', 10, '0.28', 'Laser')
        .v('Transient.SSDir', 'normalize(float3(0.6+0.4*{H1},({H2}-0.5)*2.0,({H3}-0.5)*0.8))')
        .pos('{DIR}*{SZ}*(0.15+0.85*{EASE})*(0.6+0.4*{H4})')
        .along('{DIR}')
        .size('float2(1.5+{SZ}*0.01,{SZ}*0.4*(1.0-{A}))')
        .tint('float4({HOT}*1.4,{C}.a*(1.0-{A}))'),
    burst('Frame', 1, '0.3', 'Hex')
        .pos('float3({SZ}*0.15,0.0,0.0)')
        .size('float2(1.0,1.0)*{SZ}*0.5*(0.6+0.6*{EASE})')
        .turn('{SEED}*60.0')
        .tint('float4({BODY}*1.2,{C}.a*(1.0-{A}))')
        .dyn(w='0.0'),
])

# Hologram hit: a lit hex tile, a hex-grid wave, a dashed tick ring, a thin shock ring, target brackets snapping in,
# wire triangles, blinking pixels, rising bits, radial lasers and a horizontal lens flare.
system('NS_SS_Burst_Tech', ['Radius'], [
    burst('Smoke', 4, '0.95', 'InkPlume')
        .v('Transient.SSDir', 'normalize(' + sphere('{H1}', '{H2}') + '*float3(1.0,1.0,0.6)+float3(0.0,0.0,0.35))')
        .pos('{DIR}*{R}*(0.1+0.6*{EASE})+float3(0.0,0.0,{R}*0.2*{A})')
        .size('float2(1.0,1.0)*{R}*(0.7+0.9*{EASE})')
        .turn('{H1}*360.0+{A}*30.0')
        .tint('float4({INK},{C}.a*0.55*(1.0-{A}))')
        .dyn(z='{A}*0.9', w='0.5'),
    burst('Splash', 1, '0.6', 'InkSplat')
        .pos(ZERO)
        .size('float2(1.0,1.0)*{R}*1.3*(0.55+0.45*{EASE})')
        .turn('{SEED}*360.0')
        .tint('float4({INK},{C}.a*0.85*(1.0-pow({A},2.5)))')
        .dyn(z='{A}', w='0.0'),
    burst('Panel', 1, '0.35', 'Hex')
        .pos(ZERO)
        .size('float2(1.0,1.0)*{R}*1.1*(0.5+0.6*{EASE})')
        .turn('{SEED}*60.0')
        .tint('float4({BODY}*1.2,{C}.a*pow(1.0-{A},1.5))')
        .dyn(w='2.0'),
    burst('Grid', 1, '0.55', 'Hex')
        .pos(ZERO)
        .size('float2(1.0,1.0)*{R}*2.3')
        .tint('float4({BODY}*1.1,{C}.a*0.9*(1.0-{A}))')
        .dyn(x='0.35', z='{A}*0.9', w='1.0'),
    burst('Ring', 1, '0.4', 'Reticle')
        .pos(ZERO)
        .size('float2(1.0,1.0)*{R}*2.0*(0.2+1.0*{EASE})')
        .turn('{SEED}*90.0+{A}*60.0')
        .tint('float4({BODY}*1.2,{C}.a*pow(1.0-{A},1.3))')
        .dyn(w='1.0'),
    burst('Shock', 1, '0.3', 'Ring')
        .pos(ZERO)
        .size('float2(1.0,1.0)*{R}*3.0*{EASE}')
        .tint('float4({HOT}*1.4,{C}.a*(1.0-{A}))')
        .dyn(x='0.25', w='0.05'),
    burst('Lock', 1, '0.35', 'Reticle')
        .pos(ZERO)
        .size('float2(1.0,1.0)*{R}*1.3*(1.4-0.4*{EASE})')
        .tint('float4({BODY}*1.3,{C}.a*(1.0-{A}*{A}))')
        .dyn(w='0.0'),
    burst('Tris', 12, '0.5', 'Tri')
        .v('Transient.SSDir', sphere('{H1}', '{H2}'))
        .pos('{DIR}*{R}*(0.2+1.0*{EASE})')
        .turn('{H1}*360.0+{A}*({H2}-0.5)*900.0')
        .size('float2(1.0,1.0)*{R}*(0.1+0.1*{H3})*(1.0-0.4*{A})')
        .tint('float4({BODY}*1.2,{C}.a*(1.0-{A}))')
        .dyn(w='step(0.5,{H4})'),
    burst('Pixels', 20, '0.45', 'Digit')
        .v('Transient.SSDir', sphere('{H3}', '{H1}'))
        .pos('{DIR}*{R}*(0.3+1.1*{EASE})*(0.6+0.4*{H2})')
        .size('float2(1.0,1.0)*{R}*(0.05+0.05*{H4})')
        .tint('float4(lerp({BODY},{MAG},step(0.8,{H2}))*1.2,{C}.a*(1.0-{A})*step(0.3,frac({PAGE}*11.0+{H1})))')
        .dyn(w='1.0'),
    burst('Bits', 8, '0.7', 'Digit')
        .v('Transient.SSDir', sphere('{H2}', '{H3}'))
        .pos('{DIR}*{R}*(0.3+0.6*{EASE})+float3(0.0,0.0,{R}*0.6*{A})')
        .size('float2(0.7,1.0)*{R}*0.16')
        .tint('float4({BODY}*1.2,{C}.a*{BELL})')
        .dyn(w='0.0'),
    burst('Lasers', 14, '0.3', 'Laser')
        .v('Transient.SSDir', sphere('{H1}', '{H3}'))
        .pos('{DIR}*{R}*(0.2+1.2*{EASE})')
        .along('{DIR}')
        .size('float2(1.5+{R}*0.012,{R}*0.45*(1.0-{A}))')
        .tint('float4({HOT}*1.4,{C}.a*(1.0-{A}))'),
    burst('Flare', 1, '0.22', 'Laser')
        .pos(ZERO)
        .size('float2({R}*0.06,{R}*3.0*(0.6+0.4*{A}))')
        .turn('90.0')
        .tint('float4({HOT}*1.5,{C}.a*pow(1.0-{A},1.5))')
        .dyn(x='0.1'),
])

# Hologram ground slam (local Z up): a hex-grid wave across the ground, a loading dial, a dashed tick ring and a thin
# echo, a wall of laser lines rising along the wave, hex panels popping up, bits rising and wire triangles kicked up.
system('NS_SS_Ring_Tech', ['Radius'], [
    burst('Smoke', 16, '1.0', 'InkPlume')
        .f('Transient.SSTheta', '{ID}/16.0*{TAU}+{H1}*0.3')
        .pos('float3(cos({TH}),sin({TH}),0.0)*{R}*(0.1+0.85*{EASE})*(0.85+0.3*{H2})+float3(0.0,0.0,min({R}*0.08,50.0)+min({R}*0.14,90.0)*{A}*{H3})')
        .size('float2(1.0,1.0)*min({R}*(0.2+0.28*{EASE}),240.0)*(0.7+0.6*{H3})')
        .turn('{H1}*360.0+{A}*40.0')
        .tint('float4({INK},{C}.a*0.5*(1.0-{A}))')
        .dyn(z='{A}*0.9', w='0.5'),
    burst('Grid', 1, '0.9', 'Hex').flat()
        .pos('float3(0.0,0.0,4.0)')
        .size('float2(1.0,1.0)*{R}*2.4')
        .tint('float4({BODY}*1.1,{C}.a*0.9*(1.0-{A}))')
        .dyn(x='0.35', z='{EASE}*0.95', w='1.0'),
    burst('Dial', 1, '1.0', 'Reticle').flat()
        .pos('float3(0.0,0.0,6.0)')
        .size('float2(1.0,1.0)*{R}*1.5')
        .turn('{SEED}*360.0+{A}*90.0')
        .tint('float4({BODY}*1.2,{C}.a*pow(1.0-{A},1.2)*saturate({A}*8.0+0.1))')
        .dyn(z='saturate({A}*2.0)', w='2.0'),
    burst('Wave', 1, '0.6', 'Reticle').flat()
        .pos('float3(0.0,0.0,8.0)')
        .size('float2(1.0,1.0)*{R}*2.1*(0.1+0.95*{EASE})')
        .tint('float4({HOT}*1.3,{C}.a*pow(1.0-{A},1.2))')
        .dyn(w='1.0'),
    burst('Echo', 1, '0.75', 'Ring').flat().after(0.08)
        .pos('float3(0.0,0.0,5.0)')
        .size('float2(1.0,1.0)*{R}*2.0*(0.05+0.85*(1.0-(1.0-{A})*(1.0-{A})))')
        .tint('float4({BODY}*1.2,{C}.a*0.7*(1.0-{A})*(1.0-{A}))')
        .dyn(x='0.3', w='0.08'),
    burst('Wall', 40, '0.55', 'Laser')
        .f('Transient.SSTheta', '{ID}/40.0*{TAU}')
        .f('Transient.SSE', 'min({R}*0.42,230.0)*(0.5+0.5*{H1})*(1.0-0.5*{A})')
        .pos('float3(cos({TH}),sin({TH}),0.0)*{R}*(0.12+0.9*{EASE})+float3(0.0,0.0,{E}*0.5)')
        .along('float3(0.0,0.0,1.0)')
        .size('float2(min({R}*0.02,10.0)+2.0,{E})')
        .tint('float4({BODY}*1.2,{C}.a*0.85*pow(1.0-{A},1.3)*saturate({A}*8.0+0.1))')
        .dyn(w='1.0'),
    burst('Panels', 16, '0.8', 'Hex')
        .f('Transient.SSTheta', '{ID}/16.0*{TAU}+{H1}*0.2')
        .f('Transient.SSE', 'min({R}*0.12,60.0)*(0.7+0.6*{H2})')
        .pos('float3(cos({TH}),sin({TH}),0.0)*{R}*(0.3+0.6*{EASE})+float3(0.0,0.0,{E}*(0.5+1.5*{EASE}))')
        .size('float2(1.0,1.0)*{E}*saturate({A}*8.0)')
        .tint('float4({BODY}*1.2,{C}.a*(1.0-{A})*step(0.25,frac({PAGE}*8.0+{H3})))')
        .dyn(w='2.0'),
    burst('Bits', 24, '0.9', 'Digit')
        .f('Transient.SSTheta', '{H1}*{TAU}')
        .pos('float3(cos({TH}),sin({TH}),0.0)*{R}*(0.2+0.7*{H2})+float3(0.0,0.0,min({R}*0.6,300.0)*{EASE}*(0.4+0.6*{H3}))')
        .size('float2(0.7,1.0)*min({R}*0.08,40.0)')
        .tint('float4({BODY}*1.2,{C}.a*{BELL})')
        .dyn(w='0.0'),
    burst('Tris', 14, '0.7', 'Tri')
        .f('Transient.SSTheta', '{H1}*{TAU}')
        .v('Transient.SSDir', 'float3(cos({TH}),sin({TH}),0.0)')
        .f('Transient.SSE', 'min({R}*0.5,260.0)*(0.5+0.5*{H2})')
        .pos('{DIR}*{R}*(0.15+0.6*{EASE})+float3(0.0,0.0,{E}*({A}-{A}*{A})*4.0)')
        .turn('{H1}*360.0+{A}*({H2}-0.5)*900.0')
        .size('float2(1.0,1.0)*min({R}*0.07,40.0)*(0.6+0.8*{H3})')
        .tint('float4({BODY}*1.2,{C}.a*(1.0-{A}))')
        .dyn(w='step(0.5,{H4})'),
])

# Hologram sword-qi (local +X is the flight direction): a scan-line crescent with a magenta twin and a hot rim, a data
# ribbon and pixels and bits left in the world.
system('NS_SS_Qi_Tech', ['Length', 'Width'], [
    loop('Trail', 60, '0.25', 'TechRibbon').ribbon().world()
        .width('{W}*0.9*(1.0-{A})')
        .tint('float4({BODY}*1.1,{C}.a*pow(1.0-{A},1.2))')
        .dyn(x='0.45'),
    loop('Body', 7, '0.42', 'TechArc')
        .pos('float3({L}*0.02,0.0,0.0)').along('float3(1.0,0.0,0.0)')
        .size('float2({L}*1.05,{L}*0.62)')
        .tint('float4({BODY}*1.15,{C}.a*0.7*{BELL})')
        .dyn(y=QI_SEED, z='1.0', w='-0.75'),
    loop('Split', 7, '0.42', 'TechArc')
        .pos('float3(0.0,{L}*0.04,0.0)').along('float3(1.0,0.0,0.0)')
        .size('float2({L}*1.05,{L}*0.62)')
        .tint('float4({MAG}*0.9,{C}.a*0.3*{BELL})')
        .dyn(x='0.15', y=QI_SEED, z='1.0', w='-0.75'),
    loop('Edge', 14, '0.2', 'TechArc')
        .pos('float3({L}*0.05,0.0,0.0)').along('float3(1.0,0.0,0.0)')
        .size('float2({L}*1.1,{L}*0.66)')
        .tint('float4({HOT}*1.5,{C}.a*0.5*{BELL})')
        .dyn(x='0.1', y=QI_SEED, z='1.0', w='-0.06'),
    loop('Pixels', 30, '0.4', 'Digit').world().origin()
        .pos('{ORG}+' + sphere('{H1}', '{H2}') + '*{L}*0.12*{EASE}')
        .size('float2(1.0,1.0)*{L}*(0.025+0.03*{H3})')
        .tint('float4(lerp({BODY},{MAG},step(0.85,{H2}))*1.2,{C}.a*(1.0-{A})*step(0.3,frac({PAGE}*10.0+{H1})))')
        .dyn(w='1.0'),
    loop('Bits', 6, '0.6', 'Digit').world().origin()
        .pos('{ORG}+float3(0.0,0.0,{L}*0.25*{EASE})+' + sphere('{H3}', '{H1}') + '*{L}*0.1')
        .size('float2(0.7,1.0)*{L}*0.08')
        .tint('float4({BODY}*1.2,{C}.a*{BELL})')
        .dyn(w='0.0'),
])

# Hologram break-up: a horizontal flare, a hex frame, wire triangles spinning up and falling, pixels, bits and glitch
# bars.
system('NS_SS_Shatter_Tech', ['Size'], [
    burst('Smoke', 3, '0.85', 'InkPlume')
        .v('Transient.SSDir', sphere('{H1}', '{H2}'))
        .pos('{DIR}*{SZ}*0.4*{EASE}+float3(0.0,0.0,{SZ}*0.2*{A})')
        .size('float2(1.0,1.0)*{SZ}*(0.45+0.55*{EASE})')
        .turn('{H3}*360.0')
        .tint('float4({INK},{C}.a*0.5*(1.0-{A}))')
        .dyn(z='{A}*0.85', w='0.5'),
    burst('Flare', 1, '0.2', 'Laser')
        .pos(ZERO)
        .size('float2({SZ}*0.05,{SZ}*2.2*(0.6+0.4*{A}))')
        .turn('90.0')
        .tint('float4({HOT}*1.5,{C}.a*pow(1.0-{A},1.5))')
        .dyn(x='0.1'),
    burst('Frame', 1, '0.35', 'Hex')
        .pos(ZERO)
        .size('float2(1.0,1.0)*{SZ}*(0.5+0.7*{EASE})')
        .turn('{SEED}*60.0+{A}*40.0')
        .tint('float4({BODY}*1.2,{C}.a*(1.0-{A}))')
        .dyn(w='0.0'),
    burst('Tris', 24, '0.75', 'Tri')
        .v('Transient.SSDir', SHARD_DIR)
        .f('Transient.SSE', '{SZ}*(0.7+0.8*{H3})')
        .pos('{DIR}*{E}*{A}*1.6+float3(0.0,0.0,-{SZ}*2.2*{A}*{A})')
        .turn('{H1}*360.0+{A}*({H2}-0.5)*900.0')
        .size('float2(1.0,1.0)*{SZ}*(0.08+0.1*{H3})*(1.0-0.4*{A})')
        .tint('float4({BODY}*1.2,{C}.a*pow(1.0-{A},1.2))')
        .dyn(w='step(0.5,{H4})'),
    burst('Pixels', 20, '0.6', 'Digit')
        .v('Transient.SSDir', sphere('{H2}', '{H1}'))
        .pos('{DIR}*{SZ}*(0.2+0.6*{EASE})-float3(0.0,0.0,{SZ}*0.3*{A}*{A})')
        .size('float2(1.0,1.0)*{SZ}*(0.04+0.04*{H3})')
        .tint('float4(lerp({BODY},{MAG},step(0.8,{H3}))*1.2,{C}.a*(1.0-{A})*step(0.3,frac({PAGE}*10.0+{H1})))')
        .dyn(w='1.0'),
    burst('Bits', 6, '0.7', 'Digit')
        .v('Transient.SSDir', sphere('{H3}', '{H2}'))
        .pos('{DIR}*{SZ}*0.4*{EASE}+float3(0.0,0.0,{SZ}*0.4*{A})')
        .size('float2(0.7,1.0)*{SZ}*0.14')
        .tint('float4({BODY}*1.2,{C}.a*{BELL})')
        .dyn(w='0.0'),
    burst('Glitch', 4, '0.22', 'Laser')
        .pos('float3(0.0,0.0,({H1}-0.5)*{SZ}*0.8)')
        .size('float2({SZ}*0.04,{SZ}*(0.8+0.8*{H2}))')
        .turn('90.0')
        .tint('float4(lerp({BODY},{MAG},step(0.5,{H3}))*1.2,{C}.a*step(0.4,frac({PAGE}*15.0+{H1})))')
        .dyn(x='0.2'),
])

# Sci-fi seal of the mechanical form: a rotating seal and a counter-rotating inner one, a hex frame, target brackets,
# a scan ring, flying 0/1 bits and pixels, dashed circuit traces along the axes and a horizontal flare.
AXIS = ('float3(1.0-step(0.5,floor({K}/2.0)),step(0.5,floor({K}/2.0))-step(1.5,floor({K}/2.0)),step(1.5,floor({K}/2.0)))'
        '*(1.0-2.0*fmod({K},2.0))')
system('NS_SS_Glyph', ['Radius'], [
    burst('Frame', 1, '0.5', 'Glyph')
        .pos(ZERO)
        .size('float2(1.0,1.0)*{R}*2.0*(0.4+0.75*{EASE})')
        .turn('45.0+{A}*90.0')
        .tint('float4({BODY}*1.15,{C}.a*{FADE})')
        .dyn(x='0.5', y='{SEED}', w='0.0'),
    burst('Inner', 1, '0.4', 'Glyph')
        .pos(ZERO)
        .size('float2(1.0,1.0)*{R}*2.0*(1.2-0.6*{A})')
        .turn('-{A}*150.0')
        .tint('float4({BODY}*0.95,{C}.a*0.7*(1.0-{A}))')
        .dyn(x='0.4', y='1.0-{SEED}', w='0.0'),
    burst('Hex', 1, '0.45', 'Hex')
        .pos(ZERO)
        .size('float2(1.0,1.0)*{R}*2.4*(0.5+0.6*{EASE})')
        .turn('{SEED}*60.0-{A}*30.0')
        .tint('float4({BODY}*1.1,{C}.a*(1.0-{A}))')
        .dyn(w='0.0'),
    burst('Lock', 1, '0.35', 'Reticle')
        .pos(ZERO)
        .size('float2(1.0,1.0)*{R}*1.4*(1.4-0.4*{EASE})')
        .tint('float4({HOT}*1.2,{C}.a*(1.0-{A}*{A}))')
        .dyn(w='0.0'),
    burst('Scan', 1, '0.3', 'Ring')
        .pos(ZERO)
        .size('float2(1.0,1.0)*{R}*2.6*{EASE}')
        .tint('float4({HOT}*1.5,{C}.a*(1.0-{A}))'),
    burst('Bits', 14, '0.5', 'Digit')
        .v('Transient.SSDir', sphere('{H1}', '{H2}'))
        .pos('{DIR}*{R}*(0.3+1.2*{EASE})')
        .size('float2(0.7,1.0)*{R}*0.2*(1.0-0.4*{A})')
        .tint('float4({BODY}*1.2,{C}.a*(1.0-{A}))')
        .dyn(w='0.0'),
    burst('Pixels', 10, '0.45', 'Digit')
        .v('Transient.SSDir', sphere('{H3}', '{H1}'))
        .pos('{DIR}*{R}*(0.4+1.0*{EASE})')
        .size('float2(1.0,1.0)*{R}*0.1*(1.0-0.4*{A})')
        .tint('float4(lerp({BODY},{MAG},step(0.8,{H2}))*1.2,{C}.a*(1.0-{A})*step(0.3,frac({PAGE}*11.0+{H1})))')
        .dyn(w='1.0'),
    burst('Traces', 12, '0.35', 'Laser')
        .f('Transient.SSK', 'fmod({ID},6.0)')
        .v('Transient.SSDir', AXIS)
        .pos('{DIR}*{R}*(0.4+0.8*{EASE})+' + sphere('{H1}', '{H2}') + '*{R}*0.15')
        .along('{DIR}')
        .size('float2(1.5,{R}*0.6*(1.0-{A}))')
        .tint('float4({HOT}*1.5,{C}.a*(1.0-{A}))')
        .dyn(w='1.0'),
    burst('Flare', 1, '0.18', 'Laser')
        .pos(ZERO)
        .size('float2({R}*0.05,{R}*2.6)')
        .turn('90.0')
        .tint('float4({HOT}*1.5,{C}.a*pow(1.0-{A},2.0))')
        .dyn(x='0.1'),
])

# Whirling spin (loops; the component keeps a level world rotation): a hex-grid disc and a loading dial, scan-line
# crescents chasing round with magenta twins, laser blade arms and pixels flung outward.
SPIN = '({AGE}-{PAGE})'
system('NS_SS_Spin', ['Radius'], [
    loop('Grid', 1.5, '0.9', 'Hex').flat()
        .pos(ZERO)
        .size('float2(1.0,1.0)*{R}*2.4')
        .tint('float4({BODY}*1.0,{C}.a*0.5*{BELL})')
        .dyn(x='0.3', z='{A}', w='1.0'),
    loop('Dial', 2.0, '0.7', 'Reticle').flat()
        .pos(ZERO)
        .size('float2(1.0,1.0)*{R}*2.0')
        .turn('-{AGE}*720.0')
        .tint('float4({BODY}*1.1,{C}.a*0.6*{BELL})')
        .dyn(z='frac({AGE}*1.5)', w='2.0'),
    loop('Arcs', 24, '0.3', 'TechArc').flat()
        .f('Transient.SSTheta', SPIN + '*14.0+fmod({ID},2.0)*{PI}')
        .pos('float3(cos({TH}),sin({TH}),0.0)*{R}*0.55')
        .along('float3(-sin({TH}),cos({TH}),0.0)')
        .size('float2({R}*1.0,{R}*0.55)*(1.0-0.3*{A})')
        .tint('float4({BODY}*1.15,{C}.a*0.8*pow(1.0-{A},1.2))')
        .dyn(y=QI_SEED, z='1.0', w='-0.5'),
    loop('Split', 12, '0.3', 'TechArc').flat()
        .f('Transient.SSTheta', SPIN + '*14.0+fmod({ID},2.0)*{PI}-0.25')
        .pos('float3(cos({TH}),sin({TH}),0.0)*{R}*0.58')
        .along('float3(-sin({TH}),cos({TH}),0.0)')
        .size('float2({R}*1.0,{R}*0.55)*(1.0-0.3*{A})')
        .tint('float4({MAG}*0.9,{C}.a*0.3*pow(1.0-{A},1.2))')
        .dyn(x='0.15', y=QI_SEED, z='1.0', w='-0.5'),
    loop('Blades', 90, '0.25', 'Laser')
        .f('Transient.SSTheta', SPIN + '*15.0+fmod({ID},3.0)*2.0943951')
        .f('Transient.SSE', '{R}*(0.55+0.45*{H1})')
        .pos('float3(cos({TH})*{E},sin({TH})*{E},({H2}-0.5)*{R}*0.12)')
        .along('float3(-sin({TH}),cos({TH}),0.0)')
        .size('float2({R}*0.02*(1.0-{A})+1.0,{R}*0.38*(1.0-0.5*{A}))')
        .tint('float4({HOT}*1.4,{C}.a*0.9*pow(1.0-{A},1.2))'),
    loop('Fling', 30, '0.4', 'Digit')
        .f('Transient.SSTheta', SPIN + '*14.0+{H1}*{TAU}')
        .v('Transient.SSDir', 'normalize(float3(-sin({TH}),cos({TH}),0.0)+float3(cos({TH}),sin({TH}),0.0)*0.6)')
        .pos('float3(cos({TH}),sin({TH}),0.0)*{R}*0.9+{DIR}*{R}*0.9*{A}')
        .size('float2(1.0,1.0)*{R}*(0.04+0.04*{H2})*(1.0-0.3*{A})')
        .tint('float4(lerp({BODY},{MAG},step(0.85,{H3}))*1.2,{C}.a*(1.0-{A}))')
        .dyn(w='1.0'),
], warmup=0.4)

# ---------------------------------------------------------------- tribulation (天劫)


def hashed(x, salt):
    """Stable per-value hash in [0, 1)."""
    return 'frac(sin((%s)*12.9898+%s)*43758.5453)' % (x, salt)


# Tribulation storm (loops). The storm actor keeps the component on the camera's sight line to the anchor at cloud
# height, yawed so local +X looks away from the camera: a vortex of storm billows turns round a clear eye, so the anchor
# stays in view, and is thickest on the far side that fills the top of the screen. User.Radius is the world half-width
# the camera shows at that depth, User.Eye the eye radius in Radius, User.Density the gathering (0..1), User.Flash the
# strike flash (0..1, pulsed by the actor) and User.Storm the tribulation's pressure (0..1: faster turning, more light).
STORM_SPIN = '({H1}*{TAU}+{PAGE}*(0.12+0.3*{ST})*sqrt({EYE}*{R}/max({RHO},1.0)))'
# Far side 1, sides about 0.6, near side 0.15: the clouds frame the top and flanks and leave the bottom of the screen clear.
STORM_WEIGHT = '(0.15+0.85*saturate(0.55+0.55*cos({TH})))'
# Lightning inside the cloud lights the eye wall most; a faint flicker remains between strikes under pressure.
STORM_FLASH = ('({FL}*(0.15+0.85*saturate(1.0-({RHO}/{R}-{EYE})/0.8))'
               '+{ST}*0.1*(0.5+0.5*sin({PAGE}*3.0+{H3}*20.0)))')


def billows(name, rate, life, rho, z, size, tint, alpha, flash='1.0'):
    return (loop(name, rate, life, 'Storm')
            .f('Transient.SSRho', rho)
            .f('Transient.SSTheta', STORM_SPIN)
            .f('Transient.SSWeight', STORM_WEIGHT)
            .pos('float3(cos({TH})*{RHO},sin({TH})*{RHO},' + z + ')')
            .size('float2(1.0,1.0)*' + size)
            .tint('float4(' + tint + ',' + alpha + ')')
            .dyn(y='{H1}', z=STORM_FLASH + '*' + flash, w='0.5'))


CRAWL_CHAIN = 'floor({ID}/6.0)'
CRAWL_JOINT = ('(float3(cos({TH}),sin({TH}),0.0)*{E}+float3(-sin({TH}),cos({TH}),0.0)*{X2}*$k*{R}*0.07'
               '+float3(cos({TH}),sin({TH}),0.0)*(' + hashed('{X1}*7.0+$k', '3.3') + '-0.5)*{R}*0.07'
               '+float3(0.0,0.0,(' + hashed('{X1}*7.0+$k', '4.4') + '-0.5)*{R}*0.04-{R}*0.03))')

system('NS_SS_TribCloud', ['Radius', 'Eye', 'Density', 'Flash', 'Storm'], [
    # Puffs grow with their distance from the eye, so the eye wall stays a crisp edge instead of fogging the anchor.
    billows('Belly', 7, '9.0', '{R}*({EYE}+0.3+(1.75-{EYE})*pow({H2},1.1))',
            '({H3}-0.5)*{R}*0.06-{R}*0.1', '{R}*(0.55+0.35*{H4})',
            'float3(0.016,0.018,0.027)', 'min(1.0,pow({BELL},0.5)*{WT}*{DEN})', '0.7'),
    billows('Billows', 18, '7.0', '{R}*({EYE}+0.12+(1.7-{EYE})*pow({H2},1.25))',
            '({H3}-0.5)*{R}*0.1-{R}*0.12*saturate(1.0-({RHO}/{R}-{EYE})/0.45)+{R}*0.03*sin({PAGE}*0.6+{H1}*6.0)',
            '{R}*(0.28+0.5*saturate(({RHO}/{R}-{EYE})/0.9))*(0.8+0.4*{H4})',
            'float3(0.028,0.032,0.046)', 'min(1.0,pow({BELL},0.5)*{WT}*{DEN})'),
    billows('Wall', 10, '5.0', '{R}*({EYE}+0.1+0.06*{H2})',
            '-{R}*0.1+({H3}-0.5)*{R}*0.06', '{R}*(0.16+0.1*{H4})',
            'float3(0.048,0.054,0.074)', 'pow({BELL},0.6)*{WT}*{DEN}*0.95', '1.4'),
    # Faint light deep in the cloud mass between strikes, never over the eye.
    loop('Sheet', 6, '0.4')
        .f('Transient.SSRho', '{R}*({EYE}+0.35+0.8*{H2})')
        .f('Transient.SSTheta', '{H1}*{TAU}')
        .f('Transient.SSWeight', STORM_WEIGHT)
        .pos('float3(cos({TH})*{RHO},sin({TH})*{RHO},-{R}*0.02)')
        .size('float2(1.0,1.0)*{R}*(0.25+0.2*{H3})')
        .tint('float4(float3(0.5,0.6,1.0)*1.6,{DEN}*{WT}*(0.04+0.22*{ST})*step(0.5,frac({PAGE}*17.0+{H4}))*{BELL})')
        .dyn(x='0.0'),
    loop('Crawl', 36, '0.28', 'Laser')
        .f('Transient.SSX1', CRAWL_CHAIN)
        .f('Transient.SSK', 'fmod({ID},6.0)')
        .f('Transient.SSTheta', hashed('{X1}', '1.1') + '*{TAU}')
        .f('Transient.SSE', '{R}*({EYE}+0.05+0.7*' + hashed('{X1}', '5.5') + ')')
        .f('Transient.SSX2', '(' + hashed('{X1}', '2.2') + '<0.5?1.0:-1.0)')
        .f('Transient.SSWeight', STORM_WEIGHT)
        .v('Transient.SSPa', sub(CRAWL_JOINT, k='{K}'))
        .v('Transient.SSPb', sub(CRAWL_JOINT, k='({K}+1.0)'))
        .pos('({QA}+{QB})*0.5')
        .along('normalize({QB}-{QA}+float3(0.0,0.0,0.001))')
        .size('float2({R}*0.004+1.0,length({QB}-{QA})*1.2)')
        .tint('float4(float3(0.7,0.8,1.0)*3.5,{DEN}*{WT}*{WT}*(0.15+0.6*{ST})*(1.0-{A}))')
        .dyn(x='0.3'),
], warmup=3.0)

# Tribulation bolt (one strike, spawned in the cloud; User.Target is the strike point on the anchor relative to it). A
# stepped leader races down in TRIB_LEADER seconds along a tortuous channel: a slow wander, kinks at a medium scale and fine
# kinks at every joint, the ends pinned, so it reads as fractal rather than a cartoon zig-zag. Then the return strokes
# flash it white-hot two or three times over a dim afterglow, inside a violet-blue glow and a wide faint halo of lit air.
# Forks split off the upper channel, thinner and dimmer, and die after the first stroke. The cloud flashes where it was
# born. What the strike does at the anchor (flash, shock ring, ground and body arcs, sparks, steam, scorch) is
# NS_SS_TribHit, which rides on the anchor's feet so it stays under the anchor.
TRIB_SEGMENTS = 72
TRIB_FORKS, TRIB_FORK_SEGMENTS = 9, 12
TRIB_LEADER = '0.07'
TRIB_FADE = 'pow(1.0-{A},1.4)'
# Leader at 35% while it descends, then return strokes at 0.07, 0.16 and 0.28 s, each decaying fast.
TRIB_STROKES = ('(0.35*step({AGE},' + TRIB_LEADER + ')+step(' + TRIB_LEADER + ',{AGE})*max(max(exp(-max({AGE}-' + TRIB_LEADER + ',0.0)*24.0),'
                '0.8*step(0.16,{AGE})*exp(-max({AGE}-0.16,0.0)*22.0)),max(0.55*step(0.28,{AGE})*exp(-max({AGE}-0.28,0.0)*18.0),0.12)))')
# A segment shows once the leader has passed it.
TRIB_REACHED = 'step({K}/%.1f,{AGE}/' + TRIB_LEADER + ')'
TRIB_GLOW = 'float3(0.42,0.5,1.0)'
TRIB_HOT = 'float3(0.85,0.9,1.0)'


def vnoise(x, salt):
    """Smooth 1-D value noise in [0, 1): hashed lattice values eased between (spelt out for the vector VM)."""
    f = 'frac(%s)' % x
    return 'lerp(%s,%s,%s*%s*(3.0-2.0*%s))' % (hashed('floor(%s)' % x, salt), hashed('floor(%s)+1.0' % x, salt), f, f, f)


def trib_joint(k, n):
    """Joint k of an n-joint channel toward {T}: wander, medium and fine kinks on both perpendiculars, ends pinned."""
    t = '((%s)/%s)' % (k, n)
    a1 = '(%s-0.5)' % vnoise(t + '*4.5+{SEED}*7.0', '0.31')
    b1 = '(%s-0.5)' % vnoise(t + '*4.5+{SEED}*13.0', '1.37')
    a2 = '(%s-0.5)' % vnoise(t + '*17.0+{SEED}*3.0', '2.11')
    b2 = '(%s-0.5)' % vnoise(t + '*17.0+{SEED}*5.0', '3.73')
    a3 = '(%s-0.5)' % hashed('(%s)+{SEED}*91.0' % k, '4.19')
    b3 = '(%s-0.5)' % hashed('(%s)*1.37+{SEED}*57.0' % k, '5.51')
    off = ('({P1}*(%s*0.22+%s*0.08+%s*min(0.03,30.0/{LEN}))+{P2}*(%s*0.22+%s*0.08+%s*min(0.03,30.0/{LEN})))*{LEN}*0.5'
           % (a1, a2, a3, b1, b2, b3))
    return sub('({T}*$t+' + off + '*pow(saturate(sin($t*{PI})),0.55))', t=t)


def trib_fork_joint(j):
    """Joint j of a fork leaving the channel at {V1} toward {V2}, {E} long, kinked at every joint."""
    t = '((%s)/%.1f)' % (j, TRIB_FORK_SEGMENTS)
    return sub('({V1}+{V2}*{E}*$t+({P1}*(' + hashed('{X1}*13.0+($j)', '{SEED}*5.1+0.9') + '-0.5)+{P2}*(' + hashed('{X1}*13.0+($j)', '{SEED}*7.7+2.1')
               + '-0.5))*min({E}*0.16,34.0)*saturate(sin($t*{PI})*1.5)+{P1}*sin($t*{PI})*{E}*0.18*(' + hashed('{X1}', '{SEED}*2.2+0.7') + '-0.5))', t=t, j=j)


def channel(e):
    return (e.f('Transient.SSLen', 'max(length({T}),1.0)')
            .v('Transient.SSDir', '{T}/{LEN}')
            .v('Transient.SSP1', PERP1)
            .v('Transient.SSP2', PERP2))


def trib_trunk(name, stride, life, mat, width, colour, occlusion, w='0.0'):
    """The main channel as sprites from joint {K}·stride to the next; stride > 1 draws a coarser copy (the halo)."""
    n = '%.1f' % TRIB_SEGMENTS
    count = TRIB_SEGMENTS // stride
    s = '%.1f' % stride
    return (channel(burst(name, count, life, mat))
            .f('Transient.SSK', 'fmod({ID},%.1f)*%s' % (count, s))
            .v('Transient.SSPa', trib_joint('{K}', n))
            .v('Transient.SSPb', trib_joint('{K}+' + s, n))
            .pos('({QA}+{QB})*0.5')
            .along('normalize({QB}-{QA}+float3(0.0,0.0,0.001))')
            .size('float2(' + width + ',length({QB}-{QA})*1.15)*' + TRIB_REACHED % TRIB_SEGMENTS)
            .tint(colour)
            .dyn(x=occlusion, z='1.0', w=w))


def trib_branch(name, life, mat, width, colour, occlusion):
    n = '%.1f' % TRIB_SEGMENTS
    m = '%.1f' % TRIB_FORK_SEGMENTS
    return (channel(burst(name, TRIB_FORKS * TRIB_FORK_SEGMENTS, life, mat))
            .f('Transient.SSX1', 'floor({ID}/' + m + ')')
            .f('Transient.SSK', 'fmod({ID},' + m + ')')
            .f('Transient.SSX2', 'floor(' + n + '*(0.08+0.62*' + hashed('{X1}', '{SEED}*3.3+0.4') + '))')
            .f('Transient.SSTheta', hashed('{X1}', '{SEED}*9.1+1.9') + '*{TAU}')
            .v('Transient.SSV1', trib_joint('{X2}', n))
            .v('Transient.SSV2', 'normalize({DIR}*1.1+{P1}*cos({TH})+{P2}*sin({TH}))')
            .f('Transient.SSE', '{LEN}*(0.06+0.16*' + hashed('{X1}', '{SEED}*4.7+2.6') + ')')
            .v('Transient.SSPa', trib_fork_joint('{K}'))
            .v('Transient.SSPb', trib_fork_joint('{K}+1.0'))
            .pos('({QA}+{QB})*0.5')
            .along('normalize({QB}-{QA}+float3(0.0,0.0,0.001))')
            # A fork grows with the leader past its root, and is gone soon after the first return stroke.
            .size('float2(' + width + ',length({QB}-{QA})*1.15)*step(({X2}+{K}*0.5)/' + n + ',{AGE}/' + TRIB_LEADER + ')')
            .tint(colour)
            .dyn(x=occlusion, z='1.0'))


FORK_FADE = 'exp(-max({AGE}-' + TRIB_LEADER + ',0.0)*14.0)'
system('NS_SS_TribBolt', ['Width', 'Target'], [
    trib_trunk('Halo', 3, '0.5', 'Streak', '{W}*11.0', 'float4(float3(0.5,0.56,1.0)*1.4,{C}.a*0.3*' + TRIB_FADE + '*' + TRIB_STROKES + ')', '0.0'),
    trib_trunk('Glow', 1, '0.5', 'Streak', '{W}*2.8*(0.8+0.4*{K}/72.0)', 'float4(' + TRIB_GLOW + '*3.2,{C}.a*0.75*' + TRIB_FADE + '*' + TRIB_STROKES + ')', '0.1'),
    trib_trunk('Core', 1, '0.46', 'Laser', '({W}*0.2+1.0)*(0.75+0.5*{K}/72.0)', 'float4(' + TRIB_HOT + '*8.0,{C}.a*' + TRIB_FADE + '*' + TRIB_STROKES + ')', '0.35'),
    trib_branch('BranchGlow', '0.4', 'Streak', '{W}*1.3*(1.0-{K}/12.0*0.7)', 'float4(' + TRIB_GLOW + '*2.2,{C}.a*0.45*' + FORK_FADE + ')', '0.05'),
    trib_branch('Branch', '0.36', 'Laser', '({W}*0.1+0.5)*(1.0-{K}/12.0*0.7)', 'float4(' + TRIB_HOT + '*5.0,{C}.a*0.8*' + FORK_FADE + ')', '0.3'),
    burst('Sky', 1, '0.4')
        .pos(ZERO)
        .size('float2(1.0,1.0)*{W}*30.0')
        .tint('float4(float3(0.55,0.62,1.0)*2.0,{C}.a*' + TRIB_FADE + '*' + TRIB_STROKES + ')')
        .dyn(x='0.0'),
])

# Where a tribulation bolt meets the anchor (spawned on the anchor's feet and riding on them, level; User.Width as the
# bolt's, User.Height how tall the anchor stands), all of it TRIB_LEADER in, when the leader reaches the ground: a
# blinding flash on the body with the first return stroke, a dome of
# light on the ground, a shock ring, electric arcs crawling flat over the ground and up the body, sparks that bounce on
# the floor, steam rising off a scorch whose centre glows for a moment.
TRIB_FLICKER = 'floor({AGE}*30.0)'
GROUND_ARC = ('(float3(cos({TH}),sin({TH}),0.0)*{E}*$t'
              '+float3(-sin({TH}),cos({TH}),0.0)*(' + hashed('{X1}*7.0+($k)+' + TRIB_FLICKER + '*0.37', '{SEED}*3.1+0.7') + '-0.5)*{E}*0.4*saturate(sin($t*{PI})*1.6)'
              '+float3(0.0,0.0,2.0+3.0*' + hashed('{X1}*5.0+($k)', '{SEED}*1.9+2.2') + '))')
BODY_ARC = ('(float3(cos({TH}+($k)*0.5*{X2}),sin({TH}+($k)*0.5*{X2}),0.0)*{W}*(2.3+0.7*' + hashed('{X1}*3.0+($k)+' + TRIB_FLICKER + '*0.61', '{SEED}*4.3+1.1') + ')'
            '+float3(0.0,0.0,{E}+({HT}*0.07)*($k)+({HT}*0.05)*(' + hashed('{X1}*9.0+($k)+' + TRIB_FLICKER + '*0.29', '{SEED}*2.7+3.4') + '-0.5)))')
SPARK_V = 'normalize(' + sphere('{H1}', '{H2}') + '*float3(1.0,1.0,0.6)+float3(0.0,0.0,0.7))*(220.0+480.0*{H3})'
SPARK_Z = '({V1}.z*{PAGE}-490.0*{PAGE}*{PAGE})'
system('NS_SS_TribHit', ['Width', 'Height'], [
    burst('Dome', 1, '0.5').after(float(TRIB_LEADER)).flat()
        .pos('float3(0.0,0.0,2.0)')
        .size('float2(1.0,1.0)*{W}*(18.0+8.0*{EASE})')
        .tint('float4(float3(0.55,0.62,1.0)*2.2,{C}.a*pow(1.0-{A},1.8))')
        .dyn(x='0.05'),
    burst('Shock', 1, '0.55', 'Ring').after(float(TRIB_LEADER)).flat()
        .pos('float3(0.0,0.0,3.0)')
        .size('float2(1.0,1.0)*{W}*(4.0+26.0*{EASE})')
        .tint('float4(float3(0.62,0.72,1.0)*2.4,{C}.a*(1.0-{A})*(1.0-{A}))')
        .dyn(x='0.25', w='0.25'),
    burst('Scorch', 1, '3.0', 'InkSplat').after(float(TRIB_LEADER)).flat()
        .pos('float3(0.0,0.0,1.0)')
        .size('float2(1.0,1.0)*{W}*12.0')
        .turn('{SEED}*360.0')
        .tint('float4(0.012,0.012,0.016,{C}.a*0.7*(1.0-{A}*{A}*{A}))')
        .dyn(x='0.85', z='1.0', w='0.0'),
    burst('Molten', 1, '1.3').after(float(TRIB_LEADER)).flat()
        .pos('float3(0.0,0.0,1.5)')
        .size('float2(1.0,1.0)*{W}*(4.5-1.5*{A})')
        .tint('float4(float3(1.0,0.42,0.12)*2.2,{C}.a*pow(1.0-{A},2.2))')
        .dyn(x='0.1'),
    burst('Steam', 6, '1.4+0.4*{H3}', 'InkPlume').after(float(TRIB_LEADER))
        .pos('float3(({H1}-0.5)*{W}*7.0,({H2}-0.5)*{W}*7.0,{W}*(1.0+9.0*{EASE}))')
        .size('float2(1.0,1.0)*{W}*(4.0+7.0*{A})')
        .turn('{H1}*360.0+{A}*50.0')
        .tint('float4(float3(0.5,0.54,0.62),{C}.a*0.26*{BELL})')
        .dyn(x='0.5', z='0.3+{A}*0.6', w='0.6'),
    burst('Flash', 1, '0.3').after(float(TRIB_LEADER))
        .pos('float3(0.0,0.0,{HT}*0.5)')
        .size('float2(1.0,1.0)*{W}*20.0*(0.7+0.3*{A})')
        .tint('float4(' + TRIB_HOT + '*2.6,{C}.a*pow(1.0-{A},2.0))')
        .dyn(x='0.0'),
    # Arcs: chains of six joints that re-kink every frame, flat over the ground out to 0.9-1.9 m and round the body.
    burst('GroundArcs', 8 * 6, '0.5', 'Laser').after(float(TRIB_LEADER))
        .f('Transient.SSX1', 'floor({ID}/6.0)')
        .f('Transient.SSK', 'fmod({ID},6.0)')
        .f('Transient.SSTheta', '({X1}+' + hashed('{X1}', '{SEED}*6.3+0.2') + '*0.8)/8.0*{TAU}')
        .f('Transient.SSE', '{W}*(7.0+7.0*' + hashed('{X1}', '{SEED}*2.9+1.3') + ')')
        .v('Transient.SSPa', sub(GROUND_ARC, t='({K}/6.0)', k='{K}'))
        .v('Transient.SSPb', sub(GROUND_ARC, t='(({K}+1.0)/6.0)', k='({K}+1.0)'))
        .pos('({QA}+{QB})*0.5')
        .along('normalize({QB}-{QA}+float3(0.0,0.0,0.001))')
        .size('float2({W}*0.1+0.7,length({QB}-{QA})*1.2)')
        .tint('float4(float3(0.72,0.78,1.0)*4.0,{C}.a*0.9*(1.0-{A})*(0.5+0.5*step(0.35,' + hashed('{X1}+' + TRIB_FLICKER, '0.77') + ')))')
        .dyn(x='0.3'),
    burst('BodyArcs', 5 * 5, '0.42', 'Laser').after(float(TRIB_LEADER))
        .f('Transient.SSX1', 'floor({ID}/5.0)')
        .f('Transient.SSK', 'fmod({ID},5.0)')
        .f('Transient.SSTheta', hashed('{X1}', '{SEED}*8.1+0.5') + '*{TAU}')
        .f('Transient.SSX2', '(' + hashed('{X1}', '{SEED}*1.3+2.9') + '<0.5?1.0:-1.0)')
        .f('Transient.SSE', '{HT}*(0.08+0.55*' + hashed('{X1}', '{SEED}*5.7+0.3') + ')')
        .v('Transient.SSPa', sub(BODY_ARC, k='{K}'))
        .v('Transient.SSPb', sub(BODY_ARC, k='({K}+1.0)'))
        .pos('({QA}+{QB})*0.5')
        .along('normalize({QB}-{QA}+float3(0.0,0.0,0.001))')
        .size('float2({W}*0.08+0.6,length({QB}-{QA})*1.2)')
        .tint('float4(' + TRIB_HOT + '*4.5,{C}.a*0.85*(1.0-{A})*(0.4+0.6*step(0.4,' + hashed('{X1}*3.0+' + TRIB_FLICKER, '1.91') + ')))')
        .dyn(x='0.25'),
    # Sparks fly out, fall and skip off the floor at a third of their speed.
    burst('Sparks', 28, '0.8+0.3*{H4}', 'Streak').after(float(TRIB_LEADER))
        .v('Transient.SSV1', SPARK_V)
        .pos('float3({V1}.x,{V1}.y,0.0)*{PAGE}*(1.0-0.35*{A})+float3(0.0,0.0,{HT}*0.3*saturate(1.0-{PAGE}*8.0)+max(' + SPARK_Z + ',0.0)+max(-' + SPARK_Z + ',0.0)*0.3+1.0)')
        .along('normalize(float3({V1}.x,{V1}.y,{V1}.z-980.0*{PAGE})+float3(0.0,0.0,0.001))')
        .size('float2({W}*0.2+0.6,6.0+26.0*{H4})*(1.0-{A})')
        .tint('float4(' + TRIB_HOT + '*4.0,{C}.a*(1.0-{A}))')
        .dyn(x='0.2'),
])


# ================================================================ realm breakthrough (仙途 境界突破)
#
# Played on the anchor's feet, level (User.Radius the ground reach, User.Height how high the show climbs, User.Storm the
# tier: the major realm reached / 10). The colour is the realm's hue, brighter with every major realm. Three systems,
# grander as the realm rises (XianTuBreakthroughFx picks and sizes them):
#   NS_SS_BreakMinor   a minor step: qi wisps spiral up the body over a coloured ensō, motes rise, a star flares at the
#                      heart and one light ring runs out over the ground.
#   NS_SS_BreakMajor   a new major realm: a trigram formation turning in ink and in hue, a light pillar with a white core,
#                      ink strokes and light wisps winding up it in opposite turns, a lotus opening on the ground (a second
#                      ring of petals from 结晶 on), clouds gathering and parting, three shockwaves, a ring of standing
#                      swords from 元神 on, motes climbing the pillar.
#   NS_SS_BreakAscend  from 元婴 on (and under 问道灯/无极香): the sky opens. A wide beam pours down from far above, halos
#                      turn up its length, two streams of light coil up opposite ways (from 化神), the Big Dipper is written
#                      overhead (from 炼虚), petals of light drift down and a sea of clouds rolls out from the feet.
BREAK_TIER2 = 'step(0.15,{ST})'
BREAK_TIER5 = 'step(0.55,{ST})'

system('NS_SS_BreakMinor', ['Radius', 'Height', 'Storm'], [
    burst('Glow', 1, '1.3').flat()
        .pos('float3(0.0,0.0,3.0)')
        .size('float2(1.0,1.0)*{R}*2.6*(0.6+0.4*{EASE})')
        .tint('float4({GLOW}*0.9,{C}.a*0.55*{BELL})')
        .dyn(x='0.05'),
    burst('Ink', 1, '0.9', 'Enso').flat()
        .pos('float3(0.0,0.0,4.0)')
        .size('float2(1.0,1.0)*{R}*2.3*(0.15+0.95*{EASE})')
        .tint('float4({INK},{C}.a*0.45*(1.0-{A}*{A}))')
        .dyn(y='{SEED}+0.5', z='saturate({A}*3.5)', w='0.4'),
    burst('Enso', 1, '1.0', 'Enso').flat()
        .pos('float3(0.0,0.0,5.0)')
        .size('float2(1.0,1.0)*{R}*2.0*(0.2+0.9*{EASE})')
        .tint('float4({HUE},{C}.a*0.9*pow(1.0-{A},1.3))')
        .dyn(x='0.85', y='{SEED}', z='saturate({A}*3.0)', w='0.1'),
    burst('Column', 1, '1.1', 'Streak')
        .pos('float3(0.0,0.0,{HT}*0.9)')
        .along(UP)
        .size('float2({R}*0.22*saturate({A}*10.0+0.02)*(1.0-{A}*{A}),{HT}*1.8)')
        .tint('float4({GLOW}*1.1,{C}.a*0.75*pow(1.0-{A},1.3))')
        .dyn(x='0.4', w='3.0'),
    burst('Spiral', 34, '1.0+0.35*{H3}', 'Wisp')
        .f('Transient.SSK', '{ID}/34.0')
        .f('Transient.SSTheta', '{K}*{TAU}*2.0+{A}*5.0')
        .v('Transient.SSDir', 'normalize(float3(-sin({TH}),cos({TH}),0.6))')
        .pos('float3(cos({TH}),sin({TH}),0.0)*{R}*0.5*(1.0-0.35*{A})+float3(0.0,0.0,{HT}*(0.05+1.15*{EASE}*(0.35+0.65*{K})))')
        .along('{DIR}')
        .size('float2({R}*0.15,{R}*0.5)*(1.0-0.5*{A})')
        .tint('float4({GLOW}*1.3,{C}.a*0.95*{BELL})')
        .dyn(x='0.4'),
    burst('Motes', 32, '1.0+0.5*{H4}')
        .f('Transient.SSTheta', '{H1}*{TAU}')
        .pos('float3(cos({TH}),sin({TH}),0.0)*{R}*(0.2+0.6*{H2})+float3(0.0,0.0,{HT}*{EASE}*(0.5+0.9*{H3}))')
        .size('float2(1.0,1.0)*(2.0+{R}*0.05*{BELL})')
        .tint('float4({HOTG}*1.4,{C}.a*{BELL})'),
    burst('Flare', 1, '0.55', 'Star').after(0.25)
        .pos('float3(0.0,0.0,{HT}*0.55)')
        .size('float2(1.0,1.0)*{R}*(0.9+0.7*{ST})*(1.0-0.5*{A})')
        .turn('{SEED}*90.0')
        .tint('float4({HOTG}*1.6,{C}.a*pow(1.0-{A},1.5))'),
    burst('Shock', 1, '0.65', 'Ring').after(0.25).flat()
        .pos('float3(0.0,0.0,6.0)')
        .size('float2(1.0,1.0)*{R}*(0.5+2.3*{EASE})')
        .tint('float4({GLOW}*1.3,{C}.a*0.8*(1.0-{A})*(1.0-{A}))')
        .dyn(x='0.25', w='0.25'),
])


def break_shock(name, delay, life, reach, gate='1.0'):
    return (burst(name, 1, life, 'Ring').after(delay).flat()
            .pos('float3(0.0,0.0,6.0)')
            .size('float2(1.0,1.0)*{R}*(0.4+' + reach + '*{EASE})*' + gate)
            .tint('float4({GLOW}*1.4,{C}.a*0.85*(1.0-{A})*(1.0-{A}))')
            .dyn(x='0.25', w='0.25'))


system('NS_SS_BreakMajor', ['Radius', 'Height', 'Storm'], [
    burst('Glow', 1, '2.6').flat()
        .pos('float3(0.0,0.0,2.0)')
        .size('float2(1.0,1.0)*{R}*3.3*(0.6+0.4*{EASE})')
        .tint('float4({GLOW},{C}.a*0.45*{BELL})')
        .dyn(x='0.05'),
    burst('Formation', 1, '2.8', 'Bagua').flat()
        .pos('float3(0.0,0.0,3.0)')
        .size('float2(1.0,1.0)*{R}*2.5*(0.5+0.55*{EASE})')
        .turn('{A}*60.0+{SEED}*360.0')
        .tint('float4({INK},{C}.a*0.6*pow(1.0-{A},1.1)*saturate({A}*6.0+0.1))')
        .dyn(z='saturate({A}*2.0)', w='0.0'),
    burst('HueFormation', 1, '2.4', 'Bagua').flat()
        .pos('float3(0.0,0.0,4.0)')
        .size('float2(1.0,1.0)*{R}*2.2*(0.5+0.55*{EASE})')
        .turn('-{A}*45.0+{SEED}*360.0+22.5')
        .tint('float4({HUE},{C}.a*0.75*pow(1.0-{A},1.2)*saturate({A}*5.0))')
        .dyn(x='0.85', z='saturate({A}*2.2)', w='0.0'),
    burst('Lotus', 24, '2.2', 'Wisp').flat()
        .f('Transient.SSK', 'floor({ID}/12.0)')
        .f('Transient.SSTheta', 'fmod({ID},12.0)*0.5235988+{K}*0.2617994')
        .v('Transient.SSDir', 'float3(cos({TH}),sin({TH}),0.0)')
        .pos('{DIR}*{R}*(0.38+0.32*{K})*(0.6+0.4*{EASE})+float3(0.0,0.0,6.0+{K}*2.0)')
        .along('{DIR}')
        .size('float2({R}*(0.28+0.1*{K}),{R}*(0.58+0.2*{K}))*(0.4+0.6*{EASE})*(1.0-{K}*(1.0-' + BREAK_TIER2 + '))')
        .tint('float4({HUE},{C}.a*0.85*pow(1.0-{A},1.2)*saturate({A}*5.0+0.1))')
        .dyn(x='0.85', w='0.0'),
    burst('Beam', 1, '2.4', 'Streak')
        .pos('float3(0.0,0.0,{HT}*0.5)')
        .along(UP)
        .size('float2({R}*(0.5+0.35*{ST})*saturate({A}*10.0+0.02)*(1.0-{A}*{A}),{HT})')
        .tint('float4({GLOW}*1.15,{C}.a*pow(1.0-{A},1.1))')
        .dyn(x='0.55', w='3.0'),
    burst('Core', 1, '2.2', 'Streak')
        .pos('float3(0.0,0.0,{HT}*0.5)')
        .along(UP)
        .size('float2({R}*0.14*saturate({A}*10.0+0.02)*(1.0-{A}*{A}),{HT}*0.98)')
        .tint('float4({HOTG}*1.8,{C}.a*{FADE})')
        .dyn(x='0.1', w='4.0'),
    burst('Spiral', 40, '1.8', 'InkStroke')
        .f('Transient.SSK', 'floor({ID}/2.0)/20.0')
        .f('Transient.SSTheta', '{K}*{TAU}*2.5+fmod({ID},2.0)*{PI}+{A}*5.0')
        .pos('float3(cos({TH}),sin({TH}),0.0)*{R}*0.55*(1.0-0.3*{K})+float3(0.0,0.0,{K}*{HT}*0.8*(0.3+0.7*{EASE}))')
        .along('normalize(float3(-sin({TH}),cos({TH}),0.7))')
        .size('float2({R}*0.1,{R}*0.5)*(1.0-0.4*{A})')
        .tint('float4({INK},{C}.a*0.85*pow(1.0-{A},1.2)*saturate({A}*6.0+0.1))')
        .dyn(w='0.55'),
    burst('Lights', 30, '2.0', 'Wisp')
        .f('Transient.SSK', '{ID}/30.0')
        .f('Transient.SSTheta', '-{K}*{TAU}*2.0-{A}*6.0+{PI}*0.5')
        .v('Transient.SSDir', 'normalize(float3(sin({TH}),-cos({TH}),0.8))')
        .pos('float3(cos({TH}),sin({TH}),0.0)*{R}*0.7*(1.0-0.4*{K})+float3(0.0,0.0,{K}*{HT}*0.9*(0.25+0.75*{EASE}))')
        .along('{DIR}')
        .size('float2({R}*0.11,{R}*0.4)*(1.0-0.5*{A})')
        .tint('float4({GLOW}*1.3,{C}.a*0.9*{BELL})')
        .dyn(x='0.4'),
    burst('Clouds', 8, '2.4', 'Cloud')
        .f('Transient.SSTheta', '{ID}/8.0*{TAU}+{SEED}*{TAU}')
        .pos('float3(cos({TH}),sin({TH}),0.0)*{R}*(1.55-0.45*{BELL})+float3(0.0,0.0,{R}*(0.3+0.2*{H1}))')
        .size('float2({R}*1.1,{R}*0.66)*(0.8+0.3*{H2})')
        .tint('float4(lerp({INK},{GLOW}*1.1,step(0.5,{H3})),{C}.a*0.85*saturate({A}*4.0)*pow(1.0-{A},1.3))')
        .dyn(x='0.9'),
    break_shock('Shock1', 0.15, '0.8', '2.4'),
    break_shock('Shock2', 0.55, '0.9', '2.9'),
    break_shock('Shock3', 1.0, '1.0', '3.4', BREAK_TIER2),
    burst('Swords', 12, '2.4', 'Sword')
        .f('Transient.SSTheta', '{ID}/12.0*{TAU}+{A}*1.2')
        .pos('float3(cos({TH}),sin({TH}),0.0)*{R}*0.95+float3(0.0,0.0,{R}*0.55*(0.3+0.7*{EASE}))')
        .along(UP)
        .size('float2({R}*0.14,{R}*0.5)*' + BREAK_TIER5)
        .tint('float4({GLOW}*1.2,{C}.a*0.85*saturate({A}*5.0)*pow(1.0-{A},1.3))')
        .dyn(x='0.6'),
    burst('Motes', 72, '1.6+0.6*{H4}')
        .f('Transient.SSTheta', '{H1}*{TAU}')
        .pos('float3(cos({TH}),sin({TH}),0.0)*{R}*0.7*{H2}+float3(0.0,0.0,{HT}*{EASE}*(0.3+0.7*{H3}))')
        .size('float2(1.0,1.0)*(2.5+{R}*0.05*{BELL})')
        .tint('float4({HOTG}*1.4,{C}.a*{BELL})'),
    burst('Flare', 1, '0.8', 'Star').after(0.1)
        .pos('float3(0.0,0.0,{R}*0.6)')
        .size('float2(1.0,1.0)*{R}*(1.6+1.0*{ST})*(1.0-0.4*{A})')
        .turn('{SEED}*90.0')
        .tint('float4({HOTG}*1.6,{C}.a*pow(1.0-{A},1.5))'),
])

system('NS_SS_BreakAscend', ['Radius', 'Height', 'Storm'], [
    burst('Flash', 1, '0.7')
        .pos('float3(0.0,0.0,{HT}*0.3)')
        .size('float2(1.0,1.0)*{R}*(4.0+3.0*{ST})*(0.6+0.4*{A})')
        .tint('float4({HOTG}*2.2,{C}.a*pow(1.0-{A},2.0))')
        .dyn(x='0.0'),
    burst('SkyBeam', 1, '3.4', 'Streak')
        .pos('float3(0.0,0.0,{HT}*0.5)')
        .along(UP)
        .size('float2({R}*(0.8+0.6*{ST})*saturate({A}*6.0+0.02)*(1.0-{A}*{A}),{HT}*1.05)')
        .tint('float4({GLOW}*0.9,{C}.a*0.45*pow(1.0-{A},1.4))')
        .dyn(x='0.5', w='3.0'),
    burst('SkyCore', 1, '3.2', 'Streak')
        .pos('float3(0.0,0.0,{HT}*0.5)')
        .along(UP)
        .size('float2({R}*0.3*saturate({A}*6.0+0.02)*(1.0-{A}*{A}),{HT})')
        .tint('float4({HOTG}*2.0,{C}.a*{FADE})')
        .dyn(x='0.1', w='4.0'),
    burst('Halos', 3, '3.2', 'Ring').flat()
        .f('Transient.SSK', '{ID}')
        .pos('float3(0.0,0.0,{HT}*(0.16+0.2*{K})*(0.4+0.6*{EASE}))')
        .size('float2(1.0,1.0)*{R}*(1.7-0.4*{K})*(0.3+0.7*{EASE})*(1.0+0.05*sin({PAGE}*3.0+{K}))')
        .turn('{A}*90.0*(1.0-2.0*fmod({K},2.0))')
        .tint('float4({GLOW}*1.3,{C}.a*0.8*saturate({A}*4.0)*pow(1.0-{A},1.4))')
        .dyn(x='0.3', w='0.25'),
    burst('Coils', 64, '2.6', 'Streak')
        .f('Transient.SSK', 'floor({ID}/2.0)/32.0')
        .f('Transient.SSX1', '1.0-2.0*fmod({ID},2.0)')
        .f('Transient.SSTheta', '{X1}*({K}*{TAU}*3.0+{A}*4.0)+fmod({ID},2.0)*{PI}')
        .pos('float3(cos({TH}),sin({TH}),0.0)*{R}*(0.8-0.35*{K})+float3(0.0,0.0,{K}*{HT}*0.75*(0.25+0.75*{EASE}))')
        .along('normalize(float3(-sin({TH})*{X1},cos({TH})*{X1},0.8))')
        .size('float2({R}*0.06,{R}*0.42)*(1.0-0.5*{A})*' + BREAK_TIER5)
        .tint('float4({GLOW}*1.4,{C}.a*0.9*saturate({A}*6.0)*pow(1.0-{A},1.3))')
        .dyn(x='0.3'),
    burst('Chart', 1, '3.2', 'Dipper').flat()
        .pos('float3(0.0,0.0,{HT}*0.85)')
        .size('float2(1.0,1.0)*{R}*2.6*(0.5+0.5*{EASE})*step(0.65,{ST})')
        .turn('{A}*20.0+{SEED}*360.0')
        .tint('float4({HOTG}*1.4,{C}.a*0.8*saturate({A}*3.0)*pow(1.0-{A},1.3))')
        .dyn(z='saturate({A}*1.8)'),
    burst('Petals', 48, '3.0+0.6*{H4}', 'Blossom')
        .f('Transient.SSTheta', '{H1}*{TAU}')
        .pos('float3(cos({TH}),sin({TH}),0.0)*{R}*1.4*{H2}+float3(0.0,0.0,{HT}*(0.9-0.85*{EASE})*(0.6+0.4*{H3}))+' + SWAY + '*{R}*0.12')
        .size('float2(1.0,1.0)*{R}*(0.06+0.04*{H4})')
        .turn('{H1}*360.0+{PAGE}*({H2}-0.5)*200.0')
        .tint('float4({HUE}*1.3,{C}.a*0.9*saturate({A}*6.0)*(1.0-{A}*{A}))')
        .dyn(w='step({H4},0.6)'),
    burst('CloudSea', 14, '3.2', 'Cloud')
        .f('Transient.SSTheta', '{ID}/14.0*{TAU}+{H1}*0.3')
        .pos('float3(cos({TH}),sin({TH}),0.0)*{R}*(0.6+1.1*{EASE})+float3(0.0,0.0,{R}*0.1)')
        .size('float2({R}*1.0,{R}*0.6)*(0.8+0.4*{H2})')
        .tint('float4(lerp({WASH},{GLOW},0.35+0.4*step(0.5,{H3})),{C}.a*0.65*saturate({A}*4.0)*pow(1.0-{A},1.4))')
        .dyn(x='0.9'),
])

EFFECTS = tuple(name for name, *_ in SYSTEMS)


def spec():
    return [dict(name=name, params=params, warmup=warmup, emitters=[e.data() for e in emitters]) for name, params, emitters, warmup in SYSTEMS]


def spec_json():
    return json.dumps(spec(), ensure_ascii=False)

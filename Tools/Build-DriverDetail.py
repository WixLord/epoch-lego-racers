"""Retarget the original menu minifigures to the seated driving skeleton.

Input is the token JSON exported by Build-DriverDetail.ps1. This is an asset
conversion, not a change to the driver's physics or animation controllers.
"""
import copy
import json
import math
import struct
import sys
import shutil
from pathlib import Path

data = json.loads(Path(sys.argv[1]).read_text(encoding='utf-8-sig'))
output = Path(sys.argv[2])


def tokens(name):
    return [(v['Type'], v.get('Value')) for v in data[name]]


def section(kind, values, count):
    return [(kind, None), (7, None), (4, count), (8, None), (5, None)] + values + [(6, None)]


def write(name, values):
    dest = output / name
    dest.parent.mkdir(parents=True, exist_ok=True)
    with dest.open('wb') as f:
        for kind, value in values:
            f.write(bytes([kind]) if kind < 256 else b'\x13' + struct.pack('<H', kind))
            if kind == 2:
                f.write(value.encode('ascii') + b'\0')
            elif kind == 3:
                f.write(struct.pack('<f', value))
            elif kind == 4:
                f.write(struct.pack('<i', value))


def mesh(name):
    t = tokens(name)
    p = 0
    vertices, indices, faces, materials, cache = [], [], [], [], {}
    node = material = 0
    scale = 1.0
    while p < len(t):
        kind = t[p][0]
        p += 1
        if kind == 0:
            continue
        if kind == 51:
            scale = t[p][1]
            p += 1
            continue
        assert t[p][0] == 7
        count = t[p + 1][1]
        p += 4
        for _ in range(count):
            if kind == 39:
                materials.append(t[p][1])
                p += 1
            elif kind in (41, 42):
                size = 8 if kind == 41 else 9
                vertices.append([v[1] for v in t[p:p + size]])
                p += size
            elif kind == 45:
                indices.append([v[1] for v in t[p:p + 3]])
                p += 3
            elif kind == 46:
                cmd = t[p][0]
                p += 1
                if cmd == 39:
                    material = t[p][1]
                    p += 1
                elif cmd == 50:
                    node = t[p][1]
                    p += 1
                elif cmd == 49:
                    dest, first, n = [v[1] for v in t[p:p + 3]]
                    p += 3
                    for j in range(n):
                        cache[dest + j] = (vertices[first + j], node)
                elif cmd == 45:
                    first, n = [v[1] for v in t[p:p + 2]]
                    p += 2
                    for j in range(n):
                        faces.append((materials[material], copy.deepcopy([cache[k] for k in indices[first + j]])))
                else:
                    raise ValueError((name, cmd))
            else:
                raise ValueError((name, kind))
        assert t[p][0] == 6
        p += 1
    return faces, scale


def write_mesh(name, faces, scale, colored=False):
    materials = list(dict.fromkeys(m for m, _ in faces))
    # The runtime's expression controller reserves material zero for the face.
    if 'face' in materials:
        materials.remove('face')
        materials.insert(0, 'face')
    verts, groups, triangles = [], [], []
    last_mat = None
    for i, (material, corners) in enumerate(faces):
        if material != last_mat:
            groups.append([(39, None), (4, materials.index(material))])
            last_mat = material
        for j, (v, node) in enumerate(corners):
            verts.extend((4 if colored and k >= 5 else 3, value) for k, value in enumerate(v))
            groups.append([(50, None), (4, node)])
            groups.append([(49, None), (4, j), (4, i * 3 + j), (4, 1)])
        groups.append([(45, None), (4, i), (4, 1)])
        triangles.extend([(4, 0), (4, 1), (4, 2)])
    result = section(39, [(2, m) for m in materials], len(materials))
    result += [(51, None), (3, scale)]
    result += section(42 if colored else 41, verts, len(faces) * 3)
    result += section(45, triangles, len(faces))
    result += section(46, [t for group in groups for t in group], len(groups))
    write(name, result)


def skeleton(name):
    t = tokens(name)
    p = 5
    nodes = []
    while p < len(t) and t[p][0] == 39:
        node = dict(name=t[p + 1][1], pos=[0, 0, 0], quat=[0, 0, 0, 1], parent=None)
        p += 3
        while t[p][0] != 6:
            kind = t[p][0]
            p += 1
            if kind == 40:
                node['pos'] = [v[1] for v in t[p:p + 3]]
                p += 3
            elif kind == 41:
                node['quat'] = [v[1] for v in t[p:p + 4]]
                p += 4
            elif kind == 42:
                node['parent'] = next(i for i, n in enumerate(nodes) if n['name'] == t[p][1])
                p += 1
        nodes.append(node)
        p += 1
    return nodes


def rotate(p, q):
    x, y, z, w = q
    a, b, c = p
    tx, ty, tz = 2 * (y*c-z*b), 2 * (z*a-x*c), 2 * (x*b-y*a)
    return [a+w*tx+y*tz-z*ty, b+w*ty+z*tx-x*tz, c+w*tz+x*ty-y*tx]


def world(p, node, nodes):
    while node is not None:
        n = nodes[node]
        p = [a+b for a, b in zip(rotate(p, n['quat']), n['pos'])]
        node = n['parent']
    return p


def local(p, node, nodes):
    chain = []
    while node is not None:
        chain.append(node)
        node = nodes[node]['parent']
    for i in reversed(chain):
        n = nodes[i]
        p = rotate([a-b for a, b in zip(p, n['pos'])], [-v for v in n['quat'][:3]] + n['quat'][3:])
    return p


def retarget(variant, target, standing=False):
    faces, scale = mesh('menu/' + variant + '.GDB')
    nodes = skeleton('menu/' + variant + '.SDB')
    dest = skeleton(target)
    # Original menu skeleton: pelvis, two leg chains, torso, and two arm chains.
    # Bake the sitting pose, then retain the existing in-car torso/arm animation.
    result = []
    for material, corners in faces:
        if material == 'face':
            continue  # Head/hat is supplied separately by the original cosmetic system.
        new = []
        for v, bone in corners:
            transformed = []
            for sample in (v[:3], [v[k]+v[k+5] for k in range(3)]):
                p = world(sample, bone, nodes)
                dest_bone = 0
                if 2 <= bone <= 13 and standing:
                    p[2] += 22
                elif 2 <= bone <= 13:
                    hip = 2 if bone < 8 else 8
                    pivot = world([0, 0, 0], hip, nodes)
                    d = [a-b for a, b in zip(p, pivot)]
                    p = [pivot[0]-d[2], pivot[1]+d[1], pivot[2]+d[0]+22]
                elif bone >= 19:
                    left = bone < 24
                    shoulder, elbow = (19, 21) if left else (24, 26)
                    pivot = world([0, 0, 0], shoulder, nodes)
                    d = [a-b for a, b in zip(p, pivot)]
                    # Swing outstretched arms forward, bend the elbows to the steering wheel.
                    forward = d[1] * (1 if left else -1)
                    side = (-d[0] + 16) * (1 if left else -1)
                    upper = abs(world([0, 0, 0], elbow, nodes)[1]-pivot[1])
                    if bone >= elbow:
                        forward, height = upper*.5+(forward-upper), -upper*.8660254+d[2]
                    else:
                        forward, height = forward*.5+d[2]*.8660254, -forward*.8660254+d[2]*.5
                    dest_bone = 2 if left else 3
                    base = world([0, 0, 0], dest_bone, dest)
                    p = [base[0]+forward*.82, base[1]+side, base[2]+height*.82+10]
                else:
                    p[2] += 22
                    if bone >= 14:
                        dest_bone = 1
                transformed.append(local(p, dest_bone, dest))
            normal = [b-a for a,b in zip(*transformed)]
            length = math.sqrt(sum(a*a for a in normal))
            v = transformed[0] + v[3:5] + [a/length for a in normal]
            new.append((v, dest_bone))
        result.append((material, new))
    return result, scale


def catalog():
    t = tokens('catalog')
    result = {}
    p = 0
    while p < len(t) and t[p][0]:
        kind = t[p][0]
        count = t[p+2][1]
        p += 5
        end = next(i for i in range(p, len(t)) if t[i][0] == 6)
        result[kind] = (count, [v[1] for v in t[p:end]])
        p = end+1
    return result


def named_blocks(name):
    t = tokens(name)
    p = 5
    result = {}
    while p < len(t) and t[p][0] == 39:
        start = p
        label = t[p+1][1]
        p += 3
        depth = 1
        while depth:
            depth += (t[p][0] == 5) - (t[p][0] == 6)
            p += 1
        result[label.lower()] = t[start:p]
    return result


def hats():
    t = tokens('hats')
    assert t[0][0] == 40
    n = t[2][1]
    vertices = []
    p = 5
    for _ in range(n):
        v = [v[1] for v in t[p:p+8]]
        for k in range(5,8):
            v[k] = (v[k] if v[k] < 128 else v[k]-256)/127
        shade = max(0,min(255,round(255*(.60+.40*max(0,.3*v[5]-.4*v[6]+.8660254*v[7])))))
        v = v[:5]+[shade,shade,shade,255]
        v[3] /= 4096
        v[4] /= 4096
        vertices.append(v)
        p += 8
    assert t[p][0] == 6 and t[p+1][0] == 44
    p += 6
    result = {}
    while t[p][0] == 44:
        name = t[p+1][1]
        p += 3
        faces = []
        while t[p][0] != 6:
            kind = t[p][0]
            p += 1
            if kind == 45:
                scale = t[p][1]
                p += 1
                assert abs(scale-1/64) < 1e-6
            elif kind == 43:
                count = t[p+1][1]
                p += 4
                for _ in range(count):
                    assert t[p][0] == 43 and t[p+1][0] == 5
                    p += 2
                    material = None
                    while t[p][0] != 6:
                        field = t[p][0]
                        p += 1
                        if field == 39:
                            material = t[p][1]
                            p += 1
                        elif field == 42:
                            triangles = t[p+1][1]
                            p += 4
                            for _ in range(triangles):
                                corners = [(vertices[t[p+j][1]][:], 5) for j in range(3)]
                                faces.append((material, corners))
                                p += 3
                            assert t[p][0] == 6
                            p += 1
                    p += 1
                assert t[p][0] == 6
                p += 1
            else:
                raise ValueError(('hat section', kind))
        p += 1
        result[name.lower()] = faces
    return result


def build_presets():
    cat = catalog()
    head_names = cat[45][1]
    face_names = cat[42][1][1::2]
    torsos = cat[43][1][2:]
    legs = cat[44][1][2:]
    body_names = cat[46][1]
    head_meshes = hats()
    material_defs = named_blocks('materials')
    texture_defs = named_blocks('textures')
    sources = Path(sys.argv[3]) / 'MENUDATA/PARTDB'
    for number, (driver, entry) in enumerate(named_blocks('drivers').items()):
        fields = {entry[i][0]: entry[i+1][1] for i in range(3, len(entry)-1, 2)}
        hat, face, torso, leg = [fields.get(k, 0) for k in (53,54,55,56)]
        variant = body_names[torsos[torso*3+1] + 2*legs[leg*3+1]].upper()
        body, scale = retarget(variant, 'race/PELVIS.SDB')
        # Racing models use baked vertex lighting. Keep the original cosmetic
        # textures and menu head colors while supplying a restrained body shade.
        converted = []
        for material, corners in body:
            colored = []
            for v, node in corners:
                shade = int(round(255*(.60+.40*max(0, .3*v[5]-.4*v[6]+.8660254*v[7]))))
                shade = max(0, min(255, shade))
                colored.append((v[:5]+[shade,shade,shade,255], node))
            converted.append((material, colored))
        head = copy.deepcopy(head_meshes[head_names[hat].lower()])
        converted += head
        replacements = {'face':face_names[face]+'dflt', 'torso':torsos[torso*3], 'legs':legs[leg*3]}
        used = list(dict.fromkeys(m for m, _ in converted))
        mapping, material_blocks, texture_blocks = {}, [], []
        for index, material in enumerate(used):
            source = replacements.get(material.lower(), material).lower()
            block = copy.deepcopy(material_defs[source])
            tex_pos = next((i+1 for i,t in enumerate(block) if t[0] == 44), None)
            source_texture = block[tex_pos][1].lower() if tex_pos is not None else None
            unique = 'hd%02d%02d' % (number, index)
            mapping[material] = unique
            block[1] = (2, unique)
            if tex_pos is None:
                material_blocks += block
                continue
            block[tex_pos] = (2, unique)
            # Match the race material's diffuse and ambient brightness.
            for i,t in enumerate(block):
                if t[0] in (40,41):
                    for j in (1,2,3):
                        block[i+j] = (4,127)
            material_blocks += block
            texture = copy.deepcopy(texture_defs[source_texture])
            texture[1] = (2, unique)
            texture_blocks += texture
            src = sources / (source_texture.upper()+'.BMP')
            dest = output / 'GAMEDATA/COMMON' / (unique.upper()+'.BMP')
            dest.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(src, dest)
        converted = [(mapping[m], corners) for m,corners in converted]
        write_mesh('GAMEDATA/COMMON/'+fields[42].upper()+'.GDB', converted, scale, True)
        write('GAMEDATA/COMMON/'+fields[40].upper()+'.MDB', section(39, material_blocks, len(used)))
        write('GAMEDATA/COMMON/'+fields[41].upper()+'.TDB', section(39, texture_blocks, sum(1 for i,t in enumerate(texture_blocks) if t[0] == 39)))
        assert all(0 <= node < 6 for _, corners in converted for _,node in corners)
        print(driver, variant, len(converted), 'triangles')


if __name__ == '__main__':
    # First produce and validate the common seated body; preset cosmetics follow.
    body, scale = retarget('RR', 'low/LEG_BOX.SDB')
    original, _ = mesh('low/LEG_BOX.GDB')
    body += [f for f in original if f[0] == 'face']
    write_mesh('MENUDATA/PARTDB/GAMEPART/LEG_BOX.GDB', body, scale)
    write_mesh('MENUDATA/PARTDB/MENUPART/LEG_BOX.GDB', body, scale)
    standing_body, _ = retarget('RR', 'low/LEG_BOX.SDB', True)
    standing_body += [f for f in original if f[0] == 'face']
    for folder in ('GAMEPART', 'MENUPART'):
        write_mesh('MENUDATA/PARTDB/'+folder+'/BIKEMAN.GDB', standing_body, scale)
    # Save posed geometry for offline visual inspection before installing it.
    posed = [[world(v[:3], n, skeleton('low/LEG_BOX.SDB')) for v, n in corners]
             for m, corners in body if m != 'face']
    (output / 'posed-body.json').write_text(json.dumps(posed))
    print('Seated body:', len(body), 'triangles (original:', len(original), ')')
    build_presets()

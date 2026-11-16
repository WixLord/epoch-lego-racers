"""Build a mod-only release; all base data is reconstructed on the recipient's PC.

Never zip the working game folder. Payload binaries are explicitly allowlisted.
The RMP1 delta copies unchanged spans from the verified base archive.
"""
import argparse
import hashlib
import io
import json
from pathlib import Path
import struct
import zipfile


def digest(data):
    return hashlib.sha256(data).hexdigest()


def make_delta(base, modified):
    block = 64
    index = {}
    for pos in range(0, len(base) - block + 1, block):
        index.setdefault(base[pos:pos + block], pos)
    out = io.BytesIO()
    out.write(b'RMP1')
    literal = bytearray()
    copied = inserted = 0

    def flush():
        nonlocal inserted
        if literal:
            out.write(b'\x01' + struct.pack('<i', len(literal)) + literal)
            inserted += len(literal)
            literal.clear()

    pos = 0
    while pos < len(modified):
        match = index.get(modified[pos:pos + block]) if pos + block <= len(modified) else None
        if match is None:
            literal.append(modified[pos])
            pos += 1
            if len(literal) >= 65536:
                flush()
            continue
        flush()
        length = block
        limit = min(len(base) - match, len(modified) - pos)
        while length + 4096 <= limit and base[match + length:match + length + 4096] == modified[pos + length:pos + length + 4096]:
            length += 4096
        while length < limit and base[match + length] == modified[pos + length]:
            length += 1
        out.write(b'\x00' + struct.pack('<qi', match, length))
        pos += length
        copied += length
    flush()
    out.write(b'\xff')
    return out.getvalue(), copied, inserted


def apply_delta(base, delta):
    stream = io.BytesIO(delta)
    assert stream.read(4) == b'RMP1'
    result = bytearray()
    while True:
        op = stream.read(1)
        if op == b'\xff':
            assert not stream.read()
            return bytes(result)
        if op == b'\x00':
            offset, count = struct.unpack('<qi', stream.read(12))
            assert offset >= 0 and count > 0 and offset + count <= len(base)
            result.extend(base[offset:offset + count])
        elif op == b'\x01':
            count, = struct.unpack('<i', stream.read(4))
            data = stream.read(count)
            assert count > 0 and len(data) == count
            result.extend(data)
        else:
            raise ValueError('Invalid delta')


def build(workspace, output, version):
    base = workspace / 'Game'
    preview = workspace / 'Game Renderer Preview'
    original = (base / 'LEGO.JAM').read_bytes()
    modified = (preview / 'LEGO.JAM').read_bytes()
    patch, copied, inserted = make_delta(original, modified)
    assert apply_delta(original, patch) == modified, 'Patch roundtrip failed'
    files = {}
    manifest = dict(schema=1, gameId='lego-racers', version=version,
                    executable='RacersReborn.exe', arguments=['-novideo', '-alphatrans'],
                    baseFiles=[], files=[], patches=[])
    for path in [base / 'LEGO.JAM', *sorted(base.glob('*.tun'))]:
        manifest['baseFiles'].append(dict(path=path.name, sha256=digest(path.read_bytes())))
    for name in ['RacersReborn.exe', 'GolDP.dll', 'RebornRenderer.dll']:
        data = (preview / name).read_bytes()
        files['payload/' + name] = data
        manifest['files'].append(dict(source='payload/' + name, destination=name, sha256=digest(data)))
    redist = Path('C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Redist/MSVC/14.44.35112/x86/Microsoft.VC143.CRT')
    for name in ['vcruntime140.dll', 'msvcp140.dll']:
        data = (redist / name).read_bytes()
        files['payload/' + name] = data
        manifest['files'].append(dict(source='payload/' + name, destination=name, sha256=digest(data)))
    logo = (workspace / 'Assets/Branding/RemasterLogo.tga').read_bytes()
    files['payload/RemasterLogo.tga'] = logo
    manifest['files'].append(dict(source='payload/RemasterLogo.tga', destination='RemasterLogo.tga', sha256=digest(logo)))
    for name in ['BradBunR.ttf', 'bradybunch.txt']:
        data = (workspace / 'Assets/Fonts/BradyBunchRemastered' / name).read_bytes()
        source = 'payload/Fonts/' + name
        files[source] = data
        manifest['files'].append(dict(source=source, destination='Fonts/' + name, sha256=digest(data)))
    config = b'[Video]\nRenderer=d3d11\nRaceResolution=desktop\nAntialiasing=4x\nFiltering=16\nForceVerticalSync=true\nShadows=scene\nBloom=medium\nAmbientOcclusion=subtle\n[UI]\nFontFile=Fonts\\BradBunR.ttf\n'
    files['payload/Reborn.ini'] = config
    manifest['files'].append(dict(source='payload/Reborn.ini', destination='Reborn.ini', sha256=digest(config), preserve=True))
    files['patches/LEGO.JAM.rmp'] = patch
    manifest['patches'].append(dict(source='patches/LEGO.JAM.rmp', base='LEGO.JAM', destination='LEGO.JAM',
                                    baseSha256=digest(original), sha256=digest(modified), size=len(modified)))
    files['manifest.json'] = json.dumps(manifest, indent=2).encode()
    files['licenses/racers-LGPL-3.0.txt'] = (workspace / 'Source/racers/LICENSE').read_bytes()
    files['licenses/COPYING-GPL-3.0.txt'] = (workspace / 'Distribution/Mod/COPYING-GPL-3.0.txt').read_bytes()
    files['licenses/Microsoft-runtime.txt'] = b'Microsoft Visual C++ x86 runtime DLLs, copied unmodified from Visual Studio 2022 redistributable files. Copyright Microsoft Corporation. Microsoft software terms apply. https://learn.microsoft.com/en-us/cpp/windows/redistributing-visual-cpp-files\n'
    files['NOTICE.txt'] = b'''LEGO Racers Remastered - mod-only technical preview
Requires the supported original PC game files. No original game archive, music,
save files, original game executable or compatibility-wrapper binaries included.
RacersReborn and GolDP derive from isledecomp/racers (LGPL-3.0).
Corresponding modified source must accompany every binary release.
This package has not yet completed its public redistribution review.
'''
    output.mkdir(parents=True, exist_ok=True)
    archive = output / 'lego-racers-remaster.zip'
    with zipfile.ZipFile(archive, 'w', zipfile.ZIP_DEFLATED, compresslevel=9) as z:
        for name, data in files.items():
            z.writestr(name, data)
    report = dict(version=version, sha256=digest(archive.read_bytes()), size=archive.stat().st_size,
                  baseArchiveSha256=digest(original), outputArchiveSha256=digest(modified),
                  bytesCopiedFromUserArchive=copied, deltaLiteralBytes=inserted,
                  includedFiles=sorted(files), excluded=['Original game executables', 'LEGO.JAM', '*.tun', 'Save/',
                      'D3DImm.dll', 'DDraw.dll', 'logs', 'credentials'],
                  publicReleaseReady=False)
    (output / 'package-audit.json').write_text(json.dumps(report, indent=2))
    print(json.dumps({k: v for k, v in report.items() if k != 'includedFiles'}, indent=2))


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--workspace', type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument('--output', type=Path, default=Path(__file__).resolve().parents[1] / 'artifacts')
    parser.add_argument('--version', default='v0.1.0')
    args = parser.parse_args()
    build(args.workspace.resolve(), args.output.resolve(), args.version)

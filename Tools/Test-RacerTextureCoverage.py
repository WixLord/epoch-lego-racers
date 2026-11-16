"""Check saved-racer texture reloads against a reconstructed game archive."""
import struct
import sys
from pathlib import Path

data = Path(sys.argv[1]).read_bytes()
assert data[:4] == b'LJAM', 'Invalid game archive'
entries = {}
pending = [(4, '')]
while pending:
    offset, prefix = pending.pop()
    count, = struct.unpack_from('<I', data, offset)
    offset += 4
    for _ in range(count):
        name = data[offset:offset + 12].rstrip(b'\0').decode('ascii')
        start, size = struct.unpack_from('<II', data, offset + 12)
        assert start + size <= len(data), 'Truncated archive entry'
        entries[prefix + name] = data[start:start + size]
        offset += 20
    count, = struct.unpack_from('<I', data, offset)
    offset += 4
    for _ in range(count):
        name = data[offset:offset + 12].rstrip(b'\0').decode('ascii')
        start, = struct.unpack_from('<I', data, offset + 12)
        pending.append((start, prefix + name + '/'))
        offset += 16

directories = sorted({name.rsplit('/', 1)[0] for name in entries
                      if name.rsplit('/', 1)[-1].startswith('LH_') and name.endswith('.BMP')})
assert 'GAMEDATA/COMMON' in directories
names = ['SP_' + expression for expression in ['DFLT', 'ANGRY', 'BLINK', 'HAPPY', 'SAD', 'SUPRZ']]
names += ['SP_CHST', 'SP_LEGS']
missing = [folder + '/' + name + '.BMP' for folder in directories for name in names
           if folder + '/' + name + '.BMP' not in entries]
assert not missing, 'Missing saved-racer resources:\n' + '\n'.join(missing)
for folder in directories:
    for name in names:
        assert entries[folder + '/' + name + '.BMP'] == entries['MENUDATA/PARTDB/' + name + '.BMP'], \
            'Different texture data in ' + folder + '/' + name
print('PASS:', len(directories) * len(names), 'texture lookups across', len(directories),
      'race/menu/cinematic resource directories.')

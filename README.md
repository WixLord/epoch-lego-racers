# LEGO Racers Remastered — private development preview

Personal remaster mod based on [isledecomp/racers](https://github.com/isledecomp/racers).
This repository contains modified engine source, the Direct3D 11 renderer and
asset-processing tools. The release ZIP contains mod binaries and a delta patch.
It does not contain the full game archive, music, original game executable,
personal saves, third-party fonts, branded replacement logo, or dgVoodoo binaries.

## Install

Use the Epoch Remasters installer with your supported original PC installation.
The exact base archive and music hashes are in the release manifest. The modified
JAM is reconstructed locally. This private preview is not ready for public release.
Required Microsoft Visual C++ x86 runtime DLLs are included app-locally.

## Build binaries

Windows, Visual Studio 2022 C++ tools, and CMake 3.25+:

```powershell
cmake -S Source/racers -B Build/racers -G "Visual Studio 17 2022" -A Win32 -DRACERS_DECOMP_ASSERT=OFF
cmake --build Build/racers --config Release
cmake -S Source/Renderer -B Build/renderer -G "Visual Studio 17 2022" -A Win32
cmake --build Build/renderer --config Release --target RebornRenderer
```

The renderer font-unload test references an optional private font asset; build
the RebornRenderer target as above without that test. No base assets are needed
to compile the three binaries. Asset tools require a locally extracted base game
and the locally edited bike source; those are not committed. The release delta
is the installable representation of those data changes.

## Notices

Engine/GolDP source retains upstream LGPL-3.0 notices and license. DirectX 6
import libraries and headers are inherited build dependencies from upstream.
Original game rights and trademarks remain with their respective owners.
Private development does not establish permission for public redistribution.

Upstream source revision: `7675cebe85578e529d384bc3a5a47ac6163cb553`

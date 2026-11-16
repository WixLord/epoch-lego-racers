"""Build the opt-in online package from the verified stable mod payload, never a game folder."""
import hashlib
import json
import subprocess
import zipfile
from pathlib import Path

root = Path(__file__).resolve().parents[2]
output = root / "Distribution/artifacts/online-v0.1.7"
output.mkdir(parents=True, exist_ok=True)
with zipfile.ZipFile(root / "Distribution/artifacts/lego-racers-remaster.zip") as source:
    files = {name: source.read(name) for name in source.namelist()}
manifest = json.loads(files["manifest.json"])
assert manifest["gameId"] == "lego-racers" and manifest["version"] == "v0.1.10"
manifest.update(gameId="lego-racers-online", version="online-v0.1.7", executable="EpochOnline.exe", arguments=["--lobby"])
for name, path in {
    "RacersReborn.exe": root / "Build/racers/Release/RacersReborn.exe",
    "EpochOnline.exe": root / "Build/online/Release/EpochOnline.exe",
}.items():
    content = path.read_bytes()
    source = "payload/" + name
    files[source] = content
    manifest["files"] = [entry for entry in manifest["files"] if entry["destination"] != name]
    manifest["files"].append(dict(source=source, destination=name, sha256=hashlib.sha256(content).hexdigest()))
files["licenses/GameNetworkingSockets.txt"] = (root / "Source/GameNetworkingSockets/LICENSE").read_bytes()
for dependency in ["abseil", "openssl", "protobuf", "utf8-range", "nlohmann-json"]:
    files["licenses/" + dependency + ".txt"] = (root / "Build/vcpkg/installed/x64-windows-static/share" / dependency / "copyright").read_bytes()
files["ONLINE-WIP.txt"] = b"""Lego Racers - Epoch Remastered (Online WIP)
Install this separate entry through Epoch Launcher 0.4.13 or newer.
Requires your own supported PC game. No base archive, music or personal saves included.

Play opens the online lobby. Enter a name and host or join a room.
Wait for every peer to connect. Everyone clicks Ready; the host clicks Start race.
The game opens the online racer picker directly. Select your saved racer and click Join Race.
Wait for every player to join before the synchronized start.
The prototype selects Pirate Skull Pass with 2-6 human racers and no bots.
Names above cars use the saved racer names. Custom cars are exchanged automatically.
For the next session, everyone exits the game to return to the same lobby.
Late arrivals wait for the next session and an available player slot.

WIP limitations: weapons and shared race results are not synchronized. Host migration
is not implemented. A broken connection can end the session. This is a first driving
playtest, not finished multiplayer. Keep the stable remaster for normal play.

Feedback: https://discord.gg/BFJZM7KFQQ
Include player count, steps to reproduce, and whether connections use direct or relay.
"""
files["manifest.json"] = json.dumps(manifest, indent=2).encode()
archive = output / "lego-racers-online-wip.zip"
with zipfile.ZipFile(archive, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as target:
    for name, content in files.items():
        assert not name.lower().endswith((".jam", ".tun", ".sav", ".env"))
        target.writestr(name, content)

# Corresponding engine/renderer source and build metadata, without personal assets.
allowed = {".cpp", ".h", ".c", ".hpp", ".cmake", ".txt", ".def", ".rc", ".ico", ".lib", ".in", ".asm"}
with zipfile.ZipFile(output / "online-corresponding-source.zip", "w", zipfile.ZIP_DEFLATED) as target:
    engine = root / "Source/racers"
    names = subprocess.check_output(["git", "-C", str(engine), "ls-files", "--cached", "--others", "--exclude-standard", "-z"]).decode().split("\0")
    for name in names:
        path = Path(name)
        if not name or not (engine / path).is_file() or path.parts[0] in {".github", "assets", "docker", "reccmp"}:
            continue
        if path.suffix.lower() in allowed or path.name in {"LICENSE", "CMakeLists.txt", ".clang-format"}:
            target.write(engine / path, "Source/racers/" + path.as_posix())
    for path in (root / "Source/Renderer").iterdir():
        if path.is_file() and path.suffix.lower() in allowed:
            target.write(path, "Source/Renderer/" + path.name)
    for path in (root / "Distribution/Online").iterdir():
        if path.is_file() and path.suffix in {".cpp", ".py", ".txt"}:
            target.write(path, "Distribution/Online/" + path.name)
    target.write(Path(__file__), "Distribution/Packaging/build_online_package.py")
    for name in ["Paint-SpiderMan.jsx", "Export-PaintCommands.cjs", "Render-PixelPaint.ps1", "Build-SpiderManParts.ps1", "Test-RacerTextureCoverage.py", "PackedBitmap.cs", "BinaryTokens.cs", "JamArchive.cs", "Read-JamArchive.ps1"]:
        target.write(root / "Tools" / name, "Tools/" + name)
    for name in ["README.md", "mask.png", "torso-front.png", "torso-game.png", "mask.json", "torso-front.json"]:
        target.write(root / "Assets/SpiderMan" / name, "Assets/SpiderMan/" + name)
    for name in ["Build-BatmanParts.ps1", "Build-BatmanCowl.py", "Export-BatmanCowl.py", "Build-CapeBodies.py", "Build-DriverDetail.py"]:
        target.write(root / "Tools" / name, "Tools/" + name)
    for name in ["README.md", "BatmanCowl.fbx", "cowl-mesh.json", "mask.png", "mask.json", "torso-front-custom.png", "torso-game.png", "torso-front.json", "torso-back.json"]:
        target.write(root / "Assets/Batman" / name, "Assets/Batman/" + name)
    for name in ["Build-PlatesPackage.ps1", "Build-Plates.py", "MeshAsset.cs"]:
        target.write(root / "Tools" / name, "Tools/" + name)
    target.write(root / "Assets/Plates/README.md", "Assets/Plates/README.md")
    target.writestr("BUILD.txt", """Engine: Visual Studio 2022, CMake, Win32.
cmake -S Source/racers -B Build/racers -G "Visual Studio 17 2022" -A Win32 -DRACERS_DECOMP_ASSERT=OFF -DCMAKE_EXE_LINKER_FLAGS=/SAFESEH:NO -DCMAKE_SHARED_LINKER_FLAGS=/SAFESEH:NO
cmake --build Build/racers --config Release
Renderer: cmake -S Source/Renderer -B Build/renderer -A Win32
cmake --build Build/renderer --config Release --target RebornRenderer
Network bridge: clone https://github.com/ValveSoftware/GameNetworkingSockets into Source/GameNetworkingSockets at d534c19aa760df3fb75fd20db13ba1932b8a5463.
Use vcpkg revision 8f363bdc11359ede4bc51e5eb32a2617538049c8 with protobuf, openssl, nlohmann-json (x64-windows-static).
Configure Distribution/Online with the vcpkg toolchain, VCPKG_TARGET_TRIPLET=x64-windows-static, Visual Studio 2022 x64.
cmake --build Build/online --config Release --target EpochOnline -- /p:VCToolsVersion=14.44.35207
No original game assets are required to build the binaries. The installer reconstructs game data from the user's verified base installation.
""")
print(json.dumps(dict(package=str(archive), sha256=hashlib.sha256(archive.read_bytes()).hexdigest(), bytes=archive.stat().st_size)))

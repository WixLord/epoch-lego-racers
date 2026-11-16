# Spider-Man racer parts

Hand-authored pixel artwork based on the user's minifigure references. No image-generation output is used.

The offline renderer draws at 4× size with anti-aliasing and downsamples to the original texture dimensions. This preserves blended print edges instead of baking nearest-neighbor stair steps into the artwork. The game material/filter flags remain inherited from the original parts.

- `mask.png`: 64 × 64 mask print; the bottom-left UV color sample stays solid red for the unprinted head/neck surfaces, with a filtering margin. Eyes are enlarged by 18% from the first preview.
- `torso-front.png`: 128 × 128 front print.
- `torso-game.png`: 128 × 128 game texture, including the original arm/hand/body color sample strip.
- `torso-back.png`: unused reference artwork. The game intentionally retains a **solid red back** at the user's request.
- Matching legs use a solid blue 16 × 16 texture.

`Tools/Paint-SpiderMan.jsx` contains the editable, layered Photoshop drawing instructions. Photoshop exited before executing it locally; the PNGs were rendered from the same hand-authored polygon commands with `Tools/Export-PaintCommands.cjs` and `Tools/Render-PixelPaint.ps1`. The JSON files retain separate artwork layers.

Rebuild the appended part catalog and game archive with `Tools/Build-SpiderManParts.ps1`. It reads the current preview archive without changing it and writes the merged assets to `Assets/SpiderManBuild`, plus `LEGO.JAM` in this directory. That full archive is for local testing only; distribution must use the existing mod-delta packaging pipeline.

New zero-based catalog indices: face **30**, torso **29**, legs **20**. Existing indices and unlock flags are preserved. All six facial-expression files use the same mask. The compiled remaster increases the head catalog capacity to 32; do not install this archive with an older executable limited to 30 faces.

Local preview: `Build/SpiderManPreview/RacersReborn.exe`. This preview contains copies of the existing saves, leaving the originals untouched. Release versions: stable v0.1.7 and Online WIP online-v0.1.4. All online peers need the updated parts catalog.

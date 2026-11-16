# Batman racer parts (local preview)

Classic grey suit with a yellow oval bat emblem, yellow utility belt, black gloves and legs, and a skin-tone face with a white headband behind the cowl eye openings. The back is plain grey, following the original body mapping.

Artwork is hand-authored polygon geometry in the JSON layer files, rendered at 4x resolution and downsampled by `Tools/Render-PixelPaint.ps1`. Textures retain the original 64x64 head, 128x128 torso, and 16x16 solid legs resolutions.

The last hat option adds a 147-triangle cowl with modeled brow, nose, cheek panels and tall ears. The torso selects a static 14-triangle scalloped cape in assembled racer models, using the original torso skeleton node in menu, seated race and standing bike poses. Use it with the last head, torso and leg options. Zero-based indices: hat 36, face 31, torso 30, legs 21. Original indices remain unchanged.

Build with `Tools/Build-BatmanParts.ps1` from a Spider-Man-enabled archive without Batman already installed. The cowl script currently expects the standard `Assets/BatmanBuild` output path. Local preview is `Build/BatmanPreview/RacersReborn.exe`; its saves are copies.

Not published. Full LEGO.JAM output is local-only; distribute via the existing mod delta packaging pipeline after visual and race testing.

Latest preview: connected cowl shell, higher stern Bruce face with normal eyes behind the nose/cheek panels, and a curved symmetric classic bat emblem.

Authored cowl: BatmanCowl.fbx supplied by the user, 140 triangles. Tools/Export-BatmanCowl.py imports it through Blender, preserves split normals and restores the original game axes; cowl-mesh.json is used by the package build. Dedicated bt_cowl material is dark near-black. The caped torso is also available in the Build Driver selector. User-edited torso-front-custom.png is the authoritative front artwork.

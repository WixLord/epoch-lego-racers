# Thin plates and race geometry (local preview)

Eight new plate entries: 1x1â€“1x4 and 2x1â€“2x4, IDs 6000â€“6007, appended without changing original part IDs. One plate unit high, with top studs and bottom connections. All ten colors appear at the end of the basic/first theme. The redundant 2x1 browser choice is hidden; its model remains loadable for existing saves. Rotate 1x2 to place the same shape.

Build: Tools/Build-PlatesPackage.ps1 reads the preserved v0.1.9 archive at Build/PlatesBase-v0.1.9.JAM (or an explicit -SourceArchive) and writes Assets/Plates/LEGO.JAM. It updates both piece detail libraries, adds studs to prebuilt AI car bodies, using six-sided, 16-triangle studs matching the player stud polygon count. Stock wheel assets are preserved unchanged. The motorcycle mesh is unchanged.

Local preview: Build/PlatesPreview. Approved in builder and race playtesting; released in v0.1.10 / online-v0.1.7; performance correction in v0.1.11 / online-v0.1.8.

Stud coverage correction: sample all four rows of the original 4x4 stud atlas, covering all 20 AI car bodies (331 studs total). Original wheel hashes remain unchanged.

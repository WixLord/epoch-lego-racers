# Thin plates and race geometry (local preview)

Eight new plate entries: 1x1–1x4 and 2x1–2x4, IDs 6000–6007, appended without changing original part IDs. One plate unit high, with top studs and bottom connections. All ten colors appear at the end of the basic/first theme. The 2x1 entry is intentionally retained as the requested separate orientation of 1x2.

Build: Tools/Build-PlatesPackage.ps1 reads the preserved v0.1.9 archive at Build/PlatesBase-v0.1.9.JAM (or an explicit -SourceArchive) and writes Assets/Plates/LEGO.JAM. It updates both piece detail libraries, adds studs to prebuilt AI car bodies, using six-sided, 16-triangle studs matching the player stud polygon count. Stock wheel assets are preserved unchanged. The motorcycle mesh is unchanged.

Local preview: Build/PlatesPreview. Approved in builder and race playtesting; released in v0.1.10 / online-v0.1.7; performance correction in v0.1.11 / online-v0.1.8.

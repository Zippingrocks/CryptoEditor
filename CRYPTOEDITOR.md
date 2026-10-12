# CryptoEditor

CryptoEditor is the map-editor frontend for the **Destroy All Humans! Pandemic
Development Suite**. It starts from TrenchBroom's proven brush-editing and
viewport foundation, then adds Pandemic/DAH-native terrain, object, mission,
collision, destruction, streaming, and packaging workflows.

This repository deliberately keeps the internal upstream target and source
layout recognizable while the product is separated into CryptoEditor. That
keeps future TrenchBroom merges practical and avoids a high-risk mechanical
rename before DAH support exists.

## Upstream baseline

- Repository: <https://github.com/Zippingrocks/CryptoEditor>
- Upstream: <https://github.com/TrenchBroom/TrenchBroom>
- Baseline revision: `30dd5b6189487f666cea6f1b6502dcabd13809a5`
- Local upstream remote: `upstream`
- CryptoEditor integration branch: `main`
- License: GNU General Public License v3.0; see `LICENSE.txt`

The Windows release baseline produces `CryptoEditor.exe`. The complete
`TbMdlLib` model/editor regression suite passes 132 of 132 tests, covering the
brush, map, selection, command, persistence, material, and UV foundation.

WorldEdit is a feature reference, not a data model or source dependency. The
active Codeberg tree was inspected at
`cd171e7e206966c060e81c594740c63edcb480f4` and has no project-level license
grant, so none of its code can currently be copied, linked, or ported here.
Features such as terrain sculpting, terrain painting, large-world navigation,
and DAH-oriented scene tools must be implemented independently against the
DAH-native core rather than by routing saves through Battlefront formats.

## Architectural boundary

CryptoEditor owns interactive editing: viewport, selection, transforms,
brushes, undo/redo, inspectors, terrain tools, and visualization.

The Pandemic Development Suite owns native data semantics: lossless parsing,
resource identities, relocations, loadgroups, native meshes and both LODs,
materials, textures, unknown fields, validation, and—only after round-trip
gates pass—package writing.

The first boundary is the versioned, read-only `.dahscene` v2 directory emitted
by `dahdev`. CryptoEditor has a strict manifest loader that verifies the
schema, read-only marker, package and buffer-view hashes, path containment,
geometry references, collection counts, and Area 42's exact native geometry
counts before accepting a scene. In addition to both native terrain LODs, v2
preserves all 41 world-instance buckets and 849 placement records. It currently
resolves the placement/archetype/LOD/static-mesh chain for 17 of Area 42's 18
building placements; the guard tower's segmented visual remains represented but
is explicitly unresolved. The editor displays those buildings at their retail
transforms rather than presenting them as an unplaced model catalog.

CryptoEditor frames the combined terrain and building reference in the 3D
viewport and exposes locked LOD0 and LOD1 display layers alongside an ordinary
editor-geometry layer. The Layers
panel's eye controls switch either native LOD on or off; LOD0 is initially shown
and LOD1 is initially hidden. The renderer consumes the executable-verified UV
channel, exact segment-to-material assignments, hash-verified decoded PNG
previews, and verified per-axis wrap/clamp modes for every material used by Area
42's two terrain LODs. The pinned retail executable also proves the terrain
stream's final four bytes are `D3DCOLOR`; the viewport applies native RGB vertex
modulation while leaving alpha/blending disabled until its render-state meaning
is established. Viewport orientation, remaining render states, and the high
segment-flag byte still require matching in-game confirmation.
Native save, autosave, revert, and misleading map/OBJ exports are disabled for
this document type. A converted heightmap or Battlefront `.wld`/`.ter` file must
never become the authoritative representation of a DAH map.

The **Elements** inspector establishes the DAH-native content taxonomy. Its
**Buildings** tab separates human residential, civic/commercial,
industrial/agricultural, military/government, infrastructure, and modular
structures from Furon architecture. Neighboring World, Props, Actors, Gameplay,
and Systems tabs keep terrain, set dressing, spawns, mission logic, collision,
destruction, streaming, effects, audio, and preserved unknown records from being
flattened into one asset list. The Buildings tab now enumerates the actual Area
42 building placements, grouped by their native asset names with positions,
render status, and preserved record identity. It deliberately does not invent
assets that are absent from the retail map. Native placement editing remains
disabled until the unchanged-save gate supplies a verified writer. See
[`DAH_MAP_ELEMENTS.md`](DAH_MAP_ELEMENTS.md).

## Delivery sequence

1. Keep the upstream editor building and tested on Windows.
2. **Complete:** connect the verified `.dahscene` loader to a read-only DAH
   document-open path and render all 92 native terrain placements at both LODs.
3. **Complete for the read-only preview:** add executable-verified UVs, exact
   material batches, hash-verified decoded textures, native U/V wrap/clamp, and
   native terrain RGB vertex modulation.
4. **Complete for Area 42 buildings:** preserve all native world placements and
   render 17 verified building placements through their native resource chain.
5. **Next:** compare representative terrain against matching in-game locations,
   resolve remaining visual differences, then add terrain selection and
   inspection without changing package data.
6. Resolve segmented/destructible visuals and the other native object mesh
   families, then connect scene selection to preserved object records.
7. Add brush-to-native geometry workflows alongside native terrain editing.
8. Prove byte-preserving unchanged serialization.
9. Prove one controlled object move on a disposable game copy.
10. Expand to collision, destruction, missions, streaming, and new-map creation.

## Scene organization

CryptoEditor reuses the editor's existing flat layers for map-wide visibility,
locking, ordering, and export policy. Native DAH display data is represented by
locked display layers, while future authored brushes and objects remain in
ordinary editable layers. Nested groups provide the folder-like hierarchy for
authored objects. A separate Photoshop-style nested-layer implementation is not
needed until DAH-specific workflows demonstrate a limitation in this combined
layer-and-group model.

DAH1 US Xbox retail Area 42 is the reference target. DAH2 retail and prototype
builds receive separate adapters until evidence proves which structures are
actually shared.

## Windows baseline build

The first local baseline was configured with Visual Studio 2022's MSVC toolset,
Ninja, CMake, Qt 6.10.3 (`msvc2022_64`), and Pandoc. Keep third-party downloads
outside this repository.

From a Visual Studio developer PowerShell:

```powershell
cmake -S . -B build-cryptoeditor-ninja -G Ninja `
  -DCMAKE_BUILD_TYPE=Release `
  -DCMAKE_PREFIX_PATH=<path-to-Qt-msvc2022_64> `
  -DPANDOC_PATH=<path-to-pandoc>
cmake --build build-cryptoeditor-ninja --target TrenchBroom --parallel 8
```

The generated build directory is intentionally ignored by Git.

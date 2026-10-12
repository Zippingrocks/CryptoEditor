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

The first boundary is the versioned, read-only `.dahscene` v1 directory emitted
by `dahdev`. CryptoEditor has a strict manifest loader that verifies the
schema, read-only marker, package and buffer-view hashes, path containment,
geometry references, collection counts, and Area 42's exact native geometry
counts before accepting a scene. It opens an accepted scene as a read-only DAH
reference document, decodes the native LOD0 triangle views, applies all 92
placement matrices, frames the terrain in the 3D viewport, and retains the
ordinary empty brush layer for future authored geometry. Native save, autosave,
revert, and misleading map/OBJ exports are disabled for this document type. A
converted heightmap or Battlefront `.wld`/`.ter` file must never become the
authoritative representation of a DAH map.

## Delivery sequence

1. Keep the upstream editor building and tested on Windows.
2. **Complete:** connect the verified `.dahscene` loader to a read-only DAH
   document-open path and render all 92 native terrain placements at LOD0.
3. **In progress:** add LOD switching, candidate UVs, material batches, and
   decoded textures; report unresolved dependencies explicitly.
4. Add terrain selection and inspection without changing package data.
5. Add brush-to-native geometry workflows alongside native terrain editing.
6. Prove byte-preserving unchanged serialization.
7. Prove one controlled object move on a disposable game copy.
8. Expand to collision, destruction, missions, streaming, and new-map creation.

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

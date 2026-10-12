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

The boundary is the versioned, read-only `.dahscene` v4 directory emitted
by `dahdev`. CryptoEditor has a strict manifest loader that verifies the
schema, read-only marker, package and buffer-view hashes, path containment,
geometry references, collection counts, and Area 42's exact native geometry
counts before accepting a scene. Existing v2 and v3 directories remain readable,
but their smaller inventories are not the complete v4 reconstruction.

Area 42 v4 reads both native instance lists in all 41 world-instance buckets:
2,674 authored records, not the earlier single-list inventory of 849. It resolves
79 building and 1,468 prop placements, including intact multipart visuals. A
component retains its native mesh, materials, skeleton/bone association and local
matrix; the renderer composes that matrix with the authored world placement.
Components are not counted as additional authored entities.

The current read-only fixture contains:

| Content | Native/reference coverage |
| --- | --- |
| Terrain | 92 placements; both LODs; 184 geometries |
| World instance database | 2,674 records; 2,535 with reference visuals |
| Buildings and props | 79 buildings and 1,468 props, all with reference visuals |
| Human actors | 295 placements; three mesh LODs, native clothing variants and skeleton data |
| Static scatter | 15,121 foliage/rock instances in 654 batches using five templates |
| Landing sites | Three registrations plus the native 11-part marker visual at each authored center |
| Shared scene assets | 592 geometries, 695 materials and 331 decoded texture previews |

Vehicles, animals, gameplay-owned objects and other native actor classes remain
distinct from humans and buildings. The inspector retains original class/asset
names, list membership and mission-layer provenance. These are editor categories,
not a claim that every authored actor is active simultaneously. Reference-pose
skinned actors retain skeletons, weights and batch palettes; runtime animation
and held attachments remain unapplied.

Three landing regions are resolved through retail Lua registration calls and
native region hash lookups. A separate locked display layer shows 142 editor
annotations: those three landing regions and 139 random population positions.
Resolved entity visuals are not replaced by generic annotations. The native
landing visual and its editor annotation remain separate records; native
collision-based height projection and pulsing/rise/fall animation are not
reproduced. The interchange preserves three cumulative-probability
population definitions, four directed-spawn recipe tables, all regions/tags and
mission-layer links. Markers do not imply simultaneous runtime activation.

CryptoEditor frames the combined terrain and building reference in the 3D
viewport and exposes locked LOD0 and LOD1 display layers alongside an ordinary
editor-geometry layer. The Layers
panel's eye controls switch either native LOD on or off; LOD0 is initially shown
and LOD1 is initially hidden. The renderer consumes the executable-verified UV
channel, exact segment-to-material assignments, hash-verified decoded PNG
previews, and verified per-axis wrap/clamp modes for every material used by Area
42's two terrain LODs. The pinned retail executable also proves the terrain
stream's final four bytes are `D3DCOLOR`. The terrain path retains all RGBA
channels and the basic retail pixel shader's RGB-times-two modulation. The high
segment-flag byte supplies native layer-dependent draw state: base surfaces and
alpha-blended overlays are separate batches, with native `LESS_EQUAL` depth
testing and overlay depth writes disabled. Five native terrain materials use
ONE/ONE reverse-subtractive blending for 1,071 triangles across both LODs.
Cutout thresholds, ordinary source-alpha blending and per-axis addressing are
derived from executable evidence rather than guessed from texture appearance.

Native authoring UV matrices are preserved, but terrain initialization resets
them to identity before drawing; the viewer does not apply those matrices twice
to the compiled UVs. It also does not blindly multiply basic terrain pixels by
material opacity or diffuse-color fields that the verified shader does not use.
Texture decoding covers native BC1/BC3, swizzled A8R8G8B8, and the verified
single-level linear A8R8G8B8/A4R4G4B4 layouts. Scatter preserves per-instance RGB
and original matrices.

This is not a game-exact viewport. Native face-culling parity remains unverified
and the preview currently disables culling. Retail environment fog and lighting,
animated materials, human/vehicle animation, scatter wind/distance fade,
individual transparent-instance ordering and exceptional render paths remain
incomplete. Generic editor face shading and fog are not substitutes for the
native terrain shading path. Collision, destruction-state behavior, streaming
and gameplay activation are not simulated. Matching retail captures are still
required to assess the remaining visual differences.
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

The layer panel also exposes all 92 native mission regions as a separate,
hidden-by-default translucent overlay. These are not guessed extents: the
retail executable transforms points by each native matrix and tests all three
local coordinates inclusively against `-1` and `+1`. Spawn/landing markers,
region volumes, LOD0, and LOD1 therefore remain independently switchable.

## Delivery sequence

1. Keep the upstream editor building and tested on Windows.
2. **Complete:** connect the verified `.dahscene` loader to a read-only DAH
   document-open path and render all 92 native terrain placements at both LODs.
3. **Complete for the read-only preview:** add executable-verified UVs, exact
   material batches, hash-verified decoded textures, native U/V wrap/clamp, and
   native terrain RGBA/layer blending, basic RGB-times-two shading, cutout
   thresholds and verified shadow blend equations.
4. **Complete for the schema v4 read-only inventory:** preserve both native world
   lists, all 2,674 placement records and layer/list provenance; resolve 79
   buildings, 1,468 props, multipart reference visuals, 15,121 scatter instances,
   295 human reference meshes and three native landing visuals. Preserve
   unresolved runtime behavior explicitly rather than inventing it.
5. **Complete offline foundations:** attest the region membership routine,
   account for all 7,804 resources across 42 type families, and prove a safe
   byte-identical passthrough copy (not yet a semantic writer).
6. **Next:** compare representative terrain against matching in-game locations,
   resolve remaining visual differences, then add terrain selection and
   inspection without changing package data.
7. Verify native lighting/fog/culling, actor and landing animation, collision
   projection, destruction states and scatter dynamics, then connect scene
   selection to preserved object records.
8. Add brush-to-native geometry workflows alongside native terrain editing.
9. Prove semantic unchanged serialization while preserving unknown bytes.
10. Prove one controlled object move on a disposable game copy.
11. Expand to collision, destruction, missions, streaming, and new-map creation.

## Scene organization

### Native preview checkpoint — October 12, 2026

The Area 42 read-only preview now consumes original authored texture mipmaps
and native filtering/LOD bias. A separate DAH shader preserves terrain RGBA
through interpolation and performs native RGB2 after sampling. Rigid basic
materials use original UVs and a restricted authored-sun preview; generic editor
camera-facing shading and brightness are not substituted for native lighting.
The first native sky visual is camera-relative, retains its real mesh depth,
and never writes depth. Transparent instances are depth-sorted separately from
native terrain layers. This is not full retail draw-queue or lighting parity.

Gameplay inspection includes 264 existing pickups and 146 mission objective
definitions, grouped by mission and native objective type. Double-click placed
objects, resolved region targets or object links to navigate to their native
location. Unresolved targets have no fabricated position. Mission activation
and script execution are not simulated.

The upgraded scene retains 2,674 world records, 79 buildings, 76 selected
building fixtures, 15,121 scatter instances, and both terrain LODs. It contains
641 geometries, 792 materials and 348 referenced textures including secondary
shader dependencies. Reflection shaders 3/4 now use their decoded RGB2 base and
exact output-alpha behavior; their secondary view-dependent term is preserved
but remains disabled until generated t1 and live c5 are recovered. Retail assets
remain read-only; no AI upscaling is used.

Driver-backed regression coverage compiles/links the actual GLSL and checks
pixel output for native RGB scaling, alpha rejection, camera-relative sky and
authored lighting. This does not replace a matched in-game camera comparison.
The viewport now applies Area 42's authored fog color with the executable-proven
linear camera-forward coordinate and the retail alien camera profile's configured
0..250 endpoint. It is not generic radial editor fog. UFO profile switching,
local lights, runtime sun/tint changes, shadows, secondary reflection terms and
the second sky visual remain explicitly unresolved.

CryptoEditor reuses the editor's existing flat layers for map-wide visibility,
locking, ordering, and export policy. Native DAH display data is represented by
locked display layers, while future authored brushes and objects remain in
ordinary editable layers. Nested groups provide the folder-like hierarchy for
authored objects. A separate Photoshop-style nested-layer implementation is not
needed until DAH-specific workflows demonstrate a limitation in this combined
layer-and-group model. Native mission-layer membership and instance-list
provenance are shown independently of these editor display layers. Showing a
record in the inspector does not activate its mission layer or spawn recipe.

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

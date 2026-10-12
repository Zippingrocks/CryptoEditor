# DAH map element organization

CryptoEditor separates map content by native ownership and editing behavior. A
visual object may reference several systems, but those systems are not folded
into the object. This prevents a building mesh from being mistaken for its
collision, destruction, mission, or streaming data.

## Elements inspector

Schema v4 reads both instance lists in all 41 native world buckets, preserving
2,674 authored placements. The earlier single-list inventory of 849 records was
incomplete. Buildings now contains 79 placements and Props 1,468, with reference
visuals for all 1,547 structures/set pieces. Across native actor classes, 2,535
world records resolve to reference visuals; the remaining 139 population
positions remain annotations rather than invented human meshes.

World contains all 15,121 native scatter instances grouped by source-template
hash. Actors distinguishes 295 humans, 145 vehicle-class placements and 70 animal
placements, while retaining native classes for other actors such as powersuits.
Gameplay contains three landing registrations and the population/spawn data.
Each landing site has the native 11-part reference marker visual, separate from
its editor annotation. The locked marker layer contains 142 annotations: three
landing centers and 139 population positions. Annotations are excluded from
native triangle counts.

Each item keeps its original position, identity, native class and asset name.
List membership and mission-layer provenance are displayed independently of its
editor category. The interface presents loaded native records first; proposed
authoring categories are not evidence that a corresponding asset exists in Area
42. Mission activation is not evaluated, and authored records may belong to
mutually exclusive gameplay states.

The **Elements** inspector contains six tabs:

| Tab | Owns | Does not own |
| --- | --- | --- |
| **Buildings** | Human and Furon structure assets, modular construction pieces, native placements, mesh LOD relationships | Collision, destruction graphs, mission triggers |
| **World** | Terrain, native foliage/rock scatter, water, atmosphere, lighting, bounds, streaming partitions | Buildings and actor-based set dressing |
| **Props** | Human props, natural dressing, pickups, and interactable objects | Building shells and actors |
| **Actors** | Human characters, animals, vehicles, powersuits and other native actor classes, initial placements, and spawnable asset identities | Mission behavior and navigation graphs |
| **Gameplay** | Player starts, spawns, encounters, missions, objectives, triggers, regions, paths, and navigation | Render meshes and package layout |
| **Systems** | Collision, destruction, effects, audio, cameras, resource identities, relocations, load groups, package ownership, and unknown records | Authored visual category membership |

## Buildings

Human structures are the first-class contents of the Buildings tab. They are
organized as:

- Residential
- Civic and commercial
- Industrial and agricultural
- Military and government
- Infrastructure
- Modular construction

Furon architecture remains in a separate collection so searches and future
placement rules cannot mix it with human structures.

A building asset will eventually bind several independent records:

```text
building placement
  -> native mesh identity and mesh LODs
  -> material and texture dependencies
  -> collision resource
  -> destruction stages and debris set
  -> optional interior modules and portals
  -> optional gameplay references
  -> load group and streaming ownership
```

For Area 42, the Buildings tab is now populated from the retail
`world.instance-database`, not from a speculative catalog. It exposes all 79
building placements reached through both native instance lists. Placements
resolve through actor archetype, visual LOD, mesh, material and texture records.
Intact segmented structures retain all resolved components and their native
reference-pose transforms instead of displaying only the first component.
Component geometry and materials can be shared; one multipart building remains
one authored placement, not a collection of new map entities. Native destruction
states and collision behavior are not yet simulated.

Placement editing stays disabled until unchanged native serialization proves
that record identity, package ownership, linked collision/destruction data, and
unknown fields survive a no-op round trip.

## Rendering and provenance

The read-only scene preserves source identities, relocations, raw placement
headers, bucket/list offsets, mission-layer links, material variants and skeleton
associations. Editor organization does not rewrite any of these native records.
Terrain remains the native mesh with both LODs, never an authoritative heightmap
conversion. Native static scatter is a distinct instancing system, not an
actor-placement list and not a guessed foliage asset catalog.

Terrain rendering preserves native vertex RGBA, the verified basic pixel shader's
RGB-times-two modulation, high-byte segment layering, alpha-test thresholds,
LESS_EQUAL depth behavior and ordinary/subtractive blend equations. Original UVs
remain authoritative; preserved authoring matrices are not reapplied after the
native terrain-load reset. These verified states improve reconstruction without
establishing pixel-exact parity.

Face-culling parity, retail fog and lighting, animated materials, actor animation,
scatter dynamics, collision projection and gameplay activation remain unverified
or unapplied. Landing visuals use authored centers pending native collision-query
reproduction. Collision, destruction, streaming and mission execution remain
separate systems, not inferred from a visible mesh. The viewport is neither a
game-exact simulation nor proof that the map is natively editable.

## Authoring rule

Layers control visibility and editing workflow. Element tabs classify what a
record is. Groups may form an authored hierarchy. These concepts remain
separate: changing a layer must not change a native resource's category,
identity, package ownership, or dependencies.

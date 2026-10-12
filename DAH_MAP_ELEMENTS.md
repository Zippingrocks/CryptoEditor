# DAH map element organization

CryptoEditor separates map content by native ownership and editing behavior. A
visual object may reference several systems, but those systems are not folded
into the object. This prevents a building mesh from being mistaken for its
collision, destruction, mission, or streaming data.

## Elements inspector

The **Elements** inspector contains six tabs:

| Tab | Owns | Does not own |
| --- | --- | --- |
| **Buildings** | Human and Furon structure assets, modular construction pieces, native placements, mesh LOD relationships | Collision, destruction graphs, mission triggers |
| **World** | Terrain, water, atmosphere, lighting, bounds, streaming partitions | Buildings and set dressing |
| **Props** | Human props, natural dressing, pickups, and interactable objects | Building shells and actors |
| **Actors** | Characters, creatures, vehicles, initial placements, and spawnable asset identities | Mission behavior and navigation graphs |
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
`world.instance-database`, not from a speculative catalog. It exposes 18 native
placements grouped under the eight asset identities present in the map. Seventeen
placements currently resolve through actor archetype, visual LOD, static mesh,
material, and texture records and appear in the reference viewport. The guard
tower placement and archetype are preserved, but its segmented/destructible
material indirection is still reported as unresolved.

Placement editing stays disabled until unchanged native serialization proves
that record identity, package ownership, linked collision/destruction data, and
unknown fields survive a no-op round trip.

## Authoring rule

Layers control visibility and editing workflow. Element tabs classify what a
record is. Groups may form an authored hierarchy. These concepts remain
separate: changing a layer must not change a native resource's category,
identity, package ownership, or dependencies.

# Terrain Generation Transition Plan

This note exists so terrain work can resume cleanly after a broken session.

## Current Direction

1. `CelestialBodyStaticMesh` remains the source asset. For the current planet test this is `CelestialBodyMesh2`, a cube mesh subdivided in Blender with Catmull-Clark.
2. Runtime terrain is generated into `CelestialBodyDynamicMesh`.
3. Planet and moon terrain no longer edits shared source vertices along per-vertex normals.
4. The static mesh triangles are paired back into their original quad cells where possible.
5. Each recovered quad cell receives one terrain sample and one discrete height level.
6. The four corners of that quad use the same height offset, and the two UE triangles share one flat normal/color.
7. Surface grid rendering should use quad cell edges, not arbitrary low-resolution cube-sphere cells and not triangle diagonals.

## Implementation Order

1. Keep the source static mesh import path unchanged.
2. In `ASRCelestialBody::CopySourceStaticMeshToCelestialBodyDynamicMesh`, read LOD0 render vertices and triangle indices.
3. Recover quad cells by pairing adjacent triangles across the likely triangulation diagonal.
4. Generate a flat Dynamic Mesh from duplicated quad vertices so normals are per-cell, not smoothed across neighbors.
5. Derive cell height from the existing multi-noise terrain sample, then quantize it into Minecraft-like height levels.
6. Preserve biome vertex color so the planet material/texture coloring still works.
7. Make grid wire rendering prefer quad boundaries. If quad metadata is not available yet, owner dynamic mesh wire is only a visual fallback.

## Known Follow-Ups

1. Add explicit side-wall geometry if large neighboring height steps expose visible cracks.
2. Store recovered quad cell data on the body/grid instead of recomputing it.
3. Use quad cells for assembly placement/raycasting, not only for wire display.

# Pattern Content Catalog

## Scope

This catalog replaces the numbered Processor/Synthesizer assets and the Territe/Nitain/Aquid/Waste resource set. It is intentionally small: every asset teaches one rule, occupies a clear place in an automation line, and uses bounded first-pass values that can be tuned after playtesting.

## Resources

All mined resources begin with four occupied cells (16% board density). Their two-glyph pairings form a closed ring, so every playable glyph appears in exactly two source families.

| Resource DA | Resource ID | Source glyphs | Seed salt | Primary source |
|---|---|---:|---:|---|
| `DA_Resource_StarIron` | `StarIron` | Metal 2, Crystal 2 | 101 | Gravemantle, Ironwake |
| `DA_Resource_BloomSap` | `BloomSap` | Organic 2, Fluid 2 | 211 | Verdant Cradle |
| `DA_Resource_PrismShard` | `PrismShard` | Crystal 2, Plasma 2 | 307 | Cinderstream |
| `DA_Resource_TidalOre` | `TidalOre` | Fluid 2, Metal 2 | 401 | Gravemantle, Ironwake |
| `DA_Resource_SolarMycelium` | `SolarMycelium` | Plasma 2, Organic 2 | 503 | Cinderstream, Verdant Cradle |
| `DA_Resource_StellarWeave` | `StellarWeave` | one of each glyph for preview only | 607 | Stellar Loom synthesis output |

`StellarWeave` does not replace its synthesized cells with the preview layout. The Loom assigns only the resource identity; the output Pattern is still resolved from both input boards.

## Environments

Environment rules live in dedicated Data Assets. Planet and moon Data Assets reference one environment instead of embedding an anonymous rule.

| Environment DA | Environment ID | Rule | Intensity | Line-design effect |
|---|---|---|---:|---|
| `DA_Environment_CrushingGravity` | `CrushingGravity` | Metal pulls Down | 1 | Compacts the world's guaranteed Metal sources after every facility operation |
| `DA_Environment_PlasmaJetstream` | `PlasmaJetstream` | Plasma continuously drifts Right | 2 | Makes Cinderstream the fast Plasma-alignment world but risks edge loss |
| `DA_Environment_SporeBloom` | `SporeBloom` | Organic grows once per component | 1 | Produces bonus-shape material while increasing collision pressure |
| `DA_Environment_MagneticShear` | `MagneticShear` | Metal pulls Left | 2 | Pre-aligns Metal cargo before inter-body export |

The initial bodies are named Gravemantle, Cinderstream, Verdant Cradle, and Ironwake so their asset and display names communicate the attached environment.

Every generated run includes each configured planet type once before duplicates are allowed. Each body's enabled deposits include its environment's affected glyph plus one or two secondary glyphs, and every enabled deposit family receives at least one dry-land placement whenever the body has a valid dry cell. Moons participate in the same placement pass.

## Celestial Bodies

All starter bodies use the baked `DA_PlanetShape_Cube64` base and `DA_Profile_Earth` terrain profile so an ordinary Play session can build every generated mesh promptly. Cube256 remains available for a later quality tier.

| Body DA | Role | Scale | Mass | Gravity | Surface and volume materials | Environment |
|---|---|---:|---:|---:|---|---|
| `DA_Planet_Gravemantle` | Metal/Fluid world | 22 | 340 | 1.80 | `M_BadLands1`; no ocean or atmosphere | Crushing Gravity |
| `DA_Planet_Cinderstream` | Crystal/Plasma world | 20 | 220 | 1.10 | `M_Ground`, `M_LavaOcean_Ocean`, temperate atmosphere | Plasma Jetstream |
| `DA_Planet_VerdantCradle` | Organic/Fluid world | 20 | 180 | 0.90 | `M_Planet`, `M_Temperate_Water2`, temperate atmosphere | Spore Bloom |
| `DA_Moon_Ironwake` | Metal/Crystal/Fluid moon | 6 | 45 | 0.45 | `M_BadLands1`; no ocean or atmosphere | Magnetic Shear |

## Facilities

Inventory capacity is per port. Small capacities make belt balancing and back-pressure visible without requiring large buffers.

| Facility DA | Operation | Ports | Capacity | Process time | Technology | Purpose |
|---|---|---:|---:|---:|---|---|
| `DA_Facility_PatternExtractor` | Mine | 0 → 1 | 8 | 2.5 s | Foundation | Copies the mining point's fixed Pattern |
| `DA_Facility_VectorShifterEast` | Transform all cells East | 1 → 1 | 8 | 2.0 s | Foundation | Coarse horizontal alignment |
| `DA_Facility_VectorShifterWest` | Transform all cells West | 1 → 1 | 8 | 2.0 s | Foundation | Coarse horizontal correction |
| `DA_Facility_VectorShifterNorth` | Transform all cells North | 1 → 1 | 8 | 2.0 s | Foundation | Coarse vertical correction |
| `DA_Facility_VectorShifterSouth` | Transform all cells South | 1 → 1 | 8 | 2.0 s | Foundation | Coarse vertical alignment |
| `DA_Facility_CenterlineShifter` | Move center row East | 1 → 1 | 6 | 1.5 s | Expansion | Fine adjustment without moving the whole board |
| `DA_Facility_StellarLoom` | Synthesize | 2 → 1 | 4 | 5.0 s | Foundation | Combines Star Iron and Bloom Sap into Stellar Weave |
| `DA_Facility_LatticeSeparator` | Separate alternating columns | 1 → 2 | 6 | 3.5 s | Expansion | Recovers two non-duplicating sub-Patterns for branch lines |
| `DA_Facility_PatternHub` | Hub | 20 ↔ 20 | 16 | n/a | Foundation | Pattern-filtered surface and inter-body distribution |

The four Vector Shifters are deliberately symmetric. Their output is not symmetric because Metal, Organic, Crystal, Fluid, and Plasma still resolve movement and collision through their individual rules.

## First Automation Spine

1. Gravemantle exports Metal-bearing Star Iron after Crushing Gravity or Vector Shifter alignment.
2. Verdant Cradle exports Organic-bearing Bloom Sap; Spore Bloom can grow additional bonus-shape cells.
3. A Pattern Hub brings the two cargo families to one body.
4. Stellar Loom synthesizes them into Stellar Weave.
5. Vector and Centerline Shifters align the contract cells; the Lattice Separator is an optional recovery/branch tool.
6. The star accepts one of 16 two-cell contracts: eight Metal/Organic directions, four Metal/Plasma directions, or four edge-oriented Crystal/Fluid shapes. The selected glyph pair cannot be completed from one body's source pool, so the accepted Run target necessarily includes inter-body transport and synthesis.

## Initial Numerical Baseline

- Source density: 4/25 occupied cells per mined Pattern.
- Basic transform throughput: 0.5 Pattern/s per machine before modifiers.
- Synthesis throughput: 0.2 Pattern/s and two input streams, making it the intended first bottleneck.
- Contract base score: 12 per accepted Pattern; initial requirement 48 and +3 per Cycle, equivalent to four clean Patterns in Cycle 0 before bonuses.
- Stellar health: 1000 maximum/initial, 0.25 health lost per simulated second in Period 0, and a 1.05x compounded decrease-rate multiplier per Period. Each accepted Pattern score restores 1 health. First depletion refills the Red Giant reserve; second depletion causes Supernova Game Over.
- Deposit spawn baseline: 5-10 deposits per enabled resource family, 2-3 cells minimum spacing, and 1.5-3.5% chance per candidate cell depending on the body.

These values are balance anchors, not hidden formulas. Facility times, capacities, source density, spawn supply, contract demand, and transport travel time remain independently measurable.

## Runtime Entry Wiring

- The `BP_SolarSystemGenerator` class defaults and the actor serialized in `/Game/Levels/SolarSystem` both own the Pattern Generation Profile and the complete star/planet/moon DA arrays.
- Both generator entry points create a fresh runtime seed for an ordinary Run. Explicit `GenerateRuntimeSystemWithSeed` calls remain reproducible for saves, tests, and diagnostics.
- Begin Play validates those references, including shape base, material, terrain, ocean, atmosphere, environment, facility, and contract dependencies. An incomplete map actor restores the complete Blueprint class-default set before generation.
- `BP_SRPlayerController` loads the nine Pattern structure DAs plus the conveyor as its construction catalog. Technology, Augment, and Trial DAs load from `USRSimulationSettings`.
- The generation profile contains 16 initial-contract candidates and guarantees the selected candidate with the five Foundation line operators: four Vector Shifters and Stellar Loom. Centerline Shifter and Lattice Separator remain loaded in the construction catalog and unlock through Expansion, but are not assumed by the starter solvability proof.
- The validator samples one deposit Pattern per resource family per body. This is an existential proof optimization only; every generated deposit remains present and mineable in the world.
- The content commandlet writes and validates all of these references on the Blueprint defaults and the actual map actor. A read-only run reports zero creates, renames, changes, and validation errors when the project is synchronized.

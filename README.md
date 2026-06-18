# Star Rovers

## 1. Overview

Star Rovers is an automation roguelike game.

The player must keep the parent star in its main-sequence state by supplying at least the required amount of stellar fuel every cycle. If the parent star runs short on fuel, it expands toward a red giant state. The player builds stellar fuel production automation infrastructure on child planets and moons, then builds space-side supply infrastructure to keep stellar fuel production and delivery running.

The main gameplay is not simple mining and processing automation. The core fun comes from maximizing automation efficiency while adapting to randomly offered `Augments`, `Technologies`, and `Trials` during a run. These variables can help the player, disrupt existing automation, or change the design constraints of the automation network.

## 2. Working Rules

Codex must use this document to understand the project's feature structure and implementation structure before working.

Workflow:

- First read current structure section to understand the project's feature structure and implementation structure.
- Once the target feature is known, directly read the related C++ files and understand them at code level before editing.
- Do not guess. Decisions must be grounded in actual code, README.md, or confirmed asset state.
- If the required evidence cannot be found, ask the user.

Code work:

- After modifying C++ files, always run a build and confirm there are no compile errors.
- Do not implement everything in C++. Split responsibilities across C++ classes, Blueprint classes, and Data Assets using the criteria below.
- Do not edit `.uasset` or `.umap` files as text. For BP/DA work that cannot be done in code, explain the required editor steps to the user in detail.
- Only update README.md when the user explicitly asks for it.

C++ / BP / DA criteria:

- C++: runtime source of truth, performance-sensitive logic, algorithms, state management, validation, save/load data structures.
- Blueprint: Actor/Component assembly, visual setup, editor placement, simple event wiring, asset reference setup.
- Data Asset: data-driven configuration such as numbers, rules, recipes, biomes, facilities, structures, and resources.
- UI Widget: display runtime state or dispatch user requests. Widgets are not gameplay source of truth.

## 3. Current Structure

This section is a compact implementation map for Codex. Before editing, read the target classes directly.

Legend:

- `[O]`: runtime state/behavior owner
- `[D]`: Data Asset or config
- `[H]`: helper / query / stateless logic
- `[UI]`: widget or UI event surface

Core flow:

`ASRSolarSystemGenerator`
-> `ASRCelestialBody` / `ASRPlanet` / `ASRStar`
-> Dynamic Mesh / `USRPlanetSurfaceGrid`
-> `USRAssemblyComponent`
-> `USRStructureInstanceManagerComponent` / `USRConveyorNetworkComponent` / `USRFacilityNetworkComponent`

Feature owners:

- Game / Player / Camera: [O] `ASRGameMode`, `ASRPlayerController`, `ASRCameraPawn`
- Runtime System Generation: [O] `ASRSolarSystemGenerator`, `USRCelestialBodyRegistrySubsystem`
- Celestial Runtime: [O] `ASRCelestialBody`, `ASRPlanet`, `ASRStar`; [H] `USRCelestialBodyRuntimeLibrary`
- Celestial Data: [D] `USRStarDataAsset`, `USRPlanetDataAsset`, `USRMoonDataAsset`, `USRDynamicMeshBaseDataAsset`
- Orbit / Time / Gravity: [O] `USROrbit`, `USRTimeControlSubsystem`, `USRGravityParent`, `USRGravityChild`
- Terrain / Biome: [D] `FSRDynamicMeshGeneration`, `USRPlanetBiomeDataAsset`, `USRPlanetTerrainProfileDataAsset`; [H] `FSRPlanetTerrainGenerator`
- Surface Grid: [O] `USRPlanetSurfaceGrid`; [H] `USRPlanetSurfaceGridLibrary`
- Assembly / Structure: [O] `USRAssemblyComponent`, `USRStructureInstanceManagerComponent`, `ASRStructure`; [D] `USRStructureDataAsset`; [H] `USRStructurePlacementLibrary`; interface `ISRBuildableStructureInterface`
- Conveyor: [O] `USRConveyorNetworkComponent`, `ASRConveyorBeltActor`
- Resource / Facility Automation: [O] `USRFacilityNetworkComponent`; [D] `USRResourceDataAsset`, `USRFacilityDataAsset`
- Natural Structures: [O] `ASRSolarSystemGenerator` with `USRStructureInstanceManagerComponent`; [D] `FSRProfileNaturalStructureSpawnRule`, `FSRNaturalStructureSpawnRuleOverride`
- UI: [UI] `USRCelestialBodyFocusInfoWidget`, `USRCelestialBodyOverviewWidget`, `USRStructureSelectionWidget`, `USRLoadingScreenWidget`, `USRTimeControlWidget`
- Diagnostics / Visual Utilities: [H] `FSRLineThicknessUtils`, `FSRMemoryDiagnostics`, `FSRTimingLog`

C++ implementation structure:

- Major runtime owners are intentionally split across partial `.cpp` files by responsibility. Read the owner header first, then use `rg --files` or symbol search to open the relevant private implementation file.
- Common split names are descriptive: `Input`, `Selection`, `UI`, `Focus`, `Surface`, `Placement`, `Deletion`, `Visuals`, `Path`, `Transport`, `PCG`, `Diagnostics`, `DynamicMesh`, `Biome`, `Noise`, and `ActorGroups`.
- Private `*Internal.h` files are implementation-only helper declarations for large algorithms such as dynamic mesh generation, terrain generation, and solar system generation.
- Widgets remain UI surfaces. Even when split across files, gameplay state still belongs to controller/component/subsystem owners.

Runtime ownership contract:

- Data Assets are config input. Runtime state must not be stored only in DA.
- Runtime celestial actors are spawned, tracked, and cleared by `ASRSolarSystemGenerator`.
- Active celestial body list and primary star are mirrored by `USRCelestialBodyRegistrySubsystem`.
- `ASRCelestialBody` owns common body runtime data, Static/Dynamic Mesh switching, Dynamic Mesh build/cache, material application, gravity source setup, and surface cell highlight data.
- `ASRPlanet` owns planet/moon-specific component composition: orbit, ocean, atmosphere, surface grid, conveyor network, structure instance manager, facility network, and rotation axis.
- Dynamic Mesh build/cache belongs to `ASRCelestialBody`.
- Surface cells, cell index maps, raycast buckets, hover/select, and occupancy belong to `USRPlanetSurfaceGrid`.
- Cell occupancy must go through `USRPlanetSurfaceGrid::SetCellOccupied` or `SetCellsOccupied`.
- Structure placement flow belongs to `USRAssemblyComponent`.
- Permanent placed structure truth belongs to `USRStructureInstanceManagerComponent`; key is `OccupantId`.
- Normal placed structures are primarily HISM instances, not actor-per-structure.
- Conveyor graph truth belongs to `USRConveyorNetworkComponent`; lane key is `CellId + Layer`.
- `ASRConveyorBeltActor` is visual/PCG output, not conveyor graph source of truth.
- Facility runtime truth belongs to `USRFacilityNetworkComponent`; facility instances are keyed by structure `OccupantId`.
- Widgets mirror state or dispatch user requests. Gameplay state belongs to Controller, Components, Subsystems, or runtime owner classes.

Current C++ / BP / DA connection:

- `BP_SolarSystemGenerator` uses `ASRSolarSystemGenerator`.
- `BP_CelestialBody`, `BP_Star`, and `BP_Planet` are Blueprint surfaces for C++ celestial actors.
- `BP_Planet` is used for both planets and moons. The distinction comes from Data Asset `BodyCategory`.
- Star/Planet/Moon Data Assets build `FSRCelestialBodyData`, then runtime actors copy that data through `SetData` / `ApplyData`.
- Planet/Moon DA references `TerrainProfileDataAsset`; the profile applies the allowed Biome DA list into `FSRDynamicMeshGeneration`.
- Profile/Biome/Material mapping is normalized through `FSRDynamicMeshGeneration::NormalizeBiomeMaterials`.
- Structure selection uses `USRStructureDataAsset` entries registered on `ASRPlayerController`.
- Conveyor is also a `USRStructureDataAsset` with `BuildKind == Conveyor`.
- Facility behavior is connected through optional `USRStructureDataAsset::FacilityDataAsset`.
- Resource and Facility runtime values use `FSRResourceInstance` and `FSRFacilityInstance`, not the DA itself.

Hot paths / optimization targets:

- Dynamic Mesh preparation and generated grid build.
- Surface Grid raycast, hover, highlight, and interaction overlay.
- Structure placement/removal with HISM visual groups and Surface Grid occupancy.
- Conveyor pathfinding, dirty-group visual refresh, PCG refresh, and item transfer tick.
- Facility processing tick and conveyor input/output transfer.
- Camera Dynamic Mesh visibility switching and screen-space line thickness.

## 4. Game Structure

This section describes the intended completed game from the player's point of view.

### 4.1 Random Star System Generation

At the start of a run, the game generates a star system.

- Parent star
- Child planets
- Moons
- Orbits and gravity zones
- Terrain Profiles, Biomes, and natural structures for planets/moons
- Strategic resource distribution for stellar fuel production

Celestial bodies provide different automation locations:

- Star: the stellar fuel delivery target and the center of run pressure.
- Planet: the main automation infrastructure location.
- Moon: a secondary automation location for auxiliary resources, special environments, or extra constraints.

Planet/moon terrain is defined by Terrain Profiles and Biomes.

- Profile: high-level terrain theme and allowed Biome list for a planet/moon.
- Biome: Surface Cell-level terrain/environment condition.
- Biomes are placed using terrain/climate metrics such as height, continentalness, coast, ocean depth, temperature, moisture, latitude, river/lake mask, and mountain mask.

### 4.2 Celestial Surface Automation Infrastructure

The player builds automation infrastructure on planet/moon surfaces.

- Surface Grid-based construction
- Resource extraction facilities
- Processing, synthesis, and splitting facilities
- Surface logistics through Conveyor paths
- Storage, splitter, merger, and filter-style logistics control
- Placement optimization based on terrain, Biome, and resource conditions
- Efficiency changes from temperature, process tags, and resource properties

The core surface automation model is cell occupancy and item flow.

- Facilities occupy Surface Grid cells.
- Conveyors build paths using cell and layer data.
- Facility input/output connects to conveyor flow.
- The player combines limited space and resource flow to raise stellar fuel production.

### 4.3 Stellar Fuel Production

Stellar fuel is produced through multiple resource and processing stages.

- Basic resource extraction
- Intermediate resource processing
- Catalyst resource usage
- Stellar fuel recipes
- Production changes from facilities, environment, and Augments
- Production management to satisfy each stellar fuel cycle requirement

Resources are broadly split into:

- Energy Resource: contributes the main energy value for processing or stellar fuel production.
- Catalyst Resource: changes, amplifies, or transforms processing results.

Resources can gain process tags:

- Responsive
- Waste
- HalfLife
- Volatile
- Singularity

These tags affect facility effects, process limits, energy values, byproducts, and risks.

### 4.4 Spaceship Routes / Interplanetary Logistics

Resources and stellar fuel produced on planets/moons are moved through spaceship routes.

- Routes between celestial bodies
- Stellar fuel delivery to the parent star
- Intermediate resource exchange between planets
- Throughput, travel time, and storage management
- Logistics efficiency affected by orbit, gravity zones, and distance
- Route timing optimized against stellar fuel cycle timing

Spaceship routes are the higher-level logistics layer connecting surface automation to star maintenance.

### 4.5 Star Maintenance / Red Giant Pressure

The parent star demands stellar fuel every cycle.

- Per-cycle stellar fuel requirement
- Expansion pressure increase when supply is insufficient
- Main-sequence maintenance when supply meets or exceeds demand
- Requirement escalation as the run progresses
- Failure or crisis states

The player's goal is to scale automation and space logistics fast enough to keep up with rising demand.

### 4.6 Augment System

Augments are a core automation roguelike variable system.

- Positive effects for production, logistics, facilities, or resource flow
- Constraint effects that disrupt facilities or automation routes
- Run-specific choices
- Conditions that change automation design priorities
- Optimization around both positive and negative effects

Augments are not simple bonuses. They are variables that change what kind of automation design is efficient.

### 4.7 Technology System

Technologies expand the player's automation options during a run or through long-term progression.

- Unlock new facilities
- Unlock new resource processing methods
- Expand logistics features such as conveyors, storage, and spaceship routes
- Expand stellar fuel recipes
- Provide special automation strategies that combine with Augments

Technologies reinforce the automation direction chosen by the player.

### 4.8 Trial System

Trials pressure the player's automation infrastructure during a run.

- Reduced production for specific resources
- Reduced efficiency for specific facilities
- Logistics bottlenecks
- Worse planetary/moon environmental conditions
- Changes to stellar fuel requirements or supply cycles
- Risky conditions that can combine with Augments for higher reward

Trials force the player to modify existing infrastructure or discover new automation solutions.

## 5. Current Development Status

### 5.1 Development Plan

1. Random Star System Generation
2. Celestial Surface Automation Infrastructure
3. Stellar Fuel Supply Automation
4. Spaceship Routes / Interplanetary Logistics
5. Augment System
6. Technology System
7. Trial System
8. Roguelike Run Progression

### 5.2 Current Status

Current active work:

- Define the actual facility infrastructure gameplay.
- Improve Resource / Facility / Structure DA values.
- Verify gameplay flow between Conveyor and Facility input/output.
- Complete the basic extraction / processing / transport loop.
- Improve Surface Grid-based structure placement UX and performance.
- Clarify Natural Structure and normal Structure placement flow.

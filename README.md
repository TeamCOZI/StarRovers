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
- Unreal unity builds can include multiple split `.cpp` files in one generated translation unit. Do not duplicate same-named anonymous-namespace helpers, constants, or `DEFINE_LOG_CATEGORY_STATIC` entries across split `.cpp` files for the same owner. Put shared private helpers in a `Private/*Internal.h` header as `inline` functions inside a named `StarRovers::<Feature>` namespace, and use `DECLARE_LOG_CATEGORY_EXTERN` plus one `DEFINE_LOG_CATEGORY` for log categories shared across split files.
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
-> `USRSpaceLogisticsSubsystem` / `ASRSpaceshipActor`

Feature owners:

- Game / Player / Camera: [O] `ASRGameMode`; [O] `ASRPlayerController` for input, UI creation/visibility, selected build option state, and assembly coordination; [O] `ASRCameraPawn` for camera movement, focus, and focused-surface view control; small runtime/UI state lives in `FSRPlayerControllerRuntimeState`, `ESRPlayerUiLayer`, and `FSRCameraPawnRuntimeState`
- Runtime System Generation: [O] `ASRSolarSystemGenerator`, `USRCelestialBodyRegistrySubsystem`
- Celestial Runtime: [O] `ASRCelestialBody`, `ASRPlanet`, `ASRStar`; [H] `USRCelestialBodyRuntimeLibrary`; runtime/data helper types live in `FSRCelestialBodyDynamicMeshRuntimeState` and `FSRCelestialBodyData`
- Celestial Data: [D] `USRStarDataAsset`, `USRPlanetDataAsset`, `USRMoonDataAsset`, `USRDynamicMeshBaseDataAsset`
- Orbit / Time / Gravity: [O] `USROrbit`, `USRTimeControlSubsystem`, `USRGravityParent`, `USRGravityChild`; [D] `USRSimulationSettings`
- Terrain / Biome: [D] `FSRDynamicMeshGeneration`, `USRPlanetBiomeDataAsset`, `USRPlanetTerrainProfileDataAsset`; [H] `FSRPlanetTerrainGenerator`
- Surface Grid: [O] `USRPlanetSurfaceGrid`; [H] `USRPlanetSurfaceGridLibrary`; runtime indexing/raycast/batch state lives in `FSRPlanetSurfaceGrid*State` helper structs
- Assembly / Structure: [O] `USRAssemblyComponent` for placement/editing workflow, area selection/copy, placement queue, and placement history; [O] `USRStructureInstanceManagerComponent` for placed structure truth, `ASRStructure`; [D] `USRStructureDataAsset`; [H] `USRStructurePlacementLibrary`; runtime structs `FSRPlacedStructureInstance`, `FSRResourceDepositInstance`; interface `ISRBuildableStructureInterface`
- Conveyor: [O] `USRConveyorNetworkComponent` for graph/path/transport truth, `ASRConveyorBeltActor` for visual output; runtime structs `FSRConveyorLaneKey`, `FSRConveyorSegment`, `FSRConveyorVisualPath`, `FSRConveyorItem`, with transport/cache state grouped in `FSRConveyorTransportRuntimeState`
- Resource / Facility Automation: [O] `USRFacilityNetworkComponent`; [D] `USRResourceDataAsset`, `USRFacilityDataAsset`; runtime structs `FSRResourceInstance`, `FSRFacilityInstance`, `FSRFacilityPortInventory`, with facility network state grouped in `FSRFacilityNetworkRuntimeState`
- Space Logistics: [O] `USRSpaceLogisticsSubsystem`, `ASRSpaceshipActor`; runtime structs `FSRHubEndpoint`, `FSRHubRoute`, `FSRSpaceLogisticsSaveData`
- Natural Structures: [O] `ASRSolarSystemGenerator` with `USRStructureInstanceManagerComponent`; [D] `FSRProfileNaturalStructureSpawnRule`, `FSRNaturalStructureSpawnRuleOverride`
- UI: [UI] `USRCelestialBodyFocusInfoWidget`, `USRCelestialBodyOverviewWidget`, `USRTimeControlWidget`, `USRStructureSelectionWidget`, `USRFacilityControlWidget`, `USRLoadingScreenWidget`; widgets mirror state or dispatch requests, and are created/refreshed by `ASRPlayerController`
- Diagnostics / Visual / Editor Utilities: [H] `FSRLineThicknessUtils`, `FSRMemoryDiagnostics`, `FSRTimingLog`, `FSRVisualPerformanceSettings`; editor-only helpers live under `Private/Editor`

C++ implementation structure:

- Major runtime owners are intentionally split across partial `.cpp` files by responsibility. Read the owner header first, then use `rg --files` or symbol search to open the relevant private implementation file.
- Large owner headers may also delegate runtime state and shared data structs to small public helper headers named `*RuntimeState`, `*RuntimeTypes`, `*DataTypes`, or feature-specific helpers such as assembly history/selection/copy/queue.
- Current source roots include `Assembly`, `Automation`, `Camera`, `Celestial`, `Conveyor`, `Gravity`, `Logistics`, `Simulation`, `Structure`, `Surface`, `UI`, `Utility`, and `Visual`.
- Common split names are descriptive: `Input`, `Selection`, `UI`, `Focus`, `Surface`, `SurfaceInteraction`, `Placement`, `Deletion`, `AreaSelection`, `AreaCopy`, `History`, `Visuals`, `Path`, `Transport`, `PCG`, `Diagnostics`, `DynamicMesh`, `Biome`, `Noise`, and `ActorGroups`.
- Private `*Internal.h` files are implementation-only helper declarations for large algorithms such as dynamic mesh generation, terrain generation, and solar system generation.
- Widgets remain UI surfaces. Even when split across files, gameplay state still belongs to controller/component/subsystem owners.

Asset-side structure:

- Hub/spaceship additions are under `Content/BlueprintClasses/CelestialBody`, `Content/Objects/Structure`, and `Content/Materials`.

Runtime ownership contract:

- Data Assets are config input. Runtime state must not be stored only in DA.
- `USRStructureDataAsset`, `USRFacilityDataAsset`, and `USRResourceDataAsset` own config only; runtime values use structs such as `FSRPlacedStructureInstance`, `FSRFacilityInstance`, and `FSRResourceInstance`.
- Runtime celestial spawning/tracking belongs to `ASRSolarSystemGenerator` and `USRCelestialBodyRegistrySubsystem`.
- Celestial body runtime, mesh build/cache, material setup, and gravity setup belong to `ASRCelestialBody`; planet/moon component composition belongs to `ASRPlanet`.
- Camera movement, focus tracking, and focused-surface view state belong to `ASRCameraPawn`; `ASRPlayerController` requests focus and owns input/UI coordination.
- Extracted runtime state/helper structs are organizational only; the owning Actor/Component remains the runtime source of truth.
- Surface cells, cell maps, raycast, hover/select, overlays, port highlights, and occupancy belong to `USRPlanetSurfaceGrid`.
- Cell occupancy must go through `USRPlanetSurfaceGrid::SetCellOccupied` or `SetCellsOccupied`.
- Assembly input, selected build option, automatic Assembly Mode activation, and UI coordination belong to `ASRPlayerController`.
- Assembly editing workflows, placement previews, and per-surface undo/redo history belong to `USRAssemblyComponent`.
- Structure placement/removal truth belongs to `USRStructureInstanceManagerComponent`; placed structures are keyed by `OccupantId` and are primarily HISM instances.
- Space logistics runtime state belongs to `USRSpaceLogisticsSubsystem`; spaceship actor state belongs to `ASRSpaceshipActor`.
- Conveyor graph, visual paths, debug lines, PCG refresh, branch/merge state, and item transport belong to `USRConveyorNetworkComponent`.
- Conveyor pathfinding uses Surface Grid cell/layer state and treats non-endpoint conveyors as blocking placement/path cells.
- `ASRConveyorBeltActor` is visual output only, not conveyor graph source of truth.
- Facility runtime, per-port inventories, processing, and conveyor transfer checks belong to `USRFacilityNetworkComponent`.
- Resource deposit runtime amount belongs to `USRStructureInstanceManagerComponent`; mining output is produced by `USRFacilityNetworkComponent`.
- UI z-order is configured by `ASRPlayerController::WidgetLayerOrder`.
- Widgets mirror state or dispatch user requests. Gameplay state belongs to Controller, Components, Subsystems, or runtime owner classes.

Current C++ / BP / DA connection:

- `BP_SolarSystemGenerator`, `BP_CelestialBody`, `BP_Star`, and `BP_Planet` are Blueprint surfaces for C++ runtime actors.
- Star/Planet/Moon Data Assets feed celestial runtime data; Planet/Moon DAs also connect terrain profiles, biomes, and material mapping.
- `ASRCameraPawn` owns focused camera movement and view rotation settings.
- Structure selection UI is `USRStructureSelectionWidget` plus WBP subclasses; it receives available `USRStructureDataAsset` entries from `ASRPlayerController`, categorizes them for display, and dispatches selected build requests back to the controller.
- `USRStructureDataAsset` configures structures and conveyors through `BuildKind`, placement data, construction flags, ports, optional deposits, and optional `FacilityDataAsset`.
- `USRFacilityDataAsset` defines facility processing behavior. Structure DA ports define facility slots and conveyor connection positions.
- `USRAssemblyComponent` enters/exits Assembly Mode from focused celestial screen size, places/removes structures and conveyors, then updates `USRStructureInstanceManagerComponent` and `USRConveyorNetworkComponent`.
- `USRStructureInstanceManagerComponent`, `USRFacilityNetworkComponent`, and `USRConveyorNetworkComponent` connect placed structures, facilities, and conveyor transfer at runtime.
- Widgets display runtime state or dispatch requests only. They are not gameplay source of truth.

Hot paths / optimization targets:

- Celestial Dynamic Mesh generation/cache and Surface Grid build.
- Surface Grid raycast, hover, overlays, and occupancy updates.
- Structure/conveyor placement, deletion, visual refresh, and PCG refresh.
- Conveyor/facility automation ticks and resource transfer.
- Camera visibility switching and screen-space line rendering.

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

- Structures occupy Surface Grid cells and define physical footprint plus input/output ports.
- Facility behavior is attached to a structure through Facility DA.
- Facilities use Structure DA ports as resource slots: Input Port equals Input Inventory slot, and Output Port equals Output Inventory slot.
- Conveyors build paths using cell and layer data.
- Conveyors move resources from a facility Output Port to another facility Input Port.
- Facility UI shows process state, process/deliver toggles, per-port input/output resource slots, output preview, and port inventories.
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

- Energy Resource: has an `EnergyValue` / Energy Total, Remaining Process Limit, process count, stack count, and process tags.
- Catalyst Resource: has a catalyst operator. Aquid is `+`, and Nitain is `*`.

Energy resources can pass through facilities only while their Remaining Process Limit allows it. When an Energy resource moves from input inventory into processing/output, its process limit is reduced. Hot temperature applies an additional process limit reduction. Energy has no lower bound, so negative Energy Total values are valid.

Facilities are broadly split into:

- Processing facility: usually consumes one Energy resource and amplifies its Energy Total.
- Synthesis facility: usually consumes two Energy resources plus one Catalyst resource, then applies the catalyst operator to produce one Energy result.
- Split facility: usually consumes one Energy resource and splits it into multiple Energy outputs.

Facility processing result order:

1. Reduce consumed Energy resource process limit.
2. Apply facility operation: processing, synthesis, or split.
3. Apply facility effects such as Energy/process-limit arithmetic, tag add/remove, byproduct production, or cell temperature effects.
4. Apply existing resource tag effects.

Resources can gain process tags:

- Responsive
- HalfLife
- Volatile
- Singularity

Tag effects:

- Responsive: passing through a facility in Hot temperature adds extra Energy Total.
- HalfLife: after 3 game cycles, Energy Total is halved.
- Volatile: passing through a facility reduces Energy Total.
- Singularity: the resource cannot enter another facility.

Waste is not a tag. It is an Energy resource / byproduct with a low starting Energy Total.

Facility temperature states affect processing:

- Frozen: facility stops.
- Cold: process time is doubled.
- Normal: no special modifier.
- Hot: consumed Energy resources lose additional Remaining Process Limit.
- Overheated: facility stops.

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

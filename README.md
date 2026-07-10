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

This section is a compact file-structure map. Use it to find the relevant source files before editing.

### 3.1 Project Root

- `StarRovers.uproject`: Unreal project file
- `StarRovers.code-workspace`: VS Code build/debug workspace
- `.gitignore`: generated files, build outputs, local editor files

### 3.2 Source Layout

C++ code lives under `Source/StarRovers`.

- `Public/`: public headers, runtime types, component/actor APIs
- `Private/`: implementation files, split by feature owner
- `Private/<Feature>/*.h`: private helper headers named by owner and responsibility

Current source roots:

- `Assembly`: build mode, placement, deletion, area selection/copy, undo/redo
- `Automation`: facilities, resources, processing, inventories
- `Camera`: player controller, camera pawn, focus, input, UI coordination
- `Celestial`: star/planet/moon actors, dynamic mesh, orbit-facing runtime data
- `Conveyor`: conveyor graph, placement, visuals, transport, PCG
- `Gravity`: gravity parent/child components
- `Logistics`: space logistics and spaceship runtime
- `Performance`: lightweight visual/runtime performance helpers
- `Rendering`: render-facing components and screen-space visual utilities
- `Simulation`: solar system generation, runtime settings, augment system
- `Structure`: structure data, placed structure instances, placement helpers
- `Surface`: planet surface grid, terrain generation, cell state
- `UI`: native widget classes
- `Utility`: diagnostics, timing, shared utility helpers
- `Editor`: editor-only commandlets and tools

### 3.3 Split Implementation Pattern

Large owners are split across multiple `.cpp` files. Private helper headers use the same owner/responsibility naming.

Common responsibility suffixes:

- `Input`
- `UI`
- `Selection`
- `Focus`
- `SurfaceInteraction`
- `Placement`
- `Deletion`
- `AreaSelection`
- `AreaCopy`
- `History`
- `DynamicMesh`
- `Runtime`
- `Spawn`
- `Path`
- `Transport`
- `PCG`
- `Diagnostics`
- `Rendering`
- `Display`
- `Pipeline`

Read the owner header first, then open the matching split `.cpp`.

### 3.4 Content Layout

Main project assets live under `Content`.

- `Content/StarRovers/Core/Blueprints`: core gameplay Blueprint classes
- `Content/StarRovers/Input`: input actions and mapping contexts
- `Content/StarRovers/Generation/Blueprints`: solar system generator Blueprint
- `Content/StarRovers/Celestial`: celestial body Blueprints, meshes, and star/planet/moon data
- `Content/StarRovers/Surface`: terrain profiles and biome data
- `Content/StarRovers/Automation`: facility and resource data
- `Content/StarRovers/Structure`: structure Blueprints and structure data
- `Content/StarRovers/Conveyor`: conveyor Blueprint and PCG assets
- `Content/StarRovers/Logistics`: spaceship Blueprint and logistics VFX assets
- `Content/StarRovers/Rendering`: space rendering Blueprint and meshes
- `Content/StarRovers/UI/Widgets`: widget Blueprints
- `Content/Effect`: VFX assets
- `Content/Externals/Space`: external space textures
- `Content/Levels`: playable/test maps
- `Content/Materials`: shared materials
- `Content/Objects/Natural`: natural structure meshes/materials
- `Content/Objects/Structure`: structure meshes/assets

Generated dynamic mesh cache assets matching `DA_DynamicMeshBase_*.uasset` are ignored by Git.

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

## 5. Star Rovers Logs

Star Rovers C++ logs are disabled by default. Enable one channel with `sr.Log.<Channel> 1`, disable it with `0`, or enable all with `sr.Log.All 1`.

- `sr.Log.Assembly`: build mode placement, hover, copy/delete, undo/redo diagnostics.
- `sr.Log.FacilityNetwork`: facility registration, inventories, processing, transfer, and debug actions.
- `sr.Log.Camera`: camera, player controller, focus, input, and UI routing diagnostics.
- `sr.Log.UIClickTrace`: widget click and pointer handling traces.
- `sr.Log.Celestial`: celestial body setup, materials, outline, and star lifecycle logs.
- `sr.Log.DynamicMesh`: dynamic mesh build, cache, base data, and mesh validation logs.
- `sr.Log.SolarSystem`: star system generation, spawn validation, and natural structure generation logs.
- `sr.Log.Surface`: surface grid, terrain, biome, and patch overlay logs.
- `sr.Log.SpaceLogistics`: hub routes, cargo travel, route visuals, and save/load logs.
- `sr.Log.Structure`: structure placement validation logs.
- `sr.Log.Conveyor`: conveyor path and actor-group placement logs.
- `sr.Log.Gravity`: gravity/orbit visual diagnostics.
- `sr.Log.Augment`: augment candidate generation and unlock logs.
- `sr.Log.Timing`: `[SR Timing]` performance summaries.
- `sr.Log.Memory`: `[SR Memory]` memory snapshots and tracked object counts.
- `sr.Log.EditorCommandlet`: editor commandlet diagnostics.

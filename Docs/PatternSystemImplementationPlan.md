# Pattern System Implementation Plan

## Goal

Replace the legacy scalar Energy/resource-effect model with deterministic 5x5 Pattern automation while retaining the existing structure placement, facility lifecycle, conveyor, celestial, and space-logistics foundations.

The runtime contains one gameplay interpretation: Pattern. Historical compatibility is isolated to explicitly versioned save DTO migration and never participates in live resource, facility, logistics, or stellar-contract rules.

## Overall Progress

**100% - Stages 1-10 implemented and verified**

| Stage | Cumulative progress | Deliverable | Status |
|---|---:|---|---|
| 1. Pattern foundation | 10% | Canonical board/mask types, coordinate rules, identity/hash, tests | Complete |
| 2. Glyph resolver | 20% | Five glyph movement, collision, post-cycle effects, deterministic trace | Complete |
| 3. Resources and deposits | 30% | Pattern resource instances and fixed source patterns from mining points | Complete |
| 4. Pattern facilities | 45% | Transform, synthesis, and separation operators in the facility pipeline | Complete |
| 5. Environments and generation | 55% | Planet/moon environment rules and solvability validation | Complete |
| 6. Surface and space logistics | 65% | Exact-pattern stacking, routing filters, cargo/save payload migration | Complete |
| 7. Stellar contracts and hands | 77% | Demand masks, hand scoring, stellar health, cycle settlement | Complete |
| 8. Run modifiers | 85% | Technology, true Augment, Trial, and shared modifier context | Complete |
| 9. UI and save | 93% | Reusable 5x5 UI, previews, unified versioned run save | Complete |
| 10. Content migration | 100% | New DA/BP references, legacy removal, full build and regression pass | Complete |

## Non-negotiable Invariants

- A Pattern is always a row-major 5x5 board with exactly 25 cells.
- Empty is an explicit cell value; the five playable glyphs are Metal, Organic, Crystal, Fluid, and Plasma.
- Pattern coordinates are canonical and independent of physical structure rotation. Pattern rotation only occurs through an explicit Pattern operator.
- Runtime execution, UI preview, generation validation, and balance simulation use the same C++ resolver.
- Random source layout is rolled once per mining point and copied for every harvested item.
- Transport and storage never mutate a Pattern implicitly.
- Separation cannot duplicate occupied glyph cells.
- Score is evaluated only against a stellar contract; facilities do not store or increase scalar Energy.

## Stage Gates

Every stage must pass all of the following before progress advances:

1. Relevant C++ compiles in `StarRoversEditor Win64 Development`.
2. New deterministic behavior has focused automation tests.
3. Existing unrelated behavior remains build-compatible.
4. Data Asset or Blueprint work that cannot be authored safely in C++ is recorded as an explicit Editor migration task.
5. The plan status and cumulative percentage are updated after verification.

## Latest Verification

- Source/static checks: passed through Stage 10. Scalar resource/facility/stellar gameplay fields and APIs are absent from the live runtime.
- `git diff --check`: passed.
- Build task source: `StarRovers.code-workspace`, task `StarRoversEditor Win64 Development Build` (`-waitmutex -MaxParallelActions=4`).
- The tracked workspace now points its terminal and project-generation .NET environment at UE 5.8's bundled `10.0\win-x64` SDK. Ignored build artifacts are not required.
- Unreal Editor build: passed after final legacy removal with UE 5.8's bundled .NET 10 SDK after removing the NiagaraEditor private-header Rules dependency and updating targets to `BuildSettingsVersion.V7`.
- Pattern content migration: the initial Apply rewrote 93 assets; the stellar-health and seeded-contract-catalog follow-ups each synchronized 58 authoritative assets. Every pass completed with zero validation errors and the final InspectOnly pass reported `created=0 changed=0 validationErrors=0`.
- Full Blueprint compile: 0 errors, 0 warnings, and 0 Blueprints failed to load.
- `StarRovers.Pattern`: 24/24 tests passed, including stellar demand masks, transformed/non-overlapping hands, score settlement, periodic health-rate math, environment, seeded candidate-order reproducibility, and bounded generation-reachability coverage.
- `StarRovers.Celestial`: 1/1 test passed, covering Main Sequence to Red Giant refill and Red Giant to Supernova Game Over transitions.
- `StarRovers.Facility.Pattern`: 7/7 tests passed, including environment application and snapshotted Run Modifier application after facility output.
- `StarRovers.ResourceSystem.PatternSource`: 3/3 tests passed.
- `StarRovers.Logistics.Pattern`: 3/3 tests passed, covering payload/stack identity, routing filters, and save-version contracts.
- `StarRovers.RunModifiers`: 4/4 tests passed, covering canonical resolution, balance bounds, progression authority, and stellar-contract projection.
- `StarRovers.Save`: 4/4 tests passed, covering generation-payload validation, unified binary memory round-trip, and atomic time/modifier validation.
- `StarRovers.UI`: 1/1 test passed, covering canonical Pattern presentation and inactive contract-mask cells.
- Full `StarRovers` automation filter: 47/47 tests passed.
- Generated `Binaries`, `Intermediate`, `Saved`, and dynamic-mesh cache files remain intentionally ignored and are not required in a clean checkout.

## Implemented Resolver Semantics

- A cycle contains zero to eight ordered move commands and applies Organic post-cycle growth once after every command.
- Every command snapshots selected occupied cells and resolves them front-to-back in its movement direction.
- Metal moves at most one cell regardless of requested distance.
- Organic uses the requested distance, remains in place when Metal blocks it, and grows by a configurable amount per connected component after the cycle.
- Crystal ignores requested distance, slides to the boundary, continues after defeating Organic or Plasma, and is destroyed by Metal or Fluid.
- Fluid follows requested movement, attempts a deterministic preferred-side detour around equal or stronger blockers, and otherwise resolves collision normally.
- Plasma jumps to the exact requested destination, ignores intermediate cells, and is ejected if its destination lies outside the board.
- The five playable glyphs each defeat exactly two and lose to exactly two other glyphs.
- Validation is atomic: an invalid command reports its index and produces neither partial board mutation nor partial trace.
- Every move, collision, block, ejection, boundary stop, detour, and growth is recorded in a contiguous deterministic trace.

## Implemented Resource Source Semantics

- A resource instance now carries its canonical 5x5 Pattern, source mining-point ID, source seed, resource identity, and stack count.
- A resource Data Asset declares exact per-glyph source counts. Empty glyph entries, duplicate glyph entries, non-positive counts, and totals above 25 are rejected atomically.
- Source layout generation is deterministic for a seed and independent of the order of glyph-count entries.
- Every mining point derives and stores one source seed and one source Pattern when registered. The owning celestial actor path is part of the source ID so equivalent local cell IDs on different bodies do not share identity.
- Mining previews copy the stored Pattern without mutating the deposit. Successful harvests copy the same Pattern and assign a new resource-instance ID.
- A configured positive deposit amount is finite and decreases by exactly one per successful harvest; zero continues to mean infinite for existing content.
- Inventory stack equivalence depends on resource identity and exact Pattern cells, not source provenance or seed.
- Scalar Energy, process-limit, process-count, and resource-tag members were removed from resource instances and Data Assets. The Pattern payload is the sole mechanical resource state.

## Stage 3 Content Result and Authoring Rules

- Migrated resource Data Assets contain validated `SourceGlyphCounts` and `SourcePatternSeedSalt`; the idempotent content inspection reports no remaining changes.
- Review deposits with `DepositTotalAmount`: positive values are now finite; zero is infinite.
- New Blueprint-authored Pattern views should use the reusable Stage 9 5x5 Pattern widget or its pure presentation model.

## Implemented Pattern Facility Semantics

- The serialized `Process` enum identifier remains intact for existing assets, but its gameplay and Editor display semantics are now `Transform`.
- Transform accepts exactly one Pattern resource and emits exactly one. It applies one configured mask/direction command with fixed distance one, then delegates glyph movement, collision, Fluid preference, and Organic growth to the shared Pattern resolver.
- Synthesis accepts exactly two Pattern resources. Input port zero is the base/defender and input port one is the overlay/attacker. Non-overlapping overlay cells copy into the result; occupied overlaps use the same glyph collision table as movement.
- A synthesis facility declares its output resource Data Asset. Its output receives that identity and a fresh instance ID, while single-source mining provenance is cleared.
- Separation accepts exactly one Pattern and emits exactly two. The configured primary mask routes cells to output zero and its complement routes cells to output one. Every occupied input cell appears in exactly one output, and an operation that would leave either output empty is rejected atomically.
- Transform and Separation preserve the input resource identity and source provenance but assign a fresh resource-instance ID to every output.
- Runtime execution, output preview, and focused tests all call the same pure facility resolver through one pipeline adapter.
- Pattern facilities and mining use `BaseProcessSeconds`, the snapshotted Run Modifier context, and the facility's physical temperature state. Frozen and Overheated stop processing; no separate resource-effect rule path exists.

## Stage 4 Content Result and Authoring Rules

- Migrated facility Data Assets are classified as Transform, Synthesize, Separate, or Mine. The serialized zero-valued `Process` identifier now means Transform.
- Configure each Transform asset's 25-cell `SelectionMask`, direction, Fluid side preference, Organic growth count, and base process time.
- Configure each Synthesis asset's product resource Data Asset. Its Structure Data Asset needs at least two physical input ports and one physical output port for an automation line.
- Configure each Separation asset's 25-cell primary-output mask. Its Structure Data Asset needs at least one physical input port and two physical output ports.
- The old facility `Effects` schema and scalar capacity fields were removed from C++; migrated assets were resaved so those serialized properties no longer remain in project content.
- Validate Separation masks against their intended input families: both the mask and its complement must receive at least one occupied cell, otherwise the facility correctly remains unable to start.

## Implemented Pattern Environment Semantics

- Every planet and moon carries one ordered environment specification. Existing content defaults to the valid `Neutral` environment with no effects.
- `DirectionalPull` submits one directional move command with distance one to four. `AffectedGlyph = Empty` selects all occupied cells; otherwise it filters to one glyph type.
- `ContinuousDrift` repeatedly submits one-cell commands in a fixed direction, rebuilding its selection after each step and stopping when the Pattern becomes stable or after four steps.
- `OrganicBloom` applies one to four Organic growth attempts per connected Organic component without selecting a movement source.
- Environment commands delegate movement, collision, destruction, and growth to the same deterministic glyph resolver used by facilities and previews. Environment effects do not maintain a second rule implementation.
- Transform and Synthesis outputs receive the owning body's environment after the facility operation. Separation applies the same environment independently to both outputs. Mining preserves the mining point's fixed source Pattern and does not apply the environment.
- Environment trace events retain facility output identity and record their ordered environment-effect index. Invalid environment specifications fail atomically and expose neither partial output nor partial trace.

## Implemented Generation Validation Semantics

- An optional Pattern Generation Profile declares the required Pattern/mask, available facility Data Assets, search depth/state bounds, and whether the generated system must require inter-body transfer.
- Runtime generation gathers representative fixed deposit Patterns, their owning celestial bodies, each body's environment, and the configured facility capabilities after natural structures have been generated. `MaxValidationSourcesPerResourcePerBody` bounds only the existential validator's sample; it never removes or changes runtime deposits.
- The bounded reachability search uses the production facility and environment resolvers. It explores at most depth 8, 8192 states, and 16 Transform plus 16 Separation operators per body. State queues are processed in operation-depth order and stop immediately at the configured cap.
- Validation first searches without transport, then with transfer between generated bodies. If no one body contains all non-empty glyph families required by the contract, glyph preservation proves the local goal impossible without enumerating irrelevant local states.
- The global pass checks the common automation spine (independent Transform branches followed by one Synthesis) before retaining non-goal Synthesis outputs for more general post-Synthesis processing. A Synthesis output cannot re-enter Synthesis, matching the Stellar Loom resource filters and preventing impossible recursive-product branches.
- A profile that requires transport fails when any source can satisfy the goal locally. Reaching the configured state cap reports `StateLimitExceeded` rather than incorrectly classifying the generated system as unsolvable.
- Environment rules alone influence which body is advantageous, but do not guarantee that transport is necessary. The profile's global-reachable/local-unreachable gate is the explicit guarantee; later stellar-contract and resource-distribution content must be authored against it.
- Stage 7 makes stellar contracts the authoritative goals. After source generation, the Run seed and generated-system signature produce a deterministic shuffle of every candidate; the first contract in that order that passes the Stage 5 reachability gate becomes active. The same explicit seed reproduces the order, while ordinary Runs create a fresh seed. An all-candidate failure remains an explicit invalid generation result.

## Stage 5 Content Result and Authoring Rules

- Configure `PatternEnvironment` on every planet and moon Data Asset. Suggested first-pass presets are Neutral (no effects), High Gravity (`DirectionalPull`, Down, all glyphs, distance 1), Frozen Gale (`ContinuousDrift`, all glyphs), Magnetic (`DirectionalPull` filtered to Metal), and Living World (`OrganicBloom`).
- Treat an environment's effect order as gameplay data: changing the order can change collision and growth outcomes.
- The Pattern Generation Profile contains real candidate stellar contracts, available facilities, and conservative search bounds; the deprecated single-goal fallback was removed.
- Enable `RequireInterBodyTransfer` only for profiles whose generated source distribution and body environments are intentionally designed to make local completion impossible.
- The profile is assigned to the Solar System Generator. Candidate selection uses the production reachability validator and reports an explicit generation failure when no candidate is solvable.

## Implemented Pattern Logistics Semantics

- A transportable payload must have a non-empty resource ID, positive stack count, and a non-empty canonical 25-cell Pattern. A reset resource remains the one valid empty-cargo sentinel.
- Stack equivalence is exactly stable resource ID plus all 25 Pattern cells. Resource Data Asset pointer, resource-instance ID, source provenance, and seed do not split an otherwise homogeneous stack.
- Facility inventory slots reject invalid Pattern payloads. A non-empty slot accepts only an exactly stack-equivalent Pattern, so no inventory, Hub buffer, or conveyor transfer can merge visually similar but mechanically different resources.
- Every physical Structure port now owns an optional Pattern routing filter. `Any Pattern` accepts any valid Pattern, `Exact Pattern` compares all 25 cells, and `Masked Pattern` compares only active mask cells, including cells that explicitly require Empty.
- Pattern coordinates in a port filter remain canonical when the physical structure rotates. Structure placement rotation changes only the port's surface direction and offset.
- Conveyor transfer uses the facility-port filter at its destination and copies the complete resource instance without changing Pattern cells. Invalid pre-Pattern conveyor cargo remains in place instead of being silently consumed.
- Hub routes use the same Pattern routing filter as surface ports. The native resource-criterion button constructs a complete `Any Pattern` filter; runtime route state has no parallel Resource-ID-only field.
- A route waits when no homogeneous cargo matching its resource and Pattern Manifest is available. It never substitutes another Pattern merely to depart.
- Hub loading, flight cargo, unloading, and star-fuel missile cargo all require valid Pattern payloads. Ordinary transport never applies glyph, environment, facility, or scoring rules.

## Implemented Pattern Logistics Save Semantics

- Space-logistics save data is now version 2. Route cargo, missile cargo, and complete Pattern Manifests are serialized with their 25-cell Pattern payloads.
- Version 1 Resource-ID filters migrate deterministically to `Any Pattern` filters with the same resource criterion.
- Version 1 in-flight cargo is accepted only when it already contains a valid Pattern payload. Legacy Energy-only cargo is not assigned a fabricated Pattern.
- Import validates every route and missile into temporary state first. Unsupported versions, malformed filters, invalid Pattern cargo, duplicate routes, or unresolved endpoints reject the import without clearing the currently running logistics state.
- Export logs and omits any corrupt runtime route or missile rather than writing an apparently valid save that has already lost its Pattern contract.
- Only the independently versioned route-save v1 DTO retains `CargoResourceId` for deterministic old-save migration. Runtime routes and save version 2 use `CargoFilter` as their sole filter state.

## Stage 6 Content Result and Authoring Rules

- Configure `RoutingFilter` on Structure Data Asset input ports that must accept a specific intermediate Pattern. Leave it at `Any Pattern` for unrestricted ports.
- Use `Exact Pattern` for a dedicated homogeneous line and `Masked Pattern` when only contract-relevant cells matter. An inactive masked filter is invalid and intentionally accepts nothing.
- Advanced Hub route `CargoFilter` values are available through Blueprint/API. The native route buttons edit the valid resource-criterion subset by replacing the full filter atomically.
- Treat existing Structure ports and routes as unrestricted after migration because their new filter defaults to `Any Pattern`.
- Discard or regenerate pre-Pattern saves that contain in-flight Energy-only cargo. Empty version 1 routes and version 1 Resource-ID Manifest settings remain migratable.
- Facility/conveyor world state now participates in the Stage 9 unified run save. Stage 6's independently versioned space-logistics payload remains its serialization boundary inside that aggregate.

## Implemented Stellar Contract Semantics

- One active stellar contract owns an exact demand Pattern and partial 5x5 Mask. Every active Mask cell is compared, including cells that explicitly require `Empty`; cells outside the Mask do not affect qualification.
- A submitted cargo stack must already be a valid Pattern payload and every identical item in the stack must satisfy the active demand. Contract preview, missile selection, impact, scoring, health, and stellar evolution consume no scalar resource value.
- Every qualified Pattern earns `BaseScorePerPattern`. Completed bonus hands add their configured score, and the per-Pattern total is multiplied by the homogeneous stack count.
- A hand rule can require one uniform non-empty glyph over a Shape or an exact ordered glyph Shape. Rules may be fixed, translated, rotated and translated, or rotated/reflected and translated. Matching order and results are deterministic.
- Hand rarity is explicit gameplay metadata (`Common`, `Uncommon`, `Rare`, `Epic`, `Legendary`) while `BonusScore` remains independently tunable. The default examples are Five in a Line (+5), Solid 2x2 Block (+8), Stellar Cross (+14), and Outer Ring (+40).
- Bonus hands do not overlap by default. Matching candidates are ordered by score, rarity, rule ID, and placement identity; the highest-value eligible hands claim their cells first. A contract can explicitly permit overlapping hands when multiplicative combination scoring is desired.
- Runtime submission, Blueprint preview, missile eligibility, tests, and future balance simulation call the same pure `FSRStellarPatternContractResolver`. A rejected Pattern contributes zero partial score.
- Contracts settle when the shared simulation Period advances. The score target remains linear: `RequiredScorePerCycle + RequiredScoreGrowthPerCycle * PeriodIndex`, but missing or surplus score at settlement does not directly change health.
- Stellar health decreases once per simulated second. Period 0 uses `InitialStellarHealthDecreasePerSecond`; Period `n` uses `InitialStellarHealthDecreasePerSecond * StellarHealthDecreaseMultiplierPerPeriod^n`. Pause and speed controls affect the same simulation clock.
- Every accepted Pattern immediately restores `TotalScore * StellarHealthRestoredPerPatternScore`, after the active health-recovery modifier, and health is clamped to `[0, StellarHealthMaximum]`. This is the Pattern-system equivalent of legacy fuel delivery without restoring scalar Energy to resources.
- First depletion advances Main Sequence to Red Giant and refills `StartingStellarHealth`. A second depletion advances Red Giant to Supernova, pauses simulation, and opens the Game Over screen.
- Star-bound missiles take only cargo that matches the active contract. Impact re-evaluates the cargo so an in-flight submission cannot score against a contract that changed after launch.
- A Pattern Generation Profile owns candidate stellar contracts. A seeded Fisher-Yates shuffle combines the runtime generation seed with the generated source signature, every candidate uses the production reachability validator, and the first solvable shuffled candidate becomes the star's active contract. Duplicate IDs and invalid contracts are skipped; an `ASRStar` primary class is mandatory.
- Legacy scalar fuel-delivery APIs remain removed. Accepted Pattern score, the per-second stellar-health clock, and two-stage stellar evolution are the sole runtime authority.

## Stage 7 Balance Baseline

- The generated starting contract grants 12 base score per qualified Pattern, starts at a 48-point target, and grows by 3 points per Period. Four clean Patterns meet Period 0 before bonuses.
- The generous pre-balance health baseline is 1000/1000, 0.25 health per simulated second in Period 0, a 1.05x compounded decrease per Period, and 1 health restored per accepted Pattern score. `SecondsPerPeriod` is 60 in Project Settings by default.
- With no deliveries, one Main Sequence health bar lasts about 30 one-minute Periods; continued rate growth makes the Red Giant reserve last roughly another 13. Meeting the initial 48-point target restores more than the initial 15 health consumed per 60-second Period, leaving substantial setup time before higher Period rates make bonus hands necessary.
- Throughput can be balanced as `score delivered / PeriodDuration`; net health change for Period `n` is approximately `delivered score * restoration coefficient - PeriodDuration * initial decrease * growth multiplier^n`. Bonus value remains comparable to completion probability and occupied operations/cells.

## Stage 7 Content Result and Authoring Rules

- Configure `DefaultStellarPatternContract` on every Star Data Asset as a valid fallback. Its demand Mask must contain at least one active cell requiring a playable glyph.
- Pattern Generation Profiles contain one or more uniquely named `CandidateStellarContracts`; the deprecated `LegacyGoal` field and fallback path were removed.
- Keep candidate requirements aligned with the profile's available facilities and `RequireInterBodyTransfer` gate. A candidate reachable locally is correctly rejected when transfer is required.
- Tune base score, Period target growth, health maximum, initial per-second decrease, Period multiplier, and score restoration together using the baseline formula above. Increase bonuses with practical construction rarity, not rarity labels alone.
- Review hand Shapes, transform policies, match caps, and overlap policy. Fixed Shapes use canonical board coordinates; physical Structure rotation never rotates a hand or demand Mask.
- Ensure the generator's configured Star class derives from `ASRStar`; a generic celestial actor cannot own or settle a stellar contract.
- Blueprint content contains no references to removed scalar fuel APIs; the full Blueprint compile confirms every asset loads and compiles.
- The native star/focus UI reads only the authoritative contract Mask, score, hand breakdown, Cycle, and stellar-health state.

## Implemented Run Modifier Semantics

- Technology, Augment, and Trial are authored as distinct Data Asset types but compile into one `FSRRunModifierContext`. The context is canonicalized by source kind (`Technology`, `Augment`, `Trial`), priority, source ID, effect channel, condition, and effect ID, so registration and map iteration order cannot change a result.
- An effect can scale facility process time, stellar base score, stellar bonus score, stellar required score, per-second stellar-health decrease, Pattern-score health restoration, or logistics travel time. Discrete effects adjust Transform Organic growth and post-facility environment intensity.
- Facility effects can target Transform, Synthesis, Separation, Mining, or every facility. They can also target the deterministic dominant playable glyph across the current input Patterns. Stellar effects can target one contract ID; `None`, `Empty`, and `Any Facility` are explicit wildcards.
- Technology is the sole progression authority for guaranteed facility access. A Technology declares prerequisite Technology IDs, facility Structure IDs, optional modifier effects, and whether it is unlocked by default. The Augment subsystem remains the shared construction-query and offer facade, but Augment choices never unlock a facility.
- A true Augment declares rarity (`Common`, `Rare`, `Epic`), offer role (`Immediate`, `Synergy`, `Pivot`), stack cap, and modifier effects. Cycle offers draw from eligible Augment Data Assets, try to expose the three roles, retain the existing deterministic seed/pity behavior, and apply a modifier stack through the shared authority.
- A Trial declares a positive duration in Cycles and one combined risk/reward effect list. Activation records an inclusive start and exclusive end Cycle; expiry removes its source and rebuilds the context. This permits a Trial to worsen environment, throughput, demand, or health risk while increasing base/bonus score through the same bounded resolver.
- A facility snapshots the complete context before a batch starts. Process time, Pattern output, Organic growth, and environment application use that snapshot until completion; an idle output preview takes the current context, while an in-progress preview uses the batch snapshot.
- A negative environment-intensity delta can remove an effect when its intensity reaches zero. Positive intensity is clamped to the four-cell board limit and changes Directional Pull distance, Continuous Drift steps, or Organic Bloom growth count without introducing a second environment resolver.
- A stellar contract snapshots the context for one Period. Base and hand score, required score, per-second health decrease, and Pattern-score restoration use that projection; a modifier change may update an empty Period but cannot mix scoring formulae after a submission. Contract math remains in the pure stellar resolver for runtime, preview, and balance tests.
- Hub routes and star-fuel missiles resolve the travel-time channel once at departure and store the resulting duration. In-flight Technology, Augment, or Trial changes therefore do not teleport or retime existing cargo.
- Configured Data Assets are soft-referenced by `USRSimulationSettings` and synchronously registered by the world subsystem at run initialization. Blueprint registration APIs remain available for generated or test content.

## Stage 8 Balance Baseline

- Multipliers compose multiplicatively once per source stack. There is deliberately no arbitrary override operation, so effect ordering cannot create hidden last-writer behavior.
- Facility time, stellar score, and health multipliers are clamped to `[0.25, 4.0]`; stellar requirement is clamped to `[0.5, 3.0]`; logistics travel time is clamped to `[0.5, 2.0]`. Organic-growth and environment-intensity deltas are integer values clamped to `[-4, 4]`.
- Authored individual multiplier magnitudes must be in `[0.01, 10.0]`, source stacks are limited to 16, and discrete authored magnitudes must already be integers in `[-4, 4]`. The final clamps remain authoritative even under extreme legal stacking.
- Modified integer scores use deterministic nearest-integer rounding and saturating arithmetic. Base score and bonus score are scaled independently before they are added, so a reward-focused Trial or Augment can value difficult hands without also inflating guaranteed demand score.
- A first content pass should budget one primary advantage and one meaningful line-design tradeoff per Augment. Trial reward should be compared against expected lost throughput and stellar-health risk, not merely assigned from its rarity label.

## Stage 8 Content Result and Authoring Rules

- Create Technology Data Assets for the guaranteed progression spine. The default-unlocked set must expose every facility needed to build a solvable first stellar-contract line; later Technologies may unlock Synthesis, Separation, routing, or recovery options behind explicit prerequisites.
- Create enough true Augment Data Assets that each offer can usually contain Immediate, Synergy, and Pivot roles. Configure rarity, stack cap, conditions, and effects; do not put mandatory facility unlocks in an Augment.
- Create Trial Data Assets with explicit Cycle durations and paired risk/reward channels. Prefer pressures that force a line change (environment intensity, targeted throughput, demand, logistics) over universal penalties that only ask the player to wait.
- Technology, Augment, and Trial Data Assets are authored and assigned through the Star Rovers Simulation settings.
- The old `bDebugUnlockAllFacilitiesWithoutAugments` key was replaced by `bDebugUnlockAllFacilitiesWithoutTechnology`; config, Editor label, and runtime meaning now agree.
- Stage 9 persists unlocked Technology IDs, Augment stack counts, active Trial boundaries, and the contract/facility context revisions needed by in-progress work. The native Augment panel now presents role, stack, conditions, and concrete effect previews.

## Implemented Pattern UI Semantics

- `USRPatternGridWidget` is the reusable native 5x5 renderer. Its pure `FSRPatternGridPresentation` model validates canonical 25-cell Patterns, assigns stable glyph text/colors, and dims cells outside an optional contract Mask without changing board coordinates.
- Facility input, output, and in-progress previews render the actual resolved Pattern, stack count, and stable Pattern identity. They contain no scalar Energy summary.
- Idle facility previews resolve with the current modifier context; an active batch presents the snapshotted context and resulting Pattern used by runtime completion. Preview and execution therefore retain the Stage 4/8 shared-resolver contract.
- Celestial focus exposes an authoritative `FSRFocusedStellarContractInfo`: demand Pattern/Mask, current Period score split, accepted/rejected counts, stellar health, last submitted-hand breakdown, and authored bonus-hand rules. The always-visible upper-right HUD renders the target Pattern and score, while the uppermost center Progress Bar renders `CurrentStellarHealth / MaximumStellarHealth`.
- The native focus widget renders the partial demand Mask directly. Inactive cells are visually distinct from active cells that explicitly require `Empty`.
- Augment choices show rarity, Immediate/Synergy/Pivot role, current/resulting stack cap, effect conditions, and concrete resolved modifier effects instead of compatibility-only labels.

## Implemented Unified Run Save Semantics

- `USRRunSaveGame` owns one versioned `FSRRunSaveData` aggregate, and `USRRunSaveSubsystem` exposes capture, validate, transactional restore, slot save/load, and a retained diagnostic error for UI.
- The aggregate persists the shared clock, unlocked Technologies, Augment stacks, active Trial Cycle intervals, modifier-context revision, pending Augment offer/pity/pause ownership, and every generated celestial body's transform and orbit phase.
- Body records persist fixed mining deposits and exact occupant IDs, facility inventories and processing progress, canonical in-flight Pattern outputs and snapshotted modifier context, conveyor paths/items/progress and merge/branch round-robin state, stellar contract/runtime state, and space-logistics routes/missiles/cargo.
- The aggregate records the generator identity, topology content version, runtime generation seed, and body keys. Restore regenerates topology from that root seed, then requires actor name, variable name, category, and body generation seed to match exactly; changed content, missing bodies, or ambiguous identities are rejected rather than guessed.
- Every nested DTO is versioned and validated before the first mutation. Validation covers registered assets, IDs, capacities, port routing, canonical Pattern cargo, cross-component occupant/facility relationships, logistics endpoints, active Trial boundaries, and Augment caps.
- Restore pauses time, applies state in dependency order, and rolls the complete captured snapshot back if any later component import fails. No rejected import intentionally exposes a partially restored automation line.
- Save data contains no dependency on ignored `Binaries`, `Intermediate`, or `Saved` artifacts; a clean checkout builds the serializers from tracked source and writes slot files only at runtime.

## Stage 9 Integration Result and Authoring Rules

- Player-facing Save/Load controls can call `USRRunSaveSubsystem::SaveRunToSlot` and `LoadRunFromSlot` and surface `GetLastSaveError` when capture, validation, regeneration, or restore fails.
- Slot loading validates the saved generation payload, regenerates the recorded topology from its runtime seed, registers the resulting content, and then restores transactional world state. Failure rolls back by regenerating and restoring the pre-load snapshot.
- Replace remaining Blueprint Pattern summaries with `USRPatternGridWidget` subclasses/styles where authored visual treatment is needed. Preserve canonical row-major coordinates and distinguish inactive Mask cells from active `Empty` requirements.
- Blueprint focus and Augment assets compile against only the new authoritative fields.
- Before shipping a balance/content release, manually exercise a representative authored save containing an active facility batch, items between conveyor nodes, a route or missile in flight, a pending Augment choice, and an active Trial. This is release QA rather than an unfinished engineering migration stage.

## Stage 10 Completion

- The semantic content pass now owns six resource, four environment, nine facility, nine artificial-structure, and five mining-deposit Data Assets. Four renamed celestial bodies reference the environment assets directly; the complete naming and numerical baseline is recorded in `Docs/PatternContentCatalog.md`.
- The numbered Processor/Synthesizer catalog, Territe/Nitain/Aquid/Waste resources, superseded structures, and old-name redirectors were removed as 155 Legacy assets. Current Pattern content no longer depends on the old Automation Data Asset roots.
- Applying the Pattern content commandlet configured and resaved 58 authoritative assets with zero validation errors. Its immediately following read-only inspection is idempotent with zero creates, renames, changes, or validation errors.
- `BP_SolarSystemGenerator` defaults and the generator actor stored in `SolarSystem.umap` both reference the Pattern Generation Profile, star/planet classes, one star DA, three planet DAs, and one moon DA. Begin Play validates the map instance and restores the complete Blueprint class-default configuration if an instance is incomplete.
- `BP_SRPlayerController` owns the construction catalog for all nine Pattern structures plus the conveyor. The Simulation settings own the two Technology, six Augment, and three Trial assets, so ordinary Play requires no manual Details-panel assignment.
- The authored celestial DAs include their shape base, surface material, terrain profile, optional ocean/atmosphere materials, physical values, and Pattern environment. The starter runtime uses `DA_PlanetShape_Cube64`; the substantially heavier Cube256 asset remains an optional quality upgrade rather than a default startup cost.
- `BP_SolarSystemGenerator` and the real `/Game/Levels/SolarSystem` generator instance both enable a fresh generation seed for ordinary Runs. `DA_PatternGeneration_Default` owns 16 two-cell candidate contracts spanning Metal, Organic, Plasma, Crystal, and Fluid, while explicit-seed generation remains available for exact save restoration.
- Scalar resource state, scalar facility effects, scalar stellar fuel, deprecated generation-goal fallback, duplicate runtime route filter state, and obsolete compatibility helpers were removed from live C++ gameplay paths.
- Historical logistics save v1 migration remains deliberately isolated in its versioned DTO; it cannot fabricate a Pattern or influence current runtime behavior.
- The final verification gate passed C++ Editor builds, a full Blueprint compile with zero errors/warnings/unloadable Blueprints, all 47 StarRovers automation tests, content inspection, and static/diff checks.

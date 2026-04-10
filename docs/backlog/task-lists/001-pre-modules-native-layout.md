# Pre-Modules Native Layout Backlog

Purpose: define and sequence the work needed to restructure the current
header-only library around explicit shared and native-platform ownership before
introducing C++ modules, using CMake target boundaries to separate platform
implementations without forcing an immediate public API rewrite.

Status legend:
- `todo`: not started
- `doing`: active
- `done`: completed
- `blocked`: waiting on dependency
- `deferred`: intentionally postponed

## Implementation Status

Current status: planning complete for `MODLAY-01` through `MODLAY-03`, and the
shared-surface move from `MODLAY-04` has landed with forwarding compatibility
headers retained under `src/lumalink/platform/`.

Planning decisions are recorded in
`docs/pre-modules-native-layout-plan.md`.

A draft downstream migration guide exists in
`docs/upgrading-pre-modules-native-layout.md`, but it should be treated as a
preliminary playbook until the final destination paths and target names are
confirmed by the implementation work.

The current native and embedded platform code still lives under a mixed header
layout centered on `src/lumalink/platform/`, with platform selection primarily
expressed in umbrella and factory headers rather than in CMake target
ownership.

The current platform-facing tree is concentrated in:

- `src/lumalink/LumaLinkPlatform.h`
- `src/lumalink/platform/ClockFactory.h`
- `src/lumalink/platform/TransportFactory.h`
- `src/lumalink/platform/PathMapper.h`
- `src/lumalink/platform/buffers/`
- `src/lumalink/platform/filesystem/`
- `src/lumalink/platform/time/`
- `src/lumalink/platform/transport/`
- `src/lumalink/platform/posix/`
- `src/lumalink/platform/windows/`
- `src/lumalink/platform/arduino/`

The intended direction from `docs/modules.md` is to keep shared contracts in
platform-agnostic roots, move native Windows and POSIX implementation-heavy
headers into explicit native roots, keep embedded code on a separate non-module
path for now, and let CMake choose implementation families instead of making
all translation units see all platform headers.

## Design Intent

- Make shared versus platform-specific ownership obvious in the source tree before any module work begins.
- Use CMake targets and include paths to express native platform selection instead of relying only on header-level preprocessor branching.
- Preserve the current public behavior and umbrella/factory surface during the transition unless a specific step intentionally narrows it.
- Keep embedded targets out of the native-host restructuring until the native layout is stable.
- Prepare the tree for later module interface units without requiring the module rollout to happen in the same change set.

## Scope

- Source-tree restructuring under `src/lumalink/` for shared contracts, native-common code, and Windows/POSIX implementation families
- CMake target and include-path changes needed to support the new ownership boundaries
- Thinning of `LumaLinkPlatform.h`, `ClockFactory.h`, and `TransportFactory.h` into facade headers as implementation headers move
- Validation that native tests still build and pass after each structural move
- A downstream-upgrade howto document that agents can use to update dependent libraries after the layout stabilizes

## Non-Goals

- Do not introduce actual C++ module interface units in this backlog
- Do not redesign transport, filesystem, time, or path abstractions beyond the structural moves needed to separate ownership
- Do not perform a broad public include-path rename unless it is needed for a specific task in scope
- Do not merge shared contracts into duplicated platform-specific trees
- Do not restructure embedded targets in parallel with the native-host layout changes

## Architectural Rules

- Shared contracts and value types stay in platform-agnostic roots; only concrete native implementations and system-header-facing adapters move under native platform roots.
- CMake becomes the source of truth for native implementation selection where practical; headers should become thinner over time, not more responsible for platform discovery.
- Public umbrella and factory headers may remain as temporary facades, but they should stop owning implementation-heavy include graphs.
- Native Windows and native POSIX code should be able to compile without exposing unrelated platform headers through broad include directories.
- Embedded code remains on its current header-driven path until the native layout and downstream migration story are proven.
- Downstream migration steps must be documented in a deterministic way that another agent can execute without reverse-engineering intent from commit history.

## Work Phases

## Phase 1 - Inventory And Target Boundary Definition

| ID | Status | Task | Depends On | Definition of Done |
|---|---|---|---|---|
| MODLAY-01 | done | Inventory the current shared, Windows-native, POSIX-native, and embedded-only headers under `src/lumalink/platform/` and classify which ones are contracts versus concrete implementations | none | Completed in `docs/pre-modules-native-layout-plan.md` |
| MODLAY-02 | done | Define the target boundary plan for shared contracts, native-common code, Windows-native code, POSIX-native code, and embedded code under CMake | MODLAY-01 | Completed in `docs/pre-modules-native-layout-plan.md` |
| MODLAY-03 | done | Decide the destination naming and root structure for the pre-modules layout, including whether facades live under a dedicated `facade/` root or remain at the current public paths as forwarding headers | MODLAY-02 | Completed in `docs/pre-modules-native-layout-plan.md` |

## Phase 2 - Shared Surface Extraction

| ID | Status | Task | Depends On | Definition of Done |
|---|---|---|---|---|
| MODLAY-04 | done | Move platform-agnostic contracts and helpers into explicit shared roots such as `core/`, `path/`, `buffers/`, `filesystem/`, `transport/`, and `time/` without changing behavior | MODLAY-03 | Completed by moving canonical shared headers under `src/lumalink/` roots, retaining forwarding headers under `src/lumalink/platform/`, and passing the native CTest lane |
| MODLAY-05 | todo | Extract any native-host shared implementation helpers into a dedicated `native/common/` area if they are currently buried in platform-specific or mixed roots | MODLAY-04 | Host-only shared helpers have a clear home and are no longer duplicated or hidden behind unrelated platform directories |
| MODLAY-06 | todo | Thin `LumaLinkPlatform.h` so it acts as a facade over the reorganized shared and selected platform surfaces rather than directly representing the old mixed tree | MODLAY-04 | The umbrella header still supports current consumers while depending on thinner, better-owned include layers |

## Phase 3 - Native Platform Root Split

| ID | Status | Task | Depends On | Definition of Done |
|---|---|---|---|---|
| MODLAY-07 | todo | Move Windows-specific implementation headers into a dedicated native Windows root and update internal includes accordingly | MODLAY-04, MODLAY-03 | Windows-native code is no longer housed in the mixed `platform/` tree and the native build still succeeds on the supported Windows path |
| MODLAY-08 | todo | Move POSIX-specific implementation headers into a dedicated native POSIX root and update internal includes accordingly | MODLAY-04, MODLAY-03 | POSIX-native code is no longer housed in the mixed `platform/` tree and the intended POSIX ownership boundary is explicit in the tree |
| MODLAY-09 | todo | Keep embedded code on its existing non-module path for now and explicitly separate it from the new native-host roots | MODLAY-03 | The tree reflects that embedded code is intentionally deferred and no native-host move accidentally drags embedded headers into the new roots |
| MODLAY-10 | todo | Convert the factory headers into thinner facade selectors over the new shared and native roots instead of having them sit on top of a broad mixed directory | MODLAY-07, MODLAY-08, MODLAY-09 | `ClockFactory.h` and `TransportFactory.h` preserve current behavior while their include responsibility is reduced and easier to replace later |

## Phase 4 - CMake Ownership And Validation

| ID | Status | Task | Depends On | Definition of Done |
|---|---|---|---|---|
| MODLAY-11 | todo | Update CMake so shared, native-common, Windows-native, and POSIX-native headers and sources are owned by distinct targets or target-like groupings with scoped include paths | MODLAY-05, MODLAY-07, MODLAY-08 | The build uses target-level include and source ownership rather than exposing the whole mixed tree globally |
| MODLAY-12 | todo | Add or refine native test coverage expectations so each structural move is validated by the existing native test path | MODLAY-11 | The intended native validation command is documented and used consistently after restructuring changes |
| MODLAY-13 | todo | Remove temporary forwarding layers or redundant include paths that are no longer needed once the native layout compiles cleanly | MODLAY-06, MODLAY-10, MODLAY-11 | Transitional shims are reduced to the minimum still needed for downstream compatibility and the tree no longer carries avoidable duplication |

## Phase 5 - Downstream Migration Exit

| ID | Status | Task | Depends On | Definition of Done |
|---|---|---|---|---|
| MODLAY-14 | todo | Write an upgrade howto document for dependent libraries covering include-path changes, expected facade usage, CMake consumption changes, and the recommended migration sequence | MODLAY-13 | A checked-in howto exists with deterministic steps that an agent can apply to a dependent library without needing additional architectural context |
| MODLAY-15 | todo | Include at least one agent-oriented migration recipe in the upgrade howto that describes how to inventory old includes, replace them with the new surface, and validate the dependent build | MODLAY-14 | The howto contains a concrete, reproducible workflow suitable for agent-driven downstream upgrades rather than only high-level prose |
| MODLAY-16 | todo | Validate the upgrade howto against the single known downstream consumer or a representative dependent-library fixture before treating the backlog as complete | MODLAY-15 | The documented migration steps have been exercised successfully, necessary corrections are folded back into the howto, and the native layout work has an explicit downstream exit condition |

## Exit Criteria

This backlog is complete when:

- shared, native-common, Windows-native, and POSIX-native ownership is explicit in the tree
- CMake, not only header branching, governs the native implementation layout
- umbrella and factory headers are reduced to thin facade roles
- embedded code remains intentionally separate rather than half-migrated
- a downstream upgrade howto exists and has been validated well enough that agents can use it to upgrade dependent libraries with low ambiguity

## Appendix A - Initial Header Ownership Inventory

This appendix is the starting point for `MODLAY-01`. It reflects the current
tree as of backlog creation time and should be refined as implementation work
uncovers tighter ownership boundaries.

### Current Public Or Facade Surface

| Current header | Initial classification | Notes |
|---|---|---|
| `src/lumalink/LumaLinkPlatform.h` | facade/public umbrella | Current broad public include surface |
| `src/lumalink/platform/ClockFactory.h` | facade/public selector | Currently mixes shared clock contracts with platform-selected implementation aliases |
| `src/lumalink/platform/TransportFactory.h` | facade/public selector | Currently mixes shared transport contracts with platform-selected implementation factories |

### Shared Contract And Helper Candidates

| Current header | Initial classification | Likely destination area |
|---|---|---|
| `src/lumalink/platform/PathMapper.h` | shared contract/helper | `path/` |
| `src/lumalink/platform/buffers/Availability.h` | shared contract/helper | `buffers/` |
| `src/lumalink/platform/buffers/ByteStream.h` | shared contract/helper | `buffers/` |
| `src/lumalink/platform/filesystem/FileSystem.h` | shared contract | `filesystem/` |
| `src/lumalink/platform/memory/MemoryFileAdapter.h` | shared helper or shared filesystem utility | likely `filesystem/` or another shared root pending `MODLAY-03` |
| `src/lumalink/platform/time/ClockTypes.h` | shared contract | `time/` |
| `src/lumalink/platform/time/Clock.h` | shared contract | `time/` |
| `src/lumalink/platform/time/ManualClock.h` | shared helper/testable utility | `time/` |
| `src/lumalink/platform/time/TimeSource.h` | shared contract | `time/` |
| `src/lumalink/platform/transport/TransportInterfaces.h` | shared contract | `transport/` |
| `src/lumalink/platform/transport/TransportTraits.h` | shared contract/helper | `transport/` |

### Native Windows Candidates

| Current header | Initial classification | Likely destination area |
|---|---|---|
| `src/lumalink/platform/windows/WindowsTime.h` | Windows-native implementation | `native/windows/` |
| `src/lumalink/platform/windows/WindowsSocketTransport.h` | Windows-native implementation | `native/windows/` |
| `src/lumalink/platform/windows/WindowsFileAdapter.h` | Windows-native implementation | `native/windows/` |

### Native POSIX Candidates

| Current header | Initial classification | Likely destination area |
|---|---|---|
| `src/lumalink/platform/posix/PosixTime.h` | POSIX-native implementation | `native/posix/` |
| `src/lumalink/platform/posix/PosixSocketTransport.h` | POSIX-native implementation | `native/posix/` |
| `src/lumalink/platform/posix/PosixFileAdapter.h` | POSIX-native implementation | `native/posix/` |

### Embedded Deferred Candidates

| Current header | Initial classification | Notes |
|---|---|---|
| `src/lumalink/platform/arduino/ArduinoTime.h` | embedded implementation | Keep on the non-module embedded path during native-layout work |
| `src/lumalink/platform/arduino/ArduinoWiFiTransport.h` | embedded implementation | Keep on the non-module embedded path during native-layout work |
| `src/lumalink/platform/arduino/ArduinoFileAdapter.h` | embedded implementation | Keep on the non-module embedded path during native-layout work |

## Appendix B - Initial Migration Seed Map

This appendix is a draft seed for `MODLAY-14` and `MODLAY-15`. It records the
first-pass old-to-new mapping that downstream migration work should use unless
the implementation lands on a different final public path.

| Current include | Planned destination |
|---|---|
| `lumalink/platform/PathMapper.h` | `lumalink/path/PathMapper.h` |
| `lumalink/platform/buffers/Availability.h` | `lumalink/buffers/Availability.h` |
| `lumalink/platform/buffers/ByteStream.h` | `lumalink/buffers/ByteStream.h` |
| `lumalink/platform/filesystem/FileSystem.h` | `lumalink/filesystem/FileSystem.h` |
| `lumalink/platform/time/ClockTypes.h` | `lumalink/time/ClockTypes.h` |
| `lumalink/platform/time/Clock.h` | `lumalink/time/Clock.h` |
| `lumalink/platform/time/ManualClock.h` | `lumalink/time/ManualClock.h` |
| `lumalink/platform/time/TimeSource.h` | `lumalink/time/TimeSource.h` |
| `lumalink/platform/transport/TransportInterfaces.h` | `lumalink/transport/TransportInterfaces.h` |
| `lumalink/platform/transport/TransportTraits.h` | `lumalink/transport/TransportTraits.h` |
| `lumalink/platform/windows/WindowsTime.h` | `lumalink/native/windows/WindowsTime.h` |
| `lumalink/platform/windows/WindowsSocketTransport.h` | `lumalink/native/windows/WindowsSocketTransport.h` |
| `lumalink/platform/windows/WindowsFileAdapter.h` | `lumalink/native/windows/WindowsFileAdapter.h` |
| `lumalink/platform/posix/PosixTime.h` | `lumalink/native/posix/PosixTime.h` |
| `lumalink/platform/posix/PosixSocketTransport.h` | `lumalink/native/posix/PosixSocketTransport.h` |
| `lumalink/platform/posix/PosixFileAdapter.h` | `lumalink/native/posix/PosixFileAdapter.h` |

Items that still need explicit confirmation before downstream automation should
assume them:

- the final public path for `MemoryFileAdapter.h`
- whether `ClockFactory.h` and `TransportFactory.h` keep their current public paths as facades
- whether `LumaLinkPlatform.h` remains the preferred broad-consumption include

# C++ Modules Export Backlog

Purpose: define and sequence the work needed to migrate the project's public
header-based surface to exported C++20 modules while preserving native-host
builds, keeping platform-specific code isolated, and updating the project's
in-repo tests to the module surface as part of the cutover while leaving
maintained downstream consumer migrations to their own backlogs.

Status legend:
- `todo`: not started
- `doing`: active
- `done`: completed
- `blocked`: waiting on dependency
- `deferred`: intentionally postponed

## Implementation Status

Current status: Phase 1 decision work is complete; implementation work has not started.

The current public surface is still header-based. The main entry points are
`src/LumaLinkPlatform.h` and `src/lumalink/LumaLinkPlatform.h`, with the
portable abstraction headers under `src/lumalink/platform/` and native-host
implementations under `src/windows/` and `src/posix/`.

The current build target `lumalink-platform` is an interface library that
exports include directories and installs headers by copying the `src/` tree.
No module interface units, module partitions, compiler feature gating, or BMI
packaging/export flow exists yet.

The current public API surface is concentrated in:

- `src/LumaLinkPlatform.h`
- `src/lumalink/LumaLinkPlatform.h`
- `src/lumalink/platform/Availability.h`
- `src/lumalink/platform/ByteStream.h`
- `src/lumalink/platform/FileSystem.h`
- `src/lumalink/platform/TransportInterfaces.h`
- `src/lumalink/platform/TransportTraits.h`
- `src/lumalink/platform/TransportFactory.h`
- `src/lumalink/platform/ClockTypes.h`
- `src/lumalink/platform/Clock.h`
- `src/lumalink/platform/ManualClock.h`
- `src/lumalink/platform/TimeSource.h`
- `src/windows/WindowsFactory.h`
- `src/posix/PosixFactory.h`

The test suite currently consumes the library through header includes. The
compatibility story for this migration is to update the in-repo tests to import
the module surface rather than building a long-lived dual public API. Maintained
downstream consumers should track their own migrations in their own backlogs.

## Design Intent

- Make exported C++20 modules the canonical public surface instead of header inclusion.
- Preserve the existing platform abstraction boundaries: buffers, filesystem, transport, time, and platform factory dispatch.
- Update the in-repo tests to the module surface instead of carrying a long-term compatibility layer for header-based usage.
- Keep host-specific Windows and POSIX implementation details out of the portable module surface unless explicitly imported.
- Keep the default arithmetic and time semantics unchanged; this is a packaging and interface migration, not a behavior rewrite.
- Use standard CMake module support rather than project-specific build scripts where toolchain support is sufficient.
- Treat installed module interface sources as the portable package artifact; do not plan around shipping prebuilt BMI artifacts.

## Scope

- Three exported modules named `lumalink.platform`, `lumalink.platform.windows`, and `lumalink.platform.posix`
- Module-aware CMake target wiring, install rules, and package export behavior
- Test and fixture migration from header includes to module imports
- Platform-specific module entry points for Windows and POSIX host implementations
- Test and sample-consumer coverage for the module import path and any explicitly temporary migration scaffolding
- Documentation of supported compiler and build-system expectations for modules

## Non-Goals

- Do not rewrite core transport, filesystem, or clock behavior as part of the module migration.
- Do not build a permanent dual header-and-module public surface just to preserve existing tests or fixtures.
- Do not convert third-party dependencies or the C++ standard library surface into project-owned wrapper modules unless there is a concrete need.
- Do not expose OS SDK headers from the portable root module interface.
- Do not make module support mandatory for toolchains that cannot reliably build or consume the exported module set yet.
- Do not depend on `import std` for the initial migration.
- Do not ship prebuilt BMI files as part of the supported install contract.
- Do not execute maintained downstream consumer migrations in this backlog; those belong in the consumers' own repos and backlogs.

## Architectural Rules

- Define one authoritative three-module topology with exact names: `lumalink.platform`, `lumalink.platform.windows`, and `lumalink.platform.posix`.
- Keep the portable module graph acyclic; the Windows and POSIX modules may depend on the portable module, but not the other way around.
- Preserve the distinction between portable abstractions under `src/lumalink/platform/` and host-specific implementations under `src/windows/` and `src/posix/`.
- Any temporary bridging surface must exist only to unblock the migration work and should be removed once the in-repo tests are updated.
- Do not turn the current header layout into many peer public modules; buffers, filesystem, transport, and time remain part of the portable public module unless there is a later documented reason to split them.
- Raise the project CMake floor to `3.30` and use CMake-managed `CXX_MODULES` file sets instead of bespoke BMI-management scripts.
- Treat `GCC 15+` with `Ninja 1.11+` as the required canonical module toolchain baseline; `MSVC toolset 14.38+` on Windows is supported but not required.
- Replace the current header-only `INTERFACE` target shape with a real target that owns the module interface unit and any implementation units needed for the exported modules.
- Install module interface sources and export them through CMake package metadata so downstream consumers build their own BMIs with their own toolchains.
- Installed-package behavior must remain deterministic: the supported public contract should be explicit, not an accidental mix of stale headers and module exports.
- The canonical span surface remains `lumalink::span` / `lumalink::dynamic_extent`; do not reintroduce removed span compatibility headers during the module migration.

## Work Phases

## Phase 1 - Module Architecture And Test Migration Strategy

| ID | Status | Task | Depends On | Definition of Done |
|---|---|---|---|---|
| MODULE-01 | done | Define the dependency relationships and exported-surface boundaries for `lumalink.platform`, `lumalink.platform.windows`, and `lumalink.platform.posix` | none | The canonical three-module topology, exact names, and dependency graph are documented and accepted as the public interface plan |
| MODULE-02 | done | Define the migration plan for converting the in-repo tests from header includes to module imports and document the handoff to maintained downstream consumer backlogs | MODULE-01 | A documented plan exists for updating tests and fixtures in this repo while making the downstream migration expectations explicit for consumer-owned backlogs |
| MODULE-03 | done | Document the supported compiler, generator, and CMake feature matrix for module-producing and module-consuming builds using `CMake 3.30`, `GCC 15+`, and `Ninja 1.11+` as the required canonical baseline, with `MSVC toolset 14.38+` on Windows as supported but optional | MODULE-01 | The project has an explicit support policy that sets `CMake 3.30`, `GCC 15+`, and `Ninja 1.11+` as the required module-capable baseline and makes clear that missing `MSVC toolset 14.38+` on Windows is not itself a warning unless no supported compiler is available |
| MODULE-04 | done | Decide the install/export contract around source-installed module interface units, downstream-built BMIs, and any strictly temporary migration scaffolding | MODULE-02, MODULE-03 | The install/export contract is defined clearly enough to drive CMake packaging work without ambiguity, accidental header fallback, or BMI shipping |

## Phase 2 - Portable Public Module Surface

| ID | Status | Task | Depends On | Definition of Done |
|---|---|---|---|---|
| MODULE-05 | todo | Add a top-level `LUMALINK_ENABLE_MODULES` option and CMake wiring for module-aware builds, including a real owning target and `CXX_MODULES` file sets | MODULE-03 | Module compilation is isolated behind the option, uses a module-capable target model, and does not regress unrelated builds when disabled |
| MODULE-06 | todo | Introduce `lumalink.platform` as the public portable module interface and map it to the current top-level entry points | MODULE-01, MODULE-05 | A consumer can import `lumalink.platform` and receive the intended portable public surface without including headers directly |
| MODULE-07 | todo | Organize the portable implementation behind `lumalink.platform` without turning buffers, filesystem, transport, and time into separate public modules | MODULE-06 | The `lumalink.platform` surface imports cleanly, builds without cycles, and keeps the subsystem layering as implementation detail rather than public topology |
| MODULE-08 | todo | Refactor portable headers so they become private implementation-detail includes or are removed once the in-repo tests have moved to `lumalink.platform` imports | MODULE-06, MODULE-07 | Public portable headers no longer carry the primary API-definition burden once modules are enabled |
| MODULE-09 | todo | Update the native test suite to import `lumalink.platform` and verify the intended module-based API directly | MODULE-02, MODULE-08 | Tests pass through `lumalink.platform` imports and no longer depend on legacy public headers as their primary consumption path |

## Phase 3 - Platform-Specific Module Exports

| ID | Status | Task | Depends On | Definition of Done |
|---|---|---|---|---|
| MODULE-10 | todo | Define the entry points and dependency boundaries for `lumalink.platform.windows` and `lumalink.platform.posix` relative to `lumalink.platform` | MODULE-01, MODULE-07 | The platform-specific module names and import rules are documented and do not leak back into the portable dependency graph |
| MODULE-11 | todo | Implement `lumalink.platform.windows` so it exposes the supported native clock, transport, filesystem, and factory surfaces without polluting `lumalink.platform` with Windows SDK headers | MODULE-10, MODULE-08 | Windows-native consumers can import `lumalink.platform.windows` and the portable module remains free of Windows-specific header leakage |
| MODULE-12 | todo | Implement `lumalink.platform.posix` so it exposes the supported native clock, transport, filesystem, and factory surfaces without widening the Windows-specific build path | MODULE-10, MODULE-08 | POSIX-native consumers can import `lumalink.platform.posix` and the platform split remains explicit |
| MODULE-13 | todo | Update the top-level platform selection surface so the existing host-platform factory entry points remain available under the new module strategy | MODULE-11, MODULE-12 | The current host-platform selection behavior is preserved for module consumers and remains testable |

## Phase 4 - Packaging, Tooling, And Downstream Verification

| ID | Status | Task | Depends On | Definition of Done |
|---|---|---|---|---|
| MODULE-14 | todo | Update the `lumalink-platform` CMake target to publish module file sets and the install/export metadata needed for downstream source-based module consumption | MODULE-04, MODULE-05, MODULE-13 | The build target declares and exports the module surface through `CXX_MODULES` file sets in a way supported by the chosen compiler and CMake matrix |
| MODULE-15 | todo | Add an installed-consumer verification path that proves both in-tree and installed-package module consumption work as designed without relying on shipped BMIs | MODULE-14 | A downstream verification project or fixture builds successfully against the installed package using the documented module flow and consumer-built BMIs |
| MODULE-16 | todo | Document consumer setup for modules and the expected handoff to maintained downstream consumer backlogs | MODULE-14, MODULE-15 | The repository documents how to consume the library with modules and makes the downstream migration expectations explicit without owning those migrations here |
| MODULE-17 | todo | Add configure presets or equivalent documented flows for module-enabled native builds | MODULE-05, MODULE-14 | There is a repeatable configure/build flow for module-enabled development and CI verification |

## Phase 5 - Module-First Cutover And Validation

| ID | Status | Task | Depends On | Definition of Done |
|---|---|---|---|---|
| MODULE-18 | todo | Tighten CI and test coverage around the supported module export path and any explicitly retained migration scaffolding | MODULE-09, MODULE-15, MODULE-17 | The supported consumption path is enforced by automation instead of informal manual testing |
| MODULE-19 | todo | Make module export the default public path only after the packaging, documentation, and downstream-verification requirements are met | MODULE-18 | The project can switch to a modules-first posture with in-repo tests already migrated and the downstream consumer handoff documented |

## Phase 6 - Header Removal Exit Gate

| ID | Status | Task | Depends On | Definition of Done |
|---|---|---|---|---|
| MODULE-20 | todo | Identify the remaining public headers and umbrella-header patterns that must be removed before the migration is considered complete | MODULE-19 | There is an explicit removal list covering the legacy public header surface that is still exposed after the module-first cutover |
| MODULE-21 | todo | Remove the legacy public headers and any temporary bridging surfaces after the in-repo tests have fully migrated | MODULE-20 | The supported public surface no longer depends on the legacy header exports and the removed headers are not required by in-repo consumers |
| MODULE-22 | todo | Verify the final no-public-headers end state in CI, packaging, and downstream-consumer checks | MODULE-21 | The work is only considered complete when the module export path succeeds without shipping the legacy public headers as part of the supported interface |

# Upgrading Dependent Libraries For The Pre-Modules Native Layout

Purpose: provide a deterministic migration guide for dependent libraries once
this repository finishes the pre-modules native layout work. This document is
intended to be usable by both humans and agents updating downstream codebases.

Status: draft.

Use this document after the native-layout backlog has landed enough of the new
structure that the old mixed `src/lumalink/platform/` ownership is no longer the
recommended dependency surface.

Until then:

- existing downstream consumers should keep using the current published surface
- this document should be treated as the migration playbook to finalize when the exact destination paths and target names are confirmed

## Upgrade Goal

The migration goal is to move dependent libraries away from relying on the old
mixed platform tree and toward one of two supported consumption styles:

- facade consumption: consume the stable public umbrella and factory surface
- implementation-aware consumption: consume explicit shared headers and, only when truly necessary, explicit native implementation headers

For most downstream libraries, facade consumption should remain the default.
Implementation-aware consumption should be limited to code that intentionally
uses concrete Windows or POSIX implementations.

## Expected Source Layout Direction

The planned destination shape from the native-layout backlog is:

```text
src/lumalink/
  core/
  path/
  buffers/
  filesystem/
  transport/
  time/

  native/
    common/
    windows/
    posix/

  embedded/
    arduino/
    pico/
    esp8266/
    esp32/

  facade/
    LumaLinkPlatform.h
    ClockFactory.h
    TransportFactory.h
```

The exact destination may keep the current public header paths as forwarding
facades for one transition period. When that happens, downstream consumers
should prefer the documented facade surface over chasing internal header moves.

## Recommended Downstream Strategy

Use this decision order when upgrading a dependent library.

### 1. Prefer The Facade First

If the dependent library only needs the public library surface, migrate it to
use the public facade and stop there.

That means:

- prefer `#include "lumalink/LumaLinkPlatform.h"` for broad consumption
- prefer factory headers only when the consumer intentionally depends on the factory surface
- do not keep direct includes of internal transport, time, filesystem, or platform implementation headers unless the code genuinely needs them

A consumer that only uses public contracts should not be reaching into
`windows/`, `posix/`, or other internal implementation roots after the upgrade.

### 2. Use Shared Headers For Contract-Level Includes

If the dependent library includes internal contract headers directly today,
replace those includes with the corresponding shared-root headers once the new
paths exist.

Planned mapping:

| Current include | Planned destination |
|---|---|
| `lumalink/platform/PathMapper.h` | `lumalink/path/PathMapper.h` |
| `lumalink/platform/buffers/Availability.h` | `lumalink/buffers/Availability.h` |
| `lumalink/platform/buffers/ByteStream.h` | `lumalink/buffers/ByteStream.h` |
| `lumalink/platform/filesystem/FileSystem.h` | `lumalink/filesystem/FileSystem.h` |
| `lumalink/platform/transport/TransportInterfaces.h` | `lumalink/transport/TransportInterfaces.h` |
| `lumalink/platform/transport/TransportTraits.h` | `lumalink/transport/TransportTraits.h` |
| `lumalink/platform/time/ClockTypes.h` | `lumalink/time/ClockTypes.h` |
| `lumalink/platform/time/Clock.h` | `lumalink/time/Clock.h` |
| `lumalink/platform/time/ManualClock.h` | `lumalink/time/ManualClock.h` |
| `lumalink/platform/time/TimeSource.h` | `lumalink/time/TimeSource.h` |
| `lumalink/platform/memory/MemoryFileAdapter.h` | `lumalink/filesystem/MemoryFileAdapter.h` or another explicitly documented shared location |

The `MemoryFileAdapter.h` destination is the least certain item in the current
plan. If the repository publishes a different final path for it, follow the
published path map rather than this draft.

### 3. Move Concrete Native Includes Only When The Consumer Really Uses Them

If a dependent library directly includes concrete host implementations today,
move those includes to the corresponding native roots.

Planned mapping:

| Current include | Planned destination |
|---|---|
| `lumalink/platform/windows/WindowsTime.h` | `lumalink/native/windows/WindowsTime.h` |
| `lumalink/platform/windows/WindowsSocketTransport.h` | `lumalink/native/windows/WindowsSocketTransport.h` |
| `lumalink/platform/windows/WindowsFileAdapter.h` | `lumalink/native/windows/WindowsFileAdapter.h` |
| `lumalink/platform/posix/PosixTime.h` | `lumalink/native/posix/PosixTime.h` |
| `lumalink/platform/posix/PosixSocketTransport.h` | `lumalink/native/posix/PosixSocketTransport.h` |
| `lumalink/platform/posix/PosixFileAdapter.h` | `lumalink/native/posix/PosixFileAdapter.h` |

Do not migrate embedded includes as part of this native-layout change unless the
repository publishes a separate embedded migration guide.

### 4. Treat Facade Headers As Compatibility Surfaces

The current factory and umbrella headers are expected to survive as thin facade
surfaces for at least one transition window.

That means downstream libraries should usually prefer:

- `lumalink/LumaLinkPlatform.h`
- `lumalink/platform/ClockFactory.h`
- `lumalink/platform/TransportFactory.h`

If the final restructuring moves those facade headers to new public paths, use
the published facade paths and avoid bypassing them unless the downstream code
has a real implementation-level dependency.

## CMake Consumption Guidance

The current package exports the interface target `lumalink::platform`.
Dependent libraries should continue to prefer the published facade target unless
and until this repository documents a new split-target model.

Use this rule set:

- if the repository still exports `lumalink::platform` as the public target, keep linking that target
- if additional shared or native targets are introduced, consume them only when the repository explicitly documents them as supported downstream targets
- do not guess internal target names from source layout alone
- if a downstream library only needs public headers, it should not need direct knowledge of Windows-native or POSIX-native target partitions

## Agent-Oriented Upgrade Recipe

Use the following workflow when upgrading a dependent library.

### Step 1. Inventory Existing Includes

From the dependent repository root, collect all include sites that reference the
current mixed lumalink tree.

PowerShell examples:

```powershell
Get-ChildItem -Recurse -File -Include *.h,*.hpp,*.c,*.cc,*.cpp,*.ixx |
    Select-String -Pattern 'lumalink/platform/|lumalink/LumaLinkPlatform.h'
```

If the dependent library also uses angle-bracket includes, repeat with any
repository-specific prefixes that appear in its codebase.

### Step 2. Classify Each Include Site

For each result, classify it into one of these buckets:

- facade include
- shared contract include
- concrete Windows include
- concrete POSIX include
- embedded include
- unknown or mixed internal include

If an include site falls into the unknown bucket after the final path map is
published, stop and inspect it manually rather than guessing.

### Step 3. Apply Deterministic Replacements

Perform replacements in this order:

1. Replace broad internal usage with the facade include when the file only needs public surface area.
2. Replace shared contract includes using the shared-root mapping table.
3. Replace concrete Windows and POSIX includes using the native-root mapping table.
4. Leave embedded includes unchanged unless an embedded migration plan is explicitly in scope.

Do not replace platform-specific includes with the umbrella header if the code
really depends on a concrete implementation type. That would hide a real
platform dependency instead of documenting it.

### Step 4. Update Build Files Only If Needed

Inspect the dependent library's CMake files after the include replacements.

Apply changes only if one of these is true:

- the old include directories pointed directly at internal lumalink subtrees
- the repository documents a new public target to replace `lumalink::platform`
- the dependent library intentionally consumes newly published shared or native targets

If none of those are true, avoid unnecessary CMake churn.

### Step 5. Validate The Dependent Library

Run the dependent library's normal configure, build, and test flow.

If the dependent library does not already have a documented validation path,
use this minimum sequence and adapt the preset or build directory as needed:

```powershell
cmake -S . -B build
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

If the library is an app or firmware integration rather than a testable unit,
run its standard integration build instead of inventing a synthetic test step.

### Step 6. Record Residual Internal Dependencies

If the downstream library still depends on concrete native implementation
headers after the upgrade, record that explicitly in its notes or backlog.

That makes later module migration work easier because the implementation-level
dependencies are already known.

## Review Checklist

A downstream upgrade is complete when:

- no obsolete include paths remain from the old mixed tree except documented temporary facades
- the consumer uses the facade surface wherever implementation-level access is unnecessary
- any remaining Windows or POSIX implementation includes are intentional and documented
- the dependent build succeeds
- the dependent tests or integration validation succeed

## Maintainer Notes

Before using this document for broad downstream migrations, refresh these items
against the final repository state:

- the exact destination paths chosen by `MODLAY-03`
- the final location of `MemoryFileAdapter.h`
- whether `ClockFactory.h` and `TransportFactory.h` remain at their current public paths or move under a dedicated facade root
- whether `lumalink::platform` remains the only supported downstream CMake target

Do not treat this draft as authoritative until those items are confirmed.

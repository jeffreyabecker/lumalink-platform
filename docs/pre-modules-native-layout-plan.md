# Pre-Modules Native Layout Plan

This document closes the planning work for backlog items `MODLAY-01`,
`MODLAY-02`, and `MODLAY-03` from
`docs/backlog/task-lists/001-pre-modules-native-layout.md`.

It records the current ownership inventory, the approved pre-modules directory
layout, and the CMake target boundary plan that later structural moves should
follow.

## Current Header Inventory

The current tree under `src/lumalink/` contains 23 headers.

### Public Facade Surface

| Header | Classification | Notes |
|---|---|---|
| `src/lumalink/LumaLinkPlatform.h` | facade umbrella | Broad public include; currently re-exports most shared contracts plus both factory headers |
| `src/lumalink/platform/ClockFactory.h` | facade selector | Owns platform selection for clock aliases via preprocessor includes |
| `src/lumalink/platform/TransportFactory.h` | facade selector | Owns platform selection for transport factories via preprocessor includes |

### Shared Contract And Helper Surface

| Header | Classification | Approved destination |
|---|---|---|
| `src/lumalink/platform/PathMapper.h` | shared helper/contract | `src/lumalink/path/PathMapper.h` |
| `src/lumalink/platform/buffers/Availability.h` | shared helper | `src/lumalink/buffers/Availability.h` |
| `src/lumalink/platform/buffers/ByteStream.h` | shared helper/contract | `src/lumalink/buffers/ByteStream.h` |
| `src/lumalink/platform/filesystem/FileSystem.h` | shared filesystem contract | `src/lumalink/filesystem/FileSystem.h` |
| `src/lumalink/platform/memory/MemoryFileAdapter.h` | shared in-memory filesystem utility | `src/lumalink/filesystem/MemoryFileAdapter.h` |
| `src/lumalink/platform/time/ClockTypes.h` | shared time contract | `src/lumalink/time/ClockTypes.h` |
| `src/lumalink/platform/time/Clock.h` | shared time contract | `src/lumalink/time/Clock.h` |
| `src/lumalink/platform/time/ManualClock.h` | shared testable clock utility | `src/lumalink/time/ManualClock.h` |
| `src/lumalink/platform/time/TimeSource.h` | shared time contract | `src/lumalink/time/TimeSource.h` |
| `src/lumalink/platform/transport/TransportInterfaces.h` | shared transport contract | `src/lumalink/transport/TransportInterfaces.h` |
| `src/lumalink/platform/transport/TransportTraits.h` | shared transport helper | `src/lumalink/transport/TransportTraits.h` |

### Native Windows Surface

| Header | Classification | Approved destination |
|---|---|---|
| `src/lumalink/platform/windows/WindowsTime.h` | Windows-native implementation | `src/lumalink/native/windows/WindowsTime.h` |
| `src/lumalink/platform/windows/WindowsSocketTransport.h` | Windows-native implementation | `src/lumalink/native/windows/WindowsSocketTransport.h` |
| `src/lumalink/platform/windows/WindowsFileAdapter.h` | Windows-native implementation | `src/lumalink/native/windows/WindowsFileAdapter.h` |

### Native POSIX Surface

| Header | Classification | Approved destination |
|---|---|---|
| `src/lumalink/platform/posix/PosixTime.h` | POSIX-native implementation | `src/lumalink/native/posix/PosixTime.h` |
| `src/lumalink/platform/posix/PosixSocketTransport.h` | POSIX-native implementation | `src/lumalink/native/posix/PosixSocketTransport.h` |
| `src/lumalink/platform/posix/PosixFileAdapter.h` | POSIX-native implementation | `src/lumalink/native/posix/PosixFileAdapter.h` |

### Embedded Deferred Surface

| Header | Classification | Decision |
|---|---|---|
| `src/lumalink/platform/arduino/ArduinoTime.h` | embedded implementation | Remains under `src/lumalink/platform/arduino/` during the native-layout backlog |
| `src/lumalink/platform/arduino/ArduinoWiFiTransport.h` | embedded implementation | Remains under `src/lumalink/platform/arduino/` during the native-layout backlog |
| `src/lumalink/platform/arduino/ArduinoFileAdapter.h` | embedded implementation | Remains under `src/lumalink/platform/arduino/` during the native-layout backlog |

## Dependency Observations From The Current Tree

- `LumaLinkPlatform.h` is a broad aggregator and currently pulls in both shared
  contracts and the platform-selecting factory headers.
- `ClockFactory.h` and `TransportFactory.h` both include shared headers using
  relative paths and then select concrete native implementations with platform
  preprocessor branches.
- `MemoryFileAdapter.h` is not platform-specific. It implements an in-memory
  filesystem for tests and host-side utilities, and it depends only on shared
  filesystem and buffer contracts. It should move with the shared filesystem
  surface rather than into a native platform root.
- The current top-level CMake target exposes the entire `src` tree as a single
  include root. That makes all platform headers reachable to any consumer even
  when only one native family should participate in a build.

## Approved Destination Layout

The pre-modules layout will move concrete ownership into explicit shared and
native roots while keeping the current umbrella and factory paths as temporary
facades.

```text
src/lumalink/
  LumaLinkPlatform.h                    # retained as umbrella facade
  path/
    PathMapper.h
  buffers/
    Availability.h
    ByteStream.h
  filesystem/
    FileSystem.h
    MemoryFileAdapter.h
  transport/
    TransportInterfaces.h
    TransportTraits.h
  time/
    ClockTypes.h
    Clock.h
    ManualClock.h
    TimeSource.h
  native/
    common/                            # added only when host-shared helpers exist
    windows/
      WindowsTime.h
      WindowsSocketTransport.h
      WindowsFileAdapter.h
    posix/
      PosixTime.h
      PosixSocketTransport.h
      PosixFileAdapter.h
  platform/
    ClockFactory.h                     # retained facade/selector during migration
    TransportFactory.h                 # retained facade/selector during migration
    arduino/                           # intentionally deferred embedded path
```

### Naming Decisions

- Shared contracts move to direct roots under `src/lumalink/` instead of living
  under another compatibility namespace.
- Native host implementations move under `src/lumalink/native/` with explicit
  `windows/` and `posix/` children.
- No separate `facade/` root will be introduced. Existing public paths remain
  the compatibility layer during the transition.
- `MemoryFileAdapter.h` is treated as part of the shared filesystem surface,
  not as a standalone `memory/` root.

## Approved CMake Target Boundary Plan

The implementation work should move toward the following target graph.

```text
lumalink-platform-shared
  owns: shared contract/helper headers under path(), buffers(), filesystem(),
        transport(), time()

lumalink-platform-native-common
  owns: host-only helpers shared by Windows and POSIX
  links: lumalink-platform-shared
  note: create only once such helpers actually exist

lumalink-platform-native-windows
  owns: native/windows/ headers and any future sources
  links: lumalink-platform-shared, lumalink-platform-native-common

lumalink-platform-native-posix
  owns: native/posix/ headers and any future sources
  links: lumalink-platform-shared, lumalink-platform-native-common

lumalink-platform-embedded
  owns: platform/arduino/ headers
  links: lumalink-platform-shared

lumalink-platform
  public compatibility target
  links: lumalink-platform-shared plus exactly one selected native or embedded
         implementation family
```

### Include Path Rules

- `lumalink-platform-shared` exposes `src/` and eventually only the shared
  roots needed for public contracts.
- `lumalink-platform-native-windows` exposes only the include paths required for
  shared roots plus `src/lumalink/native/windows` ownership.
- `lumalink-platform-native-posix` exposes only the include paths required for
  shared roots plus `src/lumalink/native/posix` ownership.
- `lumalink-platform` remains the compatibility consumption target during the
  transition so downstream code does not need to switch targets immediately.
- Tests should link against `lumalink::platform` during the transition, but the
  implementation of that target should stop acting as a single undifferentiated
  owner of the whole tree.

## Validation Command For Structural Moves

Each structural change in the native-host path should be validated with the
existing native CTest lane driven from `test/test_native`.

The expected validation command is the native CMake test run for
`lumalink_platform_tests` after a successful configure/build.

## Consequences For Later Backlog Phases

- `MODLAY-04` should move shared contracts first and leave forwarding headers in
  the current `platform/` paths where necessary.
- `MODLAY-05` should only create `native/common/` if real host-shared helpers
  emerge; the directory exists in the plan to prevent naming churn later, not
  because the current tree already requires it.
- `MODLAY-06` and `MODLAY-10` should keep `LumaLinkPlatform.h`,
  `ClockFactory.h`, and `TransportFactory.h` at their current public paths until
  the downstream migration document is complete and validated.
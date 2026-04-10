# C++ Modules Design Sketch

## Status

Exploratory design only. This sketches what the library could look like if C++
modules were adopted for the native-host build path.

This design is anchored in the current public umbrella header at
[src/lumalink/LumaLinkPlatform.h](../src/lumalink/LumaLinkPlatform.h), and the
current platform-selection model in
[src/lumalink/platform/ClockFactory.h](../src/lumalink/platform/ClockFactory.h)
and
[src/lumalink/platform/TransportFactory.h](../src/lumalink/platform/TransportFactory.h).

## Summary

If modules were implemented here, the clean design is not "one module for the
whole library" and not literally "only one built module per platform."

The likely outcome is:

- a small set of shared, platform-agnostic modules
- one platform implementation module set per native platform
- one thin facade module for the active native platform
- headers retained for compatibility and embedded consumers

In practice that means Windows builds would compile the shared modules plus
Windows platform modules, and POSIX builds would compile the shared modules plus
POSIX platform modules.

## Goals

- Reduce transitive parsing from the current header-heavy surface.
- Make platform boundaries explicit instead of hiding them behind broad include chains.
- Keep the current runtime and API behavior stable for consumers.
- Improve native build hygiene under CMake and modern compilers.
- Preserve a non-module path for consumers and targets that cannot use modules yet.

## Non-Goals

- Do not make modules the only supported consumption model immediately.
- Do not force Arduino, Pico, ESP8266, or ESP32 targets onto modules in the first iteration.
- Do not collapse all platform code into a single giant module with heavy `#if` branching.
- Do not redesign the transport, filesystem, or clock abstractions as part of the initial module rollout.
- Do not remove existing public headers in the first phase.

## Current Constraints

The current codebase has three characteristics that strongly shape the module
design:

- Platform selection is macro-driven in [src/lumalink/platform/ClockFactory.h](../src/lumalink/platform/ClockFactory.h) and [src/lumalink/platform/TransportFactory.h](../src/lumalink/platform/TransportFactory.h).
- The umbrella include in [src/lumalink/LumaLinkPlatform.h](../src/lumalink/LumaLinkPlatform.h) pulls a broad surface into every translation unit.
- Platform implementations depend on system headers that are mutually platform-specific, especially the socket and time layers.

Those constraints make a single all-platform module unattractive. The likely
design is shared core modules plus per-platform implementation modules.

## Proposed Module Architecture

### 1. Shared Core Modules

These modules contain platform-agnostic contracts, value types, and helpers.

Suggested module set:

- `lumalink.core`
- `lumalink.path`
- `lumalink.buffers`
- `lumalink.filesystem`
- `lumalink.transport`
- `lumalink.time`

Suggested contents:

- `lumalink.core`
  - common type aliases
  - minimal shared traits and utility types
- `lumalink.path`
  - the path-mapper surface currently centered around [src/lumalink/platform/PathMapper.h](../src/lumalink/platform/PathMapper.h)
- `lumalink.buffers`
  - byte-stream interfaces and helpers from [src/lumalink/platform/buffers/ByteStream.h](../src/lumalink/platform/buffers/ByteStream.h)
  - availability/result types from the buffer layer
- `lumalink.filesystem`
  - `IFile`, `IFileSystem`, `DirectoryEntry`, and related contracts from [src/lumalink/platform/filesystem/FileSystem.h](../src/lumalink/platform/filesystem/FileSystem.h)
- `lumalink.transport`
  - transport interfaces and traits from [src/lumalink/platform/transport/TransportInterfaces.h](../src/lumalink/platform/transport/TransportInterfaces.h) and related traits headers
- `lumalink.time`
  - clock/time value types and interfaces from the time headers

### 2. Native Platform Implementation Modules

These modules expose concrete implementations for the host platform.

Suggested module set:

- `lumalink.platform.posix.time`
- `lumalink.platform.posix.transport`
- `lumalink.platform.posix.filesystem`
- `lumalink.platform.windows.time`
- `lumalink.platform.windows.transport`
- `lumalink.platform.windows.filesystem`

These modules would import the shared contracts and provide the existing
concrete implementations now spread across the platform headers.

### 3. Thin Native Facade Module

A small facade module would represent "the current native platform build."

Suggested name:

- `lumalink.native`

Its job would be:

- re-export shared core modules
- re-export the active native platform implementation modules
- expose aliases equivalent to the current native selections in the factory headers

That means:

- on Windows, `lumalink.native` re-exports Windows implementation modules
- on Linux/POSIX, `lumalink.native` re-exports POSIX implementation modules

### 4. Header Compatibility Layer

Existing headers stay in place initially.

The first version should support:

- module consumers using `import lumalink.native;`
- existing consumers continuing to `#include <LumaLinkPlatform.h>`

That preserves downstream compatibility while the module path matures.

## Proposed Artifact Model

This is the most likely build layout.

```text
Native Windows build:
  shared modules
  + windows platform modules
  + optional facade module lumalink.native

Native POSIX build:
  shared modules
  + posix platform modules
  + optional facade module lumalink.native

Embedded builds:
  existing header path only
```

So the practical answer is: yes, you would probably end up with one native
implementation module set per platform, but not only one module per platform.
You would still have shared core modules compiled alongside the platform
modules.

## Source Layout Sketch

One reasonable layout would be:

```text
src/modules/
  lumalink.core.ixx
  lumalink.path.ixx
  lumalink.buffers.ixx
  lumalink.filesystem.ixx
  lumalink.transport.ixx
  lumalink.time.ixx

  lumalink.platform.posix.time.ixx
  lumalink.platform.posix.transport.ixx
  lumalink.platform.posix.filesystem.ixx

  lumalink.platform.windows.time.ixx
  lumalink.platform.windows.transport.ixx
  lumalink.platform.windows.filesystem.ixx

  lumalink.native.ixx
```

An alternative is module partitions, but I would not start there. Separate
interface units are easier to reason about and easier to wire in CMake.

## CMake Model

The module design fits best if the native-host build is split into distinct
targets rather than one target with a mixed all-platform source set.

Suggested targets:

- `lumalink-platform-headers`
- `lumalink-platform-modules-common`
- `lumalink-platform-modules-native`

Suggested behavior:

- `lumalink-platform-headers` remains the compatibility target
- `lumalink-platform-modules-common` contains shared module interface units
- `lumalink-platform-modules-native` adds either Windows or POSIX module sources based on the host platform
- embedded targets do not consume the module targets initially

A rough CMake shape would be:

```cmake
add_library(lumalink-platform-modules-common)
target_sources(lumalink-platform-modules-common
  PUBLIC
    FILE_SET CXX_MODULES FILES
      src/modules/lumalink.core.ixx
      src/modules/lumalink.path.ixx
      src/modules/lumalink.buffers.ixx
      src/modules/lumalink.filesystem.ixx
      src/modules/lumalink.transport.ixx
      src/modules/lumalink.time.ixx
)

add_library(lumalink-platform-modules-native)
target_link_libraries(lumalink-platform-modules-native
  PUBLIC lumalink-platform-modules-common
)

if(WIN32)
  target_sources(lumalink-platform-modules-native
    PUBLIC
      FILE_SET CXX_MODULES FILES
        src/modules/lumalink.platform.windows.time.ixx
        src/modules/lumalink.platform.windows.transport.ixx
        src/modules/lumalink.platform.windows.filesystem.ixx
        src/modules/lumalink.native.ixx
  )
else()
  target_sources(lumalink-platform-modules-native
    PUBLIC
      FILE_SET CXX_MODULES FILES
        src/modules/lumalink.platform.posix.time.ixx
        src/modules/lumalink.platform.posix.transport.ixx
        src/modules/lumalink.platform.posix.filesystem.ixx
        src/modules/lumalink.native.ixx
  )
endif()
```

## Public Consumption Model

### Existing Header Consumer

```cpp
#include <LumaLinkPlatform.h>
```

This keeps working unchanged.

### Native Module Consumer

```cpp
import lumalink.native;

int main() {
    lumalink::platform::TransportFactory transportFactory;
    lumalink::platform::Clock clock;
}
```

### Fine-Grained Consumer

A more advanced consumer could import only what it needs:

```cpp
import lumalink.buffers;
import lumalink.transport;
import lumalink.platform.posix.transport;
```

That is useful for native-only internal code, but it should not be the first
public recommendation.

## Mapping From Current Headers

A reasonable initial mapping would be:

- [src/lumalink/platform/PathMapper.h](../src/lumalink/platform/PathMapper.h) -> `lumalink.path`
- [src/lumalink/platform/buffers/ByteStream.h](../src/lumalink/platform/buffers/ByteStream.h) -> `lumalink.buffers`
- [src/lumalink/platform/filesystem/FileSystem.h](../src/lumalink/platform/filesystem/FileSystem.h) -> `lumalink.filesystem`
- [src/lumalink/platform/transport/TransportInterfaces.h](../src/lumalink/platform/transport/TransportInterfaces.h) -> `lumalink.transport`
- time headers under [src/lumalink/platform/time](../src/lumalink/platform/time) -> `lumalink.time`
- [src/lumalink/platform/posix/PosixSocketTransport.h](../src/lumalink/platform/posix/PosixSocketTransport.h) -> `lumalink.platform.posix.transport`
- [src/lumalink/platform/windows/WindowsSocketTransport.h](../src/lumalink/platform/windows/WindowsSocketTransport.h) -> `lumalink.platform.windows.transport`

The factory headers are the awkward part. They should eventually become:

- small header wrappers over module exports, or
- module-provided aliases re-exposed through compatibility headers

## Transition Plan

### Phase A: Core Contracts

- Create shared modules for buffers, filesystem, transport, time, and path mapping.
- Do not change behavior.
- Keep all existing headers.

### Phase B: Native Platform Modules

- Add Windows and POSIX implementation modules.
- Keep platform headers as thin compatibility wrappers.
- Do not touch embedded targets yet.

### Phase C: Native Facade

- Add `lumalink.native`.
- Make it the recommended native module import surface.
- Keep the umbrella header for compatibility.

### Phase D: Optional Header Thinning

- Convert heavyweight compatibility headers into minimal forwarding layers.
- Reassess whether the factory headers should remain macro-driven or become per-target configured exports.

## Risks

- Preprocessor-selected platform aliases do not translate naturally into a single module interface.
- Windows system headers and macro behavior can still be awkward, even inside module units.
- GCC and MSVC module behavior is good enough for experiments, but still not friction-free across all workflows.
- Downstream consumers using `add_subdirectory` may not be ready for module-aware builds.
- Embedded toolchains remain the main blocker to "modules everywhere."

## Recommendation

If this were implemented, the right first target is:

- native-host only
- shared core modules plus per-platform native modules
- headers retained as the stable compatibility surface

I would not start with:

- a single all-platform umbrella module
- embedded module support
- immediate header removal

The cleanest initial goal is to make Windows and POSIX native builds
module-aware while preserving the existing header-based library contract.

## Open Questions

- Should `lumalink.native` expose the existing alias names exactly, or only re-export module members and leave aliasing to headers?
- Should filesystem implementations become modules in the first wave, or wait until transport/time prove the model?
- Should the module targets be opt-in initially, for example behind `LUMALINK_ENABLE_MODULES`?
- Is the desired downstream story "modules for native app builds only" or "eventual first-class public module API"?

If you want, I can turn this into a repository-ready backlog/design entry next, or
tighten it into a shorter ADR-style document.
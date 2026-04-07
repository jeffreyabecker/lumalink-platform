# Platform Extraction Todo

## Purpose

This checklist tracks the concrete work required to turn `c:\ode\lumalink-platform` from a scaffold plus raw imports into the actual home of the platform layer extracted from `pico-httpserveradvanced`.

This file assumes the current plan is still in force:

- `lumalink::http` depends on `lumalink::platform`
- `lumalink::platform` owns buffers, transport, and filesystem concerns only
- compile-time platform selection remains, but lives in `lumalink::platform`
- adapter-specific tests move into this repository
- the old repository keeps its local platform code only until this repository is validated and consumable

## Current State

- The scaffold exists and has an initial commit.
- A placeholder native test lane passes.
- A raw import of the current `httpadv/v1/platform` header tree and immediate support headers exists under `src/httpadv/v1/`.
- Platform-focused native tests were copied into `test/imported_http_platform/` and are currently excluded from the active test lane.
- The new public `lumalink/platform/...` include surface is still placeholder-only.

## Phase 1: Stabilize The Imported Snapshot

- [x] Add `build/` to `.gitignore` so repository status stays clean after local validation.
- [x] Treat the raw-imported `src/httpadv/v1/` tree as a temporary staging area and move it to `src/imported/httpadv/v1/` quarantine path.
- [x] Record the exact imported file set from the old repository in `docs/imported-file-list.txt` so future diffs can distinguish copy-only changes from semantic rewrites.
- [x] Confirm that the imported platform slice is limited to platform-owned code and immediate transport/util/core support headers (manual audit recorded in docs).
- [x] Commit the current raw-import snapshot before namespace and include rewrites begin.

## Phase 2: Build The Real Contract Surface

### Buffers

- [x] Decide which imported byte-stream and availability types become the public `lumalink/platform/buffers/` contract. (Decision: expose all imported types as public)
- [ ] Create real headers under `src/lumalink/platform/buffers/` for the chosen contract surface.
- [ ] Move or rewrite the required pieces from `src/httpadv/v1/transport/Availability.h` and `src/httpadv/v1/transport/ByteStream.h` into the new contract headers.
- [ ] Rewrite namespaces from `httpadv::v1::transport` to the target `lumalink::platform` buffer namespace.
- [ ] Remove any accidental HTTP-specific helpers from the buffer contract if they are not needed by platform code.

### Transport

- [ ] Define the final transport contract headers under `src/lumalink/platform/transport/`.
- [ ] Port `IClient`, `IServer`, `IPeer`, `ITransportFactory`, and factory traits from the imported transport headers into the new namespace.
- [ ] Decide whether `TransportTraits.h` remains public or becomes an internal support header.
- [ ] Eliminate the old `httpadv/v1` include relationships from the transport contract layer.
- [ ] Make compile-time factory selection depend only on platform-owned headers, not on HTTP-owned headers.

### Filesystem

- [ ] Define the final filesystem contract headers under `src/lumalink/platform/filesystem/`.
- [ ] Port `IFile`, `IFileSystem`, `FileOpenMode`, `DirectoryEntry`, and related callback types into the new namespace.
- [ ] Decide whether path abstraction types belong in filesystem or a narrower internal utility area.
- [ ] Remove any HTTP-specific assumptions from the filesystem contract surface.

### Shared Utility Support

- [ ] Replace imported `httpadv::v1::util::span` usage with the new repository-owned namespace and include paths.
- [x] Decide whether `Span.h` and `span.h` remain public or become internal support for the public contract headers. (Decision updated: expose `lumalink::span` as the stable public symbol, implemented in `src/lumalink/span.h`)
- [ ] Audit `Defines.h` and split out anything that is actually HTTP-specific versus truly platform-owned.
- [ ] Create a platform-owned constants/configuration header if socket/file adapters still need shared compile-time constants.

## Phase 3: Migrate Concrete Implementations

### Path Mapping

- [ ] Move `src/httpadv/v1/platform/PathMapper.h` into the new `lumalink::platform` namespace.
- [ ] Decide whether path mapping is a public filesystem helper or an internal implementation detail.
- [ ] Update all adapter code to include the new path-mapper location.

### Transport Implementations

- [ ] Rewrite `src/httpadv/v1/platform/TransportFactory.h` into the new namespace and include layout.
- [x] Port `src/httpadv/v1/platform/posix/PosixSocketTransport.h` into the new tree.
- [x] Port `src/httpadv/v1/platform/windows/WindowsSocketTransport.h` into the new tree.
- [x] Port `src/httpadv/v1/platform/arduino/ArduinoWiFiTransport.h` into the new tree.
- [ ] Keep compile-time platform selection in this repository, but ensure it no longer depends on the old HTTP umbrella header layout.

### Filesystem Implementations

- [x] Port `src/httpadv/v1/platform/posix/PosixFileAdapter.h` into the new tree.
- [x] Port `src/httpadv/v1/platform/windows/WindowsFileAdapter.h` into the new tree.
- [x] Port `src/httpadv/v1/platform/arduino/ArduinoFileAdapter.h` into the new tree.
- [x] Port `src/httpadv/v1/platform/memory/MemoryFileAdapter.h` into the new tree.
- [ ] Decide on the final namespace layout for concrete adapters, for example whether `lumalink::platform::posix` and similar namespaces remain public.

### Cleanup Of Raw Imports

- [x] Remove copied headers from the temporary `src/httpadv/v1/` import tree once equivalent `lumalink/platform/...` headers exist. (Note: `src/imported` still contains non-platform files and was not removed entirely.)
- [ ] Confirm that no public includes in this repo still require `httpadv/v1` paths.
- [ ] Confirm that no source in this repo still declares `httpadv::v1` namespaces.

## Phase 4: Make The Imported Tests Real

### Test Harness Restructure

- [ ] Decide whether the imported tests should move from `test/imported_http_platform/` into `test/test_native/` once they no longer depend on old HTTP headers.
- [ ] Create or adapt a local consolidated test runner for platform-only suites if the old HTTP consolidated runner shape is still useful.
- [ ] Remove `test_ignore = imported_http_platform` from `platformio.ini` once the imported suites are runnable.

### Shared Support Cleanup

- [ ] Rewrite copied support headers in `test/support/include/` so they include platform-owned headers from this repository rather than `../../../src/httpadv/v1/...`.
- [ ] Remove support helpers that only exist for HTTP-domain tests and are not needed by the platform suites.
- [ ] Keep the minimal Arduino shim in `test/support/native_arduino/` only as long as imported tests still require it.

### Suite Migration

- [ ] Rewrite `test_transport_native.cpp` to compile against the new transport contract and concrete transport implementations in this repository.
- [ ] Rewrite `test_filesystem_posix.cpp` to compile against the new filesystem contract and POSIX adapter location.
- [ ] Rewrite `test_memory_filesystem.cpp` to compile against the new memory filesystem implementation location.
- [ ] Split any HTTP-specific assertions out of those suites if they are not truly platform-owned.
- [ ] Add new platform-specific regression tests where the imported suites currently rely on broader HTTP includes than they actually need.

### Validation

- [ ] Make `pio test -e native` run the real extracted platform suites, not just the placeholder scaffold tests.
- [ ] Ensure the native lane passes on the supported host environment after the suite migration.
- [ ] Add follow-up validation strategy notes for Windows and Arduino-specific build coverage if they are not immediately runnable in this repo.

## Phase 5: Finalize Public Surface

- [x] Replace the placeholder `src/LumaLinkPlatform.h` umbrella header with one that exports the real contract surface.
- [ ] Replace the placeholder contract-name constants in `src/lumalink/platform/...` with actual public APIs.
- [ ] Update `README.md` to describe the real public layout and current supported implementations.
- [ ] Update `library.json`, `library.properties`, and `CMakeLists.txt` if the final public header names or folder layout differ from the scaffold.
- [ ] Decide whether this repository should expose only contracts publicly or also expose concrete adapter headers as public entrypoints.

## Phase 6: Prepare Integration Back To pico-httpserveradvanced

- [ ] Identify the first minimal dependency slice that `pico-httpserveradvanced` can consume from this repo.
- [ ] Confirm the include paths the HTTP repository should switch to first.
- [ ] Confirm the package/dependency story for PlatformIO and any other build systems used by the HTTP repository.
- [ ] Confirm when the HTTP repository can stop shipping its local platform headers and tests.
- [ ] Keep a compatibility checklist for the downstream switch so the extraction can happen in small verified steps.

## Suggested First Implementation Slice

- [ ] Rewrite `Availability.h`, `ByteStream.h`, `TransportInterfaces.h`, `ITransportFactory.h`, and `IFileSystem.h` into the new namespace and include layout.
- [ ] Rewrite `PathMapper.h` into the new namespace.
- [ ] Rewrite `TransportFactory.h` into the new namespace.
- [ ] Update the placeholder public headers to export those real contracts.
- [ ] Rewrite the imported memory filesystem suite first, since it is the most self-contained adapter test.
- [ ] Commit that slice before attempting the POSIX, Windows, or Arduino adapter rewrites.
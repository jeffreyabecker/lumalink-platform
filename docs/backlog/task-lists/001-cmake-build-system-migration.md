# CMake Build System Migration Backlog

Purpose: define and sequence the work needed to replace PlatformIO with a CMake-based build and test system, targeting MSVC on Windows and GCC on Linux for native development, while keeping the library consumable as a CMake subdirectory or installed package by downstream applications.

Embedded targets are explicitly out of scope for this backlog. Arduino SDK dependencies are to be replaced with the appropriate native platform SDK when those platform layers are implemented.

Status legend:
- `todo`: not started
- `doing`: active
- `done`: completed
- `blocked`: waiting on dependency
- `deferred`: intentionally postponed

## Implementation Status

Current status: not started.

The repository ships a minimal stub `CMakeLists.txt` that declares `lumalink-platform` as a header-only `INTERFACE` library and sets `cxx_std_17`. This stub is sufficient for downstream consumption of the headers but does not build or run any tests.

PlatformIO remains the active build and test driver. Tests are unity-based and compiled through PlatformIO's native environment. A `tools/link_winsock.py` extra-script handles `ws2_32` linkage on Windows and a `tools/run_native_tests.ps1` wrapper invokes `pio test -e native`.

What is needed is a full CMake build graph that:
- compiles and links the unity-based native test suite under MSVC on Windows and GCC on Linux
- applies the correct platform link requirements (`ws2_32` on Windows, POSIX socket headers on Linux) without PlatformIO scripts
- exposes `lumalink-platform` as a proper CMake library target that downstream applications can consume via `add_subdirectory` or `find_package`
- replaces or migrates the PowerShell test runner to invoke CMake/CTest directly

## Design Intent

- CMake is the single build system for native development going forward.
- PlatformIO is removed once CMake covers the full native test surface.
- The library remains header-only; no source files need to be compiled into the library target itself.
- Test executables are standalone binaries and are registered with CTest so `ctest` is the canonical test runner.
- Compiler selection follows platform convention: MSVC on Windows, GCC on Linux. No cross-compiler configuration is required by this backlog.
- When Arduino-dependent platform headers are eventually replaced, the native SDK headers will be dropped in directly; the CMake build graph should not couple itself to Arduino toolchain assumptions.
- Downstream integration is the primary contract: a consumer must be able to add this repo as a subdirectory and link against `lumalink::platform` with no additional setup.

## Scope

- CMake build graph for the native test suite (`test/test_native/`)
- CTest registration and invocation replacing `run_native_tests.ps1`
- MSVC-specific and GCC-specific compiler flags and link requirements
- `lumalink::platform` CMake target with correct interface include paths and feature requirements
- CMake install rules and namespace alias so the library is consumable as both a subdirectory and a found package
- Removal of PlatformIO configuration files after CMake coverage is confirmed

## Non-Goals

- Do not add CMake support for embedded or Arduino targets
- Do not implement cross-compilation toolchain files
- Do not migrate away from unity as the test framework (unity is used directly, not via PlatformIO)
- Do not add new platform implementations as part of this migration
- Do not restructure the source tree layout as part of this migration
- Do not add CPack or packaging configuration beyond basic install rules

## Architectural Rules

- The `lumalink-platform` library target must be `INTERFACE` (header-only); no object compilation belongs in the library target itself.
- Compiler and linker specifics belong inside `target_compile_options` and `target_link_libraries` calls scoped to the test executable, not injected into the interface library.
- Windows socket linkage (`ws2_32`) must be expressed as a generator-expression-guarded `target_link_libraries` on the test target, not as a global or toolchain variable.
- All test executables must be registered via `add_test` so CTest discovers them without custom scripts.
- The exported target name must follow the `lumalink::platform` namespace convention so downstream `target_link_libraries` calls use a namespaced alias.
- Install include paths must be expressed relative to `CMAKE_INSTALL_PREFIX`, not as absolute paths from the source tree.

## Work Phases

## Phase 1 - Library Target And Downstream Consumability

| ID | Status | Task | Depends On | Definition of Done |
|---|---|---|---|---|
| CMAKE-01 | todo | Expand `CMakeLists.txt` to declare a proper `lumalink::platform` alias for the interface library | none | `add_subdirectory` from a downstream project and `target_link_libraries(app lumalink::platform)` compiles and resolves headers correctly |
| CMAKE-02 | todo | Add CMake install rules for the interface library with correct include destination | CMAKE-01 | `cmake --install` places headers under `include/` and exports a `lumalinkPlatformConfig.cmake` that a downstream `find_package(lumalinkPlatform)` can consume |
| CMAKE-03 | todo | Add a `lumalinkPlatformConfig.cmake.in` and configure it as part of the install step | CMAKE-02 | An installed copy of the library is locatable via `find_package` and exposes the `lumalink::platform` target |
| CMAKE-04 | todo | Verify the installed target works from a minimal out-of-tree consumer project | CMAKE-03 | A trivial `add_executable` that links `lumalink::platform` after `find_package` builds cleanly on Windows with MSVC |

## Phase 2 - Native Test Executable Under CMake

| ID | Status | Task | Depends On | Definition of Done |
|---|---|---|---|---|
| CMAKE-05 | todo | Locate or fetch unity as a CMake dependency (source copy or FetchContent) | none | `unity.h` and `unity.c` are available to the test executable CMake target without PlatformIO involvement |
| CMAKE-06 | todo | Add a `test/test_native/CMakeLists.txt` that builds the native test executable | CMAKE-01, CMAKE-05 | The test executable compiles all files in `test/test_native/` and links against `lumalink::platform` and unity |
| CMAKE-07 | todo | Apply Windows-specific link flags (`ws2_32`) inside the test CMake target using generator expressions | CMAKE-06 | The test executable links on Windows without the `link_winsock.py` extra-script |
| CMAKE-08 | todo | Apply Linux-specific compiler and link flags (POSIX socket headers are implicit; confirm no extra link flags needed) | CMAKE-06 | The test executable compiles and links on Linux under GCC without errors or PlatformIO involvement |
| CMAKE-09 | todo | Include `test/support/include/` in the test executable's include path | CMAKE-06 | Headers under `test/support/include/` resolve during compilation without manual include path manipulation |
| CMAKE-10 | todo | Register the test executable with CTest via `add_test` | CMAKE-06 | `ctest --test-dir build` discovers and executes all unity tests; pass/fail is reported per test binary |

## Phase 3 - Compiler Configuration

| ID | Status | Task | Depends On | Definition of Done |
|---|---|---|---|---|
| CMAKE-11 | todo | Set the C++ standard to C++17 for all targets without PlatformIO's `-std=gnu++17` flag | CMAKE-06 | All targets compile under C++17 using standard CMake `target_compile_features(... cxx_std_17)` |
| CMAKE-12 | todo | Add MSVC-specific warning and conformance flags to the test executable target | CMAKE-11 | MSVC builds compile without unnecessary warnings; `/W3` or `/W4` is set; permissive mode is off |
| CMAKE-13 | todo | Add GCC-specific warning flags to the test executable target | CMAKE-11 | GCC builds compile with `-Wall -Wextra`; no PlatformIO-injected flags remain |
| CMAKE-14 | todo | Confirm that `_WIN32` and POSIX platform guards in source already work correctly under the new compiler invocations | CMAKE-12, CMAKE-13 | Existing `#if defined(_WIN32)` / `#else` guards in transport and filesystem tests select the right implementation on each platform |

## Phase 4 - Test Runner And CI Integration

| ID | Status | Task | Depends On | Definition of Done |
|---|---|---|---|---|
| CMAKE-15 | todo | Replace `tools/run_native_tests.ps1` with a script that configures, builds, and runs tests via CMake and CTest | CMAKE-10 | A single PowerShell script (or a documented `cmake` / `ctest` invocation) replaces `pio test -e native` as the canonical test command |
| CMAKE-16 | todo | Document the build and test workflow in `README.md` or a `docs/` build guide | CMAKE-15 | A developer can clone the repo, run the documented commands, and have all native tests pass without installing PlatformIO |
| CMAKE-17 | todo | Verify the full test suite passes under MSVC on Windows using the new CMake build | CMAKE-10, CMAKE-14 | All tests that pass under PlatformIO also pass under CTest with MSVC |
| CMAKE-18 | todo | Verify the full test suite passes under GCC on Linux using the new CMake build | CMAKE-10, CMAKE-14 | All tests that pass under PlatformIO also pass under CTest with GCC |

## Phase 5 - PlatformIO Removal

| ID | Status | Task | Depends On | Definition of Done |
|---|---|---|---|---|
| CMAKE-19 | todo | Remove `platformio.ini` once CMake build and test coverage is confirmed | CMAKE-17, CMAKE-18 | `platformio.ini` is deleted; no remaining tooling references PlatformIO for native builds |
| CMAKE-20 | todo | Remove `tools/link_winsock.py` once the CMake build applies winsock linkage directly | CMAKE-07 | The Python extra-script is deleted; winsock linkage is handled exclusively by the CMake target definition |
| CMAKE-21 | todo | Remove `tools/run_native_tests.ps1` or repurpose it as a thin CMake/CTest wrapper | CMAKE-15 | The PlatformIO-based PowerShell script no longer exists or no longer references `pio` |
| CMAKE-22 | todo | Update `library.json` and `library.properties` to reflect that PlatformIO is no longer the primary build system, or remove them if they serve no downstream purpose | CMAKE-19 | Remaining Arduino-ecosystem metadata files are either accurate or deleted |

## Sequencing Notes

Phase 1 can be started immediately; the stub `CMakeLists.txt` already exists and needs only expansion. Phase 2 and Phase 3 can proceed in parallel once Phase 1 is settled. Phase 4 depends on Phases 2 and 3 being complete for both platforms. Phase 5 is the final cleanup gate and must not begin until CMAKE-17 and CMAKE-18 confirm full test parity.

The Arduino SDK dependencies in `test/support/native_arduino/Arduino.h` are currently stubbed for native builds. When those stubs are eventually replaced by real native SDK headers, the CMake targets in Phase 2 will need only `target_include_directories` updates — no structural changes to the build graph are expected.

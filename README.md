# lumalink-platform

`lumalink-platform` is the new home for platform-owned code extracted from `pico-httpserveradvanced`.

This project is expected to own:

- buffer-oriented platform contracts used by `lumalink::http`
- transport contracts and concrete transport implementations
- filesystem contracts and concrete filesystem adapters
- compile-time platform-selection glue
- adapter-specific tests that validate Arduino, POSIX, Windows, and in-memory/native implementations

The initial scaffold is intentionally small. It gives the extracted platform library a standalone package identity, a native test lane, and a stable public include root before implementation files are copied over.

## Location

This project lives at `c:\ode\lumalink-platform`.

## Current Status

- package metadata created
- public include root created under `src/lumalink/platform/`
- native PlatformIO test lane created
- placeholder tests added so the project can validate independently before extraction work begins

## Next Steps

1. Copy platform-owned code from `pico-httpserveradvanced/src/httpadv/v1/platform/` into this project.
2. Update copied code to `lumalink::platform` namespaces and include roots.
3. Move adapter-specific tests here and make the native test lane pass.
4. Add this library as a dependency of `pico-httpserveradvanced`.
5. Remove in-repo platform code from `pico-httpserveradvanced` after the dependency path is stable.

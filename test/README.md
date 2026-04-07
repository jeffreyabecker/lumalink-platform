# Native Test Conventions

This project starts with a single native PlatformIO lane so extracted platform adapters can validate independently from `pico-httpserveradvanced`.

Current intent:

- keep adapter-specific tests in this repository once platform code is copied here
- use the native lane for in-memory and host-safe adapter validation
- expand board-specific validation later, only when concrete platform code lands

Primary command:

```powershell
./tools/run_native_tests.ps1
```

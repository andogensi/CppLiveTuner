# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Compatibility and regression fixes

- Polling reads never sleep for retries; unchanged errors are reported once and
  successful/failed mtime caches expire after 10 ms. Equal-timestamp rapid saves
  are eventually reread without requiring another edit.
- Follow configuration links for normal development reads. Opt into final-link
  rejection with `LIVETUNER_STRICT_FILE_READS=1`; explicit writes remain strict.
- Recheck linked targets outside the watched directory, preserving unsaved
  nlohmann edits when disk content is unchanged.
- Document required Quick Start files, whole-file INI/YAML rejection and atomic
  Params binding updates; restore LiveTuner API documentation.

### Security

- Own and cancel asynchronous operations, preserve watched Params across moves,
  and drain native watcher work before destroying resources.
- Synchronize logging and cache invalidation; deliver diagnostics outside locks.
- Bound configuration reads and parser resources, reject invalid numeric input,
  and preserve the last accepted Params/nlohmann snapshot on validation failure.
- Stop creating files from read APIs; require regular bounded reads and offer
  opt-in final-link rejection. Explicit nlohmann saves always require an existing
  non-reparse, single-link regular file.
- Escape and bound diagnostic messages; add a production runtime-disable macro.

### Validation

- Add Debug/Release cross-platform regression tests for embedded, int64,
  external picojson and nlohmann modes, separate ASan/UBSan and TSan jobs,
  parser fuzzing, multi-translation-unit and installed-package tests.
- Replace Release-disabled smoke assertions with always-active checks; pin
  Actions/dependencies and require all CI jobs to succeed without error suppression.
- Restore proper implementation linkage and install the test-support header.

See [migration details](docs/SECURITY_AND_MIGRATION.md) for compatibility changes.

## [1.0.0] - 2025-12-05

### Added
- Initial public release
- STB-style header-only library design
- Live parameter tuning via file watching
- Support for multiple file formats (JSON, YAML, INI, plain text)
- Event-driven file watching using native OS APIs
  - Windows: ReadDirectoryChangesW
  - Linux: inotify
  - macOS: FSEvents
- `tune_try()` macro for simple single-value tuning
- `Params` class for named parameter management
- Thread-safe implementation with mutex protection
- Async parameter loading support
- Comprehensive error handling with `ErrorInfo`
- Configurable logging system
- nlohmann/json adapter support
- Embedded picojson for JSON parsing
- Cross-platform support (Windows, Linux, macOS)
- CMake build system with installation support
- Test utilities (`LiveTunerTest.h`)

### Documentation
- English README.md
- Japanese README_JA.md
- Detailed documentation in docs/ folder
  - Async usage guide
  - Error handling guide
  - Logging guide
  - nlohmann/json integration guides

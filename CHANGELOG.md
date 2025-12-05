# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

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

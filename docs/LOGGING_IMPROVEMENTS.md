# Logging/Error Handling External Injection Feature Improvements

## Change Summary

Improved LiveTuner's logging system for more flexible use in various environments (especially GUI-only Windows applications and game engines).

## Implemented Features

### 1. Default Logging Control Macro

Added a new macro `LIVETUNER_ENABLE_DEFAULT_LOGGING`.

#### Default Behavior
- **Debug build** (`_DEBUG`, `DEBUG`, or `NDEBUG` not defined): Output logs to `std::cerr`
- **Release build** (`NDEBUG` defined): Disable log output

#### Customization

```cpp
// Disable default log output (useful for GUI apps)
#define LIVETUNER_ENABLE_DEFAULT_LOGGING 0
#include "LiveTuner.h"
```

```cpp
// Force enable even in release builds
#define LIVETUNER_ENABLE_DEFAULT_LOGGING 1
#include "LiveTuner.h"
```

### 2. Default Log Handler Implementation

- Implemented new internal function `internal::default_log_handler()`
- Behavior controlled by `LIVETUNER_ENABLE_DEFAULT_LOGGING` macro
- When disabled, optimized away at compile time with zero overhead

### 3. Enhanced Custom Logging Integration

Maintained existing `set_log_callback()` functionality with improved documentation:

```cpp
// Set custom logger
livetuner::set_log_callback([](livetuner::LogLevel level, const std::string& msg) {
    MyEngine::Log(level, msg);
});

// Disable all logging
livetuner::set_log_callback(nullptr);
```

## Modified Files

### Core Library
- `include/LiveTuner.h`
  - Added `LIVETUNER_ENABLE_DEFAULT_LOGGING` macro
  - Implemented `internal::default_log_handler()`
  - Added `<iostream>` include
  - Enhanced `set_log_callback()` documentation

### Documentation
- `README.md` - Added logging configuration section (English)
- `README_JA.md` - Added logging configuration section (Japanese)
- `LOGGING.md` - Created detailed logging guide

### Tests
- `Test/test_logging.cpp` - Comprehensive logging feature tests
- `Test/test_logging_disabled.cpp` - Log disabling tests

## Usage Examples

### GUI-only Windows Application

```cpp
// Disable console output
#define LIVETUNER_ENABLE_DEFAULT_LOGGING 0
#include "LiveTuner.h"

int WINAPI WinMain(...) {
    // No output to std::cerr
    livetuner::Params params("config.json");
    // ...
}
```

### Unreal Engine Integration

```cpp
#define LIVETUNER_ENABLE_DEFAULT_LOGGING 0
#include "LiveTuner.h"

void InitializeLiveTuner() {
    livetuner::set_log_callback([](livetuner::LogLevel level, const std::string& msg) {
        switch (level) {
            case livetuner::LogLevel::Error:
                UE_LOG(LogTemp, Error, TEXT("%s"), UTF8_TO_TCHAR(msg.c_str()));
                break;
            // ...
        }
    });
}
```

### Embedded Systems (Resource Constrained)

```cpp
// Disable logging to minimize overhead
#define LIVETUNER_ENABLE_DEFAULT_LOGGING 0
#include "LiveTuner.h"

int main() {
    // No callback set, so no log output
    livetuner::Params params("config.txt");
    // ...
}
```

## Backward Compatibility

- **Fully backward compatible**: Existing code works without changes
- Default behavior: Log output in debug builds, disabled in release builds
- Existing code using `set_log_callback()` is not affected

## Test Results

### test_logging.cpp
- ✅ Default log output (debug build)
- ✅ Custom logger setup
- ✅ Logging disabled

### test_logging_disabled.cpp
- ✅ Compile-time log disabling via macro
- ✅ Custom logging works even with default logging disabled

## Benefits

1. **Console-less app support**: Prevent unnecessary `std::cerr` output in GUI-only Windows apps
2. **Game engine integration**: Easy integration with native loggers in Unreal, Unity, etc.
3. **Performance optimization**: Default logging disabled in release builds
4. **Flexibility**: Fine-grained control via macros
5. **Zero overhead**: Optimized away at compile time when disabled

## Summary

These improvements make LiveTuner easier to use in various application environments (especially game engines and GUI apps). Default behavior doesn't affect existing code and can be flexibly customized as needed.

## Blocking API and Event-Driven Fallback Clarification

### About Blocking Behavior

`LiveTuner::get<T>()` is a **blocking API** that blocks the caller until a valid value is read from the file.

```cpp
float speed;
tuner.get(speed);  // Blocks until value is read
```

**⚠️ Note**: While this API is convenient for debugging and experimental use, blocking APIs tend to be avoided in commercial/enterprise code. The following non-blocking APIs are recommended for production code:

```cpp
// Recommended: Non-blocking API
float speed = 1.0f;  // Default value
tuner.try_get(speed);  // Returns immediately (doesn't block)

// Recommended: Timeout API
float speed = 1.0f;
if (tuner.get_timeout(speed, std::chrono::milliseconds(100))) {
    // Value was read
} else {
    // Timeout - use default value
}
```

### Event-Driven File Watching Fallback

When Event-Driven mode fails to start file watching, it automatically falls back to polling mode. The following detailed log messages notify the user:

**Log output example**:
```
[WARNING] [Error] WatcherError: Failed to start file watcher, falling back to polling mode (path: config.json)
[INFO] LiveTuner: Event-driven file watching failed for 'config.json'. Falling back to polling mode (100ms interval). This may occur due to: OS watcher resource limits, unsupported filesystem, or permission issues. Performance may be slightly reduced.
```

**Causes of fallback**:
1. **OS watcher resource limit**: Reached inotify (Linux) or kqueue (macOS) watch limit
2. **Unsupported filesystem**: Network filesystems like NFS, CIFS
3. **Permission issues**: Insufficient access permissions to file or directory
4. **Platform-specific issues**: Specific OS settings or kernel parameters

**Behavior during fallback**:
- Polling interval: 100ms
- Performance impact: Slightly increased (but functionality is fully maintained)
- User action: Usually not required (auto-recovery)

### Error Handling Customization

You can receive fallback notifications with a custom log handler:

```cpp
livetuner::set_log_callback([](livetuner::LogLevel level, const std::string& msg) {
    if (level == livetuner::LogLevel::Warning && 
        msg.find("falling back to polling") != std::string::npos) {
        // Special handling when fallback occurs
        notifyUser("File watching has switched to polling mode");
    }
    // Normal log processing
    MyLogger::log(level, msg);
});
```


# Logging Configuration Guide

## Overview

LiveTuner provides flexible logging capabilities that can be customized for various deployment scenarios. This guide explains how to control logging behavior for different use cases.

## Default Behavior

### Debug Builds
In debug builds (`_DEBUG`, `DEBUG`, or `NDEBUG` not defined), LiveTuner automatically outputs log messages to `std::cerr` by default.

### Release Builds
In release builds (`NDEBUG` defined), default logging is disabled to minimize overhead.

## Controlling Default Logging

### Disable Default Logging

Useful for GUI-only Windows applications where console output is not visible:

```cpp
#define LIVETUNER_ENABLE_DEFAULT_LOGGING 0
#include "LiveTuner.h"

int main() {
    // No logging to stderr by default
    livetuner::Params params("config.json");
    // ...
}
```

### Force Enable Default Logging

Force enable logging even in release builds:

```cpp
#define LIVETUNER_ENABLE_DEFAULT_LOGGING 1
#include "LiveTuner.h"

int main() {
    // Logs to stderr even in release builds
    livetuner::Params params("config.json");
    // ...
}
```

## Custom Logging Integration

### Game Engine Integration

#### Unreal Engine Example
```cpp
#define LIVETUNER_ENABLE_DEFAULT_LOGGING 0
#include "LiveTuner.h"

void InitializeLiveTuner() {
    livetuner::set_log_callback([](livetuner::LogLevel level, const std::string& msg) {
        switch (level) {
            case livetuner::LogLevel::Debug:
                UE_LOG(LogTemp, Verbose, TEXT("[LiveTuner] %s"), UTF8_TO_TCHAR(msg.c_str()));
                break;
            case livetuner::LogLevel::Info:
                UE_LOG(LogTemp, Log, TEXT("[LiveTuner] %s"), UTF8_TO_TCHAR(msg.c_str()));
                break;
            case livetuner::LogLevel::Warning:
                UE_LOG(LogTemp, Warning, TEXT("[LiveTuner] %s"), UTF8_TO_TCHAR(msg.c_str()));
                break;
            case livetuner::LogLevel::Error:
                UE_LOG(LogTemp, Error, TEXT("[LiveTuner] %s"), UTF8_TO_TCHAR(msg.c_str()));
                break;
        }
    });
}
```

#### Unity Native Plugin Example
```cpp
#define LIVETUNER_ENABLE_DEFAULT_LOGGING 0
#include "LiveTuner.h"

extern "C" {
    // Unity Debug.Log callback
    typedef void (*UnityLogCallback)(int level, const char* message);
    UnityLogCallback g_UnityLog = nullptr;
    
    void SetUnityLogger(UnityLogCallback callback) {
        g_UnityLog = callback;
        
        livetuner::set_log_callback([](livetuner::LogLevel level, const std::string& msg) {
            if (g_UnityLog) {
                g_UnityLog(static_cast<int>(level), msg.c_str());
            }
        });
    }
}
```

### Custom Logger Example

```cpp
#define LIVETUNER_ENABLE_DEFAULT_LOGGING 0
#include "LiveTuner.h"
#include "MyGameLogger.h"

void SetupLiveTunerLogging() {
    livetuner::set_log_callback([](livetuner::LogLevel level, const std::string& msg) {
        MyGame::LogLevel gameLevel;
        switch (level) {
            case livetuner::LogLevel::Debug:   gameLevel = MyGame::LogLevel::Debug; break;
            case livetuner::LogLevel::Info:    gameLevel = MyGame::LogLevel::Info; break;
            case livetuner::LogLevel::Warning: gameLevel = MyGame::LogLevel::Warning; break;
            case livetuner::LogLevel::Error:   gameLevel = MyGame::LogLevel::Error; break;
        }
        
        MyGame::Logger::Log(gameLevel, "LiveTuner", msg);
    });
}
```

### File Logging Example

```cpp
#include <fstream>

void SetupFileLogging() {
    static std::ofstream logFile("livetuner.log", std::ios::app);
    
    livetuner::set_log_callback([](livetuner::LogLevel level, const std::string& msg) {
        if (logFile.is_open()) {
            auto now = std::chrono::system_clock::now();
            auto time = std::chrono::system_clock::to_time_t(now);
            
            const char* level_str = "INFO";
            switch (level) {
                case livetuner::LogLevel::Debug:   level_str = "DEBUG"; break;
                case livetuner::LogLevel::Info:    level_str = "INFO"; break;
                case livetuner::LogLevel::Warning: level_str = "WARN"; break;
                case livetuner::LogLevel::Error:   level_str = "ERROR"; break;
            }
            
            logFile << "[" << std::ctime(&time) << "] "
                    << "[" << level_str << "] " 
                    << msg << std::endl;
        }
    });
}
```

## Disable All Logging

For production or performance-critical scenarios, logging can be completely disabled:

```cpp
livetuner::set_log_callback(nullptr);
```

## Use Cases

### GUI-only Windows Application
```cpp
// Disable stderr output as there's no console window
#define LIVETUNER_ENABLE_DEFAULT_LOGGING 0
#include "LiveTuner.h"

// Optional: Send logs to file or custom logging system
void SetupLogging() {
    livetuner::set_log_callback([](livetuner::LogLevel level, const std::string& msg) {
        // Write to file or custom GUI log window
        MyApp::ShowLogInGUI(level, msg);
    });
}
```

### Embedded Systems / Resource Constrained
```cpp
// Disable logging to minimize overhead
#define LIVETUNER_ENABLE_DEFAULT_LOGGING 0
#include "LiveTuner.h"

int main() {
    // No logging callback - minimal overhead
    livetuner::Params params("config.txt");
    // ...
}
```

### Development with Debug Output
```cpp
// Keep default behavior (logs to stderr in debug builds)
#include "LiveTuner.h"

int main() {
    // Shows logs in console in debug builds
    // No logging in release builds
    livetuner::Params params("config.json");
    // ...
}
```

### Conditional Logging
```cpp
#include "LiveTuner.h"

void SetupConditionalLogging(bool enableLogging) {
    if (enableLogging) {
        livetuner::set_log_callback([](livetuner::LogLevel level, const std::string& msg) {
            const char* level_str = "INFO";
            switch (level) {
                case livetuner::LogLevel::Debug:   level_str = "DEBUG"; break;
                case livetuner::LogLevel::Info:    level_str = "INFO"; break;
                case livetuner::LogLevel::Warning: level_str = "WARN"; break;
                case livetuner::LogLevel::Error:   level_str = "ERROR"; break;
            }
            std::cerr << "[LiveTuner:" << level_str << "] " << msg << std::endl;
        });
    } else {
        livetuner::set_log_callback(nullptr);
    }
}
```

## Log Levels

LiveTuner provides four log levels:

- **Debug**: Detailed diagnostic information
- **Info**: General informational messages
- **Warning**: Warning messages that don't prevent operation
- **Error**: Error messages indicating failures

## Best Practices

1. **During Development**: Use default logging or custom logger for debugging
2. **Production**: Disable logging or use minimal logging for errors only
3. **GUI Applications**: Always disable default stderr logging
4. **Game Engines**: Integrate with engine's native logging system
5. **Performance Critical**: Completely disable logging or use conditional compilation

## Thread Safety

The logging system is thread-safe. The global log callback can be safely accessed from multiple threads. However, individual log callback implementations need to ensure their own thread safety if they access I/O or shared resources.

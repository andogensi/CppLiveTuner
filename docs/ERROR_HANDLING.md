# Error Handling Guide

CppLiveTuner v2.0 adds detailed error handling features.

## Overview

In previous versions, file reading or parse failures would simply return `false` or `nullopt`, making it unclear why the failure occurred. The new version provides the following features:

1. **Error Type Classification**: File not found, access permission denied, parse errors, etc.
2. **Detailed Error Messages**: Includes failure reason and file path
3. **Log Callbacks**: Customizable log output
4. **Error Retrieval API**: Retrieve last error information programmatically

## Error Types

```cpp
enum class ErrorType {
    None,                   // No error
    FileNotFound,          // File does not exist
    FileAccessDenied,      // File access denied
    FileEmpty,             // File is empty
    FileReadError,         // Failed to read file
    ParseError,            // Parse error (syntax error)
    InvalidFormat,         // Invalid format
    Timeout,               // Timeout
    WatcherError,          // Failed to start file watcher
    Unknown                // Unknown error
};
```

## Basic Usage

### For LiveTuner

```cpp
#include <LiveTuner.h>
#include <iostream>

int main() {
    livetuner::LiveTuner tuner("params.txt");
    
    float speed = 1.0f;
    
    while (running) {
        if (!tuner.try_get(speed)) {
            // On failure, check error information
            auto error = tuner.last_error();
            if (error) {
                std::cerr << "Error: " << error.to_string() << std::endl;
                
                // Handle based on error type
                switch (error.type) {
                    case livetuner::ErrorType::FileNotFound:
                        std::cerr << "Create the file: " << error.file_path << std::endl;
                        break;
                    case livetuner::ErrorType::FileAccessDenied:
                        std::cerr << "Check file permissions!" << std::endl;
                        break;
                    case livetuner::ErrorType::ParseError:
                        std::cerr << "Fix the file format!" << std::endl;
                        break;
                    default:
                        break;
                }
            }
        }
        
        player.move(speed);
    }
}
```

### For Params

```cpp
#include <LiveTuner.h>
#include <iostream>

int main() {
    livetuner::Params params("config.json");
    
    float speed = 1.0f;
    float gravity = 9.8f;
    
    params.bind("speed", speed, 1.0f);
    params.bind("gravity", gravity, 9.8f);
    
    while (running) {
        if (!params.update()) {
            // On failure, check error information
            if (params.has_error()) {
                auto error = params.last_error();
                std::cerr << "Failed to update config: " 
                         << error.to_string() << std::endl;
            }
        }
        
        // speed, gravity are updated (default values on failure)
        player.move(speed);
        physics.set_gravity(gravity);
    }
}
```

## Setting Log Callbacks

You can catch all errors and warnings by setting a global log callback.

```cpp
#include <LiveTuner.h>
#include <iostream>
#include <fstream>

int main() {
    // Open log file
    std::ofstream log_file("livetuner.log", std::ios::app);
    
    // Set log callback
    livetuner::set_log_callback([&log_file](livetuner::LogLevel level, const std::string& msg) {
        const char* level_str = "INFO";
        switch (level) {
            case livetuner::LogLevel::Debug:   level_str = "DEBUG"; break;
            case livetuner::LogLevel::Info:    level_str = "INFO";  break;
            case livetuner::LogLevel::Warning: level_str = "WARN";  break;
            case livetuner::LogLevel::Error:   level_str = "ERROR"; break;
        }
        
        // Output to both stderr and file
        std::string log_msg = "[LiveTuner:" + std::string(level_str) + "] " + msg;
        std::cerr << log_msg << std::endl;
        log_file << log_msg << std::endl;
    });
    
    // From now on, all errors will be logged
    livetuner::LiveTuner tuner("params.txt");
    float speed;
    tuner.try_get(speed);
    
    return 0;
}
```

## Error Information Details

The `ErrorInfo` struct contains the following information:

```cpp
struct ErrorInfo {
    ErrorType type;                              // Error type
    std::string message;                         // Error message
    std::string file_path;                       // File path where the error occurred
    std::chrono::system_clock::time_point timestamp;  // Error occurrence time
    
    // Check if there is an error
    explicit operator bool() const;
    
    // Convert error information to string
    std::string to_string() const;
};
```

### Usage Example

```cpp
livetuner::Params params("config.json");

if (!params.update()) {
    auto error = params.last_error();
    
    if (error) {  // If there is an error
        std::cerr << "Error type: " << livetuner::ErrorInfo::type_to_string(error.type) << std::endl;
        std::cerr << "Message: " << error.message << std::endl;
        std::cerr << "File: " << error.file_path << std::endl;
        
        // Or output all at once
        std::cerr << error.to_string() << std::endl;
        // Output example: [FileNotFound] config.json: File does not exist
    }
}
```

## Debugging After Release

Example of obtaining diagnostic information even in release builds:

```cpp
#include <LiveTuner.h>
#include <fstream>
#include <ctime>

class DiagnosticLogger {
public:
    DiagnosticLogger(const std::string& log_path) {
        log_file_.open(log_path, std::ios::app);
        
        livetuner::set_log_callback([this](livetuner::LogLevel level, const std::string& msg) {
            // Log only errors and warnings (exclude debug info)
            if (level == livetuner::LogLevel::Error || 
                level == livetuner::LogLevel::Warning) {
                
                auto now = std::chrono::system_clock::now();
                auto time_t = std::chrono::system_clock::to_time_t(now);
                
                log_file_ << std::ctime(&time_t) << " - " << msg << std::endl;
                log_file_.flush();
            }
        });
    }
    
private:
    std::ofstream log_file_;
};

int main() {
    DiagnosticLogger logger("diagnostics.log");
    
    livetuner::Params params("config.json");
    // ... application code ...
    
    // If a user reports "settings are not being applied",
    // check diagnostics.log to find the cause
    
    return 0;
}
```

## Clearing Errors

Error information is not automatically cleared, but can be cleared manually:

```cpp
livetuner::LiveTuner tuner("params.txt");

float speed;
if (!tuner.try_get(speed)) {
    // Error handling
    auto error = tuner.last_error();
    std::cerr << error.to_string() << std::endl;
}

// Clear error (if needed)
tuner.clear_error();

// Or it will be automatically cleared on next successful read
```

## Performance Impact

The error handling features are designed as follows:

- **When log callback is not set**: Almost no overhead (only function pointer check)
- **Error information recording**: Only occurs on failure, no impact on normal operation
- **Error retrieval API**: Uses mutex lock, so avoid frequent calls

## Common Errors and Solutions

### FileNotFound
```
[FileNotFound] params.txt: File does not exist
```
**Solution**: Create the file or specify the correct path

### FileAccessDenied
```
[FileAccessDenied] config.json: Cannot open file for reading
```
**Solution**: Check file permissions, or verify another process isn't locking it

### ParseError
```
[ParseError] config.json: Failed to parse JSON format
```
**Solution**: Verify file syntax (JSON/YAML/INI format)

### FileEmpty
```
[FileEmpty] params.txt: File is empty
```
**Solution**: Write a value to the file

### Timeout
```
[Timeout] params.txt: Timeout waiting for valid value
```
**Solution**: Extend timeout duration or check if file is being updated correctly

## Summary

The error handling features enable:

- ✅ Identify why settings are not being applied
- ✅ Easier debugging in the field after release
- ✅ Record detailed diagnostic information to log files
- ✅ Take appropriate action based on error type

This helps avoid situations where "settings are not being applied but the reason is unknown".

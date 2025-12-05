# Callback Reentrancy Protection

## Overview

Implemented a reentrancy protection mechanism to prevent undefined behavior when dangerous methods that modify the instance state are called during callback execution in the `Params` class.

## Problem

In the previous implementation, the following situations could cause undefined behavior:

```cpp
params.on_change([&params]() {
    // Calling dangerous methods from within callback
    params.set_file("other.json");     // Change file path
    params.unbind_all();                // Delete all bindings
    params.invalidate_cache();          // Clear cache
    params.reset_to_defaults();         // Reset to default values
    params.update();                    // Try to update again (risk of infinite loop)
});
```

While the "lock extraction" pattern itself is a correct approach, if processing that significantly changes the instance's lifetime or state is called within the callback, the following problems occur:

1. **Recursive calls**: `update()` → callback → `update()` → ... infinite loop
2. **State inconsistency**: Bindings or cache deleted during callback execution
3. **Undefined behavior**: Other processing runs during file watcher reinitialization

## Solution

### 1. Added Reentrancy Prevention Flag

Added `std::atomic<bool> in_callback_` flag to the `Params` class:

```cpp
// Reentrancy prevention flag (whether currently in callback)
std::atomic<bool> in_callback_{false};
```

### 2. Reentrancy Check in update()

Manage the flag before and after callback execution:

```cpp
bool update() {
    // Prevent reentrancy during callback execution
    if (in_callback_.load()) {
        internal::log(LogLevel::Debug, 
            "update() called during callback execution - skipped to prevent recursion");
        return false;
    }
    
    // ... file reading process ...
    
    // Execute callback
    if (callback_to_invoke) {
        in_callback_.store(true);
        try {
            callback_to_invoke();
        } catch (...) {
            in_callback_.store(false);
            throw;
        }
        in_callback_.store(false);
    }
    
    return updated;
}
```

### 3. Reentrancy Check in Dangerous Methods

Block calls during callback execution for the following methods:

```cpp
void set_file(std::string_view file_path, FileFormat format = FileFormat::Auto) {
    if (in_callback_.load()) {
        internal::log(LogLevel::Warning, 
            "set_file() called during callback execution - operation skipped");
        return;
    }
    // ... normal processing ...
}

void unbind_all() {
    if (in_callback_.load()) {
        internal::log(LogLevel::Warning, 
            "unbind_all() called during callback execution - operation skipped");
        return;
    }
    // ... normal processing ...
}

// Similarly protected methods:
// - start_watching()
// - stop_watching()
// - invalidate_cache()
// - reset_to_defaults()
```

## Behavior

### Normal Usage Example

```cpp
Params params("config.json");
float speed = 1.0f;
params.bind("speed", speed, 1.0f);

params.on_change([&]() {
    std::cout << "Speed changed to: " << speed << std::endl;
    // Reading values and logging is safe
});

params.update(); // OK: callback executes safely
```

### Example Where Reentrancy is Prevented

```cpp
params.on_change([&params]() {
    // These calls output warning logs and are ignored
    params.update();              // Debug log: skipped to prevent recursion
    params.set_file("other.json"); // Warning log: operation skipped
    params.unbind_all();           // Warning log: operation skipped
});
```

## Tests

The following test cases are implemented in `Test/test_reentrancy.cpp`:

1. ✅ Prevention of `update()` recursive calls
2. ✅ Reentrancy prevention for `set_file()`
3. ✅ Reentrancy prevention for `unbind_all()`
4. ✅ Reentrancy prevention for `invalidate_cache()`
5. ✅ Reentrancy prevention for `reset_to_defaults()`

All tests have been confirmed to pass.

## Performance Impact

- Reading/writing `std::atomic<bool>` is very lightweight (a few nanoseconds)
- When no callback exists: virtually zero overhead
- When callback exists: only flag set/reset (exception-safe)

## Compatibility

This change maintains full backward compatibility with existing code:

- ✅ No API changes
- ✅ No impact on normal usage
- ✅ Only dangerous usage is safely blocked

## Summary

**Problem before fix**: Calling `reset_to_defaults()`, `set_file()`, etc. within a callback could cause undefined behavior

**Behavior after fix**: These calls are safely blocked and a warning log is output

This ensures that even if users accidentally perform dangerous operations, the program won't crash and maintains a debuggable state.

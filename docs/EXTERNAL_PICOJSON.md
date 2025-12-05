# External picojson Support

## Overview

CppLiveTuner supports using an external picojson installation instead of the embedded version. This feature is useful when:

- Your project already uses picojson
- You need a specific version of picojson
- You want to reduce code duplication

## Usage

### Option 1: Use Embedded picojson (Default)

By default, CppLiveTuner includes its own copy of picojson. Simply include the header as usual:

```cpp
#include "LiveTuner.h"

// picojson is available as livetuner::picojson
```

### Option 2: Use External picojson

To use your own picojson installation:

1. Define `LIVETUNER_USE_EXTERNAL_PICOJSON` before including LiveTuner.h
2. Include your own picojson header first
3. Include LiveTuner.h

```cpp
#define LIVETUNER_USE_EXTERNAL_PICOJSON
#include "picojson.h"  // External picojson
#include "LiveTuner.h"
```

Or specify via compiler flags:

```bash
g++ -std=c++17 -DLIVETUNER_USE_EXTERNAL_PICOJSON \
    -I/path/to/picojson/include \
    -Iinclude your_program.cpp -pthread -o program
```

## Implementation Details

The implementation uses preprocessor guards to conditionally include the embedded picojson:

```cpp
#ifndef LIVETUNER_USE_EXTERNAL_PICOJSON

namespace picojson {
    // ... embedded picojson code ...
} // namespace picojson

#endif // LIVETUNER_USE_EXTERNAL_PICOJSON
```

When `LIVETUNER_USE_EXTERNAL_PICOJSON` is defined:
- The embedded picojson code is completely excluded from compilation
- LiveTuner expects picojson to be available from an external source
- No code duplication or namespace conflicts occur

## Testing

A test program is provided to verify the embedded picojson functionality:

```bash
cd CppLiveTuner
g++ -std=c++17 -Iinclude Test/test_embedded_picojson.cpp -o build/test_embedded_picojson.exe -pthread
./build/test_embedded_picojson.exe
```

Expected output:
```
=== Testing Embedded picojson (Default Behavior) ===

Test 1: Creating test JSON file...
  ✓ Test file created: test_embedded_picojson_config.json

Test 2: Using LiveTuner::Params with embedded picojson...
  Values read from JSON:
    speed: 3.14
    gravity: 9.8
    debug: true
    name: EmbeddedPicojsonTest
  ✓ All values correct!

Test 3: Verify embedded picojson namespace is accessible...
  ✓ picojson is embedded within livetuner namespace
  ✓ JSON parsing works correctly (verified in Test 2)

=== All Tests Passed ===

LiveTuner successfully works with embedded picojson (default behavior)!
```

## Benefits

### For Embedded picojson Users (Default)
- Zero dependencies
- Works out of the box
- No setup required

### For External picojson Users
- No code duplication
- Use your preferred picojson version
- Better control over dependencies
- Smaller binary if picojson is already linked

## Compatibility

- Requires picojson API-compatible with the embedded version
- The embedded version is based on picojson v1.3.0
- External picojson must provide the same API (namespace picojson, value class, parse function, etc.)

## References

- [picojson on GitHub](https://github.com/kazuho/picojson)
- [README.md](README.md) - Main documentation
- [README_JA.md](README_JA.md) - Japanese documentation

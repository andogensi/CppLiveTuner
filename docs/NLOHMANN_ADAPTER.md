# nlohmann/json Adapter

An adapter for using the **nlohmann/json** library with CppLiveTuner to handle complex JSON structures.

## Features

- ✨ **Nested JSON object** reading support
- 📦 **Array support** - Easily handle JSON arrays
- 🔒 **Type-safe** - Template-based type-safe value retrieval
- 🔄 **Real-time updates** - Auto-detect file changes
- 🎯 **JSON paths** - Access nested values with dot notation (e.g., `"player.speed"`)
- 🔧 **Auto-binding** - Automatically update variables
- 💾 **Read/write support** - Read and save JSON files

## Requirements

- C++17 or later
- nlohmann/json library (`json.hpp`)
- CppLiveTuner base functionality

## Installation

1. Add the nlohmann/json library to your project
2. Define `LIVETUNER_USE_NLOHMANN_JSON` macro
3. Include nlohmann/json first, then include `LiveTuner.h`

```cpp
#define LIVETUNER_USE_NLOHMANN_JSON
#include <nlohmann/json.hpp>
#define LIVETUNER_IMPLEMENTATION  // Only define in one source file
#include "LiveTuner.h"
```

## Basic Usage

### 1. Simple Value Retrieval

```cpp
#define LIVETUNER_USE_NLOHMANN_JSON
#include <nlohmann/json.hpp>
#define LIVETUNER_IMPLEMENTATION
#include "LiveTuner.h"

livetuner::NlohmannParams params("config.json");

// Basic value retrieval
float speed = params.get<float>("player.speed", 1.0f);
std::string name = params.get<std::string>("player.name", "Player");
bool debug = params.get<bool>("debug", false);

while (running) {
    if (params.update()) {
        // Re-fetch values when file changes
        speed = params.get<float>("player.speed", 1.0f);
    }
    
    // Game logic
    player.move(speed);
}
```

**config.json:**
```json
{
  "player": {
    "name": "Hero",
    "speed": 2.5
  },
  "debug": true
}
```

### 2. Array Retrieval

```cpp
// Get entire array
auto colors = params.get<std::vector<int>>("colors", {255, 0, 0});

// Access specific array element
int red = params.get<int>("colors[0]", 255);
int green = params.get<int>("colors[1]", 0);

// Nested arrays
auto position = params.get<std::vector<float>>("player.position", {0.0f, 0.0f, 0.0f});
```

**config.json:**
```json
{
  "colors": [255, 128, 64],
  "player": {
    "position": [10.0, 20.0, 30.0]
  }
}
```

### 3. Auto-binding

Use `NlohmannBinder` when you want variables to be automatically updated.

```cpp
livetuner::NlohmannBinder binder("config.json");

// Bind variables
float speed;
std::string name;
bool debug;

binder.bind("player.speed", speed, 1.0f);
binder.bind("player.name", name, std::string("Player"));
binder.bind("debug", debug, false);

while (running) {
    if (binder.update()) {
        // All variables are automatically updated!
        std::cout << "Speed: " << speed << "\n";
        std::cout << "Name: " << name << "\n";
    }
}
```

### 4. Nested Objects

Access deeply nested values.

```cpp
// Deeply nested values
auto resolution_width = params.get<int>("game.settings.graphics.resolution[0]", 1920);
auto volume = params.get<float>("game.settings.audio.master_volume", 1.0f);
auto quality = params.get<std::string>("game.settings.graphics.quality", "medium");
```

**config.json:**
```json
{
  "game": {
    "settings": {
      "graphics": {
        "resolution": [1920, 1080],
        "quality": "high"
      },
      "audio": {
        "master_volume": 0.8
      }
    }
  }
}
```

### 5. Full JSON Operations

```cpp
// Get entire JSON
auto full_json = params.get_json();
std::cout << full_json.dump(2) << "\n";

// Get specific section
auto player_data = params.get_json("player");

// Check if value exists
if (params.has("player.speed")) {
    // ...
}
```

### 6. Setting Values and Saving

```cpp
// Change values programmatically
params.set("player.speed", 3.5f);
params.set("player.name", std::string("NewName"));
params.set("debug", true);

// Save to file
params.save(true);  // true = pretty print
```

## API Reference

### NlohmannParams

Main parameter management class.

#### Constructor

```cpp
NlohmannParams(const std::string& file_path)
```

#### Methods

```cpp
// Check for updates
bool update()

// Get value (with optional default)
template<typename T>
T get(const std::string& json_path, const T& default_value = T{})

// Get JSON object
json get_json(const std::string& json_path = "")

// Check if value exists
bool has(const std::string& json_path)

// Set value
template<typename T>
bool set(const std::string& json_path, const T& value)

// Save to file
bool save(bool pretty = true)

// Get entire JSON as string (for debugging)
std::string dump(int indent = 2)

// Set error callback
void set_error_callback(ErrorCallback callback)

// Get last error
const ErrorInfo& last_error()
```

### NlohmannBinder

Helper class for auto-binding.

```cpp
// Bind variable
template<typename T>
void bind(const std::string& json_path, T& variable, const T& default_value = T{})

// Check for updates and update all variables
bool update()

// Access the underlying NlohmannParams
NlohmannParams& params()
```

## JSON Path Notation

| Path | Description | Example |
|------|-------------|---------|
| `"key"` | Top-level key | `"debug"` |
| `"parent.child"` | Nested key | `"player.speed"` |
| `"array[0]"` | Array element | `"colors[0]"` |
| `"parent.array[1]"` | Nested array | `"player.position[1]"` |

## Error Handling

```cpp
livetuner::NlohmannParams params("config.json");

// Set error callback
params.set_error_callback([](const livetuner::ErrorInfo& error) {
    std::cerr << "Error: " << error.message << "\n";
    std::cerr << "Type: " << livetuner::ErrorInfo::type_to_string(error.type) << "\n";
});

// Check last error
if (params.last_error()) {
    std::cerr << params.last_error().message << "\n";
}
```

## Supported Types

- Basic types: `int`, `float`, `double`, `bool`, `std::string`
- Containers: `std::vector<T>`, `std::array<T, N>`
- All types supported by nlohmann/json

## Running Example

```bash
# Compile (specify nlohmann/json path)
g++ -std=c++17 examples/example_nlohmann.cpp -I include -I <nlohmann_include_path> -o example_nlohmann.exe

# Run
./example_nlohmann.exe
```

Editing `config_nlohmann.json` while the program is running will reflect values in real-time.

## Example Code

See the following for detailed usage examples:

- [`examples/example_nlohmann.cpp`](../examples/example_nlohmann.cpp) - From basic to advanced usage

## Performance

- File watching uses OS native APIs (Windows: ReadDirectoryChangesW, Linux: inotify)
- JSON parsing is only executed when changes are detected
- Thread-safe implementation

## Limitations

- JSON files must be in valid JSON format
- For very large JSON files (several MB or more), parsing time may increase
- JSON paths only support basic dot notation (complex queries not supported)

## Troubleshooting

### Q: JSON parse error occurs

**A:** Check the JSON file syntax. Verify comma positions, bracket matching, string quotation marks, etc.

### Q: Values are not being updated

**A:** 
1. Ensure `update()` method is being called periodically
2. Verify JSON path is correct (case-sensitive)
3. Set an error callback to check error messages

### Q: Cannot access array elements

**A:** Array access notation is `"array[0]"`. Indices are 0-based.

## License

This adapter is provided under the same license (MIT License) as CppLiveTuner.

The nlohmann/json library is provided under MIT License.

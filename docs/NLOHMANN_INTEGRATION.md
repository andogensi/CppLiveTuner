# Adding nlohmann/json Adapter to CppLiveTuner

## Added Features

nlohmann/json support is integrated into `LiveTuner.h`.

### Enabling

Define the `LIVETUNER_USE_NLOHMANN_JSON` macro and include nlohmann/json before including `LiveTuner.h`.

```cpp
#define LIVETUNER_USE_NLOHMANN_JSON
#include <nlohmann/json.hpp>
#define LIVETUNER_IMPLEMENTATION  // Only in one source file
#include "LiveTuner.h"
```

### Documentation
- `docs/NLOHMANN_ADAPTER.md` - Detailed documentation (including API reference)
- `docs/NLOHMANN_QUICKSTART.md` - 5-minute quickstart guide

### Samples and Tests
- `examples/example_nlohmann.cpp` - Practical sample code
- `Test/test_nlohmann_adapter.cpp` - Comprehensive test code

---

## Suggested README.md Additions

We recommend adding the following content to `README.md` or `README_JA.md`:

### Location: After "Features" Section

```markdown
## 🚀 nlohmann/json Support

CppLiveTuner supports integration with the **nlohmann/json** library!

Easily handle complex JSON structures, arrays, and nested objects.

```cpp
#define LIVETUNER_USE_NLOHMANN_JSON
#include <nlohmann/json.hpp>
#define LIVETUNER_IMPLEMENTATION
#include "LiveTuner.h"

livetuner::NlohmannParams params("config.json");

while (running) {
    if (params.update()) {
        // Get nested values
        auto speed = params.get<float>("player.speed", 1.0f);
        auto colors = params.get<std::vector<int>>("colors", {});
    }
}
```

**See also:**
- 📖 [Quickstart](docs/NLOHMANN_QUICKSTART.md) - Get started in 5 minutes
- 📚 [Detailed Documentation](docs/NLOHMANN_ADAPTER.md) - API Reference

---
```

### Location: Usage Examples Section

```markdown
### 📦 Advanced Example with nlohmann/json

```cpp
#define LIVETUNER_USE_NLOHMANN_JSON
#include <nlohmann/json.hpp>
#define LIVETUNER_IMPLEMENTATION
#include "LiveTuner.h"

livetuner::NlohmannBinder binder("config.json");

float speed;
std::string name;
std::vector<float> position;

// Auto-bind variables
binder.bind("player.speed", speed, 1.0f);
binder.bind("player.name", name, std::string("Hero"));
binder.bind("player.position", position, {0.0f, 0.0f, 0.0f});

while (running) {
    if (binder.update()) {
        // All variables are automatically updated!
        player.move(speed);
    }
}
```

**config.json:**
```json
{
  "player": {
    "name": "Warrior",
    "speed": 2.5,
    "position": [10.0, 20.0, 30.0]
  }
}
```

See [examples/example_nlohmann.cpp](examples/example_nlohmann.cpp) for more details.
```

### Location: Build Instructions Section

```markdown
### Building nlohmann/json Sample

```bash
# Build with CMake
mkdir build && cd build
cmake .. -DLIVETUNER_BUILD_EXAMPLES=ON
cmake --build . --target livetuner_example_nlohmann

# Direct compilation
g++ -std=c++17 examples/example_nlohmann.cpp -I include -o example_nlohmann
```
```

---

## Feature List

### `NlohmannParams` Class

| Feature | Description |
|---------|-------------|
| `get<T>()` | Type-safe value retrieval |
| `get_json()` | Get JSON object |
| `set()` | Set value |
| `has()` | Check value existence |
| `save()` | Save to file |
| `update()` | Detect file changes and update |

### `NlohmannBinder` Class

| Feature | Description |
|---------|-------------|
| `bind()` | Auto-bind variables |
| `update()` | Auto-update all bound variables |

### JSON Path Notation

- `"key"` - Top-level key
- `"parent.child"` - Nested key
- `"array[0]"` - Array element
- `"parent.array[1].value"` - Complex path

---

## Usage Examples

### Game Development
```cpp
binder.bind("player.speed", player_speed, 5.0f);
binder.bind("enemy.speed", enemy_speed, 3.0f);
binder.bind("physics.gravity", gravity, 9.8f);
```

### Simulation
```cpp
auto time_step = params.get<double>("sim.time_step", 0.01);
auto iterations = params.get<int>("sim.iterations", 100);
```

### Machine Learning
```cpp
auto learning_rate = params.get<double>("ml.learning_rate", 0.001);
auto batch_size = params.get<int>("ml.batch_size", 32);
```

---

## Testing

Run tests to verify functionality:

```bash
# Build & run tests with CMake
mkdir build && cd build
cmake .. -DLIVETUNER_BUILD_TESTS=ON
cmake --build . --target livetuner_test_nlohmann_adapter
ctest

# Or run directly
./livetuner_test_nlohmann_adapter
```

---

## Support

If you have questions or issues:

1. First check [NLOHMANN_QUICKSTART.md](NLOHMANN_QUICKSTART.md)
2. Refer to troubleshooting in [NLOHMANN_ADAPTER.md](NLOHMANN_ADAPTER.md)
3. Try the sample code [examples/example_nlohmann.cpp](examples/example_nlohmann.cpp)

---

## License

This adapter is under the same MIT License as CppLiveTuner.
nlohmann/json is also provided under MIT License.

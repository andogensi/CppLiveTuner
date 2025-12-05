# nlohmann/json Adapter Quickstart

## Get Started with nlohmann/json + CppLiveTuner in 5 Minutes

### Step 1: Prepare Required Files

```
your_project/
├── LiveTuner.h                    # CppLiveTuner main file
├── nlohmann/
│   └── json.hpp                   # nlohmann/json library
└── main.cpp                        # Your code
```

### Step 2: Minimal Code Example

```cpp
// Step 1: Enable nlohmann/json support
#define LIVETUNER_USE_NLOHMANN_JSON

// Step 2: Include nlohmann/json first
#include <nlohmann/json.hpp>

// Step 3: Enable implementation (only in one source file)
#define LIVETUNER_IMPLEMENTATION
#include "LiveTuner.h"

#include <iostream>
#include <thread>
#include <chrono>

int main() {
    // Watch JSON file
    livetuner::NlohmannParams params("config.json");
    
    while (true) {
        // Check for file changes
        if (params.update()) {
            std::cout << "Settings updated!\n";
        }
        
        // Get values
        float speed = params.get<float>("player.speed", 1.0f);
        std::string name = params.get<std::string>("player.name", "Player");
        
        std::cout << name << "'s speed: " << speed << "\n";
        
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    
    return 0;
}
```

### Step 3: Create JSON File

Create `config.json`:

```json
{
  "player": {
    "name": "Hero",
    "speed": 2.5
  }
}
```

### Step 4: Compile & Run

```bash
# Windows (MinGW/MSYS2) - Specify nlohmann/json include path
g++ -std=c++17 main.cpp -I <nlohmann_include_path> -o main.exe

# Linux/Mac
g++ -std=c++17 main.cpp -I <nlohmann_include_path> -o main -pthread

# vcpkg example
g++ -std=c++17 main.cpp -I /path/to/vcpkg/installed/x64-windows/include -o main.exe

# Run
./main
```

### Step 5: Live Tuning!

Edit `config.json` while the program is running:

```json
{
  "player": {
    "name": "Warrior",
    "speed": 5.0
  }
}
```

Changes are reflected immediately on save!

---

## More Convenient Usage

### Auto-binding

```cpp
livetuner::NlohmannBinder binder("config.json");

float speed;
std::string name;

binder.bind("player.speed", speed, 1.0f);
binder.bind("player.name", name, std::string("Player"));

while (true) {
    if (binder.update()) {
        // speed and name are auto-updated!
        std::cout << name << ": " << speed << "\n";
    }
    std::this_thread::sleep_for(std::chrono::seconds(1));
}
```

### Handling Arrays

```cpp
// JSON: {"colors": [255, 128, 64]}
auto colors = params.get<std::vector<int>>("colors", {0, 0, 0});

// Or individually
int r = params.get<int>("colors[0]", 0);
int g = params.get<int>("colors[1]", 0);
int b = params.get<int>("colors[2]", 0);
```

### Nested Objects

```cpp
// JSON: {"game": {"settings": {"volume": 0.8}}}
float volume = params.get<float>("game.settings.volume", 1.0f);
```

### Error Handling

```cpp
params.set_error_callback([](const livetuner::ErrorInfo& error) {
    std::cerr << "Error: " << error.message << "\n";
});
```

---

## Common Usage Examples

### Game Development

```cpp
livetuner::NlohmannBinder binder("game_config.json");

float player_speed, enemy_speed, gravity;
int max_enemies;
bool debug_mode;

binder.bind("player.speed", player_speed, 5.0f);
binder.bind("enemy.speed", enemy_speed, 3.0f);
binder.bind("physics.gravity", gravity, 9.8f);
binder.bind("spawn.max_enemies", max_enemies, 10);
binder.bind("debug", debug_mode, false);

while (game_running) {
    binder.update();  // Change values at runtime!
    
    // Game logic
    player.move(player_speed);
    physics.apply_gravity(gravity);
    // ...
}
```

### Simulation

```cpp
livetuner::NlohmannParams params("simulation.json");

while (simulation_running) {
    if (params.update()) {
        // Re-fetch parameters
        auto time_step = params.get<double>("sim.time_step", 0.01);
        auto iterations = params.get<int>("sim.iterations", 100);
        auto damping = params.get<float>("sim.damping", 0.99f);
        
        // Update simulation parameters
        simulation.set_time_step(time_step);
        simulation.set_iterations(iterations);
    }
    
    simulation.step();
}
```

### Algorithm Tuning

```cpp
livetuner::NlohmannParams params("algorithm.json");

while (running) {
    if (params.update()) {
        // Adjust algorithm parameters
        auto learning_rate = params.get<double>("ml.learning_rate", 0.001);
        auto batch_size = params.get<int>("ml.batch_size", 32);
        auto dropout = params.get<float>("ml.dropout", 0.5f);
        
        model.set_learning_rate(learning_rate);
        model.set_batch_size(batch_size);
    }
    
    model.train();
}
```

---

## Troubleshooting

### Q: Values are not being updated

✅ Are you calling `update()` periodically?  
✅ Is the JSON path correct? (e.g., `"player.speed"`)  
✅ Is the JSON file syntax correct?

### Q: Compile errors

✅ Is C++17 enabled? (`-std=c++17`)  
✅ Is `json.hpp` in the correct path?  
✅ On Windows, are you compiling with multi-threading support?

### Q: Link errors (pthread)

On Linux/Mac, the `-pthread` option is required:

```bash
g++ -std=c++17 main.cpp -o main -pthread
```

---

## Next Steps

- 📖 Read the [Detailed Documentation](NLOHMANN_ADAPTER.md)
- 💡 Try the [Sample Code](examples/example_nlohmann.cpp)
- 🧪 Check the [Test Code](Test/test_nlohmann_adapter.cpp)

---

**Happy Live Tuning! 🎮🔧**

# Async API Documentation

Detailed documentation for the async and non-blocking APIs of the CppLiveTuner library.

## Overview

In live tuning, there are cases where you don't want to block the main thread while waiting for parameter changes.
Especially in game loops and GUI applications, you need to monitor parameters while maintaining frame rate.

CppLiveTuner provides the following API variations:

### Single Value API

| Function | Feature | Use Case |
|----------|---------|----------|
| `tune_try()` | Immediate check, non-blocking | Game loops, real-time processing |
| `tune_timeout()` | Wait with timeout | Preventing long freezes |
| `tune_async<T>()` | Async version returning `std::future` | Background processing |
| `tune_async<T>(callback)` | Callback invocation | Event-driven programming |

### Params API

| Function | Feature | Use Case |
|----------|---------|----------|
| `params.update()` | Immediate check, non-blocking | Manual update loop |
| `params.start_watching()` + `poll()` | Background monitoring | Auto update |
| `params.on_change(callback)` | Callback on change | Event-driven |

---

## 1. tune_try - Non-blocking Immediate Check

The most lightweight API. Immediately checks if the file has changed at the time of call.

### Signature

```cpp
template<typename T>
bool tune_try(T& value);
```

### Supported Types

- `int`, `float`, `double`
- Other types with `>>` operator defined

### Usage Example

```cpp
#include "LiveTuner.h"

int main() {
    float speed = 1.0f;
    
    // Usage example in game loop
    while (game_running) {
        // Non-blocking parameter check
        if (tune_try(speed)) {
            std::cout << "Speed updated: " << speed << "\n";
        }
        
        // Update game (not blocked!)
        update_game(speed);
        render_frame();
        
        // 60 FPS
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }
    
    return 0;
}
```

### Performance

`tune_try()` performs the following optimizations:

1. **File modification time caching**: For consecutive calls within 10ms, determines from cache without opening the file
2. **Lightweight filesystem check**: Uses `std::filesystem::last_write_time()`
3. **Conditional file reading**: Opens file only when update is detected

---

## 2. tune_timeout - Wait with Timeout

Returns `false` if the parameter cannot be read within the specified time.

### Signature

```cpp
template<typename T>
bool tune_timeout(T& value, std::chrono::milliseconds timeout);
```

### Usage Example

```cpp
#include "LiveTuner.h"
#include <chrono>

int main() {
    float difficulty = 1.0f;
    
    std::cout << "Please enter difficulty in params.txt (within 5 seconds)...\n";
    
    // Wait for 5 seconds
    if (tune_timeout(difficulty, std::chrono::seconds(5))) {
        std::cout << "Difficulty set: " << difficulty << "\n";
    } else {
        std::cout << "Timeout! Using default value\n";
        difficulty = 1.0f;
    }
    
    start_game(difficulty);
    return 0;
}
```

---

## 3. tune_async - Future Version

Async version that returns `std::future<T>`. Waits for input in a background thread.

### Signature

```cpp
template<typename T>
std::future<T> tune_async();
```

### Usage Example

```cpp
#include "LiveTuner.h"
#include <future>

int main() {
    // Start async parameter wait
    auto future = tune_async<float>();
    
    // Main thread continues with other processing
    std::cout << "Waiting for parameters in background...\n";
    
    // Periodically check for results
    while (future.wait_for(std::chrono::milliseconds(100)) != std::future_status::ready) {
        std::cout << "Loading...\n";
        show_loading_screen();
    }
    
    // Get result
    float value = future.get();
    std::cout << "Parameter received: " << value << "\n";
    
    return 0;
}
```

---

## 4. tune_async(callback) - Callback Version

Specify a callback function to be called when the parameter is read.

### Signature

```cpp
template<typename T, typename Callback>
void tune_async(Callback callback);
```

### Usage Example

```cpp
#include "LiveTuner.h"
#include <atomic>

int main() {
    std::atomic<bool> config_loaded{false};
    float config_value = 0.0f;
    
    // Set callback and start async wait
    tune_async<float>([&](float value) {
        config_value = value;
        std::cout << "Value received via callback: " << value << "\n";
        config_loaded = true;
    });
    
    // Main thread continues immediately
    std::cout << "Main thread is continuing...\n";
    
    // Show splash screen until config is loaded
    while (!config_loaded) {
        show_splash_screen();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    start_with_config(config_value);
    return 0;
}
```

### Notes

- Callbacks are called from a different thread
- Threads are `detach()`ed, so use `std::atomic` etc. if explicit termination wait is needed

---

## 5. Params::update - Non-blocking Update

Recommended API when using named parameters.

### Usage Example

```cpp
#include "LiveTuner.h"

int main() {
    livetuner::Params params("game_config.json");
    
    float player_speed = 5.0f;
    float gravity = 9.8f;
    int max_enemies = 10;
    bool god_mode = false;
    
    params.bind("player_speed", player_speed, 5.0f);
    params.bind("gravity", gravity, 9.8f);
    params.bind("max_enemies", max_enemies, 10);
    params.bind("god_mode", god_mode, false);
    
    while (game_running) {
        // Non-blocking check and update all parameters
        if (params.update()) {
            std::cout << "Settings updated!\n";
            std::cout << "  Speed: " << player_speed << "\n";
            std::cout << "  Gravity: " << gravity << "\n";
            std::cout << "  Max enemies: " << max_enemies << "\n";
            std::cout << "  God mode: " << (god_mode ? "ON" : "OFF") << "\n";
        }
        
        update_game();
        render_frame();
    }
    
    return 0;
}
```

---

## 6. Params::start_watching + poll - Background Monitoring

Background file monitoring using OS native APIs.

### Usage Example

```cpp
#include "LiveTuner.h"

int main() {
    livetuner::Params params("config.json");
    
    float speed = 1.0f;
    params.bind("speed", speed, 1.0f);
    
    // Start background monitoring
    params.start_watching();
    
    while (game_running) {
        // Check for changes with poll() (almost zero CPU load)
        if (params.poll()) {
            std::cout << "Speed changed to " << speed << "\n";
        }
        
        player.move(speed);
        render();
    }
    
    // Stop monitoring (also auto-stops in destructor)
    params.stop_watching();
    
    return 0;
}
```

### Difference between start_watching and update

| Method | CPU Load | Latency | Use Case |
|--------|----------|---------|----------|
| `update()` | Low to Medium (polling) | Cache period (10ms) | Simple use |
| `start_watching()` + `poll()` | Almost zero | OS event dependent (almost instant) | Serious game development |

---

## 7. on_change Callback

Automatically calls a callback when parameters change.

### Usage Example

```cpp
#include "LiveTuner.h"

int main() {
    livetuner::Params params("audio_config.json");
    
    float master_volume = 1.0f;
    float music_volume = 0.8f;
    float sfx_volume = 1.0f;
    
    params.bind("master_volume", master_volume, 1.0f);
    params.bind("music_volume", music_volume, 0.8f);
    params.bind("sfx_volume", sfx_volume, 1.0f);
    
    // Set callback for changes
    params.on_change([&]() {
        std::cout << "Audio settings changed\n";
        
        // Apply to audio engine
        audio_engine.setMasterVolume(master_volume);
        audio_engine.setMusicVolume(music_volume);
        audio_engine.setSFXVolume(sfx_volume);
    });
    
    params.start_watching();
    
    while (running) {
        params.poll();  // on_change is automatically called when there are changes
        // ...
    }
    
    return 0;
}
```

---

## Parameter File Formats

### params.txt (for single values)

```
# Comment line (lines starting with # are ignored)
2.5

# Empty lines are also ignored
```

### config.json (for named parameters)

```json
{
    "player_speed": 5.0,
    "jump_height": 10.0,
    "gravity": 9.8,
    "god_mode": false,
    "player_name": "Hero"
}
```

### config.yaml

```yaml
# Game settings
player_speed: 5.0
jump_height: 10.0
gravity: 9.8
god_mode: false
player_name: Hero
```

### config.ini

```ini
# Game settings
player_speed = 5.0
jump_height = 10.0
gravity = 9.8
god_mode = false
player_name = Hero
```

---

## Usage Scenarios

### Game Development - Parameter Tuning

```cpp
void game_loop() {
    livetuner::Params params("gameplay.json");
    
    float jump_force, move_speed, air_control;
    
    params.bind("jump_force", jump_force, 15.0f);
    params.bind("move_speed", move_speed, 5.0f);
    params.bind("air_control", air_control, 0.3f);
    
    params.start_watching();
    
    while (!should_quit) {
        params.poll();
        
        // Changes are reflected immediately!
        // Adjust game feel without restarting
        player.setJumpForce(jump_force);
        player.setMoveSpeed(move_speed);
        player.setAirControl(air_control);
        
        update();
        render();
    }
}
```

### Shader Development - Real-time Preview

```cpp
void render_loop() {
    livetuner::Params params("shader_params.json");
    
    float exposure, contrast, saturation;
    float bloom_threshold, bloom_intensity;
    
    params.bind("exposure", exposure, 1.0f);
    params.bind("contrast", contrast, 1.0f);
    params.bind("saturation", saturation, 1.0f);
    params.bind("bloom_threshold", bloom_threshold, 0.8f);
    params.bind("bloom_intensity", bloom_intensity, 1.0f);
    
    params.start_watching();
    
    while (rendering) {
        params.poll();
        
        // Update shader uniforms immediately
        postprocess_shader.setFloat("exposure", exposure);
        postprocess_shader.setFloat("contrast", contrast);
        postprocess_shader.setFloat("saturation", saturation);
        postprocess_shader.setFloat("bloom_threshold", bloom_threshold);
        postprocess_shader.setFloat("bloom_intensity", bloom_intensity);
        
        render_scene();
        apply_postprocess();
        present();
    }
}
```

### AI/Machine Learning - Hyperparameter Tuning

```cpp
void training_loop() {
    livetuner::Params params("hyperparams.json");
    
    float learning_rate, momentum, weight_decay;
    int batch_size;
    
    params.bind("learning_rate", learning_rate, 0.001f);
    params.bind("momentum", momentum, 0.9f);
    params.bind("weight_decay", weight_decay, 0.0001f);
    params.bind("batch_size", batch_size, 32);
    
    params.on_change([&]() {
        std::cout << "Hyperparameters updated\n";
        optimizer.setLearningRate(learning_rate);
        optimizer.setMomentum(momentum);
        optimizer.setWeightDecay(weight_decay);
    });
    
    params.start_watching();
    
    for (int epoch = 0; epoch < num_epochs; ++epoch) {
        params.poll();
        train_epoch(batch_size);
        evaluate();
    }
}
```

---

## Troubleshooting

### tune_try() always returns false

- Check if the parameter file contains a valid value
- Ensure it's not just comment lines or empty lines
- Check if the value type is correct (e.g., decimal entered when integer expected)

### Callback is not called

- Ensure `poll()` is called in the loop
- Ensure `start_watching()` has been called
- Check that the main thread hasn't exited first

### File changes are not detected

- Ensure the file is properly saved (check editor auto-save settings)
- Verify the file path is correct
- Check if event-driven mode is enabled: `tune_is_event_driven()`

---

## Reference

- [README.md](README.md) - Main documentation
- [LiveTuner.h](include/LiveTuner.h) - Source code

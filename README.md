<div align="center">

# 🎚️ CppLiveTuner

### **Stop Recompiling. Start Tuning.**

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![C++17](https://img.shields.io/badge/C%2B%2B-17-blue.svg)](https://isocpp.org/std/the-standard)
[![Header-only](https://img.shields.io/badge/header--only-yes-green.svg)](https://en.wikipedia.org/wiki/Header-only)
[![Platform](https://img.shields.io/badge/platform-Windows%20%7C%20Linux%20%7C%20macOS-lightgrey.svg)]()

**A live parameter tuning library for C++.**  
*Edit a config file → See changes instantly in your running program. No rebuild required.*

[📖 日本語版ドキュメント](README_JA.md)

</div>

---

## ✨ Why CppLiveTuner?

Tired of the **edit → compile → run → check → repeat** cycle just to tweak a single value?

```
❌ Traditional workflow:
   Change value → Recompile (30 sec~) → Restart → Test → Repeat...

✅ With CppLiveTuner:
   Change value → Save file → See result instantly!
```

**CppLiveTuner eliminates recompilation entirely** for parameter tuning. Whether you're adjusting game physics, tweaking shader values, or fine-tuning UI animations, you'll see results in real-time.

### 🎯 Perfect For

| Use Case | Example |
|----------|---------|
| 🎮 **Game Development** | Tweak jump height, movement speed, spawn rates while playing |
| 🎨 **Graphics/Shaders** | Adjust exposure, bloom, color grading in real-time |
| 🤖 **Robotics/Simulation** | Tune PID parameters, sensor thresholds live |
| 🔧 **Debug/Profiling** | Toggle debug displays, adjust log levels on the fly |

---

## 🚀 Features

| Feature | Description |
|---------|-------------|
| ⚡ **Zero Recompilation** | Change parameters while the program is running |
| 🔌 **Header-only** | Single file, no dependencies. Just `#include` and go |
| 🛡️ **STB-style** | No `Windows.h` pollution in your project |
| 🎯 **Non-blocking API** | Perfect for game loops and real-time apps |
| 📝 **Multiple Formats** | JSON, YAML, INI, plain text |
| 👁️ **Event-driven Watching** | Native OS APIs (inotify/FSEvents/ReadDirectoryChanges) |
| 🧵 **Thread-safe** | Full mutex protection |
| 🌐 **Cross-platform** | Windows, Linux, macOS |
| 🧪 **Testable Design** | DI support, mock interfaces, test fixtures |
| 📚 **nlohmann/json Support** | Optional adapter for nlohmann/json users |

---

## 📦 Installation

**Just copy one file** — that's it!

```bash
# Option 1: Copy directly
cp LiveTuner.h /your/project/include/

# Option 2: Git submodule
git submodule add https://github.com/andogensi/CppLiveTuner.git vendor/CppLiveTuner
```

---

## 🏁 Quick Start

### 1️⃣ Setup (STB-style Header-only)

```cpp
// In main.cpp (or ONE dedicated file)
#define LIVETUNER_IMPLEMENTATION
#include "LiveTuner.h"
```

```cpp
// In all other files — just include, no Windows.h pollution!
#include "LiveTuner.h"
```

### 2️⃣ Simple Usage — Single Value

```cpp
#define LIVETUNER_IMPLEMENTATION
#include "LiveTuner.h"

int main() {
    float speed = 1.0f;
    
    while (running) {
        tune_try(speed);         // ← Checks params.txt, updates if changed
        player.move(speed);
    }
}
```

**Edit `params.txt` while running:**
```
2.5
```
→ `speed` instantly becomes `2.5`! 🎉

### 3️⃣ Recommended Usage — Named Parameters

```cpp
#define LIVETUNER_IMPLEMENTATION
#include "LiveTuner.h"

int main() {
    livetuner::Params params("config.json");
    
    float speed = 1.0f;
    float gravity = 9.8f;
    bool debug = false;
    
    // Bind variables — they auto-update when file changes!
    params.bind("speed", speed, 1.0f);
    params.bind("gravity", gravity, 9.8f);
    params.bind("debug", debug, false);
    
    while (running) {
        params.update();         // ← All bound variables updated automatically!
        
        player.move(speed);
        physics.setGravity(gravity);
        if (debug) showDebugInfo();
    }
}
```

**Edit `config.json` while running:**
```json
{
    "speed": 2.5,
    "gravity": 15.0,
    "debug": true
}
```
→ All three values update instantly! 🚀

---

## 🔨 Build

```bash
# GCC / MinGW (C++17+)
g++ -std=c++17 your_program.cpp -I include -o program

# MSVC (C++17+)
cl /std:c++17 your_program.cpp /I include

# Linux (requires pthread)
g++ -std=c++17 your_program.cpp -I include -pthread -o program
```

---

## 📖 API Reference

### Simple API — Single Value Tuning

| Function | Description |
|----------|-------------|
| `tune_init(path)` | Set parameter file (default: `params.txt`) |
| `tune_try(value)` | **Non-blocking**: Update if changed, returns `true` if updated |
| `tune(value)` | **Blocking**: Wait until value is available |
| `tune_timeout(value, dur)` | Returns `false` if timeout |
| `tune_async<T>()` | Returns `std::future<T>` |
| `tune_async<T>(callback)` | Async with callback |
| `tune_set_event_driven(bool)` | Enable/disable event-driven mode |

### Params API — Named Parameters

| Function | Description |
|----------|-------------|
| `Params(path, format)` | Constructor (format: Auto/Json/Yaml/KeyValue/Plain) |
| `bind(name, var, default)` | Bind variable to parameter |
| `update()` | **Non-blocking**: Update all bindings if changed |
| `get<T>(name)` | Get as `std::optional<T>` |
| `get_or<T>(name, default)` | Get with default value |
| `on_change(callback)` | Set change callback |
| `start_watching()` / `poll()` | Background file monitoring |

---

## 📁 Supported File Formats

| Format | Extension | Example |
|--------|-----------|---------|
| **JSON** | `.json` | `{"speed": 1.5, "debug": true}` |
| **YAML** | `.yaml`, `.yml` | `speed: 1.5` (simple key-value only) |
| **INI** | `.ini`, `.cfg` | `speed = 1.5` |
| **Plain Text** | `.txt` | `1.5` |

<details>
<summary>📄 Format Examples (click to expand)</summary>

### JSON
```json
{
    "speed": 1.5,
    "gravity": 9.8,
    "debug": true
}
```

### YAML (Simple Key-Value Only)

> ⚠️ **Limitation**: CppLiveTuner supports **only simple `key: value` format**. This is a lightweight parser designed for parameter tuning, not a full YAML parser.

**✅ Supported:**
```yaml
# Comments are supported
speed: 1.5
gravity: 9.8
debug: true
name: "player1"
---
# Document markers are ignored
```

**❌ NOT Supported:**
```yaml
# Nested structures
player:
  speed: 1.5      # ❌ Nested objects not supported
  position:
    x: 100        # ❌ Multi-level nesting not supported

# Arrays/Lists
items:            # ❌ Arrays not supported
  - sword
  - shield

scores: [1, 2, 3] # ❌ Inline arrays not supported

# Multi-line strings
description: |    # ❌ Block scalars not supported
  This is a
  multi-line text

# Anchors and aliases
defaults: &defaults  # ❌ Anchors not supported
  speed: 1.0
```

**💡 Need full YAML support?**  
Use JSON format instead, or integrate a full YAML parser library (like yaml-cpp) and convert to JSON before passing to CppLiveTuner.

### INI / Key-Value
```ini
speed = 1.5
gravity = 9.8
debug = true
```

### Plain Text
```
1.5
```
</details>

---

## ⚡ Event-Driven File Watching

CppLiveTuner uses **native OS APIs** for maximum efficiency:

| OS | API | Latency |
|----|-----|---------|
| Windows | `ReadDirectoryChangesW` | ~1ms |
| Linux | `inotify` | ~1ms |
| macOS | `FSEvents` | ~1ms |

### Polling vs Event-Driven

| | Polling 🐢 | Event-Driven ⚡ |
|---|---|---|
| **CPU Usage** | High (constant) | Near zero |
| **Latency** | 50-100ms | Real-time |
| **SSD Impact** | Wear over time | None |

---

## 🎮 Real-World Use Cases

### Game Development
```cpp
float jump_height = 10.0f, move_speed = 5.0f;
params.bind("jump_height", jump_height);
params.bind("move_speed", move_speed);

while (game_running) {
    params.update();
    player.jump(jump_height);    // Tweak while playing!
    player.move(move_speed);
}
```

### Shader/Graphics Tuning
```cpp
float exposure = 1.0f, bloom = 0.8f;
params.bind("exposure", exposure);
params.bind("bloom_threshold", bloom);

while (rendering) {
    params.update();
    shader.setUniform("exposure", exposure);
    shader.setUniform("bloom", bloom);
}
```

### Debug Toggles
```cpp
bool show_fps = true, show_hitboxes = false;
params.bind("show_fps", show_fps);
params.bind("show_hitboxes", show_hitboxes);

params.on_change([]() {
    std::cout << "Debug settings changed!\n";
});
```

---

## 🧪 Testable Design

CppLiveTuner supports **dependency injection** for enterprise-grade testability.

<details>
<summary>🔧 Testing Patterns (click to expand)</summary>

### TestFixture for Unit Tests
```cpp
TEST(MyTest, TestSomething) {
    livetuner::TestFixture fixture;  // Auto-reset state
    tune_init("test.txt");
    // Test runs in isolation
}
```

### Interface-Based DI
```cpp
class ConfigManager {
    livetuner::IParams& params_;
public:
    explicit ConfigManager(livetuner::IParams& p) : params_(p) {}
    float get_speed() { return params_.get_or("speed", 1.0f); }
};

// Production
livetuner::Params real_params("config.json");
livetuner::ParamsAdapter adapter(real_params);
ConfigManager config(adapter);

// Test with mock
class MockParams : public livetuner::IParams { /* ... */ };
```

### ScopedContext for Parallel Tests
```cpp
void test_thread_1() {
    livetuner::LiveTuner tuner("test1.txt");
    livetuner::ScopedContext ctx(tuner, params);
    game_loop();  // Uses test1.txt (thread-local)
}
```
</details>

---

## ⚙️ Advanced Configuration

<details>
<summary>Preprocessor Options (click to expand)</summary>

### External picojson
```cpp
#define LIVETUNER_USE_EXTERNAL_PICOJSON
#include "picojson.h"
#include "LiveTuner.h"
```

### nlohmann/json Support
```cpp
#define LIVETUNER_USE_NLOHMANN_JSON
#include <nlohmann/json.hpp>
#define LIVETUNER_IMPLEMENTATION
#include "LiveTuner.h"

livetuner::NlohmannParams params("config.json");
```

### Windows.h Configuration
```cpp
#define LIVETUNER_NO_WIN32_LEAN  // Keep full Windows.h
#define LIVETUNER_NO_NOMINMAX    // Keep min/max macros
#include "LiveTuner.h"
```

### Logging Control
```cpp
// Disable default stderr logging
#define LIVETUNER_ENABLE_DEFAULT_LOGGING 0
#include "LiveTuner.h"

// Or use custom logger
livetuner::set_log_callback([](LogLevel lvl, const std::string& msg) {
    MyEngine::Log(lvl, msg);
});
```
</details>

---

## 📂 Project Structure

```
CppLiveTuner/
├── include/
│   └── LiveTuner.h          # 🎯 Main header (just include this!)
├── examples/
│   └── example.cpp          # Usage examples
├── Test/                    # Comprehensive test suite
├── cmake/                   # CMake support files
└── README.md
```

---

## ❓ Troubleshooting

| Problem | Solution |
|---------|----------|
| File not detected | Use absolute paths, check permissions |
| Values not updating | Verify JSON syntax, ensure `update()` is called |
| Build errors | Enable C++17: `-std=c++17` or `/std:c++17` |

---

## 🤝 Contributing

Contributions welcome! Feel free to:
- 🐛 Report bugs
- 💡 Suggest features  
- 🔧 Submit pull requests

---

## 📜 License

**MIT License** — Free for personal and commercial use.

This project includes third-party components:
- **picojson** (embedded in LiveTuner.h) — BSD 2-Clause License © Kazuho Oku, Cybozu Labs, Inc.
- **nlohmann/json** (Json.hpp, optional) — MIT License © Niels Lohmann

See [LICENSE](LICENSE) for full details.

---

<div align="center">

**[⭐ Star this repo](https://github.com/andogensi/CppLiveTuner)** if you find it useful!

Made with ❤️ for the C++ community

</div>

/**
 * @file example_nlohmann.cpp
 * @brief nlohmann/json adapter usage example
 * 
 * This sample demonstrates how to use nlohmann/json with LiveTuner
 * to handle complex JSON structures.
 * 
 * Prerequisites:
 *   - nlohmann/json library (https://github.com/nlohmann/json)
 *   - Install via vcpkg: vcpkg install nlohmann-json
 *   - Or download json.hpp from GitHub releases
 * 
 * Build instructions:
 *   g++ -std=c++17 examples/example_nlohmann.cpp -I include -I <nlohmann_include_path> -o example_nlohmann.exe
 * 
 * Or using CMake:
 *   mkdir build && cd build
 *   cmake ..
 *   cmake --build . --target example_nlohmann
 * 
 * Edit config_nlohmann.json during execution
 * and values will be updated in real-time.
 */

// Step 1: Define LIVETUNER_USE_NLOHMANN_JSON to enable nlohmann/json support
#define LIVETUNER_USE_NLOHMANN_JSON

// Step 2: Include nlohmann/json BEFORE LiveTuner.h
#include <nlohmann/json.hpp>

// Step 3: Define LIVETUNER_IMPLEMENTATION in exactly ONE source file
#define LIVETUNER_IMPLEMENTATION
#include "../include/LiveTuner.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <iomanip>

using json = nlohmann::json;

/**
 * @brief Structure to store player information
 */
struct Player {
    std::string name;
    float speed;
    float health;
    std::vector<float> position;
    bool debug_mode;
};

/**
 * @brief Basic usage example
 */
void basic_example() {
    std::cout << "=== Basic Usage Example ===\n\n";
    
    // Create JSON file if it doesn't exist
    std::ofstream config("config_nlohmann.json");
    config << R"({
  "player": {
    "name": "Hero",
    "speed": 2.5,
    "health": 100.0,
    "position": [10.0, 20.0, 30.0]
  },
  "enemy": {
    "name": "Goblin",
    "speed": 1.8,
    "health": 50.0
  },
  "settings": {
    "debug": true,
    "difficulty": "normal",
    "max_enemies": 10
  }
})";
    config.close();
    
    std::cout << "Created config_nlohmann.json.\n";
    std::cout << "Edit this file and values will be updated in real-time.\n\n";
    
    livetuner::NlohmannParams params("config_nlohmann.json");
    
    // Error handling
    params.set_error_callback([](const livetuner::ErrorInfo& error) {
        std::cerr << "Error: " << error.message << "\n";
    });
    
    for (int frame = 0; frame < 100; ++frame) {
        if (params.update()) {
            std::cout << "\n[Frame " << frame << "] JSON file updated!\n";
            
            // Get values
            auto player_name = params.get<std::string>("player.name", "Unknown");
            auto player_speed = params.get<float>("player.speed", 1.0f);
            auto player_health = params.get<float>("player.health", 100.0f);
            auto position = params.get<std::vector<float>>("player.position", {0.0f, 0.0f, 0.0f});
            auto debug_mode = params.get<bool>("settings.debug", false);
            
            std::cout << "Player: " << player_name << "\n";
            std::cout << "  Speed: " << player_speed << "\n";
            std::cout << "  Health: " << player_health << "\n";
            std::cout << "  Position: [" << position[0] << ", " 
                      << position[1] << ", " << position[2] << "]\n";
            std::cout << "  Debug mode: " << (debug_mode ? "ON" : "OFF") << "\n";
        }
        
        if (frame % 20 == 0) {
            auto player_name = params.get<std::string>("player.name", "Unknown");
            std::cout << "[Frame " << frame << "] " << player_name << " is moving...\n";
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

/**
 * @brief Automatic binding example
 */
void binding_example() {
    std::cout << "\n\n=== Automatic Binding Example ===\n\n";
    
    // Create JSON file
    std::ofstream config("config_binding.json");
    config << R"({
  "player": {
    "name": "Warrior",
    "speed": 3.0,
    "health": 150.0,
    "position": [0.0, 0.0, 0.0]
  },
  "settings": {
    "debug": false
  }
})";
    config.close();
    
    std::cout << "Created config_binding.json.\n\n";
    
    livetuner::NlohmannBinder binder("config_binding.json");
    
    // Bind variables
    std::string player_name;
    float player_speed;
    float player_health;
    std::vector<float> position;
    bool debug_mode;
    
    binder.bind("player.name", player_name, std::string("Unknown"));
    binder.bind("player.speed", player_speed, 1.0f);
    binder.bind("player.health", player_health, 100.0f);
    binder.bind("player.position", position, std::vector<float>{0.0f, 0.0f, 0.0f});
    binder.bind("settings.debug", debug_mode, false);
    
    std::cout << "Initial values:\n";
    std::cout << "  Name: " << player_name << "\n";
    std::cout << "  Speed: " << player_speed << "\n";
    std::cout << "  Health: " << player_health << "\n\n";
    
    for (int frame = 0; frame < 100; ++frame) {
        if (binder.update()) {
            std::cout << "\n[Frame " << frame << "] Auto-updated!\n";
            std::cout << "  Name: " << player_name << "\n";
            std::cout << "  Speed: " << player_speed << "\n";
            std::cout << "  Health: " << player_health << "\n";
            std::cout << "  Position: [" << position[0] << ", " 
                      << position[1] << ", " << position[2] << "]\n";
            std::cout << "  Debug: " << (debug_mode ? "ON" : "OFF") << "\n";
        }
        
        if (frame % 20 == 0) {
            std::cout << "[Frame " << frame << "] " << player_name 
                      << " (Speed: " << player_speed << ")\n";
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

/**
 * @brief Example with arrays and nested objects
 */
void advanced_example() {
    std::cout << "\n\n=== Advanced Usage Example (Arrays and Nesting) ===\n\n";
    
    // Create complex JSON file
    std::ofstream config("config_advanced.json");
    config << R"({
  "game": {
    "title": "Epic Adventure",
    "version": "1.0.0",
    "settings": {
      "graphics": {
        "resolution": [1920, 1080],
        "quality": "high",
        "fps_limit": 60
      },
      "audio": {
        "master_volume": 0.8,
        "music_volume": 0.6,
        "sfx_volume": 0.7
      }
    }
  },
  "players": [
    {
      "name": "Player1",
      "score": 1000,
      "level": 5
    },
    {
      "name": "Player2",
      "score": 1500,
      "level": 7
    }
  ],
  "colors": [255, 128, 64, 32]
})";
    config.close();
    
    std::cout << "Created config_advanced.json.\n\n";
    
    livetuner::NlohmannParams params("config_advanced.json");
    
    for (int frame = 0; frame < 100; ++frame) {
        if (params.update()) {
            std::cout << "\n[Frame " << frame << "] Configuration updated!\n\n";
            
            // Get nested values
            auto title = params.get<std::string>("game.title", "Unknown");
            auto resolution = params.get<std::vector<int>>("game.settings.graphics.resolution", {800, 600});
            auto quality = params.get<std::string>("game.settings.graphics.quality", "medium");
            auto fps_limit = params.get<int>("game.settings.graphics.fps_limit", 30);
            
            std::cout << "Game: " << title << "\n";
            std::cout << "Resolution: " << resolution[0] << "x" << resolution[1] << "\n";
            std::cout << "Quality: " << quality << "\n";
            std::cout << "FPS Limit: " << fps_limit << "\n\n";
            
            // Access array elements
            auto player1_name = params.get<std::string>("players[0].name", "");
            auto player1_score = params.get<int>("players[0].score", 0);
            auto player2_name = params.get<std::string>("players[1].name", "");
            auto player2_score = params.get<int>("players[1].score", 0);
            
            std::cout << "Player Information:\n";
            std::cout << "  " << player1_name << ": " << player1_score << " pts\n";
            std::cout << "  " << player2_name << ": " << player2_score << " pts\n\n";
            
            // Audio settings
            auto master_vol = params.get<float>("game.settings.audio.master_volume", 1.0f);
            auto music_vol = params.get<float>("game.settings.audio.music_volume", 1.0f);
            
            std::cout << "Audio:\n";
            std::cout << "  Master Volume: " << (master_vol * 100) << "%\n";
            std::cout << "  Music Volume: " << (music_vol * 100) << "%\n";
        }
        
        if (frame % 20 == 0) {
            std::cout << "[Frame " << frame << "] Game running...\n";
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

/**
 * @brief Example of manipulating entire JSON
 */
void json_manipulation_example() {
    std::cout << "\n\n=== JSON Manipulation Example ===\n\n";
    
    std::ofstream config("config_manipulation.json");
    config << R"({
  "test": {
    "value": 42
  }
})";
    config.close();
    
    livetuner::NlohmannParams params("config_manipulation.json");
    
    // Get entire JSON
    auto full_json = params.get_json();
    std::cout << "Initial JSON:\n" << full_json.dump(2) << "\n\n";
    
    // Set values
    params.set("test.value", 100);
    params.set("test.name", std::string("New Value"));
    params.set("new_section.enabled", true);
    
    std::cout << "After changing values:\n" << params.dump(2) << "\n\n";
    
    // Save to file
    if (params.save()) {
        std::cout << "Changes saved to file.\n";
    }
    
    // Get specific section
    auto test_section = params.get_json("test");
    std::cout << "\nTest Section:\n" << test_section.dump(2) << "\n";
}

int main() {
    std::cout << "╔════════════════════════════════════════════════════════╗\n";
    std::cout << "║  CppLiveTuner - nlohmann/json Adapter Sample          ║\n";
    std::cout << "╚════════════════════════════════════════════════════════╝\n\n";
    
    try {
        // Select example
        std::cout << "Select an example to run:\n";
        std::cout << "1. Basic usage example\n";
        std::cout << "2. Automatic binding\n";
        std::cout << "3. Advanced usage example (arrays and nesting)\n";
        std::cout << "4. JSON manipulation example\n";
        std::cout << "5. Run all\n";
        std::cout << "\nChoice (1-5): ";
        
        int choice;
        std::cin >> choice;
        
        std::cout << "\n";
        
        switch (choice) {
            case 1:
                basic_example();
                break;
            case 2:
                binding_example();
                break;
            case 3:
                advanced_example();
                break;
            case 4:
                json_manipulation_example();
                break;
            case 5:
                basic_example();
                binding_example();
                advanced_example();
                json_manipulation_example();
                break;
            default:
                std::cout << "Invalid choice. Running basic example.\n\n";
                basic_example();
                break;
        }
        
        std::cout << "\n\nExecution completed!\n";
        
    } catch (const std::exception& e) {
        std::cerr << "An error occurred: " << e.what() << "\n";
        return 1;
    }
    
    return 0;
}

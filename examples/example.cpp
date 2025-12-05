/**
 * @file example.cpp
 * @brief CppLiveTuner Usage Examples
 * 
 * This sample demonstrates the basic usage of CppLiveTuner.
 * 
 * Build instructions:
 *   g++ -std=c++17 examples/example.cpp -I include -o example.exe
 * 
 * Edit params.txt or config.json while the program is running,
 * and the values will be updated in real-time.
 */

// Define LIVETUNER_IMPLEMENTATION in exactly ONE source file
// This is required for STB-style single-header library
#define LIVETUNER_IMPLEMENTATION
#include "../include/LiveTuner.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <fstream>

using namespace livetuner;

// Simulated game loop
void simulated_game_loop() {
    std::cout << "=== Single Value Tuning Example ===\n";
    std::cout << "Edit params.txt to change the value\n";
    std::cout << "Example: 2.5\n\n";
    
    float speed = 1.0f;
    
    for (int frame = 0; frame < 100; ++frame) {
        // Check value non-blocking
        if (tune_try(speed)) {
            std::cout << "[Frame " << frame << "] Speed updated: " << speed << "\n";
        }
        
        // Game update (simulation)
        // player.move(speed);
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        if (frame % 20 == 0) {
            std::cout << "[Frame " << frame << "] Current speed: " << speed << "\n";
        }
    }
}

// Named parameters example
void named_params_example() {
    std::cout << "\n=== Named Parameters Example ===\n";
    std::cout << "Edit config.json to change the values\n";
    std::cout << "Example:\n";
    std::cout << "{\n";
    std::cout << "  \"speed\": 2.5,\n";
    std::cout << "  \"gravity\": 15.0,\n";
    std::cout << "  \"debug\": true\n";
    std::cout << "}\n\n";
    
    livetuner::Params params("config.json");
    
    float speed = 1.0f;
    float gravity = 9.8f;
    bool debug = false;
    
    params.bind("speed", speed, 1.0f);
    params.bind("gravity", gravity, 9.8f);
    params.bind("debug", debug, false);
    
    // Callback on change
    params.on_change([]() {
        std::cout << ">>> Settings changed! <<<\n";
    });
    
    for (int frame = 0; frame < 100; ++frame) {
        if (params.update()) {
            std::cout << "[Frame " << frame << "] Parameters updated:\n";
            std::cout << "  speed: " << speed << "\n";
            std::cout << "  gravity: " << gravity << "\n";
            std::cout << "  debug: " << (debug ? "true" : "false") << "\n";
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        if (frame % 20 == 0) {
            std::cout << "[Frame " << frame << "] Current values: speed=" << speed 
                      << ", gravity=" << gravity << ", debug=" << debug << "\n";
        }
    }
}

// Global API usage example
void global_api_example() {
    std::cout << "\n=== Global API Example ===\n";
    
    // Create settings.ini file if it doesn't exist
    std::ofstream settings("settings.ini");
    settings << "level = 1\n";
    settings << "volume = 0.8\n";
    settings.close();
    
    params_init("settings.ini");
    
    int level = 1;
    float volume = 0.8f;
    
    params_bind("level", level, 1);
    params_bind("volume", volume, 0.8f);
    
    params_on_change([]() {
        std::cout << "Settings file changed\n";
    });
    
    for (int i = 0; i < 50; ++i) {
        if (params_update()) {
            std::cout << "Level: " << level << ", Volume: " << volume << "\n";
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

// Timeout input example
void timeout_example() {
    std::cout << "\n=== Timeout Example ===\n";
    std::cout << "Enter a value in params.txt (within 5 seconds)...\n";
    
    float value = 0.0f;
    
    if (tune_timeout(value, std::chrono::seconds(5))) {
        std::cout << "Value received: " << value << "\n";
    } else {
        std::cout << "Timeout! Using default value\n";
        value = 1.0f;
    }
    
    std::cout << "Using value: " << value << "\n";
}

// get/get_or API example
void get_api_example() {
    std::cout << "\n=== get API Example ===\n";
    
    livetuner::Params params("config.json");
    params.update();
    
    // Get value with std::optional
    auto speed_opt = params.get<float>("speed");
    if (speed_opt) {
        std::cout << "speed = " << *speed_opt << "\n";
    } else {
        std::cout << "speed is not set\n";
    }
    
    // Get with default value
    float gravity = params.get_or<float>("gravity", 9.8f);
    std::cout << "gravity = " << gravity << "\n";
    
    // Non-existent key
    int missing = params.get_or<int>("missing_key", 42);
    std::cout << "missing_key (default) = " << missing << "\n";
}

int main() {
    std::cout << "========================================\n";
    std::cout << "    CppLiveTuner Demo\n";
    std::cout << "========================================\n\n";
    
    // Run each example in sequence
    simulated_game_loop();
    named_params_example();
    global_api_example();
    timeout_example();
    get_api_example();
    
    std::cout << "\n========================================\n";
    std::cout << "    Demo Complete\n";
    std::cout << "========================================\n";
    
    return 0;
}

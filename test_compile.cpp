/**
 * @file test_compile.cpp
 * @brief Basic compilation test for LiveTuner
 * 
 * This test verifies that:
 * - LiveTuner.h compiles correctly
 * - Basic functionality works
 * - STB-style header-only usage is correct
 */

#define LIVETUNER_IMPLEMENTATION
#include "include/LiveTuner.h"

#include <iostream>
#include <stdexcept>

static void check(bool condition) {
    if (!condition) throw std::runtime_error("smoke test failed");
}

int main() {
    std::cout << "=== LiveTuner Compilation Test ===" << std::endl;
    
    // Test 1: Basic types compile
    {
        int i = 0;
        float f = 0.0f;
        double d = 0.0;
        bool b = false;
        std::string s = "test";
        
        // Suppress unused variable warnings
        (void)i;
        (void)f;
        (void)d;
        (void)b;
        (void)s;
        
        std::cout << "[PASS] Basic types compile" << std::endl;
    }
    
    // Test 2: ErrorInfo compiles and works
    {
        livetuner::ErrorInfo err;
        check(err.type == livetuner::ErrorType::None);
        check(!err);  // No error
        
        livetuner::ErrorInfo err2(livetuner::ErrorType::FileNotFound, "test.txt not found", "test.txt");
        check(err2.type == livetuner::ErrorType::FileNotFound);
        check(static_cast<bool>(err2));  // Has error
        
        std::cout << "[PASS] ErrorInfo works" << std::endl;
    }
    
    // Test 3: LogLevel and callback compile
    {
        livetuner::set_log_callback([](livetuner::LogLevel level, const std::string& msg) {
            // Suppress logs in test
            (void)level;
            (void)msg;
        });
        
        std::cout << "[PASS] Log callback works" << std::endl;
    }
    
    // Test 4: Params class compiles (without actual file)
    {
        livetuner::Params params("unused-smoke.json");
        int value = 0;
        params.bind("value", value, 7);
        check(value == 7 && params.get_bound_names().size() == 1);
        
        std::cout << "[PASS] Params class available" << std::endl;
    }
    
    // Test 5: FileFormat enum compiles
    {
        auto format = livetuner::FileFormat::Auto;
        format = livetuner::FileFormat::Json;
        format = livetuner::FileFormat::Plain;
        format = livetuner::FileFormat::KeyValue;
        (void)format;
        
        std::cout << "[PASS] FileFormat enum works" << std::endl;
    }
    
    std::cout << std::endl;
    std::cout << "=== All Compilation Tests Passed ===" << std::endl;
    
    return 0;
}

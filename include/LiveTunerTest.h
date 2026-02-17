#pragma once

/**
 * @file LiveTunerTest.h
 * @brief LiveTuner Test Support Library
 * 
 * This header provides testing utilities, dependency injection support,
 * and mock interfaces for LiveTuner. It is separated from the main
 * LiveTuner.h to keep the core library lightweight.
 * 
 * Include this header in your test code:
 * @code
 * #include "LiveTuner.h"
 * #include "LiveTunerTest.h"  // For testing utilities
 * @endcode
 * 
 * Or enable inline in LiveTuner.h:
 * @code
 * #define LIVETUNER_ENABLE_TEST_SUPPORT
 * #include "LiveTuner.h"
 * @endcode
 * 
 * Features provided:
 * - TestFixture: RAII helper for resetting global state
 * - ITuner/IParams: Interfaces for dependency injection and mocking
 * - TunerAdapter/ParamsAdapter: Adapters for DI patterns
 * - ScopedContext: Thread-local context for isolated testing
 * - TunerFactory/ParamsFactory: Factory pattern for instance creation
 */

#include "LiveTuner.h"

namespace livetuner {

// ============================================================
// Test Support Utilities
// ============================================================

/**
 * @brief Test helper for resetting global state (RAII)
 * 
 * Automatically resets global state when leaving scope.
 * Can be used as unit test fixture.
 * 
 * Example:
 * @code
 * TEST(MyTest, TestSomething) {
 *     livetuner::TestFixture fixture;  // Reset on test start
 *     
 *     tune_init("test.txt");
 *     float value = 1.0f;
 *     tune_try(value);
 *     
 *     // Automatic reset on test end
 * }
 * @endcode
 */
class TestFixture {
public:
    enum class ResetTarget {
        Tuner,      ///< Reset LiveTuner only
        Params,     ///< Reset Params only
        All         ///< Reset both
    };
    
    explicit TestFixture(ResetTarget target = ResetTarget::All) 
        : target_(target) {
        // Reset on test start
        reset();
    }
    
    ~TestFixture() {
        // Also reset on test end (to not affect next test)
        reset();
    }
    
    TestFixture(const TestFixture&) = delete;
    TestFixture& operator=(const TestFixture&) = delete;
    
    void reset() {
        switch (target_) {
        case ResetTarget::Tuner:
            internal::GlobalInstanceManager::instance().reset_tuner();
            break;
        case ResetTarget::Params:
            internal::GlobalInstanceManager::instance().reset_params();
            break;
        case ResetTarget::All:
            internal::GlobalInstanceManager::instance().reset_all();
            break;
        }
    }
    
private:
    ResetTarget target_;
};

// ============================================================
// Dependency Injection Support (For Enterprise/Production Code)
// ============================================================

/**
 * @brief Interface for parameter tuning
 * 
 * This interface allows for dependency injection and mocking in tests.
 * Use this in production code instead of global functions.
 * 
 * Example (production code):
 * @code
 * class GameEngine {
 *     livetuner::ITuner& tuner_;
 * public:
 *     explicit GameEngine(livetuner::ITuner& tuner) : tuner_(tuner) {}
 *     
 *     void update() {
 *         float speed = 1.0f;
 *         tuner_.try_get(speed);
 *         // ...
 *     }
 * };
 * @endcode
 * 
 * Example (test code):
 * @code
 * class MockTuner : public livetuner::ITuner {
 *     // Mock implementation...
 * };
 * 
 * TEST(GameEngine, UpdatesSpeed) {
 *     MockTuner mock;
 *     GameEngine engine(mock);
 *     // ...
 * }
 * @endcode
 */
class ITuner {
public:
    virtual ~ITuner() = default;
    
    virtual void set_file(std::string_view file_path) = 0;
    virtual std::string get_file() const = 0;
    
    template<typename T>
    bool try_get(T& value) {
        return do_try_get(&value, typeid(T));
    }
    
    template<typename T>
    void get(T& value) {
        do_get(&value, typeid(T));
    }
    
    template<typename T>
    bool get_timeout(T& value, std::chrono::milliseconds timeout) {
        return do_get_timeout(&value, typeid(T), timeout);
    }
    
    virtual void reset() = 0;
    virtual void set_event_driven(bool enabled) = 0;
    virtual bool is_event_driven() const = 0;
    virtual ErrorInfo get_last_error() const = 0;
    
protected:
    virtual bool do_try_get(void* value, const std::type_info& type) = 0;
    virtual void do_get(void* value, const std::type_info& type) = 0;
    virtual bool do_get_timeout(void* value, const std::type_info& type, 
                                std::chrono::milliseconds timeout) = 0;
};

/**
 * @brief Interface for named parameter management
 * 
 * This interface allows for dependency injection and mocking in tests.
 * 
 * Example (production code):
 * @code
 * class ConfigManager {
 *     livetuner::IParams& params_;
 * public:
 *     explicit ConfigManager(livetuner::IParams& params) : params_(params) {}
 *     
 *     void load() {
 *         params_.update();
 *     }
 *     
 *     float get_speed() {
 *         return params_.get_or("speed", 1.0f);
 *     }
 * };
 * @endcode
 */
class IParams {
public:
    virtual ~IParams() = default;
    
    virtual void set_file(std::string_view file_path, FileFormat format = FileFormat::Auto) = 0;
    virtual std::string get_file() const = 0;
    
    template<typename T>
    void bind(const std::string& name, T& variable, T default_value = T{}) {
        do_bind(name, &variable, typeid(T), &default_value);
    }
    
    virtual void unbind(const std::string& name) = 0;
    virtual void unbind_all() = 0;
    
    virtual bool update() = 0;
    virtual void start_watching() = 0;
    virtual void stop_watching() = 0;
    virtual bool poll() = 0;
    
    template<typename T>
    std::optional<T> get(const std::string& name) {
        T value{};
        if (do_get(name, &value, typeid(T))) {
            return value;
        }
        return std::nullopt;
    }
    
    template<typename T>
    T get_or(const std::string& name, T default_value) {
        T value{};
        if (do_get(name, &value, typeid(T))) {
            return value;
        }
        return default_value;
    }
    
    virtual void on_change(std::function<void()> callback) = 0;
    virtual ErrorInfo get_last_error() const = 0;
    
protected:
    virtual void do_bind(const std::string& name, void* variable, 
                         const std::type_info& type, const void* default_value) = 0;
    virtual bool do_get(const std::string& name, void* value, const std::type_info& type) = 0;
};

/**
 * @brief Adapter that wraps LiveTuner to implement ITuner interface
 * 
 * Use this to inject a LiveTuner instance via the ITuner interface.
 * 
 * Example:
 * @code
 * livetuner::LiveTuner tuner("params.txt");
 * livetuner::TunerAdapter adapter(tuner);
 * 
 * // Pass adapter to code expecting ITuner&
 * GameEngine engine(adapter);
 * @endcode
 */
class TunerAdapter : public ITuner {
public:
    explicit TunerAdapter(LiveTuner& tuner) : tuner_(tuner) {}
    
    void set_file(std::string_view file_path) override {
        tuner_.set_file(file_path);
    }
    
    std::string get_file() const override {
        return tuner_.get_file();
    }
    
    void reset() override {
        tuner_.reset();
    }
    
    void set_event_driven(bool enabled) override {
        tuner_.set_event_driven(enabled);
    }
    
    bool is_event_driven() const override {
        return tuner_.is_event_driven();
    }
    
    ErrorInfo get_last_error() const override {
        return tuner_.last_error();
    }
    
protected:
    bool do_try_get(void* value, const std::type_info& type) override {
        if (type == typeid(int)) return tuner_.try_get(*static_cast<int*>(value));
        if (type == typeid(float)) return tuner_.try_get(*static_cast<float*>(value));
        if (type == typeid(double)) return tuner_.try_get(*static_cast<double*>(value));
        if (type == typeid(bool)) return tuner_.try_get(*static_cast<bool*>(value));
        if (type == typeid(std::string)) return tuner_.try_get(*static_cast<std::string*>(value));
        return false;
    }
    
    void do_get(void* value, const std::type_info& type) override {
        if (type == typeid(int)) tuner_.get(*static_cast<int*>(value));
        else if (type == typeid(float)) tuner_.get(*static_cast<float*>(value));
        else if (type == typeid(double)) tuner_.get(*static_cast<double*>(value));
        else if (type == typeid(bool)) tuner_.get(*static_cast<bool*>(value));
        else if (type == typeid(std::string)) tuner_.get(*static_cast<std::string*>(value));
    }
    
    bool do_get_timeout(void* value, const std::type_info& type,
                        std::chrono::milliseconds timeout) override {
        if (type == typeid(int)) return tuner_.get_timeout(*static_cast<int*>(value), timeout);
        if (type == typeid(float)) return tuner_.get_timeout(*static_cast<float*>(value), timeout);
        if (type == typeid(double)) return tuner_.get_timeout(*static_cast<double*>(value), timeout);
        if (type == typeid(bool)) return tuner_.get_timeout(*static_cast<bool*>(value), timeout);
        if (type == typeid(std::string)) return tuner_.get_timeout(*static_cast<std::string*>(value), timeout);
        return false;
    }
    
private:
    LiveTuner& tuner_;
};

/**
 * @brief Adapter that wraps Params to implement IParams interface
 * 
 * Use this to inject a Params instance via the IParams interface.
 * 
 * Example:
 * @code
 * livetuner::Params params("config.json");
 * livetuner::ParamsAdapter adapter(params);
 * 
 * // Pass adapter to code expecting IParams&
 * ConfigManager config(adapter);
 * @endcode
 */
class ParamsAdapter : public IParams {
public:
    explicit ParamsAdapter(Params& params) : params_(params) {}
    
    void set_file(std::string_view file_path, FileFormat format = FileFormat::Auto) override {
        params_.set_file(file_path, format);
    }
    
    std::string get_file() const override {
        return params_.get_file();
    }
    
    void unbind(const std::string& name) override {
        params_.unbind(name);
    }
    
    void unbind_all() override {
        params_.unbind_all();
    }
    
    bool update() override {
        return params_.update();
    }
    
    void start_watching() override {
        params_.start_watching();
    }
    
    void stop_watching() override {
        params_.stop_watching();
    }
    
    bool poll() override {
        return params_.poll();
    }
    
    void on_change(std::function<void()> callback) override {
        params_.on_change(std::move(callback));
    }
    
    ErrorInfo get_last_error() const override {
        return params_.last_error();
    }
    
protected:
    void do_bind(const std::string& name, void* variable, 
                 const std::type_info& type, const void* default_value) override {
        if (type == typeid(int)) 
            params_.bind(name, *static_cast<int*>(variable), *static_cast<const int*>(default_value));
        else if (type == typeid(float)) 
            params_.bind(name, *static_cast<float*>(variable), *static_cast<const float*>(default_value));
        else if (type == typeid(double)) 
            params_.bind(name, *static_cast<double*>(variable), *static_cast<const double*>(default_value));
        else if (type == typeid(bool)) 
            params_.bind(name, *static_cast<bool*>(variable), *static_cast<const bool*>(default_value));
        else if (type == typeid(std::string)) 
            params_.bind(name, *static_cast<std::string*>(variable), *static_cast<const std::string*>(default_value));
    }
    
    bool do_get(const std::string& name, void* value, const std::type_info& type) override {
        if (type == typeid(int)) {
            auto opt = params_.get<int>(name);
            if (opt) { *static_cast<int*>(value) = *opt; return true; }
        } else if (type == typeid(float)) {
            auto opt = params_.get<float>(name);
            if (opt) { *static_cast<float*>(value) = *opt; return true; }
        } else if (type == typeid(double)) {
            auto opt = params_.get<double>(name);
            if (opt) { *static_cast<double*>(value) = *opt; return true; }
        } else if (type == typeid(bool)) {
            auto opt = params_.get<bool>(name);
            if (opt) { *static_cast<bool*>(value) = *opt; return true; }
        } else if (type == typeid(std::string)) {
            auto opt = params_.get<std::string>(name);
            if (opt) { *static_cast<std::string*>(value) = *opt; return true; }
        }
        return false;
    }
    
private:
    Params& params_;
};

/**
 * @brief Scoped context for dependency injection
 * 
 * This class provides a scoped context that can be used to inject
 * custom Tuner/Params instances for a specific scope (e.g., a test).
 * Within the scope, get_context_tuner() and get_context_params() will
 * return the injected instances.
 * 
 * Example (test with isolated context):
 * @code
 * TEST(MyTest, TestWithInjection) {
 *     livetuner::LiveTuner tuner("test_params.txt");
 *     livetuner::Params params("test_config.json");
 *     
 *     livetuner::ScopedContext context(tuner, params);
 *     
 *     // Within this scope, context-aware code uses these instances
 *     auto& t = livetuner::get_context_tuner();  // Returns tuner
 *     auto& p = livetuner::get_context_params(); // Returns params
 * }
 * @endcode
 * 
 * Example (parallel tests):
 * @code
 * void test_thread_1() {
 *     livetuner::LiveTuner tuner("test1.txt");
 *     livetuner::Params params("config1.json");
 *     livetuner::ScopedContext context(tuner, params);
 *     
 *     // This thread uses tuner/params from its own context
 *     run_test_1();
 * }
 * 
 * void test_thread_2() {
 *     livetuner::LiveTuner tuner("test2.txt");
 *     livetuner::Params params("config2.json");
 *     livetuner::ScopedContext context(tuner, params);
 *     
 *     // This thread uses its own isolated context
 *     run_test_2();
 * }
 * @endcode
 */
class ScopedContext {
public:
    /**
     * @brief Create a scoped context with both Tuner and Params
     */
    ScopedContext(LiveTuner& tuner, Params& params)
        : prev_tuner_(current_tuner())
        , prev_params_(current_params())
    {
        current_tuner() = &tuner;
        current_params() = &params;
    }
    
    /**
     * @brief Create a scoped context with Tuner only
     */
    explicit ScopedContext(LiveTuner& tuner)
        : prev_tuner_(current_tuner())
        , prev_params_(current_params())
    {
        current_tuner() = &tuner;
    }
    
    /**
     * @brief Create a scoped context with Params only
     */
    explicit ScopedContext(Params& params)
        : prev_tuner_(current_tuner())
        , prev_params_(current_params())
    {
        current_params() = &params;
    }
    
    ~ScopedContext() {
        current_tuner() = prev_tuner_;
        current_params() = prev_params_;
    }
    
    ScopedContext(const ScopedContext&) = delete;
    ScopedContext& operator=(const ScopedContext&) = delete;
    
    /**
     * @brief Check if a context is active
     */
    static bool has_context() {
        return current_tuner() != nullptr || current_params() != nullptr;
    }
    
    /**
     * @brief Check if a Tuner context is active
     */
    static bool has_tuner_context() {
        return current_tuner() != nullptr;
    }
    
    /**
     * @brief Check if a Params context is active
     */
    static bool has_params_context() {
        return current_params() != nullptr;
    }
    
private:
    friend LiveTuner& get_context_tuner();
    friend Params& get_context_params();
    
    static LiveTuner*& current_tuner() {
        thread_local LiveTuner* ptr = nullptr;
        return ptr;
    }
    
    static Params*& current_params() {
        thread_local Params* ptr = nullptr;
        return ptr;
    }
    
    LiveTuner* prev_tuner_;
    Params* prev_params_;
};

/**
 * @brief Get the current context's Tuner
 * 
 * If a ScopedContext is active, returns the injected Tuner.
 * Otherwise, returns the global default Tuner.
 * 
 * This function is the recommended way to access a Tuner in code
 * that needs to support both production (global) and test (injected) scenarios.
 * 
 * Example:
 * @code
 * void game_loop() {
 *     float speed = 1.0f;
 *     livetuner::get_context_tuner().try_get(speed);
 *     // Works with global tuner in production
 *     // Works with injected tuner in tests
 * }
 * @endcode
 */
inline LiveTuner& get_context_tuner() {
    if (ScopedContext::current_tuner()) {
        return *ScopedContext::current_tuner();
    }
    return get_default_tuner();
}

/**
 * @brief Get the current context's Params
 * 
 * If a ScopedContext is active, returns the injected Params.
 * Otherwise, returns the global default Params.
 * 
 * This function is the recommended way to access Params in code
 * that needs to support both production (global) and test (injected) scenarios.
 * 
 * Example:
 * @code
 * void load_config() {
 *     auto& params = livetuner::get_context_params();
 *     params.update();
 *     // Works with global params in production
 *     // Works with injected params in tests
 * }
 * @endcode
 */
inline Params& get_context_params() {
    if (ScopedContext::current_params()) {
        return *ScopedContext::current_params();
    }
    return get_default_params();
}

/**
 * @brief Factory for creating configured Tuner/Params instances
 * 
 * This factory can be used to centralize instance creation configuration.
 * Useful when you want to apply consistent settings across the application.
 * 
 * Example:
 * @code
 * // Configure factory once
 * livetuner::TunerFactory::set_default_config([](livetuner::LiveTuner& tuner) {
 *     tuner.set_event_driven(true);
 *     auto config = tuner.get_read_retry_config();
 *     config.max_retries = 5;
 *     tuner.set_read_retry_config(config);
 * });
 * 
 * // Create instances with consistent configuration
 * auto tuner = livetuner::TunerFactory::create("params.txt");
 * @endcode
 */
class TunerFactory {
public:
    using Configurator = std::function<void(LiveTuner&)>;
    
    /**
     * @brief Set default configuration applied to all created instances
     */
    static void set_default_config(Configurator config) {
        get_config() = std::move(config);
    }
    
    /**
     * @brief Create a new LiveTuner instance with default configuration
     */
    static std::unique_ptr<LiveTuner> create(std::string_view file_path = "params.txt") {
        auto tuner = std::make_unique<LiveTuner>(file_path);
        if (get_config()) {
            get_config()(*tuner);
        }
        return tuner;
    }
    
    /**
     * @brief Create a new LiveTuner instance with custom configuration
     */
    static std::unique_ptr<LiveTuner> create(std::string_view file_path, 
                                              Configurator custom_config) {
        auto tuner = std::make_unique<LiveTuner>(file_path);
        if (get_config()) {
            get_config()(*tuner);
        }
        if (custom_config) {
            custom_config(*tuner);
        }
        return tuner;
    }
    
private:
    static Configurator& get_config() {
        static Configurator config;
        return config;
    }
};

/**
 * 
 * Similar to TunerFactory, but for Params instances.
 * 
 * Example:
 * @code
 * // Configure factory once
 * livetuner::ParamsFactory::set_default_config([](livetuner::Params& params) {
 *     auto config = params.get_read_retry_config();
 *     config.max_retries = 5;
 *     params.set_read_retry_config(config);
 * });
 * 
 * // Create instances with consistent configuration
 * auto params = livetuner::ParamsFactory::create("config.json");
 * @endcode
 */
class ParamsFactory {
public:
    using Configurator = std::function<void(Params&)>;
    
    static void set_default_config(Configurator config) {
        get_config() = std::move(config);
    }

    static std::unique_ptr<Params> create(std::string_view file_path = "config.json",
                                           FileFormat format = FileFormat::Auto) {
        auto params = std::make_unique<Params>(file_path, format);
        if (get_config()) {
            get_config()(*params);
        }
        return params;
    }
    
    static std::unique_ptr<Params> create(std::string_view file_path,
                                           FileFormat format,
                                           Configurator custom_config) {
        auto params = std::make_unique<Params>(file_path, format);
        if (get_config()) {
            get_config()(*params);
        }
        if (custom_config) {
            custom_config(*params);
        }
        return params;
    }
    
private:
    static Configurator& get_config() {
        static Configurator config;
        return config;
    }
};

} // namespace livetuner

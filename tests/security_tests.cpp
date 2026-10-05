#ifdef LIVETUNER_USE_EXTERNAL_PICOJSON
#include <picojson.h>
#endif
#ifdef LIVETUNER_USE_NLOHMANN_JSON
#include <nlohmann/json.hpp>
#endif
#define LIVETUNER_IMPLEMENTATION
#include "LiveTuner.h"

#include <stdexcept>
#include <utility>

using namespace std::chrono_literals;
namespace fs = std::filesystem;

struct TestDirectory {
    fs::path previous = fs::current_path();
    fs::path path = fs::temp_directory_path() / ("livetuner-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    TestDirectory() {
        if (!fs::create_directory(path)) throw std::runtime_error("test directory already exists");
        fs::current_path(path);
    }
    ~TestDirectory() {
        std::error_code error;
        fs::current_path(previous, error);
        fs::remove_all(path, error); // Only the directory this test successfully created.
    }
};

static void write_file(const fs::path& path, const std::string& text) {
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    file.exceptions(std::ios::failbit | std::ios::badbit);
    file << text;
    file.close();
}

static void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

static void missing_file_regressions() {
    int logs = 0;
    livetuner::set_log_callback([&](livetuner::LogLevel, const std::string&) { ++logs; });
    livetuner::Params params("not-created.json");
    int named = 7, plain = 9;
    params.bind("x", named, 7);
    livetuner::tune_init("not-created.txt");
    auto started = std::chrono::steady_clock::now();
    for (int i = 0; i < 30; ++i) require(!params.update(), "missing Params file reported an update");
    const auto params_ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started);
    started = std::chrono::steady_clock::now();
    for (int i = 0; i < 30; ++i) require(!livetuner::tune_try(plain), "missing plain file reported an update");
    const auto tuner_ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started);
    std::cout << "30 missing reads: Params=" << params_ms.count() << "ms, tune_try=" << tuner_ms.count() << "ms, logs=" << logs << '\n';
    require(params_ms < 500ms && tuner_ms < 500ms, "non-blocking API slept while reading a missing file");
    require(logs == 2, "unchanged missing-file errors were logged repeatedly");
    require(named == 7 && plain == 9 && !fs::exists("not-created.json") && !fs::exists("not-created.txt"), "missing read changed defaults or created a file");

    // Retry configuration must never add sleeps to an update/try_get call.
    livetuner::FileReadRetryConfig retry;
    retry.retry_delay = 250ms;
    params.set_read_retry_config(retry);
    livetuner::get_default_tuner().set_read_retry_config(retry);
    params.invalidate_cache();
    livetuner::get_default_tuner().invalidate_cache();
    started = std::chrono::steady_clock::now();
    params.update();
    livetuner::tune_try(plain);
    require(std::chrono::steady_clock::now() - started < 500ms, "polling used the blocking retry policy");
    require(logs == 2, "cache invalidation re-logged an unchanged error");

    write_file("not-created.json", "{\"x\":11}");
    write_file("not-created.txt", "13\n");
    require(params.update() && livetuner::tune_try(plain) && named == 11 && plain == 13, "negative cache hid a newly created file");
    write_file("empty.json", "");
    params.set_file("empty.json");
    started = std::chrono::steady_clock::now();
    require(!params.update(), "empty configuration accepted");
    require(std::chrono::steady_clock::now() - started < 500ms, "empty-file read slept");
    livetuner::get_default_tuner().set_read_retry_config({});
    livetuner::tune_reset();
    livetuner::set_log_callback(nullptr);
}

static void timestamp_regressions() {
    // No advancing clock helper: use actual rapid writes, then force equal mtimes
    // to exercise coarse timestamp resolution deterministically on every OS.
    for (const bool same_time : {false, true}) {
        for (int trial = 0; trial < 50; ++trial) {
            write_file("rapid.json", "{\"x\":1}");
            write_file("rapid.txt", "1\n");
            livetuner::Params params("rapid.json");
            livetuner::LiveTuner tuner("rapid.txt");
            int named = 0, plain = 0;
            params.bind("x", named);
            require(params.update() && tuner.try_get(plain) && named == 1 && plain == 1, "rapid-write control failed");
            const auto json_time = fs::last_write_time("rapid.json");
            const auto text_time = fs::last_write_time("rapid.txt");
            for (const int value : {2, 3}) {
                // Bypass write_file entirely, so this test cannot inherit fake mtimes.
                { std::ofstream file("rapid.json"); file << "{\"x\":" << value << '}'; }
                { std::ofstream file("rapid.txt"); file << value << '\n'; }
                if (same_time) {
                    fs::last_write_time("rapid.json", json_time);
                    fs::last_write_time("rapid.txt", text_time);
                }
                params.update();
                tuner.try_get(plain);
            }
            const auto deadline = std::chrono::steady_clock::now() + 500ms;
            while ((named != 3 || plain != 3) && std::chrono::steady_clock::now() < deadline) {
                params.update();
                tuner.try_get(plain);
                std::this_thread::sleep_for(1ms);
            }
            require(named == 3 && plain == 3, "final rapid write remained stale without another save");
        }
        std::cout << (same_time ? "Equal-mtime" : "Native-mtime") << " writes: 50/50 final values observed\n";
    }
}

static void link_read_regressions() {
    fs::create_directory("assets");
    fs::create_directory("build");
    write_file("assets/config.json", "{\"x\":1}");
    write_file("assets/value.txt", "1\n");
    fs::create_symlink("../assets/config.json", "build/linked.json");
    fs::create_symlink("../assets/value.txt", "build/linked.txt");
    livetuner::Params params("build/linked.json");
    livetuner::LiveTuner tuner("build/linked.txt");
    int named = 7, plain = 7;
    params.bind("x", named, 7);
#ifdef LIVETUNER_USE_NLOHMANN_JSON
    livetuner::NlohmannParams optional("build/linked.json");
#endif
    if (LIVETUNER_STRICT_FILE_READS) {
        require(!params.update() && params.has_error() && named == 7, "strict Params followed a link");
        require(!tuner.try_get(plain) && tuner.has_error() && plain == 7, "strict LiveTuner followed a link");
#ifdef LIVETUNER_USE_NLOHMANN_JSON
        require(optional.get<int>("x", 7) == 7 && optional.last_error(), "strict nlohmann followed a link");
#endif
    } else {
        require(params.update() && tuner.try_get(plain) && named == 1 && plain == 1, "development link reads failed");
        params.start_watching();
        const auto json_time = fs::last_write_time("assets/config.json");
        const auto text_time = fs::last_write_time("assets/value.txt");
        write_file("assets/config.json", "{\"x\":2}");
        write_file("assets/value.txt", "2\n");
        fs::last_write_time("assets/config.json", json_time);
        fs::last_write_time("assets/value.txt", text_time);
        bool observed = false;
        const auto deadline = std::chrono::steady_clock::now() + 500ms;
        do {
            params.poll();
            tuner.try_get(plain);
            observed = named == 2 && plain == 2;
#ifdef LIVETUNER_USE_NLOHMANN_JSON
            optional.update();
            observed = observed && optional.get<int>("x") == 2;
#endif
            if (observed) break;
            std::this_thread::sleep_for(1ms);
        } while (std::chrono::steady_clock::now() < deadline);
        require(observed, "linked target outside the watched directory stayed stale");
#ifdef LIVETUNER_USE_NLOHMANN_JSON
        require(optional.set("x", 99), "optional unsaved edit setup failed");
        std::this_thread::sleep_for(20ms);
        require(!optional.update() && optional.get<int>("x") == 99, "periodic probe discarded an unsaved edit");
#endif
    }
    livetuner::ErrorInfo error;
    require(!livetuner::internal::write_regular_file("build/linked.txt", "overwrite", error), "read policy weakened linked-file writes");
#ifdef LIVETUNER_USE_NLOHMANN_JSON
    require(!optional.save(), "read policy weakened optional linked-file saves");
    auto disk = livetuner::internal::read_regular_file("assets/config.json");
    require(disk && *disk.content == (LIVETUNER_STRICT_FILE_READS ? "{\"x\":1}" : "{\"x\":2}"), "linked save changed target content");
#endif
}

static void parser_regressions() {
    using Parser = livetuner::internal::PicojsonParser;
    Parser::ValueMap values;
    require(Parser::parse(R"({"speed":12,"nested":{"enabled":true}})", values), "ordinary JSON rejected");
    require(values.at("speed") == "12", "ordinary JSON changed");
    std::string deep = "1";
    for (int i = 0; i < 150; ++i) deep = "{\"x\":" + deep + "}";
    require(!Parser::parse(deep, values), "JSON object depth limit bypassed");
    require(!Parser::parse(R"({"x":1e309})", values), "overflowing JSON number accepted");
    require(!Parser::parse(R"({"x":-1e309})", values), "negative overflow accepted");
    require(!Parser::parse(R"({"x":1} trailing)", values), "trailing data accepted");
    require(!Parser::parse(R"({"x":[1,2,})", values) && values.empty(), "partial JSON leaked");
    deep = "0";
    for (size_t i = 1; i < livetuner::internal::max_config_depth; ++i) deep = "[" + deep + "]";
    require(Parser::parse("{\"x\":" + deep + "}", values), "depth boundary rejected");
    require(!Parser::parse("{\"x\":[" + deep + "]}", values), "mixed depth limit bypassed");
    require(Parser::parse(R"({"x":1e-7,"text":"[\\\"{}]"})", values), "valid exponents or escaped delimiters rejected");
    double small = 0;
    require(livetuner::internal::parse_value(values.at("x"), small) && small == 1e-7, "small number rounded");
    require(!Parser::parse("{\"x\":\"" + std::string(livetuner::internal::max_value_bytes + 1, 'x') + "\"}", values), "large string accepted");
    require(!Parser::parse("{\"" + std::string(livetuner::internal::max_key_bytes + 1, 'x') + "\":1}", values), "large key accepted");
    std::string many = "{";
    for (size_t i = 0; i <= livetuner::internal::max_config_entries; ++i) {
        if (i) many += ',';
        many += "\"x" + std::to_string(i) + "\":1";
    }
    require(!Parser::parse(many + "}", values), "item budget bypassed");
    using KV = livetuner::internal::SimpleKeyValueParser;
    require(KV::parse("a=12\nb=true\n", values), "ordinary key/value rejected");
    require(KV::parse("a: 12\nb: true\n", values, true), "ordinary YAML subset rejected");
    require(!KV::parse("a=12\nbroken\n", values) && values.empty(), "partial key/value accepted");
    require(!KV::parse(std::string(livetuner::internal::max_config_bytes + 1, 'x'), values), "large direct parser input accepted");
#ifndef LIVETUNER_USE_EXTERNAL_PICOJSON
    livetuner::picojson::value raw;
    require(!livetuner::picojson::parse(raw, R"({"x":1e-9999})").empty(), "embedded underflow accepted");
#endif
}

static void ingress_regressions() {
    write_file("params.json", R"({"speed":12,"count":3})");
    int speed = 0, count = 0;
    livetuner::Params params("params.json");
    params.bind("speed", speed);
    params.bind("count", count);
    // Ingress assertions inspect a completed probe, not the short polling cache.
    // Actual rapid/equal-mtime writes are covered separately by timestamps.
    const auto refresh = [&] {
        std::this_thread::sleep_for(livetuner::Params::FileCache::cache_duration + 1ms);
        return params.update();
    };
    require(refresh() && speed == 12 && count == 3, "Params normal read failed");
    write_file("params.json", R"({"speed":99,"count":"invalid"})");
    require(!refresh() && params.has_error(), "invalid binding reported success");
    require(speed == 12 && count == 3 && params.get<int>("speed") == 12, "last good snapshot was partially overwritten");
    write_file("params.json", R"({"speed":1e309})");
    require(!refresh() && speed == 12, "numeric overflow corrupted bindings");
    write_file("params.json", std::string(livetuner::internal::max_config_bytes + 1, 'x'));
    require(!refresh() && speed == 12, "oversized file accepted");
    write_file("params.json", R"({"speed":15,"count":4})");
    require(refresh() && speed == 15, "recovery after invalid input failed");
    std::this_thread::sleep_for(20ms);
    require(!params.update(), "unchanged snapshot reparsed as update");
    write_file("params.json", "broken");
    require(!refresh() && params.has_error(), "invalid snapshot had no error");
    write_file("params.json", R"({"speed":15,"count":4})");
    require(!refresh() && !params.has_error() && speed == 15, "restoring the same good snapshot did not clear the error");

    livetuner::Params missing("missing.json");
    require(!missing.update() && !fs::exists("missing.json"), "read API created a file");
    livetuner::LiveTuner tuner("missing.txt");
    int value = 8;
    require(!tuner.try_get(value) && value == 8 && !fs::exists("missing.txt"), "low-level read created a file");
    require(!tuner.get_timeout(value, 20ms) && !fs::exists("missing.txt"), "timeout read created a file");
    write_file("plain.txt", "42\n");
    tuner.set_file("plain.txt");
    require(tuner.try_get(value) && value == 42, "plain value control failed");
    require(tuner.get_timeout(value, 100ms), "timeout API incorrectly reused stale cache");

    write_file("sentinel.txt", "do not change\n");
    fs::create_symlink(fs::absolute("sentinel.txt"), "link.txt");
    const auto link = livetuner::internal::read_regular_file("link.txt");
    require(LIVETUNER_STRICT_FILE_READS ? !link : (link && *link.content == "do not change\n"), "file read policy mismatch");
    fs::create_symlink(fs::absolute("not-created.txt"), "dangling.txt");
    livetuner::LiveTuner dangling("dangling.txt");
    require(!dangling.try_get(value) && !fs::exists("not-created.txt"), "dangling link target created");
    livetuner::ErrorInfo error;
    require(!livetuner::internal::write_regular_file("link.txt", "overwrite", error), "write followed link");
    fs::create_hard_link("sentinel.txt", "hardlink.txt");
    require(!livetuner::internal::write_regular_file("hardlink.txt", "overwrite", error), "write followed hard link");
    std::ifstream sentinel("sentinel.txt");
    std::string text;
    std::getline(sentinel, text);
    require(text == "do not change", "sentinel clobbered");
#ifndef _WIN32
    require(mkfifo("pipe.txt", 0600) == 0, "cannot create FIFO fixture");
    livetuner::LiveTuner fifo("pipe.txt");
    require(!fifo.try_get(value), "non-regular file accepted");
#endif
#ifdef LIVETUNER_USE_NLOHMANN_JSON
    write_file("optional.json", R"({"x":12,"items":[1,2],"large":1e100,"negative":-1,"narrow":256,"fraction":12.75,"uintmax":18446744073709551615,"intmin":-9223372036854775808})");
    livetuner::NlohmannParams optional("optional.json");
    require(optional.get<int>("x") == 12, "optional normal read failed");
    bool called = false;
    optional.set_error_callback([&](const livetuner::ErrorInfo&) {
        called = true;
        require(static_cast<bool>(optional.last_error()), "optional error state unavailable");
        require(optional.get<int>("x") > 0, "optional callback cannot reenter");
    });
    require(optional.get<int>("items[bad]", 7) == 7 && called, "optional error callback missing");
    require(optional.get<float>("large", 7.0f) == 7.0f, "optional floating narrowing overflow accepted");
    require(optional.get<int>("large", 7) == 7, "optional float-to-integer overflow accepted");
    require(optional.get<unsigned>("negative", 7) == 7, "optional signed-to-unsigned wrap accepted");
    require(optional.get<uint8_t>("narrow", 7) == 7, "optional integer narrowing wrap accepted");
    require(optional.get<int64_t>("uintmax", 7) == 7, "optional unsigned-to-signed wrap accepted");
    require(optional.get<uint64_t>("uintmax") == UINT64_MAX, "optional uint64 boundary rejected");
    require(optional.get<int64_t>("intmin") == INT64_MIN, "optional int64 boundary rejected");
    require(optional.get<int>("fraction") == 12, "optional representable fractional conversion changed");
    require(optional.set("bounds", nlohmann::json{1.0, std::ldexp(1.0, 63)}), "optional numeric array setup failed");
    require(optional.get<std::vector<int64_t>>("bounds", {7}) == std::vector<int64_t>{7}, "optional array conversion bypassed numeric limits");
    require(optional.get<std::array<int64_t, 2>>("bounds", {7, 8}) == std::array<int64_t, 2>{7, 8}, "optional fixed array conversion bypassed numeric limits");
    require(optional.set("bounds", nlohmann::json{{"value", -1}}), "optional numeric map setup failed");
    require(optional.get<std::map<std::string, unsigned>>("bounds").empty(), "optional map conversion bypassed numeric limits");
    require(optional.get<std::unordered_map<std::string, unsigned>>("bounds").empty(), "optional unordered map conversion bypassed numeric limits");
    require(!optional.set("", std::numeric_limits<double>::infinity()), "optional nonfinite set accepted");
    require(optional.set("x", 13) && optional.save(), "optional save control failed");
    write_file("optional.json", R"({"x":1e309})");
    for (int i = 0; i < 100; ++i) { optional.update(); if (optional.last_error()) break; std::this_thread::sleep_for(10ms); }
    require(optional.get<int>("x") == 13, "optional invalid reload replaced last good state");
    livetuner::NlohmannParams optional_link("link.txt");
    require(!optional_link.save(), "optional save followed symlink");
#endif
}

static void lifetime_regressions() {
    write_file("wait.txt", "not-a-number\n");
    std::future<int> pending;
    std::atomic<bool> callback_called{false};
    const auto start = std::chrono::steady_clock::now();
    {
        livetuner::LiveTuner owner("wait.txt");
        pending = owner.get_async<int>();
        owner.get_async<int>([&](int) { callback_called = true; });
    }
    require(std::chrono::steady_clock::now() - start < 3s, "destruction failed to cancel promptly");
    require(pending.wait_for(1s) == std::future_status::ready, "future abandoned after destruction");
    bool cancelled = false;
    try { (void)pending.get(); } catch (const livetuner::AsyncCancelled&) { cancelled = true; }
    require(cancelled && !callback_called, "cancelled async delivered a value");
    write_file("success.txt", "17\n");
    livetuner::LiveTuner source("success.txt");
    auto successful = source.get_async<int>();
    source.set_file("wait.txt");
    require(successful.wait_for(2s) == std::future_status::ready && successful.get() == 17, "async did not snapshot path");
    pending = source.get_async<int>();
    {
        livetuner::LiveTuner moved(std::move(source));
        livetuner::LiveTuner assigned;
        assigned = std::move(moved);
    }
    require(pending.wait_for(1s) == std::future_status::ready, "move lost async ownership");
    try { (void)pending.get(); require(false, "moved cancellation delivered value"); }
    catch (const livetuner::AsyncCancelled&) {}

    std::promise<void> destroyed;
    auto completion = destroyed.get_future();
    auto self = std::make_unique<livetuner::LiveTuner>("wait.txt");
    self->get_async<int>([&](int v) { require(v == 23, "self-destroy callback value"); self.reset(); destroyed.set_value(); });
    write_file("wait.txt", "23\n");
    require(completion.wait_for(3s) == std::future_status::ready, "callback owner destruction deadlocked");
    completion.get();
    require(!self, "owner survived callback destruction");
    std::atomic<bool> exception_logged{false};
    livetuner::set_log_callback([&](livetuner::LogLevel, const std::string&) { exception_logged = true; });
    {
        livetuner::LiveTuner owner("success.txt");
        std::promise<void> called;
        auto ready = called.get_future();
        owner.get_async<int>([&](int) { called.set_value(); throw std::runtime_error("callback test"); });
        require(ready.wait_for(2s) == std::future_status::ready, "callback did not execute");
    }
    require(exception_logged, "callback exception silently discarded");
    livetuner::set_log_callback(nullptr);

    // Capture destruction is user code too: it may schedule another operation.
    livetuner::LiveTuner owner("success.txt");
    std::promise<void> capture_destroyed;
    auto capture_ready = capture_destroyed.get_future();
    auto capture = std::shared_ptr<int>(new int(1), [&](int* value) {
        delete value;
        auto nested = owner.get_async<int>();
        require(nested.get() == 17, "capture destructor could not reenter async API");
        capture_destroyed.set_value();
    });
    owner.get_async<int>([capture = std::move(capture)](int) { (void)capture; });
    require(capture_ready.wait_for(3s) == std::future_status::ready, "async capture destruction deadlocked");
    capture_ready.get();
}

static void watcher_regressions() {
    write_file("watch.txt", "x=1\n");
    std::atomic<int> notifications{0};
    for (int i = 0; i < 40; ++i) {
        livetuner::internal::FileWatcher watcher;
        require(watcher.start("watch.txt", [&] { ++notifications; }), "watcher failed to start");
        write_file("watch.txt", "x=" + std::to_string(i) + "\n");
        if (i == 0) require(watcher.wait_for_change(3s), "native watcher did not deliver event");
        watcher.stop();
        const auto stopped = notifications.load();
        write_file("watch.txt", "x=99\n");
        require(notifications == stopped, "callback occurred after stop");
    }
    int bound = 0;
    auto source = std::make_unique<livetuner::Params>("watch.txt");
    source->bind("x", bound);
    source->start_watching();
    livetuner::Params moved(std::move(*source));
    source.reset();
    livetuner::Params assigned("unused.txt");
    assigned = std::move(moved);
    const auto caller = std::this_thread::get_id();
    bool callback_on_caller = false;
    assigned.on_change([&] { callback_on_caller = std::this_thread::get_id() == caller; });
    write_file("watch.txt", "x=123\n");
    for (int i = 0; i < 150 && bound != 123; ++i) { assigned.poll(); std::this_thread::sleep_for(10ms); }
    require(bound == 123 && callback_on_caller, "watched move lost notifications or main-thread callback");
    assigned.stop_watching();
}

static void concurrency_regressions() {
    livetuner::Params missing("unreadable.json");
    int reentries = 0;
    livetuner::set_log_callback([&](livetuner::LogLevel, const std::string&) { (void)missing.last_error(); ++reentries; });
    require(!missing.update() && reentries > 0, "reader diagnostics did not reenter safely");
    write_file("bad.txt", "bad\n");
    livetuner::LiveTuner bad("bad.txt");
    livetuner::set_log_callback([&](livetuner::LogLevel, const std::string&) { (void)bad.last_error(); });
    int val = 0;
    require(!bad.try_get(val), "bad plain value accepted");
    std::string record;
    livetuner::set_log_callback([&](livetuner::LogLevel, const std::string& message) { record = message; });
    livetuner::internal::log(livetuner::LogLevel::Warning, std::string("line\r\n\x1b[31m\0", 13) + std::string(4096, 'a'));
    require(record.find('\n') == std::string::npos && record.find('\r') == std::string::npos &&
        record.find('\x1b') == std::string::npos && record.find('\0') == std::string::npos && record.size() < 2100,
        "log record was not escaped and bounded");
    livetuner::set_log_callback(nullptr);
    write_file("race.txt", "x=12\n");
    write_file("race-value.txt", "12\n");
    livetuner::Params params("race.txt");
    livetuner::LiveTuner tuner("race-value.txt");
    std::thread logger([] {
        for (int i = 0; i < 400; ++i) {
            livetuner::set_log_callback([](livetuner::LogLevel, const std::string&) {});
            livetuner::set_log_callback(nullptr);
        }
    });
    std::thread reader([&] {
        for (int i = 0; i < 400; ++i) {
            params.update();
            (void)params.get<int>("x");
            int value = 0;
            tuner.try_get(value);
            livetuner::internal::log(livetuner::LogLevel::Info, "concurrent log");
        }
    });
    std::thread invalidator([&] {
        for (int i = 0; i < 400; ++i) { params.invalidate_cache(); tuner.invalidate_cache(); tuner.set_file("race-value.txt"); }
    });
    logger.join(); reader.join(); invalidator.join();
    livetuner::set_log_callback(nullptr);
}

int main(int argc, char** argv) {
    try {
        TestDirectory directory;
        livetuner::set_log_callback(nullptr);
        require(argc == 2, "select a regression group");
        const std::string group = argv[1];
        if (group == "parser") parser_regressions();
        else if (group == "ingress") ingress_regressions();
        else if (group == "lifetime") lifetime_regressions();
        else if (group == "watcher") watcher_regressions();
        else if (group == "concurrency") concurrency_regressions();
        else if (group == "missing") missing_file_regressions();
        else if (group == "timestamps") timestamp_regressions();
        else if (group == "links") link_read_regressions();
        else require(false, "unknown regression group");
        std::cout << "security regressions passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "FAIL: " << e.what() << '\n';
        return 1;
    }
}

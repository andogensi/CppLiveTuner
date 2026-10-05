#define LIVETUNER_ENABLE_RUNTIME 0
#define LIVETUNER_IMPLEMENTATION
#ifdef LIVETUNER_USE_NLOHMANN_JSON
#include <nlohmann/json.hpp>
#endif
#include "LiveTuner.h"

int main() {
    livetuner::set_log_callback(nullptr);
    livetuner::LiveTuner tuner("must-not-create.txt");
    int value = 12;
    tuner.get(value);
    if (tuner.try_get(value) || value != 12 || tuner.last_error().type != livetuner::ErrorType::Disabled) return 1;
    if (tuner.get_timeout(value, std::chrono::seconds(1))) return 1;
    auto future = tuner.get_async<int>();
    bool rejected = false;
    try { (void)future.get(); } catch (const std::runtime_error&) { rejected = true; }
    if (!rejected) return 1;
    livetuner::Params params("must-not-create.json");
    params.bind("value", value, 12);
    params.start_watching();
    if (params.poll() || params.update() || value != 12) return 1;
#ifdef LIVETUNER_USE_NLOHMANN_JSON
    livetuner::NlohmannParams optional("must-not-create.json");
    if (optional.get<int>("value", 12) != 12 || optional.update() || optional.set("value", 7) || optional.save()) return 1;
    if (optional.last_error().type != livetuner::ErrorType::Disabled) return 1;
#endif
    if (std::filesystem::exists("must-not-create.txt") || std::filesystem::exists("must-not-create.json")) return 1;
    return 0;
}

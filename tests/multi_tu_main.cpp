#include "LiveTuner.h"
#include "LiveTunerTest.h"

int main() {
    livetuner::set_log_callback(nullptr);
    livetuner::Params params("missing-multi-tu.json");
    if (params.update()) return 1;
    livetuner::LiveTuner tuner("missing-multi-tu.txt");
    int value = 7;
    if (tuner.try_get(value) || value != 7) return 1;
    return 0;
}

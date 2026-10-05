#define LIVETUNER_IMPLEMENTATION
#define LIVETUNER_ENABLE_DEFAULT_LOGGING 0
#include "LiveTuner.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size > livetuner::internal::max_config_bytes + 1) return 0;
    std::string content(reinterpret_cast<const char*>(data), size);
    livetuner::internal::PicojsonParser::ValueMap values;
    livetuner::internal::PicojsonParser::parse(content, values);
    livetuner::internal::SimpleKeyValueParser::parse(content, values);
    livetuner::internal::SimpleKeyValueParser::parse(content, values, true);
    return 0;
}

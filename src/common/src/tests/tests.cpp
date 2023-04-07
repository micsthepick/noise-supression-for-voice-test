#define CATCH_CONFIG_MAIN
#include <catch.hpp>

#include "common/RnNoiseCommonPlugin.h"

TEST_CASE("Init -> Deinit cycle", "[common_plugin]") {
    auto channels = GENERATE(1, 2, 4);

    CAPTURE(channels);

    RnNoiseCommonPlugin plugin(channels);

    SECTION("init -> deinit") {
        plugin.init();
        plugin.deinit();
    }

    SECTION("repeated init") {
        plugin.init();
        plugin.init();
        plugin.init();
    }
}

TEST_CASE("All options", "[common_plugin]") {
    auto channels = GENERATE(1, 2, 4);
    auto blockPerCall = GENERATE(1, 10);
    /* 200 - Audacity (actually it has variable block size)
     * 480 - PulseAudio
     * 512 - Pipewire
     */
    auto sampleFrames = GENERATE(200, 480, 512);

    CAPTURE(channels, blockPerCall, sampleFrames);

    RnNoiseCommonPlugin plugin(channels);
    plugin.init();

    auto inputs = std::vector<const float *>();
    auto outputs = std::vector<float *>();

    uint32_t iterations = 10 / blockPerCall;
    for (int i = 0; i < channels; i++) {
        inputs.push_back(new float[sampleFrames * blockPerCall * iterations]);
        outputs.push_back(new float[sampleFrames * blockPerCall * iterations]);
    }

    for (uint32_t i = 0; i < iterations; i++) {
        for (int ch = 0; ch < channels; ch++) {
            std::fill(&outputs[ch][0], &outputs[ch][sampleFrames * blockPerCall], -1.f);
        }

        plugin.process(inputs.data(), outputs.data(), sampleFrames * blockPerCall);

        for (int ch = 0; ch < channels; ch++) {
            for (int j = 0; j < sampleFrames; j++) {
                if (outputs[ch][j] == -1.f) {
                    CAPTURE(ch, j);
                    FAIL("No output written");
                }
            }
        }
    }

    plugin.deinit();

    for (int i = 0; i < channels; i++) {
        delete[] inputs[i];
        delete[] outputs[i];
    }
}
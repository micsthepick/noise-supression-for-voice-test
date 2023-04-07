#pragma once

#include <memory>
#include <vector>

struct DenoiseState;

class RnNoiseCommonPlugin {
public:

    explicit RnNoiseCommonPlugin(uint32_t channels) :
            m_channelCount(channels) {}

    void init();

    void deinit();

    /**
     *
     * @param in
     * @param out
     * @param sampleFrames The amount of frames to process.
     */
    void process(const float *const *in, float **out, size_t sampleFrames);

private:

    void createDenoiseState();

private:
    static const size_t k_denoiseBlockSize = 480;
    static const uint32_t k_denoiseSampleRate = 48000;

    uint32_t m_channelCount;

    uint64_t m_newOutputIdx = 0;
    uint64_t m_lastOutputIdxOverVADThreshold = 0;

    uint64_t m_currentOutputIdxToOutput = 0;

    uint32_t m_prevRetroactiveVADGraceBlocks = 0;

    enum class ChunkUnmuteState {
        MUTED,
        UNMUTED_BY_DEFAULT,
        UNMUTED_VAD,
        UNMUTED_RETRO_VAD,
    };

    struct OutputChunk {
        uint64_t idx;
        float vadProbability;
        float maxVadProbability;
        ChunkUnmuteState muteState;
        float frames[480];
        size_t curOffset;
    };

    struct ChannelData {
        uint32_t idx;

        std::shared_ptr<DenoiseState> denoiseState;

        std::vector<float> rnnoiseInput;
        std::vector<std::unique_ptr<OutputChunk>> rnnoiseOutput;

        std::vector<std::unique_ptr<OutputChunk>> outputBlocksCache;
    };
    std::vector<ChannelData> m_channels;
};




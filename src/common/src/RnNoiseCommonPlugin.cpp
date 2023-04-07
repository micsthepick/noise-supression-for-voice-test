#include "common/RnNoiseCommonPlugin.h"

#include <cstring>
#include <limits>
#include <algorithm>
#include <cassert>

#include <rnnoise/rnnoise.h>

void RnNoiseCommonPlugin::init() {
    deinit();
    createDenoiseState();
}

void RnNoiseCommonPlugin::deinit() {
    m_channels.clear();
}

void RnNoiseCommonPlugin::process(const float *const *in, float **out, size_t sampleFrames) {
    if (sampleFrames == 0) {
        return;
    }

    /* For offline processing hosts could pass a lot of frames at once, there is also no
     * indicator whether additional frames are expected. By default, we accumulate enough
     * output frame to write sampleFrames number of frames into output, however with large
     * input we have to output what we have and zero out the rest. Otherwise, we'd have too
     * much output buffered and never written.
     * Fixes compatibility with Audacity. 500 ms cut-off is arbitrary.
     * TODO: Is there a better way to handle offline processing with big inputs?
     */
    bool waitForEnoughFrames = sampleFrames < (k_denoiseBlockSize * 50);

    /* Copy input data (since we are not allowed to change it inplace) */
    for (auto &channel: m_channels) {
        size_t newSamplesStart = channel.rnnoiseInput.size();
        channel.rnnoiseInput.insert(channel.rnnoiseInput.end(), in[channel.idx], in[channel.idx] + sampleFrames);
        float *inMultiplied = &channel.rnnoiseInput[newSamplesStart];
        for (size_t i = 0; i < sampleFrames; i++) {
            inMultiplied[i] = inMultiplied[i] * std::numeric_limits<short>::max();
        }
    }

    size_t blocksFromRnnoise = m_channels[0].rnnoiseInput.size() / k_denoiseBlockSize;

    /* Do all the denoising. Separating output into chunks containing additional metadata
     * allows to divide code in a more simple and comprehensible chunks, also allows to
     * reuse memory allocations.
     */
    for (auto &channel: m_channels) {
        for (size_t blockIdx = 0; blockIdx < blocksFromRnnoise; blockIdx++) {
            std::unique_ptr<OutputChunk> outBlock;
            if (channel.outputBlocksCache.empty()) {
                outBlock = std::make_unique<OutputChunk>();
            } else {
                outBlock = std::move(channel.outputBlocksCache.back());
                channel.outputBlocksCache.pop_back();
            }

            outBlock->curOffset = 0;
            outBlock->idx = m_newOutputIdx + blockIdx;
            outBlock->muteState = ChunkUnmuteState::UNMUTED_BY_DEFAULT;

            float *currentIn = &channel.rnnoiseInput[blockIdx * k_denoiseBlockSize];
            outBlock->vadProbability = rnnoise_process_frame(channel.denoiseState.get(),
                                                             outBlock->frames,
                                                             currentIn);

            channel.rnnoiseOutput.push_back(std::move(outBlock));
        }

        if (blocksFromRnnoise > 0) {
            /* Erasing is cheap since it just copies the elements that are left to the beginning of the vector. */
            channel.rnnoiseInput.erase(channel.rnnoiseInput.begin(),
                                       channel.rnnoiseInput.begin() + blocksFromRnnoise * k_denoiseBlockSize);
        }
    }

    m_newOutputIdx += blocksFromRnnoise;

    for (auto &channel: m_channels) {
        for (size_t blockIdx = channel.rnnoiseOutput.size() - blocksFromRnnoise;
             blockIdx < channel.rnnoiseOutput.size(); blockIdx++) {
            auto &outBlock = channel.rnnoiseOutput[blockIdx];
            for (float &frame: outBlock->frames) {
                frame /= std::numeric_limits<short>::max();
            }
        }
    }

    bool hasEnoughFrames = !m_channels[0].rnnoiseOutput.empty();
    if (hasEnoughFrames)
    {
        int32_t blockIdxRelative = static_cast<int32_t>(m_newOutputIdx - m_currentOutputIdxToOutput) - 1;
        blockIdxRelative = std::max(blockIdxRelative, 0);

        size_t firstBlockFrames =
                k_denoiseBlockSize - m_channels[0].rnnoiseOutput.rbegin()[blockIdxRelative]->curOffset;
        size_t totalFrames = blockIdxRelative * k_denoiseBlockSize + firstBlockFrames;
        hasEnoughFrames = totalFrames >= sampleFrames;
    }

    /* Wait until there are enough frames to fill all the output. Yes, it creates latency but
     * That's why it is STRONGLY recommended for sampleFrames to be divisible by k_denoiseBlockSize.
     */
    if (waitForEnoughFrames && !hasEnoughFrames) {
        for (uint32_t channelIdx = 0; channelIdx < m_channelCount; channelIdx++) {
            std::fill(out[channelIdx], out[channelIdx] + sampleFrames, 0.f);
        }

        return;
    }

    uint64_t newOutputIdxToOutput = 0;
    for (auto &channel: m_channels) {
        size_t toOutputFrames = sampleFrames;
        size_t curOutFrameIdx = 0;
        int32_t blockIdxRelative = static_cast<int32_t>(m_newOutputIdx - m_currentOutputIdxToOutput) - 1;
        while (curOutFrameIdx < sampleFrames && blockIdxRelative >= 0) {
            auto &outBlock = channel.rnnoiseOutput.rbegin()[blockIdxRelative];
            size_t copyFromThisBlock = std::min(k_denoiseBlockSize - outBlock->curOffset, toOutputFrames);
            if (outBlock->muteState == ChunkUnmuteState::MUTED) {
                // TODO: Maybe we should output some noise instead? Make it an option?
                std::fill(out[channel.idx] + curOutFrameIdx, out[channel.idx] + curOutFrameIdx + copyFromThisBlock, 0.f);
            } else {
                std::copy(outBlock->frames + outBlock->curOffset,
                          outBlock->frames + outBlock->curOffset + copyFromThisBlock,
                          out[channel.idx] + curOutFrameIdx);
            }

            outBlock->curOffset += copyFromThisBlock;
            if (outBlock->curOffset == k_denoiseBlockSize) {
                blockIdxRelative--;
            }

            curOutFrameIdx += copyFromThisBlock;
            toOutputFrames -= copyFromThisBlock;
        }

        blockIdxRelative = std::max(blockIdxRelative, 0);

        for (; curOutFrameIdx < sampleFrames; curOutFrameIdx++) {
            out[channel.idx][curOutFrameIdx] = 0.f;
        }

        newOutputIdxToOutput = m_newOutputIdx - 1 - blockIdxRelative;
    }

    m_currentOutputIdxToOutput = newOutputIdxToOutput;
    uint32_t blocksToLeave = static_cast<uint32_t>(m_newOutputIdx - m_currentOutputIdxToOutput);

    /* Move output blocks we don't need to the cache to save on allocations. */
    if (blocksToLeave < m_channels[0].rnnoiseOutput.size()) {
        for (auto &channel: m_channels) {
            uint32_t blocksToRemove = static_cast<uint32_t>(channel.rnnoiseOutput.size()) - blocksToLeave;
            channel.outputBlocksCache.insert(channel.outputBlocksCache.end(),
                                             std::make_move_iterator(channel.rnnoiseOutput.begin()),
                                             std::make_move_iterator(
                                                       channel.rnnoiseOutput.begin() + blocksToRemove));
            channel.rnnoiseOutput.erase(channel.rnnoiseOutput.begin(),
                                        channel.rnnoiseOutput.begin() + blocksToRemove);
        }
    }
}

void RnNoiseCommonPlugin::createDenoiseState() {
    m_newOutputIdx = 0;
    m_currentOutputIdxToOutput = 0;

    for (uint32_t i = 0; i < m_channelCount; i++) {
        auto denoiseState = std::shared_ptr<DenoiseState>(rnnoise_create(), [](DenoiseState *st) {
            rnnoise_destroy(st);
        });

        m_channels.push_back(ChannelData{i, denoiseState, {}, {}, {}});
    }
}


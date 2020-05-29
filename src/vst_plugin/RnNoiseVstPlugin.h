#pragma once

#include <memory>
#include <vector>
#include "vst2.x/audioeffectx.h"

class RnNoiseCommonPlugin;

class RnNoiseVstPlugin : public AudioEffectX {
public:

    RnNoiseVstPlugin(audioMasterCallback audioMaster, VstInt32 numPrograms, VstInt32 numParams);

    ~RnNoiseVstPlugin() override;

    VstInt32 startProcess() override;

    VstInt32 stopProcess() override;

    void processReplacing(float **inputs, float **outputs, VstInt32 sampleFrames) override;

    bool getEffectName(char *name) override;

    bool getProductString(char *name) override;

private:
    static const char* s_effectName;

    static const char* s_productString;

    std::unique_ptr<RnNoiseCommonPlugin> m_rnNoisePlugin;
};
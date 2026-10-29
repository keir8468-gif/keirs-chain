#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "dsp/ChainDSP.h"
#include "dsp/KeyDetector.h"
#include <atomic>

namespace ParamID {
inline constexpr auto tcOn = "tcOn", grid = "grid", swing = "swing", shiftDir = "shiftDir", shiftAmt = "shiftAmt";
inline constexpr auto mkOn = "mkOn", trim = "trim";
inline constexpr auto eqOn = "eqOn", bright = "bright", brightFreq = "brightFreq", eqPos = "eqPos";
inline constexpr auto xlOn = "xlOn", out = "out", bypass = "bypass";
}

class KeirsChainProcessor : public juce::AudioProcessor, private juce::Thread {
public:
    KeirsChainProcessor();
    ~KeirsChainProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.05; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;
    juce::AudioProcessorParameter* getBypassParameter() const override { return apvts.getParameter(ParamID::bypass); }

    juce::AudioProcessorValueTreeState apvts;

    // ---- read by the editor ----
    struct MeterSnapshot { float xlL = 0, xlR = 0, mkClip = 0, out = 0; };
    MeterSnapshot takeMeters();
    struct KeySnapshot { bool keyValid = false, chordValid = false, silent = true; juce::String key, camelot, alt, altCamelot, chord; double margin = 0, heard = 0, tuning = 0; };
    KeySnapshot getKey();
    void resetKey() { keyResetRequested.store(true); }
    double getHostBpm() const { return hostBpm.load(); }
    bool hostBpmKnown() const { return bpmKnown.load(); }
    int getEvenTicks() const { return evenTicks.load(); }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void run() override;                       // key-detection thread
    void updateParams(double bpm);
    static void maxInto(std::atomic<float>& a, float v) { float cur = a.load(); while (v > cur && !a.compare_exchange_weak(cur, v)) {} }

    kc::Chain chain;
    kc::Chain::Params lastParams; bool paramsInit = false;
    double sr = 44100; int64_t freeRunPos = 0; std::vector<float> monoScratch;
    std::atomic<double> hostBpm{ 120.0 }; std::atomic<bool> bpmKnown{ false }; std::atomic<int> evenTicks{ 0 };
    std::atomic<float> mXlL{ 0 }, mXlR{ 0 }, mMk{ 0 }, mOut{ 0 };

    // key detection: the audio thread writes mono input into a ring, the thread analyses the latest window
    static constexpr int RING = 1 << 17, WIN = 16384;
    std::vector<float> ring; std::atomic<int64_t> ringWrite{ 0 }; std::atomic<double> ringRate{ 44100 };
    std::atomic<bool> keyResetRequested{ false };
    juce::SpinLock keyLock; KeySnapshot keySnap;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(KeirsChainProcessor)
};

#include "PluginProcessor.h"
#include "PluginEditor.h"

using APVTS = juce::AudioProcessorValueTreeState;

KeirsChainProcessor::KeirsChainProcessor()
    : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true)
                                      .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      juce::Thread("Keir's Chain key detection"),
      apvts(*this, nullptr, "KeirsChain", createLayout())
{
    ring.assign(RING, 0.f);
    startThread(juce::Thread::Priority::low);
}

KeirsChainProcessor::~KeirsChainProcessor() { stopThread(2000); }

APVTS::ParameterLayout KeirsChainProcessor::createLayout()
{
    using namespace juce;
    APVTS::ParameterLayout l;
    auto dB = [](float v, int) { return (v > 0 ? "+" : "") + String(v, 1) + " dB"; };
    l.add(std::make_unique<AudioParameterBool>(ParameterID{ ParamID::tcOn, 1 }, "MPC Timing On", true));
    l.add(std::make_unique<AudioParameterChoice>(ParameterID{ ParamID::grid, 1 }, "Note Value", StringArray{ "1/8", "1/8T", "1/16", "1/16T", "1/32", "1/32T" }, 2));
    l.add(std::make_unique<AudioParameterInt>(ParameterID{ ParamID::swing, 1 }, "Swing", 50, 75, 50, AudioParameterIntAttributes().withLabel("%")));
    l.add(std::make_unique<AudioParameterChoice>(ParameterID{ ParamID::shiftDir, 1 }, "Shift Timing", StringArray{ "Earlier", "Later" }, 1));
    l.add(std::make_unique<AudioParameterInt>(ParameterID{ ParamID::shiftAmt, 1 }, "Shift Amount", 0, 24, 0, AudioParameterIntAttributes().withLabel("tk")));
    l.add(std::make_unique<AudioParameterBool>(ParameterID{ ParamID::mkOn, 1 }, "Mackie On", true));
    l.add(std::make_unique<AudioParameterFloat>(ParameterID{ ParamID::trim, 1 }, "Mackie Trim", NormalisableRange<float>(-13.f, 50.f, 0.5f), 0.f,
                                                AudioParameterFloatAttributes().withStringFromValueFunction([](float v, int) { return std::abs(v) < 0.25f ? String("U") : (v > 0 ? "+" : "") + String(v, 1) + " dB"; })));
    l.add(std::make_unique<AudioParameterBool>(ParameterID{ ParamID::eqOn, 1 }, "W492 On", true));
    l.add(std::make_unique<AudioParameterFloat>(ParameterID{ ParamID::bright, 1 }, "Brightness", NormalisableRange<float>(-15.f, 15.f, 0.1f), 0.f,
                                                AudioParameterFloatAttributes().withStringFromValueFunction(dB)));
    NormalisableRange<float> fr(3000.f, 10000.f, 1.f); fr.setSkewForCentre(5477.f);
    l.add(std::make_unique<AudioParameterFloat>(ParameterID{ ParamID::brightFreq, 1 }, "Brightness Freq", fr, 10000.f,
                                                AudioParameterFloatAttributes().withStringFromValueFunction([](float v, int) { return String(v / 1000.f, 1) + " kHz"; })));
    l.add(std::make_unique<AudioParameterChoice>(ParameterID{ ParamID::eqPos, 1 }, "W492 Position", StringArray{ "Before A/D", "After D/A" }, 0));
    l.add(std::make_unique<AudioParameterBool>(ParameterID{ ParamID::xlOn, 1 }, "2000XL On", true));
    l.add(std::make_unique<AudioParameterFloat>(ParameterID{ ParamID::out, 1 }, "Output", NormalisableRange<float>(-24.f, 12.f, 0.5f), 0.f,
                                                AudioParameterFloatAttributes().withStringFromValueFunction(dB)));
    l.add(std::make_unique<AudioParameterBool>(ParameterID{ ParamID::bypass, 1 }, "Bypass", false));
    return l;
}

bool KeirsChainProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet(), out = layouts.getMainOutputChannelSet();
    if (in != out) return false;
    return out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
}

void KeirsChainProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    chain.prepare(sr, std::max(samplesPerBlock, 32));
    monoScratch.assign((size_t)std::max(samplesPerBlock, 32) * 2, 0.f);
    paramsInit = false;
    updateParams(hostBpm.load());
    ringRate.store(sr);
    setLatencySamples(chain.latency());
}

void KeirsChainProcessor::updateParams(double bpm)
{
    auto b = [this](const char* id) { return apvts.getRawParameterValue(id)->load() > 0.5f; };
    auto f = [this](const char* id) { return (double)apvts.getRawParameterValue(id)->load(); };
    kc::Chain::Params p;
    p.timing.on = b(ParamID::tcOn); p.timing.bpm = bpm; p.timing.grid = (kc::Grid)(int)f(ParamID::grid);
    p.timing.swing = f(ParamID::swing); p.timing.earlier = (int)f(ParamID::shiftDir) == 0; p.timing.shiftAmt = f(ParamID::shiftAmt);
    p.mkOn = b(ParamID::mkOn); p.drive = f(ParamID::trim);
    p.eqOn = b(ParamID::eqOn); p.bright = f(ParamID::bright); p.brightFreq = f(ParamID::brightFreq); p.eqPost = (int)f(ParamID::eqPos) == 1;
    p.xlOn = b(ParamID::xlOn); p.outDb = f(ParamID::out); p.bypass = b(ParamID::bypass);
    const auto& q = lastParams;
    const bool changed = !paramsInit || p.timing.on != q.timing.on || p.timing.bpm != q.timing.bpm || p.timing.grid != q.timing.grid
        || p.timing.swing != q.timing.swing || p.timing.earlier != q.timing.earlier || p.timing.shiftAmt != q.timing.shiftAmt
        || p.mkOn != q.mkOn || p.drive != q.drive || p.eqOn != q.eqOn || p.bright != q.bright || p.brightFreq != q.brightFreq
        || p.eqPost != q.eqPost || p.xlOn != q.xlOn || p.outDb != q.outDb || p.bypass != q.bypass;
    if (changed) { chain.set(p, !paramsInit); lastParams = p; paramsInit = true; evenTicks.store(chain.evenTicks()); }
}

void KeirsChainProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples(), nch = buffer.getNumChannels();
    if (n == 0) return;

    // follow Live's tempo and beat position so the MPC grid sits on the bar lines
    double bpm = hostBpm.load(), pos0 = (double)freeRunPos;
    if (auto* ph = getPlayHead()) if (auto pos = ph->getPosition()) {
        if (auto t = pos->getBpm()) { bpm = *t; bpmKnown.store(true); }
        if (pos->getIsPlaying()) if (auto ppq = pos->getPpqPosition()) pos0 = *ppq * 60.0 / bpm * sr;
    }
    hostBpm.store(bpm);
    updateParams(bpm);
    const int lat = chain.latency();
    if (lat != getLatencySamples()) setLatencySamples(lat);

    // key detection gets the incoming audio, before any processing
    {
        int64_t w = ringWrite.load(std::memory_order_relaxed);
        const float* l = buffer.getReadPointer(0); const float* r = buffer.getReadPointer(nch > 1 ? 1 : 0);
        for (int i = 0; i < n; i++) ring[(size_t)((w + i) & (RING - 1))] = 0.5f * (l[i] + r[i]);
        ringWrite.store(w + n, std::memory_order_release);
    }

    float* L = buffer.getWritePointer(0); float* R = nch > 1 ? buffer.getWritePointer(1) : L;
    if (nch > 1) chain.process(L, R, L, R, n, pos0);
    else { if ((int)monoScratch.size() < n) monoScratch.resize((size_t)n); std::copy(L, L + n, monoScratch.begin()); chain.process(L, monoScratch.data(), L, monoScratch.data(), n, pos0); }
    freeRunPos += n;

    const auto m = chain.takeMeters();
    maxInto(mXlL, m.xlL); maxInto(mXlR, m.xlR); maxInto(mMk, m.mkClip); maxInto(mOut, m.out);
}

KeirsChainProcessor::MeterSnapshot KeirsChainProcessor::takeMeters()
{
    return { mXlL.exchange(0.f), mXlR.exchange(0.f), mMk.exchange(0.f), mOut.exchange(0.f) };
}

KeirsChainProcessor::KeySnapshot KeirsChainProcessor::getKey()
{
    const juce::SpinLock::ScopedLockType sl(keyLock);
    return keySnap;
}

void KeirsChainProcessor::run()
{
    std::unique_ptr<kc::KeyDetector> kd; double kdRate = 0;
    std::vector<float> win(WIN);
    int64_t lastWrite = 0;
    while (!threadShouldExit()) {
        wait(100);
        const double rate = ringRate.load();
        if (!kd || rate != kdRate) { kd = std::make_unique<kc::KeyDetector>(rate, WIN); kdRate = rate; lastWrite = ringWrite.load(); }
        if (keyResetRequested.exchange(false)) kd->reset();
        const int64_t w = ringWrite.load(std::memory_order_acquire);
        const int64_t fresh = w - lastWrite;
        if (fresh <= 0 || w < WIN) {                 // transport stopped: hold the key, clear the chord
            const juce::SpinLock::ScopedLockType sl(keyLock); keySnap.chordValid = false; keySnap.silent = true;
            continue;
        }
        lastWrite = w;
        for (int i = 0; i < WIN; i++) win[(size_t)i] = ring[(size_t)((w - WIN + i) & (RING - 1))];
        kd->push(win.data(), std::min(0.5, (double)fresh / rate));
        KeySnapshot s;
        s.keyValid = kd->key.valid && kd->heard >= 4; s.chordValid = kd->chord.valid && !kd->silent; s.silent = kd->silent;
        s.key = juce::String::fromUTF8(kd->key.name.c_str()); s.camelot = kd->key.camelot;
        s.alt = juce::String::fromUTF8(kd->key.altName.c_str()); s.altCamelot = kd->key.altCamelot;
        s.chord = juce::String::fromUTF8(kd->chord.name.c_str()); s.margin = kd->key.margin; s.heard = kd->heard; s.tuning = kd->tuning;
        const juce::SpinLock::ScopedLockType sl(keyLock); keySnap = s;
    }
}

void KeirsChainProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml()) copyXmlToBinary(*xml, destData);
}

void KeirsChainProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary(data, sizeInBytes))
        if (xml->hasTagName(apvts.state.getType())) apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessorEditor* KeirsChainProcessor::createEditor() { return new KeirsChainEditor(*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new KeirsChainProcessor(); }

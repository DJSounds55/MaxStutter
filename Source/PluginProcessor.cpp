#include "PluginProcessor.h"
#include <cmath>

namespace
{
    const int kSteps[]      = { 2, 4, 8, 16, 32, 6, 12, 24 };   // Schritte pro Takt
    const double kLenBars[] = { 0.25, 0.5, 1.0, 2.0 };

    inline float readInterp (const float* d, int size, double p)
    {
        p = std::fmod (p, (double) size);
        if (p < 0.0) p += size;
        const int i = (int) p;
        const float f = (float) (p - i);
        const int i2 = (i + 1 >= size) ? 0 : i + 1;
        return d[i] * (1.0f - f) + d[i2] * f;
    }

    inline float gateEnv (int pos, float gl)
    {
        if (pos >= gl) return 0.0f;
        const float fd = juce::jmin (96.0f, gl * 0.5f);
        if (pos < fd) return pos / fd;
        if (pos > gl - fd) return (gl - pos) / fd;
        return 1.0f;
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout MaxStutterAudioProcessor::createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;
    p.push_back (std::make_unique<AudioParameterBool>   (ParameterID { "stutter", 1 }, "Stutter", false));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "mode", 1 }, "Modus",
        StringArray { "Stutter", "Roll (beschleunigend)", "Gate-Chop", "Tape-Stop", "Backspin", "Echo (letzter Slice)" }, 0));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "div", 1 }, "Teilung",
        StringArray { "1/2", "1/4", "1/8", "1/16", "1/32", "1/4 Triole", "1/8 Triole", "1/16 Triole" }, 3));
    p.push_back (std::make_unique<AudioParameterFloat>  (ParameterID { "gate", 1 }, "Gate %", NormalisableRange<float> (20.f, 100.f, 1.f), 100.f));
    p.push_back (std::make_unique<AudioParameterFloat>  (ParameterID { "decay", 1 }, "Decay %", NormalisableRange<float> (0.f, 90.f, 1.f), 0.f));
    p.push_back (std::make_unique<AudioParameterFloat>  (ParameterID { "pitch", 1 }, "Tonhöhe/Wiederholung (Halbtöne)", NormalisableRange<float> (-12.f, 12.f, 0.5f), 0.f));
    p.push_back (std::make_unique<AudioParameterBool>   (ParameterID { "pingpong", 1 }, "Ping-Pong rückwärts", false));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "len", 1 }, "Länge Tape-Stop/Backspin",
        StringArray { "1/4 Takt", "1/2 Takt", "1 Takt", "2 Takte" }, 2));
    p.push_back (std::make_unique<AudioParameterBool>   (ParameterID { "hostbpm", 1 }, "BPM vom Host", true));
    p.push_back (std::make_unique<AudioParameterFloat>  (ParameterID { "bpm", 1 }, "BPM manuell", NormalisableRange<float> (40.f, 240.f, 0.1f), 128.f));
    p.push_back (std::make_unique<AudioParameterFloat>  (ParameterID { "mix", 1 }, "Mix %", NormalisableRange<float> (0.f, 100.f, 1.f), 100.f));
    return { p.begin(), p.end() };
}

MaxStutterAudioProcessor::MaxStutterAudioProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
    pStutter = apvts.getRawParameterValue ("stutter");
    pMode    = apvts.getRawParameterValue ("mode");
    pDiv     = apvts.getRawParameterValue ("div");
    pGate    = apvts.getRawParameterValue ("gate");
    pDecay   = apvts.getRawParameterValue ("decay");
    pPitch   = apvts.getRawParameterValue ("pitch");
    pPing    = apvts.getRawParameterValue ("pingpong");
    pLen     = apvts.getRawParameterValue ("len");
    pHostBpm = apvts.getRawParameterValue ("hostbpm");
    pBpm     = apvts.getRawParameterValue ("bpm");
    pMix     = apvts.getRawParameterValue ("mix");
}

bool MaxStutterAudioProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    return l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo()
        && l.getMainInputChannelSet()  == juce::AudioChannelSet::stereo();
}

void MaxStutterAudioProcessor::prepareToPlay (double sampleRate, int)
{
    sr = sampleRate;
    ringSize = (int) (sr * 30.0);   // 30 s Verlauf (für Tape-Stop/Backspin)
    capSize  = (int) (sr * 8.0);    // max. 8 s Stutter-Slice
    ring.setSize (2, ringSize); ring.clear();
    cap.setSize  (2, capSize);  cap.clear();
    writePos = 0; prevOn = active = running = false; wet = 0.0f;
}

void MaxStutterAudioProcessor::processBlock (juce::AudioBuffer<float>& buf, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buf.getNumSamples();
    if (buf.getNumChannels() < 2 || ringSize == 0) return;

    const bool on = pStutter->load() > 0.5f;
    double bpm = pBpm->load();
    if (pHostBpm->load() > 0.5f)
        if (auto* ph = getPlayHead())
            if (auto pos = ph->getPosition())
                if (auto hb = pos->getBpm())
                    if (*hb > 20.0) bpm = *hb;

    const double beat   = 60.0 / bpm * sr;
    const int    steps  = kSteps[juce::jlimit (0, 7, (int) pDiv->load())];
    const int    mode   = (int) pMode->load();
    const float  gate   = pGate->load() / 100.0f;
    const float  decay  = pDecay->load() / 100.0f;
    const float  pitch  = pPitch->load();
    const bool   ping   = pPing->load() > 0.5f;
    const float  mix    = pMix->load() / 100.0f;
    const int    baseLen = juce::jlimit (64, capSize, (int) std::lround (beat * 4.0 / steps));
    const int    minS    = juce::jmax (64, (int) (beat * 4.0 / 32.0));
    const int    tapeLen = (int) (beat * 4.0 * kLenBars[juce::jlimit (0, 3, (int) pLen->load())]);

    if (on && ! prevOn)       // Stutter startet: genau hier wird der Slice "geschnitten"
    {
        active = running = true;
        t = u = j = 0;
        B = S = baseLen;
        ringStart = (mode == 5) ? (writePos - baseLen + ringSize) % ringSize : writePos;
        readPos = (double) writePos;
        T = juce::jmax (64, tapeLen);
        trigMode = mode;
    }
    if (! on && prevOn) active = false;
    prevOn = on;

    float* L  = buf.getWritePointer (0);
    float* R  = buf.getWritePointer (1);
    float* rL = ring.getWritePointer (0);
    float* rR = ring.getWritePointer (1);
    float* cL = cap.getWritePointer (0);
    float* cR = cap.getWritePointer (1);

    for (int i = 0; i < n; ++i)
    {
        const float inL = L[i], inR = R[i];
        rL[writePos] = inL;
        rR[writePos] = inR;

        wet += ((active ? 1.0f : 0.0f) - wet) * 0.01f;
        if (! active && wet < 0.001f) { running = false; wet = 0.0f; }

        if (running)
        {
            float wL = inL, wR = inR;

            switch (trigMode)
            {
                case 0:   // Stutter
                case 1:   // Roll (Teilung halbiert sich pro Takt)
                case 5:   // Echo: wiederholt den Slice, der VOR dem Einschalten lief
                {
                    const int startT = (trigMode == 5) ? 0 : B;
                    if (t == startT)   // Slice ist aufgenommen -> einfrieren
                    {
                        for (int q = 0; q < B; ++q)
                        {
                            const int idx = (ringStart + q) % ringSize;
                            cL[q] = rL[idx]; cR[q] = rR[idx];
                        }
                        u = 0; j = 0;
                    }
                    if (t >= startT)
                    {
                        if (u == 0)
                        {
                            const int k = (trigMode == 1) ? juce::jmin (5, (int) ((t - startT) / (4.0 * beat))) : 0;
                            S = juce::jmin (B, juce::jmax (minS, B >> k));
                            curRate = std::pow (2.0, pitch * juce::jmin (j, 32) / 12.0);
                            curRev  = ping && (j & 1);
                            curGain = 1.0f - decay * (float) juce::jmin (j, 16) / 16.0f;
                        }
                        const double x = (curRev ? (S - 1 - u) : u) * curRate;
                        const float env = gateEnv (u, S * gate) * curGain;
                        wL = readInterp (cL, B, x) * env;
                        wR = readInterp (cR, B, x) * env;
                        if (++u >= S) { u = 0; ++j; }
                    }
                    break;
                }
                case 2:   // Gate-Chop
                {
                    const float env = gateEnv (t % B, B * gate);
                    wL = inL * env; wR = inR * env;
                    break;
                }
                case 3:   // Tape-Stop
                {
                    const float x = (float) t / (float) T;
                    if (x >= 1.0f) { wL = wR = 0.0f; break; }
                    readPos += 1.0 - 0.95 * x;
                    const float e = juce::jmin (1.0f, (1.0f - x) * 10.0f);
                    wL = readInterp (rL, ringSize, readPos) * e;
                    wR = readInterp (rR, ringSize, readPos) * e;
                    break;
                }
                case 4:   // Backspin
                {
                    const float x = (float) t / (float) T;
                    if (x >= 1.0f) { wL = wR = 0.0f; break; }
                    readPos -= 3.0 * (1.0 - x) + 0.2;
                    const float e = juce::jmin (1.0f, (1.0f - x) * 10.0f);
                    wL = readInterp (rL, ringSize, readPos) * e;
                    wR = readInterp (rR, ringSize, readPos) * e;
                    break;
                }
                default: break;
            }

            const float m = mix * wet;
            L[i] = inL * (1.0f - m) + wL * m;
            R[i] = inR * (1.0f - m) + wR * m;
            ++t;
        }
        if (++writePos >= ringSize) writePos = 0;
    }
}

juce::AudioProcessorEditor* MaxStutterAudioProcessor::createEditor()
{
    return new juce::GenericAudioProcessorEditor (*this);
}

void MaxStutterAudioProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}

void MaxStutterAudioProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new MaxStutterAudioProcessor(); }

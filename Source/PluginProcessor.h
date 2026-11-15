#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

class MaxStutterAudioProcessor : public juce::AudioProcessor
{
public:
    MaxStutterAudioProcessor();
    ~MaxStutterAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Max Stutter"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    std::atomic<float> *pStutter, *pMode, *pDiv, *pGate, *pDecay, *pPitch, *pPing, *pLen, *pHostBpm, *pBpm, *pMix;

    double sr = 44100.0;
    juce::AudioBuffer<float> ring, cap;
    int ringSize = 0, capSize = 0, writePos = 0;

    bool prevOn = false, active = false, running = false;
    int trigMode = 0, t = 0, B = 0, S = 0, T = 0, u = 0, j = 0, ringStart = 0;
    double readPos = 0.0, curRate = 1.0;
    bool curRev = false;
    float curGain = 1.0f, wet = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MaxStutterAudioProcessor)
};

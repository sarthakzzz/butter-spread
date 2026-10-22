#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "Presets.h"

/** Butter Spread: mid/side width, a mono-safe doubler and bass kept in the middle. */
class ButterSpreadProcessor final : public juce::AudioProcessor
{
public:
    ButterSpreadProcessor();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::UndoManager undoManager;
    juce::AudioProcessorValueTreeState state;
    bs::PresetManager presets { state };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    juce::dsp::LinkwitzRileyFilter<float> crossover;
    juce::SmoothedValue<float> width, doubler, bassAmount, gain;
    std::vector<float> delayLine;
    int delayPos = 0, delaySamples = 1;
    float hpState = 0.0f, hpInPrev = 0.0f, hpCoeff = 0.0f;

    std::atomic<float>* widthParam = nullptr;
    std::atomic<float>* doublerParam = nullptr;
    std::atomic<float>* bassMonoParam = nullptr;
    std::atomic<float>* monoBelowParam = nullptr;
    std::atomic<float>* outputParam = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ButterSpreadProcessor)
};

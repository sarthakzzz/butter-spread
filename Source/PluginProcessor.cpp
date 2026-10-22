#include "PluginProcessor.h"
#include "PluginEditor.h"

ButterSpreadProcessor::ButterSpreadProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      state (*this, &undoManager, "ButterSpread", createLayout())
{
    widthParam = state.getRawParameterValue ("width");
    doublerParam = state.getRawParameterValue ("doubler");
    bassMonoParam = state.getRawParameterValue ("bassMono");
    monoBelowParam = state.getRawParameterValue ("monoBelow");
    outputParam = state.getRawParameterValue ("output");
}

juce::AudioProcessorValueTreeState::ParameterLayout ButterSpreadProcessor::createLayout()
{
    using namespace juce;
    auto pct = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v)) + "%"; });
    auto hz = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v)) + " Hz"; });
    auto db = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return (v > 0.05f ? "+" : "") + String (v, 1) + " dB"; });

    NormalisableRange<float> freq (40.0f, 400.0f);
    freq.setSkewForCentre (120.0f);

    return {
        std::make_unique<AudioParameterFloat> (ParameterID { "width", 1 }, "Width", NormalisableRange<float> (0.0f, 200.0f, 0.1f), 100.0f, pct),
        std::make_unique<AudioParameterFloat> (ParameterID { "doubler", 1 }, "Doubler", NormalisableRange<float> (0.0f, 100.0f, 0.1f), 0.0f, pct),
        std::make_unique<AudioParameterBool> (ParameterID { "bassMono", 1 }, "Bass Mono", true),
        std::make_unique<AudioParameterFloat> (ParameterID { "monoBelow", 1 }, "Mono Below", freq, 120.0f, hz),
        std::make_unique<AudioParameterFloat> (ParameterID { "output", 1 }, "Output", NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f, db),
    };
}

bool ButterSpreadProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo()
        && (layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo()
            || layouts.getMainInputChannelSet() == juce::AudioChannelSet::mono());
}

void ButterSpreadProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    crossover.setType (juce::dsp::LinkwitzRileyFilterType::lowpass);
    crossover.prepare ({ sampleRate, (juce::uint32) samplesPerBlock, 2 });
    crossover.setCutoffFrequency (monoBelowParam->load());
    crossover.reset();

    for (auto* s : { &width, &doubler, &bassAmount, &gain })
        s->reset (sampleRate, 0.03);
    width.setCurrentAndTargetValue (widthParam->load() / 100.0f);
    doubler.setCurrentAndTargetValue (doublerParam->load() / 100.0f);
    bassAmount.setCurrentAndTargetValue (bassMonoParam->load() > 0.5f ? 1.0f : 0.0f);
    gain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (outputParam->load()));

    // The doubler: the mid, 11 ms late and high-passed, added to the sides only.
    // It cancels completely when the track is summed to mono.
    delaySamples = juce::jmax (1, juce::roundToInt (sampleRate * 0.011));
    delayLine.assign ((size_t) delaySamples, 0.0f);
    delayPos = 0;
    hpCoeff = std::exp (-juce::MathConstants<float>::twoPi * 180.0f / (float) sampleRate);
    hpState = hpInPrev = 0.0f;
}

void ButterSpreadProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();

    if (getTotalNumInputChannels() == 1)
        buffer.copyFrom (1, 0, buffer, 0, 0, n);

    width.setTargetValue (widthParam->load() / 100.0f);
    doubler.setTargetValue (doublerParam->load() / 100.0f);
    bassAmount.setTargetValue (bassMonoParam->load() > 0.5f ? 1.0f : 0.0f);
    gain.setTargetValue (juce::Decibels::decibelsToGain (outputParam->load()));
    crossover.setCutoffFrequency (monoBelowParam->load());

    auto* left = buffer.getWritePointer (0);
    auto* right = buffer.getWritePointer (1);

    for (int i = 0; i < n; ++i)
    {
        float lowL, highL, lowR, highR;
        crossover.processSample (0, left[i], lowL, highL);
        crossover.processSample (1, right[i], lowR, highR);

        // bass: blend each side towards the shared middle
        const float a = bassAmount.getNextValue();
        const float lowMid = 0.5f * (lowL + lowR);
        lowL += a * (lowMid - lowL);
        lowR += a * (lowMid - lowR);

        // everything above: mid/side width
        const float mid = 0.5f * (highL + highR);
        float side = 0.5f * (highL - highR);

        const float delayed = delayLine[(size_t) delayPos];
        delayLine[(size_t) delayPos] = mid;
        delayPos = (delayPos + 1) % delaySamples;
        hpState = hpCoeff * (hpState + delayed - hpInPrev);
        hpInPrev = delayed;

        side = width.getNextValue() * (side + 0.7f * doubler.getNextValue() * hpState);

        const float g = gain.getNextValue();
        left[i] = (lowL + mid + side) * g;
        right[i] = (lowR + mid - side) * g;
    }
}

juce::AudioProcessorEditor* ButterSpreadProcessor::createEditor()
{
    return new ButterSpreadEditor (*this);
}

void ButterSpreadProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto tree = state.copyState();
    presets.saveState (tree);
    if (auto xml = tree.createXml())
        copyXmlToBinary (*xml, dest);
}

void ButterSpreadProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size); xml != nullptr && xml->hasTagName (state.state.getType()))
    {
        auto tree = juce::ValueTree::fromXml (*xml);
        state.replaceState (tree);
        presets.restoreState (tree);
        undoManager.clearUndoHistory();
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ButterSpreadProcessor();
}

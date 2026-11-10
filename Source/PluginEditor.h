#pragma once

#include "PluginProcessor.h"
#include "FullScreen.h"

/** A felt breakfast table. The knife spreads the butter (width), the cinnamon shaker is the
    doubler, the toaster keeps the bass mono (lever) below its dial, and the teacup is the output. */
class ButterSpreadEditor final : public juce::AudioProcessorEditor,
                                 private juce::Timer
{
public:
    explicit ButterSpreadEditor (ButterSpreadProcessor&);
    ~ButterSpreadEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    void mouseMove (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    bool keyPressed (const juce::KeyPress&) override;

    static constexpr int baseWidth = 880, baseHeight = 616;

private:
    enum class Target { none, knife, shaker, dial, lever, cup, undo, redo, presetPrev, presetNext, presetName };

    Target targetAt (juce::Point<float>) const;
    juce::Point<float> toBase (juce::Point<float> p) const { return p / scale; }
    juce::RangedAudioParameter* paramFor (Target) const;
    float value (const char* id) const;
    void timerCallback() override;
    void showPresetMenu();
    void savePresetAsync();

    ButterSpreadProcessor& bsProcessor;
    juce::Image bg, title, board, biscuit, toast, knife, toasterOn, toasterOff, dial, shaker, dust, tag;
    juce::Image butter[25], cup[9];

    float scale = 1.0f;
    Target dragging = Target::none, pressed = Target::none, hovered = Target::none;
    float dragStartValue = 0.0f;
    juce::Point<float> dragStart;
    juce::String lastSnapshot;
    std::unique_ptr<juce::AlertWindow> saveWindow;

    /** Full screen button: fills the screen, press again to go back. */
    sk::FullScreen fullScreen;
    sk::FullScreenButton fullScreenButton;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ButterSpreadEditor)
};

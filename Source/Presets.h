#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace bs
{
struct Preset
{
    const char* name;
    float width, doubler, monoBelow, output;
    bool bassMono;
};

const std::vector<Preset>& factoryPresets();

/** Factory presets plus user presets saved as .bspreset files, and the "edited" mark. */
class PresetManager final : private juce::AudioProcessorValueTreeState::Listener
{
public:
    explicit PresetManager (juce::AudioProcessorValueTreeState&);
    ~PresetManager() override;

    juce::String getDisplayName() const;
    void loadFactory (int index);
    void step (int delta);
    bool loadUser (const juce::File&);
    juce::File saveUser (const juce::String& name);

    static juce::File getUserFolder();
    juce::Array<juce::File> getUserPresets() const;

    /** Menu IDs: factory presets are index + 1, user presets start at userBase. */
    static constexpr int userBase = 1000, saveId = 5000, revealId = 5001;
    juce::PopupMenu createMenu() const;
    void handleMenuResult (int id);

    void saveState (juce::ValueTree&) const;
    void restoreState (const juce::ValueTree&);

private:
    void apply (const std::function<void()>&);
    void parameterChanged (const juce::String&, float) override;

    juce::AudioProcessorValueTreeState& state;
    juce::String name { "Unsalted" };
    int factoryIndex = 0;
    bool edited = false, applying = false;
};
} // namespace bs

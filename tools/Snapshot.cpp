// Renders the editor offscreen to a PNG: ButterSpreadSnapshot out.png [width% doubler% bassMono monoBelow outputDb]
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    ButterSpreadProcessor proc;
    const char* ids[] = { "width", "doubler", "bassMono", "monoBelow", "output" };
    for (int i = 0; i < 5 && i + 2 < argc; ++i)
        if (auto* p = proc.state.getParameter (ids[i]))
            p->setValueNotifyingHost (p->convertTo0to1 ((float) juce::String (argv[i + 2]).getDoubleValue()));

    std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
    auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 2.0f);
    juce::File out (juce::File::getCurrentWorkingDirectory().getChildFile (argc > 1 ? argv[1] : "snapshot.png"));
    out.deleteFile();
    juce::FileOutputStream os (out);
    juce::PNGImageFormat().writeImageToStream (img, os);
    return 0;
}

#include "Presets.h"

namespace bs
{
static const char* const paramIds[] = { "width", "doubler", "bassMono", "monoBelow", "output" };

const std::vector<Preset>& factoryPresets()
{
    //                      name                  width doubler monoBelow out  bassMono
    static const std::vector<Preset> list {
        { "Unsalted",            100.0f,  0.0f, 120.0f,  0.0f, true  },
        { "Lightly Buttered",    120.0f,  0.0f, 120.0f,  0.0f, true  },
        { "Spread Thin",         145.0f, 10.0f, 150.0f, -0.5f, true  },
        { "Full Fat",            170.0f, 20.0f, 140.0f, -1.0f, true  },
        { "Knife Edge",          200.0f, 30.0f, 180.0f, -2.0f, true  },
        { "Toast of the Town",   160.0f, 35.0f, 120.0f, -1.0f, true  },
        { "Butter Me Up",        135.0f, 50.0f, 100.0f, -0.5f, true  },
        { "Ghee Whiz",           180.0f, 60.0f, 200.0f, -1.5f, true  },
        { "Breakfast in Bed",    125.0f, 25.0f,  90.0f,  0.0f, true  },
        { "Crumb Doubler",       110.0f, 80.0f, 160.0f, -1.0f, true  },
        { "Bass Pat Down",       115.0f,  0.0f, 250.0f,  0.0f, true  },
        { "Clarified",           110.0f,  0.0f,  80.0f,  0.0f, true  },
        { "Stereo Croissant",    150.0f, 40.0f, 120.0f, -1.0f, false },
        { "Narrow Loaf",          60.0f,  0.0f, 120.0f,  0.0f, false },
        { "Margarine Mono",        0.0f,  0.0f, 120.0f,  0.0f, false },
    };
    return list;
}

PresetManager::PresetManager (juce::AudioProcessorValueTreeState& s) : state (s)
{
    for (auto* id : paramIds)
        state.addParameterListener (id, this);

    // JUCE always passes on each parameter's first change notification, even for an unchanged value.
    // Use it up here, so a host re-sending unchanged values when it opens the plug-in doesn't mark the preset edited.
    apply ([this] { for (auto* p : state.processor.getParameters()) p->sendValueChangedMessageToListeners (p->getValue()); });
}

PresetManager::~PresetManager()
{
    for (auto* id : paramIds)
        state.removeParameterListener (id, this);
}

juce::String PresetManager::getDisplayName() const { return name + (edited ? " *" : ""); }

void PresetManager::apply (const std::function<void()>& fn)
{
    const juce::ScopedValueSetter<bool> setter (applying, true);
    fn();
    edited = false;
}

static void setParam (juce::AudioProcessorValueTreeState& s, const char* id, float value)
{
    if (auto* p = s.getParameter (id))
        p->setValueNotifyingHost (p->convertTo0to1 (value));
}

void PresetManager::loadFactory (int index)
{
    const auto& list = factoryPresets();
    index = juce::jlimit (0, (int) list.size() - 1, index);
    const auto& p = list[(size_t) index];
    state.undoManager->beginNewTransaction ("Preset");
    apply ([&] {
        setParam (state, "width", p.width);
        setParam (state, "doubler", p.doubler);
        setParam (state, "bassMono", p.bassMono ? 1.0f : 0.0f);
        setParam (state, "monoBelow", p.monoBelow);
        setParam (state, "output", p.output);
    });
    factoryIndex = index;
    name = p.name;
}

void PresetManager::step (int delta)
{
    const int n = (int) factoryPresets().size();
    loadFactory (((factoryIndex + delta) % n + n) % n);
}

juce::File PresetManager::getUserFolder()
{
    return juce::File::getSpecialLocation (juce::File::userMusicDirectory)
        .getChildFile ("Plugins for Breakfast/Butter Spread/Presets");
}

juce::Array<juce::File> PresetManager::getUserPresets() const
{
    auto files = getUserFolder().findChildFiles (juce::File::findFiles, false, "*.bspreset");
    files.sort();
    return files;
}

juce::File PresetManager::saveUser (const juce::String& newName)
{
    auto clean = juce::File::createLegalFileName (newName.trim());
    if (clean.isEmpty())
        return {};

    juce::ValueTree tree ("ButterSpreadPreset");
    for (auto* id : paramIds)
        if (auto* p = state.getParameter (id))
            tree.setProperty (id, p->convertFrom0to1 (p->getValue()), nullptr);

    getUserFolder().createDirectory();
    auto file = getUserFolder().getChildFile (clean + ".bspreset");
    if (auto xml = tree.createXml(); xml != nullptr && xml->writeTo (file))
    {
        name = clean;
        edited = false;
        return file;
    }
    return {};
}

bool PresetManager::loadUser (const juce::File& file)
{
    auto xml = juce::XmlDocument::parse (file);
    if (xml == nullptr || ! xml->hasTagName ("ButterSpreadPreset"))
        return false;

    auto tree = juce::ValueTree::fromXml (*xml);
    state.undoManager->beginNewTransaction ("Preset");
    apply ([&] {
        for (auto* id : paramIds)
            if (tree.hasProperty (id))
                setParam (state, id, (float) tree[id]);
    });
    name = file.getFileNameWithoutExtension();
    return true;
}

juce::PopupMenu PresetManager::createMenu() const
{
    juce::PopupMenu menu;
    const auto& list = factoryPresets();
    for (int i = 0; i < (int) list.size(); ++i)
        menu.addItem (i + 1, list[(size_t) i].name, true, name == list[(size_t) i].name);

    auto user = getUserPresets();
    if (! user.isEmpty())
    {
        juce::PopupMenu mine;
        for (int i = 0; i < user.size(); ++i)
            mine.addItem (userBase + i, user[i].getFileNameWithoutExtension(), true, name == user[i].getFileNameWithoutExtension());
        menu.addSubMenu ("My Presets", mine);
    }
    menu.addSeparator();
    menu.addItem (saveId, "Save Preset...");
    menu.addItem (revealId, "Show Preset Folder");
    return menu;
}

void PresetManager::handleMenuResult (int id)
{
    if (id >= 1 && id <= (int) factoryPresets().size())
        loadFactory (id - 1);
    else if (id >= userBase && id < saveId)
    {
        auto user = getUserPresets();
        if (juce::isPositiveAndBelow (id - userBase, user.size()))
            loadUser (user[id - userBase]);
    }
    else if (id == revealId)
    {
        getUserFolder().createDirectory();
        getUserFolder().revealToUser();
    }
}

void PresetManager::saveState (juce::ValueTree& tree) const
{
    tree.setProperty ("presetName", name, nullptr);
    tree.setProperty ("presetIndex", factoryIndex, nullptr);
    tree.setProperty ("presetEdited", edited, nullptr);
}

void PresetManager::restoreState (const juce::ValueTree& tree)
{
    name = tree.getProperty ("presetName", "Unsalted").toString();
    factoryIndex = tree.getProperty ("presetIndex", 0);
    edited = tree.getProperty ("presetEdited", false);
}

void PresetManager::parameterChanged (const juce::String&, float)
{
    if (! applying)
        edited = true;
}
} // namespace bs

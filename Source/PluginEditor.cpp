#include "PluginEditor.h"
#include "ButterArt.h"

namespace
{
juce::Image art (const char* name)
{
    int size = 0;
    if (auto* data = ButterArt::getNamedResource (name, size))
        return juce::ImageCache::getFromMemory (data, size);
    jassertfalse;
    return {};
}

// Where everything sits on the 880 x 616 table (art is rendered at 2x)
const juce::Rectangle<float> titleArea { 16, 6, 522, 104 }, boardArea { 555, 15, 310, 100 },
    undoArea { 730, 114, 65, 60 }, redoArea { 800, 114, 65, 60 },
    toastArea { 240, 212, 400, 300 }, toasterArea { 625, 235, 230, 260 },
    shakerArea { 92, 236, 110, 170 }, dustArea { 118, 372, 150, 80 }, cupArea { 40, 452, 150, 130 },
    shakerTag { 88, 414, 120, 40 }, cupTag { 186, 540, 120, 40 }, toastTag { 380, 522, 120, 40 }, toasterTag { 680, 466, 120, 40 };
const juce::Point<float> dialCentre { 625.0f + 78.8f, 235.0f + 171.3f };
constexpr float dialRadius = 30.0f;

// From the scene's toast.json: the knife tip in the toast image (2x px) as the butter edge moves
constexpr float tipPerUnit = 118.0f; // tip at 0: x 400 (the centre)
float knifeTipX (float t) { return (0.35f + t * 1.3f) * tipPerUnit * 0.5f; } // offset from the toast centre, 1x px

// Small text is lifted so captions, hints and tags stay readable (nothing under ~12 px at 100% size).
static float readableHeight (float h) noexcept { return juce::jmax (h, 0.55f * h + 7.0f); }

juce::Font feltFont (float height, bool bold = true)
{
    return juce::Font (juce::FontOptions ("Georgia", readableHeight (height), bold ? juce::Font::bold : juce::Font::plain));
}

const juce::Colour ink (0xff4a2a18), chalk (0xfff6efe0), chalkDim (0xffc8c0b0);
} // namespace

ButterSpreadEditor::ButterSpreadEditor (ButterSpreadProcessor& p)
    : AudioProcessorEditor (p), bsProcessor (p)
{
    bg = art ("bg_jpg"); title = art ("title_png"); board = art ("board_png"); biscuit = art ("biscuit_png");
    toast = art ("toast_png"); knife = art ("knife_png"); toasterOn = art ("toasteron_png"); toasterOff = art ("toasteroff_png");
    dial = art ("dial_png"); shaker = art ("shaker_png"); dust = art ("dust_png"); tag = art ("tag_png");
    for (int i = 0; i < 25; ++i)
        butter[i] = art (juce::String::formatted ("butter%02d_png", i).toRawUTF8());
    for (int i = 0; i < 9; ++i)
        cup[i] = art (juce::String::formatted ("cup%d_png", i).toRawUTF8());

    setWantsKeyboardFocus (true);
    setResizable (true, true);
    setResizeLimits (baseWidth * 3 / 4, baseHeight * 3 / 4, baseWidth * 2, baseHeight * 2);
    if (auto* c = getConstrainer())
        c->setFixedAspectRatio ((double) baseWidth / baseHeight);
    setSize (baseWidth, baseHeight);
    startTimerHz (30);
}

ButterSpreadEditor::~ButterSpreadEditor() = default;

void ButterSpreadEditor::resized() { scale = (float) getWidth() / (float) baseWidth; }

float ButterSpreadEditor::value (const char* id) const
{
    return bsProcessor.state.getRawParameterValue (id)->load();
}

juce::RangedAudioParameter* ButterSpreadEditor::paramFor (Target t) const
{
    switch (t)
    {
        case Target::knife:  return bsProcessor.state.getParameter ("width");
        case Target::shaker: return bsProcessor.state.getParameter ("doubler");
        case Target::dial:   return bsProcessor.state.getParameter ("monoBelow");
        case Target::lever:  return bsProcessor.state.getParameter ("bassMono");
        case Target::cup:    return bsProcessor.state.getParameter ("output");
        case Target::none: case Target::undo: case Target::redo:
        case Target::presetPrev: case Target::presetNext: case Target::presetName: break;
    }
    return nullptr;
}

//==============================================================================
static void shadow (juce::Graphics& g, juce::Rectangle<float> r, float alpha = 0.32f)
{
    juce::ColourGradient grad (juce::Colour (0xff3a1a08).withAlpha (alpha), r.getCentre(),
                               juce::Colours::transparentBlack, { r.getCentreX(), r.getY() - r.getHeight() * 0.1f }, true);
    g.setGradientFill (grad);
    g.fillEllipse (r);
}

static void drawTag (juce::Graphics& g, const juce::Image& tag, juce::Rectangle<float> r, const juce::String& caption, const juce::String& value)
{
    g.drawImage (tag, r);
    g.setColour (ink.withAlpha (0.7f));
    g.setFont (feltFont (9.5f));
    g.drawFittedText (caption, (r.withTrimmedTop (7).withHeight (12)).toNearestInt(), juce::Justification::centred, 1, 0.75f);
    g.setColour (ink);
    g.setFont (feltFont (14.5f));
    g.drawFittedText (value, (r.withTrimmedTop (17).withHeight (17)).toNearestInt(), juce::Justification::centred, 1, 0.75f);
}

void ButterSpreadEditor::paint (juce::Graphics& g)
{
    g.addTransform (juce::AffineTransform::scale (scale));
    g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
    g.drawImage (bg, { 0.0f, 0.0f, (float) baseWidth, (float) baseHeight });
    g.drawImage (title, titleArea);

    // preset board, chalked name and arrows
    g.drawImage (board, boardArea);
    {
        auto felt = boardArea.reduced (22, 18);
        g.setColour (hovered == Target::presetPrev ? chalk : chalkDim);
        g.setFont (feltFont (32.0f));
        g.drawFittedText (juce::CharPointer_UTF8 ("\xe2\x80\xb9"), (felt.removeFromLeft (26)).toNearestInt(), juce::Justification::centred, 1, 0.75f);
        g.setColour (hovered == Target::presetNext ? chalk : chalkDim);
        g.drawFittedText (juce::CharPointer_UTF8 ("\xe2\x80\xba"), (felt.removeFromRight (26)).toNearestInt(), juce::Justification::centred, 1, 0.75f);
        // caption + name as one block, centred top-to-bottom on the felt
        auto text = felt.withSizeKeepingCentre (felt.getWidth(), 16.0f + 26.0f);
        g.setColour (chalkDim);
        g.setFont (feltFont (9.5f, false));
        g.drawFittedText ("TODAY'S SPREAD", (text.removeFromTop (16)).toNearestInt(), juce::Justification::centred, 1, 0.75f);
        g.setColour (chalk);
        g.setFont (feltFont (19.0f));
        g.drawFittedText (bsProcessor.presets.getDisplayName(), text.toNearestInt(), juce::Justification::centred, 1);
    }

    // shortbread undo / redo (redo is the same biscuit, mirrored)
    auto drawBiscuit = [&] (juce::Rectangle<float> r, bool mirrored, bool enabled, bool down)
    {
        if (down) r = r.reduced (2);
        shadow (g, r.withTrimmedTop (r.getHeight() * 0.55f).expanded (2, 2), 0.25f);
        g.setOpacity (enabled ? 1.0f : 0.75f);
        auto t = juce::RectanglePlacement (juce::RectanglePlacement::stretchToFit).getTransformToFit (biscuit.getBounds().toFloat(), r);
        if (mirrored)
            t = juce::AffineTransform::scale (-1.0f, 1.0f, (float) biscuit.getWidth() * 0.5f, 0.0f).followedBy (t);
        g.drawImageTransformed (biscuit, t);
        g.setOpacity (1.0f);
    };
    drawBiscuit (undoArea, false, bsProcessor.undoManager.canUndo(), pressed == Target::undo);
    drawBiscuit (redoArea, true, bsProcessor.undoManager.canRedo(), pressed == Target::redo);

    // toast: plate, butter spread to the width, and the knife at the butter's edge
    const float w = value ("width") / 200.0f;
    shadow (g, toastArea.withTrimmedTop (150).expanded (6, 4).translated (0, 6), 0.3f);
    g.drawImage (toast, toastArea);
    g.drawImage (butter[juce::jlimit (0, 24, juce::roundToInt (w * 24.0f))], toastArea);
    {
        auto k = juce::Rectangle<float> (500, 500).withCentre (toastArea.getCentre()).translated (knifeTipX (w), 0);
        g.drawImage (knife, k);
    }
    drawTag (g, tag, toastTag, "WIDTH", juce::String (juce::roundToInt (value ("width"))) + "%");

    // cinnamon shaker: tilts and dusts the table as the doubler goes up
    const float dbl = value ("doubler") / 100.0f;
    shadow (g, juce::Rectangle<float> (80, 22).withCentre ({ shakerArea.getCentreX(), shakerArea.getBottom() - 6 }));
    g.setOpacity (juce::jmin (1.0f, dbl * 1.4f));
    g.drawImage (dust, dustArea);
    g.setOpacity (1.0f);
    {
        auto t = juce::RectanglePlacement (juce::RectanglePlacement::stretchToFit).getTransformToFit (shaker.getBounds().toFloat(), shakerArea)
                     .rotated (dbl * 1.05f, shakerArea.getCentreX(), shakerArea.getBottom() - 20);
        g.drawImageTransformed (shaker, t);
    }
    drawTag (g, tag, shakerTag, "DOUBLER", juce::String (juce::roundToInt (value ("doubler"))) + "%");

    // toaster: lever down keeps the bass mono, the browning dial sets how high
    const bool mono = value ("bassMono") > 0.5f;
    shadow (g, juce::Rectangle<float> (220, 34).withCentre ({ toasterArea.getCentreX(), toasterArea.getBottom() - 40 }));
    g.drawImage (mono ? toasterOn : toasterOff, toasterArea);
    {
        auto* p = bsProcessor.state.getParameter ("monoBelow");
        const float angle = juce::degreesToRadians (-135.0f + 270.0f * p->getValue());
        auto r = juce::Rectangle<float> (2 * dialRadius, 2 * dialRadius).withCentre (dialCentre);
        auto t = juce::RectanglePlacement (juce::RectanglePlacement::stretchToFit).getTransformToFit (dial.getBounds().toFloat(), r)
                     .rotated (angle, dialCentre.x, dialCentre.y);
        g.setOpacity (mono ? 1.0f : 0.8f);
        g.drawImageTransformed (dial, t);
        g.setOpacity (1.0f);
    }
    drawTag (g, tag, toasterTag, mono ? "BASS MONO BELOW" : "BASS STEREO",
             mono ? juce::String (juce::roundToInt (value ("monoBelow"))) + " Hz" : juce::String ("off"));

    // teacup: fuller is louder
    const float outDb = value ("output");
    g.drawImage (cup[juce::jlimit (0, 8, juce::roundToInt ((outDb + 12.0f) / 24.0f * 8.0f))], cupArea);
    drawTag (g, tag, cupTag, "OUTPUT", (outDb > 0.05f ? "+" : "") + juce::String (outDb, 1) + " dB");
}

//==============================================================================
ButterSpreadEditor::Target ButterSpreadEditor::targetAt (juce::Point<float> p) const
{
    if (p.getDistanceFrom (dialCentre) < dialRadius + 4) return Target::dial;
    if (undoArea.contains (p)) return Target::undo;
    if (redoArea.contains (p)) return Target::redo;
    if (boardArea.contains (p))
    {
        if (p.x < boardArea.getX() + 50) return Target::presetPrev;
        if (p.x > boardArea.getRight() - 50) return Target::presetNext;
        return Target::presetName;
    }
    if (toastArea.reduced (20, 10).contains (p) || toastTag.contains (p)) return Target::knife;
    if (toasterArea.reduced (10, 30).contains (p) || toasterTag.contains (p)) return Target::lever;
    if (shakerArea.contains (p) || shakerTag.contains (p) || dustArea.contains (p)) return Target::shaker;
    if (cupArea.contains (p) || cupTag.contains (p)) return Target::cup;
    return Target::none;
}

void ButterSpreadEditor::mouseMove (const juce::MouseEvent& e)
{
    auto t = targetAt (toBase (e.position));
    if (t != hovered) { hovered = t; repaint(); }
    setMouseCursor (t == Target::none ? juce::MouseCursor::NormalCursor
                    : t == Target::knife ? juce::MouseCursor::LeftRightResizeCursor
                    : (t == Target::shaker || t == Target::dial || t == Target::cup) ? juce::MouseCursor::UpDownResizeCursor
                    : juce::MouseCursor::PointingHandCursor);
}

void ButterSpreadEditor::mouseDown (const juce::MouseEvent& e)
{
    grabKeyboardFocus();
    auto t = targetAt (toBase (e.position));
    pressed = t;

    if (t == Target::presetName) { showPresetMenu(); return; }
    if (t == Target::presetPrev || t == Target::presetNext) { bsProcessor.presets.step (t == Target::presetNext ? 1 : -1); repaint(); return; }
    if (t == Target::undo || t == Target::redo) { repaint(); return; }

    if (auto* p = paramFor (t))
    {
        bsProcessor.undoManager.beginNewTransaction();
        if (t == Target::lever)
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost (p->getValue() > 0.5f ? 0.0f : 1.0f);
            p->endChangeGesture();
            return;
        }
        dragging = t;
        dragStart = e.position;
        dragStartValue = p->getValue();
        p->beginChangeGesture();
    }
}

void ButterSpreadEditor::mouseDrag (const juce::MouseEvent& e)
{
    if (auto* p = paramFor (dragging))
    {
        auto d = (e.position - dragStart) / scale;
        const float sens = e.mods.isShiftDown() ? 0.0008f : 0.004f;
        const float delta = dragging == Target::knife ? d.x * sens * 0.8f : -d.y * sens;
        p->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, dragStartValue + delta));
    }
}

void ButterSpreadEditor::mouseUp (const juce::MouseEvent& e)
{
    if (auto* p = paramFor (dragging))
        p->endChangeGesture();
    dragging = Target::none;

    const auto t = targetAt (toBase (e.position));
    if (pressed == Target::undo && t == Target::undo) bsProcessor.undoManager.undo();
    if (pressed == Target::redo && t == Target::redo) bsProcessor.undoManager.redo();
    pressed = Target::none;
    repaint();
}

void ButterSpreadEditor::mouseDoubleClick (const juce::MouseEvent& e)
{
    auto t = targetAt (toBase (e.position));
    if (t == Target::lever) return;
    if (auto* p = paramFor (t))
    {
        bsProcessor.undoManager.beginNewTransaction();
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->getDefaultValue());
        p->endChangeGesture();
    }
}

void ButterSpreadEditor::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    auto t = targetAt (toBase (e.position));
    if (t == Target::lever) return;
    if (auto* p = paramFor (t))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, p->getValue() + (wheel.deltaY + wheel.deltaX) * 0.05f));
        p->endChangeGesture();
    }
}

bool ButterSpreadEditor::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress ('z', juce::ModifierKeys::commandModifier | juce::ModifierKeys::shiftModifier, 0)
        || key == juce::KeyPress ('y', juce::ModifierKeys::commandModifier, 0))
    {
        bsProcessor.undoManager.redo();
        repaint();
        return true;
    }
    if (key == juce::KeyPress ('z', juce::ModifierKeys::commandModifier, 0))
    {
        bsProcessor.undoManager.undo();
        repaint();
        return true;
    }
    return false;
}

//==============================================================================
void ButterSpreadEditor::showPresetMenu()
{
    auto opts = juce::PopupMenu::Options().withTargetComponent (this)
                    .withTargetScreenArea (localAreaToGlobal ((boardArea * scale).toNearestInt()));
    bsProcessor.presets.createMenu().showMenuAsync (opts, [safe = juce::Component::SafePointer (this)] (int id)
    {
        if (safe == nullptr || id == 0) return;
        if (id == bs::PresetManager::saveId) safe->savePresetAsync();
        else safe->bsProcessor.presets.handleMenuResult (id);
        safe->repaint();
    });
}

void ButterSpreadEditor::savePresetAsync()
{
    saveWindow = std::make_unique<juce::AlertWindow> ("Save Preset", "Name your spread:", juce::MessageBoxIconType::NoIcon, this);
    saveWindow->addTextEditor ("name", bsProcessor.presets.getDisplayName().upToFirstOccurrenceOf (" *", false, false));
    saveWindow->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    saveWindow->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    saveWindow->enterModalState (true, juce::ModalCallbackFunction::create ([safe = juce::Component::SafePointer (this)] (int result)
    {
        if (safe == nullptr) return;
        if (result == 1)
            safe->bsProcessor.presets.saveUser (safe->saveWindow->getTextEditorContents ("name"));
        safe->saveWindow.reset();
        safe->repaint();
    }));
}

void ButterSpreadEditor::timerCallback()
{
    // repaint only when something visible changed (host automation, undo, presets)
    juce::String snap;
    for (auto* id : { "width", "doubler", "bassMono", "monoBelow", "output" })
        snap << value (id) << ";";
    snap << bsProcessor.presets.getDisplayName() << (int) bsProcessor.undoManager.canUndo() << (int) bsProcessor.undoManager.canRedo();
    if (snap != lastSnapshot)
    {
        lastSnapshot = snap;
        repaint();
    }
}

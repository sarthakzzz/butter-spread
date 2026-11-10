#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

/*  Full screen for a plug-in window that is drawn at one design size and scaled as a whole.

    Every Plugins for Breakfast editor keeps a fixed shape and scales its felt scene to the window, so "full screen"
    means: grow the window, keeping its shape, until it fills the screen it is on (leaving room for the host's own
    title bar and plug-in header), and press again to go back to the size it had before.

        // constructor, instead of setResizeLimits:
        sk::FullScreen::allowSizes (*this, baseWidth, baseHeight, 0.75);
        addAndMakeVisible (fullScreenButton);
        fullScreenButton.onClick = [this] { fullScreen.toggle (*this, baseWidth, baseHeight); };

        // resized():
        fullScreenButton.setBounds (sk::FullScreen::place ({ x, y }, scale));
        fullScreenButton.setFull (fullScreen.isFull (*this));

    Dragging the window corner also goes all the way up to full screen now (it used to stop at 1.5x or 2x).
*/
namespace sk
{
class FullScreen
{
public:
    /** Room left for the host around the plug-in: its title bar and plug-in header above, a little margin below. */
    static constexpr int hostChromeTop = 92, hostChromeBottom = 8, hostChromeSide = 8;

    /** Size of the button at 100% (it scales with the window, like everything else). */
    static constexpr int buttonSize = 34;

    /** The largest window of this shape that fits the given screen area, after the host's chrome. */
    static juce::Rectangle<int> fit (juce::Rectangle<int> screenArea, int baseWidth, int baseHeight)
    {
        const auto w = (double) juce::jmax (1, screenArea.getWidth() - 2 * hostChromeSide);
        const auto h = (double) juce::jmax (1, screenArea.getHeight() - hostChromeTop - hostChromeBottom);
        const auto s = juce::jmin (w / baseWidth, h / baseHeight);
        return { juce::roundToInt (baseWidth * s), juce::roundToInt (baseHeight * s) };
    }

    /** Usable area of the screen the component is on (the main screen if it is not on one yet). */
    static juce::Rectangle<int> screenAreaFor (const juce::Component& c)
    {
        const auto& displays = juce::Desktop::getInstance().getDisplays();
        const juce::Displays::Display* d = nullptr;

        if (c.isShowing())
            d = displays.getDisplayForRect (c.getScreenBounds());

        if (d == nullptr)
            d = displays.getPrimaryDisplay();

        return d != nullptr ? d->userArea : juce::Rectangle<int>();
    }

    /** Sets the editor's resize limits: from minScale of the design size up to filling the biggest screen. */
    static void allowSizes (juce::AudioProcessorEditor& ed, int baseWidth, int baseHeight, double minScale)
    {
        int maxW = baseWidth * 2, maxH = baseHeight * 2;   // fallback when no screen is known (offscreen renders)

        for (const auto& d : juce::Desktop::getInstance().getDisplays().displays)
        {
            const auto r = fit (d.userArea, baseWidth, baseHeight);
            maxW = juce::jmax (maxW, r.getWidth());
            maxH = juce::jmax (maxH, r.getHeight());
        }

        ed.setResizable (true, true);
        ed.setResizeLimits (juce::roundToInt (baseWidth * minScale), juce::roundToInt (baseHeight * minScale), maxW, maxH);

        if (auto* c = ed.getConstrainer())
            c->setFixedAspectRatio ((double) baseWidth / (double) baseHeight);
    }

    /** True while the window is at the size the full screen button gave it. */
    bool isFull (const juce::Component& ed) const
    {
        return ! fullSize.isEmpty() && std::abs (ed.getWidth() - fullSize.getWidth()) <= 4;
    }

    /** Full screen, or back to the size the window had before. */
    void toggle (juce::AudioProcessorEditor& ed, int baseWidth, int baseHeight)
    {
        const auto area = screenAreaFor (ed);
        if (area.isEmpty())
            return;

        if (isFull (ed))
        {
            auto back = previous.isEmpty() || previous.getWidth() >= ed.getWidth() - 4
                          ? juce::Rectangle<int> (baseWidth, baseHeight)
                          : previous;
            fullSize = {};
            ed.setSize (back.getWidth(), back.getHeight());
            return;
        }

        previous = ed.getLocalBounds();
        auto target = fit (area, baseWidth, baseHeight);

        // A plug-in can only change its own size, not move the host's window, and the window grows down and to the
        // right from where it is. So use the room between the window and the screen's bottom-right edge, as long as
        // that still makes it clearly bigger; if the window sits too low for that, take the full size anyway.
        if (ed.isShowing())
        {
            const auto topLeft = ed.getScreenPosition();
            const auto roomW = area.getRight() - topLeft.x - hostChromeSide;
            const auto roomH = area.getBottom() - topLeft.y - hostChromeBottom;
            const auto s = juce::jmin ((double) roomW / baseWidth, (double) roomH / baseHeight);
            const juce::Rectangle<int> onScreen (juce::roundToInt (baseWidth * s), juce::roundToInt (baseHeight * s));

            if (onScreen.getWidth() < target.getWidth() && onScreen.getWidth() > ed.getWidth() * 1.05)
                target = onScreen;
        }

        // let the window grow past the limits set when it opened (a screen may have been plugged in since)
        if (auto* c = ed.getConstrainer())
            if (c->getMaximumWidth() < target.getWidth() || c->getMaximumHeight() < target.getHeight())
                ed.setResizeLimits (c->getMinimumWidth(), c->getMinimumHeight(), target.getWidth(), target.getHeight());

        fullSize = target;
        ed.setSize (target.getWidth(), target.getHeight());
    }

    /** Where the button goes: its top-left corner in design coordinates, scaled with the window. */
    static juce::Rectangle<int> place (juce::Point<float> designTopLeft, float scale, float size = (float) buttonSize)
    {
        return juce::Rectangle<float> (designTopLeft.x, designTopLeft.y, size, size)
                   .transformedBy (juce::AffineTransform::scale (scale)).toNearestInt();
    }

private:
    juce::Rectangle<int> previous, fullSize;
};

/** A small felt patch with four corner arrows: arrows out to go full screen, arrows in to come back. */
class FullScreenButton : public juce::Component,
                         public juce::SettableTooltipClient
{
public:
    std::function<void()> onClick;

    FullScreenButton()
    {
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
        setTitle ("Full screen");
        setTooltip ("Full screen");
    }

    void setFull (bool shouldBeFull)
    {
        if (full == shouldBeFull)
            return;

        full = shouldBeFull;
        setTooltip (full ? "Exit full screen" : "Full screen");
        setTitle (full ? "Exit full screen" : "Full screen");
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        const auto b = getLocalBounds().toFloat();
        const auto u = b.getWidth() / (float) FullScreen::buttonSize;   // one design pixel
        const auto patch = b.reduced (2.0f * u);
        const auto corner = 8.0f * u;

        const juce::Colour cream (0xfff4eedc), creamDark (0xffe0d3b4), ink (0xff0f2d52);

        // soft drop shadow, then a puffy felt patch
        g.setColour (juce::Colours::black.withAlpha (0.22f));
        g.fillRoundedRectangle (patch.translated (0.0f, 1.6f * u), corner);

        juce::ColourGradient puff (cream.brighter (hover ? 0.08f : 0.0f), patch.getCentreX(), patch.getY(),
                                   creamDark, patch.getCentreX(), patch.getBottom(), false);
        g.setGradientFill (puff);
        g.fillRoundedRectangle (patch, corner);

        // running stitch just inside the edge
        juce::Path edge;
        edge.addRoundedRectangle (patch.reduced (3.0f * u), corner - 3.0f * u);
        juce::Path stitched;
        const float dashes[] = { 2.6f * u, 2.2f * u };
        juce::PathStrokeType (1.1f * u).createDashedStroke (stitched, edge, dashes, 2);
        g.setColour (ink.withAlpha (0.45f));
        g.fillPath (stitched);

        // four corner arrows
        const auto c = patch.getCentre();
        const auto reach = 7.6f * u, arm = 4.4f * u;
        juce::Path arrows;

        for (int i = 0; i < 4; ++i)
        {
            const float sx = (i & 1) ? 1.0f : -1.0f, sy = (i & 2) ? 1.0f : -1.0f;
            const juce::Point<float> outer (c.x + sx * reach, c.y + sy * reach);
            const juce::Point<float> inner (c.x + sx * (reach - arm - 1.2f * u), c.y + sy * (reach - arm - 1.2f * u));
            const auto tip = full ? inner : outer;
            const float dx = full ? sx : -sx, dy = full ? sy : -sy;   // the arm runs away from the tip

            arrows.startNewSubPath (tip.x + dx * arm, tip.y);
            arrows.lineTo (tip);
            arrows.lineTo (tip.x, tip.y + dy * arm);
        }

        g.setColour (pressed ? ink.brighter (0.4f) : ink);
        g.strokePath (arrows, juce::PathStrokeType (2.2f * u, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    void mouseEnter (const juce::MouseEvent&) override { hover = true; repaint(); }
    void mouseExit (const juce::MouseEvent&) override { hover = false; repaint(); }
    void mouseDown (const juce::MouseEvent&) override { pressed = true; repaint(); }

    void mouseUp (const juce::MouseEvent& e) override
    {
        pressed = false;
        repaint();

        if (getLocalBounds().contains (e.getPosition()) && onClick != nullptr)
            onClick();
    }

private:
    bool full = false, hover = false, pressed = false;
};
} // namespace sk

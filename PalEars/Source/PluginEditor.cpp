#include "PluginEditor.h"

namespace colours
{
    const juce::Colour background { 0xff141519 };
    const juce::Colour panel      { 0xff1d1f25 };
    const juce::Colour line       { 0xff2c2f37 };
    const juce::Colour text       { 0xffe8e6e1 };
    const juce::Colour quiet      { 0xff8d8f96 };
    const juce::Colour accent     { 0xffe0a84e };   // live / active things only
    const juce::Colour good       { 0xff6cc08b };
    const juce::Colour warn       { 0xffe0a84e };
    const juce::Colour bad        { 0xffe0645a };
}

static juce::String fmtDb (double v, const char* unit = "")
{
    if (v <= -99.0) return juce::String ("--");
    return juce::String (v, 1) + unit;
}

PalEarsEditor::PalEarsEditor (PalEarsProcessor& p)
    : AudioProcessorEditor (p), proc (p)
{
    setSize (480, 330);

    roleBox.addItemList (PalEarsProcessor::roleNames(), 1);
    roleBox.setText (proc.getRole(), juce::dontSendNotification);
    roleBox.onChange = [this] { proc.setRole (roleBox.getText()); repaint(); };
    addAndMakeVisible (roleBox);

    nameEditor.setText (proc.getCustomName(), false);
    nameEditor.setTextToShowWhenEmpty (proc.getHostTrackName().isNotEmpty() ? proc.getHostTrackName()
                                                                            : juce::String ("Track name (optional)"),
                                       colours::quiet);
    nameEditor.onTextChange = [this] { proc.setCustomName (nameEditor.getText().trim()); };
    addAndMakeVisible (nameEditor);

    resetButton.setTooltip ("Restart the integrated loudness and max peak measurement");
    resetButton.onClick = [this] { proc.resetMeasurements(); };
    addAndMakeVisible (resetButton);

    for (auto* l : { &roleLabel, &nameLabel })
    {
        l->setColour (juce::Label::textColourId, colours::quiet);
        l->setFont (juce::FontOptions (12.0f));
        addAndMakeVisible (l);
    }

    for (auto* c : { (juce::Component*) &roleBox, (juce::Component*) &nameEditor, (juce::Component*) &resetButton })
    {
        c->setColour (juce::ComboBox::backgroundColourId, colours::panel);
        c->setColour (juce::ComboBox::outlineColourId, colours::line);
        c->setColour (juce::ComboBox::textColourId, colours::text);
        c->setColour (juce::TextEditor::backgroundColourId, colours::panel);
        c->setColour (juce::TextEditor::outlineColourId, colours::line);
        c->setColour (juce::TextEditor::textColourId, colours::text);
        c->setColour (juce::TextButton::buttonColourId, colours::panel);
        c->setColour (juce::TextButton::textColourOffId, colours::text);
    }

    startTimerHz (10);
}

PalEarsEditor::~PalEarsEditor() { stopTimer(); }

void PalEarsEditor::timerCallback()
{
    const bool connected = proc.isCompanionConnected();
    if (proc.getSnapshotCounter() != lastCounter || connected != lastConnected)
    {
        lastCounter = proc.getSnapshotCounter();
        lastConnected = connected;
        repaint();   // only when something changed: keeps an Intel Mac idle
    }
}

void PalEarsEditor::resized()
{
    auto r = getLocalBounds().reduced (16);
    auto bottom = r.removeFromBottom (28);
    roleLabel.setBounds (bottom.removeFromLeft (34));
    roleBox.setBounds (bottom.removeFromLeft (160));
    bottom.removeFromLeft (10);
    nameLabel.setBounds (bottom.removeFromLeft (40));
    resetButton.setBounds (bottom.removeFromRight (64));
    bottom.removeFromRight (8);
    nameEditor.setBounds (bottom);
}

void PalEarsEditor::drawStat (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& label,
                              const juce::String& value, juce::Colour valueColour)
{
    g.setColour (colours::panel);
    g.fillRoundedRectangle (r.toFloat(), 6.0f);
    g.setColour (colours::quiet);
    g.setFont (juce::FontOptions (11.0f));
    g.drawText (label, r.reduced (8, 4).removeFromTop (16), juce::Justification::centredLeft);
    g.setColour (valueColour);
    g.setFont (juce::FontOptions (17.0f, juce::Font::bold));
    g.drawText (value, r.reduced (8, 4).removeFromBottom (22), juce::Justification::centredLeft);
}

void PalEarsEditor::paint (juce::Graphics& g)
{
    const auto& s = proc.getSnapshot();
    g.fillAll (colours::background);

    auto r = getLocalBounds().reduced (16);

    // Header: name + connection state.
    auto header = r.removeFromTop (24);
    g.setColour (colours::text);
    g.setFont (juce::FontOptions (16.0f, juce::Font::bold));
    g.drawText ("PAL EARS", header.removeFromLeft (110), juce::Justification::centredLeft);
    g.setColour (colours::quiet);
    g.setFont (juce::FontOptions (12.0f));
    g.drawText (proc.getDisplayName(), header.removeFromLeft (170), juce::Justification::centredLeft);

    const bool connected = proc.isCompanionConnected();
    auto status = header;
    g.setColour (connected ? colours::good : colours::quiet);
    g.fillEllipse (status.removeFromLeft (10).withSizeKeepingCentre (8, 8).toFloat());
    g.drawText (connected ? "Production Pal listening" : "Pal app not running",
                status.withTrimmedLeft (6), juce::Justification::centredLeft);

    r.removeFromTop (12);

    // Hero: short-term loudness.
    auto hero = r.removeFromTop (78);
    auto heroLeft = hero.removeFromLeft (190);
    g.setColour (colours::panel);
    g.fillRoundedRectangle (heroLeft.toFloat(), 8.0f);
    g.setColour (colours::quiet);
    g.setFont (juce::FontOptions (11.0f));
    g.drawText ("SHORT-TERM LUFS", heroLeft.reduced (10, 6).removeFromTop (14), juce::Justification::centredLeft);
    g.setColour (s.silent ? colours::quiet : colours::accent);
    g.setFont (juce::FontOptions (38.0f, juce::Font::bold));
    g.drawText (fmtDb (s.lufsShortTerm), heroLeft.reduced (10, 4).withTrimmedTop (14), juce::Justification::centredLeft);

    // Tone balance bars (relative to the whole signal).
    hero.removeFromLeft (10);
    g.setColour (colours::panel);
    g.fillRoundedRectangle (hero.toFloat(), 8.0f);
    auto bars = hero.reduced (10, 8);
    g.setColour (colours::quiet);
    g.setFont (juce::FontOptions (11.0f));
    g.drawText ("TONE BALANCE", bars.removeFromTop (14), juce::Justification::centredLeft);
    const char* bandLabels[] = { "Sub", "Bass", "Low-mid", "Hi-mid", "Air" };
    const int bw = bars.getWidth() / 5;
    for (int i = 0; i < 5; ++i)
    {
        auto col = bars.removeFromLeft (bw).reduced (4, 0);
        auto labelArea = col.removeFromBottom (14);
        const double db = s.bandsDb[(size_t) i];
        const float frac = (float) juce::jlimit (0.0, 1.0, (db + 40.0) / 40.0);
        auto bar = col.toFloat();
        g.setColour (colours::line);
        g.fillRoundedRectangle (bar, 2.0f);
        if (! s.silent)
        {
            g.setColour (colours::accent.withAlpha (0.85f));
            g.fillRoundedRectangle (bar.removeFromBottom (bar.getHeight() * frac), 2.0f);
        }
        g.setColour (colours::quiet);
        g.setFont (juce::FontOptions (10.5f));
        g.drawText (bandLabels[i], labelArea, juce::Justification::centred);
    }

    r.removeFromTop (10);

    // Stat grid.
    auto grid = r.removeFromTop (r.getHeight() - 40);
    const int cw = (grid.getWidth() - 3 * 8) / 4, rh = (grid.getHeight() - 8) / 2;
    auto cell = [&] (int row, int col) { return juce::Rectangle<int> (grid.getX() + col * (cw + 8), grid.getY() + row * (rh + 8), cw, rh); };

    const auto tpCol = s.truePeakMaxDb > -1.0 ? colours::bad : (s.truePeakMaxDb > -3.0 ? colours::warn : colours::text);
    const auto corrCol = s.correlation < 0 ? colours::bad : (s.correlation < 0.3 ? colours::warn : colours::text);
    const auto lowCol = s.lowCorrelation < 0.7 ? colours::warn : colours::text;
    const auto clipCol = s.clipsRecent > 0 ? colours::bad : colours::text;

    drawStat (g, cell (0, 0), "Integrated", fmtDb (s.lufsIntegrated, " LU"), colours::text);
    drawStat (g, cell (0, 1), "True peak max", fmtDb (s.truePeakMaxDb, " dB"), tpCol);
    drawStat (g, cell (0, 2), "Peak (3 s)", fmtDb (s.peakDb, " dB"), colours::text);
    drawStat (g, cell (0, 3), "Crest", s.silent ? "--" : juce::String (s.crestDb, 1) + " dB", colours::text);
    drawStat (g, cell (1, 0), "Stereo corr.", s.silent ? "--" : juce::String (s.correlation, 2), corrCol);
    drawStat (g, cell (1, 1), "Low-end mono", s.silent ? "--" : juce::String (s.lowCorrelation, 2), lowCol);
    drawStat (g, cell (1, 2), "Overs (3 s)", juce::String ((int) s.clipsRecent), clipCol);
    drawStat (g, cell (1, 3), "RMS (3 s)", fmtDb (s.rmsDb, " dB"), colours::text);
}

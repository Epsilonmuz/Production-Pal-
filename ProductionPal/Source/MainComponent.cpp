#include "MainComponent.h"
#include "Knowledge.h"
#include "ScreenCapture.h"

namespace colours
{
    const juce::Colour background { 0xff141519 };
    const juce::Colour panel      { 0xff1d1f25 };
    const juce::Colour line       { 0xff2c2f37 };
    const juce::Colour text       { 0xffe8e6e1 };
    const juce::Colour quiet      { 0xff8d8f96 };
    const juce::Colour accent     { 0xffe0a84e };
    const juce::Colour good       { 0xff6cc08b };
    const juce::Colour bad        { 0xffe0645a };
}

static const juce::StringArray kModels { "claude-sonnet-5-5", "claude-opus-5-5", "claude-haiku-5-5" };

static void styleEditor (juce::TextEditor& e)
{
    e.setColour (juce::TextEditor::backgroundColourId, colours::panel);
    e.setColour (juce::TextEditor::outlineColourId, colours::line);
    e.setColour (juce::TextEditor::focusedOutlineColourId, colours::accent);
    e.setColour (juce::TextEditor::textColourId, colours::text);
    e.setColour (juce::CaretComponent::caretColourId, colours::accent);
}

static void styleButton (juce::Button& b)
{
    b.setColour (juce::TextButton::buttonColourId, colours::panel);
    b.setColour (juce::TextButton::textColourOffId, colours::text);
    b.setColour (juce::ToggleButton::textColourId, colours::text);
    b.setColour (juce::ToggleButton::tickColourId, colours::accent);
}

//==============================================================================
class SettingsPanel final : public juce::Component
{
public:
    explicit SettingsPanel (MainComponent& o) : owner (o)
    {
        auto& p = *owner.getProperties().getUserSettings();

        title.setText ("Settings", juce::dontSendNotification);
        title.setFont (juce::FontOptions (18.0f, juce::Font::bold));
        title.setColour (juce::Label::textColourId, colours::text);
        addAndMakeVisible (title);

        keyLabel.setText ("Claude API key (from console.anthropic.com). Stored in a settings file on this Mac.",
                          juce::dontSendNotification);
        apiKey.setPasswordCharacter ((juce::juce_wchar) 0x2022);
        apiKey.setText (p.getValue ("apiKey"), false);

        modelLabel.setText ("Model (Sonnet = fast and smart, Opus = deepest, Haiku = cheapest)", juce::dontSendNotification);
        model.addItemList (kModels, 1);
        model.setText (p.getValue ("model", kModels[0]), juce::dontSendNotification);
        model.setEditableText (true);

        voiceLabel.setText ("Voice (macOS voice name, blank = system default) and speed", juce::dontSendNotification);
        voice.setText (p.getValue ("voice"), false);
        rate.setRange (140, 260, 5);
        rate.setValue (p.getIntValue ("rate", 190));
        rate.setTextValueSuffix (" wpm");

        onTop.setToggleState (p.getBoolValue ("alwaysOnTop", true), juce::dontSendNotification);

        hotkeyInfo.setText ("Hotkeys (work inside FL Studio):\n"
                            "Control + Option + P   ask (screenshot first)\n"
                            "Control + Option + K   quick check, answer is spoken\n"
                            "Control + Option + S   stop speaking\n"
                            "Tip: press the dictation key in the question box to talk instead of type.",
                            juce::dontSendNotification);

        for (auto* l : { &keyLabel, &modelLabel, &voiceLabel, &hotkeyInfo })
        {
            l->setColour (juce::Label::textColourId, colours::quiet);
            l->setFont (juce::FontOptions (12.5f));
            addAndMakeVisible (l);
        }
        for (auto* e : { &apiKey, &voice }) { styleEditor (*e); addAndMakeVisible (e); }
        model.setColour (juce::ComboBox::backgroundColourId, colours::panel);
        model.setColour (juce::ComboBox::textColourId, colours::text);
        model.setColour (juce::ComboBox::outlineColourId, colours::line);
        addAndMakeVisible (model);
        rate.setColour (juce::Slider::thumbColourId, colours::accent);
        rate.setColour (juce::Slider::trackColourId, colours::line);
        rate.setColour (juce::Slider::textBoxTextColourId, colours::text);
        addAndMakeVisible (rate);
        for (auto* b : { (juce::Button*) &onTop, (juce::Button*) &save, (juce::Button*) &cancel }) { styleButton (*b); addAndMakeVisible (b); }

        save.onClick = [this]
        {
            auto& s = *owner.getProperties().getUserSettings();
            s.setValue ("apiKey", apiKey.getText().trim());
            s.setValue ("model", model.getText().trim());
            s.setValue ("voice", voice.getText().trim());
            s.setValue ("rate", (int) rate.getValue());
            s.setValue ("alwaysOnTop", onTop.getToggleState());
            s.saveIfNeeded();
            if (onClose) onClose (true);
        };
        cancel.onClick = [this] { if (onClose) onClose (false); };
    }

    void paint (juce::Graphics& g) override { g.fillAll (colours::background); }

    void resized() override
    {
        auto r = getLocalBounds().reduced (20);
        title.setBounds (r.removeFromTop (30));
        r.removeFromTop (8);
        keyLabel.setBounds (r.removeFromTop (34));
        apiKey.setBounds (r.removeFromTop (28));
        r.removeFromTop (14);
        modelLabel.setBounds (r.removeFromTop (20));
        model.setBounds (r.removeFromTop (28));
        r.removeFromTop (14);
        voiceLabel.setBounds (r.removeFromTop (20));
        auto row = r.removeFromTop (28);
        voice.setBounds (row.removeFromLeft (row.getWidth() / 2 - 5));
        row.removeFromLeft (10);
        rate.setBounds (row);
        r.removeFromTop (14);
        onTop.setBounds (r.removeFromTop (26));
        r.removeFromTop (10);
        hotkeyInfo.setBounds (r.removeFromTop (90));
        auto buttons = r.removeFromBottom (32);
        save.setBounds (buttons.removeFromRight (100));
        buttons.removeFromRight (10);
        cancel.setBounds (buttons.removeFromRight (100));
    }

    std::function<void (bool saved)> onClose;

private:
    MainComponent& owner;
    juce::Label title, keyLabel, modelLabel, voiceLabel, hotkeyInfo;
    juce::TextEditor apiKey, voice;
    juce::ComboBox model;
    juce::Slider rate { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::ToggleButton onTop { "Keep Production Pal on top of FL Studio" };
    juce::TextButton save { "Save" }, cancel { "Cancel" };
};

//==============================================================================
MainComponent::MainComponent()
{
    juce::PropertiesFile::Options o;
    o.applicationName = "Production Pal";
    o.folderName = "Production Pal";
    o.filenameSuffix = "settings";
    o.osxLibrarySubFolder = "Application Support";
    props.setStorageParameters (o);

    chatView.setMultiLine (true, true);
    chatView.setReadOnly (true);
    chatView.setScrollbarsShown (true);
    chatView.setCaretVisible (false);
    chatView.setFont (juce::FontOptions (14.0f));
    styleEditor (chatView);
    addAndMakeVisible (chatView);

    input.setMultiLine (true, true);
    input.setReturnKeyStartsNewLine (false);
    input.setTextToShowWhenEmpty ("Ask anything... (Return to send)", colours::quiet);
    input.setFont (juce::FontOptions (14.0f));
    input.onReturnKey = [this] { askFromInput(); };
    styleEditor (input);
    addAndMakeVisible (input);

    askButton.onClick = [this] { askFromInput(); };
    quickButton.onClick = [this] { ask (PalKnowledge::quickCheckQuestion(), false); };
    stopButton.onClick = [this] { speech.stop(); };
    settingsButton.onClick = [this] { showSettings (true); };
    clearButton.onClick = [this] { history.clear(); chatView.clear(); setStatus ("New conversation."); };

    screenshotToggle.setToggleState (getSetting ("screenshot", "1") == "1", juce::dontSendNotification);
    speakToggle.setToggleState (getSetting ("speak", "1") == "1", juce::dontSendNotification);
    screenshotToggle.onClick = [this] { props.getUserSettings()->setValue ("screenshot", screenshotToggle.getToggleState() ? "1" : "0"); };
    speakToggle.onClick = [this] { props.getUserSettings()->setValue ("speak", speakToggle.getToggleState() ? "1" : "0"); if (! speakToggle.getToggleState()) speech.stop(); };

    for (auto* b : { (juce::Button*) &askButton, (juce::Button*) &quickButton, (juce::Button*) &stopButton,
                     (juce::Button*) &settingsButton, (juce::Button*) &clearButton,
                     (juce::Button*) &screenshotToggle, (juce::Button*) &speakToggle })
    {
        styleButton (*b);
        addAndMakeVisible (b);
    }
    askButton.setColour (juce::TextButton::buttonColourId, colours::accent.withAlpha (0.85f));
    askButton.setColour (juce::TextButton::textColourOffId, juce::Colours::black);

    statusLabel.setColour (juce::Label::textColourId, colours::quiet);
    statusLabel.setFont (juce::FontOptions (12.0f));
    addAndMakeVisible (statusLabel);

    hotkeys = std::make_unique<GlobalHotkeys> ([this] (GlobalHotkeys::Action a) { onHotkey (a); });

    appendChat ("Pal", "Hey! Put Pal Ears on your Master (and any track you want me to hear), "
                       "then press Control + Option + P from inside FL Studio to ask me anything, "
                       "or Control + Option + K for a quick check of what to fix.");

    if (! hub.isListening())
        setStatus ("Couldn't open the meter port. Is another copy of Production Pal running?");
    else if (getSetting ("apiKey").isEmpty())
        setStatus ("Add your Claude API key in Settings to start.");
    else
        setStatus (hotkeys->isActive() ? "Ready. Hotkeys active." : "Ready.");

    setSize (460, 720);
    startTimerHz (4);
}

MainComponent::~MainComponent()
{
    stopTimer();
    speech.stop();
    props.saveIfNeeded();
}

juce::String MainComponent::getSetting (const juce::String& key, const juce::String& fallback) const
{
    auto* s = const_cast<juce::ApplicationProperties&> (props).getUserSettings();
    if (key == "apiKey")
    {
        auto v = s->getValue ("apiKey");
        if (v.isEmpty()) v = juce::SystemStats::getEnvironmentVariable ("ANTHROPIC_API_KEY", {});
        return v;
    }
    return s->getValue (key, fallback);
}

//==============================================================================
void MainComponent::onHotkey (GlobalHotkeys::Action a)
{
    switch (a)
    {
        case GlobalHotkeys::Action::ask:
            // FL Studio is still in front: grab it before we show ourselves.
            takeScreenshotThen (false, [this]
            {
                juce::Process::makeForegroundProcess();
                if (setWindowVisible) setWindowVisible (true);
                if (auto* top = getTopLevelComponent()) top->toFront (true);
                input.grabKeyboardFocus();
                setStatus (pendingShot.getSize() > 0 ? "Screenshot ready. Type or dictate your question."
                                                     : "Type or dictate your question.");
            });
            break;

        case GlobalHotkeys::Action::quickCheck:
            ask (PalKnowledge::quickCheckQuestion(), false);
            break;

        case GlobalHotkeys::Action::stopSpeaking:
            speech.stop();
            break;
    }
}

void MainComponent::takeScreenshotThen (bool hideWindowFirst, std::function<void()> then)
{
    if (! screenshotToggle.getToggleState()) { then(); return; }

    auto doCapture = [this, hideWindowFirst, then]
    {
        auto shot = ScreenCapture::captureMainDisplay();
        if (hideWindowFirst && setWindowVisible) setWindowVisible (true);

        if (shot.isValid())
        {
            pendingShot = std::move (shot.jpeg);
            pendingShotMs = juce::Time::getMillisecondCounter();
        }
        else
        {
            pendingShot.reset();
            if (shot.error.isNotEmpty()) setStatus (shot.error);
        }
        then();
    };

    if (hideWindowFirst && setWindowVisible)
    {
        setWindowVisible (false);
        juce::Timer::callAfterDelay (350, doCapture);   // let the window disappear first
    }
    else
        doCapture();
}

void MainComponent::askFromInput()
{
    const auto q = input.getText().trim();
    if (q.isEmpty()) return;
    input.clear();
    ask (q, true);
}

void MainComponent::ask (const juce::String& question, bool fromWindow)
{
    if (claude.isBusy()) { setStatus ("Still thinking about the last one..."); return; }

    // Reuse a hotkey screenshot taken in the last minute; otherwise take a fresh one.
    const bool haveRecentShot = pendingShot.getSize() > 0
                                && juce::Time::getMillisecondCounter() - pendingShotMs < 60000;

    auto send = [this, question]
    {
        ClaudeClient::Request req;
        req.apiKey = getSetting ("apiKey");
        req.model = getSetting ("model", kModels[0]);
        req.system = PalKnowledge::systemPrompt();
        req.history = history;

        juce::String full;
        full << question << "\n\n--- Live data from my session ---\n" << hub.buildContextForAI();
        if (screenshotToggle.getToggleState() && pendingShot.getSize() > 0)
        {
            req.jpegScreenshot = pendingShot;
            full << "A screenshot of my screen is attached.\n";
        }
        else
            full << "No screenshot this time.\n";
        req.question = full;

        pendingShot.reset();
        appendChat ("You", question);
        setStatus ("Thinking...");
        speech.stop();

        claude.send (std::move (req), [this, question] (ClaudeClient::Result r)
        {
            if (! r.ok)
            {
                appendChat ("Pal", "(" + r.error + ")");
                setStatus (r.error);
                return;
            }

            appendChat ("Pal", r.text);
            setStatus ("Done.");
            history.push_back ({ "user", question });
            history.push_back ({ "assistant", r.text });
            while (history.size() > 12) history.erase (history.begin());

            if (speakToggle.getToggleState())
                speech.speak (r.text, getSetting ("rate", "190").getIntValue(), getSetting ("voice"));
        });
    };

    if (haveRecentShot || ! screenshotToggle.getToggleState())
        send();
    else
        takeScreenshotThen (fromWindow, send);   // hide Pal first if the question came from its window
}

void MainComponent::appendChat (const juce::String& who, const juce::String& text)
{
    chatView.moveCaretToEnd();
    if (! chatView.isEmpty()) chatView.insertTextAtCaret ("\n\n");
    chatView.setColour (juce::TextEditor::textColourId, who == "You" ? colours::accent : colours::text);
    chatView.insertTextAtCaret (who + ":  " + text);
    chatView.moveCaretToEnd();
}

void MainComponent::setStatus (const juce::String& s)
{
    statusLabel.setText (s, juce::dontSendNotification);
}

//==============================================================================
void MainComponent::showSettings (bool show)
{
    if (show)
    {
        settings = std::make_unique<SettingsPanel> (*this);
        settings->onClose = [this] (bool saved)
        {
            if (saved) applySettings();
            juce::MessageManager::callAsync ([this] { showSettings (false); });
        };
        addAndMakeVisible (*settings);
        settings->setBounds (getLocalBounds());
    }
    else
        settings.reset();
}

void MainComponent::applySettings()
{
    if (setWindowAlwaysOnTop) setWindowAlwaysOnTop (getSetting ("alwaysOnTop", "1") == "1");
    setStatus (getSetting ("apiKey").isEmpty() ? "Add your Claude API key in Settings to start." : "Settings saved.");
}

//==============================================================================
void MainComponent::timerCallback()
{
    const auto counter = hub.getChangeCounter();
    const bool fl = hub.isFlConnected();
    const int liveCount = (int) hub.getLiveEars().size();
    if (counter != lastHubCounter || fl != lastFl || liveCount != lastLiveCount)
    {
        lastHubCounter = counter; lastFl = fl; lastLiveCount = liveCount;
        repaint (liveArea);
    }
}

void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (colours::background);

    auto r = getLocalBounds().reduced (14);
    auto header = r.removeFromTop (26);
    g.setColour (colours::text);
    g.setFont (juce::FontOptions (18.0f, juce::Font::bold));
    g.drawText ("Production Pal", header, juce::Justification::centredLeft);

    paintLivePanel (g, liveArea);
}

void MainComponent::paintLivePanel (juce::Graphics& g, juce::Rectangle<int> area)
{
    g.setColour (colours::panel);
    g.fillRoundedRectangle (area.toFloat(), 8.0f);
    auto r = area.reduced (10, 8);

    const auto live = hub.getLiveEars();
    const bool fl = hub.isFlConnected();

    g.setFont (juce::FontOptions (11.5f));
    g.setColour (colours::quiet);
    auto top = r.removeFromTop (16);
    g.drawText ("LISTENING", top, juce::Justification::centredLeft);
    g.setColour (fl ? colours::good : colours::quiet);
    g.drawText (fl ? "FL Studio connected" : "FL script not connected", top, juce::Justification::centredRight);

    if (live.empty())
    {
        g.setColour (colours::quiet);
        g.setFont (juce::FontOptions (13.0f));
        g.drawText ("No Pal Ears meters yet. Load Pal Ears on your Master.", r, juce::Justification::centredLeft);
        return;
    }

    const int rows = juce::jmin ((int) live.size(), 4);
    const int rowH = r.getHeight() / 4;
    for (int i = 0; i < rows; ++i)
    {
        const auto& v = live[(size_t) i];
        auto row = r.removeFromTop (rowH);
        const bool silent = (bool) v["silent"];
        const double lufs = (double) v["lufs_s"], tp = (double) v["true_peak_max_db"];
        const int clips = (int) v["clips_recent"];
        const double corr = (double) v["correlation"];

        g.setColour (silent ? colours::quiet : colours::accent);
        g.fillEllipse (row.removeFromLeft (12).withSizeKeepingCentre (7, 7).toFloat());

        g.setColour (colours::text);
        g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
        g.drawText (v["track"].toString(), row.removeFromLeft (130), juce::Justification::centredLeft, true);

        juce::String detail;
        if (silent) detail = "silent";
        else
        {
            detail << juce::String (lufs, 1) << " LUFS   TP " << juce::String (tp, 1);
            if (clips > 0) detail << "   CLIPPING";
            else if (corr < 0) detail << "   PHASE";
        }
        g.setColour (clips > 0 || corr < 0 ? colours::bad : colours::quiet);
        g.setFont (juce::FontOptions (12.5f));
        g.drawText (detail, row, juce::Justification::centredLeft, true);
    }
    if ((int) live.size() > rows)
    {
        g.setColour (colours::quiet);
        g.drawText ("+" + juce::String ((int) live.size() - rows) + " more", area.reduced (10, 6), juce::Justification::bottomRight);
    }
}

void MainComponent::resized()
{
    auto r = getLocalBounds().reduced (14);
    auto header = r.removeFromTop (26);
    settingsButton.setBounds (header.removeFromRight (80));
    header.removeFromRight (6);
    clearButton.setBounds (header.removeFromRight (80));

    r.removeFromTop (10);
    liveArea = r.removeFromTop (110);
    r.removeFromTop (10);

    statusLabel.setBounds (r.removeFromBottom (20));
    r.removeFromBottom (4);
    auto toggles = r.removeFromBottom (24);
    screenshotToggle.setBounds (toggles.removeFromLeft (170));
    speakToggle.setBounds (toggles.removeFromLeft (140));
    stopButton.setBounds (toggles.removeFromRight (90));
    r.removeFromBottom (6);
    auto buttons = r.removeFromBottom (32);
    askButton.setBounds (buttons.removeFromRight (90));
    buttons.removeFromRight (8);
    quickButton.setBounds (buttons.removeFromRight (110));
    r.removeFromBottom (6);
    input.setBounds (r.removeFromBottom (70));
    r.removeFromBottom (8);
    chatView.setBounds (r);

    if (settings != nullptr) settings->setBounds (getLocalBounds());
}

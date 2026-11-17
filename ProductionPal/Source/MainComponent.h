#pragma once

#include <juce_gui_extra/juce_gui_extra.h>
#include "ReadingsHub.h"
#include "ClaudeClient.h"
#include "Speech.h"
#include "GlobalHotkeys.h"

class SettingsPanel;

class MainComponent final : public juce::Component,
                            private juce::Timer
{
public:
    MainComponent();
    ~MainComponent() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    // Supplied by the window so we can hide it for an in-app screenshot.
    std::function<void (bool visible)> setWindowVisible;
    std::function<void (bool onTop)> setWindowAlwaysOnTop;

    juce::ApplicationProperties& getProperties() { return props; }

private:
    void timerCallback() override;
    void onHotkey (GlobalHotkeys::Action);

    void askFromInput();
    void ask (const juce::String& question, bool bringToFront);
    void takeScreenshotThen (bool hideWindowFirst, std::function<void()> then);
    void appendChat (const juce::String& who, const juce::String& text);
    void setStatus (const juce::String&);
    void showSettings (bool);
    void applySettings();
    void paintLivePanel (juce::Graphics&, juce::Rectangle<int>);

    juce::String getSetting (const juce::String& key, const juce::String& fallback = {}) const;

    juce::ApplicationProperties props;
    ReadingsHub hub;
    ClaudeClient claude;
    Speech speech;
    std::unique_ptr<GlobalHotkeys> hotkeys;

    std::vector<ClaudeClient::Turn> history;
    juce::MemoryBlock pendingShot;
    juce::uint32 pendingShotMs = 0;

    juce::TextEditor chatView, input;
    juce::TextButton askButton { "Ask" }, quickButton { "Quick check" }, stopButton { "Stop voice" },
                     settingsButton { "Settings" }, clearButton { "New chat" };
    juce::ToggleButton screenshotToggle { "Include screenshot" }, speakToggle { "Speak answers" };
    juce::Label statusLabel;
    std::unique_ptr<SettingsPanel> settings;

    juce::Rectangle<int> liveArea;
    juce::uint32 lastHubCounter = 0;
    bool lastFl = false;
    int lastLiveCount = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};

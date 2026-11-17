#include <juce_gui_extra/juce_gui_extra.h>
#include "MainComponent.h"

class ProductionPalApplication final : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override    { return "Production Pal"; }
    const juce::String getApplicationVersion() override { return PRODUCTION_PAL_VERSION; }
    bool moreThanOneInstanceAllowed() override          { return false; }

    void initialise (const juce::String&) override { mainWindow = std::make_unique<MainWindow>(); }
    void shutdown() override                       { mainWindow = nullptr; }
    void systemRequestedQuit() override            { quit(); }

    void anotherInstanceStarted (const juce::String&) override
    {
        if (mainWindow != nullptr) { mainWindow->setVisible (true); mainWindow->toFront (true); }
    }

    class MainWindow final : public juce::DocumentWindow
    {
    public:
        MainWindow()
            : DocumentWindow ("Production Pal", juce::Colour (0xff141519), DocumentWindow::allButtons)
        {
            setUsingNativeTitleBar (true);
            auto* content = new MainComponent();
            content->setWindowVisible = [this] (bool v) { setVisible (v); };
            content->setWindowAlwaysOnTop = [this] (bool onTop) { setAlwaysOnTop (onTop); };
            setContentOwned (content, true);

            setResizable (true, false);
            setResizeLimits (380, 560, 900, 1400);
            centreWithSize (getWidth(), getHeight());
            setAlwaysOnTop (content->getProperties().getUserSettings()->getBoolValue ("alwaysOnTop", true));
            setVisible (true);
        }

        void closeButtonPressed() override { JUCEApplication::getInstance()->systemRequestedQuit(); }

    private:
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainWindow)
    };

private:
    std::unique_ptr<MainWindow> mainWindow;
};

START_JUCE_APPLICATION (ProductionPalApplication)

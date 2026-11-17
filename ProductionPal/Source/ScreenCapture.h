#pragma once

#include <juce_graphics/juce_graphics.h>

// Takes a screenshot of the main display and returns it as a JPEG sized for the
// Claude API (long edge <= 1568 px). On macOS this uses the built-in
// `screencapture` tool, which needs Screen Recording permission for Production Pal.
namespace ScreenCapture
{
    struct Shot
    {
        juce::MemoryBlock jpeg;
        int width = 0, height = 0;
        juce::String error;
        bool isValid() const { return jpeg.getSize() > 0; }
    };

    Shot captureMainDisplay();
    juce::MemoryBlock encodeForApi (const juce::Image&, int maxEdge = 1568, float quality = 0.8f);
}

#include "ScreenCapture.h"

namespace ScreenCapture
{

juce::MemoryBlock encodeForApi (const juce::Image& source, int maxEdge, float quality)
{
    juce::MemoryBlock out;
    if (! source.isValid()) return out;

    auto img = source;
    const int longEdge = juce::jmax (img.getWidth(), img.getHeight());
    if (longEdge > maxEdge)
    {
        const double scale = (double) maxEdge / longEdge;
        img = img.rescaled (juce::roundToInt (img.getWidth() * scale),
                            juce::roundToInt (img.getHeight() * scale),
                            juce::Graphics::highResamplingQuality);
    }

    juce::JPEGImageFormat jpeg;
    jpeg.setQuality (quality);
    juce::MemoryOutputStream stream (out, false);
    jpeg.writeImageToStream (img, stream);
    return out;
}

Shot captureMainDisplay()
{
    Shot shot;

   #if JUCE_MAC
    const auto file = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("production-pal-shot.png");
    file.deleteFile();

    juce::ChildProcess proc;
    // -x: no shutter sound, -m: main display only.
    if (! proc.start (juce::StringArray { "/usr/sbin/screencapture", "-x", "-m", "-t", "png", file.getFullPathName() }))
    {
        shot.error = "Couldn't run screencapture.";
        return shot;
    }
    proc.waitForProcessToFinish (8000);

    const auto image = juce::ImageFileFormat::loadFrom (file);
    file.deleteFile();

    if (! image.isValid())
    {
        shot.error = "No screenshot. Allow Production Pal in System Settings > Privacy & Security > Screen Recording.";
        return shot;
    }

    shot.width = image.getWidth();
    shot.height = image.getHeight();
    shot.jpeg = encodeForApi (image);
   #else
    shot.error = "Screenshots are only supported on macOS in this version.";
   #endif

    return shot;
}

} // namespace ScreenCapture

#pragma once

#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>

// Talks to the Claude Messages API on a background thread.
class ClaudeClient final
{
public:
    struct Turn { juce::String role, text; };

    struct Request
    {
        juce::String apiKey, model, system;
        std::vector<Turn> history;          // earlier turns, text only
        juce::String question;              // this turn's text (question + live readings)
        juce::MemoryBlock jpegScreenshot;   // optional
        int maxTokens = 900;
    };

    struct Result
    {
        bool ok = false;
        juce::String text, error;
        int statusCode = 0;
    };

    ClaudeClient();
    ~ClaudeClient();

    // Calls onDone on the message thread. Only one request runs at a time.
    void send (Request, std::function<void (Result)> onDone);
    bool isBusy() const noexcept { return busy.load(); }

    static juce::String buildRequestBody (const Request&);   // exposed for tests
    static Result parseResponse (const juce::String& body, int status);

private:
    juce::ThreadPool pool { juce::ThreadPoolOptions().withNumberOfThreads (1) };
    std::atomic<bool> busy { false };
    std::shared_ptr<std::atomic<bool>> alive = std::make_shared<std::atomic<bool>> (true);
};

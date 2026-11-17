// Tests for the Claude request builder / response parser and the readings JSON
// round trip (JUCE core only, no network).
#include <juce_core/juce_core.h>
#include "../ProductionPal/Source/ClaudeClient.h"
#include "../Shared/PalProtocol.h"

static int failures = 0;
static void check (bool ok, const juce::String& what)
{
    std::printf ("%s  %s\n", ok ? "PASS" : "FAIL", what.toRawUTF8());
    if (! ok) ++failures;
}

int main()
{
    {   // Request body shape.
        ClaudeClient::Request req;
        req.model = "claude-sonnet-5-5";
        req.system = "You are Production Pal.";
        req.history = { { "user", "is my vocal loud enough?" }, { "assistant", "Turn it up 2 dB." } };
        req.question = "what now?";
        const char fakeJpeg[] = { (char) 0xFF, (char) 0xD8, (char) 0xFF, 0x00 };
        req.jpegScreenshot.append (fakeJpeg, sizeof (fakeJpeg));

        const auto body = juce::JSON::parse (ClaudeClient::buildRequestBody (req));
        check (body["model"].toString() == "claude-sonnet-5-5", "model is set");
        check ((int) body["max_tokens"] > 0, "max_tokens is set");
        check (body["system"].toString().startsWith ("You are"), "system prompt is top-level");
        auto* msgs = body["messages"].getArray();
        check (msgs != nullptr && msgs->size() == 3, "history + current turn = 3 messages");
        if (msgs != nullptr && msgs->size() == 3)
        {
            check ((*msgs)[0]["role"].toString() == "user" && (*msgs)[1]["role"].toString() == "assistant", "roles alternate");
            auto* content = (*msgs)[2]["content"].getArray();
            check (content != nullptr && content->size() == 2, "last turn has image + text blocks");
            if (content != nullptr && content->size() == 2)
            {
                check ((*content)[0]["type"].toString() == "image", "image block first");
                check ((*content)[0]["source"]["media_type"].toString() == "image/jpeg", "image is JPEG base64");
                check ((*content)[0]["source"]["data"].toString() == juce::Base64::toBase64 (fakeJpeg, sizeof (fakeJpeg)), "image data is base64 of the bytes");
                check ((*content)[1]["text"].toString() == "what now?", "question text block");
            }
        }
    }

    {   // Without a screenshot: one text block.
        ClaudeClient::Request req; req.model = "m"; req.question = "hi";
        const auto body = juce::JSON::parse (ClaudeClient::buildRequestBody (req));
        auto* content = (*body["messages"].getArray())[0]["content"].getArray();
        check (content != nullptr && content->size() == 1 && (*content)[0]["type"].toString() == "text", "no screenshot -> text only");
    }

    {   // Response parsing.
        const auto ok = ClaudeClient::parseResponse (R"({"content":[{"type":"text","text":"Pull the 808 down 3 dB."}],"stop_reason":"end_turn"})", 200);
        check (ok.ok && ok.text == "Pull the 808 down 3 dB.", "parses a text reply");
        const auto bad = ClaudeClient::parseResponse (R"({"type":"error","error":{"type":"authentication_error","message":"invalid x-api-key"}})", 401);
        check (! bad.ok && bad.error.contains ("API key"), "401 -> friendly API key message");
        const auto offline = ClaudeClient::parseResponse ({}, 0);
        check (! offline.ok && offline.error.contains ("internet"), "no connection -> friendly message");
    }

    {   // A Pal Ears reading parses back in the companion.
        pal::ReadingHeader h; h.instanceId = "x1"; h.trackName = "Master"; h.role = "Master"; h.playing = true;
        pal::Snapshot s; s.lufsShortTerm = -9.4; s.truePeakMaxDb = -0.3; s.silent = false;
        const auto v = juce::JSON::parse (juce::String (pal::readingToJson (h, s)));
        check (v["type"].toString() == "ears" && v["track"].toString() == "Master", "reading JSON parses in JUCE");
        check (std::abs ((double) v["lufs_s"] + 9.4) < 0.01 && (bool) v["playing"], "reading values survive the round trip");
        check (v["bands_db"].isObject(), "bands object present");
    }

    std::printf (failures ? "\n%d FAILURE(S)\n" : "\nALL CLIENT TESTS PASSED\n", failures);
    return failures ? 1 : 0;
}

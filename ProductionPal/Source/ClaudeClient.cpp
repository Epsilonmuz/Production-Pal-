#include "ClaudeClient.h"

static const char* kEndpoint = "https://api.anthropic.com/v1/messages";
static const char* kApiVersion = "2023-06-01";

ClaudeClient::ClaudeClient() = default;

ClaudeClient::~ClaudeClient()
{
    *alive = false;
    pool.removeAllJobs (true, 5000);
}

static juce::var textBlock (const juce::String& text)
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("type", "text");
    o->setProperty ("text", text);
    return juce::var (o);
}

juce::String ClaudeClient::buildRequestBody (const Request& req)
{
    juce::Array<juce::var> messages;

    for (const auto& t : req.history)
    {
        auto* m = new juce::DynamicObject();
        m->setProperty ("role", t.role);
        m->setProperty ("content", t.text);
        messages.add (juce::var (m));
    }

    juce::Array<juce::var> content;
    if (req.jpegScreenshot.getSize() > 0)
    {
        auto* src = new juce::DynamicObject();
        src->setProperty ("type", "base64");
        src->setProperty ("media_type", "image/jpeg");
        src->setProperty ("data", juce::Base64::toBase64 (req.jpegScreenshot.getData(), req.jpegScreenshot.getSize()));

        auto* img = new juce::DynamicObject();
        img->setProperty ("type", "image");
        img->setProperty ("source", juce::var (src));
        content.add (juce::var (img));
    }
    content.add (textBlock (req.question));

    auto* user = new juce::DynamicObject();
    user->setProperty ("role", "user");
    user->setProperty ("content", content);
    messages.add (juce::var (user));

    auto* body = new juce::DynamicObject();
    body->setProperty ("model", req.model);
    body->setProperty ("max_tokens", req.maxTokens);
    body->setProperty ("system", req.system);
    body->setProperty ("messages", messages);

    return juce::JSON::toString (juce::var (body), true);
}

ClaudeClient::Result ClaudeClient::parseResponse (const juce::String& body, int status)
{
    Result r;
    r.statusCode = status;
    const auto json = juce::JSON::parse (body);

    if (status == 200 && json.isObject())
    {
        if (auto* arr = json["content"].getArray())
            for (const auto& block : *arr)
                if (block["type"].toString() == "text")
                    r.text << block["text"].toString();

        r.ok = r.text.isNotEmpty();
        if (! r.ok) r.error = "The reply was empty.";
        return r;
    }

    juce::String msg = json["error"]["message"].toString();
    if (msg.isEmpty()) msg = body.substring (0, 300);

    if (status == 401)       r.error = "Your API key was rejected. Check it in Settings.";
    else if (status == 429)  r.error = "Rate limited by the API, try again in a moment. (" + msg + ")";
    else if (status == 529 || status == 503) r.error = "The API is overloaded right now, try again shortly.";
    else if (status == 0)    r.error = "Couldn't reach the Claude API. Check your internet connection.";
    else                     r.error = "API error " + juce::String (status) + ": " + msg;
    return r;
}

void ClaudeClient::send (Request req, std::function<void (Result)> onDone)
{
    if (busy.exchange (true))
    {
        Result r; r.error = "Still working on the last question.";
        onDone (r);
        return;
    }

    auto aliveFlag = alive;
    pool.addJob ([this, aliveFlag, req = std::move (req), onDone = std::move (onDone)]
    {
        Result result;

        if (req.apiKey.trim().isEmpty())
            result.error = "Add your Claude API key in Settings first (console.anthropic.com).";
        else
        {
            const auto body = buildRequestBody (req);
            juce::StringPairArray responseHeaders;
            int status = 0;

            const auto headers = juce::String ("x-api-key: ") + req.apiKey.trim() + "\r\n"
                               + "anthropic-version: " + kApiVersion + "\r\n"
                               + "content-type: application/json";

            auto url = juce::URL (kEndpoint).withPOSTData (body);
            auto options = juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inPostData)
                               .withExtraHeaders (headers)
                               .withConnectionTimeoutMs (90000)
                               .withResponseHeaders (&responseHeaders)
                               .withStatusCode (&status)
                               .withHttpRequestCmd ("POST");

            if (auto stream = url.createInputStream (options))
                result = parseResponse (stream->readEntireStreamAsString(), status);
            else
                result = parseResponse ({}, status);
        }

        juce::MessageManager::callAsync ([this, aliveFlag, result, onDone]
        {
            if (! aliveFlag->load()) return;
            busy = false;
            onDone (result);
        });
    });
}

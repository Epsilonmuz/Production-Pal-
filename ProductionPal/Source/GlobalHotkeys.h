#pragma once

#include <functional>
#include <memory>

// System-wide hotkeys that work while FL Studio has focus.
//   Control + Option + P : ask a question (screenshot first, then focus the question box)
//   Control + Option + K : quick check (screenshot + automatic "what should I fix?")
//   Control + Option + S : stop speaking
// macOS uses Carbon's RegisterEventHotKey (no Accessibility permission needed).
class GlobalHotkeys final
{
public:
    enum class Action { ask = 1, quickCheck = 2, stopSpeaking = 3 };

    explicit GlobalHotkeys (std::function<void (Action)> onHotkey);
    ~GlobalHotkeys();

    bool isActive() const noexcept;

    struct Impl;
private:
    std::unique_ptr<Impl> impl;
};

#include "GlobalHotkeys.h"

#if defined (__APPLE__)
#import <Carbon/Carbon.h>
#include <vector>

struct GlobalHotkeys::Impl
{
    std::function<void (Action)> callback;
    EventHandlerRef handler = nullptr;
    std::vector<EventHotKeyRef> refs;
    bool active = false;

    static OSStatus onEvent (EventHandlerCallRef, EventRef event, void* user)
    {
        EventHotKeyID hk {};
        GetEventParameter (event, kEventParamDirectObject, typeEventHotKeyID, nullptr, sizeof (hk), nullptr, &hk);
        auto* self = static_cast<Impl*> (user);
        if (self->callback) self->callback (static_cast<Action> (hk.id));
        return noErr;
    }

    void registerKey (UInt32 keyCode, Action action)
    {
        EventHotKeyID hk { 'PPal', static_cast<UInt32> (action) };
        EventHotKeyRef ref = nullptr;
        if (RegisterEventHotKey (keyCode, controlKey | optionKey, hk, GetApplicationEventTarget(), 0, &ref) == noErr)
        {
            refs.push_back (ref);
            active = true;
        }
    }
};

GlobalHotkeys::GlobalHotkeys (std::function<void (Action)> onHotkey)
    : impl (std::make_unique<Impl>())
{
    impl->callback = std::move (onHotkey);

    EventTypeSpec spec { kEventClassKeyboard, kEventHotKeyPressed };
    InstallApplicationEventHandler (&Impl::onEvent, 1, &spec, impl.get(), &impl->handler);

    impl->registerKey (kVK_ANSI_P, Action::ask);
    impl->registerKey (kVK_ANSI_K, Action::quickCheck);
    impl->registerKey (kVK_ANSI_S, Action::stopSpeaking);
}

GlobalHotkeys::~GlobalHotkeys()
{
    for (auto ref : impl->refs) UnregisterEventHotKey (ref);
    if (impl->handler != nullptr) RemoveEventHandler (impl->handler);
}

bool GlobalHotkeys::isActive() const noexcept { return impl->active; }

#endif

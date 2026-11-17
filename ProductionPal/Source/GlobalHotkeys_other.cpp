#include "GlobalHotkeys.h"

#if ! defined (__APPLE__)
// Other platforms: no system-wide hotkeys yet; the in-app buttons still work.
struct GlobalHotkeys::Impl {};

GlobalHotkeys::GlobalHotkeys (std::function<void (Action)>) : impl (std::make_unique<Impl>()) {}
GlobalHotkeys::~GlobalHotkeys() = default;
bool GlobalHotkeys::isActive() const noexcept { return false; }
#endif

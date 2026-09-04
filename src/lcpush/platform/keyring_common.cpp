#include "lcpush/platform/keyring.hpp"

namespace lcpush::keyring {

namespace {

Keyring* g_override = nullptr;
bool g_override_active = false;

}  // namespace

Keyring* keyring() {
    if (g_override_active) return g_override;
    return platform_keyring();
}

void set_keyring_override(Keyring* override_impl, bool active) {
    g_override = override_impl;
    g_override_active = active;
}

}  // namespace lcpush::keyring

// OS keyring seam. macOS uses the Keychain with the same service/account
// attributes the Python keyring library wrote, so stored tokens carry over.
// Platforms without a backend get a null implementation and the token flow
// degrades to the 0600 fallback file.
#pragma once

#include <optional>
#include <string>

namespace lcpush::keyring {

class Keyring {
  public:
    virtual ~Keyring() = default;

    // Each returns nullopt / false on any backend failure. Callers treat
    // failure as "keyring unavailable", never as an error.
    virtual std::optional<std::string> get(const std::string& service,
                                           const std::string& account) = 0;
    virtual bool set(const std::string& service, const std::string& account,
                     const std::string& value) = 0;
    virtual bool remove(const std::string& service, const std::string& account) = 0;
};

// The active implementation: the test override when set, else the platform one.
// Returns nullptr when no backend exists on this platform.
Keyring* keyring();

// Install an override for tests. Passing nullptr restores the platform backend.
void set_keyring_override(Keyring* override_impl, bool active);

// RAII helper for tests: force a specific keyring, or force "no keyring".
class KeyringOverride {
  public:
    explicit KeyringOverride(Keyring* impl) { set_keyring_override(impl, true); }
    ~KeyringOverride() { set_keyring_override(nullptr, false); }
    KeyringOverride(const KeyringOverride&) = delete;
    KeyringOverride& operator=(const KeyringOverride&) = delete;
};

// Defined per platform in keyring_macos.cpp / keyring_null.cpp.
Keyring* platform_keyring();

}  // namespace lcpush::keyring

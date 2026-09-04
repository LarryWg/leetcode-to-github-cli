// No keyring backend on this platform. The token flow degrades to the env
// vars, gh CLI, and the 0600 fallback file.
#include "lcpush/platform/keyring.hpp"

#if !defined(__APPLE__)

namespace lcpush::keyring {

Keyring* platform_keyring() { return nullptr; }

}  // namespace lcpush::keyring

#endif  // !defined(__APPLE__)

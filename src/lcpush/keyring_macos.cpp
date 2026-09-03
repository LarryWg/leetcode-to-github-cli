// macOS Keychain backend via the Security framework. Uses generic passwords
// with kSecAttrService / kSecAttrAccount exactly as Python keyring did, so
// tokens stored by the previous implementation keep resolving.
#include "lcpush/keyring.hpp"

#if defined(__APPLE__)

#include <CoreFoundation/CoreFoundation.h>
#include <Security/Security.h>

namespace lcpush::keyring {

namespace {

CFStringRef cf_string(const std::string& text) {
    return CFStringCreateWithBytes(kCFAllocatorDefault,
                                   reinterpret_cast<const UInt8*>(text.data()),
                                   static_cast<CFIndex>(text.size()),
                                   kCFStringEncodingUTF8, false);
}

// Owns the CFDictionary of common query attributes.
class Query {
  public:
    Query(const std::string& service, const std::string& account) {
        service_ = cf_string(service);
        account_ = cf_string(account);
        dict_ = CFDictionaryCreateMutable(kCFAllocatorDefault, 6,
                                          &kCFTypeDictionaryKeyCallBacks,
                                          &kCFTypeDictionaryValueCallBacks);
        CFDictionarySetValue(dict_, kSecClass, kSecClassGenericPassword);
        CFDictionarySetValue(dict_, kSecAttrService, service_);
        CFDictionarySetValue(dict_, kSecAttrAccount, account_);
    }

    ~Query() {
        if (dict_ != nullptr) CFRelease(dict_);
        if (service_ != nullptr) CFRelease(service_);
        if (account_ != nullptr) CFRelease(account_);
    }

    CFMutableDictionaryRef get() { return dict_; }

  private:
    CFStringRef service_ = nullptr;
    CFStringRef account_ = nullptr;
    CFMutableDictionaryRef dict_ = nullptr;
};

class MacKeyring final : public Keyring {
  public:
    std::optional<std::string> get(const std::string& service,
                                   const std::string& account) override {
        Query query(service, account);
        CFDictionarySetValue(query.get(), kSecReturnData, kCFBooleanTrue);
        CFDictionarySetValue(query.get(), kSecMatchLimit, kSecMatchLimitOne);
        CFTypeRef found = nullptr;
        OSStatus status = SecItemCopyMatching(query.get(), &found);
        if (status != errSecSuccess || found == nullptr) return std::nullopt;
        auto data = static_cast<CFDataRef>(found);
        std::string value(reinterpret_cast<const char*>(CFDataGetBytePtr(data)),
                          static_cast<size_t>(CFDataGetLength(data)));
        CFRelease(found);
        return value;
    }

    bool set(const std::string& service, const std::string& account,
             const std::string& value) override {
        CFDataRef data = CFDataCreate(kCFAllocatorDefault,
                                      reinterpret_cast<const UInt8*>(value.data()),
                                      static_cast<CFIndex>(value.size()));
        Query query(service, account);
        CFDictionarySetValue(query.get(), kSecValueData, data);
        OSStatus status = SecItemAdd(query.get(), nullptr);
        if (status == errSecDuplicateItem) {
            Query find(service, account);
            CFMutableDictionaryRef update = CFDictionaryCreateMutable(
                kCFAllocatorDefault, 1, &kCFTypeDictionaryKeyCallBacks,
                &kCFTypeDictionaryValueCallBacks);
            CFDictionarySetValue(update, kSecValueData, data);
            status = SecItemUpdate(find.get(), update);
            CFRelease(update);
        }
        CFRelease(data);
        return status == errSecSuccess;
    }

    bool remove(const std::string& service, const std::string& account) override {
        Query query(service, account);
        return SecItemDelete(query.get()) == errSecSuccess;
    }
};

}  // namespace

Keyring* platform_keyring() {
    static MacKeyring instance;
    return &instance;
}

}  // namespace lcpush::keyring

#endif  // defined(__APPLE__)

// Scripted Subprocess and Keyring fakes, the C++ counterpart of the Python
// tests' monkeypatching of subprocess.run, shutil.which, and the keyring.
#pragma once

#include <functional>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "lcpush/platform/keyring.hpp"
#include "lcpush/util/subprocess.hpp"

namespace lcpush::testing {

class FakeSubprocess final : public util::Subprocess {
  public:
    std::function<util::RunResult(const std::vector<std::string>&)> on_run_capture =
        [](const auto&) { return util::RunResult{}; };
    std::function<std::optional<int>(const std::vector<std::string>&)> on_run_interactive =
        [](const auto&) { return std::nullopt; };
    std::function<std::optional<std::string>(const std::string&)> on_which =
        [](const auto&) { return std::nullopt; };

    util::RunResult run_capture(const std::vector<std::string>& argv, int) override {
        return on_run_capture(argv);
    }

    std::optional<int> run_interactive(const std::vector<std::string>& argv) override {
        return on_run_interactive(argv);
    }

    std::optional<std::string> which(const std::string& name) override {
        return on_which(name);
    }
};

class FakeKeyring final : public keyring::Keyring {
  public:
    std::map<std::pair<std::string, std::string>, std::string> store;
    bool broken = false;

    std::optional<std::string> get(const std::string& service,
                                   const std::string& account) override {
        if (broken) return std::nullopt;
        auto hit = store.find({service, account});
        if (hit == store.end()) return std::nullopt;
        return hit->second;
    }

    bool set(const std::string& service, const std::string& account,
             const std::string& value) override {
        if (broken) return false;
        store[{service, account}] = value;
        return true;
    }

    bool remove(const std::string& service, const std::string& account) override {
        if (broken) return false;
        return store.erase({service, account}) > 0;
    }
};

}  // namespace lcpush::testing

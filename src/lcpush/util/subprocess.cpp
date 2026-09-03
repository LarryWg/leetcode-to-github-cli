#include "lcpush/util/subprocess.hpp"

#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cerrno>
#include <chrono>
#include <cstdlib>
#include <cstring>

#include "lcpush/util/strings.hpp"

namespace lcpush::util {

namespace {

using Clock = std::chrono::steady_clock;

class RealSubprocess final : public Subprocess {
  public:
    RunResult run_capture(const std::vector<std::string>& argv,
                          int timeout_seconds) override {
        RunResult result;
        int out_pipe[2];
        if (::pipe(out_pipe) != 0) return result;

        pid_t pid = ::fork();
        if (pid < 0) {
            ::close(out_pipe[0]);
            ::close(out_pipe[1]);
            return result;
        }
        if (pid == 0) {
            // Child: stdout to the pipe, stderr silenced, stdin closed.
            ::dup2(out_pipe[1], STDOUT_FILENO);
            int devnull = ::open("/dev/null", O_RDWR);
            if (devnull >= 0) {
                ::dup2(devnull, STDERR_FILENO);
                ::dup2(devnull, STDIN_FILENO);
            }
            ::close(out_pipe[0]);
            ::close(out_pipe[1]);
            exec_argv(argv);
            ::_exit(127);
        }

        ::close(out_pipe[1]);
        auto deadline = Clock::now() + std::chrono::seconds(timeout_seconds);
        bool timed_out = false;
        char buffer[4096];
        while (true) {
            auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
                deadline - Clock::now());
            if (remaining.count() <= 0) {
                timed_out = true;
                break;
            }
            struct pollfd pfd = {out_pipe[0], POLLIN, 0};
            int ready = ::poll(&pfd, 1, static_cast<int>(remaining.count()));
            if (ready < 0) {
                if (errno == EINTR) continue;
                break;
            }
            if (ready == 0) {
                timed_out = true;
                break;
            }
            ssize_t n = ::read(out_pipe[0], buffer, sizeof(buffer));
            if (n < 0) {
                if (errno == EINTR) continue;
                break;
            }
            if (n == 0) break;
            result.out.append(buffer, static_cast<size_t>(n));
        }
        ::close(out_pipe[0]);

        if (timed_out) {
            ::kill(pid, SIGKILL);
            int status = 0;
            ::waitpid(pid, &status, 0);
            return result;
        }

        int status = 0;
        if (::waitpid(pid, &status, 0) < 0) return result;
        if (!WIFEXITED(status)) return result;
        result.exit_code = WEXITSTATUS(status);
        result.ok = true;
        return result;
    }

    std::optional<int> run_interactive(const std::vector<std::string>& argv) override {
        pid_t pid = ::fork();
        if (pid < 0) return std::nullopt;
        if (pid == 0) {
            exec_argv(argv);
            ::_exit(127);
        }
        int status = 0;
        while (::waitpid(pid, &status, 0) < 0) {
            if (errno != EINTR) return std::nullopt;
        }
        if (WIFEXITED(status)) {
            int code = WEXITSTATUS(status);
            // 127 from our own _exit means exec failed.
            return code == 127 ? std::optional<int>{} : std::optional<int>{code};
        }
        return std::nullopt;
    }

    std::optional<std::string> which(const std::string& name) override {
        if (name.find('/') != std::string::npos) {
            return executable(name) ? std::optional<std::string>{name} : std::nullopt;
        }
        const char* path_env = std::getenv("PATH");
        if (path_env == nullptr) return std::nullopt;
        for (const std::string& dir : split(path_env, ':')) {
            if (dir.empty()) continue;
            std::string candidate = dir + "/" + name;
            if (executable(candidate)) return candidate;
        }
        return std::nullopt;
    }

  private:
    static void exec_argv(const std::vector<std::string>& argv) {
        std::vector<char*> args;
        args.reserve(argv.size() + 1);
        for (const std::string& arg : argv) args.push_back(const_cast<char*>(arg.c_str()));
        args.push_back(nullptr);
        ::execvp(args[0], args.data());
    }

    static bool executable(const std::string& path) {
        struct stat info{};
        if (::stat(path.c_str(), &info) != 0) return false;
        if (!S_ISREG(info.st_mode)) return false;
        return ::access(path.c_str(), X_OK) == 0;
    }
};

Subprocess* g_override = nullptr;

}  // namespace

Subprocess& subprocess() {
    static RealSubprocess real;
    return g_override != nullptr ? *g_override : static_cast<Subprocess&>(real);
}

void set_subprocess_override(Subprocess* override_impl) { g_override = override_impl; }

}  // namespace lcpush::util
